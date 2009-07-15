/******************************************************************************
 * devLgCamacK3924.c -- LAM Grader Support Module for Kinetic Systems 3924
 *
 *----------------------------------------------------------------------------
 * Author:	Eric Bjorklund
 * Date:	29-May-1995
 *
 *----------------------------------------------------------------------------
 * MODIFICATION LOG:
 *
 * 29-May-1995	Bjo	Original release
 * 08-Jan-1996	Bjo	Changes for EPICS R3.12
 *
 *----------------------------------------------------------------------------
 * MODULE DESCRIPTION:
 *
 * This module provides support routines for interfacing the Kinetic Systems
 * model 3924 serial LAM grader to the EPICS CAMAC driver.
 *
 * Seven routines are provided by this module:
 * o k3924_LamGrader    -- Declare the presence of a K3924 LAM grader at the
 *                         specified branch, crate, and slot.
 * o k3924_init         -- Initialize the crate function block and the
 *                         K3924 card.
 * o k3924_disableSlot  -- Disable demands for specified slot (by clearing the
 *                         relevant bit in the grader's LAM mask register).
 *                         This routine also clears the LAM grader's "Message
 *                         Suspend" flag, allowing it to also double for the
 *                         "Acknowledge Slot LAM" function.
 * o k3924_enableSlot   -- Enable demands for specified slot (by setting the
 *                         relevant bit in the grader's LAM mask register).
 * o k3924_disableCrate -- Disable demand message generation from the LAM
 *                         grader card.
 * o k3924_enableCrate  -- Enable demand message generation from the LAM 
 *                         grader card.
 * o k3924_resetHungDmd -- Reset the LAM grader "Hung Demand Message" flag.
 *
 *----------------------------------------------------------------------------
 * CARD DESCRIPTION:
 *
 * The Kinetic Systems 3924 is a serial LAM grader card which is based on
 * (but not completely compliant with) the ESONE serial driver sub-group
 * recommendations.  In particular it does not support the XEQ function to
 * reset demand message suspension (F25 A2).  This module relys on the 
 * selective clear (F22 A11) to re-enable message generation after a LAM
 * (since the EPICS CAMAC driver is capable of queuing demand messages).
 * The K3924 also uses a slightly different XEQ function (F25 A1) to clear
 * the "Hung Demand" flag than specified in the standard (F25 A3).
 *
 * The K3924 also supports a LAM pattern register, which is good since
 * reading the crate controller's LAM pattern register can sometimes
 * introduce long delays (100-200 msec) on some serial crate controllers
 * (e.g. BiRa 1502).  This module's LAM-grader initialization routine
 * replaces the CAMAC channel variable for reading the serial crate
 * controller's LAM pattern register with a channel variable which will read
 * the LAM pattern register from the K3924 instead.
 *  
 *****************************************************************************/

/************************************************************************/
/*  Header Files							*/
/************************************************************************/

#include	<vxWorks.h>		/* VxWorks Symbol Definitions	*/
#include	<camacLib.h>		/* ESONE CAMAC Library Routines	*/

/************************************************************************/
/*  Function Prototypes							*/
/************************************************************************/

LOCAL camacInitRtn   k3924_init;	/* LAM Grader init routine	*/
LOCAL camacSlotLam   k3924_disableSlot;	/* Disable LAMs for slot	*/
LOCAL camacSlotLam   k3924_enableSlot;	/* Enable LAMs for slot		*/

LOCAL camacCrateLam  k3924_disableCrate;/* Disable LAM grader		*/
LOCAL camacCrateLam  k3924_enableCrate;	/* Enable LAM grader		*/
LOCAL camacCrateLam  k3924_resetHungDmd;/* Reset hung demand flag	*/


/************************************************************************/
/*  Define Symbolic Constants						*/
/************************************************************************/

/*---------------------
 * CAMAC function codes used by this module
 */
