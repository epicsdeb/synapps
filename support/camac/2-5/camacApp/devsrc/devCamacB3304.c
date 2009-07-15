/******************************************************************************
 * devCamacB3304.c -- Device Support Routines for B3304 Binary Output Module
 *
 *----------------------------------------------------------------------------
 * Author:	Eric Bjorklund
 * Date:	03-Nov-1994
 *
 *----------------------------------------------------------------------------
 * MODIFICATION LOG:
 *
 * 03-Nov-1994	Bjo	First release
 * 05-Sep-1995	Bjo	Added support for mbbo and longout records
 * 09-Jan-1996	Bjo	Changes for EPICS R3.12
 *
 *----------------------------------------------------------------------------
 * MODULE DESCRIPTION:
 *
 * This module provides EPICS CAMAC device support for the BiRa 3304 binary
 * output card.  This code supports two variants of the BiRa 3304 card:
 *	B3304  = Momentary variant
 *	B3340L = Latched variant
 *
 * The only difference (from the device-support point of view) between the
 * momentary and latched variants, is that the latched variant employs a
 * hardware readback function so that it can be initialized from the actual
 * hardware value at boot time).
 *
 * Support is provided for "binary output" (bo), "multi-bit binary output"
 * (mbbo) and "longword output" (longout) records for both the momentary and
 * latched variants of the card.
 *
 * For longout records, the HOPR and LOPR fields may be used to control the
 * range and bipolar properties of the record.
 * o If HOPR and LOPR are both zero, the default range (24-bits) and polarity
 *   (unipolar) will be used.  Record initialization will set the values of
 *   HOPR and LOPR to the defaults so that OPI displays will display correct
 *   limits (e.g. for bar charts, etc.)
 * o If either HOPR or LOPR (or both) have non-zero values, these values will
 *   be used to compute the range mask.  For example, setting HOPR=255 and
 *   LOPR=0, will result in a longout record with an 8-bit (unipolar) range
 *   mask.  Higher order bits will be masked off.  If LOPR is a negative number
 *   and HOPR is positive or zero, the record will be considered "bipolar".
 *   For example, setting HOPR=255 and LOPR=-256 (or -255), will result in a
 *   bipolar longout record with a 9-bit range mask.
 *
 *----------------------------------------------------------------------------
 * CARD DESCRIPTION:
 *
 *  The BiRa 3304 is a 16-channel binary output module that may be strapped
 *  for several configurations including:
 *     Momentary or latched output
 *     Normally open or normally closed contacts
 *
 *  The card supports a readback function code (F0) but the readback
 *  will be meaningless in momentary mode.  The card does not support
 *  a function code to rewrite the entire register (such as F16).  This
 *  makes it somewhat ill-suited for latched multi-bit binary or longword
 *  output operations.  The reason for this is that latched multi-bit writes
 *  must be implemented by first clearing the desired zero bits (with a
 *  selective clear operation) and then setting all the desired one bits (with
 *  a selective set operation)  Although these two operations are performed
 *  atomically within the CAMAC device support routines,  There will still be
 *  a brief intermediate state between the original and final states.  Note
 *  that this is not a problem for cards strapped for momentary output, since
 *  only the selective set operation has any effect.
 *
 *----------------------------------------------------------------------------
 * RECORD FIELDS OF INTEREST:
 *
 * The following fields should be set by database configuration:
 *	Type	= bo, mbbo, or longout
 *	DTYP	= Camac B3304   or
 *		  Camac B3304L
 *	OUT	= B -> Branch number of B3304 card
 *		  C -> Crate number of B3304 card
 *		  N -> Slot number of B3304 card
 *		  PARM -> Starting bit number 0-15 (bo and mbbo records only)
 *
 *	HOPR	= (longout records only) High operating range value.  If
 *		  set, will override default range.
 *	LOPR	= (longout records only) Low operating range value.  If
 *		  set, will override default range.
 *	NOBT	= (mbbo records only) Number of bits in the value.
 *
 *****************************************************************************/

/************************************************************************/
/*  Header Files							*/
/************************************************************************/

#include	<dbDefs.h>	/* EPICS Standard Definitions		*/
#include	<recSup.h>	/* EPICS Record Support Definitions	*/
#include	<devSup.h>	/* EPICS Device Support Definitions	*/
#include	<devCamac.h>	/* Standard CAMAC Device Support	*/
#include	<camacLib.h>	/* ESONE CAMAC Library Definitions	*/
#include        <dset.h>        /* Standard DSET definitions		*/


