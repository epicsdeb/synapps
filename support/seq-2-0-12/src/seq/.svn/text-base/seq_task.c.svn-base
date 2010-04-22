/**************************************************************************
			GTA PROJECT   AT division
		Copyright, 1990-1994
		The Regents of the University of California and
		the University of Chicago.
		Los Alamos National Laboratory
	seq_task.c,v 1.3 1995/10/19 16:30:18 wright Exp

	DESCRIPTION: Seq_task.c: Thread creation and control for sequencer
	state-sets.

	ENVIRONMENT: VxWorks
	HISTORY:

04dec91,ajk	Implemented linked list of state programs, eliminating task
		variables.
11dec91,ajk	Made cosmetic changes and cleaned up comments.
19dec91,ajk	Changed algoritm in seq_getTimeout().
29apr92,ajk	Implemented new event flag mode.
30apr92,ajk	Periodically call ca_pend_event() to detect connection failures.
21may92,ajk	In sprog_delete() wait for logging semaphore before suspending
		tasks. Some minor changes in the way semaphores are deleted.
18feb92,ajk	Changed to allow sharing of single CA task by all state
		programs. Added seqAuxTask() and removed ca_pend_event() from
		ss_entry().
09aug93,ajk	Added calls to taskwdInsert() & taskwdRemove().
24nov93,ajk	Added support for assigning array elements to db channels.
24nov93,ajk	Changed implementation of event bits to support unlimited
		channels.
20may94,ajk	Changed sprog_delete() to spawn a separate cleanup task.
19oct95,ajk/rmw Fixed bug which kept events from being cleared in old eventflag
		mode.
20jul95,ajk	Add user-specified task priority to taskSpwan().
02may99,wfl	Replaced VxWorks dependencies with OSI.
17may99,wfl	Changed interface to ss_entry() for single argument; used new
		SEQFUNCPTR etc.
?????96,joh 	Fixed problem with delay calculations.
07sep99,wfl	Added destroy of put completion semaphore.
22sep99,grw     Supported entry and exit actions; supported state options.
15feb00,wfl	Fixed problem (introduced by wfl) with sequencer deletion.
18feb00,wfl	Used struct for cleanup thread args (so can use under Unix);
		ditto for aux thread args (so can pass debug flag)
29feb00,wfl	Supported new OSI (and errlogPrintf); new use of DEBUG macro;
		implemented new sequencer deletion method.
31mar00,wfl	Changed caSemId to be a mutex.
***************************************************************************/
#define	DEBUG nothing /* "nothing", "printf", "errlogPrintf" etc. */

#include	<limits.h>
#include 	<string.h>
/*#include 	<unistd.h> */

#define		DECLARE_PV_SYS
#define epicsExportSharedSymbols
#include	"seq.h"

/* Used to disable debug output */
void nothing(const char *format,...) {}

/* Function declarations */
LOCAL	long seq_waitConnect(SPROG *pSP, SSCB *pSS);
LOCAL	void ss_thread_init(SPROG *, SSCB *);
LOCAL	void ss_thread_uninit(SPROG *, SSCB *,int);
LOCAL	void seq_clearDelay(SSCB *,STATE *);
LOCAL	double seq_getTimeout(SSCB *);
epicsStatus seqAddProg(SPROG *pSP);
long seq_connect(SPROG *pSP);
long seq_disconnect(SPROG *pSP);

/*
 * sequencer() - Sequencer main thread entry point.
 */
