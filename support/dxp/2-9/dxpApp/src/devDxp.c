/*
    devDxp.c
    Author: Mark Rivers

    This is device support for the DXP record with the asyn driver.

    28-Jun-2004  MLR  Created from old file devDxpMpf.cc that used MPF 

*/


#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <dbAccess.h>
#include <dbDefs.h>
#include <link.h>
#include <errlog.h>
#include <dbCommon.h>
#include <dbEvent.h>
#include <recSup.h>
#include <recGbl.h>
#include <alarm.h>
#include <cantProceed.h>
#include <epicsString.h>
#include <epicsThread.h>

#include <asynDriver.h>
#include <asynEpicsUtils.h>

#include "epicsHandelUtils.h"
#include "dxpRecord.h"
#include "devDxp.h"
#include "handel.h"
#include <epicsExport.h>

typedef struct {
    long fast_peaks;
    long slow_peaks;
    double icr;
    double ocr;
    double slow_trig;
    double pktim;
    double gaptim;
    double adc_rule;
    double ecal;
    double mca_bin_width;
    double number_mca_channels;
    double fast_trig;
    double trig_pktim;
    double trig_gaptim;
    double base_thresh;
    double base_len;
    double base_cut_pct;
    double base_cut_enbl;
    double maxwidth;
    double pgain;
    int newBaselineHistory;
    int newBaselineHistogram;
    int newAdcTrace;
    int newTraceWait;
    int newBHistTime;
    int acquiring;
    int blmin;
    int blmax;
    double number_scas;
    double sca_lo[NUM_DXP_SCAS];
    double sca_hi[NUM_DXP_SCAS];
    double sca_counts[NUM_DXP_SCAS];
    double eVPerBin;
} dxpReadbacks;

typedef struct {
    dxpRecord *pdxp;
    asynUser *pasynUser;
    char *portName;
    int channel;
    dxpReadbacks dxpReadbacks;
} devDxpPvt;

typedef struct {
    devDxpCommand dxpCommand;
    char *name;
    short param;
    double dvalue;
    void *pointer;
} devDxpMessage;

static long init(int after);
static long init_record(dxpRecord *pdxp, int *detChan);
static long send_dxp_msg(dxpRecord *pdxp, devDxpCommand command, 
                         char *name, int param, double dvalue,
                         void *pointer);
static void asynCallback(asynUser *pasynUser);
static void readDxpParams(asynUser *pasynUser);
static long monitor(struct dxpRecord *pdxp);

static char *sca_lo[NUM_DXP_SCAS];
static char *sca_hi[NUM_DXP_SCAS];
#define SCA_NAME_LEN 10

#define XMAP_APPLY(detChan) { \
    int i, ignore=0; \
    if (minfo->moduleType == DXP_XMAP) { \
        if (detChan < 0) { \
            for (i=0; i<minfo->numModules; i++) { \
                xiaBoardOperation(minfo->first_channels[i], "apply", &ignore); \
            } \
         } else { \
             xiaBoardOperation(detChan, "apply", &ignore); \
         } \
    } \
}

static const struct devDxpDset devDxp = {
    5,
    NULL,
    init,
    init_record,
    NULL,
    send_dxp_msg,
};
epicsExportAddress(dset, devDxp);



static long init(int after)
{
    int i;

    if (after) return(0);
    /* Create SCA strings */
    for (i=0; i<NUM_DXP_SCAS; i++) {
        sca_lo[i] = calloc(1, SCA_NAME_LEN);
        sprintf(sca_lo[i], "sca%d_lo", i);
        sca_hi[i] = calloc(1, SCA_NAME_LEN);
        sprintf(sca_hi[i], "sca%d_hi", i);
    }
    return(0);
}


static long init_record(dxpRecord *pdxp, int *detChan)
{
    char *userParam;
    asynUser *pasynUser;
    asynStatus status;
    devDxpPvt *pPvt;

    /* Allocate asynDxpPvt private structure */
    pPvt = callocMustSucceed(1, sizeof(devDxpPvt), 
                             "devDxp:: init_record");

     /* Create asynUser */
    pasynUser = pasynManager->createAsynUser(asynCallback, 0);
    pasynUser->userPvt = pPvt;
    pPvt->pasynUser = pasynUser;
    pPvt->pdxp = pdxp;
    pdxp->dpvt = pPvt;

    status = pasynEpicsUtils->parseLink(pasynUser, &pdxp->inp,
                                        &pPvt->portName, &pPvt->channel,
                                        &userParam);
    if (status != asynSuccess) {
        errlogPrintf("devDxp::init_record %s bad link %s\n",
                     pdxp->name, pasynUser->errorMessage);
        goto bad;
    }

    *detChan = pPvt->channel;

    /* Connect to device */
    status = pasynManager->connectDevice(pasynUser, pPvt->portName, 
                                         pPvt->channel);
    if (status != asynSuccess) {
        errlogPrintf("devDxp::init_record %s, connectDevice failed\n",
                     pdxp->name);
        goto bad;
    }

    return(0);
    bad:
    pdxp->pact = 1;
    return(-1);
}


