/******************************************************************************
 * devAiCamacL4434.c -- Device Support Routines for LeCroy 4434 Scalar Modules
 *
 *----------------------------------------------------------------------------
 * Author:	Eric Bjorklund
 * Date:	05-Apr-1995
 *
 *----------------------------------------------------------------------------
 * MODIFICATION LOG:
 *
 * 13-Jun-1995 	Bjo	Fixed bit-ordering problem in csr definition
 * 08-Jan-1996	Bjo	Changes for EPICS R3.12
 *
 *----------------------------------------------------------------------------
 * MODULE DESCRIPTION:
 *
 * This module provides EPICS device support for the LeCroy 4434 latching
 * scalar card.  A task is spawned which scans each 4434 card on the IOC and
 * then signals record processing by posting an I/O event (one per card).
 *
 *----------------------------------------------------------------------------
 * CARD DESCRIPTION:
 *
 * The LeCroy 4434 is a 24-bit, 32-channel latching scalar card.  Each scalar
 * counts logical signals which have a duration of > 10 nsec and a maximum
 * frequency of 20 MHz.  The maximum instantaneous rate for the scalar is
 * 30 MHz and a local double pulse resolution of 30 nsec is permitted.
 *
 * Each channel in the module is followed by an internal buffer or latch which
 * may be used to store and readout accumulated data independent of counting.
 * This readout may take place at any time.  Load, clear, and veto input
 * commands can be sent over the CAMAC dataway or via Lemo connectors on the
 * front panel.
 *
 * Receipt of a CAMAC or front panel load command temporarily halts all the
 * scalars for approximately 220 nanoseconds and transfers the contents to the
 * internal buffer.  The scalars may then be optionally reset before counting
 * is resumed with a clear command.  The duration of the clear command is
 * approximately 100 nanoseconds, during which the module's inputs are
 * disabled.  During data acquisition, unwanted data is rejected for the
 * duration of a front panel veto signal or CAMAC inhibit command.
 *
 * This device-support module makes the following assumptions about the setup
 * of the LeCroy 4434 card:
 *  o Latching is not disabled (via the side switch options).
 *  o There is an independant system which generates load and clear signals
 *    at the front panel every second.  Therefore, all that the device support
 *    module needs to do is read out the module once per second.
 *
 *----------------------------------------------------------------------------
 * RECORD FIELDS OF INTEREST:
 *
 * The following fields should be set by database configuration:
 *	Type	= ai
 *	DTYP	= Camac L4434
 *	INP	= B -> Branch number of L4434 card
 *		  C -> Crate number of L4434 card
 *		  N -> Slot number of L4434 card
 *		  PARM -> Channel number of scalar (0-31)
 *	PREC	= 0 digits (normally)
 *	EGUF	= Engineering value corresponding to 2**24 counts
 *	EGUL	= Engineering value corresponding to 0 counts
 *	SCAN	= I/O Intr
 *	LINR	= LINEAR
 *
 *****************************************************************************/

/************************************************************************/
/*  Header Files							*/
/************************************************************************/

#include	<sys/types.h>	/* Standard Data Type Definitions	*/
#include	<vxWorks.h>	/* VxWorks Definitions & Routines	*/
#include	<stdlib.h>	/* Standard C Routines			*/
#include	<errno.h>	/* Standard Error Codes			*/

#include	<logLib.h>	/* VxWorks Message Logging Library	*/
#include	<semLib.h>	/* VxWorks Semaphore Library		*/
#include	<taskLib.h>	/* VxWorks Task Library			*/
#include	<sysLib.h>	/* VxWorks System Definitions		*/

#include	<dbDefs.h>	/* EPICS Standard Definitions		*/
#include	<dbScan.h>	/* EPICS Data Base Scan Definitions	*/
#include        <recSup.h>	/* EPICS Record Support Definitions	*/
#include	<devSup.h>	/* EPICS Device Support Definitions	*/
#include	<devLib.h>	/* EPICS Device Support Routines	*/
#include	<link.h>	/* EPICS db Link Field Definitions	*/

#include 	<camacLib.h>	/* ESONE CAMAC Library			*/
#include	<devCamac.h>	/* EPICS CAMAC Device Support Library	*/

/************************************************************************/
/*  Local Constants							*/
/************************************************************************/

/*---------------------
 * Conversion codes returned to record support layer
 */
