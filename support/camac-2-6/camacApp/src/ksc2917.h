/******************************************************************************
 * ksc2917.h -- Hardware Definition Module for the Kinetic Systems 2917/3922
 *              Parallel Bus Driver.
 *
 *-----------------------------------------------------------------------------
 * Author:            Mark Rivers (Univ. of Chicago)
 *                    Adapted from the work of:
 *                      Eric Bjorklund and Rozelle Wright (LANL)
 *                      Marty Wise (CEBAF)
 *                      Ying Wu (Duke University)
 *
 * Origination Date:  4/28/95
 *
 *-----------------------------------------------------------------------------
 * MODIFICATION LOG:
 *
 * 06-Feb-1996	Bjo	Modifications for EPICS R3.12.2
 * 05-Mar-1996	MLR	Added routines for LAM handling
 * 08-Mar-1996	MLR	Improved error reporting and returns
 * 09-Mar-1996	MLR	Modified block transfer (DMA) routines to use interrupts
 * 30-Sep-2000	MLR	Added semaphore to serialize access to hardware,
 * 			disable LAM interrupts during I/O operations.
 *                      Added some debugging with new ksc2917Debug global.
 * 08-Feb-2001	MLR	Added hw_cssa_noblock.  The LAM interrupt routine was
 *                      calling hw_cssa, which calls semTake().  This is illegal
 *                      and was hanging tNetTask on the MVME167.
 *                      Created HW_INTERLOCK_ON and HW_INTERLOCK_OFF macros.
 * 15-Apr-2002	MLR     Added same debugging to hw_csblock that was in hw_cfblock
 * 
 *****************************************************************************/

/************************************************************************/
/*  Configuration Constants						*/
/************************************************************************/

/*=====================
 * The following constants identify the hardware as a Kinetic Systems
 *   2917/3922 VME/CAMAC interface
 */
#define	BRANCH_TYPE	OTHER		/* Not a standard Serial nor Parallel Highway Driver	*/

/*=====================
 * The following constant defines the maximum number of times which the
 * software should loop waiting for CSR Ready or Done.
 *
 * NOTE: 1000 represents a reasonable value for the MV167 board.
 * It may need to be adjusted if a faster CPU is used.
 */
/* TRY 10000 FOR PPC, TEMPORARY */
#define	MAX_WAITLOOPS	10000		/* Max loops waiting for ready	     */

/*=====================
 * The following constant defines the maximum time to allow per transfer for a
 * block transfer (DMA) operation. The total time allowed for a block transfer,
 * before declaring a timeout error, will be this value multiplied by the number
 * of CAMAC transfers in the operation. This value might need to be increased
 * if there are unusal modules which are operated in Q-Repeat mode and take
 * a long time to return Q=1.  It is probably best to talk to such modules with
 * software loops, since slow block transfers will tie up the Dataway.
 */
#define	MAX_BLOCKTIME	1.e-4		/* 100 usec per transfer 	    */

/*=====================
 * The following constant defines the minimum number of clock ticks to allow
 * before declaring a timeout on a block transfer (DMA) operation.  
 * A value greater than 1 is required, otherwise the semTake() will time out
 * before the operation completes.  A value of 3 seems safe.
 */
#define	MIN_DMA_CLOCK_TICKS 3		/* 3 clock ticks	    */

/* Debug support 
 * This macro only works for a single argument 
*/
#define Debug(l,FMT,ARG) {if (l <= ksc2917Debug) errlogPrintf(FMT,ARG);}
volatile int ksc2917Debug = 0;
 
/*=====================
 * Define 4 possible interrupt types.
 */
#define LAM_INTERRUPT   0
#define DONE_INTERRUPT  1
#define DMA_INTERRUPT   2
#define ABORT_INTERRUPT 3
  
/************************************************************************/
/*  Header Files							*/
/************************************************************************/

#include	<intLib.h>		/* Interrupt service library	*/
#include	<iv.h>			/* Interrupt vector definitions	*/
#include	<rebootLib.h>		/* Reboot hook definitions	*/
#include	<vme.h>			/* VME address descriptions	*/
#include	<sys/types.h>		/* Arch-independant type defs	*/

#include	<errlog.h>		/* This is from EPICS. If EPICS */
 					/* is not being used just define */
					/* errlogPrintf to be printf */

/************************************************************************/
/* Debug data. At present, this is simply global data that may be	*/
/* operated upon by the functions to accumulate operational statistics	*/
/* on the functions, and read by diagnostic routines. Ultimately, this	*/
/* should become shared memory, or some other, more formal intertask	*/
/* communication mechanism.						*/
/************************************************************************/

struct	glob_dat {
	int	total;
	int	read_error[5];
	int	write_error[5];
	int	cmd_error[5];
	int	total_err;
} ;
#ifdef CAMACLIB
int debug_hook = 0;
struct glob_dat debug_dat;
#else 
extern int debug_hook;
extern struct glob_dat debug_dat;
#endif

/************************************************************************/
/* Register Definitions for the Kinetic Systems 2917/3922 Branch Driver	*/
/************************************************************************/

typedef struct /* ksc2917_registers */ {
  uint16_t volatile  cser;	/* 00 -- Control Status/Error Register 	*/
  uint16_t volatile  unused02;	/* 02 					*/
  uint16_t volatile  docr;	/* 04 -- Device/Operation Control Register*/
  uint16_t volatile  sccr;	/* 06 -- Sequence/Channel Control Register*/
  uint16_t volatile  unused08;	/* 08 				*/
  uint16_t volatile  mtc;	/* 0a -- Memory Transfer Count Register */
  uint16_t volatile  machi;	/* 0c -- Memory Address Counter High 	*/
  uint16_t volatile  maclo;	/* 0e -- Memory Address Counter Low 	*/
  uint16_t volatile  unused20[24]; /*					*/
  uint16_t volatile  icr_lam;	/* 40 -- LAM Interrupt Control Register	*/
  uint16_t volatile  icr_done;	/* 42 -- Done Interrupt Control Register*/
  uint16_t volatile  icr_dma;	/* 44 -- DMA Buffer Int. Control Register*/
  uint16_t volatile  icr_abort;	/* 46 -- List Abort Int. Control Register*/
  uint16_t volatile  ivr_lam;	/* 48 -- LAM Interrupt Vector Register	*/
  uint16_t volatile  ivr_done;	/* 4A -- Done Interrupt Vector Register	*/
  uint16_t volatile  ivr_dma;	/* 4C -- DMA Buffer Int. Vector Register*/
  uint16_t volatile  ivr_abort;	/* 4E -- List Abort Int. Vector Register*/
  uint16_t volatile  unused50[8]; /*					*/
  uint16_t volatile  amr;	/* 60 -- Address Modifier Register	*/
  uint16_t volatile  cmr;	/* 62 -- Command Memory Register	*/
  uint16_t volatile  cmar;	/* 64 -- Command Memory Address Register*/
  uint16_t volatile  cwc;	/* 66 -- Command Word Count Register 	*/
  uint16_t volatile  srr;	/* 68 -- Serice Request Register 	*/
  uint16_t volatile  dlr;	/* 6A -- Data Low Register		*/
  uint16_t volatile  dhr;	/* 6C -- Data High Register		*/
  uint16_t volatile  csr;	/* 6E -- Control Status Register 	*/
} ksc2917Regs;

#define ksc2917_cser		(pksc2917->cser)
#define ksc2917_docr		(pksc2917->docr)
#define ksc2917_sccr		(pksc2917->sccr)
#define ksc2917_mtc		(pksc2917->mtc)
#define ksc2917_machi		(pksc2917->machi)
#define ksc2917_maclo		(pksc2917->maclo)
#define ksc2917_icr_lam		(pksc2917->icr_lam)
#define ksc2917_icr_done	(pksc2917->icr_done)
#define ksc2917_icr_dma		(pksc2917->icr_dma)
#define ksc2917_icr_abort	(pksc2917->icr_abort)
#define ksc2917_ivr_lam		(pksc2917->ivr_lam)
#define ksc2917_ivr_done	(pksc2917->ivr_done)
#define ksc2917_ivr_dma		(pksc2917->ivr_dma)
#define ksc2917_ivr_abort	(pksc2917->ivr_abort)
#define ksc2917_amr		(pksc2917->amr)
#define ksc2917_cmr		(pksc2917->cmr)
#define ksc2917_cmar		(pksc2917->cmar)
#define ksc2917_cwc		(pksc2917->cwc)
#define ksc2917_srr		(pksc2917->srr)
#define ksc2917_dlr		(pksc2917->dlr)
#define ksc2917_dhr		(pksc2917->dhr)
#define ksc2917_csr		(pksc2917->csr)


/************************************************************************/
/* Define the Channel Status/Error Register bits			*/
/************************************************************************/

#define MASK_cser_coc		0x8000	/* Channel Operation Complete	*/
#define MASK_cser_ndt		0x2000	/* Normal device terminate	*/
#define MASK_cser_err  		0x1000	/* Error			*/
#define MASK_cser_act  		0x0800	/* Channel Active		*/
#define MASK_cser_ready  	0x0100	/* Ready			*/
#define MASK_cser_code		0x001f	/* Mask for entire error code field */
#define MASK_cser_berr		0x0009	/* Bus error			*/
#define MASK_cser_abort		0x0011	/* Software abort		*/

/************************************************************************/
/* Define the Device/Operation Control Register bits			*/
/************************************************************************/

#define MASK_docr_cycstl	0x8000	/* Cycle Steal Mode		*/
#define MASK_docr_read  	0x0080	/* Read (2917 to memory)	*/
#define MASK_docr_word  	0x0010	/* Word transfer		*/

/************************************************************************/
/* Define Sequence/Channel Control Register bits			*/
/************************************************************************/

#define MASK_sccr_start 	0x0080	/* Start DMA controller */
#define MASK_sccr_abort 	0x0010	/* Abort DMA transfer	*/

/************************************************************************/
/* Define Interrupt Control Register bits				*/
/************************************************************************/

#define MASK_icr_fautoclr 	0x0040	/* Flag auto clear 		*/
#define MASK_icr_intena 	0x0010	/* Interrupt enable		*/
#define MASK_icr_iautoclr 	0x0008	/* Interrupt auto clear 	*/

