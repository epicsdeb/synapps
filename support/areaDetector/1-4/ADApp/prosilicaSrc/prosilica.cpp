/* prosilica.cpp
 *
 * This is a driver for Prosilica cameras (GigE and CameraLink).
 *
 * Author: Mark Rivers
 *         University of Chicago
 *
 * Created:  March 20, 2008
 *
 */
 
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef linux
#include <readline/readline.h>
#endif

#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsEvent.h>
#include <epicsString.h>
#include <epicsStdio.h>
#include <epicsMutex.h>
#include <cantProceed.h>

#include "PvApi.h"
#include "ImageLib.h"

#include "ADStdDriverParams.h"
#include "NDArray.h"
#include "ADDriver.h"

#include "drvProsilica.h"

static const char *driverName = "prosilica";

static int PvApiInitialized;

#define MAX_FRAMES  2  /* Number of frame buffers for PvApi */
#define MAX_PACKET_SIZE 8228
                                      
class prosilica : public ADDriver {
public:
    prosilica(const char *portName, int uniqueId, int maxBuffers, size_t maxMemory);
                 
    /* These are the methods that we override from ADDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual asynStatus drvUserCreate(asynUser *pasynUser, const char *drvInfo, 
                                     const char **pptypeName, size_t *psize);
    void report(FILE *fp, int details);
                                        
    /* These are the methods that are new to this class */
    asynStatus writeFile();
    void frameCallback(tPvFrame *pFrame);
    asynStatus setPixelFormat();
    asynStatus setGeometry();
    asynStatus getGeometry();
    asynStatus readStats();
    asynStatus readParameters();
    asynStatus disconnectCamera();
    asynStatus connectCamera();
    
    /* These items are specific to the Prosilica driver */
    tPvHandle PvHandle;                /* GenericPointer for the Prosilica PvAPI library */
    unsigned long uniqueId;
    tPvCameraInfo PvCameraInfo;
    tPvFrame PvFrames[MAX_FRAMES];
    size_t maxFrameSize;
    int framesRemaining;
    char sensorType[20];
    char IPAddress[50];
    tPvUint32 sensorBits;
    tPvUint32 sensorWidth;
    tPvUint32 sensorHeight;
    tPvUint32 timeStampFrequency;
};

/* If we have any private driver commands they begin with ADFirstDriverCommand and should end
   with ADLastDriverCommand, which is used for setting the size of the parameter library table */
typedef enum {
    /* These parameters describe the trigger modes of the Prosilica
     * They must agree with the values in the mbbo/mbbi records in
     * the Prosilca database. */
    PSTriggerStartFreeRun,
    PSTriggerStartSyncIn1,
    PSTriggerStartSyncIn2,
    PSTriggerStartSyncIn3,
    PSTriggerStartSyncIn4,
    PSTriggerStartFixedRate,
    PSTriggerStartSoftware
} PSTriggerStartMode_t;

static const char *PSTriggerStartModes[] = {
    "Freerun",
    "SyncIn1",
    "SyncIn2",
    "SyncIn3",
    "SyncIn4",
    "FixedRate",
    "Software"
};
#define NUM_START_TRIGGER_MODES (int)(sizeof(PSTriggerStartModes) / sizeof(PSTriggerStartModes[0]))

static const char *PSSyncOutModes[] = {
    "GPO",
    "AcquisitionTriggerReady",
    "FrameTriggerReady",
    "FrameTrigger",
    "Exposing",
    "FrameReadout",
    "Imaging",
    "Acquiring",
    "SyncIn1",
    "SyncIn2",
    "SyncIn3",
    "SyncIn4",
    "Strobe1",
    "Strobe2",
    "Strobe3",
    "Strobe4"
};
#define NUM_SYNC_OUT_MODES (int)(sizeof(PSSyncOutModes) / sizeof(PSSyncOutModes[0]))

static const char *PSStrobeModes[] = {
    "AcquisitionTriggerReady",
    "FrameTriggerReady",
    "FrameTrigger",
    "Exposing",
    "FrameReadout",
    "Acquiring",
    "SyncIn1",
    "SyncIn2",
    "SyncIn3",
    "SyncIn4",
};
#define NUM_STROBE_MODES (int)(sizeof(PSStrobeModes) / sizeof(PSStrobeModes[0]))
     
 

typedef enum {
    /* These parameters are specific to the Prosilica camera */
    /*    Name               asyn interface  access   Description  */
    PSReadStatistics        /* (asynInt32,    r/w) Write to read statistics  */ 
        = ADFirstDriverParam,
    PSDriverType,           /* (asynOctet,    r/o) Ethernet driver type */ 
    PSFilterVersion,        /* (asynOctet,    r/o) Ethernet packet filter version */ 
    PSFrameRate,            /* (asynFloat64,  r/o) Frame rate */ 
    PSFramesCompleted,      /* (asynInt32,    r/o) Frames completed */ 
    PSFramesDropped,        /* (asynInt32,    r/o) Frames dropped */ 
    PSPacketsErroneous,     /* (asynInt32,    r/o) Erroneous packets */ 
    PSPacketsMissed,        /* (asynInt32,    r/o) Missed packets */ 
    PSPacketsReceived,      /* (asynInt32,    r/o) Packets received */ 
    PSPacketsRequested,     /* (asynInt32,    r/o) Packets requested */ 
    PSPacketsResent,        /* (asynInt32,    r/o) Packets resent */
    PSBadFrameCounter,      /* (asynInt32,    r/o) Bad frame counter */
    PSSyncIn1Level,         /* (asynInt32,    r/o) Sync input 1 level */
    PSSyncIn2Level,         /* (asynInt32,    r/o) Sync input 2 level */
    PSSyncOut1Mode,         /* (asynInt32,    r/w) Sync output 1 mode */
    PSSyncOut1Level,        /* (asynInt32,    r/w) Sync output 1 level */
    PSSyncOut1Invert,       /* (asynInt32,    r/w) Sync output 1 invert */
    PSSyncOut2Mode,         /* (asynInt32,    r/w) Sync output 2 mode */
    PSSyncOut2Level,        /* (asynInt32,    r/w) Sync output 2 level */
    PSSyncOut2Invert,       /* (asynInt32,    r/w) Sync output 2 invert */
    PSSyncOut3Mode,         /* (asynInt32,    r/w) Sync output 3 mode */
    PSSyncOut3Level,        /* (asynInt32,    r/w) Sync output 3 level */
    PSSyncOut3Invert,       /* (asynInt32,    r/w) Sync output 3 invert */
    PSStrobe1Mode,          /* (asynInt32,    r/w) Strobe 1 mode */
    PSStrobe1Delay,         /* (asynFloat64,  r/w) Strobe 1 delay */
    PSStrobe1CtlDuration,   /* (asynInt32,    r/w) Strobe 1 controlled duration */
    PSStrobe1Duration,      /* (asynFloat64,  r/w) Strobe 1 duration */
    ADLastDriverParam
} PSDetParam_t;

