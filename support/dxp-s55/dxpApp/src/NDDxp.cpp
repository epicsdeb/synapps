/* Standard includes... */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* EPICS includes */
#include <epicsString.h>
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsEvent.h>
#include <epicsMutex.h>
#include <epicsExit.h>
#include <envDefs.h>
#include <iocsh.h>
#include <epicsExport.h>

/* Handel includes */
#include <handel.h>
#include <handel_errors.h>
#include <handel_generic.h>
#include <xerxes_generic.h>
#include <md_generic.h>
#include <handel_constants.h>

/* MCA includes */
#include <drvMca.h>

/* Area Detector includes */
#include <asynNDArrayDriver.h>

#include "NDDxp.h"

#define DXP_ALL                   -1
#define XMAP_NCHANS_MODULE         4
#define XMAP_MAX_MCA_BINS      16384
#define XMAP_MCA_BIN_RES         256
#define DXP_MAX_SCAS              16
#define LEN_SCA_NAME              10
#define XMAP_CLOCK_PERIOD     320e-9

/* It is much easier to define a maximum fixed number of low-level DXP parameters.
 * The actual maximum is current 209 for the xMAP, so this is safe for now, and is
 * easy to increase in the future */
#define DXP_MAX_LL_PARAMS        220

/** < The maximum number of bytes in the 2MB mapping mode buffer */
#define XMAP_MAPBUF_SIZE 2097152
/** < The XMAP buffer takes up 2MB of 16bit words. Unfortunatly the transfer over PCI
  * uses 32bit words, so the data we receive from from the Handel library is 2x2MB. */
#define XMAP_MAPBUF_READ_SIZE 2*XMAP_MAPBUF_SIZE
#define MEGABYTE             1048576

#define CALLHANDEL( handel_call, msg ) { \
    xiastatus = handel_call; \
    status = this->xia_checkError( pasynUser, xiastatus, msg ); \
}

/** Only used for debugging/error messages to identify where the message comes from*/
static const char *driverName = "NDDxp";

typedef enum {
    NDDxpModelXMAP, 
    NDDxpModelSaturn, 
    NDDxpModel4C2X
} NDDxpModel_t;

typedef enum {
    NDDxpModeMCA, 
    NDDxpModeSpectraMapping,
    NDDxpModeSCAMapping
} NDDxpCollectMode_t;

typedef enum {
    NDDxpPresetModeNone,
    NDDxpPresetModeReal,
    NDDxpPresetModeLive,
    NDDxpPresetModeEvents,
    NDDxpPresetModeTriggers
} NDDxpPresetMode_t;

typedef enum {
    NDDxpPixelAdvanceGate,
    NDDxpPixelAdvanceSync,
} NDDxpPixelAdvanceMode_t;

typedef enum {
    NDDxpTraceADC, 
    NDDxpTraceBaselineHistory,
    NDDxpTraceTriggerFilter, 
    NDDxpTraceBaselineFilter, 
    NDDxpTraceEnergyFilter,
    NDDxpTraceBaselineSamples, 
    NDDxpTraceEnergySamples
} NDDxpTraceMode_t;

static char *NDDxpTraceCommands[] = {"adc_trace", "baseline_history",
                                     "trigger_filter", "baseline_filter", "energy_filter",
                                     "baseline_samples", "energy_samples"};

static char *NDDxpBufferCharString[2] = {"a", "b"};
static char *NDDxpBufferFullString[2] = {"buffer_full_a", "buffer_full_b"};
static char *NDDxpBufferString[2]     = {"buffer_a", "buffer_b"};

static char SCA_NameLow[DXP_MAX_SCAS][LEN_SCA_NAME];
static char SCA_NameHigh[DXP_MAX_SCAS][LEN_SCA_NAME];

/* These values are static because we need to access them in the qsort callback, 
 * no class reference available */
static unsigned short numLLParams;
static char *LLParamNames[DXP_MAX_LL_PARAMS];
static unsigned short LLParamValues[DXP_MAX_LL_PARAMS];
static int LLParamSort[DXP_MAX_LL_PARAMS];

typedef struct moduleStatistics {
    double realTime;
    double triggerLiveTime;
    double energyLiveTime;
    double triggers;
    double events;
    double icr;
    double ocr;
} moduleStatistics;

/* Mapping mode parameters */
#define NDDxpCollectModeString              "DxpCollectMode"
#define NDDxpPixelAdvanceModeString         "DxpPixelAdvanceMode"
#define NDDxpCurrentPixelString             "DxpCurrentPixel"
#define NDDxpNextPixelString                "DxpNextPixel"
#define NDDxpPixelsPerBufferString          "DxpPixelsPerBuffer"
#define NDDxpAutoPixelsPerBufferString      "DxpAutoPixelsPerBuffer"
#define NDDxpPixelsPerRunString             "DxpPixelsPerRun"
#define NDDxpBufferOverrunString            "DxpBufferOverrun"
#define NDDxpMBytesReceivedString           "DxpMBytesReceived"
#define NDDxpReadSpeedString                "DxpReadSpeed"
#define NDDxpIgnoreGateString               "DxpIgnoreGate"
#define NDDxpSyncCountString                "DxpSyncCount"
#define NDDxpInputLogicPolarityString       "DxpInputLogicPolarity"

/* Internal asyn driver parameters */
#define NDDxpErasedString                   "DxpErased"
#define NDDxpAcquiringString                "NDDxpAcquiring"  /* Internal use only !!! */
#define NDDxpPixelCounterString             "DxpPixelCounter"
#define NDDxpBufferCounterString            "DxpBufferCounter"
#define NDDxpPollTimeString                 "DxpPollTime"
#define NDDxpForceReadString                "DxpForceRead"
#define NDDxpXMAPApplyString                "DxpXMAPApply"
#define NDDxpXMAPAutoApplyString            "DxpXMAPAutoApply"

/* Diagnostic trace parameters */
#define NDDxpTraceModeString                "DxpTraceMode"
#define NDDxpTraceTimeString                "DxpTraceTime"
#define NDDxpTraceString                    "DxpTrace"
#define NDDxpBaselineHistogramString        "DxpBaselineHistogram"

/* Runtime statistics */
#define NDDxpTriggerLiveTimeString          "DxpTriggerLiveTime"
#define NDDxpTriggersString                 "DxpTriggers"
#define NDDxpEventsString                   "DxpEvents"
#define NDDxpInputCountRateString           "DxpInputCountRate"
#define NDDxpOutputCountRateString          "DxpOutputCountRate"

/* High-level DXP parameters */
#define NDDxpPeakingTimeString              "DxpPeakingTime"
#define NDDxpDynamicRangeString             "DxpDynamicRange"
#define NDDxpTriggerThresholdString         "DxpTriggerThreshold"
#define NDDxpBaselineThresholdString        "DxpBaselineThreshold"
#define NDDxpEnergyThresholdString          "DxpEnergyThreshold"
#define NDDxpCalibrationEnergyString        "DxpCalibrationEnergy"
#define NDDxpADCPercentRuleString           "DxpADCPercentRule"
#define NDDxpMCABinWidthString              "DxpMCABinWidth"
#define NDDxpMaxEnergyString                "DxpMaxEnergy"
#define NDDxpPreampGainString               "DxpPreampGain"
#define NDDxpNumMCAChannelsString           "DxpNumMCAChannels"
#define NDDxpDetectorPolarityString         "DxpDetectorPolarity"
#define NDDxpResetDelayString               "DxpResetDelay"
#define NDDxpDecayTimeString                "DxpDecayTime"
#define NDDxpGapTimeString                  "DxpGapTime"
#define NDDxpTriggerPeakingTimeString       "DxpTriggerPeakingTime"
#define NDDxpTriggerGapTimeString           "DxpTriggerGapTime"
#define NDDxpBaselineAverageString          "DxpBaselineAverage"
#define NDDxpBaselineCutString              "DxpBaselineCut"
#define NDDxpEnableBaselineCutString        "DxpEnableBaselineCut"
#define NDDxpMaxWidthString                 "DxpMaxWidth"
#define NDDxpPresetModeString               "DxpPresetMode"
#define NDDxpPresetEventsString             "DxpPresetEvents"
#define NDDxpPresetTriggersString           "DxpPresetTriggers"

/* SCA parameters */
#define NDDxpNumSCAsString                  "DXPNumSCAs"
/* For each SCA there are 3 parameters
  * DXPSCA$(N)Low
  * DXPSCA$(N)High
  * DXPSCA$(N)Counts
*/

/* INI file parameters */
#define NDDxpSaveSystemFileString           "DxpSaveSystemFile"
#define NDDxpSaveSystemString               "DxpSaveSystem"

/* Low-level DXP parameters */
#define NDDxpNumLLParamsString              "DxpNumLLParams"
#define NDDxpReadLLParamsString             "DxpReadLLParams"
/* For each low-level parameter
  * DxpLL_$(PARAM_NAME)
*/

class NDDxp : public asynNDArrayDriver
{
public:
    NDDxp(const char *portName, int nCChannels, int maxBuffers, size_t maxMemory);

    /* virtual methods to override from asynNDArrayDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual asynStatus readInt32Array(asynUser *pasynUser, epicsInt32 *value, size_t nElements, size_t *nIn);
    void report(FILE *fp, int details);

    /* Local methods to this class */
    asynStatus xia_checkError( asynUser* pasynUser, epicsInt32 xiastatus, char *xiacmd );
    void shutdown();

    void acquisitionTask();
    asynStatus pollMappingMode();
    int getChannel(asynUser *pasynUser, int *addr);
    int getModuleType();
    asynStatus apply(int channel, int forceApply=0);
    asynStatus setPresets(asynUser *pasynUser, int addr);
    asynStatus setDxpParam(asynUser *pasynUser, int addr, int function, double value);
    asynStatus getDxpParams(asynUser *pasynUser, int addr);
    asynStatus setLLDxpParam(asynUser *pasynUser, int addr, int value);
    asynStatus getLLDxpParams(asynUser *pasynUser, int addr);
    asynStatus setSCAs(asynUser *pasynUser, int addr);
    asynStatus getSCAs(asynUser *pasynUser, int addr);
    asynStatus getSCAData(asynUser *pasynUser, int addr);
    asynStatus getAcquisitionStatus(asynUser *pasynUser, int addr);
    asynStatus getModuleStatistics(asynUser *pasynUser, int addr, moduleStatistics *stats);
    asynStatus getAcquisitionStatistics(asynUser *pasynUser, int addr);
    asynStatus getMcaData(asynUser *pasynUser, int addr);
    asynStatus getMappingData();
    asynStatus getTrace(asynUser* pasynUser, int addr,
                        epicsInt32* data, size_t maxLen, size_t *actualLen);
    asynStatus getBaselineHistogram(asynUser* pasynUser, int addr,
                        epicsInt32* data, size_t maxLen, size_t *actualLen);
    asynStatus configureCollectMode();
    asynStatus setNumChannels(asynUser *pasynUser, epicsInt32 newsize, epicsInt32 *rbValue);
    asynStatus startAcquiring(asynUser *pasynUser);
    asynStatus stopAcquiring(asynUser *pasynUser);
    asynStatus xmapGetModuleChannels(int currentChannel, int* firstChannel, int* nChannels);
    int lookupParam(const char *paramName);

protected:
    /* Mapping mode parameters */
    int NDDxpCollectMode;                   /** < Change mode of the XMAP (0=mca; 1=spectra mapping; 2=sca mapping) (int32 read/write) addr: all/any */
    #define FIRST_DXP_PARAM NDDxpCollectMode
    int NDDxpPixelAdvanceMode;             /** < XMAP mapping mode only: pixel advance mode (int) */
    int NDDxpCurrentPixel;                  /** < XMAP mapping mode only: read the current pixel that is being acquired into (int) */
    int NDDxpNextPixel;                     /** < XMAP mapping mode only: force a pixel increment in the xmap buffer (write only int). Value is ignored. */
    int NDDxpPixelsPerBuffer;
    int NDDxpAutoPixelsPerBuffer;
    int NDDxpPixelsPerRun;                  /** < Preset value how many pixels to acquire in one run (r/w) mapping mode*/
    int NDDxpBufferOverrun;
    int NDDxpMBytesReceived;
    int NDDxpReadSpeed;
    int NDDxpIgnoreGate;
    int NDDxpSyncCount;
    int NDDxpInputLogicPolarity;

    /* Internal asyn driver parameters */
    int NDDxpErased;               /** < Erased flag. (0=not erased; 1=erased) */
    int NDDxpAcquiring;            /** < Internal acquiring flag, not exposed via drvUser */
    int NDDxpPixelCounter;         /** < Count how many pixels have been acquired (read) mapping mode */
    int NDDxpBufferCounter;        /** < Count how many buffers have been collected (read) mapping mode */
    int NDDxpPollTime;             /** < Status/data polling time in seconds */
    int NDDxpForceRead;            /** < Force reading MCA spectra - used for mcaData when addr=ALL */
    int NDDxpXMAPApply;            /** < Force apply on xMAP */
    int NDDxpXMAPAutoApply;        /** < Auto-apply on xMAP */

    /* Diagnostic trace parameters */
    int NDDxpTraceMode;            /** < Select what type of trace to do: ADC, baseline hist, .. etc. */
    int NDDxpTraceTime;            /** < Set the trace sample time in us. */
    int NDDxpTrace;                /** < The trace array data (read) */
    int NDDxpBaselineHistogram;    /** < The baseline histogram array data (read) */

    /* Runtime statistics */
    int NDDxpTriggerLiveTime;           /** < live time in seconds (double) */
    int NDDxpTriggers;                  /** < number of triggers received (double) */
    int NDDxpEvents;                    /** < total number of events registered (double) */
    int NDDxpInputCountRate;            /** < input count rate in Hz (double) */
    int NDDxpOutputCountRate;           /** < output count rate in Hz (double) */

    /* High-level DXP parameters */
    int NDDxpPeakingTime;
    int NDDxpDynamicRange;
    int NDDxpTriggerThreshold;
    int NDDxpBaselineThreshold;
    int NDDxpEnergyThreshold;
    int NDDxpCalibrationEnergy;
    int NDDxpADCPercentRule;
    int NDDxpMCABinWidth;
    int NDDxpMaxEnergy;            /** < Maximum energy */
    int NDDxpPreampGain;
    int NDDxpNumMCAChannels;
    int NDDxpDetectorPolarity;
    int NDDxpResetDelay;
    int NDDxpDecayTime;
    int NDDxpGapTime;
    int NDDxpTriggerPeakingTime;
    int NDDxpTriggerGapTime;
    int NDDxpBaselineAverage;
    int NDDxpBaselineCut;
    int NDDxpEnableBaselineCut;
    int NDDxpMaxWidth;
    int NDDxpPresetMode;
    int NDDxpPresetEvents;
    int NDDxpPresetTriggers;

