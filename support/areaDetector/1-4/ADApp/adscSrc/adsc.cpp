/* adsc.cpp
 *
 * This is a driver for ADSC detectors (Q4, Q4r, Q210, Q210r, Q270, Q315,
 * Q315r).
 *
 * The ADSC control library should be accessed from only one thread of
 * execution at a time; a mutex is used to ensure this.  Similarly, the
 * paramList instance should be accessed from only one thread of execution at
 * a time; a mutex is used to ensure this.
 *
 * Author: J. Lewis Muir (based on Prosilica driver by Mark Rivers)
 *         University of Chicago
 *
 * Created: April 11, 2008
 *
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>

#include <epicsAssert.h>
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsEvent.h>
#include <epicsString.h>
#include <epicsStdio.h>
#include <epicsMutex.h>
#include <cantProceed.h>

#include "ADStdDriverParams.h"
#include "ADDriver.h"

#include "drvAdsc.h"

extern "C" {
#include <detcon_entry.h>
#include <detcon_par.h>
#include <detcon_state.h>
}

#ifdef USE_SIMADSC
#include <simadsc.h>
#endif

#define ADSC_CCD_INITIALIZE_SLEEP_TIME 0.5
#define ADSC_CCD_INITIALIZE_TIMEOUT 10.0
#define PATH_COMPONENT_SEPARATOR "/"
#define START_EXPOSURE_TIMEOUT 30.0
#define STOP_EXPOSURE_TIMEOUT 10.0
#define STATE_POLL_DELAY 0.05

static const char *driverName = "drvAdsc";

static int DriverConfiguredForOnePort = 0;
static int AdscDetCtrlLibInitialized = 0;

typedef enum {
    AdscStatusOk,
    AdscStatusInterrupt,
    AdscStatusTimeout,
    AdscStatusAgain,
    AdscStatusError
} AdscStatus_t;

typedef enum {
    AdscExternalTriggerControlStop,
    AdscExternalTriggerControlStart,
    AdscExternalTriggerControlOk,
    AdscExternalTriggerControlAgain
} AdscExternalTriggerControl_t;

typedef enum {
    AdscQ4,
    AdscQ4r,
    AdscQ210,
    AdscQ210r,
    AdscQ270,
    AdscQ315,
    AdscQ315r
} AdscModel_t;

static const char *AdscModelStrings[] = {
    "Q4","Q4r","Q210","Q210r","Q270","Q315","Q315r"
};

#define NUM_ADSC_MODELS ((int)(sizeof(AdscModelStrings) / \
    sizeof(AdscModelStrings[0])))

typedef struct AdscSensor_t {
  int xSize;        /* number of pixels in X dimension    */
  int ySize;        /* number of pixels in Y dimension    */
  int bitsPerPixel; /* ADC digitizes to bitsPerPixel bits */
} AdscSensor_t;

static AdscSensor_t AdscSensors[] = {
    /* X     Y    B */
    { 2304, 2304, 16 }, /* Q4    */
    { 2304, 2304, 16 }, /* Q4r   */
    { 4096, 4096, 16 }, /* Q210  */
    { 4096, 4096, 16 }, /* Q210r */
    { 4168, 4168, 16 }, /* Q270  */
    { 6144, 6144, 16 }, /* Q315  */
    { 6144, 6144, 16 }  /* Q315r */
};

static const char *AdscCcdStateStrings[] = {
    "IDLE","EXPOSING","READING","ERROR","CONFIGDET","RETRY","TEMPCONTROL"
};

#define NUM_ADSC_CCD_STATES ((int)(sizeof(AdscCcdStateStrings) / \
    sizeof(AdscCcdStateStrings[0])))

typedef enum {
    /* These parameters describe the trigger modes of this driver; they must
     * agree with the values in the mbbo/mbbi records in the adsc.template
     * database (or the ADBase.template database if the default is used) */
    AdscTriggerStartInternal,
    AdscTriggerStartExternal
} AdscTriggerStartMode_t;

static const char *AdscTriggerStartStrings[] = {
    "Internal","External"
};

#define NUM_START_TRIGGER_MODES ((int)(sizeof(AdscTriggerStartStrings) / \
    sizeof(AdscTriggerStartStrings[0])))

/* If we have any private driver commands, they begin with
 * ADFirstDriverCommand and should end with ADLastDriverCommand, which is used
 * for setting the size of the parameter library table */
typedef enum {
    AdscReadCondition = ADFirstDriverParam,
    AdscState,
    AdscStatus,
    AdscLastError,
    AdscSoftwareReset,
    AdscLastImage,
    AdscOkToExpose,
    AdscExternalTriggerControl,
    AdscReuseDarks,
    AdscDezinger,
    AdscAdc,
    AdscRaw,
    AdscImageTransform,
    AdscStoredDarks,
    AdscBeamCenterX,
    AdscBeamCenterY,
    AdscDistance,
    AdscTwoTheta,
    AdscAxis,
    AdscWavelength,
    AdscImageWidth,
    AdscPhi,
    AdscOmega,
    AdscKappa,
    AdscPrivateStopExpRetryCnt, /* only for testing with simadsc */
    ADLastDriverParam
} AdscParam_t;

static asynParamString_t AdscParamString[] = {
    { AdscReadCondition,          "ADSC_READ_CONDITION" },
    { AdscState,                  "ADSC_STATE" },
    { AdscStatus,                 "ADSC_STATUS" },
    { AdscLastError,              "ADSC_LAST_ERROR" },
    { AdscSoftwareReset,          "ADSC_SOFTWARE_RESET" },
    { AdscLastImage,              "ADSC_LAST_IMAGE" },
    { AdscOkToExpose,             "ADSC_OK_TO_EXPOSE" },
    { AdscExternalTriggerControl, "ADSC_EXTERNAL_TRIGGER_CTRL" },
    { AdscReuseDarks,             "ADSC_REUSE_DARKS" },
    { AdscDezinger,               "ADSC_DEZINGER" },
    { AdscAdc,                    "ADSC_ADC" },
    { AdscRaw,                    "ADSC_RAW" },
    { AdscImageTransform,         "ADSC_IMAGE_TRANSFORM" },
    { AdscStoredDarks,            "ADSC_STORED_DARKS" },
    { AdscBeamCenterX,            "ADSC_BEAM_CENTER_X" },
    { AdscBeamCenterY,            "ADSC_BEAM_CENTER_Y" },
    { AdscDistance,               "ADSC_DISTANCE" },
    { AdscTwoTheta,               "ADSC_TWO_THETA" },
    { AdscAxis,                   "ADSC_AXIS" },
    { AdscWavelength,             "ADSC_WAVELENGTH" },
    { AdscImageWidth,             "ADSC_IMAGE_WIDTH" },
    { AdscPhi,                    "ADSC_PHI" },
    { AdscOmega,                  "ADSC_OMEGA" },
    { AdscPrivateStopExpRetryCnt, "ADSC_PRVT_STOP_EXP_RTRY_CNT" },
    { AdscKappa,                  "ADSC_KAPPA" }
};

#define NUM_ADSC_PARAMS ((int)(sizeof(AdscParamString) / \
    sizeof(AdscParamString[0])))

