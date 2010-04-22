/* pilatusDetector.cpp
 *
 * This is a driver for a Pilatus pixel array detector.
 *
 * Author: Mark Rivers
 *         University of Chicago
 *
 * Created:  June 11, 2008
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
#include <epicsMutex.h>
#include <epicsString.h>
#include <epicsStdio.h>
#include <epicsMutex.h>
#include <cantProceed.h>
#include <iocsh.h>
#include <epicsExport.h>

#include <asynOctetSyncIO.h>

#include "ADDriver.h"

/** Messages to/from camserver */
#define MAX_MESSAGE_SIZE 256 
#define MAX_FILENAME_LEN 256
#define MAX_BAD_PIXELS 100
/** Time to poll when reading from camserver */
#define ASYN_POLL_TIME .01 
#define CAMSERVER_DEFAULT_TIMEOUT 1.0 
/** Time between checking to see if TIFF file is complete */
#define FILE_READ_DELAY .01

/** Trigger modes */
typedef enum {
    TMInternal,
    TMExternalEnable,
    TMExternalTrigger,
    TMMultipleExternalTrigger,
    TMAlignment
} PilatusTriggerMode;

/** Bad pixel structure for Pilatus detector */
typedef struct {
    int badIndex;
    int replaceIndex;
} badPixel;


static const char *gainStrings[] = {"lowG", "midG", "highG", "uhighG"};

static const char *driverName = "pilatusDetector";

#define PilatusDelayTimeString      "DELAY_TIME"
#define PilatusThresholdString      "THRESHOLD"
#define PilatusArmedString          "ARMED"
#define PilatusBadPixelFileString   "BAD_PIXEL_FILE"
#define PilatusNumBadPixelsString   "NUM_BAD_PIXELS"
#define PilatusFlatFieldFileString  "FLAT_FIELD_FILE"
#define PilatusMinFlatFieldString   "MIN_FLAT_FIELD"
#define PilatusFlatFieldValidString "FLAT_FIELD_VALID"
#define PilatusTiffTimeoutString    "TIFF_TIMEOUT"


/** Driver for Dectris Pilatus pixel array detectors using their camserver server over TCP/IP socket */
class pilatusDetector : public ADDriver {
public:
    pilatusDetector(const char *portName, const char *camserverPort,
                    int maxSizeX, int maxSizeY,
                    int maxBuffers, size_t maxMemory,
                    int priority, int stackSize);
                 
    /* These are the methods that we override from ADDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual asynStatus writeOctet(asynUser *pasynUser, const char *value, 
                                    size_t nChars, size_t *nActual);
    void report(FILE *fp, int details);
    void pilatusTask(); /* This should be private but is called from C so must be public */
 
 protected:
    int PilatusDelayTime;
    #define FIRST_PILATUS_PARAM PilatusDelayTime
    int PilatusThreshold;
    int PilatusArmed;
    int PilatusTiffTimeout;
    int PilatusBadPixelFile;
    int PilatusNumBadPixels;
    int PilatusFlatFieldFile;
    int PilatusMinFlatField;
    int PilatusFlatFieldValid;
    #define LAST_PILATUS_PARAM PilatusFlatFieldValid

 private:                                       
    /* These are the methods that are new to this class */
    void abortAcquisition();
    void makeMultipleFileFormat(const char *baseFileName);
    asynStatus readTiff(const char *fileName, epicsTimeStamp *pStartTime, double timeout, NDArray *pImage);
    asynStatus writeCamserver(double timeout);
    asynStatus readCamserver(double timeout);
    asynStatus writeReadCamserver(double timeout);
    asynStatus setAcquireParams();
    asynStatus setThreshold();
    void readBadPixelFile(const char *badPixelFile);
    void readFlatFieldFile(const char *flatFieldFile);
   
    /* Our data */
    int imagesRemaining;
    epicsEventId startEventId;
    epicsEventId stopEventId;
    char toCamserver[MAX_MESSAGE_SIZE];
    char fromCamserver[MAX_MESSAGE_SIZE];
    NDArray *pFlatField;
    char multipleFileFormat[MAX_FILENAME_LEN];
    int multipleFileNumber;
    asynUser *pasynUserCamserver;
    badPixel badPixelMap[MAX_BAD_PIXELS];
    double averageFlatField;
};

#define NUM_PILATUS_PARAMS (&LAST_PILATUS_PARAM - &FIRST_PILATUS_PARAM + 1)

