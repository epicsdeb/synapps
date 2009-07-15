
/*
 *      Original Author: Ned D. Arnold
 *      Date:            07-18-94
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 *
 * Modification Log:
 * -----------------
 * .01  07-18-94  nda   significantly expanded functionality from prototype
 * .02  08-18-94  nda   Starting with R3.11.6, dbGetField locks the record
 *                      before fetching the data. This can cause deadlocks
 *                      within a database. Change all dbGetField() to dbGet()
 * .03  08-23-94  nda   added code for checking/adjusting linear sscan
 *                      params (it gets a little messy !)
 * .04  08-25-94  nda   Added check of scan positions vs Positioner Control
 *                      Limits
 * .05  08-29-94  nda   Added "viewScanPos" that puts desired positions
 *                      in D1 array any time a scan parameter is changed
 * .06  10-03-94  nda   added code for enumerated string .CMND. Changed
 *                      .EXSC to a SHORT in record definition. Added VERSION
 *                      for .VERS field (1.06) to keep track of version.
 * .07  10-21-94  nda   changed linear scan parameter algorithms so changing
 *                      start/end modifies step/width unless frozen. This
 *                      seems more intuitive.
 * .08  12-06-94  nda   added support for .FFO .When set to 1, frzFlag values
 *                      are saved in recPvtStruct. Restored when FFO set to 0.
 * .09  02-02-95  nda   fixed order of posting monitors to be what people
 *                      expect (i.e. .cpt, .pxdv, .dxcv)
 * .10  02-10-95  nda   fixed on-the-fly so 1st step is to end position
 * .11  02-21-95  nda   added "Return To Start" flag. If set, positioners
 *                      will be commanded to the start pos after the sscan.
 * .12  03-02-95  nda   added positioner readback arrays (PxRA). These arrays
 *                      will contain actual readback positions (if RxPV are
 *                      defined. If not, the desired positions will be loaded
 *                      into them.
 * .13  03-02-95  nda   Post .val field when a new point is complete during a
 *                      sscan. This will assist in poin by point plots.
 * .14  03-02-95  nda   Bug fix on filling PxRA's. ALWAYS CALL CHECK_MOTORS.
 * .15  03-15-95  nda   If no readback PV (RxPV) is specified, copy desired
 *                      value (PxDV) to current value (RxCV). Now, plotting
 *                      programs can always monitor RxCV.
 * .16  04-03-95  nda   If PV field = DESC, change to VAL
 *
 * 3.00 08-28-95  nda   > Significant rewrite to add Channel Access
 *                      for dynamic links using recDynLink.c . All
 *                      inputs are now "monitored" via Channel Access.
 *                      > Name Valid field is used to keep track of PV
 *                      connection status: 0-PV_OK,
 *                      1-NotConnected,  2-NO_PV
 *                      > added relative/absolute mode on positioners
 *                      > added retrace options of stay/start/prior
 *                      > supports 15 detectors
 *                      > added "preview sscan"
 *                      > added "Before Scan" and "After Scan" links
 *                      > fetch DRVL/DRVH/prec/egu from dynamic links
 *                      > when posting arrays at end of sscan, use DBE_LOG, too
 *                      > Record timeStamp only at beginning of sscan
 *                      > Record positioner readbacks when reading detectors
 *                      > "TIME" or "time" in a readback PV records time
 *
 * 3.01 03-07-96  nda   Rearrange order of precedence of linear scan parameters
 *                      per Tim Mooney's memo duplicated at end of this file
 *
 * 3.02 03-12-96  nda   If scan request and any PV's are PV_NC, callback 3
 *                      seconds later ... if still PV_NC, abort. Also, if any
 *                      control PV's (positioners, readbacks, detctr triggers)
 *                      lose connection during a sscan, abort sscan.
 *
 * 3.02 03-12-96  nda   Changed detector reading to accommodate the possibility
 *                      of connections "coming and going" during a sscan: If a
 *                      detector PV is valid at the beginning of the sscan, the
 *                      value will be read each point. If the PV is not
 *                      connected for a given point, a zero will be stored.
 *                      Previously, nothing would be stored, which left old
 *                      data in the array for those points. Any detector PV's
 *                      added during a scan will be ignored until the next sscan
 *
 * 3.02 03-12-96  nda   Writing a 3 to the .CMND field will clear all PV's,
 *                      set all positioners to LINEAR & ABSOLUTE, resets all
 *                      freeze flags & .FFO, sets .SCAN to Passive,RETRACE=stay
 *
 * 3.03 04-26-96  tmm   Writing a 4 to the .CMND field will clear positioner
 *                      PV's, set all positioners to LINEAR & ABSOLUTE, reset
 *                      all freeze flags & .FFO, set .SCAN to Passive, set
 *                      RETRACE=stay
 *
 * 3.13 _______   nda   changes for 3.13
 *      03-15-96        - remove pads from detFields structure
 *                      - use fieldOffset indexes
 *
 * 3.14 02-10-98  tmm   added BUSY field.  BUSY stays true until all sscan-
 *                      related positioners & detectors are done.  Allow for
 *                      sscans with no positioners and no det triggers.
 * 3.15 03-08-98  tmm   if (start position == current position) add DBL_EPSILON
 * 3.16 03-27-98  tmm   BEFORE_SCAN_WAIT distinct from BEFORE_SCAN
 * 4.00 04-15-98  tmm   use putNotify for completion detection
 * 4.01 05-20-98  tmm   .EXSC is now pp field, implement positioner/detector
 *                      settling delays, pause function.
 * 4.02 07-10-98  tmm   Fixed on-the-fly mode.  Added accumulate mode: add
 *                      this sscan's data to that of previous sscan.
 * 4.03 10-02-98  tmm   "Some PV's not connected" --> "Waiting for PV's to
 *                      connect".  Forbid change of range,npts during sscan.
 * 4.04 11-09-98  tmm   If RnDL (readback delta) is negative, the delta against
 *                      which a positioning error compares is the step increment
 *                      multiplied by the integer nearest to -RnDL.  (Not
 *                      applicable in table mode, because the step increment is
 *                      unknown.)
 * 4.05 01-28-99  tmm   If scan is paused, process posts a message and returns.
 * 4.06 01-29-99  tmm   More pause changes: .exsc can never rescind .paus; if
 *						(.faze == IDLE and .paus), .exsc -> 1 is undone
 * 4.07 03-26-99  tmm   Allow specific CMND's during sscan.
 * 4.08 04-14-99  tmm   Handle scan request while scan already in progress;
 *                      abort waits for outstanding callbacks.  For now, allow
 *                      abort during retrace to succeed immediately.
 * 4.09 04-14-99  tmm   Add DATA field clients can use to determine when sscan
 *                      starts and finishes.  Previously, clients used EXSC,
 *                      but user can set this field, and scan record may not
 *                      be able to prepare data quickly enough for client.
 *                      Implemented the after-scan motion "go to peak position",
 *                      which uses the field REFD to specify the detector whose
 *                      data are to be searched for a peak. Get time stamp
 *                      at the beginning of the sscan--before posting anything.
 *                      Renamed as sscan record.
 * 4.10 08-24-99  tmm   pvSearchCallback was saying positioner PV was ok when
 *                      either of the two associated links (inlink and outlink)
 *                      connected.  Now it requires both to be connected, which
 *                      avoids a race condition that caused scans to hang.
 * 4.11 08-26-99  tmm   Rework handling of scan pending PV connections.
 * 4.12 08-27-99  tmm   Add field XSC, an internal copy of EXSC protected by
 *                      special() from user writes.
 * 5.00 09-29-99  tmm   Added more detectors D01* - D70* (total 70 added).  Keep
 *                      the old 15 D1* - DF* for now.  For specifying the ref
 *                      detector, D01* is detector 1, D1* is detector 71.
 * 5.01 11-30-99  tmm   Added acquisition mode GET_ARRAY, in which the scan record
 *                      simply grabs arrays of data acquired by some other record.
 *                      In this mode, the scan record moves positioners to their starting
 *                      positions, gets arrays of data (zero filled if fewer than npts
 *                      are gotten), sets cpt to npts, and behaves as though it acquired
 *                      the data.
 * 5.02 02-11-00  tmm   If recDynLinkPutCallback() returns nonzero, don't wait
 *                      for a callback.  (Duh!!! -- dipshit!)  Handle "fake" callback
 *                      sent by recDynLinkOut task when it thinks that a real callback
 *                      will not be issued.
 * 5.03 06-29-00  tmm   Fixed timing of VAL posting so it can be used to trigger
 *                      MEDM plot.
 * 5.04 07-10-00  tmm   Renew positioner links when a new scan starts so that we
 *                      get current limits data.
 * 5.05 09-27-00  tmm   #include <...> changed to #include "..."
 * 5.06 10-04-00  tmm   Second attempt to abort kills scan, even if there are outstanding
 *                      callbacks.
 * 5.07 11-09-00  tmm   Avoid lock-set conflict: if posMonCallback is the last of the three
 *                      positioner PV callbacks to come in, we used to wait for it in
 *                      checkScanLimits.  But this is called from process, after the lock set
 *                      is locked.  If the caget that will result in posMonCallback hasn't
 *                      occurred by this time (recDynLinkINP's event task is low priority), and the 
 *                      PV is part of the sscan record's lock set, the caget will wait for the
 *                      lock set to be unlocked, and we'll deadlock.  Now, we don't call scanOnce
 *                      until all three callbacks have come in.
 * 5.08 11-29-00  tmm   Fix move-to-peak algorithm.  Fix get_enum_strs() support for CMND 
 *                      field
 * 5.09 12-06-00  tmm   Scan pending positioner connections was starting after the *first*
 *                      completed connection had produced a callback, rather the last.
 * 5.10 01-25-01  tmm   Don't renew links unless last renew was more than X seconds ago.
 * 5.11 01-30-01  mlr   Changed DEF_WF_SIZE from 100 to 10.
 * 5.12 02-27-01  tmm   Fixed link problem during scan startup: if search callback for PnPV's
 *                      monitor link was received before the search callback for its output link,
 *                      then pvSearchCallback would not collect limit information for the link, and
 *                      stale limit info would be used in checkScanLimits().  Also, scans with more
 *                      one positioner could begin before all monitor callbacks had come in if a
 *                      particular ordering (maybe an impossible ordering) of search and monitor
 *                      callbacks occurred.
 * 5.13 04-27-01  tmm   Fixed various problems with PAUS, WAIT, and scan aborts, for example, what
 *                      happens when you abort a scan that was paused during a detector wait. 
 * 5.14 06-27-01  tmm   Macro for db_post_events.  If we can't get current positioner values in 
 *                      initScan(), abort.
 * 5.15 10-01-01  tmm   Looking for PPC problems involving floating-point exceptions.  Try to
 *                      allow npts == 1, for fly scans.  Fix ppvn pointer problems.  If PV name
 *                      is all whitespace, reset it to an empty string.
 * 5.16 03-15-02  tmm   Add CMND==5 to clear positioner PV's without changing anything else.
 * 5.17 10-17-02  tmm   ACQM (acquisition mode) used to specify scalar/arrays and normal/accumulate.
 *                      This meant that arrays could not accumulate from scan to scan.  Not good, so
 *                      a new field ACQT specifies scalar/array.  Improved move-to-peak and related
 *                      algorithms.
 * 5.18 04-07-03  tmm   Converted to 3.14.  Deleted D1*-Df* detectors
 * 5.18a 05-21-04 tmm   Added AWAIT handshake with data-storage client.
 *                      If not in array mode, and array-valued detectors have been specified,
 *                      trigger array-read link (new field) and read them when the callback comes in.
 * 5.19 07-10-03  rls   Bug fix for NUM_DET not matching 70 detectors in *.dbd file.
 * 5.20 07-08-04  tmm   Merged 3.13.x-compatible 5.18, with AWAIT and array read,
 *                      with 3.14.x-compatible 5.19, into 3.14.6-compatible 5.20
 * 5.21 08-18-04  tmm   get enum strings; FLNK if scan fails, so ca_put_callback() completes;
 *                      call scanOnce() from special if we're waiting for saveData and AWAIT==0;
 *                      if limit trouble stops scan, hang instead of declaring done.
 * 5.22 10-13-04  tmm   Use ca_get_callback() to acquire data.  Previously, the sscan record
 *                      relied on monitors coming in before the callback resulting from a
 *                      ca_put_callback -- whose processing triggered the monitors.  This was
 *                      almost always ok, but CA buffer overruns and TCP retries could result
 *                      in the sscan record acquiring stale data.
 * 5.23 10-22-04  tmm   Fixed get_callback stuff: process() wasn't resetting numGetCallbacks to
 *                      zero when a scan was killed; userGetCallback() wasn't changing dstate
 *                      after all array callbacks had arrived, so process wasn't routing execution
 *                      correctly; pvSearchCallback() wasn't changing puserPvt->nelem, or resetting
 *                      data-buffer pointers, unless nelem's previous value was zero; readArrays()
 *                      had code for array-valued positioners that would have crashed if it ever ran,
 *                      because recDynLink would not have allocated buffer memory for them, since we
 *                      specify that positioners are scalars in calls to recDynLinkAddInput;
 *                      readArrays() was using puserPvt->pAddr->no_elements for the number of
 *                      elements, but this only works for local PV's; readArrays() was not
 *                      changing dstate after queuing recDynLinkGetCallback() requests, so nobody
 *                      knew to wait for the callbacks; packData() wasn't looking for any change in
 *                      dstate after calling arrayRead(), so it didn't wait for callbacks.
 * 5.24 11-08-04  tmm   Abort no longer clears PAUS.
 * 5.25 01-14-05  tmm	Abort and killing scans reworked to allow sscan record to wait for
 *                      data-storage client even if it doesn't wait for anything else, to
 *                      abandon even that caution if user insists, and to show user where it
 *                      is in the process.
 * 5.25a 02-25-05  tmm  Before-scan link didn't work right in NoWait case.
 * 5.25b 03-11-05  tmm  ExportAddress for volatile variables.  Check for npts <=0 in
 *                      init_record.  Reject illegal npts in special().  ajdLinParms() now
 *                      checks that step-increment agrees with overall scan direction.
 *                      Freeze flags for start, end, and width are ignored in this check.
 *                      Probably should consider deleting all the freeze-flag stuff.
 * 5.26 03-16-05  tmm   Allow before-scan and after-scan links to write to
 *                      their own sscan record.
 * 5.27 11-02-05  tmm   Changed epicsMutexMustLock() to epicsMutexLock() for pvStatSem
 *                      Set recDynLinkPvt.connectInProgress for each link while it is
 *                      connecting, and clear when pvConnectCallback is called.  Use this
 *                      field to deny request to change a link while it's connecting.
 * 5.28 11-02-05  tmm   Added "Move to center of mass" after-scan option.
 * 5.29 03-07-06  tmm   Check that "Move To ..." can work before trying.
 * 5.29a 03-07-06  tmm  denyConnectCollision set to 0, else bad link can never be fixed.
 * 5.30 04-17-06  tmm   If recDynLinkPvt.connectInProgress, wait a while.
 * 5.31 06-03-06  tmm   If retrace fails because recDynLinkPutCallback() returned
 *                      an error for all retrace motors, then just skip the retrace.
 *                      Similarly, if aswait and after-scan link can't be written to,
 *                      skip it.  Replace #define'd ticsPerSecond with 1./epicsThreadSleepQuantum();
 * 5.32 07-12-06  tmm   Include calledBy to help trace execution.  Looking for ways to avoid
 *                      hanging if, e.g., a completion callback is missed.
 * 5.33 07-28-06  tmm   removed a race condition that caused scans to hang: doPuts was changing the
 *                      value of FAZE if it intended to do a putCallback, and then looking at the
 *                      value to see if it hadn't done any putCallbacks, and acting on that info.
 *                      But the resulting callbacks could have caused FAZE to change while doPuts
 *                      (itself running in a callback thread) was doing other things, like calling
 *                      sprintf().  Now doPuts uses a local variable to keep track of what it did.
 * 5.34 09-21-06  tmm   More fixes for pipelined data acquisition: cpt field of sscan record now
 *                      buffered in bcpt field along with data array.
 * 5.35 10-10-06  tmm   Autosaved PnSP, PnEP did not correctly initialize PnCP or PnWD.
 * 5.36 10-10-06  tmm   If AQCT="1D ARRAY", P1RA contains (0,1,...) for use as array index.
 * 5.37 10-12-06  tmm   If recDynLinkPvt.connectInProgress, wait for sscanRecordConnectWaitSeconds,
 *                      which may be zero.  Default is 1 second.  (We used to wait 5 seconds.)
 * 5.38 10-10-06  tmm   If AQCT="1D ARRAY", P1RA contains [p1sp, p1sp_p1si,...p1sp+N*p1si} for use as
 *                      array index.
 * 5.39 11-14-06  tmm   Don't complain about the number of elements in a positioner's table unless the
 *                      positioner has a bad PV, and is in table mode.  (Comment added on 3/27/07)
 * 5.40 03-27-07  tmm   special() was checking npts before it could see the new value ("if (!after)").
 *                      This prevented an illegal NPTS value (e.g., autorestored) from being fixed.
 *                      Added checks at beginning of process(), special(), and put_array_info()
 * 5.41 06-06-07  tmm   CMND == 6: clear positioner and readback PV's, set all positioners to LINEAR
 *                      & ABSOLUTE, reset all freeze flags & .FFO, set .SCAN to Passive, set
 *                      RETRACE=stay
 *                      CMND==7: clear positioner and readback PV's without changing anything else.
 * 5.5  05-05-08  tmm   v5.5: Post data arrays every ATIME seconds during a scan.  For these postings,
 *                      repeat last data point to user specified array element number, COPYTO.  At
 *                      end of scan, post data arrays with DBE_LOG, so clients can choose to ignore
 *                      intermediate postings.
 * 5.51 05-08-08  tmm   v5.51 If no positioner, and no readback, then store current point number
 *                      where we would have stored positioner value or time.
 * 5.52 06-19-08  tmm   v5.52 Fixed bug in get_array_info().  Calls about PnPA, for n>1,  were getting
 *               		wrong buffer pointer.
 * 5.53 07-22-08  tmm   v5.53 Removed race conditions affecting callback counters, and added mutex to
 *                      protect them.  Changed timing of when to renew positioner links from
 *                      now-last_scan_start to now-last_scan_end.
 */

#define VERSION 5.53


#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <ctype.h>

#include <alarm.h>
#include <dbDefs.h>
#include <dbAccess.h>
#include <dbEvent.h>
#include <dbScan.h>
#include <dbDefs.h>
#include <dbFldTypes.h>
#include <devSup.h>
#include <errMdef.h>
#include <recSup.h>
#include <recGbl.h>
#include <special.h>
#include <callback.h>
#include <taskwd.h>
#include <epicsMutex.h>		/* semaphore */
#include <epicsTimer.h>		/* access to timers for delayed callbacks */
#include <epicsThread.h>	/* for epicsThreadSleepQuantum() */
#include <dbStaticLib.h>	/* for enumStrings stuff */
#include <epicsVersion.h>       /* for LT_EPICSBASE macro */

#include	"recDynLink.h"
#include "epicsExport.h"

#define GEN_SIZE_OFFSET
#include "sscanRecord.h"
#undef  GEN_SIZE_OFFSET

#define denyConnectCollision 0

/* The following macro assumes psscan points to an instance of the sscan record */
#define POST(A) (db_post_events(psscan, (A), DBE_VALUE))

/************ begin special values.  These must agree with the .dbd file! *********/
#define SPC_SC_S                111   /* start position   */
#define SPC_SC_I                112   /* step increment   */
#define SPC_SC_E                113   /* end position     */
#define SPC_SC_C                114   /* center position  */
#define SPC_SC_W                115   /* width            */
#define SPC_SC_N                116   /* # of steps       */
#define SPC_SC_FFO              117   /* FrzFlag Override */
#define SPC_SC_F                118   /* FrzFlag Changing */
#define SPC_SC_DPT              119   /* Desired Pt       */
#define SPC_SC_MO               120   /* scan mode        */

/************ end special values *********/

#define NINT(f)	(long)((f)>0 ? (f)+0.5 : (f)-0.5)
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
/* Less than EPICS base version test.*/
#define LT_EPICSBASE(v,r,l) ((EPICS_VERSION<=(v)) && (EPICS_REVISION<=(r)) && (EPICS_MODIFICATION<(l)))

/***************************
  Declare constants
***************************/

#define OK               0
#define ERROR           (-1)
#define DEF_WF_SIZE     10
#define PVN_SIZE        40

/* linkType */
#define POSITIONER      0	/* positioner inlink, sometimes this serves for both in and out */
#define READBACK        1	/* input from a positioner's readback */
#define DETECTOR        2	/* input from a detector */
#define TRIGGER         3	/* cause detectors to acquire */
#define BS_AS_LINK      4	/* cause before-scan or after-scan action */
#define POSITIONER_OUT  5	/* positioner outlink */
#define READ_ARRAY_TRIG	6  	/* cause array-valued detectors to read already acquired data */

#define NUM_POS         4
#define NUM_RDKS        4
#define NUM_DET         70
#define NUM_TRGS        4
#define NUM_ATRGS       1
#define NUM_MISC        2	/* Before Scan PV, After Scan PV */
#define NUM_PVS         (NUM_POS + NUM_RDKS + NUM_DET + NUM_TRGS + NUM_ATRGS + NUM_MISC)
/* Determine total # of dynLinks (each positioner requires two: IN & OUT */
#define NUM_LINKS       (NUM_PVS + NUM_POS)

static char linkNames[NUM_LINKS][6] =
{"P1", "P2", "P3", "P4",
 "R1", "R2", "R3", "R4",
 "D01", "D02", "D03", "D04", "D05", "D06", "D07", "D08", "D09", "D10",
 "D11", "D12", "D13", "D14", "D15", "D16", "D17", "D18", "D19", "D20",
 "D21", "D22", "D23", "D24", "D25", "D26", "D27", "D28", "D29", "D30",
 "D31", "D32", "D33", "D34", "D35", "D36", "D37", "D38", "D39", "D40",
 "D41", "D42", "D43", "D44", "D45", "D46", "D47", "D48", "D49", "D50",
 "D51", "D52", "D53", "D54", "D55", "D56", "D57", "D58", "D59", "D60",
 "D61", "D62", "D63", "D64", "D65", "D66", "D67", "D68", "D69", "D70",
 "T1", "T2", "T3", "T4",
 "A1",
 "BS", "AS",
 "P1mon", "P2mon", "P3mon", "P4mon"
};

/* linkIndex */
#define P1_IN       0
#define R1_IN       (NUM_POS)
#define D1_IN       (NUM_POS + NUM_RDKS)
#define T1_OUT      (D1_IN + NUM_DET)
#define A1_OUT      (T1_OUT + NUM_TRGS)
#define BS_OUT      (A1_OUT + NUM_ATRGS)
#define AS_OUT      (BS_OUT + 1)

/* Added four recDynLinks at the end of PV's for positioner outs */
#define P1_OUT      (NUM_PVS)

#define MIN_MON         .05	/* minimum time in seconds between monitor postings. */

/* PV status: if (PvStat & PV_NC) then PV link is bad */
/* NOTE: the following must agree with .dbd file */
#define PV_OK		0
#define NO_PV		1
#define PV_NoRead	2		/* part of a bit map */
#define PV_NoWrite	4		/* part of a bit map */
#define PV_NC		6		/* set bits define a bit map */

#define A_BUFFER        0
#define B_BUFFER        1

/* CMND field */
#define CLEAR_MSG           	0
#define CHECK_LIMITS        	1
#define PREVIEW_SCAN        	2	/* Preview the SCAN positions */
#define CLEAR_RECORD        	3	/* Clear PV's, frzFlags, modes, abs/rel, etc */
#define CLEAR_POSITIONERS   	4	/* Clear positioner PV's, frzFlags, modes, abs/rel, etc */
#define CLEAR_POSITIONER_PVS	5	/* Clear positioner PV's */
#define CLEAR_POSandRDBK		6	/* Clear positioner and readback PV's, frzFlags, modes, abs/rel, etc */
#define CLEAR_POSandRDBK_PVS	7	/* Clear positioner and readback PV's */

#define DBE_VAL_LOG     (DBE_VALUE | DBE_LOG)


/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long     init_record();
static long     process();
static long     special();
#define get_value NULL
static long     cvt_dbaddr();
static long     get_array_info();
static long     put_array_info();
static long     get_units();
static long     get_precision();
static long     get_enum_str();
static long     get_enum_strs();
static long     put_enum_str();
static long     get_graphic_double();
static long     get_control_double();
static long     get_alarm_double();

rset     sscanRSET = {
	RSETNUMBER,
	report,
	initialize,
	init_record,
	process,
	special,
	get_value,
	cvt_dbaddr,
	get_array_info,
	put_array_info,
	get_units,
	get_precision,
	get_enum_str,
	get_enum_strs,
	put_enum_str,
	get_graphic_double,
	get_control_double,
	get_alarm_double
};
epicsExportAddress(rset, sscanRSET);

typedef struct recDynLinkPvt {
	sscanRecord	*psscan;		/* pointer to scan record */
	unsigned short		linkIndex;	/* specifies which dynamic link */
	unsigned short		linkType;	/* Positioner, Rdbk, Trig, Det */
	struct dbAddr		*pAddr;		/* Pointer to dbAddr for local PV's */
	long				dbAddrNv;	/* Zero if dbNameToAddr succeeded */
	unsigned long		nelem;		/* # of elements for this PV  */
	unsigned short		ts;			/* if 1, use timestamp as value */
	short				useDynLinkAlways;
	epicsTimeStamp      lookupTime;	/* used to determine time of last lookupPv */
	short				connectInProgress;	/* to avoid starting a new connection */
											/* while a connection is being made */
} recDynLinkPvt;

/* Note that the following structure must exactly match the data returned by
 * dbGet(paddr, DBR_FLOAT, buf, &options, ...), with 'options' =
 * DBR_UNITS | DBR_PRECISION | DBR_GR_DOUBLE | DBR_CTRL_DOUBLE
 * The structure is used only in calls to dbGet(), and only to get the information
 * indicated in 'options'.
 */
typedef struct dynLinkInfo {
	DBRunits
	DBRprecision
	DBRgrDouble
	DBRctrlDouble
	float           value;
} dynLinkInfo;

typedef struct posBuffers {
	double         *pFill;
	double         *pBufA;
	double         *pBufB;
} posBuffers;

typedef struct detBuffers {
	float          *pFill;
	float          *pBufA;
	float          *pBufB;
} detBuffers;


/* the following structure must match EXACTLY with the order and type of
   fields defined in sscanRecord.h for each positioner (even including
   the "Created Pad"s  */

typedef struct posFields {
	double          p_pp;	/* P1 Previous Position */
	double          p_cv;	/* P1 Current Value */
	double          p_dv;	/* P1 Desired Value */
	double          p_lv;	/* P1 Last Value Posted */
	double          p_sp;	/* P1 Start Position */
	double          p_si;	/* P1 Step Increment */
	double          p_ep;	/* P1 End Position */
	double          p_cp;	/* P1 Center Position */
	double          p_wd;	/* P1 Scan Width */
	double          r_cv;	/* P1 Readback Value */
	double          r_lv;	/* P1 Rdbk Last Val Pst */
	double          r_dl;	/* P1 Readback Delta */
	double          p_hr;	/* P1 High Oper Range */
	double          p_lr;	/* P1 Low  Oper Range */
	double         *p_pa;	/* P1 Step Array */
	double         *p_ra;	/* P1 Readback Array */
	double         *p_ca;	/* P1 Current Readback Array */
	unsigned short  p_fs;	/* P1 Freeze Start Pos */
	unsigned short  p_fi;	/* P1 Freeze Step Inc */
	unsigned short  p_fe;	/* P1 Freeze End Pos */
	unsigned short  p_fc;	/* P1 Freeze Center Pos */
	unsigned short  p_fw;	/* P1 Freeze Width */
	unsigned short  p_sm;	/* P1 Step Mode */
	unsigned short  p_ar;	/* P1 Absolute/Relative */
	char            p_eu[16];	/* P1 Engineering Units */
	short           p_pr;	/* P1 Display Precision */
} posFields;

/* the following structure must match EXACTLY with the order and type of
   fields defined in sscanRecord.h for each detector (even including
   the "Created Pad"s  */

typedef struct detFields {
	double          d_hr;	/* D1 High Oper Range */
	double          d_lr;	/* D1 Low  Oper Range */
	float          *d_da;	/* D1 Data Array */
	float          *d_ca;	/* D1 Current Data Array */
	float           d_cv;	/* D1 Current Value */
	float           d_lv;	/* D1 Last Value Posted */
	unsigned long   d_ne;	/* D1 # of Elements/Pt */
	char            d_eu[16];	/* D1 Engineering Units */
	short           d_pr;	/* D1 Display Precision */
} detFields;

/* calledBy values */
#define UNKNOWN					0x00
#define SPECIAL_PAUS			0x01
#define SPECIAL_EXSC			0x02
#define SPECIAL_WAIT			0x03
#define SPECIAL_AWAIT			0x04
#define NOTIFY_TRIG				0x05
#define NOTIFY_READ_ARRAY_TRIG	0x06
#define NOTIFY					0x07
#define USERGETCALLBACK			0x08
#define SEARCH					0x09
#define POSMON					0x0a
#define DO_PUTS					0x0b
#define DO_PUTS_TRIG			0x0c
#define DELAY					0x10

