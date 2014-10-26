/*************************************************************************\
Copyright (c) 1991-1994 The Regents of the University of California
                        and the University of Chicago.
                        Los Alamos National Laboratory
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
/*	Process Variable interface for sequencer.
 *
 *	Author:  Andy Kozubal
 *	Date:    July, 1991
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *	  The Controls and Automation Group (AT-8)
 *	  Ground Test Accelerator
 *	  Accelerator Technology Division
 *	  Los Alamos National Laboratory
 *
 *	Co-developed with
 *	  The Controls and Computing Group
 *	  Accelerator Systems Division
 *	  Advanced Photon Source
 *	  Argonne National Laboratory
 */
#include "seq.h"
#include "seq_debug.h"

/*
 * Event type (extra argument passed to proc_db_events().
 */
enum event_type {
	GET_COMPLETE,
	PUT_COMPLETE,
	MON_COMPLETE
};

static const char *event_type_name[] = {
	"get",
	"put",
	"mon"
};

static void proc_db_events(pvValue *value, pvType type,
	CHAN *ch, SSCB *ss, enum event_type evtype, pvStat status);
static void proc_db_events_queued(SPROG *sp, CHAN *ch, pvValue *value);

/*
 * seq_connect() - Initiate connect & monitor requests to PVs.
 * If wait is TRUE, wait for all connections to be established.
 */
pvStat seq_connect(SPROG *sp, boolean wait)
{
	pvStat		status;
	unsigned	nch;
	int		delay = 2.0;
	boolean		ready = FALSE;

	/*
	 * For each channel: create pv object, then subscribe if monitored.
	 */
	for (nch = 0; nch < sp->numChans; nch++)
	{
		CHAN *ch = sp->chan + nch;
		DBCHAN *dbch = ch->dbch;

		if (dbch == NULL)
			continue; /* skip records without pv names */
		DEBUG("seq_connect: connect %s to %s\n", ch->varName,
			dbch->dbName);
		/* Connect to it */
		dbch->pvid = NULL;
		status = pvVarCreate(
				sp->pvSys,		/* PV system context */
				ch->dbch->dbName,	/* PV name */
				seq_conn_handler,	/* connection handler routine */
				ch,			/* private data is CHAN struc */
				sp->debug,		/* debug level (inherited) */
				&dbch->pvid);		/* ptr to PV id */
		if (status != pvStatOK)
		{
			errlogSevPrintf(errlogFatal, "seq_connect: var %s, pv %s: pvVarCreate() failure: "
				"%s\n", ch->varName, dbch->dbName, pvVarGetMess(dbch->pvid));
			if (ch->dbch->ssMetaData)
				free(ch->dbch->ssMetaData);
			free(ch->dbch->dbName);
			free(ch->dbch);
			continue;
		}
	}
	pvSysFlush(sp->pvSys);

	if (wait)
	{
		boolean firstTime = TRUE;
		double timeStartWait;
		pvTimeGetCurrentDouble(&timeStartWait);

		do {
			unsigned ac, mc, cc, gmc;
			/* Check whether we have been asked to exit */
			if (sp->die)
				return pvStatERROR;

			epicsMutexMustLock(sp->programLock);
			ac = sp->assignCount;
			mc = sp->monitorCount;
			cc = sp->connectCount;
			gmc = sp->firstMonitorCount;
			epicsMutexUnlock(sp->programLock);

			ready = ac == cc && mc == gmc;
			if (!ready)
			{
				double timeNow;
				if (!firstTime)
				{
					errlogSevPrintf(errlogMinor,
						"%s[%d](after %d sec): assigned=%d, connected=%d, "
						"monitored=%d, got monitor=%d\n",
						sp->progName, sp->instance,
						(int)(timeNow - timeStartWait),
						ac, cc, mc, gmc);
				}
				firstTime = FALSE;
				if (epicsEventWaitWithTimeout(
					sp->ready, (double)delay) == epicsEventWaitError)
				{
					errlogSevPrintf(errlogFatal, "seq_connect: "
						"epicsEventWaitWithTimeout failure\n");
					return pvStatERROR;
				}
				pvTimeGetCurrentDouble(&timeNow);
				if (delay < 3600)
					delay *= 1.71;
				else
					delay = 3600;
			}
		} while (!ready);
		printf("%s[%d]: all channels connected & received 1st monitor\n",
			sp->progName, sp->instance);
	}
	return pvStatOK;
}

