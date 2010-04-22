/* pvCamSrc.h
 *
 * This is a driver for a PVCam (PI/Acton) detector.
 *
 * Author: Brian Tieman
 *
 * Created:  06/14/2008
 *
 */

#ifndef PVCAMSRC_H
#define PVCAMSRC_H

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

#include "ADDriver.h"

#include "drvPVCam.h"

/* PM FILES */
#include "master.h"
#include "pvcam.h"

//______________________________________________________________________________________________

static const char *driverName = "drvPVCam";

//______________________________________________________________________________________________

/** The polling interval when checking to see if acquisition is complete */
#define POLL_TIME                       0.01

#define NUM_PV_CAM_PARAMS (sizeof(PVCamParamString)/sizeof(PVCamParamString[0]))

#define MAX_DETECTORS_SUPPORTED         3
#define MAX_SPEEDS_SUPPORTED            5

//______________________________________________________________________________________________

#define PVCamInitDetectorString             "PVCAM_INITIALIZE_DETECTOR"
#define PVCamSlot1CamString                 "PVCAM_SLOT1"
#define PVCamSlot2CamString                 "PVCAM_SLOT2"
#define PVCamSlot3CamString                 "PVCAM_SLOT3"
#define PVCamDetectorSelectedString         "PVCAM_DETECTORSELECTED"
#define PVCamChipNameRBVString              "PVCAM_CHIPNAME"
#define PVCamNumParallelPixelsRBVString     "PVCAM_NUMPARALLELPIXELS"
#define PVCamNumSerialPixelsRBVString       "PVCAM_NUMSERIALPIXELS"
#define PVCamPixelParallelSizeRBVString     "PVCAM_PIXELPARALLELSIZE"
#define PVCamPixelSerialSizeRBVString       "PVCAM_PIXELSERIALSIZE"
#define PVCamChipHeightMMRBVString          "PVCAM_CHIPHEIGHT"
#define PVCamChipWidthMMRBVString           "PVCAM_CHIPWIDTH"
#define PVCamPixelParallelDistRBVString     "PVCAM_PIXELPARALLELDIST"
#define PVCamPixelSerialDistRBVString       "PVCAM_PIXELSERIALDIST"
#define PVCamPostMaskRBVString              "PVCAM_POSTMASK"
#define PVCamPreMaskRBVString               "PVCAM_PREMASK"
#define PVCamPostScanRBVString              "PVCAM_POSTSCAN"
#define PVCamPreScanRBVString               "PVCAM_PRESCAN"
#define PVCamNumPortsRBVString              "PVCAM_NUMPORTS"
#define PVCamFullWellCapacityRBVString      "PVCAM_FULLWELLCAPACITY"
#define PVCamFrameTransferCapableRBVString  "PVCAM_FRAMETRANSFERCAPABLE"
#define PVCamNumSpeedTableEntriesRBVString  "PVCAM_NUMSPEEDTABLEENTRIES"
#define PVCamSpeedTableIndexString          "PVCAM_SPEEDTABLEINDEX"
#define PVCamSpeedTableIndexRBVString       "PVCAM_SPEEDTABLEINDEX_RBV"
#define PVCamBitDepthRBVString              "PVCAM_BITDEPTH"
#define PVCamPixelTimeRBVString             "PVCAM_PIXELTIME"
#define PVCamGainIndexString                "PVCAM_GAININDEX"
#define PVCamGainIndexRBVString             "PVCAM_GAININDEX_RBV"
#define PVCamMaxGainIndexRBVString          "PVCAM_MAXGAININDEX"
#define PVCamMinShutterOpenDelayRBVString   "PVCAM_MINSHUTTEROPENDELAY"
#define PVCamMaxShutterOpenDelayRBVString   "PVCAM_MAXSHUTTEROPENDELAY"
#define PVCamMinShutterCloseDelayRBVString  "PVCAM_MINSHUTTERCLOSEDELAY"
#define PVCamMaxShutterCloseDelayRBVString  "PVCAM_MAXSHUTTERCLOSEDELAY"
#define PVCamShutterOpenDelayString         "PVCAM_SHUTTEROPENDELAY"
#define PVCamShutterOpenDelayRBVString      "PVCAM_SHUTTEROPENDELAY_RBV"
#define PVCamShutterCloseDelayString        "PVCAM_SHUTTERCLOSEDELAY"
#define PVCamShutterCloseDelayRBVString     "PVCAM_SHUTTERCLOSEDELAY_RBV"
#define PVCamMeasuredTemperatureRBVString   "PVCAM_MEASUREDTEMPERATURE"
#define PVCamMinTemperatureRBVString        "PVCAM_MINTEMPERATURE"
#define PVCamMaxTemperatureRBVString        "PVCAM_MAXTEMPERATURE"
#define PVCamSetTemperatureString           "PVCAM_SETTEMPERATURE"
#define PVCamSetTemperatureRBVString        "PVCAM_SETTEMPERATURE_RBV"
#define PVCamDetectorModeString             "PVCAM_DETECTORMODE"
#define PVCamDetectorModeRBVString          "PVCAM_DETECTORMODE_RBV"
#define PVCamTriggerModeString              "PVCAM_TRIGGERMODE"
#define PVCamTriggerModeRBVString           "PVCAM_TRIGGERMODE_RBV"
#define PVCamTriggerEdgeString              "PVCAM_TRIGGEREDGE"
#define PVCamTriggerEdgeRBVString           "PVCAM_TRIGGEREDGE_RBV"
#define PVCamCamFirmwareVersRBVString       "PVCAM_CAMFIRMWAREVERS_RBV"
#define PVCamPCIFWVersRBVString             "PVCAM_PCIFWVERS_RBV"
#define PVCamHeadSerialNumRBVString         "PVCAM_HEADSERNUM_RBV"
#define PVCamSerialNumRBVString             "PVCAM_SERIALNUM_RBV"
#define PVCamPVCamVersRBVString 			"PVCAM_PVCAMVERS_RBV"
#define PVCamDevDrvVersRBVString			"PVCAM_DEVDRVVERS_RBV"

