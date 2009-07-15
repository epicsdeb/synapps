/* drvDac128V.c

    Author: Mark Rivers

    28-June-2004  MLR  Converted from C++ to C

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <drvIpac.h>
#include <drvSup.h>
#include <ellLib.h>
#include <iocsh.h>
#include <asynDriver.h>
#include <epicsString.h>
#include <epicsExport.h>
#include <cantProceed.h>
 
#include "asynInt32.h"
#include "asynFloat64.h"

#define SYSTRAN_ID 0x45
#define SYSTRAN_DAC128V 0x69

static asynStatus writeInt32(void *drvPvt, asynUser *pasynUser, 
                             epicsInt32 value);
static asynStatus readInt32(void *drvPvt, asynUser *pasynUser, 
                            epicsInt32 *value);
static asynStatus getBounds(void *drvPvt, asynUser *pasynUser, 
                             epicsInt32 *low, epicsInt32 *high);
static asynStatus writeFloat64(void *drvPvt, asynUser *pasynUser, 
                               epicsFloat64 value);
static asynStatus readFloat64(void *drvPvt, asynUser *pasynUser, 
                              epicsFloat64 *value);
static void report(void *drvPvt, FILE *fp, int details);
static asynStatus connect(void *drvPvt, asynUser *pasynUser);
static asynStatus disconnect(void *drvPvt, asynUser *pasynUser);

/* asynCommon methods */
static const struct asynCommon dac128VCommon = {
    report,
    connect,
    disconnect
};

/* asynInt32 methods */
static asynInt32 drvDac128VInt32 = {
   writeInt32,
   readInt32,
   getBounds,
   NULL,
   NULL
};

/* asynFloat64 methods */
static asynFloat64 drvDac128VFloat64 = {
   writeFloat64,
   readFloat64,
   NULL,
   NULL
};

typedef struct {
    char *portName;
    int lastChan;
    int maxValue;
    volatile unsigned short* regs;
    asynInterface common;
    asynInterface int32;
    asynInterface float64;
} drvDac128VPvt;

int initDAC128V(const char *portName, ushort_t carrier, ushort_t slot)
{
    drvDac128VPvt *pPvt;
    asynStatus status;

    if (ipmValidate(carrier, slot, SYSTRAN_ID, SYSTRAN_DAC128V) != 0) {
       errlogPrintf("initDAC128V module not found in carried %d slot %d\n",
                    carrier, slot);
       return(-1);
    }
    pPvt = callocMustSucceed(1, sizeof(*pPvt), "initDAC128V");
    pPvt->portName = epicsStrDup(portName);
    pPvt->regs = (unsigned short *) ipmBaseAddr(carrier, slot, ipac_addrIO);

    /* lastChan and maxValue could be set by looking at "model" in the future
     * if models with more channels or more bits are available */
    pPvt->lastChan = 7;
    pPvt->maxValue = 4095;

    /* Link with higher level routines */
    pPvt->common.interfaceType = asynCommonType;
    pPvt->common.pinterface  = (void *)&dac128VCommon;
    pPvt->common.drvPvt = pPvt;
    pPvt->int32.interfaceType = asynInt32Type;
    pPvt->int32.pinterface  = (void *)&drvDac128VInt32;
    pPvt->int32.drvPvt = pPvt;
    pPvt->float64.interfaceType = asynFloat64Type;
    pPvt->float64.pinterface  = (void *)&drvDac128VFloat64;
    pPvt->float64.drvPvt = pPvt;
    status = pasynManager->registerPort(pPvt->portName,
                                   ASYN_MULTIDEVICE, /*is multiDevice*/
                                   1, /* autoConnect */
                                   0, /* medium priority */
                                   0); /* default stack size */
    if (status != asynSuccess) {
        errlogPrintf("initDAC128V ERROR: Can't register port\n");
        return -1;
    }
    status = pasynManager->registerInterface(pPvt->portName,&pPvt->common);
    if (status != asynSuccess) {
        errlogPrintf("initDAC128V ERROR: Can't register common.\n");
        return -1;
    }
    status = pasynInt32Base->initialize(pPvt->portName,&pPvt->int32);
    if (status != asynSuccess) {
        errlogPrintf("initDAC128V ERROR: Can't register int32.\n");
        return -1;
    }
    status = pasynFloat64Base->initialize(pPvt->portName,&pPvt->float64);
    if (status != asynSuccess) {
        errlogPrintf("initDAC128V ERROR: Can't register float64.\n");
        return -1;
    }
    return(0);
}

