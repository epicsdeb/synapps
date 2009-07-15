/* File:    pilatusROIs.st
 * Author:  Mark Rivers
 * Date:    23-May-2007
 *
 * Purpose: 
 * This SNL program provides the logic to compute net and total counts in 
 * spatial ROIs on the Pilatus detector and put the results in to EPICS PVs.
 *
 * It works in EXPOSURE mode to take individual images.  The ROI counts are stored
 * in ai records as doubles.
 *
 * It also works to take a series of images.  The results are stored in 
 * waveform records.  The update rate of the waveform records is controlled by a PV.
 *
 * It posts monitors on the image data itself if enabled by PV.  
 * That update rate is also controlled by a PV.
 *
 */

program pilatusROIs("DET=PILATUS1:, PORT=pilatus1, NROIS=32")

/* System includes */
%% #include <sys/types.h>
%% #include <sys/stat.h>
%% #include <string.h>
#ifdef _WIN32
%%#include <io.h>
#else
%%#include <unistd.h>
#endif
%% #include <fcntl.h>
%% #include <ctype.h>
%% #include <time.h>
%% #include <errno.h>

/* EPICS includes */
%%#include <errlog.h>
%%#include <epicsEvent.h>
%%#include <epicsTypes.h>
%%#include <epicsTime.h>
%%#include <epicsThread.h>
%%#include <epicsString.h>
%%#include <asynDriver.h>
%%#include <asynOctetSyncIO.h>

#define TIFF_HEADER_SIZE 4096
#define MAX_MESSAGE_SIZE 256 /* Messages to/from camserver */
#define MAX_FILENAME_LEN 256
#define MAX_PVNAME_SIZE 40
#define MAX_BAD_PIXELS 100
#define MAX_ROIS 32
#define MAX_CHANS 2000
#define MAX_PIXELS 94965
#define DRIVER_NAME "pilatusROIs"
#define MAX_READ_ERRORS 3
#define MAX_COUNTS 1048575;

/* Time to poll when reading from camserver */
#define ASYN_POLL_TIME .01 
/* Time between checking to see if TIFF file is complete */
#define FILE_READ_DELAY .01

/* The following definitions must agree with the AcquireMode record */
#define INTERNAL_EXPOSURE          0
#define EXTERNAL_ENABLE            1
#define EXTERNAL_TRIGGER           2    
#define MULTIPLE_EXTERNAL_TRIGGER  3   
#define ALIGNMENT                  4    

%% typedef struct {
%%    int badIndex;
%%    int replaceIndex;
%% } badPixel;

%% static badPixel badPixelMap[MAX_BAD_PIXELS];

/* Define escaped C functions at end of file */
%% static int asynConnect();
%% static int asynWrite(double timeout);
%% static int asynRead(double timeout);
%% static int readTIFF(char *fileName, double timeout);
%% static void computeROIs();
%% static void postImage();
%% static int validROI(int roi);
%% static void computeRectangleCounts(int xmin, int xmax, int ymin, int ymax,
%%                                    int *nPixels, double *counts, 
%%                                    int *minCounts, int *maxCounts);
%% static void pilatusErase();
%% static void readBadPixelFile();
%% static void readFlatFieldFile();
%% static void makeMultipleFileFormat();
%% static int swapInt32(int in);


/* PVs */
int     ROIXMin[MAX_ROIS]; 
assign  ROIXMin to {"","","","","","","","","","","","","","","","",
                    "","","","","","","","","","","","","","","",""};
monitor ROIXMin;

int     ROIXMax[MAX_ROIS]; 
assign  ROIXMax to {"","","","","","","","","","","","","","","","",
                    "","","","","","","","","","","","","","","",""};
monitor ROIXMax;

int     ROIYMin[MAX_ROIS]; 
assign  ROIYMin to {"","","","","","","","","","","","","","","","",
                    "","","","","","","","","","","","","","","",""};
monitor ROIYMin;

int     ROIYMax[MAX_ROIS]; 
assign  ROIYMax to {"","","","","","","","","","","","","","","","",
                    "","","","","","","","","","","","","","","",""};
monitor ROIYMax;

long    ROIBgdWidth[MAX_ROIS]; 
assign  ROIBgdWidth to {"","","","","","","","","","","","","","","","",
                    "","","","","","","","","","","","","","","",""};
monitor ROIBgdWidth;

string  ROILabel[MAX_ROIS]; 
assign  ROILabel to {"","","","","","","","","","","","","","","","",
                     "","","","","","","","","","","","","","","",""};

double  ROITotalCounts[MAX_ROIS]; 
assign  ROITotalCounts to {"","","","","","","","","","","","","","","","",
                    "","","","","","","","","","","","","","","",""};

double  ROINetCounts[MAX_ROIS]; 
assign  ROINetCounts to {"","","","","","","","","","","","","","","","",
                    "","","","","","","","","","","","","","","",""};
                    
int     ROIMaxCounts[MAX_ROIS]; 
assign  ROIMaxCounts to {"","","","","","","","","","","","","","","","",
                    "","","","","","","","","","","","","","","",""};
                    
int     ROIMinCounts[MAX_ROIS]; 
assign  ROIMinCounts to {"","","","","","","","","","","","","","","","",
                    "","","","","","","","","","","","","","","",""};
                    
double  ROIWFTotalCounts[MAX_ROIS][MAX_CHANS]; 
assign  ROIWFTotalCounts to {"","","","","","","","","","","","","","","","",
                             "","","","","","","","","","","","","","","",""};

