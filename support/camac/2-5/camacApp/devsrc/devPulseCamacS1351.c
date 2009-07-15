/******************************************************************************
 * devPulseCamacS1351.c -- Device Support Routines for Sension 1351
 *                         Pulse Delay Module
 *
 *----------------------------------------------------------------------------
 * Author:	Eric Bjorklund
 * Date:	11/30/94
 *
 *----------------------------------------------------------------------------
 * MODIFICATION LOG:
 *
 * 11-30-94	Bjo	Initial release
 * 07-26-95	Bjo	Fix problem with DLY not processing when VAL written
 * 10-26-95	Bjo	Added support for cascading delay registers
 *
 *----------------------------------------------------------------------------
 * MODULE DESCRIPTION:
 *
 * This module provides EPICS CAMAC device support for the Sension 1351
 * delayed pulse generator card.  Two variants are supported:
 *   S1351  -- uses the card in its normal mode, with four 12-bit delay
 *             registers.
 *   S1351L -- puts the card in "cascaded" mode so that it has two 20-bit
 *             delay registers.
 *
 *----------------------------------------------------------------------------
 * CARD DESCRIPTION:
 *
 * The Sension 1351 is a four-channel delayed pulse generator module.  Each
 * channel has a separate 12-bit delay register.  However, all four channels
 * share a common length register, clock, and external trigger.  Consequently,
 * the only controllable value in an S1351 record is the delay (DLY) field
 * (and, by implication the VAL field -- which is actually controlled by
 * adjusting the DLY field).  Values for the clock rate (CLKR), and pulse
 * width (WIDE) registers are set at record initialization time.  Each record
 * for a delay channel on the same S1351 card must have the same values for
 * the clock rate (CLKR) and pulse width (WIDE) fields.
 *
 * The delay channels can be cascaded, yielding two 20-bit delay registers
 * instead of four 12-bit registers.  In cascade mode, the pulse outputs only
 * appear on channels 0 and 2 (1 and 3 as they are labled on the front panel).
 *
 * The S1351 has five clock rates; 10 Khz (1.0e4 hz), 100 Khz (1.0e5 hz),
 * 1 Mhz (1.0e6 hz), 10 Mhz (1.0e7 hz), and 100 Mhz (1.0e8 hz).  The minimum
 * delay (as measured by scope) at the fastest clock rate (100 Mhz) is 80
 * nanoseconds.  For slower clock rates, the minimum delay is one tick plus 80
 * nanoseconds (note that for rates of 1 Mhz or less, the 80 nanoseconds
 * becomes insignificant).
 *
 * The minimum and maximum delay and width values for each clock rate are
 * shown in the tables below:
 *
 *   Clk Rate      Min Delay       Max Delay  Max (cascaded)   Delay Increment
 *   --------      ---------    ------------  --------------   ---------------
 *     10 Khz       100 uSec    409.600 mSec    104.858  Sec          100 uSec
 *    100 Khz        10 uSec     40.960 mSec     10.486  Sec           10 uSec
 *      1 Mhz      1.08 uSec      4.096 mSec      1.049  Sec            1 uSec
 *     10 Mhz       180 nSec    409.680 uSec    104.858 mSec          100 nSec
 *    100 Mhz        80 nSec     41.040 uSec     10.486 mSec           10 nSec
 *
 *
 *   Clk Rate      Min Width       Max Width  Width Increment
 *   --------      ---------    ------------  ---------------
 *     10 Khz       1.6 mSec    409.600 mSec	     1.6 mSec
 *    100 Khz     160.0 uSec     40.960 mSec	   160.0 uSec
 *      1 Mhz      16.0 uSec      4.096 mSec        16.0 uSec
 *     10 Mhz       1.6 uSec    409.600 uSec         1.6 uSec
 *    100 Mhz     160.0 nSec     40.960 uSec       160.0 nSec
 *
 * Some other interesting facts about the S1351 card and associated device
 * support:
 *
 *  o The card does not normally return Q=1 for valid operations.  Q
 *    is only used for test operations (F8 and F27).
 *
 *  o After writing the delay or width registers, device support rewrites
 *    the DLY or WIDE value by converting the raw value back to engineering
 *    units.  Thus the value contained in the DLY, WIDE, and VAL fields
 *    represents the actual delay and width values.  For example, if the
 *    clock rate is 100 Mhz, then the values in the DLY, WIDE, and VAL fields
 *    are always rounded to the nearest 10 nanoseconds.
 *
 *----------------------------------------------------------------------------
 * RECORD FIELDS OF INTEREST:
 *
 * The following fields should be set by database configuration:
 *	Type	= pulse
 *	DTYP	= Camac S1351
 *		  Camac S1351L
 *	OUT	= B -> Branch number of S1351 card
 *		  C -> Crate number of S1351 card
 *		  N -> Slot number of S1351 card
 *		  PARM -> Delay channel number (0-3 for S1351, 0-1 for S1351L).
 *	CLKR	= Clock Rate (in Hz.) for this card.  Note that all four delay
 *		  delay channels must have the same value for CLKR.
 *		  Valid values are (1e4, 1e5, 1e6, 1e7, 1e8).
 *	UNIT	= Units for VAL, DLY, WIDE, MAXD, MIND, MAXW, and MINW fields.
 *		  Valid values for UNIT are:
 *		    ("Seconds", "mSec", "uSec", "nSec").
 *	DLY	= Initial value for this delay channel.  If PINI is "True",
 *		  this value will be loaded into the delay register at IOC
 *		  initialization time.
 *	MAXD	= Maximum value for delay.  Record support will not write any
 *		  value higher than this to the DLY field.
 *	MIND	= Minimum value for delay.  Record support will not write any
 *		  value lower than this to the DLY field.
 *	WIDE	= Pulse width for this card.  Note that all four delay channels
 *		  must have the same value for WIDE.
 *	SDOF	= System Delay Offset value.  If not zero, this field
 *		  represents the "system delay."  Device support must subtract
 *		  this value from the DLY field before converting it to the
 *		  raw value.  SDOF represents a raw delay value of zero.
 *	PINI	= If set, the delay value will be written at record init time.
 *
 * Other fields of interest:
 *	PFLD	= Field processing mask.  Record support sets a mask in this
 *		  field to indicate which field or fields are being processed.
 *		  The "process" routine uses this field to determine what
 *		  to do and which values to change.
 *	MAXV	= Maximum value for VAL field.  Record support uses this value
 *		  to limit values written to the VAL field.  Device support for
 *		  the S1351 card initializes this field based on the values in
 *		  the MAXD and WIDE fields.
 *	MINV	= Minimum value for VAL field.  Record support uses this value
 *		  to limit values written to the VAL field.  Device support for
 *		  the S1351 card initializes this field based on the values in
 *		  the MIND, SDOF, and WIDE fields.
 *	RDLY	= Raw delay value.  This is the value actually written to the
 *		  delay register.  Device support calculates this value from
 *		  the DLY field.
 *	RWID	= Raw width value.  This is the value actually written to the
 *		  pulse width register.  Device support calculates this value
 *		  from the WIDE field.
 *	VAL	= Contains the pulse end value (delay plus width).  Note that
 *		  this field is controllable.  Device support for the S1351
 *		  adjusts this field by adjusting the pulse delay.
 *
 *----------------------------------------------------------------------------
 * RECOMMENDATIONS FOR RANGE AND SCALING:
 *
 * The clock rate (CLKR), units (UNIT), precision (PREC), delay offset (SDOF),
 * and limits (MAXD, MIND) fields are all mostly independant and give the
 * system designer a great deal of flexibility as to how the pulse data is
 * displayed, translated, and interpreted.  Not all of these options are
 * particularly useful, however.
 *
 * As a starting place for implementing S1351 records, the following table
 * lists recommended values for the CLKR, UNIT, PREC, SDOF, MAXD, and MIND
 * fields for various clock rates.  Note that this table assumes there are
 * no additional trigger delays other than the S1351, and that there are no
 * additional constraints on pulse width or delay.
 *
 *                                Clock Rate
 *              10 Khz   100 Khz     1 Mhz    10 Mhz   100 Mhz
 *             -------   -------   -------   -------   -------
 *
 *  CLKR           1e4       1e5       1e6       1e7       1e8
 *  UNIT          mSec      mSec      uSec      uSec      uSec
 *  PREC             1         2         0         1         2
 *  SDOF           0.0       0.0       0.0      0.18      0.08
 *  MIND           0.1      0.01       1.0       0.0       0.0
 *  MAXD         409.6     40.96    4096.0     409.6     41.04
 *
 *****************************************************************************/