long sequencer (pSP)
SPROG		*pSP;	/* ptr to original (global) state program table */
{
	SSCB		*pSS;
	int		nss;
	epicsThreadId	tid;
	size_t		threadLen;
	char		threadName[THREAD_NAME_SIZE+10];
	extern		void ss_entry();

	/* Retrieve info about this thread */
	pSP->threadId = epicsThreadGetIdSelf();
	epicsThreadGetName(pSP->threadId, threadName, sizeof(threadName));
	pSS = pSP->pSS;
	pSS->threadId = pSP->threadId;

	/* Add the program to the state program list */
	seqAddProg(pSP);

	/* Attach to PV context of pvSys creator (auxiliary thread) */
	pvSysAttach(pvSys);

	/* Initiate connect & monitor requests to database channels */
	seq_connect(pSP);

	/* Call sequencer entry function */
	pSP->entryFunc((SS_ID)pSS, pSP->pVar);

	/* Create each additional state-set task (additional state-set thread
	   names are derived from the first ss) */
	threadLen = strlen(threadName);
	for (nss = 1, pSS = pSP->pSS + 1; nss < pSP->numSS; nss++, pSS++)
	{
		/* Form thread name from program name + state-set number */
		sprintf(threadName+threadLen, "_%d", nss);

		/* Spawn the task */
		tid = epicsThreadCreate(
			threadName,			/* thread name */
			pSP->threadPriority,		/* priority */
			pSP->stackSize,			/* stack size */
			(EPICSTHREADFUNC)ss_entry,		/* entry point */
			pSS);				/* parameter */

		errlogPrintf("Spawning thread %p: \"%s\"\n", tid,
			    threadName);
	}

	/* First state-set jumps directly to entry point */
	ss_entry(pSP->pSS);

	return 0;
}
/*
 * ss_entry() - Thread entry point for all state-sets.
 * Provides the main loop for state-set processing.
 */
void ss_entry(pSS)
SSCB	*pSS;
{
	SPROG		*pSP = pSS->sprog;
	epicsBoolean	ev_trig;
	STATE		*pST, *pStNext;
	double		delay;
	char		*pVar;
	int		nWords;
	SS_ID		ssId;

	/* Initialize this state-set thread */
	ss_thread_init(pSP, pSS);
	
	/* If "+c" option, wait for all channels to connect (a failure
	 * return means that we have been asked to exit) */
	if ((pSP->options & OPT_CONN) != 0)
		if (seq_waitConnect(pSP, pSS) < 0) goto exit;

	/* Initialize state-set to enter the first state */
	pST = pSS->pStates;
	pSS->currentState = 0;
	pSS->nextState = -1;
	pSS->prevState = -1;

	/* Use the event mask for this state */
	pSS->pMask = (pST->pEventMask);
	nWords = (pSP->numEvents + NBITS - 1) / NBITS;

	/* Local ptr to user variables (for reentrant code only) */
	pVar = pSP->pVar;

	/* state-set id */
	ssId = (SS_ID)pSS;

	/*
	 * ============= Main loop ==============
	 */
	while (TRUE)
	{
		/* If we've changed state, do any entry actions. Also do these
                 * even if it's the same state if option to do so is enabled. 
                 */
		if ( pSS->prevState != pSS->currentState ||
                     pST->options & OPT_DOENTRYFROMSELF )
	  	         if ( pST->entryFunc ) pST->entryFunc( ssId, pVar );

		seq_clearDelay(pSS, pST); /* Clear delay list */
		pST->delayFunc(ssId, pVar); /* Set up new delay list */

		/* Setting this semaphore here guarantees that a when() is
		 * always executed at least once when a state is first
		 * entered.
		 */
		epicsEventSignal(pSS->syncSemId);

		/* Loop until an event is triggered, i.e. when() returns TRUE
		 */
		do {
			/* Wake up on PV event, event flag, or expired delay */
			delay = seq_getTimeout(pSS); /* min. delay from list */
			if (delay > 0.0)
				(void) epicsEventWaitWithTimeout(pSS->syncSemId,
							    delay);

			/* Check whether we have been asked to exit */
			if (epicsEventTryWait(pSS->death1SemId) ==
							epicsEventWaitOK) goto exit;

			/* Call the event function to check for an event
			 * trigger. The statement inside the when() statement
			 * is executed. Note, we lock out PV events while doing 
			 * this. */
			epicsMutexMustLock(pSP->caSemId);

			ev_trig = pST->eventFunc(ssId, pVar,
			 &pSS->transNum, &pSS->nextState); /* check events */

		        /* Clear all event flags (old ef mode only) */
			if ( ev_trig && ((pSP->options & OPT_NEWEF) == 0) )
			{
			    register int i;

			    for (i = 0; i < nWords; i++)
			    	pSP->pEvents[i] =
					pSP->pEvents[i] & !pSS->pMask[i];
			}

			epicsMutexUnlock(pSP->caSemId);

		} while (!ev_trig);

		/* An event triggered:
		 * execute the action statements and enter the new state. */

		/* Change event mask ptr for next state */
		pStNext = pSS->pStates + pSS->nextState;
		pSS->pMask = (pStNext->pEventMask);

		/* Execute the action for this event */
		pST->actionFunc(ssId, pVar, pSS->transNum);

		/* If changing state, do any exit actions. */
		if ( pSS->currentState != pSS->nextState ||
                     pST->options & OPT_DOEXITTOSELF )
		         if ( pST->exitFunc ) pST->exitFunc( ssId, pVar );

		/* Flush any outstanding DB requests */
		pvSysFlush( pvSys );

		/* Change to next state */
		pSS->prevState = pSS->currentState;
		pSS->currentState = pSS->nextState;
		pST = pSS->pStates + pSS->currentState;
	}

	/* Thread exit has been requested */
exit:

	/* Uninitialize this state-set thread (phase 1) */
	ss_thread_uninit(pSP, pSS, 1);

	/* Pass control back (so all state-set threads can complete phase 1
	 * before embarking on phase 2) */
	epicsEventSignal(pSS->death2SemId);

	/* Wait for request to perform uninitialization (phase 2) */
	epicsEventMustWait(pSS->death3SemId);
	ss_thread_uninit(pSP, pSS, 2);

	/* Pass control back and die (i.e. exit) */
	epicsEventSignal(pSS->death4SemId);
}
/* Initialize a state-set thread */
LOCAL void ss_thread_init(pSP, pSS)
SPROG	*pSP;
SSCB	*pSS;
{
	/* Get this thread's id */
	pSS->threadId = epicsThreadGetIdSelf();

	/* Attach to PV context of pvSys creator (auxiliary thread); was
	   already done for the first state-set */
	if (pSP->threadId != pSS->threadId)
	    pvSysAttach(pvSys);

	/* Register this thread with the EPICS watchdog (no callback func) */
	taskwdInsert(pSS->threadId, (SEQVOIDFUNCPTR)0, (void *)0);

	return;
}