#define	CLEAR_LAM	10		/* Clear internal LAM		*/
#define	CLEAR_MASK	11		/* Clear LAM mask regisater	*/
#define	SET_LAM_MASK	20		/* Set bit in LAM mask register	*/
#define	CLEAR_LAM_MASK	22		/* Clear bit in LAM mask reg	*/
#define	DISABLE_DMD	24		/* Disable demand generation	*/
#define	CLEAR_HUNG_DMD	25		/* Clear hung demand flag	*/
#define	ENABLE_DMD	26		/* Enable demand generation	*/

/*---------------------
 * CAMAC subaddress values used by the module
 */
#define	DEMAND_REG	 1		/* Subaddress for dmd ena/dis.	*/
#define	CLR_MASK_REG	11		/* Subaddress for clearing mask	*/
#define	SET_MASK_REG	13		/* Subaddress for setting mask	*/
#define	LAM_PAT_REG	14		/* Subaddress for LAM pattern	*/

/*---------------------
 * Other constants
 */
#define	MAX_SLOT	23		/* Maximum slot number		*/

/************************************************************************/
/* k3924_LamGrader () -- Declare presence of K3924 LAM Grader Card	*/
/*	o Informs CAMAC driver of location of K3924 card.		*/
/*	o CAMAC Driver will invoke the initialization routine after the	*/
/*	  crate has been initialized.					*/
/*									*/
/************************************************************************/

void   k3924_LamGrader (int b, int c, int n)
{
   camacDeclareInitRtn (k3924_init, b, c, n);

}/*end k3924_LamGrader*/

/************************************************************************/
/* k3924_init () -- Initialize function block and hardware for		*/
/*                  K3924 LAM grader card				*/
/*	o Computes CAMAC channel variables for enabling and disabling	*/
/*	  LAMs and reading the LAM pattern register.			*/
/*	o Provides function addresses for enabling and disabling LAMs	*/
/*	  and the LAM grader.						*/
/*	o Initializes the LAM grader and enables slots that had		*/
/*	  previously been enabled.					*/
/*	o NOTE: This routine also re-directs the LAM mask register	*/
/*	  address from the serial crate controller to the LAM grader.	*/
/*									*/
/************************************************************************/

LOCAL void   k3924_init (crateFuncBlock *pfunc, int b, int c, int n)
{
   short int      dummy;	/* Dummy data word			*/
   int            ext;		/* Temporary CAMAC channel variable	*/
   int            q;		/* Local Q status return		*/
   short int      slot;		/* Slot to check for enabling		*/
   unsigned int   slot_mask;	/* Mask used to check for enabled slots	*/

  /*---------------------
   * Register what kind of card this is (for the diagnostic routines)
   */
   camacRegisterCard (b, c, n, "K3924 Lam Grader", NULL, 0);

  /*---------------------
   * Initialize the CAMAC channel variables in the crate function block
   * Re-direct reads of the serial crate-controller's LAM pattern register
   * to the K3924 LAM pattern register.
   */
   cdreg (&pfunc->lg_demand,     b, c, n, DEMAND_REG);	/* Ena/Dis demands   */
   cdreg (&pfunc->lg_lamMaskClr, b, c, n, CLR_MASK_REG);/* Clear bit in mask */
   cdreg (&pfunc->lg_lamMaskSet, b, c, n, SET_MASK_REG);/* Set bit in mask   */
   cdreg (&pfunc->cc_lamPattern, b, c, n, LAM_PAT_REG);	/* Read LAM pattern  */

  /*---------------------
   * Initialize external function pointers
   */
   pfunc->disableSlotLam = k3924_disableSlot;
   pfunc->enableSlotLam = k3924_enableSlot;
   pfunc->ackSlotLam = k3924_disableSlot;

   pfunc->disableGrader = k3924_disableCrate;
   pfunc->enableGrader = k3924_enableCrate;
   pfunc->resetCrateLam = k3924_resetHungDmd;

  /*---------------------
   * Initialize the LAM grader card by disabling demand generation, clearing
   * the LAM mask, and clearing the internal LAM source.
   */
   cdreg (&ext, b, c, n, 0);				/* Internal LAM chan */
   cssa (DISABLE_DMD, pfunc->lg_demand, &dummy, &q);	/* Disable demands   */
   cssa (CLEAR_MASK, pfunc->lg_lamMaskSet, &dummy, &q);	/* Clear mask reg.   */
   cssa (CLEAR_LAM, ext, &dummy, &q);			/* Clear internal LAM*/

  /*---------------------
   * Check the software LAM mask and enable LAMs for any slot which the
   * CAMAC driver thinks should be enabled.  We need to do this because
   * this routine may have been called as a result of a crate re-initialize.
   */
   for ((slot = 1, slot_mask = 1); slot <= MAX_SLOT; (slot++, slot_mask<<=1)) {
      if (slot_mask & pfunc->lam_mask)
         cssa (SET_LAM_MASK, pfunc->lg_lamMaskSet, &slot, &q);
   }/*end for each slot*/

  /*---------------------
   * Reset the hung demand flag and enable demand generation
   */
   cssa (CLEAR_HUNG_DMD, pfunc->lg_demand, &dummy, &q);	/* Reset hung dmd    */
   cssa (ENABLE_DMD, pfunc->lg_demand, &dummy, &q);	/* Enable demands    */

}/*end k3924_init()*/