/************************************************************************/
/* Define Address Modifier Register bits				*/
/************************************************************************/
#define MASK_amr_lword 		0x0040	/* Longword. Must be one.	*/

/************************************************************************/
/* Define the Command Memory Register bits			      	*/
/************************************************************************/

#define	MASK_cmr_ws_24		0x0000	/* 24 bit transfer		*/
#define	MASK_cmr_ws_16		0x0002	/* 16 bit transfer		*/
#define	MASK_cmr_qm_stop	0x0000	/* Q Stop Mode			*/
#define	MASK_cmr_qm_ignore	0x0008	/* Q Ignore Mode		*/
#define	MASK_cmr_qm_repeat	0x0010	/* Q Repeat Mode		*/
#define	MASK_cmr_qm_scan	0x0018	/* Q Scan Mode			*/
#define	MASK_cmr_tm_single	0x0000	/* Single cycle transfer	*/
#define	MASK_cmr_tm_block	0x0020	/* Block transfer		*/
#define	MASK_cmr_tm_inline	0x0060	/* Inline write transfer	*/
#define	MASK_cmr_halt		0x0080	/* Halt instruction		*/
#define	MASK_cmr_jump		0x00C0	/* Jump instruction		*/

/************************************************************************/
/* Define the Control Status Register fields				*/
/************************************************************************/

#define	MASK_csr_go		0x0001	/* Start executing list		*/
#define	MASK_csr_noq		0x0002	/* Q=0				*/
#define	MASK_csr_nox		0x0004	/* X=0				*/
#define	MASK_csr_write		0x0020	/* VME to CAMAC transfer	*/
#define	MASK_csr_dma		0x0040	/* DMA mode			*/
#define	MASK_csr_done		0x0080	/* Command list complete	*/
#define	MASK_csr_ready		0x0100	/* DLR/DHR can be read/written	*/
#define	MASK_csr_reset		0x1000	/* Reset 2917			*/
#define	MASK_csr_timeout 	0x2000	/* Timeout 			*/
#define	MASK_csr_abort		0x4000	/* Abort 			*/
#define	MASK_csr_error		0x8000	/* Command list error 		*/


/************************************************************************/
/* Define the Crate Controller Parameters 				*/
/************************************************************************/
#define	CC_SLOT			30	/* Slot for addressing CC	*/

/*---------------------
 * CAMAC Function codes used for 3922 crate controller			
 */
#define READ_LMASK		 1	/* Read CC LAM mask register	*/
#define READ_LPATTERN		 1	/* Read CC LAM pattern register	*/
#define	READ_CC_STATUS		 1	/* Read CC status register	*/
#define WRITE_LMASK		17	/* Write CC LAM mask register	*/
#define	WRITE_CC_STATUS		17	/* Write CC status register	*/

/*---------------------
 * Define Bits in Crate Controller Status Register
 */
#define	MASK_cc_z	0x0001		/* Initiate a dataway Z		*/
#define	MASK_cc_c	0x0002		/* Initiate a dataway C		*/
#define	MASK_cc_dic	0x0004		/* Dataway inhibit controller	*/
#define	MASK_cc_dir	0x0040		/* Dataway inhibit read-back	*/
#define	MASK_cc_dblbuf	0x0080		/* Double buffer mode		*/
#define	MASK_cc_denb	0x0100		/* Enable demand messages	*/
#define	MASK_cc_idmd	0x0200		/* Set internal demand		*/
#define	MASK_cc_soln	0x2000		/* Switch is off-line (hardware)*/
#define	MASK_cc_lam	0x8000		/* LAM present			*/

/************************************************************************/
/* Structure Definitions						*/
/************************************************************************/

typedef struct /* cdreg_type */ {	/* Format of CAMAC channel variable  */
  unsigned char  crate;			/*   Crate number		     */
  unsigned char  branch;		/*   Branch number		     */
  unsigned int   :2;
  unsigned int   n:5;			/*   Slot number		     */
  unsigned int   a:4;			/*   Subaddress number		     */
  unsigned int   :5;
} cdreg_type;

typedef union /* double_word */ {	/* Access parts of long word	     */
   int         whole;			/*    Whole word		     */
   struct {
     uint16_t  hi;			/*    High-order word		     */
     uint16_t  lo;			/*    Low-order word		     */
   } half;
} double_word;

typedef struct /* hwInfo */ {		/* Hardware-specific information     */
   ksc2917Regs      *prBase;		/*  Register base address	     */
   uint16_t         csr;		/*  Copy of CSR 		     */
   SEM_ID           ready;		/*  Semaphore to signal hwy ready    */
   SEM_ID           lock;		/*  Semaphore to interlock access to */
                         		/*  hardware                         */
   int		    dma_timeout;	/*  Timout (clock ticks) for DMA     */
   int              dma_am;		/*  Address mode for DMA transfers   */
   int		    int_type;		/*  Flag to signal which interrupt   */
   char             *dma_offset;	/*  Address offset for DMA transfers */
   unsigned char    b;			/*  Branch number		     */
} hwInfo;


/************************************************************************/
/*  Utility Macro Definitions						*/
/************************************************************************/

/*=====================
 * Check for read function
 */
#define	READ_F(f)	(f < 8)

/*=====================
 * Check for write function
 */
#define	WRITE_F(f)	((f & 0x0018) == 0x0010)

/* Take semaphore and disable LAM interrupts so 2 threads can't access */
/* the 2917 simultaneuosly */
#define HW_INTERLOCK_ON \
     {semTake(phwInfo->lock, WAIT_FOREVER); \
      ksc2917_icr_lam  &= ~MASK_icr_intena;}

/* Release semaphore and re-enable LAM interrupts */
#define HW_INTERLOCK_OFF \
   {ksc2917_icr_lam  |= MASK_icr_intena; \
    semGive(phwInfo->lock);}


/************************************************************************/
/*  Local Variables							*/
/************************************************************************/

LOCAL
char      *drvName = "drvKsc2917";	/* Name of driver module	*/

LOCAL
int       one_second = 0;		/* No. of ticks in one second	*/

LOCAL
hwInfo    **pksc2917_info = NULL;	/* Pointer to hw info array	*/

LOCAL
int       ksc2917_num_cards = 1;	/* Number of cards, default=1	*/

LOCAL
char      *ksc2917_addrs=(char *)0xFF00;/* Base address of first card	*/
                                        /* Default=0XFF00		*/

LOCAL
int       ksc2917_ivec_base = 0x00A0;	/* Base interrupt vector of	*/
                                     	/* first card. Default=A0	*/

LOCAL
int       ksc2917_irq_level = 2;	/* Interrupt level, default=2	*/

/************************************************************************/
/*  Function Prototypes							*/
/************************************************************************/

GLOBAL_RTN void    ksc2917_setup(int, char*, int, int);

LOCAL_RTN  void    hw_LamInterrupt (hwInfo*);
LOCAL_RTN  void    hw_DoneInterrupt (hwInfo*);
LOCAL_RTN  void    hw_DMAInterrupt (hwInfo*);
LOCAL_RTN  void    hw_AbortInterrupt (hwInfo*);
INLINE_RTN void    hw_DMAInt (hwInfo*);
LOCAL_RTN  void    hw_reboot (int);

INLINE_RTN STATUS  hw_checkExtb (int[2], char*);
INLINE_RTN STATUS  hw_checkStatus (int, int, hwInfo*, int*);
INLINE_RTN STATUS  hw_checkBlockStatus (int, int, hwInfo*);
LOCAL_RTN  STATUS  hw_errorRecovery (int, int, hwInfo*, uint16_t*);
INLINE_RTN int     hw_extBranch (int);
INLINE_RTN int     hw_extCrate  (int);
INLINE_RTN STATUS  hw_waitReady (ksc2917Regs*, hwInfo*);
INLINE_RTN STATUS  hw_waitDone (ksc2917Regs*, hwInfo*);
INLINE_RTN hwInfo *hw_ccinit (int);
INLINE_RTN void    hw_cdreg (int*, int, int, int, int);
INLINE_RTN void    hw_cgreg (int, int*, int*, int*, int*);
INLINE_RTN STATUS  hw_cfsa (int, int, hwInfo*, int*, int*);
INLINE_RTN STATUS  hw_cssa (int, int, hwInfo*, short*, int*);
INLINE_RTN STATUS  hw_cssa_noblock (int, int, hwInfo*, short*, int*);
INLINE_RTN STATUS  hw_cfblock (int, int*, hwInfo*, int*, int[4], int);
INLINE_RTN STATUS  hw_csblock (int, int*, hwInfo*, short*, int[4], int);
INLINE_RTN STATUS  hw_cfmad (int, int[2], hwInfo*, int*, int[4]);
INLINE_RTN STATUS  hw_csmad (int, int[2], hwInfo*, short*, int[4]);
INLINE_RTN STATUS  hw_cfubc (int, int, hwInfo*, int*, int[4]);
INLINE_RTN STATUS  hw_csubc (int, int, hwInfo*, short*, int[4]);
INLINE_RTN STATUS  hw_cfubr (int, int, hwInfo*, int*, int[4]);
INLINE_RTN STATUS  hw_csubr (int, int, hwInfo*, short*, int[4]);
INLINE_RTN STATUS  hw_crateInit (crateFuncBlock*, hwInfo*);
INLINE_RTN void    hw_initFuncBlock (crateFuncBlock*, int, int);
INLINE_RTN int     hw_getLamPattern (crateFuncBlock*, hwInfo*);
INLINE_RTN void    hw_enableSlotLam (crateFuncBlock*, short int slot);
INLINE_RTN void    hw_disableSlotLam (crateFuncBlock*, short int slot);
INLINE_RTN void    hw_resetCrateLam (crateFuncBlock*);
INLINE_RTN STATUS  hw_cccc (crateFuncBlock*, hwInfo*);
INLINE_RTN STATUS  hw_cccz (crateFuncBlock*, hwInfo*);
INLINE_RTN STATUS  hw_cccd (crateFuncBlock*, hwInfo*, int);
INLINE_RTN STATUS  hw_ctcd (crateFuncBlock*, hwInfo*, int*);
INLINE_RTN STATUS  hw_ctgl (crateFuncBlock*, hwInfo*, int*);
INLINE_RTN STATUS  hw_ccci (crateFuncBlock*, hwInfo*, int);
INLINE_RTN STATUS  hw_ctci (crateFuncBlock*, hwInfo*, int*);
LOCAL_RTN  void    hw_dump_regs (hwInfo*);