static void imageAcquisitionTaskC(void *drvPvt);

class adsc : public ADDriver {
public:
    adsc(const char *portName, const char *modelName);
    virtual ~adsc();

    /* These are the methods that we override from ADDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual asynStatus writeOctet(asynUser *pasynUser, const char *value,
                                  size_t nChars, size_t *nActual);
    virtual asynStatus drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                                     const char **pptypeName, size_t *psize);
    void report(FILE *fp, int details);

    /* These are the methods that are new to this class */
    void imageAcquisitionTask();
protected:
    AdscStatus_t acquireImages();
    void acquisitionFinished(int adstatus);
    void clearLastDarksParameters();
    AdscStatus_t createFileNameNoIncrement(char *dst, size_t dstSize);
    const char *getAdscCcdStateString(int ccdState);
    AdscStatus_t getImage(int lastImage);
    int getImageSize(AdscModel_t model, int binMode);
    int getImageSizeX(AdscModel_t model, int binMode);
    int getImageSizeY(AdscModel_t model, int binMode);
    AdscStatus_t loadPerDatasetParameters();
    AdscStatus_t loadPerImageParameters();
    AdscStatus_t readDetectorCondition();
    AdscStatus_t resetControlLibrary();
    AdscStatus_t setBinModeInParams(int binMode);
    void setLastDarksParameters(double exposureTime, int adc, int bin);
    AdscStatus_t setOkToExpose(int isEnabled);
    AdscStatus_t setExternalTriggerControl(AdscExternalTriggerControl_t
                                           value);
    int shouldTakeDarks();
    AdscStatus_t startExposure();
    AdscStatus_t stopExposure();
    AdscStatus_t takeDarks(const char *destDir);
    AdscStatus_t takeDarksIfRequired();
    AdscStatus_t takeImage(const char *fullFileName, int imageKind,
                           int lastImage, int triggerMode);
    AdscStatus_t imageAcquired();
    AdscStatus_t waitForDetectorState(int desiredState, double timeout,
                                      int failOnErrorState);
    AdscStatus_t waitForExternalTrigger(epicsEventId eventId);
    AdscStatus_t writeDetectorParametersBeforeDataset();
    AdscStatus_t writeDetectorParametersBeforeImage();

    /* These items are specific to the ADSC driver */
    AdscModel_t model;

    int lastImage;

    double lastDarksExposureTime;
    int lastDarksAdc;
    int lastDarksBin;

    int perDatasetReuseDarks;
    int perDatasetDezinger;
    int perDatasetAdc;
    int perDatasetRaw;
    int perDatasetImageTransform;
    int perDatasetStoredDarks;
    int perDatasetAxis;
    double perDatasetExposureTime;
    double perDatasetAcquirePeriod;
    int perDatasetBin;
    int perDatasetImageMode;
    int perDatasetTriggerMode;
    int perDatasetNumImages;

    double perImageBeamCenterX;
    double perImageBeamCenterY;
    double perImageDistance;
    double perImageTwoTheta;
    double perImageWavelength;
    double perImageImageWidth;
    double perImagePhi;
    double perImageOmega;
    double perImageKappa;
    char perImageFullFileName[MAX_FILENAME_LEN];

    epicsEventId startEventId;
    epicsEventId stopEventId;
    epicsEventId startTriggerEventId;
    epicsEventId stopTriggerEventId;
    epicsEventId lastImageEventId;
};

asynStatus adsc::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function = pasynUser->reason;
    int status = asynSuccess;
    int addr = 0;
    int acquire;
    int adstatus;
    const char *functionName = "writeInt32";

    switch (function) {
        case ADBinX:
        case ADBinY:
            if (value < 1 || value > 2) {
                status = asynError;
                break;
            }
            status |= setBinModeInParams(value);
            break;
        case ADImageMode:
            if (value != ADImageSingle && value != ADImageMultiple &&
                    value != ADImageContinuous) {
                status = asynError;
                break;
            }
            status |= setIntegerParam(addr, ADImageMode, value);
            break;
        case ADTriggerMode:
            if (value < 0 || value >= NUM_START_TRIGGER_MODES) {
                status = asynError;
                break;
            }
            status |= setIntegerParam(addr, ADTriggerMode, value);
            break;
        case ADNumImages:
            if (value < 0) {
                status = asynError;
                break;
            }
            status |= setIntegerParam(addr, ADNumImages, value);
            break;
        case ADAcquire:
            if (value < 0 || value > 1) {
                status = asynError;
                break;
            }

            status |= getIntegerParam(addr, ADAcquire, &acquire);
            status |= getIntegerParam(addr, ADStatus, &adstatus);
            if (status != asynSuccess) {
                status = asynError;
                break;
            }
            if (value == acquire) break;

            status |= setIntegerParam(addr, ADAcquire, value);
            if (status != asynSuccess) {
                status = asynError;
                break;
            }

            if (value == 1) {
                /* If detector is in an error state, attempt auto-recovery */
                if (adstatus == ADStatusError ||
                        CCDState() == DTC_STATE_ERROR ||
                        strcmp("", CCDGetLastError()) != 0) {
                    status |= resetControlLibrary();
                    if (status != 0) {
                        status = asynError;
                        break;
                    }
                }
                /* Clear events. */
                epicsEventTryWait(this->stopEventId);
                epicsEventTryWait(this->startTriggerEventId);
                epicsEventTryWait(this->stopTriggerEventId);
                epicsEventTryWait(this->lastImageEventId);
                /* Send an event to wake up the collect task.  It won't
                 * actually start collecting until we release the lock below.
                 */
                epicsEventSignal(this->startEventId);
            } else {
                /* This is a command to stop acquisition; send stop event. */
                epicsEventSignal(this->stopEventId);
                epicsEventSignal(this->startTriggerEventId);
                epicsEventSignal(this->stopTriggerEventId);
            }
            break;
        case ADImageCounter:
            if (value < 0) {
                status = asynError;
                break;
            }
            status |= setIntegerParam(addr, ADImageCounter, value);
            break;
        case ADFileNumber:
            if (value < 0) {
                status = asynError;
                break;
            }
            status |= setIntegerParam(addr, ADFileNumber, value);
            break;
        case ADAutoIncrement:
            if (value != 0 && value != 1) {
                status = asynError;
                break;
            }
            status |= setIntegerParam(addr, ADAutoIncrement, value);
            break;
        case AdscReadCondition:
            status |= readDetectorCondition();
            break;
        case AdscSoftwareReset:
            status |= getIntegerParam(addr, ADAcquire, &acquire);
            if (status != 0 || acquire == 1) break;
            status |= resetControlLibrary();
            break;
        case AdscLastImage:
            status |= getIntegerParam(addr, ADAcquire, &acquire);
            if (status != 0 || acquire == 0) break;
            epicsEventSignal(this->lastImageEventId);
            break;
        case AdscExternalTriggerControl:
            if (value < 0 || value > 3) {
                status = asynError;
                break;
            }
            status |= getIntegerParam(addr, ADAcquire, &acquire);
            if (status != 0 || acquire == 0) break;
            status |= setIntegerParam(addr, AdscExternalTriggerControl,
                                      value);
            if (value == AdscExternalTriggerControlStart)
                epicsEventSignal(this->startTriggerEventId);
            else if (value == AdscExternalTriggerControlStop)
                epicsEventSignal(this->stopTriggerEventId);
            break;
        case AdscReuseDarks:
            if (value != 0 && value != 1) {
                status = asynError;
                break;
            }
            status |= setIntegerParam(addr, AdscReuseDarks, value);
            break;
        case AdscDezinger:
            if (value != 0 && value != 1) {
                status = asynError;
                break;
            }
            status |= setIntegerParam(addr, AdscDezinger, value);
            break;
        case AdscAdc:
            if (value != 0 && value != 1) {
                status = asynError;
                break;
            }
            status |= setIntegerParam(addr, AdscAdc, value);
            break;
        case AdscRaw:
            if (value != 0 && value != 1) {
                status = asynError;
                break;
            }
            status |= setIntegerParam(addr, AdscRaw, value);
            break;
        case AdscImageTransform:
            if (value != 0 && value != 1) {
                status = asynError;
                break;
            }
            status |= setIntegerParam(addr, AdscImageTransform, value);
            break;
        case AdscStoredDarks:
            if (value != 0 && value != 1) {
                status = asynError;
                break;
            }
            status |= setIntegerParam(addr, AdscStoredDarks, value);
            if (value == 0) clearLastDarksParameters();
            break;
        case AdscAxis:
            if (value != 0 && value != 1) {
                status = asynError;
                break;
            }
            status |= setIntegerParam(addr, AdscAxis, value);
            break;
        case AdscPrivateStopExpRetryCnt:
            #ifdef USE_SIMADSC
                simadscSetStopExposureRetryCount(value);
            #endif
            break;
        default:
            break;
    }

    status |= callParamCallbacks(addr, addr);

    if (status != asynSuccess)
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "%s:%s: error, status=%d function=%d, value=%d\n",
                  driverName, functionName, status, function, value);
    else
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
                  "%s:%s: function=%d, value=%d\n",
                  driverName, functionName, function, value);

    return (asynStatus)status;
}

