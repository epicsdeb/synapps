/*************************************************************************\
Copyright (c) 1991-1994 The Regents of the University of California
                        and the University of Chicago.
                        Los Alamos National Laboratory
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
/*	Interface functions from state program to run-time sequencer.
 *
 *	Author:  Andy Kozubal
 *	Date:    1 March, 1994
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

static pvStat check_connected(DBCHAN *dbch, PVMETA *meta)
{
	if (!dbch->connected)
	{
		meta->status = pvStatDISCONN;
		meta->severity = pvSevrINVALID;
		meta->message = "disconnected";
		return meta->status;
	}
	else
	{
		return pvStatOK;
	}
}

/*
 * Get value from a channel.
 * TODO: add optional timeout argument.
 */
epicsShareFunc pvStat epicsShareAPI seq_pvGet(SS_ID ss, VAR_ID varId, enum compType compType)
{
	SPROG		*sp = ss->sprog;
	CHAN		*ch = sp->chan + varId;
	pvStat		status;
	PVREQ		*req;
	epicsEventId	getSem = ss->getSemId[varId];
	DBCHAN		*dbch = ch->dbch;
	PVMETA		*meta = metaPtr(ch,ss);
	double		tmo = 10.0;

	/* Anonymous PV and safe mode, just copy from shared buffer.
	   Note that completion is always immediate, so no distinction
	   between SYNC and ASYNC needed. See also pvGetComplete. */
	if ((sp->options & OPT_SAFE) && !dbch)
	{
		/* Copy regardless of whether dirty flag is set or not */
		ss_read_buffer(ss, ch, FALSE);
		return pvStatOK;
	}
	/* No named PV and traditional mode => user error */
	if (!dbch)
	{
		errlogSevPrintf(errlogFatal,
			"pvGet(%s): user error (variable not assigned)\n",
			ch->varName
		);
		return pvStatERROR;
	}

	if (compType == DEFAULT)
	{
		compType = (sp->options & OPT_ASYNC) ? ASYNC : SYNC;
	}

	if (compType == SYNC)
	{
		double before, after;
		pvTimeGetCurrentDouble(&before);
		switch (epicsEventWaitWithTimeout(getSem, tmo))
		{
		case epicsEventWaitOK:
			status = check_connected(dbch, meta);
			if (status) return epicsEventSignal(getSem), status;
			pvTimeGetCurrentDouble(&after);
			tmo -= (after - before);
			break;
		case epicsEventWaitTimeout:
			errlogSevPrintf(errlogFatal,
				"pvGet(ss %s, var %s, pv %s): failed (timeout "
				"waiting for other get requests to finish)\n",
				ss->ssName, ch->varName, dbch->dbName
			);
			return pvStatERROR;
		case epicsEventWaitError:
			errlogSevPrintf(errlogFatal,
				"pvGet: epicsEventWaitWithTimeout() failure\n");
			return pvStatERROR;
		}
	}
	else if (compType == ASYNC)
	{
		switch (epicsEventTryWait(getSem))
		{
		case epicsEventWaitOK:
			status = check_connected(dbch, meta);
			if (status) return epicsEventSignal(getSem), status;
			break;
		case epicsEventWaitTimeout:
			errlogSevPrintf(errlogFatal,
				"pvGet(ss %s, var %s, pv %s): user error "
				"(there is already a get pending for this variable/"
				"state set combination)\n",
				ss->ssName, ch->varName, dbch->dbName
			);
			return pvStatERROR;
		case epicsEventWaitError:
			errlogSevPrintf(errlogFatal,
				"pvGet: epicsEventTryWait() failure\n");
			return pvStatERROR;
		}
	}

	/* Allocate and initialize a pv request */
	req = (PVREQ *)freeListMalloc(sp->pvReqPool);
	req->ss = ss;
	req->ch = ch;

	/* Perform the PV get operation with a callback routine specified.
	   Requesting more than db channel has available is ok. */
	status = pvVarGetCallback(
			dbch->pvid,		/* PV id */
			ch->type->getType,	/* request type */
			ch->count,		/* element count */
			seq_get_handler,	/* callback handler */
			req);			/* user arg */
	if (status != pvStatOK)
	{
		meta->status = pvStatERROR;
		meta->severity = pvSevrMAJOR;
		meta->message = "get failure";
		errlogSevPrintf(errlogFatal, "pvGet(var %s, pv %s): pvVarGetCallback() failure: %s\n",
			ch->varName, dbch->dbName, pvVarGetMess(dbch->pvid));
		freeListFree(sp->pvReqPool, req);
		epicsEventSignal(getSem);
		check_connected(dbch, meta);
		return status;
	}

	/* Synchronous: wait for completion */
	if (compType == SYNC)
	{
		pvSysFlush(sp->pvSys);
		switch (epicsEventWaitWithTimeout(getSem, tmo))
		{
		case epicsEventWaitOK:
			epicsEventSignal(getSem);
			status = check_connected(dbch, meta);
			if (status) return status;
			if (sp->options & OPT_SAFE)
				/* Copy regardless of whether dirty flag is set or not */
				ss_read_buffer(ss, ch, FALSE);
			break;
		case epicsEventWaitTimeout:
			meta->status = pvStatTIMEOUT;
			meta->severity = pvSevrMAJOR;
			meta->message = "get completion timeout";
			return meta->status;
		case epicsEventWaitError:
			meta->status = pvStatERROR;
			meta->severity = pvSevrMAJOR;
			meta->message = "get completion failure";
			return meta->status;
		}
	}

	return pvStatOK;
}

