/* drvIpUnidig.cc

    Author: Mark Rivers

    This is the driver for the Greenspring IP-Unidig series of digital I/O IP
    modules.  It also supports the Systran DIO316I module.

    Modifications:
    1-Apr-2003  MLR  Added support for interrupts on the IP-Unidig-I series.
                     Added additional arguments to IpUnidig::init and
                     IpUnidig::IpUnidig to support this.
    23-Apr-2003 MLR  Added functions for changing the rising and falling masks
    27-May-2003 MLR  Converted to EPICS R3.14.
    29-Jun-2004 MLR  Converted from MPF to asyn, and from C++ to C
    28-Jul-2004 MLR  Converted to generic asynUInt32Digital interfaces
*/

/* System includes */
#include <string.h> 

/* EPICS includes */
#include <drvIpac.h>
#include <errlog.h>
#include <ellLib.h>
#include <devLib.h>
#include <cantProceed.h> 
#include <epicsTypes.h>
#include <epicsThread.h> 
#include <epicsMutex.h> 
#include <epicsString.h> 
#include <epicsExit.h>
#include <epicsMessageQueue.h>
#include <epicsExport.h>
#include <iocsh.h>

#include <asynPortDriver.h>


#define GREENSPRING_ID 0xF0
#define SYSTRAN_ID     0x45
#define SBS_ID         0xB3

#define UNIDIG_E          0x51 /*IP-Unidig-E          (24 I/O, LineSafe) */
#define UNIDIG            0x61 /*IP-Unidig            (24 I/O) */
#define UNIDIG_D          0x62 /*IP-Unidig-D          (24 I/O, differential) */
#define UNIDIG_O_24IO     0x63 /*IP-Unidig-O-24IO     (24 I/O, optically iso.) */
#define UNIDIG_HV_16I8O   0x64 /*IP-Unidig-HV-16I8O   (16I, 8O, high voltage) */
#define UNIDIG_E48        0x65 /*IP-Unidig-E48        (48 I/O, LineSafe) */
#define UNIDIG_I_O_24I    0x66 /*IP-Unidig-I-O-24I    (24 I, optical,  interrupts) */
#define UNIDIG_I_E        0x67 /*IP-Unidig-I-E        (24 I/O, LineSafe, interrupts) */
#define UNIDIG_I          0x68 /*IP-Unidig-I          (24 I/O, interrupts) */
#define UNIDIG_I_D        0x69 /*IP-Unidig-I-D        (24 I/O, differential, interrupts) */
#define UNIDIG_I_O_24IO   0x6A /*IP-Unidig-I-O-24IO   (24 I/O, optical,  interrupts) */
#define UNIDIG_I_HV_16I8O 0x6B /*IP-Unidig-I-HV-16I8O (16I, 8O, high voltage, ints.) */
#define UNIDIG_T          0x6D /*IP-Unidig-T          (Timer) */
#define UNIDIG_T_D        0x6E /*IP-Unidig-T-D        (Timer, differential) */
#define UNIDIG_O_12I12O   0x6F /*IP-Unidig-O-12I12O   (12I, 12O, optical) */
#define UNIDIG_I_O_12I12O 0x70 /*IP-Unidig-I-O-12I12O (12I, 12O, optical, interrupts) */
#define UNIDIG_P          0x71 /*IP-Unidig-P          (16I, 16O) */
#define UNIDIG_P_D        0x72 /*IP-Unidig-P-D        (16I, 16O, differential) */
#define UNIDIG_O_24I      0x73 /*IP-Unidig-O-24I      (24I. opt. iso.) */
#define UNIDIG_HV_8I16O   0x74 /*IP-Unidig-HV-8I16O   (8I, 16O, high voltage) */
#define UNIDIG_I_HV_8I16O 0x75 /*IP-Unidig-I-HV-8I16O (8I, 16O, high voltage, ints.) */

#define SYSTRAN_DIO316I   0x63

#define SBS_IPOPTOIO8     0x02

#define MAX_MESSAGES 1000