/************************************************************************/
/* ksc2917_setup () -- Define the specific hardware setup in this IOC	*/
/*                     This routine should be called from startup.cmd	*/
/*                     if the KSC 2917 hardware is not the default	*/
/*                     configuration as defined above.			*/
/************************************************************************/

void ksc2917_setup(int num_cards, char *addrs, int ivec_base, int irq_level)
{
   ksc2917_num_cards = num_cards;
   ksc2917_addrs = addrs;
   ksc2917_ivec_base = ivec_base;
   ksc2917_irq_level = irq_level;
}

/*======================================================================*/
/*	Routines to Create and Manipulate CAMAC Channel Variables	*/
/*									*/


/************************************************************************/
/* hw_cdreg () -- Construct a CAMAC channel variable			*/
/*									*/
/************************************************************************/

INLINE_RTN
void hw_cdreg (int *ext, int b, int c, int n, int a)
{
   cdreg_type   cdreg = {0,0,0,0};	/* Temp variable for building channel variable	*/

  /*---------------------
   * Make sure branch number is legal
   */
   if ((unsigned char) b >= ksc2917_num_cards) {
      errno = S_camacLib_Bad_B;
      dbgLog ("%s: Routine cdreg -- Invalid branch value (%d)\n",
             (int)drvName, b, 0, 0, 0, 0);
      return;
   }/*end if branch is invalid*/

  /*---------------------
   * Pack branch, crate, slot and subaddress into temp
   */
   cdreg.branch = b;
   cdreg.crate  = c;
   cdreg.n      = n;
   cdreg.a      = a;

   *(cdreg_type *)ext = cdreg;

}/*end hw_cdreg()*/


/************************************************************************/
/* hw_cgreg () -- Unpack a CAMAC channel variable			*/
/*									*/
/************************************************************************/

INLINE_RTN
void hw_cgreg (int ext, int *b, int *c, int *n, int *a)
{
   register cdreg_type  cdreg;		/* Temporary channel variable	*/

   cdreg = *((cdreg_type *)&ext);
   *b = cdreg.branch;
   *c = cdreg.crate;
   *n = cdreg.n;
   *a = cdreg.a;

}/*end hw_cgreg()*/

/************************************************************************/
/* hw_extBranch () -- Extract branch number from CAMAC variable		*/
/*									*/
/************************************************************************/

INLINE_RTN
int  hw_extBranch (int ext) {
   return (int) (((cdreg_type *) &ext)->branch);
}/*end hw_extBranch()*/


/************************************************************************/
/* hw_extCrate () -- Extract crate number from CAMAC variable		*/
/*									*/
/************************************************************************/

INLINE_RTN
int  hw_extCrate (int ext) {
   return (int) (((cdreg_type *) &ext)->crate);
}/*end hw_extCrate()*/

/*======================================================================*/
/*		Miscellaneous Utility Routines				*/
/*									*/


/************************************************************************/
/* hw_checkExtb () -- Check starting and ending ext values for MAD rtns.*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_checkExtb (int extb[2], char *caller)
{
  /*---------------------
   * Check for starting address greater than ending address
   */
   if (extb[0] > extb[1]) {
      dbgLog ("%s: Routine %s -- Starting CAMAC address is greater than ending CAMAC address\n",
              (int)drvName, (int)caller, 0, 0, 0, 0);
      return S_camacLib_BadAddrSpec;
   }/*end if bad starting or ending address*/

  /*---------------------
   * Check for multiple branches
   */
   if (hw_extBranch(extb[0]) != hw_extBranch(extb[1])) {
      dbgLog ("%s: Routine %s -- Mulitiple branches not supported by this routine.\n",
             (int)drvName, (int)caller, 0, 0, 0, 0);
      return S_camacLib_MultiBranchM;
   }/*end if multiple branches specified*/

  /*---------------------
   * Check for multiple crates
   */
   if (hw_extCrate(extb[0]) != hw_extCrate(extb[1])) {
      dbgLog ("%s: Routine %s -- Multiple crates not supported by this routine.\n",
             (int)drvName, (int)caller, 0, 0, 0, 0);
      return S_camacLib_MultiCrate;
   }/*end if multiple crates specified*/

  /*---------------------
   * If we get to here, everything is OK
   */
   return OK;

}/*end hw_checkExtb*/



/************************************************************************/
/* hw_checkStatus () -- Check error and Q status after a single-cycle   */
/* CAMAC operation							*/
/*	o Returns status code and q reply				*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_checkStatus (int f, int ext, hwInfo *phwInfo, int *q)
{
   register ksc2917Regs  *pksc2917;	/* Pointer to register base address */
   uint16_t  csr;			/* Local copy of CSR		*/
   int  status = OK;			/* Local status variable	*/


  /*---------------------
   * If there are any error bits set in the CSR invoke the error 
   * recovery procedure.
   */
   pksc2917 = phwInfo->prBase;		/* Get base register address	*/
   csr = ksc2917_csr;
   if (csr & (MASK_csr_error | MASK_csr_timeout))
      status = hw_errorRecovery (f, ext, phwInfo, &csr);

  /*---------------------
   * Set the Q status
   */
   if (csr & MASK_csr_noq) {
      	*q=0; 
      	if (status == OK) status = S_camacLib_noQ;
      	if (status == S_camacLib_noX) status = S_camacLib_noQ_noX;
   } else *q=1;

  /*---------------------
   * Return Q and status
   */
   return status;

}/*end hw_checkStatus()*/


/************************************************************************/
/* hw_checkBlockStatus () -- Check error and Q status after a block 	*/
/* transfer CAMAC operation						*/
/*	o Returns status code 						*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_checkBlockStatus (int f, int ext, hwInfo *phwInfo)
{
   int  	status = OK;			/* Local status variable */
   uint16_t 	csr = phwInfo->csr;		/* Local copy of csr	 */


  /*---------------------
   * If there are any error bits set in the CSR invoke the error 
   * recovery procedure.
   */
   Debug(2, "hw_checkBlockStatus, interrupt = %d", phwInfo->int_type);
   Debug(2, "csr = %x\n", csr);
   if (csr & (MASK_csr_error | MASK_csr_timeout))
      status = hw_errorRecovery (f, ext, phwInfo, &csr);

  /*---------------------
   * Set the status
   */
   if (csr & MASK_csr_noq) {
      	if (status == OK) status = S_camacLib_noQ;
      	if (status == S_camacLib_noX) status = S_camacLib_noQ_noX;
   };

   return status;

}/*end hw_checkBlockStatus()*/


/************************************************************************/
/* hw_errorRecovery () -- Attempt error recovery			*/
/*	o Examine the CSR to determine what went wrong			*/
/*	  with the operation, log the error, and attempt an appropriate	*/
/*	  recovery procedure.						*/
/*	o NOTE: at present, all that this routine does is log the error	*/
/*	  and report it back through the returned status variable.	*/
/*	  Error recovery will be added later.				*/
/*									*/
/************************************************************************/

LOCAL_RTN
STATUS hw_errorRecovery (int f, int ext, hwInfo *phwInfo, uint16_t *csr)
{
   if (*csr & MASK_csr_timeout) {
      camacRecordError (ext, ERR_HW_TIMEOUT);
      return S_camacLib_Hw_Timeout;
   }/*end if hardware timeout*/
   else if (*csr & MASK_csr_nox) {
      camacRecordError (ext, ERR_NO_X);
      return S_camacLib_noX;
   }/*end if no X response*/
   else {
      camacRecordError (ext, ERR_CTRL_ERR);
      return S_camacLib_Hw_Timeout;
   }

   return OK;

}/*end hw_errorRecovery()*/

/************************************************************************/
/* hw_waitReady () -- Wait for the 2917 to be ready to read/write data	*/
/*	o Busy-waits for "csr_ready"					*/
/*	o Return status							*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_waitReady (ksc2917Regs *pksc2917, hwInfo *phwInfo)
{
   register uint16_t  csr = 0;	/* Contents of LAM status reg	*/
   int                wait_count;	/* Counter for wait loop	*/

   for (wait_count=0; wait_count < MAX_WAITLOOPS; wait_count++) {
      csr = ksc2917_csr;			/* Read CSR		*/
      if (csr & MASK_csr_timeout) return S_camacLib_Hw_Timeout;
      if (csr & MASK_csr_nox) return S_camacLib_noX;
      if (csr & MASK_csr_error) return S_camacLib_Hw_Timeout;
      if (csr & MASK_csr_ready) return OK;
   }/*end for*/

   /* Timeout */
   return S_camacLib_Hw_Timeout;

}/*end hw_waitReady()*/

/************************************************************************/
/* hw_waitDone () -- Wait for CSR "done" indicating transfer complete 	*/
/*	o Returns status						*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_waitDone (ksc2917Regs *pksc2917, hwInfo *phwInfo)
{
   register uint16_t  csr = 0;		/* Contents of LAM status reg	*/
   int                wait_count;	/* Counter for wait loop	*/

  /*---------------------
   * Wait for done or timeout
   */
   for (wait_count=0; wait_count < MAX_WAITLOOPS; wait_count++) {
      csr = ksc2917_csr;			/* Read CSR		*/
      if (csr & MASK_csr_timeout) return S_camacLib_Hw_Timeout;
      if (csr & MASK_csr_nox) return S_camacLib_noX;
      if (csr & MASK_csr_error) return S_camacLib_Hw_Timeout;
      if (csr & MASK_csr_done) return OK;
   }/*end for*/

   /* Timeout */
   return S_camacLib_Hw_Timeout;

}/*end hw_waitDone()*/

