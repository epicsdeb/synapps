/*recDynLink.c*/
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/


/*
 * 02/11/00  tmm Added checks to recDynLinkPutCallback, so user won't hang
 *               waiting for a callback that will never come.
 * 03/08/01  tmm Guarantee that client gets search callback before monitor
 *               callback
 * 11/12/03  tmm Fixed mem leak.  (Wasn't calling epicsMutexDestroy.)
 *               Changed ring buffer to message queue.  DEBUG macro.
 * 04/19/06  tmm If recDynLinkClear found that the PV whose channel it wanted
 *               to clear had a queued action, it would loop forever waiting for
 *               the action to be dispatched, because it would not give recDynOut
 *               any processing time to dispatch the action.
 * 08/21/06  tmm CA callback functions were not checking the status field of
 *               the event_handler_args argument they were being passed, and
 *               this resulted in a crash if they tried to use other elements
 *               of the structure when eha.status != ECA_NORMAL.
 *
 */

#include <epicsMessageQueue.h>
#include <epicsMutex.h>
#include <epicsThread.h>
#include <epicsEvent.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <taskwd.h>
#include <dbDefs.h>

#include <dbAddr.h>
/* #include <dbAccessDefs.h> */
epicsShareFunc long epicsShareAPI dbNameToAddr(const char *pname,struct dbAddr *); 

#include <epicsPrint.h>
#include <db_access.h>
#include <db_access_routines.h>
#include <cadef.h>
#include <caerr.h>
#include <caeventmask.h>
/* not in 3.15.0.1 #include <tsDefs.h> */
#include <errlog.h>
#include <epicsExport.h>
#include <epicsExit.h>
#include "recDynLink.h"

volatile int recDynINPCallPendEvent = 1;
volatile int recDynINPCallPendEventTime_ms = 100;
volatile int recDynINPCallPendIoTime_ms = 100;
volatile int recDynOUTCallPend = 0;
volatile int recDynOUTCallFlush = 1;

volatile int recDynLinkDebug = 0;
epicsExportAddress(int, recDynLinkDebug);

/*Definitions to map between old and new database access*/
/*because we are using CA must include db_access.h*/
/* new field types */
#define newDBF_STRING	0
#define	newDBF_CHAR	1
#define	newDBF_UCHAR	2
#define	newDBF_SHORT	3
#define	newDBF_USHORT	4
#define	newDBF_LONG	5
#define	newDBF_ULONG	6
#define	newDBF_FLOAT	7
#define	newDBF_DOUBLE	8
#define	newDBF_ENUM	9

/* new data request buffer types */
#define newDBR_STRING      newDBF_STRING
#define newDBR_CHAR        newDBF_CHAR
#define newDBR_UCHAR       newDBF_UCHAR
#define newDBR_SHORT       newDBF_SHORT
#define newDBR_USHORT      newDBF_USHORT
#define newDBR_LONG        newDBF_LONG
#define newDBR_ULONG       newDBF_ULONG
#define newDBR_FLOAT       newDBF_FLOAT
#define newDBR_DOUBLE      newDBF_DOUBLE
#ifndef newDBR_ENUM
#define newDBR_ENUM        newDBF_ENUM
#endif
#define VALID_newDB_REQ(x) ((x >= 0) && (x <= newDBR_ENUM))
static short mapNewToOld[newDBR_ENUM+1] = {
	DBF_STRING,DBF_CHAR,DBF_CHAR,DBF_SHORT,DBF_SHORT,
	DBF_LONG,DBF_LONG,DBF_FLOAT,DBF_DOUBLE,DBF_ENUM};

int   recDynLinkQsize = 256;
epicsExportAddress(int, recDynLinkQsize);
LOCAL int shutting_down = 0;
LOCAL epicsThreadId inpTaskId=NULL;
LOCAL epicsThreadId	outTaskId=NULL;
LOCAL epicsEventId	wakeUpEvt;
epicsMessageQueueId	recDynLinkInpMsgQ = NULL;
epicsMessageQueueId	recDynLinkOutMsgQ = NULL;

typedef enum{cmdSearch,cmdClear,cmdPut,cmdPutCallback,cmdGetCallback} cmdType;
char commands[5][15] = {"Search","Clear","Put","PutCallback","GetCallback"};
typedef enum{ioInput,ioOutput} ioType;
typedef enum{stateStarting,stateSearching,stateGetting,stateConnected} stateType;

typedef struct dynLinkPvt{
    epicsMutexId	lock;
    char		*pvname;
    chid		chid;
    evid		evid;
    recDynCallback	searchCallback;
    recDynCallback	monitorCallback;
    recDynCallback	notifyCallback;
	recDynCallback	userGetCallback;
	short notifyInProgress;
    TS_STAMP		timestamp;
    short		status;
    short		severity;
    void		*pbuffer;
    size_t		nRequest;
    short		dbrType;
    double		graphicLow,graphHigh;
    double		controlLow,controlHigh;
    char		units[MAX_UNITS_SIZE];
    short		precision;
    ioType		io;
    stateType		state;
    short		scalar;
} dynLinkPvt;

/* For cmdClear data is pdynLinkPvt. For all other commands precDynLink */
typedef struct {
	union {
		recDynLink	*precDynLink;
		dynLinkPvt	*pdynLinkPvt;
	} data;
	cmdType	cmd;
} msgQCmd;

LOCAL void recDynLinkStartTasks(void);
LOCAL void connectCallback(struct connection_handler_args cha);
LOCAL void getCallback(struct event_handler_args eha);
LOCAL void monitorCallback(struct event_handler_args eha);
LOCAL void notifyCallback(struct event_handler_args eha);
LOCAL void recDynLinkInp(void);
LOCAL void recDynLinkOut(void);


