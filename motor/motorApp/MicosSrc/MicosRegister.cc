/*
FILENAME... MicosRegister.cc
USAGE...    Register Micos MoCo dc motor controller device driver shell commands.

Version:	$Revision: 1.2 $
Modified By:	$Author: sluiter $
Last Modified:	$Date: 2004-07-16 19:22:58 $
*/

/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO

This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <iocsh.h>
#include "motor.h"
#include "drvMicos.h"
#include "epicsExport.h"

extern "C"
{

// Micos Setup arguments
static const iocshArg setupArg0 = {"Max. controller count", iocshArgInt};
static const iocshArg setupArg1 = {"Max. motor count", iocshArgInt};
static const iocshArg setupArg2 = {"Polling rate", iocshArgInt};
// Micos Config arguments
static const iocshArg configArg0 = {"Card being configured", iocshArgInt};
static const iocshArg configArg1 = {"asyn port name", iocshArgString};

static const iocshArg * const MicosSetupArgs[3]  = {&setupArg0, &setupArg1,
						     &setupArg2};
static const iocshArg * const MicosConfigArgs[2] = {&configArg0, &configArg1};

static const iocshFuncDef  setupMicos = {"MicosSetup",  3, MicosSetupArgs};
static const iocshFuncDef configMicos = {"MicosConfig", 2, MicosConfigArgs};

static void setupMicosCallFunc(const iocshArgBuf *args)
{
    MicosSetup(args[0].ival, args[1].ival, args[2].ival);
}


static void configMicosCallFunc(const iocshArgBuf *args)
{
    MicosConfig(args[0].ival, args[1].sval);
}


static void MicosmotorRegister(void)
{
    iocshRegister(&setupMicos, setupMicosCallFunc);
}

epicsExportRegistrar(MicosmotorRegister);

} // extern "C"
