/*
 * Author: Mark Rivers 
 *         University of Chicago
 *
 * License: This file is part of 'areaDetector'
 * 
 * 'firewireDCAM' is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * 'firewireWinDCAM' is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with 'firewireWinDCAM'.  If not, see <http://www.gnu.org/licenses/>.
 */
 
/** firewireWinDCAM.cpp
 *  This is areaDetector driver support for firewire cameras that comply
 *  with the IIDC DCAM protocol. This implements the FirewireWinDCAM class which
 *  inherits from the areaDetector ADDriver class.
 *
 *  The driver uses the 1394Camera library from Carnegie-Mellon University
 *
 *  Created: March 2009
 *
 */

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
#include <epicsEndian.h>
#include <iocsh.h>
#include <epicsExport.h>

/* Dependency support modules includes:
 * asyn, areaDetector, CMU 1394 camera library */
#include <ADDriver.h>

#include <stdafx.h>

/* 1394Camera includes */
#include <1394Camera.h>

/** Convenience macro to be used inside the firewireDCAM class. */
#define PERR(errCode) this->err(errCode, __LINE__)

#define MAX_1394_BUFFERS 6
#define MAX_1394_VIDEO_FORMATS 8
#define MAX_1394_VIDEO_MODES 8
#define MAX_1394_FRAME_RATES 8

#define MAX(x,y) ((x)>(y)?(x):(y))

/** Specific asyn commands for this support module. These will be used and
 * managed by the parameter library (part of areaDetector). */
#define FDC_feat_valString           "FDC_FEAT_VAL"
#define FDC_feat_val_maxString       "FDC_FEAT_VAL_MAX"
#define FDC_feat_val_minString       "FDC_FEAT_VAL_MIN"
#define FDC_feat_val_absString       "FDC_FEAT_VAL_ABS"
#define FDC_feat_val_abs_maxString   "FDC_FEAT_VAL_ABS_MAX"
#define FDC_feat_val_abs_minString   "FDC_FEAT_VAL_ABS_MIN"
#define FDC_feat_modeString          "FDC_FEAT_MODE"
#define FDC_feat_availableString     "FDC_FEAT_AVAILABLE"
#define FDC_feat_absoluteString      "FDC_FEAT_ABSOLUTE"
#define FDC_formatString             "FDC_FORMAT"
#define FDC_modeString               "FDC_MODE"
#define FDC_framerateString          "FDC_FRAMERATE"
#define FDC_colorcodeString          "FDC_COLORCODE"
#define FDC_valid_formatString       "FDC_VALID_FORMAT"
#define FDC_valid_modeString         "FDC_VALID_MODE"
#define FDC_valid_framerateString    "FDC_VALID_FRAMERATE"
#define FDC_valid_colorcodeString    "FDC_VALID_COLORCODE"
#define FDC_has_formatString         "FDC_HAS_FORMAT"
#define FDC_has_modeString           "FDC_HAS_MODE"
#define FDC_has_framerateString      "FDC_HAS_FRAMERATE"
#define FDC_has_colorcodeString      "FDC_HAS_COLORCODE"
#define FDC_current_formatString     "FDC_CURRENT_FORMAT"
#define FDC_current_modeString       "FDC_CURRENT_MODE"
#define FDC_current_framerateString  "FDC_CURRENT_FRAMERATE"
#define FDC_current_colorcodeString  "FDC_CURRENT_COLORCODE"
#define FDC_dropped_framesString     "FDC_DROPPED_FRAMES"

/** Only used for debugging/error messages to identify where the message comes from*/
static const char *driverName = "FirewireWinDCAM";

/** Main driver class inherited from areaDetectors ADDriver class.
 * One instance of this class will control one firewire camera on the bus.
 */
class FirewireWinDCAM : public ADDriver
{
public:
    FirewireWinDCAM(const char *portName, const char* camid,
                 int maxBuffers, size_t maxMemory,
                 int priority, int stackSize);