#define	CONVERT			0	/* Successfull return, do conversion */
#define	DO_NOT_CONVERT		2	/* Successfull return, do not convert*/

/*---------------------
 * Timing constants (in seconds)
 */
#define	SCAN_DELAY		1.00	/* Delay time for scanner loop	     */

/*---------------------
 * CAMAC Function codes used by this module
 */
#define	READ_SEQUENTIAL		2	/* Sequential read of next channel   */
#define	WRITE_CONTROL		16	/* Write to control register	     */

/*---------------------
 * Other constants pertaining to the L4434 card
 */
#define L4434_RANGE	  0xffffff	/* Range of scalar channel (24-bits) */
#define	L4434_NUM_CHAN		32	/* Number of channels per card	     */
#define	L4434_SCANNER_PRIO	45	/* Priority for the scanner task     */

/************************************************************************/
/*  Local Structure Declarations					*/
/************************************************************************/


/*=====================
 * L4434_fields -- Field definitions for L4434 control register
 */
typedef struct /* L4434_fields */ {
   unsigned int   test_pulse  :1;	/* Generate test pulse		     */
   unsigned int               :2;
   unsigned int   read_count  :5;	/* Number of channels to read - 1    */
   unsigned int   read        :1;	/* Initialize a read sequence	     */
   unsigned int   clear       :1;	/* Clear counters		     */
   unsigned int   load        :1;	/* Load counters into buffer	     */
   unsigned int   start_addr  :5;	/* Starting channel for read	     */
} L4434_fields;

/*=====================
 * L4434_csr -- L4434 control register
 */
typedef union /* L4434_csr */ {
   uint16_t       value;		/* Control register value	     */
   L4434_fields   field;		/* Control register fields	     */
} L4434_csr;

/*=====================
 * L4434_card -- Structure describing status and values for one L4434 card
 */
typedef struct /* L4434_card */ {
   void        *link;			/* Link to next card structure	     */
   int          ext;			/* CAMAC channel var for this card   */
   L4434_csr    csr;			/* CSR value used to init read	     */
   short int    broken;			/* True if system is broken	     */
   int          first_chan;		/* First channel used in this card   */
   int          last_chan;		/* Last channel used in this card    */
   int          num_chans;		/* Number of channels used	     */
   IOSCANPVT    scan_done;		/* I/O Event to post for this card   */
   SEM_ID       data_mutex;		/* Mutex to protect data buffer	     */
   int          val[L4434_NUM_CHAN];	/* Data values for each channel	     */
   int          status[L4434_NUM_CHAN];	/* Status values for each channel    */
} L4434_card;

/*=====================
 * L4434_devInfo -- Device-dependent structure for L4434 records
 */
typedef struct /* L4434_devInfo */ {
   L4434_card  *pL4434;			/* Pointer to card structure	     */
   int          channel;		/* Channel number for this record    */
   int         *pdata;			/* Ptr to data value for this record */
   int         *pstatus;		/* Ptr to status value for this rec. */
} L4434_devInfo;

/************************************************************************/
/*  Local Function Declarations						*/
/************************************************************************/

/*---------------------
 * Utility Routines
 */
LOCAL L4434_card   *L4434_initStruct (int channel, int ext);

/*---------------------
 * Device Support Routines
 */
LOCAL STATUS        L4434_initDevSup (int after);
LOCAL STATUS        L4434_initRecord (struct aiRecord *pai);
LOCAL STATUS        L4434_read (struct aiRecord *pai);
LOCAL STATUS        L4434_linconv (struct aiRecord *pai, int after);
LOCAL STATUS        L4434_get_ioint_info (int cmd, struct dbCommon *precord, IOSCANPVT *ppvt);

/*---------------------
 * Scanner Task
 */
LOCAL void          L4434_scanner ();


/************************************************************************/
/*  Device Support Entry Table						*/
/************************************************************************/

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_ai;
	DEVSUPFUN	special_linconv;
}devCamacL4434={
	6,
	NULL,
	(DEVSUPFUN) L4434_initDevSup,
	(DEVSUPFUN) L4434_initRecord,
	(DEVSUPFUN) L4434_get_ioint_info,
	(DEVSUPFUN) L4434_read,
        (DEVSUPFUN) L4434_linconv};


/************************************************************************/
/*  Local Variable Declarations						*/
/************************************************************************/

