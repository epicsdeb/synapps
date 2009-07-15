/* marCCD.cpp
 *
 * This is a driver for a MAR CCD detector.
 *
 * Author: Mark Rivers
 *         University of Chicago
 *
 * Created:  Nov. 1, 2008
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
#include <tiffio.h>

#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsEvent.h>
#include <epicsTimer.h>
#include <epicsMutex.h>
#include <epicsString.h>
#include <epicsStdio.h>
#include <epicsMutex.h>
#include <cantProceed.h>

#include <asynOctetSyncIO.h>

#include "ADStdDriverParams.h"
#include "NDArray.h"
#include "ADDriver.h"

#include "drvMARCCD.h"

#define MAX_MESSAGE_SIZE 256 /* Messages to/from server */
#define MAX_FILENAME_LEN 256
#define MARCCD_SERVER_TIMEOUT 1.0 
/* Time between checking to see if TIFF file is complete */
#define FILE_READ_DELAY .01
#define MARCCD_POLL_DELAY .01

/* Task numbers */
#define TASK_ACQUIRE		0
#define TASK_READ		1
#define TASK_CORRECT		2
#define TASK_WRITE		3
#define TASK_DEZINGER		4

/* The status bits for each task are: */
/* Task Status bits */
#define TASK_STATUS_QUEUED	0x1
#define TASK_STATUS_EXECUTING	0x2
#define TASK_STATUS_ERROR	0x4
#define TASK_STATUS_RESERVED	0x8

/* This are the "old" states from version 0, but BUSY is also used in version 1 */
#define TASK_STATE_IDLE 0
#define TASK_STATE_ACQUIRE 1
#define TASK_STATE_READOUT 2
#define TASK_STATE_CORRECT 3
#define TASK_STATE_WRITING 4
#define TASK_STATE_ABORTING 5
#define TASK_STATE_UNAVAILABLE 6
#define TASK_STATE_ERROR 7
#define TASK_STATE_BUSY 8

/* These are the definitions of masks for looking at task state bits */
#define STATE_MASK		0xf
#define STATUS_MASK		0xf
#define TASK_STATUS_MASK(task)	(STATUS_MASK << (4*((task)+1)))

/* These are some convenient macros for checking and setting the state of each task */
/* They are used in the marccd code and can be used in the client code */
#define TASK_STATE(current_status) ((current_status) & STATE_MASK)
#define TASK_STATUS(current_status, task) (((current_status) & TASK_STATUS_MASK(task)) >> (4*((task) + 1)))
#define TEST_TASK_STATUS(current_status, task, status) (TASK_STATUS(current_status, task) & (status))

typedef enum {
    TMInternal,
    TMExternal,
    TMAlignment
} marCCDTriggerMode_t;

typedef enum {
    marCCDFrameNormal,
    marCCDFrameBackground,
    marCCDFrameRaw,
    marCCDFrameDoubleCorrelation
} marCCDFrameType_t;


static const char *driverName = "marCCD";

class marCCD : public ADDriver {
public:
    marCCD(const char *portName, const char *marCCDPort,
           int maxBuffers, size_t maxMemory);
                 
    /* These are the methods that we override from ADDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus drvUserCreate(asynUser *pasynUser, const char *drvInfo, 
                                     const char **pptypeName, size_t *psize);
    void report(FILE *fp, int details);
                                        
    /* These are the methods that are new to this class */
    void marCCDTask();
    asynStatus readTiff(const char *fileName, NDArray *pImage);
    asynStatus writeServer(const char *output);
    asynStatus readServer(char *input, size_t maxChars, double timeout);
    asynStatus writeReadServer(const char *output, char *input, size_t maxChars, double timeout);
    asynStatus writeHeader();
    int getState();
    asynStatus getConfig();
    void acquireFrame(double exposureTime, int useShutter);
    void readoutFrame(int bufferNumber, const char* fileName, int wait);
    void setShutter(int open);
    void saveFile(int correctedFlag, int wait);
    void getImageDataTask();
    void getImageData();
   
    /* Our data */
    epicsEventId startEventId;
    epicsEventId stopEventId;
    epicsEventId imageEventId;
    epicsTimeStamp acqStartTime;
    epicsTimeStamp acqEndTime;
    epicsTimerId timerId;
    char toServer[MAX_MESSAGE_SIZE];
    char fromServer[MAX_MESSAGE_SIZE];
    NDArray *pData;
    asynUser *pasynUserServer;
};

/* If we have any private driver parameters they begin with ADFirstDriverParam and should end
   with ADLastDriverParam, which is used for setting the size of the parameter library table */
typedef enum {
    marCCDTiffTimeout
        = ADFirstDriverParam,
    marCCDOverlap,
    marCCDState,
    marCCDStatus,
    marCCDTaskAcquireStatus,
    marCCDTaskReadoutStatus,
    marCCDTaskCorrectStatus,
    marCCDTaskWritingStatus,
    marCCDTaskDezingerStatus,
    marCCDFrameShift,
    marCCDDetectorDistance,
    marCCDBeamX,
    marCCDBeamY,
    marCCDStartPhi,
    marCCDRotationAxis,
    marCCDRotationRange,
    marCCDWavelength,
    marCCDFileComments,
    marCCDDatasetComments,
    ADLastDriverParam
} marCCDParam_t;

