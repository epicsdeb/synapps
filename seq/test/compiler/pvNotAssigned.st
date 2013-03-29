/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
program pvNotAssigned

int a;

entry {
    pvGet(a);
    pvPut(a);
    pvAssign(a, "");
    pvMonitor(a);
}
ss x {
    state y {
        entry {
            pvAssign(a, "");
            pvMonitor(a);
            pvGet(a);
            pvPut(a);
        }
        when () {
            pvAssign(a, "");
            pvMonitor(a);
            pvGet(a);
            pvPut(a);
        } exit
        exit {
            pvAssign(a, "");
            pvMonitor(a);
            pvGet(a);
            pvPut(a);
        }
    }
}
exit {
    pvAssign(a, "");
    pvMonitor(a);
    pvGet(a);
    pvPut(a);
}
