/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
program stopTest

ss main {
    state start {
        when (delay(5)) {
            seqStop(epicsThreadGetIdSelf());
        } state start
    }
}

ss progress {
    state running {
        when (delay(1)) {
            printf("still running...\n");
        } state running
    }
}
