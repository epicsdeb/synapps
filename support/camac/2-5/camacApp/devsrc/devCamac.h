/******************************************************************************
 * devCamac.h -- Header file containing structure definitions and function
 *               prototypes for generic CAMAC device support routines.
 *
 *-----------------------------------------------------------------------------
 * Author:		Eric Bjorklund and Rozelle Wright (LANL)
 *			Adapted from the work of Dave Barker and
 *			Johnny Tang (CEBAF).
 *
 * Origination Date:	10/26/94
 * 11/7/94	rmw	- added NO_XPARM definition for no external parameters in a field
 *
 *-----------------------------------------------------------------------------
 * MODIFICATION LOG:
 * 
 * 3/12/96	mlr	Added waveform record support
 *
 *****************************************************************************/

/************************************************************************/
/*  Header Files							*/
/************************************************************************/

#include	<dbCommon.h>		/* EPICS Common record fields	 */
#include	<aiRecord.h>		/* EPICS AI Record Definitions	 */
#include	<aoRecord.h>		/* EPICS AO Record Definitions	 */
#include	<biRecord.h>		/* EPICS BI Record Definitions	 */
#include	<boRecord.h>		/* EPICS BO Record Definitions	 */
#include	<longinRecord.h>	/* EPICS LI Record Definitions	 */
#include	<longoutRecord.h>	/* EPICS LO Record Definitions	 */
#include	<mbbiRecord.h>		/* EPICS MBBI Record Definitions */
#include	<mbboRecord.h>		/* EPICS MBBO Record Definitions */
#include	<waveformRecord.h>	/* EPICS WF Record Definitions */

#ifndef vxWorks
typedef int STATUS;
#endif

/************************************************************************/
/*  Global Structure Definitions					*/
/************************************************************************/

/*---------------------
 * Structure used to pass parameter information from external device support
 * modules.
 */
#define NO_XPARM	-1	/* No external parameter to be passed in this field */

typedef struct /* devCamacParm */ {
   short int      f;		/* Function code			*/
   short int      a;		/* Subaddress				*/
   short int      readback;	/* Function code for readback		*/
   short int      bipolar;	/* True if range is bipolar		*/
   unsigned int   range_mask;	/* Mask parameter			*/
   short int      qmode;	/* Q transfer mode 			*/
} devCamacParm;

/*---------------------
 * AI/AO device-dependant structure
 */
typedef struct /* devInfo_aiCamac */ {
   short          f_read;	/* Function code for readback		*/
   short          f_write;	/* Function code for writes (ao only)	*/
   int            ext;		/* CAMAC channel variable		*/
   int            range;	/* Range for slope calculation		*/
   unsigned long  mask;		/* Mask for precision correction	*/
   unsigned long  sign_mask;	/* Mask for sign bit			*/
   unsigned long  sign_extend;	/* Mask for sign extension		*/
} devInfo_aiCamac;

typedef  devInfo_aiCamac   devInfo_aoCamac;	/* AO same as AI	*/

/*---------------------
 * BI/BO/MBBI/MBBO device-dependant structure
 */
typedef struct /* devInfo_biCamac */ {
   short int      f_read;	/* Function code for readback		*/
   short int      f_write;	/* Function code for write/set		*/
   short int      f_clear;	/* Function code for selective clear	*/
   short int      test;		/* If true, read is actually a test	*/
   int            ext;		/* CAMAC channel variable		*/
   unsigned long  mask;		/* Mask to use for bit extraction	*/
   unsigned long  merge_mask;	/* Mask to use for read/mask/write	*/
} devInfo_biCamac;

typedef  devInfo_biCamac   devInfo_boCamac;	/* BO   same as BI	*/
typedef  devInfo_biCamac   devInfo_mbbiCamac;	/* MBBI same as BI	*/
typedef  devInfo_biCamac   devInfo_mbboCamac;	/* MBBO same as BI	*/

