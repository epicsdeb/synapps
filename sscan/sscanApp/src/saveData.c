/************************************************************************/
/*
 *      Original Author: Eric Boucher
 *      Date:            04-09-98
 *      Current Author: Tim Mooney
 *      Date:            3/2/2007
 
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contract
 *      W-31-109-ENG-38 at Argonne National Laboratory.
 *
 *      Beamline Controls & Data Acquisition Group
 *      Experimental Facilities Division
 *      Advanced Photon Source
 *      Argonne National Laboratory
 *
 * Modification Log:
 * .01 04-01-98  erb  Initial development
 * .02 04-15-98  erb  development of the request file format for 
 *                    easy customisation
 * .03 04-28-98  erb  Work on the archive file format.
 * .04 05-25-98  erb  added hand-shaking customisation thru request file
 * .05 05-27-98  erb  added ioc prefix customisation in the request file
 *                    for archive naming convention.
 * .06 07-14-98  erb  Added real-time 1D data aquisition
 *                    - changed the file format
 *                    - added debug_saveDataCpt
 *                      0: wait forever for space in message queue, then send message
 *                      1: send message only if queue is not full
 *                      2: send message only if queue is not full and specified
 *                         time has passed (SetCptWait() sets this time
 *                      3: if specified time has passed, wait for space in queue,
 *                         then send message
 *                      else: don't send message
 * .07 08-03-98  erb  Added environment pv's
 *                    saveData will request data for those env pv's at the 
 *                    begining of the scan, but will not wait for answers
 *                    and save the env data at the end of the scan.
 * .08 08-09-98  erb  bug fixes: time-stamp
 *                    still some work to do: the time stamp is not availlable
 *                    when the scan start.
 *
 * .09 09-01-98  erb  First release: version 1.0
 *
 * .10 09-08-98  tmm  rename debug_saveDataCpt -> saveData_MessagePolicy
 * .11 09-09-98  erb  Clarification of the messages send to the user
 * .12 09-12-98  erb  Modification of saveData_active
 *                    -rename saveData_active to saveData_status
 *                     0: saveData is not active
 *                     1: saveData is active and ready to save scans
 *                     2: saveData is active but unable to save scan
 * .13 09-20-98  erb  work on the file-system and sub-directory
 *                    -check if file system is correctly mounted
 *                    -check R/W permission of the target directory
 *                    -Prevent user from starting a scan if something is wrong
 * .14 10-02-98  erb  Modification of the file format to improve "weird" scan storage
 *                    VERSION 1.1
 * .15 05-06-99  erb  Change EXSC field to DATA and reverse the logic
 *                    Save time stamp when scan starts
 * .16 07-15-99  erb  -Fix the trigger bug for 2D and higer scans.
 *                    -Change version to 1.2 to avoid confusion with 
 *                     "invalid 1.1" files.
 *                    -Add the saveData_Version function.
 *                    -Add the saveData_CVS function.
 * .17 11-30-99  tmm  v1.3 Added 70 new detectors, and two new triggers
 * .18 03-23-00  erb  -fix filename bug.
 *                    -Check that txpv is a registered scan pv AND that txcd=1.0
 *                     to link scans together.
 *                    -suspend calling task (shell) until all connections are made.
 * .19 12-19-00  tmm  v1.5 check file open to see if it really succeeded.
 * .20 01-09-01  tmm  v1.6 fold in Ron Sluiter's mods to 1.4 that made it work
 *                    with the powerpC processor.
 * .20 03-08-01  tmm  v1.7 Don't handshake if file system can't be mounted, but do
 *                    continue to enable and disable puts to filesystem PV's.
 * .21 05-01-01  tmm  v1.8 Don't allow punctuation characters in filename.
 *                    Extension changed to ".mda".
 *                    e.g., tmm:scan1_0000.scan  --> tmm_scan1_000.mda
 * .22 02-27-02  tmm  v1.9 Support filename PV, so clients can easily determine
 *                    the current data file.  Write char array with xdr_vector()
 *                    instead of xdr_bytes(), which wasn't working.
 * .23 03-24-02  tmm  v1.10 Increased stack from 5000 to 10000.
 * .24 04-02-02  tmm  v1.11 trigger PV now retains field name.  Trig command now
 *                    written correctly
 * .25 04-23-02  tmm  v1.12 realTime1D wasn't received as DBR_SHORT but treated as int.
 *                    This caused saveData to behave as if realTime1D were on at boot.
 *                    Added code to print file-write time if debug_saveData==1.
 * .26 07-22-02  tmm  v1.13 null terminate DBR_STRING variables
 * .28 04-07-03  tmm  v1.15 Convert to EPICS 3.14; delete D1*-DF* detectors
 * .29 10-30-03  tmm  v1.16 (EPICS 3.14) Added buf-size arg to epicsMessageQueueReceive()
 *                    for EPICS 3.14.3.  Added epicsExport stuff; changed saveData_Init
 *                    to return void, because iocsh doesn't support functions that return
 *                    a value.
 * .27 04-26-04  tmm  v1.14 (EPICS 3.13) Allow user to specify the value of the sscan-record field,
 *                    DATA, on which saveData signals that the sscan record should wait.
 *                    If 0 (the default) saveData will signal when a scan starts; if 1,
 *                    when the scan posts data.  This is specified with the volatile
 *                    variable saveData_HandshakeOnDATAHigh.
 *                    Also, fixes for a few inconsequential compiler warnings.
 * .28 05-20-04  tmm  (EPICS 3.13) Ignore handshake PV in saveData.req, and remove the variable
 *                    saveData_HandshakeOnDATAHigh.  Handshake now always writes 1 to
 *                    <sscanRecord>.AWAIT when <sscanRecord>.DATA goes to 1, and 0
 *                    when the data have been written.
 *     07-08-04  tmm  v1.20 Merged 3.13 array/AWAIT into 3.14 versions
 *     02-16-05  tmm  v1.21 Run on Linux and Solaris ioc's.  ca_add_event()
 *                    apparently does not return an event id; replaced calls that require this
 *                    with calls to ca_add_subscription().  If not vxWorks, then
 *                    we don't do file-system mounting (we let system admin do it)
 *                    This means we always use server path, rather than local path
 *                    relative to a locally defined mount point.
 *     03-30-06  tmm  v1.22 Clear unused data points before writing
 *     05-19-06  tmm  v1.23 Changed test filename from "rix:_" to "rix_"
 *     03-02-07  tmm  v1.24 Check file writes; retry if they fail
 *     03-05-07  tmm  v1.25 Extend file-write check to point-by-point writing, but don't
 *                    retry point-by-point.  Deleted all references to pscan->all_pts,
 *                    which used to indicate that all data had been written point-by-point,
 *                    so array write was not needed.  Now we always do the array write.
 *     03-27-07  tmm  v1.26 Control and display retries with PV's, instead of volatile
 *                    variables, so end users can control and monitor the behavior.
 *                    Discovered that scanSee uses saveData's [message] PV to determine the
 *                    name of the MDA file, which is assumed to be the last word of the
 *                    string.  (I don't know why it doesn't use the [filename] PV, which
 *                    should be much simpler and less error prone.)  Recent modifications
 *                    to saveData caused the file name to be shown in quotes, which scanSee
 *                    treated as part of the filename, causing it to crash.   Took out the
 *                    quotes from all user messages that include the MDA file name..
 *     06-08-07  tmm  v1.27 Set prefix size consistently as PREFIX_SIZE (currently 10)
 *     06-11-07  tmm  v1.28 saveData was writing scan dimensions to the wrong file offset,
 *                    under some circumstances, because proc_scan_data() was setting those
 *                    offsets from variables that had not been initialized.  Moved the
 *                    setting of pscan->nxt->dims_offset and pscan->nxt->regular_offset from
 *                    proc_scan_data(), to writeScanRecInProgress().
 *     11-13-07  tmm  v1.29 Increased stack size.  Ron Sluiter found an ioc with a stack
 *                    margin of only 616 (of 11720).
 *     01-29-08  tmm  v1.30 Allow end user to specify filename (see "basename")
 *     06-24-08  tmm  v1.31 If saveData's init file did not have a [basename]
 *                    section, or the database did not have the specified PV,
 *                    saveData would fail to complete initialization, and then
 *                    fail to store data files, but it would not make this
 *                    obvious to users, and it would not cause scans to hang.
 *                    Now, basename-related failures disable the use of basename,
 *                    but saveData still functions.
 *     11-14-08  tmm  Instead of including nfsDrv.h (which is renamed in tornado 2.2, define
 *                    nfsMount, nfsUnmount by hand.
 *     03-26-09  tmm  Chid check before ca_array_get was wrong.  Added more chid checks.
 *     04-04-09  tmm  v1.32 If file exists, use <base>_nnnn_mm.mda instead of <base>_nnnn.mda_mm
 *     06-04-09  dmk  Moved the strncpy() calls in saveData_Init() to inside the
 *                    brackets as to not overwrite the existing values if the
 *                    method is called multiple times.

 */

#define FILE_FORMAT_VERSION (float)1.3
#define SAVE_DATA_VERSION   "1.32.0"

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
/* #include <osiUnistd.h> 3.14.11 */
#include <unistd.h>

#ifdef vxWorks
#include <usrLib.h>
#include <ioLib.h>

/* nfsDrv.h was renamed nfsDriver.h in Tornado 2.2.2 */
/* #include	<nfsDrv.h> */
extern STATUS nfsMount(char *host, char *fileSystem, char *localName);
extern STATUS nfsUnmount(char *localName);

#else
#include <sys/stat.h>
#include <fcntl.h>
#define OK 0
#define ERROR -1
#endif

#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include <dbEvent.h>
#include <special.h>
#include <cadef.h>
#include <tsDefs.h>
#include <epicsMutex.h>
#include <epicsMessageQueue.h>
#include <epicsThread.h>
#include <dbDefs.h>         /* for PVNAME_STRINGSZ */
#include <epicsTypes.h>     /* for MAX_STRING_SIZE */

#define DESC_SIZE 30
#define EGU_SIZE 16
#define PREFIX_SIZE PVNAME_STRINGSZ/2
#define BASENAME_SIZE 20

#include "req_file.h"
#include "xdr_lib.h"
#ifdef vxWorks
#include "xdr_stdio.h"
#endif

/************************************************************************/
/*                           MACROS                                     */

#define ALLOC_ALL_DETS 0
/* This seeems approximately where saveData was in 3.13 (priority was 150) */
#define PRIORITY epicsThreadPriorityCAServerHigh+4	/* The saveDataTask priority */

/*#define NODEBUG*/

#define LOCAL static

volatile int debug_saveData = 0;
volatile int debug_saveDataMsg = 0;
volatile int saveData_MessagePolicy = 0;

#ifdef NODEBUG 
#define Debug0(d,s) ;
#define Debug1(d,s,p) ;
#define Debug2(d,s,p1,p2) ;
#define Debug3(d,s,p1,p2,p3) ;
#define Debug4(d,s,p1,p2,p3,p4) ;
#define Debug5(d,s,p1,p2,p3,p4,p5) ;
#define Debug6(d,s,p1,p2,p3,p4,p5,p6) ;
#define DebugMsg0(d,s) ;
#define DebugMsg1(d,s,p) ;
#define DebugMsg2(d,s,p1,p2) ;
#define DebugMsg3(d,s,p1,p2,p3) ;
#else
#define Debug0(d,s)       {if (d<=debug_saveData) {printf(s);}}
#define Debug1(d,s,p)     {if (d<=debug_saveData) {printf(s,p);}}
#define Debug2(d,s,p1,p2) {if (d<=debug_saveData) {printf(s,p1,p2);}}
#define Debug3(d,s,p1,p2,p3) {if (d<=debug_saveData) {printf(s,p1,p2,p3);}}
#define Debug4(d,s,p1,p2,p3,p4) {if (d<=debug_saveData) {printf(s,p1,p2,p3,p4);}}
#define Debug5(d,s,p1,p2,p3,p4,p5) {if (d<=debug_saveData) {printf(s,p1,p2,p3,p4,p5);}}
#define Debug6(d,s,p1,p2,p3,p4,p5,p6) {if (d<=debug_saveData) {printf(s,p1,p2,p3,p4,p5,p6);}}
#define DebugMsg0(d,s)       {if (d<=debug_saveDataMsg) {printf(s);}}
#define DebugMsg1(d,s,p)     {if (d<=debug_saveDataMsg) {printf(s,p);}}
#define DebugMsg2(d,s,p1,p2) {if (d<=debug_saveDataMsg) {printf(s,p1,p2);}}
#define DebugMsg3(d,s,p1,p2,p3) {if (d<=debug_saveDataMsg) {printf(s,p1,p2,p3);}}
#endif


/************************************************************************/
/*---------- STATIC DECLARATIONS TO MATCH SCAN RECORD FIELDS -----------*/

#define SCAN_NBP  4     /* # of scan positioners        */
#define SCAN_NBD 70     /* # of scan detectors          */
#define SCAN_NBT  4     /* # of scan triggers           */

LOCAL char *pxnv[SCAN_NBP]= {
	"P1NV", "P2NV", "P3NV", "P4NV" 
};
LOCAL char *pxpv[SCAN_NBP]= {
	"P1PV", "P2PV", "P3PV", "P4PV"
};
LOCAL char *pxsm[SCAN_NBP]= {
	"P1SM", "P2SM", "P3SM", "P4SM"
};
LOCAL char *rxnv[SCAN_NBP]= {
	"R1NV", "R2NV", "R3NV", "R4NV"
};
LOCAL char *rxpv[SCAN_NBP]= { 
	"R1PV", "R2PV", "R3PV", "R4PV"
};
LOCAL char *pxra[SCAN_NBP]= {
	"P1RA", "P2RA", "P3RA", "P4RA"
};
LOCAL char *rxcv[SCAN_NBP]= {
	"R1CV", "R2CV", "R3CV", "R4CV"
};
LOCAL char *dxnv[SCAN_NBD]= {
	"D01NV","D02NV","D03NV","D04NV","D05NV","D06NV","D07NV","D08NV","D09NV","D10NV",
	"D11NV","D12NV","D13NV","D14NV","D15NV","D16NV","D17NV","D18NV","D19NV","D20NV",
	"D21NV","D22NV","D23NV","D24NV","D25NV","D26NV","D27NV","D28NV","D29NV","D30NV",
	"D31NV","D32NV","D33NV","D34NV","D35NV","D36NV","D37NV","D38NV","D39NV","D40NV",
	"D41NV","D42NV","D43NV","D44NV","D45NV","D46NV","D47NV","D48NV","D49NV","D50NV",
	"D51NV","D52NV","D53NV","D54NV","D55NV","D56NV","D57NV","D58NV","D59NV","D60NV",
	"D61NV","D62NV","D63NV","D64NV","D65NV","D66NV","D67NV","D68NV","D69NV","D70NV"
};
LOCAL char *dxpv[SCAN_NBD]= {
	"D01PV","D02PV","D03PV","D04PV","D05PV","D06PV","D07PV","D08PV","D09PV","D10PV",
	"D11PV","D12PV","D13PV","D14PV","D15PV","D16PV","D17PV","D18PV","D19PV","D20PV",
	"D21PV","D22PV","D23PV","D24PV","D25PV","D26PV","D27PV","D28PV","D29PV","D30PV",
	"D31PV","D32PV","D33PV","D34PV","D35PV","D36PV","D37PV","D38PV","D39PV","D40PV",
	"D41PV","D42PV","D43PV","D44PV","D45PV","D46PV","D47PV","D48PV","D49PV","D50PV",
	"D51PV","D52PV","D53PV","D54PV","D55PV","D56PV","D57PV","D58PV","D59PV","D60PV",
	"D61PV","D62PV","D63PV","D64PV","D65PV","D66PV","D67PV","D68PV","D69PV","D70PV"
};
LOCAL char *dxda[SCAN_NBD]= {
	"D01DA","D02DA","D03DA","D04DA","D05DA","D06DA","D07DA","D08DA","D09DA","D10DA",
	"D11DA","D12DA","D13DA","D14DA","D15DA","D16DA","D17DA","D18DA","D19DA","D20DA",
	"D21DA","D22DA","D23DA","D24DA","D25DA","D26DA","D27DA","D28DA","D29DA","D30DA",
	"D31DA","D32DA","D33DA","D34DA","D35DA","D36DA","D37DA","D38DA","D39DA","D40DA",
	"D41DA","D42DA","D43DA","D44DA","D45DA","D46DA","D47DA","D48DA","D49DA","D50DA",
	"D51DA","D52DA","D53DA","D54DA","D55DA","D56DA","D57DA","D58DA","D59DA","D60DA",
	"D61DA","D62DA","D63DA","D64DA","D65DA","D66DA","D67DA","D68DA","D69DA","D70DA"
};
LOCAL char *dxcv[SCAN_NBD]= {
	"D01CV","D02CV","D03CV","D04CV","D05CV","D06CV","D07CV","D08CV","D09CV","D10CV",
	"D11CV","D12CV","D13CV","D14CV","D15CV","D16CV","D17CV","D18CV","D19CV","D20CV",
	"D21CV","D22CV","D23CV","D24CV","D25CV","D26CV","D27CV","D28CV","D29CV","D30CV",
	"D31CV","D32CV","D33CV","D34CV","D35CV","D36CV","D37CV","D38CV","D39CV","D40CV",
	"D41CV","D42CV","D43CV","D44CV","D45CV","D46CV","D47CV","D48CV","D49CV","D50CV",
	"D51CV","D52CV","D53CV","D54CV","D55CV","D56CV","D57CV","D58CV","D59CV","D60CV",
	"D61CV","D62CV","D63CV","D64CV","D65CV","D66CV","D67CV","D68CV","D69CV","D70CV"
};
LOCAL char *txnv[SCAN_NBT]= {
	"T1NV", "T2NV", "T3NV", "T4NV"
};
LOCAL char *txpv[SCAN_NBT]= {
	"T1PV", "T2PV", "T3PV", "T4PV"
};
LOCAL char *txcd[SCAN_NBT]= {
	"T1CD", "T2CD", "T3CD", "T4CD"
};



/************************************************************************/
/*----- STRUCT TO CONNECT A SCAN RECORD AND RETRIEVE DATA FROM IT ------*/

LOCAL const char  save_data_version[]=SAVE_DATA_VERSION;

#define NOT_CONNECTED 0 /* SCAN not connected                           */
#define CONNECTED     1 /* SCAN is correctly connected                  */
#define INITIALIZED   2 /* SCAN buffers are initialized                 */

#define XXNV_OK     0
#define XXNV_BADPV  1
#define XXNV_NOPV   2

#define HANDSHAKE_BUSY 1
#define HANDSHAKE_DONE 0

