/******************************************************************************
 * camacLib.c -- EPICS ESONE-Compliant CAMAC Driver Library for VxWorks
 *
 *-----------------------------------------------------------------------------
 * AUTHORS:
 *	Eric Bjorklund and Rozelle Wright (LANL)
 *	Mark Rivers (University of Chicago)
 *
 *	Adapted from the work of Marty Wise (CEBAF) and
 *	Ying Wu (Duke University)
 *
 *-----------------------------------------------------------------------------
 * MODIFICATION LOG:
 *
 * 09-17-94  bjo,rmw	Initial Release
 * 04-28-95  mr		Added support for OTHER highway types
 * 06-05-95  bjo	Added support for LAM graders and error logging
 * 03-12-96  mlr	Improved LAM routines
 *                      Added #ifdefs to allow operation without EPICS
 * 05-08-00  mlr	Fixed format problem
 * 09-13-00  mlr	Fixed logical comparison problem.  Increased size of error
 *                      monitor stack, was overflowing on MV2700.
 * 02-08-01  mlr	Changed priority of lamMonitor and camErrMon tasks from
 *                      10 and 30 to 55 and 60. They must have lower priority than
 *                      tNetTask.
 *
 *-----------------------------------------------------------------------------
 * MODULE DESCRIPTION:
 *
 * This module contains the device-independent code for the EPICS VxWorks
 * ESONE-compliant CAMAC drivers.  It contains:
 *   o	An implementation of the ESONE CAMAC subroutine library routines
 *   o	Standard EPICS device driver linkage routines and structures.
 *   o	Routines for handling LAM interrupts.
 *
 * To create a functioning CAMAC driver, you must compile this module with
 * an appropriate hardware definition header file (see the definition of the
 * HW_HEADER macro below).
 *
 *-----------------------------------------------------------------------------
 * REFERENCES:
 *
 * IEEE Std 595-1976  "IEEE Standard Serial Highway Interface System (CAMAC)"
 *
 * IEEE Std 596-1976  "IEEE Standard Parallel Highway Interface System (CAMAC)"
 *
 * DOE/EV-0006        "Recommendations for CAMAC Serial Highway Drivers and LAM
 *                     Graders for the SCC-L2 Serial Crate Controller"
 *
 * DOE/EV-0016        "Subroutines for CAMAC"
 *
 *-----------------------------------------------------------------------------
 * CONFIGURATION MACROS:
 *
 * The following macros may be defined at compile time through the -D option
 * of the GNU C compiler.  They allow a certain amount of site-specific
 * configuration (tweeking) based on your own site's hardware and needs.  Of
 * the macros listed below, only HW_HEADER is required to produce a functioning
 * CAMAC driver.  The others all have pre-defined default values.
 *
 * The configuration macros are:
 *
 * HW_HEADER  =	Name of hardware definition module.  This macro is used
 *		to include the appropriate hardware definition file.  Its
 *		declaration should be of the form:
 *			 -DHW_HEADER="<ht2992.h>"
 *		The above example will build a driver for the Hytec VSD2992
 *		serial highway driver module.
 *
 * DEBUG      =	If this macro is defined, then a "development/debug" version
 *		of the driver will be compiled.  This macro disables the
 *		"inline" declaration, and forces all routines to be global.
 *		The effect is to make all routines, even "in-line" routines,
 *		callable from the vxWorks shell.  It also enables a broader
 *		range of error reporting.  Errors which would normally only
 *		be reported through status codes (such as invalid function
 *		codes) will also be displayed on the IOC terminal or logging
 *		device.  This option is useful for developing new drivers or
 *		device support routines.
 *
 * ERROR_DECAY=	This macro sets the exponential decay rate for the "Current
 *		Rate" fields in the driver status reports.  The value should
 *		be between 0.5 and 1.0.  At the default value of 0.6, a single
 *		CAMAC error will decay out of the "Current Rate" field after
 *		10 seconds (assuming there are no further occurrences of that
 *		error).  Larger error counts will decay at the rate of one
 *		decade every 5 seconds.  In other words, 10 errors will decay
 *		away in 15 seconds, 100 errors will decay in 20 seconds, etc.
 *		An ERROR_DECAY value of 0.96 will produce a single-error
 *		decay of about 2 minutes and a decay rate around one decade
 *		every minute.  0.99 will produce a single-error decay of 8.8
 *		minutes and a decay rate of one decade every 3.8 minutes.
 *
 * MAX_BRANCH =	This macro defines the maximum number of CAMAC interface
 *		modules (branches) the driver will support on a single IOC.
 *		Branch numbers start at 0.  Note that the number of branches
 *		supported can be further restricted through the individual
 *		module entries in "module_types.o".
 *		Default value = 7 (eight branches).
 *
 * MAX_CRATE  =	This macro defines the maximum crate number supported.
 *		The default value is 62 for serial highways, 7 for branch
 *		and other highways.
 *
 * MAX_LAM_MSG = This macro defines the maximum depth of the message queue
 *		used by the the LAM monitor task.
 *		Default value = 64.
 *
 * MAX_NOQ_RETRY = This macro defines the maximum number of times a UBR
 *		function (cfubr or csubr) will loop on no-Q before aborting
 *		the attempt.
 *		Default value = 100.
 *
 * MIN_CRATE  =	This macro defines the minimum crate number supported.
 *		The default is 1 for serial and branch highways, and 0 for
 *		other highways.
 *
 * PRIO_ERR_MON = This macro defines the priority for the CAMAC error
 *		  monitor task.  Default value = 60.
 *
 * PRIO_LAM_MON = This macro defines the priority for the LAM monitor task.
 *		  Default value = 55.
 *
 *-----------------------------------------------------------------------------
 * HARDWARE INTERFACE:
 *
 * This module expects the following routines to be defined by the hardware
 * interface module:
 *
 *   o	hw_extBranch	= Extract branch number from CAMAC channel variable
 *   o	hw_extCrate	= Extract crate number from CAMAC channel variable
 *   o	hw_checkExtb	= Check starting and ending CAMAC variables for
 *			  multiple-address (MAD) routines
 *   o	hw_ccinit	= Initialize CAMAC branch or serial highway
 *   o	hw_cdreg	= Create a CAMAC channel variable
 *   o	hw_cgreg	= Unpack a CAMAC channel variable
 *   o	hw_cfsa		= Perform single function on long (24-bit) data
 *   o	hw_cssa		= Perform single function on short (16-bit) data
 *   o	hw_cfmad	= Perform multiple address (MAD) function on long data
 *   o	hw_csmad	= Perform multiple address (MAD) function on short data
 *   o	hw_cfubc	= Perform repeat-until-Q (UBC) function on long data
 *   o	hw_csubc	= Perform repeat-until-Q (UBC) function on short data
 *   o	hw_cfubr	= Perform repeat-until-noQ (UBR) function on long data
 *   o	hw_csubr	= Perform repeat-until-noQ (UBR) function on short data
 *
 * Hardware definition modules for other highways or single-crate controllers
 * must also provide the following additional routines:
 *
 *   o	hw_crateInit	 = Initialize one CAMAC crate
 *   o	hw_initFuncBlock = Initialize CAMAC variables for crate functions
 *   o  hw_getLamPattern = Read LAM pattern register for specifiec crate
 *
 *   o	hw_cccc		 = Issue dataway C to crate
 *   o	hw_cccd		 = Set or clear LAM/demand enable on crate
 *   o	hw_ccci		 = Set or clear the dataway inhibit on crate
 *   o  hw_cccz		 = Issue dataway Z to crate
 *   o  hw_ctcd		 = Test LAM/demand enabled for crate
 *   o	hw_ctci		 = Test dataway inhibit
 *   o	hw_ctgl		 = Test for LAM present at crate
 *
 * The hardware definition module must also define the following structures
 * and symbols:
 *
 *   o	drvName		= Pointer to character string containing the name of
 *			  the driver (eg. "drvHt2992")
 *   o	hwInfo		= Structure declaration (typedef) for hardware-specific
 *			  information pertaining to a branch.  The contents of
 *			  this structure are relevant only to the hardware
 *			  definition module.
 *
 *   o	BRANCH_TYPE	= Symbol describing what type of branch we are
 *			  supposed to be controlling.  Acceptable values are:
 *				BRANCH = Branch Highway
 *				SERIAL = Serial Highway
 *				SINGLE = Single Crate Interface
 *				OTHER  = Any other type of crate controller (e.g. KSC 2917/3922)
 *
 *-----------------------------------------------------------------------------
 * CAVEATS AND RESTRICTIONS:
 *
 *   o	The first caveat to note is that this code is still very much under
 *	development.  Nothing here should be assumed to be cast in concrete.
 *
 *   o	Because the CAMAC library is compiled with a specific hardware
 *	support module, it is impossible to support more than one type
 *	of CAMAC interface card on a single IOC (although multiple instances
 *	of a single card are supported).
 *
 *   o	If your installation does support more than one type of CAMAC interface
 *	card, you can not have the CAMAC driver automatically loaded.  This is
 *	because there is no way for the record and device support layers to
 *	know which CAMAC interface is being used.
 *
 *   o	There is an implicit assumption in this implementation that pointers
 *	are the same size as int's.
 *
 *   o	The ESONE standard allows for MAD routines (cfmad and csmad) to operate
 *	on multiple crates and branches.  Currently, however, few (if any) of
 *	the supplied hardware support modules implement multi-crate MAD
 *	operations.
 *
 *   o	This implementation does not allow GA routines (cfga and csga) to
 *	operate on multiple branches (multiple crates, however, are allowed).
 *
 *   o	This implementation attempts to achieve both modularity and efficiency
 *	through the use of "in-line" routines.  In-lineing was chosen over
 *	using macros mainly because it is easier to do devlopment with in-line
 *	routines (see note above on the use of the DEBUG macro).  The down-
 *	side is that inlineing is not ANSI standard, and hence not portable
 *	(although it is supported by the GNU compilers, which makes it
 *	portable to any platform we currently support).  The arguments pro and
 *	con are legion here.  Suffice it to say that if you have major moral
 *	objections to the use of inline (for example if the driver won't build
 *	on your platform) it can be removed by editing the INLINE_RTN macro
 *	found further down in this file.
 *
 *   o	Support for branch highways is not fully implented yet.
 *
 *   o	At this time, error recovery and retry are not implemented.	
 *
 *****************************************************************************/

/************************************************************************/
/* Header Files								*/
/************************************************************************/

#include	<stdlib.h>	/* Standard C library routines		*/
#include	<stdio.h>	/* Standard I/O routines		*/
#include        <string.h>	/* String handling routines		*/
#include	<limits.h>	/* Environmental limit definitions	*/

#include	<vxWorks.h>	/* VxWorks definitions & routines	*/
#include	<logLib.h>	/* Message logging library		*/
#include	<msgQLib.h>	/* Message queue service library	*/
#include        <semLib.h>	/* Semaphore service library		*/
#include	<sysLib.h>	/* VxWorks system support library	*/
#include	<taskLib.h>	/* Task handling library		*/
#include	<vxLib.h>	/* VxWorks memory probe routines	*/

#ifdef EPICS
#include	<drvSup.h>	/* EPICS driver support library		*/
#endif

#include	<camacLib.h>	/* ESONE CAMAC routines & message codes	*/

/************************************************************************/
/* Configure Driver Based on the Value of the DEBUG Macro		*/
/************************************************************************/

/*=====================
 * Define the dbgLog routine.
 * If DEBUG is set, log the message, otherwise ignore it.
 */
#ifdef DEBUG
#define dbgLog(p1, p2, p3, p4, p5, p6, p7) logMsg (p1, p2, p3, p4, p5, p6, p7)
#else
#define dbgLog(p1, p2, p3, p4, p5, p6, p7)
#endif


/*=====================
 * Header for routines to be exported from this module
 */
#define GLOBAL_RTN

/*=====================
 * Header for utility routines which will be expanded in-line.
 * If DEBUG is set, make these routines callable from the vxWorks shell.
 */
#ifdef DEBUG
#define INLINE_RTN
#else
#define INLINE_RTN LOCAL __inline__
#endif

/*=====================
 * Header for local (non-inline) routines
 * If DEBUG is set, make these routines callable from the vxWorks shell.
 */
#ifdef DEBUG
#define LOCAL_RTN
#else
#define LOCAL_RTN LOCAL
#endif

/*=====================
 * If DEBUG is set, make local variables globally accessible
 */
#ifdef  DEBUG
#undef  LOCAL
#define LOCAL
#endif

/****************************************************************************/
/* Further Configure the Driver Based on Values of the Configuration Macros */
/****************************************************************************/

/*=====================
 * ERROR_DECAY - Decay constant for exponential error decay algorithm
 */
#if !defined(ERROR_DECAY) || (ERROR_DECAY) <= 0
#undef  ERROR_DECAY
#define ERROR_DECAY	0.6
#endif

/*=====================
 * MAX_BRANCH - Maximum number of CAMAC branches we will support per IOC
 */
#if !defined(MAX_BRANCH) || (MAX_BRANCH) <= 0
#undef	MAX_BRANCH
#define	MAX_BRANCH	7
#endif

/*=====================
 * MAX_LAM_MSG - Maximum depth of message queue for LAM monitor process
 */
#if !defined(MAX_LAM_MSG) || (MAX_LAM_MSG) <= 0
#undef	MAX_LAM_MSG
#define	MAX_LAM_MSG	64
#endif

/*=====================
 * MAX_NOQ_RETRY - Maximum number of times for a UBR command to repeat on No Q
 */
#if !defined(MAX_NOQ_RETRY) || (MAX_NOQ_RETRY) <= 0
#undef	MAX_NOQ_RETRY
#define MAX_NOQ_RETRY	100
#endif

/*=====================
 * PRIO_ERR_MON - Priority for the error monitor task.
 */
#if !defined(PRIO_ERR_MON) || (PRIO_ERR_MON) <= 0
#undef	PRIO_ERR_MON
#define	PRIO_ERR_MON	60
#endif

/*=====================
 * PRIO_LAM_MON - Priority for the LAM monitor task.
 */
#if !defined(PRIO_LAM_MON) || (PRIO_LAM_MON) <= 0
#undef	PRIO_LAM_MON
#define	PRIO_LAM_MON	55
#endif

/************************************************************************/
/* Define Local Constants						*/
/************************************************************************/

#define	ERROR_RATE_THRESH    0.005	/* Error decay threshold	     */
#define	LAM_BLOCK_ID	0x6c416d00	/* Block id for lam_block	     */

#define	MAX_SLOT		30	/* Maximum addressable slot number   */
#define	MAX_NORML_SLOT		23	/* Maximum normal slot number	     */
#define	MAX_SUBADDR		15	/* Maximum subaddres value	     */

/*---------------------
 * Branch type definitions (values used by BRANCH_TYPE)
 */
#define	BRANCH			1	/* Branch Highway		     */
#define	SERIAL			2	/* Serial Highway		     */
#define	SINGLE			3	/* Single Crate Interface	     */
#define	OTHER			4	/* Other type of branch		     */

/*---------------------
 * General CAMAC Function Codes Used By This Driver
 */
#define	READ_LAM		 1	/* Read LAM mask and/or pattern	     */
#define TEST_LAM		 8	/* Test for LAM			     */
#define	CLEAR_LAM		10	/* Clear LAM source		     */
#define	SELECTIVE_SET		19	/* Selective set		     */
#define	SELECTIVE_CLEAR		23	/* Selective clear		     */
#define	DISABLE_LAM		24	/* Disable LAM source		     */
#define	ENABLE_LAM		26	/* Enable LAM source		     */

/*---------------------
 * CAMAC Function Codes and Slots Used for Branch Crate Controllers
 */
#define	BCC_READ_LAMS		 0	/* Read BCC LAM pattern register     */
#define	BCC_LOAD_SNR		16	/* Write slot selection register     */
#define	BCC_CLEAR		24	/* Clear BCC signal (C, Z, I, etc.)  */
#define	BCC_DISABLE_DMD		24	/* Disable demands from crate ctrlr  */
#define	BCC_SET			26	/* Set BCC signal (C, Z, I, etc.)    */
#define	BCC_ENABLE_DMD		26	/* Enable demands at crate ctrlr     */
#define	BCC_TEST		27	/* Test for signal (I, DMD, etc.)    */

#define BCC_NSLOTS		24	/* Address all selected slots	     */
#define BCC_ALLSLOTS		26	/* Address all slots in crate	     */
#define BCC_CZSLOT		28	/* Slot for generating C & Z signals */
#define	BCC_SLOT		30	/* Slot for crate ctrl functions     */

/*---------------------
 * CAMAC Function Codes and Slots Used for Serial Crate Controllers
 */
#define	SCC_REREAD		 0	/* Re-read data from last serial cmd */
#define SCC_READ_LAMS		 1	/* Read SCC LAM pattern register     */
#define	SCC_READ_STATUS		 1	/* Read SCC status register	     */
#define	SCC_WRITE_STATUS	17	/* Write SCC status register	     */
#define	SCC_SET_STATUS		19	/* Selective set SCC status register */
#define	SCC_CLEAR_STATUS	23	/* Selective clear SCC status reg    */

#define	SCC_SLOT		30	/* Slot for crate ctrl functions     */

/*---------------------
 * Define Bits in Serial Crate Controller Status Register
 */
