/* ab1771NRecord.c*/
/*************************************************************************
* Copyright (c) 2003 The University of Chicago, as Operator of Argonne
* National Laboratory, the Regents of the University of California, as
* Operator of Los Alamos National Laboratory, and Leland Stanford Junior
* University, as the Operator of Stanford Linear Accelerator.
* This code is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
*************************************************************************/

/*
 *      Author:  Marty Kraimer
 *      Date:   June 1, 1997
*/

#include <vxWorks.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "alarm.h"
#include "caeventmask.h"
#include "dbStaticLib.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "errlog.h"
#include "callback.h"
#include "dbScan.h"
#include "dbEvent.h"
#include "recSup.h"
#include "task_params.h"
#include "alarm.h"
#include "link.h"
#include "drvAb.h"
#include "devIai1.h"
#include "devIao1.h"
#include "dbCommon.h"
#define GEN_SIZE_OFFSET
#include "ab1771NRecord.h"
#undef GEN_SIZE_OFFSET
#include <epicsExport.h>

/* IN THIS SOURCE MODULE
 * channel and signal both refer to one adc channel.
 * channel is indexed from 1 to 8
 * signal is indexed from 0 to 7
*/

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
LOCAL long init_record();
LOCAL long process();
#define special NULL
#define get_value NULL
LOCAL long cvt_dbaddr();
LOCAL long get_array_info();
#define put_array_info NULL
#define get_units NULL
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL
 
struct rset ab1771NRSET={
    RSETNUMBER,
    report,
    initialize,
    init_record,
    process,
    special,
    get_value,
    cvt_dbaddr,
    get_array_info,
    put_array_info,
    get_units,
    get_precision,
    get_enum_str,
    get_enum_strs,
    put_enum_str,
    get_graphic_double,
    get_control_double,
    get_alarm_double
};
epicsExportAddress(rset,ab1771NRSET);

LOCAL void monitor(ab1771NRecord *precord);

int ab1771NDebug=0;

/*structure for set of fields in ab1771NRecord.h describing one channel*/
typedef struct channelFields {
    unsigned short typ;
    float          fil;
    float          ral;
    float          ohm;
    short          sta;
    short          raw;
} channelFields;

#define MAX_CHANS     8
#define MAX_WORDS_IN  20
#define MAX_WORDS_OUT 60

/*definitions for messages to allen bradley driver*/
typedef enum {
    msgStateGetInitialOutputs,
    msgStateWaitInitialOutputs,
    msgStateInit,
    msgStateWaitInit,
    msgStatePut,
    msgStateWaitPut,
    msgStateGet,
    msgStateWaitGet,
    msgStateDone
}msgStates;

/*definitions for error status values*/
typedef enum {
    errOK,
    errAb,
    errFatal
} errStatus;

/*structure attached to dpvt */
typedef struct recordPvt{
    CALLBACK    callback;
    errStatus   err;
    abStatus    status;
    abStatus    prevStatus;
    IOSCANPVT   ioscanpvt[MAX_CHANS];
    char	ioscanpvtInited[MAX_CHANS];
    void        *drvPvt;
    int         nChan;
    int         nOutChan;
    msgStates	msgState;
    boolean     waitAsynComplete;
    boolean     newValMsg;
    boolean     newInMsg;
    boolean     newOutMsg;
    boolean     needsPut;
    boolean     processCalled;
}recordPvt;

typedef enum {
    ab1771NOK=devIai1OK,
    ab1771NAbError=devIai1HardwareFault,
    ab1771NUnderflow=devIai1Underflow,
    ab1771NOverflow=devIai1Overflow,
    ab1771NNoRecord,
    ab1771NIllegalRecordType,
    ab1771NIllegalType,
    ab1771NFatalError
} ab1771NStatus;

LOCAL char *ab1771NStatusMessage[] = {
	"ab1771N OK",
	"ab1771N drvAb error",
	"ab1771N Underflow",
	"ab1771N Overflow",
	"ab1771N Record not found",
	"ab1771N Not an ab1771NRecord",
	"ab1771N in/out type mismatch",
	"ab1771N fatal error"
};

/* forward references for local routines */
LOCAL void issueAsynCallback(ab1771NRecord *precord);
LOCAL void setValMsg(ab1771NRecord *precord,char *msg);
LOCAL void issueError(ab1771NRecord *precord,errStatus err,char *msg);
LOCAL void checkConfiguration(ab1771NRecord *precord);

LOCAL void drvCallback(void *drvPvt);
LOCAL void myCallback(CALLBACK *pCallback);

LOCAL void msgInitOutMsg(ab1771NRecord *precord);
LOCAL void msgCheckInpHeader(ab1771NRecord *precord);
LOCAL void msgGetInitialOutputs(ab1771NRecord *precord);
LOCAL void msgCompleteInitialOutputs(ab1771NRecord *precord);
LOCAL void msgInit(ab1771NRecord *precord);
LOCAL void msgPut(ab1771NRecord *precord);
LOCAL void msgGet(ab1771NRecord *precord);
LOCAL void msgCompleteGet(ab1771NRecord *precord);

/*Interface routines for devIai1 and devIao1*/
LOCAL int ai_connect(void **pdevIai1Pvt,DBLINK *plink, boolean *isEng);
LOCAL char *ai_get_status_message(void *pdevIai1Pvt,int status);
LOCAL int ai_get_ioint_info(void *devIai1Pvt,int cmd, IOSCANPVT *ppvt);
LOCAL int ai_get_linconv(void *devIai1Pvt,long *range,long *roff);
LOCAL int ai_get_raw(void *devIai1Pvt,long *praw);
LOCAL int ai_get_eng(void *devIai1Pvt,double *peng);

LOCAL int ao_connect(void **pdevIao1Pvt,DBLINK *plink, boolean *isEng);
LOCAL char *ao_get_status_message(void *pdevIao1Pvt,int status);
LOCAL int ao_get_linconv(void *devIao1Pvt,long *range,long *roff);
LOCAL int ao_get_raw(void *devIao1Pvt,long *praw);
LOCAL int ao_get_eng(void *devIao1Pvt,double *peng);
LOCAL int ao_put_raw(void *devIao1Pvt,long raw);
LOCAL int ao_put_eng(void *devIao1Pvt,double eng);

LOCAL devIai1 mydevIai1= {
    ai_connect,ai_get_status_message,ai_get_ioint_info,
    ai_get_linconv,ai_get_raw,ai_get_eng
};

