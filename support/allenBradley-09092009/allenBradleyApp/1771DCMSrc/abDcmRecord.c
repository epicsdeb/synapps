/* abDcmRecord.c */
/*
 *      Author:  Marty Kraimer
 *      Date:   June 9, 1995
 *        Major revision July 23, 1996
 *        Major revision May, 1997
 */
/*************************************************************************
* Copyright (c) 2003 The University of Chicago, as Operator of Argonne
* National Laboratory, the Regents of the University of California, as
* Operator of Los Alamos National Laboratory, and Leland Stanford Junior
* University, as the Operator of Stanford Linear Accelerator.
* This code is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
*************************************************************************/
#include <vxWorks.h>
#include <tickLib.h>
#include <taskLib.h>
#include <semLib.h>
#include <sysLib.h>
#include <epicsExit.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <logLib.h>

#include "dbDefs.h"
#include "errlog.h"
#include "taskwd.h"
#include "dbScan.h"
#include "dbEvent.h"
#include "ellLib.h"
#include "task_params.h"
#include "alarm.h"
#include "dbStaticLib.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "errMdef.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "drvAb.h"
#define GEN_SIZE_OFFSET
#include "abDcmRecord.h"
#undef GEN_SIZE_OFFSET
#include "abDcm.h"
#include <epicsExport.h>

int abDcmDebug=0;

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
LOCAL long init_record();
LOCAL long process();
#define special NULL
#define get_value NULL
LOCAL long cvt_dbaddr();
LOCAL long get_array_info();
LOCAL long put_array_info();
#define get_units NULL
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

struct rset abDcmRSET={
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
        get_alarm_double };
epicsExportAddress(rset,abDcmRSET);
LOCAL void monitor(struct abDcmRecord *precord);
extern struct dbBase *pdbbase;

/*Definitions for handling messages from dcm*/
#define NUM_DCM_WORDS     64
#define NUM_PUT_WORDS     64
#define DCM_STATUS         0
/*DCM_STATUS Errors */
#define INVALID_DATA  0x0100

/*Definitions for handling requests from device support*/
typedef struct deviceMsg{/*For output messages*/
        ELLNODE           outMsgNode;
        struct deviceData *pdeviceData;
        dcmPutCallback    callback;
        void              *userPvt;
        unsigned short    outMsg[NUM_PUT_WORDS];
        unsigned short    outMsgLen;
        short             msgQueued;
}deviceMsg;

typedef struct deviceData{
        struct deviceMsg   *pdeviceMsg;
        struct abDcmRecord *pabDcm;
        unsigned short     table;
        unsigned short     word;
}deviceData;

/*definitions for abDcmRecord*/
typedef struct {
        IOSCANPVT      ioscanpvt;
        unsigned short *pvalue; /*address of word within array field*/
        short          extraWords; /*for multi word elements*/
        short          ioscanpvtInited; /*has scanIoInit been called*/
} wordData;

typedef struct {
        wordData *pawordData;    /*p array of wordData*/
        short    postEvent;
        short    firstTime;
        void     *drvPvt; /*Only used for abDcm_1771MultiSlot*/
        short    nExtraMessages;/*Only used for !abDcm_1771MultiSlot*/
        short    gotNewMessage; /*Only used for !abDcm_1771MultiSlot*/
}tableData;

typedef struct recordData{
        abStatus       status;
        abStatus       prevStatus;
        tableData      **ptableData;
        int            nTables;
        unsigned short buffer[NUM_DCM_WORDS];
        int            nNewTables;
        int            taskId;
        SEM_ID         startSem;
        SEM_ID         waitSem;
        short          postEvent;
        short          writeFailure;
        /* this drvPvt always used for output*/
        /*For input it only used for !abDcm_1771MultiSlot*/
        void           *drvPvt;
        /*remaining fields are for outputs*/
        ELLLIST        outMsgList;
        SEM_ID         outMsgSem;
}recordData;

LOCAL void myCallback(void *drvPvt)
{
    struct abDcmRecord *precord;
    recordData         *precordData;

    precord = (*pabDrv->getUserPvt)(drvPvt);
    precordData = precord->dpvt;
    precordData->status = (*pabDrv->getStatus)(drvPvt);
    semGive(precordData->waitSem);
}

