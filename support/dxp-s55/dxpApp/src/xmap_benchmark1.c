#include <stdlib.h>
#include "handel.h"
#include "xerxes_generic.h"
#include "handel_generic.h"
#include "handel_constants.h"
#include <epicsTime.h>

#define NUM_MCA_CHANNELS 2048
#define NUM_XMAP_CHANNELS 12
#define XMAP_INI_FILE "xmap12.ini"
#define NUM_XMAP_CHANNELS_PER_MODULE 4
#define COUNT_TIME 1.0
/* Clock speed of Saturn for real time and live time */
#define CHECK_STATUS(status) if (status) {printf("Error %d, line %d\n", status, __LINE__); exit(status);}

typedef struct moduleStats {
    double realTime;
    double triggerLiveTime;
    double energyLiveTime;
    double triggers;
    double events;
    double icr;
    double ocr;
} moduleStats;

static int lookupParam(char **paramNames, unsigned short numParams, char *name) 
{
    int i;
    for (i=0; i<numParams; i++) {
        if (strcmp(paramNames[i], name) == 0) {
            return(i);
        }
    }
    printf("ERROR: cannot find parameter %s\n", name);
    return(-1);
}

static double dxp_to_double(unsigned short *values, int offsets[4])
{
  double d;

  d = (((values[offsets[3]]*65536. + values[offsets[2]])*65536. + values[offsets[1]])*65536.) + values[offsets[0]];

  return d;
}

