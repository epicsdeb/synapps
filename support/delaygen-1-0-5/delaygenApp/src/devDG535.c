/*
 * dg535 device support
 * Device support for Tektronix TDS3000-series digital oscilloscopes
 *
 ************************************************************************
 * Copyright (c) 2003 The University of Chicago, as Operator of Argonne
 * National Laboratory, and the Regents of the University of
 * California, as Operator of Los Alamos National Laboratory.
 * TDS3000 support is distributed subject to a Software License Agreement
 * found in file LICENSE that is included with this distribution.
 ************************************************************************
 *
 * Original Author: Ned Arnold <nda@aps.anl.gov>
 */


#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <epicsStdio.h>

#include <devCommonGpib.h>

/******************************************************************************
 *
 * The following define statements are used to declare the names to be used
 * for the dset tables.   
 *
 * A DSET_AI entry must be declared here and referenced in an application
 * database description file even if the device provides no AI records.
 *
 ******************************************************************************/
#define DSET_AI     devAidg535
#define DSET_AO     devAodg535
#define DSET_BI     devBidg535
#define DSET_BO     devBodg535
#define DSET_LI     devLidg535
#define DSET_LO     devLodg535
#define DSET_MBBI   devMbbidg535
#define DSET_MBBO   devMbbodg535
#define DSET_SI     devSidg535
#define DSET_SO     devSodg535

#include <devGpib.h> /* must be included after DSET defines */

#define TIMEOUT     4.0    /* I/O must complete within this time */
#define TIMEWINDOW  2.0    /* Wait this long after device timeout */

/******************************************************************************
 * Structures used by the init routines to fill in the onst,twst,... and the
 * onvl,twvl,... fields in MBBI and MBBO record types.
 *
 ******************************************************************************/

static  char    *lozHizList[] = { "50 OHM", "HIGH Z" };
static  struct  devGpibNames    lozHiz = {2, lozHizList, NULL, 1};

static  char    *invertNormList[] = { "INVERT", "NORM" };
static  struct  devGpibNames    invertNorm = { 2, invertNormList, NULL, 1 };

static  char    *fallingRisingList[] = { "FALLING", "RISING" };
static  struct  devGpibNames fallingRising = { 2, fallingRisingList, NULL, 1 };

static  char    *singleShotList[] = { "OFF", "TRIGGER" };
static  struct  devGpibNames    singleShot = { 2, singleShotList, NULL, 1 };

static  char    *clearList[] = { "OFF", "CLEAR" };
static  struct  devGpibNames    clear = { 2, clearList, NULL, 1 };

static  char            *tABCDList[] = { "T", "A", "B", "C", "D" };
static  unsigned long   tABCDVal[] = { 1, 2, 3, 5, 6 };
static  struct  devGpibNames    tABCD = { 5, tABCDList, tABCDVal, 3 };

static  char            *ttlNimEclVarList[] = { "TTL", "NIM", "ECL", "VAR" };
static  unsigned long   ttlNimEclVarVal[] = { 0, 1, 2, 3 };
static  struct  devGpibNames    ttlNimEclVar = { 4, ttlNimEclVarList,
                                        ttlNimEclVarVal, 2 };

static  char            *intExtSsBmLineList[] = { "INTERNAL", "EXTERNAL",
                                        "SINGLE SHOT", "BURST MODE", "LINE" };
static  unsigned long   intExtSsBmLineVal[] = { 0, 1, 2, 3, 4 };

static  struct  devGpibNames    intExtSsBmLine = { 5, intExtSsBmLineList,
                                        intExtSsBmLineVal, 3 };

/* Channel Names, used to derive string representation of programmed delay */

char  *pchanName[8] = {" ", "T + ", "A + ", "B + ", " ", "C + ", "D + ", " "};

/******************************************************************************
 * Array of structures that define all GPIB messages
 * supported for this type of instrument.
 ******************************************************************************/

/* forward declarations of some custom convert routines */
static int setDelay();
static int rdDelay();

static struct gpibCmd gpibCmds[] =
{
  /* Param 0, (model)   */
  FILL,

