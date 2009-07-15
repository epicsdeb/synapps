program smartControl("P=13IDC:,R=smart1,PHI=PHI,KAPPA=KAPPA,OMEGA=OMEGA,TH=TTH,SCALER=SCALER,I0=I0")

/*  This sequencer program works with smartControl.db.  It allows a PC running 
 *  the Bruker SMART software to control a goniometer under EPICS.  
 *  It does this by emulating the Bruker GGCS goniometer controller.  SMART 
 *  sends commands to the GGCS over a serial port, and expects reponses from 
 *  the GGCS.  This software listens for SMART commands on a serial port using
 *  an EPICS generic serial record.  It converts those commands into commands 
 *  to move EPICS motors, etc.
 *  This software currently only works with standard EPICS motors and PVs to 
 *  control the shutter. 
 *  A future version will use trajectory scanning with the MM4000 to
 *  provide more accurate timing with the MM4000 and the Newport 
 *  diffractometers.
 *
 *   SMART refers to motors in the following order:
 *     1) 2-theta
 *     2) Omega
 *     3) Phi
 *     4) Kappa
 *
 *   For the GSECARS GPD goniometer the MM4000 motors are in the following
 *   order:
 *     1) Phi
 *     2) Kappa
 *     3) Omega
 *     4) Psi
 *     5) 2-theta
 *     6) Nu
 *
 *   For the N3020 small kappa goniometer the MM4000 motors are in the
 *   following order:
 *     1) Omega
 *     2) Kappa
 *     3) Phi
 *     4) 2-theta (Not actually connected)
 *
 *
 *  Mark Rivers
 *  Sept. 17, 2000 Based on a previous C program "smart_to_mm4000" 
 *                 by Mark Rivers
 *
 *  Modifications:
 *  November 9, 2000 MLR    Cleaned up comments, implemented "I" commands for 
 *                          incident beam intensity monitors.
 *  November 13, 2000 MLR   Implemented Bn, W+2, W-2, RCn, GSn commands required
 *                          by new version of KappaSmart
 *  March 16, 2001    MLR   Lots of changes to be compatible with the factory
 *                          release of KappaSmart.  This involved implementing
 *                          or ignorring a number of GGCS commands which Smart
 *                          previously did not issue.
 *  March 17, 2001    MLR   Adjust the acceleration time of EPICS motors so that
 *                          it scales with velocity, i.e. deg/sec/sec is
 *                          constant, rather than time.  This minimizes the
 *                          acceleration time during an oscillation, producing
 *                          better data.  The program assumes that when it
 *                          starts the EPICS motors have been set to their
 *                          maximum acceleration (in deg/sec/sec) and maximum
 *                          velocity via the .VELO and .ACCL fields.  It uses 
 *                          this acceleration (deg/sec/sec) for all moves.
 *  June 27, 2001    MLR    Made the maximum velocity be the field .VMAX,
 *                          rather than the initial velocity .VELO when the
 *                          program starts.  Also limited velocity to
 *                          this value, even if SMART asks for a larger
 *                          velocity.  Previously, EPICS would ignore velocity
 *                          requests greater than .VMAX, and the motor could
 *                          take hours to slew a long distance, moving at the
 *                          previous (oscillation) speed.
 *  Feb. 8, 2002     MLR    Changed _SIGN PVs from mbbo to bi.  Fixed logic
 *                          of sign, since it was not working right, -1 was
 *                          coming out as 2^32 because .RVAL field is ULONG.
 *                          Fixed problem with reporting stage position=0 because
 *                          kappa=0 when running Kappa_SMART with ADMODEL=P4.
 *                          Fixed problem that smartVelocity was not initialized
 *                          and SMART does not send velocity until motor is moved,
 *                          so EPICS motor speeds were being set to 0 for unused axes.
 *
*/

%% #include <string.h>
%% #include <epicsThread.h>
%% #include <epicsString.h>
%% #include <time.h>
%% #include <math.h>
%% #include <errlog.h>
%% #include <asynOctetSyncIO.h>

/* This program must be compiled with the recursive option */
option +r;
#define INBOARD 0
#define OUTBOARD 1
#define GPD 0
#define N3020 1
#define EPICS 2