/************************************************************************/
/*  Header Files							*/
/************************************************************************/

#include  <sys/types.h>		/* Standard Data Type Definitions	    */
#include  <stdlib.h>		/* Standard C Routines			    */
#include  <stdio.h>		/* Standard I/O Routines		    */
#include  <string.h>		/* Standard String Handling Rtns	    */
#include  <math.h>		/* Standard math functions		    */

#include  <vxWorks.h>		/* VxWorks Definitions & Routines	    */
#include  <logLib.h>		/* VxWorks Message Logging Library	    */
#include  <semLib.h>		/* VxWorks Semaphore Library		    */

#include  <dbDefs.h>		/* EPICS Standard Definitions		    */
#include  <dbAccess.h>		/* EPICS Data Base Message Codes	    */
#include  <recSup.h>		/* EPICS Record Support Definitions	    */
#include  <devSup.h>		/* EPICS Device Support Definitions	    */
#include  <devLib.h>		/* EPICS Device Support Library Definitions */
#include  <link.h>		/* EPICS db Link Field Definitions	    */

#include  <choicePulse.h>	/* Pulse Record Choice & Field Definitions  */
#include  <pulseRecord.h>	/* Pulse Record Definitions		    */

#include  <camacLib.h>		/* EPICS/ESONE CAMAC Library Routines	    */
#include  <devCamac.h>		/* Standard CAMAC Device Support Routines   */