typedef struct {
    volatile epicsUInt16 *outputRegisterLow;
    volatile epicsUInt16 *outputRegisterHigh;
    volatile epicsUInt16 *outputEnableLow;
    volatile epicsUInt16 *outputEnableHigh;
    volatile epicsUInt16 *inputRegisterLow;
    volatile epicsUInt16 *inputRegisterHigh;
    volatile epicsUInt16 *controlRegister0;
    volatile epicsUInt16 *controlRegister1;
    volatile epicsUInt16 *intVecRegister;
    volatile epicsUInt16 *intEnableRegisterLow;
    volatile epicsUInt16 *intEnableRegisterHigh;
    volatile epicsUInt16 *intPolarityRegisterLow;
    volatile epicsUInt16 *intPolarityRegisterHigh;
    volatile epicsUInt16 *intClearRegisterLow;
    volatile epicsUInt16 *intClearRegisterHigh;
    volatile epicsUInt16 *intPendingRegisterLow;
    volatile epicsUInt16 *intPendingRegisterHigh;
    volatile epicsUInt16 *DACRegister;
} ipUnidigRegisters;

typedef struct {
    epicsUInt32 bits;
    epicsUInt32 interruptMask;
} ipUnidigMessage;


static const char *driverName = "IpUnidig";


/** This is the class definition for the IpUnidig class*/
class IpUnidig : public asynPortDriver
{
public:
    IpUnidig(const char *portName, int carrier, int slot, int msecPoll, int intVec, int risingMask, int fallingMask);
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value);
    virtual asynStatus getBounds(asynUser *pasynUser, epicsInt32 *low, epicsInt32 *high);
    virtual asynStatus readUInt32Digital(asynUser *pasynUser, epicsUInt32 *value, epicsUInt32 mask);
    virtual asynStatus writeUInt32Digital(asynUser *pasynUser, epicsUInt32 value, epicsUInt32 mask);
    virtual asynStatus setInterruptUInt32Digital(asynUser *pasynUser, epicsUInt32 mask, interruptReason reason);
    virtual asynStatus clearInterruptUInt32Digital(asynUser *pasynUser, epicsUInt32 mask);
    virtual asynStatus getInterruptUInt32Digital(asynUser *pasynUser, epicsUInt32 *mask, interruptReason reason);
    virtual void report(FILE *fp, int details);
    // These should be private, but are called from C, so must be public
    void pollerThread();  
    void intFunc();
    void rebootCallback();

private:
    unsigned char manufacturer;
    unsigned char model;
    epicsUInt16 *baseAddress;
    int supportsInterrupts;
    int rebooting;
    epicsUInt32 risingMask;
    epicsUInt32 fallingMask;
    epicsUInt32 polarityMask;
    epicsUInt32 oldBits;
    ipUnidigRegisters regs;
    int forceCallback;
    double pollTime;
    epicsMessageQueueId msgQId;
    int messagesSent;
    int messagesFailed;
    int dataParam;
    
    void writeIntEnableRegs();
};

// These functions must have C linkage because they are called from other EPICS components
extern "C" {
static void rebootCallbackC(void * pPvt)
{
    IpUnidig *pIpUnidig = (IpUnidig *)pPvt;
    pIpUnidig->rebootCallback();
}

static void pollerThreadC(void * pPvt)
{
    IpUnidig *pIpUnidig = (IpUnidig *)pPvt;
    pIpUnidig->pollerThread();
}

static void intFuncC(void * pPvt)
{
    IpUnidig *pIpUnidig = (IpUnidig *)pPvt;
    pIpUnidig->intFunc();
}
}

