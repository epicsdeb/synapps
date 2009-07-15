/******************************************************************************
 * ht2992.h -- Hardware Definition Module for the Hytec VSD 2992 Serial
 *             Highway Link Driver.
 *
 *-----------------------------------------------------------------------------
 * AUTHORS:
 *	Eric Bjorklund and Rozelle Wright (LANL)
 *
 *	Adapted from the work of Marty Wise (CEBAF) and
 *	Ying Wu (Duke University)
 *
 * Origination Date:  8/10/94
 *
 *-----------------------------------------------------------------------------
 * MODIFICATION LOG:
 *
 * 08-10-94  bjo,rmw	Initial Release
 * 06-05-95  bjo	Added error recording
 * 12-14-95  bjo	Update for EPICS R3.12
 * 
 *****************************************************************************/

/************************************************************************/
/*  Configuration Constants						*/
/************************************************************************/

/*=====================
 * The following constants identify the hardware as a Hytec VSD2992
 * serial highway driver.
 */
#define	BRANCH_TYPE		SERIAL	/* Serial Highway Driver	*/
#define	HYTEC_ID		0xff7f 	/* Hytec Manufacturer Id	*/
#define VSD_2992		2992	/* Module Id Number		*/

/*====================
 * The following constants are default hardware addresses, and interrupt
 * levels for the VSD2992 card.
 */
#define HT2992_ADDRESS		0xDE00	/* Base address for Branch 0	*/
#define	HT2992_IRQ_LEVEL	3	/* Interrupt Request Level	*/
#define	HT2992_IVEC_BASE	0X80	/* Vector address for Branch 0	*/
#define	HT2992_NUM_CARDS	7	/* Maximum number of cards	*/

/*=====================
 * The following constants are used to determine whether we should wait for
 * command completion by busy-waiting or by taking an interrupt on "highway
 * ready."
 *
 * NOTE: These constants represent reasonable values for the MV167 board.
 * They may need to be adjusted if a faster CPU is used.
 */
#define	MAX_BUSYWAIT		1000000	/* Max loops waiting for ready	     */
#define	BUSYWAIT_CUTOFF		100	/* Cutoff point for busy-wait method */


/************************************************************************/
/*  Header Files							*/
/************************************************************************/

#include	<intLib.h>		/* Interrupt service library	*/
#include	<iv.h>			/* Interrupt vector definitions	*/
#include	<rebootLib.h>		/* Reboot hook definitions	*/
#include	<vme.h>			/* VME address descriptions	*/
#include	<sys/types.h>		/* Arch-independant type defs	*/

/************************************************************************/
/* Register Definitions for the Hytec VSD2992 Serial Highway Driver	*/
/************************************************************************/

typedef struct /* ht2992_read_registers */ {
  uint16_t volatile  rdid;	/* 00 -- Manufacturer id	  	*/
  uint16_t volatile  rmod;	/* 02 -- Module code		  	*/
  uint16_t volatile  rcsr;	/* 04 -- Controller status register	*/
  uint16_t volatile  unused06;	/* 06					*/
  uint16_t volatile  unused08;	/* 08					*/
  uint16_t volatile  rlmk;	/* 0A -- LAM mask register		*/
  uint16_t volatile  rvec;	/* 0C -- Interrupt vector register	*/
  uint16_t volatile  rlst;	/* 0E -- LAM status register		*/
  uint16_t volatile  unused10;	/* 10					*/
  uint16_t volatile  unused12;	/* 12					*/
  uint16_t volatile  unused14;	/* 14					*/
  uint16_t volatile  rsts;	/* 16 -- Status from last reply		*/
  uint16_t volatile  rdst;	/* 18 -- Demand status register		*/
  uint16_t volatile  rddb;	/* 1A -- Low-order data (16-bits)	*/
  uint16_t volatile  rddt;	/* 1C -- High-order data (8-bits)	*/
  uint16_t volatile  rdip;	/* 1E -- Read dump store & inc pointer	*/
  uint16_t volatile  rdpt;	/* 20 -- Read dump store pointer	*/
  uint16_t volatile  rups;	/* 22 -- U-port settings		*/
  uint16_t volatile  unused24;	/* 24					*/
  uint16_t volatile  unused26;	/* 26					*/
  uint16_t volatile  unused28;	/* 28					*/
  uint16_t volatile  unused2A;	/* 2A					*/
  uint16_t volatile  unused2C;	/* 2C					*/
  uint16_t volatile  unused2E;	/* 2E					*/
  uint16_t volatile  rtcs;	/* 30 -- Read crate (loop-back)		*/
  uint16_t volatile  rtxn;	/* 32 -- Read slot (loop-back)		*/
  uint16_t volatile  rtxa;	/* 34 -- Read sub-address (loop-back)	*/
  uint16_t volatile  rtxf;	/* 36 -- Read function code (loop-back)	*/
  uint16_t volatile  rdt4;	/* 38 -- Read data byte 4 (6-bit)	*/
  uint16_t volatile  rdt3;	/* 3A -- Read data byte 3 (6-bit)	*/
  uint16_t volatile  rdt2;	/* 3C -- Read data byte 2 (6-bit)	*/
  uint16_t volatile  rdt1;	/* 3E -- Read data byte 1 (6-bit)	*/
} ht2992_read_registers;

typedef struct /* ht2992_write_registers */ {
  uint16_t volatile  wvec;	/* 00 -- Interrupt vector register	*/
  uint16_t volatile  unused02;	/* 02					*/
  uint16_t volatile  wien;	/* 04 -- Controller status register	*/
  uint16_t volatile  unused06;	/* 06					*/
  uint16_t volatile  unused08;	/* 08					*/
  uint16_t volatile  wlmk;	/* 0A -- LAM mask register		*/
  uint16_t volatile  wclm;	/* 0C -- Clear LAM mask register	*/
  uint16_t volatile  wscl;	/* 0E -- Selective clear LAM register	*/
  uint16_t volatile  wcll;	/* 10 -- Clear LAM register		*/
  uint16_t volatile  wscm;	/* 12 -- Selective clear LAM mask reg	*/
  uint16_t volatile  wssm;	/* 14 -- Selective set LAM mask reg	*/
  uint16_t volatile  wnaf;	/* 16 -- Write NAF			*/
  uint16_t volatile  wtca;	/* 18 -- Write crate			*/
  uint16_t volatile  wtdb;	/* 1A -- Low order data (16 bits)	*/
  uint16_t volatile  wtdt;	/* 1C -- High order data (8 bits)	*/
  uint16_t volatile  wdsf;	/* 1E -- Enable dump store		*/
  uint16_t volatile  wdsp;	/* 20 -- Write dump store pointer	*/
  uint16_t volatile  wups;	/* 22 -- U-port setting			*/
  uint16_t volatile  wfif;	/* 24 -- Write FIFO			*/
  uint16_t volatile  wlbf;	/* 26 -- Enable loopback		*/
} ht2992_write_registers;

typedef union /* ht2992Regs */ {
  ht2992_read_registers   R;	/* Read registers			*/
  ht2992_write_registers  W;	/* Write registers			*/
} ht2992Regs;

#define	ht2992_rdid	(pht2992->R.rdid)	/* Manufacturer id	  */
#define ht2992_rmod	(pht2992->R.rmod)	/* Module code		  */
#define ht2992_rcsr	(pht2992->R.rcsr)	/* Controller status	  */
#define ht2992_rlmk	(pht2992->R.rlmk)	/* LAM mask register	  */
#define ht2992_rvec	(pht2992->R.rvec)	/* Interrupt vector	  */
#define ht2992_rlst	(pht2992->R.rlst)	/* LAM status register	  */
#define ht2992_rsts	(pht2992->R.rsts)	/* Status from last rply  */
#define ht2992_rdst	(pht2992->R.rdst)	/* Demand status reg	  */
#define ht2992_rddb	(pht2992->R.rddb)	/* Low-order data	  */
#define ht2992_rddt	(pht2992->R.rddt)	/* Hi-order data	  */
#define ht2992_rdip	(pht2992->R.rdip)	/* Read dump & inc ptr	  */
#define ht2992_rdpt	(pht2992->R.rdpt)	/* Dump store pointer	  */
#define ht2992_rups	(pht2992->R.rups)	/* U-Port settings	  */
#define ht2992_rtcs	(pht2992->R.rtcs)	/* Crate (loop back)	  */
#define ht2992_rtxn	(pht2992->R.rtxn)	/* Slot (loop back)	  */
#define ht2992_rtxa	(pht2992->R.rtxa)	/* Subaddress (loop back) */
#define ht2992_rtxf	(pht2992->R.rtxf)	/* Function (loop back)	  */
#define ht2992_rdt1	(pht2992->R.rdt1)	/* Data byte 4 (loop back)*/
#define ht2992_rdt2	(pht2992->R.rdt2)	/* Data byte 3 (loop back)*/
#define ht2992_rdt3	(pht2992->R.rdt3)	/* Data byte 2 (loop back)*/
#define ht2992_rdt4	(pht2992->R.rdt4)	/* Data byte 1 (loop back)*/

