/*  drvDxp.c

    Author: Mark Rivers
    Date: 16-Jun-2001  Modified from mcaAIMServer.cc

    This module is an asyn driver that is called from devMcaAsyn.c and
    devDxpAsyn.cc and commmunicates with an XIA DXP module.  The code is written
    as an asynchronous driver because:
     - Some calls to the XIA Xerxes software are relatively slow and so require
       that device support be asynchronous
     - If more than one task tries to talk to a single module through the
       Xerxes software at the same time bad things happen.  By using one asyn
       port per module we guarantee that access to the module is
       serialized, there will only be one task communicating with each module.
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <errlog.h>
#include <cantProceed.h>
#include <epicsTime.h>
#include <epicsEvent.h>
#include <epicsThread.h>
#include <epicsString.h>

#ifdef linux
#include <sys/io.h>
#endif

#include "mca.h"
#include "drvMca.h"
#include "asynDriver.h"
#include "asynInt32.h"
#include "asynFloat64.h"
#include "asynInt32Array.h"
#include "asynDrvUser.h"
#include "handel.h"
#include "handel_constants.h"
#include "handel_generic.h"
#include "xia_xerxes.h"
#include "drvDxp.h"
#include "epicsHandelUtils.h"


typedef struct {
    int detChan;
    int exists;
    double preal;
    double ereal;
    double plive;
    double elive;
    double ptotal;
    int ptschan;
    int ptechan;
    double etotal;
    int acquiring;
    int prev_acquiring;
    int erased;
    int moduleType;
    asynUser *pasynUser;
} dxpChannel_t;

typedef struct {
    int detChan;
    dxpChannel_t *dxpChannel;
    int ndetectors;
    int ngroups;
    int nchans;     /* Number of MCA bins */
    unsigned int numModules;
    int *first_channels;
    char *portName;
    epicsEventId pollEventId;
    double pollTime;
    asynInterface common;
    asynInterface int32;
    void *int32InterruptPvt;
    asynInterface float64;
    asynInterface int32Array;
    asynInterface drvUser;
} drvDxpPvt;

#define XMAP_APPLY(detChan) { \
    int i, ignore=0; \
    if (detChan < 0) { \
        for (i=0; i<pPvt->numModules; i++) { \
            xiaBoardOperation(pPvt->first_channels[i], "apply", &ignore); \
        } \
    } else { \
        xiaBoardOperation(detChan, "apply", &ignore); \
    } \
}



/* These functions are private, not in any interface */
static dxpChannel_t *findChannel(      drvDxpPvt *pPvt, asynUser *pasynUser, 
                                       int signal);
static void setPreset(                 drvDxpPvt *pPvt, asynUser *pasynUser,
                                       dxpChannel_t *dxpChan, int mode, 
                                       double time);
static void getAcquisitionStatus(      drvDxpPvt *pPvt, asynUser *pasynUser,
                                       dxpChannel_t *dxpChan);
static asynStatus drvDxpWrite(         void *drvPvt, asynUser *pasynUser,
                                       epicsInt32 ivalue, epicsFloat64 dvalue);
static asynStatus drvDxpRead(          void *drvPvt, asynUser *pasynUser,
                                       epicsInt32 *pivalue, epicsFloat64 *pfvalue);
static void pollTask(                  drvDxpPvt *pPvt);

/* These functions are public, exported in interfaces */
static asynStatus int32Write(          void *drvPvt, asynUser *pasynUser,
                                       epicsInt32 value);
static asynStatus int32Read(           void *drvPvt, asynUser *pasynUser,
                                       epicsInt32 *value);
static asynStatus getBounds(           void *drvPvt, asynUser *pasynUser,
                                       epicsInt32 *low, epicsInt32 *high);
static asynStatus float64Write(        void *drvPvt, asynUser *pasynUser,
                                       epicsFloat64 value);
static asynStatus float64Read(         void *drvPvt, asynUser *pasynUser,
                                       epicsFloat64 *value);
static asynStatus int32ArrayRead(      void *drvPvt, asynUser *pasynUser,
                                       epicsInt32 *data, size_t maxChans,
                                       size_t *nactual);
static asynStatus int32ArrayWrite(     void *drvPvt, asynUser *pasynUser,
                                       epicsInt32 *data, size_t maxChans);
