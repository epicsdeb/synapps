/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
program pvGetSyncTest

%%#include "../testSupport.h"

%%epicsShareExtern double seq_sync_timeout;

foreign seq_sync_timeout;

entry {
    seq_test_init(2);
}

ss sstest {
    double x = 0;
    assign x to "pvGetSync";
    int p = 1;
    assign p to "pvGetSync.PROC";

    state stest1 {
        when () {
            int status;
            seq_sync_timeout = 0.1;
            pvPut(p, ASYNC);
            testDiag("x=%f",x);
            status = pvGet(x,SYNC);
            testOk(status==pvStatTIMEOUT, "pvGet/SYNC, status=%d (%s)",
                status, status ? pvMessage(x) : "");
            testDiag("x=%f",x);
        } state stest2
    }
    state stest2 {
        when (pvPutComplete(p)) {
            int status;
            pvPut(p, ASYNC);
            seq_sync_timeout = 10.0;
            testDiag("x=%f",x);
            status = pvGet(x,SYNC);
            testOk(status==pvStatOK, "pvGet/SYNC, status=%d (%s)",
                status, status ? pvMessage(x) : "");
            testDiag("x=%f",x);
        } exit
    }
}

exit {
    seq_test_done();
}
