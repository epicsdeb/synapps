/* roper.cpp
 *
 * This is a driver for Roper Scientific area detectors.
 *
 * Author: Mark Rivers
 *         University of Chicago
 *
 * Created: November 26, 2008
 *
 */
 
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

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

#include "ADDriver.h"

#include "stdafx.h"
#include "CWinx32App20.h"
#include "CExpSetup20.h"
#include "CDocFile40.h"
#include "CROIRect0.h"

/** The following macro initializes COM for the default COINIT_MULTITHREADED model 
 * This needs to be done in each thread that can call the COM interfaces 
 * These threads are:
 *   - The thread that runs when the roper object is created (typically from st.cmd)
 *   - The roperTask thread that controls acquisition
 *   - The port driver thread that sets parameters */
#define INITIALIZE_COM CoInitializeEx(NULL, 0)
#define ERROR_MESSAGE_SIZE 256
#define MAX_COMMENT_SIZE 80
/** The polling interval when checking to see if acquisition is complete */
#define ROPER_POLL_TIME .01

static const char *controllerNames[] = {
    "No Controller",
    "ST143",
    "ST130",
    "ST138",
    "VICCD BOX",
    "PentaMax",
    "ST120_T1",
    "ST120_T2",
    "ST121",
    "ST135",
    "ST133",
    "VICCD",
    "ST116",
    "OMA3",
    "LOW_COST_SPEC",
    "MICROMAX",
    "SPECTROMAX",
    "MICROVIEW",
    "ST133_5MHZ",
    "EMPTY_5MHZ",
    "EPIX_CONTROLLER",
    "PVCAM",
    "GENERIC",
    "ARC_CCD_100",
    "ST133_2MHZ"
};

typedef enum {
    RoperImageNormal,
    RoperImageContinuous,
    RoperImageFocus
} RoperImageMode_t;

typedef enum {
    RoperShutterNormal,
    RoperShutterClosed,
    RoperShutterOpen
} RoperShutterMode_t;

/** Driver-specific parameters for the Roper driver */
#define RoperShutterModeString            "ROPER_SHUTTER_MODE"
#define RoperNumAcquisitionsString        "ROPER_NACQUISITIONS"
#define RoperNumAcquisitionsCounterString "ROPER_NACQUISITIONS_COUNTER"
#define RoperAutoDataTypeString           "AUTO_DATA_TYPE"
#define RoperComment1String               "COMMENT1"
#define RoperComment2String               "COMMENT2"
#define RoperComment3String               "COMMENT3"
#define RoperComment4String               "COMMENT4"
#define RoperComment5String               "COMMENT5"

static int numControllerNames = sizeof(controllerNames)/sizeof(controllerNames[0]);

static const char *driverName = "drvRoper";

/** Driver for Roper (Princeton Instrument and Photometrics) cameras using the COM interface
  * to WinView or WinSpec. 
  * Version 2.5.22 or later of those programs is required. */
class roper : public ADDriver {
public:
    roper(const char *portName,
          int maxBuffers, size_t maxMemory,
          int priority, int stackSize);
                 
    /* These are the methods that we override from ADDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual void setShutter(int open);
    virtual asynStatus drvUserCreate(asynUser *pasynUser, const char *drvInfo, 
                                     const char **pptypeName, size_t *psize);
    void report(FILE *fp, int details);
    void roperTask();  /* This should be private but is called from C, must be public */

protected:
    int RoperShutterMode;
    #define FIRST_ROPER_PARAM RoperShutterMode
    int RoperNumAcquisitions;
    int RoperNumAcquisitionsCounter;
    int RoperAutoDataType;
    int RoperComment1;
    int RoperComment2;
    int RoperComment3;
    int RoperComment4;
    int RoperComment5;
    #define LAST_ROPER_PARAM RoperComment5
         
private:                               
    asynStatus setROI();
    NDArray *getData();
    asynStatus getStatus();
    asynStatus convertDataType(NDDataType_t NDArrayType, int *roperType);
    int getAcquireStatus();
    asynStatus saveFile();
    
    /* Our data */
    epicsEventId startEventId;
    epicsEventId stopEventId;
    CWinx32App20 *pWinx32App;
    CExpSetup20  *pExpSetup;
    CDocFile40   *pDocFile;
    CROIRect0    *pROIRect;
    char errorMessage[ERROR_MESSAGE_SIZE];
};


#define NUM_ROPER_PARAMS (&LAST_ROPER_PARAM - &FIRST_ROPER_PARAM + 1)

/* Convert from a C string to a BSTR.  This must be freed by the caller! */
BSTR stringToBSTR(char *str)
{
    size_t len = strlen(str)+1;
    OLECHAR *wideTemp = new OLECHAR[len];
    BSTR pOut;
    
    mbstowcs(wideTemp, str, len);
    pOut = SysAllocString(wideTemp);
    delete wideTemp;
    return(pOut);
}
    