double  ROIWFNetCounts[MAX_ROIS][MAX_CHANS]; 
assign  ROIWFNetCounts to {"","","","","","","","","","","","","","","","",
                           "","","","","","","","","","","","","","","",""};
                                     

int     Acquire;            assign  Acquire         to "{DET}Acquire";
                            monitor Acquire;
evflag  Acquire_mon;        sync    Acquire         Acquire_mon;

int     Armed;              assign  Armed           to "{DET}Armed";

int     Abort;              assign  Abort           to "{DET}Abort";
                            monitor Abort;
evflag  Abort_mon;          sync    Abort           Abort_mon;

int     AcquireMode;        assign  AcquireMode     to "{DET}AcquireMode";
                            monitor AcquireMode;
evflag  AcquireMode_mon;    sync    AcquireMode     AcquireMode_mon;

int     ClearROIs;          assign  ClearROIs       to "{DET}ClearROIs";
                            monitor ClearROIs;
evflag  ClearROIs_mon;      sync    ClearROIs       ClearROIs_mon;

double  ExposureTime;       assign  ExposureTime    to "{DET}ExposureTime";
                            monitor ExposureTime;
int     NImages;            assign  NImages         to "{DET}NImages";
                            monitor NImages;
double  ExposurePeriod;     assign  ExposurePeriod  to "{DET}ExposurePeriod";
                            monitor ExposurePeriod;
int     NExposures;         assign  NExposures      to "{DET}NExposures";
                            monitor NExposures;
double  DelayTime;          assign  DelayTime       to "{DET}DelayTime";
                            monitor DelayTime;
double  ThresholdEnergy;    assign  ThresholdEnergy to "{DET}ThresholdEnergy";
                            monitor ThresholdEnergy;
evflag  ThresholdEnergy_mon; sync   ThresholdEnergy ThresholdEnergy_mon;
int     Gain;               assign  Gain            to "{DET}Gain";
                            monitor Gain;
evflag  Gain_mon;           sync    Gain            Gain_mon;
char    FilePath[MAX_FILENAME_LEN]; 
                            assign  FilePath        to "{DET}FilePath";
                            monitor FilePath;
char    FullFilename[MAX_FILENAME_LEN]; 
                            assign  FullFilename    to "{DET}FullFilename";
char    BadPixelFile[MAX_FILENAME_LEN]; 
                            assign  BadPixelFile    to "{DET}BadPixelFile";
                            monitor BadPixelFile;
evflag  BadPixelFile_mon;   sync    BadPixelFile    BadPixelFile_mon;
int     NBadPixels;         assign  NBadPixels      to "{DET}NBadPixels";
int     FlatFieldValid;     assign  FlatFieldValid  to "{DET}FlatFieldValid";
int     MinFlatField;       assign  MinFlatField    to "{DET}MinFlatField";
                            monitor MinFlatField;
evflag  MinFlatField_mon;   sync    MinFlatField    MinFlatField_mon;
char    FlatFieldFile[MAX_FILENAME_LEN]; 
                            assign  FlatFieldFile   to "{DET}FlatFieldFile";
                            monitor FlatFieldFile;
evflag  FlatFieldFile_mon;  sync    FlatFieldFile   FlatFieldFile_mon;
string  Filename;           assign  Filename        to "{DET}Filename";
                            monitor Filename;
string  FileFormat;         assign  FileFormat      to "{DET}FileFormat";
                            monitor FileFormat;
int     FileNumber;         assign  FileNumber      to "{DET}FileNumber";
                            monitor FileNumber;
int     AutoIncrement;      assign  AutoIncrement   to "{DET}AutoIncrement";
                            monitor AutoIncrement;
double  ReadTiffTimeout;    assign  ReadTiffTimeout to "{DET}ReadTiffTimeout";
                            monitor ReadTiffTimeout; 
double  MinWFUpdateTime;    assign  MinWFUpdateTime to "{DET}MinWFUpdateTime";
                            monitor MinWFUpdateTime; 
double  MinImageUpdateTime; assign  MinImageUpdateTime to "{DET}MinImageUpdateTime";
                            monitor MinImageUpdateTime; 
int     NXPixels;           assign  NXPixels        to "{DET}NXPixels";
                            monitor NXPixels;
int     NYPixels;           assign  NYPixels        to "{DET}NYPixels";
                            monitor NYPixels;
int     PostImages;         assign  PostImages      to "{DET}PostImages";
                            monitor PostImages;
int     HighlightROIs;      assign  HighlightROIs   to "{DET}HighlightROIs";
                            monitor HighlightROIs;
int     ImageData[MAX_PIXELS]; assign  ImageData    to "{DET}ImageData";
string  StatusMessage;      assign StatusMessage    to "{DET}StatusMessage";
char    SendMessage[MAX_MESSAGE_SIZE];              
                            assign SendMessage to "{DET}SendMessage";
char    ReplyMessage[MAX_MESSAGE_SIZE];             
                            assign ReplyMessage to "{DET}ReplyMessage";
%% time_t acqStartTime;

/* Variables */
char    *det;
char    *port;
int     nROIs;
int     sendMessageIndex;
int     replyMessageIndex;
int     ROITotalCountsIndex[MAX_ROIS];
int     ROINetCountsIndex[MAX_ROIS];
int     ROIMinCountsIndex[MAX_ROIS];
int     ROIMaxCountsIndex[MAX_ROIS];
int     ROIWFTotalCountsIndex[MAX_ROIS];
int     ROIWFNetCountsIndex[MAX_ROIS];
int     imageDataIndex;
double  averageFlatField;
int     flatField[MAX_PIXELS];
int     readErrors;
int     status;
int     i;
int     roi;
char    pvName[MAX_PVNAME_SIZE];
string  gainStrings[4];
/* The variables beginning with "mf" are "multiple file" variables */
char    mfFormat[MAX_FILENAME_LEN];
int     mfNumber;     /* This is the number in the filename - may not start at 0 */
int     mfNextImage;  /* This is the next image number, starting at 0 */