/* Uninitialize a state-set thread */
LOCAL void ss_thread_uninit(pSP, pSS, phase)
SPROG   *pSP;
SSCB    *pSS;
int	phase;
{
	/* Phase 1: if this is the first state-set, call user exit routine
	   and disconnect all channels */
	if (phase == 1 && pSS->threadId == pSP->threadId)
	{
	    DEBUG("   Call exit function\n");
	    pSP->exitFunc((SS_ID)pSS, pSP->pVar);

	    DEBUG("   Disconnect all channels\n");
	    seq_disconnect(pSP);
	}

	/* Phase 2: unregister the thread with the EPICS watchdog */
	else if (phase == 2)
	{
	    DEBUG("   taskwdRemove(%p)\n", pSS->threadId);
	    taskwdRemove(pSS->threadId);
	}

	return;
}
/* Wait for all channels to connect */
LOCAL long seq_waitConnect(SPROG *pSP, SSCB *pSS)
{
	epicsStatus	status;
	double		delay;

	delay = 10.0; /* 10, 20, 30, 40, 40,... sec */
	while (1) {
		status = epicsEventWaitWithTimeout(
                    pSS->allFirstConnectAndMonitorSemId, delay);
		if(status==OK) break;
		if (delay < 40.0) {
			delay += 10.0;
			errlogPrintf("numMonitoredChans %ld firstMonitorCount %ld",
				pSP->numMonitoredChans,pSP->firstMonitorCount);
			errlogPrintf(" assignCount %ld firstConnectCount %ld\n",
				pSP->assignCount,pSP->firstConnectCount);
		}
		/* Check whether we have been asked to exit */
		if (epicsEventTryWait(pSS->death1SemId) == epicsEventWaitOK)
			return ERROR;
	}
	return OK;
}
/*
 * seq_clearDelay() - clear the time delay list.
 */
LOCAL void seq_clearDelay(pSS,pST)
SSCB		*pSS;
STATE           *pST;
{
	int		ndelay;


        /* On state change set time we entered this state; or if transition from
         * same state if option to do so is on for this state. 
         */
	if ( (pSS->currentState != pSS->prevState) ||
             !(pST->options & OPT_NORESETTIMERS) )
	{
		pvTimeGetCurrentDouble(&pSS->timeEntered);
	}

	for (ndelay = 0; ndelay < MAX_NDELAY; ndelay++)
	{
		pSS->delay[ndelay] = 0;
	 	pSS->delayExpired[ndelay] = FALSE;
	}

	pSS->numDelays = 0;

	return;
}

