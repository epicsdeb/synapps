/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
program subscriptTest

float single[2];
assign single;                  /* one single channel, count = 2 */

float multiple3[3];
assign multiple3 to {"x", "y"}; /* should be 3 channels, last one assigned to "" */

int multiple2[2];
assign multiple2 to {};         /* should be 2 channels, all assigned to "" */

monitor multiple2;              /* should all have monitored flag */

ss test {
    state test {
        when (FALSE) {
            %%multiple2[0] = 0;
        } state test
    }
}