asynStatus roper::saveFile() 
{
    char fullFileName[MAX_FILENAME_LEN];
    char comment[MAX_COMMENT_SIZE];
    VARIANT varArg;
    BSTR bstr;
    int docType;
    asynStatus status=asynSuccess;
    const char *functionName="saveFile";
    
    VariantInit(&varArg);
    getIntegerParam(NDFileFormat, &docType);
    this->createFileName(MAX_FILENAME_LEN, fullFileName);

    try {
        getStringParam(RoperComment1, MAX_COMMENT_SIZE, comment);
        bstr = stringToBSTR(comment);
        varArg.vt = VT_BSTR;
        varArg.bstrVal = bstr;
        this->pDocFile->SetParam(DM_USERCOMMENT1, &varArg);
        SysFreeString(bstr);

        getStringParam(RoperComment2, MAX_COMMENT_SIZE, comment);
        bstr = stringToBSTR(comment);
        varArg.vt = VT_BSTR;
        varArg.bstrVal = bstr;
        this->pDocFile->SetParam(DM_USERCOMMENT2, &varArg);
        SysFreeString(bstr);

        getStringParam(RoperComment3, MAX_COMMENT_SIZE, comment);
        bstr = stringToBSTR(comment);
        varArg.vt = VT_BSTR;
        varArg.bstrVal = bstr;
        this->pDocFile->SetParam(DM_USERCOMMENT3, &varArg);
        SysFreeString(bstr);

        getStringParam(RoperComment4, MAX_COMMENT_SIZE, comment);
        bstr = stringToBSTR(comment);
        varArg.vt = VT_BSTR;
        varArg.bstrVal = bstr;
        this->pDocFile->SetParam(DM_USERCOMMENT4, &varArg);
        SysFreeString(bstr);

        getStringParam(RoperComment5, MAX_COMMENT_SIZE, comment);
        bstr = stringToBSTR(comment);
        varArg.vt = VT_BSTR;
        varArg.bstrVal = bstr;
        this->pDocFile->SetParam(DM_USERCOMMENT5, &varArg);
        SysFreeString(bstr);

        this->pDocFile->SaveAs(fullFileName, docType);
        setStringParam(NDFullFileName, fullFileName);
    }
    catch(CException *pEx) {
        pEx->GetErrorMessage(this->errorMessage, sizeof(this->errorMessage));
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: exception = %s\n", 
            driverName, functionName, this->errorMessage);
        pEx->Delete();
        status = asynError;
    }
    return(status);
}

NDArray *roper::getData()
{
    NDArray *pArray = NULL;
    VARIANT varData, varResult;
    short result;
    SAFEARRAY *pData;
    int dim;
    LONG lbound, ubound;
    int dims[ND_ARRAY_MAX_DIMS];
    int nDims;
    VARTYPE varType;
    NDDataType_t dataType;
    int docDataType;
    NDArrayInfo arrayInfo;
    void HUGEP *pVarData;
    bool typeMismatch;
    const char *functionName = "getData";
        
    VariantInit(&varData);
    VariantInit(&varResult);
    try {
        this->pDocFile->GetFrame(1, &varData);
        varResult = this->pDocFile->GetParam(DM_DATATYPE, &result);
        docDataType = varResult.lVal;
        pData = varData.parray;
        nDims = SafeArrayGetDim(pData);
        for (dim=0; dim<nDims; dim++) {
            SafeArrayGetLBound(pData, dim+1, &lbound);
            SafeArrayGetUBound(pData, dim+1, &ubound);
            dims[dim] = ubound - lbound + 1;
        }
        SafeArrayGetVartype(pData, &varType);
        typeMismatch = TRUE;
        switch (docDataType) {
            case X_BYTE:
                dataType = NDUInt8;
                typeMismatch = (varType != VT_UI1);
                break;
            case X_SHORT:
                dataType = NDInt16;
                typeMismatch = (varType != VT_I2);
                break;
            case X_UINT16:
                dataType = NDUInt16;
                typeMismatch = (varType != VT_I2);
                break;
            case X_LONG:
                dataType = NDInt32;
                typeMismatch = (varType != VT_I4);
                break;
            case X_ULONG:
                dataType = NDUInt32;
                typeMismatch = (varType != VT_I4);
                break;
            case X_FLOAT:
                dataType = NDFloat32;
                typeMismatch = (varType != VT_R4);
                break;
            case X_DOUBLE:
                dataType = NDFloat64;
                typeMismatch = (varType != VT_R8);
                break;
            default:
                asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: unknown data type = %d\n", 
                    driverName, functionName, docDataType);
                return(NULL);
        }
        if (typeMismatch) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "%s:%s: data type mismatch: docDataType=%d, varType=%d\n", 
                driverName, functionName, docDataType, varType);
            return(NULL);
        }
        pArray = this->pNDArrayPool->alloc(nDims, dims, dataType, 0, NULL);
        pArray->getInfo(&arrayInfo);
        SafeArrayAccessData(pData, &pVarData);
        memcpy(pArray->pData, pVarData, arrayInfo.totalBytes);
        SafeArrayUnaccessData(pData);
        SafeArrayDestroy(pData);
        setIntegerParam(NDArraySize, arrayInfo.totalBytes);
        setIntegerParam(NDArraySizeX, dims[0]);
        setIntegerParam(NDArraySizeY, dims[1]);
        setIntegerParam(NDDataType, dataType);
    }
    catch(CException *pEx) {
        pEx->GetErrorMessage(this->errorMessage, sizeof(this->errorMessage));
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: exception = %s\n", 
            driverName, functionName, this->errorMessage);
        pEx->Delete();
        return(NULL);
    }
        
    return(pArray);
}