LOCAL devIao1 mydevIao1= {
    ao_connect,ao_get_status_message,ao_get_linconv,
    ao_get_raw,ao_get_eng,ao_put_raw,ao_put_eng
};

extern DBBASE *pdbbase;

/*BEGIN STANDARD RECORD SUPPORT ROUTINES*/
LOCAL long init_record(ab1771NRecord *precord,int pass)
{
    abStatus		status;
    recordPvt		*precordPvt;

    if(pass!=0) return(0);
    precord->iai1 = &mydevIai1;
    precord->iao1 = &mydevIao1;
    precord->dpvt = precordPvt = dbCalloc(1,sizeof(recordPvt));
    callbackSetCallback(myCallback,&precordPvt->callback);
    callbackSetUser(precord,&precordPvt->callback);
    precord->inpm = (short *)dbCalloc(MAX_WORDS_IN,sizeof(short));
    precord->outm = (short *)dbCalloc(MAX_WORDS_OUT,sizeof(short));
    checkConfiguration(precord);
    status = (*pabDrv->registerCard)(precord->link,precord->rack,
	precord->slot,typeBt,"ab1771N",drvCallback,&precordPvt->drvPvt);
    if(status!=abNewCard) {
	if(status==abSuccess)
	    errlogPrintf("record %s slot already used\n",precord->name);
	else
	    errlogPrintf("record %s init error %s\n",precord->name,
		abStatusMessage[status]);
	issueError(precord,errFatal," while registering card");
	precord->pact = TRUE; /*leave record active*/
	return(0);
    }
    (*pabDrv->setUserPvt)(precordPvt->drvPvt,(void *)precord);
    if(precordPvt->nOutChan > 0)
        msgGetInitialOutputs(precord);
    else
	msgInit(precord);
    precord->udf = FALSE;
    return(0);
}

LOCAL long process(ab1771NRecord *precord)
{
    recordPvt	*precordPvt = (recordPvt *)precord->dpvt;

    precordPvt->processCalled = TRUE;
    if(precordPvt->err != errFatal) {
	if(!precord->pact) {/* starting new request*/
	    if(precord->cmd==ab1771N_Init) {
		precordPvt->msgState = msgStateInit;
		precord->cmd = ab1771N_CMD_None;
	        db_post_events(precord,&precord->cmd,DBE_VALUE);
	    }
	    precordPvt->err = errOK;
  	    /* starting new request*/
	    switch(precordPvt->msgState) {
	    case msgStateGetInitialOutputs:
		msgGetInitialOutputs(precord);
		break;
	    case msgStateInit:
	        msgInit(precord);
		break;
	    case msgStatePut:
		msgPut(precord);
		break;
	    case msgStateGet:
	    case msgStateDone:
		msgGet(precord);
		break;
	    default: break;
	    }/*Note that state may switch because of msgRequest just issued*/
	}
	switch(precordPvt->msgState) {
	case msgStateWaitInitialOutputs: 
	case msgStateWaitInit: 
	case msgStateWaitPut: 
	case msgStateWaitGet: 
	    precord->pact = TRUE;
	    precordPvt->waitAsynComplete = TRUE;
	    return(0);
	default:
	    break;
	}
    }
    precord->pact = TRUE;
    monitor(precord);
    recGblFwdLink(precord);
    precordPvt->waitAsynComplete = FALSE;
    if(precordPvt->err != errFatal) precord->pact = FALSE;
    return(0);
}

LOCAL long cvt_dbaddr(struct dbAddr *paddr)
{
    int            ind;
    ab1771NRecord  *precord;
    recordPvt      *precordPvt;

    precord = (ab1771NRecord *)paddr->precord;
    precordPvt = (recordPvt *)precord->dpvt;
    ind = dbGetFieldIndex(paddr);
    switch(ind) {
	case ab1771NRecordINPM:
	    paddr->pfield = precord->inpm;
	    paddr->field_type = DBF_SHORT;
	    paddr->dbr_field_type = DBR_SHORT;
	    paddr->no_elements = MAX_WORDS_IN;
	    paddr->field_size = sizeof(short);
	    break;
	case ab1771NRecordOUTM:
	    paddr->pfield = precord->outm;
	    paddr->field_type = DBF_SHORT;
	    paddr->dbr_field_type = DBR_SHORT;
	    paddr->no_elements = MAX_WORDS_OUT;
	    paddr->field_size = sizeof(short);
	    break;
	case ab1771NRecordIAI1:
	    paddr->pfield = precord->iai1;
	    paddr->field_type = DBF_NOACCESS;
	    paddr->dbr_field_type = DBR_NOACCESS;
	    paddr->no_elements = 0;
	    paddr->field_size = sizeof(void *);
	    break;
	case ab1771NRecordIAO1:
	    paddr->pfield = precord->iao1;
	    paddr->field_type = DBF_NOACCESS;
	    paddr->dbr_field_type = DBR_NOACCESS;
	    paddr->no_elements = 0;
	    paddr->field_size = sizeof(void *);
	    break;
	default:
	    errlogPrintf("%s cvt_dbaddr for illegal field\n",precord->name);
    }
    return(0);
}

LOCAL long get_array_info(struct dbAddr *paddr,long *no_elements,long *offset)
{
    int                  ind;
    ab1771NRecord *precord;
    recordPvt           *precordPvt;

    precord = (ab1771NRecord *)paddr->precord;
    precordPvt = (recordPvt *)precord->dpvt;
    ind = dbGetFieldIndex(paddr);
    switch(ind) {
	case ab1771NRecordINPM:
	    *no_elements = MAX_WORDS_IN;
	    *offset = 0;
	    break;
	case ab1771NRecordOUTM:
	    *no_elements = MAX_WORDS_OUT;
	    *offset = 0;
	    break;
	default:
	    errlogPrintf("%s get_array_info for illegal field\n",precord->name);
    }
    return(0);
}

