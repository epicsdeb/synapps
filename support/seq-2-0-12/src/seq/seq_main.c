/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990-1994, The Regents of the University of California
	and the University of Chicago.
	Los Alamos National Laboratory

 	seq_main.c,v 1.2 1995/06/27 15:25:58 wright Exp

	DESCRIPTION: Seq() initiates a sequence as a group of cooperating
	tasks.  An optional string parameter specifies the values for
	macros.  The PV context and auxiliary thread are shared by all state
	programs.

	ENVIRONMENT: VxWorks

	HISTORY:
23apr91,ajk	Fixed problem with state program invoking the sequencer.
01jul91,ajk	Added ANSI functional prototypes.
05jul91,ajk	Changed semCreate() in three places to semBCreate.
		Modified semTake() second param. to WAIT_FOREVER.
		These provide VX5.0 compatability.  
16aug91,ajk	Improved "magic number" error message.
25oct91,ajk	Code to create semaphores "pSS->getSemId" was left out.
		Added this code to init_sscb().
25nov91,ajk	Removed obsolete seqLog() code dealing with global locking.
04dec91,ajk	Implemented state program linked list, eliminating need for
		task variables.
11dec91,ajk	Cleaned up comments.
05feb92,ajk	Decreased minimum allowable stack size to SPAWN_STACK_SIZE/2.
24feb92,ajk	Print error code for log file failure.
28apr92,ajk	Implemented new event flag mode.
29apr92,ajk	Now alocates private program structures, even when reentry option
		is not specified.  This avoids problems with seqAddTask().
29apr92,ajk	Implemented mutual exclusion lock in seq_log().
16feb93,ajk	Converted to single channel access task for all state programs.
16feb93,ajk	Removed VxWorks pre-v5 stuff.
17feb93,ajk	Evaluation of channel names moved here from seq_ca.c.
19feb93,ajk	Fixed time stamp format for seq_log().
16jun93,ajk	Fixed taskSpawn() to have 15 args per vx5.1.
20jul93,ajk	Replaced obsolete delete() with remove() per vx5.1 release notes.
20jul93,ajk	Removed #define ANSI
15mar94,ajk	Implemented i/f to snc through structures in seqCom.h.
15mar94,ajk	Allowed assignment of array elements to db.
15mar94,ajk	Rearranged code that builds program structures.
02may94,ajk	Performed initialization when sequencer is evoked, even w/o
		parameters.
19jul95,ajk	Added unsigned types (unsigned char, short, int, long).
20jul95,ajk	Added priority specification at run time. 
03aug95,ajk	Fixed problem with +r option: user variable space (pSP->pVar)
		was not being allocated.
03jun96,ajk	Now compiles with -wall and -pedantic switches.
09aug96,wfl	Added initialization of syncQ queues.
30apr99,wfl	Replaced VxWorks dependencies with OSI.
17may99,wfl	Under UNIX, call OSIthreadJoinAll() rather than exiting.
17may99,wfl	Moved misplaced event array initialization.
06jul99,wfl	Slightly improved Unix command-line interpreter.
07sep99,wfl	Added "-" Unix command (==seqChanShow("-"));
		Unconditionally create get/put completion semaphores.
22sep99,grw     Supported entry and exit actions; supported state options.
18feb00,wfl	Re-worked args (inc. run-time debug level) for seqAuxThread;
		Called sprog_delete() on Unix thread exit
29feb00,wfl	Supported new OSI (and errlogPrintf); got rid of remaining
		OS-dependencies; improved command-line interpreter
31mar00,wfl	Supported 'shell' macro to run shell; made caSemId a mutex
***************************************************************************/
/*#define	DEBUG	1*/

#include	<sys/types.h>
#include	<sys/stat.h>

#include	<errno.h>
#include	<fcntl.h>
#include 	<string.h>
#include 	<stddef.h>
#include 	<stdarg.h>

/* #include 	<unistd.h>*/

#define epicsExportSharedSymbols
#include	"seq.h"

#ifdef		DEBUG
#undef		LOCAL
#define		LOCAL
#endif		/*DEBUG*/

/* function prototypes for local routines */
LOCAL	SPROG *seqInitTables(struct seqProgram *);
LOCAL	void init_sprog(struct seqProgram *, SPROG *);
LOCAL	void init_sscb(struct seqProgram *, SPROG *);
LOCAL	void init_chan(struct seqProgram *, SPROG *);
LOCAL	void init_mac(SPROG *);

