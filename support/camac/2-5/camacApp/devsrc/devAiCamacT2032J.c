/******************************************************************************
 * devAiCamacT2032J.c -- Device Support Routines for Transiac 2032j and
 *                       BiRa 5305j SAM (Smart Analog Monitor) Scanning DVM
 *                       CAMAC Modules
 *
 *----------------------------------------------------------------------------
 * Author:	Eric Bjorklund
 * Date:	15-Sep-1994
 *
 *----------------------------------------------------------------------------
 * MODIFICATION LOG:
 *
 * 25-Apr-1995	Bjo	Add revert2iss routine
 * 21-Jul-1995	Bjo	Return INVALID_ALARM severity on overrange error
 * 18-Dec-1995	Bjo	Remove revert2iss.  "ANSIfy" the code for R3.12
 *
 *----------------------------------------------------------------------------
 * MODULE DESCRIPTION:
 *
 * This module provides EPICS device support for the Transiac 2032J and BiRa
 * 5305J SAM (Smart Analog Monitor) scanning DVM CAMAC cards.
 *
 * A task is spawned which scans each 2032/5305 card on the IOC and then
 * signals record processing by posting an I/O event (one per card).
 *
 *----------------------------------------------------------------------------
 * CARD DESCRIPTION:
 *
 * The Transiac 2032 and BiRa 5305 SAM (Smart Analog Monitor) cards are
 * 32-channel scanning DVMs with digital filtering to remove common mode and
 * 60 Hz noise.
 *
 * The card uses a 12-bit ADC, but provides 14-bit accuracy through dithering.
 * Output may be in either VAX or IEEE floating point format.  All 32 channels
 * are continuously scanned.  Each channel requires 20 ms to convert, hence
 * the minimum refresh rate for the card is 640 ms.  If any of the inputs
 * are floating or oscillating quickly, the refresh time will increase.  Worst
 * case is 2 seconds.
 *
 * Every minute, the card executes a calibration and AC measurement sequence.
 * During this time (about 640 ms), the card will not return a Q response to
 * any valid command.  Note that "repeat until Q" (UBR-mode) commands will
 * normally time out before the calibration/measurement sequence completes.
 * Consequently, we do not use UBR-mode commands to access the card.
 *
 * After intializing the card (with an F9 command), there is a delay of up
 * to 4.7 seconds before the card starts scanning.  During this initialization
 * period, read instructions (F0) will return Q=0.  Other valid instructions,
 * however, will still return Q=1.
 *
 * Another "feature" of the card is that you must issue the first read
 * within 100 ms of setting the channel number to avoid getting data from
 * two separate scans.
 *
 * This best use of these cards is on a fast highway monitoring slowly changing
 * signals (e.g. magnet power supplies). 
 *
 * Cards used in the LAMPF/PSR control system (with the "j" designation)
 * have been modified to always return X for a valid command, even during the
 * calibration/AC measurement sequence.
 *
 *----------------------------------------------------------------------------
 * RECORD FIELDS OF INTEREST:
 *
 * The following fields should be set by database configuration:
 *	Type	= ai
 *	DTYP	= Camac T2032J
 *	INP	= B -> Branch number of T2032J card
 *		  C -> Crate number of T2032J card
 *		  N -> Slot number of T2032J card
 *		  A -> Ignored
 *		  F -> Ignored
 *		  PARM -> Channel number of data value (0-31)
 *	PREC	= 5 digits (14 bits)
 *	EGUF	= Engineering value corresponding to:
 *		   +10.00 volts for bipolar or positive unipolar devices
 *		     0.00 volts for negative unipolar devices
 *	EGUL	= Engineering value corresponding to:
 *		   -10.00 volts for bipolar or negative unipolar devices
 *		     0.00 volts for positive unipolar devices
 *	SCAN	= I/O Intr
 *	LINR	= LINEAR
 *
 * The following fields are ignored by record and device processing:
 *	AOFF
 *	ASLO
 *	SMOO
 *
 * Other fields of interest:
 *	RVAL	= Contains an integer representation of the channel value
 *		  in millivolts.
 *
 *
 *****************************************************************************/

