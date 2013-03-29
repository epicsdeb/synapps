/*************************************************************************\
Copyright (c) 1990-1994 The Regents of the University of California
                        and the University of Chicago.
                        Los Alamos National Laboratory
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
/*************************************************************************\
            Thread creation and control for sequencer state sets
\*************************************************************************/
#include "seq.h"
#include "seq_debug.h"

static void ss_entry(void *arg);
static void clearDelays(SSCB *,STATE *);
static boolean calcTimeout(SSCB *, double *);

/*
 * sequencer() - Sequencer main thread entry point.
 */
void sequencer (void *arg)	/* ptr to original (global) state program table */
{
	SPROG		*sp = (SPROG *)arg;
	unsigned	nss;
	size_t		threadLen;
	char		threadName[THREAD_NAME_SIZE+10];

	/* Get this thread's id */
	sp->ss->threadId = epicsThreadGetIdSelf();

	/* Add the program to the state program list
	   and if necessary create pvSys */
	seqAddProg(sp);

	if (!sp->pvSys)
	{
		sp->die = TRUE;
		goto exit;
	}

	/* Note that the program init function
	   gets the global var buffer sp->var passed,
	   not the state set local one, even in safe mode. */

	/* Call sequencer init function to initialize variables. */
	sp->initFunc(sp->var);

	/* Initialize state set variables. In safe mode, copy variable
	   block to state set buffers. Must do all this before connecting. */
	for (nss = 0; nss < sp->numSS; nss++)
	{
		SSCB	*ss = sp->ss + nss;

		if (sp->options & OPT_SAFE)
			memcpy(ss->var, sp->var, sp->varSize);
	}

	/* Attach to PV system */
	pvSysAttach(sp->pvSys);

	/* Initiate connect & monitor requests to database channels, waiting
	   for all connections to be established if the option is set. */
	if (seq_connect(sp, ((sp->options & OPT_CONN) != 0)) != pvStatOK)
		goto exit;

	/* Emulate the 'first monitor event' for anonymous PVs */
	if ((sp->options & OPT_SAFE) != 0)
	{
		unsigned nch;
		for (nch=0; nch<sp->numChans; nch++)
			if (sp->chan[nch].syncedTo && !sp->chan[nch].dbch)
				seq_efSet(sp->ss, sp->chan[nch].syncedTo);
	}

	/* Call program entry function if defined.
	   Treat as if called from 1st state set. */
	if (sp->entryFunc) sp->entryFunc(sp->ss, sp->ss->var);

	/* Create each additional state set task (additional state set thread
	   names are derived from the first ss) */
	epicsThreadGetName(sp->ss->threadId, threadName, sizeof(threadName));
	threadLen = strlen(threadName);
	for (nss = 1; nss < sp->numSS; nss++)
	{
		SSCB		*ss = sp->ss + nss;
		epicsThreadId	tid;

		/* Form thread name from program name + state set number */
		sprintf(threadName+threadLen, "_%d", nss);

		/* Spawn the task */
		tid = epicsThreadCreate(
			threadName,			/* thread name */
			sp->threadPriority,		/* priority */
			sp->stackSize,			/* stack size */
			ss_entry,			/* entry point */
			ss);				/* parameter */

		DEBUG("Spawning additional state set thread %p: \"%s\"\n", tid, threadName);
	}

	/* First state set jumps directly to entry point */
	ss_entry(sp->ss);

	DEBUG("   Wait for other state sets to exit\n");
	for (nss = 1; nss < sp->numSS; nss++)
	{
		SSCB *ss = sp->ss + nss;
		epicsEventMustWait(ss->dead);
	}

	/* Call program exit function if defined.
	   Treat as if called from 1st state set. */
	if (sp->exitFunc) sp->exitFunc(sp->ss, sp->ss->var);

exit:
	DEBUG("   Disconnect all channels\n");
	seq_disconnect(sp);
	DEBUG("   Remove program instance from list\n");
	seqDelProg(sp);

	printf("Instance %d of sequencer program \"%s\" terminated\n",
		sp->instance, sp->progName);

	/* Free all allocated memory */
	seq_free(sp);
}

