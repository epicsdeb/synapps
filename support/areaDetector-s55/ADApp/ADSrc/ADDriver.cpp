/** ADDriver.cpp
 *
 * This is the base class from which actual area detectors are derived.
 *
 * /author Author: Mark Rivers
 *         University of Chicago
 *
 * Created:  March 20, 2008
 *
 */

#include <stdio.h>

#include <epicsString.h>
#include <epicsThread.h>
#include <asynStandardInterfaces.h>

#include "ADDriver.h"

static const char *driverName = "ADDriver";

/** Set the shutter position.
  * This method will open (1) or close (0) the shutter if
  * ADShutterMode==ADShutterModeEPICS. Drivers will implement setShutter if they
  * support ADShutterModeDetector. If ADShutterMode=ADShutterModeDetector they will
  * control the shutter directly, else they will call this method.
  * \param[in] open 1 (open) or 0 (closed)
  */
void ADDriver::setShutter(int open)
{
    ADShutterMode_t shutterMode;
    double delay;
    double shutterOpenDelay, shutterCloseDelay;

    getIntegerParam(ADShutterMode, (int *)&shutterMode);
    getDoubleParam(ADShutterOpenDelay, &shutterOpenDelay);
    getDoubleParam(ADShutterCloseDelay, &shutterCloseDelay);

    switch (shutterMode) {
        case ADShutterModeNone:
            break;
        case ADShutterModeEPICS:
            setIntegerParam(ADShutterControlEPICS, open);
            callParamCallbacks();
            delay = shutterOpenDelay - shutterCloseDelay;
            epicsThreadSleep(delay);
            break;
        case ADShutterModeDetector:
            break;
    }
}

/** Sets an int32 parameter.
  * \param[in] pasynUser asynUser structure that contains the function code in pasynUser->reason. 
  * \param[in] value The value for this parameter 
  *
  * Takes action if the function code requires it.  Currently only ADShutterControl requires
  * action here.  This method is normally called from the writeInt32 method in derived classes, which
  * should set the value of the parameter in the parameter library. */
asynStatus ADDriver::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char *functionName = "writeInt32";

    status = setIntegerParam(function, value);

    if (function == ADShutterControl) {
        setShutter(value);
    } else {
        /* If this parameter belongs to a base class call its method */
        if (function < FIRST_AD_PARAM) status = asynNDArrayDriver::writeInt32(pasynUser, value);
    }

    /* Do callbacks so higher layers see any changes */
    callParamCallbacks();

    if (status)
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "%s:%s: error, status=%d function=%d, value=%d\n",
              driverName, functionName, status, function, value);
    else
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "%s:%s: function=%d, value=%d\n",
              driverName, functionName, function, value);
    return status;
}


/** All of the arguments are simply passed to
  * the constructor for the asynNDArrayDriver base class. After calling the base class
  * constructor this method sets reasonable default values for all of the parameters
  * defined in ADDriver.h.
  */
ADDriver::ADDriver(const char *portName, int maxAddr, int numParams, int maxBuffers, size_t maxMemory,
                   int interfaceMask, int interruptMask,
                   int asynFlags, int autoConnect, int priority, int stackSize)

    : asynNDArrayDriver(portName, maxAddr, numParams+NUM_AD_PARAMS, maxBuffers, maxMemory,
          interfaceMask | asynInt32Mask | asynFloat64Mask | asynOctetMask | asynGenericPointerMask | asynDrvUserMask,
          interruptMask | asynInt32Mask | asynFloat64Mask | asynOctetMask | asynGenericPointerMask,
          asynFlags, autoConnect, priority, stackSize)

