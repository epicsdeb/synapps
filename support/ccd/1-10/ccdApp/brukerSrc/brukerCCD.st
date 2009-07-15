program marCCD("P=marCCD:,C=ccd1")

/*  This sequencer program works with ccd.db.  
 *  It allows EPICS control of the MAR 165 CCD detector.  The detector must be put in remote
 *  mode, running the marccd_server_socket program.
 *
 *  Mark Rivers
 *  Sept 27, 2003
*/

/* This program must be compiled with the recursive option so that multiple copies
 * can run at once */
option +r;

%%#include <string.h>
%%#include <sys/types.h>
%%#include <dirent.h>
%%#include <socket_utils.h>
%%#include <errlog.h>
%%#include <epicsTime.h>
%%#include <epicsEvent.h>
%%#include <epicsPrint.h>
#include "epicsCCD.h"
#define MAX_COMMAND_SIZE 200

/* Define escaped C functions at end of file */
%% static int send_command(SS_ID ssId, struct UserVar *pVar, char *command);
%% static int read_response(SS_ID ssId, struct UserVar *pVar, int poll);

/* Variables */
int    socket;
char   buffer[MAX_COMMAND_SIZE];
double exposureTime;
double delay;
char   *blank_pos;
int    nchars;
int    save_single;
int    status;
int    validPath;
int    DetInStrIndex;
int    DetOutStrIndex;
%%epicsTimeStamp startTime, currentTime;
%%epicsEventId abortEvent;

