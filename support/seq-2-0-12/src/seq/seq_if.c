/*	
	seq_if.c,v 1.3 1995/10/10 01:56:49 wright Exp
 *   DESCRIPTION: Interface functions from state program to run-time sequencer.
 *	Note: To prevent global name conflicts "seq_" is added by the SNC, e.g.
 *	pvPut() becomes seq_pvPut().
 *
 *	Author:  Andy Kozubal
 *	Date:    1 March, 1994
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991-1994, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
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
 *
 * Modification Log:
 * -----------------
 * 20jul95,ajk	Fixed seq_pvPut() so count <= NELM in db.
 * 09aug96,wfl	Added seq_pvGetQ() to support syncQ.
 * 13aug96,wfl	Added seq_pvFreeQ() to free entries on syncQ queue.
 * 23jun96,wfl  Added task wakeup code to seq_efClear() (like seq_efSet()).
 * 29apr99,wfl	Used SEQ_TIME_STAMP; removed unused vx_opt option.
 * 06jul99,wfl	Added freeing of queue entries; supported "+m" (main) option.
 * 07sep99,wfl	Supported per-call sync/async specification on get/put;
 *		Supported setting of message on get/put completion;
 *		Supported put completion (c.f. get completion);
 *		Added seq_pvName() and seq_pvMessage().
 * 22sep99,grw  Supported not re-starting timers on transition to curr. state.
 * 18feb00,wfl	Changed to use pvVarGetMess() everywhere
 * 29feb00,wfl	Converted to new OSI; new macros for resource lock / unlock
 *		(always lock if needed); completed "putComplete" logic
 * 31mar00,wfl	Used mutex for caSemId; fixed placement of flush calls
 */

#include 	<stdlib.h>
#include 	<string.h>

#define epicsExportSharedSymbols
#include	"seq.h"

/*#define		DEBUG*/

/* The following "pv" functions are included here:
	seq_pvGet()
	seq_pvGetQ()
	seq_pvFreeQ()
	seq_pvGetComplete()
	seq_pvPut()
	seq_pvPutComplete()
	seq_pvFlush()
	seq_pvMonitor()
	seq_pvStopMonitor()
	seq_pvName()
	seq_pvStatus()
	seq_pvSeverity()
	seq_pvMessage()
	seq_pvConnected()
	seq_pvAssigned()
	seq_pvChannelCount()
	seq_pvAssignCount()
	seq_pvConnectCount()
	seq_pvCount()
	seq_pvIndex()
	seq_pvTimeStamp()
 */

/* I/O completion type (extra argument passed to seq_pvGet() and seq_pvPut()) */
#define	HONOR_OPTION 0
#define ASYNCHRONOUS 1
#define SYNCHRONOUS  2

/* Macros for resource lock */
#define LOCK   epicsMutexMustLock(pSP->caSemId)
#define UNLOCK epicsMutexUnlock(pSP->caSemId)

/* Flush outstanding PV requests */
epicsShareFunc void epicsShareAPI seq_pvFlush()
{
	pvSysFlush(pvSys);
}	

/*
 * seq_pvGet() - Get DB value.
 */