#define	MASK_scc_z	0x0001		/* Initiate a dataway Z		     */
#define	MASK_scc_c	0x0002		/* Initiate a dataway C		     */
#define	MASK_scc_dic	0x0004		/* Dataway inhibit control	     */
#define	MASK_scc_derr	0x0008		/* Delayed error indicator	     */
#define	MASK_scc_dsx	0x0010		/* Delayed X response		     */
#define	MASK_scc_dsq	0x0020		/* Delayed Q response		     */
#define	MASK_scc_dir	0x0040		/* Dataway inhibit read-back	     */
#define	MASK_scc_denb	0x0100		/* Enable demand messages	     */
#define	MASK_scc_idmd	0x0200		/* Set internal demand		     */
#define	MASK_scc_lc	0x0400		/* Loop collapse		     */
#define	MASK_scc_byp	0x0800		/* Bypass			     */
#define	MASK_scc_ofln	0x1000		/* Off-line (software)		     */
#define	MASK_scc_soln	0x2000		/* Switch is off-line (hardware)     */
#define	MASK_scc_lam	0x8000		/* LAM present			     */

/************************************************************************/
/*  Define Error Logging Codes and Message Strings			*/
/************************************************************************/

/*---------------------
 * Highway Errors
 */
#define	BRANCH_ERROR	0x10000
#define	MAX_BRANCH_ERR	5

#define	ERR_NOT_READY	 (BRANCH_ERROR | 0)	/* Branch driver not ready   */
#define	ERR_HW_TIMEOUT	 (BRANCH_ERROR | 1)	/* Hardware timeout	     */
#define	ERR_LONG_PARITY	 (BRANCH_ERROR | 2)	/* Longitudinal parity error */
#define	ERR_TRANS_PARITY (BRANCH_ERROR | 3)	/* Transverse parity error   */
#define	ERR_LOST_SYNCH	 (BRANCH_ERROR | 4)	/* Lost highway synch	     */

LOCAL char   *branch_error [MAX_BRANCH_ERR] = {
  "Driver Not Ready",
  "Hardware Timeout",
  "Longitudinal Parity",
  "Transverse Parity",
  "Lost Synch"
};


/*---------------------
 * Codes for Crate Errors
 */
#define	CRATE_ERROR	0x20000
#define	MAX_CRATE_ERR	4

#define	ERR_CTRL_ERR	(CRATE_ERROR | 0)	/* Crate controller error    */
#define	ERR_NO_RESP	(CRATE_ERROR | 1)	/* Crate does not respond    */
#define ERR_OFFLINE	(CRATE_ERROR | 2)	/* Crate is off-line	     */
#define	ERR_BYPASSED	(CRATE_ERROR | 3)	/* Crate is bypassed	     */

LOCAL char   *crate_error [MAX_CRATE_ERR] = {
  "Controller Error",
  "Crate Not Present",
  "Crate Off-Line",
  "Crate In Bypass"
};


/*---------------------
 * Codes for Slot Errors
 */
#define	SLOT_ERROR	0x30000
#define	MAX_SLOT_ERR	3

#define	ERR_UNEXP_LAM	  (SLOT_ERROR | 0)	/* Unexpected LAM	     */
#define	ERR_NO_X	  (SLOT_ERROR | 1)	/* No X response	     */
#define	ERR_WAITQ_TIMEOUT (SLOT_ERROR | 2)	/* Timeout in "Wait-for-Q"   */

LOCAL char   *slot_error [MAX_SLOT_ERR] = {
  "Unexpected LAM",
  "No X Response",
  "Wait-for-Q Timeout"
};

/************************************************************************/
/* Further Configure the Driver Based on the Hardware Interface		*/
/************************************************************************/


/*=====================
 * Prototype declarations for routines in this module that are called by
 * the hardware interface module.
 */
LOCAL_RTN void  camacLamInt (int, int, int);
LOCAL_RTN void  camacRecordError (int, int);


/*=====================
 * Import the hardware-specific header file.
 */
#include  HW_HEADER


/*=====================
 * Set the default value for MAX_CRATE based on the branch type
 * Note that the default value may be overridden by the hardware module.
 */
#if !defined(MAX_CRATE) || (MAX_CRATE) <= 0
#undef MAX_CRATE
#if   BRANCH_TYPE == BRANCH
#define MAX_CRATE   7		/* Default for branch highway = 8 crates    */
#elif BRANCH_TYPE == SERIAL
#define MAX_CRATE  62		/* Default for serial highway = 62 crates   */
#elif BRANCH_TYPE == SINGLE
#define MAX_CRATE   0		/* Default for single-crate = 1 crate	    */
#elif BRANCH_TYPE == OTHER
#define MAX_CRATE   7		/* Default for other = 8 per branch	    */
#else
#define MAX_CRATE   1		/* Don't know, default to one crate	    */
#endif /*branch type*/
#endif /*MAX_CRATE not already defined*/


/*=====================
 * Set the default value for MIN_CRATE based on the branch type
 * Note that the default value may be overridden by the hardware module.
 */
#if !defined(MIN_CRATE) || (MIN_CRATE) < 0
#undef MIN_CRATE
#if	BRANCH_TYPE == BRANCH
#define	MIN_CRATE   1		/* Default for branch highway = 1	    */
#elif   BRANCH_TYPE == SERIAL
#define MIN_CRATE   1		/* Default for serial highway = 1	    */
#else
#define MIN_CRATE   0		/* Default for all others = 0		    */
#endif /*branch type*/
#endif /*MIN_CRATE not already defined*/

/*=====================
 * Set the definition of a legal slot based on branch type
 * Note that the default definition may be overridden by the hardware module.
 */
#if !defined(LEGAL_SLOT)
#if BRANCH_TYPE == BRANCH
#define LEGAL_SLOT(n) ((n>0) && ((n<=MAX_NORML_SLOT) || (n==BCC_SLOT)   ||  \
                                 (n==BCC_CZSLOT)     || (n==BCC_NSLOTS) ||  \
                                 (n==BCC_ALLSLOTS)))
#elif BRANCH_TYPE == SERIAL
#define LEGAL_SLOT(n) ((n>0) && ((n<=MAX_NORML_SLOT) || (n==SCC_SLOT)))
#elif BRANCH_TYPE == SINGLE
#define LEGAL_SLOT(n) ((n>0) && (n<=MAX_NORML_SLOT))
#elif BRANCH_TYPE == OTHER
#define LEGAL_SLOT(n) ((n>0) && ((n<=MAX_NORML_SLOT) || (n==SCC_SLOT)))
#else
#define LEGAL_SLOT(n) ((n>0) && (n<=MAX_NORML_SLOT))
#endif /*branch type*/
#endif /*LEGAL_SLOT not already defined*/

/*=====================
 * Set the definition of a legal crate based on the previously defined values
 * for MIN_CRATE and MAX_CRATE.
 * Note that this definition may be overridden by the hardware module.
 */
#if !defined(LEGAL_CRATE)
#define LEGAL_CRATE(c) ((c>=MIN_CRATE) && (c<=MAX_CRATE))
#endif /*LEGAL_CRATE not already defined*/

/*=====================
 * Set the definition of a legal branch
 * Note that this definition may be overridden by the hardware module
 */
#if !defined(LEGAL_BRANCH)
#define LEGAL_BRANCH(b) ((unsigned int) b <= MAX_BRANCH)
#endif /*LEGAL_BRANCH not already defined*/

/*=====================
 * Define string for driver type
 */
#if   BRANCH_TYPE == BRANCH
#define	DRIVER_TYPE "Branch Highway"
#elif BRANCH_TYPE == SERIAL
#define	DRIVER_TYPE "Serial Highway"
#else
#define	DRIVER_TYPE "CAMAC"
#endif

/************************************************************************/
/* Local Structure Definitions						*/
/************************************************************************/

typedef struct error_structure    error_structure;
typedef struct init_block         init_block;
typedef struct lam_block          lam_block;


typedef struct branch_structure   branch_structure;
typedef struct crate_structure    crate_structure;
typedef struct slot_structure     slot_structure;


/*=====================
 * error_structure -- Describes the structure for recording CAMAC errors
 */
struct error_structure {
  unsigned long int   current;			/* Errors seen this period   */
  unsigned long int   total;			/* Total seen so far	     */
  double              rate;			/* Error rate (per second)   */
};

/*=====================
 * init_block -- Describes a LAM-Grader (or crate) initialization block
 */
struct init_block {
  init_block         *link;			/* Link to next init_block   */
  camacInitRtn       *initRtn;			/* Initialization routine    */
  int                 b;			/* Branch number	     */
  int                 c;			/* Crate number		     */
  int                 n;			/* Slot number of LAM grader */
};

/*=====================
 * lam_block -- Describes a LAM variable.
 */
struct lam_block {
  unsigned int        block_type;		/* Block ID (sanity check)   */
  int                 ext_test;			/* CAMAC var to test LAM     */
  unsigned char       f_test;			/* Func code to test LAM     */
  int        	      mask_test;		/* Mask to test LAM          */
  int                 ext_clear;		/* CAMAC var to clear LAM    */
  unsigned char       f_clear;			/* Func code to clear LAM    */
  int                 mask_clear;		/* Mask to clear LAM         */
  int                 ext_enable;		/* CAMAC var to enable LAM   */
  unsigned char       f_enable;			/* Func code to enable LAM   */
  int                 mask_enable;		/* Mask to enable LAM        */
  int                 ext_disable;		/* CAMAC var to disable LAM  */
  unsigned char       f_disable;		/* Func code to disable LAM  */
  int                 mask_disable;		/* Mask to disable LAM       */
  SEM_ID              lam_mutex;		/* Lock on LAM structures    */
  int                 waiting;			/* Num tasks waiting for LAM */
  crate_structure    *pcrate;			/* Address of crate structure*/
  int                 slot;			/* Slot number		     */
  int                 slot_mask;		/* Mask to enable slot LAMS  */
  SEM_ID              lam_fired;		/* Sem to signal LAM fired   */
  FUNCPTR             callback;			/* Address of callback routine */
  void               *callback_param;		/* Callback parameter        */
  lam_block          *link;			/* Link to next lam_block    */
};
#define LAM_BLOCK_STATIC_SIZE  OFFSET(lam_block, waiting)

/*=====================
 * lam_message -- Message format from the LAM interrupt service routine
 */
typedef struct /* lam_message */ {
  short int           branch;			/* Branch with LAM	     */
  short int           crate;			/* Crate with LAM	     */
  short int           slot;			/* Slot with LAM source set  */
} lam_message;

/*=====================
 * slot_structure -- Describes a single slot in a crate
 */
struct slot_structure {
  error_structure     errors [MAX_SLOT_ERR];	/* Error recording array     */
  int                 error_flag;		/* Errors present this slot  */
  char               *name;			/* Name of card in this slot */
  camacCardInitRtn   *slotInit;			/* Ptr to slot init routine  */
  int                 slotInitParm;		/* Parm for slot init routine*/
  int                 lam_count;		/* Num LAM blocks this slot  */
  lam_block          *lam_list;			/* LAM block list head	     */
};

/*=====================
 * crate_structure -- Describes a single crate on the highway or interface
 */
struct crate_structure {
  error_structure     errors [MAX_CRATE_ERR];	/* Error recording array     */
  unsigned short int  c;			/* Crate number		     */
  unsigned short int  b;			/* Branch number	     */
  branch_structure   *branch;			/* Addr of branch structure  */
  SEM_ID              lam_mutex;		/* Mutex to lock LAM list    */
  unsigned short int  lam_init_flag;		/* Flag for initial LAM ena. */
  unsigned short int  lams_enabled;		/* LAMs enabled on this crate*/
  crateFuncBlock      func;			/* Crate function defs	     */
  slot_structure      slot [MAX_SLOT];		/* Slot structure array	     */
};

/*=====================
 * branch_structure -- Describes the hardware interface to the branch, serial,
 *                     or crate highway.
 */
struct branch_structure {
  error_structure     errors [MAX_BRANCH_ERR];	/* Error recording array     */
  void               *phwInfo;			/* Ptr to hw-specific info   */
  int                 number;			/* Branch number	     */
  SEM_ID              branch_mutex;		/* Mutex to lock branch	     */
  crate_structure    *crate [MAX_CRATE+1];	/* Crate descriptions	     */
};

/************************************************************************/
/* Local Static Data							*/
/************************************************************************/

LOCAL branch_structure  *branch [MAX_BRANCH+1];	/* Array of ptrs to branch descriptors	*/
						/*  (one per branch/serial driver card)	*/

LOCAL int                camacLib_init = FALSE;	/* CAMAC Library initialized flag	*/
LOCAL int                clock_rate;		/* Clock rate (in ticks per second)	*/

LOCAL init_block        *init_block_list = NULL;/* List head for crate initi blocks	*/
LOCAL init_block        *last_init_block	/* Pointer to last crate init block	*/
                         = (init_block *) &init_block_list;

LOCAL int                max_timeout;		/* Max value for LAM timeout (in msec)	*/
LOCAL int                min_timeout;		/* Min value for LAM timeout (in msec)	*/

LOCAL MSG_Q_ID           lam_msg_q;		/* Message Q for LAM monitor task	*/



/************************************************************************/
/* Function Prototypes For Local Routines 				*/
/************************************************************************/


/*---------------------
 * Common Utility Routines
 */
INLINE_RTN  STATUS             check_fcna (int, int, char*);
INLINE_RTN  void               clear_camacError (void);
INLINE_RTN  branch_structure  *get_branch (int, char*);
INLINE_RTN  lam_block         *get_lam (int);
LOCAL_RTN   STATUS             lock_branch (branch_structure*, char*);
LOCAL_RTN   STATUS             lock_lam_list (crate_structure*, char*);
INLINE_RTN  void               unlock_branch (branch_structure*);
INLINE_RTN  void               unlock_lam_list (crate_structure*);
LOCAL_RTN   STATUS             wait_lam (int[4], char*);

/*---------------------
 * Common Setup Routines
 */
INLINE_RTN  branch_structure  *rtn_setupB  (int, int, int[4], char*);
INLINE_RTN  crate_structure   *rtn_setupC  (int, char*);
INLINE_RTN  branch_structure  *rtn_setupG  (int, int*, int[4], char*);
INLINE_RTN  lam_block         *rtn_setupL  (int, char*);
INLINE_RTN  branch_structure  *rtn_setupS  (int, int, char*);

/*---------------------
 * Driver Initialization Routines
 */
GLOBAL_RTN  long int           camacLibInit (void);
LOCAL_RTN   void               camacCrateInit (int, int);

/*---------------------
 * LAM Interrupt Handling Routines
 */
LOCAL_RTN   void               camacLamMonitor (void);
LOCAL_RTN   void               deliver_LAMs (crate_structure*, int);

/*---------------------
 * Error Recording and Reporting Routines
 */
INLINE_RTN  void               compute_error_rate (error_structure*);
LOCAL_RTN   void               format_error (error_structure*, char*, char*);
LOCAL_RTN   int                print_errors (error_structure*, char*[], int);

LOCAL_RTN   void               camacErrorMonitor (void);

GLOBAL_RTN  int                camacBranchReport (int);
GLOBAL_RTN  int                camacCrateReport (int, int);
GLOBAL_RTN  int                camacSlotReport (int, int, int);
GLOBAL_RTN  int                camacErrorReport (int, int, int);

GLOBAL_RTN  int                camac_io_report (short int);


#ifdef EPICS
/************************************************************************/
/* EPICS Driver Entry Table						*/
/************************************************************************/

struct {
  long int  number;
  DRVSUPFUN  report;
  DRVSUPFUN  init;
} drvCamac = {
  2,				/* Number of entries			*/
  (DRVSUPFUN) camac_io_report,	/* Report routine			*/
  (DRVSUPFUN) camacLibInit	/* Initialization routine		*/
};/*end drvCamac*/
#endif

#if BRANCH_TYPE == BRANCH
/*======================================================================*/
/*	Highway Specific Routines for Branch Highway Drivers		*/
/*									*/
/* The routines in this section are conditionally compiled depending on	*/
/* whether the BRANCH_TYPE symbol in the hardware-specific header file	*/
/* has the value "BRANCH"						*/
/*									*/


/*---------------------
 * Function prototypes for branch-highway-specific routines
 */
INLINE_RTN STATUS  hw_crateInit (crateFuncBlock*, hwInfo*);
INLINE_RTN void    hw_initFuncBlock (crateFuncBlock*, int, int);
INLINE_RTN int     hw_getLamPattern (crateFuncBlock*, hwInfo*);
INLINE_RTN STATUS  hw_cccc (crateFuncBlock*, hwInfo*);
INLINE_RTN STATUS  hw_cccz (crateFuncBlock*, hwInfo*);
INLINE_RTN STATUS  hw_cccd (crateFuncBlock*, hwInfo*, int);
INLINE_RTN STATUS  hw_ctcd (crateFuncBlock*, hwInfo*, int*);
INLINE_RTN STATUS  hw_ctgl (crateFuncBlock*, hwInfo*, int*);
INLINE_RTN STATUS  hw_ccci (crateFuncBlock*, hwInfo*, int);
INLINE_RTN STATUS  hw_ctci (crateFuncBlock*, hwInfo*, int*);


/************************************************************************/
/* hw_crateInit () -- Initialize a branch highway crate controller	*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS  hw_crateInit (crateFuncBlock *pfunc, hwInfo *phwInfo)
{
   /* Not Yet Implemented */
   return OK;

}/*end hw_crateInit()*/


/************************************************************************/
/* hw_initFuncBlock () - Initialize the crateFuncBlock Structure	*/
/*									*/
/************************************************************************/

INLINE_RTN
void hw_initFuncBlock (crateFuncBlock *pfunc, int b, int c)
{
   hw_cdreg (&pfunc->cc_lamPattern, b, c, BCC_SLOT,    0);
   hw_cdreg (&pfunc->cc_lamPresent, b, c, BCC_SLOT,   11);
   hw_cdreg (&pfunc->cc_demand;     b, c, BCC_SLOT,   10);
   hw_cdreg (&pfunc->cc_inhibit;    b, c, BCC_SLOT,    9);
   hw_cdreg (&pfunc->cc_setC;       b, c, BCC_CZSLOT,  9);
   hw_cdreg (&pfunc->cc_setZ;       b, c, BCC_CZSLOT,  8);
   hw_cdreg (&pfunc->cc_selectMask, b, c, BCC_SLOT,    8);

}/*end hw_initFuncBlock()*/


