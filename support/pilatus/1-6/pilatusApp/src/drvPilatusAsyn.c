/* File:    drvPilatusAsyn.c
 * Author:  Mark Rivers
 * Date:    18-May-2007
 *
 * Purpose: 
 * This module provides the driver support for scaler and mca record asyn device support layers
 * for the Pilatus detector.
 *
 *
 */

/*******************/
/* System includes */
/*******************/

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>


/******************/
/* EPICS includes */
/******************/

/* EPICS includes */
#include <dbAccess.h>
#include <asynDriver.h>
#include <asynInt32.h>
#include <asynFloat64.h>
#include <asynInt32Array.h>
#include <asynOctet.h>
#include <asynOctetSyncIO.h>
#include <asynDrvUser.h>
#include <cantProceed.h>
#include <epicsEvent.h>
#include <epicsMutex.h>
#include <epicsString.h>
#include <epicsThread.h>
#include <epicsTime.h>
#include <epicsExport.h>
#include <errlog.h>
#include <iocsh.h>

/*******************/
/* Custom includes */
/*******************/

#include <devScalerAsyn.h>
#include <mca.h>
#include <drvMca.h>
#include <drvPilatusAsyn.h>

/***************/
/* Definitions */
/***************/

#define TIFF_HEADER_SIZE 4096
#define MAX_MESSAGE_SIZE 200 /* Mesages to/from camserver */
/* Fake frequency that converts preset counts into time. Scaler record must be set to this. */
#define CHANNEL1_FREQUENCY 1e6
#define MAX_FILENAME_LEN 256
#define MAX_PVNAME_SIZE 40

#define FILE_READ_TIMEOUT 2.0 /* Time after TIFF file exists before it is the right size */
#define FILE_READ_DELAY .01   /* Time between checking to see if TIFF file is complete */

#define MULTICHANNEL_SCALER_MODE 0
#define SIMPLE_SCALER_MODE       1

#define WRITE_READ_TIMEOUT 2.0
#define READ_TIMEOUT 2.0
#define READ_RETRIES 2

typedef enum {
    UNDEFINED_MODE,
    MCS_MODE,
    SCALER_MODE
} pilatusAcquireMode;


/**************/
/* Structures */
/**************/
typedef enum {
    scalerResetCommand=MAX_MCA_COMMANDS + 1,
    scalerChannelsCommand,
    scalerReadCommand,
    scalerPresetCommand,
    scalerArmCommand,
    scalerDoneCommand,
    pilatusROIXMinCommand,
    pilatusROIXMaxCommand,
    pilatusROIYMinCommand,
    pilatusROIYMaxCommand,
    pilatusROIBgdWidthCommand
} pilatusCommand;


typedef struct {
    pilatusCommand command;
    char *commandString;
} pilatusCommandStruct;

static pilatusCommandStruct pilatusCommands[MAX_PILATUS_COMMANDS + MAX_SCALER_COMMANDS] = {
    {scalerResetCommand,           SCALER_RESET_COMMAND_STRING},    /* int32, write */
    {scalerChannelsCommand,        SCALER_CHANNELS_COMMAND_STRING}, /* int32, read */
    {scalerReadCommand,            SCALER_READ_COMMAND_STRING},     /* int32Array, read */
    {scalerPresetCommand,          SCALER_PRESET_COMMAND_STRING},   /* int32, write */
    {scalerArmCommand,             SCALER_ARM_COMMAND_STRING},      /* int32, write */
    {scalerDoneCommand,            SCALER_DONE_COMMAND_STRING},     /* int32, read */
    {pilatusROIXMinCommand,        PILATUS_ROI_XMIN_STRING},        /* int32, write */
    {pilatusROIXMaxCommand,        PILATUS_ROI_XMAX_STRING},        /* int32, write */
    {pilatusROIYMinCommand,        PILATUS_ROI_YMIN_STRING},        /* int32, write */
    {pilatusROIYMaxCommand,        PILATUS_ROI_YMAX_STRING},        /* int32, write */
    {pilatusROIBgdWidthCommand,    PILATUS_ROI_BGD_WIDTH_STRING},   /* int32, write */
};

typedef struct {
    int xmin;
    int xmax;
    int ymin;
    int ymax;
    int bgdWidth;
    double totalCounts;
    double netCounts;
} pilatusROIStruct;


typedef struct pilatusPvt {
    char *portName;
    char *tcpPortName;
    asynUser *pasynUserOctet;
    char *dbPrefix;
    char sendMessage[MAX_MESSAGE_SIZE];
    char replyMessage[MAX_MESSAGE_SIZE];
    char *driver;
    char filename[MAX_FILENAME_LEN];
    char fileFormat[MAX_FILENAME_LEN];
    char fullFilename[MAX_FILENAME_LEN];
    int fileNumber;
    int autoIncrement;
    int exists;
    int moduleID;
    int firmwareVersion;
    pilatusAcquireMode acquireMode;
    int maxSignals;
    int maxChans;
    int nchans;
    int nextChan;
    int nextSignal;
    double presetReal;
    double presetLive;
    epicsTimeStamp startTime;
    epicsTimeStamp statusTime;
    double maxStatusTime;
    double elapsedTime;
    double elapsedPrevious;
    pilatusROIStruct *ROIs;
    int *presetStartChan;  /* maxSignals */
    int *presetEndChan;    /* maxSignals */
    long *presetCounts;    /* maxSignals */
    long *elapsedCounts;   /* maxSignals */
    long *scalerPresets;   /* maxSignals */
    double countTime;
    int numSweeps;
    double dwellTime;
    int softAdvance;
    epicsUInt32 *buffer;  /* maxSignals * maxChans */
    epicsUInt32 *buffPtr;
    int imageXSize;
    int imageYSize;
    epicsUInt32 *imageBuffer;  /* [imageXSize][imageYSize] */
    int acquiring;
    int prevAcquiring;
    epicsMutexId lock;

    asynUser *pasynUser;
    asynInterface common;
    asynInterface int32;
    void *int32InterruptPvt;
    asynInterface float64;
    void *float64InterruptPvt;
    asynInterface int32Array;
    void *int32ArrayInterruptPvt;
    asynInterface octet;
    asynInterface drvUser;

    epicsThreadId acquireThreadId;
    epicsEventId acquireStartEventId;
    DBADDR filenameAddr;
    DBADDR fileNumberAddr;
    DBADDR fileFormatAddr;
    DBADDR autoIncrementAddr;
} pilatusPvt;

/*******************************/
/* Global variable declaration */
/*******************************/

/**************/
/* Prototypes */
/**************/

/* These functions are in public interfaces */
static asynStatus int32Write        (void *drvPvt, asynUser *pasynUser,
                                     epicsInt32 value);
static asynStatus int32Read         (void *drvPvt, asynUser *pasynUser,
                                     epicsInt32 *value);
static asynStatus getBounds         (void *drvPvt, asynUser *pasynUser,
                                     epicsInt32 *low, epicsInt32 *high);
static asynStatus float64Write      (void *drvPvt, asynUser *pasynUser,
                                     epicsFloat64 value);
static asynStatus float64Read       (void *drvPvt, asynUser *pasynUser,
                                     epicsFloat64 *value);
static asynStatus int32ArrayRead    (void *drvPvt, asynUser *pasynUser,
                                     epicsInt32 *data, size_t maxChans,
                                     size_t *nactual);
static asynStatus int32ArrayWrite   (void *drvPvt, asynUser *pasynUser,
                                     epicsInt32 *data, size_t maxChans);