LOCAL L4434_card   *L4434_list;		/* List head for L4434 cards	*/

/************************************************************************/
/*			Utility Macro Routines				*/
/*									*/


/*----------------------------------------------------------------------*/
/* L4434_initAbort () -- Abort L4434 record initialization		*/
/*									*/
/*	This macro is called from the L4434 record initialization	*/
/*	routine when an error is detected which which will prevent	*/
/*	the record from successfully initializing.  It performs the	*/
/*	following actions:						*/
/*	  o Disables the record so that it will not be processed	*/
/*	  o Logs and error message based on the status code and		*/
/*	    message text.						*/
/*	o Aborts the record initialization routine			*/
/*									*/
/*	Macro parameters are:						*/
/*	   status  = Status code describing reason for aborting		*/
/*	   message = Message to output along with error message text	*/
/*									*/
/*	The following variables are assumed to be declared with the	*/
/*	calling procedure:						*/
/*	   pai    = Pointer to record being initialized			*/
/*									*/
/*----------------------------------------------------------------------*/


#define L4434_initAbort(status, message)				\
{									\
   devCamac_recDisable ((void *) pai);					\
   recGblRecordError (status, (void *)pai,		 		\
   "devAiCamacL4434 (L4434_InitRecord)\n-- " message "\n");		\
   return status;							\
									\
}/*end L4434_initAbort()*/

/*----------------------------------------------------------------------*/
/* L4434_LOCK_CARD () -- Lock data area of L4434 card structure		*/
/*									*/
/*	Lock the data area of an L4434 card structure to insure that	*/
/*	the status and data values are consistent for each channel.	*/
/*									*/
/*	Macro parameters are:						*/
/*	  caller = Name of calling routine				*/
/*									*/
/*	The following variables are assumed to be declared within the	*/
/*	calling procedure:						*/
/*	  pL4434 = Pointer to L4434 card structure			*/
/*									*/
/*----------------------------------------------------------------------*/

#define L4434_LOCK_CARD(caller)						\
{									\
   int   status;		/* Local status variable	*/	\
									\
   status = semTake (pL4434->data_mutex, WAIT_FOREVER);			\
   if ((status != OK) && !pL4434->broken) {				\
      logMsg ("devAiCamacL4434 (" #caller ")\n-- Unable to lock card,"	\
              " errno = %d\n", errno, 0, 0, 0, 0, 0);			\
      pL4434->broken = TRUE;						\
   }/*end if could not lock card*/					\
									\
}/*end L4434_LOCK_CARD()*/


/*----------------------------------------------------------------------*/
/* L4434_UNLOCK_CARD () -- Unlock data area of L4434 card structure	*/
/*									*/
/*	Unlock the data area of an L4434 card structure.		*/
/*									*/
/*	Macro parameters are:						*/
/*	  caller = Name of calling routine				*/
/*									*/
/*	The following variables are assumed to be declared within the	*/
/*	calling procedure:						*/
/*	  pL4434 = Pointer to L4434 card structure			*/
/*									*/
/*----------------------------------------------------------------------*/

#define L4434_UNLOCK_CARD(caller)					\
{									\
   int   status;		/* Local status variable	*/	\
									\
   status = semGive (pL4434->data_mutex);				\
   if ((status != OK) && !pL4434->broken) {				\
      logMsg ("devAiCamacL4434 (" #caller ")\n-- Unable to unlock card,"\
              " errno = %d\n", errno, 0, 0, 0, 0, 0);			\
      pL4434->broken = TRUE;						\
   }/*end if could not unlock card*/					\
									\
}/*end L4434_UNLOCK_CARD()*/

/************************************************************************/
/* L4434_initStruct () -- Initialize L4434 card structure		*/
/*	o Allocate the structure, and initialize it.			*/
/*	o Return the address of the card structure or NULL if failed.	*/
/*									*/
/************************************************************************/