    /* virtual methods to override from ADDriver */
    virtual asynStatus writeInt32( asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus writeFloat64( asynUser *pasynUser, epicsFloat64 value);
    void report(FILE *fp, int details);
    void imageGrabTask();  /**< This should be private but is called from C callback function, must be public. */

protected:
    int FDC_feat_val;                       /** Feature value (int32 read/write) addr: 0-17 */
    #define FIRST_FDC_PARAM FDC_feat_val
    int FDC_feat_val_max;                  /** Feature maximum boundry value (int32 read) addr: 0-17 */
    int FDC_feat_val_min;                  /** Feature minimum boundry value (int32 read)  addr: 0-17*/
    int FDC_feat_val_abs;                  /** Feature absolute value (float64 read/write) addr: 0-17 */
    int FDC_feat_val_abs_max;              /** Feature absolute maximum boundry value (float64 read) addr: 0-17 */
    int FDC_feat_val_abs_min;              /** Feature absolute minimum boundry value (float64 read) addr: 0-17 */
    int FDC_feat_mode;                     /** Feature control mode: 0:manual or 1:automatic (camera controlled) (int32 read/write)*/
    int FDC_feat_available;                /** Is a given featurea available in the camera 1=available 0=not available (int32, read) */
    int FDC_feat_absolute;                 /** Feature has absolute (floating point) controls available 1=available 0=not available (int32 read) */
    int FDC_format;                        /** Set and read back the video format (int32 (enums) read/write)*/
    int FDC_mode;                          /** Set and read back the video mode (int32 (enums) read/write)*/
    int FDC_framerate;                     /** Set and read back the frame rate (int32 (enums) read/write)*/
    int FDC_colorcode;                     /** Set and read back the color code (int32 (enums) read/write)*/
    int FDC_valid_format;                  /** Read back the valid video formats (octet, read)*/
    int FDC_valid_mode;                    /** Read back the valid video modes (octet, read)*/
    int FDC_valid_framerate;               /** Read back the valid frame rates (octet, read)*/
    int FDC_valid_colorcode;               /** Read back the valid color codes (octet, read)*/
    int FDC_has_format;                    /** Read back whether video format is supported (int32, read)*/
    int FDC_has_mode;                      /** Read back whether video mode is supported (int32, read)*/
    int FDC_has_framerate;                 /** Read back whether video framerate is supported (int32, read)*/
    int FDC_has_colorcode;                 /** Read back whether color code is supported (int32, read)*/
    int FDC_current_format;                /** Read back the current video format (octet, read)*/
    int FDC_current_mode;                  /** Read back the current video mode (octet, read)*/
    int FDC_current_framerate;             /** Read back the current frame rate (octet, read)*/
    int FDC_current_colorcode;             /** Read back the current color mcde (octet, read)*/
    int FDC_dropped_frames;                /** Number of dropped frames (int32, read)*/
    #define LAST_FDC_PARAM FDC_dropped_frames

private:
    /* Local methods to this class */
    int grabImage();
    asynStatus startCapture();
    asynStatus stopCapture();

    /* camera feature control functions */
    asynStatus setFeatureValue(int feature, epicsInt32 value);
    asynStatus setFeatureAbsValue(int feature, epicsFloat64 value);
    asynStatus setFeatureMode(int feature, epicsInt32 value);
    C1394CameraControl* checkFeature(int feature, char **featureName);
    asynStatus setVideoFormat(epicsInt32 format);
    asynStatus setVideoMode(epicsInt32 mode);
    asynStatus setFrameRate(epicsInt32 rate);
    asynStatus setFormat7Params();
    asynStatus formatFormat7Modes();
    asynStatus formatValidModes();
    asynStatus getAllFeatures();
    asynStatus err(int CAM_err, int errOriginLine);


    /* Data */
    NDArray *pRaw;
    C1394Camera *pCamera;
    C1394CameraControlSize *pCameraControlSize;
    C1394CameraControl **pCameraControl;
    epicsEventId startEventId;
};
/* end of FirewireWinDCAM class description */

/** Number of asyn parameters (asyn commands) this driver supports. */
#define NUM_FDC_PARAMS (&LAST_FDC_PARAM - &FIRST_FDC_PARAM + 1)

/** Configuration function to configure one camera.
 *
 * This function need to be called once for each camera to be used by the IOC. A call to this
 * function instanciates one object from the FirewireWinDCAM class.
 * \param[in] portName Asyn port name to assign to the camera.
 * \param[in] camid The camera ID or serial number in a hexadecimal string. Lower case and
 *            upper case letters can be used. This is used to identify a specific camera
 *            on the bus. For instance: "0x00b09d01007139d0".  If this parameter is empty ("")
 *            then the first camera found on the Firewire bus will be used.
 * \param[in] maxBuffers Maxiumum number of NDArray objects (image buffers) this driver is allowed to allocate.
 *            This driver requires 2 buffers, and each queue element in a plugin can require one buffer
 *            which will all need to be added up in this parameter. Use -1 for unlimited.
 * \param[in] maxMemory Maximum memory (in bytes) that this driver is allowed to allocate. So if max. size = 1024x768 (8bpp)
 *            and maxBuffers is, say 14. maxMemory = 1024x768x14 = 11010048 bytes (~11MB). Use -1 for unlimited.
 * \param[in] priority The EPICS thread priority for this driver.  0=use asyn default.
 * \param[in] stackSize The size of the stack for the EPICS port thread. 0=use asyn default.
 */
extern "C" int WinFDC_Config(const char *portName, const char* camid, int maxBuffers, size_t maxMemory, int priority, int stackSize)
{
    new FirewireWinDCAM( portName, camid, maxBuffers, maxMemory, priority, stackSize);
    return asynSuccess;
}

static char *errMsg[] = {
    "Success",                  // 0
    "Generic error",            //-1
    "Unknown error",            //-2
    "Unknown error",            //-3
    "Unknown error",            //-4
    "Unknown error",            //-5
    "Unknown error",            //-6
    "Unknown error",            //-7
    "Unknown error",            //-8
    "Unknown error",            //-9
    "Unsupported",              //-10
    "Not initialized",          //-11
    "Invalid video settings",   //-12
    "Busy",                     //-13
    "Insufficient resources",   //-14
    "Param out of range",       //-15
    "Frame timeout",            //-16
};

static char *videoFormatStrings[MAX_1394_VIDEO_FORMATS] = {
    "VGA",
    "Super VGA 1",
    "Super VGA 2",
    "Reserved",
    "Reserved",
    "Reserved",
    "Still image",
    "User-defined"
};

static char *videoModeStrings[MAX_1394_VIDEO_FORMATS][MAX_1394_VIDEO_MODES] = {
    {"160x120 YUV444",  "320x240 YUV422", "640x480 YUV411", "640X480 YUV422",   "640x480 RGB",   "640x480 Mono8",   "640x480 Mono16",  "Reserved"        },
    {"800x600 YUV422",  "800x600 RGB",    "800x600 Mono8",  "1024x768 YUV422",  "1024x768 RGB",  "1024x768 Mono8",  "800x600 Mono16",  "1024x768 Mono16" },
    {"1280x960 YUV422", "1280x960 RGB",   "1280x960 Mono8", "1600x1200 YUV422", "1600x1200 RGB", "1600x1200 Mono8", "1280x960 Mono16", "1600x1200 Mono16"},
    {"Reserved",        "Reserved",       "Reserved",       "Reserved",         "Reserved",      "Reserved",        "Reserved",        "Reserved"        },
    {"Reserved",        "Reserved",       "Reserved",       "Reserved",         "Reserved",      "Reserved",        "Reserved",        "Reserved"        },
    {"Reserved",        "Reserved",       "Reserved",       "Reserved",         "Reserved",      "Reserved",        "Reserved",        "Reserved"        },
    {"Exif",            "Reserved",       "Reserved",       "Reserved",         "Reserved",      "Reserved",        "Reserved",        "Reserved"        },
    {"N.A.",            "N.A.",           "N.A.",           "N.A.",             "N.A.",          "N.A.",            "N.A.",            "N.A."            }
};

static char *frameRateStrings[MAX_1394_FRAME_RATES] = {
    "1.875",
    "3.75",
    "7.5",
    "15",
    "30",
    "60",
    "120",
    "240"
};

static char *colorCodeStrings[COLOR_CODE_MAX] = {
    "Mono8",
    "YUV411",
    "YUV422",
    "YUV444",
    "RGB8",
    "Mono16",
    "RGB16",
    "Mono16_Signed",
    "RGB16_Signed",
    "Raw8",
    "Raw16",
};

/* This array converts from the 0-21 index used in the asyn addr field for features to the enum values
 * used by the CMU driver, which are not sequential */
static CAMERA_FEATURE featureIndex[] = {
    FEATURE_BRIGHTNESS,
    FEATURE_AUTO_EXPOSURE,
    FEATURE_SHARPNESS,
    FEATURE_WHITE_BALANCE,
    FEATURE_HUE,
    FEATURE_SATURATION,
    FEATURE_GAMMA,
    FEATURE_SHUTTER,
    FEATURE_GAIN,
    FEATURE_IRIS,
    FEATURE_FOCUS,
    FEATURE_TEMPERATURE,
    FEATURE_TRIGGER_MODE,
    FEATURE_TRIGGER_DELAY,
    FEATURE_WHITE_SHADING,
    FEATURE_FRAME_RATE,
    FEATURE_ZOOM,
    FEATURE_PAN,
    FEATURE_TILT,
    FEATURE_OPTICAL_FILTER,
    FEATURE_CAPTURE_SIZE,
    FEATURE_CAPTURE_QUALITY
};
static int num1394Features = sizeof(featureIndex) / sizeof(featureIndex[0]);

static void imageGrabTaskC(void *drvPvt)
{
    FirewireWinDCAM *pPvt = (FirewireWinDCAM *)drvPvt;

    pPvt->imageGrabTask();
}

/** Constructor for the FirewireWinDCAM class
 * Initialises the camera object by setting all the default parameters and initializing
 * the camera hardware with it. This function also reads out the current settings of the
 * camera and prints out a selection of parameters to the shell.
 * \param[in] portName Asyn port name to assign to the camera driver.
 * \param[in] camid The camera ID or serial number in a hexadecimal string. Lower case and
 *            upper case letters can be used. This is used to identify a specific camera
 *            on the bus. For instance: "0x00b09d01007139d0".  If this parameter is empty ("")
 *            then the first camera found on the Firewire bus will be used.
 * \param[in] maxBuffers Maxiumum number of NDArray objects (image buffers) this driver is allowed to allocate.
 *            This driver requires 2 buffers, and each queue element in a plugin can require one buffer
 *            which will all need to be added up in this parameter. Use -1 for unlimited.
 * \param[in] maxMemory Maximum memory (in bytes) that this driver is allowed to allocate. So if max. size = 1024x768 (8bpp)
 *            and maxBuffers is, say 14. maxMemory = 1024x768x14 = 11010048 bytes (~11MB). Use -1 for unlimited.
 * \param[in] priority The EPICS thread priority for this asyn port driver.  0=use asyn default.
 * \param[in] stackSize The size of the stack for the asyn port thread. 0=use asyn default.
 */
FirewireWinDCAM::FirewireWinDCAM(    const char *portName, const char* camid, 
                            int maxBuffers, size_t maxMemory, int priority, int stackSize )
    : ADDriver(portName, num1394Features, NUM_FDC_PARAMS, maxBuffers, maxMemory, 0, 0,
               ASYN_CANBLOCK | ASYN_MULTIDEVICE, 1, priority, stackSize),
        pRaw(NULL)
{
    const char *functionName = "FirewireWinDCAM";
    char vendorName[256], cameraName[256];
    LARGE_INTEGER uniqueId;
    unsigned long long int camUID = 0;
    unsigned long version;
    int err;
    int numCameras;
    int selectedCamera = -1;
    int i, status, ret;
    char chMode = 'A';

    /* parse the string of hex-numbers that is the cameras unique ID
     * If this string is not specified then we just use the first camera found */
    if (camid && (strlen(camid) > 0)) {
        ret = sscanf(camid, "%lld", &camUID);
    } else {
        selectedCamera = 0;
    }
    this->pCamera = new C1394Camera();
    numCameras = this->pCamera->RefreshCameraList();
    // Print out information about all the cameras found
    printf("%s::%s: %d cameras found\n", driverName, functionName, numCameras);
    if (numCameras < 1) return;
    for (i=0; i<numCameras; i++) {
        printf("  camera %d\n", i);
        err = this->pCamera->SelectCamera(i);
        status = PERR(err);
        this->pCamera->GetCameraVendor(vendorName, sizeof(vendorName));
        this->pCamera->GetCameraName(cameraName, sizeof(cameraName));
        this->pCamera->GetCameraUniqueID(&uniqueId);
        if (uniqueId.QuadPart == camUID) selectedCamera=i;
        version = this->pCamera->GetVersion();
        printf("Vendor name: %s\n", vendorName);
        printf("Camera name: %s\n", cameraName);
        printf("UniqueId: %lld (0x%llX)\n", uniqueId.QuadPart, uniqueId.QuadPart);
        printf("Version: 0x%lX\n", version);
    }
            
    /* If we didn't find the camera with the specific ID we return... */
    if (selectedCamera < 0)
    {
        fprintf(stderr,"### ERROR ### Did not find camera with GUID: 0x%16.16llX\n", camUID);
        return;
    } else {
        printf("Found requested camera, initializing ...\n ");
        err = this->pCamera->SelectCamera(selectedCamera);
        status = PERR(err);
        err = this->pCamera->InitCamera(TRUE);
        status = PERR(err);
    }
    this->pCameraControlSize = this->pCamera->GetCameraControlSize();
    this->pCameraControl = (C1394CameraControl **)malloc(num1394Features * sizeof(pCameraControl[0]));
    for (i=0; i<num1394Features; i++) {
        this->pCameraControl[i] = new C1394CameraControl(this->pCamera, featureIndex[i]);
    }

    createParam(FDC_feat_valString,             asynParamInt32,   &FDC_feat_val);
    createParam(FDC_feat_val_maxString,         asynParamInt32,   &FDC_feat_val_max);
    createParam(FDC_feat_val_minString,         asynParamInt32,   &FDC_feat_val_min);
    createParam(FDC_feat_val_absString,         asynParamFloat64, &FDC_feat_val_abs);
    createParam(FDC_feat_val_abs_maxString,     asynParamFloat64, &FDC_feat_val_abs_max);
    createParam(FDC_feat_val_abs_minString,     asynParamFloat64, &FDC_feat_val_abs_min);
    createParam(FDC_feat_modeString,            asynParamInt32,   &FDC_feat_mode);
    createParam(FDC_feat_availableString,       asynParamInt32,   &FDC_feat_available);
    createParam(FDC_feat_absoluteString,        asynParamInt32,   &FDC_feat_absolute);
    createParam(FDC_formatString,               asynParamInt32,   &FDC_format);
    createParam(FDC_modeString,                 asynParamInt32,   &FDC_mode);
    createParam(FDC_framerateString,            asynParamInt32,   &FDC_framerate);
    createParam(FDC_colorcodeString,            asynParamInt32,   &FDC_colorcode);
    createParam(FDC_valid_formatString,         asynParamOctet,   &FDC_valid_format);
    createParam(FDC_valid_modeString,           asynParamOctet,   &FDC_valid_mode);
    createParam(FDC_valid_framerateString,      asynParamOctet,   &FDC_valid_framerate);
    createParam(FDC_valid_colorcodeString,      asynParamOctet,   &FDC_valid_colorcode);
    createParam(FDC_has_formatString,           asynParamInt32,   &FDC_has_format);
    createParam(FDC_has_modeString,             asynParamInt32,   &FDC_has_mode);
    createParam(FDC_has_framerateString,        asynParamInt32,   &FDC_has_framerate);
    createParam(FDC_has_colorcodeString,        asynParamInt32,   &FDC_has_colorcode);
    createParam(FDC_current_formatString,       asynParamOctet,   &FDC_current_format);
    createParam(FDC_current_modeString,         asynParamOctet,   &FDC_current_mode);
    createParam(FDC_current_framerateString,    asynParamOctet,   &FDC_current_framerate);
    createParam(FDC_current_colorcodeString,    asynParamOctet,   &FDC_current_colorcode);
    createParam(FDC_dropped_framesString,       asynParamInt32,   &FDC_dropped_frames);

    this->pCamera->GetCameraVendor(vendorName, sizeof(vendorName));
    this->pCamera->GetCameraName(cameraName, sizeof(cameraName));
    status =  setStringParam (ADManufacturer, vendorName);
    status |= setStringParam (ADModel, cameraName);

    // We would like to get the camera chip size, but there is really no way to do this.
    // In Format 7 one can call 
    // this->pCameraControlSize->GetSizeLimits(&maxSizeX, &maxSizeY);
    // but even that just returns the largest size for that video mode, not for the entire chip. */
    
    /* Create the start and stop event that will be used to signal our
     * image grabbing thread when to start/stop     */
    printf("Creating EPICS events...                 ");
    this->startEventId = epicsEventCreate(epicsEventEmpty);
    printf("OK\n");

    status |= setIntegerParam(NDDataType, NDUInt8);
    status |= setIntegerParam(ADImageMode, ADImageContinuous);
    status |= setIntegerParam(ADNumImages, 100);
    printf("Creating Format 7 mode strings...                 ");
    status |= this->formatFormat7Modes();
    status |= this->formatValidModes();
    status |= this->getAllFeatures();
    if (status)
    {
         fprintf(stderr, "ERROR %s: unable to set camera parameters\n", functionName);
    } else printf("OK\n");

    /* Start up acquisition thread */
    printf("Starting up image grabbing task...     ");
    status = (epicsThreadCreate("imageGrabTask",
            epicsThreadPriorityMedium,
            epicsThreadGetStackSize(epicsThreadStackMedium),
            (EPICSTHREADFUNC)imageGrabTaskC,
            this) == NULL);
    if (status) {
        printf("%s:%s epicsThreadCreate failure for image task\n",
                driverName, functionName);
        return;
    } else printf("OK\n");
    printf("Configuration complete!\n");
    return;
}


/** Task to grab images off the camera and send them up to areaDetector
 *
 */
void FirewireWinDCAM::imageGrabTask()
{
    int status = asynSuccess;
    int imageCounter;
    int numImages, numImagesCounter;
    int imageMode;
    int arrayCallbacks;
    epicsTimeStamp startTime;
    int acquire;
    const char *functionName = "imageGrabTask";

    printf("FirewireWinDCAM::imageGrabTask: Got the image grabbing thread started!\n");

    this->lock();

    while (1) /* ... round and round and round we go ... */
    {
        /* Is acquisition active? */
        getIntegerParam(ADAcquire, &acquire);

        /* If we are not acquiring then wait for a semaphore that is given when acquisition is started */
        if (!acquire)
        {
            setIntegerParam(ADStatus, ADStatusIdle);
            callParamCallbacks();

            /* Release the lock while we wait for an event that says acquire has started, then lock again */
            this->unlock();

            /* Wait for a signal that tells this thread that the transmission
             * has started and we can start asking for image buffers...     */
            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                "%s::%s [%s]: waiting for acquire to start\n", 
                driverName, functionName, this->portName);
            status = epicsEventWait(this->startEventId);
            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                "%s::%s [%s]: started!\n", 
                driverName, functionName, this->portName);
            this->lock();
            setIntegerParam(ADNumImagesCounter, 0);
            setIntegerParam(ADAcquire, 1);
        }

