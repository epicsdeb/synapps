/******************************************************************************
 * devAoCamacSE258.c -- Device Support Routines for Standard Engineering E258
 *                      Digital to Analog Converter
 *
 *----------------------------------------------------------------------------
 * Author:	Rozelle Wright
 * Date:	07-Nov-1994
 *
 *----------------------------------------------------------------------------
 * MODIFICATION LOG:
 *
 * 07-Nov-1994	rmw	First release
 * 09-Jan-1995	Bjo	Changes for EPICS R3.12
 *
 *----------------------------------------------------------------------------
 * MODULE DESCRIPTION:
 *
 * This module provides EPICS CAMAC device support for the Standard Engineering
 * E258 Digital to Analog Converter card.
 *
 *----------------------------------------------------------------------------
 * CARD DESCRIPTION:
 *
 * The E258 is an 8-channel 16-bit digital to analog converter.  Its range is
 * 0 to 10 volts (unipolar).  Data is written to the card with an F(16) A(i)
 * command.  The E258 has no readback function.
 *
 *----------------------------------------------------------------------------
 * RECORD FIELDS OF INTEREST:
 *
 * The following fields should be set by database configuration:
 *	Type	= ao
 *	DTYP	= Camac SE258
 *	OUT	= B -> Branch number of SE258 card
 *		  C -> Crate number of SE258 card
 *		  N -> Slot number of SE258 card
 *		  PARM -> Channel number (0-7)
 *
 *	Note that the A and F specifications in the OUT field are not used and
 *	may be omitted.
 *
 * Other fields of interest:
 *	EGUF	= Engineering value for 10 Volts
 *	EGUL	= Engineering value for 0 Volts 
 *	DRVHI	= Upper drive limit.  Should be <= EGUF
 *	DRVLO	= Lower drive limit.  Should be >= EGUL
 *
 *	Note that if DRVHI == DRVLO no output will be sent to the device.
 *	In particular, if DRVHI == 0 == DRVLO, (which is the default) no
 *	output will be sent to the device.
 *           
 *
 *****************************************************************************/

/************************************************************************/
/*  Header Files							*/
/************************************************************************/

#include	<stdlib.h>	/* Standard C Routines			*/
#include	<dbDefs.h>	/* EPICS Standard Definitions		*/
#include	<dbAccess.h>	/* EPICS database routines and codes	*/
#include	<recSup.h>	/* EPICS Record Support Definitions	*/
#include	<devSup.h>	/* EPICS Device Support Definitions	*/
#include	<camacLib.h>	/* EPICS ESONE CAMAC Library		*/
#include	<devCamac.h>	/* Standard EPICS CAMAC Device Support	*/
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
 * Constants pertaining to the SE258 Module
 */
#define	MAX_CHANNEL	7	/* Maximum channel number		*/

/*=====================
 * Parameter Structure for SE258 card
 */
static devCamacParm  se258_parms = {
   16,				/* Write function code (F16)		*/
   0,				/* Subaddress (from parameter field)	*/
   NO_XPARM,			/* No readback function code		*/
   FALSE,			/* Unipolar device			*/
   0xffff			/* 16 bits of precision			*/
};/*end se258_parms*/


/************************************************************************/
/*  Local Device Support Routine Declarations				*/
/************************************************************************/

LOCAL STATUS   se258_initRecord  (struct aoRecord*);


/************************************************************************/
/*  Device Support Entry Tables						*/
/************************************************************************/

struct aodset devCamacSE258 = {
   6,				/* 6 entries				*/
   NULL,			/* --- No report routine		*/
   NULL,			/* --- No device support init		*/
  (DEVSUPFUN) se258_initRecord,	/* Local record init routine		*/
   NULL,			/* --- No I/O event support		*/
  (DEVSUPFUN) devCamac_aoWrite,	/* Use generic write routine		*/
  (DEVSUPFUN) devCamac_aoLinConv/* Use generic linear conversion	*/
};

/************************************************************************/
/* se258_initRecord () -- Record Initialization Routine for SE258	*/
/*									*/
/************************************************************************/

LOCAL STATUS se258_initRecord (struct aoRecord *prec)
{
   struct camacio   *pcamacio;	/* Ptr to camacio part of OUT field	*/

  /*---------------------
   * Get the channel number from the PARM field and put it in the subaddress
   * field (a) of the parameter structure. Abort if channel out of range.
   */
   pcamacio = &(prec->out.value.camacio);
   se258_parms.a = atoi(pcamacio->parm);
   if ((unsigned int)se258_parms.a > MAX_CHANNEL) {
      devCamac_recDisable ((void *) prec);
      recGblRecordError (S_db_badField, (void *)prec,
                         "se258_initRecord:\n-- Bad channel");
      return INIT_FAILED;
   }/*end if invalid channel number*/

  /*---------------------
   * Register the card with the CAMAC Driver
   */
   camacRegisterCard (pcamacio->b, pcamacio->c, pcamacio->n,
                     "SE258  16-bit DAC", NULL, 0);

  /*---------------------
   * Invoke the standard AO record initialization routine and pass it our
   * own parameters.
   */
   prec->dpvt = &se258_parms;
   return devCamac_aoInitRecord (prec);

}/*end se258_initRecord()*/