#define	ht2992_wvec	(pht2992->W.wvec)	/* Interrupt vector	  */
#define ht2992_wien	(pht2992->W.wien)	/* Controller status	  */
#define ht2992_wlmk	(pht2992->W.wlmk)	/* LAM mask register	  */
#define ht2992_wclm	(pht2992->W.wclm)	/* Clear LAM mask reg	  */
#define ht2992_wscl	(pht2992->W.wscl)	/* Sel. Clr. LAM reg.	  */
#define ht2992_wcll	(pht2992->W.wcll)	/* Clear LAM register	  */
#define ht2992_wscm	(pht2992->W.wscm)	/* Sel. Clr. LAM mask reg.*/
#define ht2992_wssm	(pht2992->W.wssm)	/* Sel. Set LAM mask reg. */
#define ht2992_wnaf	(pht2992->W.wnaf)	/* Write NAF		  */
#define ht2992_wtca	(pht2992->W.wtca)	/* Write crate		  */
#define ht2992_wtdb	(pht2992->W.wtdb)	/* Low-order data	  */
#define ht2992_wtdt	(pht2992->W.wtdt)	/* Hi-order data	  */
#define ht2992_wdsf	(pht2992->W.wdsf)	/* Enable dump store	  */
#define ht2992_wdsp	(pht2992->W.wdsp)	/* Write dump store ptr	  */
#define ht2992_wups	(pht2992->W.wups)	/* U-port settings	  */
#define ht2992_wfif	(pht2992->W.wfif)	/* Write FIFO		  */
#define ht2992_wlbf	(pht2992->W.wlbf)	/* Enable loopback	  */

/************************************************************************/
/* Define the ht2992 CSR register fields				*/
/************************************************************************/

#define	MASK_rcsr_busy	0x0001	/* Module is "booked"			*/
#define	MASK_rcsr_rdy	0x0010	/* Module is not "booked"		*/
#define	MASK_rcsr_xrsp	0x0020	/* X response from last command		*/
#define	MASK_rcsr_qrsp	0x0040	/* Q response from last command		*/
#define	MASK_rcsr_ie	0x0080	/* Interrupts enabled			*/
#define	MASK_rcsr_xmr	0x0100	/* Ready to transmit commands		*/
#define	MASK_rcsr_shr	0x0200	/* Serial highway ready (reply received)*/
#define	MASK_rcsr_lam	0x0400	/* Interrupt (LAM) pending		*/
#define	MASK_rcsr_sync	0x0800	/* Sync lost on serial highway		*/
#define	MASK_rcsr_dmd	0x1000	/* Demand message received		*/

#define	ht2992_rcsr_busy  (ht2992_rcsr & MASK_rcsr_busy)
#define	ht2992_rcsr_rdy   (ht2992_rcsr & MASK_rcsr_rdy )
#define	ht2992_rcsr_xrsp  (ht2992_rcsr & MASK_rcsr_xrsp)
#define	ht2992_rcsr_qrsp  (ht2992_rcsr & MASK_rcsr_qrsp)
#define	ht2992_rcsr_ie    (ht2992_rcsr & MASK_rcsr_ie  )
#define	ht2992_rcsr_xmr   (ht2992_rcsr & MASK_rcsr_xmr )
#define	ht2992_rcsr_shr   (ht2992_rcsr & MASK_rcsr_shr )
#define	ht2992_rcsr_lam   (ht2992_rcsr & MASK_rcsr_lam )
#define	ht2992_rcsr_sync  (ht2992_rcsr & MASK_rcsr_sync)
#define	ht2992_rcsr_dmd   (ht2992_rcsr & MASK_rcsr_dmd )


/************************************************************************/
/* Define the ht2992 LAM status register fields				*/
/************************************************************************/

#define	MASK_rlst_nq	0x0001	/* No Q on last command			*/
#define	MASK_rlst_nx	0x0002	/* No X on last command			*/
#define	MASK_rlst_err	0x0004	/* Error detected by serial crate ctrlr	*/
#define	MASK_rlst_derr	0x0008	/* Previous message had error		*/
#define	MASK_rlst_adnr	0x0010	/* Crate address not recognized		*/
#define	MASK_rlst_lpe	0x0020	/* Longitudinal parity error		*/
#define	MASK_rlst_tmo	0x0040	/* Timeout waiting for reply		*/
#define	MASK_rlst_tpe	0x0080	/* Transverse parity error		*/
#define	MASK_rlst_rdy	0x0100	/* Serial highway ready			*/
#define	MASK_rlst_xmr	0x0200	/* Command transmission complete	*/
#define	MASK_rlst_dmd	0x0400	/* Demand message received		*/
#define	MASK_rlst_serr	0x0800	/* Lost synchronization			*/

#define	ht2992_rlst_nq    (ht2992_rlst & MASK_rlst_nq  )
#define	ht2992_rlst_nx    (ht2992_rlst & MASK_rlst_nx  )
#define	ht2992_rlst_err   (ht2992_rlst & MASK_rlst_err )
#define	ht2992_rlst_derr  (ht2992_rlst & MASK_rlst_derr)
#define	ht2992_rlst_adnr  (ht2992_rlst & MASK_rlst_adnr)
#define	ht2992_rlst_lpe   (ht2992_rlst & MASK_rlst_lpe )
#define	ht2992_rlst_tmo   (ht2992_rlst & MASK_rlst_tmo )
#define	ht2992_rlst_tpe   (ht2992_rlst & MASK_rlst_tpe )
#define	ht2992_rlst_rdy   (ht2992_rlst & MASK_rlst_rdy )
#define	ht2992_rlst_xmr   (ht2992_rlst & MASK_rlst_xmr )
#define	ht2992_rlst_dmd   (ht2992_rlst & MASK_rlst_dmd )
#define	ht2992_rlst_serr  (ht2992_rlst & MASK_rlst_serr)

#define READY_MASK  (MASK_rlst_rdy  | MASK_rlst_xmr)

#define RESET_MASK  (MASK_rlst_nq   | MASK_rlst_nx   | MASK_rlst_err |	\
                     MASK_rlst_derr | MASK_rlst_adnr | MASK_rlst_lpe |	\
                     MASK_rlst_tmo  | MASK_rlst_tpe  | MASK_rlst_rdy |	\
                     MASK_rlst_xmr  | MASK_rlst_serr)

#define ERROR_MASK  (MASK_rlst_nx   | MASK_rlst_err  | MASK_rlst_adnr |	\
                     MASK_rlst_lpe  | MASK_rlst_tmo  | MASK_rlst_tpe  |	\
                     MASK_rlst_serr)

#define	HIGHWAY_BROKEN_MASK	(MASK_rlst_tmo | MASK_rlst_serr)

/************************************************************************/
/* Define the ht2992 NAF register fields				*/
/************************************************************************/

#define	CONST_wnaf_a1	0x0020	/* Constant for A=1			*/
#define	CONST_wnaf_n1	0x0200	/* Constant for N=1			*/

#define	MASK_wnaf_f	0x001f	/* Function code			*/
#define	MASK_wnaf_a	0x01e0	/* Subaddress				*/
#define	MASK_wnaf_n	0x3e00	/* Slot number				*/
#define	MASK_wnaf_mr	0x4000	/* "Repeat Mode"			*/
#define	MASK_wnaf_mq	0x8000	/* Used for "Address Scan Mode"		*/
#define	MASK_wnaf_mad	0xc000	/* "Address Scan Mode"			*/

#define	MAX_NAF		0x2fff	/* Maximum value for valid NAF		*/
#define	DUMMY_WRITE	0x0010	/* Dummy write cmd to stop block reads	*/

/************************************************************************/
/* Define the ht2992 U-Port register fields				*/
/************************************************************************/

#define	MASK_wups_reset	0x0080	/* Reset non-control functions		*/

/************************************************************************/
/* Structure Definitions						*/
/************************************************************************/

typedef struct /* cdreg_type */ {	/* Format of CAMAC channel variable  */
  unsigned char  branch;		/*   Branch number		     */
  unsigned char  crate;			/*   Crate number		     */
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
   ht2992Regs      *prBase;		/*    Register base address	     */
   int              timeout;		/*    Max times to busy-wait	     */
   SEM_ID           ready;		/*    Semaphore to signal hwy ready  */
   uint16_t         lstat;		/*    Contents of LAM status reg.    */
   unsigned char    waiting;		/*    Indicates waiting for ready    */
   unsigned char    b;			/*    Branch number		     */
} hwInfo;

