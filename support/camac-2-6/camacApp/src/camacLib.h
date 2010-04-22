/******************************************************************************
 * camacLib.h -- Header file containing function prototypes, message codes,
 *               and macro definitions for the EPICS CAMAC drivers.
 *
 *-----------------------------------------------------------------------------
 * Authors:
 *	Eric Bjorklund and Rozelle Wright (LANL)
 *	Mark Rivers (University of Chicago)
 *
 *	Adapted from the work of Marty Wise (CEBAF) and
 *	Ying Wu (Duke University)
 *
 *-----------------------------------------------------------------------------
 * MODIFICATION LOG:
 * 
 * 09-17-94  bjo,rmw	Original Release 
 * 06-05-95  bjo	Added crateFuncBlock structure to support LAM graders
 * 03-12-96  mlr	Added lamParams structure to support user defined LAM
 *			functions.
 *			Implemented inta[] parameter to cdlam().
 *
 *****************************************************************************/

/******************************************************************************
 *			FUNCTION DESCRIPTIONS
 *                        o All functions return void
 *                        o Parameter definitions follow this section
 *
 *-----------------------------------------------------------------------------
 * Initialization and Declaration Functions:
 *
 * ccinit(int b) --
 *     Takes branch number (b) and performs initialization for branch.
 *
 * cdlam(int *lam, int b, int c, int n, int m, void *inta[]) --
 *    Define a CAMAC LAM variable to be used in calls to LAM-related
 *    CAMAC I/O routines.
 *
 * cdreg(int *ext, int b, int c, int n, int a) --
 *    Define a CAMAC channel variable. Takes CAMAC address data and creates
 *    variable used in other calls to CAMAC library routines.
 *
 * cglam(int lam, int *b, int *c, int *n, int *m, void *inta[]) --
 *    Takes CAMAC LAM variable and returns component address information
 *    (b, c, n, m, inta) in the appropriate buffers.
 *
 * cgreg(int ext, int *b, int *c, int *n, int *a) --
 *     Takes CAMAC channel variable and returns component address information
 *     (b, c, n, a) in the appropriate buffers.
 *
 *-----------------------------------------------------------------------------
 * Crate and LAM Signal Functions:
 *
 * cccc(int ext) --
 *     Crate clear. Takes CAMAC channel variable of crate controller and 
 *     issues Crate Clear (dataway C).
 *
 * cccd(int ext, int l) --
 *     Takes a CAMAC channel variable of a crate controller and sets or clears
 *     the crate demand enable flag in the controller as indicated by the value
 *     of l.
 *
 * ccci(int ext, int l) --
 *     Takes CAMAC channel variable of CAMAC crate controller and sets
 *     or clears dataway inhibit (I) as indicated by the logical value of l.
 *
 * cccz(int ext) --
 *     Crate initialize. Takes CAMAC channel variable of crate controler and
 *     issues crate initialize (dataway Z).
 *
 * cclc(int lam) --
 *	Clear LAM.  Clears the LAM signal associated with the specified LAM
 *      variable.
 *
 * cclm(int lam, int l) --
 *     Enable/disable LAM.  Takes LAM identifier and either enables or disables
 *     the LAM, depending on the value of l (TRUE = enable, FALSE = disable).
 *
 * cclnk(int lam, FUNCPTR rtn) --
 *	Connect the routine specified by rtn to the specified LAM signal.
 *
 *-----------------------------------------------------------------------------
 * CAMAC I/O Functions (single action):
 *
 * cfsa(int f, int ext, int *dat, int *q) --
 *    Takes Function code, CAMAC channel variable, data and status (Q) buffer.
 *    Performs 24 bit single action I/O as indicated by F and returns data in
 *    data buffer and Q in Q buffer.
 *
 * cssa(int f, int ext, short *dat, int *q) --
 *    Takes Function code, CAMAC channel variable, data and status (Q) buffer.
 *    Performs 16 bit single action I/O as indicated by F and returns data in
 *    data buffer and Q in Q buffer.
 *
 *-----------------------------------------------------------------------------
 * CAMAC I/O Functions (multiple action):
 *
 * cfga(int fa[], int exta[], int intc[], int qa[], int cb[4]) --
 *     Takes an array of all parameters to cfsa, and performs a series of 
 *     single action operations, returning data and Q in the appropriate
 *     buffers.  The size of the arrays is given by the first element of
 *     cb.  The second element of cb will contain the actual number of
 *     operations performed.
 *
 * csga(int fa[], int exta[], short intc[], int qa[], int cb[4]) --
 *     As above, but operates on short (16 bit) data.
 *
 * cfmad(int f, int extb[2], int *intc, int cb[4]) --
 *     CAMAC address scan. Takes function code and an array of two CAMAC 
 *     channel variables, a pointer to a data buffer, and a control block 
 *     containing as its first element, the maximum number of I/O transactions
 *     to be performed. The given function code is applied to the first
 *     channel address. The address is incremented per the IEEE standard
 *     algorithm for address scan:
 *       o If Q=1, increment the subaddress. If the subaddress exceeds 15, set
 *         it to 0 and increment the slot number.
 *       o If Q=0, increment the slot number and set the subaddress to 0.
 *     This is repeated until either a) a non-recoverable error occurs, b)
 *     the CAMAC channel value equals the ending value specified in the second
 *     element of the extb array, or the maximum number of transactions (as
 *     indicated in cb[0] is performed. The actual number of operations
 *     performed is returned in cb[1].
 *
 * csmad(int f, int extb[2], short *intc, int cb[4]) --
 *     Short (16 bit) address scan. Same as cfmad described above, except 
 *     data is 16 bit.
 *
 * cfubc(int f, int ext, int *intc, int cb[4]) --
 *     Takes a function code, a CAMAC channel variable, a pointer to a data 
 *     buffer and a control block. Repeatedly applies the function code to the
 *     given CAMAC channel until a No-Q response is received or until the
 *     maximum number of operations specified by cb[0] is reached.  The actual
 *     number of operations performed is returned in cb[1].
 *
 * csubc(int f, int ext, short intc[], int cb[4]) --
 *     Same as cfubc above, but operates on short (16 bit) data.
 *
 * cfubr(int f, int ext, int intc[], int cb[4]) --
 *     Takes a function code, channel variable, pointer to a data buffer and a
 *     control block. Repeatedly performs the operation on the address until Q
 *     is returned, then increments the data pointer and repeats. The
 *     repetition continues until the number of operations given in cb[0] is
 *     performed. Each operation is performed a maximum number of times before
 *     it is aborted. The maximum number if times a particular operation is
 *     retried before abort can be defined on a per-site basis (see header
 *     comments for "camacLib.c"  The default is 100. 
 *
 * csubr(int f, int ext, short intc[], int cb[4]) --
 *     Same as cfubr above, but operates on short (16 bit) data.
 *
 *-----------------------------------------------------------------------------
 * Test and Status Functions:
 *
 * ctcd(int ext, int *l) --
 *     Takes a CAMAC channel variable, tests the state of the crate's demand
 *     enable flag, and returns the result in *l.
 *
 * ctci(int ext, int *l) --
 *     Takes CAMAC channel variable, tests the state of the crate's dataway
 *     inhibit (I) line, and returns the result in *l.
 *
 * ctgl (int ext, int *l) --
 *     Takes CAMAC channel variable, tests for the presence of any LAM signal
 *     on the specified crate, and returns the result in *l.
 *
 * ctlm (int lam, int *l) --
 *     Takes CAMAC LAM variable and tests for presence of LAM signal.  Returns
 *     the result in *l.
 *
 * ctstat(int *k) --
 *     Returns the Q, X, and status values from the last CAMAC operation.
 *     The Q and X responses are coded into the low-order two bits of the
 *     status code and can be extracted with the Q_STATUS, and X_STATUS
 *     macros defined in this header.  NOTE: The EPICS CAMAC drivers use
 *     errno to store the status code.  Therefore the call to ctsat (or
 *     Q_STATUS and X_STATUS) should come shortly after the call to the
 *     CAMAC routine -- before calling any other library routines which might
 *     reset errno.
 *
 *-----------------------------------------------------------------------------
 * Non-Esone Initialization Routines
 *
 * camacDeclareInitRtn (camacInitRtn *initRtn, int b, int c, int n) --
 *	Declares an external (to the CAMAC driver) initialization routine for
 *	a crate controller or LAM grader.
 *
 * camacRegisterCard (int b, int c, int n, char *name,
 *                    camacCardInitRtn *initRtn, int parm) --
 *	Registers the name of a card and an initialization routine to be
 *	called if the crate is re-initialized.
 *
 *-----------------------------------------------------------------------------
 * Other non-ESONE Routines (status returned as function value)
 *
 * camacLibInit()
 *	Initialized the CAMAC library.  This function must be called before any
 *	of the other functions in this package are called.
 *	If the library is built with EPICS support enabled then this function
 *	will be called automatically by iocInit.  If the library is built
 *	without EPICS support then this function must be called explicitly,
 *	typically from the vxWorks startup command file.
 *
 * camacLockBranch (int ext) --
 *	Locks a CAMAC branch or serial highway for exclusive access.  The
 *	The branch specified by the "ext" parameter is locked for exclusive
 *	access.  If no hardware exists for this branch, the routine returns
 *	an error status.  This routine can be used when a sequence of actions
 *	needs to be atomic, but can not be incorporated into any of the ESONE
 *	multiple-action or block-mode routines (for example, a read/mask/write
 *	sequence for multi-bit binary output).
 *
 * camacUnlockBranch (int ext) --
 *	Unlocks the CAMAC branch or serial highway specified in the "ext"
 *	parameter.  If no hardware exists for the specified branch, the
 *	routine exits with an error status.
 *
 *****************************************************************************/