typedef struct scan {
	/*========================= PRIVATE FIELDS ===========================*/
	short        state;       /* state of the structure                   */
	char         name[PVNAME_STRINGSZ];    /* name of the scan            */
	short        scan_dim;    /* dimension of this scan                   */
	char         fname[100];  /* filename                                 */
	char         ffname[100]; /* full filename                            */
	int          first_scan;  /* true if this is the first scan           */
	struct scan* nxt;         /* link to the inner scan                   */
	long         savedSeekPos; /* position at which failed write started  */
	long         offset;      /* where to store this scan's offset        */
	long         offset_extraPV;  /* where to store the extra pv's offset */
	long         counter;     /* current scan                             */
	long         dims_offset;
	long         regular_offset;
	long         old_npts;

	/*=======================SCAN RECORD FIELDS ==========================*/
	short    data;        /* scan execution                               */
	chid     cdata;
	char     stamp[MAX_STRING_SIZE];   /* scan's start time stamp         */
	long     time_fpos;

	long    mpts;     /* max # of points                                  */
	chid    cmpts;
	long    npts;     /* # of points                                      */
	chid    cnpts;
	long    cpt;      /* current point                                    */
	chid    ccpt;
	long    bcpt;     /* buffered current point                           */
	chid    cbcpt;
	long    cpt_fpos; /* where to store data of the current point         */
	epicsTimeStamp  cpt_time; /* time of the last cpt monitor             */
	int     cpt_monitored;
	evid    cpt_evid;

	int     nb_pos;   /* # of pos to save                                 */
	int     nb_det;   /* # of det to save                                 */
	int     nb_trg;   /* # of trg to save                                 */

	short   handShake_state;
	chid    chandShake;
	chid    cautoHandShake;

	/*========================== POSITIONERS =============================*/

	short   pxnv[SCAN_NBP];     /* positioner X valid PV                  */
	chid    cpxnv[SCAN_NBP];
	char    pxpv[SCAN_NBP][PVNAME_STRINGSZ]; /* positioner X PV name      */
	chid    cpxpv[SCAN_NBP];
	char    pxds[SCAN_NBP][MAX_STRING_SIZE]; /* positioner X description string */
	chid    cpxds[SCAN_NBP];
	struct dbr_ctrl_double  pxeu[SCAN_NBP];/* positioner X eng unit       */
	chid    cpxeu[SCAN_NBP];
	char    pxsm[SCAN_NBP][MAX_STRING_SIZE]; /* positioner step mode      */
	chid    cpxsm[SCAN_NBP];

	/*========================== READBACKS ===============================*/

	short   rxnv[SCAN_NBP];     /* readback X valid PV                    */
	chid    crxnv[SCAN_NBP];
	char    rxpv[SCAN_NBP][PVNAME_STRINGSZ]; /* readback X PV name        */
	chid    crxpv[SCAN_NBP];
	char    rxds[SCAN_NBP][MAX_STRING_SIZE]; /* readback X description string */
	chid    crxds[SCAN_NBP];
	struct dbr_ctrl_double  rxeu[SCAN_NBP];/* readback X eng unit         */
	chid    crxeu[SCAN_NBP];
	double* pxra[SCAN_NBP];       /* readback X steps array               */
	chid    cpxra[SCAN_NBP];
	long    pxra_fpos[SCAN_NBP];
	double  rxcv[SCAN_NBP];     /* current readback value                 */
	chid    crxcv[SCAN_NBP];

	/*========================== DETECTORS ===============================*/

	short   dxnv[SCAN_NBD];   /* detector X valid PV                      */
	chid    cdxnv[SCAN_NBD];
	char    dxpv[SCAN_NBD][PVNAME_STRINGSZ]; /* detector X PV name        */
	chid    cdxpv[SCAN_NBD];
	char    dxds[SCAN_NBD][MAX_STRING_SIZE]; /* detector X description string */
	chid    cdxds[SCAN_NBD];
	struct dbr_ctrl_float  dxeu[SCAN_NBD];/* detector X eng unit          */
	chid    cdxeu[SCAN_NBD];
	float*  dxda[SCAN_NBD];   /* detector X steps array                   */
	chid    cdxda[SCAN_NBD];
	long    dxda_fpos[SCAN_NBD];
	float   dxcv[SCAN_NBD];   /* current readback value                   */
	chid    cdxcv[SCAN_NBD];

	/*========================== TRIGGERS ================================*/
	
	short txnv[SCAN_NBT];     /* trigger X valid pv                       */
	short txsc[SCAN_NBT];     /* if 0, trig is linked to a sscan record   */
	chid  ctxnv[SCAN_NBT];
	char  txpv[SCAN_NBT][PVNAME_STRINGSZ]; /* trigger X pv name           */
	chid  ctxpv[SCAN_NBT];
	float txcd[SCAN_NBT];     /* trigger X command                        */
	chid  ctxcd[SCAN_NBT];
	char  txpvRec[SCAN_NBT][PVNAME_STRINGSZ];	/* trigger X pv name minus field */

} SCAN;    /****** end of structure SCAN ******/


/************************************************************************/
/*---------------------- saveDataTask's message queue ------------------*/

#define MAX_MSG    1000 /* max # of messages in saveDataTask's queue    */
#define MAX_SIZE   80   /* max size in byte of the messages             */

#define MSG_SCAN_DATA  1  /* save scan                                  */
#define MSG_SCAN_NPTS  2  /* NPTS changed                               */
#define MSG_SCAN_PXNV  3  /* positioner pv name changed                 */
#define MSG_SCAN_PXSM  4  /* positioner step mode changed               */
#define MSG_SCAN_RXNV  5  /* readback pv name changed                   */
#define MSG_SCAN_DXNV  6  /* detector pv name changed                   */
#define MSG_SCAN_TXNV  7  /* trigger pv name changed                    */
#define MSG_SCAN_TXCD  8  /* trigger command changed                    */
#define MSG_SCAN_CPT   9  /* the current point changed                  */

#define MSG_DESC 10
#define MSG_EGU  11

#define MSG_FILE_SYSTEM 20
#define MSG_FILE_SUBDIR 21
#define MSG_REALTIME1D  22
#define MSG_FILE_BASENAME 23

/* Message structures */
typedef struct scan_short_msg {
	int   type;
	epicsTimeStamp time;
	SCAN* pscan;
	short val;
} SCAN_SHORT_MSG;

#define SCAN_SHORT_SIZE (sizeof(SCAN_SHORT_MSG)<MAX_SIZE? \
	sizeof(SCAN_SHORT_MSG):MAX_SIZE)

#define sendScanShortMsg(t, s, v) { \
	SCAN_SHORT_MSG msg; \
	msg.type= t; msg.pscan=s; msg.val= v;\
	epicsTimeGetCurrent(&(msg.time)); \
	epicsMessageQueueTrySend(msg_queue, (void *)&msg, \
	SCAN_SHORT_SIZE); }

#define sendScanShortMsgWait(t, s, v) { \
	SCAN_SHORT_MSG msg; \
	msg.type= t; msg.pscan=s; msg.val= v;\
	epicsTimeGetCurrent(&(msg.time)); \
	epicsMessageQueueSend(msg_queue, (void *)&msg, \
	SCAN_SHORT_SIZE); }

typedef struct scan_ts_short_msg {
	int   type;
	epicsTimeStamp time;
	SCAN* pscan;
	epicsTimeStamp stamp;
	short    val;
} SCAN_TS_SHORT_MSG;

#define SCAN_TS_SHORT_SIZE (sizeof(SCAN_TS_SHORT_MSG)<MAX_SIZE? \
	sizeof(SCAN_TS_SHORT_MSG):MAX_SIZE)

#define sendScanTSShortMsgWait(t, s, q, v) { \
	SCAN_TS_SHORT_MSG msg; \
	msg.type= t; msg.pscan=s; msg.val= v;\
	msg.stamp.secPastEpoch= q.secPastEpoch; \
	msg.stamp.nsec= q.nsec; \
	epicsTimeGetCurrent(&(msg.time)); \
	epicsMessageQueueSend(msg_queue, (void *)&msg, \
	SCAN_TS_SHORT_SIZE); }

typedef struct scan_long_msg {
	int   type;
	epicsTimeStamp time;
	SCAN* pscan;
	long val;
} SCAN_LONG_MSG;

#define SCAN_LONG_SIZE (sizeof(SCAN_LONG_MSG)<MAX_SIZE? \
	sizeof(SCAN_LONG_MSG):MAX_SIZE)

#define sendScanLongMsg(t, s, v) { \
	SCAN_LONG_MSG msg; \
	msg.type= t; msg.pscan=s; msg.val= v;\
	epicsTimeGetCurrent(&(msg.time)); \
	epicsMessageQueueTrySend(msg_queue, (void *)&msg, \
	SCAN_LONG_SIZE); }

#define sendScanLongMsgWait(t, s, v) { \
	SCAN_LONG_MSG msg; \
	msg.type= t; msg.pscan=s; msg.val= v;\
	epicsTimeGetCurrent(&(msg.time)); \
	epicsMessageQueueSend(msg_queue, (void *)&msg, \
	SCAN_LONG_SIZE); }

typedef struct scan_index_msg {
	int   type;
	epicsTimeStamp time;
	SCAN* pscan;
	int   index;
	double val;
} SCAN_INDEX_MSG;

#define SCAN_INDEX_SIZE (sizeof(SCAN_INDEX_MSG)<MAX_SIZE? \
	sizeof(SCAN_INDEX_MSG):MAX_SIZE)

#define sendScanIndexMsgWait(t,s,i,v) { \
	SCAN_INDEX_MSG msg; \
	msg.type=t; msg.pscan=s; msg.index=i; msg.val= (double)v; \
	epicsTimeGetCurrent(&(msg.time)); \
	epicsMessageQueueSend(msg_queue, (void *)&msg, \
	SCAN_INDEX_SIZE); }

typedef struct string_msg {
	int  type;
	epicsTimeStamp time;
	char* pdest;   /* specified as user arg in ca_create_subscription() call */
	char  string[MAX_STRING_SIZE];
} STRING_MSG;

#define STRING_SIZE (sizeof(STRING_MSG)<MAX_SIZE? \
	sizeof(STRING_MSG):MAX_SIZE)

#define sendStringMsgWait(t,d,s) { \
	STRING_MSG msg; \
	msg.type=t; msg.pdest=(char*)d; strncpy(msg.string, s, MAX_STRING_SIZE); \
	epicsTimeGetCurrent(&(msg.time)); \
	epicsMessageQueueSend(msg_queue, (void *)&msg, \
	STRING_SIZE); }

typedef struct integer_msg {
	int type;
	epicsTimeStamp time;
	int val;
} INTEGER_MSG;

#define INTEGER_SIZE (sizeof(INTEGER_MSG)<MAX_SIZE? \
	sizeof(INTEGER_MSG):MAX_SIZE)

#define sendIntegerMsgWait(t,v) { \
	INTEGER_MSG msg; \
	msg.type=t; msg.val=v; \
	epicsTimeGetCurrent(&(msg.time)); \
	epicsMessageQueueSend(msg_queue,(void *)&msg, \
	INTEGER_SIZE); }

/************************************************************************/
/*--------------------- list of scan to be saved -----------------------*/

typedef struct scan_node {
	SCAN              scan;
	struct scan_node* nxt;
} SCAN_NODE;

typedef union db_access_val DBR_VAL;

typedef struct pv_node {
	epicsMutexId   lock;
	chid     channel;
	chid     desc_chid;
	char     name[PVNAME_STRINGSZ];
	char     desc[MAX_STRING_SIZE];   /* note DESC size is 30, but the description
	                                   can also be read from the req file. */
	int      dbr_type;
	long     count;
	DBR_VAL* pval;
	struct pv_node* nxt;
} PV_NODE;


LOCAL char  req_file[40];
LOCAL char  req_macros[40];

LOCAL char  server_pathname[200];
LOCAL char* server_subdir;
LOCAL char  local_pathname[200];
LOCAL char* local_subdir;

#define STATUS_INACTIVE         0
#define STATUS_ACTIVE_OK        1
#define STATUS_ACTIVE_FS_ERROR  2
#define STATUS_ERROR            3
LOCAL chid  save_status_chid;
LOCAL short save_status= STATUS_INACTIVE;
LOCAL chid  message_chid;
LOCAL chid  filename_chid;
LOCAL chid  full_pathname_chid;


#define FS_NOT_MOUNTED  0
#define FS_MOUNTED      1
LOCAL chid  file_system_chid;
LOCAL chid  file_system_disp_chid;
LOCAL int   file_system_state= FS_NOT_MOUNTED;

LOCAL chid  file_subdir_chid;
LOCAL chid  file_subdir_disp_chid;

LOCAL chid  file_basename_chid = NULL;
LOCAL chid  file_basename_disp_chid = NULL;

LOCAL int   realTime1D= 1;
LOCAL chid  realTime1D_chid;

LOCAL long  counter;  /* data file counter*/
LOCAL chid  counter_chid;
LOCAL char  ioc_prefix[PREFIX_SIZE];
LOCAL char  scanFile_basename[BASENAME_SIZE] = "";

/* file-write retries */
LOCAL chid  maxAllowedRetries_chid;
long maxAllowedRetries = 5;
LOCAL chid  totalRetries_chid;
long totalRetries = 0;
LOCAL chid  currRetries_chid;
long currRetries = 0;
LOCAL chid  retryWaitInSecs_chid;
long retryWaitInSecs = 20;
LOCAL chid  abandonedWrites_chid;
long abandonedWrites = 0;

LOCAL epicsThreadId			threadId=NULL;  /* saveDataTask thread id */
LOCAL epicsMessageQueueId	msg_queue=NULL; /* saveDataTask's message queue */

LOCAL double       cpt_wait_time;
LOCAL int          nb_scan_running=0; /* # of scans currently running */
LOCAL SCAN_NODE*   list_scan=NULL;    /* list of scan to be saved */
LOCAL PV_NODE*     list_pv=NULL;      /* list of pvs to be saved with each scan */
LOCAL int          nb_extraPV=0;

/***********************/
/* function prototypes */

LOCAL int connectScan(char* name, char* handShake, char* autoHandShake);
LOCAL int disconnectScan(SCAN* pscan);
LOCAL int monitorScan(SCAN* pscan, int pass);
LOCAL void monitorScans();
LOCAL SCAN* searchScan(char* name);
LOCAL int  scan_getDim(SCAN* pscan);
LOCAL void infoScan(SCAN* pscan);
LOCAL void dataMonitor(struct event_handler_args eha);
LOCAL void nptsMonitor(struct event_handler_args eha);
LOCAL void cptMonitor(struct event_handler_args eha);
LOCAL void pxnvMonitor(struct event_handler_args eha);
LOCAL void pxsmMonitor(struct event_handler_args eha);
LOCAL void rxnvMonitor(struct event_handler_args eha);
LOCAL void dxnvMonitor(struct event_handler_args eha);
LOCAL void txnvMonitor(struct event_handler_args eha);
LOCAL void txcdMonitor(struct event_handler_args eha);
LOCAL int saveDataTask(void *parm);
LOCAL void remount_file_system(char* filesystem);



/*----------------------------------------------------------------------*/
/* test the status of a file                                            */
/* return OK if the file exists                                         */
/*   ERROR otherwise                                                    */
/*                                                                      */
LOCAL int fileStatus(char* fname)
{
	struct stat status;
	int retVal;

	errno = 0;
	retVal = stat(fname, &status);
	if ((retVal == -1) && (debug_saveData)) {
		printf("saveData: stat returned -1 for filename '%s'; errno=%d\n", fname, errno);
	}
	return retVal;
}

/*----------------------------------------------------------------------*/
/* test for Read/Write permission in a directory                        */
/* path: the pathname                                                   */
/* return OK if R/W allowed                                             */
/*   ERROR otherwise                                                    */
/*                                                                      */
LOCAL int checkRWpermission(char* path) {
	/* Quick and dirty way to check for R/W permission */
	int  file;
	char tmpfile[100];

	strcpy(tmpfile, path);
	strcat(tmpfile, "/rix_");

	while (fileStatus(tmpfile)==OK && strlen(tmpfile)<100) {
		strcat(tmpfile, "_");
	}

	if (fileStatus(tmpfile)==OK) {
		return ERROR;
	}

	file= creat(tmpfile, O_RDWR);

	if (fileStatus(tmpfile)!=OK) {
		return ERROR;
	}

	close(file);
	remove(tmpfile);

	return OK;
}

LOCAL void sendUserMessage(char* msg) {
	/* printf("%s\n", msg); */
	if (message_chid) {
		ca_array_put(DBR_STRING, 1, message_chid, msg);
	}
}



void saveData_Init(char* fname, char* macros)
{
	if (msg_queue==NULL) {
        strncpy(req_file, fname, 39);
        strncpy(req_macros, macros, 39);

		msg_queue = epicsMessageQueueCreate(MAX_MSG, MAX_SIZE);
		if (msg_queue==NULL) {
			Debug0(1, "Unable to create message queue\n");
			return;
		}
		printf("saveData: message queue created\n");

		threadId = epicsThreadCreate("saveDataTask", PRIORITY,
			epicsThreadGetStackSize(epicsThreadStackBig),
			(EPICSTHREADFUNC)saveDataTask, (void *)epicsThreadGetIdSelf());

		if (threadId==NULL) {
			Debug0(1, "Unable to create saveDataTask\n");
			epicsMessageQueueDestroy(msg_queue);
			return;
		} else {
			epicsThreadSuspendSelf();
		}
	}
    else
        printf("saveData already initialized\n");
    
	return;
}


/************************************************************************/
/*                   DIAGNOSTIC FUNCTIONS                               */

void saveData_PrintScanInfo(char* name)
{
	SCAN* pscan;

	pscan= searchScan(name);
	if (pscan) {
		infoScan(pscan);
	}
}

void saveData_Priority(int p)
{
	epicsThreadSetPriority(threadId, p);
}

void saveData_SetCptWait_ms(int ms)
{
	cpt_wait_time = (double) (ms/1000);
}

void saveData_Version()
{
	printf("saveData Version: %s\n", save_data_version);
}

void saveData_CVS() 
{
	printf("saveData CVS: $Id: saveData.c,v 1.46 2009-10-15 19:23:02 mooney Exp $\n");
}

void saveData_Info() {
	SCAN_NODE* pnode;
	SCAN* scan;
	SCAN* cur;

	pnode= list_scan;
	printf("saveData: scan info:\n");
	while (pnode) {
		scan= &pnode->scan;
		printf("scan   : %s\n", scan->name);
		printf("  rank : %d\n", scan_getDim(scan));
		printf("  links:");
		cur= scan;
		while (cur) {
			printf(cur->name);
			cur= cur->nxt;
			if (cur) printf("->");
		}
		printf("\n");
		pnode= pnode->nxt;
	}
}

/************************************************************************/
/*                        IMPLEMENTATION                                */

