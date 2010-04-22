/*
	seq_ca.c,v 1.2 1995/06/27 15:25:54 wright Exp

 *   DESCRIPTION: Process Variable (was CA) interface for sequencer.
 *
 *	Author:  Andy Kozubal
 *	Date:    July, 1991
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
 * 03jul91,ajk	.
 * 11dec91,ajk	Cosmetic changes (comments & names)
 * 13feb92,ajk	All seqLog() calls compile only if DEBUG is defined.
 * 28apr92,ajk	Implemented new event flag mode.
 * 21may92,ajk	Will periodically announce number of connected channels
 *		if waiting form some to connect.
 * 17feb93,ajk	Implemented code to allow sharing of a single CA task by
 *		all state programs.  Added seq_disconnect() and ca_import_
 *		cancel(). DB name resolution was moved to seq_main.c.
 * 19feb93,ajk	Added patched version of VxWorks 5.02b taskVarDelete().
 * 01mar94,ajk	Moved "seq_pv*()" functions to seq_if.c.
 * 28mar94,ajk	Restructured event& callback handlers to call proc_db_events().
 * 29mar94,ajk	Removed getPtrToValue().  Offset is now in db_channel structure.
 * 08apr94,ajk	Added support for time stamp.
 * 17jan96,ajk	Removed ca_import_cancel(), which is now in channel access lib.
 * 17jan96,ajk	Many routines changed to use ANSI-style function headers.
 * 09aug96,wfl	Added support for syncQ queued values.
 * 30apr99,wfl	Replaced VxWorks task dependencies with OSI.
 * 17may99,wfl	Fixed errors in debug output; checked for NULL mask pointer
 *		(NULL mask pointer bug had always been present).
 * 07sep99,wfl	Supported put completion (c.f. existing get completion);
 *		Copied message text (returned via seq_pvMessage());
 *		On completion, unconditionally give semaphore.
 * 18feb00,wfl	Supported simple types for putCallback (fixed bug...).
 * 29feb00,wfl	Converted to new OSI (and errlogPrintf).
 * 31mar00,wfl	Checked for NULL value returned on put completion.
 */

#include	<string.h>

#define epicsExportSharedSymbols
#include	"seq.h"

LOCAL void proc_db_events(pvValue *, pvType, CHAN *, long);
LOCAL void proc_db_events_queued(pvValue *, CHAN *);

/*#define		DEBUG*/

#ifdef		DEBUG
#undef		LOCAL
#define		LOCAL
#endif		/*DEBUG*/
/*
 * seq_connect() - Connect to all database channels.
 */
epicsShareFunc long seq_connect(SPROG *pSP)
{
	CHAN		*pDB;
	int		status, i;
	extern		void seq_conn_handler();

	for (i = 0, pDB = pSP->pChan; i < pSP->numChans; i++, pDB++)
	{
		if (pDB->dbName == NULL || pDB->dbName[0] == 0)
			continue; /* skip records without pv names */
		pSP->assignCount += 1; /* keep track of number of *assigned* channels */
		if (pDB->monFlag) pSP->numMonitoredChans++;/*do it before pvVarCreate*/
	}
	/*
	 * For each channel: connect to db & issue monitor (if monFlag is TRUE).
	 */
	for (i = 0, pDB = pSP->pChan; i < pSP->numChans; i++, pDB++)
	{
		if (pDB->dbName == NULL || pDB->dbName[0] == 0)
			continue; /* skip records without pv names */
#ifdef	DEBUG
		errlogPrintf("seq_connect: connect %s to %s\n", pDB->pVarName,
			pDB->dbName);
#endif	/*DEBUG*/
		/* Connect to it */
		status = pvVarCreate(
			pvSys,			/* PV system context */
			pDB->dbName,		/* PV name */
			seq_conn_handler,	/* connection handler routine */
			pDB,			/* private data is CHAN struc */
			0,			/* debug level (inherited) */
			&pDB->pvid );		/* ptr to PV id */
		if (status != pvStatOK)
		{
			errlogPrintf("seq_connect: pvVarCreate() %s failure: "
				"%s\n", pDB->dbName, pvVarGetMess(pDB->pvid));
			return status;
		}
		pDB->assigned = TRUE;
		/* Clear monitor indicator */
		pDB->monitored = FALSE;

		/*
		 * Issue monitor request
		 */
		if (pDB->monFlag)
		{
			seq_pvMonitor((SS_ID)pSP->pSS, i);
		}
	}
	pvSysFlush(pvSys);
	return 0;
}

