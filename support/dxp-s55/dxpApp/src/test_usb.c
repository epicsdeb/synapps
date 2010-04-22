#include <stdlib.h>
#include <sys/time.h>
#include "handel.h"
#include "handel_generic.h"
#include "handel_constants.h"

#define NUM_MCA_CHANNELS 2048
#define COUNT_TIME 0.1
#define MAX_CHECKS 100
#define CHECK_STATUS(status) if (status) {printf("Error %d\n", status); exit(status);}

int main(int argc, char **argv)
{
    int check;
    int acquiring;
    double ereal;
    double elive;
    int data[NUM_MCA_CHANNELS];
    int done;
    int status;
    int ignore;
    double dvalue;

    printf("Initializing ...\n");
    status = xiaSetLogLevel(2);
    CHECK_STATUS(status);
    status = xiaInit("saturn.ini");
    CHECK_STATUS(status);
    status = xiaStartSystem();
    CHECK_STATUS(status);

    dvalue = NUM_MCA_CHANNELS;
    status = xiaSetAcquisitionValues(0, "number_mca_channels", &dvalue);
    CHECK_STATUS(status);

    dvalue = COUNT_TIME;
    status = xiaSetAcquisitionValues(0, "preset_runtime", &dvalue);
    CHECK_STATUS(status);
    status = xiaBoardOperation(0, "apply", &ignore);
    CHECK_STATUS(status);

    status = xiaStartRun(0, 0);
    CHECK_STATUS(status);
 
    for (check=0; check<MAX_CHECKS; check++) {
        xiaGetRunData(0, "livetime", &elive);
        CHECK_STATUS(status);
        xiaGetRunData(0, "runtime", &ereal);
        CHECK_STATUS(status);
        status = xiaGetRunData(0, "run_active", &acquiring);
        CHECK_STATUS(status);
        if (acquiring & XIA_RUN_HARDWARE) done=0;
        if (done) break;
    }
    if (acquiring == XIA_RUN_HANDEL) {
        status = xiaStopRun(0);
        CHECK_STATUS(status);
        status = xiaGetRunData(0, "mca", data);
        CHECK_STATUS(status);
    }
    xiaExit();
    return(0);
}