typedef struct recPvtStruct {
	/* THE CODE ASSUMES doPutsCallback is THE FIRST THING IN THIS STRUCTURE! */
	CALLBACK        doPutsCallback;	/* do output links callback structure */
	CALLBACK        dlyCallback;	/* implement delays callback structure */
	short           validBuf;		/* which data array buffer is valid */
	recDynLink      caLinkStruct[NUM_LINKS];	/* req'd for recDynLink */
	posBuffers      posBufPtr[NUM_POS];
	detBuffers      detBufPtr[NUM_DET];
	unsigned short  valPosPvs;		/* # of valid positioners */
	unsigned short  valRdbkPvs;		/* # of valid Readbacks   */
	unsigned short  valDetPvs;		/* # of valid Detectors */
	unsigned short  valTrigPvs;		/* # of valid Det Triggers  */
	unsigned short  valATrigPvs;	/* # of valid array-read triggers  */
	unsigned short  acqDet[NUM_DET];	/* which detectors to acquire */
	dynLinkInfo    *pDynLinkInfo;
	short           pffo;			/* previous state of ffo */
	short           fpts;			/* backup copy of all freeze flags */
	unsigned short  prevSm[NUM_POS];	/* previous states of p_sm */
	posFields       posParms[NUM_POS];	/* backup copy of all pos parms */
	unsigned long   tablePts[NUM_POS];	/* # of pts loaded in P_PA */
	short           onTheFly;
	short           flying;
	float          *nullArray;
	float          *nullArray2;
	epicsTimeStamp  timeStart;			/* used to time the scan */
	epicsTimeStamp  lastScanEndTime;	/* to determine whether we should renew positioner links */
	unsigned char   scanErr;
	unsigned char   badOutputPv;	/* positioner, detector trig, readbk */
	unsigned char   badInputPv;		/* detector BAD_PV */
	char            nptsCause;		/* who caused the "# of points to
					 * change: -1:operator; 0-3
					 * Positioners */
	short           numPositionerCallbacks;	/* count positioner callbacks */
	short           numTriggerCallbacks;	/* count trigger callbacks */
	short           numAReadCallbacks;		/* count array-read callbacks */
	short           numGetCallbacks;		/* count get callbacks */
	short			prevACQM;
	long			prevNumPts;
	short			scanBySearchCallback;
	epicsMutexId	pvStatSem;
	double			*dataBuffer;
	short			userSetAWAIT;
	short			calledBy;
	epicsMutexId	numCallbacksSem;
} recPvtStruct;

/* enum strings */
int sscanFAZE_numStrings;
char **sscanFAZE_strings;
int sscanDSTATE_numStrings;
char **sscanDSTATE_strings;
int sscanPASM_numStrings;
char **sscanPASM_strings;

/*  forward declarations */
static void		checkMonitors(sscanRecord *psscan);
static long		initScan(sscanRecord *psscan);
static void		contScan(sscanRecord *psscan);
static void		endScan(sscanRecord *psscan);
static void 	readArrays(sscanRecord *psscan);
static void		copyLastPoint(sscanRecord *psscan, long pointNumber, long copyTo);
static void		packData(sscanRecord *psscan, int caller);
static void		afterScan(sscanRecord *psscan);
static void		doPuts(CALLBACK *pCB);
static void		adjLinParms(struct dbAddr *paddr);
static void		changedNpts(sscanRecord *psscan);
static long		checkScanLimits(sscanRecord *psscan);
static void		saveFrzFlags(sscanRecord *psscan);
static void		resetFrzFlags(sscanRecord *psscan);
static void		restoreFrzFlags(sscanRecord *psscan);

static void		previewScan(sscanRecord * psscan);

static void		lookupPV(sscanRecord * psscan, unsigned short i);
static void		checkConnections(sscanRecord * psscan);
static void		pvSearchCallback(recDynLink * precDynLink);
static void		posMonCallback(recDynLink * precDynLink);
static void		notifyCallback(recDynLink * precDynLink);
static void		delayCallback(CALLBACK *pCB);
static void		restorePosParms(sscanRecord * psscan, unsigned short i);
static void		savePosParms(sscanRecord * psscan, unsigned short i);
static void		zeroPosParms(sscanRecord * psscan, unsigned short i);

static double ticsPerSecond;
/* variables ... */
volatile int	sscanRecordDebug = 0;
epicsExportAddress(int, sscanRecordDebug);
volatile int	sscanRecordViewPos = 0;
epicsExportAddress(int, sscanRecordViewPos);
volatile int	sscanRecordDontCheckLimits = 0;
epicsExportAddress(int, sscanRecordDontCheckLimits);
volatile int	sscanRecordLookupTime = 1;
epicsExportAddress(int, sscanRecordLookupTime);
volatile int	sscanRecordConnectWaitSeconds = 1;
epicsExportAddress(int, sscanRecordConnectWaitSeconds);

static int isBlank(char *name)
{
	int i;

	for (i=0; name[i]; i++) {
		if (!(isspace((int)name[i]))) return(0);
	}
	return((i>0));
}

/* safe double to float conversion -- stolen from dbConvert.c */
static void safeDoubleToFloat(double *pd,float *pf)
{
    double abs = fabs(*pd);
    if (*pd==0.0) {
        *pf = 0.0;
    } else if(abs>=FLT_MAX) {
        if(*pd>0.0) *pf = FLT_MAX; else *pf = -FLT_MAX;
    } else if(abs<=FLT_MIN) {
        if(*pd>0.0) *pf = FLT_MIN; else *pf = -FLT_MIN;
    } else {
        *pf = *pd;
    }
}

static long 
init_record(sscanRecord *psscan, int pass)
{
	recPvtStruct   *precPvt = (recPvtStruct *) psscan->rpvt;
	posFields      *pPos;
	detFields      *pDet;

	char           *ppvn;
	unsigned short *pPvStat;
	unsigned short  i;

	recDynLinkPvt  *puserPvt;

	DBENTRY	dbentry;
	DBENTRY	*pdbentry = &dbentry;

	if (pass == 0) {
		psscan->vers = VERSION;
		if (psscan->mpts < DEF_WF_SIZE)
			psscan->mpts = DEF_WF_SIZE;

		ticsPerSecond = NINT(1./epicsThreadSleepQuantum());

		/* First time through, rpvt needs initialized */
		psscan->rpvt = calloc(1, sizeof(recPvtStruct));
		precPvt = (recPvtStruct *) psscan->rpvt;

		psscan->faze = sscanFAZE_IDLE; POST(&psscan->faze);
		precPvt->numPositionerCallbacks = 0;
		precPvt->numTriggerCallbacks = 0;
		precPvt->numAReadCallbacks = 0;
		precPvt->numGetCallbacks = 0;
		precPvt->userSetAWAIT = 0;

		precPvt->prevSm[0] = psscan->p1sm;
		precPvt->prevSm[1] = psscan->p2sm;
		precPvt->prevSm[2] = psscan->p3sm;
		precPvt->prevSm[3] = psscan->p4sm;

		precPvt->pDynLinkInfo = (dynLinkInfo *) calloc(1, sizeof(dynLinkInfo));

		/* init the private area of the caLinkStruct's   */
		for (i = 0; i < (NUM_LINKS); i++) {
			precPvt->caLinkStruct[i].puserPvt
				= calloc(1, sizeof(struct recDynLinkPvt));
			puserPvt = (recDynLinkPvt *) precPvt->caLinkStruct[i].puserPvt;
			puserPvt->psscan = psscan;
			puserPvt->linkIndex = i;
			puserPvt->pAddr = calloc(1, sizeof(struct dbAddr));
			puserPvt->dbAddrNv = -1;
			if (i < R1_IN) { /* positioner inlink */
				puserPvt->linkType = POSITIONER;
				puserPvt->useDynLinkAlways = 1;
			}
			else if (i < D1_IN) {
				puserPvt->linkType = READBACK;
			}
			else if (i < T1_OUT) {
				puserPvt->linkType = DETECTOR;
			}
			else if (i < A1_OUT) {
				puserPvt->linkType = TRIGGER;
				puserPvt->useDynLinkAlways = 1;
			}
			else if (i < BS_OUT) {
				puserPvt->linkType = READ_ARRAY_TRIG;
				puserPvt->useDynLinkAlways = 1;
			}
			else if (i < P1_OUT) {
				puserPvt->linkType = BS_AS_LINK;
				puserPvt->useDynLinkAlways = 1;
			}
			else { /* positioner outlink */
				puserPvt->linkType = POSITIONER_OUT;
				puserPvt->useDynLinkAlways = 1;
			}
		}

		psscan->p1pa = (double *) calloc(psscan->mpts, sizeof(double));
		psscan->p2pa = (double *) calloc(psscan->mpts, sizeof(double));
		psscan->p3pa = (double *) calloc(psscan->mpts, sizeof(double));
		psscan->p4pa = (double *) calloc(psscan->mpts, sizeof(double));

		/* Readbacks need double buffering. Allocate space and initialize */
		/* Fill pointer and readback array pointers */
		precPvt->validBuf = A_BUFFER;
		pPos = (posFields *) & psscan->p1pp;
		for (i = 0; i < NUM_RDKS; i++, pPos++) {
			precPvt->posBufPtr[i].pBufA =
				(double *) calloc(psscan->mpts, sizeof(double));
			precPvt->posBufPtr[i].pBufB =
				(double *) calloc(psscan->mpts, sizeof(double));

			pPos->p_ra = precPvt->posBufPtr[i].pBufA;
			precPvt->posBufPtr[i].pFill = precPvt->posBufPtr[i].pBufB;
		}

		/* allocate arrays for 4 detectors assuming nelem = 1 */
		/* (previewScan uses the first four detectors) */
		/* For now, just point other detectors to a NULL array */

		precPvt->nullArray = (float *) calloc(psscan->mpts, sizeof(float));
		precPvt->nullArray2 = (float *) calloc(psscan->mpts, sizeof(float));
		pDet = (detFields *) & psscan->d01hr;
		for (i = 0; i < NUM_DET; i++, pDet++) {
			puserPvt = (recDynLinkPvt *) precPvt->caLinkStruct[D1_IN + i].puserPvt;
			if (i < 4) {
				precPvt->detBufPtr[i].pBufA = (float *) calloc(psscan->mpts, sizeof(float));
				precPvt->detBufPtr[i].pBufB = (float *) calloc(psscan->mpts, sizeof(float));
				puserPvt->nelem = 1;
			} else {
				precPvt->detBufPtr[i].pBufA = precPvt->nullArray;
				precPvt->detBufPtr[i].pBufB = precPvt->nullArray;
				puserPvt->nelem = 0;
			}
			pDet->d_da = precPvt->detBufPtr[i].pBufA;
			precPvt->detBufPtr[i].pFill = precPvt->detBufPtr[i].pBufB;
		}

		if ((precPvt->pvStatSem = epicsMutexCreate()) == 0) {
			printf("%s:init_record: could not create semaphore\n", psscan->name);
			return(-1);
		}

		if ((precPvt->numCallbacksSem = epicsMutexCreate()) == 0) {
			printf("%s:init_record: could not create semaphore\n", psscan->name);
			return(-1);
		}

		/* allocate data buffer for array operations (when .acqm = scanACQM_ARRAY) */
		precPvt->dataBuffer = (double *) calloc(psscan->mpts, sizeof(double));

		return (0);
	}
	precPvt = (recPvtStruct *) psscan->rpvt; /* pass 1 will not have done this */
	epicsTimeGetCurrent(&precPvt->timeStart);
	epicsTimeGetCurrent(&precPvt->lastScanEndTime);

	/* Make sure npts is reasonable */
	if (psscan->npts > psscan->mpts) psscan->npts = psscan->mpts;
	if (psscan->npts <= 0) psscan->npts = 1;

	callbackSetCallback(doPuts, &precPvt->doPutsCallback);
	callbackSetPriority(psscan->prio, &precPvt->doPutsCallback);
	callbackSetUser((void *)psscan, &precPvt->doPutsCallback);

	callbackSetCallback(delayCallback, &precPvt->dlyCallback);
	callbackSetPriority(psscan->prio, &precPvt->dlyCallback);
	callbackSetUser((void *)psscan, &precPvt->dlyCallback);

	/* initialize all linear scan fields */
	precPvt->nptsCause = -1;/* resolve all positioner parameters */
	changedNpts(psscan);

	if (psscan->ffo) {
		saveFrzFlags(psscan);
		resetFrzFlags(psscan);
	}
	/* init field values */
	psscan->exsc = 0;
	psscan->xsc = 0;
	psscan->pxsc = 0;
	psscan->data = 0;

	ppvn = &psscan->p1pv[0];
	pPvStat = &psscan->p1nv;

	/* check all dynLink PV's for non-NULL  */
	for (i = 0; i < NUM_PVS; i++, pPvStat++, ppvn += PVN_SIZE) {
		if (isBlank(ppvn)) {
			ppvn[0] = '\0';
			POST(ppvn);
			*pPvStat = NO_PV;
		} else {
			if (ppvn[0] != '\0') {
				*pPvStat = PV_NC;
				lookupPV(psscan, i);
			} else {
				*pPvStat = NO_PV;
			}
		}
	}

	psscan->dstate = sscanDSTATE_UNPACKED; POST(&psscan->dstate);

	/* get menu choices for selected fields */
	dbInitEntry(pdbbase, pdbentry);
	dbFindRecord(pdbentry, psscan->name);
	dbFindField(pdbentry, "FAZE");
	sscanFAZE_numStrings = dbGetNMenuChoices(pdbentry);
	sscanFAZE_strings = dbGetMenuChoices(pdbentry);
	dbFindField(pdbentry, "DSTATE");
	sscanDSTATE_numStrings = dbGetNMenuChoices(pdbentry);
	sscanDSTATE_strings = dbGetMenuChoices(pdbentry);
	dbFindField(pdbentry, "PASM");
	sscanPASM_numStrings = dbGetNMenuChoices(pdbentry);
	sscanPASM_strings = dbGetMenuChoices(pdbentry);
	dbFinishEntry(pdbentry);

	return (0);
}

static long 
process(sscanRecord *psscan)
{
	recPvtStruct   *precPvt = (recPvtStruct *) psscan->rpvt;
	long            status = 0;
	epicsTimeStamp	timeCurrent;
	int numPosCb, numTrigCb, numAReadCb, numGetCb;

	epicsMutexLock(precPvt->numCallbacksSem);
	numPosCb = precPvt->numPositionerCallbacks;
	numTrigCb = precPvt->numTriggerCallbacks;
	numAReadCb = precPvt->numAReadCallbacks;
	numGetCb = precPvt->numGetCallbacks;
	epicsMutexUnlock(precPvt->numCallbacksSem);

	if (sscanRecordDebug) {
		epicsPrintf("%s:process:entry:faze='%s', nPTRG_CBs=%1d_%1d_%1d_%2d, xsc=%d, pxsc=%d, calledBy 0x%x\n",
			psscan->name, sscanFAZE_strings[psscan->faze], numPosCb, numTrigCb, numAReadCb, numGetCb,
			psscan->xsc, psscan->pxsc, precPvt->calledBy);
	}

	/* Make sure npts is reasonable.  Autosave might have  changed it after init_record. */
	if (psscan->npts > psscan->mpts) {psscan->npts = psscan->mpts; POST(&psscan->npts);}
	if (psscan->npts <= 0) {psscan->npts = 1; POST(&psscan->npts);}

	if (psscan->kill) {
		if (sscanRecordDebug>=5) printf("%s:process: kill\n", psscan->name);
		if (psscan->wait) {psscan->wait = 0; POST(&psscan->wait);}
		if (psscan->wcnt) {psscan->wcnt = 0; POST(&psscan->wcnt);}
		if (psscan->wtng) {psscan->wtng = 0; POST(&psscan->wtng);}
		if (numPosCb) {
			sprintf(psscan->smsg, "NOTE: positioner still active");
			POST(&psscan->smsg);
		} else if (numTrigCb) {
			sprintf(psscan->smsg, "NOTE: detector still active");
			POST(&psscan->smsg);
		} else if (numAReadCb) {
			sprintf(psscan->smsg, "NOTE: array-read still active");
			POST(&psscan->smsg);
		} else if (numGetCb) {
			sprintf(psscan->smsg, "NOTE: outstanding getCallback(s)");
			POST(&psscan->smsg);
		}
		psscan->alrt = 0; POST(&psscan->alrt);
		epicsMutexLock(precPvt->numCallbacksSem);
		numPosCb = (precPvt->numPositionerCallbacks = 0);
		numTrigCb = (precPvt->numTriggerCallbacks = 0);
		numAReadCb = (precPvt->numAReadCallbacks = 0);
		numGetCb = (precPvt->numGetCallbacks = 0);
		epicsMutexUnlock(precPvt->numCallbacksSem);

		if (psscan->dstate <= sscanDSTATE_PACKED) {
			if (psscan->await && !(precPvt->userSetAWAIT)) {
				/*
				 * We set await after posting last scan's data.  Since data-storage
				 * client did not set AWAIT, it's possible the data-storage client isn't
				 * running, or for some other reason is not going to reset await.  Since
				 * this is a kill, we reset it.
				 */
				 psscan->await = 0; POST(&psscan->await);
			}
			/*
			 * we're killing: we can bypass everything to do with this scan,
			 * but we can't pull data buffers (last scan's data) out from under 
			 * data-storage client.
			 */
			if (psscan->dstate == sscanDSTATE_ARRAY_READ_WAIT) {
				/*
				 * we've already triggered array read, and user apparently
				 * doesn't want to wait for it to complete.  Now we wait for
				 * data-storage client to finish writing last scan's data.
				 */
				psscan->dstate = sscanDSTATE_SAVE_DATA_WAIT;
				POST(&psscan->dstate);
			} else if ((psscan->dstate == sscanDSTATE_SAVE_DATA_WAIT) && (psscan->kill > 2)) {
				/*
				 * If data-storage client is able to write scan data, we should never
				 * get to this point.  User evidently wants to abort without waiting for
				 * data storage.
				 */
				 psscan->dstate = sscanDSTATE_PACKED; POST(&psscan->dstate);
				 psscan->await = 0; POST(&psscan->await);
				 sprintf(psscan->smsg, "Abandoning unsaved scan data"); POST(&psscan->smsg);
				 printf("%s:process(): Abandoning unsaved scan data\n", psscan->name);
			}
			packData(psscan, 0);
			checkMonitors(psscan);
		}
		if (psscan->dstate == sscanDSTATE_SAVE_DATA_WAIT) {
			/*precPvt->calledBy = UNKNOWN;*/
			return(status);
		}

		psscan->busy = 0; POST(&psscan->busy);
		psscan->faze = sscanFAZE_IDLE; POST(&psscan->faze);
		recGblFwdLink(psscan);
		psscan->pxsc = psscan->xsc;
		recGblResetAlarms(psscan);

		psscan->kill = 0;
		psscan->pact = FALSE;
		/*precPvt->calledBy = UNKNOWN;*/
		return (status);
	}

	if (psscan->xsc) {
		/* Make sure it's ok to go */
		if (psscan->paus) {
			sprintf(psscan->smsg, "Scan is paused ...");
			POST(&psscan->smsg);
			/*precPvt->calledBy = UNKNOWN;*/
			return(-1);
		}
		if (psscan->wtng) {
			sprintf(psscan->smsg, "waiting for client ...");
			POST(&psscan->smsg);
			/*precPvt->calledBy = UNKNOWN;*/
			return(-1);
		}
	}

	if (precPvt->scanBySearchCallback) {
		/*
		 * pvSearchCallback says it's now ok to start a scan that was pending PV
		 * connections.  There is a race condition here:
		 * 1) special notices bad PV's and starts the process of resolving them,
		 * which ends with pvSearchCallback noticing that all are now good and
		 * calling scanOnce().
		 * 2) after special returns, EPICS processes the record; process(),
		 * noticing that all PV's are now good, starts the scan.
		 *
		 * If the scanOnce() call is delayed, we're going to get called while
		 * the scan is already in progress.  To handle this, pvSearchCallback
		 * sets precPvt->scanBySearchCallback, and we check for an unneeded
		 * call to process.
		 */
		if (sscanRecordDebug >= 2) printf("%s:process: processed by pvSearchCallback\n", psscan->name);
		precPvt->scanBySearchCallback = 0;
		if (psscan->faze != sscanFAZE_SCAN_PENDING) {
			printf("%s:process: Ignoring redundant scanBySearchCallback\n", psscan->name);
			return(status);
		}
	}

	if (psscan->faze == sscanFAZE_SCAN_PENDING) {
		checkConnections(psscan);
		if (!psscan->xsc || precPvt->badOutputPv || precPvt->badInputPv) {
			/*precPvt->calledBy = UNKNOWN;*/
			return (status);
		} else {
			if (sscanRecordDebug >= 2) printf("%s:process: unpending scan\n", psscan->name);
			psscan->alrt = 0; POST(&psscan->alrt);
		}
	}

	if (psscan->busy && psscan->xsc && (numPosCb || numTrigCb || numAReadCb || numGetCb)) {
		if (sscanRecordDebug >= 2) {
			printf("%s:process already busy faze='%s', nPCBs=%d, nTCBs=%d, nRCBs=%d, nGCBs=%d, xsc=%d, pxsc=%d\n",
				psscan->name, sscanFAZE_strings[psscan->faze], numPosCb,
				numTrigCb, numAReadCb, numGetCb, psscan->xsc, psscan->pxsc);
		}
		if (psscan->paus) {
			sprintf(psscan->smsg, "Scan is paused");
		} else {
			sprintf(psscan->smsg, "Already busy! PTAG_CBs=%1d_%1d_%1d_%02d; CB=0x%x", numPosCb,
				numTrigCb, numAReadCb, numGetCb, precPvt->calledBy);
		}
		POST(&psscan->smsg);
		/*precPvt->calledBy = UNKNOWN;*/
		return(0);
	}

	if ((psscan->pxsc == 0) && (psscan->xsc == 1)) {
		/* Brand new scan */
		if (psscan->busy) {
			sprintf(psscan->smsg, "Still busy! PTAG_CBs=%1d_%1d_%1d_%02d; CB=0x%x", numPosCb,
				numTrigCb, numAReadCb, numGetCb, precPvt->calledBy);
			/*precPvt->calledBy = UNKNOWN;*/
			return (status);
		}
		/* use TimeStamp to record beginning of scan */
		recGblGetTimeStamp(psscan);
		psscan->dstate = sscanDSTATE_UNPACKED; POST(&psscan->dstate);
		psscan->data = 0;
		db_post_events(psscan, &psscan->data, DBE_VAL_LOG);
		if (sscanRecordDebug >= 5) printf("%s:process: new sscan\n", psscan->name);
		if (psscan->wait) {psscan->wait = 0; POST(&psscan->wait);}
		if (psscan->wcnt) {psscan->wcnt = 0; POST(&psscan->wcnt);}
		if (psscan->wtng) {psscan->wtng = 0; POST(&psscan->wtng);}

		epicsMutexLock(precPvt->numCallbacksSem);
		numPosCb = (precPvt->numPositionerCallbacks = 0);
		numTrigCb = (precPvt->numTriggerCallbacks = 0);
		numAReadCb = (precPvt->numAReadCallbacks = 0);
		numGetCb = (precPvt->numGetCallbacks = 0);
		epicsMutexUnlock(precPvt->numCallbacksSem);

		psscan->faze = sscanFAZE_INIT_SCAN; POST(&psscan->faze);
		psscan->busy = 1; POST(&psscan->busy);
		status = initScan(psscan);
	} else if ((psscan->pxsc == 1) && (psscan->xsc == 0)) {
		/* Operator abort */
		if (numPosCb || numTrigCb || numAReadCb || numGetCb) {
			/*
			 * Don't actually have to wait for numTriggerCallbacks==0.
			 * Don't actually have to wait for numPositionerCallbacks==0 unless psscan->pasm:
			 * if psscan->pasm, endScan() will do a positioner move, which will fail
			 * if a positioner is still moving as the result of a ca_put_callback.
			 * But we want to wait for callbacks so they don't come during the next scan.
			 * If user aborts again, special() will set .KILL, and then we can be more
			 * aggressive about getting the scan over with.
			 */
			if (psscan->dstate < sscanDSTATE_PACKED) {
				packData(psscan, 1);
				checkMonitors(psscan);
			}
			sprintf(psscan->smsg, "Abort: waiting for callback(s)");
			POST(&psscan->smsg);
			/*precPvt->calledBy = UNKNOWN;*/
			return(status);
		} else {
			sprintf(psscan->smsg, "Scan aborted by operator");
			POST(&psscan->smsg);
			if (psscan->wait) {psscan->wait = 0; POST(&psscan->wait);}
			if (psscan->wcnt) {psscan->wcnt = 0; POST(&psscan->wcnt);}
			if (psscan->wtng) {psscan->wtng = 0; POST(&psscan->wtng);}
			/* if (psscan->paus) {psscan->paus = 0; POST(&psscan->paus);} */
			printf("%s:process: Scan aborted by operator\n", psscan->name);
			endScan(psscan);
		}
	} else if (psscan->faze == sscanFAZE_BEFORE_SCAN_WAIT) {
		/* Before Scan Fwd Link is complete, initScan again */
		status = initScan(psscan);
	} else if (psscan->xsc == 1) {
		/* Still executing scan; data has not been packed. (putNotify callbacks normally land here.) */
		if (sscanRecordDebug >= 5) printf("%s:process: continuing scan\n", psscan->name);
		if (precPvt->badOutputPv) {
			psscan->alrt = 1; POST(&psscan->alrt);
			sprintf(psscan->smsg, "Lost connection to Control PV");
			POST(&psscan->smsg);
			psscan->exsc = 0; POST(&psscan->exsc);
			psscan->xsc = 0; POST(&psscan->xsc);
			printf("%s:process: Lost connection to Control PV\n", psscan->name);
			endScan(psscan);
		} else if ((psscan->dstate == sscanDSTATE_SAVE_DATA_WAIT) ||
				(psscan->dstate == sscanDSTATE_ARRAY_READ_WAIT)) {
				/* endScan is waiting for something to complete */
			endScan(psscan);
		} else {
			contScan(psscan);
		}
	} else if (psscan->busy) {
		/* Scan is essentially finished (since xsc==0), but may still have some after-scan business */
		if (psscan->dstate < sscanDSTATE_PACKED) {
			packData(psscan, 2);
			if (psscan->dstate < sscanDSTATE_PACKED) {
				sprintf(psscan->smsg, "waiting for packData"); POST(&psscan->smsg);
				/*precPvt->calledBy = UNKNOWN;*/
				return(status);
			}
		}
		if (psscan->faze == sscanFAZE_RETRACE_WAIT) {
			afterScan(psscan);
		} else if ((psscan->faze == sscanFAZE_AFTER_SCAN_WAIT) ||
			   (psscan->faze == sscanFAZE_SCAN_DONE)) {
			psscan->faze = sscanFAZE_SCAN_DONE; POST(&psscan->faze);
		} else {
			printf("%s:process: How did I get here? (faze='%s', dstate='%s', calledBy = 0x%x)\n",
				psscan->name, sscanFAZE_strings[psscan->faze], sscanDSTATE_strings[psscan->dstate],
				precPvt->calledBy);
		}
	}
	checkMonitors(psscan);

	/* do forward link on last scan aquisition */
	if (psscan->busy && (psscan->faze == sscanFAZE_SCAN_DONE) && (psscan->dstate == sscanDSTATE_POSTED)) {
		psscan->busy = 0; POST(&psscan->busy);
		psscan->faze = sscanFAZE_IDLE; POST(&psscan->faze);
		recGblFwdLink(psscan);
		if (sscanRecordDebug) {
			epicsTimeGetCurrent(&timeCurrent);
			printf("%s:Scan Time = %.5f s\n\n", psscan->name, 
			       epicsTimeDiffInSeconds(&timeCurrent, &precPvt->timeStart));
		}
	}
	psscan->pxsc = psscan->xsc;
	recGblResetAlarms(psscan);

	psscan->pact = FALSE;
	/*precPvt->calledBy = UNKNOWN;*/
	if (psscan->rpro) printf("%s:process:atexit: rpro=%d\n", psscan->name, psscan->rpro);
	return (status);
}