/*char   *abortEvent;   */ /* Really epicsEventId */

%% asynUser  *pasynUser;
%% SS_ID ssid;
%% epicsTimeStamp lastWFPostTime;
%% epicsTimeStamp lastImagePostTime;

ss main {
    state init {
        when() {
            %%ssid = ssId;
            strcpy(gainStrings[0], "lowG");
            strcpy(gainStrings[1], "midG");
            strcpy(gainStrings[2], "highG");
            strcpy(gainStrings[3], "uhighG");
            det = macValueGet("DET");
            port = macValueGet("PORT");
            nROIs = atoi(macValueGet("NROIS"));
            status = asynConnect();
            if (status) {
                printf("%s: ERROR unable to connect to camserver\n",
                    DRIVER_NAME);
                epicsThreadSuspendSelf();
            }
            for (i=0; i<nROIs; i++) {
                roi = i+1;
                epicsSnprintf(pvName, sizeof(pvName), "%sROI%dXMin", det, roi);
                pvAssign(ROIXMin[i], pvName);
                epicsSnprintf(pvName, sizeof(pvName), "%sROI%dXMax", det, roi);
                pvAssign(ROIXMax[i], pvName);
                epicsSnprintf(pvName, sizeof(pvName), "%sROI%dYMin", det, roi);
                pvAssign(ROIYMin[i], pvName);
                epicsSnprintf(pvName, sizeof(pvName), "%sROI%dYMax", det, roi);
                pvAssign(ROIYMax[i], pvName);
                epicsSnprintf(pvName, sizeof(pvName), "%sROI%dBgdWidth", det, roi);
                pvAssign(ROIBgdWidth[i], pvName);
                epicsSnprintf(pvName, sizeof(pvName), "%sROI%dLabel", det, roi);
                pvAssign(ROILabel[i], pvName);
                epicsSnprintf(pvName, sizeof(pvName), "%sROI%dTotalCounts", det, roi);
                pvAssign(ROITotalCounts[i], pvName);
                epicsSnprintf(pvName, sizeof(pvName), "%sROI%dNetCounts", det, roi);
                pvAssign(ROINetCounts[i], pvName);
                epicsSnprintf(pvName, sizeof(pvName), "%sROI%dMaxCounts", det, roi);
                pvAssign(ROIMaxCounts[i], pvName);
                epicsSnprintf(pvName, sizeof(pvName), "%sROI%dMinCounts", det, roi);
                pvAssign(ROIMinCounts[i], pvName);
                epicsSnprintf(pvName, sizeof(pvName), "%sROI%dWFTotalCounts", det, roi);
                pvAssign(ROIWFTotalCounts[i], pvName);
                epicsSnprintf(pvName, sizeof(pvName), "%sROI%dWFNetCounts", det, roi);
                pvAssign(ROIWFNetCounts[i], pvName);
                ROITotalCountsIndex[i] = pvIndex(ROITotalCounts[i]);
                ROINetCountsIndex[i] = pvIndex(ROINetCounts[i]);
                ROIMinCountsIndex[i] = pvIndex(ROIMinCounts[i]);
                ROIMaxCountsIndex[i] = pvIndex(ROIMaxCounts[i]);
                ROIWFTotalCountsIndex[i] = pvIndex(ROIWFTotalCounts[i]);
                ROIWFNetCountsIndex[i] = pvIndex(ROIWFNetCounts[i]);
            }
            sendMessageIndex = pvIndex(SendMessage);
            replyMessageIndex = pvIndex(ReplyMessage);
            imageDataIndex = pvIndex(ImageData);

            /* Set the event flags that will cause initial processing */            
            efSet(AcquireMode_mon);
            efSet(Gain_mon);
            efClear(ClearROIs_mon);
           
        } state waitConnected
    }

    state waitConnected {
        entry {
            strcpy(StatusMessage, "Waiting for PVs to connect"); pvPut(StatusMessage);
        }
        when (pvAssignCount() == pvConnectCount()) {
            printf ("pilatusROIs: All channels connected.\n");
            /* Force the bad pixel file to be read before the flat field */
            readBadPixelFile();
            readFlatFieldFile();
            pvPut(NBadPixels);
            pvPut(FlatFieldValid);
            /* Clear event flags to avoid reading files again */
            efClear(BadPixelFile_mon);
            efClear(FlatFieldFile_mon);
            efClear(MinFlatField_mon);
        } state monitorChanges
    }
    
    state monitorChanges {
        entry {
            strcpy(StatusMessage, "Waiting for command"); pvPut(StatusMessage);
        }

        /* New value of Gain or ThresholdEnergy */
        when(efTestAndClear(Gain_mon) || efTestAndClear(ThresholdEnergy_mon)) {
            epicsSnprintf(SendMessage, sizeof(SendMessage),
                         "SetThreshold %s %f", gainStrings[Gain], ThresholdEnergy);
            asynWrite(1.0);
            asynRead(3.0);  /* This command can take a few seconds */
        } state monitorChanges

        /* New value of bad pixel file name */
        when(efTestAndClear(BadPixelFile_mon)) {
            readBadPixelFile();
            pvPut(NBadPixels);
        } state monitorChanges

        /* New value of flat field file name or minimum flag field */
        when(efTestAndClear(FlatFieldFile_mon) ||
             efTestAndClear(MinFlatField_mon)) {
            readFlatFieldFile();
            pvPut(FlatFieldValid);
        } state monitorChanges

       when(efTestAndClear(AcquireMode_mon)) {
            /* When we change modes download all exposure parameters, since some modes
             * replace values with new parameters */
            if (AcquireMode == ALIGNMENT) {
                NImages = 1;
            }
            /* nexpf > 1 is only supported in External Enable mode */  
            if (AcquireMode != EXTERNAL_ENABLE) {
                NExposures = 1;
            }  
            /* We do these puts with ca_put_callback so that we wait for camserver to
             * reply to them */
            pvPut(NImages, SYNC);
            pvPut(ExposureTime, SYNC);
            pvPut(ExposurePeriod, SYNC);
            pvPut(DelayTime, SYNC);
            pvPut(NExposures, SYNC);
        } state monitorChanges

        when(efTestAndClear(ClearROIs_mon)) {
            for (i=0; i<nROIs; i++) {
                ROIXMin[i]=-1; pvPut(ROIXMin[i]);
                ROIXMax[i]=-1; pvPut(ROIXMax[i]);
                ROIYMin[i]=-1; pvPut(ROIYMin[i]);
                ROIYMax[i]=-1; pvPut(ROIYMax[i]);
                ROIBgdWidth[i]=1; pvPut(ROIBgdWidth[i]);
                strcpy(ROILabel[i], ""); pvPut(ROILabel[i]);
            }
        } state monitorChanges

        when(efTestAndClear(Acquire_mon)  && (Acquire==1)) {
            strcpy(StatusMessage, "Starting exposure"); pvPut(StatusMessage);
            /* Get the time that acquisition started.  This is used to make sure
             * TIFF files are new, not stale */
            %% acqStartTime = time(NULL);
            /* Acquire data */
            /* Build the full file name */
            epicsSnprintf(FullFilename, sizeof(FullFilename), FileFormat, 
                          FilePath, Filename, FileNumber);
            pvPut(FullFilename);
            if (AcquireMode==INTERNAL_EXPOSURE) {
                epicsSnprintf(SendMessage, sizeof(SendMessage),
                    "exposure %s", FullFilename);
            }
            else if (AcquireMode==EXTERNAL_ENABLE) {
                epicsSnprintf(SendMessage, sizeof(SendMessage),
                    "ExtEnable %s", FullFilename);
            }            
            else if (AcquireMode==EXTERNAL_TRIGGER) {
                epicsSnprintf(SendMessage, sizeof(SendMessage),
                    "ExtTrigger %s", FullFilename);
            }            
            else if (AcquireMode==MULTIPLE_EXTERNAL_TRIGGER) {
                epicsSnprintf(SendMessage, sizeof(SendMessage),
                    "ExtMTrigger %s", FullFilename);
            }            
            else if (AcquireMode==ALIGNMENT) {
                epicsSnprintf(FullFilename, sizeof(FullFilename), "%salignment.tif", 
                              FilePath);
                pvPut(FullFilename);
                epicsSnprintf(SendMessage, sizeof(SendMessage),
                    "exposure %s", FullFilename);
            }
            status = asynWrite(1.0);

            /* Wait for the "15 OK" response from the controller.  
             * This now comes when the detector is "armed" and ready for trigger pulses. */
            status = asynRead(2.0);
            if (status) printf("Timeout waiting for 15 OK reply from camserver\n");
            Armed = 1; pvPut(Armed);

            /* Reset ROI PVs */
            pilatusErase();
            makeMultipleFileFormat();
            status = 0;  /* Clear error status */
        } state collectImages
    }
    
    
    state collectImages {
        when (Acquire == 0) {
            /* We are done collecting images. 
             * Could be because sequence is complete, because user ended alignment mode, 
             * or Abort */
             pvPut(Acquire);
         } state monitorChanges
        
        when ((AcquireMode == ALIGNMENT) && (Acquire==1)) {
            /* Wait for the "7 OK" response from the controller.  
             * This comes when the exposure is done */
            strcpy(StatusMessage, "Waiting for exposure"); pvPut(StatusMessage);
            status = asynRead(ExposureTime + ReadTiffTimeout);
            strcpy(StatusMessage, "Reading TIFF file"); pvPut(StatusMessage);
            status = readTIFF(FullFilename, ReadTiffTimeout);
            if (status) {
                printf("%s: readTIFF, error reading file\n", DRIVER_NAME);
            }
            computeROIs();
            /* Repeat exposure until Acquire=0 */
            status = asynWrite(1.0);
            /* Wait for the "15 OK" response from the controller.  
             * This should come immediately. */
            status = asynRead(1.0);
            /* Reset ROI PVs */
            pilatusErase();            
        } state collectImages
       
        when((NImages==1)) { /* We could put polling here */
            /* Wait for the "7 OK" response from the controller.  
             * This comes when the exposure is done */
            strcpy(StatusMessage, "Waiting for exposure"); pvPut(StatusMessage);
            status = asynRead(ExposureTime + ReadTiffTimeout);
            strcpy(StatusMessage, "Reading TIFF file"); pvPut(StatusMessage);
            status = readTIFF(FullFilename, ReadTiffTimeout);
            if (status) {
                printf("%s: readTIFF, error reading file\n", DRIVER_NAME);
            }
            computeROIs();
            Acquire = 0;
            Armed = 0; pvPut(Armed);
            if (AutoIncrement) {
                FileNumber++;  pvPut(FileNumber);
            }
        } state collectImages
   
        when((status == 0) && (NImages>1) && (mfNextImage < NImages)) {
            /* Build the name of the next TIFF file to read. */
            strcpy(StatusMessage, "Reading TIFF file"); pvPut(StatusMessage);
            epicsSnprintf(FullFilename, sizeof(FullFilename), mfFormat, 
                          mfNumber);
            pvPut(FullFilename);
            /* Read the next TIFF file */
            status = readTIFF(FullFilename, ReadTiffTimeout);
            if (status) {
                readErrors++;
                if (readErrors < MAX_READ_ERRORS) {
                    /* Try again, this could be a missed file */
                    status = 0;
                } else break;
            }
            computeROIs();
            mfNextImage++;
            mfNumber++;
        } state collectImages
        
        when((status != 0) || (mfNextImage == NImages)) {
            /* Read the final message (7 OK) is the final message about the last file. 
             * For some reason this can take a long time to arrive. */
            strcpy(StatusMessage, "Waiting for camserver completion"); pvPut(StatusMessage);
            status = asynRead(ReadTiffTimeout);
            if (status) printf("Timeout waiting for 7 OK reply from camserver\n");
            Acquire = 0;
            Armed = 0; pvPut(Armed);
            if (AutoIncrement) {
                FileNumber++;  pvPut(FileNumber);
            }
        } state collectImages
    }
}