typedef struct /* rdst_register */ {	/* Format of demand status register  */
   unsigned int  :4;			/*    Unused			     */
   unsigned int  crate:6;		/*    Crate number		     */
   unsigned int  m2:1;			/*    Demand message indicator	     */
   unsigned int  sgl:5;			/*    Slot (from serial grader)	     */
} rdst_register;

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

/*=====================
 * Get data word for block-mode read
 */
#define GET_DATA_WORD(intc, wordSize)					\
{									\
   if (wordSize == sizeof(int32_t)) {					\
      register double_word local_data;					\
      local_data.half.hi = LSB(ht2992_rddt);				\
      local_data.half.lo = ht2992_rddb;					\
      *((int32_t *)intc)++ = local_data.whole;				\
   }/*end full (32-bit) word read*/					\
									\
   else /*short (16 bit) word read*/ {					\
      *((int16_t *)intc)++ = ht2992_rddb;				\
   }/*end short word read*/						\
}

/*=====================
 * Put data word for block-mode write
 */
#define PUT_DATA_WORD(intc, wordSize)					\
{									\
   if (wordSize == sizeof(long int)) {					\
      ht2992_wtdt = *(short *)(intc);					\
      ht2992_wtdb = (short)(*intc);					\
   }/*end full word write*/						\
									\
   else /*short word write*/ {						\
      ht2992_wtdb = *(short *)intc;					\
   }/*end short word write*/						\
}

/************************************************************************/
/*  Local Variables							*/
/************************************************************************/

LOCAL
char      *drvName = "drvHt2992";	/* Name of driver module	*/

LOCAL
int        one_second = 0;		/* No. of ticks in one second	*/

LOCAL
hwInfo   **pht2992_info = NULL;		/* Pointer to hw info array	*/


/************************************************************************/
/*  Global Variables							*/
/************************************************************************/

int ht2992_addrs     = HT2992_ADDRESS;		/* Base address for Branch 0 */
int ht2992_irq_level = HT2992_IRQ_LEVEL;	/* Interrupt Request Level   */
int ht2992_ivec_base = HT2992_IVEC_BASE;	/* Vector addr for Branch 0  */
int ht2992_num_cards = HT2992_NUM_CARDS;	/* Maximum number of cards   */

/************************************************************************/
/*  Function Prototypes							*/
/************************************************************************/

LOCAL_RTN  void       hw_camacInterrupt (hwInfo *phwInfo);
LOCAL_RTN  void       hw_reboot (int unused);

INLINE_RTN STATUS     hw_checkExtb (int extb[2], char *caller);
INLINE_RTN STATUS     hw_checkStatus (int f, int ext, hwInfo *phwInfo, int *q, uint16_t *lstat);
LOCAL_RTN  STATUS     hw_errorRecovery (int f, int ext, hwInfo *phwInfo, uint16_t *lstat);
INLINE_RTN int        hw_extBranch (int ext);
INLINE_RTN int        hw_extCrate  (int ext);
INLINE_RTN uint16_t   hw_waitReply (ht2992Regs *pht2992, hwInfo *phwInfo);
LOCAL_RTN  uint16_t   hw_waitInterrupt (ht2992Regs *pht2992, hwInfo *phwInfo);
INLINE_RTN STATUS     hw_MADcmd (int f, double_word cnaf, int end_cnaf,
                                 ht2992Regs *pht2992, hwInfo *phwInfo,
                                 int rptMax, int *rptCount);
INLINE_RTN STATUS     hw_MADread (int f, double_word cnaf, int end_cnaf,
                                  ht2992Regs *pht2992, hwInfo *phwInfo,
                                  int *intc, int rptMax, int *rptCount,
                                  int wordSize);
INLINE_RTN STATUS     hw_MADwrite (int f, double_word cnaf, int end_cnaf,
                                   ht2992Regs *pht2992, hwInfo *phwInfo,
                                   int *intc, int rptMax, int *rptCount,
                                   int wordSize);
INLINE_RTN STATUS     hw_UBCcmd (int f, double_word cnaf, ht2992Regs *pht2992,
                                 hwInfo *phwInfo, int rptMax, int *rptCount);
INLINE_RTN STATUS     hw_UBCread (int f, double_word cnaf, ht2992Regs *pht2992,
                                  hwInfo *phwInfo, int *intc, int rptMax,
                                  int *rptCount, int wordSize);
INLINE_RTN STATUS     hw_UBCwrite (int f, double_word cnaf, ht2992Regs *pht2992,
                                   hwInfo *phwInfo, int *intc, int rptMax,
                                   int *rptCount, int wordSize);
INLINE_RTN STATUS     hw_UBRcmd (int f, double_word cnaf, ht2992Regs *pht2992,
                                 hwInfo *phwInfo, int rptMax, int *rptCount);
INLINE_RTN STATUS     hw_UBRread (int f, double_word cnaf, ht2992Regs *pht2992,
                                  hwInfo *phwInf, int *intc, int rptMax,
                                  int *rptCount, int wordSize);
INLINE_RTN STATUS     hw_UBRwrite (int f, double_word cnaf, ht2992Regs *pht2992,
                                   hwInfo *phwInfo, int *intc, int rptMax,
                                   int *rptCount, int wordSize);
INLINE_RTN hwInfo    *hw_ccinit (int b);
INLINE_RTN void       hw_cdreg (int *ext, int b, int c, int n, int a);
INLINE_RTN void       hw_cgreg (int ext, int *b, int *c, int *n, int *a);
INLINE_RTN STATUS     hw_cfsa (int f, int ext, hwInfo *phwInfo, int *dat, int *q);
INLINE_RTN STATUS     hw_cssa (int f, int ext, hwInfo *phwInfo, short *dat, int *q);
INLINE_RTN STATUS     hw_cfmad (int f, int extb[2], hwInfo *phwInfo, int *intc, int cb[4]);
INLINE_RTN STATUS     hw_csmad (int f, int extb[2], hwInfo *phwInfo, short *intc, int cb[4]);
INLINE_RTN STATUS     hw_cfubc (int f, int ext, hwInfo *phwInfo, int *intc, int cb[4]);
INLINE_RTN STATUS     hw_csubc (int f, int ext, hwInfo *phwInfo, short *intc, int cb[4]);
INLINE_RTN STATUS     hw_cfubr (int f, int ext, hwInfo *phwInfo, int *intc, int cb[4]);
INLINE_RTN STATUS     hw_csubr (int f, int ext, hwInfo *phwInfo, short *intc, int cb[4]);

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
   if ((unsigned char) b >= ht2992_num_cards) {
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
/* hw_checkStatus () -- Check error and Q status after a CAMAC operation*/
/*	o Returns status code and q reply				*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_checkStatus (int f, int ext, hwInfo *phwInfo, int *q, uint16_t *lstat)
{
   int  Qstat  = 0;		/* Local Q status variable	*/
   int  status = OK;		/* Local status variable	*/

  /*---------------------
   * If there are any error bits set in the LAM status register, or if the
   * "highway ready" bit is not set, invoke the error recovery procedure.
   */
   if ((*lstat & ERROR_MASK) || !(*lstat & MASK_rlst_rdy))
      status = hw_errorRecovery (f, ext, phwInfo, lstat);

  /*---------------------
   * Set the Q status
   */
   if (status == OK)
      if (!(*lstat & MASK_rlst_nq)) Qstat = 1;

  /*---------------------
   * Return Q and status
   */
   *q = Qstat;
   return status;

}/*end hw_checkStatus()*/

/************************************************************************/
/* hw_errorRecovery () -- Log error and attempt recovery		*/
/*	o Examine the LAM status register to determine what went wrong	*/
/*	  with the operation, log the error, and attempt an appropriate	*/
/*	  recovery procedure.						*/
/*	o NOTE: at present, all that this routine does is record the	*/
/*	  error in the branch, crate, or slot structure and return the	*/
/*	  appropriate error code back as the function value.  Recovery	*/
/*	  will be added later.						*/
/*									*/
/************************************************************************/