  /* Channel A Delay and Output */
  /* Param 1, write A delay  */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, "DT 2", "DT 2,?,%.12lf", 0, 32,
  setDelay, 0, 0, NULL, NULL, 0},

  /* Param 2, currently undefined */
  FILL,

  /* Param 3, read A Delay */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "DT 2", NULL, 0, 32,
  rdDelay, 0, 0, NULL, NULL, 0},

  /* Param 4, read A delay reference channel and delay as string */
  {&DSET_SI, GPIBREAD, IB_Q_LOW, "DT 2", NULL, 0, 32,
  rdDelay, 0, 0, NULL, NULL, 0},

  /* Param 5, set A delay reference channel */
  {&DSET_MBBO, GPIBWRITE, IB_Q_HIGH, "DT 2", "DT 2,%u,", 0, 32,
  setDelay, 0, 0, NULL, &tABCD, 0},

  /* Param 6, read A delay reference */
  {&DSET_MBBI, GPIBREAD, IB_Q_LOW, "DT 2", NULL, 0, 32,
  rdDelay, 0, 0, NULL, &tABCD, 0},

  /* Param 7, set A output mode */
  {&DSET_MBBO, GPIBWRITE, IB_Q_HIGH, NULL, "OM 2,%u", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar, 0},

  /* Param 8, read A output mode */
  {&DSET_MBBI, GPIBREAD, IB_Q_LOW, "OM 2", "%lu", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar, 0},

  /* Param 9, set A output amplitude */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "OA 2,%.2f", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 10, read A output amplitude */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "OA 2", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 11, set A output offset */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "OO 2,%.2f", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 12, read A output offset */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "OO 2", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 13, set A output Termination */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "TZ 2,%u", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, 0},

  /* Param 14, read A output Termination */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "TZ 2", "%lu", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, 0},

  /* Param 15, set A output Polarity */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "OP 2,%u", 0, 32,
  NULL, 0, 0, NULL, &invertNorm, 0},

  /* Param 16, read A output Polarity */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "OP 2", "%lu", 0, 32,
  NULL, 0, 0, NULL, &invertNorm, 0},

/* Channel B Delay and Output */
  /* Param 17, write B delay  */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, "DT 3", "DT 3,?,%.12lf", 0, 32,
  setDelay, 0, 0, NULL, NULL, 0},

  /* Param 18, currently undefined */
  FILL,

  /* Param 19, read B Delay */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "DT 3", NULL, 0, 32,
  rdDelay, 0, 0, NULL, NULL, 0},

  /* Param 20, read B delay reference channel and delay as string */
  {&DSET_SI, GPIBREAD, IB_Q_LOW, "DT 3", NULL, 0, 32,
  rdDelay, 0, 0, NULL, NULL, 0},

  /* Param 21, set B delay reference channel */
  {&DSET_MBBO, GPIBWRITE, IB_Q_HIGH, "DT 3", "DT 3,%u,", 0, 32,
  setDelay, 0, 0, NULL, &tABCD, 0},

  /* Param 22, read B delay reference */
  {&DSET_MBBI, GPIBREAD, IB_Q_LOW, "DT 3", NULL, 0, 32,
  rdDelay, 0 ,0, NULL, &tABCD, 0},

  /* Param 23, set B output mode */
  {&DSET_MBBO, GPIBWRITE, IB_Q_HIGH, NULL, "OM 3,%u", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar, 0},

  /* Param 24, read B output mode */
  {&DSET_MBBI, GPIBREAD, IB_Q_LOW, "OM 3", "%lu", 0 ,32,
  NULL, 0, 0, NULL, &ttlNimEclVar, 0},

  /* Param 25, set B output amplitude */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "OA 3,%.2f", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 26, read B output amplitude */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "OA 3", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 27, set B output offset */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "OO 3,%.2f", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 28, read B output offset */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "OO 3", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 29, set B output Termination */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "TZ 3,%u", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, 0},

  /* Param 30, read B output Termination */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "TZ 3", "%lu", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, 0},

  /* Param 31, set B output Polarity */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "OP 3,%u", 0, 32,
  NULL, 0, 0, NULL, &invertNorm, 0},

  /* Param 32, read B output Polarity */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "OP 3", "%lu", 0, 32,
  NULL, 0, 0, NULL, &invertNorm, 0},

