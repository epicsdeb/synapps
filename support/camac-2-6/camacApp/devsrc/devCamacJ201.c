/******************************************************************************
 * devCamacJ201.c -- Device Support Routines for Jorway 201
 *                   Switches and Lights Diagnostic Cards
 *
 *----------------------------------------------------------------------------
 * Author:	Eric Bjorklund
 * Date:	15-Aug-1995
 *
 *----------------------------------------------------------------------------
 * MODIFICATION LOG:
 *
 * 15-Aug-1995	Bjo	First release
 * 09-Jan-1996	Bjo	Changes for EPICS R3.12
 *
 *----------------------------------------------------------------------------
 * MODULE DESCRIPTION:
 *
 * This module provides the LCS/EPICS CAMAC device support for the Jorway 201
 * "Switches & Lights" diagnostic card.  The Jorway 201 contains a 24-bit
 * manual switch register, an independent 24-bit light register, and LAM
 * generation ability.
 *
 * Generic device support is provided for longin and longout records.  The
 * default range for a longin or longout record is 24-bits, unipolar.  Range
 * and polarity can be overridden using the HOPR and LOPR fields of the record.
 * Range will be adjusted by device support to reflect the nearest power of 2
 * that will contain the difference between HOPR and LOPR.  If HOPR and LOPR
 * have different signs, the record will be considered bipolar.
 *
 * This module is also provides support for the LCS-specific CAMAC diagnostic
 * routines such as SWLITE, and the Serial Highway Health Monitor.  Device
 * support maintains a structure for each Jorway 201 card it finds.  This
 * structure contains the following information:
 *  o The branch, crate, and slot number of the J201 card.
 *  o A flag that can be used to mark whether the crate/card is offline.
 *  o A CAMAC register variable for reading or writing the light register.
 *  o A CAMAC register variable for reading the switch register.
 *  o A CAMAC register variable for reading the serial crate controller
 *    status register.
 *
 * The format of the j201_card structure is defined in the header file,
 * "devJ201.h"  A global variable, "j201_card_list", points to the start of a
 * linked list of j201_card structures.
 *
 *----------------------------------------------------------------------------
 * RECORD FIELDS OF INTEREST:
 *
 * The following fields should be set by database configuration:
 *	Type	= longin or longout
 *	DTYP	= Camac J201S -- Switch register (input only)
 *		  Camac J201L -- Light register (input and output)
 *	HOPR	= High operating value (used to override range)
 *	LOPR 	= Low operating value (usually 0 or -HOPR)
 *	INP/OUT	= B -> Branch number of J201 card
 *		  C -> Crate number of J201 card
 *		  N -> Slot number of J201 card
 *
 * Note that the A and F specifications in the OUT field are not used and
 * may be omitted.
 *
 *****************************************************************************/

/************************************************************************/
/*  Header Files							*/
/************************************************************************/

#include	<stdlib.h>		/* Standard C Definitions	    */

#include	<dbDefs.h>		/* EPICS Standard Definitions	    */
#include	<devSup.h>		/* EPICS Device Support Definitions */
#include	<camacLib.h>		/* ESONE CAMAC Library Routines	    */
#include	<devCamac.h>		/* Standard CAMAC Device Support    */
#include	<dset.h>		/* Standard DSET structure defns.   */
#include	<devJ201.h>		/* J201 card structure definitions  */


/************************************************************************/
/*  Global Variable Declarations					*/
/************************************************************************/

j201_card  *j201_card_list = NULL;	/* List head for J201 cards	*/


/************************************************************************/
/*  Local Device Support Routine Declarations				*/
/************************************************************************/

LOCAL STATUS   j201L_initLiRecord (struct longinRecord*);
LOCAL STATUS   j201S_initLiRecord (struct longinRecord*);
LOCAL STATUS   j201L_initLoRecord (struct longoutRecord*);

LOCAL STATUS   j201_commonLiInit (struct longinRecord*, const short int);
LOCAL STATUS   j201_initStruct (register struct camacio*);