static asynParamString_t marCCDParamString[] = {
    {marCCDTiffTimeout,        "MAR_TIFF_TIMEOUT"},
    {marCCDOverlap,            "MAR_OVERLAP"},
    {marCCDState,              "MAR_STATE"},
    {marCCDStatus,             "MAR_STATUS"},
    {marCCDTaskAcquireStatus,  "MAR_ACQUIRE_STATUS"},
    {marCCDTaskReadoutStatus,  "MAR_READOUT_STATUS"},
    {marCCDTaskCorrectStatus,  "MAR_CORRECT_STATUS"},
    {marCCDTaskWritingStatus,  "MAR_WRITING_STATUS"},
    {marCCDTaskDezingerStatus, "MAR_DEZINGER_STATUS"},
    {marCCDFrameShift,         "MAR_FRAME_SHIFT"},
    {marCCDDetectorDistance,   "MAR_DETECTOR_DISTANCE"},
    {marCCDBeamX,              "MAR_BEAM_X"},
    {marCCDBeamY,              "MAR_BEAM_Y"},
    {marCCDStartPhi,           "MAR_START_PHI"},
    {marCCDRotationAxis,       "MAR_ROTATION_AXIS"},
    {marCCDRotationRange,      "MAR_ROTATION_RANGE"},
    {marCCDWavelength,         "MAR_WAVELENGTH"},
    {marCCDFileComments,       "MAR_FILE_COMMENTS"},
    {marCCDDatasetComments,    "MAR_DATASET_COMMENTS"},
};

#define NUM_MARCCD_PARAMS (sizeof(marCCDParamString)/sizeof(marCCDParamString[0]))

void getImageDataTaskC(marCCD *pmarCCD)
{
    pmarCCD->getImageDataTask();
}

void marCCD::getImageDataTask()
{
    int status;
  
    /* This task does the correction and file saving in the background, so that acquisition
     * can be overlapped with these operations */  
    while (1) {
        status = epicsEventWait(this->imageEventId);
        /* Wait for the correction to complete */
        status = getState();
        while (TEST_TASK_STATUS(status, TASK_CORRECT, TASK_STATUS_EXECUTING | TASK_STATUS_QUEUED)) {
            epicsThreadSleep(MARCCD_POLL_DELAY);
            status = getState();
        }

        /* Wait for the write to complete */
        status = getState();
        while (TEST_TASK_STATUS(status, TASK_WRITE, TASK_STATUS_EXECUTING | TASK_STATUS_QUEUED) || 
               TASK_STATE(status) >= 8) {
            epicsThreadSleep(MARCCD_POLL_DELAY);
            status = getState();
        }
        epicsMutexLock(this->mutexId);
        getImageData();
        epicsMutexUnlock(this->mutexId);
    }
}

void marCCD::getImageData()
{
    char fullFileName[MAX_FILENAME_LEN];
    int dims[2];
    int imageCounter;
    NDArray *pImage;
    asynStatus status;
    char statusMessage[MAX_MESSAGE_SIZE];
    const char *functionName = "getImageData";

    /* Inquire about the image dimensions */
    getConfig();
    getStringParam(ADFullFileName, MAX_FILENAME_LEN, fullFileName);
    getIntegerParam(ADImageSizeX, &dims[0]);
    getIntegerParam(ADImageSizeY, &dims[1]);
    getIntegerParam(ADImageCounter, &imageCounter);
    pImage = this->pNDArrayPool->alloc(2, dims, NDUInt16, 0, NULL);

    epicsSnprintf(statusMessage, sizeof(statusMessage), "Reading TIFF file %s", fullFileName);
    setStringParam(ADStatusMessage, statusMessage);
    callParamCallbacks();
    status = readTiff(fullFileName, pImage); 

    /* Put the frame number and time stamp into the buffer */
    pImage->uniqueId = imageCounter;
    pImage->timeStamp = this->acqStartTime.secPastEpoch + this->acqStartTime.nsec / 1.e9;

    /* Call the NDArray callback */
    /* Must release the lock here, or we can get into a deadlock, because we can
     * block on the plugin lock, and the plugin can be calling us */
    epicsMutexUnlock(this->mutexId);
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
         "%s:%s: calling NDArray callback\n", driverName, functionName);
    doCallbacksGenericPointer(pImage, NDArrayData, 0);
    epicsMutexLock(this->mutexId);

    /* Free the image buffer */
    pImage->release();
}

/* This function reads TIFF files using libTiff.  It is not intended to be general,
 * it is intended to read the TIFF files that marCCDServer creates.  It checks to make sure
 * that the creation time of the file is after a start time passed to it, to force it to
 * wait for a new file to be created.
 */
