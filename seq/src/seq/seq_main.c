/*************************************************************************\
Copyright (c) 1990-1994 The Regents of the University of California
                        and the University of Chicago.
                        Los Alamos National Laboratory
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
#include "seq.h"
#include "seq_debug.h"

static boolean init_sprog(SPROG *sp, seqProgram *seqProg);
static boolean init_sscb(SPROG *sp, SSCB *ss, seqSS *seqSS);
static boolean init_chan(SPROG *sp, CHAN *ch, seqChan *seqChan);
static PVTYPE *find_type(const char *userType);

/*
 * seq: Run a state program.
 * Usage:  seq(<sp>, <macros string>, <stack size>)
 *	sp is the ptr to the state program structure.
 *	Example:  seq(&myprog, "logfile=mylog", 0)
 * When called from the shell, the 2nd & 3rd parameters are optional.
 *
 * Creates the initial state program thread and returns its thread id.
 * Most initialization is performed here.
 */
epicsShareFunc epicsThreadId epicsShareAPI seq(
	seqProgram *seqProg, const char *macroDef, unsigned stackSize)
{
	epicsThreadId	tid;
	SPROG		*sp;
	char		*str;
	const char	*threadName;
	unsigned int	smallStack;

	/* Register this program (if not yet done) */
	seqRegisterSequencerProgram(seqProg);

	/* Print version & date of sequencer */
	printf(SEQ_RELEASE "\n");

	/* Exit if no parameters specified */
	if (!seqProg)
	{
		errlogSevPrintf(errlogFatal, "seq: bad first argument seqProg (is NULL)\n");
		return 0;
	}

	/* Check for correct state program format */
	if (seqProg->magic != MAGIC)
	{
		errlogSevPrintf(errlogFatal, "seq: illegal magic number in state program.\n"
			"      - probable mismatch between SNC & SEQ versions\n"
			"      - re-compile your program?\n");
		return 0;
	}

	sp = new(SPROG);
	if (!sp)
	{
		errlogSevPrintf(errlogFatal, "seq: calloc failed\n");
		return 0;
	}

	/* Parse the macro definitions from the "program" statement */
	seqMacParse(sp, seqProg->params);

	/* Parse the macro definitions from the command line */
	seqMacParse(sp, macroDef);

	/* Initialize program struct */
	if (!init_sprog(sp, seqProg))
		return 0;

	/* Specify stack size */
	if (stackSize == 0)
		stackSize = epicsThreadGetStackSize(THREAD_STACK_SIZE);
	str = seqMacValGet(sp, "stack");
	if (str && str[0] != '\0')
	{
		sscanf(str, "%ud", &stackSize);
	}
	smallStack = epicsThreadGetStackSize(epicsThreadStackSmall);
	if (stackSize < smallStack)
		stackSize = smallStack;
	sp->stackSize = stackSize;

	/* Specify thread name */
	str = seqMacValGet(sp, "name");
	if (str && str[0] != '\0')
		threadName = str;
	else
		threadName = sp->progName;

	/* Specify PV system name (defaults to CA) */
	str = seqMacValGet(sp, "pvsys");
	if (str && str[0] != '\0')
		sp->pvSysName = str;
	else
		sp->pvSysName = "ca";

	/* Determine debug level (currently only used for PV-level debugging) */
	str = seqMacValGet(sp, "debug");
	if (str && str[0] != '\0')
		sp->debug = atoi(str);
	else
		sp->debug = 0;

	/* Specify thread priority */
	sp->threadPriority = THREAD_PRIORITY;
	str = seqMacValGet(sp, "priority");
	if (str && str[0] != '\0')
	{
		sscanf(str, "%ud", &(sp->threadPriority));
	}
	if (sp->threadPriority > THREAD_PRIORITY)
		sp->threadPriority = THREAD_PRIORITY;

	tid = epicsThreadCreate(threadName, sp->threadPriority,
		sp->stackSize, sequencer, sp);
	if (!tid)
	{
		errlogSevPrintf(errlogFatal, "seq: epicsThreadCreate failed");
		return 0;
	}

	printf("Spawning sequencer program \"%s\", thread %p: \"%s\"\n",
		sp->progName, tid, threadName);

	return tid;
}