        /* Get the current time */
        epicsTimeGetCurrent(&startTime);
        /* We are now waiting for an image  */
        setIntegerParam(ADStatus, ADStatusWaiting);
        /* Call the callbacks to update any changes */
        callParamCallbacks();

        status = this->grabImage();        /* #### GET THE IMAGE FROM CAMERA HERE! ##### */
        if (status == asynError)         /* check for error */
        {
            /* remember to release the NDArray back to the pool now
             * that we are not using it (we didn't get an image...) */
            if(this->pRaw) this->pRaw->release();
            /* We abort if we had some problem with grabbing an image...
             * This is perhaps not always the desired behaviour but it'll do for now. */
            setIntegerParam(ADStatus, ADStatusAborting);
            setIntegerParam(ADAcquire, 0);
            continue;
        }

        /* Set a bit of image/frame statistics... */
        getIntegerParam(NDArrayCounter, &imageCounter);
        getIntegerParam(ADNumImages, &numImages);
        getIntegerParam(ADNumImagesCounter, &numImagesCounter);
        getIntegerParam(ADImageMode, &imageMode);
        getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
        imageCounter++;
        numImagesCounter++;
        setIntegerParam(NDArrayCounter, imageCounter);
        setIntegerParam(ADNumImagesCounter, numImagesCounter);
        /* Put the frame number into the buffer */
        this->pRaw->uniqueId = imageCounter;
        /* Set a timestamp in the buffer */
        this->pRaw->timeStamp = startTime.secPastEpoch + startTime.nsec / 1.e9;

        /* Get any attributes that have been defined for this driver */        
        this->getAttributes(this->pRaw->pAttributeList);

        /* Call the callbacks to update any changes */
        callParamCallbacks();

        if (arrayCallbacks)
        {
            /* Call the NDArray callback */
            /* Must release the lock here, or we can get into a deadlock, because we can
             * block on the plugin lock, and the plugin can be calling us */
            this->unlock();
            doCallbacksGenericPointer(this->pRaw, NDArrayData, 0);
            this->lock();
        }
        /* Release the NDArray buffer now that we are done with it.
         * After the callback just above we don't need it anymore */
        this->pRaw->release();
        this->pRaw = NULL;

        /* See if acquisition is done if we are in single or multiple mode */
        if ((imageMode == ADImageSingle) || ((imageMode == ADImageMultiple) && (numImagesCounter >= numImages)))
        {
            /* command the camera to stop acquiring.. */
            setIntegerParam(ADAcquire, 0);
        }
        getIntegerParam(ADAcquire, &acquire);
        if (!acquire)
        {
            /* Acquisition has been turned off.  This could be because it was done by setting ADAcquire=0 from CA, or
             * because the requested number of frames is done, or because of an error.  Stop capture. */
            status = this->stopCapture();
            if (status == asynError)
            {
                asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
                    "%s::%s [%s] Stopping transmission failed...\n",
                    driverName, functionName, this->portName);
            }
            callParamCallbacks();
        }
    }/* back to the top... */
    return;
}

/** Grabs one image off the dc1394 queue, notifies areaDetector about it and
 * finally clears the buffer off the dc1394 queue.
 * This function expects the driver to be locked already by the caller!
 */
