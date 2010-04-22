/* devAsynBusy.c */
/***********************************************************************e
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory, and the Regents of the University of
* California, as Operator of Los Alamos National Laboratory, and
* Berliner Elektronenspeicherring-Gesellschaft m.b.H. (BESSY).
* asynDriver is distributed subject to a Software License Agreement
* found in file LICENSE that is included with this distribution.
***********************************************************************/
/*
    Authors:  Mark Rivers
    05-Sept-2004
*/

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include <alarm.h>
#include <recGbl.h>
#include "epicsMath.h"
#include <dbAccess.h>
#include <dbDefs.h>
#include <dbStaticLib.h>
#include <dbEvent.h>
#include <link.h>
#include <epicsPrint.h>
#include <epicsMutex.h>
#include <epicsThread.h>
#include <cantProceed.h>
#include <dbCommon.h>
#include <dbScan.h>
#include <callback.h>
#include <busyRecord.h>
#include <recSup.h>
#include <devSup.h>

#include <epicsExport.h>
#include "asynDriver.h"
#include "asynDrvUser.h"
#include "asynInt32.h"
#include "asynInt32SyncIO.h"
#include "asynEpicsUtils.h"
#include <epicsExport.h>


typedef struct devBusyPvt{
    busyRecord       *pr;
    asynUser          *pasynUser;
    asynUser          *pasynUserSync;
    asynInt32         *pint32;
    void              *int32Pvt;
    void              *registrarPvt;
    int               canBlock;
    epicsMutexId      mutexId;
    asynStatus        status;
    CALLBACK          callback;
    char              *portName;
    char              *userParam;
    int               addr;
    epicsInt32        value;
}devBusyPvt;

static void processCallback(asynUser *pasynUser);
static void interruptCallback(void *drvPvt, asynUser *pasynUser,
                              epicsInt32 value);

static long initBusy(busyRecord *pbusy);
static long processBusy(busyRecord *pr);

typedef struct analogDset { /* analog  dset */
    long          number;
    DEVSUPFUN     dev_report;
    DEVSUPFUN     init;
    DEVSUPFUN     init_record;
    DEVSUPFUN     get_ioint_info;
    DEVSUPFUN     processCommon;/*(0)=>(success ) */
    DEVSUPFUN     special_linconv;
} analogDset;

analogDset asynBusyInt32 = {
    5,0,0,initBusy,0,processBusy };

epicsExportAddress(dset, asynBusyInt32);

static long initBusy(busyRecord *pr)
{
    devBusyPvt *pPvt;
    asynStatus status;
    asynUser *pasynUser;
    asynInterface *pasynInterface;
    epicsInt32 value;

    pPvt = callocMustSucceed(1, sizeof(*pPvt), "devAsynBusy::initCommon");
    pr->dpvt = pPvt;
    pPvt->pr = pr;
    /* Create asynUser */
    pasynUser = pasynManager->createAsynUser(processCallback, 0);
    pasynUser->userPvt = pPvt;
    pPvt->pasynUser = pasynUser;
    pPvt->mutexId = epicsMutexCreate();
 
    /* Parse the link to get addr and port */
    status = pasynEpicsUtils->parseLink(pasynUser, &pr->out, 
                &pPvt->portName, &pPvt->addr, &pPvt->userParam);
    if (status != asynSuccess) {
        printf("%s devAsynBusy::initCommon  %s\n",
                     pr->name, pasynUser->errorMessage);
        goto bad;
    }
    
    /* Connect to device */
    status = pasynManager->connectDevice(pasynUser, pPvt->portName, pPvt->addr);
    if (status != asynSuccess) {
        printf("%s devAsynBusy::initCommon connectDevice failed %s\n",
                     pr->name, pasynUser->errorMessage);
        goto bad;
    }
    status = pasynManager->canBlock(pPvt->pasynUser, &pPvt->canBlock);
    if (status != asynSuccess) {
        printf("%s devAsynBusy::initCommon canBlock failed %s\n",
                     pr->name, pasynUser->errorMessage);
        goto bad;
    }
    /*call drvUserCreate*/
    pasynInterface = pasynManager->findInterface(pasynUser,asynDrvUserType,1);
    if(pasynInterface && pPvt->userParam) {
        asynDrvUser *pasynDrvUser;
        void       *drvPvt;

        pasynDrvUser = (asynDrvUser *)pasynInterface->pinterface;
        drvPvt = pasynInterface->drvPvt;
        status = pasynDrvUser->create(drvPvt,pasynUser,pPvt->userParam,0,0);
        if(status!=asynSuccess) {
            printf("%s devAsynBusy::initCommon drvUserCreate %s\n",
                     pr->name, pasynUser->errorMessage);
            goto bad;
        }
    }
    /* Get interface asynInt32 */
    pasynInterface = pasynManager->findInterface(pasynUser, asynInt32Type, 1);
    if (!pasynInterface) {
        printf("%s devAsynBusy::initCommon findInterface asynInt32Type %s\n",
                     pr->name,pasynUser->errorMessage);
        goto bad;
    }
    pPvt->pint32 = pasynInterface->pinterface;
    pPvt->int32Pvt = pasynInterface->drvPvt;
    /* Register for callbacks when value changes */
    status = pPvt->pint32->registerInterruptUser(
             pPvt->int32Pvt,pPvt->pasynUser,
             interruptCallback,pPvt,&pPvt->registrarPvt);
    if(status!=asynSuccess) {
       printf("%s devAsynBusy registerInterruptUser %s\n",
              pr->name,pPvt->pasynUser->errorMessage);
    }
    /* Initialize synchronous interface */
    status = pasynInt32SyncIO->connect(pPvt->portName, pPvt->addr, 
                 &pPvt->pasynUserSync, pPvt->userParam);
    if (status != asynSuccess) {
        printf("%s devAsynBusy::initCommon Int32SyncIO->connect failed %s\n",
               pr->name, pPvt->pasynUserSync->errorMessage);
        goto bad;
    }
    /* Read the current value from the device */
    status = pasynInt32SyncIO->read(pPvt->pasynUserSync,
                      &value, pPvt->pasynUser->timeout);
    if (status == asynSuccess) {
        pr->rval = value;
        return 0;
    }
    return 2;
bad:
   pr->pact=1;
   return 0;
}