epicsShareFunc long epicsShareAPI seq_pvGet(SS_ID ssId, long pvId, long compType)
{
	SPROG		*pSP;	/* ptr to state program */
	SSCB		*pSS;	/* ptr to state set */
	CHAN		*pDB;	/* ptr to channel struct */
	int		sync;	/* whether synchronous get */
	int		status;
	epicsEventWaitStatus	sem_status;
	extern		void seq_get_handler();

	pSS = (SSCB *)ssId;
	pSP = pSS->sprog;
	pDB = pSP->pChan + pvId;

	/* Determine whether performing asynchronous or synchronous i/o */
	switch (compType)
	{
	    case HONOR_OPTION:
		sync = (pSP->options & OPT_ASYNC) == 0;
		break;
	    case ASYNCHRONOUS:
		sync = FALSE;
		break;
	    case SYNCHRONOUS:
	    default:
		sync = TRUE;
		break;
	}

	/* Flag this pvGet() as not completed */
	pDB->getComplete = FALSE;

	/* Check for channel connected */
	if (!pDB->connected)
	{
		pDB->status = pvStatDISCONN;
		pDB->severity = pvSevrINVALID;
		pDB->message = Strdcpy(pDB->message, "disconnected");
		return pDB->status;
	}

	/* Note the current state set (used in the completion callback) */
	pDB->sset = pSS;

	/* If synchronous pvGet then clear the pvGet pend semaphore */
	if (sync)
	{
		epicsEventTryWait(pSS->getSemId);
	}

	/* Perform the PV get operation with a callback routine specified */
	status = pvVarGetCallback(
			pDB->pvid,		/* PV id */
			pDB->getType,		/* request type */
			pDB->count,		/* element count */
			seq_get_handler,	/* callback handler */
			pDB);			/* user arg */
	if (status != pvStatOK)
	{
		errlogPrintf("seq_pvGet: pvVarGetCallback() %s failure: %s\n",
                            pDB->dbName, pvVarGetMess(pDB->pvid));
		pDB->getComplete = TRUE;
		return status;
	}

	/* Synchronous: wait for completion (10s timeout) */	
	if (sync)
	{
		pvSysFlush(pvSys);
		sem_status = epicsEventWaitWithTimeout(pSS->getSemId, 10.0);
		if (sem_status != epicsEventWaitOK)
		{
			pDB->status = pvStatTIMEOUT;
			pDB->severity = pvSevrMAJOR;
			pDB->message = Strdcpy(pDB->message,
						"get completion timeout");
			return pDB->status;
		}
	}
        
	return pvStatOK;
}

/*
 * seq_pvGetComplete() - returns TRUE if the last get completed.
 */
epicsShareFunc long epicsShareAPI seq_pvGetComplete(SS_ID ssId, long pvId)
{
	SPROG		*pSP;	/* ptr to state program */
	CHAN		*pDB;	/* ptr to channel struct */

	pSP = ((SSCB *)ssId)->sprog;
	pDB = pSP->pChan + pvId;
	return pDB->getComplete;
}


/*
 * seq_pvPut() - Put DB value.
 */
epicsShareFunc long epicsShareAPI seq_pvPut(SS_ID ssId, long pvId, long compType)
{
	SPROG		*pSP;	/* ptr to state program */
	SSCB		*pSS;	/* ptr to state set */
	CHAN		*pDB;	/* ptr to channel struct */
	int		nonb;	/* whether non-blocking put */
	int		sync;	/* whether synchronous put */
	int		status;
	int		count;
        epicsEventWaitStatus   sem_status;
        extern          void seq_put_handler();

	pSS = (SSCB *)ssId;
	pSP = pSS->sprog;
	pDB = pSP->pChan + pvId;
#ifdef	DEBUG
	errlogPrintf("seq_pvPut: pv name=%s, pVar=%p\n", pDB->dbName,
		pDB->pVar);
#endif	/*DEBUG*/

        /* Determine whether performing asynchronous or synchronous i/o
	   ((+a) option was never honored for put, so HONOR_OPTION
	   means non-blocking and therefore implicitly asynchronous) */
        switch (compType)
        {
            case HONOR_OPTION:
		nonb = TRUE;
                sync = FALSE;
                break;
            case ASYNCHRONOUS:
		nonb = FALSE;
                sync = FALSE;
                break;
            case SYNCHRONOUS:
            default:
		nonb = FALSE;
                sync = TRUE;
                break;
        }

	/* Flag this pvPut() as not completed */
	pDB->putComplete = FALSE;
	pDB->putWasComplete = FALSE;

        /* Check for channel connected */
	if (!pDB->connected)
	{
		pDB->status = pvStatDISCONN;
		pDB->severity = pvSevrINVALID;
		pDB->message = Strdcpy(pDB->message, "disconnected");
		return pDB->status;
	}

        /* Note the current state set (used in the completion callback) */
        pDB->sset = pSS;

        /* If synchronous pvPut then clear the pvPut pend semaphore */
        if (sync)
        {
                epicsEventTryWait(pSS->putSemId);
        }

	/* Determine number of elements to put (don't try to put more
	   than db count) */
	count  = pDB->count;
	if (count > pDB->dbCount)
		count = pDB->dbCount;

	/* Perform the PV put operation (either non-blocking or with a
	   callback routine specified) */
	if (nonb)
		status = pvVarPutNoBlock(
				pDB->pvid,              /* PV id */
				pDB->putType,           /* data type */
				count,			/* element count */
				(pvValue *)pDB->pVar);	/* data value */
	else
		status = pvVarPutCallback(
				pDB->pvid,              /* PV id */
				pDB->putType,           /* data type */
				count,			/* element count */
				(pvValue *)pDB->pVar,	/* data value */
				seq_put_handler,	/* callback handler */
				pDB);                   /* user arg */
	if (status != pvStatOK)
	{
		errlogPrintf("seq_pvPut: pvVarPut%s() %s failure: %s\n",
			    nonb ? "NoBlock" : "Callback", pDB->dbName,
			    pvVarGetMess(pDB->pvid));
		pDB->putComplete = TRUE;
		return status;
	}

        /* Synchronous: wait for completion (10s timeout) */
        if (sync)
        {
		pvSysFlush(pvSys);
                sem_status = epicsEventWaitWithTimeout(pSS->putSemId, 10.0);
                if (sem_status != epicsEventWaitOK)
                {
			pDB->status = pvStatTIMEOUT;
			pDB->severity = pvSevrMAJOR;
			pDB->message = Strdcpy(pDB->message,
						"put completion timeout");
			return pDB->status;
                }
        }

#ifdef	DEBUG
	errlogPrintf("seq_pvPut: status=%d, mess=%s\n", status,
		pvVarGetMess(pDB->pvid));
	if (status != pvStatOK)
	{
		errlogPrintf("pvPut on \"%s\" failed (%d)\n", pDB->dbName,
			     status);
		errlogPrintf("  putType=%d\n", pDB->putType);
		errlogPrintf("  size=%d, count=%d\n", pDB->size, count);
	}
#endif	/*DEBUG*/

	return pvStatOK;
}