/*---------------------
 * LI/LO device-dependant structure
 */
typedef struct /* devInfo_liCamac */ {
   short          f_read;	/* Function code for readback		*/
   short          f_write;	/* Function code for writes (lo only)	*/
   int            ext;		/* CAMAC channel variable		*/
   unsigned long  mask;		/* Mask for precision correction	*/
   unsigned long  sign_mask;	/* Mask for sign bit			*/
   unsigned long  sign_extend;	/* Mask for sign extension		*/
} devInfo_liCamac;

typedef  devInfo_liCamac   devInfo_loCamac;	/* LO same as LI	*/

/*---------------------
 * WF device-dependant structure
 */
typedef struct /* devInfo_wfCamac */ {
   short          f;		/* Function code for read or write	*/
   int            ext;		/* CAMAC channel variable		*/
   unsigned long  mask;		/* Mask for precision correction	*/
   unsigned long  sign_mask;	/* Mask for sign bit			*/
   unsigned long  sign_extend;	/* Mask for sign extension		*/
   int		  qmode;	/* Q transfer mode			*/
} devInfo_wfCamac;


/************************************************************************/
/*  Function Prototypes							*/
/************************************************************************/

/*---------------------
 * Common Utility Routines
 */
unsigned long  devCamac_getMask (int range1, int range2);
int            devCamac_parseParm (int *range1, int *range2, int *read_f, 
		int *qmode, char *parm);
void           devCamac_recDisable (struct dbCommon *prec);

/*---------------------
 * Common Readback Routines
 */
STATUS   devCamac_aioReadback (devInfo_aiCamac *pdevInfo, int *value);
STATUS   devCamac_bioReadback (devInfo_biCamac *pdevInfo, int *value);
STATUS   devCamac_lioReadback (devInfo_liCamac *pdevInfo, int *value);
STATUS   devCamac_mbbioReadback (devInfo_mbbiCamac *pdevInfo, int *value);

/*---------------------
 * Analog Input Routines
 */
STATUS   devCamac_aiInitRecord (struct aiRecord *prec);
STATUS   devCamac_aiRead (struct aiRecord *prec);
STATUS   devCamac_aiLinConv (struct aiRecord *prec, int after);

/*---------------------
 * Analog Output Routines
 */
STATUS   devCamac_aoInitRecord (struct aoRecord *prec);
STATUS   devCamac_aoWrite (struct aoRecord *prec);
STATUS   devCamac_aoLinConv (struct aoRecord *prec, int after);

/*---------------------
 * Binary Input Routines
 */
STATUS   devCamac_biInitRecord (struct biRecord *prec);
STATUS   devCamac_biRead (struct biRecord *prec);

/*---------------------
 * Binary Output Routines
 */
STATUS   devCamac_boInitRecord (struct boRecord *prec);
STATUS   devCamac_boWrite (struct boRecord *prec);

/*---------------------
 * Longword Input Routines
 */
STATUS   devCamac_liInitRecord (struct longinRecord *prec);
STATUS   devCamac_liRead (struct longinRecord *prec);

/*---------------------
 * Longword Output Routines
 */
STATUS   devCamac_loInitRecord (struct longoutRecord *prec);
STATUS   devCamac_loWrite (struct longoutRecord *prec);

/*---------------------
 * Multi-Bit Binary Input Routines
 */
STATUS   devCamac_mbbiInitRecord (struct mbbiRecord *prec);
STATUS   devCamac_mbbiRead (struct mbbiRecord *prec);

/*---------------------
 * Multi-Bit Binary Output Routines
 */
STATUS   devCamac_mbboInitRecord (struct mbboRecord *prec);
STATUS   devCamac_mbboWrite (struct mbboRecord *prec);

/*---------------------
 * Waveform Routines
 */
STATUS   devCamac_wfInitRecord (struct waveformRecord *prec);
STATUS   devCamac_wfReadWrite (struct waveformRecord *prec);