asynStatus marCCD::readTiff(const char *fileName, NDArray *pImage)
{
    int fd=-1;
    int fileExists=0;
    struct stat statBuff;
    epicsTimeStamp tStart, tCheck;
    time_t startTime;
    double deltaTime;
    int status=-1;
    const char *functionName = "readTiff";
    int size, totalSize;
    int numStrips, strip;
    char *buffer;
    TIFF *tiff=NULL;
    epicsUInt32 uval;
    double timeout;

    getDoubleParam(marCCDTiffTimeout, &timeout);
    deltaTime = 0.;
    epicsTimeGetCurrent(&tStart);
    epicsTimeToTime_t(&startTime, &tStart);
    
    /* Suppress error messages from the TIFF library */
    TIFFSetErrorHandler(NULL);
    TIFFSetWarningHandler(NULL);
    
    while (deltaTime <= timeout) {
        fd = open(fileName, O_RDONLY, 0);
        if ((fd >= 0) && (timeout != 0.)) {
            fileExists = 1;
            /* The file exists.  Make sure it is a new file, not an old one.
             * We don't do this check if timeout==0, which is used for reading flat field files */
            status = fstat(fd, &statBuff);
            if (status){
                asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling fstat, errno=%d %s\n",
                    driverName, functionName, errno, fileName);
                close(fd);
                return(asynError);
            }
            /* We allow up to 10 second clock skew between time on machine running this IOC
             * and the machine with the file system returning modification time */
            if (difftime(statBuff.st_mtime, startTime) > -10) break;
            close(fd);
            fd = -1;
        }
        /* Sleep, but check for stop event, which can be used to abort a long acquisition */
        status = epicsEventWaitWithTimeout(this->stopEventId, FILE_READ_DELAY);
        if (status == epicsEventWaitOK) {
            return(asynError);
        }
        epicsTimeGetCurrent(&tCheck);
        deltaTime = epicsTimeDiffInSeconds(&tCheck, &tStart);
    }
    if (fd < 0) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s timeout waiting for file to be created %s\n",
            driverName, functionName, fileName);
        if (fileExists) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "  file exists but is more than 10 seconds old, possible clock synchronization problem\n");
        } 
        return(asynError);
    }
    close(fd);

    deltaTime = 0.;
    while (deltaTime <= timeout) {
        /* At this point we know the file exists, but it may not be completely written yet.
         * If we get errors then try again */
        tiff = TIFFOpen(fileName, "rc");
        if (tiff == NULL) {
            status = asynError;
            goto retry;
        }
        
        /* Do some basic checking that the image size is what we expect */
        status = TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &uval);
        if (uval != (epicsUInt32)pImage->dims[0].size) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s, image width incorrect =%u, should be %d\n",
                driverName, functionName, uval, pImage->dims[0].size);
            goto retry;
        }
        status = TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &uval);
        if (uval != (epicsUInt32)pImage->dims[1].size) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s, image length incorrect =%u, should be %d\n",
                driverName, functionName, uval, pImage->dims[1].size);
            goto retry;
        }
        numStrips= TIFFNumberOfStrips(tiff);
        buffer = (char *)pImage->pData;
        totalSize = 0;
        for (strip=0; (strip < numStrips) && (totalSize < pImage->dataSize); strip++) {
            size = TIFFReadEncodedStrip(tiff, 0, buffer, pImage->dataSize-totalSize);
            if (size == -1) {
                /* There was an error reading the file.  Most commonly this is because the file
                 * was not yet completely written.  Try again. */
                asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                    "%s::%s, error reading TIFF file %s\n",
                    driverName, functionName, fileName);
                goto retry;
            }
            buffer += size;
            totalSize += size;
        }
        if (totalSize > pImage->dataSize) {
            status = asynError;
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s, file size too large =%d, must be <= %d\n",
                driverName, functionName, totalSize, pImage->dataSize);
            goto retry;
        }
        /* Sucesss! */
        break;
        
        retry:
        if (tiff != NULL) TIFFClose(tiff);
        tiff = NULL;
        /* Sleep, but check for stop event, which can be used to abort a long acquisition */
        status = epicsEventWaitWithTimeout(this->stopEventId, FILE_READ_DELAY);
        if (status == epicsEventWaitOK) {
            return(asynError);
        }
        epicsTimeGetCurrent(&tCheck);
        deltaTime = epicsTimeDiffInSeconds(&tCheck, &tStart);
    }

    if (tiff != NULL) TIFFClose(tiff);

    return(asynSuccess);
}   

asynStatus marCCD::writeServer(const char *output)
{
    size_t nwrite;
    asynStatus status;
    asynUser *pasynUser = this->pasynUserServer;
    const char *functionName="writeServer";

    /* Flush any stale input, since the next operation is likely to be a read */
    status = pasynOctetSyncIO->flush(pasynUser);
    status = pasynOctetSyncIO->write(pasynUser, output,
                                     strlen(output), MARCCD_SERVER_TIMEOUT,
                                     &nwrite);
                                        
    if (status) asynPrint(pasynUser, ASYN_TRACE_ERROR,
                    "%s:%s, status=%d, sent\n%s\n",
                    driverName, functionName, status, output);

    /* Set output string so it can get back to EPICS */
    setStringParam(ADStringToServer, output);
    callParamCallbacks();
    
    return(status);
}