/*
 * Return whether the last get completed. In safe mode, as a
 * side effect, copy value from shared buffer to state set local buffer.
 */
epicsShareFunc boolean epicsShareAPI seq_pvGetComplete(SS_ID ss, VAR_ID varId)
{
	epicsEventId	getSem = ss->getSemId[varId];
	SPROG		*sp = ss->sprog;
	CHAN		*ch = sp->chan + varId;
	pvStat		status;

	if (!ch->dbch)
	{
		if (sp->options & OPT_SAFE)
		{
			/* Anonymous PVs always complete immediately */
			return TRUE;
		}
		else
		{
			errlogSevPrintf(errlogFatal,
				"pvGetComplete(%s): user error (variable not assigned)\n",
				ch->varName
			);
			return FALSE;
		}
	}

	switch (epicsEventTryWait(getSem))
	{
	case epicsEventWaitOK:
		epicsEventSignal(getSem);
		status = check_connected(ch->dbch, metaPtr(ch,ss));
		/* TODO: returning either TRUE or FALSE here seems wrong. We return TRUE,
		   so that state sets don't hang. Still means that user code has to check
		   status by calling pvStatus and/or pvMessage. */
		if (status) return TRUE;
		/* In safe mode, copy value and meta data from shared buffer
		   to ss local buffer. */
		if (sp->options & OPT_SAFE)
			/* Copy regardless of whether dirty flag is set or not */
			ss_read_buffer(ss, ch, FALSE);
		return TRUE;
	case epicsEventWaitTimeout:
		return FALSE;
	case epicsEventWaitError:
		errlogSevPrintf(errlogFatal, "pvGetComplete: "
		  "epicsEventTryWait(getSemId[%d]) failure\n", varId);
	default:
		return FALSE;
	}
}

struct putq_cp_arg {
	CHAN	*ch;
	void	*var;
};

static void *putq_cp(void *dest, const void *src, size_t elemSize)
{
	struct putq_cp_arg *arg = (struct putq_cp_arg *)src;
	CHAN *ch = arg->ch;

	return memcpy(pv_value_ptr(dest, ch->type->getType),
		arg->var, ch->type->size * ch->count);
}

static void anonymous_put(SS_ID ss, CHAN *ch)
{
	char *var = valPtr(ch,ss);

	if (ch->queue)
	{
		QUEUE queue = ch->queue;
		pvType type = ch->type->getType;
		size_t size = ch->type->size;
		boolean full;
		struct putq_cp_arg arg = {ch, var};

		DEBUG("anonymous_put: type=%d, size=%d, count=%d, buf_size=%d, q=%p\n",
			type, size, ch->count, pv_size_n(type, ch->count), queue);
		print_channel_value(DEBUG, ch, var);

		/* Note: Must lock here because multiple state sets can issue
		   pvPut calls concurrently. OTOH, no need to lock against CA
		   callbacks, because anonymous and named PVs are disjoint. */
		epicsMutexMustLock(ch->varLock);

		full = seqQueuePutF(queue, putq_cp, &arg);
		if (full)
		{
			errlogSevPrintf(errlogMinor,
			  "pvPut on queued variable '%s' (anonymous): "
			  "last queue element overwritten (queue is full)\n",
			  ch->varName
			);
		}

		epicsMutexUnlock(ch->varLock);
	}
	else
	{
		/* Set dirty flag only if monitored */
		ss_write_buffer(ch, var, 0, ch->monitored);
	}
	/* If there's an event flag associated with this channel, set it */
	if (ch->syncedTo)
		seq_efSet(ss, ch->syncedTo);
	/* Wake up each state set that uses this channel in an event */
	seqWakeup(ss->sprog, ch->eventNum);
}