/*
 * seq_get_handler() - Sequencer callback handler.
 * Called when a "get" completes.
 */
void seq_get_handler(
	void *var, pvType type, unsigned count, pvValue *value, void *arg, pvStat status)
{
	PVREQ	*rq = (PVREQ *)arg;
	CHAN	*ch = rq->ch;
	SSCB	*ss = rq->ss;
	SPROG	*sp = ch->sprog;

	assert(ch->dbch != NULL);
	freeListFree(sp->pvReqPool, arg);
	/* ignore callback if not expected, e.g. already timed out */
	if (ss->getReq[chNum(ch)] == rq)
		proc_db_events(value, type, ch, ss, GET_COMPLETE, status);
}

/*
 * seq_put_handler() - Sequencer callback handler.
 * Called when a "put" completes.
 */
void seq_put_handler(
	void *var, pvType type, unsigned count, pvValue *value, void *arg, pvStat status)
{
	PVREQ	*rq = (PVREQ *)arg;
	CHAN	*ch = rq->ch;
	SSCB	*ss = rq->ss;
	SPROG	*sp = ch->sprog;

	assert(ch->dbch != NULL);
	freeListFree(sp->pvReqPool, arg);
	/* ignore callback if not expected, e.g. already timed out */
	if (ss->putReq[chNum(ch)] == rq)
		proc_db_events(value, type, ch, ss, PUT_COMPLETE, status);
}

/*
 * seq_mon_handler() - PV events (monitors) come here.
 */
void seq_mon_handler(
	void *var, pvType type, unsigned count, pvValue *value, void *arg, pvStat status)
{
	CHAN	*ch = (CHAN *)arg;
	SPROG	*sp = ch->sprog;
	DBCHAN	*dbch = ch->dbch;

	assert(dbch != NULL);
	/* Process event handling in each state set */
	proc_db_events(value, type, ch, sp->ss, MON_COMPLETE, status);
	if (!dbch->gotOneMonitor)
	{
		dbch->gotOneMonitor = TRUE;
		epicsMutexMustLock(sp->programLock);
		sp->firstMonitorCount++;
		if (sp->firstMonitorCount == sp->monitorCount
			&& sp->connectCount == sp->assignCount)
		{
			epicsEventSignal(sp->ready);
		}
		epicsMutexUnlock(sp->programLock);
	}
}

/* Common code for completion and monitor handling */
static void proc_db_events(
	pvValue	*value,
	pvType	type,
	CHAN	*ch,
	SSCB	*ss,		/* originator, for put and get */
	enum event_type evtype,
	pvStat	status)
{
	SPROG	*sp = ch->sprog;
	DBCHAN	*dbch = ch->dbch;

	assert(dbch != NULL);

	DEBUG("proc_db_events: var=%s, pv=%s, type=%s, status=%d\n", ch->varName,
		dbch->dbName, event_type_name[evtype], status);

	/* If monitor on var queued via syncQ, branch to alternative routine */
	if (ch->queue && evtype == MON_COMPLETE)
	{
		proc_db_events_queued(sp, ch, value);
		return;
	}

	/* Copy value and meta data into user variable shared buffer
	   (can get NULL value pointer for put completion only) */
	if (value != NULL)
	{
		void *val = pv_value_ptr(value,type);
		PVMETA meta;

		/* must not use an initializer here, the MS C compiler chokes on it */
		meta.timeStamp = *pv_stamp_ptr(value,type);
		meta.status = *pv_status_ptr(value,type);
		meta.severity = *pv_severity_ptr(value,type);
		meta.message = NULL;

		/* Set error message only when severity indicates error */
		if (meta.severity != pvSevrNONE)
		{
			const char *pmsg = pvVarGetMess(dbch->pvid);
			if (!pmsg) pmsg = "unknown";
			meta.message = pmsg;
		}

		/* Write value and meta data to shared buffers.
		   Set the dirty flag only if this was a monitor event. */
		ss_write_buffer(ch, val, &meta, evtype == MON_COMPLETE);
	}

	/* Signal completion */
	switch (evtype)
	{
	case GET_COMPLETE:
		epicsEventSignal(ss->getSemId[chNum(ch)]);
		break;
	case PUT_COMPLETE:
		epicsEventSignal(ss->putSemId[chNum(ch)]);
		break;
	default:
		break;
	}

	/* If there's an event flag associated with this channel, set it */
	/* TODO: check if correct/documented to do this for non-monitor completions */
	if (ch->syncedTo)
		seq_efSet(sp->ss, ch->syncedTo);

	/* Wake up each state set that uses this channel in an event */
	seqWakeup(sp, ch->eventNum);
}