/*
 * Copy data from seqCom.h structures into this thread's dynamic structures
 * as defined in seq.h.
 */
static boolean init_sprog(SPROG *sp, seqProgram *seqProg)
{
	unsigned nss, nch;

	/* Copy information for state program */
	sp->numSS = seqProg->numSS;
	sp->numChans = seqProg->numChans;
	sp->numEvFlags = seqProg->numEvFlags;
	sp->options = seqProg->options;
	sp->progName = seqProg->progName;
	sp->initFunc = seqProg->initFunc;
	sp->entryFunc = seqProg->entryFunc;
	sp->exitFunc = seqProg->exitFunc;
	sp->varSize = seqProg->varSize;
	sp->numQueues = seqProg->numQueues;

	/* Allocate user variable area if reentrant option (+r) is set */
	if ((sp->options & OPT_REENT) && sp->varSize > 0)
	{
		sp->var = (USER_VAR *)newArray(char, sp->varSize);
		if (!sp->var)
		{
			errlogSevPrintf(errlogFatal, "init_sprog: calloc failed\n");
			return FALSE;
		}
	}

	DEBUG("init_sprog: numSS=%d, numChans=%d, numEvFlags=%u, "
		"progName=%s, varSize=%u\n", sp->numSS, sp->numChans,
		sp->numEvFlags, sp->progName, sp->varSize);

	/* Create semaphores */
	sp->programLock = epicsMutexCreate();
	if (!sp->programLock)
	{
		errlogSevPrintf(errlogFatal, "init_sprog: epicsMutexCreate failed\n");
		return FALSE;
	}
	sp->ready = epicsEventCreate(epicsEventEmpty);
	if (!sp->ready)
	{
		errlogSevPrintf(errlogFatal, "init_sprog: epicsEventCreate failed\n");
		return FALSE;
	}
	sp->dead = epicsEventCreate(epicsEventEmpty);
	if (!sp->dead)
	{
		errlogSevPrintf(errlogFatal, "init_sprog: epicsEventCreate failed\n");
		return FALSE;
	}

	/* Allocate an array for event flag bits. Note this does
	   *not* reserve space for all event numbers (i.e. including
	   channels), only for event flags. */
	assert(NWORDS(sp->numEvFlags) > 0);
	sp->evFlags = newArray(bitMask, NWORDS(sp->numEvFlags));
	if (!sp->evFlags)
	{
		errlogSevPrintf(errlogFatal, "init_sprog: calloc failed\n");
		return FALSE;
	}
	/* NOTE: event flags count from 1 upward */
	sp->syncedChans = newArray(CHAN*, sp->numEvFlags+1);

	/* Allocate and initialize syncQ queues */
	if (sp->numQueues > 0)
	{
		sp->queues = newArray(QUEUE, sp->numQueues);
		if (!sp->queues)
		{
			errlogSevPrintf(errlogFatal, "init_sprog: calloc failed\n");
			return FALSE;
		}
	}
	/* Initial pool for pv requests is 1kB on 32-bit systems */
	freeListInitPvt(&sp->pvReqPool, 128, sizeof(PVREQ));
	if (!sp->pvReqPool)
	{
		errlogSevPrintf(errlogFatal, "init_sprog: freeListInitPvt failed\n");
		return FALSE;
	}

	/* Allocate array of state set structs and initialize it */
	if (sp->numSS > 0)
	{
		sp->ss = newArray(SSCB, sp->numSS);
		if (!sp->ss)
		{
			errlogSevPrintf(errlogFatal, "init_sprog: calloc failed\n");
			return FALSE;
		}
	}
	for (nss = 0; nss < sp->numSS; nss++)
	{
		if (!init_sscb(sp, sp->ss + nss, seqProg->ss + nss))
			return FALSE;
	}

	/* Allocate array of channel structs and initialize it */
	if (sp->numChans > 0)
	{
		sp->chan = newArray(CHAN, sp->numChans);
		if (!sp->chan)
		{
			errlogSevPrintf(errlogFatal, "init_sprog: calloc failed\n");
			return FALSE;
		}
	}
	for (nch = 0; nch < sp->numChans; nch++)
	{
		if (!init_chan(sp, sp->chan + nch, seqProg->chan + nch))
			return FALSE;
	}
	return TRUE;
}

