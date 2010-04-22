#include <stdlib.h>
#include "handel.h"
#include "xerxes_generic.h"
#include "handel_generic.h"
#include "handel_constants.h"
#include <epicsTime.h>
#include <asynDriver.h>

#define NUM_MCA_CHANNELS 2048
#define COUNT_TIME 10.0
/* Clock speed of Saturn for real time and live time */
#define CHECK_STATUS(status) if (status) {printf("Error %d\n", status); exit(status);}
asynUser *pasynUser;

static int lookupParam(char **paramNames, unsigned short numParams, char *name) 
{
    int i;
    for (i=0; i<numParams; i++) {
        if (strcmp(paramNames[i], name) == 0) {
            return(i);
        }
    }
    return(-1);
}

int main(int argc, char **argv)
{
    int acquiring;
    double ereal, elive, icr, ocr;
    double clockSpeed;
    int events, triggers;
    int data[NUM_MCA_CHANNELS];
    int done=0;
    int status;
    int i, j, k;
    unsigned short numParams;
    unsigned short *paramValues;
    char **paramNames;
    double dvalue;
    epicsTimeStamp t[12];
    
    pasynUser = pasynManager->createAsynUser(0,0);
    

    printf("Initializing ...\n");
    status = xiaSetLogLevel(2);
    CHECK_STATUS(status);
    status = xiaInit("saturn.ini");
    CHECK_STATUS(status);
    status = xiaStartSystem();
    CHECK_STATUS(status);

    status = xiaGetNumParams(0, &numParams);
    CHECK_STATUS(status);
    printf("Number of parameters = %d\n", numParams);
    paramValues = (unsigned short *) calloc(numParams, sizeof(*paramValues));
    paramNames = (char **) calloc(numParams, sizeof(char *));
    for (i=0; i<numParams; i++) {
        paramNames[i] = (char *) malloc(MAX_DSP_PARAM_NAME_LEN * sizeof(char *));
        status = xiaGetParamName(0, i, paramNames[i]);
        CHECK_STATUS(status);
    }

    dvalue = NUM_MCA_CHANNELS;
    status = xiaSetAcquisitionValues(0, "number_mca_channels", &dvalue);
    CHECK_STATUS(status);

    dvalue = COUNT_TIME;
    status = xiaSetAcquisitionValues(0, "preset_runtime", &dvalue);
    CHECK_STATUS(status);

    printf("Starting run ...\n");
    epicsTimeGetCurrent(&t[0]);
    status = xiaStartRun(0, 0);
    CHECK_STATUS(status);
 
    while(!done) {
        status = xiaGetRunData(0, "run_active", &acquiring);
        CHECK_STATUS(status);
        if ((acquiring & XIA_RUN_HARDWARE) == 0) done=1;
    }
    epicsTimeGetCurrent(&t[1]);
    if (acquiring == XIA_RUN_HANDEL) {
        status = xiaStopRun(0);
        CHECK_STATUS(status);
    }
    epicsTimeGetCurrent(&t[2]);

    xiaGetRunData(0, "runtime", &ereal);
    CHECK_STATUS(status);
    epicsTimeGetCurrent(&t[3]);

    xiaGetRunData(0, "livetime", &elive);
    CHECK_STATUS(status);
    epicsTimeGetCurrent(&t[4]);

    status = xiaGetRunData(0, "input_count_rate", &icr);
    CHECK_STATUS(status);
    epicsTimeGetCurrent(&t[5]);

    status = xiaGetRunData(0, "output_count_rate", &ocr);
    CHECK_STATUS(status);
    epicsTimeGetCurrent(&t[6]);

    status = xiaGetRunData(0, "events_in_run", &events);
    CHECK_STATUS(status);
    epicsTimeGetCurrent(&t[7]);

    status = xiaGetRunData(0, "triggers", &triggers);
    CHECK_STATUS(status);
    epicsTimeGetCurrent(&t[8]);

    status = xiaGetRunData(0, "mca", data);
    CHECK_STATUS(status);
    epicsTimeGetCurrent(&t[9]);
    
    printf("\n");
    printf("Values read individually from Handel\n");
    printf("Real time        = %f\n", ereal);
    printf("Trigger livetime = %f\n", elive);
    printf("ICR              = %f\n", icr);
    printf("OCR              = %f\n", ocr);
    printf("Events           = %d\n", events);
    printf("Triggers         = %d\n", triggers);

    printf("Time for run                  = %f\n", epicsTimeDiffInSeconds(&t[1], &t[0]));
    printf("Time to stop run              = %f\n", epicsTimeDiffInSeconds(&t[2], &t[1]));
    printf("Time to read realtime         = %f\n", epicsTimeDiffInSeconds(&t[3], &t[2]));
    printf("Time to read trigger livetime = %f\n", epicsTimeDiffInSeconds(&t[4], &t[3]));
    printf("Time to read ICR              = %f\n", epicsTimeDiffInSeconds(&t[5], &t[4]));
    printf("Time to read OCR              = %f\n", epicsTimeDiffInSeconds(&t[6], &t[5]));
    printf("Time to read events           = %f\n", epicsTimeDiffInSeconds(&t[7], &t[6]));
    printf("Time to read triggers         = %f\n", epicsTimeDiffInSeconds(&t[8], &t[7]));
    printf("Time to read MCA spectrum     = %f\n", epicsTimeDiffInSeconds(&t[9], &t[8]));
    printf("Time to get elapsed info via Handel calls = %f\n", epicsTimeDiffInSeconds(&t[8], &t[2]));


    epicsTimeGetCurrent(&t[10]);
    /* Now get the elapsed parameters via the parameter list */
    status = xiaGetParamData(0, "values", paramValues);
    CHECK_STATUS(status);

    /* Determine the speed of the realtime and livetime clocks.
     * It is 16 times less than the system clock speed. */

    i = lookupParam(paramNames, numParams, "SYSMICROSEC");
    clockSpeed = (paramValues[i] * 1e6) / 16.;
    i = lookupParam(paramNames, numParams, "REALTIME0");
    j = lookupParam(paramNames, numParams, "REALTIME1");
    k = lookupParam(paramNames, numParams, "REALTIME2");
    ereal = (paramValues[k] * 65536. * 65536. + paramValues[i] * 65536. + paramValues[j]) / clockSpeed;
    i = lookupParam(paramNames, numParams, "LIVETIME0");
    j = lookupParam(paramNames, numParams, "LIVETIME1");
    k = lookupParam(paramNames, numParams, "LIVETIME2");
    elive = (paramValues[k] * 65536. * 65536. + paramValues[i] * 65536. + paramValues[j]) / clockSpeed;
    i = lookupParam(paramNames, numParams, "EVTSINRUN0");
    j = lookupParam(paramNames, numParams, "EVTSINRUN1");
    events = paramValues[i] * 65536 + paramValues[j];
    i = lookupParam(paramNames, numParams, "UNDERFLOWS0");
    j = lookupParam(paramNames, numParams, "UNDERFLOWS1");
    events += paramValues[i] * 65536 + paramValues[j];
    i = lookupParam(paramNames, numParams, "OVERFLOWS0");
    j = lookupParam(paramNames, numParams, "OVERFLOWS1");
    events += paramValues[i] * 65536 + paramValues[j];
    i = lookupParam(paramNames, numParams, "FASTPEAKS0");
    j = lookupParam(paramNames, numParams, "FASTPEAKS1");
    triggers = paramValues[i] * 65536 + paramValues[j];
    icr = triggers / elive;
    ocr = events / ereal;
    epicsTimeGetCurrent(&t[11]);

    printf("\n");
    printf("Values determined from raw parameter list\n");
    printf("Real time        = %f\n", ereal);
    printf("Trigger livetime = %f\n", elive);
    printf("ICR              = %f\n", icr);
    printf("OCR              = %f\n", ocr);
    printf("Events           = %d\n", events);
    printf("Triggers         = %d\n", triggers);
    printf("Time to get elapsed info via parameters list = %f\n", epicsTimeDiffInSeconds(&t[11], &t[10]));

    xiaExit();
    return(0);
}