static long 
special(struct dbAddr *paddr, int after)
{
	sscanRecord    *psscan = (sscanRecord *) (paddr->precord);
	recPvtStruct   *precPvt = (recPvtStruct *) psscan->rpvt;
	recDynLinkPvt  *puserPvt, *pPosOut_userPvt;
	posFields      *pPos;
	int             special_type = paddr->special;
	char           *ppvn;
	unsigned short *pPvStat;
	unsigned short  oldStat;
	short           i = -1, linkIndex = -1;
	unsigned char   prevAlrt;
    int             fieldIndex = dbGetFieldIndex(paddr);
	epicsTimeStamp	timeCurrent;
	int				clearThisPV;

	if (sscanRecordDebug) {
		printf("%s:special:entry for fieldIx %d, after=%d.\n", psscan->name, fieldIndex, after);
	}

	/* Make sure npts is reasonable.  Autosave might have  changed it after init_record. */
	if (psscan->npts > psscan->mpts) {psscan->npts = psscan->mpts; POST(&psscan->npts);}
	if (psscan->npts <= 0) {psscan->npts = 1; POST(&psscan->npts);}

	if (!after) {
		precPvt->pffo = psscan->ffo;	/* save previous ffo flag */
		/* Forbid certain changes while scan is in progress. */
		if (psscan->busy) {
			if ((fieldIndex == sscanRecordACQM) || (fieldIndex == sscanRecordACQT)) {
				return(-1);
			} else if ((fieldIndex >= sscanRecordP1PV) && (fieldIndex <= sscanRecordASPV)) {
				return(-1);
			} else if ((psscan->faze > sscanFAZE_BEFORE_SCAN_WAIT) &&
					   (psscan->faze < sscanFAZE_AFTER_SCAN_DO)) {
				/* We're in the scan loop; almost nothing is permitted now */
				switch (fieldIndex) {
				case (sscanRecordCMND):
					return (psscan->cmnd == CLEAR_MSG) ? 0 : -1;
				case (sscanRecordEXSC):
				case (sscanRecordPAUS):
				case (sscanRecordWAIT):
				case (sscanRecordAWCT):
				case (sscanRecordAWAIT):
				case (sscanRecordATIME):
					return(0);
				default:
					return(-1);
				}
			}
		}

		if ((fieldIndex >= sscanRecordP1PV) && (fieldIndex <= sscanRecordASPV)) {
			linkIndex = fieldIndex - sscanRecordP1PV;
			puserPvt = (recDynLinkPvt *) precPvt->caLinkStruct[linkIndex].puserPvt;
	 		if (puserPvt->connectInProgress) {
				printf("%s:special:connect already in progress for link %s.  Waiting...\n",
					psscan->name, linkNames[puserPvt->linkIndex]);
#if denyConnectCollision
				return(-1);
#else
				if (sscanRecordConnectWaitSeconds < 0) {
					printf("%s:special: sscanRecordConnectWaitSeconds can't be negative; setting it to zero.", psscan->name);
					sscanRecordConnectWaitSeconds = 0;
				}
				for (i=0; i<sscanRecordConnectWaitSeconds && puserPvt->connectInProgress; i++) epicsThreadSleep(1.);
				if (puserPvt->connectInProgress) {
					printf("%s:special:connect still in progress for link %s.  Trying new PV name.\n",
						psscan->name, linkNames[puserPvt->linkIndex]);
				}
#endif
			}
			/* positioners have a second link */
			if ((fieldIndex >= sscanRecordP1PV) && (fieldIndex <= sscanRecordP4PV)) {
				puserPvt = (recDynLinkPvt *) precPvt->caLinkStruct[linkIndex + NUM_PVS].puserPvt;
				if (puserPvt->connectInProgress) {
					printf("%s:special:connect already in progress for link %s.  Waiting...\n",
						psscan->name, linkNames[puserPvt->linkIndex]);
#if denyConnectCollision
					return(-1);
#else
					for (i=0; i<sscanRecordConnectWaitSeconds && puserPvt->connectInProgress; i++) epicsThreadSleep(1.);
					if (puserPvt->connectInProgress) {
						printf("%s:special:connect still in progress for link %s.  Trying new PV name.\n",
							psscan->name, linkNames[puserPvt->linkIndex]);
					}
#endif
				}
			}
		}
		return (0);
	}

	if (sscanRecordDebug > 10) {
		printf("%s:special(),special_type=%d, fieldIx=%d, exsc=%d, xsc=%d, faze='%s'\n",
			psscan->name, special_type, fieldIndex, psscan->exsc, psscan->xsc,
			sscanFAZE_strings[psscan->faze]);
	} else if (sscanRecordDebug > 5) {
		printf("%s:special(),special_type=%d, fieldIx=%d\n",
			psscan->name, special_type, fieldIndex);
	}

	switch (special_type) {
	case (SPC_MOD):
		switch (fieldIndex) {
		case sscanRecordEXSC:
			if (psscan->exsc) {
				if (psscan->paus) {
					sprintf(psscan->smsg, "Scan is paused"); POST(&psscan->smsg);
					if (!psscan->xsc) {psscan->exsc = 0; POST(&psscan->exsc);}
					return(-1);
				} else if (psscan->xsc) {
					/* redundant request to start scan */
					sprintf(psscan->smsg, "Already scanning"); POST(&psscan->smsg);
					return(-1);
				} else if (psscan->busy) {
					/* Not scanning, but not done either (saveData wait?) */
					if (psscan->dstate == sscanDSTATE_SAVE_DATA_WAIT) {
						sprintf(psscan->smsg, "Waiting for saveData");
					} else {
						sprintf(psscan->smsg, "Waiting for callback");
					}
					db_post_events(psscan, &psscan->smsg, DBE_VAL_LOG);
					return(-1);
				} else {
					/* New scan.  Renew old positioner links so we get current limits data */
					if ((psscan->faze != sscanFAZE_IDLE) && (psscan->faze != sscanFAZE_PREVIEW)) {
						printf("Starting new scan with unexpected faze ('%s').\n",
							sscanFAZE_strings[psscan->faze]);
					}
					/*
					 * Note 'i' numbering below assumes positioners are elements 0..3 (monitors),
					 * and NUM_PVS..NUM_PVS+3 (outputs) in the caLinkStruct array.
					 */
					epicsTimeGetCurrent(&timeCurrent);
					if (epicsTimeDiffInSeconds(&timeCurrent, &precPvt->lastScanEndTime) >= sscanRecordLookupTime) {
						for (i=0; i<NUM_POS; i++) {
							puserPvt = (recDynLinkPvt *) precPvt->caLinkStruct[i].puserPvt;
							pPosOut_userPvt = (recDynLinkPvt *) precPvt->caLinkStruct[i + NUM_PVS].puserPvt;
							if ((epicsTimeDiffInSeconds(&timeCurrent, &puserPvt->lookupTime) >= sscanRecordLookupTime) &&
								 (puserPvt->connectInProgress == 0) && (pPosOut_userPvt->connectInProgress == 0)) {
								ppvn = &psscan->p1pv[0] + (i * PVN_SIZE);
								if (ppvn[0] != '\0') {
									if (sscanRecordDebug > 5)
										printf("%s:special: renewing link %d\n", psscan->name, i);
									/* force flags to indicate PV_NC until callback happens */
									pPvStat = &psscan->p1nv + i;	/* pointer arithmetic */
									*pPvStat = PV_NC;
									precPvt->badOutputPv = 1;
									if (precPvt->caLinkStruct[i].pdynLinkPvt) {
										recDynLinkClear(&precPvt->caLinkStruct[i]);
										/* Positioners have two recDynLinks */
										if (precPvt->caLinkStruct[i + NUM_PVS].pdynLinkPvt) {
											recDynLinkClear(&precPvt->caLinkStruct[i + NUM_PVS]);
										}
									}
									/* remember when we did this lookup */
									epicsTimeGetCurrent(&puserPvt->lookupTime);
									lookupPV(psscan, i);
								}
							}
						}
					}
					psscan->xsc = 1; POST(&psscan->xsc);
					checkConnections(psscan);
					if (precPvt->badOutputPv || precPvt->badInputPv) {
						if (sscanRecordDebug >= 2)
							printf("%s:special:scan pending PV connection.\n", psscan->name);
						psscan->alrt = 1; POST(&psscan->alrt);
						strcpy(psscan->smsg, "Waiting for PV's to connect"); POST(&psscan->smsg);
						psscan->faze = sscanFAZE_SCAN_PENDING; POST(&psscan->faze);
					} else {
						psscan->alrt = 0; POST(&psscan->alrt);
					}
					precPvt->calledBy = SPECIAL_EXSC;
					return(0);
				}
			} else {
				/* EXSC == 0 */
				if (psscan->xsc) {
					psscan->xsc = 0; POST(&psscan->xsc);
					sprintf(psscan->smsg, "Aborting scan");
					db_post_events(psscan, &psscan->smsg, DBE_VAL_LOG);
					return(0);
				} else if (psscan->faze != sscanFAZE_IDLE) {
					/* The first abort didn't succeed, or is taking too long */
					psscan->kill++;
					printf("%s:special(): Killing scan (kill=%1d/3).\n", psscan->name, psscan->kill);
					sprintf(psscan->smsg, "Killing scan (kill=%1d/3)", psscan->kill);
					db_post_events(psscan, &psscan->smsg, DBE_VAL_LOG);
					/* Cancel any outstanding active timer */
					if (precPvt->dlyCallback.timer) epicsTimerCancel(precPvt->dlyCallback.timer);
					return(0);
				} else {
					/* request to abort scan that is not active */
					sprintf(psscan->smsg, "Scan record is idle");
					db_post_events(psscan, &psscan->smsg, DBE_VAL_LOG);
					return(-1);
				}
			}
			break;
		case sscanRecordDDLY:
			if (psscan->ddly < 0.0) psscan->ddly = 0.0;
			psscan->ddly = NINT(psscan->ddly * ticsPerSecond) / ticsPerSecond;
			POST(&psscan->ddly);
			break;
		case sscanRecordPDLY:
			if (psscan->pdly < 0.0) psscan->pdly = 0.0;
			psscan->pdly = NINT(psscan->pdly * ticsPerSecond) / ticsPerSecond;
			POST(&psscan->pdly);
			break;
		case sscanRecordRDLY:
			if (psscan->rdly < 0.0) psscan->rdly = 0.0;
			psscan->rdly = NINT(psscan->rdly * ticsPerSecond) / ticsPerSecond;
			POST(&psscan->pdly);
			break;
		case sscanRecordPAUS:

			if (sscanRecordDebug) {
				printf("%s:special:paus: faze='%s', nPTR_CBs=%1d%1d%1d, xsc=%d, pxsc=%d, calledBy 0x%x\n",
					psscan->name, sscanFAZE_strings[psscan->faze], precPvt->numPositionerCallbacks,
					precPvt->numTriggerCallbacks, precPvt->numAReadCallbacks, psscan->xsc, psscan->pxsc,
					precPvt->calledBy);
			}
			if (psscan->paus != psscan->lpau) {
				if (psscan->paus == 0) {
					int numPosCb, numTrigCb, numAReadCb, numGetCb;

					epicsMutexLock(precPvt->numCallbacksSem);
					numPosCb = precPvt->numPositionerCallbacks;
					numTrigCb = precPvt->numTriggerCallbacks;
					numAReadCb = precPvt->numAReadCallbacks;
					numGetCb = precPvt->numGetCallbacks;
					epicsMutexUnlock(precPvt->numCallbacksSem);


					sprintf(psscan->smsg, "Scan pause rescinded");
					POST(&psscan->smsg);
					if ((numTrigCb == 0) && (numPosCb == 0) && (numAReadCb == 0) && (numGetCb == 0)) {
						/* The P, T, R, or G callback that would have sent us to the next scan
						 * phase came in while we were paused, so we must get the record processed.
						 */
						if (psscan->wtng || psscan->await) {
							sprintf(psscan->smsg, "Waiting for client");
							POST(&psscan->smsg);
						} else {
							precPvt->calledBy = SPECIAL_PAUS;
							if (psscan->rdly < .001) {
								scanOnce((struct dbCommon *)psscan);
							} else {
								callbackRequestDelayed(&precPvt->dlyCallback, psscan->rdly);
							}
						}
					}
				} else {
					/* Cancel any outstanding delayed unpause */
					if (precPvt->dlyCallback.timer) epicsTimerCancel(precPvt->dlyCallback.timer);
					sprintf(psscan->smsg, "Scan pause asserted");
					POST(&psscan->smsg);
				}
			}
			psscan->lpau = psscan->paus;
			break;
		case sscanRecordCMND:
			if (psscan->cmnd == CLEAR_MSG) {
				psscan->alrt = 0; POST(&psscan->alrt);
				strcpy(psscan->smsg, "");
				POST(&psscan->smsg);
			}
			if (psscan->xsc || psscan->busy) {
				psscan->cmnd = 0;
				break;
			}
			switch (psscan->cmnd) {
			case CHECK_LIMITS:
				prevAlrt = psscan->alrt;
				psscan->alrt = 0;
				checkScanLimits(psscan);
				POST(&psscan->smsg);
				if (psscan->alrt != prevAlrt) POST(&psscan->alrt);
				break;
			case PREVIEW_SCAN:
				/* get_array_info() needs to know that we're just previewing */
				psscan->faze = sscanFAZE_PREVIEW; POST(&psscan->faze);
				previewScan(psscan);
				break;
			case CLEAR_RECORD:
			case CLEAR_POSITIONERS:
			case CLEAR_POSandRDBK:
				/* clear PV's, frzFlags, modes, etc */
				psscan->scan = 0; POST(&psscan->scan);
				resetFrzFlags(psscan);
				psscan->p1sm = 0; POST(&psscan->p1sm);
				psscan->p1ar = 0; POST(&psscan->p1ar);
				psscan->p2sm = 0; POST(&psscan->p2sm);
				psscan->p2ar = 0; POST(&psscan->p2ar);
				psscan->p3sm = 0; POST(&psscan->p3sm);
				psscan->p3ar = 0; POST(&psscan->p3ar);
				psscan->p4sm = 0; POST(&psscan->p4sm);
				psscan->p4ar = 0; POST(&psscan->p4ar);
				psscan->pasm = 0; POST(&psscan->pasm);
				psscan->ffo = 0; POST(&psscan->ffo);
				/* fall through */
			case CLEAR_POSITIONER_PVS:
			case CLEAR_POSandRDBK_PVS:
				for (i = 0; i < NUM_PVS; i++) {
					puserPvt = (recDynLinkPvt *) precPvt->caLinkStruct[i].puserPvt;
					clearThisPV = (psscan->cmnd == CLEAR_RECORD);
					clearThisPV |= ((psscan->cmnd == CLEAR_POSITIONERS)    && (puserPvt->linkType == POSITIONER));
					clearThisPV |= ((psscan->cmnd == CLEAR_POSandRDBK)     && (puserPvt->linkType == POSITIONER));
					clearThisPV |= ((psscan->cmnd == CLEAR_POSandRDBK)     && (puserPvt->linkType == READBACK));
					clearThisPV |= ((psscan->cmnd == CLEAR_POSITIONER_PVS) && (puserPvt->linkType == POSITIONER));
					clearThisPV |= ((psscan->cmnd == CLEAR_POSandRDBK_PVS) && (puserPvt->linkType == POSITIONER));
					clearThisPV |= ((psscan->cmnd == CLEAR_POSandRDBK_PVS) && (puserPvt->linkType == READBACK));
					if (clearThisPV) {
						/* clear this PV */
						epicsMutexLock(precPvt->pvStatSem);
						pPvStat = &psscan->p1nv + i;	/* pointer arithmetic */
						oldStat = *pPvStat;
						ppvn = &psscan->p1pv[0] + (i * PVN_SIZE);
						ppvn[0] = '\0'; POST(ppvn);
						if (*pPvStat != NO_PV) {
							/* PV is now NULL but didn't used to be */
							*pPvStat = NO_PV;
							if (*pPvStat != oldStat) POST(pPvStat);
							if (precPvt->caLinkStruct[i].pdynLinkPvt /*puserPvt->dbAddrNv*/) {
								recDynLinkClear(&precPvt->caLinkStruct[i]);
								/* Positioners have two recDynLinks */
								if ((i < NUM_POS) && (precPvt->caLinkStruct[i + NUM_PVS].pdynLinkPvt)) {
									recDynLinkClear(&precPvt->caLinkStruct[i + NUM_PVS]);
								}
							} else
								puserPvt->dbAddrNv = -1;
						} else {
							/* PV is NULL, but previously MAY have been "time" */
							if (puserPvt->ts) {
								puserPvt->ts = 0;
								POST(pPvStat); /* Give client a poke */
							}
						}
						epicsMutexUnlock(precPvt->pvStatSem);
					}
				}
				break;
			}
			break;
		case sscanRecordPRIO:
			callbackSetPriority(psscan->prio, &precPvt->doPutsCallback);
			break;
		case sscanRecordAWCT:
			if (psscan->awct < 0) {
				psscan->awct = 0; POST(&psscan->awct);
			}
			break;
		case sscanRecordWAIT:
			if (psscan->wait) {
				psscan->wcnt++; POST(&psscan->wcnt);
			} else {
				if (psscan->wcnt > 0) {
					psscan->wcnt--; POST(&psscan->wcnt);
				}
				if (psscan->wtng && (psscan->wcnt == 0)) {
					psscan->wtng = 0; POST(&psscan->wtng);
					if (psscan->paus) {
						sprintf(psscan->smsg, "Scan is paused ...");
						POST(&psscan->smsg);
					} else {
						sprintf(psscan->smsg, "Scanning ...");
						POST(&psscan->smsg);
						precPvt->calledBy = SPECIAL_WAIT;
						(void) scanOnce((struct dbCommon *)psscan);
					}
				}
			}
			break;
		case sscanRecordAWAIT:
			if (psscan->await) precPvt->userSetAWAIT = 1;
			if (sscanRecordDebug)
				printf("%s:special await=%d, dstate='%s'\n",
					psscan->name, psscan->await, sscanDSTATE_strings[psscan->dstate]);
			if ((psscan->dstate == sscanDSTATE_SAVE_DATA_WAIT) && (psscan->await == 0)) {
				precPvt->calledBy = SPECIAL_AWAIT;
				scanOnce((struct dbCommon *)psscan);
			}
			break;

		case sscanRecordACQM:
			precPvt->prevACQM = sscanACQM_NORMAL;
			break;
		case sscanRecordREFD:
			if (psscan->refd < 1) {
				psscan->refd = 1; POST(&psscan->refd);
			} else if (psscan->refd > NUM_DET) {
				psscan->refd = NUM_DET; POST(&psscan->refd);
			}
			break;

		default:
			/* Check for a dynamic link PV change */
			if ((fieldIndex >= sscanRecordP1PV) &&
						(fieldIndex <= sscanRecordASPV)) {
				epicsMutexLock(precPvt->pvStatSem);
				i = fieldIndex - sscanRecordP1PV;
				puserPvt = (recDynLinkPvt *) precPvt->caLinkStruct[i].puserPvt;
				pPvStat = &psscan->p1nv + i;	/* pointer arithmetic */
				oldStat = *pPvStat;
				ppvn = &psscan->p1pv[0] + (i * PVN_SIZE);
				/* If we got a blank (but not empty) PV, make it empty */
				if (isBlank(ppvn)) {
					ppvn[0] = '\0';
					POST(ppvn);
				} 
				if (ppvn[0] != '\0') {
					if (sscanRecordDebug > 5)
						printf("%s:Search during special \n", psscan->name);
					*pPvStat = PV_NC;
					/* force flags to indicate PV_NC until callback happens */
					if ((i < D1_IN) || ((i >= T1_OUT) && (i <= AS_OUT))) {
						precPvt->badOutputPv = 1;
					} else {
						precPvt->badInputPv = 1;
					}
					/*
					 * need to post_event before lookupPV calls recDynLinkAddXxx
					 * because SearchCallback could happen immediately
					 */
					if (*pPvStat != oldStat) POST(pPvStat);
					/* save time of this lookup so we don't end up doing another unnecessarily */
					epicsTimeGetCurrent(&puserPvt->lookupTime);
					lookupPV(psscan, i);
				} else if (*pPvStat != NO_PV) {
					/* PV is now NULL but didn't used to be */
					*pPvStat = NO_PV;
					if (*pPvStat != oldStat) POST(pPvStat);
					if (precPvt->caLinkStruct[i].pdynLinkPvt /*puserPvt->dbAddrNv*/) {
						recDynLinkClear(&precPvt->caLinkStruct[i]);
						/* Positioners have two recDynLinks */
						if ((i < NUM_POS) && (precPvt->caLinkStruct[i + NUM_PVS].pdynLinkPvt)) {
							recDynLinkClear(&precPvt->caLinkStruct[i + NUM_PVS]);
						}
					} else
						puserPvt->dbAddrNv = -1;
				} else {
					/* PV is NULL, but previously MAY have been "time" */
					if (puserPvt->ts) {
						puserPvt->ts = 0;
						/* Client may need a poke to realize PV has gone from apparently invalid state
						 * ("time" is flagged as "No PV", but it does generate readback data) to a
						 * really invalid state (no PV name at all, no generated data).
						 */
						POST(pPvStat);
					}
				}
				epicsMutexUnlock(precPvt->pvStatSem);
			}
			break;

		}

		break;

	case (SPC_SC_MO):	/* Step Mode changed for a positioner */

		pPos = (posFields *) & psscan->p1pp;
		for (i = 0; i < NUM_POS; i++, pPos++) {
			if (paddr->pfield == (void *) &pPos->p_sm) {
				/* Entering Table mode ? */
				if ((pPos->p_sm == sscanP1SM_Table) &&
				  (precPvt->prevSm[i] != sscanP1SM_Table)) {
					savePosParms(psscan, (unsigned short) i);
					zeroPosParms(psscan, (unsigned short) i);
					precPvt->prevSm[i] = pPos->p_sm;
					if (precPvt->tablePts[i] < psscan->npts) {
						sprintf(psscan->smsg, "Pts in P%d Table < # of steps.", i + 1);
						POST(&psscan->smsg);
						if (!psscan->alrt) {
							psscan->alrt = 1; POST(&psscan->alrt);
						}
					}
				}
				/* Leaving Table mode ? */
				else if ((pPos->p_sm != sscanP1SM_Table) &&
				  (precPvt->prevSm[i] == sscanP1SM_Table)) {
					restorePosParms(psscan, (unsigned short) i);
					prevAlrt = psscan->alrt;
					precPvt->nptsCause = -1;
					changedNpts(psscan);
					POST(&psscan->smsg);
					if (psscan->alrt != prevAlrt) POST(&psscan->alrt);
					precPvt->prevSm[i] = pPos->p_sm;
				}
			}
		}
		break;

	case (SPC_SC_S):	/* linear scan parameter change */
	case (SPC_SC_I):
	case (SPC_SC_E):
	case (SPC_SC_C):
	case (SPC_SC_W):
		/* resolve linear scan parameters affected by this fields change */
		prevAlrt = psscan->alrt;
		psscan->alrt = 0;
		strcpy(psscan->smsg, "");
		adjLinParms(paddr);
		POST(&psscan->smsg);
		if (psscan->alrt != prevAlrt) POST(&psscan->alrt);
		if (sscanRecordViewPos) previewScan(psscan);
		break;

	case (SPC_SC_N):
		/* adjust all linear scan parameters per new # of steps */
		if (psscan->npts > psscan->mpts) {
			psscan->npts = psscan->mpts; POST(&psscan->npts);
		} else if (psscan->npts < 1) {
			psscan->npts = 1; POST(&psscan->npts);
		}
		prevAlrt = psscan->alrt;
		psscan->alrt = 0;
		strcpy(psscan->smsg, "");
		precPvt->nptsCause = -1;	/* resolve all positioner parameters */
		changedNpts(psscan);
		POST(&psscan->smsg);
		if (psscan->alrt != prevAlrt) POST(&psscan->alrt);
		if (sscanRecordViewPos) previewScan(psscan);
		break;

	case (SPC_SC_FFO):
		/* Freeze Flag Override field */
		if ((psscan->ffo) && (!precPvt->pffo)) {
			saveFrzFlags(psscan);
			resetFrzFlags(psscan);
		} else if (!psscan->ffo && precPvt->pffo)	/* only on 1->0 */
			restoreFrzFlags(psscan);
		break;

	case (SPC_SC_F):
		/* Freeze Flag Override field */
		if (psscan->ffo) resetFrzFlags(psscan);
		break;


	default:	
		printf("%s:special(), no handler for special_type %d\n", psscan->name, special_type);
		break;

	}
	return (0);
}

static long 
cvt_dbaddr(struct dbAddr *paddr)
{
	sscanRecord	*psscan = (sscanRecord *) paddr->precord;
	posFields	*pPos = (posFields *) & psscan->p1pp;
	detFields	*pDet = (detFields *) & psscan->d01hr;
    int			i, fieldIndex = dbGetFieldIndex(paddr);
	unsigned short	numFieldsInGroup;

	if (sscanRecordDebug > 0)
		printf("sscanRecord:cvt_dbaddr: fieldIndex=%d\n", fieldIndex);
	numFieldsInGroup = sscanRecordD02DA - sscanRecordD01DA;
	i = (fieldIndex - sscanRecordD01HR) / numFieldsInGroup;
	if ((i >= 0) && (i < NUM_DET)) {
		pDet += i;
		paddr->pfield = pDet->d_da; /* doesn't matter what goes here, as long as it has the right type */
		paddr->no_elements = psscan->mpts;
		paddr->field_type = DBF_FLOAT;
		paddr->field_size = sizeof(float);
		paddr->dbr_field_type = DBF_FLOAT;
		if (sscanRecordDebug > 0)
			printf("sscanRecord:cvt_dbaddr: pfield set to %p\n", paddr->pfield);
		return (0);
	}

	numFieldsInGroup = sscanRecordP2PP - sscanRecordP1PP;
	i = (fieldIndex - sscanRecordP1PP) / numFieldsInGroup;
	if ((i >= 0) && (i < NUM_POS)) {
		pPos += i;
		i = (fieldIndex - sscanRecordP1PP) % numFieldsInGroup;
		if (i == (sscanRecordP1PA - sscanRecordP1PP)) {
			paddr->pfield = (void *) (pPos->p_pa);
		} else if (i == (sscanRecordP1RA - sscanRecordP1PP)) {
			paddr->pfield = (void *) (pPos->p_ra);
		} else if (i == (sscanRecordP1CA - sscanRecordP1PP)) {
			paddr->pfield = (void *) (pPos->p_ca);
		} else {
			return(-1);	/* dbd problem: no cvt_dbaddr support for field */
		}
		paddr->no_elements = psscan->mpts;
		paddr->field_type = DBF_DOUBLE;
		paddr->field_size = sizeof(double);
		paddr->dbr_field_type = DBF_DOUBLE;
		if (sscanRecordDebug > 0)
			printf("sscanRecord:cvt_dbaddr: pfield set to %p\n", paddr->pfield);
		return (0);
	}

	return(-1);
}

static long 
get_array_info(struct dbAddr *paddr, long *no_elements, long *offset)
{
	sscanRecord		*psscan = (sscanRecord *) paddr->precord;
	recPvtStruct	*precPvt = (recPvtStruct *) psscan->rpvt;
	unsigned short	numFieldsInGroup, group;
    int				fieldIndex = dbGetFieldIndex(paddr);
	int				groupField;


	if (sscanRecordDebug > 0)
		printf("sscanRecord:get_array_info: fieldIndex=%d, pfield=%p\n", fieldIndex,
			paddr->pfield);
	/*
	 * This routine is called because someone wants an array. Determine
	 * which array they want by comparing fieldIndex to the indices of
	 * the array fields we host.  Note that these array fields are not
	 * contiguous, but instead are in groups of fields specific to a
	 * particular detector or positioner.
	 */

	*offset = 0;
	*no_elements = 0;

	/* Is field a detector array? */
	numFieldsInGroup = sscanRecordD02DA - sscanRecordD01DA;
	if ((fieldIndex >= sscanRecordD01DA) &&
	    (fieldIndex < sscanRecordD01DA + NUM_DET*numFieldsInGroup)) {
		group = (fieldIndex - sscanRecordD01DA)/numFieldsInGroup;
		groupField = fieldIndex - (sscanRecordD01DA + group*numFieldsInGroup);
		if (sscanRecordDebug > 0)
			printf("sscanRecord:get_array_info: groupField=%d\n", groupField);

		if ((precPvt->acqDet[group]) ||
		    	((group < NUM_POS) && (psscan->faze == sscanFAZE_PREVIEW))) {
			if ((groupField==0) || (psscan->dstate >= sscanDSTATE_PACKED)) {
				/* e.g., D01DA, or any after buffers have been switched*/
				if (precPvt->validBuf == B_BUFFER) {
					paddr->pfield = precPvt->detBufPtr[group].pBufB;
				} else {
					paddr->pfield = precPvt->detBufPtr[group].pBufA;
				}
			} else {
				/* e.g., D01CA during scan */
				if (precPvt->validBuf == A_BUFFER) {
					paddr->pfield = precPvt->detBufPtr[group].pBufB;
				} else {
					paddr->pfield = precPvt->detBufPtr[group].pBufA;
				}
			}
		} else {
			/* Caller is connected to a field whose buffer has not been allocated */
			paddr->pfield = precPvt->nullArray;
		}
		*no_elements = psscan->mpts;
		/* *no_elements = psscan->cpt; */ /* but we fill arrays for medm */
		return (0);
	}


	/* Is field a positioner-readback array? */

	numFieldsInGroup = sscanRecordP2RA - sscanRecordP1RA;
/*
 *	if ((fieldIndex >= sscanRecordP1RA) &&
 *	    (fieldIndex < sscanRecordP1RA + NUM_POS*numFieldsInGroup)) {
 */
	if ((fieldIndex != sscanRecordP1PA) && (fieldIndex != sscanRecordP2PA) &&
		(fieldIndex != sscanRecordP3PA) && (fieldIndex != sscanRecordP4PA)) {

		group = (fieldIndex - sscanRecordP1RA)/numFieldsInGroup;
		groupField = fieldIndex - (sscanRecordP1RA + group*numFieldsInGroup);
		if (sscanRecordDebug > 0)
			printf("sscanRecord:get_array_info: group=%d; groupField=%d\n", group, groupField);

		if ((groupField==0) || (psscan->dstate >= sscanDSTATE_PACKED)) {
			/* e.g., P1RA, or any after buffers have been switched*/
			if (precPvt->validBuf == B_BUFFER)
				paddr->pfield = precPvt->posBufPtr[group].pBufB;
			else
				paddr->pfield = precPvt->posBufPtr[group].pBufA;
		} else {
			/* e.g., P1CA, during scan*/
			if (precPvt->validBuf == A_BUFFER)
				paddr->pfield = precPvt->posBufPtr[group].pBufB;
			else
				paddr->pfield = precPvt->posBufPtr[group].pBufA;
		}
		*no_elements = psscan->mpts;
		/* *no_elements = psscan->cpt; */ /* but we fill arrays for medm */
		return (0);
	}

	/* If field is not a detector array or a positioner-readback array, then
	 * it must be a positioner array.  We don't have to set paddr->pfield,
	 * because these arrays are not double-buffered. 
	 */
	if ((fieldIndex >= sscanRecordP1PA) && (fieldIndex <= sscanRecordP4RA)) {
		*no_elements = psscan->mpts;
	}
	return (0);
}