/************************************************************************/
/*  Local Constants							*/
/************************************************************************/

/*---------------------
 * CAMAC Function codes used by this module
 */
#define	WT1			16	/* Group 1 write		     */
#define	WT2			17	/* Group 2 write		     */

/*---------------------
 * S1351 Register Sub-Address Definitions
 */
#define	REG_MODE		0	/* Mode register		     */
#define	REG_CLOCK_RATE		0	/* Clock rate register		     */
#define	REG_PULSE_WIDTH		5	/* Pulse width register		     */
#define	REG_LAM_MASK		13	/* LAM mask register		     */

/*---------------------
 * S1351 Mode Register Definitions
 */
#define	MODE_HW_TRIG_ENA	0x0001	/* Enable hardware trigger	     */
#define	MODE_SW_TRIG_ENA	0x0002	/* Enable software trigger	     */
#define	MODE_CASCADE		0x0004	/* Cascade delay register pairs	     */

/*---------------------
 * Other constants pertaining to the S1351 card
 */
#define	S1351_MIN_DELAY		1.0	/* Minimum delay (unitless)	     */

#define	S1351_MAX_RAW_WIDTH	0xff	/* Maximum width (raw units)	     */
#define	S1351_MAX_WIDTH		4096.0	/* Maximum width (unitless)	     */
#define	S1351_MIN_WIDTH		16.0	/* Minimum width (unitless)	     */

#define	S1351_NUM_RANGE		5	/* Number of range values	     */

#define	S1351_MAX_UNIT		REC_PULSE_UNIT_NSEC

/*---------------------
 * Constants pertaining to the S1351 (normal) variant
 */
#define	S1351_MAX_RAW_DELAY	0xfff	/* Maximum raw delay		     */
#define	S1351_MAX_DELAY		4096.0	/* Maximum delay (unitless)	     */
#define	S1351_NUM_CHAN		4	/* Number of delay channels per card */

/*---------------------
 * Constants pertaining to the S1351L (cascaded) variant
 */
#define S1351L_MAX_RAW_DELAY  0xfffff	/* Maximum raw delay		     */
#define S1351L_MAX_DELAY      1048576.0	/* Maximum delay (unitless)	     */
#define	S1351L_NUM_CHAN	      2		/* Number of delay channels per card */

/************************************************************************/
/*  Local Structure Declarations					*/
/************************************************************************/


/*=====================
 * s1351_card -- Structure describing width, range, and status for one S1351 card
 */
typedef struct /* s1351_card */ {
   void       *link;		/* Link to next card structure		*/
   int         ext;		/* CAMAC channel ID for this card	*/
   int         cascade;		/* TRUE if delay channels are cascaded	*/
   int         card_status;	/* Status of card			*/
   uint16_t    raw_width;	/* Pulse width for this card (raw)	*/
   int16_t     range_index;	/* Range index for this card		*/
   char       *firstPV;		/* Name of first PV on this card	*/
} s1351_card;


/*=====================
 * s1351_devInfo -- Device-dependent structure for S1351 records
 */
typedef struct /* s1351_devInfo */ {
   double        scale;		/* Scale factor for raw conversion	*/
   int           max_raw_delay;	/* Maximum raw delay value		*/
   int           cascade;	/* TRUE if delay channels are cascaded	*/
   int           ext_lo;	/* CAMAC Channel for low-order delay	*/
   int           ext_hi;	/* CAMAC Channel for high-order delay	*/
   s1351_card   *pcard;		/* Pointer to card structure		*/
} s1351_devInfo;


/*=====================
 * pulse_dset -- Device Support Entry Table for pulse records
 */
typedef struct /* pulse_dset */ {
   long		number;
   DEVSUPFUN	report;
   DEVSUPFUN	init;
   DEVSUPFUN	init_record;
   DEVSUPFUN	get_ioint_info;
   DEVSUPFUN	write;
} pulse_dset;

/************************************************************************/
/*  Local Function Declarations						*/
/************************************************************************/

/*---------------------
 * Utility Routines
 */
LOCAL STATUS   s1351_initCard (s1351_card*, struct camacio*);
LOCAL STATUS   s1351_writeDelay (struct pulseRecord*);

/*---------------------
 * Device Support Routines
 */
