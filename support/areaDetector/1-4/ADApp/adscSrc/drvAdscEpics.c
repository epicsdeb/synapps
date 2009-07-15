/* drvAdscEpics.c
 *
 * This is the EPICS dependent code for the driver for ADSC detectors (Q4,
 * Q4r, Q210, Q210r, Q270, Q315, Q315r).
 *
 * By making this separate file for the EPICS dependent code, the driver
 * itself only needs libCom from EPICS for OS-independence.
 *
 * Author: J. Lewis Muir (based on Prosilica driver by Mark Rivers)
 *         University of Chicago
 *
 * Created: April 11, 2008
 *
 */
 
#include <iocsh.h>
#include <drvSup.h>
#include <epicsExport.h>

#include "drvAdsc.h"

/* Code for iocsh registration */

/* adscConfig */
static const iocshArg adscConfigArg0  = {"Port name", iocshArgString};
static const iocshArg adscConfigArg1  = {"Model name", iocshArgString};
static const iocshArg * const adscConfigArgs[2] = {&adscConfigArg0,
                                                   &adscConfigArg1};
static const iocshFuncDef configadsc = {"adscConfig", 2, adscConfigArgs};
static void configadscCallFunc(const iocshArgBuf *args)
{
    adscConfig(args[0].sval, args[1].sval);
}

static void adscRegister(void)
{
    iocshRegister(&configadsc, configadscCallFunc);
}

epicsExportRegistrar(adscRegister);