/*
 * seq_pvPutComplete() - returns TRUE if the last put completed.
 */
epicsShareFunc long epicsShareAPI seq_pvPutComplete(SS_ID ssId, long pvId, long length, long any,
		       long *pComplete)
{
	SPROG		*pSP;	/* ptr to state program */
	SSCB		*pSS;	/* ptr to state set */
	CHAN		*pDB;	/* ptr to channel struct */
	long		anyDone, allDone;
	int		i;

	pSS = (SSCB *)ssId;
	pSP = ((SSCB *)ssId)->sprog;

	/* Apply resource lock */
	LOCK;

	anyDone = FALSE;
	allDone = TRUE;
	for (i=0, pDB = pSP->pChan + pvId; i<length; i++, pDB++)
	{
		if (pDB->putComplete)
		{
			if (!pDB->putWasComplete) anyDone = TRUE;
			pDB->putWasComplete = TRUE;
		}
		else
		{
			allDone = FALSE;
		}
		if (pComplete != NULL)
			pComplete[i] = pDB->putComplete;
	}

#ifdef DEBUG
	errlogPrintf("pvPutComplete: pvId=%ld, length=%ld, anyDone=%ld, "
		     "allDone=%ld\n", pvId, length, anyDone, allDone);
#endif

	/* Unlock resource */
	UNLOCK;

	return any?anyDone:allDone;
}
/*
 * seq_pvAssign() - Assign/Connect to a channel.
 * Assign to a zero-length string ("") disconnects/de-assigns.
 */