LOCAL_RTN
STATUS hw_errorRecovery (int f, int ext, hwInfo *phwInfo, uint16_t *lstat)
{

  /*---------------------
   * Hardware timeout waiting for reply message
   */
   if (*lstat & MASK_rlst_tmo) {
      camacRecordError (ext, ERR_HW_TIMEOUT);
      return S_camacLib_Hw_Timeout;
   }/*end if hardware timeout*/

  /*---------------------
   * Lost synch on serial highway
   */
   if (*lstat & MASK_rlst_serr) {
      camacRecordError (ext, ERR_LOST_SYNCH);
      return S_camacLib_Lost_Synch;
   }/*end if lost highway synch*/

  /*---------------------
   * Longitudinal parity error
   */
   if (*lstat & MASK_rlst_lpe) {
      camacRecordError (ext, ERR_LONG_PARITY);
      return S_camacLib_Long_Parity;
   }/*end if longitudinal parity error*/

  /*---------------------
   * Transverse parity error
   */
   if (*lstat & MASK_rlst_tpe) {
      camacRecordError (ext, ERR_TRANS_PARITY);
      return S_camacLib_Trans_Parity;
   }/*end if transverse parity error*/

  /*---------------------
   * Crate controller does not respond
   */
   if (*lstat & MASK_rlst_adnr) {
      camacRecordError (ext, ERR_NO_RESP);
      return S_camacLib_NoCrate;
   }/*end if crate does not respond*/

  /*---------------------
   * Error detected in crate controller
   */
   if (*lstat & MASK_rlst_err) {
      camacRecordError (ext, ERR_CTRL_ERR);
      return S_camacLib_SCC_Err;
   }/*end if crate controller detected error*/

  /*---------------------
   * No X response
   */
   if (*lstat & MASK_rlst_nx) {
      camacRecordError (ext, ERR_NO_X);
      return S_camacLib_noQ_noX;
   }/*end if no X response*/

  /*---------------------
   * Highway driver did not come ready
   */
   if (!(*lstat & MASK_rlst_rdy)) {
      camacRecordError (ext, ERR_NOT_READY);
      return S_camacLib_Not_Ready;
   }/*end if highway driver not ready*/

   return OK;

}/*end hw_errorRecovery()*/

/************************************************************************/
/* hw_waitInterrupt () -- Wait for highway ready interrupt		*/
/*	o Waits for "highway ready" interrupt or timeout		*/
/*	o Returns value of LAM status register after interrupt		*/
/*	o Note that this routine assumes that the branch hardware	*/
/*	  has been locked so that no more than one task at a time can	*/
/*	  call us.							*/
/*									*/
/************************************************************************/

LOCAL_RTN
uint16_t hw_waitInterrupt (ht2992Regs *pht2992, hwInfo *phwInfo)
{
   int  int_context;			/* Interrupt lock context	*/
   int  status;				/* Local status variable	*/

  /*---------------------
   * Set up to take an interrupt on highway ready
   */
   int_context = intLock ();		/* Disable interrupts		   */
   phwInfo->waiting = TRUE;		/* Set waiting flag		   */
   ht2992_wssm = MASK_rlst_rdy;		/* Enable highway-ready interrupt  */
  
  /*---------------------
   * Wait for interrupt service routine to signal the ready semaphore,
   * then return the value of the LAM status register as read by the
   * interrupt service routine.
   */
   status = semTake (phwInfo->ready, one_second);
   intUnlock (int_context);
   if (status == OK) return phwInfo->lstat;

  /*---------------------
   * If we timed out waiting for the ready interrupt, we must disable the
   * highway-ready interrupt now.
   */
   phwInfo->waiting = FALSE;			/* Clear waiting flag	     */
   ht2992_wscm = MASK_rlst_rdy;			/* Clear rdy bit in LAM mask */
   status = semTake (phwInfo->ready, NO_WAIT);	/* Flush ready semaphore     */

   /* Return LAM mask minus the "highway-ready" bit */
   return (ht2992_rlst & ~MASK_rlst_rdy);

}/*end hw_waitInterrupt()*/

/************************************************************************/
/* hw_waitReply () -- Wait for reply message from serial highway	*/
/*	o Busy-waits for "highway-ready"				*/
/*	o If busy-wait times out, waits for interrupt instead.		*/
/*	o Returns value of LAM status register after reply received	*/
/*									*/
/************************************************************************/

INLINE_RTN
uint16_t  hw_waitReply (ht2992Regs *pht2992, hwInfo *phwInfo)
{
   register uint16_t  lstat = 0;	/* Contents of LAM status reg	*/
   int                wait_count;	/* Counter for wait loop	*/

  /*---------------------
   * Wait for serial highway ready or timeout
   */
   wait_count = 0;
   for (lstat = ht2992_rlst; ((lstat & READY_MASK) != READY_MASK); lstat = ht2992_rlst) {
      if (++wait_count > phwInfo->timeout)
         return hw_waitInterrupt (pht2992, phwInfo);
   }/*end for*/

  /*---------------------
   * If no timeout, return the value of the LAM status register
   */
   return lstat;

}/*end hw_waitReply()*/

/************************************************************************/
/* hw_MADcmd () -- MAD Mode Command Function				*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_MADcmd (int f, double_word cnaf, int end_cnaf, ht2992Regs *pht2992,
                  hwInfo *phwInfo, int rptMax, int *rptCount)
{
   uint16_t   lstatus;		/* Copy of LAM status register	*/
   int        q;		/* Q response from last command	*/
   int        status;		/* Local status variable	*/

  /*---------------------
   * Repeat until we have either executed the number of commands specified by
   * rptMax, or we encounter an unrecoverable CAMAC error, or we exceed the
   * ending CNAF.
   */
   do /*while rptCount < rptMax*/ {

     /*---------------------
      * Start the command
      */
      ht2992_wscl = RESET_MASK;		/* Reset the LAM status reg.	*/
      ht2992_wnaf = cnaf.half.lo;	/* Load NAF (and start command)	*/

     /*---------------------
      * Wait for the command to complete and check its status
      */
      lstatus = hw_waitReply (pht2992, phwInfo);
      status  = hw_checkStatus (f, cnaf.whole, phwInfo, &q, &lstatus);
      if (status != OK) break;

     /*---------------------
      * If Q=1, increment the operation counter, and the subaddress
      */
      if (q) {
         *rptCount += 1;
         cnaf.whole += CONST_wnaf_a1;
      }/*end if Q=1*/

     /*---------------------
      * If Q=0, clear the subaddress and increment the slot.
      */
      else
         cnaf.whole = (~MASK_wnaf_a & cnaf.whole) + CONST_wnaf_n1;

     /*---------------------
      * Check to see if we have reached the end of the crate or the
      * ending CNAF.
      */
      if ((cnaf.half.lo > MAX_NAF) || (cnaf.whole > end_cnaf)) break;

   } while (*rptCount < rptMax);

  /*---------------------
   * Return the final status
   */
   return status;

}/*end hw_MADcmd()*/

/************************************************************************/
/* hw_MADread () -- MAD Mode Read Function				*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_MADread (int f, double_word cnaf, int end_cnaf, ht2992Regs *pht2992,
                   hwInfo *phwInfo, int *intc, int rptMax, int *rptCount,
                   int wordSize)
{
   int        done;		/* Flag for exit conditions	*/
   uint16_t   dummy;		/* Temp for dummy reads		*/
   uint16_t   lstatus;		/* Copy of LAM status register	*/
   int        q;		/* Q response from last command	*/
   int        status;		/* Local status variable	*/

  /*---------------------
   * Start the first read
   */
   ht2992_wscl = RESET_MASK;			/* Reset the LAM status reg. */
   ht2992_wnaf = cnaf.half.lo | MASK_wnaf_mad;	/* Start the read	     */

  /*---------------------
   * Repeat until we have either read the number of words specified by rptMax,
   * or we encounter an unrecoverable CAMAC error, or we exceed the specified
   * ending CNAF, or we run out of modules in the crate.
   */
   done = FALSE;
   do /*while not done*/ {

     /*---------------------
      * Wait for the previous read to complete and check its status
      */
      lstatus = hw_waitReply (pht2992, phwInfo);
      status  = hw_checkStatus (f, cnaf.whole, phwInfo, &q, &lstatus);
      /*if (status != OK) break;*/

     /*---------------------
      * If Q=1, increment the operation counter and the subaddress
      */
      if (q) {
         *rptCount += 1;
         cnaf.whole += CONST_wnaf_a1;
      }/*end if Q=1*/

     /*---------------------
      * If Q=0, clear the subaddress and increment the slot.
      */
      else
         cnaf.whole = (~MASK_wnaf_a & cnaf.whole) + CONST_wnaf_n1;

     /*---------------------
      * If this is the last word to transfer, load a dummy write command
      * into the "wnaf" register to stop the block transfer.
      */
      if ((*rptCount >= rptMax) || (cnaf.whole > end_cnaf) || (cnaf.half.lo > MAX_NAF)) {
         done = TRUE;
         ht2992_wnaf = DUMMY_WRITE;
      }/*end if last word*/

     /*---------------------
      * If Q=1, read the next word into the data array.  Otherwise do a dummy
      * read on the lo-order word to start the next transfer.
      */
      ht2992_wscl = RESET_MASK;			/* Reset the LAM status reg. */
      if (q) GET_DATA_WORD (intc, wordSize)	/* Fetch data word	     */
      else dummy = ht2992_rddb;			/* Restart read		     */

   } while (!done);

  /*---------------------
   * Return final status code
   */
   return status;

}/*end hw_MADread()*/