static asynStatus octetWrite        (void *drvPvt, asynUser *pasynUser,
                                     const char *data, size_t numchars, 
                                     size_t *nbytesTransfered);
static asynStatus octetRead         (void *drvPvt, asynUser *pasynUser,
                                     char *data, size_t maxchars, 
                                     size_t *nbytesTransfered, int *eomReason);
static asynStatus drvUserCreate     (void *drvPvt, asynUser *pasynUser,
                                     const char *drvInfo,
                                     const char **pptypeName, size_t *psize);
static asynStatus drvUserGetType    (void *drvPvt, asynUser *pasynUser,
                                     const char **pptypeName, size_t *psize);
static asynStatus drvUserDestroy    (void *drvPvt, asynUser *pasynUser);

static void pilatusReport               (void *drvPvt, FILE *fp, int details);
static asynStatus pilatusConnect        (void *drvPvt, asynUser *pasynUser);
static asynStatus pilatusDisconnect     (void *drvPvt, asynUser *pasynUser);

/* Private methods */
static asynStatus pilatusWrite(void *drvPvt, asynUser *pasynUser,
                               epicsInt32 ivalue, epicsFloat64 dvalue);
static asynStatus pilatusRead(void *drvPvt, asynUser *pasynUser,
                              epicsInt32 *pivalue, epicsFloat64 *pfvalue);
static int pilatusErase(pilatusPvt *pPvt);
static void acquireTask(pilatusPvt *pPvt);
static int checkAcquireComplete(pilatusPvt *pPvt, asynUser *pasynUser);
static void acquireDoneCallbacks(pilatusPvt *pPvt);
static void setAcquireMode(pilatusPvt *pPvt, pilatusAcquireMode acquireMode);
static int readTIFF(pilatusPvt *pPvt, char *filename);
static void computeROIs(pilatusPvt *pPvt);
static void computeRectangleCounts(pilatusPvt *pPvt, 
                                   int xmin, int xmax, int ymin, int ymax,
                                   int *nPixels, double *counts);
static int camserverWriteRead(pilatusPvt *pPvt);
static int camserverRead(pilatusPvt *pPvt);
/*
 * asynCommon methods
 */
static struct asynCommon pilatusCommon = {
    pilatusReport,
    pilatusConnect,
    pilatusDisconnect
};

/* asynInt32 methods */
static asynInt32 pilatusInt32 = {
    int32Write,
    int32Read,
    getBounds,
    NULL,
    NULL
};

/* asynFloat64 methods */
static asynFloat64 pilatusFloat64 = {
    float64Write,
    float64Read,
    NULL,
    NULL
};

/* asynInt32Array methods */
static asynInt32Array pilatusInt32Array = {
    int32ArrayWrite,
    int32ArrayRead,
    NULL,
    NULL
};

