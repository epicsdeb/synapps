/* drvIp1k125.cc

    Author: Marty Smith

    This is the driver for the Acromag IP1K125 series of reconfigurable
    digital I/O IP modules.

    Modifications:
    12-Aug-2008  MLS  Initial release based on the IpUnidig driver
    16-Nov-2009  TMM  Allow port to handle more than one address, access
                      IP module's mem space, add the following docs.

    This driver cooperates with specific FPGA firmware loaded into the
    Acromag IP-EP200 (and probably also the IP-1K125, for which the support
    was originally written).  The loaded FPGA firmware includes Eric Norum's
    IndustryPack Bridge, which is an interface between the IndustryPack bus
    and the Altera FPGA's Avalon bus.  The IndustryPack Bridge does not
    define anything we can write to in the FPGA.  It's job is to support
    additional firmware loaded into the FPGA.  The additional firmware
    defines registers that we can read and write, and it can take one of the
    two forms (sopc components) supported by this driver:

        1) fieldIO_registerSet component

           A set of seven 16-bit registers defined by 'fieldIO_registerSet'
           below.  This register set provides bit-level I/O direction and
           interrupt-generation support, and is intended primarily to
           implement field I/O registers.

        2) single 16-bit register component

           a single 16-bit register, which has no interrupt service or bit-level
           I/O direction.  This type of sopc component is just a plain 16-bit
           register, which can be written to or read.  This driver doesn't know
           or care what the register might be connected to inside the FPGA.

x The following is no longer true:
x    Currently, a fieldIO_registerSet must be located in the IndustryPack
x    module's IO space, because we believe there is a bug in the IP-EP200's
x    implementation that prevents us from reading from this module's MEM
x    space, and because there's no point to the a fieldIO_registerSet that
x    cannot be read.  The single register can be functional in either address
x    space, though if it's located in MEM space, we won't be able to read it.
    
    Each fieldIO_registerSet component must be initialized by a separate call to
    initIp1k125(), because the component's sopc address must be specified at
    init time, so that the interrupt-service routine associated with the
    component can use the sopc address.  Currently, each call to initIp1k125()
    defines a new asyn port, connects an interrupt-service routine, creates a
    message queue, and launches a thread.

    Single 16-bit register components need not have their sopc addresses known
    at init time, because they are not associated with an interrupt service
    routine.  As a consequence, many such single-register components can be
    served by a single asyn port.  Users of this port must specify the sopc
    address of the register they want to  read or write in their asynUser
    structure. Records do this by including the address in the definition of the
    record's OUT or INP field.  For example, the ADDR macro in the following
    field definition should be set to the register's sopc address:
    field(OUT,"@asynMask($(PORT),$(ADDR),0x2f)")

    The addressing of sopc components requires some explanation.  When a
    component is loaded into the FPGA, it is given an sopc address, which is a
    number in one of two regions of the Avalon address space.  These regions of
    Avalon memory space are mapped by the IndustryPack Bridge to specific ranges
    of the IndustryPack module's IO and MEM spaces.  The IO and MEM spaces, in
    turn, are mapped by the IndustryPack carrier, and by the ipac-module
    software, to corresponding ranges in a VME address space.  The lowest
    address in the IndustryPack module's IO space is mapped to the VME A16
    address given by ipmBaseAddr(carrier, slot, ipac_addrIO), which I'll call
    IOBASE in the following table.  The lowest address in the IndustryPack
    module's MEM space is mapped to the VME A32 address given by
    ipmBaseAddr(carrier, slot, ipac_addrMem), which I'll call MEMBASE in the
    following table.  (The module's MEM space could also have been mapped to the
    VME A24 space.  This code doesn't know or care, because it just gets the VME
    address by making a function call to code provided by the ipac module.)
    
    Note that IOBASE and MEMBASE depend on the IndustryPack carrier and slot
    into which the IP-EP200 module has been placed.

    Avalon_address | IP_space  IP_address  | VME_space   VME_address
    (sopc address) |                       |                 
    ---------------|-----------------------|-----------------------------
    0x000000       | IO        0x000000    | A16         IOBASE+0x000000     
    ...            | IO        ...         | A16         ...             
    0x00007f       | IO        0x00007f    | A16         IOBASE+0x00007f     
                   |                       |                                        
    0x800000       | MEM       0x000000    | A32         MEMBASE+0x000000
    ...            | MEM       ...         | A32         ...
    0xffffff       | MEM       0x7fffff    | A32         MEMBASE+0x7fffff
    
    Thus, if a component is created with the sopc address 0x000010, it can be
    accessed at the IO-space address 0x000010, which is mapped to the VME
    address IOBASE+0x000010.  If a component is created with the sopc address
    0x800003, it can be accessed at MEM-space address 0x000003, which is
    mapped to the VME address MEMBASE+0x000003.

    Note that users of this code are not expected to know anything about this
    address-mapping business.  The only address users ever specify is the sopc
    address, exactly as it was specified to Quartus.

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
#include <epicsInterrupt.h>

#define SIMULATE 0
#define APS_ID	0x53
#define CAN_READ_MEM_SPACE 1
#define MAX_MESSAGES 1000
#define MAX_PORTS 10

#define COMPONENTTYPE_FIELD_IO 0
#define COMPONENTTYPE_BARE_REG 1

typedef struct {
    epicsUInt16 controlRegister;    /* control register            */
    epicsUInt16 writeDataRegister;  /* 16-bit data write/read      */
    epicsUInt16 readDataRegister;   /* Input Data Read register    */
    epicsUInt16 risingIntStatus;    /* Rising Int Status Register  */
    epicsUInt16 risingIntEnable;    /* Rising Int Enable Reg       */
    epicsUInt16 fallingIntStatus;   /* Falling Int Status Register */
    epicsUInt16 fallingIntEnable;   /* Falling Int Enable Register */
} fieldIO_registerSet;