static long 
put_array_info(struct dbAddr *paddr, long nNew)
{
	sscanRecord *psscan = (sscanRecord *) paddr->precord;
	recPvtStruct   *precPvt = (recPvtStruct *) psscan->rpvt;
	unsigned short	numFieldsInGroup, group;
    int				fieldIndex = dbGetFieldIndex(paddr);

	if (sscanRecordDebug > 0)
		printf("sscanRecord:put_array_info: fieldIndex=%d, pfield=%p\n", fieldIndex,
			paddr->pfield);
	/*
	 * This routine is called because someone wrote a table to the
	 * "positioner" array p_pa. Determine which positioner and store
	 * nNew for future use. Also check against current npts
	 */

	/* Make sure npts is reasonable.  Autosave might have  changed it after init_record. */
	if (psscan->npts > psscan->mpts) {psscan->npts = psscan->mpts; POST(&psscan->npts);}
	if (psscan->npts <= 0) {psscan->npts = 1; POST(&psscan->npts);}


	/* Is field a positioner array? */
	numFieldsInGroup = sscanRecordP2PA - sscanRecordP1PA;
	if ((fieldIndex >= sscanRecordP1PA) &&
	    (fieldIndex < sscanRecordP1PA + NUM_POS*numFieldsInGroup)) {
		group = (fieldIndex - sscanRecordP1PA)/numFieldsInGroup;

		precPvt->tablePts[group] = nNew;
		if (nNew < psscan->npts) {
			sprintf(psscan->smsg, "Pts in P%d Table < # of Steps.", group + 1);
			POST(&psscan->smsg);
			if (!psscan->alrt) {
				psscan->alrt = 1; POST(&psscan->alrt);
			}
		} else {
			strcpy(psscan->smsg, "");
			POST(&psscan->smsg);
			if (psscan->alrt) {
				psscan->alrt = 0; POST(&psscan->alrt);
			}
		}
		return (0);
	}
	return (0);
}



static long 
get_enum_str(struct dbAddr *paddr, char *pstring)
{
	sscanRecord *psscan = (sscanRecord *) paddr->precord;

	if (paddr->pfield == (void *) &psscan->cmnd) {
		sprintf(pstring, "%d", psscan->cmnd);
	} else {
		strcpy(pstring, "No string");
	}
	return (0);
}

static long 
get_enum_strs(struct dbAddr *paddr, struct dbr_enumStrs *pes)
{
	sscanRecord *psscan = (sscanRecord *) paddr->precord;

	if (paddr->pfield == (void *) &psscan->cmnd) {
		memset(pes->strs, '\0', sizeof(pes->strs));
		strncpy(pes->strs[0], "0-Clear msg", sizeof("0-Clear msg"));
		strncpy(pes->strs[1], "1-Check limits", sizeof("1-Check limits"));
		strncpy(pes->strs[2], "2-Preview scan", sizeof("2-Preview scan"));
		strncpy(pes->strs[3], "3-Clear all PV's", sizeof("3-Clear all PV's"));
		strncpy(pes->strs[4], "4-Clear positioner PV's, etc", sizeof("4-Clear positioner PV's, etc"));
		strncpy(pes->strs[5], "5-Clear positioner PV's", sizeof("5-Clear positioner PV's"));
		strncpy(pes->strs[6], "6-Clear pos&rdbk PV's, etc", sizeof("6-Clear pos&rdbk PV's, etc"));
		strncpy(pes->strs[7], "7-Clear pos&rdbk PV's", sizeof("7-Clear pos&rdbk PV's"));
		pes->no_str = 8;
	} else {
		strcpy(pes->strs[0], "No string");
		pes->no_str = 1;
	}

	return (0);
}

static long 
put_enum_str(struct dbAddr *paddr, char *pstring)
{
	sscanRecord *psscan = (sscanRecord *) paddr->precord;

	if (paddr->pfield == (void *) &psscan->cmnd) {
		if (sscanf(pstring, "%hu", &psscan->cmnd) <= 0)
			return (S_db_badChoice);
	} else {
		return (S_db_badChoice);
	}

	return (0);
}

static long 
get_units(struct dbAddr *paddr, char *units)
{
	sscanRecord *psscan = (sscanRecord *) paddr->precord;
	posFields   *pPos = (posFields *) & psscan->p1pp;
	detFields   *pDet = (detFields *) & psscan->d01hr;
    int          i, fieldIndex = dbGetFieldIndex(paddr);

	if (fieldIndex >= sscanRecordP1PP) {
		i = (fieldIndex - sscanRecordP1PP) / (sscanRecordP2PP - sscanRecordP1PP);
		if (i>=0 && i<NUM_POS) {
			strncpy(units, pPos[i].p_eu, 7);
			units[7] = 0;
			return(0);
		}
	} else if (fieldIndex >= sscanRecordD01HR) {
		i = (fieldIndex - sscanRecordD01HR) / (sscanRecordD02HR - sscanRecordD01HR);
		if (i>=0 && i<NUM_DET) {
			strncpy(units, pDet[i].d_eu, 7);
			units[7] = 0;
			return(0);
		}
	}
	return (0);
}

static long 
get_precision(struct dbAddr *paddr, long *precision)
{
	sscanRecord *psscan = (sscanRecord *) paddr->precord;
	posFields   *pPos = (posFields *) & psscan->p1pp;
	detFields   *pDet = (detFields *) & psscan->d01hr;
    int          i, fieldIndex = dbGetFieldIndex(paddr);

	if (fieldIndex >= sscanRecordP1PP) {
		i = (fieldIndex - sscanRecordP1PP) / (sscanRecordP2PP - sscanRecordP1PP);
		if (i>=0 && i<NUM_POS) {
			*precision = MIN(10, MAX(0, pPos[i].p_pr));
			return(0);
		}
	} else if (fieldIndex >= sscanRecordD01HR) {
		i = (fieldIndex - sscanRecordD01HR) / (sscanRecordD02HR - sscanRecordD01HR);
		if (i>=0 && i<NUM_DET) {
			*precision = MIN(10, MAX(0, pDet[i].d_pr));
			return(0);
		}
	} else if (fieldIndex == sscanRecordATIME) {
		*precision = 1;
		return(0);
	}
	*precision = 3;
	return (0);
}

static long 
get_graphic_double(struct dbAddr *paddr, struct dbr_grDouble *pgd)
{
	sscanRecord *psscan = (sscanRecord *) paddr->precord;
	posFields   *pPos = (posFields *) & psscan->p1pp;
	detFields   *pDet = (detFields *) & psscan->d01hr;
    int          i, fieldIndex = dbGetFieldIndex(paddr);

	if (fieldIndex >= sscanRecordP1PP) {
		i = (fieldIndex - sscanRecordP1PP) / (sscanRecordP2PP - sscanRecordP1PP);
		if (i>=0 && i<NUM_POS) {
			pgd->upper_disp_limit = pPos[i].p_hr;
			pgd->lower_disp_limit = pPos[i].p_lr;
			return(0);
		}
	} else if (fieldIndex >= sscanRecordD01HR) {
		i = (fieldIndex - sscanRecordD01HR) / (sscanRecordD02HR - sscanRecordD01HR);
		if (i>=0 && i<NUM_DET) {
			pgd->upper_disp_limit = pDet[i].d_hr;
			pgd->lower_disp_limit = pDet[i].d_lr;
			return(0);
		}
	}

	recGblGetGraphicDouble(paddr, pgd);
	return (0);
}

static long 
get_control_double(struct dbAddr *paddr, struct dbr_ctrlDouble *pcd)
{
	/* for testing .... */
	/* return upper_disp_limit and lower_disp_limit as control limits */
	struct dbr_grDouble grDblStruct;
	long            status;

	status = get_graphic_double(paddr, &grDblStruct);
	pcd->upper_ctrl_limit = grDblStruct.upper_disp_limit;
	pcd->lower_ctrl_limit = grDblStruct.lower_disp_limit;

	return (0);
}

static long 
get_alarm_double(struct dbAddr *paddr, struct dbr_alDouble *pad)
{
	recGblGetAlarmDouble(paddr, pad);
	return (0);
}


static void 
checkMonitors(sscanRecord *psscan)
{
	recPvtStruct   *precPvt = (recPvtStruct *) psscan->rpvt;
	detFields      *pDet = (detFields *) & psscan->d01hr;
	posFields      *pPos = (posFields *) & psscan->p1pp;
	epicsTimeStamp  timeCurrent;
	int             i, end_of_scan;

	if (psscan->dstate == sscanDSTATE_POSTED) return;
	/*
	 * If last posting time is > MIN_MON, check to see if any dynamic
	 * fields have changed (also post monitors on end of sscan)
	 */

	/* post scalars */
	epicsTimeGetCurrent(&timeCurrent);
	if ((epicsTimeDiffInSeconds(&timeCurrent, &psscan->tolp) > MIN_MON) ||
	    ((psscan->pxsc == 1) && (psscan->xsc == 0))) {
		psscan->tolp = timeCurrent;

		/* check all detectors */
		for (i = 0; i < NUM_DET; i++, pDet++) {
			if (fabs(pDet->d_lv - pDet->d_cv) > 0) {
				POST(&pDet->d_cv);
				pDet->d_lv = pDet->d_cv;
			}
		}

		/* check all positioners and readbacks  */
		for (i = 0; i < NUM_POS; i++, pPos++) {
			if (fabs(pPos->p_lv - pPos->p_dv) > 0) {
				POST(&pPos->p_dv);
				pPos->p_lv = pPos->p_dv;
			}
			if (fabs(pPos->r_lv - pPos->r_cv) > 0) {
				POST(&pPos->r_cv);
				pPos->r_lv = pPos->r_cv;
			}
		}

		if (psscan->pcpt != psscan->cpt) {
			POST(&psscan->cpt);
			psscan->pcpt = psscan->cpt;
			if (psscan->cpt) POST(&psscan->val);
		}
	}

	end_of_scan = (psscan->dstate == sscanDSTATE_PACKED);

	/* post arrays during scan */
	if (!end_of_scan && (psscan->atime >= 0.1) &&
		((epicsTimeDiffInSeconds(&timeCurrent, &psscan->tlap) > psscan->atime) ||
	    	((psscan->pxsc == 1) && (psscan->xsc == 0)))) {
		psscan->tlap = timeCurrent;
		for (i = 0; i < NUM_POS; i++) {
			db_post_events(psscan, precPvt->posBufPtr[i].pBufA, DBE_VALUE);
			db_post_events(psscan, precPvt->posBufPtr[i].pBufB, DBE_VALUE);
		}
		for (i = 0; i < NUM_DET; i++) {
			if (precPvt->acqDet[i]) {
				db_post_events(psscan, precPvt->detBufPtr[i].pBufA, DBE_VALUE);
				db_post_events(psscan, precPvt->detBufPtr[i].pBufB, DBE_VALUE);
			}
		}
		db_post_events(psscan, precPvt->nullArray, DBE_VALUE);
	}

	/*
	 * If this is the end of a scan, post data arrays with DBE_LOG, for
	 * archiver, and for clients that want arrays only at end of scan.
	 */
	if (end_of_scan) {
		psscan->dstate = sscanDSTATE_POSTED; POST(&psscan->dstate);

		/* Must post events on both pointers, since toggle.  Note that this is
		 * merely the notification step of the posting, since these PV's are
		 * arrays.  The actual accociation between PV and data will
		 * occur in and get_array_info().
		 */
		for (i = 0; i < NUM_POS; i++) {
			db_post_events(psscan, precPvt->posBufPtr[i].pBufA, DBE_VAL_LOG);
			db_post_events(psscan, precPvt->posBufPtr[i].pBufB, DBE_VAL_LOG);
		}
		for (i = 0; i < NUM_DET; i++) {
			if (precPvt->acqDet[i]) {
				db_post_events(psscan, precPvt->detBufPtr[i].pBufA, DBE_VAL_LOG);
				db_post_events(psscan, precPvt->detBufPtr[i].pBufB, DBE_VAL_LOG);
			}
		}
		/*
		 * I must also post a monitor on the NULL array, because some
		 * clients connected to D?PV's without valid PV's !
		 */
		db_post_events(psscan, precPvt->nullArray, DBE_VAL_LOG);

		/* post alert if changed */
		if (precPvt->scanErr) {
			psscan->alrt = precPvt->scanErr; POST(&psscan->alrt);
		}
		if (psscan->xsc != psscan->pxsc) POST(&psscan->xsc);
		if (psscan->exsc != psscan->xsc) {
			psscan->exsc = psscan->xsc; POST(&psscan->exsc);
		}

		/* post buffered copy of cpt for use by data-storage client */
		psscan->bcpt = psscan->cpt;
		POST(&psscan->bcpt);

		/* Tell clients that new array data have been posted */
		psscan->data = 1;
		db_post_events(psscan, &psscan->data, DBE_VAL_LOG);
		if (psscan->aawait == sscanNOYES_YES) {
			psscan->await = 1;
			POST(&psscan->await);
		}
	}
}


/**************************************************************
 * lookupPV()
 * This is the routine that looks up newly entered "dynamic"
 * links. A dbNameToAddr call is done first to
 * see if the PV resides on this IOC. Much better performance is
 * obtained by using dbGet/dbPutField rather than the recDynLink
 * library for local PV's. If the dbNameToAddr fails,
 * recDynLinkAddInput is used.
 *
 * For positioners:
 *     If the PV is local, the dbAddr is used for puts and gets (
 *        to get the "previous positioner" before a scan starts. A
 *        recDynLinkInput is also used to monitor the positioner
 *        for changes made from other places.
 *        This value is stored in p_cv.
 *     If the PV is not local, a recDynLinkOutput and a
 *        recDynLinkInput are used. The recDynLinkInput has a
 *        monitor callback routine for p_cv, but a get is used
 *        to find the previous position.
 * !!! v4.00 use ca for everything so we can do putNotify !!!
 *
 * For Readbacks:
 *     If the PV name starts with TIME or time, set a flag that
 *        indicates that the timestamp should be used as the
 *        readback value.
 *     Otherwise, do a PV lookup.
 **************************************************************/
static void 
lookupPV(sscanRecord * psscan, unsigned short i)
{
	recPvtStruct   *precPvt = (recPvtStruct *) psscan->rpvt;
	recDynLinkPvt  *puserPvt, *pPosOut_userPvt;
	posFields      *pPos;
	char           *ppvn;
	char           *pdesc = ".DESC";
	char           *pval = ".VAL";
	char           *pTIME = "TIME";
	char           *ptime = "time";
	char           *pdot;
	unsigned short *pPvStat;

	/* Point to correct places by using index i */
	ppvn = &psscan->p1pv[0] + (i * PVN_SIZE);
	pPvStat = &psscan->p1nv + i;	/* pointer arithmetic */
	puserPvt = (recDynLinkPvt *) precPvt->caLinkStruct[i].puserPvt;
	pPos = (posFields *) &psscan->p1pp + i;


	/* If PV field name = DESC, change to VAL */
	pdot = strrchr(ppvn, '.');
	if (pdot != NULL) {
		if (strncmp(pdot, pdesc, 5) == 0) {
			strcpy(pdot, pval);
			POST(ppvn);
		}
	}
	/* See if it's a local PV */
	puserPvt->dbAddrNv = dbNameToAddr(ppvn, puserPvt->pAddr);
	if (sscanRecordDebug >= 2) {
		printf("%s:lookupPV: dbNameToAddr('%s') returned %lx (%s)\n",
			psscan->name, ppvn, puserPvt->dbAddrNv, puserPvt->dbAddrNv?"failure":"success");
	}
	switch (puserPvt->linkType) {
	case POSITIONER:	/* setup both inlink and outlink */
		/* Init p_cv so we can use it's value to know if the first monitor callback
		 * has come in. */
		pPos->p_cv = -HUGE_VAL;
		pPosOut_userPvt = (recDynLinkPvt *) precPvt->caLinkStruct[i + NUM_PVS].puserPvt;
		pPosOut_userPvt->connectInProgress = 1;
		recDynLinkAddOutput(&precPvt->caLinkStruct[i + NUM_PVS], ppvn,
			  DBR_DOUBLE, rdlSCALAR, pvSearchCallback);
		puserPvt->connectInProgress = 1;
		recDynLinkAddInput(&precPvt->caLinkStruct[i], ppvn,
			   DBR_DOUBLE, rdlSCALAR, pvSearchCallback, posMonCallback);
		break;

	case READBACK:
		/* Check to see if it equals "time" */
		if ((strncmp(ppvn, pTIME, 4) == 0) || (strncmp(ppvn, ptime, 4) == 0)) {
			puserPvt->ts = 1;
			*pPvStat = NO_PV; POST(pPvStat);
			break;	/* don't do lookups or pvSearchCallback */
		} else {
			puserPvt->ts = 0;
		}
		if (puserPvt->dbAddrNv || puserPvt->useDynLinkAlways) {
			puserPvt->connectInProgress = 1;
			recDynLinkAddInput(&precPvt->caLinkStruct[i], ppvn,
			     DBR_DOUBLE, rdlSCALAR, pvSearchCallback, NULL);
		} else {
			pvSearchCallback(&precPvt->caLinkStruct[i]);
		}
		break;

	case DETECTOR:
		if (puserPvt->dbAddrNv || puserPvt->useDynLinkAlways) {
			/* might be array valued, so don't specify rdlSCALAR */
			puserPvt->connectInProgress = 1;
			recDynLinkAddInput(&precPvt->caLinkStruct[i], ppvn,
			      DBR_FLOAT, 0 /*rdlSCALAR*/, pvSearchCallback, NULL);
		} else {
			pvSearchCallback(&precPvt->caLinkStruct[i]);
		}
		break;

	case TRIGGER:
	case READ_ARRAY_TRIG:
	case BS_AS_LINK:
		puserPvt->connectInProgress = 1;
		recDynLinkAddOutput(&precPvt->caLinkStruct[i], ppvn,
			      DBR_FLOAT, rdlSCALAR, pvSearchCallback);
		break;

	case POSITIONER_OUT:	/* case POSITIONER did everything */
	default:
		break;
	}
}

static void 
delayCallback(CALLBACK *pCB)
{
	sscanRecord     *psscan;
	recPvtStruct	*precPvt;

	callbackGetUser(psscan, pCB);
	precPvt = (recPvtStruct *)psscan->rpvt;
	if (sscanRecordDebug > 10) epicsPrintf("%s:delayCallback:entry\n", psscan->name);
	if (psscan->wcnt) {
		psscan->wtng = 1; POST(&psscan->wtng);
		sprintf(psscan->smsg, "Waiting for client");
		POST(&psscan->smsg);
	} else {
		precPvt->calledBy |= DELAY;
		(void) scanOnce((struct dbCommon *) psscan);
	}
}


LOCAL void 
notifyCallback(recDynLink * precDynLink)
{
	recDynLinkPvt *puserPvt = (recDynLinkPvt *) precDynLink->puserPvt;
	sscanRecord   *psscan = puserPvt->psscan;
	recPvtStruct  *precPvt = (recPvtStruct *) psscan->rpvt;
	int numPosCb, numTrigCb, numAReadCb, numGetCb;

	epicsMutexLock(precPvt->numCallbacksSem);
	numPosCb = precPvt->numPositionerCallbacks;
	numTrigCb = precPvt->numTriggerCallbacks;
	numAReadCb = precPvt->numAReadCallbacks;
	numGetCb = precPvt->numGetCallbacks;
	epicsMutexUnlock(precPvt->numCallbacksSem);

	if (sscanRecordDebug >= 10)
		printf("%s: notifyCallback: num{P,T,R,G}Callbacks = %d, %d, %d, %d\n", psscan->name, 
		       numPosCb, numTrigCb, numAReadCb, numGetCb);

	if (psscan->faze == sscanFAZE_IDLE) {
		/* we must have been aborted */
		sprintf(psscan->smsg, "callback while scan record is idle");
		POST(&psscan->smsg);
		return;
	}

	if (precDynLink->status) printf("%s: notifyCallback: error %d\n", psscan->name, 
		precDynLink->status);


	if (puserPvt->linkType == TRIGGER) {
		if (numTrigCb) {
			epicsMutexLock(precPvt->numCallbacksSem);
			numTrigCb = --(precPvt->numTriggerCallbacks);
			epicsMutexUnlock(precPvt->numCallbacksSem);
			if (numTrigCb == 0) {
				if (psscan->paus) {
					sprintf(psscan->smsg, "Scan paused by operator");
					POST(&psscan->smsg);
					return;
				}
				if (psscan->ddly < .001) {
					if (psscan->wcnt) {
						psscan->wtng = 1; POST(&psscan->wtng);
						sprintf(psscan->smsg, "Waiting for client");
						POST(&psscan->smsg);
					} else {
						precPvt->calledBy = NOTIFY_TRIG;
						(void) scanOnce((struct dbCommon *)psscan);
					}
				} else {
					precPvt->calledBy = NOTIFY_TRIG;
					callbackRequestDelayed(&precPvt->dlyCallback, psscan->ddly);
				}
			}
		}
	} else if (puserPvt->linkType == READ_ARRAY_TRIG) {
		if (numAReadCb) {
			epicsMutexLock(precPvt->numCallbacksSem);
			numAReadCb = --(precPvt->numAReadCallbacks);
			epicsMutexUnlock(precPvt->numCallbacksSem);
			if (numAReadCb == 0) {
				if (psscan->paus) {
					sprintf(psscan->smsg, "Scan paused by operator");
					POST(&psscan->smsg);
					return;
				}
				precPvt->calledBy = NOTIFY_READ_ARRAY_TRIG;
				scanOnce((struct dbCommon *)psscan);
			}
		}
	} else {	/* POSITIONER_OUT, BS_AS_LINK */
		if (numPosCb) {
			epicsMutexLock(precPvt->numCallbacksSem);
			numPosCb = --(precPvt->numPositionerCallbacks);
			epicsMutexUnlock(precPvt->numCallbacksSem);
			if (numPosCb == 0) {
				if (psscan->paus) {
					sprintf(psscan->smsg, "Scan paused by operator");
					POST(&psscan->smsg);
					return;
				}
				precPvt->calledBy = NOTIFY;
				if ((psscan->faze != sscanFAZE_CHECK_MOTORS) || (psscan->pdly == 0.)) {
					scanOnce((struct dbCommon *)psscan);
				} else {
					callbackRequestDelayed(&precPvt->dlyCallback, psscan->pdly);
				}
			}
		}
	} 
}


LOCAL void 
userGetCallback(recDynLink * precDynLink)
{
	recDynLinkPvt	*puserPvt = (recDynLinkPvt *) precDynLink->puserPvt;
	sscanRecord		*psscan = puserPvt->psscan;
	recPvtStruct	*precPvt = (recPvtStruct *) psscan->rpvt;
	size_t			nRequest;
	long			status;
	int				numGetCb;

	if (sscanRecordDebug >= 5) epicsPrintf("%s:userGetCallback, faze='%s', data_state='%s', link='%s'\n",
		psscan->name, sscanFAZE_strings[psscan->faze], sscanDSTATE_strings[psscan->dstate],
		linkNames[puserPvt->linkIndex]);

	if (precDynLink->status) {
		epicsPrintf("%s:userGetCallback: error %d on link '%s'.  Retrying.\n",
			psscan->name, precDynLink->status, linkNames[puserPvt->linkIndex]);
		/* Retry. */
		nRequest = (psscan->faze == sscanFAZE_RECORD_SCALAR_DATA) ? 1 : psscan->npts;
		status = recDynLinkGetCallback(precDynLink, &nRequest, userGetCallback);
		return;
	}

	if (precDynLink->getCallbackInProgress) {
		/* mark this one off */
		precDynLink->getCallbackInProgress = 0;
	} else {
		/* why did we get an extra callback? */
		epicsPrintf("%s:userGetCallback:callback while getCallbackInProgress==0 ignored\n", psscan->name);
		return;
	}

	epicsMutexLock(precPvt->numCallbacksSem);
	precPvt->numGetCallbacks -= 1;
	if (precPvt->numGetCallbacks < 0) {
		epicsPrintf("%s:userGetCallback:ERROR: numGetCallbacks<0 (%d)\n", psscan->name, precPvt->numGetCallbacks);
		precPvt->numGetCallbacks = 0;
	}
	numGetCb = precPvt->numGetCallbacks;
	epicsMutexUnlock(precPvt->numCallbacksSem);

	if (numGetCb == 0) {
		if (psscan->paus) {
			sprintf(psscan->smsg, "Scan paused by operator");
			POST(&psscan->smsg);
			return;
		}
		/* At this point, faze == sscanFAZE_RECORD_SCALAR_DATA or sscanFAZE_SCAN_DONE */
		if (psscan->dstate == sscanDSTATE_ARRAY_GET_CALLBACK_WAIT) {
			psscan->dstate = sscanDSTATE_RECORD_ARRAY_DATA;
		}
		if (sscanRecordDebug >= 5) {
			epicsPrintf("%s:userGetCallback: calling scanOnce(), faze='%s', data_state='%s'\n",
				psscan->name, sscanFAZE_strings[psscan->faze], sscanDSTATE_strings[psscan->dstate]);
		}
		precPvt->calledBy = USERGETCALLBACK;
		scanOnce((struct dbCommon *)psscan);
		return;
	}
	if (sscanRecordDebug >= 5) epicsPrintf("%s:userGetCallback:exit, numGetCallbacks=%d\n",
		psscan->name, numGetCb);
}