LOCAL void tableDataUpdate(recordData *precordData, unsigned short table)
{
    tableData      *ptableData = precordData->ptableData[table];
    unsigned short word;
    word=0;
    while(word<NUM_DCM_WORDS) {
        unsigned short value;
        wordData       *pwordData = &ptableData->pawordData[word];
        unsigned short *pvalue;
        int            calledscanIoRequest,indExtra;

        calledscanIoRequest = FALSE;
        for(indExtra=0; indExtra<=pwordData->extraWords; indExtra++) {
            value = precordData->buffer[word+indExtra];
            pvalue = (&ptableData->pawordData[word+indExtra])->pvalue;
 	    if(ptableData->firstTime || value!= *pvalue) {
              ptableData->postEvent = TRUE;
              *pvalue = value;
              if(!calledscanIoRequest)
                  if(pwordData->ioscanpvtInited)
                        scanIoRequest(pwordData->ioscanpvt);
                  calledscanIoRequest=TRUE;
              }
        }
        /*If data is bad only process status word */
        if((precordData->buffer[DCM_STATUS]&INVALID_DATA) !=0) break;
        word += pwordData->extraWords + 1;
    }
}

LOCAL void dcmThread(struct abDcmRecord *precord)
{
    recordData     *precordData;
    struct rset    *prset;
    unsigned short table;
    tableData      *ptableData;
    abStatus       status;
    int            nticks;
    int            nwrite;
    deviceMsg      *pdeviceMsg;

    precordData = precord->dpvt;
    taskwdInsert((epicsThreadId)precordData->taskId,NULL,0L);
    while(TRUE) {
        semTake(precordData->startSem,WAIT_FOREVER);
        /*Handle outputs first*/
        nwrite = 0; precordData->writeFailure=FALSE;
        while(TRUE) {
            if((precord->mcw) && (++nwrite > precord->mcw)) break;
            semTake(precordData->outMsgSem,WAIT_FOREVER);
            pdeviceMsg = (deviceMsg *)ellFirst(&precordData->outMsgList);
            if(pdeviceMsg) {
                ellDelete(&precordData->outMsgList,&pdeviceMsg->outMsgNode);
                pdeviceMsg->msgQueued = FALSE;
            }
            semGive(precordData->outMsgSem);
            if(!pdeviceMsg) break;
            status = (*pabDrv->btWrite)(precordData->drvPvt,
                &pdeviceMsg->outMsg[0],pdeviceMsg->outMsgLen);
            if(status!=abBtqueued) {
                precordData->writeFailure = TRUE;
                (*pdeviceMsg->callback)(pdeviceMsg->userPvt,abDcmError);
                continue;
            }
            semTake(precordData->waitSem,WAIT_FOREVER);
            if(precordData->status!=abSuccess) precordData->writeFailure = TRUE;
            (*pdeviceMsg->callback)(pdeviceMsg->userPvt,precordData->status);
        }
        nticks = sysClkRateGet()*precord->dly;
        if(nticks<=0 && precord->dly>0.0) nticks=1;
        precordData->nNewTables = 0;
        precordData->postEvent = FALSE;
        for(table=0; table<precordData->nTables; table++) {
            ptableData = precordData->ptableData[table];
            ptableData->nExtraMessages = 0;
            ptableData->gotNewMessage = FALSE;
            ptableData->postEvent = FALSE;
        }
        if(precord->dcmt==abDcm_1771MultiSlot) {
            for(table=0; table<precordData->nTables; table++) {
                ptableData = precordData->ptableData[table];
                if(nticks>0) taskDelay(nticks);
                status = (*pabDrv->btRead)(ptableData->drvPvt,
                    (unsigned short *)precordData->buffer,NUM_DCM_WORDS);
                if(status!=abBtqueued) {
                    precordData->status = status;
                    break;
                }
                semTake(precordData->waitSem,WAIT_FOREVER);
                if(precordData->status!=abSuccess) break;
                tableDataUpdate(precordData,table);
                ptableData->firstTime = FALSE;
            }
            precordData->nNewTables = precordData->nTables;
        } else while(TRUE) { /* Until all tables are read*/
            if(nticks>0) taskDelay(nticks);
            status = (*pabDrv->btRead)(precordData->drvPvt,
            (unsigned short *)precordData->buffer,NUM_DCM_WORDS);
            if(status!=abBtqueued) {
                precordData->status = status;
                break;
            }
            semTake(precordData->waitSem,WAIT_FOREVER);
            if(precordData->status!=abSuccess) break;
            table = precordData->buffer[precord->toff];
            if(table>=precordData->nTables) {
                logMsg("abDcmRecord: Illegal table. record=%s table=%d\n",
                    (int)(precord->name),(int)table,0,0,0,0);
                precordData->status = abFailure;
                break;
            }
            ptableData = precordData->ptableData[table];
            if(!(ptableData->gotNewMessage)
            && ((precordData->buffer[DCM_STATUS]&INVALID_DATA)==0)) {
                ptableData->gotNewMessage = TRUE;
                precordData->nNewTables++;
            } else {
                ptableData->nExtraMessages ++;
                if(ptableData->nExtraMessages>5) {
                    /*If repeating same table too often then error*/
                    precordData->status = abFailure;
                    break;
                }
            }
            tableDataUpdate(precordData,table);
            if(ptableData->gotNewMessage) {
                ptableData->firstTime = FALSE;
                if(precordData->nNewTables>=precordData->nTables) break;
            }
        }
        /*Will reach this when all tables are read or error */
        prset=(struct rset *)(precord->rset);
        dbScanLock((void *)precord);
        (*prset->process)(precord);
        dbScanUnlock((void *)precord);
    }
    taskSuspend(0);
}