LOCAL STATUS   s1351_initDevSup (int);
LOCAL STATUS   s1351_initRecord (struct pulseRecord*);
LOCAL STATUS   s1351L_initRecord (struct pulseRecord*);
LOCAL STATUS   s1351_commonRecInit (struct pulseRecord*, int, int);
LOCAL STATUS   s1351_write (struct pulseRecord*);


/************************************************************************/
/*  Device Support Entry Tables						*/
/************************************************************************/

pulse_dset devCamacS1351 = {	/* S1351 -- Regular Mode		*/
	5,				/* 5 entries in table		*/
	NULL,				/* -- No report routine		*/
	(DEVSUPFUN) s1351_initDevSup,	/* Device support init routine	*/
	(DEVSUPFUN) s1351_initRecord,	/* Record init routine		*/
	NULL,				/* -- No I/O Interrupt info	*/
	(DEVSUPFUN) s1351_write};	/* Write routine		*/

pulse_dset devCamacS1351L = {	/* S1351L -- Cascaded Mode		*/
	5,				/* 5 entries in table		*/
	NULL,				/* -- No report routine		*/
	(DEVSUPFUN) s1351_initDevSup,	/* Device support init routine	*/
	(DEVSUPFUN) s1351L_initRecord,	/* Record init routine		*/
	NULL,				/* -- No I/O Interrupt info	*/
	(DEVSUPFUN) s1351_write};	/* Write routine		*/

/************************************************************************/
/*  Local Variable Declarations						*/
/************************************************************************/


/*=====================
 * clock_rate -- Valid values for the external clock rate (CLKR field)
 */
static double const    clock_rate[S1351_NUM_RANGE] = {
   1e4,		/*  10 Khz	*/
   1e5,		/* 100 Khz	*/
   1e6,		/*   1 Mhz	*/
   1e7,		/*  10 Mhz	*/
   1e8		/* 100 Mhz	*/
};

/*=====================
 * range_mask -- Mask values for setting the time range register
 */
static uint16_t const  range_mask[S1351_NUM_RANGE] = {
   0x01,	/*  10 Khz	*/
   0x02,	/* 100 Khz	*/
   0x04,	/*   1 Mhz	*/
   0x08,	/*  10 Mhz	*/
   0x10		/* 100 Mhz	*/
};

/*=====================
 * range_scale -- Scaling constants for converting values in nanosecond units
 *                to raw values based on the selected time range.
 */
static long int const  range_scale[S1351_NUM_RANGE] = {
   100000,	/*  10 Khz	*/
    10000,	/* 100 Khz	*/
     1000,	/*   1 Mhz	*/
      100,	/*  10 Mhz	*/
       10	/* 100 Mhz	*/
};

/*=====================
 * time_const -- Conversion constants for converting values to nanosecond
 *               units from the units specified in the UNITS field
 */
static double const    time_const[5] = {
   1e9,		/* Seconds	*/
   1e6,		/* Milliseconds	*/
   1e3,		/* Microseconds	*/
   1e0,		/* Nanoseconds	*/
   1e-3		/* Picoseconds	*/
};

/*=====================
 * s1351_list -- List head for linked list of all S1351 cards on this IOC
 */
LOCAL s1351_card     *s1351_list;

/************************************************************************/
/* 			 Utility Macro Routines				*/
/*									*/


/*----------------------------------------------------------------------*/
/* InitAbort () -- Abort Record Initialization				*/
/*									*/
/*	This macro is called to abort the record initialization		*/
/*	routine on error.  It performs the following actions:		*/
/*	o Reports an error to the local console				*/
/*	o Aborts the record initialization routine with the specified	*/
/*	  status code.							*/
/*	o Note that aborting the initialization of a pulse record will	*/
/*	  disable that record from any further processing.		*/
/*									*/
/*	Macro parameters are:						*/
/*	   status  = Error status code					*/
/*	   message = Message to output along with standard error msg.	*/
/*									*/
/*	The following variables are assumed to be declared with the	*/
/*	calling procedure:						*/
/*	   prec    = Pointer to record being initialized		*/
/*									*/
/*----------------------------------------------------------------------*/


#define InitAbort(status, message)					\
{									\
   recGblRecordError (status, (void *)prec,		 		\
   "devPulseS1351 (s1351_initRecord):\n-- " message);			\
   return (status);							\
									\
}/*end InitAbort()*/

/************************************************************************/
/*			Utility Routines				*/
/*									*/


/************************************************************************/
/* s1351_initCard () -- Initialize S1351 CAMAC card			*/
/*	o Initialize the card, clear LAMs, and LAM mask			*/
/*	o Load the pulse width register and the time-range register	*/
/*	o Enable the hardware trigger					*/
/*	o Returns OK if the hardware initialization succeeded.		*/
/*	  Returns error status if the initialization failed.		*/
/*	o NOTE: Does NOT issue an initialize (F25 A15).  This allows	*/
/*	  the card to continue generating pulses across an IOC reboot.	*/
/*									*/
/************************************************************************/