typedef struct {
    epicsUInt32 bits;
    epicsUInt32 interruptMask;
} interruptMessage;

typedef struct {
	ipac_idProm_t *id;			/* ID space */
	volatile epicsUInt16 *io;	/* IO space (mapped to Avalon-bus-address range 0x00 -- 0x7f) */
	volatile epicsUInt16 *mem;	/* MEM space (mapped to Avalon-bus-address range 0x800000 -- 0xffffff) */
	int is_fieldIO_registerSet;
    unsigned char manufacturer;
    unsigned char model;
    char *portName;
    asynUser *pasynUser;
    epicsUInt32 oldBits;
    epicsUInt32 risingMask;
    epicsUInt32 fallingMask;
    volatile fieldIO_registerSet *regs;
    double pollTime;
    epicsMessageQueueId msgQId;
    int messagesSent;
    int messagesFailed;
    asynInterface common;
    asynInterface uint32D;
    asynInterface int32;
    void *interruptPvt;
    int intVector;
} drvIp1k125Pvt;


/*
 * asynCommon interface
 */
static void report                 	(void *drvPvt, FILE *fp, int details);
static asynStatus connect          	(void *drvPvt, asynUser *pasynUser);
static asynStatus disconnect       	(void *drvPvt, asynUser *pasynUser);

static const struct asynCommon ip1k125Common = {
    report,
    connect,
    disconnect
};

/*
 * asynUInt32Digital interface - we only implement part of this interface.
 */
static asynStatus readUInt32D      	(void *drvPvt, asynUser *pasynUser,
                                    	epicsUInt32 *value, epicsUInt32 mask);
static asynStatus writeUInt32D     	(void *drvPvt, asynUser *pasynUser,
                                    	epicsUInt32 value, epicsUInt32 mask);

/* default implementations are provided by asynUInt32DigitalBase. */
static struct asynUInt32Digital ip1k125UInt32D = {
    writeUInt32D, /* write */
    readUInt32D,  /* read */
    NULL,         /* setInterrupt: default does nothing */
    NULL,         /* clearInterrupt: default does nothing */
    NULL,         /* getInterrupt: default does nothing */
    NULL,         /* registerInterruptUser: default adds user to pollerThread's clientList. */
    NULL          /* cancelInterruptUser: default removes user from pollerThread's clientList. */
};


/*
 * asynInt32 interface 
 */

static asynStatus writeInt32(void *drvPvt, asynUser *pasynUser, epicsInt32 value);
static asynStatus readInt32(void *drvPvt, asynUser *pasynUser, epicsInt32 *value);
static asynStatus getBounds(void *drvPvt, asynUser *pasynUser, epicsInt32 *low, epicsInt32 *high);

static struct asynInt32 ip1k125Int32 = {
    writeInt32, /* write */
    readInt32,  /* read */
	getBounds,	/* getBounds */
	NULL,		/* RegisterInterruptUser */
	NULL		/* cancelInterruptUser */
};