/************************************************************************/
/* k3924_disableSlot () -- Disable demand messages from specified slot.	*/
/*	o NOTE:  Clearing the mask register in the way we do it here	*/
/*	         (F22 A11) will also re-enable demands from other slots	*/
/*									*/
/************************************************************************/

LOCAL void   k3924_disableSlot (crateFuncBlock *pfunc, short int n)
{
   int   q;	/* Local Q return */

   cssa (CLEAR_LAM_MASK, pfunc->lg_lamMaskClr, &n, &q);

}/*end k3924_disableSlot()*/


/************************************************************************/
/* k3924_enableSlot () -- Enable demand messages from specified slot.	*/
/*									*/
/************************************************************************/

LOCAL void   k3924_enableSlot (crateFuncBlock *pfunc, short int n)
{
   int   q;	/* Local Q return */

   cssa (SET_LAM_MASK, pfunc->lg_lamMaskSet, &n, &q);

}/*end k3924_enableSlot()*/

/************************************************************************/
/* k3924_disableCrate () -- Disable demand messages from LAM Grader	*/
/*									*/
/************************************************************************/

LOCAL void   k3924_disableCrate (crateFuncBlock *pfunc)
{
   short int   dummy;			/* Dummy data word for cssa	*/
   int         q;			/* Local Q return		*/

   cssa (DISABLE_DMD, pfunc->lg_demand, &dummy, &q);

}/*end k3924_disableCrate()*/


/************************************************************************/
/* k3924_enableCrate () -- Enable demand messages from LAM Grader	*/
/*									*/
/************************************************************************/

LOCAL void   k3924_enableCrate (crateFuncBlock *pfunc)
{
   short int   dummy;			/* Dummy data word for cssa	*/
   int         q;			/* Local Q return		*/

   cssa (ENABLE_DMD, pfunc->lg_demand, &dummy, &q);

}/*end k3924_enableCrate()*/


/************************************************************************/
/* k3924_resetHungDmd () -- Reset the "hung demand" flag		*/
/*									*/
/************************************************************************/

LOCAL void   k3924_resetHungDmd (crateFuncBlock *pfunc)
{
   short int   dummy;			/* Dummy data word for cssa	*/
   int         q;			/* Local Q return		*/

   cssa (CLEAR_HUNG_DMD, pfunc->lg_demand, &dummy, &q);

}/*end k3924_resetHungDmd()*/
