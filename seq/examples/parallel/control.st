/*************************************************************************\
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
/* Demo KTL EPICS sequence; illustrates and tests parallel control */

program control/* ("pvsys=ktl") */

#define KTL_NAMES "dcs.axeinit", "dcs.chpinit", "dcs.domeinit",\
    "dcs.rotinit", "dcs.secinit", "dcs.tertinit"
#define CA_NAMES "dcs:axeinit", "dcs:chpinit", "dcs:domeinit",\
    "dcs:rotinit", "dcs:secinit", "dcs:tertinit"

/* options */
option  +r;

/* control variables */
#define N 6
int32_t init[N];
assign init to {CA_NAMES};

/* main state-set */
ss main {
    /* ordinary variables */
    int32_t done[N];

    state inactive {
        when () {
            int i;
            for (i = 0; i < N; i++) {
                init[i] = 1;
                pvPut(init[i], ASYNC);
            }
            printf("all init commands sent\n");
        } state active
    }

    state active {
        when (pvPutComplete(init, TRUE, done)) {
            int i;
            printf("init commands not all done:");
            for (i = 0; i < N; i++)
                printf(" %d", done[i]);
            printf("\n");
        } state active

        when (pvPutComplete(init)) {
            printf("init commands all done\n");
        } state done

        when (delay(5)) {
            printf("timeout waiting for inits to complete\n");
        } state done
    }

    state done {
        when () {
            unsigned short i;
            for (i = 0; i < N; i++) {
                printf("%s: done,stat,sevr,mess = %d,%d,%d,%s\n",
                    pvName(init[i]),
                    pvPutComplete(init[i]),
                    pvStatus(init[i]),
                    pvSeverity(init[i]),
                    pvMessage(init[i]) ? pvMessage(init[i]) : "");
            }
        } state wait
    }

    state wait {
        when (delay(5)) {
        } state inactive
    }
}