static asynStatus drvUserCreate(       void *drvPvt, asynUser *pasynUser,
                                       const char *drvInfo,
                                       const char **pptypeName, size_t *psize);
static asynStatus drvUserGetType(      void *drvPvt, asynUser *pasynUser,
                                       const char **pptypeName, size_t *psize);
static asynStatus drvUserDestroy(      void *drvPvt, asynUser *pasynUser);
static void report(                    void *drvPvt, FILE *fp, int details);
static asynStatus connect(             void *drvPvt, asynUser *pasynUser);
static asynStatus disconnect(          void *drvPvt, asynUser *pasynUser);


/* asynCommon methods */
static const struct asynCommon drvDxpCommon = {
    report,
    connect,
    disconnect
};

/* asynInt32 methods */
static asynInt32 drvDxpInt32 = {
    int32Write,
    int32Read,
    getBounds
};

/* asynFloat64 methods */
static asynFloat64 drvDxpFloat64 = {
    float64Write,
    float64Read
};

/* asynInt32Array methods */
static asynInt32Array drvDxpInt32Array = {
    int32ArrayWrite,
    int32ArrayRead
};

static asynDrvUser drvDxpDrvUser = {
    drvUserCreate,
    drvUserGetType,
    drvUserDestroy
};


int DXPConfig(const char *portName, int ndetectors, int ngroups, int pollFreq) 
{
    int i;
    dxpChannel_t *dxpChannel;
    int detChan;
    asynStatus status;
    drvDxpPvt *pPvt;
    char module_name[MAXALIAS_LEN];

    pPvt = callocMustSucceed(1, sizeof(*pPvt),  "DXPConfig");

    /* Copy parameters to object private */
    pPvt->portName = epicsStrDup(portName);

    pPvt->ndetectors = ndetectors;
    pPvt->ngroups = ngroups;
    pPvt->pollEventId = epicsEventCreate(epicsEventFull);
    pPvt->dxpChannel = (dxpChannel_t *)
                calloc((ndetectors+ngroups), sizeof(dxpChannel_t));

    /* Interlock access to Handel library */
    epicsHandelLock();

    /* Allocate memory arrays for each channel if it is valid */
    for (i=0; i<ndetectors; i++) {
       dxpChannel = &pPvt->dxpChannel[i];
       detChan = i;
       dxpChannel->detChan = i;
       dxpChannel->exists = 1;
       dxpChannel->acquiring = 0;
       dxpChannel->prev_acquiring = -1;
       dxpChannel->erased = 1;
       dxpChannel->moduleType = dxpGetModuleType();
       if ((dxpChannel->moduleType < 0) || (dxpChannel->moduleType > MAX_DXP_MODULE_TYPE)) {
          printf("DXPConfig: unknown module type = %d\n", dxpChannel->moduleType);
          epicsHandelUnlock();
          return(-1);
       }
    }
    for (i=0; i<ngroups; i++) {
       dxpChannel = &pPvt->dxpChannel[ndetectors+i];
       detChan = -(i+1);
       dxpChannel->detChan = detChan;
       dxpChannel->exists = 1;
       dxpChannel->acquiring = 0;
       dxpChannel->prev_acquiring = -1;
       dxpChannel->erased = 1;
       /* Figure out what kind of module this is */
       dxpChannel->moduleType = dxpGetModuleType();
       if ((dxpChannel->moduleType < 0) || (dxpChannel->moduleType > MAX_DXP_MODULE_TYPE)) {
          printf("DXPConfig: unknown module type = %d\n", dxpChannel->moduleType);
          epicsHandelUnlock();
          return(-1);
       }
    } 
    xiaGetNumModules(&pPvt->numModules);
    pPvt->first_channels = calloc(pPvt->numModules, sizeof(int));
    for (i=0; i<pPvt->numModules; i++) {
        xiaGetModules_VB(i, module_name);
        xiaGetModuleItem(module_name, "channel0_alias", &pPvt->first_channels[i]);
    }
    epicsHandelUnlock();
    pPvt->common.interfaceType = asynCommonType;
    pPvt->common.pinterface  = (void *)&drvDxpCommon;
    pPvt->common.drvPvt = pPvt;
    pPvt->int32.interfaceType = asynInt32Type;
    pPvt->int32.pinterface  = (void *)&drvDxpInt32;
    pPvt->int32.drvPvt = pPvt;
    pPvt->float64.interfaceType = asynFloat64Type;
    pPvt->float64.pinterface  = (void *)&drvDxpFloat64;
    pPvt->float64.drvPvt = pPvt;
    pPvt->int32Array.interfaceType = asynInt32ArrayType;
    pPvt->int32Array.pinterface  = (void *)&drvDxpInt32Array;
    pPvt->int32Array.drvPvt = pPvt;
    pPvt->drvUser.interfaceType = asynDrvUserType;
    pPvt->drvUser.pinterface  = (void *)&drvDxpDrvUser;
    pPvt->drvUser.drvPvt = pPvt;
    status = pasynManager->registerPort(portName,
                                        ASYN_MULTIDEVICE | ASYN_CANBLOCK,
                                        1,  /* autoconnect */
                                        0,  /* medium priority */
                                        0); /* default stack size */
    if (status != asynSuccess) {
        errlogPrintf("DXPConfig: Can't register myself.\n");
        return(-1);
    }
    status = pasynManager->registerInterface(portName,&pPvt->common);
    if (status != asynSuccess) {
        errlogPrintf("DXPConfig: Can't register common.\n");
        return(-1);
    }
    status = pasynInt32Base->initialize(portName,&pPvt->int32);
    if (status != asynSuccess) {
        errlogPrintf("DXPConfig: Can't register int32.\n");
        return(-1);
    }
    pasynManager->registerInterruptSource(portName, &pPvt->int32,
                                          &pPvt->int32InterruptPvt);
    status = pasynFloat64Base->initialize(portName,&pPvt->float64);
    if (status != asynSuccess) {
        errlogPrintf("DXPConfig: Can't register float64.\n");
        return(-1);
    }
    status = pasynInt32ArrayBase->initialize(portName,&pPvt->int32Array);
    if (status != asynSuccess) {
        errlogPrintf("DXPConfig: Can't register int32Array.\n");
        return(-1);
    }
    status = pasynManager->registerInterface(portName,&pPvt->drvUser);
    if (status != asynSuccess) {
        errlogPrintf("DXPConfig: Can't register drvUser.\n");
        return(-1);
    }
    for (i=0; i<pPvt->ndetectors+pPvt->ngroups; i++) {
        /* Create asynUser and connect to device for debugging in pollTask */
        pPvt->dxpChannel[i].pasynUser = pasynManager->createAsynUser(0, 0);
        /* Connect to device */
        status = pasynManager->connectDevice(pPvt->dxpChannel[i].pasynUser, portName, pPvt->dxpChannel[i].detChan);
        if (status != asynSuccess) {
            errlogPrintf("DXPConfig, connectDevice failed for DXP channel %d\n", pPvt->dxpChannel[i].detChan);
            return -1;
        }
    }

    if (pollFreq > 0) {
        pPvt->pollTime = 1./pollFreq;
        if (pPvt->pollTime < epicsThreadSleepQuantum())
            pPvt->pollTime = epicsThreadSleepQuantum();
        if (epicsThreadCreate("dxpPollTask",
                               epicsThreadPriorityHigh,
                               epicsThreadGetStackSize(epicsThreadStackMedium),
                               (EPICSTHREADFUNC)pollTask,
                               pPvt) == NULL)
          errlogPrintf("DXPConfig: epicsThreadCreate failure\n");
    }

    return(0);
}


