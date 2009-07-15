program marCCD("P=marCCD:,C=ccd1")

/*  This sequencer program works with ccd.db.  
 *  It allows EPICS control of the MAR 165 CCD detector.  The detector must 
 *  be put in remote mode, running the marccd_server_socket program.
 * *
 *  Mark Rivers
 *  Sept 27, 2003
*/

/* This program must be compiled with the recursive option so that 
 * multiple copies can run at once
 */
option +r;

%%#include <string.h>
%%#include <stdio.h>
%%#include <errlog.h>
%%#include <epicsTime.h>
%%#include <epicsEvent.h>
%%#include <asynDriver.h>
%%#include <asynOctetSyncIO.h>
#include "epicsCCD.h"
#define MAX_COMMAND_SIZE 200
#define TIMEOUT 1.0 /* Default timeout */

/* Define escaped C functions at end of file */
%% static int asynConnect(SS_ID ssId, struct UserVar *pVar);
%% static int asynWrite(SS_ID ssId, struct UserVar *pVar, char *command);
%% static int asynWriteRead(SS_ID ssId, struct UserVar *pVar, char *command, 
%%                          double timeout);
%% static void acquireFrame(SS_ID ssId, struct UserVar *pVar, double exposureTime,
%%                          int useShutter, int bufferNumber);


