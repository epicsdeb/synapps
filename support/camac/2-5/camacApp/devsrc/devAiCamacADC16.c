/******************************************************************************
 * devAoCamacADC16.c -- Device Support Routines for Joerger ADC-16
 *                      Analog to Digital Converter
 *
 *----------------------------------------------------------------------------
 * Author:	Eric Bjorklund
 * Date:	18-Sep-1995
 *
 *----------------------------------------------------------------------------
 * MODIFICATION LOG:
 *
 * 18-Sep-1995	Bjo	First release
 * 08-Jan-1996	Bjo	Changes for EPICS R3.12
 *
 *----------------------------------------------------------------------------
 * MODULE DESCRIPTION:
 *
 * This module provides EPICS CAMAC device support for the Joerger ADC-16
 * Analog to Digital Converter card.
 *
 *----------------------------------------------------------------------------
 * CARD DESCRIPTION:
 *
 * The ADC-16 is a 16-channel 12-bit analog to digital converter.  Its range is
 * strap selectable for either 0-10 volts, 0-5 volts, +/-10 volts, +/-5 volts,
 * or +/-2.5 volts.  Data is read from the card with an F(0) A(i) command.
 *
 * This device support module only supports the ADC-16 strapped for bipolar
 * operation.
 *
 *----------------------------------------------------------------------------
 * RECORD FIELDS OF INTEREST:
 *
 * The following fields should be set by database configuration:
 *	Type	= ai
 *	DTYP	= Camac ADC16
 *	INP	= B -> Branch number of ADC-16 card
 *		  C -> Crate number of ADC-16 card
 *		  N -> Slot number of ADC-16 card
 *		  PARM -> Channel number (0-15)
 *
 *	Note that the A and F specifications in the INP field are not used and
 *	may be omitted.
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
#include	<recSup.h>	/* EPICS Record Support Definitions	*/
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
 * Constants pertaining to the ADC16 Module
 */
#define	MAX_CHANNEL	15	/* Maximum channel number		*/

/*=====================
 * Parameter Structure for ADC-16 card
 */
static devCamacParm  ADC16_parms = {
   F(0),			/* Read function code (F0)		*/
   0,				/* Subaddress (from parameter field)	*/
   0,				/* Unused				*/
   TRUE,			/* Bipolar device			*/
   0xfff			/* 12 bits of precision			*/
};/*end ADC16_parms*/


/************************************************************************/
/*  Local Device Support Routine Declarations				*/
/************************************************************************/

LOCAL STATUS   ADC16_initRecord  (struct aiRecord*);


/************************************************************************/
/*  Device Support Entry Tables						*/
/************************************************************************/

struct aidset devCamacADC16 = {
   6,				/* 6 entries				*/
   NULL,			/* --- No report routine		*/
   NULL,			/* --- No device support init		*/
  (DEVSUPFUN) ADC16_initRecord,	/* Local record init routine		*/
   NULL,			/* --- No I/O event support		*/
  (DEVSUPFUN) devCamac_aiRead,	/* Use generic read routine		*/
  (DEVSUPFUN) devCamac_aiLinConv/* Use generic linear conversion	*/
};

/************************************************************************/
/* ADC16_initRecord () -- Record Initialization Routine for ADC-16	*/
/*									*/
/************************************************************************/

LOCAL STATUS ADC16_initRecord (struct aiRecord *prec)
{
   struct camacio   *pcamacio;	/* Ptr to camacio part of INP field	*/

  /*---------------------
   * Get the channel number from the PARM field and put it in the subaddress
   * field (a) of the parameter structure. Abort if channel out of range.
   */
   pcamacio = &(prec->inp.value.camacio);
   ADC16_parms.a = atoi(pcamacio->parm);
   if ((unsigned int)ADC16_parms.a > MAX_CHANNEL) {
      devCamac_recDisable ((void *) prec);
      recGblRecordError (S_db_badField, (void *)prec,
                         "ADC16_initRecord:\n-- Bad channel");
      return INIT_FAILED;
   }/*end if invalid channel number*/

  /*---------------------
   * Register card location with CAMAC driver
   */
   camacRegisterCard (pcamacio->b, pcamacio->c, pcamacio->n,
                      "ADC-16 12-bit ADC", NULL, 0);

  /*---------------------
   * Invoke the standard ai record initialization routine and pass it our
   * own parameters.
   */
   prec->dpvt = &ADC16_parms;
   return devCamac_aiInitRecord (prec);

}/*end ADC16_initRecord()*/