static dxpChannel_t *findChannel(drvDxpPvt *pPvt, asynUser *pasynUser, 
                                 int signal)
{
    int i;
    dxpChannel_t *dxpChan = NULL;

    /* Find which channel on this module this signal is */
    for (i=0; i<(pPvt->ndetectors + pPvt->ngroups); i++) {
        if (pPvt->dxpChannel[i].exists && pPvt->dxpChannel[i].detChan == signal) { 
            dxpChan = &pPvt->dxpChannel[i];
        }
    }
    if (dxpChan == NULL) {
       asynPrint(pasynUser, ASYN_TRACE_ERROR,
                 "drvDxp::findChannel [%s signal=%d]: not a valid channel\n",
                 pPvt->portName, signal);
    }
    return(dxpChan);
}


static asynStatus int32Write(void *drvPvt, asynUser *pasynUser,
                             epicsInt32 value)
{
    return(drvDxpWrite(drvPvt, pasynUser, value, 0.));
}

static asynStatus float64Write(void *drvPvt, asynUser *pasynUser,
                               epicsFloat64 value)
{
    return(drvDxpWrite(drvPvt, pasynUser, 0, value));
}

static asynStatus drvDxpWrite(void *drvPvt, asynUser *pasynUser,
                              epicsInt32 ivalue, epicsFloat64 dvalue)
{
    drvDxpPvt *pPvt = (drvDxpPvt *)drvPvt;
    mcaCommand command=pasynUser->reason;
    asynStatus status=asynSuccess;
    unsigned short resume;
    int s;
    int signal;
    int i;
    dxpChannel_t *dxpChan;
    double double_value;
    int runActive=0;
    int detChan, detChan0;

    pasynManager->getAddr(pasynUser, &signal);
    dxpChan = findChannel(pPvt, pasynUser, signal);

    detChan = signal;
    pPvt->detChan = detChan;
    if (detChan < 0) detChan0 = 0; else detChan0 = detChan;
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "drvDxpWrite %s detChan=%d, command=%d, ivalue=%d, dvalue=%f\n",
              pPvt->portName, detChan, command, ivalue, dvalue);
    if (dxpChan == NULL) return(asynError);

    epicsHandelLock();

    switch (command) {
        case mcaStartAcquire:
            /* Start acquisition. 
             * Don't do anything if already acquiring */
            if (!dxpChan->acquiring) {
                if (dxpChan->erased) resume=0; else resume=1;
                s = xiaStartRun(detChan, resume);
                dxpChan->acquiring = 1;
                dxpChan->erased = 0;
                /* Send signal to polling task to wake it up */
                epicsEventSignal(pPvt->pollEventId);
                /* If this is a detChan set then set the acquiring flag and clear the erased
                 * flags on all detChans in the set.  (For now do all detChans). */
                if (detChan < 0) {
                    for (i=0; i<pPvt->ndetectors; i++) {
                        pPvt->dxpChannel[i].acquiring = 1;
                        pPvt->dxpChannel[i].erased = 0;
                    }
                }
                asynPrint(pasynUser, ASYN_TRACE_FLOW,
                          "drvDxpPvt [%s detChan=%d]: start acq. status=%d\n",
                          pPvt->portName, detChan, s);
            }
            break;
        case mcaStopAcquire:
            /* stop data acquisition */
            xiaStopRun(detChan);
            if (dxpChan->acquiring) {
                dxpChan->acquiring = 0;
            }
            /* If this is a detChan set then clear the acquiring flag 
             * on all detChans in the set.  (For now do all detChans). */
            if (detChan < 0) {
                for (i=0; i<pPvt->ndetectors; i++) {
                    pPvt->dxpChannel[i].acquiring = 0;
                }
            }
            break;
        case mcaErase:
            dxpChan->elive = 0.;
            dxpChan->etotal = 0.;
            /* If this is a detChan set then clear the elive, etotal values
             * on all detChans in the set.  (For now do all detChans). */
            if (detChan < 0) {
                for (i=0; i<pPvt->ndetectors; i++) {
                    pPvt->dxpChannel[i].elive = 0.;
                    pPvt->dxpChannel[i].etotal = 0.;
                }
            }
            /* If DXP is acquiring, turn it off and back on and don't 
             * set the erased flag */
            if (dxpChan->acquiring) {
                xiaStopRun(detChan);
                resume = 0;
                s = xiaStartRun(detChan, resume);
            } else {
                dxpChan->erased = 1;
                /* If this is a detChan set then 
                * set the erased flag on all detChans in the set.  (For now do all detChans). */
                if (detChan < 0) {
                    for (i=0; i<pPvt->ndetectors; i++) {
                        pPvt->dxpChannel[i].erased = 1;
                    }
                }
            }
            break;
        case mcaReadStatus:
            getAcquisitionStatus(pPvt, pasynUser, dxpChan);
            break;
        case mcaChannelAdvanceInternal:
            /* set channel advance source to internal (timed) */
            /* This is a NOOP for DXP */
            break;
        case mcaChannelAdvanceExternal:
            /* set channel advance source to external */
            /* This is a NOOP for DXP */
            break;
        case mcaNumChannels:
            /* set number of channels */
            pPvt->nchans = ivalue;
            double_value = ivalue;
            xiaGetRunData(detChan0, "run_active", &runActive);
            xiaStopRun(detChan);
            xiaSetAcquisitionValues(detChan, "number_mca_channels", 
                                    &double_value);
            if (dxpChan->moduleType == DXP_XMAP) {
                XMAP_APPLY(detChan);
            }
            break;
        case mcaModePHA:
            /* set mode to Pulse Height Analysis */
            /* This is a NOOP for DXP */
            break;
        case mcaModeMCS:
            /* set mode to MultiChannel Scaler */
            /* This is a NOOP for DXP */
            break;
        case mcaModeList:
            /* set mode to LIST (record each incoming event) */
            /* This is a NOOP for DXP */
            break;
        case mcaSequence:
            /* set sequence number */
            /* This is a NOOP for DXP */
            break;
        case mcaPrescale:
            /* This is a NOOP for DXP */
            break;
        case mcaPresetSweeps:
            /* set number of sweeps (for MCS mode) */
            /* This is a NOOP for DXP */
            break;
        case mcaPresetLowChannel:
            /* set lower side of region integrated for preset counts */
            dxpChan->ptschan = ivalue;
            break;
        case mcaPresetHighChannel:
            /* set high side of region integrated for preset counts */
            dxpChan->ptechan = ivalue;
            break;
        case mcaDwellTime:
            /* set dwell time */
            /* This is a NOOP for DXP */
            break;
        case mcaPresetRealTime:
            /* set preset real time. */
            setPreset(pPvt, pasynUser, dxpChan, mcaPresetRealTime, dvalue);
            break;
        case mcaPresetLiveTime:
            /* set preset live time. */
            setPreset(pPvt, pasynUser, dxpChan, mcaPresetLiveTime, dvalue);
            break;
        case mcaPresetCounts:
            /* set preset counts */
            dxpChan->ptotal = ivalue;
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                      "drvDXPWrite, invalid command=%d\n", command);
            status = asynError;
            break;
    }
    if (runActive) xiaStartRun(detChan, 1);
    epicsHandelUnlock();
    return(status);
}