/*
 * Event completion type (extra argument passed to proc_db_events().
 */
#define	GET_COMPLETE 0
#define	PUT_COMPLETE 1
#define	MON_COMPLETE 2

/*
 * seq_get_handler() - Sequencer callback handler.
 * Called when a "get" completes.
 */
epicsShareFunc void seq_get_handler(void *var, pvType type, int count, pvValue *pValue,
		     void *arg)
{
	/* Process event handling in each state set */
	proc_db_events(pValue, type, (CHAN *)arg, GET_COMPLETE);
}

/*
 * seq_put_handler() - Sequencer callback handler.
 * Called when a "put" completes.
 */
epicsShareFunc void seq_put_handler(void *var, pvType type, int count, pvValue *pValue,
		     void *arg)
{
	/* Process event handling in each state set */
	proc_db_events(pValue, type, (CHAN *)arg, PUT_COMPLETE);
}

/*
 * seq_mon_handler() - PV events (monitors) come here.
 */
epicsShareFunc void seq_mon_handler(void *var, pvType type, int count, pvValue *pValue,
		     void *arg)
{
	CHAN *pCHAN = (CHAN *)arg;
        SPROG *pSP = pCHAN->sprog;

	/* Process event handling in each state set */
	proc_db_events(pValue, type, pCHAN, MON_COMPLETE);
        if(!pCHAN->gotFirstMonitor) {
		pCHAN->gotFirstMonitor = 1;
		pSP->firstMonitorCount++;
		if((pSP->firstMonitorCount==pSP->numMonitoredChans)
		&& (pSP->firstConnectCount==pSP->assignCount)) {
			SSCB *pSS;
			int i;
			for(i=0, pSS=pSP->pSS; i<pSP->numSS; i++,pSS++) {
				epicsEventSignal(pSS->allFirstConnectAndMonitorSemId);
			}
		}
	}
}