/************************************************************************/
/*  Header Files							*/
/************************************************************************/

#include	<sys/types.h>	/* Standard Data Type Definitions	*/
#include	<stdlib.h>	/* Standard C Routines			*/
#include	<stdio.h>	/* Standard I/O Routines		*/
#include	<string.h>	/* Standard String Handling Routines	*/
#include	<errno.h>	/* Standard Error Codes			*/

#include	<vxWorks.h>	/* VxWorks Definitions & Routines	*/
#include	<sysLib.h>	/* VxWorks System Routines		*/
#include	<logLib.h>	/* VxWorks Message Logging Library	*/
#include	<semLib.h>	/* VxWorks Semaphore Library		*/
#include	<taskLib.h>	/* VxWorks Task Library			*/

#include	<dbDefs.h>	/* EPICS Standard Definitions		*/
#include	<dbScan.h>	/* EPICS Data Base Scan Definitions	*/
#include        <recSup.h>	/* EPICS Record Support Definitions	*/
#include	<devSup.h>	/* EPICS Device Support Definitions	*/
#include	<link.h>	/* EPICS db Link Field Definitions	*/
#include	<errMdef.h>	/* EPICS Error Display Routines		*/
#include	<aiRecord.h>	/* EPICS AI Record Definitions		*/

#include 	<camacLib.h>	/* EPICS/ESONE CAMAC Library Routines	*/

/************************************************************************/
/*  Local Constants							*/
/************************************************************************/

/*---------------------
 * Conversion codes returned to record support layer
 */
#define	CONVERT		0	/* Successfull return, request conversion    */
#define	DO_NOT_CONVERT	2	/* Successfull return, do not convert	     */

/*---------------------
 * Timing constants (in seconds)
 */
#define	INIT_DELAY	0.04	/* Delay after init before setting mode	     */
#define	SCAN_DELAY	0.64	/* Delay time for scanner loop		     */
#define	RETRY_TIME	10.00	/* Time to retry before saying card is dead  */

/*---------------------
 * Range constants
 */
#define BIPOLAR_RANGE	20.00	/* Bipolar scaling range -10 to +10 volts    */
#define	UNIPOLAR_RANGE	10.00	/* Unipolar scaling range 0 to +/-10 volts   */
#define	MAX_RANGE	10.24	/* Full scale max/min +/-10.24 volts	     */
#define	MAX_SCALE	10.00	/* Upper value for scaling computations	     */
#define OVERRANGE	999999.	/* Overrange value for 10 volt DVM	     */
#define UNDERRANGE     -999999.	/* Underrange value for 10 volt DVM	     */

/*---------------------
 * CAMAC Function codes used by this module
 */
#define	READ		0	/* Read next channel			     */
#define	RESET		9	/* Reset module				     */
#define	WRITE_CONTROL	16	/* Write to module control register	     */
#define	WRITE_CHANNEL	17	/* Write starting channel number	     */

/*---------------------
 * Other constants pertaining to the T2032J card
 */
#define	IEEE_MODE	0x0004	/* Mask to set card in IEEE floating pt mode */
#define	LOW_DATA_MASK	0xff00	/* Mask to remove range/cal data from value  */
#define	NUM_T2032_CHAN	32	/* Maximum number of channels per card	     */
#define	SCANNER_PRIO	45	/* Priority for the scanner task	     */

/************************************************************************/
/*  Local Structure Declarations					*/
/************************************************************************/


/*=====================
 * t2032j_buffer -- Data buffer for reading T2032J card
 */
typedef union /* t2032j_buffer */ {
   float     fval [NUM_T2032_CHAN];	/*   Data values in floating point   */
   uint16_t  rval [2*NUM_T2032_CHAN];	/*   Raw data values (from CAMAC)    */
} t2032j_buffer;


/*=====================
 * t2032j_card -- Structure describing status and values for one T2032J card
 */