LOCAL long init_record(struct abDcmRecord *precord,int pass)
{
    DBENTRY        dbEntry;
    abStatus       status;
    void           *drvPvt;
    recordData     *precordData;
    unsigned short table,word;
    unsigned short **papt = (unsigned short**)&precord->t0;
                   /*papt is pointer to array of pointers*/

    if(pass!=0) return(0);
    if(precord->nt<=0) {
        errlogPrintf("FATAL ERROR: record %s has NT<=0\n",precord->name);
        errlogFlush();
        epicsExit(1);
    }
    if(precord->toff<=0) {
        errlogPrintf("FATAL ERROR: record %s has TOFF<=0\n",precord->name);
        errlogFlush();
        epicsExit(1);
    }
    /*Check that nt fields are present*/
    dbInitEntry(pdbbase,&dbEntry);
    status = dbFindRecord(&dbEntry,precord->name);
    if (precord->nt > 999) {
        errlogPrintf("FATAL ERROR: record %s NT is illegal\n",precord->name);
        errlogFlush();
        epicsExit(1);
    }

    for (table = 0; table < precord->nt; table++) {
        char fname[5];
        sprintf(fname, "T%d", table);
        status = dbFindField(&dbEntry, fname);
        if (status) {
            errlogPrintf("FATAL ERROR: record %s field T%d doesn't exist (NT = %d)\n",
                precord->name, table, precord->nt);
            errlogFlush();
            epicsExit(1);
        }
    }
    precord->dpvt = precordData = dbCalloc(1,sizeof(recordData));
    precordData->ptableData = dbCalloc(precord->nt,sizeof(tableData *));
    precordData->nTables = precord->nt;
    for(table=0; table<precordData->nTables; table++) {
        unsigned short **ppt = &papt[table];
        tableData      *ptableData;
        wordData       *pwordData;

        *ppt = dbCalloc(NUM_DCM_WORDS,sizeof(unsigned short));
        ptableData = dbCalloc(1,sizeof(tableData));
        precordData->ptableData[table] = ptableData;
        ptableData->firstTime = TRUE;
        ptableData->pawordData = dbCalloc(NUM_DCM_WORDS,sizeof(wordData));
        for(word=0; word<NUM_DCM_WORDS; word++) { 
            pwordData = &ptableData->pawordData[word];
            pwordData->pvalue = &(*ppt)[word];
        }
    }
    if((precordData->startSem=semBCreate(SEM_Q_FIFO,SEM_EMPTY))==NULL) {
        errMessage(0,"abDcmRecord: semBCreate failed\n");
        taskSuspend(0);
    }
    if((precordData->waitSem=semBCreate(SEM_Q_FIFO,SEM_EMPTY))==NULL) {
        errMessage(0,"abDcmRecord: semBCreate failed\n");
        taskSuspend(0);
    }
    if((precordData->outMsgSem=semBCreate(SEM_Q_FIFO,SEM_FULL))==NULL) {
        errMessage(0,"abDcmRecord: semBCreate failed\n");
        taskSuspend(0);
    }
    ellInit(&precordData->outMsgList);
    if(precord->dcmt==abDcm_1771MultiSlot) {
        for(table=0; table<precordData->nTables; table++) {
            status = (*pabDrv->registerCard)(precord->link,precord->rack,
                precord->slot+table,typeBt,"abDcm",myCallback,&drvPvt);
            if(status!=abNewCard) {
                if(status==abSuccess)
                    errlogPrintf("record %s slot already used\n",precord->name);
                else
                    errlogPrintf("record %s init error %s\n",precord->name,
                        abStatusMessage[status]);
                precord->pact = TRUE; /*leave record active*/
                return(0);
            }
            (*pabDrv->setUserPvt)(drvPvt,(void *)precord);
            precordData->ptableData[table]->drvPvt = drvPvt;
        }
        precordData->drvPvt = precordData->ptableData[0]->drvPvt;
    } else {
        status = (*pabDrv->registerCard)(precord->link,precord->rack,
            precord->slot,typeBt,"abDcm",myCallback,&drvPvt);
        if(status!=abNewCard) {
            if(status==abSuccess)
                errlogPrintf("record %s slot already used\n",precord->name);
            else
                errlogPrintf("record %s init error %s\n",precord->name,
                    abStatusMessage[status]);
            precord->pact = TRUE; /*leave record active*/
            return(0);
        }
        (*pabDrv->setUserPvt)(drvPvt,(void *)precord);
        precordData->drvPvt = drvPvt;
    }
    precordData->taskId = taskSpawn(precord->name,CALLBACK_PRI_LOW,VX_FP_TASK,
        CALLBACK_STACK,(FUNCPTR)dcmThread,
        (int)precord,0,0,0,0,0,0,0,0,0);
    precord->udf = FALSE;
    recGblResetAlarms(precord);
    return(0);
}

