/******************************************************************************
 * devCamacK3470.c -- Device Support Routines for K3470 Binary Input Module
 *
 *----------------------------------------------------------------------------
 * Author:	Eric Bjorklund
 * Date:	27-Jan-1995
 *
 *----------------------------------------------------------------------------
 * MODIFICATION LOG:
 *
 * 27-Jan-1995	Bjo	Original release
 * 05-Sep-1995	Bjo	Added support for mbbi and longin records
 * 09-Jan-1996	Bjo	Changes for EPICS R3.12
 *
 *----------------------------------------------------------------------------
 * MODULE DESCRIPTION:
 *
 * This module provides EPICS CAMAC device support for the Kinetic Systems 3470
 * binary input card.  Support is provided for "binary input" (bi), "multi-
 * bit binary input" (mbbi), and "longword input" (longin) records.
 *
 * For longin records, the HOPR and LOPR fields may be used to control the
 * range and bipolar properties of the record.
 * o If HOPR and LOPR are both zero, the default range (24-bits) and polarity
 *   (unipolar) will be used.  Record initialization will set the values of
 *   HOPR and LOPR to the defaults so that OPI displays will display correct
 *   limits (e.g. for bar charts, etc.)
 * o If either HOPR or LOPR (or both) have non-zero values, these values will
 *   be used to compute the range mask.  For example, setting HOPR=255 and
 *   LOPR=0, will result in a longin record with an 8-bit (unipolar) range
 *   mask.  Higher order bits will be masked off.  If LOPR is a negative number
 *   and HOPR is positive or zero, the record will be considered "bipolar" and
 *   the high-order bit will be "sign extended" to make up 32-bits.  For
 *   example, setting HOPR=255 and LOPR=-256 (or -255), will result in a
 *   bipolar longin record with a 9-bit range mask.
 *
 *----------------------------------------------------------------------------
 * CARD DESCRIPTION:
 *
 * The Kinetic Systems 3470 is a 24-bit binary input card.  The card can
 * generate a LAM from up to 6 strobe inputs.  The LAM capability, however,
 * is not exploited by this device support module.
 *  
 *----------------------------------------------------------------------------
 * RECORD FIELDS OF INTEREST:
 *
 * The following fields should be set by database configuration:
 *	Type	= bi, mbbi, or longin
 *	DTYP	= Camac K3470
 *	INP	= B -> Branch number of K3470 card
 *		  C -> Crate number of K3470 card
 *		  N -> Slot number of K3470 card
 *        	  A -> Ignored
 *                F -> Ignored
 *		  PARM -> starting bit number 0-23 (bi and mbbi records only)
 *
 *	HOPR	= (longin records only) High operating range value.  If
 *		  set, will override default range.
 *	LOPR	= (longin records only) Low operating range value.  If
 *		  set, will override default range.
 *	NOBT	= (mbbi records only) Number of bits in the value.
 *
 *****************************************************************************/

/************************************************************************/
/*  Header Files							*/
/************************************************************************/

#include	<dbDefs.h>	/* EPICS Standard Definitions		*/
#include	<devSup.h>	/* EPICS Device Support Definitions	*/
#include	<devCamac.h>	/* Standard CAMAC Device Support	*/
#include	<camacLib.h>	/* ESONE CAMAC Library Definitions	*/
#include        <dset.h>        /* Standard DSET definitions		*/


/************************************************************************/
/*  Local Constants							*/
/************************************************************************/

#define DEFAULT_MASK	0xffffff	/* Default range mask value	*/
#define	DEFAULT_MAX	0xffffff	/* Default upper range value	*/
#define	DEFAULT_MIN	0		/* Default lower range value	*/


/************************************************************************/
/*  Function Prototypes							*/
/************************************************************************/

LOCAL void     K3470_registerCard   (register struct camacio*);
LOCAL STATUS   K3470_initBiRecord   (struct biRecord*);
LOCAL STATUS   K3470_initMbbiRecord (struct mbbiRecord*);
LOCAL STATUS   K3470_initLiRecord   (struct longinRecord*);


/************************************************************************/
/*  Parameter Structure for K3470 bi and mbbi records			*/
/************************************************************************/

static devCamacParm  K3470_parms = {
   F(0),				/* Function code for read	*/
   A(1),				/* Subaddress for read		*/
   0, 0, 0};				/* Unused fields		*/

/************************************************************************/
/*  Device Support Entry Tables for K3470 Records			*/
/************************************************************************/