void pilatusDetector::readBadPixelFile(const char *badPixelFile)
{
    int i; 
    int xbad, ybad, xgood, ygood;
    int n;
    FILE *file;
    int nx, ny;
    const char *functionName = "readBadPixelFile";
    int numBadPixels=0;

    getIntegerParam(NDArraySizeX, &nx);
    getIntegerParam(NDArraySizeY, &ny);
    setIntegerParam(PilatusNumBadPixels, numBadPixels);
    if (strlen(badPixelFile) == 0) return;
    file = fopen(badPixelFile, "r");
    if (file == NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s, cannot open file %s\n",
            driverName, functionName, badPixelFile);
        return;
    }
    for (i=0; i<MAX_BAD_PIXELS; i++) {
        n = fscanf(file, " %d,%d %d,%d",
                  &xbad, &ybad, &xgood, &ygood);
        if (n == EOF) break;
        if (n != 4) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s, too few items =%d, should be 4\n",
                driverName, functionName, n);
            return;
        }
        this->badPixelMap[i].badIndex = ybad*nx + xbad;
        this->badPixelMap[i].replaceIndex = ygood*ny + xgood;
        numBadPixels++;
    }
    setIntegerParam(PilatusNumBadPixels, numBadPixels);
}


void pilatusDetector::readFlatFieldFile(const char *flatFieldFile)
{
    int i;
    int status;
    int ngood;
    int minFlatField;
    epicsInt32 *pData;
    const char *functionName = "readFlatFieldFile";
    NDArrayInfo arrayInfo;
    
    setIntegerParam(PilatusFlatFieldValid, 0);
    this->pFlatField->getInfo(&arrayInfo);
    getIntegerParam(PilatusMinFlatField, &minFlatField);
    if (strlen(flatFieldFile) == 0) return;
    status = readTiff(flatFieldFile, NULL, 0., this->pFlatField);
    if (status) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s, error reading flat field file %s\n",
            driverName, functionName, flatFieldFile);
        return;
    }
    /* Compute the average counts in the flat field */
    this->averageFlatField = 0.;
    ngood = 0;
    
    for (i=0, pData = (epicsInt32 *)this->pFlatField->pData; 
         i<arrayInfo.nElements; 
         i++, pData++) {
        if (*pData < minFlatField) continue;
        ngood++;
        averageFlatField += *pData;
    }
    averageFlatField = averageFlatField/ngood;
    
    for (i=0, pData = (epicsInt32 *)this->pFlatField->pData; 
         i<arrayInfo.nElements; 
         i++, pData++) {
        if (*pData < minFlatField) *pData = (epicsInt32)averageFlatField;
    }
    /* Call the NDArray callback */
    /* Must release the lock here, or we can get into a deadlock, because we can
     * block on the plugin lock, and the plugin can be calling us */
    this->unlock();
    doCallbacksGenericPointer(this->pFlatField, NDArrayData, 0);
    this->lock();
    setIntegerParam(PilatusFlatFieldValid, 1);
}


void pilatusDetector::makeMultipleFileFormat(const char *baseFileName)
{
    /* This function uses the code from camserver */
    char *p, *q;
    int fmt;
    char mfTempFormat[MAX_FILENAME_LEN];
    char mfExtension[10];
    int numImages;
    
    /* baseFilename has been built by the caller.
     * Copy to temp */
    strncpy(mfTempFormat, baseFileName, sizeof(mfTempFormat));
    getIntegerParam(ADNumImages, &numImages);
    p = mfTempFormat + strlen(mfTempFormat) - 5; /* look for extension */
    if ( (q=strrchr(p, '.')) ) {
        strcpy(mfExtension, q);
        *q = '\0';
    } else {
        strcpy(mfExtension, ""); /* default is raw image */
    }
    multipleFileNumber=0;   /* start number */
    fmt=5;        /* format length */
    if ( !(p=strrchr(mfTempFormat, '/')) ) {
        p=mfTempFormat;
    }
    if ( (q=strrchr(p, '_')) ) {
        q++;
        if (isdigit(*q) && isdigit(*(q+1)) && isdigit(*(q+2))) {
            multipleFileNumber=atoi(q);
            fmt=0;
            p=q;
            while(isdigit(*q)) {
                fmt++;
                q++;
            }
            *p='\0';
            if (((fmt<3)  || ((fmt==3) && (numImages>999))) || 
                ((fmt==4) && (numImages>9999))) { 
                fmt=5;
            }
        } else if (*q) {
            strcat(p, "_"); /* force '_' ending */
        }
    } else {
        strcat(p, "_"); /* force '_' ending */
    }
    /* Build the final format string */
    epicsSnprintf(this->multipleFileFormat, sizeof(this->multipleFileFormat), "%s%%.%dd%s",
                  mfTempFormat, fmt, mfExtension);
}


