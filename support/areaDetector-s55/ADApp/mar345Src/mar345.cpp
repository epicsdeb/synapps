/* mar345.cpp
 *
 * This is a driver for a MAR 345 detector.
 * It uses a TCP/IP socket to communicate with the mar345dtb program.
 * It reads files written by mar345dtb to obtain the data.
 *
 * Author: Mark Rivers
 *         University of Chicago
 *
 * Created:  March 15, 2009
 *
 */
 
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsEvent.h>
#include <epicsTimer.h>
#include <epicsMutex.h>
#include <epicsString.h>
#include <epicsStdio.h>
#include <epicsMutex.h>
#include <cantProceed.h>
#include <iocsh.h>
#include <epicsExport.h>

#include <asynOctetSyncIO.h>

#include "ADDriver.h"

#include "mar3xx_pck.h"

/** Messages to/from server */
#define MAX_MESSAGE_SIZE 256
#define MAX_FILENAME_LEN 256
#define MAR345_SOCKET_TIMEOUT 1.0
#define MAR345_COMMAND_TIMEOUT 180.0
#define MAR345_POLL_DELAY .01

/** Trigger mode choices */
typedef enum {
    TMInternal,
    TMExternal,
    TMAlignment
} mar345TriggerMode_t;

/** Erase mode choices */
typedef enum {
    mar345EraseNone,
    mar345EraseBefore,
    mar345EraseAfter
} mar345EraseMode_t;

/** Readout size choices */
typedef enum {
    mar345Size180,
    mar345Size240,
    mar345Size300,
    mar345Size345
} mar345Size_t;

/** Resolution choices */
typedef enum {
    mar345Res100,
    mar345Res150
} mar345Res_t;

/** Mode choices */
typedef enum {
    mar345ModeIdle,
    mar345ModeErase,
    mar345ModeAcquire,
    mar345ModeChange
} mar345Mode_t;

/** Status choices */
typedef enum {
    mar345StatusIdle,
    mar345StatusExpose,
    mar345StatusScan,
    mar345StatusErase,
    mar345StatusChangeMode,
    mar345StatusAborting,
    mar345StatusError,
    mar345StatusWaiting
} mar345Status_t;


static int imageSizes[2][4] = {{1800, 2400, 3000, 3450},{1200, 1600, 2000, 2300}}; 

/** Driver-specific parameter strings for the mar345 driver */
#define mar345EraseString         "MAR_ERASE"
#define mar345EraseModeString     "MAR_ERASE_MODE"
#define mar345NumEraseString      "MAR_NUM_ERASE"
#define mar345NumErasedString     "MAR_NUM_ERASED"
#define mar345ChangeModeString    "MAR_CHANGE_MODE"
#define mar345SizeString          "MAR_SIZE"
#define mar345ResString           "MAR_RESOLUTION"
#define mar345AbortString         "MAR_ABORT"
    
static const char *driverName = "mar345";

/** Driver for mar345 online image plate detector; communicates with the mar345dtb program 
  * over a TCP/IP socket.
  * The mar345dtb program must be running and must be configured to listen for commands on a
  * socket.  This is done by adding a line like the following to 
  * the file /home/mar345/tables/config.xxx (where xxx is the detector serial number)
  *  COMMAND PORT 5001
  * In this example 5001 is the TCP/IP port number that the mar345dtb and this driver will use to
  * communicate.
  */
class mar345 : public ADDriver {
public:
    mar345(const char *portName, const char *mar345Port,
           int maxBuffers, size_t maxMemory,
           int priority, int stackSize);
                 
    /* These are the methods that we override from ADDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    void report(FILE *fp, int details);

    epicsEventId startEventId; /**< Should be private but accessed from C, must be public */
    epicsEventId stopEventId;  /**< Should be private but accessed from C, must be public */
    epicsEventId abortEventId; /**< Should be private but accessed from C, must be public */
    void mar345Task();         /**< Should be private but accessed from C, must be public */

protected:
    int mar345Erase;
    #define FIRST_MAR345_PARAM mar345Erase
    int mar345EraseMode;
    int mar345NumErase;
    int mar345NumErased;
    int mar345ChangeMode;
    int mar345Size;
    int mar345Res;
    int mar345Abort;
    #define LAST_MAR345_PARAM mar345Abort

private:                                        
    /* These are the methods that are new to this class */
    asynStatus readFile(const char *fileName, NDArray *pImage);
    asynStatus writeServer(const char *output);
    asynStatus readServer(char *input, size_t maxChars, double timeout);
    asynStatus erase();
    asynStatus changeMode();
    asynStatus acquireFrame();
    void readoutFrame(int bufferNumber, const char* fileName, int wait);
    void setShutter(int open);
    void getImageDataTask();
    void getImageData();
    asynStatus waitForCompletion(const char *doneString, double timeout);
  
