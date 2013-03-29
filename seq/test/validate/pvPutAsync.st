/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
program pvPutAsyncTest

%%#include "../testSupport.h"

foreign pvStatOK;

int x;
assign x to "pvPutAsync1";

entry {
    seq_test_init(10);
}

ss test1 {
    state put_async {
        int i;
        entry {
            i = 0;
        }
        when (i == 5) {
        } state wait_complete
        when (delay(0.4)) {
            int status;
            x = 1;
            status = pvPut(x,ASYNC);
            /* first ok, next three failure */
            testOk((i==0)==(status==pvStatOK), "pvPut/ASYNC %d: result=%d (%s)",
                i, status, status ? pvMessage(x) : "");
            ++i;
        } state put_async
    }
    state wait_complete {
        when (delay(5.0)) {
            testFail("pvPut completion timeout");
        } state put_sync
        when (pvPutComplete(x)) {
            testPass("pvPut/ASYNC complete");
        } state put_sync
    }
    state put_sync {
        entry {
            int i = 0;
            for (i=0; i<4; i++) {
                int status = pvPut(x,SYNC);
                /* should always succeed */
                testOk(status==pvStatOK, "pvPut/SYNC %d, status=%d (%s)",
                    i, status, status ? pvMessage(x) : "");
            }
        }
        when (delay(1)) {
        } exit
    }
}

#if 0
ss test2 {
    int x;
    assign x to "pvPutAsync2";
    state wait {
        when (delay(0.5)) {
        } state put_async
    }
    state put_async {
        when (pvPutComplete(x)) {
            int status;
            printf("test2: pvPut/ASYNC complete\n");
            x = 1;
            status = pvPut(x,ASYNC);
            /* should always succeed */
            if (status)
                printf("test2: pvPut/ASYNC 1 failed, %s\n", pvMessage(x));
            else
                printf("test2: pvPut/ASYNC 1 issued\n");
            /* should always fail */
            status = pvPut(x,ASYNC);
            if (status)
                printf("test2: pvPut/ASYNC 2 failed, %s\n", pvMessage(x));
            else
                printf("test2: pvPut/ASYNC 2 issued\n");
        } state wait
    }
}
#endif

exit {
    seq_test_done();
}
