/******************************************************************************
 * devAoCamacJDA16.c -- Device Support Routines for Jorway DAC-16 Modules 
 *
 *----------------------------------------------------------------------------
 * Author:	Rozelle Wright
 * Date:	07-Nov-1994
 *
 *----------------------------------------------------------------------------
 * MODIFICATION LOG:
 *
 * 07-Nov-1994	rmw	First release
 * 07-Nov-1995	Bjo	Register card with CAMAC Driver
 * 09-Jan-1995	Bjo	Changes for EPICS R3.12
 *
 *----------------------------------------------------------------------------
 * MODULE DESCRIPTION:
 *
 * This module provides EPICS CAMAC device support for the Joerger Enterprises
 * DAC-16 Analog output cards.  This code supports 2 variants of the card:
 *	JDA16A  = 12 bit unipolar output
 *      JDA16B  = 12 bit bipolar output
 *
 *----------------------------------------------------------------------------
 * CARD DESCRIPTION:
 *
 * The DAC-16 is a 16-channel 12-bit Digital to Analog converter.  The card
 * may be strapped for 0-10v, 0-5v, +/-10v, or +/-5v.  An 8-channel version
 * (DAC-8) is also available and may use this same device support module.
 *
 * Data is written to the card with an F(16) A(i) command.  Data can be read
 * back through an F(0) A(i) command.
 *
 *----------------------------------------------------------------------------
 * RECORD FIELDS OF INTEREST:
 *
 * The following fields should be set by database configuration:
 *	Type	= ao
 *	DTYP	= Camac JDA16A		(if strapped for unipolar output)
 *		  Camac JDA16B		(if strapped for bipolar output)
 *	OUT	= B -> Branch number of JDA16x card
 *		  C -> Crate number of JDA16x card
 *		  N -> Slot number of JDA16x card
 *		  PARM -> Channel number (0-15)
 *
 * Note that the A and F specifications in the OUT field are not used and
 * may be omitted.
 *
 *****************************************************************************/

/************************************************************************/
/*  Header Files							*/
/************************************************************************/

#include	<stdlib.h>	/* Standard C Routines			*/
#include	<dbDefs.h>	/* EPICS Standard Definitions		*/
#include	<devSup.h>	/* EPICS Device Support Definitions	*/
#include	<devCamac.h>	/* Standard EPICS CAMAC Device Support	*/
#include	<camacLib.h>	/* EPICS CAMAC Driver Library		*/
#include        <dset.h>        /* Standard DSET definitions		*/

/************************************************************************/
/*  Local Constants and Structures					*/
/************************************************************************/

/*====================
 * Parameter Structure for JDA16A
 */
static  devCamacParm  jda16a_parms = {
      16,		/* Write function code (F16)			*/
      0,		/* Subaddress (filled in from parameter field)	*/
      0,		/* Readback function code (F0)			*/
      FALSE,		/* unipolar device				*/
      0xfff		/* 12 bits of precision				*/
};/*end jda16b_parms*/

/*====================
 * Parameter Structure for JDA16B
 */
static  devCamacParm  jda16b_parms = {
      16,		/* Write function code (F16)			*/
      0,		/* Subaddress (filled in from parameter field)	*/
      0,		/* Readback function code (F0)			*/
      TRUE,		/* bipolar device				*/
      0xfff		/* 12 bits of precision				*/
};/*end jda16b_parms*/

/************************************************************************/
/*  Local Device Support Routine Declarations				*/
/************************************************************************/

LOCAL STATUS   jda16a_initRecord  (struct aoRecord*);
LOCAL STATUS   jda16b_initRecord  (struct aoRecord*);


/************************************************************************/
/*  Device Support Entry Tables						*/
/************************************************************************/

struct aodset devCamacJDA16A = {	/* 12 bit uni-polar output	    */
	6,				   /* 6 entries			    */
	NULL,				   /* --- No report routine	    */
	NULL,				   /* --- No device support init    */
	(DEVSUPFUN) jda16a_initRecord,	   /* Local record init routine	    */
	NULL,				   /* --- No I/O event support	    */
	(DEVSUPFUN) devCamac_aoWrite,	   /* Use generic write routine	    */
	(DEVSUPFUN) devCamac_aoLinConv	   /* Use generic linear conversion */
};

struct aodset devCamacJDA16B = {	/* 12 bit bi-polar output	    */
	6,				   /* 6 entries			    */
	NULL,				   /* --- No report routine	    */
	NULL,				   /* --- No device support init    */
	(DEVSUPFUN) jda16b_initRecord,	   /* Local record init routine	    */
	NULL,				   /* --- No I/O event support	    */
	(DEVSUPFUN) devCamac_aoWrite,	   /* Use generic write routine	    */
	(DEVSUPFUN) devCamac_aoLinConv	   /* Use generic linear conversion */
};

/************************************************************************/
/* jda16a_initRecord () -- Record Initialization Routine for JDA16A	*/
/*                         (12 bit unipolar )				*/
/*									*/
/************************************************************************/

LOCAL STATUS jda16a_initRecord (struct aoRecord *prec)
{
   struct camacio   *pcamacio;  /* Ptr to camacio part of OUT field	*/

  /*---------------------
   * Get parameter from parm field and put it in subaddress field (a) of
   * the parameter structure.
   */
   pcamacio = &(prec->out.value.camacio);
   jda16a_parms.a = atoi(pcamacio->parm);

  /*---------------------
   * Register the card with the CAMAC Driver
   */
   camacRegisterCard (pcamacio->b, pcamacio->c, pcamacio->n,
                     "JDA16A 12-bit DAC (unipolar)", NULL, 0);

  /*---------------------
   * Invoke the standard AO record initialization routine and pass it our
   * own parameters.
   */
   prec->dpvt = &jda16a_parms;
   return devCamac_aoInitRecord (prec);

}/*end jda16a_initRecord()*/



/************************************************************************/
/* jda16b_initRecord () -- Record Initialization Routine for JDA16B	*/
/*                         (12 bit bipolar )				*/
/*									*/
/************************************************************************/

LOCAL STATUS jda16b_initRecord (struct aoRecord *prec)
{
   struct camacio   *pcamacio;  /* Ptr to camacio part of OUT field	*/

  /*---------------------
   * Get parameter from parm field and put it in subaddress field (a) of
   * the parameter structure.
   */
   pcamacio = &(prec->out.value.camacio);
   jda16b_parms.a = atoi(pcamacio->parm);

  /*---------------------
   * Register the card with the CAMAC Driver
   */
   camacRegisterCard (pcamacio->b, pcamacio->c, pcamacio->n,
                     "JDA16B 12-bit DAC (bipolar)", NULL, 0);

  /*---------------------
   * Invoke the standard AO record initialization routine and pass it our
   * own parameters.
   */
   prec->dpvt = &jda16b_parms;
   return (devCamac_aoInitRecord (prec));

}/*end jda16b_initRecord()*/