struct bidset devCamacK3470bi = {    /* Binary Input Records		*/
   5,					/* 5 entries			*/
   NULL,				/* -- No report routine		*/
   NULL,				/* -- No device support init	*/
   (DEVSUPFUN) K3470_initBiRecord,	/* Local record init routine	*/
   NULL,				/* -- No I/O event support	*/
   (DEVSUPFUN) devCamac_biRead		/* Use generic read routine	*/
};


struct mbbidset devCamacK3470mbbi = {/* Multi-Bit Binary Input Records	*/
   5,					/* 5 entries			*/
   NULL,				/* -- No report routine		*/
   NULL,				/* -- No device support init	*/
   (DEVSUPFUN) K3470_initMbbiRecord,	/* Local record init routine	*/
   NULL,				/* -- No I/O event support	*/
   (DEVSUPFUN) devCamac_mbbiRead	/* Use generic read routine	*/
};


struct longindset devCamacK3470li = {/* Longword Input Records		*/
   5,					/* 5 entries			*/
   NULL,				/* -- No report routine		*/
   NULL,				/* -- No device support init	*/
   (DEVSUPFUN) K3470_initLiRecord,	/* Local record init routine	*/
   NULL,				/* -- No I/O event support	*/
   (DEVSUPFUN) devCamac_liRead		/* Use generic read routine	*/
};

/************************************************************************/
/* K3470_initBiRecord () -- Binary Input Record Initialization Routine 	*/
/*	o Declare card type to the CAMAC driver				*/
/*	o Call the standard bi record initialization routine with our	*/
/*	  own function and subaddress specifications.			*/
/*									*/
/************************************************************************/

LOCAL STATUS K3470_initBiRecord (struct biRecord *prec)
{
   K3470_registerCard (&(prec->inp.value.camacio));
   prec->dpvt = &K3470_parms;
   return devCamac_biInitRecord (prec);
   
}/*end K3470_initBiRecord()*/


/************************************************************************/
/* K3470_initMbbiRecord () -- Multi-Bit Binary Input Record Init Rtn. 	*/
/*	o Declare card type to the CAMAC driver				*/
/*	o Call the standard mbbi record initialization routine with our	*/
/*	  own function and subaddress specifications.			*/
/*									*/
/************************************************************************/

LOCAL STATUS K3470_initMbbiRecord (struct mbbiRecord *prec)
{
   K3470_registerCard (&(prec->inp.value.camacio));
   prec->dpvt = &K3470_parms;
   return devCamac_mbbiInitRecord (prec);
   
}/*end K3470_initMbbiRecord()*/

/************************************************************************/
/* K3470_initLiRecord () -- Longword Input Record Initialization Routine*/
/*	o Use HOPR and LOPR fields to determine setting for range mask	*/
/*	  and bipolar flag.						*/
/*	o Declare card type to the CAMAC driver				*/
/*	o Call the standard longin record initialization routine with	*/
/*	  our own function and subaddress specifications.		*/
/*									*/
/************************************************************************/

LOCAL STATUS K3470_initLiRecord (struct longinRecord *prec)
{
  /*---------------------
   * Allocate and initialize the CAMAC parameter block
   */
   devCamacParm  K3470_parms = {
      F(0),		/* Function code for read			*/
      A(1),		/* Subaddress for read				*/
      0,		/* Readback (not used for longin records)	*/
      FALSE,		/* Default bipolar flag				*/
      DEFAULT_MASK	/* Default range mask				*/
   };/*end K3470_parms*/

  /*---------------------
   * If HOPR and LOPR have been set in the record, use these fields to compute
   * the range mask and bipolar flag.
   */
   if (prec->hopr || prec->lopr) {
      K3470_parms.range_mask = devCamac_getMask (prec->hopr, prec->lopr);
      if ((prec->hopr ^ prec->lopr) < 0) K3470_parms.bipolar = TRUE;
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
   * Register the card type, then invoke the standard longin record
   * initialization routine with the parameter block we just constructed.
   */
   K3470_registerCard (&(prec->inp.value.camacio));
   prec->dpvt = &K3470_parms;
   return devCamac_liInitRecord (prec);
   
}/*end K3470_initLiRecord()*/

/************************************************************************/
/* K3470_registerCard () -- Register Card Type With CAMAC Driver	*/
/*									*/
/************************************************************************/

LOCAL void K3470_registerCard (register struct camacio  *pcamacio)
{
   camacRegisterCard (pcamacio->b, pcamacio->c, pcamacio->n,
                     "K3470  Binary Input", NULL, 0);
}/*end K3470_registerCard()*/