void exit_handler(void *arg) {
	shutting_down = 1;
}

long epicsShareAPI recDynLinkAddInput(recDynLink *precDynLink,char *pvname,
	short dbrType,int options,
	recDynCallback searchCallback,recDynCallback monitorCallback)
{
	dynLinkPvt	*pdynLinkPvt;
	DBADDR		dbaddr;
	msgQCmd		cmd;

	if (recDynLinkDebug > 10)
            printf("recDynLinkAddInput: precDynLink=%p, pvname='%s'\n",
				(void *)precDynLink, pvname);
	if (precDynLink==NULL) {
		printf("recDynLinkAddInput: precDynLink is NULL.\n");
		return(-1);
	}
	if (pvname==NULL) {
		printf("recDynLinkAddInput: pvname was not supplied.\n");
		return(-1);
	}
	if (*pvname=='\0') {
		printf("recDynLinkAddInput: pvname is blank\n");
		return(-1);
	}
	if (options&rdlDBONLY  && dbNameToAddr(pvname,&dbaddr)) return(-1);
	if (!inpTaskId) recDynLinkStartTasks();
	if (precDynLink->pdynLinkPvt) {
		if (recDynLinkDebug > 10)
                    printf("recDynLinkAddInput: clearing old pdynLinkPvt\n"); 
		recDynLinkClear(precDynLink);
	}
	pdynLinkPvt = (dynLinkPvt *)calloc(1,sizeof(dynLinkPvt));
	if (!pdynLinkPvt) {
		printf("recDynLinkAddInput can't allocate storage");
		epicsThreadSuspendSelf();
	}
	pdynLinkPvt->lock = epicsMutexMustCreate();
	precDynLink->pdynLinkPvt = pdynLinkPvt;
	pdynLinkPvt->pvname = pvname;
	pdynLinkPvt->dbrType = dbrType;
	pdynLinkPvt->searchCallback = searchCallback;
	pdynLinkPvt->monitorCallback = monitorCallback;
	pdynLinkPvt->io = ioInput;
	pdynLinkPvt->scalar = (options&rdlSCALAR) ? TRUE : FALSE;
	pdynLinkPvt->state = stateStarting;
	cmd.data.precDynLink = precDynLink;
	cmd.cmd = cmdSearch;
	precDynLink->onQueue++;
	if (epicsMessageQueueTrySend(recDynLinkInpMsgQ, (void *)&cmd, sizeof(cmd))) {
		errMessage(0,"recDynLinkAddInput: epicsMessageQueueTrySend error");
		precDynLink->onQueue--;
	}
	return(0);
}

long epicsShareAPI recDynLinkAddOutput(recDynLink *precDynLink,char *pvname,
	short dbrType, int options, recDynCallback searchCallback)
{
	dynLinkPvt	*pdynLinkPvt;
	DBADDR		dbaddr;
	msgQCmd		cmd;
    
	if (recDynLinkDebug > 10) 
            printf("recDynLinkAddOutput: precDynLink=%p, pvname='%s'\n",
				(void *)precDynLink, pvname);
	if (precDynLink==NULL) {
		printf("recDynLinkAddInput: precDynLink is NULL.\n");
		return(-1);
	}
	if (pvname==NULL) {
		printf("recDynLinkAddOutput: pvname was not supplied.\n");
		return(-1);
	}
	if (*pvname=='\0') {
		printf("recDynLinkAddOutput: pvname is empty\n");
		return(-1);
	}
	if (options&rdlDBONLY  && dbNameToAddr(pvname,&dbaddr)) return(-1);
	if (!outTaskId) recDynLinkStartTasks();
	if (precDynLink->pdynLinkPvt) {
		if (recDynLinkDebug > 10) 
                    printf("recDynLinkAddOutput: clearing old pdynLinkPvt\n"); 
		recDynLinkClear(precDynLink);
	}
	pdynLinkPvt = (dynLinkPvt *)calloc(1,sizeof(dynLinkPvt));
	if (!pdynLinkPvt) {
		printf("recDynLinkAddOutput can't allocate storage");
		epicsThreadSuspendSelf();
	}
	pdynLinkPvt->lock = epicsMutexMustCreate();
	precDynLink->pdynLinkPvt = pdynLinkPvt;
	pdynLinkPvt->pvname = pvname;
	pdynLinkPvt->dbrType = dbrType;
	pdynLinkPvt->searchCallback = searchCallback;
	pdynLinkPvt->io = ioOutput;
	pdynLinkPvt->scalar = (options&rdlSCALAR) ? TRUE : FALSE;
	pdynLinkPvt->state = stateStarting;
	cmd.data.precDynLink = precDynLink;
	cmd.cmd = cmdSearch;
	precDynLink->onQueue++;
	if (epicsMessageQueueTrySend(recDynLinkOutMsgQ, (void *)&cmd, sizeof(cmd))) {
		errMessage(0,"recDynLinkAddOutput: epicsMessageQueueTrySend error");
		precDynLink->onQueue--;
	}
	epicsEventSignal(wakeUpEvt);
	return(0);
}