epicsShareFunc long epicsShareAPI seq_pvAssign(SS_ID ssId, long pvId, char *pvName)
{
	SPROG		*pSP;	/* ptr to state program */
	CHAN		*pDB;	/* ptr to channel struct */
	int		status, nchar;
	extern		void seq_conn_handler();

	pSP = ((SSCB *)ssId)->sprog;
	pDB = pSP->pChan + pvId;

#ifdef	DEBUG
	errlogPrintf("Assign %s to \"%s\"\n", pDB->pVarName, pvName);
#endif	/*DEBUG*/
	if (pDB->assigned)
	{	/* Disconnect this PV */
		status = pvVarDestroy(pDB->pvid);
		if (status != pvStatOK)
		{
			errlogPrintf("seq_pvAssign: pvVarDestroy() %s failure: "
				"%s\n", pDB->dbName, pvVarGetMess(pDB->pvid));
		}
		free(pDB->dbName);
		pDB->assigned = FALSE;
		pSP->assignCount -= 1;
	}

	if (pDB->connected)
	{
		pDB->connected = FALSE;
		pSP->connCount -= 1;
	}
	pDB->monitored = FALSE;
	nchar = strlen(pvName);
	pDB->dbName = (char *)calloc(1, nchar + 1);
	strcpy(pDB->dbName, pvName);

	/* Connect */
	if (nchar > 0)
	{
		pDB->assigned = TRUE;
		pSP->assignCount += 1;
		status = pvVarCreate(
			pvSys,			/* PV system context */
			pDB->dbName,		/* DB channel name */
			seq_conn_handler,	/* connection handler routine */
			pDB,			/* user ptr is CHAN structure */
			0,			/* debug level (inherited) */
			&pDB->pvid);		/* ptr to pvid */
		if (status != pvStatOK)
		{
			errlogPrintf("seq_pvAssign: pvVarCreate() %s failure: "
				"%s\n", pDB->dbName, pvVarGetMess(pDB->pvid));
			return status;
		}

		if (pDB->monFlag)
		{
			status = seq_pvMonitor(ssId, pvId);
			if (status != pvStatOK)
				return status;
		}
	}

	pvSysFlush(pvSys);
	
	return pvStatOK;
}
/*
 * seq_pvMonitor() - Initiate a monitor on a channel.
 */
epicsShareFunc long epicsShareAPI seq_pvMonitor(SS_ID ssId, long pvId)
{
	SPROG		*pSP;	/* ptr to state program */
	CHAN		*pDB;	/* ptr to channel struct */
	int		status;
	extern		void seq_mon_handler();

	pSP = ((SSCB *)ssId)->sprog;
	pDB = pSP->pChan + pvId;

#ifdef	DEBUG
	errlogPrintf("monitor \"%s\"\n", pDB->dbName);
#endif	/*DEBUG*/

/*	if (pDB->monitored || !pDB->assigned)	*/
/*	WFL, 96/08/07, don't check monitored because it can get set TRUE */
/*	in the connection handler before this routine is executed; this */
/*	fix pending a proper fix */
	if (!pDB->assigned)
		return pvStatOK;

	status = pvVarMonitorOn(
		 pDB->pvid,		/* pvid */
		 pDB->getType,		/* requested type */
		 pDB->count,		/* element count */
		 seq_mon_handler,	/* function to call */
		 pDB,			/* user arg (db struct) */
		 &pDB->evid);		/* where to put event id */

	if (status != pvStatOK)
	{
		errlogPrintf("seq_pvMonitor: pvVarMonitorOn() %s failure: %s\n",
                            pDB->dbName, pvVarGetMess(pDB->pvid));
		return status;
	}
	pvSysFlush(pvSys);

	pDB->monitored = TRUE;
	return pvStatOK;
}

/*
 * seq_pvStopMonitor() - Cancel a monitor
 */
epicsShareFunc long epicsShareAPI seq_pvStopMonitor(SS_ID ssId, long pvId)
{
	SPROG		*pSP;	/* ptr to state program */
	CHAN		*pDB;	/* ptr to channel struct */
	int		status;

	pSP = ((SSCB *)ssId)->sprog;
	pDB = pSP->pChan + pvId;
	if (!pDB->monitored)
		return -1;

	status = pvVarMonitorOff(pDB->pvid,pDB->evid);
	if (status != pvStatOK)
	{
		errlogPrintf("seq_pvStopMonitor: pvVarMonitorOff() %s failure: "
			    "%s\n", pDB->dbName, pvVarGetMess(pDB->pvid));
		return status;
	}

	pDB->monitored = FALSE;

	return status;
}

/*
 * seq_pvChannelCount() - returns total number of database channels.
 */
epicsShareFunc long epicsShareAPI seq_pvChannelCount(SS_ID ssId)
{
	SPROG		*pSP;	/* ptr to state program */

	pSP = ((SSCB *)ssId)->sprog;
	return pSP->numChans;
}

/*
 * seq_pvConnectCount() - returns number of database channels connected.
 */
epicsShareFunc long epicsShareAPI seq_pvConnectCount(SS_ID ssId)
{
	SPROG		*pSP;	/* ptr to state program */

	pSP = ((SSCB *)ssId)->sprog;
	return pSP->connCount;
}

/*
 * seq_pvAssignCount() - returns number of database channels assigned.
 */