/*
 * ss_read_buffer_static() - static version of ss_read_buffer.
 * This is to enable inlining in the for loop in ss_read_all_buffer.
 */
static void ss_read_buffer_static(SSCB *ss, CHAN *ch, boolean dirty_only)
{
	char *val = valPtr(ch,ss);
	char *buf = bufPtr(ch);
	ptrdiff_t nch = chNum(ch);
	/* Must take dbCount for db channels, else we overwrite
	   elements we didn't get */
	size_t count = ch->dbch ? ch->dbch->dbCount : ch->count;
	size_t var_size = ch->type->size * count;

	if (!ss->dirty[nch] && dirty_only)
		return;

	epicsMutexMustLock(ch->varLock);

	DEBUG("ss %s: before read %s", ss->ssName, ch->varName);
	print_channel_value(DEBUG, ch, val);

	memcpy(val, buf, var_size);
	if (ch->dbch) {
		/* structure copy */
		ch->dbch->ssMetaData[ssNum(ss)] = ch->dbch->metaData;
	}

	DEBUG("ss %s: after read %s", ss->ssName, ch->varName);
	print_channel_value(DEBUG, ch, val);

	ss->dirty[nch] = FALSE;

	epicsMutexUnlock(ch->varLock);
}

/*
 * ss_read_buffer() - Copy value and meta data
 * from shared buffer to state set local buffer
 * and reset corresponding dirty flag. Do this
 * only if dirty flag is set or dirty_only is FALSE.
 */
void ss_read_buffer(SSCB *ss, CHAN *ch, boolean dirty_only)
{
	ss_read_buffer_static(ss, ch, dirty_only);
}

/*
 * ss_read_all_buffer() - Call ss_read_buffer_static
 * for all channels.
 */
static void ss_read_all_buffer(SPROG *sp, SSCB *ss)
{
	unsigned nch;

	for (nch = 0; nch < sp->numChans; nch++)
	{
		CHAN *ch = sp->chan + nch;
		/* Call static version so it gets inlined */
		ss_read_buffer_static(ss, ch, TRUE);
	}
}

/*
 * ss_read_all_buffer_selective() - Call ss_read_buffer_static
 * for all channels that are sync'ed to the given event flag.
 * NOTE: calling code must take sp->programLock, as we traverse
 * the list of channels synced to this event flag.
 */
void ss_read_buffer_selective(SPROG *sp, SSCB *ss, EV_ID ev_flag)
{
	CHAN *ch = sp->syncedChans[ev_flag];
	while (ch)
	{
		/* Call static version so it gets inlined */
		ss_read_buffer_static(ss, ch, TRUE);
		ch = ch->nextSynced;
	}
}

/*
 * ss_write_buffer() - Copy given value and meta data
 * to shared buffer. In safe mode, if dirtify is TRUE then
 * set dirty flag for each state set.
 */
void ss_write_buffer(CHAN *ch, void *val, PVMETA *meta, boolean dirtify)
{
	SPROG *sp = ch->sprog;
	char *buf = bufPtr(ch);		/* shared buffer */
	/* Must use dbCount for db channels, else we overwrite
	   elements we didn't get */
	size_t count = ch->dbch ? ch->dbch->dbCount : ch->count;
	size_t var_size = ch->type->size * count;
	ptrdiff_t nch = chNum(ch);
	unsigned nss;

	epicsMutexMustLock(ch->varLock);

	DEBUG("ss_write_buffer: before write %s", ch->varName);
	print_channel_value(DEBUG, ch, buf);

	memcpy(buf, val, var_size);
	if (ch->dbch && meta)
		/* structure copy */
		ch->dbch->metaData = *meta;

	DEBUG("ss_write_buffer: after write %s", ch->varName);
	print_channel_value(DEBUG, ch, buf);

	if ((sp->options & OPT_SAFE) && dirtify)
		for (nss = 0; nss < sp->numSS; nss++)
			sp->ss[nss].dirty[nch] = TRUE;

	epicsMutexUnlock(ch->varLock);
}

/*
 * ss_entry() - Thread entry point for all state sets.
 * Provides the main loop for state set processing.
 */
