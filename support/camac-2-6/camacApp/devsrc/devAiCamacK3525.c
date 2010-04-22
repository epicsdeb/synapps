/******************************************************************************
 * devAiCamacK3525.c -- Device Support Routines for Kinetics Systems 3525
 *                      Temperature Monitor
 *
 *----------------------------------------------------------------------------
 * Author:	Rozelle Wright 
 * Date:	24-Oct-1994
 *
 *----------------------------------------------------------------------------
 * MODIFICATION LOG:
 *
 * 24-Oct-1994	rmw	First release
 * 08-Jan-1996	Bjo	Changes for EPICS R3.12
 *
 *----------------------------------------------------------------------------
 * MODULE DESCRIPTION:
 *
 * This module provides EPICS CAMAC device support for the Kinetic Systems
 * KS3525 16 channel temperature monitor card.
 *
 *----------------------------------------------------------------------------
 * CARD DESCRIPTION:  
 *
 * The KS3525 is a 16-channel temperature monitor card which provides the
 * hardware and software necessary for converting thermocouple inputs to
 * temperatures in degrees Farenheit or degrees Centegrade.
 *
 * Data is read from the module with an F(0) A(i) command, where "i" is the
 * channel number to read.  Data is returned as a 15-bit sign-magnitude value
 * where the high order bit is the sign (0 = positive, 1 = negative) and the
 * low order 14 bits represent the temperature in tenths of a degree.
 * Conversion time is approximately 175 milliseconds per channel.  If all
 * 16 channels are scanned (default strap setting) then the update time will
 * be 2.8 seconds.
 *
 * The card contains limit registers with provision to generate a LAM when the
 * a limit is exceeded.  This capability is not used by this device support
 * module.
 * 
 *----------------------------------------------------------------------------
 * RECORD FIELDS OF INTEREST:
 *
 * The following fields should be set by database configuration:
 *	Type	= ai
 *	DTYP	= Camac K3525
 *	INP	= B -> Branch number of K3525 card
 *		  C -> Crate number of K3525 card
 *		  N -> Slot number of K3525 card
 *		  PARM -> Channel number (0-15)
 *	LINR	= LINEAR
 *	EGUF	= Engineering value corresponding to 100 degrees
 *	EGUL	= Engineering value corresponding to 0 degrees
 *	PREC	= 1
 *         
 *****************************************************************************/

/************************************************************************/
/*  Header Files							*/
/************************************************************************/

#include	<stdlib.h>	/* Standard C Routines			*/
#include	<alarm.h>	/* EPICS Alarm Code Definitions		*/
#include	<dbDefs.h>	/* EPICS Standard Definitions		*/
#include	<devSup.h>	/* EPICS Device Support Definitions	*/
#include	<recSup.h>	/* EPICS Record Support Definitions	*/
#include 	<camacLib.h>	/* ESONE CAMAC Library Routines		*/
#include	<devCamac.h>	/* Standard EPICS CAMAC Device Support	*/
#include	<dset.h>	/* Standard DSET definitions		*/

/************************************************************************/
/*  Local Constants and Structures					*/
/************************************************************************/

/*=====================
 * CAMAC Function codes
 */
#define	RD1		0		/* Read group 1 register	*/

/*=====================
 * Constants pertaining to the K3525 Card
 */
#define	K3525_DMASK	0x3ff		/* Data mask (14 bits) 		*/
#define	K3525_SIGN_BIT	0x4000		/* Sign bit mask		*/

/*=====================
 * Parameter Structure for K3525 Card
 */
static devCamacParm k3525_parms = {
   0,					/* Read function code (F0)	*/
   0,					/* Subaddress (from parm field)	*/
   0,0,					/* Unused			*/
   1000					/* Range (for conversions)	*/
};/*end k3525_parms*/
  

/************************************************************************/
/*  Local Device Support Routine Declarations				*/
/************************************************************************/

LOCAL STATUS   k3525_initRecord  (struct aiRecord*);
LOCAL STATUS   k3525_read  (struct aiRecord*);


/************************************************************************/
/*  Device Support Entry Table						*/
/************************************************************************/

struct aidset devCamacK3525 = {
    6,					/* 6 entries			*/
    NULL,				/* --- No report routine	*/
    NULL,				/* --- No Dev Support init	*/
   (DEVSUPFUN) k3525_initRecord,	/* Local record init routine	*/
    NULL,				/* --- No I/O event		*/
   (DEVSUPFUN) k3525_read,		/* Local read routine		*/
   (DEVSUPFUN) devCamac_aiLinConv	/* Use generic linear conversion*/
};

/************************************************************************/
/* k3525_initRecord () -- Record Initialization Routine			*/
/*									*/
/************************************************************************/

LOCAL STATUS k3525_initRecord (struct aiRecord *prec)
{
   struct camacio   *pcamacio;	/* Pointer to camacio part of INP field	*/

  /*---------------------
   * Get parameter from parm field and put it in subaddress field (a) of
   * the parameter structure.
   */
   pcamacio = &(prec->inp.value.camacio);
   k3525_parms.a = atoi (pcamacio->parm);

  /*---------------------
   * Register card with CAMAC Driver
   */
   camacRegisterCard (pcamacio->b, pcamacio->c, pcamacio->n,
                     "K3525  Temperature Input", NULL, 0);

  /*---------------------
   * Invoke the standard AI record initialization routine and pass it our
   * own parameters
   */
   prec->dpvt = &k3525_parms;
   return devCamac_aiInitRecord (prec); 

}/*end k3525_initRecord()*/

/************************************************************************/
/* k3525_read () -- Read K3525 channel					*/
/*	o Read raw data from specified K3525 channel			*/
/*	o Convert from sign-magnitude to twos-complement format.	*/
/*									*/
/************************************************************************/

LOCAL STATUS k3525_read (struct aiRecord *prec)
{
   register devInfo_aiCamac  *pdevInfo;	/* Ptr to device dependant struct  */
   short                      data;	/* Raw data value		   */
   int                        q;	/* Q reply			   */

  /*---------------------
   * Read the raw data word
   */
   pdevInfo = (devInfo_aiCamac *)prec->dpvt;
   cssa (RD1, pdevInfo->ext, &data, &q);

  /*---------------------
   * If read succeeded, convert from sign-magnitude to two's complement
   */
   if (q) {
      prec->rval = (data & K3525_DMASK) * ((data & K3525_SIGN_BIT) ? -1 : 1);
      return OK;
   }/*end if read succeeded*/

  /*---------------------
   * Raise an alarm condition if the read failed
   */   
   recGblSetSevr (prec, READ_ALARM, INVALID_ALARM);
   return READ_ALARM;

}/*end k3525_read()*/
