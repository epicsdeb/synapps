#include "stdlib.h"
#include "handel.h"
#include "handel_generic.h"

#define NUM_SCAS 16
#define NUM_CHANNELS 16
#define NUM_LOOPS 10000

int main(int argc, char **argv)
{
    int loop, i;
    int sca_counts[NUM_SCAS];
    double num_scas = NUM_SCAS;

    printf("Initializing ...\n");
    xiaSetLogLevel(2);
    xiaInit("xmap16.ini");
    xiaStartSystem();

    for (i=0; i<NUM_CHANNELS; i++) {
         xiaSetAcquisitionValues(i, "number_of_scas", &num_scas);
    }
 
    for (loop=0; loop<NUM_LOOPS; loop++) {
        for (i=0; i<NUM_CHANNELS; i++) {
            xiaGetRunData(i, "sca", sca_counts);
        }
        printf("Loop = %d\n", loop);
    }
    return(0);
}