/*
 * Put a variable's value to a PV.
 */
epicsShareFunc pvStat epicsShareAPI seq_pvPut(SS_ID ss, VAR_ID varId, enum compType compType)
{
	SPROG	*sp = ss->sprog;
	CHAN	*ch = sp->chan + varId;
	pvStat	status;
	unsigned count;
	char	*var = valPtr(ch,ss);	/* ptr to value */
	PVREQ	*req;
	DBCHAN	*dbch = ch->dbch;
	PVMETA	*meta = metaPtr(ch,ss);
	epicsEventId	putSem = ss->putSemId[varId];
	double	tmo = 10.0;

	DEBUG("pvPut: pv name=%s, var=%p\n", dbch ? dbch->dbName : "<anonymous>", var);

	/* First handle anonymous PV (safe mode only) */
	if ((sp->options & OPT_SAFE) && !dbch)
	{
		anonymous_put(ss, ch);
		return pvStatOK;
	}
	if (!dbch)
	{
		errlogSevPrintf(errlogFatal,
			"pvPut(%s): user error (variable not assigned)\n",
			ch->varName
		);
		return pvStatERROR;
	}

	/* Check for channel connected */
	status = check_connected(dbch, meta);
	if (status) return status;

	/* Determine whether to perform synchronous, asynchronous, or
	   plain put ((+a) option was never honored for put, so DEFAULT
	   means non-blocking and therefore implicitly asynchronous) */
	if (compType == SYNC)
	{
		double before, after;
		pvTimeGetCurrentDouble(&before);
		switch (epicsEventWaitWithTimeout(putSem, tmo))
		{
		case epicsEventWaitOK:
			pvTimeGetCurrentDouble(&after);
			tmo -= (after - before);
			break;
		case epicsEventWaitTimeout:
			errlogSevPrintf(errlogFatal,
				"pvPut(ss %s, var %s, pv %s): failed (timeout "
				"waiting for other put requests to finish)\n",
				ss->ssName, ch->varName, dbch->dbName
			);
			return pvStatERROR;
		case epicsEventWaitError:
			errlogSevPrintf(errlogFatal,
				"pvPut: epicsEventWaitWithTimeout() failure\n");
			return pvStatERROR;
		}
	}
	else if (compType == ASYNC)
	{
		switch (epicsEventTryWait(putSem))
		{
		case epicsEventWaitOK:
			break;
		case epicsEventWaitTimeout:
			meta->status = pvStatERROR;
			meta->severity = pvSevrMAJOR;
			meta->message = "already one put pending";
			status = meta->status;
			errlogSevPrintf(errlogFatal,
				"pvPut(ss %s, var %s, pv %s): user error "
				"(there is already a put pending for this variable/"
				"state set combination)\n",
				ss->ssName, ch->varName, dbch->dbName
			);
			return pvStatERROR;
		case epicsEventWaitError:
			errlogSevPrintf(errlogFatal,
				"pvPut: epicsEventTryWait() failure\n");
			return pvStatERROR;
		}
	}

	/* Determine number of elements to put (don't try to put more
	   than db count) */
	count = dbch->dbCount;

	/* Perform the PV put operation (either non-blocking or with a
	   callback routine specified) */
	if (compType == DEFAULT)
	{
		status = pvVarPutNoBlock(
				dbch->pvid,		/* PV id */
				ch->type->putType,	/* data type */
				count,			/* element count */
				(pvValue *)var);	/* data value */
		if (status != pvStatOK)
		{
			errlogSevPrintf(errlogFatal, "pvPut(var %s, pv %s): pvVarPutNoBlock() failure: %s\n",
				ch->varName, dbch->dbName, pvVarGetMess(dbch->pvid));
			return status;
		}
	}
	else
	{
		/* Allocate and initialize a pv request */
		req = (PVREQ *)freeListMalloc(sp->pvReqPool);
		req->ss = ss;
		req->ch = ch;

		status = pvVarPutCallback(
				dbch->pvid,		/* PV id */
				ch->type->putType,	/* data type */
				count,			/* element count */
				(pvValue *)var,		/* data value */
				seq_put_handler,	/* callback handler */
				req);			/* user arg */
		if (status != pvStatOK)
		{
			errlogSevPrintf(errlogFatal, "pvPut(var %s, pv %s): pvVarPutCallback() failure: %s\n",
				ch->varName, dbch->dbName, pvVarGetMess(dbch->pvid));
			freeListFree(sp->pvReqPool, req);
			epicsEventSignal(putSem);
			check_connected(dbch, meta);
			return status;
		}
	}

	/* Synchronous: wait for completion (10s timeout) */
	if (compType == SYNC)
	{
		pvSysFlush(sp->pvSys);
		switch (epicsEventWaitWithTimeout(putSem, tmo))
		{
		case epicsEventWaitOK:
			epicsEventSignal(putSem);
			status = check_connected(dbch, meta);
			if (status) return status;
			break;
		case epicsEventWaitTimeout:
			meta->status = pvStatTIMEOUT;
			meta->severity = pvSevrMAJOR;
			meta->message = "put completion timeout";
			return meta->status;
			break;
		case epicsEventWaitError:
			meta->status = pvStatERROR;
			meta->severity = pvSevrMAJOR;
			meta->message = "put completion failure";
			return meta->status;
			break;
		}
	}

	return pvStatOK;
}