IpUnidig::IpUnidig(const char *portName, int carrier, int slot, int msecPoll, int intVec, int risingMask, int fallingMask)
    :asynPortDriver(portName,1,1,
                    asynInt32Mask | asynUInt32DigitalMask | asynDrvUserMask,
                    asynUInt32DigitalMask,
                    0,1,0,0)
{
    //static const char *functionName = "IpUnidig";
    ipac_idProm_t *id;
    epicsUInt16 *base;
   
    /* Default of 100 msec for backwards compatibility with old version */
    if (msecPoll == 0) msecPoll = 100;
    this->pollTime = msecPoll / 1000.;
    this->messagesSent = 0;
    this->messagesFailed = 0;
    this->rebooting = 0;
    this->forceCallback = 0;
    this->oldBits = 0;
    this->msgQId = epicsMessageQueueCreate(MAX_MESSAGES, sizeof(ipUnidigMessage));

    if (ipmCheck(carrier, slot)) {
       errlogPrintf("IpUnidig: bad carrier or slot\n");
    }
    id = (ipac_idProm_t *) ipmBaseAddr(carrier, slot, ipac_addrID);
    base = (epicsUInt16 *) ipmBaseAddr(carrier, slot, ipac_addrIO);
    this->baseAddress = base;

    manufacturer = id->manufacturerId & 0xff;
    model = id->modelId & 0xff;
    switch (manufacturer) {
    case GREENSPRING_ID:
       switch (model) {
          case UNIDIG_E:
          case UNIDIG:
          case UNIDIG_D:
          case UNIDIG_O_24IO:
          case UNIDIG_HV_16I8O:
          case UNIDIG_E48:
          case UNIDIG_I_O_24I:
          case UNIDIG_I_E:
          case UNIDIG_I:
          case UNIDIG_I_D:
          case UNIDIG_I_O_24IO:
          case UNIDIG_I_HV_16I8O:
          case UNIDIG_O_12I12O:
          case UNIDIG_I_O_12I12O:
          case UNIDIG_O_24I:
          case UNIDIG_HV_8I16O:
          case UNIDIG_I_HV_8I16O:
             break;
          default:
             errlogPrintf("IpUnidig model 0x%x not supported\n",model);
             break;
       }
       break;

    case SYSTRAN_ID:
       if(model != SYSTRAN_DIO316I) {
          errlogPrintf("IpUnidig model 0x%x not Systran DIO316I\n",model);
       }
       break;
       
    case SBS_ID:
       if(model != SBS_IPOPTOIO8) {
          errlogPrintf("IpUnidig model 0x%x not SBS IP-OPTOIO-8\n",model);
       }
       break;
       
    default: 
       errlogPrintf("IpUnidig manufacturer 0x%x not supported\n", manufacturer);
       break;
    }

    this->manufacturer = manufacturer;
    this->model = model;

    /* Set up the register pointers.  Set the defaults for most modules */
    /* Define registers in units of 16-bit words */
    this->regs.outputRegisterLow        = base;
    this->regs.outputRegisterHigh       = base + 0x1;
    this->regs.inputRegisterLow         = base + 0x2;
    this->regs.inputRegisterHigh        = base + 0x3;
    this->regs.outputEnableLow          = base + 0x8;
    this->regs.outputEnableHigh         = base + 0x5;
    this->regs.controlRegister0         = base + 0x6;
    this->regs.intVecRegister           = base + 0x8;
    this->regs.intEnableRegisterLow     = base + 0x9;
    this->regs.intEnableRegisterHigh    = base + 0xa;
    this->regs.intPolarityRegisterLow   = base + 0xb;
    this->regs.intPolarityRegisterHigh  = base + 0xc;
    this->regs.intClearRegisterLow      = base + 0xd;
    this->regs.intClearRegisterHigh     = base + 0xe;
    this->regs.intPendingRegisterLow    = base + 0xd;
    this->regs.intPendingRegisterHigh   = base + 0xe;
    this->regs.DACRegister              = base + 0xe;
    
    /* Set things up for specific models which need to be treated differently */
    switch (manufacturer) {
    case GREENSPRING_ID: 
       switch (model) {
       case UNIDIG_O_24IO:
       case UNIDIG_O_12I12O:
       case UNIDIG_I_O_24IO:
       case UNIDIG_I_O_12I12O:
          /* Enable outputs */
          *this->regs.controlRegister0 |= 0x4;
          break;
       case UNIDIG_HV_16I8O:
       case UNIDIG_I_HV_16I8O:
          /*  These modules don't allow access to outputRegisterLow */
          this->regs.outputRegisterLow = NULL;
          /* Set the comparator DAC for 2.5 volts.  Each bit is 15 mV. */
          *this->regs.DACRegister = 2500/15;
          break;
       }
       break;
    case SYSTRAN_ID:
       switch (model) {
       case SYSTRAN_DIO316I:
          /* Different register layout */
          this->regs.outputRegisterLow  = base;
          this->regs.outputRegisterHigh = base + 0x1;
          this->regs.inputRegisterLow   = base + 0x2;
          this->regs.inputRegisterHigh  = NULL;
          this->regs.controlRegister0   = base + 0x3;
          this->regs.controlRegister1   = base + 0x4;
          /* Enable outputs for ports 0-3 */
          *this->regs.controlRegister0  |= 0xf;
          /* Set direction of ports 0-1 to be output */
          *this->regs.controlRegister1  |= 0x3;
          break;
       }
       break;
    case SBS_ID:
       switch (model) {
       case SBS_IPOPTOIO8:
         /* Different register layout */
         memset(&this->regs, 0, sizeof(this->regs));
         this->regs.inputRegisterLow   = base + 1;
         this->regs.outputRegisterLow  = base + 2;
         this->regs.controlRegister0   = base + 3;
         *this->regs.controlRegister0 = 0x00;   /* Start state machine reset */
         *this->regs.controlRegister0 = 0x01;   /* ....   */
         *this->regs.controlRegister0 = 0x00;   /* State machine in state 0 */
         *this->regs.controlRegister0 = 0x2B;   /* Select Port B DDR */
         *this->regs.controlRegister0 = 0xFF;   /* All Port B bits are inputs */
         *this->regs.controlRegister0 = 0x2A;   /* Select Port B DPPR */
         *this->regs.controlRegister0 = 0xFF;   /* All Port B bits inverted */
         *this->regs.controlRegister0 = 0x01;   /* Select MCCR */
         *this->regs.controlRegister0 = 0x84;   /* Enable ports A and B */
          break;
        }
        break;
    }
    switch (model) {
       case UNIDIG_I_O_24I:
       case UNIDIG_I_E:
       case UNIDIG_I:
       case UNIDIG_I_D:
       case UNIDIG_I_O_24IO:
       case UNIDIG_I_HV_16I8O:
       case UNIDIG_I_O_12I12O:
       case UNIDIG_I_HV_8I16O:
          this->supportsInterrupts = 1;
          break;
       default:
          this->supportsInterrupts = 0;
          break;
    }

    /* Create the asynPortDriver parameter for the data */
    createParam("DIGITAL_DATA", asynParamUInt32Digital, &this->dataParam); 

    /* Start the thread to poll and handle interrupt callbacks to 
     * device support */
    epicsThreadCreate("ipUnidig",
                      epicsThreadPriorityHigh,
                      epicsThreadGetStackSize(epicsThreadStackBig),
                      (EPICSTHREADFUNC)pollerThreadC,
                      this);
    
    this->risingMask = risingMask;
    this->fallingMask = fallingMask;
    this->polarityMask = risingMask;
    /* If the interrupt vector is zero, don't bother with interrupts, 
     * since the user probably didn't pass this
     * parameter to IpUnidig::init().  This is an optional parameter added
     * after initial release. */
    if (this->supportsInterrupts && (intVec !=0)) {
       /* Interrupt support */
       /* Write to the interrupt polarity and enable registers */
       *this->regs.intVecRegister = intVec;
       if (devConnectInterruptVME(intVec, intFuncC, (void *)this)) {
           errlogPrintf("ipUnidig interrupt connect failure\n");
       }
       *this->regs.intPolarityRegisterLow  = (epicsUInt16) this->polarityMask;
       *this->regs.intPolarityRegisterHigh = (epicsUInt16) 
                                                    (this->polarityMask >> 16);
       writeIntEnableRegs();

       /* Enable IPAC module interrupts and set module status. */
       ipmIrqCmd(carrier, slot, 0, ipac_irqEnable);
       ipmIrqCmd(carrier, slot, 0, ipac_statActive);
    }

    epicsAtExit(rebootCallbackC, this);
}

    
asynStatus IpUnidig::readUInt32Digital(asynUser *pasynUser, epicsUInt32 *value, epicsUInt32 mask)
{
    static const char *functionName = "readUInt32Digital";
    ipUnidigRegisters r = this->regs;

    if(this->rebooting) epicsThreadSuspendSelf();
    *value = 0;
    if (r.inputRegisterLow)  *value  = (epicsUInt32) *r.inputRegisterLow;
    if (r.inputRegisterHigh) *value |= (epicsUInt32) (*r.inputRegisterHigh << 16);
    *value &= mask;
    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "%s:%s:, *value=%x\n", 
              driverName, functionName, *value);
    return(asynSuccess);
}