LOCAL	void seq_logInit(SPROG *);
LOCAL	void seqChanNameEval(SPROG *);
LOCAL	void selectDBtype(char *, short *, short *, short *, short *);

#define	SCRATCH_SIZE	(MAX_MACROS*(MAX_STRING_SIZE+1)*12)

/*	Globals */

/*	Auxiliary sequencer thread id; used to share PV context. */
epicsThreadId seqAuxThreadId = (epicsThreadId) 0;

/*
 * seq: User-callable routine to initiate a state program.
 * Usage:  seq(<pSP>, <macros string>, <stack size>)
 *	pSP is the ptr to the state program structure.
 *	Example:  seq(&myprog, "logfile=mylog", 0)
 * When called from the shell, the 2nd & 3rd parameters are optional.
 *
 * Creates the initial state program thread and returns its thread id.
 * Most initialization is performed here.
 */
epicsShareFunc epicsThreadId epicsShareAPI seq (
    struct seqProgram *pSeqProg, char *macroDef, unsigned int stackSize)
{
	epicsThreadId	tid;
	extern char	*seqVersion;
	SPROG		*pSP;
	char		*pValue, *pThreadName;
	unsigned int	smallStack;
	AUXARGS		auxArgs;
	extern		void *seqAuxThread(void *);

	/* Print version & date of sequencer */
	printf("%s\n", seqVersion);

	/* Exit if no parameters specified */
	if (pSeqProg == 0)
	{
		return 0;
	}

	/* Check for correct state program format */
	if (pSeqProg->magic != MAGIC)
	{	/* Oops */
		errlogPrintf("Illegal magic number in state program.\n");
		errlogPrintf(" - Possible mismatch between SNC & SEQ "
			"versions\n");
		errlogPrintf(" - Re-compile your program?\n");
		epicsThreadSleep( 1.0 );	/* let error messages get printed */
		return 0;
	}

	/* Initialize the sequencer tables */
	pSP = seqInitTables(pSeqProg);

	/* Parse the macro definitions from the "program" statement */
	seqMacParse(pSeqProg->pParams, pSP);

	/* Parse the macro definitions from the command line */
	seqMacParse(macroDef, pSP);

	/* Do macro substitution on channel names */
	seqChanNameEval(pSP);

	/* Initialize sequencer logging */
	seq_logInit(pSP);

	/* Specify stack size */
	if (stackSize == 0)
		stackSize = epicsThreadGetStackSize(THREAD_STACK_SIZE);
	pValue = seqMacValGet(pSP->pMacros, "stack");
	if (pValue != NULL && strlen(pValue) > 0)
	{
		sscanf(pValue, "%ud", &stackSize);
	}
	smallStack = epicsThreadGetStackSize(epicsThreadStackSmall);
	if (stackSize < smallStack)
		stackSize = smallStack;
	pSP->stackSize = stackSize;

	/* Specify thread name */
	pValue = seqMacValGet(pSP->pMacros, "name");
	if (pValue != NULL && strlen(pValue) > 0)
		pThreadName = pValue;
	else
		pThreadName = pSP->pProgName;

	/* Specify PV system name (defaults to CA) */
	pValue = seqMacValGet(pSP->pMacros, "pvsys");
	if (pValue != NULL && strlen(pValue) > 0)
		auxArgs.pPvSysName = pValue;
	else
		auxArgs.pPvSysName = "ca";

	/* Determine debug level (currently only used for PV-level debugging) */
	pValue = seqMacValGet(pSP->pMacros, "debug");
	if (pValue != NULL && strlen(pValue) > 0)
		auxArgs.debug = atol(pValue);
	else
		auxArgs.debug = 0;

	/* Spawn the sequencer auxiliary thread */
	if (seqAuxThreadId == (epicsThreadId) 0)
	{
		unsigned int auxStack = epicsThreadGetStackSize(epicsThreadStackMedium);
		epicsThreadCreate("seqAux", THREAD_PRIORITY+1, auxStack,
				(EPICSTHREADFUNC)seqAuxThread, &auxArgs);
		while (seqAuxThreadId == (epicsThreadId) 0)
		    /* wait for thread to init. message system */
		    epicsThreadSleep(0.1);

		if (seqAuxThreadId == (epicsThreadId) -1)
		{
		    epicsThreadSleep( 1.0 );	/* let error messages get printed */
		    return 0;
		}
#ifdef	DEBUG
	printf("thread seqAux spawned, tid=%p\n", (int) seqAuxThreadId);
#endif	/*DEBUG*/
	}