/************************************************************************/
/*  Device Support Entry Tables						*/
/************************************************************************/

struct longindset devCamacJ201Lli = {	/* J201 Lights Long In Record	    */
	5,				   /* 5 entries			    */
	NULL,				   /* -- No report routine	    */
	NULL,				   /* -- No device support init	    */
	(DEVSUPFUN) j201L_initLiRecord,	   /* Local record init routine	    */
	NULL,				   /* -- No I/O event support	    */
	(DEVSUPFUN) devCamac_liRead	   /* Use generic read routine	    */
};

struct longindset devCamacJ201Sli = {	/* J201 Switches Long In Record	    */
	5,				   /* 5 entries			    */
	NULL,				   /* -- No report routine	    */
	NULL,				   /* -- No device support init	    */
	(DEVSUPFUN) j201S_initLiRecord,	   /* Local record init routine	    */
	NULL,				   /* -- No I/O event support	    */
	(DEVSUPFUN) devCamac_liRead	   /* Use generic read routine	    */
};

struct longoutdset devCamacJ201Llo = {	/* J201 Lights Long Out Record	    */
	5,				   /* 5 entries			    */
	NULL,				   /* -- No report routine	    */
	NULL,				   /* -- No device support init	    */
	(DEVSUPFUN) j201L_initLoRecord,	   /* Local record init routine	    */
	NULL,				   /* -- No I/O event support	    */
	(DEVSUPFUN) devCamac_loWrite	   /* Use generic write routine	    */
};

/************************************************************************/
/* j201L_initLiRecord () -- Record Initialization Routine for J201	*/
/*                          Light Register Long Input Record		*/
/*									*/
/************************************************************************/

LOCAL STATUS j201L_initLiRecord (struct longinRecord *prec)
{
   return j201_commonLiInit (prec, A(1));

}/*end j201L_initLiRecord()*/



/************************************************************************/
/* j201S_initLiRecord () -- Record Initialization Routine for J201	*/
/*                          Switch Register Long Input Record		*/
/*									*/
/************************************************************************/

LOCAL STATUS j201S_initLiRecord (struct longinRecord *prec)
{
   return j201_commonLiInit (prec, A(0));

}/*end j201S_initLiRecord()*/

/************************************************************************/
/* j201L_initLoRecord () -- Record Initialization Routine for J201	*/
/*                          Light Register Long Output Record		*/
/*									*/
/************************************************************************/

LOCAL STATUS j201L_initLoRecord (struct longoutRecord *prec)
{
   int   status;	/* Local status variable	*/

  /*---------------------
   * Allocate and initialize the CAMAC parameter block
   */
   devCamacParm  j201_parms = {
      F(16),		/* Function code for write	*/
      A(1),		/* Subaddress			*/
      F(0),		/* Readback function code	*/
      FALSE,		/* Default bipolar flag		*/
      0xffffff		/* Default range mask		*/
   };/*end j201_parms*/

  /*---------------------
   * If HOPR and LOPR are set, use them to compute the range mask and
   * bipolar flags.
   */
   if (prec->hopr || prec->lopr) {
      j201_parms.range_mask = devCamac_getMask (prec->hopr, prec->lopr);
      if ((prec->hopr ^ prec->lopr) < 0) j201_parms.bipolar = TRUE;
   }/*end if range set by HOPR & LOPR*/

  /*---------------------
   * If HOPR and LOPR are both 0, set them to default values.
   */
   else {
      prec->hopr = 0xffffff;
      prec->lopr = 0;
   }/*end if HOPR and LOPR both zero*/

  /*---------------------
   * Invoke the standard long output record initialization routine with the
   * parameter block we just constructed.
   */
   prec->dpvt = &j201_parms;
   status = devCamac_loInitRecord (prec);
   if (status != OK) return status;

  /*---------------------
   * Allocate and initialize the card structure if one does not already exist.
   */
   return j201_initStruct (&(prec->out.value.camacio));

}/*end j201L_initLoRecord()*/