    /* SCA parameters */
    int NDDxpNumSCAs;
    int NDDxpSCALow[DXP_MAX_SCAS];
    int NDDxpSCAHigh[DXP_MAX_SCAS];
    int NDDxpSCACounts[DXP_MAX_SCAS];

    /* INI file parameters */
    int NDDxpSaveSystemFile;
    int NDDxpSaveSystem;

    /* Commands from MCA interface */
    int mcaData;                   /* int32Array, write/read */
    int mcaStartAcquire;           /* int32, write */
    int mcaStopAcquire;            /* int32, write */
    int mcaErase;                  /* int32, write */
    int mcaReadStatus;             /* int32, write */
    int mcaChannelAdvanceInternal; /* int32, write */
    int mcaChannelAdvanceExternal; /* int32, write */
    int mcaNumChannels;            /* int32, write */
    int mcaModePHA;                /* int32, write */
    int mcaModeMCS;                /* int32, write */
    int mcaModeList;               /* int32, write */
    int mcaSequence;               /* int32, write */
    int mcaPrescale;               /* int32, write */
    int mcaPresetSweeps;           /* int32, write */
    int mcaPresetLowChannel;       /* int32, write */
    int mcaPresetHighChannel;      /* int32, write */
    int mcaDwellTime;              /* float64, write/read */
    int mcaPresetLiveTime;         /* float64, write */
    int mcaPresetRealTime;         /* float64, write */
    int mcaPresetCounts;           /* float64, write */
    int mcaAcquiring;              /* int32, read */
    int mcaElapsedLiveTime;        /* float64, read */
    int mcaElapsedRealTime;        /* float64, read */
    int mcaElapsedCounts;          /* float64, read */

    /* Low-level DXP parameters */
    int NDDxpNumLLParams;
    int NDDxpReadLLParams;        /** < Force read of values of low-level parameters */
    int NDDxpLLParamNames[DXP_MAX_LL_PARAMS];
    int NDDxpLLParamVals[DXP_MAX_LL_PARAMS];
    #define LAST_DXP_PARAM NDDxpLLParamVals[DXP_MAX_LL_PARAMS-1]

private:
    /* Data */
    epicsUInt32 **pMcaRaw;
    epicsUInt32 **pXmapMcaRaw;
    epicsUInt32 *pMapRaw;
    epicsFloat64 *tmpStats;

    NDDxpModel_t deviceType;
    int nCards;
    int nChannels;
    int supportsMapping;

    epicsEvent *cmdStartEvent;
    epicsEvent *cmdStopEvent;
    epicsEvent *stoppedEvent;

    epicsUInt32 *currentBuf;
    int traceLength;
    int baselineLength;
    epicsInt32 *traceBuffer;
    epicsInt32 *baselineBuffer;
    
    /* These values are needed temporarily until Handel adds "module_statistics" for Saturn and DXP2X */
    moduleStatistics moduleStats[XMAP_NCHANS_MODULE];
    double clockSpeed;
    int triggerOffsets[2], eventOffsets[2];
    int realTimeOffsets[3], triggerLiveTimeOffsets[3];
    int overFlowOffsets[2], underFlowOffsets[2];

    char polling;

};

/** Number of asyn parameters (asyn commands) this driver supports. This algorithm does NOT include the
  * low-level parameters whose number we can only determine at run-time.
  * That value is passed to the constructor. */
#define NUM_DXP_PARAMS (&LAST_DXP_PARAM - &FIRST_DXP_PARAM + 1)

static void c_shutdown(void* arg)
{
    NDDxp *pNDDxp = (NDDxp*)arg;
    pNDDxp->shutdown();
    free(pNDDxp);
}

static void acquisitionTaskC(void *drvPvt)
{
    NDDxp *pNDDxp = (NDDxp *)drvPvt;
    pNDDxp->acquisitionTask();
}

static int paramCompare(const void *p1, const void *p2)
{
    int ip1 = *(int *)p1;
    int ip2 = *(int *)p2;
    
    return(strcmp(LLParamNames[ip1], LLParamNames[ip2]));
}

static int indexCompare(const void *p1, const void *p2)
{
    int ip1 = *(int *)p1;
    int ip2 = *(int *)p2;
    
    return(LLParamSort[ip2] - LLParamSort[ip1]);
}

extern "C" int NDDxp_config(const char *portName, int nChannels,
                            int maxBuffers, size_t maxMemory)
{
    NDDxp *dummy = new NDDxp(portName, nChannels, maxBuffers, maxMemory);
    dummy = NULL;
    return 0;
}

/* Note: we use nChannels+1 for maxAddr because the last address is used for "all" channels" */
NDDxp::NDDxp(const char *portName, int nChannels, int maxBuffers, size_t maxMemory)
    : asynNDArrayDriver(portName, nChannels + 1, NUM_DXP_PARAMS, maxBuffers, maxMemory,
            asynFloat64Mask | asynInt32ArrayMask | asynGenericPointerMask | asynOctetMask | asynInt32Mask | asynDrvUserMask,
            asynFloat64Mask | asynInt32ArrayMask | asynGenericPointerMask | asynOctetMask | asynInt32Mask,
            ASYN_MULTIDEVICE | ASYN_CANBLOCK, 1, 0, 0)
{
    int status = asynSuccess;
    int i, ch;
    int sca;
    char tmpStr[MAXSYMBOL_LEN + 10];
    int xiastatus = 0;
    unsigned short runTasks;
    const char *functionName = "NDDxp";

    this->nChannels = nChannels;

    /* Mapping mode parameters */
    createParam(NDDxpCollectModeString,            asynParamInt32,   &NDDxpCollectMode);
    createParam(NDDxpPixelAdvanceModeString,       asynParamInt32,   &NDDxpPixelAdvanceMode);
    createParam(NDDxpCurrentPixelString,           asynParamInt32,   &NDDxpCurrentPixel);
    createParam(NDDxpNextPixelString,              asynParamInt32,   &NDDxpNextPixel);
    createParam(NDDxpPixelsPerBufferString,        asynParamInt32,   &NDDxpPixelsPerBuffer);
    createParam(NDDxpAutoPixelsPerBufferString,    asynParamInt32,   &NDDxpAutoPixelsPerBuffer);
    createParam(NDDxpPixelsPerRunString,           asynParamInt32,   &NDDxpPixelsPerRun);
    createParam(NDDxpBufferOverrunString,          asynParamInt32,   &NDDxpBufferOverrun);
    createParam(NDDxpMBytesReceivedString,         asynParamFloat64, &NDDxpMBytesReceived);
    createParam(NDDxpReadSpeedString,              asynParamFloat64, &NDDxpReadSpeed);
    createParam(NDDxpIgnoreGateString,             asynParamInt32,   &NDDxpIgnoreGate);
    createParam(NDDxpSyncCountString,              asynParamInt32,   &NDDxpSyncCount);
    createParam(NDDxpInputLogicPolarityString,     asynParamInt32,   &NDDxpInputLogicPolarity);


    /* Internal asyn driver parameters */
    createParam(NDDxpErasedString,                 asynParamInt32,   &NDDxpErased);
    createParam(NDDxpAcquiringString,              asynParamInt32,   &NDDxpAcquiring);
    createParam(NDDxpPixelCounterString,           asynParamInt32,   &NDDxpPixelCounter);
    createParam(NDDxpBufferCounterString,          asynParamInt32,   &NDDxpBufferCounter);
    createParam(NDDxpPollTimeString,               asynParamFloat64, &NDDxpPollTime);
    createParam(NDDxpForceReadString,              asynParamInt32,   &NDDxpForceRead);
    createParam(NDDxpXMAPApplyString,              asynParamInt32,   &NDDxpXMAPApply);
    createParam(NDDxpXMAPAutoApplyString,          asynParamInt32,   &NDDxpXMAPAutoApply);

    /* Diagnostic trace parameters */
    createParam(NDDxpTraceModeString,              asynParamInt32,   &NDDxpTraceMode);
    createParam(NDDxpTraceTimeString,              asynParamFloat64, &NDDxpTraceTime);
    createParam(NDDxpTraceString,                  asynParamInt32Array, &NDDxpTrace);
    createParam(NDDxpBaselineHistogramString,      asynParamInt32Array, &NDDxpBaselineHistogram);

    /* Runtime statistics */
    createParam(NDDxpTriggerLiveTimeString,        asynParamFloat64, &NDDxpTriggerLiveTime);
    createParam(NDDxpTriggersString,               asynParamInt32,   &NDDxpTriggers);
    createParam(NDDxpEventsString,                 asynParamInt32,   &NDDxpEvents);
    createParam(NDDxpInputCountRateString,         asynParamFloat64, &NDDxpInputCountRate);
    createParam(NDDxpOutputCountRateString,        asynParamFloat64, &NDDxpOutputCountRate);

    /* High-level DXP parameters */
    createParam(NDDxpPeakingTimeString,            asynParamFloat64, &NDDxpPeakingTime);
    createParam(NDDxpDynamicRangeString,           asynParamFloat64, &NDDxpDynamicRange);
    createParam(NDDxpTriggerThresholdString,       asynParamFloat64, &NDDxpTriggerThreshold);
    createParam(NDDxpBaselineThresholdString,      asynParamFloat64, &NDDxpBaselineThreshold);
    createParam(NDDxpEnergyThresholdString,        asynParamFloat64, &NDDxpEnergyThreshold);
    createParam(NDDxpCalibrationEnergyString,      asynParamFloat64, &NDDxpCalibrationEnergy);
    createParam(NDDxpADCPercentRuleString,         asynParamFloat64, &NDDxpADCPercentRule);
    createParam(NDDxpMCABinWidthString,            asynParamFloat64, &NDDxpMCABinWidth);
    createParam(NDDxpMaxEnergyString,              asynParamFloat64, &NDDxpMaxEnergy);
    createParam(NDDxpPreampGainString,             asynParamFloat64, &NDDxpPreampGain);
    createParam(NDDxpNumMCAChannelsString,         asynParamInt32,   &NDDxpNumMCAChannels);
    createParam(NDDxpDetectorPolarityString,       asynParamInt32,   &NDDxpDetectorPolarity);
    createParam(NDDxpResetDelayString,             asynParamFloat64, &NDDxpResetDelay);
    createParam(NDDxpDecayTimeString,              asynParamFloat64, &NDDxpDecayTime);
    createParam(NDDxpGapTimeString,                asynParamFloat64, &NDDxpGapTime);
    createParam(NDDxpTriggerPeakingTimeString,     asynParamFloat64, &NDDxpTriggerPeakingTime);
    createParam(NDDxpTriggerGapTimeString,         asynParamFloat64, &NDDxpTriggerGapTime);
    createParam(NDDxpBaselineAverageString,        asynParamInt32,   &NDDxpBaselineAverage);
    createParam(NDDxpBaselineCutString,            asynParamFloat64, &NDDxpBaselineCut);
    createParam(NDDxpEnableBaselineCutString,      asynParamInt32,   &NDDxpEnableBaselineCut);
    createParam(NDDxpMaxWidthString,               asynParamFloat64, &NDDxpMaxWidth);
    createParam(NDDxpPresetModeString,             asynParamInt32,   &NDDxpPresetMode);
    createParam(NDDxpPresetEventsString,           asynParamInt32,   &NDDxpPresetEvents);
    createParam(NDDxpPresetTriggersString,         asynParamInt32,   &NDDxpPresetTriggers);

    /* SCA parameters */
    createParam(NDDxpNumSCAsString,                asynParamInt32,   &NDDxpNumSCAs);
    for (sca=0; sca<DXP_MAX_SCAS; sca++) {
        /* Create SCA name strings that Handel uses */
        sprintf(SCA_NameLow[sca],  "sca%d_lo", sca);
        sprintf(SCA_NameHigh[sca], "sca%d_hi", sca);
        /* Create asyn parameters for SCAs */
        sprintf(tmpStr, "DxpSCA%dLow", sca);
        createParam(tmpStr,                        asynParamInt32,   &NDDxpSCALow[sca]);
        sprintf(tmpStr, "DxpSCA%dHigh", sca);
        createParam(tmpStr,                        asynParamInt32,   &NDDxpSCAHigh[sca]);
        sprintf(tmpStr, "DxpSCA%dCounts", sca);
        createParam(tmpStr,                        asynParamInt32,   &NDDxpSCACounts[sca]);
    }

    /* INI file parameters */
    createParam(NDDxpSaveSystemFileString,         asynParamOctet,   &NDDxpSaveSystemFile);
    createParam(NDDxpSaveSystemString,             asynParamInt32,   &NDDxpSaveSystem);

    /* Commands from MCA interface */
    createParam(mcaDataString,                     asynParamInt32Array, &mcaData);
    createParam(mcaStartAcquireString,             asynParamInt32,   &mcaStartAcquire);
    createParam(mcaStopAcquireString,              asynParamInt32,   &mcaStopAcquire);
    createParam(mcaEraseString,                    asynParamInt32,   &mcaErase);
    createParam(mcaReadStatusString,               asynParamInt32,   &mcaReadStatus);
    createParam(mcaChannelAdvanceInternalString,   asynParamInt32,   &mcaChannelAdvanceInternal);
    createParam(mcaChannelAdvanceExternalString,   asynParamInt32,   &mcaChannelAdvanceExternal);
    createParam(mcaNumChannelsString,              asynParamInt32,   &mcaNumChannels);
    createParam(mcaModePHAString,                  asynParamInt32,   &mcaModePHA);
    createParam(mcaModeMCSString,                  asynParamInt32,   &mcaModeMCS);
    createParam(mcaModeListString,                 asynParamInt32,   &mcaModeList);
    createParam(mcaSequenceString,                 asynParamInt32,   &mcaSequence);
    createParam(mcaPrescaleString,                 asynParamInt32,   &mcaPrescale);
    createParam(mcaPresetSweepsString,             asynParamInt32,   &mcaPresetSweeps);
    createParam(mcaPresetLowChannelString,         asynParamInt32,   &mcaPresetLowChannel);
    createParam(mcaPresetHighChannelString,        asynParamInt32,   &mcaPresetHighChannel);
    createParam(mcaDwellTimeString,                asynParamFloat64, &mcaDwellTime);
    createParam(mcaPresetLiveTimeString,           asynParamFloat64, &mcaPresetLiveTime);
    createParam(mcaPresetRealTimeString,           asynParamFloat64, &mcaPresetRealTime);
    createParam(mcaPresetCountsString,             asynParamInt32,   &mcaPresetCounts);
    createParam(mcaAcquiringString,                asynParamInt32,   &mcaAcquiring);
    createParam(mcaElapsedLiveTimeString,          asynParamFloat64, &mcaElapsedLiveTime);
    createParam(mcaElapsedRealTimeString,          asynParamFloat64, &mcaElapsedRealTime);
    createParam(mcaElapsedCountsString,            asynParamFloat64, &mcaElapsedCounts);

    /* Low-level DXP parameters */
    status = xiaGetNumParams(0, &numLLParams);
    if (numLLParams > DXP_MAX_LL_PARAMS) {
        printf("Error: actual number of DXP params=%d, more than allowed maximum of %d\n", 
                numLLParams, DXP_MAX_LL_PARAMS);
    }
    for (i=0; i < numLLParams; i++) {
        LLParamNames[i] = (char *) malloc(MAXSYMBOL_LEN * sizeof(char *));
        status = xiaGetParamName(0, i, LLParamNames[i]);
        LLParamSort[i] = i;
    }
    /* Sort the parameters in alphabetical order, much nicer to display */
    qsort(LLParamSort, numLLParams, sizeof(int), paramCompare);  

    createParam(NDDxpNumLLParamsString,            asynParamInt32,   &NDDxpNumLLParams);
    createParam(NDDxpReadLLParamsString,           asynParamInt32,   &NDDxpReadLLParams);
    for (i=0; i<DXP_MAX_LL_PARAMS; i++) {
        sprintf(tmpStr, "DxpLL%dName", i);
        createParam(tmpStr,                        asynParamOctet,   &NDDxpLLParamNames[i]);
        sprintf(tmpStr, "DxpLL%dVal", i);
        createParam(tmpStr,                        asynParamInt32,   &NDDxpLLParamVals[i]);
    }
    
    for (ch=0; ch<this->nChannels; ch++) {
        setIntegerParam(ch, NDDxpNumLLParams, numLLParams);
        for (i=0; i<numLLParams; i++) {
            setStringParam(ch, NDDxpLLParamNames[i], LLParamNames[LLParamSort[i]]);
        }
        for (i=numLLParams; i<DXP_MAX_LL_PARAMS; i++) {
            setStringParam(ch, NDDxpLLParamNames[i], "Unused");
            setIntegerParam(ch, NDDxpLLParamVals[i], 0);
        }
    }

    this->deviceType = (NDDxpModel_t) this->getModuleType();
    switch (this->deviceType)
    {
    case NDDxpModelXMAP:
        this->supportsMapping = 1;
        /* TODO: this solution is a bit crude and not always correct... */
        this->nCards = ((this->nChannels-1) / XMAP_NCHANS_MODULE) + 1;
    case NDDxpModel4C2X:
        this->supportsMapping = 0;
        this->nCards = ((this->nChannels-1) / XMAP_NCHANS_MODULE) + 1;
        break;
    case NDDxpModelSaturn:
        this->supportsMapping = 0;
        this->nCards = 1;
        break;
    }
    
    /* Register the epics exit function to be called when the IOC exits... */
    xiastatus = epicsAtExit(c_shutdown, this);

    /* Set the parameters in param lib */
    status |= setIntegerParam(NDDxpCollectMode, 0);

    /* Create the start and stop events that will be used to signal our
     * acquisitionTask when to start/stop polling the HW     */
    this->cmdStartEvent = new epicsEvent();
    this->cmdStopEvent = new epicsEvent();
    this->stoppedEvent = new epicsEvent();

    /* allocate a memory pointer for each of the channels */
    this->pMcaRaw = (epicsUInt32**) calloc(this->nChannels, sizeof(epicsUInt32*));
    if (this->deviceType == NDDxpModelXMAP)
    {
        int spectSize = XMAP_MAX_MCA_BINS * sizeof(epicsUInt32);
        int xmapSize = spectSize * XMAP_NCHANS_MODULE;
        char *buff;
        this->pXmapMcaRaw = (epicsUInt32**)calloc(this->nCards, sizeof(epicsUInt32*));
        /* Allocate one big block of memory that is large enough for all channels */
        buff = (char *)malloc(this->nCards * xmapSize);
        for (i=0; i<this->nCards; i++)
            this->pXmapMcaRaw[i] = (epicsUInt32 *)(buff + i*xmapSize);
        for (i=0; i<this->nChannels; i++)
            this->pMcaRaw[i] = (epicsUInt32 *)(buff + i*spectSize);
    } else {
        /* allocate a memory area for each spectrum */
        for (ch=0; ch<this->nChannels; ch++)
            this->pMcaRaw[ch] = (epicsUInt32*)calloc(XMAP_MAX_MCA_BINS, sizeof(epicsUInt32));
    }

    this->tmpStats = (epicsFloat64*)calloc(28, sizeof(epicsFloat64));
    this->currentBuf = (epicsUInt32*)calloc(this->nChannels, sizeof(epicsUInt32));

    xiastatus = xiaGetSpecialRunData(0, "adc_trace_length",  &(this->traceLength));
    if (xiastatus != XIA_SUCCESS) printf("Error calling xiaGetSpecialRunData for adc_trace_length");

    /* Allocate a buffer for the trace data */
    this->traceBuffer = (epicsInt32 *)malloc(this->traceLength * sizeof(epicsInt32));

    xiastatus = xiaGetRunData(0, "baseline_length",  &(this->baselineLength));
    if (xiastatus != XIA_SUCCESS) printf("Error calling xiaGetRunData for baseline_length");

    /* Allocate a buffer for the baseline histogram data */
    this->baselineBuffer = (epicsInt32 *)malloc(this->baselineLength * sizeof(epicsInt32));

    /* Allocating a temporary buffer for use in mapping mode. */
    this->pMapRaw = (epicsUInt32*)malloc(XMAP_MAPBUF_READ_SIZE);

    /* On the Saturn enable special timing mode in RUNTASKS so we can use ROI pulse output
     * Hardcoding this here should be TEMPORARY until low-level parameters can be controlled by EPICS? */
    if (this->deviceType == NDDxpModelSaturn) {
        xiastatus = xiaGetParameter(0, "RUNTASKS", &runTasks);
        if (xiastatus != XIA_SUCCESS) printf("Error calling xiaGetParameter for RUNTASKS");
        /* Set bit 11 */
        runTasks |= 0x800;
        xiastatus = xiaSetParameter(0, "RUNTASKS", runTasks);
        if (xiastatus != XIA_SUCCESS) printf("Error calling xiaSetParameter for RUNTASKS");
    }

    /* Start up acquisition thread */
    setDoubleParam(NDDxpPollTime, 0.001);
    this->polling = 1;
    status = (epicsThreadCreate("acquisitionTask",
                epicsThreadPriorityMedium,
                epicsThreadGetStackSize(epicsThreadStackMedium),
                (EPICSTHREADFUNC)acquisitionTaskC, this) == NULL);
    if (status)
    {
        printf("%s:%s epicsThreadCreate failure for image task\n",
                driverName, functionName);
        return;
    }

    /* Read actual values of all parameters from Handel.  
     * Reading low-level parameters also reads high-level parameters */
    getLLDxpParams(this->pasynUserSelf, DXP_ALL);

    /* If this is an xMAP read the current mapping mode, temporarily switch to MCA mapping to
     * force mapping firmware to download, and then switch back to previous mode.  
     * This allows getDxpParams() to read the Handel values for mapping parameters.
     * We only read the values for the first module, since we force them to be the same for all
     * modules. */
    if (this->deviceType == NDDxpModelXMAP) {
        double mappingMode, prevMappingMode;
        xiastatus = xiaGetAcquisitionValues(0, "mapping_mode", &prevMappingMode);
        status = this->xia_checkError(pasynUserSelf, xiastatus, "get mapping_mode");
        mappingMode = NDDxpModeSpectraMapping;
        xiastatus = xiaSetAcquisitionValues(0, "mapping_mode", &mappingMode);
        status = this->xia_checkError(pasynUserSelf, xiastatus, "set mapping_mode");
        xiastatus = xiaBoardOperation(0, "apply", &mappingMode);
        status = this->xia_checkError(pasynUserSelf, xiastatus, "apply");
        this->getDxpParams(this->pasynUserSelf, 0);
        xiastatus = xiaSetAcquisitionValues(0, "mapping_mode", &prevMappingMode);
        status = this->xia_checkError(pasynUserSelf, xiastatus, "set mapping_mode");
        xiastatus = xiaBoardOperation(0, "apply", &mappingMode);
        status = this->xia_checkError(pasynUserSelf, xiastatus, "apply");
    }

    /* Set default values for parameters that cannot be read from Handel */
    for (i=0; i<this->nChannels; i++) {
        setIntegerParam(i, NDDxpTraceMode, NDDxpTraceADC);
        setDoubleParam(i, NDDxpTraceTime, 0.1);
    }

    /* Read the MCA and DXP parameters once */
    this->getDxpParams(this->pasynUserSelf, DXP_ALL);
    this->getAcquisitionStatus(this->pasynUserSelf, DXP_ALL);
    this->getAcquisitionStatistics(this->pasynUserSelf, DXP_ALL);
    
    // Enable array callbacks by defaults
    setIntegerParam(NDArrayCallbacks, 1);
    
    // Disable auto-apply
    setIntegerParam(NDDxpXMAPAutoApply, 0);
}