/** This function reads TIFF files using libTiff.  It is not intended to be general,
 * it is intended to read the TIFF files that camserver creates.  It checks to make sure
 * that the creation time of the file is after a start time passed to it, to force it to
 * wait for a new file to be created.
 */
 
asynStatus pilatusDetector::readTiff(const char *fileName, epicsTimeStamp *pStartTime, double timeout, NDArray *pImage)
{
    int fd=-1;
    int fileExists=0;
    struct stat statBuff;
    epicsTimeStamp tStart, tCheck;
    time_t acqStartTime;
    double deltaTime;
    int status=-1;
    const char *functionName = "readTiff";
    int size, totalSize;
    int numStrips, strip;
    char *buffer;
    TIFF *tiff=NULL;
    epicsUInt32 uval;
    int i;
    int numBadPixels;

    deltaTime = 0.;
    if (pStartTime) epicsTimeToTime_t(&acqStartTime, pStartTime);
    epicsTimeGetCurrent(&tStart);
    
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
            if (difftime(statBuff.st_mtime, acqStartTime) > -10) break;
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
        if (totalSize != pImage->dataSize) {
            status = asynError;
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s, file size incorrect =%d, should be %d\n",
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

    /* Correct the bad pixels */
    getIntegerParam(PilatusNumBadPixels, &numBadPixels);
    for (i=0; i<numBadPixels; i++) {
        ((epicsInt32 *)pImage->pData)[this->badPixelMap[i].badIndex] = 
        ((epicsInt32 *)pImage->pData)[this->badPixelMap[i].replaceIndex];
    }    
    return(asynSuccess);
}   

asynStatus pilatusDetector::setAcquireParams()
{
    int ival;
    double dval;
    int triggerMode;
    
    getIntegerParam(ADTriggerMode, &triggerMode);
    
     /* When we change modes download all exposure parameters, since some modes
     * replace values with new parameters */
    if (triggerMode == TMAlignment) {
        setIntegerParam(ADNumImages, 1);
    }
    /* nexpf > 1 is only supported in External Enable mode */  
    if (triggerMode != TMExternalEnable) {
        setIntegerParam(ADNumExposures, 1);
    }

    getIntegerParam(ADNumImages, &ival);
    epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "nimages %d", ival);
    writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT); 

    getIntegerParam(ADNumExposures, &ival);
    epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "nexpframe %d", ival);
    writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT); 

    getDoubleParam(ADAcquireTime, &dval);
    epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "exptime %f", dval);
    writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);

    getDoubleParam(ADAcquirePeriod, &dval);
    epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "expperiod %f", dval);
    writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);

    getDoubleParam(PilatusDelayTime, &dval);
    epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "delay %f", dval);
    writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
    
    return(asynSuccess);

}

asynStatus pilatusDetector::setThreshold()
{
    int igain;
    double threshold, dgain;
    
    getDoubleParam(ADGain, &dgain);
    igain = (int)(dgain + 0.5);
    if (igain < 0) igain = 0;
    if (igain > 3) igain = 3;
    getDoubleParam(PilatusThreshold, &threshold);
    epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "SetThreshold %s %f", 
                    gainStrings[igain], threshold*1000.);
    writeReadCamserver(3.0);  /* This command can take a few seconds */
    return(asynSuccess);
}

asynStatus pilatusDetector::writeCamserver(double timeout)
{
    size_t nwrite;
    asynStatus status;
    const char *functionName="writeCamserver";

    /* Flush any stale input, since the next operation is likely to be a read */
    status = pasynOctetSyncIO->flush(this->pasynUserCamserver);
    status = pasynOctetSyncIO->write(this->pasynUserCamserver, this->toCamserver,
                                     strlen(this->toCamserver), timeout,
                                     &nwrite);
                                        
    if (status) asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s, status=%d, sent\n%s\n",
                    driverName, functionName, status, this->toCamserver);

    /* Set output string so it can get back to EPICS */
    setStringParam(ADStringToServer, this->toCamserver);
    
    return(status);
}