/*
 * seq_getTimeout() - return time-out for pending on events.
 * Returns number of seconds to next expected timeout of a delay() call.
 * Returns (double) INT_MAX if no delays pending 
 */
LOCAL double seq_getTimeout(pSS)
SSCB		*pSS;
{
	int	ndelay, delayMinInit;
	double	cur, delay, delayMin, delayN;

	if (pSS->numDelays == 0)
		return (double) INT_MAX;

	/*
	 * calculate the delay since this state was entered
	 */
	pvTimeGetCurrentDouble(&cur);
	delay = cur - pSS->timeEntered;

	delayMinInit = 0;
	delayMin = (double) INT_MAX;

	/* Find the minimum  delay among all non-expired timeouts */
	for (ndelay = 0; ndelay < pSS->numDelays; ndelay++)
	{
		if (pSS->delayExpired[ndelay])
			continue; /* skip if this delay entry already expired */

		delayN = pSS->delay[ndelay];
		if (delay >= delayN)
		{	/* just expired */
			pSS->delayExpired[ndelay] = TRUE; /* mark as expired */
			return 0.0;
		}

		if (delayN<=delayMin) {
			delayMinInit=1;
			delayMin = delayN;  /* this is the min. delay so far */
		}
	}

	/*
	 * If there is no unexpired delay in the list
	 * then wait forever until there is a PV state
	 * change
	 */
	if (!delayMinInit) {
		return (double) INT_MAX;
	}

	/*
	 * unexpired delay _is_ in the list
	 */
	if (delayMin>delay) {
		delay = delayMin - delay;
		return (double) delay;
	}
	else {
		return 0.0;
	}
}

/*
 * Delete all state-set threads and do general clean-up.
 */