/*
 * Initialize a state set control block
 */
static boolean init_sscb(SPROG *sp, SSCB *ss, seqSS *seqSS)
{
	unsigned nch;

	/* Fill in SSCB */
	ss->ssName = seqSS->ssName;
	ss->numStates = seqSS->numStates;
	ss->maxNumDelays = seqSS->numDelays;

	if (ss->maxNumDelays > 0)
	{
		ss->delay = newArray(double, ss->maxNumDelays);
		if (!ss->delay)
		{
			errlogSevPrintf(errlogFatal, "init_sscb: calloc failed\n");
			return FALSE;
		}
		ss->delayExpired = newArray(boolean, ss->maxNumDelays);
		if (!ss->delayExpired)
		{
			errlogSevPrintf(errlogFatal, "init_sscb: calloc failed\n");
			return FALSE;
		}
	}
	ss->currentState = 0; /* initial state */
	ss->nextState = 0;
	ss->prevState = 0;
	ss->threadId = 0;
	/* Initialize to start time rather than zero time! */
	pvTimeGetCurrentDouble(&ss->timeEntered);
	ss->sprog = sp;

	ss->syncSemId = epicsEventCreate(epicsEventEmpty);
	if (!ss->syncSemId)
	{
		errlogSevPrintf(errlogFatal, "init_sscb: epicsEventCreate failed\n");
		return FALSE;
	}

	if (sp->numChans > 0)
	{
		ss->getSemId = newArray(epicsEventId, sp->numChans);
		if (!ss->getSemId)
		{
			errlogSevPrintf(errlogFatal, "init_sscb: calloc failed\n");
			return FALSE;
		}
		ss->putSemId = newArray(epicsEventId, sp->numChans);
		if (!ss->putSemId)
		{
			errlogSevPrintf(errlogFatal, "init_sscb: calloc failed\n");
			return FALSE;
		}
	}
	for (nch = 0; nch < sp->numChans; nch++)
	{
		ss->getSemId[nch] = epicsEventCreate(epicsEventFull);
		if (!ss->getSemId[nch])
		{
			errlogSevPrintf(errlogFatal, "init_sscb: epicsEventCreate failed\n");
			return FALSE;
		}
		ss->putSemId[nch] = epicsEventCreate(epicsEventFull);
		if (!ss->putSemId[nch])
		{
			errlogSevPrintf(errlogFatal, "init_sscb: epicsEventCreate failed\n");
			return FALSE;
		}
	}
	ss->dead = epicsEventCreate(epicsEventEmpty);
	if (!ss->dead)
	{
		errlogSevPrintf(errlogFatal, "init_sscb: epicsEventCreate failed\n");
		return FALSE;
	}

	/* No need to copy the state structs, they can be shared
	   because nothing gets mutated. */
	ss->states = seqSS->states;

	/* Allocate separate user variable area if safe mode option (+s) is set */
	if (sp->options & OPT_SAFE)
	{
		if (sp->numChans > 0)
		{
			ss->dirty = newArray(boolean, sp->numChans);
			if (!ss->dirty)
			{
				errlogSevPrintf(errlogFatal, "init_sscb: calloc failed\n");
				return FALSE;
			}
		}
		if (sp->varSize > 0)
		{
			ss->var = (USER_VAR *)newArray(char, sp->varSize);
			if (!ss->var)
			{
				errlogSevPrintf(errlogFatal, "init_sscb: calloc failed\n");
				return FALSE;
			}
		}
	}
	else
	{
		ss->dirty = NULL;
		ss->var = sp->var;
	}
	return TRUE;
}

/*
 * Build the database channel structures.
 */