asynStatus pilatusDetector::readCamserver(double timeout)
{
    size_t nread;
    asynStatus status=asynSuccess;
    int eventStatus;
    asynUser *pasynUser = this->pasynUserCamserver;
    int eomReason;
    epicsTimeStamp tStart, tCheck;
    double deltaTime;
    const char *functionName="readCamserver";

    /* We implement the timeout with a loop so that the port does not
     * block during the entire read.  If we don't do this then it is not possible
     * to abort a long exposure */
    deltaTime = 0;
    epicsTimeGetCurrent(&tStart);
    while (deltaTime <= timeout) {
        status = pasynOctetSyncIO->read(pasynUser, this->fromCamserver,
                                        sizeof(this->fromCamserver), ASYN_POLL_TIME,
                                        &nread, &eomReason);
        if (status != asynTimeout) break;
        /* Sleep, but check for stop event, which can be used to abort a long acquisition */
        eventStatus = epicsEventWaitWithTimeout(this->stopEventId, ASYN_POLL_TIME);
        if (eventStatus == epicsEventWaitOK) {
            return(asynError);
        }
        epicsTimeGetCurrent(&tCheck);
        deltaTime = epicsTimeDiffInSeconds(&tCheck, &tStart);
    }

    if (status) asynPrint(pasynUser, ASYN_TRACE_ERROR,
                    "%s:%s, timeout=%f, status=%d received %d bytes\n%s\n",
                    driverName, functionName, timeout, status, nread, this->fromCamserver);
    else {
        /* Look for the string OK in the response */
        if (!strstr(this->fromCamserver, "OK"))
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                    "%s:%s unexpected response from camserver, no OK, response=%s\n",
                    driverName, functionName, this->fromCamserver);
    }

    /* Set output string so it can get back to EPICS */
    setStringParam(ADStringFromServer, this->fromCamserver);

    return(status);
}

asynStatus pilatusDetector::writeReadCamserver(double timeout)
{
    asynStatus status;
    
    status = writeCamserver(timeout);
    if (status) return status;
    status = readCamserver(timeout);
    return status;
}

static void pilatusTaskC(void *drvPvt)
{
    pilatusDetector *pPvt = (pilatusDetector *)drvPvt;
    
    pPvt->pilatusTask();
}

/** This thread controls acquisition, reads TIFF files to get the image data, and
  * does the callbacks to send it to higher layers */
