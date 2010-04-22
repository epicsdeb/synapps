/*
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
#include <iocsh.h>
#include <epicsExport.h>

#include "ADDriver.h"

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

/** Status choices */
typedef enum {
    AdscStatusOk,
    AdscStatusInterrupt,
    AdscStatusTimeout,
    AdscStatusAgain,
    AdscStatusError
} AdscStatus_t;

/** Trigger choices */
typedef enum {
    AdscExternSwTriggerControlStop,
    AdscExternSwTriggerControlStart,
    AdscExternSwTriggerControlOk,
    AdscExternSwTriggerControlAgain
} AdscExternSwTriggerControl_t;

/** Model choices */
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

/** ADSC sensor structure */
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

/** These parameters describe the trigger modes of this driver; they must
  * agree with the values in the mbbo/mbbi records in the adsc.template
  * database (or the ADBase.template database if the default is used) */
typedef enum {
    AdscTriggerStartInternal,
    AdscTriggerStartExternSw
} AdscTriggerStartMode_t;

static const char *AdscTriggerStartStrings[] = {
    "Internal","Ext. Software"
};

#define NUM_START_TRIGGER_MODES ((int)(sizeof(AdscTriggerStartStrings) / \
    sizeof(AdscTriggerStartStrings[0])))

/** Driver-specific parameters for the ADSC driver */
#define AdscReadConditionString          "ADSC_READ_CONDITION"
#define AdscStateString                  "ADSC_STATE"
#define AdscStatusString                 "ADSC_STATUS"
#define AdscLastErrorString              "ADSC_LAST_ERROR"
#define AdscSoftwareResetString          "ADSC_SOFTWARE_RESET"
#define AdscLastImageString              "ADSC_LAST_IMAGE"
#define AdscOkToExposeString             "ADSC_OK_TO_EXPOSE"
#define AdscExternSwTriggerControlString "ADSC_EXTERN_SW_TRIGGER_CTRL"
#define AdscReuseDarksString             "ADSC_REUSE_DARKS"
#define AdscDezingerString               "ADSC_DEZINGER"
#define AdscAdcString                    "ADSC_ADC"
#define AdscRawString                    "ADSC_RAW"
#define AdscImageTransformString         "ADSC_IMAGE_TRANSFORM"
#define AdscStoredDarksString            "ADSC_STORED_DARKS"
#define AdscBeamCenterXString            "ADSC_BEAM_CENTER_X"
#define AdscBeamCenterYString            "ADSC_BEAM_CENTER_Y"
#define AdscDistanceString               "ADSC_DISTANCE"
#define AdscTwoThetaString               "ADSC_TWO_THETA"
#define AdscAxisString                   "ADSC_AXIS"
#define AdscWavelengthString             "ADSC_WAVELENGTH"
#define AdscImageWidthString             "ADSC_IMAGE_WIDTH"
#define AdscPhiString                    "ADSC_PHI"
#define AdscOmegaString                  "ADSC_OMEGA"
#define AdscPrivateStopExpRetryCntString "ADSC_PRVT_STOP_EXP_RTRY_CNT"
#define AdscKappaString                  "ADSC_KAPPA"


static void imageAcquisitionTaskC(void *drvPvt);

/** Driver for ADSC detectors (Q4, Q4r, Q210, Q210r, Q270, Q315, Q315r).
*/
class adsc : public ADDriver {
public:
    adsc(const char *portName, const char *modelName);
    virtual ~adsc();

