/* drvPilatusDetectorEpics.c
 *
 * This is the EPICS dependent code for the Pilatus driver.
 * By making this separate file for the EPICS dependent code the driver itself
 * only needs libCom from EPICS for OS-independence.
 *
 * Author: Mark Rivers
 *         University of Chicago
 *
 * Created:  June 10, 2008
 *
 */
 
#include <iocsh.h>
#include <drvSup.h>
#include <epicsExport.h>

#include "drvPilatusDetector.h"


/* Code for iocsh registration */

/* pilatusDetectorConfig */
static const iocshArg pilatusDetectorConfigArg0 = {"Port name", iocshArgString};
static const iocshArg pilatusDetectorConfigArg1 = {"camserver port name", iocshArgString};
static const iocshArg pilatusDetectorConfigArg2 = {"maxSizeX", iocshArgInt};
static const iocshArg pilatusDetectorConfigArg3 = {"maxSizeY", iocshArgInt};
static const iocshArg pilatusDetectorConfigArg4 = {"maxBuffers", iocshArgInt};
static const iocshArg pilatusDetectorConfigArg5 = {"maxMemory", iocshArgInt};
static const iocshArg * const pilatusDetectorConfigArgs[] =  {&pilatusDetectorConfigArg0,
                                                              &pilatusDetectorConfigArg1,
                                                              &pilatusDetectorConfigArg2,
                                                              &pilatusDetectorConfigArg3,
                                                              &pilatusDetectorConfigArg4,
                                                              &pilatusDetectorConfigArg5};
static const iocshFuncDef configPilatusDetector = {"pilatusDetectorConfig", 6, pilatusDetectorConfigArgs};
static void configPilatusDetectorCallFunc(const iocshArgBuf *args)
{
    pilatusDetectorConfig(args[0].sval, args[1].sval, args[2].ival,  args[3].ival,  
                          args[4].ival, args[5].ival);
}


static void pilatusDetectorRegister(void)
{

    iocshRegister(&configPilatusDetector, configPilatusDetectorCallFunc);
}

epicsExportRegistrar(pilatusDetectorRegister);