	/* Spawn the initial sequencer thread */
#ifdef	DEBUG
	printf("Spawning thread %s, stackSize=%d\n", pThreadName,
		pSP->stackSize);
#endif	/*DEBUG*/
	/* Specify thread priority */
	pSP->threadPriority = THREAD_PRIORITY;
	pValue = seqMacValGet(pSP->pMacros, "priority");
	if (pValue != NULL && strlen(pValue) > 0)
	{
		sscanf(pValue, "%ud", &(pSP->threadPriority));
	}
	if (pSP->threadPriority > THREAD_PRIORITY)
		pSP->threadPriority = THREAD_PRIORITY;

	tid = epicsThreadCreate(pThreadName, pSP->threadPriority, pSP->stackSize,
			      (EPICSTHREADFUNC)sequencer, pSP);

	printf("Spawning state program \"%s\", thread %p: \"%s\"\n",
		     pSP->pProgName, tid, pThreadName);

	return tid;
}
/* seqInitTables - initialize sequencer tables */
LOCAL SPROG *seqInitTables(pSeqProg)
struct seqProgram	*pSeqProg;
{
	SPROG		*pSP;

	pSP = (SPROG *)calloc(1, sizeof (SPROG));

	/* Initialize state program block */
	init_sprog(pSeqProg, pSP);

	/* Initialize state set control blocks */
	init_sscb(pSeqProg, pSP);

	/* Initialize database channel blocks */
	init_chan(pSeqProg, pSP);

	/* Initialize the macro table */
	init_mac(pSP);


	return pSP;
}
/*
 * Copy data from seqCom.h structures into this thread's dynamic structures
 * as defined in seq.h.
 */
LOCAL void init_sprog(pSeqProg, pSP)
struct seqProgram	*pSeqProg;
SPROG			*pSP;
{
	int		i, nWords;

	/* Copy information for state program */
	pSP->numSS = pSeqProg->numSS;
	pSP->numChans = pSeqProg->numChans;
	pSP->numEvents = pSeqProg->numEvents;
	pSP->options = pSeqProg->options;
	pSP->pProgName = pSeqProg->pProgName;
	pSP->entryFunc = (ENTRY_FUNC)pSeqProg->entryFunc;
	pSP->exitFunc = (EXIT_FUNC)pSeqProg->exitFunc;
	pSP->varSize = pSeqProg->varSize;
	/* Allocate user variable area if reentrant option (+r) is set */
	if ((pSP->options & OPT_REENT) != 0)
		pSP->pVar = (char *)calloc(pSP->varSize, 1);

#ifdef	DEBUG
	printf("init_sprog: num SS=%d, num Chans=%d, num Events=%d, "
		"Prog Name=%s, var Size=%d\n", pSP->numSS, pSP->numChans,
		pSP->numEvents, pSP->pProgName, pSP->varSize);
#endif	/*DEBUG*/

	/* Create a semaphore for resource locking on PV events */
	pSP->caSemId = epicsMutexMustCreate();
	pSP->connCount = 0;
	pSP->assignCount = 0;
	pSP->logFd = NULL;

	/* Allocate an array for event flag bits */
	nWords = (pSP->numEvents + NBITS - 1) / NBITS;
	if (nWords == 0)
		nWords = 1;
	pSP->pEvents = (bitMask *)calloc(nWords, sizeof(bitMask));
	for (i = 0; i < nWords; i++)
		pSP->pEvents[i] = 0;

	/* Allocate and initialize syncQ queues */
	pSP->numQueues = pSeqProg->numQueues;
	pSP->pQueues = NULL;

	if (pSP->numQueues > 0 )
	{
		pSP->pQueues = (ELLLIST *)calloc(pSP->numQueues,
						 sizeof(ELLLIST));
		for (i = 0; i < pSP->numQueues; i++)
			ellInit(&pSP->pQueues[i]);
	}

	return;
}
/*
 * Initialize the state set control blocks
 */
