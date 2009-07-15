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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
#include <asynDriver.h>
#include <asynUInt32Digital.h>
#include <asynInt32.h>


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

typedef struct {
    unsigned char manufacturer;
    unsigned char model;
    char *portName;
    asynUser *pasynUser;
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
    asynInterface common;
    asynInterface uint32D;
    asynInterface int32;
    void *interruptPvt;
} drvIpUnidigPvt;

/* These functions are in the asynCommon interface */
static void report                 (void *drvPvt, FILE *fp, int details);
static asynStatus connect          (void *drvPvt, asynUser *pasynUser);
static asynStatus disconnect       (void *drvPvt, asynUser *pasynUser);

/* These functions are in the asynUInt32Digital interface */
static asynStatus writeUInt32D     (void *drvPvt, asynUser *pasynUser,
                                    epicsUInt32 value, epicsUInt32 mask);
static asynStatus readUInt32D      (void *drvPvt, asynUser *pasynUser,
                                    epicsUInt32 *value, epicsUInt32 mask);
static asynStatus setInterrupt     (void *drvPvt, asynUser *pasynUser,
                                    epicsUInt32 mask, interruptReason reason);
static asynStatus clearInterrupt   (void *drvPvt, asynUser *pasynUser,
                                    epicsUInt32 mask);
static asynStatus getInterrupt     (void *drvPvt, asynUser *pasynUser,
                                    epicsUInt32 *mask, interruptReason reason);

/* These functions are in the asynInt32 interface */
static asynStatus setDAC           (void *drvPvt, asynUser *pasynUser,
                                    epicsInt32 value);
static asynStatus getDAC           (void *drvPvt, asynUser *pasynUser,
                                    epicsInt32 *value);
static asynStatus getDACBounds     (void *drvPvt, asynUser *pasynUser,
                                    epicsInt32 *low, epicsInt32 *high);

/* These are private functions */
static void pollerThread            (drvIpUnidigPvt *pPvt);
static void intFunc                 (void *); /* Interrupt function */
static void rebootCallback          (void *);
static void writeIntEnableRegs      (drvIpUnidigPvt *pPvt);

/* asynCommon methods */
static const struct asynCommon ipUnidigCommon = {
    report,
    connect,
    disconnect
};

/* asynUInt32Digital methods */
static struct asynUInt32Digital ipUnidigUInt32D = {
    writeUInt32D,
    readUInt32D,
    setInterrupt,
    clearInterrupt,
    getInterrupt,
    NULL,
    NULL
};

/* asynInt32 methods */
static asynInt32 ipUnidigInt32 = {
    setDAC,
    getDAC,
    getDACBounds
};