/* These are private functions */
static void pollerThread           	(drvIp1k125Pvt *pPvt);
static void intFunc                	(void *); /* Interrupt function */
static void rebootCallback         	(void *);



/* Initialize IP module */
int initIp1k125(const char *portName, ushort_t carrier, ushort_t slot,
				int msecPoll, int dataDir, int sopcOffset, int sopcVector,
				int risingMask, int fallingMask)
{
	drvIp1k125Pvt *pPvt;
	int status;
	char threadName[80] = "";

	pPvt = callocMustSucceed(1, sizeof(*pPvt), "initIp1k125");
	pPvt->portName = epicsStrDup(portName);
	pPvt->is_fieldIO_registerSet = 1;

	/* Default of 100 msec */
	if (msecPoll == 0) msecPoll = 100;
	pPvt->pollTime = msecPoll / 1000.;
	pPvt->msgQId = epicsMessageQueueCreate(MAX_MESSAGES, sizeof(interruptMessage));

	if (ipmCheck(carrier, slot)) {
		errlogPrintf("initIp1k125: bad carrier or slot\n");
		return(-1);
	}
	
	/* Set up ID and I/O space addresses for IP module */
	pPvt->id = (ipac_idProm_t *) ipmBaseAddr(carrier, slot, ipac_addrID);
	pPvt->io = (epicsUInt16 *) ipmBaseAddr(carrier, slot, ipac_addrIO);
	pPvt->mem = (epicsUInt16 *) ipmBaseAddr(carrier, slot, ipac_addrMem);
	printf("initIp1k125: ID:%p, IO:%p, MEM:%p\n", pPvt->id, pPvt->io, pPvt->mem);
	/* Get address of fieldIO_registerSet */
	if (sopcOffset & 0x800000) {
		/* The component is in the module's MEM space */
#if CAN_READ_MEM_SPACE
		pPvt->regs = (fieldIO_registerSet *) ((char *)(pPvt->mem) + (sopcOffset & 0x7fffff));
#else
		errlogPrintf("initIp1k125: Can't use MEM space for this component\n");
		return(-1);
#endif
	} else {
		/* The component is in the module's IO space */
		pPvt->regs = (fieldIO_registerSet *) ((char *)(pPvt->io) + sopcOffset);
	}

	pPvt->intVector = sopcVector;

	pPvt->manufacturer = pPvt->id->manufacturerId & 0xff;
	pPvt->model = pPvt->id->modelId & 0xff;

	/* Link with higher level routines */
	pPvt->common.interfaceType = asynCommonType;
	pPvt->common.pinterface  = (void *)&ip1k125Common;
	pPvt->common.drvPvt = pPvt;

	pPvt->uint32D.interfaceType = asynUInt32DigitalType;
	pPvt->uint32D.pinterface  = (void *)&ip1k125UInt32D;
	pPvt->uint32D.drvPvt = pPvt;

	pPvt->int32.interfaceType = asynInt32Type;
	pPvt->int32.pinterface  = (void *)&ip1k125Int32;
	pPvt->int32.drvPvt = pPvt;

	status = pasynManager->registerPort(pPvt->portName,
	                                    ASYN_MULTIDEVICE, /* multiDevice, cannot block */
	                                    1, /* autoconnect */
	                                    0, /* medium priority */
	                                    0);/* default stack size */
	if (status != asynSuccess) {
		errlogPrintf("initIp1k125 ERROR: Can't register port\n");
		return(-1);
	}
	status = pasynManager->registerInterface(pPvt->portName,&pPvt->common);
	if (status != asynSuccess) {
		errlogPrintf("initIp1k125 ERROR: Can't register common.\n");
		return(-1);
	}
	status = pasynUInt32DigitalBase->initialize(pPvt->portName, &pPvt->uint32D);
	if (status != asynSuccess) {
		errlogPrintf("initIp1k125 ERROR: Can't register UInt32Digital.\n");
		return(-1);
	}
	pasynManager->registerInterruptSource(pPvt->portName, &pPvt->uint32D,
	                                      &pPvt->interruptPvt);

	status = pasynInt32Base->initialize(pPvt->portName,&pPvt->int32);
	if (status != asynSuccess) {
		errlogPrintf("initIp1k125 ERROR: Can't register Int32.\n");
		return(-1);
	}

	/* Create asynUser for asynTrace */
	pPvt->pasynUser = pasynManager->createAsynUser(0, 0);
	pPvt->pasynUser->userPvt = pPvt;

	/* Connect to device */
	status = pasynManager->connectDevice(pPvt->pasynUser, pPvt->portName, 0);
	if (status != asynSuccess) {
		errlogPrintf("initIp1k125, connectDevice failed %s\n",
			pPvt->pasynUser->errorMessage);
		return(-1);
	}

	/* Set up the control register */
	pPvt->regs->controlRegister = dataDir;  /* Set Data Direction Reg IP1k125 */  
	pPvt->risingMask = risingMask;
	pPvt->fallingMask = fallingMask;
	
	/* Start the thread to poll and handle interrupt callbacks to device support */
	strcat(threadName, "ip1k125");
	strcat(threadName, portName);
	epicsThreadCreate(threadName, epicsThreadPriorityHigh,
		epicsThreadGetStackSize(epicsThreadStackBig),
		(EPICSTHREADFUNC)pollerThread, pPvt);

	/* Interrupt support
	 * If the risingMask and the fallingMask are zero, don't bother with interrupts, 
	 * since the user probably didn't pass this parameter to Ip1k125::init()
	 */
	if (pPvt->risingMask || pPvt->fallingMask) {
	
		pPvt->regs->risingIntStatus = risingMask;
		pPvt->regs->fallingIntStatus = fallingMask;
		
		/* Enable interrupt generation in FPGA firmware */
		if (pPvt->risingMask) { 
			pPvt->regs->risingIntEnable = pPvt->risingMask;
		}
		if (pPvt->fallingMask) {  
			pPvt->regs->fallingIntEnable = pPvt->fallingMask;
		}

		/* Associate interrupt service routine with intVector */
		if (devConnectInterruptVME(pPvt->intVector, intFunc, (void *)pPvt)) {
			errlogPrintf("ip1k125 interrupt connect failure\n");
			return(-1);
		}
		/* Enable IPAC module interrupts and set module status. */
		ipmIrqCmd(carrier, slot, 0, ipac_irqEnable);
		ipmIrqCmd(carrier, slot, 0, ipac_statActive);
	}
	epicsAtExit(rebootCallback, pPvt);
	return(0);
}