/* virtual methods to override from ADDriver */
asynStatus NDDxp::writeInt32( asynUser *pasynUser, epicsInt32 value)
{
    asynStatus status = asynSuccess;
    int function = pasynUser->reason;
    int channel, rbValue, xiastatus, chStep = 1;
    int addr, i;
    int acquiring, numChans, mode;
    const char* functionName = "writeInt32";
    int firstCh, ignored;
    char fileName[MAX_FILENAME_LEN];

    channel = this->getChannel(pasynUser, &addr);
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s::%s [%s]: function=%d value=%d addr=%d channel=%d\n",
        driverName, functionName, this->portName, function, value, addr, channel);

    /* Set the parameter and readback in the parameter library.  This may be overwritten later but that's OK */
    status = setIntegerParam(addr, function, value);

    if ((function == NDDxpCollectMode)         ||
        (function == NDDxpPixelsPerRun)        ||
        (function == NDDxpPixelsPerBuffer)     ||
        (function == NDDxpAutoPixelsPerBuffer) ||
        (function == NDDxpSyncCount)           ||
        (function == NDDxpIgnoreGate)          ||
        (function == NDDxpInputLogicPolarity)  ||
        (function == NDDxpPixelAdvanceMode)) 
    {
        status = this->configureCollectMode();
    } 
    else if (function == NDDxpNextPixel) 
    {
        if (this->deviceType == NDDxpModelXMAP) chStep = XMAP_NCHANS_MODULE;
        for (firstCh=0; firstCh<this->nChannels; firstCh+=chStep)
        {
            CALLHANDEL( xiaBoardOperation(firstCh, "mapping_pixel_next", &ignored), "mapping_pixel_next" )
        }
        setIntegerParam(addr, function, 0);
    }
    else if (function == NDDxpXMAPApply)
    {
        if (value) this->apply(DXP_ALL, 1);
    }
    else if (function == mcaErase) 
    {
        getIntegerParam(addr, mcaNumChannels, &numChans);
        getIntegerParam(addr, mcaAcquiring, &acquiring);
        if (acquiring) {
            xiaStopRun(channel);
            CALLHANDEL(xiaStartRun(channel, 0), "xiaStartRun(channel, 0)");
        } else {
            setIntegerParam(addr, NDDxpErased, 1);
            if (channel == DXP_ALL) {
                for (i=0; i<this->nChannels; i++) {
                    setIntegerParam(i, NDDxpErased, 1);
                    memset(this->pMcaRaw[i], 0, numChans * sizeof(epicsUInt32));
                }
            } else {
                memset(this->pMcaRaw[addr], 0, numChans * sizeof(epicsUInt32));
            }
            /* Need to call getAcquisitionStatistics to set elapsed values to 0 */
            this->getAcquisitionStatistics(pasynUser, addr);
        }
    } 
    else if (function == mcaStartAcquire) 
    {
        status = this->startAcquiring(pasynUser);
    } 
    else if (function == mcaStopAcquire) 
    {
        CALLHANDEL(xiaStopRun(channel), "xiaStopRun(detChan)");
    } 
    else if (function == mcaNumChannels) 
    {
        // rbValue not used here, call setIntegerParam if needed.
        status = this->setNumChannels(pasynUser, value, &rbValue);
    } 
    else if (function == mcaReadStatus) 
    {
        getIntegerParam(NDDxpCollectMode, &mode);
        asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            "%s::%s mcaReadStatus [%d] mode=%d\n", 
            driverName, functionName, function, mode);
        /* We let the polling task set the acquiring flag, so that we can be sure that
         * the statistics and data have been read when needed.  DON'T READ ACQUIRE STATUS HERE */
        getIntegerParam(addr, mcaAcquiring, &acquiring);
        if (mode == NDDxpModeMCA) {
            /* If we are acquiring then read the statistics, else we use the cached values */
            if (acquiring) status = this->getAcquisitionStatistics(pasynUser, addr);
        }
    }
    else if ((function == NDDxpPresetMode)   ||
             (function == NDDxpPresetEvents) ||
             (function == NDDxpPresetTriggers)) 
    {
        this->setPresets(pasynUser, addr);
    } 
    else if (function == NDDxpReadLLParams) 
    {
        this->getLLDxpParams(pasynUser, addr);
    } 
    else if ((function == NDDxpDetectorPolarity) ||
             (function == NDDxpEnableBaselineCut)||
             (function == NDDxpBaselineAverage)) 
    {
        this->setDxpParam(pasynUser, addr, function, (double)value);
    }
    else if ((function == NDDxpNumSCAs)    ||
             ((function >= NDDxpSCALow[0]) &&
              (function <= NDDxpSCAHigh[DXP_MAX_SCAS-1]))) 
    {
        this->setSCAs(pasynUser, addr);
    }
    else if ((function >= NDDxpLLParamVals[0]) &&
             (function <= NDDxpLLParamVals[numLLParams-1])) 
    {
        this->setLLDxpParam(pasynUser, addr, value);
    }
    else if (function == NDDxpSaveSystem) 
    {
        if (value) {
            callParamCallbacks(addr, addr);
            status = getStringParam(NDDxpSaveSystemFile, sizeof(fileName), fileName);
            if (status || (strlen(fileName) == 0)) {
                asynPrint(pasynUser, ASYN_TRACE_ERROR,
                    "%s::%s error, bad system file name, status=%d, fileName=%s\n",
                    driverName, functionName, status, fileName);
                goto done;
            }
            CALLHANDEL(xiaSaveSystem("handel_ini", fileName), "xiaSaveSystem(handel_ini, fileName)");
            /* Set the save command back to 0 */
            setIntegerParam(addr, NDDxpSaveSystem, 0);
        }
    }
    done:

    /* Call the callback */
    callParamCallbacks(addr, addr);
    return status;
}

