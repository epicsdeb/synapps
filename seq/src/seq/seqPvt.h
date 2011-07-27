/*seqPvt.h
 *
 *	DESCRIPTION: Definitions for the run-time sequencer.
 *
 *      Author:         Andy Kozubal
 *      Date:           
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991,2,3, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
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
 *
 * Modification Log:
 * -----------------
 * 07mar91,ajk	Changed SSCB semaphore id names.
 * 05jul91,ajk	Added function prototypes.
 * 16dec91,ajk	Setting up for VxWorks version 5.
 * 27apr92,ajk	Changed to new event flag mode (SSCB & PROG changed).
 * 27apr92,ajk	Removed getSemId from CHAN.
 * 28apr92,ajk	Implemented new event flag mode.
 * 17feb93,ajk	Fixed some functions prototypes.
 * 10jun93,ajk	Removed VxWorks V4/V5 conditional compile.
 * 20jul93,ajk	Removed non-ANSI function definitions.
 * 21mar94,ajk	Implemented new i/f with snc (see seqCom.h).
 * 21mar94,ajk	Implemented assignment of array elements to db.  Also,
 *		allow "unlimited" number of channels.
 * 28mar94,ajk	Added time stamp support.
 * 29mar94,ajk	Added dbOffset in db_channel structure; allows faster processing
 *		of values returned with monitors and pvGet().
 * 09aug96,wfl	Added syncQ queue support.
 * 30apr99,wfl	Replaced VxWorks dependencies with OSI.
 * 17may99,wfl	Changed FUNCPTR etc to SEQFUNCPTR; corrected sequencer() proto.
 * 07sep99,wfl	Added putComplete, message and putSemId to SSCB.
 * 22sep99,grw  Supported entry and exit actions; supported state options.
 * 18feb00,wfl	Added 'auxiliary_args' typedef (for seqAuxThread).
 * 29feb00,wfl	Converted to new OSI; defs for new thread death scheme etc.
 * 06mar00,wfl	Added function prototypes for global routines.
 * 31mar00,wfl	Made caSemId a mutex; added seqFindProgXxx() prototypes.
 * 22mar01,mrk	mover to seqPvt.h from seq.h for stupid windows problem
 */
#ifndef	INCLseqPvth
#define	INCLseqPvth

#define		OK 0
#define		ERROR (-1)
#define		LOCAL static

typedef		void (*SEQVOIDFUNCPTR) (); /* used for task watchdog only */

/* global variable for PV system context */
#ifdef		DECLARE_PV_SYS
		void *pvSys;
#else
		extern void *pvSys;
#endif

/* Structure to hold information about database channels */
struct	db_channel
{
	/* These are supplied by SNC */
	char		*dbAsName;	/* channel name from assign statement */
	char		*pVar;		/* ptr to variable */
	char		*pVarName;	/* variable name string */
	char		*pVarType;	/* variable type string (e.g. ("int") */
	long		count;		/* number of elements in array */
	long		efId;		/* event flag id if synced */
	long		eventNum;	/* event number */
	epicsBoolean	monFlag;	/* TRUE if channel is to be monitored */
	int		queued;		/* TRUE if queued via syncQ */
#define MAX_QUEUE_SIZE 100		/* default max_queue_size */
	int		maxQueueSize;	/* max syncQ queue size (0 => def) */
	int		queueIndex;	/* syncQ queue index */

	/* These are filled in at run time */
	char		*dbName;	/* channel name after macro expansion */
	long		index;		/* index in array of db channels */
	void		*pvid;		/* PV (process variable) id */
	epicsBoolean	assigned;	/* TRUE only if channel is assigned */
	epicsBoolean	connected;	/* TRUE only if channel is connected */
	epicsBoolean	getComplete;	/* TRUE if previous pvGet completed */
	epicsBoolean	putComplete;	/* TRUE if previous pvPut completed */
	epicsBoolean	putWasComplete;	/* previous value of putComplete */
	short		dbOffset;	/* offset to value in db access struct*/
	short		status;		/* last db access status code */
	epicsTimeStamp	timeStamp;	/* time stamp */
	long		dbCount;	/* actual count for db access */
	short		severity;	/* last db access severity code */
	short		size;		/* size (in bytes) of single var elem */
	short		getType;	/* db get type (e.g. DBR_STS_INT) */
	short		putType;	/* db put type (e.g. DBR_INT) */
	char		*message;	/* last db access error message */
        epicsBoolean    gotFirstMonitor;
        epicsBoolean    gotFirstConnect;
	epicsBoolean	monitored;	/* TRUE if channel IS monitored */
	void		*evid;		/* event id (supplied by PV lib) */
	struct state_program *sprog;	/* state program that owns this struct*/
	struct state_set_control_block *sset; /* current state-set (temp.) */
};
typedef	struct db_channel CHAN;

/* Structure for syncQ queue entry */
struct	queue_entry
{
	ELLNODE		node;		/* linked list node */
	CHAN		*pDB;		/* ptr to db channel info */
	pvValue		value;		/* value, time stamp etc */
};
typedef struct queue_entry QENTRY;