int roper::getAcquireStatus()
{
    short result;
    int acquire;
    const char *functionName = "getAcquireStatus";
    VARIANT varResult;
    
    try {
        varResult = pExpSetup->GetParam(EXP_RUNNING, &result);
        acquire = varResult.lVal;
        /* NOTE: EXP_CSEQUENTS and EXP_CACCUMS should do what we want, but in spite of being
         * documented they actually do not work.  Skip. */
        /*
        varResult = pExpSetup->GetParam(EXP_CSEQUENTS, &result);
        setIntegerParam(ADNumImagesCounter, varResult.lVal);
        varResult = pExpSetup->GetParam(EXP_CACCUMS, &result);
        setIntegerParam(ADNumExposuresCounter, varResult.lVal);
        */
    }
    catch(CException *pEx) {
        pEx->GetErrorMessage(this->errorMessage, sizeof(this->errorMessage));
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: exception = %s\n", 
            driverName, functionName, this->errorMessage);
        pEx->Delete();
        return(-1);
    }
    callParamCallbacks();
    return(acquire);
}

asynStatus roper::getStatus()
{
    /* NOTE: There seems to be a problem in WinView.  
     * If this function is called immediately after acquisition is started
     * then the detector seems to be instantly done when getAcquireStatus() is called?
     * Work around this by only calling getStatus() from writeInt32 and writeFloat64 if a parameter
     * is actually changed, not when the ADFileNumber or other writes happen */
    short result;
    const char *functionName = "getStatus";
    VARIANT varResult;
    IDispatch *pROIDispatch;
    double top, bottom, left, right;
    long minX, minY, sizeX, sizeY, binX, binY;
    
    try {
        varResult = pExpSetup->GetParam(EXP_REVERSE, &result);
        setIntegerParam(ADReverseX, varResult.lVal);
        varResult = pExpSetup->GetParam(EXP_FLIP, &result);
        setIntegerParam(ADReverseY, varResult.lVal);
        varResult = pExpSetup->GetParam(EXP_SEQUENTS, &result);
        setIntegerParam(ADNumImages, varResult.lVal);
        varResult = pExpSetup->GetParam(EXP_SHUTTER_CONTROL, &result);
        setIntegerParam(RoperShutterMode, varResult.lVal);
        varResult = pExpSetup->GetParam(EXP_TIMING_MODE, &result);
        setIntegerParam(ADTriggerMode, varResult.lVal);
        varResult = pExpSetup->GetParam(EXP_AUTOD, &result);
        setIntegerParam(RoperAutoDataType, varResult.lVal);
        varResult = pExpSetup->GetParam(EXP_GAIN, &result);
        setDoubleParam(ADGain, (double)varResult.lVal);
        //varResult = pExpSetup->GetParam(EXP_FOCUS_NFRAME, &result);
        varResult = pExpSetup->GetParam(EXP_EXPOSURE, &result);
        setDoubleParam(ADAcquireTime, varResult.dblVal);
        varResult = pExpSetup->GetParam(EXP_ACTUAL_TEMP, &result);
        setDoubleParam(ADTemperature, varResult.dblVal);
        pROIDispatch = pExpSetup->GetROI(1);
        pROIRect->AttachDispatch(pROIDispatch);
        pROIRect->Get(&top, &left, &bottom, &right, &binX, &binY);
        minX = int(left);
        minY = int(top);
        sizeX = int(right)-int(left)+1;
        sizeY = int(bottom)-int(top)+1;
        setIntegerParam(ADMinX, minX);
        setIntegerParam(ADMinY, minY);
        setIntegerParam(ADSizeX, sizeX);
        setIntegerParam(ADSizeY, sizeY);
    }
    catch(CException *pEx) {
        pEx->GetErrorMessage(this->errorMessage, sizeof(this->errorMessage));
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: exception = %s\n", 
            driverName, functionName, this->errorMessage);
        pEx->Delete();
        return(asynError);
    }
    callParamCallbacks();
    return(asynSuccess);
}

asynStatus roper::convertDataType(NDDataType_t NDArrayType, int *roperType) 
{
    asynStatus status = asynSuccess;
    const char *functionName = "convertDataType";
    
    switch (NDArrayType) {
        case NDInt8:
        case NDUInt8:
            *roperType = X_BYTE;
            break;
        case NDInt16:
            *roperType = X_SHORT;
            break;
        case NDUInt16:
            *roperType = X_UINT16;
            break;
        case NDInt32:
            *roperType = X_LONG;
            break;
        case NDUInt32:
            *roperType = X_ULONG;
            break;
        case NDFloat32:
            *roperType = X_FLOAT;
            break;
        case NDFloat64:
            *roperType = X_DOUBLE;
            break;
        default:
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "%s:%s: unknown data type = %d\n", 
                driverName, functionName, NDArrayType);
            status = asynError;
        break;
    }
    return(status);
}