LOCAL void 
pvSearchCallback(recDynLink * precDynLink)
{

	recDynLinkPvt  *puserPvt = (recDynLinkPvt *) precDynLink->puserPvt;
	sscanRecord    *psscan = puserPvt->psscan;
	recPvtStruct   *precPvt = (recPvtStruct *) psscan->rpvt;
	posFields      *pPos = (posFields *) & psscan->p1pp;
	detFields      *pDet = (detFields *) & psscan->d01hr;
	unsigned short  linkIndex = puserPvt->linkIndex;
	unsigned short  pvIndex = linkIndex % NUM_PVS;
	unsigned short  detIndex;
	unsigned short *pPvStat, PvStat;
	size_t          nelem;
	long            status;
	long            nRequest = 1;
	long            options = DBR_UNITS | DBR_PRECISION | DBR_GR_DOUBLE | DBR_CTRL_DOUBLE;
	int             i, precision;

	/*
	 * Positioner PVs have two links, so we will have two connection callbacks
	 * competing for access to the shared link-status bitmap *pPvStat.
	 */
	epicsMutexLock(precPvt->pvStatSem);

	pPvStat = &psscan->p1nv + pvIndex;	/* pointer arithmetic */
	PvStat = *pPvStat;	/* start with previous status value */
	puserPvt->connectInProgress = 0;

	if ((puserPvt->dbAddrNv || puserPvt->useDynLinkAlways) &&
			recDynLinkConnectionStatus(precDynLink)) {
		if (sscanRecordDebug >= 2) printf("%s:pvSearchCallback: FAILURE: link '%s'.\n",
				psscan->name, linkNames[puserPvt->linkIndex]);
		switch (puserPvt->linkType) {
		case POSITIONER:
			/* set the bit that indicates the input link is bad */
			PvStat |= PV_NoRead;
			break;
		case POSITIONER_OUT:
			/* set the bit that indicates the output link is bad */
			PvStat |= PV_NoWrite;
			break;
		default:
			PvStat = PV_NC;
			break;
		}
	} else {
		if (sscanRecordDebug >= 2) printf("%s:pvSearchCallback: Success: link '%s'.\n",
				psscan->name, linkNames[puserPvt->linkIndex]);
		switch (puserPvt->linkType) {
		case POSITIONER:
			/* clear the bit that indicates the input link is bad */
			PvStat &= ~PV_NoRead;
			break;
		case POSITIONER_OUT:
			/* clear the bit that indicates the output link is bad */
			PvStat &= ~PV_NoWrite;
			break;
		default:
			PvStat = PV_OK;
			break;
		}
	}

	/* Do some linkType-specific initialization. */
	switch (puserPvt->linkType) {
	case POSITIONER:	/* pvIndexes 0,1,2,3 */
		if (PvStat & PV_NoRead) break;

		pPos += pvIndex;
		/* Get control limits, precision, and egu */
		if (puserPvt->dbAddrNv || puserPvt->useDynLinkAlways) {
			/* use the recDynLink lib */
			recDynLinkGetUnits(precDynLink, pPos->p_eu, 8);
			recDynLinkGetPrecision(precDynLink, &precision);
			pPos->p_pr = precision;
			recDynLinkGetControlLimits(precDynLink, &pPos->p_lr, &pPos->p_hr);
		} else {
			status = dbGet(puserPvt->pAddr, DBR_FLOAT, precPvt->pDynLinkInfo,
					&options, &nRequest, NULL);
			if (status == OK) {
				strcpy(pPos->p_eu, precPvt->pDynLinkInfo->units);
#if LT_EPICSBASE(3,14,10)
				pPos->p_pr = precPvt->pDynLinkInfo->precision;
#else
				pPos->p_pr = precPvt->pDynLinkInfo->precision.dp;
#endif
				pPos->p_hr = precPvt->pDynLinkInfo->upper_ctrl_limit;
				pPos->p_lr = precPvt->pDynLinkInfo->lower_ctrl_limit;
			}
		}
		POST(&pPos->p_eu);
		POST(&pPos->p_pr);
		POST(&pPos->p_hr);
		POST(&pPos->p_lr);

		break;

	case DETECTOR:
		if (PvStat != PV_OK) break;
		/* Derive pointer to appropriate detector fields */
		detIndex = pvIndex - D1_IN;
		pDet += detIndex;	/* pointer arithmetic */

		/* Get display limits, precision, and egu */
		if (puserPvt->dbAddrNv || puserPvt->useDynLinkAlways) {
			/* use the recDynLink lib */
			recDynLinkGetUnits(precDynLink, pDet->d_eu, 8);
			recDynLinkGetPrecision(precDynLink, &precision);
			pDet->d_pr = precision;
			recDynLinkGetGraphicLimits(precDynLink, &pDet->d_lr, &pDet->d_hr);
		} else {
			status = dbGet(puserPvt->pAddr, DBR_FLOAT, precPvt->pDynLinkInfo,
					&options, &nRequest, NULL);
			if (status == OK) {
				strcpy(pDet->d_eu, precPvt->pDynLinkInfo->units);
#if LT_EPICSBASE(3,14,10)
				pPos->p_pr = precPvt->pDynLinkInfo->precision;
#else
				pDet->d_pr = precPvt->pDynLinkInfo->precision.dp;
#endif
				pDet->d_hr = precPvt->pDynLinkInfo->upper_disp_limit;
				pDet->d_lr = precPvt->pDynLinkInfo->lower_disp_limit;
			}
		}
		POST(&pDet->d_eu);
		POST(&pDet->d_pr);
		POST(&pDet->d_hr);
		POST(&pDet->d_lr);

		/* get # of elements, re-allocate array as necessary */
		if (puserPvt->dbAddrNv || puserPvt->useDynLinkAlways) {
			status = recDynLinkGetNelem(precDynLink, &nelem);
		} else {
			nelem = puserPvt->pAddr->no_elements;
		}

		if (nelem > 1) {
			sprintf(psscan->smsg, "Array-valued detector");
			POST(&psscan->smsg);
		}
		/*
		 * Note we haven't set puserPvt->nelem to nelem yet, so if it's nonzero,
		 * this link has been used before.  Otherwise, we need to allocate and
		 * initialize it's data buffers.
		 */ 
		if (puserPvt->nelem == 0) {
			/* Allocate data buffers, and init data pointer */
			printf("%s: Allocating memory for detector %d (link '%s')\n",
				psscan->name, detIndex+1, linkNames[puserPvt->linkIndex]);
			precPvt->detBufPtr[detIndex].pBufA =
				(float *) calloc(psscan->mpts, sizeof(float));
			precPvt->detBufPtr[detIndex].pBufB =
				(float *) calloc(psscan->mpts, sizeof(float));
			pDet->d_da = precPvt->detBufPtr[detIndex].pBufA;

			if (!(precPvt->detBufPtr[detIndex].pBufA) ||
			    !(precPvt->detBufPtr[detIndex].pBufB)) {
				/* MEMORY ALLOCATION FAILED */
				printf("%s:MEMORY ALLOCATION FAILED \n", psscan->name);
				/* leave puserPvt->nelem == 0 */
			} else {
				puserPvt->nelem = nelem;
			}
		} else {
			puserPvt->nelem = nelem;
		}
		if (sscanRecordDebug >= 5) printf("%s:pvSearchCallback: link '%s', setting nelem to %ld.\n",
			psscan->name, linkNames[puserPvt->linkIndex], puserPvt->nelem);

		if (precPvt->validBuf == B_BUFFER) {
			precPvt->detBufPtr[detIndex].pFill =
				precPvt->detBufPtr[detIndex].pBufA;
		} else {
			precPvt->detBufPtr[detIndex].pFill =
				precPvt->detBufPtr[detIndex].pBufB;
		}
		break;

	default:
		break;
	}

	/* Announce link status to the rest of the world */
	if (*pPvStat != PvStat) {
		*pPvStat = PvStat; POST(pPvStat);
	}
	epicsMutexUnlock(precPvt->pvStatSem);

	if (PvStat != PV_OK) return;

	/*
	 * If a scan is pending PV connections, see if the PV we just certified was the last
	 * one holding the scan up.  If so, start the scan.
	 */
	if (psscan->faze == sscanFAZE_SCAN_PENDING) {
		checkConnections(psscan);
		if (precPvt->badOutputPv || precPvt->badInputPv) {
			return;
		} else if (!psscan->xsc) {
			if (sscanRecordDebug >= 2) printf("%s:pvSearchCallback: pending scan was aborted\n",
					psscan->name);
			psscan->faze = sscanFAZE_IDLE; POST(&psscan->faze);
		} else {
			/* If monitors have come in for all positioners, start scan. */
			pPvStat = &psscan->p1nv;
			pPos = (posFields *) &psscan->p1pp;
			for (i = 1; i <= NUM_POS; i++, pPvStat++, pPos++) {
				if (sscanRecordDebug >= 2)
					printf("%s:pvSearchCallback: pPvStat[%d]=%d, pPos[%d].p_cv=%g\n",
						psscan->name, i, *pPvStat, i, pPos->p_cv);
				if ((*pPvStat == PV_OK) && (pPos->p_cv == -HUGE_VAL)) {
					/* Haven't received the first monitor callback yet.  Wait for it. */
					return;
				}
			}
			if (sscanRecordDebug >= 2) printf("%s:pvSearchCallback: scan pending - call scanOnce()\n",
					psscan->name);
			precPvt->scanBySearchCallback = 1;
			precPvt->calledBy = SEARCH;
			scanOnce((struct dbCommon *)psscan);
		}
	}
	return;
}

LOCAL void 
posMonCallback(recDynLink * precDynLink)
{

	recDynLinkPvt  *puserPvt = (recDynLinkPvt *) precDynLink->puserPvt;
	sscanRecord    *psscan = puserPvt->psscan;
	recPvtStruct   *precPvt = (recPvtStruct *) psscan->rpvt;
	unsigned short  linkIndex = puserPvt->linkIndex;
	unsigned short  pvIndex = linkIndex % NUM_PVS;
	posFields      *pPos = (posFields *) & psscan->p1pp + pvIndex;
	long            status, i;
	size_t          nRequest = 1;
	unsigned short *pPvStat = &psscan->p1nv + pvIndex;

	if (sscanRecordDebug >= 20) printf("%s:posMonCallback: link '%s'.\n",
		psscan->name, linkNames[puserPvt->linkIndex]);

	/* After a link has been cleared and targeted at a new PV, we might still get a late monitor
	 * callback from the old PV.  If we copy the value to p_cv, it will look like the new link has
	 * begun to function.  Avoid this by ignoring monitor callbacks that occur after a link has
	 * been cleared but before the link's search callback has come in.
	 */
	if (*pPvStat & PV_NoRead) {
		printf("%s:posMonCallback: ignoring obsolete monitor callback (link state: 'PV_NoRead')\n",
					psscan->name);
		return;
	}

	/* update p_cv with current positioner value */
	/*
	 * NOTE: For fast scans, these will get lost, because it is the low
	 * priority channel access task that is doing these callbacks
	 */
	status = recDynLinkGet(&precPvt->caLinkStruct[linkIndex],
			       &pPos->p_cv, &nRequest, 0, 0, 0);
	POST(&pPos->p_cv);
	if (sscanRecordDebug > 5) {
		printf("%s:posMonCallback: pvIndex=%d, cv=%f\n", psscan->name, pvIndex,
				pPos->p_cv);
	}
	/*
	 * If a scan is pending PV connections, see if the PV whose value just came in was the last
	 * one holding the scan up.  If so, start the scan.
	 */
	if (psscan->faze == sscanFAZE_SCAN_PENDING) {
		checkConnections(psscan);
		if (precPvt->badOutputPv || precPvt->badInputPv) {
			return;
		}
		if (!psscan->xsc) {
			if (sscanRecordDebug >= 2) printf("%s:posMonCallback: pending scan was aborted\n",
					psscan->name);
			psscan->faze = sscanFAZE_IDLE; POST(&psscan->faze);
			return;
		}

		/* Have any of the positioners involved in the scan still not called back with a value? */
		pPvStat = &psscan->p1nv;
		pPos = (posFields *) &psscan->p1pp;
		for (i = 1; i <= NUM_POS; i++, pPvStat++, pPos++) {
			if ((*pPvStat == PV_OK) && (pPos->p_cv == -HUGE_VAL)) {
				/* Haven't received a posMonCallback for this positioner yet. */
				return;
			}
		}
		if (sscanRecordDebug >= 2) printf("%s:posMonCallback: scan pending - call scanOnce()\n",
				psscan->name);
		precPvt->scanBySearchCallback = 1;
		precPvt->calledBy = POSMON;
		scanOnce((struct dbCommon *)psscan);
	}
}


static void 
checkConnections(sscanRecord * psscan)
{
	recPvtStruct   *precPvt = (recPvtStruct *) psscan->rpvt;
	unsigned short *pPvStat;
	unsigned char   badOutputPv = 0;
	unsigned char   badInputPv = 0;
	unsigned short  i;

	pPvStat = &psscan->p1nv;

	for (i = 1; i <= NUM_POS; i++, pPvStat++) {
		if (*pPvStat & PV_NC) badOutputPv = 1;
	}
	/* let pPvStat continue to increment into Readbacks */
	for (i = 1; i <= NUM_POS; i++, pPvStat++) {
		if (*pPvStat & PV_NC) badOutputPv = 1;
	}
	/* let pPvStat continue to increment into Detectors */
	for (i = 1; i <= NUM_DET; i++, pPvStat++) {
		if (*pPvStat & PV_NC) badInputPv = 1;
	}
	/* let pPvStat continue to increment into Triggers */
	for (i = 1; i <= NUM_TRGS; i++, pPvStat++) {
		if (*pPvStat & PV_NC) badOutputPv = 1;
	}
	/* let pPvStat continue to increment into Array-read triggers */
	for (i = 1; i <= NUM_ATRGS; i++, pPvStat++) {
		if (*pPvStat & PV_NC) badOutputPv = 1;
	}
	/* let pPvStat continue to increment into before/after-scan links */
	for (i = 1; i <= NUM_MISC; i++, pPvStat++) {
		if (*pPvStat & PV_NC) badOutputPv = 1;
	}
	precPvt->badOutputPv = badOutputPv;
	precPvt->badInputPv = badInputPv;
}



/********************** SCAN Initialization  ********************************/
/*
 * If a "Before Scan Link PV" is defined, this routine will be called twice,
 * once before the link is processed and once after. The starting positions
 * and limits are calculated each time, in case the link caused something to
 * change.
 *
 */
static long 
initScan(sscanRecord *psscan)
{
	recPvtStruct   *precPvt = (recPvtStruct *) psscan->rpvt;
	posFields      *pPos;
	long            status;
	unsigned short *pPvStat;
	int             i;


	/* General initialization ... */
	epicsTimeGetCurrent(&precPvt->timeStart);
	psscan->cpt = 0;		/* reset point counter */
	precPvt->scanErr = 0;

	/* determine highest valid positioner, readback, trigger, and detector */
	precPvt->valPosPvs = 0;
	precPvt->valRdbkPvs = 0;
	precPvt->valDetPvs = 0;
	precPvt->valTrigPvs = 0;
	precPvt->valATrigPvs = 0;
	pPvStat = &psscan->p1nv;

	for (i = 1; i <= NUM_POS; i++, pPvStat++) {
		if (*pPvStat == PV_OK) precPvt->valPosPvs = i;
	}
	/* let pPvStat continue to increment into Readbacks */
	for (i = 1; i <= NUM_POS; i++, pPvStat++) {
		if (*pPvStat == PV_OK) precPvt->valRdbkPvs = i;
	}
	/* let pPvStat continue to increment into Detectors */
	/* flag which detectors should be acquired during scan */
	for (i = 1; i <= NUM_DET; i++, pPvStat++) {
		if (*pPvStat == PV_OK) {
			precPvt->valDetPvs = i;
			precPvt->acqDet[i - 1] = 1;
		} else {
			precPvt->acqDet[i - 1] = 0;
		}
	}
	/* let pPvStat continue to increment into Triggers */
	for (i = 1; i <= NUM_TRGS; i++, pPvStat++) {
		if (*pPvStat == PV_OK) precPvt->valTrigPvs = i;
	}
	/* let pPvStat continue to increment into Array-read triggers */
	for (i = 1; i <= NUM_ATRGS; i++, pPvStat++) {
		if (*pPvStat == PV_OK) precPvt->valATrigPvs = i;
	}
	if (sscanRecordDebug > 10) {
		printf("%s:Positioners      : %u\n", psscan->name, precPvt->valPosPvs);
		printf("%s:Readbacks        : %u\n", psscan->name, precPvt->valRdbkPvs);
		printf("%s:Detectors        : %u\n", psscan->name, precPvt->valDetPvs);
		printf("%s:Triggers         : %u\n", psscan->name, precPvt->valTrigPvs);
		printf("%s:Array-read Trigs : %u\n", psscan->name, precPvt->valATrigPvs);
	}
	/*
	 * checkScanLimits must be called to update the current value of each
	 * positioner into p_pp. If sscanRecordDontCheckLimits is set, it will
	 * return successful w/o actually checking
	 */

	if ((status = checkScanLimits(psscan))) {
		/* limits didn't pass, or couldn't get current positioner value.  abort scan */
		psscan->xsc = 0; POST(&psscan->xsc);
		psscan->exsc = 0; POST(&psscan->exsc);
		POST(&psscan->smsg);
		POST(&psscan->alrt);
		/* Probably shouldn't say the scan is done when limit trouble is
		 * encountered, because multidimensional scan could appear to be acquiring
		 * data that it's not, in fact, even attempting to acquire.  Better to
		 * hang, so user will notice the problem.
		 */ 
		/* psscan->faze = sscanFAZE_SCAN_DONE; POST(&psscan->faze); */
		return (status);
	}

	/* Then calculate the starting position */
	precPvt->onTheFly = precPvt->flying = 0;	/* clear onTheFly flag */
	pPvStat = &psscan->p1nv;
	pPos = (posFields *) & psscan->p1pp;
	for (i = 0; i < precPvt->valPosPvs; i++, pPos++, pPvStat++) {
		if (*pPvStat == PV_OK) {
			/* Figure out starting positions for each positioner */
			pPos->p_dv = (pPos->p_sm == sscanP1SM_Table) ? pPos->p_pa[0] : pPos->p_sp;
			if (pPos->p_ar) pPos->p_dv += pPos->p_pp;

			/* tmm v3.15 some positioners must see a change in their drive value to move */
			if (pPos->p_dv == pPos->p_pp) pPos->p_dv *= (1 + DBL_EPSILON);

			POST(&pPos->p_dv);
			if (pPos->p_sm == sscanP1SM_On_The_Fly) {
				precPvt->onTheFly |= 1;	/* set flag if onTheFly */
			}
		}
	}

	if ((psscan->bsnv == PV_OK) && (psscan->faze == sscanFAZE_INIT_SCAN)) {
		psscan->faze = sscanFAZE_BEFORE_SCAN; POST(&psscan->faze);
		sprintf(psscan->smsg, "Before Scan FLNK ...");
		POST(&psscan->smsg);
	} else {
		psscan->faze = sscanFAZE_MOVE_MOTORS; POST(&psscan->faze);
		sprintf(psscan->smsg, "Scanning ...");
		POST(&psscan->smsg);
	}
	/* request callback to do dbPutFields */
	callbackRequest(&precPvt->doPutsCallback);
	return (OK);
}

static void 
contScan(sscanRecord *psscan)
{
	recPvtStruct   *precPvt = (recPvtStruct *) psscan->rpvt;
	posFields      *pPos = (posFields *) & psscan->p1pp;
	detFields      *pDet = (detFields *) & psscan->d01hr;
	recDynLinkPvt  *puserPvt;
	epicsTimeStamp  currentTime;
	unsigned short *pPvStat;
	unsigned short *pPvStatPos;
	unsigned short  i;
	long			status;
	size_t          nRequest = 1;
	double			oldPos;

	if (sscanRecordDebug) printf("%s:contScan, faze='%s', data_state='%s'\n",
		psscan->name, sscanFAZE_strings[psscan->faze], sscanDSTATE_strings[psscan->dstate]);

	switch ((int)psscan->faze) {
	case sscanFAZE_TRIG_DETCTRS:
		/* go output to detector trigger fields */
		callbackRequest(&precPvt->doPutsCallback);
		return;

	case sscanFAZE_CHECK_MOTORS:
		if (sscanRecordDebug >= 5) {
			printf("%s:contScan:CHECK_MOTORS  - Point %ld\n", psscan->name, (long)psscan->cpt);
		}
		/* check if a readback PV and a delta are specified */
		pPvStat = &psscan->r1nv;
		pPvStatPos = &psscan->p1nv;
		for (i = 0; i < NUM_POS; i++, pPos++, pPvStatPos++, pPvStat++) {
			if ((pPos->r_dl != 0) &&
			    (*pPvStat == PV_OK) &&
			    (*pPvStatPos == PV_OK)) {
				puserPvt = precPvt->caLinkStruct[i + NUM_POS].puserPvt;
				if (puserPvt->dbAddrNv || puserPvt->useDynLinkAlways) {
					status = recDynLinkGet(&precPvt->caLinkStruct[i + NUM_POS],
							       &pPos->r_cv, &nRequest, 0, 0, 0);
				} else {
					status = dbGet(puserPvt->pAddr, DBR_DOUBLE, &pPos->r_cv,
						       0, 0, NULL);
				}

				if ((pPos->r_dl > 0) &&
				    (fabs(pPos->p_dv - pPos->r_cv) > pPos->r_dl)) {
					sprintf(psscan->smsg, "SCAN Aborted: P%1d Error > delta", i+1);
					POST(&psscan->smsg);
					precPvt->scanErr = 1;
					printf("%s: P%1d Error > delta.  Ending scan.\n", psscan->name, i+1);
					endScan(psscan);
					return;
				} else if ((pPos->r_dl < 0) && (pPos->p_sm != sscanP1SM_Table) &&
						(fabs(pPos->p_dv - pPos->r_cv) >
						fabs(pPos->p_si * NINT(pPos->r_dl)))
					) {
					sprintf(psscan->smsg, "SCAN Aborted: P%1d Error > stepsize", i+1);
					POST(&psscan->smsg);
					precPvt->scanErr = 1;
					printf("%s: P%1d Error > stepsize.  Ending scan.\n", psscan->name, i+1);
					endScan(psscan);
					return;
				}
			}
		}

		if (precPvt->onTheFly && !precPvt->flying) {
			/* determine next desired position for each positioner */
			pPos = (posFields *) & psscan->p1pp;
			pPvStat = &psscan->p1nv;
			for (i = 0; i < precPvt->valPosPvs; i++, pPos++, pPvStat++) {
				if (*pPvStat == PV_OK) {
					oldPos = pPos->p_dv;
					switch (pPos->p_sm) {
					case sscanP1SM_Linear:
						pPos->p_dv = pPos->p_dv + pPos->p_si;
						break;
					case sscanP1SM_Table:
						if (pPos->p_ar) {
							pPos->p_dv = pPos->p_pp + pPos->p_pa[psscan->cpt];
						} else {
							pPos->p_dv = pPos->p_pa[psscan->cpt];
						}
						break;
					case sscanP1SM_On_The_Fly:
						if (pPos->p_ar) {
							pPos->p_dv = pPos->p_pp + pPos->p_ep;
						} else {
							pPos->p_dv = pPos->p_ep;
						}
						break;
					}
					if (pPos->p_dv == oldPos) pPos->p_dv *= (1 + DBL_EPSILON);
				}
			}
		}

		if (precPvt->valTrigPvs && !precPvt->flying) {
			/* do detector trigger fields */
			psscan->faze = precPvt->onTheFly ? sscanFAZE_START_FLY : sscanFAZE_TRIG_DETCTRS;
			POST(&psscan->faze);
			callbackRequest(&precPvt->doPutsCallback);
			return;
		}
		/* if no validTrigPvs, fall through to READ_DETCTRS */
		/* if on-the-fly mode and flying, fall through to READ_DETCTRS */
		psscan->faze = sscanFAZE_READ_DETCTRS; POST(&psscan->faze);

	case sscanFAZE_READ_DETCTRS:
		/*** Queue reads for any remote positioner or detector data. ***/
		if (sscanRecordDebug >= 5) {
			printf("%s:contScan:READ_DETCTRS - Point %ld\n", psscan->name, (long)psscan->cpt);
		}

		/* Preset numGetCallbacks so callback routine can't decrement to zero before we're done launching all. */
		epicsMutexLock(precPvt->numCallbacksSem);
		precPvt->numGetCallbacks = 1;
		epicsMutexUnlock(precPvt->numCallbacksSem);

		/* Positioner readbacks */
		pPvStat = &psscan->r1nv;
		pPvStatPos = &psscan->p1nv;
		pPos = (posFields *) & psscan->p1pp;
		for (i = 0; i < NUM_POS; i++, pPos++, pPvStatPos++, pPvStat++) {
			/* if readback PV is OK, use that value */
			puserPvt = precPvt->caLinkStruct[i + NUM_POS].puserPvt;
			if (*pPvStat == PV_OK) {
				if (puserPvt->dbAddrNv || puserPvt->useDynLinkAlways) {
					epicsMutexLock(precPvt->numCallbacksSem);
					precPvt->numGetCallbacks++;
					epicsMutexUnlock(precPvt->numCallbacksSem);
					status = recDynLinkGetCallback(&precPvt->caLinkStruct[i + NUM_POS],
								&nRequest, userGetCallback);
				}
			}
		}

		/* Detectors */
		pPvStat = &psscan->d01nv;
		pDet = (detFields *) & psscan->d01hr;
		for (i = 0; i < precPvt->valDetPvs; i++, pDet++, pPvStat++) {
			if (precPvt->acqDet[i] && (precPvt->detBufPtr[i].pFill != NULL)) {
				if (*pPvStat == PV_OK) {
					puserPvt = precPvt->caLinkStruct[i + D1_IN].puserPvt;
					if (puserPvt->dbAddrNv || puserPvt->useDynLinkAlways) {
						epicsMutexLock(precPvt->numCallbacksSem);
						precPvt->numGetCallbacks++;
						epicsMutexUnlock(precPvt->numCallbacksSem);
						status = recDynLinkGetCallback(&precPvt->caLinkStruct[i + D1_IN],
									&nRequest, userGetCallback);
					}
				}
			}
		}

		psscan->faze = sscanFAZE_RECORD_SCALAR_DATA; POST(&psscan->faze);

		/* Remove the preset we started with. */
		epicsMutexLock(precPvt->numCallbacksSem);
		precPvt->numGetCallbacks -= 1;
		if (precPvt->numGetCallbacks) {
			/* Wait for callbacks */
			epicsMutexUnlock(precPvt->numCallbacksSem);
			return;
		}
		epicsMutexUnlock(precPvt->numCallbacksSem);

		/* else fall through */

	case sscanFAZE_RECORD_SCALAR_DATA:
		/*** Read positioner and detector data into arrays. ***/
		if (sscanRecordDebug >= 5) {
			printf("%s:contScan:RECORD_SCALER_DATA - Point %ld\n", psscan->name, (long)psscan->cpt);
		}
		/* Store the appropriate value into the positioner readback array */
		/* from RxCV or PxDV or TIME */
		pPvStat = &psscan->r1nv;
		pPvStatPos = &psscan->p1nv;
		pPos = (posFields *) & psscan->p1pp;
		for (i = 0; i < NUM_POS; i++, pPos++, pPvStatPos++, pPvStat++) {
			/* if readback PV is OK, use that value */
			puserPvt = precPvt->caLinkStruct[i + NUM_POS].puserPvt;
			if (*pPvStat == PV_OK) {
				if (puserPvt->dbAddrNv || puserPvt->useDynLinkAlways) {
					status = recDynLinkGet(&precPvt->caLinkStruct[i + NUM_POS],
							       &pPos->r_cv, &nRequest, 0, 0, 0);
				} else {
					status = dbGet(puserPvt->pAddr, DBR_DOUBLE, &pPos->r_cv,
						       0, 0, NULL);
				}
			}
			/* Does the readback PV = "time" ? */
			else if (puserPvt->ts) {
					epicsTimeGetCurrent(&currentTime);
					pPos->r_cv = epicsTimeDiffInSeconds(&currentTime, &psscan->time);
			}
			/* Is the positioner PV valid ? */
			else if (*pPvStatPos == PV_OK) {
				/* stuff array with desired value */
				/* If onTheFly and flying, add the step increment to the previous */
				if ((pPos->p_sm != sscanP1SM_On_The_Fly) || !precPvt->flying) {
					pPos->r_cv = pPos->p_dv;
				} else {
					pPos->r_cv = precPvt->posBufPtr[i].pFill[psscan->cpt - 1] + pPos->p_si;
				}
			} else {
				/* Neither PV is valid, store the current point number */
				pPos->r_cv = psscan->cpt;
			}
			precPvt->posBufPtr[i].pFill[psscan->cpt] = pPos->r_cv;
		}

		/* read each valid detector PV, place data in buffered array */
		status = 0;
		pPvStat = &psscan->d01nv;
		pDet = (detFields *) & psscan->d01hr;
		for (i = 0; i < precPvt->valDetPvs; i++, pDet++, pPvStat++) {
			if (precPvt->acqDet[i] && (precPvt->detBufPtr[i].pFill != NULL)) {
				if (*pPvStat == PV_OK) {
					puserPvt = precPvt->caLinkStruct[i + D1_IN].puserPvt;
					if (puserPvt->dbAddrNv || puserPvt->useDynLinkAlways) {
						status |= recDynLinkGet(&precPvt->caLinkStruct[i + D1_IN],
									&pDet->d_cv, &nRequest, 0, 0, 0);
					} else {
						status |= dbGet(puserPvt->pAddr, DBR_FLOAT, &pDet->d_cv,
								0, 0, NULL);
					}
				} else {
					pDet->d_cv = 0.;
				}
				if ((precPvt->prevNumPts > psscan->cpt) && ((psscan->acqm == sscanACQM_ADD) ||
						((psscan->acqm == sscanACQM_ACC) && (precPvt->prevACQM == sscanACQM_ACC)))) {
					/* validBuf indicates where previous sscan's data are stored */
					if (precPvt->validBuf == A_BUFFER) {
						pDet->d_cv += precPvt->detBufPtr[i].pBufA[psscan->cpt];
					} else {
						pDet->d_cv += precPvt->detBufPtr[i].pBufB[psscan->cpt];
					}
				}
				precPvt->detBufPtr[i].pFill[psscan->cpt] = pDet->d_cv;
			}
		}

		/*
		 * If we're posting arrays during scans, and user wants the arrays fixed for plotting
		 * software that doesn't pay attention to .CPT, then repeat the last data point in all
		 * arrays.  Note that this can be a very expensive operation if MPTS is large.
		 */
		if (psscan->copyto && (psscan->atime >= 0.1))
			copyLastPoint(psscan, psscan->cpt, psscan->copyto);

		psscan->udf = 0;
		if (psscan->acqt == sscanACQT_1D_ARRAY) {
			/*** scan record gets all points in one pass ***/
			psscan->cpt = psscan->npts;
		} else {
			/*** normal point-by-point data acquisition ***/
			psscan->cpt++;
		}

		/* Has number of points been reached ? */
		if (psscan->cpt < (psscan->npts)) {
			/* determine next desired position for each  positioner */
			pPos = (posFields *) & psscan->p1pp;
			pPvStat = &psscan->p1nv;

			/* figure out next position (if on-the-fly, we're already going there) */
			for (i = 0; i < precPvt->valPosPvs; i++, pPos++, pPvStat++) {
				if (*pPvStat == PV_OK && (pPos->p_sm != sscanP1SM_On_The_Fly)) {
					oldPos = pPos->p_dv;
					if (pPos->p_sm ==  sscanP1SM_Linear) {
						pPos->p_dv = pPos->p_dv + pPos->p_si;
					} else {
						pPos->p_dv = pPos->p_pa[psscan->cpt];
						if (pPos->p_ar) pPos->p_dv += pPos->p_pp;
					}
					if (pPos->p_dv == oldPos) pPos->p_dv *= (1 + DBL_EPSILON);
				}
			}

			/* request callback to move motors to new positions */
			psscan->faze = sscanFAZE_MOVE_MOTORS; POST(&psscan->faze);
			/* For onTheFly, doPuts will fall through to TRIG_DETCTRS after MOVE_MOTORS */
			callbackRequest(&precPvt->doPutsCallback);
			return;
		} else {
			endScan(psscan);	/* scan is successfully complete */
			sprintf(psscan->smsg, "SCAN Complete");
			POST(&psscan->smsg);
			precPvt->scanErr = 0;
			return;
		}
	}
}

