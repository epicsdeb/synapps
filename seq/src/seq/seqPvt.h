/*************************************************************************\
Copyright (c) 1991-1993 The Regents of the University of California
                        and the University of Chicago.
                        Los Alamos National Laboratory
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
/*	Internal common definitions for the run-time sequencer library
 *
 *      Author:         Andy Kozubal
 *      Date:           
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 */
#ifndef	INCLseqPvth
#define	INCLseqPvth

#include "seqCom.h"
#define boolean seqBool
#define bitMask seqMask

#include "seq_queue.h"

#define valPtr(ch,ss)		((char*)(ss)->var+(ch)->offset)
#define bufPtr(ch)		((char*)(ch)->sprog->var+(ch)->offset)

#define ssNum(ss)		((ss)-(ss)->sprog->ss)
#define chNum(ch)		((ch)-(ch)->sprog->chan)

#define metaPtr(ch,ss) (				\
	(ch)->dbch					\
	?(((ch)->sprog->options&OPT_SAFE)		\
		?((ch)->dbch->ssMetaData+ssNum(ss))	\
		:(&(ch)->dbch->metaData))		\
	:0						\
)

/* Generic iteration on lists */
#define foreach(e,l)		for (e = l; e != 0; e = e->next)

/* Generic min and max */
#ifndef min
#define min(x, y)		(((x) < (y)) ? (x) : (y))
#endif

#ifndef max
#define max(x, y)		(((x) < (y)) ? (y) : (x))
#endif

#define free(p)			{DEBUG("%s:%d:free(%p)\n",__FILE__,__LINE__,p); if(p){free(p); p=0;}}

/* Generic allocation */
#define newArray(type,count)	(DEBUG("%s:%d:calloc(%u,%u)\n",__FILE__,__LINE__,count,sizeof(type)),(type *)calloc(count, sizeof(type)))
#define new(type)		newArray(type,1)

typedef struct db_channel	DBCHAN;
typedef struct channel		CHAN;
typedef seqState		STATE;
typedef struct macro		MACRO;
typedef struct state_set	SSCB;
typedef struct program_instance	SPROG;
typedef struct pvreq		PVREQ;
typedef const struct pv_type	PVTYPE;
typedef struct pv_meta_data	PVMETA;

/* Channel, i.e. an assigned variable */
struct channel
{
	/* static channel data (assigned once on startup) */
	size_t		offset;		/* offset to value (e.g. in sprog->var) */
	const char	*varName;	/* variable name */
	unsigned	count;		/* number of elements in array */
	unsigned	eventNum;	/* event number */
	PVTYPE		*type;		/* request type info */
	SPROG		*sprog;		/* state program that owns this struct*/

	/* dynamic channel data (assigned at runtime) */
	DBCHAN		*dbch;		/* channel assigned to a named db pv */
	EV_ID		syncedTo;	/* event flag id if synced */
	CHAN		*nextSynced;	/* next channel synced to same flag */
	QUEUE		queue;		/* queue if queued */
	boolean		monitored;	/* whether channel is monitored */
	/* buffer access, only used in safe mode */
	epicsMutexId	varLock;	/* mutex for locking access to shared
					   var buffer and meta data */
};

struct pv_type
{
	const char	*typeStr;
	pvType		putType;
	pvType		getType;
	size_t		size;
};

/* Meta data received from pv layer per request/monitor */
struct pv_meta_data
{
	epicsTimeStamp	timeStamp;	/* time stamp */
	pvStat		status;		/* status code */
	pvSevr		severity;	/* severity code */
	const char	*message;	/* error message */
};

/* Channel assigned to a named (database) pv */
struct db_channel
{
	char		*dbName;	/* channel name after macro expansion */
	void		*pvid;		/* PV (process variable) id */
	unsigned	dbCount;	/* actual count for db access */
	boolean		connected;	/* whether channel is connected */
	void		*monid;		/* monitor id (supplied by PV lib) */
	boolean		gotOneMonitor;	/* whether got at least one monitor */
	PVMETA		metaData;	/* meta data (shared buffer) */
	PVMETA		*ssMetaData;	/* array of meta data,
					   one for each state set (safe mode) */
};

struct state_set
{
	/* static state set data (assigned once on startup) */
	const char	*ssName;	/* state set name (for debugging) */
	epicsThreadId	threadId;	/* thread id */
	unsigned	numStates;	/* number of states */
	STATE		*states;	/* ptr to array of state blocks */
	unsigned	maxNumDelays;	/* max. number of delays */
	SPROG		*sprog;		/* ptr back to state program block */

	/* dynamic state set data (assigned at runtime) */
	int		currentState;	/* current state index, -1 if none */
	int		nextState;	/* next state index, -1 if none */
	int		prevState;	/* previous state index, -1 if none */
	const bitMask	*mask;		/* current event mask */
	unsigned	numDelays;	/* number of delays activated */
	double		*delay;		/* queued delay values in secs (array) */
	boolean		*delayExpired;	/* whether delay expired (array) */
	double		timeEntered;	/* time that a state was entered */
	epicsEventId	syncSemId;	/* semaphore for event sync */
	epicsEventId	dead;		/* event to signal state set exit done */
	/* these are arrays, one for each channel */
	epicsEventId	*getSemId;	/* semaphores for async get */
	epicsEventId	*putSemId;	/* semaphores for async put */
	/* safe mode */
	boolean		*dirty;		/* array of flags, one for each channel */
	USER_VAR	*var;		/* variable value block */
};