    /* Our data */
    epicsTimeStamp acqStartTime;
    epicsTimeStamp acqEndTime;
    epicsTimerId timerId;
    mar345Mode_t mode;
    char toServer[MAX_MESSAGE_SIZE];
    char fromServer[MAX_MESSAGE_SIZE];
    NDArray *pData;
    asynUser *pasynUserServer;
};

#define NUM_MAR345_PARAMS (&LAST_MAR345_PARAM - &FIRST_MAR345_PARAM + 1)

void mar345::getImageData()
{
    char fullFileName[MAX_FILENAME_LEN];
    int dims[2];
    int imageCounter;
    NDArray *pImage;
    char statusMessage[MAX_MESSAGE_SIZE];
    char errorBuffer[MAX_MESSAGE_SIZE];
    FILE *input;
    const char *functionName = "getImageData";

    /* Inquire about the image dimensions */
    getStringParam(NDFullFileName, MAX_FILENAME_LEN, fullFileName);
    getIntegerParam(NDArraySizeX, &dims[0]);
    getIntegerParam(NDArraySizeY, &dims[1]);
    getIntegerParam(NDArrayCounter, &imageCounter);
    pImage = this->pNDArrayPool->alloc(2, dims, NDUInt16, 0, NULL);

    epicsSnprintf(statusMessage, sizeof(statusMessage), "Reading mar345 file %s", fullFileName);
    setStringParam(ADStatusMessage, statusMessage);
    callParamCallbacks();
    input = fopen(fullFileName, "rb");
    if (input == NULL) {
        (void) strerror_r(errno, errorBuffer, sizeof(errorBuffer));
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s%s: unable to open input file %s, error=%s\n",
            driverName, functionName, fullFileName, errorBuffer);
        return;
    }
	get_pck(input, (epicsInt16 *)pImage->pData);
    fclose(input);

    /* Put the frame number and time stamp into the buffer */
    pImage->uniqueId = imageCounter;
    pImage->timeStamp = this->acqStartTime.secPastEpoch + this->acqStartTime.nsec / 1.e9;

    /* Get any attributes that have been defined for this driver */        
    this->getAttributes(pImage->pAttributeList);

    /* Call the NDArray callback */
    /* Must release the lock here, or we can get into a deadlock, because we can
     * block on the plugin lock, and the plugin can be calling us */
    this->unlock();
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
         "%s:%s: calling NDArray callback\n", driverName, functionName);
    doCallbacksGenericPointer(pImage, NDArrayData, 0);
    this->lock();

    /* Free the image buffer */
    pImage->release();
}

asynStatus mar345::writeServer(const char *output)
{
    size_t nwrite;
    asynStatus status;
    asynUser *pasynUser = this->pasynUserServer;
    const char *functionName="writeServer";

    /* Flush any stale input, since the next operation is likely to be a read */
    status = pasynOctetSyncIO->flush(pasynUser);
    status = pasynOctetSyncIO->write(pasynUser, output,
                                     strlen(output), MAR345_SOCKET_TIMEOUT,
                                     &nwrite);
                                        
    if (status) asynPrint(pasynUser, ASYN_TRACE_ERROR,
                    "%s:%s, status=%d, sent\n%s\n",
                    driverName, functionName, status, output);

    /* Set output string so it can get back to EPICS */
    setStringParam(ADStringToServer, output);
    callParamCallbacks();
    
    return(status);
}