static asynStatus writeInt32(void *drvPvt, asynUser *pasynUser, 
                             epicsInt32 value)
{
    drvDac128VPvt *pPvt = (drvDac128VPvt *)drvPvt;
    int channel;

    pasynManager->getAddr(pasynUser, &channel);
    if(value<0 || value>pPvt->maxValue || channel<0 || channel>pPvt->lastChan)
        return(-1);
    pPvt->regs[channel] = value;
    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "drvDac128V, port %s, wrote %d to channel %d\n",
              pPvt->portName, value, channel);
    return(0);
}

static asynStatus getBounds(void *drvPvt, asynUser *pasynUser, 
                             epicsInt32 *low, epicsInt32 *high)
{
    *low = 0;
    *high = 4095;
    return(0);
}

static asynStatus writeFloat64(void *drvPvt, asynUser *pasynUser, 
                               epicsFloat64 value)
{
   return(writeInt32(drvPvt, pasynUser, (epicsInt32) value));
}

static asynStatus readInt32(void *drvPvt, asynUser *pasynUser, 
                            epicsInt32 *value)
{
    drvDac128VPvt *pPvt = (drvDac128VPvt *)drvPvt;
    int channel;

    pasynManager->getAddr(pasynUser, &channel);
    if(channel<0 || channel>pPvt->lastChan) return(-1);
    *value=pPvt->regs[channel];
    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "drvDac128V, port %s, read %d from channel %d\n",
              pPvt->portName, *value, channel);
    return(0);
}

static asynStatus readFloat64(void *drvPvt, asynUser *pasynUser, 
                              epicsFloat64 *value)
{
    epicsInt32 ivalue;
    int status;

    status = readInt32(drvPvt, pasynUser, &ivalue);
    *value = (epicsFloat64)ivalue;
    return(status);
}

/* asynCommon routines */

/* Report  parameters */
static void report(void *drvPvt, FILE *fp, int details)
{
    drvDac128VPvt *pPvt = (drvDac128VPvt *)drvPvt;

    fprintf(fp, "  Port: %s, address %p\n", pPvt->portName, pPvt->regs);
    if (details >= 1) {
        fprintf(fp, "  lastChan=%d, maxValue=%d\n", 
                pPvt->lastChan, pPvt->maxValue);
    }
}

/* Connect */
static asynStatus connect(void *drvPvt, asynUser *pasynUser)
{
    int channel;
    drvDac128VPvt *pPvt = (drvDac128VPvt *)drvPvt;

    pasynManager->getAddr(pasynUser, &channel);
    if (channel < -1 || channel > pPvt->lastChan) return(asynError);
    pasynManager->exceptionConnect(pasynUser);
    return(asynSuccess);
}

/* Disonnect */
static asynStatus disconnect(void *drvPvt, asynUser *pasynUser)
{
    drvDac128VPvt *pPvt = (drvDac128VPvt *)drvPvt;
    int channel;

    pasynManager->getAddr(pasynUser, &channel);
    if (channel < -1 || channel > pPvt->lastChan) return(asynError);
    pasynManager->exceptionDisconnect(pasynUser);
    return(asynSuccess);
}

static const iocshArg initArg0 = { "Port name",iocshArgString};
static const iocshArg initArg1 = { "Carrier",iocshArgInt};
static const iocshArg initArg2 = { "Slot",iocshArgInt};
static const iocshArg * const initArgs[3] = {&initArg0,
                                             &initArg1,
                                             &initArg2};
static const iocshFuncDef initFuncDef = {"initDAC128V",3,initArgs};
static void initCallFunc(const iocshArgBuf *args)
{
    initDAC128V(args[0].sval, args[1].ival, args[2].ival);
}

void drvDac128VRegister(void)
{
    iocshRegister(&initFuncDef,initCallFunc);
}

epicsExportRegistrar(drvDac128VRegister);
