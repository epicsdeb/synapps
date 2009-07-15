program roperCCD("P=roperCCD:,C=ccd1")

/*  This sequencer program works with ccd.db.  
 *  It allows EPICS control of a Roper detector.  
 *  The software talks to the roperServer, which in turn controls WinView 
 *  or WinSpec through the Microsoft COM interface.
 *
 *  NOTE: I am using "sprintf" where I would like to use "snprintf", but
 *        vxWorks does not have snprintf.
 *
 *  Mark Rivers
 *  Oct. 7, 2003
*/

/* This program must be compiled with the recursive option so that multiple 
 * copies can run at once */
option +r;

%%#include <string.h>
%%#include <errlog.h>
%%#include <epicsEvent.h>
%%#include <asynDriver.h>
%%#include <asynOctetSyncIO.h>
#include "epicsCCD.h"
#define MAX_COMMAND_SIZE 128
/* Some commands take a long time to respond - e.g. get_state when 
 * acquisition completes */
#define TIMEOUT 3.0

/* Define escaped C functions at end of file */
%% static int asynConnect(SS_ID ssId, struct UserVar *pVar);
%% static int asynWriteRead(SS_ID ssId, struct UserVar *pVar, char *command);

/* Variables */
double exposureTime;
double totalTime;
double delay;
char   *blank_pos;
char   *portName;
char   buffer[MAX_COMMAND_SIZE];
int    nchars;
int    save_single;
int    status;
int    validPath;
int    AsynPortIndex;
int    DetOutStrIndex;
int    DetInStrIndex;
/* Note, have to fake out the following definitions, because we need these
 * variables to be put into the UserVar structure (for +r option), but SNL
 * only knows about char*, not these more complex things */
char   *startTime;    /* Really epicsTimeStamp* */
char   *currentTime;  /* Really epicsTimeStamp* */
char   *abortEvent;   /* Really epicsEventId */
char   *pasynUser;    /* Really asynUser */

