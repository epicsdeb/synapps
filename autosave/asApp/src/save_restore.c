/* 12/12/96  tmm  v1.3 get doubles and doubles native (independent of
 *                the EPICS "precision" setting, as is BURT) and convert
 *                them using #define FLOAT_FMT, DOUBLE_FMT, below.
 * 01/03/97  tmm  v1.4 make backup save file if save succeeds
 * 06/03/97  tmm  v1.5 write doubles and doubles in g format. Fixed fdbrestore.
 * 04/26/99  tmm  v1.6 set first character of save file to '&' until we've
 *                written and flushed everything, then set it to '#'.
 * 05/11/99  tmm  v1.7 make sure backup save file is ok before writing new
 *                save file.
 * 11/23/99  tmm  v1.8 defend against condition in which ioc does not have
 *                write access to directory, but does have write access to
 *                existing autosave file.  In this case, we can't erase the
 *                file or change its length.
 * 01/05/01  tmm  v1.9 proper treatment of event_handler_args in function
 *                argument list.
 * 05/11/01  tmm  v2.0 defend against null trigger channel.
 * 12/20/01  tmm  v2.1 Test file open only if save_restore_test_fopen != 0
 * 02/22/02  tmm  v2.2 Work around for problems with USHORT fields
 * 02/26/02  tmm  v2.3 Adding features from Frank Lenkszus' version of the code:
 *                replaced logMsg with epicsPrintf  [that was a mistake!!]
 *                added set_savefile_path(char *path)
 *			      added set_requestfile_path(char *path)
 *			      added set_saveTask_priority(int priority)
 *			      added remove_data_set(char *filename)
 *                added reload_periodic_set(char * filename, double period)
 *                added reload_triggered_set(char *filename, char *trig_channel)
 *                added reload_monitor_set(char * filename, double period)
 *                added reload_manual_set(char * filename)
 *                check for errors on backup file creation
 *                check for errors on writing save file
 *                don't overwrite backup file if previous save failed
 *                added fdbrestoreX(char *filename) to restore from backup file
 *
 *                in set_XXX_path(), allow path with or without trailing '/'
 * 03/08/02  tmm  v2.4 Use William Lupton's macLib to simplify request files.
 *                Command-line arguments redefined from double to int. (Required
 *                for PPC.)
 * 03/13/02  tmm  v2.5 Allow for multiple directories in reqFilePath.
 * 03/15/02  tmm  v2.6 check saveRestoreFilePath before using it.
 * 04/22/03  tmm  v2.7 Add add_XXXfile_path(): like set_XXXfile_path(),
 *                but allows caller to pass path as two args to be concatenated.
 * 04/23/03  tmm  v2.8 Add macro-string argument to create_xxx_set(), added
 *                argument 'verbose' to save_restoreShow();
 * 04/28/03  tmm  v2.9 Drop add_XXXfile_path, and add second argument to
 *                set_XXXfile_path();
 * 05/20/03  tmm  v2.9a Looking for problems that cause save_restore() to hang
 * 07/14/03  tmm  v3.0 In addition to .sav and .savB, can save/restore <= 10
 *                sequenced files .sav0 -.sav9, which are written at preset
 *                intervals independent of the channel-list settings.  No longer
 *                test fopen() before save.  This caused more problems than it
 *                solved.
 * 07/21/03  tmm  v3.1 More checks during file writing.
 * 08/13/03  tmm  v3.2 Status PV's.  Merge bug fixes from 3.13 and 3.14 versions into
 *                something that will work under 3.13
 * 08/19/03  tmm  v3.3 Manage NFS mount.  Replace epicsPrintf with errlogPrintf.
 *                Replace logMsg with errlogPrintf except in callback routines.
 * 09/04/03  tmm  v3.4 Do manual restore operations (fdbrestore*) in the save_restore
 *                task.  With this change, all CA work is done by the save_restore task.
 * 04/12/04  tmm  v3.5 Added array support. fdbrestore* was reimplmented as a macro,
 *                but this meant that user could not invoke from command line.
 * 04/20/04  tmm  v4.0 3.13-compatible v3.5, with array support, converted to
 *                EPICS 3.14, though NFS stuff is #ifdef vxWorks
 *                In addition to .sav and .savB, can save/restore <= 10
 *                sequenced files .sav0 -.sav9, which are written at preset
 *                intervals independent of the channel-list settings.
 *                Status PV's have been reworked so database and medm file
 *                don't have to contain the names of save sets.
 * 04/23/04  tmm  List linking; was failing when readReqFile was called recursively.
 * 05/14/04  tmm  New status PV's.  The old ones had the save-set name as part of
 *                the record names.  Now, PV's are generic, and the save-set name
 *                is written to them.
 * 06/28/04  tmm  Check that chid is non-NULL before calling ca_state().
 * 10/04/04  tmm  Allow DOS line termination (CRLF) or unix (LF) in .sav file.
 * 10/29/04  tmm  v4.1 Init all strings to "".  Check that sr_mutex exists in
 *                save_restoreShow().
 * 11/03/04  tmm  v4.2 Changed the way time strings are processed.  Added some code
 *                to defend against partial init failures.
 * 11/30/04  tmm  v4.3 Arrays now maintain max and curr number of elements, and
 *                save_restore tracks changes in curr_elements.  Previously,
 *                if dbGet() set num_elements to zero for a PV, that was the last
 *                time save_restore looked at the PV.
 * 12/13/04  tmm  v4.4 Use epicsThreadGetStackSize(epicsThreadStackMedium)
 *                instead of hard-coded stack size.
 * 01/28/05  tmm  v4.5 Increased size of status-prefix to 29 bytes+NULL.  Added
 *                new status value: 'No Status'
 * 02/16/05  tmm  v4.6 Fixed faulty building of PV-name strings for status PV's
 *                Post selected save-set status strings promptly.
 * 06/27/05  tmm  v4.7 Request-file parsing fixed.  Previously, blank but not
 *                empty lines (e.g., containing a space character) seemed to
 *                contain the first word of the previous line, because name[]
 *                was not cleared before parsing a new line.
 * 02/02/06  tmm  v4.8 Don't quit trying to write file just because errno got set.
 *                Status PV's were being restricted to STRING_LEN chars, instead
 *                of PV_NAME_LEN chars.  (Thanks to Kay Kasemir for diagnosing and
 *                fixing these problems.)
 * 03/24/06  tmm  v4.9 Use epicsThreadGetStackSize(epicsThreadStackBig)
 * 03/28/06  tmm  Replaced all Debug macros with straight code
 *...
 * 09/07/06  tmm  v5.0 Don't hold sr_mutex; instead, use it to protect the
 *                variable listLock, which we can hold.  (vxWorks nversion-safe
 *                mutex was keeping save_restore task at high priority for
 *                much lomger than necessary.)
 * 12/06/06  tmm  v5.1 Don't even print the value of errno if fprintf() sets it.
 *                In tornado 2.2.1, fprintf() is setting errno every time, so
 *                the info is useless for diagnostic purposes.
 * 03/16/07  tmm  v5.2 create_xxx_set can now specify PV names from which file
 *                path and file name are to be read, by including SAVENAMEPV
 *                and/or SAVEPATHPV in the macro string supplied as an argument.
 *                Note this can be done only on the first call to create_xxx_set()
 *                for a save set, because only the first call results in a call to
 *                readReqFile(), in which the macro string is parsed.  If either
 *                macro is specified, save_restore will not maintain backup or
 *                files for the save set.
 * 03/19/07  tmm  v5.2 Don't print errno unless function returns an error.
 * 09/21/07  tmm  v5.3 New function, save_restoreSet_FilePermissions(),  allows control
 *                over permissions with which .sav files are created.
 * 11/19/07  tmm  v5.4 After failing to write a save file, retry after
 *                save_restoreRetrySeconds.  (New function, save_restoreSet_RetrySeconds().)
 *                Previously, we retried only after the file system was remounted, or waited
 *                until the next time the set was triggered.  Also, no longer retry fclose()
 *                100 times; now retry twice at most before giving up.
 *                Don't write sequence files (copy .sav to .sav<n>) for a list in failure.
 *                v5.3 failed to truncate the file when it open()'ed it (when compiled with
 *                SET_FILE_PERMISSIONS nonzero).  This caused the file to have extra characters
 *                if its useful length decreased and then increased, and restore thought the
 *                file was bad.
 * 03/10/08  tmm  v5.5 Merged in several improvements from Ben Franksen:
 *                Make use of status PV's optional and controllable by function call.
 *                Report failed connections to PV's at connect time.
 *                Allow embedded '.' in save-file name.
 */
#define		SRVERSION "autosave R5.3"

#include	<stdio.h>
#include	<errno.h>
#include	<stdlib.h>
#ifndef _WIN32
#include	<unistd.h>
#include	<dirent.h> /* for dirList */
#else
#include    <windows.h>
#include	"tr_dirent.h" /* for dirList */
#endif
#include	<string.h>
#include	<ctype.h>
#include	<sys/stat.h>
#include	<time.h>
#include	<sys/types.h> /* for dirList */

#include	<dbDefs.h>
#include	<cadef.h>		/* includes dbAddr.h */
#include	<epicsPrint.h>
#include	<epicsThread.h>
#include	<epicsExport.h>
#include	<iocsh.h>
/* not in 3.15.0.1 #include <tsDefs.h> */
#include    <macLib.h>
#include	<callback.h>
#include	<epicsMutex.h>
#include	<epicsEvent.h>
#include	<epicsTime.h>
#include	<epicsMessageQueue.h>
#include	"save_restore.h"
#include 	"fGetDateStr.h"
#include 	"osdNfs.h"              /* qiao: routine of os dependent code, for NFS */
#include	"configMenuClient.h"

#ifndef _WIN32
  #define SET_FILE_PERMISSIONS 1
#endif