long epicsShareAPI recDynLinkClear(recDynLink *precDynLink)
{
	dynLinkPvt	*pdynLinkPvt;
	msgQCmd	cmd;
	int i;

	if (recDynLinkDebug > 10) 
            printf("recDynLinkClear: precDynLink=%p\n", (void *)precDynLink);
	pdynLinkPvt = precDynLink->pdynLinkPvt;
	if (!pdynLinkPvt) {
		printf("recDynLinkClear: recDynLinkSearch was never called\n");
		epicsThreadSuspendSelf();
	}
	if (pdynLinkPvt->chid) ca_set_puser(pdynLinkPvt->chid, NULL);
	cmd.data.pdynLinkPvt = pdynLinkPvt;
	cmd.cmd = cmdClear;
	if (precDynLink->onQueue) {
		if (recDynLinkDebug > 1) 
			printf("recDynLinkClear: waiting for queued action on %s\n", pdynLinkPvt->pvname);
		/* try to wait until queued action is dispatched */
		for (i=0; i<10 && precDynLink->onQueue; i++) {
			epicsThreadSleep(epicsThreadSleepQuantum());
			epicsEventSignal(wakeUpEvt);
		}
		if (precDynLink->onQueue && pdynLinkPvt) {
			printf("recDynLinkClear: abandoning queued action on '%s'\n", pdynLinkPvt->pvname);
			if (pdynLinkPvt->io==ioInput) {
				if (epicsMessageQueuePending(recDynLinkInpMsgQ) == 0) precDynLink->onQueue = 0;
			} else {
				if (epicsMessageQueuePending(recDynLinkOutMsgQ) == 0) precDynLink->onQueue = 0;
			}
		}
	}
	if (pdynLinkPvt->io==ioInput) {
		if (epicsMessageQueueTrySend(recDynLinkInpMsgQ, (void *)&cmd, sizeof(cmd))) {
			errMessage(0,"recDynLinkClear: epicsMessageQueueTrySend error");
		}
	} else {
		if (epicsMessageQueueTrySend(recDynLinkOutMsgQ, (void *)&cmd, sizeof(cmd))) {
			errMessage(0,"recDynLinkClear: epicsMessageQueueTrySend error");
		}
	}
	precDynLink->pdynLinkPvt = NULL;
	precDynLink->status = 0;
	return(0);
}

long epicsShareAPI recDynLinkConnectionStatus(recDynLink *precDynLink)
{
	dynLinkPvt	*pdynLinkPvt;
	long		status;

	if (precDynLink == NULL) return(-1);
	pdynLinkPvt = precDynLink->pdynLinkPvt;
	if ((pdynLinkPvt == NULL) || (pdynLinkPvt->chid == NULL)) return(-1);
	status = (ca_state(pdynLinkPvt->chid)==cs_conn) ? 0 : -1;
	return(status);
}

long epicsShareAPI recDynLinkGetNelem(recDynLink *precDynLink,size_t *nelem)
{
	dynLinkPvt  *pdynLinkPvt;

	if (precDynLink == NULL) return(-1);
	pdynLinkPvt = precDynLink->pdynLinkPvt;
	if ((pdynLinkPvt == NULL) || (pdynLinkPvt->chid == NULL)) return(-1);
	if (ca_state(pdynLinkPvt->chid)!=cs_conn) return(-1);
	*nelem = ca_element_count(pdynLinkPvt->chid);
	return(0);
}

long epicsShareAPI recDynLinkGetControlLimits(recDynLink *precDynLink,
	double *low,double *high)
{
	dynLinkPvt	*pdynLinkPvt;

	pdynLinkPvt = precDynLink->pdynLinkPvt;
	if (pdynLinkPvt->state!=stateConnected) return(-1);
	if (low) *low = pdynLinkPvt->controlLow;
	if (high) *high = pdynLinkPvt->controlHigh;
	return(0);
}

long epicsShareAPI recDynLinkGetGraphicLimits(recDynLink *precDynLink,
	double *low,double *high)
{
	dynLinkPvt	*pdynLinkPvt;

	pdynLinkPvt = precDynLink->pdynLinkPvt;
	if (pdynLinkPvt->state!=stateConnected) return(-1);
	if (low) *low = pdynLinkPvt->graphicLow;
	if (high) *high = pdynLinkPvt->graphHigh;
	return(0);
}

long epicsShareAPI recDynLinkGetPrecision(recDynLink *precDynLink,int *prec)
{
	dynLinkPvt	*pdynLinkPvt;

	pdynLinkPvt = precDynLink->pdynLinkPvt;
	if (pdynLinkPvt->state!=stateConnected) return(-1);
	if (prec) *prec = pdynLinkPvt->precision;
	return(0);
}

long epicsShareAPI recDynLinkGetUnits(recDynLink *precDynLink,char *units,int maxlen)
{
	dynLinkPvt	*pdynLinkPvt;
    int			maxToCopy;

	pdynLinkPvt = precDynLink->pdynLinkPvt;
	if (pdynLinkPvt->state!=stateConnected) return(-1);
	maxToCopy = MAX_UNITS_SIZE;
	if (maxlen<maxToCopy) maxToCopy = maxlen;
	strncpy(units,pdynLinkPvt->units,maxToCopy);
	if (maxToCopy<maxlen) units[maxToCopy] = '\0';
	return(0);
}

long epicsShareAPI recDynLinkGet(recDynLink *precDynLink,void *pbuffer,size_t *nRequest,
	TS_STAMP *timestamp,short *status,short *severity)
{
	dynLinkPvt	*pdynLinkPvt;
	long		caStatus, save_nRequest = *nRequest;

	if (precDynLink == NULL) return(-1);
	precDynLink->status = 0;
	pdynLinkPvt = precDynLink->pdynLinkPvt;
	if ((pdynLinkPvt == NULL) || (pdynLinkPvt->chid == NULL)) return(-1);
	caStatus = (ca_state(pdynLinkPvt->chid)==cs_conn) ? 0 : -1;
	if (caStatus) goto all_done;
	if (*nRequest > pdynLinkPvt->nRequest) {
		*nRequest = pdynLinkPvt->nRequest;
	}
	epicsMutexMustLock(pdynLinkPvt->lock);
	memcpy(pbuffer,pdynLinkPvt->pbuffer,
		(*nRequest * dbr_size[mapNewToOld[pdynLinkPvt->dbrType]]));
	if (recDynLinkDebug > 5) 
            printf("recDynLinkGet: PV=%s, user asked for=%ld, got %ld\n", pdynLinkPvt->pvname,
		save_nRequest, (long)*nRequest);
 	if (timestamp) *timestamp = pdynLinkPvt->timestamp; /*array copy*/
	if (status) *status = pdynLinkPvt->status;
	if (severity) *severity = pdynLinkPvt->severity;
	epicsMutexUnlock(pdynLinkPvt->lock);

all_done:
	return(caStatus);
}