/************************************************************************/
/* hw_getLamPattern () -- Read the LAM pattern register			*/
/*									*/
/************************************************************************/

INLINE_RTN
int  hw_getLamPattern (crateFuncBlock *pfunc, hwInfo *phwInfo);
{
   /* Not Yet Implemented */
   return OK;

}/*end hw_getLamPattern()*/


/************************************************************************/
/* hw_cccc() -- Issue dataway C to branch highway crate controller	*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_cccc (crateFuncBlock *pfunc, hwInfo *phwInfo)
{
   /* Not Yet Implemented */
   return OK;

}/*end hw_cccc()*/


/************************************************************************/
/* hw_cccz() -- Issue dataway Z to branch highway crate controller	*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_cccz (crateFuncBlock *pfunc, hwInfo *phwInfo)
{
   /* Not Yet Implemented */
   return OK;

}/*end hw_cccz()*/


/************************************************************************/
/* hw_cccd() -- Set/Clear LAM enable for branch highway crate controller*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_cccd (crateFuncBlock *pfunc, hwInfo *phwInfo, int l)
{
   /* Not Yet Implemented */
   return OK;

}/*end hw_cccd()*/


/************************************************************************/
/* hw_ctcd() -- Test LAM enable on branch highway crate controller	*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_ctcd (crateFuncBlock *pfunc, hwInfo *phwInfo, int *l)
{
   /* Not Yet Implemented */
   return OK;

}/*end hw_ctcd()*/


/************************************************************************/
/* hw_ctgl() -- Test branch highway crate controller for presence of LAM*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_ctgl (crateFuncBlock *pfunc, hwInfo *phwInfo, int *l)
{
   /* Not Yet Implemented */
   return OK;

}/*end hw_ctgl()*/


/************************************************************************/
/* hw_ccci() -- Set/Clear Inhibit for branch highway crate controller	*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_ccci (crateFuncBlock *pfunc, hwInfo *phwInfo, int l)
{
   /* Not Yet Implemented */
   return OK;

}/*end hw_ccci()*/


/************************************************************************/
/* hw_ctci() -- Test dataway inhibit on branch highway crate controller	*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_ctci (crateFuncBlock *pfunc, hwInfo *phwInfo, int *l)
{
   /* Not Yet Implemented */
   return OK;

}/*end hw_ctci()*/
#endif /*BRANCH_TYPE == BRANCH*/

#if BRANCH_TYPE == SERIAL
/*======================================================================*/
/*	Highway Specific Routines for Serial Highway Drivers		*/
/*									*/
/* The routines in this section are conditionally compiled depending on	*/
/* whether the BRANCH_TYPE symbol in the hardware-specific header file	*/
/* has the value "SERIAL"						*/
/*									*/


/*---------------------
 * Function prototypes for serial-highway-specific routines
 */
INLINE_RTN STATUS  hw_crateInit (crateFuncBlock*, hwInfo*);
INLINE_RTN void    hw_initFuncBlock (crateFuncBlock*, int, int);
INLINE_RTN int     hw_getLamPattern (crateFuncBlock*, hwInfo*);
INLINE_RTN STATUS  hw_cccc (crateFuncBlock*, hwInfo*);
INLINE_RTN STATUS  hw_cccz (crateFuncBlock*, hwInfo*);
INLINE_RTN STATUS  hw_cccd (crateFuncBlock*, hwInfo*, int);
INLINE_RTN STATUS  hw_ctcd (crateFuncBlock*, hwInfo*, int*);
INLINE_RTN STATUS  hw_ctgl (crateFuncBlock*, hwInfo*, int*);
INLINE_RTN STATUS  hw_ccci (crateFuncBlock*, hwInfo*, int);
INLINE_RTN STATUS  hw_ctci (crateFuncBlock*, hwInfo*, int*);


/************************************************************************/
/* hw_crateInit () -- Initialize a serial crate controller		*/
/*	o Sets the crate on-line, unbypassed, not inhibited, with	*/
/*	  demands disabled.						*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS  hw_crateInit (crateFuncBlock *pfunc, hwInfo *phwInfo)
{
   int16_t            scc_stat = 0;	/* Enable mask for SCC		*/
   int                status;		/* Local status variable	*/
   int                q;		/* Q response			*/

   status = hw_cssa (SCC_WRITE_STATUS, pfunc->cc_status, phwInfo, &scc_stat, &q);
   if (!q && (status == OK)) status = S_camacLib_noQ;
   return status;

}/*end hw_crateInit()*/


/************************************************************************/
/* hw_initFuncBlock () - Initialize the crateFuncBlock Structure	*/
/*	o Note that serial highway drivers only use the cc_status,	*/
/*	  cc_lamPattern, and cc_reread fields.				*/
/*									*/
/************************************************************************/

INLINE_RTN
void hw_initFuncBlock (crateFuncBlock *pfunc, int b, int c)
{
   hw_cdreg (&pfunc->cc_status,     b, c, SCC_SLOT,  0);
   hw_cdreg (&pfunc->cc_lamPattern, b, c, SCC_SLOT, 12);
   hw_cdreg (&pfunc->cc_reread,     b, c, SCC_SLOT,  1);

}/*end hw_initFuncBlock()*/


/************************************************************************/
/* hw_getLamPattern () -- Read the LAM pattern register			*/
/*									*/
/************************************************************************/

INLINE_RTN
int  hw_getLamPattern (crateFuncBlock *pfunc, hwInfo *phwInfo)
{
   int   lam_pattern;		/* LAM pattern register			*/
   int   q;			/* Q status return			*/

   hw_cfsa (SCC_READ_LAMS, pfunc->cc_lamPattern, phwInfo, &lam_pattern, &q);
   if (!q) lam_pattern = 0;
   return lam_pattern;

}/*end hw_getLamPattern()*/


/************************************************************************/
/* hw_cccc() -- Issue dataway C to serial crate controller		*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_cccc (crateFuncBlock *pfunc, hwInfo *phwInfo)
{
   int16_t    mask=MASK_scc_c;		/* Mask for dataway C		*/
   int	      q;			/* Q status of operation	*/
   int        status;			/* Local status variable	*/

  /*---------------------
   * Issue the C operation and return the status 
   */
   status = hw_cssa (SCC_SET_STATUS, pfunc->cc_status, phwInfo, &mask, &q);
   if (!q && (status == OK)) status = S_camacLib_noQ;
   return status;

}/*end hw_cccc()*/


/************************************************************************/
/* hw_cccz() -- Issue dataway Z to serial crate controller		*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_cccz (crateFuncBlock *pfunc, hwInfo *phwInfo)
{
   int16_t    mask=MASK_scc_z;		/* Mask for dataway Z		*/
   int        q;			/* Q status of operation	*/
   int        status;			/* Local status variable	*/

  /*---------------------
   * Issue the Z operation and return the status 
   */
   status = hw_cssa (SCC_SET_STATUS, pfunc->cc_status, phwInfo, &mask, &q);
   if (!q && (status == OK)) status = S_camacLib_noQ;
   return status;

}/*end hw_cccz()*/


/************************************************************************/
/* hw_cccd() -- Set/Clear demand enable for serial crate controller	*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_cccd (crateFuncBlock *pfunc, hwInfo *phwInfo, int l)
{
   int        f;			/* Function code		     */
   int16_t    mask=MASK_scc_denb;	/* Mask for dataway inhibit control  */
   int	      q;			/* Q status of operation	     */
   int        status;			/* Local status variable	     */

  /*---------------------
   * Determine whether we should be setting or clearing the demand enable
   */
   f = (l ? SCC_SET_STATUS : SCC_CLEAR_STATUS);

  /*---------------------
   * Perform the set or clear and return the status
   */
   status = hw_cssa (f, pfunc->cc_status, phwInfo, &mask, &q);
   if (!q && (status == OK)) status = S_camacLib_noQ;
   return status;

}/*end hw_cccd()*/


/************************************************************************/
/* hw_ctcd() -- Test demand enable on serial crate controller		*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_ctcd (crateFuncBlock *pfunc, hwInfo *phwInfo, int *l)
{
   int	      q;			/* Q status of operation	     */
   int16_t    scc_status;		/* Contents of SCC status register   */
   int        status;			/* Local status variable	     */

  /*---------------------
   * Read the serial crate controller status register and check the status
   */
   status = hw_cssa (SCC_READ_STATUS, pfunc->cc_status, phwInfo, &scc_status, &q);
   if (!q && (status == OK)) status = S_camacLib_noQ;

  /*---------------------
   * Return the state of the "demand enable" bit and the status of the
   * read operation.
   */
   if (status == OK) *l = (scc_status & MASK_scc_denb) ? TRUE: FALSE;
   return status;

}/*end hw_ctcd()*/


/************************************************************************/
/* hw_ctgl() -- Test serial crate controller for presence of LAM	*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_ctgl (crateFuncBlock *pfunc, hwInfo *phwInfo, int *l)
{
   int  lam_pattern;		/* Contents of LAM pattern register	*/
   int	q;			/* Q status of operation		*/
   int  status;			/* Local status variable		*/

  /*---------------------
   * Read the crate controller LAM pattern register and check the status
   */
   status = hw_cfsa (SCC_READ_LAMS, pfunc->cc_lamPattern, phwInfo, &lam_pattern, &q);
   if (!q && (status == OK)) status = S_camacLib_noQ;

  /*---------------------
   * Return TRUE if any LAMs are pending
   */
   if (status == OK) *l = lam_pattern ? TRUE: FALSE;
   return status;

}/*end hw_ctgl()*/


/************************************************************************/
/* hw_ccci() -- Set/Clear dataway Inhibit for serial crate controller	*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_ccci (crateFuncBlock *pfunc, hwInfo *phwInfo, int l)
{
   int        f;			/* Function code		     */
   int16_t    mask=MASK_scc_dic;	/* Mask for dataway inhibit control  */
   int	      q;			/* Q status of operation	     */
   int        status;			/* Local status variable	     */

  /*---------------------
   * Determine whether we should be setting or clearing the dataway inhibit
   */
   f = (l ? SCC_SET_STATUS : SCC_CLEAR_STATUS);

  /*---------------------
   * Perform the set or clear and return the status
   */
   status = hw_cssa (f, pfunc->cc_status, phwInfo, &mask, &q);
   if (!q && (status == OK)) status = S_camacLib_noQ;
   return status;

}/*end hw_ccci()*/


/************************************************************************/
/* hw_ctci() -- Test dataway inhibit on serial crate controller		*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS hw_ctci (crateFuncBlock *pfunc, hwInfo *phwInfo, int *l)
{
   int	      q;			/* Q status of operation	     */
   int16_t    scc_status;		/* Contents of SCC status register   */
   int        status;			/* Local status variable	     */

  /*---------------------
   * Read the serial crate controller status register and check the status
   */
   status = hw_cssa (SCC_READ_STATUS, pfunc->cc_status, phwInfo, &scc_status, &q);
   if (!q && (status == OK)) status = S_camacLib_noQ;

  /*---------------------
   * Return the state of the "dataway inhibit" bit and the status of the
   * read operation.
   */
   if (status == OK) *l = (scc_status & MASK_scc_dir) ? TRUE: FALSE;
   return status;

}/*end hw_ctci()*/
#endif /*BRANCH_TYPE == SERIAL*/

/*======================================================================*/
/*			Common Utility Routines				*/
/*									*/

/************************************************************************/
/* check_fcna () -- Check function code and CAMAC variable		*/
/*									*/
/************************************************************************/

INLINE_RTN
STATUS  check_fcna (int f, int ext, char *caller)
{
   int  status = OK;		/* Local status variable */

   /* Check the function code */
   if ((unsigned int) f > 31) {
      dbgLog ("%s: Routine %s -- Invalid function code (%d)\n",
             (int)drvName, (int)caller, f, 0, 0, 0);
      status = S_camacLib_Bad_F;
   }/*end if invalid function code*/

   /* Check the CAMAC channel variable */
   else if (ext == 0) {
      dbgLog ("%s: Routine %s -- Invalid CAMAC channel variable\n",
             (int)drvName, (int)caller, 0, 0, 0, 0);
      status = S_camacLib_Bad_Var;
   }/*end if invalid CAMAC variable*/

   /* Return the status code */
   if (status != OK) errno = status;
   return status;

}/*end check_fcna()*/


/************************************************************************/
/* clear_camacError () -- Clear CAMAC error status from errno		*/
/*									*/
/************************************************************************/

INLINE_RTN
void  clear_camacError (void)
{
   if ((errno & 0xffff0000) == M_camacLib) errno = OK;
   return;
}/*end clear_camacError()*/

/************************************************************************/
/* get_branch () -- Return address of branch structure			*/
/*									*/
/************************************************************************/

INLINE_RTN
branch_structure *get_branch (int ext, char *caller)
{
   int                         b;	/*Branch number			*/
   register branch_structure  *pbranch;	/*Pointer to branch structure	*/

   /* Get branch number and check it for validity			*/
   b = hw_extBranch (ext);
   if (!LEGAL_BRANCH(b)) {
      dbgLog ("%s: Routine %s -- Invalid CAMAC channel variable\n",
             (int)drvName, (int)caller, 0, 0, 0, 0);
      errno = S_camacLib_Bad_Var;
      return NULL;
   }/*end if invalid branch*/

   /* Return the address of the branch structure */
   pbranch = branch[b];
   if (pbranch == NULL) {
      errno = S_camacLib_No_Hw;
      return NULL;
   }/*end if no hardware*/

   return pbranch;

}/*end get_branch()*/


/************************************************************************/
/* get_lam () -- Return LAM identifier, or ERROR if invalid		*/
/*									*/
/************************************************************************/

INLINE_RTN
lam_block *get_lam (int lam)
{
   if (lam)
      if (((lam_block *)lam)->block_type != LAM_BLOCK_ID)
         return (lam_block *) ERROR;

   return (lam_block *) lam;

 }/*end get_lam()*/

/************************************************************************/
/* lock_branch () -- Lock branch					*/
/*									*/
/************************************************************************/

LOCAL_RTN
STATUS  lock_branch (branch_structure *pbranch, char *caller)
{
   int  status;		/* Local status variable */

   status = semTake (pbranch->branch_mutex, WAIT_FOREVER);
   if (status != OK) {
      logMsg ("%s: Routine %s -- Unable to lock Mutex for branch %d\n",
             (int)drvName, (int)caller, pbranch->number, 0, 0, 0);
   }/*end if error locking mutex*/

   return status;

}/*end lock_branch()*/


/************************************************************************/
/* lock_lam_list () -- Lock LAM list for a specified crate		*/
/*									*/
/************************************************************************/

LOCAL_RTN
STATUS  lock_lam_list (crate_structure *pcrate, char *caller)
{
   int  status;		/* Local status variable */

   status = semTake (pcrate->lam_mutex, WAIT_FOREVER);
   if (status != OK) {
      logMsg ("%s: Routine %s -- Unable to lock LAM Mutex for branch %d, crate %d\n",
             (int)drvName, (int)caller, pcrate->b, pcrate->c, 0, 0);
   }/*end if error locking mutex*/
   return status;

}/*end lock_lam_list()*/

/************************************************************************/
/* unlock_branch () -- Unlock branch					*/
/*									*/
/************************************************************************/

INLINE_RTN
void  unlock_branch (branch_structure *pbranch)
{
   semGive (pbranch->branch_mutex);
   return;

}/*end unlock_branch()*/


/************************************************************************/
/* unlock_lam_list () -- Unlock lam list for a specified crate		*/
/*									*/
/************************************************************************/

INLINE_RTN
void  unlock_lam_list (crate_structure *pcrate)
{
   semGive (pcrate->lam_mutex);
   return;

}/*end unlock_lam_list()*/

/************************************************************************/
/* wait_lam () -- Wait for LAM or timeout				*/
/*	o If a LAM id is specified in the third entry of the control	*/
/*	  block wait for the LAM before continuing.			*/
/*	o The last parameter in control	block is a timeout		*/
/*	  (in milliseconds)						*/
/*									*/
/************************************************************************/

LOCAL_RTN
STATUS wait_lam (int cb[4], char *caller)
{
  register lam_block  *plamb;	/* Pointer to LAM block		*/
  STATUS               status;	/* Temporary status variable	*/
  int                  timeout;	/* Timeout (in clock ticks)	*/

  /*---------------------
   * Don't wait if no LAM id specified
   */
   plamb = get_lam (cb[2]);
   if (plamb == NULL) return (OK);

  /*---------------------
   * Check for error in LAM id
   */
   if ((int)plamb == ERROR) {
      dbgLog ("%s: Routine %s -- Invalid LAM identifier\n",
             (int)drvName, (int)caller, 0, 0, 0, 0);
      return (S_camacLib_Bad_LAM);
   }/*end if error in LAM id*/

  /*---------------------
   * Check the timeout parameter
   */
   timeout = cb[3];
   if ((timeout == 0) || (timeout == WAIT_FOREVER)) timeout = WAIT_FOREVER;
   else if (timeout < min_timeout) timeout = min_timeout;
   else if (timeout <= max_timeout) timeout = ((timeout * clock_rate)/1000) + 1;
   else return S_camacLib_Bad_Timeout;

  /*---------------------
   * Lock the crate's LAM list while we register interest in this LAM 
   */
   status = lock_lam_list (plamb->pcrate, caller);
   if (status == ERROR) return ERROR;

  /*---------------------
   * Indicate that we are waiting for this LAM.  Make sure the LAM is
   * enabled.  Disable task preemption until we block on the LAM
   * semaphore.
   */
   plamb->waiting += 1;			/* Register interest in this LAM */
   cclm ((int)plamb, TRUE);		/* Make sure LAM is enabled	 */
   clear_camacError ();			/* Ignore any errors from cclm	 */
   taskLock ();				/* Disable preemption		 */
   unlock_lam_list (plamb->pcrate);	/* Release lock on LAM list	 */

  /*---------------------
   * Wait for the LAM or timeout
   */
   status = semTake (plamb->lam_fired, timeout);

  /*---------------------
   * Check for timeout
   */
   if (status == ERROR) {
      if (errno == S_objLib_OBJ_TIMEOUT) status = S_camacLib_LAM_Timeout;
      lock_lam_list (plamb->pcrate, caller);
      if (plamb->waiting > 0) plamb->waiting -= 1;
      unlock_lam_list (plamb->pcrate);
   }/*end if timeout*/

  /*---------------------
   * Re-enable task preemption and return to caller
   */
   taskUnlock ();		/* Re-enable preemptions	*/
   return (status);		/* Return completion status	*/

}/*end wait_lam()*/