void pilatusDetector::pilatusTask()
{
    int status = asynSuccess;
    int imageCounter;
    int numImages;
    int multipleFileNextImage;  /* This is the next image number, starting at 0 */
    int acquire;
    ADStatus_t acquiring;
    NDArray *pImage;
    double acquireTime, acquirePeriod;
    double readTiffTimeout, timeout;
    int triggerMode;
    epicsTimeStamp startTime;
    const char *functionName = "pilatusTask";
    char fullFileName[MAX_FILENAME_LEN];
    char filePath[MAX_FILENAME_LEN];
    char statusMessage[MAX_MESSAGE_SIZE];
    int dims[2];
    int arrayCallbacks;
    int flatFieldValid;

    this->lock();

    /* Loop forever */
    while (1) {
        /* Is acquisition active? */
        getIntegerParam(ADAcquire, &acquire);
        
        /* If we are not acquiring then wait for a semaphore that is given when acquisition is started */
        if (!acquire) {
            setStringParam(ADStatusMessage, "Waiting for acquire command");
            setIntegerParam(ADStatus, ADStatusIdle);
            callParamCallbacks();
            /* Release the lock while we wait for an event that says acquire has started, then lock again */
            this->unlock();
            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
                "%s:%s: waiting for acquire to start\n", driverName, functionName);
            status = epicsEventWait(this->startEventId);
            this->lock();
            getIntegerParam(ADAcquire, &acquire);
        }
        
        /* We are acquiring. */
        /* Get the current time */
        epicsTimeGetCurrent(&startTime);
        
        /* Get the exposure parameters */
        getDoubleParam(ADAcquireTime, &acquireTime);
        getDoubleParam(ADAcquirePeriod, &acquirePeriod);
        getDoubleParam(PilatusTiffTimeout, &readTiffTimeout);
        
        /* Get the acquisition parameters */
        getIntegerParam(ADTriggerMode, &triggerMode);
        getIntegerParam(ADNumImages, &numImages);
        
        acquiring = ADStatusAcquire;
        setIntegerParam(ADStatus, acquiring);

        /* Create the full filename */
        createFileName(sizeof(fullFileName), fullFileName);
        
        switch (triggerMode) {
            case TMInternal:
                epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), 
                    "Exposure %s", fullFileName);
                break;
            case TMExternalEnable:
                epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), 
                    "ExtEnable %s", fullFileName);
                break;
            case TMExternalTrigger:
                epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), 
                    "ExtTrigger %s", fullFileName);
                break;
            case TMMultipleExternalTrigger:
                epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), 
                    "ExtMTrigger %s", fullFileName);
                break;
            case TMAlignment:
                getStringParam(NDFilePath, sizeof(filePath), filePath);
                epicsSnprintf(fullFileName, sizeof(fullFileName), "%salignment.tif", 
                              filePath);
                epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), 
                    "Exposure %s", fullFileName);
                break;
        }
        setStringParam(ADStatusMessage, "Starting exposure");
        /* Send the acquire command to camserver and wait for the 15OK response */
        writeReadCamserver(2.0);
        /* Open the shutter */
        setShutter(1);
        /* Set the armed flag */
        setIntegerParam(PilatusArmed, 1);
        /* Create the format string for constructing file names for multi-image collection */
        makeMultipleFileFormat(fullFileName);
        multipleFileNextImage = 0;
        /* Call the callbacks to update any changes */
        setStringParam(NDFullFileName, fullFileName);
        callParamCallbacks();

        while (acquire) {
            if (numImages == 1) {
                /* For single frame or alignment mode need to wait for 7OK response from camserver
                 * saying acquisition is complete before trying to read file, else we get a
                 * recent but stale file. */
                setStringParam(ADStatusMessage, "Waiting for 7OK response");
                callParamCallbacks();
                /* We release the mutex when waiting for 7OK because this takes a long time and
                 * we need to allow abort operations to get through */
                this->unlock();
                status = readCamserver(acquireTime + readTiffTimeout);
                this->lock();
                /* If there was an error jump to bottom of loop */
                if (status) {
                    acquire = 0;
                    continue;
                }
             } else {
                /* If this is a multi-file acquisition the file name is built differently */
                epicsSnprintf(fullFileName, sizeof(fullFileName), multipleFileFormat, 
                              multipleFileNumber);
            }
            getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
            getIntegerParam(NDArrayCounter, &imageCounter);
            imageCounter++;
            setIntegerParam(NDArrayCounter, imageCounter);
            /* Call the callbacks to update any changes */
            callParamCallbacks();

            if (arrayCallbacks) {
                /* Get an image buffer from the pool */
                getIntegerParam(ADMaxSizeX, &dims[0]);
                getIntegerParam(ADMaxSizeY, &dims[1]);
                pImage = this->pNDArrayPool->alloc(2, dims, NDInt32, 0, NULL);
                epicsSnprintf(statusMessage, sizeof(statusMessage), "Reading TIFF file %s", fullFileName);
                setStringParam(ADStatusMessage, statusMessage);
                callParamCallbacks();
                /* We release the mutex when calling readTiff, because this takes a long time and
                 * we need to allow abort operations to get through */
                this->unlock();
                status = readTiff(fullFileName, &startTime, acquireTime + readTiffTimeout, pImage); 
                this->lock();
                /* If there was an error jump to bottom of loop */
                if (status) {
                    acquire = 0;
                    pImage->release();
                    continue;
                }

                getIntegerParam(PilatusFlatFieldValid, &flatFieldValid);
                if (flatFieldValid) {
                    epicsInt32 *pData, *pFlat, i;
                    for (i=0, pData = (epicsInt32 *)pImage->pData, pFlat = (epicsInt32 *)this->pFlatField->pData;
                         i<dims[0]*dims[1]; 
                         i++, pData++, pFlat++) {
                        *pData = (epicsInt32)((this->averageFlatField * *pData) / *pFlat);
                    }
                } 
                /* Put the frame number and time stamp into the buffer */
                pImage->uniqueId = imageCounter;
                pImage->timeStamp = startTime.secPastEpoch + startTime.nsec / 1.e9;

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
            if (numImages == 1) {
                if (triggerMode == TMAlignment) {
                   epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), 
                        "Exposure %s", fullFileName);
                    /* Send the acquire command to camserver and wait for the 15OK response */
                    writeReadCamserver(2.0);
                } else {
                    acquire = 0;
                }
            } else if (numImages > 1) {
                multipleFileNextImage++;
                multipleFileNumber++;
                if (multipleFileNextImage == numImages) acquire = 0;
            }
            
        }
        /* We are done acquiring */
        /* Wait for the 7OK response from camserver in the case of multiple images */
        if ((numImages > 1) && (status == asynSuccess)) {
            /* If arrayCallbacks is 0we will have gone through the above loop without waiting
             * for each TIFF file to be written.  Thus, we may need to wait a long time for
             * the 7OK response.  
             * If arrayCallbacks is 1 then the response should arrive fairly soon. */
            if (arrayCallbacks) 
                timeout = readTiffTimeout;
            else 
                timeout = numImages * acquireTime + readTiffTimeout;
            setStringParam(ADStatusMessage, "Waiting for 7OK response");
            callParamCallbacks();
            readCamserver(timeout);
        }
        setShutter(0);
        setIntegerParam(ADAcquire, 0);
        setIntegerParam(PilatusArmed, 0);

        /* Call the callbacks to update any changes */
        callParamCallbacks();
    }
}


