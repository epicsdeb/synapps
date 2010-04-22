/* This is the EPICS dependent code for the XIA NDDxp driver module.
 * By making this separate file for the EPICS dependent code the driver itself
 * only needs libCom from EPICS for OS-independence (i.e. it is then possible to create
 * module tests withouth needing to load the entire EPICS libraries!)
 */

#include <iocsh.h>
#include <drvSup.h>
#include <epicsExport.h>

#include <handel.h>

#include "NDDxp.h"

static const iocshArg NDDxpConfigArg0 = {"Asyn port name", iocshArgString};
static const iocshArg NDDxpConfigArg1 = {"Number of channels", iocshArgInt};
static const iocshArg NDDxpConfigArg2 = {"Maximum number of buffers", iocshArgInt};
static const iocshArg NDDxpConfigArg3 = {"Maximum amount of memory (bytes)", iocshArgInt};
static const iocshArg * const NDDxpConfigArgs[] =  {&NDDxpConfigArg0,
                                                    &NDDxpConfigArg1,
                                                    &NDDxpConfigArg2,
                                                    &NDDxpConfigArg3};
static const iocshFuncDef configNDDxp = {"NDDxpConfig", 4, NDDxpConfigArgs};
static void configNDDxpCallFunc(const iocshArgBuf *args)
{
    /*NDDxp_config(const char *portName, int nCards, const char *iniFile, int maxBuffers, size_t maxMemory);*/
    NDDxp_config(args[0].sval, args[1].ival, args[2].ival, args[3].ival);
}

static const iocshArg xiaLogLevelArg0 = { "logging level",iocshArgInt};
static const iocshArg * const xiaLogLevelArgs[1] = {&xiaLogLevelArg0};
static const iocshFuncDef xiaLogLevelFuncDef = {"xiaSetLogLevel",1,xiaLogLevelArgs};
static void xiaLogLevelCallFunc(const iocshArgBuf *args)
{
    /* epicsHandelLock(); */
    xiaSetLogLevel(args[0].ival);
   /*  epicsHandelLock(); */
}

static const iocshArg xiaLogOutputArg0 = { "logging output file",iocshArgString};
static const iocshArg * const xiaLogOutputArgs[1] = {&xiaLogOutputArg0};
static const iocshFuncDef xiaLogOutputFuncDef = {"xiaSetLogOutput",1,xiaLogOutputArgs};
static void xiaLogOutputCallFunc(const iocshArgBuf *args)
{
    /* epicsHandelLock(); */
    xiaSetLogOutput(args[0].sval);
    /* epicsHandelLock(); */
}

static const iocshArg xiaInitArg0 = { "ini file",iocshArgString};
static const iocshArg * const xiaInitArgs[1] = {&xiaInitArg0};
static const iocshFuncDef xiaInitFuncDef = {"xiaInit",1,xiaInitArgs};
static void xiaInitCallFunc(const iocshArgBuf *args)
{
    /* epicsHandelLock(); */
    xiaInit(args[0].sval);
    /* epicsHandelUnlock(); */
}

static const iocshFuncDef xiaStartSystemFuncDef = {"xiaStartSystem",0,0};
static void xiaStartSystemCallFunc(const iocshArgBuf *args)
{
    /* epicsHandelLock(); */
    xiaStartSystem();
    /* epicsHandelUnlock(); */
}

static const iocshArg xiaSaveSystemArg0 = { "ini file",iocshArgString};
static const iocshArg * const xiaSaveSystemArgs[1] = {&xiaSaveSystemArg0};
static const iocshFuncDef xiaSaveSystemFuncDef = {"xiaSaveSystem",1,xiaSaveSystemArgs};
static void xiaSaveSystemCallFunc(const iocshArgBuf *args)
{
    /* epicsHandelLock(); */
    xiaSaveSystem("handel_ini", args[0].sval);
    /* epicsHandelUnlock(); */
}



static void NDDxpRegister(void)
{
    iocshRegister(&configNDDxp, configNDDxpCallFunc);
    iocshRegister(&xiaInitFuncDef,xiaInitCallFunc);
    iocshRegister(&xiaLogLevelFuncDef,xiaLogLevelCallFunc);
    iocshRegister(&xiaLogOutputFuncDef,xiaLogOutputCallFunc);
    iocshRegister(&xiaStartSystemFuncDef,xiaStartSystemCallFunc);
    iocshRegister(&xiaSaveSystemFuncDef,xiaSaveSystemCallFunc);
}

epicsExportRegistrar(NDDxpRegister);