long epicsShareAPI recDynLinkGetCallback(recDynLink *precDynLink, size_t *nRequest,
	recDynCallback userGetCallback)
{
	dynLinkPvt	*pdynLinkPvt;
	long		status;
	msgQCmd		cmd;

	if (precDynLink == NULL) return(-1);
	precDynLink->status = 0;
	precDynLink->getCallbackInProgress = 1;
	pdynLinkPvt = precDynLink->pdynLinkPvt;
	if ((pdynLinkPvt == NULL) || (pdynLinkPvt->chid == NULL)) return(-1);
	if (pdynLinkPvt->io!=ioInput || pdynLinkPvt->state!=stateConnected) {
		status = -1;
	} else {
		status = (ca_state(pdynLinkPvt->chid)==cs_conn) ? 0 : -1;
	}
	if (status) goto all_done;
	if (userGetCallback) pdynLinkPvt->userGetCallback = userGetCallback;
	if (*nRequest>ca_element_count(pdynLinkPvt->chid))
		*nRequest = ca_element_count(pdynLinkPvt->chid);
	pdynLinkPvt->nRequest = *nRequest;
	cmd.data.precDynLink = precDynLink;
	cmd.cmd = cmdGetCallback;
	precDynLink->onQueue++;
	if (recDynLinkDebug > 5) 
            printf("recDynLinkGetCallback: PV=%s, nRequest=%ld\n", pdynLinkPvt->pvname,
		(long)pdynLinkPvt->nRequest); 
	if (epicsMessageQueueTrySend(recDynLinkOutMsgQ, (void *)&cmd, sizeof(cmd))) {
		errMessage(0,"recDynLinkGetCallback: epicsMessageQueueTrySend error");
		status = RINGBUFF_PUT_ERROR;
		precDynLink->onQueue--;
	}
	epicsEventSignal(wakeUpEvt);

all_done:
	return(status);
}

/* for backward compatibility with recDynLink in base */
long epicsShareAPI recDynLinkPut(recDynLink *precDynLink,void *pbuffer,size_t nRequest)
{
	return(recDynLinkPutCallback(precDynLink, pbuffer, nRequest, NULL));
}

/*
 * Note caller should interpret any non-zero return code as failure to initiate
 * an action, and caller should not wait for that action to complete.
 */
long epicsShareAPI recDynLinkPutCallback(recDynLink *precDynLink,void *pbuffer,size_t nRequest,
	recDynCallback notifyCallback)
{
	dynLinkPvt	*pdynLinkPvt;
	long		status;
	msgQCmd	cmd;

	if (precDynLink == NULL) return(-1);
	precDynLink->status = 0;
	pdynLinkPvt = precDynLink->pdynLinkPvt;
	if (pdynLinkPvt == NULL) return(-1);
	if (pdynLinkPvt->io!=ioOutput || pdynLinkPvt->state!=stateConnected) {
		status = -1;
	} else {
		if (pdynLinkPvt->chid == NULL) return(-1);
		status = (ca_state(pdynLinkPvt->chid)==cs_conn) ? 0 : -1;
	}
	if (status) goto all_done;
	if (notifyCallback) {
		if (pdynLinkPvt->notifyInProgress) return(NOTIFY_IN_PROGRESS);
		pdynLinkPvt->notifyCallback = notifyCallback;
	}
	if (pdynLinkPvt->scalar) nRequest = 1;
	if (nRequest>ca_element_count(pdynLinkPvt->chid))
	nRequest = ca_element_count(pdynLinkPvt->chid);
	pdynLinkPvt->nRequest = nRequest;
	memcpy(pdynLinkPvt->pbuffer,pbuffer,
		(nRequest * dbr_size[mapNewToOld[pdynLinkPvt->dbrType]]));
	cmd.data.precDynLink = precDynLink;
	cmd.cmd = notifyCallback ? cmdPutCallback : cmdPut;
	precDynLink->onQueue++;
	if (epicsMessageQueueTrySend(recDynLinkOutMsgQ, (void *)&cmd, sizeof(cmd))) {
		errMessage(0,"recDynLinkPut: epicsMessageQueueTrySend error");
		status = RINGBUFF_PUT_ERROR;
		precDynLink->onQueue--;
	}
	epicsEventSignal(wakeUpEvt);

all_done:
	return(status);
}