int initIpUnidig(const char *portName, ushort_t carrier, ushort_t slot,
                 int msecPoll, int intVec, int risingMask, 
                 int fallingMask)
{
    drvIpUnidigPvt *pPvt;
    ipac_idProm_t *id;
    unsigned char manufacturer;
    unsigned char model;
    epicsUInt16 *base;
    int status;

    pPvt = callocMustSucceed(1, sizeof(*pPvt), "initIpUnidig");
    pPvt->portName = epicsStrDup(portName);

    /* Default of 100 msec for backwards compatibility with old version */
    if (msecPoll == 0) msecPoll = 100;
    pPvt->pollTime = msecPoll / 1000.;
    pPvt->msgQId = epicsMessageQueueCreate(MAX_MESSAGES, 
                                           sizeof(ipUnidigMessage));

    if (ipmCheck(carrier, slot)) {
       errlogPrintf("initIpUnidig: bad carrier or slot\n");
       return(-1);
    }
    id = (ipac_idProm_t *) ipmBaseAddr(carrier, slot, ipac_addrID);
    base = (epicsUInt16 *) ipmBaseAddr(carrier, slot, ipac_addrIO);
    pPvt->baseAddress = base;

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
             errlogPrintf("initIpUnidig model 0x%x not supported\n",model);
             return(-1);
             break;
       }
       break;

    case SYSTRAN_ID:
       if(model != SYSTRAN_DIO316I) {
          errlogPrintf("initIpUnidig model 0x%x not Systran DIO316I\n",model);
          return(-1);
       }
       break;
       
    case SBS_ID:
       if(model != SBS_IPOPTOIO8) {
          errlogPrintf("initIpUnidig model 0x%x not SBS IP-OPTOIO-8\n",model);
          return(-1);
       }
       break;
       
    default: 
       errlogPrintf("initIpUnidig manufacturer 0x%x not supported\n", 
                    manufacturer);
       return(-1);
       break;
    }

    pPvt->manufacturer = manufacturer;
    pPvt->model = model;

    /* Link with higher level routines */
    pPvt->common.interfaceType = asynCommonType;
    pPvt->common.pinterface  = (void *)&ipUnidigCommon;
    pPvt->common.drvPvt = pPvt;
    pPvt->uint32D.interfaceType = asynUInt32DigitalType;
    pPvt->uint32D.pinterface  = (void *)&ipUnidigUInt32D;
    pPvt->uint32D.drvPvt = pPvt;
    pPvt->int32.interfaceType = asynInt32Type;
    pPvt->int32.pinterface  = (void *)&ipUnidigInt32;
    pPvt->int32.drvPvt = pPvt;
    status = pasynManager->registerPort(pPvt->portName,
                                        0, /* not multiDevice, can't block */
                                        1, /* autoconnect */
                                        0, /* medium priority */
                                        0); /* default stack size */
    if (status != asynSuccess) {
        errlogPrintf("initIpUnidig ERROR: Can't register port\n");
        return -1;
    }
    status = pasynManager->registerInterface(pPvt->portName,&pPvt->common);
    if (status != asynSuccess) {
        errlogPrintf("initIpUnidig ERROR: Can't register common.\n");
        return -1;
    }
    status = pasynUInt32DigitalBase->initialize(pPvt->portName, &pPvt->uint32D);
    if (status != asynSuccess) {
        errlogPrintf("initIpUnidig ERROR: Can't register UInt32Digital.\n");
        return -1;
    }
    pasynManager->registerInterruptSource(pPvt->portName, &pPvt->uint32D,
                                          &pPvt->interruptPvt);

    status = pasynInt32Base->initialize(pPvt->portName,&pPvt->int32);
    if (status != asynSuccess) {
        errlogPrintf("initIpUnidig ERROR: Can't register Int32.\n");
        return -1;
    }

    /* Create asynUser for asynTrace */
    pPvt->pasynUser = pasynManager->createAsynUser(0, 0);
    pPvt->pasynUser->userPvt = pPvt;

    /* Connect to device */
    status = pasynManager->connectDevice(pPvt->pasynUser, pPvt->portName, 0);
    if (status != asynSuccess) {
        errlogPrintf("initIpUnidig, connectDevice failed %s\n",
                     pPvt->pasynUser->errorMessage);
        return(-1);
    }

    /* Set up the register pointers.  Set the defaults for most modules */
    /* Define registers in units of 16-bit words */
    pPvt->regs.outputRegisterLow        = base;
    pPvt->regs.outputRegisterHigh       = base + 0x1;
    pPvt->regs.inputRegisterLow         = base + 0x2;
    pPvt->regs.inputRegisterHigh        = base + 0x3;
    pPvt->regs.outputEnableLow          = base + 0x8;
    pPvt->regs.outputEnableHigh         = base + 0x5;
    pPvt->regs.controlRegister0         = base + 0x6;
    pPvt->regs.intVecRegister           = base + 0x8;
    pPvt->regs.intEnableRegisterLow     = base + 0x9;
    pPvt->regs.intEnableRegisterHigh    = base + 0xa;
    pPvt->regs.intPolarityRegisterLow   = base + 0xb;
    pPvt->regs.intPolarityRegisterHigh  = base + 0xc;
    pPvt->regs.intClearRegisterLow      = base + 0xd;
    pPvt->regs.intClearRegisterHigh     = base + 0xe;
    pPvt->regs.intPendingRegisterLow    = base + 0xd;
    pPvt->regs.intPendingRegisterHigh   = base + 0xe;
    pPvt->regs.DACRegister              = base + 0xe;
    
    /* Set things up for specific models which need to be treated differently */
    switch (manufacturer) {
    case GREENSPRING_ID: 
       switch (model) {
       case UNIDIG_O_24IO:
       case UNIDIG_O_12I12O:
       case UNIDIG_I_O_24IO:
       case UNIDIG_I_O_12I12O:
          /* Enable outputs */
          *pPvt->regs.controlRegister0 |= 0x4;
          break;
       case UNIDIG_HV_16I8O:
       case UNIDIG_I_HV_16I8O:
          /*  These modules don't allow access to outputRegisterLow */
          pPvt->regs.outputRegisterLow = NULL;
          /* Set the comparator DAC for 2.5 volts.  Each bit is 15 mV. */
          *pPvt->regs.DACRegister = 2500/15;
          break;
       }
       break;
    case SYSTRAN_ID:
       switch (model) {
       case SYSTRAN_DIO316I:
          /* Different register layout */
          pPvt->regs.outputRegisterLow  = base;
          pPvt->regs.outputRegisterHigh = base + 0x1;
          pPvt->regs.inputRegisterLow   = base + 0x2;
          pPvt->regs.inputRegisterHigh  = NULL;
          pPvt->regs.controlRegister0   = base + 0x3;
          pPvt->regs.controlRegister1   = base + 0x4;
          /* Enable outputs for ports 0-3 */
          *pPvt->regs.controlRegister0  |= 0xf;
          /* Set direction of ports 0-1 to be output */
          *pPvt->regs.controlRegister1  |= 0x3;
          break;
       }
       break;
    case SBS_ID:
       switch (model) {
       case SBS_IPOPTOIO8:
         /* Different register layout */
         memset(&pPvt->regs, 0, sizeof(pPvt->regs));
         pPvt->regs.inputRegisterLow   = base + 1;
         pPvt->regs.outputRegisterLow  = base + 2;
         pPvt->regs.controlRegister0   = base + 3;
         *pPvt->regs.controlRegister0 = 0x00;   /* Start state machine reset */
         *pPvt->regs.controlRegister0 = 0x01;   /* ....   */
         *pPvt->regs.controlRegister0 = 0x00;   /* State machine in state 0 */
         *pPvt->regs.controlRegister0 = 0x2B;   /* Select Port B DDR */
         *pPvt->regs.controlRegister0 = 0xFF;   /* All Port B bits are inputs */
         *pPvt->regs.controlRegister0 = 0x2A;   /* Select Port B DPPR */
         *pPvt->regs.controlRegister0 = 0xFF;   /* All Port B bits inverted */
         *pPvt->regs.controlRegister0 = 0x01;   /* Select MCCR */
         *pPvt->regs.controlRegister0 = 0x84;   /* Enable ports A and B */
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
          pPvt->supportsInterrupts = 1;
          break;
       default:
          pPvt->supportsInterrupts = 0;
          break;
    }

    /* Start the thread to poll and handle interrupt callbacks to 
     * device support */
    epicsThreadCreate("ipUnidig",
                      epicsThreadPriorityHigh,
                      epicsThreadGetStackSize(epicsThreadStackBig),
                      (EPICSTHREADFUNC)pollerThread,
                      pPvt);
 
    pPvt->risingMask = risingMask;
    pPvt->fallingMask = fallingMask;
    pPvt->polarityMask = risingMask;
    /* If the interrupt vector is zero, don't bother with interrupts, 
     * since the user probably didn't pass this
     * parameter to IpUnidig::init().  This is an optional parameter added
     * after initial release. */
    if (pPvt->supportsInterrupts && (intVec !=0)) {
       /* Interrupt support */
       /* Write to the interrupt polarity and enable registers */
       *pPvt->regs.intVecRegister = intVec;
       if (devConnectInterruptVME(intVec, intFunc, (void *)pPvt)) {
           errlogPrintf("ipUnidig interrupt connect failure\n");
           return(-1);
       }
       *pPvt->regs.intPolarityRegisterLow  = (epicsUInt16) pPvt->polarityMask;
       *pPvt->regs.intPolarityRegisterHigh = (epicsUInt16) 
                                                    (pPvt->polarityMask >> 16);
       writeIntEnableRegs(pPvt);

       /* Enable IPAC module interrupts and set module status. */
       ipmIrqCmd(carrier, slot, 0, ipac_irqEnable);
       ipmIrqCmd(carrier, slot, 0, ipac_statActive);
    }

    epicsAtExit(rebootCallback, pPvt);
    return(0);
}