LOCAL void monitor(ab1771NRecord *precord)
{
    unsigned short	monitor_mask;
    unsigned short	mask;
    recordPvt		*precordPvt = (recordPvt *)precord->dpvt;
    int                 signal;
    channelFields       *pcf;

    monitor_mask = recGblResetAlarms(precord);
    recGblGetTimeStamp(precord);
    mask = monitor_mask;
    if(strlen(precord->val)==0) setValMsg(precord,0);
    if(precordPvt->newValMsg) mask |= DBE_VALUE;
    if(mask) {
	db_post_events(precord,&precord->val[0],mask);
    }
    mask = monitor_mask;
    if(precordPvt->newInMsg) mask |= DBE_VALUE;
    if(mask) {
	db_post_events(precord,precord->inpm,mask);
    }
    mask = monitor_mask;
    if(precordPvt->newOutMsg) mask |= DBE_VALUE;
    if(mask) {
	db_post_events(precord,precord->outm,mask);
    }
    pcf = (channelFields *)&precord->typ1;
    for(signal=0; signal<precordPvt->nChan; signal++) {
	mask = monitor_mask;
	if(signal>=precordPvt->nOutChan && precordPvt->newInMsg)
	    mask |= DBE_VALUE;
	db_post_events(precord,&pcf->raw,mask);
	if(precordPvt->newInMsg 
	|| (precord->loca && (precordPvt->prevStatus!=precordPvt->status))) {
	    if(precordPvt->ioscanpvtInited[signal])
	        scanIoRequest(precordPvt->ioscanpvt[signal]);
	}
	pcf++;
    }
    precordPvt->prevStatus = precordPvt->status;
    precordPvt->newValMsg = FALSE;
    precordPvt->newInMsg = FALSE;
    precordPvt->newOutMsg = FALSE;
    return;
}

/*BEGIN UTILITY ROUTINES*/
LOCAL void issueAsynCallback(ab1771NRecord *precord)
{
    recordPvt     *precordPvt = (recordPvt *)precord->dpvt;
    struct rset   *prset=(struct rset *)(precord->rset);

    if(precordPvt->waitAsynComplete) {
	(*prset->process)(precord);
    }
}

LOCAL void setValMsg(ab1771NRecord *precord,char *msg)
{
    recordPvt     *precordPvt = (recordPvt *)precord->dpvt;

    if(!msg || strlen(msg)==0) {
	if(strcmp(precord->val,"Ok") == 0) return;
	strcpy(precord->val,"Ok");
	precordPvt->newValMsg = TRUE;
    } else {
	if(strcmp(msg,precord->val)!=0) {
            strcpy(precord->val,msg);
            precordPvt->newValMsg = TRUE;
	}
    }
}

LOCAL void issueError(ab1771NRecord *precord,errStatus err,char *msg)
{
    recordPvt *precordPvt = (recordPvt *)precord->dpvt;
    char      message[80];

    precordPvt->err = err;
    if(err!=errOK) recGblSetSevr(precord,COMM_ALARM,INVALID_ALARM);
    message[0] = '\0';
    switch(err) {
    case errOK:
	if(msg) sprintf(message,"%s",msg);
	break;
    case errAb:
        if(msg) sprintf(message,"abError %s %s",
	    abStatusMessage[precordPvt->status],msg);
	else sprintf(message,"abError %s",
	    abStatusMessage[precordPvt->status]);
	break;
    case errFatal:
        if(msg) sprintf(message,"%s fatal error",msg);
	else strcpy(message,"Fatal error");
	break;
    default:
	strcpy(message,"Logic error issueError called with illegal err\n");
	break;
    }
    setValMsg(precord,message);
}

