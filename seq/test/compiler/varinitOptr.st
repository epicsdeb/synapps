/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
program varInitOptrTest

option +r;

int x = 1;
int xx = x;

ss test {

    int y = x;
    int yy = y;

    state test {

        int z = y;
        int zz = z;

        when () {} exit
    }
}