asynStatus mar345::readServer(char *input, size_t maxChars, double timeout)
{
    size_t nread;
    asynStatus status=asynSuccess;
    asynUser *pasynUser = this->pasynUserServer;
    int eomReason;
    const char *functionName="readServer";

    status = pasynOctetSyncIO->read(pasynUser, input, maxChars, timeout,
                                    &nread, &eomReason);
    if (nread == 0) return(status);
    if (status) asynPrint(pasynUser, ASYN_TRACE_ERROR,
                    "%s:%s, timeout=%f, status=%d received %d bytes\n%s\n",
                    driverName, functionName, timeout, status, nread, input);
    /* Set output string so it can get back to EPICS */
    setStringParam(ADStringFromServer, input);
    callParamCallbacks();
    return(status);
}

/* This function is called when the exposure time timer expires */
extern "C" {static void timerCallbackC(void *drvPvt)
{
    mar345 *pPvt = (mar345 *)drvPvt;
    
   epicsEventSignal(pPvt->stopEventId);
}}

asynStatus mar345::waitForCompletion(const char *doneString, double timeout)
{
    char response[MAX_MESSAGE_SIZE];
    asynStatus status;
    double elapsedTime;
    epicsTimeStamp start, now;
    const char *functionName = "waitForCompletion";
 
    epicsTimeGetCurrent(&start);
    while (1) {
        this->unlock();
        status = readServer(response, sizeof(response), MAR345_POLL_DELAY);
        this->lock();
        if (status == asynSuccess) {
            if (strstr(response, doneString)) return(asynSuccess);
        }
        epicsTimeGetCurrent(&now);
        elapsedTime = epicsTimeDiffInSeconds(&now, &start);
        if (elapsedTime > timeout) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "%s:%s: error waiting for response from marServer\n",
                driverName, functionName);
            return(asynError);
        }
    }
}

asynStatus mar345::changeMode()
{
    asynStatus status=asynSuccess;
    //const char *functionName = "changeMode";
    int size, res;
    int sizeX;

    setIntegerParam(ADStatus, mar345StatusChangeMode);
    callParamCallbacks();
    getIntegerParam(mar345Size, &size);
    getIntegerParam(mar345Res, &res);
    sizeX = imageSizes[res][size];
    setIntegerParam(NDArraySizeX, sizeX);
    setIntegerParam(NDArraySizeY, sizeX);
    setIntegerParam(NDArraySize, sizeX*sizeX*sizeof(epicsInt16));
    epicsSnprintf(this->toServer, sizeof(this->toServer), "COMMAND CHANGE %d", imageSizes[res][size]);
    writeServer(this->toServer);
    status = waitForCompletion("MODE_CHANGE  Ended o.k.", MAR345_COMMAND_TIMEOUT);
    setIntegerParam(ADStatus, mar345StatusIdle);
    setIntegerParam(mar345ChangeMode, 0);
    callParamCallbacks();
    return(status);
}

asynStatus mar345::erase()
{
    int numErase;
    int i;
    asynStatus status=asynSuccess;
    //const char *functionName = "erase";

    getIntegerParam(mar345NumErase, &numErase);
    if (numErase < 1) numErase=1;
    setIntegerParam(ADStatus, mar345StatusErase);
    setIntegerParam(mar345NumErased, 0);
    callParamCallbacks();
    for (i=0; i<numErase; i++) {
        if (epicsEventTryWait(this->abortEventId) == epicsEventWaitOK) {
            status = asynError;
            break;
        }
        epicsSnprintf(this->toServer, sizeof(this->toServer), "COMMAND ERASE");
        writeServer(this->toServer);
        status = waitForCompletion("SCAN_DATA    Ended o.k.", MAR345_COMMAND_TIMEOUT);
        if (status) break;
        setIntegerParam(mar345NumErased, i+1);
        callParamCallbacks();
    }
    setIntegerParam(ADStatus, mar345StatusIdle);
    setIntegerParam(mar345Erase, 0);
    callParamCallbacks();
    return(status);
}