asynStatus IpUnidig::writeUInt32Digital(asynUser *pasynUser, epicsUInt32 value, epicsUInt32 mask)
{
    static const char *functionName = "writeUInt32Digital";
    ipUnidigRegisters r = this->regs;

    /* For the IP-Unidig differential output models, must enable all outputs */
    if(this->rebooting) epicsThreadSuspendSelf();
    if ((this->manufacturer == GREENSPRING_ID)  &&
        ((this->model == UNIDIG_D) || (this->model == UNIDIG_I_D))) {
         *r.outputEnableLow  |= (epicsUInt16) mask;
         *r.outputEnableHigh |= (epicsUInt16) (mask >> 16);
    }
    /* Set any bits that are set in the value and the mask */
    if (r.outputRegisterLow)  *r.outputRegisterLow  |= (epicsUInt16) (value & mask);
    if (r.outputRegisterHigh) *r.outputRegisterHigh |= (epicsUInt16) ((value & mask) >> 16);
    /* Clear bits that are clear in the value and set in the mask */
    if (r.outputRegisterLow)  *r.outputRegisterLow  &= (epicsUInt16) (value | ~mask);
    if (r.outputRegisterHigh) *r.outputRegisterHigh &=(epicsUInt16) ((value | ~mask) >> 16);
    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "%s:%s, value=%x, mask=%x\n", driverName, functionName, value, mask);
    return(asynSuccess);
}