/* Initialize IP module */
int initIp1k125SingleRegisterPort(const char *portName, ushort_t carrier, ushort_t slot)
{
	drvIp1k125Pvt *pPvt;
	int status;

	pPvt = callocMustSucceed(1, sizeof(*pPvt), "drvIp1k125Pvt");
	pPvt->portName = epicsStrDup(portName);
	pPvt->is_fieldIO_registerSet = 0;

#if !SIMULATE
	if (ipmCheck(carrier, slot)) {
		errlogPrintf("initIp1k125SingleRegisterPort: bad carrier or slot\n");
		return(-1);
	}
#endif
	/* Set up ID and I/O space addresses for IP module */
	pPvt->id = (ipac_idProm_t *) ipmBaseAddr(carrier, slot, ipac_addrID);
	pPvt->io = (epicsUInt16 *) ipmBaseAddr(carrier, slot, ipac_addrIO);
	pPvt->mem = (epicsUInt16 *) ipmBaseAddr(carrier, slot, ipac_addrMem);

	pPvt->manufacturer = pPvt->id->manufacturerId & 0xff;
	pPvt->model = pPvt->id->modelId & 0xff;

	/* Link with higher level routines */
	pPvt->common.interfaceType = asynCommonType;
	pPvt->common.pinterface  = (void *)&ip1k125Common;
	pPvt->common.drvPvt = pPvt;

	pPvt->uint32D.interfaceType = asynUInt32DigitalType;
	pPvt->uint32D.pinterface  = (void *)&ip1k125UInt32D;
	pPvt->uint32D.drvPvt = pPvt;

	pPvt->int32.interfaceType = asynInt32Type;
	pPvt->int32.pinterface  = (void *)&ip1k125Int32;
	pPvt->int32.drvPvt = pPvt;

	status = pasynManager->registerPort(pPvt->portName,
	                                    ASYN_MULTIDEVICE, /* multiDevice, cannot block */
	                                    1, /* autoconnect */
	                                    0, /* medium priority */
	                                    0);/* default stack size */
	if (status != asynSuccess) {
		errlogPrintf("initIp1k125SingleRegisterPort ERROR: Can't register port\n");
		return(-1);
	}
	status = pasynManager->registerInterface(pPvt->portName,&pPvt->common);
	if (status != asynSuccess) {
		errlogPrintf("initIp1k125SingleRegisterPort ERROR: Can't register common.\n");
		return(-1);
	}
	status = pasynUInt32DigitalBase->initialize(pPvt->portName, &pPvt->uint32D);
	if (status != asynSuccess) {
		errlogPrintf("initIp1k125SingleRegisterPort ERROR: Can't register UInt32Digital.\n");
		return(-1);
	}
	pasynManager->registerInterruptSource(pPvt->portName, &pPvt->uint32D,
	                                      &pPvt->interruptPvt);

	status = pasynInt32Base->initialize(pPvt->portName,&pPvt->int32);
	if (status != asynSuccess) {
		errlogPrintf("initIp1k125SingleRegisterPort ERROR: Can't register Int32.\n");
		return(-1);
	}

	/* Create an asynUser that we can use when we want to call asynPrint, but don't
	 * have any client's asynUser to supply as an argument.
	 */
	pPvt->pasynUser = pasynManager->createAsynUser(0, 0);
	pPvt->pasynUser->userPvt = pPvt;

	/* Connect our asynUser to device */
	status = pasynManager->connectDevice(pPvt->pasynUser, pPvt->portName, 0);
	if (status != asynSuccess) {
		errlogPrintf("initIp1k125SingleRegisterPort, connectDevice failed %s\n",
			pPvt->pasynUser->errorMessage);
		return(-1);
	}
	return(0);
}