void mar345::setShutter(int open)
{
    ADShutterMode_t shutterMode;
    double delay;
    double shutterOpenDelay, shutterCloseDelay;
    
    getIntegerParam(ADShutterMode, (int *)&shutterMode);
    getDoubleParam(ADShutterOpenDelay, &shutterOpenDelay);
    getDoubleParam(ADShutterCloseDelay, &shutterCloseDelay);
    
    switch (shutterMode) {
        case ADShutterModeDetector:
            if (open) {
                /* Open the shutter */
                writeServer("COMMAND SHUTTER OPEN");
                /* This delay is to get the exposure time correct.  
                * It is equal to the opening time of the shutter minus the
                * closing time.  If they are equal then no delay is needed, 
                * except use 1msec so delay is not negative and commands are 
                * not back-to-back */
                delay = shutterOpenDelay - shutterCloseDelay;
                if (delay < .001) delay=.001;
                epicsThreadSleep(delay);
            } else {
                /* Close shutter */
                writeServer("COMMAND SHUTTER CLOSE");
                epicsThreadSleep(shutterCloseDelay);
            }
            /* The mar345 does not provide a way to read the actual shutter status, so
             * set it to agree with the control value */
            setIntegerParam(ADShutterStatus, open);
            callParamCallbacks();
            break;
        default:
            ADDriver::setShutter(open);
            break;
    }
}


asynStatus mar345::acquireFrame()
{
    asynStatus status=asynSuccess;
    epicsTimeStamp startTime, currentTime;
    int eraseMode;
    epicsEventWaitStatus waitStatus;
    int imageCounter;
    int arrayCallbacks;
    double acquireTime;
    double timeRemaining;
    int size, res;
    int shutterMode, useShutter;
    char tempFileName[MAX_FILENAME_LEN];
    char fullFileName[MAX_FILENAME_LEN];
    //const char *functionName = "acquireframe";

    /* Get current values of some parameters */
    getDoubleParam(ADAcquireTime, &acquireTime);
    getIntegerParam(ADShutterMode, &shutterMode);
    getIntegerParam(mar345Size, &size);
    getIntegerParam(mar345Res, &res);
    getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
    getIntegerParam(mar345EraseMode, &eraseMode);
    if (shutterMode == ADShutterModeNone) useShutter=0; else useShutter=1;

    epicsTimeGetCurrent(&this->acqStartTime);

    createFileName(MAX_FILENAME_LEN, tempFileName);
    /* We need to append the extension */
    epicsSnprintf(fullFileName, sizeof(fullFileName), "%s.mar%d", tempFileName, imageSizes[res][size]);

    /* Erase before exposure if set */
    if (eraseMode == mar345EraseBefore) {
        status = this->erase();
        if (status) return(status);
    }
    
    /* Set the the start time for the TimeRemaining counter */
    epicsTimeGetCurrent(&startTime);
    timeRemaining = acquireTime;
    if (useShutter) setShutter(1);

    /* Wait for the exposure time using epicsEventWaitWithTimeout, 
     * so we can abort */
    epicsTimerStartDelay(this->timerId, acquireTime);
    setIntegerParam(ADStatus, mar345StatusExpose);
    callParamCallbacks();
    while(1) {
        if (epicsEventTryWait(this->abortEventId) == epicsEventWaitOK) {
            status = asynError;
            break;
        }
        this->unlock();
        waitStatus = epicsEventWaitWithTimeout(this->stopEventId, MAR345_POLL_DELAY);
        this->lock();
        if (waitStatus == epicsEventWaitOK) {
            /* The acquisition was stopped before the time was complete */
            epicsTimerCancel(this->timerId);
            break;
        }
        epicsTimeGetCurrent(&currentTime);
        timeRemaining = acquireTime - 
            epicsTimeDiffInSeconds(&currentTime, &startTime);
        if (timeRemaining < 0.) timeRemaining = 0.;
        setDoubleParam(ADTimeRemaining, timeRemaining);
        callParamCallbacks();
    }
    setDoubleParam(ADTimeRemaining, 0.0);
    if (useShutter) setShutter(0);
    setIntegerParam(ADStatus, mar345StatusIdle);
    callParamCallbacks();
    // If the exposure was aborted return error
    if (status) return asynError;
    setIntegerParam(ADStatus, mar345StatusScan);
    callParamCallbacks();
    epicsSnprintf(this->toServer, sizeof(this->toServer), "COMMAND SCAN %s", fullFileName);
    setStringParam(NDFullFileName, fullFileName);
    callParamCallbacks();
    writeServer(this->toServer);
    status = waitForCompletion("SCAN_DATA    Ended o.k.", MAR345_COMMAND_TIMEOUT);
    if (status) {
        return asynError;
    }
    getIntegerParam(NDArrayCounter, &imageCounter);
    imageCounter++;
    setIntegerParam(NDArrayCounter, imageCounter);
    /* Call the callbacks to update any changes */
    callParamCallbacks();

    /* If arrayCallbacks is set then read the file back in */
    if (arrayCallbacks) {
        getImageData();
    }

    /* Erase after scanning if set */
    if (eraseMode == mar345EraseAfter) status = this->erase();

    return status;
}

