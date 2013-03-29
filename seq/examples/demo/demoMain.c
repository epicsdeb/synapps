/*************************************************************************\
Copyright (c) 1990-1994 The Regents of the University of California
                        and the University of Chicago.
                        Los Alamos National Laboratory
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
/* Author:  Marty Kraimer Date:    17MAR2000 */
/*
 * Main program for demo sequencer
 */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "iocsh.h"

int main(int argc,char *argv[])
{
    if(argc>=2)
        iocsh(argv[1]);
    iocsh(NULL);
    return(0);
}