static void ss_entry(void *arg)
{
	SSCB		*ss = (SSCB *)arg;
	SPROG		*sp = ss->sprog;
	USER_VAR	*var;

	if (sp->options & OPT_SAFE)
		var = ss->var;
	else
		var = sp->var;

	/* Attach to PV system; was already done for the first state set */
	if (ss != sp->ss)
	{
		ss->threadId = epicsThreadGetIdSelf();
		pvSysAttach(sp->pvSys);
	}

	/* Register this thread with the EPICS watchdog (no callback func) */
	taskwdInsert(ss->threadId, 0, 0);

	/* In safe mode, update local var buffer with global one before
	   entering the event loop. Must do this using
	   ss_read_all_buffer since CA and other state sets could
	   already post events resp. pvPut. */
	if (sp->options & OPT_SAFE)
		ss_read_all_buffer(sp, ss);

	/* Initial state is the first one */
	ss->currentState = 0;
	ss->nextState = -1;
	ss->prevState = -1;

	DEBUG("ss %s: entering main loop\n", ss->ssName);

	/*
	 * ============= Main loop ==============
	 */
	while (TRUE)
	{
		boolean	ev_trig;
		int	transNum = 0;	/* highest prio trans. # triggered */
		STATE	*st = ss->states + ss->currentState;

		/* Set state to current state */
		assert(ss->currentState >= 0);

		/* Set state set event mask to this state's event mask */
		ss->mask = st->eventMask;

		/* If we've changed state, do any entry actions. Also do these
		 * even if it's the same state if option to do so is enabled.
		 */
		if (st->entryFunc && (ss->prevState != ss->currentState
			|| (st->options & OPT_DOENTRYFROMSELF)))
		{
			st->entryFunc(ss, var);
		}

		/* Flush any outstanding DB requests */
		pvSysFlush(sp->pvSys);

		clearDelays(ss, st); /* Clear delay list */
		st->delayFunc(ss, var); /* Set up new delay list */

		/* Setting this semaphore here guarantees that a when() is
		 * always executed at least once when a state is first
		 * entered.
		 */
		epicsEventSignal(ss->syncSemId);

		/* Loop until an event is triggered, i.e. when() returns TRUE
		 */
		do {
			double delay = 0.0;

			/* Wake up on PV event, event flag, or expired delay */
			if (calcTimeout(ss, &delay))
			{
				DEBUG("before epicsEventWaitWithTimeout(ss=%d,delay=%f)\n",
					ss - sp->ss, delay);
				epicsEventWaitWithTimeout(ss->syncSemId, delay);
				DEBUG("after epicsEventWaitWithTimeout()\n");
			}
			else
			{
				DEBUG("before epicsEventWait\n");
				epicsEventWait(ss->syncSemId);
				DEBUG("after epicsEventWait\n");
			}

			/* Check whether we have been asked to exit */
			if (sp->die) goto exit;

			/* Copy dirty variable values from CA buffer
			 * to user (safe mode only).
			 */
			if (sp->options & OPT_SAFE)
				ss_read_all_buffer(sp, ss);

			/* Check state change conditions */
			ev_trig = st->eventFunc(ss, var,
				&transNum, &ss->nextState);

			/* Clear all event flags (old ef mode only) */
			if (ev_trig && !(sp->options & OPT_NEWEF))
			{
				unsigned i;
				for (i = 0; i < NWORDS(sp->numEvFlags); i++)
				{
					sp->evFlags[i] &= ~ss->mask[i];
				}
			}
		} while (!ev_trig);

		/* Execute the state change action */
		st->actionFunc(ss, var, transNum, &ss->nextState);

		/* Check whether we have been asked to exit */
		if (sp->die) goto exit;

		/* If changing state, do exit actions */
		if (st->exitFunc && (ss->currentState != ss->nextState
			|| (st->options & OPT_DOEXITTOSELF)))
		{
			st->exitFunc(ss, var);
		}

		/* Change to next state */
		ss->prevState = ss->currentState;
		ss->currentState = ss->nextState;
	}

	/* Thread exit has been requested */
exit:
	taskwdRemove(ss->threadId);
	/* Declare ourselves dead */
	if (ss != sp->ss)
		epicsEventSignal(ss->dead);
}

/*
 * clearDelays() - clear the time delay list.
 */