/* The order in which SMART numbers the motors */
#define TTH   0
#define OMEGA 1
#define PHI   2
#define KAPPA 3

/* Maximum size of string sent to/from SMART */
#define MAX_STRING 256

/* Maximum number of motors being controlled */
#define MAX_AXES 4

/* Polling interval for waiting for motors to reach their targets */
#define POLL_INTERVAL 0.05

#define FIRST_TOKEN epicsStrtok_r(smartIn, " ,", &token_save)
#define NEXT_TOKEN epicsStrtok_r(0, " ,", &token_save)



/* Define PVs */
string asynPort;              assign asynPort     to "{P}{R}asynPort.VAL";
                                                        monitor asynPort;
int    debugLevel;            assign debugLevel   to "{P}{R}debugLevel.VAL";
                                                        monitor debugLevel;
string fastShutter;           assign fastShutter  to "{P}{R}FSHUT.VAL";
string slowShutter;           assign slowShutter  to "{P}{R}SSHUT.VAL";
string detTrigger;            assign detTrigger   to "{P}{R}TRIG.VAL";
double scalerPreset;          assign scalerPreset to "{P}{SCALER}.TP";
                                                        monitor scalerPreset;
int    scalerCount;           assign scalerCount  to "{P}{SCALER}.CNT";
double scalerTime;            assign scalerTime   to "{P}{SCALER}.T";
                                                        monitor scalerTime;
double I0Counts;              assign I0Counts     to "{P}{SCALER}.S{I0}";
                                                        monitor I0Counts;

double  epicsMotorPos[MAX_AXES]; 
                              assign  epicsMotorPos to
                                                   {"{P}{TTH}.VAL",
                                                    "{P}{OMEGA}.VAL",
                                                    "{P}{PHI}.VAL",
                                                    "{P}{KAPPA}.VAL"};
                              monitor epicsMotorPos;
        
int     epicsMotorDone[MAX_AXES]; 
                              assign  epicsMotorDone  to
                                                   {"{P}{TTH}.DMOV",
                                                    "{P}{OMEGA}.DMOV",
                                                    "{P}{PHI}.DMOV",
                                                    "{P}{KAPPA}.DMOV"};
                              monitor epicsMotorDone;

int     epicsMotorHome[MAX_AXES]; 
                              assign  epicsMotorHome  to
                                                   {"{P}{TTH}.HOMF",
                                                    "{P}{OMEGA}.HOMF",
                                                    "{P}{PHI}.HOMF",
                                                    "{P}{KAPPA}.HOMF"};

double  epicsMotorVel[MAX_AXES]; 
                              assign  epicsMotorVel  to
                                                   {"{P}{TTH}.VELO",
                                                    "{P}{OMEGA}.VELO",
                                                    "{P}{PHI}.VELO",
                                                    "{P}{KAPPA}.VELO"};
                              monitor epicsMotorVel;
double  epicsMotorMaxVel[MAX_AXES]; 
                              assign  epicsMotorMaxVel  to
                                                   {"{P}{TTH}.VMAX",
                                                    "{P}{OMEGA}.VMAX",
                                                    "{P}{PHI}.VMAX",
                                                    "{P}{KAPPA}.VMAX"};
                              monitor epicsMotorMaxVel;
double  epicsMotorAccel[MAX_AXES]; 
                              assign  epicsMotorAccel  to
                                                   {"{P}{TTH}.ACCL",
                                                    "{P}{OMEGA}.ACCL",
                                                    "{P}{PHI}.ACCL",
                                                    "{P}{KAPPA}.ACCL"};
                              monitor epicsMotorAccel;
double  epicsMotorMaxAccel[MAX_AXES]; 

double  epicsMotorLowLim[MAX_AXES]; 
                              assign  epicsMotorLowLim  to
                                                   {"{P}{TTH}.LLM",
                                                    "{P}{OMEGA}.LLM",
                                                    "{P}{PHI}.LLM",
                                                    "{P}{KAPPA}.LLM"};
                              monitor epicsMotorLowLim;