LOCAL void init_sscb(pSeqProg, pSP)
struct seqProgram	*pSeqProg;
SPROG			*pSP;
{
	SSCB		*pSS;
	STATE		*pState;
	int		nss, nstates;
	struct seqSS	*pSeqSS;
	struct seqState	*pSeqState;


	/* Allocate space for the SSCB structures */
	pSP->pSS = pSS = (SSCB *)calloc(pSeqProg->numSS, sizeof(SSCB));

	/* Copy information for each state set and state */
	pSeqSS = pSeqProg->pSS;
	for (nss = 0; nss < pSeqProg->numSS; nss++, pSS++, pSeqSS++)
	{
		/* Fill in SSCB */
		pSS->pSSName = pSeqSS->pSSName;
		pSS->numStates = pSeqSS->numStates;
		pSS->errorState = pSeqSS->errorState;
		pSS->currentState = 0; /* initial state */
		pSS->nextState = 0;
		pSS->prevState = 0;
		pSS->threadId = 0;
		/* Initialize to start time rather than zero time! */
		pvTimeGetCurrentDouble(&pSS->timeEntered);
		pSS->sprog = pSP;
#ifdef	DEBUG
		printf("init_sscb: SS Name=%s, num States=%d, pSS=%p\n",
			pSS->pSSName, pSS->numStates, pSS);
#endif	/*DEBUG*/
		pSS->allFirstConnectAndMonitorSemId = epicsEventMustCreate(epicsEventEmpty);
		/* Create a binary semaphore for synchronizing events in a SS */
		pSS->syncSemId = epicsEventMustCreate(epicsEventEmpty);

		/* Create binary semaphores for synchronous pvGet() and
		   pvPut() */
		pSS->getSemId = epicsEventMustCreate(epicsEventFull);
		pSS->putSemId = epicsEventMustCreate(epicsEventFull);

		/* Create binary semaphores for thread death */
		pSS->death1SemId = epicsEventMustCreate(epicsEventEmpty);
		pSS->death2SemId = epicsEventMustCreate(epicsEventEmpty);
		pSS->death3SemId = epicsEventMustCreate(epicsEventEmpty);
		pSS->death4SemId = epicsEventMustCreate(epicsEventEmpty);

		/* Allocate & fill in state blocks */
		pSS->pStates = pState = (STATE *)calloc(pSS->numStates,
							sizeof(STATE));

		pSeqState = pSeqSS->pStates;
		for (nstates = 0; nstates < pSeqSS->numStates;
					       nstates++, pState++, pSeqState++)
		{
			pState->pStateName = pSeqState->pStateName;
			pState->actionFunc = (ACTION_FUNC)pSeqState->actionFunc;
			pState->eventFunc = (EVENT_FUNC)pSeqState->eventFunc;
			pState->delayFunc = (DELAY_FUNC)pSeqState->delayFunc;
			pState->entryFunc = (ENTRY_FUNC)pSeqState->entryFunc;
			pState->exitFunc = (EXIT_FUNC)pSeqState->exitFunc;
			pState->pEventMask = pSeqState->pEventMask;
                        pState->options = pSeqState->options;
#ifdef	DEBUG
		printf("init_sscb: State Name=%s, Event Mask=%p\n",
			pState->pStateName, *pState->pEventMask);
#endif	/*DEBUG*/
		}
	}

#ifdef	DEBUG
	printf("init_sscb: numSS=%d\n", pSP->numSS);
#endif	/*DEBUG*/
	return;
}

/*
 * init_chan--Build the database channel structures.
 * Note:  Actual PV name is not filled in here. */
LOCAL void init_chan(pSeqProg, pSP)
struct seqProgram	*pSeqProg;
SPROG			*pSP;
{
	int		nchan;
	CHAN		*pDB;
	struct seqChan	*pSeqChan;

	/* Allocate space for the CHAN structures */
	pSP->pChan = (CHAN *)calloc(pSP->numChans, sizeof(CHAN));
	pDB = pSP->pChan;

	pSeqChan = pSeqProg->pChan;
	for (nchan = 0; nchan < pSP->numChans; nchan++, pDB++, pSeqChan++)
	{
#ifdef	DEBUG
		printf("init_chan: pDB=%p\n", pDB);
#endif	/*DEBUG*/
		pDB->sprog = pSP;
		pDB->sset = NULL;	/* set temporarily during get/put */
		pDB->dbAsName = pSeqChan->dbAsName;
		pDB->pVarName = pSeqChan->pVarName;
		pDB->pVarType = pSeqChan->pVarType;
		pDB->pVar = pSeqChan->pVar;	/* offset for +r option */
		pDB->count = pSeqChan->count;
		pDB->efId = pSeqChan->efId;
		pDB->monFlag = pSeqChan->monFlag;
		pDB->eventNum = pSeqChan->eventNum;
		pDB->queued = pSeqChan->queued;
		pDB->maxQueueSize = pSeqChan->maxQueueSize ?
				    pSeqChan->maxQueueSize : MAX_QUEUE_SIZE;
		pDB->queueIndex = pSeqChan->queueIndex;
		pDB->assigned = 0;

		/* Latest error message (dynamically allocated) */
		pDB->message = NULL;

		/* Fill in get/put db types, element size, & access offset */
		selectDBtype(pSeqChan->pVarType, &pDB->getType,
		 &pDB->putType, &pDB->size, &pDB->dbOffset);

		/* Reentrant option: Convert offset to addr of the user var. */
		if ((pSP->options & OPT_REENT) != 0)
			pDB->pVar += (ptrdiff_t)pSP->pVar;
#ifdef	DEBUG
		printf(" Assigned Name=%s, VarName=%s, VarType=%s, "
			"count=%d\n", pDB->dbAsName, pDB->pVarName,
			pDB->pVarType, pDB->count);
		printf("   size=%d, dbOffset=%d\n", pDB->size,
			pDB->dbOffset);
		printf("   efId=%d, monFlag=%d, eventNum=%d\n",
			pDB->efId, pDB->monFlag, pDB->eventNum);
#endif	/*DEBUG*/
	}
}