typedef struct /* t2032j_card */ {
   void       *link;			/* Link to next card structure	     */
   int         ext;			/* CAMAC channel var for this card   */
   short int   first_chan;		/* First channel used in this card   */
   short int   num_chans;		/* Number of channels used	     */
   int         card_status;		/* Status of card		     */
   int         retry_count;		/* Calibration retry countdown	     */
   IOSCANPVT   scan_done;		/* I/O Event to post for this card   */
   SEM_ID      card_mutex;		/* Mutex to protect data array	     */
   float       val [NUM_T2032_CHAN];	/* Data values for each channel	     */
   int         status [NUM_T2032_CHAN];	/* Status values for each channel    */
} t2032j_card;

/* The following fields are "re-used" during record initialization	*/
#define	min_chan  pt2032j->first_chan	/* Min channel (during init)	*/
#define	max_chan  pt2032j->num_chans	/* Max channel (during init)	*/


/*=====================
 * t2032j_devInfo -- Device-dependent structure for T2032J records
 */
typedef struct /* t2032j_devInfo */ {
   t2032j_card  *pt2032j;	/* Pointer to card structure		*/
   int           channel;	/* Channel number for this record	*/
   float        *pdata;		/* Pointer to data value for this rec.	*/
   int          *pstatus;	/* Pointer to status value for this rec */
   float         offset;	/* Offset value for conversion to eng.  */
} t2032j_devInfo;

/************************************************************************/
/*  Local Function Declarations						*/
/************************************************************************/

/*---------------------
 * Utility Routines
 */
LOCAL t2032j_card  *t2032j_initStruct (int, int);
LOCAL void          t2032j_initCard (t2032j_card*);
LOCAL void          t2032j_lock_card (t2032j_card*, char*);
LOCAL void          t2032j_unlock_card (t2032j_card*, char*);

/*---------------------
 * Device Support Routines
 */
LOCAL STATUS        t2032j_initDevSup (int);
LOCAL STATUS        t2032j_initRecord (struct aiRecord*);
LOCAL STATUS        t2032j_read (struct aiRecord*);
LOCAL STATUS        t2032j_linconv (struct aiRecord*, int);
LOCAL STATUS        t2032j_get_ioint_info (int, struct dbCommon*, IOSCANPVT*);

/*---------------------
 * Scanner Task
 */
LOCAL void          t2032j_scanner ();


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
}devCamacT2032J={
	6,
	NULL,
	(DEVSUPFUN) t2032j_initDevSup,
	(DEVSUPFUN) t2032j_initRecord,
	(DEVSUPFUN) t2032j_get_ioint_info,
	(DEVSUPFUN) t2032j_read,
        (DEVSUPFUN) t2032j_linconv};


/************************************************************************/
/*  Local Variable Declarations						*/
/************************************************************************/

LOCAL int            init_delay;	/* Delay after init before mode set */
LOCAL int            scan_delay;	/* Delay time for scanner loop	    */

LOCAL int            retry_count;	/* Retry count for Q=0 conditions   */

LOCAL int            t2032j_scannerId;	/* Scanner task ID		    */
LOCAL t2032j_card   *t2032j_list;	/* List head for T2032J cards	    */

/************************************************************************/
/* t2032j_initStruct () -- Initialize T2032J card structure		*/
/*	o Allocate the structure, and initialize it.			*/
/*	o Initialize the CAMAC card.					*/
/*	o Return the address of the card structure or NULL if failed.	*/
/*									*/
/************************************************************************/