ss main {
    state init {
        when() {
            efClear(SetTemp_mon);
            efClear(AcquireCLBK_mon);
            efClear(AcquirePOLL_mon);
            efClear(Abort_mon);
            efClear(SaveFile_mon);
            efClear(ServerName_mon);
            efClear(ServerPort_mon);
            efClear(BinX_mon);
            efClear(BinY_mon);
            efClear(ROITop_mon);
            efClear(ROIBottom_mon);
            efClear(ROILeft_mon);
            efClear(ROIRight_mon);
            efClear(Hours_mon);
            efClear(Minutes_mon);
            efClear(Seconds_mon);
            efClear(Milliseconds_mon);
            efSet(ShutterStatLink_mon); /* Copy link first time through */
            efSet(PollDetState_mon); /* Poll detector first time through */
            Initialize = 1; pvPut(Initialize); /* Force initialize first time */
            strncpy(CCDManufacturer, "Roper", sizeof(CCDManufacturer))
            ; pvPut(CCDManufacturer);
            strncpy(CCDModel, "WinView/WinSpec", sizeof(CCDModel)); 
            pvPut(CCDModel);
            BitDepth = 16; pvPut(BitDepth);
            AsynPortIndex = pvIndex(AsynPort);
            DetOutStrIndex = pvIndex(DetOutStr);
            DetInStrIndex = pvIndex(DetInStr);
            %%pVar->abortEvent = (char *) epicsEventCreate(epicsEventEmpty);
            /* Allocate epicsTimeStamp objects */
            %%pVar->startTime   = (char *) calloc(1, sizeof(epicsTimeStamp));
            %%pVar->currentTime = (char *) calloc(1, sizeof(epicsTimeStamp));
        } state wait_connect
    }

    state wait_connect {
        entry {
            ConnectState = STATE_DISCONNECTED;
            pvPut(ConnectState);
        }
        when((ConnectState == STATE_DISCONNECTED) &&
             ((efTestAndClear(Initialize_mon) && Initialize == 1) ||
              (efTestAndClear(ServerName_mon)) ||
              (efTestAndClear(ServerPort_mon)))) {
            /* Set the PV back to 0 */
            Initialize = 0; pvPut(Initialize);
            /* Try to create socket connection to server */
            %%pVar->status = asynConnect(ssId, pVar);
            if (status == 0) ConnectState = STATE_CONNECTED;
            else ConnectState = STATE_DISCONNECTED;
            pvPut(ConnectState);
        } state wait_connect

        when(ConnectState == STATE_CONNECTED) {
            %%asynWriteRead(ssId, pVar, "get_controller");
            strncpy(CCDModel, buffer, sizeof(CCDModel)); pvPut(CCDModel);
        } state monitor_changes
    }

    state monitor_changes {
        when((ConnectState == STATE_DISCONNECTED) ||
             (efTest(Initialize_mon) && Initialize == 1) ||
             (efTest(ServerName_mon)) ||
             (efTest(ServerPort_mon))) {
        } state wait_connect

        when(efTestAndClear(PollDetState_mon)) { 
            %%asynWriteRead(ssId, pVar, "get_state");
            DetectorState = atoi(buffer); pvPut(DetectorState);
            %%asynWriteRead(ssId, pVar, "get_binning");
            sscanf(buffer, "%d,%d", &ActualBinX, &ActualBinY);
            pvPut(ActualBinX); pvPut(ActualBinY);
            %%asynWriteRead(ssId, pVar, "get_temperature");
            sscanf(buffer, "%lf", &MeasuredTemp);
            pvPut(MeasuredTemp);
            %%asynWriteRead(ssId, pVar, "get_exposure");
            sscanf(buffer, "%lf", &ActualSeconds);
            pvPut(ActualSeconds);
            %%asynWriteRead(ssId, pVar, "get_nframes");
            sscanf(buffer, "%d", &ActualNumFrames);
            pvPut(ActualNumFrames);
            %%asynWriteRead(ssId, pVar, "get_roi");
            sscanf(buffer, "%d,%d,%d,%d", 
                   &ActualROITop,  &ActualROIBottom, 
                   &ActualROILeft, &ActualROIRight);
            pvPut(ActualROITop);
            pvPut(ActualROIBottom);
            pvPut(ActualROILeft);
            pvPut(ActualROIRight);
        } state monitor_changes

        when(efTestAndClear(ShutterStatLink_mon)) {
            /* Copy the link to the shutter PV, but strip off CP MS*/
            blank_pos = strchr(ShutterStatLink, ' ');
            if (blank_pos != 0) nchars = blank_pos-ShutterStatLink; 
            else nchars=strlen(ShutterStatLink);
            strncpy(buffer, ShutterStatLink, sizeof(buffer));
            buffer[nchars] = 0;
            pvAssign(Shutter, buffer);
            /* Process the shutter status PV so the link status will change */
            /* Need to wait a little while for link to establish before */
            /* processing status record */
            epicsThreadSleep(0.1);
            ShutterStatProc = 1; pvPut(ShutterStatProc);
        } state monitor_changes

        when(efTestAndClear(OpenShutter_mon) && 
                            OpenShutter==1) {
            /* EPICS PV for the shutter, or the Roper shutter? */
            if (ShutterMode == SHUTTER_EPICS) { 
                strncpy(Shutter, OpenShutterStr, sizeof(Shutter)); 
                pvPut(Shutter);
            } else if (ShutterMode == SHUTTER_CAMERA) {
                %%asynWriteRead(ssId, pVar, "shutter,1");
            }
            /* Set the PV back to 0 */
            OpenShutter = 0; pvPut(OpenShutter);
        } state monitor_changes

        when(efTestAndClear(CloseShutter_mon) && 
                            CloseShutter==1) {
            /* EPICS PV for the shutter, or Roper shutter? */
            if (ShutterMode == SHUTTER_EPICS) { 
                strncpy(Shutter, CloseShutterStr, sizeof(Shutter)); 
                pvPut(Shutter);
            } else if (ShutterMode == SHUTTER_CAMERA) {
                %%asynWriteRead(ssId, pVar, "shutter,0");
            }
            /* Set the PV back to 0 */
            CloseShutter = 0; pvPut(CloseShutter);
        } state monitor_changes

        when((efTestAndClear(BinX_mon)) ||
             (efTestAndClear(BinY_mon))) {
            /* Set binning */
            sprintf(buffer, "set_binning,%d,%d", BinX, BinY);
            %%asynWriteRead(ssId, pVar, pVar->buffer);
        } state monitor_changes

        when((efTestAndClear(Hours_mon)) ||
             (efTestAndClear(Minutes_mon)) ||
             (efTestAndClear(Seconds_mon)) ||
             (efTestAndClear(Milliseconds_mon))) {
            /* Start acquisition */
            exposureTime = Hours*3600. + Minutes*60. + Seconds + 
                           Milliseconds/1000.;
            sprintf(buffer, "set_exposure,%f", exposureTime);
            %%asynWriteRead(ssId, pVar, pVar->buffer);
        } state monitor_changes

        when((efTestAndClear(ROITop_mon)) ||
             (efTestAndClear(ROIBottom_mon)) ||
             (efTestAndClear(ROILeft_mon)) ||
             (efTestAndClear(ROIRight_mon))) {
            /* Set ROI */
            sprintf(buffer, "set_roi,%d,%d,%d,%d", 
                    ROITop, ROIBottom, ROILeft, ROIRight);
            %%asynWriteRead(ssId, pVar, pVar->buffer);
        } state monitor_changes

        when(efTestAndClear(SetTemp_mon)) {
            /* Send set temperature command */
            sprintf(buffer, "set_temperature,%f", SetTemp);
            %%asynWriteRead(ssId, pVar, pVar->buffer);
        } state monitor_changes

        when(efTestAndClear(NumFrames_mon)) {
            /* Send number of frames command */
            sprintf(buffer, "set_nframes,%d", NumFrames);
            %%asynWriteRead(ssId, pVar, pVar->buffer);
        } state monitor_changes

        when(efTestAndClear(FilePath_mon)) {
            /* Make sure path has a trailing \ */
/* THIS IS BROKEN DUE TO BUG IN SNC 
            if ((FilePath[strlen(FilePath)-1] != '\\') &&
                (strlen(FilePath) < sizeof(FilePath)-2)) {
               strcat(FilePath, "\\");
               pvPut(FilePath);
            }
*/
        } state monitor_changes

        when(efTestAndClear(SaveFile_mon) && 
                            SaveFile==1) {
            /* Save file */
            save_single = 1;
        } state save_file

        when((efTestAndClear(AcquireCLBK_mon) && AcquireCLBK==1) ||
             (efTestAndClear(AcquirePOLL_mon) && AcquirePOLL==1)) {
            /* Start acquisition */
            exposureTime = Hours*3600. + Minutes*60. + Seconds + 
                           Milliseconds/1000.;
            totalTime = exposureTime;
            if (ShutterMode == SHUTTER_EPICS) {
               totalTime += OpenShutterDly + CloseShutterDly;
            }
            sprintf(buffer, "set_exposure,%f", totalTime);
            %%asynWriteRead(ssId, pVar, pVar->buffer);
            DetectorState = DETECTOR_ACQUIRE; pvPut(DetectorState);
            /* Set the the start time for the TimeRemaining counter */
            %%epicsTimeGetCurrent((epicsTimeStamp *) pVar->startTime);
            TimeRemaining = exposureTime; pvPut(TimeRemaining);
            if (FrameType != FRAME_NORMAL) {
                /* Background and flat field save file immediately, so we */
                /* need to build file name here */
                sprintf(FullFilename, FilenameFormat, FileTemplate, SeqNumber);
                pvPut(FullFilename);
                SeqNumber++; pvPut(SeqNumber);
            }
            if (FrameType == FRAME_NORMAL) 
                sprintf(buffer, "start,%d,%d", 
                        CorrectBackground, CorrectFlatfield);
            else if (FrameType == FRAME_BACKGROUND)
                sprintf(buffer, "acquire_background,%s%s", 
                        FilePath, FullFilename);
            else if (FrameType == FRAME_FLATFIELD) 
                sprintf(buffer, "acquire_flatfield,%s%s", 
                        FilePath, FullFilename);
            %%asynWriteRead(ssId, pVar, pVar->buffer);
            if (ShutterMode == SHUTTER_EPICS) {
                epicsThreadSleep(OpenShutterDly); 
                strncpy(Shutter, OpenShutterStr, sizeof(Shutter)); 
                        pvPut(Shutter, SYNC);
                /* Wait for the exposure time using */
                /* epicsEventWaitWithTimeout, so we can abort */
                %%epicsEventWaitWithTimeout((epicsEventId) pVar->abortEvent, 
                %%                          pVar->exposureTime);
                /* Close shutter */
                strncpy(Shutter, CloseShutterStr, sizeof(Shutter)); 
                pvPut(Shutter, SYNC);
                epicsThreadSleep(CloseShutterDly);
            }
        } state wait_readout
    }

    state wait_readout {
        when (DetectorState != DETECTOR_IDLE && 
              delay(0.05)) {
            /* Read the status */
            %%asynWriteRead(ssId, pVar, "get_state");
            DetectorState = atoi(buffer); pvPut(DetectorState);
        } state wait_readout

        when (DetectorState == DETECTOR_IDLE) {
            if (ComputeROICts) {
               %%asynWriteRead(ssId, pVar, "get_roi_counts");
               sscanf(buffer, "%lf,%lf", &ROITotal, &ROINet);
               pvPut(ROITotal); pvPut(ROINet);
            }
        } state save_file
    }

    state save_file {
        when() {
            if ((AutoSave && (FrameType == FRAME_NORMAL)) || save_single) {
                /* Send command commands */
                sprintf(buffer, "set_comment,1,%s", Comment1);
                %%asynWriteRead(ssId, pVar, pVar->buffer);
                sprintf(buffer, "set_comment,2,%s", Comment2);
                %%asynWriteRead(ssId, pVar, pVar->buffer);
                sprintf(buffer, "set_comment,3,%s", Comment3);
                %%asynWriteRead(ssId, pVar, pVar->buffer);
                sprintf(buffer, "set_comment,4,%s", Comment4);
                %%asynWriteRead(ssId, pVar, pVar->buffer);
                sprintf(buffer, "set_comment,5,%s", Comment5);
                %%asynWriteRead(ssId, pVar, pVar->buffer);
                /* Save file */
                sprintf(FullFilename, FilenameFormat, FileTemplate, SeqNumber);
                pvPut(FullFilename);
                SeqNumber++; pvPut(SeqNumber);
                sprintf(buffer, "save,%s%s", FilePath, 
                         FullFilename);
                %%asynWriteRead(ssId, pVar, pVar->buffer);
            }
            /* Set PVs back to 0 */
            if (save_single) {
                save_single = 0;
                /* Set the PV back to 0 */
                SaveFile = 0; pvPut(SaveFile);
            } else {
                AcquireCLBK = 0; pvPut(AcquireCLBK);
                AcquirePOLL = 0; pvPut(AcquirePOLL);
            }
        } state monitor_changes
    }
}