double  epicsMotorHighLim[MAX_AXES]; 
                              assign  epicsMotorHighLim  to
                                                   {"{P}{TTH}.HLM",
                                                    "{P}{OMEGA}.HLM",
                                                    "{P}{PHI}.HLM",
                                                    "{P}{KAPPA}.HLM"};
                              monitor epicsMotorHighLim;

double  smartPosition[MAX_AXES];
                              assign  smartPosition  to
                                                   {"{P}{R}TTH_POS.VAL",
                                                    "{P}{R}OMEGA_POS.VAL",
                                                    "{P}{R}PHI_POS.VAL",
                                                    "{P}{R}KAPPA_POS.VAL"};

double  smartVelocity[MAX_AXES];
                              assign  smartVelocity  to
                                                   {"{P}{R}TTH_VEL.VAL",
                                                    "{P}{R}OMEGA_VEL.VAL",
                                                    "{P}{R}PHI_VEL.VAL",
                                                    "{P}{R}KAPPA_VEL.VAL"};

double  smartLowLimit[MAX_AXES];
                              assign  smartLowLimit  to
                                                   {"{P}{R}TTH_LOW_LIM.VAL",
                                                    "{P}{R}OMEGA_LOW_LIM.VAL",
                                                    "{P}{R}PHI_LOW_LIM.VAL",
                                                    "{P}{R}KAPPA_LOW_LIM.VAL"};
                              monitor smartLowLimit;

double  smartHighLimit[MAX_AXES];
                              assign  smartHighLimit  to
                                                   {"{P}{R}TTH_HIGH_LIM.VAL",
                                                    "{P}{R}OMEGA_HIGH_LIM.VAL",
                                                    "{P}{R}PHI_HIGH_LIM.VAL",
                                                    "{P}{R}KAPPA_HIGH_LIM.VAL"};
                              monitor smartHighLimit;

double  axisOffset[MAX_AXES];
                              assign  axisOffset  to
                                                   {"{P}{R}TTH_OFFSET.VAL",
                                                    "{P}{R}OMEGA_OFFSET.VAL",
                                                    "{P}{R}PHI_OFFSET.VAL",
                                                    "{P}{R}KAPPA_OFFSET.VAL"};
                              monitor axisOffset;

double  axisSign[MAX_AXES];
                              assign  axisSign  to
                                                   {"{P}{R}TTH_SIGN.VAL",
                                                    "{P}{R}OMEGA_SIGN.VAL",
                                                    "{P}{R}PHI_SIGN.VAL",
                                                    "{P}{R}KAPPA_SIGN.VAL"};
                              monitor axisSign;

double  axisLowCut[MAX_AXES];
                              assign  axisLowCut  to
                                                   {"{P}{R}TTH_LOW_CUT.VAL",
                                                    "{P}{R}OMEGA_LOW_CUT.VAL",
                                                    "{P}{R}PHI_LOW_CUT.VAL",
                                                    "{P}{R}KAPPA_LOW_CUT.VAL"};
                              monitor axisLowCut;

double  axisHighCut[MAX_AXES];
                              assign  axisHighCut  to
                                                   {"{P}{R}TTH_HIGH_CUT.VAL",
                                                    "{P}{R}OMEGA_HIGH_CUT.VAL",
                                                    "{P}{R}PHI_HIGH_CUT.VAL",
                                                    "{P}{R}KAPPA_HIGH_CUT.VAL"};
                              monitor axisHighCut;

int     motorExists[MAX_AXES];
                              assign  motorExists  to
                                                   {"{P}{R}TTH_EXISTS.RVAL",
                                                    "{P}{R}OMEGA_EXISTS.RVAL",
                                                    "{P}{R}PHI_EXISTS.RVAL",
                                                    "{P}{R}KAPPA_EXISTS.RVAL"};
                              monitor motorExists;

char *token;
char *token_save;
int tokenLen;
int motor;
int i;
int j;
int commandLen;
int time;
int status;
double inc;
double speed;
int measureI0;
double presetSave;
char smartIn[MAX_STRING];
char smartOut[MAX_STRING];
char *pasynUser;   /* This is really asynUser* */

/* All PVs which will be accessed in local C functions need to have their index
 * extracted with pvIndex() */
int epicsMotorDoneIndex[MAX_AXES];