/*
 * calcRegisterAddress - For single-register components, the sopc address is
 * specified in the EPICS record's INP or OUT field, and we need to translate
 * that into a VME address.  Part of the job was done by
 * initIp1k125SingleRegisterPort(), which got the VME base addresses of the
 * IndustryPack module's IO and MEM spaces.
 * See "The addressing of sopc components", in comments at the top of this file
 * for a complete description of how addresses are handled.
 */
static epicsUInt16 *calcRegisterAddress(void *drvPvt, asynUser *pasynUser)
{
	drvIp1k125Pvt *pPvt = (drvIp1k125Pvt *)drvPvt;
	int addr;
	epicsUInt16 *reg;

	pasynManager->getAddr(pasynUser, &addr);
	if (addr & 0x800000) {
		addr &= 0x7fffff;	/* mask the sopc MEM-space indicator bit */
		addr >>= 1;			/* convert byte-address offset to (two-byte) word-address offset */
		reg = (epicsUInt16 *) (pPvt->mem+addr);
	} else {
		addr = addr >> 1;
		reg = (epicsUInt16 *) (pPvt->io+addr);
	}
	return(reg);
}

/*
 * readUInt32D
 * This method provides masked read access to the readDataRegister of a
 * fieldIO_registerSet component, or to the sopc address of a single register
 * component.  
 * Note that fieldIO_registerSet components have a control register that
 * may determine what data will be available for reading.
 */
static asynStatus readUInt32D(void *drvPvt, asynUser *pasynUser,
	epicsUInt32 *value, epicsUInt32 mask)
{
	drvIp1k125Pvt *pPvt = (drvIp1k125Pvt *)drvPvt;
	volatile epicsUInt16 *reg;

	*value = 0;
#if SIMULATE
	*value = 1234;
#else
	if (pPvt->is_fieldIO_registerSet) {
		*value = (epicsUInt32) (pPvt->regs->readDataRegister & mask);
	} else {
		reg = calcRegisterAddress(drvPvt, pasynUser);
		*value = (epicsUInt32) (*reg & mask);
	}
#endif
	asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
		"drvIp1k125::readUInt32D, value=0x%X, mask=0x%X\n", *value,mask);
	return(asynSuccess);
}

/*
 * writeUInt32D
 * This method provides bit level write access to the writeDataRegister of a
 * fieldIO_registerSet component, or to the sopc address of a single register
 * component.  Bits of 'value' that correspond to zero bits of 'mask' will
 * be ignored -- corresponding bits of the destination register will be left
 * as they were before the write operation.  
 * Note that fieldIO_registerSet components have a control register that
 * may determine what use will be made of the data we write.
 */