/******************************************************************************
 *				PARAMETER DESCRIPTIONS
 *
 *
 * Name		Type	Description
 * ----		----	-------------------------------------------------------
 *
 * a		int	CAMAC subaddress.  Must be in the range 0-15.
 *
 * b		int	Branch number for serial or branch highway systems.
 *			For parallel systems, the branch and crate number
 *			are usually the same.  The branch number identifies
 *			which CAMAC interface card in the IOC we are talking to
 *
 * c		int	Crate number.  For serial systems, must be in the
 *			range 1-62.  For branch highway systems, must be in
 *			the range 1-7.
 *
 * cb[4]	int	Control block used by multiple-action routines.  The
 *	       array	contents of cb are described below:
 *			cb[0] = Maximum number of operations to perform.
 *			cb[1] = Number of operations actually performed (this
 *				field is written to by the driver).
 *			cb[2] = LAM variable.  If cb[2] is non-zero, it is
 *				interpreted as a LAM variable to wait on
 *				before begining the operation.
 *			cb[3] = The ESONE standard describes this field as
 *				implementation-dependant. In this implementation
 *				the cb[3] field is used to contain a timeout
 *				(in milliseconds) on how long we will wait for
 *				the LAM specified in cb[2].  If the value of
 *				cb[3] is 0 or WAIT_FOREVER, the wait-for-LAM
 *				will not timeout.  If the wait-for-LAM does
 *				timeout, the operation will not be performed.
 *				An error status of S_camacLib_LAM_Timeout will
 *				be returned by ctstat. Note that the value of
 *				cb[3] will be rounded up to the nearest clock
 *				tick.
 *
 * dat		int or	CAMAC data word.  May be either 16 or 24 bits long
 *		short	depending on which routine is called.  Note that in
 *			this implementation, 24-bit words are NOT sign-extended
 *			to 32-bits.
 *
 * ext		int	CAMAC channel variable.  Created by "cdreg".  Contains
 *			a driver-specific encoding of the branch, crate, slot,
 *			and subaddress values.
 * 
 * exta[]	int	Array of CAMAC channel variables used in call to cfga
 *	       array	and csga routines.  Each element of exta has the same
 *			format as the "ext" variable described above.  The size
 *                      of the array is given by the value of cb[0].	
 *
 * extb[2]	int	Array of CAMAC channel variables used in call to cfmad
 *	       array	and csmad routines.  The first element (extb[0])
 *			contains the starting channel and the second element
 *			contains the ending channel.  The starting channel
 *			must be less than or equal to the ending channel as
 *			defined by the ordering; branch, crate, slot, subaddr.
 *
 * f		int	CAMAC function code.  Must be in the range 0-31.
 *
 * fa[]		int	Array of CAMAC function codes.  Used in call to cfga
 *	       array	and csga routines.  The size of the array is given by
 *			the value of cb[0].	
 *
 * inta[]	void*	Array containing implementation-dependent
 *		array	information used by cdlam to define a LAM variable.
 *			In this implementation inta[] is defined as follows:
 *			  - If inta = NULL then nothing special is done
 *			  - If inta != NULL then inta[] must be defined as
 *			    follows:
 *			inta[0] = pointer to structure of type lamParams
 *			          which defines the F and A values for 
 *			          processing LAMS on non-standard modules.
 *				  If only standard processing is to be used 
 *				  then inta[0] should be set to NULL.
 *			inta[1] = pointer which will passed to routines
 *			          called with cclnk() for this LAM. 
 *				  If no parameter should be passed then 
 *				  inta[1] should be set to NULL.
 *
 * intc[]	int or	Array of CAMAC data words.  Each element may be either
 *		short	16 (short) or 24 (int) bits long depending on which
 *		array	routine is called.  The size of the array is given by
 *			the value of cb[0].
 *		
 * k		int	Status of last operation.  The low-order bit of this
 *			word is the complement of the Q status.  The next bit
 *			is the complement of the X status.  The entire word
 *			may be interpreted as an overall status code (e.g. by
 *			printErrno) where 0 (OK) indicates no errors.  This
 *			violates the ESONE standard slightly (the status code
 *			should be extracted via a "k >> 2" operation).  It is,
 *			however, not possible to comply simultaneously with
 *			both ESONE, UNIX, and EPICS status code conventions.
 *			For more information on status codes, see the
 *			description of the ctstat routine above, and the
 *			message code section below.
 *
 * l		int	Boolean value returned by test routines.  Takes the
 *			values TRUE or FALSE.
 *
 * lam		int	LAM identifier variable.  Created by cdlam.  Contains
 *			implementation-specific encoding of the branch, crate,
 *			and slot, plus information on whether it is accessed
 *			via dataless functions at a subaddress or via read/
 *			write functions in group 2 registers.
 *
 * m		int	LAM access specifier.  Used in calls to cdlam.
 *			Specifies the mode of access of a LAM and the lowest
 *			order address component for LAM addressing.  If m is
 *			zero or positive, it is interpreted as a subaddress
 *			and the LAM is assumed to be accessed via dataless
 *			functions at this subaddress.  If m is negative, it is
 *			interpreted as the negative of a bit position for a
 *			LAM which is accessed via reading, setting, or clearing
 *			bits in the group2 registers or subadresses 12, 13, or
 *			14.
 *
 * n		int	CAMAC slot number.  Must be in the range 1-23.  For
 *			serial CAMAC systems, n=30 may be used to address the
 *			serial crate controller.
 *
 * q		int	Q response from command.  Returns either 0 or 1.
 *
 * qa[]		int	Array of Q responses used in call to cfga or csga
 *	       array	routines.  The size of the array is given by the value
 *			of cb[0].	
 *
 * rtn()      FUNCPTR   Address of a function which will be called whenever the
 *			associated LAM occurs.  This function will be called
 *			with a single optional parameter, which is a pointer 
 *			specified in inta[1], described above.
 *	
 *****************************************************************************/