static asynParamString_t PSDetParamString[] = {
    {PSReadStatistics,        "PS_READ_STATISTICS"},
    {PSDriverType,            "PS_DRIVER_TYPE"},
    {PSFilterVersion,         "PS_FILTER_VERSION"},
    {PSFrameRate,             "PS_FRAME_RATE"},
    {PSFramesCompleted,       "PS_FRAMES_COMPLETED"},
    {PSFramesDropped,         "PS_FRAMES_DROPPED"},
    {PSPacketsErroneous,      "PS_PACKETS_ERRONEOUS"},
    {PSPacketsMissed,         "PS_PACKETS_MISSED"},
    {PSPacketsReceived,       "PS_PACKETS_RECEIVED"},
    {PSPacketsRequested,      "PS_PACKETS_REQUESTED"},
    {PSPacketsResent,         "PS_PACKETS_RESENT"},
    {PSBadFrameCounter,       "PS_BAD_FRAME_COUNTER"},
    {PSSyncIn1Level,          "PS_SYNC_IN_1_LEVEL"},
    {PSSyncIn2Level,          "PS_SYNC_IN_2_LEVEL"},
    {PSSyncOut1Mode,          "PS_SYNC_OUT_1_MODE"},
    {PSSyncOut1Level,         "PS_SYNC_OUT_1_LEVEL"},
    {PSSyncOut1Invert,        "PS_SYNC_OUT_1_INVERT"},
    {PSSyncOut2Mode,          "PS_SYNC_OUT_2_MODE"},
    {PSSyncOut2Level,         "PS_SYNC_OUT_2_LEVEL"},
    {PSSyncOut2Invert,        "PS_SYNC_OUT_2_INVERT"},
    {PSSyncOut3Mode,          "PS_SYNC_OUT_3_MODE"},
    {PSSyncOut3Level,         "PS_SYNC_OUT_3_LEVEL"},
    {PSSyncOut3Invert,        "PS_SYNC_OUT_3_INVERT"},
    {PSStrobe1Mode,           "PS_STROBE_1_MODE"},
    {PSStrobe1Delay,          "PS_STROBE_1_DELAY"},
    {PSStrobe1CtlDuration,    "PS_STROBE_1_CTL_DURATION"},
    {PSStrobe1Duration,       "PS_STROBE_1_DURATION"},

};

#define NUM_PS_DET_PARAMS (sizeof(PSDetParamString)/sizeof(PSDetParamString[0]))

asynStatus prosilica::writeFile()
{
    /* Writes last image to disk as a TIFF file. */
    int status = asynSuccess, tiffStatus;
    char fullFileName[MAX_FILENAME_LEN];
    int fileFormat;
    NDArray *pImage = this->pArrays[0];
    tPvFrame PvFrame, *pFrame=&PvFrame;
    NDArrayInfo_t arrayInfo;
    static const char *functionName = "writeFile";

    if (!pImage) return asynError;
    
    /* Set all fields in frame to 0 */
    memset(pFrame, 0, sizeof(tPvFrame));

    status |= createFileName(MAX_FILENAME_LEN, fullFileName);
    if (status) { 
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
              "%s:%s: error creating full file name, fullFileName=%s, status=%d\n", 
              driverName, functionName, fullFileName, status);
        return((asynStatus)status);
    }
    
    /* Copy the data from our last image buffer to a frame buffer structure, which is
     * required by ImageWriteTiff */
    pFrame->Width = pImage->dims[0].size;
    pFrame->Height = pImage->dims[1].size;
    pFrame->ImageBuffer = pImage->pData;
    pImage->getInfo(&arrayInfo);
    pFrame->ImageBufferSize = arrayInfo.totalBytes;
    pFrame->ImageSize = pFrame->ImageBufferSize;
    
    /* Note, this needs work because we need to support color models */
    switch(pImage->dataType) {
        case NDInt8:
        case NDUInt8:
            pFrame->Format = ePvFmtMono8;
            pFrame->BitDepth = 8;
            break;
        case NDInt16:
        case NDUInt16:
            pFrame->Format = ePvFmtMono16;
            pFrame->BitDepth = 16;
            break;
        default:
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "%s:%s: error unsupported data type=%d\n",
                driverName, functionName, pImage->dataType);
            break;
    }
    
    status |= getIntegerParam(ADFileFormat, &fileFormat);
    /* We only support writing in TIFF format for now */
    tiffStatus = ImageWriteTiff(fullFileName, pFrame);
    if (tiffStatus != 1) {
        status |= asynError;
    } else {
        status |= setStringParam(ADFullFileName, fullFileName);
    }
    return((asynStatus)status);
}

static void PVDECL frameCallbackC(tPvFrame *pFrame)
{
    prosilica *pPvt = (prosilica *) pFrame->Context[0];
    
    pPvt->frameCallback(pFrame);
}