static asynStatus int32Read(void *drvPvt, asynUser *pasynUser,
                            epicsInt32 *value)
{
    return(drvDxpRead(drvPvt, pasynUser, value, NULL));
}

static asynStatus float64Read(void *drvPvt, asynUser *pasynUser,
                              epicsFloat64 *value)
{
    return(drvDxpRead(drvPvt, pasynUser, NULL, value));
}

static asynStatus drvDxpRead(void *drvPvt, asynUser *pasynUser,
                             epicsInt32 *pivalue, epicsFloat64 *pfvalue)
{
    drvDxpPvt *pPvt = (drvDxpPvt *)drvPvt;
    mcaCommand command=pasynUser->reason;
    asynStatus status=asynSuccess;
    int signal;
    dxpChannel_t *dxpChan;
    int detChan;

    pasynManager->getAddr(pasynUser, &signal);
    dxpChan = findChannel(pPvt, pasynUser, signal);
    detChan = signal;
    if (dxpChan == NULL) return(asynError);

    switch (command) {
        case mcaAcquiring:
            *pivalue = dxpChan->acquiring;
            break;
        case mcaDwellTime:
            *pfvalue = 0.;
            break;
        case mcaElapsedLiveTime:
            *pfvalue = dxpChan->elive;
            break;
        case mcaElapsedRealTime:
            *pfvalue = dxpChan->ereal;
            break;
        case mcaElapsedCounts:
            *pfvalue = dxpChan->etotal;
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                      "drvDxp::drvDxpRead got illegal command %d\n",
                      command);
            status = asynError;
            break;
    }
    if (pivalue) asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "drvDxpRead %s detChan=%d, command=%d, *pivalue=%d\n",
              pPvt->portName, detChan, command, *pivalue);
    if (pfvalue) asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "drvDxpRead %s detChan=%d, command=%d, *pfvalue=%f\n",
              pPvt->portName, detChan, command, *pfvalue);
    return(status);
}