/************************************************************************/
/*  Header Files							*/
/************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef EPICS
#include "errMdef.h"		/* EPICS Module Code definitions	*/
#endif

#ifndef vxWorks
typedef int (*FUNCPTR) ();     /* ptr to function returning int */
#endif

/************************************************************************/
/*  Type Declarations							*/
/************************************************************************/

typedef struct crateFuncBlock  crateFuncBlock;
typedef struct lamParams  lamParams;

/*---------------------
 * Define types for function addresses that are stored in the crateFuncBlock
 * structure.
 */
typedef void   camacInitRtn  (crateFuncBlock *pfunc, int b, int c, int n);
typedef void   camacSlotLam  (crateFuncBlock *pfunc, short int n);
typedef void   camacCrateLam (crateFuncBlock *pfunc);

/*---------------------
 * Define function type for card-specific slot initialization routines
 */
typedef void   camacCardInitRtn ();

/************************************************************************/
/*  Function Prototypes							*/
/************************************************************************/

void  ccinit (int b);
void  cdlam  (int *lam, int b, int c, int n, int m, void* inta[]);
void  cdreg  (int *ext, int b, int c, int n, int a);
void  cglam  (int lam, int *b, int *c, int *n, int *m, void* inta[]);
void  cgreg  (int ext, int *b, int *c, int *n, int *a);

