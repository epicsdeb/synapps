program dxpMED("P=13GE2:med:,DXP=dxp,MCA=mca,N_DETECTORS=16")

/*  This sequencer program works with dxp.db and dxpMED.db. 
 *  It supports copying PVs from the first DXP to all of the DXPs
 *  and multi-element MCA operations
 *
 *  Mark Rivers
 *  January 7, 2005
 *
 * This program must be compiled with the recursive option so that multiple copies
 * can run at once */
option +r;

/* Allow the program to start, even if not all assigned channels are connected.
 * This could be useful if dxpMED is running on a different ioc. */
option -c;

%%#include <stdlib.h>
%%#include <errlog.h>

#define MAX_DETECTORS 32
#define MAX_SCAS 16
/* MAX_ROIS must be defined as MAX_DETECTORS * MAX_SCAS.
   It can't be done in SNL because of syntax limitations. */
#define MAX_ROIS 512

int i;
int j;
int k;
int chan;
int det;
int nDetectors;
int prevAcquiring;
int allDone;
char temp[100];

char *prefix;
char *dxp;
char *mca;

double  TRIG_PKTIM[MAX_DETECTORS];
assign  TRIG_PKTIM to {"","","","","","","","","","","","","","","","",
                       "","","","","","","","","","","","","","","",""};
int     copyTRIG_PKTIM; assign copyTRIG_PKTIM to "{P}CopyTRIG_PKTIM";
monitor copyTRIG_PKTIM; evflag copyTRIG_PKTIMMon; sync copyTRIG_PKTIM copyTRIG_PKTIMMon;

double  TRIG_GAPTIM[MAX_DETECTORS];
assign  TRIG_GAPTIM to {"","","","","","","","","","","","","","","","",
                        "","","","","","","","","","","","","","","",""};
int     copyTRIG_GAPTIM; assign copyTRIG_GAPTIM to "{P}CopyTRIG_GAPTIM";
monitor copyTRIG_GAPTIM; evflag copyTRIG_GAPTIMMon; sync copyTRIG_GAPTIM copyTRIG_GAPTIMMon;

double  FAST_TRIG[MAX_DETECTORS]; 
assign  FAST_TRIG to {"","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","",""};
int     copyFAST_TRIG; assign copyFAST_TRIG to "{P}CopyFAST_TRIG";
monitor copyFAST_TRIG; evflag copyFAST_TRIGMon; sync copyFAST_TRIG copyFAST_TRIGMon;

double  PKTIM[MAX_DETECTORS]; 
assign  PKTIM to {"","","","","","","","","","","","","","","","",
                  "","","","","","","","","","","","","","","",""};
int     copyPKTIM; assign copyPKTIM to "{P}CopyPKTIM";
monitor copyPKTIM; evflag copyPKTIMMon; sync copyPKTIM copyPKTIMMon;

double  GAPTIM[MAX_DETECTORS]; 
assign  GAPTIM to {"","","","","","","","","","","","","","","","",
                   "","","","","","","","","","","","","","","",""};
int     copyGAPTIM; assign copyGAPTIM to "{P}CopyGAPTIM";
monitor copyGAPTIM; evflag copyGAPTIMMon; sync copyGAPTIM copyGAPTIMMon;

double  SLOW_TRIG[MAX_DETECTORS]; 
assign  SLOW_TRIG to {"","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","",""};
int     copySLOW_TRIG; assign copySLOW_TRIG to "{P}CopySLOW_TRIG";
monitor copySLOW_TRIG; evflag copySLOW_TRIGMon; sync copySLOW_TRIG copySLOW_TRIGMon;

double  MAX_WIDTH[MAX_DETECTORS]; 
assign  MAX_WIDTH to {"","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","",""};
int     copyMAX_WIDTH; assign copyMAX_WIDTH to "{P}CopyMAX_WIDTH";
monitor copyMAX_WIDTH; evflag copyMAX_WIDTHMon; sync copyMAX_WIDTH copyMAX_WIDTHMon;