/*======================================================================*/
/*		KSC2917 LAM Interrupt Service Routine			*/
/* This routine is entered with the Interrupt Enable bit in the LAM     */
/* Interrupt Control Register cleared.  It was cleared by the hardware  */
/* during the interrupt acknowlege cycle, because we set the Auto-Clr   */
/* bit.  In this routine we clear the Demand Enable bit in the crate    */
/* controller, pass the LAM on to the LAM handling routines which at    */
/* normal (non-interrupt) level.  When all of the LAMs in the crate have*/
/* been cleared, Demand Enable will be turned back on.  This routine    */
/* re-sets the Interrupt Enable bit in the ICR, to enable interrupts    */
/* from other crates and from this crate once Demand Enable is re-set.  */
/*======================================================================*/

LOCAL_RTN
void hw_LamInterrupt (hwInfo  *phwInfo)
{
   register ksc2917Regs  *pksc2917;	/* Pointer to register base address */
   int                   crate;		/* Crate loop counter		    */
   uint16_t              srr;		/* Copy of Service Request Register */
   int16_t		 temp, mask;
   int			 q;
   int			 ext;

   phwInfo->int_type = LAM_INTERRUPT;	/* Flag which interrupt we got	*/
   pksc2917 = phwInfo->prBase;		/* Get base register address	*/

   srr = ksc2917_srr;			   /* Read Service Request Register */
   if ((srr & 0x00FF) != 0) {		   /* Does any crate have a LAM?    */
   					   /* Yes, loop over crates 	    */
      for (crate=0, mask=1; crate < 8; mask<<=1, crate++){  
         if (srr & mask) {	   	   /* Does this crate have LAM?	    */
            hw_cdreg(&ext, phwInfo->b,     /* Yes, turn off the Demand     */
            	   crate, CC_SLOT, 0);     /*    Enable for this crate 	   */
            hw_cssa_noblock(READ_CC_STATUS, ext, /* so we don't get           */
            	   phwInfo, &temp, &q);    /*	 interrupt storms          */
            temp &= ~MASK_cc_denb;
            hw_cssa_noblock(WRITE_CC_STATUS, ext, 
            	   phwInfo, &temp, &q);
            camacLamInt (phwInfo->b,	   /* Invoke the		    */
                   crate,		   /*   hardware-independent	    */
                   0);			   /*   LAM processing routine	    */
         }
      }
   }

  /*---------------------
   * Re-enable LAM interrupts on the 2917 (they are still disabled on the
   * crate controller which generated this LAM)
   */
   ksc2917_icr_lam |= MASK_icr_intena;

}/*end hw_LamInterrupt()*/

/*======================================================================*/
/*		KSC2917 Done Interrupt Service Routine			*/
/*======================================================================*/

LOCAL_RTN
void hw_DoneInterrupt (hwInfo  *phwInfo)
{
   phwInfo->int_type = DONE_INTERRUPT;	/* Flag which interrupt we got	*/
   hw_DMAInt(phwInfo);			/* Go do the work		*/
}
   
/*======================================================================*/
/*		KSC2917 DMA Interrupt Service Routine			*/
/*======================================================================*/

LOCAL_RTN
void hw_DMAInterrupt (hwInfo  *phwInfo)
{
   phwInfo->int_type = DMA_INTERRUPT;	/* Flag which interrupt we got	*/
   hw_DMAInt(phwInfo);			/* Go do the work		*/
}
   
/*======================================================================*/
/*		KSC2917 Abort Interrupt Service Routine			*/
/*======================================================================*/

LOCAL_RTN
void hw_AbortInterrupt (hwInfo  *phwInfo)
{
   phwInfo->int_type = ABORT_INTERRUPT;	/* Flag which interrupt we got	*/
   hw_DMAInt(phwInfo);			/* Go do the work		*/
}

/*======================================================================*/
/* Routine called from Done, DMA and Abort interrupt routines		*/
/*======================================================================*/

INLINE_RTN
void hw_DMAInt (hwInfo  *phwInfo)
{
   register ksc2917Regs  *pksc2917;	/* Pointer to register base	*/
   
   pksc2917 = phwInfo->prBase;		/* Get base register address	*/
   ksc2917_icr_dma  &= ~MASK_icr_intena;/* Disable DMA interrupts	*/
   ksc2917_icr_abort &= ~MASK_icr_intena;/* Disable Abort interrupts	*/
   ksc2917_sccr = MASK_sccr_abort; 	/* Abort any left over DMA */
   phwInfo->csr = ksc2917_csr;		/* Save the Control/Status reg	*/
   semGive (phwInfo->ready);		/* Release waiting task		*/

}/*end hw_DMAInt()*/

/************************************************************************/
/* hw_reboot () -- Reset hardware prior to reboot			*/
/*									*/
/************************************************************************/

LOCAL_RTN
void hw_reboot (int unused)
{
   int                   b;		/* Branch number		*/
   hwInfo               *phwInfo;	/* Pointer to hardware info	*/
   register ksc2917Regs  *pksc2917;	/* Pointer to register base	*/

  /*---------------------
   * Loop to search the hardware information array and disable interrupts
   * on every ksc2917 card we find.
   */
   for (b=0; b < ksc2917_num_cards; b++) {
      phwInfo = pksc2917_info[b];
      if (phwInfo != NULL) {		/* Check for card present	  */
         pksc2917 = phwInfo->prBase;	/* Get the register base address  */
         ksc2917_icr_lam = 0;		/* Disable interrupts		  */
         ksc2917_icr_done = 0;		/* Disable interrupts		  */
         ksc2917_icr_dma = 0;		/* Disable interrupts		  */
         ksc2917_icr_abort = 0;		/* Disable interrupts		  */
      }/*end if hardware card found*/
   }/*end for each possible card*/

}/*end hw_reboot()*/

/************************************************************************/
/* hw_ccinit () -- Initialize hardware for branch 			*/
/*									*/
/************************************************************************/

INLINE_RTN
hwInfo  *hw_ccinit (int b)
{
   ksc2917Regs           *base_addr;	/* Base addr for ksc2917 cards	  */
   int                   lam_int_vec;	/* LAM interrupt vector number	  */
   int                   done_int_vec;	/* Done Interrupt vector number	  */
   int                   dma_int_vec;	/* DMA Interrupt vector number	  */
   int                   abort_int_vec;	/* Abort Interrupt vector number  */
   uint16_t              csr;		/* Contents of LAM status reg.	  */
   register ksc2917Regs  *pksc2917;	/* Base address for this branch	  */
   hwInfo               *phwInfo;	/* Ptr to hardware info structure */
   register int          status;	/* Temporary status variable	  */

  /*---------------------
   * Get the number of clock ticks in one second (this is used to time-out
   * waits for DMA interrupts.
   */
   if (one_second == 0) one_second = sysClkRateGet ();

  /*---------------------
   * Allocate the array of hardware branch structures
   */
   if (pksc2917_info == NULL) {

      /* Allocate the array */
      pksc2917_info = (hwInfo **) calloc(ksc2917_num_cards, sizeof *pksc2917_info);

      /* Check for allocation error */
      if (pksc2917_info == NULL) {
         logMsg ("%s: Routine hw_ccinit -- Unable to allocate memory for ksc2917 hardware structure array.\n",
                (int)drvName, 0, 0, 0, 0, 0);
         return NULL;
      }/*end if could not allocate memory*/

      /* Add reboot hook to disable interrupts on each card in array */
      rebootHookAdd ((FUNCPTR)hw_reboot);

   }/*end if structure pointer array not allocated*/

  /*---------------------
   * Check for maximum number of hardware cards allowed
   */
   if (b >= ksc2917_num_cards) return NULL;

  /*---------------------
   * Get base address for first ksc2917 card
   */
   status = sysBusToLocalAdrs (
               VME_AM_SUP_SHORT_IO,	/* A16 space, supervisor mode	*/
               (char  *)ksc2917_addrs,	/* Bus address for 1st ksc2917	*/
               (char **)&base_addr);	/* Local address		*/
   if (status != OK) {
      logMsg ("%s: Routine hw_ccinit -- Unable to obtain A16 base address for ksc2917 cards.\n",
              (int)drvName, 0, 0, 0, 0, 0);
      return NULL;
   }/*end if can't get base address*/

  /*---------------------
   * Compute and probe the register base address for this card
   */
   pksc2917  = base_addr + b;
   status = vxMemProbe (
               (void *) &ksc2917_csr,	/* Probe CSR */
               VX_READ, 		/* Probe for read access	*/
               sizeof(short), 		/* Probe A16 address		*/
               (void *) &csr);	/* Save value read		*/
   if (status != OK) return NULL;

  /*---------------------
   * If the card is present and correct, report that we found it.
   */
   logMsg ("%s: Initializing Kinetic Systems 2917 Driver Card for Branch %d\n",
          (int)drvName, b, 0, 0, 0, 0);

  /*---------------------
   * Allocate a hardware-specific information structure for this branch.
   */
   phwInfo = calloc (1, sizeof(hwInfo));
   if (phwInfo == NULL) {
      logMsg ("%s: -- Unable to allocate memory for hardware-specific information structure for branch %d\n",
             (int)drvName, b, 0, 0, 0, 0);
      return NULL;
   }/*end if can't allocate hardware information structure */

  /*---------------------
   * Initialize the hardware-specific information structure
   */
   phwInfo->int_type = -1;              /* Set to invalid type               */
   phwInfo->prBase  = pksc2917;		/* Pointer to register base address  */
   phwInfo->b       = b;		/* Branch number		     */
   phwInfo->dma_am  = VME_AM_EXT_SUP_DATA; /* Address mode for DMA transfers  */
   phwInfo->ready = semBCreate (	/* Create "branch ready" semaphore   */
                      SEM_Q_FIFO,	/*    Queue in FIFO order	     */
                      SEM_EMPTY);	/*    Initially empty		     */
   if (phwInfo->ready == NULL) {
      logMsg ("%s: -- Unable to create 'ready' semaphore for branch %d\n",
             (int)drvName, b, 0, 0, 0, 0);
      free (phwInfo);
      return NULL;
   }/*end if can't create semaphore*/
   phwInfo->lock = semBCreate (		/* Create "branch lock" semaphore    */
                      SEM_Q_FIFO,	/*    Queue in FIFO order	     */
                      SEM_FULL);	/*    Initially full		     */
   if (phwInfo->lock == NULL) {
      logMsg ("%s: -- Unable to create 'lock' semaphore for branch %d\n",
             (int)drvName, b, 0, 0, 0, 0);
      free (phwInfo);
      return NULL;
   }/*end if can't create semaphore*/
   status = sysLocalToBusAdrs (		/* Compute address offset for DMA    */
               phwInfo->dma_am,		/* Address mode from above	     */
               (char  *)NULL,	        /* Local memory address 0	     */
               &phwInfo->dma_offset);	/* Corresponding VME address	     */
   if (status != OK) {
      logMsg ("%s: Routine hw_ccinit -- Unable to obtain VME address for DMA.\n",
              (int)drvName, 0, 0, 0, 0, 0);
      return NULL;
   }/*end if can't get local address*/

  /*---------------------
   * Connect to the interrupt service routines
   * Each branch uses 4 interrupt vectors - LAM, Done, DMA, Abort
   */ 
   lam_int_vec   = ksc2917_ivec_base + 4*b + 0;
   done_int_vec  = ksc2917_ivec_base + 4*b + 1;
   dma_int_vec   = ksc2917_ivec_base + 4*b + 2;
   abort_int_vec = ksc2917_ivec_base + 4*b + 3;
   status = intConnect (INUM_TO_IVEC (lam_int_vec), hw_LamInterrupt, 
   	(int)phwInfo);
   if (status != OK) {
      logMsg ("%s: -- Unable to connect branch %d to LAM interrupt service routine\n",
             (int)drvName, b, 0, 0, 0, 0);
      free (phwInfo);
      return NULL;
   }/*end if can't connect to interrupt service routine*/

   status = intConnect (INUM_TO_IVEC (done_int_vec), hw_DoneInterrupt, 
   	(int)phwInfo);
   if (status != OK) {
      logMsg ("%s: -- Unable to connect branch %d to DMA interrupt service routine\n",
             (int)drvName, b, 0, 0, 0, 0);
      free (phwInfo);
      return NULL;
   }/*end if can't connect to interrupt service routine*/
   
   status = intConnect (INUM_TO_IVEC (dma_int_vec), hw_DMAInterrupt, 
   	(int)phwInfo);
   if (status != OK) {
      logMsg ("%s: -- Unable to connect branch %d to LAM interrupt service routine\n",
             (int)drvName, b, 0, 0, 0, 0);
      free (phwInfo);
      return NULL;
   }/*end if can't connect to interrupt service routine*/

   status = intConnect (INUM_TO_IVEC (abort_int_vec), hw_AbortInterrupt, 
   	(int)phwInfo);
   if (status != OK) {
      logMsg ("%s: -- Unable to connect branch %d to LAM interrupt service routine\n",
             (int)drvName, b, 0, 0, 0, 0);
      free (phwInfo);
      return NULL;
   }/*end if can't connect to interrupt service routine*/

  /*---------------------
   * Initialize the 2917 module
   */
   pksc2917_info[b] = phwInfo;		/* Store address of hw info struct   */

   ksc2917_csr = MASK_csr_reset;	/* Reset 2917 to power up state      */
   ksc2917_sccr = MASK_sccr_abort;	/* Abort any left over DMA	     */
   ksc2917_ivr_lam = lam_int_vec;	/* LAM interrupt vector		     */
   ksc2917_ivr_done = done_int_vec;	/* Done interrupt vector     	     */
   ksc2917_ivr_dma = dma_int_vec;	/* DMA complete interrupt vector     */
   ksc2917_ivr_abort = abort_int_vec;	/* Abort interrupt vector 	     */
   /* Enable LAM interrupts, auto-clear, set IRQ level */
   ksc2917_icr_lam = MASK_icr_intena | MASK_icr_iautoclr | ksc2917_irq_level;	
   /* Disable Done, DMA and Abort interrupts, set auto-clear, set IRQ level */
   ksc2917_icr_done = MASK_icr_iautoclr | ksc2917_irq_level;	
   ksc2917_icr_abort = MASK_icr_iautoclr | ksc2917_irq_level;	
   ksc2917_icr_dma  = MASK_icr_iautoclr | ksc2917_irq_level;	

   sysIntEnable (ksc2917_irq_level);	/* Enable interrupts for this level  */

  /*---------------------
   * Report successful initialization
   */
   logMsg ("%s: -- KSC2917 Branch %d Successfully Initialized\n",
          (int)drvName, b, 0, 0, 0, 0);

   return phwInfo;	/* Return the address of the hardware info struct */

}/*end hw_ccinit()*/