/*======================================================================*/
/*			Common Setup Routines				*/
/*									*/


/************************************************************************/
/* rtn_setupB () -- Common setup for block-mode routines		*/
/*	o Clear the "operations completed" counter			*/
/*	o Clear CAMAC errors from errno					*/
/*	o Validate "f" and "ext" parameters				*/
/*	o Validate the repeat count field of the control block		*/
/*	o Locate branch structure for this branch			*/
/*	o Note: does not lock the branch structure			*/
/*									*/
/************************************************************************/

INLINE_RTN
branch_structure  *rtn_setupB (int f, int ext, int cb[4], char *caller)
{
   branch_structure  *pbranch;		/* Ptr to branch structure  */
   int                status;		/* Local status variable    */

  /*---------------------
   * Clear the "operations completed" counter (cb[1]) and any
   * CAMAC errors left over from previous operations.
   */
   cb[1] = 0;
   clear_camacError ();

  /*---------------------
   * Check the function code and the CAMAC channel variable
   */
   status = check_fcna (f, ext, caller);
   if (status != OK) return NULL; 

  /*---------------------
   * Check the repeat count field of the control block
   */
   if (cb[0] < 1) {
      dbgLog ("%s: Routine %s -- Invalid repeat count.\n",
             (int)drvName, (int)caller, 0, 0, 0, 0);
      errno = S_camacLib_BadRptCnt;
      return NULL;
   }/*end if repeat count not ok*/

  /*---------------------
   * Return the address of the branch structure
   */
   pbranch = get_branch (ext, caller);
   return pbranch;

}/*end rtn_setupB()*/

/************************************************************************/
/* rtn_setupC () -- Common setup for crate controller routines		*/
/*	o Clear CAMAC errors from errno					*/
/*	o Locate crate structure for this crate and branch		*/
/*	o Lock the branch.						*/
/*	o Return the address of the crate structure			*/
/*									*/
/************************************************************************/

INLINE_RTN
crate_structure  *rtn_setupC (int ext, char *caller)
{
   int                c;		/* Crate number			*/
   branch_structure  *pbranch;		/* Pointer to branch structure	*/
   crate_structure   *pcrate;		/* Pointer to crate structure	*/
   int                status;		/* Local status variable	*/

  /*---------------------
   * Clear any CAMAC errors from previous operations
   */
   clear_camacError ();

  /*---------------------
   * Get the branch structure
   */
   pbranch = get_branch (ext, caller);
   if (pbranch == NULL) return NULL;

  /*---------------------
   * Get the crate structure
   */
   c = hw_extCrate (ext);
   if (!LEGAL_CRATE(c)) pcrate = NULL;
   else pcrate = pbranch->crate[c];

  /*---------------------
   * If we have got this far and there is no crate structure, then
   * either we have a counterfeit ext, or cdreg failed when it tried
   * to create the ext.  In either case, report an invalid channel
   * variable and terminate.
   */
   if (pcrate == NULL) {
      dbgLog ("%s: Routine %s -- Invalid CAMAC channel variable\n",
             (int)drvName, (int)caller, 0, 0, 0, 0);
      errno = S_camacLib_Bad_Var;
      return NULL;
   }/*end if no crate structure */

  /*---------------------
   * Try to lock the branch
   */
   status = lock_branch (pbranch, caller);
   if (status != OK) return NULL;

  /*---------------------
   * Return a pointer to the crate structure
   */
   return pcrate;

}/*end rtn_setupC()*/

/************************************************************************/
/* rtn_setupG () -- Common setup for general-purpose multiple-action	*/
/*                  routines.						*/
/*	o Clear the "operations completed" counter			*/
/*	o Clear CAMAC errors from errno					*/
/*	o Validate the repeat count field of the control block		*/
/*	o Locate branch structure for this branch			*/
/*	o Return the branch number					*/
/*	o Note: does not lock the branch structure			*/
/*									*/
/************************************************************************/

INLINE_RTN
branch_structure  *rtn_setupG (int ext, int *b, int cb[4], char *caller)
{
   branch_structure  *pbranch;		/* Ptr to branch structure  */

  /*---------------------
   * Clear the "operations completed" counter (cb[1]) and any
   * CAMAC errors left over from previous operations.
   */
   cb[1] = 0;
   clear_camacError ();

  /*---------------------
   * Check the repeat count field of the control block
   */
   if (cb[0] < 1) {
      dbgLog ("%s: Routine %s -- Invalid repeat count.\n",
             (int)drvName, (int)caller, 0, 0, 0, 0);
      errno = S_camacLib_BadRptCnt;
      return NULL;
   }/*end if repeat count not ok*/

  /*---------------------
   * Return the branch number and the address of the branch structure
   */
   pbranch = get_branch (ext, caller);
   if (pbranch) *b = pbranch->number;
   return pbranch;

}/*end rtn_setupG()*/

/************************************************************************/
/* rtn_setupL () -- Common setup for LAM routines			*/
/*	o Clear CAMAC errors from errno					*/
/*	o Check for valid LAM block					*/
/*	o Returns the address of the LAM block or NULL if invalid	*/
/*									*/
/************************************************************************/

INLINE_RTN
lam_block  *rtn_setupL (int lam, char *caller)
{
   register lam_block  *plamb;		/* Pointer to LAM block		*/

  /*---------------------
   * Clear any CAMAC errors from previous operations
   */
   clear_camacError ();

  /*---------------------
   * Check for valid LAM block
   */
   plamb = get_lam (lam);
   if ((plamb == NULL) || ((int)plamb == ERROR)) {
      dbgLog ("%s: Routine %s -- Invalid LAM identifier\n",
             (int)drvName, (int)caller, 0, 0, 0, 0);
      errno = S_camacLib_Bad_LAM;
      plamb = NULL;
   }/*end if bad LAM block*/

   return plamb;

}/*end rtn_setupL()*/

/************************************************************************/
/* rtn_setupS () -- Common setup for single-action routines		*/
/*	o Clear CAMAC errors from errno					*/
/*	o Validate "f" and "ext" parameters				*/
/*	o Locate branch structure for this branch			*/
/*	o Lock the branch						*/
/*									*/
/************************************************************************/

INLINE_RTN
branch_structure  *rtn_setupS (int f, int ext, char *caller)
{
   register branch_structure  *pbranch;		/* Ptr to branch structure  */
   int                         status;		/* Local status variable    */

  /*---------------------
   * Clear any CAMAC errors from previous operations
   */
   clear_camacError ();

  /*---------------------
   * Check the function code and the CAMAC channel variable
   */
   status = check_fcna (f, ext, caller);
   if (status != OK) return NULL;

  /*---------------------
   * Get the branch structure
   */
   pbranch = get_branch (ext, caller);
   if (pbranch == NULL) return NULL;

  /*---------------------
   * Lock the branch
   */
   status = lock_branch (pbranch, caller);
   if (status != OK) return NULL;

  /*---------------------
   * Return the address of the branch structure
   */
   return pbranch;

}/*end rtn_setupS()*/

/*======================================================================*/
/*			ESONE Compliant CAMAC Routines			*/
/*									*/
/* Note: See the header file, camacLib.h, for a description of		*/
/*       these routines and their parameters.				*/
/*									*/


/************************************************************************/
/* ccinit() -- Initialize a single branch or serial highway.		*/
/*	o Initialize the branch driver hardware.			*/
/*	o Allocate and initialize a branch data structure if one does	*/
/*	  not already exist.						*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void ccinit (int b)
{
   void              *phwInfo;	/* Pointer to hw-specific info	*/
   branch_structure  *pbranch;	/* Pointer to branch structure	*/

  /*---------------------
   * Check for branch in range
   */
   if (!LEGAL_BRANCH(b)) {
      logMsg ("%s: Routine ccinit -- Invalid branch number - %d\n",
             (int)drvName, b, 0, 0, 0, 0);
      return;
   }/*end if invalid branch number*/

  /*---------------------
   * Initialize the hardware
   */
   phwInfo = hw_ccinit (b);

  /*---------------------
   * If the branch driver was present and not previously initialized,
   * allocate and initialize a branch structure for this branch.
   */
   if ((phwInfo != NULL) && (branch[b] == NULL)) {

     /*---------------------
      * Allocate the branch structure
      */
      pbranch = calloc(1, sizeof(branch_structure));
      if (pbranch == NULL) {
         logMsg ("%s: Routine ccinit -- Insufficient memory to initialize branch %d\n",
                 (int)drvName, b, 0, 0, 0, 0);
         return;
      }/*end if can't allocate branch structure*/

     /*---------------------
      * Initialize the branch structure
      */
      pbranch->phwInfo = phwInfo;		/* Store ptr to hw-specific info	 */
      pbranch->number = b;			/* Store the branch number		 */
      pbranch->branch_mutex = semMCreate (	/* Create the branch mutex               */
                      SEM_Q_PRIORITY     |	/*    use priority queueing              */
                      SEM_INVERSION_SAFE |	/*    protect against priority inversion */
                      SEM_DELETE_SAFE);		/*    protect against task deletion      */
      if (pbranch->branch_mutex == NULL) {
         logMsg ("%s: Routine ccinit -- Unable to create mutex for branch %d\n",
                (int)drvName, b, 0, 0, 0, 0);
         free (pbranch);
         return;
      }/*end if can't allocate branch mutex*/

     /*---------------------
      * Indicate there is hardware present for this branch
      */
      branch[b] = pbranch;		/* Store pointer to branch struct */

   }/*end if need to initialize the branch structure*/

}/*end ccinit()*/

/************************************************************************/
/* cdlam () -- Construct a CAMAC LAM variable.				*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void cdlam (int *lam, int b, int c, int n, int m, void *inta[])
{
   branch_structure   *pbranch;		/* Pointer to branch structure	*/
   crate_structure    *pcrate;		/* Pointer to crate structure	*/
   slot_structure     *pslot;		/* Pointer to slot structure	*/
   lam_block          *plamb;		/* Pointer to new lam_block	*/
   lam_block          *p;		/* Pointer for list traversal	*/
   lamParams	      *plamParams;	/* Pointer to lamParams structure */
   int                 status;		/* Local status variable	*/

   *lam = 0; /* Initialize to invalid value */

  /*---------------------
   * Check for legal branch
   */
   if (!LEGAL_BRANCH(b)) {
      errno = S_camacLib_Bad_B;
      dbgLog ("%s: Routine cdlam -- Invalid branch number (%d)\n",
             (int)drvName, b, 0, 0, 0, 0);
      return;
   }/*end if invalid branch id*/

  /*---------------------
   * Check for legal crate
   */
   if (!LEGAL_CRATE(c)) {
      errno = S_camacLib_Bad_C;
      dbgLog ("%s: Routine cdlam -- Invalid crate number (%d)\n",
             (int)drvName, c, 0, 0, 0, 0);
      return;
   }/*end if invalid crate id*/

  /*---------------------
   * Check for legal slot
   */
   if ((n < 1) || (n > MAX_NORML_SLOT)) {
      errno = S_camacLib_Bad_N;
      dbgLog ("%s: Routine cdlam -- Invalid slot number (%d)\n",
             (int)drvName, n, 0, 0, 0, 0);
      return;
   }/*end if invalid slot id*/

  /*---------------------
   * Check for legal subaddress
   */
   if (m > MAX_SUBADDR) {
      errno = S_camacLib_Bad_A;
      dbgLog ("%s: Routine cdlam -- Invalid subaddress (%d)\n",
             (int)drvName, m, 0, 0, 0, 0);
      return;
   }/*end if invalid subaddress*/

  /*---------------------
   * Get branch, crate, and slot structures.
   * Abort if branch or crate structures don't exist.
   */
   pbranch = branch[b];
   if (pbranch == NULL) return;

   pcrate = pbranch->crate[c];
   if (pcrate == NULL) return;

   pslot = &(pcrate->slot[n-1]);

  /*---------------------
   * Allocate a LAM block
   */
   plamb = malloc (sizeof(lam_block));
   if (plamb == NULL) {
      dbgLog ("%s: Routine cdlam -- Unable to allocate memory for LAM variable.\n",
             (int)drvName, 0, 0, 0, 0, 0);
      return;
   }/*end if can't allocate lam_block*/

  /*---------------------
   * Initialize the LAM block
   */
   plamb->block_type = LAM_BLOCK_ID;		/* lam_block ID		     */
   plamb->lam_mutex = pcrate->lam_mutex;	/* Copy of lam-list mutex    */
   plamb->pcrate = pcrate;			/* Address of crate struct   */
   plamb->slot = n;				/* Slot number		     */
   plamb->slot_mask = 1 << (n-1);		/* Mask for enabling slot    */
   plamb->waiting = 0;				/* Num. tasks waiting on LAM */
  
   /* 
    * See if the user has specifed special LAM processing function codes
    */
   if ((inta != NULL) && (inta[0] != NULL)) {
      plamParams = inta[0];
      if (plamParams->f_test >= 0) {
        hw_cdreg (&(plamb->ext_test), b, c, n, plamParams->a_test);
        plamb->f_test = plamParams->f_test;
        plamb->mask_test = plamParams->mask_test;
      } else {
        plamb->ext_test = 0;
        plamb->f_test = 0;
        plamb->mask_test = 0;
      }
      if (plamParams->f_clear >= 0) {
        hw_cdreg (&(plamb->ext_clear), b, c, n, plamParams->a_clear);
        plamb->f_clear = plamParams->f_clear;
        plamb->mask_clear = plamParams->mask_clear;
      } else {
        plamb->ext_clear = 0;
        plamb->f_clear = 0;
        plamb->mask_clear = 0;
      }
      if (plamParams->f_enable >= 0) {
        hw_cdreg (&(plamb->ext_enable), b, c, n, plamParams->a_enable);
        plamb->f_enable = plamParams->f_enable;
        plamb->mask_enable = plamParams->mask_enable;
      } else {
        plamb->ext_enable = 0;
        plamb->f_enable = 0;
        plamb->mask_enable = 0;
      }
      if (plamParams->f_disable >= 0) {
        hw_cdreg (&(plamb->ext_disable), b, c, n, plamParams->a_disable);
        plamb->f_disable = plamParams->f_disable;
        plamb->mask_disable = plamParams->mask_disable;
      } else {
        plamb->ext_disable = 0;
        plamb->f_disable = 0;
        plamb->mask_disable = 0;
      }
   }
   else if (m >= 0) /* "Subaddress" style of LAM addressing */ {
      hw_cdreg (&(plamb->ext_test), b, c, n, m);
      plamb->ext_clear = plamb->ext_test;
      plamb->ext_enable = plamb->ext_test;
      plamb->ext_disable = plamb->ext_test;
      plamb->f_test = TEST_LAM;
      plamb->f_clear = CLEAR_LAM;
      plamb->f_enable = ENABLE_LAM;
      plamb->f_disable = DISABLE_LAM;
      plamb->mask_test = 0;
      plamb->mask_clear = 0;
      plamb->mask_enable = 0;
      plamb->mask_disable = 0;
   }
   else /* "Mask-Register" style of LAM addressing */ {
      hw_cdreg (&(plamb->ext_test), b, c, n, 14);
      hw_cdreg (&(plamb->ext_clear), b, c, n, 12);
      hw_cdreg (&(plamb->ext_enable), b, c, n, 13);
      hw_cdreg (&(plamb->ext_disable), b, c, n, 13);
      plamb->f_test = READ_LAM;
      plamb->f_clear = SELECTIVE_CLEAR;
      plamb->f_enable = SELECTIVE_SET;
      plamb->f_disable = SELECTIVE_CLEAR;
      plamb->mask_test = 1 << -(m+1);
      plamb->mask_clear = 1 << -(m+1);
      plamb->mask_enable = 1 << -(m+1);
      plamb->mask_disable = 1 << -(m+1);
   }/*end "Mask-Register" style of LAM addressing*/

   plamb->callback = NULL;
   /* See if the user has specifed parameter to be passed to callback routine
      specified in cclnk */
   if ((inta != NULL) && (inta[1] != NULL)) 
      plamb->callback_param = inta[1];
   else
      plamb->callback_param = NULL;
  /*---------------------
   * Lock the crate's LAM  list
   */
   status = lock_lam_list (pcrate, "cdlam");
   if (status != OK) {
      free (plamb);
      return;
   }/*end if can't lock LAM list*/

  /*---------------------
   * See if there already is a LAM block like the one we just created
   */
   for (p=pslot->lam_list; p; p=p->link) {
      if (!memcmp(p, plamb, LAM_BLOCK_STATIC_SIZE)) break;
   }/*end for each block on LAM list*/

  /*---------------------
   * If the LAM block already exists on the list, release the new one.
   */
   if (p) {
      free (plamb);
      plamb = p;
   }/*end if LAM block already exists*/

  /*---------------------
   * If the LAM block is not already on the list, try to create the "LAM
   * fired" semaphore for it.
   */
   else if (!(plamb->lam_fired = semBCreate (SEM_Q_FIFO, SEM_EMPTY))) {
      logMsg ("%s: Routine cdlam -- Unable to create LAM synchronization semaphore\n",
             (int)drvName, 0, 0, 0, 0, 0);
      free (plamb);
      plamb = NULL;
   }/*end if semaphore create failed*/

  /*---------------------
   * If the semaphore create succeeded, add this block to the LAM block
   * list for this slot and increment the LAM block counter.
   */
   else {
      plamb->link = pslot->lam_list;
      pslot->lam_list = plamb;
      pslot->lam_count++;
   }/*end add lam block to list*/

  /*---------------------
   * If this was the first LAM block created for this crate, enable LAMs
   * on the crate controller.
   */
   if ((plamb != NULL) && (!pcrate->lam_init_flag))
       cccd (plamb->ext_clear, TRUE);

  /*---------------------
   * Unlock the LAM list and return the lam identifier
   */
   unlock_lam_list (pcrate);
   *lam = (int)plamb;

}/*end cdlam()*/