LOCAL STATUS  s1351_initCard (s1351_card *pcard, struct camacio *pcamacio)
{
   uint16_t   clear_mask;	/* Clear LAM Mask			*/
   int        ext;		/* CAMAC channel for mode register	*/
   uint16_t   mode;		/* Hardware mode value			*/
   int        q;		/* Q response				*/
   uint16_t   range;		/* Value for time range register	*/
   int        status;		/* Local status variable		*/

  /*---------------------
   * Make sure that LAMs are disabled by clearing the LAM mask register.
   */
   clear_mask = 0;
   cdreg (&ext, pcamacio->b, pcamacio->c, pcamacio->n, REG_LAM_MASK);
   cssa (WT2, ext, &clear_mask, &q);

  /*---------------------
   * Set the pulse width register.
   */
   cdreg (&ext, pcamacio->b, pcamacio->c, pcamacio->n, REG_PULSE_WIDTH);
   cssa (WT1, ext, &(pcard->raw_width), &q);

  /*---------------------
   * Set the clock rate register and enable the hardware trigger
   */
   ext = pcard->ext;
   range = range_mask[pcard->range_index];
   cssa (WT1, ext, &range, &q);

  /*---------------------
   * Set the hardware mode register
   */
   mode = MODE_HW_TRIG_ENA;			/* Enable hardware trigger  */
   if (pcard->cascade) mode |= MODE_CASCADE;	/* Cascaded delay registers */
   cssa (WT2, ext, &mode,  &q);

  /*---------------------
   * Check the hardware status.  Ignore NoQ.
   */
   ctstat (&status);
   if (status == S_camacLib_noQ) status = OK;
   pcard->card_status = status;
   return (status);

}/*end s1351_initCard()*/

/************************************************************************/
/* s1351_writeDelay () -- Write New Value to Delay Register		*/
/*									*/
/************************************************************************/

LOCAL STATUS   s1351_writeDelay (struct pulseRecord  *prec)
{
   int             cb [4] = {1,0,0,0};	/* Control block for csga routine    */
   uint16_t        delay [2];		/* Values for delay registers	    */
   int             exta [2];		/* CAMAC channel array for csga rtn  */
   static int      fa [2] = {WT1, WT1};	/* Function array for csga routine   */
   int             qa [2];		/* Q status array for csga routine   */

   int             offset;		/* Minimum delay offset		     */
   s1351_devInfo  *pdevInfo;		/* Pointer to device info structure  */
   int             raw_delay;		/* Delay value (in raw units)	     */
   int             status;		/* Local status variable	     */

  /*---------------------
   * Compute minimum delay offset based on clock rate (one tick)
   * Note, however, that if a system delay offset (SDOF) was specified,
   * we assume that the minimum delay is included.
   */
   pdevInfo = (s1351_devInfo *)prec->dpvt;
   if (prec->sdof > 0.0) offset = 0;
   else offset = 1;

  /*---------------------
   * Convert delay value to raw units
   */
   raw_delay = floor((prec->dly - prec->sdof)*pdevInfo->scale + 0.5) - offset;
   if (raw_delay < 0) raw_delay = 0;
   if (raw_delay > pdevInfo->max_raw_delay) return S_dev_hdwLimit;

  /*---------------------
   * Set up the data and channel arrays to write the new value to the
   * delay registers.  Note that we will only write one register if the card
   * is in "normal" mode.  In "cascade" mode, we write two registers.
   *
   * Note that in "cascade" mode, the high-order part of the delay is written
   * into the *upper* 8 bits of the second 12-bit register.
   */
   if (pdevInfo->cascade) cb[0] = 2;

   delay[0] = raw_delay & 0x0fff;
   delay[1] = (raw_delay >> 8) & 0x0ff0;

   exta[0] = pdevInfo->ext_lo;
   exta[1] = pdevInfo->ext_hi;

   csga (fa, exta, delay, qa, cb);

  /*---------------------
   * Store the actual (rounded) value back in the DLY field.
   */
   prec->rdly = raw_delay;
   prec->dly  = ((raw_delay + offset) / pdevInfo->scale) + prec->sdof;

  /*---------------------
   * Return the write status (ignore No Q)
   */
   ctstat (&status);
   if (status == S_camacLib_noQ) status = OK;
   return (status);

}/*end s1351_writeDelay()*/

/************************************************************************/
/*			Device Support Routines				*/
/*									*/



/************************************************************************/
/* s1351_initDevSup () -- Initialize S1351 Device Support Package	*/
/*	o Initialize the list head for S1351 card structures.		*/
/*	o Note that this is called for both S1351 and S1351L variants	*/
/*									*/
/************************************************************************/

