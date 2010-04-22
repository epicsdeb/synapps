#include "stdlib.h"
#include "handel.h"
#include "handel_generic.h"
#include "unistd.h"
#include "time.h"

int main(int argc, char **argv)
{
    time_t t0, t1, t2;
    double info[2] = {1.0, 0.0};
    int nbase_history;
    long *baseline_history;

    printf("Initializing ...\n");
    xiaSetLogLevel(2);
    xiaInit("xmap4.ini");
    xiaStartSystem();

    xiaGetSpecialRunData(0, "baseline_history_length", &nbase_history);
    printf("Length of baseline history = %d\n", nbase_history);
    baseline_history = calloc(nbase_history, sizeof(long));

    t0 = time(NULL);
    xiaDoSpecialRun(0, "baseline_history", info);
    t1 = time(NULL);
    xiaGetSpecialRunData(0, "baseline_history", baseline_history);
    t2 = time(NULL);
    printf("time to do special run = %f\n", difftime(t1, t0));
    printf("time to read special run data = %f\n", difftime(t2, t1));

    return(0);
}