/* asynOctet methods */
static asynOctet pilatusOctet = {
    octetWrite,
    NULL,
    octetRead,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static asynDrvUser pilatusDrvUser = {
    drvUserCreate,
    drvUserGetType,
    drvUserDestroy
};


/********/
/* Code */
/********/

/* iocsh commands */

/* This command needs to be called once for each Pilatus */
int drvPilatusAsynConfigure(char  *portName,
                            char  *tcpPortName,
                            int   maxROIs,
                            int   maxChans,
                            int   imageXSize,
                            int   imageYSize,
                            char  *dbPrefix)
{
    pilatusPvt *pPvt;
    int status;

    /* Allocate and initialize pilatusPvt*/

    pPvt = callocMustSucceed(1, sizeof(pilatusPvt), "drvPilatusAsynConfig");
    pPvt->portName = epicsStrDup(portName);
    pPvt->tcpPortName = epicsStrDup(tcpPortName);
    pPvt->driver = "drvPilatusAsyn";
    pPvt->maxSignals = maxROIs;
    pPvt->maxChans = maxChans;
    pPvt->nchans = maxChans;
    pPvt->imageXSize = imageXSize;
    pPvt->imageYSize = imageYSize;
    pPvt->dbPrefix = epicsStrDup(dbPrefix);

    /* Create asynUser for debugging */
    pPvt->pasynUser = pasynManager->createAsynUser(0, 0);

    /* Make sure we can talk to camserver, and get the firmware version */
    status = pasynOctetSyncIO->connect(tcpPortName, 0, &pPvt->pasynUserOctet, 0);
    if (status != asynSuccess) {
        errlogPrintf("%s::drvPilatusAsynConfig port %s"
                     " can't connect to camserver %s.\n",
                     pPvt->driver, portName, tcpPortName);
        return(asynError);
    }
 
    pPvt->acquiring = 0;
    pPvt->prevAcquiring = 0;

    pPvt->maxStatusTime = 2.0;

    /* Allocate memory buffers */
    pPvt->buffer = (epicsUInt32 *)callocMustSucceed(maxROIs*maxChans, 
                                                    sizeof(epicsUInt32), 
                                                    "drvPilatusAsynConfig");
    pPvt->imageBuffer = (epicsUInt32 *)callocMustSucceed(imageXSize*imageYSize, 
                                                         sizeof(epicsUInt32), 
                                                         "drvPilatusAsynConfig");

    pPvt->ROIs = (pilatusROIStruct *)callocMustSucceed(maxChans, 
                                                       sizeof(pilatusROIStruct), 
                                                       "drvPilatusAsynConfig");

    pPvt->presetStartChan = (int *)callocMustSucceed(maxChans, sizeof(int), "drvPilatusAsynConfig");
    pPvt->presetEndChan   = (int *)callocMustSucceed(maxChans, sizeof(int), "drvPilatusAsynConfig");
    pPvt->presetCounts    = (long *)callocMustSucceed(maxChans, sizeof(long), "drvPilatusAsynConfig");
    pPvt->elapsedCounts   = (long *)callocMustSucceed(maxChans, sizeof(long), "drvPilatusAsynConfig");
    pPvt->scalerPresets   = (long *)callocMustSucceed(maxChans, sizeof(long), "drvPilatusAsynConfig");

    /* Initialize the buffer pointer to the start of the buffer area */
    pPvt->buffPtr = pPvt->buffer;

    /* Create semaphore to interlock access to card */
    pPvt->lock = epicsMutexCreate();

    /* Create the EPICS event used to wake up the interrupt task */
    pPvt->acquireStartEventId = epicsEventCreate(epicsEventFull);

    /* Initialize board in MCS mode */
    pPvt->acquireMode = UNDEFINED_MODE;
    setAcquireMode(pPvt, MCS_MODE);

    pilatusErase(pPvt);
    
    /* Set an initial filename */
    strcpy(pPvt->filename, "junk.tif");
        
    /* Create the thread that will do the actual I/O */ 
    pPvt->acquireThreadId = epicsThreadCreate("pilatusAcquire",
           epicsThreadPriorityMedium,
           epicsThreadGetStackSize(epicsThreadStackMedium),
           (EPICSTHREADFUNC)acquireTask, 
           pPvt);

    /* Link with higher level routines */
    pPvt->common.interfaceType = asynCommonType;
    pPvt->common.pinterface  = (void *)&pilatusCommon;
    pPvt->common.drvPvt = pPvt;
    pPvt->int32.interfaceType = asynInt32Type;
    pPvt->int32.pinterface  = (void *)&pilatusInt32;
    pPvt->int32.drvPvt = pPvt;
    pPvt->float64.interfaceType = asynFloat64Type;
    pPvt->float64.pinterface  = (void *)&pilatusFloat64;
    pPvt->float64.drvPvt = pPvt;
    pPvt->int32Array.interfaceType = asynInt32ArrayType;
    pPvt->int32Array.pinterface  = (void *)&pilatusInt32Array;
    pPvt->int32Array.drvPvt = pPvt;
    pPvt->octet.interfaceType = asynOctetType;
    pPvt->octet.pinterface  = (void *)&pilatusOctet;
    pPvt->octet.drvPvt = pPvt;
    pPvt->drvUser.interfaceType = asynDrvUserType;
    pPvt->drvUser.pinterface = (void *)&pilatusDrvUser;
    pPvt->drvUser.drvPvt = pPvt;

    status = pasynManager->registerPort(pPvt->portName,
        ASYN_MULTIDEVICE /* | ASYN_CANBLOCK*/ , 
        1, /* autoconnect */
        0, /* medium priority */
        0); /* default stacksize */
    if (status != asynSuccess)
    {
      errlogPrintf("pilatusConfig ERROR: Can't register myself.\n");
      return -1;
    }

    status = pasynManager->registerInterface(pPvt->portName, &pPvt->common);
    if (status != asynSuccess) 
    {
      errlogPrintf("pilatusConfig : Can't register common.\n");
      return -1;
    }

    status = pasynInt32Base->initialize(pPvt->portName, &pPvt->int32);
    if (status != asynSuccess) 
    {
      errlogPrintf("pilatusConfig: Can't register int32.\n");
      return -1;
    }

    status = pasynFloat64Base->initialize(pPvt->portName, &pPvt->float64);
    if (status != asynSuccess) 
    {
      errlogPrintf("pilatusConfig: Can't register float64.\n");
      return -1;
    }

    status = pasynInt32ArrayBase->initialize(pPvt->portName, &pPvt->int32Array);
    if (status != asynSuccess) 
    {
      errlogPrintf("pilatusConfig: Can't register int32Array.\n");
      return -1;
    }

    status = pasynOctetBase->initialize(pPvt->portName, &pPvt->octet, 0, 0, 0);
    if (status != asynSuccess) 
    {
      errlogPrintf("pilatusConfig: Can't register octet.\n");
      return -1;
    }

     pasynManager->registerInterruptSource(portName, &pPvt->int32,
        &pPvt->int32InterruptPvt);

    pasynManager->registerInterruptSource(portName, &pPvt->float64,
        &pPvt->float64InterruptPvt);

    pasynManager->registerInterruptSource(portName, &pPvt->int32Array,
        &pPvt->int32ArrayInterruptPvt);

    status = pasynManager->registerInterface(pPvt->portName,&pPvt->drvUser);
    if (status != asynSuccess) 
    {
      errlogPrintf("pilatusConfig ERROR: Can't register drvUser\n");
      return -1;
    }

    /* Connect pasynUser to device for debugging */
    status = pasynManager->connectDevice(pPvt->pasynUser, portName, 0);
    if (status != asynSuccess) 
    {
      errlogPrintf("pilatusConfig, connectDevice failed for pilatus\n");
      return -1;
    }

    return (0);
}


/* Routines to write to the pilatus */

static asynStatus int32Write(void *drvPvt, asynUser *pasynUser,
    epicsInt32 value)
{
    return(pilatusWrite(drvPvt, pasynUser, value, 0.));
}

static asynStatus float64Write(void *drvPvt, asynUser *pasynUser,
    epicsFloat64 value)
{
    return(pilatusWrite(drvPvt, pasynUser, 0, value));
}

static asynStatus pilatusWrite(void *drvPvt, asynUser *pasynUser,
    epicsInt32 ivalue, epicsFloat64 dvalue)
{
    pilatusPvt *pPvt = (pilatusPvt *)drvPvt;
    int command=pasynUser->reason;
    int signal;
    int i;
 
    pasynManager->getAddr(pasynUser, &signal);

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s::pilatusWrite entry, command=%d, signal=%d, "
        "ivalue=%d, dvalue=%f\n", pPvt->driver, command, signal, ivalue, dvalue);
    switch (command) 
    {
        case mcaStartAcquire:
          /* Start acquisition. */
          /* Note that we don't start acquisition except for channel 0. */
          if (signal != 0) break;

          /* If the MCS is set to use internal channel advance, just start
           * collection. If it is set to use an external channel advance, arm
           * the scaler so that the next LNE pulse starts collection
           */

          pPvt->acquiring = 1;
          pPvt->prevAcquiring = 1;

          setAcquireMode(pPvt, MCS_MODE);

        case mcaStopAcquire:
          /* stop data acquisition */
          break;

        case mcaErase:
          /* Erase. If this is signal 0 then erase memory for all signals.
           * Databases take advantage of this for performance with
           * multielement detectors with multiplexors */
          /* For the pilatus, just do the complete erase. 
           * The alternative would be to erase sections of the buffer, but this
           * could get messy, so will keep it simple at the moment.
           */

          /* If pilatus is acquiring, turn if off */

          asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "%s::pilatusWrite [%s signal=%d]:"
              " erased\n",
              pPvt->driver, pPvt->portName, signal);

          /* Erase the buffer in the private data structure */
          
          /* If pilatus was acquiring, turn it back on */
          /* If the MCS is set to use internal channel advance, just start
           * collection. If it is set to use an external channel advance, arm
           * the scaler so that the next LNE pulse starts collection
           */
          if (pPvt->acquiring)
          {
          }
          break;

        case mcaReadStatus:
          /* Read the FIFO.  This is needed to see if acquisition is complete and to update the
           * elapsed times.
           */
           break;

        case mcaChannelAdvanceInternal:
          /* set channel advance source to internal (timed) */
          /* Just cache this setting here, set it when acquisition starts */
          break;

        case mcaChannelAdvanceExternal:
          /* set channel advance source to external */
          /* Just cache this setting here, set it when acquisition starts */
          break;

        case mcaNumChannels:
          /* Terminology warning:
           * This is the number of channels that are to be acquired. Channels
           * correspond to time bins or external channel advance triggers, as
           * opposed to the 32 input signals that the pilatus supports.
           */
          /* Just cache this value for now, set it when acquisition starts */
          pPvt->nchans = ivalue;
          if (pPvt->nchans > pPvt->maxChans) 
          {
            asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                "%s::pilatusWrite Illegal nuse field %d", pPvt->driver, pPvt->nchans);
            pPvt->nchans = pPvt->maxChans;
          }
          break;

        case mcaModePHA:
          /* set mode to Pulse Height Analysis */
          /* This is a NOOP for the pilatus */
          /* To make sure, we set the mode to MCS anyway */
          pPvt->acquireMode = MCS_MODE;
          break;

        case mcaModeMCS:
          /* set mode to MultiChannel Scaler */
          pPvt->acquireMode = MCS_MODE;
          break;

        case mcaModeList:
          /* set mode to LIST (record each incoming event) */
          /* This is a NOOP for the pilatus */
          break;

        case mcaSequence:
          /* set sequence number */
          /* This is a NOOP for the pilatus */
          break;

        case mcaPrescale:
          /* We just cache the value here.  It gets written to the hardware when acquisition
           * starts if we are in external channel advance mode 
           */
           break;

        case mcaPresetSweeps:
          /* set number of sweeps (for MCS mode) */
          /* This is not implemented yet, but we cache the value anyway */
          pPvt->numSweeps = ivalue;
          asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s::pilatusWrite mcaPresetSweeps = %d\n", pPvt->driver, pPvt->numSweeps);
          break;

        case mcaPresetLowChannel:
          /* set lower side of region integrated for preset counts */
          pPvt->presetStartChan[signal] = ivalue;
          break;

        case mcaPresetHighChannel:
          /* set high side of region integrated for preset counts */
          pPvt->presetEndChan[signal] = ivalue;
          break;

        case mcaDwellTime:
          /* set dwell time */
          /* We just cache the value here and set in mcaStartAquire if using internal channel advance */
          pPvt->dwellTime = dvalue;
          break;

        case mcaPresetRealTime:
          /* set preset real time. */
          pPvt->presetReal = dvalue;
          break;

        case mcaPresetLiveTime:
          /* set preset live time */
          pPvt->presetLive = dvalue;
          break;

        case mcaPresetCounts:
          /* set preset counts */
          pPvt->presetCounts[signal] = ivalue;
          break;

        case scalerResetCommand:
          /* Reset scaler */
           pPvt->acquiring = 0;
          /* Clear all of the presets */
          for (i=0; i<pPvt->maxSignals; i++) {
            pPvt->scalerPresets[i] = 0;
          }
          asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s::pilatusWrite scalerResetCommand\n", pPvt->driver);
          break;

        case scalerArmCommand:
          /* Arm or disarm scaler */
          setAcquireMode(pPvt, SCALER_MODE);
          if (ivalue != 0) {
           epicsEventSignal(pPvt->acquireStartEventId);
           } else {
          }
          asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s::pilatusWrite scalerArmCommand=%d\n", pPvt->driver, ivalue);
          break;

        case scalerPresetCommand:
          /* Set scaler preset */
          pPvt->scalerPresets[signal] = ivalue;
          asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s::pilatusWrite scalerPresetCommand channel %d=%d\n", pPvt->driver, signal, ivalue);
          break;

        case pilatusROIXMinCommand:
          /* Set ROI Xmin */
          pPvt->ROIs[signal].xmin = ivalue;
          asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s::pilatusWrite pilatusROIXMin ROI %d=%d\n", pPvt->driver, signal, ivalue);
          break;

        case pilatusROIXMaxCommand:
          /* Set ROI Xmax */
          pPvt->ROIs[signal].xmax = ivalue;
          asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s::pilatusWrite pilatusROIXMax ROI %d=%d\n", pPvt->driver, signal, ivalue);
          break;

        case pilatusROIYMinCommand:
          /* Set ROI Ymin */
          pPvt->ROIs[signal].ymin = ivalue;
          asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s::pilatusWrite pilatusROIYMin ROI %d=%d\n", pPvt->driver, signal, ivalue);
          break;

        case pilatusROIYMaxCommand:
          /* Set ROI Ymax */
          pPvt->ROIs[signal].ymax = ivalue;
          asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s::pilatusWrite pilatusROIYMax ROI %d=%d\n", pPvt->driver, signal, ivalue);
          break;

        case pilatusROIBgdWidthCommand:
          /* Set ROI bgdWidth */
          pPvt->ROIs[signal].bgdWidth = ivalue;
          asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s::pilatusWrite pilatusROIBgdWidth ROI %d=%d\n", pPvt->driver, signal, ivalue);
          break;

        default:
          asynPrint(pasynUser, ASYN_TRACE_ERROR, 
              "%s::pilatusWrite port %s got illegal command %d\n",
              pPvt->driver, pPvt->portName, command);
          break;
    }
    return(asynSuccess);
}