static asynStatus readUInt32D(void *drvPvt, asynUser *pasynUser, epicsUInt32 *value,
                       epicsUInt32 mask)
{
    drvIpUnidigPvt *pPvt = (drvIpUnidigPvt *)drvPvt;
    ipUnidigRegisters r = pPvt->regs;

    if(pPvt->rebooting) epicsThreadSuspendSelf();
    *value = 0;
    if (r.inputRegisterLow)  *value  = (epicsUInt32) *r.inputRegisterLow;
    if (r.inputRegisterHigh) *value |= (epicsUInt32) (*r.inputRegisterHigh << 16);
    *value &= mask;
    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "drvIpUnidig::read, *value=%x\n", *value);
    return(asynSuccess);
}

static asynStatus writeUInt32D(void *drvPvt, asynUser *pasynUser, epicsUInt32 value,
                        epicsUInt32 mask)
{
    drvIpUnidigPvt *pPvt = (drvIpUnidigPvt *)drvPvt;
    ipUnidigRegisters r = pPvt->regs;

    /* For the IP-Unidig differential output models, must enable all outputs */
    if(pPvt->rebooting) epicsThreadSuspendSelf();
    if ((pPvt->manufacturer == GREENSPRING_ID)  &&
        ((pPvt->model == UNIDIG_D) || (pPvt->model == UNIDIG_I_D))) {
         *r.outputEnableLow  |= (epicsUInt16) mask;
         *r.outputEnableHigh |= (epicsUInt16) (mask >> 16);
    }
    /* Set any bits that are set in the value and the mask */
    if (r.outputRegisterLow)  *r.outputRegisterLow  |= 
                                          (epicsUInt16) (value & mask);
    if (r.outputRegisterHigh) *r.outputRegisterHigh |= 
                                          (epicsUInt16) ((value & mask) >> 16);
    /* Clear bits that are clear in the value and set in the mask */
    if (r.outputRegisterLow)  *r.outputRegisterLow  &= 
                                          (epicsUInt16) (value | ~mask);
    if (r.outputRegisterHigh) *r.outputRegisterHigh &= 
                                          (epicsUInt16) ((value | ~mask) >> 16);
    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "drvIpUnidig::write, value=%x, mask=%x\n",
              value, mask);
    return(asynSuccess);
}


