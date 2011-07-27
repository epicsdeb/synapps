/* drvDac128V.cpp
 * Driver for Systran DAC-128V using asynPortDriver base class
*/

#include <asynPortDriver.h>
#include <drvIpac.h>
#include <iocsh.h>
#include <epicsExport.h>

#define SYSTRAN_ID 0x45
#define SYSTRAN_DAC128V 0x69
#define MAX_CHANNELS 8

static const char *driverName = "DAC128V";

/** This is the class definition for the DAC128V class
  */
class DAC128V : public asynPortDriver {
public:
    DAC128V(const char *portName, int carrier, int slot);

    /* These are the methods that we override from asynPortDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value);
    virtual asynStatus getBounds(asynUser *pasynUser, epicsInt32 *low, epicsInt32 *high);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual asynStatus readFloat64(asynUser *pasynUser, epicsFloat64 *value);
    virtual void report(FILE *fp, int details);

protected:
    int DAC128V_Data;       /**< (asynInt32, asynFloat64,    r/w) DAC output value in device units */
    #define FIRST_DAC_PARAM DAC128V_Data
    #define LAST_DAC_PARAM DAC128V_Data
    
private:
    int lastChan;
    int maxValue;
    volatile unsigned short* regs;    
};


#define DAC128VDataString  "DATA"

#define NUM_PARAMS (&LAST_DAC_PARAM - &FIRST_DAC_PARAM + 1)

DAC128V::DAC128V(const char *portName, int carrier, int slot)
    : asynPortDriver(portName, MAX_CHANNELS, NUM_PARAMS, 
          asynInt32Mask | asynFloat64Mask | asynDrvUserMask,
          asynInt32Mask | asynFloat64Mask, 
          ASYN_MULTIDEVICE, 1, /* ASYN_CANBLOCK=0, ASYN_MULTIDEVICE=1, autoConnect=1 */
          0, 0)  /* Default priority and stack size */
{
    static const char *functionName = "DAC128V";

    createParam(DAC128VDataString, asynParamInt32, &DAC128V_Data);
    
    if (ipmValidate(carrier, slot, SYSTRAN_ID, SYSTRAN_DAC128V) != 0) {
       asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: module not found in carrier %d slot %d\n",
            driverName, functionName, carrier, slot);
    } else {
        this->regs = (unsigned short *) ipmBaseAddr(carrier, slot, ipac_addrIO);
    }

    /* lastChan and maxValue could be set by looking at "model" in the future
     * if models with more channels or more bits are available */
    this->lastChan = 7;
    this->maxValue = 4095;
}

asynStatus DAC128V::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int channel;
    static const char *functionName = "writeInt32";

    this->getAddress(pasynUser, &channel);
    if(value<0 || value>this->maxValue || channel<0 || channel>this->lastChan)
        return(asynError);
    this->regs[channel] = value;
    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s, port %s, wrote %d to channel %d\n",
              driverName, functionName, this->portName, value, channel);
    return(asynSuccess);
}

asynStatus DAC128V::getBounds(asynUser *pasynUser, epicsInt32 *low, epicsInt32 *high)
{
    *low = 0;
    *high = this->maxValue;
    return(asynSuccess);
}

asynStatus DAC128V::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
   return(this->writeInt32(pasynUser, (epicsInt32) value));
}

asynStatus DAC128V::readInt32(asynUser *pasynUser, epicsInt32 *value)
{
    int channel;
    static const char *functionName = "readInt32";

    this->getAddress(pasynUser, &channel);
    if(channel<0 || channel>this->lastChan) return(asynError);
    *value=this->regs[channel];
    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s, port %s, read %d from channel %d\n",
              driverName, functionName, this->portName, *value, channel);
    return(asynSuccess);
}

asynStatus DAC128V::readFloat64(asynUser *pasynUser, epicsFloat64 *value)
{
    epicsInt32 ivalue;
    asynStatus status;

    status = this->readInt32(pasynUser, &ivalue);
    *value = (epicsFloat64)ivalue;
    return(status);
}

/* Report  parameters */
void DAC128V::report(FILE *fp, int details)
{
    asynPortDriver::report(fp, details);
    fprintf(fp, "  Port: %s, address %p\n", this->portName, this->regs);
    if (details >= 1) {
        fprintf(fp, "  lastChan=%d, maxValue=%d\n", 
                this->lastChan, this->maxValue);
    }
}

/** Configuration command, called directly or from iocsh */
extern "C" int initDAC128V(const char *portName, int carrier, int slot)
{
    DAC128V *pDAC128V = new DAC128V(portName, carrier, slot);
    pDAC128V = NULL;  /* This is just to avoid compiler warnings */
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

extern "C" {
epicsExportRegistrar(drvDac128VRegister);
}