LOCAL t2032j_card  *t2032j_initStruct (int channel, int ext)
{
   int            i;			/* Loop counter			*/
   SEM_ID         mutex;		/* Mutex to guard data area	*/
   t2032j_card   *pt2032j;		/* Pointer to card structure	*/

  /*---------------------
   * Allocate the card structure
   */
   pt2032j = (t2032j_card *) calloc (1, sizeof(t2032j_card));
   if (pt2032j == NULL) return (NULL);

  /*---------------------
   * Create the mutex to guard the card's data area.
   * Abort if we could not create the mutex.
   */
   mutex = semMCreate (SEM_Q_PRIORITY | SEM_DELETE_SAFE | SEM_INVERSION_SAFE);
   if (mutex != NULL) pt2032j->card_mutex = mutex;

   else /* Mutex could not be created.  Free the structure and abort */ {
      free (pt2032j);
      return (NULL);
   }/*end if could not create mutex*/

  /*---------------------
   * Create the I/O scan event to signal when this card has been read
   */
   scanIoInit (&(pt2032j->scan_done));

  /*---------------------
   * Initialize the minimum and maximum channel numbers for this card
   */
   min_chan = channel;
   max_chan = channel;

  /*---------------------
   * Initialize the hardware
   */
   pt2032j->ext = ext;		/* Store the CAMAC channel variable	*/
   t2032j_initCard (pt2032j);	/* Initialize the card			*/

  /*---------------------
   * Intialize the status and value arrays.
   *
   * Status for each channel is the status returned from initializing the
   * card.  Even if the status is OK, the value field will indicate UNDERFLOW
   * until the card is actually read.
   */
   for (i = 0; i < NUM_T2032_CHAN; i++) {
      pt2032j->status[i] = pt2032j->card_status;
      pt2032j->val[i]    = UNDERRANGE;
   }/*end for each channel*/

  /*---------------------
   * Add this structure to the linked list of all T2032J cards and
   * return the address of the structure.
   */
   pt2032j->link = t2032j_list;
   t2032j_list = pt2032j;

   return (pt2032j);

}/*end t2032j_initStruct()*/

/************************************************************************/
/* t2032j_initCard () -- Initialize T2032J CAMAC card			*/
/*	o Reset the processor and specify IEEE floating point format	*/
/*	o Set the NoQ retry counter to its "initialize" value		*/
/*	o Return status in the "card_status" field			*/
/*									*/
/************************************************************************/

LOCAL void  t2032j_initCard (t2032j_card *pt2032j)
{
   int        b, c, n, a;		/* CAMAC address parameters	*/
   int16_t    dummy;			/* Dummy data word		*/
   int16_t    mode = IEEE_MODE;		/* Select IEEE mode		*/
   int        q;			/* Dummy Q response		*/

  /*---------------------
   * Reset the T2032J card and processor.
   */
   cssa (RESET, pt2032j->ext, &dummy, &q);
   ctstat (&(pt2032j->card_status));
   if (pt2032j->card_status != OK) return;

  /*---------------------
   * Wait awhile after initialization before setting the mode, otherwise
   * the write to the control register will be ignored.
   */
   taskDelay (init_delay);

  /*---------------------
   * Set the card to IEEE floating point mode.
   */
   cssa (WRITE_CONTROL, pt2032j->ext, &mode, &q);
   ctstat (&(pt2032j->card_status));
   if (pt2032j->card_status != OK) return;

  /*---------------------
   * If initialization was successfull, set the retry count so that we will
   * wait for the card to start scanning again.
   * Log that this card was initialized.
   */
   pt2032j->retry_count = retry_count;
   cgreg (pt2032j->ext, &b, &c, &n, &a);
   /*logMsg ("T2032J card initialized in branch %d crate %d slot %d.\n",
            b, c, n, 0, 0, 0);*/
   return;

}/*end t2032j_initCard()*/

/************************************************************************/
/* t2032j_lock_card () -- Lock data area of card structure		*/
/*									*/
/************************************************************************/

LOCAL void t2032j_lock_card (t2032j_card *pt2032j, char *caller)
{
   int    status;			/* Local status variable	*/

   status = semTake (pt2032j->card_mutex, WAIT_FOREVER);
   if (status != OK) {
      logMsg ("devAiCamacT2032J (%s) Unable to lock card\n",
             (int)caller, 0, 0, 0, 0, 0);
   }/*end if could not lock*/
   return;

}/*end t2032j_lock_card()*/



/************************************************************************/
/* t2032j_unlock_card () -- Unlock data area of card structure		*/
/*									*/
/************************************************************************/