/*----------------------------------------------------------------------*/
/* Try to establish the connections with a scanRecord, allocate         */
/* memory for the buffers and add it to the list of scans               */
/* name: the name of the scan record.                                   */
/* return 0 if successful.                                              */
/*        -1 otherwise.                                                 */
LOCAL int connectScan(char* name, char* handShake, char* autoHandShake)
{
	SCAN_NODE* pnode;
	SCAN_NODE* pcur;
	SCAN* pscan;
	int   i, ok, nc;
	char  pvname[80];
	char* field;
	short shortData;

	Debug1(1, "connectScan(%s)...\n", name);

	pnode= (SCAN_NODE*) malloc(sizeof(SCAN_NODE));
	if (!pnode) {
		printf("saveData: Not enough space to connect %s\n", name);
		return -1;
	}
	pnode->nxt= NULL;
	pscan= &pnode->scan;

	/* initialize critical fields */
	memset((void*)pscan, 0, sizeof(SCAN));

	pscan->data= -1;

	pscan->first_scan= TRUE;
	pscan->scan_dim= 1;
	strncpy(pscan->name, name, PVNAME_STRINGSZ-1);
	pscan->name[PVNAME_STRINGSZ-1]='\0';
	pscan->nxt= NULL;
	epicsTimeGetCurrent(&(pscan->cpt_time));
	pscan->cpt_monitored= FALSE;


	/* try to connect to the hand shake variable */
	pscan->chandShake= NULL;
	if (handShake && (*handShake!='\0')) {
		ca_search(handShake, &(pscan->chandShake));
		ca_pend_io(1.0);
	}

	/* try to connect to the auto hand shake variable */
	pscan->cautoHandShake= NULL;
	if (autoHandShake && (*autoHandShake!='\0')) {
		ca_search(autoHandShake, &(pscan->cautoHandShake));
		ca_pend_io(1.0);
	}
	shortData = 1;
	ca_array_put(DBR_SHORT, 1, pscan->cautoHandShake, &shortData);


	/* try to connect to the scan record */
	strcpy(pvname, pscan->name);
	strcat(pvname, ".");
	field= &pvname[strlen(pvname)];

	strcpy(field, "DATA");
	ca_search_and_connect(pvname, &(pscan->cdata), NULL, (void*)pscan);

	strcpy(field, "MPTS");
	ca_search_and_connect(pvname, &(pscan->cmpts), NULL, (void*)pscan);

	strcpy(field, "NPTS");
	ca_search_and_connect(pvname, &(pscan->cnpts), NULL, (void*)pscan);

	strcpy(field, "CPT");
	ca_search_and_connect(pvname, &(pscan->ccpt), NULL, (void*)pscan);

	strcpy(field, "BCPT");
	ca_search_and_connect(pvname, &(pscan->cbcpt), NULL, (void*)pscan);

	if (ca_pend_io(5.0)!= ECA_NORMAL) {
		printf("saveData: Unable to connect to %s\n", pscan->name);
		disconnectScan(pscan);
		free(pnode);
		return -1;
	}


	/*---------------------- POSITIONERS & READBACKS ---------------------*/
	for (i=0; i<SCAN_NBP; i++) {
		pscan->pxnv[i]= XXNV_NOPV;
		strcpy(field, pxnv[i]);
		ca_search_and_connect(pvname, &(pscan->cpxnv[i]), NULL, (void*)pscan);
		strcpy(field, pxpv[i]);
		ca_search_and_connect(pvname, &(pscan->cpxpv[i]), NULL, (void*)pscan);
		strcpy(field, pxsm[i]);
		ca_search_and_connect(pvname, &(pscan->cpxsm[i]), NULL, (void*)pscan);
		pscan->rxnv[i]= XXNV_NOPV;
		strcpy(field, rxnv[i]);
		ca_search_and_connect(pvname, &(pscan->crxnv[i]), NULL, (void*)pscan);
		strcpy(field, rxpv[i]);
		ca_search_and_connect(pvname, &(pscan->crxpv[i]), NULL, (void*)pscan);
		strcpy(field, pxra[i]);
		ca_search_and_connect(pvname, &(pscan->cpxra[i]), NULL, (void*)pscan);
		strcpy(field, rxcv[i]);
		ca_search_and_connect(pvname, &(pscan->crxcv[i]), NULL, (void*)pscan);
	}

	/*------------------------- DETECTORS --------------------------------*/
	for (i=0; i<SCAN_NBD; i++) {
		pscan->dxnv[i]= XXNV_NOPV;
		strcpy(field, dxnv[i]);
		ca_search_and_connect(pvname, &(pscan->cdxnv[i]), NULL, (void*)pscan);
		strcpy(field, dxpv[i]);
		ca_search_and_connect(pvname, &(pscan->cdxpv[i]), NULL, (void*)pscan);
		strcpy(field, dxda[i]);
		ca_search_and_connect(pvname, &(pscan->cdxda[i]), NULL, (void*)pscan);
		strcpy(field, dxcv[i]);
		ca_search_and_connect(pvname, &(pscan->cdxcv[i]), NULL, (void*)pscan);
	}

	/*------------------------- TRIGGERS ---------------------------------*/
	for (i=0; i<SCAN_NBT; i++) {
		pscan->txnv[i]= XXNV_NOPV;
		pscan->txsc[i]= 1;	/* presume NOT linked to another sscan record */
		strcpy(field, txnv[i]);
		ca_search_and_connect(pvname, &(pscan->ctxnv[i]), NULL, (void*)pscan);
		strcpy(field, txpv[i]);
		ca_search_and_connect(pvname, &(pscan->ctxpv[i]), NULL, (void*)pscan);
		strcpy(field, txcd[i]);
		ca_search_and_connect(pvname, &(pscan->ctxcd[i]), NULL, (void*)pscan);
	}

	if (ca_pend_io(5.0)!= ECA_NORMAL) {
		nc=0;
		for (i=0; i<SCAN_NBP; ++i) {
			if (ca_state(pscan->cpxnv[i])!=cs_conn ||
					ca_state(pscan->cpxpv[i])!=cs_conn ||
					ca_state(pscan->cpxsm[i])!=cs_conn ||
					ca_state(pscan->crxnv[i])!=cs_conn ||
					ca_state(pscan->crxpv[i])!=cs_conn ||
					ca_state(pscan->cpxra[i])!=cs_conn ||
					ca_state(pscan->crxcv[i])!=cs_conn) {
				++nc;
				ca_clear_channel(pscan->cpxnv[i]);
				pscan->cpxnv[i]=NULL;
				pscan->pxnv[i]= XXNV_NOPV;
				ca_clear_channel(pscan->cpxpv[i]);
				pscan->cpxpv[i]= NULL;
				pscan->pxpv[i][0]= '\0';
				ca_clear_channel(pscan->cpxsm[i]);
				pscan->cpxsm[i]= NULL;
				pscan->pxsm[i][0]= '\0';
				ca_clear_channel(pscan->crxnv[i]);
				pscan->crxnv[i]= NULL;
				pscan->rxnv[i]= XXNV_NOPV;
				ca_clear_channel(pscan->crxpv[i]);
				pscan->crxpv[i]= NULL;
				pscan->rxpv[i][0]= '\0';
				ca_clear_channel(pscan->cpxra[i]);
				pscan->cpxra[i]= NULL;
				pscan->pxra[i]= NULL;
				ca_clear_channel(pscan->crxcv[i]);
				pscan->crxcv[i]= NULL;
			}
		}
		if (nc>0) printf("saveDataTask warning: %s: %d positioner(s) not connected\n", pscan->name, nc);

		nc=0;
		for (i=0; i<SCAN_NBD; ++i) {
			if (ca_state(pscan->cdxnv[i])!=cs_conn ||
					ca_state(pscan->cdxpv[i])!=cs_conn ||
					ca_state(pscan->cdxda[i])!=cs_conn ||
					ca_state(pscan->cdxcv[i])!=cs_conn) {
				++nc;
				ca_clear_channel(pscan->cdxnv[i]);
				pscan->cdxnv[i]=NULL;
				pscan->dxnv[i]= XXNV_NOPV;
				ca_clear_channel(pscan->cdxpv[i]);
				pscan->cdxpv[i]= NULL;
				pscan->dxpv[i][0]= '\0';
				ca_clear_channel(pscan->cdxda[i]);
				pscan->cdxda[i]= NULL;
				pscan->dxda[i]= NULL;
				ca_clear_channel(pscan->cdxcv[i]);
				pscan->cdxcv[i]= NULL;
			}
		}
		if (nc>0) printf("saveDataTask warning: %s: %d detector(s) not connected\n", pscan->name, nc);

		nc=0;
		for (i=0; i<SCAN_NBT; ++i) {
			if (ca_state(pscan->ctxnv[i])!=cs_conn ||
					ca_state(pscan->ctxpv[i])!=cs_conn ||
					ca_state(pscan->ctxcd[i])!=cs_conn) {
				++nc;
				ca_clear_channel(pscan->ctxnv[i]);
				pscan->ctxnv[i]=NULL;
				pscan->txnv[i]= XXNV_NOPV;
				ca_clear_channel(pscan->ctxpv[i]);
				pscan->ctxpv[i]= NULL;
				pscan->txpv[i][0]= '\0';
				pscan->txpvRec[i][0]= '\0';
				ca_clear_channel(pscan->ctxcd[i]);
				pscan->ctxcd[i]= NULL;
			}
		}
		if (nc>0) printf("saveDataTask warning: %s: %d trigger(s) not connected\n", pscan->name, nc);
	}
	
	/* get the max number of points of the scan to allocate the buffers */
	if (ca_array_get(DBR_LONG, 1, pscan->cmpts, &(pscan->mpts))!=ECA_NORMAL) {
		if (ca_pend_io(5.0)==ECA_TIMEOUT) {
			printf("saveData: %s: Unable to read MPTS. Aborting connection", pscan->name);
			disconnectScan(pscan);
			free(pnode);
			return -1;
		}
	}

	/* Try to allocate memory for the buffers */
	ok= 1;
	Debug2(1, "connectScan(%s): allocating %ld pts for positioners\n", name, pscan->mpts);
	for (i=0; ok && i<SCAN_NBP; i++) {
		Debug3(3, "connectScan(%s): allocating %ld pts for positioner %d\n",
			name, pscan->mpts, i);
		if (pscan->cpxra[i]!=NULL) {
			ok= (pscan->pxra[i]= (double*) calloc(pscan->mpts, sizeof(double)))!= NULL;
		}
	}

	Debug2(1, "connectScan(%s): allocating %ld pts for detectors\n", name, pscan->mpts);
	for (i=0; ok && i<SCAN_NBD; i++) {
#if ALLOC_ALL_DETS
		if (pscan->cdxda[i]!=NULL) {
			ok= (pscan->dxda[i]= (float*) calloc(pscan->mpts, sizeof(float)))!= NULL;
		}
#else
		/* Don't allocate array space until sscan record has valid link for det */
		if ((pscan->cdxda[i]!=NULL) && (pscan->dxnv[i]==XXNV_OK)) {
			ok = (pscan->dxda[i] = (float*)calloc(pscan->mpts, sizeof(float)))!= NULL;
		}
#endif
	}

	if (!ok) {
		printf("saveData: %s: Memory allocation failed\n", pscan->name);
		disconnectScan(pscan);
		free(pnode);
		return -1;
	}
	
	if (!list_scan) {
		list_scan= pnode;
	} else {
		pcur= list_scan;
		while (pcur->nxt!=NULL) pcur= pcur->nxt;
		pcur->nxt= pnode;
	}

	Debug1(1, "connectScan(%s) OK\n", pscan->name);

	return 0;
}


/*----------------------------------------------------------------------*/
/* Disconnect a SCAN from the scan record:                              */
/* Clear all ca connections and events and free all buffers.            */
/* pscan: a pointer to a SCAN structure                                 */
/* return 0 if successful                                               */
/*       -1 otherwise                                                   */
LOCAL int disconnectScan(SCAN* pscan)
{
	int i;
	short shortData;

	Debug1(1, "disconnectScan(%s)...\n", pscan->name);
	if ((pscan == NULL) || (pscan->name[0] == 0)) return(-1);
	if (pscan->chandShake) ca_clear_channel(pscan->chandShake);
	if (pscan->cautoHandShake) {
		Debug1(2, "Clear autoHandshake %s\n", ca_name(pscan->cautoHandShake));
		shortData = 0;
		ca_array_put(DBR_SHORT, 1, pscan->cautoHandShake, &shortData);
		ca_clear_channel(pscan->cautoHandShake);
	}

	/* Disconnect fields */
	if (pscan->cdata) ca_clear_channel(pscan->cdata);
	if (pscan->cmpts) ca_clear_channel(pscan->cmpts);
	if (pscan->cnpts) ca_clear_channel(pscan->cnpts);
	if (pscan->ccpt) ca_clear_channel(pscan->ccpt);
	if (pscan->cbcpt) ca_clear_channel(pscan->cbcpt);

	for (i=0; i<SCAN_NBP; i++) {
		if (pscan->cpxnv[i]) ca_clear_channel(pscan->cpxnv[i]);
		if (pscan->cpxpv[i]) ca_clear_channel(pscan->cpxpv[i]);
		if (pscan->cpxds[i]) ca_clear_channel(pscan->cpxds[i]);
		if (pscan->cpxeu[i]) ca_clear_channel(pscan->cpxeu[i]);
		if (pscan->cpxsm[i]) ca_clear_channel(pscan->cpxsm[i]);

		if (pscan->crxnv) ca_clear_channel(pscan->crxnv[i]);
		if (pscan->crxpv[i]) ca_clear_channel(pscan->crxpv[i]);
		if (pscan->crxds[i]) ca_clear_channel(pscan->crxds[i]);
		if (pscan->crxeu[i]) ca_clear_channel(pscan->crxeu[i]);
		if (pscan->cpxra[i]) ca_clear_channel(pscan->cpxra[i]);
		if (pscan->crxcv[i]) ca_clear_channel(pscan->crxcv[i]);
	}

	for (i=0; i<SCAN_NBD; i++) {
		if (pscan->cdxnv[i]) ca_clear_channel(pscan->cdxnv[i]);
		if (pscan->cdxpv[i]) ca_clear_channel(pscan->cdxpv[i]);
		if (pscan->cdxds[i]) ca_clear_channel(pscan->cdxds[i]);
		if (pscan->cdxeu[i]) ca_clear_channel(pscan->cdxeu[i]);
		if (pscan->cdxda[i]) ca_clear_channel(pscan->cdxda[i]);
		if (pscan->cdxcv[i]) ca_clear_channel(pscan->cdxcv[i]);
	}

	for (i=0; i<SCAN_NBT; i++) {
		if (pscan->ctxnv[i]) ca_clear_channel(pscan->ctxnv[i]);
		if (pscan->ctxpv[i]) ca_clear_channel(pscan->ctxpv[i]);
		if (pscan->ctxcd[i]) ca_clear_channel(pscan->ctxcd[i]);
	}

	/* free buffers */
	for (i=0; i<SCAN_NBP; i++) {
		if (pscan->pxra[i]) free(pscan->pxra[i]);
	}

	for (i=0; i<SCAN_NBD; i++) {
		if (pscan->dxda[i]) free(pscan->dxda[i]);
	}

	Debug1(1, "disconnectScan(%s) done\n", pscan->name);
	
	return 0;
}



/*----------------------------------------------------------------------*/
/* start the monitoring of a scan record.                               */
/* pscan: a pointer to the SCAN structure connected to the scan record. */
/* return 0 if all monitors have been successfuly added.                */
/*        -1 otherwise.                                                 */
LOCAL int monitorScan(SCAN* pscan, int pass)
{
	int i;

	if (pscan == NULL) return(-1);
	Debug1(1, "monitorScan(%s)...\n", pscan->name);
	if (pscan->name[0] == 0) return(-1);

	if (pass==0) {
		if (ca_add_event(DBR_TIME_SHORT, pscan->cdata, 
				dataMonitor, (void*)NULL, 0)!=ECA_NORMAL) {
			Debug1(2, "Unable to monitor %s\n", ca_name(pscan->cdata));
			return -1;
		}
		return 0;
	}

	if (pscan->cnpts == NULL) {
		Debug1(2, "Unable to monitor %s npts field\n", pscan->name);
		return -1;
	}
	if (ca_add_event(DBR_LONG, pscan->cnpts, 
			nptsMonitor, (void*)NULL, 0)!=ECA_NORMAL) {
		Debug1(2, "Unable to monitor %s\n", ca_name(pscan->cnpts));
		return -1;
	}
	
	for (i=0; i<SCAN_NBP; i++) { 
		if (pscan->cpxnv[i]!=NULL && pscan->cpxsm[i]!=NULL && pscan->crxnv[i]!=NULL) {
			if (ca_add_event(DBR_SHORT, pscan->cpxnv[i], pxnvMonitor, 
					(void*)(long)i, 0)!=ECA_NORMAL) {
				Debug1(2, "Unable to monitor %s\n", ca_name(pscan->cpxnv[i]));
				return -1;
			}
			if (ca_add_event(DBR_STRING, pscan->cpxsm[i], pxsmMonitor, 
					(void*)pscan->pxsm[i], 0)!=ECA_NORMAL) {
				Debug1(2, "Unable to monitor %s\n", ca_name(pscan->cpxsm[i]));
				return -1;
			}
			if (ca_add_event(DBR_SHORT, pscan->crxnv[i], rxnvMonitor, 
					(void*)(long)i, 0)!=ECA_NORMAL) {
				Debug1(2, "Unable to monitor %s\n", ca_name(pscan->crxnv[i]));
				return -1;
			}
		} else {
			Debug2(2, "Unable to monitor %s positioner %d\n", pscan->name, i);
			return -1;
		}
	}
	
	for (i=0; i<SCAN_NBD; i++) {
		if (pscan->cdxnv[i]!=NULL) {
			if (ca_add_event(DBR_SHORT, pscan->cdxnv[i], dxnvMonitor, 
					(void*)(long)i, 0)!=ECA_NORMAL) {
				Debug1(2, "Unable to monitor %s\n", ca_name(pscan->cdxnv[i]));
				return -1;
			}
		} else {
			Debug2(2, "Unable to monitor %s detector %d\n", pscan->name, i);
			return -1;
		}
	}

	for (i=0; i<SCAN_NBT; i++) {
		if (pscan->ctxnv[i]!=NULL && pscan->ctxcd[i]!=NULL) {
			if (ca_add_event(DBR_SHORT, pscan->ctxnv[i], txnvMonitor, 
					(void*)(long)i, 0)!=ECA_NORMAL) {
				Debug1(2, "Unable to monitor %s\n", ca_name(pscan->ctxnv[i]));
				return -1;
			}
			if (ca_add_event(DBR_FLOAT, pscan->ctxcd[i], txcdMonitor, 
					(void*)(long)i, 0)!=ECA_NORMAL) {
				Debug1(2, "Unable to monitor %s\n", ca_name(pscan->ctxcd[i]));
				return -1;
			}
		} else {
			Debug2(2, "Unable to monitor %s trigger %d\n", pscan->name, i);
			return -1;
		}
	}

	Debug1(1, "monitorScan(%s) OK\n", pscan->name);

	return 0;
}



LOCAL void updateScan(SCAN* pscan)
{
	int i;

	if ((pscan == NULL) || (pscan->name[0] == 0)) return;

	Debug1(2, "updateScan:entry for '%s'\n", pscan->name);
	pscan->nxt=0;
	for (i=0; i<SCAN_NBT; i++) {
		if (pscan->txsc[i]==0 && pscan->txcd[i]!=0) {
			/* we're linked to another sscan record, and the link will cause that record to start a scan */
			Debug2(2, "updateScan:%s: calling searchScan(%s)\n",
				pscan->name, pscan->txpvRec[i]);
			/*
			 * Is the sscan record we're linked to in our list of sscan records to monitor?   If so,
			 * we'll receive it's SCAN* pointer; else, we'll receive NULL. 
			 */
			pscan->nxt= searchScan(pscan->txpvRec[i]);
			/* If we have a SCAN* pointer, stop looking for one. */
			if (pscan->nxt) break;
		}
	}
	if (!(pscan->nxt) && (realTime1D==0)) {
		/*
		 * We're not monitoring an inner scan, and we're not writing data point-by-point, so we don't
		 * want to receive monitor events from this sscan record's .CPT field.
		 */
		if (pscan->cpt_monitored==TRUE) {
			Debug2(2, "updateScan:%s: clear .CPT subscription (cpt_evid = %p)\n", pscan->name, pscan->cpt_evid);
			if (pscan->cpt_evid) ca_clear_subscription(pscan->cpt_evid);
			pscan->cpt_monitored= FALSE;
		}
	} else {
		/* Make sure we will receive .CPT monitors. */
		if (pscan->cpt_monitored==FALSE) {
			Debug1(2, "updateScan:%s: subscribe to .CPT\n", pscan->name);
			if (ca_create_subscription(DBR_LONG, 1, pscan->ccpt, DBE_VALUE, cptMonitor, NULL, &pscan->cpt_evid) == ECA_NORMAL) {
				Debug2(2, "updateScan:%s: cpt_evid=%p\n", pscan->name, pscan->cpt_evid);
				pscan->cpt_monitored=TRUE;
			} else {
				Debug1(2, "Unable to monitor %s\n", ca_name(pscan->ccpt));
			}
		}
	}
	Debug1(2, "updateScan:exit cpt_monitored = %d\n", pscan->cpt_monitored);
}

LOCAL void updateScans()
{
	SCAN_NODE* pnode;

	pnode= list_scan;
	while (pnode) {
		updateScan(&pnode->scan);
		pnode= pnode->nxt;
	}
}


/*----------------------------------------------------------------------*/
/* monitor all SCAN of the list.                                        */
LOCAL void monitorScans()
{
	SCAN_NODE* pnode;

	/*
	 * Make two passes through list of sscan records.  On first pass, monitor
	 * only DATA fields.  This ensures that all DATA fields will be in the same
	 * event-queue segment, which in turn ensures that DATA events will be
	 * ordered correctly in time, at least with respect to other DATA events,
	 * regardless of when the event queue is read, and regardless of any
	 * discarded events.  
	 */
	pnode = list_scan;
	while (pnode) {
		monitorScan(&pnode->scan, 0);
		pnode = pnode->nxt;
	}
	pnode = list_scan;
	while (pnode) {
		monitorScan(&pnode->scan, 1);
		pnode = pnode->nxt;
	}
	updateScans();
}

/*----------------------------------------------------------------------*/
/* search for a scan in the SCAN list.                                  */
/* name: the name of the scan to look for.                              */
/* return a pointer to the SCAN structure if found                      */
/*        NULL otherwise.                                               */
LOCAL SCAN* searchScan(char* name)
{
	SCAN_NODE* current;

	Debug1(2,"searchScan '%s'\n", name);
	current= list_scan;
	while (current) {
		if (strcmp(current->scan.name, name)==0) {
			return &current->scan;
		}
		current= current->nxt;
	}
	return NULL;
}


/*----------------------------------------------------------------------*/
/* print debug info on a SCAN structure.                                */

LOCAL char* cs[]= {
	"valid chid, IOC not found                 ",
	"valid chid, IOC was found but unavailable ",
	"valid chid, IOC was found, still available",
	"invalid chid                              "
};

