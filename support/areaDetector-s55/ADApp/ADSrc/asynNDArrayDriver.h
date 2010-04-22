#ifndef asynNDArrayDriver_H
#define asynNDArrayDriver_H

#include "asynPortDriver.h"
#include "NDArray.h"
#include "PVAttribute.h"


/** Maximum length of a filename or any of its components */
#define MAX_FILENAME_LEN 256

/** Enumeration of file saving modes */
typedef enum {
    NDFileModeSingle,       /**< Write 1 array per file */
    NDFileModeCapture,      /**< Capture NDNumCapture arrays into memory, write them out when capture is complete.
                              *  Write all captured arrays to a single file if the file format supports this */
    NDFileModeStream        /**< Stream arrays continuously to a single file if the file format supports this */
} NDFileMode_t;


/** Strings defining parameters that affect the behaviour of the detector. 
  * These are the values passed to drvUserCreate. 
  * The driver will place in pasynUser->reason an integer to be used when the
  * standard asyn interface methods are called. */
 /*                               String                 asyn interface  access   Description  */
#define NDPortNameSelfString    "PORT_NAME_SELF"    /**< (asynOctet,    r/o) Asyn port name of this driver instance */

    /* Parameters defining characteristics of the array data from the detector.
     * NDArraySizeX and NDArraySizeY are the actual dimensions of the array data,
     * including effects of the region definition and binning */
#define NDArraySizeXString      "ARRAY_SIZE_X"      /**< (asynInt32,    r/o) Size of the array data in the X direction */
#define NDArraySizeYString      "ARRAY_SIZE_Y"      /**< (asynInt32,    r/o) Size of the array data in the Y direction */
#define NDArraySizeZString      "ARRAY_SIZE_Z"      /**< (asynInt32,    r/o) Size of the array data in the Z direction */
#define NDArraySizeString       "ARRAY_SIZE"        /**< (asynInt32,    r/o) Total size of array data in bytes */
#define NDNDimensionsString     "ARRAY_NDIMENSIONS" /**< (asynInt32,    r/o) Number of dimensions in array */
#define NDDimensionsString      "ARRAY_DIMENSIONS"  /**< (asynInt32Array, r/o) Array dimensions */
#define NDDataTypeString        "DATA_TYPE"         /**< (asynInt32,    r/w) Data type (NDDataType_t) */
#define NDColorModeString       "COLOR_MODE"        /**< (asynInt32,    r/w) Color mode (NDColorMode_t) */
#define NDUniqueIdString        "UNIQUE_ID"         /**< (asynInt32,    r/o) Unique ID number of array */
#define NDTimeStampString       "TIME_STAMP"        /**< (asynFloat64,  r/o) Time stamp of array */
#define NDBayerPatternString    "BAYER_PATTERN"     /**< (asynInt32,    r/o) Bayer pattern of array  (from bayerPattern array attribute if present) */

    /* Statistics on number of arrays collected */
#define NDArrayCounterString    "ARRAY_COUNTER"     /**< (asynInt32,    r/w) Number of arrays since last reset */

    /* File name related parameters for saving data.
     * Drivers are not required to implement file saving, but if they do these parameters
     * should be used.
     * The driver will normally combine NDFilePath, NDFileName, and NDFileNumber into
     * a file name that order using the format specification in NDFileTemplate.
     * For example NDFileTemplate might be "%s%s_%d.tif" */