/** Called when asyn clients call pasynInt32->write().
  * This function performs actions for some parameters, including ADAcquire, ADTriggerMode, etc.
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Value to write. */
asynStatus pilatusDetector::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function = pasynUser->reason;
    int adstatus;
    asynStatus status = asynSuccess;
    const char *functionName = "writeInt32";

    status = setIntegerParam(function, value);

    if (function == ADAcquire) {
        getIntegerParam(ADStatus, &adstatus);
        if (value && (adstatus == ADStatusIdle)) {
            /* Send an event to wake up the Pilatus task.  */
            epicsEventSignal(this->startEventId);
        } 
        if (!value && (adstatus != ADStatusIdle)) {
            /* This was a command to stop acquisition */
            epicsEventSignal(this->stopEventId);
            epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "Stop");
            writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
            epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "K");
            writeCamserver(CAMSERVER_DEFAULT_TIMEOUT);
        }
    } else if ((function == ADTriggerMode) ||
               (function == ADNumImages) ||
               (function == ADNumExposures)) {
        setAcquireParams();
    } else { 
        /* If this parameter belongs to a base class call its method */
        if (function < FIRST_PILATUS_PARAM) status = ADDriver::writeInt32(pasynUser, value);
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


/** Called when asyn clients call pasynFloat64->write().
  * This function performs actions for some parameters, including ADAcquireTime, ADGain, etc.
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Value to write. */
asynStatus pilatusDetector::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char *functionName = "writeFloat64";

    /* Set the parameter and readback in the parameter library.  This may be overwritten when we read back the
     * status at the end, but that's OK */
    status = setDoubleParam(function, value);

    /* Changing any of the following parameters requires recomputing the base image */
    if ((function == ADGain) ||
        (function == PilatusThreshold)) {
        setThreshold();
    } else if ((function == ADAcquireTime) ||
               (function == ADAcquirePeriod) ||
               (function == PilatusDelayTime)) {
        setAcquireParams();
    } else {
        /* If this parameter belongs to a base class call its method */
        if (function < FIRST_PILATUS_PARAM) status = ADDriver::writeFloat64(pasynUser, value);
    }

    /* Do callbacks so higher layers see any changes */
    callParamCallbacks();
    if (status) 
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
              "%s:%s error, status=%d function=%d, value=%f\n", 
              driverName, functionName, status, function, value);
    else        
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, value=%f\n", 
              driverName, functionName, function, value);
    return status;
}

/** Called when asyn clients call pasynOctet->write().
  * This function performs actions for some parameters, including PilatusBadPixelFile, ADFilePath, etc.
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Address of the string to write.
  * \param[in] nChars Number of characters to write.
  * \param[out] nActual Number of characters actually written. */
asynStatus pilatusDetector::writeOctet(asynUser *pasynUser, const char *value, 
                                    size_t nChars, size_t *nActual)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char *functionName = "writeOctet";

    /* Set the parameter in the parameter library. */
    status = (asynStatus)setStringParam(function, (char *)value);

    if (function == PilatusBadPixelFile) {
        this->readBadPixelFile(value);
    } else if (function == PilatusFlatFieldFile) {
        this->readFlatFieldFile(value);
    } else if (function == NDFilePath) {
        epicsSnprintf(this->toCamserver, sizeof(this->toCamserver), "imgpath %s", value);
        writeReadCamserver(CAMSERVER_DEFAULT_TIMEOUT);
    } else {
        /* If this parameter belongs to a base class call its method */
        if (function < FIRST_PILATUS_PARAM) status = ADDriver::writeOctet(pasynUser, value, nChars, nActual);
    }
    
     /* Do callbacks so higher layers see any changes */
    status = (asynStatus)callParamCallbacks();

    if (status) 
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize, 
                  "%s:%s: status=%d, function=%d, value=%s", 
                  driverName, functionName, status, function, value);
    else        
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, value=%s\n", 
              driverName, functionName, function, value);
    *nActual = nChars;
    return status;
}