/************************************************************************/
/* hw_MADwrite () -- MAD Mode Write Function				*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_MADwrite (int f, double_word cnaf, int end_cnaf, ht2992Regs *pht2992,
                    hwInfo *phwInfo, int *intc, int rptMax, int *rptCount,
                    int wordSize)
{
   uint16_t   lstatus;		/* Copy of LAM status register	*/
   int        q;		/* Q response from last command	*/
   int        status;		/* Local status variable	*/

  /*---------------------
   * Repeat until we have either written the number of words specified by
   * rptMax, or we encounter an unrecoverable CAMAC error, or we exceed the
   * ending CNAF.
   */
   do /* while rptCount < rptMax */ {

     /*---------------------
      * Start the write
      */
      ht2992_wscl = RESET_MASK;		/* Reset the LAM status reg.	*/
      ht2992_wnaf = cnaf.half.lo;	/* Load NAF			*/
      PUT_DATA_WORD (intc, wordSize);	/* Load data word (starts cmd)	*/

     /*---------------------
      * Wait for the command to complete and check its status
      */
      lstatus = hw_waitReply (pht2992, phwInfo);
      status  = hw_checkStatus (f, cnaf.whole, phwInfo, &q, &lstatus);
      /*if (status != OK) break;*/

     /*---------------------
      * If Q=1, increment the operation counter, the data pointer, and
      * the subaddress
      */
      if (q) {
         *rptCount += 1;
         intc = (void *)((int)intc + wordSize);
         cnaf.whole += CONST_wnaf_a1;
      }/*end if Q=1*/

     /*---------------------
      * If Q=0, clear the subaddress and increment the slot.
      */
      else
         cnaf.whole = (~MASK_wnaf_a & cnaf.whole) + CONST_wnaf_n1;

     /*---------------------
      * Check to see if we have reached the end of the crate or the
      * ending CNAF.
      */
      if ((cnaf.half.lo > MAX_NAF) || (cnaf.whole > end_cnaf)) break;

   } while (*rptCount < rptMax);

  /*---------------------
   * Return the final status
   */
   return status;

}/*end hw_MADwrite()*/

/************************************************************************/
/* hw_UBCcmd () -- UBC Mode Command Function				*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_UBCcmd (int f, double_word cnaf, ht2992Regs *pht2992,
                  hwInfo *phwInfo, int rptMax, int *rptCount)
{
   uint16_t   lstatus;		/* Copy of LAM status register	*/
   int        q;		/* Q response from last command	*/
   int        status;		/* Local status variable	*/

  /*---------------------
   * Repeat until we have either executed the number of commands specified by
   * rptMax, or we encounter an unrecoverable CAMAC error.
   */
   FOREVER {

     /*---------------------
      * Start the command
      */
      ht2992_wscl = RESET_MASK;		/* Reset the LAM status reg.	*/
      ht2992_wnaf = cnaf.half.lo;	/* Load NAF (and start command)	*/

     /*---------------------
      * Wait for the command to complete and check its status
      */
      lstatus = hw_waitReply (pht2992, phwInfo);
      status  = hw_checkStatus (f, cnaf.whole, phwInfo, &q, &lstatus);
      if (!q || (status != OK)) return status;

     /*---------------------
      * If Q=1, increment the operation counter and check for end
      */
      if (++(*rptCount) >= rptMax) return status;

   }/*end forever loop*/

}/*end hw_UBCcmd()*/

/************************************************************************/
/* hw_UBCread () -- UBC Mode Read Function				*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_UBCread (int f, double_word cnaf, ht2992Regs *pht2992,
                   hwInfo *phwInfo, int *intc, int rptMax, int *rptCount,
                   int wordSize)
{
   int        done;		/* Flag for exit conditions	*/
   uint16_t   lstatus;		/* Copy of LAM status register	*/
   int        q;		/* Q response from last command	*/
   int        status;		/* Local status variable	*/

  /*---------------------
   * Start the first read
   */
   ht2992_wscl = RESET_MASK;			/* Reset the LAM status reg. */
   ht2992_wnaf = cnaf.half.lo | MASK_wnaf_mr;	/* Start the read	     */

  /*---------------------
   * Repeat until we have either read the number of words specified by rptMax,
   * or we encounter an unrecoverable CAMAC error.
   */
   done = FALSE;
   do /*while not done*/ {

     /*---------------------
      * Wait for the previous read to complete and check its status
      */
      lstatus = hw_waitReply (pht2992, phwInfo);
      status  = hw_checkStatus (f, cnaf.whole, phwInfo, &q, &lstatus);
      if (!q || (status != OK)) break;

     /*---------------------
      * If this is the last data word, load a dummy "write" command in the
      * wnaf register to stop the block mode transfers.
      */
      if (++(*rptCount) >= rptMax) {
         ht2992_wnaf = DUMMY_WRITE;	/* Load dummy write	*/
         done = TRUE;			/* Signal end of loop	*/
      }/*end if last data word*/

     /*---------------------
      * Get the next data word.
      */
      ht2992_wscl = RESET_MASK;		/* Reset LAM status register	*/
      GET_DATA_WORD (intc, wordSize);	/* Get next data word		*/

   } while (!done);

  /*---------------------
   * Return final status code
   */
   return status;

}/*end hw_UBCread()*/

/************************************************************************/
/* hw_UBCwrite () -- UBC Mode Write Function				*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_UBCwrite (int f, double_word cnaf, ht2992Regs *pht2992,
                    hwInfo *phwInfo, int *intc, int rptMax, int *rptCount,
                    int wordSize)
{
   uint16_t   lstatus;		/* Copy of LAM status register	*/
   int        q;		/* Q response from last command	*/
   int        status;		/* Local status variable	*/

  /*---------------------
   * Repeat until we have either written the number of words specified by
   * rptMax, or we encounter an unrecoverable CAMAC error.
   */
   FOREVER {

     /*---------------------
      * Start the write
      */
      ht2992_wscl = RESET_MASK;		/* Reset the LAM status reg.	*/
      ht2992_wnaf = cnaf.half.lo;	/* Load NAF			*/
      PUT_DATA_WORD (intc, wordSize);	/* Load data word (starts cmd)	*/

     /*---------------------
      * Wait for the command to complete and check its status
      */
      lstatus = hw_waitReply (pht2992, phwInfo);
      status  = hw_checkStatus (f, cnaf.whole, phwInfo, &q, &lstatus);
      if (!q || (status != OK)) return status;

     /*---------------------
      * Increment the operation counter and the data address.
      */
      if (++(*rptCount) >= rptMax) return status;
      intc = (void *)((int)intc + wordSize);

   }/*end forever loop*/

}/*end hw_UBCwrite()*/

/************************************************************************/
/* hw_UBRcmd () -- UBR Mode Command Function				*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_UBRcmd (int f, double_word cnaf, ht2992Regs *pht2992,
                  hwInfo *phwInfo, int rptMax, int *rptCount)
{
   uint16_t   lstatus;		/* Copy of LAM status register	*/
   int        q;		/* Q response from last command	*/
   int        retry;		/* Retry count for no-Q		*/
   int        status;		/* Local status variable	*/

  /*---------------------
   * Repeat until we have either executed the number of commands specified by
   * rptMax, or we encounter an unrecoverable CAMAC error, or we exceed the
   * retry count for no-Q.
   */
   retry = 0;
   FOREVER {

     /*---------------------
      * Start the command
      */
      ht2992_wscl = RESET_MASK;		/* Reset the LAM status reg.	*/
      ht2992_wnaf = cnaf.half.lo;	/* Load NAF (and start command)	*/

     /*---------------------
      * Wait for the command to complete and check its status
      */
      lstatus = hw_waitReply (pht2992, phwInfo);
      status  = hw_checkStatus (f, cnaf.whole, phwInfo, &q, &lstatus);
      if (status != OK) return status;

     /*---------------------
      * If Q=1, increment the operation counter, and reset the retry counter
      */
      if (q) {
         if (++(*rptCount) >= rptMax) return status;
         retry = 0;
      }/*end if Q=1*/

     /*---------------------
      * If Q=0, retry the operation until we exceed the retry count
      */
      else if (++retry > MAX_NOQ_RETRY) {
         camacRecordError (cnaf.whole, ERR_WAITQ_TIMEOUT);
         return S_camacLib_WaitQtimeout;
      }/*end if exceeded retry count*/

   }/*end forever loop*/

}/*end hw_UBRcmd()*/