void prosilica::frameCallback(tPvFrame *pFrame)
{
    int status = asynSuccess;
    int autoSave;
    int ndims, dims[2];
    int imageCounter;
    NDArray *pImage;
    int binX, binY;
    int badFrameCounter;
    static const char *functionName = "frameCallback";

    /* If this callback is coming from a shutdown operation rather than normal collection, 
     * we will not be able to take the mutex and things will hang.  Prevent this by looking at the frame
     * status and returning immediately if it is and in that case the mutex has already been taken.  Just return in
     * that case */
    if (pFrame->Status == ePvErrCancelled) return;

    epicsMutexLock(this->mutexId);

    pImage = (NDArray *)pFrame->Context[1];

    if (pFrame->Status == ePvErrSuccess) {
        /* The frame we just received has NDArray* in Context[1] */ 
        /* We save the most recent good image buffer so it can be used in the PSWriteFile
         * and readADImage functions.  Now release it. */
        if (this->pArrays[0]) this->pArrays[0]->release();
        this->pArrays[0] = pImage;
        /* Set the properties of the image to those of the current frame */
        /* Convert from the PvApi data types to ADDataType */
        /* The pFrame structure does not contain the binning, get that from param lib,
         * but it could be wrong for this frame if recently changed */
        getIntegerParam(ADBinX, &binX);
        getIntegerParam(ADBinY, &binY);
        /* The mono cameras can return a Bayer pattern which is invalid, and this can
         * crash the file plugin.  Fix it here. */
        if (pFrame->BayerPattern > ePvBayerBGGR) pFrame->BayerPattern = ePvBayerRGGB;
        pImage->bayerPattern = (NDBayerPattern_t)pFrame->BayerPattern;
        switch(pFrame->Format) {
            case ePvFmtMono8:
                pImage->colorMode = NDColorModeMono;
                pImage->dataType = NDUInt8;
                pImage->ndims = 2;
                pImage->dims[0].size    = pFrame->Width;
                pImage->dims[0].offset  = pFrame->RegionX;
                pImage->dims[0].binning = binX;
                pImage->dims[1].size    = pFrame->Height;
                pImage->dims[1].offset  = pFrame->RegionY;
                pImage->dims[1].binning = binY;
                break;
            case ePvFmtMono16:
                pImage->colorMode = NDColorModeMono;
                pImage->dataType = NDUInt16;
                pImage->ndims = 2;
                pImage->dims[0].size    = pFrame->Width;
                pImage->dims[0].offset  = pFrame->RegionX;
                pImage->dims[0].binning = binX;
                pImage->dims[1].size    = pFrame->Height;
                pImage->dims[1].offset  = pFrame->RegionY;
                pImage->dims[1].binning = binY;
                break;
            case ePvFmtBayer8:
                pImage->colorMode = NDColorModeBayer;
                pImage->dataType = NDUInt8;
                pImage->ndims = 2;
                pImage->dims[0].size   = pFrame->Width;
                pImage->dims[0].offset = pFrame->RegionX;
                pImage->dims[0].binning = binX;
                pImage->dims[1].size   = pFrame->Height;
                pImage->dims[1].offset = pFrame->RegionY;
                pImage->dims[1].binning = binY;
                break;
            case ePvFmtBayer16:
                pImage->colorMode = NDColorModeBayer;
                pImage->dataType = NDUInt16;
                pImage->ndims = 2;
                pImage->dims[0].size    = pFrame->Width;
                pImage->dims[0].offset  = pFrame->RegionX;
                pImage->dims[0].binning = binX;
                pImage->dims[1].size    = pFrame->Height;
                pImage->dims[1].offset  = pFrame->RegionY;
                pImage->dims[1].binning = binY;
                break;
            case ePvFmtRgb24:
                pImage->colorMode = NDColorModeRGB1;
                pImage->dataType = NDUInt8;
                pImage->ndims = 3;
                pImage->dims[0].size    = 3;
                pImage->dims[0].offset  = 0;
                pImage->dims[0].binning = 1;
                pImage->dims[1].size    = pFrame->Width;
                pImage->dims[1].offset  = pFrame->RegionX;
                pImage->dims[1].binning = binX;
                pImage->dims[2].size    = pFrame->Height;
                pImage->dims[2].offset  = pFrame->RegionY;
                pImage->dims[2].binning = binY;
                break;
            case ePvFmtRgb48:
                pImage->colorMode = NDColorModeRGB1;
                pImage->dataType = NDUInt16;
                pImage->ndims = 3;
                pImage->dims[0].size    = 3;
                pImage->dims[0].offset  = 0;
                pImage->dims[0].binning = 1;
                pImage->dims[1].size    = pFrame->Width;
                pImage->dims[1].offset  = pFrame->RegionX;
                pImage->dims[1].binning = binX;
                pImage->dims[2].size    = pFrame->Height;
                pImage->dims[2].offset  = pFrame->RegionY;
                pImage->dims[2].binning = binY;
                break;
            default:
                 /* We don't support other formats yet */
                asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
                    "%s:%s: error unsupported pixel format %d\n", 
                    driverName, functionName, pFrame->Format);
                break;
        }
        
        /* Set the uniqueId and time stamp */
        pImage->uniqueId = pFrame->FrameCount;
        if (this->timeStampFrequency == 0) this->timeStampFrequency = 1;
        pImage->timeStamp = ((double)pFrame->TimestampLo + 
                             (double)pFrame->TimestampHi*4294967296.)/this->timeStampFrequency;
        
        /* Call the NDArray callback */
        /* Must release the lock here, or we can get into a deadlock, because we can
         * block on the plugin lock, and the plugin can be calling us */
        epicsMutexUnlock(this->mutexId);
        doCallbacksGenericPointer(pImage, NDArrayData, 0);
        epicsMutexLock(this->mutexId);

        /* See if acquisition is done */
        if (this->framesRemaining > 0) this->framesRemaining--;
        if (this->framesRemaining == 0) {
            setIntegerParam(ADAcquire, 0);
            setIntegerParam(ADStatus, ADStatusIdle);
        }

        /* Update the frame counter */
        getIntegerParam(ADImageCounter, &imageCounter);
        imageCounter++;
        setIntegerParam(ADImageCounter, imageCounter);

        /* If autoSave is set then save the image */
        status = getIntegerParam(ADAutoSave, &autoSave);
        if (autoSave) status = writeFile();

        asynPrintIO(this->pasynUserSelf, ASYN_TRACEIO_DRIVER, 
            (const char *)this->pArrays[0]->pData, this->pArrays[0]->dataSize,
            "%s:%s: frameId=%d, timeStamp=%f\n",
            driverName, functionName, pImage->uniqueId, pImage->timeStamp);

        /* Allocate a new image buffer, make the size be the maximum that the frames can be */
        ndims = 2;
        dims[0] = this->sensorWidth;
        dims[1] = this->sensorHeight;
        pImage = this->pNDArrayPool->alloc(ndims, dims, NDInt8, this->maxFrameSize, NULL);
        /* Put the pointer to this image buffer in the frame context[1] */
        pFrame->Context[1] = pImage;
        /* Reset the frame buffer data pointer be this image buffer data pointer */
        pFrame->ImageBuffer = pImage->pData;
    } else {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: ERROR, frame has error code %d\n",
            driverName, functionName, pFrame->Status);
        getIntegerParam(PSBadFrameCounter, &badFrameCounter);
        badFrameCounter++;
        setIntegerParam(PSBadFrameCounter, badFrameCounter);
    }

    /* Update any changed parameters */
    callParamCallbacks();
    
    /* Queue this frame to run again */
    status = PvCaptureQueueFrame(this->PvHandle, pFrame, frameCallbackC); 
    epicsMutexUnlock(this->mutexId);
}