asynStatus adsc::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
    int function = pasynUser->reason;
    int status = asynSuccess;
    int addr = 0;
    const char *functionName = "writeFloat64";

    switch (function) {
        case ADAcquireTime:
            if (value < 0.0) {
                status = asynError;
                break;
            }
            status |= setDoubleParam(addr, ADAcquireTime, value);
            break;
        case ADAcquirePeriod:
            if (value < 0.0) {
                status = asynError;
                break;
            }
            status |= setDoubleParam(addr, ADAcquirePeriod, value);
            break;
        case AdscBeamCenterX:
            status |= setDoubleParam(addr, AdscBeamCenterX, value);
            break;
        case AdscBeamCenterY:
            status |= setDoubleParam(addr, AdscBeamCenterY, value);
            break;
        case AdscDistance:
            if (value < 0.0) {
                status = asynError;
                break;
            }
            status |= setDoubleParam(addr, AdscDistance, value);
            break;
        case AdscTwoTheta:
            status |= setDoubleParam(addr, AdscTwoTheta, value);
            break;
        case AdscWavelength:
            if (value < 0.0) {
                status = asynError;
                break;
            }
            status |= setDoubleParam(addr, AdscWavelength, value);
            break;
        case AdscImageWidth:
            status |= setDoubleParam(addr, AdscImageWidth, value);
            break;
        case AdscPhi:
            status |= setDoubleParam(addr, AdscPhi, value);
            break;
        case AdscOmega:
            status |= setDoubleParam(addr, AdscOmega, value);
            break;
        case AdscKappa:
            status |= setDoubleParam(addr, AdscKappa, value);
            break;
        default:
            break;
    }

    status |= callParamCallbacks(addr, addr);

    if (status != asynSuccess)
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "%s:%s: error, status=%d function=%d, value=%f\n",
                  driverName, functionName, status, function, value);
    else
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
                  "%s:%s: function=%d, value=%f\n",
                  driverName, functionName, function, value);

    return (asynStatus)status;
}

asynStatus adsc::writeOctet(asynUser *pasynUser, const char *value,
                            size_t nChars, size_t *nActual)
{
    int function = pasynUser->reason;
    int status = asynSuccess;
    int addr = 0;
    const char *functionName = "writeOctet";

    switch (function) {
        case ADFilePath:
        case ADFileName:
        case ADFileTemplate:
            status |= setStringParam(addr, function, (char *)value);
            break;
        default:
            break;
    }

    status |= callParamCallbacks(addr, addr);

    if (status != asynSuccess)
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "%s:%s: error, status=%d function=%d, value=%s\n",
                  driverName, functionName, status, function, value);
    else
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
                  "%s:%s: function=%d, value=%s\n",
                  driverName, functionName, function, value);

    *nActual = nChars;
    return (asynStatus)status;
}

asynStatus adsc::drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                               const char **pptypeName, size_t *psize)
{
    int status = asynSuccess;
    int param;
    const char *functionName = "drvUserCreate";

    /* See if this is one of this drivers' parameters */
    status = findParam(AdscParamString, NUM_ADSC_PARAMS, drvInfo, &param);
    if (status == asynSuccess) {
        pasynUser->reason = param;
        if (pptypeName) {
            *pptypeName = epicsStrDup(drvInfo);
        }
        if (psize) {
            *psize = sizeof(param);
        }
        asynPrint(pasynUser, ASYN_TRACE_FLOW,
                  "%s:%s: drvInfo=%s, param=%d\n",
                  driverName, functionName, drvInfo, param);
        return asynSuccess;
    }

    /* This was not one of our driver parameters, call base class method */
    status = ADDriver::drvUserCreate(pasynUser, drvInfo, pptypeName, psize);

    return (asynStatus)status;
}

void adsc::report(FILE *fp, int details)
{
    fprintf(fp, "ADSC detector %s\n", this->portName);
    if (details > 0) {
        fprintf(fp, "    Model:         %s\n", AdscModelStrings[this->model]);
        fprintf(fp, "    Sensor bits:   %d\n",
                AdscSensors[this->model].bitsPerPixel);
        fprintf(fp, "    Sensor width:  %d\n",
                AdscSensors[this->model].xSize);
        fprintf(fp, "    Sensor height: %d\n",
                AdscSensors[this->model].ySize);
    }

    ADDriver::report(fp, details);
}

/*
 * This function configures the ADSC driver.
 *
 * portName   asyn port name
 * modelName  ADSC detector model name; must be one of those defined in
 *            AdscModelStrings
 */
extern "C" int adscConfig(const char *portName, const char *modelName)
{
    new adsc(portName, modelName);
    return asynSuccess;
}

