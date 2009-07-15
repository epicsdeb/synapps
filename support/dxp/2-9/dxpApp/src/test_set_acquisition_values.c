#include "stdlib.h"
#include "handel.h"
#include "handel_generic.h"
#include "unistd.h"

#define NUM_CHANNELS 16
#define NUM_PARAMETERS 11
typedef struct {
   char *name;
   double value;
} dxp_parameter;

static dxp_parameter dxp_parameters[NUM_PARAMETERS] = {
   {"baseline_threshold", 1.0},
   {"energy_threshold", 0.0},
   {"peaking_time", 4.0},
   {"gap_time", 0.5},
   {"trigger_threshold", 1.0},
   {"trigger_peaking_time", 0.15},
   {"calibration_energy", 10000.0},
   {"adc_percent_rule", 5.0},
   {"baseline_average", 128.},
   {"mca_bin_width", 10.0},
   {"preamp_gain", 1.7}
};
  

int main(int argc, char **argv)
{
    int i, j;
    int ignore=0;
    int status;
    unsigned short runtype, errinfo;

    printf("Initializing ...\n");
    xiaSetLogLevel(2);
    xiaInit("xmap16.ini");
    xiaStartSystem();

    for (i=0; i<NUM_CHANNELS; i++) {
        for (j=0; j<NUM_PARAMETERS; j++) {
            xiaSetAcquisitionValues(i, dxp_parameters[j].name, &dxp_parameters[j].value);
            status = xiaBoardOperation(i, "apply", &ignore);
            xiaGetParameter(i, "RUNTYPE", &runtype);
            xiaGetParameter(i, "ERRINFO", &errinfo);
            printf("Channel %d setting %s = %f, RUNTYPE=%d, ERRINFO=%d\n", 
                    i, dxp_parameters[j].name, dxp_parameters[j].value, runtype, errinfo) ;
        }
    }

    return(0);
}
