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

#define digitalInputString  "DIGITAL_INPUT"
#define digitalOutputString "DIGITAL_OUTPUT"
#define DACOutputString     "DAC_OUTPUT"

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


static const char *driverName = "drvIpUnidig";


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
  unsigned char manufacturer_;
  unsigned char model_;
  epicsUInt16 *baseAddress_;
  int supportsInterrupts_;
  int rebooting_;
  epicsUInt32 risingMask_;
  epicsUInt32 fallingMask_;
  epicsUInt32 polarityMask_;
  epicsUInt32 oldBits_;
  ipUnidigRegisters regs_;
  int forceCallback_;
  double pollTime_;
  epicsMessageQueueId msgQId_;
  int messagesSent_;
  int messagesFailed_;
  // We need separate parameters for input and output because we don't want device
  // support to set the output records based on the input records, which it will do
  // if they are the same parameter.
  int digitalInputParam_;
  #define FIRST_IPUNIDIG_PARAM digitalInputParam_
  int digitalOutputParam_;
  int DACOutputParam_;
  #define LAST_IPUNIDIG_PARAM DACOutputParam_
  
  void writeIntEnableRegs();
};

#define NUM_IPUNIDIG_PARAMS (&LAST_IPUNIDIG_PARAM - &FIRST_IPUNIDIG_PARAM + 1)

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
  :asynPortDriver(portName,1,NUM_IPUNIDIG_PARAMS,
                  asynInt32Mask | asynUInt32DigitalMask | asynDrvUserMask,
                  asynUInt32DigitalMask,
                  0,1,0,0),
  risingMask_(risingMask),
  fallingMask_(fallingMask),
  polarityMask_(risingMask)
  