asynStatus roper::setROI()
{
    int minX, minY, sizeX, sizeY, binX, binY, maxSizeX, maxSizeY;
    double ROILeft, ROIRight, ROITop, ROIBottom;
    asynStatus status;
    VARIANT  varArg;
    const char *functionName = "setROI";

    VariantInit(&varArg);
    status = getIntegerParam(ADMinX,  &minX);
    status = getIntegerParam(ADMinY,  &minY);
    status = getIntegerParam(ADSizeX, &sizeX);
    status = getIntegerParam(ADSizeY, &sizeY);
    status = getIntegerParam(ADBinX,  &binX);
    status = getIntegerParam(ADBinY,  &binY);
    status = getIntegerParam(ADMaxSizeX, &maxSizeX);
    status = getIntegerParam(ADMaxSizeY, &maxSizeY);
    /* Make sure parameters are consistent, fix them if they are not */
    if (binX < 1) {
        binX = 1; 
        status = setIntegerParam(ADBinX, binX);
    }
    if (binY < 1) {
        binY = 1;
        status = setIntegerParam(ADBinY, binY);
    }
    if (minX < 1) {
        minX = 1; 
        status = setIntegerParam(ADMinX, minX);
    }
    if (minY < 1) {
        minY = 1; 
        status = setIntegerParam(ADMinY, minY);
    }
    if (minX > maxSizeX-binX) {
        minX = maxSizeX-binX; 
        status = setIntegerParam(ADMinX, minX);
    }
    if (minY > maxSizeY-binY) {
        minY = maxSizeY-binY; 
        status = setIntegerParam(ADMinY, minY);
    }
    if (sizeX < binX) sizeX = binX;    
    if (sizeY < binY) sizeY = binY;    
    if (minX+sizeX-1 > maxSizeX) sizeX = maxSizeX-minX+1; 
    if (minY+sizeY-1 > maxSizeY) sizeY = maxSizeY-minY+1; 
    /* The size must be a multiple of the binning or the controller can generate an error */
    sizeX = (sizeX/binX) * binX;
    sizeY = (sizeY/binY) * binY;
    status = setIntegerParam(ADSizeX, sizeX);
    status = setIntegerParam(ADSizeY, sizeY);
    ROILeft = minX;
    ROIRight = minX + sizeX - 1;
    ROITop = minY;
    ROIBottom = minY + sizeY - 1;
    
    try {
        this->pROIRect->Set(ROITop, ROILeft, ROIBottom, ROIRight, binX, binY);
        this->pExpSetup->ClearROIs();
        this->pExpSetup->SetROI(this->pROIRect->m_lpDispatch);
        varArg.vt = VT_I2;
        varArg.iVal = 1;
        this->pExpSetup->SetParam(EXP_USEROI, &varArg);
    }
    catch(CException *pEx) {
        pEx->GetErrorMessage(this->errorMessage, sizeof(this->errorMessage));
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: exception = %s\n", 
            driverName, functionName, this->errorMessage);
        pEx->Delete();
        return(asynError);
    }
    return(asynSuccess);
}

void roper::setShutter(int open)
{
    int shutterMode;
    
    getIntegerParam(ADShutterMode, &shutterMode);
    if (shutterMode == ADShutterModeDetector) {
        /* Simulate a shutter by just changing the status readback */
        setIntegerParam(ADShutterStatus, open);
    } else {
        /* For no shutter or EPICS shutter call the base class method */
        ADDriver::setShutter(open);
    }
}


static void roperTaskC(void *drvPvt)
{
    roper *pPvt = (roper *)drvPvt;
    
    pPvt->roperTask();
}