ss main {
    state init {
        when() {
            socket = -1;
            efClear(SetTemp_mon);
            efClear(AcquireCLBK_mon);
            efClear(AcquirePOLL_mon);
            efClear(Abort_mon);
            efClear(SaveFile_mon);
            efClear(ServerName_mon);
            efClear(ServerPort_mon);
            strncpy(Comment1Desc, "Det. Dist. (mm)", sizeof(Comment1Desc));  pvPut(Comment1Desc);
            strncpy(Comment2Desc, "Rotation axis", sizeof(Comment2Desc));    pvPut(Comment2Desc);
            strncpy(Comment3Desc, "Wavelength", sizeof(Comment3Desc));       pvPut(Comment3Desc);
            strncpy(Comment4Desc, "File comment", sizeof(Comment4Desc));     pvPut(Comment4Desc);
            strncpy(Comment5Desc, "Dataset comment", sizeof(Comment5Desc));  pvPut(Comment5Desc);
            efSet(ShutterStatLink_mon); /* Copy link first time through */
            ConnectState = STATE_DISCONNECTED; pvPut(ConnectState);
            Initialize = 1; pvPut(Initialize); /* Force initialize first time through */
            strncpy(CCDManufacturer, "MAR", sizeof(CCDManufacturer)); pvPut(CCDManufacturer);
            strncpy(CCDModel, "165", sizeof(CCDModel)); pvPut(CCDModel);
            MeasuredTemp = 25; pvPut(MeasuredTemp);
            BitDepth = 16; pvPut(BitDepth);
            DetInStrIndex = pvIndex(DetInStr);
            DetOutStrIndex = pvIndex(DetOutStr);
            %%abortEvent = epicsEventCreate(epicsEventEmpty);
        } state wait_connect
    }

    state wait_connect {
        when(delay(1) && ConnectState == STATE_DISCONNECTED) {
            /* Close any open socket *
            if (socket >= 0) close(socket); 
            /* Try to open socket connection to server */
            socket = open_socket(ServerName, ServerPort);
            /* Set the Connected PV to indicate if it was successful */
            if (socket < 0) ConnectState = STATE_DISCONNECTED;
            else            ConnectState = STATE_CONNECTED;
            pvPut(ConnectState);
        } state wait_connect

        when(ConnectState == STATE_CONNECTED) {
        } state monitor_changes
    }

    state monitor_changes {
        when((ConnectState == STATE_DISCONNECTED) ||
             (efTestAndClear(Initialize_mon) && Initialize == 1) ||
             (efTestAndClear(ServerName_mon)) ||
             (efTestAndClear(ServerPort_mon))) {
            /* Set the PV back to 0 */
            Initialize = 0; pvPut(Initialize);
        } state wait_connect

        when(efTestAndClear(PollDetState_mon)) { 
            %%send_command(ssId, pVar, "get_state\n");
            %%read_response(ssId, pVar, 0);
            DetectorState = atoi(buffer); pvPut(DetectorState);
            %%send_command(ssId, pVar, "get_bin\n");
            %%read_response(ssId, pVar, 0);
            sscanf(buffer, "%d,%d", &ActualBinX, &ActualBinY);
            pvPut(ActualBinX); pvPut(ActualBinY);
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
            /* Need to wait a little while for link to establish before processing status record */
            epicsThreadSleep(0.1);
            ShutterStatProc = 1; pvPut(ShutterStatProc);
        } state monitor_changes

        when(efTestAndClear(OpenShutter_mon) && 
                            OpenShutter==1) {
            /* Are we using an EPICS PV for the shutter, or the MAR 165 shutter? */
            if (ShutterMode == SHUTTER_EPICS) { 
                strncpy(Shutter, OpenShutterStr, sizeof(Shutter)); pvPut(Shutter);
            } else if (ShutterMode == SHUTTER_CAMERA) {
                %%send_command(ssId, pVar, "shutter,1\n");
            }
            /* Set the PV back to 0 */
            OpenShutter = 0; pvPut(OpenShutter);
        } state monitor_changes

        when(efTestAndClear(CloseShutter_mon) && 
                            CloseShutter==1) {
            /* Are we using an EPICS PV for the shutter, or the MAR 165 shutter? */
            if (ShutterMode == SHUTTER_EPICS) { 
                strncpy(Shutter, CloseShutterStr, sizeof(Shutter)); pvPut(Shutter);
            } else if (ShutterMode == SHUTTER_CAMERA) {
                %%send_command(ssId, pVar, "shutter,0\n");
            }
            /* Set the PV back to 0 */
            CloseShutter = 0; pvPut(CloseShutter);
        } state monitor_changes

        when((efTestAndClear(BinX_mon)) ||
             (efTestAndClear(BinY_mon))) {
            /* Set binning */
            epicsSnPrintf(buffer, sizeof(buffer), "set_bin,%d,%d\n", BinX, BinY);
            %%send_command(ssId, pVar, pVar->buffer);
        } state monitor_changes

        when(efTestAndClear(UserInStr_mon)) {
            /* Send user-input command */
            %%send_command(ssId, pVar, pVar->UserInStr);
            strcpy(buffer, "");
            /* Wait .5 second, read response if any */
            epicsThreadSleep(0.5);
            %%read_response(ssId, pVar, 1);
            strncpy(DetInStr, buffer, sizeof(DetInStr));
            pvPut(DetInStr);
        } state monitor_changes

        when(efTestAndClear(FilePath_mon)) {
            /* Make sure path has a trailing / */
            if ((FilePath[strlen(FilePath)-1] != '/') &&
                (strlen(FilePath) < sizeof(FilePath)-2)) {
               strcat(FilePath, "/");
               pvPut(FilePath);
            }
            /* Check that this is a valid path */
/*           validPath = 1;
           %%if (opendir(pVar->FilePath) == NULL) pVar->validPath = 0;
           if (validPath == 0) {
               strcpy(FilePath, "./");
               pvPut(FilePath);
            }
*/
        } state monitor_changes

        when(efTestAndClear(SaveFile_mon) && 
                            SaveFile==1) {
            /* Set the PV back to 0 */
            SaveFile = 0; pvPut(SaveFile);
            /* Save file */
            save_single = 1;
        } state save_file

        when((efTestAndClear(AcquireCLBK_mon) && AcquireCLBK==1) ||
             (efTestAndClear(AcquirePOLL_mon) && AcquirePOLL==1)) {
            /* Start acquisition */
            exposureTime = Hours*3600. + Minutes*60. + Seconds + Milliseconds/1000.;
            /* Minimum exposure time of 1msec, because the readout command is not seen if it */
            /* comes after the start command with no delay */
            if (exposureTime < .001) exposureTime = .001;
            %%send_command(ssId, pVar, "start\n");
        } state wait_acquire_start
    }

    state wait_acquire_start {
        when (DetectorState != DETECTOR_ACQUIRE && 
              delay(0.05)) {
            /* Read the status */
            %%send_command(ssId, pVar, "get_state\n");
            %%read_response(ssId, pVar, 0);
            DetectorState = atoi(buffer); pvPut(DetectorState);
        } state wait_acquire_start

        when (DetectorState == DETECTOR_ACQUIRE) {
        } state do_acquire
    }

    state do_acquire {
        when () {
            /* Set the the start time for the TimeRemaining counter */
            %%epicsTimeGetCurrent(&startTime);
            TimeRemaining = exposureTime; pvPut(TimeRemaining);
            /* Open the shutter */
            /* Are we using an EPICS PV for the shutter, or the MAR 165 shutter? */
            if (ShutterMode == SHUTTER_EPICS) { 
                strncpy(Shutter, OpenShutterStr, sizeof(Shutter)); pvPut(Shutter);
            } else if (ShutterMode == SHUTTER_CAMERA) {
                %%send_command(ssId, pVar, "shutter,1\n");
            }
            /* This delay is to get the exposure time correct.  It is equal to the opening time of the shutter minus the
             * closing time.  If they are equal then no delay is needed, except use 1msec so delay is not negative and 
             * commands are not back-to-back */
            delay = OpenShutterDly-CloseShutterDly;
            if (delay < .001) delay=.001;
            epicsThreadSleep(delay);
            /* Wait for the exposure time using epicsEventWaitWithTimeout, so we can abort */
            %%epicsEventWaitWithTimeout(abortEvent, pVar->exposureTime);
            /* Close shutter */
            /* Are we using an EPICS PV for the shutter, or the MAR 165 shutter? */
            if (ShutterMode == SHUTTER_EPICS) { 
                strncpy(Shutter, CloseShutterStr, sizeof(Shutter)); pvPut(Shutter);
            } else if (ShutterMode == SHUTTER_CAMERA) {
                %%send_command(ssId, pVar, "shutter,0\n");
            }
            epicsThreadSleep(CloseShutterDly);
            /* Start readout */
            if (FrameType == FRAME_BACKGROUND) %%send_command(ssId, pVar, "readout,1\n");
            else                               %%send_command(ssId, pVar, "readout,0\n");
            DetectorState = DETECTOR_READOUT; pvPut(DetectorState);
            /* Detector sends 0 when readout is done */
            %%read_response(ssId, pVar, 0);
            DetectorState = atoi(buffer); pvPut(DetectorState);
        } state wait_readout
    }

    state wait_readout {
        when (DetectorState != DETECTOR_IDLE && 
              delay(0.05)) {
            /* Read the status */
            %%send_command(ssId, pVar, "get_state\n");
            %%read_response(ssId, pVar, 0);
            DetectorState = atoi(buffer); pvPut(DetectorState);
        } state wait_readout

        when (DetectorState == DETECTOR_IDLE) {
        } state save_file
    }

    state save_file {
        when() {
            if (AutoSave || save_single) {
                /* Construct header command */
                epicsSnPrintf(buffer, sizeof(buffer), "header,detector_distance=%s,exposure_time=%f,rotation_axis=%s\n",
                                                         Comment1,            exposureTime,    Comment2);
                %%send_command(ssId, pVar, pVar->buffer);
                epicsSnPrintf(buffer, sizeof(buffer), "header,source_wavelength=%s,file_comments=%s,dataset_comments=%s\n",
                                                         Comment3,            Comment4,        Comment5);
                %%send_command(ssId, pVar, pVar->buffer);
                /* Save file */
                epicsSnPrintf(FullFilename, sizeof(FullFilename, FilenameFormat, FileTemplate, SeqNumber);
                pvPut(FullFilename);
                SeqNumber++; pvPut(SeqNumber);
                epicsSnPrintf(buffer, sizeof(buffer), "writefile,%s%s,%d\n", FilePath, FullFilename, FileType);
                /* Without the following 20msec delay we get an access fault in listen_socket().*/
                epicsThreadSleep(.02);
                %%send_command(ssId, pVar, pVar->buffer);
                DetectorState = DETECTOR_WRITING; pvPut(DetectorState);
                /* Server sends back 0 when the write is complete */
                %%read_response(ssId, pVar, 0);
                save_single = 0;
            }
        } state wait_save
    }

    state wait_save {
        when (DetectorState != DETECTOR_IDLE && delay(0.05)) {
            /* Read the status */
            %%send_command(ssId, pVar, "get_state\n");
            %%read_response(ssId, pVar, 0);
            DetectorState = atoi(buffer); pvPut(DetectorState);
        } state wait_save

        when (DetectorState == DETECTOR_ERROR) {
            /* Set PVs back to 0 */
            AcquireCLBK = 0; pvPut(AcquireCLBK);
            AcquirePOLL = 0; pvPut(AcquirePOLL);
        } state monitor_changes
        
        when (DetectorState == DETECTOR_IDLE) {
            /* Set PVs back to 0 */
            AcquireCLBK = 0; pvPut(AcquireCLBK);
            AcquirePOLL = 0; pvPut(AcquirePOLL);
        } state monitor_changes
    }
}


/* This is a separate state set so the watchdog runs even during long acquisitions */
ss watchdog {
    state poll {
        when(delay(1.0) && SNLWatchdog < 5) {
            SNLWatchdog = 5; pvPut(SNLWatchdog);
        } state poll
    }
}

/* This is a separate state set so we can abort during the epicsEventWaitWithTimeout that times the shutter */
ss abort {
    state poll {
        when(Abort_mon && Abort==1) {
            %%send_command(ssId, pVar, "abort");
            %%epicsEventSignal(abortEvent);
            Abort=0; pvPut(Abort);
            TimeRemaining=0.; pvPut(TimeRemaining);
        } state poll
    }
}

/* This is a separate state set so we update the time remaining during the epicsEventWaitWithTimeout that times the shutter */
ss timeRemaining {
    state poll {
        when(delay(.1) && TimeRemaining != 0.) {
            %%epicsTimeGetCurrent(&currentTime);
            %%pVar->TimeRemaining = pVar->exposureTime - epicsTimeDiffInSeconds(&currentTime, &startTime);
            if (TimeRemaining < 0.) TimeRemaining = 0.;
            pvPut(TimeRemaining);
        } state poll
    }
}

/* Begin C code functions */
%{

static int send_command(SS_ID ssId, struct UserVar *pVar, char *command)
{
   int status;
   char buffer[MAX_COMMAND_SIZE];
   epicsTimeStamp ts;
   char timeBuff[MAX_COMMAND_SIZE];

   if (pVar->ConnectState != STATE_CONNECTED) return(-1);
   /* A short delay is required between sending commands to MAR, else they get partly lost */
   epicsThreadSleep(.001);
   strncpy(buffer, command, sizeof(buffer));
   status = write_socket(pVar->socket, buffer, strlen(buffer)+1);
   if (pVar->DebugFlag) {
      epicsTimeGetCurrent(&ts);
      epicsTimeGetCurrent(&ts);
      epicsTimeToStrftime(timeBuff, sizeof(timeBuff), "%Y/%m/%d %H:%M:%S.%03f", &ts);
      errlogPrintf("%s marCCD:sent %s\n", timeBuff, command);
      strncpy(pVar->DetOutStr, command, sizeof(pVar->DetOutStr));
      seq_pvPut(ssId, pVar->DetOutStrIndex, 0);
   }
   if (status < 0) pVar->ConnectState = STATE_DISCONNECTED;
   /* Should really do a pvPut of ConnectState if status != 0 */
   return(status);
}

static int read_response(SS_ID ssId, struct UserVar *pVar, int poll)
{
   int status;
   epicsTimeStamp ts;
   char timeBuff[MAX_COMMAND_SIZE];

   if (pVar->ConnectState != STATE_CONNECTED) return(-1);
   strcpy(pVar->buffer, "");
   status = listen_socket(pVar->buffer, sizeof(pVar->buffer), 1, 0, poll);
   if (status <= 0) strcpy(pVar->buffer, "");
   if (pVar->DebugFlag) {
      epicsTimeGetCurrent(&ts);
      epicsTimeToStrftime(timeBuff, sizeof(timeBuff), "%Y/%m/%d %H:%M:%S.%03f", &ts);
      errlogPrintf("%s marCCD:receive (%d) %s\n", timeBuff, status, pVar->buffer);
      strncpy(pVar->DetInStr, pVar->buffer, sizeof(pVar->DetInStr));
      seq_pvPut(ssId, pVar->DetInStrIndex, 0);
   }
   return(status);
}

}%
