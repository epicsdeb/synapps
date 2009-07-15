/******************************************************************************
 * devCamacK3094.c -- Device Support Routines for Kinetic Systems 3094
 *                       Latched Binary Input/Output Modules
 *
 *----------------------------------------------------------------------------
 * Author:	Eric Bjorklund
 * Date:	15-May-1995
 *
 *----------------------------------------------------------------------------
 * MODIFICATION LOG:
 *
 * 15-May-1995	Bjo	First release
 * 09-Jan-1996	Bjo	Changes for EPICS R3.12
 *
 *----------------------------------------------------------------------------
 * MODULE DESCRIPTION:
 *
 * This module provides EPICS CAMAC device support for the Kinetic Systems 3094
 * latched binary I/O card.
 *
 * Device support is provided for both the output (BO) register and the input
 * (BI) register.
 *
 *----------------------------------------------------------------------------
 * CARD DESCRIPTION:
 *
 * The K3094 has a 16-bit relay-driven output register and a 16-bit input
 * register.  Under normal operation ("Local Mode") the output register
 * tracks the contents of the input register.  The output register may also
 * be commanded via CAMAC write, selective-set, and selective-clear commands.
 * When the output register is commanded via the CAMAC dataway, the module
 * enters "Command Mode", and remains there until the contents of the input
 * register match the contents of the output register (i.e. the command
 * has taken effect).  While in Command Mode, the output register may be
 * written to by other CAMAC commands, but it will not be affected by the
 * contents of the input register until the input and output registers match,
 * at which time the card reverts to Local Mode.
 *
 * Strap settings are provided to disable latching (inputs and outputs become
 * two independent 16-bit registers) and to place selected channels in
 * "Momentary Mode" (relay releases when Local Mode is reached).
 *
 *----------------------------------------------------------------------------
 * RECORD FIELDS OF INTEREST:
 *
 * The following fields should be set by database configuration:
 *	Type	= bi or bo
 *	DTYP	= Camac K3094
 *	INP/OUT	= B -> Branch number of K3094 card
 *		  C -> Crate number of K3094 card
 *		  N -> Slot number of K3094 card
 *		  PARM -> Bit number (0 - 15).
 *
 * Note that the A and F specifications in the OUT field are not used and
 * may be omitted.
 *
 *----------------------------------------------------------------------------
 * NOTES:
 *
 * This device support module specifies a readback function code for the
 * bo register.  The readback value, however, comes from the input register
 * rathar than from the output register itself.
 *
 * In this implementation of the device support module, it is possible for
 * faulty hardware, or ill-timed commands, to cause the card to never exit
 * Command Mode, thereby effectively disabling local control of the
 * equipment.  In a future version of this device support module, we should
 * wait a respectable time for the command to take effect and then force the
 * card back into local mode with an F(12) command.
 *
 * The K3094 cards in use at LANSCE have a local modification which will
 * prevent a C or Z from clearing the output register.  On power-up, the
 * module is placed in Local Mode so that the output register will track the
 * current state of the input register.
 *
 *****************************************************************************/

/************************************************************************/
/*  Header Files							*/
/************************************************************************/

#include	<dbDefs.h>		/* EPICS Standard Definitions	    */
#include	<devSup.h>		/* EPICS Device Support Definitions */
#include	<camacLib.h>		/* EPICS ESONE CAMAC Library	    */
#include	<devCamac.h>		/* Standard CAMAC Device Support    */
#include	<dset.h>		/* Standard DSET structure defns.   */


/************************************************************************/
/*  Local Constants and Structures					*/
/************************************************************************/

/*---------------------
 * CAMAC Function codes used by this module
 */
#define	RD1	0		/* Group 1 read				 */
#define SS1	18		/* Group 1 selective set		 */


/************************************************************************/
/*  Local Device Support Routine Declarations				*/
/************************************************************************/

LOCAL STATUS   k3094_initBiRecord (struct biRecord*);
LOCAL STATUS   k3094_initBoRecord (struct boRecord*);


/************************************************************************/
/*  Device Support Entry Tables						*/
/************************************************************************/

struct bidset devBiCamacK3094 = {	/* K3094 Bi Record		    */
	5,				   /* 5 entries			    */
	NULL,				   /* -- No report routine	    */
	NULL,				   /* -- No device support init	    */
	(DEVSUPFUN) k3094_initBiRecord,	   /* Local record init routine	    */
	NULL,				   /* -- No I/O event support	    */
	(DEVSUPFUN) devCamac_biRead	   /* Use generic read routine	    */
};

struct bodset devBoCamacK3094 = {	/* K3094 Bo Record		    */
	5,				   /* 5 entries			    */
	NULL,				   /* -- No report routine	    */
	NULL,				   /* -- No device support init	    */
	(DEVSUPFUN) k3094_initBoRecord,	   /* Local record init routine	    */
	NULL,				   /* -- No I/O event support	    */
	(DEVSUPFUN) devCamac_boWrite	   /* Use generic write routine	    */
};

/************************************************************************/
/* k3094_initBiRecord () -- Record Initialization Routine for K3094	*/
/*			    Binary Input				*/
/*									*/
/************************************************************************/

LOCAL STATUS k3094_initBiRecord (struct biRecord *prec)
{
   struct camacio   *pcamacio;  /* Ptr to camacio part of INP field	*/

  /*---------------------
   * Parameter Structure for K3094 Binary Input
   */
   static devCamacParm  k3094_parms = {
      RD1,		/* Function code		*/
      0,		/* Subaddress			*/
      0, 0, 0		/* These fields not used	*/
   };/*end k3094_parms*/

  /*---------------------
   * Register card location with CAMAC driver
   */
   pcamacio = &(prec->inp.value.camacio);
   camacRegisterCard (pcamacio->b, pcamacio->c, pcamacio->n,
                      "K3094  Binary Input/Output", NULL, 0);

  /*---------------------
   * Invoke the standard BI record initialization routine and pass it our
   * own parameters.
   */
   prec->dpvt = &k3094_parms;
   return (devCamac_biInitRecord (prec));

}/*end k3094_initBiRecord()*/

/************************************************************************/
/* k3094_initBoRecord () -- Record Initialization Routine for K3094	*/
/*                          Binary Output  				*/
/*									*/
/************************************************************************/

LOCAL STATUS k3094_initBoRecord (struct boRecord *prec)
{
   struct camacio   *pcamacio;  /* Ptr to camacio part of OUT field	*/

  /*---------------------
   * Parameter Structure for K3094 Binary Output
   */
   static  devCamacParm  k3094_parms = {
      SS1,		/* Function code		*/
      0,		/* Subaddress			*/
      RD1,		/* Readback function		*/
      0, 0		/* These fields not used	*/
   };/*end k3094l_parms*/

  /*---------------------
   * Register card location with CAMAC driver
   */
   pcamacio = &(prec->out.value.camacio);
   camacRegisterCard (pcamacio->b, pcamacio->c, pcamacio->n,
                      "K3094  Binary Input/Output", NULL, 0);

  /*---------------------
   * Invoke the standard BO record initialization routine and pass it our
   * own parameters.
   */
   prec->dpvt = &k3094_parms;
   return (devCamac_boInitRecord (prec));

}/*end k3094_initBoRecord()*/