int main(int argc, char **argv)
{
    int acquiring;
    double realTime, triggerLiveTime, energyLiveTime, icr, ocr;
    double clockSpeed;
    int events, triggers, overFlows, underFlows, totalEvents;
    int triggerOffsets[4], eventOffsets[4];
    int realTimeOffsets[4], triggerLiveTimeOffsets[4], energyLiveTimeOffsets[4];
    int overFlowOffsets[4], underFlowOffsets[4];
    int data[NUM_XMAP_CHANNELS_PER_MODULE * NUM_MCA_CHANNELS];
    int chan;
    int done=0;
    int status;
    int i;
    unsigned short numParams;
    unsigned short *paramValues;
    char **paramNames;
    double dvalue;
    moduleStats moduleStatistics[NUM_XMAP_CHANNELS_PER_MODULE];
    epicsTimeStamp t[20];
    
    printf("Initializing ...\n");
    status = xiaSetLogLevel(2);
    CHECK_STATUS(status);
    status = xiaInit(XMAP_INI_FILE);
    CHECK_STATUS(status);
    status = xiaStartSystem();
    CHECK_STATUS(status);

    status = xiaGetNumParams(0, &numParams);
    CHECK_STATUS(status);
    printf("Number of parameters = %d\n", numParams);
    paramValues = (unsigned short *) calloc(numParams, sizeof(*paramValues));
    paramNames = (char **) calloc(numParams, sizeof(char *));
    for (i=0; i<numParams; i++) {
        paramNames[i] = (char *) calloc(MAX_DSP_PARAM_NAME_LEN, sizeof(char *));
        status = xiaGetParamName(0, i, paramNames[i]);
        CHECK_STATUS(status);
    }

    dvalue = NUM_MCA_CHANNELS;
    status = xiaSetAcquisitionValues(-1, "number_mca_channels", &dvalue);
    CHECK_STATUS(status);

    dvalue = XIA_PRESET_FIXED_REAL;
    status = xiaSetAcquisitionValues(-1, "preset_type", &dvalue);
    CHECK_STATUS(status);

    dvalue = COUNT_TIME;
    status = xiaSetAcquisitionValues(-1, "preset_value", &dvalue);
    CHECK_STATUS(status);

    for (i=0; i<NUM_XMAP_CHANNELS; i+=NUM_XMAP_CHANNELS_PER_MODULE) {
        status = xiaBoardOperation(i/NUM_XMAP_CHANNELS_PER_MODULE, "apply", &dvalue);
        CHECK_STATUS(status);
    }
    
    i = lookupParam(paramNames, numParams, "SYSMICROSEC");
    clockSpeed = (paramValues[i] * 1e6) / 16.;
    realTimeOffsets[0] = lookupParam(paramNames, numParams, "REALTIME");
    realTimeOffsets[1] = lookupParam(paramNames, numParams, "REALTIMEA");
    realTimeOffsets[2] = lookupParam(paramNames, numParams, "REALTIMEB");
    realTimeOffsets[3] = lookupParam(paramNames, numParams, "REALTIMEC");

    triggerLiveTimeOffsets[0] = lookupParam(paramNames, numParams, "TLIVETIME");
    triggerLiveTimeOffsets[1] = lookupParam(paramNames, numParams, "TLIVETIMEA");
    triggerLiveTimeOffsets[2] = lookupParam(paramNames, numParams, "TLIVETIMEB");
    triggerLiveTimeOffsets[3] = lookupParam(paramNames, numParams, "TLIVETIMEC");

    energyLiveTimeOffsets[0] = lookupParam(paramNames, numParams, "ELIVETIME");
    energyLiveTimeOffsets[1] = lookupParam(paramNames, numParams, "ELIVETIMEA");
    energyLiveTimeOffsets[2] = lookupParam(paramNames, numParams, "ELIVETIMEB");
    energyLiveTimeOffsets[3] = lookupParam(paramNames, numParams, "ELIVETIMEC");

    eventOffsets[0] = lookupParam(paramNames, numParams, "MCAEVENTS");
    eventOffsets[1] = lookupParam(paramNames, numParams, "MCAEVENTSA");
    eventOffsets[2] = lookupParam(paramNames, numParams, "MCAEVENTSB");
    eventOffsets[3] = lookupParam(paramNames, numParams, "MCAEVENTSC");

    underFlowOffsets[0] = lookupParam(paramNames, numParams, "UNDERFLOWS");
    underFlowOffsets[1] = lookupParam(paramNames, numParams, "UNDERFLOWSA");
    underFlowOffsets[2] = lookupParam(paramNames, numParams, "UNDERFLOWSB");
    underFlowOffsets[3] = lookupParam(paramNames, numParams, "UNDERFLOWSC");

    overFlowOffsets[0] = lookupParam(paramNames, numParams, "OVERFLOWS");
    overFlowOffsets[1] = lookupParam(paramNames, numParams, "OVERFLOWSA");
    overFlowOffsets[2] = lookupParam(paramNames, numParams, "OVERFLOWSB");
    overFlowOffsets[3] = lookupParam(paramNames, numParams, "OVERFLOWSC");

    triggerOffsets[0] = lookupParam(paramNames, numParams, "TRIGGERS");
    triggerOffsets[1] = lookupParam(paramNames, numParams, "TRIGGERSA");
    triggerOffsets[2] = lookupParam(paramNames, numParams, "TRIGGERSB");
    triggerOffsets[3] = lookupParam(paramNames, numParams, "TRIGGERSC");


    printf("Starting run ...\n");
    epicsTimeGetCurrent(&t[0]);
    status = xiaStartRun(-1, 0);
    CHECK_STATUS(status);
 
    while(1) {
        done = 1;
        for (chan=0; chan<NUM_XMAP_CHANNELS; chan++) {
            status = xiaGetRunData(0, "run_active", &acquiring);
            CHECK_STATUS(status);
            if (acquiring & XIA_RUN_HARDWARE) done = 0;
        }
        if (done) break;
    }
    epicsTimeGetCurrent(&t[1]);
    if (acquiring == XIA_RUN_HANDEL) {
        status = xiaStopRun(-1);
        CHECK_STATUS(status);
    }
    epicsTimeGetCurrent(&t[2]);

    for (chan=0; chan<NUM_XMAP_CHANNELS; chan++) {
        xiaGetRunData(chan, "runtime", &realTime);
        CHECK_STATUS(status);
        epicsTimeGetCurrent(&t[3]);

        xiaGetRunData(chan, "trigger_livetime", &triggerLiveTime);
        CHECK_STATUS(status);
        epicsTimeGetCurrent(&t[4]);

        xiaGetRunData(chan, "livetime", &energyLiveTime);
        CHECK_STATUS(status);
        epicsTimeGetCurrent(&t[5]);

        status = xiaGetRunData(chan, "input_count_rate", &icr);
        CHECK_STATUS(status);
        epicsTimeGetCurrent(&t[6]);

        status = xiaGetRunData(chan, "output_count_rate", &ocr);
        CHECK_STATUS(status);
        epicsTimeGetCurrent(&t[7]);

        status = xiaGetRunData(chan, "events_in_run", &events);
        CHECK_STATUS(status);
        epicsTimeGetCurrent(&t[8]);

        /* Cannot read triggers on xMAP?
        status = xiaGetRunData(chan, "triggers", &triggers);
        CHECK_STATUS(status);
        */
        epicsTimeGetCurrent(&t[9]);
        triggers = (icr * triggerLiveTime) + 0.5;

        status = xiaGetRunData(chan, "mca", data);
        CHECK_STATUS(status);
        epicsTimeGetCurrent(&t[10]);
    }
    
    printf("\n");
    printf("Values read individually from Handel\n");
    printf("Real time        = %f\n", realTime);
    printf("Trigger livetime = %f\n", triggerLiveTime);
    printf("Energy livetime  = %f\n", energyLiveTime);
    printf("ICR              = %f\n", icr);
    printf("OCR              = %f\n", ocr);
    printf("Events           = %d\n", events);
    printf("Triggers (calc)  = %d\n", triggers);

    printf("Time for run                  = %f\n", epicsTimeDiffInSeconds(&t[1], &t[0]));
    printf("Time to stop run              = %f\n", epicsTimeDiffInSeconds(&t[2], &t[1]));
    printf("Time to read realtime         = %f\n", epicsTimeDiffInSeconds(&t[3], &t[2]));
    printf("Time to read trigger livetime = %f\n", epicsTimeDiffInSeconds(&t[4], &t[3]));
    printf("Time to read energy livetime  = %f\n", epicsTimeDiffInSeconds(&t[5], &t[4]));
    printf("Time to read ICR              = %f\n", epicsTimeDiffInSeconds(&t[6], &t[5]));
    printf("Time to read OCR              = %f\n", epicsTimeDiffInSeconds(&t[7], &t[6]));
    printf("Time to read events           = %f\n", epicsTimeDiffInSeconds(&t[8], &t[7]));
    printf("Time to read triggers         = %f\n", epicsTimeDiffInSeconds(&t[9], &t[8]));
    printf("Time to read MCA spectrum     = %f\n", epicsTimeDiffInSeconds(&t[10], &t[9]));
    printf("Time to get all info via Handel calls = %f\n", epicsTimeDiffInSeconds(&t[10], &t[2]));

    realTime = 0.;
    triggerLiveTime = 0.;
    energyLiveTime = 0.;
    icr = 0.;
    ocr = 0.;
    triggers = 0.;
    events = 0.;

    epicsTimeGetCurrent(&t[11]);

    for (chan=0; chan<NUM_XMAP_CHANNELS; chan++) {
        /* Now get the elapsed parameters via the parameter list */
        status = xiaGetParamData(chan, "values", paramValues);
        i = lookupParam(paramNames, numParams, "SYSMICROSEC");
        clockSpeed = (paramValues[i] * 1e6) / 16.;
        CHECK_STATUS(status);
        realTime = dxp_to_double(paramValues, realTimeOffsets) / clockSpeed;

        triggerLiveTime = dxp_to_double(paramValues, triggerLiveTimeOffsets) / clockSpeed;

        energyLiveTime = dxp_to_double(paramValues, energyLiveTimeOffsets) / clockSpeed;

        events = dxp_to_double(paramValues, eventOffsets);

        underFlows = dxp_to_double(paramValues, underFlowOffsets);

        overFlows = dxp_to_double(paramValues, overFlowOffsets);

        triggers = dxp_to_double(paramValues, triggerOffsets);

        if (triggerLiveTime > 0.0) {
	        icr = triggers / triggerLiveTime;
        } else {
	        icr = 0.0;
        }
        
        totalEvents = events + underFlows + overFlows;

        if (realTime > 0.0) {
	        ocr = totalEvents / realTime;
        } else {
	        ocr = 0.0;
        }
        status = xiaGetRunData(chan, "mca", data);
        CHECK_STATUS(status);
    }

    epicsTimeGetCurrent(&t[12]);

    printf("\n");
    printf("Values determined from raw parameter list\n");
    printf("Real time        = %f\n", realTime);
    printf("Trigger livetime = %f\n", triggerLiveTime);
    printf("Energy livetime  = %f\n", energyLiveTime);
    printf("ICR              = %f\n", icr);
    printf("OCR              = %f\n", ocr);
    printf("Events           = %d\n", events);
    printf("Total events     = %d\n", totalEvents);
    printf("Triggers         = %d\n", triggers);
    printf("Time to get elapsed info via parameters list = %f\n", epicsTimeDiffInSeconds(&t[12], &t[11]));
    
    realTime = 0.;
    triggerLiveTime = 0.;
    energyLiveTime = 0.;
    icr = 0.;
    ocr = 0.;
    triggers = 0.;
    events = 0.;

    epicsTimeGetCurrent(&t[13]);

    /* Now get the information with the module_statistics aquisition parameters */
    for (chan=0; chan<NUM_XMAP_CHANNELS; chan+=NUM_XMAP_CHANNELS_PER_MODULE) {
        status = xiaGetRunData(chan, "module_statistics", (double *)&moduleStatistics);
        CHECK_STATUS(status);
        for (i=0; i<NUM_XMAP_CHANNELS_PER_MODULE; i++) {
            realTime = moduleStatistics[i].realTime;
            triggerLiveTime = moduleStatistics[i].triggerLiveTime;
            energyLiveTime = moduleStatistics[i].energyLiveTime;
            icr = moduleStatistics[i].icr;
            ocr = moduleStatistics[i].ocr;
            triggers = moduleStatistics[i].triggers;
            totalEvents = moduleStatistics[i].events;
        }
        status = xiaGetRunData(chan, "module_mca", data);
        CHECK_STATUS(status);
    }

    epicsTimeGetCurrent(&t[14]);

    printf("\n");
    printf("Values determined from module_statistics\n");
    printf("Real time        = %f\n", realTime);
    printf("Trigger livetime = %f\n", triggerLiveTime);
    printf("Energy livetime  = %f\n", energyLiveTime);
    printf("ICR              = %f\n", icr);
    printf("OCR              = %f\n", ocr);
    printf("Events           = %d\n", totalEvents);
    printf("Triggers         = %d\n", triggers);
    printf("Time to get elapsed info via module_statistics = %f\n", epicsTimeDiffInSeconds(&t[14], &t[13]));

    xiaExit();
    return(0);
}