static void 
endScan(sscanRecord *psscan)
{
	recPvtStruct   *precPvt = (recPvtStruct *) psscan->rpvt;

	if (sscanRecordDebug) printf("%s:endScan, faze='%s', data_state='%s'\n",
		psscan->name, sscanFAZE_strings[psscan->faze], sscanDSTATE_strings[psscan->dstate]);

	if (psscan->dstate == sscanDSTATE_UNPACKED) packData(psscan, 3);
	if (psscan->dstate == sscanDSTATE_UNPACKED) {
		/* packData didn't finish; probably waiting for previous scan's data to be written.
		 * For now, don't try to do after-scan stuff until packData finishes
		 */
		return;
	}

	psscan->xsc = 0;	/* done with scan */
	epicsTimeGetCurrent(&precPvt->lastScanEndTime);

	if (psscan->pasm && precPvt->valPosPvs) {
		psscan->faze = sscanFAZE_RETRACE_MOVE; POST(&psscan->faze);
		/* request callback to do dbPutFields */
		callbackRequest(&precPvt->doPutsCallback);
	} else {
		afterScan(psscan);
	}
	return;
}


static void 
readArrays(sscanRecord *psscan)
{
	recPvtStruct   *precPvt = (recPvtStruct *) psscan->rpvt;
	posFields      *pPos = (posFields *) & psscan->p1pp;
	detFields      *pDet = (detFields *) & psscan->d01hr;
	recDynLinkPvt  *puserPvt;
	unsigned short *pPvStat;
	unsigned short *pPvStatPos;
	unsigned short  i, addToPrev;
	long			j;
	long            status, nReq = 1;
	size_t          nRequest = 1;
	double			*pDbuff;
	float			*pFbuff, *pf;

	/*** Read positioner and detector data into arrays. ***/
	/*
	 * Note this routine will get called twice if it needs to read any remote array-valued PV's.
	 * The first time it's called, it do any needed recDynLinkGetCallback()'s.  If it did any,
	 * it will just return.  When all the callbacks have come in, this routine will be called
	 * again, this time to copy data to local storage.
	 */
	 
	if (sscanRecordDebug >= 5) {
		printf("%s:readArrays - dstate=%s\n", psscan->name, sscanDSTATE_strings[psscan->dstate]);
	}

	if (psscan->dstate == sscanDSTATE_ARRAY_READ_WAIT) {
		/* Queue any remote reads */

#if 0
		/*
		 * array-valued positioners, which we told recDynLink we don't have.  (We told
		 * recDynLink these were scalars, so it didn't allocate the memory buffer that
		 * recDynLinkGetCallback's callback would fill.)
		 */
		pPvStat = &psscan->r1nv;
		pPvStatPos = &psscan->p1nv;
		pPos = (posFields *) & psscan->p1pp;
		for (i = 0; i < NUM_POS; i++, pPos++, pPvStatPos++, pPvStat++) {
			/* if positioner-readback PV is OK, use it */
			puserPvt = precPvt->caLinkStruct[i + NUM_POS].puserPvt;
			if (puserPvt->nelem > 1) {
				nRequest = psscan->npts;
				if (*pPvStat == PV_OK) {
					if (puserPvt->dbAddrNv || puserPvt->useDynLinkAlways) {
						status = recDynLinkGetCallback(&precPvt->caLinkStruct[i + NUM_POS],
								       &nRequest, userGetCallback);
						epicsMutexLock(precPvt->numCallbacksSem);
						precPvt->numGetCallbacks++;
						epicsMutexUnlock(precPvt->numCallbacksSem);
					}
				}
			}
		}
#endif

		/* Queue reads for array-valued detectors, if any. */
		status = 0;
		pPvStat = &psscan->d01nv;
		pDet = (detFields *) & psscan->d01hr;
		for (i = 0; i < precPvt->valDetPvs; i++, pDet++, pPvStat++) {
			if (precPvt->acqDet[i] && (precPvt->detBufPtr[i].pFill != NULL)) {
				puserPvt = precPvt->caLinkStruct[i + D1_IN].puserPvt;
				if (sscanRecordDebug >= 5) {
					printf("%s:readArrays: link=%s, nelem=%ld\n", psscan->name,
						linkNames[puserPvt->linkIndex], puserPvt->nelem);
				}
				if (puserPvt->nelem > 1) {
					nRequest = psscan->npts;
					if (*pPvStat == PV_OK) {
						if (puserPvt->dbAddrNv || puserPvt->useDynLinkAlways) {
							status |= recDynLinkGetCallback(&precPvt->caLinkStruct[i + D1_IN],
										&nRequest, userGetCallback);
							epicsMutexLock(precPvt->numCallbacksSem);
							precPvt->numGetCallbacks++;
							epicsMutexUnlock(precPvt->numCallbacksSem);
						} 
					}
				}
			}
		}

		epicsMutexLock(precPvt->numCallbacksSem);
		if (precPvt->numGetCallbacks) {
			psscan->dstate = sscanDSTATE_ARRAY_GET_CALLBACK_WAIT;
			epicsMutexUnlock(precPvt->numCallbacksSem);
			return;
		}
		epicsMutexUnlock(precPvt->numCallbacksSem);
		psscan->dstate = sscanDSTATE_RECORD_ARRAY_DATA;
	}

	/*
	 * Either we had no remote reads, or we've seen callbacks for all of them.
	 * In either case, recDynLink now has good data in its buffers.  Record it.
	 */

	/* Read array-valued positioner readbacks, if any */
	pPvStat = &psscan->r1nv;
	pPvStatPos = &psscan->p1nv;
	pPos = (posFields *) & psscan->p1pp;
	for (i = 0; i < NUM_POS; i++, pPos++, pPvStatPos++, pPvStat++) {
		pDbuff = precPvt->posBufPtr[i].pFill;
		/* if readback PV is OK, use that value */
		puserPvt = precPvt->caLinkStruct[i + NUM_POS].puserPvt;
		if (puserPvt->nelem > 1) {
			nRequest = nReq = psscan->npts;
			if (*pPvStat == PV_OK) {
				if (puserPvt->dbAddrNv || puserPvt->useDynLinkAlways) {
					status = recDynLinkGet(&precPvt->caLinkStruct[i + NUM_POS],
							       pDbuff, &nRequest, 0, 0, 0);
					if (nRequest < psscan->npts)
						for (j = nRequest; j < psscan->npts; j++) pDbuff[j] = 0;
				} else {
					status = dbGet(puserPvt->pAddr, DBR_DOUBLE, pDbuff,
						       0, &nReq, NULL);
					if (nReq < psscan->npts)
						for (j = nReq; j < psscan->npts; j++) pDbuff[j] = 0;
				}
			} else {
				/* stuff array with desired values */
				for (j = 0; j < psscan->npts; j++) pDbuff[j] = pPos->p_sp + j * pPos->p_si;
			}
		} else if (psscan->acqt == sscanACQT_1D_ARRAY) {
			/*
			 * We don't have point-by-point data, and we don't support positioner readback
			 * arrays.  Make sure we have something client can use as an X axis.
			 */
			for (j = 0; j < psscan->npts; j++) pDbuff[j] = pPos->p_sp + j * pPos->p_si;
		}
	}

	/*
	 * Read array-valued detectors, if any.  Note that we might be accumulating with the
	 * previous scan's data, so read into a buffer first.
	 */
	status = 0;
	addToPrev = (psscan->acqm == sscanACQM_ADD) ||
				((psscan->acqm == sscanACQM_ACC) && (precPvt->prevACQM == sscanACQM_ACC));
	pPvStat = &psscan->d01nv;
	pDet = (detFields *) & psscan->d01hr;
	for (i = 0; i < precPvt->valDetPvs; i++, pDet++, pPvStat++) {
		pFbuff = addToPrev ? (float *)precPvt->dataBuffer : precPvt->detBufPtr[i].pFill;
		if (precPvt->acqDet[i] && (precPvt->detBufPtr[i].pFill != NULL)) {
			puserPvt = precPvt->caLinkStruct[i + D1_IN].puserPvt;
			if (puserPvt->nelem > 1) {
				nRequest = nReq = psscan->npts;
				if (*pPvStat == PV_OK) {
					if (puserPvt->dbAddrNv || puserPvt->useDynLinkAlways) {
						status |= recDynLinkGet(&precPvt->caLinkStruct[i + D1_IN],
									pFbuff, &nRequest, 0, 0, 0);
						if (sscanRecordDebug >= 5) {
							printf("%s:recDynLinkGet returned %ld, nRequest=%d\n",
								psscan->name, status, (int)nRequest);
						}
						if (nRequest < psscan->npts)
							for (j = nRequest; j < psscan->npts; j++) pFbuff[j] = 0;
					} else {
						status |= dbGet(puserPvt->pAddr, DBR_FLOAT, pFbuff,
								0, &nReq, NULL);
						if (nReq < psscan->npts)
							for (j = nReq; j < psscan->npts; j++) pFbuff[j] = 0;
					}
				} else {
					for (j = 0; j < psscan->npts; j++) pFbuff[j] = 0;
				}
				if (addToPrev) {
					/* validBuf indicates where previous sscan's data are stored */
					pf = (precPvt->validBuf == A_BUFFER) ? precPvt->detBufPtr[i].pBufA : 
						precPvt->detBufPtr[i].pBufB;
					for (j = 0; j < psscan->npts; j++) {
						precPvt->detBufPtr[i].pFill[j] = pFbuff[j] + pf[j];
					}
				}
			}
		}
	}

	psscan->udf = 0;
	if (psscan->acqt == sscanACQT_1D_ARRAY) {
		/*** scan record gets all points in one pass ***/
		psscan->cpt = psscan->npts;
	}
}

volatile int sscan_fit_smooth = 3;
volatile int sscan_test_fit = 0;

static void copyLastPoint(sscanRecord *psscan, long pointNumber, long copyTo)
{
	recPvtStruct	*precPvt = (recPvtStruct *) psscan->rpvt;
	long			i, j;
	detFields		*pDet;
	double			d;
	/*
	 * It turns out that medm plots the whole array, so for it to look
	 * right the remainder of the arrays will be filled with the last
	 * values. This will cause medm to plot the same point over and over
	 * again, but it will look correct
	 */
	/* Fill valid detector arrays with last value */
	if (copyTo == -1)
		copyTo = psscan->mpts;
	else
		copyTo = MIN(copyTo, psscan->mpts);
	pointNumber = MAX(0, pointNumber);
	pDet = (detFields *) & psscan->d01hr;
	for (i = 0; i < precPvt->valDetPvs; i++, pDet++) {
		if (precPvt->acqDet[i]) {
			d = precPvt->detBufPtr[i].pFill[pointNumber];
			for (j = pointNumber+1; j < copyTo; j++) {
				precPvt->detBufPtr[i].pFill[j] = d;
			}
		}
	}
	/* Fill in the readback arrays with last values.  (Make sure we at least do P1.) */
	for (i = 0; (i == 0) || (i < precPvt->valPosPvs); i++) {
		d = precPvt->posBufPtr[i].pFill[pointNumber];
		for (j = pointNumber+1; j < copyTo; j++) {
			precPvt->posBufPtr[i].pFill[j] = d;
		}
	}

}


static void 
packData(sscanRecord *psscan, int caller)
{

	recPvtStruct	*precPvt = (recPvtStruct *) psscan->rpvt;
	long			i, j, markIndex, found;
	double			highVal, lowVal, aveDiff;
	int 			highIndex, lowIndex, moveToRef=0;
	detFields		*pDet;
	posFields		*pPos;
	double			d, *pPBuf, sum1, sum2;
	float			*pDBuf, *pf, *pf1, *pf2;
	unsigned short	*pPvStat;

	if (sscanRecordDebug >= 1) printf("%s:packData, caller=%d, faze='%s', data_state='%s'\n",
		psscan->name, caller, sscanFAZE_strings[psscan->faze], sscanDSTATE_strings[psscan->dstate]);

	if (psscan->dstate == sscanDSTATE_PACKED) return;

	switch ((int)psscan->dstate) {
	case sscanDSTATE_UNPACKED:
		/* scan completed or aborted; haven't triggered array read yet */
		if (precPvt->valATrigPvs) {
			psscan->dstate = sscanDSTATE_TRIG_ARRAY_READ; POST(&psscan->dstate);
			callbackRequest(&precPvt->doPutsCallback);
			return;
		}
		psscan->dstate = sscanDSTATE_ARRAY_READ_WAIT;
		/* fall through; we don't have to wait before reading arrays */

	case sscanDSTATE_ARRAY_READ_WAIT:
		/* array-read triggers (if any) have completed, but data may not be here yet */
	case sscanDSTATE_RECORD_ARRAY_DATA:
		/* remote array gets have completed, the data is now in recDynLink buffers */
		readArrays(psscan);
		if (psscan->dstate == sscanDSTATE_ARRAY_GET_CALLBACK_WAIT) {
			/* Wait for callbacks to arrive.  When they do, process() should send us back here. */
			return;
		}
		if (psscan->await) {
			/* can't pack; saveData's still using last scan's data buffers */
			psscan->dstate = sscanDSTATE_SAVE_DATA_WAIT; POST(&psscan->dstate);
			/* wait for saveData to write to AWAIT */
			return;
		}
		/* fall through; data-storage client is done with last scan's buffers */
	case sscanDSTATE_SAVE_DATA_WAIT:
		if (psscan->await) {
			/* wait for saveData to write to AWAIT */
			return;
		}
		/* saveData's done, we can pack the data now */
		break;
	default:
		printf("%s:packData: unexpected dstate (%d)\n", psscan->name, psscan->dstate);
		break;
	}

	psscan->dstate = sscanDSTATE_PACKED; POST(&psscan->dstate);

	copyLastPoint(psscan, psscan->cpt-1, -1);

	/* check after-scan move-to-data-feature before trying to do it */
	if (psscan->pasm >= sscanPASM_Peak_Pos) {
		int pos_ok=0;
		/* Do we have enough information to do this? */
		pPvStat = &psscan->p1nv;
		for (i=0; i<NUM_POS && pos_ok==0; i++, pPvStat++) {
			if (*pPvStat == PV_OK) pos_ok = 1;
		}
		moveToRef = pos_ok && (psscan->cpt > 1) && precPvt->acqDet[psscan->refd - 1];
		if (!moveToRef) {
			sprintf(psscan->smsg, "Can't move to %s ", sscanPASM_strings[psscan->pasm]);
			POST(&psscan->smsg);
			psscan->alrt = 1; POST(&psscan->alrt);
		}
	}

	/* after-scan move to some feature in reference-detector data */
	if (moveToRef) {
		if (sscanRecordDebug >= 5) printf("%s:packData cpt=%ld, pasm='%s'\n",
			psscan->name, (long)psscan->cpt, sscanPASM_strings[psscan->pasm]);
		/* Find peak/valley/edge in reference detector data array and go to it. */
		markIndex = -1;

		/* First, identify a valid positioner */
		pPBuf=NULL;
		pPvStat = &psscan->p1nv;
		for (i=0; i<NUM_POS && pPBuf==NULL; i++, pPvStat++) {
			if (*pPvStat == PV_OK) pPBuf = precPvt->posBufPtr[i].pFill;
		}

		/* Make sure reference detector is valid */
		pDet = ((detFields *) &psscan->d01hr) + (psscan->refd - 1);
		pDBuf = precPvt->detBufPtr[psscan->refd - 1].pFill;

		/* copy detector data to spare array, and (optionally) smooth it */
		pf = precPvt->nullArray;
		for (i = 0; i < psscan->cpt; i++) pf[i] = pDBuf[i];
		if (sscan_fit_smooth) {
			pf1 = pf;
			pf2 = precPvt->nullArray2;
			for (j=0; j<=sscan_fit_smooth; j++) {
				for (i = 0; i < psscan->cpt; i++) {
					pf2[i] = pf1[i];
					if (sscan_fit_smooth && ((i > 0) && (i < psscan->cpt-1))) {
						pf2[i] = pf1[i-1]/4 + pf1[i]/2 + pf1[i+1]/4;
					}
				}
				pf = pf2; pf2 = pf1; pf1 = pf;	/* swap for next run through data */
			}
		}
		pDBuf = pf;
		pf1 = (pf == precPvt->nullArray) ? precPvt->nullArray2 : precPvt->nullArray; 

		if (sscan_test_fit) {
			for (i = 0; i < psscan->cpt; i++) precPvt->detBufPtr[1].pFill[i] = pf[i];
			db_post_events(psscan, precPvt->detBufPtr[1].pBufA, DBE_VALUE);
			db_post_events(psscan, precPvt->detBufPtr[1].pBufB, DBE_VALUE);
		}

		found = 0;
		if (pPBuf && pDBuf) {
			switch (psscan->pasm) {
			default:
				break;

			case sscanPASM_RisingEdge_Pos:
			case sscanPASM_FallingEdge_Pos:
				/* calc derivative of detector data, fall through to find peak */
				for (i = 0; i < psscan->cpt; i++) pf1[i] = pDBuf[i];
				for (i = 1; i < psscan->cpt-1; i++) {
					d = pPBuf[i+1] - pPBuf[i-1];
					if (fabs(d) < 1.e-6) d = d<0.0 ? -1.0e-6 : 1.0e-6;
					pf1[i] = (pDBuf[i+1] - pDBuf[i-1]) / d;
				}
				pf1[i] = pf1[i-1];
				pf1[0] = pf1[1];
				pDBuf = pf1;

				if (sscan_test_fit) {
					for (i = 0; i < psscan->cpt; i++)
						precPvt->detBufPtr[2].pFill[i] = pDBuf[i];
					db_post_events(psscan, precPvt->detBufPtr[2].pBufA, DBE_VALUE);
					db_post_events(psscan, precPvt->detBufPtr[2].pBufB, DBE_VALUE);
				}
				/* fall through */

			case sscanPASM_Peak_Pos:
			case sscanPASM_Valley_Pos:

				/* find average positive difference between ref-detector values of adjacent points */
				/* find high and low ref-detector values, and the data points at which they occurred */
				aveDiff = 0;
				highVal = -HUGE_VAL;
				lowVal = HUGE_VAL;
				highIndex = lowIndex = 0; /* Make compiler happy.  These WILL be init'ed before use. */
				for (i = 1; i < psscan->cpt; i++) {
					aveDiff += fabs(pDBuf[i] - pDBuf[i-1]);
					if (pDBuf[i] > highVal) {
						highVal = pDBuf[i];
						highIndex = i;
					}
					if (pDBuf[i] < lowVal) {
						lowVal = pDBuf[i];
						lowIndex = i;
					}
				}
				aveDiff /= MAX(1,(psscan->cpt - 1));

				/* did we find an acceptable max or min? */
				if ((highVal - lowVal) > 2*aveDiff) {
					/* ...yes */
					if ((psscan->pasm == sscanPASM_Peak_Pos) ||
						(psscan->pasm == sscanPASM_RisingEdge_Pos)) {
						markIndex = highIndex;
					} else {
						markIndex = lowIndex;
					}
				}

				if ((markIndex >= 0)  && (markIndex < (psscan->cpt))) found = 1;

				pPos = (posFields *) & psscan->p1pp;
				for (i = 0; i < precPvt->valPosPvs; i++, pPos++) {
					pPBuf = precPvt->posBufPtr[i].pFill;
					pPos->p_dv = found ? pPBuf[markIndex] : pPos->p_pp;
				}
				break;

			case sscanPASM_COM:
				/* X value(s) of the 'center of mass' of the curve Y(X) */
				/* Do for all valid positioners */
				pPBuf = NULL;
				pPvStat = &psscan->p1nv;
				pPos = (posFields *) &psscan->p1pp;
				for (j=0; j<NUM_POS; j++, pPvStat++, pPos++) {
					if (*pPvStat == PV_OK) {
						pPBuf = precPvt->posBufPtr[j].pFill;
						for (i = 0; i < precPvt->valPosPvs; i++, pPos++) {
							sum1 = 0;  /* integral{X[1]..X[N]}{ X Y dX } */
							sum2 = 0;  /* integral{X[1]..X[N]}{ Y dX } */
							for (i = 1; i < psscan->cpt; i++) {
								d = pPBuf[i] - pPBuf[i-1];
								if (fabs(d) < 1.e-6) d = d<0.0 ? -1.0e-6 : 1.0e-6;
								sum1 += ((pPBuf[i] + pPBuf[i-1])/2) * ((pDBuf[i] + pDBuf[i-1])/2) * d;
								sum2 += ((pDBuf[i] + pDBuf[i-1])/2) * d;
							}
							if (fabs(sum2) > 1.e-6) {
								found = 1;
								pPos->p_dv = sum1 / sum2 ;
							} else {
								pPos->p_dv = pPos->p_pp;
							}
						}
					}
				}
				break;	
			}
		}

		/* tell user what happened */
		for (i=0, pf=precPvt->nullArray; i < psscan->cpt; i++) pf[i] = 0.;

		if (found) {
			sprintf(psscan->smsg, "%s found.", sscanPASM_strings[psscan->pasm]);
			POST(&psscan->smsg);
		} else {
			sprintf(psscan->smsg, "%s NOT found.", sscanPASM_strings[psscan->pasm]);
			POST(&psscan->smsg);
			psscan->alrt = 1; POST(&psscan->alrt);
		}
	}

	/* Switch validBuf flag and fill pointers */
	if (precPvt->validBuf == A_BUFFER) {
/* if (strcmp(psscan->name, "xxx:scanH")==0) printf("---packData: validBuf is now B---\n"); */
		precPvt->validBuf = B_BUFFER;
		for (i = 0; i < NUM_POS; i++) {
			precPvt->posBufPtr[i].pFill = precPvt->posBufPtr[i].pBufA;
		}
		for (i = 0; i < NUM_DET; i++) {
			precPvt->detBufPtr[i].pFill = precPvt->detBufPtr[i].pBufA;
		}
	} else {
		precPvt->validBuf = A_BUFFER;
/* if (strcmp(psscan->name, "xxx:scanH")==0) printf("---packData: validBuf is now A---\n"); */
		for (i = 0; i < NUM_POS; i++) {
			precPvt->posBufPtr[i].pFill = precPvt->posBufPtr[i].pBufB;
		}
		for (i = 0; i < NUM_DET; i++) {
			precPvt->detBufPtr[i].pFill = precPvt->detBufPtr[i].pBufB;
		}
	}
	/* save acquisition mode so we know whether to add next sscan's data to this scan */
	precPvt->prevACQM = psscan->acqm;
	precPvt->prevNumPts = psscan->cpt;
	return;
}


static void 
afterScan(psscan)
	sscanRecord *psscan;
{
	recPvtStruct   *precPvt = (recPvtStruct *) psscan->rpvt;

	/* See if there is an After Scan Link to process */

	if (psscan->asnv == PV_OK) {
		psscan->faze = sscanFAZE_AFTER_SCAN_DO; POST(&psscan->faze);
		/* request callback to do dbPutFields */
		callbackRequest(&precPvt->doPutsCallback);
	} else {
		psscan->faze = sscanFAZE_SCAN_DONE; POST(&psscan->faze);
	}
	return;
}

/****************************************************************************
 *
 * This is the code that is executed to initiate the next
 * step in the scan or trigger detectors (does the "puts" to the
 * reassignable links). Since the recDynLinkPut request is actually
 * done with a separate task, one need not worry about lock sets.
 *
 ***************************************************************************/