/* 
 * init_mac - initialize the macro table.
 */
LOCAL void init_mac(pSP)
SPROG		*pSP;
{
	int		i;
	MACRO		*pMac;

	pSP->pMacros = pMac = (MACRO *)calloc(MAX_MACROS, sizeof (MACRO));
#ifdef	DEBUG
	printf("init_mac: pMac=%p\n", pMac);
#endif	/*DEBUG*/

	for (i = 0 ; i < MAX_MACROS; i++, pMac++)
	{
		pMac->pName = NULL;
		pMac->pValue = NULL;
	}
}	
/*
 * Evaluate channel names by macro substitution.
 */
#define		MACRO_STR_LEN	(MAX_STRING_SIZE+1)
LOCAL void seqChanNameEval(pSP)
SPROG		*pSP;
{
	CHAN		*pDB;
	int		i;

	pDB = pSP->pChan;
	for (i = 0; i < pSP->numChans; i++, pDB++)
	{
		pDB->dbName = calloc(1, MACRO_STR_LEN);
		seqMacEval(pDB->dbAsName, pDB->dbName, MACRO_STR_LEN, pSP->pMacros);
#ifdef	DEBUG
		printf("seqChanNameEval: \"%s\" evaluated to \"%s\"\n",
			pDB->dbAsName, pDB->dbName);
#endif	/*DEBUG*/
	}
}
/*
 * selectDBtype -- returns types for DB put/get, element size, and db access
 * offset based on user variable type.
 * Mapping is determined by the following typeMap[] array.
 * pvTypeTIME_* types for gets/monitors return status and time stamp.
 */
LOCAL	struct typeMap {
	char	*pTypeStr;
	short	putType;
	short	getType;
	short	size;
	short	offset;
} typeMap[] = {
	{
	"char",		 pvTypeCHAR,	pvTypeTIME_CHAR,
	sizeof (char),   OFFSET(pvTimeChar, value[0])
	},

	{
	"short",	 pvTypeSHORT,	pvTypeTIME_SHORT,
	sizeof (short),  OFFSET(pvTimeShort, value[0])
	},

	{
	"int",		 pvTypeLONG,	pvTypeTIME_LONG,
	sizeof (long),   OFFSET(pvTimeLong, value[0])
	},

	{
	"long",		 pvTypeLONG,	pvTypeTIME_LONG,
	sizeof (long),   OFFSET(pvTimeLong, value[0])
	},

	{
	"unsigned char", pvTypeCHAR,	pvTypeTIME_CHAR,
	sizeof (char),   OFFSET(pvTimeChar, value[0])
	},

	{
	"unsigned short",pvTypeSHORT,	pvTypeTIME_SHORT,
	sizeof (short),  OFFSET(pvTimeShort, value[0])
	},

	{
	"unsigned int",  pvTypeLONG,	pvTypeTIME_LONG,
	sizeof (long),   OFFSET(pvTimeLong, value[0])
	},

	{
	"unsigned long", pvTypeLONG,	pvTypeTIME_LONG,
	sizeof (long),   OFFSET(pvTimeLong, value[0])
	},

	{
	"float",	 pvTypeFLOAT,	pvTypeTIME_FLOAT,
	sizeof (float),  OFFSET(pvTimeFloat, value[0])
	},

	{
	"double",	 pvTypeDOUBLE,	pvTypeTIME_DOUBLE,
	sizeof (double), OFFSET(pvTimeDouble, value[0])
	},

	{
	"string",	 pvTypeSTRING,	pvTypeTIME_STRING,
	MAX_STRING_SIZE, OFFSET(pvTimeString, value[0])
	},

	{
	0, 0, 0, 0, 0
	}
};