asynStatus prosilica::setPixelFormat()
{
    int status = asynSuccess;
    int colorMode, dataType;
    static const char *functionName = "setPixelFormat";
    char pixelFormat[20];

    status |= getIntegerParam(ADColorMode, &colorMode);
    status |= getIntegerParam(ADDataType, &dataType);
    if      ((colorMode == NDColorModeMono)  && (dataType == NDUInt8))  strcpy(pixelFormat, "Mono8");
    else if ((colorMode == NDColorModeMono)  && (dataType == NDUInt16)) strcpy(pixelFormat, "Mono16");
    else if ((colorMode == NDColorModeRGB1)  && (dataType == NDUInt8))  strcpy(pixelFormat, "Rgb24");
    else if ((colorMode == NDColorModeRGB1)  && (dataType == NDUInt16)) strcpy(pixelFormat, "Rgb48");
    else if ((colorMode == NDColorModeBayer) && (dataType == NDUInt8))  strcpy(pixelFormat, "Bayer8");
    else if ((colorMode == NDColorModeBayer) && (dataType == NDUInt16)) strcpy(pixelFormat, "Bayer16");
    else {
         /* We don't support other formats yet */
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
            "%s:%s: error unsupported data type %d and/or color mode %d\n", 
            driverName, functionName, dataType, colorMode);
        return(asynError);
    }
    status |= PvAttrEnumSet(this->PvHandle, "PixelFormat", pixelFormat);
    return((asynStatus)status);
}      

asynStatus prosilica::setGeometry()
{
    int status = asynSuccess;
    int binX, binY, minY, minX, sizeX, sizeY;
    static const char *functionName = "setGeometry";
    
    /* Get all of the current geometry parameters from the parameter library */
    status |= getIntegerParam(ADBinX, &binX);
    if (binX < 1) binX = 1;
    status |= getIntegerParam(ADBinY, &binY);
    if (binY < 1) binY = 1;
    status |= getIntegerParam(ADMinX, &minX);
    status |= getIntegerParam(ADMinY, &minY);
    status |= getIntegerParam(ADSizeX, &sizeX);
    status |= getIntegerParam(ADSizeY, &sizeY);
    
    status |= PvAttrUint32Set(this->PvHandle, "BinningX", binX);
    status |= PvAttrUint32Set(this->PvHandle, "BinningY", binY);
    status |= PvAttrUint32Set(this->PvHandle, "RegionX", minX/binX);
    status |= PvAttrUint32Set(this->PvHandle, "RegionY", minY/binY);
    status |= PvAttrUint32Set(this->PvHandle, "Width",   sizeX/binX);
    status |= PvAttrUint32Set(this->PvHandle, "Height",  sizeY/binY);
    
    if (status) asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
                      "%s:%s: error, status=%d\n", 
                      driverName, functionName, status);
    return((asynStatus)status);
}

asynStatus prosilica::getGeometry()
{
    int status = asynSuccess;
    tPvUint32 binX, binY, minY, minX, sizeX, sizeY;
    static const char *functionName = "setGeometry";

    status |= PvAttrUint32Get(this->PvHandle, "BinningX", &binX);
    status |= PvAttrUint32Get(this->PvHandle, "BinningY", &binY);
    status |= PvAttrUint32Get(this->PvHandle, "RegionX",  &minX);
    status |= PvAttrUint32Get(this->PvHandle, "RegionY",  &minY);
    status |= PvAttrUint32Get(this->PvHandle, "Width",    &sizeX);
    status |= PvAttrUint32Get(this->PvHandle, "Height",   &sizeY);
    
    status |= setIntegerParam(ADBinX,  binX);
    status |= setIntegerParam(ADBinY,  binY);
    status |= setIntegerParam(ADMinX,  minX*binX);
    status |= setIntegerParam(ADMinY,  minY*binY);
    status |= setIntegerParam(ADSizeX, sizeX*binX);
    status |= setIntegerParam(ADSizeY, sizeY*binY);

    status |= setIntegerParam(ADBinX,  binX);
    status |= setIntegerParam(ADBinY,  binY);
    status |= setIntegerParam(ADMinX,  minX*binX);
    status |= setIntegerParam(ADMinY,  minY*binY);
    status |= setIntegerParam(ADSizeX, sizeX*binX);
    status |= setIntegerParam(ADSizeY, sizeY*binY);
    status |= setIntegerParam(ADImageSizeX, sizeX);
    status |= setIntegerParam(ADImageSizeY, sizeY);
    
    if (status) asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
                      "%s:%s: error, status=%d\n", 
                      driverName, functionName, status);
    return((asynStatus)status);
}