LOCAL void recDynLinkStartTasks(void)
{
	recDynLinkInpMsgQ = epicsMessageQueueCreate(recDynLinkQsize, sizeof(msgQCmd));
	if (recDynLinkInpMsgQ == NULL) {
		errMessage(0,"recDynLinkStartTasks failed");
		exit(1);
	}
	inpTaskId = epicsThreadCreate("recDynInp",epicsThreadPriorityCAServerHigh+3,
		epicsThreadGetStackSize(epicsThreadStackBig), (EPICSTHREADFUNC)recDynLinkInp,0);
	if (inpTaskId==NULL) {
		errMessage(0,"recDynLinkStartTasks: taskSpawn Failure\n");
	}
	wakeUpEvt = epicsEventCreate(epicsEventEmpty);
	if (wakeUpEvt == 0)
		errMessage(0, "epicsEventCreate failed in recDynLinkStartOutput");
	recDynLinkOutMsgQ = epicsMessageQueueCreate(recDynLinkQsize, sizeof(msgQCmd));
	if (recDynLinkOutMsgQ == NULL) {
		errMessage(0,"recDynLinkStartTasks failed");
		exit(1);
	}
	outTaskId = epicsThreadCreate("recDynOut",epicsThreadPriorityCAServerHigh+3,
		epicsThreadGetStackSize(epicsThreadStackBig), (EPICSTHREADFUNC)recDynLinkOut,0);
	if (outTaskId == NULL) {
		errMessage(0,"recDynLinkStartTasks: taskSpawn Failure\n");
	}
}

LOCAL void connectCallback(struct connection_handler_args cha)
{
	chid		chid = cha.chid;
	recDynLink	*precDynLink;
	dynLinkPvt	*pdynLinkPvt;
    
	precDynLink = (recDynLink *)ca_puser(cha.chid);
	if (!precDynLink) return;
	pdynLinkPvt = precDynLink->pdynLinkPvt;
	if (pdynLinkPvt == NULL) return;
	if (chid && (ca_state(chid) == cs_conn)) {
		pdynLinkPvt->state = stateGetting;
		SEVCHK(ca_get_callback(DBR_CTRL_DOUBLE,chid,getCallback,precDynLink),
			"ca_get_callback");
    } else {
		if (pdynLinkPvt->searchCallback)
			(pdynLinkPvt->searchCallback)(precDynLink);
	}
}

LOCAL void getCallback(struct event_handler_args eha)
{
	struct dbr_ctrl_double	*pdata = (struct dbr_ctrl_double *)eha.dbr;
	recDynLink				*precDynLink;
	dynLinkPvt				*pdynLinkPvt;
	size_t					nRequest;
   
    if (eha.status != ECA_NORMAL) {
		printf("recDynLink:getCallback: CA returns eha.status=%d\n", eha.status);
		return;
	}
	precDynLink = (recDynLink *)ca_puser(eha.chid);
	if (!precDynLink) return;
	pdynLinkPvt = precDynLink->pdynLinkPvt;
	pdynLinkPvt -> graphicLow = pdata->lower_disp_limit;
	pdynLinkPvt -> graphHigh = pdata->upper_disp_limit;
	pdynLinkPvt -> controlLow = pdata->lower_ctrl_limit;
	pdynLinkPvt -> controlHigh = pdata->upper_ctrl_limit;
	pdynLinkPvt -> precision = pdata->precision;
	strncpy(pdynLinkPvt->units,pdata->units,MAX_UNITS_SIZE);
	if (pdynLinkPvt->scalar) {
		pdynLinkPvt->nRequest = 1;
	} else {
		pdynLinkPvt->nRequest = ca_element_count(pdynLinkPvt->chid);
		if (recDynLinkDebug >= 5)
			printf("recDynLink:getCallback: array of %ld elements\n", (long)pdynLinkPvt->nRequest);
	}
	nRequest = pdynLinkPvt->nRequest;
	pdynLinkPvt->pbuffer = calloc(nRequest,
		dbr_size[mapNewToOld[pdynLinkPvt->dbrType]]);
	pdynLinkPvt->state = stateConnected;
	if (pdynLinkPvt->searchCallback) (pdynLinkPvt->searchCallback)(precDynLink);
	if (pdynLinkPvt->io==ioInput) {
		SEVCHK(ca_add_array_event(
			dbf_type_to_DBR_TIME(mapNewToOld[pdynLinkPvt->dbrType]),
			pdynLinkPvt->nRequest,
			pdynLinkPvt->chid,monitorCallback,precDynLink,
			0.0,0.0,0.0,
			&pdynLinkPvt->evid),"ca_add_array_event");
	}
}