/** This thread computes new image data and does the callbacks to send it to higher layers */
void roper::roperTask()
{
    int status = asynSuccess;
    int imageCounter;
    int numAcquisitions, numAcquisitionsCounter;
    int imageMode;
    int arrayCallbacks;
    int acquire, autoSave;
    NDArray *pImage;
    double acquireTime, acquirePeriod, delay;
    epicsTimeStamp startTime, endTime;
    double elapsedTime;
    const char *functionName = "roperTask";
    VARIANT varArg;
    IDispatch *pDocFileDispatch;
    HRESULT hr;

    /* Initialize the COM system for this thread */
    hr = INITIALIZE_COM;
    if (hr == S_FALSE) {
        /* COM was already initialized for this thread */
        CoUninitialize();
    } else if (hr != S_OK) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: error initializing COM\n",
            driverName, functionName);
    }
    VariantInit(&varArg);

    this->lock();
    /* Loop forever */
    while (1) {
        /* Is acquisition active? */
        getIntegerParam(ADAcquire, &acquire);
        
        /* If we are not acquiring then wait for a semaphore that is given when acquisition is started */
        if (!acquire) {
            setIntegerParam(ADStatus, ADStatusIdle);
            callParamCallbacks();
            /* Release the lock while we wait for an event that says acquire has started, then lock again */
            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
                "%s:%s: waiting for acquire to start\n", driverName, functionName);
            this->unlock();
            status = epicsEventWait(this->startEventId);
            this->lock();
            getIntegerParam(ADAcquire, &acquire);
            setIntegerParam(RoperNumAcquisitionsCounter, 0);
        }
        
        /* We are acquiring. */
        /* Get the current time */
        epicsTimeGetCurrent(&startTime);
        
        /* Get the exposure parameters */
        getDoubleParam(ADAcquireTime, &acquireTime);
        getDoubleParam(ADAcquirePeriod, &acquirePeriod);
        getIntegerParam(ADImageMode, &imageMode);
        getIntegerParam(RoperNumAcquisitions, &numAcquisitions);
        
        setIntegerParam(ADStatus, ADStatusAcquire);
        
        /* Open the shutter */
        setShutter(ADShutterOpen);

        /* Call the callbacks to update any changes */
        callParamCallbacks();

        try {
            /* Collect the frame(s) */
            /* Stop current exposure, if any */
            this->pExpSetup->Stop();
            this->pDocFile->Close();
            switch (imageMode) {
                case RoperImageNormal:
                case RoperImageContinuous:
                    pDocFileDispatch = pExpSetup->Start2(&varArg);
                    break;
                case RoperImageFocus:
                    pDocFileDispatch = pExpSetup->StartFocus2(&varArg);
                    break;
            }
            pDocFile->AttachDispatch(pDocFileDispatch);

            /* Wait for acquisition to complete, but allow acquire stop events to be handled */
            while (1) {
                this->unlock();
                status = epicsEventWaitWithTimeout(this->stopEventId, ROPER_POLL_TIME);
                this->lock();
                if (status == epicsEventWaitOK) {
                    /* We got a stop event, abort acquisition */
                    this->pExpSetup->Stop();
                    acquire = 0;
                } else {
                    acquire = this->getAcquireStatus();
                }
                if (!acquire) {
                    /* Close the shutter */
                    setShutter(ADShutterClosed);
                    break;
                }
            }
        }
        catch(CException *pEx) {
            pEx->GetErrorMessage(this->errorMessage, sizeof(this->errorMessage));
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "%s:%s: exception = %s\n", 
                driverName, functionName, this->errorMessage);
            pEx->Delete();
        }
        
        /* Get the current parameters */
        getIntegerParam(NDAutoSave,         &autoSave);
        getIntegerParam(NDArrayCounter,     &imageCounter);
        getIntegerParam(RoperNumAcquisitionsCounter, &numAcquisitionsCounter);
        getIntegerParam(NDArrayCallbacks,   &arrayCallbacks);
        imageCounter++;
        numAcquisitionsCounter++;
        setIntegerParam(NDArrayCounter, imageCounter);
        setIntegerParam(RoperNumAcquisitionsCounter, numAcquisitionsCounter);
        
        if (arrayCallbacks) {
            /* Get the data from the DocFile */
            pImage = this->getData();
            if (pImage)  {
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
                     "%s:%s: calling imageData callback\n", driverName, functionName);
                doCallbacksGenericPointer(pImage, NDArrayData, 0);
                this->lock();
                pImage->release();
            }
        }
        
        /* See if we should save the file */
        if ((imageMode != RoperImageFocus) && autoSave) {
            setIntegerParam(ADStatus, ADStatusSaving);
            callParamCallbacks();
            this->saveFile();
            callParamCallbacks();
        }
        
        /* See if acquisition is done */
        if ((imageMode == RoperImageFocus) ||
            ((imageMode == RoperImageNormal) && 
             (numAcquisitionsCounter >= numAcquisitions))) {
            setIntegerParam(ADAcquire, 0);
            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
                  "%s:%s: acquisition completed\n", driverName, functionName);
        }
        
        /* Call the callbacks to update any changes */
        callParamCallbacks();
        getIntegerParam(ADAcquire, &acquire);
        
        /* If we are acquiring then sleep for the acquire period minus elapsed time. */
        if (acquire) {
            epicsTimeGetCurrent(&endTime);
            elapsedTime = epicsTimeDiffInSeconds(&endTime, &startTime);
            delay = acquirePeriod - elapsedTime;
            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
                     "%s:%s: delay=%f\n",
                      driverName, functionName, delay);            
            if (delay >= 0.0) {
                /* We set the status to indicate we are in the period delay */
                setIntegerParam(ADStatus, ADStatusWaiting);
                callParamCallbacks();
                this->unlock();
                status = epicsEventWaitWithTimeout(this->stopEventId, delay);
                this->lock();
            }
        }
    }
}