    /* These are the methods that we override from ADDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    void report(FILE *fp, int details);

    /* These are the methods that are new to this class */
    void imageAcquisitionTask();
protected:
    int AdscReadCondition;
    #define FIRST_ADSC_PARAM AdscReadCondition
    int AdscState;
    int AdscStatus;
    int AdscLastError;
    int AdscSoftwareReset;
    int AdscLastImage;
    int AdscOkToExpose;
    int AdscExternSwTriggerControl;
    int AdscReuseDarks;
    int AdscDezinger;
    int AdscAdc;
    int AdscRaw;
    int AdscImageTransform;
    int AdscStoredDarks;
    int AdscBeamCenterX;
    int AdscBeamCenterY;
    int AdscDistance;
    int AdscTwoTheta;
    int AdscAxis;
    int AdscWavelength;
    int AdscImageWidth;
    int AdscPhi;
    int AdscOmega;
    int AdscKappa;
    int AdscPrivateStopExpRetryCnt;
    #define LAST_ADSC_PARAM AdscPrivateStopExpRetryCnt


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
    AdscStatus_t setExternSwTriggerControl(AdscExternSwTriggerControl_t
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
#define NUM_ADSC_PARAMS (&LAST_ADSC_PARAM - &FIRST_ADSC_PARAM + 1)


/** Called when asyn clients call pasynInt32->write().
  * This function performs actions for some parameters, including ADAcquire,
  * ADBinX, etc. For all parameters it sets the value in the parameter library
  * and calls any registered callbacks.
  * \param[in] pasynUser pasynUser structure that encodes the reason and
  *                      address
  * \param[in] value Value to write */
asynStatus adsc::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function = pasynUser->reason;
    int status = asynSuccess;
    int addr = 0;
    int acquire;
    int adstatus;
    const char *functionName = "writeInt32";

    if ((function == ADBinX) ||
        (function == ADBinY)) {
            if (value < 1 || value > 2) {
                status = asynError;
                goto done;
            }
            status |= setBinModeInParams(value);
    } else if (function == ADImageMode) {
            if (value != ADImageSingle && value != ADImageMultiple &&
                    value != ADImageContinuous) {
                status = asynError;
                goto done;
            }
            status |= setIntegerParam(addr, ADImageMode, value);
    } else if (function == ADTriggerMode) {
            if (value < 0 || value >= NUM_START_TRIGGER_MODES) {
                status = asynError;
                goto done;
            }
            status |= setIntegerParam(addr, ADTriggerMode, value);
    } else if (function == ADNumImages) {
            if (value < 0) {
                status = asynError;
                goto done;
            }
            status |= setIntegerParam(addr, ADNumImages, value);
    } else if (function == ADAcquire) {
            if (value < 0 || value > 1) {
                status = asynError;
                goto done;
            }

            status |= getIntegerParam(addr, ADAcquire, &acquire);
            status |= getIntegerParam(addr, ADStatus, &adstatus);
            if (status != asynSuccess) {
                status = asynError;
                goto done;
            }
            if (value == acquire) goto done;

            status |= setIntegerParam(addr, ADAcquire, value);
            if (status != asynSuccess) {
                status = asynError;
                goto done;
            }

            if (value == 1) {
                /* If detector is in an error state, attempt auto-recovery */
                if (adstatus == ADStatusError ||
                        CCDState() == DTC_STATE_ERROR ||
                        strcmp("", CCDGetLastError()) != 0) {
                    status |= resetControlLibrary();
                    if (status != 0) {
                        status = asynError;
                        goto done;
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
    } else if (function == NDArrayCounter) {
            if (value < 0) {
                status = asynError;
                goto done;
            }
            status |= setIntegerParam(addr, NDArrayCounter, value);
    } else if (function == NDFileNumber) {
            if (value < 0) {
                status = asynError;
                goto done;
            }
            status |= setIntegerParam(addr, NDFileNumber, value);
    } else if (function == NDAutoIncrement) {
            if (value != 0 && value != 1) {
                status = asynError;
                goto done;
            }
            status |= setIntegerParam(addr, NDAutoIncrement, value);
    } else if (function == AdscReadCondition) {
            status |= readDetectorCondition();
    } else if (function == AdscSoftwareReset) {
            status |= getIntegerParam(addr, ADAcquire, &acquire);
            if (status != 0 || acquire == 1) goto done;
            status |= resetControlLibrary();
    } else if (function == AdscLastImage) {
            status |= getIntegerParam(addr, ADAcquire, &acquire);
            if (status != 0 || acquire == 0) goto done;
            epicsEventSignal(this->lastImageEventId);
    } else if (function == AdscExternSwTriggerControl) {
            if (value < 0 || value > 3) {
                status = asynError;
                goto done;
            }
            status |= getIntegerParam(addr, ADAcquire, &acquire);
            if (status != 0 || acquire == 0) goto done;
            status |= setIntegerParam(addr, AdscExternSwTriggerControl,
                                      value);
            if (value == AdscExternSwTriggerControlStart)
                epicsEventSignal(this->startTriggerEventId);
            else if (value == AdscExternSwTriggerControlStop)
                epicsEventSignal(this->stopTriggerEventId);
    } else if (function == AdscReuseDarks) {
            if (value != 0 && value != 1) {
                status = asynError;
                goto done;
            }
            status |= setIntegerParam(addr, AdscReuseDarks, value);
    } else if (function == AdscDezinger) {
            if (value != 0 && value != 1) {
                status = asynError;
                goto done;
            }
            status |= setIntegerParam(addr, AdscDezinger, value);
    } else if (function == AdscAdc) {
            if (value != 0 && value != 1) {
                status = asynError;
                goto done;
            }
            status |= setIntegerParam(addr, AdscAdc, value);
    } else if (function == AdscRaw) {
            if (value != 0 && value != 1) {
                status = asynError;
                goto done;
            }
            status |= setIntegerParam(addr, AdscRaw, value);
    } else if (function == AdscImageTransform) {
            if (value != 0 && value != 1) {
                status = asynError;
                goto done;
            }
            status |= setIntegerParam(addr, AdscImageTransform, value);
    } else if (function == AdscStoredDarks) {
            if (value != 0 && value != 1) {
                status = asynError;
                goto done;
            }
            status |= setIntegerParam(addr, AdscStoredDarks, value);
            if (value == 0) clearLastDarksParameters();
    } else if (function == AdscAxis) {
            if (value != 0 && value != 1) {
                status = asynError;
                goto done;
            }
            status |= setIntegerParam(addr, AdscAxis, value);
    } else if (function == AdscPrivateStopExpRetryCnt) {
            #ifdef USE_SIMADSC
                simadscSetStopExposureRetryCount(value);
            #endif
            goto done;
    } else {
            /* If this is not a parameter we have handled, call the base
             * class */
            if (function < FIRST_ADSC_PARAM)
                status = ADDriver::writeInt32(pasynUser, value);
    }

    done:
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

/** Called when asyn clients call pasynFloat64->write().
  * This function performs actions for some parameters, including
  * ADAcquireTime, AdscTwoTheta, etc. For all parameters it sets the value in
  * the parameter library and calls any registered callbacks.
  * \param[in] pasynUser pasynUser structure that encodes the reason and
  *                      address
  * \param[in] value Value to write */
asynStatus adsc::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
    int function = pasynUser->reason;
    int status = asynSuccess;
    int addr = 0;
    const char *functionName = "writeFloat64";

    if (function == ADAcquireTime) {
            if (value < 0.0) {
                status = asynError;
                goto done;
            }
            status |= setDoubleParam(addr, ADAcquireTime, value);
    } else if (function == ADAcquirePeriod) {
            if (value < 0.0) {
                status = asynError;
                goto done;
            }
            status |= setDoubleParam(addr, ADAcquirePeriod, value);
    } else if (function == AdscBeamCenterX) {
            status |= setDoubleParam(addr, AdscBeamCenterX, value);
    } else if (function == AdscBeamCenterY) {
            status |= setDoubleParam(addr, AdscBeamCenterY, value);
    } else if (function == AdscDistance) {
            if (value < 0.0) {
                status = asynError;
                goto done;
            }
            status |= setDoubleParam(addr, AdscDistance, value);
    } else if (function == AdscTwoTheta) {
            status |= setDoubleParam(addr, AdscTwoTheta, value);
    } else if (function == AdscWavelength) {
            if (value < 0.0) {
                status = asynError;
                goto done;
            }
            status |= setDoubleParam(addr, AdscWavelength, value);
    } else if (function == AdscImageWidth) {
            status |= setDoubleParam(addr, AdscImageWidth, value);
    } else if (function == AdscPhi) {
            status |= setDoubleParam(addr, AdscPhi, value);
    } else if (function == AdscOmega) {
            status |= setDoubleParam(addr, AdscOmega, value);
    } else if (function == AdscKappa) {
            status |= setDoubleParam(addr, AdscKappa, value);
    } else {
            /* If this is not a parameter we have handled, call the base
             * class */
            if (function < FIRST_ADSC_PARAM)
                status = ADDriver::writeFloat64(pasynUser, value);
    }
    
    done:

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

/** Report status of the driver.
  * Prints details about the driver if details>0.
  * It then calls the ADDriver::report() method.
  * \param[in] fp File pointed passed by caller where the output is written to
  * \param[in] details If >0 then driver details are printed
  */
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

/** Constructor for ADSC driver; most parameters are simply passed to
  * ADDriver::ADDriver.
  * After calling the base class constructor, this method creates a thread to
  * collect the detector data and sets reasonable default values for the
  * parameters defined in this class, asynNDArrayDriver and ADDriver.
  * \param[in] portName The name of the asyn port driver to be created
  * \param[in] modelName The model name of the detector being used; choices
  *            are "Q4", "Q4r", "Q210", "Q210r", "Q270", "Q315", or "Q315r"
  */
adsc::adsc(const char *portName, const char *modelName)
    : ADDriver(portName, 1, NUM_ADSC_PARAMS,
               0, 0,             /* maxBuffers and maxMemory are 0 because we
                                  * don't support NDArray callbacks yet */
               0, 0,             /* No interfaces beyond those set in
                                  * ADDriver.cpp */
               ASYN_CANBLOCK, 1, /* ASYN_CANBLOCK=1, ASYN_MULTIDEVICE=0,
                                  * autoConnect=1 */
               0, 0)             /* priority and stackSize=0, so use
                                  * defaults */
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

    createParam(AdscReadConditionString,           asynParamInt32,   &AdscReadCondition);
    createParam(AdscStateString,                   asynParamInt32,   &AdscState);
    createParam(AdscStatusString,                  asynParamOctet,   &AdscStatus);
    createParam(AdscLastErrorString,               asynParamOctet,   &AdscLastError);
    createParam(AdscSoftwareResetString,           asynParamInt32,   &AdscSoftwareReset);
    createParam(AdscLastImageString,               asynParamInt32,   &AdscLastImage);
    createParam(AdscOkToExposeString,              asynParamInt32,   &AdscOkToExpose);
    createParam(AdscExternSwTriggerControlString,  asynParamInt32,   &AdscExternSwTriggerControl);
    createParam(AdscReuseDarksString,              asynParamInt32,   &AdscReuseDarks);
    createParam(AdscDezingerString,                asynParamInt32,   &AdscDezinger);
    createParam(AdscAdcString,                     asynParamInt32,   &AdscAdc);
    createParam(AdscRawString,                     asynParamInt32,   &AdscRaw);
    createParam(AdscImageTransformString,          asynParamInt32,   &AdscImageTransform);
    createParam(AdscStoredDarksString,             asynParamInt32,   &AdscStoredDarks);
    createParam(AdscBeamCenterXString,             asynParamFloat64, &AdscBeamCenterX);
    createParam(AdscBeamCenterYString,             asynParamFloat64, &AdscBeamCenterY);
    createParam(AdscDistanceString,                asynParamFloat64, &AdscDistance);
    createParam(AdscTwoThetaString,                asynParamFloat64, &AdscTwoTheta);
    createParam(AdscAxisString,                    asynParamInt32,   &AdscAxis);
    createParam(AdscWavelengthString,              asynParamFloat64, &AdscWavelength);
    createParam(AdscImageWidthString,              asynParamFloat64, &AdscImageWidth);
    createParam(AdscPhiString,                     asynParamFloat64, &AdscPhi);
    createParam(AdscOmegaString,                   asynParamFloat64, &AdscOmega);
    createParam(AdscKappaString,                   asynParamFloat64, &AdscKappa);
    createParam(AdscPrivateStopExpRetryCntString,  asynParamInt32,   &AdscPrivateStopExpRetryCnt);

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
    status |= setStringParam (addr, ADManufacturer, "ADSC");
    status |= setStringParam (addr, ADModel, AdscModelStrings[this->model]);
    status |= setIntegerParam(addr, ADSizeX, AdscSensors[this->model].xSize);
    status |= setIntegerParam(addr, ADSizeY, AdscSensors[this->model].ySize);
    status |= setIntegerParam(addr, ADMaxSizeX,
                              AdscSensors[this->model].xSize);
    status |= setIntegerParam(addr, ADMaxSizeY,
                              AdscSensors[this->model].ySize);
    status |= setBinModeInParams(1);
    status |= setIntegerParam(addr, NDDataType, NDUInt16);
    status |= setIntegerParam(addr, NDAutoSave, 1);
    status |= setDoubleParam (addr, ADAcquireTime, 1.0);
    status |= setDoubleParam (addr, ADAcquirePeriod, 0.0);
    status |= setIntegerParam(addr, ADNumImages, 1);
    status |= setIntegerParam(addr, ADImageMode, ADImageSingle);
    status |= setIntegerParam(addr, AdscReadCondition, 0);
    status |= setIntegerParam(addr, AdscSoftwareReset, 0);
    status |= setIntegerParam(addr, AdscLastImage, 0);
    status |= setIntegerParam(addr, AdscOkToExpose, 0);
    status |= setIntegerParam(addr, AdscExternSwTriggerControl,
                              AdscExternSwTriggerControlOk);
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

    this->lock();

    status |= CCDAbort();
    status |= CCDReset();
    status |= setIntegerParam(addr, ADStatus, ADStatusIdle);
    status |= readDetectorCondition();

    this->unlock();

    if (status == 0) return AdscStatusOk;

    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s:resetControlLibrary "
              "error, status=%d\n", driverName, status);
    return AdscStatusError;
}

AdscStatus_t adsc::readDetectorCondition()
{
    int status = 0;
    int addr = 0;

    this->lock();

    status |= setIntegerParam(addr, AdscState, CCDState());
    status |= setStringParam(addr, AdscStatus, CCDStatus());
    status |= setStringParam(addr, AdscLastError, CCDGetLastError());

    this->unlock();

    if (status == 0) return AdscStatusOk;

    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:readDetectorCondition error, status=%d\n", driverName,
              status);
    return AdscStatusError;
}

AdscStatus_t adsc::writeDetectorParametersBeforeDataset()
{
    int status = 0;
    int ival;

    this->lock();

    status |= CCDSetHwPar(HWP_BIN, (char *)&this->perDatasetBin);
    status |= CCDSetHwPar(HWP_ADC, (char *)&this->perDatasetAdc);
    status |= CCDSetHwPar(HWP_SAVE_RAW, (char *)&this->perDatasetRaw);
    ival = this->perDatasetImageTransform == 0 ? 1 : 0;
    status |= CCDSetHwPar(HWP_NO_XFORM, (char *)&ival);
    status |= CCDSetHwPar(HWP_STORED_DARK,
                          (char *)&this->perDatasetStoredDarks);
    status |= CCDSetFilePar(FLP_AXIS, (char *)&this->perDatasetAxis);

    this->unlock();

    if (status == 0) return AdscStatusOk;

    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:writeDetectorParameters error, status=%d\n", driverName,
              status);
    return AdscStatusError;
}

AdscStatus_t adsc::writeDetectorParametersBeforeImage()
{
    int status = 0;
    float fval;

    this->lock();

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

    this->unlock();

    if (status == 0) return AdscStatusOk;

    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:writeDetectorParameters error, status=%d\n", driverName,
              status);
    return AdscStatusError;
}