struct program_instance
{
	/* static program data (assigned once on startup) */
	const char	*progName;	/* program name (for messages) */
	int		instance;	/* program instance number */
	unsigned	threadPriority;	/* thread priority (all threads) */
	unsigned	stackSize;	/* stack size (all threads) */
	void		*pvSys;		/* pv system handle */
	char		*pvSysName;	/* pv system name ("ca", "ktl", ...) */
	int		debug;		/* pv system debug level */
	CHAN		*chan;		/* table of channels */
	unsigned	numChans;	/* number of channels */
	QUEUE		*queues;	/* array of syncQ queues */
	unsigned	numQueues;	/* number of syncQ queues */
	SSCB		*ss;		/* array of state set control blocks */
	unsigned	numSS;		/* number of state sets */
	USER_VAR	*var;		/* user variable area (shared buffer) */
	size_t		varSize;	/* size of user variable area */
	MACRO		*macros;	/* ptr to macro table */
	char		*params;	/* program parameters */
	unsigned	options;	/* options (bit-encoded) */
	INIT_FUNC	*initFunc;	/* init function */
	ENTRY_FUNC	*entryFunc;	/* entry function */
	EXIT_FUNC	*exitFunc;	/* exit function */
	unsigned	numEvFlags;	/* number of event flags */

	/* dynamic program data (assigned at runtime) */
	epicsMutexId	programLock;	/* mutex for locking dynamic program data */
        /* the following five members must always be protected by programLock */
	bitMask		*evFlags;	/* event bits for event flags & channels */
	CHAN		**syncedChans;	/* for each event flag, start of synced list */
	unsigned	assignCount;	/* number of channels assigned to ext. pv */
	unsigned	connectCount;	/* number of channels connected */
	unsigned	monitorCount;	/* number of channels monitored */
	unsigned	firstMonitorCount; /* number of channels that received
					   at least one monitor event */

	void		*pvReqPool;	/* freeList for pv requests (has own lock) */
	boolean		die;		/* flag set when seqStop is called */
	epicsEventId	ready;		/* all channels connected & got 1st monitor */
	epicsEventId	dead;		/* event to signal exit of main thread done */
	SPROG		*next;		/* next element in program list (global lock) */
};

/* Request data for pvPut and pvGet */
struct pvreq
{
	CHAN		*ch;		/* requested variable */
	SSCB		*ss;		/* state set that made the request */
};

/* Thread parameters */
#define THREAD_NAME_SIZE	32
#define THREAD_STACK_SIZE	epicsThreadStackBig
#define THREAD_PRIORITY		epicsThreadPriorityMedium

/* Internal procedures */

/* seq_task.c */
void sequencer (void *arg);
void ss_write_buffer(CHAN *ch, void *val, PVMETA *meta, boolean dirtify);
void ss_read_buffer(SSCB *ss, CHAN *ch, boolean dirty_only);
void ss_read_buffer_selective(SPROG *sp, SSCB *ss, EV_ID ev_flag);
void seqWakeup(SPROG *sp, unsigned eventNum);
void seqCreatePvSys(SPROG *sp);
/* seq_mac.c */
void seqMacParse(SPROG *sp, const char *macStr);
char *seqMacValGet(SPROG *sp, const char *name);
void seqMacEval(SPROG *sp, const char *inStr, char *outStr, size_t maxChar);
void seqMacFree(SPROG *sp);
/* seq_ca.c */
void seq_get_handler(void *var, pvType type, unsigned count,
	pvValue *value, void *arg, pvStat status);
void seq_put_handler(void *var, pvType type, unsigned count,
	pvValue *value, void *arg, pvStat status);
void seq_mon_handler(void *var, pvType type, unsigned count,
	pvValue *value, void *arg, pvStat status);
void seq_conn_handler(void *var,int connected);
pvStat seq_connect(SPROG *sp, boolean wait);
void seq_disconnect(SPROG *sp);
pvStat seq_monitor(CHAN *ch, boolean on);
/* seq_prog.c */
typedef int seqTraversee(SPROG *prog, void *param);
void seqTraverseProg(seqTraversee *func, void *param);
SSCB *seqFindStateSet(epicsThreadId threadId);
SPROG *seqFindProg(epicsThreadId threadId);
void seqDelProg(SPROG *sp);
void seqAddProg(SPROG *sp);
/* seqCommands.c */
typedef int sequencerProgramTraversee(SPROG **sprog, seqProgram *pseq, void *param);
int traverseSequencerPrograms(sequencerProgramTraversee *traversee, void *param);
/* seq_main.c */
void seq_free(SPROG *sp);
/* debug/query support */
typedef int pr_fun(const char *format,...);
void print_channel_value(pr_fun *, CHAN *ch, void *val);

#endif	/*INCLseqPvth*/