static long send_dxp_msg(dxpRecord *pdxp, devDxpCommand command, 
                         char *name, int param, double dvalue,
                         void *pointer)
{
    devDxpPvt *pPvt = (devDxpPvt *)pdxp->dpvt;
    asynUser *pasynUser;
    int status;
    devDxpMessage *pmsg;

    asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW,
              "devDxp::send_dxp_msg: command=%d, pact=%d, "
              "name=%s, param=%d, dvalue=%f, pointer=%p\n",
              command, pdxp->pact,
              name, param, dvalue, pointer);

    if (pdxp->pact) {
       return(0);
    }
    pmsg = pasynManager->memMalloc(sizeof(*pmsg));
    pmsg->dxpCommand = command;
    pmsg->name = name;
    pmsg->param = param;
    pmsg->dvalue = dvalue;
    pmsg->pointer = pointer;

    pasynUser = pasynManager->duplicateAsynUser(pPvt->pasynUser,
                                                asynCallback, 0);
    pasynUser->userData = pmsg;
    /* Queue asyn request, so we get a callback when driver is ready */
    status = pasynManager->queueRequest(pasynUser, 0, 0);
    if (status != asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devDxp::send_dxp_msg: error calling queueRequest, %s\n",
                  pasynUser->errorMessage);
        return(-1);
    }
    if (interruptAccept && (command == MSG_DXP_FINISH)) pdxp->pact = 1;
    asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW,
              "devDxp::send_dxp_msg: exit\n");
    return(0);
}