int FirewireWinDCAM::grabImage()
{
    int status = asynSuccess;
    NDDataType_t dataType;
    int dims[3];
    int err;
    unsigned long lsizeX, lsizeY;
    unsigned short sizeX, sizeY;
    unsigned long dataLength;
    unsigned short depth;
    int format, mode;
    int bytesPerColor;
    int numColors;
    int ndims;
    int droppedFrames, newDroppedFrames;
    NDColorMode_t colorMode;
    COLOR_CODE colorCode;
    unsigned char * pTmpData;
    int unsupportedFormat = 0;
    const char* functionName = "grabImage";

    /* unlock the driver while we wait for a new image to be ready */
    this->unlock();
    err = this->pCamera->AcquireImageEx(TRUE, &newDroppedFrames);
    status = PERR(err);
    this->lock();
    if (status) return status;   /* if we didn't get an image properly... */

    getIntegerParam(FDC_dropped_frames, &droppedFrames);
    droppedFrames += newDroppedFrames;
    setIntegerParam(FDC_dropped_frames, droppedFrames);
    
    /* Get the current video format */
    format = this->pCamera->GetVideoFormat();

    /* Get the size of the frame */
    if (format == 7) {
        this->pCameraControlSize->GetSize(&sizeX, &sizeY);
        this->pCameraControlSize->GetDataDepth(&depth);
        this->pCameraControlSize->GetColorCode(&colorCode);
        switch (colorCode) {
            case COLOR_CODE_Y8:
            case COLOR_CODE_RAW8:
                numColors = 1;
                bytesPerColor = 1;
                dataType = NDUInt8;
                break;
            case COLOR_CODE_Y16:
            case COLOR_CODE_RAW16:
                numColors = 1;
                bytesPerColor = 2;
                dataType = NDUInt16;
                break;
            case COLOR_CODE_Y16_SIGNED:
                numColors = 1;
                bytesPerColor = 2;
                dataType = NDInt16;
                break;
            case COLOR_CODE_YUV411:
            case COLOR_CODE_YUV422:
            case COLOR_CODE_YUV444:
            case COLOR_CODE_RGB8:
                numColors = 3;
                bytesPerColor = 1;
                dataType = NDUInt8;
                break;
            case COLOR_CODE_RGB16:
                numColors = 3;
                bytesPerColor = 2;
                dataType = NDUInt8;
                break;
            case COLOR_CODE_RGB16_SIGNED:
                numColors = 3;
                bytesPerColor = 2;
                dataType = NDInt8;
                break;
            default:
                unsupportedFormat = 1;
                break;
        }
    } else {
        this->pCamera->GetVideoFrameDimensions(&lsizeX, &lsizeY);
        sizeX = (unsigned short)lsizeX;
        sizeY = (unsigned short)lsizeY;
        this->pCamera->GetVideoDataDepth(&depth);
        mode = this->pCamera->GetVideoMode();
        switch (format) {
            case 0:
                switch (mode) {
                    case 0:  /* YUV 444 */
                    case 1:  /* YUV 422 */
                    case 2:  /* YUV 411 */
                    case 3:  /* YUV 422 */
                        numColors = 3;
                        bytesPerColor = 1;
                        dataType = NDUInt8;
                        break;
                    case 4: /* RGB 8 */
                        numColors = 3;
                        bytesPerColor = 1;
                        dataType = NDUInt8;
                        break;
                    case 5: /* Mono 8 */
                        numColors = 1;
                        bytesPerColor = 1;
                        dataType = NDUInt8;
                        break;
                    case 6: /* Mono 16 */
                        numColors = 1;
                        bytesPerColor = 2;
                        dataType = NDUInt16;
                        break;
                    default:
                        unsupportedFormat = 1;
                        break;
                }
                break;
            case 1:
            case 2:
                switch (mode) {
                    case 0:  /* YUV 422 */
                    case 1:  /* RGB 8 */
                    case 3:  /* YUV 422 */
                    case 4:  /* RGB 8 */
                        numColors = 3;
                        bytesPerColor = 1;
                        dataType = NDUInt8;
                        break;
                    case 2:
                    case 5: /* Mono 8 */
                        numColors = 1;
                        bytesPerColor = 1;
                        dataType = NDUInt8;
                        break;
                    case 6: /* Mono 16 */
                    case 7:
                        numColors = 1;
                        bytesPerColor = 2;
                        dataType = NDUInt16;
                        break;
                    default:
                        unsupportedFormat = 1;
                        break;
                }
                break;
            default:
                unsupportedFormat = 1;
                break;
        }
            
    }
    
    if (unsupportedFormat) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
            "%s:%s: unsupported format=%d and mode=%d combination\n",
            driverName, functionName, format, mode);
        return(asynError);
    }
    
    setIntegerParam(NDArraySizeX, sizeX);
    setIntegerParam(NDArraySizeY, sizeY);
    setIntegerParam(NDDataType, dataType);
    if (numColors == 3) {
        colorMode = NDColorModeRGB1;
    } else {
        /* If the color mode is currently set to Bayer leave it alone */
        getIntegerParam(NDColorMode, (int *)&colorMode);
        if (colorMode != NDColorModeBayer) colorMode = NDColorModeMono;
    }
    
    setIntegerParam(NDColorMode, colorMode);
    if (numColors == 1) {
        ndims = 2;
        dims[0] = sizeX;
        dims[1] = sizeY;
    } else {
        ndims = 3;
        dims[0] = 3;
        dims[1] = sizeX;
        dims[2] = sizeY;
    }
   
    this->pRaw = this->pNDArrayPool->alloc(ndims, dims, dataType, 0, NULL);
    if (!this->pRaw) {
        /* If we didn't get a valid buffer from the NDArrayPool we must abort
         * the acquisition as we have nowhere to dump the data...       */
        setIntegerParam(ADStatus, ADStatusAborting);
        callParamCallbacks();
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
            "%s::%s [%s] ERROR: Serious problem: not enough buffers left! Aborting acquisition!\n",
            driverName, functionName, this->portName);
        setIntegerParam(ADAcquire, 0);
        return(asynError);
    }


    /* tell our driver where to find the image buffer with this latest image */
    switch (colorMode) {
        case NDColorModeMono:
        case NDColorModeBayer:
            pTmpData = this->pCamera->GetRawData(&dataLength);
            if ((int)dataLength > this->pRaw->dataSize) dataLength = this->pRaw->dataSize;
            /* The Firewire byte order is big-endian.  If this is 16-bit data and we are on a little-endian
             * machine we need to swap bytes */
            if ((bytesPerColor == 1) || (EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG)) {
                memcpy((unsigned char*)this->pRaw->pData, pTmpData, dataLength);
            } else {
                swab((char *)pTmpData, (char *)this->pRaw->pData, dataLength);
            }
            break;
        case NDColorModeRGB1:
            err = this->pCamera->getRGB((unsigned char*)this->pRaw->pData, this->pRaw->dataSize);
            status = PERR(err);
            break;
        default:
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
                "%s:%s: unsupported color mode=%d\n",
                driverName, functionName, colorMode);
            break;
    }
    
    asynPrintIO(this->pasynUserSelf, ASYN_TRACEIO_DRIVER, 
        (const char*)this->pRaw->pData, this->pRaw->dataSize,
        "%s:%s: size=%d\n",
        driverName, functionName, this->pRaw->dataSize);
    
    /* Change the status to be readout... */
    setIntegerParam(ADStatus, ADStatusReadout);
    callParamCallbacks();

    this->pRaw->pAttributeList->add("ColorMode", "Color mode", NDAttrInt32, &colorMode);

    return (status);
}


/** Sets an int32 parameter.
  * \param[in] pasynUser asynUser structure that contains the function code in pasynUser->reason. 
  * \param[in] value The value for this parameter 
  *
  * Takes action if the function code requires it.  ADAcquire, ADSizeX, and many other
  * function codes make calls to the Firewire library from this function. */
asynStatus FirewireWinDCAM::writeInt32( asynUser *pasynUser, epicsInt32 value)
{
    asynStatus status = asynSuccess;
    int function = pasynUser->reason;
    int adstatus;
    int addr, feature, tmpVal;
    const char* functionName = "writeInt32";

    pasynManager->getAddr(pasynUser, &addr);
    feature = addr;
    if (addr < 0) addr=0;

    /* Set the value in the parameter library.  This may change later but that's OK */
    status = setIntegerParam(addr, function, value);

    if (function == ADAcquire) {
        getIntegerParam(ADStatus, &adstatus);
        if (value && (adstatus == ADStatusIdle))
        {
            /* start acquisition */
            status = this->startCapture();
        } else 
        {
            /* Nothing to do when acquire is turned off, just setting ADAcquire=0 above is enough */
        }
    } else if ( (function == ADSizeX) ||
                (function == ADSizeY) ||
                (function == ADMinX)  ||
                (function == ADMinY)  ||
                (function == FDC_colorcode)) {
        status = this->setFormat7Params();
    } else if (function == FDC_feat_val) {
        /* First check if the camera is set for manual control... */
        getIntegerParam(addr, FDC_feat_mode, &tmpVal);
        /* if it is not set to 'manual' (0) then we do set it to manual */
        if (tmpVal != 0) status = this->setFeatureMode(feature, 0);
        if (status == asynError) goto done;

        /* now send the feature value to the camera */
        status = this->setFeatureValue(feature, value);

        /* update all feature values to check if any settings have changed */
        status = this->getAllFeatures();
    } else if (function == FDC_feat_mode) {
        status = this->setFeatureMode(feature, value);
    } else if (function == FDC_format) {
        status = this->setVideoFormat(value);
    } else if (function == FDC_mode) {
        status = this->setVideoMode(value);
    } else if (function == FDC_framerate) {
        status = this->setFrameRate(value);
    } else {
        /* If this parameter belongs to a base class call its method */
        if (function < FIRST_FDC_PARAM) status = ADDriver::writeInt32(pasynUser, value);
    }

    done:
    
    /* Call the callback for the specific address .. and address ... weird? */
    asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER, 
        "%s::%s function=%d, value=%d, status=%d\n",
        driverName, functionName, function, value, status);
            
    callParamCallbacks(addr, addr);
    return status;
}