static asynStatus getBounds(void *drvPvt, asynUser *pasynUser,
                            epicsInt32 *low, epicsInt32 *high)
{
    *low = 0;
    *high = 0;
    return(asynSuccess);
}

static asynStatus int32ArrayRead(void *drvPvt, asynUser *pasynUser, 
                                 epicsInt32 *data, size_t maxChans, 
                                 size_t *nactual)
{
    drvDxpPvt *pPvt = (drvDxpPvt *)drvPvt;
    int signal;
    dxpChannel_t *dxpChan;
    int i;

    pasynManager->getAddr(pasynUser, &signal);
    dxpChan = findChannel(pPvt, pasynUser, signal);
    if (dxpChan == NULL) return(asynError);
    if (dxpChan->detChan < 0) return(asynSuccess); /* Ignore detChan groups */

    epicsHandelLock();

    if (dxpChan->erased) {
        memset(data, 0, pPvt->nchans*sizeof(int));
    } else {
        xiaGetRunData(dxpChan->detChan, "mca", data);
    }
    *nactual = pPvt->nchans;
    /* Compute counts in preset count window */
    if ((dxpChan->ptschan > 0) &&
        (dxpChan->ptechan > 0) &&
        (dxpChan->ptechan > dxpChan->ptschan)) {
        for (i=dxpChan->ptschan; i<=dxpChan->ptechan; i++) {
            dxpChan->etotal += data[i];
        }
     }
     if ((dxpChan->ptotal != 0.0) && 
         (dxpChan->etotal > dxpChan->ptotal)) {
         xiaStopRun(dxpChan->detChan);
         dxpChan->acquiring = 0;
     }
    epicsHandelUnlock();
    return(asynSuccess);
}