static asynStatus int32Read(void *drvPvt, asynUser *pasynUser,
    epicsInt32 *value)
{
    return(pilatusRead(drvPvt, pasynUser, value, NULL));
}

static asynStatus float64Read(void *drvPvt, asynUser *pasynUser,
    epicsFloat64 *value)
{
    return(pilatusRead(drvPvt, pasynUser, NULL, value));
}

static asynStatus pilatusRead(void *drvPvt, asynUser *pasynUser,
    epicsInt32 *pivalue, epicsFloat64 *pfvalue)
{
    pilatusPvt *pPvt = (pilatusPvt *)drvPvt;
    int command = pasynUser->reason;
    asynStatus status=asynSuccess;
    int signal;

    pasynManager->getAddr(pasynUser, &signal);

    switch (command) {
        case mcaAcquiring:
          asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s::pilatusRead mcaAcquiring\n",
              pPvt->driver);
          *pivalue = pPvt->acquiring;
          break;

        case mcaDwellTime:
          asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s::pilatusRead mcaDwellTime\n",
              pPvt->driver);
          *pfvalue = pPvt->dwellTime;
          break;

        case mcaElapsedLiveTime:
          /* The pilatus does not support live time recording */
          asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s::pilatusRead mcaElapsedLiveTime\n",
              pPvt->driver);
          *pfvalue = pPvt->elapsedTime;
          break;

        case mcaElapsedRealTime:
          /* The pilatus does not support real time recording */
          asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s::pilatusRead mcaElapsedRealTime\n",
              pPvt->driver);
          *pfvalue = pPvt->elapsedTime;
          break;

        case mcaElapsedCounts:
          /* The pilatus does not support elapsed count recording */
          asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s::pilatusRead mcaElapsedCounts\n",
              pPvt->driver);
          *pfvalue = 0;
          break;

        case scalerChannelsCommand:
          /* Return the number of scaler channels */
          *pivalue = pPvt->maxSignals;
          asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s::pilatusRead scalerChanneksCommand %d\n", pPvt->driver, *pivalue);
          break;

        case scalerReadCommand:
          /* Read a single scaler channel */
           asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s::pilatusRead scalerReadCommand channel%d=%d\n", pPvt->driver, signal, *pivalue);
          break;

        case scalerDoneCommand:
          /* Return scaler done, which is opposite of pPvt->acquiring */
          *pivalue = 1 - pPvt->acquiring;
          asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s::pilatusRead scalerDoneCommand, returning %d\n", pPvt->driver, *pivalue);
          break;

        case pilatusROIXMinCommand:
          /* Set ROI Xmin */
          *pivalue = pPvt->ROIs[signal].xmin;
          /* If this is an invalid ROI (min>=max) return error so init_record does not load garbage */
          if (pPvt->ROIs[signal].xmin >= pPvt->ROIs[signal].xmax) status = asynError;
          asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s::pilatusRead pilatusROIXMin ROI %d=%d\n", pPvt->driver, signal, *pivalue);
          break;

        case pilatusROIXMaxCommand:
          /* Set ROI Xmax */
          *pivalue = pPvt->ROIs[signal].xmax;
          /* If this is an invalid ROI (min>=max) return error so init_record does not load garbage */
          if (pPvt->ROIs[signal].xmin >= pPvt->ROIs[signal].xmax) status = asynError;
          asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s::pilatusRead pilatusROIXMax ROI %d=%d\n", pPvt->driver, signal, *pivalue);
          break;

        case pilatusROIYMinCommand:
          /* Set ROI Ymin */
          *pivalue = pPvt->ROIs[signal].ymin;
          /* If this is an invalid ROI (min>=max) return error so init_record does not load garbage */
          if (pPvt->ROIs[signal].ymin >= pPvt->ROIs[signal].ymax) status = asynError;
          asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s::pilatusRead pilatusROIYMin ROI %d=%d\n", pPvt->driver, signal, *pivalue);
          break;

        case pilatusROIYMaxCommand:
          /* Set ROI Ymax */
          *pivalue = pPvt->ROIs[signal].ymax;
          /* If this is an invalid ROI (min>=max) return error so init_record does not load garbage */
          if (pPvt->ROIs[signal].ymin >= pPvt->ROIs[signal].ymax) status = asynError;
          asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s::pilatusRead pilatusROIYMax ROI %d=%d\n", pPvt->driver, signal, *pivalue);
          break;

        case pilatusROIBgdWidthCommand:
          /* Set ROI bgdWidth */
          *pivalue = pPvt->ROIs[signal].bgdWidth;
          /* If this is an invalid ROI (min>=max) return error so init_record does not load garbage */
          if ((pPvt->ROIs[signal].xmin >= pPvt->ROIs[signal].xmax) ||
              (pPvt->ROIs[signal].ymin >= pPvt->ROIs[signal].ymax)) status = asynError;
          asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s::pilatusRead pilatusROIBgdWidth ROI %d=%d\n", pPvt->driver, signal, *pivalue);
          break;

        default:
          asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "%s::pilatusRead got illegal command %d\n",
              pPvt->driver, command);
          status = asynError;
          break;
    }
    return(status);
}