/** Called when asyn clients call pasynInt32->write().
  * This function performs actions for some parameters, including ADAcquire, ADBinX, etc.
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Value to write. */
asynStatus roper::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function = pasynUser->reason;
    int currentlyAcquiring;
    int dataType;
    int roperDataType;
    asynStatus status = asynSuccess;
    VARIANT varArg;
    int needReadStatus=1;
    const char* functionName="writeInt32";

    /* Initialize the variant and set reasonable defaults for data type and value */
    VariantInit(&varArg);
    varArg.vt = VT_I4;
    varArg.lVal = value;
    
    /* See if we are currently acquiring.  This must be done before the call to setIntegerParam below */
    getIntegerParam(ADAcquire, &currentlyAcquiring);
    
    /* Set the parameter and readback in the parameter library.  This may be overwritten when we read back the
     * status at the end, but that's OK */
    status = setIntegerParam(function, value);

    try {
        if (function == ADAcquire) {
            if (value && !currentlyAcquiring) {
                /* Send an event to wake up the Roper task.  
                 * It won't actually start generating new images until we release the lock below */
                epicsEventSignal(this->startEventId);
            } 
            if (!value && currentlyAcquiring) {
                /* This was a command to stop acquisition */
                /* Send the stop event */
                epicsEventSignal(this->stopEventId);
            }
        } else if ( (function == ADBinX) ||
                    (function == ADBinY) ||
                    (function == ADMinX) ||
                    (function == ADMinY) ||
                    (function == ADSizeX) ||
                    (function == ADSizeY)) {
            this->setROI();
        } else if (function == NDDataType) {
            convertDataType((NDDataType_t) value, &roperDataType);
            varArg.lVal = roperDataType;
            this->pExpSetup->SetParam(EXP_DATATYPE, &varArg);
        } else if (function == ADNumImages) {
            this->pExpSetup->SetParam(EXP_SEQUENTS, &varArg);
        } else if (function == ADNumExposures) {
            this->pExpSetup->SetParam(EXP_ACCUMS, &varArg);
        } else if (function == ADReverseX) {
            this->pExpSetup->SetParam(EXP_REVERSE, &varArg);
        } else if (function == ADReverseY) {
            this->pExpSetup->SetParam(EXP_FLIP, &varArg);
        } else if (function == NDWriteFile) {
            /* Call the callbacks so the busy state is visible while file is being saved */
            callParamCallbacks();
            status = this->saveFile();
            setIntegerParam(NDWriteFile, 0);
        } else if (function == ADTriggerMode) {
            this->pExpSetup->SetParam(EXP_TIMING_MODE, &varArg);
        } else if (function == RoperShutterMode) {
            this->pExpSetup->SetParam(EXP_SHUTTER_CONTROL, &varArg);
        } else if (function == ADShutterControl) {
            setShutter(value);
            needReadStatus = 0;
        } else if (function == RoperAutoDataType) {
            getIntegerParam(NDDataType, &dataType);
            this->pExpSetup->SetParam(EXP_AUTOD, &varArg);
            if (value == 0) { /* Not auto data type, re-send data type */
                getIntegerParam(NDDataType, &dataType);
                convertDataType((NDDataType_t)dataType, &roperDataType);
                varArg.lVal = roperDataType;
                this->pExpSetup->SetParam(EXP_DATATYPE, &varArg);
            }
        } else {
            needReadStatus = 0;
            /* If this parameter belongs to a base class call its method */
            if (function < FIRST_ROPER_PARAM) status = ADDriver::writeInt32(pasynUser, value);
        }
    }
    catch(CException *pEx) {
        pEx->GetErrorMessage(this->errorMessage, sizeof(this->errorMessage));
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: exception = %s\n", 
            driverName, functionName, this->errorMessage);
        pEx->Delete();
        status = asynError;
    }
    
    /* Read the actual state of the detector after this operation if anything could have changed */
    if (needReadStatus) this->getStatus();
    
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
asynStatus roper::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    VARIANT varArg;
    int needReadStatus=1;
    const char* functionName="writeInt32";

    /* Initialize the variant and set reasonable defaults for data type and value */
    VariantInit(&varArg);
    varArg.vt = VT_R4;
    varArg.fltVal = (epicsFloat32)value;

    /* Set the parameter and readback in the parameter library.  This may be overwritten when we read back the
     * status at the end, but that's OK */
    status = setDoubleParam(function, value);

    /* Changing any of the following parameters requires recomputing the base image */
    try {
        if (function == ADAcquireTime) {
            this->pExpSetup->SetParam(EXP_EXPOSURE, &varArg);
        } else if (function == ADTemperature) {
            this->pExpSetup->SetParam(EXP_TEMPERATURE, &varArg);
        } else if (function == ADGain) {
            varArg.vt = VT_I4;
            varArg.lVal = (int)value;
            this->pExpSetup->SetParam(EXP_GAIN, &varArg);
        } else {
            needReadStatus = 0;
            /* If this parameter belongs to a base class call its method */
            if (function < FIRST_ROPER_PARAM) status = ADDriver::writeFloat64(pasynUser, value);
        }
    }
    catch(CException *pEx) {
        pEx->GetErrorMessage(this->errorMessage, sizeof(this->errorMessage));
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: exception = %s\n", 
            driverName, functionName, this->errorMessage);
        pEx->Delete();
    }

    /* Read the actual state of the detector after this operation */
    if (needReadStatus) this->getStatus();

    /* Do callbacks so higher layers see any changes */
    callParamCallbacks();
    if (status) 
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
              "%s:%s, status=%d function=%d, value=%f\n", 
              driverName, functionName, status, function, value);
    else        
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, value=%f\n", 
              driverName, functionName, function, value);
    return status;
}