/************************************************************************/
/* hw_cfsa () -- Perform a single, 24-bit CAMAC function		*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS  hw_cfsa (int f, int ext, hwInfo *phwInfo, int *data, int *q)
{
   double_word           cnaf;		/* Local copy of cnaf sequence	*/
   double_word           local_data;	/* Local data word		*/
   register ksc2917Regs  *pksc2917;	/* Pointer to register base	*/
   int                   status;	/* Local status variable	*/

  /*---------------------
   * Load the cnaf sequence
   */
   pksc2917 = phwInfo->prBase;		/* Get register base		*/
   cnaf.whole  = ext | f;

   HW_INTERLOCK_ON;
   
   ksc2917_cmar = 0;
   		/* Load the crate number, transfer mode, word-size 	*/
   ksc2917_cmr = cnaf.half.hi | MASK_cmr_tm_single | MASK_cmr_ws_24;	
   ksc2917_cmr = cnaf.half.lo;	/* Load the slot, subaddress & function	*/
   ksc2917_cmr = MASK_cmr_halt; /* Load Halt instruction */
   ksc2917_cmar = 0;
   if (WRITE_F(f)) {				/* Write operation */
        ksc2917_csr = MASK_csr_write | MASK_csr_go;
        status = hw_waitReady(pksc2917, phwInfo);
        if (status == OK) {
           local_data.whole = *data;
           ksc2917_dlr = local_data.half.lo;
           ksc2917_dhr = local_data.half.hi;
           status = hw_waitDone(pksc2917, phwInfo);
   	}
   }
   else if (READ_F(f)) { 			/* Read operation */
        ksc2917_csr = MASK_csr_go;
        status = hw_waitReady(pksc2917, phwInfo);
        if (status == OK) {
           local_data.half.lo = ksc2917_dlr;	/* Fetch LS 16 bits of data */
           local_data.half.hi = LSB(ksc2917_dhr);/* Fetch MS 8 bits of data */
           *data = local_data.whole;
   	}
   }
   else {					/* Control operation */
        ksc2917_csr = MASK_csr_go;
        status = hw_waitDone(pksc2917, phwInfo);
   }

   status = hw_checkStatus (f, ext, phwInfo, q);
   
   HW_INTERLOCK_OFF;

   /* Return status
   */
   return status;

}/*end hw_cfsa()*/

/************************************************************************/
/* hw_cssa () -- Perform a single, 16-bit CAMAC function		*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS  hw_cssa (int f, int ext, hwInfo *phwInfo, short *data, int *q)
{
   register ksc2917Regs  *pksc2917;	/* Pointer to register base	*/
   int                   status;	/* Local status variable	*/


   pksc2917 = phwInfo->prBase;	/* Get register base			*/
   HW_INTERLOCK_ON;
   status = hw_cssa_noblock(f, ext, phwInfo, data, q);
   HW_INTERLOCK_OFF;

   /* Return status */
   return status;

}/*end hw_cssa()*/

/************************************************************************/
/* hw_cssa_noblock () -- Perform a single, 16-bit CAMAC function        */
/* This does NOT take a semaphore or disable LAM interrupts, so it can  */
/* be called from the LAM interrupt routine                             */
/************************************************************************/

INLINE_RTN
STATUS  hw_cssa_noblock (int f, int ext, hwInfo *phwInfo, short *data, int *q)
{
   double_word           cnaf;		/* Local copy of cnaf sequence	*/
   register ksc2917Regs  *pksc2917;	/* Pointer to register base	*/
   int                   status;	/* Local status variable	*/


   /* Assume failure, set Q=0 */
   *q = 0;
   
  /*---------------------
   * Load the cnaf sequence
   */
   pksc2917 = phwInfo->prBase;	/* Get register base			*/
   cnaf.whole  = ext | f;

   ksc2917_cmar = 0;
   	   /* Load the crate number, transfer mode, word size		*/
   ksc2917_cmr = cnaf.half.hi | MASK_cmr_tm_single | MASK_cmr_ws_16;	
   ksc2917_cmr = cnaf.half.lo;	/* Load the slot, subaddress & function	*/
   ksc2917_cmr = MASK_cmr_halt; /* Load Halt instruction */
   ksc2917_cmar = 0;
   if (WRITE_F(f)) {				/* Write operation */
      ksc2917_csr = MASK_csr_write | MASK_csr_go;
      status = hw_waitReady(pksc2917, phwInfo);
      if (status == OK)  {
         ksc2917_dlr = *data;
         status = hw_waitDone(pksc2917, phwInfo);
      }
   }
   else if (READ_F(f)) { 			/* Read operation */
      ksc2917_csr = MASK_csr_go;
      status = hw_waitReady(pksc2917, phwInfo);
      if (status == OK) *data = ksc2917_dlr;    /* Fetch LS 16 bits of data */
   }
   else {					/* Control operation */
      ksc2917_csr = MASK_csr_go;
      status = hw_waitDone(pksc2917, phwInfo);
   }

   status = hw_checkStatus (f, ext, phwInfo, q);

   /* Return status
   */
   
   return status;

}/*end hw_cssa_noblock()*/

