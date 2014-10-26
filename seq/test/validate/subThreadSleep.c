/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
#include "epicsThread.h"
#include "subRecord.h"
#include "registryFunction.h"
#include "epicsExport.h"

long subThreadSleep(struct subRecord *psub)
{
    epicsThreadSleep(1);
    psub->val += 1.0;
    return 0;
}
epicsRegisterFunction(subThreadSleep);