double  BASE_CUT_PCT[MAX_DETECTORS]; 
assign  BASE_CUT_PCT to {"","","","","","","","","","","","","","","","",
                         "","","","","","","","","","","","","","","",""}; 
int     copyBASE_CUT_PCT; assign copyBASE_CUT_PCT to "{P}CopyBASE_CUT_PCT";
monitor copyBASE_CUT_PCT; evflag copyBASE_CUT_PCTMon; sync copyBASE_CUT_PCT copyBASE_CUT_PCTMon;

double  BASE_CUT_ENBL[MAX_DETECTORS]; 
assign  BASE_CUT_ENBL to {"","","","","","","","","","","","","","","","",
                          "","","","","","","","","","","","","","","",""};
int     copyBASE_CUT_ENBL; assign copyBASE_CUT_ENBL to "{P}CopyBASE_CUT_ENBL";
monitor copyBASE_CUT_ENBL; evflag copyBASE_CUT_ENBLMon; sync copyBASE_CUT_ENBL copyBASE_CUT_ENBLMon;

double  BASE_THRESH[MAX_DETECTORS]; 
assign  BASE_THRESH to {"","","","","","","","","","","","","","","","",
                        "","","","","","","","","","","","","","","",""};
int     copyBASE_THRESH; assign copyBASE_THRESH to "{P}CopyBASE_THRESH";
monitor copyBASE_THRESH; evflag copyBASE_THRESHMon; sync copyBASE_THRESH copyBASE_THRESHMon;

double  BASE_THRESH_ENBL[MAX_DETECTORS]; 
assign  BASE_THRESH_ENBL to {"","","","","","","","","","","","","","","","",
                             "","","","","","","","","","","","","","","",""};
int     copyBASE_THRESH_ENBL; assign copyBASE_THRESH_ENBL to "{P}CopyBASE_THRESH_ENBL";
monitor copyBASE_THRESH_ENBL; evflag copyBASE_THRESH_ENBLMon; sync copyBASE_THRESH_ENBL copyBASE_THRESH_ENBLMon;

double  BASE_LEN[MAX_DETECTORS]; 
assign  BASE_LEN to {"","","","","","","","","","","","","","","","",
                     "","","","","","","","","","","","","","","",""};
int     copyBASE_LEN; assign copyBASE_LEN to "{P}CopyBASE_LEN";
monitor copyBASE_LEN; evflag copyBASE_LENMon; sync copyBASE_LEN copyBASE_LENMon;

double  PGAIN[MAX_DETECTORS]; 
assign  PGAIN to {"","","","","","","","","","","","","","","","",
                  "","","","","","","","","","","","","","","",""};
int     copyPGAIN; assign copyPGAIN to "{P}CopyPGAIN";
monitor copyPGAIN; evflag copyPGAINMon; sync copyPGAIN copyPGAINMon;

double  EMAX[MAX_DETECTORS]; 
assign  EMAX to {"","","","","","","","","","","","","","","","",
                 "","","","","","","","","","","","","","","",""};
int     copyEMAX; assign copyEMAX to "{P}CopyEMAX";
monitor copyEMAX; evflag copyEMAXMon; sync copyEMAX copyEMAXMon;

double  ADC_RULE[MAX_DETECTORS]; 
assign  ADC_RULE to {"","","","","","","","","","","","","","","","",
                     "","","","","","","","","","","","","","","",""};
int     copyADC_RULE; assign copyADC_RULE to "{P}CopyADC_RULE";
monitor copyADC_RULE; evflag copyADC_RULEMon; sync copyADC_RULE copyADC_RULEMon;

int     readDXP[MAX_DETECTORS]; 
assign  readDXP to {"","","","","","","","","","","","","","","","",
                    "","","","","","","","","","","","","","","",""};
int     readDXPs; assign readDXPs to "{P}DoReadDXPs";
monitor readDXPs; evflag readDXPsMon; sync readDXPs readDXPsMon;