/************************************************************************/
/* hw_cfblock() -- Block transfer routine (24 bit). 			*/
/*   Called by cfmad, cfubr, etc.					*/
/*									*/
/************************************************************************/
INLINE_RTN
STATUS  hw_cfblock (int f, int *extb, hwInfo *phwInfo, int *intc, int cb[4],
	int qmode)
{
   register ksc2917Regs  *pksc2917;	/* Pointer to register base	*/
   int                   status;	/* Local status variable	*/
   double_word           cnaf;		/* Local copy of cnaf sequence	*/
   int                   i;		/* Loop counter			*/
   int                   *datap;	/* Local pointer		*/
   unsigned int          vme_addr;	/* Used to compute VME address	*/

   phwInfo->dma_timeout = cb[0]*MAX_BLOCKTIME*one_second;/* DMA	timeout */
   if (phwInfo->dma_timeout < MIN_DMA_CLOCK_TICKS ) 
   		phwInfo->dma_timeout = MIN_DMA_CLOCK_TICKS;
   Debug(2, "hw_cfblock: phwInfo->dma_timeout = %d\n", phwInfo->dma_timeout);
   pksc2917     = phwInfo->prBase;	/* Get register base address	*/

   HW_INTERLOCK_ON;

   ksc2917_cmar = 0;
   cnaf.whole = extb[0];
   ksc2917_cmr = cnaf.half.hi | MASK_cmr_tm_block | qmode | MASK_cmr_ws_24;
   ksc2917_cmr = cnaf.half.lo | f;
   ksc2917_cmr = -(cb)[0];	/* 2's complement transfer length	*/
   ksc2917_cmr = 0xFFFF;	/* Word count high (not used)		*/
   ksc2917_cmr = MASK_cmr_halt;
   ksc2917_cmar = 0;		/* Reset to start of list 		*/
   if (ksc2917Debug > 1) {
   	for (i=0; i<5; i++) errlogPrintf("hw_cfblock: cmr = %x\n", ksc2917_cmr);
   	ksc2917_cmar = 0; /* Reset to start of list 		*/
   }
   /* Compute VME address for DMA 	*/
   vme_addr      = (unsigned int)intc | (unsigned int)phwInfo->dma_offset;
   Debug(1, "hw_cfblock: intc = %p", intc);
   Debug(1, "vme_addr = %x\n", vme_addr);
   /* This address gets put into 3 different registers.			*/
   /*   Bits 24-31 go in amr (along with address modifier)		*/
   /*   Bits 16-23 go in machi						*/
   /*   Bits 0-15  go in maclo						*/
   ksc2917_amr = ((vme_addr >> 16) & 0xFF00) | MASK_amr_lword | phwInfo->dma_am;
   Debug(2, "hw_cfblock: amr = %x\n", ksc2917_amr);
   ksc2917_machi = vme_addr >> 16;
   Debug(2, "hw_cfblock: machi = %x\n", ksc2917_machi);
   ksc2917_maclo = vme_addr;
   Debug(2, "hw_cfblock: maclo = %x\n", ksc2917_maclo);
   ksc2917_mtc = 2 * (cb)[0];	/* DMA transfer length (16 bit words)	*/
   Debug(2, "hw_cfblock: mtc = %x\n", ksc2917_mtc);
   ksc2917_sccr = 0;		/* Release soft abort 			*/
   Debug(2, "hw_cfblock: sccr = %x\n", ksc2917_sccr);
   /* Note - the following seems strange because we are CLEARING the	*/
   /* coc, ndt and err bits. However, the DMA chip clears these fields	*/
   /* when they are written with a 1.					*/
   ksc2917_cser = MASK_cser_coc | MASK_cser_ndt | MASK_cser_err;
   Debug(2, "hw_cfblock: cser = %x\n", ksc2917_cser);
   /* Enable interrupts at end of DMA operation				*/
   ksc2917_icr_dma   |= MASK_icr_intena;	/* Enable DMA interrupt */
   Debug(2, "hw_cfblock: icr_dma = %x\n", ksc2917_icr_dma);
   ksc2917_icr_abort |= MASK_icr_intena;  	/* Enable Abort interrupt */
   Debug(2, "hw_cfblock: icr_abort = %x\n", ksc2917_icr_abort);

   if (WRITE_F(f)) {
      ksc2917_docr = MASK_docr_cycstl | MASK_docr_word;
						/* Set DMA engine for writes */
      ksc2917_sccr = MASK_sccr_start;		/* Start DMA engine */
      ksc2917_csr = MASK_csr_write | MASK_csr_go | MASK_csr_dma;
      status = semTake(phwInfo->ready, phwInfo->dma_timeout);
   } 
   else if (READ_F(f)) {			/* Read operation */
      ksc2917_docr = MASK_docr_cycstl | MASK_docr_read | MASK_docr_word;
						/* Set DMA engine for reads */
      Debug(2, "hw_cfblock: docr = %x\n", ksc2917_docr);
      ksc2917_sccr = MASK_sccr_start;	/* Start DMA engine */
      Debug(2, "hw_cfblock: sccr = %x\n", ksc2917_sccr);
      ksc2917_csr = MASK_csr_go | MASK_csr_dma;
      Debug(2, "hw_cfblock: csr = %x\n", ksc2917_csr);
      status = semTake(phwInfo->ready, phwInfo->dma_timeout);
   }
   else { 					/* Control operation */
      ksc2917_csr = MASK_csr_go;
      status = semTake(phwInfo->ready, phwInfo->dma_timeout);
   }
   Debug(2, "%s", "  After DMA operation\n");
   Debug(2, "hw_cfblock: cser = %x\n", ksc2917_cser);
   Debug(2, "hw_cfblock: csr = %x\n", ksc2917_csr);
   ksc2917_icr_dma  &= ~MASK_icr_intena;/* Disable DMA interrupts	*/
   ksc2917_icr_abort &= ~MASK_icr_intena;/* Disable Abort interrupts	*/
   ksc2917_sccr = MASK_sccr_abort; 	/* Abort any left over DMA */
   cb[1] = (unsigned short)((cb)[0] + ksc2917_cwc); /* Actual number of
                                                       transfers */
   if (status != OK) {
   	hw_dump_regs(phwInfo);
   	return S_camacLib_Hw_Timeout;
   }
   if (READ_F(f)) {
      datap = intc;			/* clear high bytes */
      for (i=0;i<cb[1];i++) *(char *)(datap++) = 0;
   }

   HW_INTERLOCK_OFF;

   status = hw_checkBlockStatus(f, extb[0], phwInfo);
   return status;
}


/************************************************************************/
/* hw_csblock() -- Block transfer routine (16 bit). 			*/
/*   Called by csmad, csubr, etc.					*/
/*									*/
/************************************************************************/
INLINE_RTN
STATUS  hw_csblock (int f, int *extb, hwInfo *phwInfo, short *intc, int cb[4],
	int qmode)
{
   register ksc2917Regs  *pksc2917;	/* Pointer to register base	*/
   int                   status;	/* Local status variable	*/
   int                   i;		/* Loop counter			*/
   double_word           cnaf;		/* Local copy of cnaf sequence	*/
   unsigned int          vme_addr;	/* Used to compute VME address	*/

   phwInfo->dma_timeout = cb[0]*MAX_BLOCKTIME*one_second;/* DMA	timeout */
   if (phwInfo->dma_timeout < MIN_DMA_CLOCK_TICKS) 
   		phwInfo->dma_timeout = MIN_DMA_CLOCK_TICKS;
   Debug(2, "hw_csblock: phwInfo->dma_timeout = %d\n", phwInfo->dma_timeout);
   pksc2917     = phwInfo->prBase;	/* Get register base address	*/

   HW_INTERLOCK_ON;

   ksc2917_cmar = 0;
   cnaf.whole = extb[0];
   ksc2917_cmr = cnaf.half.hi | MASK_cmr_tm_block | qmode | MASK_cmr_ws_16;
   ksc2917_cmr = cnaf.half.lo | f;
   ksc2917_cmr = -(cb)[0];	/* 2's complement transfer length 	*/
   ksc2917_cmr = 0xFFFF;	/* Word count high (not used)		*/
   ksc2917_cmr = MASK_cmr_halt;
   ksc2917_cmar = 0;		/* Reset to start of list 		*/
   if (ksc2917Debug > 1) {
   	for (i=0; i<5; i++) errlogPrintf("hw_csblock: cmr = %x\n", ksc2917_cmr);
   	ksc2917_cmar = 0; /* Reset to start of list 		*/
   }
   /* Compute VME address for DMA 					*/
   vme_addr      = (unsigned int)intc | (unsigned int)phwInfo->dma_offset;
   Debug(1, "hw_csblock: intc = %p", intc);
   Debug(1, "vme_addr = %x\n", vme_addr);
   /* This address gets put into 3 different registers.			*/
   /*   Bits 24-31 go in amr (along with address modifier)		*/
   /*   Bits 16-23 go in machi						*/
   /*   Bits 0-15  go in maclo						*/
   ksc2917_amr = ((vme_addr >> 16) & 0xFF00) | MASK_amr_lword | phwInfo->dma_am;
   Debug(2, "hw_csblock: amr = %x\n", ksc2917_amr);
   ksc2917_machi = vme_addr >> 16;
   Debug(2, "hw_csblock: machi = %x\n", ksc2917_machi);
   ksc2917_maclo = vme_addr;
   Debug(2, "hw_csblock: maclo = %x\n", ksc2917_maclo);
   ksc2917_mtc = cb[0];	/* DMA transfer length (16 bit words) */
   Debug(2, "hw_csblock: mtc = %x\n", ksc2917_mtc);
   ksc2917_sccr = 0;		/* Release soft abort */
   Debug(2, "hw_csblock: sccr = %x\n", ksc2917_sccr);
   /* Note - the following seems strange because we are CLEARING the	*/
   /* coc, ndt and err bits. However, the DMA chip clears these fields	*/
   /* when they are written with a 1.					*/
   ksc2917_cser = MASK_cser_coc | MASK_cser_ndt | MASK_cser_err;
   Debug(2, "hw_csblock: cser = %x\n", ksc2917_cser);
   /* Enable interrupts at end of DMA operation				*/
   ksc2917_icr_dma  |= MASK_icr_intena;	  	/* Enable DMA interrupt */
   Debug(2, "hw_csblock: icr_dma = %x\n", ksc2917_icr_dma);
   ksc2917_icr_abort |= MASK_icr_intena; 	/* Enable Abort interrupt */
   Debug(2, "hw_csblock: icr_abort = %x\n", ksc2917_icr_abort);
   if (WRITE_F(f)) {
      ksc2917_docr = MASK_docr_cycstl | MASK_docr_word;
						/* Set DMA engine for writes */
      ksc2917_sccr = MASK_sccr_start;	/* Start DMA engine */
      ksc2917_csr = MASK_csr_write | MASK_csr_go | MASK_csr_dma;
      status = semTake(phwInfo->ready, phwInfo->dma_timeout);
   } 
   else if (READ_F(f)) {			/* Read operation */
      ksc2917_docr = MASK_docr_cycstl | MASK_docr_read | MASK_docr_word;
						/* Set DMA engine for reads */
      Debug(2, "hw_csblock: docr = %x\n", ksc2917_docr);
      ksc2917_sccr = MASK_sccr_start;		/* Start DMA engine */
      Debug(2, "hw_csblock: sccr = %x\n", ksc2917_sccr);
      ksc2917_csr = MASK_csr_go | MASK_csr_dma; /* Start executing list */
      Debug(2, "hw_csblock: csr = %x\n", ksc2917_csr);
      status = semTake(phwInfo->ready, phwInfo->dma_timeout);
   }
   else { 					/* Control operation */
      ksc2917_csr = MASK_csr_go;
      status = semTake(phwInfo->ready, phwInfo->dma_timeout);
   }
   Debug(2, "%s", "  After DMA operation\n");
   Debug(2, "hw_csblock: cser = %x\n", ksc2917_cser);
   Debug(2, "hw_csblock: csr = %x\n", ksc2917_csr);
   ksc2917_icr_dma  &= ~MASK_icr_intena;/* Disable DMA interrupts	*/
   ksc2917_icr_abort &= ~MASK_icr_intena;/* Disable Abort interrupts	*/
   ksc2917_sccr = MASK_sccr_abort; 	/* Abort any left over DMA */
   if (status != OK) {
   	hw_dump_regs(phwInfo);
   	return S_camacLib_Hw_Timeout;
   }
   cb[1] = (unsigned short)((cb)[0] + ksc2917_cwc); /* Actual number of
                                                       transfers */
   status = hw_checkBlockStatus(f, extb[0], phwInfo);

   HW_INTERLOCK_OFF;

   return status;
}