void asynCallback(asynUser *pasynUser)
{
    devDxpPvt *pPvt = (devDxpPvt *)pasynUser->userPvt;
    devDxpMessage *pmsg = pasynUser->userData;
    dxpRecord *pdxp = pPvt->pdxp;
    rset *prset = (rset *)pdxp->rset;
    dxpReadbacks *pdxpReadbacks = &pPvt->dxpReadbacks;
    unsigned short monitor_mask = DBE_VALUE | DBE_LOG;
    MODULE_INFO *minfo = (MODULE_INFO *)pdxp->miptr;
    int status;
    int detChan, detChan0;
    double *pfield;
    double info[2];
    double dvalue;
    int i;
    int runActive=0;
    DXP_SCA *sca;

    pasynManager->getAddr(pasynUser, &detChan);
    if (detChan < 0) detChan0 = 0; else detChan0 = detChan;
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "devDxp::asynCallback: command=%d\n",
              pmsg->dxpCommand);

     /* Interlock access to Handel library */
     epicsHandelLock();

     switch(pmsg->dxpCommand) {
     case MSG_DXP_START_RUN:
         xiaStartRun(detChan, 1);
         break;
     case MSG_DXP_STOP_RUN:
         xiaStopRun(detChan);
         break; 
     case MSG_DXP_SET_SHORT_PARAM:
         asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
             "devDxp::asynCallback, MSG_DXP_SET_SHORT_PARAM"
             " calling xiaSetAcquisitionValues name=%s value=%d\n",
             pmsg->name, pmsg->param);
         xiaGetRunData(detChan0, "run_active", &runActive);
         xiaStopRun(detChan);
         /* Note that we use xiaSetAcquisitionValues rather than xiaSetParameter
          * so that the new value will be saved with xiaSaveSystem */
         dvalue = pmsg->param;
         xiaSetAcquisitionValues(detChan, pmsg->name, &dvalue);
         XMAP_APPLY(detChan);
         readDxpParams(pasynUser);
         break;
     case MSG_DXP_SET_DOUBLE_PARAM:
         xiaGetRunData(detChan0, "run_active", &runActive);
         xiaStopRun(detChan);
         pfield = (double *)pmsg->pointer;
         if (pfield == &pdxp->slow_trig) {
             /* Convert from keV to eV */
             dvalue = pmsg->dvalue * 1000.;
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting energy_threshold=%f\n",
                 pmsg->dvalue);
             xiaSetAcquisitionValues(detChan, "energy_threshold", &dvalue);
             XMAP_APPLY(detChan);
         }
         else if (pfield == &pdxp->pktim) {
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting peaking_time=%f\n",
                 pmsg->dvalue);
             xiaSetAcquisitionValues(detChan, "peaking_time", &pmsg->dvalue);
             XMAP_APPLY(detChan);
         }
         else if (pfield == &pdxp->gaptim) {
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting gap_time=%f\n",
                 pmsg->dvalue);
             if (minfo->moduleType == DXP_XMAP) {
                 /* On the xMAP the parameter that can be written is minimum_gap_time */
                 xiaSetAcquisitionValues(detChan, "minimum_gap_time", &pmsg->dvalue);
             } else {
                /* On the Saturn and DXP2X it is gap_time */
                 xiaSetAcquisitionValues(detChan, "gap_time", &pmsg->dvalue);
             }
             XMAP_APPLY(detChan);
         }
         else if (pfield == &pdxp->adc_rule) {
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting adc_percent_rule=%f\n",
                 pmsg->dvalue);
             xiaSetAcquisitionValues(detChan, "adc_percent_rule", &pmsg->dvalue);
             XMAP_APPLY(detChan);
         }
         else if (pfield == &pdxp->ecal) {
             dvalue = pdxp->ecal * 1000.;
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting calibration_energy=%f\n",
                 dvalue);
             xiaSetAcquisitionValues(detChan, "calibration_energy", &dvalue);
             XMAP_APPLY(detChan);
         }
         else if (pfield == &pdxp->fast_trig) {
             /* Convert from keV to eV */
             dvalue = pmsg->dvalue * 1000.;
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting trigger_threshold=%f\n",
                 dvalue);
             xiaSetAcquisitionValues(detChan, "trigger_threshold", &dvalue);
             XMAP_APPLY(detChan);
         }
         else if (pfield == &pdxp->trig_pktim) {
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting trigger_peaking_time=%f\n",
                 pmsg->dvalue);
             xiaSetAcquisitionValues(detChan, "trigger_peaking_time", &pmsg->dvalue);
             XMAP_APPLY(detChan);
         }
         else if (pfield == &pdxp->trig_gaptim) {
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting trigger_gap_time=%f\n",
                 pmsg->dvalue);
             xiaSetAcquisitionValues(detChan, "trigger_gap_time", &pmsg->dvalue);
             XMAP_APPLY(detChan);
         }
         else if (pfield == &pdxp->maxwidth) {
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting maxwidth=%f\n",
                 pmsg->dvalue);
             xiaSetAcquisitionValues(detChan, "maxwidth", &pmsg->dvalue);
             XMAP_APPLY(detChan);
         }
         else if (pfield == &pdxp->pgain) {
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting pre-amp gain=%f\n",
                 pmsg->dvalue);
             xiaSetAcquisitionValues(detChan, "preamp_gain", &pmsg->dvalue);
             XMAP_APPLY(detChan);
         }
         else if (pfield == &pdxp->base_cut_pct) {
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting baseline cut=%f\n",
                 pmsg->dvalue);
             /* The xMAP does not support this yet */
             if (minfo->moduleType != DXP_XMAP) {
                xiaSetAcquisitionValues(detChan, "baseline_cut", &pmsg->dvalue);
                XMAP_APPLY(detChan);
             }
         }
         else if (pfield == (double *)&pdxp->base_cut_enbl) {
             dvalue = pdxp->base_cut_enbl;
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting baseline cut enable=%f\n",
                 dvalue);
             /* The xMAP does not support this yet */
             if (minfo->moduleType != DXP_XMAP) {
                xiaSetAcquisitionValues(detChan, "enable_baseline_cut", &dvalue);
                XMAP_APPLY(detChan);
             }
         }
         else if (pfield == (double *)&pdxp->base_len) {
             /* Values start at 2 */
             dvalue = 1 << (pdxp->base_len + 1);
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting baseline filter length=%f\n",
                 dvalue);
             if (minfo->moduleType == DXP_XMAP) {
                 xiaSetAcquisitionValues(detChan, "baseline_average", &dvalue);
                 XMAP_APPLY(detChan);
             } else {
                 xiaSetAcquisitionValues(detChan, "baseline_filter_length", &dvalue);
             }
         }
         else if (pfield == &pdxp->base_thresh) {
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting baseline threshold=%f\n",
                 pmsg->dvalue);
              dvalue = pmsg->dvalue * 1000.;    /* Convert to eV */
              xiaSetAcquisitionValues(detChan, "baseline_threshold", &dvalue);
              XMAP_APPLY(detChan);
         }
         else if (pfield == &pdxp->emax) {
             if (pdxpReadbacks->number_mca_channels <= 0.)
                 pdxpReadbacks->number_mca_channels = 2048.;
             /* Set the bin width in eV */
             dvalue = pmsg->dvalue *1000. / pdxpReadbacks->number_mca_channels;
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting emax=%f, mca_bin_width=%f\n",
                 pmsg->dvalue, dvalue);
             xiaSetAcquisitionValues(detChan, "mca_bin_width", &dvalue);
             XMAP_APPLY(detChan);
         }
         else if (pfield == &pdxp->trace_wait) {
             /* New value of trace wait.  Just set flag, monitor() will post events */
             pdxpReadbacks->newTraceWait = 1;
         } 
         else if (pfield == &pdxp->bhist_time) {
             /* New value of baseline history time.  Just set flag, monitor() will post events */
             pdxpReadbacks->newBHistTime = 1;
         } 
         readDxpParams(pasynUser);
         break;
     case MSG_DXP_SET_SCAS:
         xiaGetRunData(detChan0, "run_active", &runActive);
         xiaStopRun(detChan);
         xiaSetAcquisitionValues(detChan, "number_of_scas", &pmsg->dvalue);
         XMAP_APPLY(detChan);
         sca = (DXP_SCA *)&pdxp->sca0_lo;
         for (i=0; i<pmsg->dvalue; i++) {
             if (sca[i].lo < 0) {
                 sca[i].lo = 0;
                 db_post_events(pdxp,&sca[i].lo,monitor_mask);
             }
             if (sca[i].hi < 0) {
                 sca[i].hi = 0;
                 db_post_events(pdxp,&sca[i].hi,monitor_mask);
             }
             if (sca[i].hi < sca[i].lo) {
                 sca[i].hi = sca[i].lo;
                 db_post_events(pdxp,&sca[i].hi,monitor_mask);
             }
             dvalue = sca[i].lo;
             xiaSetAcquisitionValues(detChan, sca_lo[i], &dvalue);
             XMAP_APPLY(detChan);
             dvalue = sca[i].hi;
             xiaSetAcquisitionValues(detChan, sca_hi[i], &dvalue);
             XMAP_APPLY(detChan);
          }
         readDxpParams(pasynUser);
         break;
     case MSG_DXP_READ_BASELINE:
         xiaGetRunData(detChan0, "baseline", pdxp->bptr);
         pdxpReadbacks->newBaselineHistogram = 1;
         break;
     case MSG_DXP_CONTROL_TASK:
         xiaGetRunData(detChan0, "run_active", &runActive);
         xiaStopRun(detChan);
         info[0] = 1.;
         info[1] = pmsg->dvalue;
         asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
             "devDxp::asynCallback, MSG_DXP_CONTROL_TASK"
             " doing special run=%s, info=%f %f\n",
             pmsg->name, info[0], info[1]);
         xiaDoSpecialRun(detChan, pmsg->name, info);
         if (strcmp(pmsg->name, "adc_trace") == 0) {
            asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_CONTROL_TASK"
                 " reading adc_trace\n",
                 pmsg->dvalue);
            xiaGetSpecialRunData(detChan0, "adc_trace", pdxp->tptr);
            pdxpReadbacks->newAdcTrace = 1;
         }
         else if (strcmp(pmsg->name, "baseline_history") == 0) {
            asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_CONTROL_TASK"
                 " reading baseline_history\n",
                 pmsg->dvalue);
            xiaGetSpecialRunData(detChan0, "baseline_history", pdxp->bhptr);
            pdxpReadbacks->newBaselineHistory = 1;
         }   
         break;
     case MSG_DXP_READ_PARAMS:
         readDxpParams(pasynUser);
         break;
     case MSG_DXP_FINISH:
         if (pdxp->udf==1) pdxp->udf=0;
         if (!interruptAccept) break;
         dbScanLock((dbCommon *)pdxp);
         monitor(pdxp);
         (*prset->process)(pdxp);
         dbScanUnlock((dbCommon *)pdxp);
         break;
     default:
         asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                   "devDxp::asynCallback, invalid command=%d\n",
                   pmsg->dxpCommand);
         break;
     }
     if (runActive) xiaStartRun(detChan, 1);
     pasynManager->memFree(pmsg, sizeof(*pmsg));
     status = pasynManager->freeAsynUser(pasynUser);
     if (status != asynSuccess) {
         asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devMcaAsyn::asynCallback: error in freeAsynUser\n");
     }
     epicsHandelUnlock();
     asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW,
              "devDxp::asynCallback: exit\n");
}