int     readDXPBaseline[MAX_DETECTORS]; 
assign  readDXPBaseline to {"","","","","","","","","","","","","","","","",
                            "","","","","","","","","","","","","","","",""};
int     readDXPBaselines; assign readDXPBaselines to "{P}DoReadDXPBaselines";
monitor readDXPBaselines; evflag readDXPBaselinesMon; sync readDXPBaselines readDXPBaselinesMon;

int     readDXPBaselineHistory[MAX_DETECTORS]; 
assign  readDXPBaselineHistory to {"","","","","","","","","","","","","","","","",
                                   "","","","","","","","","","","","","","","",""};
int     readDXPBaselineHistories; assign readDXPBaselineHistories to "{P}DoReadDXPBaselineHistories";
monitor readDXPBaselineHistories; evflag readDXPBaselineHistoriesMon; sync readDXPBaselineHistories readDXPBaselineHistoriesMon;

int     readDXPTrace[MAX_DETECTORS]; 
assign  readDXPTrace to {"","","","","","","","","","","","","","","","",
                         "","","","","","","","","","","","","","","",""};
int     readDXPTraces; assign readDXPTraces to "{P}DoReadDXPTraces";
monitor readDXPTraces; evflag readDXPTracesMon; sync readDXPTraces readDXPTracesMon;

double  DXPTraceWait[MAX_DETECTORS]; 
assign  DXPTraceWait to {"","","","","","","","","","","","","","","","",
                         "","","","","","","","","","","","","","","",""};
double  DXPTraceWaits; assign DXPTraceWaits to "{P}TraceWaits";
monitor DXPTraceWaits; evflag DXPTraceWaitsMon; sync DXPTraceWaits DXPTraceWaitsMon;

double  DXPBaselineHistoryTime[MAX_DETECTORS]; 
assign  DXPBaselineHistoryTime to {"","","","","","","","","","","","","","","","",
                                   "","","","","","","","","","","","","","","",""};
double  DXPBaselineHistoryTimes; assign DXPBaselineHistoryTimes to "{P}BaselineHistoryTimes";
monitor DXPBaselineHistoryTimes; evflag DXPBaselineHistoryTimesMon; sync DXPBaselineHistoryTimes DXPBaselineHistoryTimesMon;

int     readMCA[MAX_DETECTORS]; 
assign  readMCA to {"","","","","","","","","","","","","","","","",
                    "","","","","","","","","","","","","","","",""};
int     ReadMCAs; assign ReadMCAs to "{P}DoReadAll";
monitor ReadMCAs; evflag ReadMCAsMon; sync ReadMCAs ReadMCAsMon;
int     readMCAAll; assign readMCAAll to "";

int     statusMCA[MAX_DETECTORS];
assign  statusMCA to {"","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","",""};
int     StatusMCAs; assign StatusMCAs to "{P}DoStatusAll";
monitor StatusMCAs; evflag StatusMCAsMon; sync StatusMCAs StatusMCAsMon;
int     statusMCAAll; assign statusMCAAll to "";

int     mcaAcquiring[MAX_DETECTORS]; 
assign  mcaAcquiring to {"","","","","","","","","","","","","","","","",
                         "","","","","","","","","","","","","","","",""};
monitor mcaAcquiring; evflag  mcaAcquiringMon; sync mcaAcquiring mcaAcquiringMon;
int     Acquiring; assign Acquiring to "";
monitor Acquiring; evflag AcquiringMon; sync Acquiring AcquiringMon;

double  mcaPresetReal[MAX_DETECTORS]; 
assign  mcaPresetReal to {"","","","","","","","","","","","","","","","",
                          "","","","","","","","","","","","","","","",""};
double  PresetReal; assign PresetReal to "{P}PresetReal";
monitor PresetReal; evflag PresetRealMon; sync PresetReal PresetRealMon;

double  mcaPresetLive[MAX_DETECTORS]; 
assign  mcaPresetLive to {"","","","","","","","","","","","","","","","",
                          "","","","","","","","","","","","","","","",""};