adsc::adsc(const char *portName, const char *modelName)
    : ADDriver(portName, 1, ADLastDriverParam, 0, 0, 0, 0)
{
    int status = asynSuccess;
    const char *functionName = "adscConfig";
    int addr = 0;
    epicsTimeStamp startTime, endTime;
    double elapsedTime;
    int ccdState;
    int i;

    if (DriverConfiguredForOnePort) {
        fprintf(stderr, "%s ERROR: Already configured; this driver may be "
                "configured only once for one port\n", functionName);
        return;
    }

    for (i = 0; i < NUM_ADSC_MODELS; i++) {
        if (strcmp(AdscModelStrings[i], modelName) != 0) continue;
        this->model = (AdscModel_t)i; break;
    }
    if (i == NUM_ADSC_MODELS) {
        fprintf(stderr, "%s ERROR: Invalid model name\n", functionName);
        return;
    }
    this->lastImage = 0;
    clearLastDarksParameters();

    /* Create the epicsEvents for signaling acquisition related events */
    this->startEventId = epicsEventMustCreate(epicsEventEmpty);
    this->stopEventId = epicsEventMustCreate(epicsEventEmpty);
    this->startTriggerEventId = epicsEventMustCreate(epicsEventEmpty);
    this->stopTriggerEventId = epicsEventMustCreate(epicsEventEmpty);
    this->lastImageEventId = epicsEventMustCreate(epicsEventEmpty);

    /* Initialize the ADSC detector control library (the CCDInitialize()
     * function is only supposed to be called once) */
    if (!AdscDetCtrlLibInitialized) {
        status = CCDInitialize();
        if (status != 0) {
            fprintf(stderr, "%s ERROR: CCDInitialize failed, status=%d\n",
                    driverName, status);
            return;
        }
        epicsTimeGetCurrent(&startTime);
        for (ccdState = CCDState(); ccdState != DTC_STATE_IDLE;
                ccdState = CCDState()) {
            epicsThreadSleep(ADSC_CCD_INITIALIZE_SLEEP_TIME);
            epicsTimeGetCurrent(&endTime);
            elapsedTime = epicsTimeDiffInSeconds(&endTime, &startTime);
            if (elapsedTime < ADSC_CCD_INITIALIZE_TIMEOUT) continue;
            fprintf(stderr, "%s ERROR: CCDInitialize timeout, CCDState=%s\n",
                    driverName, getAdscCcdStateString(ccdState));
            return;
        }
        AdscDetCtrlLibInitialized = 1;
    }

    /* Set driver specific defaults */
    status |= setStringParam(addr, ADManufacturer, "ADSC");
    status |= setStringParam(addr, ADModel, AdscModelStrings[this->model]);
    status |= setIntegerParam(addr, ADSizeX, AdscSensors[this->model].xSize);
    status |= setIntegerParam(addr, ADSizeY, AdscSensors[this->model].ySize);
    status |= setIntegerParam(addr, ADMaxSizeX,
                              AdscSensors[this->model].xSize);
    status |= setIntegerParam(addr, ADMaxSizeY,
                              AdscSensors[this->model].ySize);
    status |= setBinModeInParams(1);
    status |= setIntegerParam(addr, ADDataType, NDUInt16);
    status |= setIntegerParam(addr, ADAutoSave, 1);
    status |= setDoubleParam(addr, ADAcquireTime, 1.0);
    status |= setDoubleParam(addr, ADAcquirePeriod, 0.0);
    status |= setIntegerParam(addr, ADNumImages, 1);
    status |= setIntegerParam(addr, ADImageMode, ADImageSingle);
    status |= setIntegerParam(addr, AdscReadCondition, 0);
    status |= setIntegerParam(addr, AdscSoftwareReset, 0);
    status |= setIntegerParam(addr, AdscLastImage, 0);
    status |= setIntegerParam(addr, AdscOkToExpose, 0);
    status |= setIntegerParam(addr, AdscExternalTriggerControl,
                              AdscExternalTriggerControlOk);
    status |= setIntegerParam(addr, AdscReuseDarks, 1);
    status |= setIntegerParam(addr, AdscDezinger, 0);
    status |= setIntegerParam(addr, AdscAdc, 1);
    status |= setIntegerParam(addr, AdscRaw, 0);
    status |= setIntegerParam(addr, AdscImageTransform, 1);
    status |= setIntegerParam(addr, AdscStoredDarks, 0);
    status |= setIntegerParam(addr, AdscAxis, 1);
    status |= setIntegerParam(addr, AdscPrivateStopExpRetryCnt, 0);
    status |= readDetectorCondition();

    if (status != 0) {
        fprintf(stderr, "%s ERROR: failed to set driver specific parameter "
                "defaults\n", functionName);
        return;
    }

    /* Create the image acquisition thread */
    status = (epicsThreadCreate("ImageAcquisitionTask",
                                epicsThreadPriorityMedium,
                                epicsThreadGetStackSize(
                                    epicsThreadStackMedium),
                                (EPICSTHREADFUNC)imageAcquisitionTaskC,
                                this) == NULL);
    if (status) {
        fprintf(stderr, "%s ERROR: epicsThreadCreate failed for image "
                "acquisition task\n", functionName);
        return;
    }

    DriverConfiguredForOnePort = 1;
}

adsc::~adsc()
{
    /* not designed to be destroyed */
}

void adsc::clearLastDarksParameters()
{
    setLastDarksParameters(-1.0, -1, -1);
}

void adsc::setLastDarksParameters(double exposureTime, int adc, int bin)
{
    this->lastDarksExposureTime = exposureTime;
    this->lastDarksAdc = adc;
    this->lastDarksBin = bin;
}

AdscStatus_t adsc::resetControlLibrary()
{
    int status = 0;
    int addr = 0;

    epicsMutexMustLock(this->mutexId);

    status |= CCDAbort();
    status |= CCDReset();
    status |= setIntegerParam(addr, ADStatus, ADStatusIdle);
    status |= readDetectorCondition();

    epicsMutexUnlock(this->mutexId);

    if (status == 0) return AdscStatusOk;

    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s:resetControlLibrary "
              "error, status=%d\n", driverName, status);
    return AdscStatusError;
}

AdscStatus_t adsc::readDetectorCondition()
{
    int status = 0;
    int addr = 0;

    epicsMutexMustLock(this->mutexId);

    status |= setIntegerParam(addr, AdscState, CCDState());
    status |= setStringParam(addr, AdscStatus, CCDStatus());
    status |= setStringParam(addr, AdscLastError, CCDGetLastError());

    epicsMutexUnlock(this->mutexId);

    if (status == 0) return AdscStatusOk;

    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s:readDetectorCondition "
              "error, status=%d\n", driverName, status);
    return AdscStatusError;
}

AdscStatus_t adsc::writeDetectorParametersBeforeDataset()
{
    int status = 0;
    int ival;

    epicsMutexMustLock(this->mutexId);

    status |= CCDSetHwPar(HWP_BIN, (char *)&this->perDatasetBin);
    status |= CCDSetHwPar(HWP_ADC, (char *)&this->perDatasetAdc);
    status |= CCDSetHwPar(HWP_SAVE_RAW, (char *)&this->perDatasetRaw);
    ival = this->perDatasetImageTransform == 0 ? 1 : 0;
    status |= CCDSetHwPar(HWP_NO_XFORM, (char *)&ival);
    status |= CCDSetHwPar(HWP_STORED_DARK,
                          (char *)&this->perDatasetStoredDarks);
    status |= CCDSetFilePar(FLP_AXIS, (char *)&this->perDatasetAxis);

    epicsMutexUnlock(this->mutexId);

    if (status == 0) return AdscStatusOk;

    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s:writeDetectorParameters "
              "error, status=%d\n", driverName, status);
    return AdscStatusError;
}