/** Report status of the driver.
  * Prints details about the driver if details>0.
  * It then calls the ADDriver::report() method.
  * \param[in] fp File pointed passed by caller where the output is written to.
  * \param[in] details If >0 then driver details are printed.
  */
void pilatusDetector::report(FILE *fp, int details)
{

    fprintf(fp, "Pilatus detector %s\n", this->portName);
    if (details > 0) {
        int nx, ny, dataType;
        getIntegerParam(ADSizeX, &nx);
        getIntegerParam(ADSizeY, &ny);
        getIntegerParam(NDDataType, &dataType);
        fprintf(fp, "  NX, NY:            %d  %d\n", nx, ny);
        fprintf(fp, "  Data type:         %d\n", dataType);
    }
    /* Invoke the base class method */
    ADDriver::report(fp, details);
}

extern "C" int pilatusDetectorConfig(const char *portName, const char *camserverPort, 
                                    int maxSizeX, int maxSizeY,
                                    int maxBuffers, size_t maxMemory,
                                    int priority, int stackSize)
{
    new pilatusDetector(portName, camserverPort, maxSizeX, maxSizeY, maxBuffers, maxMemory,
                        priority, stackSize);
    return(asynSuccess);
}

/** Constructor for Pilatus driver; most parameters are simply passed to ADDriver::ADDriver.
  * After calling the base class constructor this method creates a thread to collect the detector data, 
  * and sets reasonable default values for the parameters defined in this class, asynNDArrayDriver, and ADDriver.
  * \param[in] portName The name of the asyn port driver to be created.
  * \param[in] camserverPort The name of the asyn port previously created with drvAsynIPPortConfigure to
  *            communicate with camserver.
  * \param[in] maxSizeX The size of the Pilatus detector in the X direction.
  * \param[in] maxSizeY The size of the Pilatus detector in the Y direction.
  * \param[in] portName The name of the asyn port driver to be created.
  * \param[in] maxBuffers The maximum number of NDArray buffers that the NDArrayPool for this driver is 
  *            allowed to allocate. Set this to -1 to allow an unlimited number of buffers.
  * \param[in] maxMemory The maximum amount of memory that the NDArrayPool for this driver is 
  *            allowed to allocate. Set this to -1 to allow an unlimited amount of memory.
  * \param[in] priority The thread priority for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  * \param[in] stackSize The stack size for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  */
pilatusDetector::pilatusDetector(const char *portName, const char *camserverPort,
                                int maxSizeX, int maxSizeY,
                                int maxBuffers, size_t maxMemory,
                                int priority, int stackSize)

    : ADDriver(portName, 1, NUM_PILATUS_PARAMS, maxBuffers, maxMemory,
               0, 0,             /* No interfaces beyond those set in ADDriver.cpp */
               ASYN_CANBLOCK, 1, /* ASYN_CANBLOCK=1, ASYN_MULTIDEVICE=0, autoConnect=1 */
               priority, stackSize),
      imagesRemaining(0)