LOCAL void t2032j_unlock_card (t2032j_card *pt2032j, char *caller)
{
   int    status;			/* Local status variable	*/

   status = semGive (pt2032j->card_mutex);
   if (status != OK) {
      logMsg ("devAiCamacT2032J (%s) Unable to unlock card\n",
             (int)caller, 0, 0, 0, 0, 0);
   }/*end if could not unlock*/
   return;

}/*end t2032j_unlock_card()*/

/************************************************************************/
/* t2032j_initDevSup () -- Initialize T2032J Device Support Package	*/
/*	o Before record initialization:					*/
/*	  - Initialize timing values.					*/
/*	  - Initialize the list head for T2032J card structures.	*/
/*	  - Initialize the scanner task id				*/
/*									*/
/*	o After record intialization:					*/
/*	  - Compute the number of active channels for each card.	*/
/*	  - Spawn the T2032J scanner task.				*/
/*									*/
/************************************************************************/

LOCAL STATUS t2032j_initDevSup (int after)
{
   t2032j_card   *pt2032j;	/* Pointer to T2032J card structure	*/
   int            status;	/* Local status variable		*/

  /*---------------------
   * If this call is before record initialization, initialize the timing
   * variables by converting them from seconds to clock ticks (or loop
   * iterations.  Then initialize the T2032J card-structure list head.
   */
   if (!after) {
      init_delay = vxTicksPerSecond * INIT_DELAY;	/* Delay after init */
      scan_delay = vxTicksPerSecond * SCAN_DELAY;	/* Scanner delay    */
      retry_count = RETRY_TIME / SCAN_DELAY;		/* Retry countdown  */
      t2032j_list = NULL;				/* List head	    */
      t2032j_scannerId = 0;				/* Scan task ID	    */
      return (OK);
   }/*end if first call*/

  /*---------------------
   * Second call.  Record initialization is complete.
   * Exit if we didn't find any T2032J records.
   */
   if (t2032j_list == NULL) return (OK);

  /*---------------------
   * Loop to compute the number of active channels for each card.
   * Note that "min_chan" and "max_chan" are aliased to the "first_chan" and
   * "num_chans" fields (respectively) of the card structure.
   */
   for (pt2032j = t2032j_list;  pt2032j;  pt2032j = pt2032j->link)
      pt2032j->num_chans = max_chan - min_chan + 1;

  /*---------------------
   * Start the T2032J scanner task
   */
   status = taskSpawn (
            "t2032jScan",		/* Task name			*/
            SCANNER_PRIO,		/* Task priority		*/
            VX_FP_TASK,			/* Enable floating point	*/
            3000,			/* Stack size			*/
            (FUNCPTR) t2032j_scanner,	/* Entry point			*/
            0, 0, 0, 0, 0,		/* No arguments			*/
            0, 0, 0, 0, 0);

  /*---------------------
   * Report error if failed to create task
   */
   if (status == ERROR) {
      logMsg ("devAiCamacT2032J (t2032j_initDevSup) Failed to create T2032J scanner task\n",
              0, 0, 0, 0, 0, 0);
      return ERROR;
   }/*end if could not create scanner task*/

  /*---------------------
   * If no error, store the scanner task id and exit
   */
   t2032j_scannerId = status;
   return (OK);

}/*end t2032j_initDevSup()*/

/************************************************************************/
/* t2032j_initRecord () -- Record Initialization Routine		*/
/*									*/
/************************************************************************/