/* Variables */
char   buffer[MAX_COMMAND_SIZE];
double exposureTime;
char   *blank_pos;
char   *portName;
int    nchars;
int    save_single;
int    status;
int    useShutter;
int    validPath;
int    AsynPortIndex;
int    DetOutStrIndex;
int    DetInStrIndex;
int    DetectorStateIndex;
int    ShutterIndex;
int    TimeRemainingIndex;

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
            strncpy(Comment1Desc, "Det. Dist. (mm)", sizeof(Comment1Desc));  
            pvPut(Comment1Desc);
            strncpy(Comment2Desc, "Rotation axis", sizeof(Comment2Desc));
            pvPut(Comment2Desc);
            strncpy(Comment3Desc, "Wavelength", sizeof(Comment3Desc));
            pvPut(Comment3Desc);
            strncpy(Comment4Desc, "File comment", sizeof(Comment4Desc));
            pvPut(Comment4Desc);
            strncpy(Comment5Desc, "Dataset comment", sizeof(Comment5Desc));
            pvPut(Comment5Desc);
            efSet(ShutterStatLink_mon); /* Copy link first time through */
            ConnectState = STATE_DISCONNECTED; 
            pvPut(ConnectState);
            Initialize = 1; pvPut(Initialize); /* Force initialize first time */
            strncpy(CCDManufacturer, "MAR", sizeof(CCDManufacturer)); 
            pvPut(CCDManufacturer);
            strncpy(CCDModel, "165", sizeof(CCDModel)); 
            pvPut(CCDModel);
            MeasuredTemp = 25; 
            pvPut(MeasuredTemp);
            BitDepth = 16; 
            pvPut(BitDepth);
            AsynPortIndex = pvIndex(AsynPort);
            DetInStrIndex = pvIndex(DetInStr);
            DetOutStrIndex = pvIndex(DetOutStr);
            DetectorState = pvIndex(DetectorState);
            ShutterIndex = pvIndex(Shutter);
            TimeRemainingIndex = pvIndex(TimeRemaining);
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
        } state monitor_changes
 
    }


    state monitor_changes {
        when((ConnectState == STATE_DISCONNECTED) ||
             (efTest(Initialize_mon) && Initialize == 1) ||
             (efTest(ServerName_mon)) ||
             (efTest(ServerPort_mon))) {
        } state wait_connect

        when(efTestAndClear(PollDetState_mon)) { 
            %%asynWriteRead(ssId, pVar, "get_state", TIMEOUT);
            DetectorState = atoi(buffer); pvPut(DetectorState);
            %%asynWriteRead(ssId, pVar, "get_bin", TIMEOUT);
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
            /* Need to wait a little while for link to establish before 
            /* processing status record */
            epicsThreadSleep(0.1);
            ShutterStatProc = 1; pvPut(ShutterStatProc);
        } state monitor_changes

        when(efTestAndClear(OpenShutter_mon) && 
                            OpenShutter==1) {
            /* Are we using an EPICS PV for the shutter, or the MAR 165 
             * shutter? */
            if (ShutterMode == SHUTTER_EPICS) { 
                strncpy(Shutter, OpenShutterStr, sizeof(Shutter)); 
                pvPut(Shutter);
            } else if (ShutterMode == SHUTTER_CAMERA) {
                %%asynWrite(ssId, pVar, "shutter,1");
            }
            /* Set the PV back to 0 */
            OpenShutter = 0; pvPut(OpenShutter);
        } state monitor_changes

        when(efTestAndClear(CloseShutter_mon) && 
                            CloseShutter==1) {
            /* Are we using an EPICS PV for the shutter, or the MAR 165 
             * shutter? */
            if (ShutterMode == SHUTTER_EPICS) { 
                strncpy(Shutter, CloseShutterStr, sizeof(Shutter)); 
                pvPut(Shutter);
            } else if (ShutterMode == SHUTTER_CAMERA) {
                %%asynWrite(ssId, pVar, "shutter,0");
            }
            /* Set the PV back to 0 */
            CloseShutter = 0; pvPut(CloseShutter);
        } state monitor_changes

        when((efTestAndClear(BinX_mon)) ||
             (efTestAndClear(BinY_mon))) {
            /* Set binning */
            epicsSnprintf(buffer, sizeof(buffer), "set_bin,%d,%d", BinX, BinY);
            %%asynWrite(ssId, pVar, pVar->buffer);
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
        } state acquire
    }


    state acquire {
        when(FrameType == FRAME_NORMAL) {
        } state acquire_normal

        when(FrameType == FRAME_DOUBLE_CORRELATION) {
        } state acquire_double_correlation

        when(FrameType == FRAME_BACKGROUND) {
        } state acquire_background
        
        when(FrameType == FRAME_FLATFIELD) {
            printf("Frame type FRAME_FLATFIELD not supported by marCCD\n");
        } state monitor_changes
    }

    state acquire_background {
        when () {
            /* Acquire 2 backgrounds with 1msec exposure each.  First into
             * scratch buffer, second into background buffer, then dezinger
             * Minimum exposure time of 1msec, because the readout command 
             * is not seen if it comes after the start command with no delay */
            exposureTime = .001;
            useShutter = 0;
            %%acquireFrame(ssId, pVar, pVar->exposureTime, pVar->useShutter, 2);
            %%acquireFrame(ssId, pVar, pVar->exposureTime, pVar->useShutter, 1); 
            /* Remove zingers from background */
            %%asynWrite(ssId, pVar, "dezinger,1");
        } state save_file
    }


    state acquire_normal {
        when() {
            /* Start acquisition */
            exposureTime = Hours*3600. + Minutes*60. + Seconds +
                           Milliseconds/1000.;
            /* Minimum exposure time of 1msec, because the readout command
             * is not seen if it comes after the start command with no delay */
            if (exposureTime < .001) exposureTime = .001;
            useShutter = 1;
            %%acquireFrame(ssId, pVar, pVar->exposureTime, pVar->useShutter, 0); 
        } state save_file
    }

    state acquire_double_correlation {
        when () {
            /* Acquire 2 frames with half the requested exposure time each.  First into
             * scratch buffer, second into data buffer buffer, then dezinger */
            exposureTime = Hours*3600. + Minutes*60. + Seconds +
                           Milliseconds/1000.;
            exposureTime = exposureTime/2.;
            useShutter = 1;
            /* Minimum exposure time of 1msec, because the readout command
             * is not seen if it comes after the start command with no delay */
            if (exposureTime < .001) exposureTime = .001;
            /* Acquire first frame into scratch buffer */
            %%acquireFrame(ssId, pVar, pVar->exposureTime, pVar->useShutter, 2); 
            /* Acquire second frame into image buffer */
            %%acquireFrame(ssId, pVar, pVar->exposureTime, pVar->useShutter, 0); 
             
            /* Remove zingers from image */
            %%asynWrite(ssId, pVar, "dezinger,0");
        } state save_file
    }


    state save_file {
        when() {
            if (AutoSave || save_single) {
                /* Construct header command */
                epicsSnprintf(buffer, sizeof(buffer), "header,detector_distance=%s", Comment1);
                %%asynWrite(ssId, pVar, pVar->buffer);
                epicsSnprintf(buffer, sizeof(buffer), "header,exposure_time=%f", exposureTime);
                %%asynWrite(ssId, pVar, pVar->buffer);
                epicsSnprintf(buffer, sizeof(buffer), "header,rotation_axis=%s", Comment2);
                %%asynWrite(ssId, pVar, pVar->buffer);
                epicsSnprintf(buffer, sizeof(buffer), "header,source_wavelength=%s", Comment3);
                %%asynWrite(ssId, pVar, pVar->buffer);
                epicsSnprintf(buffer, sizeof(buffer), "header,file_comments=%s", Comment4);
                %%asynWrite(ssId, pVar, pVar->buffer);
                epicsSnprintf(buffer, sizeof(buffer), "header,dataset_comments=%s", Comment5);
                %%asynWrite(ssId, pVar, pVar->buffer);
                if (CorrectFlatfield) {
                    DetectorState = DETECTOR_CORRECT; pvPut(DetectorState);
                    /* Without the following 20msec delay we get an access 
                     * fault in listen_socket().*/
                    epicsThreadSleep(.02);
                    /* Server sends back 0 when the correction is complete */
                    %%asynWriteRead(ssId, pVar, "correct", 10.0);
                }
                /* Save file */
                epicsSnprintf(FullFilename, sizeof(FullFilename), FilenameFormat, FileTemplate, SeqNumber);
                pvPut(FullFilename);
                SeqNumber++; pvPut(SeqNumber);
                epicsSnprintf(buffer, sizeof(buffer), "writefile,%s%s,%d", 
                        FilePath, FullFilename, CorrectFlatfield?1:0);
                DetectorState = DETECTOR_WRITING; pvPut(DetectorState);
                /* Server sends back 0 when the write is complete */
                %%asynWriteRead(ssId, pVar, pVar->buffer, -1.0);
                save_single = 0;
            }
            while (DetectorState != DETECTOR_IDLE) {
               epicsThreadSleep(0.05);
               /* Read the status */
               %%asynWriteRead(ssId, pVar, "get_state", TIMEOUT);
               DetectorState = atoi(buffer); pvPut(DetectorState);
            }
            AcquireCLBK = 0; pvPut(AcquireCLBK);
            AcquirePOLL = 0; pvPut(AcquirePOLL);
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
            %%asynWrite(ssId, pVar, "abort");
            %%epicsEventSignal((epicsEventId) pVar->abortEvent);
            Abort=0; pvPut(Abort);
            TimeRemaining=0.; pvPut(TimeRemaining);
        } state poll
    }
}