/* This state set simply monitors the abort input.  It is a separate state set
 * so that it is always active, no matter what the state of the main
 * state set. */
ss pilatusROIAbort {
    state monitorAbort {
        when ((efTestAndClear(Abort_mon)) && (Abort==1)) {
            if (Acquire) {
                Acquire = 0; pvPut(Acquire);
                Armed = 0; pvPut(Armed);
                strcpy(SendMessage, "K");
                asynWrite(1.0);
            }    
            /* Clear abort command, post. */
            Abort=0; pvPut(Abort);
        } state monitorAbort
    }
}



/* Begin C code functions */
%{

static int asynConnect()
{
   asynStatus status;
   status = pasynOctetSyncIO->connect(port, 0, &pasynUser, NULL);
   if (status != asynSuccess) return(status);
   return(status);

}

static int asynWrite(double timeout)
{
    int nwrite;
    asynStatus status;

    /* Flush any stale input, since the next operation is likely to be a read */
    status = pasynOctetSyncIO->flush(pasynUser);
    status = pasynOctetSyncIO->write(pasynUser, SendMessage,
                                     strlen(SendMessage), timeout,
                                     &nwrite);
                                        
    if (status) asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "%s:asynWrite, status=%d, sent\n%s\n",
              DRIVER_NAME, status, SendMessage);

    /* Post output string so user can see it */
    seq_pvPut(ssid, sendMessageIndex, 0);
    
    return(status);
}