asynStatus marCCD::readServer(char *input, size_t maxChars, double timeout)
{
    size_t nread;
    asynStatus status=asynSuccess;
    asynUser *pasynUser = this->pasynUserServer;
    int eomReason;
    const char *functionName="readServer";

    status = pasynOctetSyncIO->read(pasynUser, input, maxChars, timeout,
                                    &nread, &eomReason);
    if (status) asynPrint(pasynUser, ASYN_TRACE_ERROR,
                    "%s:%s, timeout=%f, status=%d received %d bytes\n%s\n",
                    driverName, functionName, timeout, status, nread, input);
    /* Set output string so it can get back to EPICS */
    setStringParam(ADStringFromServer, input);
    callParamCallbacks();
    return(status);
}

asynStatus marCCD::writeReadServer(const char *output, char *input, size_t maxChars, double timeout)
{
    asynStatus status;
    
    status = writeServer(output);
    if (status) return status;
    status = readServer(input, maxChars, timeout);
    return status;
}

asynStatus marCCD::writeHeader()
{
    asynStatus status;
    double detectorDistance, beamX, beamY, exposureTime, startPhi, rotationRange, wavelength;
    char rotationAxis[MAX_MESSAGE_SIZE], fileComments[MAX_MESSAGE_SIZE], datasetComments[MAX_MESSAGE_SIZE];
    
    getDoubleParam(marCCDDetectorDistance, &detectorDistance);
    getDoubleParam(marCCDBeamX, &beamX);
    getDoubleParam(marCCDBeamY, &beamY);
    getDoubleParam(ADAcquireTime, &exposureTime);
    getDoubleParam(marCCDStartPhi, &startPhi);
    getDoubleParam(marCCDRotationRange, &rotationRange);
    getDoubleParam(marCCDWavelength, &wavelength);
    getStringParam(marCCDRotationAxis, sizeof(rotationAxis), rotationAxis);
    getStringParam(marCCDFileComments, sizeof(fileComments), fileComments);
    getStringParam(marCCDDatasetComments, sizeof(datasetComments), datasetComments);
    
    epicsSnprintf(this->toServer, sizeof(this->toServer),
                  "header,"
                  "detector_distance=%f,"
                  "beam_x=%f,"
                  "beam_y=%f,"
                  "exposure_time=%f,"
                  "start_phi=%f,"
                  "rotation_axis=%s,"
                  "rotation_range=%f,"
                  "source_wavelength=%f,"
                  "file_comments=%s,"
                  "dataset_comments=%s",
                  detectorDistance,
                  beamX,
                  beamY,
                  exposureTime,
                  startPhi,
                  rotationAxis,
                  rotationRange,
                  wavelength,
                  fileComments,
                  datasetComments);
    status = writeServer(this->toServer);
    return status;
}

int marCCD::getState()
{
    int marState;
    int marStatus;
    ADStatus_t adStatus = ADStatusIdle;
    asynStatus status;
    int acquireStatus, readoutStatus, correctStatus, writingStatus, dezingerStatus;
    
    status = writeReadServer("get_state", this->fromServer, sizeof(this->fromServer),
                              MARCCD_SERVER_TIMEOUT);
    if (status) return(adStatus);
    marState = strtol(this->fromServer, NULL, 0);
    marStatus = TASK_STATE(marState);
    acquireStatus = TASK_STATUS(marState, TASK_ACQUIRE); 
    readoutStatus = TASK_STATUS(marState, TASK_READ); 
    correctStatus = TASK_STATUS(marState, TASK_CORRECT); 
    writingStatus = TASK_STATUS(marState, TASK_WRITE); 
    dezingerStatus = TASK_STATUS(marState, TASK_DEZINGER);
    setIntegerParam(marCCDState, marState);
    setIntegerParam(marCCDStatus, marStatus);
    setIntegerParam(marCCDTaskAcquireStatus, acquireStatus);
    setIntegerParam(marCCDTaskReadoutStatus, readoutStatus);
    setIntegerParam(marCCDTaskCorrectStatus, correctStatus);
    setIntegerParam(marCCDTaskWritingStatus, writingStatus);
    setIntegerParam(marCCDTaskDezingerStatus, dezingerStatus);
    if (marState == 0) adStatus = ADStatusIdle;
    else if (marState == 7) adStatus = ADStatusError;
    else if (marState == 8) adStatus = ADStatusIdle;  /* This is really busy interpreting command,
                                                          but we don't have a status for that yet */
    else if (acquireStatus & (TASK_STATUS_EXECUTING)) adStatus = ADStatusAcquire;
    else if (readoutStatus & (TASK_STATUS_EXECUTING)) adStatus = ADStatusReadout;
    else if (correctStatus & (TASK_STATUS_EXECUTING)) adStatus = ADStatusCorrect;
    else if (writingStatus & (TASK_STATUS_EXECUTING)) adStatus = ADStatusSaving;
    if ((acquireStatus | readoutStatus | correctStatus | writingStatus | dezingerStatus) & 
        TASK_STATUS_ERROR) adStatus = ADStatusError;
    setIntegerParam(ADStatus, adStatus);
    callParamCallbacks();
    return(marState);
}