asynStatus prosilica::readStats()
{
    int status = asynSuccess;
    char buffer[50];
    unsigned long nchars;
    tPvUint32 uval;
    int i;
    float fval;
    static const char *functionName = "readStats";
    
    status |= PvAttrEnumGet      (this->PvHandle, "StatDriverType", buffer, sizeof(buffer), &nchars);
    if (status == ePvErrNotFound) {
        status = 0;
        strcpy(buffer, "Unsupported parameter");
    }
    status |= setStringParam ( PSDriverType, buffer);    
    status |= PvAttrStringGet    (this->PvHandle, "StatFilterVersion", buffer, sizeof(buffer), &nchars);
    if (status == ePvErrNotFound) {
        status = 0;
        strcpy(buffer, "Unsupported parameter");
    }
    status |= setStringParam ( PSFilterVersion, buffer);
    status |= PvAttrFloat32Get   (this->PvHandle, "StatFrameRate", &fval);
    status |= setDoubleParam ( PSFrameRate, fval);
    status |= PvAttrUint32Get    (this->PvHandle, "StatFramesCompleted", &uval);
    status |= setIntegerParam( PSFramesCompleted, (int)uval);
    status |= PvAttrUint32Get    (this->PvHandle, "StatFramesDropped", &uval);
    status |= setIntegerParam( PSFramesDropped, (int)uval);
    status |= PvAttrUint32Get    (this->PvHandle, "StatPacketsErroneous", &uval);
    status |= setIntegerParam( PSPacketsErroneous, (int)uval);
    status |= PvAttrUint32Get    (this->PvHandle, "StatPacketsMissed", &uval);
    status |= setIntegerParam( PSPacketsMissed, (int)uval);
    status |= PvAttrUint32Get    (this->PvHandle, "StatPacketsReceived", &uval);
    status |= setIntegerParam( PSPacketsReceived, (int)uval);
    status |= PvAttrUint32Get    (this->PvHandle, "StatPacketsRequested", &uval);
    status |= setIntegerParam( PSPacketsRequested, (int)uval);
    status |= PvAttrUint32Get    (this->PvHandle, "StatPacketsResent", &uval);
    status |= setIntegerParam( PSPacketsResent, (int)uval);
    status |= PvAttrUint32Get    (this->PvHandle, "SyncInLevels", &uval);
    status |= setIntegerParam( PSSyncIn1Level, uval&0x01 ? 1:0);
    status |= setIntegerParam( PSSyncIn2Level, uval&0x02 ? 1:0);
    status |= PvAttrUint32Get    (this->PvHandle, "SyncOutGpoLevels", &uval);
    status |= setIntegerParam( PSSyncOut1Level, uval&0x01 ? 1:0);
    status |= setIntegerParam( PSSyncOut2Level, uval&0x02 ? 1:0);
    status |= setIntegerParam( PSSyncOut3Level, uval&0x04 ? 1:0);
    status |= PvAttrEnumGet(this->PvHandle, "SyncOut1Mode", buffer, sizeof(buffer), &nchars);
    for (i=0; i<NUM_SYNC_OUT_MODES; i++) {
        if (strcmp(buffer, PSSyncOutModes[i]) == 0) {
            status |= setIntegerParam(PSSyncOut1Mode, i);
            break;
        }
    }
    if (i == NUM_SYNC_OUT_MODES) {
        status |= setIntegerParam(PSSyncOut1Mode, 0);
        status |= asynError;
    }
    status |= PvAttrEnumGet(this->PvHandle, "SyncOut2Mode", buffer, sizeof(buffer), &nchars);
    for (i=0; i<NUM_SYNC_OUT_MODES; i++) {
        if (strcmp(buffer, PSSyncOutModes[i]) == 0) {
            status |= setIntegerParam(PSSyncOut2Mode, i);
            break;
        }
    }
    if (i == NUM_SYNC_OUT_MODES) {
        status |= setIntegerParam(PSSyncOut2Mode, 0);
        status |= asynError;
    }
    status |= PvAttrEnumGet(this->PvHandle, "SyncOut3Mode", buffer, sizeof(buffer), &nchars);
    /* This parameter can be not supported */
    if (status == ePvErrNotFound) {
        status = 0;
        status |= setIntegerParam(PSSyncOut3Mode, 0);
    } else {
        for (i=0; i<NUM_SYNC_OUT_MODES; i++) {
            if (strcmp(buffer, PSSyncOutModes[i]) == 0) {
                status |= setIntegerParam(PSSyncOut3Mode, i);
                break;
            }
        }
    }
    if (i == NUM_SYNC_OUT_MODES) {
        status |= setIntegerParam(PSSyncOut3Mode, 0);
        status |= asynError;
    }
    
    status |= PvAttrEnumGet(this->PvHandle, "SyncOut1Invert", buffer, sizeof(buffer), &nchars);
    if (strcmp(buffer, "Off") == 0) i = 0;
    else if (strcmp(buffer, "On") == 0) i = 1;
    else {
        i=0;
        status |= asynError;
    }
    status |= setIntegerParam( PSSyncOut1Invert, i);
    status |= PvAttrEnumGet(this->PvHandle, "SyncOut2Invert", buffer, sizeof(buffer), &nchars);
    if (strcmp(buffer, "Off") == 0) i = 0;
    else if (strcmp(buffer, "On") == 0) i = 1;
    else {
        i=0;
        status |= asynError;
    }
    status |= setIntegerParam( PSSyncOut2Invert, i);
    status |= PvAttrEnumGet(this->PvHandle, "SyncOut3Invert", buffer, sizeof(buffer), &nchars);
    if (status == ePvErrNotFound) {
        status = 0;
        i=0;
    } else {
        if (strcmp(buffer, "Off") == 0) i = 0;
        else if (strcmp(buffer, "On") == 0) i = 1;
        else {
            i=0;
            status |= asynError;
        }
    }
    status |= setIntegerParam( PSSyncOut3Invert, i);

    status |= PvAttrEnumGet(this->PvHandle, "Strobe1Mode", buffer, sizeof(buffer), &nchars);
    for (i=0; i<NUM_STROBE_MODES; i++) {
        if (strcmp(buffer, PSStrobeModes[i]) == 0) {
            status |= setIntegerParam(PSStrobe1Mode, i);
            break;
        }
    }
    if (i == NUM_STROBE_MODES) {
        status |= setIntegerParam(PSStrobe1Mode, 0);
        status |= asynError;
    }
    status |= PvAttrEnumGet(this->PvHandle, "Strobe1ControlledDuration", buffer, sizeof(buffer), &nchars);
    if (strcmp(buffer, "Off") == 0) i = 0;
    else if (strcmp(buffer, "On") == 0) i = 1;
    else {
        i=0;
        status |= asynError;
    }
    status |= setIntegerParam( PSStrobe1CtlDuration, i);

    status |= PvAttrUint32Get    (this->PvHandle, "Strobe1Delay", &uval);
    status |= setDoubleParam( PSStrobe1Delay, uval/1.e6);
    status |= PvAttrUint32Get    (this->PvHandle, "Strobe1Duration", &uval);
    status |= setDoubleParam( PSStrobe1Duration, uval/1.e6);

    if (status) asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
                      "%s:%s: error, status=%d\n", 
                      driverName, functionName, status);
    return(asynSuccess);
}

asynStatus prosilica::readParameters()
{
    int status = asynSuccess;
    tPvUint32 intVal;
    int i;
    int dataType=NDUInt8, colorMode=NDColorModeMono;
    tPvFloat32 fltVal;
    double dval;
    unsigned long nchars;
    char buffer[20];
    static const char *functionName = "setGeometry";

    status |= PvAttrUint32Get(this->PvHandle, "TotalBytesPerFrame", &intVal);
    setIntegerParam(ADImageSize, intVal);

    status |= PvAttrEnumGet(this->PvHandle, "PixelFormat", buffer, sizeof(buffer), &nchars);
    if      (!strcmp(buffer, "Mono8")) {
        dataType = NDUInt8;
        colorMode = NDColorModeMono;
    }
    else if (!strcmp(buffer, "Mono16")) {
        dataType = NDUInt16;
        colorMode = NDColorModeMono;
    }
    else if (!strcmp(buffer, "Rgb24")) {
        dataType = NDUInt8;
        colorMode = NDColorModeRGB1;
    }
    else if (!strcmp(buffer, "Rgb48")) {
        dataType = NDUInt16;
        colorMode = NDColorModeRGB1;
    }
    else if (!strcmp(buffer, "Bayer8")) {
        dataType = NDUInt8;
        colorMode = NDColorModeBayer;
    }
    else if (!strcmp(buffer, "Bayer16")) { 
        dataType = NDUInt16;
        colorMode = NDColorModeBayer;
    }
    else {
         /* We don't support other formats yet */
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
            "%s:%s: error unsupported data type %d and/or color mode %d\n", 
            driverName, functionName, dataType, colorMode);
    }
    status |= setIntegerParam(ADDataType, dataType);
    status |= setIntegerParam(ADColorMode, colorMode);
    
    status |= getGeometry();

    status |= PvAttrUint32Get(this->PvHandle, "AcquisitionFrameCount", &intVal);
    status |= setIntegerParam(ADNumImages, intVal);

    status |= PvAttrEnumGet(this->PvHandle, "AcquisitionMode", buffer, sizeof(buffer), &nchars);
    if      (!strcmp(buffer, "SingleFrame")) i = ADImageSingle;
    else if (!strcmp(buffer, "MultiFrame"))  i = ADImageMultiple;
    else if (!strcmp(buffer, "Recorder"))    i = ADImageMultiple;
    else if (!strcmp(buffer, "Continuous"))  i = ADImageContinuous;
    else {i=0; status |= asynError;}
    status |= setIntegerParam(ADImageMode, i);

    status |= PvAttrEnumGet(this->PvHandle, "FrameStartTriggerMode", buffer, sizeof(buffer), &nchars);
    for (i=0; i<NUM_START_TRIGGER_MODES; i++) {
        if (strcmp(buffer, PSTriggerStartModes[i]) == 0) {
            status |= setIntegerParam(ADTriggerMode, i);
            break;
        }
    }
    if (i == NUM_START_TRIGGER_MODES) {
        status |= setIntegerParam(ADTriggerMode, 0);
        status |= asynError;
    }
    
    /* Prosilica does not support more than 1 exposure per frame */
    status |= setIntegerParam(ADNumExposures, 1);

    /* Prosilica uses integer microseconds */
    status |= PvAttrUint32Get(this->PvHandle, "ExposureValue", &intVal);
    dval = intVal / 1.e6;
    status |= setDoubleParam(ADAcquireTime, dval);

    /* Prosilica uses a frame rate in Hz */
    status |= PvAttrFloat32Get(this->PvHandle, "FrameRate", &fltVal);
    if (fltVal == 0.) fltVal = 1;
    dval = 1. / fltVal;
    status |= setDoubleParam(ADAcquirePeriod, dval);

    /* Prosilica uses an integer value */
    status |= PvAttrUint32Get(this->PvHandle, "GainValue", &intVal);
    dval = intVal;
    status |= setDoubleParam(ADGain, dval);

    /* Call the callbacks to update the values in higher layers */
    callParamCallbacks();
    
    if (status) asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
                      "%s:%s: error, status=%d\n", 
                      driverName, functionName, status);
    return((asynStatus)status);
}