static int asynRead(double timeout)
{
    int nread;
    asynStatus status=asynSuccess;
    int eomReason;
    epicsTimeStamp tStart, tCheck;
    double deltaTime;

    /* We implement the timeout with a loop so that the port does not
     * block during the entire read.  If we don't do this then it is not possible
     * to abort a long exposure */
    deltaTime = 0;
    epicsTimeGetCurrent(&tStart);
    while (deltaTime <= timeout) {
        status = pasynOctetSyncIO->read(pasynUser, ReplyMessage,
                                        sizeof(ReplyMessage), ASYN_POLL_TIME,
                                        &nread, &eomReason);
        if (status != asynTimeout) break;
        /* Need to sleep so that other threads can access port */
        epicsThreadSleep(ASYN_POLL_TIME);
        epicsTimeGetCurrent(&tCheck);
        deltaTime = epicsTimeDiffInSeconds(&tCheck, &tStart);
    }

    if (status) asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "%s:asynRead, timeout=%f, status=%d received %d bytes\n%s\n",
              DRIVER_NAME, timeout, status, nread, ReplyMessage);

    /* Post input string so user can see it */
    seq_pvPut(ssid, replyMessageIndex, 0);

    return(status);
}


static int readTIFF(char *fileName, double timeout)
{
    /* Reads a TIFF file into the imageBuffer 
     * If timeout>0 then this function waits up to timeout seconds for the
     * file to be created and for it to be the expected size.  In this
     * case it also monitors the Acquire PV and exits if Acquire=0, which
     * means that an acquisition was aborted.
     *
     * If timeout=0 then the file is assumed to already exist.
     */
    int fd=-1;
    int file_exists=0;
    int count;
    int nread;
    int offset;
    struct stat statBuff;
    epicsTimeStamp tStart, tCheck;
    double deltaTime;
    int status=-1;
    int retries;
    int expectedSize = TIFF_HEADER_SIZE + NXPixels*NYPixels*sizeof(epicsUInt32);
    union {
        unsigned uint;
        unsigned char uchar[sizeof(unsigned)];
    } endianTest;
    int  littleEndian;    /* 1 if host is little endian*/

    endianTest.uint = 1;
    littleEndian = (endianTest.uchar[0] == 1);

    if (timeout > 0.) {
        /* Wait for timeout for a new file to be created */
        deltaTime = 0;
        epicsTimeGetCurrent(&tStart);
        while (deltaTime <= timeout) {
            if (Acquire==0) return(0);
            fd = open(fileName, O_RDONLY, 0);
            if (fd >= 0) {
                file_exists = 1;
                /* The file exists.  Make sure it is a new file, not an old one */
                status = fstat(fd, &statBuff);
                if (status){
                    asynPrint(pasynUser, ASYN_TRACE_ERROR,
                        "%s::readTIFF error calling fstat, errno=%d %s\n",
                        DRIVER_NAME, errno, fileName);
                    close(fd);
                    return(-1);
                }
                /* We allow up to 10 second clock skew between time on machine running this IOC
                 * and the machine with the file system returning modification time */
                if (difftime(statBuff.st_mtime, acqStartTime) > -10) break;
                close(fd);
                fd = -1;
            }
            epicsThreadSleep(FILE_READ_DELAY);
            epicsTimeGetCurrent(&tCheck);
            deltaTime = epicsTimeDiffInSeconds(&tCheck, &tStart);
        }
        if (fd < 0) {
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                "%s::readTIFF timeout waiting for file to be created %s\n",
                DRIVER_NAME, fileName);
            if (file_exists) {
                asynPrint(pasynUser, ASYN_TRACE_ERROR,
                    "  file exists but is more than 10 seconds old, possible clock synchronization problem\n");
            } 
            return(-1);
        }   

        /* At this point we know the file exists, but it may not be completely written yet.
         * Check the file size repeatedly for up to readTimeout seconds until it
         * is the expected size */
        deltaTime = 0;
        epicsTimeGetCurrent(&tStart);
        retries=0;
        while (deltaTime <= timeout) {
            if (Acquire==0) return(0);
            status = fstat(fd, &statBuff);
            if (status){
                asynPrint(pasynUser, ASYN_TRACE_ERROR,
                    "%s::readTIFF error calling fstat, errno=%d %s\n",
                    DRIVER_NAME, errno, fileName);
                close(fd);
                return(-1);
            }
            if (statBuff.st_size == expectedSize) break;
            retries++;
            epicsThreadSleep(FILE_READ_DELAY);
            epicsTimeGetCurrent(&tCheck);
            deltaTime = epicsTimeDiffInSeconds(&tCheck, &tStart);
            status = -1;
        }
        if (status){
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                "%s::readTIFF timeout waiting for file to be correct size, size=%d/%d %s\n",
                DRIVER_NAME, statBuff.st_size, expectedSize, fileName);
            close(fd);
            return(-1);
        }
        if (retries > 1) {
            asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                "%s::readTIFF Time waiting for file to be expected size=%f, retries=%d\n", 
                DRIVER_NAME, deltaTime, retries);
        }
    } else {  /* timeout=0, file should already exist */
        fd = open(fileName, O_RDONLY, 0);
        if (fd < 0) {
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                "%s::readTIFF error opening file %s\n",
                DRIVER_NAME, fileName);
            return(-1);
        }
    }   
    offset = lseek(fd, TIFF_HEADER_SIZE, SEEK_SET);
    if (offset < TIFF_HEADER_SIZE) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "%s::readTIFF cannot seek to start of data, file %s\n",
            DRIVER_NAME, fileName);
        close(fd);
        return(-1);
    }
    nread = NXPixels * NYPixels * sizeof(epicsUInt32);
    count = read(fd, (void *)ImageData, nread);
    if (count < nread) { 
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "%s::readTIFF error reading data, count=%d, should be %d, file %s\n",
            DRIVER_NAME, count, nread, fileName);
        close(fd);
        return(-1);
    }
    status = close(fd);
    if (status) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "%s::readTIFF cannot close file %s, error=%d\n",
            DRIVER_NAME, fileName, status);
        return(-1);
    }
    /* TIFF files written by camserver are little-endian.  
     * If we are running on a big-endian machine then we need to byte-swap */
    if (!littleEndian) {
        for (i=0; i<MAX_PIXELS; i++) {
            ImageData[i] = swapInt32(ImageData[i]);
        }
    }
    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
        "%s::readTIFF read file %d/%d\n",
        DRIVER_NAME, mfNextImage+1, NImages);
        
    /* Correct the bad pixels */
    for (i=0; i<NBadPixels; i++) {
        ImageData[badPixelMap[i].badIndex] = 
        ImageData[badPixelMap[i].replaceIndex];
    }    
    return(0);
}