/************************************************************************/
/* j201_commonLiInit () -- Common Record Initialization Routine for	*/
/*			   J201S and J201L Long Input Records.		*/
/*									*/
/************************************************************************/

LOCAL STATUS j201_commonLiInit (struct longinRecord *prec, const short int a)
{
   int   status;	/* Local status variable	*/

  /*---------------------
   * Allocate and initialize the CAMAC parameter block
   */
   devCamacParm  j201_parms = {
      F(0),		/* Function code for read		*/
      A(0),		/* Subaddress (fill in from parm list)	*/
      0,		/* Readback (not used)			*/
      FALSE,		/* Default bipolar flag			*/
      0xffffff		/* Default range mask			*/
   };/*end j201_parms*/
   j201_parms.a = a;	/* Use subaddress from parameter list	*/

  /*---------------------
   * If HOPR and LOPR are set, use them to compute the range mask and
   * bipolar flags.
   */
   if (prec->hopr || prec->lopr) {
      j201_parms.range_mask = devCamac_getMask (prec->hopr, prec->lopr);
      if ((prec->hopr ^ prec->lopr) < 0) j201_parms.bipolar = TRUE;
   }/*end if range set by HOPR & LOPR*/

  /*---------------------
   * If HOPR and LOPR are both 0, set them to default values.
   */
   else {
      prec->hopr = 0xffffff;
      prec->lopr = 0;
   }/*end if HOPR and LOPR both zero*/

  /*---------------------
   * Invoke the standard long input record initialization routine with the
   * parameter block we just constructed.
   */
   prec->dpvt = &j201_parms;
   status = devCamac_liInitRecord (prec);
   if (status != OK) return status;

  /*---------------------
   * Allocate and initialize the card structure if one does not already exist.
   */
   return j201_initStruct (&(prec->inp.value.camacio));

}/*end j201_commonLiInit()*/

/************************************************************************/
/* j201_initStruct () -- Allocate & Initialize J201 Card Structure	*/
/*									*/
/************************************************************************/

LOCAL STATUS  j201_initStruct (register struct camacio  *pcamacio)
{
   j201_card   *pJ201;		/* Pointer to card structure		*/
   int          status;		/* Local status variable		*/
   int          switch_ext;	/* CAMAC variable for switch register	*/

  /*---------------------
   * Construct the CAMAC register variable for the switch register.
   * Abort on invalid CAMAC address.
   */
   cdreg (&switch_ext, pcamacio->b, pcamacio->c, pcamacio->n, A(0));
   ctstat (&status);
   if (status != OK) return status;

  /*---------------------
   * Search the linked list to see if a structure for this card already
   * exists.  Exit if card already defined.
   */
   for (pJ201=j201_card_list; pJ201; pJ201 = pJ201->link)
      if (pJ201->switch_ext == switch_ext) break;

   if (pJ201 != NULL) return OK;

  /*---------------------
   * Card structure does not already exist, allocate one.
   */
   pJ201 = (j201_card *)malloc (sizeof(j201_card));
   if (pJ201 == NULL) return ERROR;

  /*---------------------
   * Initialize the branch, crate, slot, and offline flags.
   */
   pJ201->b = pcamacio->b;
   pJ201->c = pcamacio->c;
   pJ201->n = pcamacio->n;
   pJ201->offline = FALSE;

  /*---------------------
   * Initialize CAMAC variables for the switch register, light register, and
   * the crate controller status register.
   */
   pJ201->switch_ext = switch_ext;
   cdreg (&pJ201->light_ext, pcamacio->b, pcamacio->c, pcamacio->n, A(1));
   cdreg (&pJ201->crate_ext, pcamacio->b, pcamacio->c, N(30), A(0));

  /*---------------------
   * Add the structure for this card to the linked list.
   */
   pJ201->link = j201_card_list;
   j201_card_list = pJ201;

  /*---------------------
   * Register the card type with the CAMAC driver.
   */
   camacRegisterCard (pcamacio->b, pcamacio->c, pcamacio->n,
                     "J201   Switches & Lights", NULL, 0);

   return OK;

}/*end j201_initStruct()*/