LOCAL STATUS s1351_initDevSup (int after)
{
   if (!after) s1351_list = NULL;
   return (OK);

}/*end s1351_initDevSup()*/

/************************************************************************/
/* s1351_initRecord () -- Record Init Routine for S1351 Variant		*/
/*	o Validate the link type, and channel number.			*/
/*	o Register the card with the CAMAC driver.			*/
/*	o Invoke the common record initialization routine.		*/
/*									*/
/************************************************************************/

LOCAL STATUS s1351_initRecord (struct pulseRecord *prec)
{
   int              channel;	/* Delay channel number for this record	     */
   struct camacio  *pcamacio;	/* Pointer to camacio portion of OUT field   */
   char            *pend;	/* Pointer used to parse PARM field	     */

  /*---------------------
   * Make sure the record type is valid
   */
   if (prec->out.type != CAMAC_IO)
      InitAbort (S_dev_badOutType, "Link type not Camac");

  /*---------------------
   * Make sure the channel number is valid
   */
   pcamacio = &(prec->out.value.camacio);
   channel = strtol ((char *)pcamacio->parm, &pend, 10);
   if ((pend == (char *)pcamacio->parm) || ((unsigned int)channel >= S1351_NUM_CHAN))
      InitAbort (S_db_badField, "Illegal OUT.parm field");

  /*---------------------
   * Register the card with the CAMAC driver
   */
   camacRegisterCard (pcamacio->b, pcamacio->c, pcamacio->n,
                     "S1351  Pulse Delay Generator", NULL, 0);

  /*---------------------
   * Return after invoking common record initialization
   */
   return s1351_commonRecInit (prec, channel, FALSE);

}/*end s1351_initRecord()*/

/************************************************************************/
/* s1351L_initRecord () -- Record Init Routine for S1351L Variant	*/
/*	o Validate the link type, and channel number.			*/
/*	o Register the card with the CAMAC driver.			*/
/*	o Invoke the common record initialization routine.		*/
/*									*/
/************************************************************************/

LOCAL STATUS s1351L_initRecord (struct pulseRecord *prec)
{
   int              channel;	/* Delay channel number for this record	     */
   struct camacio  *pcamacio;	/* Pointer to camacio portion of OUT field   */
   char            *pend;	/* Pointer used to parse PARM field	     */

  /*---------------------
   * Make sure the record type is valid
   */
   if (prec->out.type != CAMAC_IO)
      InitAbort (S_dev_badOutType, "Link type not Camac");

  /*---------------------
   * Make sure the channel number is valid
   */
   pcamacio = &(prec->out.value.camacio);
   channel = strtol ((char *)pcamacio->parm, &pend, 10);
   if ((pend == (char *)pcamacio->parm) || ((unsigned int)channel >= S1351L_NUM_CHAN))
      InitAbort (S_db_badField, "Illegal OUT.parm field");

  /*---------------------
   * Register the card with the CAMAC driver
   */
   camacRegisterCard (pcamacio->b, pcamacio->c, pcamacio->n,
                     "S1351L Cascaded Pulse Delay Generator", NULL, 0);

  /*---------------------
   * Adjust the channel number for subaddress computation.
   * Return after invoking common record initialization.
   */
   channel = 2 * channel;
   return s1351_commonRecInit (prec, channel, TRUE);

}/*end s1351L_initRecord()*/

/************************************************************************/
/* s1351_commonRecInit () -- Common Record Initialization Routine	*/
/*	o Validate the unit field, and clock rate			*/
/*	o Allocate and initialize the device-dependant information	*/
/*	  block.							*/
/*	o Check for valid CAMAC address paramters.			*/
/*	o Check to see if a card structure exists for this card.	*/
/*	  - If card structure does not already exist, allocate and	*/
/*	    initialize the card structure and the card.			*/
/*	  - If card structure exists, make sure common parameters are	*/
/*	    compatible.							*/
/*	o Set appropriate limits for the VAL field.			*/
/*	o If PINI is set, flag record support to initialize DLY field.	*/
/*									*/
/************************************************************************/

