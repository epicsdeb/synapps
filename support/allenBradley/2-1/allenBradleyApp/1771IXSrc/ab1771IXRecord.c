/* ab1771IXRecord.c*/
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
 *      Date:   Nov 21, 1997
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
#include "ab1771IXRecord.h"
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
 
struct rset ab1771IXRSET={
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
epicsExportAddress(rset,ab1771IXRSET);

LOCAL void monitor(ab1771IXRecord *precord);

int ab1771IXDebug=0;

/*structure for set of fields in ab1771IXRecord.h describing one channel*/
typedef struct channelFields {
    short          sta;
    short          raw;
} channelFields;

#define NUM_CHANS     8
#define NUM_WORDS_IN  12
#define NUM_WORDS_OUT 27

static int typeMask[8] = {0, 7, 1, 2, 3, 5, 6, 4};

/*definitions for messages to allen bradley driver*/
typedef enum {
    msgStateInit,
    msgStateWaitInit,
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
    IOSCANPVT   ioscanpvt[NUM_CHANS];
    char	ioscanpvtInited[NUM_CHANS];
    void        *drvPvt;
    msgStates	msgState;
    boolean     waitAsynComplete;
    boolean     newValMsg;
    boolean     newInMsg;
    boolean     newOutMsg;
}recordPvt;

typedef enum {
    ab1771IXOK=devIai1OK,
    ab1771IXAbError=devIai1HardwareFault,
    ab1771IXUnderflow=devIai1Underflow,
    ab1771IXOverflow=devIai1Overflow,
    ab1771IXNoRecord,
    ab1771IXIllegalRecordType,
    ab1771IXIllegalType,
    ab1771IXFatalError
} ab1771IXStatus;

LOCAL char *ab1771IXStatusMessage[] = {
	"ab1771IX OK",
	"ab1771IX drvAb error",
	"ab1771IX Underflow",
	"ab1771IX Overflow",
	"ab1771IX Record not found",
	"ab1771IX Not an ab1771IXRecord",
	"ab1771IX in/out type mismatch",
	"ab1771IX fatal error"
};

/* forward references for local routines */
LOCAL void issueAsynCallback(ab1771IXRecord *precord);
LOCAL void setValMsg(ab1771IXRecord *precord,char *msg);
LOCAL void issueError(ab1771IXRecord *precord,errStatus err,char *msg);

LOCAL void drvCallback(void *drvPvt);
LOCAL void myCallback(CALLBACK *pCallback);

LOCAL void msgInit(ab1771IXRecord *precord);
LOCAL void msgGet(ab1771IXRecord *precord);
LOCAL void msgCompleteGet(ab1771IXRecord *precord);

/*Interface routines for devIai1 and devIao1*/
LOCAL int connect(void **pdevIai1Pvt,DBLINK *plink, boolean *isEng);
LOCAL char *get_status_message(void *pdevIai1Pvt,int status);
LOCAL int get_ioint_info(void *devIai1Pvt,int cmd, IOSCANPVT *ppvt);
LOCAL int get_linconv(void *devIai1Pvt,long *range,long *roff);
LOCAL int get_raw(void *devIai1Pvt,long *praw);
LOCAL int get_eng(void *devIai1Pvt,double *peng);

LOCAL devIai1 mydevIai1= {
    connect,get_status_message,get_ioint_info,
    get_linconv,get_raw,get_eng
};

extern DBBASE *pdbbase;

/*BEGIN STANDARD RECORD SUPPORT ROUTINES*/
LOCAL long init_record(ab1771IXRecord *precord,int pass)
{
    abStatus		status;
    recordPvt		*precordPvt;

    if(pass!=0) return(0);
    precord->iai1 = &mydevIai1;
    precord->dpvt = precordPvt = dbCalloc(1,sizeof(recordPvt));
    callbackSetCallback(myCallback,&precordPvt->callback);
    callbackSetUser(precord,&precordPvt->callback);
    precord->inpm = (short *)dbCalloc(NUM_WORDS_IN,sizeof(short));
    precord->outm = (short *)dbCalloc(NUM_WORDS_OUT,sizeof(short));
    status = (*pabDrv->registerCard)(precord->link,precord->rack,
	precord->slot,typeBt,"ab1771IX",drvCallback,&precordPvt->drvPvt);
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
    msgInit(precord);
    precord->udf = FALSE;
    return(0);
}

LOCAL long process(ab1771IXRecord *precord)
{
    recordPvt	*precordPvt = (recordPvt *)precord->dpvt;

    if(precordPvt->err == errFatal) {
	precord->pact = TRUE;
	return(0);
    }
    if(!precord->pact) {/* starting new request*/
	if(precord->cmd==ab1771IX_Init) {
	    precordPvt->msgState = msgStateInit;
	    precord->cmd = ab1771IX_CMD_None;
	    db_post_events(precord,&precord->cmd,DBE_VALUE|DBE_LOG);
	}
	precordPvt->err = errOK;
  	/* starting new request*/
	switch(precordPvt->msgState) {
	    case msgStateInit:
	        msgInit(precord);
		break;
	    case msgStateGet:
	    case msgStateDone:
		msgGet(precord);
		break;
	    default: break;
	}/*Note that state may switch because of msgRequest just issued*/
    }
    switch(precordPvt->msgState) {
	case msgStateWaitInit: 
	case msgStateWaitGet: 
	    precord->pact = TRUE;
	    precordPvt->waitAsynComplete = TRUE;
	    return(0);
	default:
	    break;
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
    ab1771IXRecord  *precord;
    recordPvt      *precordPvt;

    precord = (ab1771IXRecord *)paddr->precord;
    precordPvt = (recordPvt *)precord->dpvt;
    ind = dbGetFieldIndex(paddr);
    switch(ind) {
	case ab1771IXRecordINPM:
	    paddr->pfield = precord->inpm;
	    paddr->field_type = DBF_SHORT;
	    paddr->dbr_field_type = DBR_SHORT;
	    paddr->no_elements = NUM_WORDS_IN;
	    paddr->field_size = sizeof(short);
	    break;
	case ab1771IXRecordOUTM:
	    paddr->pfield = precord->outm;
	    paddr->field_type = DBF_SHORT;
	    paddr->dbr_field_type = DBR_SHORT;
	    paddr->no_elements = NUM_WORDS_OUT;
	    paddr->field_size = sizeof(short);
	    break;
	case ab1771IXRecordIAI1:
	    paddr->pfield = precord->iai1;
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
    ab1771IXRecord *precord;
    recordPvt           *precordPvt;

    precord = (ab1771IXRecord *)paddr->precord;
    precordPvt = (recordPvt *)precord->dpvt;
    if(dbGetFieldIndex(paddr)==ab1771IXRecordINPM) {
	*no_elements = NUM_WORDS_IN;
	*offset = 0;
    } else if(dbGetFieldIndex(paddr)==ab1771IXRecordOUTM) {
	*no_elements = NUM_WORDS_OUT;
	*offset = 0;
    } else {
	errlogPrintf("%s get_array_info for illegal field\n",precord->name);
    }
    return(0);
}

LOCAL void monitor(ab1771IXRecord *precord)
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
    if(precordPvt->newValMsg) mask |= DBE_VALUE|DBE_LOG;
    if(mask) db_post_events(precord,precord->val,mask);
    mask = monitor_mask;
    if(precordPvt->newInMsg) mask |= DBE_VALUE|DBE_LOG;
    if(mask) {
	db_post_events(precord,precord->inpm,mask);
    }
    mask = monitor_mask;
    if(precordPvt->newOutMsg) mask |= DBE_VALUE|DBE_LOG;
    if(mask) {
	db_post_events(precord,precord->outm,mask);
    }
    if((precordPvt->newInMsg) 
    || (precord->loca && (precordPvt->prevStatus!=precordPvt->status))) {
	for(signal=0, pcf = (channelFields *)&precord->sta1;
	signal<NUM_CHANS; signal++, pcf++) {
	    mask = monitor_mask |= DBE_VALUE|DBE_LOG;
	    if(precordPvt->newInMsg) {
		db_post_events(precord,&pcf->raw,mask);
		db_post_events(precord,&pcf->sta,mask);
	    }
	    if(precordPvt->ioscanpvtInited[signal])
	        scanIoRequest(precordPvt->ioscanpvt[signal]);
	}
	monitor_mask |= DBE_VALUE|DBE_LOG;
	db_post_events(precord,&precord->cjt,monitor_mask);
    }
    precordPvt->prevStatus = precordPvt->status;
    precordPvt->newValMsg = FALSE;
    precordPvt->newInMsg = FALSE;
    precordPvt->newOutMsg = FALSE;
    return;
}

/*BEGIN UTILITY ROUTINES*/
LOCAL void issueAsynCallback(ab1771IXRecord *precord)
{
    recordPvt     *precordPvt = (recordPvt *)precord->dpvt;
    struct rset   *prset=(struct rset *)(precord->rset);

    if(precordPvt->waitAsynComplete) {
	(*prset->process)(precord);
    }
}

LOCAL void setValMsg(ab1771IXRecord *precord,char *msg)
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

LOCAL void issueError(ab1771IXRecord *precord,errStatus err,char *msg)
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

/*CODE that interacts with drvAb*/
LOCAL void drvCallback(void *drvPvt)
{
    ab1771IXRecord *precord;
    recordPvt	 *precordPvt;

    /*run myCallback under general purpose callback task*/
    precord = (*pabDrv->getUserPvt)(drvPvt);
    precordPvt = precord->dpvt;
    callbackSetPriority(precord->prio,&precordPvt->callback);
    callbackRequest(&precordPvt->callback);
}

LOCAL void myCallback(CALLBACK *pCallback)
{
    ab1771IXRecord *precord;
    recordPvt	  *precordPvt;
    int           callLock;

    callbackGetUser(precord,pCallback);
    callLock = interruptAccept;
    if(callLock)dbScanLock((void *)precord);
    precordPvt = precord->dpvt;
    precordPvt->status = (*pabDrv->getStatus)(precordPvt->drvPvt);
    if(precordPvt->status!=abSuccess) {
        switch(precordPvt->msgState) {
	case msgStateWaitInit:
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
	case msgStateWaitInit:
	    precordPvt->msgState = msgStateGet;
	    break;;
	case msgStateWaitGet:
	    setValMsg(precord,0);
	    msgCompleteGet(precord);
	    issueAsynCallback(precord);
            break;;
        default:
	    errlogPrintf("ILLEGAL myCallback state: record %s\n",precord->name);
	    break;
	}
	if(precordPvt->err == errOK) switch(precordPvt->msgState) {
	    case msgStateInit: msgInit(precord); break;
	    case msgStateGet: msgGet(precord); break;
	    default: break;
	}
    }
    if(precordPvt->err != errOK) issueAsynCallback(precord);
    if(callLock)dbScanUnlock((void *)precord);
}

LOCAL void msgInit(ab1771IXRecord *precord)
{
    recordPvt		*precordPvt = (recordPvt *)precord->dpvt;
    short               *pmsg;
    int			i;

    if(precord->mtyp==ab1771IX_E) {
	if((precord->typa==ab1771IX_TYPE_tcB)
	|| (precord->typb==ab1771IX_TYPE_tcB)) {
	    errlogPrintf("%s Type B Tcf illegal for 1771-IXE\n",precord->name);
	    precord->typa = precord->typb = ab1771IX_TYPE_MV;
	}
	if(precord->zoom) {
	    errlogPrintf("%s ZOOM not supported for 1771-IXE\n",precord->name);
	    precord->zoom = FALSE;
	}
    }
    pmsg = precord->outm;
    if(precord->mtyp==ab1771IX_HR) {
        *pmsg = precord->stim << 9;
        if(precord->zoom) *pmsg |= 0x80;
    } else {
	*pmsg = precord->stim << 11;
	*pmsg |= 0x0400; /*Signed magnitude*/
    }
    if(precord->scal==ab1771IX_UnitsF) *pmsg |= 0x100;
    *pmsg |= typeMask[precord->typa];
    if(precord->typa!=precord->typb) {
	*pmsg |= 0x40;
	*pmsg |= (typeMask[precord->typb] << 3);
    }
    ++pmsg;
    if(precord->mtyp==ab1771IX_HR) {
        *pmsg++ = (precord->zcb <<8) | precord->zca;
        *pmsg++ = (precord->filb <<8) | precord->fila;
    } else {
	*pmsg++ = 0;
	*pmsg++ = 0;
    }
    for(i=3; i<NUM_WORDS_OUT; i++) *pmsg++ = 0;
    precordPvt->msgState = msgStateWaitInit;
    precordPvt->status = (*pabDrv->btWrite)(precordPvt->drvPvt,
	(unsigned short *)precord->outm,NUM_WORDS_OUT);
    if(precordPvt->status!=abBtqueued) {
	precordPvt->msgState = msgStateInit;
	issueError(precord,errAb,0);
	return;
    }
    precordPvt->newOutMsg = TRUE;
    return;
}

LOCAL void msgGet(ab1771IXRecord *precord)
{
    recordPvt		*precordPvt = (recordPvt *)precord->dpvt;

    precordPvt->msgState = msgStateWaitGet;
    precordPvt->status = (*pabDrv->btRead)(precordPvt->drvPvt,
	(unsigned short *)precord->inpm,NUM_WORDS_IN);
    if(precordPvt->status!=abBtqueued) {
	precordPvt->msgState = msgStateInit;
	issueError(precord,errAb,"msgGet");
	return;
    }
    return;

}

LOCAL void msgCompleteGet(ab1771IXRecord *precord)
{
    recordPvt		*precordPvt = (recordPvt *)precord->dpvt;
    channelFields	*pcf;
    int                 signal;
    short               *pmsg;
    char		statusMsg[100];

    if(precordPvt->err!=errOK) return;
    precordPvt->newInMsg = TRUE;
    pmsg = precord->inpm;
    statusMsg[0] = 0;
    if(pmsg[0] & 0xff) {
	if(pmsg[0]&0x1) strcat(statusMsg,"PowerUp ");
	if(pmsg[0]&0x2) strcat(statusMsg,"OutOfRange ");
	if(pmsg[0]&0x4) strcat(statusMsg,"SampleTimeOut ");
	if(pmsg[0]&0x10) strcat(statusMsg,"Low CJ ");
	if(pmsg[0]&0x20) strcat(statusMsg,"High CJ ");
	setValMsg(precord,statusMsg);
    }
    precordPvt->msgState = msgStateDone;
    pcf = (channelFields *)&precord->sta1;
    for(signal=0; signal<NUM_CHANS; signal++,pcf++) {
	int ind;

	ind = signal+3;
	if(pmsg[1]&(1 << signal)) {
	    pcf->sta = ab1771IXUnderflow;
	} else if(pmsg[1]&(1 << (signal+8))) {
	    pcf->sta = ab1771IXOverflow;
	} else {
	    pcf->sta = devIai1OK;
	}
	pcf->raw = pmsg[ind];
	if(precord->mtyp==ab1771IX_E && (pmsg[0]&(1<<(signal+8))) )
	    pcf->raw = -pcf->raw;
    }
    if(precord->mtyp==ab1771IX_HR) {
        precord->cjt = (double)pmsg[11]/10.0;
    } else {
	precord->cjt = pmsg[11];
    }
    return;
}

/*CODE INTERFACE FOR DEVICE SUPPORT*/
/*Definitions for handling requests from device support*/
typedef struct deviceData{
    ab1771IXRecord *precord;
    int           signal;
}deviceData;

#define MAX_BUFFER 80
LOCAL int connect(void **pab1771IXPvt,DBLINK *plink,boolean *isEng)
{
    DBENTRY       dbEntry;
    DBENTRY       *pdbEntry = &dbEntry;
    DBADDR        dbAddr;
    deviceData	  *pdeviceData;
    ab1771IXRecord *precord;
    recordPvt	  *precordPvt;
    long	  status;
    char          buffer[MAX_BUFFER];
    char          *recordname = &buffer[0];
    char          *pstring;
    unsigned short signal;
    char          *pLeftbracket;
    char          *pRightbracket;

    if(plink->type!=INST_IO) return(ab1771IXFatalError);
    pstring = plink->value.instio.string;
    if(strlen(pstring)>=MAX_BUFFER) return(ab1771IXFatalError);
    strcpy(buffer,pstring);
    pLeftbracket = strchr(buffer,'['); pRightbracket = strchr(buffer,']');
    if(!pLeftbracket || !pRightbracket) {
	errlogPrintf("link was not of the form record[signal]\n");
	return(ab1771IXFatalError);
    }
    *pLeftbracket++ = '\0'; *pRightbracket = '\0';
    sscanf(pLeftbracket,"%hu",&signal);
    dbInitEntry(pdbbase,pdbEntry);
    status = dbFindRecord(pdbEntry,recordname);
    if(status) return(ab1771IXNoRecord);
    if(strcmp(dbGetRecordTypeName(pdbEntry),"ab1771IX")!=0)
	return(ab1771IXIllegalRecordType);
    dbFinishEntry(pdbEntry);
    status = dbNameToAddr(recordname,&dbAddr);
    if(status) return(ab1771IXNoRecord);
    precord = (ab1771IXRecord *)dbAddr.precord;
    if(!(precordPvt = (recordPvt *)precord->dpvt)) {
	printf("%s precordPvt is NULL ?\n",precord->name);
	return(ab1771IXFatalError);
    }
    if(signal>=NUM_CHANS) return(ab1771IXIllegalType);
    if(signal<=3) {
	*isEng = ((precord->typa==ab1771IX_TYPE_MV)?FALSE:TRUE);
    } else {
	*isEng = ((precord->typb==ab1771IX_TYPE_MV)?FALSE:TRUE);
    }
    pdeviceData = dbCalloc(1,sizeof(deviceData));
    pdeviceData->precord = precord;
    pdeviceData->signal = signal;
    *pab1771IXPvt = (void *)pdeviceData;
    return(ab1771IXOK);
}

LOCAL char *illegalStatus = "Illegal Status Value";
LOCAL char *get_status_message(void *pdevIai1Pvt,int status) {
    if(status<0 || status>ab1771IXFatalError) return(illegalStatus);
    return(ab1771IXStatusMessage[status]);
}

LOCAL int get_linconv(void *ab1771IXPvt,long *range,long *roff)
{
    deviceData    *pdeviceData = (deviceData *)ab1771IXPvt;
    ab1771IXRecord *precord;
    recordPvt	  *precordPvt;

    if(!pdeviceData) return(ab1771IXFatalError);
    if(!(precord = pdeviceData->precord)) return(ab1771IXFatalError);
    if(!(precordPvt = (recordPvt *)precord->dpvt)) return(ab1771IXFatalError);
    if(precord->mtyp==ab1771IX_HR) {
        *range = 20000;
        *roff = 10000;
    } else {
	*range = 20000;
	*roff  = 10000;
    }
    return(ab1771IXOK);
}

LOCAL int get_ioint_info(void *ab1771IXPvt,int cmd, IOSCANPVT *ppvt)
{
    deviceData    *pdeviceData = (deviceData *)ab1771IXPvt;
    ab1771IXRecord *precord;
    recordPvt	  *precordPvt;
    int           signal;

    if(!pdeviceData) return(ab1771IXFatalError);
    if(!(precord = pdeviceData->precord)) return(ab1771IXFatalError);
    if(!(precordPvt = (recordPvt *)precord->dpvt)) return(ab1771IXFatalError);
    signal = pdeviceData->signal;
    if(!precordPvt->ioscanpvtInited[signal]) {
        scanIoInit(&precordPvt->ioscanpvt[signal]);
	precordPvt->ioscanpvtInited[pdeviceData->signal] = TRUE;
    }
    *ppvt = precordPvt->ioscanpvt[signal];
    return(0);
}

LOCAL int get_raw(void *ab1771IXPvt,long *praw)
{
    deviceData	  *pdeviceData = (deviceData *)ab1771IXPvt;
    ab1771IXRecord *precord;
    recordPvt	  *precordPvt;
    channelFields *pcf;

    if(!pdeviceData) return(ab1771IXFatalError);
    if(!(precord = pdeviceData->precord)) return(ab1771IXFatalError);
    if(!(precordPvt = (recordPvt *)precord->dpvt)) return(ab1771IXFatalError);
    if((precordPvt->status != abSuccess)
    && (precordPvt->status != abBtqueued)) return(ab1771IXAbError);
    pcf = (channelFields *)&precord->sta1;
    pcf += pdeviceData->signal;
    *praw = (long)pcf->raw;
    return(pcf->sta);
}

LOCAL int get_eng(void *ab1771IXPvt,double *peng)
{
    deviceData	  *pdeviceData = (deviceData *)ab1771IXPvt;
    ab1771IXRecord *precord;
    recordPvt	  *precordPvt;
    channelFields *pcf;
    int           signal;

    if(!pdeviceData) return(ab1771IXFatalError);
    if(!(precord = pdeviceData->precord)) return(ab1771IXFatalError);
    if(!(precordPvt = (recordPvt *)precord->dpvt)) return(ab1771IXFatalError);
    if((precordPvt->status != abSuccess)
    && (precordPvt->status != abBtqueued)) return(ab1771IXAbError);
    pcf = (channelFields *)&precord->sta1;
    signal = pdeviceData->signal;
    pcf += pdeviceData->signal;
    if( (signal<3 && precord->typa==ab1771IX_TYPE_MV)
    ||  (signal>3 && precord->typb==ab1771IX_TYPE_MV)) {
	errlogPrintf("%s get_eng Logic Error in device support\n",precord->name);
	return(ab1771IXFatalError);
    }
    if(precord->mtyp==ab1771IX_HR) {
        *peng = (double)pcf->raw/10.0;
    } else {
	*peng = pcf->raw;
    }
    return(pcf->sta);
}

/*utility routine to dump raw messages*/
int ab1771IXpm(char *recordname)
{
    DBENTRY       dbEntry;
    DBENTRY       *pdbEntry = &dbEntry;
    DBADDR        dbAddr;
    ab1771IXRecord *precord;
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
    if(strcmp(dbGetRecordTypeName(pdbEntry),"ab1771IX")!=0) {
	printf("Not a ab1771IXRecord\n");
	return(0);
    }
    dbFinishEntry(pdbEntry);
    status = dbNameToAddr(recordname,&dbAddr);
    if(status) {
	printf("Cant find %s\n",recordname);
	return(0);
    }
    precord = (ab1771IXRecord *)dbAddr.precord;
    if(!(precordPvt = (recordPvt *)precord->dpvt)) {
	printf("dpvt is NULL\n");
	return(0);
    }
    printf("output message");
    pdata = precord->outm;
    for(i=0; i< NUM_WORDS_OUT; i++) {
	if(i%10 == 0) printf("\n");
	printf(" %4hx",*pdata);
	pdata++;
    }
    printf("\ninput message");
    pdata = precord->inpm;
    for(i=0; i< NUM_WORDS_IN; i++) {
	if(i%10 == 0) printf("\n");
	printf(" %4hx",*pdata);
	pdata++;
    }
    printf("\n");
    return(0);
}