double  PresetLive; assign PresetLive to "{P}PresetLive";
monitor PresetLive; evflag PresetLiveMon; sync PresetLive PresetLiveMon;

double  mcaDeadTime[MAX_DETECTORS]; 
assign  mcaDeadTime to {"","","","","","","","","","","","","","","","",
                        "","","","","","","","","","","","","","","",""};
double  DeadTime; assign DeadTime to "{P}DeadTime";
evflag  DeadTimeMon; sync mcaDeadTime DeadTimeMon;

int     mcaROILow[MAX_ROIS]; 
assign  mcaROILow to {"","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","",""};
monitor mcaROILow; evflag  mcaROILowMon; sync mcaROILow mcaROILowMon;

int    mcaROIHigh[MAX_ROIS]; 
assign mcaROIHigh to {"","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","",""};
monitor mcaROIHigh; evflag  mcaROIHighMon; sync mcaROIHigh mcaROIHighMon;

int     dxpSCALow[MAX_ROIS]; 
assign  dxpSCALow to {"","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","",""};

int    dxpSCAHigh[MAX_ROIS]; 
assign dxpSCAHigh to {"","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","","",
                      "","","","","","","","","","","","","","","",""};

int     AcquireBusy; assign AcquireBusy to "{P}AcquireBusy";

