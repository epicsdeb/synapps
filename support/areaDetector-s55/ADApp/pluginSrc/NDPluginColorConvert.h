#ifndef NDPluginColorConvert_H
#define NDPluginColorConvert_H

#include <epicsTypes.h>

#include "NDPluginDriver.h"

#define NDPluginColorConvertColorModeOutString   "COLOR_MODE_OUT" /* (NDColorMode_t r/w) Output color mode */

/** Convert NDArrays from one NDColorMode to another.
  * This plugin is as source of NDArray callbacks, passing the (possibly converted) NDArray
  * data to clients that register for callbacks. 
  * The plugin currently supports the following conversions
  * <ul>
  *  <li> Bayer color to RGB1, RGB2 or RGB3 </li>
  *  <li> RGB1 to RGB2 or RGB3 </li> 
  *  <li> RGB2 to RGB1 or RGB3 </li> 
  *  <li> RGB3 to RGB1 or RGB2 </li> 
  * </ul> 
  * If the conversion required by the input color mode and output color mode are not
  * in this supported list then the NDArray is passed on without conversion. */
class NDPluginColorConvert : public NDPluginDriver {
public:
    NDPluginColorConvert(const char *portName, int queueSize, int blockingCallbacks, 
                         const char *NDArrayPort, int NDArrayAddr,
                         int maxBuffers, size_t maxMemory,
                         int priority, int stackSize);

    /* These methods override the virtual methods in the base class */
    void processCallbacks(NDArray *pArray);
protected:
    int NDPluginColorConvertColorModeOut;
    #define FIRST_NDPLUGIN_COLOR_CONVERT_PARAM NDPluginColorConvertColorModeOut
    #define LAST_NDPLUGIN_COLOR_CONVERT_PARAM NDPluginColorConvertColorModeOut
private:
    /* These methods are just for this class */
    template <typename epicsType> void convertColor(NDArray *pArray);
};
#define NUM_NDPLUGIN_COLOR_CONVERT_PARAMS (&LAST_NDPLUGIN_COLOR_CONVERT_PARAM - &FIRST_NDPLUGIN_COLOR_CONVERT_PARAM + 1)

    
#endif