LOCAL void infoScan(SCAN* pscan)
{
	int i;

	if ((pscan == NULL) || (pscan->name[0] == 0)) return;

	printf("scan name: %s\n", pscan->name);

	if (pscan->chandShake) {
		printf("hand shake: %s\n", ca_name(pscan->chandShake));
	} else {
		printf(" No hand shake\n");
	}

	if (pscan->cautoHandShake) {
		printf("auto hand shake: %s\n", ca_name(pscan->cautoHandShake));
	} else {
		printf(" No auto hand shake\n");
	}

	printf("%s.DATA[%s]= %d\n", pscan->name, 
		cs[ca_state(pscan->cdata)], pscan->data);
	printf("%s.MPTS[%s]= %ld\n", pscan->name,
		cs[ca_state(pscan->cmpts)], pscan->mpts);
	printf("%s.NPTS[%s]= %ld\n", pscan->name,
		cs[ca_state(pscan->cnpts)], pscan->npts);
	printf("%s.CPT [%s]= %ld\n", pscan->name,
		cs[ca_state(pscan->ccpt)], pscan->cpt);
	printf("%s.BCPT [%s]= %ld\n", pscan->name,
		cs[ca_state(pscan->cbcpt)], pscan->bcpt);

	for (i=0; i<SCAN_NBP; i++)
		if (pscan->cpxnv[i]!=NULL) printf("%s.%s[%s]= %d\n", pscan->name, pxnv[i], cs[ca_state(pscan->cpxnv[i])], pscan->pxnv[i]);
	for (i=0; i<SCAN_NBP; i++) {
		if (pscan->cpxpv[i]!=NULL) {
			printf("%s.%s[%s]= %s\n", pscan->name, pxpv[i], cs[ca_state(pscan->cpxpv[i])], pscan->pxpv[i]);
			printf("  DESC: %s\n", pscan->pxds[i]);
			printf("  EGU : %s\n", pscan->pxeu[i].units);
		}
	}
	for (i=0; i<SCAN_NBP; i++)
		if (pscan->cpxsm[i]!=NULL) printf("%s.%s[%s]= %s\n", pscan->name, pxsm[i], cs[ca_state(pscan->cpxsm[i])], pscan->pxsm[i]);
	for (i=0; i<SCAN_NBP; i++)
		if (pscan->crxnv[i]!=NULL) printf("%s.%s[%s]= %d\n", pscan->name, rxnv[i], cs[ca_state(pscan->crxnv[i])], pscan->rxnv[i]);
	for (i=0; i<SCAN_NBP; i++) {
		if (pscan->crxpv[i]!=NULL) {
			printf("%s.%s[%s]= %s\n", pscan->name, rxpv[i],
				cs[ca_state(pscan->crxpv[i])], pscan->rxpv[i]);
			printf("  DESC: %s\n", pscan->rxds[i]);
			printf("  EGU : %s\n", pscan->rxeu[i].units);
		}
	}
	for (i=0; i<SCAN_NBP; i++)
		if (pscan->cpxra[i]!=NULL) printf("%s.%s[%s]= %p\n", pscan->name, pxra[i], cs[ca_state(pscan->cpxra[i])], pscan->pxra[i]);
	for (i=0; i<SCAN_NBP; i++)
		if (pscan->crxcv[i]!=NULL) printf("%s.%s[%s]= %f\n", pscan->name, rxcv[i], cs[ca_state(pscan->crxcv[i])], pscan->rxcv[i]);

	
	for (i=0; i<SCAN_NBD; i++)
		if (pscan->cdxnv[i]!=NULL) printf("%s.%s[%s]= %d\n", pscan->name, dxnv[i], cs[ca_state(pscan->cdxnv[i])], pscan->dxnv[i]);
	for (i=0; i<SCAN_NBD; i++) {
		if (pscan->cdxpv[i]!=NULL) {
			printf("%s.%s[%s]= %s\n", pscan->name, dxpv[i],
				cs[ca_state(pscan->cdxpv[i])], pscan->dxpv[i]);
			printf("  DESC: %s\n", pscan->dxds[i]);
			printf("  EGU : %s\n", pscan->dxeu[i].units);
		}
	}
	for (i=0; i<SCAN_NBD; i++)
		if (pscan->cdxda[i]!=NULL) printf("%s.%s[%s] (%s)\n", pscan->name, dxda[i], cs[ca_state(pscan->cdxda[i])], pscan->dxda[i]?"allocated":"not allocated");
	for (i=0; i<SCAN_NBD; i++)
		if (pscan->cdxcv[i]!=NULL) printf("%s.%s[%s]= %f\n", pscan->name, dxcv[i], cs[ca_state(pscan->cdxcv[i])], pscan->dxcv[i]);
	
	for (i=0; i<SCAN_NBT; i++)
		if (pscan->ctxnv[i]!=NULL) printf("%s.%s[%s]= %d\n", pscan->name, txnv[i], cs[ca_state(pscan->ctxnv[i])], pscan->txnv[i]);
	for (i=0; i<SCAN_NBT; i++)
		if (pscan->ctxpv[i]!=NULL) printf("%s.%s[%s]= %s\n", pscan->name, txpv[i], cs[ca_state(pscan->ctxpv[i])], pscan->txpv[i]);
	for (i=0; i<SCAN_NBT; i++)
		if (pscan->ctxcd[i]!=NULL) printf("%s.%s[%s]= %f\n", pscan->name, txcd[i], cs[ca_state(pscan->ctxcd[i])], pscan->txcd[i]);
}


/*----------------------------------------------------------------------*/
/* DATA field monitor.                                                  */
/*                                                                      */
LOCAL void dataMonitor(struct event_handler_args eha)
{
	struct dbr_time_short *pval;
	SCAN* pscan;
	short newData, sval;
	char  disp;
	/* diagnostics */
	epicsTimeStamp currtime;
	char currtimestr[MAX_STRING_SIZE]; 

	epicsTimeGetCurrent(&currtime);
	pscan= (SCAN*)ca_puser(eha.chid);

if (pscan->nxt) {
	pscan->nxt->first_scan = FALSE;
	pscan->nxt->scan_dim = pscan->scan_dim-1;
}
	pval = (struct dbr_time_short *) eha.dbr;
	sval = pval->value;
	Debug2(5,"dataMonitor(%s): (DATA=%d)\n", pscan->name, sval);

	if (pscan->data != -1) {
		if (sval == 1) {
			/* hand shaking notify */
			if (pscan->chandShake) {
				newData = HANDSHAKE_BUSY;
				/* printf("dataMonitor: putting %d to %s.AWAIT\n", newData, pscan->name); */
				ca_array_put(DBR_SHORT, 1, pscan->chandShake, &newData);
			}
		}
		if ((sval==0) && (nb_scan_running++ == 0)) {
			/* new scan started: disable put to filesystem and subdir */
			disp = (char)1;
			if (file_system_disp_chid) ca_array_put(DBR_CHAR, 1, file_system_disp_chid, &disp);
			disp = (char)1;
			if (file_subdir_disp_chid) ca_array_put(DBR_CHAR, 1, file_subdir_disp_chid, &disp);
			disp = (char)1;
			if (file_basename_disp_chid) ca_array_put(DBR_CHAR, 1, file_basename_disp_chid, &disp);
			disp = (char)0;
			if (message_chid) ca_array_put(DBR_STRING, 1, message_chid, &disp);
		}
		Debug1(2,"\n nb_scan_running=%d\n", nb_scan_running);
	}
	epicsTimeToStrftime(pscan->stamp, MAX_STRING_SIZE, "%b %d, %Y %H:%M:%S.%06f", &pval->stamp);
	sendScanTSShortMsgWait(MSG_SCAN_DATA, (SCAN*)ca_puser(eha.chid), pval->stamp, pval->value);
	epicsTimeToStrftime(currtimestr, MAX_STRING_SIZE, "%b %d, %Y %H:%M:%S.%06f", &currtime);
	Debug6(1,"dataMonitor(%s)tid=%p(%s): (DATA=%d) eha time:%s, currtime=%s\n", pscan->name,
		epicsThreadGetIdSelf(), epicsThreadGetNameSelf(), sval, pscan->stamp, currtimestr);
}

/*----------------------------------------------------------------------*/
/* NPTS field monitor.                                                  */
/*                                                                      */
LOCAL void nptsMonitor(struct event_handler_args eha)
{
	sendScanLongMsgWait(MSG_SCAN_NPTS, (SCAN *) ca_puser(eha.chid), *((dbr_long_t *) eha.dbr));
}

/*----------------------------------------------------------------------*/
/* CPT field monitor.                                                   */
/*                                                                      */
LOCAL void cptMonitor(struct event_handler_args eha)
{
	SCAN* pscan;
	epicsTimeStamp currentTime;

	Debug0(2,"cptMonitor:entry\n");
	switch(saveData_MessagePolicy) {
	case 0:
		sendScanLongMsgWait(MSG_SCAN_CPT, (SCAN *) ca_puser(eha.chid), *((dbr_long_t *) eha.dbr));
		break;
	case 1:
		sendScanLongMsg(MSG_SCAN_CPT, (SCAN *) ca_puser(eha.chid), *((dbr_long_t *) eha.dbr));
		break;
	case 2:
		pscan = (SCAN *) ca_puser(eha.chid);
		(void)epicsTimeGetCurrent(&currentTime);
		if (epicsTimeDiffInSeconds(&currentTime, &(pscan->cpt_time)) >= cpt_wait_time) {
			pscan->cpt_time = currentTime;
			sendScanLongMsg(MSG_SCAN_CPT, (SCAN *) ca_puser(eha.chid), *((dbr_long_t *) eha.dbr));
		}
		break;
	case 3:
		pscan = (SCAN *) ca_puser(eha.chid);
		(void)epicsTimeGetCurrent(&currentTime);
		if (epicsTimeDiffInSeconds(&currentTime, &(pscan->cpt_time)) >= cpt_wait_time) {
			pscan->cpt_time = currentTime;
			sendScanLongMsgWait(MSG_SCAN_CPT, (SCAN *) ca_puser(eha.chid), *((dbr_long_t *) eha.dbr));
		}
		break;
	}
}

/*----------------------------------------------------------------------*/
/* PXNV field monitor.                                                  */
/*                                                                      */
LOCAL void pxnvMonitor(struct event_handler_args eha)
{
	sendScanIndexMsgWait(MSG_SCAN_PXNV, (SCAN *) ca_puser(eha.chid), (long) eha.usr, *((dbr_short_t *) eha.dbr));
}

/*----------------------------------------------------------------------*/
/* PXSM field monitor.                                                  */
/*                                                                      */
LOCAL void pxsmMonitor(struct event_handler_args eha)
{
	sendStringMsgWait(MSG_SCAN_PXSM, (char *)eha.usr, eha.dbr);
}

/*----------------------------------------------------------------------*/
/* RXNV field monitor.                                                  */
/*                                                                      */
LOCAL void rxnvMonitor(struct event_handler_args eha)
{
	sendScanIndexMsgWait(MSG_SCAN_RXNV, (SCAN *) ca_puser(eha.chid), (long) eha.usr, *((dbr_short_t *) eha.dbr));
}

/*----------------------------------------------------------------------*/
/* DXNV field monitor.                                                  */
/*                                                                      */
LOCAL void dxnvMonitor(struct event_handler_args eha)
{
	sendScanIndexMsgWait(MSG_SCAN_DXNV, (SCAN *) ca_puser(eha.chid), (long) eha.usr, *((dbr_short_t *) eha.dbr));
}


/*----------------------------------------------------------------------*/
/* TXNV field monitor.                                                  */
/*                                                                      */
LOCAL void txnvMonitor(struct event_handler_args eha)
{
	sendScanIndexMsgWait(MSG_SCAN_TXNV, (SCAN *) ca_puser(eha.chid), (long) eha.usr, *((dbr_short_t *) eha.dbr));
}

/*----------------------------------------------------------------------*/
/* TXCD field monitor.                                                  */
/*                                                                      */
LOCAL void txcdMonitor(struct event_handler_args eha)
{
	sendScanIndexMsgWait(MSG_SCAN_TXCD, (SCAN *) ca_puser(eha.chid), (long) eha.usr, *((dbr_float_t *) eha.dbr));
}

/*----------------------------------------------------------------------*/
/* DESC field monitor.                                                  */
/*                                                                      */
LOCAL void descMonitor(struct event_handler_args eha)
{
	sendStringMsgWait(MSG_DESC, (char *)eha.usr, eha.dbr);
}


LOCAL void fileSystemMonitor(struct event_handler_args eha)
{
	sendStringMsgWait(MSG_FILE_SYSTEM, NULL, eha.dbr);
}

LOCAL void fileSubdirMonitor(struct event_handler_args eha)
{
	sendStringMsgWait(MSG_FILE_SUBDIR, NULL, eha.dbr);
}

LOCAL void fileBasenameMonitor(struct event_handler_args eha)
{
	sendStringMsgWait(MSG_FILE_BASENAME, NULL, eha.dbr);
}

LOCAL void realTime1DMonitor(struct event_handler_args eha)
{
	sendIntegerMsgWait(MSG_REALTIME1D, *((dbr_long_t *) eha.dbr));
}


LOCAL int connectCounter(char* name)
{
	counter= 0;

	ca_search(name, &counter_chid);
	if (ca_pend_io(0.5)!=ECA_NORMAL) {
		Debug1(1, "Can't connect counter %s\n", name);
		return -1;
	}
	return 0;
}

LOCAL int connectFileSystem(char* fs)
{
	char fs_disp[80];
	file_system_state= FS_NOT_MOUNTED;

	sprintf(fs_disp, "%s.DISP", fs);

	ca_search(fs, &file_system_chid);
	ca_search(fs_disp, &file_system_disp_chid);
	if (ca_pend_io(0.5)!=ECA_NORMAL) {
		printf("saveData: Unable to connect %s\nsaveDataTask not initialized\n", fs);
		return -1;
	} else {
		if (ca_add_event(DBR_STRING, file_system_chid, 
				fileSystemMonitor, NULL, NULL)!=ECA_NORMAL) {
			printf("saveData: Can't monitor %s\nsaveDataTask not initialized\n", fs);
			ca_clear_channel(file_system_chid);
			return -1;
		}
	}
	return 0;
}

LOCAL int connectSubdir(char* sd)
{
	char sd_disp[80];

	sprintf(sd_disp, "%s.DISP", sd);
	ca_search(sd, &file_subdir_chid);
	ca_search(sd_disp, &file_subdir_disp_chid);
	if (ca_pend_io(0.5)!=ECA_NORMAL) {
		printf("saveData: Unable to connect %s\nsaveDataTask not initialized\n", sd);
		return -1;
	} else {
		if (ca_add_event(DBR_STRING, file_subdir_chid, 
				fileSubdirMonitor, NULL, NULL)!=ECA_NORMAL) {
			printf("saveData: Can't monitor %s\nsaveDataTask not initialized\n", sd);
			ca_clear_channel(file_subdir_chid);
			return -1;
		}
	}
	return 0;
}

LOCAL int connectBasename(char* bn)
{
	char bn_disp[80];

	sprintf(bn_disp, "%s.DISP", bn);
	ca_search(bn, &file_basename_chid);
	ca_search(bn_disp, &file_basename_disp_chid);
	if (ca_pend_io(0.5)!=ECA_NORMAL) {
		printf("saveData: Unable to connect %s\n", bn);
		file_basename_chid = NULL;
		return 0;
	} else {
		if (ca_add_event(DBR_STRING, file_basename_chid, 
				fileBasenameMonitor, NULL, NULL)!=ECA_NORMAL) {
			printf("saveData: Can't monitor %s\n", bn);
			ca_clear_channel(file_basename_chid);
			file_basename_disp_chid = NULL;
			return 0;
		}
	}
	return 0;
}

LOCAL int connectRealTime1D(char* rt)
{
	ca_search(rt, &realTime1D_chid);
	if (ca_pend_io(0.5)!=ECA_NORMAL) {
		printf("saveData: Unable to connect %s\n", rt);
		return -1;
	} else {
		if (ca_add_event(DBR_LONG, realTime1D_chid,
				realTime1DMonitor, NULL, NULL)!= ECA_NORMAL) {
			printf("saveData: Can't monitor %s\n", rt);
			ca_clear_channel(realTime1D_chid);
			return -1;
		}
	}
	return 0;
}

LOCAL void maxAllowedRetriesMonitor(struct event_handler_args eha)
{
	long i;
	if (eha.status != ECA_NORMAL) {
		printf("maxAllowedRetriesMonitor: bad status\n");
	} else {
		i = *((dbr_long_t *) eha.dbr);
		if (i >= 0) maxAllowedRetries = i;
		printf("saveData:maxAllowedRetries = %ld\n", maxAllowedRetries);
	}
}

LOCAL void retryWaitInSecsMonitor(struct event_handler_args eha)
{
	long i;
	if (eha.status != ECA_NORMAL) {
		printf("maxAllowedRetriesMonitor: bad status\n");
	} else {
		i = *((dbr_long_t *) eha.dbr);
		if (i >= 0) retryWaitInSecs = i;
	}
	printf("saveData:retryWaitInSecs = %ld\n", retryWaitInSecs);
}

LOCAL int connectRetryPVs(char *prefix)
{
	char pvName[PVNAME_STRINGSZ];

	strcpy(pvName, prefix); strcat(pvName, "saveData_currRetries");
	ca_search(pvName, &currRetries_chid);

	strcpy(pvName, prefix); strcat(pvName, "saveData_maxAllowedRetries");
	ca_search(pvName, &maxAllowedRetries_chid);

	strcpy(pvName, prefix); strcat(pvName, "saveData_totalRetries");
	ca_search(pvName, &totalRetries_chid);

	strcpy(pvName, prefix); strcat(pvName, "saveData_retryWaitInSecs");
	ca_search(pvName, &retryWaitInSecs_chid);

	strcpy(pvName, prefix); strcat(pvName, "saveData_abandonedWrites");
	ca_search(pvName, &abandonedWrites_chid);

	if (ca_pend_io(0.5)!=ECA_NORMAL) {
		printf("saveData: Can't connect to some or all retry PVs\n");
	}
	if (maxAllowedRetries_chid && (ca_add_event(DBR_LONG, maxAllowedRetries_chid, 
			maxAllowedRetriesMonitor, NULL, NULL)!=ECA_NORMAL)) {
		printf("saveData: Can't monitor %ssaveData_maxAllowedRetries.  Using default of %ld\n",
			prefix, maxAllowedRetries);
		if (maxAllowedRetries_chid) ca_clear_channel(maxAllowedRetries_chid);
	}
	if (retryWaitInSecs_chid && (ca_add_event(DBR_LONG, retryWaitInSecs_chid, 
			retryWaitInSecsMonitor, NULL, NULL)!=ECA_NORMAL)) {
		printf("saveData: Can't monitor %ssaveData_retryWaitInSecs.  Using default of %ld\n",
			prefix, retryWaitInSecs);
		if (retryWaitInSecs_chid) ca_clear_channel(retryWaitInSecs_chid);
	}
	return 0;
}

LOCAL void extraValCallback(struct event_handler_args eha)
{
	PV_NODE * pnode = eha.usr;
	long type = eha.type;
	long count = eha.count;
	READONLY DBR_VAL * pval = eha.dbr;
	char *string;

	size_t size=0;

	epicsMutexLock(pnode->lock);

	switch(type) {
	case DBR_STRING:
		size= strlen((char*)pval);
		/* logMsg("extraValCallback: count=%d, strlen=%d\n", count, size); */
		break;
	case DBR_CTRL_CHAR:
		size= dbr_size[DBR_CTRL_CHAR]+(count-1);
		break;
	case DBR_CTRL_SHORT:
		size= dbr_size[DBR_CTRL_SHORT]+(count-1)*sizeof(short);
		break;
	case DBR_CTRL_LONG:
		size= dbr_size[DBR_CTRL_LONG]+(count-1)*sizeof(long);
		break;
	case DBR_CTRL_FLOAT:
		size= dbr_size[DBR_CTRL_FLOAT]+(count-1)*sizeof(float);
		break;
	case DBR_CTRL_DOUBLE:
		size= dbr_size[DBR_CTRL_DOUBLE]+(count-1)*sizeof(double);
		break;
	default:
		printf("saveDta: unsupported dbr_type %d\n", (int)type);
		epicsMutexUnlock(pnode->lock);
		return;
	}

	memcpy(pnode->pval, pval, size);
	if (type == DBR_STRING) {
		string = (char *)pnode->pval;
		string[size>(MAX_STRING_SIZE-1)?(MAX_STRING_SIZE-1):size] = '\0';
		/* logMsg("extraValCallback: string is >%s<\n", (char *)(pnode->pval)); */
	}
	pnode->count= count;

	epicsMutexUnlock(pnode->lock);
}

LOCAL void extraDescCallback(struct event_handler_args eha)
{
	PV_NODE * pnode = eha.usr;
	READONLY DBR_VAL * pval = eha.dbr;

	epicsMutexLock(pnode->lock);

	strcpy(pnode->desc, (char *)pval);
	if (pnode->desc_chid) ca_clear_channel(pnode->desc_chid);

	epicsMutexUnlock(pnode->lock);
}