void  cccc   (int ext);
void  cccd   (int ext, int l);
void  ccci   (int ext, int l);
void  cccz   (int ext);
void  cclc   (int lam);
void  cclm   (int lam, int l);
void  cclnk  (int lam, FUNCPTR rtn);

void  cfsa   (int f, int ext, int *dat, int *q);
void  cssa   (int f, int ext, short *dat, int *q);

void  cfga   (int fa[], int exta[], int intc[], int qa[], int cb[4]);
void  csga   (int fa[], int exta[], short intc[], int qa[], int cb[4]);
void  cfmad  (int f, int extb[2], int intc[], int cb[4]);
void  csmad  (int f, int extb[2], short intc[], int cb[4]);
void  cfubc  (int f, int ext, int intc[], int cb[4]);
void  csubc  (int f, int ext, short intc[], int cb[4]);
void  cfubr  (int f, int ext, int intc[], int cb[4]);
void  csubr  (int f, int ext, short intc[], int cb[4]);

void  ctcd   (int ext, int *l);
void  ctci   (int ext, int *l);
void  ctgl   (int ext, int *l);
void  ctlm   (int lam, int *l);
void  ctstat (int *k);

int   camacLockBranch (int ext);
int   camacUnlockBranch (int ext);

void  camacDeclareInitRtn (camacInitRtn *initRtn, int b, int c, int n);
void  camacRegisterCard (int b, int c, int n, char *name,
                         camacCardInitRtn *initRtn, int parm);