asynStatus NDDxp::writeFloat64( asynUser *pasynUser, epicsFloat64 value)
{
    asynStatus status = asynSuccess;
    int function = pasynUser->reason;
    int addr;
    int channel;
    const char *functionName = "writeFloat64";

    channel = this->getChannel(pasynUser, &addr);
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
        "%s::%s [%s]: function=%d value=%f addr=%d channel=%d\n",
        driverName, functionName, this->portName, function, value, addr, channel);

    /* Set the parameter and readback in the parameter library.  This may be overwritten later but that's OK */
    status = setDoubleParam(addr, function, value);

    if ((function == mcaPresetRealTime) ||
        (function == mcaPresetLiveTime)) 
    {
        this->setPresets(pasynUser, addr);
    } 
    else if 
       ((function == NDDxpPeakingTime) ||
        (function == NDDxpDynamicRange) ||
        (function == NDDxpTriggerThreshold) ||
        (function == NDDxpBaselineThreshold) ||
        (function == NDDxpEnergyThreshold) ||
        (function == NDDxpCalibrationEnergy) ||
        (function == NDDxpADCPercentRule) ||
        (function == NDDxpPreampGain) ||
        (function == NDDxpDetectorPolarity) ||
        (function == NDDxpResetDelay) ||
        (function == NDDxpGapTime) ||
        (function == NDDxpTriggerPeakingTime) ||
        (function == NDDxpTriggerGapTime) ||
        (function == NDDxpBaselineCut) ||
        (function == NDDxpMaxWidth) ||
        (function == NDDxpMaxEnergy)) 
    {
        this->setDxpParam(pasynUser, addr, function, value);
    }

    /* Call the callback */
    callParamCallbacks(addr, addr);

    return status;
}

asynStatus NDDxp::readInt32Array(asynUser *pasynUser, epicsInt32 *value, size_t nElements, size_t *nIn)
{
    asynStatus status = asynSuccess;
    int function = pasynUser->reason;
    int addr;
    int channel;
    int nBins, acquiring,mode;
    int ch;
    const char *functionName = "readInt32Array";

    channel = this->getChannel(pasynUser, &addr);

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s::%s addr=%d channel=%d function=%d\n",
        driverName, functionName, addr, channel, function);
    if (function == NDDxpTrace) 
    {
        status = this->getTrace(pasynUser, channel, value, nElements, nIn);
    } 
    else if (function == NDDxpBaselineHistogram) 
    {
        status = this->getBaselineHistogram(pasynUser, channel, value, nElements, nIn);
    } 
    else if (function == mcaData) 
    {
        if (channel == DXP_ALL)
        {
            // if the MCA ALL channel is being read - force reading of all individual
            // channels using the NDDxpForceRead command.
            for (ch=0; ch<this->nChannels; ch++)
            {
                setIntegerParam(ch, NDDxpForceRead, 1);
                callParamCallbacks(ch, ch);
                setIntegerParam(ch, NDDxpForceRead, 0);
                callParamCallbacks(ch, ch);
            }
            goto done;
        }
        getIntegerParam(channel, mcaNumChannels, &nBins);
        if (nBins > (int)nElements) nBins = (int)nElements;
        getIntegerParam(channel, mcaAcquiring, &acquiring);
        asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            "%s::%s getting mcaData. ch=%d mcaNumChannels=%d mcaAcquiring=%d\n",
            driverName, functionName, channel, nBins, acquiring);
        *nIn = nBins;
        getIntegerParam(NDDxpCollectMode, &mode);

        asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            "%s::%s mode=%d acquiring=%d\n",
            driverName, functionName, mode, acquiring);
        if (acquiring)
        {
            if (mode == NDDxpModeMCA)
            {
                /* While acquiring we'll force reading the data from the HW */
                this->getMcaData(pasynUser, addr);
            } else if ((mode == NDDxpModeSpectraMapping) || (mode == NDDxpModeSCAMapping))
            {
                /*  Nothing needed here, the last data read from the mapping buffer has already been
                 *  copied to the buffer pointed to by pMcaRaw. */
            }
        }
        memcpy(value, pMcaRaw[addr], nBins * sizeof(epicsUInt32));
    } 
    else {
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                "%s::%s Function not implemented: [%d]\n",
                driverName, functionName, function);
            status = asynError;
    }
    done:
    
    return(status);
}


int NDDxp::getChannel(asynUser *pasynUser, int *addr)
{
    int channel;
    pasynManager->getAddr(pasynUser, addr);

    channel = *addr;
    switch (this->deviceType)
    {
    case NDDxpModelXMAP:
        if (*addr == this->nChannels) channel = DXP_ALL;
        break;
    default:
        break;
    }
    return channel;
}

asynStatus NDDxp::xmapGetModuleChannels(int currentChannel, int* firstChannel, int* nChannels)
{
    asynStatus status = asynSuccess;
    int modulus;
    if (this->deviceType != NDDxpModelXMAP)
    {
        *firstChannel = currentChannel;
        *nChannels = this->nChannels;
    } 
    else
    {
        if (currentChannel == DXP_ALL)
        {
            *firstChannel = 0;
            *nChannels = this->nChannels;
        } else
        {
            modulus = currentChannel % XMAP_NCHANS_MODULE;
            *firstChannel = currentChannel - modulus;
            *nChannels = XMAP_NCHANS_MODULE;
        }

    }
    return status;
}

asynStatus NDDxp::apply(int channel, int forceApply)
{
    int i;
    asynStatus status=asynSuccess;
    int xiastatus, ignore;
    int autoApply;

    if (this->deviceType != NDDxpModelXMAP) return(asynSuccess);
    getIntegerParam(NDDxpXMAPAutoApply, &autoApply);
    if (!autoApply && !forceApply) {
        return(asynSuccess);
    }

    if (channel == DXP_ALL) {
        for (i=0; i<this->nChannels; i+=XMAP_NCHANS_MODULE)
        {
            xiastatus = xiaBoardOperation(i, "apply", &ignore);
            status = this->xia_checkError(this->pasynUserSelf, xiastatus, "apply");
        }
    } else {
        xiastatus = xiaBoardOperation(channel, "apply", &ignore);
        status = this->xia_checkError(this->pasynUserSelf, xiastatus, "apply");
    }
    return(status);
}


asynStatus NDDxp::setPresets(asynUser *pasynUser, int addr)
{
    asynStatus status = asynSuccess;
    NDDxpPresetMode_t presetMode;
    double presetReal;
    double presetLive;
    int presetEvents;
    int presetTriggers;
    double presetValue;
    double presetType;
    int runActive=0;
    int channel=addr;
    int channel0;
    char presetString[40];
    const char* functionName = "setPresets";

    if (addr == this->nChannels) channel = DXP_ALL;
    if (channel == DXP_ALL) channel0 = 0; else channel0 = channel;

    getDoubleParam(addr,  mcaPresetRealTime,   &presetReal);
    getDoubleParam(addr,  mcaPresetLiveTime,   &presetLive);
    getIntegerParam(addr, NDDxpPresetEvents,   &presetEvents);
    getIntegerParam(addr, NDDxpPresetTriggers, &presetTriggers);
    getIntegerParam(addr, NDDxpPresetMode,     (int *)&presetMode);

    xiaGetRunData(channel0, "run_active", &runActive);
    xiaStopRun(channel);
    
    switch (presetMode) {
        case  NDDxpPresetModeNone:
            presetValue = 0;
            presetType = XIA_PRESET_NONE;
            strcpy(presetString, "preset_standard");
            break;

        case NDDxpPresetModeReal:
            presetValue = presetReal;
            presetType = XIA_PRESET_FIXED_REAL;
            strcpy(presetString, "preset_runtime");
            break;

        case NDDxpPresetModeLive:
            presetValue = presetLive;
            presetType = XIA_PRESET_FIXED_LIVE;
            strcpy(presetString, "preset_livetime");
            break;

        case NDDxpPresetModeEvents:
            presetValue = presetEvents;
            presetType = XIA_PRESET_FIXED_EVENTS;
            strcpy(presetString, "error");
            break;

        case NDDxpPresetModeTriggers:
            presetValue = presetTriggers;
            presetType = XIA_PRESET_FIXED_TRIGGERS;
            strcpy(presetString, "error");
            break;
    }

    if (this->deviceType == NDDxpModelXMAP) {
        xiaSetAcquisitionValues(channel, "preset_type", &presetType);
        xiaSetAcquisitionValues(channel, "preset_value", &presetValue);
        this->apply(channel);
    } else {
        if (strcmp(presetString, "error") == 0) {
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                "%s:%s: error, preset events and triggers are only available on the xMAP\n",
                driverName, functionName);
            status = asynError;
        } else {            
            xiaSetAcquisitionValues(channel, presetString, &presetValue);
        }
    }
    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
        "%s:%s: addr=%d channel=%d set presets mode=%d, value=%f\n",
        driverName, functionName, addr, channel, presetMode, presetValue);

    if (runActive) xiaStartRun(channel, 1);
    return(status);
}