/*
 * Return whether the last put completed.
 */
epicsShareFunc boolean epicsShareAPI seq_pvPutComplete(
	SS_ID		ss,
	VAR_ID		varId,
	unsigned	length,
	boolean		any,
	boolean		*complete)
{
	boolean		anyDone = FALSE, allDone = TRUE;
	unsigned	n;

	for (n = 0; n < length; n++)
	{
		epicsEventId	putSem = ss->putSemId[varId+n];
		boolean		done = FALSE;
		CHAN		*ch = ss->sprog->chan + varId + n;

		switch (epicsEventTryWait(putSem))
		{
		case epicsEventWaitOK:
			epicsEventSignal(putSem);
			check_connected(ch->dbch, metaPtr(ch,ss));
			/* TODO: returning either TRUE or FALSE here seems wrong. We return TRUE,
			   so that state sets don't hang. Still means that user code has to check
			   status by calling pvStatus and/or pvMessage. */
			done = TRUE;
			break;
		case epicsEventWaitError:
			errlogSevPrintf(errlogFatal, "pvPutComplete(%s): "
			  "epicsEventTryWait(putSem[%d]) failure\n", ch->varName, varId);
		case epicsEventWaitTimeout:
			break;
		}

		anyDone = anyDone || done;
		allDone = allDone && done;

		if (complete)
		{
			complete[n] = done;
		}
		else if (any && done)
		{
			break;
		}
	}

	DEBUG("pvPutComplete: varId=%u, length=%u, anyDone=%u, allDone=%u\n",
		varId, length, anyDone, allDone);

	return any?anyDone:allDone;
}

/*
 * Assign/Connect to a channel.
 * Assign to a zero-length string ("") disconnects/de-assigns,
 * in safe mode, creates an anonymous PV.
 */