/* This is a separate state set so the watchdog runs even during long 
 * acquisitions */
ss watchdog {
    state poll {
        when(delay(1.0) && SNLWatchdog < 5) {
            SNLWatchdog = 5; pvPut(SNLWatchdog);
        } state poll
    }
}

/* This is a separate state set so we can abort during the 
 * epicsEventWaitWithTimeout that times the shutter */
ss abort {
    state poll {
        when(Abort_mon && Abort==1) {
            %%asynWriteRead(ssId, pVar, "abort");
            %%epicsEventSignal((epicsEventId) pVar->abortEvent);
            Abort=0; pvPut(Abort);
            TimeRemaining=0.; pvPut(TimeRemaining);
        } state poll
    }
}

/* This is a separate state set so we update the time remaining during 
*  the epicsEventWaitWithTimeout that times the shutter */
ss timeRemaining {
    state poll {
        when(delay(.1) && TimeRemaining != 0.) {
            %%epicsTimeGetCurrent((epicsTimeStamp *) pVar->currentTime);
            %%pVar->TimeRemaining = pVar->exposureTime -
            %%      epicsTimeDiffInSeconds(
            %%           (epicsTimeStamp *)pVar->currentTime,
            %%           (epicsTimeStamp *)pVar->startTime);
            if (TimeRemaining < 0.) TimeRemaining = 0.;
            pvPut(TimeRemaining);
        } state poll
    }
}