/************************************************************************/
/* cdreg () -- Create a CAMAC channel variable.				*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void cdreg (int *ext, int b, int c, int n, int a)
{
   branch_structure  *pbranch;	/* Pointer to branch structure */

  /*---------------------
   * Check the parameters
   */
   clear_camacError ();		/* Initialize error return	*/
   *ext = 0;			/* Initialize channel variable	*/

   /* Check the branch number */
   if (!LEGAL_BRANCH(b)) {
      errno = S_camacLib_Bad_B;
      dbgLog ("%s: Routine cdreg -- Invalid branch value (%d)\n",
             (int)drvName, b, 0, 0, 0, 0);
      return;
   }/*end if invalid branch*/

   /* See if branch hardware is present */
   pbranch = branch[b];
   if (pbranch == NULL) {
      errno = S_camacLib_No_Hw;
      dbgLog ("%s: Routine cdreg -- No hardware present for branch %d\n",
             (int)drvName, b, 0, 0, 0, 0);
   }/*end if no branch hardware */

   /* Check the crate number */
   if (!LEGAL_CRATE(c)) {
      errno = S_camacLib_Bad_C;
      dbgLog ("%s: Routine cdreg -- Invalid crate value (%d)\n",
             (int)drvName, c, 0, 0, 0, 0);
      return;
   }/*end if invalid crate*/

   /* Check the slot number */
   if (!LEGAL_SLOT(n)) {
      errno = S_camacLib_Bad_N;
      dbgLog ("%s: Routine cdreg -- Invalid slot value (%d)\n",
             (int)drvName, n, 0, 0, 0, 0);
      return;
   }/*end if invalid slot*/

   /* Check the subaddress */
   if ((unsigned int) a > MAX_SUBADDR) {
      errno = S_camacLib_Bad_A;
      dbgLog ("%s: Routine cdreg -- Invalid sub-address value (%d)\n",
             (int)drvName, a, 0, 0, 0, 0);
      return;
   }/*end if invalid subaddress*/

  /*---------------------
   * Call hardware-specific routine to pack the channel variable
   */
   hw_cdreg (ext, b, c, n, a);

  /*---------------------
   * Check to see if we need to initialize the crate
   */
   if (*ext) {
      if (pbranch == NULL) return;
      if (pbranch->crate[c] == NULL) camacCrateInit (b, c);
   }/*end if all parameters valid*/

}/*end cdreg()*/



/************************************************************************/
/* cgreg () -- Unpack a CAMAC channel variable				*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void cgreg (int ext, int *b, int *c, int *n, int *a)
{
   hw_cgreg (ext, b, c, n, a);
   return;
}/*end cgreg()*/

/************************************************************************/
/* cclm () -- Enable/Disable LAM					*/
/*	o Issue the CAMAC command to enable or disable the LAM source.	*/
/*	o When enabling a LAM, check to see if we need to enable it at	*/
/*	  the crate controller or lam-grader as well.			*/
/*	o When disabling a LAM, do not disable it at the crate ctrlr or	*/
/*	  lam-grader, since the card may have more than one LAM source,	*/
/*	  UNLESS there is no Disable LAM function defined, since then	*/
/*	  the only way to disable the LAM is to disable the slot	*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void cclm (int lam, int l)
{
   register crateFuncBlock   *pfunc;	/* Pointer to crate function block */
   register lam_block        *plamb;	/* Pointer to lam_block		   */
   int                        q;	/* Temp for q return		   */

  /*---------------------
   * Check for valid LAM id
   */
   plamb = rtn_setupL (lam, "cclm");
   if (plamb == NULL) return;
      
  /*---------------------
   * LAM identifier is valid, get the crate function block.
   */
   pfunc = &plamb->pcrate->func;

  /*---------------------
   * Enable the LAM and possibly its slot as well
   */
   if (l) {
      if (plamb->ext_enable)
         cfsa (plamb->f_enable, plamb->ext_enable, 
      				(int *)&(plamb->mask_enable), &q);

  /*---------------------
   * We call enableSlotLam (if it exists) either if it has never been called
   * for this slot, or if this LAM does not have a "disable" function, since
   * in that case it may have been disabled by disabling the slot.
   */
      if (!(pfunc->lam_mask & plamb->slot_mask) || !(plamb->ext_disable)) {
         pfunc->lam_mask |= plamb->slot_mask;
         if (pfunc->enableSlotLam != NULL)
            pfunc->enableSlotLam (pfunc, plamb->slot);
      }/*end if slot not already enabled*/

   }/*end enable LAM*/

  /*---------------------
   * Disable the LAM
   */
   else {
     if (plamb->ext_disable)
        cfsa (plamb->f_disable, plamb->ext_disable, 
     				(int *)&(plamb->mask_disable), &q);
     else if (pfunc->disableSlotLam != NULL)
            pfunc->disableSlotLam (pfunc, plamb->slot);
   }/*end disable LAM*/

}/*end cclm()*/

/************************************************************************/
/* cclc() -- Clear LAM							*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void cclc (int lam)
{
   register lam_block  *plamb;	/* Pointer to lam_block		*/
   int                  q;	/* Temp for q return		*/ 

  /*---------------------
   * Check for valid LAM id
   */
   plamb = rtn_setupL (lam, "cclc");
   if (plamb == NULL) return;
      
  /*---------------------
   * If LAM identifier is valid, issue the clear
   */ 
   if (plamb->ext_clear) 
       cfsa (plamb->f_clear, plamb->ext_clear, (int *)&(plamb->mask_clear), &q);

   return;

}/*end cclc()*/

/************************************************************************/
/* ctlm() -- Test for LAM						*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void ctlm (int lam, int *l)
{
   register lam_block  *plamb;		/* Pointer to lam_block	*/
   int                  lam_status = 0;	/* LAM status mask	*/

  /*---------------------
   * Check for valid LAM id
   */
   plamb = rtn_setupL (lam, "ctlm");
   if (plamb == NULL) return;
      
  /*---------------------
   * If LAM identifier is valid, perform the test
   */ 
   if (plamb->ext_test) {
      cfsa (plamb->f_test, plamb->ext_test, &lam_status, l);
      if (READ_F(plamb->f_test)) *l = (lam_status & plamb->mask_test) ? 1 : 0;

   } else *l=0;
   
   return;

}/*end ctlm()*/

/************************************************************************/
/* cclnk() -- Link routine to a LAM					*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void cclnk (int lam, FUNCPTR rtn)
{
   register lam_block  *plamb;		/* Pointer to lam_block	*/

  /*---------------------
   * Check for valid LAM id
   */
   plamb = rtn_setupL (lam, "ctlnk");
   if (plamb == NULL) return;
      
  /*---------------------
   * Set the routine as the callback
   */
   plamb->callback = rtn;
   
   return;

}/*end ctlnk()*/

/************************************************************************/
/* cfsa () -- Single-Action CAMAC Function (24 bits).			*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void cfsa (int f, int ext, int *dat, int *q)
{
   register branch_structure  *pbranch;	/* Pointer to branch structure	*/
   register hwInfo            *phwInfo;	/* Hardware-specific info	*/
   int                         status;	/* Local status variable	*/

  /*---------------------
   * Check the parameters and lock the branch
   */
   pbranch = rtn_setupS (f, ext, "cfsa");
   if (pbranch == NULL) return;

  /*---------------------
   * Issue the command
   */
   phwInfo = pbranch->phwInfo;
   status = hw_cfsa (f, ext, phwInfo, dat, q);
   unlock_branch (pbranch);

  /*---------------------
   * Set the status and return
   */
   if (!*q) {
      if (status == OK) status = S_camacLib_noQ;
   }/*end if no Q*/

   if (status != OK) errno = status;
   return;

}/*end cfsa()*/

/************************************************************************/
/* cssa () -- Single-Action CAMAC Function (16 bits).			*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void cssa (int f, int ext, short *dat, int *q)
{
   register branch_structure  *pbranch;	/* Pointer to branch structure	*/
   register hwInfo            *phwInfo;	/* Hardware-specific info	*/
   int                         status;	/* Local status variable	*/

  /*---------------------
   * Check the parameters and lock the branch
   */
   pbranch = rtn_setupS (f, ext, "cssa");
   if (pbranch == NULL) return;

  /*---------------------
   * Issue the command
   */
   phwInfo = pbranch->phwInfo;
   status = hw_cssa (f, ext, phwInfo, dat, q);
   unlock_branch (pbranch);

  /*---------------------
   * Set the status and return
   */
   if (!*q) {
      if (status == OK) status = S_camacLib_noQ;
   }/*end if no Q*/
   if (status != OK) errno = status;

   return;

}/*end cssa()*/

/************************************************************************/
/* cfga() -- CAMAC General purpose, multiple action (24 bit)		*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void cfga (int fa[], int exta[], int intc[], int qa[], int cb[4])
{
   int                b;		/* Branch number		*/
   int                i=0;		/* Loop counter			*/
   branch_structure  *pbranch;		/* Pointer to branch structure	*/
   register hwInfo   *phwInfo;		/* Pointer to branch hw info	*/
   char              *rtnName="cfga";	/* Name of this routine		*/
   int                status;		/* Local status variable	*/

  /*---------------------
   * Validate the arguments
   */
   pbranch = rtn_setupG (exta[0], &b, cb, rtnName);
   if (pbranch == NULL) return;
   phwInfo = pbranch->phwInfo;

  /*---------------------
   * Wait for LAM (if specified)
   */
   status = wait_lam (cb, rtnName);

  /*---------------------
   * Lock the branch while we perform the list of CAMAC functions
   */
   if (status == OK) status = lock_branch (pbranch, rtnName);

  /*---------------------
   * Loop to execute the array of CAMAC commands
   */
   if (status == OK) {
      for (i=0; i < cb[0]; i++) {

         /* Make sure the commands are all on the same branch */
         if (b != hw_extBranch(exta[i])) {
            status = S_camacLib_MultiBranchG;
            break;
         }/*end if different branch*/

         /* Validate the other parameters */
         status = check_fcna (fa[i], exta[i], rtnName);
         if (status != OK) break;

         /* Issue the next command in the list */
         status = hw_cfsa (fa[i], exta[i], phwInfo, &intc[i], &qa[i]);
         if (status != OK) break;

      }/*end for each command*/

     /*---------------------
      * Unlock the branch and store the operation counter
      */
      unlock_branch (pbranch);
      cb[1] = i;
   }/*end if OK to execute loop*/

  /*---------------------
   * Check the Q and the status of the last command
   */
   if ((status == OK) && (i > 0) && !qa[i-1]) status = S_camacLib_noQ;
   if ((status != OK) && (status != ERROR)) errno = status;

   return;

}/*end cfga()*/

/************************************************************************/
/* csga() -- CAMAC General purpose, multiple action (16 bit)		*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void csga (int fa[], int exta[], short intc[], int qa[], int cb[4])
{
   int                b;		/* Branch number		*/
   int                i=0;		/* Loop counter			*/
   branch_structure  *pbranch;		/* Pointer to branch structure	*/
   register hwInfo   *phwInfo;		/* Pointer to branch hw info	*/
   char              *rtnName="csga";	/* Name of this routine		*/
   int                status;		/* Local status variable	*/

  /*---------------------
   * Validate the arguments
   */
   pbranch = rtn_setupG (exta[0], &b, cb, rtnName);
   if (pbranch == NULL) return;
   phwInfo = pbranch->phwInfo;

  /*---------------------
   * Wait for LAM (if specified)
   */
   status = wait_lam (cb, rtnName);

  /*---------------------
   * Lock the branch while we perform the list of CAMAC functions
   */
   if (status == OK) status = lock_branch (pbranch, rtnName);

  /*---------------------
   * Loop to execute the array of CAMAC commands
   */
   if (status == OK) {
      for (i=0; i < cb[0]; i++) {

         /* Make sure the commands are all on the same branch */
         if (b != hw_extBranch(exta[i])) {
            status = S_camacLib_MultiBranchG;
            break;
         }/*end if different branch*/

         /* Validate the other parameters */
         status = check_fcna (fa[i], exta[i], rtnName);
         if (status != OK) break;

         /* Issue the next command in the list */
         status = hw_cssa (fa[i], exta[i], phwInfo, &intc[i], &qa[i]);
         if (status != OK) break;

      }/*end for each command*/

     /*---------------------
      * Unlock the branch and store the operation counter
      */
      unlock_branch (pbranch);
      cb[1] = i;
   }/*end if OK to execute loop*/

  /*---------------------
   * Check the Q and the status of the last command
   */
   if ((status == OK) && (i > 0) && !qa[i-1]) status = S_camacLib_noQ;
   if ((status != OK) && (status != ERROR)) errno = status;

   return;

}/*end csga()*/

/************************************************************************/
/* cfmad () -- 24-bit Address scan					*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void cfmad (int f, int extb[2], int *intc, int cb[4])
{
   branch_structure   *pbranch;		/* Pointer to branch structure	*/
   register hwInfo    *phwInfo;		/* Hardware-specific info	*/
   char               *rtnName="cfmad";	/* Name of this routine		*/
   int                 status;		/* Local status variable	*/

  /*---------------------
   * Check the arguments
   */
   pbranch = rtn_setupB (f, extb[0], cb, rtnName);
   if (pbranch == NULL) return;

   /* Validate the ending CAMAC channel value */
   status = check_fcna (0, extb[1], rtnName);
   if (status != OK) return;

   /* Check the range on the starting and ending CAMAC channels */
   status = hw_checkExtb (extb, rtnName);

  /*---------------------
   * Wait for LAM (if specified) before starting the operation
   */
   if (status == OK) status = wait_lam (cb, rtnName);

  /*---------------------
   * Lock the branch and perform the operation
   */
   if (status == OK) {
      phwInfo = pbranch->phwInfo;
      status = lock_branch (pbranch, rtnName);
      if (status == OK) status = hw_cfmad (f, extb, phwInfo, intc, cb);
      unlock_branch (pbranch);
   }/*end if it is ok to perform the operation*/

  /*---------------------
   * Check for errors and return
   */
   if ((status != OK) && (status != ERROR)) errno = status;
   return;

}/*end cfmad()*/

/************************************************************************/
/* csmad () -- 16-bit Address scan					*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void csmad (int f, int extb[2], short *intc, int cb[4])
{
   branch_structure   *pbranch;		/* Pointer to branch structure	*/
   register hwInfo    *phwInfo;		/* Hardware-specific info	*/
   char               *rtnName="csmad";	/* Name of this routine		*/
   int                 status;		/* Local status variable	*/

  /*---------------------
   * Check the arguments
   */
   pbranch = rtn_setupB (f, extb[0], cb, rtnName);
   if (pbranch == NULL) return;

   /* Validate the ending CAMAC channel value */
   status = check_fcna (0, extb[1], rtnName);
   if (status != OK) return;

   /* Check the range on the starting and ending CAMAC channels */
   status = hw_checkExtb (extb, rtnName);

  /*---------------------
   * Wait for LAM (if specified) before starting the operation
   */
   if (status == OK) status = wait_lam (cb, rtnName);

  /*---------------------
   * Lock the branch and perform the operation
   */
   if (status == OK) {
      phwInfo = pbranch->phwInfo;
      status = lock_branch (pbranch, rtnName);
      if (status == OK) status = hw_csmad (f, extb, phwInfo, intc, cb);
      unlock_branch (pbranch);
   }/*end if it is ok to perform the operation*/

  /*---------------------
   * Check for errors and return
   */
   if ((status != OK) && (status != ERROR)) errno = status;
   return;

}/*end csmad()*/

/************************************************************************/
/* cfubc () -- 24-bit Repeat Until No Q					*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void cfubc (int f, int ext, int *intc, int cb[4])
{
   branch_structure   *pbranch;		/* Pointer to branch structure	*/
   register hwInfo    *phwInfo;		/* Hardware-specific info	*/
   char               *rtnName="cfubc";	/* Name of this routine		*/
   int                 status;		/* Local status variable	*/

  /*---------------------
   * Check the arguments
   */
   pbranch = rtn_setupB (f, ext, cb, rtnName);
   if (pbranch == NULL) return;

  /*---------------------
   * Wait for LAM (if specified) before starting the operation
   */
   status = wait_lam (cb, rtnName);

  /*---------------------
   * Lock the branch and perform the operation
   */
   if (status == OK) {
      phwInfo = pbranch->phwInfo;
      status = lock_branch (pbranch, rtnName);
      if (status == OK) status = hw_cfubc (f, ext, phwInfo, intc, cb);
      unlock_branch (pbranch);
   }/*end if it is ok to perform the operation*/

  /*---------------------
   * Check for errors and return
   */
   if ((status != OK) && (status != ERROR)) errno = status;
   return;

}/*end cfubc()*/