static void processCallback(asynUser *pasynUser)
{
    devBusyPvt *pPvt = (devBusyPvt *)pasynUser->userPvt;
    busyRecord *pr = pPvt->pr;
    int status=asynSuccess;

    status = pPvt->pint32->write(pPvt->int32Pvt, pPvt->pasynUser,pPvt->value);
    if(status == asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
            "%s devAsynBusy process value %d\n",pr->name,pPvt->value);
    } else {
       asynPrint(pasynUser, ASYN_TRACE_ERROR,
           "%s devAsynBusy process error %s\n",
           pr->name, pasynUser->errorMessage);
       recGblSetSevr(pr, WRITE_ALARM, INVALID_ALARM);
    }
    if(pr->pact) callbackRequestProcessCallback(&pPvt->callback,pr->prio,pr);
}


static void interruptCallback(void *drvPvt, asynUser *pasynUser,
                epicsInt32 value)
{
    devBusyPvt *pPvt = (devBusyPvt *)drvPvt;
    busyRecord *pr = (busyRecord *)pPvt->pr;
    unsigned short	monitor_mask;

    dbScanLock((dbCommon *)pr);
    asynPrint(pPvt->pasynUser, ASYN_TRACEIO_DEVICE,
        "%s devAsynBusy::interruptCallback new value=%d\n",
        pr->name, value);
    /* If the current value of the record is 1 and the new value is 0 then post monitors
     * and call recGblFwdLink */
    if ((pr->val == 1) && (value == 0)) {
        /* If the current value of the record is 1 and the new value is 0 then post monitors
        * and call recGblFwdLink */
        asynPrint(pPvt->pasynUser, ASYN_TRACEIO_DEVICE,
            "%s devAsynBusy::interruptCallback 1 to 0 transition, posting monitors and calling recGblFwdLink\n",
            pr->name);
        pr->val = value;
        monitor_mask = recGblResetAlarms(pr);
        /* post events for value change and archive change */
        monitor_mask |= (DBE_VALUE | DBE_LOG);
        /* update last value monitored */
        pr->mlst = pr->val;
        /* send out monitors connected to the value field */
        if (monitor_mask){
            db_post_events(pr,&pr->val,monitor_mask);
        }
        recGblFwdLink(pr);
    }
    dbScanUnlock((dbCommon *)pr);
}

static long processBusy(busyRecord *pr)
{
    devBusyPvt *pPvt = (devBusyPvt *)pr->dpvt;
    int status;

    if(pr->pact == 0) {
        pPvt->value = pr->rval;
        if(pPvt->canBlock) pr->pact = 1;
        status = pasynManager->queueRequest(pPvt->pasynUser, 0, 0);
        if((status==asynSuccess) && pPvt->canBlock) return 0;
        if(pPvt->canBlock) pr->pact = 0;
        if(status != asynSuccess) {
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                "%s devAsynBusy::processCommon, error queuing request %s\n",
                pr->name,pPvt->pasynUser->errorMessage);
            recGblSetSevr(pr, WRITE_ALARM, INVALID_ALARM);
        }
    }
    return 0;
}