/** Sets pasynUser->reason to one of the enum values for the parameters defined for
  * this class if the drvInfo field matches one the strings defined for it.
  * If the parameter is not recognized by this class then calls ADDriver::drvUserCreate.
  * \param[in] pasynUser pasynUser structure that driver modifies
  * \param[in] drvInfo String containing information about what driver function is being referenced
  * \param[out] pptypeName Location in which driver puts a copy of drvInfo.
  * \param[out] psize Location where driver puts size of param 
  * \return Returns asynSuccess if a matching string was found, asynError if not found. */
asynStatus roper::drvUserCreate(asynUser *pasynUser,
                                       const char *drvInfo, 
                                       const char **pptypeName, size_t *psize)
{
    asynStatus status;
    const char *functionName = "drvUserCreate";
    
    HRESULT hr;

    /* Initialize the COM system for this thread */
    hr = INITIALIZE_COM;
    if (hr == S_FALSE) {
        /* COM was already initialized for this thread */
        CoUninitialize();
    } else if (hr != S_OK) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "%s:%s: error initializing COM\n",
            driverName, functionName);
    }

     /* Call the base class method */
    status = ADDriver::drvUserCreate(pasynUser, drvInfo, pptypeName, psize);
    return(status);
}
    
/** Report status of the driver.
  * Prints details about the driver if details>0.
  * It then calls the ADDriver::report() method.
  * \param[in] fp File pointed passed by caller where the output is written to.
  * \param[in] details If >0 then driver details are printed.
  */