static void clearDelays(SSCB *ss, STATE *st)
{
	unsigned ndelay;

	/* On state change set time we entered this state; or if transition from
	 * same state if option to do so is on for this state.
	 */
	if ((ss->currentState != ss->prevState) ||
		!(st->options & OPT_NORESETTIMERS))
	{
		pvTimeGetCurrentDouble(&ss->timeEntered);
	}

	for (ndelay = 0; ndelay < ss->maxNumDelays; ndelay++)
	{
		ss->delay[ndelay] = 0;
	 	ss->delayExpired[ndelay] = FALSE;
	}

	ss->numDelays = 0;
}

/*
 * calcTimeout() - calculate the time-out for pending on events
 * Return whether to time out when waiting for events.
 * If yes, set *pdelay to the timout (in seconds).
 */
static boolean calcTimeout(SSCB *ss, double *pdelay)
{
	unsigned ndelay;
	boolean	do_timeout = FALSE;
	double	now, timeElapsed;
	double	delayMin = 0;
	/* not really necessary to initialize delayMin,
	   but tell that to the compiler...
	 */

	if (ss->numDelays == 0)
		return FALSE;

	/*
	 * Calculate the timeElapsed since this state was entered.
	 */
	pvTimeGetCurrentDouble(&now);
	timeElapsed = now - ss->timeEntered;
	DEBUG("calcTimeout: now=%f, timeElapsed=%f\n", now, timeElapsed);

	/*
	 * Find the minimum delay among all unexpired timeouts if
	 * one exists, and set do_timeout in this case.
	 */
	for (ndelay = 0; ndelay < ss->numDelays; ndelay++)
	{
		double delayN = ss->delay[ndelay];

		if (ss->delayExpired[ndelay])
			continue; /* skip if this delay entry already expired */
		if (timeElapsed >= delayN)
		{	/* just expired */
			ss->delayExpired[ndelay] = TRUE; /* mark as expired */
			*pdelay = 0.0;
			DEBUG("calcTimeout: %d expired\n", ndelay);
			return TRUE;
		}
		if (!do_timeout || delayN<=delayMin)
		{
			do_timeout = TRUE;
			delayMin = delayN;  /* this is the min. delay so far */
		}
	}
	if (do_timeout)
	{
		*pdelay = max(0.0, delayMin - timeElapsed);
	}
	DEBUG("calcTimeout: do_timeout=%s, *pdelay=%f\n",
		do_timeout?"yes":"no", *pdelay);
	return do_timeout;
}

/*
 * Delete all state set threads and do general clean-up.
 */
void epicsShareAPI seqStop(epicsThreadId tid)
{
	SPROG	*sp;

	/* Check that this is indeed a state program thread */
	sp = seqFindProg(tid);
	if (sp == NULL)
		return;
	seq_exit(sp->ss);
}

void seqCreatePvSys(SPROG *sp)
{
	int debug = sp->debug;
	pvStat status = pvSysCreate(sp->pvSysName,
		max(0, debug-1), &sp->pvSys);
	if (status != pvStatOK)
		errlogSevPrintf(errlogFatal, "pvSysCreate(\"%s\") failure\n", sp->pvSysName);
}

/*
 * seqWakeup() -- wake up each state set that is waiting on this event
 * based on the current event mask; eventNum = 0 means wake all state sets.
 */
void seqWakeup(SPROG *sp, unsigned eventNum)
{
	unsigned nss;

	/* Check event number against mask for all state sets: */
	for (nss = 0; nss < sp->numSS; nss++)
	{
		SSCB *ss = sp->ss + nss;

		epicsMutexMustLock(sp->programLock);
		/* If event bit in mask is set, wake that state set */
		DEBUG("seqWakeup: eventNum=%d, mask=%u, state set=%d\n", eventNum, 
			ss->mask? *ss->mask : 0, (int)ssNum(ss));
		if (eventNum == 0 || 
			(ss->mask && bitTest(ss->mask, eventNum)))
		{
			DEBUG("seqWakeup: waking up state set=%d\n", (int)ssNum(ss));
			epicsEventSignal(ss->syncSemId); /* wake up ss thread */
		}
		epicsMutexUnlock(sp->programLock);
	}
}