{
    int status = asynSuccess;
    const char *functionName = "pilatusDetector";
    int dims[2];

    /* Create the epicsEvents for signaling to the pilatus task when acquisition starts and stops */
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
    
    /* Allocate the raw buffer we use to readTiff files.  Only do this once */
    dims[0] = maxSizeX;
    dims[1] = maxSizeY;
    /* Allocate the raw buffer we use for flat fields. */
    this->pFlatField = this->pNDArrayPool->alloc(2, dims, NDUInt32, 0, NULL);
    
    /* Connect to camserver */
    status = pasynOctetSyncIO->connect(camserverPort, 0, &this->pasynUserCamserver, NULL);

    createParam(PilatusDelayTimeString,      asynParamFloat64, &PilatusDelayTime);
    createParam(PilatusThresholdString,      asynParamFloat64, &PilatusThreshold);
    createParam(PilatusArmedString,          asynParamInt32,   &PilatusArmed);
    createParam(PilatusTiffTimeoutString,    asynParamFloat64, &PilatusTiffTimeout);
    createParam(PilatusBadPixelFileString,   asynParamOctet,   &PilatusBadPixelFile);
    createParam(PilatusNumBadPixelsString,   asynParamInt32,   &PilatusNumBadPixels);
    createParam(PilatusFlatFieldFileString,  asynParamOctet,   &PilatusFlatFieldFile);
    createParam(PilatusMinFlatFieldString,   asynParamInt32,   &PilatusMinFlatField);
    createParam(PilatusFlatFieldValidString, asynParamInt32,   &PilatusFlatFieldValid);

    /* Set some default values for parameters */
    status =  setStringParam (ADManufacturer, "Dectris");
    status |= setStringParam (ADModel, "Pilatus");
    status |= setIntegerParam(ADMaxSizeX, maxSizeX);
    status |= setIntegerParam(ADMaxSizeY, maxSizeY);
    status |= setIntegerParam(ADSizeX, maxSizeX);
    status |= setIntegerParam(ADSizeX, maxSizeX);
    status |= setIntegerParam(ADSizeY, maxSizeY);
    status |= setIntegerParam(NDArraySizeX, maxSizeX);
    status |= setIntegerParam(NDArraySizeY, maxSizeY);
    status |= setIntegerParam(NDArraySize, 0);
    status |= setIntegerParam(NDDataType,  NDUInt32);
    status |= setIntegerParam(ADImageMode, ADImageContinuous);
    status |= setIntegerParam(ADTriggerMode, TMInternal);
    status |= setDoubleParam (ADAcquireTime, .001);
    status |= setDoubleParam (ADAcquirePeriod, .005);
    status |= setIntegerParam(ADNumImages, 100);

    status |= setIntegerParam(PilatusArmed, 0);
    status |= setDoubleParam (PilatusThreshold, 10.0);
    status |= setDoubleParam (PilatusDelayTime, 0);
    status |= setDoubleParam (PilatusTiffTimeout, 20.);
    status |= setStringParam (PilatusBadPixelFile, "");
    status |= setIntegerParam(PilatusNumBadPixels, 0);
    status |= setStringParam (PilatusFlatFieldFile, "");
    status |= setIntegerParam(PilatusMinFlatField, 100);
    status |= setIntegerParam(PilatusFlatFieldValid, 0);
       
    if (status) {
        printf("%s: unable to set camera parameters\n", functionName);
        return;
    }
    
    /* Create the thread that updates the images */
    status = (epicsThreadCreate("PilatusDetTask",
                                epicsThreadPriorityMedium,
                                epicsThreadGetStackSize(epicsThreadStackMedium),
                                (EPICSTHREADFUNC)pilatusTaskC,
                                this) == NULL);
    if (status) {
        printf("%s:%s epicsThreadCreate failure for image task\n", 
            driverName, functionName);
        return;
    }
}

/* Code for iocsh registration */
static const iocshArg pilatusDetectorConfigArg0 = {"Port name", iocshArgString};
static const iocshArg pilatusDetectorConfigArg1 = {"camserver port name", iocshArgString};
static const iocshArg pilatusDetectorConfigArg2 = {"maxSizeX", iocshArgInt};
static const iocshArg pilatusDetectorConfigArg3 = {"maxSizeY", iocshArgInt};
static const iocshArg pilatusDetectorConfigArg4 = {"maxBuffers", iocshArgInt};
static const iocshArg pilatusDetectorConfigArg5 = {"maxMemory", iocshArgInt};
static const iocshArg pilatusDetectorConfigArg6 = {"priority", iocshArgInt};
static const iocshArg pilatusDetectorConfigArg7 = {"stackSize", iocshArgInt};
static const iocshArg * const pilatusDetectorConfigArgs[] =  {&pilatusDetectorConfigArg0,
                                                              &pilatusDetectorConfigArg1,
                                                              &pilatusDetectorConfigArg2,
                                                              &pilatusDetectorConfigArg3,
                                                              &pilatusDetectorConfigArg4,
                                                              &pilatusDetectorConfigArg5,
                                                              &pilatusDetectorConfigArg6,
                                                              &pilatusDetectorConfigArg7};
static const iocshFuncDef configPilatusDetector = {"pilatusDetectorConfig", 8, pilatusDetectorConfigArgs};
static void configPilatusDetectorCallFunc(const iocshArgBuf *args)
{
    pilatusDetectorConfig(args[0].sval, args[1].sval, args[2].ival,  args[3].ival,  
                          args[4].ival, args[5].ival, args[6].ival,  args[7].ival);
}


static void pilatusDetectorRegister(void)
{

    iocshRegister(&configPilatusDetector, configPilatusDetectorCallFunc);
}

extern "C" {
epicsExportRegistrar(pilatusDetectorRegister);
}