asynStatus prosilica::disconnectCamera()
{
    int status = asynSuccess;
    tPvFrame *pFrame;
    NDArray *pImage;
    static const char *functionName = "disconnectCamera";

    if (!this->PvHandle) return(asynSuccess);
    status |= PvCaptureQueueClear(this->PvHandle);
    status |= PvCaptureEnd(this->PvHandle);
    status |= PvCameraClose(this->PvHandle);
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
          "%s:%s: disconnecting camera %d\n", 
          driverName, functionName, this->uniqueId);
    if (status) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
              "%s:%s: unable to close camera %d\n",
              driverName, functionName, this->uniqueId);
    }
    /* If we have allocated frame buffers, free them. */
    /* Must first free any image buffers they point to */
    pFrame = this->PvFrames;
    while (pFrame) {
        pImage = (NDArray *)pFrame->Context[1];
        if (pImage) pImage->release();
        pFrame->Context[1] = 0;
        pFrame++;
    }
    this->PvHandle = NULL;
    return((asynStatus)status);
}

asynStatus prosilica::connectCamera()
{
    int status = asynSuccess;
    unsigned long nchars;
    tPvFrame *pFrame;
    int i;
    int ndims, dims[2];
    int bytesPerPixel;
    NDArray *pImage;
    static const char *functionName = "connectCamera";

    /* First disconnect from the camera */
    disconnectCamera();
    
    status = ::PvCameraInfo(this->uniqueId, &this->PvCameraInfo);
    if (status) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
              "%s:%s: Cannot find camera %d\n", 
              driverName, functionName, this->uniqueId);
        return asynError;
    }

    if ((this->PvCameraInfo.PermittedAccess & ePvAccessMaster) == 0) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
              "%s:%s: Cannot get control of camera %d\n", 
               driverName, functionName, this->uniqueId);
        return asynError;
    }

    status = PvCameraOpen(this->uniqueId, ePvAccessMaster, &this->PvHandle);
    if (status) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
              "%s:%s: unable to open camera %d\n",
              driverName, functionName, this->uniqueId);
       return asynError;
    }
    
    /* Negotiate maximum frame size */
    status = PvCaptureAdjustPacketSize(this->PvHandle, MAX_PACKET_SIZE);
    if (status) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
              "%s:%s: unable to adjust packet size %d\n",
              driverName, functionName, this->uniqueId);
       return asynError;
    }
    
    /* Initialize the frame buffers and queue them */
    status = PvCaptureStart(this->PvHandle);
    if (status) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
              "%s:%s: unable to start capture on camera %d\n",
              driverName, functionName, this->uniqueId);
        return asynError;
    }

    /* We allocate image buffers that are large enough for the biggest possible image.
       This is simpler than reallocating when readout parameters change.  It is also safer,
       since changing readout parameters happens instantly, but there will still be frames
       queued with the wrong size */
    /* Query the parameters of the image sensor */
    status = PvAttrEnumGet(this->PvHandle, "SensorType", this->sensorType, 
                             sizeof(this->sensorType), &nchars);
    status |= PvAttrUint32Get(this->PvHandle, "SensorBits", &this->sensorBits);
    status |= PvAttrUint32Get(this->PvHandle, "SensorWidth", &this->sensorWidth);
    status |= PvAttrUint32Get(this->PvHandle, "SensorHeight", &this->sensorHeight);
    status |= PvAttrUint32Get(this->PvHandle, "TimeStampFrequency", &this->timeStampFrequency);
    status |= PvAttrStringGet(this->PvHandle, "DeviceIPAddress", this->IPAddress, 
                              sizeof(this->IPAddress), &nchars);
    if (status) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
              "%s:%s: unable to get sensor data on camera %d\n",
              driverName, functionName, this->uniqueId);
        return asynError;
    }
    
    bytesPerPixel = (this->sensorBits-1)/8 + 1;
    /* If the camera supports color then there can be 3 values per pixel */
    if (strcmp(this->sensorType, "Mono") != 0) bytesPerPixel *= 3;
    this->maxFrameSize = this->sensorWidth * this->sensorHeight * bytesPerPixel;    
    for (i=0; i<MAX_FRAMES; i++) {
        pFrame = &this->PvFrames[i];
        ndims = 2;
        dims[0] = this->sensorWidth;
        dims[1] = this->sensorHeight;
       /* Allocate a new image buffer, make the size be the maximum that the frames can be */
        pImage = this->pNDArrayPool->alloc(ndims, dims, NDInt8, this->maxFrameSize, NULL);
        if (!pImage) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
                  "%s:%s: unable to allocate image %d on camera %d\n",
                  driverName, functionName, i, this->uniqueId);
            return asynError;
        }
        /* Set the frame buffer data pointer be this image buffer data pointer */
        pFrame->ImageBuffer = pImage->pData;
        pFrame->ImageBufferSize = this->maxFrameSize;
        /* Put a pointer to ourselves in Context[0] */
        pFrame->Context[0] = (void *)this;
        /* Put the pointer to this image buffer in the frame context[1] */
        pFrame->Context[1] = pImage;
        status = PvCaptureQueueFrame(this->PvHandle, pFrame, frameCallbackC); 
        if (status) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
                  "%s:%s: unable to queue frame %d on camera %d\n",
                  driverName, functionName, i, this->uniqueId);
            return asynError;
        }
    }

    /* Set some initial values for other parameters */
    status =  setStringParam (ADManufacturer, "Prosilica");
    status |= setStringParam (ADModel, this->PvCameraInfo.DisplayName);
    status |= setIntegerParam(ADSizeX, this->sensorWidth);
    status |= setIntegerParam(ADSizeY, this->sensorHeight);
    status |= setIntegerParam(ADMaxSizeX, this->sensorWidth);
    status |= setIntegerParam(ADMaxSizeY, this->sensorHeight);
    status |= setIntegerParam(PSBadFrameCounter, 0);
    if (status) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
              "%s:%s: unable to set camera parameters on camera %d\n",
              driverName, functionName, this->uniqueId);
        return asynError;
    }
    
     /* Read the current camera settings */
    status = readParameters();
    if (status) return((asynStatus)status);

    /* Read the current camera statistics */
    status = readStats();
    if (status) return((asynStatus)status);
        
    /* We found the camera and everything is OK.  Signal to asynManager that we are connected. */
    pasynManager->exceptionConnect(this->pasynUserSelf);
    return((asynStatus)status);
}


