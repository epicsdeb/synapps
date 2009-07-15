/* ab1771IFERecord.c*/
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
 *      Date:   Jan 19,1999
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
#include "dbCommon.h"
#define GEN_SIZE_OFFSET
#include "ab1771IFERecord.h"
#undef GEN_SIZE_OFFSET
#include <epicsExport.h>

/* IN THIS SOURCE MODULE
 * channel and signal both refer to one adc channel.
 * channel is indexed from 1 to 16
 * signal is indexed from 0 to 15
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
 
struct rset ab1771IFERSET={
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
epicsExportAddress(rset,ab1771IFERSET);

LOCAL void monitor(ab1771IFERecord *precord);

int ab1771IFEDebug=0;

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
    int		nChan;
    int		nWordsIn;
    int		nWordsOut;
    IOSCANPVT   *ioscanpvt;
    char	*ioscanpvtInited;
    void        *drvPvt;
    msgStates	msgState;
    boolean     waitAsynComplete;
    boolean     newValMsg;
    boolean     newInMsg;
    boolean     newOutMsg;
}recordPvt;

typedef enum {
    ab1771IFEOK=devIai1OK,
    ab1771IFEAbError=devIai1HardwareFault,
    ab1771IFEUnderflow=devIai1Underflow,
    ab1771IFEOverflow=devIai1Overflow,
    ab1771IFENoRecord,
    ab1771IFEIllegalRecordType,
    ab1771IFEIllegalType,
    ab1771IFEFatalError
} ab1771IFEStatus;

LOCAL char *ab1771IFEStatusMessage[] = {
	"ab1771IFE OK",
	"ab1771IFE drvAb error",
	"ab1771IFE Underflow",
	"ab1771IFE Overflow",
	"ab1771IFE Record not found",
	"ab1771IFE Not an ab1771IFERecord",
	"ab1771IFE in/out type mismatch",
	"ab1771IFE fatal error"
};

/* forward references for local routines */
LOCAL void issueAsynCallback(ab1771IFERecord *precord);
LOCAL void setValMsg(ab1771IFERecord *precord,char *msg);
LOCAL void issueError(ab1771IFERecord *precord,errStatus err,char *msg);

LOCAL void drvCallback(void *drvPvt);
LOCAL void myCallback(CALLBACK *pCallback);

LOCAL void msgInit(ab1771IFERecord *precord);
LOCAL void msgGet(ab1771IFERecord *precord);
LOCAL void msgCompleteGet(ab1771IFERecord *precord);

/*Interface routines for devIai1 and devIao1*/
LOCAL int connect(void **pdevIai1Pvt,DBLINK *plink, boolean *isEng);
LOCAL char *get_status_message(void *pdevIai1Pvt,int status);
LOCAL int get_ioint_info(void *devIai1Pvt,int cmd, IOSCANPVT *ppvt);
LOCAL int get_linconv(void *devIai1Pvt,long *range,long *roff);
LOCAL int get_raw(void *devIai1Pvt,long *praw);

LOCAL devIai1 mydevIai1= {
    connect,get_status_message,get_ioint_info,
    get_linconv,get_raw,0
};

extern DBBASE *pdbbase;