epicsShareFunc pvStat epicsShareAPI seq_pvAssign(SS_ID ss, VAR_ID varId, const char *pvName)
{
	SPROG	*sp = ss->sprog;
	CHAN	*ch = sp->chan + varId;
	pvStat	status = pvStatOK;
	DBCHAN	*dbch = ch->dbch;

	if (!pvName) pvName = "";

	DEBUG("Assign %s to \"%s\"\n", ch->varName, pvName);

	epicsMutexMustLock(sp->programLock);

	if (dbch)	/* was assigned to a named PV */
	{
		status = pvVarDestroy(dbch->pvid);
		dbch->pvid = NULL;

		sp->assignCount -= 1;

		if (dbch->connected)	/* see connection handler */
		{
			dbch->connected = FALSE;
			sp->connectCount--;

			if (ch->monitored)
			{
				seq_monitor(ch, FALSE);
			}
		}

		if (status != pvStatOK)
		{
			errlogSevPrintf(errlogFatal, "pvAssign(var %s, pv %s): pvVarDestroy() failure: "
				"%s\n", ch->varName, dbch->dbName, pvVarGetMess(dbch->pvid));
		}
		free(dbch->dbName);
	}

	if (pvName == NULL || pvName[0] == 0)	/* new name is empty -> free resources */
	{
		if (dbch) {
			free(ch->dbch->ssMetaData);
			free(ch->dbch);
		}
	}
	else		/* new name is non-empty -> create resources */
	{
		if (!dbch)
		{
			dbch = new(DBCHAN);
			if (!dbch)
			{
				errlogSevPrintf(errlogFatal, "pvAssign: calloc failed\n");
				return pvStatERROR;
			}
		}
		dbch->dbName = epicsStrDup(pvName);
		if (!dbch->dbName)
		{
			errlogSevPrintf(errlogFatal, "pvAssign: epicsStrDup failed\n");
			free(dbch);
			return pvStatERROR;
		}
		if ((sp->options & OPT_SAFE) && sp->numSS > 0)
		{
			dbch->ssMetaData = newArray(PVMETA, sp->numSS);
			if (!dbch->ssMetaData)
			{
				errlogSevPrintf(errlogFatal, "pvAssign: calloc failed\n");
				free(dbch->dbName);
				free(dbch);
				return pvStatERROR;
			}
		}
		ch->dbch = dbch;
		sp->assignCount++;
		status = pvVarCreate(
			sp->pvSys,		/* PV system context */
			dbch->dbName,		/* DB channel name */
			seq_conn_handler,	/* connection handler routine */
			ch,			/* user ptr is CHAN structure */
			sp->debug,		/* debug level (inherited) */
			&dbch->pvid);		/* ptr to pvid */
		if (status != pvStatOK)
		{
			errlogSevPrintf(errlogFatal, "pvAssign(var %s, pv %s): pvVarCreate() failure: "
				"%s\n", ch->varName, dbch->dbName, pvVarGetMess(dbch->pvid));
			if (ch->dbch->ssMetaData)
				free(ch->dbch->ssMetaData);
			free(ch->dbch->dbName);
			free(ch->dbch);
		}
	}

	epicsMutexUnlock(sp->programLock);

	return status;
}

/*
 * Initiate a monitor.
 */
epicsShareFunc pvStat epicsShareAPI seq_pvMonitor(SS_ID ss, VAR_ID varId)
{
	SPROG	*sp = ss->sprog;
	CHAN	*ch = sp->chan + varId;
	DBCHAN	*dbch = ch->dbch;

	if (!dbch && (sp->options & OPT_SAFE))
	{
		ch->monitored = TRUE;
		return pvStatOK;
	}
	if (!dbch)
	{
		errlogSevPrintf(errlogFatal,
			"pvMonitor(%s): user error (variable not assigned)\n",
			ch->varName
		);
		return pvStatERROR;
	}
	ch->monitored = TRUE;
	return seq_monitor(ch, TRUE);
}

/*
 * Cancel a monitor.
 */
epicsShareFunc pvStat epicsShareAPI seq_pvStopMonitor(SS_ID ss, VAR_ID varId)
{
	SPROG	*sp = ss->sprog;
	CHAN	*ch = sp->chan + varId;
	DBCHAN	*dbch = ch->dbch;

	if (!dbch && (sp->options & OPT_SAFE))
	{
		ch->monitored = FALSE;
		return pvStatOK;
	}
	if (!dbch)
	{
		errlogSevPrintf(errlogFatal,
			"pvStopMonitor(%s): user error (variable not assigned)\n",
			ch->varName
		);
		return pvStatERROR;
	}
	ch->monitored = FALSE;
	return seq_monitor(ch, FALSE);
}

/*
 * Synchronize pv with an event flag.
 * ev_flag == 0 means unSync.
 */
epicsShareFunc void epicsShareAPI seq_pvSync(SS_ID ss, VAR_ID varId, EV_ID new_ev_flag)
{
	SPROG	*sp = ss->sprog;
	CHAN	*this_ch = sp->chan + varId;
	EV_ID	old_ev_flag = this_ch->syncedTo;

	assert(new_ev_flag >= 0 && new_ev_flag <= sp->numEvFlags);

	epicsMutexMustLock(sp->programLock);
	if (old_ev_flag != new_ev_flag)
	{
		if (old_ev_flag)
		{
			/* remove it from the old list */
			CHAN *ch = sp->syncedChans[old_ev_flag];
			assert(ch);			/* since old_ev_flag != 0 */
			if (ch == this_ch)		/* first in list */
			{
				sp->syncedChans[old_ev_flag] = this_ch->nextSynced;
				ch->nextSynced = 0;
			}
			else
			{
				while (ch->nextSynced != this_ch)
				{
					ch = ch->nextSynced;
					assert(ch);	/* since old_ev_flag != 0 */
				}
				assert (ch->nextSynced == this_ch);
				ch->nextSynced = this_ch->nextSynced;
			}
		}
		this_ch->syncedTo = new_ev_flag;
		if (new_ev_flag)
		{
			/* insert it into the new list */
			CHAN *ch = sp->syncedChans[new_ev_flag];
			sp->syncedChans[new_ev_flag] = this_ch;
			this_ch->nextSynced = ch;
		}
	}
	epicsMutexUnlock(sp->programLock);
}