/* Channel AB Outputs */
  /* Param 33, set AB output mode */
  {&DSET_MBBO, GPIBWRITE, IB_Q_HIGH, NULL, "OM 4,%u", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar, 0},

  /* Param 34, read AB output mode */
  {&DSET_MBBI, GPIBREAD, IB_Q_LOW, "OM 4", "%lu", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar, 0},

  /* Param 35, set AB output amplitude */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "OA 4,%.2f", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 36, read AB output amplitude */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "OA 4", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 37, set AB output offset */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "OO 4,%.2f", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 38, read AB output offset */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "OO 4", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 39, set AB output Termination */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "TZ 4,%u", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, 0},

  /* Param 40, read AB output Termination */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "TZ 4", "%lu", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, 0},

  /* Param 41, set AB output Polarity */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "OP 4,%u", 0, 32,
  NULL, 0, 0, NULL, &invertNorm, 0},

  /* Param 42, read AB output Polarity */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "OP 4", "%lu", 0, 32,
  NULL, 0, 0, NULL, &invertNorm, 0},

/* Channel C Delay and Output */
  /* Param 43, write C delay  */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, "DT 5", "DT 5,?,%.12lf", 0, 32,
  setDelay, 0, 0, NULL, NULL, 0},

  /* Param 44, currently undefined */
  FILL,

  /* Param 45, read C Delay */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "DT 5", NULL, 0, 32,
  rdDelay, 0, 0, NULL, NULL, 0},

  /* Param 46, read C delay reference channel and delay as string */
  {&DSET_SI, GPIBREAD, IB_Q_LOW, "DT 5", NULL, 0, 32,
  rdDelay, 0, 0, NULL, NULL, 0},

  /* Param 47, set C delay reference channel */
  {&DSET_MBBO, GPIBWRITE, IB_Q_HIGH, "DT 5", "DT 5,%u,", 0, 32,
  setDelay, 0, 0, NULL, &tABCD, 0},

  /* Param 48, read C delay reference */
  {&DSET_MBBI, GPIBREAD, IB_Q_LOW, "DT 5", NULL, 0, 32,
  rdDelay, 0, 0, NULL, &tABCD, 0},

  /* Param 49, set C output mode */
  {&DSET_MBBO, GPIBWRITE, IB_Q_HIGH, NULL, "OM 5,%u", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar, 0},

  /* Param 50, read C output mode */
  {&DSET_MBBI, GPIBREAD, IB_Q_LOW, "OM 5", "%lu", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar, 0},

  /* Param 51, set C output amplitude */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "OA 5,%.2f", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 52, read C output amplitude */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "OA 5", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 53, set C output offset */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "OO 5,%.2f", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 54, read C output offset */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "OO 5", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 55, set C output Termination */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "TZ 5,%u", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, 0},

  /* Param 56, read C utput Termination */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "TZ 5", "%lu", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, 0},

  /* Param 57, set C output Polarity */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "OP 5,%u", 0, 32,
  NULL, 0, 0, NULL, &invertNorm, 0},

  /* Param 58, read C output Polarity */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "OP 5", "%lu", 0, 32,
  NULL, 0, 0, NULL, &invertNorm, 0},

/* Channel D Delay and Output */
  /* Param 59, write D delay  */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, "DT 6", "DT 6,?,%.12lf", 0, 32,
  setDelay, 0, 0, NULL, NULL, 0},

  /* Param 60, currently undefined */
  FILL,

  /* Param 61, read D Delay */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "DT 6", NULL, 0, 32,
  rdDelay, 0, 0, NULL, NULL, 0},

  /* Param 62, read D delay reference channel and delay as string */
  {&DSET_SI, GPIBREAD, IB_Q_LOW, "DT 6", NULL, 0, 32,
  rdDelay, 0, 0, NULL, NULL, 0},

  /* Param 63, set D delay reference channel */
  {&DSET_MBBO, GPIBWRITE, IB_Q_HIGH, "DT 6", "DT 6,%u,", 0, 32,
  setDelay, 0, 0, NULL, &tABCD, 0},

  /* Param 64, read D delay reference */
  {&DSET_MBBI, GPIBREAD, IB_Q_LOW, "DT 6", NULL, 0, 32,
  rdDelay, 0 ,0, NULL, &tABCD, 0},

  /* Param 65, set D output mode */
  {&DSET_MBBO, GPIBWRITE, IB_Q_HIGH, NULL, "OM 6,%u", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar, 0},

  /* Param 66, read D output mode */
  {&DSET_MBBI, GPIBREAD, IB_Q_LOW, "OM 6", "%lu", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar, 0},

  /* Param 67, set D output amplitude */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "OA 6,%.2f", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 68, read D output amplitude */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "OA 6", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 69, set D output offset */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "OO 6,%.2f", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 70, read D output offset */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "OO 6", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 71, set D output Termination */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "TZ 6,%u", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, 0},

  /* Param 72, read D output Termination */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "TZ 6", "%lu", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, 0},

  /* Param 73, set D output Polarity */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "OP 6,%u", 0, 32,
  NULL, 0, 0, NULL, &invertNorm, 0},

  /* Param 74, read D output Polarity */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "OP 6", "%lu", 0, 32,
  NULL, 0, 0, NULL, &invertNorm, 0},