epicsShareFunc long epicsShareAPI seq_pvAssignCount(SS_ID ssId)
{
	SPROG		*pSP;	/* ptr to state program */

	pSP = ((SSCB *)ssId)->sprog;
	return pSP->assignCount;
}

/*
 * seq_pvConnected() - returns TRUE if database channel is connected.
 */
epicsShareFunc long epicsShareAPI seq_pvConnected(SS_ID ssId, long pvId)
{
	SPROG		*pSP;	/* ptr to state program */
	CHAN		*pDB;

	pSP = ((SSCB *)ssId)->sprog;
	pDB = pSP->pChan + pvId;
	return pDB->connected;
}

/*
 * seq_pvAssigned() - returns TRUE if database channel is assigned.
 */
epicsShareFunc long epicsShareAPI seq_pvAssigned(SS_ID ssId, long pvId)
{
	SPROG		*pSP;	/* ptr to state program */
	CHAN		*pDB;

	pSP = ((SSCB *)ssId)->sprog;
	pDB = pSP->pChan + pvId;
	return pDB->assigned;
}

/*
 * seq_pvCount() - returns number elements in an array, which is the lesser of
 * (1) the array size and (2) the element count returned by the PV layer.
 */
epicsShareFunc long epicsShareAPI seq_pvCount(SS_ID ssId, long pvId)
{
	SPROG		*pSP;	/* ptr to state program */
	CHAN		*pDB;	/* ptr to channel struct */

	pSP = ((SSCB *)ssId)->sprog;
	pDB = pSP->pChan + pvId;
	return pDB->dbCount;
}

/*
 * seq_pvName() - returns channel name.
 */
epicsShareFunc char *epicsShareAPI seq_pvName(SS_ID ssId, long pvId)
{
	SPROG		*pSP;	/* ptr to state program */
	CHAN		*pDB;	/* ptr to channel struct */

	pSP = ((SSCB *)ssId)->sprog;
	pDB = pSP->pChan + pvId;
	return pDB->dbName;
}

/*
 * seq_pvStatus() - returns channel alarm status.
 */
epicsShareFunc long epicsShareAPI seq_pvStatus(SS_ID ssId, long pvId)
{
	SPROG		*pSP;	/* ptr to state program */
	CHAN		*pDB;	/* ptr to channel struct */

	pSP = ((SSCB *)ssId)->sprog;
	pDB = pSP->pChan + pvId;
	return pDB->status;
}

/*
 * seq_pvSeverity() - returns channel alarm severity.
 */
epicsShareFunc long epicsShareAPI seq_pvSeverity(SS_ID ssId, long pvId)
{
	SPROG		*pSP;	/* ptr to state program */
	CHAN		*pDB;	/* ptr to channel struct */

	pSP = ((SSCB *)ssId)->sprog;
	pDB = pSP->pChan + pvId;
	return pDB->severity;
}

/*
 * seq_pvMessage() - returns channel error message.
 */
epicsShareFunc char *epicsShareAPI seq_pvMessage(SS_ID ssId, long pvId)
{
	SPROG		*pSP;	/* ptr to state program */
	CHAN		*pDB;	/* ptr to channel struct */

	pSP = ((SSCB *)ssId)->sprog;
	pDB = pSP->pChan + pvId;
	return pDB->message;
}

/*
 * seq_pvIndex() - returns index of database variable.
 */
epicsShareFunc long epicsShareAPI seq_pvIndex(SS_ID ssId, long pvId)
{
	return pvId; /* index is same as pvId */
}

/*
 * seq_pvTimeStamp() - returns channel time stamp.
 */
epicsShareFunc epicsTimeStamp epicsShareAPI seq_pvTimeStamp(SS_ID ssId, long pvId)
{
	SPROG		*pSP;	/* ptr to state program */
	CHAN		*pDB;	/* ptr to channel struct */

	pSP = ((SSCB *)ssId)->sprog;
	pDB = pSP->pChan + pvId;
	return pDB->timeStamp;
}
/*
 * seq_efSet() - Set an event flag, then wake up each state
 * set that might be waiting on that event flag.
 */