static void mar345TaskC(void *drvPvt)
{
    mar345 *pPvt = (mar345 *)drvPvt;
    
    pPvt->mar345Task();
}

/** This thread controls handling of slow events - erase, acquire, change mode */
void mar345::mar345Task()
{
    int status = asynSuccess;
    int numImages, numImagesCounter;
    int imageMode;
    int acquire;
    double acquirePeriod;
    double elapsedTime, delayTime;
    const char *functionName = "mar345Task";

    this->lock();

    /* Loop forever */
    while (1) {
        setStringParam(ADStatusMessage, "Waiting for event");
        callParamCallbacks();
        /* Release the lock while we wait for an event that says acquire has started, then lock again */
        this->unlock();
        asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: waiting for start event\n", driverName, functionName);
        status = epicsEventWait(this->startEventId);
        this->lock();

        switch(this->mode) {
            case mar345ModeErase:
                this->erase();
                this->mode = mar345ModeIdle;
                break;

            case mar345ModeAcquire:
                getIntegerParam(ADImageMode, &imageMode);
                getIntegerParam(ADNumImages, &numImages);
                if (numImages < 1) numImages = 1;
                if (imageMode == ADImageSingle) numImages=1;
                for (numImagesCounter=0;
                        numImagesCounter<numImages || (imageMode == ADImageContinuous); 
                        numImagesCounter++) {
                    if (epicsEventTryWait(this->abortEventId) == epicsEventWaitOK) break;
                    setIntegerParam(ADNumImagesCounter, numImagesCounter);
                    callParamCallbacks();
                    status = acquireFrame();
                    if (status) break;
                    /* We get out of the loop in single shot mode or if acquire was set to 0 by client */
                    if (imageMode == ADImageSingle) setIntegerParam(ADAcquire, 0);
                    getIntegerParam(ADAcquire, &acquire);
                    if (!acquire) break;
                    /* We are in continuous or multiple mode.
                     * Sleep until the acquire period expires or acquire is set to stop */
                    epicsTimeGetCurrent(&this->acqEndTime);
                    elapsedTime = epicsTimeDiffInSeconds(&this->acqEndTime, &this->acqStartTime);
                    getDoubleParam(ADAcquirePeriod, &acquirePeriod);
                    delayTime = acquirePeriod - elapsedTime;
                    if (delayTime > 0.) {
                        setIntegerParam(ADStatus, mar345StatusWaiting);
                        callParamCallbacks();
                        this->unlock();
                        status = epicsEventWaitWithTimeout(this->abortEventId, delayTime);
                        this->lock();
                        if (status == epicsEventWaitOK) break;
                    }
                }
                this->mode = mar345ModeIdle;
                setIntegerParam(ADAcquire, 0);
                setIntegerParam(ADStatus, mar345StatusIdle);
                break;

            case mar345ModeChange:
                this->changeMode();
                this->mode = mar345ModeIdle;
                break;
                
            default:
                break;
        }

        /* Call the callbacks to update any changes */
        callParamCallbacks();
    }
}