/************************************************************************/
/*		crateFuncBlock Structure Definition			*/
/*									*/
/* The crateFuncBlock structure is a part of the CAMAC driver's crate	*/
/* structure.  It contains CAMAC channel variables, function addresses,	*/
/* and other data useful to hardware implementation modules and		*/
/* external crate controller and lam grader implementation routines.	*/
/*									*/
/************************************************************************/

struct crateFuncBlock {

  /*---------------------
   * Crate Controller Channel Variables
   */
   int             cc_status;		/* Read/Write crate status register  */
   int             cc_reread;		/* Re-read last data (serial highway)*/
   int             cc_lamPattern;	/* Read crate's LAM pattern register */
   int             cc_lamMask;		/* Read/Write crate's LAM mask reg.  */
   int             cc_lamPresent;	/* Test for LAM present at this crate*/
   int             cc_demand;		/* Ena/Dis demands from this crate   */
   int             cc_inhibit;		/* Set/Clear crate inhibit	     */
   int             cc_setC;		/* Generate dataway C signal	     */
   int             cc_setZ;		/* Generate dataway Z signal	     */
   int             cc_selectMask;	/* Write station select mask (SNR)   */

  /*---------------------
   * LAM-Grader Channel Variables
   */
   int             lg_demand;		/* Ena/Dis demands at LAM grader     */
   int             lg_lamMaskClr;	/* Clear bit in LAM mask register    */
   int             lg_lamMaskSet;	/* Set bit in LAM mask register	     */
   int             lg_hungDemand;	/* Reset hung demand flag	     */

  /*---------------------
   * External LAM Grader and/or Crate Controller Functions
   */
   camacSlotLam   *disableSlotLam;	/* Disable LAM on selected slot	     */
   camacSlotLam   *enableSlotLam;	/* Enable LAM on selected slot	     */
   camacSlotLam   *ackSlotLam;		/* Acknowledge receipt of LAM	     */