asynStatus IpUnidig::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    static const char *functionName = "writeInt32";
    ipUnidigRegisters r = this->regs;
    if ((this->manufacturer == GREENSPRING_ID)  &&
        ((this->model == UNIDIG_HV_16I8O)   || (this->model == UNIDIG_HV_8I16O)  ||
         (this->model == UNIDIG_I_HV_16I8O) || (this->model == UNIDIG_HV_8I16O))) 
    {
         *r.DACRegister  = value;
         return(asynSuccess);
    } 
    else 
    {
        asynPrint(pasynUser, ASYN_TRACE_ERROR, "%s:%s,not allowed for this model",driverName, functionName);
        return(asynError);
    }
}

asynStatus IpUnidig::readInt32(asynUser *pasynUser, epicsInt32 *value)
{
    static const char *functionName = "readInt32";
    ipUnidigRegisters r = this->regs;

    if ((this->manufacturer == GREENSPRING_ID)  &&
        ((this->model == UNIDIG_HV_16I8O)   || (this->model == UNIDIG_HV_8I16O)  ||
         (this->model == UNIDIG_I_HV_16I8O) || (this->model == UNIDIG_HV_8I16O))) 
    {
         *value = *r.DACRegister;
         return(asynSuccess);
    } 
    else 
    {
        asynPrint(pasynUser, ASYN_TRACE_ERROR, "%s:%s,not allowed for this model",driverName, functionName);
        return(asynError);
    }
}

asynStatus IpUnidig::getBounds(asynUser *pasynUser, epicsInt32 *low, epicsInt32 *high)
{
    static const char *functionName = "getBounds";
    if ((this->manufacturer == GREENSPRING_ID)  &&
        ((this->model == UNIDIG_HV_16I8O)   || (this->model == UNIDIG_HV_8I16O)  ||
         (this->model == UNIDIG_I_HV_16I8O) || (this->model == UNIDIG_HV_8I16O))) 
    {
         *low = 0;
         *high = 4095;
         return(asynSuccess);
    } 
    else 
    {
       asynPrint(pasynUser, ASYN_TRACE_ERROR,"%s %s,not allowed for this model", driverName, functionName);
       return(asynError);
    }
}
 