static void readDxpParams(asynUser *pasynUser)
{
    devDxpPvt *pPvt = (devDxpPvt *)pasynUser->userPvt;
    dxpRecord *pdxp = pPvt->pdxp;
    dxpReadbacks *pdxpReadbacks = &pPvt->dxpReadbacks;
    int i;
    double number_mca_channels;
    double dvalue;
    int detChan;
    int acquiring;
    int runActive=0;
    MODULE_INFO *minfo = (MODULE_INFO *)pdxp->miptr;

    asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW,
             "devDxp::readDxpParams: entry\n");
    pasynManager->getAddr(pasynUser, &detChan);
    if (detChan < 0) detChan = 0;
    xiaGetParamData(detChan, "values", pdxp->pptr);
    xiaGetRunData(detChan, "input_count_rate", &pdxpReadbacks->icr);
    xiaGetRunData(detChan, "output_count_rate", &pdxpReadbacks->ocr);
    /* The xMAP does not yet support "triggers" as an acquisition parameter. */
    if (minfo->moduleType == DXP_XMAP) {
        pdxpReadbacks->fast_peaks = (pdxp->pptr[minfo->offsets.triggersa] << 16) + 
                                    pdxp->pptr[minfo->offsets.triggers];
    } else {
       xiaGetRunData(detChan, "triggers", &pdxpReadbacks->fast_peaks);
    }
    xiaGetRunData(detChan, "events_in_run", &pdxpReadbacks->slow_peaks);
    xiaGetRunData(detChan, "run_active", &acquiring);
    /* run_active returns multiple bits - convert to 0/1 */
    pdxpReadbacks->acquiring = (acquiring != 0);
    /* Only read the following if the channel is not acquiring. They
     * have already been read when they were last changed, and we want
     * to be as efficient as possible */
    if (pdxpReadbacks->acquiring == 0) {
        xiaGetAcquisitionValues(detChan, "energy_threshold",
                                &pdxpReadbacks->slow_trig);
        /* Convert energy thresholds from eV to keV */
        pdxpReadbacks->slow_trig /= 1000.;
        xiaGetAcquisitionValues(detChan, "peaking_time",
                                &pdxpReadbacks->pktim);
        xiaGetAcquisitionValues(detChan, "gap_time",
                                &pdxpReadbacks->gaptim);
        xiaGetAcquisitionValues(detChan, "trigger_threshold",
                                &pdxpReadbacks->fast_trig);
        /* Convert energy thresholds from eV to keV */
        pdxpReadbacks->fast_trig /= 1000.;
        xiaGetAcquisitionValues(detChan, "trigger_peaking_time",
                                &pdxpReadbacks->trig_pktim);
        xiaGetAcquisitionValues(detChan, "trigger_gap_time",
                                &pdxpReadbacks->trig_gaptim);
        xiaGetAcquisitionValues(detChan, "preamp_gain",
                                &pdxpReadbacks->pgain);
        if (minfo->moduleType == DXP_XMAP) {
           xiaGetAcquisitionValues(detChan, "baseline_average",
                                   &pdxpReadbacks->base_len);
        } else {
           xiaGetAcquisitionValues(detChan, "baseline_filter_length",
                                   &pdxpReadbacks->base_len);
        }
        xiaGetAcquisitionValues(detChan, "baseline_threshold",
                                &pdxpReadbacks->base_thresh);
        pdxpReadbacks->base_thresh /= 1000.;  /* Convert to keV */
        xiaGetAcquisitionValues(detChan, "maxwidth",
                                &pdxpReadbacks->maxwidth);
        if (minfo->moduleType != DXP_XMAP) {
           xiaGetAcquisitionValues(detChan, "baseline_cut",
                                   &pdxpReadbacks->base_cut_pct);
           xiaGetAcquisitionValues(detChan, "enable_baseline_cut",
                                   &pdxpReadbacks->base_cut_enbl);
        }
        xiaGetAcquisitionValues(detChan, "adc_percent_rule",
                                &pdxpReadbacks->adc_rule);
        xiaGetAcquisitionValues(detChan, "calibration_energy",
                                &pdxpReadbacks->ecal);
        xiaGetAcquisitionValues(detChan, "mca_bin_width",
                                &pdxpReadbacks->mca_bin_width);
        xiaGetAcquisitionValues(detChan, "number_mca_channels",
                                &number_mca_channels);
        if (pdxpReadbacks->number_mca_channels == 0) 
           pdxpReadbacks->number_mca_channels = number_mca_channels;
        /* If the number of mca channels has changed then recompute mca_bin_width from
         * emax */
        if (number_mca_channels != pdxpReadbacks->number_mca_channels) {
           pdxpReadbacks->number_mca_channels = number_mca_channels;
           if (pdxpReadbacks->number_mca_channels <= 0.)
               pdxpReadbacks->number_mca_channels = 2048.;
           /* Set the bin width in eV */
           dvalue = pdxp->emax *1000. / pdxpReadbacks->number_mca_channels;
           asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
               "devDxp::readDxpParams resetting mca_bin_width"
               " setting emax=%f, mca_bin_width=%f\n",
               pdxp->emax, dvalue);
           xiaGetRunData(detChan, "run_active", &runActive);
           xiaStopRun(detChan);
           xiaSetAcquisitionValues(detChan, "mca_bin_width", &dvalue);
           XMAP_APPLY(detChan);
           if (runActive) xiaStartRun(detChan, 1);
           xiaGetAcquisitionValues(detChan, "mca_bin_width",
                                   &pdxpReadbacks->mca_bin_width);
        }
  
        xiaGetAcquisitionValues(detChan, "number_of_scas",
                                &pdxpReadbacks->number_scas);
        for (i=0; i<pdxpReadbacks->number_scas; i++) {
            xiaGetAcquisitionValues(detChan, sca_lo[i], &pdxpReadbacks->sca_lo[i]);
            xiaGetAcquisitionValues(detChan, sca_hi[i], &pdxpReadbacks->sca_hi[i]);
        }
        /* The sca data on the xMAP is double, it is long on other products */
        if (minfo->moduleType == DXP_XMAP) {
            xiaGetRunData(detChan, "sca", pdxpReadbacks->sca_counts);
        } else {
            long long_sca_counts[NUM_DXP_SCAS];
            xiaGetRunData(detChan, "sca", long_sca_counts);
            for (i=0; i<pdxpReadbacks->number_scas; i++) {
                pdxpReadbacks->sca_counts[i] = long_sca_counts[i];
            }
        }
    }
    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
        "devDxp::readDxpParams\n"
        "input_count_rate:     %f\n"
        "output_count_rate:    %f\n"
        "triggers:             %d\n"
        "events_in_run:        %d\n"
        "run_active:           %d\n"
        "energy_threshold:     %f\n"
        "peaking_time:         %f\n"
        "gap_time:             %f\n"
        "trigger_threshold:    %f\n"
        "trigger_peaking_time: %f\n"
        "trigger_gap_time:     %f\n"
        "base_threshold:       %f\n"
        "adc_percent_rule:     %f\n"
        "calibration_energy:   %f\n"
        "mca_bin_width:        %f\n"
        "number_mca_channels:  %f\n",
        pdxpReadbacks->icr, 
        pdxpReadbacks->ocr,
        pdxpReadbacks->fast_peaks, 
        pdxpReadbacks->slow_peaks, 
        acquiring,
        pdxpReadbacks->slow_trig, 
        pdxpReadbacks->pktim,
        pdxpReadbacks->gaptim,
        pdxpReadbacks->fast_trig, 
        pdxpReadbacks->trig_pktim,
        pdxpReadbacks->trig_gaptim,
        pdxpReadbacks->base_thresh,
        pdxpReadbacks->adc_rule, 
        pdxpReadbacks->ecal,
        pdxpReadbacks->mca_bin_width,
        pdxpReadbacks->number_mca_channels);

    asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW,
             "devDxp::readDxpParams: exit\n");
}