/** Sets an float64 parameter.
  * \param[in] pasynUser asynUser structure that contains the function code in pasynUser->reason. 
  * \param[in] value The value for this parameter 
  *
  * Takes action if the function code requires it.  The FDC_feat_val_abs
  * function code makes calls to the Firewire library from this function. */
asynStatus FirewireWinDCAM::writeFloat64( asynUser *pasynUser, epicsFloat64 value)
{
    asynStatus status = asynSuccess;
    int function = pasynUser->reason;
    int addr, feature, tmpVal;
    const char* functionName = "writeFloat64";
    
    pasynManager->getAddr(pasynUser, &addr);
    feature = addr;
    if (addr < 0) addr=0;

    /* Set the value in the parameter library.  This may change later but that's OK */
    status = setDoubleParam(addr, function, value);

    if ((function == FDC_feat_val_abs) || (function == ADAcquireTime)) {
        if (function == ADAcquireTime) feature = FEATURE_SHUTTER;
        /* First check if the camera is set for manual control... */
        getIntegerParam(feature, FDC_feat_mode, &tmpVal);
        /* if it is not set to 'manual' (0) then we do set it to manual */
        if (tmpVal != 0) status = this->setFeatureMode(feature, 0);

        status = this->setFeatureAbsValue(feature, value);
        /* update all feature values to check if any settings have changed */
        status = this->getAllFeatures();
    } else {
        /* If this parameter belongs to a base class call its method */
        if (function < FIRST_FDC_PARAM) status = ADDriver::writeFloat64(pasynUser, value);
    }

    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
        "%s::%s function=%d, value=%f, status=%d\n",
        driverName, functionName, function, value, status);
    callParamCallbacks(addr, addr);
    return status;
}

/** Check if a requested feature is valid
 *
 * Checks for:
 * 
 * Valid range of the asyn request address against feature index.
 * Availability of the requested feature in the current camera.
 * 
 * pasynUser
 * featInfo     Pointer to a feature info structure pointer. The function
 *              will write a valid feature info struct into this pointer or NULL on error.
 * featureName  The function will return a string in this parameter with a
 *              readable name for the given feature.
 * functionName The caller can pass a string which contain the callers function
 *              name. For debugging/printing purposes only.
 * asyn status
 */
C1394CameraControl* FirewireWinDCAM::checkFeature(int feature, char **featureName)
{
    asynStatus status = asynSuccess;
    const char* functionName = "checkFeature";
    C1394CameraControl *pFeature=NULL;

    if (feature < 0 || feature >= num1394Features)
    {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
            "%s::%s ERROR addr: %d is out of range [0..%d]\n",
            driverName, functionName, feature, num1394Features);
        return NULL;
    }

    /* Read the information from the camera */
    pFeature = this->pCameraControl[feature];
    pFeature->Inquire();

    /* Get a readable name for the feature we are working on */
    if (*featureName != NULL)
    {
        *featureName = (char *)pFeature->GetName();
    }

    /* check if the feature we are working on is available in this camera */
    if (!pFeature->HasPresence())
    {
        return NULL;
    }

    return pFeature;
}

asynStatus FirewireWinDCAM::setFeatureMode(int feature, epicsInt32 value)
{
    asynStatus status = asynSuccess;
    const char *functionName = "setFeatureMode";
    char *featureName;
    int err;
    C1394CameraControl *pFeature;
    
    /* First check if the feature is valid for this camera */
    pFeature = this->checkFeature(feature, &featureName);
    if (!pFeature) return asynError;

    /* Check if the desired mode is even supported by the camera on this feature */
    if (value == 0) {
        if (!pFeature->HasManualMode()) return asynError;
    } else {
        if (!pFeature->HasAutoMode()) return asynError;
    }

    /* Send the feature mode to the cam */
    err = pFeature->SetAutoMode(value);
    status = PERR(err);
    return status;
}


asynStatus FirewireWinDCAM::setFeatureValue(int feature, epicsInt32 value)
{
    asynStatus status = asynSuccess;
    int err;
    unsigned short min, max, lo, hi;
    const char *functionName = "setFeatureValue";
    C1394CameraControl *pFeature;
    char *featureName;

    /* First check if the feature is valid for this camera */
    pFeature = this->checkFeature(feature, &featureName);
    if (!pFeature) return status;

    /* Disable absolute mode control for this feature */
    err = pFeature->SetAbsControl(FALSE);
    status = PERR(err);
    if(status == asynError) return status;

     /* Set the feature value in the camera */
    lo = value & 0xFFF;
    hi = (value >> 12) & 0xFFF;
    /* Check the value is within the expected boundaries */
    pFeature->GetRange(&min, &max);
    if (lo < (epicsInt32)min || lo > (epicsInt32)max)
    {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
            "%s::%s ERROR [%s] setting feature %s, lo %d is out of range [%d..%d]\n",
            driverName, functionName, this->portName, featureName, lo, min, max);
        return asynError;
    }
    if (hi > (epicsInt32)max)
    {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
            "%s::%s ERROR [%s] setting feature %s, hi %d is out of range [%d..%d]\n",
            driverName, functionName, this->portName, featureName, hi, min, max);
        return asynError;
    }

    err = pFeature->SetValue(lo, hi);
    status = PERR(err);
    asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER, 
        "%s::%s set value=%d, status=%d\n",
        driverName, functionName, value, status);
    return status;
}


asynStatus FirewireWinDCAM::setFeatureAbsValue(int feature, epicsFloat64 value)
{
    asynStatus status = asynSuccess;
    int err;
    float min, max;
    const char *functionName = "setFeatureAbsValue";
    C1394CameraControl *pFeature;
    char *featureName;

    /* First check if the feature is valid for this camera */
    pFeature = this->checkFeature(feature, &featureName);
    if (!pFeature) return status;

    /* Check if the specific feature supports absolute values */
    if (!pFeature->HasAbsControl()) { 
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
            "%s::%s ERROR [%s] setting feature \'%s\': No absolute control for this feature\n",
            driverName, functionName, this->portName, featureName);
        return asynError;
    }

    /* Check the value is within the expected boundaries */
    pFeature->GetRangeAbsolute(&min, &max);
    if (value < min || value > max)
    {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
            "%s::%s ERROR [%s] setting feature %s, value %.5f is out of range [%.3f..%.3f]\n",
            driverName, functionName, this->portName, featureName, value, min, max);
        return asynError;
    }

    /* Enable absolute mode control for this feature */
    err = pFeature->SetAbsControl(TRUE);
    status = PERR(err);
    if(status == asynError) return status;

    /* Finally set the feature value in the camera */
    err = pFeature->SetValueAbsolute((float)value);
    status = PERR(err);

    asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER, 
        "%s::%s set value to cam: %f, status=%d\n",
        driverName, functionName, value, status);

    return status;
}

/** Set the framerate in the camera. */
asynStatus FirewireWinDCAM::setVideoFormat(epicsInt32 format)
{
    asynStatus status = asynSuccess;
    int err;
    int wasAcquiring;
    const char* functionName = "setVideoFormat";

    getIntegerParam(ADAcquire, &wasAcquiring);
    if (wasAcquiring) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
            "%s::%s ERROR [%s]: must stop acquisition before changing format\n",
            driverName, functionName, this->portName);
       return(asynError);
    }
    
    if (!this->pCamera->HasVideoFormat(format)) {
         asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
            "%s::%s ERROR [%s]: camera does not support format %d\n",
            driverName, functionName, this->portName, format);
        return(asynError);
    }

    /* attempt to write the format to camera */
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
        "%s::%s [%s]: setting video format:%d\n",
        driverName, functionName, this->portName,format);
    err = this->pCamera->SetVideoFormat(format);
    status = PERR(err );
    if (status == asynError) goto done;
    
    /* If the new format is format 7 then set the parameters */
    if (format == 7) this->setFormat7Params();

    done:
    /* When the format changes the supported values of video mode and frame rate change */
    this->formatValidModes();
    /* When the format changes the available features can also change */
    this->getAllFeatures();

    return status;
}