static asynStatus writeUInt32D(void *drvPvt, asynUser *pasynUser, epicsUInt32 value,
	epicsUInt32 mask)
{
	drvIp1k125Pvt *pPvt = (drvIp1k125Pvt *)drvPvt;
	volatile epicsUInt16 *reg=0;

#if SIMULATE
#else
	if (pPvt->is_fieldIO_registerSet) {
		pPvt->regs->writeDataRegister = (pPvt->regs->writeDataRegister & ~mask) | (value & mask);	
	} else {
		int addr;
		pasynManager->getAddr(pasynUser, &addr);
		reg = calcRegisterAddress(drvPvt, pasynUser);
		if (addr & 0x800000) {
#if CAN_READ_MEM_SPACE
			*reg = (*reg & ~mask) | (epicsUInt16) (value & mask);
#else
			*reg = (epicsUInt16) (value & mask);
#endif
		} else {
			*reg = (*reg & ~mask) | (epicsUInt16) (value & mask);
		}
	}
#endif

	asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
		"drvIp1k125::writeUInt32D, addr=0x%X, value=0x%X, mask=0x%X\n", reg, value, mask);
	return(asynSuccess);
}

/*
 * readInt32
 * This method reads from the readDataRegister of a fieldIO_registerSet
 * component, or from the sopc address of a single register component.  
 * Note that fieldIO_registerSet components have a control register that
 * may determine what data will be available for reading.
 */
static asynStatus readInt32(void *drvPvt, asynUser *pasynUser, epicsInt32 *value)
{
	drvIp1k125Pvt *pPvt = (drvIp1k125Pvt *)drvPvt;
	volatile epicsUInt16 *reg;

	*value = 0;

	if (pPvt->is_fieldIO_registerSet) {
		*value = (epicsUInt32) pPvt->regs->readDataRegister;
	} else {
		reg = calcRegisterAddress(drvPvt, pasynUser);
		*value = (epicsUInt32)(*reg);
	}

	asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
		"drvIp1k125::readInt32, value=0x%X\n", *value);
	return(asynSuccess);
}

/* writeInt32
 * This method writes to the writeDataRegister of a fieldIO_registerSet
 * component, or to the sopc address of a single register component.  
 * Note that fieldIO_registerSet components have a control register that
 * may determine what use will be made of the data we write.
 */
static asynStatus writeInt32(void *drvPvt, asynUser *pasynUser, epicsInt32 value)
{
	drvIp1k125Pvt *pPvt = (drvIp1k125Pvt *)drvPvt;
	volatile epicsUInt16 *reg;

	if (pPvt->is_fieldIO_registerSet) {
		pPvt->regs->writeDataRegister = (epicsUInt16) value;	
	} else {
		reg = calcRegisterAddress(drvPvt, pasynUser);
		*reg = (epicsUInt16) value;
	}

	asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
		"drvIp1k125::writeInt32, value=0x%X\n",value);
	return(asynSuccess);
}

static asynStatus getBounds(void *drvPvt, asynUser *pasynUser, epicsInt32 *low, epicsInt32 *high)
{
	const char *portName;
	asynStatus status;
	int        addr;

	status = pasynManager->getPortName(pasynUser,&portName);
	if (status!=asynSuccess) return status;
	status = pasynManager->getAddr(pasynUser,&addr);
	if (status!=asynSuccess) return status;
	*low = 0;
	*high = 0xffff;
	asynPrint(pasynUser,ASYN_TRACE_FLOW,
		"%s %d getBounds setting low=high=0\n",portName,addr);
	return(asynSuccess);
}


/*
 * This is the interrupt-service routine associated with the interrupt
 * vector pPvt->intVector supplied in the drvPvt structure.
 */
static void intFunc(void *drvPvt)
{
	drvIp1k125Pvt *pPvt = (drvIp1k125Pvt *)drvPvt;
	epicsUInt32 inputs=0, pendingLow, pendingHigh, pendingMask;
	interruptMessage msg;

	/* Make sure interrupt is from this hardware otherwise just leave */
	if (pPvt->regs->risingIntStatus || pPvt->regs->fallingIntStatus) {
		pendingLow = pPvt->regs->fallingIntStatus;
		pendingHigh = pPvt->regs->risingIntStatus;
		pendingMask = pendingLow | pendingHigh;

		/* Read the current input */
		inputs = (epicsUInt16) pPvt->regs->readDataRegister;
		msg.bits = inputs;
		msg.interruptMask = pendingMask;

		if (epicsMessageQueueTrySend(pPvt->msgQId, &msg, sizeof(msg)) == 0)
			pPvt->messagesSent++;
		else
			pPvt->messagesFailed++;
		/* Clear the interrupts */
		pPvt->regs->risingIntStatus = pendingHigh;
		pPvt->regs->fallingIntStatus = pendingLow;
		
		/* Generate dummy read cycle for PPC */
		pendingHigh = pPvt->regs->risingIntStatus;
	}
}