long epicsShareAPI seqStop(epicsThreadId tid)
{
	SPROG		*pSP;
	SSCB		*pSS;
	int		nss;
	extern epicsStatus seqDelProg(SPROG *pSP);

	/* Check that this is indeed a state program thread */
	pSP = seqFindProg(tid);
	if (pSP == NULL)
		return -1;

	DEBUG("Stop %s: pSP=%p, tid=%p\n", pSP->pProgName, pSP,tid);

	/* Ask all state-set threads to exit (phase 1) */
	DEBUG("   Asking state-set threads to exit (phase 1):\n");
	for (nss = 0, pSS = pSP->pSS; nss < pSP->numSS; nss++, pSS++)
	{
		/* Just possibly hasn't started yet, so check... */
		if (pSS->threadId == 0)
			continue;

		/* Ask the thread to exit */
		DEBUG("      tid=%p\n", pSS->threadId);
		epicsEventSignal(pSS->death1SemId);
	}

	/* Wake up all state-sets */
	DEBUG("   Waking up all state-sets\n");
	seqWakeup (pSP, 0);

	/* Wait for them all to complete phase 1 of their deaths */
	DEBUG("   Waiting for state-set threads phase 1 death:\n");
	for (nss = 0, pSS = pSP->pSS; nss < pSP->numSS; nss++, pSS++)
	{
		if (pSS->threadId == 0)
			continue;

		if (epicsEventWaitWithTimeout(pSS->death2SemId,10.0) != epicsEventWaitOK)
		{
			errlogPrintf("Timeout waiting for thread %p "
				     "(\"%s\") death phase 1 (ignored)\n",
				     pSS->threadId, pSS->pSSName);
		}
		else
		{
			DEBUG("      tid=%p\n", pSS->threadId);
		}
	}

	/* Ask all state-set threads to exit (phase 2) */
	DEBUG("   Asking state-set threads to exit (phase 2):\n");
	for (nss = 0, pSS = pSP->pSS; nss < pSP->numSS; nss++, pSS++)
	{
		if (pSS->threadId == 0)
			continue;

		DEBUG("      tid=%p\n", pSS->threadId);
		epicsEventSignal(pSS->death3SemId);
	}

	/* Wait for them all to complete phase 2 of their deaths */
	DEBUG("   Waiting for state-set threads phase 2 death:\n");
	for (nss = 0, pSS = pSP->pSS; nss < pSP->numSS; nss++, pSS++)
	{
		if (pSS->threadId == 0)
			continue;

		if (epicsEventWaitWithTimeout(pSS->death4SemId,10.0) != epicsEventWaitOK)
		{
			errlogPrintf("Timeout waiting for thread %p "
				     "(\"%s\") death phase 2 (ignored)\n",
				     pSS->threadId, pSS->pSSName);
		}
		else
		{
			DEBUG("      tid=%p\n", pSS->threadId);
		}
	}

	/* Close the log file */
	if (pSP->logFd != NULL)
	{
		DEBUG("Log file closed, fd=%d, file=%s\n", fileno(pSP->logFd),
		      pSP->pLogFile);
		fclose(pSP->logFd);
		pSP->logFd = NULL;
		pSP->pLogFile = "";
	}

	/* Remove the state program from the state program list */
	seqDelProg(pSP);

	/* Delete state-set semaphores */
	for (nss = 0, pSS = pSP->pSS; nss < pSP->numSS; nss++, pSS++)
	{
		if (pSS->allFirstConnectAndMonitorSemId != NULL)
			epicsEventDestroy(pSS->allFirstConnectAndMonitorSemId);
		if (pSS->syncSemId != NULL)
			epicsEventDestroy(pSS->syncSemId);
		if (pSS->getSemId != NULL)
			epicsEventDestroy(pSS->getSemId);
		if (pSS->putSemId != NULL)
			epicsEventDestroy(pSS->putSemId);
		if (pSS->death1SemId != NULL)
			epicsEventDestroy(pSS->death1SemId);
		if (pSS->death2SemId != NULL)
			epicsEventDestroy(pSS->death2SemId);
		if (pSS->death3SemId != NULL)
			epicsEventDestroy(pSS->death3SemId);
		if (pSS->death4SemId != NULL)
			epicsEventDestroy(pSS->death4SemId);
	}

	/* Delete program-wide semaphores */
	epicsMutexDestroy(pSP->caSemId);
	epicsMutexDestroy(pSP->logSemId);

	/* Free all allocated memory */
	seqFree(pSP);

	DEBUG("   Done\n");
	return 0;
}
/* seqFree()--free all allocated memory */
void seqFree(pSP)
SPROG		*pSP;
{
	SSCB		*pSS;
	CHAN		*pDB;
	MACRO		*pMac;
	int		n;

	/* Free macro table entries */
	for (pMac = pSP->pMacros, n = 0; n < MAX_MACROS; pMac++, n++)
	{
		if (pMac->pName != NULL)
			free(pMac->pName);
		if (pMac->pValue != NULL)
			free(pMac->pValue);
	}

	/* Free MACRO table */
	free(pSP->pMacros);

	/* Free channel names */
	for (pDB = pSP->pChan, n = 0; n < pSP->numChans; pDB++, n++)
	{
		if (pDB->dbName != NULL)
			free(pDB->dbName);
	}

	/* Free channel structures */
	free(pSP->pChan);

	/* Free STATE blocks */
	pSS = pSP->pSS;
	free(pSS->pStates);

	/* Free event words */
	free(pSP->pEvents);

	/* Free SSCBs */
	free(pSP->pSS);

	/* Free SPROG */
	free(pSP);
}

/* 
 * Sequencer auxiliary thread -- loops on pvSysPend().
 */
void *seqAuxThread(void *tArgs)
{
	AUXARGS		*pArgs = (AUXARGS *)tArgs;
	char		*pPvSysName = pArgs->pPvSysName;
	long		debug = pArgs->debug;
	int		status;
	extern		epicsThreadId seqAuxThreadId;

	/* Register this thread with the EPICS watchdog */
	taskwdInsert(epicsThreadGetIdSelf(),(SEQVOIDFUNCPTR)0, (void *)0);

	/* All state program threads will use a common PV context (subtract
	   1 from debug level for PV debugging) */
	status = pvSysCreate(pPvSysName, debug>0?debug-1:0, &pvSys);
        if (status != pvStatOK)
        {
                errlogPrintf("seqAuxThread: pvSysCreate() %s failure: %s\n",
                            pPvSysName, pvSysGetMess(pvSys));
		seqAuxThreadId = (epicsThreadId) -1;
                return NULL;
        }
	seqAuxThreadId = epicsThreadGetIdSelf(); /* AFTER pvSysCreate() */

	/* This loop allows for check for connect/disconnect on PVs */
	for (;;)
	{
		pvSysPend(pvSys, 10.0, TRUE); /* returns every 10 sec. */
	}

	/* Return no result (never exit in any case) */
	return NULL;
}