/* Common code for completion and monitor handling */
LOCAL void proc_db_events(pvValue *pValue, pvType type, CHAN *pDB,
			  long complete_type)
{
	SPROG			*pSP;
	void			*pVal;

#ifdef	DEBUG
	errlogPrintf("proc_db_events: var=%s, pv=%s, type=%s\n", pDB->pVarName,
	    pDB->dbName, complete_type==0?"get":complete_type==1?"put":"mon");
#endif	/*DEBUG*/

	/* If monitor on var queued via syncQ, branch to alternative routine */
	if (pDB->queued && complete_type == MON_COMPLETE)
	{
	    proc_db_events_queued(pValue, pDB);
	    return;
	}

	/* Copy value returned into user variable (can get NULL value pointer
	   for put completion only) */
	if (pValue == NULL)
	    ;
	else if (PV_SIMPLE(type))
	{
	    pVal = (void *)pValue; /* ptr to data */
	    memcpy(pDB->pVar, pVal, pDB->size * pDB->dbCount);
	}
	else
	{
	    pVal = (void *)((long)pValue + pDB->dbOffset); /* ptr to data */
	    memcpy(pDB->pVar, pVal, pDB->size * pDB->dbCount);
	}

	/* Copy status, severity and time stamp (leave unchanged if absent) */
	if (pValue != NULL && !PV_SIMPLE(type))
	{
	    pDB->status = (short) pValue->timeStringVal.status;
	    pDB->severity = (short) pValue->timeStringVal.severity;
	    pDB->timeStamp = pValue->timeStringVal.stamp;
	}

	/* Copy error message (only when severity indicates error) */
	if (pDB->severity != pvSevrNONE)
	{
	    char *pmsg = pvVarGetMess(pDB->pvid);
	    if (!pmsg) pmsg = "unknown";
	    pDB->message = Strdcpy(pDB->message, pmsg);
	}

	/* Get ptr to the state program that owns this db entry */
	pSP = pDB->sprog;

	/* Indicate completed pvGet() or pvPut()  */
	switch (complete_type)
	{
	    case GET_COMPLETE:
		pDB->getComplete = TRUE;
		break;
	    case PUT_COMPLETE:
		pDB->putComplete = TRUE;
		break;
	    default:
		break;
	}

	/* Wake up each state set that uses this channel in an event */
	seqWakeup(pSP, pDB->eventNum);

	/* If there's an event flag associated with this channel, set it */
	if (pDB->efId > 0)
		seq_efSet((SS_ID)pSP->pSS, pDB->efId);

	/* Give semaphore for completed synchronous pvGet() and pvPut() */
	switch (complete_type)
	{
	    case GET_COMPLETE:
		if (pDB->sset != NULL)
			epicsEventSignal(pDB->sset->getSemId);
		break;
	    case PUT_COMPLETE:
		if (pDB->sset != NULL)
			epicsEventSignal(pDB->sset->putSemId);
		break;
	    default:
		break;
	}

	return;
}
/* Common code for event and callback handling (queuing version) */
LOCAL void proc_db_events_queued(pvValue *pValue, CHAN *pDB)
{
	QENTRY			*pEntry;
	SPROG			*pSP;
	int			count;

	/* Get ptr to the state program that owns this db entry */
	pSP = pDB->sprog;

	/* Determine number of items currently on the queue */
	count = ellCount(&pSP->pQueues[pDB->queueIndex]);

#ifdef	DEBUG
	errlogPrintf("proc_db_events_queued: var=%s, pv=%s, count(max)=%d(%d), "
	       "index=%d\n", pDB->pVarName, pDB->dbName, count,
	       pDB->maxQueueSize, pDB->queueIndex);
#endif	/*DEBUG*/

	/* Allocate queue entry (re-use last one if queue has reached its
	   maximum size) */
	if ( count < pDB->maxQueueSize )
	{
		pEntry = (QENTRY *) calloc(sizeof(QENTRY), 1);
		if (pEntry == NULL)
		{
			errlogPrintf("proc_db_events_queued: %s queue memory "
			       "allocation failure\n", pDB->pVarName);
			return;
		}
		ellAdd(&pSP->pQueues[pDB->queueIndex], (ELLNODE *) pEntry);
	}
	else
	{
		pEntry = (QENTRY *) ellLast(&pSP->pQueues[pDB->queueIndex]);
		if (pEntry == NULL)
		{
			errlogPrintf("proc_db_events_queued: %s queue "
				"inconsistent failure\n", pDB->pVarName);
			return;
		}
	}

	/* Copy channel id, value and associated information into queue
	   entry (NB, currently only copy _first_ value for arrays) */
	pEntry->pDB = pDB;
	memcpy((char *)&pEntry->value, (char *)pValue, sizeof(pEntry->value));

	/* Set the event flag associated with this channel */
#ifdef DEBUG
	errlogPrintf("setting event flag %d\n", pDB->efId);
#endif /*DEBUG*/
	seq_efSet((SS_ID)pSP->pSS, pDB->efId);

	return;
}

/*	Disconnect all database channels */
/*#define	DEBUG_DISCONNECT*/