static int swapInt32(int in)
{
    union {
        unsigned char bytes[4];
        int longWord;
    } result, tmp;

    tmp.longWord = in;

    result.bytes[0] = tmp.bytes[3];
    result.bytes[1] = tmp.bytes[2];
    result.bytes[2] = tmp.bytes[1];
    result.bytes[3] = tmp.bytes[0];
    return result.longWord;
}


static void computeROIs()
{
    /* Computes the total and net counts in each ROI */
    int i;
    int xmin, ymin, xmax, ymax, bgdWidth;
    int totalPixels, bgdPixels, bgdPixelsTemp;
    int minCounts, maxCounts;
    double bgdCounts, bgdCountsTemp;
    double averageBgd;
    epicsTimeStamp tCheck;
    double deltaTime;

    if (FlatFieldValid) {
        for (i=0; i<MAX_PIXELS; i++) {
            ImageData[i] = (averageFlatField * ImageData[i])/flatField[i];
        }
    } 
    epicsTimeGetCurrent(&tCheck);
    deltaTime = epicsTimeDiffInSeconds(&tCheck, &lastWFPostTime);

    for (i=0; i<nROIs; i++) {
        if (!validROI(i)) continue;
        ROITotalCounts[i] = 0;
        bgdPixels = 0;
        bgdCounts = 0;
        ROINetCounts[i] = 0;
        xmin = ROIXMin[i];
        xmax = ROIXMax[i];
        ymin = ROIYMin[i];
        ymax = ROIYMax[i];
        bgdWidth = ROIBgdWidth[i];

        /* Compute the total ROI counts */
        computeRectangleCounts(xmin, xmax,
                               ymin, ymax,
                               &totalPixels, &ROITotalCounts[i],
                               &ROIMinCounts[i], &ROIMaxCounts[i]);
        ROINetCounts[i] = ROITotalCounts[i];
        
        if (bgdWidth > 0) {
            /* Compute the total background counts */
            /* There are 4 rectangles to compute */
            /* First rectangle is below the ROI */
            computeRectangleCounts(xmin - bgdWidth, xmax + bgdWidth,
                                   ymin - bgdWidth, ymin - 1,
                                   &bgdPixelsTemp, &bgdCountsTemp,
                                   &minCounts, &maxCounts);
            bgdPixels += bgdPixelsTemp;
            bgdCounts += bgdCountsTemp;
            /* Second rectangle is above the ROI */
            computeRectangleCounts(xmin - bgdWidth, xmax + bgdWidth,
                                   ymax + 1, ymax + bgdWidth,
                                   &bgdPixelsTemp, &bgdCountsTemp,
                                   &minCounts, &maxCounts);
            bgdPixels += bgdPixelsTemp;
            bgdCounts += bgdCountsTemp;
            /* Third rectangle is left of the ROI */
            computeRectangleCounts(xmin - bgdWidth, xmin - 1,
                                   ymin, ymax,
                                   &bgdPixelsTemp, &bgdCountsTemp,
                                   &minCounts, &maxCounts);
            bgdPixels += bgdPixelsTemp;
            bgdCounts += bgdCountsTemp;
            /* Third rectangle is right of the ROI */
            computeRectangleCounts(xmax + 1, xmax + bgdWidth,
                                   ymin, ymax,
                                   &bgdPixelsTemp, &bgdCountsTemp,
                                   &minCounts, &maxCounts);
            bgdPixels += bgdPixelsTemp;
            bgdCounts += bgdCountsTemp;

            /* Compute the net ROI counts */
            averageBgd = bgdCounts/bgdPixels;
            ROINetCounts[i] = ROITotalCounts[i] - averageBgd*totalPixels;
        } 
       
        seq_pvPut(ssid, ROITotalCountsIndex[i], ROITotalCounts[i]);
        seq_pvPut(ssid, ROINetCountsIndex[i], ROINetCounts[i]);
        seq_pvPut(ssid, ROIMinCountsIndex[i], ROIMinCounts[i]);
        seq_pvPut(ssid, ROIMaxCountsIndex[i], ROIMaxCounts[i]);
        
        /* If we are collecting multiple images then store the counts in the array */
        if (NImages > 1) {
            ROIWFTotalCounts[i][mfNextImage] = ROITotalCounts[i];
            ROIWFNetCounts[i][mfNextImage] = ROINetCounts[i];
            /* Post monitors on the arrays. Throttle this to < minWFUpdateTime
             * Always post after last file */
            if ((deltaTime > MinWFUpdateTime) || (mfNextImage == NImages-1)) {
                epicsTimeGetCurrent(&lastWFPostTime);
                seq_pvPut(ssid, ROIWFTotalCountsIndex[i], 0);
                seq_pvPut(ssid, ROIWFNetCountsIndex[i], 0);
            }
        }
    }
    /* Post the image array if enabled */
    if (PostImages) postImage();
}