/************************************************************************/
/*  Local Constants							*/
/************************************************************************/

#define DEFAULT_MASK	0xffff		/* Default range mask value	*/
#define	DEFAULT_MAX	0xffff		/* Default upper range value	*/
#define	DEFAULT_MIN	0		/* Default lower range value	*/


/************************************************************************/
/*  Function Prototypes							*/
/************************************************************************/

LOCAL STATUS   B3304_initBoRecord    (struct boRecord*);
LOCAL STATUS   B3304L_initBoRecord   (struct boRecord*);
LOCAL STATUS   B3304_initMbboRecord  (struct mbboRecord*);
LOCAL STATUS   B3304L_initMbboRecord (struct mbboRecord*);
LOCAL STATUS   B3304_initLoRecord    (struct longoutRecord*);
LOCAL STATUS   B3304L_initLoRecord   (struct longoutRecord*);

LOCAL STATUS   B3304L_loWrite        (struct longoutRecord*);
LOCAL STATUS   B3304_commonLoInit    (struct longoutRecord*, short int);

LOCAL void     B3304_registerCard    (register struct camacio*);
LOCAL void     B3304L_registerCard   (register struct camacio*);

/************************************************************************/
/*  Parameter Structures for B3304 bo and mbbo records			*/
/************************************************************************/

static devCamacParm  B3304_parms = { /* Parameters for "Momentary" cards*/
   F(18),				/* Function code		*/
   A(0),				/* Subaddress			*/
   NO_XPARM,				/* No readback function		*/
   0, 0					/* These fields not used	*/
};/*end B3304_parms*/

static devCamacParm  B3304L_parms = {/* Parameters for "Latched" cards	*/
   F(18),				/* Function code		*/
   A(0),				/* Subaddress			*/
   F(0),				/* Readback function code	*/
   0, 0					/* These fields not used	*/
};/*end B3304_parms*/

/************************************************************************/
/*  Device Support Entry Tables for B3304 Records			*/
/************************************************************************/

struct bodset devCamacB3304bo = {    /* Binary Output Records (momentary)   */
   5,					/* 5 entries			    */
   NULL,				/* -- No report routine		    */
   NULL,				/* -- No device support init	    */
   (DEVSUPFUN) B3304_initBoRecord,	/* Local record init routine	    */
   NULL,				/* -- No I/O event support	    */
   (DEVSUPFUN) devCamac_boWrite		/* Use generic write routine	    */
};


struct bodset devCamacB3304Lbo = {   /* Binary Output Records (latched)	    */
   5,					/* 5 entries			    */
   NULL,				/* -- No report routine		    */
   NULL,				/* -- No device support init	    */
   (DEVSUPFUN) B3304L_initBoRecord,	/* Local record init routine	    */
   NULL,				/* -- No I/O event support	    */
   (DEVSUPFUN) devCamac_boWrite		/* Use generic write routine	    */
};


struct mbbodset devCamacB3304mbbo = {/* Multi-Bit Binary Output (momentary) */
   5,					/* 5 entries			    */
   NULL,				/* -- No report routine		    */
   NULL,				/* -- No device support init	    */
   (DEVSUPFUN) B3304_initMbboRecord,	/* Local record init routine	    */
   NULL,				/* -- No I/O event support	    */
   (DEVSUPFUN) devCamac_mbboWrite	/* Use generic write routine	    */
};


struct mbbodset devCamacB3304Lmbbo ={/* Multi-Bit Binary Output (latched)   */
   5,					/* 5 entries			    */
   NULL,				/* -- No report routine		    */
   NULL,				/* -- No device support init	    */
   (DEVSUPFUN) B3304L_initMbboRecord,	/* Local record init routine	    */
   NULL,				/* -- No I/O event support	    */
   (DEVSUPFUN) devCamac_mbboWrite	/* Use generic write routine	    */
};


struct longoutdset devCamacB3304lo ={/* Longword Output Records (momentary) */
   5,					/* 5 entries			    */
   NULL,				/* -- No report routine		    */
   NULL,				/* -- No device support init	    */
   (DEVSUPFUN) B3304_initLoRecord,	/* Local record init routine	    */
   NULL,				/* -- No I/O event support	    */
   (DEVSUPFUN) devCamac_loWrite		/* Use generic write routine	    */
};