asynStatus IpUnidig::setInterruptUInt32Digital(asynUser *pasynUser, epicsUInt32 mask, interruptReason reason)
{
    //static const char *functionName = "setInterruptUInt32Digital";
 
    switch (reason) {
    case interruptOnZeroToOne:
        this->risingMask = mask;
        break;
    case interruptOnOneToZero:
        this->fallingMask = mask;
        break;
    case interruptOnBoth:
        this->risingMask = mask;
        this->fallingMask = mask;
        break;
    }
    writeIntEnableRegs();
    return(asynSuccess);
}

asynStatus IpUnidig::clearInterruptUInt32Digital(asynUser *pasynUser, epicsUInt32 mask)
{
    //static const char *functionName = "clearInterruptUInt32Digital";

    this->risingMask &= ~mask;
    this->fallingMask &= ~mask;
    writeIntEnableRegs();
    return(asynSuccess);
}

asynStatus IpUnidig::getInterruptUInt32Digital(asynUser *pasynUser, epicsUInt32 *mask, interruptReason reason)
{
    //static const char *functionName = "getInterruptUInt32Digital";

    switch (reason) {
    case interruptOnZeroToOne:
        *mask = this->risingMask;
        break;
    case interruptOnOneToZero:
        *mask = this->fallingMask;
        break;
    case interruptOnBoth:
        *mask = this->risingMask | this->fallingMask;
        break;
    }
    return(asynSuccess);
}

void IpUnidig::intFunc()
{
    ipUnidigRegisters r = this->regs;
    epicsUInt32 inputs=0, pendingLow, pendingHigh, pendingMask, invertMask;
    ipUnidigMessage msg;

    /* Clear the interrupts by copying from the interrupt pending register to
     * the interrupt clear register */
    *r.intClearRegisterLow = pendingLow = *r.intPendingRegisterLow;
    *r.intClearRegisterHigh = pendingHigh = *r.intPendingRegisterHigh;
    pendingMask = pendingLow | (pendingHigh << 16);
    /* Read the current input.  Don't use read() because that can print debugging. */
    if (r.inputRegisterLow)  inputs = (epicsUInt32) *r.inputRegisterLow;
    if (r.inputRegisterHigh) inputs |= (epicsUInt32) (*r.inputRegisterHigh << 16);
    msg.bits = inputs;
    msg.interruptMask = pendingMask;
    if (epicsMessageQueueTrySend(this->msgQId, &msg, sizeof(msg)) == 0)
        this->messagesSent++;
    else
        this->messagesFailed++;

    /* Are there any bits which should generate interrupts on both the rising
     * and falling edge, and which just generated this interrupt? */
    invertMask = pendingMask & this->risingMask & this->fallingMask;
    if (invertMask != 0) {
        /* We want to invert all bits in the polarityMask that are set in 
         * invertMask. This is done with xor. */
        this->polarityMask = this->polarityMask ^ invertMask;
        *r.intPolarityRegisterLow  = (epicsUInt16) this->polarityMask;
        *r.intPolarityRegisterHigh = (epicsUInt16) (this->polarityMask >> 16);
    }
}


void IpUnidig::pollerThread()
{
    /* This function runs in a separate thread.  It waits for the poll
     * time, or an interrupt, whichever comes first.  If the bits read from
     * the ipUnidig have changed then it does callbacks to all clients that
     * have registered with registerDevCallback */
    //static const char *functionName = "pollerThread";
    epicsUInt32 newBits, changedBits, interruptMask=0;
    ipUnidigMessage msg;

    while(1) {      
        /*  Wait for an interrupt or for the poll time, whichever comes first */
        if (epicsMessageQueueReceiveWithTimeout(this->msgQId, 
                                                &msg, sizeof(msg), 
                                                this->pollTime) == -1) {
            /* The wait timed out, so there was no interrupt, so we need
             * to read the bits.  If there was an interrupt the bits got
             * set in the interrupt routines */
            readUInt32Digital(this->pasynUserSelf, &newBits, 0xffffffff);
            interruptMask = 0;
        } else {
            newBits = msg.bits;
            interruptMask = msg.interruptMask;
            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                      "drvIpUnidig::pollerThread, got interrupt\n");
        }

        asynPrint(this->pasynUserSelf, ASYN_TRACEIO_DRIVER,
                  "drvIpUnidig::pollerThread, bits=%x, this->oldBits=%x, interruptMask=%x\n", 
                  newBits, this->oldBits, interruptMask);

        /* We detect change both from interruptMask (which only works for
         * interrupts) and changedBits, which works for polling */
        changedBits = newBits ^ this->oldBits;
        interruptMask = interruptMask | changedBits;
        if (this->forceCallback) interruptMask = 0xffffff;
        if (interruptMask) {
            this->oldBits = newBits;
            this->forceCallback = 0;
            asynPortDriver::setUIntDigitalParam(0, newBits, 0xFFFFFFFF);
            asynPortDriver::setInterruptUInt32Digital(this->pasynUserSelf, interruptMask, interruptOnBoth);
            callParamCallbacks();
        }
    }
}


