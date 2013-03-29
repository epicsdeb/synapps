/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
program state_not_reachable

ss with_not_reachable_states {
    state init {
        when () {
        } state init
    }
    state not_reachable_1 {
        when () {
        } state not_reachable_2
    }
    state not_reachable_2 {
        when () {
        } state not_reachable_3
    }
    state not_reachable_3 {
        when () {
        } state not_reachable_1
    }
}