LOCAL long process(struct abDcmRecord *precord)
{
    recordData *precordData = precord->dpvt;
    int        table;
    int        extraMessages=0;

    if(!precord->pact) { /* starting new request*/
        semGive(precordData->startSem);
        precord->pact = TRUE;
        return(0);
    }
    /*arrive here when called by dcmThread*/
    for(table=0; table < precordData->nTables; table++) {
        tableData *ptableData = precordData->ptableData[table];

        extraMessages += ptableData->nExtraMessages;
    }
    strcpy(&precord->val[0],abStatusMessage[precordData->status]);
    if(precordData->status!=abSuccess) {
        recGblSetSevr(precord,COMM_ALARM,MAJOR_ALARM);
    } 
    if((precordData->status==abSuccess)
    && precordData->nNewTables!=precordData->nTables){
        sprintf(&precord->val[0],"%d missing messages",
                (precordData->nTables - precordData->nNewTables));
        precordData->postEvent = TRUE;
        recGblSetSevr(precord,COMM_ALARM,MAJOR_ALARM);
    }else if(extraMessages) {
        sprintf(&precord->val[0],"%d extra messages",extraMessages);
        precordData->postEvent = TRUE;
        recGblSetSevr(precord,COMM_ALARM,MINOR_ALARM);
    }else if (precordData->writeFailure) {
        sprintf(&precord->val[0],"Write Error");
        precordData->postEvent = TRUE;
        recGblSetSevr(precord,COMM_ALARM,MINOR_ALARM);
    }
    monitor(precord);
    recGblFwdLink(precord);
    precord->pact = FALSE;
    return(0);
}