static asynStatus getBounds(void *drvPvt, asynUser *pasynUser,
    epicsInt32 *low, epicsInt32 *high)
{
    *low = 0;
    *high = 0;
    return(asynSuccess);
}


static asynStatus int32ArrayRead(
    void *drvPvt, 
    asynUser *pasynUser,
    epicsInt32 *data, 
    size_t maxChans, 
    size_t *nactual)
{
    pilatusPvt *pPvt = (pilatusPvt *)drvPvt;
    pilatusROIStruct *roi;
    int nroi;
    int signal;
    int maxSignals = pPvt->maxSignals;
    int numSamples;
    int command = pasynUser->reason;
    asynStatus status = asynSuccess;
    epicsInt32 *pBuff = data;
    epicsUInt32 *pPvtBuff;
    int i;

    pasynManager->getAddr(pasynUser, &signal);

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s::int32ArrayRead entry, signal=%d, maxChans=%d\n", 
        pPvt->driver, signal, maxChans);

    switch (command) {
        case mcaData:
          /* Update the private data buffer by reading the data from the FIFO memory */
          checkAcquireComplete(pPvt, pasynUser);

          /* Transfer the data from the private driver structure to the supplied data
           * buffer. The private data structure will have the information for all the
           * signals, so we need to just extract the signal being requested.
           */
          for (pPvtBuff = pPvt->buffer + signal, numSamples = 0; 
              pPvtBuff < pPvt->buffPtr; 
              pPvtBuff += maxSignals, numSamples++)
          {
            *pBuff++ = (epicsInt32) *pPvtBuff;
          }

          asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s::int32ArrayRead [signal=%d]: read %d chans\n", pPvt->driver, signal, numSamples);

          *nactual = numSamples;
          break;

        case scalerReadCommand:
          if (maxChans > 1+pPvt->maxSignals*2) maxChans = 1+pPvt->maxSignals*2;
          data[0] = pPvt->countTime * CHANNEL1_FREQUENCY;
          roi = &pPvt->ROIs[0];
          nroi = maxChans/2;
          for (i=0; i<nroi; i++) {
            data[2*i+1] = roi[i].totalCounts;
            /* The net counts could be negative.  The scaler uses unsigned numbers */
            if (roi[i].netCounts >= 0)
                data[2*i+2] = roi[i].netCounts;
            else
                data[2*i+2] = 0;
          }
          *nactual = maxChans;
          asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s::int32ArrayRead scalerReadCommand: read %d chans, channel[0]=%d\n", 
              pPvt->driver, maxChans, data[0]);
          break;

        default:
          asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "%s::int32ArrayRead got illegal command %d\n",
              pPvt->driver, command);
          status = asynError;
    }
    return(status);
}


static asynStatus int32ArrayWrite(void *drvPvt, asynUser *pasynUser,
    epicsInt32 *data, size_t maxChans)
{
  pilatusPvt *pPvt = (pilatusPvt *)drvPvt;

  /* This function is not implemented in this driver. */
  asynPrint(pasynUser, ASYN_TRACE_ERROR,
      "%s::int32ArrayWrite, write to pilatus not implemented\n", pPvt->driver);
  return(asynError);
}


static asynStatus octetWrite        (void *drvPvt, asynUser *pasynUser,
                                     const char *data, size_t numchars, 
                                     size_t *nbytesTransfered)
{
    return(asynSuccess);
}

static asynStatus octetRead         (void *drvPvt, asynUser *pasynUser,
                                     char *data, size_t maxchars, 
                                     size_t *nbytesTransfered, int *eomReason)
{
    return(asynSuccess);
}


/* asynDrvUser routines */
static asynStatus drvUserCreate(void *drvPvt, asynUser *pasynUser,
    const char *drvInfo,
    const char **pptypeName, size_t *psize)
{
  int i;
  char *pstring;
  pilatusPvt *pPvt = (pilatusPvt *)drvPvt;

  for (i=0; i<MAX_MCA_COMMANDS; i++) {
    pstring = mcaCommands[i].commandString;
    if (epicsStrCaseCmp(drvInfo, pstring) == 0) {
      pasynUser->reason = mcaCommands[i].command;
      if (pptypeName) *pptypeName = epicsStrDup(pstring);
      if (psize) *psize = sizeof(mcaCommands[i].command);
      asynPrint(pasynUser, ASYN_TRACE_FLOW,
          "%s::drvUserCreate, command=%s\n", pPvt->driver, pstring);
      return(asynSuccess);
    }
  }
  for (i=0; i<MAX_SCALER_COMMANDS + MAX_PILATUS_COMMANDS; i++) {
    pstring = pilatusCommands[i].commandString;
    if (epicsStrCaseCmp(drvInfo, pstring) == 0) {
      pasynUser->reason = pilatusCommands[i].command;
      if (pptypeName) *pptypeName = epicsStrDup(pstring);
      if (psize) *psize = sizeof(pilatusCommands[i].command);
      asynPrint(pasynUser, ASYN_TRACE_FLOW,
          "drvSweep::drvUserCreate, command=%s\n", pPvt->driver, pstring);
      return(asynSuccess);
    }
  }
  asynPrint(pasynUser, ASYN_TRACE_ERROR,
      "%s::drvUserCreate, unknown command=%s\n", pPvt->driver, drvInfo);
  return(asynError);
}

static asynStatus drvUserGetType(void *drvPvt, asynUser *pasynUser,
    const char **pptypeName, size_t *psize)
{
  int command = pasynUser->reason;

  *pptypeName = NULL;
  *psize = 0;
  if (pptypeName)
    *pptypeName = epicsStrDup(mcaCommands[command].commandString);
  if (psize) *psize = sizeof(command);
  return(asynSuccess);
}

static asynStatus drvUserDestroy(void *drvPvt, asynUser *pasynUser)
{
  return(asynSuccess);
}



/***********************/
/* asynCommon routines */
/***********************/

/* Report  parameters */
static void pilatusReport(void *drvPvt, FILE *fp, int details)
{
    pilatusPvt *pPvt = (pilatusPvt *)drvPvt;
    pilatusROIStruct *roi;
    int i;

    assert(pPvt);
    fprintf(fp, "%s: port %s, connected to camserver port %s\n", 
        pPvt->driver, pPvt->portName, pPvt->tcpPortName);
    if (details>0) {
        fprintf(fp, "    maxSignals=%d\n", pPvt->maxSignals);
        fprintf(fp, "    maxChans=%d\n", pPvt->maxChans);
        fprintf(fp, "    imageXSize=%d\n", pPvt->imageXSize);
        fprintf(fp, "    imageYSize=%d\n", pPvt->imageYSize);
        fprintf(fp, "    filename=%s\n", pPvt->filename);
        fprintf(fp, "    fullFilename=%s\n", pPvt->fullFilename);
    }
    if (details>1) {
        for (i=0; i<pPvt->maxSignals; i++) {
            roi = &pPvt->ROIs[i];
            if ((roi->xmax > roi->xmin) && (roi->ymax > roi->ymin)) {
                fprintf(fp, "    ROI[%d]\n", i);   
                fprintf(fp, "        xmin=%d\n",       roi->xmin);
                fprintf(fp, "        xmax=%d\n",       roi->xmax);
                fprintf(fp, "        ymin=%d\n",       roi->ymin);
                fprintf(fp, "        ymax=%d\n",       roi->ymax);
                fprintf(fp, "        bgdWidth=%d\n",   roi->bgdWidth);
                fprintf(fp, "        totalCounts=%f\n", roi->totalCounts);
                fprintf(fp, "        netCounts=%f\n",   roi->netCounts);
            }
        }
    }
}