AdscStatus_t adsc::loadPerDatasetParameters()
{
    int status = 0;
    int addr = 0;

    this->lock();

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

    this->unlock();

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

    this->lock();

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

    this->unlock();

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

    this->lock();

    status = setIntegerParam(addr, ADStatus, adstatus);
    status |= setIntegerParam(addr, ADAcquire, 0);
    status |= callParamCallbacks(addr, addr);

    this->unlock();

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
        if (this->perDatasetTriggerMode == AdscTriggerStartExternSw) {
            status = setOkToExpose(1);
            if (status != AdscStatusOk) return AdscStatusError;
            status = waitForExternalTrigger(this->startTriggerEventId);
            status2 = setOkToExpose(0);
            if (status != AdscStatusOk) return (AdscStatus_t)status;
            if (status2 != AdscStatusOk) return AdscStatusError;
        }

        status = epicsTimeGetCurrent(&startTime);
        assert(status == epicsTimeOK);

        status = loadPerImageParameters();
        if (status != AdscStatusOk) return AdscStatusError;

        status = takeImage(this->perImageFullFileName, imageKind,
                           i == this->perDatasetNumImages - 1 ? 1 : 0,
                           this->perDatasetTriggerMode);
        if (status == AdscStatusAgain) {
            if (this->perDatasetTriggerMode == AdscTriggerStartExternSw) {
                status = setExternSwTriggerControl(
                                             AdscExternSwTriggerControlAgain);
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
            if (this->perDatasetTriggerMode == AdscTriggerStartExternSw) {
                status = setExternSwTriggerControl(
                                                AdscExternSwTriggerControlOk);
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

    this->lock();

    status = getIntegerParam(addr, NDAutoIncrement, &autoIncrement);
    if (status == 0) {
        status |= setIntegerParam(addr, NDAutoIncrement, 0);
        status |= createFileName(dstSize, dst);
        status |= setIntegerParam(addr, NDAutoIncrement, autoIncrement);
    }

    this->unlock();

    if (status == 0) return AdscStatusOk;

    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:createFileNameNoIncrement error, getting or setting "
              "ADAutoIncrement, or asynNDArrayDriver::createFileName failed "
              "with ORed status=%d\n", driverName, status);
    return AdscStatusError;
}

AdscStatus_t adsc::setOkToExpose(int isEnabled)
{
    int status;
    int addr = 0;

    this->lock();

    status = setIntegerParam(addr, AdscOkToExpose, isEnabled);
    status |= callParamCallbacks(addr, addr);

    this->unlock();

    if (status == asynSuccess) return AdscStatusOk;

    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:setOkToExpose error, failed to set AdscOkToExpose or call "
              "callbacks\n", driverName);
    return AdscStatusError;
}

AdscStatus_t adsc::setExternSwTriggerControl(AdscExternSwTriggerControl_t
                                             value)
{
    int status;
    int addr = 0;

    this->lock();

    status = setIntegerParam(addr, AdscExternSwTriggerControl, value);
    status |= callParamCallbacks(addr, addr);

    this->unlock();

    if (status == asynSuccess) return AdscStatusOk;

    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:setExternSwTriggerControl error, failed to set "
              "AdscExternSwTriggerControl or call callbacks\n", driverName);
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

    this->lock();

    fexposureTime = (float)this->perDatasetExposureTime;
    status = CCDSetFilePar(FLP_TIME, (char *)&fexposureTime);

    this->unlock();

    if (status != 0) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s:takeDarks error, CCDSetFilePar for FLP_TIME failed\n",
                  driverName);
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
                asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                          "%s:takeDarks error, takeImage returned "
                          "AdscStatusAgain more than 3 times while trying to "
                          "take darks\n", driverName);
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

    this->lock();
    fexposureTime = (float)this->perDatasetExposureTime;
    status = CCDSetFilePar(FLP_TIME, (char *)&fexposureTime);
    if (status != 0) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s:takeImage error, CCDSetFilePar for FLP_TIME failed\n",
                  driverName);
        this->unlock();
        return AdscStatusError;
    }
    status = CCDSetFilePar(FLP_KIND, (char *)&imageKind);
    if (status != 0) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s:takeImage error, CCDSetFilePar for FLP_KIND failed\n",
                  driverName);
        this->unlock();
        return AdscStatusError;
    }
    status = CCDSetFilePar(FLP_FILENAME, strlen(fullFileName) == 0 ?
                           "_null_" : fullFileName);
    if (status != 0) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s:takeImage error, CCDSetFilePar for FLP_FILENAME "
                  "failed\n", driverName);
        this->unlock();
        return AdscStatusError;
    }
    if (this->perDatasetStoredDarks) {
        status = CCDSetHwPar(HWP_STORED_DARK,
                             (char *)&this->perDatasetStoredDarks);
        if (status != 0) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s:takeImage "
                      "error, CCDSetHwPar for HWP_STORED_DARK failed\n",
                      driverName);
            this->unlock();
            return AdscStatusError;
        }
    }
    this->unlock();

    status = startExposure();
    if (status != AdscStatusOk) return (AdscStatus_t)status;

    /* XXX Open shutter behavior goes here.  Currently not implemented in
     * areaDetector base so the behavior has not been implemented here. */

    wasAborted = 0;
    if (triggerMode == AdscTriggerStartExternSw) {
        status = waitForExternalTrigger(this->stopTriggerEventId);
        if (status == AdscStatusInterrupt) wasAborted = 1;
    } else {
        status = epicsEventWaitWithTimeout(this->stopEventId,
                                           this->perDatasetExposureTime);
        if (status == epicsEventWaitOK) wasAborted = 1;
    }
    if (wasAborted) {
        this->lock();

        setIntegerParam(addr, ADStatus, ADStatusAborting);
        callParamCallbacks(addr, addr);

        this->unlock();
    }

    /* XXX Close shutter behavior goes here.  Currently not implemented in
     * areaDetector base so the behavior has not been implemented here. */

    status2 = stopExposure();

    if (triggerMode == AdscTriggerStartExternSw) {
        if (status != AdscStatusOk) {
            this->lock();
            CCDAbort();
            this->unlock();
            return (AdscStatus_t)status;
        }
    } else {
        if (status != epicsEventWaitTimeout) {
            this->lock();
            CCDAbort();
            this->unlock();
            if (status == epicsEventWaitOK) {
                return AdscStatusInterrupt;
            } else {
                asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                          "%s:takeImage error, epicsEventWaitWithTimeout "
                          "returned status=%d\n", driverName, status);
                return AdscStatusError;
            }
        }
    }

    if (status2 == AdscStatusAgain) {
        this->lock();
        CCDReset();
        this->unlock();
        return (AdscStatus_t)status2;
    } else if (status2 != AdscStatusOk) {
        this->lock();
        CCDAbort();
        this->unlock();
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

    this->lock();
    status = CCDSetFilePar(FLP_LASTIMAGE, (char *)&lastImage);
    if (status != 0) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s:getImage error, "
                  "CCDSetFilePar for FLP_LASTIMAGE failed with return "
                  "status=%d\n", driverName, status);
        this->unlock();
        return AdscStatusError;
    }
    status = CCDGetImage();
    this->unlock();

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

    this->lock();
    status = CCDStartExposure();
    this->unlock();
    if (status != 0) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s:startExposure "
                  "error, CCDStartExposure failed with return status=%d\n",
                  driverName, status);
        return AdscStatusError;
    }

    status = waitForDetectorState(DTC_STATE_EXPOSING, START_EXPOSURE_TIMEOUT,
                                  1);
    if (status != AdscStatusOk) return (AdscStatus_t)status;

    this->lock();

    status = setIntegerParam(addr, ADStatus, ADStatusAcquire);
    status |= callParamCallbacks(addr, addr);

    this->unlock();

    if (status == asynSuccess) return AdscStatusOk;

    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:startExposure error, setting ADStatus or calling callbacks "
              "failed\n", driverName);
    return AdscStatusError;
}