LOCAL L4434_card  *L4434_initStruct (int channel, int ext)
{
   register L4434_card  *pL4434;	/* Pointer to card structure	*/

  /*---------------------
   * Allocate the card structure
   */
   pL4434 = (L4434_card *) calloc (1, sizeof(L4434_card));
   if (pL4434 == NULL) return NULL;

  /*---------------------
   * Create the mutex to guard the card's data buffer
   */
   pL4434->data_mutex = semMCreate (SEM_Q_PRIORITY | SEM_DELETE_SAFE | SEM_INVERSION_SAFE);
   if (pL4434->data_mutex == NULL) {
      free (pL4434);
      return NULL;
   }/*end if could not create mutex*/

  /*---------------------
   * Create the I/O scan event to signal when this card has been read
   */
   scanIoInit (&(pL4434->scan_done));

  /*---------------------
   * Initialize the CAMAC channel variable and the minimum and maximum
   * channel numbers for this card.
   */
   pL4434->ext = ext;
   pL4434->first_chan = channel;
   pL4434->last_chan  = channel;

  /*---------------------
   * Add this structure to the linked list of all L4434 cards and
   * return the address of the structure.
   */
   pL4434->link = L4434_list;
   L4434_list = pL4434;

   return pL4434;

}/*end L4434_initStruct()*/

/************************************************************************/
/* L4434_initDevSup () -- Initialize L4434 Device Support Package	*/
/*	o Before record initialization:					*/
/*	  - Initialize the list head for L4434 card structures.		*/
/*									*/
/*	o After record intialization:					*/
/*	  - Compute the number of active channels for each card.	*/
/*	  - Spawn the L4434 scanner task.				*/
/*									*/
/************************************************************************/

LOCAL STATUS L4434_initDevSup (int after)
{
   L4434_card   *pL4434;	/* Pointer to L4434 card structure	*/
   int           status;	/* Local status variable		*/

   /*===================================================================*/
   /* If this call is before record initialization, initialize the	*/
   /* L4434 card structure list head.					*/
   /*===================================================================*/

   if (!after) {
      L4434_list = NULL;
      return OK;
   }/*end if first call*/

   /*===================================================================*/
   /* If this is the second call (record initialization is complete):	*/
   /*  o Check to see if there were any L4434 cards in the system	*/
   /*  o Compute the number of channels to read on each card		*/
   /*  o Compute and store the csr value to initiate a read on the card	*/
   /*  o Start the scanner task						*/
   /*===================================================================*/

  /*---------------------
   * Exit if we didn't find any L4434 records.
   */
   if (L4434_list == NULL) return (OK);

  /*---------------------
   * Loop to compute the number of active channels and the csr value to
   * initiate a read for each card.
   */
   for (pL4434 = L4434_list;  pL4434;  pL4434 = pL4434->link) {
      pL4434->num_chans = pL4434->last_chan - pL4434->first_chan + 1;
      pL4434->csr.field.start_addr = pL4434->first_chan;
      pL4434->csr.field.read_count = pL4434->num_chans - 1;
      pL4434->csr.field.read = TRUE;
   }/*end for each L4434 card*/

  /*---------------------
   * Start the L4434 scanner task
   */
   status = taskSpawn (
            "L4434Scan",		/* Task name			*/
            L4434_SCANNER_PRIO,		/* Task priority		*/
            VX_FP_TASK,			/* Enable floating point	*/
            3000,			/* Stack size			*/
            (FUNCPTR) L4434_scanner,	/* Entry point			*/
            0, 0, 0, 0, 0,		/* No arguments			*/
            0, 0, 0, 0, 0);

  /*---------------------
   * Report error if failed to create task
   */
   if (status == ERROR) {
      logMsg ("devAiCamacL4434 (L4434_initDevSup)\n"
              "-- Failed to create L4434 scanner task\n",
              0, 0, 0, 0, 0, 0);
      return ERROR;
   }/*end if could not create scanner task*/

   return OK;

}/*end L4434_initDevSup()*/

/************************************************************************/
/* L4434_initRecord () -- Record Initialization Routine			*/
/*									*/
/************************************************************************/