/** Called when asyn clients call pasynInt32->write().
  * This function performs actions for some parameters, including ADAcquire, mar345Erase, etc.
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Value to write. */
asynStatus mar345::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char *functionName = "writeInt32";

    status = setIntegerParam(function, value);

    if (function == ADAcquire) {
        if (value && (this->mode == mar345ModeIdle)) {
            /* Send an event to wake up the mar345 task.  */
            this->mode = mar345ModeAcquire;
            epicsEventSignal(this->startEventId);
        } 
        if (!value && (this->mode != mar345ModeIdle)) {
            /* Stop acquiring (ends exposure, does not abort) */
            epicsEventSignal(this->stopEventId);
        }
    } else if (function == mar345Erase) {
        if (value && (this->mode == mar345ModeIdle)) {
            this->mode = mar345ModeErase;
            /* Send an event to wake up the mar345 task.  */
            epicsEventSignal(this->startEventId);
        } 
    } else if (function == mar345ChangeMode) {
        if (value && (this->mode == mar345ModeIdle)) {
           this->mode = mar345ModeChange;
            /* Send an event to wake up the mar345 task.  */
            epicsEventSignal(this->startEventId);
        } 
    } else if (function == mar345Abort) {
        if (value && (this->mode != mar345ModeIdle)) {
            /* Abort operation */
            setIntegerParam(ADStatus, mar345StatusAborting);
            epicsEventSignal(this->abortEventId);
        }
    } else if (function == ADShutterControl) {
        setShutter(value);
    } else {
        /* If this is not a parameter we have handled call the base class */
        if (function < FIRST_MAR345_PARAM) status = ADDriver::writeInt32(pasynUser, value);
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



/** Report status of the driver.
  * Prints details about the driver if details>0.
  * It then calls the ADDriver::report() method.
  * \param[in] fp File pointed passed by caller where the output is written to.
  * \param[in] details If >0 then driver details are printed.
  */
void mar345::report(FILE *fp, int details)
{
    fprintf(fp, "MAR-345 detector %s\n", this->portName);
    /* Invoke the base class method */
    ADDriver::report(fp, details);
}

extern "C" int mar345Config(const char *portName, const char *serverPort, 
                            int maxBuffers, size_t maxMemory,
                            int priority, int stackSize)
{
    new mar345(portName, serverPort, maxBuffers, maxMemory, priority, stackSize);
    return(asynSuccess);
}

/** Constructor for mar345 driver; most parameters are simply passed to ADDriver::ADDriver.
  * After calling the base class constructor this method creates a thread to collect the detector data, 
  * and sets reasonable default values for the parameters defined in this class and ADDriver.
  * \param[in] portName The name of the asyn port driver to be created.
  * \param[in] serverPort The name of the asyn port driver previously created with drvAsynIPPortConfigure
  *            connected to the mar345dtb program.
  * \param[in] maxBuffers The maximum number of NDArray buffers that the NDArrayPool for this driver is 
  *            allowed to allocate. Set this to -1 to allow an unlimited number of buffers.
  * \param[in] maxMemory The maximum amount of memory that the NDArrayPool for this driver is 
  *            allowed to allocate. Set this to -1 to allow an unlimited amount of memory.
  * \param[in] priority The thread priority for the asyn port driver thread.
  * \param[in] stackSize The stack size for the asyn port driver thread.
  */
mar345::mar345(const char *portName, const char *serverPort,
                                int maxBuffers, size_t maxMemory,
                                int priority, int stackSize)

    : ADDriver(portName, 1, NUM_MAR345_PARAMS, maxBuffers, maxMemory,
               0, 0,             /* No interfaces beyond those set in ADDriver.cpp */
               ASYN_CANBLOCK, 1, /* ASYN_CANBLOCK=1, ASYN_MULTIDEVICE=0, autoConnect=1 */
               priority, stackSize),
      pData(NULL)

{
    int status = asynSuccess;
    epicsTimerQueueId timerQ;
    const char *functionName = "mar345";
    int dims[2];

    createParam(mar345EraseString,     asynParamInt32, &mar345Erase);
    createParam(mar345EraseModeString, asynParamInt32, &mar345EraseMode);
    createParam(mar345NumEraseString,  asynParamInt32, &mar345NumErase);
    createParam(mar345NumErasedString, asynParamInt32, &mar345NumErased);
    createParam(mar345ChangeModeString,asynParamInt32, &mar345ChangeMode);
    createParam(mar345SizeString,      asynParamInt32, &mar345Size);
    createParam(mar345ResString,       asynParamInt32, &mar345Res);
    createParam(mar345AbortString,     asynParamInt32, &mar345Abort);

    this->mode = mar345ModeIdle;
    
    /* Create the epicsEvents for signaling to the mar345 task when acquisition starts and stops */
    this->startEventId = epicsEventCreate(epicsEventEmpty);
    if (!this->startEventId) {
        printf("%s:%s epicsEventCreate failure for start event\n", 
            driverName, functionName);
        return;
    }
    this->stopEventId = epicsEventCreate(epicsEventEmpty);
    if (!this->stopEventId) {
        printf("%s:%s epicsEventCreate failure for stop event\n", 
            driverName, functionName);
        return;
    }
    this->abortEventId = epicsEventCreate(epicsEventEmpty);
    if (!this->abortEventId) {
        printf("%s:%s epicsEventCreate failure for abort event\n", 
            driverName, functionName);
        return;
    }

    /* Create the epicsTimerQueue for exposure time handling */
    timerQ = epicsTimerQueueAllocate(1, epicsThreadPriorityScanHigh);
    this->timerId = epicsTimerQueueCreateTimer(timerQ, timerCallbackC, this);
    
    
    /* Connect to server */
    status = pasynOctetSyncIO->connect(serverPort, 0, &this->pasynUserServer, NULL);
    
    dims[0] = 3450;
    dims[1] = 3450;
    /* Allocate the raw buffer we use to files.  Only do this once */
    setIntegerParam(ADMaxSizeX, dims[0]);
    setIntegerParam(ADMaxSizeY, dims[1]);
    this->pData = this->pNDArrayPool->alloc(2, dims, NDInt16, 0, NULL);

    /* Set some default values for parameters */
    status =  setStringParam (ADManufacturer, "MAR");
    status |= setStringParam (ADModel, "345");
    status |= setIntegerParam(NDDataType,  NDInt16);
    status |= setIntegerParam(ADImageMode, ADImageSingle);
    status |= setIntegerParam(ADTriggerMode, TMInternal);
    status |= setDoubleParam (ADAcquireTime, 1.);
    status |= setDoubleParam (ADAcquirePeriod, 0.);
    status |= setIntegerParam(ADNumImages, 1);

    status |= setIntegerParam(mar345EraseMode, mar345EraseAfter);
    status |= setIntegerParam(mar345Size, mar345Size345);
    status |= setIntegerParam(mar345Res, mar345Res100);
    status |= setIntegerParam(mar345NumErase, 1);
    status |= setIntegerParam(mar345NumErased  , 0);
    status |= setIntegerParam(mar345Erase, 0);
    status |= setIntegerParam(mar345Res, mar345Res100);

    if (status) {
        printf("%s: unable to set camera parameters\n", functionName);
        return;
    }
    
    /* Create the thread that collects the data */
    status = (epicsThreadCreate("mar345Task",
                                epicsThreadPriorityMedium,
                                epicsThreadGetStackSize(epicsThreadStackMedium),
                                (EPICSTHREADFUNC)mar345TaskC,
                                this) == NULL);
    if (status) {
        printf("%s:%s epicsThreadCreate failure for data collection task\n", 
            driverName, functionName);
        return;
    }
}

/* Code for iocsh registration */
static const iocshArg mar345ConfigArg0 = {"Port name", iocshArgString};
static const iocshArg mar345ConfigArg1 = {"server port name", iocshArgString};
static const iocshArg mar345ConfigArg2 = {"maxBuffers", iocshArgInt};
static const iocshArg mar345ConfigArg3 = {"maxMemory", iocshArgInt};
static const iocshArg mar345ConfigArg4 = {"priority", iocshArgInt};
static const iocshArg mar345ConfigArg5 = {"stackSize", iocshArgInt};
static const iocshArg * const mar345ConfigArgs[] =  {&mar345ConfigArg0,
                                                     &mar345ConfigArg1,
                                                     &mar345ConfigArg2,
                                                     &mar345ConfigArg3,
                                                     &mar345ConfigArg4,
                                                     &mar345ConfigArg5};
static const iocshFuncDef configMAR345 = {"mar345Config", 6, mar345ConfigArgs};
static void configMAR345CallFunc(const iocshArgBuf *args)
{
    mar345Config(args[0].sval, args[1].sval, args[2].ival,
                 args[3].ival, args[4].ival, args[5].ival);
}


static void mar345Register(void)
{

    iocshRegister(&configMAR345, configMAR345CallFunc);
}

extern "C" {
epicsExportRegistrar(mar345Register);
}
