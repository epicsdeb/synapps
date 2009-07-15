#include "stdlib.h"
#include "handel.h"
#include "handel_generic.h"

#define NUM_SCAS 16
#define NUM_CHANNELS 16
#define NUM_MCA_BINS 2048
#define NUM_LOOPS 5
static char *sca_lo[NUM_SCAS];
static char *sca_hi[NUM_SCAS];
#define SCA_NAME_LEN 10
#define CHECK_STATUS(status) if (status) {printf("Error %d\n", status); exit(status);}

static unsigned short param_data[4096];
static double sca_counts[NUM_SCAS * 2];

int main(int argc, char **argv)
{
    int i, j, loop;
    int ignore=0;
    double num_scas = NUM_SCAS;
    double dvalue;
    int status;

    printf("Initializing ...\n");
    xiaSetLogLevel(2);
    xiaInit("xmap16.ini");
    xiaStartSystem();
    for (i=0; i<NUM_SCAS; i++) {
        sca_lo[i] = calloc(1, SCA_NAME_LEN);
        sprintf(sca_lo[i], "sca%d_lo", i);
        sca_hi[i] = calloc(1, SCA_NAME_LEN);
        sprintf(sca_hi[i], "sca%d_hi", i);
    }
    for (loop=0; loop<NUM_LOOPS; loop++) {
        printf("Loop = %d/%d\n", loop+1, NUM_LOOPS);
        for (i=0; i<NUM_CHANNELS; i++) {
             xiaSetAcquisitionValues(i, "number_of_scas", &num_scas);
             for (j=0; j<NUM_SCAS; j++) {
                 dvalue = 100.;
                 status = xiaSetAcquisitionValues(i, sca_lo[j], &dvalue);
                 CHECK_STATUS(status);
                 status = xiaBoardOperation(i, "apply", &ignore);
                 CHECK_STATUS(status);
                 dvalue = 150.;
                 xiaSetAcquisitionValues(i, sca_hi[j], &dvalue);
                 CHECK_STATUS(status);
                 xiaBoardOperation(i, "apply", &ignore);
                 CHECK_STATUS(status);
            }

            xiaGetParamData(i, "values", param_data);
            xiaGetRunData(i, "input_count_rate", &dvalue);
            xiaGetRunData(i, "output_count_rate", &dvalue);
            xiaGetRunData(i, "events_in_run", &dvalue);
            xiaGetRunData(i, "run_active", &dvalue);
            xiaGetAcquisitionValues(i, "energy_threshold", &dvalue);
            xiaGetAcquisitionValues(i, "peaking_time", &dvalue);
            xiaGetAcquisitionValues(i, "gap_time", &dvalue);
            xiaGetAcquisitionValues(i, "trigger_threshold", &dvalue);
            xiaGetAcquisitionValues(i, "trigger_peaking_time", &dvalue);
            xiaGetAcquisitionValues(i, "trigger_gap_time", &dvalue);
            xiaGetAcquisitionValues(i, "preamp_gain", &dvalue);
            xiaGetAcquisitionValues(i, "baseline_average", &dvalue);
            xiaGetAcquisitionValues(i, "baseline_threshold", &dvalue);
            xiaGetAcquisitionValues(i, "maxwidth", &dvalue);
            xiaGetAcquisitionValues(i, "adc_percent_rule", &dvalue);
            xiaGetAcquisitionValues(i, "calibration_energy", &dvalue);
            xiaGetAcquisitionValues(i, "mca_bin_width", &dvalue);
            xiaGetAcquisitionValues(i, "number_mca_channels", &dvalue);
            /* Set the bin width in eV */
            dvalue = 20. * 1000. / NUM_MCA_BINS;
            xiaStopRun(i);
            xiaSetAcquisitionValues(i, "mca_bin_width", &dvalue);
            status = xiaBoardOperation(i, "apply", &ignore);
            CHECK_STATUS(status);
            xiaGetAcquisitionValues(i, "mca_bin_width", &dvalue);
            xiaGetAcquisitionValues(i, "number_of_scas", &dvalue);
            for (j=0; j<NUM_SCAS; j++) {
                xiaGetAcquisitionValues(i, sca_lo[j], &dvalue);
                xiaGetAcquisitionValues(i, sca_hi[j], &dvalue);
            }
            xiaGetRunData(i, "sca", sca_counts);
        }
    } 
    return(0);
}