/************************************************************************/
/* hw_UBRread () -- UBR Mode Read Function				*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_UBRread (int f, double_word cnaf, ht2992Regs *pht2992,
                   hwInfo *phwInfo, int *intc, int rptMax, int *rptCount,
                   int wordSize)
{
   int        done;		/* Flag for exit conditions	*/
   uint16_t   dummy;		/* Temp for dummy reads		*/
   uint16_t   lstatus;		/* Copy of LAM status register	*/
   int        q;		/* Q response from last command	*/
   int        retry;		/* Retry counter for no-Q	*/
   int        status;		/* Local status variable	*/

  /*---------------------
   * Start the first read
   */
   ht2992_wscl = RESET_MASK;			/* Reset the LAM status reg. */
   ht2992_wnaf = cnaf.half.lo | MASK_wnaf_mr;	/* Start the read	     */

  /*---------------------
   * Repeat until we have either read the number of words specified by rptMax,
   * or we encounter an unrecoverable CAMAC error, or we exceed the retry
   * count for no-Q
   */
   retry = 0;
   done = FALSE;
   do /*while not done*/ {

     /*---------------------
      * Wait for the previous read to complete and check its status
      */
      lstatus = hw_waitReply (pht2992, phwInfo);
      status  = hw_checkStatus (f, cnaf.whole, phwInfo, &q, &lstatus);
      if (status != OK) break;
      ht2992_wscl = RESET_MASK;		/* Reset LAM mask register	*/

     /*---------------------
      * If Q=1, reset the retry counter, increment the operation counter, and
      * get the next data word.
      */
      if (q) {

        /*---------------------
         * If this is the last data word, load a dummy "write" command in the
         * wnaf register to stop the block mode transfers.
         */
         if (++(*rptCount) >= rptMax) {
            ht2992_wnaf = DUMMY_WRITE;		/* Load dummy write	*/
            done = TRUE;			/* Signal end of loop	*/
	 }/*end if last data word*/

         retry = 0;				/* Reset retry count	*/
         GET_DATA_WORD (intc, wordSize);	/* Get next data word	*/

      }/*end if Q=1*/

     /*---------------------
      * If Q=0 increment the retry count. Abort if retry count exceeded.
      */
      else if (++retry > MAX_NOQ_RETRY) {
         camacRecordError (cnaf.whole, ERR_WAITQ_TIMEOUT);
         return S_camacLib_WaitQtimeout;
      }/*end if exceeded retry count*/

     /*---------------------
      * Repeat the read
      */
      else dummy = ht2992_rddb;

   } while (!done);

  /*---------------------
   * Return final status code
   */
   return status;

}/*end hw_UBRread()*/

/************************************************************************/
/* hw_UBRwrite () -- UBR Mode Write Function				*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_UBRwrite (int f, double_word cnaf, ht2992Regs *pht2992,
                    hwInfo *phwInfo, int *intc, int rptMax, int *rptCount,
                    int wordSize)
{
   uint16_t   lstatus;		/* Copy of LAM status register	*/
   int        q;		/* Q response from last command	*/
   int        retry;		/* Retry count for no-Q		*/
   int        status;		/* Local status variable	*/

  /*---------------------
   * Repeat until we have either written the number of words specified by
   * rptMax, or we encounter an unrecoverable CAMAC error, or we exceed the
   * retry count for no-Q.
   */
   retry = 0;
   FOREVER {

     /*---------------------
      * Start the write
      */
      ht2992_wscl = RESET_MASK;		/* Reset the LAM status reg.	*/
      ht2992_wnaf = cnaf.half.lo;	/* Load NAF			*/
      PUT_DATA_WORD (intc, wordSize);	/* Load data word (starts cmd)	*/

     /*---------------------
      * Wait for the command to complete and check its status
      */
      lstatus = hw_waitReply (pht2992, phwInfo);
      status  = hw_checkStatus (f, cnaf.whole, phwInfo, &q, &lstatus);
      if (status != OK) return status;

     /*---------------------
      * If Q=1, increment the operation counter, the data address, and reset
      * the retry counter.
      */
      if (q) {
         if (++(*rptCount) >= rptMax) return status;
         intc = (void *)((int)intc + wordSize);
         retry = 0;
      }/*end if Q=1*/

     /*---------------------
      * If Q=0, retry the operation until we exceed the retry count
      */
      else if (++retry > MAX_NOQ_RETRY) {
         camacRecordError (cnaf.whole, ERR_WAITQ_TIMEOUT);
         return S_camacLib_WaitQtimeout;
      }/*end if exceeded retry count*/

   }/*end forever loop*/

}/*end hw_UBRwrite()*/

/*======================================================================*/
/*		HT2992 Interrupt Service Routine			*/
/*									*/
/*======================================================================*/

LOCAL_RTN
void hw_camacInterrupt (hwInfo  *phwInfo)
{
   register uint16_t     lstat;		/* Contents of lam status register  */
   register ht2992Regs  *pht2992;	/* Pointer to register base address */
   int                   stat;		/* Local status variable	    */

   register union {			/* Local copy of demand status reg. */
     uint16_t        word;		/*    Whole word		    */
     rdst_register   reg;		/*    Register structure	    */
   } dmd;

  /*---------------------
   * Read the lam status register for this highway driver
   */
   pht2992 = phwInfo->prBase;		/* Get base register address	*/
   lstat   = ht2992_rlst;		/* Read the LAM status reg	*/

  /*---------------------
   * Check for serial-highway ready interrupt
   */
   if ((phwInfo->waiting) && (lstat & MASK_rlst_rdy)) {
      ht2992_wscm = MASK_rlst_rdy;	/* Clear ready bit in LAM mask	*/
      phwInfo->waiting = FALSE;		/* Clear waiting flag		*/
      phwInfo->lstat = lstat;		/* Save LAM status register	*/
      stat = semGive (phwInfo->ready);	/* Release waiting task		*/
   }/*end if branch ready interrupt*/

  /*---------------------
   * Check for demand message interrupt
   */
   if (lstat & MASK_rlst_dmd) {
      ht2992_wscl = MASK_rlst_dmd;	/* Clear dmd bit in LAM status	*/
      dmd.word = ht2992_rdst;		/* Read demand status register	*/
      camacLamInt (phwInfo->b,		/* Invoke the			*/
                   dmd.reg.crate,	/*   hardware-independent	*/
                   dmd.reg.sgl);	/*   LAM processing routine	*/
   }/*end if demand interrupt*/

  /*---------------------
   * Re-enable interrupts on the serial driver
   */
   ht2992_wien = MASK_rcsr_ie;

}/*end hw_camacInterrupt()*/

/************************************************************************/
/* hw_reboot () -- Reset hardware prior to reboot			*/
/*									*/
/************************************************************************/

LOCAL_RTN
void hw_reboot (int unused)
{
   int                   b;		/* Branch number		*/
   hwInfo               *phwInfo;	/* Pointer to hardware info	*/
   register ht2992Regs  *pht2992;	/* Pointer to register base	*/

  /*---------------------
   * Loop to search the hardware information array and disable interrupts
   * on every ht2992 card we find.
   */
   for (b=0; b < ht2992_num_cards; b++) {
      phwInfo = pht2992_info[b];
      if (phwInfo != NULL) {		/* Check for card present	  */
         pht2992 = phwInfo->prBase;	/* Get the register base address  */
         ht2992_wien = 0;		/* Disable interrupts		  */
      }/*end if hardware card found*/
   }/*end for each possible card*/

}/*end hw_reboot()*/

/************************************************************************/
/* hw_ccinit () -- Initialize hardware for branch or serial highway	*/
/*									*/
/************************************************************************/