asynStatus prosilica::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function = pasynUser->reason;
    int status = asynSuccess;
    tPvUint32 syncs;
    static const char *functionName = "writeInt32";

    /* Set the parameter and readback in the parameter library.  This may be overwritten when we read back the
     * status at the end, but that's OK */
    status |= setIntegerParam(function, value);

    switch (function) {
        case ADBinX:
        case ADBinY:
        case ADMinX:
        case ADSizeX:
        case ADMinY:
        case ADSizeY:
            /* These commands change the chip readout geometry.  We need to cache them and apply them in the
             * correct order */
            status |= setGeometry();
            break;
        case ADNumImages:
            status |= PvAttrUint32Set(this->PvHandle, "AcquisitionFrameCount", value);
            break;
        case ADImageMode:
            switch(value) {
            case ADImageSingle:
                status |= PvAttrEnumSet(this->PvHandle, "AcquisitionMode", "SingleFrame");
                break;
            case ADImageMultiple:
                status |= PvAttrEnumSet(this->PvHandle, "AcquisitionMode", "MultiFrame");
                break;
            case ADImageContinuous:
                status |= PvAttrEnumSet(this->PvHandle, "AcquisitionMode", "Continuous");
                break;
            }
            break;
        case ADAcquire:
            if (value) {
                /* We need to set the number of images we expect to collect, so the frame callback function
                   can know when acquisition is complete.  We need to find out what mode we are in and how
                   many frames have been requested.  If we are in continuous mode then set the number of
                   remaining frames to -1. */
                int imageMode, numImages;
                status |= getIntegerParam(ADImageMode, &imageMode);
                status |= getIntegerParam(ADNumImages, &numImages);
                switch(imageMode) {
                case ADImageSingle:
                    this->framesRemaining = 1;
                    break;
                case ADImageMultiple:
                    this->framesRemaining = numImages;
                    break;
                case ADImageContinuous:
                    this->framesRemaining = -1;
                    break;
                }
                setIntegerParam(ADStatus, ADStatusAcquire);
                status |= PvCommandRun(this->PvHandle, "AcquisitionStart");
            } else {
                setIntegerParam(ADStatus, ADStatusIdle);
                status |= PvCommandRun(this->PvHandle, "AcquisitionAbort");
            }
            break;
        case ADTriggerMode:
            if ((value < 0) || (value > (NUM_START_TRIGGER_MODES-1))) {
                status = asynError;
                break;
            }
            status |= PvAttrEnumSet(this->PvHandle, "FrameStartTriggerMode", 
                                    PSTriggerStartModes[value]);
            break;
        case PSReadStatistics:
            readStats();
            break;
        case PSSyncOut1Mode:
            status |= PvAttrEnumSet(this->PvHandle, "SyncOut1Mode", PSSyncOutModes[value]);
            break;
        case PSSyncOut2Mode:
            status |= PvAttrEnumSet(this->PvHandle, "SyncOut2Mode", PSSyncOutModes[value]);
            break;
        case PSSyncOut3Mode:
            status |= PvAttrEnumSet(this->PvHandle, "SyncOut3Mode", PSSyncOutModes[value]);
            if (status == ePvErrNotFound) status = 0;
            break;
        case PSSyncOut1Level:
            status |= PvAttrUint32Get(this->PvHandle, "SyncOutGpoLevels", &syncs);
            syncs = (syncs & ~0x01) | ((value<<0) & 0x01);
            status |= PvAttrUint32Set(this->PvHandle, "SyncOutGpoLevels", syncs);
            break;
        case PSSyncOut2Level:
            status |= PvAttrUint32Get(this->PvHandle, "SyncOutGpoLevels", &syncs);
            syncs = (syncs & ~0x02) | ((value<<1) & 0x02);
            status |= PvAttrUint32Set(this->PvHandle, "SyncOutGpoLevels", syncs);
            break;
        case PSSyncOut3Level:
            status |= PvAttrUint32Get(this->PvHandle, "SyncOutGpoLevels", &syncs);
            syncs = (syncs & ~0x04) | ((value<<2) & 0x04);
            status |= PvAttrUint32Set(this->PvHandle, "SyncOutGpoLevels", syncs);
            break;
        case PSSyncOut1Invert:
            status |= PvAttrEnumSet(this->PvHandle, "SyncOut1Invert", value ? "On":"Off");
            break;
        case PSSyncOut2Invert:
            status |= PvAttrEnumSet(this->PvHandle, "SyncOut2Invert", value ? "On":"Off");
            break;
        case PSSyncOut3Invert:
            status |= PvAttrEnumSet(this->PvHandle, "SyncOut3Invert", value ? "On":"Off");
            if (status == ePvErrNotFound) status = 0;
            break;
        case PSStrobe1Mode:
            status |= PvAttrEnumSet(this->PvHandle, "Strobe1Mode", PSStrobeModes[value]);
            break;
        case PSStrobe1CtlDuration:
            status |= PvAttrEnumSet(this->PvHandle, "Strobe1ControlledDuration", value ? "On":"Off");
            break;
        case ADWriteFile:
            if (value) {
                /* Call the callbacks so the status changes */
                callParamCallbacks();
                if (this->pArrays[0]) {
                    status = writeFile();
                } else {
                    asynPrint(pasynUser, ASYN_TRACE_ERROR,
                        "%s:%s: ERROR, no valid array to write",
                        driverName, functionName);
                    status = asynError;
                }
                /* Set the flag back to 0, since this could be a busy record */
                setIntegerParam(ADWriteFile, 0);
            }
            break;
        case ADDataType:
        case ADColorMode:
            status = setPixelFormat();
            break;
    }
    
    /* Read the camera parameters and do callbacks */
    status |= readParameters();    
    if (status) 
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
              "%s:%s: error, status=%d function=%d, value=%d\n", 
              driverName, functionName, status, function, value);
    else        
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, value=%d\n", 
              driverName, functionName, function, value);
    return((asynStatus)status);
}

