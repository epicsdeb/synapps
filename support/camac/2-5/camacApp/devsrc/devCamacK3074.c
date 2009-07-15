/******************************************************************************
 * devCamacK3074.c -- Device Support Routines for K3074 Binary Input Module
 *
 *----------------------------------------------------------------------------
 * Author:	Eric Bjorklund
 * Date:	14-Feb-1995
 *
 *----------------------------------------------------------------------------
 * MODIFICATION LOG:
 *
 * 14-Feb-1995	Bjo	Original release
 * 13-Jul-1995	Bjo	Add support for multi-bit binary output
 * 09-Jan-1996	Bjo	Changes for EPICS R3.12
 *
 *----------------------------------------------------------------------------
 * MODULE DESCRIPTION:
 *
 * This module provides EPICS CAMAC device support for the Kinetic Systems 3074
 * binary output card.
 *
 *----------------------------------------------------------------------------
 * CARD DESCRIPTION:
 *
 * The Kinetic Systems 3074 is a 24-bit binary output card with readback.
 * The card is commanded through F(16) A(0) and read through F(0) A(0).
 *  
 *----------------------------------------------------------------------------
 * RECORD FIELDS OF INTEREST:
 *
 * The following fields should be set by database configuration:
 *	Type	= bo or mbbo
 *	DTYP	= Camac K3074
 *	INP	= B -> Branch number of K3074 card
 *		  C -> Crate number of K3074 card
 *		  N -> Slot number of K3074 card
 *        	  A -> Ignored
 *                F -> Ignored
 *		  PARM -> bit number 0-23 of low-order bit
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
/*  Function Prototypes							*/
/************************************************************************/

LOCAL STATUS   K3074_initBoRecord   (struct boRecord*);
LOCAL STATUS   K3074_initMbboRecord (struct mbboRecord*);


/************************************************************************/
/*  Device Support Entry Tables for K3074				*/
/************************************************************************/

struct bodset devCamacK3074bo = {
   5,					/* 5 entries			*/
   NULL,				/* -- No report routine		*/
   NULL,				/* -- No device support init	*/
   (DEVSUPFUN) K3074_initBoRecord,	/* Local record init routine	*/
   NULL,				/* -- No I/O event support	*/
   (DEVSUPFUN) devCamac_boWrite		/* Use generic write routine	*/
};

struct mbbodset devCamacK3074mbbo = {
   5,					/* 5 entries			*/
   NULL,				/* -- No report routine		*/
   NULL,				/* -- No device support init	*/
   (DEVSUPFUN) K3074_initMbboRecord,	/* Local record init routine	*/
   NULL,				/* -- No I/O event support	*/
   (DEVSUPFUN) devCamac_mbboWrite	/* Use generic write routine	*/
};


/************************************************************************/
/*  Parameter Structure for K3074					*/
/************************************************************************/

static devCamacParm  K3074_parms =
   {F(16), A(0), F(0), 0, 0};	/* Function, subaddress, and readback	*/

/************************************************************************/
/* K3074_initBoRecord () -- Record Initialization Routine for K3074	*/
/*                          Binary Output (bo) Records			*/
/*									*/
/*	o Register the card with the CAMAC driver			*/
/*	o Call the standard bo record initialization routine with our	*/
/*	  own function, subaddress, and readback specifications.	*/
/*									*/
/************************************************************************/

LOCAL STATUS K3074_initBoRecord (struct boRecord *prec)
{
   struct camacio   *pcamacio;  /* Ptr to camacio part of OUT field     */

  /*---------------------
   * Register card location with CAMAC driver
   */
   pcamacio = &(prec->out.value.camacio);
   camacRegisterCard (pcamacio->b, pcamacio->c, pcamacio->n,
                      "K3074  Latched Binary Output", NULL, 0);

  /*---------------------
   * Invoke standard bo record initialization
   */
   prec->dpvt = &K3074_parms;
   return devCamac_boInitRecord (prec);
   
}/*end K3074_initBoRecord()*/

/************************************************************************/
/* K3074_initMbboRecord () -- Record Initialization Routine for K3074	*/
/*                            Multi-Bit Binary Output (mbbo) Records	*/
/*									*/
/*	o Register the card with the CAMAC driver			*/
/*	o Call the standard mbbo record initialization routine with our	*/
/*	  own function, subaddress, and readback specifications.	*/
/*									*/
/************************************************************************/

LOCAL STATUS K3074_initMbboRecord (struct mbboRecord *prec)
{
   struct camacio   *pcamacio;  /* Ptr to camacio part of OUT field     */

  /*---------------------
   * Register card location with CAMAC driver
   */
   pcamacio = &(prec->out.value.camacio);
   camacRegisterCard (pcamacio->b, pcamacio->c, pcamacio->n,
                      "K3074  Latched Binary Output", NULL, 0);

  /*---------------------
   * Invoke standard mbbo record initialization
   */
   prec->dpvt = &K3074_parms;
   return devCamac_mbboInitRecord (prec);
   
}/*end K3074_initMbboRecord()*/