/************************************************************************/
/* hw_cfmad() -- Address scan (24-bit)					*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS  hw_cfmad (int f, int extb[2], hwInfo *phwInfo, int *intc, int cb[4])
{
   int                   status;	/* Local status variable	*/

  /*---------------------
   * Execute the 24 bit block mode function
   */
   status = hw_cfblock (f, extb, phwInfo, intc, cb, MASK_cmr_qm_scan);

  /*---------------------
   * Return completion status
   */
   return status;

}/*end hw_cfmad()*/

/************************************************************************/
/* hw_csmad() -- Address scan (16-bit)					*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS  hw_csmad (int f, int extb[2], hwInfo *phwInfo, short *intc, int cb[4])
{
   int                   status;	/* Local status variable	*/

  /*---------------------
   * Execute the 16 bit block mode function
   */
   status = hw_csblock (f, extb, phwInfo, intc, cb, MASK_cmr_qm_scan);

  /*---------------------
   * Return completion status
   */
   return status;

}/*end hw_csmad()*/

/************************************************************************/
/* hw_cfubc() -- Repeat Until No Q (24-bit)				*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS  hw_cfubc (int f, int ext, hwInfo *phwInfo, int *intc, int cb[4])
{
   int                   status;	/* Local status variable	*/

  /*---------------------
   * Execute the 24 bit block mode function
   */
   status = hw_cfblock (f, &ext, phwInfo, intc, cb, MASK_cmr_qm_stop);

   /* Return completion status
   */
   return status;

}/*end hw_cfubc()*/

/************************************************************************/
/* hw_csubc() -- Repeat Until No Q (16-bit)				*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS  hw_csubc (int f, int ext, hwInfo *phwInfo, short *intc, int cb[4])
{
   int                   status;	/* Local status variable	*/

  /*---------------------
   * Execute the 16 bit block mode function
   */
   status = hw_csblock (f, &ext, phwInfo, intc, cb, MASK_cmr_qm_stop);

  /*---------------------
   * Return completion status
   */
   return status;

}/*end hw_csubc()*/

/************************************************************************/
/* hw_cfubr() -- Repeat Until Q (24-bit)				*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS  hw_cfubr (int f, int ext, hwInfo *phwInfo, int *intc, int cb[4])
{
   int                   status;	/* Local status variable	*/

  /*---------------------
   * Execute the 24 bit block mode function
   */
   status = hw_cfblock (f, &ext, phwInfo, intc, cb, MASK_cmr_qm_repeat);

   /* Return completion status
   */
   return status;

}/*end hw_cfubr()*/

/************************************************************************/
/* hw_csubr() -- Repeat Until Q (16-bit)				*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS  hw_csubr (int f, int ext, hwInfo *phwInfo, short *intc, int cb[4])
{
   int                   status;	/* Local status variable	*/

  /*---------------------
   * Execute the 16 bit block mode function
   */
   status = hw_csblock (f, &ext, phwInfo, intc, cb, MASK_cmr_qm_repeat);

   /* Return completion status
   */
   return status;

}/*end hw_csubr()*/

/************************************************************************/
/* hw_initFuncBlock () - Initialize the crateFuncBlock Structure	*/
/*									*/
/************************************************************************/

INLINE_RTN
void hw_initFuncBlock (crateFuncBlock *pfunc, int b, int c)
{
  hw_cdreg (&pfunc->cc_status,     b, c, CC_SLOT,  0);
  hw_cdreg (&pfunc->cc_lamPattern, b, c, CC_SLOT, 12);
  hw_cdreg (&pfunc->cc_lamMask,    b, c, CC_SLOT, 13);
  pfunc->disableSlotLam = (camacSlotLam*) hw_disableSlotLam;
  pfunc->enableSlotLam =  (camacSlotLam*) hw_enableSlotLam;
  pfunc->resetCrateLam =  (camacCrateLam*) hw_resetCrateLam;

}/*end hw_initFuncBlock()*/

/************************************************************************/
/* hw_crateInit () -- Initialize a KSC 3922 crate controller		*/
/*  o Clears the crate inhibit						*/
/*  o Disable service request						*/
/*  o Disables double-buffer mode					*/
/*  o Set LAM mask register to disable LAMs from all slots		*/
/************************************************************************/

INLINE_RTN
STATUS  hw_crateInit (crateFuncBlock *pfunc, hwInfo *phwInfo)
{
   int16_t            cc_stat = 0;		/* Clear demand enable for CC*/	
   int                lam_mask = 0;		/* Disable LAMs from all slots*/
   int                q;			/* Q response		     */
   int                status;			/* Local status variable     */

  /* Copy phwInfo into pfunc->hwPvt.  This should really be done in
   *  hw_initFuncBlock, but that routine does not have access to phwInfo.
   */
   pfunc->hwPvt = phwInfo;
   
  /*---------------------
   * Disable service request, disable double buffer mode, and clear inhibit
   */
   status = hw_cssa (WRITE_CC_STATUS, pfunc->cc_status, phwInfo, &cc_stat, &q);
   if (status != OK) return status;

  /*---------------------
   * Set LAM mask register to disable LAMs from all slots
   */
   status = hw_cfsa (WRITE_LMASK, pfunc->cc_lamMask, phwInfo, &lam_mask, &q);
   if (!q && (status == OK)) status = S_camacLib_noQ;
   return status;

}/*end hw_crateInit()*/

/************************************************************************/
/* hw_getLamPattern () -- Read the LAM pattern register			*/
/* Note: It is essential that this routine return only the pattern of   */
/* LAMs which could have generated an interrupt, i.e. not the LAM pattern */
/* register itself, but the AND of the LAM pattern register and the LAM */
/* mask register. If this is not done, then the following error occurs: */
/*  - LAMS are enabled from a LAM from slot=1 without a LAM disable   */
/*    function.  Those LAMs are then disabled, by calling disableSlotLam*/
/*  - Another LAM source (slot=2) is enabled and results in lamMonitor  */
/*    begin called.  lamMonitor calls this routine to get the lamPattern. */
/*    It will think incorrectly that the slot=1 LAM has fired if the    */
/*    raw LAM pattern register value is returned.			*/
/*									*/
/************************************************************************/

INLINE_RTN
int hw_getLamPattern (crateFuncBlock *pfunc, hwInfo *phwInfo)
{
   int   lam_pattern;			/* LAM pattern register		*/
   int   lam_mask;			/* LAM mask register		*/
   int   q;				/* Q status return		*/

   hw_cfsa (READ_LPATTERN, pfunc->cc_lamPattern, phwInfo, &lam_pattern, &q);
   if (!q) return 0;
   hw_cfsa (READ_LMASK, pfunc->cc_lamMask, phwInfo, &lam_mask, &q);
   if (!q) return 0;
   return (lam_pattern & lam_mask);

}/*end hw_getLamPattern()*/

/************************************************************************/
/* hw_disableSlotLam () -- Disable LAM from a specific slot  		*/
/*									*/
/************************************************************************/

INLINE_RTN
void hw_disableSlotLam (crateFuncBlock *pfunc, short int slot)
{
   int   lam_mask;			/* LAM mask register contents	*/
   int   q;				/* Q status return		*/
   hwInfo *phwInfo = (hwInfo *)pfunc->hwPvt;
   
   /* Read existing LAM mask register value */
   hw_cfsa (READ_LMASK, pfunc->cc_lamMask, phwInfo, &lam_mask, &q);
   lam_mask &= ~(1 << (slot-1));
   hw_cfsa (WRITE_LMASK, pfunc->cc_lamMask, phwInfo, &lam_mask, &q);

}/*end hw_disableSlotLam()*/

/************************************************************************/
/* hw_enableSlotLam () -- Enable LAM from a specific slot  		*/
/*									*/
/************************************************************************/

INLINE_RTN
void hw_enableSlotLam (crateFuncBlock *pfunc, short int slot)
{
   int   lam_mask;			/* LAM mask register contents	*/
   int   q;				/* Q status return		*/
   hwInfo *phwInfo = (hwInfo *)pfunc->hwPvt;

   /* Read existing LAM mask register value */
   hw_cfsa (READ_LMASK, pfunc->cc_lamMask, phwInfo, &lam_mask, &q);
   lam_mask |= (1 << (slot-1));
   hw_cfsa (WRITE_LMASK, pfunc->cc_lamMask, phwInfo, &lam_mask, &q);

}/*end hw_enableSlotLam()*/