LOCAL STATUS t2032j_initRecord (struct aiRecord *pai)
{
   int              channel;	/* T2032J channel number for this record     */
   int              ext;	/* CAMAC channel variable for this card	     */
   struct camacio  *pcamacio;	/* Pointer to camacio portion of INP field   */
   t2032j_devInfo  *pdevInfo;	/* Pointer to record's device info structure */
   t2032j_card     *pt2032j;	/* Pointer to T2032J card structure	     */

  /*---------------------
   * Make sure the record type is valid
   */
   if (pai->inp.type != CAMAC_IO) {
      recGblRecordError (S_db_badField, (void *)pai,
         "devAiCamacT2032J (t2032j_initRecord) Illegal INP.type field");
      return(S_db_badField);
   }/*end if record type not CAMAC_IO*/

  /*---------------------
   * Make sure the channel number is valid
   */
   pcamacio = &(pai->inp.value.camacio);
   channel = atoi ((char *)pcamacio->parm);
   if ((unsigned int)channel >= NUM_T2032_CHAN) {
      recGblRecordError (S_db_badField, (void *)pai,
         "devAiCamacT2032J (t2032j_initRecord) Illegal INP.parm field");
      return(S_db_badField);
   }/*end if invalid channel number*/

  /*---------------------
   * Allocate and initialize the device-specific information structure.
   * Abort if not enough memory.
   */
   pdevInfo = (t2032j_devInfo *) malloc (sizeof(t2032j_devInfo));
   if (pdevInfo != NULL) pai->dpvt = (long *)pdevInfo;
   else return (ENOMEM);

   pdevInfo->channel = channel;
   pdevInfo->pdata   = NULL;
   pdevInfo->pstatus = NULL;

  /*---------------------
   * Check to see if a card structure already exists for this card
   */
   cdreg (&ext, pcamacio->b, pcamacio->c, pcamacio->n, 0);
   for (pt2032j = t2032j_list;  pt2032j;  pt2032j = pt2032j->link)
      if (pt2032j->ext == ext) break;

  /*---------------------
   * If we don't already have a card structure for this card, allocate one.
   * Abort if there is not enough memory.
   */
   if (!pt2032j) pt2032j = t2032j_initStruct (channel, ext);
   if (pt2032j) pdevInfo->pt2032j = pt2032j;
   else return (ENOMEM);

  /*---------------------
   * Compute the minimum and maximum channel numbers seen so far.
   */
   if (channel < min_chan) min_chan = channel;
   if (channel > max_chan) max_chan = channel;

  /*---------------------
   * Return after initializing the slope for linear conversions and
   * registering the card.
   */
   t2032j_linconv (pai, 0);
   camacRegisterCard (pcamacio->b, pcamacio->c, pcamacio->n,
                     "T2032J Scanning DVM", NULL, 0);

   return (OK);

}/*end t2032j_initRecord()*/

/************************************************************************/
/* t2032j_read () -- Read value for this channel from local buffer	*/
/*									*/
/************************************************************************/

LOCAL STATUS t2032j_read (struct aiRecord *pai)
{
   register int     channel;	/* T2032J channel number for this record    */
   register double  data;	/* Data value for this channel		    */
   t2032j_devInfo  *pdevInfo;	/* Pointer to device-specific information   */
   register int    *pstatus;	/* Pointer to status value for this channel */
   t2032j_card     *pt2032j;	/* Pointer to card-specific information	    */
   register int     status;	/* Status value for this channel	    */

  /*---------------------
   * Get the addresses of the device-dependent structure, the card structure,
   * and the channel status.
   */
   pdevInfo = (t2032j_devInfo *) pai->dpvt;
   pt2032j = pdevInfo->pt2032j;
   pstatus = pdevInfo->pstatus;

  /*---------------------
   * If this is the first time to process this record, compute the address
   * of the data and status values for this channel.
   */
   if (!pstatus) {
      pt2032j = pdevInfo->pt2032j;
      channel = pdevInfo->channel - pt2032j->first_chan;
      pdevInfo->pdata = &(pt2032j->val[channel]);
      pdevInfo->pstatus = pstatus = &(pt2032j->status[channel]);
   }/*end if first time through*/

  /*---------------------
   * Get the data and status values for this record
   */
   t2032j_lock_card (pt2032j, "t2032j_read");
   status = *pstatus;
   data   = *(pdevInfo->pdata);
   t2032j_unlock_card (pt2032j, "t2032j_read");

  /*---------------------
   * Check the status value for this channel.  If there was an error,
   * raise an alarm, and quit.
   */
   if (status != OK) {
      recGblSetSevr (pai, READ_ALARM, INVALID_ALARM);
      return (DO_NOT_CONVERT);
   }/*end if hardware error*/

  /*---------------------
   * Check the value of the data word and raise an alarm if it is out of
   * range.  This can happen during the T2032 initialization sequence.
   */
   if (fabs(data) > MAX_RANGE) {
      recGblSetSevr (pai, READ_ALARM, INVALID_ALARM);
      return (DO_NOT_CONVERT);
   }/*end if range error*/

  /*---------------------
   * Convert the data value to engineering and raw units.
   * Note that the value is already in floating point format, so we
   * bypass the conversion in the record support layer.  The raw value
   * is stored in "millivolt" units.
   */
   pai->rval = data * 1000.0;
   pai->val  = (data + pdevInfo->offset)*pai->eslo + pai->egul;
   pai->udf  = FALSE;

   return (DO_NOT_CONVERT);

}/*end t2032j_read()*/