   camacCrateLam  *disableGrader;	/* Disable LAM grader		     */
   camacCrateLam  *enableGrader;	/* Enable LAM grader		     */
   camacCrateLam  *resetCrateLam;	/* Reset crate LAM after interrupt   */

  /*---------------------
   * Miscellaneous Useful Information
   */
   int            lam_mask;		/* Mask of which LAM's are enabled   */
   void           *hwPvt;		/* For hardware module's private use */
};

/************************************************************************/
/*		lamParams Structure Definition			        */
/*									*/
/* the lamParams structure is used to define the A and F values to      */
/* handle LAMs from modules which do not strictly adhere to the ESONE	*/
/* standard.  It contains fields which specify the A, F and Mask values */
/* to clear, test, disable, and enable LAMs from the module.            */
/*									*/
/* If f_test is for a CAMAC Read function (F=0-7) then			*/
/* mask_test is "anded" with the value read with (f_test,a_test), and   */
/* if the result is non-zero then a LAM is assumed to be present.       */
/* If f_test is not for a CAMAC read funtion, then the Q response of 	*/
/* the module is assumed to encode the LAM status, Q=1 meaning a LAM 	*/
/* is present.               						*/
/*									*/
/* mask_clear, mask_enable, mask_disable are data values which are      */
/* written to the module when those functions are performed.            */
/*									*/
/* The address of a lamParams structure should be passed as the first   */
/* element of inta[] to function cdlam() if the module requires         */
/* non-standard F and A values to process LAMs.                         */
/*									*/
/* If the module does not support one or more of the "test", "clear",   */
/* "enable" or "disable" functions, then the F values for that function */
/* should be set to -1.                                                 */
/*									*/
/************************************************************************/

struct lamParams {
   int             a_test;		/* "A" to test LAM              */
   int             f_test;		/* "F" to test LAM              */
   int             mask_test;		/* Mask to test LAM             */
   int             a_clear;		/* "A" to clear LAM             */
   int             f_clear;		/* "F" to clear LAM             */
   int             mask_clear;		/* Mask to clear LAM            */
   int             a_enable;		/* "A" to enable LAM            */
   int             f_enable;		/* "F" to enable LAM            */
   int             mask_enable;		/* Mask to enable LAM           */
   int             a_disable;		/* "A" to disable LAM           */
   int             f_disable;		/* "F" to disable LAM           */
   int             mask_disable;	/* Mask to disable LAM          */
};

/************************************************************************
 *  CAMAC Error Status Codes
 *
 * NOTE:  The ESONE convention for status codes specifies that the
 * low-order 2-bits of the code reflect the Q and X status of the
 * request as follows:
 *	00 = Both Q and X present
 *	01 = X present, but not Q
 *	10 = Q present, but not X
 *	11 = Neither Q nor X present.
 *
 * The following status code definitions reflect this convention by
 * always setting the two low-order bits on most of the error codes
 * (this implies that no Q or X are returned if an operation aborts).
 *
 * Note that this convention results in a small violation of the EPICS
 * standard in that the "No X" condition is an even number, which is
 * a success code in EPICS.
 ************************************************************************/

/*---------------------
 * Assign a default value to M_camacLib if it is not in errMdef.h
 */
#ifndef M_camacLib
#define M_camacLib  (600 << 16)
#endif