/* Define escaped C functions at end of file */
%% static int writeSmart(SS_ID ssId, struct UserVar *pVar);
%% static int readSmart(SS_ID ssId, struct UserVar *pVar);
%% static int getEpicsMotorMoving(SS_ID ssId, struct UserVar *pVar);
%% static void waitEpicsMotors(SS_ID ssId, struct UserVar *pVar);
%% static void transformSmartToEpics(SS_ID ssId, struct UserVar *pVar);
%% static void transformEpicsToSmart(SS_ID ssId, struct UserVar *pVar);


ss smartControl {
    /* Initialize things when first starting */
    state init {
        when() {
            measureI0 = 0;
            /* Get PV indices of PVs used in C functions at the end */
            if (debugLevel >= 3) printf("Initializing smartControl\n");
            for (j=0; j<MAX_AXES; j++) {
                epicsMotorDoneIndex[j] = pvIndex(epicsMotorDone[j]);
                /* Save maximum acceleration time */
                epicsMotorMaxAccel[j] = epicsMotorAccel[j];
                /* Initialize smartVelocity to the current EPICS motor velocity,
                 * converting from degrees/second to degrees/minute. We do this
                 * because SMART does not send velocity until motor is moved */
                smartVelocity[j] = epicsMotorVel[j]*60.;
            }
            %%pVar->status = pasynOctetSyncIO->connect(pVar->asynPort, 0, 
            %%                                   (asynUser **)&pVar->pasynUser, NULL);
            if (status != 0) {
               printf("smartControl error in pasynOctetSyncIO->connect\n");
               printf("   status=%d, port=%s\n", status, asynPort);
            }
        } state monitor_inputs
    }  /* End state init */


    /* Monitor inputs which control what to do
     * right now only monitor SMART commands */
    state monitor_inputs {
      when() {
         if (debugLevel >= 5) printf("Entering state monitor_inputs\n");
      } state parseSmart
    }

    state parseSmart { 
      when() {
        if (debugLevel >= 5) printf("Entering state parseSmart\n");
        %%pVar->status = readSmart(ssId, pVar);
        commandLen = strlen(smartIn);
        if (debugLevel >= 2) printf("Command length from SMART=%d, string=%s\n", 
                  commandLen, smartIn);
        token_save = 0;
        for (token=FIRST_TOKEN; 
             ((status==0) && (token != 0) && ((tokenLen=strlen(token))>0));
             token=NEXT_TOKEN) {
            if (debugLevel >= 3) printf("Token length=%d token = (%s) = (", tokenLen, token);
            for (i=0; i<tokenLen; i++) if (debugLevel >= 3) printf(" %x ", token[i]);
            if (debugLevel >= 3) printf(")\n");

            /* For now this is a simple-minded translation of the commands.
             * We need to put in a test to determine when a command is an
             * oscillation command and treat it specially, using trajectory 
             * scanning if output device is MM4000 
             */

            if (token[0] == 6) {
                /* Control F - soft abort */
                /* Echo ^F back to SMART */
                smartOut[0] = 6;
                smartOut[1] = 0;
                %%writeSmart(ssId, pVar);
            } else

            if (token[0] == 7) {
                /* Control G - echo it back */
                smartOut[0]=7;
                smartOut[1]=13;
                smartOut[2]=0;
                %%writeSmart(ssId, pVar);
            } else

            if (token[0] == 17) {
                /* Control Q - XON, ignore */
            } else

            if (token[0] == 'A') {
                /* Motor target position */
                motor = atoi(&token[1]) - 1;
                if (motor >= MAX_AXES) break;
                token = NEXT_TOKEN;
                inc = atof(token);
                smartPosition[motor] += inc;
                pvPut(smartPosition[motor]);
            } else

            if (token[0] == 'B') {
                if (commandLen > 4) {
                    /* Set soft limits */
                    motor = atoi(&token[1]) - 1;
                    if (motor >= MAX_AXES) break;
                    token = NEXT_TOKEN;
                    smartLowLimit[motor] = atof(token);
                    pvPut(smartLowLimit[motor]);
                    token = NEXT_TOKEN;
                    smartHighLimit[motor] = atof(token);
                    pvPut(smartHighLimit[motor]);
                } else {
                    /* Report soft limits */
                    motor = atoi(&token[1]) - 1;
                    if (motor >= MAX_AXES) motor = MAX_AXES-1;
                    sprintf(smartOut, "B%d, %f, %f\r", 
                            motor+1, smartLowLimit[motor], 
                            smartHighLimit[motor]);
                    %%writeSmart(ssId, pVar);
                }
            } else

            if (token[0] == 'D') {
                if (tokenLen == 1) {
                    /* Drive the motors */
                    /* Transform from Smart coordinates to 
                       Newport coordinates */
                    %%transformSmartToEpics(ssId, pVar);
                    for (j=0; j<MAX_AXES; j++) {
                        if (motorExists[j]) {
                            pvPut(epicsMotorAccel[j]);
                            pvPut(epicsMotorVel[j]);
                            pvPut(epicsMotorPos[j]);
                        }
                    }
                    %%waitEpicsMotors(ssId, pVar);
                } else {
                    if ((token[1] == 'L') || (token[1] == 'T')) {
                        /* Ignore "DL" and "DT" commands */
                    } else {
                        if (debugLevel >= 1) printf("Error - unknown token = %s \n", token);
                    }
                }
            } else

            if (token[0] == 'H') {
                /* Home motor */
                motor = atoi(&token[2]) - 1;
                if (motor >= MAX_AXES) break;
                if (motorExists[motor]) {
                    epicsMotorHome[motor]=1;
                    pvPut(epicsMotorHome[motor]);
                }
                token = NEXT_TOKEN; /* Ignore speed parameter */
                %%waitEpicsMotors(ssId, pVar);
            } else

            if (token[0] == 'I') {
                /* Intensity enable/read */
                if (token[1] == '-') {
                    /* This is a command to start measuring I0 */
                    measureI0 = 1;
                } else {
                    /* Report back the time in milliseconds and the I0 counts */
                    sprintf(smartOut, "%f, %f,\r", scalerTime*1000., I0Counts);
                    %%writeSmart(ssId, pVar);
                }
            } else

            if (token[0] == 'L') {
                /* Omega-2theta soft limits */
                token = NEXT_TOKEN;
                /* Need to figure out how to handle these */
                token = NEXT_TOKEN;
            } else

            if (token[0] == 'M') {
                /* Manual mode - ignore this token and the next*/
                token = NEXT_TOKEN;
            } else

            if (token[0] == 'P') {
                /* Show motor position */
                motor = atoi(&token[1]) - 1;
                if (motor >= MAX_AXES) {
                    sprintf(smartOut, "%f\r", 6.3);
                } else {
                    sprintf(smartOut, "%f\r", smartPosition[motor]);
                }
                %%writeSmart(ssId, pVar);
            } else

            if (token[0] == 'R') {
                /* This is an RC command, which can be ignorred.
                 * However, there could be a GS command embedded in it.
                 * If so we must reply. */
                if (token[3] == 'G') {
                    sprintf(smartOut, "GS %d\r", 1000);
                    %%writeSmart(ssId, pVar);
                }
            } else

            if (token[0] == 'S') {
                /* Speed command */
                motor = atoi(&token[1]) - 1;
                if (motor >= MAX_AXES) break;
                token = NEXT_TOKEN;
                speed = atof(token);
                smartVelocity[motor] = speed;
                pvPut(smartVelocity[motor]);
            } else

            if (token[0] == 'U') {
                /* Report positions (what do parameters mean?) */
                if (token[1] == '0') {
                    token = NEXT_TOKEN;
                    /* Transform MM4000 coordinates to SMART */
                    %%transformEpicsToSmart(ssId, pVar);
                    /* Send response string back to SMART */
                    sprintf(smartOut, "%010.6f %10.6f %10.6f %10.6f %5d %5d\r",
                        smartPosition[0], 
                        smartPosition[1], 
                        smartPosition[2], 
                        smartPosition[3], 
                        0,
                        0);
                    %%writeSmart(ssId, pVar);
                } else {
                    if (commandLen > 2) {
                        /* This is a command to update the position of 
                         * motor.  Ignore. */
                        token = NEXT_TOKEN;
                    } 
                    /* Report the position of the motor */
                    motor = atoi(&token[1]) - 1;
                    if (motor >= MAX_AXES) {
                        /* If this is non-existent motor (stage) report position=10. */
                        sprintf(smartOut, "%f\r", 10.);
                    } else {
                        sprintf(smartOut, "%f\r", smartPosition[motor]);
                    }
                    %%writeSmart(ssId, pVar);
                }
            } else

            if (!strncmp(token, "W+1", 3)) {
                if (measureI0) {
                    /* Set the preset count time to 0. so it counts forever */
                    presetSave = scalerPreset;
                    scalerPreset = 0.;
                    pvPut(scalerPreset);
                    /* Start I0 monitor */
                    scalerCount = 1;
                    pvPut(scalerCount);
                }
                /* Open fast shutter */
                strcpy(fastShutter, "Open");
                pvPut(fastShutter);
            } else

            if (!strncmp(token, "W-1", 3)) {
                /* Close fast shutter */
                strcpy(fastShutter, "Closed");
                pvPut(fastShutter);
                if (measureI0) {
                    /* Stop I0 monitor */
                    scalerCount = 0;
                    pvPut(scalerCount);
                    /* Restore preset time */
                    scalerPreset = presetSave;
                    pvPut(scalerPreset);
                }
            } else

            if (!strncmp(token, "W+2", 3)) {
                /* Turn on laser for microdiffraction - ignore */
            } else

            if (!strncmp(token, "W-2", 3)) {
                /* Turn off laser for microdiffraction - ignore */
            } else

            if (!strncmp(token, "W+3", 3)) {
                /*  Open slow shutter (typically on x-ray tube) */
                strcpy(slowShutter, "Open");
                pvPut(slowShutter);
            } else

            if (!strncmp(token, "W-3", 3)) {
                /* Close slow shutter (typically on x-ray tube) */
                strcpy(slowShutter, "Closed");
                pvPut(slowShutter);
            } else

            if (!strncmp(token, "W+7", 3)) {
                /* Turn on detector */
                strcpy(detTrigger,"On");
                pvPut(detTrigger);
            } else

            if (!strncmp(token, "W-7", 3)) {
                /* Turn off detector */
                strcpy(detTrigger,"Off");
                pvPut(detTrigger);
            } else

            if (token[0] == 'W') {
                /* WA (wait) command */
                token = NEXT_TOKEN;
                time = atoi(token);
                epicsThreadSleep(time/1000.);

            } else {
                if (debugLevel >= 1) printf("Error - unknown token = %s \n", token);
                if (debugLevel >= 1) printf("  decimal = %d ", token[0]);
                for (i=1; i<tokenLen; i++) if (debugLevel >= 1) printf("%d ", token[i]);
                if (debugLevel >= 1) printf("\n");

            }  /* End token switch - done parsing this token */
        } /* Next token */
      } state monitor_inputs   /* End when() */
    } /* End state parseSmart */
} /* End ss smartControl */


