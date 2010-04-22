#include "stdlib.h"
#include "handel.h"
#include "handel_generic.h"

int main(int argc, char **argv)
{
    unsigned short sca_length;
    char module_alias[MAXALIAS_LEN];
    char module_type[MAXITEM_LEN];
    int status;
    double num_scas;

    printf("Initializing ...\n");
    xiaSetLogLevel(2);
    xiaInit("xmap4.ini");
    xiaStartSystem();
    xiaSetLogLevel(4);

    /* Tell it there are 8 SCAS */
    num_scas = 8.;
    status = xiaSetAcquisitionValues(0, "number_of_scas", &num_scas);
    printf("status=%d, set number_of_scas=%f\n", status, num_scas);

    /* Read back number of SCAS */
    num_scas = 0.;
    status = xiaGetAcquisitionValues(0, "number_of_scas", &num_scas);
    printf("status=%d, read number_of_scas=%f\n", status, num_scas);

    /* Query for the size of the sca arrays */
    status = xiaGetRunData(0, "sca_length", &sca_length);
    printf("status=%d, sca_length=%d\n", status, sca_length);

    /* Get the board type */
    status = xiaGetModules_VB(0, module_alias);
    printf("status=%d, module_alias=%s\n", status, module_alias);
    status = xiaGetModuleItem(module_alias, "module_type", module_type);
    printf("status=%d, module_type=%s\n", status, module_type);
    return(0);
}