/* This is a separate state set so we update the time remaining during the 
 * epicsEventWaitWithTimeout that times the shutter */
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
   if (status != asynSuccess) return(status);
   status = pasynOctetSyncIO->connect(portString, 0, (asynUser **)&pVar->pasynUser, NULL);
   if (status != asynSuccess) return(status);
   pasynTrace->setTraceIOMask((asynUser *)pVar->pasynUser, ASYN_TRACEIO_ESCAPE);
   /* Also connect the asynRecord for user interaction */
   strncpy(pVar->AsynPort, portString, sizeof(pVar->AsynPort));
   seq_pvPut(ssId, pVar->AsynPortIndex, 0);
   return(status);
}

static int asynWrite(SS_ID ssId, struct UserVar *pVar, char *command)
{
   int nwrite;
   asynStatus status;

   if (pVar->ConnectState != STATE_CONNECTED) return(-1);
   /* A short delay is required between sending commands to MAR, else they 
    * get partly lost? */
   epicsThreadSleep(.001);
   status = pasynOctetSyncIO->write((asynUser *)pVar->pasynUser, command, 
                                    strlen(command), TIMEOUT, &nwrite);
   /* Copy output string to PV so user can see it */
   strncpy(pVar->DetOutStr, command, sizeof(pVar->DetOutStr));
   seq_pvPut(ssId, pVar->DetOutStrIndex, 0);
   return(nwrite);
}