asynStatus prosilica::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
    int function = pasynUser->reason;
    int status = asynSuccess;
    static const char *functionName = "writeFloat64";

    /* Set the parameter and readback in the parameter library.  This may be overwritten when we read back the
     * status at the end, but that's OK */
    status |= setDoubleParam(function, value);

    switch (function) {
        case ADAcquireTime:
            /* Prosilica uses integer microseconds */
            status |= PvAttrUint32Set(this->PvHandle, "ExposureValue", (tPvUint32)(value * 1e6));
            break;
        case ADAcquirePeriod:
            /* Prosilica uses a frame rate in Hz */
            if (value == 0.) value = .01;
            status |= PvAttrFloat32Set(this->PvHandle, "FrameRate", (tPvFloat32)(1./value));
            break;
        case ADGain:
            /* Prosilica uses an integer value */
            status |= PvAttrUint32Set(this->PvHandle, "GainValue", (tPvUint32)(value));
            break;
        case PSStrobe1Delay:
            /* Prosilica uses integer microseconds */
            status |= PvAttrUint32Set(this->PvHandle, "Strobe1Delay", (tPvUint32)(value*1e6));
            break;
        case PSStrobe1Duration:
            /* Prosilica uses integer microseconds */
            status |= PvAttrUint32Set(this->PvHandle, "Strobe1Duration", (tPvUint32)(value*1e6));
            break;
        default:
            break;
    }

    /* Read the camera parameters and do callbacks */
    status |= readParameters();
    if (status) 
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
              "%s:%s: error, status=%d function=%d, value=%f\n", 
              driverName, functionName, status, function, value);
    else        
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, value=%f\n", 
              driverName, functionName, function, value);
    return((asynStatus)status);
}


/* asynDrvUser routines */
asynStatus prosilica::drvUserCreate(asynUser *pasynUser,
                                    const char *drvInfo, 
                                    const char **pptypeName, size_t *psize)
{
    asynStatus status;
    int param;
    static const char *functionName = "drvUserCreate";

    /* See if this is one of this drivers' parameters */
    status = findParam(PSDetParamString, NUM_PS_DET_PARAMS, 
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
                  "%s:%s, drvInfo=%s, param=%d\n", 
                  driverName, functionName, drvInfo, param);
        return(asynSuccess);
    } 
    /* This was not one of our driver parameters, call the base class method */
    status = ADDriver::drvUserCreate(pasynUser, drvInfo, pptypeName, psize); 
    return(status);
}
    
void prosilica::report(FILE *fp, int details)
{
    tPvCameraInfo cameraInfo[20]; 
    int i;
    unsigned long numReturned, numTotal;
    
    numReturned = PvCameraList(cameraInfo, 20, &numTotal);

    fprintf(fp, "Prosilica camera %s Unique ID=%d\n", 
            this->portName, (int)this->uniqueId);
    if (details > 0) {
        fprintf(fp, "  ID:                %lu\n", this->PvCameraInfo.UniqueId);
        fprintf(fp, "  IP address:        %s\n",  this->IPAddress);
        fprintf(fp, "  Serial number:     %s\n",  this->PvCameraInfo.SerialString);
        fprintf(fp, "  Model:             %s\n",  this->PvCameraInfo.DisplayName);
        fprintf(fp, "  Sensor type:       %s\n",  this->sensorType);
        fprintf(fp, "  Sensor bits:       %d\n",  (int)this->sensorBits);
        fprintf(fp, "  Sensor width:      %d\n",  (int)this->sensorWidth);
        fprintf(fp, "  Sensor height:     %d\n",  (int)this->sensorHeight);
        fprintf(fp, "  Frame buffer size: %d\n",  (int)this->PvFrames[0].ImageBufferSize);
        fprintf(fp, "  Time stamp freq:   %d\n",  (int)this->timeStampFrequency);
        fprintf(fp, "\n");
        fprintf(fp, "List of all Prosilica cameras found, (total=%d):\n", (int)numReturned);
        for (i=0; i<(int)numReturned; i++) {
            fprintf(fp, "    ID: %d\n", (int)cameraInfo[i].UniqueId);
        }
    }

    /* Call the base class method */
    ADDriver::report(fp, details);
}


extern "C" int prosilicaConfig(char *portName, /* Port name */
                               int uniqueId,   /* Unique ID # of this camera. */
                               int maxBuffers,
                               size_t maxMemory)
{
    new prosilica(portName, uniqueId, maxBuffers, maxMemory);
    return(asynSuccess);
}   


prosilica::prosilica(const char *portName, int uniqueId, int maxBuffers, size_t maxMemory)
    : ADDriver(portName, 1, ADLastDriverParam, maxBuffers, maxMemory, 0, 0), 
      PvHandle(NULL), uniqueId(uniqueId), framesRemaining(0)

{
    int status = asynSuccess;
    static const char *functionName = "prosilica";

    /* There is a conflict with readline use of signals, don't use readline signal handlers */
#ifdef linux
    rl_catch_signals = 0;
#endif
    /* Initialize the Prosilica PvAPI library 
     * We get an error if we call this twice, so we need a global flag to see if 
     * it's already been done.*/
    if (!PvApiInitialized) {
        status = PvInitialize();
        if (status) {
            printf("%s:%s: ERROR: PvInitialize failed for camera %d, status=%d\n", 
            driverName, functionName, uniqueId, status);
            return;
        }
        PvApiInitialized = 1;
    }
    
    /* It appears to be necessary to wait a little for the PvAPI library to find the cameras */
    epicsThreadSleep(0.2);
    
    /* Try to connect to the camera.  
     * It is not a fatal error if we cannot now, the camera may be off or owned by
     * someone else.  It may connect later. */
    status = connectCamera();
    if (status) {
        printf("%s:%s: cannot connect to camera %d, manually connect when available.\n", 
               driverName, functionName, uniqueId);
        return;
    }
        
}