/*
 * Return total number of database channels.
 */
epicsShareFunc unsigned epicsShareAPI seq_pvChannelCount(SS_ID ss)
{
	return ss->sprog->numChans;
}

/*
 * Return number of database channels connected.
 */
epicsShareFunc unsigned epicsShareAPI seq_pvConnectCount(SS_ID ss)
{
	return ss->sprog->connectCount;
}

/*
 * Return number of database channels assigned.
 */
epicsShareFunc unsigned epicsShareAPI seq_pvAssignCount(SS_ID ss)
{
	return ss->sprog->assignCount;
}

/* Flush outstanding PV requests */
epicsShareFunc void epicsShareAPI seq_pvFlush(SS_ID ss)
{
	pvSysFlush(ss->sprog->pvSys);
}

/*
 * Return whether database channel is connected.
 */
epicsShareFunc boolean epicsShareAPI seq_pvConnected(SS_ID ss, VAR_ID varId)
{
	CHAN *ch = ss->sprog->chan + varId;
	return ch->dbch && ch->dbch->connected;
}

/*
 * Return whether database channel is assigned.
 */
epicsShareFunc boolean epicsShareAPI seq_pvAssigned(SS_ID ss, VAR_ID varId)
{
	return ss->sprog->chan[varId].dbch != NULL;
}

/*
 * Return number elements in an array, which is the lesser of
 * the array size and the element count returned by the PV layer.
 */
epicsShareFunc unsigned epicsShareAPI seq_pvCount(SS_ID ss, VAR_ID varId)
{
	CHAN *ch = ss->sprog->chan + varId;
	return ch->dbch ? ch->dbch->dbCount : ch->count;
}

/*
 * Return a channel name of an assigned variable.
 */
epicsShareFunc char *epicsShareAPI seq_pvName(SS_ID ss, VAR_ID varId)
{
	CHAN *ch = ss->sprog->chan + varId;
	return ch->dbch ? ch->dbch->dbName : NULL;
}

/*
 * Return channel alarm status.
 */
epicsShareFunc pvStat epicsShareAPI seq_pvStatus(SS_ID ss, VAR_ID varId)
{
	CHAN	*ch = ss->sprog->chan + varId;
	PVMETA	*meta = metaPtr(ch,ss);
	return ch->dbch ? meta->status : pvStatOK;
}

/*
 * Return channel alarm severity.
 */
epicsShareFunc pvSevr epicsShareAPI seq_pvSeverity(SS_ID ss, VAR_ID varId)
{
	CHAN	*ch = ss->sprog->chan + varId;
	PVMETA	*meta = metaPtr(ch,ss);
	return ch->dbch ? meta->severity : pvSevrOK;
}

/*
 * Return channel error message.
 */
epicsShareFunc const char *epicsShareAPI seq_pvMessage(SS_ID ss, VAR_ID varId)
{
	CHAN	*ch = ss->sprog->chan + varId;
	PVMETA	*meta = metaPtr(ch,ss);
	return ch->dbch ? meta->message : "";
}

/*
 * Return index of database variable.
 */
epicsShareFunc VAR_ID epicsShareAPI seq_pvIndex(SS_ID ss, VAR_ID varId)
{
	return varId; /* index is same as varId */
}

/*
 * Return channel time stamp.
 */
epicsShareFunc epicsTimeStamp epicsShareAPI seq_pvTimeStamp(SS_ID ss, VAR_ID varId)
{
	CHAN	*ch = ss->sprog->chan + varId;
	PVMETA	*meta = metaPtr(ch,ss);
	if (ch->dbch)
	{
		return meta->timeStamp;
	}
	else
	{
		epicsTimeStamp ts;
		epicsTimeGetCurrent(&ts);
		return ts;
	}
}