#if SET_FILE_PERMISSIONS
#include "sys/types.h"
#include "fcntl.h"
/* common file_permissions = S_IRUSR S_IWUSR S_IRGRP S_IWGRP S_IROTH S_IWOTH */
mode_t file_permissions = (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
#endif

#define BACKWARDS_LIST 0	/* old list order was backwards */

/*** data structure definitions  ***/

/* save_methods - used to determine when a file should be deleted */
#define PERIODIC	0x01		/* set when timer goes off */
#define TRIGGERED	0x02		/* set when trigger channel goes off */
#define	TIMER		0x04
#define	CHANGE		0x08
#define MONITORED	(TIMER|CHANGE)	/* set when timer expires and channel changes */
#define MANUAL		0x10		/* set on request */
#define	SINGLE_EVENTS	(PERIODIC|TRIGGERED|MANUAL)

#define TIMEFMT "%a %b %d %H:%M:%S %Y\n"	/* e.g. 'Fri Sep 13 00:00:00 1986\n'	*/
#define TIMEFMT_noY "%a %b %d %H:%M:%S"		/* e.g. 'Fri Sep 13 00:00:00'			*/

struct chlist {								/* save set list element */
	struct chlist	*pnext;					/* next list */
	struct channel	*pchan_list;			/* channel list head */
	struct channel	*plast_chan;		/* channel list tail */
	char			reqFile[FN_LEN];		/* request file name */
	char			*macrostring; /* copy of the macrostring with which list was created */
	char			saveFile[NFS_PATH_LEN+1];	/* full save file name */
	char 			last_save_file[FN_LEN];	/* file name last used for save */
	char			save_file[FN_LEN];		/* file name to use on next save */
	int				save_method;			/* bit for each save method requested */
	int				enabled_method;			/* bit for each save method enabled */
	short			save_ok;				/* !=0 -> last save ok */
	int				save_state;				/* state of this save set 	*/
	int				period;					/* time (s) between saves (max for on change) */
	int				monitor_period;			/* time (s) between saves (max for on change) */
	char			trigger_channel[PV_NAME_LEN];	/* db channel to trigger save  */
	CALLBACK		periodicCb;
	CALLBACK		monitorCb;
	int				not_connected;			/* # bad channels not saved/connected */
	int				backup_sequence_num;	/* appended to backup files */
	epicsTimeStamp	backup_time;
	epicsTimeStamp	save_attempt_time;
	epicsTimeStamp	save_time;
	int				statusPvIndex;			/* identify this list's status reporting variables */
	char			name_PV[PV_NAME_LEN];	/* future: write save-set name to generic PV */
	chid			name_chid;
	long			status;
	char			status_PV[PV_NAME_LEN];
	chid			status_chid;
	char			save_state_PV[PV_NAME_LEN];
	chid			save_state_chid;
	char			statusStr[STRING_LEN];
	char			statusStr_PV[PV_NAME_LEN];
	chid			statusStr_chid;
	char			timeStr[STRING_LEN];
	char			time_PV[PV_NAME_LEN];
	chid			time_chid;
	char			savePathPV[PV_NAME_LEN], saveNamePV[PV_NAME_LEN];
	char			config[PV_NAME_LEN];
	chid			savePathPV_chid, saveNamePV_chid;
	int				do_backups;
	epicsTimeStamp	callback_time;		/* qiao: call back time of this list */
	epicsTimeStamp	reconnect_check_time;   /* qiao: for ca reconnection for the not-connected channels */
};

struct channel {					/* database channel list element */
	struct channel	*pnext;			/* next channel */
	char			name[64];		/* channel name */
	chid			chid;			/* channel access id */
	char			value[64];		/* value string */
	short			enum_val;		/* short value of an enumerated field */
	short			valid;			/* we think we got valid data for this channel */
	long			max_elements;	/* number of elements, initially from ca, but then from dbAddr */
	long			curr_elements;	/* number of elements from dbGet */
	long			field_type;		/* field type from dbAddr */
	void			*pArray;
	int			channel_connected;      /* qiao: show if the channel is successfully connected. 0 - failed; 1 - successfully */
	int			just_created;           /* qiao: 1 means the channel is just created, need to be handled */
};

struct pathListElement {
	struct pathListElement *pnext;
	char path[NFS_PATH_LEN+1];
};

/*** module global variables ***/

volatile int save_restoreDebug = 0;
epicsExportAddress(int, save_restoreDebug);

STATIC struct chlist *lptr = NULL;				/* save-set listhead */
STATIC int listLock = 0;						/* replaces long-term holding of sr_mutex */
#define NUM_STATUS_PV_SETS 8
STATIC int statusPvsInUse[NUM_STATUS_PV_SETS] = {0};
STATIC epicsMutexId	sr_mutex = NULL;			/* mut(ual) ex(clusion) for list of save sets */

/* Support for manual and programmed operations */

/* in configMenuClient.h
 * typedef void (*callbackFunc)(int status, void *puserPvt);
 */
#define OP_MSG_QUEUE_SIZE 10
#define OP_MSG_FILENAME_SIZE 100
#define OP_MSG_MACRO_SIZE 100
#define OP_MSG_TRIGGER_SIZE PV_NAME_LEN
typedef enum {
	op_RestoreFromSaveFile,
	op_RestoreFromAsciiFile,
	op_Remove,
	op_ReloadPeriodicSet,
	op_ReloadTriggeredSet,
	op_ReloadMonitorSet,
	op_ReloadManualSet,
	op_SaveFile
} op_type;
typedef struct op_msg {
	op_type operation;
	char filename[OP_MSG_FILENAME_SIZE];
	char requestfilename[OP_MSG_FILENAME_SIZE];
	char macrostring[OP_MSG_MACRO_SIZE];
	char trigger_channel[OP_MSG_TRIGGER_SIZE];
	int period;
	callbackFunc callbackFunction;
	void *puserPvt;
} op_msg;
#define OP_MSG_SIZE sizeof(op_msg)
STATIC epicsMessageQueueId opMsgQueue = NULL;	/* message queue for manual/programmed save/restore operations */

STATIC short	save_restore_init = 0;
STATIC char 	*SRversion = SRVERSION;
STATIC struct pathListElement
				*reqFilePathList = NULL;
char			saveRestoreFilePath[NFS_PATH_LEN] = "";	/* path to save files, also used by dbrestore.c */
STATIC unsigned int
				taskPriority = 20; /* epicsThreadPriorityCAServerLow -- initial task priority */

STATIC epicsThreadId
				taskID = 0;					/* save_restore task ID */

/* identifies the file type for a manual restore */
#define FROM_SAVE_FILE 1
#define FROM_ASCII_FILE 2

/*** stuff for reporting status to EPICS client ***/
STATIC char	status_prefix[30] = "";

STATIC long	SR_status = SR_STATUS_INIT;
STATIC unsigned short SR_heartbeat = 0;
/* Make SR_recentlyStr huge because sprintf may overrun (vxWorks has no snprintf) */
STATIC char	SR_statusStr[STRING_LEN] = "", SR_recentlyStr[300] = "";
STATIC char	SR_status_PV[PV_NAME_LEN] = "", SR_heartbeat_PV[PV_NAME_LEN] = "";
STATIC char	SR_statusStr_PV[PV_NAME_LEN] = "", SR_recentlyStr_PV[PV_NAME_LEN] = "";
STATIC chid	SR_status_chid, SR_heartbeat_chid,
			SR_statusStr_chid, SR_recentlyStr_chid;

STATIC long	SR_rebootStatus;
STATIC char	SR_rebootStatusStr[STRING_LEN] = "";
STATIC char	SR_rebootStatus_PV[PV_NAME_LEN] = "", SR_rebootStatusStr_PV[PV_NAME_LEN] = "";
STATIC chid	SR_rebootStatus_chid, SR_rebootStatusStr_chid;
STATIC char	SR_rebootTime_PV[PV_NAME_LEN] = "";
STATIC char	SR_rebootTimeStr[STRING_LEN] = "";
STATIC chid	SR_rebootTime_chid;

volatile int	save_restoreNumSeqFiles = 3;			/* number of sequence files to maintain */
volatile int	save_restoreSeqPeriodInSeconds = 60;	/* period between sequence-file writes */
volatile int	save_restoreIncompleteSetsOk = 1;		/* will save/restore incomplete sets? */
volatile int	save_restoreDatedBackupFiles = 1;		/* save backups as <filename>.bu or <filename>_YYMMDD-HHMMSS */
volatile int	save_restoreRetrySeconds = 60;			/* Time before retrying write after a failure. */
volatile int	save_restoreUseStatusPVs = 1;			/* use PVs for status etc. */
#define CA_RECONNECT_TIME_SECONDS 60
volatile int	save_restoreCAReconnect = 0;        /* qiao: if there are channels not connected, reconnect them */
volatile int	save_restoreCallbackTimeout = 600;  /* qiao: if the call back does not work than this time, force to save the data */

epicsExportAddress(int, save_restoreNumSeqFiles);
epicsExportAddress(int, save_restoreSeqPeriodInSeconds);
epicsExportAddress(int, save_restoreIncompleteSetsOk);
epicsExportAddress(int, save_restoreDatedBackupFiles);
epicsExportAddress(int, save_restoreUseStatusPVs);
epicsExportAddress(int, save_restoreCAReconnect);        /* qiao: export the new variables */
epicsExportAddress(int, save_restoreCallbackTimeout);    /* qiao: export the new variables */

/* variables for managing NFS mount */
#define REMOUNT_CHECK_INTERVAL_SECONDS 60
char save_restoreNFSHostName[NFS_PATH_LEN] = "";
char save_restoreNFSHostAddr[NFS_PATH_LEN] = "";
char save_restoreNFSMntPoint[NFS_PATH_LEN]  = "";
int saveRestoreFilePathIsMountPoint = 1;
volatile int save_restoreRemountThreshold=10;
epicsExportAddress(int, save_restoreRemountThreshold);

/* configuration parameters */
STATIC int	MIN_PERIOD	= 4;	/* save no more frequently than every 4 seconds */
STATIC int	MIN_DELAY	= 1;	/* check need to save every 1 second */
				/* worst case wait can be MIN_PERIOD + MIN_DELAY */

/*** private functions ***/
STATIC void periodic_save(CALLBACK *pcallback);
STATIC void triggered_save(struct event_handler_args);
STATIC void on_change_timer(CALLBACK *pcallback);
STATIC void on_change_save(struct event_handler_args);
STATIC int save_restore(void);
STATIC int connect_list(struct chlist *plist, int verbose);
STATIC int enable_list(struct chlist *plist);
STATIC int get_channel_values(struct chlist *plist);
STATIC int write_it(char *filename, struct chlist *plist);
STATIC int write_save_file(struct chlist *plist, char *configName);
STATIC void do_seq(struct chlist *plist);
STATIC int create_data_set(char *filename, int save_method, int period,
		char *trigger_channel, int mon_period, char *macrostring);
STATIC int do_manual_restore(char *filename, int file_type, char *macrostring);
STATIC int readReqFile(const char *file, struct chlist *plist, char *macrostring);
STATIC int do_remove_data_set(char *filename);
STATIC int request_manual_restore(char *filename, int file_type, char *macrostring, callbackFunc callbackFunction, void *puserPvt);

STATIC void ca_connection_callback(struct connection_handler_args args);      /* qiao: call back function for ca connection of the dataset channels */
STATIC void ca_disconnect();                                                  /* qiao: disconnect all existing CA channels */
STATIC void defaultCallback(int status, void *puserPvt);

/*** user-callable functions ***/

int fdbrestore(char *filename);
int set_savefile_name(char *filename, char *save_filename);
int create_periodic_set(char *filename, int period, char *macrostring);
int create_triggered_set(char *filename, char *trigger_channel, char *macrostring);
int create_monitor_set(char *filename, int period, char *macrostring);
int create_manual_set(char *filename, char *macrostring);
void save_restoreShow(int verbose);
int set_requestfile_path(char *path, char *pathsub);
int set_savefile_path(char *path, char *pathsub);
int set_saveTask_priority(int priority);
int remove_data_set(char *filename);
int reload_periodic_set(char *filename, int period, char *macrostring);
int reload_triggered_set(char *filename, char *trigger_channel, char *macrostring);
int reload_monitor_set(char * filename, int period, char *macrostring);
int reload_manual_set(char * filename, char *macrostring);

/* callable from a client */
int findConfigFiles(char *config, char names[][100], char descriptions[][100], int num, int len);

/* The following user-callable functions have an abridged argument list for iocsh use,
 * and a full argument list for calls from local client code.
 */
int fdbrestoreX(char *filename, char *macrostring, callbackFunc callbackFunction, void *puserPvt);
int manual_save(char *request_file, char *save_file, callbackFunc callbackFunction, void *puserPvt);
char *getMacroString(char *request_file);

/* functions to set save_restore parameters */
void save_restoreSet_Debug(int level) {save_restoreDebug = level;}
void save_restoreSet_NumSeqFiles(int numSeqFiles) {save_restoreNumSeqFiles = numSeqFiles;}
void save_restoreSet_SeqPeriodInSeconds(int period) {save_restoreSeqPeriodInSeconds = MAX(10, period);}
void save_restoreSet_IncompleteSetsOk(int ok) {save_restoreIncompleteSetsOk = ok;}
void save_restoreSet_DatedBackupFiles(int ok) {save_restoreDatedBackupFiles = ok;}
void save_restoreSet_status_prefix(char *prefix) {strncpy(status_prefix, prefix, 29);}
#if SET_FILE_PERMISSIONS
void save_restoreSet_FilePermissions(int permissions) {
	file_permissions = permissions;
	printf("save_restore: File permissions set to 0%o\n", (unsigned int)file_permissions);
}
#endif
void save_restoreSet_RetrySeconds(int seconds) {
	if (seconds >= 0) save_restoreRetrySeconds = seconds;
}
void save_restoreSet_UseStatusPVs(int ok) {save_restoreUseStatusPVs = ok;}
void save_restoreSet_CAReconnect(int ok) {save_restoreCAReconnect = ok;}
void save_restoreSet_CallbackTimeout(int t) {
	if ((t<0) || (t>=MIN_PERIOD)) {
		save_restoreCallbackTimeout = t;
	} else {
		printf("save_restoreCallbackTimeout must be either negative (forever) or >= %d seconds\n", MIN_PERIOD);
	}
}

/********************************* code *********************************/


/*** access to list *lptr ***/

STATIC int lockList() {
	int caller_owns_lock = 0;
	epicsMutexLock(sr_mutex);
	if (!listLock) listLock = caller_owns_lock = 1;
	epicsMutexUnlock(sr_mutex);
	if (save_restoreDebug >= 15) printf("lockList: listLock=%d\n", listLock);
	return(caller_owns_lock);
}

STATIC void unlockList() {
	epicsMutexLock(sr_mutex);
	listLock = 0;
	epicsMutexUnlock(sr_mutex);
	if (save_restoreDebug >= 15) printf("unlockList: listLock=%d\n", listLock);
}

STATIC int waitForListLock(double secondsToWait) {
	double secondsWaited = 0., waitIncrement = 1;
	while (lockList() == 0) {
		if (secondsWaited >= secondsToWait) return(0);
		epicsThreadSleep(waitIncrement);
		secondsWaited += waitIncrement;
	}
	return(1);
}

/*** callbacks ***/

/*
 * PROBLEM: these callback routines don't protect themselves against the
 * possibility that the list pointer they were given is out of date.  If
 * the list no longer exists, they will write to memory that save_restore
 * no longer owns.  This can only happen if console user has removed or
 * reloaded a save set.
 */

/* method PERIODIC - timer has elapsed */
STATIC void periodic_save(CALLBACK *pcallback)
{
	void *userArg;
	struct chlist *plist;

	callbackGetUser(userArg, pcallback);
	plist = (struct chlist *)userArg;
	if (plist) {
		plist->save_state |= PERIODIC;
	} else {
		logMsg("Periodic saving failure");
	}
}


/* method TRIGGERED - ca_monitor received for trigger PV */
STATIC void triggered_save(struct event_handler_args event)
{
	struct chlist *plist = (struct chlist *) event.usr;

	if (event.dbr) {
		if (plist) {
			plist->save_state |= TRIGGERED;
		} else {
			logMsg("Failed to activate triggered saving!");
		}
	}
}


/* method MONITORED - timer has elapsed */
STATIC void on_change_timer(CALLBACK *pcallback)
{
	void *userArg;
	struct chlist *plist;

	callbackGetUser(userArg, pcallback);
	plist = (struct chlist *)userArg;

	if (save_restoreDebug >= 10) logMsg("on_change_timer for %s (period is %d seconds)\n",
			plist->reqFile, plist->monitor_period);

	if (plist) {
		plist->save_state |= TIMER;
	} else {
		logMsg("Failed to activate saving with timer!");
	}
}


/* method MONITORED - ca_monitor received for a PV */
STATIC void on_change_save(struct event_handler_args event)
{
    struct chlist *plist;
	if (save_restoreDebug >= 10) {
		logMsg("on_change_save: event.usr=0x%lx\n", (unsigned long)event.usr);
	}
    plist = (struct chlist *) event.usr;

    if (plist) {
        plist->save_state |= CHANGE;
    } else {
        logMsg("Data changed! But failed to activate saving!");
    }
}


int findConfigFiles(char *config, char names[][100], char descriptions[][100], int num, int len) {
	int i, found;
	DIR *pdir=0;
	FILE *fd;
	struct dirent *pdirent=0;
	char thisname[FN_LEN], filename[FN_LEN], *pchar, fullpath[NFS_PATH_LEN];
	char buffer[BUF_SIZE], *bp, *bp1, config_underscore[FN_LEN];

	if (names == NULL) return(-1);
	strncpy(config_underscore, config, FN_LEN-2);
	strcat(config_underscore, "_");
	if (save_restoreDebug) printf("findConfigFiles: config='%s', config_underscore=%s\n",
		config, config_underscore);
	for (i=0; i<num; i++) {
		names[i][0] = '\0';
		if (descriptions) descriptions[i][0] = '\0';
	}

	pdir = opendir(saveRestoreFilePath);
	if (pdir) {
		if (save_restoreDebug) printf("findConfigFiles: opendir('%s') succeeded.\n", saveRestoreFilePath);
		for (i=0; i<num && (pdirent=readdir(pdir)); ) {
			if (save_restoreDebug>1) printf("findConfigFiles: checking '%s'.\n", pdirent->d_name);
			if (strncmp(config_underscore, pdirent->d_name, strlen(config_underscore)) == 0) {
				strncpy(filename, pdirent->d_name, FN_LEN-1);
				if (save_restoreDebug) printf("findConfigFiles: found '%s'\n", filename);
				strncpy(thisname, &(filename[strlen(config_underscore)]), FN_LEN-1);
				if (save_restoreDebug) printf("findConfigFiles: searching '%s' for .cfg\n", thisname);
				/* require that file end with ".cfg" */
				pchar = strstr(&thisname[strlen(thisname)-strlen(".cfg")], ".cfg");
				if (pchar) {
					*pchar = '\0';
					strncpy(names[i], thisname, len-1);
					if (save_restoreDebug) printf("findConfigFiles: found config file '%s'\n", names[i]);
					makeNfsPath(fullpath, saveRestoreFilePath, filename);
					if ((fd = fopen(fullpath, "r"))) {
						if (save_restoreDebug) printf("findConfigFiles: searching '%s' for description\n", fullpath);
						found = 0;
						while (!found && (bp=fgets(buffer, BUF_SIZE, fd))) {
							bp1 = strstr(bp, "Menu:currDesc");
							if (bp1 != 0) {
								found = 1;
								bp1 += strlen("Menu:currDesc")+1;
								strncpy(descriptions[i], bp1, len-1);
								if (( pchar = strchr(descriptions[i], '\n') )) *pchar = '\0';
								if (( pchar = strchr(descriptions[i], '\r') )) *pchar = '\0';
							}
						}
						if (fd) fclose(fd);
						i++;
					} else {
						if (save_restoreDebug) printf("findConfigFiles: can't open '%s'\n", filename);
					}
				}
			}
		}
		if (save_restoreDebug) {
			for (i=0; i<num; i++) {
				printf("findConfigFiles: name='%s'; desc='%s'\n", names[i], descriptions[i]);
			}
		}
		closedir(pdir);
		return(0);
	}
	if (save_restoreDebug) printf("findConfigFiles: opendir('%s') failed.\n", saveRestoreFilePath);

	return(-1);
}

int manual_save(char *request_file, char *save_file, callbackFunc callbackFunction, void *puserPvt)
{
	op_msg msg;

	if (save_restoreDebug) printf("manual_save: request_file='%s', save_file='%s', callbackFunction=%p, puserPvt=%p\n",
		request_file, save_file, callbackFunction, puserPvt);

	msg.operation = op_SaveFile;
	strncpy(msg.requestfilename, request_file, OP_MSG_FILENAME_SIZE);
	msg.filename[0] = '\0';
	if (save_file) strncpy(msg.filename, save_file, OP_MSG_FILENAME_SIZE);
	if (callbackFunction==NULL) {
		callbackFunction = defaultCallback;
		puserPvt = NULL;
	}
	msg.puserPvt = puserPvt;
	msg.callbackFunction = callbackFunction;
	epicsMessageQueueSend(opMsgQueue, (void *)&msg, OP_MSG_SIZE);
	return(0);
}


STATIC void ca_connection_callback(struct connection_handler_args args)
{
	struct channel *pchannel = ca_puser(args.chid);

	if (!pchannel) return;

	if (args.op == CA_OP_CONN_UP) {
		pchannel->channel_connected = 1;
	} else {
		pchannel->channel_connected = 0;
		ca_clear_channel(args.chid);
		pchannel->chid = NULL;
	}
}

/*** functions to manage NFS mount ***/
STATIC void do_mount() {
	if (save_restoreNFSHostName[0] && save_restoreNFSHostAddr[0] && save_restoreNFSMntPoint[0]) {
		if (mountFileSystem(save_restoreNFSHostName, save_restoreNFSHostAddr, save_restoreNFSMntPoint, save_restoreNFSMntPoint) == OK) {
			errlogPrintf("save_restore:mountFileSystem:successfully mounted '%s'\n", save_restoreNFSMntPoint);
			strncpy(SR_recentlyStr, "mountFileSystem succeeded", (STRING_LEN-1));
			save_restoreIoErrors = 0;
			save_restoreNFSOK = 1;
		}
		else {
			errlogPrintf("save_restore: Can't mount '%s'\n", save_restoreNFSMntPoint);
		}
	} else {
		save_restoreNFSOK = 1;
	}
}

/* Concatenate s1 and s2, making sure there is a directory separator between them,
 * and copy the result to dest.  Make local copies of s1 and s2 to defend against
 * calls in which one of them is specified also as dest, e.g. makeNfsPath(a,b,a).
 */
void makeNfsPath(char *dest, const char *s1, const char *s2) {
	char tmp1[NFS_PATH_LEN], tmp2[NFS_PATH_LEN];
	if (dest == NULL) return;
	tmp1[0] = '\0';
	if (s1 && *s1) strncpy(tmp1, s1, NFS_PATH_LEN-1);
	tmp2[0] = '\0';
	if (s2 && *s2) strncpy(tmp2, s2, NFS_PATH_LEN-1);

	if (*tmp1) strncpy(dest, tmp1, NFS_PATH_LEN-1);
	if (*tmp2 && (*tmp2 != '/') && (strlen(dest) !=0 ) && (dest[strlen(dest)-1] != '/'))
		strncat(dest,"/", MAX(NFS_PATH_LEN-1 - strlen(dest),0));

	if ((*tmp2 == '/') && (strlen(dest) !=0 ) && (dest[strlen(dest)-1] == '/')) {
		strncat(dest, &(tmp2[1]), MAX(NFS_PATH_LEN-1 - strlen(dest),0));
	} else {
		strncat(dest, tmp2, MAX(NFS_PATH_LEN-1 - strlen(dest),0));
	}
	if (save_restoreDebug > 2) {
		errlogPrintf("save_restore:makeNfsPath: dest='%s'\n", dest);
	}
}

int testMakeNfsPath() {
	char dest[NFS_PATH_LEN];

	dest[0] = '\0';
	makeNfsPath(dest,"","");
	printf("makeNfsPath(dest,\"\",\"\") yields '%s'\n", dest);

	dest[0] = '\0';
	makeNfsPath(dest,"abc","");
	printf("makeNfsPath(dest,\"abc\",\"\") yields '%s'\n", dest);

	dest[0] = '\0';
	makeNfsPath(dest,"","def");
	printf("makeNfsPath(dest,\"\",\"def\") yields '%s'\n", dest);

	dest[0] = '\0';
	makeNfsPath(dest,"","/def");
	printf("makeNfsPath(dest,\"\",\"/def\") yields '%s'\n", dest);

	dest[0] = '\0';
	makeNfsPath(dest,"abc/","def");
	printf("makeNfsPath(dest,\"abc/\",\"def\") yields '%s'\n", dest);

	dest[0] = '\0';
	makeNfsPath(dest,"abc/","/def");
	printf("makeNfsPath(dest,\"abc/\",\"/def\") yields '%s'\n", dest);
	return(0);
}

void save_restoreSet_NFSHost(char *hostname, char *address, char *mntpoint)
{
	/* If file system is mounted (save_restoreNFSOK) and we mounted it (save_restoreNFSMntPoint[0]),
	 * then dismount, presuming that caller wants us to remount from new information.  If we didn't
	 * mount it, presume that caller did, and that caller wants us to manage the mount point.
	 */
	if (save_restoreNFSOK && save_restoreNFSMntPoint[0]) dismountFileSystem(save_restoreNFSMntPoint);

	/* get the settings */
	strncpy(save_restoreNFSHostName, hostname, (NFS_PATH_LEN-1));
	strncpy(save_restoreNFSHostAddr, address, (NFS_PATH_LEN-1));
    if (mntpoint && mntpoint[0]) {
		saveRestoreFilePathIsMountPoint = 0;
		strncpy(save_restoreNFSMntPoint, mntpoint, (NFS_PATH_LEN-1));
		if (saveRestoreFilePath[0]) {
			/* If we already have a file path, make sure it begins with the mount point. */
			if (strstr(saveRestoreFilePath, save_restoreNFSMntPoint) != saveRestoreFilePath) {
				makeNfsPath(saveRestoreFilePath, save_restoreNFSMntPoint, saveRestoreFilePath);
			}
		}
	} else if (saveRestoreFilePath[0]) {
		strncpy(save_restoreNFSMntPoint, saveRestoreFilePath, (NFS_PATH_LEN-1));
		saveRestoreFilePathIsMountPoint = 1;
	}

	/* mount the file system */
	do_mount();
}


/*** save_restore task ***/

/*
 * Init, then loop forever to service save sets, and user requests.
 * This task does all of the channel-access stuff.
 */
STATIC int save_restore(void)
{
	struct chlist *plist = NULL;
	char *cp, nameString[FN_LEN];
	int i, do_seq_check, just_remounted, n, saveNeeded=0;
	epicsTimeStamp currTime, last_seq_check, remount_check_time, delayStart;
	char datetime[32];
	double timeDiff;
	int NFS_managed = save_restoreNFSHostName[0] && save_restoreNFSHostAddr[0] && save_restoreNFSMntPoint[0];
	op_msg msg;
	struct restoreFileListItem *pLI;

	if (save_restoreDebug > 1)
			errlogPrintf("save_restore:save_restore: entry; status_prefix='%s'\n", status_prefix);

	opMsgQueue = epicsMessageQueueCreate(OP_MSG_QUEUE_SIZE, OP_MSG_SIZE);
	epicsTimeGetCurrent(&currTime);
	last_seq_check = remount_check_time = currTime; /* struct copy */

	ca_context_create(ca_enable_preemptive_callback);

	if ((save_restoreNFSOK == 0) && NFS_managed) do_mount();

	/* Build names for save_restore general status PV's with status_prefix */
	if (save_restoreUseStatusPVs && *status_prefix && (*SR_status_PV == '\0')) {
		strcpy(SR_status_PV, status_prefix);
		strcat(SR_status_PV, "SR_status");
		strcpy(SR_heartbeat_PV, status_prefix);
		strcat(SR_heartbeat_PV, "SR_heartbeat");
		strcpy(SR_statusStr_PV, status_prefix);
		strcat(SR_statusStr_PV, "SR_statusStr");
		strcpy(SR_recentlyStr_PV, status_prefix);
		strcat(SR_recentlyStr_PV, "SR_recentlyStr");
		TATTLE(ca_search(SR_status_PV, &SR_status_chid), "save_restore: ca_search(%s) returned %s", SR_status_PV);
		TATTLE(ca_search(SR_heartbeat_PV, &SR_heartbeat_chid), "save_restore: ca_search(%s) returned %s", SR_heartbeat_PV);
		TATTLE(ca_search(SR_statusStr_PV, &SR_statusStr_chid), "save_restore: ca_search(%s) returned %s", SR_statusStr_PV);
		TATTLE(ca_search(SR_recentlyStr_PV, &SR_recentlyStr_chid), "save_restore: ca_search(%s) returned %s", SR_recentlyStr_PV);

		strcpy(SR_rebootStatus_PV, status_prefix);
		strcat(SR_rebootStatus_PV, "SR_rebootStatus");
		strcpy(SR_rebootStatusStr_PV, status_prefix);
		strcat(SR_rebootStatusStr_PV, "SR_rebootStatusStr");
		strcpy(SR_rebootTime_PV, status_prefix);
		strcat(SR_rebootTime_PV, "SR_rebootTime");
		TATTLE(ca_search(SR_rebootStatus_PV, &SR_rebootStatus_chid), "save_restore: ca_search(%s) returned %s", SR_rebootStatus_PV);
		TATTLE(ca_search(SR_rebootStatusStr_PV, &SR_rebootStatusStr_chid), "save_restore: ca_search(%s) returned %s", SR_rebootStatusStr_PV);
		TATTLE(ca_search(SR_rebootTime_PV, &SR_rebootTime_chid), "save_restore: ca_search(%s) returned %s", SR_rebootTime_PV);
		if (ca_pend_io(0.5)!=ECA_NORMAL) {
			errlogPrintf("save_restore: Can't connect to all status PV(s)\n");
		}
		/* Show reboot status */
		SR_rebootStatus = SR_STATUS_OK;
		strcpy(SR_rebootStatusStr, "Ok");
		maybeInitRestoreFileLists();

		for (i=0; i<2; i++) {
			if (i==0) {
				pLI = (struct restoreFileListItem *) ellFirst(&pass0List);
			} else {
				pLI = (struct restoreFileListItem *) ellFirst(&pass1List);
			}
			while (pLI) {
				if (pLI->restoreStatus < SR_rebootStatus) {
					SR_rebootStatus = pLI->restoreStatus;
					strncpy(SR_rebootStatusStr, pLI->restoreStatusStr, (STRING_LEN-1));
				}
				pLI = (struct restoreFileListItem *) ellNext(&(pLI->node));
			}
		}

		TRY_TO_PUT(DBR_LONG, SR_rebootStatus_chid, &SR_rebootStatus);
		TRY_TO_PUT(DBR_STRING, SR_rebootStatusStr_chid, &SR_rebootStatusStr);
		epicsTimeGetCurrent(&currTime);
		epicsTimeToStrftime(SR_rebootTimeStr, sizeof(SR_rebootTimeStr),
			TIMEFMT_noY, &currTime);
		TRY_TO_PUT(DBR_STRING, SR_rebootTime_chid, &SR_rebootTimeStr);
	}

	while(1) {

		SR_status = SR_STATUS_OK;
		strcpy(SR_statusStr, "Ok");
		save_restoreSeqPeriodInSeconds = MAX(10, save_restoreSeqPeriodInSeconds);
		save_restoreNumSeqFiles = MIN(10, MAX(0, save_restoreNumSeqFiles));
		epicsTimeGetCurrent(&currTime);
		do_seq_check = (epicsTimeDiffInSeconds(&currTime, &last_seq_check) >
			save_restoreSeqPeriodInSeconds/2);
		if (do_seq_check) last_seq_check = currTime; /* struct copy */

		just_remounted = 0;

		/* remount NFS if necessary. If the file written failure happens more times than defined threshold,
		 * we will assume the NFS need to be remounted */
		if ((save_restoreNFSOK == 0)  && NFS_managed) {
			/* NFS problem, and we're managing the mount: Try every 60 seconds to remount. */
			timeDiff = epicsTimeDiffInSeconds(&currTime, &remount_check_time);
			errlogPrintf("save_restore: save_restoreNFSOK==0 for %f seconds\n", timeDiff);
			if (timeDiff > REMOUNT_CHECK_INTERVAL_SECONDS) {
				remount_check_time = currTime;                           	/* struct copy */
				errlogPrintf("save_restore: attempting to remount filesystem\n");
				dismountFileSystem(save_restoreNFSMntPoint);          /* first dismount it */
				/* We don't care if dismountFileSystem fails.
				 * It could fail simply because an earlier dismount, succeeded.
				 */
				if (mountFileSystem(save_restoreNFSHostName, save_restoreNFSHostAddr,
							save_restoreNFSMntPoint, save_restoreNFSMntPoint) == OK) {
					just_remounted = 1;
					errlogPrintf("save_restore: remounted '%s'\n", save_restoreNFSMntPoint);
					SR_status = SR_STATUS_OK;
					strcpy(SR_statusStr, "NFS remounted");
				} else {
					errlogPrintf("save_restore: failed to remount '%s' \n", save_restoreNFSMntPoint);
					SR_status = SR_STATUS_FAIL;
					strcpy(SR_statusStr, "NFS failed!");
				}
			}
		}

		/* look at each list */
		while (waitForListLock(5) == 0) {
			if (save_restoreDebug > 1) errlogPrintf("save_restore: '%s' waiting for listLock()\n", plist->reqFile);
		}
		plist = lptr;
		while (plist != 0) {
			if (save_restoreDebug >= 30)
				errlogPrintf("save_restore: '%s' save_state = 0x%x\n", plist->reqFile, plist->save_state);

			/* connect the channels on the first instance of this set */
			if (plist->enabled_method == 0) {
				/* qiao: first, Connect to savePathPV and saveNamePV, if they are defined (this is moved from the connect_list() routine */
				if (plist->savePathPV[0] || plist->saveNamePV[0]) {
					if (plist->savePathPV[0]) {
						TATTLE(ca_search(plist->savePathPV,&plist->savePathPV_chid), "save_restore: ca_search(%s) returned %s", plist->savePathPV);
					}
					if (plist->saveNamePV[0]) {
						TATTLE(ca_search(plist->saveNamePV,&plist->saveNamePV_chid), "save_restore: ca_search(%s) returned %s", plist->saveNamePV);
					}
					if (ca_pend_io(0.5)!=ECA_NORMAL) {
						errlogPrintf("save_restore: Can't connect to list-specific path/name PV(s)\n");

						plist->status = SR_STATUS_WARN;
						strcpy(plist->statusStr, "List path/name PVs connection failed");
					}
				}

				/* qiao: second, connect the list */
				plist->not_connected = connect_list(plist, 1);
				plist->reconnect_check_time = currTime;

			} else if (save_restoreCAReconnect &&
				plist->not_connected > 0 &&
				epicsTimeDiffInSeconds(&currTime, &plist->reconnect_check_time) > CA_RECONNECT_TIME_SECONDS) {
				/* Try to connect to disconnected channels every CA_RECONNECT_TIME_SECONDS */
				plist->reconnect_check_time = currTime;
				plist->not_connected = connect_list(plist, 0);
			}

			/*
			 * We used to call enable_list() from create_data_set(), if the
			 * list already existed and was just getting a new method.  In that
			 * case, we'd only enable new lists (those with enabled_method==0) here.
			 * Now, we want to avoid having the task that called create_data_set()
			 * setting up CA monitors that we're going to have to manage, so we
			 * make all the calls to enable_list().
			 */
			if (plist->enabled_method != plist->save_method) enable_list(plist);

			/* qiao: check the call back timeout if the save method is periodic or monitored */
			if ((plist->save_method & PERIODIC) || (plist->save_method & MONITORED) == MONITORED) {
				if ((save_restoreCallbackTimeout > MIN_PERIOD) &&
					(epicsTimeDiffInSeconds(&currTime, &plist->callback_time) > save_restoreCallbackTimeout)) {
			    		plist->save_state = plist->save_method;

					if (save_restoreDebug > 1)
			    			errlogPrintf("save_restore: Callback time out of %s, force to save!\n", plist->reqFile);
				}
			}

			/*
			 * Save lists that have triggered.  Save lists that are in failure, if we've just remounted,
			 * or if RETRY_SECS have elapsed since the last save attempt.
			 */
			saveNeeded = FALSE;
			if (plist->save_state & SINGLE_EVENTS)
				saveNeeded = TRUE;
			else if ((plist->save_state & MONITORED) == MONITORED)
				saveNeeded = TRUE;
			else if (plist->status <= SR_STATUS_FAIL) {
				if (just_remounted)
					saveNeeded = TRUE;
				else if (epicsTimeDiffInSeconds(&currTime, &plist->save_attempt_time) > save_restoreRetrySeconds)
					saveNeeded = TRUE;
			}

			if (saveNeeded) {

				/* fetch values all of the channels */
				plist->not_connected = get_channel_values(plist);

				/* write the data to disk */
				if ((plist->not_connected == 0) || (save_restoreIncompleteSetsOk))
					write_save_file(plist, NULL);
			}

			/*** Periodically make sequenced backup of most recent saved file ***/
			if (do_seq_check && plist->do_backups && (plist->status > SR_STATUS_FAIL)) {
				if (save_restoreNumSeqFiles && plist->last_save_file &&
					(epicsTimeDiffInSeconds(&currTime, &plist->backup_time) >
						save_restoreSeqPeriodInSeconds)) {
					do_seq(plist);
				}
			}

			/*** restart timers and reset save requests ***/
			if (plist->save_state & PERIODIC) {
				callbackRequestDelayed(&plist->periodicCb, (double)plist->period);
				plist->callback_time = currTime;                                /* qiao: rememter the time starting callback */
				fGetDateStr(datetime);
			}
			if (plist->save_state & SINGLE_EVENTS) {
				/* Note that this clears PERIODIC, TRIGGERED, and MANUAL bits */
				plist->save_state = plist->save_state & ~SINGLE_EVENTS;
			}
			if ((plist->save_state & MONITORED) == MONITORED) {
				callbackRequestDelayed(&plist->monitorCb, (double)plist->monitor_period);
				plist->save_state = plist->save_state & ~MONITORED;
				plist->callback_time = currTime;                                /* qiao: rememter the time starting callback */
				fGetDateStr(datetime);
			}

			/* find and record worst status */
			if (plist->status <= SR_status ) {
				SR_status = plist->status;
				strcpy(SR_statusStr, plist->statusStr);
			}
			/*if (SR_rebootStatus < SR_status) {
				SR_status = SR_rebootStatus;
				strcpy(SR_statusStr, SR_rebootStatusStr);
			}*/           /* qiao: disable this part, because sometimes the system recovers during runtime though some errors during reboot */

			/* next list */
			plist = plist->pnext;
		}

		/* release the list */
		unlockList();

		/* report status */
		SR_heartbeat = (SR_heartbeat+1) % 2;
		TRY_TO_PUT(DBR_LONG, SR_status_chid, &SR_status);
		TRY_TO_PUT(DBR_SHORT, SR_heartbeat_chid, &SR_heartbeat);
		TRY_TO_PUT(DBR_STRING, SR_statusStr_chid, &SR_statusStr);
		SR_recentlyStr[(STRING_LEN-1)] = '\0';
		TRY_TO_PUT(DBR_STRING, SR_recentlyStr_chid, &SR_recentlyStr);

		if (save_restoreUseStatusPVs) {
			/*** set up list-specific status PV's for any new lists ***/
			while (waitForListLock(5) == 0) {
				if (save_restoreDebug > 1) errlogPrintf("save_restore: waiting for listLock()\n");
			}
			for (plist = lptr; plist; plist = plist->pnext) {
				/*
				 * If this is the first time for a list, and user has defined a status prefix,
				 * connect to the list's status PV's
				 */
				if (*status_prefix && (plist->status_PV[0] == '\0') && (plist->statusPvIndex < NUM_STATUS_PV_SETS)) {
					/*** Build PV names ***/
					/* make common portion of PVname strings */
					n = (PV_NAME_LEN-1) - sprintf(plist->status_PV, "%sSR_%1d_", status_prefix, plist->statusPvIndex);
					strcpy(plist->name_PV, plist->status_PV);
					strcpy(plist->save_state_PV, plist->status_PV);
					strcpy(plist->statusStr_PV, plist->status_PV);
					strcpy(plist->time_PV, plist->status_PV);
					/* make all PVname strings */
					strncat(plist->status_PV, "Status", n);
					strncat(plist->name_PV, "Name", n);
					strncat(plist->save_state_PV, "State", n);
					strncat(plist->statusStr_PV, "StatusStr", n);
					strncat(plist->time_PV, "Time", n);
					/* connect with PV's */
					TATTLE(ca_search(plist->status_PV, &plist->status_chid), "save_restore: ca_search(%s) returned %s", plist->status_PV);
					TATTLE(ca_search(plist->name_PV, &plist->name_chid), "save_restore: ca_search(%s) returned %s", plist->name_PV);
					TATTLE(ca_search(plist->save_state_PV, &plist->save_state_chid), "save_restore: ca_search(%s) returned %s", plist->save_state_PV);
					TATTLE(ca_search(plist->statusStr_PV, &plist->statusStr_chid), "save_restore: ca_search(%s) returned %s", plist->statusStr_PV);
					TATTLE(ca_search(plist->time_PV, &plist->time_chid), "save_restore: ca_search(%s) returned %s", plist->time_PV);
					if (ca_pend_io(0.5)!=ECA_NORMAL) {
						errlogPrintf("save_restore: Can't connect to status PV(s) for list '%s'\n", plist->save_file);
					}
				}

				if (plist->statusPvIndex < NUM_STATUS_PV_SETS) {
					TRY_TO_PUT(DBR_LONG, plist->status_chid, &plist->status);
					if (CONNECTED(plist->name_chid)) {
						strncpy(nameString, plist->save_file, STRING_LEN-1);
						cp = strrchr(nameString, (int)'.');
						if (cp) *cp = 0;
						ca_put(DBR_STRING, plist->name_chid, &nameString);
					}
					TRY_TO_PUT(DBR_LONG, plist->save_state_chid, &plist->save_state);
					TRY_TO_PUT(DBR_STRING, plist->statusStr_chid, &plist->statusStr);
					if ((plist->status >= SR_STATUS_WARN) && (plist->save_time.secPastEpoch != 0)) {
						epicsTimeToStrftime(plist->timeStr, sizeof(plist->timeStr),
							TIMEFMT_noY, &plist->save_time);
						TRY_TO_PUT(DBR_STRING, plist->time_chid, &plist->timeStr);
					}
				}
			}
			unlockList();
		}

		/*** service client commands and/or sleep for MIN_DELAY ***/

		epicsTimeGetCurrent(&delayStart);
		while (epicsMessageQueueReceiveWithTimeout(opMsgQueue, (void*) &msg, OP_MSG_SIZE, (double)MIN_DELAY) >= 0) {
			int status=0;
			int num_errs;
			char fullPath[NFS_PATH_LEN+1] = "";

			switch (msg.operation) {

			case op_RestoreFromSaveFile:
				if (save_restoreDebug) printf("save_restore task: calling do_manual_restore('%s')\n", msg.filename);
				status = do_manual_restore(msg.filename, FROM_SAVE_FILE, NULL);
				if (save_restoreDebug>1) printf("save_restore: manual restore status=%d (0==success)\n", status);
				sprintf(SR_recentlyStr, "Restore of '%s' %s", msg.filename, status?"Failed":"Succeeded");
				break;

			case op_RestoreFromAsciiFile:
				if (save_restoreDebug) printf("save_restore task: calling do_manual_restore('%s')\n", msg.filename);
				status = do_manual_restore(msg.filename, FROM_ASCII_FILE, msg.macrostring);
				if (save_restoreDebug>1) printf("save_restore: manual restore status=%d (0==success)\n", status);
				sprintf(SR_recentlyStr, "Restore of '%s' %s", msg.filename, status?"Failed":"Succeeded");
				if (status == 0) {
					makeNfsPath(fullPath, saveRestoreFilePath, msg.filename);
					status = asVerify(fullPath, -1, save_restoreDebug, 0, "");
				}
				if (msg.callbackFunction) (msg.callbackFunction)(status, msg.puserPvt);
				break;

			case op_Remove:
				if (save_restoreDebug) printf("save_restore task: calling do_remove_data_set('%s')\n", msg.filename);
				status = do_remove_data_set(msg.filename);
				if (save_restoreDebug>1) printf("save_restore: remove status=%d (0==success)\n", status);
				sprintf(SR_recentlyStr, "Remove '%s' %s", msg.filename, status?"Failed":"Succeeded");
				break;

			case op_ReloadPeriodicSet:
			case op_ReloadTriggeredSet:
			case op_ReloadMonitorSet:
			case op_ReloadManualSet:
				if (save_restoreDebug) printf("save_restore task: calling do_remove_data_set('%s')\n", msg.filename);
				status = do_remove_data_set(msg.filename);
				if (save_restoreDebug>1) printf("save_restore: remove status=%d (0==success)\n", status);
				if (status == 0) {
				switch (msg.operation) {
					case op_ReloadPeriodicSet:
						status = create_periodic_set(msg.filename, msg.period, msg.macrostring);
						break;
					case op_ReloadTriggeredSet:
						status = create_triggered_set(msg.filename, msg.trigger_channel, msg.macrostring);
						break;
					case op_ReloadMonitorSet:
						status = create_monitor_set(msg.filename, msg.period, msg.macrostring);
						break;
					case op_ReloadManualSet:
						status = create_manual_set(msg.filename, msg.macrostring);
						break;
					/* These can't occur, but are included anyway just to shut the compiler up. */
					case op_RestoreFromSaveFile: case op_RestoreFromAsciiFile: case op_Remove:
					default:
						break;
					}
				}
				sprintf(SR_recentlyStr, "Reload '%s' %s", msg.filename, status?"Failed":"Succeeded");
				break;


			case op_SaveFile:
				if (save_restoreDebug) printf("save_restore task: manual save('%s')\n", msg.filename);
				num_errs = 0;
				while (waitForListLock(5) == 0) {
					if (save_restoreDebug > 1) errlogPrintf("save_restore: waiting for listLock()\n");
				}
				status = -1;
				plist = lptr;
				while (plist != 0) {
					if (strcmp(plist->reqFile, msg.requestfilename) == 0) break;
					plist = plist->pnext;
				}
				if (plist) {
					/* fetch values all of the channels */
					plist->not_connected = get_channel_values(plist);
					num_errs += plist->not_connected;

					/* write the data to disk */
					if ((plist->not_connected == 0) || (save_restoreIncompleteSetsOk))
						status = write_save_file(plist, msg.filename);
					if (save_restoreDebug>1) printf("save_restore: op_SaveFile: write_save_file() returned %d\n", status);
				}
				unlockList();
				if (status == 0) {
					makeNfsPath(fullPath, saveRestoreFilePath, msg.filename);
					status = asVerify(fullPath, -1, save_restoreDebug, 0, "");
				}

				if (save_restoreDebug>1) printf("save_restore: manual save status=%d (0==success)\n", status);
				sprintf(SR_recentlyStr, "Save of '%s' %s", msg.filename, status?"Failed":"Succeeded");
				if (!status && num_errs) status = num_errs;
				if (msg.callbackFunction) (msg.callbackFunction)(status, msg.puserPvt);
				break;


			default:
				break;
			}
			ca_pend_event(0.001);
		}

		/* Make sure MIN_DELAY has elapsed before we make next pass through the list */
		epicsTimeGetCurrent(&currTime);
		timeDiff = epicsTimeDiffInSeconds(&currTime, &delayStart);
		if (timeDiff < MIN_DELAY) ca_pend_event(timeDiff - MIN_DELAY);
    }
	/* before exit, clear all CA channels */
	ca_disconnect();

	/* We're never going to exit */
	return(OK);
}


/*
 * connect all of the channels in a save set
 *
 * NOTE: Assumes sr_mutex is locked
 */
STATIC int connect_list(struct chlist *plist, int verbose)
{
	struct channel	*pchannel;
	int				n, m;
	long			status, field_size;

	strcpy(plist->statusStr,"Connecting PVs...");

	/* connect all channels in the list */
	for (pchannel = plist->pchan_list, n=0; pchannel != 0; pchannel = pchannel->pnext) {
		if (save_restoreDebug >= 10)
			errlogPrintf("save_restore:connect_list: channel '%s'\n", pchannel->name);

		if (!(pchannel->channel_connected)) {
			/* printf("The chid of %s is %p\n", pchannel->name, pchannel->chid); */
			if (pchannel->chid) ca_clear_channel(pchannel->chid);         /* qiao: release the channel, avoid duplicate resource allocation */
			if (ca_create_channel(pchannel->name, ca_connection_callback, (void *)pchannel,
					CA_PRIORITY_DEFAULT, &pchannel->chid) == ECA_NORMAL) {
				strcpy(pchannel->value,"Search Issued");
				pchannel->just_created = 1;
				n++;
			} else {
				strcpy(pchannel->value,"Search Failed");
			}
		}
	}
	if (ca_pend_io(MAX(5.0, n * 0.01)) == ECA_TIMEOUT) {
		errlogPrintf("save_restore:connect_list: not all searches successful\n");
	}

	for (pchannel = plist->pchan_list, n=m=0; pchannel != 0; pchannel = pchannel->pnext) {
		if (!(pchannel->just_created))
			continue;

		/* check newly created channels */
		pchannel->just_created = 0;
		m++;	/* number of newly created channels */

		if (pchannel->chid) {
			if (ca_state(pchannel->chid) == cs_conn) {
				strcpy(pchannel->value,"Connected");
				n++;
			} else {
				if (verbose) {
					errlogPrintf("save_restore: connect failed for channel '%s'\n", pchannel->name);
				}
			}
 		}

		pchannel->max_elements = ca_element_count(pchannel->chid);	/* just to see if it's an array */
		pchannel->curr_elements = pchannel->max_elements;				/* begin with this assumption */
		if (save_restoreDebug >= 10)
			errlogPrintf("save_restore:connect_list: '%s' has, at most, %ld elements\n",
				pchannel->name, pchannel->max_elements);
		if (pchannel->max_elements > 1) {
			/* We use database access for arrays, so get that info */
			status = SR_get_array_info(pchannel->name, &pchannel->max_elements, &field_size, &pchannel->field_type);
			if (status) {
				pchannel->curr_elements = pchannel->max_elements = -1;
				errlogPrintf("save_restore:connect_list: array PV '%s' is not local.\n", pchannel->name);
			} else {
				/* info resulting from dbNameToAddr() might be different, but it's still not the actual element count */
				pchannel->curr_elements = pchannel->max_elements;
				if (save_restoreDebug >= 10)
					errlogPrintf("save_restore:connect_list:(after SR_get_array_info) '%s' has, at most, %ld elements\n",
						pchannel->name, pchannel->max_elements);
				pchannel->pArray = calloc(pchannel->max_elements, field_size);
				if (pchannel->pArray == NULL) {
					errlogPrintf("save_restore:connect_list: can't alloc array for '%s'\n", pchannel->name);
					pchannel->curr_elements = pchannel->max_elements = -1;
				}
			}
		}
	}
	sprintf(SR_recentlyStr, "%s: %d of %d PV's connected", plist->save_file, n, m);
	if (verbose) {
		errlogPrintf(SR_recentlyStr);
		errlogPrintf("\n");
	}

	return(get_channel_values(plist));
}

/**
 * qiao: disconnect all CA channels used in this module. This routine is called when terminating the main thread
 */
STATIC void ca_disconnect()
{
	struct chlist  *plist    = NULL;
	struct channel *pchannel = NULL;

	/* disconnect all channels in the data set */
	plist = lptr;

	while (plist != 0) {
		/* disconnect all channels in the data list */
		for (pchannel = plist -> pchan_list; pchannel != 0; pchannel = pchannel -> pnext)
			if (pchannel->chid) ca_clear_channel(pchannel->chid);

		/* disconnect the data list specific PVs */
		if(plist->savePathPV_chid) ca_clear_channel(plist->savePathPV_chid);
		if(plist->saveNamePV_chid) ca_clear_channel(plist->saveNamePV_chid);

		if(plist->status_chid) 		ca_clear_channel(plist->status_chid);
		if(plist->name_chid) 		ca_clear_channel(plist->name_chid);
		if(plist->save_state_chid) 	ca_clear_channel(plist->save_state_chid);
		if(plist->statusStr_chid) 	ca_clear_channel(plist->statusStr_chid);
		if(plist->time_chid) 		ca_clear_channel(plist->time_chid);

		/* next list */
		plist = plist->pnext;
	}

	/* disconnect the global level channels */
	if(SR_heartbeat_chid) 		ca_clear_channel(SR_heartbeat_chid);
	if(SR_recentlyStr_chid) 	ca_clear_channel(SR_recentlyStr_chid);
	if(SR_status_chid) 		ca_clear_channel(SR_status_chid);
	if(SR_statusStr_chid) 		ca_clear_channel(SR_statusStr_chid);
	if(SR_rebootStatus_chid) 	ca_clear_channel(SR_rebootStatus_chid);
	if(SR_rebootStatusStr_chid) 	ca_clear_channel(SR_rebootStatusStr_chid);
	if(SR_rebootTime_chid) 		ca_clear_channel(SR_rebootTime_chid);
}


/*
 * enable new save methods
 *
 * NOTE: Assumes sr_mutex is locked
 */
STATIC int enable_list(struct chlist *plist)
{
	struct channel	*pchannel;
	chid 			chid;			/* channel access id */

	if (save_restoreDebug >= 4) errlogPrintf("save_restore:enable_list: entry\n");
	strcpy(plist->statusStr,"Enabling list...");

	/* enable a periodic set */
	if ((plist->save_method & PERIODIC) && !(plist->enabled_method & PERIODIC)) {
		callbackRequestDelayed(&plist->periodicCb, (double)plist->period);
		plist->enabled_method |= PERIODIC;
		epicsTimeGetCurrent(&plist->callback_time);
	}

	/* enable a triggered set */
	if ((plist->save_method & TRIGGERED) && !(plist->enabled_method & TRIGGERED)) {
		if (ca_search(plist->trigger_channel, &chid) != ECA_NORMAL) {
			errlogPrintf("save_restore:enable_list: trigger %s search failed\n", plist->trigger_channel);
		} else if (ca_pend_io(2.0) != ECA_NORMAL) {
			errlogPrintf("save_restore:enable_list: timeout on search of %s\n", plist->trigger_channel);
		} else if (chid == NULL) {
			errlogPrintf("save_restore:enable_list: no CHID for trigger channel '%s'\n", plist->trigger_channel);
		} else if (ca_state(chid) != cs_conn) {
			errlogPrintf("save_restore:enable_list: trigger %s search not connected\n", plist->trigger_channel);
		} else if (ca_add_event(DBR_FLOAT, chid, triggered_save, (void *)plist, 0) !=ECA_NORMAL) {
			errlogPrintf("save_restore:enable_list: trigger event for %s failed\n", plist->trigger_channel);
		} else{
			plist->enabled_method |= TRIGGERED;
		}
	}

	/* enable a monitored set */
	if ((plist->save_method & MONITORED) && !(plist->enabled_method & MONITORED)) {
		for (pchannel = plist->pchan_list; pchannel != 0; pchannel = pchannel->pnext) {
			if (save_restoreDebug >= 10) {
				errlogPrintf("save_restore:enable_list: calling ca_add_event for '%s'\n", pchannel->name);
				errlogPrintf("save_restore:enable_list: arg = %p\n", (void *)plist);
			}
			/*
			 * Work around obscure problem affecting USHORTS by making DBR type different
			 * from any possible field type.  This avoids tickling a bug that causes dbGet
			 * to overwrite the source field with its own value converted to LONG.
			 * (Changed DBR_LONG to DBR_TIME_LONG.)
			 */
			if (ca_add_event(DBR_TIME_LONG, pchannel->chid, on_change_save,
					(void *)plist, 0) != ECA_NORMAL) {
				errlogPrintf("save_restore:enable_list: could not add event for %s in %s\n",
					pchannel->name,plist->reqFile);
			}
		}
		if (save_restoreDebug >= 4) errlogPrintf("save_restore:enable_list: done calling ca_add_event for list channels\n");
		if (ca_pend_io(5.0) != ECA_NORMAL) {
			errlogPrintf("save_restore:enable_list: timeout on monitored set: %s to monitored scan\n",plist->reqFile);
		}
		callbackRequestDelayed(&plist->monitorCb, (double)plist->monitor_period);
		plist->enabled_method |= MONITORED;
		epicsTimeGetCurrent(&plist -> callback_time);
	}

	/* enable a manual request set */
	if ((plist->save_method & MANUAL) && !(plist->enabled_method & MANUAL)) {
		plist->enabled_method |= MANUAL;
	}

	sprintf(SR_recentlyStr, "list '%s' enabled", plist->save_file);
	return(OK);
}


/*
 * fetch values for all channels in the save set
 *
 * NOTE: Assumes sr_mutex is locked
 */
#define INIT_STRING "!@#$%^&*()"
STATIC int get_channel_values(struct chlist *plist)
{

	struct channel *pchannel;
	int				not_connected = 0;
	unsigned short	num_channels = 0;
	short			field_type;
	long			status, field_size;
	float			*pf;
	double			*pd;

	/* attempt to fetch all channels that are connected */
	for (pchannel = plist->pchan_list; pchannel != 0; pchannel = pchannel->pnext) {
		pchannel->valid = 0;

		/* Handle channels whose element count has not yet been determined. */
		if (pchannel->chid && (ca_state(pchannel->chid) == cs_conn) && (pchannel->max_elements == 0)) {
			/* Channel probably wasn't connected when connect_list() was called */
			pchannel->max_elements = pchannel->curr_elements = ca_element_count(pchannel->chid);
			if (pchannel->max_elements > 1) {
				status = SR_get_array_info(pchannel->name, &pchannel->max_elements, &field_size, &pchannel->field_type);
				if (status) {
					pchannel->curr_elements = pchannel->max_elements = -1; /* Mark channel so we ignore it forever. */
					errlogPrintf("save_restore:get_channel_values: array PV '%s' is not local.\n", pchannel->name);
				} else {
					/* info resulting from dbNameToAddr() might be different, but it's still not the actual element count */
					pchannel->curr_elements = pchannel->max_elements;
					if (save_restoreDebug >= 10)
						errlogPrintf("save_restore:get_channel_values:(after SR_get_array_info) '%s' has, at most, %ld elements\n",
							pchannel->name, pchannel->max_elements);
					pchannel->pArray = calloc(pchannel->max_elements, field_size);
					if (pchannel->pArray == NULL) {
						errlogPrintf("save_restore:get_channel_values: can't alloc array for '%s'\n", pchannel->name);
						pchannel->curr_elements = pchannel->max_elements = -1; /* Mark channel so we ignore it forever. */
					}
				}
			}
		}

		if (pchannel->chid && (ca_state(pchannel->chid) == cs_conn) && (pchannel->max_elements >= 1)) {
			field_type = ca_field_type(pchannel->chid);
			strcpy(pchannel->value, INIT_STRING);
			if (field_type == DBF_FLOAT) {
				ca_array_get(DBR_FLOAT,1,pchannel->chid,(float *)pchannel->value);
			} else if (field_type == DBF_DOUBLE) {
				ca_array_get(DBR_DOUBLE,1,pchannel->chid,(double *)pchannel->value);
			} else {
				ca_array_get(DBR_STRING,1,pchannel->chid,pchannel->value);
			}
			if (field_type == DBF_ENUM) {
				ca_array_get(DBR_SHORT,1,pchannel->chid,&pchannel->enum_val);
				num_channels++;
			}
			num_channels++;
			pchannel->valid = 1;
			if (pchannel->max_elements > 1) {
				pchannel->curr_elements = pchannel->max_elements;
				(void)SR_get_array(pchannel->name, pchannel->pArray, &pchannel->curr_elements);
			}
			if (save_restoreDebug >= 15) {
				errlogPrintf("save_restore:get_channel_values: '%s' currently has %ld elements\n",
					pchannel->name, pchannel->curr_elements);
			}
		} else {
			not_connected++;
			if (pchannel->chid == NULL) {
				if (save_restoreDebug > 1) errlogPrintf("save_restore:get_channel_values: no CHID for '%s'\n", pchannel->name);
			} else if (ca_state(pchannel->chid) != cs_conn) {
				if (save_restoreDebug > 1) errlogPrintf("save_restore:get_channel_values: %s not connected\n", pchannel->name);
			} else if (pchannel->max_elements == 0) {
				if (save_restoreDebug > 1) errlogPrintf("save_restore:get_channel_values: %s has an undetermined # elements\n",
					pchannel->name);
			} else if (pchannel->max_elements == -1) {
				if (save_restoreDebug > 1) errlogPrintf("save_restore:get_channel_values: %s has a serious problem\n",
					pchannel->name);
			}

		}
	}
	if (ca_pend_io(MIN(10.0, .1*num_channels)) != ECA_NORMAL) {
		errlogPrintf("save_restore:get_channel_values: not all gets completed");
		not_connected++;
	}

	/* convert floats and doubles, check to see which get's completed */
	for (pchannel = plist->pchan_list; pchannel != 0; pchannel = pchannel->pnext) {
		if (pchannel->valid) {
			if (ca_field_type(pchannel->chid) == DBF_FLOAT) {
				pf = (float *)pchannel->value;
				sprintf(pchannel->value, FLOAT_FMT, *pf);
			} else if (ca_field_type(pchannel->chid) == DBF_DOUBLE) {
				pd = (double *)pchannel->value;
				sprintf(pchannel->value, DOUBLE_FMT, *pd);
			}
			/* then we at least had a CA connection.  Did it produce? */
			pchannel->valid = strcmp(pchannel->value, INIT_STRING);
		} else {
			if (save_restoreDebug > 1) errlogPrintf("save_restore:get_channel_values: invalid channel %s\n", pchannel->name);
		}
	}

	return(not_connected);
}

/* state of a backup restore file */
#define BS_NONE 	0	/* Couldn't open the file */
#define BS_BAD		1	/* File exists but looks corrupted */
#define BS_OK		2	/* File is good */
#define BS_NEW		3	/* Just wrote the file */

#ifdef _WIN32
  #define BS_SEEK_DISTANCE -7
#else
  #define BS_SEEK_DISTANCE -6
#endif

STATIC int check_file(char *file)
{
	FILE *fd;
	char tmpstr[20];
	int	 file_state = BS_NONE;

	if ((fd = fopen(file, "r")) != NULL) {
		if ((fseek(fd, BS_SEEK_DISTANCE, SEEK_END)) ||
			(fgets(tmpstr, 6, fd) == 0) ||
			(strncmp(tmpstr, "<END>", 5) != 0)) {
			file_state = BS_BAD;
		} else file_state = BS_OK;

		fclose(fd);
	}
	return(file_state);
}


/*
 * Actually write the file
 *
 * NOTE: Assumes sr_mutex is locked
 *
 */
#define FPRINTF_FAILED	1
#define CLOSE_FAILED 	2
STATIC int write_it(char *filename, struct chlist *plist)
{
	FILE 			*out_fd;
	int 			filedes = -1;
	struct channel	*pchannel;
	int 			n, problem = 0;
	char			datetime[32];
    int             file_check;
    double          delta_time;
	struct stat		fileStat;		/* qiao: file state */
	char			realName[64];	/* name without trailing '$' */
	int				is_long_string;
	char			value_string[BUF_SIZE];

	fGetDateStr(datetime);

	/* open the file */
	errno = 0;
#if SET_FILE_PERMISSIONS
	/* Note: must truncate, else file retains old characters when its used length decreases. */
	filedes = open(filename, O_RDWR | O_CREAT | O_TRUNC, file_permissions);
	if (filedes < 0) {
		errlogPrintf("save_restore:write_it - unable to open file '%s' [%s]\n",
			filename, datetime);
		if (errno) myPrintErrno("write_it", __FILE__, __LINE__);
		if (++save_restoreIoErrors > save_restoreRemountThreshold) {
			save_restoreNFSOK = 0;
			strncpy(SR_recentlyStr, "Too many I/O errors",(STRING_LEN-1));
		}
		return(ERROR);
	} else {
		out_fd = fdopen(filedes, "w");
	}
#else
	if ((out_fd = fopen(filename,"w")) == NULL) {
		errlogPrintf("save_restore:write_it - unable to open file '%s' [%s]\n",
			filename, datetime);
		if (errno) myPrintErrno("write_it", __FILE__, __LINE__);
		if (++save_restoreIoErrors > save_restoreRemountThreshold) {
			save_restoreNFSOK = 0;
			strncpy(SR_recentlyStr, "Too many I/O errors",(STRING_LEN-1));
		}
		return(ERROR);
	}
#endif


	/* write header info */
	errno = 0;
	n = fprintf(out_fd,"# %s\tAutomatically generated - DO NOT MODIFY - %s\n",
			SRversion, datetime);
	if (n <= 0) {
		errlogPrintf("save_restore:write_it: fprintf returned %d. [%s]\n", n, datetime);
		if (errno) myPrintErrno("write_it", __FILE__, __LINE__);
		problem |= FPRINTF_FAILED;
		goto trouble;
	}

	if (plist->not_connected) {
		errno = 0;
		n = fprintf(out_fd,"! %d channel(s) not connected - or not all gets were successful\n",
				plist->not_connected);
		if (n <= 0) {
			errlogPrintf("save_restore:write_it: fprintf returned %d. [%s]\n", n, datetime);
			if (errno) myPrintErrno("write_it", __FILE__, __LINE__);
			problem |= FPRINTF_FAILED;
			goto trouble;
		}
	}

	/* write PV names and values */
	for (pchannel = plist->pchan_list; pchannel != 0; pchannel = pchannel->pnext) {
		errno = 0;
		is_long_string = 0;
		strcpy(realName, pchannel->name);
		if (realName[strlen(realName)-1] == '$') {
			realName[strlen(realName)-1] = '\0';
			is_long_string = 1;
		}
		if (pchannel->valid) {
			n = fprintf(out_fd, "%s ", pchannel->name);
		} else {
			n = fprintf(out_fd, "#%s ", pchannel->name);
		}
		if (n <= 0) {
			errlogPrintf("save_restore:write_it: fprintf returned %d. [%s]\n", n, datetime);
			if (errno) myPrintErrno("write_it", __FILE__, __LINE__);
			problem |= FPRINTF_FAILED;
			goto trouble;
		}

		errno = 0;
		if (pchannel->curr_elements <= 1) {
			/* treat as scalar */
			if (pchannel->enum_val >= 0) {
				n = fprintf(out_fd, "%d\n",pchannel->enum_val);
			} else {
				n = fprintf(out_fd, "%-s\n", pchannel->value);
			}
		} else if (is_long_string) {
			/* write first BUF-SIZE-1 characters of long string, so dbrestore doesn't choke. */
			strncpy(value_string, pchannel->pArray, BUF_SIZE-1);
			value_string[BUF_SIZE-1] = '\0';
			n = fprintf(out_fd, "%-s\n", value_string);
		} else {
			/* treat as array */
			n = SR_write_array_data(out_fd, pchannel->name, (void *)pchannel->pArray, pchannel->curr_elements);
		}
		if (n <= 0) {
			errlogPrintf("save_restore:write_it: fprintf returned %d [%s].\n", n, datetime);
			if (errno) myPrintErrno("write_it", __FILE__, __LINE__);
			problem |= FPRINTF_FAILED;
			goto trouble;
		}
	}

	/* debug: simulate task crash */
#if 0
	if (save_restoreDebug == 999) {
		errlogPrintf("save_restore: simulating task crash.  Bye, bye!\n");
		exit(-1);
	}
#endif

	/* write file-is-ok marker */
	errno = 0;
	n = fprintf(out_fd, "<END>\n");
	if (n <= 0) {
		errlogPrintf("save_restore:write_it: fprintf returned %d. [%s]\n", n, datetime);
		if (errno) myPrintErrno("write_it", __FILE__, __LINE__);
		problem |= FPRINTF_FAILED;
		goto trouble;
	}

	/* flush everything to disk */
	errno = 0;
	n = fflush(out_fd);
	if (n) {
		errlogPrintf("save_restore:write_it: fflush returned %d [%s]\n", n, datetime);
		if (errno) myPrintErrno("write_it", __FILE__, __LINE__);
	}


	errno = 0;
#if defined(vxWorks)
	n = ioctl(fileno(out_fd),FIOSYNC,0);	/* NFS flush to disk */
	if (n == ERROR) {
		errlogPrintf("save_restore:write_it: ioctl(,FIOSYNC,) returned %d [%s]\n",
			n, datetime);
		if (errno) myPrintErrno("write_it", __FILE__, __LINE__);
	}
#elif defined(_WIN32)
        /* WIN32 has no real equivalent to fsync? */
#else
	n = fsync(fileno(out_fd));
	if (n && (errno == ENOTSUP)) { n = 0; errno = 0; }
	if (n) {
		errlogPrintf("save_restore:write_it: fsync returned %d [%s]\n", n, datetime);
		if (errno) myPrintErrno("write_it", __FILE__, __LINE__);
	}
#endif

	/* close the file */
	errno = 0;
	n = fclose(out_fd);
	if (n) {
		errlogPrintf("save_restore:write_it: fclose returned %d [%s]\n", n, datetime);
		if (errno) myPrintErrno("write_it", __FILE__, __LINE__);
		problem |= CLOSE_FAILED;
		goto trouble;
	}
#if 0 /* SET_FILE_PERMISSIONS */
	if (filedes >= 0) {
		close(filedes);
		filedes = -1;
	}
#endif

	/* qiao: check the file state: the file contents, file size and the save time of the file */
	stat(filename, &fileStat);
    file_check = check_file(filename);
    delta_time = difftime(time(NULL), fileStat.st_mtime);
	if ((file_check != BS_OK) || (fileStat.st_size <= 0) ||  (delta_time > 10.0)) {
		errlogPrintf("save_restore:write_it: file written checking failure [%s], check_file=%d, size=%lld, delta_time=%f\n",
            datetime, file_check, (long long)fileStat.st_size, delta_time);
		return(ERROR);
	}

	/* qiao: up to now, the file is successfully saved, which means the NFS is OK. So here clean up
	          the error flag for NFS, corresponding to the auto-recover of NFS */
	save_restoreNFSOK    = 1;
	save_restoreIoErrors = 0;

	return(OK);

trouble:
	/* close the file */
	errno = 0;
	n = fclose(out_fd);
	if (n) {
		errlogPrintf("save_restore:write_it: fclose('%s') returned %d\n", plist->save_file, n);
		if (errno) myPrintErrno("write_it", __FILE__, __LINE__);
	} else {
		problem &= ~CLOSE_FAILED;
	}
	if (problem) {
		fGetDateStr(datetime);
		errlogPrintf("save_restore:write_it: Giving up on this attempt to write '%s'. [%s]\n",
			plist->save_file, datetime);
	}
#if 0 /* SET_FILE_PERMISSIONS */
	if (filedes >= 0) {
		close(filedes);
		filedes = -1;
	}
#endif
	return(problem ? ERROR : OK);
}

/*
 * save_file - save routine
 *
 * Write a list of channel names and values to an ASCII file.
 *
 * NOTE: Assumes sr_mutex is locked
 *
 */
STATIC int write_save_file(struct chlist *plist, char *configName)
{
	char	save_file[NFS_PATH_LEN+3] = "", backup_file[NFS_PATH_LEN+3] = "";
	char	tmpstr[NFS_PATH_LEN+50];
	int		backup_state = BS_OK;
	char	datetime[32];

	fGetDateStr(datetime);
	plist->status = SR_STATUS_OK;
	strcpy(plist->statusStr, "Ok");
	epicsTimeGetCurrent(&plist->save_attempt_time);

	/* Make full file names */
	if (plist->savePathPV_chid) {
		/* This list's path name comes from a PV */
		ca_array_get(DBR_STRING,1,plist->savePathPV_chid,tmpstr);
		ca_pend_io(1.0);
		if (tmpstr[0] == '\0') return(OK);
		strncpy(save_file, tmpstr, sizeof(save_file) - 1);
		if (save_file[0] != '/') {
			makeNfsPath(save_file, saveRestoreFilePath, save_file);
		}
	} else {
		/* Use standard path name. */
		strncpy(save_file, saveRestoreFilePath, sizeof(save_file) - 1);
	}
	if (configName && configName[0]) {
		makeNfsPath(save_file, save_file, configName);
	} else if (plist->saveNamePV_chid) {
		/* This list's file name comes from a PV */
		ca_array_get(DBR_STRING,1,plist->saveNamePV_chid,tmpstr);
		ca_pend_io(1.0);
		if (tmpstr[0] == '\0') return(OK);
		makeNfsPath(save_file, save_file, tmpstr);
	} else {
		/* Use file name constructed from the request file name. */
		makeNfsPath(save_file, save_file, plist->save_file);
	}

	/* Currently, all lists do backups, unless their file path or file name comes from a PV, or the configName argument. */
	if (plist->do_backups && (configName==NULL)) {
		strcpy(backup_file, save_file);
		strncat(backup_file, "B", 1);

		/* Ensure that backup is ok before we overwrite .sav file. */
		backup_state = check_file(backup_file);
		if (backup_state != BS_OK) {
			errlogPrintf("save_restore:write_save_file: Backup file (%s) bad or not found.  Writing a new one. [%s]\n",
				backup_file, datetime);
			if (backup_state == BS_BAD) {
				/* make a backup copy of the corrupted file */
				strcpy(tmpstr, backup_file);
				strcat(tmpstr, "_SBAD_");
				if (save_restoreDatedBackupFiles) {
					strcat(tmpstr, datetime);
					sprintf(SR_recentlyStr, "Bad file: '%sB'", plist->save_file);
				}
				(void)myFileCopy(backup_file, tmpstr);
			}
			if (write_it(backup_file, plist) == ERROR) {
				printf("*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***\n");
				printf("save_restore:write_save_file: Can't write new backup file. [%s]\n", datetime);
				printf("*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***\n");
				plist->status = SR_STATUS_FAIL;
				strcpy(plist->statusStr, "Can't write .savB file");
				TRY_TO_PUT_AND_FLUSH(DBR_STRING, plist->statusStr_chid, &plist->statusStr);
				return(ERROR);
			}
			plist->status = SR_STATUS_WARN;
			strcpy(plist->statusStr, ".savB file was bad");
			TRY_TO_PUT_AND_FLUSH(DBR_STRING, plist->statusStr_chid, &plist->statusStr);
			backup_state = BS_NEW;
		}
	}

	if (configName!=NULL) {
		char datetime[32];
		FILE *test_fd;

		if ((test_fd = fopen(save_file,"rb")) != NULL) {
			fGetDateStr(datetime);
			strcpy(backup_file, save_file);
			strcat(backup_file, "_");
			strcat(backup_file, datetime);
			myFileCopy(save_file,backup_file);
		}
	}

	/*** Write the save file ***/
	if (save_restoreDebug > 2) errlogPrintf("write_save_file: saving to %s\n", save_file);
	if (write_it(save_file, plist) == ERROR) {
		printf("*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***\n");
		errlogPrintf("save_restore:write_save_file: Can't write save file. [%s]\n", datetime);
		printf("*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***\n");
		plist->status = SR_STATUS_FAIL;
		strcpy(plist->statusStr, "Can't write .sav file");
		TRY_TO_PUT_AND_FLUSH(DBR_STRING, plist->statusStr_chid, &plist->statusStr);
		sprintf(SR_recentlyStr, "Can't write '%s'", plist->save_file);
		return(ERROR);
	}

	/* keep the name and time of the last saved file */
	epicsTimeGetCurrent(&plist->save_time);
	strcpy(plist->last_save_file, plist->save_file);

	if (plist->do_backups) {
		/*** Write a backup copy of the save file ***/
		if (backup_state != BS_NEW) {
			/* make a backup copy */
			if (myFileCopy(save_file, backup_file) != OK) {
				errlogPrintf("save_restore:write_save_file - Couldn't make backup '%s' [%s]\n",
					backup_file, datetime);
				plist->status = SR_STATUS_WARN;
				strcpy(plist->statusStr, "Can't copy .sav to .savB file");
				TRY_TO_PUT_AND_FLUSH(DBR_STRING, plist->statusStr_chid, &plist->statusStr);
				sprintf(SR_recentlyStr, "Can't write '%sB'", plist->save_file);
				return(ERROR);
			}
		}
	}

	/* Update status PV */
	if (plist->not_connected) {
		plist->status = SR_STATUS_WARN;
		sprintf(plist->statusStr,"%d %s not saved", plist->not_connected,
			plist->not_connected==1?"value":"values");
		TRY_TO_PUT_AND_FLUSH(DBR_STRING, plist->statusStr_chid, &plist->statusStr);
	}
	sprintf(SR_recentlyStr, "Wrote '%s'", plist->save_file);
	return(OK);
}


/*
 * do_seq - copy .sav file to .savX, where X is in
 * [0..save_restoreNumSeqFiles].  If .sav file can't be opened,
 * write .savX file explicitly, as we would write .sav file.
 *
 * NOTE: Assumes sr_mutex is locked *
 */
STATIC void do_seq(struct chlist *plist)
{
	char	*p, save_file[NFS_PATH_LEN+3] = "", backup_file[NFS_PATH_LEN+3] = "";
	int		i;
	struct stat fileStat;
	char	datetime[32];

	fGetDateStr(datetime);

	/* Make full file names */
	makeNfsPath(save_file, saveRestoreFilePath, plist->save_file);
	strcpy(backup_file, save_file);
	p = &backup_file[strlen(backup_file)];

	/* If first time for this list, determine which existing file is oldest. */
	if (plist->backup_sequence_num == -1) {
		double dTime, max_dTime = -1.e9;

		plist->backup_sequence_num = 0;
		for (i=0; i<save_restoreNumSeqFiles; i++) {
			sprintf(p, "%1d", i);	/* (over)write sequence number */
			if (stat(backup_file, &fileStat)) {
				/* can't check date; just assume this file is oldest */
				plist->backup_sequence_num = i;
				break;
			}
			dTime = difftime(time(NULL), fileStat.st_mtime);
			if (dTime > max_dTime) {
				max_dTime = dTime;
				plist->backup_sequence_num = i;
			}
		}
	}

	if (check_file(save_file) == BS_NONE) {
		errlogPrintf("save_restore:do_seq - '%s' not found.  Writing a new one. [%s]\n",
			save_file, datetime);
		(void) write_save_file(plist, NULL);
	}
	sprintf(p, "%1d", plist->backup_sequence_num);
	if (myFileCopy(save_file, backup_file) != OK) {
		errlogPrintf("save_restore:do_seq - Can't copy save file to '%s' [%s]\n",
			backup_file, datetime);
		if (write_it(backup_file, plist) == ERROR) {
			errlogPrintf("save_restore:do_seq - Can't write seq. file from PV list. [%s]\n", datetime);
			if (plist->status >= SR_STATUS_WARN) {
				plist->status = SR_STATUS_SEQ_WARN;
				strcpy(plist->statusStr, "Can't write sequence file");
			}
			sprintf(SR_recentlyStr, "Can't write '%s%1d'",
				plist->save_file, plist->backup_sequence_num);
			return;
		} else {
			errlogPrintf("save_restore:do_seq: Wrote seq. file from PV list. [%s]\n", datetime);
			sprintf(SR_recentlyStr, "Wrote '%s%1d'",
				plist->save_file, plist->backup_sequence_num);
		}
	} else {
		sprintf(SR_recentlyStr, "Wrote '%s%1d'",
			plist->save_file, plist->backup_sequence_num);
	}

	epicsTimeGetCurrent(&plist->backup_time);
	if (++(plist->backup_sequence_num) >=  save_restoreNumSeqFiles)
		plist->backup_sequence_num = 0;
}

/* Called only by the user */
int set_savefile_name(char *filename, char *save_filename)
{
	struct chlist	*plist;

	if (waitForListLock(5) == 0) {
		printf("set_savefile_name:failed to lock resource.  Try later.\n");
		return(ERROR);
	}
	plist = lptr;
	while (plist != 0) {
		if (!strcmp(plist->reqFile,filename)) {
			strcpy(plist->save_file,save_filename);
			unlockList();
			sprintf(SR_recentlyStr, "New save file: '%s'", save_filename);
			return(OK);
		}
		plist = plist->pnext;
	}
	errlogPrintf("save_restore:set_savefile_name: No save set enabled for %s\n",filename);
	unlockList();
	return(ERROR);
}


int create_periodic_set(char *filename, int period, char *macrostring)
{
	return(create_data_set(filename, PERIODIC, period, 0, 0, macrostring));
}


int create_triggered_set(char *filename, char *trigger_channel, char *macrostring)
{
	if (trigger_channel && (isalpha((int)trigger_channel[0]) || isdigit((int)trigger_channel[0]))) {
		return(create_data_set(filename, TRIGGERED, 0, trigger_channel, 0, macrostring));
	}
	else {
		errlogPrintf("save_restore:create_triggered_set: Error: trigger-channel name is required.\n");
		return(ERROR);
	}
}


int create_monitor_set(char *filename, int period, char *macrostring)
{
	return(create_data_set(filename, MONITORED, 0, 0, period, macrostring));
}


int create_manual_set(char *filename, char *macrostring)
{
	return(create_data_set(filename, MANUAL, 0, 0, 0, macrostring));
}


/*
 * create a data set
 */
STATIC int create_data_set(
	char	*filename,			/* save set request file */
	int		save_method,
	int		period,				/* maximum time between saves  */
	char	*trigger_channel,	/* db channel to trigger save  */
	int		mon_period,			/* minimum time between saves  */
	char	*macrostring
)
{
	struct chlist	*plist;
	int				inx;			/* i/o status 	       */
	int i;

	if (save_restoreDebug > 1) {
		errlogPrintf("save_restore:create_data_set: file '%s', method %x, period %d, trig_chan '%s', mon_period %d\n",
			filename, save_method, period, trigger_channel ? trigger_channel : "NONE", mon_period);
	}

	/* initialize save_restore routines */
	if (!save_restore_init) {
		if ((sr_mutex = epicsMutexCreate()) == 0) {
			errlogPrintf("save_restore:create_data_set: could not create list header mutex");
			return(ERROR);
		}
		taskID = epicsThreadCreate("save_restore", taskPriority,
			epicsThreadGetStackSize(epicsThreadStackBig),
			(EPICSTHREADFUNC)save_restore, 0);
		if (taskID == NULL) {
			errlogPrintf("save_restore:create_data_set: could not create save_restore task");
			return(ERROR);
		}

		save_restore_init = 1;
	}

	if (filename==NULL || filename[0] == '\0') {
		/* User probably wanted to start the save_restore task without creating
		 * a save set.  This is ok.
		 */
		 return(0);
	}

	/* is save set defined - add new save mode if necessary */
	while (waitForListLock(5) == 0) {
		if (save_restoreDebug > 1) errlogPrintf("create_data_set: '%s' waiting for listLock()\n", filename);
	}
	plist = lptr;
	while (plist != 0) {
		if (!strcmp(plist->reqFile,filename)) {
			if (plist->save_method & save_method) {
				errlogPrintf("save_restore:create_data_set: '%s' already in %x mode",filename,save_method);
				unlockList();
				return(ERROR);
			} else {
				/* Add a new method to an existing list */
				if (save_method == TRIGGERED) {
					if (trigger_channel) {
						strcpy(plist->trigger_channel,trigger_channel);
					} else {
						errlogPrintf("save_restore:create_data_set: no trigger channel");
						unlockList();
						return(ERROR);
					}
				} else if (save_method == PERIODIC) {
					plist->period = period;
				} else if (save_method == MONITORED) {
					plist->monitor_period = mon_period;
				}
				plist->save_method |= save_method;
				/*
				 * We used to call enable_list() from here, but it starts CA
				 * monitors that will be handled by the save_restore task.
				 * Now, we let the save_restore task do all of its own CA stuff.
				 */
				/* enable_list(plist); */

				unlockList();
				return(OK);
			}
		}
		plist = plist->pnext;
	}
	unlockList();

	/* create a new channel list */
	if ((plist = (struct chlist *)calloc(1,sizeof (struct chlist))) == (struct chlist *)0) {
		errlogPrintf("save_restore:create_data_set: channel list calloc failed");
		return(ERROR);
	}
	if (macrostring && (strlen(macrostring)>0)) {
		plist->macrostring = malloc(1+strlen(macrostring));
		strcpy(plist->macrostring, macrostring);
	}
	plist->do_backups = 1;	/* Do backups and sequences backups, unless we're told not to. */
	callbackSetCallback(periodic_save, &plist->periodicCb);
	callbackSetUser(plist, &plist->periodicCb);
	callbackSetCallback(on_change_timer, &plist->monitorCb);
	callbackSetUser(plist, &plist->monitorCb);
	strncpy(plist->reqFile, filename, sizeof(plist->reqFile)-1);
	plist->pchan_list = (struct channel *)0;
	plist->period = MAX(period, MIN_PERIOD);
	if (trigger_channel) {
	    strncpy(plist->trigger_channel, trigger_channel, sizeof(plist->trigger_channel)-1);
	} else {
	    plist->trigger_channel[0]=0;
	}
	plist->last_save_file[0] = 0;
	plist->save_method = save_method;
	plist->enabled_method = 0;
	plist->save_state = 0;
	plist->save_ok = 0;
	plist->monitor_period = MAX(mon_period, MIN_PERIOD);
	/* init times */
	epicsTimeGetCurrent(&plist->backup_time);
	epicsTimeGetCurrent(&plist->save_attempt_time);
	epicsTimeGetCurrent(&plist->save_time);
	plist->backup_sequence_num = -1;
	plist->save_ok = 0;
	plist->not_connected = -1;
	plist->status = SR_STATUS_INIT;
	strcpy(plist->statusStr,"Initializing list");

	/** construct the save_file name **/
	strcpy(plist->save_file, plist->reqFile);
#if 0
	inx = 0;
	while ((plist->save_file[inx] != 0) && (plist->save_file[inx] != '.') && (inx < (FN_LEN-6))) inx++;
#else
	/* fix bfr 2007-10-01: need to search for last '.', not first */
	inx = strlen(plist->save_file)-1;
	while (inx > 0 && plist->save_file[inx] != '.') inx--;
#endif
	plist->save_file[inx] = 0;	/* truncate if necessary to leave room for ".sav" + null */
	strcat(plist->save_file,".sav");
	/* make full name, including file path */
	makeNfsPath(plist->saveFile, saveRestoreFilePath, plist->save_file);

	/* read the request file and populate plist with the PV names */
	if (readReqFile(plist->reqFile, plist, macrostring) == ERROR) {
		free(plist);
		return(ERROR);
	}

	/* associate the list with a set of status PV's */
	for (i=0; i<NUM_STATUS_PV_SETS; i++) {
		if (!statusPvsInUse[i]) break;
	}
	plist->statusPvIndex = i;
	if (i < NUM_STATUS_PV_SETS) statusPvsInUse[i] = 1;

	/* qiao: init the call back time of this list */
	epicsTimeGetCurrent(&plist->callback_time);

	/* link it to the save set list */
	while (waitForListLock(5) == 0) {
		if (save_restoreDebug > 1) errlogPrintf("create_data_set: '%s' waiting for listLock()\n", filename);
	}
	plist->pnext = lptr;
	lptr = plist;
	strcpy(plist->statusStr,"Ready to connect...");
	unlockList();

	return(OK);
}




/*
 * save_restoreShow -  Show state of save_restore; optionally, list save sets
 *
 */
static char ca_state_string[4][10] = {"Never", "Prev", "Conn", "Closed"};
void save_restoreShow(int verbose)
{
	struct chlist	*plist;
	struct channel 	*pchannel;
	struct pathListElement *p = reqFilePathList;
	char tmpstr[50];
	char	datetime[32];
	int NFS_managed = save_restoreNFSHostName[0] && save_restoreNFSHostAddr[0] &&
		save_restoreNFSMntPoint[0];

	fGetDateStr(datetime);
	printf("BEGIN save_restoreShow\n");
	printf("  Current date-time (yymmdd-hhmmss): [%s] \n", datetime);
	printf("  Status: '%s' - '%s'\n", SR_STATUS_STR[SR_status], SR_statusStr);
	printf("  Debug level: %d\n", save_restoreDebug);
#if SET_FILE_PERMISSIONS
	printf("  File permissions: 0%o\n", (unsigned int)file_permissions);
#endif
	printf("  Save/restore incomplete save sets? %s\n", save_restoreIncompleteSetsOk?"YES":"NO");
	printf("  Write dated backup files? %s\n", save_restoreDatedBackupFiles?"YES":"NO");
	printf("  Number of sequence files to maintain: %d\n", save_restoreNumSeqFiles);
	printf("  Time interval between sequence files: %d seconds\n", save_restoreSeqPeriodInSeconds);
	printf("  Time interval between .sav-file write failure and retry: %d seconds\n", save_restoreRetrySeconds);
	printf("  NFS host: '%s'; address:'%s'\n", save_restoreNFSHostName, save_restoreNFSHostAddr);
	printf("  NFS mount point:\n    '%s'\n", save_restoreNFSMntPoint);
	printf("  NFS mount status: %s\n",
		NFS_managed ? (save_restoreNFSOK?"Ok":"Failed") : "not managed by save_restore");
	printf("  I/O errors: %d\n", save_restoreIoErrors);
	printf("  request file path list:\n");
	while (p) {
		printf("    '%s'\n", p->path);
		p = p->pnext;
	}
	printf("  save file path:\n    '%s'\n", saveRestoreFilePath);
	if (sr_mutex && (waitForListLock(5) == 1)) {
		for (plist = lptr; plist != 0; plist = plist->pnext) {
			printf("  %s: \n",plist->reqFile);
			printf("    macro string: '%s'\n", plist->macrostring ? plist->macrostring : "");
			printf("    Status PV: %s\n", plist->status_PV);
			printf("    Status: '%s' - '%s'\n", SR_STATUS_STR[plist->status], plist->statusStr);
			epicsTimeToStrftime(tmpstr, sizeof(tmpstr), TIMEFMT, &plist->save_time);
			printf("    Last save time  :%s", tmpstr);
			epicsTimeToStrftime(tmpstr, sizeof(tmpstr), TIMEFMT, &plist->backup_time);
			printf("    Last backup time:%s", tmpstr);
			strcpy(tmpstr, "[ ");
			if (plist->save_method & PERIODIC) strcat(tmpstr, "PERIODIC ");
			if (plist->save_method & TRIGGERED) strcat(tmpstr, "TRIGGERED ");
			if ((plist->save_method & MONITORED)==MONITORED) strcat(tmpstr, "TIMER+CHANGE ");
			if (plist->save_method & MANUAL) strcat(tmpstr, "MANUAL ");
			strcat(tmpstr, "]");
			printf("    methods: %s\n", tmpstr);
			strcpy(tmpstr, "[ ");
			if (plist->save_state & PERIODIC) strcat(tmpstr, "PERIOD ");
			if (plist->save_state & TRIGGERED) strcat(tmpstr, "TRIGGER ");
			if (plist->save_state & TIMER) strcat(tmpstr, "TIMER ");
			if (plist->save_state & CHANGE) strcat(tmpstr, "CHANGE ");
			if (plist->save_state & MANUAL) strcat(tmpstr, "MANUAL ");
			strcat(tmpstr, "]");
			printf("    path PV: %s\n", plist->savePathPV[0]?plist->savePathPV:"None");
			if (plist->savePathPV[0]) {
				ca_array_get(DBR_STRING, 1, plist->savePathPV_chid, tmpstr);
				printf("        path: '%s'\n", tmpstr);
			}
			printf("    name PV: %s\n", plist->saveNamePV[0]?plist->saveNamePV:"None");
			if (plist->saveNamePV[0]) {
				ca_array_get(DBR_STRING, 1, plist->saveNamePV_chid, tmpstr);
				printf("        name: '%s'\n", tmpstr);
			}
			printf("    backups: %s\n", plist->do_backups?"YES":"NO");
			printf("    save_state = 0x%x\n", plist->save_state);
			printf("    period: %d; trigger chan: '%s'; monitor period: %d\n",
			   plist->period,plist->trigger_channel,plist->monitor_period);
			printf("    last saved file - %s\n",plist->last_save_file);
			printf("    %d channel%c not connected (or ca_get failed)\n",plist->not_connected,
				(plist->not_connected == 1) ? ' ' : 's');
			if (verbose) {
				for (pchannel = plist->pchan_list; pchannel != 0; pchannel = pchannel->pnext) {
					printf("\t%s chid:%p state:%s (max:%ld curr:%ld elements)\t%s", pchannel->name,
						pchannel->chid, pchannel->chid?ca_state_string[ca_state(pchannel->chid)]:"noChid",
						pchannel->max_elements, pchannel->curr_elements, pchannel->value);
					printf("   channel_connected = %d", pchannel->channel_connected);
					if (pchannel->enum_val >= 0) printf("\t%d\n",pchannel->enum_val);
					else printf("\n");
				}
			}
		}
		unlockList();
	} else {
		if (!sr_mutex)
			printf("  The save_restore task apparently is not running.\n");
		else
			printf("  Can't lock sr_mutex.\n");
	}
	printf("reboot-restore status:\n");
	dbrestoreShow();
	printf("END save_restoreShow\n");
}


int set_requestfile_path(char *path, char *pathsub)
{
	struct pathListElement *p, *pnew;
	char fullpath[NFS_PATH_LEN+1] = "";
	int path_len=0, pathsub_len=0;

	if (path && *path) path_len = strlen(path);
	if (pathsub && *pathsub) pathsub_len = strlen(pathsub);
	if (path_len + pathsub_len > (NFS_PATH_LEN-1)) {	/* may have to add '/' */
		errlogPrintf("save_restore:set_requestfile_path: 'path'+'pathsub' is too long\n");
		return(ERROR);
	}

	makeNfsPath(fullpath, path, pathsub);

	if (*fullpath) {
		/* return(set_requestfile_path(fullpath)); */
		pnew = (struct pathListElement *)calloc(1, sizeof(struct pathListElement));
		if (pnew == NULL) {
			errlogPrintf("save_restore:set_requestfile_path: calloc failed\n");
			return(ERROR);
		}

		strcpy(pnew->path, fullpath);
		if (pnew->path[strlen(pnew->path)-1] != '/') {
			strcat(pnew->path, "/");
		}

		if (reqFilePathList == NULL) {
			reqFilePathList = pnew;
		} else {
			for (p = reqFilePathList; p->pnext; p = p->pnext)
				;
			p->pnext = pnew;
		}
		return(OK);
	} else {
		return(ERROR);
	}
}

int set_savefile_path(char *path, char *pathsub)
{
	char fullpath[NFS_PATH_LEN] = "";
	int NFS_managed = save_restoreNFSHostName[0] && save_restoreNFSHostAddr[0] && save_restoreNFSMntPoint[0];

	if (save_restoreNFSOK && NFS_managed) dismountFileSystem(save_restoreNFSMntPoint);

	makeNfsPath(fullpath, path, pathsub);

	if (*fullpath) {
		if (saveRestoreFilePathIsMountPoint) {
			strcpy(saveRestoreFilePath, fullpath);
			strcpy(save_restoreNFSMntPoint, fullpath);
		} else {
			makeNfsPath(saveRestoreFilePath, save_restoreNFSMntPoint, fullpath);
		}
		if (save_restoreNFSHostName[0] && save_restoreNFSHostAddr[0] && save_restoreNFSMntPoint[0]) {
			if (mountFileSystem(save_restoreNFSHostName, save_restoreNFSHostAddr, save_restoreNFSMntPoint, save_restoreNFSMntPoint) == OK) {
				errlogPrintf("save_restore:mountFileSystem:successfully mounted '%s'\n", save_restoreNFSMntPoint);
				strncpy(SR_recentlyStr, "mountFileSystem succeeded", (STRING_LEN-1));
			}
			else {
				errlogPrintf("save_restore: Can't mount '%s'\n", save_restoreNFSMntPoint);
			}
		}
		return(OK);
	} else {
		return(ERROR);
	}
}

int set_saveTask_priority(int priority)
{
	if ((priority < epicsThreadPriorityMin) || (priority > epicsThreadPriorityMax)) {
		epicsPrintf("save_restore - priority must be >= %d and <= %d\n",
			epicsThreadPriorityMin, epicsThreadPriorityMax);
		return(ERROR);
	}
	taskPriority = priority;
	if (taskID != NULL) {
		epicsThreadSetPriority(taskID, priority);
	}
	return(OK);
}

STATIC int remove_data_set(char *filename)
{
	op_msg msg;

	msg.operation = op_Remove;
	strncpy(msg.filename, filename, OP_MSG_FILENAME_SIZE);
	epicsMessageQueueSend(opMsgQueue, (void *)&msg, OP_MSG_SIZE);
	return(0);
}

/*** remove a data set from the list ***/
STATIC int do_remove_data_set(char *filename)
{
	int found = 0;
	int numchannels = 0;
	struct chlist *plist, *previous;
	struct channel *pchannel, *pchannelt;

	/* find the data set */
	if (waitForListLock(5) == 0) {
		printf("do_remove_data_set:failed to lock resource.  Try later.\n");
		return(ERROR);
	}
	plist = lptr;
	previous = 0;
	while(plist) {
		if (!strcmp(plist->reqFile, filename)) {
			found = 1;
			break;
		}
		previous = plist;
		plist = plist->pnext;
	}
	unlockList();

	if (found) {
		if (waitForListLock(5) == 0) {
			printf("do_remove_data_set:failed to lock resource.  Try later.\n");
			return(ERROR);
		}
		if (plist->macrostring) free(plist->macrostring);
		statusPvsInUse[plist->statusPvIndex] = 0; /* say we're not using these status PVs anymore */
		pchannel = plist->pchan_list;
		while (pchannel) {
			if (ca_clear_channel(pchannel->chid) != ECA_NORMAL) {
				errlogPrintf("save_restore:do_remove_data_set: couldn't remove ca connection for %s\n", pchannel->name);
			}
			pchannel = pchannel->pnext;
			numchannels++;
		}
		if (ca_pend_io(MIN(10.0, numchannels*0.1)) != ECA_NORMAL) {
		       errlogPrintf("save_restore:do_remove_data_set: ca_pend_io() timed out\n");
		}
		pchannel = plist->pchan_list;
		while (pchannel) {
			pchannelt = pchannel->pnext;
			if (pchannel->pArray) free(pchannel->pArray);
			free(pchannel);
			pchannel = pchannelt;
		}
		if (previous == 0) {
			lptr = plist->pnext;
		} else {
			previous->pnext = plist->pnext;
		}
		free(plist);

		unlockList();

	} else {
		errlogPrintf("save_restore:do_remove_data_set: Couldn't find '%s'\n", filename);
		sprintf(SR_recentlyStr, "Can't remove data set '%s'", filename);
		return(ERROR);
	}
	sprintf(SR_recentlyStr, "Removed data set '%s'", filename);
	return(OK);
}

int reload_periodic_set(char *filename, int period, char *macrostring)
{
	op_msg msg;

	msg.operation = op_ReloadPeriodicSet;
	msg.period = period;
	strncpy(msg.filename, filename, OP_MSG_FILENAME_SIZE);
	if (strlen(macrostring) > (OP_MSG_MACRO_SIZE-1)) {
		printf("macro string '%s' is too long for message queue\n", macrostring);
		return(-1);
	}
	strncpy(msg.macrostring, macrostring, OP_MSG_MACRO_SIZE);
	epicsMessageQueueSend(opMsgQueue, (void *)&msg, OP_MSG_SIZE);
	return(0);
}

int reload_triggered_set(char *filename, char *trigger_channel, char *macrostring)
{
	op_msg msg;

	msg.operation = op_ReloadTriggeredSet;
	strncpy(msg.filename, filename, OP_MSG_FILENAME_SIZE);
	if (strlen(macrostring) > (OP_MSG_MACRO_SIZE-1)) {
		printf("macro string '%s' is too long for message queue\n", macrostring);
		return(-1);
	}
	strncpy(msg.macrostring, macrostring, OP_MSG_MACRO_SIZE);
	strncpy(msg.trigger_channel, trigger_channel, OP_MSG_TRIGGER_SIZE);
	epicsMessageQueueSend(opMsgQueue, (void *)&msg, OP_MSG_SIZE);
	return(0);
}


int reload_monitor_set(char * filename, int period, char *macrostring)
{
	op_msg msg;

	msg.operation = op_ReloadMonitorSet;
	msg.period = period;
	strncpy(msg.filename, filename, OP_MSG_FILENAME_SIZE);
	if (strlen(macrostring) > (OP_MSG_MACRO_SIZE-1)) {
		printf("macro string '%s' is too long for message queue\n", macrostring);
		return(-1);
	}
	strncpy(msg.macrostring, macrostring, OP_MSG_MACRO_SIZE);
	epicsMessageQueueSend(opMsgQueue, (void *)&msg, OP_MSG_SIZE);
	return(0);
}

int reload_manual_set(char * filename, char *macrostring)
{
	op_msg msg;

	msg.operation = op_ReloadManualSet;
	strncpy(msg.filename, filename, OP_MSG_FILENAME_SIZE);
	if (strlen(macrostring) > (OP_MSG_MACRO_SIZE-1)) {
		printf("macro string '%s' is too long for message queue\n", macrostring);
		return(-1);
	}
	strncpy(msg.macrostring, macrostring, OP_MSG_MACRO_SIZE);
	epicsMessageQueueSend(opMsgQueue, (void *)&msg, OP_MSG_SIZE);
	return(0);
}

int fdbrestore(char *filename)
{
	printf("save_restore:fdbrestore:entry\n");
	return(request_manual_restore(filename, FROM_SAVE_FILE, NULL, NULL, NULL));
}

int fdbrestoreX(char *filename, char *macrostring, callbackFunc callbackFunction, void *puserPvt)
{
	return(request_manual_restore(filename, FROM_ASCII_FILE, macrostring, callbackFunction, puserPvt));
}

STATIC void defaultCallback(int status, void *puserPvt) {
	printf("save_restore:defaultCallback:status=%d\n", status);
}

STATIC int request_manual_restore(char *filename, int file_type, char *macrostring, callbackFunc callbackFunction, void *puserPvt)
{
	op_msg msg;

	if (save_restoreDebug >= 5) {
		errlogPrintf("save_restore:request_manual_restore: entry\n");
	}
	msg.operation = (file_type==FROM_SAVE_FILE) ? op_RestoreFromSaveFile : op_RestoreFromAsciiFile;
	if ((filename == NULL) || (strlen(filename)<1) || (strlen(filename)>=OP_MSG_FILENAME_SIZE-1)) {
		printf("request_manual_restore: bad filename\n");
		return(-1);
	}
	strncpy(msg.filename, filename, OP_MSG_FILENAME_SIZE);
	if ((macrostring) && (strlen(macrostring)>0)) {
		strncpy(msg.macrostring, macrostring, OP_MSG_MACRO_SIZE);
	} else {
		msg.macrostring[0] = '\0';
	}
	if (callbackFunction==NULL) {
		callbackFunction = defaultCallback;
		puserPvt = NULL;
	}
	msg.puserPvt = puserPvt;
	msg.callbackFunction = callbackFunction;
	epicsMessageQueueSend(opMsgQueue, (void *)&msg, OP_MSG_SIZE);
	return(0);
}

char *getMacroString(char *request_file)
{
	struct chlist	*plist;
	int				found;

	for (plist=lptr, found=0; plist && !found; ) {
		if (strcmp(plist->reqFile, request_file) == 0) {
			found = 1;
		} else {
			plist = plist->pnext;
		}
	}
	if (found) {
		return(plist->macrostring);
	} else {
		return(NULL);
	}
}

/*
 * do_manual_restore - restore routine
 *
 * Read a list of channel names and values from an ASCII file,
 * and update database vaules.
 *
 */
STATIC int do_manual_restore(char *filename, int file_type, char *macrostring)
{
	struct channel	*pchannel;
	struct chlist	*plist;
	int				found, is_scalar;
	char			PVname[80];
	char			restoreFile[NFS_PATH_LEN+1] = "";
	char			bu_filename[NFS_PATH_LEN+1] = "";
	char			buffer[BUF_SIZE], *bp, c;
	char			ebuffer[BUF_SIZE];
	char			value_string[BUF_SIZE];
	int				n;
	long			status, num_errs=0;
	FILE			*inp_fd;
	chid			chanid = 0;
	char			realName[64];	/* name without trailing '$' */
	int				is_long_string;
	MAC_HANDLE      *handle = NULL;
	char            **pairs = NULL;

	if (save_restoreDebug >= 5) {
		errlogPrintf("save_restore:do_manual_restore: entry for file '%s'\n", filename);
	}
	if (file_type == FROM_SAVE_FILE) {
		/* if this is the current file name for a save set - restore from there */
		if (waitForListLock(5) == 0) {
			printf("do_manual_restore:failed to lock resource.  Try later.\n");
			return(ERROR);
		}
		for (plist=lptr, found=0; plist && !found; ) {
			if (strcmp(plist->last_save_file,filename) == 0) {
				found = 1;
			} else {
				plist = plist->pnext;
			}
		}
		if (found) {
			/* verify quality of the save set */
			if (plist->not_connected > 0) {
				errlogPrintf("save_restore:do_manual_restore: %d channel(s) not connected or fetched\n",
					plist->not_connected);
				if (!save_restoreIncompleteSetsOk) {
					errlogPrintf("save_restore:do_manual_restore: aborting restore\n");
					unlockList();
					strncpy(SR_recentlyStr, "Manual restore failed",(STRING_LEN-1));
					return(ERROR);
				}
			}

			for (pchannel = plist->pchan_list; pchannel !=0; pchannel = pchannel->pnext) {
				if (pchannel->curr_elements <= 1) {
					status = ca_put(DBR_STRING, pchannel->chid, pchannel->value);
				} else {
					status = SR_put_array_values(pchannel->name, pchannel->pArray, pchannel->curr_elements);
				}
			}
			if (status) num_errs++;
			if (ca_pend_io(1.0) != ECA_NORMAL) {
				errlogPrintf("save_restore:do_manual_restore: not all channels restored\n");
			}
			unlockList();
			if (num_errs == 0) {
				strncpy(SR_recentlyStr, "Manual restore succeeded",(STRING_LEN-1));
			} else {
				sprintf(SR_recentlyStr, "%ld errors during manual restore", num_errs);
			}
			return(num_errs);
		}
		unlockList();
	}

	/* open file */
	makeNfsPath(restoreFile, saveRestoreFilePath, filename);

	if (file_type == FROM_SAVE_FILE) {
		inp_fd = fopen_and_check(restoreFile, &status);
	} else {
		inp_fd = fopen(restoreFile, "r");
	}
	if (inp_fd == NULL) {
		errlogPrintf("save_restore:do_manual_restore: Can't open save file.");
		strncpy(SR_recentlyStr, "Manual restore failed",(STRING_LEN-1));
		return(ERROR);
	}

	if (file_type == FROM_SAVE_FILE) {
		(void)fgets(buffer, BUF_SIZE, inp_fd); /* discard header line */
	}

	/* Prepare to use macro substitution */
	if (macrostring && macrostring[0]) {
		macCreateHandle(&handle, NULL);
		if (handle) {
			macParseDefns(handle, macrostring, &pairs);
			if (pairs) macInstallMacros(handle, pairs);
			if (save_restoreDebug >= 5) {
				errlogPrintf("save_restore:do_manual_restore: Current macro definitions:\n");
				macReportMacros(handle);
				errlogPrintf("save_restore:do_manual_restore: --------------------------\n");
			}
		}
	}

	/* restore from data file */
	while ((bp=fgets(buffer, BUF_SIZE, inp_fd))) {
		if (handle && pairs) {
			ebuffer[0] = '\0';
			macExpandString(handle, buffer, ebuffer, BUF_SIZE-1);
			bp = ebuffer;
		}

		/* get PV_name, one space character, value */
		/* (value may be a string with leading whitespace; it may be */
		/* entirely whitespace; the number of spaces may be crucial; */
		/* it might also consist of zero characters) */
		n = sscanf(bp,"%s%c%[^\n]", PVname, &c, value_string);
		if (n < 3) *value_string = 0;
		if (save_restoreDebug >= 5) {
			errlogPrintf("save_restore:do_manual_restore: PVname='%s'\n", PVname);
		}
		if (isalpha((int)PVname[0]) || isdigit((int)PVname[0])) {
			/* handle long string name */
			strcpy(realName, PVname);
			is_long_string = 0;
			if (realName[strlen(realName)-1] == '$') {
				realName[strlen(realName)-1] = '\0';
				is_long_string = 1;
			}
			is_scalar = strncmp(value_string, ARRAY_MARKER, ARRAY_MARKER_LEN);
			if (is_scalar) {
				long num_elements, field_size, field_type;
				/* check the field itself, because an empty string is saved as no value at all , which would look like a scalar. */
				SR_get_array_info(PVname, &num_elements, &field_size, &field_type);
				if (num_elements > 1) {
					if (save_restoreDebug >= 5) {
						errlogPrintf("save_restore:do_manual_restore: PV '%s' is scalar in .sav file, but has %ld elements.  Treating as array.\n",
							PVname, num_elements);
					}
					is_scalar = 0;
				}
			}
			if (is_scalar || is_long_string) {
				if (!is_long_string) {
					/* Discard additional characters until end of line */
					while (bp[strlen(bp)-1] != '\n') fgets(buffer, BUF_SIZE, inp_fd);
					value_string[40] = '\0';
					if (ca_search(realName, &chanid) != ECA_NORMAL) {
						errlogPrintf("save_restore:do_manual_restore: ca_search for %s failed\n", realName);
						num_errs++;
					} else if (ca_pend_io(0.5) != ECA_NORMAL) {
						errlogPrintf("save_restore:do_manual_restore: ca_search for %s timeout\n", realName);
						num_errs++;
					} else if (ca_put(DBR_STRING, chanid, value_string) != ECA_NORMAL) {
						errlogPrintf("save_restore:do_manual_restore: ca_put of %s to %s failed\n", value_string,realName);
						num_errs++;
					}
				} else  {
					if (save_restoreDebug >= 5) {
						errlogPrintf("save_restore:do_manual_restore: PV '%s' is long string; value='%s'.\n", PVname, value_string);
					}
					/* See if we got the whole line */
					if (bp[strlen(bp)-1] != '\n') {
						/* No, we didn't.  One more read will certainly accumulate a value string of length BUF_SIZE */
						bp = fgets(buffer, BUF_SIZE, inp_fd);
						n = BUF_SIZE-strlen(value_string)-1;
						strncat(value_string, bp, n);
						if (value_string[strlen(value_string)-1] == '\n') value_string[strlen(value_string)-1] = '\0';
					}
					/* Discard additional characters until end of line */
					while (bp[strlen(bp)-1] != '\n') bp = fgets(buffer, BUF_SIZE, inp_fd);
					if (ca_search(PVname, &chanid) != ECA_NORMAL) {
						errlogPrintf("save_restore:do_manual_restore: ca_search for %s failed\n", PVname);
						num_errs++;
					} else if (ca_pend_io(0.5) != ECA_NORMAL) {
						num_errs++;
					/* Don't forget trailing null character: "strlen(value_string)+1" below */
					} else if (ca_array_put(DBR_CHAR, strlen(value_string)+1, chanid, value_string) != ECA_NORMAL) {
						errlogPrintf("save_restore:do_manual_restore: ca_array_put of '%s' to '%s' failed\n", value_string,PVname);
						num_errs++;
					}
				}
			} else {
				status = SR_array_restore(1, inp_fd, PVname, value_string, 0);
				if (status) num_errs++;
			}
			if (chanid) {
				ca_clear_channel(chanid);
				chanid = 0;
			}
		} else if (PVname[0] == '!') {
			n = atoi(value_string);	/* value_string actually contains 2nd word of error msg */
			num_errs += n;
			errlogPrintf("save_restore:do_manual_restore: %d PV%c had no saved value\n",
				n, (n==1) ? ' ':'s');
			if (!save_restoreIncompleteSetsOk) {
				errlogPrintf("save_restore:do_manual_restore: aborting restore\n");
				fclose(inp_fd);
				if (handle) macDeleteHandle(handle);
				if (pairs) free(pairs);
				strncpy(SR_recentlyStr, "Manual restore failed",(STRING_LEN-1));
				return(ERROR);
			}
		} else if (PVname[0] == '#') {
			/* comment line */
			is_scalar = strncmp(value_string, ARRAY_MARKER, ARRAY_MARKER_LEN);
			if (!is_scalar) {
				/* Parse and gobble up the whole array, without restoring anything. */
				status = SR_array_restore(1, inp_fd, PVname, value_string, 1);
			}
		}
	}
	fclose(inp_fd);
	if (handle) macDeleteHandle(handle);
	if (pairs) free(pairs);

	if (file_type == FROM_SAVE_FILE) {
		/* make  backup */
		strcpy(bu_filename,restoreFile);
		strcat(bu_filename,".bu");
		(void)myFileCopy(restoreFile,bu_filename);
	}
	strncpy(SR_recentlyStr, "Manual restore succeeded",(STRING_LEN-1));
	return(num_errs);
}


STATIC int readReqFile(const char *reqFile, struct chlist *plist, char *macrostring)
{
	struct channel	*pchannel = NULL;
	FILE   			*inp_fd = NULL;
	char			name[80] = "", *t=NULL, line[BUF_SIZE]="", eline[BUF_SIZE]="";
	char            templatefile[FN_LEN] = "";
	char            new_macro[80] = "";
	int             i=0;
	MAC_HANDLE      *handle = NULL;
	char            **pairs = NULL;
	struct pathListElement *p;
	char tmpfile[NFS_PATH_LEN+1] = "";

	if (save_restoreDebug > 1) {
		errlogPrintf("save_restore:readReqFile: entry: reqFile='%s', plist=%p, macrostring='%s'\n",
			reqFile, (void *)plist, macrostring?macrostring:"NULL");
	}

	/* open request file */
	if (reqFilePathList) {
		/* try to find reqFile in every directory specified in reqFilePathList */
		for (p = reqFilePathList; p; p = p->pnext) {
			makeNfsPath(tmpfile, p->path, reqFile);
			inp_fd = fopen(tmpfile, "r");
			if (inp_fd) break;
		}
	} else {
		/* try to find reqFile only in current working directory */
		inp_fd = fopen(reqFile, "r");
	}

	if (!inp_fd) {
		plist->status = SR_STATUS_FAIL;
		strcpy(plist->statusStr, "Can't open .req file");
		TRY_TO_PUT_AND_FLUSH(DBR_STRING, plist->statusStr_chid, &plist->statusStr);
		errlogPrintf("save_restore:readReqFile: unable to open file %s. Exiting.\n", reqFile);
		return(ERROR);
	}

	if (macrostring && macrostring[0]) {
		macCreateHandle(&handle, NULL);
		if (handle) {
			macParseDefns(handle, macrostring, &pairs);
			if (pairs) macInstallMacros(handle, pairs);
			if (save_restoreDebug >= 5) {
				errlogPrintf("save_restore:readReqFile: Current macro definitions:\n");
				macReportMacros(handle);
				errlogPrintf("save_restore:readReqFile: --------------------------\n");
			}
		}
	}

	/* place all of the channels in the group */
	while (fgets(line, BUF_SIZE, inp_fd)) {
		/* Expand input line. */
		name[0] = '\0';
		eline[0] = '\0';
		if (handle && pairs) {
			macExpandString(handle, line, eline, BUF_SIZE-1);
		} else {
			strcpy(eline, line);
		}
		sscanf(eline, "%s", name);
		if (save_restoreDebug >= 2) errlogPrintf("save_restore:readReqFile: line='%s', eline='%s', name='%s'\n", line, eline, name);
		if (name[0] == '#') {
			/* take the line as a comment */
		} else if (strncmp(eline, "file", 4) == 0) {
			/* handle include file */
			if (save_restoreDebug >= 2) errlogPrintf("save_restore:readReqFile: preparing to include file: eline='%s'\n", eline);

			/* parse template-file name and fix obvious problems */
			templatefile[0] = '\0';
			t = &(eline[4]);
			while (isspace((int)(*t))) t++;  /* delete leading whitespace */
			if (*t == '"') t++;  /* delete leading quote */
			while (isspace((int)(*t))) t++;  /* delete any additional whitespace */
			/* copy to filename; terminate at whitespace or quote or comment */
			for (	i = 0;
					i<NFS_PATH_LEN && !(isspace((int)(*t))) && (*t != '"') && (*t != '#');
					t++,i++) {
				templatefile[i] = *t;
			}
			templatefile[i] = 0;

			/* parse new macro string and fix obvious problems */
			/* for (i=0; *t && *t != '#'; t++) { */
			for (i=0; *t; t++) {
				if (isspace((int)(*t)) || *t == ',') {
					if (i>=3 && (new_macro[i-1] != ','))
						new_macro[i++] = ',';
				} else if (*t != '"') {
					new_macro[i++] = *t;
				}
			}
			new_macro[i] = 0;
			if (i && new_macro[i-1] == ',') new_macro[--i] = 0;
			if (i < 3) new_macro[0] = 0; /* if macro has less than 3 chars, punt */
			readReqFile(templatefile, plist, new_macro);
		} else if (isalpha((int)name[0]) || isdigit((int)name[0]) || name[0] == '$') {
			pchannel = (struct channel *)calloc(1,sizeof (struct channel));
			if (pchannel == (struct channel *)0) {
				plist->status = SR_STATUS_WARN;
				strcpy(plist->statusStr, "Can't alloc channel memory");
				TRY_TO_PUT_AND_FLUSH(DBR_STRING, plist->statusStr_chid, &plist->statusStr);
				errlogPrintf("save_restore:readReqFile: channel calloc failed");
			} else {
				/* add new element to the list */
#if BACKWARDS_LIST
				pchannel->pnext = plist->pchan_list;
				if (plist->pchan_list==NULL) plist->plast_chan = pchannel;
				plist->pchan_list = pchannel;
#else
				if (plist->plast_chan) {
					plist->plast_chan->pnext = pchannel;
				} else {
					plist->pchan_list = pchannel;
				}
				plist->plast_chan = pchannel;
#endif
				strcpy(pchannel->name, name);
				strcpy(pchannel->value,"Not Connected");
				pchannel->enum_val = -1;
				pchannel->max_elements = 0;
				pchannel->curr_elements = 0;
				pchannel->channel_connected=0;          /* qiao: init the channel connection flag 0 */
				pchannel->just_created=0;               /* qiao: init the just created flag 0 */
			}
		}
	}
	/* close file */
	fclose(inp_fd);

	/*
	 * Allow macro string supplied to create_xxx_set() to specify a PV from which the
	 * path and/or file name will be read when it's time to write the file.  Currently,
	 * this can only be done when the list is defined.
	 */
	if (handle) {
		if (macGetValue(handle, "SAVEPATHPV", name, 80) > 0) {
			plist->do_backups = 0;
			strncpy(plist->savePathPV, name, 80);
		}
		if (macGetValue(handle, "SAVENAMEPV", name, 80) > 0) {
			plist->do_backups = 0;
			strncpy(plist->saveNamePV, name, 80);
		}
		if (macGetValue(handle, "CONFIG", name, 80) > 0) {
			strncpy(plist->config, name, 80);
		}
		if (macGetValue(handle, "CONFIGMENU", name, 80) > 0) {
			plist->do_backups = 0;
		}
		macDeleteHandle(handle);
		if (pairs) free(pairs);
	}

	if (save_restoreDebug > 1)
		errlogPrintf("save_restore:readReqFile: exit: reqFile='%s'.\n", reqFile);
	return(OK);
}

/* Caller has a config name, and wants to know which PVlist has that config name */
int findConfigList(char *configName, char *requestFileName) {
	struct chlist *plist = lptr;

	while (plist != 0) {
		if (strcmp(plist->config, configName) == 0) {
			strcpy(requestFileName, plist->reqFile);
			return(0);
		}
		plist = plist->pnext;
	}
	return(-1);
}

/*-------------------------------------------------------------------------------*/
/*** ioc-shell command registration (sheesh!) ***/

#define IOCSH_ARG		static const iocshArg
#define IOCSH_ARG_ARRAY	static const iocshArg * const
#define IOCSH_FUNCDEF	static const iocshFuncDef

/* int fdbrestore(char *filename); */
IOCSH_ARG       fdbrestore_Arg0    = {"filename",iocshArgString};
IOCSH_ARG_ARRAY fdbrestore_Args[1] = {&fdbrestore_Arg0};
IOCSH_FUNCDEF   fdbrestore_FuncDef = {"fdbrestore",1,fdbrestore_Args};
static void     fdbrestore_CallFunc(const iocshArgBuf *args) {fdbrestore(args[0].sval);}

/* int fdbrestoreX(char *filename, char *macrostring); */
IOCSH_ARG       fdbrestoreX_Arg0    = {"filename",iocshArgString};
IOCSH_ARG       fdbrestoreX_Arg1    = {"macrostring",iocshArgString};
IOCSH_ARG_ARRAY fdbrestoreX_Args[2] = {&fdbrestoreX_Arg0, &fdbrestoreX_Arg1};
IOCSH_FUNCDEF   fdbrestoreX_FuncDef = {"fdbrestoreX",2,fdbrestoreX_Args};
static void     fdbrestoreX_CallFunc(const iocshArgBuf *args) {fdbrestoreX(args[0].sval, args[1].sval, NULL, NULL);}

/* int manual_save(char *request_file); */
IOCSH_ARG       manual_save_Arg0    = {"request file",iocshArgString};
IOCSH_ARG_ARRAY manual_save_Args[1] = {&manual_save_Arg0};
IOCSH_FUNCDEF   manual_save_FuncDef = {"manual_save",1,manual_save_Args};
static void     manual_save_CallFunc(const iocshArgBuf *args) {manual_save(args[0].sval, NULL, NULL, NULL);}

/* int set_savefile_name(char *filename, char *save_filename); */
IOCSH_ARG       set_savefile_name_Arg0    = {"filename",iocshArgString};
IOCSH_ARG       set_savefile_name_Arg1    = {"save_filename",iocshArgString};
IOCSH_ARG_ARRAY set_savefile_name_Args[2] = {&set_savefile_name_Arg0,&set_savefile_name_Arg1};
IOCSH_FUNCDEF   set_savefile_name_FuncDef = {"set_savefile_name",2,set_savefile_name_Args};
static void     set_savefile_name_CallFunc(const iocshArgBuf *args) {set_savefile_name(args[0].sval,args[1].sval);}

/* int create_periodic_set(char *filename, int period, char *macrostring); */
IOCSH_ARG       create_periodic_set_Arg0    = {"filename",iocshArgString};
IOCSH_ARG       create_periodic_set_Arg1    = {"period",iocshArgInt};
IOCSH_ARG       create_periodic_set_Arg2    = {"macro string",iocshArgString};
IOCSH_ARG_ARRAY create_periodic_set_Args[3] = {&create_periodic_set_Arg0,&create_periodic_set_Arg1,&create_periodic_set_Arg2};
IOCSH_FUNCDEF   create_periodic_set_FuncDef = {"create_periodic_set",3,create_periodic_set_Args};
static void     create_periodic_set_CallFunc(const iocshArgBuf *args) {create_periodic_set(args[0].sval,args[1].ival,args[2].sval);}

/* int create_triggered_set(char *filename, char *trigger_channel, char *macrostring); */
IOCSH_ARG       create_triggered_set_Arg0    = {"filename",iocshArgString};
IOCSH_ARG       create_triggered_set_Arg1    = {"trigger_channel",iocshArgString};
IOCSH_ARG       create_triggered_set_Arg2    = {"macro string",iocshArgString};
IOCSH_ARG_ARRAY create_triggered_set_Args[3] = {&create_triggered_set_Arg0,&create_triggered_set_Arg1,&create_triggered_set_Arg2};
IOCSH_FUNCDEF   create_triggered_set_FuncDef = {"create_triggered_set",3,create_triggered_set_Args};
static void     create_triggered_set_CallFunc(const iocshArgBuf *args) {create_triggered_set(args[0].sval,args[1].sval,args[2].sval);}

/* int create_monitor_set(char *filename, int period, char *macrostring); */
IOCSH_ARG       create_monitor_set_Arg0    = {"filename",iocshArgString};
IOCSH_ARG       create_monitor_set_Arg1    = {"period",iocshArgInt};
IOCSH_ARG       create_monitor_set_Arg2    = {"macro string",iocshArgString};
IOCSH_ARG_ARRAY create_monitor_set_Args[3] = {&create_monitor_set_Arg0,&create_monitor_set_Arg1,&create_monitor_set_Arg2};
IOCSH_FUNCDEF   create_monitor_set_FuncDef = {"create_monitor_set",3,create_monitor_set_Args};
static void     create_monitor_set_CallFunc(const iocshArgBuf *args) {create_monitor_set(args[0].sval,args[1].ival,args[2].sval);}

/* int create_manual_set(char *filename, char *macrostring); */
IOCSH_ARG       create_manual_set_Arg0    = {"filename",iocshArgString};
IOCSH_ARG       create_manual_set_Arg1    = {"macrostring",iocshArgString};
IOCSH_ARG_ARRAY create_manual_set_Args[2] = {&create_manual_set_Arg0,&create_manual_set_Arg1};
IOCSH_FUNCDEF   create_manual_set_FuncDef = {"create_manual_set",2,create_manual_set_Args};
static void     create_manual_set_CallFunc(const iocshArgBuf *args) {create_manual_set(args[0].sval,args[1].sval);}

/* void save_restoreShow(int verbose); */
IOCSH_ARG       save_restoreShow_Arg0    = {"verbose",iocshArgInt};
IOCSH_ARG_ARRAY save_restoreShow_Args[1] = {&save_restoreShow_Arg0};
IOCSH_FUNCDEF   save_restoreShow_FuncDef = {"save_restoreShow",1,save_restoreShow_Args};
static void     save_restoreShow_CallFunc(const iocshArgBuf *args) {save_restoreShow(args[0].ival);}

/* int set_requestfile_path(char *path, char *pathsub); */
IOCSH_ARG       set_requestfile_path_Arg0    = {"path",iocshArgString};
IOCSH_ARG       set_requestfile_path_Arg1    = {"subpath",iocshArgString};
IOCSH_ARG_ARRAY set_requestfile_path_Args[2] = {&set_requestfile_path_Arg0,&set_requestfile_path_Arg1};
IOCSH_FUNCDEF   set_requestfile_path_FuncDef = {"set_requestfile_path",2,set_requestfile_path_Args};
static void     set_requestfile_path_CallFunc(const iocshArgBuf *args) {set_requestfile_path(args[0].sval,args[1].sval);}

/* int set_savefile_path(char *path, char *pathsub); */
IOCSH_ARG       set_savefile_path_Arg0    = {"path",iocshArgString};
IOCSH_ARG       set_savefile_path_Arg1    = {"subpath",iocshArgString};
IOCSH_ARG_ARRAY set_savefile_path_Args[2] = {&set_savefile_path_Arg0,&set_savefile_path_Arg1};
IOCSH_FUNCDEF   set_savefile_path_FuncDef = {"set_savefile_path",2,set_savefile_path_Args};
static void     set_savefile_path_CallFunc(const iocshArgBuf *args) {set_savefile_path(args[0].sval,args[1].sval);}

/* int set_saveTask_priority(int priority); */
IOCSH_ARG       set_saveTask_priority_Arg0    = {"priority",iocshArgInt};
IOCSH_ARG_ARRAY set_saveTask_priority_Args[1] = {&set_saveTask_priority_Arg0};
IOCSH_FUNCDEF   set_saveTask_priority_FuncDef = {"set_saveTask_priority",1,set_saveTask_priority_Args};
static void     set_saveTask_priority_CallFunc(const iocshArgBuf *args) {set_saveTask_priority(args[0].ival);}

/* aqiao: void save_restoreSet_NFSHost(char *hostname, char *address, char *mntpoint); */
IOCSH_ARG       save_restoreSet_NFSHost_Arg0    = {"hostname",iocshArgString};
IOCSH_ARG       save_restoreSet_NFSHost_Arg1    = {"address", iocshArgString};
IOCSH_ARG       save_restoreSet_NFSHost_Arg2    = {"mntpoint",iocshArgString};
IOCSH_ARG_ARRAY save_restoreSet_NFSHost_Args[3] = {&save_restoreSet_NFSHost_Arg0,
                                                   &save_restoreSet_NFSHost_Arg1,
                                                   &save_restoreSet_NFSHost_Arg2};
IOCSH_FUNCDEF   save_restoreSet_NFSHost_FuncDef = {"save_restoreSet_NFSHost",3,save_restoreSet_NFSHost_Args};
static void     save_restoreSet_NFSHost_CallFunc(const iocshArgBuf *args) {save_restoreSet_NFSHost(args[0].sval,args[1].sval,args[2].sval);}

/* int remove_data_set(char *filename); */
IOCSH_ARG       remove_data_set_Arg0    = {"filename",iocshArgString};
IOCSH_ARG_ARRAY remove_data_set_Args[1] = {&remove_data_set_Arg0};
IOCSH_FUNCDEF   remove_data_set_FuncDef = {"remove_data_set",1,remove_data_set_Args};
static void     remove_data_set_CallFunc(const iocshArgBuf *args) {remove_data_set(args[0].sval);}

/* int reload_periodic_set(char *filename, int period, char *macrostring); */
IOCSH_ARG       reload_periodic_set_Arg0    = {"filename",iocshArgString};
IOCSH_ARG       reload_periodic_set_Arg1    = {"period",iocshArgInt};
IOCSH_ARG       reload_periodic_set_Arg2    = {"macro string",iocshArgString};
IOCSH_ARG_ARRAY reload_periodic_set_Args[3] = {&reload_periodic_set_Arg0, &reload_periodic_set_Arg1,&reload_periodic_set_Arg2};
IOCSH_FUNCDEF   reload_periodic_set_FuncDef = {"reload_periodic_set",3,reload_periodic_set_Args};
static void     reload_periodic_set_CallFunc(const iocshArgBuf *args) {reload_periodic_set(args[0].sval,args[1].ival,args[2].sval);}

/* int reload_triggered_set(char *filename, char *trigger_channel, char *macrostring); */
IOCSH_ARG       reload_triggered_set_Arg0    = {"filename",iocshArgString};
IOCSH_ARG       reload_triggered_set_Arg1    = {"trigger_channel",iocshArgString};
IOCSH_ARG       reload_triggered_set_Arg2    = {"macro string",iocshArgString};
IOCSH_ARG_ARRAY reload_triggered_set_Args[3] = {&reload_triggered_set_Arg0, &reload_triggered_set_Arg1,&reload_triggered_set_Arg2};
IOCSH_FUNCDEF   reload_triggered_set_FuncDef = {"reload_triggered_set",3,reload_triggered_set_Args};
static void     reload_triggered_set_CallFunc(const iocshArgBuf *args) {reload_triggered_set(args[0].sval,args[1].sval,args[2].sval);}

/* int reload_monitor_set(char *filename, int period, char *macrostring); */
IOCSH_ARG       reload_monitor_set_Arg0    = {"filename",iocshArgString};
IOCSH_ARG       reload_monitor_set_Arg1    = {"period",iocshArgInt};
IOCSH_ARG       reload_monitor_set_Arg2    = {"macro string",iocshArgString};
IOCSH_ARG_ARRAY reload_monitor_set_Args[3] = {&reload_monitor_set_Arg0, &reload_monitor_set_Arg1,&reload_monitor_set_Arg2};
IOCSH_FUNCDEF   reload_monitor_set_FuncDef = {"reload_monitor_set",3,reload_monitor_set_Args};
static void     reload_monitor_set_CallFunc(const iocshArgBuf *args) {reload_monitor_set(args[0].sval,args[1].ival,args[2].sval);}

/* int reload_manual_set(char *filename, char *macrostring); */
IOCSH_ARG       reload_manual_set_Arg0    = {"filename",iocshArgString};
IOCSH_ARG       reload_manual_set_Arg1    = {"macrostring",iocshArgString};
IOCSH_ARG_ARRAY reload_manual_set_Args[2] = {&reload_manual_set_Arg0,&reload_manual_set_Arg1};
IOCSH_FUNCDEF   reload_manual_set_FuncDef = {"reload_manual_set",2,reload_manual_set_Args};
static void     reload_manual_set_CallFunc(const iocshArgBuf *args) {reload_manual_set(args[0].sval,args[1].sval);}

/* void save_restoreSet_Debug(int level); */
IOCSH_ARG       save_restoreSet_Debug_Arg0    = {"level",iocshArgInt};
IOCSH_ARG_ARRAY save_restoreSet_Debug_Args[1] = {&save_restoreSet_Debug_Arg0};
IOCSH_FUNCDEF   save_restoreSet_Debug_FuncDef = {"save_restoreSet_Debug",1,save_restoreSet_Debug_Args};
static void     save_restoreSet_Debug_CallFunc(const iocshArgBuf *args) {save_restoreSet_Debug(args[0].ival);}

/* void save_restoreSet_NumSeqFiles(int numSeqFiles); */
IOCSH_ARG       save_restoreSet_NumSeqFiles_Arg0    = {"numSeqFiles",iocshArgInt};
IOCSH_ARG_ARRAY save_restoreSet_NumSeqFiles_Args[1] = {&save_restoreSet_NumSeqFiles_Arg0};
IOCSH_FUNCDEF   save_restoreSet_NumSeqFiles_FuncDef = {"save_restoreSet_NumSeqFiles",1,save_restoreSet_NumSeqFiles_Args};
static void     save_restoreSet_NumSeqFiles_CallFunc(const iocshArgBuf *args) {save_restoreSet_NumSeqFiles(args[0].ival);}

/* void save_restoreSet_SeqPeriodInSeconds(int period); */
IOCSH_ARG       save_restoreSet_SeqPeriodInSeconds_Arg0    = {"period",iocshArgInt};
IOCSH_ARG_ARRAY save_restoreSet_SeqPeriodInSeconds_Args[1] = {&save_restoreSet_SeqPeriodInSeconds_Arg0};
IOCSH_FUNCDEF   save_restoreSet_SeqPeriodInSeconds_FuncDef = {"save_restoreSet_SeqPeriodInSeconds",1,save_restoreSet_SeqPeriodInSeconds_Args};
static void     save_restoreSet_SeqPeriodInSeconds_CallFunc(const iocshArgBuf *args) {save_restoreSet_SeqPeriodInSeconds(args[0].ival);}

/* void save_restoreSet_IncompleteSetsOk(int ok); */
IOCSH_ARG       save_restoreSet_IncompleteSetsOk_Arg0    = {"ok",iocshArgInt};
IOCSH_ARG_ARRAY save_restoreSet_IncompleteSetsOk_Args[1] = {&save_restoreSet_IncompleteSetsOk_Arg0};
IOCSH_FUNCDEF   save_restoreSet_IncompleteSetsOk_FuncDef = {"save_restoreSet_IncompleteSetsOk",1,save_restoreSet_IncompleteSetsOk_Args};
static void     save_restoreSet_IncompleteSetsOk_CallFunc(const iocshArgBuf *args) {save_restoreSet_IncompleteSetsOk(args[0].ival);}

/* void save_restoreSet_DatedBackupFiles(int ok); */
IOCSH_ARG       save_restoreSet_DatedBackupFiles_Arg0    = {"ok",iocshArgInt};
IOCSH_ARG_ARRAY save_restoreSet_DatedBackupFiles_Args[1] = {&save_restoreSet_DatedBackupFiles_Arg0};
IOCSH_FUNCDEF   save_restoreSet_DatedBackupFiles_FuncDef = {"save_restoreSet_DatedBackupFiles",1,save_restoreSet_DatedBackupFiles_Args};
static void     save_restoreSet_DatedBackupFiles_CallFunc(const iocshArgBuf *args) {save_restoreSet_DatedBackupFiles(args[0].ival);}

/* void save_restoreSet_status_prefix(char *prefix); */
IOCSH_ARG       save_restoreSet_status_prefix_Arg0    = {"prefix",iocshArgString};
IOCSH_ARG_ARRAY save_restoreSet_status_prefix_Args[1] = {&save_restoreSet_status_prefix_Arg0};
IOCSH_FUNCDEF   save_restoreSet_status_prefix_FuncDef = {"save_restoreSet_status_prefix",1,save_restoreSet_status_prefix_Args};
static void     save_restoreSet_status_prefix_CallFunc(const iocshArgBuf *args) {save_restoreSet_status_prefix(args[0].sval);}

#if SET_FILE_PERMISSIONS
/* void save_restoreSet_FilePermissions(int permissions); */
IOCSH_ARG       save_restoreSet_FilePermissions_Arg0    = {"permissions",iocshArgInt};
IOCSH_ARG_ARRAY save_restoreSet_FilePermissions_Args[1] = {&save_restoreSet_FilePermissions_Arg0};
IOCSH_FUNCDEF   save_restoreSet_FilePermissions_FuncDef = {"save_restoreSet_FilePermissions",1,save_restoreSet_FilePermissions_Args};
static void     save_restoreSet_FilePermissions_CallFunc(const iocshArgBuf *args) {save_restoreSet_FilePermissions(args[0].ival);}
#endif

/* void save_restoreSet_RetrySeconds(int seconds); */
IOCSH_ARG       save_restoreSet_RetrySeconds_Arg0    = {"seconds",iocshArgInt};
IOCSH_ARG_ARRAY save_restoreSet_RetrySeconds_Args[1] = {&save_restoreSet_RetrySeconds_Arg0};
IOCSH_FUNCDEF   save_restoreSet_RetrySeconds_FuncDef = {"save_restoreSet_RetrySeconds",1,save_restoreSet_RetrySeconds_Args};
static void     save_restoreSet_RetrySeconds_CallFunc(const iocshArgBuf *args) {save_restoreSet_RetrySeconds(args[0].ival);}

/* void save_restoreSet_UseStatusPVs(int ok); */
IOCSH_ARG       save_restoreSet_UseStatusPVs_Arg0    = {"ok",iocshArgInt};
IOCSH_ARG_ARRAY save_restoreSet_UseStatusPVs_Args[1] = {&save_restoreSet_UseStatusPVs_Arg0};
IOCSH_FUNCDEF   save_restoreSet_UseStatusPVs_FuncDef = {"save_restoreSet_UseStatusPVs",1,save_restoreSet_UseStatusPVs_Args};
static void     save_restoreSet_UseStatusPVs_CallFunc(const iocshArgBuf *args) {save_restoreSet_UseStatusPVs(args[0].ival);}

/* qiao: void save_restoreSet_CAReconnect(int ok); */
IOCSH_ARG       save_restoreSet_CAReconnect_Arg0    = {"ok",iocshArgInt};
IOCSH_ARG_ARRAY save_restoreSet_CAReconnect_Args[1] = {&save_restoreSet_CAReconnect_Arg0};
IOCSH_FUNCDEF   save_restoreSet_CAReconnect_FuncDef = {"save_restoreSet_CAReconnect",1,save_restoreSet_CAReconnect_Args};
static void     save_restoreSet_CAReconnect_CallFunc(const iocshArgBuf *args) {save_restoreSet_CAReconnect(args[0].ival);}

/* qiao: void save_restoreSet_CallbackTimeout(int t); */
IOCSH_ARG       save_restoreSet_CallbackTimeout_Arg0    = {"t",iocshArgInt};
IOCSH_ARG_ARRAY save_restoreSet_CallbackTimeout_Args[1] = {&save_restoreSet_CallbackTimeout_Arg0};
IOCSH_FUNCDEF   save_restoreSet_CallbackTimeout_FuncDef = {"save_restoreSet_CallbackTimeout",1,save_restoreSet_CallbackTimeout_Args};
static void     save_restoreSet_CallbackTimeout_CallFunc(const iocshArgBuf *args) {save_restoreSet_CallbackTimeout(args[0].ival);}

void save_restoreRegister(void)
{
    iocshRegister(&fdbrestore_FuncDef, fdbrestore_CallFunc);
    iocshRegister(&fdbrestoreX_FuncDef, fdbrestoreX_CallFunc);
    iocshRegister(&manual_save_FuncDef, manual_save_CallFunc);
    iocshRegister(&set_savefile_name_FuncDef, set_savefile_name_CallFunc);
    iocshRegister(&create_periodic_set_FuncDef, create_periodic_set_CallFunc);
    iocshRegister(&create_triggered_set_FuncDef, create_triggered_set_CallFunc);
    iocshRegister(&create_monitor_set_FuncDef, create_monitor_set_CallFunc);
    iocshRegister(&create_manual_set_FuncDef, create_manual_set_CallFunc);
    iocshRegister(&save_restoreShow_FuncDef, save_restoreShow_CallFunc);
    iocshRegister(&set_requestfile_path_FuncDef, set_requestfile_path_CallFunc);
    iocshRegister(&set_savefile_path_FuncDef, set_savefile_path_CallFunc);
    iocshRegister(&set_saveTask_priority_FuncDef, set_saveTask_priority_CallFunc);
    iocshRegister(&save_restoreSet_NFSHost_FuncDef, save_restoreSet_NFSHost_CallFunc);
    iocshRegister(&remove_data_set_FuncDef, remove_data_set_CallFunc);
    iocshRegister(&reload_periodic_set_FuncDef, reload_periodic_set_CallFunc);
    iocshRegister(&reload_triggered_set_FuncDef, reload_triggered_set_CallFunc);
    iocshRegister(&reload_monitor_set_FuncDef, reload_monitor_set_CallFunc);
    iocshRegister(&reload_manual_set_FuncDef, reload_manual_set_CallFunc);
    iocshRegister(&save_restoreSet_Debug_FuncDef, save_restoreSet_Debug_CallFunc);
    iocshRegister(&save_restoreSet_NumSeqFiles_FuncDef, save_restoreSet_NumSeqFiles_CallFunc);
    iocshRegister(&save_restoreSet_SeqPeriodInSeconds_FuncDef, save_restoreSet_SeqPeriodInSeconds_CallFunc);
    iocshRegister(&save_restoreSet_IncompleteSetsOk_FuncDef, save_restoreSet_IncompleteSetsOk_CallFunc);
    iocshRegister(&save_restoreSet_DatedBackupFiles_FuncDef, save_restoreSet_DatedBackupFiles_CallFunc);
    iocshRegister(&save_restoreSet_status_prefix_FuncDef, save_restoreSet_status_prefix_CallFunc);
#if SET_FILE_PERMISSIONS
    iocshRegister(&save_restoreSet_FilePermissions_FuncDef, save_restoreSet_FilePermissions_CallFunc);
#endif
    iocshRegister(&save_restoreSet_RetrySeconds_FuncDef, save_restoreSet_RetrySeconds_CallFunc);
    iocshRegister(&save_restoreSet_UseStatusPVs_FuncDef, save_restoreSet_UseStatusPVs_CallFunc);
    iocshRegister(&save_restoreSet_CAReconnect_FuncDef, save_restoreSet_CAReconnect_CallFunc);
    iocshRegister(&save_restoreSet_CallbackTimeout_FuncDef, save_restoreSet_CallbackTimeout_CallFunc);
}

epicsExportRegistrar(save_restoreRegister);