/* Connect */
static asynStatus pilatusConnect(void *drvPvt, asynUser *pasynUser)
{
  pilatusPvt *pPvt = (pilatusPvt *)drvPvt;
  int signal;
  
  pasynManager->getAddr(pasynUser, &signal);
  if (signal < pPvt->maxSignals) {
    pasynManager->exceptionConnect(pasynUser);
    return(asynSuccess);
  } else {
    return(asynError);
  }
}

/* Disconnect */
static asynStatus pilatusDisconnect(void *drvPvt, asynUser *pasynUser)
{
  /* Does nothing for now.  
   * May be used if connection management is implemented */
  pasynManager->exceptionDisconnect(pasynUser);
  return(asynSuccess);
}


/**********************/
/* Internal functions */
/**********************/

static int pilatusErase(pilatusPvt *pPvt)
{
    int i;

    if (pPvt->exists == 0) return (-1);

    /* Erase buffer in driver */

    memset(pPvt->buffer, 0, pPvt->maxSignals*pPvt->nchans*sizeof(pPvt->buffer[0]));

    /* Reset buffer pointer to start of buffer */
    pPvt->buffPtr = pPvt->buffer;
    pPvt->nextChan = 0;
    pPvt->nextSignal = 0;

    /* Reset elapsed times */
    pPvt->elapsedTime = 0.;
    pPvt->elapsedPrevious = 0.;

    /* Reset the elapsed counts */
    for (i=0; i<pPvt->maxSignals; i++) {
        pPvt->elapsedCounts[i] = 0;
        pPvt->ROIs[i].totalCounts = 0;
        pPvt->ROIs[i].netCounts = 0;
    }

    /* Reset the start time.  This is necessary here because we may be
     * acquiring, and AcqOn will not be called. Normally this is set in AcqOn.
     */
    epicsTimeGetCurrent(&pPvt->startTime);

    return (0);
}


static void acquireTask(pilatusPvt *pPvt)
{
    /* This task does the actual communication with camserver, so that the driver
     * does not block.  This is needed because the scaler record requires synchronous device
     * support */
    int status;
    char pvName[MAX_PVNAME_SIZE];

    /* Look up the database addresses of the PVs we need */
    /* Must wait till after iocInit to do this */
    while (!interruptAccept) epicsThreadSleep(.1);
    strcpy(pvName, pPvt->dbPrefix);
    strcat(pvName, "Filename");
    status = dbNameToAddr(pvName, &pPvt->filenameAddr);
    if (status) {
        errlogPrintf("%s::drvPilatusAsynConfig port %s"
                     " can't find PV %s.\n",
                     pPvt->driver, pPvt->portName, pvName);
        return;
    }
    strcpy(pvName, pPvt->dbPrefix);
    strcat(pvName, "FileNumber");
    status = dbNameToAddr(pvName, &pPvt->fileNumberAddr);
    if (status) {
        errlogPrintf("%s::drvPilatusAsynConfig port %s"
                     " can't find PV %s.\n",
                     pPvt->driver, pPvt->portName, pvName);
        return;
    }
    strcpy(pvName, pPvt->dbPrefix);
    strcat(pvName, "FileFormat");
    status = dbNameToAddr(pvName, &pPvt->fileFormatAddr);
    if (status) {
        errlogPrintf("%s::drvPilatusAsynConfig port %s"
                     " can't find PV %s.\n",
                     pPvt->driver, pPvt->portName, pvName);
        return;
    }
    strcpy(pvName, pPvt->dbPrefix);
    strcat(pvName, "AutoIncrement");
    status = dbNameToAddr(pvName, &pPvt->autoIncrementAddr);
    if (status) {
        errlogPrintf("%s::drvPilatusAsynConfig port %s"
                     " can't find PV %s.\n",
                     pPvt->driver, pPvt->portName, pvName);
        return;
    }

    while (1) {
        epicsEventWait(pPvt->acquireStartEventId);
        status = dbGetField(&pPvt->filenameAddr, DBR_STRING, pPvt->filename, NULL, NULL, NULL);
        if (status) {
            errlogPrintf("%s::drvPilatusAsynConfig port %s"
                         " can't get value of filename.\n",
                         pPvt->driver, pPvt->portName);
        }
        status = dbGetField(&pPvt->fileNumberAddr, DBR_LONG, &pPvt->fileNumber, NULL, NULL, NULL);
        if (status) {
            errlogPrintf("%s::drvPilatusAsynConfig port %s"
                         " can't get value of fileNumber.\n",
                         pPvt->driver, pPvt->portName);
        }
        status = dbGetField(&pPvt->fileFormatAddr, DBR_STRING, pPvt->fileFormat, NULL, NULL, NULL);
        if (status) {
            errlogPrintf("%s::drvPilatusAsynConfig port %s"
                         " can't get value of fileFormat.\n",
                         pPvt->driver, pPvt->portName);
        }
        status = dbGetField(&pPvt->autoIncrementAddr, DBR_LONG, &pPvt->autoIncrement, NULL, NULL, NULL);
        if (status) {
            errlogPrintf("%s::drvPilatusAsynConfig port %s"
                         " can't get value of autoIncrement.\n",
                         pPvt->driver, pPvt->portName);
        }
        /* Build the full file name */
        epicsSnprintf(pPvt->fullFilename, sizeof(pPvt->fullFilename), pPvt->fileFormat, 
                     pPvt->filename, pPvt->fileNumber);
        if (pPvt->acquireMode == SCALER_MODE) {
            pilatusErase(pPvt);
            pPvt->acquiring = 1;
            pPvt->prevAcquiring = 1;
            /* Set scaler presets */
            pPvt->countTime = pPvt->scalerPresets[0]/CHANNEL1_FREQUENCY;
            epicsSnprintf(pPvt->sendMessage, sizeof(pPvt->sendMessage),
                "exptime %f\n", pPvt->countTime);
            status = camserverWriteRead(pPvt);
            if (status) goto done;
            epicsSnprintf(pPvt->sendMessage, sizeof(pPvt->sendMessage),
                "exposure %s\n", pPvt->fullFilename);
            status = camserverWriteRead(pPvt);
            if (status) goto done;
            status = camserverRead(pPvt);
            if (status) goto done;
            /* The reply message from camserverRead contains the name of the file just
             * written. */
            status = sscanf(pPvt->replyMessage, "6 OK %s", pPvt->fullFilename);
            status = readTIFF(pPvt, pPvt->fullFilename);
            if (status) goto done;
            computeROIs(pPvt);
            done:
            pPvt->acquiring = 0;
            pPvt->prevAcquiring = 1;
            acquireDoneCallbacks(pPvt);
            if (pPvt->autoIncrement) {
                pPvt->fileNumber++;
                status = dbPutField(&pPvt->fileNumberAddr, DBR_LONG, &pPvt->fileNumber, 1);
                if (status) {
                    errlogPrintf("%s::drvPilatusAsynConfig port %s"
                                " can't put value of fileNumber.\n",
                                 pPvt->driver, pPvt->portName);
                } 
            }
        }
    }
}