asynStatus NDDxp::setDxpParam(asynUser *pasynUser, int addr, int function, double value)
{
    int channel = addr;
    int channel0;
    int runActive=0;
    double dvalue=value;
    int numMcaChannels;
    int xiastatus;
    asynStatus status=asynSuccess;
    //static const char *functionName = "setDxpParam";

    if (addr == this->nChannels) channel = DXP_ALL;
    if (channel == DXP_ALL) channel0 = 0; else channel0 = channel;

    xiaGetRunData(channel0, "run_active", &runActive);
    xiaStopRun(channel);

    if (function == NDDxpPeakingTime) {
        xiastatus = xiaSetAcquisitionValues(channel, "peaking_time", &dvalue);
        status = this->xia_checkError(pasynUser, xiastatus, "setting peaking_time");
        /* Sometimes the gap time is rejected because the peaking time has not yet been 
         * accepted, so we set it again here */
        getDoubleParam(addr, NDDxpGapTime, &dvalue);
        if (this->deviceType == NDDxpModelXMAP) {
            /* On the xMAP the parameter that can be written is minimum_gap_time */
            xiastatus = xiaSetAcquisitionValues(channel, "minimum_gap_time", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "minimum_gap_time");
        } else {
           /* On the Saturn and DXP2X it is gap_time */
            xiastatus = xiaSetAcquisitionValues(channel, "gap_time", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting gap_time");
        }
    } else if (function == NDDxpDynamicRange) {
        /* dynamic_range is only supported on the xMAP */
        if (this->deviceType == NDDxpModelXMAP) {
            /* Convert from eV to keV */
            dvalue = value * 1000.;
            xiastatus = xiaSetAcquisitionValues(channel, "dynamic_range", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting dynamic_range");
        }
    } else if (function == NDDxpTriggerThreshold) {
        /* Convert from keV to eV */
        dvalue = value * 1000.;
        xiastatus = xiaSetAcquisitionValues(channel, "trigger_threshold", &dvalue);
        status = this->xia_checkError(pasynUser, xiastatus, "setting trigger_threshold");
    } else if (function == NDDxpBaselineThreshold) {
         dvalue = value * 1000.;    /* Convert to eV */
         xiastatus = xiaSetAcquisitionValues(channel, "baseline_threshold", &dvalue);
         status = this->xia_checkError(pasynUser, xiastatus, "setting baseline_threshold");
    } else if (function == NDDxpEnergyThreshold) {
        /* Convert from keV to eV */
        dvalue = value * 1000.;
        xiastatus = xiaSetAcquisitionValues(channel, "energy_threshold", &dvalue);
        status = this->xia_checkError(pasynUser, xiastatus, "setting energy_threshold");
    } else if (function == NDDxpCalibrationEnergy) {
        /* Convert from keV to eV */
        dvalue = value * 1000.;
        xiastatus = xiaSetAcquisitionValues(channel, "calibration_energy", &dvalue);
        status = this->xia_checkError(pasynUser, xiastatus, "setting calibration_energy");
    } else if (function == NDDxpADCPercentRule) {
        xiastatus = xiaSetAcquisitionValues(channel, "adc_percent_rule", &dvalue);
        status = this->xia_checkError(pasynUser, xiastatus, "setting adc_percent_rule");
    } else if (function == NDDxpPreampGain) {
        xiastatus = xiaSetAcquisitionValues(channel, "preamp_gain", &dvalue);
        status = this->xia_checkError(pasynUser, xiastatus, "setting preamp_gain");
    } else if (function == NDDxpDetectorPolarity) {
        xiastatus = xiaSetAcquisitionValues(channel, "detector_polarity", &dvalue);
        status = this->xia_checkError(pasynUser, xiastatus, "setting detector_polarity");
    } else if (function == NDDxpResetDelay) {
        xiastatus = xiaSetAcquisitionValues(channel, "reset_delay", &dvalue);
    } else if (function == NDDxpGapTime) {
        if (this->deviceType == NDDxpModelXMAP) {
            /* On the xMAP the parameter that can be written is minimum_gap_time */
            xiastatus = xiaSetAcquisitionValues(channel, "minimum_gap_time", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "minimum_gap_time");
        } else {
           /* On the Saturn and DXP2X it is gap_time */
            xiastatus = xiaSetAcquisitionValues(channel, "gap_time", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting gap_time");
        }
    } else if (function == NDDxpTriggerPeakingTime) {
         xiastatus = xiaSetAcquisitionValues(channel, "trigger_peaking_time", &dvalue);
         status = this->xia_checkError(pasynUser, xiastatus, "setting trigger_peaking_time");
    } else if (function == NDDxpTriggerGapTime) {
        xiastatus = xiaSetAcquisitionValues(channel, "trigger_gap_time", &dvalue);
        status = this->xia_checkError(pasynUser, xiastatus, "setting trigger_gap_time");
    } else if (function == NDDxpBaselineAverage) {
        if (this->deviceType == NDDxpModelXMAP) {
            xiastatus = xiaSetAcquisitionValues(channel, "baseline_average", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting baseline_average");
        } else {
            xiastatus = xiaSetAcquisitionValues(channel, "baseline_filter_length", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting baseline_filter_length");
        }
    } else if (function == NDDxpMaxWidth) {
        xiastatus = xiaSetAcquisitionValues(channel, "maxwidth", &dvalue);
        status = this->xia_checkError(pasynUser, xiastatus, "setting maxwidth");
    } else if (function == NDDxpBaselineCut) {
        /* The xMAP does not support this yet */
        if (this->deviceType != NDDxpModelXMAP) {
            xiastatus = xiaSetAcquisitionValues(channel, "baseline_cut", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting baseline_cut");
        }
    } else if (function == NDDxpEnableBaselineCut) {
        /* The xMAP does not support this yet */
        if (this->deviceType != NDDxpModelXMAP) {
            xiastatus = xiaSetAcquisitionValues(channel, "enable_baseline_cut", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting enable_baseline_cut");
        }
    } else if (function == NDDxpMaxEnergy) {
        getIntegerParam(addr, mcaNumChannels, &numMcaChannels);
        if (numMcaChannels <= 0.)
            numMcaChannels = 2048;
        /* Set the bin width in eV */
        dvalue = value * 1000. / numMcaChannels;
        xiastatus = xiaSetAcquisitionValues(channel, "mca_bin_width", &dvalue);
        status = this->xia_checkError(pasynUser, xiastatus, "setting mca_bin_width");
        /* We always make the calibration energy be 50% of MaxEnergy */
        dvalue = value * 1000. / 2.;
        xiastatus = xiaSetAcquisitionValues(channel, "calibration_energy", &dvalue);
        status = this->xia_checkError(pasynUser, xiastatus, "setting calibration_energy");
        /* Must re-apply the ADC percent rule when changing calibration energy */
        getDoubleParam(addr, NDDxpADCPercentRule, &dvalue);
        xiastatus = xiaSetAcquisitionValues(channel, "adc_percent_rule", &dvalue);
        status = this->xia_checkError(pasynUser, xiastatus, "setting adc_percent_rule");
    }
    this->apply(channel);
    this->getDxpParams(pasynUser, addr);
    if (runActive) xiaStartRun(channel, 1);
    return asynSuccess;
}

asynStatus NDDxp::setSCAs(asynUser *pasynUser, int addr)
{
    int channel = addr;
    int channel0;
    int runActive=0;
    int xiastatus;
    int i;
    int numSCAs;
    int low, high;
    double dTmp;
    asynStatus status=asynSuccess;
    //static const char *functionName = "setSCAs";

    if (addr == this->nChannels) channel = DXP_ALL;
    if (channel == DXP_ALL) channel0 = 0; else channel0 = channel;
    xiaGetRunData(channel0, "run_active", &runActive);
    xiaStopRun(channel);

    getIntegerParam(addr, NDDxpNumSCAs, &numSCAs);
    dTmp = numSCAs;
    CALLHANDEL(xiaSetAcquisitionValues(channel, "number_of_scas", &dTmp), "number_of_scas");
    for (i=0; i<numSCAs; i++) {
        getIntegerParam(addr, NDDxpSCALow[i], &low);
        if (low < 0) {
            low = 0;
            setIntegerParam(addr, NDDxpSCALow[i], low);
        }
        getIntegerParam(addr, NDDxpSCAHigh[i], &high);
        if (high < 0) {
            high = 0;
            setIntegerParam(addr, NDDxpSCAHigh[i], high);
        }
        if (high < low) {
            high = low;
            setIntegerParam(addr, NDDxpSCAHigh[i], high);
        }
        dTmp = (double) low;
        CALLHANDEL(xiaSetAcquisitionValues(channel, SCA_NameLow[i], &dTmp), SCA_NameLow[i]);
        dTmp = (double) high;
        CALLHANDEL(xiaSetAcquisitionValues(channel, SCA_NameHigh[i], &dTmp), SCA_NameHigh[i]);
    }
    this->apply(channel);
    getSCAs(pasynUser, addr);
    if (runActive) xiaStartRun(channel, 1);
    return(status);
}

asynStatus NDDxp::getSCAs(asynUser *pasynUser, int addr)
{
    int channel = addr;
    int xiastatus;
    int i;
    int numSCAs;
    int low, high;
    double dTmp;
    asynStatus status=asynSuccess;
    //static const char *functionName = "getSCAs";

    if (addr == this->nChannels) channel = DXP_ALL;

    CALLHANDEL(xiaGetAcquisitionValues(channel, "number_of_scas", &dTmp), "number_of_scas");
    numSCAs = (int) dTmp;
    setIntegerParam(addr, NDDxpNumSCAs, numSCAs);
    for (i=0; i<numSCAs; i++) {
        CALLHANDEL(xiaGetAcquisitionValues(channel, SCA_NameLow[i], &dTmp), SCA_NameLow[i]);
        low = (int) dTmp;
        setIntegerParam(addr, NDDxpSCALow[i], low);
        CALLHANDEL(xiaGetAcquisitionValues(channel, SCA_NameHigh[i], &dTmp), SCA_NameHigh[i]);
        high = (int) dTmp;
        setIntegerParam(addr, NDDxpSCAHigh[i], high);
    }
    return(status);
}

asynStatus NDDxp::getSCAData(asynUser *pasynUser, int addr)
{
    int channel = addr;
    asynStatus status=asynSuccess;
    int xiastatus;
    int i;
    int scaCounts[DXP_MAX_SCAS];
    int numSCAs;
    //const char *functionName = "getSCAData";
    
    if (addr == this->nChannels) channel = DXP_ALL;
    if (channel == DXP_ALL) {  /* All channels */
        for (i=0; i<this->nChannels; i++) {
            /* Call ourselves recursively but with a specific channel */
            this->getSCAData(pasynUser, i);
        }
    } else {
        /* The SCA data on the xMAP is double, it is long on other products */
        getIntegerParam(addr, NDDxpNumSCAs, &numSCAs);
        if (this->deviceType == NDDxpModelXMAP) {
            double dTmp[DXP_MAX_SCAS];
            CALLHANDEL(xiaGetRunData(channel, "sca", dTmp), "sca");
            for (i=0; i<numSCAs; i++)
            {
                scaCounts[i] = (int) dTmp[i];
            }
        } else {
            CALLHANDEL(xiaGetRunData(channel, "sca", scaCounts), "sca");
        }
        for (i=0; i<numSCAs; i++) 
        {
            setIntegerParam(addr, NDDxpSCACounts[i], scaCounts[i]);
        }
    }
    return(status);
}


asynStatus NDDxp::setNumChannels(asynUser* pasynUser, epicsInt32 value, epicsInt32 *rbValue)
{
    asynStatus status = asynSuccess;
    int xiastatus;
    int i;
    int modulus, realnbins, mode=0;
    double dblNbins;
    static const char* functionName = "setNumChannels";

    asynPrint(pasynUser, ASYN_TRACE_FLOW,
        "%s::%s new number of bins: %d\n", 
        driverName, functionName, value);

    if (value > XMAP_MAX_MCA_BINS || value < XMAP_MCA_BIN_RES)
    {
        status = asynError;
        return status;
    }

    /* TODO: check here whether the xmap is acquiring (if so, break out...)  */

    /* MCA number of bins on the xmap has a resolution of XMAP_MCA_BIN_RES (256)
     * bins per channel. Since the user can enter any value we must cap this to
     * the closest multiple of 256 */
    modulus = value % XMAP_MCA_BIN_RES;
    if (modulus == 0) realnbins = value;
    else if (modulus < XMAP_MCA_BIN_RES/2) realnbins = value - modulus;
    else realnbins = value + (XMAP_MCA_BIN_RES - modulus);
    *rbValue = realnbins;
    dblNbins = (epicsFloat64)*rbValue;

    for (i=0; i<this->nChannels; i++)
    {
        asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            "xiaSetAcquisitinValues ch=%d nbins=%.1f\n", 
            i, dblNbins);
        xiastatus = xiaSetAcquisitionValues(i, "number_mca_channels", &dblNbins);
        status = this->xia_checkError(pasynUser, xiastatus, "number_mca_channels");
        if (status == asynError)
            {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s [%s] can not set nbins=%u (%.3f) ch=%u\n",
                driverName, functionName, this->portName, *rbValue, dblNbins, i);
            return status;
            }
        setIntegerParam(i, mcaNumChannels, *rbValue);
        callParamCallbacks(i,i);
    }

    /* If in mapping mode we need to modify the Y size as well in order to fit in the 2MB buffer!
     * We also need to read out the new lenght of the mapping buffer because that can possibly change... */
    getIntegerParam(NDDxpCollectMode, &mode);
    if (mode != NDDxpModeMCA) configureCollectMode();
    this->apply(DXP_ALL);

    return status;
}


asynStatus NDDxp::configureCollectMode()
{
    asynStatus status = asynSuccess;
    NDDxpCollectMode_t collectMode;
    int xiastatus, acquiring;
    int i;
    int firstCh, chStep = 1, bufLen;
    double dTmp;
    int pixelsPerBuffer;
    int autoPixelsPerBuffer;
    int pixelsPerRun;
    int syncCount;
    int ignoreGate;
    int inputLogicPolarity;
    NDDxpPixelAdvanceMode_t pixelAdvanceMode;
    const char* functionName = "configureCollectMode";
    
    getIntegerParam(NDDxpCollectMode, (int *)&collectMode);

    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
        "%s::%s Changing collectMode to %d\n",
        driverName, functionName, collectMode);

    if (collectMode < NDDxpModeMCA || collectMode > NDDxpModeSCAMapping) return asynError;
    getIntegerParam(mcaAcquiring, &acquiring);
    if (acquiring) return asynError;

    dTmp = (double)collectMode;
    xiastatus = xiaSetAcquisitionValues(DXP_ALL, "mapping_mode", &dTmp);
    status = this->xia_checkError(pasynUserSelf, xiastatus, "mapping_mode");
    if (status == asynError) return status;

    switch(collectMode)
    {
    case NDDxpModeMCA:
        getIntegerParam(mcaNumChannels, (int*)&bufLen);
        setIntegerParam(NDDataType, NDUInt32);
        for (i=0; i<this->nChannels; i++)
        {
            /* Clear the normal mapping mode status settings */
            setIntegerParam(i, NDDxpEvents, 0);
            setDoubleParam(i, NDDxpInputCountRate, 0);
            setDoubleParam(i, NDDxpOutputCountRate, 0);

            /* Set the new ArraySize down to just one buffer length */
            setIntegerParam(i, NDArraySize, (int)bufLen);
            callParamCallbacks(i,i);
        }
        break;
    case NDDxpModeSpectraMapping:
    case NDDxpModeSCAMapping:
        getIntegerParam(NDDxpPixelAdvanceMode, (int *)&pixelAdvanceMode);
        getIntegerParam(NDDxpPixelsPerRun, &pixelsPerRun);
        getIntegerParam(NDDxpPixelsPerBuffer, &pixelsPerBuffer);
        getIntegerParam(NDDxpAutoPixelsPerBuffer, &autoPixelsPerBuffer);
        if (autoPixelsPerBuffer) pixelsPerBuffer = -1;  /* Handel will compute maximum */
        getIntegerParam(NDDxpSyncCount, &syncCount);
        if (syncCount < 1) syncCount = 1;
        getIntegerParam(NDDxpIgnoreGate, &ignoreGate);
        getIntegerParam(NDDxpInputLogicPolarity, &inputLogicPolarity);
        setIntegerParam(NDDataType, NDUInt16);
            
        if (this->deviceType == NDDxpModelXMAP) chStep = XMAP_NCHANS_MODULE;
        for (firstCh=0; firstCh<this->nChannels; firstCh+=chStep)
        {
            dTmp = XIA_MAPPING_CTL_SYNC;
            if (pixelAdvanceMode == NDDxpPixelAdvanceGate) dTmp = XIA_MAPPING_CTL_GATE;
            xiastatus = xiaSetAcquisitionValues(firstCh, "pixel_advance_mode", &dTmp);
            status = this->xia_checkError(pasynUserSelf, xiastatus, "pixel_advance_mode");

            dTmp = pixelsPerRun;
            xiastatus = xiaSetAcquisitionValues(firstCh, "num_map_pixels", &dTmp);
            status = this->xia_checkError(pasynUserSelf, xiastatus, "num_map_pixels");

            dTmp = pixelsPerBuffer;
            xiastatus = xiaSetAcquisitionValues(firstCh, "num_map_pixels_per_buffer", &dTmp);
            status = this->xia_checkError(pasynUserSelf, xiastatus, "num_map_pixels_per_buffer");

            /* The xMAP actually divides the sync input by N+1, e.g. sync_count=0 does no division
             * of sync clock.  We subtract 1 so user-units are intuitive. */
            dTmp = syncCount-1;
            xiastatus = xiaSetAcquisitionValues(firstCh, "sync_count", &dTmp);
            status = this->xia_checkError(pasynUserSelf, xiastatus, "sync_count");

            dTmp = ignoreGate;
            xiastatus = xiaSetAcquisitionValues(firstCh, "gate_ignore", &dTmp);
            status = this->xia_checkError(pasynUserSelf, xiastatus, "gate_ignore");

            dTmp = inputLogicPolarity;
            xiastatus = xiaSetAcquisitionValues(firstCh, "input_logic_polarity", &dTmp);
            status = this->xia_checkError(pasynUserSelf, xiastatus, "input_logic_polarity");

            /* Apply the values */
            this->apply(firstCh);

            /* Clear the normal mapping mode status settings */
            for (i=0; i<XMAP_NCHANS_MODULE; i++)
            {
                setIntegerParam(firstCh+i, NDDxpTriggers, 0);
                setDoubleParam(firstCh+i, mcaElapsedRealTime, 0);
                setDoubleParam(firstCh+i, NDDxpTriggerLiveTime, 0);
                setDoubleParam(firstCh+i, mcaElapsedLiveTime, 0);
                callParamCallbacks(firstCh+i, firstCh+i);
            }
            /* Read back the actual settings */
            getDxpParams(this->pasynUserSelf, firstCh);
        }
        break;
    }

    return status;
}


asynStatus NDDxp::getAcquisitionStatus(asynUser *pasynUser, int addr)
{
    int acquiring=0, run_active;
    int ivalue;
    int channel=addr;
    asynStatus status;
    int xiastatus;
    int i, chStep = 1;
    //const char *functionName = "getAcquisitionStatus";
    
    /* Note: we use the internal parameter NDDxpAcquiring rather than mcaAcquiring here
     * because we need to do callbacks in acquisitionTask() on all other parameters before
     * we do callbacks on mcaAcquiring, and callParamCallbacks does not allow control over the order. */

    if (addr == this->nChannels) channel = DXP_ALL;
    else if (addr == DXP_ALL) addr = this->nChannels;
    //asynPrint(pasynUser, ASYN_TRACE_FLOW, 
    //    "%s::%s addr=%d channel=%d\n", 
    //    driverName, functionName, addr, channel);
    if (channel == DXP_ALL) { /* All channels */
        //if (this->deviceType == NDDxpModelXMAP) chStep = XMAP_NCHANS_MODULE;
        for (i=0; i<this->nChannels; i+=chStep) {
            /* Call ourselves recursively but with a specific channel */
            this->getAcquisitionStatus(pasynUser, i);
            getIntegerParam(i, NDDxpAcquiring, &ivalue);
            acquiring = MAX(acquiring, ivalue);
        }
        setIntegerParam(addr, NDDxpAcquiring, acquiring);
    } else {
        /* Get the run time status from the handel library - informs whether the
         * HW is acquiring or not.        */
        CALLHANDEL( xiaGetRunData(channel, "run_active", &run_active), "xiaGetRunData (run_active)" )
        /* If Handel thinks the run is active, but the hardware does not, then
         * stop the run */
        if (run_active == XIA_RUN_HANDEL)
            CALLHANDEL( xiaStopRun(channel), "xiaStopRun")
        /* Get the acquiring state from the XIA hardware */
        acquiring = (run_active & XIA_RUN_HARDWARE);
        setIntegerParam(addr, NDDxpAcquiring, acquiring);
    }
    //asynPrint(pasynUser, ASYN_TRACE_FLOW,
    //    "%s::%s addr=%d channel=%d: acquiring=%d\n",
    //    driverName, functionName, addr, channel, acquiring);
    return(asynSuccess);
}

int NDDxp::lookupParam(const char *paramName) 
{
    int i;
    for (i=0; i<numLLParams; i++) {
        if (strcmp(LLParamNames[i], paramName) == 0) {
            return(i);
        }
    }
    return(-1);
}

static double dxp_to_double(unsigned short *values, int numOffsets, int *offsets)
{
  double d;
  
  if (numOffsets == 3) { 
    d = values[offsets[2]] * 65536. * 65536. +
        values[offsets[0]] * 65536. + 
        values[offsets[1]];
  } else {
    d = values[offsets[0]] * 65536 + 
        values[offsets[1]];
  }
  return d;
}

asynStatus NDDxp::getModuleStatistics(asynUser *pasynUser, int addr, moduleStatistics *stats)
{
    /* This function returns the module statistics with a single block read.
     * It is more than 30 times faster on the USB Saturn than reading individual
     * parameters.  This is a temporary fix until Handel adds a "module_statistics"
     * acquisition parameter on the Saturn and DXP2X */
     static int firstTime = 1;
     int i;
     double totalEvents, underFlows, overFlows;
     int status;
     
     if (this->deviceType == NDDxpModelXMAP) {
        status = xiaGetRunData(addr, "module_statistics", (double *)stats);
        /* It appears that the xMAP sometimes returns 0 energy live time when it should not.
         * Fix this here */
        for (i=0; i<XMAP_NCHANS_MODULE; i++) {
            if (stats[i].energyLiveTime == 0.) {
                if ((stats[i].triggers > 0.) && (stats[i].events > 0)) 
                    stats[i].energyLiveTime = stats[i].triggerLiveTime * stats[i].events / stats[i].triggers;
                else
                    stats[i].energyLiveTime = stats[i].triggerLiveTime;
            }
        }
        return((asynStatus)status);
     }
     
     if (firstTime) {
        firstTime = 0;
        status = xiaGetParamData(addr, "values", LLParamValues);
        /* Determine the speed of the realtime and livetime clocks.
         * It is 16 times less than the system clock speed. */
        i = lookupParam("SYSMICROSEC");
        clockSpeed = (LLParamValues[i] * 1e6) / 16.;
        realTimeOffsets[0] = lookupParam("REALTIME0");
        realTimeOffsets[1] = lookupParam("REALTIME1");
        realTimeOffsets[2] = lookupParam("REALTIME2");
        triggerLiveTimeOffsets[0] = lookupParam("LIVETIME0");
        triggerLiveTimeOffsets[1] = lookupParam("LIVETIME1");
        triggerLiveTimeOffsets[2] = lookupParam("LIVETIME2");
        eventOffsets[0] = lookupParam("EVTSINRUN0");
        eventOffsets[1] = lookupParam("EVTSINRUN1");
        underFlowOffsets[0] = lookupParam("UNDRFLOWS0");
        underFlowOffsets[1] = lookupParam("UNDRFLOWS1");
        overFlowOffsets[0] = lookupParam("OVERFLOWS0");
        overFlowOffsets[1] = lookupParam("OVERFLOWS1");
        triggerOffsets[0] = lookupParam("FASTPEAKS0");
        triggerOffsets[1] = lookupParam("FASTPEAKS1");
    }
    status = xiaGetParamData(addr, "values", LLParamValues);
    stats->triggers        = dxp_to_double(LLParamValues, 2, triggerOffsets);
    stats->events          = dxp_to_double(LLParamValues, 2, eventOffsets);
    underFlows             = dxp_to_double(LLParamValues, 2, underFlowOffsets);
    overFlows              = dxp_to_double(LLParamValues, 2, overFlowOffsets);
    totalEvents = stats->events + underFlows + overFlows;
    stats->realTime        = dxp_to_double(LLParamValues, 3, realTimeOffsets) / clockSpeed;
    stats->triggerLiveTime = dxp_to_double(LLParamValues, 3, triggerLiveTimeOffsets) / clockSpeed;
    if (stats->triggers > 0.) 
        stats->energyLiveTime = stats->triggerLiveTime * totalEvents / stats->triggers;
    else
        stats->energyLiveTime = stats->triggerLiveTime;
    if (stats->triggerLiveTime > 0.)
        stats->icr = stats->triggers / stats->triggerLiveTime;
    else
        stats->icr = 0.;
    if (stats->realTime > 0.)
        stats->ocr = totalEvents / stats->realTime;
    else
        stats->ocr = 0.;
    return((asynStatus)status);
}     
     

asynStatus NDDxp::getAcquisitionStatistics(asynUser *pasynUser, int addr)
{
    double dvalue, triggerLiveTime=0, energyLiveTime=0, realTime=0, icr=0, ocr=0;
    moduleStatistics *stats;
    int events=0, triggers=0;
    int ivalue;
    int channel=addr;
    int erased;
    int i;
    const char *functionName = "getAcquisitionStatistics";

    if (addr == this->nChannels) channel = DXP_ALL;
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s::%s addr=%d channel=%d\n", 
        driverName, functionName, addr, channel);
    if (channel == DXP_ALL) { /* All channels */
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
            "%s::%s start DXP_ALL\n", 
            driverName, functionName);
        addr = this->nChannels;
        for (i=0; i<this->nChannels; i++) {
            /* Call ourselves recursively but with a specific channel */
            this->getAcquisitionStatistics(pasynUser, i);
            getDoubleParam(i, mcaElapsedLiveTime, &dvalue);
            energyLiveTime = MAX(energyLiveTime, dvalue);
            getDoubleParam(i, NDDxpTriggerLiveTime, &dvalue);
            triggerLiveTime = MAX(triggerLiveTime, dvalue);
            getDoubleParam(i, mcaElapsedRealTime, &realTime);
            realTime = MAX(realTime, dvalue);
            getIntegerParam(i, NDDxpEvents, &ivalue);
            events = MAX(events, ivalue);
            getIntegerParam(i, NDDxpTriggers, &ivalue);
            triggers = MAX(triggers, ivalue);
            getDoubleParam(i, NDDxpInputCountRate, &dvalue);
            icr = MAX(icr, dvalue);
            getDoubleParam(i, NDDxpOutputCountRate, &dvalue);
            ocr = MAX(ocr, dvalue);
        }
        setDoubleParam(addr, mcaElapsedLiveTime, energyLiveTime);
        setDoubleParam(addr, NDDxpTriggerLiveTime, triggerLiveTime);
        setDoubleParam(addr, mcaElapsedRealTime, realTime);
        setIntegerParam(addr,NDDxpEvents, events);
        setIntegerParam(addr, NDDxpTriggers, triggers);
        setDoubleParam(addr, NDDxpInputCountRate, icr);
        setDoubleParam(addr, NDDxpOutputCountRate, ocr);
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
            "%s::%s end DXP_ALL\n", 
            driverName, functionName);
    } else {
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
            "%s::%s start channel %d\n", 
            driverName, functionName, addr);
        getIntegerParam(addr, NDDxpErased, &erased);
        if (erased) {
            setDoubleParam(addr, mcaElapsedLiveTime, 0);
            setDoubleParam(addr, mcaElapsedRealTime, 0);
            setIntegerParam(addr, NDDxpEvents, 0);
            setDoubleParam(addr, NDDxpInputCountRate, 0);
            setDoubleParam(addr, NDDxpOutputCountRate, 0);
            setIntegerParam(addr, NDDxpTriggers, 0);
            setDoubleParam(addr, NDDxpTriggerLiveTime, 0);
        } else {
            if (this->deviceType == NDDxpModelXMAP) {
                /* We only read the module statistics data if this is the first channel in a module.
                 * This assumes we are reading in numerical order, else we may get stale data! */
                if ((channel % XMAP_NCHANS_MODULE) == 0) getModuleStatistics(pasynUser, channel, &moduleStats[0]);
                stats = &moduleStats[channel % XMAP_NCHANS_MODULE];
            } else {
                stats = &moduleStats[0];
                getModuleStatistics(pasynUser, channel, &moduleStats[0]);
            }
            setIntegerParam(addr, NDDxpTriggers, (int)stats->triggers);
            setIntegerParam(addr, NDDxpEvents, (int)stats->events);
            setDoubleParam(addr, mcaElapsedRealTime, stats->realTime);
            setDoubleParam(addr, mcaElapsedLiveTime, stats->energyLiveTime);
            setDoubleParam(addr, NDDxpTriggerLiveTime, stats->triggerLiveTime);
            setDoubleParam(addr, NDDxpInputCountRate, stats->icr);
            setDoubleParam(addr, NDDxpOutputCountRate, stats->ocr);

            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
                "%s::%s  channel %d \n"
                "              events=%f\n" 
                "            triggers=%f\n" 
                "           real time=%f\n" 
                "     energy livetime=%f\n" 
                "    trigger livetime=%f\n" 
                "    input count rate=%f\n" 
                "   output count rate=%f\n",
                driverName, functionName, addr, 
                stats->events,
                stats->triggers,
                stats->realTime,
                stats->energyLiveTime,
                stats->triggerLiveTime,
                stats->icr,
                stats->ocr);
        }
    }
    return(asynSuccess);
}

