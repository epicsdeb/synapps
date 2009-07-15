/******************************************************************************
 *devAiCamacJ1616.c -- Device Support Routines for Joerger ADC-1616
 *
 *----------------------------------------------------------------------------
 * Author:	Rozelle Wright 
 * Date:	3-Nov-1994
 *
 *----------------------------------------------------------------------------
 * MODIFICATION LOG:
 * 07-Nov-1995	Bjo	Register card with CAMAC Driver
 * 08-Jan-1996	Bjo	Changes for EPICS R3.12
 *
 *----------------------------------------------------------------------------
 * MODULE DESCRIPTION:
 *
 * This module provides EPICS CAMAC device support for 
 * the Joerger Enterprises Model ADC-1616
 *
 *----------------------------------------------------------------------------
 * CARD DESCRIPTION:
 *
 * 
 *----------------------------------------------------------------------------
 * RECORD FIELDS OF INTEREST:
 *
 * The following fields should be set by database configuration:
 *	Type	= AI
 *	DTYP	= Camac J1616
 *	OUT	= B -> Branch number of J1616 card
 *		  C -> Crate number of J1616 card
 *		  N -> Slot number of J1616 card
 *		  A -> ignored
 *		  F -> ignored
 *		  PARM -> 0 -15 reads data out of channel 0 - 15.
 *
 *  	  LINR = LINEAR
 *        EGUF   value in engineering units for highest number of bits
 *        EGUL   value in engineering units for lowestLow value for engineering 
 *
 *****************************************************************************/

/************************************************************************/
/*  Header Files							*/
/************************************************************************/

#include	<sys/types.h>	/* Standard Data Type Definitions   */
#include	<stdlib.h>	/* Standard C Routines		    */
#include	<stdio.h>	/* Standard I/O Routines	    */
#include	<string.h>	/* Standard String Handling Rtns    */
#include	<errno.h>	/* Standard Error Codes		    */

#include	<vxWorks.h>	/* VxWorks Definitions & Routines   */
#include	<logLib.h>	/* VxWorks Message Logging Library  */

#include	<dbDefs.h>	/* EPICS Standard Definitions	    */
#include	<recSup.h>	/* EPICS Record Support Definitions */
#include	<devSup.h>	/* EPICS Device Support Definitions */
#include	<link.h>	/* EPICS db Link Field Definitions  */
#include	<aiRecord.h>	/* EPICS analog input Record	    */

#include 	<camacLib.h>	/* EPICS/ESONE CAMAC Library Rtns   */
#include	<devCamac.h>	/* Standard CAMAC Device Support    */

/************************************************************************/
/*  Local Constants							*/
/************************************************************************/

/*---------------------
 * CAMAC Function codes
 */
#define	RD1			0	/* Read group 1 register	*/

#define J1616_DMASK		0xFFFF	/* 16 bits of data		*/
#define J1616_SIGN_BIT		0x10000	/* Sign bit			*/
#define J1616__OVERRANGE	0x20000	/* Over range bit		*/
#define J1616__SINGLECHANNEL	0x40000	/* Single channel mode bit	*/
#define J1616_NOTCYCLING	0x80000	/* Error not cycling		*/

/************************************************************************/
/*  Local Function Declarations						*/
/************************************************************************/

/*---------------------
 * Device Support Routines
 */
LOCAL STATUS   j1616_initRecord  (struct aiRecord*);
LOCAL STATUS   j1616_read  (struct aiRecord*);


/************************************************************************/
/*  Device Support Entry Table						*/
/************************************************************************/

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_write;
	DEVSUPFUN	special_linconv;
}devCamacJ1616={
	6,
	NULL,				/* -- No report routine		*/
	NULL,				/* -- No Dev Support init	*/
	(DEVSUPFUN) j1616_initRecord,	/* Record init routine		*/
	NULL,				/* -- No I/O event		*/
	(DEVSUPFUN) j1616_read,		/* Standard read routine	*/
	(DEVSUPFUN) devCamac_aiLinConv	/* Standard linear conversion	*/
	};

/************************************************************************/
/*			Device Support Routines				*/
/*									*/


/************************************************************************/
/* j1616_initRecord () -- Record Initialization Routine			*/
/*									*/
/************************************************************************/

LOCAL STATUS j1616_initRecord (struct aiRecord *prec)
{
   int               status;	/* Local status variable		*/
   struct camacio   *pcamacio;	/* Ptr to camacio part of INP field	*/
  
  /*---------------------
   * Set device parameters to send to generic device initialization
   * f=0, a=0, readback=0, bipolar=1, mask=0x1FFFF
   */
   static devCamacParm devInfo = {0,0,0,1, 0x1FFFF};
   
  /*---------------------
   * Extract the channel number and use it for the subaddress.
   */
   pcamacio = &(prec->inp.value.camacio);
   devInfo.a = atoi(pcamacio->parm);

  /*---------------------
   * Register card with the CAMAC driver
   */
   camacRegisterCard (pcamacio->b, pcamacio->c, pcamacio->n,
                     "J1616  16-bit ADC", NULL, 0);

  /*---------------------
   * Invoke standard ai record processing and return the result.
   */
   prec->dpvt = &devInfo;
   status = devCamac_aiInitRecord(prec); 
   return (status);

}/*end j1616_initRecord()*/

/************************************************************************/
/* j1616_read () -- Read Routine for J1616				*/
/*									*/
/************************************************************************/

LOCAL STATUS j1616_read (struct aiRecord *prec)
{
   register devInfo_aiCamac  *pdevInfo;	/* Ptr to device-dependant structure */
   int                        data;	/* Raw value read from card	     */
   int                        q;	/* Q reply			     */

  /*---------------------
   * Read the raw value, sign, and error bits
   */
   pdevInfo = (devInfo_aiCamac *)prec->dpvt;
   cfsa (RD1, pdevInfo->ext, &data, &q);

  /*---------------------
   * Check for errors
   */
   if (!q || (data & (J1616__OVERRANGE | J1616__SINGLECHANNEL | J1616_NOTCYCLING))) {
      recGblSetSevr (prec, READ_ALARM, INVALID_ALARM);
      return (READ_ALARM);
   }/*end if error*/
   
  /*---------------------
   * Remove error bits and return sign-extended raw data to record.
   */
   prec->rval = (data & J1616_DMASK) * ((data&J1616_SIGN_BIT) ? -1:1);
   return (OK);
      
}/*end j1616_read()*/