/*BEGIN STANDARD RECORD SUPPORT ROUTINES*/
LOCAL long init_record(ab1771IFERecord *precord,int pass)
{
    abStatus		status;
    recordPvt		*precordPvt;

    if(pass!=0) return(0);
    precord->iai1 = &mydevIai1;
    precord->dpvt = precordPvt = dbCalloc(1,sizeof(recordPvt));
    if(precord->sedi==ab1771IFE_SE) {
        precordPvt->nChan = 16;
        precordPvt->nWordsIn = 20;
        precordPvt->nWordsOut = 37;
    } else {
        precordPvt->nChan = 8;
        precordPvt->nWordsIn = 12;
        precordPvt->nWordsOut = 29;
    }
    precordPvt->ioscanpvt =
        (IOSCANPVT *)dbCalloc(precordPvt->nChan,sizeof(IOSCANPVT));
    precordPvt->ioscanpvtInited =
        (char *)dbCalloc(precordPvt->nChan,sizeof(char));
    callbackSetCallback(myCallback,&precordPvt->callback);
    callbackSetUser(precord,&precordPvt->callback);
    precord->inpm = (short *)dbCalloc(precordPvt->nWordsIn,sizeof(short));
    precord->outm = (short *)dbCalloc(precordPvt->nWordsOut,sizeof(short));
    status = (*pabDrv->registerCard)(precord->link,precord->rack,
	precord->slot,typeBt,"ab1771IFE",drvCallback,&precordPvt->drvPvt);
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

LOCAL long process(ab1771IFERecord *precord)
{
    recordPvt	*precordPvt = (recordPvt *)precord->dpvt;

    if(precordPvt->err == errFatal) {
	precord->pact = TRUE;
	return(0);
    }
    if(!precord->pact) {/* starting new request*/
	if(precord->cmd==ab1771IFE_Init) {
	    precordPvt->msgState = msgStateInit;
	    precord->cmd = ab1771IFE_CMD_None;
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
    ab1771IFERecord  *precord;
    recordPvt      *precordPvt;

    precord = (ab1771IFERecord *)paddr->precord;
    precordPvt = (recordPvt *)precord->dpvt;
    ind = dbGetFieldIndex(paddr);
    switch(ind) {
	case ab1771IFERecordINPM:
	    paddr->pfield = precord->inpm;
	    paddr->field_type = DBF_SHORT;
	    paddr->dbr_field_type = DBR_SHORT;
	    paddr->no_elements = precordPvt->nWordsIn;
	    paddr->field_size = sizeof(short);
	    break;
	case ab1771IFERecordOUTM:
	    paddr->pfield = precord->outm;
	    paddr->field_type = DBF_SHORT;
	    paddr->dbr_field_type = DBR_SHORT;
	    paddr->no_elements = precordPvt->nWordsOut;
	    paddr->field_size = sizeof(short);
	    break;
	case ab1771IFERecordIAI1:
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
    ab1771IFERecord *precord;
    recordPvt           *precordPvt;

    precord = (ab1771IFERecord *)paddr->precord;
    precordPvt = (recordPvt *)precord->dpvt;
    if(dbGetFieldIndex(paddr)==ab1771IFERecordINPM) {
	*no_elements = precordPvt->nWordsIn;
	*offset = 0;
    } else if(dbGetFieldIndex(paddr)==ab1771IFERecordOUTM) {
	*no_elements = precordPvt->nWordsOut;
	*offset = 0;
    } else {
	errlogPrintf("%s get_array_info for illegal field\n",precord->name);
    }
    return(0);
}

LOCAL void monitor(ab1771IFERecord *precord)
{
    unsigned short	monitor_mask;
    unsigned short	mask;
    recordPvt		*precordPvt = (recordPvt *)precord->dpvt;
    int                 signal;

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
    if(precordPvt->newInMsg
    || (precord->loca && (precordPvt->prevStatus!=precordPvt->status))) {
	for(signal=0; signal<precordPvt->nChan; signal++) {
	    if(precordPvt->ioscanpvtInited[signal])
	        scanIoRequest(precordPvt->ioscanpvt[signal]);
	}
    }
    precordPvt->prevStatus = precordPvt->status;
    precordPvt->newValMsg = FALSE;
    precordPvt->newInMsg = FALSE;
    precordPvt->newOutMsg = FALSE;
    return;
}

/*BEGIN UTILITY ROUTINES*/
LOCAL void issueAsynCallback(ab1771IFERecord *precord)
{
    recordPvt     *precordPvt = (recordPvt *)precord->dpvt;
    struct rset   *prset=(struct rset *)(precord->rset);

    if(precordPvt->waitAsynComplete) {
	(*prset->process)(precord);
    }
}

LOCAL void setValMsg(ab1771IFERecord *precord,char *msg)
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

LOCAL void issueError(ab1771IFERecord *precord,errStatus err,char *msg)
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
    ab1771IFERecord *precord;
    recordPvt	 *precordPvt;

    /*run myCallback under general purpose callback task*/
    precord = (*pabDrv->getUserPvt)(drvPvt);
    precordPvt = precord->dpvt;
    callbackSetPriority(precord->prio,&precordPvt->callback);
    callbackRequest(&precordPvt->callback);
}

LOCAL void myCallback(CALLBACK *pCallback)
{
    ab1771IFERecord *precord;
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

LOCAL void msgInit(ab1771IFERecord *precord)
{
    recordPvt	*precordPvt = (recordPvt *)precord->dpvt;
    short       *pmsg;
    short	range;
    int		signal;
    unsigned short *pr = &precord->r0;
    unsigned short tens,units;

    range = 0;
    pmsg = precord->outm;
    *pmsg = 0;
    for(signal=0; signal<16; signal++, pr++) {
        *pmsg += (*pr << (signal%8)*2);
        if(signal==7) *(++pmsg) = 0;
    }
    /*convert filt to bcd */
    tens = precord->filt / 10;
    units = precord->filt - tens*10;
    if(tens>9) tens = 9;
    if(units>9) units = 9;
    *(++pmsg) = (tens<<4) + units;
    if(precord->sedi==ab1771IFE_DI) *pmsg += 0x100;
    *pmsg += 0x0400; /*two's complement binary*/
    *pmsg += (precord->stim) << 11;
    *(++pmsg) = 0; *(++pmsg) = 0; /*all sign bits 0 */
    for(signal=0; signal< precordPvt->nChan; signal++) {
        *(++pmsg) = 0;
        *(++pmsg) = 0x4095; /*NOTE that value is in BCD*/
    }
    precordPvt->msgState = msgStateWaitInit;
    precordPvt->status = (*pabDrv->btWrite)(precordPvt->drvPvt,
	(unsigned short *)precord->outm,precordPvt->nWordsOut);
    if(precordPvt->status!=abBtqueued) {
	precordPvt->msgState = msgStateInit;
	issueError(precord,errAb,0);
	return;
    }
    precordPvt->newOutMsg = TRUE;
    return;
}

LOCAL void msgGet(ab1771IFERecord *precord)
{
    recordPvt		*precordPvt = (recordPvt *)precord->dpvt;

    precordPvt->msgState = msgStateWaitGet;
    precordPvt->status = (*pabDrv->btRead)(precordPvt->drvPvt,
	(unsigned short *)precord->inpm,precordPvt->nWordsIn);
    if(precordPvt->status!=abBtqueued) {
	precordPvt->msgState = msgStateInit;
	issueError(precord,errAb,"msgGet");
	return;
    }
    return;

}

LOCAL void msgCompleteGet(ab1771IFERecord *precord)
{
    recordPvt		*precordPvt = (recordPvt *)precord->dpvt;
    short               *pmsg;
    char		statusMsg[100];

    if(precordPvt->err!=errOK) return;
    precordPvt->newInMsg = TRUE;
    pmsg = precord->inpm;
    statusMsg[0] = 0;
    if(pmsg[0] & 0xf) {
	if(pmsg[0]&0x1) strcat(statusMsg,"PowerUp");
	if(pmsg[0]&0x2) strcat(statusMsg,"OutOfRange");
	if(pmsg[0]&0x4) strcat(statusMsg,"Invalid scaling bit");
	if(pmsg[0]&0x8) strcat(statusMsg,"Real time sample fault");
	setValMsg(precord,statusMsg);
    }
    precordPvt->msgState = msgStateDone;
    return;
}

/*CODE INTERFACE FOR DEVICE SUPPORT*/
/*Definitions for handling requests from device support*/
typedef struct deviceData{
    ab1771IFERecord *precord;
    int           signal;
}deviceData;

#define MAX_BUFFER 80
LOCAL int connect(void **pab1771IFEPvt,DBLINK *plink,boolean *isEng)
{
    DBENTRY       dbEntry;
    DBENTRY       *pdbEntry = &dbEntry;
    DBADDR        dbAddr;
    deviceData	  *pdeviceData;
    ab1771IFERecord *precord;
    recordPvt	  *precordPvt;
    long	  status;
    char          buffer[MAX_BUFFER];
    char          *recordname = &buffer[0];
    char          *pstring;
    unsigned short signal;
    char          *pLeftbracket;
    char          *pRightbracket;

    if(plink->type!=INST_IO) return(ab1771IFEFatalError);
    pstring = plink->value.instio.string;
    if(strlen(pstring)>=MAX_BUFFER) return(ab1771IFEFatalError);
    strcpy(buffer,pstring);
    pLeftbracket = strchr(buffer,'['); pRightbracket = strchr(buffer,']');
    if(!pLeftbracket || !pRightbracket) {
	errlogPrintf("link was not of the form record[signal]\n");
	return(ab1771IFEFatalError);
    }
    *pLeftbracket++ = '\0'; *pRightbracket = '\0';
    sscanf(pLeftbracket,"%hu",&signal);
    dbInitEntry(pdbbase,pdbEntry);
    status = dbFindRecord(pdbEntry,recordname);
    if(status) return(ab1771IFENoRecord);
    if(strcmp(dbGetRecordTypeName(pdbEntry),"ab1771IFE")!=0)
	return(ab1771IFEIllegalRecordType);
    dbFinishEntry(pdbEntry);
    status = dbNameToAddr(recordname,&dbAddr);
    if(status) return(ab1771IFENoRecord);
    precord = (ab1771IFERecord *)dbAddr.precord;
    if(!(precordPvt = (recordPvt *)precord->dpvt)) {
	printf("%s precordPvt is NULL ?\n",precord->name);
	return(ab1771IFEFatalError);
    }
    if(signal>=precordPvt->nChan) return(ab1771IFEIllegalType);
    *isEng = FALSE;
    pdeviceData = dbCalloc(1,sizeof(deviceData));
    pdeviceData->precord = precord;
    pdeviceData->signal = signal;
    *pab1771IFEPvt = (void *)pdeviceData;
    return(ab1771IFEOK);
}

LOCAL char *illegalStatus = "Illegal Status Value";
LOCAL char *get_status_message(void *pdevIai1Pvt,int status) {
    if(status<0 || status>ab1771IFEFatalError) return(illegalStatus);
    return(ab1771IFEStatusMessage[status]);
}

LOCAL int get_linconv(void *ab1771IFEPvt,long *range,long *roff)
{
    deviceData    *pdeviceData = (deviceData *)ab1771IFEPvt;
    ab1771IFERecord *precord;
    recordPvt	  *precordPvt;

    if(!pdeviceData) return(ab1771IFEFatalError);
    if(!(precord = pdeviceData->precord)) return(ab1771IFEFatalError);
    if(!(precordPvt = (recordPvt *)precord->dpvt)) return(ab1771IFEFatalError);
    *range = 4095;
    *roff = 0;
    return(ab1771IFEOK);
}

LOCAL int get_ioint_info(void *ab1771IFEPvt,int cmd, IOSCANPVT *ppvt)
{
    deviceData    *pdeviceData = (deviceData *)ab1771IFEPvt;
    ab1771IFERecord *precord;
    recordPvt	  *precordPvt;
    int           signal;

    if(!pdeviceData) return(ab1771IFEFatalError);
    if(!(precord = pdeviceData->precord)) return(ab1771IFEFatalError);
    if(!(precordPvt = (recordPvt *)precord->dpvt)) return(ab1771IFEFatalError);
    signal = pdeviceData->signal;
    if(!precordPvt->ioscanpvtInited[signal]) {
        scanIoInit(&precordPvt->ioscanpvt[signal]);
	precordPvt->ioscanpvtInited[pdeviceData->signal] = TRUE;
    }
    *ppvt = precordPvt->ioscanpvt[signal];
    return(0);
}

LOCAL int get_raw(void *ab1771IFEPvt,long *praw)
{
    deviceData	  *pdeviceData = (deviceData *)ab1771IFEPvt;
    ab1771IFERecord *precord;
    recordPvt	  *precordPvt;
    short         *pmsg;

    if(!pdeviceData) return(ab1771IFEFatalError);
    if(!(precord = pdeviceData->precord)) return(ab1771IFEFatalError);
    if(!(precordPvt = (recordPvt *)precord->dpvt)) return(ab1771IFEFatalError);
    if((precordPvt->status != abSuccess)
    && (precordPvt->status != abBtqueued)) return(ab1771IFEAbError);
    pmsg = precord->inpm;
    *praw = *(pmsg + 4 + pdeviceData->signal);
    if(*(pmsg+1)&(1 << pdeviceData->signal)) return(devIai1Underflow);
    if(*(pmsg+2)&(1 << pdeviceData->signal)) return(devIai1Overflow);
    return(devIai1OK);
}

/*utility routine to dump raw messages*/
int ab1771IFEpm(char *recordname)
{
    DBENTRY       dbEntry;
    DBENTRY       *pdbEntry = &dbEntry;
    DBADDR        dbAddr;
    ab1771IFERecord *precord;
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
    if(strcmp(dbGetRecordTypeName(pdbEntry),"ab1771IFE")!=0) {
	printf("Not a ab1771IFERecord\n");
	return(0);
    }
    dbFinishEntry(pdbEntry);
    status = dbNameToAddr(recordname,&dbAddr);
    if(status) {
	printf("Cant find %s\n",recordname);
	return(0);
    }
    precord = (ab1771IFERecord *)dbAddr.precord;
    if(!(precordPvt = (recordPvt *)precord->dpvt)) {
	printf("dpvt is NULL\n");
	return(0);
    }
    printf("output message");
    pdata = precord->outm;
    for(i=0; i< precordPvt->nWordsOut; i++) {
	if(i%10 == 0) printf("\n");
	printf(" %4hx",*pdata);
	pdata++;
    }
    printf("\ninput message");
    pdata = precord->inpm;
    for(i=0; i< precordPvt->nWordsIn; i++) {
	if(i%10 == 0) printf("\n");
	printf(" %4hx",*pdata);
	pdata++;
    }
    printf("\n");
    return(0);
}
