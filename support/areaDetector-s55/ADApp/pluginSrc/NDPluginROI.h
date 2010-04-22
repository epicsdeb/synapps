#ifndef NDPluginROI_H
#define NDPluginROI_H

#include <epicsTypes.h>
#include <asynStandardInterfaces.h>

#include "NDPluginDriver.h"

/* ROI general parameters */
#define NDPluginROINameString               "NAME"                /* (asynOctet,   r/w) Name of this ROI */

/* ROI definition */
#define NDPluginROIDim0MinString            "DIM0_MIN"          /* (asynInt32,   r/w) Starting element of ROI in each dimension */
#define NDPluginROIDim0SizeString           "DIM0_SIZE"         /* (asynInt32,   r/w) Size of ROI in each dimension */
#define NDPluginROIDim0MaxSizeString        "DIM0_MAX_SIZE"     /* (asynInt32,   r/o) Maximum size of ROI in each dimension */
#define NDPluginROIDim0BinString            "DIM0_BIN"          /* (asynInt32,   r/w) Binning of ROI in each dimension */
#define NDPluginROIDim0ReverseString        "DIM0_REVERSE"      /* (asynInt32,   r/w) Reversal of ROI in each dimension */
#define NDPluginROIDim1MinString            "DIM1_MIN"          /* (asynInt32,   r/w) Starting element of ROI in each dimension */
#define NDPluginROIDim1SizeString           "DIM1_SIZE"         /* (asynInt32,   r/w) Size of ROI in each dimension */
#define NDPluginROIDim1MaxSizeString        "DIM1_MAX_SIZE"     /* (asynInt32,   r/o) Maximum size of ROI in each dimension */
#define NDPluginROIDim1BinString            "DIM1_BIN"          /* (asynInt32,   r/w) Binning of ROI in each dimension */
#define NDPluginROIDim1ReverseString        "DIM1_REVERSE"      /* (asynInt32,   r/w) Reversal of ROI in each dimension */
#define NDPluginROIDim2MinString            "DIM2_MIN"          /* (asynInt32,   r/w) Starting element of ROI in each dimension */
#define NDPluginROIDim2SizeString           "DIM2_SIZE"         /* (asynInt32,   r/w) Size of ROI in each dimension */
#define NDPluginROIDim2MaxSizeString        "DIM2_MAX_SIZE"     /* (asynInt32,   r/o) Maximum size of ROI in each dimension */
#define NDPluginROIDim2BinString            "DIM2_BIN"          /* (asynInt32,   r/w) Binning of ROI in each dimension */
#define NDPluginROIDim2ReverseString        "DIM2_REVERSE"      /* (asynInt32,   r/w) Reversal of ROI in each dimension */
#define NDPluginROIDataTypeString           "ROI_DATA_TYPE"     /* (asynInt32,   r/w) Data type for ROI.  -1 means automatic. */
#define NDPluginROIEnableScaleString        "ENABLE_SCALE"      /* (asynInt32,   r/w) Disable/Enable scaling */
#define NDPluginROIScaleString              "SCALE_VALUE"       /* (asynFloat64, r/w) Scaling value, used as divisor */

/** Extract Regions-Of-Interest (ROI) from NDArray data; the plugin can be a source of NDArray callbacks for
  * other plugins, passing these sub-arrays. 
  * The plugin also optionally computes a statistics on the ROI. */
class NDPluginROI : public NDPluginDriver {
public:
    NDPluginROI(const char *portName, int queueSize, int blockingCallbacks, 
                 const char *NDArrayPort, int NDArrayAddr,
                 int maxBuffers, size_t maxMemory,
                 int priority, int stackSize);
    /* These methods override the virtual methods in the base class */
    void processCallbacks(NDArray *pArray);

protected:
    /* ROI general parameters */
    int NDPluginROIName;
    #define FIRST_NDPLUGIN_ROI_PARAM NDPluginROIName

    /* ROI definition */
    int NDPluginROIDim0Min;
    int NDPluginROIDim0Size;
    int NDPluginROIDim0MaxSize;
    int NDPluginROIDim0Bin;
    int NDPluginROIDim0Reverse;
    int NDPluginROIDim1Min;
    int NDPluginROIDim1Size;
    int NDPluginROIDim1MaxSize;
    int NDPluginROIDim1Bin;
    int NDPluginROIDim1Reverse;
    int NDPluginROIDim2Min;
    int NDPluginROIDim2Size;
    int NDPluginROIDim2MaxSize;
    int NDPluginROIDim2Bin;
    int NDPluginROIDim2Reverse;
    int NDPluginROIDataType;
    int NDPluginROIEnableScale;
    int NDPluginROIScale;

    #define LAST_NDPLUGIN_ROI_PARAM NDPluginROIScale
                                
private:
};
#define NUM_NDPLUGIN_ROI_PARAMS (&LAST_NDPLUGIN_ROI_PARAM - &FIRST_NDPLUGIN_ROI_PARAM + 1)
    
#endif