asynStatus NDDxp::getDxpParams(asynUser *pasynUser, int addr)
{
    int i;
    double mcaBinWidth, numMcaChannels;
    double dvalue;
    double emax;
    int channel=addr;
    asynStatus status = asynSuccess;
    int xiastatus;
    int bufLen;
    double dTmp;
    int collectMode;
    int pixelsPerBuffer;
    int pixelsPerRun;
    int syncCount;
    int ignoreGate;
    int inputLogicPolarity;
    NDDxpPixelAdvanceMode_t pixelAdvanceMode;
    const char* functionName = "getDxpParams";

    if (addr == this->nChannels) channel = DXP_ALL;
    if (channel == DXP_ALL) {  /* All channels */
        addr = this->nChannels;
        for (i=0; i<this->nChannels; i++) {
            /* Call ourselves recursively but with a specific channel */
            this->getDxpParams(pasynUser, i);
        }
    } else {
        xiaGetAcquisitionValues(channel, "energy_threshold", &dvalue);
        /* Convert energy threshold from eV to keV */
        dvalue /= 1000.;
        setDoubleParam(channel, NDDxpEnergyThreshold, dvalue);
        xiaGetAcquisitionValues(channel, "peaking_time", &dvalue);
        setDoubleParam(channel, NDDxpPeakingTime, dvalue);
        xiaGetAcquisitionValues(channel, "gap_time", &dvalue);
        setDoubleParam(channel, NDDxpGapTime, dvalue);
        xiaGetAcquisitionValues(channel, "trigger_threshold", &dvalue);
        /* Convert trigger threshold from eV to keV */
        dvalue = dvalue / 1000.;
        setDoubleParam(channel, NDDxpTriggerThreshold, dvalue);
        xiaGetAcquisitionValues(channel, "trigger_peaking_time", &dvalue);
        setDoubleParam(channel, NDDxpTriggerPeakingTime, dvalue);
        xiaGetAcquisitionValues(channel, "trigger_gap_time", &dvalue);
        setDoubleParam(channel, NDDxpTriggerGapTime, dvalue);
        xiaGetAcquisitionValues(channel, "preamp_gain", &dvalue);
        setDoubleParam(channel, NDDxpPreampGain, dvalue);
        if (this->deviceType == NDDxpModelXMAP) {
           xiaGetAcquisitionValues(channel, "baseline_average", &dvalue);
        } else {
           xiaGetAcquisitionValues(channel, "baseline_filter_length", &dvalue);
        }
        setIntegerParam(channel, NDDxpBaselineAverage, (int)dvalue);
        xiaGetAcquisitionValues(channel, "baseline_threshold", &dvalue);
        dvalue/= 1000.;  /* Convert to keV */
        setDoubleParam(channel, NDDxpBaselineThreshold, dvalue);
        xiaGetAcquisitionValues(channel, "maxwidth", &dvalue);
        setDoubleParam(channel, NDDxpMaxWidth, dvalue);
        if (this->deviceType == NDDxpModelXMAP) {
            setDoubleParam(channel, NDDxpBaselineCut, 0.0);
            setIntegerParam(channel, NDDxpEnableBaselineCut, 0);
        } else {
            xiaGetAcquisitionValues(channel, "baseline_cut", &dvalue);
            setDoubleParam(channel, NDDxpBaselineCut, dvalue);
            xiaGetAcquisitionValues(channel, "enable_baseline_cut", &dvalue);
            setIntegerParam(channel, NDDxpEnableBaselineCut, (int)dvalue);
        }
        xiaGetAcquisitionValues(channel, "adc_percent_rule", &dvalue);
        setDoubleParam(channel, NDDxpADCPercentRule, dvalue);
        if (this->deviceType == NDDxpModelXMAP) {
            xiaGetAcquisitionValues(channel, "dynamic_range", &dvalue);
        } else dvalue = 0.;
        setDoubleParam(channel, NDDxpDynamicRange, dvalue);
        xiaGetAcquisitionValues(channel, "calibration_energy", &dvalue);
        setDoubleParam(channel, NDDxpCalibrationEnergy, dvalue);
        xiaGetAcquisitionValues(channel, "mca_bin_width", &mcaBinWidth);
        setDoubleParam(channel, NDDxpMCABinWidth, mcaBinWidth);
        xiaGetAcquisitionValues(channel, "number_mca_channels", &numMcaChannels);
        setIntegerParam(channel, mcaNumChannels, (int)numMcaChannels);
        /* Compute emax from mcaBinWidth and mcaNumChannels, convert eV to keV */
        emax = numMcaChannels * mcaBinWidth / 1000.;
        setDoubleParam(channel, NDDxpMaxEnergy, emax);
        xiaGetAcquisitionValues(channel, "detector_polarity", &dvalue);
        setIntegerParam(channel, NDDxpDetectorPolarity, (int)dvalue);
        xiaGetAcquisitionValues(channel, "decay_time", &dvalue);
        setDoubleParam(channel, NDDxpDecayTime, dvalue);
        xiaGetAcquisitionValues(channel, "reset_delay", &dvalue);
        setDoubleParam(channel, NDDxpResetDelay, dvalue);
        
        /* If this is an xMAP and this is channel 0 then read the mapping parameters */
        if ((channel == 0) && (this->deviceType == NDDxpModelXMAP)) {
            // Read mapping parameters, which are assumed to be the same for all modules 
            dTmp = 0;
            xiastatus = xiaGetAcquisitionValues(channel, "mapping_mode", &dTmp);
            status = this->xia_checkError(pasynUserSelf, xiastatus, "GET mapping_mode");
            asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                "%s::%s [%d] Got mapping_mode = %.1f\n", 
                driverName, functionName, channel, dTmp);
            collectMode = (int)dTmp;
            setIntegerParam(NDDxpCollectMode, collectMode);

            if (collectMode != NDDxpModeMCA) {
                dTmp = 0;
                xiastatus = xiaGetAcquisitionValues(channel, "pixel_advance_mode", &dTmp);
                status = this->xia_checkError(pasynUserSelf, xiastatus, "GET pixel_advance_mode");
                asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                    "%s::%s [%d] Got pixel_advance_mode = %.1f\n", 
                    driverName, functionName, channel, dTmp);
                pixelAdvanceMode = NDDxpPixelAdvanceSync;
                if (dTmp == XIA_MAPPING_CTL_GATE) pixelAdvanceMode = NDDxpPixelAdvanceGate;
                setIntegerParam(NDDxpPixelAdvanceMode, pixelAdvanceMode);

                dTmp = 0;
                xiastatus = xiaGetAcquisitionValues(channel, "num_map_pixels", &dTmp);
                status = this->xia_checkError(pasynUserSelf, xiastatus, "GET num_map_pixels");
                asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                    "%s::%s [%d] Got num_map_pixels = %.1f\n", 
                    driverName, functionName, channel, dTmp);
                pixelsPerRun = (int)dTmp;
                setIntegerParam(NDDxpPixelsPerRun, pixelsPerRun);

                dTmp = 0;
                xiastatus = xiaGetAcquisitionValues(channel, "num_map_pixels_per_buffer", &dTmp);
                status = this->xia_checkError(pasynUserSelf, xiastatus, "GET num_map_pixels_per_buffer");
                asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                    "%s::%s [%d] Got num_map_pixels_per_buffer = %.1f\n", 
                    driverName, functionName, channel, dTmp);
                pixelsPerBuffer = (int)dTmp;
                setIntegerParam(NDDxpPixelsPerBuffer, pixelsPerBuffer);

                dTmp = 0;
                xiastatus = xiaGetAcquisitionValues(channel, "sync_count", &dTmp);
                status = this->xia_checkError(pasynUserSelf, xiastatus, "GET sync_count");
                asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                    "%s::%s [%d] Got sync_count = %.1f\n", 
                    driverName, functionName, channel, dTmp);
                /* We add 1 to sync count because xMAP actually divides by N+1 */
                syncCount = (int)dTmp + 1;
                setIntegerParam(NDDxpSyncCount, syncCount);

                dTmp = 0;
                xiastatus = xiaGetAcquisitionValues(channel, "gate_ignore", &dTmp);
                status = this->xia_checkError(pasynUserSelf, xiastatus, "GET gate_ignore");
                asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                    "%s::%s [%d] Got gate_ignore = %.1f\n", 
                    driverName, functionName, channel, dTmp);
                ignoreGate = (int)dTmp;
                setIntegerParam(NDDxpIgnoreGate, ignoreGate);

                dTmp = 0;
                xiastatus = xiaGetAcquisitionValues(channel, "input_logic_polarity", &dTmp);
                status = this->xia_checkError(pasynUserSelf, xiastatus, "GET input_logic_polarity");
                asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                    "%s::%s [%d] Got input_logic_polarity = %.1f\n", 
                    driverName, functionName, channel, dTmp);
                inputLogicPolarity = (int)dTmp;
                setIntegerParam(NDDxpInputLogicPolarity, inputLogicPolarity);

                bufLen = 0;
                xiastatus = xiaGetRunData(channel, "buffer_len", &bufLen);
                status = this->xia_checkError(pasynUserSelf, xiastatus, "GET buffer_len");
                asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                    "%s::%s [%d] Got buffer_len = %u\n", 
                    driverName, functionName, channel, bufLen);
                setIntegerParam(channel, NDArraySize, (int)bufLen);
            }
        }
    }
    return(asynSuccess);
}