static asynStatus setDAC(void *drvPvt, asynUser *pasynUser, epicsInt32 value)
{
    drvIpUnidigPvt *pPvt = (drvIpUnidigPvt *)drvPvt;
    ipUnidigRegisters r = pPvt->regs;

    if ((pPvt->manufacturer == GREENSPRING_ID)  &&
        ((pPvt->model == UNIDIG_HV_16I8O)   || (pPvt->model == UNIDIG_HV_8I16O)  ||
         (pPvt->model == UNIDIG_I_HV_16I8O) || (pPvt->model == UNIDIG_HV_8I16O))) {
         *r.DACRegister  = value;
         return(asynSuccess);
    } else {
       epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                     "drvIpUnidig:setDAC not allowed for this model");
       return(asynError);
    }
    return(asynSuccess); 
}

static asynStatus getDAC(void *drvPvt, asynUser *pasynUser, epicsInt32 *value)
{
    drvIpUnidigPvt *pPvt = (drvIpUnidigPvt *)drvPvt;
    ipUnidigRegisters r = pPvt->regs;
    if ((pPvt->manufacturer == GREENSPRING_ID)  &&
        ((pPvt->model == UNIDIG_HV_16I8O)   || (pPvt->model == UNIDIG_HV_8I16O)  ||
         (pPvt->model == UNIDIG_I_HV_16I8O) || (pPvt->model == UNIDIG_HV_8I16O))) {
         *value = *r.DACRegister;
         return(asynSuccess);
    } else {
       epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                     "drvIpUnidig:getDAC not allowed for this model");
       return(asynError);
    }
    return(asynSuccess);
}

static asynStatus getDACBounds(void *drvPvt, asynUser *pasynUser, 
                               epicsInt32 *low, epicsInt32 *high)
{
    drvIpUnidigPvt *pPvt = (drvIpUnidigPvt *)drvPvt;
    if ((pPvt->manufacturer == GREENSPRING_ID)  &&
        ((pPvt->model == UNIDIG_HV_16I8O)   || (pPvt->model == UNIDIG_HV_8I16O)  ||
         (pPvt->model == UNIDIG_I_HV_16I8O) || (pPvt->model == UNIDIG_HV_8I16O))) {
         *low = 0;
         *high = 4095;
         return(asynSuccess);
    } else {
       epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                 "drvIpUnidig:getDAC not allowed for this model");
       return(asynError);
    }
    return(asynSuccess);
}

