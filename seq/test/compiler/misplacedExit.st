/*************************************************************************\
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
program misplacedExitTest

ss x { 
    state y {
        when () {
        } state y
        exit {
        }
        when() {
        } state y
    }
}