asynStatus FirewireWinDCAM::setVideoMode(epicsInt32 mode)
{
    asynStatus status = asynSuccess;
    int err;
    int wasAcquiring;
    int format;
    const char* functionName = "setVideoMode";

    getIntegerParam(ADAcquire, &wasAcquiring);
    if (wasAcquiring) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
            "%s::%s ERROR [%s]: must stop acquisition before changing video mode\n",
            driverName, functionName, this->portName);
       return(asynError);
    }
    /* Get the current video format */
    format = this->pCamera->GetVideoFormat();
    if (!this->pCamera->HasVideoMode(format, mode)) {
         asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
            "%s::%s ERROR [%s]: camera does not support mode %d\n",
            driverName, functionName, this->portName, mode);
       status = asynError;
       goto done;
    }

    /* attempt to write the mode to camera */
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
        "%s::%s [%s]: setting video mode:%d\n",
        driverName, functionName, this->portName, mode);
    err = this->pCamera->SetVideoMode(mode);
    status = PERR(err);

    done:
    /* If the new format is format 7 then set the parameters */
    if (format == 7) this->setFormat7Params();

    /* When the mode changes the supported values of frame rate change */
    this->formatValidModes();
    /* When the mode changes the available features can also change */
    this->getAllFeatures();

    return status;
}

asynStatus FirewireWinDCAM::setFrameRate(epicsInt32 rate)
{
    asynStatus status = asynSuccess;
    unsigned int newframerate = 0;
    int err;
    int wasAcquiring;
    int format, mode;
    const char* functionName = "setFrameRate";

    getIntegerParam(ADAcquire, &wasAcquiring);
    if (wasAcquiring) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
            "%s::%s ERROR [%s]: must stop acquisition before changing frame rate\n",
            driverName, functionName, this->portName);
       return(asynError);
    }

    /* Get the current video format and mode */
    format = this->pCamera->GetVideoFormat();
    mode = this->pCamera->GetVideoMode();
    if (!this->pCamera->HasVideoFrameRate(format, mode, rate)) {
         asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
            "%s::%s ERROR [%s]: camera does not support framerate %d\n",
            driverName, functionName, this->portName, rate);
        goto done;
    }

    /* attempt to write the framerate to camera */
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
        "%s::%s [%s]: setting framerate:%d\n",
        driverName, functionName, this->portName, rate);
    err = this->pCamera->SetVideoFrameRate(rate);
    status = PERR(err );
    if (status == asynError) goto done;

    done:
    /* When the mode changes the supported values of frame rate change */
    this->formatValidModes();
    /* When the mode changes the available features can also change */
    this->getAllFeatures();

    return status;
}

asynStatus FirewireWinDCAM::setFormat7Params()
{
    asynStatus status = asynSuccess;
    int err;
    int wasAcquiring;
    int format;
    int sizeX, sizeY, minX, minY;
    COLOR_CODE colorCode;
    unsigned short width, height, left, top;
    unsigned short hsMax, vsMax, hsUnit, vsUnit;
    unsigned short hpMax, vpMax, hpUnit, vpUnit;
    unsigned short bppMin, bppMax, bppCur, bppRec, bppAct;
    char str[40];
    const char* functionName = "setFormat7Params";

    getIntegerParam(ADAcquire, &wasAcquiring);
    if (wasAcquiring) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
            "%s::%s ERROR [%s]: must stop acquisition before changing format 7 params\n",
            driverName, functionName, this->portName);
       return(asynError);
    }

    /* Get the current video format */
    format = this->pCamera->GetVideoFormat();
    /* If not format 7 then silently exit */
    if (format != 7) goto done;
    
    getIntegerParam(ADSizeX, &sizeX);
    getIntegerParam(ADSizeY, &sizeY);
    getIntegerParam(ADMinX, &minX);
    getIntegerParam(ADMinY, &minY);
    getIntegerParam(FDC_colorcode, (int *)&colorCode);

    /* Get the size limits */
    this->pCameraControlSize->GetSizeLimits(&hsMax, &vsMax);
    setIntegerParam(ADMaxSizeX, hsMax);
    setIntegerParam(ADMaxSizeY, vsMax);
    /* Get the size units (minimum increment) */
    this->pCameraControlSize->GetSizeUnits(&hsUnit, &vsUnit);
    /* Get the position limits */
    this->pCameraControlSize->GetPosLimits(&hpMax, &vpMax);
    /* Get the position units (minimum increment) */
    this->pCameraControlSize->GetPosUnits(&hpUnit, &vpUnit);
 
    /* The position limits are defined as the difference from the max size values */
    hpMax = hsMax - hpMax;
    vpMax = vsMax - vpMax;
    asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER, 
        "%s:%s hsMax=%d, vsMax=%d, hsUnit=%d, vsUnit=%d, hpMax=%d, vpMax=%d, hpUnit=%d, vpUnit=%d\n", 
        driverName, functionName, hsMax, vsMax, hsUnit, vsUnit,  hpMax, vpMax, hpUnit, vpUnit);

    /* Force the requested values to obey the increment and range */
    if (sizeX % hsUnit) sizeX = (sizeX/hsUnit) * hsUnit;
    if (sizeY % vsUnit) sizeY = (sizeY/vsUnit) * vsUnit;
    if (minX % hpUnit)  minX  = (minX/hpUnit)  * hpUnit;
    if (minY % vpUnit)  minY  = (minY/vpUnit)  * vpUnit;
    
    if (sizeX < hsUnit) sizeX = hsUnit;
    if (sizeX > hsMax)  sizeX = hsMax;
    if (sizeY < vsUnit) sizeY = vsUnit;
    if (sizeY > vsMax)  sizeY = vsMax;
    
    if (minX < 0) minX = 0;
    if (minX > hpMax)  minX = hpMax;
    if (minY < 0) minY = 0;
    if (minY > vpMax)  minY = vpMax;
 
    /* attempt to write the parameters to camera */
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
        "%s::%s [%s]: setting format 7 parameters sizeX=%d, sizeY=%d, minX=%d, minY=%d, colorCode=%d\n",
        driverName, functionName, this->portName, sizeX, sizeY, minX, minY, colorCode);
    err = this->pCameraControlSize->SetColorCode(colorCode);
      status = PERR(err );
    if (status == asynError) goto done;
    err = this->pCameraControlSize->SetPos(minX, minY);
      status = PERR(err );
    if (status == asynError) goto done;
    err = this->pCameraControlSize->SetSize(sizeX, sizeY);
      status = PERR(err );
    if (status == asynError) goto done;
    this->pCameraControlSize->GetBytesPerPacketRange(&bppMin, &bppMax);
    this->pCameraControlSize->GetBytesPerPacket(&bppCur, &bppRec);
    bppAct = bppRec;
    asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
        "%s:%s bytes per packet: min=%d, max=%d, current=%d, recommended=%d, actually setting=%d\n", 
        driverName, functionName, bppMin, bppMax, bppCur, bppRec, bppAct);

    err = this->pCameraControlSize->SetBytesPerPacket(bppAct);
      status = PERR(err );
    if (status == asynError) goto done;

    done:
    /* Read back the actual values */
    this->pCameraControlSize->GetPos(&left, &top);
    setIntegerParam(ADMinX, left);
    setIntegerParam(ADMinY, top);
    this->pCameraControlSize->GetSize(&width, &height);
    setIntegerParam(ADSizeX, width);
    setIntegerParam(ADSizeY, height);
    this->pCameraControlSize->GetColorCode((COLOR_CODE *)&colorCode);
    setIntegerParam(FDC_colorcode, colorCode);
    sprintf(str, "%s", colorCodeStrings[colorCode]);
    setStringParam(FDC_current_colorcode, str);
    callParamCallbacks();

    return status;
}