static int checkAcquireComplete(pilatusPvt *pPvt, asynUser *pasynUser)
{
  /* Read the data stored in the FIFO to the buffer in the private driver data
   * structure.
   */

  epicsUInt32 *pBuff;
  epicsUInt32 *endBuff;
  int count = 0;
  int signal, i, j;
  epicsTimeStamp now;


  asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            "%s::checkAcquireComplete entering\n", 
            pPvt->driver);

  /* Current end of buffer stored in the private data structure */
  pBuff = pPvt->buffPtr;
  endBuff = pPvt->buffer + (pPvt->maxSignals * pPvt->nchans);

  epicsTimeGetCurrent(&now);

  asynPrint(pasynUser, ASYN_TRACE_FLOW, 
      "%s::checkAcquireComplete enter: acquiring=%d, prevAcquiring=%d\n",
      pPvt->driver,
      pPvt->acquiring,
      pPvt->prevAcquiring);

 
  if (pPvt->acquiring) {
    pPvt->elapsedTime = epicsTimeDiffInSeconds(&now, &pPvt->startTime);
    if ((pPvt->presetReal > 0) && (pPvt->elapsedTime >= pPvt->presetReal)) {
      pPvt->acquiring = 0;
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s::checkAcquireComplete, stopped acquisition by preset real time\n",
        pPvt->driver);
    }
  }


  if (pPvt->acquiring) {
    if ((pPvt->presetLive > 0) && (pPvt->elapsedTime >= pPvt->presetLive)) {
      pPvt->acquiring = 0;
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s::checkAcquireComplete, stopped acquisition by preset live time\n",
        pPvt->driver);
    }
  }

  /* Check that acquisition is complete by buffer pointer.  This ensures
   * that it will be detected even if interrupts are disabled.
   */
  if (pPvt->acquiring) {
    if (pBuff >= endBuff) {
      pPvt->acquiring = 0;
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "%s::checkAcquireComplete, stopped acquisition by buffer pointer = %p\n",
          pPvt->driver, endBuff);
    }
  }

  /* Check that acquisition is complete by preset counts */
  if (pPvt->acquiring) {
    for (signal=0; signal<pPvt->maxSignals; signal++) {
      if (pPvt->presetCounts[signal] > 0) {
        pPvt->elapsedCounts[signal] = 0;
        for (i = pPvt->presetStartChan[signal],
             j = pPvt->presetStartChan[signal] * pPvt->maxSignals + signal;
             i <= pPvt->presetEndChan[signal];
             i++, j += pPvt->maxSignals) {
               pPvt->elapsedCounts[signal] += pPvt->buffer[j];
        }
        if (pPvt->elapsedCounts[signal] >= pPvt->presetCounts[signal]) {
          pPvt->acquiring=0;
          asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            "%s::checkAcquireComplete, stopped acquisition by preset counts\n",
            pPvt->driver);
        }
      }
    }
  }
 
  /* If acquisition just stopped then ... */
  if ((pPvt->prevAcquiring == 1) && (pPvt->acquiring == 0)) {
    /* Turn off hardware acquisition */
    pPvt->elapsedTime = epicsTimeDiffInSeconds(&now, &pPvt->startTime);
    acquireDoneCallbacks(pPvt);
  }
   
  /* Save the new pointer to the start of the empty section of the data buffer */
  pPvt->buffPtr = pBuff;

  asynPrint(pasynUser, ASYN_TRACE_FLOW, 
      "%s::checkAcquireComplete exit: acquiring=%d, prevAcquiring=%d\n",
      pPvt->driver,
      pPvt->acquiring,
      pPvt->prevAcquiring);

  /* Save the acquisition status */
  pPvt->prevAcquiring = pPvt->acquiring;

  return count;
}

static void acquireDoneCallbacks(pilatusPvt *pPvt)
{
    int reason;
    ELLLIST *pclientList;
    interruptNode *pNode;
    asynInt32Interrupt *pint32Interrupt;

    /* Make callbacks to any clients that have requested notification when acquisition completes */
    pasynManager->interruptStart(pPvt->int32InterruptPvt, &pclientList);
    pNode = (interruptNode *)ellFirst(pclientList);
    while (pNode) {
        pint32Interrupt = pNode->drvPvt;
        reason = pint32Interrupt->pasynUser->reason;
        if (((pPvt->acquireMode == MCS_MODE)     && (reason == mcaAcquiring)) || 
            ((pPvt->acquireMode == SCALER_MODE)  && (reason == scalerDoneCommand))) {
            asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
                "%s::checkAcquireComplete, making pint32Interrupt->Callback\n",
                pPvt->driver);
            pint32Interrupt->callback(pint32Interrupt->userPvt,
                pint32Interrupt->pasynUser,
                0);
        }
        pNode = (interruptNode *)ellNext(&pNode->node);
    }
    pasynManager->interruptEnd(pPvt->int32InterruptPvt);
}


static void setAcquireMode(pilatusPvt *pPvt, pilatusAcquireMode acquireMode)
{

    if (pPvt->acquireMode == acquireMode) return;  /* Nothing to do */
    pPvt->acquireMode = acquireMode;

}

static int readTIFF(pilatusPvt *pPvt, char *fileName)
{
    /* Reads a TIFF file into the imageBuffer */
    int fd;
    int count;
    int nread;
    int offset;
    struct stat statBuff;
    epicsTimeStamp tStart, tCheck;
    double deltaTime = 0;
    int status;
    int expectedSize = TIFF_HEADER_SIZE + pPvt->imageXSize*pPvt->imageYSize*sizeof(epicsUInt32);

    fd = open(fileName, O_RDONLY, 0);
    if (fd < 0) {
        asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
            "%s::readTIFF cannot open file %s\n",
            pPvt->driver, fileName);
        return(-1);
    }
    /* At this point we know the file exists, but it may not be completely written yet.
     * Check the file size repeatedly for up to FILE_WRITE_TIMEOUT seconds until it
     * is the expected size */
    epicsTimeGetCurrent(&tStart);
    while (deltaTime < FILE_READ_TIMEOUT) {
        status = fstat(fd, &statBuff);
        if (statBuff.st_size == expectedSize) break;
        epicsThreadSleep(FILE_READ_DELAY);
        epicsTimeGetCurrent(&tCheck);
        deltaTime = epicsTimeDiffInSeconds(&tCheck, &tStart);
        status = -1;
    }
    if (status){
        asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
            "%s::readTIFF timeout waiting for file to be correct size, size=%d/%d %s\n",
            pPvt->driver, statBuff.st_size, expectedSize, fileName);
        return(-1);
    }
    asynPrint(pPvt->pasynUser, ASYN_TRACEIO_DRIVER,
        "Time after camserver said file was written before it is expected size=%f\n", 
        deltaTime);
    offset = lseek(fd, TIFF_HEADER_SIZE, SEEK_SET);
    if (offset < TIFF_HEADER_SIZE) {
        asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
            "%s::readTIFF cannot seek to start of data, file %s\n",
            pPvt->driver, fileName);
        return(-1);
    }
    nread = pPvt->imageXSize * pPvt->imageYSize * sizeof(epicsUInt32);
    count = read(fd, (void *)pPvt->imageBuffer, nread);
    if (count < nread) { 
        asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
            "%s::readTIFF error reading data, count=%d, should be %d, file %s\n",
            pPvt->driver, count, nread, fileName);
        return(-1);
    }
    return(0);
}


