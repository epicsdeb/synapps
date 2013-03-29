/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
program arrayTest

%%#include "../testSupport.h"

entry {
    seq_test_init(20);
}

ss array {
    state init {
        string a4a5s[4][5] = {
            { "00", "01", "02", "03", "04" },
            { "10", "11", "12", "13", "14" },
            { "20", "21", "22", "23", "24" },
            { "30", "31", "32", "33", "34" }
        };
        string (*a4ps[4])[5] = {
            &a4a5s[0],
            &a4a5s[1],
            &a4a5s[2],
            &a4a5s[3]
        };
        assign a4a5s to {};
        when () {
            int i, j;
            for (i = 0; i < 4; i++) {
                for (j = 0; j < 5; j++) {
                    testOk(a4a5s[i][j] == (*a4ps[i])[j],
                        "a4a5s[%d][%d]=%s==%s=(*a4ps[%d])[%d])",
                        i, j, a4a5s[i][j], (*a4ps[i])[j], i, j);
                }
            }
            for (i = 0; i < 4; i++) {
                testDiag("a4ps[%d]=%p", i, a4ps[i]);
            }
        } exit
    }
}

exit {
    seq_test_done();
}