LOCAL int connectPV(char* pv, char* desc)
{
	PV_NODE* pnode;
	PV_NODE* pcur;
	char buff[PVNAME_STRINGSZ];
	long  count;
	int   type;
	int   len;
	long  size;

	/* allocate space for the new pv */
	pnode= (PV_NODE*) malloc(sizeof(PV_NODE));
	if (!pnode) {
		printf("saveData: Unable to add %s\n", pv);
		return -1;
	}
	
	/* set everything to 0 */
	memset(pnode, 0, sizeof(PV_NODE));

	/* try to connect the pv */
	ca_search(pv, &pnode->channel);
	if (ca_pend_io(10)!=ECA_NORMAL) {
		/* Unable to connect in 10 seconds. Discard the pv ! */
		printf("saveData: Unable to connect to %s\n", pv);
		if (pnode->channel) ca_clear_channel(pnode->channel);
		free(pnode);
		return -1;
	}

	strncpy(pnode->name, pv, PVNAME_STRINGSZ-1);
	pnode->name[PVNAME_STRINGSZ-1]= '\0';

	/* Allocate space for the pv's value */
	size= 0;
	type= ca_field_type(pnode->channel);
	count= ca_element_count(pnode->channel);
	switch(type) {
	case DBR_STRING:
		pnode->dbr_type= DBR_STRING;
		count= 1;
		size= MAX_STRING_SIZE;
		break;
	case DBR_CHAR:
		pnode->dbr_type= DBR_CTRL_CHAR;
		size= dbr_size[DBR_CTRL_CHAR]+ (count-1);
		break;
	case DBR_SHORT:
		pnode->dbr_type= DBR_CTRL_SHORT;
		size= dbr_size[DBR_CTRL_SHORT]+ (count-1)*sizeof(short);
		break;
	case DBR_ENUM:
		pnode->dbr_type= DBR_STRING;
		count= 1;
		size= MAX_STRING_SIZE;
		break;
	case DBR_LONG:
		pnode->dbr_type= DBR_CTRL_LONG;
		size= dbr_size[DBR_CTRL_LONG]+ (count-1) * sizeof(long);
		break;
	case DBR_FLOAT:
		pnode->dbr_type= DBR_CTRL_FLOAT;
		size= dbr_size[DBR_CTRL_FLOAT]+ (count-1) * sizeof(float);
		break;
	case DBR_DOUBLE:
		pnode->dbr_type= DBR_CTRL_DOUBLE;
		size= dbr_size[DBR_CTRL_DOUBLE]+ (count-1) * sizeof(double);
		break;
	default:
		printf("saveData: %s has an unsupported type\n", pv);
		ca_clear_channel(pnode->channel);
		free(pnode);
		return -1;
	}

	pnode->pval= malloc(size);
	memset(pnode->pval, 0, size);

	pnode->lock = epicsMutexCreate();
	/* Get a first image of the pv's value */
	ca_array_get_callback(pnode->dbr_type, count,
		pnode->channel, extraValCallback, (void*)pnode);

	/* Get the pv's description */
	if (!desc || (*desc=='\0')) {
		/* The description was not given in the req file. */
		/* Get it from the .DESC field */
		len= strcspn(pv, ".");
		strncpy(buff, pv, len);
		buff[len]='\0';
		strcat(buff, ".DESC");
		ca_search(buff, &pnode->desc_chid);
		pnode->desc[0]='\0';
		if (ca_pend_io(10)!=ECA_NORMAL) {
			printf("saveData: Unable to connect to %s\n", buff);
			if (pnode->desc_chid) ca_clear_channel(pnode->desc_chid);
		} else {
			ca_array_get_callback(DBR_STRING, 1, pnode->desc_chid, 
				extraDescCallback, (void*)pnode);
		}
	} else {
		/* Copy the description from the req file. */
		strncpy(pnode->desc, desc, MAX_STRING_SIZE-1);
		pnode->desc[MAX_STRING_SIZE-1]='\0';
	}

	/* Append the pv to the list */
	if (!list_pv) {
		list_pv= pnode;
	} else {
		pcur= list_pv;
		while (pcur->nxt) pcur=pcur->nxt;
		pcur->nxt= pnode;
	}

	/* Increase the number of extra pvs */
	nb_extraPV++;

	return 0;
}

LOCAL int initSaveDataTask() 
{
	REQ_FILE* rf;
	char  buff1[PVNAME_STRINGSZ];
	char  buff2[PVNAME_STRINGSZ];
	char  buff3[PVNAME_STRINGSZ];
	int i;

	server_pathname[0]= '\0';
	server_subdir= server_pathname;
	strcpy(local_pathname, "/data/");
	local_subdir= &local_pathname[strlen(local_pathname)];

	rf= req_open_file(req_file, req_macros);
	if (!rf) {
		Debug1(1, "Unable to open \"%s\". saveDataTask aborted\n", req_file);
		return -1;
	}

	/* get the IOC prefix                                                 */
	ioc_prefix[0]='\0';
	if (req_gotoSection(rf, "prefix")==0) {
		req_readMacId(rf, ioc_prefix, PREFIX_SIZE);
	}
	connectRetryPVs(ioc_prefix);

	/* replace punctuation with underscore, so we can use the prefix in a file name */
	for (i=0; i<PREFIX_SIZE && ioc_prefix[i]; i++) {
		if (ispunct((int)ioc_prefix[i])) ioc_prefix[i] = '_';
	}

	/* Connect to saveData_active                                         */
	if (req_gotoSection(rf, "status")!=0) {
		printf("saveData: section [status] not found\n");
		return -1;
	}
	if (req_readMacId(rf, buff1, PVNAME_STRINGSZ)==0) {
		printf("saveData: status pv name not defined\n");
		return -1;
	}
	ca_search(buff1, &save_status_chid);
	if (ca_pend_io(0.5)!=ECA_NORMAL) {
		printf("saveData: Unable to connect %s\n", buff1);
		return -1;
	}

	/* Connect to saveData_message                                        */
	message_chid= NULL;
	if (req_gotoSection(rf, "message")!=0) {
		printf("saveData: section [message] not found\n");
	} else {
		if (req_readMacId(rf, buff1, PVNAME_STRINGSZ)==0) {
			printf("saveData: message pv name not defined\n");
		} else {
			ca_search(buff1, &message_chid);
			ca_pend_io(0.5);
		}
	}

	/* Connect to saveData_filename                                       */
	filename_chid= NULL;
	if (req_gotoSection(rf, "filename")!=0) {
		printf("saveData: section [filename] not found\n");
	} else {
		if (req_readMacId(rf, buff1, PVNAME_STRINGSZ)==0) {
			printf("saveData: filename pv name not defined\n");
		} else {
			ca_search(buff1, &filename_chid);
			ca_pend_io(0.5);
		}
	}

	/* Connect to saveData_fullPathName                                   */
	full_pathname_chid= NULL;
	if (req_gotoSection(rf, "fullPathName")!=0) {
		printf("saveData: section [fullPathName] not found\n");
	} else {
		if (req_readMacId(rf, buff1, PVNAME_STRINGSZ)==0) {
			printf("saveData: fullPathName pv name not defined\n");
		} else {
			ca_search(buff1, &full_pathname_chid);
			ca_pend_io(0.5);
		}
	}

	/* Connect to saveData_scanNumber                                     */
	if (req_gotoSection(rf, "counter")!=0) {
		printf("saveData: section [counter] not found\n");
		return -1;
	}
	if (req_readMacId(rf, buff1, PVNAME_STRINGSZ)==0) {
		printf("saveData: counter pv name not defined\n");
		return -1;
	}
	if (connectCounter(buff1)==-1) return -1;


	/* Connect to saveData_fileSystem                                     */
	if (req_gotoSection(rf, "fileSystem")!=0) {
		printf("saveData: section [fileSystem] not found\n");
		return -1;
	}
	if (req_readMacId(rf, buff1, PVNAME_STRINGSZ)==0) {
		printf("saveData: fileSystem pv name not defined\n");
		return -1;
	}
	if (connectFileSystem(buff1)==-1) return -1;


	/* Connect to saveData_subDir                                        */
	if (req_gotoSection(rf, "subdir")!=0) {
		printf("saveData: section [subdir] not found\n");
		return -1;
	}
	if (req_readMacId(rf, buff1, PVNAME_STRINGSZ)==0) {
		printf("saveData: subDir pv name not defined\n");
		return -1;
	}
	if (connectSubdir(buff1)==-1) return -1;

	/* Connect to saveData_baseName.  We can run without this PV.        */
	if (req_gotoSection(rf, "basename")!=0) {
		printf("saveData: section [basename] not found\n");
	} else {
		if (req_readMacId(rf, buff1, PVNAME_STRINGSZ)==0) {
			printf("saveData: baseName pv name not defined\n");
		} else {
			if (connectBasename(buff1)==-1) {
				printf("saveData: failed to connect to baseName pv\n");
			}
		}
	}

	/* Connect all scan records.                                          */
	if (req_gotoSection(rf, "scanRecord")==0) {
		while (!eos(rf)) {
			req_readMacId(rf, buff1, PVNAME_STRINGSZ);
			if (debug_saveData) req_infoSection(rf);
			Debug2(2, "current char '%c' ?= '%c'\n", current(rf), rf->cur);
			if (debug_saveData) req_infoSection(rf);
			if (current(rf)==',') {
				req_readChar(rf);
				req_readMacId(rf, buff2, PVNAME_STRINGSZ);
				printf("saveData: handshake PV '%s' ignored.  Using '%s%s'\n",
					buff2, buff1, ".AWAIT");
			} else {
				buff2[0]= '\0';
			}
			strcpy(buff2, buff1);
			strcat(buff2, ".AWAIT");
			strcpy(buff3, buff1);
			strcat(buff3, ".AAWAIT");
			Debug2(2,"saveData: call connectScan(%s,%s)\n", buff1, buff2);
			connectScan(buff1, buff2, buff3);
		}
	}

	/* Connect all extra pvnames                                          */
	nb_extraPV= 0;
	if (req_gotoSection(rf, "extraPV")==0) {
		while (!eos(rf)) {
			req_readMacId(rf, buff1, PVNAME_STRINGSZ);
			if (current(rf)=='"') {
				req_readString(rf, buff2, PVNAME_STRINGSZ);
			} else {
				buff2[0]= '\0';
			}
			connectPV(buff1, buff2);
		}
	}


	/* Connect to saveData_realTime1D                                     */
	if (req_gotoSection(rf, "realTime1D")!=0) {
		printf("saveData: section [realTime1D] not found\n");
	} else {
		if (req_readMacId(rf, buff1, PVNAME_STRINGSZ)==0) {
			printf("saveData: realTime1D pv name not defined\n");
		} else {
			connectRealTime1D(buff1);
		}
	}

	req_close_file(rf);

	monitorScans();

	return 0;
}

LOCAL int  scan_getDim(SCAN* pscan)
{
	int   dim= 0;
	SCAN* cur= pscan;

	while (cur) {
		dim++;
		cur= cur->nxt;
	}
	return dim;
}

LOCAL void getExtraPV()
{
	PV_NODE* pcur;
	chid     channel;

	pcur= list_pv;
	while (pcur) {
		channel= pcur->channel;
		if (channel) ca_array_get_callback(pcur->dbr_type, ca_element_count(channel),
			channel, extraValCallback, (void*)pcur);
		pcur= pcur->nxt;
	}
}

/*
 * Write extra (also called "environment") PV's.
 * return: 0 if successful, else nonzero
 */
LOCAL int saveExtraPV(XDR* pxdrs)
{
	PV_NODE* pcur;
	chid     channel;
	int      type;
	DBR_VAL* pval;
	long     count;
	char*    cptr;
	bool_t writeFailed = FALSE;

	/* number of pv saved */
	writeFailed |= !xdr_int(pxdrs, &nb_extraPV);

	if (nb_extraPV>0) {

		pcur= list_pv;
		while (pcur) {
			epicsMutexLock(pcur->lock);

			channel= pcur->channel;
			pval= pcur->pval;
			
			cptr= pcur->name;
			writeFailed |= !xdr_counted_string(pxdrs, &cptr);

			cptr= pcur->desc;
			writeFailed |= !xdr_counted_string(pxdrs, &cptr);
			
			type= pcur->dbr_type;
			writeFailed |= !xdr_int(pxdrs, &type);

			if (type!=DBR_STRING) {
				count= pcur->count;
				writeFailed |= !xdr_long(pxdrs, &count);
			}

			switch (type) {
			case DBR_STRING:
				writeFailed |= !xdr_counted_string(pxdrs, (char**)&pval);
				break;
			case DBR_CTRL_CHAR:
				cptr= pval->cchrval.units;
				writeFailed |= !xdr_counted_string(pxdrs, &cptr);
				/* xdr_bytes(pxdrs,(char**)&pval->cchrval.value,&count, count); */
				writeFailed |= !xdr_vector(pxdrs,(char*)&pval->cchrval.value,count,sizeof(char),(xdrproc_t)xdr_char);
				break;
			case DBR_CTRL_SHORT:
				cptr= pval->cshrtval.units;
				writeFailed |= !xdr_counted_string(pxdrs, &cptr);
				writeFailed |= !xdr_vector(pxdrs,(char*)&pval->cshrtval.value,count,sizeof(short),(xdrproc_t)xdr_short);
				break;
			case DBR_CTRL_LONG:
				cptr= pval->clngval.units;
				writeFailed |= !xdr_counted_string(pxdrs, &cptr);
				writeFailed |= !xdr_vector(pxdrs,(char*)&pval->clngval.value,count, sizeof(long),(xdrproc_t)xdr_long);
				break;
			case DBR_CTRL_FLOAT:
				cptr= pval->cfltval.units;
				writeFailed |= !xdr_counted_string(pxdrs, &cptr);
				writeFailed |= !xdr_vector(pxdrs,(char*)&pval->cfltval.value,count, sizeof(float),(xdrproc_t)xdr_float);
				break;
			case DBR_CTRL_DOUBLE:
				cptr= pval->cdblval.units;
				writeFailed |= !xdr_counted_string(pxdrs, &cptr);
				writeFailed |= !xdr_vector(pxdrs,(char*)&pval->cdblval.value,count, sizeof(double),(xdrproc_t)xdr_double);
				break;
			}

			epicsMutexUnlock(pcur->lock);
			pcur= pcur->nxt;
		}
	}
	return(writeFailed ? -1 : 0);
}

LOCAL void reset_old_npts(SCAN *pscan) {
	pscan->old_npts= -1;
	if (pscan->nxt) reset_old_npts(pscan->nxt);
}