INLINE_RTN
hwInfo  *hw_ccinit (int b)
{
   ht2992Regs           *base_addr;	/* Base addr for ht2992 cards	  */
   int                   int_context;	/* Context for interrupt ena/dis  */
   int                   int_vec;	/* Interrupt vector number	  */
   uint16_t              lstat;		/* Contents of LAM status reg.	  */
   uint16_t              ManID;		/* Manufacturer ID number	  */
   register ht2992Regs  *pht2992;	/* Base address for this branch	  */
   hwInfo               *phwInfo;	/* Ptr to hardware info structure */
   int                   timeout;	/* Timeout counter		  */
   register int          status;	/* Temporary status variable	  */

  /*---------------------
   * Get the number of clock ticks in one second (this is used to time-out
   * waits for highway-ready interrupts.
   */
   if (one_second == 0) one_second = sysClkRateGet ();

  /*---------------------
   * Allocate the array of hardware branch structures
   */
   if (pht2992_info == NULL) {

      /* Allocate the array */
      pht2992_info = (hwInfo **) calloc(ht2992_num_cards, sizeof *pht2992_info);

      /* Check for allocation error */
      if (pht2992_info == NULL) {
         logMsg ("%s: Routine hw_ccinit -- Unable to allocate memory for ht2992 hardware structure array.\n",
                (int)drvName, 0, 0, 0, 0, 0);
         return NULL;
      }/*end if could not allocate memory*/

      /* Add reboot hook to disable interrupts on each card in array */
      rebootHookAdd ((FUNCPTR)hw_reboot);

   }/*end if structure pointer array not allocated*/

  /*---------------------
   * Check for maximum number of hardware cards allowed
   */
   if (b >= ht2992_num_cards) return NULL;

  /*---------------------
   * Get base address for first ht2992 card
   */
   status = sysBusToLocalAdrs (
               VME_AM_SUP_SHORT_IO,	/* A16 space, supervisor mode	*/
               (char  *)ht2992_addrs,	/* Bus address for 1st ht2992	*/
               (char **)&base_addr);	/* Local address		*/
   if (status != OK) {
      logMsg ("%s: Routine hw_ccinit -- Unable to obtain A16 base address for ht2992 cards.\n",
              (int)drvName, 0, 0, 0, 0, 0);
      return NULL;
   }/*end if can't get base address*/

  /*---------------------
   * Compute and probe the register base address for this card
   */
   pht2992  = base_addr + b;
   status = vxMemProbe (
               (void *) &ht2992_rdid,	/* Probe manufacturer's ID	*/
               VX_READ, 		/* Probe for read access	*/
               sizeof(short), 		/* Probe A16 address		*/
               (void *) &ManID);	/* Save value read		*/
   if (status != OK) return NULL;

  /*---------------------
   * If the card is present, make sure it is a Hytec 2992
   */
   if ((ManID != HYTEC_ID) || (ht2992_rmod != VSD_2992)) {
      logMsg ("%s: Routine hw_ccinit -- Did not find ht2992 card where expected for branch %d\n",
             (int)drvName, b, 0, 0, 0, 0);
      return NULL;
   }/*end if wrong manufacturer or module id*/

  /*---------------------
   * If the card is present and correct, report that we found it.
   */
   logMsg ("%s: Initializing Hytec VSD2992 Serial Driver Card for Branch %d\n",
          (int)drvName, b, 0, 0, 0, 0);

  /*---------------------
   * Send a bogus write command to a bogus crate and slot (F16,C0,N0) and
   * time how long it takes to get around the serial highway.  This will
   * determine whether we should busy-wait for command completion, or take
   * an interrupt.
   */
   int_context = intLock ();		/* Lock out interrupts		*/
   ht2992_wups = MASK_wups_reset;	/* Reset non-control functions	*/
   ht2992_wclm = 0;			/* Clear LAM mask register	*/
   ht2992_wcll = 0;			/* Clear LAM status register	*/
   ht2992_wtca = 0;			/* Set crate = 0		*/
   ht2992_wnaf = 16;			/* Set NAF (N0, A0, F16)	*/
   ht2992_wtdt = 0;			/* Set data word = 0		*/
   ht2992_wtdb = 0;			/* Start the transfer		*/

   /* Wait for highway ready or timeout */
   for ((timeout=0, lstat=ht2992_rlst);
       ((timeout < MAX_BUSYWAIT) && !(lstat & MASK_rlst_rdy));
       (lstat = ht2992_rlst, timeout++));

   intUnlock (int_context);	/* Re-enable interrupts		*/

  /*---------------------
   * If the highway never came ready, or reported a timeout or synch error,
   * report that the highway is broken and quit.
   */
   if ((lstat & HIGHWAY_BROKEN_MASK) || !(lstat & MASK_rlst_rdy)) {
      logMsg ("%s: -- Serial highway on branch %d is broken, initialization terminated.\n",
             (int)drvName, b, 0, 0, 0, 0);
      return NULL;
    }/*end if serial highway broken*/

  /*---------------------
   * Set the timeout count to be twice the measured transmission time.
   * If the timeout is too big, however, set it to 1 (which is effectively
   * no busy wait)
   */
   if (timeout > BUSYWAIT_CUTOFF) timeout = 1;
   else timeout *= 2;

  /*---------------------
   * Allocate a hardware-specific information structure for this highway.
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
   phwInfo->prBase  = pht2992;		/* Pointer to register base address  */
   phwInfo->timeout = timeout;		/* Timeout for busy-waits	     */
   phwInfo->waiting = FALSE;		/* Not waiting for ready	     */
   phwInfo->b       = b;		/* Branch number		     */

   phwInfo->ready = semBCreate (	/* Create "branch ready" semaphore   */
                      SEM_Q_FIFO,	/*    Queue in FIFO order	     */
                      SEM_EMPTY);	/*    Initially empty		     */
   if (phwInfo->ready == NULL) {
      logMsg ("%s: -- Unable to create 'ready' semaphore for branch %d\n",
             (int)drvName, b, 0, 0, 0, 0);
      free (phwInfo);
      return NULL;
   }/*end if can't create semaphore*/

  /*---------------------
   * Connect to the interrupt service routine
   */ 
   int_vec = ht2992_ivec_base + b;
   status = intConnect (INUM_TO_IVEC (int_vec), hw_camacInterrupt, (int)phwInfo);
   if (status != OK) {
      logMsg ("%s: -- Unable to connect branch %d to interrupt service routine\n",
             (int)drvName, b, 0, 0, 0, 0);
      free (phwInfo);
      return NULL;
   }/*end if can't connect to interrupt service routine*/
   
  /*---------------------
   * Initialize the serial highway driver module
   */
   pht2992_info[b] = phwInfo;		/* Store address of hw info struct   */

   ht2992_wups = MASK_wups_reset;	/* Reset all non-control functions   */
   ht2992_wcll = 0;			/* Clear all LAMs & error flags	     */
   ht2992_wvec = int_vec;		/* Set the interrupt vector number   */
   ht2992_wlmk = MASK_rlst_dmd;		/* Set mask to take dmd interrupts   */
   ht2992_wien = MASK_rcsr_ie;		/* Enable interrupts for this card   */

   sysIntEnable (ht2992_irq_level);	/* Enable interrupts for this level  */

  /*---------------------
   * Report successful initialization and whether this is a fast or slow
   * highway.
   */
   logMsg ("%s: -- Serial Highway Branch %d Successfully Initialized\n",
          (int)drvName, b, 0, 0, 0, 0);

   if (phwInfo->timeout <= 1) {
      logMsg ("%s: -- Highway %d is a slow highway, will use interrupt service routine to signal command completion\n",
             (int)drvName, b, 0, 0, 0, 0);
   }
   else {
      logMsg ("%s: -- Highway %d is a fast highway, will use busy-loop to wait for command completion.\n",
             (int)drvName, b, 0, 0, 0, 0);
      logMsg ("%s: -- Maximum loop count is %d.\n",
             (int)drvName, phwInfo->timeout, 0, 0, 0, 0);
   }/*end else fast highway*/

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
   uint16_t              lstatus;	/* Local copy of LAM status reg	*/
   register ht2992Regs  *pht2992;	/* Pointer to register base	*/
   int                   status;	/* Local status variable	*/

  /*---------------------
   * Load the cnaf sequence
   */
   pht2992 = phwInfo->prBase;	/* Get register base			*/
   cnaf.whole  = ext | f;
   ht2992_wscl = RESET_MASK;	/* Clear relevant LAM status bits	*/
   ht2992_wtca = cnaf.half.hi;	/* Load the crate number		*/
   ht2992_wnaf = cnaf.half.lo;	/* Load the slot, subaddress & function	*/

  /*---------------------
   * If this is a write function, load the data
   */
   if (WRITE_F(f)) {
      ht2992_wtdt = *(short *)(data);	/* Load hi-order 8-bits		*/
      ht2992_wtdb = (short)(*data);	/* Load low-order 16-bits	*/
   }/*end if write function*/

  /*---------------------
   * Wait for operation to complete and check error status
   */
   lstatus = hw_waitReply (pht2992, phwInfo);
   status = hw_checkStatus (f, ext, phwInfo, q, &lstatus);

  /*---------------------
   * If this is a read function, get the data words
   */
   if (READ_F(f)) {
      if (status != OK) *data = 0;		/* No data if error	*/
      else {
         local_data.half.hi = LSB(ht2992_rddt);	/* Read hi-order byte	*/
         local_data.half.lo = ht2992_rddb;	/* Read lo-order word	*/
         *data = local_data.whole;		/* Return 24-bit data	*/
      }/*end if no error*/
   }/*end if read function*/

  /*---------------------
   * Return status
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
   double_word           cnaf;		/* Local copy of cnaf sequence	*/
   uint16_t              lstatus;	/* Local copy of LAM status reg	*/
   register ht2992Regs  *pht2992;	/* Pointer to register base	*/
   int                   status;	/* Local status variable	*/

  /*---------------------
   * Load the cnaf sequence
   */
   pht2992 = phwInfo->prBase;	/* Get register base			*/
   cnaf.whole  = ext | f;
   ht2992_wscl = RESET_MASK;	/* Clear relevant LAM status bits	*/
   ht2992_wtca = cnaf.half.hi;	/* Load the crate number		*/
   ht2992_wnaf = cnaf.half.lo;	/* Load the slot, subaddress & function	*/

  /*---------------------
   * If this is a write function, load the data
   */
   if (WRITE_F(f)) {
      ht2992_wtdb = *data;
   }/*end if write function*/

  /*---------------------
   * Wait for operation to complete and check error status
   */
   lstatus = hw_waitReply (pht2992, phwInfo);
   status = hw_checkStatus (f, ext, phwInfo, q, &lstatus);

  /*---------------------
   * If this is a read function, get the data words
   */
   if (READ_F(f)) {
      if (status != OK) *data = 0;	/* No data if error	*/
      else *data = ht2992_rddb;		/* Read lo-order word	*/
   }/*end if read function*/

  /*---------------------
   * Return status
   */
   return status;

}/*end hw_cssa()*/