LOCAL void monitorCallback(struct event_handler_args eha)
{
	recDynLink	*precDynLink;
	dynLinkPvt	*pdynLinkPvt;
	long		count = eha.count;
	const void	*pbuffer = eha.dbr;
	struct dbr_time_string	*pdbr_time_string;
	void		*pdata;
	short		timeType;
	char		*pchar;
	short		*pshort;
	long		*plong;
	float		*pfloat;
	double		*pdouble;
   
    if (eha.status != ECA_NORMAL) {
		printf("recDynLink:monitorCallback: CA returns eha.status=%d\n", eha.status);
		return;
	}
	precDynLink = (recDynLink *)ca_puser(eha.chid);
	if (!precDynLink) return;
	pdynLinkPvt = precDynLink->pdynLinkPvt;
	if (recDynLinkDebug >= 5) {
		printf("recDynLink:monitorCallback:  PV=%s, nRequest=%ld, status=%d\n",
			pdynLinkPvt->pvname, (long)pdynLinkPvt->nRequest, eha.status);
		if (recDynLinkDebug >= 15) {
			printf("recDynLink:monitorCallback:  eha.usr=%p, .chid=%p, .type=%ld, .count=%ld, .dbr=%p, .status=%d\n",
				(void *)eha.usr, (void *)eha.chid, eha.type, eha.count, (void *)eha.dbr, eha.status);
		}
	}
	if (pdynLinkPvt->pbuffer) {
		epicsMutexMustLock(pdynLinkPvt->lock);
		if (count>=pdynLinkPvt->nRequest) count = pdynLinkPvt->nRequest;
		pdbr_time_string = (struct dbr_time_string *)pbuffer;
		if (recDynLinkDebug >= 15) {printf("recDynLink:monitorCallback: pdbr_time_string=%p\n", (void *)pdbr_time_string); epicsThreadSleep(.1);}
		timeType = dbf_type_to_DBR_TIME(mapNewToOld[pdynLinkPvt->dbrType]);
		pdata = (void *)((char *)pbuffer + dbr_value_offset[timeType]);
		if (recDynLinkDebug >= 15) {printf("recDynLink:monitorCallback: copying time stamp\n"); epicsThreadSleep(.1);}
		pdynLinkPvt->timestamp = pdbr_time_string->stamp; /*array copy*/
		if (recDynLinkDebug >= 15) {printf("recDynLink:monitorCallback: copying status\n"); epicsThreadSleep(.1);}
		pdynLinkPvt->status = pdbr_time_string->status;
		pdynLinkPvt->severity = pdbr_time_string->severity;
		if (recDynLinkDebug >= 15) printf("recDynLink:monitorCallback: calling memcpy\n");
		memcpy(pdynLinkPvt->pbuffer,pdata,
			(count * dbr_size[mapNewToOld[pdynLinkPvt->dbrType]]));
		epicsMutexUnlock(pdynLinkPvt->lock);
		if ((count > 1) && (recDynLinkDebug >= 5)) {
			printf("recDynLink:monitorCallback: array of %ld elements\n", (long)pdynLinkPvt->nRequest);
			switch (mapNewToOld[pdynLinkPvt->dbrType]) {
			case DBF_STRING: case DBF_CHAR:
				if (recDynLinkDebug >= 15) printf("recDynLink:monitorCallback: case DBF_STRING\n");
				pchar = (char *)pdata;
				printf("...char/string: %c, %c, %c...\n", pchar[0], pchar[1], pchar[2]);
				break;
			case DBF_SHORT: case DBF_ENUM:
				pshort = (short *)pdata;
				printf("...short: %d, %d, %d...\n", pshort[0], pshort[1], pshort[2]);
				break;
			case DBF_LONG:
				plong = (long *)pdata;
				printf("...long: %ld, %ld, %ld...\n", plong[0], plong[1], plong[2]);
				break;
			case DBF_FLOAT:
				pfloat = (float *)pdata;
				printf("...float: %f, %f, %f...\n", pfloat[0], pfloat[1], pfloat[2]);
				break;
			case DBF_DOUBLE:
				pdouble = (double *)pdata;
				printf("...double: %f, %f, %f...\n", pdouble[0], pdouble[1], pdouble[2]);
				break;
			default:
				pchar = (char *)pdata;
				printf("...unknown type: %x, %x, %x...\n", pchar[0], pchar[1], pchar[2]);
				break;
			}
		}
	}
	if (recDynLinkDebug >= 15) printf("recDynLink:monitorCallback: executing client callback\n");

	if (pdynLinkPvt->monitorCallback)
		(*pdynLinkPvt->monitorCallback)(precDynLink);
	if (recDynLinkDebug >= 15) printf("recDynLink:monitorCallback: exit\n");

}

LOCAL void userGetCallback(struct event_handler_args eha)
{
	recDynLink	*precDynLink;
	dynLinkPvt	*pdynLinkPvt;
	long		count = eha.count;
	const void	*pbuffer = eha.dbr;
	struct dbr_time_string	*pdbr_time_string;
	void		*pdata;
	short		timeType;

    if (eha.status != ECA_NORMAL) {
		printf("recDynLink:userGetCallback: CA returns eha.status=%d\n", eha.status);
		return;
	}
	precDynLink = (recDynLink *)ca_puser(eha.chid);
	if (!precDynLink) return;
	pdynLinkPvt = precDynLink->pdynLinkPvt;
	if (recDynLinkDebug >= 5) {
		printf("recDynLink:userGetCallback:  PV=%s, nRequest=%ld\n",
			pdynLinkPvt->pvname, (long)pdynLinkPvt->nRequest);
	}
	if (pdynLinkPvt->pbuffer) {
		epicsMutexMustLock(pdynLinkPvt->lock);
		if (count>=pdynLinkPvt->nRequest) count = pdynLinkPvt->nRequest;
		pdbr_time_string = (struct dbr_time_string *)pbuffer;
		timeType = dbf_type_to_DBR_TIME(mapNewToOld[pdynLinkPvt->dbrType]);
		pdata = (void *)((char *)pbuffer + dbr_value_offset[timeType]);
		pdynLinkPvt->timestamp = pdbr_time_string->stamp; /*array copy*/
		pdynLinkPvt->status = pdbr_time_string->status;
		pdynLinkPvt->severity = pdbr_time_string->severity;
		memcpy(pdynLinkPvt->pbuffer,pdata,
			(count * dbr_size[mapNewToOld[pdynLinkPvt->dbrType]]));
		epicsMutexUnlock(pdynLinkPvt->lock);
	}
	if (pdynLinkPvt->userGetCallback)
		(*pdynLinkPvt->userGetCallback)(precDynLink);
}

LOCAL void notifyCallback(struct event_handler_args eha)
{
	recDynLink	*precDynLink;
	dynLinkPvt	*pdynLinkPvt;

    if (eha.status != ECA_NORMAL) {
		printf("recDynLink:notifyCallback: CA returns eha.status=%d (%s)\n",
			eha.status, ca_message(eha.status));
/* try to find out who sent the command that produced this result */
#if 1
		precDynLink = (recDynLink *)ca_puser(eha.chid);
		if (!precDynLink) {
			printf("recDynLink:notifyCallback: ...Can't examine recDynLink\n");
			return;
		}
		pdynLinkPvt = precDynLink->pdynLinkPvt;
		if (!pdynLinkPvt) {
			printf("recDynLink:notifyCallback: ...Can't examine dynLinkPvt\n");
			return;
		}
		printf("recDynLink:notifyCallback: ...pvname='%s'\n", pdynLinkPvt->pvname);
#endif
		return;
	}
	precDynLink = (recDynLink *)ca_puser(eha.chid);
	if (!precDynLink) return;
	pdynLinkPvt = precDynLink->pdynLinkPvt;
	if (pdynLinkPvt->notifyCallback) {
		pdynLinkPvt->notifyInProgress = 0;
		(pdynLinkPvt->notifyCallback)(precDynLink);
	}
}