/* Write a scan to the data file.  Return zero if successful. */
LOCAL int writeScanRecInProgress(SCAN *pscan, epicsTimeStamp stamp, int isRetry)
{
	FILE *fd = NULL;
	XDR   xdrs;
	char  msg[200], timeStr[MAX_STRING_SIZE];
	char  *cptr, cval;
	epicsTimeStamp  openTime;
	int i, ival;
	long lval, scan_offset, data_size;
	static float fileFormatVersion = FILE_FORMAT_VERSION;
	bool_t writeFailed = FALSE;

	/* Attempt to open data file */
	Debug1(3, "saveData:writeScanRecInProgress: Opening file '%s'\n", pscan->ffname);
	epicsTimeGetCurrent(&openTime);
	fd = fopen(pscan->ffname, pscan->first_scan ? "wb+" : "rb+");

	if ((fd==NULL) || (fileStatus(pscan->ffname) == ERROR)) {
		if (fd!=NULL) {
			printf("saveData:writeScanRecInProgress: stat(%s) set errno to %d ('%s')\n", 
				pscan->ffname, errno, strerror(errno));
			fclose(fd);
		}
		printf("saveData:writeScanRecInProgress(%s): can't open data file!!\n", pscan->name);
		sprintf(msg, "!! Can't open file %s", pscan->fname);
		msg[MAX_STRING_SIZE-1] = '\0';
		sendUserMessage(msg);
		save_status = STATUS_ERROR;
		ca_array_put(DBR_SHORT, 1, save_status_chid, &save_status);
		return(-1);
	}

	if (!(pscan->first_scan)) {
		if (isRetry && pscan->savedSeekPos) {
			/*
			 * We've tried before and failed to write this scan, and any writing that did
			 * succeed would have changed the end-of-file position.  Go back to where the
			 * end-of-file was on our first write attempt.
			 */
			if (fseek(fd, pscan->savedSeekPos, SEEK_SET)==EOF) {fclose(fd); return(-1);}
		} else {
			/* Append this scan to the data file. */
			if (fseek(fd, 0, SEEK_END)==EOF) {fclose(fd); return(-1);}
			pscan->savedSeekPos = ftell(fd);
			if (pscan->savedSeekPos == EOF) {pscan->savedSeekPos = 0; fclose(fd); return(-1);}
		}
	}
	xdrstdio_create(&xdrs, fd, XDR_ENCODE);

	if (pscan->first_scan) {
		/*----------------------------------------------------------------*/
		/* Write the file header */
		Debug0(3, "saveData:writeScanRecInProgress: Writing file header\n");
		writeFailed |= !xdr_float(&xdrs, &fileFormatVersion); /* file format version      */
		writeFailed |= !xdr_long(&xdrs, &pscan->counter);     /* scan number              */
		Debug1(2, "saveData:writeScanRecInProgress: scan_dim=%d\n", pscan->scan_dim);
		writeFailed |= !xdr_short(&xdrs, &pscan->scan_dim);   /* rank of the data         */
		pscan->dims_offset = xdr_getpos(&xdrs);
		Debug3(2, "saveData:writeScanRecInProgress:(%s) scan_dim=%d, dims_offset=%ld\n",
			pscan->name, pscan->scan_dim, pscan->dims_offset);
		if (pscan->dims_offset == (u_int)(-1)) {writeFailed = TRUE; goto cleanup;}
		ival = -1;
		for (i=0; i<pscan->scan_dim; i++) writeFailed |= !xdr_int(&xdrs, &ival);
		pscan->regular_offset = xdr_getpos(&xdrs);
		if (pscan->regular_offset == (u_int)(-1)) {writeFailed = TRUE; goto cleanup;}
		ival = 1;                     /* regular (rectangular array) = true */
		writeFailed |= !xdr_int(&xdrs, &ival);
			
		/* offset to the extraPVs */
		pscan->offset_extraPV = xdr_getpos(&xdrs);
		if (pscan->offset_extraPV == (u_int)(-1)) {writeFailed = TRUE; goto cleanup;}
		lval = 0;
		writeFailed |= !xdr_long(&xdrs, &lval);
		if (writeFailed) goto cleanup;
		Debug0(3, "saveData:writeScanRecInProgress: File Header written\n");
	}

	/*
	 *If an inner scan dimension exists, set it's offsets. 
	 * (This used to be done in proc_scan_data().)
	 */
	if (pscan->nxt) {
		pscan->nxt->dims_offset= pscan->dims_offset+sizeof(int);
		Debug3(2, "saveData:proc_scan_data:(%s) scan_dim=%d, dims_offset=%ld\n",
			pscan->name, pscan->scan_dim, pscan->dims_offset);
		pscan->nxt->regular_offset= pscan->regular_offset;
	}

	/* the offset of this scan                                          */
	scan_offset = xdr_getpos(&xdrs);
	Debug2(2, "saveData:writeScanRecInProgress:(%s) scan_offset=%ld\n",
			pscan->name, scan_offset);
	if (scan_offset == (u_int)(-1)) {writeFailed = TRUE; goto cleanup;}

	/*------------------------------------------------------------------*/
	/* Scan header                                                      */
	Debug0(3, "saveData:writeScanRecInProgress: Writing per-scan header\n");
	writeFailed |= !xdr_short(&xdrs, &pscan->scan_dim); /* scan dimension               */
	lval = pscan->npts;
	writeFailed |= !xdr_long(&xdrs, &lval);             /* # of pts                     */
	pscan->cpt_fpos = xdr_getpos(&xdrs);
	if (pscan->cpt_fpos == (u_int)(-1)) {writeFailed = TRUE; goto cleanup;}
	lval = pscan->cpt;                   /* last valid point             */
	writeFailed |= !xdr_long(&xdrs, &lval);    

	if (pscan->scan_dim>1) {             /* index of lower scans         */
		lval = xdr_getpos(&xdrs);
		if (lval == (u_int)(-1)) {writeFailed = TRUE; goto cleanup;}
		pscan->nxt->offset = lval; /* tell inner scan where to write its offset */
		Debug3(15, "saveData:writeScanRecInProgress(%s) telling %s to write its next offset at loc %ld\n",
			pscan->name, pscan->nxt->name, lval);
		writeFailed |= !xdr_setpos(&xdrs, lval+pscan->npts*sizeof(long));
	}
	if (writeFailed) goto cleanup;

	/*------------------------------------------------------------------*/
	/* Scan info                                                        */
	Debug0(3, "saveData:writeScanRecInProgress: Save scan info\n");
	cptr = pscan->name;
	writeFailed |= !xdr_counted_string(&xdrs, &cptr);   /* scan name                    */

	epicsTimeToStrftime(timeStr, MAX_STRING_SIZE, "%b %d, %Y %H:%M:%S.%06f", &stamp);
	cptr = timeStr;
	pscan->time_fpos = xdr_getpos(&xdrs);
	if (pscan->time_fpos == (u_int)(-1)) {writeFailed = TRUE; goto cleanup;}
	writeFailed |= !xdr_counted_string(&xdrs, &cptr);   /* time stamp                   */

	writeFailed |= !xdr_int(&xdrs, &pscan->nb_pos);     /* # of positioners             */
	writeFailed |= !xdr_int(&xdrs, &pscan->nb_det);     /* # of detectors               */
	writeFailed |= !xdr_int(&xdrs, &pscan->nb_trg);     /* # of triggers                */
	if (writeFailed) goto cleanup;

	if (pscan->nb_pos) {
		for (i=0; i<SCAN_NBP; i++) {
			if ((pscan->rxnv[i]==XXNV_OK) || (pscan->pxnv[i]==XXNV_OK)) {
				Debug1(3, "saveData:writeScanRecInProgress: Pos[%d] info\n", i);
				writeFailed |= !xdr_int(&xdrs, &i);           /* positioner number            */
				cptr = pscan->pxpv[i];
				writeFailed |= !xdr_counted_string(&xdrs, &cptr);/* positioner name           */
				cptr = pscan->pxds[i];
				writeFailed |= !xdr_counted_string(&xdrs, &cptr);/* positioner desc           */
				cptr = pscan->pxsm[i];
				writeFailed |= !xdr_counted_string(&xdrs, &cptr);/* positioner step mode      */
				cptr = pscan->pxeu[i].units;
				writeFailed |= !xdr_counted_string(&xdrs, &cptr);/* positioner unit           */
				cptr = pscan->rxpv[i];
				writeFailed |= !xdr_counted_string(&xdrs, &cptr);/* readback name             */
				cptr = pscan->rxds[i];
				writeFailed |= !xdr_counted_string(&xdrs, &cptr);/* readback description      */
				cptr = pscan->rxeu[i].units;
				writeFailed |= !xdr_counted_string(&xdrs, &cptr);/* readback unit             */
			}
		}
	}
	if (writeFailed) goto cleanup;

	if (pscan->nb_det) {
		for (i=0; i<SCAN_NBD; i++) {
			if (pscan->dxnv[i]==XXNV_OK) {
				Debug1(3, "saveData:writeScanRecInProgress: Det[%d] info\n", i);
				writeFailed |= !xdr_int(&xdrs, &i);              /* detector number           */
				cptr = pscan->dxpv[i];
				writeFailed |= !xdr_counted_string(&xdrs, &cptr);/* detector name             */
				cptr = pscan->dxds[i];
				writeFailed |= !xdr_counted_string(&xdrs, &cptr);/* detector description      */
				cptr = pscan->dxeu[i].units;
				writeFailed |= !xdr_counted_string(&xdrs, &cptr);/* detector unit             */
			}
		}
	}
	if (writeFailed) goto cleanup;

	if (pscan->nb_trg) {
		for (i=0; i<SCAN_NBT; i++) {
			if (pscan->txnv[i]==XXNV_OK) {
				Debug1(3, "saveData:writeScanRecInProgress: Trg[%d] info\n", i);
				writeFailed |= !xdr_int(&xdrs, &i);               /* trigger number           */
				cptr = pscan->txpv[i];
				writeFailed |= !xdr_counted_string(&xdrs, &cptr); /* trigger name             */
				writeFailed |= !xdr_float(&xdrs, &pscan->txcd[i]);/* trigger command          */
			}
		}
	}
	if (writeFailed) goto cleanup;

	data_size = 0;
	lval = xdr_getpos(&xdrs);
	if (lval == (u_int)(-1)) {writeFailed = TRUE; goto cleanup;}
	if (pscan->nb_pos) {
		/* calculate file space required for nb_pos positioners                          */
		for (i=0; i<SCAN_NBP; i++) {
			if ((pscan->rxnv[i]==XXNV_OK) || (pscan->pxnv[i]==XXNV_OK)) {
				Debug1(3, "saveData:writeScanRecInProgress: Allocate space for Pos[%d]\n", i);
				pscan->pxra_fpos[i] = lval+data_size;
				data_size += pscan->npts*sizeof(double);
			}
		}
	}
	if (pscan->nb_det) {
		/* calculate file space required for nb_det detectors                            */
		for (i=0; i<SCAN_NBD; i++) {
			if (pscan->dxnv[i]==XXNV_OK) {
				Debug1(3, "saveData:writeScanRecInProgress: Allocate space for Det[%d]\n", i);
				pscan->dxda_fpos[i] = lval+data_size;
				data_size += pscan->npts*sizeof(float);
			}
		}
	}

	if (data_size>0) {
		/* reserve file space for data */
		if (fseek(fd, data_size-1, SEEK_CUR)==EOF) {writeFailed = TRUE; goto cleanup;}
		cval = 0;
		fwrite((void*)&cval, 1,1, fd);
	}
		
	if (pscan->old_npts < pscan->npts) {
		writeFailed |= !xdr_setpos(&xdrs, pscan->dims_offset);
		Debug3(2, "saveData:writeScanRecInProgress:(%s) scan_dim=%d, dims_offset=%ld\n",
			pscan->name, pscan->scan_dim, pscan->dims_offset);
		if (writeFailed) goto cleanup;
		lval = pscan->npts;
		writeFailed |= !xdr_long(&xdrs, &lval);
	}

	if (pscan->old_npts!=-1 && pscan->old_npts!=pscan->npts) {
		/* npts changed during scan (possible only for a multidimensional scan) */
		ival = 0;  /*regular= FALSE */
		writeFailed |= !xdr_setpos(&xdrs, pscan->regular_offset);
		if (writeFailed) goto cleanup;
		writeFailed |= !xdr_int(&xdrs, &ival);
	}
	pscan->old_npts = pscan->npts;


	if (pscan->first_scan==FALSE) {
		Debug3(15, "saveData:writeScanRecInProgress(%s): writing offset %ld at loc %ld\n",
			pscan->name, scan_offset, pscan->offset);
		writeFailed |= !xdr_setpos(&xdrs, pscan->offset);
		if (writeFailed) goto cleanup;
		writeFailed |= !xdr_long(&xdrs, &scan_offset);
		lval = xdr_getpos(&xdrs);
		if (lval == (u_int)(-1)) {writeFailed = TRUE; goto cleanup;}
		pscan->offset = lval;
	}

	if (save_status == STATUS_ERROR) {
		save_status = STATUS_ACTIVE_OK;
		ca_array_put(DBR_SHORT, 1, save_status_chid, &save_status);
	}
	if (isRetry) {
		printf("saveData:writeScanRecInProgress(%s): retry succeeded\n", pscan->name);
		sprintf(msg, "Retry succeeded for %s", pscan->fname);
		msg[MAX_STRING_SIZE-1]= '\0';
		sendUserMessage(msg);
	} else {
		sprintf(msg,"Wrote data to %s", pscan->fname);
		sendUserMessage(msg);
	}

cleanup:
	xdr_destroy(&xdrs);
	fclose(fd);
	return(writeFailed ? -1 : 0);
}

/* Write the last or only scan to the data file.  Return zero if successful. */
LOCAL int writeScanRecCompleted(SCAN *pscan, int isRetry)
{
	FILE *fd = NULL;
	XDR   xdrs;
	char  msg[200];
	int i, status;
	long j, lval;
	bool_t writeFailed = FALSE;

	fd = fopen(pscan->ffname, "rb+");
	if ((fd == NULL) || (fileStatus(pscan->ffname) == ERROR)) {
		printf("saveData:writeScanRecCompleted(%s): can't open data file!!\n", pscan->name);
		sprintf(msg, "!! Can't open file %s", pscan->fname);
		msg[MAX_STRING_SIZE-1]= '\0';
		sendUserMessage(msg);
		save_status = STATUS_ERROR;
		if (save_status_chid) ca_array_put(DBR_SHORT, 1, save_status_chid, &save_status);
	    if (fd) fclose(fd);
		return(-1);
	}

	xdrstdio_create(&xdrs, fd, XDR_ENCODE);

	/* The scan just finished. update buffers and save scan */
	Debug2(3, "saveData:writeScanRecCompleted: writing %s to %s\n", pscan->name, pscan->fname);

	/* get cpt that goes with buffered data arrays */
	pscan->cpt = pscan->bcpt;
	Debug2(5, "saveData:writeScanRecCompleted(%s): bcpt=%ld\n", pscan->name, pscan->bcpt);

	/* Get all valid arrays */
	if (pscan->nb_pos) {
		for (i=0; i<SCAN_NBP; i++) {
			if ((pscan->pxnv[i]==XXNV_OK) || (pscan->rxnv[i]==XXNV_OK)) {
				if (pscan->cpxra[i] == NULL) {
					printf("saveData:writeScanRecCompleted: Can't get %s positioner array %d\n", pscan->name, i);
				} else {
					status = ca_array_get(DBR_DOUBLE, pscan->bcpt, pscan->cpxra[i], pscan->pxra[i]);
					if (status != ECA_NORMAL) {
						printf("saveData:writeScanRecCompleted: ca_array_get() (%ld pts) returned %d for scan %s, p%d\n",
							pscan->bcpt, status, pscan->name, i);
						printf("...%d means '%s'\n", status, ca_message(status));
					}
				}
				if (pscan->bcpt < pscan->npts) { /* zero unacquired data points */
					for (j=pscan->bcpt; j<pscan->npts; j++) pscan->pxra[i][j] = 0.0;
				}
			}
		}
	}
	if (pscan->nb_det) {
		for (i=0; i<SCAN_NBD; i++) {
#if ALLOC_ALL_DETS
			if (pscan->dxnv[i]==XXNV_OK) {
				if (pscan->cdxda[i] == NULL) {
					printf("saveData:writeScanRecCompleted: Can't get %s detector array %d\n", pscan->name, i);
				} else {
					ca_array_get(DBR_FLOAT, pscan->bcpt, pscan->cdxda[i], pscan->dxda[i]);
				}
			}
#else
			if (pscan->dxnv[i]==XXNV_OK) {
				if (!pscan->dxda[i]) {
					if ((pscan->dxda[i] = (float*)calloc(pscan->mpts, sizeof(float))) != NULL) {
						printf("saveData:writeScanRecCompleted: Allocated array for det %s.%s\n", pscan->name, dxda[i]);
						sprintf(msg, "Allocated mem for %s.%s", pscan->name, dxda[i]);
					} else {
						printf("saveData:writeScanRecCompleted: Can't alloc array for det %s.%s\n", pscan->name, dxda[i]);
						sprintf(msg, "!! No mem for %s.%s", pscan->name, dxda[i]);
					}
					msg[MAX_STRING_SIZE-1] = '\0';
					sendUserMessage(msg);
				}
				if (pscan->dxda[i]) {
					if (pscan->cdxda[i] == NULL) {
						printf("saveData:writeScanRecCompleted: Can't get %s detector array %d\n", pscan->name, i);
					} else {
						status = ca_array_get(DBR_FLOAT, pscan->bcpt, pscan->cdxda[i], pscan->dxda[i]);
						if (status != ECA_NORMAL) {
							printf("saveData:writeScanRecCompleted: ca_array_get() (%ld pts) returned %d for scan %s, d%d\n",
								pscan->bcpt, status, pscan->name, i);
							printf("...%d means '%s'\n", status, ca_message(status));
						}
					}
				}
			}
#endif
			if (pscan->dxda[i] && (pscan->bcpt < pscan->npts)) { /* zero unacquired data points */
				for (j=pscan->bcpt; j<pscan->npts; j++) pscan->dxda[i][j] = 0.0;
			}
		}
	}
	if (ca_pend_io(1.0)!=ECA_NORMAL) {
		Debug0(3, "saveData:writeScanRecCompleted: unable to get all valid arrays \n");
		sprintf(msg, "!! Can't get data");
		msg[MAX_STRING_SIZE-1] = '\0';
		sendUserMessage(msg);
		return(-1);
	}

	/* Write the positioner arrays */
	if (pscan->nb_pos) {
		for (i=0; i<SCAN_NBP; i++) {
			if ((pscan->rxnv[i]==XXNV_OK) || (pscan->pxnv[i]==XXNV_OK)) {
				writeFailed |= !xdr_setpos(&xdrs, pscan->pxra_fpos[i]);
				if (writeFailed) goto cleanup;
				writeFailed |= !xdr_vector(&xdrs, (char*)pscan->pxra[i], pscan->npts, 
					sizeof(double), (xdrproc_t)xdr_double);
			}
		}
	}
	/* Write the detector arrays */
	if (pscan->nb_det) {
		for (i=0; i<SCAN_NBD; i++) {
			if ((pscan->dxnv[i]==XXNV_OK) && pscan->dxda[i]) {
				writeFailed |= !xdr_setpos(&xdrs, pscan->dxda_fpos[i]);
				if (writeFailed) goto cleanup;
				writeFailed |= !xdr_vector(&xdrs, (char*)pscan->dxda[i], pscan->npts,
					sizeof(float), (xdrproc_t)xdr_float);
			}
		}
	}

	writeFailed |= !xdr_setpos(&xdrs, pscan->cpt_fpos);
	if (writeFailed) goto cleanup;
	lval = pscan->bcpt;
	writeFailed |= !xdr_long(&xdrs, &lval); 


	if (pscan->first_scan) {
		/* Write extra pvs at the end of the file. */
		if (isRetry && pscan->savedSeekPos) {
			/*
			 * We've tried before and failed to write this scan, and any writing that did
			 * succeed would have changed the end-of-file position.  Luckily, we saved
			 * that position the first time we tried to write extra PV's.  Go there now.
			 */
			 if (fseek(fd, pscan->savedSeekPos, SEEK_SET)==EOF) {fclose(fd); return(-1);}
		} else {
			/*
			 * Extra PV's get tacked on at the end of the file.  Remember where that is,
			 * in case we run into trouble and have to retry.
			 */
			if (fseek(fd, 0, SEEK_END)==EOF) {fclose(fd); return(-1);}
			pscan->savedSeekPos = ftell(fd);
			if (pscan->savedSeekPos == EOF) {pscan->savedSeekPos = 0; fclose(fd); return(-1);}
		}

		lval = xdr_getpos(&xdrs);
		if (lval == (u_int)(-1)) {writeFailed = TRUE; goto cleanup;}
		writeFailed |= saveExtraPV(&xdrs);
		writeFailed |= !xdr_setpos(&xdrs, pscan->offset_extraPV);
		if (writeFailed) goto cleanup;
		writeFailed |= !xdr_long(&xdrs, &lval);

		sprintf(msg,"Done writing %s", pscan->fname);
		sendUserMessage(msg);
	}

	if (save_status == STATUS_ERROR) {
		save_status = STATUS_ACTIVE_OK;
		ca_array_put(DBR_SHORT, 1, save_status_chid, &save_status);
	}
	if (isRetry) {
		printf("saveData:writeScanRecCompleted(%s): retry succeeded\n", pscan->name);
		sprintf(msg, "Retry succeeded for '%s'", pscan->name);
		msg[MAX_STRING_SIZE-1]= '\0';
		sendUserMessage(msg);
	} else {
		sprintf(msg,"Wrote data to %s", pscan->fname);
		sendUserMessage(msg);
	}

cleanup:
	xdr_destroy(&xdrs);
	fclose(fd);
	return(writeFailed?1:0);
}