asynStatus FirewireWinDCAM::formatValidModes()
{
    int format, mode, rate;
    int addr, maxAddr;
    int colorCode;
    char str[40];
 
    /* This function writes strings for all valid video formats,
     * the valid video modes for the current video format, and the
     * valid frame rates for the current video format and video mode. */

    /* Format all valid video formats */
    for (format=0; format<MAX_1394_VIDEO_FORMATS; format++) {
        if (this->pCamera->HasVideoFormat(format)) {
            sprintf(str, "%d %s", format, videoFormatStrings[format]);
            setIntegerParam(format, FDC_has_format, 1);
        } else {
            sprintf(str, "%d N.A.", format);
            setIntegerParam(format, FDC_has_format, 0);
        }
        setStringParam(format, FDC_valid_format, str);
    }

    /* Get the current video format */
    format = this->pCamera->GetVideoFormat();
    setIntegerParam(FDC_format, format);
    sprintf(str, "%s", videoFormatStrings[format]);
    setStringParam(FDC_current_format, str);

    /* Format all valid video modes for this format */
    for (mode=0; mode<MAX_1394_VIDEO_MODES; mode++) {
        if (this->pCamera->HasVideoMode(format, mode)) {
            sprintf(str, "%d %s", mode, videoModeStrings[format][mode]);
            setIntegerParam(mode, FDC_has_mode, 1);
        } else {
            sprintf(str, "%d N.A.", mode);
            setIntegerParam(mode, FDC_has_mode, 0);
        }
        setStringParam(mode, FDC_valid_mode, str);
    }

    /* Get the current video mode */
    mode = this->pCamera->GetVideoMode();
    setIntegerParam(FDC_mode, mode);
    sprintf(str, "%s", videoModeStrings[format][mode]);
    setStringParam(FDC_current_mode, str);

    /* Format all valid video rates for this format and mode */
    for (rate=0; rate<MAX_1394_FRAME_RATES; rate++) {
        if (this->pCamera->HasVideoFrameRate(format, mode, rate)) {
            sprintf(str, "%d %s", rate, frameRateStrings[rate]);
            setIntegerParam(rate, FDC_has_framerate, 1);
        } else {
            sprintf(str, "%d N.A.", rate);
            setIntegerParam(rate, FDC_has_framerate, 0);
        }
        setStringParam(rate, FDC_valid_framerate, str);
    }

    /* Get the current video rate */
    rate = this->pCamera->GetVideoFrameRate();
    setIntegerParam(FDC_framerate, rate);
    sprintf(str, "%s", frameRateStrings[rate]);
    setStringParam(FDC_current_framerate, str);
    
    /* Format all valid format 7 color modes */
    for (colorCode=0; colorCode<COLOR_CODE_MAX; colorCode++) {
        if (this->pCameraControlSize->HasColorCode((COLOR_CODE)colorCode)) {
            sprintf(str, "%d %s", colorCode, colorCodeStrings[colorCode]);
            setIntegerParam(colorCode, FDC_has_colorcode, 1);
        } else {
            sprintf(str, "%d N.A.", colorCode);
            setIntegerParam(colorCode, FDC_has_colorcode, 0);
        }
        setStringParam(colorCode, FDC_valid_colorcode, str);
    }

    /* Get the current color code */
    this->pCameraControlSize->GetColorCode((COLOR_CODE *)&colorCode);
    setIntegerParam(FDC_colorcode, colorCode);
    sprintf(str, "%s", colorCodeStrings[colorCode]);
    setStringParam(FDC_current_colorcode, str);
    
    maxAddr = MAX(MAX_1394_VIDEO_FORMATS, MAX_1394_VIDEO_MODES);
    maxAddr = MAX(maxAddr, MAX_1394_FRAME_RATES);
    maxAddr = MAX(maxAddr, COLOR_CODE_MAX);
    /* This assumes that the number of formats, modes and rates are all the same */
    for (addr=0; addr<maxAddr; addr++) callParamCallbacks(addr, addr);

    return asynSuccess;
}

asynStatus FirewireWinDCAM::formatFormat7Modes()
{
    int format=7, mode;
    int oldFormat, oldMode, oldRate;
    int err;
    asynStatus status = asynSuccess;
    unsigned short hsMax, vsMax, hsUnit, vsUnit;
    unsigned short hpUnit, vpUnit;
    char str[100];
    const char* functionName="formatFormat7Modes";
 
    /* This function changes the camera to each valid Format 7 mode and
     * inquires about its properties to produce an informative string */

    /* If we don't suport format 7 just return success */
    if (!this->pCamera->HasVideoFormat(7)) return asynSuccess;
    
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
        "%s:%s entry\n",
        driverName, functionName);
    
    /* Remember the current format, mode and rate */
    oldFormat = this->pCamera->GetVideoFormat();
    oldMode = this->pCamera->GetVideoMode();
    oldRate = this->pCamera->GetVideoFrameRate();

    err = this->pCamera->SetVideoFormat(format);
    status = PERR(err );
    if (status == asynError) goto done;
 
    /* Loop over modes */   
    for (mode=0; mode<MAX_1394_VIDEO_MODES; mode++) {
        if (!this->pCamera->HasVideoMode(format, mode)) continue;
        err = this->pCamera->SetVideoMode(mode);
        status = PERR(err);
        if (status == asynError) goto done;
        /* Get the size limits */
        this->pCameraControlSize->GetSizeLimits(&hsMax, &vsMax);
        /* Get the size units (minimum increment) */
        this->pCameraControlSize->GetSizeUnits(&hsUnit, &vsUnit);
        /* Get the position units (minimum increment) */
        this->pCameraControlSize->GetPosUnits(&hpUnit, &vpUnit);
        sprintf(str, "%dx%d (%d,%d)(%d,%d)",
            hsMax, vsMax, hsUnit, vsUnit, hpUnit, vpUnit);
        videoModeStrings[format][mode] = epicsStrDup(str);
    }
    if (oldFormat != -1) {
        err = this->pCamera->SetVideoFormat(oldFormat);
        status = PERR(err);
    }
    if (oldMode != -1) {
        err = this->pCamera->SetVideoMode(oldMode);
        status = PERR(err);
    }
    if ((oldRate != -1) && (oldFormat != 7)) {
        err = this->pCamera->SetVideoFrameRate(oldRate);
        status = PERR(err);
    }

    done:
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
        "%s:%s exit, status=%d\n",
        driverName, functionName, status);
    return status;
}

/** Read all the feature settings and values from the camera.
 * This function will collect all the current values and settings from the camera,
 * and set the appropriate integer/double parameters in the param lib. If a certain feature
 * is not available in the given camera, this function will set all the parameters relating to that
 * feature to -1 or -1.0 to indicate it is not available.
 * Note the caller is responsible for calling any update callbacks if I/O interrupts
 * are to be processed after calling this function.
 * Returns asynStatus asynError or asynSuccess as an int.
 */
asynStatus FirewireWinDCAM::getAllFeatures()
{
    asynStatus status = asynSuccess;
    C1394CameraControl *pFeature;
    int tmp, addr, value;
    unsigned short min, max, lo, hi;
    float fmin, fmax, fvalue;
    double dtmp;
    const char* functionName="getAllFeatures";

    /* Iterate through all of the available features and update their values and settings  */
    for (addr = 0; addr < num1394Features; addr++) {
        pFeature = this->pCameraControl[addr];
        asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
            "%s:%s: checking feature %d\n",
            driverName, functionName, addr);

        /* The Inquire function reads values from the camera into registers */
        pFeature->Inquire();
        /* The Status function reads status and absolute values from the camera into registers */
        pFeature->Status();

        /* If the feature is not available in the camera, we just set
         * all the parameters to -1 to indicate this is not available to the user. */
        if (pFeature->HasPresence()) {
            pFeature->GetRange(&min, &max);
            pFeature->GetValue(&lo, &hi);
            value = lo + (hi << 12);
            setIntegerParam(addr, FDC_feat_available, 1);
            setIntegerParam(addr, FDC_feat_val, value);
            setIntegerParam(addr, FDC_feat_val_min, min);
            /* The max for white balance needs special treatment */
            if (featureIndex[addr] == FEATURE_WHITE_BALANCE) 
                setIntegerParam(addr, FDC_feat_val_max, (((int)max)<<12) + max);
            else
                setIntegerParam(addr, FDC_feat_val_max, max);
            tmp = pFeature->StatusAutoMode();
            setIntegerParam(addr, FDC_feat_mode, tmp);
        } else {
            setIntegerParam(addr, FDC_feat_available, 0);
            tmp = -1;
            dtmp = -1.0;
            setIntegerParam(addr, FDC_feat_val, tmp);
            setIntegerParam(addr, FDC_feat_val_min, tmp);
            setIntegerParam(addr, FDC_feat_val_max, tmp);
            setIntegerParam(addr, FDC_feat_mode, tmp);
        }

        /* If the feature does not support 'absolute' control then we just
         * set all the absolute values to -1.0 to indicate it is not available to the user */
        if (pFeature->HasAbsControl()) { 
            pFeature->GetRangeAbsolute(&fmin, &fmax);
            pFeature->GetValueAbsolute(&fvalue);
            setIntegerParam(addr, FDC_feat_absolute, 1);
            setDoubleParam(addr, FDC_feat_val_abs, fvalue);
            setDoubleParam(addr, FDC_feat_val_abs_min, fmin);
            setDoubleParam(addr, FDC_feat_val_abs_max, fmax);
        } else {
            dtmp = -1.0;
            setIntegerParam(addr, FDC_feat_absolute, 0);
            setDoubleParam(addr, FDC_feat_val_abs, dtmp);
            setDoubleParam(addr, FDC_feat_val_abs_max, dtmp);
            setDoubleParam(addr, FDC_feat_val_abs_min, dtmp);
        }
    }

    /* Finally map a few of the AreaDetector parameters on to the camera 'features' */
    for (addr=0; addr<num1394Features; addr++) {
        if (featureIndex[addr] == FEATURE_SHUTTER) break;
    }
    getDoubleParam(addr, FDC_feat_val_abs, &dtmp);
    setDoubleParam(ADAcquireTime, dtmp);

    for (addr=0; addr<num1394Features; addr++) {
        if (featureIndex[addr] == FEATURE_GAIN) break;
    }
    getDoubleParam(addr, FDC_feat_val_abs, &dtmp);
    setDoubleParam(ADGain, dtmp);
    /* Do callbacks for each feature */
    for (addr = 0; addr < num1394Features; addr++) callParamCallbacks(addr, addr);

    return status;
}