LOCAL void checkConfiguration(ab1771NRecord *precord)
{
    recordPvt		*precordPvt = (recordPvt *)precord->dpvt;
    channelFields	*pcf;
    int                 nChan=0;
    int                 nOutChan=0;
    int                 signal;

    switch(precord->mtyp) {
        case ab1771N_NIS:
        case ab1771N_NIV:
        case ab1771N_NIV1: {
	    nChan=8; nOutChan=0;
	    pcf = (channelFields *)&precord->typ1;
	    for(signal=0; signal<nChan; signal++,pcf++) {
		if(pcf->typ != ab1771N_TYPE_VA) {
		    errlogPrintf("%s TYP%1.1d type must be ab1771N_TYPE_VA\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_VA;
		}
	    }
	    break;
	}
        case ab1771N_NIVR: {
	    nChan=8; nOutChan=0;
	    pcf = (channelFields *)&precord->typ1;
	    for(signal=0; signal<4; signal++,pcf++) {
		if(pcf->typ != ab1771N_TYPE_VA) {
		    errlogPrintf("%s TYP%1.1d type must be ab1771N_TYPE_VA\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_VA;
		}
	    }
	    for(signal=4; signal<nChan; signal++,pcf++) {
		if(pcf->typ < ab1771N_TYPE_RTDPtEurope) {
		    errlogPrintf("%s TYP%1.1d type must be RTD\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_RTDCopper;
		}
	    }
	    break;
	}
        case ab1771N_NIVT: {
	    nChan=8; nOutChan=0;
	    pcf = (channelFields *)&precord->typ1;
	    for(signal=0; signal<4; signal++,pcf++) {
		if(pcf->typ != ab1771N_TYPE_VA) {
		    errlogPrintf("%s TYP%1.1d type must be ab1771N_TYPE_VA\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_VA;
		}
	    }
	    for(signal=4; signal<nChan; signal++,pcf++) {
		if(pcf->typ >= ab1771N_TYPE_RTDPtEurope) {
		    errlogPrintf("%s TYP%1.1d must NOT be RTD\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_VA;
		}
	    }
	    break;
	}
        case ab1771N_NR: {
	    nChan=8; nOutChan=0;
	    pcf = (channelFields *)&precord->typ1;
	    for(signal=0; signal<nChan; signal++,pcf++) {
		if(pcf->typ < ab1771N_TYPE_RTDPtEurope) {
		    errlogPrintf("%s TYP%1.1d type must be RTD\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_RTDCopper;
		}
	    }
	    break;
	}
        case ab1771N_NT1:
        case ab1771N_NT2: {
	    nChan=8; nOutChan=0;
	    pcf = (channelFields *)&precord->typ1;
	    for(signal=0; signal<nChan; signal++,pcf++) {
		if(pcf->typ >= ab1771N_TYPE_RTDPtEurope) {
		    errlogPrintf("%s TYP%1.1d must NOT be RTD\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_VA;
		}
	    }
	    break;
	}
        case ab1771N_NOC:
        case ab1771N_NOV: {
	    nChan=8; nOutChan=8;
	    pcf = (channelFields *)&precord->typ1;
	    for(signal=0; signal<nChan; signal++,pcf++) {
		if(pcf->typ != ab1771N_TYPE_VA) {
		    errlogPrintf("%s TYP%1.1d type must be ab1771N_TYPE_VA\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_VA;
		}
	    }
	    break;
	}
        case ab1771N_NB4T: {
	    nChan=4; nOutChan=2;
	    pcf = (channelFields *)&precord->typ1;
	    for(signal=0; signal<2; signal++,pcf++) {
		if(pcf->typ != ab1771N_TYPE_VA) {
		    errlogPrintf("%s TYP%1.1d type must be ab1771N_TYPE_VA\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_VA;
		}
	    }
	    for(signal=2; signal<nChan; signal++,pcf++) {
		if(pcf->typ >= ab1771N_TYPE_RTDPtEurope) {
		    errlogPrintf("%s TYP%1.1d must NOT be RTD\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_VA;
		}
	    }
	    break;
	}
        case ab1771N_NB4S: {
	    nChan=4; nOutChan=2;
	    pcf = (channelFields *)&precord->typ1;
	    for(signal=0; signal<nChan; signal++,pcf++) {
		if(pcf->typ != ab1771N_TYPE_VA) {
		    errlogPrintf("%s TYP%1.1d type must be ab1771N_TYPE_VA\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_VA;
		}
	    }
	    break;
	}
        case ab1771N_NBSC: {
	    nChan=8; nOutChan=2;
	    pcf = (channelFields *)&precord->typ1;
	    for(signal=0; signal<nChan; signal++,pcf++) {
		if(pcf->typ != ab1771N_TYPE_VA) {
		    errlogPrintf("%s TYP%1.1d type must be ab1771N_TYPE_VA\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_VA;
		}
	    }
	    break;
	}
        case ab1771N_NBRC: {
	    nChan=8; nOutChan=2;
	    pcf = (channelFields *)&precord->typ1;
	    for(signal=0; signal<2; signal++,pcf++) {
		if(pcf->typ != ab1771N_TYPE_VA) {
		    errlogPrintf("%s TYP%1.1d type must be ab1771N_TYPE_VA\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_VA;
		}
	    }
	    for(signal=2; signal<nChan; signal++,pcf++) {
		if(pcf->typ < ab1771N_TYPE_RTDPtEurope) {
		    errlogPrintf("%s TYP%1.1d type must be RTD\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_RTDCopper;
		}
	    }
	    break;
	}
        case ab1771N_NBTC: {
	    nChan=8; nOutChan=2;
	    pcf = (channelFields *)&precord->typ1;
	    for(signal=0; signal<2; signal++,pcf++) {
		if(pcf->typ != ab1771N_TYPE_VA) {
		    errlogPrintf("%s TYP%1.1d type must be ab1771N_TYPE_VA\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_VA;
		}
	    }
	    for(signal=2; signal<nChan; signal++,pcf++) {
		if(pcf->typ >= ab1771N_TYPE_RTDPtEurope) {
		    errlogPrintf("%s TYP%1.1d must NOT be RTD\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_VA;
		}
	    }
	    break;
	}
        case ab1771N_NBV1:
        case ab1771N_NBVC: {
	    nChan=8; nOutChan=2;
	    pcf = (channelFields *)&precord->typ1;
	    for(signal=0; signal<nChan; signal++,pcf++) {
		if(pcf->typ != ab1771N_TYPE_VA) {
		    errlogPrintf("%s TYP%1.1d type must be ab1771N_TYPE_VA\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_VA;
		}
	    }
	    break;
	}
        case ab1771N_NX1: {
	    nChan=8; nOutChan=5;
	    pcf = (channelFields *)&precord->typ1;
	    for(signal=0; signal<5; signal++,pcf++) {
		if(pcf->typ != ab1771N_TYPE_VA) {
		    errlogPrintf("%s TYP%1.1d type must be ab1771N_TYPE_VA\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_VA;
		}
	    }
	    for(signal=5; signal<7; signal++,pcf++) {
		if(pcf->typ < ab1771N_TYPE_RTDPtEurope) {
		    errlogPrintf("%s TYP%1.1d type must be RTD\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_RTDCopper;
		}
	    }
	    for(signal=7; signal<nChan; signal++,pcf++) {
		if(pcf->typ >= ab1771N_TYPE_RTDPtEurope) {
		    errlogPrintf("%s TYP%1.1d must NOT be RTD\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_VA;
		}
	    }
	    break;
	}
        case ab1771N_NX2: {
	    nChan=8; nOutChan=4;
	    pcf = (channelFields *)&precord->typ1;
	    for(signal=0; signal<4; signal++,pcf++) {
		if(pcf->typ != ab1771N_TYPE_VA) {
		    errlogPrintf("%s TYP%1.1d type must be ab1771N_TYPE_VA\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_VA;
		}
	    }
	    for(signal=4; signal<6; signal++,pcf++) {
		if(pcf->typ < ab1771N_TYPE_RTDPtEurope) {
		    errlogPrintf("%s TYP%1.1d type must be RTD\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_RTDCopper;
		}
	    }
	    for(signal=6; signal<nChan; signal++,pcf++) {
		if(pcf->typ >= ab1771N_TYPE_RTDPtEurope) {
		    errlogPrintf("%s TYP%1.1d must NOT be RTD\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_VA;
		}
	    }
	    break;
	}
        case ab1771N_NX3: {
	    nChan=8; nOutChan=4;
	    pcf = (channelFields *)&precord->typ1;
	    for(signal=0; signal<4; signal++,pcf++) {
		if(pcf->typ != ab1771N_TYPE_VA) {
		    errlogPrintf("%s TYP%1.1d type must be ab1771N_TYPE_VA\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_VA;
		}
	    }
	    for(signal=4; signal<7; signal++,pcf++) {
		if(pcf->typ < ab1771N_TYPE_RTDPtEurope) {
		    errlogPrintf("%s TYP%1.1d type must be RTD\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_RTDCopper;
		}
	    }
	    for(signal=7; signal<nChan; signal++,pcf++) {
		if(pcf->typ >= ab1771N_TYPE_RTDPtEurope) {
		    errlogPrintf("%s TYP%1.1d must NOT be RTD\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_VA;
		}
	    }
	    break;
	}
        case ab1771N_NX4: {
	    nChan=8; nOutChan=6;
	    pcf = (channelFields *)&precord->typ1;
	    for(signal=0; signal<6; signal++,pcf++) {
		if(pcf->typ != ab1771N_TYPE_VA) {
		    errlogPrintf("%s TYP%1.1d type must be ab1771N_TYPE_VA\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_VA;
		}
	    }
	    for(signal=6; signal<7; signal++,pcf++) {
		if(pcf->typ < ab1771N_TYPE_RTDPtEurope) {
		    errlogPrintf("%s TYP%1.1d type must be RTD\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_RTDCopper;
		}
	    }
	    for(signal=7; signal<nChan; signal++,pcf++) {
		if(pcf->typ >= ab1771N_TYPE_RTDPtEurope) {
		    errlogPrintf("%s TYP%1.1d must NOT be RTD\n",
			precord->name,signal);
		    pcf->typ = ab1771N_TYPE_VA;
		}
	    }
	    break;
	}
	default:
	    errlogPrintf("Logic error: reached default\n");
	    break;
    }
    precordPvt->nOutChan = nOutChan;
    precordPvt->nChan = nChan;
}

/*CODE that interacts with drvAb*/
LOCAL void drvCallback(void *drvPvt)
{
    ab1771NRecord *precord;
    recordPvt		 *precordPvt;

    /*run myCallback under general purpose callback task*/
    precord = (*pabDrv->getUserPvt)(drvPvt);
    precordPvt = precord->dpvt;
    callbackSetPriority(precord->prio,&precordPvt->callback);
    callbackRequest(&precordPvt->callback);
}

LOCAL void myCallback(CALLBACK *pCallback)
{
    ab1771NRecord *precord;
    recordPvt	  *precordPvt;
    int           callLock;

    callbackGetUser(precord,pCallback);
    callLock = interruptAccept;
    if(callLock)dbScanLock((void *)precord);
    precordPvt = precord->dpvt;
    precordPvt->status = (*pabDrv->getStatus)(precordPvt->drvPvt);
    if(precordPvt->status!=abSuccess) {
        switch(precordPvt->msgState) {
	case msgStateWaitInitialOutputs:
	    precordPvt->msgState = msgStateGetInitialOutputs;
	    break;
	case msgStateWaitInit:
	case msgStateWaitPut:
	case msgStateWaitGet:
	    precordPvt->msgState = msgStateInit;
	    break;
        default:
	    errlogPrintf("ILLEGAL myCallback state: record %s\n",precord->name);
	    break;
        }
	issueError(precord,errAb,0);
    } else {
        precordPvt->err = errOK;
        switch(precordPvt->msgState) {
	case msgStateWaitInitialOutputs:
	    msgCompleteInitialOutputs(precord);
	    break;
	case msgStateWaitInit:
	    if(precordPvt->nOutChan > 0)
	        precordPvt->msgState = msgStatePut;
	    else
	        precordPvt->msgState = msgStateGet;
	    break;
	case msgStateWaitPut:
	    precordPvt->msgState = msgStateGet;
	    break;
	case msgStateWaitGet:
	    msgCompleteGet(precord);
	    if(precordPvt->err == errOK) {
		setValMsg(precord,0);
		issueAsynCallback(precord);
	    }
            break;
        default:
	    errlogPrintf("ILLEGAL myCallback state: record %s\n",precord->name);
	    break;
	}
	if(precordPvt->err == errOK) switch(precordPvt->msgState) {
	case msgStateGetInitialOutputs:msgGetInitialOutputs(precord); break;
        case msgStateInit:
            /*If module has any outputs then this code can be reached
              during record initialition. We dont want to initialize outputs
              until record is being processed so that there is a chance
              to restore setpoints.
            */
             if(precordPvt->processCalled) msgInit(precord);
             break;
	case msgStatePut: msgPut(precord); break;
	case msgStateGet: msgGet(precord); break;
	default: break;
	}
    }
    if(precordPvt->err != errOK) issueAsynCallback(precord);
    if(callLock)dbScanUnlock((void *)precord);
}

LOCAL void msgInitOutMsg(ab1771NRecord *precord)
{
    recordPvt		*precordPvt = (recordPvt *)precord->dpvt;
    channelFields	*pcf;
    int                 nOutChan = precordPvt->nOutChan;
    int                 signal;
    short               *pmsg;

    precordPvt->newOutMsg = TRUE;
    pcf = (channelFields *)&precord->typ1;
    pmsg = precord->outm;
    *pmsg++ = (nOutChan<<4) | 0x8800;
    pcf = (channelFields *)&precord->typ1;
    for(signal=0;signal<precordPvt->nOutChan; signal++,pcf++) {
	pmsg++;
    }
    *pmsg++ = (precord->cjae<<15) | (precord->scal==ab1771N_UnitsF ? 0x2 : 0);
    *pmsg++ = 0;
    pcf = (channelFields *)&precord->typ1;
    for(signal=0; signal<precordPvt->nChan; signal++,pcf++) {
	*pmsg++ = -32768;
	*pmsg++ = +32767;
	if(signal<precordPvt->nOutChan) {
	    *pmsg++ = -32768;
	    *pmsg++ = +32767;
	    *pmsg++ = 0;
	    *pmsg++ = 0;
	} else {
	    *pmsg++ = 0;
	    *pmsg++ = 0;
	    *pmsg++ = 0;
	    *pmsg++ = ((short)(pcf->fil*10.0)<<8);
	    if(pcf->typ>=ab1771N_TYPE_tcB && pcf->typ<=ab1771N_TYPE_tcN) {
		*pmsg++ = (pcf->typ - ab1771N_TYPE_tcB + 1) << 12;
	    } else if(pcf->typ>=ab1771N_TYPE_RTDPtEurope
	    && pcf->typ<=ab1771N_TYPE_RTDNickel){
		*pmsg++ = ((pcf->typ - ab1771N_TYPE_RTDPtEurope + 1) << 8)
			  | ((short)(pcf->ohm*100.0));
	    } else {
		pmsg++;
	    }
	}
    }
    return;
}

LOCAL void msgCheckInpHeader(ab1771NRecord *precord)
{
    recordPvt		*precordPvt = (recordPvt *)precord->dpvt;
    unsigned short      *pmsg;
    unsigned short      stat;

    precordPvt->newInMsg = TRUE;
    pmsg = (unsigned short*)precord->inpm;
    if(*pmsg++ != 0x8800) {
	errlogPrintf("%s inMessageConvert word 0 not 0x8800\n",precord->name);
	issueError(precord,errAb,"input word 0 not 0x8800");
	return;
    }
    if((stat = *pmsg++)) {/* Is any module error bit set*/
        if(stat & !0x8800) {/*Is it one we dont expect*/
	    errlogPrintf("%s inMessageConvert word 1 bad %4.4x\n",
                precord->name,stat);
	    issueError(precord,errAb,"header word 1 error");
	    return;
	}
	issueError(precord,errOK,"module needs initialization");
	precordPvt->msgState = msgStateInit;
    }
    if(precordPvt->nOutChan==precordPvt->nChan) {
	stat = *pmsg++;
	if(stat!=0x8000) {
	    errlogPrintf("%s inMessageConvert word 2 bad %4.4x\n",
                precord->name,stat);
	    issueError(precord,errAb,"input word 2 bad");
	    return;
	}
	stat = *pmsg++;
	if(stat) {
	    errlogPrintf("%s inMessageConvert word 3 bad %4.4x",
                precord->name,stat);
	    issueError(precord,errAb,"input word 3 bad");
	    return;
	}
    } else {
	stat = *pmsg++;
	if((stat&0xfffc) != 0x8000) {
	    errlogPrintf("%s inMessageConvert word 2 bad %4.4x",
                precord->name,stat);
	    issueError(precord,errAb,"input word 2 bad");
	    return;
	}
	stat &= 0x3;
	switch(stat) {
	    case 0:  precord->cjs = ab1771N_CJS_Ok;         break;
	    case 1:  precord->cjs = ab1771N_CJS_UnderRange; break;
	    case 2:  precord->cjs = ab1771N_CJS_OverRange;  break;
	    default: precord->cjs = ab1771N_CJS_Bad;        break;
	}
	precord->cjt = (short)*pmsg++
	    / (precord->scal==ab1771N_UnitsF ? 10.0 : 100.0);
    }
    return;
}

LOCAL void msgGetInitialOutputs(ab1771NRecord *precord)
{
    recordPvt		*precordPvt = (recordPvt *)precord->dpvt;
    unsigned short      nwords;

    precordPvt->newInMsg = TRUE;
    nwords = 4 + 2*precordPvt->nOutChan;
    precordPvt->msgState = msgStateWaitInitialOutputs;
    precordPvt->status = (*pabDrv->btRead)(precordPvt->drvPvt,
	(unsigned short *)precord->inpm,nwords);
    if(precordPvt->status!=abBtqueued) {
	precordPvt->msgState = msgStateGetInitialOutputs;
	issueError(precord,errAb,"msgGetInitialOutputs");
	return;
    }
    return;
}

LOCAL void msgCompleteInitialOutputs(ab1771NRecord *precord)
{
    recordPvt		*precordPvt = (recordPvt *)precord->dpvt;
    channelFields	*pcf;
    int                 signal;
    short               *pmsg;
    unsigned short      stat;

    pmsg = precord->inpm;
    msgCheckInpHeader(precord);
    if(precordPvt->err!=errOK) return;
    precordPvt->msgState = msgStateInit;
    pmsg += 4;
    pcf = (channelFields *)&precord->typ1;
    for(signal=0; signal<precordPvt->nOutChan; signal++,pcf++) {
	stat = (unsigned short)*pmsg++;
	if(stat&0x8000) {   /*does module say it is input*/
	    errlogPrintf("%s inMessageConvert chan %d is input not output\n",
		    precord->name,signal);
	    issueError(precord,errFatal,
		    "channel declared output that is input");
		return;
	}
	pcf->sta = stat;
	pcf->raw = *pmsg;
	precord->outm[1+signal] = *pmsg++;
    }
    return;
}

LOCAL void msgInit(ab1771NRecord *precord)
{
    recordPvt		*precordPvt = (recordPvt *)precord->dpvt;
    unsigned short      nwords;

    msgInitOutMsg(precord);
    if(precordPvt->err!=errOK) return;
    nwords = 1 + 2 + 6*precordPvt->nOutChan
	+ 7*(precordPvt->nChan - precordPvt->nOutChan);
    precordPvt->msgState = msgStateWaitInit;
    precordPvt->status = (*pabDrv->btWrite)(precordPvt->drvPvt,
	(unsigned short *)precord->outm,nwords);
    if(precordPvt->status!=abBtqueued) {
	precordPvt->msgState = msgStateInit;
	issueError(precord,errAb,0);
	return;
    }
    return;
}

LOCAL void msgPut(ab1771NRecord *precord)
{
    recordPvt	  *precordPvt = (recordPvt *)precord->dpvt;
    int           nwords;
 
    precordPvt->needsPut = FALSE;
    precordPvt->newOutMsg = TRUE;
    nwords = 1 + 2 +precordPvt->nOutChan + 6*precordPvt->nOutChan
	+ 7*(precordPvt->nChan - precordPvt->nOutChan);
    precordPvt->msgState = msgStateWaitPut;
    precordPvt->status = (*pabDrv->btWrite)(precordPvt->drvPvt,
	(unsigned short *)precord->outm,nwords);
    if(precordPvt->status!=abBtqueued) {
	precordPvt->msgState = msgStateInit;
	issueError(precord,errAb,"msgPut");
	return;
    }
    return;
}

LOCAL void msgGet(ab1771NRecord *precord)
{
    recordPvt		*precordPvt = (recordPvt *)precord->dpvt;
    unsigned short      nwords;

    if(precordPvt->needsPut) {
	msgPut(precord);
	return;
    }
    nwords = 4 + 2*precordPvt->nChan;
    precordPvt->msgState = msgStateWaitGet;
    precordPvt->status = (*pabDrv->btRead)(precordPvt->drvPvt,
	(unsigned short *)precord->inpm,nwords);
    if(precordPvt->status!=abBtqueued) {
	precordPvt->msgState = msgStateGet;
	issueError(precord,errAb,"msgGet");
	return;
    }
    return;

}

LOCAL void msgCompleteGet(ab1771NRecord *precord)
{
    recordPvt		*precordPvt = (recordPvt *)precord->dpvt;
    channelFields	*pcf;
    int                 signal;
    short               *pmsg;
    unsigned short      stat;

    msgCheckInpHeader(precord);
    if(precordPvt->err!=errOK) {
        precordPvt->msgState = msgStateInit;
        return;
    }
    precordPvt->msgState = msgStateDone;
    pmsg = precord->inpm + 4;
    pcf = (channelFields *)&precord->typ1;
    for(signal=0; signal<precordPvt->nChan; signal++,pcf++) {
	stat = (unsigned short)*pmsg++;
	if(signal<precordPvt->nOutChan) {
	    if(stat&0x8000) {   /*does module say it is input*/
		errlogPrintf("%s inMessageConvert chan %d is input not output\n",
		    precord->name,signal);
		issueError(precord,errFatal,"input declared output");
		return;
	    }
	    pcf->sta = stat;
	} else {
	    if((stat&0x8000)==0) {   /*does module say it is output*/
		errlogPrintf("%s inMessageConvert chan %d is output not input\n",
		    precord->name,signal);
		issueError(precord,errFatal,"output declared input");
		return;
	    }
	    if(stat&0x7ffc) { /*should only have over/under range set*/
		errlogPrintf("%s inMessageConvert chan %d bad status %d\n",
		    precord->name,signal,stat);
		issueError(precord,errFatal,"output declared input");
		return;
	    }
	    stat &= 0x0003;
	    if(stat==1) pcf->sta = ab1771NUnderflow;
	    else if(stat==2) pcf->sta = ab1771NOverflow;
	    else pcf->sta = 0;
	}
	pcf->raw = *pmsg++;
    }
    return;
}

/*CODE INTERFACE FOR DEVICE SUPPORT*/
/*Definitions for handling requests from device support*/
typedef struct deviceData{
    ab1771NRecord *precord;
    int           signal;
}deviceData;


#define MAX_BUFFER 80
LOCAL int connect(void **pab1771NPvt,DBLINK *plink,
	boolean isOutput,boolean *isEng)
{
    DBENTRY       dbEntry;
    DBENTRY       *pdbEntry = &dbEntry;
    DBADDR        dbAddr;
    deviceData	  *pdeviceData;
    ab1771NRecord *precord;
    recordPvt	  *precordPvt;
    long	  status;
    channelFields *pcf;
    char          buffer[MAX_BUFFER];
    char          *recordname = &buffer[0];
    char          *pstring;
    unsigned short signal;
    char          *pLeftbracket;
    char          *pRightbracket;

    if(plink->type!=INST_IO) return(ab1771NFatalError);
    pstring = plink->value.instio.string;
    if(strlen(pstring)>=MAX_BUFFER) return(ab1771NFatalError);
    strcpy(buffer,pstring);
    pLeftbracket = strchr(buffer,'[');
    pRightbracket = strchr(buffer,']');
    if(!pLeftbracket || !pRightbracket) {
	errlogPrintf("link was not of the form record[signal]\n");
	return(ab1771NFatalError);
    }
    *pLeftbracket++ = '\0';
    *pRightbracket = '\0';
    sscanf(pLeftbracket,"%hu",&signal);
    dbInitEntry(pdbbase,pdbEntry);
    status = dbFindRecord(pdbEntry,recordname);
    if(status) return(ab1771NNoRecord);
    if(strcmp(dbGetRecordTypeName(pdbEntry),"ab1771N")!=0)
	return(ab1771NIllegalRecordType);
    dbFinishEntry(pdbEntry);
    status = dbNameToAddr(recordname,&dbAddr);
    if(status) return(ab1771NNoRecord);
    precord = (ab1771NRecord *)dbAddr.precord;
    if(!(precordPvt = (recordPvt *)precord->dpvt)) {
	printf("%s precordPvt is NULL ?\n",precord->name);
	return(ab1771NFatalError);
    }
    if(signal>=precordPvt->nChan) return(ab1771NIllegalType);
    if(isOutput && signal>=precordPvt->nOutChan) return(ab1771NIllegalType);
    pcf = (channelFields *)&precord->typ1;
    pcf += signal;
    *isEng = ((pcf->typ==ab1771N_TYPE_VA) ? FALSE : TRUE);
    pdeviceData = dbCalloc(1,sizeof(deviceData));
    pdeviceData->precord = precord;
    pdeviceData->signal = signal;
    *pab1771NPvt = (void *)pdeviceData;
    return(ab1771NOK);
}

LOCAL char *illegalStatus = "Illegal Status Value";
LOCAL char *get_status_message(void *pdevIai1Pvt,int status) {
    if(status<0 || status>ab1771NFatalError) return(illegalStatus);
    return(ab1771NStatusMessage[status]);
}

LOCAL int get_linconv(void *ab1771NPvt,long *range,long *roff)
{
    deviceData    *pdeviceData = (deviceData *)ab1771NPvt;
    ab1771NRecord *precord;
    recordPvt	  *precordPvt;

    if(!pdeviceData) return(ab1771NFatalError);
    if(!(precord = pdeviceData->precord)) return(ab1771NFatalError);
    if(!(precordPvt = (recordPvt *)precord->dpvt)) return(ab1771NFatalError);
    *range = 65536;
    *roff = 32768;
    return(0);
}

LOCAL int get_ioint_info(void *ab1771NPvt,int cmd, IOSCANPVT *ppvt)
{
    deviceData    *pdeviceData = (deviceData *)ab1771NPvt;
    ab1771NRecord *precord;
    recordPvt	  *precordPvt;
    int           signal;

    if(!pdeviceData) return(ab1771NFatalError);
    if(!(precord = pdeviceData->precord)) return(ab1771NFatalError);
    if(!(precordPvt = (recordPvt *)precord->dpvt)) return(ab1771NFatalError);
    signal = pdeviceData->signal;
    if(signal<precordPvt->nOutChan || signal>=precordPvt->nChan) {
	errlogPrintf("%s get_ioint_info Illegal signal\n",precord->name);
	return(ab1771NFatalError);
    }
    if(!precordPvt->ioscanpvtInited[signal]) {
        scanIoInit(&precordPvt->ioscanpvt[signal]);
	precordPvt->ioscanpvtInited[pdeviceData->signal] = TRUE;
    }
    *ppvt = precordPvt->ioscanpvt[signal];
    return(0);
}

LOCAL int get_raw(void *ab1771NPvt,long *praw)
{
    deviceData	  *pdeviceData = (deviceData *)ab1771NPvt;
    ab1771NRecord *precord;
    recordPvt	  *precordPvt;
    channelFields *pcf;

    if(!pdeviceData) return(ab1771NFatalError);
    if(!(precord = pdeviceData->precord)) return(ab1771NFatalError);
    if(!(precordPvt = (recordPvt *)precord->dpvt)) return(ab1771NFatalError);
    if((precordPvt->status != abSuccess)
    && (precordPvt->status != abBtqueued)) return(ab1771NAbError);
    pcf = (channelFields *)&precord->typ1;
    pcf += pdeviceData->signal;
    if(pcf->typ!=ab1771N_TYPE_VA){
	errlogPrintf("%s get_raw Logic Error in device support\n",precord->name);
	return(ab1771NFatalError);
    }
    *praw = (long)pcf->raw;
    return((ab1771NStatus)pcf->sta);
}

LOCAL int put_raw(void *ab1771NPvt,long raw)
{
    deviceData	  *pdeviceData = (deviceData *)ab1771NPvt;
    ab1771NRecord *precord;
    recordPvt	  *precordPvt;

    if(!pdeviceData) return(ab1771NFatalError);
    if(!(precord = pdeviceData->precord)) return(ab1771NFatalError);
    if(!(precordPvt = (recordPvt *)precord->dpvt)) return(ab1771NFatalError);
    if(pdeviceData->signal>=precordPvt->nOutChan) return(ab1771NFatalError);
    if(raw>32767) raw = 32767;
    if(raw < -32767) raw = -32767;
    precord->outm[1+pdeviceData->signal] = (short)raw;
    precordPvt->needsPut = TRUE;
    if((precordPvt->status != abSuccess)
    && (precordPvt->status != abBtqueued)) return(ab1771NAbError);
    return(0);
}

LOCAL int get_eng(void *ab1771NPvt,double *peng)
{
    deviceData	  *pdeviceData = (deviceData *)ab1771NPvt;
    ab1771NRecord *precord;
    recordPvt	  *precordPvt;
    channelFields *pcf;

    if(!pdeviceData) return(ab1771NFatalError);
    if(!(precord = pdeviceData->precord)) return(ab1771NFatalError);
    if(!(precordPvt = (recordPvt *)precord->dpvt)) return(ab1771NFatalError);
    if((precordPvt->status != abSuccess)
    && (precordPvt->status != abBtqueued)) return(ab1771NAbError);
    pcf = (channelFields *)&precord->typ1;
    pcf += pdeviceData->signal;
    if(pcf->typ==ab1771N_TYPE_VA){
	errlogPrintf("%s get_eng Logic Error in device support\n",precord->name);
	return(ab1771NFatalError);
    }
    if(pcf->typ>=ab1771N_TYPE_RTDPtEurope) {
	switch(precord->scal) {
	    case ab1771N_UnitsF:
	        *peng = ((1652.0 + 328.0)/65535.0)
		    * ((long)pcf->raw + 32767) - 328.0;
		break;
	    case ab1771N_UnitsC:
	        *peng = ((900.0 + 200.0)/65535.0)
		    * ((long)pcf->raw + 32767) - 200.0;
		break;
	    case ab1771N_UnitsOhms:
	        *peng = ((650.0 - 1.0)/65535.0)
		    * ((long)pcf->raw + 32767) + 1.0;
		break;
	}
    } else { /*Must be thermocouple*/
	switch(precord->scal) {
	    case ab1771N_UnitsF:
	        *peng = ((3272.0 + 508.0)/65535.0)
		    * ((long)pcf->raw + 32767) - 508.0;
		break;
	    case ab1771N_UnitsC:
	        *peng = ((1800.0 + 300.0)/65535.0)
		    * ((long)pcf->raw + 32767) - 300.0;
		break;
	    case ab1771N_UnitsOhms:
	        return(ab1771NAbError);
	}
    }
    return((ab1771NStatus)pcf->sta);
}


LOCAL int ai_connect(void **pdevIai1Pvt,DBLINK *plink, boolean *isEng) {
    return(connect(pdevIai1Pvt,plink,FALSE,isEng));
}
LOCAL char *ai_get_status_message(void *pdevIai1Pvt,int status) {
    return(get_status_message(pdevIai1Pvt,status));
}
LOCAL int ai_get_ioint_info(void *devIai1Pvt,int cmd, IOSCANPVT *ppvt) {
    return(get_ioint_info(devIai1Pvt,cmd,ppvt));
}
LOCAL int ai_get_linconv(void *devIai1Pvt,long *range,long *roff) {
    return(get_linconv(devIai1Pvt,range,roff));
}
LOCAL int ai_get_raw(void *devIai1Pvt,long *praw) {
    return(get_raw(devIai1Pvt,praw));
}
LOCAL int ai_get_eng(void *devIai1Pvt,double *peng) {
    return(get_eng(devIai1Pvt,peng));
}

LOCAL int ao_connect(void **pdevIao1Pvt,DBLINK *plink, boolean *isEng) {
    return(connect(pdevIao1Pvt,plink,TRUE,isEng));
}
LOCAL char *ao_get_status_message(void *pdevIao1Pvt,int status) {
    return(get_status_message(pdevIao1Pvt,status));
}
LOCAL int ao_get_linconv(void *devIao1Pvt,long *range,long *roff) {
    return(get_linconv(devIao1Pvt,range,roff));
}
LOCAL int ao_get_raw(void *devIao1Pvt,long *praw) {
    return(get_raw(devIao1Pvt,praw));
}
LOCAL int ao_get_eng(void *devIao1Pvt,double *peng) {
    return(ab1771NFatalError);
}
LOCAL int ao_put_raw(void *devIao1Pvt,long raw) {
    return(put_raw(devIao1Pvt,raw));
}
LOCAL int ao_put_eng(void *devIao1Pvt,double eng) {
    return(ab1771NFatalError);
}

/*utility routine to dump raw messages*/
int ab1771Npm(char *recordname)
{
    DBENTRY       dbEntry;
    DBENTRY       *pdbEntry = &dbEntry;
    DBADDR        dbAddr;
    ab1771NRecord *precord;
    recordPvt	  *precordPvt;
    long	  status;
    short	  *pdata;
    int           i;

    dbInitEntry(pdbbase,pdbEntry);
    status = dbFindRecord(pdbEntry,recordname);
    if(status) {
	printf("Cant find %s\n",recordname);
	return(0);
    }
    if(strcmp(dbGetRecordTypeName(pdbEntry),"ab1771N")!=0) {
	printf("Not a ab1771NRecord\n");
	return(0);
    }
    dbFinishEntry(pdbEntry);
    status = dbNameToAddr(recordname,&dbAddr);
    if(status) {
	printf("Cant find %s\n",recordname);
	return(0);
    }
    precord = (ab1771NRecord *)dbAddr.precord;
    if(!(precordPvt = (recordPvt *)precord->dpvt)) {
	printf("dpvt is NULL\n");
	return(0);
    }
    printf("output message");
    pdata = precord->outm;
    for(i=0; i< MAX_WORDS_OUT; i++) {
	if(i%10 == 0) printf("\n");
	printf(" %4hx",*pdata);
	pdata++;
    }
    printf("\ninput message");
    pdata = precord->inpm;
    for(i=0; i< MAX_WORDS_IN; i++) {
	if(i%10 == 0) printf("\n");
	printf(" %4hx",*pdata);
	pdata++;
    }
    printf("\n");
    return(0);
}