AdscStatus_t adsc::writeDetectorParametersBeforeImage()
{
    int status = 0;
    float fval;

    epicsMutexMustLock(this->mutexId);

    fval = (float)this->perImageBeamCenterX;
    status |= CCDSetFilePar(FLP_BEAM_X, (char *)&fval);
    fval = (float)this->perImageBeamCenterY;
    status |= CCDSetFilePar(FLP_BEAM_Y, (char *)&fval);
    fval = (float)this->perImageDistance;
    status |= CCDSetFilePar(FLP_DISTANCE, (char *)&fval);
    fval = (float)this->perImageTwoTheta;
    status |= CCDSetFilePar(FLP_TWOTHETA, (char *)&fval);
    fval = (float)this->perImageWavelength;
    status |= CCDSetFilePar(FLP_WAVELENGTH, (char *)&fval);
    fval = (float)this->perImageImageWidth;
    status |= CCDSetFilePar(FLP_OSC_RANGE, (char *)&fval);
    fval = (float)this->perImagePhi;
    status |= CCDSetFilePar(FLP_PHI, (char *)&fval);
    fval = (float)this->perImageOmega;
    status |= CCDSetFilePar(FLP_OMEGA, (char *)&fval);
    fval = (float)this->perImageKappa;
    status |= CCDSetFilePar(FLP_KAPPA, (char *)&fval);

    epicsMutexUnlock(this->mutexId);

    if (status == 0) return AdscStatusOk;

    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s:writeDetectorParameters "
              "error, status=%d\n", driverName, status);
    return AdscStatusError;
}

AdscStatus_t adsc::loadPerDatasetParameters()
{
    int status = 0;
    int addr = 0;

    epicsMutexMustLock(this->mutexId);

    status |= getIntegerParam(addr, AdscReuseDarks,
                              &this->perDatasetReuseDarks);
    status |= getIntegerParam(addr, AdscDezinger, &this->perDatasetDezinger);
    status |= getIntegerParam(addr, ADBinX, &this->perDatasetBin);
    if (status == 0) {
        if (this->perDatasetBin == 1)
            this->perDatasetAdc = 0;
        else
            status |= getIntegerParam(addr, AdscAdc, &this->perDatasetAdc);
    }
    status |= getIntegerParam(addr, AdscRaw, &this->perDatasetRaw);
    status |= getIntegerParam(addr, AdscImageTransform,
                              &this->perDatasetImageTransform);
    status |= getIntegerParam(addr, AdscStoredDarks,
                              &this->perDatasetStoredDarks);
    status |= getIntegerParam(addr, AdscAxis, &this->perDatasetAxis);
    status |= getIntegerParam(addr, ADImageMode, &this->perDatasetImageMode);
    if (status == 0) {
        if (this->perDatasetImageMode == ADImageSingle)
            this->perDatasetNumImages = 1;
        else
            status |= getIntegerParam(addr, ADNumImages,
                                      &this->perDatasetNumImages);
    }
    status |= getIntegerParam(addr, ADTriggerMode,
                              &this->perDatasetTriggerMode);
    status |= getDoubleParam(addr, ADAcquireTime,
                             &this->perDatasetExposureTime);
    status |= getDoubleParam(addr, ADAcquirePeriod,
                             &this->perDatasetAcquirePeriod);

    epicsMutexUnlock(this->mutexId);

    if (status == 0) return AdscStatusOk;

    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:loadPerDatasetParameters error, failed to read parameter "
              "values\n", driverName);
    return AdscStatusError;
}

AdscStatus_t adsc::loadPerImageParameters()
{
    int status = 0;
    int addr = 0;

    epicsMutexMustLock(this->mutexId);

    status |= getDoubleParam(addr, AdscBeamCenterX,
                             &this->perImageBeamCenterX);
    status |= getDoubleParam(addr, AdscBeamCenterY,
                             &this->perImageBeamCenterY);
    status |= getDoubleParam(addr, AdscDistance,
                             &this->perImageDistance);
    status |= getDoubleParam(addr, AdscTwoTheta,
                             &this->perImageTwoTheta);
    status |= getDoubleParam(addr, AdscWavelength,
                             &this->perImageWavelength);
    status |= getDoubleParam(addr, AdscImageWidth,
                             &this->perImageImageWidth);
    status |= getDoubleParam(addr, AdscPhi,
                             &this->perImagePhi);
    status |= getDoubleParam(addr, AdscOmega,
                             &this->perImageOmega);
    status |= getDoubleParam(addr, AdscKappa,
                             &this->perImageKappa);
    status |= createFileNameNoIncrement(this->perImageFullFileName,
                                        sizeof(this->perImageFullFileName));

    epicsMutexUnlock(this->mutexId);

    if (status == 0) return AdscStatusOk;

    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:loadPerImageParameters error, failed to read parameter "
              "values\n", driverName);
    return AdscStatusError;
}

static void imageAcquisitionTaskC(void *drvPvt)
{
    adsc *pPvt = (adsc *)drvPvt;
    pPvt->imageAcquisitionTask();
}

void adsc::imageAcquisitionTask()
{
    int status;

    for (;;) {
        status = epicsEventWait(this->startEventId);
        assert(status == epicsEventWaitOK);

        status |= loadPerDatasetParameters();
        if (status != 0) {
            acquisitionFinished(ADStatusError);
            continue;
        }

        status = acquireImages();
        acquisitionFinished((status == AdscStatusOk ||
                            status == AdscStatusInterrupt) ?
                                ADStatusIdle : ADStatusError);
    }
}

void adsc::acquisitionFinished(int adstatus)
{
    int status = 0;
    int addr = 0;

    epicsMutexMustLock(this->mutexId);

    status = setIntegerParam(addr, ADStatus, adstatus);
    status |= setIntegerParam(addr, ADAcquire, 0);
    status |= callParamCallbacks(addr, addr);

    epicsMutexUnlock(this->mutexId);

    this->lastImage = 0;

    if (status == 0) return;
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:acquisitionFinished error, failed to set ADStatus, "
              "ADAcquire to Done, or callParamCallbacks failed\n",
              driverName);
}