static asynStatus int32ArrayWrite(void *drvPvt, asynUser *pasynUser,
                               epicsInt32 *data, size_t maxChans)
{
    asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "drvDxp::mcaWriteData, write operations not allowed\n");
    return(asynError);
}


static void getAcquisitionStatus(drvDxpPvt *pPvt, asynUser *pasynUser, 
                                 dxpChannel_t *dxpChan) 
{
   unsigned long run_active;
   int detChan = pPvt->detChan;
   int i;

   epicsHandelLock();

   if (detChan < 0) {  /* This is a detChan set */
       dxpChan->ereal = 0.;
       dxpChan->elive = 0.;
       dxpChan->etotal = 0.;
       dxpChan->acquiring = 0;
       for (i=0; i<pPvt->ndetectors; i++) {
           dxpChan->ereal = MAX(dxpChan->ereal, pPvt->dxpChannel[i].ereal);
           dxpChan->elive = MAX(dxpChan->elive, pPvt->dxpChannel[i].elive);
           dxpChan->etotal = MAX(dxpChan->etotal, pPvt->dxpChannel[i].etotal);
           dxpChan->acquiring = MAX(dxpChan->acquiring, pPvt->dxpChannel[i].acquiring);
           asynPrint(pasynUser, ASYN_TRACE_FLOW, 
                     "getAcquisitionStatus [%s detChan set=%d, channel=%d]): acquiring=%d\n",
                     pPvt->portName, detChan, i, pPvt->dxpChannel[i].acquiring);
       }
   } else {
       if (dxpChan->erased) {
          dxpChan->elive = 0.;
          dxpChan->ereal = 0.;
          dxpChan->etotal = 0.;
       } else {
          dxpChan->etotal =  0.;
          /* The xMAP now (V0.9) supports livetime, not just trigger livetime */
          xiaGetRunData(detChan, "livetime", &dxpChan->elive);
          xiaGetRunData(detChan, "runtime", &dxpChan->ereal);
          xiaGetRunData(detChan, "run_active", &run_active);
          /* If Handel thinks the run is active, but the hardware does not, then
           * stop the run */
          if (run_active == XIA_RUN_HANDEL) xiaStopRun(detChan);
          dxpChan->acquiring = (run_active & XIA_RUN_HARDWARE);
       }
   }
   asynPrint(pasynUser, ASYN_TRACE_FLOW, 
             "getAcquisitionStatus [%s detChan=%d]): acquiring=%d\n",
             pPvt->portName, detChan, dxpChan->acquiring);
   epicsHandelUnlock();
}



