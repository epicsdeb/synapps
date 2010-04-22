/******************************************************************************
 * devCamacJW213.c -- Device Support for Jorway 213 Binary Input Cards
 *
 *----------------------------------------------------------------------------
 * Author:	Rozelle Wright
 * Date:	10-Nov-1994
 *
 *----------------------------------------------------------------------------
 * MODIFICATION LOG:
 *
 * 10-Nov-1994	rmw	First release -- binary input only lams disabled
 * 13-Oct-1995	Bjo	Add support for multi-bit binary input
 * 07-Nov-1995	Bjo	Register card with CAMAC driver
 * 09-Jan-1996	Bjo	Changes for EPICS R3.12
 *
 *----------------------------------------------------------------------------
 * MODULE DESCRIPTION:
 *
 * This module provides EPICS CAMAC device support for the Jorway 213 binary
 * input card.
 *
 *----------------------------------------------------------------------------
 * CARD DESCRIPTION:
 *
 * The Jorway 213 is a 24-bit binary input card.  It has the ability to compare
 * the current contents with a pattern register and generate a LAM when there
 * is a difference.  LAM on change is not implemented in this device-support
 * module.
 *  
 *----------------------------------------------------------------------------
 * RECORD FIELDS OF INTEREST:
 *
 * The following fields should be set by database configuration:
 *	Type	= bi or mbbi
 *	DTYP	= Camac JW213
 *	OUT	= B -> Branch number of JW213 card
 *		  C -> Crate number of JW213 card
 *		  N -> Slot number of JW213 card
 *        	  A -> Unused
 *                F -> Unused
 *		  PARM -> bit number 0-23
 *
 *****************************************************************************/

/************************************************************************/
/*  Header Files							*/
/************************************************************************/

#include	<dbDefs.h>	/* EPICS Standard Definitions		*/
#include	<devSup.h>	/* EPICS Device Support Definitions	*/
#include	<camacLib.h>	/* EPICS ESONE CAMAC Library		*/
#include	<devCamac.h>	/* Standard CAMAC Device Support	*/
#include	<dset.h>	/* Standard DSET Definitions		*/

/************************************************************************/
/*  Local Constants and Structures					*/
/************************************************************************/

/*---------------------
 * CAMAC Function codes used by this module
 */
#define	RD1		0	/* Read group 1 register	*/
#define	LAM_DISABLE	24	/* Disable lams			*/

/************************************************************************/
/*  Local Device Support Routine Declarations				*/
/************************************************************************/

LOCAL STATUS   JW213_initBiRecord   (struct biRecord*);
LOCAL STATUS   JW213_initMbbiRecord (struct mbbiRecord*);


/************************************************************************/
/*  Device Support Entry Tables						*/
/************************************************************************/

struct bidset devCamacJW213bi={	     /* Binary Input			    */
   5,					/* 5 entries			    */
   NULL,				/* -- No report routine		    */
   NULL,				/* -- No device support init	    */
   (DEVSUPFUN) JW213_initBiRecord,	/* Local record init routine	    */
   NULL,				/* -- No I/O event support	    */
   (DEVSUPFUN) devCamac_biRead		/* Use generic read routine	    */
};

struct mbbidset devCamacJW213mbbi={  /* Multi-Bit Binary Input		    */
   5,					/* 5 entries			    */
   NULL,				/* -- No report routine		    */
   NULL,				/* -- No device support init	    */
   (DEVSUPFUN) JW213_initMbbiRecord,	/* Local record init routine	    */
   NULL,				/* -- No I/O event support	    */
   (DEVSUPFUN) devCamac_mbbiRead	/* Use generic read routine	    */
};


/************************************************************************/
/*  Parameter Structure for JW213					*/
/************************************************************************/

static devCamacParm  JW213_parms = {
   0, 0,			/* Function & subaddr for read F(0) A(0)    */
   0, 0, 0			/* Unused fields			    */
}; /*end JW213_parms*/

/************************************************************************/
/* JW213_initBiRecord () -- Record Initialization Routine for JW213	*/
/*                          Binary Input (bi) Records			*/
/*									*/
/************************************************************************/

LOCAL STATUS JW213_initBiRecord (struct biRecord *prec)
{
   int               data;		/* Dummy data word		*/
   struct camacio   *pcamacio;  	/* Ptr to camacio structure	*/
   devInfo_biCamac  *pdevInfo;		/* Address of device info block	*/
   int               q;			/* Dummy Q return word		*/
   int               status;		/* Local status variable	*/

  /*---------------------
   * Register card location with CAMAC driver
   */
   pcamacio = &(prec->inp.value.camacio);
   camacRegisterCard (pcamacio->b, pcamacio->c, pcamacio->n,
                      "JW213  24-bit binary input", NULL, 0);

  /*---------------------
   * Invoke the standard Bi record initialization routine and pass it our
   * own parameters.
   */
   prec->dpvt = &JW213_parms;
   status = devCamac_biInitRecord (prec);

  /*---------------------
   * Make sure LAMs are disabled on this card
   */
   if (status == OK) {
      pdevInfo = (devInfo_biCamac *)prec->dpvt;
      cfsa(LAM_DISABLE, pdevInfo->ext, &data, &q);
   }/*if initialization succeeded*/

   return status;
   
}/*end JW213_initBiRecord()*/

/************************************************************************/
/* JW213_initMbbiRecord () -- Record Initialization Routine for JW213	*/
/*                            Multi-Bit Binary Input (mbbi) Records	*/
/*									*/
/************************************************************************/

LOCAL STATUS JW213_initMbbiRecord (struct mbbiRecord *prec)
{
   int               data;		/* Dummy data word		*/
   struct camacio   *pcamacio;  	/* Ptr to camacio structure	*/
   devInfo_biCamac  *pdevInfo;		/* Address of device info block	*/
   int               q;			/* Dummy Q return word		*/
   int               status;		/* Local status variable	*/

  /*---------------------
   * Register card location with CAMAC driver
   */
   pcamacio = &(prec->inp.value.camacio);
   camacRegisterCard (pcamacio->b, pcamacio->c, pcamacio->n,
                      "JW213  24-bit binary input", NULL, 0);

  /*---------------------
   * Invoke the standard Mbbi record initialization routine and pass it our
   * own parameters.
   */
   prec->dpvt = &JW213_parms;
   status = devCamac_mbbiInitRecord (prec);

  /*---------------------
   * Make sure LAMs are disabled on this card
   */
   if (status == OK) {
      pdevInfo = (devInfo_mbbiCamac *)prec->dpvt;
      cfsa(LAM_DISABLE, pdevInfo->ext, &data, &q);
   }/*if initialization succeeded*/

   return status;
   
}/*end JW213_initMbbiRecord()*/