/* This function runs in a separate thread.  It waits for the poll
 * time, or an interrupt, whichever comes first.  If the bits read from
 * the ip1k125 have changed then it does callbacks to all clients that
 * have registered with registerInterruptUser
 */

static void pollerThread(drvIp1k125Pvt *pPvt)
{
	epicsUInt32 newBits, changedBits, interruptMask=0;
	interruptMessage msg;
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
			 * set in the interrupt routine
			 */
			readUInt32D(pPvt, pPvt->pasynUser, &newBits, 0xffff);
		} else {
			newBits = msg.bits;
			interruptMask = msg.interruptMask;
			asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW,
				"drvIp1k125::pollerThread, got interrupt\n");
		}
		asynPrint(pPvt->pasynUser, ASYN_TRACEIO_DRIVER,
			"drvIp1k125::pollerThread, bits=%x\n", newBits);

		/* We detect change both from interruptMask (which only works for
		 * hardware interrupts) and changedBits, which works for polling */
		changedBits = newBits ^ pPvt->oldBits;
		interruptMask = interruptMask | changedBits;
		if (interruptMask) {
			pPvt->oldBits = newBits;
			pasynManager->interruptStart(pPvt->interruptPvt, &pclientList);
			pnode = (interruptNode *)ellFirst(pclientList);
			while (pnode) {
				pUInt32D = pnode->drvPvt;
				if (pUInt32D->mask & interruptMask) {
					asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW,
						"drvIp1k125::pollerThread, calling client %p"
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


static void rebootCallback(void *drvPvt)
{
	drvIp1k125Pvt *pPvt = (drvIp1k125Pvt *)drvPvt;
	int key = epicsInterruptLock();

	/* Disable interrupts first so no interrupts during reboot */
	if (pPvt->regs) {
		pPvt->regs->risingIntEnable = 0;
		pPvt->regs->fallingIntEnable = 0;
	}
	epicsInterruptUnlock(key);
}

/* asynCommon routines */

/* Report  parameters */
static void report(void *drvPvt, FILE *fp, int details)
{
	drvIp1k125Pvt *pPvt = (drvIp1k125Pvt *)drvPvt;
	interruptNode *pnode;
	ELLLIST *pclientList;

	fprintf(fp, "drvIp1k125 %s: connected at base address %p\n",
		pPvt->portName, pPvt->regs);
	if (details >= 1) {
		fprintf(fp, "  controlRegister=%x\n", pPvt->regs->controlRegister);
		fprintf(fp, "  risingMask=%x\n", pPvt->risingMask);
		fprintf(fp, "  risingIntEnable=%x\n", pPvt->regs->risingIntEnable);
		fprintf(fp, "  fallingMask=%x\n", pPvt->fallingMask);
		fprintf(fp, "  fallingIntEnable=%x\n", pPvt->regs->fallingIntEnable);
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
static const iocshArg initArg4 = { "Data Dir Reg",iocshArgInt};
static const iocshArg initArg5 = { "sopc Offset Addr",iocshArgInt};
static const iocshArg initArg6 = { "sopc Vector Addr",iocshArgInt};
static const iocshArg initArg7 = { "Rising Edge Mask",iocshArgInt};
static const iocshArg initArg8 = { "Falling Edge Mask",iocshArgInt};
static const iocshArg * const initArgs[9] = {&initArg0,
                                             &initArg1,
                                             &initArg2,
                                             &initArg3,
                                             &initArg4,
                                             &initArg5,
                                             &initArg6,
                                             &initArg7,
                                             &initArg8};
static const iocshFuncDef initFuncDef = {"initIp1k125",9,initArgs};
static void initCallFunc(const iocshArgBuf *args)
{
	initIp1k125(args[0].sval, args[1].ival, args[2].ival,
	            args[3].ival, args[4].ival, args[5].ival,
	            args[6].ival, args[7].ival, args[8].ival);
}
void ip1k125Register(void)
{
	iocshRegister(&initFuncDef,initCallFunc);
}

epicsExportRegistrar(ip1k125Register);

