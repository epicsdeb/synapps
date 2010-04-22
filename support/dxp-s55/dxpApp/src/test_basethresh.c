#include "stdlib.h"
#include "handel.h"
#include "handel_generic.h"
#include "unistd.h"

int main(int argc, char **argv)
{
    double runtasks=9339;
    double basethresh = 50.;
    unsigned short basethresh_readback=0;
    int i;

    printf("Initializing ...\n");
    xiaSetLogLevel(2);
    xiaInit("vortex.ini");
    xiaStartSystem();

    sleep(4);

    printf("Downloading parameters ...\n");
    xiaSetAcquisitionValues(0, "RUNTASKS", &runtasks);
    xiaSetAcquisitionValues(0, "BASETHRESH", &basethresh);

    for (i=0; i<5; i++) {
       xiaGetParameter(0, "BASETHRESH", &basethresh_readback);
       printf("Set BASETHRESH to %f, readback=%d\n", basethresh, basethresh_readback);
       sleep(1);
    }
    return(0);
}