{
    //char *functionName = "ADDriver";

    createParam(ADManufacturerString,        asynParamOctet, &ADManufacturer);
    createParam(ADModelString,               asynParamOctet, &ADModel);
    createParam(ADGainString,                asynParamFloat64, &ADGain);
    createParam(ADBinXString,                asynParamInt32, &ADBinX);
    createParam(ADBinYString,                asynParamInt32, &ADBinY);
    createParam(ADMinXString,                asynParamInt32, &ADMinX);
    createParam(ADMinYString,                asynParamInt32, &ADMinY);
    createParam(ADSizeXString,               asynParamInt32, &ADSizeX);
    createParam(ADSizeYString,               asynParamInt32, &ADSizeY);
    createParam(ADMaxSizeXString,            asynParamInt32, &ADMaxSizeX);
    createParam(ADMaxSizeYString,            asynParamInt32, &ADMaxSizeY);
    createParam(ADReverseXString,            asynParamInt32, &ADReverseX);
    createParam(ADReverseYString,            asynParamInt32, &ADReverseY);
    createParam(ADFrameTypeString,           asynParamInt32, &ADFrameType);
    createParam(ADImageModeString,           asynParamInt32, &ADImageMode);
    createParam(ADNumExposuresString,        asynParamInt32, &ADNumExposures);
    createParam(ADNumExposuresCounterString, asynParamInt32, &ADNumExposuresCounter);
    createParam(ADNumImagesString,           asynParamInt32, &ADNumImages);
    createParam(ADNumImagesCounterString,    asynParamInt32, &ADNumImagesCounter);
    createParam(ADAcquireTimeString,         asynParamFloat64, &ADAcquireTime);
    createParam(ADAcquirePeriodString,       asynParamFloat64, &ADAcquirePeriod);
    createParam(ADTimeRemainingString,       asynParamFloat64, &ADTimeRemaining);
    createParam(ADStatusString,              asynParamInt32, &ADStatus);
    createParam(ADTriggerModeString,         asynParamInt32, &ADTriggerMode);
    createParam(ADAcquireString,             asynParamInt32, &ADAcquire);
    createParam(ADShutterControlString,      asynParamInt32, &ADShutterControl);
    createParam(ADShutterControlEPICSString, asynParamInt32, &ADShutterControlEPICS);
    createParam(ADShutterStatusString,       asynParamInt32, &ADShutterStatus);
    createParam(ADShutterModeString,         asynParamInt32, &ADShutterMode);
    createParam(ADShutterOpenDelayString,    asynParamFloat64, &ADShutterOpenDelay);
    createParam(ADShutterCloseDelayString,   asynParamFloat64, &ADShutterCloseDelay);
    createParam(ADTemperatureString,         asynParamFloat64, &ADTemperature);
    createParam(ADReadStatusString,          asynParamInt32, &ADReadStatus);
    createParam(ADStatusMessageString,       asynParamOctet, &ADStatusMessage);
    createParam(ADStringToServerString,      asynParamOctet, &ADStringToServer);
    createParam(ADStringFromServerString,    asynParamOctet, &ADStringFromServer);    

    /* Set some default values for parameters */
    setStringParam(ADManufacturer, "Unknown");
    setStringParam(ADModel,        "Unknown");
    setDoubleParam (ADGain,         1.0);
    setIntegerParam(ADBinX,         1);
    setIntegerParam(ADBinY,         1);
    setIntegerParam(ADMinX,         0);
    setIntegerParam(ADMinY,         0);
    setIntegerParam(ADSizeX,        1);
    setIntegerParam(ADSizeY,        1);
    setIntegerParam(ADMaxSizeX,     1);
    setIntegerParam(ADMaxSizeY,     1);
    setIntegerParam(ADReverseX,     0);
    setIntegerParam(ADReverseY,     0);
    setIntegerParam(ADFrameType,    ADFrameNormal);
    setIntegerParam(ADImageMode,    ADImageContinuous);
    setIntegerParam(ADTriggerMode,  0);
    setIntegerParam(ADNumExposures, 1);
    setIntegerParam(ADNumImages,    1);
    setDoubleParam (ADAcquireTime,  1.0);
    setDoubleParam (ADAcquirePeriod,0.0);
    setIntegerParam(ADStatus,       ADStatusIdle);
    setIntegerParam(ADAcquire,      0);
    setIntegerParam(ADNumImagesCounter, 0);
    setIntegerParam(ADNumExposuresCounter, 0);
    setDoubleParam( ADTimeRemaining, 0.0);
    setIntegerParam(ADShutterControl, 0);
    setIntegerParam(ADShutterStatus, 0);
    setIntegerParam(ADShutterMode,   0);
    setDoubleParam (ADShutterOpenDelay, 0.0);
    setDoubleParam (ADShutterCloseDelay, 0.0);
    setDoubleParam (ADTemperature, 0.0);

    setStringParam (ADStatusMessage,  "");
    setStringParam (ADStringToServer, "");
    setStringParam (ADStringFromServer,  "");
}