static void postImage()
{
    epicsTimeStamp tCheck;
    double deltaTime;
    int i, ix, iy;
    int maxValue;
    int xmin, ymin, xmax, ymax;
    epicsInt32 *pRow;

    /* Post the image array if enabled and minimum time has elapsed, or if this is the final image
       or if we are in alignment mode */
    epicsTimeGetCurrent(&tCheck);
    deltaTime = epicsTimeDiffInSeconds(&tCheck, &lastImagePostTime);
    if ((deltaTime > MinImageUpdateTime) || 
        (AcquireMode==ALIGNMENT) ||
        (mfNextImage == NImages-1)) {
        /* Highlight ROIs if enabled */
        if (HighlightROIs) {
            /* Find the maximum value of data */
            maxValue = 0;
            for (i=0; i<MAX_PIXELS; i++) {
                if (ImageData[i] > maxValue) {
                    maxValue=ImageData[i];
                }
            }
            if (maxValue == 0) maxValue=1;
            for (i=0; i<nROIs; i++) {
                if (!validROI(i)) continue;
                xmin = ROIXMin[i];
                xmax = ROIXMax[i];
                ymin = ROIYMin[i];
                ymax = ROIYMax[i];
                for (iy=ymin; iy<=ymax; iy++) {
                    pRow = ImageData + iy*NXPixels;
                    if ((iy == ymin) || (iy == ymax)) {
                        for (ix=xmin; ix<=xmax; ix++) pRow[ix] = maxValue;
                    } else {
                        pRow[xmin] = maxValue; 
                        pRow[xmax] = maxValue; 
                    }
                }
            }
        }
        epicsTimeGetCurrent(&lastImagePostTime);
        seq_pvPut(ssid, imageDataIndex, 0);
    }
}


static int validROI(int roi)
{
    
    if ((ROIXMin[roi]<0) || 
        (ROIXMax[roi]<0) ||
        (ROIYMin[roi]<0) ||
        (ROIYMax[roi]<0) ||
        (ROIXMin[roi]>NXPixels-1) ||
        (ROIXMax[roi]>NXPixels-1) ||
        (ROIYMin[roi]>NYPixels-1) ||
        (ROIYMax[roi]>NYPixels-1) ||
        (ROIXMin[roi]>ROIXMax[roi]) ||
        (ROIYMin[roi]>ROIYMax[roi]))
        return(0);
    else
        return(1);
}
        