asynStatus NDDxp::getLLDxpParams(asynUser *pasynUser, int addr)
{
    int i, numParams, param;
    int status;
    int channel = ((addr < this->nChannels) ? addr : DXP_ALL);
    
    getIntegerParam(NDDxpNumLLParams, &numParams);

    if (channel == DXP_ALL) {  /* All channels */
        addr = this->nChannels;
        for (i=0; i<this->nChannels; i++) {
            /* Call ourselves recursively but with a specific channel */
            this->getLLDxpParams(pasynUser, i);
        }
    } else {
        status = xiaGetParamData(addr, "values", LLParamValues);
        for (param=0; param<numParams; param++) {
            setIntegerParam(addr, NDDxpLLParamVals[param], LLParamValues[LLParamSort[param]]);
        }
        /* Read the high-level parameters too */
        getDxpParams(pasynUser, addr);
        callParamCallbacks(addr, addr);
    }
    return(asynSuccess);
}
        

asynStatus NDDxp::setLLDxpParam(asynUser *pasynUser, int addr, int value)
{
    int i;
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    int xiastatus;
    char paramName[MAXSYMBOL_LEN];
    int channel = ((addr < this->nChannels) ? addr : DXP_ALL);
    //static const char* functionName = "setLLDxpParam";

    if (channel == DXP_ALL) {  /* All channels */
        for (i=0; i<this->nChannels; i++) {
            /* Call ourselves recursively but with a specific channel */
            this->setLLDxpParam(pasynUser, i, value);
        }
    } else {
        /* Note: we need to get the parameter Name, given pasynUser->reason, which is the index of the
         * parameter Value.  We rely on the fact that createParam is called in the order Name, Value for
         * each low-level parameter, so the index is just one less than pasynUser->reason; */
        getStringParam(addr, function-1, sizeof(paramName), paramName);
        xiastatus = xiaSetParameter(channel, paramName, (unsigned short)value);
        status = xia_checkError(pasynUser, xiastatus, "xiaSetParameter");
        /* Read back the parameters  */
        getLLDxpParams(pasynUser, addr);
    }
    return(status);
}
        

asynStatus NDDxp::getMcaData(asynUser *pasynUser, int addr)
{
    asynStatus status = asynSuccess;
    int xiastatus, paramStatus;
    int arrayCallbacks;
    int nChannels;
    int channel=addr;
    int spectrumCounter;
    int i;
    NDArray *pArray;
    NDDataType_t dataType;
    epicsTimeStamp now;
    const char* functionName = "getMcaData";

    if (addr == this->nChannels) channel = DXP_ALL;

    paramStatus  = getIntegerParam(mcaNumChannels, &nChannels);
    paramStatus |= getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
    paramStatus |= getIntegerParam(NDDataType, (int *)&dataType);
    paramStatus |= getIntegerParam(NDDxpPixelCounter, &spectrumCounter);

    epicsTimeGetCurrent(&now);

    /* If this is an xMAP and channel=DXP_ALL we can do an optimisation by reading all of the spectra
     * on a single xMAP with a single call. */
    if ((this->deviceType == NDDxpModelXMAP) && (channel==DXP_ALL)) {
        //xiaGetRunDataCmd = "module_mca";
        //nChansOnBoard = XMAP_NCHANS_MODULE;

    }

    if (channel == DXP_ALL) {  /* All channels */
        for (i=0; i<this->nChannels; i++) {
            /* Call ourselves recursively but with a specific channel */
                this->getMcaData(pasynUser, i);
        }
    } else {

        getIntegerParam(addr, NDDxpPixelCounter, &spectrumCounter);

        /* Read the MCA spectrum from Handel.
        * For most devices this means getting 1 channel spectrum here.
        * For the XMAP we get all 4 channels on the board in one go here */
        CALLHANDEL( xiaGetRunData(addr, "mca", this->pMcaRaw[addr]),"xiaGetRunData")
        spectrumCounter++;
        asynPrintIO(pasynUser, ASYN_TRACEIO_DRIVER, (const char *)pMcaRaw[addr], nChannels*sizeof(epicsUInt32),
            "%s::%s Got MCA spectrum channel:%d ptr:%p\n",
            driverName, functionName, channel, pMcaRaw[addr]);

        if (arrayCallbacks)
        {
            /* Allocate a buffer for callback */
            pArray = this->pNDArrayPool->alloc(1, &nChannels, dataType, 0, NULL);
            pArray->timeStamp = now.secPastEpoch + now.nsec / 1.e9;
            pArray->uniqueId = spectrumCounter;
            /* TODO: Need to copy the data here */
            //this->unlock();
            doCallbacksGenericPointer(pArray, NDArrayData, addr);
            //this->lock();
            pArray->release();
        }
    }
    return status;
}

/** Reads the mapping data for all of the modules in the system */
asynStatus NDDxp::getMappingData()
{
    asynStatus status = asynSuccess;
    int xiastatus;
    int arrayCallbacks;
    NDDataType_t dataType;
    int buf = 0, channel, i, k;
    NDArray *pArray=NULL;
    epicsUInt32 *pIn=NULL;
    epicsUInt32 *pStats;
    epicsUInt16 *pOut=NULL;
    int mappingMode, pixelOffset, dataOffset, events, triggers, nChans;
    double realTime, triggerLiveTime, energyLiveTime, icr, ocr;
    int dims[2], pixelCounter, bufferCounter, arraySize;
    epicsTimeStamp now, after;
    double mBytesRead;
    double readoutTime, readoutBurstSpeed, MBbufSize;
    const char* functionName = "getMappingData";

    getIntegerParam(NDDataType, (int *)&dataType);
    getIntegerParam(NDDxpPixelCounter, &pixelCounter);
    getIntegerParam(NDDxpBufferCounter, &bufferCounter);
    bufferCounter++;
    setIntegerParam(NDDxpBufferCounter, bufferCounter);
    getIntegerParam(NDArraySize, &arraySize);
    getDoubleParam(NDDxpMBytesReceived, &mBytesRead);
    getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
    MBbufSize = (double)((arraySize)*sizeof(epicsUInt16)) / (double)MEGABYTE;

    /* First read and reset the buffers, do this as quickly as possible */
    for (channel=0; channel<this->nChannels; channel+=XMAP_NCHANS_MODULE)
    {
        buf = this->currentBuf[channel];

        /* The buffer is full so read it out */
        epicsTimeGetCurrent(&now);
        xiastatus = xiaGetRunData(channel, NDDxpBufferString[buf], this->pMapRaw);
        status = xia_checkError(this->pasynUserSelf, xiastatus, "GetRunData mapping");
        epicsTimeGetCurrent(&after);
        readoutTime = epicsTimeDiffInSeconds(&after, &now);
        readoutBurstSpeed = MBbufSize / readoutTime;
        mBytesRead += MBbufSize;
        setDoubleParam(NDDxpMBytesReceived, mBytesRead);
        setDoubleParam(NDDxpReadSpeed, readoutBurstSpeed);
        /* Notify XMAP that we read out the buffer */
        xiastatus = xiaBoardOperation(channel, "buffer_done", NDDxpBufferCharString[buf]);
        status = xia_checkError(this->pasynUserSelf, xiastatus, "buffer_done");
        if (buf == 0) this->currentBuf[channel] = 1;
        else this->currentBuf[channel] = 0;
        callParamCallbacks();
        asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s::%s Got data! size=%.3fMB (%d) dt=%.3fs speed=%.3fMB/s\n",
            driverName, functionName, MBbufSize, arraySize, readoutTime, readoutBurstSpeed);
    
        /* If this is MCA mapping mode then copy the spectral data for the first pixel
         * in this buffer to the mcaRaw buffers.
         * This provides an update of the spectra and statistics while mapping is in progress
         * if the user sets the MCA spectra to periodically read. */
        mappingMode = pMapRaw[3];
        if (mappingMode == NDDxpModeSpectraMapping) {
            pixelOffset = 256;
            dataOffset = pixelOffset + 256;
            for (i=0; i<XMAP_NCHANS_MODULE; i++) {
                k = channel + i;
                nChans = pMapRaw[pixelOffset + 8 + i];
                memcpy(pMcaRaw[k], &pMapRaw[dataOffset], nChans*sizeof(epicsUInt32));
                dataOffset += nChans;
                pStats = &pMapRaw[pixelOffset + 32 + i*8];
                realTime        = (pStats[0] + (pStats[1]<<16)) * XMAP_CLOCK_PERIOD;
                triggerLiveTime = (pStats[2] + (pStats[3]<<16)) * XMAP_CLOCK_PERIOD;
                triggers        =  pStats[4] + (pStats[5]<<16);
                events          =  pStats[6] + (pStats[7]<<16);
                if (triggers > 0.) 
                    energyLiveTime = (triggerLiveTime * events) / triggers;
                else
                    energyLiveTime = triggerLiveTime;
                if (triggerLiveTime > 0.)
                    icr = triggers / triggerLiveTime;
                else
                    icr = 0.;
                if (realTime > 0.)
                    ocr = events / realTime;
                else
                    ocr = 0.;
                setDoubleParam(k, mcaElapsedRealTime, realTime);
                setDoubleParam(k, mcaElapsedLiveTime, energyLiveTime);
                setDoubleParam(k, NDDxpTriggerLiveTime, triggerLiveTime);
                setIntegerParam(k,NDDxpEvents, events);
                setIntegerParam(k, NDDxpTriggers, triggers);
                setDoubleParam(k, NDDxpInputCountRate, icr);
                setDoubleParam(k, NDDxpOutputCountRate, ocr);
                callParamCallbacks(k, k);
            }
        }
            
        if (arrayCallbacks)
        {
            /* Now rearrange the data and do the callbacks */
            /* If this is the first module then allocate an NDArray for the callback */
            if (channel == 0)
            {
               dims[0] = arraySize;
               dims[1] = this->nCards;
               pArray = this->pNDArrayPool->alloc(2, dims, dataType, 0, NULL );
               pOut = (epicsUInt16 *)pArray->pData;
            }
            
            /* First get rid of the empty parts of the 32 bit words. The Handel library
             * provides a 4MB 32 bit/word wide buffer, but the only data is in the low-order
             * 16 bits of each word. So we strip off the empty top 16 bits of each 32 bit
             * word. */
            pIn = this->pMapRaw;
            for (i=0; i<arraySize; i++)
            {
                *pOut++ = (epicsUInt16) *pIn++;
            }
        }
    }
    if (arrayCallbacks) 
    {
        pArray->timeStamp = now.secPastEpoch + now.nsec / 1.e9;
        pArray->uniqueId = bufferCounter;
        //this->unlock();
        doCallbacksGenericPointer(pArray, NDArrayData, 0);
        //this->lock();
        pArray->release();
    }
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
        "%s::%s Done reading! Ch=%d bufchar=%s\n",
        driverName, functionName, channel, NDDxpBufferCharString[buf]);

    return status;
}