static struct ca_client_context *pCaInputContext = NULL;
LOCAL void recDynLinkInp(void)
{
	int			status, n, s = sizeof(msgQCmd), retried = 0;
	recDynLink	*precDynLink;
	dynLinkPvt	*pdynLinkPvt;
	msgQCmd		cmd;
	int			didGetCallback=0;

	epicsAtExit(exit_handler, 0);
	taskwdInsert(epicsThreadGetIdSelf(),NULL,NULL);
	SEVCHK(ca_context_create(ca_enable_preemptive_callback),"ca_context_create");
	pCaInputContext = ca_current_context();
	while (pCaInputContext == NULL) {
		if (!retried) {
			printf("recDynLinkInp: ca_current_context() returned NULL\n");
			retried = 1;
		}
		epicsThreadSleep(epicsThreadSleepQuantum());
		pCaInputContext = ca_current_context();
	}
	if (retried) printf("recDynLinkInp: ca_current_context() returned non-NULL\n");
	while (!shutting_down) {
		didGetCallback = 0;
		while (epicsMessageQueuePending(recDynLinkInpMsgQ) && interruptAccept) {
			if (recDynLinkDebug > 5) 
                            printf("epicsMessageQueuePending(recDynLinkInpMsgQ)=%d\n", 
				epicsMessageQueuePending(recDynLinkInpMsgQ));
			n = epicsMessageQueueReceive(recDynLinkInpMsgQ, (void *)&cmd,sizeof(cmd));
			if (n != s) {
				printf("recDynLinkInpTask: got %d bytes, expected %d\n", n, s);
				continue;
			}
			if (cmd.cmd==cmdClear) {
				pdynLinkPvt = cmd.data.pdynLinkPvt;
				if (pdynLinkPvt->chid)
					SEVCHK(ca_clear_channel(pdynLinkPvt->chid),"ca_clear_channel");
				free(pdynLinkPvt->pbuffer);
				epicsMutexDestroy(pdynLinkPvt->lock);
				free((void *)pdynLinkPvt);
				continue;
			}
			precDynLink = cmd.data.precDynLink;
			pdynLinkPvt = precDynLink->pdynLinkPvt;
			if (recDynLinkDebug > 5) 
                            printf("recDynLinkInp: precDynLink=%p", (void *)precDynLink); 
			if (pdynLinkPvt==NULL) {
				printf("\nrecDynLinkInp: ***ERROR***: pdynLinkPvt==%p (precDynLink==%p)\n",
					(void *)pdynLinkPvt, (void *)precDynLink);
				precDynLink->onQueue--;
				continue;
			} else {
				if (recDynLinkDebug > 5) 
					printf(", pvname='%s'\n", pdynLinkPvt->pvname);
			}
			switch (cmd.cmd) {
			case (cmdSearch) :
				SEVCHK(ca_create_channel(pdynLinkPvt->pvname,
					connectCallback,precDynLink, 10 ,&pdynLinkPvt->chid),
				"ca_create_channel");
				precDynLink->onQueue--;
				break;
			case (cmdGetCallback):
				didGetCallback = 1;
				status = ca_array_get_callback(
					dbf_type_to_DBR_TIME(mapNewToOld[pdynLinkPvt->dbrType]),
					pdynLinkPvt->nRequest,pdynLinkPvt->chid, userGetCallback, precDynLink);
				if (status!=ECA_NORMAL) {
					epicsPrintf("recDynLinkTask pv=%s CA Error %s\n",
						pdynLinkPvt->pvname,ca_message(status));
					/* error indicates user won't get a callback, so we do it */
					precDynLink->status = FATAL_ERROR;
					(pdynLinkPvt->userGetCallback)(precDynLink);
				}
				precDynLink->onQueue--;
				break;
			default:
				epicsPrintf("Logic error statement in recDynLinkTask\n");
				precDynLink->onQueue--;
			}
		}
		if (didGetCallback) {
			status = ca_pend_io(recDynINPCallPendIoTime_ms/1000.);
			if (status!=ECA_NORMAL && status!=ECA_TIMEOUT)
			SEVCHK(status,"ca_pend_io");
		} else if (recDynINPCallPendEvent) {
			status = ca_pend_event(recDynINPCallPendEventTime_ms/1000. + 1.e-5);
			if (status!=ECA_NORMAL && status!=ECA_TIMEOUT)
			SEVCHK(status,"ca_pend_event");
		}
	}
}

/*
 * Note that we're a lower priority process than any expected caller,
 * so if caller should rapidly queue a bunch of cmdGetCallback's, then we
 * normally will not start processing them until the whole bunch has been
 * queued, and we'll stay in the "while (epicsMessageQueuePending)" loop
 * until the whole bunch has been dispatched, so caller needn't worry
 * about packaging messages into a group.
 */