/************************************************************************/
/* hw_resetCrateLam () -- Re-enable LAMs from a crate    		*/
/*									*/
/************************************************************************/

INLINE_RTN
void hw_resetCrateLam (crateFuncBlock *pfunc)
{
   hw_cccd(pfunc, (hwInfo *)pfunc->hwPvt, 1);

}/*end hw_resetCrateLam()*/

/************************************************************************/
/* hw_cccc() -- Issue dataway C to crate				*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_cccc (crateFuncBlock *pfunc, hwInfo *phwInfo)
{
   int16_t    mask=MASK_cc_c;		/* Mask for dataway C		 */
   int	      q;			/* Q status of operation	 */
   int        status;			/* Local status variable	 */
   int16_t    temp;			/* Local copy of status register */

  /*---------------------
   * Read the current value of the status register
   */
   status = hw_cssa (READ_CC_STATUS, pfunc->cc_status, phwInfo, &temp, &q);
   if (!q && (status == OK)) status = S_camacLib_noQ;
   if (status != OK) return status;

  /*---------------------
   * Issue the C and return the status
   */
   temp = temp | mask;
   status = hw_cssa (WRITE_CC_STATUS, pfunc->cc_status, phwInfo, &temp, &q);
   if (!q && (status == OK)) status = S_camacLib_noQ;
   return status;

}/*end hw_cccc()*/

/************************************************************************/
/* hw_cccd() -- Set / Clear demand enable for crate			*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_cccd (crateFuncBlock *pfunc, hwInfo *phwInfo, int l)
{
   int16_t    mask=MASK_cc_denb;	/* Mask for demand enable control    */
   int	      q;			/* Q status of operation	     */
   int        status;			/* Local status return		     */
   int16_t    temp;			/* Local copy of status register     */

  /*---------------------
   * Read the current value of the status register
   */
   status = hw_cssa (READ_CC_STATUS, pfunc->cc_status, phwInfo, &temp, &q);
   if (!q && (status == OK)) status = S_camacLib_noQ;
   if (status != OK) return status;
   
  /*---------------------
   * Determine whether we should be setting or clearing the demand enable
   */
   temp = (l ? temp | mask : temp & ~mask);

  /*---------------------
   * Perform the set or clear, and return the status
   */
   status = hw_cssa (WRITE_CC_STATUS, pfunc->cc_status, phwInfo, &temp, &q);
   if (!q && (status == OK)) status = S_camacLib_noQ;
   return status;

}/*end hw_cccd()*/

/************************************************************************/
/* hw_ccci() -- Set / Clear dataway Inhibit (I) for crate		*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_ccci (crateFuncBlock *pfunc, hwInfo *phwInfo, int l)
{
   int16_t    mask=MASK_cc_dic;		/* Mask for dataway inhibit control */
   int	      q;			/* Q status of operation	    */
   int        status;			/* Local status return		    */
   int16_t    temp;			/* Local copy of status register    */

  /*---------------------
   * Read the current value of the status register
   */
   status = hw_cssa (READ_CC_STATUS, pfunc->cc_status, phwInfo, &temp, &q);
   if (!q && (status == OK)) status = S_camacLib_noQ;
   if (status != OK) return status;
   
  /*---------------------
   * Determine whether we should be setting or clearing the demand enable
   */
   temp = (l ? temp | mask : temp & ~mask);

  /*---------------------
   * Perform the set or clear and return the status
   */
   status = hw_cssa (WRITE_CC_STATUS, pfunc->cc_status, phwInfo, &temp, &q);
   if (!q && (status == OK)) status = S_camacLib_noQ;
   return status;

}/*end hw_ccci()*/

/************************************************************************/
/* hw_cccz() -- Issue dataway Z to crate				*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_cccz (crateFuncBlock *pfunc, hwInfo *phwInfo)
{
   int16_t    mask=MASK_cc_z;		/* Mask for dataway Z		 */
   int        q;			/* Q status of operation	 */
   int        status;			/* Local status variable	 */
   int16_t    temp;			/* Local copy of status register */

  /*---------------------
   * Read the current value of the status register
   */
   status = hw_cssa (READ_CC_STATUS, pfunc->cc_status, phwInfo, &temp, &q);
   if (!q && (status == OK)) status = S_camacLib_noQ;
   if (status != OK) return status;

  /*---------------------
   * Issue the Z and return the status
   */
   temp = temp | mask;
   status = hw_cssa (WRITE_CC_STATUS, pfunc->cc_status, phwInfo, &temp, &q);
   if (!q && (status == OK)) status = S_camacLib_noQ;
   return status;

}/*end hw_cccz()*/

/************************************************************************/
/* hw_ctcd() -- Test demand enable on crate controller			*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_ctcd (crateFuncBlock *pfunc, hwInfo *phwInfo, int *l)
{
   int16_t    cc_status;		/* Contents of CC status register   */
   int	      q;			/* Q status of operation	    */
   int        status;			/* Local status variable	    */

  /*---------------------
   * Read the status register
   */
   status = hw_cssa (READ_CC_STATUS, pfunc->cc_status, phwInfo, &cc_status, &q);
   if (!q && (status == OK)) status = S_camacLib_noQ;

  /*---------------------
   * Return value of demand enable bit.
   */
   if (q) *l = (cc_status & MASK_cc_denb) ? TRUE: FALSE;
   return status;

}/*end hw_ctcd()*/

/************************************************************************/
/* hw_ctci() -- Test dataway inhibit on crate controller		*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_ctci (crateFuncBlock *pfunc, hwInfo *phwInfo, int *l)
{
   int16_t    cc_status;		/* Contents of CC status register   */
   int	      q;			/* Q status of operation	    */
   int        status;			/* Local status variable	    */

  /*---------------------
   * Read the status register
   */
   status = hw_cssa (READ_CC_STATUS, pfunc->cc_status, phwInfo, &cc_status, &q);
   if (!q && (status == OK)) status = S_camacLib_noQ;

  /*---------------------
   * Return value of dataway inhibit readback bit.
   */
   if (q) *l = (cc_status & MASK_cc_dir) ? TRUE: FALSE;
   return status;

}/*end hw_ctci()*/

/************************************************************************/
/* hw_ctgl() -- Test crate controller for presence of LAM		*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_ctgl (crateFuncBlock *pfunc, hwInfo *phwInfo, int *l)
{
   int16_t    cc_status;		/* Contents of CC status register   */
   int	      q;			/* Q status of operation	    */
   int        status;			/* Local status variable	    */

  /*---------------------
   * Read the status register
   */
   status = hw_cssa (READ_CC_STATUS, pfunc->cc_status, phwInfo, &cc_status, &q);
   if (!q && (status == OK)) status = S_camacLib_noQ;

  /*---------------------
   * Return TRUE if LAMs are pending (SLP bit is set)
   */
   if (q) *l = (cc_status & MASK_cc_lam) ? TRUE: FALSE;
   return status;

}/*end hw_ctgl()*/

/************************************************************************/
/* hw_dump_regs() -- Dump registers for diagnostic purposes 		*/
/*									*/
/************************************************************************/

void hw_dump_regs(hwInfo *phwInfo)
{
   register ksc2917Regs  *pksc2917;	/* Pointer to register base	*/
   int                   status;	/* Local status variable	*/
   unsigned short data;
   uint16_t save_cmar;			/* Local copy of cmar 		*/
   int i;				/* Loop counter 		*/

   pksc2917     = phwInfo->prBase;	/* Get register base address	*/
   
   printf(" KSC2917 interface -----------------------------------------\n");
   printf("   Base address: %x\n", (int)pksc2917);
   printf("   Last interrupt type: %d\n", phwInfo->int_type);
   status = vxMemProbe((char *)&ksc2917_cser,VX_READ,2,(char *)&data);
   if (status==OK) {
     printf("   cser: %8x",data);
     if (data & MASK_cser_coc) printf(" COC");
     if (data & MASK_cser_ndt) printf(" NDT");
     if (data & MASK_cser_err) printf(" ERR");
     if (data & MASK_cser_act) printf(" ACT");
     if ((data & MASK_cser_code)==MASK_cser_berr) printf(" BERR");
     if ((data & MASK_cser_code)==MASK_cser_abort) printf(" SOFT_ABRT");
     printf("\n");
   } else {
     printf("bus error accessing cser\n");
     return;
   }

   status = vxMemProbe((char *)&ksc2917_csr,VX_READ,2,(char *)&data);
   if (status==OK) {
     printf("    csr: %8x",data);
     if (data & MASK_csr_go) printf(" GO");
     if (data & MASK_csr_noq) printf(" NOQ");
     if (data & MASK_csr_nox) printf(" NOX");
     if (data & MASK_csr_write) printf(" WRITE");
     if (data & MASK_csr_dma) printf(" DMA");
     if (data & MASK_csr_done) printf(" DONE");
     if (data & MASK_csr_ready) printf(" RDY");
     if (data & MASK_csr_timeout) printf(" TIMEO");
     if (data & MASK_csr_error) printf(" ERROR");
     printf("\n");
   } else {
     printf("bus error accessing cser\n");
     return;
   }

   printf("   docr: %8x\n",ksc2917_docr);
   printf("   sccr: %8x\n",ksc2917_sccr);
   printf("    mtc: %8x\n",ksc2917_mtc);
   printf("  machi: %8x\n",ksc2917_machi);
   printf("  maclo: %8x\n",ksc2917_maclo);
   printf("    amr: %8x\n",ksc2917_amr);
   printf("    cmr: %8x\n",ksc2917_cmr);
   printf("   cmar: %8x\n",ksc2917_cmar);
   printf("    cwc: %8x\n",ksc2917_cwc);
   printf("    srr: %8x\n",ksc2917_srr);
   save_cmar=ksc2917_cmar;
   ksc2917_cmar=0;
   printf("List memory:\n");
   for (i=0; i<5; i++) printf("  %d  %x\n", i, ksc2917_cmr);
   ksc2917_cmar=save_cmar;
}