/* Channel CD Outputs */
  /* Param 75, set CD output mode */
  {&DSET_MBBO, GPIBWRITE, IB_Q_HIGH, NULL, "OM 7,%u", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar, 0},

  /* Param 76, read CD output mode */
  {&DSET_MBBI, GPIBREAD, IB_Q_LOW, "OM 7", "%lu", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar, 0},

  /* Param 77, set CD output amplitude */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "OA 7,%.2f", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 78, read CD output amplitude */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "OA 7", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 79, set CD output offset */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "OO 7,%.2f", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 80, read CD output offset */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "OO 7", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 81, set CD output Termination */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "TZ 7,%u", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, 0},

  /* Param 82, read CD output Termination */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "TZ 7", "%lu", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, 0},

  /* Param 83, set CD output Polarity */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "OP 7,%u", 0, 32,
  NULL, 0, 0, NULL, &invertNorm, 0},

  /* Param 84, read CD output Polarity */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "OP 7", "%lu", 0, 32,
  NULL, 0, 0, NULL, &invertNorm, 0},

/* Trigger Settings */
  /* Param 85, set Trig Mode */
  {&DSET_MBBO, GPIBWRITE, IB_Q_HIGH, NULL, "TM %u", 0, 32,
  NULL, 0, 0, NULL, &intExtSsBmLine, 0},

  /* Param 86, read Trig Mode */
  {&DSET_MBBI, GPIBREAD, IB_Q_LOW, "TM ", "%lu", 0, 32,
  NULL, 0, 0, NULL, &intExtSsBmLine, 0},

  /* Param 87, set Trig Rate */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "TR 0,%.3lf", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 88, read Trig Rate */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "TR 0", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 89, set Burst Rate */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "TR 1,%.3lf", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 90, read Burst Rate */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "TR 1", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 91, set Burst Count */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "BC %01.0lf", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 92, read Burst Count */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "BC", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 93, set Burst Period */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "BP %01.0lf", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 94, read Burst Period */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "BP", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 95, set Trig Input Z */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "TZ 0,%u", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, 0},

  /* Param 96, read Trig Input Z */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "TZ 0", "%lu", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, 0},

  /* Param 97, set Trig Input slope */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "TS %u", 0, 32,
  NULL, 0, 0, NULL, &fallingRising, 0},

  /* Param 98, read Trig Input slope */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "TS", "%lu", 0, 32,
  NULL, 0, 0, NULL, &fallingRising, 0},

  /* Param 99, set Trig Input level */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "TL %.2lf", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 100, read Trig Input Level */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "TL", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 101, generate Single Trig */
  {&DSET_BO, GPIBCMD, IB_Q_HIGH, "SS", NULL, 0, 32,
  NULL, 0, 0, NULL, &singleShot, 0},

  /* Param 102, Store Setting # */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "ST %01.0lf", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 103, Recall Setting # */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "RC %01.0lf", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 104, Recall Setting #  */
  {&DSET_BO, GPIBCMD, IB_Q_HIGH, "CL", NULL, 0, 32,
  NULL, 0, 0, NULL, &clear, 0},

  /* Param 105, Error Status   #  */
  {&DSET_LI, GPIBREAD, IB_Q_LOW, "ES", "%lu", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 106, Instrument Status */
  {&DSET_LI, GPIBREAD, IB_Q_LOW, "IS", "%lu", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

/* T0 Output parameters */
  /* Param 107, set T0 output mode */
  {&DSET_MBBO, GPIBWRITE, IB_Q_HIGH, NULL, "OM 1,%u", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar, 0},

  /* Param 108, read T0 output mode */
  {&DSET_MBBI, GPIBREAD, IB_Q_LOW, "OM 1", "%lu", 0, 32,
  NULL, 0, 0, NULL, &ttlNimEclVar, 0},

  /* Param 109, set T0 output amplitude */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "OA 1,%.2f", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 110, read T0 output amplitude */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "OA 1", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 111, set T0 output offset */
  {&DSET_AO, GPIBWRITE, IB_Q_HIGH, NULL, "OO 1,%.2f", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 112, read T0 output offset */
  {&DSET_AI, GPIBREAD, IB_Q_LOW, "OO 1", "%lf", 0, 32,
  NULL, 0, 0, NULL, NULL, 0},

  /* Param 113, set T0 output Termination */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "TZ 1,%u", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, 0},

  /* Param 114, read T0 output Termination */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "TZ 1", "%lu", 0, 32,
  NULL, 0, 0, NULL, &lozHiz, 0},

  /* Param 115, set T0 output Polarity */
  {&DSET_BO, GPIBWRITE, IB_Q_HIGH, NULL, "OP 1,%u", 0, 32,
  NULL, 0, 0, NULL, &invertNorm, 0},

  /* Param 116, read T0 output Polarity */
  {&DSET_BI, GPIBREAD, IB_Q_LOW, "OP 1", "%lu", 0, 32,
  NULL, 0, 0, NULL, &invertNorm, 0}

};