struct longoutdset devCamacB3304Llo ={/* Longword Output Records (latched)  */
   5,					/* 5 entries			    */
   NULL,				/* -- No report routine		    */
   NULL,				/* -- No device support init	    */
   (DEVSUPFUN) B3304L_initLoRecord,	/* Local record init routine	    */
   NULL,				/* -- No I/O event support	    */
   (DEVSUPFUN) B3304L_loWrite		/* Local write routine		    */
};

/************************************************************************/
/* B3304_initBoRecord () -- Binary Output Record Initialization Routine	*/
/*                          (momentary variety)				*/
/*	o Declare card type to the CAMAC driver				*/
/*	o Call the standard bo record initialization routine with our	*/
/*	  own function, subaddress, and readback specifications.	*/
/*									*/
/************************************************************************/

LOCAL STATUS B3304_initBoRecord (struct boRecord *prec)
{
   B3304_registerCard (&(prec->out.value.camacio));
   prec->dpvt = &B3304_parms;
   return devCamac_boInitRecord (prec);

}/*end B3304_initBoRecord()*/


/************************************************************************/
/* B3304L_initBoRecord () -- Binary Output Record Initialization Routine*/
/*                          (latched variety)				*/
/*	o Declare card type to the CAMAC driver				*/
/*	o Call the standard bo record initialization routine with our	*/
/*	  own function, subaddress, and readback specifications.	*/
/*									*/
/************************************************************************/

LOCAL STATUS B3304L_initBoRecord (struct boRecord *prec)
{
   B3304L_registerCard (&(prec->out.value.camacio));
   prec->dpvt = &B3304L_parms;
   return devCamac_boInitRecord (prec);

}/*end B3304L_initBoRecord()*/

/************************************************************************/
/* B3304_initMbboRecord () -- Multi-Bit Binary Output Record Init Rtn	*/
/*                            (momentary variety)			*/
/*	o Declare card type to the CAMAC driver				*/
/*	o Call the standard mbbo record initialization routine with our	*/
/*	  own function, subaddress, and readback specifications.	*/
/*									*/
/************************************************************************/

LOCAL STATUS B3304_initMbboRecord (struct mbboRecord *prec)
{
   B3304_registerCard (&(prec->out.value.camacio));
   prec->dpvt = &B3304_parms;
   return devCamac_mbboInitRecord (prec);

}/*end B3304_initMbboRecord()*/


/************************************************************************/
/* B3304L_initMbboRecord () -- Multi-Bit Binary Output Record Init Rtn	*/
/*                             (latched variety)			*/
/*	o Declare card type to the CAMAC driver				*/
/*	o Call the standard mbbo record initialization routine with our	*/
/*	  own function, subaddress, and readback specifications.	*/
/*									*/
/************************************************************************/

LOCAL STATUS B3304L_initMbboRecord (struct mbboRecord *prec)
{
   B3304L_registerCard (&(prec->out.value.camacio));
   prec->dpvt = &B3304L_parms;
   return devCamac_mbboInitRecord (prec);

}/*end B3304L_initMbboRecord()*/

/************************************************************************/
/* B3304_initLoRecord () -- Longword Output Record Init Routine		*/
/*                          (momentary variety)				*/
/*									*/
/************************************************************************/

LOCAL STATUS  B3304_initLoRecord (struct longoutRecord *prec)
{
   B3304_registerCard (&(prec->out.value.camacio));
   return B3304_commonLoInit (prec, NO_XPARM);

}/*end B3304_initLoRecord()*/


/************************************************************************/
/* B3304L_initLoRecord () -- Longword Output Record Init Routine	*/
/*                           (latched variety)				*/
/*									*/
/************************************************************************/

LOCAL STATUS  B3304L_initLoRecord (struct longoutRecord *prec)
{
   B3304L_registerCard (&(prec->out.value.camacio));
   return B3304_commonLoInit (prec, F(0));

}/*end B3304L_initLoRecord()*/

/************************************************************************/
/* B3304_commonLoInit () -- Common Longword Output Record Init Routine	*/
/*	o Use HOPR and LOPR fields to determine setting for range mask	*/
/*	  and bipolar flag.						*/
/*	o Call the standard longout record initialization routine with	*/
/*	  our own function, subaddress, mask, and readback values.	*/
/*									*/
/************************************************************************/