#define	S_camacLib_noQ		(M_camacLib |   1)	/* No Q response */
#define	S_camacLib_noX		(M_camacLib |   2)	/* No X response */
#define	S_camacLib_noQ_noX	(M_camacLib |   3)	/* No Q or X response */
#define	S_camacLib_Bad_B	(M_camacLib |   7)	/* Invalid branch number */
#define S_camacLib_Bad_C	(M_camacLib |  11)	/* Invalid crate number */
#define S_camacLib_Bad_N	(M_camacLib |  15)	/* Invalid slot number */
#define	S_camacLib_Bad_A	(M_camacLib |  19)	/* Invalid subaddress */
#define	S_camacLib_Bad_F	(M_camacLib |  23)	/* Invalid function code */
#define	S_camacLib_Bad_Var	(M_camacLib |  27)	/* Invalid CAMAC channel variable */
#define S_camacLib_Bad_LAM	(M_camacLib |  31)	/* Invalid CAMAC LAM variable */
#define	S_camacLib_Bad_Timeout	(M_camacLib |  35)	/* Invalid timeout value */
#define	S_camacLib_No_Hw	(M_camacLib |  39)	/* No active hardware for this branch */
#define	S_camacLib_Not_Ready	(M_camacLib |  43)	/* Highway driver is not ready */
#define	S_camacLib_Hw_Timeout	(M_camacLib |  47)	/* Hardware timeout */
#define S_camacLib_LAM_Timeout	(M_camacLib |  51)	/* Timeout waiting for LAM */
#define	S_camacLib_Long_Parity	(M_camacLib |  55)	/* Longitudinal parity error */
#define S_camacLib_Trans_Parity	(M_camacLib |  59)	/* Transverse parity error */
#define S_camacLib_Offline	(M_camacLib |  63)	/* Crate is off-line */
#define S_camacLib_Bypass	(M_camacLib |  67)	/* Crate is bypassed */
#define	S_camacLib_NoCrate	(M_camacLib |  71)	/* Crate does not respond */
#define	S_camacLib_Lost_Synch	(M_camacLib |  75)	/* Lost serial highway synch */
#define	S_camacLib_SCC_Err	(M_camacLib |  79)	/* Error detected by serial crate controller */
#define	S_camacLib_BadAddrSpec	(M_camacLib |  83)	/* Ending address is before starting address */
#define	S_camacLib_MultiBranchG	(M_camacLib |  87)	/* Multiple branches not supported for GA functions. */
#define	S_camacLib_MultiBranchM	(M_camacLib |  91)	/* Multiple branches not supported for MAD functions. */
#define	S_camacLib_MultiCrate	(M_camacLib |  95)	/* Multiple crates not supported for MAD functions. */
#define	S_camacLib_BadRptCnt	(M_camacLib |  99)	/* Repeat count is less than or equal to zero. */
#define	S_camacLib_WaitQtimeout	(M_camacLib | 103)	/* Timeout waiting for Q response */
#define	S_camacLib_NotImpl	(M_camacLib | 107)	/* Function not implemented by this module. */

/************************************************************************/
/*  Macro Definitions							*/
/************************************************************************/

/*====================
 * Return Q result of last operation (extracted from errno)
 */
#define LAST_Q ((errno & 0xffff0000) == M_camacLib ? 1-(errno & 1) : 1)


/*====================
 * Return X result of last operation (extracted from errno)
 */
#define LAST_X	((errno & 0xffff0000) == M_camacLib ? 1-((errno & 2) >> 1) : 1)


/*====================
 * Extract the Q response from the status variable returned by ctstat.
 */
#define Q_STATUS(status) (1 - (status & 1))


/*====================
 * Extract the X response from the status variable returned by ctstat.
 */
#define X_STATUS(status) (1 - ((status & 2)>>1))


/*=====================
 * The following "documentation" macros are included to help identify elements
 * of a CAMAC address by "tagging" them with their element name.  For example,
 * the following call to cdreg: "cdreg (&ext, 0, 2, 13, 0);" could also be
 * coded as: "cdreg (&ext,B(0),C(2),N(13),A(0));" for greater clarity.
 */
#define B(b) (b)		/* Branch number		*/
#define	C(c) (c)		/* Crate number			*/
#define	N(n) (n)		/* Slot number			*/
#define	A(a) (a)		/* Sub-Address			*/
#define	F(f) (f)		/* Function code		*/

/*=====================
 * The following definitions are useful in record and device support for keeping
 * track of which mode is which.
 */
#define TMOD_SNGL 0	/* Single-cycle transfer */
#define TMOD_QSTP 1	/* Q-stop transfer	 */
#define TMOD_QRPT 2	/* Q-repeat transfer	 */
#define TMOD_QSCN 3	/* Q-scan transfer	 */

#ifdef __cplusplus
}
#endif