{
  //static const char *functionName = "IpUnidig";
  ipac_idProm_t *id;
  epicsUInt16 *base;

  /* Default of 100 msec for backwards compatibility with old version */
  if (msecPoll == 0) msecPoll = 100;
  pollTime_ = msecPoll / 1000.;
  messagesSent_ = 0;
  messagesFailed_ = 0;
  rebooting_ = 0;
  forceCallback_ = 0;
  oldBits_ = 0;
  msgQId_ = epicsMessageQueueCreate(MAX_MESSAGES, sizeof(ipUnidigMessage));

  if (ipmCheck(carrier, slot)) {
    errlogPrintf("IpUnidig: bad carrier or slot\n");
  }
  id = (ipac_idProm_t *) ipmBaseAddr(carrier, slot, ipac_addrID);
  base = (epicsUInt16 *) ipmBaseAddr(carrier, slot, ipac_addrIO);
  baseAddress_ = base;

  manufacturer_ = id->manufacturerId & 0xff;
  model_ = id->modelId & 0xff;
  switch (manufacturer_) {
  case GREENSPRING_ID:
    switch (model_) {
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
        errlogPrintf("IpUnidig model 0x%x not supported\n",model_);
        break;
    }
    break;

  case SYSTRAN_ID:
    if(model_ != SYSTRAN_DIO316I) {
      errlogPrintf("IpUnidig model 0x%x not Systran DIO316I\n",model_);
    }
    break;
       
  case SBS_ID:
    if(model_ != SBS_IPOPTOIO8) {
      errlogPrintf("IpUnidig model 0x%x not SBS IP-OPTOIO-8\n",model_);
    }
    break;
     
  default: 
    errlogPrintf("IpUnidig manufacturer 0x%x not supported\n", manufacturer_);
    break;
  }

  /* Set up the register pointers.  Set the defaults for most modules */
  /* Define registers in units of 16-bit words */
  regs_.outputRegisterLow        = base;
  regs_.outputRegisterHigh       = base + 0x1;
  regs_.inputRegisterLow         = base + 0x2;
  regs_.inputRegisterHigh        = base + 0x3;
  regs_.outputEnableLow          = base + 0x8;
  regs_.outputEnableHigh         = base + 0x5;
  regs_.controlRegister0         = base + 0x6;
  regs_.intVecRegister           = base + 0x8;
  regs_.intEnableRegisterLow     = base + 0x9;
  regs_.intEnableRegisterHigh    = base + 0xa;
  regs_.intPolarityRegisterLow   = base + 0xb;
  regs_.intPolarityRegisterHigh  = base + 0xc;
  regs_.intClearRegisterLow      = base + 0xd;
  regs_.intClearRegisterHigh     = base + 0xe;
  regs_.intPendingRegisterLow    = base + 0xd;
  regs_.intPendingRegisterHigh   = base + 0xe;
  regs_.DACRegister              = base + 0xe;
    
  /* Set things up for specific models which need to be treated differently */
  switch (manufacturer_) {
    case GREENSPRING_ID: 
      switch (model_) {
        case UNIDIG_O_24IO:
        case UNIDIG_O_12I12O:
        case UNIDIG_I_O_24IO:
        case UNIDIG_I_O_12I12O:
          /* Enable outputs */
          *regs_.controlRegister0 |= 0x4;
          break;
        case UNIDIG_HV_16I8O:
        case UNIDIG_I_HV_16I8O:
          /*  These modules don't allow access to outputRegisterLow */
          regs_.outputRegisterLow = NULL;
          /* Set the comparator DAC for 2.5 volts.  Each bit is 15 mV. */
          *regs_.DACRegister = 2500/15;
          break;
      }
      break;
    case SYSTRAN_ID:
      switch (model_) {
        case SYSTRAN_DIO316I:
          /* Different register layout */
          regs_.outputRegisterLow  = base;
          regs_.outputRegisterHigh = base + 0x1;
          regs_.inputRegisterLow   = base + 0x2;
          regs_.inputRegisterHigh  = NULL;
          regs_.controlRegister0   = base + 0x3;
          regs_.controlRegister1   = base + 0x4;
          /* Enable outputs for ports 0-3 */
          *regs_.controlRegister0  |= 0xf;
          /* Set direction of ports 0-1 to be output */
          *regs_.controlRegister1  |= 0x3;
          break;
      }
      break;
    case SBS_ID:
      switch (model_) {
        case SBS_IPOPTOIO8:
          /* Different register layout */
          memset(&regs_, 0, sizeof(regs_));
          regs_.inputRegisterLow   = base + 1;
          regs_.outputRegisterLow  = base + 2;
          regs_.controlRegister0   = base + 3;
          *regs_.controlRegister0 = 0x00;   /* Start state machine reset */
          *regs_.controlRegister0 = 0x01;   /* ....   */
          *regs_.controlRegister0 = 0x00;   /* State machine in state 0 */
          *regs_.controlRegister0 = 0x2B;   /* Select Port B DDR */
          *regs_.controlRegister0 = 0xFF;   /* All Port B bits are inputs */
          *regs_.controlRegister0 = 0x2A;   /* Select Port B DPPR */
          *regs_.controlRegister0 = 0xFF;   /* All Port B bits inverted */
          *regs_.controlRegister0 = 0x01;   /* Select MCCR */
          *regs_.controlRegister0 = 0x84;   /* Enable ports A and B */
           break;
      }
      break;
  }
  switch (model_) {
    case UNIDIG_I_O_24I:
    case UNIDIG_I_E:
    case UNIDIG_I:
    case UNIDIG_I_D:
    case UNIDIG_I_O_24IO:
    case UNIDIG_I_HV_16I8O:
    case UNIDIG_I_O_12I12O:
    case UNIDIG_I_HV_8I16O:
      supportsInterrupts_ = 1;
      break;
    default:
      supportsInterrupts_ = 0;
      break;
  }

  /* Create the asynPortDriver parameter for the data */
  createParam(digitalInputString,  asynParamUInt32Digital, &digitalInputParam_); 
  createParam(digitalOutputString, asynParamUInt32Digital, &digitalOutputParam_); 
  createParam(DACOutputString,     asynParamInt32,         &DACOutputParam_); 

  // We use this to call readUInt32Digital, which needs the correct reason
  pasynUserSelf->reason = digitalInputParam_;
  
  // Set the values of rising mask and falling mask in the parameter library, just for reporting purposes
  asynPortDriver::setUInt32DigitalInterrupt(digitalInputParam_, risingMask_, interruptOnZeroToOne);
  asynPortDriver::setUInt32DigitalInterrupt(digitalInputParam_, fallingMask_, interruptOnOneToZero);
   
  /* Start the thread to poll and handle interrupt callbacks to 
   * device support */
  epicsThreadCreate("ipUnidig",
                    epicsThreadPriorityHigh,
                    epicsThreadGetStackSize(epicsThreadStackBig),
                    (EPICSTHREADFUNC)pollerThreadC,
                    this);

  /* If the interrupt vector is zero, don't bother with interrupts, 
   * since the user probably didn't pass this
   * parameter to IpUnidig::init().  This is an optional parameter added
   * after initial release. */
  if (supportsInterrupts_ && (intVec !=0)) {
    /* Interrupt support */
    /* Write to the interrupt polarity and enable registers */
    *regs_.intVecRegister = intVec;
    if (devConnectInterruptVME(intVec, intFuncC, (void *)this)) {
      errlogPrintf("ipUnidig interrupt connect failure\n");
    }
    *regs_.intPolarityRegisterLow  = (epicsUInt16)polarityMask_;
    *regs_.intPolarityRegisterHigh = (epicsUInt16)(polarityMask_ >> 16);
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
  ipUnidigRegisters r = regs_;

  if(rebooting_) epicsThreadSuspendSelf();
  if (pasynUser->reason != digitalInputParam_) {
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "%s:%s:, invalid reason=%d\n", 
              driverName, functionName, pasynUser->reason);
    return(asynError);
  }
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
  ipUnidigRegisters r = regs_;

  /* For the IP-Unidig differential output models, must enable all outputs */
  if(rebooting_) epicsThreadSuspendSelf();
  if (pasynUser->reason != digitalOutputParam_) {
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "%s:%s:, invalid reason=%d\n", 
              driverName, functionName, pasynUser->reason);
    return(asynError);
  }
  /* Put value in parameter library */
  setUIntDigitalParam(pasynUser->reason, value, mask);
  
  if ((manufacturer_ == GREENSPRING_ID)  &&
      ((model_ == UNIDIG_D) || (model_ == UNIDIG_I_D))) {
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
            "%s:%s:, value=%x, mask=%x\n", 
            driverName, functionName, value, mask);
  return(asynSuccess);
}