#define NDFilePathString        "FILE_PATH"         /**< (asynOctet,    r/w) The file path */
#define NDFilePathExistsString  "FILE_PATH_EXISTS"  /**< (asynInt32,    r/w) File path exists? */
#define NDFileNameString        "FILE_NAME"         /**< (asynOctet,    r/w) The file name */
#define NDFileNumberString      "FILE_NUMBER"       /**< (asynInt32,    r/w) The next file number */
#define NDFileTemplateString    "FILE_TEMPLATE"     /**< (asynOctet,    r/w) The file format template; C-style format string */
#define NDAutoIncrementString   "AUTO_INCREMENT"    /**< (asynInt32,    r/w) Autoincrement file number; 0=No, 1=Yes */
#define NDFullFileNameString    "FULL_FILE_NAME"    /**< (asynOctet,    r/o) The actual complete file name for the last file saved */
#define NDFileFormatString      "FILE_FORMAT"       /**< (asynInt32,    r/w) The data format to use for saving the file.  */
#define NDAutoSaveString        "AUTO_SAVE"         /**< (asynInt32,    r/w) Automatically save files */
#define NDWriteFileString       "WRITE_FILE"        /**< (asynInt32,    r/w) Manually save the most recent array to a file when value=1 */
#define NDReadFileString        "READ_FILE"         /**< (asynInt32,    r/w) Manually read file when value=1 */
#define NDFileWriteModeString   "WRITE_MODE"        /**< (asynInt32,    r/w) File saving mode (NDFileMode_t) */
#define NDFileNumCaptureString  "NUM_CAPTURE"       /**< (asynInt32,    r/w) Number of arrays to capture */
#define NDFileNumCapturedString "NUM_CAPTURED"      /**< (asynInt32,    r/o) Number of arrays already captured */
#define NDFileCaptureString     "CAPTURE"           /**< (asynInt32,    r/w) Start or stop capturing arrays */

#define NDAttributesFileString  "ND_ATTRIBUTES_FILE" /**< (asynOctet,    r/w) Attributes file name */

    /* The detector array data */
#define NDArrayDataString       "ARRAY_DATA"        /**< (asynGenericPointer,   r/w) NDArray data */
#define NDArrayCallbacksString  "ARRAY_CALLBACKS"   /**< (asynInt32,    r/w) Do callbacks with array data (0=No, 1=Yes) */

/** This is the class from which NDArray drivers are derived; implements the asynGenericPointer functions 
  * for NDArray objects. 
  * For areaDetector, both plugins and detector drivers are indirectly derived from this class.
  * asynNDArrayDriver inherits from asynPortDriver.
  */
class asynNDArrayDriver : public asynPortDriver {
public:
    asynNDArrayDriver(const char *portName, int maxAddr, int numParams, int maxBuffers, size_t maxMemory,
                      int interfaceMask, int interruptMask,
                      int asynFlags, int autoConnect, int priority, int stackSize);

    /* These are the methods that we override from asynPortDriver */
    virtual asynStatus writeOctet(asynUser *pasynUser, const char *value, size_t maxChars,
                          size_t *nActual);
    virtual asynStatus readGenericPointer(asynUser *pasynUser, void *genericPointer);
    virtual asynStatus writeGenericPointer(asynUser *pasynUser, void *genericPointer);
    virtual void report(FILE *fp, int details);

    /* These are the methods that are new to this class */
    virtual int checkPath();
    virtual int createFileName(int maxChars, char *fullFileName);
    virtual int createFileName(int maxChars, char *filePath, char *fileName);
    virtual int readNDAttributesFile(const char *fileName);
    virtual int getAttributes(NDAttributeList *pAttributeList);

protected:
    int NDPortNameSelf;
    #define FIRST_NDARRAY_PARAM NDPortNameSelf
    int NDArraySizeX;
    int NDArraySizeY;
    int NDArraySizeZ;
    int NDArraySize;
    int NDNDimensions;
    int NDDimensions;
    int NDDataType;
    int NDColorMode;
    int NDUniqueId;
    int NDTimeStamp;
    int NDBayerPattern;
    int NDArrayCounter;
    int NDFilePath;
    int NDFilePathExists;
    int NDFileName;
    int NDFileNumber;
    int NDFileTemplate;
    int NDAutoIncrement;
    int NDFullFileName;
    int NDFileFormat;
    int NDAutoSave;
    int NDWriteFile;
    int NDReadFile;
    int NDFileWriteMode;
    int NDFileNumCapture;
    int NDFileNumCaptured;
    int NDFileCapture;   
    int NDAttributesFile;
    int NDArrayData;
    int NDArrayCallbacks;
    #define LAST_NDARRAY_PARAM NDArrayCallbacks

    NDArray **pArrays;             /**< An array of NDArray pointers used to store data in the driver */
    NDArrayPool *pNDArrayPool;     /**< An NDArrayPool object used to allocate and manipulate NDArray objects */
    class NDAttributeList *pAttributeList;  /**< An NDAttributeList object used to obtain the current values of a set of
                                          *  attributes */
};

#define NUM_NDARRAY_PARAMS (&LAST_NDARRAY_PARAM - &FIRST_NDARRAY_PARAM + 1)
#endif