AdscStatus_t adsc::acquireImages()
{
    int status = 0;
    int status2 = 0;
    epicsTimeStamp startTime, endTime;
    double elapsedTime;
    double acquirePeriodDelay;
    int i;
    int imageKind;
    int numConsecutiveRetries;

    status = writeDetectorParametersBeforeDataset();
    if (status != AdscStatusOk) return AdscStatusError;

    status = loadPerImageParameters();
    if (status != AdscStatusOk) return AdscStatusError;

    status = takeDarksIfRequired();
    if (status != AdscStatusOk) return (AdscStatus_t)status;

    imageKind = this->perDatasetDezinger ? 4 : 5;

    numConsecutiveRetries = 0;
    for (i = 0; this->perDatasetImageMode == ADImageContinuous ||
            i < this->perDatasetNumImages;) {
        if (this->perDatasetTriggerMode == AdscTriggerStartExternal) {
            status = setOkToExpose(1);
            if (status != AdscStatusOk) return AdscStatusError;
            status = waitForExternalTrigger(this->startTriggerEventId);
            status2 = setOkToExpose(0);
            if (status != AdscStatusOk) return (AdscStatus_t)status;
            if (status2 != AdscStatusOk) return AdscStatusError;
        }

        status = epicsTimeGetCurrent(&startTime);
        assert(status == epicsTimeOK);

        if (i != 0) {
            status = loadPerImageParameters();
            if (status != AdscStatusOk) return AdscStatusError;
        }

        status = takeImage(this->perImageFullFileName, imageKind,
                           i == this->perDatasetNumImages - 1 ? 1 : 0,
                           this->perDatasetTriggerMode);
        if (status == AdscStatusAgain) {
            if (this->perDatasetTriggerMode == AdscTriggerStartExternal) {
                status = setExternalTriggerControl(
                                             AdscExternalTriggerControlAgain);
                if (status != AdscStatusOk) return (AdscStatus_t)status;
            } else {
                numConsecutiveRetries++;
                if (numConsecutiveRetries > 3) {
                    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                              "%s:acquireImages error, takeImage returned "
                              "AdscStatusAgain more than 3 consecutive "
                              "times\n", driverName);
                    return AdscStatusError;
                }
            }
            continue;
        } else if (status == AdscStatusOk) {
            if (this->perDatasetTriggerMode == AdscTriggerStartExternal) {
                status = setExternalTriggerControl(
                                                AdscExternalTriggerControlOk);
                if (status != AdscStatusOk) return (AdscStatus_t)status;
            } else {
                numConsecutiveRetries = 0;
            }
        } else {
            return (AdscStatus_t)status;
        }

        status = epicsTimeGetCurrent(&endTime);
        assert(status == epicsTimeOK);
        elapsedTime = epicsTimeDiffInSeconds(&endTime, &startTime);

        acquirePeriodDelay = this->perDatasetAcquirePeriod - elapsedTime;
        if (acquirePeriodDelay >= epicsThreadSleepQuantum()) {
            status = epicsEventWaitWithTimeout(this->stopEventId,
                                               acquirePeriodDelay);
            if (status != epicsEventWaitTimeout) {
                if (status == epicsEventWaitOK) {
                    return AdscStatusInterrupt;
                } else {
                    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                              "%s:acquireImages error, "
                              "epicsEventWaitWithTimeout returned with "
                              "status=%d\n", driverName, status);
                    return AdscStatusError;
                }
            }
        }

        if (imageKind == 5) {
            status = imageAcquired();
            if (status != AdscStatusOk) return AdscStatusError;
            i++;
        }

        if (this->perDatasetDezinger) imageKind = (imageKind == 4 ? 5 : 4);

        if (this->perDatasetImageMode == ADImageContinuous && this->lastImage)
            break;
    }

    return AdscStatusOk;
}

AdscStatus_t adsc::takeDarksIfRequired()
{
    char fullFileNameCopy[MAX_FILENAME_LEN];
    char *fullDirName=NULL;

    if (!shouldTakeDarks()) return AdscStatusOk;

    if (strlen(this->perImageFullFileName) == 0) {
        fullDirName = (char *)"";
    } else {
        strncpy(fullFileNameCopy, this->perImageFullFileName,
                sizeof(fullFileNameCopy) - 1);
        fullFileNameCopy[sizeof(fullFileNameCopy) - 1] = '\0';
        fullDirName = dirname(fullFileNameCopy);
        if (fullDirName == NULL) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s:takeDarksIfRequired error, dirname failed\n",
                      driverName);
            return AdscStatusError;
        }
    }
    return takeDarks(fullDirName);
}

AdscStatus_t adsc::createFileNameNoIncrement(char *dst, size_t dstSize)
{
    int status;
    int addr = 0;
    int autoIncrement;

    epicsMutexMustLock(this->mutexId);

    status = getIntegerParam(addr, ADAutoIncrement, &autoIncrement);
    if (status == 0) {
        status |= setIntegerParam(addr, ADAutoIncrement, 0);
        status |= createFileName(dstSize, dst);
        status |= setIntegerParam(addr, ADAutoIncrement, autoIncrement);
    }

    epicsMutexUnlock(this->mutexId);

    if (status == 0) return AdscStatusOk;

    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:createFileNameNoIncrement error, getting or setting "
              "ADAutoIncrement or ADUtils->createFileNameNoIncrement failed "
              "with ORed status=%d\n", driverName, status);
    return AdscStatusError;
}

AdscStatus_t adsc::setOkToExpose(int isEnabled)
{
    int status;
    int addr = 0;

    epicsMutexMustLock(this->mutexId);

    status = setIntegerParam(addr, AdscOkToExpose, isEnabled);
    status |= callParamCallbacks(addr, addr);

    epicsMutexUnlock(this->mutexId);

    if (status == asynSuccess) return AdscStatusOk;

    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s:setOkToExpose error, "
              "failed to set AdscOkToExpose or call callbacks\n", driverName);
    return AdscStatusError;
}

AdscStatus_t adsc::setExternalTriggerControl(AdscExternalTriggerControl_t
                                             value)
{
    int status;
    int addr = 0;

    epicsMutexMustLock(this->mutexId);

    status = setIntegerParam(addr, AdscExternalTriggerControl, value);
    status |= callParamCallbacks(addr, addr);

    epicsMutexUnlock(this->mutexId);

    if (status == asynSuccess) return AdscStatusOk;

    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:setExternalTriggerControl error, failed to set "
              "AdscExternalTriggerControl or call callbacks\n", driverName);
    return AdscStatusError;
}

AdscStatus_t adsc::waitForExternalTrigger(epicsEventId eventId)
{
    int status;

    status = epicsEventWait(eventId);
    assert(status == epicsEventWaitOK);
    status = epicsEventTryWait(this->stopEventId);
    if (status == epicsEventWaitTimeout) return AdscStatusOk;
    if (status == epicsEventWaitOK) return AdscStatusInterrupt;

    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:waitForExternalTrigger error, failed checking "
              "for interrupt signal after event signal\n", driverName);
    return AdscStatusError;
}

/* If destDir is an empty string, the acquisitions will take place, but the
 * images will not be written to disk. */