LOCAL long cvt_dbaddr(struct dbAddr *paddr)
{
    unsigned short **pt = paddr->pfield;

    if(*pt==NULL) return(0);
    paddr->pfield = *pt;
    paddr->no_elements = NUM_DCM_WORDS;
    paddr->field_type = DBF_USHORT;
    paddr->dbr_field_type = DBR_SHORT;
    paddr->field_size = sizeof(unsigned short);
    return(0);
}

LOCAL long get_array_info(struct dbAddr *paddr,long *no_elements,long *offset)
{/*Note that this routine can not get called unless cvt_dbaddr changed dbAddr*/

    *no_elements =  NUM_DCM_WORDS;
    *offset = 0;
    return(0);
}

LOCAL long put_array_info(struct dbAddr *paddr,long nNew)
{

    return(0);
}

LOCAL void monitor(struct abDcmRecord *precord)
{
    unsigned short monitor_mask;
    recordData     *precordData = precord->dpvt;
    tableData      *ptableData;
    int            table;
    unsigned short **ppt = (unsigned short **)&precord->t0;
    int            word;

    monitor_mask = recGblResetAlarms(precord);
    recGblGetTimeStamp(precord);
    if(strcmp(precord->val,precord->oval)!=0) {
        strcpy(precord->oval,precord->val);
        db_post_events(precord,&precord->val[0],
                monitor_mask|DBE_LOG|DBE_VALUE);
    }
    for(table=0; table<precordData->nTables; table++) {
        ptableData = precordData->ptableData[table];
        if(ptableData->postEvent || monitor_mask)
            db_post_events(precord,ppt[table],
                monitor_mask|DBE_LOG|DBE_VALUE);
        /*If status change and loca then call scanIoRequest for all words*/
        if(precord->loca && (precordData->prevStatus!=precordData->status)) {
            word=0;
            while(word<NUM_DCM_WORDS) {
                wordData        *pwordData = &ptableData->pawordData[word];

                if(!pwordData->ioscanpvtInited) {
                    scanIoInit(&pwordData->ioscanpvt);
                    pwordData->ioscanpvtInited = TRUE;
                }
                scanIoRequest(pwordData->ioscanpvt);
                word += pwordData->extraWords + 1;
            }
        }
    }
    precordData->prevStatus = precordData->status;
    return;
}

LOCAL char *abDcmStatusMessage[] = {
   "abdcm OK",
   "abdcm Connection Conflict",
   "abdcm Error",
   "abdcm no record found",
   "abdcm Illegal Table",
   "abdcm Illegal word",
   "abdcm Output Messages Active",
   "abdcm drvAb error"
};

LOCAL abDcmStatus connect(void **pabDcmPvt,char *recordname,
        unsigned short table,unsigned short word,unsigned short nwords)
{
    deviceData  *pdeviceData;
    DBADDR      dbAddr;
    abDcmRecord *pabDcm;
    recordData  *precordData;
    long        status;

    status = dbNameToAddr(recordname,&dbAddr);
    if(status) return(abDcmNoRecord);
    pabDcm = (struct abDcmRecord *)dbAddr.precord;
    precordData = (recordData *)pabDcm->dpvt;
    if( (table>=precordData->nTables) || !(precordData->ptableData[table]) ) 
        return(abDcmIllegalTable);
    if((word>=NUM_DCM_WORDS) || (nwords>2) || (nwords==2 && (word%2 != 0)))
        return(abDcmIllegalWord);
    pdeviceData = dbCalloc(1,sizeof(deviceData));
    *pabDcmPvt = (void *)pdeviceData;
    pdeviceData->pabDcm = pabDcm;
    pdeviceData->table = table;
    pdeviceData->word = word;
    return(0);
}

LOCAL abDcmStatus get_ioint_info(void *pabDcmPvt,int cmd, IOSCANPVT *ppvt)
{
    deviceData  *pdeviceData = (deviceData *)pabDcmPvt;
    abDcmRecord *pabDcm;
    recordData  *precordData;
    tableData   *ptableData;
    wordData    *pwordData;

    if(!pdeviceData) goto error_return;
    if(!(pabDcm = pdeviceData->pabDcm)) goto error_return;
    if(!(precordData = (recordData *)pabDcm->dpvt)) goto error_return;
    if(!(ptableData = precordData->ptableData[pdeviceData->table]))
        goto error_return;
    pwordData = &ptableData->pawordData[pdeviceData->word];
    if(!pwordData->ioscanpvtInited) {
        scanIoInit(&pwordData->ioscanpvt);
        pwordData->ioscanpvtInited = TRUE;
    }
    *ppvt = pwordData->ioscanpvt;
    return(0);
error_return:
    errlogPrintf("Logic error in abDcmRecord get_ioint_info\n");
    return(abDcmError);
}