LOCAL STATUS B3304_commonLoInit (struct longoutRecord *prec, short int readback)
{
  /*---------------------
   * Allocate and initialize the CAMAC parameter block
   */
   devCamacParm  B3304_parms = {
      F(18),				/* Function code (selective set)   */
      A(0),				/* Subaddress			   */
      0,				/* Readback (from parm list)	   */
      FALSE,				/* Default bipolar flag		   */
      DEFAULT_MASK			/* Default range mask		   */
   };/*end B3304_parms*/
   B3304_parms.readback = readback;	/* Use value from parameter list   */

  /*---------------------
   * If HOPR and LOPR have been set in the record, use these fields to compute
   * the range mask and bipolar flag.
   */
   if (prec->hopr || prec->lopr) {
      B3304_parms.range_mask = devCamac_getMask (prec->hopr, prec->lopr);
      if ((prec->hopr ^ prec->lopr) < 0) B3304_parms.bipolar = TRUE;
   }/*end if range specified by HOPR & LOPR fields*/

  /*---------------------
   * If HOPR and LOPR are both 0, set them to the default values so that
   * OPI displays will have the correct limits.
   */
   else {
      prec->hopr = DEFAULT_MAX;
      prec->lopr = DEFAULT_MIN;
   }/*end if HOPR and LOPR both zero*/

  /*---------------------
   * Invoke the standard longout record initialization routine with the
   * parameter block we just constructed.
   */
   prec->dpvt = &B3304_parms;
   return devCamac_loInitRecord (prec);
   
}/*end B3304_commonLoInit()*/

/************************************************************************/
/* B3304L_loWrite () -- Latched Longword Write Routine			*/
/*	o Since the BiRa 3304 has no "write the whole register"		*/
/*	  function, a latched longword write is performed by first	*/
/*	  selectively clearing all the bits we wish to be zero and then	*/
/*	  selectively setting all the bits we wish to be one.		*/
/*									*/
/************************************************************************/

LOCAL STATUS  B3304L_loWrite (struct longoutRecord *prec)
{
   short int                  data[2];	/* Data array for csga rtn	*/
   int                        exta[2];	/* CAMAC channel array for csga	*/
   register devInfo_loCamac  *pdevInfo;	/* Ptr to devisce info block	*/
   int                        qa[2];	/* Q response array for csga	*/
   int                        status;	/* Local status variable	*/
   register unsigned int      value;	/* Value to write		*/

  /*---------------------
   * Allocate and initialize the control block and function code arrays.
   */
   int cb[4] = {2, 0, 0, 0};		/* Two operations, no LAM	*/
   static int fa[2] = {F(21), F(18)};	/* Selective clear, then set	*/

  /*---------------------
   * Get the device-dependant information structure and the value to write
   */
   pdevInfo = (devInfo_loCamac *)prec->dpvt;
   value = prec->val;

  /*---------------------
   * Initialize the CAMAC channel and data arrays so that we will first
   * selectively clear all the desired zero bits and then selectively set
   * all the desired one bits.
   */
   exta[0] = exta[1] = pdevInfo->ext;

   data[0] = ~value & pdevInfo->mask;
   data[1] =  value & pdevInfo->mask;

  /*---------------------
   * Perform the write operation and check the status
   */
   csga (fa, exta, data, qa, cb);
   ctstat (&status);

  /*---------------------
   * Set alarm if write error
   */
   if (status != OK) {
      recGblSetSevr (prec, WRITE_ALARM, INVALID_ALARM);
      return WRITE_ALARM;
   }/*end if error in write*/

  /*---------------------
   * Return success status
   */
   return OK;

}/*end B3304L_loWrite()*/

/************************************************************************/
/* B3304_registerCard () -- Register Momentary B3304 Card With Driver	*/
/*									*/
/************************************************************************/

LOCAL void  B3304_registerCard (register struct camacio  *pcamacio)
{
   camacRegisterCard (pcamacio->b, pcamacio->c, pcamacio->n,
                     "B3304  Momentary Binary Output", NULL, 0);
}/*end B3304_registerCard()*/


/************************************************************************/
/* B3304L_registerCard () -- Register Latched B3304 Card With Driver	*/
/*									*/
/************************************************************************/

LOCAL void  B3304L_registerCard (register struct camacio  *pcamacio)
{
   camacRegisterCard (pcamacio->b, pcamacio->c, pcamacio->n,
                     "B3304L Latched Binary Output", NULL, 0);
}/*end B3304L_registerCard()*/