void roper::report(FILE *fp, int details)
{

    fprintf(fp, "Roper detector %s\n", this->portName);
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

extern "C" int roperConfig(const char *portName,
                           int maxBuffers, size_t maxMemory,
                           int priority, int stackSize)
{
    new roper(portName, maxBuffers, maxMemory, priority, stackSize);
    return(asynSuccess);
}

/** Constructor for Roper driver; most parameters are simply passed to ADDriver::ADDriver.
  * After calling the base class constructor this method creates a thread to collect the detector data, 
  * and sets reasonable default values for the parameters defined in this class, asynNDArrayDriver and
  * ADDriver.
  * \param[in] portName The name of the asyn port driver to be created.
  * \param[in] maxBuffers The maximum number of NDArray buffers that the NDArrayPool for this driver is 
  *            allowed to allocate. Set this to -1 to allow an unlimited number of buffers.
  * \param[in] maxMemory The maximum amount of memory that the NDArrayPool for this driver is 
  *            allowed to allocate. Set this to -1 to allow an unlimited amount of memory.
  * \param[in] priority The thread priority for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  * \param[in] stackSize The stack size for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  */
roper::roper(const char *portName,
             int maxBuffers, size_t maxMemory,
             int priority, int stackSize)

    : ADDriver(portName, 1, NUM_ROPER_PARAMS, maxBuffers, maxMemory, 
               0, 0,             /* No interfaces beyond those set in ADDriver.cpp */
               ASYN_CANBLOCK, 1, /* ASYN_CANBLOCK=1, ASYN_MULTIDEVICE=0, autoConnect=1 */
               priority, stackSize)

{
    int status = asynSuccess;
    const char *functionName = "roper";
    VARIANT varResult;
    HRESULT hr;
    short result;
    const char *controllerName;
    int controllerNum;
    IDispatch *pDocFileDispatch;

    createParam(RoperShutterModeString,            asynParamInt32,   &RoperShutterMode);
    createParam(RoperNumAcquisitionsString,        asynParamInt32,   &RoperNumAcquisitions);
    createParam(RoperNumAcquisitionsCounterString, asynParamInt32,   &RoperNumAcquisitionsCounter);
    createParam(RoperAutoDataTypeString,           asynParamInt32,   &RoperAutoDataType);
    createParam(RoperComment1String,               asynParamOctet,   &RoperComment1);
    createParam(RoperComment2String,               asynParamOctet,   &RoperComment2);
    createParam(RoperComment3String,               asynParamOctet,   &RoperComment3);
    createParam(RoperComment4String,               asynParamOctet,   &RoperComment4);
    createParam(RoperComment5String,               asynParamOctet,   &RoperComment5);
 
    /* Initialize the COM system for this thread */
    hr = INITIALIZE_COM;
    if (hr != S_OK) {
        printf("%s:%s: error initializing COM\n",
            driverName, functionName);
    }

    /* Create the COleDispatchDriver objects used to communicate with COM */
    VariantInit(&varResult);
    this->pWinx32App = new(CWinx32App20);
    this->pExpSetup  = new(CExpSetup20);
    this->pDocFile   = new(CDocFile40);
    this->pROIRect   = new(CROIRect0);

    /* Connect to the WinX32App and ExpSetup COM objects */
    if (!pWinx32App->CreateDispatch("WinX32.Winx32App.2")) {
        printf("%s:%s: error creating WinX32App COM object\n",
            driverName, functionName);
        return;
    }
    if (!pExpSetup->CreateDispatch("WinX32.ExpSetup.2")) {
        printf("%s:%s: error creating ExpSetup COM object\n",
            driverName, functionName);
        return;
    }
    if (!pROIRect->CreateDispatch("WinX32.ROIRect")) {
        printf("%s:%s: error creating ROIRect COM object\n",
            driverName, functionName);
        return;
    }

    try {
        varResult = this->pExpSetup->GetParam(EXP_CONTROLLER_NAME, &result);
        controllerNum = varResult.lVal;
        if (controllerNum < 0) controllerNum = 0;
        if (controllerNum >= numControllerNames) controllerNum = 0;
        controllerName = controllerNames[controllerNum];
        varResult = pExpSetup->GetParam(EXP_XDIMDET, &result);
        setIntegerParam(ADMaxSizeX, varResult.lVal);
        varResult = pExpSetup->GetParam(EXP_YDIMDET, &result);
        setIntegerParam(ADMaxSizeY, varResult.lVal);
        pDocFileDispatch = pExpSetup->GetDocument();
        if (pDocFileDispatch) {
            pDocFile->AttachDispatch(pDocFileDispatch);
            getData();
        } else {
            setIntegerParam(NDArraySizeX, 0);
            setIntegerParam(NDArraySizeY, 0);
            setIntegerParam(NDArraySize, 0);
            setIntegerParam(NDDataType, NDUInt16);
        }
    }
    catch(CException *pEx) {
        pEx->GetErrorMessage(this->errorMessage, sizeof(this->errorMessage));
        printf("%s:%s: exception = %s\n", 
            driverName, functionName, this->errorMessage);
        pEx->Delete();
        return;
    }

    /* Read the state of the detector */
    status = this->getStatus();
    if (status) {
        printf("%s:%s: unable to read detector status\n", driverName, functionName);
        return;
    }

   /* Create the epicsEvents for signaling to the acquisition task when acquisition starts and stops */
    this->startEventId = epicsEventCreate(epicsEventEmpty);
    if (!this->startEventId) {
        printf("%s:%s: epicsEventCreate failure for start event\n", 
            driverName, functionName);
        return;
    }
    this->stopEventId = epicsEventCreate(epicsEventEmpty);
    if (!this->stopEventId) {
        printf("%s:%s: epicsEventCreate failure for stop event\n", 
            driverName, functionName);
        return;
    }
    
    /* Set some default values for parameters */
    status =  setStringParam (ADManufacturer, "Roper Scientific");
    status |= setStringParam (ADModel, controllerName);
    status |= setIntegerParam(ADImageMode, ADImageSingle);
    status |= setDoubleParam (ADAcquireTime, .1);
    status |= setDoubleParam (ADAcquirePeriod, .5);
    status |= setIntegerParam(ADNumImages, 1);
    status |= setIntegerParam(RoperNumAcquisitions, 1);
    status |= setIntegerParam(RoperNumAcquisitionsCounter, 0);
    status |= setIntegerParam(RoperAutoDataType, 1);
    if (status) {
        printf("%s:%s: unable to set camera parameters\n", driverName, functionName);
        return;
    }
    
    /* Create the thread that updates the images */
    status = (epicsThreadCreate("roperTask",
                                epicsThreadPriorityMedium,
                                epicsThreadGetStackSize(epicsThreadStackMedium),
                                (EPICSTHREADFUNC)roperTaskC,
                                this) == NULL);
    if (status) {
        printf("%s:%s: epicsThreadCreate failure for Roper task\n", 
            driverName, functionName);
        return;
    }
}

/* Code for iocsh registration */
static const iocshArg roperConfigArg0 = {"Port name", iocshArgString};
static const iocshArg roperConfigArg1 = {"maxBuffers", iocshArgInt};
static const iocshArg roperConfigArg2 = {"maxMemory", iocshArgInt};
static const iocshArg roperConfigArg3 = {"priority", iocshArgInt};
static const iocshArg roperConfigArg4 = {"stackSize", iocshArgInt};
static const iocshArg * const roperConfigArgs[] =  {&roperConfigArg0,
                                                    &roperConfigArg1,
                                                    &roperConfigArg2,
                                                    &roperConfigArg3,
                                                    &roperConfigArg4};
static const iocshFuncDef configRoper = {"roperConfig", 5, roperConfigArgs};
static void configRoperCallFunc(const iocshArgBuf *args)
{
    roperConfig(args[0].sval, args[1].ival, args[2].ival,
                args[3].ival, args[4].ival);
}


static void roperRegister(void)
{
    iocshRegister(&configRoper, configRoperCallFunc);
}

extern "C" {
epicsExportRegistrar(roperRegister);
}