LOCAL abDcmStatus get_float(void *pabDcmPvt,float *pval)
{
    deviceData  *pdeviceData = (deviceData *)pabDcmPvt;
    abDcmRecord *pabDcm;
    recordData  *precordData;
    tableData   *ptableData;
    volatile float          value;
    volatile unsigned short *usvalue = (unsigned short *)&value;

    if(!pdeviceData) goto error_return;
    if(!(pabDcm = pdeviceData->pabDcm)) goto error_return;
    if(!(precordData = (recordData *)pabDcm->dpvt)) goto error_return;
    if(precordData->status!=abSuccess) return(abDcmAbError);
    if(!(ptableData = precordData->ptableData[pdeviceData->table]))
        goto error_return;
    usvalue[0] = *(&ptableData->pawordData[pdeviceData->word])->pvalue;
    usvalue[1] = *(&ptableData->pawordData[pdeviceData->word+1])->pvalue;
    *pval = value;
    return(0);
error_return:
    errlogPrintf("Logic error in abDcmRecord get_float\n");
    return(abDcmError);
}

LOCAL abDcmStatus get_ushort(void *pabDcmPvt,unsigned short *pval)
{
    deviceData     *pdeviceData = (deviceData *)pabDcmPvt;
    abDcmRecord    *pabDcm;
    recordData     *precordData;
    tableData      *ptableData;
    wordData       *pwordData;
    unsigned short value;

    if(!pdeviceData) goto error_return;
    if(!(pabDcm = pdeviceData->pabDcm)) goto error_return;
    if(!(precordData = (recordData *)pabDcm->dpvt)) goto error_return;
    if(precordData->status!=abSuccess) return(abDcmAbError);
    if(!(ptableData = precordData->ptableData[pdeviceData->table]))
        goto error_return;
    pwordData = &ptableData->pawordData[pdeviceData->word];
    value = *pwordData->pvalue;
    *pval = value;
    return(0);
error_return:
    errlogPrintf("Logic error in abDcmRecord get_ushort\n");
    return(abDcmError);
}

LOCAL abDcmStatus get_short(void *pabDcmPvt,short *pval)
{
    /* The following works because no actual conversion occurs*/
    return(get_ushort(pabDcmPvt,(unsigned short*)pval));
}

LOCAL abDcmStatus put_float(void *pabDcmPvt,unsigned short tag,
        float val, dcmPutCallback callback,void *userPvt)
{
    deviceData     *pdeviceData = (deviceData *)pabDcmPvt;
    deviceMsg      *pdeviceMsg;
    abDcmRecord    *pabDcm;
    recordData     *precordData;
    unsigned short *pmsg;
    volatile float          value;
    volatile unsigned short *usvalue = (unsigned short *)&value;

    if(!pdeviceData) goto error_return;
    if(!(pabDcm = pdeviceData->pabDcm)) goto error_return;
    if(!(precordData = (recordData *)pabDcm->dpvt)) goto error_return;
    if(!pdeviceData->pdeviceMsg) {
        pdeviceMsg = dbCalloc(1,sizeof(deviceMsg));
        pdeviceData->pdeviceMsg = pdeviceMsg;
        pdeviceMsg->pdeviceData = pdeviceData;
        pdeviceMsg->outMsgLen = NUM_PUT_WORDS;
    } else {
        pdeviceMsg = pdeviceData->pdeviceMsg;
    }
    semTake(precordData->outMsgSem,WAIT_FOREVER);
    if(pdeviceMsg->msgQueued) {
        semGive(precordData->outMsgSem);
        return(abDcmOutMsgActive);
    }
    pmsg = &pdeviceMsg->outMsg[0];
    pdeviceMsg->callback = callback;
    pdeviceMsg->userPvt = userPvt;
    value = val;
    *pmsg++ = 0;
    *pmsg++ = tag;
    *pmsg++ = usvalue[0];
    *pmsg++ = usvalue[1];
    ellAdd(&precordData->outMsgList,&pdeviceMsg->outMsgNode);
    pdeviceMsg->msgQueued = TRUE;
    semGive(precordData->outMsgSem);
    return(0);
error_return:
    errlogPrintf("Logic error in abDcmRecord put_float\n");
    return(abDcmError);
}