/************************************************************************/
/* csubc () -- 16-bit Repeat Until No Q					*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void csubc (int f, int ext, short *intc, int cb[4])
{
   branch_structure   *pbranch;		/* Pointer to branch structure	*/
   register hwInfo    *phwInfo;		/* Hardware-specific info	*/
   char               *rtnName="csubc";	/* Name of this routine		*/
   int                 status;		/* Local status variable	*/

  /*---------------------
   * Check the arguments
   */
   pbranch = rtn_setupB (f, ext, cb, rtnName);
   if (pbranch == NULL) return;

  /*---------------------
   * Wait for LAM (if specified) before starting the operation
   */
   status = wait_lam (cb, rtnName);

  /*---------------------
   * Lock the branch and perform the operation
   */
   if (status == OK) {
      phwInfo = pbranch->phwInfo;
      status = lock_branch (pbranch, rtnName);
      if (status == OK) status = hw_csubc (f, ext, phwInfo, intc, cb);
      unlock_branch (pbranch);
   }/*end if it is ok to perform the operation*/

  /*---------------------
   * Check for errors and return
   */
   if ((status != OK) && (status != ERROR)) errno = status;
   return;

}/*end csubc()*/

/************************************************************************/
/* cfubr () -- 24-bit Repeat Until Q					*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void cfubr (int f, int ext, int *intc, int cb[4])
{
   branch_structure   *pbranch;		/* Pointer to branch structure	*/
   register hwInfo    *phwInfo;		/* Hardware-specific info	*/
   char               *rtnName="cfubr";	/* Name of this routine		*/
   int                 status;		/* Local status variable	*/

  /*---------------------
   * Check the arguments
   */
   pbranch = rtn_setupB (f, ext, cb, rtnName);
   if (pbranch == NULL) return;

  /*---------------------
   * Wait for LAM (if specified) before starting the operation
   */
   status = wait_lam (cb, rtnName);

  /*---------------------
   * Lock the branch and perform the operation
   */
   if (status == OK) {
      phwInfo = pbranch->phwInfo;
      status = lock_branch (pbranch, rtnName);
      if (status == OK) status = hw_cfubr (f, ext, phwInfo, intc, cb);
      unlock_branch (pbranch);
   }/*end if it is ok to perform the operation*/

  /*---------------------
   * Check for errors and return
   */
   if ((status != OK) && (status != ERROR)) errno = status;
   return;

}/*end cfubr()*/

/************************************************************************/
/* csubr () -- 16-bit Repeat Until Q					*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void csubr (int f, int ext, short *intc, int cb[4])
{
   branch_structure   *pbranch;		/* Pointer to branch structure	*/
   register hwInfo    *phwInfo;		/* Hardware-specific info	*/
   char               *rtnName="csubr";	/* Name of this routine		*/
   int                 status;		/* Local status variable	*/

  /*---------------------
   * Check the arguments
   */
   pbranch = rtn_setupB (f, ext, cb, rtnName);
   if (pbranch == NULL) return;

  /*---------------------
   * Wait for LAM (if specified) before starting the operation
   */
   status = wait_lam (cb, rtnName);

  /*---------------------
   * Lock the branch and perform the operation
   */
   if (status == OK) {
      phwInfo = pbranch->phwInfo;
      status = lock_branch (pbranch, rtnName);
      if (status == OK) status = hw_csubr (f, ext, phwInfo, intc, cb);
      unlock_branch (pbranch);
   }/*end if it is ok to perform the operation*/

  /*---------------------
   * Check for errors and return
   */
   if ((status != OK) && (status != ERROR)) errno = status;
   return;

}/*end csubr()*/

/************************************************************************/
/* ctstat() -- Return status value from last operation			*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void ctstat (int *k)
{
   register int  status;

   status = errno;
   if ((status & 0xffff0000) == M_camacLib) *k = status;
   else *k = OK;

}/*end ctstat()*/

/************************************************************************/
/* cccc() -- Issue dataway C to crate					*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void cccc (int ext)
{
   register branch_structure  *pbranch;	/* Pointer to branch structure	*/
   register crate_structure   *pcrate;	/* Pointer to crate structure	*/
   register crateFuncBlock    *pfunc;	/* Pointer to crate functions	*/
   register hwInfo            *phwInfo;	/* Hardware-specific info	*/
   int                         status;	/* Local status variable	*/

  /*---------------------
   * Check the parameters, lock the branch, and get the crate structure
   */
   pcrate = rtn_setupC (ext, "cccc");
   if (pcrate == NULL) return;

  /*---------------------
   * Extract the other structures from the crate structure
   */
   pfunc = &pcrate->func;	/* Crate function block		*/
   pbranch = pcrate->branch;	/* Branch structure		*/
   phwInfo = pbranch->phwInfo;	/* Hardware information block	*/

  /*---------------------
   * Invoke the branch/hardware specific routine to issue the C.
   */
   status = hw_cccc (pfunc, phwInfo);
   unlock_branch (pbranch);

  /*---------------------
   * Set status code and return.
   */
   if (status != OK) errno = status;
   return;

}/*end cccc()*/

/************************************************************************/
/* cccz() -- Issue dataway Z to crate					*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void cccz (int ext)
{
   register branch_structure  *pbranch;	/* Pointer to branch structure	*/
   register crate_structure   *pcrate;	/* Pointer to crate structure	*/
   register crateFuncBlock    *pfunc;	/* Pointer to crate functions	*/
   register hwInfo            *phwInfo;	/* Hardware-specific info	*/
   int                         status;	/* Local status variable	*/

  /*---------------------
   * Check the parameters, lock the branch, and get the crate structure
   */
   pcrate = rtn_setupC (ext, "cccz");
   if (pcrate == NULL) return;

  /*---------------------
   * Extract the other structures from the crate structure
   */
   pfunc = &pcrate->func;	/* Crate function block		*/
   pbranch = pcrate->branch;	/* Branch structure		*/
   phwInfo = pbranch->phwInfo;	/* Hardware information block	*/

  /*---------------------
   * Invoke the branch/hardware specific routine to issue the Z.
   */
   status = hw_cccz (pfunc, phwInfo);
   unlock_branch (pbranch);

  /*---------------------
   * Set status code and return.
   */
   if (status != OK) errno = status;
   return;

}/*end cccz()*/

/************************************************************************/
/* cccd() -- Set/Clear LAM enable for crate				*/
/*	o Invoke branch/hardware specific routine to enable/disable LAM	*/
/*	o Check for external crate or LAM grader routine and invoke	*/
/*	  if present.							*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void cccd (int ext, int l)
{
   camacCrateLam              *extRtn;	/* Ext rtn for LAM grader support */
   register branch_structure  *pbranch;	/* Pointer to branch structure	  */
   register crate_structure   *pcrate;	/* Pointer to crate structure	  */
   register crateFuncBlock    *pfunc;	/* Pointer to crate functions	  */
   register hwInfo            *phwInfo;	/* Hardware-specific info	  */
   int                         status;	/* Local status variable	  */

  /*---------------------
   * Check the parameters, lock the branch, and get the crate structure
   */
   pcrate = rtn_setupC (ext, "cccd");
   if (pcrate == NULL) return;

  /*---------------------
   * Extract the other structures from the crate structure
   */
   pfunc = &pcrate->func;	/* Crate function block		*/
   pbranch = pcrate->branch;	/* Branch structure		*/
   phwInfo = pbranch->phwInfo;	/* Hardware information block	*/

  /*---------------------
   * Invoke the branch/hardware specific routine to enable/disable LAMs
   */
   status = hw_cccd (pfunc, phwInfo, l);
   pcrate->lams_enabled = l;
   pcrate->lam_init_flag = TRUE;
   unlock_branch (pbranch);

  /*---------------------
   * Quit now if there was an error in the hardware routine
   */
   if (status != OK) {
      errno = status;
      return;
   }/*end if error in hardware routine*/

  /*---------------------
   * Check for any external crate or LAM grader support routines that
   * also need invoking.
   */
   extRtn = (l ? pfunc->enableGrader : pfunc->disableGrader);
   if (extRtn != NULL) extRtn (pfunc);

   return;

}/*end cccd()*/

/************************************************************************/
/* ctcd() -- Test for LAM enable on crate controller			*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void ctcd (int ext, int *l)
{
   register branch_structure  *pbranch;	/* Pointer to branch structure	  */
   register crate_structure   *pcrate;	/* Pointer to crate structure	  */
   register crateFuncBlock    *pfunc;	/* Pointer to crate functions	  */
   register hwInfo            *phwInfo;	/* Hardware-specific info	  */
   int                         status;	/* Local status variable	  */

  /*---------------------
   * Check the parameters, lock the branch, and get the crate structure
   */
   pcrate = rtn_setupC (ext, "ctcd");
   if (pcrate == NULL) return;

  /*---------------------
   * Extract the other structures from the crate structure
   */
   pfunc = &pcrate->func;	/* Crate function block		*/
   pbranch = pcrate->branch;	/* Branch structure		*/
   phwInfo = pbranch->phwInfo;	/* Hardware information block	*/

  /*---------------------
   * Invoke the branch/hardware specific routine to test for LAM enable
   */
   status = hw_ctcd (pfunc, phwInfo, l);
   unlock_branch (pbranch);

  /*---------------------
   * Set status code and return.
   */
   if (status != OK) errno = status;
   return;

}/*end ctcd()*/

/************************************************************************/
/* ctgl() -- Test crate controller for presence of LAM			*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void ctgl (int ext, int *l)
{
   register branch_structure  *pbranch;	/* Pointer to branch structure	  */
   register crate_structure   *pcrate;	/* Pointer to crate structure	  */
   register crateFuncBlock    *pfunc;	/* Pointer to crate functions	  */
   register hwInfo            *phwInfo;	/* Hardware-specific info	  */
   int                         status;	/* Local status variable	  */

  /*---------------------
   * Check the parameters, lock the branch, and get the crate structure
   */
   pcrate = rtn_setupC (ext, "ctgl");
   if (pcrate == NULL) return;

  /*---------------------
   * Extract the other structures from the crate structure
   */
   pfunc = &pcrate->func;	/* Crate function block		*/
   pbranch = pcrate->branch;	/* Branch structure		*/
   phwInfo = pbranch->phwInfo;	/* Hardware information block	*/

  /*---------------------
   * Invoke the branch/hardware specific routine to test for LAM present
   */
   status = hw_ctgl (pfunc, phwInfo, l);
   unlock_branch (pbranch);

  /*---------------------
   * Set status code and return.
   */
   if (status != OK) errno = status;
   return;

}/*end ctgl()*/

/************************************************************************/
/* ccci() -- Set/Clear dataway Inhibit (I) for crate			*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void ccci (int ext, int l)
{
   register branch_structure  *pbranch;	/* Pointer to branch structure	  */
   register crate_structure   *pcrate;	/* Pointer to crate structure	  */
   register crateFuncBlock    *pfunc;	/* Pointer to crate functions	  */
   register hwInfo            *phwInfo;	/* Hardware-specific info	  */
   int                         status;	/* Local status variable	  */

  /*---------------------
   * Check the parameters, lock the branch, and get the crate structure
   */
   pcrate = rtn_setupC (ext, "ccci");
   if (pcrate == NULL) return;

  /*---------------------
   * Extract the other structures from the crate structure
   */
   pfunc = &pcrate->func;	/* Crate function block		*/
   pbranch = pcrate->branch;	/* Branch structure		*/
   phwInfo = pbranch->phwInfo;	/* Hardware information block	*/

  /*---------------------
   * Invoke the branch/hardware specific routine to set or clear inhibit
   */
   status = hw_ccci (pfunc, phwInfo, l);
   unlock_branch (pbranch);

  /*---------------------
   * Set status code and return.
   */
   if (status != OK) errno = status;
   return;

}/*end ccci()*/

/************************************************************************/
/* ctci() -- Test dataway inhibit on crate controller			*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void ctci (int ext, int *l)
{
   register branch_structure  *pbranch;	/* Pointer to branch structure	  */
   register crate_structure   *pcrate;	/* Pointer to crate structure	  */
   register crateFuncBlock    *pfunc;	/* Pointer to crate functions	  */
   register hwInfo            *phwInfo;	/* Hardware-specific info	  */
   int                         status;	/* Local status variable	  */

  /*---------------------
   * Check the parameters, lock the branch, and get the crate structure
   */
   pcrate = rtn_setupC (ext, "ctci");
   if (pcrate == NULL) return;

  /*---------------------
   * Extract the other structures from the crate structure
   */
   pfunc = &pcrate->func;	/* Crate function block		*/
   pbranch = pcrate->branch;	/* Branch structure		*/
   phwInfo = pbranch->phwInfo;	/* Hardware information block	*/

  /*---------------------
   * Invoke the branch/hardware specific routine to test inhibit
   */
   status = hw_ctci (pfunc, phwInfo, l);
   unlock_branch (pbranch);

  /*---------------------
   * Set status code and return.
   */
   if (status != OK) errno = status;
   return;

}/*end ctci()*/

/*======================================================================*/
/*	Driver, Branch, and Crate Initialization Routines		*/
/*									*/


/************************************************************************/
/* camacLibInit () -- Initialize the CAMAC driver			*/
/*	o Initialize the LAM message queue and spawn the LAM monitor	*/
/*	  task.								*/
/*	o Initialize all branch/serial highways.			*/
/*	o Spawn the error monitor task.					*/
/*									*/
/* This routine is called at boot time by iocInit.			*/
/*									*/
/************************************************************************/

GLOBAL_RTN
long int  camacLibInit (void)
{
   int  i;		/* Loop counter			*/
   int  status;		/* Local status variable	*/

   /* If we have already been called, then just return  */
   if (camacLib_init) return OK;

  /*---------------------
   * Set parameters for LAM timeouts
   */
   clock_rate = sysClkRateGet ();
   max_timeout = 0x7fffffff / clock_rate;
   min_timeout = (1000 + clock_rate/2) / clock_rate;

  /*---------------------
   * Initialize the message queue for the LAM monitor task
   */
   lam_msg_q = msgQCreate (
		MAX_LAM_MSG,		/* Maximum messages to queue	*/
		sizeof (lam_message),	/* Maximum message length	*/
		MSG_Q_FIFO);		/* Process queue in FIFO order	*/

   if (lam_msg_q == NULL) {
      logMsg ("%s: Unable to create message queue for LAM monitor task\n",
              (int)drvName, 0, 0, 0, 0, 0);
      return ERROR;
   }/*end if can't create message queue*/

  /*---------------------
   * Spawn the LAM monitor Task
   */
   status = taskSpawn (
            "lamMonitor",		/* Name = "lamMonitor"		*/
            PRIO_LAM_MON,		/* Task Priority		*/
            0,				/* No option flags		*/
            2000,			/* Stack size = 2000 bytes	*/
            (FUNCPTR) camacLamMonitor,	/* Entry point			*/
            0, 0, 0, 0, 0,		/* No arguments			*/
            0, 0, 0, 0, 0);

   if (status == ERROR) {
      logMsg ("%s: Unable to spawn LAM monitor task\n",
             (int)drvName, 0, 0, 0, 0, 0);
      return ERROR;
   }/*end if unable to spawn LAM monitor task*/

  /*---------------------
   * Initialize the branch pointer array and all branch/serial highways
   */
   for (i=0; i <= MAX_BRANCH; i++) {
      branch[i] = NULL;			/* Init branch structure pointers */
      ccinit (i);			/* Init hardware (if present)	  */
   }/*end for each branch*/

  /*---------------------
   * Spawn the error monitor Task
   */
   status = taskSpawn (
           "camErrMon",			/* Name = "camErrMon"		*/
            PRIO_ERR_MON,		/* Task Priority		*/
            VX_FP_TASK,			/* Task uses floating point	*/
            2000,			/* Stack size = 2000 bytes	*/
            (FUNCPTR) camacErrorMonitor,/* Entry point			*/
            0, 0, 0, 0, 0,		/* No arguments			*/
            0, 0, 0, 0, 0);

   if (status == ERROR) {
      logMsg ("%s: Unable to spawn error monitor task\n",
             (int)drvName, 0, 0, 0, 0, 0);
      return ERROR;
   }/*end if unable to spawn error monitor task*/

  /*---------------------
   * Set initialization flag and return
   */
   camacLib_init = TRUE;
   return OK;

}/*end camacLibInit()*/

/************************************************************************/
/* camacDeclareInitRtn () -- Declare a LAM-grader or crate init routine	*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void   camacDeclareInitRtn (camacInitRtn *initRtn, int b, int c, int n)
{
   branch_structure  *pbranch;		/* Pointer to branch structure	*/
   crate_structure   *pcrate;		/* Pointer to crate structure	*/
   init_block        *pinit;		/* Pointer to new init_block	*/

  /*---------------------
   * Make sure branch and crate are in range
   */
   if (!LEGAL_BRANCH(b)) {
      logMsg ("camacDeclareInitRtn:  Invalid branch number (%d)\n",
               b, 0, 0, 0, 0, 0);
      return;
    }/*end if branch out of range*/

   if (!LEGAL_CRATE(c)) {
      logMsg ("camacDeclareInitRtn:  Invalid crate number (%d)\n",
               c, 0, 0, 0, 0, 0);
      return;
   }/*end if crate out of range*/

  /*---------------------
   * Allocate and initialize the init_block
   */
   pinit = calloc (1, sizeof(init_block));
   if (pinit == NULL) return;

   pinit->initRtn = initRtn;
   pinit->b = b;
   pinit->c = c;
   pinit->n = n;

  /*---------------------
   * Add this block to the end of the init_block list.
   * This guarantees that initialization routines will be called
   * in the order they were declared.
   */
   last_init_block->link = pinit;
   last_init_block = pinit;

  /*---------------------
   * If the CAMAC driver has not been initialized yet, or if there is
   * no hardware for this branch, return now.
   */
   if (!camacLib_init) return;
   pbranch = branch[b];
   if (pbranch == NULL) return;

  /*---------------------
   * If the crate has not been initialized yet, initialize it now.
   * Note that crate initialization will also invoke the external
   * initialization routines.
   */
   pcrate = pbranch->crate[c];
   if (pcrate == NULL) {
      camacCrateInit (b, c);
      return;
   }/*end if crate not already initialized*/

  /*---------------------
   * If the crate has already been initialized, invoke the external
   * initialization routine now.
   */
   initRtn (&pcrate->func, b, c, n);

}/*end camacDeclareInitRtn()*/