epicsShareFunc void epicsShareAPI seq_efSet(SS_ID ssId, long ev_flag)
{
	SPROG		*pSP;
	SSCB		*pSS;

	pSS = (SSCB *)ssId;
	pSP = pSS->sprog;

	/* Apply resource lock */
	LOCK;

#ifdef	DEBUG
	errlogPrintf("seq_efSet: pSP=%p, pSS=%p, ev_flag=%p\n", pSP, pSS,
		ev_flag);
#endif	/*DEBUG*/

	/* Set this bit */
	bitSet(pSP->pEvents, ev_flag);

	/* Wake up state sets that are waiting for this event flag */
	seqWakeup(pSP, ev_flag);

	/* Unlock resource */
	UNLOCK;
}

/*
 * seq_efTest() - Test event flag against outstanding events.
 */
epicsShareFunc long epicsShareAPI seq_efTest(SS_ID ssId, long ev_flag)
/* event flag */
{
	SPROG		*pSP;
	SSCB		*pSS;
	int		isSet;

	pSS = (SSCB *)ssId;
	pSP = pSS->sprog;

	/* Apply resource lock */
	LOCK;

	isSet = bitTest(pSP->pEvents, ev_flag);

#ifdef	DEBUG
	errlogPrintf("seq_efTest: ev_flag=%d, event=%p, isSet=%d\n",
		ev_flag, pSP->pEvents[0], isSet);
#endif	/*DEBUG*/

	/* Unlock resource */
	UNLOCK;

	return isSet;
}

/*
 * seq_efClear() - Test event flag against outstanding events, then clear it.
 */
epicsShareFunc long epicsShareAPI seq_efClear(SS_ID ssId, long ev_flag)
{
	SPROG		*pSP;
	SSCB		*pSS;
	int		isSet;

	pSS = (SSCB *)ssId;
	pSP = pSS->sprog;

	isSet = bitTest(pSP->pEvents, ev_flag);

	/* Apply resource lock */
	LOCK;

	/* Clear this bit */
	bitClear(pSP->pEvents, ev_flag);

	/* Wake up state sets that are waiting for this event flag */
	seqWakeup(pSP, ev_flag);

	/* Unlock resource */
	UNLOCK;

	return isSet;
}

/*
 * seq_efTestAndClear() - Test event flag against outstanding events, then clear it.
 */
epicsShareFunc long epicsShareAPI seq_efTestAndClear(SS_ID ssId, long ev_flag)
{
	SPROG		*pSP;
	SSCB		*pSS;
	int		isSet;

	pSS = (SSCB *)ssId;
	pSP = pSS->sprog;

	/* Apply resource lock */
	LOCK;

	isSet = bitTest(pSP->pEvents, ev_flag);
	bitClear(pSP->pEvents, ev_flag);

	/* Unlock resource */
	UNLOCK;

	return isSet;
}

/*
 * seq_pvGetQ() - Get queued DB value (looks like pvGet() but really more
 *		  like efTestAndClear()).
 */
epicsShareFunc int epicsShareAPI seq_pvGetQ(SS_ID ssId, int pvId)

{
	SPROG		*pSP;
	SSCB		*pSS;
	CHAN		*pDB;
	int		ev_flag;
	int		isSet;

	pSS = (SSCB *)ssId;
	pSP = pSS->sprog;
	pDB = pSP->pChan + pvId;

	/* Apply resource lock */
	LOCK;

	/* Determine event flag number and whether set */
	ev_flag = pDB->efId;
	isSet = bitTest(pSP->pEvents, ev_flag);

#ifdef	DEBUG
	errlogPrintf("seq_pvGetQ: pv name=%s, isSet=%d\n", pDB->dbName, isSet);
#endif	/*DEBUG*/

	/* If set, queue should be non empty */
	if (isSet)
	{
		QENTRY	*pEntry;
		pvValue	*pAccess;
		void	*pVal;

		/* Dequeue first entry */
		pEntry = (QENTRY *) ellGet(&pSP->pQueues[pDB->queueIndex]);

		/* If none, "impossible" */
		if (pEntry == NULL)
		{
			errlogPrintf("seq_pvGetQ: evflag set but queue empty "
				"(impossible)\n");
			isSet = FALSE;
		}

		/* Extract information from entry (code from seq_ca.c)
		   (pDB changed to refer to channel for which monitor
		   was posted) */
		else
		{
			pDB = pEntry->pDB;
			pAccess = &pEntry->value;
	
			/* Copy value returned into user variable */
			/* For now, can only return _one_ array element */
			pVal = (char *)pAccess + pDB->dbOffset;
			memcpy(pDB->pVar, pVal, pDB->size * 1 );
							/* was pDB->dbCount */
	 
			/* Copy status & severity */
			pDB->status = pAccess->timeStringVal.status;
			pDB->severity = pAccess->timeStringVal.severity;
		 
			/* Copy time stamp */
			pDB->timeStamp = pAccess->timeStringVal.stamp;

			/* Free queue entry */
			free(pEntry);
		}
	}

	/* If queue is now empty, clear the event flag */
	if (ellCount(&pSP->pQueues[pDB->queueIndex]) == 0)
	{
		bitClear(pSP->pEvents, ev_flag);
	}

	/* Unlock resource */
	UNLOCK;

	/* return TRUE iff event flag was set on entry */
	return isSet;
}