LOCAL abDcmStatus put_ushort(void *pabDcmPvt,unsigned short tag,
        unsigned short mask, unsigned short value,
        dcmPutCallback callback,void *userPvt)
{
    deviceData     *pdeviceData = (deviceData *)pabDcmPvt;
    deviceMsg      *pdeviceMsg;
    abDcmRecord    *pabDcm;
    recordData     *precordData;
    unsigned short *pmsg;

    if(!pdeviceData) goto error_return;
    if(!(pabDcm = pdeviceData->pabDcm)) goto error_return;
    if(!(precordData = (recordData *)pabDcm->dpvt)) goto error_return;
    if(!pdeviceData->pdeviceMsg) {
        pdeviceMsg = dbCalloc(1,sizeof(deviceMsg));
        pdeviceData->pdeviceMsg = pdeviceMsg;
        pdeviceMsg->pdeviceData = pdeviceData;
        pdeviceMsg->outMsgLen = NUM_PUT_WORDS;
    } else {
        pdeviceMsg = pdeviceData->pdeviceMsg;
    }
    semTake(precordData->outMsgSem,WAIT_FOREVER);
    if(pdeviceMsg->msgQueued) {
        semGive(precordData->outMsgSem);
        return(abDcmOutMsgActive);
    }
    pmsg = &pdeviceMsg->outMsg[0];
    pdeviceMsg->callback = callback;
    pdeviceMsg->userPvt = userPvt;
    *pmsg++ = 0;
    *pmsg++ = tag;
    *pmsg++ = mask;
    *pmsg++ = value;
    ellAdd(&precordData->outMsgList,&pdeviceMsg->outMsgNode);
    pdeviceMsg->msgQueued = TRUE;
    semGive(precordData->outMsgSem);
    return(0);
error_return:
    errlogPrintf("Logic error in abDcmRecord put_ushort\n");
    return(abDcmError);
}

LOCAL abDcmStatus put_short(void *pabDcmPvt,unsigned short tag,
        unsigned short value,
        dcmPutCallback callback,void *userPvt)
{
    return(put_ushort(
        pabDcmPvt,tag,0xffff,(unsigned short)value,callback,userPvt));
}
/* routines for communication with device support*/
abDcm abDcmTable = {
    abDcmStatusMessage,
    connect,get_ioint_info,get_float,get_ushort,get_short,
    put_float,put_ushort,put_short
};

/*utility routine to dump DCM message*/
int dcmpt(char *name,int first,int last)
{
    DBADDR         dbaddr;
    long           status;
    unsigned short *pdata;
    int            i,j;

    if(first < 0 ) first = 0;
    if(last<first || last==0) last = 63;
    status=dbNameToAddr(name,&dbaddr);
    if(status) {
        printf("dbNameToAddr failed\n");
        return(1);
    }
    if(dbaddr.field_type!=DBF_USHORT || dbaddr.no_elements!=64) {
        printf("Not a DCM table field\n");
        return(1);
    }
    pdata = (unsigned short *)dbaddr.pfield;
    for(i=first; i<= last; i++) {
        unsigned short data;
        char           bit[16];
        
        data = pdata[i];
        for (j=0; j<16; j++) {
            bit[15 - j] = ((data%2)==0) ? '0' : '1' ;
            data >>= 1;
        }
        printf("%2.2d %6.5hu 0x%4.4X",i,pdata[i],pdata[i]);
        for (j=0; j<16; j++) {
            if((j%4)==0) printf(" ");
            printf("%c",bit[j]);
        }
        printf("\n");
    }
    return(0);
}