/************************************************************************/
/* hw_cfmad() -- Address scan (24-bit)					*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS  hw_cfmad (int f, int extb[2], hwInfo *phwInfo, int *intc, int cb[4])
{
   int                   end_cnaf;	/* Ending CNAF value		*/
   register ht2992Regs  *pht2992;	/* Pointer to register base	*/
   double_word           start_cnaf;	/* Starting CNAF value		*/
   int                   status;	/* Local status variable	*/

  /*---------------------
   * Common initialization
   */
   start_cnaf.whole = extb[0] | f;	/* Construct the starting cnaf	*/
   end_cnaf         = extb[1] | f;	/* Construct the ending cnaf	*/

   pht2992     = phwInfo->prBase;	/* Get register base address	*/
   ht2992_wtca = start_cnaf.half.hi;	/* Load the crate number	*/

  /*---------------------
   * Execute the MAD function based on function type
   */
   if (READ_F(f))
      status = hw_MADread (f, start_cnaf, end_cnaf, pht2992, phwInfo, intc,
                           cb[0], &cb[1], sizeof(long int));
   else if (WRITE_F(f))
      status = hw_MADwrite (f, start_cnaf, end_cnaf, pht2992, phwInfo, intc,
                            cb[0], &cb[1], sizeof(long int));
   else
      status = hw_MADcmd (f, start_cnaf, end_cnaf, pht2992, phwInfo,
                          cb[0], &cb[1]);

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
   int                   end_cnaf;	/* Ending CNAF value		*/
   register ht2992Regs  *pht2992;	/* Pointer to register base	*/
   double_word           start_cnaf;	/* Starting CNAF value		*/
   int                   status;	/* Local status variable	*/

  /*---------------------
   * Common initialization
   */
   start_cnaf.whole = extb[0] | f;	/* Construct the starting cnaf	*/
   end_cnaf         = extb[1] | f;	/* Construct the ending cnaf	*/

   pht2992     = phwInfo->prBase;	/* Get register base address	*/
   ht2992_wtca = start_cnaf.half.hi;	/* Load the crate number	*/

  /*---------------------
   * Execute the MAD function based on function type
   */
   if (READ_F(f))
      status = hw_MADread (f, start_cnaf, end_cnaf, pht2992, phwInfo,
                          (int *)intc, cb[0], &cb[1], sizeof(short int));
   else if (WRITE_F(f))
      status = hw_MADwrite (f, start_cnaf, end_cnaf, pht2992, phwInfo,
                            (int *)intc, cb[0], &cb[1], sizeof(short int));
   else
      status = hw_MADcmd (f, start_cnaf, end_cnaf, pht2992, phwInfo,
                          cb[0], &cb[1]);

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
   double_word           cnaf;		/* CNAF value			*/
   register ht2992Regs  *pht2992;	/* Pointer to register base	*/
   int                   status;	/* Local status variable	*/

  /*---------------------
   * Common initialization
   */
   cnaf.whole  = ext | f;		/* Construct the cnaf		*/
   pht2992     = phwInfo->prBase;	/* Get register base address	*/
   ht2992_wtca = cnaf.half.hi;		/* Load the crate number	*/

  /*---------------------
   * Execute the UBC function based on function type
   */
   if (READ_F(f))
      status = hw_UBCread (f, cnaf, pht2992, phwInfo, (int *)intc,
                           cb[0], &cb[1], sizeof(long int));
   else if (WRITE_F(f))
      status = hw_UBCwrite (f, cnaf, pht2992, phwInfo, (int *)intc,
                            cb[0], &cb[1], sizeof(long int));
   else
      status = hw_UBCcmd (f, cnaf, pht2992, phwInfo, cb[0], &cb[1]);

  /*---------------------
   * Return completion status
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
   double_word           cnaf;		/* CNAF value			*/
   register ht2992Regs  *pht2992;	/* Pointer to register base	*/
   int                   status;	/* Local status variable	*/

  /*---------------------
   * Common initialization
   */
   cnaf.whole  = ext | f;		/* Construct the cnaf		*/
   pht2992     = phwInfo->prBase;	/* Get register base address	*/
   ht2992_wtca = cnaf.half.hi;		/* Load the crate number	*/

  /*---------------------
   * Execute the UBC function based on function type
   */
   if (READ_F(f))
      status = hw_UBCread (f, cnaf, pht2992, phwInfo, (int *)intc,
                           cb[0], &cb[1], sizeof(short int));
   else if (WRITE_F(f))
      status = hw_UBCwrite (f, cnaf, pht2992, phwInfo, (int *)intc,
                            cb[0], &cb[1], sizeof(short int));
   else
      status = hw_UBCcmd (f, cnaf, pht2992, phwInfo, cb[0], &cb[1]);

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
   double_word           cnaf;		/* CNAF value			*/
   register ht2992Regs  *pht2992;	/* Pointer to register base	*/
   int                   status;	/* Local status variable	*/

  /*---------------------
   * Common initialization
   */
   cnaf.whole  = ext | f;		/* Construct the cnaf		*/
   pht2992     = phwInfo->prBase;	/* Get register base address	*/
   ht2992_wtca = cnaf.half.hi;		/* Load the crate number	*/

  /*---------------------
   * Execute the UBR function based on function type
   */
   if (READ_F(f))
      status = hw_UBRread (f, cnaf, pht2992, phwInfo, (int *)intc,
                           cb[0], &cb[1], sizeof(long int));
   else if (WRITE_F(f))
      status = hw_UBRwrite (f, cnaf, pht2992, phwInfo, (int *)intc,
                            cb[0], &cb[1], sizeof(long int));
   else
      status = hw_UBRcmd (f, cnaf, pht2992, phwInfo, cb[0], &cb[1]);

  /*---------------------
   * Return completion status
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
   double_word           cnaf;		/* CNAF value			*/
   register ht2992Regs  *pht2992;	/* Pointer to register base	*/
   int                   status;	/* Local status variable	*/

  /*---------------------
   * Common initialization
   */
   cnaf.whole  = ext | f;		/* Construct the cnaf		*/
   pht2992     = phwInfo->prBase;	/* Get register base address	*/
   ht2992_wtca = cnaf.half.hi;		/* Load the crate number	*/

  /*---------------------
   * Execute the UBR function based on function type
   */
   if (READ_F(f))
      status = hw_UBRread (f, cnaf, pht2992, phwInfo, (int *)intc,
                           cb[0], &cb[1], sizeof(short int));
   else if (WRITE_F(f))
      status = hw_UBRwrite (f, cnaf, pht2992, phwInfo, (int *)intc,
                            cb[0], &cb[1], sizeof(short int));
   else
      status = hw_UBRcmd (f, cnaf, pht2992, phwInfo, cb[0], &cb[1]);

  /*---------------------
   * Return completion status
   */
   return status;

}/*end hw_csubr()*/