asynStatus marCCD::getConfig()
{
    int sizeX, sizeY, binX, binY, imageSize, frameShift;
    asynStatus status;
    
    status = writeReadServer("get_size", this->fromServer, sizeof(this->fromServer), MARCCD_SERVER_TIMEOUT);
    if (status) return(status);
    sscanf(this->fromServer, "%d,%d", &sizeX, &sizeY);
    setIntegerParam(ADImageSizeX, sizeX);
    setIntegerParam(ADImageSizeY, sizeY);
    status = writeReadServer("get_bin", this->fromServer, sizeof(this->fromServer), MARCCD_SERVER_TIMEOUT);
    if (status) return(status);
    sscanf(this->fromServer, "%d,%d", &binX, &binY);
    setIntegerParam(ADBinX, binX);
    setIntegerParam(ADBinY, binY);
    setIntegerParam(ADMaxSizeX, sizeX*binX);
    setIntegerParam(ADMaxSizeY, sizeY*binY);
    imageSize = sizeX * sizeY * sizeof(epicsInt16);
    setIntegerParam(ADImageSize, imageSize);
    status = writeReadServer("get_frameshift", this->fromServer, sizeof(this->fromServer),
                              MARCCD_SERVER_TIMEOUT);
    sscanf(this->fromServer, "%d", &frameShift);
    setIntegerParam(marCCDFrameShift, frameShift);
    callParamCallbacks();
    return(asynSuccess);
}

/* This function is called when the exposure time timer expires */
extern "C" {static void timerCallbackC(void *drvPvt)
{
    marCCD *pPvt = (marCCD *)drvPvt;
    
   epicsEventSignal(pPvt->stopEventId);
}}

void marCCD::setShutter(int open)
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
                writeServer("shutter,1");
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
                writeServer("shutter,0");
                epicsThreadSleep(shutterCloseDelay);
            }
            /* The marCCD does not provide a way to read the actual shutter status, so
             * set it to agree with the control value */
            setIntegerParam(ADShutterStatus, open);
            callParamCallbacks();
            break;
        default:
            ADDriver::setShutter(open);
            break;
    }
}


void marCCD::acquireFrame(double exposureTime, int useShutter)
{
    int status;
    epicsTimeStamp startTime, currentTime;
    double timeRemaining;

    /* Wait for the acquire task to be done with the previous acquisition, if any */    
    status = getState();
    while (TEST_TASK_STATUS(status, TASK_ACQUIRE, TASK_STATUS_EXECUTING) || 
           TASK_STATE(status) >= 8) {
        epicsThreadSleep(MARCCD_POLL_DELAY);
        status = getState();
    }

    setStringParam(ADStatusMessage, "Starting exposure");
    writeServer("start");
    callParamCallbacks();
   
    /* Wait for acquisition to actually start */
    status = getState();
    while (!TEST_TASK_STATUS(status, TASK_ACQUIRE, TASK_STATUS_EXECUTING) || 
           TASK_STATE(status) >= 8) {
        epicsThreadSleep(MARCCD_POLL_DELAY);
        status = getState();
    }
    
    /* Set the the start time for the TimeRemaining counter */
    epicsTimeGetCurrent(&startTime);
    timeRemaining = exposureTime;
    if (useShutter) setShutter(1);

    /* Wait for the exposure time using epicsEventWaitWithTimeout, 
     * so we can abort */
    epicsTimerStartDelay(this->timerId, exposureTime);
    while(1) {
        epicsMutexUnlock(this->mutexId);
        status = epicsEventWaitWithTimeout(this->stopEventId, MARCCD_POLL_DELAY);
        epicsMutexLock(this->mutexId);
        if (status == epicsEventWaitOK) {
            /* The acquisition was stopped before the time was complete */
            epicsTimerCancel(this->timerId);
            break;
        }
        epicsTimeGetCurrent(&currentTime);
        timeRemaining = exposureTime - 
            epicsTimeDiffInSeconds(&currentTime, &startTime);
        if (timeRemaining < 0.) timeRemaining = 0.;
        setDoubleParam(ADTimeRemaining, timeRemaining);
        callParamCallbacks();
    }
    setDoubleParam(ADTimeRemaining, 0.0);
    callParamCallbacks();
    if (useShutter) setShutter(0);

}

