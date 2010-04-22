#include "stdlib.h"
#include "handel.h"
#include "handel_generic.h"

#define NUM_CHANNELS 16
#define NUM_LOOPS 10000
#define CHECK_STATUS(status) if (status) {printf("Error %d\n", status); exit(status);}

int main(int argc, char **argv)
{
    int loop, i, j;
    int events[NUM_CHANNELS];
    int prev_events[NUM_CHANNELS];
    unsigned short params[4000];
    int icr, ocr, acquiring;
    int status;

    printf("Initializing ...\n");
    status = xiaSetLogLevel(2);
    CHECK_STATUS(status);
    status = xiaInit("xmap16.ini");
    CHECK_STATUS(status);
    status = xiaStartSystem();
    CHECK_STATUS(status);

    for (i=0; i<NUM_CHANNELS; i++) {
         status = xiaStartRun(i, 0);
         CHECK_STATUS(status);
         prev_events[i]=0;
    }
 
    for (loop=0; loop<NUM_LOOPS; loop++) {
        for (i=0; i<NUM_CHANNELS; i++) {
            status = xiaGetParamData(i, "values", params);
            CHECK_STATUS(status);
            status = xiaGetRunData(i, "input_count_rate", &icr);
            CHECK_STATUS(status);
            status = xiaGetRunData(i, "output_count_rate", &ocr);
            CHECK_STATUS(status);
            status = xiaGetRunData(i, "events_in_run", &events[i]);
            CHECK_STATUS(status);
            status = xiaGetRunData(i, "run_active", &acquiring);
            CHECK_STATUS(status);
        }
        for (i=0; i<NUM_CHANNELS; i++) {
            if (events[i] < prev_events[i]) {
                printf("Glitch on channel %d in loop %d\n", i, loop);
                printf("Previous events:");
                for (j=0; j<NUM_CHANNELS; j++) printf("%8d", prev_events[j]);
                printf("\n");
                printf("         Events:");
                for (j=0; j<NUM_CHANNELS; j++) printf("%8d", events[j]);
                printf("\n");
            }
        }
        for (i=0; i<NUM_CHANNELS; i++) prev_events[i] = events[i];
    }
    return(0);
}