/* Structure to hold information about a state */
struct	state_info_block
{
	char		*pStateName;	/* state name */
	ACTION_FUNC	actionFunc;	/* ptr to action rout. for this state */
	EVENT_FUNC	eventFunc;	/* ptr to event rout. for this state */
	DELAY_FUNC	delayFunc;	/* ptr to delay rout. for this state */
        ENTRY_FUNC	entryFunc;      /* ptr to entry rout. for this state */
	EXIT_FUNC	exitFunc;       /* ptr to exit rout. for this state */
	bitMask		*pEventMask;	/* event mask for this state */
        bitMask         options;        /* options mask for this state */
};
typedef	struct	state_info_block STATE;

#define	MAX_NDELAY	20	/* max # delays allowed in each SS */
/* Structure to hold information about a State Set */
struct	state_set_control_block
{
	char		*pSSName;	/* state set name (for debugging) */
	epicsThreadId	threadId;	/* thread id */
	unsigned int	threadPriority;	/* thread priority */
	unsigned int	stackSize;	/* stack size */
	epicsEventId	allFirstConnectAndMonitorSemId;
	epicsEventId	syncSemId;	/* semaphore for event sync */
	epicsEventId	getSemId;	/* semaphore id for async get */
	epicsEventId	putSemId;	/* semaphore id for async put */
	epicsEventId	death1SemId;	/* semaphore id for death (#1) */
	epicsEventId	death2SemId;	/* semaphore id for death (#2) */
	epicsEventId	death3SemId;	/* semaphore id for death (#3) */
	epicsEventId	death4SemId;	/* semaphore id for death (#4) */
	long		numStates;	/* number of states */
	STATE		*pStates;	/* ptr to array of state blocks */
	short		currentState;	/* current state index */
	short		nextState;	/* next state index */
	short		prevState;	/* previous state index */
	short		errorState;	/* error state index (-1 if none defd)*/
	short		transNum;	/* highest prio trans. # triggered */
	bitMask		*pMask;		/* current event mask */
	long		numDelays;	/* number of delays activated */
	double		delay[MAX_NDELAY]; /* queued delay value in secs */
	epicsBoolean	delayExpired[MAX_NDELAY]; /* TRUE if delay expired */
	double		timeEntered;	/* time that a state was entered */
	struct state_program *sprog;	/* ptr back to state program block */
};
typedef	struct	state_set_control_block SSCB;

/* Macro table */
typedef	struct	macro {
	char	*pName;
	char	*pValue;
} MACRO;

/* All information about a state program.
	The address of this structure is passed to the run-time sequencer:
 */
struct	state_program
{
	char		*pProgName;	/* program name (for debugging) */
	epicsThreadId	threadId;	/* thread id (main thread) */
	unsigned int	threadPriority;	/* thread priority */
	unsigned int	stackSize;	/* stack size */
	epicsMutexId	caSemId;	/* mutex for locking CA events */
	CHAN		*pChan;		/* table of channels */
	long		numChans;	/* number of db channels, incl. unass */
	long		assignCount;	/* number of db channels assigned */
	long		connCount;	/* number of channels connected */
	long		firstConnectCount;
        long		numMonitoredChans;
        long		firstMonitorCount;
        epicsBoolean	allFirstConnectAndMonitor;
	SSCB		*pSS;		/* array of state set control blocks */
	long		numSS;		/* number of state sets */
	char		*pVar;		/* ptr to user variable area */
	long		varSize;	/* # bytes in user variable area */
	MACRO		*pMacros;	/* ptr to macro table */
	char		*pParams;	/* program paramters */
	bitMask		*pEvents;	/* event bits for event flags & db */
	long		numEvents;	/* number of events */
	long		options;	/* options (bit-encoded) */
	ENTRY_FUNC	entryFunc;	/* entry function */
	EXIT_FUNC	exitFunc;	/* exit function */
	epicsMutexId	logSemId;	/* logfile locking semaphore */
	FILE		*logFd;		/* logfile file descr. */
	char		*pLogFile;	/* logfile name */
	int		numQueues;	/* number of syncQ queues */
	ELLLIST		*pQueues;	/* ptr to syncQ queues */

};
typedef	struct state_program SPROG;

/* Auxiliary thread arguments */
struct	auxiliary_args
{
        char		*pPvSysName;	/* PV system ("ca", "ktl", ...) */
        long		debug;		/* debug level */
};
typedef struct auxiliary_args AUXARGS;

/* Macro parameters */
#define	MAX_MACROS	50

/* Thread parameters */
#define THREAD_NAME_SIZE	32
#define THREAD_STACK_SIZE	epicsThreadStackBig
#define THREAD_PRIORITY		epicsThreadPriorityMedium

/* Function declarations for internal sequencer funtions */
void	seqWakeup (SPROG *, long);
void	seqFree (SPROG *);
long	sequencer (SPROG *);
long	sprogDelete (long);
long	seqMacParse (char *, SPROG *);
char	*seqMacValGet (MACRO *, char *);
void	seqMacEval (char *, char *, long, MACRO *);
epicsStatus seq_log ();
SPROG	*seqFindProg (epicsThreadId);
SPROG  *seqFindProg(epicsThreadId tid);

#endif	/*INCLseqPvth*/