static asynStatus setInterrupt(void *drvPvt, asynUser *pasynUser,
                               epicsUInt32 mask, interruptReason reason)
{
    drvIpUnidigPvt *pPvt = (drvIpUnidigPvt *)drvPvt;

    switch (reason) {
    case interruptOnZeroToOne:
        pPvt->risingMask = mask;
        break;
    case interruptOnOneToZero:
        pPvt->fallingMask = mask;
        break;
    case interruptOnBoth:
        pPvt->risingMask = mask;
        pPvt->fallingMask = mask;
        break;
    }
    writeIntEnableRegs(pPvt);
    return(asynSuccess);
}

static asynStatus clearInterrupt(void *drvPvt, asynUser *pasynUser,
                               epicsUInt32 mask)
{
    drvIpUnidigPvt *pPvt = (drvIpUnidigPvt *)drvPvt;

    pPvt->risingMask &= ~mask;
    pPvt->fallingMask &= ~mask;
    writeIntEnableRegs(pPvt);
    return(asynSuccess);
}

static asynStatus getInterrupt(void *drvPvt, asynUser *pasynUser,
                               epicsUInt32 *mask, interruptReason reason)
{
    drvIpUnidigPvt *pPvt = (drvIpUnidigPvt *)drvPvt;

    switch (reason) {
    case interruptOnZeroToOne:
        *mask = pPvt->risingMask;
        break;
    case interruptOnOneToZero:
        *mask = pPvt->fallingMask;
        break;
    case interruptOnBoth:
        *mask = pPvt->risingMask | pPvt->fallingMask;
        break;
    }
    return(asynSuccess);
}


static void intFunc(void *drvPvt)
{
    drvIpUnidigPvt *pPvt = (drvIpUnidigPvt *)drvPvt;
    ipUnidigRegisters r = pPvt->regs;
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
    if (epicsMessageQueueTrySend(pPvt->msgQId, &msg, sizeof(msg)) == 0)
        pPvt->messagesSent++;
    else
        pPvt->messagesFailed++;

    /* Are there any bits which should generate interrupts on both the rising
     * and falling edge, and which just generated this interrupt? */
    invertMask = pendingMask & pPvt->risingMask & pPvt->fallingMask;
    if (invertMask != 0) {
        /* We want to invert all bits in the polarityMask that are set in 
         * invertMask. This is done with xor. */
        pPvt->polarityMask = pPvt->polarityMask ^ invertMask;
        *r.intPolarityRegisterLow  = (epicsUInt16) pPvt->polarityMask;
        *r.intPolarityRegisterHigh = (epicsUInt16) (pPvt->polarityMask >> 16);
    }
}


static void pollerThread(drvIpUnidigPvt *pPvt)
{
    /* This function runs in a separate thread.  It waits for the poll
     * time, or an interrupt, whichever comes first.  If the bits read from
     * the ipUnidig have changed then it does callbacks to all clients that
     * have registered with registerDevCallback */
    epicsUInt32 newBits, changedBits, interruptMask=0;
    ipUnidigMessage msg;
    ELLLIST *pclientList;
    interruptNode *pnode;
    asynUInt32DigitalInterrupt *pUInt32D;

    while(1) {      
        /*  Wait for an interrupt or for the poll time, whichever comes first */
        if (epicsMessageQueueReceiveWithTimeout(pPvt->msgQId, 
                                                &msg, sizeof(msg), 
                                                pPvt->pollTime) == -1) {
            /* The wait timed out, so there was no interrupt, so we need
             * to read the bits.  If there was an interrupt the bits got
             * set in the interrupt routines */
            readUInt32D(pPvt, pPvt->pasynUser, &newBits, 0xffffffff);
            interruptMask = 0;
        } else {
            newBits = msg.bits;
            interruptMask = msg.interruptMask;
            asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW,
                      "drvIpUnidig::pollerThread, got interrupt\n");
        }
        asynPrint(pPvt->pasynUser, ASYN_TRACEIO_DRIVER,
                  "drvIpUnidig::pollerThread, bits=%x, pPvt->oldBits=%x, interruptMask=%x\n", 
                  newBits, pPvt->oldBits, interruptMask);

        /* We detect change both from interruptMask (which only works for
         * interrupts) and changedBits, which works for polling */
        changedBits = newBits ^ pPvt->oldBits;
        interruptMask = interruptMask | changedBits;
        if (pPvt->forceCallback) interruptMask = 0xffffff;
        if (interruptMask) {
            pPvt->forceCallback = 0;
            pPvt->oldBits = newBits;
            pasynManager->interruptStart(pPvt->interruptPvt, &pclientList);
            pnode = (interruptNode *)ellFirst(pclientList);
            while (pnode) {
                pUInt32D = pnode->drvPvt;
                if (pUInt32D->mask & interruptMask) {
                    asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW,
                              "drvIpUnidig::pollerThread, calling client %p"
                              " mask=%x, callback=%p\n",
                              pUInt32D, pUInt32D->mask, pUInt32D->callback);
                    pUInt32D->callback(pUInt32D->userPvt, pUInt32D->pasynUser,
                                       pUInt32D->mask & newBits);
                }
                pnode = (interruptNode *)ellNext(&pnode->node);
            }
            pasynManager->interruptEnd(pPvt->interruptPvt);
        }
    }
}