static boolean init_chan(SPROG *sp, CHAN *ch, seqChan *seqChan)
{
	DEBUG("init_chan: ch=%p\n", ch);
	ch->sprog = sp;
	ch->varName = seqChan->varName;
	ch->offset = seqChan->offset;
	ch->count = seqChan->count;
	if (ch->count == 0) ch->count = 1;
	ch->syncedTo = seqChan->efId;
	if (ch->syncedTo)
	{
		/* insert into syncedTo list for this event flag */
		CHAN *fst = sp->syncedChans[ch->syncedTo];
		sp->syncedChans[ch->syncedTo] = ch;
		ch->nextSynced = fst;
	}
	ch->monitored = seqChan->monitored;
	ch->eventNum = seqChan->eventNum;

	/* Fill in request type info */
	ch->type = find_type(seqChan->varType);
	if (!ch->type->size)
	{
		errlogSevPrintf(errlogFatal, "init_chan: unknown type %s for assigned variable: %s\n",
			seqChan->varType, seqChan->varName);
		return FALSE;
	}

	DEBUG("  varname=%s, count=%u\n"
		"  syncedTo=%u, monitored=%u, eventNum=%u\n",
		ch->varName, ch->count,
		ch->syncedTo, ch->monitored, ch->eventNum);
	DEBUG("  type=%p: typeStr=%s, putType=%d, getType=%d, size=%d\n",
		ch->type, ch->type->typeStr,
		ch->type->putType, ch->type->getType, ch->type->size);

	if (seqChan->chName)	/* skip anonymous PVs */
	{
		char name_buffer[100];

		seqMacEval(sp, seqChan->chName, name_buffer, sizeof(name_buffer));
		if (name_buffer[0])	/* skip anonymous PVs */
		{
			DBCHAN	*dbch = new(DBCHAN);
			if (!dbch)
			{
				errlogSevPrintf(errlogFatal, "init_chan: calloc failed\n");
				return FALSE;
			}
			dbch->dbName = epicsStrDup(name_buffer);
			if (!dbch->dbName)
			{
				errlogSevPrintf(errlogFatal, "init_chan: epicsStrDup failed\n");
				return FALSE;
			}
			if ((sp->options & OPT_SAFE) && sp->numSS > 0)
			{
				dbch->ssMetaData = newArray(PVMETA, sp->numSS);
				if (!dbch->ssMetaData)
				{
					errlogSevPrintf(errlogFatal, "init_chan: calloc failed\n");
					return FALSE;
				}
			}
			ch->dbch = dbch;
			sp->assignCount++;
			if (ch->monitored)
				sp->monitorCount++;
			DEBUG("  assigned name=%s, expanded name=%s\n",
				seqChan->chName, ch->dbch->dbName);
		}
	}

	if (!ch->dbch)
	{
		DEBUG("  pv name=<anonymous>\n");
	}

	if (seqChan->queueSize)
	{
		/* We want to store the whole pv message in the queue,
		   so that we can extract status etc when we remove
		   the message. */
		size_t size = pv_size_n(ch->type->getType, ch->count);
		QUEUE *q = sp->queues + seqChan->queueIndex;

		if (*q == NULL)
		{
			*q = seqQueueCreate(seqChan->queueSize, size);
			if (!*q)
			{
				errlogSevPrintf(errlogFatal, "init_chan: seqQueueCreate failed\n");
				return FALSE;
			}
		}
		else if (seqQueueNumElems(*q) != seqChan->queueSize ||
			 seqQueueElemSize(*q) != size)
		{
			errlogSevPrintf(errlogFatal,
				"init_chan(varname=%s): inconsistent shared queue definitions\n",
				seqChan->varName);
			return FALSE;
		}
		ch->queue = *q;
		DEBUG("  queueSize=%d, queueIndex=%d, queue=%p\n",
			seqChan->queueSize, seqChan->queueIndex, ch->queue);
		DEBUG("  queue->numElems=%d, queue->elemSize=%d\n",
			seqQueueNumElems(ch->queue), seqQueueElemSize(ch->queue));
	}
	ch->varLock = epicsMutexCreate();
	if (!ch->varLock)
	{
		errlogSevPrintf(errlogFatal, "init_chan: epicsMutexCreate failed\n");
		return FALSE;
	}
	return TRUE;
}

/*
 * find_type() -- returns types for DB put/get, element size based on user variable type.
 * Mapping is determined by the following pv_type_map[] array.
 * pvTypeTIME_* types for gets/monitors return status, severity, and time stamp
 * in addition to the value.
 */