LOCAL void selectDBtype(pUserType, pGetType, pPutType, pSize, pOffset)
char		*pUserType;
short		*pGetType, *pPutType, *pSize, *pOffset;
{
	struct typeMap	*pMap;

	for (pMap = &typeMap[0]; *pMap->pTypeStr != 0; pMap++)
	{
		if (strcmp(pUserType, pMap->pTypeStr) == 0)
		{
			*pGetType = pMap->getType;
			*pPutType = pMap->putType;
			*pSize = pMap->size;
			*pOffset = pMap->offset;
			return;
		}
	}
	*pGetType = *pPutType = *pSize = *pOffset = 0; /* this shouldn't happen */

	return;
}
/*
 * seq_logInit() - Initialize logging.
 * If "logfile" is not specified, then we log to standard output.
 */
LOCAL void seq_logInit(pSP)
SPROG		*pSP;
{
	char		*pValue;
	FILE		*fd;

	/* Create a logging resource locking semaphore */
	pSP->logSemId = epicsMutexMustCreate();
	pSP->pLogFile = "";

	/* Check for logfile spec. */
	pValue = seqMacValGet(pSP->pMacros, "logfile");
	if (pValue != NULL && strlen(pValue) > 0)
	{	/* Create & open a new log file for write only */
		fd = fopen(pValue, "w");
		if (fd == NULL)
		{
			errlogPrintf("Log file open error, file=%s, error=%s\n",
			pSP->pLogFile, strerror(errno));
		}
		else
		{
			errlogPrintf("Log file opened, fd=%d, file=%s\n",
				     fileno(fd), pValue);
			pSP->logFd = fd;
			pSP->pLogFile = pValue;
		}
	}
}
/*
 * seq_logv
 * Log a message to the console or a file with thread name, date, & time of day.
 * The format looks like "mythread 12/13/93 10:07:43: Hello world!".
 */
#define	LOG_BFR_SIZE	200

epicsStatus seq_logv(SPROG *pSP, const char *fmt, va_list args)
{
	int		count, status;
	epicsTimeStamp	timeStamp;
	char		logBfr[LOG_BFR_SIZE], *eBfr=logBfr+LOG_BFR_SIZE, *pBfr;
	FILE		*fd = pSP->logFd ? pSP->logFd : stdout;
	pBfr = logBfr;

	/* Enter thread name */
	sprintf(pBfr, "%s ", epicsThreadGetNameSelf() );
	pBfr += strlen(pBfr);

	/* Get time of day */
	epicsTimeGetCurrent(&timeStamp);	/* time stamp format */

	/* Convert to text format: "yyyy/mm/dd hh:mm:ss" */
	epicsTimeToStrftime(pBfr, eBfr-pBfr, "%Y/%m/%d %H:%M:%S", &timeStamp);
	pBfr += strlen(pBfr);

	/* Insert ": " */
	*pBfr++ = ':';
	*pBfr++ = ' ';

	/* Append the user's msg to the buffer */
	vsprintf(pBfr, fmt, args);
	pBfr += strlen(pBfr);

	/* Write the msg */
	epicsMutexMustLock(pSP->logSemId);
	count = pBfr - logBfr + 1;
	status = fwrite(logBfr, 1, count, fd);
	epicsMutexUnlock(pSP->logSemId);

	if (status != count)
	{
		errlogPrintf("Log file write error, fd=%d, file=%s, error=%s\n",
			fileno(pSP->logFd), pSP->pLogFile, strerror(errno));
		return ERROR;
	}

	/* If this is not stdout, flush the buffer */
	if (fd != stdout)
	{
		epicsMutexMustLock(pSP->logSemId);
		fflush(pSP->logFd);
		epicsMutexUnlock(pSP->logSemId);
	}
	return OK;
}
/*
 * seq_seqLog() - State program interface to seq_log().
 * Does not require ptr to state program block.
 */
epicsShareFunc long epicsShareAPI seq_seqLog(SS_ID ssId, const char *fmt, ...)
{
    SPROG	*pSP;
    va_list     args;
    long rtn;

    va_start (args, fmt);
    pSP = ((SSCB *)ssId)->sprog;
    rtn = seq_logv(pSP, fmt, args);
    va_end (args);
    return(rtn);
}