LOCAL STATUS L4434_initRecord (struct aiRecord *pai)
{
   int                   channel;	/* Channel number for this record    */
   int                   ext;		/* L4434 CAMAC channel		     */
   struct camacio       *pcamacio;	/* Ptr to camacio part of INP field  */
   L4434_devInfo        *pdevInfo;	/* Ptr to record's devInfo structure */
   register L4434_card  *pL4434;	/* Ptr to L4434 card structure	     */

  /*---------------------
   * Make sure the record type is valid
   */
   if (pai->inp.type != CAMAC_IO)
      L4434_initAbort (S_db_badField, "Illegal INP.type field");

  /*---------------------
   * Make sure the channel number is valid
   */
   pcamacio = &(pai->inp.value.camacio);
   channel = atoi ((char *)pcamacio->parm);

   if ((unsigned int)channel >= L4434_NUM_CHAN) 
      L4434_initAbort (S_db_badField, "Illegal INP.parm field");

  /*---------------------
   * Allocate and initialize the device-specific information structure.
   * Abort if not enough memory.
   */
   pdevInfo = (L4434_devInfo *) malloc (sizeof(L4434_devInfo));
   if (pdevInfo != NULL) pai->dpvt = (long *)pdevInfo;
   else L4434_initAbort (S_dev_noMemory, "Unable to allocate device-dependant structure");

   pdevInfo->channel = channel;
   pdevInfo->pdata   = NULL;
   pdevInfo->pstatus = NULL;

  /*---------------------
   * Check to see if a card structure already exists for this card
   */
   cdreg (&ext, pcamacio->b, pcamacio->c, pcamacio->n, 0);
   for (pL4434 = L4434_list;  pL4434;  pL4434 = pL4434->link)
      if (pL4434->ext == ext) break;

  /*---------------------
   * If we don't already have a card structure for this card, allocate one.
   * Register the card with the CAMAC driver.
   */
   if (pL4434 == NULL) {
      pL4434 = L4434_initStruct (channel, ext);
      camacRegisterCard (pcamacio->b, pcamacio->c, pcamacio->n,
                        "L4434  24-bit scalar", NULL, 0);
    }/*end if need to allocate new card structure*/

  /*---------------------
   * Abort if unable to allocate card structure
   */
   if (pL4434 == NULL)
      L4434_initAbort (errno, "Unable to allocate card structure");

  /*---------------------
   * Compute the minimum and maximum channel numbers seen so far.
   */
   pdevInfo->pL4434 = pL4434;
   if (channel < pL4434->first_chan) pL4434->first_chan = channel;
   if (channel > pL4434->last_chan)  pL4434->last_chan  = channel;

  /*---------------------
   * Return after initializing the slope for linear conversions
   */
   L4434_linconv (pai, TRUE);
   return OK;

}/*end L4434_initRecord()*/

/************************************************************************/
/* L4434_read () -- Read value for this channel from card buffer	*/
/*									*/
/************************************************************************/

LOCAL STATUS L4434_read (struct aiRecord *pai)
{
   register int    channel;	/* L4434 channel number for this record     */
   register int    data;	/* Data value for this record		    */
   L4434_devInfo  *pdevInfo;	/* Pointer to device-specific information   */
   register int   *pstatus;	/* Pointer to status value for this channel */
   L4434_card     *pL4434;	/* Pointer to card-specific information	    */
   register int    status;	/* Local status variable		    */

  /*---------------------
   * Get the addresses of the device-dependent structure, the card structure,
   * and the channel status.
   */
   pdevInfo = (L4434_devInfo *) pai->dpvt;
   pL4434 = pdevInfo->pL4434;
   pstatus = pdevInfo->pstatus;

  /*---------------------
   * If this is the first time to process this record, compute the address
   * of the data and status values for this channel.
   */
   if (pstatus == NULL) {
      pL4434 = pdevInfo->pL4434;
      channel = pdevInfo->channel - pL4434->first_chan;
      pdevInfo->pdata = &(pL4434->val[channel]);
      pdevInfo->pstatus = pstatus = &(pL4434->status[channel]);
   }/*end if first time through*/

  /*---------------------
   * Lock the card while we retrieve the data and status values for this
   * channel.
   */
   L4434_LOCK_CARD (L4434_read);
   data = *(pdevInfo->pdata);
   status = *pstatus;
   L4434_UNLOCK_CARD (L4434_read);

  /*---------------------
   * Check the status value for this channel.  If there was an error,
   * raise an alarm, and quit.
   */
   if (status != OK) {
      recGblSetSevr (pai, READ_ALARM, INVALID_ALARM);
      return DO_NOT_CONVERT;
   }/*end if hardware error*/

  /*---------------------
   * If there is no error, store the raw value and request conversion.
   */
   pai->rval = data;
   return CONVERT;

}/*end L4434_read()*/

/************************************************************************/
/* L4434_get_ioint_info () -- Get value of I/O event			*/
/*									*/
/************************************************************************/