/*
 * seq_pvFreeQ() - Free elements on syncQ queue and clear event flag.
 *		   Intended to be called from action code.
 */
epicsShareFunc int epicsShareAPI seq_pvFreeQ(SS_ID	ssId, int	pvId)
{
	SPROG		*pSP;
	SSCB		*pSS;
	CHAN		*pDB;
	QENTRY		*pEntry;
	int		ev_flag;

	pSS = (SSCB *)ssId;
	pSP = pSS->sprog;
	pDB = pSP->pChan + pvId;

	/* Apply resource lock */
	LOCK;

#ifdef	DEBUG
	errlogPrintf("seq_pvFreeQ: pv name=%s, count=%d\n", pDB->dbName,
		ellCount(&pSP->pQueues[pDB->queueIndex]));
#endif	/*DEBUG*/

	/* Determine event flag number */
	ev_flag = pDB->efId;

	/* Free queue elements */
	while((pEntry = (QENTRY *)
			    ellGet(&pSP->pQueues[pDB->queueIndex])) != NULL)
	    free(pEntry);

	/* Clear event flag */
	bitClear(pSP->pEvents, ev_flag);

	/* Unlock resource */
	UNLOCK;

	return 0;
}


/* seq_delay() - test for delay() time-out expired */
epicsShareFunc long epicsShareAPI seq_delay(SS_ID ssId, long delayId)
{
	SSCB		*pSS;
	double		timeNow, timeElapsed;
        long            expired = FALSE;

	pSS = (SSCB *)ssId;

	/* Calc. elapsed time since state was entered */
	pvTimeGetCurrentDouble( &timeNow );
	timeElapsed = timeNow - pSS->timeEntered;

	/* Check for delay timeout */
	if ( (timeElapsed >= pSS->delay[delayId]) )
	{
		pSS->delayExpired[delayId] = TRUE; /* mark as expired */
		expired = TRUE;
	}
#if defined(DEBUG)
	errlogPrintf("Delay %ld : %ld ticks, %s\n",delayId,pSS->delay[delayId],
		expired ? "expired": "unexpired");
#endif
	return expired;
}

/*
 * seq_delayInit() - initialize delay time (in seconds) on entering a state.
 */
epicsShareFunc void epicsShareAPI seq_delayInit(SS_ID ssId, long delayId, double delay)
{
	SSCB		*pSS;
	int		ndelay;

	pSS = (SSCB *)ssId;

	/* Save delay time */
	pSS->delay[delayId] = delay;

	ndelay = delayId + 1;
	if (ndelay > pSS->numDelays)
		pSS->numDelays = ndelay;
}
/*
 * seq_optGet: return the value of an option (e.g. "a").
 * FALSE means "-" and TRUE means "+".
 */
epicsShareFunc long epicsShareAPI seq_optGet(SS_ID ssId, char *opt)
{
	SPROG		*pSP;

	pSP = ((SSCB *)ssId)->sprog;
	switch (opt[0])
	{
	    case 'a': return ( (pSP->options & OPT_ASYNC) != 0);
	    case 'c': return ( (pSP->options & OPT_CONN)  != 0);
	    case 'd': return ( (pSP->options & OPT_DEBUG) != 0);
	    case 'e': return ( (pSP->options & OPT_NEWEF) != 0);
	    case 'm': return ( (pSP->options & OPT_MAIN)  != 0);
	    case 'r': return ( (pSP->options & OPT_REENT) != 0);
	    default:  return FALSE;
	}
}