static PVTYPE pv_type_map[] =
{
	{ "char",		pvTypeCHAR,	pvTypeTIME_CHAR,	sizeof(char)		},
	{ "short",		pvTypeSHORT,	pvTypeTIME_SHORT,	sizeof(short)		},
	{ "int",		pvTypeLONG,	pvTypeTIME_LONG,	sizeof(int)		},
	{ "long",		pvTypeLONG,	pvTypeTIME_LONG,	sizeof(long)		},
	{ "unsigned char",	pvTypeCHAR,	pvTypeTIME_CHAR,	sizeof(unsigned char)	},
	{ "unsigned short",	pvTypeSHORT,	pvTypeTIME_SHORT,	sizeof(unsigned short)	},
	{ "unsigned int",	pvTypeLONG,	pvTypeTIME_LONG,	sizeof(unsigned int)	},
	{ "unsigned long",	pvTypeLONG,	pvTypeTIME_LONG,	sizeof(unsigned long)	},
	{ "float",		pvTypeFLOAT,	pvTypeTIME_FLOAT,	sizeof(float)		},
	{ "double",		pvTypeDOUBLE,	pvTypeTIME_DOUBLE,	sizeof(double)		},
	{ "string",		pvTypeSTRING,	pvTypeTIME_STRING,	sizeof(string)		},

	{ "epicsInt8",		pvTypeCHAR,	pvTypeTIME_CHAR,	sizeof(epicsInt8)	},
	{ "epicsUInt8",		pvTypeCHAR,	pvTypeTIME_CHAR,	sizeof(epicsUInt8)	},
	{ "epicsInt16",		pvTypeSHORT,	pvTypeTIME_SHORT,	sizeof(epicsInt16)	},
	{ "epicsUInt16",	pvTypeSHORT,	pvTypeTIME_SHORT,	sizeof(epicsUInt16)	},
	{ "epicsInt32",		pvTypeLONG,	pvTypeTIME_LONG,	sizeof(epicsInt32)	},
	{ "epicsUInt32",	pvTypeLONG,	pvTypeTIME_LONG,	sizeof(epicsUInt32)	},

	{ NULL,			pvTypeERROR,	pvTypeERROR,		0			}
};

static PVTYPE *find_type(const char *userType)
{
	PVTYPE	*pt;

	/* TODO: select pvType according to sizeof int/long/etc */
	assert(sizeof(char)==1);
	assert(sizeof(unsigned char)==1);
	assert(sizeof(short)==2);
	assert(sizeof(unsigned short)==2);
	assert(sizeof(int)==4);
	assert(sizeof(unsigned int)==4);
	for (pt = pv_type_map; pt->typeStr; pt++)
		if (strcmp(userType, pt->typeStr) == 0)
			break;
	return pt;
}

/* Free all allocated memory in a program structure */
void seq_free(SPROG *sp)
{
	unsigned nss, nch, nq;

	/* Delete state sets */
	for (nss = 0; nss < sp->numSS; nss++)
	{
		SSCB *ss = sp->ss + nss;

		epicsEventDestroy(ss->syncSemId);
		for (nch = 0; nch < sp->numChans; nch++)
		{
			epicsEventDestroy(ss->getSemId[nch]);
			epicsEventDestroy(ss->putSemId[nch]);
		}
		free(ss->getSemId);
		free(ss->putSemId);

		epicsEventDestroy(ss->dead);

		free(ss->delay);
		free(ss->delayExpired);
		if (sp->options & OPT_SAFE) free(ss->dirty);
		if (sp->options & OPT_SAFE) free(ss->var);
	}

	free(sp->ss);

	/* Delete program-wide semaphores */
	epicsMutexDestroy(sp->programLock);
	epicsEventDestroy(sp->ready);

	seqMacFree(sp);

	for (nch = 0; nch < sp->numChans; nch++)
	{
		CHAN *ch = sp->chan + nch;

		if (ch->dbch)
		{
			free(ch->dbch->ssMetaData);
			free(ch->dbch->dbName);
			free(ch->dbch);
		}
	}
	free(sp->chan);

	for (nq = 0; nq < sp->numQueues; nq++)
		seqQueueDestroy(sp->queues[nq]);
	free(sp->queues);

	free(sp->evFlags);
	free(sp->syncedChans);
	if (sp->options & OPT_REENT) free(sp->var);
	free(sp);
}