/* The following is the number of elements in the command array above.  */
#define NUMPARAMS sizeof(gpibCmds)/sizeof(struct gpibCmd)

/******************************************************************************
 * Initialize device support parameters
 *
 *****************************************************************************/
static long init_ai(int parm)
{
    if(parm==0) {
        devSupParms.name = "devdg535";
        devSupParms.gpibCmds = gpibCmds;
        devSupParms.numparams = NUMPARAMS;
        devSupParms.timeout = TIMEOUT;
        devSupParms.timeWindow = TIMEWINDOW;
        devSupParms.respond2Writes = -1;
    }
    return(0);
}

/******************************************************************************
 *
 * Unique message interpretaion for reading Channel Delays :
 * The command to read the delay setting returns a string with two arguments,
 * (eg  1, 4.300000000000)
 * This routine extracts both arguments and assigns them appropriately. If the
 * parameter corresponds to a "READ DELAY INTO STRING RECORD",
 * in the val field of the SI record. (ex:  T + .000000500000)
 *****************************************************************************/

static int rdDelay(struct gpibDpvt *pgpibDpvt,int P1, int P2, char **P3)
{
  asynUser *pasynUser = pgpibDpvt->pasynUser;
  int         status;
  double      delay;
  unsigned long chan;

  union delayRec {
    struct aiRecord ai;
    struct mbbiRecord mbbi;
    struct stringinRecord si;
  } *prec = (union delayRec *) (pgpibDpvt->precord);

  asynPrint(pasynUser,ASYN_TRACE_FLOW,"%s rdDelay Parm %d msg %s\n",
      pgpibDpvt->precord->name,pgpibDpvt->parm,pgpibDpvt->msg);

  /* scan response string for chan reference & delay value  */

  status = sscanf(pgpibDpvt->msg, "%ld,%lf", &chan, &delay);
  asynPrint(pasynUser,ASYN_TRACE_FLOW," sscanf status = %d\n",status);
  if(status!=2) {
      epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
          "rdDelay sscanf returned %d should be 2\n", status);
      return(-1);
  }

  switch (pgpibDpvt->parm)
  {
  case 4:   /* A Delay monitor, must be an si record */
  case 20:  /* B Delay monitor, must be an si record */
  case 46:  /* C Delay monitor, must be an si record */
  case 62:  /* D Delay monitor, must be an si record */
    /* create a string in the value field*/
    epicsSnprintf(prec->si.val,sizeof(prec->si.val),"%s%s",
        pchanName[chan],&((pgpibDpvt->msg)[3]));
    prec->si.udf = FALSE;
    break;

  case 3:   /* A Delay monitor, must be an ai record */
  case 19:  /* B Delay monitor, must be an ai record */
  case 45:  /* C Delay monitor, must be an ai record */
  case 61:  /* D Delay monitor, must be an ai record */
    /* assign new delay to value field*/
    prec->ai.val = delay;
    prec->ai.udf = FALSE;
    break;

  case 6:    /* A Delay Reference monitor, must be an mbbi record */
  case 22:   /* B Delay Reference monitor, must be an mbbi record */
  case 48:   /* C Delay Reference monitor, must be an mbbi record */
  case 64:   /* D Delay Reference monitor, must be an mbbi record */
    prec->mbbi.rval = chan;
    break;
  }
  return(0);
}

