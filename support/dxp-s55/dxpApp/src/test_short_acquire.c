#include <stdlib.h>
#include <time.h>
#include "handel.h"
#include "handel_generic.h"
#include "handel_constants.h"

#define MAX_ITERATIONS 10000
#define MAX_CHECKS 1000
#define NUM_CHANNELS 16
#define NUM_MCA_CHANNELS 2048
#define COUNT_TIME 0.1
#define CHECK_STATUS(status) if (status) {printf("Error %d\n", status); exit(status);}

int main(int argc, char **argv)
{
    int iter, check, chan;
    int acquiring[NUM_CHANNELS];
    double ereal[NUM_CHANNELS];
    double elive[NUM_CHANNELS];
    int data[NUM_CHANNELS][NUM_MCA_CHANNELS];
    int done;
    int status;
    int ignore;
    double dvalue;

    printf("Initializing ...\n");
    status = xiaSetLogLevel(2);
    CHECK_STATUS(status);
    status = xiaInit("xmap16.ini");
    CHECK_STATUS(status);
    status = xiaStartSystem();
    CHECK_STATUS(status);

    dvalue = NUM_MCA_CHANNELS;
    status = xiaSetAcquisitionValues(-1, "number_mca_channels", &dvalue);
    CHECK_STATUS(status);
    for (chan=0; chan<NUM_CHANNELS; chan+=4) { 
        status = xiaBoardOperation(chan, "apply", &ignore);
        CHECK_STATUS(status);
    }

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

    for (iter=0; iter<MAX_ITERATIONS; iter++) {
        if ((iter % 1) == 0) printf("Iteration %d\n", iter+1);
        status = xiaStartRun(-1, 0);
        CHECK_STATUS(status);
 
        for (check=0; check<MAX_CHECKS; check++) {
            done = 1;
            for (chan=0; chan<NUM_CHANNELS; chan++) {
                xiaGetRunData(chan, "trigger_livetime", &elive[chan]);
                CHECK_STATUS(status);
                if (elive[chan] > 2*COUNT_TIME) {
                    printf("Error while acquiring: iteration=%d channel=%d elapsed live = %f\n",
                            iter+1, chan, elive[chan]);
                }
                xiaGetRunData(chan, "runtime", &ereal[chan]);
                CHECK_STATUS(status);
                if (ereal[chan] > 2*COUNT_TIME) {
                    printf("Error while acquiring: iteration=%d channel=%d elapsed ereal = %f\n",
                            iter+1, chan, ereal[chan]);
                }
                status = xiaGetRunData(chan, "run_active", &acquiring[chan]);
                CHECK_STATUS(status);
                if (acquiring[chan] & XIA_RUN_HARDWARE) done=0;
            }
            if (done) break;
        }
        for (chan=0; chan<NUM_CHANNELS; chan++) {
            if (acquiring[chan] == XIA_RUN_HANDEL) {
                status = xiaStopRun(-1);
                CHECK_STATUS(status);
                break;
            }
        }
        for (chan=0; chan<NUM_CHANNELS; chan++) {
            status = xiaGetRunData(chan, "mca", data[chan]);
            CHECK_STATUS(status);
            xiaGetRunData(chan, "trigger_livetime", &elive[chan]);
            CHECK_STATUS(status);
            if (elive[chan] > 2*COUNT_TIME) {
                printf("Error while stopped: iteration=%d channel=%d elapsed live = %f\n",
                        iter+1, chan, elive[chan]);
            }
            xiaGetRunData(chan, "runtime", &ereal[chan]);
            CHECK_STATUS(status);
            if (ereal[chan] > 2*COUNT_TIME) {
                printf("Error while stopped: iteration=%d channel=%d elapsed ereal = %f\n",
                        iter+1, chan, ereal[chan]);
            }
        }
    }
    return(0);
}