static void computeROIs(pilatusPvt *pPvt)
{
    /* Computes the total and net counts in each ROI */
    int i;
    int nx, ny;
    int totalPixels, bgdPixels=0, bgdPixelsTemp;
    double bgdCounts=0, bgdCountsTemp;
    pilatusROIStruct *roi;
    
    for (i=0; i<pPvt->maxSignals; i++) {
        roi = &pPvt->ROIs[i];
        roi->totalCounts = 0;
        roi->netCounts = 0;
        nx = roi->xmax - roi->xmin;
        ny = roi->ymax - roi->ymin;
        if (nx<=0 || ny<=0) continue;
        if (roi->bgdWidth > 0) {
            /* Compute the total background counts */
            /* There are 4 rectangles to compute */
            /* First rectangle is below the ROI */
            computeRectangleCounts(pPvt,
                                   roi->xmin - roi->bgdWidth + 1,
                                   roi->xmax + roi->bgdWidth - 1,
                                   roi->ymin - roi->bgdWidth + 1,
                                   roi->ymin,
                                   &bgdPixelsTemp, &bgdCountsTemp);
            bgdPixels += bgdPixelsTemp;
            bgdCounts += bgdCountsTemp;
            /* Second rectangle is above the ROI */
            computeRectangleCounts(pPvt,
                                   roi->xmin - roi->bgdWidth + 1,
                                   roi->xmax + roi->bgdWidth - 1,
                                   roi->ymax,
                                   roi->ymax + roi->bgdWidth - 1,
                                   &bgdPixelsTemp, &bgdCountsTemp);
            bgdPixels += bgdPixelsTemp;
            bgdCounts += bgdCountsTemp;
            /* Third rectangle is left of the ROI */
            computeRectangleCounts(pPvt,
                                   roi->xmin - roi->bgdWidth + 1,
                                   roi->xmin,
                                   roi->ymin+1,
                                   roi->ymax-1,
                                   &bgdPixelsTemp, &bgdCountsTemp);
            bgdPixels += bgdPixelsTemp;
            bgdCounts += bgdCountsTemp;
            /* Third rectangle is right of the ROI */
            computeRectangleCounts(pPvt,
                                   roi->xmax,
                                   roi->xmax + roi->bgdWidth - 1,
                                   roi->ymin+1,
                                   roi->ymax-1,
                                   &bgdPixelsTemp, &bgdCountsTemp);
            bgdPixels += bgdPixelsTemp;
            bgdCounts += bgdCountsTemp;
        } 
       
        /* Compute the total ROI counts */
        computeRectangleCounts(pPvt,
                               roi->xmin, 
                               roi->xmax,
                               roi->ymin, 
                               roi->ymax,
                               &totalPixels, &roi->totalCounts);
         
        /* Compute the net ROI counts */
        roi->netCounts = roi->totalCounts - (bgdCounts*totalPixels)/bgdPixels;
    }
}


static void computeRectangleCounts(pilatusPvt *pPvt, 
                                   int xmin, int xmax, int ymin, int ymax,
                                   int *nPixels, double *counts)
{
    int ix, iy;
    epicsUInt32 *pRow;

    if (xmin < 0) xmin = 0;
    if (ymin < 0) ymin = 0;
    if (xmax > pPvt->imageXSize - 1) xmax = pPvt->imageXSize - 1;
    if (ymax > pPvt->imageYSize - 1) ymax = pPvt->imageYSize - 1;
    *nPixels = 0;
    *counts = 0;
    for (iy=ymin; iy<=ymax; iy++) {
        pRow = pPvt->imageBuffer + iy*pPvt->imageXSize;
        for (ix=xmin; ix<=xmax; ix++) { 
            (*nPixels)++;
            *counts += pRow[ix];
        }
    }
}


static int camserverWriteRead(pilatusPvt *pPvt)
{
    int status;
    size_t nwrite, nread;
    int eomReason;
    
    status = pasynOctetSyncIO->writeRead(pPvt->pasynUserOctet, 
                                         pPvt->sendMessage, strlen(pPvt->sendMessage),
                                         pPvt->replyMessage, sizeof(pPvt->replyMessage),
                                         WRITE_READ_TIMEOUT,
                                         &nwrite, &nread, &eomReason);
    
    if (status != asynSuccess) {
        asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                 "%s::camserverWriteRead port %s error calling writeRead,"
                 " error=%s, nwrite=%d, nread=%d\n", 
                 pPvt->driver, pPvt->portName, 
                 pPvt->pasynUserOctet->errorMessage, nwrite, nread);
        return(asynError);
    }

    asynPrint(pPvt->pasynUser, ASYN_TRACEIO_DRIVER,
        "%s::camserverWriteRead nwrite=%d, sent=%s\n"
        "    nread=%d, reply=%s\n", 
        pPvt->driver, nwrite, pPvt->sendMessage, 
        nread, pPvt->replyMessage);

    return(asynSuccess);
}

static int camserverRead(pilatusPvt *pPvt)
{
    int status;
    size_t nread;
    int eomReason;
    int i;

    /* This is used to read the reply message when an image is complete.
     * We set the timeout to the count time plus 1 second, and repeat on failure. */

    for (i=0; i<READ_RETRIES; i++) {      
        status = pasynOctetSyncIO->read(pPvt->pasynUserOctet, 
                                        pPvt->replyMessage, sizeof(pPvt->replyMessage),
                                        pPvt->countTime + 1,
                                        &nread, &eomReason);
        if (status == asynSuccess) break;
    }
    
    if (status != asynSuccess) {
        asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                 "%s::camserverRead port %s error calling read,"
                 " error=%s, nread=%d\n", 
                 pPvt->driver, pPvt->portName, 
                 pPvt->pasynUserOctet->errorMessage, nread);
        return(asynError);
    }
    
    asynPrint(pPvt->pasynUser, ASYN_TRACEIO_DRIVER,
        "%s::camserverRead nread=%d, reply=%s\n", 
        pPvt->driver, nread, pPvt->replyMessage);
       
    return(asynSuccess);
}


static const iocshArg ConfigureArg0 = {"Port name",            iocshArgString};
static const iocshArg ConfigureArg1 = {"camserver port name",  iocshArgString};
static const iocshArg ConfigureArg2 = {"Max. # ROIs",          iocshArgInt};
static const iocshArg ConfigureArg3 = {"Max. # MCA channels",  iocshArgInt};
static const iocshArg ConfigureArg4 = {"Image X size",         iocshArgInt};
static const iocshArg ConfigureArg5 = {"Image Y size",         iocshArgInt};
static const iocshArg ConfigureArg6 = {"Database prefix",      iocshArgString};

static const iocshArg * const drvPilatusAsynConfigureArgs[7] = {
	&ConfigureArg0,
	&ConfigureArg1,
	&ConfigureArg2,
	&ConfigureArg3,
	&ConfigureArg4,
	&ConfigureArg5,
	&ConfigureArg6
};

static const iocshFuncDef drvPilatusAsynConfigureFuncDef=
                                                    {"drvPilatusAsynConfigure", 7,
                                                     drvPilatusAsynConfigureArgs};
static void drvPilatusAsynConfigureCallFunc(const iocshArgBuf *args)
{
  drvPilatusAsynConfigure(args[0].sval, args[1].sval, args[2].ival, args[3].ival, 
                          args[4].ival, args[5].ival, args[6].sval);
}


static void drvPilatusAsynRegister(void)
{
  iocshRegister(&drvPilatusAsynConfigureFuncDef,drvPilatusAsynConfigureCallFunc);
}

epicsExportRegistrar(drvPilatusAsynRegister);