/*
 * Set an event flag, then wake up each state
 * set that might be waiting on that event flag.
 */
epicsShareFunc void epicsShareAPI seq_efSet(SS_ID ss, EV_ID ev_flag)
{
	SPROG	*sp = ss->sprog;

	DEBUG("efSet: sp=%p, ss=%p, ev_flag=%d\n", sp, ss,
		ev_flag);
	assert(ev_flag > 0 && ev_flag <= ss->sprog->numEvFlags);

	epicsMutexMustLock(sp->programLock);

	/* Set this bit */
	bitSet(sp->evFlags, ev_flag);

	/* Wake up state sets that are waiting for this event flag */
	seqWakeup(sp, ev_flag);

	epicsMutexUnlock(sp->programLock);
}

/*
 * Return whether event flag is set.
 */
epicsShareFunc boolean epicsShareAPI seq_efTest(SS_ID ss, EV_ID ev_flag)
/* event flag */
{
	SPROG	*sp = ss->sprog;
	boolean	isSet;

	assert(ev_flag > 0 && ev_flag <= ss->sprog->numEvFlags);
	epicsMutexMustLock(sp->programLock);

	isSet = bitTest(sp->evFlags, ev_flag);

	DEBUG("efTest: ev_flag=%d, isSet=%d\n", ev_flag, isSet);

	if (sp->options & OPT_SAFE)
		ss_read_buffer_selective(sp, ss, ev_flag);

	epicsMutexUnlock(sp->programLock);

	return isSet;
}

/*
 * Clear event flag.
 */
epicsShareFunc boolean epicsShareAPI seq_efClear(SS_ID ss, EV_ID ev_flag)
{
	SPROG	*sp = ss->sprog;
	boolean	isSet;

	assert(ev_flag > 0 && ev_flag <= ss->sprog->numEvFlags);
	epicsMutexMustLock(sp->programLock);

	isSet = bitTest(sp->evFlags, ev_flag);
	bitClear(sp->evFlags, ev_flag);

	/* Wake up state sets that are waiting for this event flag */
	seqWakeup(sp, ev_flag);

	epicsMutexUnlock(sp->programLock);

	return isSet;
}

/*
 * Atomically test event flag against outstanding events, then clear it
 * and return whether it was set.
 */
epicsShareFunc boolean epicsShareAPI seq_efTestAndClear(SS_ID ss, EV_ID ev_flag)
{
	SPROG	*sp = ss->sprog;
	boolean	isSet;

	assert(ev_flag > 0 && ev_flag <= ss->sprog->numEvFlags);
	epicsMutexMustLock(sp->programLock);

	isSet = bitTest(sp->evFlags, ev_flag);
	bitClear(sp->evFlags, ev_flag);

	DEBUG("efTestAndClear: ev_flag=%d, isSet=%d, ss=%d\n", ev_flag, isSet, (int)ssNum(ss));

	if (sp->options & OPT_SAFE)
		ss_read_buffer_selective(sp, ss, ev_flag);

	epicsMutexUnlock(sp->programLock);

	return isSet;
}

struct getq_cp_arg {
	CHAN	*ch;
	void	*var;
	PVMETA	*meta;
};

static void *getq_cp(void *dest, const void *value, size_t elemSize)
{
	struct getq_cp_arg *arg = (struct getq_cp_arg *)dest;
	CHAN	*ch = arg->ch;
	PVMETA	*meta = arg->meta;
	void	*var = arg->var;
	pvType	type = ch->type->getType;
	size_t	count = ch->count;

	if (ch->dbch)
	{
		assert(pv_is_time_type(type));
		/* Copy status, severity and time stamp */
		meta->status = *pv_status_ptr(value,type);
		meta->severity = *pv_severity_ptr(value,type);
		meta->timeStamp = *pv_stamp_ptr(value,type);
		count = ch->dbch->dbCount;
	}
	return memcpy(var, pv_value_ptr(value,type), ch->type->size * count);
}

/*
 * Get value from a queued PV.
 */