void IpUnidig::writeIntEnableRegs()
{
    ipUnidigRegisters r = this->regs;

    *r.intEnableRegisterLow  = (epicsUInt16) (this->risingMask | 
                                              this->fallingMask);
    *r.intEnableRegisterHigh = (epicsUInt16) ((this->risingMask | 
                                               this->fallingMask) >> 16);
}

void IpUnidig::rebootCallback()
{
    ipUnidigRegisters r = this->regs;

    *r.intEnableRegisterLow = 0;
    *r.intEnableRegisterHigh = 0;
    this->rebooting = 1;
}

void IpUnidig::report(FILE *fp, int details)
{
    ipUnidigRegisters r = this->regs;
    epicsUInt32 intEnableRegister = 0, intPolarityRegister = 0;

    fprintf(fp, "drvIpUnidig %s: connected at base address %p\n",
            this->portName, this->baseAddress);
    if (details >= 1) {
        if (r.intEnableRegisterLow)    intEnableRegister =     *r.intEnableRegisterLow;
        if (r.intEnableRegisterHigh)   intEnableRegister |=   (*r.intEnableRegisterHigh << 16);
        if (r.intPolarityRegisterLow)  intPolarityRegister =   *r.intPolarityRegisterLow;
        if (r.intPolarityRegisterHigh) intPolarityRegister |= (*r.intPolarityRegisterHigh << 16);

        fprintf(fp, "  risingMask=%x\n", this->risingMask);
        fprintf(fp, "  fallingMask=%x\n", this->fallingMask);
        fprintf(fp, "  intEnableRegister=%x\n", intEnableRegister);
        fprintf(fp, "  intPolarityRegister=%x\n", intPolarityRegister);
        fprintf(fp, "  messages sent OK=%d; send failed (queue full)=%d\n",
                this->messagesSent, this->messagesFailed);
    }
    asynPortDriver::report(fp, details);
}

extern "C" int initIpUnidig(const char *portName, int carrier, int slot,
                 int msecPoll, int intVec, int risingMask, 
                 int fallingMask)
{
    IpUnidig *pIpUnidig=new IpUnidig(portName,carrier,slot,msecPoll,intVec,risingMask,fallingMask);
    pIpUnidig=NULL;
    return(asynSuccess);
}

/* iocsh functions */
static const iocshArg initArg0 = { "Port name",iocshArgString};
static const iocshArg initArg1 = { "Carrier",iocshArgInt};
static const iocshArg initArg2 = { "Slot",iocshArgInt};
static const iocshArg initArg3 = { "msecPoll",iocshArgInt};
static const iocshArg initArg4 = { "intVec",iocshArgInt};
static const iocshArg initArg5 = { "risingMask",iocshArgInt};
static const iocshArg initArg6 = { "fallingMask",iocshArgInt};
static const iocshArg * const initArgs[7] = {&initArg0,
                                             &initArg1,
                                             &initArg2,
                                             &initArg3,
                                             &initArg4,
                                             &initArg5,
                                             &initArg6};
static const iocshFuncDef initFuncDef = {"initIpUnidig",7,initArgs};
static void initCallFunc(const iocshArgBuf *args)
{
    initIpUnidig(args[0].sval, args[1].ival, args[2].ival,
                 args[3].ival, args[4].ival, args[5].ival,
                 args[6].ival);
}
void ipUnidigRegister(void)
{
    iocshRegister(&initFuncDef,initCallFunc);
}

epicsExportRegistrar(ipUnidigRegister);






