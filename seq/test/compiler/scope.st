/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
program scope

int i, j;
evflag f, g, h;

ss start {
    assign i;
    monitor i;
    sync i to f;
    syncq i to f;
    int k;
    state first {
        assign j;
        monitor j;
        sync j to g;
        syncq k to h;
        when () {
        } exit
    }
}