LOCAL STATUS s1351_commonRecInit (struct pulseRecord *prec, int channel, int cascade)
{
   char             errmsg[100];/* String for initialization error messages  */
   int              ext;	/* CAMAC channel variable for this card	     */
   double           maxd;	/* Maximum value for delay		     */
   double           mind;	/* Minimum value for delay		     */
   struct camacio  *pcamacio;	/* Pointer to camacio portion of OUT field   */
   s1351_devInfo   *pdevInfo;	/* Pointer to record's device info structure */
   s1351_card      *pcard;	/* Pointer to s1351 card structure	     */
   int              range_index;/* Index to card's time-range value	     */
   int              raw_width;	/* Width in raw units			     */
   int              status;	/* Local status variable		     */

  /*---------------------
   * Make sure the UNIT field is valid (Note: we don't allow picosecond units)
   */
   if (prec->unit > S1351_MAX_UNIT)
      InitAbort (S_db_badField, "Illegal value in UNIT field");

  /*---------------------
   * Get the time range (in Hz) and check to see if it is one of the
   * allowed values.
   */
   for (range_index = 0;  range_index < S1351_NUM_RANGE;  range_index++)
      if (clock_rate[range_index] == prec->clkr) break;

   if (range_index >= S1351_NUM_RANGE) 
      InitAbort (S_db_badField, "Illegal value in CLKR field");

  /*---------------------
   * Allocate the device-specific information structure.
   * Abort if not enough memory.
   */
   pdevInfo = malloc (sizeof(s1351_devInfo));
   if (pdevInfo == NULL)
      InitAbort (S_dev_noMemory, "Insufficient memory to allocate device information structure");

  /*---------------------
   * Compute the scale factor for this record based on the selected units
   * and the selected clock rate.
   */
   prec->dpvt = (long *)pdevInfo;
   pdevInfo->scale = time_const[prec->unit] / range_scale[range_index];

  /*---------------------
   * Compute the maximum raw delay value based on whether or not the delay
   * channels are cascaded.
   */
   pdevInfo->cascade = cascade;
   pdevInfo->max_raw_delay = cascade ? S1351L_MAX_RAW_DELAY : S1351_MAX_RAW_DELAY;

  /*---------------------
   * Convert the pulse width to raw units and check for overflow
   */
   raw_width = floor((prec->wide * pdevInfo->scale)/S1351_MIN_WIDTH - 0.5);
   if (raw_width < 0) raw_width = 0;
   prec->rwid = raw_width;
   if ((unsigned int)raw_width > S1351_MAX_RAW_WIDTH)
      InitAbort (S_dev_hdwLimit, "Value in WIDE field is too big");

  /*---------------------
   * Construct the CAMAC channel variable for the mode register.
   * This CAMAC variable serves to uniquely identify the S1351 card so that
   * we can know if we have already initialized this card.
   * Abort on CAMAC address errors.
   */
   pcamacio = &(prec->out.value.camacio);
   cdreg (&ext, pcamacio->b, pcamacio->c, pcamacio->n, REG_MODE);
   if (!ext) {
      ctstat (&status);
      InitAbort (status, "Illegal BCNA in OUT field");
   }/*end if error in CAMAC address*/

  /*---------------------
   * Check to see if a card structure already exists for this card
   */
   for (pcard = s1351_list;  pcard;  pcard = pcard->link)
      if (pcard->ext == ext) break;

  /*---------------------
   * If we don't already have a card structure for this card, allocate and
   * initialize one, then initialize the card itself.
   */
   if (!pcard) {

     /*---------------------
      * Allocate the card structure
      */
      pcard = malloc (sizeof(s1351_card));
      if (pcard == NULL)
         InitAbort (S_dev_noMemory, "Insufficient memory to allocate card structure");

     /*---------------------
      * Initialize the card structure
      */
      pcard->ext = ext;				/* CAMAC channel ID	    */
      pcard->cascade = cascade;			/* Cascaded or not	    */
      pcard->raw_width = raw_width;		/* Pulse width in raw units */
      pcard->range_index = range_index;		/* Index to time-range	    */
      pcard->firstPV = (char *) &(prec->name);	/* Name of first record	    */

     /*---------------------
      * Add this structure to the linked list of all S1351 cards
      */
      pcard->link = s1351_list;
      s1351_list = pcard;

     /*---------------------
      * Initialize the hardware.  If initialization failed, report a message
      * but don't abort yet.
      */
      status = s1351_initCard (pcard, pcamacio);
      if (status != OK) {
         sprintf (errmsg, "devPulseS1351 (s1351_initRecord):\n"
                          "-- Error initializing S1351 card on "
                          "branch %d, crate %d, slot %d",
                           pcamacio->b, pcamacio->c, pcamacio->n);
         recGblRecordError (status, (void *)prec, errmsg);
      }/*end if card initialization failed*/

   }/*end if card structure not found*/

  /*---------------------
   * If there already was a card structure for this card, make sure the
   * current record has the same pulse length, time range, and cascade setting
   */
   else
   {
      if (raw_width != pcard->raw_width) {
         sprintf (errmsg, "devPulseS1351 (s1351_initRecord):\n-- "
                 "WIDE field does not agree with width in %s",
                  pcard->firstPV);
         recGblRecordError (S_db_badField, (void *)prec, errmsg);
         return S_db_badField;
      }/*end if widths are not the same*/

      if (range_index != pcard->range_index) {
         sprintf (errmsg, "devPulseS3151 (s1351_initRecord):\n-- "
                 "CLKR field does not agree with clock rate in %s",
                  pcard->firstPV);
         recGblRecordError (S_db_badField, (void *)prec, errmsg);
         return S_db_badField;
      }/*end if clock rates are not the same*/

      if (cascade != pcard->cascade) {
         sprintf (errmsg, "devPulseS3151 (s1351_initRecord):\n-- "
                 "This record and %s have different DTYP's for same card",
                  pcard->firstPV);
         recGblRecordError (S_dev_wrongDevice, (void *)prec, errmsg);
         return S_dev_wrongDevice;
      }/*end if DTYP fields do not match*/

   }/*end if not first channel on this card*/

  /*---------------------
   * Finish initializing the device-dependant information structure by storing
   * the address of the card structure and constructing the CAMAC channels
   * variable for writing to the delay register.
   */
   pdevInfo->pcard = pcard;
   cdreg (&pdevInfo->ext_lo, pcamacio->b, pcamacio->c, pcamacio->n, channel+1);
   cdreg (&pdevInfo->ext_hi, pcamacio->b, pcamacio->c, pcamacio->n, channel+2);

  /*---------------------
   * Compute the actual pulse width from the raw value.
   */
   prec->wide = S1351_MIN_WIDTH * (raw_width + 1) / pdevInfo->scale;

  /*---------------------
   * Set limits for the VAL field based on the pulse width and the limits
   * on the DLY field.  If no limits have been set on the DLY field, then
   * use default limits based on the clock rate and the units.
   */
   if (prec->maxd > prec->mind) {
      maxd = prec->maxd;
      mind = prec->mind;
   }/*end if delay limits were specified*/

   else {
      maxd = cascade ? S1351L_MAX_DELAY : S1351_MAX_DELAY;
      maxd = maxd / pdevInfo->scale;
      mind = S1351_MIN_DELAY / pdevInfo->scale;
   }/*end if delay limits not specified*/

   prec->maxv = prec->wide + maxd;
   prec->minv = prec->wide + max(prec->sdof, mind);

  /*---------------------
   * Set a few other non-negotiable fields
   */
   prec->env  = REC_PULSE_ENV_ENABLE;	/* Trigger is enabled		*/
   prec->tsrc = REC_PULSE_TSRC_HW;	/* Hardware trigger		*/
   prec->tmod = REC_PULSE_TMOD_RISING;	/* Trigger on rising edge	*/

  /*---------------------
   * Now that the record initialization is essentially complete, check to see
   * if the card successfully initialized.  If not, disable the record, since
   * we can't guarantee that the pulse delay and width will be correct.
   */
   if (pcard->card_status != OK) return (pcard->card_status);

  /*---------------------
   * If the PINI field is true, indicate that DLY is to be written.
   */
   if (prec->pini) prec->pfld = REC_PULSE_PFLD_DLY;
   return OK;

}/*end s1351_initRecord()*/