static int asynWriteRead(SS_ID ssId, struct UserVar *pVar, char *command,
                         double timeout)
{
   int nread=0, nwrite;
   asynStatus status;
   int eomReason;

   if (pVar->ConnectState != STATE_CONNECTED) return(-1);
   /* A short delay is required between sending commands to MAR, else they
    * get partly lost */
   epicsThreadSleep(.001);
   status = pasynOctetSyncIO->setInputEos((asynUser *)pVar->pasynUser, "\n", 1);
   status = pasynOctetSyncIO->setOutputEos((asynUser *)pVar->pasynUser, "\n", 1);
   status = pasynOctetSyncIO->writeRead((asynUser *)pVar->pasynUser, command, 
                                  strlen(command), pVar->buffer, 
                                  sizeof(pVar->buffer), timeout,
                                  &nwrite, &nread, &eomReason);

   /* Copy output string to PV so user can see it */
   strncpy(pVar->DetOutStr, command, sizeof(pVar->DetOutStr));
   seq_pvPut(ssId, pVar->DetOutStrIndex, 0);
   /* Copy input string to PV so user can see it */
   strncpy(pVar->DetInStr, pVar->buffer, sizeof(pVar->DetInStr));
   seq_pvPut(ssId, pVar->DetInStrIndex, 0);
   return(nread);
}


static void acquireFrame(SS_ID ssId, struct UserVar *pVar, double exposureTime,
                       int useShutter, int bufferNumber)
{
    char readoutCommand[20];
    double delay;
    
    asynWrite(ssId, pVar, "start");
    while (pVar->DetectorState != DETECTOR_ACQUIRE) {
        epicsThreadSleep(0.05);
        /* Read the status */
        asynWriteRead(ssId, pVar, "get_state", TIMEOUT);
        pVar->DetectorState = atoi(pVar->buffer);
        seq_pvPut(ssId, pVar->DetectorStateIndex, 0);
    }

    /* Set the the start time for the TimeRemaining counter */
    epicsTimeGetCurrent((epicsTimeStamp *)pVar->startTime);
    pVar->TimeRemaining = exposureTime;
    seq_pvPut(ssId, pVar->TimeRemainingIndex, 0);
    if (useShutter) {
        /* Open the shutter */
        /* EPICS PV for the shutter, or the MAR 165 shutter? */
        if (pVar->ShutterMode == SHUTTER_EPICS) { 
            strncpy(pVar->Shutter, pVar->OpenShutterStr, sizeof(pVar->Shutter)); 
            seq_pvPut(ssId, pVar->ShutterIndex, 0);
        } else if (pVar->ShutterMode == SHUTTER_CAMERA) {
            asynWrite(ssId, pVar, "shutter,1");
        }
        /* This delay is to get the exposure time correct.  
         * It is equal to the opening time of the shutter minus the
         * closing time.  If they are equal then no delay is needed, 
         * except use 1msec so delay is not negative and commands are 
         * not back-to-back */
        delay = pVar->OpenShutterDly - pVar->CloseShutterDly;
        if (delay < .001) delay=.001;
        epicsThreadSleep(delay);
    }

    /* Wait for the exposure time using epicsEventWaitWithTimeout, 
     * so we can abort */
    epicsEventWaitWithTimeout((epicsEventId)pVar->abortEvent, 
                               exposureTime);
    if (useShutter) {
        /* Close shutter */
        /* Are we using an EPICS PV for the shutter, or the 
         * MAR 165 shutter? */
        if (pVar->ShutterMode == SHUTTER_EPICS) { 
            strncpy(pVar->Shutter, pVar->CloseShutterStr, sizeof(pVar->Shutter));
            seq_pvPut(ssId, pVar->ShutterIndex, 0);
        } else if (pVar->ShutterMode == SHUTTER_CAMERA) {
            asynWrite(ssId, pVar, "shutter,0");
        }
        epicsThreadSleep(pVar->CloseShutterDly);
    }

    /* Start readout */
    /* Detector sends 0 when readout is done */
    pVar->DetectorState = DETECTOR_READOUT; 
    seq_pvPut(ssId, pVar->DetectorStateIndex, 0);
    epicsSnprintf(readoutCommand, sizeof(readoutCommand), "readout, %d", bufferNumber);
    asynWriteRead(ssId, pVar, readoutCommand, -1.);
    pVar->DetectorState = atoi(pVar->buffer);
    seq_pvPut(ssId, pVar->DetectorStateIndex, 0);

    while (pVar->DetectorState != DETECTOR_IDLE) {
        epicsThreadSleep(0.05);
        /* Read the status */
        asynWriteRead(ssId, pVar, "get_state", TIMEOUT);
        pVar->DetectorState = atoi(pVar->buffer);
        seq_pvPut(ssId, pVar->DetectorStateIndex, 0);
    }
}


}%