void marCCD::readoutFrame(int bufferNumber, const char* fileName, int wait)
{
    int status;
    
     /* Wait for the readout task to be done with the previous frame, if any */    
    status = getState();
    while (TEST_TASK_STATUS(status, TASK_READ, TASK_STATUS_EXECUTING | TASK_STATUS_QUEUED) || 
           TASK_STATE(status) >= 8) {
        epicsThreadSleep(MARCCD_POLL_DELAY);
        status = getState();
    }

    if (fileName && strlen(fileName)!=0) {
        epicsSnprintf(this->toServer, sizeof(this->toServer), "readout,%d,%s", bufferNumber, fileName);
        setStringParam(ADFullFileName, fileName);
        callParamCallbacks();
    } else {
        epicsSnprintf(this->toServer, sizeof(this->toServer), "readout,%d", bufferNumber);
    }
    writeServer(this->toServer);

    /* Wait for the readout to start */
    status = getState();
    while (!TEST_TASK_STATUS(status, TASK_READ, TASK_STATUS_EXECUTING | TASK_STATUS_QUEUED)) {
        epicsThreadSleep(MARCCD_POLL_DELAY);
        status = getState();
    }

    /* Wait for the readout to complete */
    status = getState();
    while (TEST_TASK_STATUS(status, TASK_READ, TASK_STATUS_EXECUTING | TASK_STATUS_QUEUED)) {
        epicsThreadSleep(MARCCD_POLL_DELAY);
        status = getState();
    }

    if (!wait) return;
    
    /* Wait for the correction complete */
    status = getState();
    while (TEST_TASK_STATUS(status, TASK_CORRECT, TASK_STATUS_EXECUTING | TASK_STATUS_QUEUED)) {
        epicsThreadSleep(MARCCD_POLL_DELAY);
        status = getState();
    }

    /* If the filename was specified wait for the write to complete */
    if (!fileName || strlen(fileName)==0) return;
    status = getState();
    while (TEST_TASK_STATUS(status, TASK_WRITE, TASK_STATUS_EXECUTING | TASK_STATUS_QUEUED) || 
           TASK_STATE(status) >= 8) {
        epicsThreadSleep(MARCCD_POLL_DELAY);
        status = getState();
    }
}
 
void marCCD::saveFile(int correctedFlag, int wait)
{
    char fullFileName[MAX_FILENAME_LEN];
    int status;

    /* Wait for any previous write to complete */
    status = getState();
    while (TEST_TASK_STATUS(status, TASK_WRITE, TASK_STATUS_EXECUTING | TASK_STATUS_QUEUED) || 
           TASK_STATE(status) >= 8) {
        epicsThreadSleep(MARCCD_POLL_DELAY);
        status = getState();
    }
    writeHeader();
    createFileName(MAX_FILENAME_LEN, fullFileName);
    epicsSnprintf(this->toServer, sizeof(this->toServer), "writefile,%s,%d", 
                  fullFileName, correctedFlag);
    writeServer(this->toServer);
    setStringParam(ADFullFileName, fullFileName);
    callParamCallbacks();
    if (!wait) return;
    status = getState();
    while (TEST_TASK_STATUS(status, TASK_WRITE, TASK_STATUS_EXECUTING | TASK_STATUS_QUEUED) || 
           TASK_STATE(status) >= 8) {
        epicsThreadSleep(MARCCD_POLL_DELAY);
        status = getState();
    }
}

static void marCCDTaskC(void *drvPvt)
{
    marCCD *pPvt = (marCCD *)drvPvt;
    
    pPvt->marCCDTask();
}