//______________________________________________________________________________________________

class pvCam : public ADDriver
{
public:
int                 imagesRemaining;
epicsEventId         startEventId,
                    stopEventId;
NDArray             *pRaw;

    pvCam(const char *portName, int maxSizeX, int maxSizeY, NDDataType_t dataType,
                int maxBuffers, size_t maxMemory, int priority, int stackSize);

    /* These are the methods that we override from ADDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    void report(FILE *fp, int details);

    /* These are the methods that are new to this class */
    template <typename epicsType> int computeArray(int maxSizeX, int maxSizeY);

    int allocateBuffer();
    int computeImage();

    void pvCamAcquisitionTask();
    void pvCamMonitorTask();

    ~pvCam ();

protected:
    int PVCamInitDetector;
    #define FIRST_PVCAM_PARAM PVCamInitDetector
    int PVCamSlot1Cam;
    int PVCamSlot2Cam;
    int PVCamSlot3Cam;
    int PVCamDetectorSelected;
    int PVCamChipNameRBV;
    int PVCamNumParallelPixelsRBV;
    int PVCamNumSerialPixelsRBV;
    int PVCamPixelParallelSizeRBV;
    int PVCamPixelSerialSizeRBV;
    int PVCamChipHeightMMRBV;
    int PVCamChipWidthMMRBV;
    int PVCamPixelParallelDistRBV;
    int PVCamPixelSerialDistRBV;
    int PVCamPostMaskRBV;
    int PVCamPreMaskRBV;
    int PVCamPostScanRBV;
    int PVCamPreScanRBV;
    int PVCamNumPortsRBV;
    int PVCamFullWellCapacityRBV;
    int PVCamFrameTransferCapableRBV;
    int PVCamNumSpeedTableEntriesRBV;
    int PVCamSpeedTableIndex;
    int PVCamSpeedTableIndexRBV;
    int PVCamBitDepthRBV;
    int PVCamPixelTimeRBV;
    int PVCamGainIndex;
    int PVCamGainIndexRBV;
    int PVCamMaxGainIndexRBV;
    int PVCamMinShutterOpenDelayRBV;
    int PVCamMaxShutterOpenDelayRBV;
    int PVCamMinShutterCloseDelayRBV;
    int PVCamMaxShutterCloseDelayRBV;
    int PVCamShutterOpenDelay;
    int PVCamShutterOpenDelayRBV;
    int PVCamShutterCloseDelay;
    int PVCamShutterCloseDelayRBV;
    int PVCamMeasuredTemperatureRBV;
    int PVCamMinTemperatureRBV;
    int PVCamMaxTemperatureRBV;
    int PVCamSetTemperature;
    int PVCamSetTemperatureRBV;
    int PVCamDetectorMode;
    int PVCamDetectorModeRBV;
    int PVCamTriggerMode;
    int PVCamTriggerModeRBV;
    int PVCamTriggerEdge;
    int PVCamTriggerEdgeRBV;
    int PVCamCamFirmwareVersRBV;
    int PVCamPCIFWVersRBV;
	int PVCamHeadSerialNumRBV;
	int PVCamSerialNumRBV;
	int PVCamPVCamVersRBV;
	int PVCamDevDrvVersRBV;
    #define LAST_PVCAM_PARAM PVCamDevDrvVersRBV

private:
int16           numDetectorsInstalled,
                detectorSelected,
                detectorHandle;

char            *detectorList[5];

unsigned short  *rawData;

    void outputErrorMessage (const char *functionName, char *appMessage);

    void initializeDetectorInterface (void);
    void selectDetector (int selectedDetector);

    void queryCurrentSettings (void);

    void initializeDetector (void);

    int getAcquireStatus (void);
	bool tempAvailable;
};

#define NUM_PVCAM_PARAMS (&LAST_PVCAM_PARAM - &FIRST_PVCAM_PARAM + 1)

//______________________________________________________________________________________________

#endif