void 
doPuts(CALLBACK *pCB)
{
	sscanRecord		*psscan;
	recPvtStruct	*precPvt;
	posFields		*pPos;
	unsigned short	*pPvStat;
	int             i, numPutCallbacks;
	long			status;
	double			oldPos;
	float			*tcd;
	int				linkIndex;

	callbackGetUser(psscan, pCB);
	precPvt = psscan->rpvt;

	if (sscanRecordDebug >= 2)
		printf("%s:doPuts:entry:faze='%s'\n", psscan->name, sscanFAZE_strings[psscan->faze]);

	if (psscan->paus) {
		sprintf(psscan->smsg, "Scan paused by operator");
		POST(&psscan->smsg);
		return;
	}

	/* We don't want TRIG_ARRAY_READ to be a scan->faze item, because that
	 * would close off the possibility of doing array read and retrace at
	 * the same time.
	 */
	if (psscan->dstate == sscanDSTATE_TRIG_ARRAY_READ) {
		if (sscanRecordDebug >= 5) {
			printf("%s:doPuts:TRIG_ARRAY_READ - Point %ld\n", psscan->name, (long)psscan->cpt);
		}
		psscan->dstate = sscanDSTATE_ARRAY_READ_WAIT; POST(&psscan->dstate);

		/* for each valid array-read trigger, write the desired value */
		tcd = &psscan->a1cd;	/* value to write */
		pPvStat = &psscan->a1nv;	/* link status */
		linkIndex = A1_OUT;
		for (i=0; i<NUM_ATRGS; i++, tcd++, pPvStat++, linkIndex++) {
			if (*pPvStat == 0) {
				epicsMutexLock(precPvt->numCallbacksSem);
				precPvt->numAReadCallbacks++;
				epicsMutexUnlock(precPvt->numCallbacksSem);
				status = recDynLinkPutCallback(&precPvt->caLinkStruct[linkIndex],
						tcd, 1, notifyCallback);
				if (status) {
					epicsMutexLock(precPvt->numCallbacksSem);
					precPvt->numAReadCallbacks--;
					epicsMutexUnlock(precPvt->numCallbacksSem);
				}
				if (status == NOTIFY_IN_PROGRESS) {
					epicsMutexLock(precPvt->numCallbacksSem);
					precPvt->numAReadCallbacks--;
					epicsMutexUnlock(precPvt->numCallbacksSem);
					if (sscanRecordDebug >= 5) {
						printf("%s:doPuts:...TRIG_ARRAY_READ: notify in progress\n", psscan->name);
					}
					psscan->alrt = NOTIFY_IN_PROGRESS; POST(&psscan->alrt);
					sprintf(psscan->smsg, "Array-read trigger %d is busy", i+1);
					POST(&psscan->smsg);
				}
			}
		}
		return;
	}


	switch ((int)psscan->faze) {

	case sscanFAZE_BEFORE_SCAN:
		if (sscanRecordDebug >= 5)
			printf("%s:doPuts:BEFORE_SCAN Link\n", psscan->name);
		if (psscan->bsnv == OK) {
			if (psscan->bswait == sscanLINKWAIT_YES) {
				psscan->faze = sscanFAZE_BEFORE_SCAN_WAIT; POST(&psscan->faze);
				epicsMutexLock(precPvt->numCallbacksSem);
				precPvt->numPositionerCallbacks++;
				epicsMutexUnlock(precPvt->numCallbacksSem);
				status = recDynLinkPutCallback(&precPvt->caLinkStruct[BS_OUT],
						&(psscan->bscd), 1, notifyCallback);
				if (status) {
					epicsMutexLock(precPvt->numCallbacksSem);
					precPvt->numPositionerCallbacks--;
					epicsMutexUnlock(precPvt->numCallbacksSem);
					if (status == NOTIFY_IN_PROGRESS) {
						psscan->alrt = NOTIFY_IN_PROGRESS; POST(&psscan->alrt);
						sprintf(psscan->smsg, "Before-scan link is busy");
						POST(&psscan->smsg);
					}
				}
			} else {
				status = recDynLinkPut(&precPvt->caLinkStruct[BS_OUT],
						&(psscan->bscd), 1);
			}
		}

		epicsMutexLock(precPvt->numCallbacksSem);
		if (precPvt->numPositionerCallbacks) {
			epicsMutexUnlock(precPvt->numCallbacksSem);
			/* wait for callback */
			return;
		} else {
			/* Fall through to MOVE_MOTORS if we didn't do a putCallback. */
			psscan->faze = sscanFAZE_MOVE_MOTORS;
		}
		epicsMutexUnlock(precPvt->numCallbacksSem);

	case sscanFAZE_START_FLY:
	case sscanFAZE_MOVE_MOTORS:
		if (sscanRecordDebug >= 5) {
			printf("%s:doPuts:MOVE_MOTORS  - Point %ld\n", psscan->name, (long)psscan->cpt);
		}

		pPos = (posFields *) & psscan->p1pp;
		pPvStat = &psscan->p1nv;

		/*
		 * Before we start, preset numPositionerCallbacks, so notifyCallback can't possibly
		 * decrement to zero (and think all positioners are done) before we've started them all.
		 */		
		epicsMutexLock(precPvt->numCallbacksSem);
		precPvt->numPositionerCallbacks = 1;
		epicsMutexUnlock(precPvt->numCallbacksSem);

		/* For each valid positioner, write the desired position.
		 * If positioner is "on-the-fly", move motors only if the current point is 0:
		 * do a PutCallback on move-to-start-point but just a Put on start-fly.
		 */
		for (i = 0; i < precPvt->valPosPvs; i++, pPos++, pPvStat++) {
			if ((*pPvStat == PV_OK) && ((pPos->p_sm != sscanP1SM_On_The_Fly) || (psscan->cpt == 0))) {
				if ((psscan->faze == sscanFAZE_START_FLY) && (pPos->p_sm == sscanP1SM_On_The_Fly)) {
					status = recDynLinkPut(&precPvt->caLinkStruct[i + P1_OUT], &(pPos->p_dv), 1);
					if (sscanRecordDebug >= 5)
						printf("%s:doPuts:start_fly to %f\n", psscan->name, pPos->p_dv);
					precPvt->flying = 1;
				} else {
					epicsMutexLock(precPvt->numCallbacksSem);
					precPvt->numPositionerCallbacks++;
					epicsMutexUnlock(precPvt->numCallbacksSem);
					psscan->faze = sscanFAZE_CHECK_MOTORS; /* post when we get out of the loop */
					status = recDynLinkPutCallback(&precPvt->caLinkStruct[i + P1_OUT],
						    &(pPos->p_dv), 1, notifyCallback);
					if (status) {
						epicsMutexLock(precPvt->numCallbacksSem);
						precPvt->numPositionerCallbacks--;
						epicsMutexUnlock(precPvt->numCallbacksSem);
						if (status == NOTIFY_IN_PROGRESS) {
							psscan->alrt = NOTIFY_IN_PROGRESS; POST(&psscan->alrt);
							sprintf(psscan->smsg, "Positioner %1d is already busy", i);
							POST(&psscan->smsg);
						}
					}
				}
			}
		}

		epicsMutexLock(precPvt->numCallbacksSem);
		--precPvt->numPositionerCallbacks; /* Take out that preset we did before writing to positioners */
		if (precPvt->numPositionerCallbacks > 0) {
			epicsMutexUnlock(precPvt->numCallbacksSem);
			POST(&psscan->faze);
			return;
		}
		epicsMutexUnlock(precPvt->numCallbacksSem);


		/*
		 * Fall through to TRIG_DETCTRS if we did no putCallbacks
		 * (i.e., in onTheFly mode and flying, or no positioners to move,
		 * or no putCallbacks succeeded)
		 */

	case sscanFAZE_TRIG_DETCTRS:
		if (sscanRecordDebug >= 5) {
			printf("%s:doPuts:TRIG_DETCTRS - Point %ld\n", psscan->name, (long)psscan->cpt);
		}
		psscan->faze = precPvt->onTheFly ? sscanFAZE_CHECK_MOTORS : sscanFAZE_READ_DETCTRS;
		POST(&psscan->faze);

		if (psscan->awct) {
			psscan->wcnt = psscan->awct; POST(&psscan->wcnt);
		}

		if (precPvt->valTrigPvs == 0) {
			/*
			 * No valid trigger PV's.  Scan will hang unless we
			 * cause the record to process.  (We get here if
			 * there are no positioners.)
			 */
			precPvt->calledBy = DO_PUTS;
			if (psscan->ddly == 0.) {
				scanOnce((struct dbCommon *)psscan);
			} else {
				callbackRequestDelayed(&precPvt->dlyCallback, psscan->ddly);
			}
			return;
		}

		/* for each valid detector trigger, write the desired value */

		/*
		 * Before we start, preset numTriggerCallbacks, so notifyCallback can't possibly
		 * decrement to zero (and think all triggers are done) before we've started them all.
		 */		
		epicsMutexLock(precPvt->numCallbacksSem);
		precPvt->numTriggerCallbacks = 1;
		epicsMutexUnlock(precPvt->numCallbacksSem);

		tcd = &psscan->t1cd;	/* value to write */
		pPvStat = &psscan->t1nv;	/* link status */
		linkIndex = T1_OUT;
		for (i=0; i<NUM_TRGS; i++, tcd++, pPvStat++, linkIndex++) {
			if (*pPvStat == 0) {
				epicsMutexLock(precPvt->numCallbacksSem);
				precPvt->numTriggerCallbacks++;
				epicsMutexUnlock(precPvt->numCallbacksSem);
				status = recDynLinkPutCallback(&precPvt->caLinkStruct[linkIndex],
						tcd, 1, notifyCallback);
				if (status) {
					epicsMutexLock(precPvt->numCallbacksSem);
					precPvt->numTriggerCallbacks--;
					epicsMutexUnlock(precPvt->numCallbacksSem);
					if (sscanRecordDebug >= 5) {
						printf("%s:doPuts:...TRIG_DETCTRS: notify in progress\n", psscan->name);
					}
					psscan->alrt = NOTIFY_IN_PROGRESS; POST(&psscan->alrt);
					sprintf(psscan->smsg, "Detector %d is busy", i+1);
					POST(&psscan->smsg);
				}
			}
		}

		epicsMutexLock(precPvt->numCallbacksSem);
		--precPvt->numTriggerCallbacks; /* Take out that preset we did before writing to triggers */
		if (precPvt->numTriggerCallbacks > 0) {
			/* There will be a callback from the trigger, so we can leave. */
			epicsMutexUnlock(precPvt->numCallbacksSem);
			return;
		}
		epicsMutexUnlock(precPvt->numCallbacksSem);

		/* We reach this point only if all trigger callbacks came in while our preset to
		 * numTriggerCallbacks was preventing notifyCallback from scheduling the next step
		 * of the scan.  In this case, we have to do what notifyCallback would have done
		 * if it had decremented numTriggerCallbacks to 0.
		 */ 
		if (psscan->paus) {
			sprintf(psscan->smsg, "Scan paused by operator");
			POST(&psscan->smsg);
			return;
		}
		if (psscan->ddly < .001) {
			if (psscan->wcnt) {
				psscan->wtng = 1; POST(&psscan->wtng);
				sprintf(psscan->smsg, "Waiting for client");
				POST(&psscan->smsg);
			} else {
				precPvt->calledBy = DO_PUTS_TRIG;
				(void) scanOnce((struct dbCommon *)psscan);
			}
		} else {
			precPvt->calledBy = DO_PUTS_TRIG;
			callbackRequestDelayed(&precPvt->dlyCallback, psscan->ddly);
		}
		return;

	case sscanFAZE_RETRACE_MOVE:
		/* If Retrace indicates motors must move ... */

		/*
		 * Before we start, preset numPositionerCallbacks, so notifyCallback can't possibly
		 * decrement to zero (and think all positioners are done) before we've started them all.
		 */		
		epicsMutexLock(precPvt->numCallbacksSem);
		precPvt->numPositionerCallbacks = 1;
		epicsMutexUnlock(precPvt->numCallbacksSem);

		if (psscan->pasm) {
			if (sscanRecordDebug >= 5) {printf("%s:doPuts:RETRACE\n", psscan->name);}
			pPos = (posFields *) & psscan->p1pp;
			pPvStat = &psscan->p1nv;
			for (i = 0; i < precPvt->valPosPvs; i++, pPos++, pPvStat++) {
				if (*pPvStat == PV_OK) {
					/* Figure out where to go ... */
					oldPos = pPos->p_dv;
					switch (psscan->pasm) {
					default:
					case sscanPASM_Prior_Pos:
						pPos->p_dv = pPos->p_pp;
						break;
					case sscanPASM_Start_Pos:
						pPos->p_dv = (pPos->p_sm == sscanP1SM_Table) ? pPos->p_pa[0] : pPos->p_sp;
						if (pPos->p_ar) pPos->p_dv += pPos->p_pp;
						break;
					case sscanPASM_Peak_Pos:
					case sscanPASM_Valley_Pos:
					case sscanPASM_RisingEdge_Pos:
					case sscanPASM_FallingEdge_Pos:
					case sscanPASM_COM:
						/* packData() has already set pPos->p_dv for us */
						break;
					}
					if (pPos->p_dv == oldPos) pPos->p_dv *= (1 + DBL_EPSILON);

					psscan->faze = sscanFAZE_RETRACE_WAIT; POST(&psscan->faze);
					/* Command motor */
					epicsMutexLock(precPvt->numCallbacksSem);
					precPvt->numPositionerCallbacks++;
					epicsMutexUnlock(precPvt->numCallbacksSem);
					status = recDynLinkPutCallback(&precPvt->caLinkStruct[i + P1_OUT],
					    &(pPos->p_dv), 1, notifyCallback);
					if (status) {
						/* nonzero return means we failed to tell the motor to move */
						epicsMutexLock(precPvt->numCallbacksSem);
						precPvt->numPositionerCallbacks--;
						epicsMutexUnlock(precPvt->numCallbacksSem);
						psscan->alrt = 1; POST(&psscan->alrt);
						sprintf(psscan->smsg, "Can't retrace positioner %1d", i);
						POST(&psscan->smsg);
					}
				}
			}
		}

		epicsMutexLock(precPvt->numCallbacksSem);
		--precPvt->numPositionerCallbacks; /* Take out that preset we did before writing to positioners*/
		if (precPvt->numPositionerCallbacks > 0) {
			epicsMutexUnlock(precPvt->numCallbacksSem);
			return;
		} else {
			epicsMutexUnlock(precPvt->numCallbacksSem);
			psscan->faze = sscanFAZE_AFTER_SCAN_DO; POST(&psscan->faze);
			/* Fall through to sscanFAZE_AFTER_SCAN_DO */
		}

	case sscanFAZE_AFTER_SCAN_DO:
		/* If an After Scan Link PV is valid, execute it */
		numPutCallbacks = 0;
		if (psscan->asnv == PV_OK) {
			if (sscanRecordDebug >= 5)
				printf("%s:doPuts:AFTER_SCAN Fwd Lnk\n", psscan->name);
			if (psscan->aswait == sscanLINKWAIT_YES) {
				psscan->faze = sscanFAZE_AFTER_SCAN_WAIT; POST(&psscan->faze);
				epicsMutexLock(precPvt->numCallbacksSem);
				precPvt->numPositionerCallbacks++;
				epicsMutexUnlock(precPvt->numCallbacksSem);
				status = recDynLinkPutCallback(&precPvt->caLinkStruct[AS_OUT],
						&(psscan->ascd), 1, notifyCallback);
				if (status) {
					epicsMutexLock(precPvt->numCallbacksSem);
					precPvt->numPositionerCallbacks--;
					epicsMutexUnlock(precPvt->numCallbacksSem);
					psscan->alrt = 1; POST(&psscan->alrt);
					sprintf(psscan->smsg, "Can't fire After-scan link");
					POST(&psscan->smsg);
				} else {
					numPutCallbacks++;
				}
			} else {
				status = recDynLinkPut(&precPvt->caLinkStruct[AS_OUT],
						&(psscan->ascd), 1);
			}
		}
		if (numPutCallbacks == 0) {
			/* Don't have to wait for after-scan link callback */
			psscan->faze = sscanFAZE_SCAN_DONE; POST(&psscan->faze);
			/* Scan must end in the process() routine. */
			precPvt->calledBy = DO_PUTS;
			scanOnce((struct dbCommon *)psscan);
		}
		return;

	case sscanFAZE_IDLE:
	case sscanFAZE_PREVIEW:
		/*
		 * This must be a request to move a positioner interactively.
		 * We don't do this anymore.
		 */

		return;

	case sscanFAZE_SCAN_PENDING:
		printf("%s:doPuts:SCAN_PENDING -- we shouldn't be here.\n", psscan->name);
		return;

	default:
		printf("%s:doPuts:default -- we shouldn't be here.\n", psscan->name);
		return;
	}
}




/* routine to resolve and adjust linear scan definition.
 * Might get complicated !!!
 */

static void 
adjLinParms(paddr)
	struct dbAddr  *paddr;
{
	sscanRecord *psscan = (sscanRecord *) (paddr->precord);
	recPvtStruct   *precPvt = (recPvtStruct *) psscan->rpvt;
	struct posFields *pParms = (posFields *) & psscan->p1pp;

	int             special_type = paddr->special;
	int             i;
	int             foundOne = 0;

	/* determine which positioner */
	for (i = 0; i < NUM_POS; i++, pParms++) {
		if ((paddr->pfield >= (void *) &pParms->p_pp) &&
		    (paddr->pfield <= (void *) &pParms->p_pr)) {

			foundOne = 1;
			break;
		}
	}
	if (!foundOne)
		return;		/* couldn't determine positioner */

	if (pParms->p_sm == sscanP1SM_Table) {
		/* if positioner is in table mode, zero parms and return */
		zeroPosParms(psscan, (unsigned short) i);
		sprintf(psscan->smsg, "Positioner #%1d is in Table Mode !", i + 1);
		psscan->alrt = 1;
		return;
	}
	if (sscanRecordDebug >= 2)
		printf("%s:Positioner %d\n", psscan->name, i);
	switch (special_type) {
	case (SPC_SC_S):	/* start position changed */
		/* if step increment/center/width are not frozen, change them  */
		if (!pParms->p_fi && !pParms->p_fc && !pParms->p_fw) {
			/* avoid div by zero */
			pParms->p_si = (pParms->p_ep - pParms->p_sp) / MAX(1,(psscan->npts - 1));
			POST(&pParms->p_si);
			pParms->p_cp = (pParms->p_ep + pParms->p_sp) / 2;
			POST(&pParms->p_cp);
			pParms->p_wd = (pParms->p_ep - pParms->p_sp);
			POST(&pParms->p_wd);
			return;
			/* If npts/center/width aren't frozen, change them */
		} else if (!psscan->fpts && !pParms->p_fc && !pParms->p_fw) {
			if (fabs(pParms->p_si) <= DBL_EPSILON) {
				psscan->npts = psscan->mpts;
			} else {
				psscan->npts = ((pParms->p_ep - pParms->p_sp) / pParms->p_si) + 1;
			}
			if (psscan->npts > psscan->mpts) {
				psscan->npts = psscan->mpts;
				sprintf(psscan->smsg, "P%1d Request Exceeded Maximum Points!", i + 1);
				/* adjust changed field to be consistent */
				pParms->p_sp = pParms->p_ep - (pParms->p_si * (psscan->npts - 1));
				POST(&pParms->p_sp);
				psscan->alrt = 1;
			}
			POST(&psscan->npts);
			pParms->p_cp = (pParms->p_ep + pParms->p_sp) / 2;
			POST(&pParms->p_cp);
			pParms->p_wd = (pParms->p_ep - pParms->p_sp);
			POST(&pParms->p_wd);
			precPvt->nptsCause = i;	/* indicate cause of # of points changed */
			changedNpts(psscan);
			return;
			/* if end/center are not frozen, change them  */
		} else if (!pParms->p_fe && !pParms->p_fc) {
			pParms->p_ep = pParms->p_sp + (MAX(1,(psscan->npts - 1)) * pParms->p_si);
			POST(&pParms->p_ep);
			pParms->p_cp = (pParms->p_ep + pParms->p_sp) / 2;
			POST(&pParms->p_cp);
			return;
			/* if step increment/end/width are not frozen, change them */
		} else if (!pParms->p_fi && !pParms->p_fe && !pParms->p_fw) {
			pParms->p_wd = (pParms->p_cp - pParms->p_sp) * 2;
			POST(&pParms->p_wd);
			/* avoid div by zero */
			pParms->p_si = pParms->p_wd / MAX(1,(psscan->npts - 1));
			POST(&pParms->p_si);
			pParms->p_ep = (pParms->p_sp + pParms->p_wd);
			POST(&pParms->p_ep);
			return;
			/* If npts/end/width aren't frozen, change them */
		} else if (!psscan->fpts && !pParms->p_fe && !pParms->p_fw) {
			if (fabs(pParms->p_si) <= DBL_EPSILON) {
				psscan->npts = psscan->mpts;
			} else {
				psscan->npts = ((pParms->p_cp - pParms->p_sp) * 2 / pParms->p_si) + 1;
			}
			if (psscan->npts > psscan->mpts) {
				psscan->npts = psscan->mpts;
				sprintf(psscan->smsg, "P%1d Request Exceeded Maximum Points!", i + 1);
				/* adjust changed field to be consistent */
				pParms->p_sp = pParms->p_cp - ((pParms->p_si * MAX(1,(psscan->npts - 1))) / 2);
				POST(&pParms->p_sp);
				psscan->alrt = 1;
			}
			POST(&psscan->npts);
			pParms->p_ep = pParms->p_sp + (MAX(1,(psscan->npts - 1)) * pParms->p_si);
			POST(&pParms->p_cp);
			pParms->p_wd = (pParms->p_ep - pParms->p_sp);
			POST(&pParms->p_wd);
			precPvt->nptsCause = i;	/* indicate cause of # of points changed */
			changedNpts(psscan);
			return;
		}
		break;

	case (SPC_SC_I):	/* step increment changed */
		/* if npts is not frozen, change it  */
		if (!psscan->fpts) {
			if (fabs(pParms->p_si) <= DBL_EPSILON) {
				psscan->npts = psscan->mpts;
			} else {
				if ((pParms->p_si < 0) != (pParms->p_ep < pParms->p_sp)) {
					if (sscanRecordDebug >= 2) printf("%s:Positioner %d; step dir != scan dir\n", psscan->name, i);
					/* step increment goes one way, end-start goes the other */ 
					if (!pParms->p_fe && !pParms->p_fs && !pParms->p_fw) {
						/* swap start and end */
						double t = pParms->p_ep;
						pParms->p_ep = pParms->p_sp; POST(&pParms->p_ep);
						pParms->p_sp = t; POST(&pParms->p_sp);
						pParms->p_wd = pParms->p_ep - pParms->p_sp; POST(&pParms->p_wd);
					} else {
						pParms->p_si = (pParms->p_ep - pParms->p_sp) / MAX(1,(psscan->npts - 1));
						POST(&pParms->p_si);
					}
				}
				psscan->npts = ((pParms->p_ep - pParms->p_sp) / pParms->p_si) + 1;
			}
			if (psscan->npts > psscan->mpts) {
				psscan->npts = psscan->mpts;
				sprintf(psscan->smsg, "P%1d Request Exceeded Maximum Points !", i + 1);
				/* adjust changed field to be consistent, avoid div by zero  */
				pParms->p_si = (pParms->p_ep - pParms->p_sp) / MAX(1,(psscan->npts - 1));
				POST(&pParms->p_si);
				psscan->alrt = 1;
			}
			POST(&psscan->npts);
			precPvt->nptsCause = i;	/* indicate cause of # of points changed */
			changedNpts(psscan);
			return;
			/* if end/center/width are not frozen, change them */
		} else if (!pParms->p_fe && !pParms->p_fc && !pParms->p_fw) {
			pParms->p_ep = pParms->p_sp + (MAX(1,(psscan->npts - 1)) * pParms->p_si);
			POST(&pParms->p_ep);
			pParms->p_cp = (pParms->p_ep + pParms->p_sp) / 2;
			POST(&pParms->p_cp);
			pParms->p_wd = (pParms->p_ep - pParms->p_sp);
			POST(&pParms->p_wd);
			return;
			/* if start/center/width are not frozen, change them */
		} else if (!pParms->p_fs && !pParms->p_fc && !pParms->p_fw) {
			pParms->p_sp = pParms->p_ep - (MAX(1,(psscan->npts - 1)) * pParms->p_si);
			POST(&pParms->p_sp);
			pParms->p_cp = (pParms->p_ep + pParms->p_sp) / 2;
			POST(&pParms->p_cp);
			pParms->p_wd = (pParms->p_ep - pParms->p_sp);
			POST(&pParms->p_wd);
			return;
			/* if start/end/width are not frozen, change them */
		} else if (!pParms->p_fs && !pParms->p_fe && !pParms->p_fw) {
			pParms->p_sp = pParms->p_cp - (MAX(1,(psscan->npts - 1)) * pParms->p_si) / 2;
			POST(&pParms->p_sp);
			pParms->p_ep = pParms->p_sp + (MAX(1,(psscan->npts - 1)) * pParms->p_si);
			POST(&pParms->p_ep);
			pParms->p_wd = (pParms->p_ep - pParms->p_sp);
			POST(&pParms->p_wd);
			return;
		} else {	/* too constrained !! */
			pParms->p_si = (pParms->p_ep - pParms->p_sp) / MAX(1,(psscan->npts - 1));
			POST(&pParms->p_si);
			sprintf(psscan->smsg, "P%1d SCAN Parameters Too Constrained !", i + 1);
			psscan->alrt = 1;
			return;
		}
		break;

	case (SPC_SC_E):	/* end position changed   */
		/* if step increment/center/width are not frozen, change them */
		if (!pParms->p_fi && !pParms->p_fc && !pParms->p_fw) {
			pParms->p_si = (pParms->p_ep - pParms->p_sp) / MAX(1,(psscan->npts - 1));
			POST(&pParms->p_si);
			pParms->p_cp = (pParms->p_ep + pParms->p_sp) / 2;
			POST(&pParms->p_cp);
			pParms->p_wd = (pParms->p_ep - pParms->p_sp);
			POST(&pParms->p_wd);
			return;
			/* If npts/center/width are not frozen, change them */
		} else if (!psscan->fpts && !pParms->p_fc && !pParms->p_fw) {
			if (fabs(pParms->p_si) <= DBL_EPSILON) {
				psscan->npts = psscan->mpts;
			} else {
				psscan->npts = ((pParms->p_ep - pParms->p_sp) / pParms->p_si) + 1;
			}
			if (psscan->npts > psscan->mpts) {
				psscan->npts = psscan->mpts;
				sprintf(psscan->smsg, "P%1d Request Exceeded Maximum Points !", i + 1);
				/* adjust changed field to be consistent */
				pParms->p_ep = pParms->p_sp + (pParms->p_si * MAX(1,(psscan->npts - 1)));
				POST(&pParms->p_ep);
				psscan->alrt = 1;
			}
			POST(&psscan->npts);
			pParms->p_cp = (pParms->p_ep + pParms->p_sp) / 2;
			POST(&pParms->p_cp);
			pParms->p_wd = (pParms->p_ep - pParms->p_sp);
			POST(&pParms->p_wd);
			precPvt->nptsCause = i;	/* indicate cause of # of points changed */
			changedNpts(psscan);
			return;
			/* if start/center are not frozen, change them  */
		} else if (!pParms->p_fs && !pParms->p_fc) {
			pParms->p_sp = pParms->p_ep - (MAX(1,(psscan->npts - 1)) * pParms->p_si);
			POST(&pParms->p_sp);
			pParms->p_cp = (pParms->p_ep + pParms->p_sp) / 2;
			POST(&pParms->p_cp);
			return;
			/* If start/width/increment aren't frozen, change them */
		} else if (!pParms->p_fs && !pParms->p_fw && !pParms->p_fi) {
			pParms->p_wd = (pParms->p_ep - pParms->p_cp) * 2;
			POST(&pParms->p_wd);
			pParms->p_sp = pParms->p_ep - pParms->p_wd;
			POST(&pParms->p_sp);
			pParms->p_si = (pParms->p_ep - pParms->p_sp) / MAX(1,(psscan->npts - 1));
			POST(&pParms->p_si);
			return;
			/* If npts/start/width are not frozen, change them */
		} else if (!psscan->fpts && !pParms->p_fs && !pParms->p_fw) {
			if (fabs(pParms->p_si) <= DBL_EPSILON) {
				psscan->npts = psscan->mpts;
			} else {
				psscan->npts = (((pParms->p_ep - pParms->p_cp) * 2) / pParms->p_si) + 1;
			}
			if (psscan->npts > psscan->mpts) {
				psscan->npts = psscan->mpts;
				sprintf(psscan->smsg, "P%1d Request Exceeded Maximum Points !", i + 1);
				/* adjust changed field to be consistent */
				pParms->p_ep = pParms->p_cp + (pParms->p_si * MAX(1,(psscan->npts - 1)) / 2);
				POST(&pParms->p_ep);
				psscan->alrt = 1;
			}
			POST(&psscan->npts);
			pParms->p_wd = (pParms->p_ep - pParms->p_cp) * 2;
			POST(&pParms->p_wd);
			pParms->p_sp = pParms->p_ep - pParms->p_wd;
			POST(&pParms->p_sp);
			precPvt->nptsCause = i;	/* indicate cause of # of points changed */
			changedNpts(psscan);
			return;
		} else {	/* too constrained !! */
			pParms->p_ep = pParms->p_sp + (MAX(1,(psscan->npts - 1)) * pParms->p_si);
			POST(&pParms->p_ep);
			sprintf(psscan->smsg, "P%1d SCAN Parameters Too Constrained !", i + 1);
			psscan->alrt = 1;
			return;
		}
		break;

	case (SPC_SC_C):	/* center position changed   */
		/* if start/end are not frozen, change them */
		if (!pParms->p_fs && !pParms->p_fe) {
			pParms->p_sp = pParms->p_cp - (MAX(1,(psscan->npts - 1)) * pParms->p_si) / 2;
			POST(&pParms->p_sp);
			pParms->p_ep = pParms->p_sp + (MAX(1,(psscan->npts - 1)) * pParms->p_si);
			POST(&pParms->p_ep);
			return;
			/* if end/inc/width are not frozen, change them */
		} else if (!pParms->p_fe && !pParms->p_fi && !pParms->p_fw) {
			pParms->p_wd = (pParms->p_cp - pParms->p_sp) * 2;
			POST(&pParms->p_wd);
			pParms->p_ep = pParms->p_sp + pParms->p_wd;
			POST(&pParms->p_ep);
			pParms->p_si = (pParms->p_ep - pParms->p_sp) / MAX(1,(psscan->npts - 1));
			POST(&pParms->p_si);
			return;
			/* if start/inc/width are not frozen, change them */
		} else if (!pParms->p_fs && !pParms->p_fi && !pParms->p_fw) {
			pParms->p_wd = (pParms->p_ep - pParms->p_cp) * 2;
			POST(&pParms->p_wd);
			pParms->p_sp = pParms->p_ep - pParms->p_wd;
			POST(&pParms->p_sp);
			pParms->p_si = (pParms->p_ep - pParms->p_sp) / MAX(1,(psscan->npts - 1));
			POST(&pParms->p_si);
			return;
			/*
			 * if # of points/end/width are not frozen, change
			 * them
			 */
		} else if (!psscan->fpts && !pParms->p_fe && !pParms->p_fw) {
			if (fabs(pParms->p_si) <= DBL_EPSILON) {
				psscan->npts = psscan->mpts;
			} else {
				psscan->npts = (((pParms->p_cp - pParms->p_sp) * 2) / pParms->p_si) + 1;
			}
			if (psscan->npts > psscan->mpts) {
				psscan->npts = psscan->mpts;
				sprintf(psscan->smsg, "P%1d Request Exceeded Maximum Points !", i + 1);
				/* adjust changed field to be consistent */
				pParms->p_cp = pParms->p_sp + (pParms->p_si * MAX(1,(psscan->npts - 1)) / 2);
				POST(&pParms->p_cp);
				psscan->alrt = 1;
			}
			POST(&psscan->npts);
			pParms->p_wd = (pParms->p_cp - pParms->p_sp) * 2;
			POST(&pParms->p_wd);
			pParms->p_ep = pParms->p_sp + pParms->p_wd;
			POST(&pParms->p_ep);
			precPvt->nptsCause = i;	/* indicate cause of # of points changed */
			changedNpts(psscan);
			return;
			/* If npts/start/width are not frozen, change them */
		} else if (!psscan->fpts && !pParms->p_fs && !pParms->p_fw) {
			if (fabs(pParms->p_si) <= DBL_EPSILON) {
				psscan->npts = psscan->mpts;
			} else {
				psscan->npts = (((pParms->p_ep - pParms->p_cp) * 2) / pParms->p_si) + 1;
			}
			if (psscan->npts > psscan->mpts) {
				psscan->npts = psscan->mpts;
				sprintf(psscan->smsg, "P%1d Request Exceeded Maximum Points !", i + 1);
				/* adjust changed field to be consistent */
				pParms->p_cp = pParms->p_ep - (pParms->p_si * MAX(1,(psscan->npts - 1)) / 2);
				POST(&pParms->p_cp);
				psscan->alrt = 1;
			}
			POST(&psscan->npts);
			pParms->p_wd = (pParms->p_ep - pParms->p_cp) * 2;
			POST(&pParms->p_wd);
			pParms->p_sp = pParms->p_ep - pParms->p_wd;
			POST(&pParms->p_sp);
			precPvt->nptsCause = i;	/* indicate cause of # of points changed */
			changedNpts(psscan);
			return;
		} else {	/* too constrained !! */
			pParms->p_cp = pParms->p_sp + (MAX(1,(psscan->npts - 1)) * pParms->p_si) / 2;
			POST(&pParms->p_cp);
			sprintf(psscan->smsg, "P%1d SCAN Parameters Too Constrained !", i + 1);
			psscan->alrt = 1;
			return;
		}
		break;

	case (SPC_SC_W):	/* width position changed   */
		/* if step start/inc/end are not frozen, change them */
		if (!pParms->p_fs && !pParms->p_fi && !pParms->p_fe) {
			pParms->p_si = pParms->p_wd / MAX(1,(psscan->npts - 1));
			POST(&pParms->p_si);
			pParms->p_sp = pParms->p_cp - (MAX(1,(psscan->npts - 1)) * pParms->p_si) / 2;
			POST(&pParms->p_sp);
			pParms->p_ep = pParms->p_sp + (MAX(1,(psscan->npts - 1)) * pParms->p_si);
			POST(&pParms->p_ep);
			return;
			/* If npts/start/end are not frozen, change them */
		} else if (!psscan->fpts && !pParms->p_fs && !pParms->p_fe) {
			if (fabs(pParms->p_si) <= DBL_EPSILON) {
				psscan->npts = psscan->mpts;
			} else {
				psscan->npts = (pParms->p_wd / pParms->p_si) + 1;
			}
			if (psscan->npts > psscan->mpts) {
				psscan->npts = psscan->mpts;
				sprintf(psscan->smsg, "P%1d Request Exceeded Maximum Points !", i + 1);
				/* adjust changed field to be consistent */
				pParms->p_wd = (pParms->p_si * MAX(1,(psscan->npts - 1)));
				POST(&pParms->p_wd);
				psscan->alrt = 1;
			}
			POST(&psscan->npts);
			pParms->p_sp = pParms->p_cp - (MAX(1,(psscan->npts - 1)) * pParms->p_si) / 2;
			POST(&pParms->p_sp);
			pParms->p_ep = pParms->p_sp + (MAX(1,(psscan->npts - 1)) * pParms->p_si);
			POST(&pParms->p_ep);
			precPvt->nptsCause = i;	/* indicate cause of # of points changed */
			changedNpts(psscan);
			return;
			/* if center/end/inc are not frozen, change them */
		} else if (!pParms->p_fc && !pParms->p_fe && !pParms->p_fi) {
			pParms->p_si = pParms->p_wd / MAX(1,(psscan->npts - 1));
			POST(&pParms->p_si);
			pParms->p_ep = pParms->p_sp + (MAX(1,(psscan->npts - 1)) * pParms->p_si);
			POST(&pParms->p_ep);
			pParms->p_cp = (pParms->p_ep + pParms->p_sp) / 2;
			POST(&pParms->p_cp);
			return;
			/* if start/center/inc are not frozen, change them */
		} else if (!pParms->p_fs && !pParms->p_fc && !pParms->p_fi) {
			pParms->p_si = pParms->p_wd / MAX(1,(psscan->npts - 1));
			POST(&pParms->p_si);
			pParms->p_sp = pParms->p_ep - (MAX(1,(psscan->npts - 1)) * pParms->p_si);
			POST(&pParms->p_sp);
			pParms->p_cp = (pParms->p_ep + pParms->p_sp) / 2;
			POST(&pParms->p_cp);
			return;
			/* If npts/center/end aren't frozen, change them */
		} else if (!psscan->fpts && !pParms->p_fc && !pParms->p_fe) {
			if (fabs(pParms->p_si) <= DBL_EPSILON) {
				psscan->npts = psscan->mpts;
			} else {
				psscan->npts = (pParms->p_wd / pParms->p_si) + 1;
			}
			if (psscan->npts > psscan->mpts) {
				psscan->npts = psscan->mpts;
				sprintf(psscan->smsg, "P%1d Request Exceeded Maximum Points !", i + 1);
				/* adjust changed field to be consistent */
				pParms->p_wd = (pParms->p_si * MAX(1,(psscan->npts - 1)));
				POST(&pParms->p_wd);
				psscan->alrt = 1;
			}
			POST(&psscan->npts);
			pParms->p_ep = pParms->p_sp + (MAX(1,(psscan->npts - 1)) * pParms->p_si);
			POST(&pParms->p_ep);
			pParms->p_cp = (pParms->p_ep + pParms->p_sp) / 2;
			POST(&pParms->p_cp);
			precPvt->nptsCause = i;	/* indicate cause of # of points changed */
			changedNpts(psscan);
			return;
			/* If npts/start/center aren't frozen, change them */
		} else if (!psscan->fpts && !pParms->p_fs && !pParms->p_fc) {
			if (fabs(pParms->p_si) <= DBL_EPSILON) {
				psscan->npts = psscan->mpts;
			} else {
				psscan->npts = (pParms->p_wd / pParms->p_si) + 1;
			}
			if (psscan->npts > psscan->mpts) {
				psscan->npts = psscan->mpts;
				sprintf(psscan->smsg, "P%1d Request Exceeded Maximum Points !", i + 1);
				/* adjust changed field to be consistent */
				pParms->p_wd = (pParms->p_si * MAX(1,(psscan->npts - 1)));
				POST(&pParms->p_wd);
				psscan->alrt = 1;
			}
			POST(&psscan->npts);
			pParms->p_sp = pParms->p_ep - (MAX(1,(psscan->npts - 1)) * pParms->p_si);
			POST(&pParms->p_sp);
			pParms->p_cp = (pParms->p_ep + pParms->p_sp) / 2;
			POST(&pParms->p_cp);
			precPvt->nptsCause = i;	/* indicate cause of # of points changed */
			changedNpts(psscan);
			return;
		} else {	/* too constrained !! */
			pParms->p_wd = (pParms->p_ep - pParms->p_sp);
			POST(&pParms->p_wd);
			sprintf(psscan->smsg, "P%1d SCAN Parameters Too Constrained !", i + 1);
			psscan->alrt = 1;
			return;
		}
		break;

	default:
		return;
	}
}