LOCAL void proc_scan_data(SCAN_TS_SHORT_MSG* pmsg)
{
	char  msg[200];
	SCAN  *pscan, *pnxt;
	int   i, status;

	char  cval;
	short sval;
	int   duplicate_scan_number;
	epicsTimeStamp  openTime, now;

	pscan= pmsg->pscan;  
	Debug2(5, "proc_scan_data(%s):entry:pmsg->val=%d\n", pscan->name, pmsg->val);

	if (pscan->data == -1) {
		/* this should happen only once, at init */
		pscan->data = pmsg->val;
		Debug1(2,"!!!proc_scan_data(%s) pscan->data == -1!!!\n", pscan->name);
		return;
	}

	if ((save_status == STATUS_INACTIVE) || (save_status == STATUS_ACTIVE_FS_ERROR)) {
		if (pmsg->val==0) {
			/* The file system is not mounted.  Warn user and punt */
			sendUserMessage("Scan not being saved !!!!!");
		} else {
			/* Scan is over.  If all scans in this group are over, enable file system record */
			if (--nb_scan_running==0) {
				cval=(char)0;
				if (file_system_disp_chid) ca_array_put(DBR_CHAR, 1, file_system_disp_chid, &cval);
				cval=(char)0;
				if (file_subdir_disp_chid) ca_array_put(DBR_CHAR, 1, file_subdir_disp_chid, &cval);
				cval=(char)0;
				if (file_basename_disp_chid) ca_array_put(DBR_CHAR, 1, file_basename_disp_chid, &cval);
			}
			Debug1(2,"(save_status inactive) nb_scan_running=%d\n", nb_scan_running);
			if (nb_scan_running < 0) {
				printf("proc_scan_data(%s): nb_scan_running was %d.  Set to zero.\n",
					pscan->name, nb_scan_running);
				nb_scan_running = 0;
			}
		}
		pscan->data= pmsg->val;
		return;
	}

	if (pmsg->val==0) {

		/********************** scan started *****************************/

		Debug1(2, "scan started: %s\n", pscan->name);
		/* the scan is started, update counter and lower scan               */
		pscan->data=0;

		/* initialize the scan */
		pscan->nb_pos=0;
		Debug0(3, "Checking number of valid positioner\n");
		for (i=0; i<SCAN_NBP; i++) {
			if ((pscan->pxnv[i]==XXNV_OK) || (pscan->rxnv[i]==XXNV_OK)) {
				pscan->nb_pos++;
				/* request ctrl info for the positioner (unit) */
				if (pscan->cpxeu[i]) ca_array_get(DBR_CTRL_DOUBLE, 1, pscan->cpxeu[i], &pscan->pxeu[i]);
				/* request ctrl info for the readback (unit) */
				if (pscan->crxeu[i]) ca_array_get(DBR_CTRL_DOUBLE, 1, pscan->crxeu[i], &pscan->rxeu[i]);
			}
		}
		Debug0(3, "Checking number of valid detector\n");
		pscan->nb_det=0;
		for (i= 0; i<SCAN_NBD; i++) {
			if (pscan->dxnv[i]==XXNV_OK) {
				pscan->nb_det++;
				/* request ctrl info for the detector (unit) */
				if (pscan->cdxeu[i]) ca_array_get(DBR_CTRL_FLOAT, 1, pscan->cdxeu[i], &pscan->dxeu[i]);
			}
		}
		pscan->nb_trg=0;
		Debug0(3, "Checking number of valid trigger\n");
		for (i= 0; i<SCAN_NBT; i++) {
			/* compute the number of valid triggers in the scan */
			if (pscan->txnv[i]==XXNV_OK) {
				pscan->nb_trg++;
			}
		}

		pscan->cpt= 0;

		/* make sure all requests for units are completed */
		if (ca_pend_io(2.0)!=ECA_NORMAL) {
			printf("saveData: Unable to get all pos/rdb/det units\n");
		}

		if (pscan->first_scan) {
			/* We're processing the outermost of a possibly multidimensional scan */
			Debug0(3, "Outermost scan\n");
			Debug1(5, "proc_scan_data(%s):New file\n", pscan->name);
			/* Get number for this scan */
			if (counter_chid == NULL) {
				printf("saveData: unable to get scan number !!!\n");
			} else {
				ca_array_get(DBR_LONG, 1, counter_chid, &counter);
				if (ca_pend_io(0.5)!=ECA_NORMAL) {
					/* error !!! */
					printf("saveData: unable to get scan number !!!\n");
				} else {
					pscan->counter = counter;
				}
			}
			/* Make file name */
			if (scanFile_basename[0] == '\0') {
				strncpy(scanFile_basename, ioc_prefix, BASENAME_SIZE);
			}
			sprintf(pscan->fname, "%s%.4d.mda", scanFile_basename, (int)pscan->counter);
#ifdef vxWorks
			sprintf(pscan->ffname, "%s%s", local_pathname, pscan->fname);
#else
			sprintf(pscan->ffname, "%s%s", server_pathname, pscan->fname);
#endif

			/* If pscan->ffname already exists, insert '_nn' into file name and try again */
			duplicate_scan_number = 0;
			while ((fileStatus(pscan->ffname) == 0) && (duplicate_scan_number < 99)) {
#ifdef vxWorks
				sprintf(pscan->ffname, "%s%s%.4d_%.2d.mda", local_pathname, scanFile_basename, (int)pscan->counter, ++duplicate_scan_number);
#else
				sprintf(pscan->ffname, "%s%s%.4d_%.2d.mda", server_pathname, scanFile_basename, (int)pscan->counter, ++duplicate_scan_number);
#endif
			}
			if (duplicate_scan_number) {
				/* sprintf(&pscan->fname[strlen(pscan->fname)], "_%.2d", duplicate_scan_number); */
				sprintf(pscan->fname, "%s%.4d_%.2d.mda", scanFile_basename, (int)pscan->counter, duplicate_scan_number);

			}

			/* Tell user what we're doing */
			sprintf(msg, "Writing to %s", pscan->fname);
			msg[MAX_STRING_SIZE-1]= '\0';
			sendUserMessage(msg);

			/* Write file name where client can easily find it. */
			if (filename_chid) ca_array_put(DBR_STRING, 1, filename_chid, pscan->fname);

			/* increment scan number and write it to the PV */
			counter = pscan->counter + 1;
			ca_array_put(DBR_LONG, 1, counter_chid, &counter);

			pscan->scan_dim= scan_getDim(pscan);
			reset_old_npts(pscan);

			/* Issue caget's for all the extra pvs */
			getExtraPV();
		}

		/* If an inner scan dimension exists, set it up. */
		if (pscan->nxt) {
			pscan->nxt->first_scan= FALSE;
			pscan->nxt->scan_dim= pscan->scan_dim-1;
			/*
			 * This is no longer a good way to set dims_offset or regular_offset. If this
			 * scan's file header hasn't been written yet, pscan->dims_offset and
			 * pscan->regular_offset will not have been set.
			 */
#if 0
			pscan->nxt->dims_offset= pscan->dims_offset+sizeof(int);
			Debug3(2, "saveData:proc_scan_data:(%s) scan_dim=%d, dims_offset=%ld\n",
				pscan->name, pscan->scan_dim, pscan->dims_offset);
			pscan->nxt->regular_offset= pscan->regular_offset;
#endif
			strcpy(pscan->nxt->fname, pscan->fname);
			strcpy(pscan->nxt->ffname, pscan->ffname);
		}

		pscan->savedSeekPos = 0;
		currRetries = 0;
		if (currRetries_chid) ca_array_put(DBR_LONG, 1, currRetries_chid, &currRetries);
		for (status = -1; status && currRetries<=maxAllowedRetries; ) {
			status = writeScanRecInProgress(pscan, pmsg->stamp, currRetries);
			if (status) {
				if (++currRetries<=maxAllowedRetries) {
					printf("saveData: ...will retry in %ld seconds\n", retryWaitInSecs);
					totalRetries++;
					if (totalRetries_chid) ca_array_put(DBR_LONG, 1, totalRetries_chid, &totalRetries);
					if (currRetries_chid) ca_array_put(DBR_LONG, 1, currRetries_chid, &currRetries);
					epicsThreadSleep((double)retryWaitInSecs);
				} else {
					printf("saveData: *******************************************\n");
					printf("saveData: too many retries; abandoning data from scan '%s'\n", pscan->name);
					printf("saveData: *******************************************\n");
					abandonedWrites++;
					if (abandonedWrites_chid) ca_array_put(DBR_LONG, 1, abandonedWrites_chid, &abandonedWrites);
				}
			}
		}

		epicsTimeGetCurrent(&now);
		Debug2(1, "%s header written (%.3fs)\n", pscan->name,
			(float)epicsTimeDiffInSeconds(&now, &openTime));
		DebugMsg2(2, "%s MSG_SCAN_DATA(0)= %f\n", pscan->name,
			(float)epicsTimeDiffInSeconds(&now, &pmsg->time));

	} else if ((pscan->data==0) && (pmsg->val==1)) {

		/********************** scan ended *****************************/

		pscan->data=1;
		/* Get buffered copy of cpt.  This copy goes with data arrays. */
		ca_array_get(DBR_LONG, 1, pscan->cbcpt, &pscan->bcpt);

		/* process the message */

		epicsTimeGetCurrent(&openTime);
		pscan->savedSeekPos = 0;
		currRetries = 0;
		if (currRetries_chid) ca_array_put(DBR_LONG, 1, currRetries_chid, &currRetries);
		for (status = -1; status && currRetries<=maxAllowedRetries; ) {
			status = writeScanRecCompleted(pscan, currRetries);
			if (status) {
				if (++currRetries<=maxAllowedRetries) {
					printf("saveData: ...will retry in %ld seconds\n", retryWaitInSecs);
					totalRetries++;
					if (totalRetries_chid) ca_array_put(DBR_LONG, 1, totalRetries_chid, &totalRetries);
					if (currRetries_chid) ca_array_put(DBR_LONG, 1, currRetries_chid, &currRetries);
					epicsThreadSleep((double)retryWaitInSecs);
				} else {
					printf("saveData: *******************************************\n");
					printf("saveData: too many retries; abandoning data from scan '%s'\n", pscan->name);
					printf("saveData: *******************************************\n\n");
					abandonedWrites++;
					if (abandonedWrites_chid) ca_array_put(DBR_LONG, 1, abandonedWrites_chid, &abandonedWrites);        }
			}
		}
		epicsTimeGetCurrent(&now);
		Debug2(1, "%s data written (%.3fs)\n", pscan->name,
			(float)epicsTimeDiffInSeconds(&now, &openTime));

		if (pscan->first_scan) {
			for (pnxt=pscan->nxt; pnxt; pnxt=pnxt->nxt) {
				pnxt->first_scan=TRUE;
			}
		}

		/* hand shaking notify */
		if (pscan->chandShake) {
			sval= HANDSHAKE_DONE;
			Debug3(5, "proc_scan_data(%s): done writing file %s; putting %d to .AWAIT\n",
				pscan->name, pscan->fname, sval);
			ca_array_put(DBR_SHORT, 1, pscan->chandShake, &sval);
		}

		/* enable file system record                                        */
		if (--nb_scan_running==0) {
			cval=(char)0;
			if (file_system_disp_chid) ca_array_put(DBR_CHAR, 1, file_system_disp_chid, &cval);
			cval=(char)0;
			if (file_subdir_disp_chid) ca_array_put(DBR_CHAR, 1, file_subdir_disp_chid, &cval);
			cval=(char)0;
			if (file_basename_disp_chid) ca_array_put(DBR_CHAR, 1, file_basename_disp_chid, &cval);
		}
		Debug1(2,"(save_status active) nb_scan_running=%d\n", nb_scan_running);

		epicsTimeGetCurrent(&now);
		DebugMsg2(2, "%s MSG_SCAN_DATA(1)= %f\n", pscan->name, 
			(float)epicsTimeDiffInSeconds(&now, &pmsg->time));

	}
	Debug2(5, "proc_scan_data(%s): exit(%d)\n", pscan->name, pmsg->val);
}

LOCAL void proc_scan_npts(SCAN_LONG_MSG* pmsg)
{
	SCAN* pscan;
	epicsTimeStamp now;

	pscan= pmsg->pscan;
	pscan->npts= pmsg->val;
	epicsTimeGetCurrent(&now);
	DebugMsg3(2, "%s MSG_SCAN_NPTS(%ld)= %f\n", pscan->name, pscan->npts, 
		(float)epicsTimeDiffInSeconds(&now, &pmsg->time));
}

LOCAL void proc_scan_cpt(SCAN_LONG_MSG* pmsg)
{
	int   i;
	long  lval;
	SCAN* pscan;
	FILE* fd;
	XDR   xdrs;
	epicsTimeStamp now, openTime;
	bool_t writeFailed = FALSE;
	char  msg[200];

	pscan = pmsg->pscan;

	if ((save_status == STATUS_INACTIVE) || (save_status == STATUS_ACTIVE_FS_ERROR)) {
		return;
	}

	pscan->cpt = pmsg->val;

	/* is the scan running ? */
	if ((pscan->data!=0) || (pscan->cpt==0)) {
		return;
	}
	Debug3(2, "saveData:proc_scan_cpt: saving %s[%ld] to %s\n", pscan->name, pscan->cpt-1, pscan->fname);
		
	for (i=0; i<SCAN_NBP; i++) {
		if ((pscan->rxnv[i]==XXNV_OK) || (pscan->pxnv[i]==XXNV_OK)) {
			if (pscan->crxcv[i]) ca_array_get(DBR_DOUBLE, 1, pscan->crxcv[i], &pscan->rxcv[i]);
		}
	}
	for (i=0; i<SCAN_NBD; i++) {
		if (pscan->dxnv[i]==XXNV_OK) {
			if (pscan->cdxcv[i]) ca_array_get(DBR_FLOAT, 1, pscan->cdxcv[i], &pscan->dxcv[i]);
		}
	}
	if (ca_pend_io(0.5)!=ECA_NORMAL) {
		/* error !!! */
		printf("saveData:proc_scan_cpt: unable to get current detector values !!!\n");
		return;
	}

	epicsTimeGetCurrent(&openTime);
	fd = fopen(pscan->ffname, "rb+");
	if (fd == NULL) {
			printf("saveData:proc_scan_cpt(%s): can't open data file!!\n", pscan->name);
			sprintf(msg, "!! Can't open file %s", pscan->fname);
			msg[MAX_STRING_SIZE-1] = '\0';
			sendUserMessage(msg);
			save_status = STATUS_ERROR;
			if (save_status_chid) ca_array_put(DBR_SHORT, 1, save_status_chid, &save_status);
			return;
	}

	xdrstdio_create(&xdrs, fd, XDR_ENCODE);

	/* point number  */
	writeFailed |= !xdr_setpos(&xdrs, pscan->cpt_fpos);
	if (writeFailed) goto cleanup;
	lval = pscan->cpt;
	writeFailed |= !xdr_long(&xdrs, &lval);
	if (writeFailed) goto cleanup;

	/* positioners and detectors values */
	if (pscan->nb_pos) {
		for (i=0; i<SCAN_NBP; i++) {
			if ((pscan->rxnv[i]==XXNV_OK) || (pscan->pxnv[i]==XXNV_OK)) {
				writeFailed |= !xdr_setpos(&xdrs, pscan->pxra_fpos[i]+(pscan->cpt-1)*sizeof(double));
				if (writeFailed) goto cleanup;
				writeFailed |= !xdr_double(&xdrs, &pscan->rxcv[i]);
			}
		}
	}
	if (writeFailed) goto cleanup;
	if (pscan->nb_det) {
		for (i=0; i<SCAN_NBD; i++) {
			if (pscan->dxnv[i]==XXNV_OK) {
				writeFailed |= !xdr_setpos(&xdrs, pscan->dxda_fpos[i]+(pscan->cpt-1)*sizeof(float));
				if (writeFailed) goto cleanup;
				writeFailed |= !xdr_float(&xdrs, &pscan->dxcv[i]);
			}
			if (writeFailed) goto cleanup;
		}
	}

	if (save_status == STATUS_ERROR) {
			sprintf(msg, "Wrote data to %s", pscan->fname);
			msg[MAX_STRING_SIZE-1] = '\0';
			sendUserMessage(msg);
			save_status = STATUS_ACTIVE_OK;
			if (save_status_chid) ca_array_put(DBR_SHORT, 1, save_status_chid, &save_status);
	}

cleanup:
	xdr_destroy(&xdrs);
	fclose(fd);
	epicsTimeGetCurrent(&now);
	Debug2(1, "saveData:proc_scan_cpt:%s data point written (%.3fs)\n", pscan->name,
		(float)epicsTimeDiffInSeconds(&now, &openTime));
}


LOCAL void proc_scan_pxnv(SCAN_INDEX_MSG* pmsg)
{
	SCAN* pscan;
	int   i;
	short val;
	char  buff[PVNAME_STRINGSZ];
	int   len, got_it;
	epicsTimeStamp now;

	pscan= pmsg->pscan;
	i= pmsg->index;
	val= (short)pmsg->val;
	/* get PxNV value                                                     */
	pscan->pxnv[i]= val;
	pscan->pxpv[i][0]='\0';
	pscan->pxds[i][0]='\0';
	pscan->pxeu[i].units[0]='\0';

	/* clear previous desc monitors */
	if (pscan->cpxds[i]) {
		ca_clear_channel(pscan->cpxds[i]);
		pscan->cpxds[i]= NULL;
	}
	/* clear previous unit channel */
	if (pscan->cpxeu[i]) {
		ca_clear_channel(pscan->cpxeu[i]);
		pscan->cpxeu[i]= NULL;
	}

	if (val==XXNV_OK) {
		/* the pvname is valid, get it.                                     */
		got_it = 0;
		if (pscan->cpxpv[i]) {
			ca_array_get(DBR_STRING, 1, pscan->cpxpv[i], pscan->pxpv[i]);
			if (ca_pend_io(2.0)==ECA_NORMAL) got_it = 1;
		}
		if (!got_it) {
			Debug2(2, "Unable to get %s.%s\n", pscan->name, pxpv[i]);
			strcpy(pscan->pxpv[i], "ERROR");
		} else {
			/* Try to connect the positioner DESC field */
			len= strcspn(pscan->pxpv[i], ".");
			strncpy(buff, pscan->pxpv[i], len);
			buff[len]='\0';
			strcat(buff, ".DESC");
			ca_search(buff, &pscan->cpxds[i]);
			if (ca_pend_io(2.0)!=ECA_NORMAL) {
				Debug1(2, "Unable to connect %s\n", buff);
				ca_clear_channel(pscan->cpxds[i]);
				pscan->cpxds[i]=NULL;
			} else {
				if (pscan->cpxds[i]) ca_add_array_event(DBR_STRING, 1, pscan->cpxds[i], 
					descMonitor, pscan->pxds[i], (float)0,(float)0,(float)0, NULL);
			}

			/* Try to connect the positioner */
			ca_search(pscan->pxpv[i], &pscan->cpxeu[i]);
			if (ca_pend_io(2.0)!=ECA_NORMAL) {
				Debug1(2, "Unable to connect %s\n", pscan->pxpv[i]);
				if (pscan->cpxeu[i]) ca_clear_channel(pscan->cpxeu[i]);
				pscan->cpxeu[i]=NULL;
			} else {
				if (pscan->cpxeu[i]) {
					ca_array_get(DBR_CTRL_DOUBLE, 1, pscan->cpxeu[i], &pscan->pxeu[i]);
					ca_pend_io(2.0);
				}
			}
		}
	}
	epicsTimeGetCurrent(&now);
	DebugMsg3(2, "%s MSG_SCAN_PXNV(%d)= %f\n", pscan->name, val, 
		(float)epicsTimeDiffInSeconds(&now, &pmsg->time));
}

LOCAL void proc_scan_pxsm(STRING_MSG* pmsg)
{
	epicsTimeStamp now;

	strncpy(pmsg->pdest, pmsg->string, MAX_STRING_SIZE-1);
	pmsg->pdest[MAX_STRING_SIZE-1]='\0';

	epicsTimeGetCurrent(&now);
	DebugMsg2(2, "MSG_SCAN_PXSM(%s)= %f\n", pmsg->string, 
		(float)epicsTimeDiffInSeconds(&now, &pmsg->time));
}

LOCAL void proc_scan_rxnv(SCAN_INDEX_MSG* pmsg)
{
	SCAN* pscan;
	int   i;
	short val;
	char  buff[PVNAME_STRINGSZ];
	int   len, got_it;
	epicsTimeStamp now;

	pscan= pmsg->pscan;
	i= pmsg->index;
	val= (short)pmsg->val;
	/* Get RxNV value                                                     */
	pscan->rxnv[i]= val;
	pscan->rxpv[i][0]='\0';
	pscan->rxds[i][0]='\0';
	pscan->rxeu[i].units[0]='\0';

	/* clear previous desc monitors */
	if (pscan->crxds[i]) {
		ca_clear_channel(pscan->crxds[i]);
		pscan->crxds[i]= NULL;
	}
	/* clear previous unit channel*/
	if (pscan->crxeu[i]) {
		ca_clear_channel(pscan->crxeu[i]);
		pscan->crxeu[i]= NULL;
	}

	/*
	 * Note this is different from, say, proc_scan_pxnv, because we permit the
	 * pseudo PV names "time" and "TIME".  These aren't valid in a "val==XXNV_OK"
	 * sense, because they are not the names of actual PV's, so we don't try to
	 * get a description, etc.
	 */

	/* Get the readback pvname                                            */
	got_it = 0;
	if (pscan->crxpv[i]) {
		ca_array_get(DBR_STRING, 1, pscan->crxpv[i], pscan->rxpv[i]);
		if (ca_pend_io(0.5)==ECA_NORMAL) got_it = 1;
	}
	if (!got_it) {
		Debug2(2, "Unable to get %s.%s\n", pscan->name, rxpv[i]);
		strcpy(pscan->rxpv[i], "ERROR");
	} else {
		if (val==XXNV_OK) {
			/* The pvname is valid                                            */
			/* Try to connect the readback DESC field                         */
			len= strcspn(pscan->rxpv[i], ".");
			strncpy(buff, pscan->rxpv[i], len);
			buff[len]='\0';
			strcat(buff, ".DESC");
			ca_search(buff, &pscan->crxds[i]);
			if (ca_pend_io(2.0)!=ECA_NORMAL) {
				Debug1(2, "Unable to connect %s\n", buff);
				ca_clear_channel(pscan->crxds[i]);
				pscan->crxds[i]=NULL;
			} else {
				ca_add_array_event(DBR_STRING, 1, pscan->crxds[i],  descMonitor,
					pscan->rxds[i], (float)0,(float)0,(float)0, NULL);
			}
			/* Try to connect the readback */
			ca_search(pscan->rxpv[i], &pscan->crxeu[i]);
			if (ca_pend_io(2.0)!=ECA_NORMAL) {
				Debug1(2, "Unable to connect %s\n", pscan->rxpv[i]);
				ca_clear_channel(pscan->crxeu[i]);
				pscan->crxeu[i]=NULL;
			} else {
				ca_array_get(DBR_CTRL_DOUBLE, 1, pscan->crxeu[i], &pscan->rxeu[i]);
				ca_pend_io(2.0);
			}
		} else {
			/* the pvname is not valid                                        */
			/* Check for time or TIME                                         */
			if ((strcmp(pscan->rxpv[i], "time")==0) || 
				 (strcmp(pscan->rxpv[i], "TIME")==0)) {
				pscan->rxnv[i]=XXNV_OK;
				strcpy(pscan->rxeu[i].units, "second");
			} else {
				pscan->rxpv[i][0]='\0';
			}
		}
	}
	epicsTimeGetCurrent(&now);
	DebugMsg3(2, "%s MSG_SCAN_RXNV(%d)= %f\n", pscan->name, val, 
		(float)epicsTimeDiffInSeconds(&now, &pmsg->time));
}