/************************************************************************/
/* camacCrateInit () -- Initialize a crate c in branch b.		*/
/*	o Allocate and initialize a crate structure if one does		*/
/*	  not already exist.						*/
/*	o Invoke hardware-specific crate initialization			*/
/*	o Invoke any external LAM-grader or crate init routines		*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void camacCrateInit (int b, int c)
{
   int                ext;	/* Local CAMAC channel		*/
   int                n;	/* Slot index (for loop)	*/
   branch_structure  *pbranch;	/* Pointer to branch structure	*/
   crate_structure   *pcrate;	/* Pointer to crate structure	*/
   register hwInfo   *phwInfo;	/* Pointer to hardware info	*/
   init_block        *pinit;	/* Pointer to init block	*/
   slot_structure    *slot;	/* Pointer to slot array	*/
   int                status;	/* Local status variable	*/

  /*---------------------
   * Check for branch in range
   */
   if (!LEGAL_BRANCH(b)) {
      logMsg ("%s: Routine camacCrateInit -- Invalid branch number - %d\n",
             (int)drvName, b, 0, 0, 0, 0);
      return;
   }/*end if invalid branch number*/

  /*---------------------
   * Check for branch hardware present
   */
   pbranch = branch[b];
   if (pbranch == NULL) { 
      logMsg ("%s: Routine camacCrateInit -- No hardware initialized for branch %d\n",
             (int)drvName, b, 0, 0, 0, 0);
      return;
   }/*end if no hardware present*/

  /*---------------------
   * Check for crate in range
   */
   if (!LEGAL_CRATE(c)) {
      logMsg ("%s: Routine camacCrateInit -- Invalid crate number - %d\n",
             (int)drvName, c, 0, 0, 0, 0);
      return;
   }/*end if invalid crate number*/

  /*---------------------
   * Lock the branch
   */
   status = lock_branch (pbranch, "camacCrateInit");
   if (status != OK) return;

  /*---------------------
   * Allocate and initialize the crate structure if we haven't already
   * done so.
   */
   pcrate = pbranch->crate[c];
   if (pcrate == NULL) {

      pbranch->crate[c] = pcrate = calloc (1, sizeof(crate_structure));
      if (pcrate == NULL) {
         logMsg ("%s: Routine camacCrateInit -- Insufficient memory to initialize crate %d, branch %d\n",
                (int)drvName, c, b, 0, 0, 0);
         unlock_branch (pbranch);
         return;
      }/*end if not enough memory*/

     /*---------------------
      * Initialize the crate structure
      */
      pcrate->c = c;				/* Crate number		     */
      pcrate->b = b;				/* Branch number	     */
      pcrate->branch = pbranch;			/* Branch structure	     */
      hw_initFuncBlock (&pcrate->func, b, c);	/* Init crate function block */

     /*---------------------
      * Create a Mutex to protect the LAM list
      */
      pcrate->lam_mutex = semMCreate (		/* LAM list mutex	     */
         SEM_Q_PRIORITY     | 			/*    use priority queueing  */
         SEM_INVERSION_SAFE |			/*    safe from prio invert  */
         SEM_DELETE_SAFE);			/*    safe from task delete  */

      if (pcrate->lam_mutex == NULL) {
         logMsg ("%s: Routine camacCrateInit -- Unable to create LAM mutex for crate %d, branch %d\n",
                (int)drvName, c, b, 0, 0, 0);
         free (pcrate);
         pbranch->crate[c] = NULL;
         unlock_branch (pbranch);
         return;
      }/*end if can't allocate lam mutex*/

   }/*end if crate structure not allocated*/

  /*---------------------
   * Initialize the crate controller hardware
   */
   phwInfo = pbranch->phwInfo;
   status = hw_crateInit (&pcrate->func, phwInfo);

  /*---------------------
   * Report on whether or not the crate was successfully initialized.
   */
   if (status != OK) {
      logMsg ("%s: Routine camacCrateInit -- Hardware error initializing crate %d, branch %d\n %x",
             (int)drvName, c, b, status, 0, 0);
   }/*end if error initializing crate*/

   else /* Crate initiialized */ {
      logMsg ("%s: Successfully Initialized Crate %d on Branch %d\n",
             (int)drvName, c, b, 0, 0, 0);
   }/*end if crate initialized*/

  /*---------------------
   * Search for any external LAM-grader or crate initialization routines.
   * External initialization routines are invoked in the order in which
   * they were declared.
   */
   for (pinit=init_block_list; pinit!=NULL; pinit=pinit->link) {
      if ((pinit->b == b) && (pinit->c == c))
         pinit->initRtn (&pcrate->func, b, c, pinit->n);
   }/*end for each init block on the list*/

  /*---------------------
   * Search for any registered card initialization routines
   */
   slot = pcrate->slot;
   for (n=0;  n < MAX_NORML_SLOT;  n++) {
      if (slot[n].slotInit != NULL)
         slot[n].slotInit (slot[n].slotInitParm);
   }/*end for each slot*/

  /*---------------------
   * Check to see if LAMs should be enabled on this crate
   */
   if (pcrate->lams_enabled) {
      cdreg (&ext, b, c, 1, 0);
      cccd (ext, TRUE);
   }/*end if LAMs should be enabled*/

  /*---------------------
   * Unlock the branch and return
   */
   unlock_branch (pbranch);

}/*end camacCrateInit()*/

/*======================================================================*/
/*			LAM Interrupt Handling Routines			*/
/*									*/


/************************************************************************/
/* camacLamInt () -- Handle LAM interrupts				*/
/*	o This routine is called by the hardware-specific interrupt	*/
/*	  service routine to process a LAM interrupt.  It runs at	*/
/*	  interrupt level.						*/
/*									*/
/************************************************************************/

LOCAL_RTN
void camacLamInt (int b, int c, int n)
{
   lam_message  msg;		/* LAM message to send monitor task	*/
   int          status;		/* Local status variable		*/

  /*---------------------
   * Construct a LAM message to send to the LAM monitor
   */
   msg.branch = b;		/* set branch number */
   msg.crate  = c;		/* set crate number  */
   msg.slot   = n;		/* set slot number   */
   
  /*---------------------
   * Wake up the LAM monitor task
   */
   status = msgQSend (		/* Queue message to LAM monitor task	*/
            lam_msg_q,		/*    Message queue id			*/
            (char *)&msg,	/*    Address of message		*/
            sizeof msg,		/*    Number of bytes to send		*/
            NO_WAIT,		/*    Don't block if queue is full	*/
            MSG_PRI_NORMAL);	/*    FIFO processing			*/

}/*end camacLamInt()*/

/************************************************************************/
/* camacLamMonitor () -- LAM Monitor Task				*/
/*									*/
/************************************************************************/

LOCAL_RTN
void camacLamMonitor (void)
{
   int                  lam_mask;	/* LAM mask read from SCC	  */
   int                  mask;		/* Mask used to test SCC lam mask */
   lam_message          msg;		/* Message from interrupt rtn.	  */
   int                  n;		/* Slot number of LAM source	  */
   crate_structure     *pcrate;		/* Pointer to crate structure	  */
   crateFuncBlock      *pfunc;		/* Pointer to crate function block*/
   int                  status;		/* Local status variable	  */

  /*---------------------
   * Loop forever waiting for messages from the interrupt service routine
   */
   FOREVER {

     /*---------------------
      * Get next message from interrupt service routine
      */
      status = msgQReceive (
               lam_msg_q,	/* Message queue id		*/
               (char *)&msg,	/* Address of message buffer	*/
               sizeof msg,	/* Number of bytes to read	*/
               WAIT_FOREVER);	/* Don't time out		*/

     /*---------------------
      * Abort the task if we get an error reading the message queue
      */
      if (status == ERROR) {
         logMsg ("%s: Routine camacLamMonitor -- Unable to read message queue\n",
                (int)drvName, 0, 0, 0, 0, 0);
         return;
      }/*if can't read message queue*/

      
     /*---------------------
      * Check for legal crate.
      * Get address of crate structure and function block.
      */
      if (!LEGAL_CRATE(msg.crate)) continue;
      pcrate = branch[msg.branch]->crate[msg.crate];
      pfunc = &(pcrate->func);

     /*---------------------
      * If the crate has not been initialized, initialize it now.  Note
      * that the crate initialization routine should disable LAMs on this
      * crate until the first LAM variable is declared for it.
      */
      if (pcrate == NULL) {
         camacCrateInit (msg.branch, msg.crate);
         continue;
      }/*end if crate not initialized*/

     /*---------------------
      * If the message contains a valid slot number, than just process
      * LAMs from that one slot.
      */
      else if ((msg.slot > 0) && (msg.slot <= MAX_NORML_SLOT)) {
         if (pfunc->ackSlotLam != NULL) pfunc->ackSlotLam (pfunc, msg.slot);
         deliver_LAMs (pcrate, msg.slot);
         if (pfunc->enableSlotLam != NULL) pfunc->enableSlotLam (pfunc, msg.slot);
      }/*end if slot number in message*/

     /*---------------------
      * If the message did not contain a valid slot number, read the LAM
      * pattern register and process each slot with a bit set.  After
      * processing, invoke the "resetCrateLam" routine (if present) to
      * reset hung demands or re-enable crate LAMs.
      */
      else {
         lam_mask = hw_getLamPattern (pfunc, pcrate->branch->phwInfo);

         /* Loop to search for all LAM sources at this crate */
         for ((n=1, mask=1); lam_mask; (n++, mask<<=1)) {
            if (lam_mask & mask) {
               deliver_LAMs (pcrate, n); /* Check LAMs in this slot */
               lam_mask ^= mask;	 /* Clear bit in LAM mask   */
	    }/*if slot "n" has LAM source set*/
	 }/*end for each bit set in lam mask*/

         /* Reset the lam-grader or crate (if specified) */
         if (pfunc->resetCrateLam != NULL) pfunc->resetCrateLam (pfunc);

      }/*end if no slot number in message*/

   }/*end FOREVER loop*/
}/*end camacLamMonitor()*/

/************************************************************************/
/* deliver_LAMs () -- Search LAM list and deliver LAMs			*/
/*	o Searches the LAM list of the specified crate and slot and	*/
/*	  "delivers" any pending LAMs that it finds by signalling the	*/
/*	  appropriate "lam_fired" semaphore.				*/
/*	o If no LAM block could be found corresponding to this LAM,	*/
/*	  try to disable all LAMs from this slot.			*/
/*									*/
/************************************************************************/

LOCAL_RTN
void deliver_LAMs (crate_structure *pcrate, int n)
{
   int                  a;		/* Subaddress (for clearing LAM)  */
   int                  b;		/* Branch number		  */
   int                  c;		/* Crate number			  */
   int                  cleared;	/* TRUE if LAM source cleared	  */
   int                  ext_clr;	/* CAMAC variable to clear LAM	  */
   int                  ext_dis;	/* CAMAC variable to disable LAM  */
   int                  dummy;		/* Dummy variable		  */
   int                  mask;		/* Mask for clear/disable	  */
   branch_structure    *pbranch;	/* Pointer to block structure	  */
   crateFuncBlock      *pfunc;		/* Pointer to crate functions	  */
   register hwInfo     *phwInfo;	/* Pointer to hw info block	  */
   register lam_block  *plamb;		/* Pointer to current LAM block	  */
   slot_structure      *pslot;		/* Pointer to slot structure	  */
   char                *rtnName;	/* Name of this routine		  */
   int                  signal;		/* If TRUE, signal the LAM	  */
   int                  status;		/* Local status variable	  */
   int                  test_flag;	/* if TRUE, test for specific LAM */

  /*---------------------
   * Initialization
   */
   pbranch = pcrate->branch;		/* Get branch structure		*/
   pslot = &pcrate->slot[n-1];		/* Get slot structure		*/
   test_flag = (pslot->lam_count > 1);	/* Check for multiple sources	*/
   cleared = FALSE;			/* Set LAM not cleared flag	*/

  /*---------------------
   * Lock the crate's LAM list
   */
   rtnName = "deliver_LAMs";
   status = lock_lam_list (pcrate, rtnName);
   if (status != OK) return;

  /*---------------------
   * Loop through each LAM block attached to this slot
   *
   * If there are multiple LAM sources in this slot,  we will have
   * test each LAM source to determine which ones to signal.
   *
   * Note that if there is only one LAM block in the list, we assume that
   * the card has only one LAM source.
   */
   for (plamb=pslot->lam_list; plamb; plamb=plamb->link) {

      if (test_flag) ctlm ((int)plamb, &signal);
      else signal = TRUE;

      if (signal) {
         cleared = TRUE;			/* Indicate LAM cleared	*/
         cclc ((int)plamb);			/* Clear the LAM source	*/
         if (plamb->callback != NULL) 		/* Call the callback routine */
            plamb->callback(plamb->callback_param);  /* if one is defined */

        /*---------------------
         * If nobody is waiting, and if there is not a callback routine for
         * this LAM, disable the LAM source and record an
         * "Unexpected LAM" error
         */
         if (!(plamb->waiting) && (plamb->callback == NULL)) {
            cclm ((int)plamb, FALSE);
            status = lock_branch (pbranch, rtnName);
            camacRecordError (plamb->ext_clear, ERR_UNEXP_LAM);
            unlock_branch (pbranch);
	 } /*end if nobody waiting for LAM*/

        /*---------------------
         * If somebody is waiting, flush the "lam_fired" semaphore
         */
         else if (plamb->waiting) {
            semFlush (plamb->lam_fired);	/* Release waiting tasks*/
            plamb->waiting = 0;			/* Clear wait count	*/
         }/*end if lam is expected*/
       }/*if we should signal this LAM*/

   }/*end for each lam block*/

  /*---------------------
   * Release the lock on the LAM list
   * Return now if the LAM was cleared.
   */
   unlock_lam_list (pcrate);
   if (cleared) return;

  /*---------------------
   * If the LAM was not cleared in the above loop, it is an "undeclared"
   * (renegade) LAM.  First check to see if there is an external or
   * lam-grader slot disable function defined for this crate.
   */
   pfunc = &pcrate->func;
   if (pfunc->disableSlotLam != NULL) {
      pfunc->disableSlotLam (pfunc, n);	/* Disable LAMs on this slot	*/
      pfunc->lam_mask &= ~(1 << (n-1));	/* Make sure mask bit is clear	*/
      return;				/* Quit now			*/
   }/*end if external slot disable function defined*/

  /*---------------------
   * If there was no external slot disable function, use a shotgun approach
   * to try and make it go away.
   */
   b = pcrate->b;
   c = pcrate->c;
   phwInfo = pbranch->phwInfo;
   
  /*---------------------
   * Lock the branch while we are issuing clear/disable commands
   */
   status = lock_branch (pbranch, rtnName);
   if (status != OK) return;

  /*---------------------
   * Loop to clear and disable all possible "Subaddress" addressed LAM sources
   * from this slot.
   */
   for (a=0; a <= MAX_SUBADDR; a++) {
      hw_cdreg (&ext_clr, b, c, n, a);
      hw_cssa (CLEAR_LAM,   ext_clr, phwInfo, (short *)&dummy, &dummy);
      hw_cssa (DISABLE_LAM, ext_clr, phwInfo, (short *)&dummy, &dummy);
   }/*end for each subaddress*/

  /*---------------------
   * Clear and disable all possible "Mask Register" addressed LAM sources
   * from this slot.
   */
   mask = -1;				/* Select all LAM sources	*/
   hw_cdreg (&ext_clr, b, c, n, 12);	/* LAM source register		*/
   hw_cdreg (&ext_dis, b, c, n, 13);	/* LAM mask register		*/

   hw_cfsa (SELECTIVE_CLEAR, ext_clr, phwInfo, &mask, &dummy);
   hw_cfsa (SELECTIVE_CLEAR, ext_dis, phwInfo, &mask, &dummy);

  /*---------------------
   * Record an "Unexpected LAM" error, unlock the branch, and return
   */
   camacRecordError (ext_clr, ERR_UNEXP_LAM);
   unlock_branch (pbranch);
   return;

}/*end deliver_LAMs()*/

/*======================================================================*/
/*		Routines for Error Reporting and Recording		*/
/*									*/


/************************************************************************/
/* compute_error_rate () -- Compute the Current Rate for a Given Error	*/
/*	o Compute new error rate based on past error rate and current	*/
/*	  error count.							*/
/*	o Reset error count.						*/
/*									*/
/************************************************************************/

INLINE_RTN
void compute_error_rate (error_structure  *pError)
{
   register double   count;		/* Number of errors this period	*/
   register double   rate;		/* New computed error rate	*/

  /*---------------------
   * Get the previous rate and the current count.
   * Note that a negative count indicates overflow.
   */
   rate  = pError->rate;
   count = (long int)pError->current;
   if (count < 0.0) count = LONG_MAX;

  /*---------------------
   * If the error rate has increased, make the new rate reflect the increase.
   * If the error rate is constant or decreasing, compute a weighted average
   * with an exponential decay on the old rate.
   * If the error rate decays below the threshold value, set it back to zero.
   */
   if (count > rate)
      rate = count;
   else
      rate = rate * ERROR_DECAY + count * (1.0 - ERROR_DECAY);

   if (rate < ERROR_RATE_THRESH) rate = 0.0;

  /*---------------------
   * Store the new rate and reset the current count
   */
   pError->rate = rate;
   pError->current = 0;

}/*compute_error_rate()*/