/* This routine attempts to bring all linear scan parameters into
 * "consistency". It is used at init and when the # of pts changes.
 * If the number of points changed because of a change in a Positioner's
 * scan parameters, that positioner is skipped to make sure that there is
 * no conflict with that logic. Any positioner in TABLE mode is also skipped.
 *
 * Any positioner that is in TABLE mode is checked to make sure the last
 * table loaded into p_pa had enough points for the new npts. If not, the
 * operator is warned
 *
 */

static void 
changedNpts(psscan)
	sscanRecord *psscan;
{

	recPvtStruct   *precPvt = (recPvtStruct *) psscan->rpvt;
	posFields      *pParms = (posFields *) & psscan->p1pp;
	int             i;
	unsigned short  freezeState = 0, *pPvStat = &psscan->p1nv;

	/* for each valid positioner, calculate scan params as best as we can */
	/* if the positioner is in table mode, don't touch linear scan parms! */
	for (i = 0; i < NUM_POS; i++, pParms++, pPvStat++) {
		/* Check if Positioner is in TABLE Mode */
		if ((*pPvStat == PV_OK) && (pParms->p_sm == sscanP1SM_Table)) {
			if (precPvt->tablePts[i] < psscan->npts) {
				sprintf(psscan->smsg, "Pts in P%d Table < # of Steps!", i + 1);
				if (!psscan->alrt) {
					psscan->alrt = 1;
				}
			}
		}
		/* Skip the positioner that caused this */
		else if (i != precPvt->nptsCause) {
			/* Adjust linear scan params as best we can */

			/* develop freezeState switching word ... */
			/* START_FRZ | STEP_FRZ | END_FRZ | CENTER_FRZ | WIDTH_FRZ */

			freezeState = (pParms->p_fs << 4) |
				(pParms->p_fi << 3) |
				(pParms->p_fe << 2) |
				(pParms->p_fc << 1) |
				(pParms->p_fw);

			if (sscanRecordDebug >= 2) {
				printf("%s:Freeze State of P%1d = 0x%hx \n", psscan->name, i, freezeState);
			}
			/* a table describing what happens is at the end of the file */
			switch (freezeState) {
			case (0):
			case (1):	/* if center or width is frozen, but not inc , ... */
			case (2):
			case (3):
			case (4):
			case (5):	/* if end/center or end/width are frozen, but not inc, */
			case (6):
			case (7):
			case (16):
			case (17):	/* if start/center or start/width are frozen, but not inc */
			case (18):
			case (19):
			case (20):
			case (21):
			case (22):
			case (23):
				pParms->p_si = (pParms->p_ep - pParms->p_sp) / MAX(1,(psscan->npts - 1));
				POST(&pParms->p_si);
				if (pParms->p_wd != (pParms->p_ep - pParms->p_sp)) {
					pParms->p_wd = (pParms->p_ep - pParms->p_sp);
					POST(&pParms->p_wd);
				}
				if (pParms->p_cp != ((pParms->p_ep + pParms->p_sp)/2)) {
					pParms->p_cp = ((pParms->p_ep + pParms->p_sp)/2);
					POST(&pParms->p_cp);
				}
				break;

			case (8):	/* end/center/width unfrozen, change them */
			case (24):
				pParms->p_ep = pParms->p_sp + (MAX(1,(psscan->npts - 1)) * pParms->p_si);
				POST(&pParms->p_ep);
				pParms->p_cp = (pParms->p_ep + pParms->p_sp) / 2;
				POST(&pParms->p_cp);
				pParms->p_wd = (pParms->p_ep - pParms->p_sp);
				POST(&pParms->p_wd);
				break;

			case (12):	/* start/center/width aren't frozen, change them  */
				pParms->p_sp = pParms->p_ep - (MAX(1,(psscan->npts - 1)) * pParms->p_si);
				POST(&pParms->p_sp);
				pParms->p_cp = (pParms->p_ep + pParms->p_sp) / 2;
				POST(&pParms->p_cp);
				pParms->p_wd = (pParms->p_ep - pParms->p_sp);
				POST(&pParms->p_wd);
				break;

			case (10):	/* if center is frozen, but not width/start/end , ...  */
				pParms->p_sp = pParms->p_cp - (MAX(1,(psscan->npts - 1)) * pParms->p_si) / 2;
				POST(&pParms->p_sp);
				pParms->p_ep = pParms->p_sp + (MAX(1,(psscan->npts - 1)) * pParms->p_si);
				POST(&pParms->p_ep);
				pParms->p_wd = (pParms->p_ep - pParms->p_sp);
				POST(&pParms->p_wd);
				break;

				/* The following freezeStates are known to be "Too Constrained" */
				/* 9,11,13,14,15,25,26,27,28,29,30,31 */
			default:	/* too constrained !! */
				sprintf(psscan->smsg, "P%1d SCAN Parameters Too Constrained !", i + 1);
				psscan->alrt = 1;
				break;
			}

		}
	}
	if (sscanRecordDebug >= 5) {
		printf("%s:changedNpts: p1sp=%f,p1cp=%f,p1ep=%f,p1wd=%f,p1si=%f\n", psscan->name,
			psscan->p1sp,psscan->p1cp,psscan->p1ep,psscan->p1wd,psscan->p1si);
	}
}



static long 
checkScanLimits(psscan)
	sscanRecord *psscan;
{

	recDynLinkPvt  *puserPvt;
	recPvtStruct   *precPvt = (recPvtStruct *) psscan->rpvt;
	posFields      *pPos = (posFields *) & psscan->p1pp;
	unsigned short *pPvStat = &psscan->p1nv;


	/* for each valid positioner, fetch control limits */
	long            status = 0;
	size_t          nRequest = 1;
	long            i, j;
	double          value;

	if (sscanRecordDebug >= 2) {
		if (!psscan->p1nv)
			printf("%s:P1 Control Limits : %.4f   %.4f\n",
			       psscan->name, psscan->p1lr, psscan->p1hr);
		if (!psscan->p2nv)
			printf("%s:P2 Control Limits : %.4f   %.4f\n",
			       psscan->name, psscan->p2lr, psscan->p2hr);
		if (!psscan->p3nv)
			printf("%s:P3 Control Limits : %.4f   %.4f\n",
			       psscan->name, psscan->p3lr, psscan->p3hr);
		if (!psscan->p4nv)
			printf("%s:P4 Control Limits : %.4f   %.4f\n",
			       psscan->name, psscan->p4lr, psscan->p4hr);
	}
	/* Update "previous position" of positioners to use in relative mode */
	pPvStat = &psscan->p1nv;
	pPos = (posFields *) & psscan->p1pp;
	for (i = 0; i < NUM_POS; i++, pPos++, pPvStat++) {
		j = i+1;
		if (*pPvStat == PV_OK) {
			puserPvt = precPvt->caLinkStruct[i].puserPvt;
			if (puserPvt->dbAddrNv || puserPvt->useDynLinkAlways) {
				status |= recDynLinkGet(&precPvt->caLinkStruct[i],
				     &pPos->p_pp, &nRequest, 0, 0, 0);
			} else {
				status |= dbGet(puserPvt->pAddr, DBR_DOUBLE, &pPos->p_pp,
						0, 0, NULL);
			}
			POST(&pPos->p_pp);
			if (sscanRecordDebug)
				printf("%s:checkScanLimits: P%1ld pp=%f (status=%ld)\n",
					psscan->name, j, pPos->p_pp, status);
			if (status) {
				printf("%s:checkScanLimits: could not get current value\n", psscan->name);
				sprintf(psscan->smsg, "Can't get current position"); POST(&psscan->smsg);
				if (!psscan->alrt) {psscan->alrt = 1; POST(&psscan->alrt);}
				return(ERROR);
			}
		}
	}

	/* If sscanRecordDontCheckLimits flag is set and XSC = 1, don't proceed */
	if (sscanRecordDontCheckLimits && psscan->xsc)
		return (OK);

	/* First check if any valid pos'rs are in Table mode with insufficient points */
	pPvStat = &psscan->p1nv;
	pPos = (posFields *) & psscan->p1pp;
	for (i = 0; i < NUM_POS; i++, pPos++, pPvStat++) {
		if ((*pPvStat == PV_OK) &&
		    (pPos->p_sm == sscanP1SM_Table) &&
		    (precPvt->tablePts[i] < psscan->npts)) {
			sprintf(psscan->smsg, "Pts in P%ld Table < # of Steps", i + 1);
			POST(&psscan->smsg);
			if (!psscan->alrt) {psscan->alrt = 1; POST(&psscan->alrt);}
			return (ERROR);
		}
	}

	/* check each point of scan against control limits. */
	/* Stop on first error */

	pPvStat = &psscan->p1nv;
	pPos = (posFields *) & psscan->p1pp;
	for (i = 0; i < NUM_POS; i++, pPos++, pPvStat++) {
		if (*pPvStat == PV_OK) {
			for (j = 0; j < psscan->npts; j++) {
				/* determine next desired position for each positioner */
				switch (pPos->p_sm) {
				case sscanP1SM_Linear:
					if (pPos->p_ar) {
						value = (pPos->p_pp + pPos->p_sp) +
							(j * pPos->p_si);
					} else {
						value = pPos->p_sp + (j * pPos->p_si);
					}
					break;

				case sscanP1SM_Table:
					if (pPos->p_ar) {
						value = pPos->p_pp + pPos->p_pa[j];
					} else {
						value = pPos->p_pa[j];
					}
					break;

				case sscanP1SM_On_The_Fly:
					if (pPos->p_ar) {
						if (j == 0)
							value = pPos->p_pp + pPos->p_sp;
						else
							value = pPos->p_pp + pPos->p_ep;
					} else {
						if (j == 0)
							value = pPos->p_sp;
						else
							value = pPos->p_ep;
					}
					break;

				default:
					value = 0;
				}

				if ((pPos->p_lr != 0) && (value < pPos->p_lr)) {
					sprintf(psscan->smsg, "P%-ld Value < LO_Limit @ point %1ld", i + 1, j);
					psscan->alrt = 1;
					return (ERROR);
				} else if ((pPos->p_hr != 0) && (value > pPos->p_hr)) {
					sprintf(psscan->smsg, "P%-ld Value > HI_Limit @ point %1ld", i + 1, j);
					psscan->alrt = 1;
					return (ERROR);
				}
			}
		}
	}

	/* No errors if we made it here ... */
	sprintf(psscan->smsg, "SCAN Values within limits");
	return (OK);

}


/* This routine show a "preview" of the scan positions by
 * loading the detector arrays (d_da) with the scan positions at each
 * point and loading the readback arrays (p_ra) with the step #.
 * A plot of d_da vs p_ra will provide a graphical view of the sscan
 * ranges.
 *
 */

static void 
previewScan(psscan)
	sscanRecord *psscan;
{

	recPvtStruct   *precPvt = (recPvtStruct *) psscan->rpvt;
	recDynLinkPvt  *puserPvt;
	posFields      *pPos;
	detFields      *pDet;
	unsigned short *pPvStat;
	double         *pPosBuf;
	float          *pDetBuf;
	float           value;
	long            i, j;
	long            status;
	size_t          nRequest = 1;

	/* Update "previous position" of positioners to use in relative mode */
	pPvStat = &psscan->p1nv;
	pPos = (posFields *) & psscan->p1pp;
	for (i = 0; i < NUM_POS; i++, pPos++, pPvStat++) {
		if (*pPvStat == PV_OK) {
			puserPvt = precPvt->caLinkStruct[i].puserPvt;
			if (puserPvt->dbAddrNv || puserPvt->useDynLinkAlways) {
				status |= recDynLinkGet(&precPvt->caLinkStruct[i],
				     &pPos->p_pp, &nRequest, 0, 0, 0);
			} else {
				status |= dbGet(puserPvt->pAddr, DBR_DOUBLE, &pPos->p_pp,
						0, 0, NULL);
			}
			POST(&pPos->p_pp);
		}
	}

	/* Run through entire scan for each valid positioner */
	pPvStat = &psscan->p1nv;
	pPos = (posFields *) & psscan->p1pp;
	pDet = (detFields *) & psscan->d01hr;
	for (i = 0; i < NUM_POS; i++, pPos++, pPvStat++, pDet++) {
		if (*pPvStat == PV_OK) {
			/* must use the current buffer pointer */
			if (precPvt->validBuf == B_BUFFER) {
				pPosBuf = precPvt->posBufPtr[i].pBufB;
				pDetBuf = precPvt->detBufPtr[i].pBufB;
			} else {
				pPosBuf = precPvt->posBufPtr[i].pBufA;
				pDetBuf = precPvt->detBufPtr[i].pBufA;
			}
			for (j = 0; j < psscan->npts; j++) {
				/* determine next desired position for each positioner */
				switch (pPos->p_sm) {
				case sscanP1SM_Linear:
					if (pPos->p_ar) {
						value = (pPos->p_pp + pPos->p_sp) +
							(j * pPos->p_si);
					} else {
						value = pPos->p_sp + (j * pPos->p_si);
					}
					break;

				case sscanP1SM_Table:
					if (pPos->p_ar) {
						value = pPos->p_pp + pPos->p_pa[j];
					} else {
						value = pPos->p_pa[j];
					}
					break;

				case sscanP1SM_On_The_Fly:
					if (pPos->p_ar) {
						if (j == 0)
							value = pPos->p_pp + pPos->p_sp;
						else
							value = pPos->p_pp + pPos->p_ep;
					} else {
						if (j == 0)
							value = pPos->p_sp;
						else
							value = pPos->p_ep;
					}
					break;

				default:
					value = 0;
				}
				pPosBuf[j] = j;
				pDetBuf[j] = value;
			}
			/* now fill the rest of the array(s) with the last values */
			for (j = j; j < psscan->mpts; j++) {
				pPosBuf[j] = pPosBuf[j - 1];
				pDetBuf[j] = pDetBuf[j - 1];
			}
			POST(precPvt->posBufPtr[i].pBufA);
			POST(precPvt->posBufPtr[i].pBufB);
			POST(precPvt->detBufPtr[i].pBufA);
			POST(precPvt->detBufPtr[i].pBufB);
			/*
			 * I must also post a monitor on the NULL array, because
			 * some clients connected to D?PV's without valid PV's !
			 */
			POST(precPvt->nullArray);
		}
	}
}

static void 
saveFrzFlags(psscan)
	sscanRecord *psscan;
{

	recPvtStruct   *precPvt = (recPvtStruct *) psscan->rpvt;
	posFields      *pPos = (posFields *) & psscan->p1pp;
	int             i;


	/* save state of each freeze flag */
	precPvt->fpts = psscan->fpts;
	for (i = 0; i < NUM_POS; i++, pPos++) {
		precPvt->posParms[i].p_fs = pPos->p_fs;
		precPvt->posParms[i].p_fi = pPos->p_fi;
		precPvt->posParms[i].p_fc = pPos->p_fc;
		precPvt->posParms[i].p_fe = pPos->p_fe;
		precPvt->posParms[i].p_fw = pPos->p_fw;
	}
}

static void 
savePosParms(sscanRecord * psscan, unsigned short i)
{

	recPvtStruct   *precPvt = (recPvtStruct *) psscan->rpvt;
	posFields      *pPos = (posFields *) & psscan->p1pp + i;

	/* save state of linear scan parameters 0 */
	/* Do this when in table mode so operator is not confused */
	precPvt->posParms[i].p_sp = pPos->p_sp;
	precPvt->posParms[i].p_si = pPos->p_si;
	precPvt->posParms[i].p_ep = pPos->p_ep;
	precPvt->posParms[i].p_cp = pPos->p_cp;
	precPvt->posParms[i].p_wd = pPos->p_wd;
}

static void 
zeroPosParms(sscanRecord * psscan, unsigned short i)
{

	posFields      *pPos = (posFields *) & psscan->p1pp + i;

	/* set them to 0 */
	/* Do this when in table mode so operator is not confused */
	pPos->p_sp = 0; POST(&pPos->p_sp);
	pPos->p_si = 0; POST(&pPos->p_si);
	pPos->p_ep = 0; POST(&pPos->p_ep);
	pPos->p_cp = 0; POST(&pPos->p_cp);
	pPos->p_wd = 0; POST(&pPos->p_wd);
}

static void 
resetFrzFlags(psscan)
	sscanRecord *psscan;
{

	posFields      *pPos = (posFields *) & psscan->p1pp;
	int             i;

	/* reset each frzFlag, post monitor if changed */

	if (psscan->fpts) {
		psscan->fpts = 0;
		POST(&psscan->fpts);
	}
	for (i = 0; i < NUM_POS; i++, pPos++) {
		if (pPos->p_fs) {pPos->p_fs = 0; POST(&pPos->p_fs);}
		if (pPos->p_fi) {pPos->p_fi = 0; POST(&pPos->p_fi);}
		if (pPos->p_fc) {pPos->p_fc = 0; POST(&pPos->p_fc);}
		if (pPos->p_fe) {pPos->p_fe = 0; POST(&pPos->p_fe);}
		if (pPos->p_fw) {pPos->p_fw = 0; POST(&pPos->p_fw);}
	}
}


/* Restores Freeze Flags to the state they were in */
static void 
restoreFrzFlags(psscan)
	sscanRecord *psscan;
{

	recPvtStruct   *precPvt = (recPvtStruct *) psscan->rpvt;
	posFields      *pPos = (posFields *) & psscan->p1pp;
	int             i;


	/* restore state of each freeze flag, post if changed */
	psscan->fpts = precPvt->fpts;
	if (psscan->fpts) POST(&psscan->fpts);
	for (i = 0; i < NUM_POS; i++, pPos++) {
		pPos->p_fs = precPvt->posParms[i].p_fs;
		if (pPos->p_fs) POST(&pPos->p_fs);
		pPos->p_fi = precPvt->posParms[i].p_fi;
		if (pPos->p_fi) POST(&pPos->p_fi);
		pPos->p_fc = precPvt->posParms[i].p_fc;
		if (pPos->p_fc) POST(&pPos->p_fc);
		pPos->p_fe = precPvt->posParms[i].p_fe;
		if (pPos->p_fe) POST(&pPos->p_fe);
		pPos->p_fw = precPvt->posParms[i].p_fw;
		if (pPos->p_fw) POST(&pPos->p_fw);
	}
}

/* Restores Position Parms after leaving table mode (for 1 positioner) */
static void 
restorePosParms(sscanRecord * psscan, unsigned short i)
{

	recPvtStruct   *precPvt = (recPvtStruct *) psscan->rpvt;
	posFields      *pPos = (posFields *) & psscan->p1pp + i;

	pPos->p_sp = precPvt->posParms[i].p_sp; POST(&pPos->p_sp);
	pPos->p_si = precPvt->posParms[i].p_si; POST(&pPos->p_si);
	pPos->p_ep = precPvt->posParms[i].p_ep; POST(&pPos->p_ep);
	pPos->p_cp = precPvt->posParms[i].p_cp; POST(&pPos->p_cp);
	pPos->p_wd = precPvt->posParms[i].p_wd; POST(&pPos->p_wd);
}

/*  Memo from Tim Mooney suggesting change in order of precedence
 * 
 *
 * 
 * Ned,
 * 
 * Current effects of sscan-parameter changes are summarized below, along
 * with the effects I think might be preferable.  The ideas are to make NPTS
 * nearly as easy to change as is INCR, to make END easier to change than
 * is START, and to prefer changing INCR in response to a change of NPTS
 * whenever permissible.
 * 
 * If an indirect change in NPTS would make it larger than MPTS, I think
 * NPTS ideally should be regarded as frozen, rather than being set to
 * MPTS.  I don't feel all that strongly about this, since I think it
 * might be more of a pain to code than it's worth.
 * 
 * Comments on any of this?
 * 
 * Tim
 * 
 * ====================================================================
 * changing	now affects		should affect
 * --------------------------------------------------------------
 * START	->	INCR CENTER WIDTH	INCR CENTER WIDTH
 * 	or	END CENTER		CENTER NPTS WIDTH
 * 	or	INCR END WIDTH		END CENTER
 * 	or	NPTS CENTER WIDTH	INCR END WIDTH
 * 	or	NPTS END WIDTH		NPTS END WIDTH
 * 	or	CENTER END		<punt>
 * 	or	<punt>
 * INCR	->	CENTER END WIDTH	NPTS
 * 	or	START CENTER WIDTH	CENTER END WIDTH
 * 	or	START END WIDTH		START CENTER WIDTH
 * 	or	NPTS			START END WIDTH
 * 	or	<punt>			<punt>
 * CENTER	->	START END		START END
 * 	or	START INCR WIDTH	END INCR WIDTH
 * 	or	END INCR WIDTH		START INCR WIDTH
 * 	or	NPTS START WIDTH	NPTS END WIDTH
 * 	or	NPTS END WIDTH		NPTS START WIDTH
 * 	or	<punt>
 * END	->	INCR CENTER WIDTH	INCR CENTER WIDTH
 * 	or	START CENTER WIDTH	NPTS CENTER WIDTH
 * 	or	START WIDTH INCR	START CENTER
 * 	or	NPTS START WIDTH	START WIDTH INCR
 * 	or	NPTS CENTER WIDTH	NPTS START WIDTH
 * 	or	START CENTER		<punt>
 * 	or	<punt>
 * WIDTH	->	START END INCR		START END INCR
 * 	or	CENTER END INCR		START END NPTS
 * 	or	START CENTER INCR	CENTER END INCR
 * 	or	NPTS START END		START CENTER INCR
 * 	or	NPTS CENTER END		NPTS CENTER END
 * 	or	NPTS START CENTER	NPTS START CENTER
 * 	or	<punt>			<punt>
 * 
 * NPTS: given
 * SIECW (Start,Incr,End,Center,Width freeze-switch states; '1' = 'frozen')
 * ---------------------------------------------------------------
 * 00000	 (0)	END CENTER WIDTH	INCR
 * 00001	 (1)	INCR			INCR
 * 00010	 (2)	START END WIDTH		INCR
 * 00011	 (3)	INCR			INCR
 * 00100	 (4)	START CENTER WIDTH	INCR
 * 00101	 (5)	INCR			INCR
 * 00110	 (6)	INCR			INCR
 * 00111	 (7)	INCR			INCR
 * 01000	 (8)	END CENTER WIDTH	END CENTER WIDTH
 * 01001	 (9)	<punt>			<punt>
 * 01010	(10)	START END WIDTH		START END WIDTH
 * 01011	(11)	<punt>			<punt>
 * 01100	(12)	START CENTER WIDTH	START CENTER WIDTH
 * 01101	(13)	<punt>			<punt>
 * 01110	(14)	<punt>			<punt>
 * 01111	(15)	<punt>			<punt>
 * 10000	(16)	END CENTER WIDTH	INCR
 * 10001	(17)	INCR			INCR
 * 10010	(18)	INCR			INCR
 * 10011	(19)	INCR			INCR
 * 10100	(20)	INCR			INCR
 * 10101	(21)	INCR			INCR
 * 10110	(22)	INCR			INCR
 * 10111	(23)	INCR			INCR
 * 11000	(24)	END CENTER WIDTH	END CENTER WIDTH
 * 11001	(25)	<punt>			<punt>
 * 11010	(26)	<punt>			<punt>
 * 11011	(27)	<punt>			<punt>
 * 11100	(28)	<punt>			<punt>
 * 11101	(29)	<punt>			<punt>
 * 11110	(30)	<punt>			<punt>
 * 11111	(31)	<punt>			<punt>
 */