/******************************************************************************
 *
 * Unique message generation for writing channel Delays :
 * The command to set the channel delay requires two parameters: The channel
 * # to reference from and the time delay. Since changing either of these
 * parameters requires the entire command to be sent, the current state of
 * other parameter must be determined. This is done by reading the delay (which
 * returns both parameters), changing one of the paramaters, and sending
 * the command back.
 *
 *************************************************************************/

static int setDelay(struct gpibDpvt *pgpibDpvt, int P1, int P2, char **P3)
{
  asynUser *pasynUser = pgpibDpvt->pasynUser;
  char        curChan;
  char        tempMsg[32];
  gpibCmd *pgpibCmd = gpibCmdGet(pgpibDpvt);
  asynOctet *pasynOctet = pgpibDpvt->pasynOctet;
  void *asynOctetPvt = pgpibDpvt->asynOctetPvt;
  asynStatus status;
  int ntimes,itime;


  union delayRec {
    struct aoRecord ao;
    struct mbboRecord mbbo;
  } *prec = (union delayRec *) (pgpibDpvt->precord);

  asynPrint(pasynUser,ASYN_TRACE_FLOW,"%s setDelay\n",pgpibDpvt->precord->name);

  /* go read the current delay & channel reference setting */
  /* Due to a fluke in the DG535, read twice to insure accurate data */
  ntimes=2;
  for(itime=0; itime<2; itime++) {
    size_t nout,nin;

    asynPrint(pasynUser,ASYN_TRACE_FLOW," write %s\n",pgpibCmd->cmd);
    status = pasynOctet->write(asynOctetPvt,pgpibDpvt->pasynUser,
      pgpibCmd->cmd,strlen(pgpibCmd->cmd),&nout);
    nin = 0;
    status = pasynOctet->read(asynOctetPvt,pgpibDpvt->pasynUser,
       pgpibDpvt->msg,pgpibCmd->msgLen,&nin,0);
    asynPrint(pasynUser,ASYN_TRACE_FLOW," read %s\n",pgpibDpvt->msg);
    if(status!=asynSuccess)
    {  /* abort operation if read failed */
      epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,
          "setDelay read failed\n");
      return(-1);
    }
  }


  /* change one of the two parameters ... */

  switch(pgpibDpvt->parm)
  {
    case 1:                             /* changing time delay */
    case 17:
    case 43:
    case 59:
      curChan = pgpibDpvt->msg[0];      /* save current chan reference */
      /* generate new delay string (correct rounding error) */
      sprintf(pgpibDpvt->msg,gpibCmds[pgpibDpvt->parm].format,
          (prec->ao.val + 1.0e-13));
      pgpibDpvt->msg[5] = curChan;    /* replace "?" with current chan */
      break;

    case 5:                         /* changing reference channel */
    case 21:
    case 47:
    case 63:
      strcpy(tempMsg, &((pgpibDpvt->msg)[3])); /* save current delay setting */
      /* generate new channel reference */
      sprintf(pgpibDpvt->msg, gpibCmds[pgpibDpvt->parm].format,
          (unsigned int)prec->mbbo.rval);
      strcat(pgpibDpvt->msg, tempMsg); /* append current delay setting */
      break;
  }

  return(0);         /* aoGpibWork or mbboGpibWork will call xxGpibWork */
}