/* Begin C code functions */
%{

static int asynConnect(SS_ID ssId, struct UserVar *pVar)
{
   asynStatus status;
   char *portString;

   status = pasynOctetSyncIO->openSocket(pVar->ServerName, pVar->ServerPort,
                                    &portString);
   /* openSocket can fail just because the port already existed, 
    * ignore error */
   status = pasynOctetSyncIO->connect(portString, 0, (asynUser **)&pVar->pasynUser, NULL);
   if (status != asynSuccess) return(status);
   status = pasynOctetSyncIO->setInputEos((asynUser *)pVar->pasynUser, "\r\n", 2);
   pasynTrace->setTraceIOMask((asynUser *)pVar->pasynUser, ASYN_TRACEIO_ESCAPE);
   /* Also connect the asynRecord for user interaction */
   strncpy(pVar->AsynPort, portString, sizeof(pVar->AsynPort));
   seq_pvPut(ssId, pVar->AsynPortIndex, 0);
   return(status);
}

static int asynWriteRead(SS_ID ssId, struct UserVar *pVar, char *command)
{
   int nwrite, nread;
   asynStatus status;
   int eomReason;

   if (pVar->ConnectState != STATE_CONNECTED) return(-1);
   status = pasynOctetSyncIO->writeRead((asynUser *)pVar->pasynUser, command,
                                         strlen(command), pVar->buffer,
                                         sizeof(pVar->buffer), TIMEOUT,
                                         &nwrite, &nread, &eomReason);

   /* Copy output string to PV so user can see it */
   strncpy(pVar->DetOutStr, command, sizeof(pVar->DetOutStr));
   seq_pvPut(ssId, pVar->DetOutStrIndex, 0);
   /* Copy input string to PV so user can see it */
   strncpy(pVar->DetInStr, pVar->buffer, sizeof(pVar->DetInStr));
   seq_pvPut(ssId, pVar->DetInStrIndex, 0);
   return(nread);
}
}%
