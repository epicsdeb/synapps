/* drvMARCCDEpics.c
 *
 * This is the EPICS dependent code for the MAR-CCD driver.
 * By making this separate file for the EPICS dependent code the driver itself
 * only needs libCom from EPICS for OS-independence.
 *
 * Author: Mark Rivers
 *         University of Chicago
 *
 * Created:  Nov. 2, 2008
 *
 */
 
#include <iocsh.h>
#include <drvSup.h>
#include <epicsExport.h>

#include "drvMARCCD.h"


/* Code for iocsh registration */

/* marCCDConfig */
static const iocshArg marCCDConfigArg0 = {"Port name", iocshArgString};
static const iocshArg marCCDConfigArg1 = {"server port name", iocshArgString};
static const iocshArg marCCDConfigArg2 = {"maxBuffers", iocshArgInt};
static const iocshArg marCCDConfigArg3 = {"maxMemory", iocshArgInt};
static const iocshArg * const marCCDConfigArgs[] =  {&marCCDConfigArg0,
                                                     &marCCDConfigArg1,
                                                     &marCCDConfigArg2,
                                                     &marCCDConfigArg3};
static const iocshFuncDef configMARCCD = {"marCCDConfig", 4, marCCDConfigArgs};
static void configMARCCDCallFunc(const iocshArgBuf *args)
{
    marCCDConfig(args[0].sval, args[1].sval, args[2].ival,  args[3].ival);
}


static void marCCD_ADRegister(void)
{

    iocshRegister(&configMARCCD, configMARCCDCallFunc);
}

epicsExportRegistrar(marCCD_ADRegister);