epicsShareFunc boolean epicsShareAPI seq_pvGetQ(SS_ID ss, VAR_ID varId)
{
	SPROG	*sp = ss->sprog;
	CHAN	*ch = sp->chan + varId;
	void	*var = valPtr(ch,ss);
	EV_ID	ev_flag = ch->syncedTo;
	PVMETA	*meta = metaPtr(ch,ss);
	boolean	was_empty;
	struct getq_cp_arg arg = {ch, var, meta};

	if (!ch->queue)
	{
		errlogSevPrintf(errlogFatal,
			"pvGetQ(%s): user error (variable not queued)\n",
			ch->varName
		);
		return FALSE;
	}

	was_empty = seqQueueGetF(ch->queue, getq_cp, &arg);

	if (ev_flag)
	{
		epicsMutexMustLock(sp->programLock);
		/* If queue is now empty, clear the event flag */
		if (seqQueueIsEmpty(ch->queue))
		{
			bitClear(sp->evFlags, ev_flag);
		}
		epicsMutexUnlock(sp->programLock);
	}

	return (!was_empty);
}

/*
 * Flush elements on syncQ queue and clear event flag.
 */
epicsShareFunc void epicsShareAPI seq_pvFlushQ(SS_ID ss, VAR_ID varId)
{
	SPROG	*sp = ss->sprog;
	CHAN	*ch = sp->chan + varId;
	EV_ID	ev_flag = ch->syncedTo;
	QUEUE	queue = ch->queue;

	DEBUG("pvFlushQ: pv name=%s, count=%d\n", ch->dbch ? ch->dbch->dbName : "<anomymous>",
		seqQueueUsed(queue));
	seqQueueFlush(queue);

	epicsMutexMustLock(sp->programLock);
	/* Clear event flag */
	bitClear(sp->evFlags, ev_flag);
	epicsMutexUnlock(sp->programLock);
}

/*
 * Test whether a given delay has expired.
 */
epicsShareFunc boolean epicsShareAPI seq_delay(SS_ID ss, DELAY_ID delayId)
{
	double	timeNow, timeElapsed;
	boolean	expired = FALSE;

	/* Calc. elapsed time since state was entered */
	pvTimeGetCurrentDouble( &timeNow );
	timeElapsed = timeNow - ss->timeEntered;

	/* Check for delay timeout */
	if (timeElapsed > ss->delay[delayId]-0.000001)
	{
		ss->delayExpired[delayId] = TRUE; /* mark as expired */
		expired = TRUE;
	}
	DEBUG("delay(%s,%u): diff=%.10f, %s\n", ss->ssName, delayId,
		timeElapsed - ss->delay[delayId], expired ? "expired": "unexpired");
	return expired;
}

/*
 * Initialize delay with given time (in seconds).
 */
epicsShareFunc void epicsShareAPI seq_delayInit(SS_ID ss, DELAY_ID delayId, double delay)
{
	DEBUG("delayInit(%s,%u,%g): numDelays=%d, maxNumDelays=%d\n",
		ss->ssName, delayId, delay, ss->numDelays, ss->maxNumDelays);
	assert(delayId <= ss->numDelays);
	assert(ss->numDelays < ss->maxNumDelays);

	ss->delay[delayId] = delay;
	ss->numDelays = max(ss->numDelays, delayId + 1);
}

/*
 * Return the value of an option (e.g. "a").
 * FALSE means "-" and TRUE means "+".
 */
epicsShareFunc boolean epicsShareAPI seq_optGet(SS_ID ss, const char *opt)
{
	SPROG	*sp = ss->sprog;

	assert(opt);
	switch (opt[0])
	{
	case 'a': return ( (sp->options & OPT_ASYNC) != 0);
	case 'c': return ( (sp->options & OPT_CONN)  != 0);
	case 'd': return ( (sp->options & OPT_DEBUG) != 0);
	case 'e': return ( (sp->options & OPT_NEWEF) != 0);
	case 'm': return ( (sp->options & OPT_MAIN)  != 0);
	case 'r': return ( (sp->options & OPT_REENT) != 0);
	case 's': return ( (sp->options & OPT_SAFE)  != 0);
	default:  return FALSE;
	}
}

/* 
 * Given macro name, return pointer to its value.
 */
epicsShareFunc char *epicsShareAPI seq_macValueGet(SS_ID ss, const char *name)
{
	return seqMacValGet(ss->sprog, name);
}

/* 
 * Immediately terminate all state sets and jump to global exit block.
 */
epicsShareFunc void epicsShareAPI seq_exit(SS_ID ss)
{
	SPROG *sp = ss->sprog;
	/* Ask all state set threads to exit */
	sp->die = TRUE;
	/* Take care that we die even if waiting for initial connect */
	epicsEventSignal(sp->ready);
	/* Wakeup all state sets unconditionally */
	seqWakeup(sp, 0);
}