struct putq_cp_arg {
	CHAN	*ch;
	void	*value;
};

static void *putq_cp(void *dest, const void *src, size_t elemSize)
{
	struct putq_cp_arg *arg = (struct putq_cp_arg *)src;
	CHAN *ch = arg->ch;

	return memcpy(dest, arg->value,
		pv_size_n(ch->type->getType, ch->dbch->dbCount));
}

/* Common code for event and callback handling (queuing version) */
static void proc_db_events_queued(SPROG *sp, CHAN *ch, pvValue *value)
{
	boolean	full;
	struct putq_cp_arg arg = {ch, value};

	DEBUG("proc_db_events_queued: var=%s, pv=%s, queue=%p, used(max)=%d(%d)\n",
		ch->varName, ch->dbch->dbName,
		ch->queue, seqQueueUsed(ch->queue), seqQueueNumElems(ch->queue));
	/* Copy whole message into queue; no need to lock against other
	   writers, because named and anonymous PVs are disjoint. */
	full = seqQueuePutF(ch->queue, putq_cp, &arg);
	if (full)
	{
		errlogSevPrintf(errlogMinor,
		  "monitor event for variable '%s' (pv '%s'): "
		  "last queue element overwritten (queue is full)\n",
		  ch->varName, ch->dbch->dbName
		);
	}
	/* Set event flag; note: it doesn't matter which state set we pass. */
	seq_efSet(sp->ss, ch->syncedTo);
	/* Wake up each state set that uses this channel in an event */
	seqWakeup(sp, ch->eventNum);
}

/* Disconnect all database channels */
void seq_disconnect(SPROG *sp)
{
	unsigned nch;

	DEBUG("seq_disconnect: sp = %p\n", sp);

	epicsMutexMustLock(sp->programLock);
	for (nch = 0; nch < sp->numChans; nch++)
	{
		CHAN	*ch = sp->chan + nch;
		pvStat	status;
		DBCHAN	*dbch = ch->dbch;

		if (!dbch)
			continue;
		DEBUG("seq_disconnect: disconnect %s from %s\n",
			ch->varName, dbch->dbName);
		/* Disconnect this PV */
		status = pvVarDestroy(dbch->pvid);
		dbch->pvid = NULL;
		if (status != pvStatOK)
			errlogSevPrintf(errlogFatal, "seq_disconnect: var %s, pv %s: pvVarDestroy() failure: "
				"%s\n", ch->varName, dbch->dbName, pvVarGetMess(dbch->pvid));

		/* Clear monitor & connect indicators */
		dbch->connected = FALSE;
		sp->connectCount -= 1;
	}
	epicsMutexUnlock(sp->programLock);

	pvSysFlush(sp->pvSys);
}

