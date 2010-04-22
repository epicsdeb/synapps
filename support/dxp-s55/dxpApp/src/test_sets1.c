#include <stdlib.h>
#include <sys/time.h>
#include "handel.h"
#include "handel_generic.h"
#include "handel_constants.h"

#define MAX_ITERATIONS 5
#define MAX_CHECKS 1000
#define NUM_CHANNELS 16
#define NUM_MCA_CHANNELS 2048
#define COUNT_TIME 0.01
#define CHECK_STATUS(status) if (status) {printf("Error %d\n", status); exit(status);}

static struct timeval starttime;
static void startTimer();
static void endTimer(double *timediff);

int main(int argc, char **argv)
{
    int iter, check, chan;
    int acquiring[MAX_CHECKS][NUM_CHANNELS];
    double ereal[NUM_CHANNELS];
    double elive[NUM_CHANNELS];
    int data[NUM_CHANNELS][NUM_MCA_CHANNELS];
    int done;
    int status;
    int ignore;
    int npolls;
    double dvalue;
    double timediff, polltime[MAX_CHECKS];

    printf("Initializing ...\n");
    status = xiaSetLogLevel(2);
    CHECK_STATUS(status);
    startTimer();
    status = xiaInit("xmap16.ini");
    CHECK_STATUS(status);
    endTimer(&timediff);
    printf("Time for xiaInit=%f\n", timediff);
    startTimer();
    status = xiaStartSystem();
    CHECK_STATUS(status);
    endTimer(&timediff);
    printf("Time for xiaStartSystem=%f\n", timediff);

    startTimer();
    dvalue = XIA_PRESET_FIXED_REAL;
    status = xiaSetAcquisitionValues(-1, "preset_type", &dvalue);
    CHECK_STATUS(status);
    dvalue = COUNT_TIME;
    status = xiaSetAcquisitionValues(-1, "preset_values", &dvalue);
    CHECK_STATUS(status);
    dvalue = NUM_MCA_CHANNELS;
    status = xiaSetAcquisitionValues(-1, "number_mca_channels", &dvalue);
    CHECK_STATUS(status);
    for (chan=0; chan<NUM_CHANNELS; chan+=4) { 
        status = xiaBoardOperation(chan, "apply", &ignore);
        CHECK_STATUS(status);
    }
    endTimer(&timediff);
    printf("Time to set acquisition values=%f\n", timediff);


    for (iter=0; iter<MAX_ITERATIONS; iter++) {
        printf("\nStart acquisition %d\n", iter+1);
        status = xiaStartRun(-1, 0);
        CHECK_STATUS(status);
        startTimer();
 
        for (check=0; check<MAX_CHECKS; check++) {
            done = 1;
            for (chan=0; chan<NUM_CHANNELS; chan++) {
                if ((iter % 2) == 0) {
                    xiaGetRunData(chan, "trigger_livetime", &elive[chan]);
                    CHECK_STATUS(status);
                    xiaGetRunData(chan, "runtime", &ereal[chan]);
                    CHECK_STATUS(status);
                }
                status = xiaGetRunData(chan, "run_active", &acquiring[check][chan]);
                CHECK_STATUS(status);
                if (acquiring[check][chan] & XIA_RUN_HARDWARE) done=0;
            }
            endTimer(&timediff);
            polltime[check] = timediff;
            if (done) break;
        }
        endTimer(&timediff);
        npolls = check+1;
        printf("Time to detect run complete=%f, polls=%d\n", timediff, npolls);
        for (check=0; check<npolls; check++) {
            printf("   Poll %d time=%f, acquiring=", check+1, polltime[check]);
            for (chan=0; chan<NUM_CHANNELS; chan++) {
                printf(" %d", acquiring[check][chan]);
            }
            printf("\n");
        }
        startTimer();
        for (chan=0; chan<NUM_CHANNELS; chan++) {
            if (acquiring[npolls-1][chan] == XIA_RUN_HANDEL) {
                status = xiaStopRun(chan);
                CHECK_STATUS(status);
            }
        }
        endTimer(&timediff);
        printf("Time to stop runs=%f\n", timediff);
        startTimer();
        for (chan=0; chan<NUM_CHANNELS; chan++) {
            status = xiaGetRunData(chan, "mca", data[chan]);
            CHECK_STATUS(status);
        }
        endTimer(&timediff);
        printf("Time to read data=%f\n", timediff);
    }
    return(0);
}

static void startTimer()
{
    struct timezone timezone;
    
    gettimeofday(&starttime, &timezone);
}

static void endTimer(double *timediff)
{
    struct timeval endtime;
    struct timezone timezone;
    
    gettimeofday(&endtime, &timezone);
    *timediff = (endtime.tv_sec + endtime.tv_usec/1.e6) -
                (starttime.tv_sec + starttime.tv_usec/1.e6);
}