/************************************************************************/
/* t2032j_get_ioint_info () -- Get value of I/O event			*/
/*									*/
/************************************************************************/

LOCAL STATUS t2032j_get_ioint_info (int cmd, struct dbCommon *precord,
                                    IOSCANPVT *ppvt)
{
   t2032j_devInfo   *pdevInfo;	/* Pointer to device-specific information */

   pdevInfo = (t2032j_devInfo *) precord->dpvt;
   *ppvt = pdevInfo->pt2032j->scan_done;
   return (OK);

}/*end t2032j_get_ioint_info()*/

/************************************************************************/
/* t2032j_linconv () -- Set linear conversion slope and offset		*/
/*	o Determine whether this record is bipolar or unipolar base	*/
/*	  on the values of EGUF and EGUL.				*/
/*	o Set slope (ESLO) and offset (in device-dependent structure)	*/
/*	  accordingly.							*/
/*									*/
/************************************************************************/

LOCAL STATUS t2032j_linconv (struct aiRecord *pai, int after)
{
   int              bipolar;	/* True if this is a bipolar record	*/
   t2032j_devInfo  *pdevInfo;	/* Pointer to device information struct	*/
   float            range;	/* Range to use in computing slope	*/

  /*---------------------
   * Get device-dependant structure and test for bipolar or unipolar device
   */
   pdevInfo = (t2032j_devInfo *) pai->dpvt;
   bipolar = (pai->eguf * pai->egul) < 0.0;

  /*---------------------
   * Set range and offset for bipolar device
   */
   if (bipolar) {
      range = BIPOLAR_RANGE;
      pdevInfo->offset = MAX_SCALE;
   }/*if bipolar device*/

  /*---------------------
   * Set range and offset for unipolar device
   */
   else {
      range = UNIPOLAR_RANGE;
      if (pai->egul < 0.0) pdevInfo->offset = MAX_SCALE;
      else pdevInfo->offset = 0.0;
   }/*if unipolar device*/

  /*---------------------
   * Set slope and return
   */
   pai->eslo = (pai->eguf - pai->egul) / range;
   return (OK);

}/*end t2032j_linconv*/

/************************************************************************/
/* t2032j_scanner () -- Scanner task for T2032J/BiRa5305 cards		*/
/*	o Periodically scan each card in the system			*/
/*	o Initialize the card if needed					*/
/*	o Read data for each card into a local buffer, correct for	*/
/*	  word-order problems (if any) and remove range/calib. info.	*/
/*	o Copy latest data and status values into the card's structure	*/
/*									*/
/************************************************************************/