pvStat seq_monitor(CHAN *ch, boolean on)
{
	DBCHAN	*dbch = ch->dbch;
	SPROG	*sp = ch->sprog;
	pvStat	status;
	boolean	done;

	assert(ch);
	assert(dbch);

	epicsMutexMustLock(sp->programLock);
	done = on == (dbch->monid != NULL);
	dbch->gotOneMonitor = FALSE;
	epicsMutexUnlock(sp->programLock);

	if (done)
		return pvStatOK;

	DEBUG("calling pvVarMonitor%s(%p)\n", on?"On":"Off", dbch->pvid);
	if (on)
		status = pvVarMonitorOn(
				dbch->pvid,		/* pvid */
				ch->type->getType,	/* requested type */
				ch->count,		/* element count */
				seq_mon_handler,	/* function to call */
				ch,			/* user arg (channel struct) */
				&dbch->monid);		/* where to put event id */
	else
		status = pvVarMonitorOff(dbch->pvid, dbch->monid);
	if (status != pvStatOK)
		errlogSevPrintf(errlogFatal, "seq_monitor: pvVarMonitor%s(var %s, pv %s) failure: %s\n",
			on?"On":"Off", ch->varName, dbch->dbName, pvVarGetMess(dbch->pvid));
	else if (!on)
		dbch->monid = NULL;
	return status;
}

/*
 * seq_conn_handler() - Sequencer connection handler.
 * Called each time a connection is established or broken.
 */
void seq_conn_handler(void *var, int connected)
{
	CHAN	*ch = (CHAN *)pvVarGetPrivate(var);
	SPROG	*sp = ch->sprog;
	DBCHAN	*dbch = ch->dbch;

	epicsMutexMustLock(sp->programLock);

	assert(dbch != NULL);

	/* Note that CA may call this while pvVarCreate is still running,
	   so dbch->pvid may not yet be initialized. */
	if (!dbch->pvid)
		dbch->pvid = var;

	DEBUG("seq_conn_handler(%p,%d), dbch->pvid=%p\n", var, connected, dbch->pvid);
	assert(dbch->pvid == var);
	if (!connected)
	{
		DEBUG("%s disconnected from %s\n", ch->varName, dbch->dbName);
		if (dbch->connected)
		{
			unsigned nss;

			dbch->connected = FALSE;
			sp->connectCount--;

			if (ch->monitored)
			{
				seq_monitor(ch, FALSE);
			}
			/* terminate outstanding requests that wait for completion */
			for (nss = 0; nss < sp->numSS; nss++)
			{
				epicsEventSignal(sp->ss[nss].getSemId[chNum(ch)]);
				epicsEventSignal(sp->ss[nss].putSemId[chNum(ch)]);
			}
		}
		else
		{
			errlogSevPrintf(errlogMinor,
				"seq_conn_handler: var '%s', pv '%s': "
				"disconnect event but already disconnected\n",
				ch->varName, dbch->dbName);
		}
	}
	else	/* connected */
	{
		DEBUG("%s connected to %s\n", ch->varName, dbch->dbName);
		if (!dbch->connected)
		{
			unsigned dbCount;
			dbch->connected = TRUE;
			sp->connectCount++;
			if (sp->firstMonitorCount == sp->monitorCount
				&& sp->connectCount == sp->assignCount)
			{
				epicsEventSignal(sp->ready);
			}
			dbCount = pvVarGetCount(var);
			assert(dbCount >= 0);
			dbch->dbCount = min(ch->count, (unsigned)dbCount);

			if (ch->monitored)
			{
				seq_monitor(ch, TRUE);
			}
		}
		else
		{
			errlogSevPrintf(errlogMinor,
				"seq_conn_handler: var '%s', pv '%s': "
				"connect event but already connected\n",
				ch->varName, dbch->dbName);
		}
	}
	epicsMutexUnlock(sp->programLock);

	/* Wake up each state set that is waiting for event processing.
	   Why each one? Because pvConnectCount and pvMonitorCount should
	   act like monitored anonymous channels. Any state set might be
	   using these functions inside a when-condition and it is expected
	   that such conditions get checked whenever these counts change. */
	seqWakeup(sp, 0);
}