/* Get trace data */
asynStatus NDDxp::getTrace(asynUser* pasynUser, int addr,
                           epicsInt32* data, size_t maxLen, size_t *actualLen)
{
    asynStatus status = asynSuccess;
    int xiastatus, channel=addr;
    int i;
    double info[2];
    double traceTime;
    int traceMode;
    //const char *functionName = "getTrace";

    if (addr == this->nChannels) channel = DXP_ALL;
    if (channel == DXP_ALL) {  /* All channels */
        for (i=0; i<this->nChannels; i++) {
            /* Call ourselves recursively but with a specific channel */
            this->getTrace(pasynUser, i, data, maxLen, actualLen);
        }
    } else {
        getDoubleParam(channel, NDDxpTraceTime, &traceTime);
        getIntegerParam(channel, NDDxpTraceMode, &traceMode);
        info[0] = 0.;
        /* Convert from us to ns */
        info[1] = traceTime * 1000.;

        xiastatus = xiaDoSpecialRun(channel, NDDxpTraceCommands[traceMode], info);
        status = this->xia_checkError(pasynUser, xiastatus, NDDxpTraceCommands[traceMode]);
        if (status == asynError) return asynError;

        *actualLen = this->traceLength;
        if (maxLen < *actualLen) *actualLen = maxLen;

        xiastatus = xiaGetSpecialRunData(channel, "adc_trace", this->traceBuffer);
        status = this->xia_checkError( pasynUser, xiastatus, "adc_trace" );
        if (status == asynError) return status;

        memcpy(data, this->traceBuffer, *actualLen * sizeof(epicsInt32));
    }
    return status;
}

/* Get trace data */
asynStatus NDDxp::getBaselineHistogram(asynUser* pasynUser, int addr,
                                       epicsInt32* data, size_t maxLen, size_t *actualLen)
{
    asynStatus status = asynSuccess;
    int i;
    int xiastatus, channel=addr;
    //const char *functionName = "getBaselineHistogram";

    if (addr == this->nChannels) channel = DXP_ALL;
    if (channel == DXP_ALL) {  /* All channels */
        for (i=0; i<this->nChannels; i++) {
            /* Call ourselves recursively but with a specific channel */
            this->getTrace(pasynUser, i, data, maxLen, actualLen);
        }
    } else {
        *actualLen = this->baselineLength;
        if (maxLen < *actualLen) *actualLen = maxLen;

        xiastatus = xiaGetRunData(channel, "baseline", this->baselineBuffer);
        status = this->xia_checkError(pasynUser, xiastatus, "baseline" );
        if (status == asynError) return status;

        memcpy(data, this->baselineBuffer, *actualLen * sizeof(epicsInt32));
    }
    return status;
}



asynStatus NDDxp::startAcquiring(asynUser *pasynUser)
{
    asynStatus status = asynSuccess;
    int xiastatus;
    int channel, addr, i;
    int acquiring, erased, resume=1;
    int firstCh;
    const char *functionName = "startAcquire";

    channel = this->getChannel(pasynUser, &addr);
    getIntegerParam(addr, mcaAcquiring, &acquiring);
    getIntegerParam(addr, NDDxpErased, &erased);
    if (erased) resume=0;

    asynPrint(pasynUser, ASYN_TRACE_FLOW,
        "%s::%s ch=%d acquiring=%d, erased=%d\n",
        driverName, functionName, channel, acquiring, erased);
    /* if already acquiring we just ignore and return */
    if (acquiring) return status;

    /* make sure we use buffer A to start with */
    for (firstCh=0; firstCh<this->nChannels; firstCh+=XMAP_NCHANS_MODULE) this->currentBuf[firstCh] = 0;

    // do xiaStart command
    CALLHANDEL( xiaStartRun(channel, resume), "xiaStartRun()" )

    setIntegerParam(addr, NDDxpErased, 0); /* reset the erased flag */
    setIntegerParam(addr, mcaAcquiring, 1); /* Set the acquiring flag */

    if (channel == DXP_ALL) {
        for (i=0; i<this->nChannels; i++) {
            setIntegerParam(i, mcaAcquiring, 1);
            setIntegerParam(i, NDDxpErased, 0);
            callParamCallbacks(i, i);
        }
    }

    callParamCallbacks(addr, addr);

    // signal cmdStartEvent to start the polling thread
    this->cmdStartEvent->signal();
    return status;
}

/** Thread used to poll the hardware for status and data.
 *
 */
void NDDxp::acquisitionTask()
{
    asynUser *pasynUser = this->pasynUserSelf;
    int paramStatus;
    int i;
    int mode;
    int acquiring = 0;
    epicsFloat64 pollTime, sleeptime, dtmp;
    epicsTimeStamp now, start;
    const char* functionName = "acquisitionTask";

    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
        "%s:%s acquisition task started!\n",
        driverName, functionName);
//    epicsEventTryWait(this->stopEventId); /* clear the stop event if it wasn't already */

    this->lock();

    while (this->polling) /* ... round and round and round we go until the IOC is shut down */
    {

        getIntegerParam(this->nChannels, mcaAcquiring, &acquiring);

        if (!acquiring)
        {
            /* Release the lock while we wait for an event that says acquire has started, then lock again */
            this->unlock();
            /* Wait for someone to signal the cmdStartEvent */
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
                "%s:%s Waiting for acquisition to start!\n",
                driverName, functionName);
            this->cmdStartEvent->wait();
            this->lock();
            asynPrint(this->pasynUserSelf, ASYN_TRACEIO_DRIVER,
                "%s::%s [%s]: started! (mode=%d)\n", 
                driverName, functionName, this->portName, mode);
        }
        epicsTimeGetCurrent(&start);

        /* In this loop we only read the acquisition status to minimise overhead.
         * If a transition from acquiring to done is detected then we read the statistics
         * and the data. */
        this->getAcquisitionStatus(this->pasynUserSelf, DXP_ALL);
        getIntegerParam(this->nChannels, NDDxpAcquiring, &acquiring);
        getIntegerParam(NDDxpCollectMode, &mode);
        if (mode == NDDxpModeMCA && (!acquiring))
        {
            /* There must have just been a transition from acquiring to not acquiring */
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
                "%s::%s Detected acquisition stop! Now reading statistics\n",
                driverName, functionName);
            this->getAcquisitionStatistics(this->pasynUserSelf, DXP_ALL);
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
                "%s::%s Detected acquisition stop! Now reading data\n",
                driverName, functionName);
            this->getMcaData(this->pasynUserSelf, DXP_ALL);
            this->getSCAData(this->pasynUserSelf, DXP_ALL);
        } else if ((mode == NDDxpModeSpectraMapping) || (mode == NDDxpModeSCAMapping))
        {
            this->pollMappingMode();
        }

        /* Do callbacks for all channels for everything except mcaAcquiring*/
        for (i=0; i<=this->nChannels; i++) callParamCallbacks(i, i);
        /* Copy internal acquiring flag to mcaAcquiring */
        for (i=0; i<=this->nChannels; i++) {
            getIntegerParam(i, NDDxpAcquiring, &acquiring);
            setIntegerParam(i, mcaAcquiring, acquiring);
            callParamCallbacks(i, i);
        }
        
        paramStatus |= getDoubleParam(NDDxpPollTime, &pollTime);
        epicsTimeGetCurrent(&now);
        dtmp = epicsTimeDiffInSeconds(&now, &start);
        sleeptime = pollTime - dtmp;
        if (sleeptime > 0.0)
        {
            //asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            //    "%s::%s Sleeping for %f seconds\n",
            //    driverName, functionName, sleeptime);
            this->unlock();
            epicsThreadSleep(sleeptime);
            this->lock();
        }
    }
}

/** Check if the current mapping buffer is full in which case it reads out the data */
asynStatus NDDxp::pollMappingMode()
{
    asynStatus status = asynSuccess;
    asynUser *pasynUser = this->pasynUserSelf;
    int xiastatus;
    int ch, chStep = 1, buf, isfull, allFull=1;
    int currentPixel = 0;
    const char* functionName = "pollMappingMode";

    if (this->deviceType == NDDxpModelXMAP) chStep = XMAP_NCHANS_MODULE;
    /* Step through all the first channels on all the boards in the system if
     * the device is an XMAP, otherwise just step through each individual channel. */
    for (ch=0; ch<this->nChannels; ch+=chStep)
    {
        buf = this->currentBuf[ch];

        CALLHANDEL( xiaGetRunData(ch, "current_pixel", &currentPixel) , "current_pixel" )
        setIntegerParam(ch, NDDxpCurrentPixel, (int)currentPixel);
        callParamCallbacks(ch, ch);
        CALLHANDEL( xiaGetRunData(ch, NDDxpBufferFullString[buf], &isfull), "NDDxpBufferFullString[buf]" )
        asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            "%s::%s %s isfull=%d\n",
            driverName, functionName, NDDxpBufferFullString[buf], isfull);
        if (!isfull) allFull = 0;
    }
    /* If all of the modules have full buffers then read them all out */
    if (allFull)
    {
        status = this->getMappingData();
    }
    return status;
}


void NDDxp::report(FILE *fp, int details)
{
    asynNDArrayDriver::report(fp, details);
}



asynStatus NDDxp::xia_checkError( asynUser* pasynUser, epicsInt32 xiastatus, char *xiacmd )
{
    if (xiastatus == XIA_SUCCESS) return asynSuccess;

    asynPrint( pasynUser, ASYN_TRACE_ERROR, 
        "### NDDxp: XIA ERROR: %d (%s)\n", 
        xiastatus, xiacmd );
    return asynError;
}

void NDDxp::shutdown()
{
    int status;
    double pollTime;
    
    getDoubleParam(NDDxpPollTime, &pollTime);
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
        "%s: shutting down in %f seconds\n", driverName, 2*pollTime);
    this->polling = 0;
    epicsThreadSleep(2*pollTime);
    status = xiaExit();
    if (status == XIA_SUCCESS)
    {
        printf("%s shut down successfully.\n",
            driverName);
        return;
    }
    printf("xiaExit() error: %d\n", status);
    return;
}

int NDDxp::getModuleType()
{
    /* This function returns an enum of type DXP_MODULE_TYPE for the module type.
     * It returns the type of the first module in the system.
     * IMPORTANT ASSUMPTION: It is assumed that a single EPICS IOC will only
     * be controlling a single type of XIA module (xMAP, Saturn, DXP2X)
     * If there is an error it returns -1.
     */
    char module_alias[MAXALIAS_LEN];
    char module_type[MAXITEM_LEN];
    int status;

    /* Get the module alias for the first channel */
    status = xiaGetModules_VB(0, module_alias);
    /* Get the module type for this module */
    status = xiaGetModuleItem(module_alias, "module_type", module_type);
    /* Look for known module types */
    if (strcmp(module_type, "xmap") == 0) return(NDDxpModelXMAP);
    if (strcmp(module_type, "dxpx10p") == 0) return(NDDxpModelSaturn);
    if (strcmp(module_type, "dxp4c2x") == 0) return(NDDxpModel4C2X);
    return(-1);
}

static const iocshArg NDDxpConfigArg0 = {"Asyn port name", iocshArgString};
static const iocshArg NDDxpConfigArg1 = {"Number of channels", iocshArgInt};
static const iocshArg NDDxpConfigArg2 = {"Maximum number of buffers", iocshArgInt};
static const iocshArg NDDxpConfigArg3 = {"Maximum amount of memory (bytes)", iocshArgInt};
static const iocshArg * const NDDxpConfigArgs[] =  {&NDDxpConfigArg0,
                                                    &NDDxpConfigArg1,
                                                    &NDDxpConfigArg2,
                                                    &NDDxpConfigArg3};
static const iocshFuncDef configNDDxp = {"NDDxpConfig", 4, NDDxpConfigArgs};
static void configNDDxpCallFunc(const iocshArgBuf *args)
{
    NDDxp_config(args[0].sval, args[1].ival, args[2].ival, args[3].ival);
}

static const iocshArg xiaLogLevelArg0 = { "logging level",iocshArgInt};
static const iocshArg * const xiaLogLevelArgs[1] = {&xiaLogLevelArg0};
static const iocshFuncDef xiaLogLevelFuncDef = {"xiaSetLogLevel",1,xiaLogLevelArgs};
static void xiaLogLevelCallFunc(const iocshArgBuf *args)
{
    xiaSetLogLevel(args[0].ival);
}

static const iocshArg xiaLogOutputArg0 = { "logging output file",iocshArgString};
static const iocshArg * const xiaLogOutputArgs[1] = {&xiaLogOutputArg0};
static const iocshFuncDef xiaLogOutputFuncDef = {"xiaSetLogOutput",1,xiaLogOutputArgs};
static void xiaLogOutputCallFunc(const iocshArgBuf *args)
{
    xiaSetLogOutput(args[0].sval);
}

static const iocshArg xiaInitArg0 = { "ini file",iocshArgString};
static const iocshArg * const xiaInitArgs[1] = {&xiaInitArg0};
static const iocshFuncDef xiaInitFuncDef = {"xiaInit",1,xiaInitArgs};
static void xiaInitCallFunc(const iocshArgBuf *args)
{
    xiaInit(args[0].sval);
}

static const iocshFuncDef xiaStartSystemFuncDef = {"xiaStartSystem",0,0};
static void xiaStartSystemCallFunc(const iocshArgBuf *args)
{
    xiaStartSystem();
}

static const iocshArg xiaSaveSystemArg0 = { "ini file",iocshArgString};
static const iocshArg * const xiaSaveSystemArgs[1] = {&xiaSaveSystemArg0};
static const iocshFuncDef xiaSaveSystemFuncDef = {"xiaSaveSystem",1,xiaSaveSystemArgs};
static void xiaSaveSystemCallFunc(const iocshArgBuf *args)
{
    xiaSaveSystem("handel_ini", args[0].sval);
}



static void NDDxpRegister(void)
{
    iocshRegister(&configNDDxp, configNDDxpCallFunc);
    iocshRegister(&xiaInitFuncDef,xiaInitCallFunc);
    iocshRegister(&xiaLogLevelFuncDef,xiaLogLevelCallFunc);
    iocshRegister(&xiaLogOutputFuncDef,xiaLogOutputCallFunc);
    iocshRegister(&xiaStartSystemFuncDef,xiaStartSystemCallFunc);
    iocshRegister(&xiaSaveSystemFuncDef,xiaSaveSystemCallFunc);
}

extern "C" {
epicsExportRegistrar(NDDxpRegister);
}