/************************************************************************/
/* format_error () -- Format an Error Report Line			*/
/*	o Create a string with the error text, error rate, and		*/
/*	  total count.							*/
/*	o If total count has overflow condition, return asterisks.	*/
/*	o If error rate has overflow return asterisks for rate and	*/
/*	  total count (note that if the rate has overflown, then the	*/
/*	  total must have overflown too).				*/
/*									*/
/************************************************************************/

LOCAL_RTN
void   format_error (error_structure *pError, char *errText, char *errString)
{

  /*---------------------
   * Check for overflow in rate (total must also be overflown as well)
   */
   if (pError->rate >= 9999999999.994)
      sprintf (errString, "  %-20s%22s%22s",
               errText, "***** (total)", "***** (rate)");
  
  /*---------------------
   * Check for overflow in total only
   */
   else if ((long int)pError->total < 0)
      sprintf (errString, "  %-20s%22s%15.2f (rate)",
               errText, "***** (total)", pError->rate);

  /*---------------------
   * No overflows, print both total and rate
   */
   else
      sprintf (errString, "  %-20s %13ld (total)%15.2f (rate)",
               errText, pError->total, pError->rate);

}/*end format_error()*/

/************************************************************************/
/* print_errors () -- Print Contents of Error Array			*/
/*	o Traverse an error array, printing the text, rate, and total	*/
/*	o Ignore those entries for which there are no errors		*/
/*									*/
/************************************************************************/

LOCAL_RTN
int   print_errors (error_structure *pError, char *errText[], int max_error)
{
   int   count = 0;		/* Number of error lines printed	*/
   char  errString [68];	/* String for error line		*/
   int   i;			/* Index into error array		*/

  /*---------------------
   * Loop through each element of the error array
   */
   for (i=0; i < max_error; (i++, pError++)) {
      if (pError->total) {
         format_error (pError, errText[i], errString);
         printf ("\n%s", errString);
         count++;
      }/*end if error count is not zero*/
   }/*end for each element in the error array*/

  /*---------------------
   * Skip a line after the last error message and return the number of
   * errors printed.
   */
   if (count) printf ("\n");
   return count;

}/*end print_errors()*/

/************************************************************************/
/* camacErrorMonitor () -- Error Monitoring Task			*/
/*									*/
/************************************************************************/

LOCAL_RTN
void   camacErrorMonitor (void)
{
   int                b, c, n, i;	/* Loop counters		   */

   branch_structure  *pBranch;		/* Ptr to current branch structure */
   crate_structure   *pCrate;		/* Ptr to current crate structure  */
   slot_structure    *pSlot;		/* Ptr to current slot structure   */
   error_structure   *pError;		/* Ptr to current error structure  */

   FOREVER {

     /*---------------------
      * Compute the error rates once a second
      */
      taskDelay (clock_rate);

     /*---------------------
      * Loop to find every valid branch in the system
      */
      for (b=0; b <= MAX_BRANCH; b++) {
         pBranch = branch[b];
         if (pBranch != NULL) {

           /*---------------------
            * Found a valid branch, lock the branch and compute
            * the branch error rates.
            */
            lock_branch (pBranch, "camacErrorMonitor");
            pError = pBranch->errors;
            for (i=0; i < MAX_BRANCH_ERR; i++)
               compute_error_rate (pError++);

           /*---------------------
            * Loop to find every known crate on this branch
            */
            for (c=0; c <= MAX_CRATE; c++) {
               pCrate = pBranch->crate[c];
               if (pCrate != NULL) {

                 /*---------------------
                  * Found a valid crate, compute the crate error rates
                  */
                  pSlot = pCrate->slot;
                  pError = pCrate->errors;
                  for (i=0; i < MAX_CRATE_ERR; i++)
                     compute_error_rate (pError++);

                 /*---------------------
                  * Compute slot error rates for every slot in this crate
                  */
                  for (n=0; n < MAX_NORML_SLOT; n++) {
                     pError = pSlot->errors;
                     for (i=0; i < MAX_SLOT_ERR; i++)
                        compute_error_rate (pError++);
                     pSlot++;
                  }/*end for each slot*/

               }/*end if crate exists*/
            }/*end for each crate*/

           /*---------------------
            * Unlock the branch
            */
            unlock_branch (pBranch);

         }/*end if branch exists*/
      }/*end for each branch*/

   }/*end FOREVER loop*/
}/*end camacErrorMonitor*/

/************************************************************************/
/* camacRecordError () -- Record Occurrence of a CAMAC Error		*/
/*	o Determine whether it is a branch, crate, or slot error	*/
/*	o Increment error counter and totals in approprate structure	*/
/*									*/
/* Note: This routine expects the branch to be locked when it is called	*/
/*									*/
/************************************************************************/

LOCAL_RTN
void  camacRecordError (int ext, int error)
{
   int                b, c, n, a;	/* Branch, crate, & slot of error    */
   error_structure   *errorArray;	/* Address of error structure array  */
   error_structure   *errorStruct;	/* Address of error structure	     */
   int                index;		/* Index to correct struct in array  */
   branch_structure  *pbranch;		/* Address of branch structure	     */
   crate_structure   *pcrate;		/* Address of crate structure	     */
   slot_structure    *pslot;		/* Address of slot structure	     */

  /*---------------------
   * Extract the branch, crate and slot numbers from the CAMAC channel
   */
   hw_cgreg (ext, &b, &c, &n, &a);

  /*---------------------
   * If this is a branch error, get the address of the error array in the
   * appropriate branch structure.
   */
   pbranch = branch[b];
   if (error < CRATE_ERROR) errorArray = pbranch->errors;

  /*---------------------
   * If this is a crate or slot error, get the address of the error array in
   * the appropriate crate or slot structure.
   */
   else {
      pcrate = pbranch->crate[c];
      if (error < SLOT_ERROR) errorArray = pcrate->errors;
      else {
         pslot = &pcrate->slot[n-1];
         pslot->error_flag = TRUE;
         errorArray = pslot->errors;
      }/*end slot error*/
   }/*end else crate or slot error*/

  /*---------------------
   * Locate the correct error structure within the array
   */
   index = error & 0xffff;
   errorStruct = &errorArray[index];

  /*---------------------
   * Increment the current talley and the total so far.
   * Note that we are using unsigned integer arithmetic and that a "negative"
   * value indicates overflow.
   */
   if ((long int)errorStruct->current >= 0) errorStruct->current++;
   if ((long int)errorStruct->total >= 0) errorStruct->total++;

   return;

}/*end camacRecordError()*/

/************************************************************************/
/* camacBranchReport () - Report on a Branch or Serial Highway		*/
/*									*/
/************************************************************************/

GLOBAL_RTN
int   camacBranchReport (int b)
{
   int                count;	/* Number of error lines printed	*/
   branch_structure  *pBranch;	/* Address of branch structure		*/
   error_structure   *pError;	/* Address of branch error array	*/

  /*---------------------
   * Check to see if the branch actually exists
   */
   if (!LEGAL_BRANCH(b)) return 0;
   pBranch = branch[b];
   if (pBranch == NULL) return 0;

  /*---------------------
   * If the branch exists, print out a line for it and display any
   * branch errors.
   */
   pError = pBranch->errors;
   printf ("Branch %d", b);
   count = print_errors (pError, branch_error, MAX_BRANCH_ERR);

  /*---------------------
   * If there were errors, skip a line before the next report.
   * Otherwise, just output that there are no errors.
   */
   if (count) printf ("\n");
   else printf (" -- No Branch Errors\n");

  /*---------------------
   * Return the number of lines printed
   */
   return count + 1;

}/*end camacBranchReport()*/

/************************************************************************/
/* camacCrateReport () - Report on a Single Crate			*/
/*									*/
/************************************************************************/

GLOBAL_RTN
int   camacCrateReport (int b, int c)
{
   int                count;	/* Number of error lines printed	*/
   branch_structure  *pBranch;	/* Address of branch structure		*/
   crate_structure   *pCrate;	/* Address of crate structure		*/
   error_structure   *pError;	/* Address of crate error array		*/

  /*---------------------
   * Check to see if the branch exists
   */
   if (!LEGAL_BRANCH(b)) return 0;
   pBranch = branch[b];
   if (pBranch == NULL) return 0;

  /*---------------------
   * Check to see if the crate exists
   */
   if (!LEGAL_CRATE(c)) return 0;
   pCrate = pBranch->crate[c];
   if (pCrate == NULL) return 0;

  /*---------------------
   * If the crate exists, print out a line for it and display any
   * crate errors.
   */
   pError = pCrate->errors;
   printf ("Branch %d, Crate %2d", b, c);
   count = print_errors (pError, crate_error, MAX_CRATE_ERR);

  /*---------------------
   * If there were errors, skip a line before the next report.
   * Otherwise, just output that there are no errors.
   */
   if (count) printf ("\n");
   else printf (" -- No Crate Errors\n");

  /*---------------------
   * Return the number of lines printed
   */
   return count + 1;

}/*end camacCrateReport()*/

/************************************************************************/
/* camacSlotReport () - Report on a Single Slot				*/
/*									*/
/************************************************************************/

GLOBAL_RTN
int   camacSlotReport (int b, int c, int n)
{
   int                count;	/* Number of error lines printed	*/
   char              *name;	/* Name of the card in this slot	*/
   branch_structure  *pBranch;	/* Address of branch structure		*/
   crate_structure   *pCrate;	/* Address of crate structure		*/
   error_structure   *pError;	/* Address of slot error array		*/
   slot_structure    *pSlot;	/* Address of slot structure		*/

  /*---------------------
   * Check to see if the branch exists
   */
   if (!LEGAL_BRANCH(b)) return 0;
   pBranch = branch[b];
   if (pBranch == NULL) return 0;

  /*---------------------
   * Check to see if the crate exists
   */
   if (!LEGAL_CRATE(c)) return 0;
   pCrate = pBranch->crate[c];
   if (pCrate == NULL) return 0;

  /*---------------------
   * Check to see if the slot is legal (Note:  we will only display
   * normal slots which have either declared cards or recorded errors.
   */
   if ((n < 1) || (n > MAX_NORML_SLOT)) return 0;
   pSlot = &pCrate->slot[n-1];
   if ((pSlot->name == NULL) && !pSlot->error_flag) return 0;

  /*---------------------
   * If the slot exists, print out a line for it and display any
   * slot errors.
   */
   pError = pSlot->errors;
   name = pSlot->name;
   if (name == NULL) name = "Unknown Card";
   printf ("Branch %d, Crate %2d, Slot %2d:  %s", b, c, n, name);
   count = print_errors (pError, slot_error, MAX_SLOT_ERR);
   if (!count) printf ("\n");

  /*---------------------
   * Return the number of lines printed
   */
   return (count + 1);

}/*end camacSlotReport()*/

/************************************************************************/
/* camacErrorReport () -- Report Errors for Specified Branch, Crate,	*/
/*                        and Slot Combinations				*/
/*									*/
/*	b = Branch number (all branches if negative)			*/
/*	c = Crate number (all crates if negative)			*/
/*	n = Slot number (all slots if negative)				*/
/*									*/
/*	Returns number of lines written (not counting blank lines)	*/
/*	Note that a return value of zero indicates that no CAMAC	*/
/*	hardware was found.						*/
/*									*/
/************************************************************************/

GLOBAL_RTN
int  camacErrorReport (int b, int c, int n)
{
   int    b_start = 0;			/* Starting branch number	     */
   int    b_end   = MAX_BRANCH;		/* Ending branch number		     */
   int    b_next;			/* Current branch number	     */
   int    b_lines;			/* Lines printed for current branch  */

   int    c_start = 0;			/* Starting crate number	     */
   int    c_end   = MAX_CRATE;		/* Ending crate number		     */
   int    c_next;			/* Current crate number		     */
   int    c_lines;			/* Lines printed for current crate   */
   int    c_total = 0;			/* Total crate lines this branch     */

   int    n_start = 1;			/* Starting slot number		     */
   int    n_end   = MAX_NORML_SLOT;	/* Ending slot number		     */
   int    n_next;			/* Current slot number		     */
   int    n_total;			/* Total slot lines this crate	     */

   int    count = 0;			/* Total lines printed this routine  */

  /*---------------------
   * Set starting and ending values for branch, crate, and slot.
   */
   if (b >= 0) b_start = b_end = b;
   if (c >= 0) c_start = c_end = c;
   if (n >= 0) n_start = n_end = n;

  /*---------------------
   * Loop to output report for each selected branch
   */
   for (b_next=b_start; b_next<=b_end; b_next++) {
      if (c_total) printf ("\n");
      c_total = n_total = 0;
      b_lines = camacBranchReport (b_next);

     /*---------------------
      * If branch was valid, loop to output report for each selected crate
      */
      if (b_lines) for (c_next=c_start; c_next<=c_end; c_next++) {
         if (n_total) printf ("\n");
         n_total = 0;
         c_lines = camacCrateReport (b_next, c_next);

        /*---------------------
         * If crate was valid, loop to output report for each selected slot
         */
         if (c_lines) for (n_next=n_start; n_next<=n_end; n_next++) {
            n_total += camacSlotReport (b_next, c_next, n_next);
         }/*end for each slot */

        /*---------------------
         * Accumulate total lines for this crate
         */
         c_total += c_lines + n_total;
      }/*end for each crate*/

     /*---------------------
      * Accumulate total lines so far
      */
      count += b_lines + c_total;
   }/*end for each branch*/

  /*---------------------
   * Return the number of lines output (not counting blanks)
   */
   return count;

}/*end camacErrorReport()*/

/************************************************************************/
/*  camac_io_report () -- EPICS Driver Report Routine			*/
/*	o Level = 0:  Only report branch summaries			*/
/*	o Level = 1;  Only report branch and crate summaries		*/
/*	o Level = 2;  Report branch, crate, and slot summaries		*/
/*									*/
/************************************************************************/

GLOBAL_RTN
int   camac_io_report (short int level)
{
   int   count;			/* Number of report lines generated	*/
   int   branch = -1;		/* Branch number (all or none)		*/
   int   crate  = -1;		/* Crate number (all or none)		*/
   int   slot   = -1;		/* Slot number (all or none)		*/

  /*---------------------
   * Determine whether to report on crates and slots based on the level.
   * Note that -1 = all crates/slots;  Out-of-range = No crates/slots
   */
   if (level < 2) slot  = MAX_SLOT  + 1;
   if (level < 1) crate = MAX_CRATE + 1;

  /*---------------------
   * Output the header and the body of the report
   */
   printf ("\n********** Start CAMAC I/O Report for %s Driver %s **********\n\n", DRIVER_TYPE, drvName);
   count = camacErrorReport (branch, crate, slot);

  /*---------------------
   * If no lines were output, indicate that no CAMAC hardware was found
   */
   if (!count) printf ("No CAMAC Hardware Found\n");

  /*---------------------
   * Output the report trailer and return success
   */
   printf ("\n**********  End CAMAC I/O Report **********\n\n");
   return OK;

}/*end camac_io_report()*/

/*======================================================================*/
/*	Other Routines Which Are Non-ESONE, But Usefull Nonetheless	*/
/*									*/


/************************************************************************/
/* camacLockBranch () -- Lock a Branch/Serial Highway			*/
/*	o Grant exclusive access to a CAMAC branch so that a sequence	*/
/*	  of operations can be executed as if they were atomic.		*/
/*	o Returns OK if branch successfully locked.			*/
/*									*/
/************************************************************************/

GLOBAL_RTN
STATUS  camacLockBranch (int ext)
{
   branch_structure  *pbranch;			/* Ptr to branch structure */
   char              *rtn = "camacLockBranch";	/* Name of this routine	   */

   pbranch = get_branch (ext, rtn);
   if (pbranch == NULL) return errno;
   return lock_branch (pbranch, rtn);

}/*end camacLockBranch()*/


/************************************************************************/
/* camacUnlockBranch () -- Unlock a Branch/Serial Highway		*/
/*	o Release exclusive access to a CAMAC branch			*/
/*	o Returns OK if branch successfully unlocked.			*/
/*									*/
/************************************************************************/

GLOBAL_RTN
STATUS  camacUnlockBranch (int ext)
{
   branch_structure  *pbranch;			/* Ptr to branch structure */

   pbranch = get_branch (ext, "camacUnlockBranch");
   if (pbranch == NULL) return errno;
   unlock_branch (pbranch);
   return OK;

}/*end camacUnlockBranch()*/

/************************************************************************/
/* camacRegisterCard () -- Set Card Type and Initialization Routine	*/
/*									*/
/************************************************************************/

GLOBAL_RTN
void  camacRegisterCard (int b, int c, int n, char *name,
                         camacCardInitRtn initRtn, int parm)
{
   branch_structure  *pbranch;
   crate_structure   *pcrate;
   slot_structure    *pslot;

  /*---------------------
   * Check for valid branch
   */
   if (!LEGAL_BRANCH(b)) return;
   pbranch = branch[b];
   if (pbranch == NULL) return;

  /*---------------------
   * Check for valid crate
   */
   if (!LEGAL_CRATE(c)) return;
   pcrate = pbranch->crate[c];

  /*---------------------
   * See if crate needs to be initialized
   */
   if (pcrate == NULL) {
      camacCrateInit (b, c);
      pcrate = pbranch->crate[c];
      if (pcrate == NULL) return;
   }/*end if crate not yet initialized*/

  /*---------------------
   * Check for valid (and uninitialized) slot
   */
   if ((n < 1) || (n > MAX_NORML_SLOT)) return;
   pslot = &pcrate->slot[n-1];
   if (pslot->name != NULL) return;

  /*---------------------
   * Set the card name and initialization routine for this slot
   */
   pslot->name = name;
   pslot->slotInit = initRtn;
   pslot->slotInitParm = parm;
   return;

}/*end camacRegisterCard()*/
