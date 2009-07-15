/******************************************************************************
 * devAiCamacK3553.c -- Device Support Routines for Kinetic Systems 3553
 *                      Triggered Analog to Digital Converter
 *
 *----------------------------------------------------------------------------
 * Author:	Eric Bjorklund
 * Date:	05-Oct-1995
 *
 *----------------------------------------------------------------------------
 * MODIFICATION LOG:
 *
 * 05-Oct-1995	Bjo	First release
 * 08-Jan-1996	Bjo	Changes for EPICS R3.12
 *
 *----------------------------------------------------------------------------
 * MODULE DESCRIPTION:
 *
 * This module provides EPICS CAMAC device support for the Kinetic Systems
 * 3553 Triggered Analog to Digital Converter card.
 *
 *----------------------------------------------------------------------------
 * CARD DESCRIPTION:
 *
 * The K3553 is a single-channel 12-bit analog to digital converter.  It
 * range is strap selectable for either 0-10 volts, 0-5 volts, +/-10 volts,
 * +/-5 volts, or +/-2.5 volts.  Data is read from the card with an F(0) A(0)
 * command.
 *
 * This device support module only supports the K3553 strapped for bipolar
 * operation.
 *
 *----------------------------------------------------------------------------
 * RECORD FIELDS OF INTEREST:
 *
 * The following fields should be set by database configuration:
 *	Type	= ai
 *	DTYP	= Camac K3553
 *	INP	= B -> Branch number of K3553 card
 *		  C -> Crate number of K3553 card
 *		  N -> Slot number of K3553 card
 *
 *	Note that the A, F, and PARM specifications in the INP field are not
 *	used and may be omitted.
 *
 * Other fields of interest:
 *	EGUF	= Eng value for plus full scale (10, 5, or 2.5 Volts)
 *	EGUL	= Eng value for minus full scale (-10, -5, or -2.5 Volts)
 *
 *****************************************************************************/

/************************************************************************/
/*  Header Files							*/
/************************************************************************/

#include	<stdlib.h>	/* Standard C Routines			*/
#include	<dbDefs.h>	/* EPICS Standard Definitions		*/
#include	<dbAccess.h>	/* EPICS database routines and codes	*/
#include	<devSup.h>	/* EPICS Device Support Definitions	*/
#include	<devCamac.h>	/* Standard EPICS CAMAC Device Support	*/
#include	<camacLib.h>	/* ESONE CAMAC Routines			*/
#include	<dset.h>	/* Standard DSET definitions		*/


/************************************************************************/
/*  Local Constants and Structures					*/
/************************************************************************/

/*=====================
 * Conversion constants returned by device support
 */
#define	INIT_OK		0	/* Successful record initialization	*/
#define	INIT_FAILED	2	/* Initialization failed		*/


/*=====================
 * Parameter Structure for K3553 card
 */
static devCamacParm  K3553_parms = {
   F(0),			/* Read function code (F0)		*/
   A(0),			/* Subaddress A(0)			*/
   0,				/* Unused				*/
   TRUE,			/* Bipolar device			*/
   0xfff			/* 12 bits of precision			*/
};/*end K3553_parms*/


/************************************************************************/
/*  Local Device Support Routine Declarations				*/
/************************************************************************/

LOCAL STATUS   K3553_initRecord  (struct aiRecord*);


/************************************************************************/
/*  Device Support Entry Tables						*/
/************************************************************************/

struct aidset devCamacK3553 = {
   6,				/* 6 entries				*/
   NULL,			/* --- No report routine		*/
   NULL,			/* --- No device support init		*/
  (DEVSUPFUN) K3553_initRecord,	/* Local record init routine		*/
   NULL,			/* --- No I/O event support		*/
  (DEVSUPFUN) devCamac_aiRead,	/* Use generic read routine		*/
  (DEVSUPFUN) devCamac_aiLinConv/* Use generic linear conversion	*/
};

/************************************************************************/
/* K3553_initRecord () -- Record Initialization Routine for K3553	*/
/*									*/
/************************************************************************/

LOCAL STATUS K3553_initRecord (struct aiRecord *prec)
{
   struct camacio   *pcamacio;	/* Ptr to camacio part of INP field	*/

  /*---------------------
   * Register card location with CAMAC driver
   */
   pcamacio = &(prec->inp.value.camacio);
   camacRegisterCard (pcamacio->b, pcamacio->c, pcamacio->n,
                      "K3553  Triggered ADC", NULL, 0);

  /*---------------------
   * Invoke the standard ai record initialization routine and pass it our
   * own parameters.
   */
   prec->dpvt = &K3553_parms;
   return devCamac_aiInitRecord (prec);

}/*end K3553_initRecord()*/
