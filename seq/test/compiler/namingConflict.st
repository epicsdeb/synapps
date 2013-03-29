/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
program namingConflictTest

ss x {
    state y_z {
        when () {
        } state y_z
    }
}

ss x_y {
    state z {
        when () {
        } state z
    }
}