static void setPreset(drvDxpPvt *pPvt, asynUser *pasynUser,
                      dxpChannel_t *dxpChan, 
                      int mode, double time)
{
   double presetType;
   double presetValue;
   int runActive=0;
   int detChan, detChan0;

   detChan = pPvt->detChan;
   if (detChan < 0) detChan0 = 0; else detChan0 = detChan;

   switch (mode) {
      case mcaPresetRealTime:
         dxpChan->preal = time;
         break;
      case mcaPresetLiveTime:
         dxpChan->plive = time;
         break;
   }
   
   epicsHandelLock();

   /* If preset live and real time are both zero set to count forever */
   if ((dxpChan->plive == 0.) && (dxpChan->preal == 0.)) {
       xiaGetRunData(detChan0, "run_active", &runActive);
       xiaStopRun(detChan);
       presetValue = 0.;
       if (dxpChan->moduleType == DXP_XMAP) {
           presetType = XIA_PRESET_NONE;
           xiaSetAcquisitionValues(detChan, "preset_type", &presetType);
           xiaSetAcquisitionValues(detChan, "preset_value", &presetValue);
           XMAP_APPLY(detChan);
       } else {
           xiaSetAcquisitionValues(detChan, "preset_standard", &presetValue);
       }
   }
   /* If preset live time is zero and real time is non-zero use real time */
   if ((dxpChan->plive == 0.) && (dxpChan->preal != 0.)) {
       time = dxpChan->preal;
       xiaGetRunData(detChan0, "run_active", &runActive);
       xiaStopRun(detChan);
       if (dxpChan->moduleType == DXP_XMAP) {
           presetType = XIA_PRESET_FIXED_REAL;
           presetValue = time; /* *1.e6;  xMAP presets are in microseconds */
           xiaSetAcquisitionValues(detChan, "preset_type", &presetType);
           xiaSetAcquisitionValues(detChan, "preset_value", &presetValue);
           XMAP_APPLY(detChan);
       } else {
           xiaSetAcquisitionValues(detChan, "preset_runtime", &time);
       }
   }
   /* If preset live time is non-zero use live time */
   if (dxpChan->plive != 0.) {
       time = dxpChan->plive;
       xiaGetRunData(detChan0, "run_active", &runActive);
       xiaStopRun(detChan);
       if (dxpChan->moduleType == DXP_XMAP) {
           presetType = XIA_PRESET_FIXED_LIVE;
           presetValue = time; /* 1.e6;  xMAP presets are in microseconds */
           xiaSetAcquisitionValues(detChan, "preset_type", &presetType);
           xiaSetAcquisitionValues(detChan, "preset_value", &presetValue);
           XMAP_APPLY(detChan);
       } else {
           xiaSetAcquisitionValues(detChan, "preset_livetime", &time);
       }
   }
   if (runActive) xiaStartRun(detChan, 1);
   epicsHandelUnlock();
}