LOCAL void proc_scan_dxnv(SCAN_INDEX_MSG* pmsg)
{
	SCAN* pscan;
	int   i;
	short val;
	char  buff[PVNAME_STRINGSZ];
	int   len, got_it;
	char  msg[200];
	epicsTimeStamp now;

	pscan= pmsg->pscan;
	i= pmsg->index;
	val= (short)pmsg->val;

	pscan->dxnv[i]= val;
	pscan->dxpv[i][0]='\0';
	pscan->dxds[i][0]='\0';
	pscan->dxeu[i].units[0]='\0';

	/* clear previous desc monitors */
	if (pscan->cdxds[i]) {
		ca_clear_channel(pscan->cdxds[i]);
		pscan->cdxds[i]= NULL;
	}
	/* clear previous unit channel*/
	if (pscan->cdxeu[i]) {
		ca_clear_channel(pscan->cdxeu[i]);
		pscan->cdxeu[i]= NULL;
	}

	if (val==XXNV_OK) {
#if (ALLOC_ALL_DETS == 0)
		if (pscan->dxda[i] == NULL) pscan->dxda[i]=(float*)calloc(pscan->mpts, sizeof(float));
		if (pscan->dxda[i] == NULL) {
			printf("saveData: Can't alloc array for det %s.%s\n", pscan->name, dxda[i]);
			sprintf(msg, "!! No mem for %s.%s", pscan->name, dxda[i]);
			msg[MAX_STRING_SIZE-1]= '\0';
			sendUserMessage(msg);
		}
#endif
		got_it = 0;
		if (pscan->cdxpv[i]) {
			ca_array_get(DBR_STRING, 1, pscan->cdxpv[i], pscan->dxpv[i]);
			if (ca_pend_io(1.0)==ECA_NORMAL) got_it = 1;
		}
		if (!got_it) {
			Debug2(2, "Unable to get %s.%s\n", pscan->name, dxpv[i]);
			strcpy(pscan->dxpv[i], "ERROR");
		} else {
			/* Try to connect the detector DESC field                         */
			len= strcspn(pscan->dxpv[i], ".");
			strncpy(buff, pscan->dxpv[i], len);
			buff[len]='\0';
			strcat(buff, ".DESC");
			ca_search(buff, &pscan->cdxds[i]);
			if (ca_pend_io(2.0)!=ECA_NORMAL) {
				Debug1(2, "Unable to connect %s\n", buff);
				ca_clear_channel(pscan->cdxds[i]);
				pscan->cdxds[i]=NULL;
			} else {
				ca_add_array_event(DBR_STRING, 1, pscan->cdxds[i], descMonitor,
					pscan->dxds[i], (float)0,(float)0,(float)0, NULL);
			}
			/* Try to connect the detector */
			ca_search(pscan->dxpv[i], &pscan->cdxeu[i]);
			if (ca_pend_io(2.0)!=ECA_NORMAL) {
				Debug1(2, "Unable to connect %s\n", pscan->dxpv[i]);
				ca_clear_channel(pscan->cdxeu[i]);
				pscan->cdxeu[i]=NULL;
			} else {
				ca_array_get(DBR_CTRL_FLOAT, 1, pscan->cdxeu[i], &pscan->dxeu[i]);
				ca_pend_io(2.0);
			}
		}
	}
	epicsTimeGetCurrent(&now);
	DebugMsg3(2, "%s MSG_SCAN_DXNV(%d)= %f\n", pscan->name, val, 
		(float)epicsTimeDiffInSeconds(&now, &pmsg->time));
}


LOCAL void proc_scan_txnv(SCAN_INDEX_MSG* pmsg)
{
	SCAN* pscan;
	int   i;
	short val;
	int   len, got_it;
	epicsTimeStamp now;

	pscan= pmsg->pscan;
	i= pmsg->index;
	val= (short)pmsg->val;

	pscan->txsc[i]= 1;    /* presume we're NOT linked to another sscan record */
	pscan->txnv[i]= val;
	pscan->txpv[i][0]= '\0';
	pscan->txpvRec[i][0]= '\0';

	if (val==XXNV_OK) {
		got_it = 0;
		if (pscan->ctxpv[i]) {
			ca_array_get(DBR_STRING, 1, pscan->ctxpv[i], pscan->txpv[i]);
			if (ca_pend_io(2.0)==ECA_NORMAL) got_it = 1;
		}
		if (!got_it) {
			Debug2(2, "Unable to get %s.%s\n", pscan->name, txpv[i]);
			pscan->txpv[i][0]='\0';
			pscan->txpvRec[i][0]='\0';
		} else {
			strcpy(pscan->txpvRec[i], pscan->txpv[i]);
			len= strcspn(pscan->txpvRec[i], ".");
			pscan->txsc[i]= strncmp(&pscan->txpv[i][len], ".EXSC", 6);
			pscan->txpvRec[i][len]='\0';
		}
	}

	updateScan(pscan);

	epicsTimeGetCurrent(&now);
	DebugMsg3(2, "%s MSG_SCAN_TXNV(%d)= %f\n", pscan->name, val, 
		(float)epicsTimeDiffInSeconds(&now, &pmsg->time));
}

LOCAL void proc_scan_txcd(SCAN_INDEX_MSG* pmsg)
{
	SCAN* pscan;
	int   i;
	epicsTimeStamp now;

	pscan= pmsg->pscan;
	i= pmsg->index;

	pscan->txcd[i]= (float)pmsg->val;

	updateScan(pscan);

	epicsTimeGetCurrent(&now);
	DebugMsg3(2, "%s MSG_SCAN_TXCD(%f)= %f\n", pscan->name, (float)pmsg->val, 
		(float)epicsTimeDiffInSeconds(&now, &pmsg->time));
}

LOCAL void proc_desc(STRING_MSG* pmsg)
{
	epicsTimeStamp now;

	strncpy(pmsg->pdest, pmsg->string, MAX_STRING_SIZE-1);
	pmsg->pdest[MAX_STRING_SIZE-1]= '\0';

	epicsTimeGetCurrent(&now);
	DebugMsg2(2, "MSG_DESC(%s)= %f\n", pmsg->string, 
		(float)epicsTimeDiffInSeconds(&now, &pmsg->time));
}

LOCAL void proc_egu(STRING_MSG* pmsg)
{
	epicsTimeStamp now;

	strncpy(pmsg->pdest, pmsg->string, 15);
	pmsg->pdest[15]= '\0';

	epicsTimeGetCurrent(&now);
	DebugMsg2(2, "MSG_EGU(%s)= %f\n", pmsg->string, 
		(float)epicsTimeDiffInSeconds(&now, &pmsg->time));
}


LOCAL void proc_file_system(STRING_MSG* pmsg)
{
	pmsg->string[MAX_STRING_SIZE-1]='\0';
	DebugMsg1(2, "MSG_FILE_SYSTEM(%s)\n", pmsg->string);
	remount_file_system(pmsg->string);
}


LOCAL void remount_file_system(char* filesystem)
{
	char  msg[MAX_STRING_SIZE];
	char *path = local_pathname;
#ifdef vxWorks
	char  hostname[40];
	char *cout;
#endif

#ifdef vxWorks
	nfsUnmount("/data");
#endif

	file_system_state= FS_NOT_MOUNTED;
	save_status= STATUS_ACTIVE_FS_ERROR;

	/* reset subdirectory to "" */
	if (*local_subdir!='\0') {
		*local_subdir='\0';
		if (file_subdir_chid) ca_array_put(DBR_STRING, 1, file_subdir_chid, local_subdir);
	}
	server_pathname[0]='\0';
	server_subdir= server_pathname;

#ifdef vxWorks

	if ((*(filesystem++)!='/') || (*(filesystem++)!='/')) {
		strcpy(msg, "Invalid file system !!!");
	} else {
		/* extract the host name */
		cout= hostname;
		while ((*filesystem!='\0') && (*filesystem!='/'))
			*(cout++)= *(filesystem++);
		*cout='\0';
		
		/* Mount the new file system */
		if (nfsMount(hostname, filesystem, "/data")==ERROR) {
			strcpy(msg, "Unable to mount file system !!!!");
		} else {
			file_system_state= FS_MOUNTED;
			path = local_pathname;
		}
	}

#else

	file_system_state= FS_MOUNTED;
	path = server_pathname;
	
#endif

	if (file_system_state == FS_MOUNTED) {
		strcpy(server_pathname, filesystem);
		strcat(server_pathname, "/");
		server_subdir= &server_pathname[strlen(server_pathname)];  

		if (checkRWpermission(path)!=OK) {
			strcpy(msg, "RW permission denied !!!");
		} else {
			strcpy(msg, "saveData OK");
			save_status= STATUS_ACTIVE_OK;
		}
	}

	if (full_pathname_chid) {
		ca_array_put(DBR_CHAR, strlen(server_pathname)+1,
			full_pathname_chid, server_pathname);
	}
	sendUserMessage(msg);
	ca_array_put(DBR_SHORT, 1, save_status_chid, &save_status);
}

LOCAL void proc_file_subdir(STRING_MSG* pmsg)
{
	char msg[MAX_STRING_SIZE];
	char* cin;
	char* server;
	char* local;
	epicsTimeStamp now;
	char *path = local_pathname;
#ifdef vxWorks
	int fd;
#endif

	if (file_system_state==FS_MOUNTED) {

		cin= pmsg->string;

		/* the new directory should be different from the previous one */
		if (strcmp(cin, local_subdir)==0) return;
		/* assume failure until we prove that we can create a file. */
		save_status= STATUS_ACTIVE_FS_ERROR;

		server= server_subdir;
		local= local_subdir;

		*server= *local= '\0';

		while ((*cin!='\0') && (*cin=='/')) cin++;
		
		while (*cin!='\0') {
			while ((*cin!='\0') && (*cin!='/')) {
				*server= *local= *cin;
				if (*cin==' ') *server=*local= '_';
				server++;
				local++;
				cin++;
			}
			/* NULL terminate file_subdirectory */
			*server= *local= '\0';
			/* skip all trailling '/' */
			while ((*cin!='\0') && (*cin=='/')) cin++;
			/* create directory */
#ifdef vxWorks
			path = local_pathname;
			fd = open (local_pathname, O_RDWR | O_CREAT,
				FSTAT_DIR | DEFAULT_DIR_PERM | 0775);
			if (fd!=ERROR) close(fd);
#else
			path = server_pathname;
			mkdir(server_pathname,0775);
#endif
			/* append '/' */
			*(server++)= *(local++)= '/';
			*(server)= *(local)= '\0';
		}

		if (fileStatus(path)!=OK) {
			strcpy(msg, "Invalid directory !!!");
			*server_subdir=*local_subdir= '\0';
		} else if (checkRWpermission(path)!=OK) {
			strcpy(msg, "RW permission denied !!!");
			*server_subdir=*local_subdir= '\0';
		} else {
			strcpy(msg, "saveData OK");
			save_status= STATUS_ACTIVE_OK;
		}

		if (full_pathname_chid) {
			ca_array_put(DBR_CHAR, strlen(server_pathname)+1,
				full_pathname_chid, server_pathname);
		}
		sendUserMessage(msg);
		if (save_status_chid) ca_array_put(DBR_SHORT, 1, save_status_chid, &save_status);
	}
	epicsTimeGetCurrent(&now);
	DebugMsg2(2, "MSG_FILE_SUBDIR(%s)= %f\n", pmsg->string, 
		(float)epicsTimeDiffInSeconds(&now, &pmsg->time));
}

LOCAL void proc_file_basename(STRING_MSG* pmsg)
{
	strncpy(scanFile_basename, pmsg->string, BASENAME_SIZE-1);
	scanFile_basename[BASENAME_SIZE-1] = '\0';
	DebugMsg1(2, "MSG_FILE_BASENAME(%s)\n", pmsg->string);
}

LOCAL void proc_realTime1D(INTEGER_MSG* pmsg)
{
	epicsTimeStamp now;

	if (realTime1D!= pmsg->val) {
		realTime1D= pmsg->val;
		updateScans();
	}
	epicsTimeGetCurrent(&now);
	DebugMsg2(2, "proc_realTime1D: MSG_REALTIME1D(%d)= %f\n", pmsg->val,
		(float)epicsTimeDiffInSeconds(&now, &pmsg->time));
}

/*----------------------------------------------------------------------*/
/* The task in charge of updating and saving scans                      */
/*                                                                      */
LOCAL int saveDataTask(void *parm)
{
	epicsThreadId Mommy = (epicsThreadId)parm; 
	char*    pmsg;
	int*     ptype;

	Debug0(1, "Task saveDataTask running...\n");

	cpt_wait_time = 0.1; /* seconds */

	SEVCHK(ca_context_create(ca_enable_preemptive_callback),"ca_context_create");

	if (initSaveDataTask()==-1) {
		printf("saveData: Unable to configure saveDataTask\n");
		if (epicsThreadIsSuspended(Mommy)) epicsThreadResume(Mommy);
		ca_context_destroy();
		return -1;
	}

	pmsg= (char*) malloc(MAX_SIZE);
	ptype= (int*)pmsg;
	if (!pmsg) {
		printf("saveData: Not enough memory to allocate message buffer\n");
		if (epicsThreadIsSuspended(Mommy)) epicsThreadResume(Mommy);
		ca_context_destroy();
		return -1;
	}

	Debug0(1, "saveDataTask waiting for messages\n");
	if (epicsThreadIsSuspended(Mommy)) epicsThreadResume(Mommy);

	while (1) {
		/* waiting for messages */
		if (epicsMessageQueueReceive(msg_queue, pmsg, MAX_SIZE) < 0) {
			/* no message received */
			Debug0(1, "saveDataTask: epicsMessageQueueReceive returned neg. number\n");
			break;
		}

		switch(*ptype) {

		case MSG_SCAN_DATA:
			Debug1(2, "saveDataTask: MSG_SCAN_DATA, val=%d\n", ((SCAN_TS_SHORT_MSG*)pmsg)->val);
			proc_scan_data((SCAN_TS_SHORT_MSG*)pmsg);
			break;

		case MSG_SCAN_NPTS:
			Debug1(2, "saveDataTask: MSG_SCAN_NPTS, val=%ld\n", ((SCAN_LONG_MSG*)pmsg)->val);
			proc_scan_npts((SCAN_LONG_MSG*)pmsg);
			break;

		case MSG_SCAN_CPT:
			Debug1(2, "saveDataTask: MSG_SCAN_CPT, val=%ld\n", ((SCAN_LONG_MSG*)pmsg)->val);
			proc_scan_cpt((SCAN_LONG_MSG*)pmsg);
			break;

		case MSG_SCAN_PXNV:
			Debug2(2, "saveDataTask: MSG_SCAN_PXNV, ix=%d, val=%f\n", ((SCAN_INDEX_MSG*)pmsg)->index, ((SCAN_INDEX_MSG*)pmsg)->val);
			proc_scan_pxnv((SCAN_INDEX_MSG*)pmsg);
			break;

		case MSG_SCAN_PXSM:
			Debug1(2, "saveDataTask: MSG_SCAN_PXSM, val=%s\n", ((STRING_MSG*)pmsg)->string);
			proc_scan_pxsm((STRING_MSG*)pmsg);
			break;

		case MSG_SCAN_RXNV:
			Debug2(2, "saveDataTask: MSG_SCAN_RXNV, ix=%d, val=%f\n", ((SCAN_INDEX_MSG*)pmsg)->index, ((SCAN_INDEX_MSG*)pmsg)->val);
			proc_scan_rxnv((SCAN_INDEX_MSG*)pmsg);
			break;

		case MSG_SCAN_DXNV:
			Debug2(2, "saveDataTask: MSG_SCAN_DXNV, ix=%d, val=%f\n", ((SCAN_INDEX_MSG*)pmsg)->index, ((SCAN_INDEX_MSG*)pmsg)->val);
			proc_scan_dxnv((SCAN_INDEX_MSG*)pmsg);
			break;

		case MSG_SCAN_TXNV:
			Debug2(2, "saveDataTask: MSG_SCAN_TXNV, ix=%d, val=%f\n", ((SCAN_INDEX_MSG*)pmsg)->index, ((SCAN_INDEX_MSG*)pmsg)->val);
			proc_scan_txnv((SCAN_INDEX_MSG*)pmsg);
			break;

		case MSG_SCAN_TXCD:
			Debug2(2, "saveDataTask: MSG_SCAN_TXCD, ix=%d, val=%f\n", ((SCAN_INDEX_MSG*)pmsg)->index, ((SCAN_INDEX_MSG*)pmsg)->val);
			proc_scan_txcd((SCAN_INDEX_MSG*)pmsg);
			break;

		case MSG_DESC:
			Debug1(2, "saveDataTask: MSG_DESC, val=%s\n", ((STRING_MSG*)pmsg)->string);
			proc_desc((STRING_MSG*)pmsg);
			break;
		case MSG_EGU:
			Debug1(2, "saveDataTask: MSG_EGU, val=%s\n", ((STRING_MSG*)pmsg)->string);
			proc_egu((STRING_MSG*)pmsg);
			break;

		case MSG_FILE_SYSTEM:
			Debug1(2, "saveDataTask: MSG_FILE_SYSTEM, val=%s\n", ((STRING_MSG*)pmsg)->string);
			proc_file_system((STRING_MSG*)pmsg);
			break;

		case MSG_FILE_SUBDIR:
			Debug1(2, "saveDataTask: MSG_FILE_SUBDIR, val=%s\n", ((STRING_MSG*)pmsg)->string);
			proc_file_subdir((STRING_MSG*)pmsg);
			break;

		case MSG_FILE_BASENAME:
			Debug1(2, "saveDataTask: MSG_FILE_BASENAME, val=%s\n", ((STRING_MSG*)pmsg)->string);
			proc_file_basename((STRING_MSG*)pmsg);
			break;

		case MSG_REALTIME1D:
			Debug1(2, "saveDataTask: MSG_REALTIME1D, val=%d\n", ((INTEGER_MSG*)pmsg)->val);
			proc_realTime1D((INTEGER_MSG*)pmsg);
			break;

		default: 
			Debug1(2, "Unknown message: #%d", *ptype);
		}
	}
	return 0;
}


/****************************** epicsExport ****************************/
#include "epicsExport.h"
#include "iocsh.h"

epicsExportAddress(int, debug_saveData);
epicsExportAddress(int, debug_saveDataMsg);
epicsExportAddress(int, saveData_MessagePolicy);

/* void saveData_Init(char* fname, char* macros) */
static const iocshArg saveData_Init_Arg0 = { "fname", iocshArgString};
static const iocshArg saveData_Init_Arg1 = { "macros", iocshArgString};
static const iocshArg * const saveData_Init_Args[2] = {&saveData_Init_Arg0, &saveData_Init_Arg1};
static const iocshFuncDef saveData_Init_FuncDef = {"saveData_Init", 2, saveData_Init_Args};
static void saveData_Init_CallFunc(const iocshArgBuf *args) {saveData_Init(args[0].sval, args[1].sval);}

/* void saveData_PrintScanInfo(char* name) */
static const iocshArg saveData_PrintScanInfo_Arg0 = { "name", iocshArgString};
static const iocshArg * const saveData_PrintScanInfo_Args[1] = {&saveData_PrintScanInfo_Arg0};
static const iocshFuncDef saveData_PrintScanInfo_FuncDef = {"saveData_PrintScanInfo", 1, saveData_PrintScanInfo_Args};
static void saveData_PrintScanInfo_CallFunc(const iocshArgBuf *args) {saveData_PrintScanInfo(args[0].sval);}

/* void saveData_Priority(int p) */
static const iocshArg saveData_Priority_Arg0 = { "priority", iocshArgInt};
static const iocshArg * const saveData_Priority_Args[1] = {&saveData_Priority_Arg0};
static const iocshFuncDef saveData_Priority_FuncDef = {"saveData_Priority", 1, saveData_Priority_Args};
static void saveData_Priority_CallFunc(const iocshArgBuf *args) {saveData_Priority(args[0].ival);}

/* void saveData_SetCptWait_ms(int ms) */
static const iocshArg saveData_SetCptWait_ms_Arg0 = { "ms", iocshArgInt};
static const iocshArg * const saveData_SetCptWait_ms_Args[1] = {&saveData_SetCptWait_ms_Arg0};
static const iocshFuncDef saveData_SetCptWait_ms_FuncDef = {"saveData_SetCptWait_ms", 1, saveData_SetCptWait_ms_Args};
static void saveData_SetCptWait_ms_CallFunc(const iocshArgBuf *args) {saveData_SetCptWait_ms(args[0].ival);}

/* void saveData_Version(void) */
static const iocshFuncDef saveData_Version_FuncDef = {"saveData_Version", 0, NULL};
static void saveData_Version_CallFunc(const iocshArgBuf *args) {saveData_Version();}

/* void saveData_CVS(void) */
static const iocshFuncDef saveData_CVS_FuncDef = {"saveData_CVS", 0, NULL};
static void saveData_CVS_CallFunc(const iocshArgBuf *args) {saveData_CVS();}

/* void saveData_Info(void) */
static const iocshFuncDef saveData_Info_FuncDef = {"saveData_Info", 0, NULL};
static void saveData_Info_CallFunc(const iocshArgBuf *args) {saveData_Info();}

/* collect all functions */
void saveDataRegistrar(void)
{
	iocshRegister(&saveData_Init_FuncDef, saveData_Init_CallFunc);
	iocshRegister(&saveData_PrintScanInfo_FuncDef, saveData_PrintScanInfo_CallFunc);
	iocshRegister(&saveData_Priority_FuncDef, saveData_Priority_CallFunc);
	iocshRegister(&saveData_SetCptWait_ms_FuncDef, saveData_SetCptWait_ms_CallFunc);
	iocshRegister(&saveData_Version_FuncDef, saveData_Version_CallFunc);
	iocshRegister(&saveData_CVS_FuncDef, saveData_CVS_CallFunc);
	iocshRegister(&saveData_Info_FuncDef, saveData_Info_CallFunc);
}

epicsExportRegistrar(saveDataRegistrar);