LOCAL void recDynLinkOut(void)
{
	int			status, n, s = sizeof(msgQCmd), retried = 0;
	recDynLink	*precDynLink;
	dynLinkPvt	*pdynLinkPvt;
	msgQCmd		cmd;
	int			caStatus;
	
	epicsAtExit(exit_handler, 0);
	taskwdInsert(epicsThreadGetIdSelf(),NULL,NULL);
	/* SEVCHK(ca_context_create(ca_enable_preemptive_callback),"ca_context_create"); */
	while (pCaInputContext == NULL) {
		if (!retried) {
			printf("recDynLinkOut: waiting for CA context\n");
			retried = 1;
		}
		epicsThreadSleep(epicsThreadSleepQuantum());
	}
	if (retried) printf("recDynLinkOut: got CA context\n");
	SEVCHK(ca_attach_context(pCaInputContext), "ca_attach_context");
	while (!shutting_down) {
		epicsEventWaitWithTimeout(wakeUpEvt,1.0);
		while (epicsMessageQueuePending(recDynLinkOutMsgQ) && interruptAccept) {
			if (recDynLinkDebug > 10) 
				printf("epicsMessageQueuePending(recDynLinkOutMsgQ)=%d\n", 
				epicsMessageQueuePending(recDynLinkOutMsgQ));
			n = epicsMessageQueueReceive(recDynLinkOutMsgQ, (void *)&cmd,
				sizeof(msgQCmd));
			if (recDynLinkDebug > 10) 
				printf("recDynLinkOut: got message of size %d, cmd=%s\n", n, commands[cmd.cmd]); 
			if (n != s) {
				printf("recDynLinkOutTask: got %d bytes, expected %d\n", n, s);
				continue;
			}
			if (cmd.cmd==cmdClear) {
				pdynLinkPvt = cmd.data.pdynLinkPvt;
				if (pdynLinkPvt->chid)
					SEVCHK(ca_clear_channel(pdynLinkPvt->chid),
						"ca_clear_channel");
				free(pdynLinkPvt->pbuffer);
				epicsMutexDestroy(pdynLinkPvt->lock);
				free((void *)pdynLinkPvt);
				continue;
			}
			precDynLink = cmd.data.precDynLink;
			pdynLinkPvt = precDynLink->pdynLinkPvt;
			if (recDynLinkDebug > 10) 
				printf("recDynLinkOut: precDynLink=%p", (void *)precDynLink); 
			if (pdynLinkPvt==NULL) {
				printf("\nrecDynLinkOut: ***ERROR***: pdynLinkPvt==%p (precDynLink==%p)\n",
					(void *)pdynLinkPvt, (void *)precDynLink);
				precDynLink->onQueue--;
				continue;
			} else if (pdynLinkPvt->pvname[0] == '\0') {
				printf("\nrecDynLinkOut: ***ERROR***: pvname=='' (precDynLink==%p)\n",
					(void *)precDynLink);
				continue;
			} else {
				if (recDynLinkDebug > 10) 
					printf(", pvname='%s'\n", pdynLinkPvt->pvname);
			}
			switch (cmd.cmd) {
			case (cmdSearch):
				SEVCHK(ca_create_channel(pdynLinkPvt->pvname,
					connectCallback,precDynLink, 10 ,&pdynLinkPvt->chid),
					"ca_create_channel");
				precDynLink->onQueue--;
				break;
			case (cmdPut):
				caStatus = ca_array_put(
					mapNewToOld[pdynLinkPvt->dbrType],
					pdynLinkPvt->nRequest,pdynLinkPvt->chid,
					pdynLinkPvt->pbuffer);
				if (caStatus!=ECA_NORMAL) {
					epicsPrintf("recDynLinkTask pv=%s CA Error %s\n",
						pdynLinkPvt->pvname,ca_message(caStatus));
				}
				precDynLink->onQueue--;
				break;
			case (cmdPutCallback):
				pdynLinkPvt->notifyInProgress = 1;
				caStatus = ca_array_put_callback(
					mapNewToOld[pdynLinkPvt->dbrType],
					pdynLinkPvt->nRequest,pdynLinkPvt->chid,
					pdynLinkPvt->pbuffer, notifyCallback, precDynLink);
				if (caStatus!=ECA_NORMAL) {
					epicsPrintf("recDynLinkTask pv=%s CA Error %s\n",
						pdynLinkPvt->pvname,ca_message(caStatus));
					/* error indicates user won't get a callback, so we do it */
					pdynLinkPvt->notifyInProgress = 0;
					precDynLink->status = FATAL_ERROR;
					(pdynLinkPvt->notifyCallback)(precDynLink);
				}
				precDynLink->onQueue--;
				break;
			case (cmdGetCallback):
				if (recDynLinkDebug > 5) 
                                    printf("recDynLinkOut: GetCallback PV=%s, nRequest=%ld\n",
					pdynLinkPvt->pvname, (long)pdynLinkPvt->nRequest); 

				status = ca_array_get_callback(
					dbf_type_to_DBR_TIME(mapNewToOld[pdynLinkPvt->dbrType]),
					pdynLinkPvt->nRequest,pdynLinkPvt->chid, userGetCallback, precDynLink);
				if (status!=ECA_NORMAL) {
					epicsPrintf("recDynLinkTask pv=%s CA Error %s\n",
						pdynLinkPvt->pvname,ca_message(status));
					/* error indicates user won't get a callback, so we do it */
					precDynLink->status = FATAL_ERROR;
					(pdynLinkPvt->userGetCallback)(precDynLink);
				}
				precDynLink->onQueue--;
				break;
			default:
				epicsPrintf("Logic error statement in recDynLinkTask\n");
				precDynLink->onQueue--;
			}
		}
		if (recDynOUTCallFlush) ca_flush_io();
		if (recDynOUTCallPend) {
			status = ca_pend_event(.00001);
			if (status!=ECA_NORMAL && status!=ECA_TIMEOUT)
			SEVCHK(status,"ca_pend_event");
		}
	}
}