/************************************************************************/
/* s1351_write () -- Write values to hardware				*/
/*	o Note that the S1351 device support module only allows you to	*/
/*	  to write the DLY and VAL fields.				*/
/*	o Process VAL field by modifying DLY.				*/
/*									*/
/************************************************************************/

LOCAL STATUS s1351_write (struct pulseRecord *prec)
{
   register unsigned short  procMask;	/* Mask of which fields to write    */
   register int             status;	/* Local status variable	    */

  /*---------------------
   * Check the processing mask to find out why we were called.
   * Quit if the mask is zero (we were processed just for the hell of it).
   */
   status = OK;
   procMask = prec->pfld;
   if (!procMask) return OK;

  /*---------------------
   * Process the VAL field by modifying the pulse delay.  Set flag in PFLD
   * so that record processing will know we modified the DLY field.
   */
   if (procMask & REC_PULSE_PFLD_VAL) {
      prec->dly = prec->val - prec->wide;
      prec->pfld |= REC_PULSE_PFLD_DLY;
   }/*end if VAL field being processed*/

  /*---------------------
   * Process the delay (DLY) field.
   */
   if (procMask & (REC_PULSE_PFLD_DLY | REC_PULSE_PFLD_VAL))
      status = s1351_writeDelay (prec);

  /*---------------------
   * Signal an error for unsupported fields and restore their previous values
   */
   if (procMask & (REC_PULSE_PFLD_WIDE | REC_PULSE_PFLD_ENV | REC_PULSE_PFLD_STV | REC_PULSE_PFLD_TVHI | REC_PULSE_PFLD_TVLO)) {
      prec->wide = prec->owid;
      prec->tvhi = prec->otvh;
      prec->tvlo = prec->otvl;
      prec->env  = prec->oenv;
      status = S_dev_badRequest;
   }/*end if unsupported field*/

  /*---------------------
   * Return with final status
   */
   return status;

}/*end s1351_write()*/