static void writeIntEnableRegs(drvIpUnidigPvt *pPvt)
{
    ipUnidigRegisters r = pPvt->regs;

    *r.intEnableRegisterLow  = (epicsUInt16) (pPvt->risingMask | 
                                              pPvt->fallingMask);
    *r.intEnableRegisterHigh = (epicsUInt16) ((pPvt->risingMask | 
                                               pPvt->fallingMask) >> 16);
}

static void rebootCallback(void *drvPvt)
{
    drvIpUnidigPvt *pPvt = (drvIpUnidigPvt *)drvPvt;
    ipUnidigRegisters r = pPvt->regs;

    *r.intEnableRegisterLow = 0;
    *r.intEnableRegisterHigh = 0;
    pPvt->rebooting = 1;
}


/* asynCommon routines */

/* Report  parameters */
static void report(void *drvPvt, FILE *fp, int details)
{
    drvIpUnidigPvt *pPvt = (drvIpUnidigPvt *)drvPvt;
    ipUnidigRegisters r = pPvt->regs;
    epicsUInt32 intEnableRegister = 0, intPolarityRegister = 0;
    interruptNode *pnode;
    ELLLIST *pclientList;

    fprintf(fp, "drvIpUnidig %s: connected at base address %p\n",
            pPvt->portName, pPvt->baseAddress);
    if (details >= 1) {
        if (r.intEnableRegisterLow)    intEnableRegister =     *r.intEnableRegisterLow;
        if (r.intEnableRegisterHigh)   intEnableRegister |=   (*r.intEnableRegisterHigh << 16);
        if (r.intPolarityRegisterLow)  intPolarityRegister =   *r.intPolarityRegisterLow;
        if (r.intPolarityRegisterHigh) intPolarityRegister |= (*r.intPolarityRegisterHigh << 16);

        fprintf(fp, "  risingMask=%x\n", pPvt->risingMask);
        fprintf(fp, "  fallingMask=%x\n", pPvt->fallingMask);
        fprintf(fp, "  intEnableRegister=%x\n", intEnableRegister);
        fprintf(fp, "  intPolarityRegister=%x\n", intPolarityRegister);
        fprintf(fp, "  messages sent OK=%d; send failed (queue full)=%d\n",
                pPvt->messagesSent, pPvt->messagesFailed);
            pasynManager->interruptStart(pPvt->interruptPvt, &pclientList);
        pnode = (interruptNode *)ellFirst(pclientList);
        while (pnode) {
            asynUInt32DigitalInterrupt *pUInt32D = pnode->drvPvt;
            fprintf(fp, "  uint32Digital callback client address=%p, mask=%x\n",
                    pUInt32D->callback, pUInt32D->mask);
            pnode = (interruptNode *)ellNext(&pnode->node);
        }
        pasynManager->interruptEnd(pPvt->interruptPvt);
     }
}

/* Connect */
static asynStatus connect(void *drvPvt, asynUser *pasynUser)
{
    pasynManager->exceptionConnect(pasynUser);
    return(asynSuccess);
}

/* Connect */
static asynStatus disconnect(void *drvPvt, asynUser *pasynUser)
{
    pasynManager->exceptionDisconnect(pasynUser);
    return(asynSuccess);
}


/* iocsh functions */
static const iocshArg initArg0 = { "Server name",iocshArgString};
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