LOCAL STATUS L4434_get_ioint_info (int cmd, struct dbCommon *precord,
                                   IOSCANPVT *ppvt)
{
   L4434_devInfo   *pdevInfo;	/* Pointer to device-specific information */

   pdevInfo = (L4434_devInfo *) precord->dpvt;
   *ppvt = pdevInfo->pL4434->scan_done;
   return OK;

}/*end L4434_get_ioint_info()*/


/************************************************************************/
/* L4434_linconv () -- Set linear conversion slope			*/
/*									*/
/************************************************************************/

LOCAL STATUS L4434_linconv (struct aiRecord *pai, int after)
{
   if (!after) return OK;
   pai->eslo = (pai->eguf - pai->egul) / L4434_RANGE;
   return OK;

}/*end L4434_linconv*/

/************************************************************************/
/* L4434_scanner () -- Scanner task for L4434 cards			*/
/*	o Periodically scan each card in the system			*/
/*	o Read latest data and status values into the card's structure	*/
/*									*/
/************************************************************************/

LOCAL void L4434_scanner ()
{
   static int            buffer[L4434_NUM_CHAN];/* Local data buffer         */
   static int            cb[4] = {0,0,0,0};	/* Control block for CAMAC   */
   register int          channels_read;		/* Num. channels read	     */
   int                   i;			/* Loop counter		     */
   register int         *pbuffer;		/* Pointer to data buffer    */
   register L4434_card  *pL4434;		/* Pointer to card structure */
   register int         *pstatus;		/* Pointer to status array   */
   register int         *pval;			/* Pointer to value array    */
   int                   q;			/* Q status return	     */
   int                   scan_delay;		/* Scanner delay value	     */
   int                   status;		/* Local status variable     */

  /*---------------------
   * Compute the number of ticks to wait between scans
   */
   scan_delay = vxTicksPerSecond * SCAN_DELAY;

  /*---------------------
   * Loop to periodically scan each L4434 card in the system
   */
   FOREVER {

     /*---------------------
      * Process each L4434 card on this IOC
      */
      for (pL4434 = L4434_list;  pL4434;  pL4434 = pL4434->link) {

        /*---------------------
         * Initialize the control array with the number of channels to read
         * and number of channels actually read.
         */
         cb[0] = pL4434->num_chans;	/* Number of words requested	*/
         cb[1] = 0;			/* Number of words read		*/
         cb[2] = 0;			/* Don't wait for LAM		*/

        /*---------------------
         * Initialize the CSR with the starting channel and number of channels
         * to read (note that this info is stored in a pre-defined csr value
         * stored in the card structure).
         */
         cssa (WRITE_CONTROL, pL4434->ext, (short *)&(pL4434->csr.value), &q);
         ctstat (&status);

        /*---------------------
         * If csr write was successfull, read out all the active channels
         * on this card.
         */
         if (status == OK) {
            cfubc (READ_SEQUENTIAL, pL4434->ext, buffer, cb);
            ctstat (&status);
	 }/*end if csr write was successfull*/

        /*---------------------
         * Initialze the pointers for data and status transfer.  Lock access
         * to the card structure's data area while we store the data and
         * status values.
         */
         channels_read = cb[1];		/* Get num channels actually read   */
         pbuffer = buffer;		/* Init pointer to data buffer      */
         pstatus = pL4434->status;	/* Init pointer to status array     */
         pval    = pL4434->val;		/* Init pointer to value array      */

         L4434_LOCK_CARD (L4434_scanner);

        /*---------------------
         * For each successful read, store the data and a success status code
         * in the card structure.
         */
         for (i = 0;  i < channels_read;  i++) {
            *pval++ = *pbuffer++;
            *pstatus++ = OK;
	 }/*end for each successfull read*/

        /*---------------------
         * For each unsuccessful read, store the failure code.
         * Do not change the data value.
         */
         for (i = channels_read;  i < pL4434->num_chans;  i++)
            *pstatus++ = status;

        /*---------------------
         * Unlock the data area and signal that the scan is done for this card
         */
         L4434_UNLOCK_CARD (L4434_scanner);

         scanIoRequest (pL4434->scan_done);

      }/*end for each card*/

     /*---------------------
      * Take a breather while the cards are scanning new values.
      */
      taskDelay (scan_delay);

   }/*end FOREVER loop*/

}/*end L4434_scanner()*/
