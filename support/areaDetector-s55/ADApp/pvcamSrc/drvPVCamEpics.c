/* drvPVCamEpics.c
 *
 * This is the EPICS dependent code for the driver for a PVCam (PI/Acton) detector.
 * By making this separate file for the EPICS dependent code the driver itself
 * only needs libCom from EPICS for OS-independence.
 *
 * Author: Brian Tieman
 *
 * Created:  06/14/2009
 *
 */

#include <iocsh.h>
#include <drvSup.h>
#include <epicsExport.h>

#include "drvPVCam.h"


/* Code for iocsh registration */

/* pvCamConfig */
static const iocshArg pvCamConfigArg0 = {"Port name", iocshArgString};
static const iocshArg pvCamConfigArg1 = {"Max X size", iocshArgInt};
static const iocshArg pvCamConfigArg2 = {"Max Y size", iocshArgInt};
static const iocshArg pvCamConfigArg3 = {"Data type", iocshArgInt};
static const iocshArg pvCamConfigArg4 = {"maxBuffers", iocshArgInt};
static const iocshArg pvCamConfigArg5 = {"maxMemory", iocshArgInt};
static const iocshArg pvCamConfigArg6 = {"priority", iocshArgInt};
static const iocshArg pvCamConfigArg7 = {"stackSize", iocshArgInt};
static const iocshArg * const pvCamConfigArgs[] =  {&pvCamConfigArg0,
                                                          &pvCamConfigArg1,
                                                          &pvCamConfigArg2,
                                                          &pvCamConfigArg3,
                                                          &pvCamConfigArg4,
                                                          &pvCamConfigArg5,
                                                          &pvCamConfigArg6,
                                                          &pvCamConfigArg7};
static const iocshFuncDef configPVCam = {"pvCamConfig", 8, pvCamConfigArgs};
static void configPVCamCallFunc(const iocshArgBuf *args)
{
    pvCamConfig(args[0].sval, args[1].ival, args[2].ival, args[3].ival,
                      args[4].ival, args[5].ival, args[6].ival, args[7].ival);
}


static void pvCamRegister(void)
{
    iocshRegister(&configPVCam, configPVCamCallFunc);
}

epicsExportRegistrar(pvCamRegister);