static void computeRectangleCounts(int xmin, int xmax, int ymin, int ymax,
                                   int *nPixels, double *counts, 
                                   int *minCounts, int *maxCounts)
{
    int ix, iy;
    epicsInt32 *pRow;
    int val;

    if (xmin < 0) xmin = 0;
    if (xmax < 0) xmax = 0;
    if (ymin < 0) ymin = 0;
    if (ymax < 0) ymax = 0;
    if (xmin > NXPixels - 1) xmin = NXPixels - 1;
    if (xmax > NXPixels - 1) xmax = NXPixels - 1;
    if (ymin > NYPixels - 1) ymin = NYPixels - 1;
    if (ymax > NYPixels - 1) ymax = NYPixels - 1;
    *nPixels = 0;
    *counts = 0;
    *maxCounts = -1;
    *minCounts = MAX_COUNTS;
    for (iy=ymin; iy<=ymax; iy++) {
        pRow = ImageData + iy*NXPixels;
        for (ix=xmin; ix<=xmax; ix++) { 
            (*nPixels)++;
            val = pRow[ix];
            *counts += val;
            if (val > *maxCounts) *maxCounts = val;
            if (val < *minCounts) *minCounts = val;
        }
    }
}


static void pilatusErase()
{
    int i, j;

    readErrors = 0;    
    mfNextImage = 0;
    for (i=0; i<nROIs; i++) {
        ROITotalCounts[i] = 0.;
        ROINetCounts[i] = 0;        
        /* If we are collecting multiple images then zero waveforms */
        if (NImages > 1) {
            for (j=0; j<MAX_CHANS; j++) {
                ROIWFTotalCounts[i][j] = 0;
                ROIWFNetCounts[i][j] = 0;
            }
            seq_pvPut(ssid, ROIWFTotalCountsIndex[i], 0);
            seq_pvPut(ssid, ROIWFNetCountsIndex[i], 0);
            epicsTimeGetCurrent(&lastWFPostTime);
        }
    }
}


static void readBadPixelFile()
{
    int i; 
    int xbad, ybad, xgood, ygood;
    int n;
    FILE *file;

    NBadPixels = 0;
    if (strlen(BadPixelFile) == 0) return;
    file = fopen(BadPixelFile, "r");
    if (file == NULL) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "%s::readBadPixelFile, cannot open file %s\n",
            DRIVER_NAME, BadPixelFile);
        return;
    }
    for (i=0; i<MAX_BAD_PIXELS; i++) {
        n = fscanf(file, " %d,%d %d,%d",
                  &xbad, &ybad, &xgood, &ygood);
        if (n == EOF) return;
        if (n != 4) {
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                "%s::readBadPixelFile, too few items =%d, should be 4\n",
                DRIVER_NAME, n);
            return;
        }
        badPixelMap[i].badIndex = ybad*NXPixels + xbad;
        badPixelMap[i].replaceIndex = ygood*NXPixels + xgood;
        NBadPixels++;
    }
}


static void readFlatFieldFile()
{
    int i;
    int status;
    int ngood;
    
    FlatFieldValid = 0;
    if (strlen(FlatFieldFile) == 0) return;
    status = readTIFF(FlatFieldFile, 0.);
    if (status) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "%s::readFlatFieldFile, error reading flat field file %s\n",
            DRIVER_NAME, FlatFieldFile);
        return;
    }
    /* Compute the average counts in the flat field */
    averageFlatField = 0.;
    ngood = 0;
    for (i=0; i<MAX_PIXELS; i++) {
         if (ImageData[i] < MinFlatField) continue;
        ngood++;
        averageFlatField += ImageData[i];
    }
    averageFlatField = averageFlatField/ngood;
    for (i=0; i<MAX_PIXELS; i++) {
        if (ImageData[i] < MinFlatField) ImageData[i] = averageFlatField;
    }
    /* Post ImageData so viewer can see flat field */
    postImage();
    /* Copy from ImageData to flatField */
    for (i=0; i<MAX_PIXELS; i++) {
        flatField[i] = ImageData[i];
    }
    FlatFieldValid = 1;
}


static void makeMultipleFileFormat()
{
    /* This function uses the code from camserver */
    char *p, *q;
    int fmt;
    char mfTempFormat[MAX_FILENAME_LEN];
    char mfExtension[10];
    
    /* FullFilename has been built by the caller.
     * Copy to temp */
    strcpy(mfTempFormat, FullFilename);
    p = mfTempFormat + strlen(mfTempFormat) - 5; /* look for extension */
    if ( (q=strrchr(p, '.')) ) {
        strcpy(mfExtension, q);
        *q = '\0';
    } else {
        strcpy(mfExtension, ""); /* default is raw image */
    }
    mfNumber=0;   /* start number */
    fmt=5;        /* format length */
    if ( !(p=strrchr(mfTempFormat, '/')) ) {
        p=mfTempFormat;
    }
    if ( (q=strrchr(p, '_')) ) {
        q++;
        if (isdigit(*q) && isdigit(*(q+1)) && isdigit(*(q+2))) {
            mfNumber=atoi(q);
            fmt=0;
            p=q;
            while(isdigit(*q)) {
                fmt++;
                q++;
            }
            *p='\0';
            if (((fmt<3)  || ((fmt==3) && (NImages>999))) || 
                ((fmt==4) && (NImages>9999))) { 
                fmt=5;
            }
        } else if (*q) {
            strcat(p, "_"); /* force '_' ending */
        }
    } else {
        strcat(p, "_"); /* force '_' ending */
    }
    /* Build the final format string */
    epicsSnprintf(mfFormat, sizeof(mfFormat), "%s%%.%dd%s",
                  mfTempFormat, fmt, mfExtension);
}

}%