AdscStatus_t adsc::takeDarks(const char *destDir)
{
    float fexposureTime;
    int status;
    int numCharsPrinted;
    int darkIndex;
    char fullFileName[MAX_FILENAME_LEN];
    int numRetries;

    epicsMutexMustLock(this->mutexId);

    fexposureTime = (float)this->perDatasetExposureTime;
    status = CCDSetFilePar(FLP_TIME, (char *)&fexposureTime);

    epicsMutexUnlock(this->mutexId);

    if (status != 0) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s:takeDarks error, "
                  "CCDSetFilePar for FLP_TIME failed\n", driverName);
        return AdscStatusError;
    }

    if (strlen(destDir) == 0) {
        fullFileName[0] = '\0';
    } else {
        numCharsPrinted = epicsSnprintf(fullFileName, sizeof(fullFileName),
                                        "%s%sdark", destDir,
                                        PATH_COMPONENT_SEPARATOR);
        if (numCharsPrinted < 0 ||
                numCharsPrinted >= (int)sizeof(fullFileName)) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s:takeDarks "
                      "error, failed to create full file name\n", driverName);
            return AdscStatusError;
        }
    }

    clearLastDarksParameters();
    numRetries = 0;
    for (darkIndex = 0; darkIndex < 2;) {
        status = takeImage(fullFileName, darkIndex, 0,
                           AdscTriggerStartInternal);
        if (status == AdscStatusAgain) {
            numRetries++;
            if (numRetries > 3) {
                asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s:takeDarks "
                          "error, takeImage returned AdscStatusAgain more "
                          "than 3 times while trying to take darks\n",
                          driverName);
                return AdscStatusError;
            }
            continue;
        } else if (status != AdscStatusOk) {
            return AdscStatusError;
        }

        darkIndex++;
    }

    setLastDarksParameters(this->perDatasetExposureTime, this->perDatasetAdc,
                           this->perDatasetBin);
    return AdscStatusOk;
}

/* If fullFileName is an empty string, the acquisition will take place, but
 * the image will not be written to disk. */
AdscStatus_t adsc::takeImage(const char *fullFileName, int imageKind,
                             int lastImage, int triggerMode)
{
    int status, status2;
    int addr = 0;
    float fexposureTime;
    int wasAborted;
    int actualLastImage;

    status = writeDetectorParametersBeforeImage();
    if (status != AdscStatusOk) return AdscStatusError;

    epicsMutexMustLock(this->mutexId);
    fexposureTime = (float)this->perDatasetExposureTime;
    status = CCDSetFilePar(FLP_TIME, (char *)&fexposureTime);
    if (status != 0) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s:takeImage error, "
                  "CCDSetFilePar for FLP_TIME failed\n", driverName);
        epicsMutexUnlock(this->mutexId);
        return AdscStatusError;
    }
    status = CCDSetFilePar(FLP_KIND, (char *)&imageKind);
    if (status != 0) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s:takeImage error, "
                  "CCDSetFilePar for FLP_KIND failed\n", driverName);
        epicsMutexUnlock(this->mutexId);
        return AdscStatusError;
    }
    status = CCDSetFilePar(FLP_FILENAME, strlen(fullFileName) == 0 ?
                           "_null_" : fullFileName);
    if (status != 0) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s:takeImage error, "
                  "CCDSetFilePar for FLP_FILENAME failed\n", driverName);
        epicsMutexUnlock(this->mutexId);
        return AdscStatusError;
    }
    if (this->perDatasetStoredDarks) {
        status = CCDSetHwPar(HWP_STORED_DARK,
                             (char *)&this->perDatasetStoredDarks);
        if (status != 0) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s:takeImage "
                      "error, CCDSetHwPar for HWP_STORED_DARK failed\n",
                      driverName);
            epicsMutexUnlock(this->mutexId);
            return AdscStatusError;
        }
    }
    epicsMutexUnlock(this->mutexId);

    status = startExposure();
    if (status != AdscStatusOk) return (AdscStatus_t)status;

    /* XXX Open shutter behavior goes here.  Currently not implemented in
     * areaDetector base so the behavior has not been implemented here. */

    wasAborted = 0;
    if (triggerMode == AdscTriggerStartExternal) {
        status = waitForExternalTrigger(this->stopTriggerEventId);
        if (status == AdscStatusInterrupt) wasAborted = 1;
    } else {
        status = epicsEventWaitWithTimeout(this->stopEventId,
                                           this->perDatasetExposureTime);
        if (status == epicsEventWaitOK) wasAborted = 1;
    }
    if (wasAborted) {
        epicsMutexMustLock(this->mutexId);

        setIntegerParam(addr, ADStatus, ADStatusAborting);
        callParamCallbacks(addr, addr);

        epicsMutexUnlock(this->mutexId);
    }

    /* XXX Close shutter behavior goes here.  Currently not implemented in
     * areaDetector base so the behavior has not been implemented here. */

    status2 = stopExposure();

    if (triggerMode == AdscTriggerStartExternal) {
        if (status != AdscStatusOk) {
            epicsMutexMustLock(this->mutexId);
            CCDAbort();
            epicsMutexUnlock(this->mutexId);
            return (AdscStatus_t)status;
        }
    } else {
        if (status != epicsEventWaitTimeout) {
            epicsMutexMustLock(this->mutexId);
            CCDAbort();
            epicsMutexUnlock(this->mutexId);
            if (status == epicsEventWaitOK) {
                return AdscStatusInterrupt;
            } else {
                asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s:takeImage "
                          "error, epicsEventWaitWithTimeout returned "
                          "status=%d\n", driverName, status);
                return AdscStatusError;
            }
        }
    }

    if (status2 == AdscStatusAgain) {
        epicsMutexMustLock(this->mutexId);
        CCDReset();
        epicsMutexUnlock(this->mutexId);
        return (AdscStatus_t)status2;
    } else if (status2 != AdscStatusOk) {
        epicsMutexMustLock(this->mutexId);
        CCDAbort();
        epicsMutexUnlock(this->mutexId);
        return (AdscStatus_t)status2;
    }

    actualLastImage = lastImage;
    if (this->perDatasetImageMode == ADImageContinuous) {
        if (!this->lastImage) {
            status = epicsEventTryWait(this->lastImageEventId);
            if (status == epicsEventWaitOK) this->lastImage = 1;
         }
         if (imageKind != 0 && imageKind != 1)
             actualLastImage = this->lastImage ? 1 : 0;
    }

    return getImage(actualLastImage);
}

AdscStatus_t adsc::getImage(int lastImage)
{
    int status;

    epicsMutexMustLock(this->mutexId);
    status = CCDSetFilePar(FLP_LASTIMAGE, (char *)&lastImage);
    if (status != 0) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s:getImage error, "
                  "CCDSetFilePar for FLP_LASTIMAGE failed with return "
                  "status=%d\n", driverName, status);
        epicsMutexUnlock(this->mutexId);
        return AdscStatusError;
    }
    status = CCDGetImage();
    epicsMutexUnlock(this->mutexId);

    if (status == 0) return AdscStatusOk;

    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s:getImage error, "
              "CCDGetImage failed with return status=%d\n", driverName,
              status);
    return AdscStatusError;
}

AdscStatus_t adsc::startExposure()
{
    int status;
    int addr = 0;

    epicsMutexMustLock(this->mutexId);
    status = CCDStartExposure();
    epicsMutexUnlock(this->mutexId);
    if (status != 0) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s:startExposure "
                  "error, CCDStartExposure failed with return status=%d\n",
                  driverName, status);
        return AdscStatusError;
    }

    status = waitForDetectorState(DTC_STATE_EXPOSING, START_EXPOSURE_TIMEOUT,
                                  1);
    if (status != AdscStatusOk) return (AdscStatus_t)status;

    epicsMutexMustLock(this->mutexId);

    status = setIntegerParam(addr, ADStatus, ADStatusAcquire);
    status |= callParamCallbacks(addr, addr);

    epicsMutexUnlock(this->mutexId);

    if (status == asynSuccess) return AdscStatusOk;

    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s:startExposure error, "
              "setting ADStatus or calling callbacks failed\n", driverName);
    return AdscStatusError;
}

