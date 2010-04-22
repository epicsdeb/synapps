/* This program tests to see if there are any points with 0 live time or very low counts
 * when acquiring with the xMAP.  
 * I am seeing such problems with the DXP EPICS software, but not with this simple program
 * so it must be an EPICS problem. */

#include <stdlib.h>
#include "handel.h"
#include "handel_generic.h"
#include "handel_constants.h"
#include <epicsTime.h>

#define MAX_ITERATIONS 1000
#define MAX_CHECKS 1000
#define NUM_CHANNELS 12
#define NUM_MCA_CHANNELS 2048
#define COUNT_TIME 0.01
#define CHECK_STATUS(status) if (status) {printf("Error %d line %d\n", status, __LINE__); exit(status);}

typedef struct moduleStatistics {
    double realTime;
    double triggerLiveTime;
    double energyLiveTime;
    double triggers;
    double events;
    double icr;
    double ocr;
} moduleStatistics;

int main(int argc, char **argv)
{
    int iter, check, chan;
    int acquiring[NUM_CHANNELS];
    double realTime[NUM_CHANNELS][MAX_ITERATIONS];
    double liveTime[NUM_CHANNELS][MAX_ITERATIONS];
    double totalCounts[NUM_CHANNELS][MAX_ITERATIONS];
    double minLive, maxLive, minReal, maxReal, minCounts, maxCounts;
    moduleStatistics modStats[4];
    int data[NUM_MCA_CHANNELS];
    int done;
    int status;
    int ignore;
    double dvalue;
    double sum;
    int i;
    epicsTimeStamp tStart, tEnd;

    printf("Initializing ...\n");
    status = xiaSetLogLevel(2);
    CHECK_STATUS(status);
    status = xiaInit("xmap12.ini");
    CHECK_STATUS(status);
    status = xiaStartSystem();
    CHECK_STATUS(status);

    dvalue = NUM_MCA_CHANNELS;
    status = xiaSetAcquisitionValues(-1, "number_mca_channels", &dvalue);
    CHECK_STATUS(status);
    dvalue = XIA_PRESET_FIXED_REAL;
    status = xiaSetAcquisitionValues(-1, "preset_type", &dvalue);
    CHECK_STATUS(status);
    dvalue = COUNT_TIME;
    status = xiaSetAcquisitionValues(-1, "preset_values", &dvalue);
    CHECK_STATUS(status);
    for (chan=0; chan<NUM_CHANNELS; chan+=4) { 
        status = xiaBoardOperation(chan, "apply", &ignore);
        CHECK_STATUS(status);
    }


    epicsTimeGetCurrent(&tStart);
    for (iter=0; iter<MAX_ITERATIONS; iter++) {
        status = xiaStartRun(-1, 0);
        CHECK_STATUS(status);
 
        for (check=0; check<MAX_CHECKS; check++) {
            done = 1;
            for (chan=0; chan<NUM_CHANNELS; chan++) {
                status = xiaGetRunData(chan, "run_active", &acquiring[chan]);
                CHECK_STATUS(status);
                if (acquiring[chan] & XIA_RUN_HARDWARE) done=0;
            }
            if (done) break;
        }
        for (chan=0; chan<NUM_CHANNELS; chan++) {
            if (acquiring[chan] == XIA_RUN_HANDEL) {
                status = xiaStopRun(chan);
                CHECK_STATUS(status);
            }
        }
        for (chan=0; chan<NUM_CHANNELS; chan++) {
            if ((chan % 4) == 0) {
                status = xiaGetRunData(chan, "module_statistics", (double *)&modStats);
                CHECK_STATUS(status);
            }
            realTime[chan][iter] = modStats[chan % 4].realTime;
            liveTime[chan][iter] = modStats[chan % 4].energyLiveTime;
            
            status = xiaGetRunData(chan, "mca", data);
            CHECK_STATUS(status);
            sum = 0.;
            for (i=0; i<NUM_MCA_CHANNELS; i++) sum += data[i];
            totalCounts[chan][iter] = sum;
        }
    }
    epicsTimeGetCurrent(&tEnd);
    printf("Time for %d iterations with %f preset real time=%f\n", 
        MAX_ITERATIONS, COUNT_TIME, epicsTimeDiffInSeconds(&tEnd, &tStart));
    for (chan=0; chan<NUM_CHANNELS; chan++) {
        minLive = liveTime[chan][0];
        maxLive = liveTime[chan][0];
        minReal = realTime[chan][0];
        maxReal = realTime[chan][0];
        minCounts = totalCounts[chan][0];
        maxCounts = totalCounts[chan][0];
        for (iter=0; iter<MAX_ITERATIONS; iter++) {
             minLive = MIN(minLive, liveTime[chan][iter]);
             maxLive = MAX(maxLive, liveTime[chan][iter]);
             minReal = MIN(minReal, realTime[chan][iter]);
             maxReal = MAX(maxReal, realTime[chan][iter]);
             minCounts = MIN(minCounts, totalCounts[chan][iter]);
             maxCounts = MAX(maxCounts, totalCounts[chan][iter]);
        }
        printf("Channel %d\n"
               "  live time (min, max)    = %f, %f\n"
               "  real time (min, max)    = %f, %f\n"
               "  total counts (min, max) = %f, %f\n",
               chan, minLive, maxLive, minReal, maxReal, minCounts, maxCounts);
        for (iter=0; iter<MAX_ITERATIONS; iter++) {
            if ((liveTime[chan][iter] < COUNT_TIME/2.) ||
                (realTime[chan][iter] < COUNT_TIME)) {
                printf("Error, chan=%d iter=%d, realTime=%f, liveTime=%f, totalCounts=%f\n",
                    chan, iter, realTime[chan][iter], liveTime[chan][iter], totalCounts[chan][iter]);
            }
        }
    }
    return(0);
}