static long monitor(struct dxpRecord *pdxp)
{
   devDxpPvt *pPvt = pdxp->dpvt;
   dxpReadbacks *pdxpReadbacks = &pPvt->dxpReadbacks;
   unsigned short monitor_mask = recGblResetAlarms(pdxp) | DBE_VALUE | DBE_LOG;
   DXP_SCA *sca;
   int i;
   int newBlCut;
   double eVPerADC;
   double eVPerBin;
   double emax;
   int blmin;
   int blmax;
   int basebinning;
   int slowlen;
   MODULE_INFO *minfo = (MODULE_INFO *)pdxp->miptr;

   asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW,
             "devDxp::monitor: entry\n");
   if (minfo == NULL) return(-1);
   blmin          = (short)pdxp->pptr[minfo->offsets.blmin];
   blmax          = (short)pdxp->pptr[minfo->offsets.blmax];
   basebinning    = pdxp->pptr[minfo->offsets.basebinning];
   slowlen        = pdxp->pptr[minfo->offsets.slowlen];

   /* See if readbacks have changed  */ 
   if (pdxp->slow_trig_rbv != pdxpReadbacks->slow_trig) {
      pdxp->slow_trig_rbv = pdxpReadbacks->slow_trig;
      db_post_events(pdxp, &pdxp->slow_trig_rbv, monitor_mask);
   }
   if (pdxp->pktim_rbv != pdxpReadbacks->pktim) {
      pdxp->pktim_rbv = pdxpReadbacks->pktim;
      db_post_events(pdxp, &pdxp->pktim_rbv, monitor_mask);
   };
   if (pdxp->gaptim_rbv != pdxpReadbacks->gaptim) {
      pdxp->gaptim_rbv = pdxpReadbacks->gaptim;
      db_post_events(pdxp, &pdxp->gaptim_rbv, monitor_mask);
   }
   if (pdxp->fast_trig_rbv != pdxpReadbacks->fast_trig) {
      pdxp->fast_trig_rbv = pdxpReadbacks->fast_trig;
      db_post_events(pdxp, &pdxp->fast_trig_rbv, monitor_mask);
   }
   if (pdxp->trig_pktim_rbv != pdxpReadbacks->trig_pktim) {
      pdxp->trig_pktim_rbv = pdxpReadbacks->trig_pktim;
      db_post_events(pdxp, &pdxp->trig_pktim_rbv, monitor_mask);
   }
   if (pdxp->trig_gaptim_rbv != pdxpReadbacks->trig_gaptim) {
      pdxp->trig_gaptim_rbv = pdxpReadbacks->trig_gaptim;
      db_post_events(pdxp, &pdxp->trig_gaptim_rbv, monitor_mask);
   }
   if (pdxp->maxwidth_rbv != pdxpReadbacks->maxwidth) {
      pdxp->maxwidth_rbv = pdxpReadbacks->maxwidth;
      db_post_events(pdxp, &pdxp->maxwidth_rbv, monitor_mask);
   }
   if (pdxp->adc_rule_rbv != pdxpReadbacks->adc_rule) {
      pdxp->adc_rule_rbv = pdxpReadbacks->adc_rule;
      db_post_events(pdxp, &pdxp->adc_rule_rbv, monitor_mask);
   }
   if (pdxp->pgain_rbv != pdxpReadbacks->pgain) {
      pdxp->pgain_rbv = pdxpReadbacks->pgain;
      db_post_events(pdxp, &pdxp->pgain_rbv, monitor_mask);
   }
   if (pdxp->base_len_rbv != pdxpReadbacks->base_len) {
      pdxp->base_len_rbv = pdxpReadbacks->base_len;
      db_post_events(pdxp, &pdxp->base_len_rbv, monitor_mask);
   }
   if (pdxp->base_cut_pct_rbv != pdxpReadbacks->base_cut_pct) {
      pdxp->base_cut_pct_rbv = pdxpReadbacks->base_cut_pct;
      db_post_events(pdxp, &pdxp->base_cut_pct_rbv, monitor_mask);
   }
   if (pdxp->base_thresh_rbv != pdxpReadbacks->base_thresh) {
      pdxp->base_thresh_rbv = pdxpReadbacks->base_thresh;
      db_post_events(pdxp, &pdxp->base_thresh_rbv, monitor_mask);
   }
   emax = pdxpReadbacks->mca_bin_width / 1000. * 
                         pdxpReadbacks->number_mca_channels;
   if (pdxp->emax_rbv != emax) {
      pdxp->emax_rbv = emax;
      db_post_events(pdxp, &pdxp->emax_rbv, monitor_mask);
   }
   if (pdxp->fast_peaks != pdxpReadbacks->fast_peaks) {
      pdxp->fast_peaks = pdxpReadbacks->fast_peaks;
      db_post_events(pdxp, &pdxp->fast_peaks, monitor_mask);
   }
   if (pdxp->slow_peaks != pdxpReadbacks->slow_peaks) {
      pdxp->slow_peaks = pdxpReadbacks->slow_peaks;
      db_post_events(pdxp, &pdxp->slow_peaks, monitor_mask);
   }
   if (pdxp->icr != pdxpReadbacks->icr) {
      pdxp->icr = pdxpReadbacks->icr;
      db_post_events(pdxp, &pdxp->icr, monitor_mask);
   }
   if (pdxp->ocr != pdxpReadbacks->ocr) {
      pdxp->ocr = pdxpReadbacks->ocr;
      db_post_events(pdxp, &pdxp->ocr, monitor_mask);
   }
   if (pdxp->num_scas != pdxpReadbacks->number_scas) {
      pdxp->num_scas = pdxpReadbacks->number_scas;
      db_post_events(pdxp, &pdxp->num_scas, monitor_mask);
   }
   if (pdxp->acqg != pdxpReadbacks->acquiring) {
      pdxp->acqg = pdxpReadbacks->acquiring;
      db_post_events(pdxp, &pdxp->acqg, monitor_mask);
   }
                                  
   /* Post events on array fields if they have changed */
   if (pdxpReadbacks->newBaselineHistogram) {
      pdxpReadbacks->newBaselineHistogram = 0;
      db_post_events(pdxp,pdxp->bptr,monitor_mask);
      /* See if the x-axis has also changed */
      eVPerADC = 1000.*pdxp->emax/2. / (pdxp->adc_rule/100. * 1024);
      eVPerBin = ((1 << basebinning) / (slowlen * 4.0)) * eVPerADC;
      if (eVPerBin != pdxpReadbacks->eVPerBin) {
         pdxpReadbacks->eVPerBin = eVPerBin;
         for (i=0; i<minfo->nbase_histogram; i++) {
            pdxp->bxptr[i] = (i-512) * eVPerBin / 1000.;
         }
         db_post_events(pdxp,pdxp->bxptr,monitor_mask);
      }
   }
   if (pdxpReadbacks->newBaselineHistory) {
      pdxpReadbacks->newBaselineHistory = 0;
      db_post_events(pdxp,pdxp->bhptr,monitor_mask);
   }
   if (pdxpReadbacks->newAdcTrace) {
      pdxpReadbacks->newAdcTrace = 0;
      db_post_events(pdxp,pdxp->tptr,monitor_mask);
   }
   if (pdxpReadbacks->newTraceWait) {
      pdxpReadbacks->newTraceWait = 0;
      /* Recompute the trace_x array, erase trace array */
      for (i=0; i<minfo->ntrace; i++) {
         pdxp->tptr[i] = 0;
         pdxp->txptr[i] = pdxp->trace_wait * i;
      }
      db_post_events(pdxp,pdxp->tptr,monitor_mask);
      db_post_events(pdxp,pdxp->txptr,monitor_mask);
   }
   if (pdxpReadbacks->newBHistTime) {
      pdxpReadbacks->newBHistTime = 0;
      /* Recompute the bhist_x array, erase baseline history array */
      for (i=0; i<minfo->nbase_history; i++) {
         pdxp->bhptr[i] = 0;
         pdxp->bhxptr[i] = pdxp->bhist_time * i;
      }
      db_post_events(pdxp,pdxp->bhptr,monitor_mask);
      db_post_events(pdxp,pdxp->bhxptr,monitor_mask);
   }


   /* If BLMIN, BLMAX, or enable_baseline_cut have changed then
    * recompute the BASE_CUT array */
   newBlCut = 0;
   if (pdxp->base_cut_enbl_rbv != pdxpReadbacks->base_cut_enbl) {
      newBlCut = 1;
      pdxp->base_cut_enbl_rbv = pdxpReadbacks->base_cut_enbl;
      db_post_events(pdxp, &pdxp->base_cut_enbl_rbv, monitor_mask);
   }
   if (blmin != pdxpReadbacks->blmin) {
      newBlCut = 1;
      pdxpReadbacks->blmin = blmin;
   }
   if (blmax != pdxpReadbacks->blmax) {
      newBlCut = 1;
      pdxpReadbacks->blmax = blmax;
   }
   if (newBlCut) {
      for (i=0; i<minfo->nbase_histogram; i++) pdxp->bcptr[i] = 1;
      if (pdxp->base_cut_enbl_rbv) {
         blmin = pdxpReadbacks->blmin / (1 << basebinning);
         blmin += 512;
         if (blmin < 0) blmin = 0;
         blmax = pdxpReadbacks->blmax / (1 << basebinning);
         blmax += 512;
         if (blmax > minfo->nbase_histogram) blmax = minfo->nbase_histogram;
         for (i=blmin; i<blmax; i++) {
            pdxp->bcptr[i] = 65535;
         }
      }
      db_post_events(pdxp,pdxp->bcptr,monitor_mask);
   }

   /* Address of first SCA */
   sca = (DXP_SCA *)&pdxp->sca0_lo;
   for (i=0; i<NUM_DXP_SCAS; i++) {
      if (pdxpReadbacks->sca_lo[i] != sca[i].lo_rbv) {
         sca[i].lo_rbv = pdxpReadbacks->sca_lo[i];
         db_post_events(pdxp, &sca[i].lo_rbv, monitor_mask);   
      }
      if (pdxpReadbacks->sca_hi[i] != sca[i].hi_rbv) {
         sca[i].hi_rbv = pdxpReadbacks->sca_hi[i];
         db_post_events(pdxp, &sca[i].hi_rbv, monitor_mask);   
      }
      if (pdxpReadbacks->sca_counts[i] != sca[i].counts) {
         sca[i].counts = pdxpReadbacks->sca_counts[i];
         db_post_events(pdxp, &sca[i].counts, monitor_mask);   
      }
   }
   asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW,
             "devDxp::monitor: exit\n");
   return(0);
}