void marCCD::marCCDTask()
{
    /* This thread controls acquisition, reads TIFF files to get the image data, and
     * does the callbacks to send it to higher layers */
    int status = asynSuccess;
    int imageCounter;
    int numImages, numImagesCounter;
    int imageMode;
    int acquire;
    int arrayCallbacks;
    double acquireTime;
    double acquirePeriod;
    int frameType;
    int autoSave;
    int overlap, wait;
    int bufferNumber;
    int shutterMode, useShutter;
    double elapsedTime, delayTime;
    const char *functionName = "marCCDTask";
    char fullFileName[MAX_FILENAME_LEN];

    epicsMutexLock(this->mutexId);

    /* Loop forever */
    while (1) {
        /* Is acquisition active? */
        getIntegerParam(ADAcquire, &acquire);
        
        /* If we are not acquiring then wait for a semaphore that is given when acquisition is started */
        if (!acquire) {
            setStringParam(ADStatusMessage, "Waiting for acquire command");
            callParamCallbacks();
            /* Release the lock while we wait for an event that says acquire has started, then lock again */
            epicsMutexUnlock(this->mutexId);
            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
                "%s:%s: waiting for acquire to start\n", driverName, functionName);
            status = epicsEventWait(this->startEventId);
            epicsMutexLock(this->mutexId);
            getIntegerParam(ADAcquire, &acquire);
            setIntegerParam(ADNumImagesCounter, 0);
            callParamCallbacks();
        }
        
        /* Get current values of some parameters */
        getIntegerParam(ADFrameType, &frameType);
        getDoubleParam(ADAcquireTime, &acquireTime);
        getIntegerParam(ADAutoSave, &autoSave);
        getIntegerParam(marCCDOverlap, &overlap);
        getIntegerParam(ADShutterMode, &shutterMode);
        getIntegerParam(ADArrayCallbacks, &arrayCallbacks);
        if (overlap) wait=0; else wait=1;
        if (shutterMode == ADShutterModeNone) useShutter=0; else useShutter=1;
        if (autoSave) writeHeader();
        
        epicsTimeGetCurrent(&this->acqStartTime);
        
        switch(frameType) {
            case marCCDFrameNormal:
            case marCCDFrameRaw:
                strcpy(fullFileName, "");
                if (autoSave) createFileName(MAX_FILENAME_LEN, fullFileName);
                acquireFrame(acquireTime, useShutter);
                if (frameType == marCCDFrameNormal) bufferNumber=0; else bufferNumber=3;
                readoutFrame(bufferNumber, fullFileName, wait);
                break;
            case marCCDFrameBackground:
                acquireFrame(.001, 0);
                readoutFrame(1, NULL, 1);
                acquireFrame(.001, 0);
                readoutFrame(2, NULL, 1);
                writeServer("dezinger,1");
                status = getState();
                while (TEST_TASK_STATUS(status, TASK_DEZINGER, 
                                        TASK_STATUS_EXECUTING | TASK_STATUS_QUEUED) || 
                                        TASK_STATE(status) >= 8) {
                    epicsThreadSleep(MARCCD_POLL_DELAY);
                    status = getState();
                }
                break;
            case marCCDFrameDoubleCorrelation:
                acquireFrame(acquireTime/2., useShutter);
                readoutFrame(2, NULL, 1);
                acquireFrame(acquireTime/2., useShutter);
                readoutFrame(0, NULL, 1);
                writeServer("dezinger,0");
                status = getState();
                while (TEST_TASK_STATUS(status, TASK_DEZINGER, 
                                        TASK_STATUS_EXECUTING | TASK_STATUS_QUEUED) || 
                                        TASK_STATE(status) >= 8) {
                    epicsThreadSleep(MARCCD_POLL_DELAY);
                    status = getState();
                }
                writeServer("correct");
                status = getState();
                while (TEST_TASK_STATUS(status, TASK_CORRECT, 
                                        TASK_STATUS_EXECUTING | TASK_STATUS_QUEUED) || 
                                        TASK_STATE(status) >= 8) {
                    epicsThreadSleep(MARCCD_POLL_DELAY);
                    status = getState();
                }
                if (autoSave) saveFile(1, 1);
        }
        
        getIntegerParam(ADImageCounter, &imageCounter);
        imageCounter++;
        setIntegerParam(ADImageCounter, imageCounter);
        getIntegerParam(ADNumImagesCounter, &numImagesCounter);
        numImagesCounter++;
        setIntegerParam(ADNumImagesCounter, numImagesCounter);
        /* Call the callbacks to update any changes */
        callParamCallbacks();

        /* If we saved a file above and arrayCallbacks is set then read the file back in */
        if (autoSave && arrayCallbacks && (frameType != marCCDFrameBackground)) {
            if (overlap) epicsEventSignal(this->imageEventId);
            else getImageData();
        }

        getIntegerParam(ADImageMode, &imageMode);
        if (imageMode == ADImageMultiple) {
            getIntegerParam(ADNumImages, &numImages);
            if (numImagesCounter >= numImages) setIntegerParam(ADAcquire, 0);
        }    
        if (imageMode == ADImageSingle) setIntegerParam(ADAcquire, 0);
        getIntegerParam(ADAcquire, &acquire);
        if (acquire) {
            /* We are in continuous or multiple mode.
             * Sleep until the acquire period expires or acquire is set to stop */
            epicsTimeGetCurrent(&this->acqEndTime);
            elapsedTime = epicsTimeDiffInSeconds(&this->acqEndTime, &this->acqStartTime);
            getDoubleParam(ADAcquirePeriod, &acquirePeriod);
            delayTime = acquirePeriod - elapsedTime;
            if (delayTime > 0.) {
                setIntegerParam(ADStatus, ADStatusWaiting);
                callParamCallbacks();
                epicsMutexUnlock(this->mutexId);
                status = epicsEventWaitWithTimeout(this->stopEventId, delayTime);
                epicsMutexLock(this->mutexId);
            }
        }

        /* Call the callbacks to update any changes */
        callParamCallbacks();
    }
}


asynStatus marCCD::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function = pasynUser->reason;
    int state, binX, binY;
    int correctedFlag, frameType;
    asynStatus status = asynSuccess;
    const char *functionName = "writeInt32";

    status = setIntegerParam(function, value);

    switch (function) {
    case ADAcquire:
        state = getState();
        if (value && (!TEST_TASK_STATUS(state, TASK_ACQUIRE, TASK_STATUS_QUEUED | TASK_STATUS_EXECUTING))) {
            /* Send an event to wake up the marCCD task.  */
            epicsEventSignal(this->startEventId);
        } 
        if (!value) {
            /* This was a command to stop acquisition */
            epicsEventSignal(this->stopEventId);
        }
        break;
    case ADBinX:
    case ADBinY:
        /* Set binning */
        getIntegerParam(ADBinX, &binX);
        getIntegerParam(ADBinY, &binY);
        epicsSnprintf(this->toServer, sizeof(this->toServer), "set_bin,%d,%d", binX, binY);
        writeServer(this->toServer);
        /* Note, we cannot read back the actual binning values from marCCDServer here because the
         * server only updates them when the next image is collected */
        break;
    case marCCDFrameShift:
         epicsSnprintf(this->toServer, sizeof(this->toServer), "set_frameshift,%d", value);
         writeServer(this->toServer);
         getConfig();
         break;       
    case ADReadStatus:
        if (value) getState();
        break;
    case ADWriteFile:
        getIntegerParam(ADFrameType, &frameType);
        if (frameType == marCCDFrameRaw) correctedFlag=0; else correctedFlag=1;
        saveFile(correctedFlag, 1);
        break;
    case ADShutterControl:
        setShutter(value);
        break;
    default:
        status = ADDriver::writeInt32(pasynUser, value);
        break;
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



/* asynDrvUser routines */
asynStatus marCCD::drvUserCreate(asynUser *pasynUser,
                                      const char *drvInfo, 
                                      const char **pptypeName, size_t *psize)
{
    asynStatus status;
    int param;
    const char *functionName = "drvUserCreate";

    /* See if this is one of our standard parameters */
    status = findParam(marCCDParamString, NUM_MARCCD_PARAMS, 
                       drvInfo, &param);
                                
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
        return(asynSuccess);
    }
    
    /* If not, then see if it is a base class parameter */
    status = ADDriver::drvUserCreate(pasynUser, drvInfo, pptypeName, psize);
    return(status);  
}
    