ss dxp_control {
    state init {
        when() {
            /* We set the following event flags to force these modes and
               labels to be set correctly initially */
            prefix = macValueGet("P");
            dxp = macValueGet("DXP");
            mca = macValueGet("MCA");
            nDetectors = atoi(macValueGet("N_DETECTORS"));
            if ((nDetectors <= 0) || (nDetectors > MAX_DETECTORS)) {
              printf ("N_DETECTORS is illegal.\n");
              nDetectors = 0;
            }
            if (MAX_ROIS != (MAX_DETECTORS * MAX_SCAS)) {
              printf ("N_DETECTORS, MAX_SCAS and MAX_ROIS are inconsistent.\n");
              nDetectors = 0;
            }
            for (i=0; i<nDetectors; i++) {
               det = i+1;
               sprintf(temp, "%s%s%d.TRIG_PKTIM", prefix, dxp, det);
               pvAssign(TRIG_PKTIM[i], temp);
               sprintf(temp, "%s%s%d.TRIG_GAPTIM", prefix, dxp, det);
               pvAssign(TRIG_GAPTIM[i], temp);
               sprintf(temp, "%s%s%d.FAST_TRIG", prefix, dxp, det);
               pvAssign(FAST_TRIG[i], temp);
               sprintf(temp, "%s%s%d.PKTIM", prefix, dxp, det);
               pvAssign(PKTIM[i], temp);
               sprintf(temp, "%s%s%d.GAPTIM", prefix, dxp, det);
               pvAssign(GAPTIM[i], temp);
               sprintf(temp, "%s%s%d.SLOW_TRIG", prefix, dxp, det);
               pvAssign(SLOW_TRIG[i], temp);
               sprintf(temp, "%s%s%d.MAXWIDTH", prefix, dxp, det);
               pvAssign(MAX_WIDTH[i], temp);
               sprintf(temp, "%s%s%d.BASE_CUT_PCT", prefix, dxp, det);
               pvAssign(BASE_CUT_PCT[i], temp);
               sprintf(temp, "%s%s%d.T11V", prefix, dxp, det);
               pvAssign(BASE_CUT_ENBL[i], temp);
               sprintf(temp, "%s%s%d.BASE_THRESH", prefix, dxp, det);
               pvAssign(BASE_THRESH[i], temp);
               sprintf(temp, "%s%s%d.T14V", prefix, dxp, det);
               pvAssign(BASE_THRESH_ENBL[i], temp);
               sprintf(temp, "%s%s%d.BASE_LEN", prefix, dxp, det);
               pvAssign(BASE_LEN[i], temp);
               sprintf(temp, "%s%s%d.PGAIN", prefix, dxp, det);
               pvAssign(PGAIN[i], temp);
               sprintf(temp, "%s%s%d.EMAX", prefix, dxp, det);
               pvAssign(EMAX[i], temp);
               sprintf(temp, "%s%s%d.ADC_RULE", prefix, dxp, det);
               pvAssign(ADC_RULE[i], temp);
               sprintf(temp, "%s%s%d:ReadParams.PROC", prefix, dxp, det);
               pvAssign(readDXP[i], temp);
               sprintf(temp, "%s%s%d:ReadHistogram.PROC", prefix, dxp, det);
               pvAssign(readDXPBaseline[i], temp);
               sprintf(temp, "%s%s%d:ReadHistory.PROC", prefix, dxp, det);
               pvAssign(readDXPBaselineHistory[i], temp);
               sprintf(temp, "%s%s%d:ReadTrace.PROC", prefix, dxp, det);
               pvAssign(readDXPTrace[i], temp);
               sprintf(temp, "%s%s%d.TRACE_WAIT", prefix, dxp, det);
               pvAssign(DXPTraceWait[i], temp);
               sprintf(temp, "%s%s%d.BHIST_TIME", prefix, dxp, det);
               pvAssign(DXPBaselineHistoryTime[i], temp);
               sprintf(temp, "%s%s%d.READ", prefix, mca, det);
               pvAssign(readMCA[i], temp);
               sprintf(temp, "%s%s%d.PROC", prefix, mca, det);
               pvAssign(statusMCA[i], temp);
               sprintf(temp, "%s%s%d.ACQG", prefix, mca, det);
               pvAssign(mcaAcquiring[i], temp);
               sprintf(temp, "%s%s%d.PRTM", prefix, mca, det);
               pvAssign(mcaPresetReal[i], temp);
               sprintf(temp, "%s%s%d.PLTM", prefix, mca, det);
               pvAssign(mcaPresetLive[i], temp);
               sprintf(temp, "%s%s%d.IDTIM", prefix, mca, det);
               pvAssign(mcaDeadTime[i], temp);
               pvMonitor(mcaDeadTime[i]);
               for (j=0; j<MAX_SCAS; j++) {
                   k = i*MAX_SCAS + j;
                   sprintf(temp, "%s%s%d.R%iLO", prefix, mca, det, j);
                   pvAssign(mcaROILow[k], temp);
                   pvMonitor(mcaROILow[k]);
                   sprintf(temp, "%s%s%d.R%iHI", prefix, mca, det, j);
                   pvAssign(mcaROIHigh[k], temp);
                   pvMonitor(mcaROIHigh[k]);
                   sprintf(temp, "%s%s%d.SCA%i_LO", prefix, dxp, det, j);
                   pvAssign(dxpSCALow[k], temp);
                   sprintf(temp, "%s%s%d.SCA%i_HI", prefix, dxp, det, j);
                   pvAssign(dxpSCAHigh[k], temp);
               }
           }
           sprintf(temp, "%s%sAll.PROC", prefix, mca);
           pvAssign(statusMCAAll, temp);
           sprintf(temp, "%s%sAll.READ", prefix, mca);
           pvAssign(readMCAAll, temp);
           sprintf(temp, "%s%sAll.ACQG", prefix, mca);
           pvAssign(Acquiring, temp);
           prevAcquiring = Acquiring;
        } state waitConnected
    }
    state waitConnected {

        when (nDetectors <= 0) {
          printf (">>>>>>>>>>>>>> dxpMED is dead. <<<<<<<<<<<<<<\n");
        } state dead

        when (pvAssignCount () == pvConnectCount ()) {
          printf ("dxpMED: All channels connected.\n");
        } state monitor_changes

    }
    state dead {
      when (delay (3600.0)) {
      } state dead
    }

    state monitor_changes {

        when (pvAssignCount () != pvConnectCount ()) {
          printf ("dxpMED: Not all channels connected.\n");
        } state waitConnected

        when(efTestAndClear(copyTRIG_PKTIMMon) && (copyTRIG_PKTIM == 1)) {
            pvGet(TRIG_PKTIM[0]);
            for (i=1; i<nDetectors; i++) {
                TRIG_PKTIM[i] = TRIG_PKTIM[0];
                pvPut(TRIG_PKTIM[i]);
            }
            copyTRIG_PKTIM = 0;
            pvPut(copyTRIG_PKTIM);
        } state monitor_changes

        when(efTestAndClear(copyTRIG_GAPTIMMon) && (copyTRIG_GAPTIM == 1)) {
            pvGet(TRIG_GAPTIM[0]);
            for (i=1; i<nDetectors; i++) {
                TRIG_GAPTIM[i] = TRIG_GAPTIM[0];
                pvPut(TRIG_GAPTIM[i]);
            }
            copyTRIG_GAPTIM = 0;
            pvPut(copyTRIG_GAPTIM);
        } state monitor_changes

        when(efTestAndClear(copyFAST_TRIGMon) && (copyFAST_TRIG == 1)) {
            pvGet(FAST_TRIG[0]);
            for (i=1; i<nDetectors; i++) {
                FAST_TRIG[i] = FAST_TRIG[0];
                pvPut(FAST_TRIG[i]);
            }
            copyFAST_TRIG = 0;
            pvPut(copyFAST_TRIG);
        } state monitor_changes

        when(efTestAndClear(copyPKTIMMon) && (copyPKTIM == 1)) {
            pvGet(PKTIM[0]);
            for (i=1; i<nDetectors; i++) {
                PKTIM[i] = PKTIM[0];
                pvPut(PKTIM[i]);
            }
            copyPKTIM = 0;
            pvPut(copyPKTIM);
        } state monitor_changes

        when(efTestAndClear(copyGAPTIMMon) && (copyGAPTIM == 1)) {
            pvGet(GAPTIM[0]);
            for (i=1; i<nDetectors; i++) {
                GAPTIM[i] = GAPTIM[0];
                pvPut(GAPTIM[i]);
            }
            copyGAPTIM = 0;
            pvPut(copyGAPTIM);
        } state monitor_changes

        when(efTestAndClear(copySLOW_TRIGMon) && (copySLOW_TRIG == 1)) {
            pvGet(SLOW_TRIG[0]);
            for (i=1; i<nDetectors; i++) {
                SLOW_TRIG[i] = SLOW_TRIG[0];
                pvPut(SLOW_TRIG[i]);
            }
            copySLOW_TRIG = 0;
            pvPut(copySLOW_TRIG);
        } state monitor_changes

        when(efTestAndClear(copyMAX_WIDTHMon) && (copyMAX_WIDTH == 1)) {
            pvGet(MAX_WIDTH[0]);
            for (i=1; i<nDetectors; i++) {
                MAX_WIDTH[i] = MAX_WIDTH[0];
                pvPut(MAX_WIDTH[i]);
            }
            copyMAX_WIDTH = 0;
            pvPut(copyMAX_WIDTH);
        } state monitor_changes

        when(efTestAndClear(copyBASE_CUT_PCTMon) && (copyBASE_CUT_PCT == 1)) {
            pvGet(BASE_CUT_PCT[0]);
            for (i=1; i<nDetectors; i++) {
                BASE_CUT_PCT[i] = BASE_CUT_PCT[0];
                pvPut(BASE_CUT_PCT[i]);
            }
            copyBASE_CUT_PCT = 0;
            pvPut(copyBASE_CUT_PCT);
        } state monitor_changes

        when(efTestAndClear(copyBASE_CUT_ENBLMon) && (copyBASE_CUT_ENBL == 1)) {
            pvGet(BASE_CUT_ENBL[0]);
            for (i=1; i<nDetectors; i++) {
                BASE_CUT_ENBL[i] = BASE_CUT_ENBL[0];
                pvPut(BASE_CUT_ENBL[i]);
            }
            copyBASE_CUT_ENBL = 0;
            pvPut(copyBASE_CUT_ENBL);
        } state monitor_changes

        when(efTestAndClear(copyBASE_THRESHMon) && (copyBASE_THRESH == 1)) {
            pvGet(BASE_THRESH[0]);
            for (i=1; i<nDetectors; i++) {
                BASE_THRESH[i] = BASE_THRESH[0];
                pvPut(BASE_THRESH[i]);
            }
            copyBASE_THRESH = 0;
            pvPut(copyBASE_THRESH);
        } state monitor_changes

        when(efTestAndClear(copyBASE_THRESH_ENBLMon) && (copyBASE_THRESH_ENBL == 1)) {
            pvGet(BASE_THRESH_ENBL[0]);
            for (i=1; i<nDetectors; i++) {
                BASE_THRESH_ENBL[i] = BASE_THRESH_ENBL[0];
                pvPut(BASE_THRESH_ENBL[i]);
            }
            copyBASE_THRESH_ENBL = 0;
            pvPut(copyBASE_THRESH_ENBL);
        } state monitor_changes

        when(efTestAndClear(copyBASE_LENMon) && (copyBASE_LEN == 1)) {
            pvGet(BASE_LEN[0]);
            for (i=1; i<nDetectors; i++) {
                BASE_LEN[i] = BASE_LEN[0];
                pvPut(BASE_LEN[i]);
            }
            copyBASE_LEN = 0;
            pvPut(copyBASE_LEN);
        } state monitor_changes

        when(efTestAndClear(copyPGAINMon) && (copyPGAIN == 1)) {
            pvGet(PGAIN[0]);
            for (i=1; i<nDetectors; i++) {
                PGAIN[i] = PGAIN[0];
                pvPut(PGAIN[i]);
            }
            copyPGAIN = 0;
            pvPut(copyPGAIN);
        } state monitor_changes

        when(efTestAndClear(copyEMAXMon) && (copyEMAX == 1)) {
            pvGet(EMAX[0]);
            for (i=1; i<nDetectors; i++) {
                EMAX[i] = EMAX[0];
                pvPut(EMAX[i]);
            }
            copyEMAX = 0;
            pvPut(copyEMAX);
        } state monitor_changes

        when(efTestAndClear(copyADC_RULEMon) && (copyADC_RULE == 1)) {
            pvGet(ADC_RULE[0]);
            for (i=1; i<nDetectors; i++) {
                ADC_RULE[i] = ADC_RULE[0];
                pvPut(ADC_RULE[i]);
            }
            copyADC_RULE = 0;
            pvPut(copyADC_RULE);
        } state monitor_changes

        when(efTestAndClear(readDXPsMon) && (readDXPs == 1)) {
            for (i=0; i<nDetectors; i++) {
                readDXP[i] = 1;
                pvPut(readDXP[i]);
            }
            readDXPs = 0;
            pvPut(readDXPs);
        } state monitor_changes

        when(efTestAndClear(readDXPBaselinesMon) && (readDXPBaselines == 1)) {
            for (i=0; i<nDetectors; i++) {
                readDXPBaseline[i] = 1;
                pvPut(readDXPBaseline[i]);
            }
            readDXPBaselines = 0;
            pvPut(readDXPBaselines);
        } state monitor_changes

        when(efTestAndClear(readDXPBaselineHistoriesMon) && (readDXPBaselineHistories == 1)) {
            for (i=0; i<nDetectors; i++) {
                readDXPBaselineHistory[i] = 1;
                pvPut(readDXPBaselineHistory[i]);
            }
            readDXPBaselineHistories = 0;
            pvPut(readDXPBaselineHistories);
        } state monitor_changes

        when(efTestAndClear(readDXPTracesMon) && (readDXPTraces == 1)) {
            for (i=0; i<nDetectors; i++) {
                readDXPTrace[i] = 1;
                pvPut(readDXPTrace[i]);
            }
            readDXPTraces = 0;
            pvPut(readDXPTraces);
        } state monitor_changes

        when(efTestAndClear(DXPTraceWaitsMon)) {
            for (i=0; i<nDetectors; i++) {
                DXPTraceWait[i] = DXPTraceWaits;
                pvPut(DXPTraceWait[i]);
            }
        } state monitor_changes

        when(efTestAndClear(DXPBaselineHistoryTimesMon)) {
            for (i=0; i<nDetectors; i++) {
                DXPBaselineHistoryTime[i] = DXPBaselineHistoryTimes;
                pvPut(DXPBaselineHistoryTime[i]);
            }
        } state monitor_changes

        when(efTestAndClear(ReadMCAsMon) && (ReadMCAs == 1)) {
            /* printf("dxpMED: read MCAs\n"); */
            for (i=0; i<nDetectors; i++) {
                readMCA[i] = 1;
                pvPut(readMCA[i]);
            }
            readMCAAll = 1;
            pvPut(readMCAAll);
            ReadMCAs = 0;
            pvPut(ReadMCAs);
        } state monitor_changes

        when(efTestAndClear(StatusMCAsMon) && (StatusMCAs == 1)) {
            /* printf("dxpMED: read MCA status\n"); */
            for (i=0; i<nDetectors; i++) {
                statusMCA[i] = 1;
                pvPut(statusMCA[i]);
            }
            statusMCAAll = 1;
            pvPut(statusMCAAll);
            StatusMCAs = 0;
            pvPut(StatusMCAs);
        } state monitor_changes


        when(efTestAndClear(AcquiringMon)) {
            /* printf("dxpMED: Acquiring=%d, prevAcquiring=%d\n", Acquiring, prevAcquiring); */
            /* On the transition from Acquiring=1 to 0 clear the AcquireBusy record */
            if ((Acquiring == 0) && (prevAcquiring == 1)) {
               AcquireBusy = 0;
               pvPut(AcquireBusy);
            }
            prevAcquiring = Acquiring;
        } state monitor_changes

        when(efTestAndClear(mcaAcquiringMon)) {
            /* Transition on one or more mcaAcquiring status, wait 10msec for mcaAll to also change */
            allDone = 1;
            for (i=0; i<nDetectors; i++) {
                if (mcaAcquiring[i]) allDone = 0;
            }
            /* If all individual MCAs are done, but mcaAll is not done, then force read on mcaAll */
            if (allDone && Acquiring) {
               readMCAAll = 1;
               pvPut(readMCAAll);
            }
        } state monitor_changes

        when(efTestAndClear(PresetRealMon)) {
            for (i=0; i<nDetectors; i++) {
                mcaPresetReal[i] = PresetReal;
                pvPut(mcaPresetReal[i]);
            }
        } state monitor_changes

        when(efTestAndClear(PresetLiveMon)) {
            for (i=0; i<nDetectors; i++) {
                mcaPresetLive[i] = PresetLive;
                pvPut(mcaPresetLive[i]);
            }
        } state monitor_changes

        when(efTestAndClear(DeadTimeMon)) {
            DeadTime = 0.;
            for (i=0; i<nDetectors; i++) {
                if (mcaDeadTime[i] > DeadTime) DeadTime = mcaDeadTime[i];
            }
            pvPut(DeadTime);
        } state monitor_changes
 
        when(efTestAndClear(mcaROILowMon) || efTestAndClear(mcaROIHighMon)) {
            /* Copy ROIs to SCAs */
            for (i=0; i<nDetectors; i++) {
                for (j=0; j<MAX_SCAS; j++) {
                    k = i*MAX_SCAS + j;
                    chan = mcaROILow[k];
                    if (chan < 0) chan = 0;
                    dxpSCALow[k] = chan;
                    pvPut(dxpSCALow[k]);
                    chan = mcaROIHigh[k];
                    if (chan < 0) chan = 0;
                    dxpSCAHigh[k] = chan;
                    pvPut(dxpSCAHigh[k]);
                }
            }
            pvPut(DeadTime);
        } state monitor_changes
 
   }
}