asynStatus FirewireWinDCAM::startCapture()
{
    asynStatus status = asynSuccess;
    int err;
    int msTimeout;
    double timeout;
    const char* functionName = "startCapture";

    getDoubleParam(ADAcquireTime, &timeout);
    /* The timeout for waiting for a frame will be the exposure time plus 1 second margin */
    msTimeout = 1000 * (int)(timeout + 1.0);
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
        "%s::%s [%s] Starting firewire transmission, timeout (ms)=%d\n",
        driverName, functionName, this->portName, msTimeout);
    /* Start the camera transmission... */
    err = this->pCamera->StartImageAcquisitionEx(MAX_1394_BUFFERS, 
                                                msTimeout, ACQ_START_VIDEO_STREAM);
    status = PERR(err);
    if (status == asynError)
    {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
            "%s::%s [%s] starting transmission failed... Staying in idle state.\n",
            driverName, functionName, this->portName);
        setIntegerParam(ADAcquire, 0);
        callParamCallbacks();
        return status;
    }

    /* Signal the image grabbing thread that the acquisition/transmission has
     * started and it can start dequeueing images from the driver buffer */
    epicsEventSignal(this->startEventId);
    return status;
}


asynStatus FirewireWinDCAM::stopCapture()
{
    asynStatus status = asynSuccess;
    int err;
    const char * functionName = "stopCapture";

    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
        "%s::%s [%s] Stopping firewire transmission\n",
        driverName, functionName, this->portName);

    /* Stop the actual transmission! */
    err=this->pCamera->StopImageAcquisition();
    status = PERR(err);
    if (status == asynError)
    {
        /* if stopping transmission results in an error (weird situation!) we print a message
         * but does not abort the function because we still want to set status to stopped...  */
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
            "%s::%s [%s] Stopping transmission failed...\n",
            driverName, functionName, this->portName);
    }

    return status;
}


/** Parse a dc1394 error code into a user readable string
 * Defaults to printing out using the pasynUser.
 * \param asynUser The asyn user to print out with on ASYN_TRACE_ERR. If pasynUser == NULL just print to stderr.
 * \param CAM_err The error code, returned from the dc1394 function call. If the error code is OK we just ignore it.
 * \param errOriginLine Line number where the error came from.
 */
asynStatus FirewireWinDCAM::err(int CAM_err, int errOriginLine)
{
    if (CAM_err == CAM_SUCCESS) return asynSuccess; /* if everything is OK we just ignore it */

    if (this->pasynUserSelf == NULL) fprintf(stderr, 
        "### ERROR port=%s line=%d, error=%d (%s)\n", 
        this->portName, errOriginLine, CAM_err, errMsg[-CAM_err]);
    else asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
        "### ERROR port=%s line=%d, error=%d (%s)\n", 
        this->portName, errOriginLine, CAM_err, errMsg[-CAM_err]);
    return asynError;
}

/** Print out a report; calls ADDriver::report to get base class report as well.
  * \param[in] fp File pointer to write output to
  * \param[in] details Level of detail desired.  If >1 prints information about 
               supported video formats and modes, etc.
 */
void FirewireWinDCAM::report(FILE *fp, int details)
{
    char vendorName[256], cameraName[256];
    LARGE_INTEGER uniqueId;
    int version;
    int format, mode, rate;
    unsigned long sizeX, sizeY;
    unsigned short maxSizeX, maxSizeY;
    int value, feature;
    unsigned short min, max, lo, hi;
    float fmin, fmax, fvalue;
    C1394CameraControl *pFeature;
    
    this->pCamera->GetCameraVendor(vendorName, sizeof(vendorName));
    this->pCamera->GetCameraName(cameraName, sizeof(cameraName));
    this->pCamera->GetCameraUniqueID(&uniqueId);
    this->pCameraControlSize->GetSizeLimits(&maxSizeX, &maxSizeY);
    version = this->pCamera->GetVersion();
    fprintf(fp, "Vendor name: %s\n", vendorName);
    fprintf(fp, "Camera name: %s\n", cameraName);
    fprintf(fp, "UniqueId: %lld (0x%llX)\n", uniqueId.QuadPart, uniqueId.QuadPart);
    fprintf(fp, "Version: 0x%lX\n", version);
    fprintf(fp, "Max size: X=%d, Y=%d\n", maxSizeX, maxSizeY);
    if (details > 1) {
        fprintf(fp, "Supported formats, modes and rates:\n");
        for (format=0; format<=7; format++) {
            if (this->pCamera->HasVideoFormat(format)) {
                fprintf(fp, "Format %d supported (%s)\n", format, videoFormatStrings[format]);
                for (mode=0; mode<=7; mode++) {
                    if (this->pCamera->HasVideoMode(format, mode)) {
                        fprintf(fp, "  Mode %d supported (%s)\n", mode, videoModeStrings[format][mode]);
                        for (rate=0; rate<=7; rate++) {
                            if (this->pCamera->HasVideoFrameRate(format, mode, rate)) {
                                fprintf(fp, "    Rate %d supported (%s)\n", rate, frameRateStrings[rate]);
                            }
                        }
                    }
                }
            }
        }
        /* Iterate through all of the available features and report on them  */
        fprintf(fp, "Supported features\n");
        for (feature = 0; feature < num1394Features; feature++) {
            pFeature = this->pCameraControl[feature];
            /* The Inquire function reads values from the camera into registers */
            pFeature->Inquire();
            if (pFeature->HasPresence()) {
                pFeature->GetRange(&min, &max);
                pFeature->GetValue(&lo, &hi);
                value = lo + (hi << 12);
                fprintf(fp, "Feature %s \n"
                            " min           = %d\n"
                            " max           = %d\n"
                            " value         = %d\n"
                            " hasAutoMode   = %d     status=%d\n" 
                            " hasManualMode = %d\n"
                            " hasOnOff      = %d     status=%d\n"
                            " hasOnePush    = %d     status=%d\n"
                            " hasReadout    = %d\n"
                            " hasAbsControl = %d     status=%d\n",
                    pFeature->GetName(), min, max, value, 
                    pFeature->HasAutoMode(),    pFeature->StatusAutoMode(),
                    pFeature->HasManualMode(),
                    pFeature->HasOnOff(),       pFeature->StatusOnOff(),
                    pFeature->HasOnePush(),     pFeature->StatusOnePush(),
                    pFeature->HasReadout(),
                    pFeature->HasAbsControl(),  pFeature->StatusAbsControl());
                /* If the feature does not support 'absolute' control then we just
                 * set all the absolute values to -1.0 to indicate it is not available to the user */
                if (pFeature->HasAbsControl()) { 
                    pFeature->GetRangeAbsolute(&fmin, &fmax);
                    pFeature->GetValueAbsolute(&fvalue);
                    fprintf(fp, "  units        = %s\n"
                                "  min          = %f\n"
                                "  max          = %f\n" 
                                "  value        = %f\n", 
                        pFeature->GetUnits(), fmin, fmax, fvalue); 
                }
            }
        }
        fprintf(fp, "Has one-shot: %s\n", this->pCamera->HasOneShot() ? "Yes":"No");
        fprintf(fp, "Has multi-shot: %s\n", this->pCamera->HasMultiShot() ? "Yes":"No");
        format = this->pCamera->GetVideoFormat();
        mode = this->pCamera->GetVideoMode();
        rate = this->pCamera->GetVideoFrameRate();
        this->pCamera->GetVideoFrameDimensions(&sizeX, &sizeY);
        fprintf(fp, "Current settings\n");
        fprintf(fp, "  Format: %d (%s)\n", format, videoFormatStrings[format]);
        fprintf(fp, "  Mode: %d (%s)\n", mode, videoModeStrings[format][mode]);
        fprintf(fp, "  Rate: %d (%s)\n", rate, frameRateStrings[rate]);
        fprintf(fp, "  Size: %d %d\n", sizeX, sizeY);
    }
                    
    ADDriver::report(fp, details);
    return;
}

static const iocshArg configArg0 = {"Port name", iocshArgString};
static const iocshArg configArg1 = {"ID", iocshArgString};
static const iocshArg configArg2 = {"maxBuffers", iocshArgInt};
static const iocshArg configArg3 = {"maxMemory", iocshArgInt};
static const iocshArg configArg4 = {"priority", iocshArgInt};
static const iocshArg configArg5 = {"stackSize", iocshArgInt};
static const iocshArg * const configArgs[] = {&configArg0,
                                              &configArg1,
                                              &configArg2,
                                              &configArg3,
                                              &configArg4,
                                              &configArg5};
static const iocshFuncDef configFirewireWinDCAM = {"WinFDC_Config", 6, configArgs};
static void configCallFunc(const iocshArgBuf *args)
{
    WinFDC_Config(args[0].sval, args[1].sval, args[2].ival, 
                  args[3].ival, args[4].ival, args[5].ival);
}


static void firewireWinDCAMRegister(void)
{
    iocshRegister(&configFirewireWinDCAM, configCallFunc);
}

extern "C" {
epicsExportRegistrar(firewireWinDCAMRegister);
}