LOCAL void t2032j_scanner ()
{
   t2032j_buffer           buffer;		/* Buffer for CAMAC read     */
   int                     channel_count;	/* Number of channels read   */
   int                     cb[4] = {0,0,0,0};	/* Control block for CAMAC   */
   int                     i;			/* Loop counter		     */
   uint16_t               *praw;		/* Raw data pointer	     */
   register t2032j_card   *pt2032j;		/* Pointer to card structure */
   int                     q;			/* Q status return	     */
   int                     status;		/* Local status variable     */
   uint16_t                temp;		/* Temp for word-swapping    */

  /*---------------------
   * Loop to periodically scan each T2032J card in the system
   */
   FOREVER {

     /*---------------------
      * Process each T2032J card on this IOC
      */
      for (pt2032j = t2032j_list;  pt2032j;  pt2032j = pt2032j->link) {

        /*---------------------
         * If this card needs to be initialized, intialize it.
         * If we could not initialize the card, don't try to read it.
         * If the failure status has not changed, don't even bother storing
         * the status code.
         */
         status = pt2032j->card_status;
         if (status != OK) {
            /*errPrintf (status, NULL, 0, "Error Detected in T2032J Read\n");*/
            t2032j_initCard (pt2032j);
            if ((pt2032j->card_status != OK) && (status != pt2032j->card_status)) continue;
            status = pt2032j->card_status;
         }/*end if card needed initializing*/

        /*---------------------
         * Initialize the number of words to read and the number of words
         * actually read.
         */
         cb[0] = 2 * pt2032j->num_chans;	/* Number of words requested */
         cb[1] = 0;				/* Number of words read      */

        /*---------------------
         * Load the starting channel into the card.
         */
         if (status == OK) {
            cssa (WRITE_CHANNEL, pt2032j->ext, &(pt2032j->first_chan), &q);
            ctstat (&status);
	  }/*end if card is intialized*/

        /*---------------------
         * If channel write was successfull, read out the active channels
         * on this card.
         */
         if (status == OK) {
            csubc (READ, pt2032j->ext, (int16_t *)buffer.rval, cb);
            ctstat (&status);
	 }/*end if channel write was successfull*/

        /*---------------------
         * If we successfully read all channels, then reset the retry counter
         */
         channel_count = cb[1] / 2;
         if (channel_count == pt2032j->num_chans)
            pt2032j->retry_count = retry_count;

        /*---------------------
         * If there was an error, then we may be in an initialization or
         * calibration sequence (indicated by Q=0 and no other error).
         * If this is the case and the retry counter has not expired, then
         * skip further processing and retry on next iteration.  Otherwise
         * post the error.
         */
         else {
            if ((status == OK) || (status == S_camacLib_noQ)) {
               if (pt2032j->retry_count <= 0) status = S_camacLib_noQ;
               else {
                  pt2032j->retry_count -= 1;
                  continue;
               }/*end if retry count not expired*/
            }/*end if error was noQ*/
         }/*end if not all channels read*/

        /*---------------------
         * Lock access to the card structure's data area while we load the
         * new data and status values into it.
         */
         t2032j_lock_card (pt2032j, "t2032j_scanner");

        /*---------------------
         * For each successful read, remove the range and calibration data,
         * correct for word order, and store the value and status in the
         * card structure.
         */
         praw = buffer.rval;
         for (i = 0;  i < channel_count;  i++) {
            temp = *praw & LOW_DATA_MASK;
            *praw++ = *(praw + 1);
            *praw++ = temp;
            pt2032j->val[i] = buffer.fval[i];
            pt2032j->status[i] = OK;
         }/*end for each channel successfully read*/

        /*---------------------
         * For each unsuccessful read, store the failure code.
         * Do not change the data value.
         */
         for (i = channel_count;  i < pt2032j->num_chans;  i++) {
            pt2032j->status[i] = status;
         }/*end for each unsuccessfull channel*/

        /*---------------------
         * Unlock the card, store the card status, and signal that the
         * scan is done for this card.
         */
         t2032j_unlock_card (pt2032j, "t2032j_scanner");
         pt2032j->card_status = status;
         scanIoRequest (pt2032j->scan_done);

      }/*end for each card*/

     /*---------------------
      * Take a breather while the cards are scanning new values.
      */
      taskDelay (scan_delay);

   }/*end FOREVER loop*/

}/*end t2032j_scanner()*/
