#include "stdlib.h"
#include "handel.h"
#include "handel_generic.h"

#define CHECK_STATUS(status) if (status) {printf("Error %d\n", status); exit(status);}

int main(int argc, char **argv)
{
    int status;
    unsigned int numModules = 0;
    unsigned int i;
    char **modules = NULL;

    printf("Initializing ...\n");
    status = xiaSetLogLevel(2);
    CHECK_STATUS(status);
    status = xiaInit("xmap16.ini");
    CHECK_STATUS(status);
    status = xiaStartSystem();
    CHECK_STATUS(status);

    status = xiaGetNumModules(&numModules);
    CHECK_STATUS(status);
    /* Allocate the memory we need for the string array */
    modules = (char **)calloc(numModules, sizeof(char *));
    if (modules == NULL) {
      printf("Error allocating memory for modules\n");
      return(-1);
    }
    for (i = 0; i < numModules; i++) {
        modules[i] = (char *)calloc(MAXALIAS_LEN, sizeof(char));
        if (modules[i] == NULL) {
            printf("Error allocating memory for modules[%d]\n", i);
            return(-1);
        }
    }
    printf("Before calling xiaGetModules, numModules=%d\n", numModules);
    for (i = 0; i < numModules; i++) {
        printf("modules[%u] address=%p, string=%s\n", i, modules[i], modules[i]);
    }
    status = xiaGetModules(modules);
    CHECK_STATUS(status);
    printf("\nAfter calling xiaGetModules\n");
    for (i = 0; i < numModules; i++) {
        printf("modules[%u] address=%p, string=%s\n", i, modules[i], modules[i]);
    }
    for (i = 0; i < numModules; i++) {
        free((void *)modules[i]);
    }
    free((void *)modules);
    modules = NULL;
    return(0);
}