AdscStatus_t adsc::stopExposure()
{
    int status;
    int addr = 0;

    epicsMutexMustLock(this->mutexId);
    status = CCDStopExposure();
    epicsMutexUnlock(this->mutexId);
    if (status != 0) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s:stopExposure error, "
                  "CCDStopExposure failed with return status=%d\n",
                  driverName, status);
        return AdscStatusError;
    }

    status = waitForDetectorState(DTC_STATE_IDLE, STOP_EXPOSURE_TIMEOUT, 1);
    if (status == AdscStatusAgain) {
        epicsMutexMustLock(this->mutexId);

        setIntegerParam(addr, ADStatus, ADStatusIdle);
        callParamCallbacks(addr, addr);

        epicsMutexUnlock(this->mutexId);
        return (AdscStatus_t)status;
    } else if (status != AdscStatusOk) {
        return (AdscStatus_t)status;
    }

    epicsMutexMustLock(this->mutexId);

    status = setIntegerParam(addr, ADStatus, ADStatusIdle);
    status |= callParamCallbacks(addr, addr);

    epicsMutexUnlock(this->mutexId);

    if (status == asynSuccess) return AdscStatusOk;

    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s:stopExposure error, "
              "setting ADStatus or calling callbacks failed\n", driverName);
    return AdscStatusError;
}

AdscStatus_t adsc::waitForDetectorState(int desiredState, double timeout,
                                        int failOnErrorState)
{
    epicsTimeStamp startTime;
    epicsTimeStamp endTime;
    double elapsedTime;
    int status;
    int addr = 0;
    int state;
    int previousState;

    status = epicsTimeGetCurrent(&startTime);
    assert(status == epicsTimeOK);
    previousState = -1;
    for (;;) {
        epicsMutexMustLock(this->mutexId);
        state = CCDState();
        epicsMutexUnlock(this->mutexId);

        if (desiredState == state) break;

        if (state != previousState) {
            previousState = state;

            epicsMutexMustLock(this->mutexId);

            setIntegerParam(addr, AdscState, state);
            callParamCallbacks(addr, addr);

            epicsMutexUnlock(this->mutexId);
        }

        if (state == DTC_STATE_RETRY) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                      "%s:waitForDetectorState, got RETRY state, returning "
                      "AdscStatusAgain\n", driverName);
            return AdscStatusAgain;
        }

        if (failOnErrorState && state == DTC_STATE_ERROR) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s:waitForDetectorState error, detector state was "
                      "ERROR\n", driverName);
            return AdscStatusError;
        }

        status = epicsEventWaitWithTimeout(this->stopEventId,
                                           STATE_POLL_DELAY);
        if (status == epicsEventWaitTimeout) {
            status = epicsTimeGetCurrent(&endTime);
            assert(status == epicsTimeOK);
            elapsedTime = epicsTimeDiffInSeconds(&endTime, &startTime);
            if (elapsedTime > timeout) return AdscStatusTimeout;
        } else if (status == epicsEventWaitOK) {
            return AdscStatusInterrupt;
        } else {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s:waitForDetectorState error, "
                      "epicsEventWaitWithTimeout returned with status=%d\n",
                      driverName, status);
            return AdscStatusError;
        }
    }

    epicsMutexMustLock(this->mutexId);

    setIntegerParam(addr, AdscState, state);
    callParamCallbacks(addr, addr);

    epicsMutexUnlock(this->mutexId);

    return AdscStatusOk;
}

int adsc::shouldTakeDarks()
{
    if (this->perDatasetStoredDarks) return 0;
    if (!this->perDatasetReuseDarks) return 1;
    if (this->perDatasetExposureTime != this->lastDarksExposureTime) return 1;
    if (this->perDatasetAdc != this->lastDarksAdc) return 1;
    if (this->perDatasetBin != this->lastDarksBin) return 1;
    return 0;
}

AdscStatus_t adsc::imageAcquired()
{
    int status = 0;
    int addr = 0;
    int imageCounter;
    int fileNumber;
    int autoIncrement;

    epicsMutexMustLock(this->mutexId);

    status |= getIntegerParam(addr, ADImageCounter, &imageCounter);
    status |= getIntegerParam(addr, ADFileNumber, &fileNumber);
    status |= getIntegerParam(addr, ADAutoIncrement, &autoIncrement);
    if (status == 0) {
        imageCounter++;
        status |= setIntegerParam(addr, ADImageCounter, imageCounter);
        if (autoIncrement) {
            fileNumber++;
            status |= setIntegerParam(addr, ADFileNumber, fileNumber);
        }
        status |= setStringParam(addr, ADFullFileName,
                                 this->perImageFullFileName);
        status |= callParamCallbacks(addr, addr);
    }

    epicsMutexUnlock(this->mutexId);

    if (status == 0) return AdscStatusOk;

    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:imageAcquired error, failed while updating image counter "
              "value, next file number, or full file name\n", driverName);
    return AdscStatusError;
}

const char *adsc::getAdscCcdStateString(int ccdState)
{
    if (ccdState >= 0 && ccdState < NUM_ADSC_CCD_STATES)
        return AdscCcdStateStrings[ccdState];
    else
        return "unknown";
}

int adsc::getImageSize(AdscModel_t model, int binMode)
{
    if (binMode < 1 || binMode > 2) return -1;
    return (AdscSensors[model].xSize / binMode) *
           (AdscSensors[model].ySize / binMode) *
           (AdscSensors[model].bitsPerPixel / 8);
}

int adsc::getImageSizeX(AdscModel_t model, int binMode)
{
    if (binMode < 1 || binMode > 2) return -1;
    return AdscSensors[model].xSize / binMode;
}

int adsc::getImageSizeY(AdscModel_t model, int binMode)
{
    if (binMode < 1 || binMode > 2) return -1;
    return AdscSensors[model].ySize / binMode;
}

AdscStatus_t adsc::setBinModeInParams(int binMode)
{
    int status = 0;
    int addr = 0;

    if (binMode < 1 || binMode > 2) return AdscStatusError;

    epicsMutexMustLock(this->mutexId);

    status |= setIntegerParam(addr, ADBinX, binMode);
    status |= setIntegerParam(addr, ADBinY, binMode);
    status |= setIntegerParam(addr, ADImageSizeX,
                              getImageSizeX(this->model, binMode));
    status |= setIntegerParam(addr, ADImageSizeY,
                              getImageSizeY(this->model, binMode));
    status |= setIntegerParam(addr, ADImageSize,
                              getImageSize(this->model, binMode));

    epicsMutexUnlock(this->mutexId);

    return status == 0 ? AdscStatusOk : AdscStatusError;
}