void marCCD::report(FILE *fp, int details)
{
    fprintf(fp, "MAR-CCD detector %s\n", this->portName);
    if (details > 0) {
        int nx, ny;
        getIntegerParam(ADSizeX, &nx);
        getIntegerParam(ADSizeY, &ny);
        fprintf(fp, "  NX, NY:            %d  %d\n", nx, ny);
    }
    /* Invoke the base class method */
    ADDriver::report(fp, details);
}

extern "C" int marCCDConfig(const char *portName, const char *serverPort, 
                            int maxBuffers, size_t maxMemory)
{
    new marCCD(portName, serverPort, maxBuffers, maxMemory);
    return(asynSuccess);
}

marCCD::marCCD(const char *portName, const char *serverPort,
                                int maxBuffers, size_t maxMemory)

    : ADDriver(portName, 1, ADLastDriverParam, maxBuffers, maxMemory, 0, 0), 
      pData(NULL)

{
    int status = asynSuccess;
    epicsTimerQueueId timerQ;
    const char *functionName = "marCCD";
    int dims[2];

    /* Create the epicsEvents for signaling to the marCCD task when acquisition starts and stops */
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
    this->imageEventId = epicsEventCreate(epicsEventEmpty);
    if (!this->imageEventId) {
        printf("%s:%s epicsEventCreate failure for image event\n", 
            driverName, functionName);
        return;
    }
    
    /* Create the epicsTimerQueue for exposure time handling */
    timerQ = epicsTimerQueueAllocate(1, epicsThreadPriorityScanHigh);
    this->timerId = epicsTimerQueueCreateTimer(timerQ, timerCallbackC, this);
    
    
    /* Connect to server */
    status = pasynOctetSyncIO->connect(serverPort, 0, &this->pasynUserServer, NULL);
    
    /* Read the current state of the server */
    status = getState();
    
    /* Compute the sensor size by reading the image size and the binning */
    status = getConfig();

    /* Allocate the raw buffer we use to readTiff files.  Only do this once */
    getIntegerParam(ADMaxSizeX, &dims[0]);
    getIntegerParam(ADMaxSizeY, &dims[1]);
    this->pData = this->pNDArrayPool->alloc(2, dims, NDInt16, 0, NULL);

    /* Set some default values for parameters */
    status =  setStringParam (ADManufacturer, "MAR");
    status |= setStringParam (ADModel, "CCD");
    status |= setIntegerParam(ADDataType,  NDInt16);
    status |= setIntegerParam(ADImageMode, ADImageSingle);
    status |= setIntegerParam(ADTriggerMode, TMInternal);
    status |= setDoubleParam (ADAcquireTime, 1.);
    status |= setDoubleParam (ADAcquirePeriod, 0.);
    status |= setIntegerParam(ADNumImages, 1);
    status |= setIntegerParam(marCCDOverlap, 0);

    status |= setDoubleParam (marCCDTiffTimeout, 20.);
       
    if (status) {
        printf("%s: unable to set camera parameters\n", functionName);
        return;
    }
    
    /* Create the thread that collects the data */
    status = (epicsThreadCreate("marCCDTask",
                                epicsThreadPriorityMedium,
                                epicsThreadGetStackSize(epicsThreadStackMedium),
                                (EPICSTHREADFUNC)marCCDTaskC,
                                this) == NULL);
    if (status) {
        printf("%s:%s epicsThreadCreate failure for data collection task\n", 
            driverName, functionName);
        return;
    }
    /* Create the thread that collects the data */
    status = (epicsThreadCreate("marCCDImageTask",
                                epicsThreadPriorityMedium,
                                epicsThreadGetStackSize(epicsThreadStackMedium),
                                (EPICSTHREADFUNC)getImageDataTaskC,
                                this) == NULL);
    if (status) {
        printf("%s:%s epicsThreadCreate failure for image task\n", 
            driverName, functionName);
        return;
    }
}