epicsShareFunc long seq_disconnect(SPROG *pSP)
{
	CHAN	*pDB;
	int	i;
	SPROG	*pMySP; /* will be NULL if this is not a sequencer thread */

	/* Did we already disconnect? */
	if (pSP->connCount < 0)
		return 0;

	/* Attach to PV context of pvSys creator (auxiliary thread) */
	pMySP = seqFindProg(epicsThreadGetIdSelf());
	if (pMySP == NULL)
	{
#ifdef	DEBUG_DISCONNECT
		errlogPrintf("seq_disconnect: pvSysAttach(pvSys)\n");
#endif	/*DEBUG_DISCONNECT*/
		/* not a sequencer thread */
		pvSysAttach(pvSys);
	}

	pDB = pSP->pChan;
#ifdef	DEBUG_DISCONNECT
	errlogPrintf("seq_disconnect: pSP = %p, pDB = %p\n", pSP, pDB);
#endif	/*DEBUG_DISCONNECT*/

	for (i = 0; i < pSP->numChans; i++, pDB++)
	{
		pvStat	status;

		if (!pDB->assigned)
			continue;
#ifdef	DEBUG_DISCONNECT
		errlogPrintf("seq_disconnect: disconnect %s from %s\n",
 			pDB->pVarName, pDB->dbName);
#endif	/*DEBUG_DISCONNECT*/
		/* Disconnect this PV */
		status = pvVarDestroy(pDB->pvid);
		if (status != pvStatOK)
		    errlogPrintf("seq_disconnect: pvVarDestroy() %s failure: "
				"%s\n", pDB->dbName, pvVarGetMess(pDB->pvid));

		/* Clear monitor & connect indicators */
		pDB->monitored = FALSE;
		pDB->connected = FALSE;
	}

	pSP->connCount = -1; /* flag to indicate all disconnected */

	pvSysFlush(pvSys);

	return 0;
}

/*
 * seq_conn_handler() - Sequencer connection handler.
 * Called each time a connection is established or broken.
 */
void seq_conn_handler(void *var,int connected)
{
	CHAN		*pDB;
	SPROG		*pSP;

	/* Private data is db ptr (specified at pvVarCreate()) */
	pDB = (CHAN *)pvVarGetPrivate(var);

	/* State program that owns this db entry */
	pSP = pDB->sprog;

	/* If PV not connected */
	if (!connected)
	{
#ifdef	DEBUG
		errlogPrintf("%s disconnected from %s\n", pDB->pVarName,
			pDB->dbName);
#endif	/*DEBUG*/
		if(pDB->connected) {
			pDB->connected = FALSE;
			pSP->connCount--;
			pDB->monitored = FALSE;
		} else {
			printf("%s disconnected but already disconnected %s\n",
				pDB->pVarName,pDB->dbName);
		}
	}
	else	/* PV connected */
	{
#ifdef	DEBUG
		errlogPrintf("%s connected to %s\n", pDB->pVarName,pDB->dbName);
#endif	/*DEBUG*/
                if(!pDB->connected) {
			pDB->connected = TRUE;
			pSP->connCount++;
			if (pDB->monFlag)
				pDB->monitored = TRUE;
			pDB->dbCount = pvVarGetCount(var);
			if (pDB->dbCount > pDB->count)
				pDB->dbCount = pDB->count;
		} else {
			printf("%s connected but already connected %s\n",
				pDB->pVarName,pDB->dbName);
		}
	        if(!pDB->gotFirstConnect) {
			pDB->gotFirstConnect = 1;
			pSP->firstConnectCount++;
			if((pSP->firstMonitorCount==pSP->numMonitoredChans)
			&& (pSP->firstConnectCount==pSP->assignCount)) {
				SSCB *pSS;
				int i;
				for(i=0, pSS=pSP->pSS; i<pSP->numSS; i++,pSS++) {
					epicsEventSignal(pSS->allFirstConnectAndMonitorSemId);
				}
			}
		}
	}

	/* Wake up each state set that is waiting for event processing */
	seqWakeup(pSP, 0);
	
	return;
}

/*
 * seqWakeup() -- wake up each state set that is waiting on this event
 * based on the current event mask.   EventNum = 0 means wake all state sets.
 */
void seqWakeup(SPROG *pSP, long eventNum)
{
	int		nss;
	SSCB		*pSS;

	/* Check event number against mask for all state sets: */
	for (nss = 0, pSS = pSP->pSS; nss < pSP->numSS; nss++, pSS++)
	{
		/* If event bit in mask is set, wake that state set */
		if ( (eventNum == 0) || 
		     ( pSS->pMask != NULL && bitTest(pSS->pMask, eventNum) ) )
		{
			epicsEventSignal(pSS->syncSemId); /* wake up ss thread */
		}

	}
	return;
}