asynStatus IpUnidig::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
  static const char *functionName = "writeInt32";
  if (pasynUser->reason != DACOutputParam_) {
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "%s:%s:, invalid reason=%d\n", 
              driverName, functionName, pasynUser->reason);
    return(asynError);
  }
  /* Put value in parameter library */
  setIntegerParam(pasynUser->reason, value);

  ipUnidigRegisters r = regs_;
  if ((manufacturer_ == GREENSPRING_ID)  &&
      ((model_ == UNIDIG_HV_16I8O)   || (model_ == UNIDIG_HV_8I16O)  ||
       (model_ == UNIDIG_I_HV_16I8O) || (model_ == UNIDIG_HV_8I16O))) 
  {
    *r.DACRegister  = value;
    return(asynSuccess);
  } 
  else 
  {
    asynPrint(pasynUser, ASYN_TRACE_ERROR, 
        "%s:%s,not allowed for this model",
        driverName, functionName);
    return(asynError);
  }
}

asynStatus IpUnidig::readInt32(asynUser *pasynUser, epicsInt32 *value)
{
  static const char *functionName = "readInt32";
  ipUnidigRegisters r = regs_;

  if (pasynUser->reason != DACOutputParam_) {
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "%s:%s:, invalid reason=%d\n", 
              driverName, functionName, pasynUser->reason);
    return(asynError);
  }
  if ((manufacturer_ == GREENSPRING_ID)  &&
      ((model_ == UNIDIG_HV_16I8O)   || (model_ == UNIDIG_HV_8I16O)  ||
       (model_ == UNIDIG_I_HV_16I8O) || (model_ == UNIDIG_HV_8I16O))) 
  {
    *value = *r.DACRegister;
    return(asynSuccess);
  } 
  else 
  {
    asynPrint(pasynUser, ASYN_TRACE_ERROR, 
        "%s:%s: not allowed for this model",
        driverName, functionName);
    return(asynError);
  }
}

asynStatus IpUnidig::getBounds(asynUser *pasynUser, epicsInt32 *low, epicsInt32 *high)
{
  static const char *functionName = "getBounds";
  if ((manufacturer_ == GREENSPRING_ID)  &&
      ((model_ == UNIDIG_HV_16I8O)   || (model_ == UNIDIG_HV_8I16O)  ||
       (model_ == UNIDIG_I_HV_16I8O) || (model_ == UNIDIG_HV_8I16O))) 
  {
    *low = 0;
    *high = 4095;
    return(asynSuccess);
  } 
  else 
  {
    asynPrint(pasynUser, ASYN_TRACE_ERROR,
        "%s:%s:, not allowed for this model", 
        driverName, functionName);
    return(asynError);
  }
}
 
asynStatus IpUnidig::setInterruptUInt32Digital(asynUser *pasynUser, epicsUInt32 mask, interruptReason reason)
{
  //static const char *functionName = "setInterruptUInt32Digital";
 
  /* Call the base class method to put mask in parameter library so report shows it */
  asynPortDriver::setInterruptUInt32Digital(pasynUser, mask, reason);
  
  switch (reason) {
    case interruptOnZeroToOne:
      risingMask_ = mask;
      break;
    case interruptOnOneToZero:
      fallingMask_ = mask;
      break;
    case interruptOnBoth:
      risingMask_ = mask;
      fallingMask_ = mask;
      break;
  }
  writeIntEnableRegs();
  return(asynSuccess);
}

asynStatus IpUnidig::clearInterruptUInt32Digital(asynUser *pasynUser, epicsUInt32 mask)
{
  //static const char *functionName = "clearInterruptUInt32Digital";

  /* Call the base class method to put mask in parameter library so report shows it */
  asynPortDriver::clearInterruptUInt32Digital(pasynUser, mask);

  risingMask_ &= ~mask;
  fallingMask_ &= ~mask;
  writeIntEnableRegs();
  return(asynSuccess);
}