/* Begin C code functions */
%{

static void transformEpicsToSmart(SS_ID ssId, struct UserVar *pVar)
{
    int i;
    double sign;

    for (i=0; i<MAX_AXES; i++)
    {
        if (pVar->axisSign[i] == 0) sign=1.; else sign=-1.;
        if (pVar->motorExists[i]) {
            pVar->smartPosition[i] = (pVar->epicsMotorPos[i] - 
                                      pVar->axisOffset[i]) * sign;
        } else {
            pVar->smartPosition[i] =  pVar->axisOffset[i] *  sign;
        }
    }
}


static void transformSmartToEpics(SS_ID ssId, struct UserVar *pVar)
{
    int i;
    double sign;

    for (i=0; i<MAX_AXES; i++)
    {
        if (pVar->axisSign[i] == 0) sign=1.; else sign=-1.;
        if (pVar->motorExists[i]) {
            pVar->epicsMotorPos[i] = pVar->axisOffset[i] + 
                                     (sign * pVar->smartPosition[i]);
            if (pVar->debugLevel >= 3) printf("epicsMotorPos[%d] =%f\n", i, pVar->epicsMotorPos[i]);
            if (pVar->debugLevel >= 3) printf("axisLowCut[%d] =%f\n", i, pVar->axisLowCut[i]);
            if (pVar->debugLevel >= 3) printf("axisHighCut[%d] =%f\n", i, pVar->axisHighCut[i]);
            if (pVar->epicsMotorPos[i] > pVar->axisHighCut[i])
                pVar->epicsMotorPos[i] = pVar->epicsMotorPos[i] - 360.;
            if (pVar->epicsMotorPos[i] < pVar->axisLowCut[i])
                pVar->epicsMotorPos[i] = pVar->epicsMotorPos[i] + 360.;
            if (pVar->epicsMotorPos[i] > pVar->epicsMotorHighLim[i])
                pVar->epicsMotorPos[i] = pVar->epicsMotorHighLim[i];
            if (pVar->epicsMotorPos[i] < pVar->epicsMotorLowLim[i])
                pVar->epicsMotorPos[i] = pVar->epicsMotorLowLim[i];
            /* Convert deg/min to deg/sec  */
            pVar->epicsMotorVel[i]     = pVar->smartVelocity[i] / 60.;
            if (pVar->epicsMotorVel[i] > pVar->epicsMotorMaxVel[i])
                   pVar->epicsMotorVel[i] = pVar->epicsMotorMaxVel[i];
            /* Compute the acceleration time to keep deg/sec/sec constant */
            pVar->epicsMotorAccel[i] = (pVar->epicsMotorVel[i] / 
                                        pVar->epicsMotorMaxVel[i]) *
                                        pVar->epicsMotorMaxAccel[i];
            if (pVar->debugLevel >= 3) printf("epicsMotorPos[%d] =%f\n", i, pVar->epicsMotorPos[i]);
            if (pVar->debugLevel >= 3) printf("epicsMotorVel[%d] =%f\n", i, pVar->epicsMotorVel[i]);
            if (pVar->debugLevel >= 3) printf("epicsMotorAccel[%d] =%f\n", i, pVar->epicsMotorAccel[i]);
        }
    }
}


/* readSmart reads a string from the SMART system. It waits "forever". */
static int readSmart(SS_ID ssId, struct UserVar *pVar)
{
    asynStatus status;
    int nread;
    int eomReason;

    /* Note, timeout of -1.0 means no timeout, wait forever */
    status = pasynOctetSyncIO->read((asynUser *)pVar->pasynUser, pVar->smartIn,
                               sizeof(pVar->smartIn), -1.0,
                               &nread, &eomReason);
    if (pVar->debugLevel >= 5) printf("readSmart: returning, status=%d\n", status);
    return(status);
}

/* writeSmart writes a string to the SMART system. */
static int writeSmart(SS_ID ssId, struct UserVar *pVar)
{
    asynStatus status;
    int nwrite;

    status = pasynOctetSyncIO->write((asynUser *)pVar->pasynUser, pVar->smartOut,
                                      strlen(pVar->smartOut), 1.0, &nwrite);
    if (pVar->debugLevel >= 3) printf("writeSmart: sent %s\n", pVar->smartOut);
    return(status);
}

/* getEpicsMotorMoving returns the EPICS moving status of each motor, packed 
 * into a single int.  
 * Bit 0 = motor 1, bit 1 = motor 2, etc. 0=not moving, 1=moving.
 * If the entire int is 0 then no motors are moving */
static int getEpicsMotorMoving(SS_ID ssId, struct UserVar *pVar)
{
    int j;
    int result=0, mask=0x01;

    for (j=0; j<MAX_AXES; j++) {
        if (pVar->motorExists[j]) {
            seq_pvGet(ssId, pVar->epicsMotorDoneIndex[j], 0);
            if (pVar->epicsMotorDone[j] == 0) result |= mask;
            mask = mask << 1;
        }
    }
    return(result);
}

/* waitEpicsMotors waits for all motors to stop moving using the EPICS motor
 * records.*/
static void waitEpicsMotors(SS_ID ssId, struct UserVar *pVar)
{
    while(getEpicsMotorMoving(ssId, pVar)) {
        epicsThreadSleep(POLL_INTERVAL);
    }
}

}%
