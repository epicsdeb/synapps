#include <epicsMutex.h>
#include <iocsh.h>
#include <epicsExport.h>
#include "epicsHandelUtils.h"
#include "drvDxp.h"
#include "handel.h"
#include "handel_generic.h"

epicsMutexId epicsHandelMutexId = 0;

int epicsHandelLock()
{
    if (epicsHandelMutexId == 0) {
        epicsHandelMutexId = epicsMutexMustCreate();
    }
    epicsMutexMustLock(epicsHandelMutexId);
    return(0);
}

int epicsHandelUnlock()
{
    epicsMutexUnlock(epicsHandelMutexId);
    return(0);
}

int dxpGetModuleType()
{
    /* This function returns an enum of type DXP_MODULE_TYPE for the module type.
     * It returns the type of the first module in the system.  
     * IMPORTANT ASSUMPTION: It is assumed that a single EPICS IOC will only
     * be controlling a single type of XIA module (xMAP, Saturn, DXP2X)
     * If there is an error it returns -1.
     */
    char module_alias[MAXALIAS_LEN];
    char module_type[MAXITEM_LEN];
    int status;

    /* Get the module alias for the first detChan */
    status = xiaGetModules_VB(0, module_alias);
    /* Get the module type for this module */
    status = xiaGetModuleItem(module_alias, "module_type", module_type);
    /* Look for known module types */
    if (strcmp(module_type, "xmap") == 0) return(DXP_XMAP);
    if (strcmp(module_type, "dxpx10p") == 0) return(DXP_SATURN);
    if (strcmp(module_type, "dxp4c2x") == 0) return(DXP_4C2X);
    return(-1);
}



/* iocsh functions */
static const iocshArg DXPConfigArg0 = { "server name",iocshArgString};
static const iocshArg DXPConfigArg1 = { "number of detectors",iocshArgInt};
static const iocshArg DXPConfigArg2 = { "number of detector groups",iocshArgInt};
static const iocshArg DXPConfigArg3 = { "poll frequency",iocshArgInt};
static const iocshArg * const DXPConfigArgs[4] = {&DXPConfigArg0,
                                                  &DXPConfigArg1,
                                                  &DXPConfigArg2,
                                                  &DXPConfigArg3};
static const iocshFuncDef DXPConfigFuncDef = {"DXPConfig",4,DXPConfigArgs};
static void DXPConfigCallFunc(const iocshArgBuf *args)
{
    DXPConfig(args[0].sval, args[1].ival, args[2].ival, args[3].ival);
}

static const iocshArg xiaLogLevelArg0 = { "logging level",iocshArgInt};
static const iocshArg * const xiaLogLevelArgs[1] = {&xiaLogLevelArg0};
static const iocshFuncDef xiaLogLevelFuncDef = {"xiaSetLogLevel",1,xiaLogLevelArgs};
static void xiaLogLevelCallFunc(const iocshArgBuf *args)
{
    epicsHandelLock();
    xiaSetLogLevel(args[0].ival);
    epicsHandelUnlock();
}

static const iocshArg xiaLogOutputArg0 = { "logging level",iocshArgString};
static const iocshArg * const xiaLogOutputArgs[1] = {&xiaLogOutputArg0};
static const iocshFuncDef xiaLogOutputFuncDef = {"xiaSetLogOutput",1,xiaLogOutputArgs};
static void xiaLogOutputCallFunc(const iocshArgBuf *args)
{
    epicsHandelLock();
    xiaSetLogOutput(args[0].sval);
    epicsHandelUnlock();
}

static const iocshArg xiaInitArg0 = { "ini file",iocshArgString};
static const iocshArg * const xiaInitArgs[1] = {&xiaInitArg0};
static const iocshFuncDef xiaInitFuncDef = {"xiaInit",1,xiaInitArgs};
static void xiaInitCallFunc(const iocshArgBuf *args)
{
    epicsHandelLock();
    xiaInit(args[0].sval);
    epicsHandelUnlock();
}

static const iocshFuncDef xiaStartSystemFuncDef = {"xiaStartSystem",0,0};
static void xiaStartSystemCallFunc(const iocshArgBuf *args)
{
    epicsHandelLock();
    xiaStartSystem();
    epicsHandelUnlock();
}

static const iocshArg xiaSaveSystemArg0 = { "ini file",iocshArgString};
static const iocshArg * const xiaSaveSystemArgs[1] = {&xiaSaveSystemArg0};
static const iocshFuncDef xiaSaveSystemFuncDef = {"xiaSaveSystem",1,xiaSaveSystemArgs};
static void xiaSaveSystemCallFunc(const iocshArgBuf *args)
{
    epicsHandelLock();
    xiaSaveSystem("handel_ini", args[0].sval);
    epicsHandelUnlock();
}

void dxpRegister(void)
{
    iocshRegister(&xiaInitFuncDef,xiaInitCallFunc);
    iocshRegister(&xiaLogLevelFuncDef,xiaLogLevelCallFunc);
    iocshRegister(&xiaLogOutputFuncDef,xiaLogOutputCallFunc);
    iocshRegister(&xiaStartSystemFuncDef,xiaStartSystemCallFunc);
    iocshRegister(&xiaSaveSystemFuncDef,xiaSaveSystemCallFunc);
    iocshRegister(&DXPConfigFuncDef,DXPConfigCallFunc);
}

epicsExportRegistrar(dxpRegister);