AdscStatus_t adsc::stopExposure()
{
    int status;
    int addr = 0;

    this->lock();
    status = CCDStopExposure();
    this->unlock();
    if (status != 0) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s:stopExposure error, CCDStopExposure failed with return "
                  "status=%d\n", driverName, status);
        return AdscStatusError;
    }

    status = waitForDetectorState(DTC_STATE_IDLE, STOP_EXPOSURE_TIMEOUT, 1);
    if (status == AdscStatusAgain) {
        this->lock();

        setIntegerParam(addr, ADStatus, ADStatusIdle);
        callParamCallbacks(addr, addr);

        this->unlock();
        return (AdscStatus_t)status;
    } else if (status != AdscStatusOk) {
        return (AdscStatus_t)status;
    }

    this->lock();

    status = setIntegerParam(addr, ADStatus, ADStatusIdle);
    status |= callParamCallbacks(addr, addr);

    this->unlock();

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
        this->lock();
        state = CCDState();
        this->unlock();

        if (desiredState == state) break;

        if (state != previousState) {
            previousState = state;

            this->lock();

            setIntegerParam(addr, AdscState, state);
            callParamCallbacks(addr, addr);

            this->unlock();
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

    this->lock();

    setIntegerParam(addr, AdscState, state);
    callParamCallbacks(addr, addr);

    this->unlock();

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

    this->lock();

    status |= getIntegerParam(addr, NDArrayCounter, &imageCounter);
    status |= getIntegerParam(addr, NDFileNumber, &fileNumber);
    status |= getIntegerParam(addr, NDAutoIncrement, &autoIncrement);
    if (status == 0) {
        imageCounter++;
        status |= setIntegerParam(addr, NDArrayCounter, imageCounter);
        if (autoIncrement) {
            fileNumber++;
            status |= setIntegerParam(addr, NDFileNumber, fileNumber);
        }
        status |= setStringParam(addr, NDFullFileName,
                                 this->perImageFullFileName);
        status |= callParamCallbacks(addr, addr);
    }

    this->unlock();

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

    this->lock();

    status |= setIntegerParam(addr, ADBinX, binMode);
    status |= setIntegerParam(addr, ADBinY, binMode);
    status |= setIntegerParam(addr, NDArraySizeX,
                              getImageSizeX(this->model, binMode));
    status |= setIntegerParam(addr, NDArraySizeY,
                              getImageSizeY(this->model, binMode));
    status |= setIntegerParam(addr, NDArraySize,
                              getImageSize(this->model, binMode));

    this->unlock();

    return status == 0 ? AdscStatusOk : AdscStatusError;
}

/* Code for iocsh registration */
static const iocshArg adscConfigArg0  = {"Port name", iocshArgString};
static const iocshArg adscConfigArg1  = {"Model name", iocshArgString};
static const iocshArg * const adscConfigArgs[2] = {&adscConfigArg0,
                                                   &adscConfigArg1};
static const iocshFuncDef configadsc = {"adscConfig", 2, adscConfigArgs};
static void configadscCallFunc(const iocshArgBuf *args)
{
    adscConfig(args[0].sval, args[1].sval);
}

static void adscRegister(void)
{
    iocshRegister(&configadsc, configadscCallFunc);
}

extern "C" {
epicsExportRegistrar(adscRegister);
}