asynStatus IpUnidig::getInterruptUInt32Digital(asynUser *pasynUser, epicsUInt32 *mask, interruptReason reason)
{
  //static const char *functionName = "getInterruptUInt32Digital";

  switch (reason) {
    case interruptOnZeroToOne:
      *mask = risingMask_;
      break;
    case interruptOnOneToZero:
      *mask = fallingMask_;
      break;
    case interruptOnBoth:
      *mask = risingMask_ | fallingMask_;
      break;
  }
  return(asynSuccess);
}

void IpUnidig::intFunc()
{
  ipUnidigRegisters r = regs_;
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
  if (epicsMessageQueueTrySend(msgQId_, &msg, sizeof(msg)) == 0)
    messagesSent_++;
  else
    messagesFailed_++;

  /* Are there any bits which should generate interrupts on both the rising
   * and falling edge, and which just generated this interrupt? */
  invertMask = pendingMask & risingMask_ & fallingMask_;
  if (invertMask != 0) {
    /* We want to invert all bits in the polarityMask that are set in 
     * invertMask. This is done with xor. */
    polarityMask_ = polarityMask_ ^ invertMask;
    *r.intPolarityRegisterLow  = (epicsUInt16) polarityMask_;
    *r.intPolarityRegisterHigh = (epicsUInt16) (polarityMask_ >> 16);
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
  static const char *functionName = "pollerThread";

  while(1) {    
    /*  Wait for an interrupt or for the poll time, whichever comes first */
    if (epicsMessageQueueReceiveWithTimeout(msgQId_, 
                                            &msg, sizeof(msg), 
                                            pollTime_) == -1) {
      /* The wait timed out, so there was no interrupt, so we need
       * to read the bits.  If there was an interrupt the bits got
       * set in the interrupt routines */
      readUInt32Digital(this->pasynUserSelf, &newBits, 0xffffffff);
      interruptMask = 0;
    } else {
      newBits = msg.bits;
      interruptMask = msg.interruptMask;
      asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                "%s:%s:, got interrupt\n",
                driverName, functionName);
    }

    asynPrint(this->pasynUserSelf, ASYN_TRACEIO_DRIVER,
              "%s:%s:, bits=%x, oldBits=%x, interruptMask=%x\n", 
              driverName, functionName, newBits, oldBits_, interruptMask);

    /* We detect change both from interruptMask (which only works for
     * interrupts) and changedBits, which works for polling */
    changedBits = newBits ^ oldBits_;
    interruptMask = interruptMask | changedBits;
    if (forceCallback_) interruptMask = 0xffffff;
    if (interruptMask) {
      oldBits_ = newBits;
      forceCallback_ = 0;
      asynPortDriver::setUIntDigitalParam(digitalInputParam_, newBits, 0xFFFFFFFF, interruptMask);
      callParamCallbacks();
    }
  }
}


void IpUnidig::writeIntEnableRegs()
{
  ipUnidigRegisters r = regs_;

  *r.intEnableRegisterLow  = (epicsUInt16) (risingMask_ | 
                                            fallingMask_);
  *r.intEnableRegisterHigh = (epicsUInt16) ((risingMask_ | 
                                             fallingMask_) >> 16);
}

void IpUnidig::rebootCallback()
{
  ipUnidigRegisters r = regs_;

  *r.intEnableRegisterLow = 0;
  *r.intEnableRegisterHigh = 0;
  rebooting_ = 1;
}

void IpUnidig::report(FILE *fp, int details)
{
  ipUnidigRegisters r = regs_;
  epicsUInt32 intEnableRegister = 0, intPolarityRegister = 0;

  fprintf(fp, "drvIpUnidig %s: connected at base address %p\n",
          this->portName, baseAddress_);
  if (details >= 1) {
    if (r.intEnableRegisterLow)    intEnableRegister =     *r.intEnableRegisterLow;
    if (r.intEnableRegisterHigh)   intEnableRegister |=   (*r.intEnableRegisterHigh << 16);
    if (r.intPolarityRegisterLow)  intPolarityRegister =   *r.intPolarityRegisterLow;
    if (r.intPolarityRegisterHigh) intPolarityRegister |= (*r.intPolarityRegisterHigh << 16);

    fprintf(fp, "  risingMask=%x\n", risingMask_);
    fprintf(fp, "  fallingMask=%x\n", fallingMask_);
    fprintf(fp, "  intEnableRegister=%x\n", intEnableRegister);
    fprintf(fp, "  intPolarityRegister=%x\n", intPolarityRegister);
    fprintf(fp, "  messages sent OK=%d; send failed (queue full)=%d\n",
            messagesSent_, messagesFailed_);
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