static void pollTask(drvDxpPvt *pPvt)
{
    int  i, j;
    int addr, reason, detChan, anyAcquiring, anyChanged;
    ELLLIST *pclientList;
    interruptNode *pnode;
    int acq, *acquiring, *prev_acquiring;

    acquiring      = calloc(pPvt->ndetectors + pPvt->ngroups, sizeof(int));
    prev_acquiring = calloc(pPvt->ndetectors + pPvt->ngroups, sizeof(int));

    /* This task waits for an event from the driver signaling that acquisition has started.
     * Once acquisition starts it quickly polls until acquisition is complete.
     * When acquisition changes state it calls all registered clients with the new value of the
     * acquisition status
     */

    while(1) {
        /* Wait for event from the driver siqnalling that acquisition has started */
        epicsEventWait(pPvt->pollEventId);
        /* We set the flag saying things are now acquiring, since we could miss acquire=1 when polling for
         * very short acquisition */
        for (i=0; i<pPvt->ndetectors+pPvt->ngroups; i++) prev_acquiring[i]  = 1;
        anyAcquiring = 1;
        while(anyAcquiring) {
            /* We lock the port here because we need exclusive access to Handel, because it is not thread safe, 
             * and the asyn port thread could access it */
            pasynManager->lockPort(pPvt->dxpChannel[0].pasynUser);
            for (i=0; i<pPvt->ndetectors; i++) {
                acquiring[i] = 0;
                detChan = pPvt->dxpChannel[i].detChan;
                epicsHandelLock();
                xiaGetRunData(detChan, "run_active", &acq);
                epicsHandelUnlock();
                if (acq & XIA_RUN_HARDWARE) acquiring[i]=1;
            }
            pasynManager->unlockPort(pPvt->dxpChannel[0].pasynUser);
            for (i=pPvt->ndetectors; i<pPvt->ndetectors+pPvt->ngroups; i++) {
                acquiring[i] = 0;
                for (j=0; j<pPvt->ndetectors; j++) {
                    acquiring[i] = MAX(acquiring[i], acquiring[j]);
                }
            }
            anyAcquiring = 0;
            anyChanged = 0;
            for (i=0; i<pPvt->ndetectors+pPvt->ngroups; i++) {
                if (acquiring[i]) anyAcquiring=1;
                if ((prev_acquiring[i]==1) && (acquiring[i]==0)) anyChanged=1;
            }
            if (anyChanged) {
                /* Make int32 callbacks */
                pasynManager->interruptStart(pPvt->int32InterruptPvt, &pclientList);
                pnode = (interruptNode *)ellFirst(pclientList);
                while (pnode) {
                    asynInt32Interrupt *pint32Interrupt = pnode->drvPvt;
                    addr = pint32Interrupt->addr;
                    reason = pint32Interrupt->pasynUser->reason;
                    for (i=0; i<pPvt->ndetectors+pPvt->ngroups; i++) {
                        if ((prev_acquiring[i]==1) && (acquiring[i]==0)) {
                            detChan = pPvt->dxpChannel[i].detChan;
                            if ((reason == mcaAcquiring) && (detChan == addr)) {
                                asynPrint(pPvt->dxpChannel[i].pasynUser, ASYN_TRACE_FLOW, 
                                          "drvDxp::pollTask: issuing callback addr=%d, reason=%d, acquiring=%d\n", 
                                          addr, reason, acquiring[i]);
                                pint32Interrupt->callback(pint32Interrupt->userPvt,
                                                          pint32Interrupt->pasynUser,
                                                          acquiring[i]);
                            }
                        }
                    }
                    pnode = (interruptNode *)ellNext(&pnode->node);
                }
                pasynManager->interruptEnd(pPvt->int32InterruptPvt);
            }
            for (i=0; i<pPvt->ndetectors+pPvt->ngroups; i++) prev_acquiring[i]  = acquiring[i];
            epicsThreadSleep(pPvt->pollTime);
        }
    }
}


/* asynDrvUser routines */
static asynStatus drvUserCreate(void *drvPvt, asynUser *pasynUser,
                                const char *drvInfo,
                                const char **pptypeName, size_t *psize)
{
    int i;
    char *pstring;

    for (i=0; i<MAX_MCA_COMMANDS; i++) {
        pstring = mcaCommands[i].commandString;
        if (epicsStrCaseCmp(drvInfo, pstring) == 0) {
            pasynUser->reason = mcaCommands[i].command;
            if (pptypeName) *pptypeName = epicsStrDup(pstring);
            if (psize) *psize = sizeof(mcaCommands[i].command);
            asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "drvDxp::drvUserCreate, command=%s\n", pstring);
            return(asynSuccess);
        }
    }
    asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "drvDxp::drvUserCreate, unknown command=%s\n", drvInfo);
    return(asynError);
}

static asynStatus drvUserGetType(void *drvPvt, asynUser *pasynUser,
                                 const char **pptypeName, size_t *psize)
{
    mcaCommand command = pasynUser->reason;

    *pptypeName = epicsStrDup(mcaCommands[command].commandString);
    *psize = sizeof(command);
    return(asynSuccess);
}

static asynStatus drvUserDestroy(void *drvPvt, asynUser *pasynUser)
{
    return(asynSuccess);
}


/* asynCommon routines */

/* Report  parameters */
void report(void *drvPvt, FILE *fp, int details)
{
    drvDxpPvt *pPvt = (drvDxpPvt *)drvPvt;

    fprintf(fp, "drvDxpPvt %s: connected\n", pPvt->portName);
    if (details >= 1) {
        fprintf(fp, "              nchans: %d\n", pPvt->nchans);
    }
}

/* Connect */
asynStatus connect(void *drvPvt, asynUser *pasynUser)
{
#ifdef linux
    /* We need to call iopl(3) once for each thread */
    iopl(3);
#endif
    pasynManager->exceptionConnect(pasynUser);
    return(asynSuccess);
}

/* Disconnect */
asynStatus disconnect(void *drvPvt, asynUser *pasynUser)
{
    pasynManager->exceptionDisconnect(pasynUser);
    return(asynSuccess);
}
