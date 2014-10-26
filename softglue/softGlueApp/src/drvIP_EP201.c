/* drvIP_EP201.cc

    Original Author: Marty Smith (as drvIp1k125.c)
    Modified: Tim Mooney - comments, added single-register component, added
        ability to program FPGA over the IP bus, renamed.


    This is the driver for the Acromag IP_EP20x series of reconfigurable
    digital I/O IP modules.

    Modifications:
    12-Aug-2008  MLS  Initial release based on the IpUnidig driver
    16-Nov-2009  TMM  Allow port to handle more than one address, access
                      IP module's mem space, add the following docs.
                      Other changes described in subversion commit messages:
                      https://subversion.xor.aps.anl.gov/synApps/softGlue/

    This driver cooperates with specific FPGA firmware loaded into the Acromag
    IP-EP201 (and other IP-EP200-series modules).  The loaded FPGA
    firmware includes Eric Norum's IndustryPack Bridge, which is an interface
    between the IndustryPack bus and the Altera FPGA's Avalon bus.  The
    IndustryPack Bridge does not define anything we can write to in the FPGA. 
    It's job is to support additional firmware loaded into the FPGA.  The
    additional firmware defines registers that we can read and write, and it can
    take one of the two forms (sopc components) supported by this driver:

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

    Each fieldIO_registerSet component must be initialized by a separate call to
    initIP_EP201(), because the component's sopc address must be specified at
    init time, so that the interrupt-service routine associated with the
    component can use the sopc address.  Currently, each call to initIP_EP201()
    defines a new asyn port, connects an interrupt-service routine, creates a
    message queue, and launches a thread.

    Single 16-bit register components need not have their sopc addresses known
    at init time, because they are not associated with an interrupt service
    routine.  As a consequence, many such single-register components can be
    served by a single asyn port.  Users of this port must specify the sopc
    address of the register they want to read or write in their asynUser
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
#include <ctype.h>

#ifdef vxWorks
extern int logMsg(char *fmt, ...);
#endif

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
#include <asynDrvUser.h> /* used for setting interrupt enable to rising, falling, or both */
#include <asynUInt32Digital.h>
#include <asynInt32.h>
#include <epicsInterrupt.h>
#include "drvIP_EP201.h"

#define STATIC static
/* #define STATIC */

volatile int drvIP_EP201Debug = 0;

#define DO_IPMODULE_CHECK 1
#define APS_ID	0x53
#define MAX_MESSAGES 1000
#define MAX_PORTS 10
#define MAX_IRQ 5	/* max number of outstanding interrupt requests */


#define COMPONENTTYPE_FIELD_IO 0
#define COMPONENTTYPE_BARE_REG 1

/* drvParams */
#define INTERRUPT_EDGE			1	/* drvParam INTEDGE */
#define POLL_TIME				2	/* drvParam POLLTIME */
#define INTERRUPT_EDGE_RESET	3	/* drvParam INT_EDGE_RESET */

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
    epicsUInt16 bits;
    epicsUInt16 interruptMask;
} interruptMessage;

typedef struct {
	ipac_idProm_t *id;			/* ID space */
	volatile epicsUInt16 *io;	/* IO space (mapped to Avalon-bus-address range 0x00 -- 0x7f) */
	volatile epicsUInt16 *mem;	/* MEM space (mapped to Avalon-bus-address range 0x800000 -- 0xffffff) */
	ushort carrier;
	ushort slot;
	int sopcAddress;
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
	asynInterface asynDrvUser;
    asynInterface uint32D;
    asynInterface int32;
    void *interruptPvt;
    int intVector;
	epicsUInt32 interruptCount;
    epicsUInt16 disabledIntMask;    /* int enable rescinded because too many interrupts received */
} drvIP_EP201Pvt;

/*
 * Pointers to up to 12 drvIP_EP201Pvt structures -- enough for four copies of softGlue.
 * This is needed to break up init into three function calls, all of which specify carrier and slot.
 * From carrier and slot, we can get the three drvIP_EP201Pvt pointers associated with an IP_EP200 module.
 */
#define MAX_DRVPVT 12
drvIP_EP201Pvt *driverTable[MAX_DRVPVT] = {0};

/*
 * asynCommon interface
 */
STATIC void report                 	(void *drvPvt, FILE *fp, int details);
STATIC asynStatus connect          	(void *drvPvt, asynUser *pasynUser);
STATIC asynStatus disconnect       	(void *drvPvt, asynUser *pasynUser);

STATIC const struct asynCommon IP_EP201Common = {
    report,
    connect,
    disconnect
};

/*
 * asynDrvUser interface
 */
STATIC asynStatus create_asynDrvUser(void *drvPvt,asynUser *pasynUser,
    const char *drvInfo, const char **pptypeName,size_t *psize);
STATIC asynStatus getType_asynDrvUser(void *drvPvt,asynUser *pasynUser,
    const char **pptypeName,size_t *psize);
STATIC asynStatus destroy_asynDrvUser(void *drvPvt,asynUser *pasynUser);
STATIC asynDrvUser drvUser = {create_asynDrvUser, getType_asynDrvUser, destroy_asynDrvUser};

/*
 * asynUInt32Digital interface - we only implement part of this interface.
 */
STATIC asynStatus readUInt32D(void *drvPvt, asynUser *pasynUser, epicsUInt32 *value, epicsUInt32 mask);
STATIC asynStatus writeUInt32D(void *drvPvt, asynUser *pasynUser, epicsUInt32 value, epicsUInt32 mask);
STATIC asynStatus setInterruptUInt32D(void *drvPvt, asynUser *pasynUser, epicsUInt32 mask, interruptReason reason);
STATIC asynStatus clearInterruptUInt32D(void *drvPvt, asynUser *pasynUser, epicsUInt32 mask);


/* default implementations are provided by asynUInt32DigitalBase. */
STATIC struct asynUInt32Digital IP_EP201UInt32D = {
    writeUInt32D, /* write */
    readUInt32D,  /* read */
    setInterruptUInt32D,	/* setInterrupt (not used) */
    clearInterruptUInt32D,	/* clearInterrupt (not used) */
    NULL,         /* getInterrupt: default does nothing */
    NULL,         /* registerInterruptUser: default adds user to pollerThread's clientList. */
    NULL          /* cancelInterruptUser: default removes user from pollerThread's clientList. */
};


/*
 * asynInt32 interface 
 */

STATIC asynStatus writeInt32(void *drvPvt, asynUser *pasynUser, epicsInt32 value);
STATIC asynStatus readInt32(void *drvPvt, asynUser *pasynUser, epicsInt32 *value);
STATIC asynStatus getBounds(void *drvPvt, asynUser *pasynUser, epicsInt32 *low, epicsInt32 *high);

STATIC struct asynInt32 IP_EP201Int32 = {
    writeInt32, /* write */
    readInt32,  /* read */
	getBounds,	/* getBounds */
	NULL,		/* RegisterInterruptUser */
	NULL		/* cancelInterruptUser */
};

/* These are private functions */
STATIC void pollerThread           	(drvIP_EP201Pvt *pPvt);
STATIC void intFunc                	(void *); /* Interrupt function */
STATIC void rebootCallback         	(void *);


int init_one_IP_EP200(const char *portName, epicsUInt16 carrier, epicsUInt16 slot, int sopcAddress);

/* Initialize IP module */
int initIP_EP200(epicsUInt16 carrier, epicsUInt16 slot, char *portName1,
	char *portName2, char *portName3, int sopcBase) {
	int retval;

	retval = init_one_IP_EP200(portName1, carrier, slot, sopcBase);
	if (retval) return(retval);
	retval = init_one_IP_EP200(portName2, carrier, slot, sopcBase+0x10);
	if (retval) return(retval);
	retval = init_one_IP_EP200(portName3, carrier, slot, sopcBase+0x20);
	return(retval);
}

int initIP_EP200_Int(epicsUInt16 carrier, epicsUInt16 slot, int intVectorBase,
	int risingMaskMS, int risingMaskLS, int fallingMaskMS, int fallingMaskLS) {

	int i, j;
	drvIP_EP201Pvt *pPvt;

	/* Go through driverTable for all drvIP_EP201Pvt pointers assoc with this carrier/slot. */
	for (i=0, j=0; i<MAX_DRVPVT; i++) {
		pPvt = driverTable[i];
		if (pPvt && (pPvt->carrier == carrier) && (pPvt->slot == slot)) {
			/* Interrupt support
			 * If risingMask, fallingMask, and intVector are zero, don't bother with interrupts.
			 */
			pPvt->intVector = intVectorBase + j;
			pPvt->risingMask = 0xff & ((j>1) ? risingMaskMS : risingMaskLS>>(j*16));
			pPvt->fallingMask = 0xff & ((j>1) ? fallingMaskMS : fallingMaskLS>>(j*16));

			if (pPvt->intVector || pPvt->risingMask || pPvt->fallingMask) {

				pPvt->regs->risingIntStatus = pPvt->risingMask;
				pPvt->regs->fallingIntStatus = pPvt->fallingMask;
		
				/* Enable interrupt generation in FPGA firmware */
				if (pPvt->risingMask) pPvt->regs->risingIntEnable = pPvt->risingMask;
				if (pPvt->fallingMask) pPvt->regs->fallingIntEnable = pPvt->fallingMask;

				/* Associate interrupt service routine with intVector */
				if (devConnectInterruptVME(pPvt->intVector, intFunc, (void *)pPvt)) {
					printf("initIP_EP200_Int: interrupt connect failure\n");
					return(-1);
				}
				/* Enable IPAC module interrupts and set module status. */
				ipmIrqCmd(carrier, slot, 0, ipac_irqEnable);
				ipmIrqCmd(carrier, slot, 0, ipac_statActive);
			}
			j++;
		}
	}
	return(0);
}

/*
 *    -------------------------------------------------------------------
 *    |  Correspondence between dataDir bits (0-8) and I/O pins (1-48)  |
 *    -------------------------------------------------------------------
 *    |             |  IP_EP201     |  IP_EP202/204      |  IP_EP203    |
 *    -------------------------------------------------------------------
 *    | bit 0       |  pins 1-8     |  pins 1, 3,25,27   |  pins 25,27  |
 *    | bit 1       |  pins 9-16    |  pins 5, 7,29,31   |  pins 29,31  |
 *    | bit 2       |  pins 17-24   |  pins 9,11,33,35   |  pins 33,35  |
 *    | bit 3       |  pins 25-32   |  pins 13,15,37,39  |  pins 37,39  |
 *    |             |               |                    |              |
 *    | bit 4       |  pins 33-40   |  pins 17,19,41,43  |  pins 41,43  |
 *    | bit 5       |  pins 41-48   |  pins 21,23,45,47  |  pins 45,47  |
 *    | bit 6       |         x     |            x       |  pins 1-8    |
 *    | bit 7       |         x     |            x       |  pins 9-16   |
 *    |             |               |                    |              |
 *    | bit 8       |         x     |            x       |  pins 17-24  |
 *    -------------------------------------------------------------------
 *
 *  In the FPGA, the control registers implement field I/O direction as follows:
 *
 *  control_x[10] specify that the upper eight bits of component 'x' are differential.  
 *  control_x[9] specify that the lower eight bits of component 'x' are differential.
 *  control_0[6..1] specify the data direction for differential signals of all three components.
 *     (These are bits [5..0] from the "Correspondence" table above.)
 *  control_x[8] specify the data direction for the upper eight single-ended signals of component 'x'.
 *  control_x[0] specify the data direction for the lower eight single-ended signals of component 'x'.
 */

int initIP_EP200_IO(epicsUInt16 carrier, epicsUInt16 slot, epicsUInt16 moduleType, epicsUInt16 dataDir) {
	int i, registerNum;
	drvIP_EP201Pvt *pPvt;

	if ((moduleType<201) || (moduleType>204)) {
		printf("initIP_EP200_IO: unrecognized moduleType %d\n", moduleType);
		return(-1);
	}
	if (drvIP_EP201Debug > 2) printf("initIP_EP200_IO: dataDir = 0x%x\n", dataDir);
	/*
	 * Go through driverTable for all drvIP_EP201Pvt pointers assoc with this carrier/slot,
	 * and initialize the control registers of all three field I/O register set components.
	 */
	for (i=0, registerNum=0; i<MAX_DRVPVT; i++) {
		pPvt = driverTable[i];
		if (pPvt && (pPvt->carrier == carrier) && (pPvt->slot == slot)) {
			pPvt->regs->controlRegister = 0; /* initialize */
			switch (moduleType) {
			case 201:
				/* all single ended signals */
				if (registerNum==0) {
					if (dataDir & 0x1) pPvt->regs->controlRegister |= 1;
					if (dataDir & 0x2) pPvt->regs->controlRegister |= 0x100;
				} else if (registerNum==1) {
					if (dataDir &  0x4) pPvt->regs->controlRegister |= 1;
					if (dataDir &  0x8) pPvt->regs->controlRegister |= 0x100;
				} else if (registerNum==2) {
					if (dataDir &  0x10) pPvt->regs->controlRegister |= 1;
					if (dataDir &  0x20) pPvt->regs->controlRegister |= 0x100;
				}
				break;
			case 202: case 204:
				/* all differential signals */
				pPvt->regs->controlRegister |= 0x600;  /* both 8-bit registers control differential signals */
				pPvt->regs->controlRegister |= (dataDir & 0x3f) << 1;
				break;
			case 203:
				/* The first 16-bit register, and the lower eight bits of the second 16-bit register
				 * are single ended.  The rest are differential.
				 */
				if (registerNum==0) {
					pPvt->regs->controlRegister |= (dataDir & 0x3f) << 1;
					if (dataDir & 0x40) pPvt->regs->controlRegister |= 1;
					if (dataDir & 0x80) pPvt->regs->controlRegister |= 0x100;
				}
				if (registerNum==1) {
					pPvt->regs->controlRegister |= 0x400;  /* upper 8-bit register controls differential signals */
					if (dataDir & 0x100) pPvt->regs->controlRegister |= 1;
				}
				if (registerNum==2) {
					pPvt->regs->controlRegister |= 0x600;  /* both 8-bit registers control differential signals */
				}
				break;
			}
			registerNum++;
		}
	}
	return(0);
}


int init_one_IP_EP200(const char *portName, epicsUInt16 carrier, epicsUInt16 slot, int sopcAddress)
{
	drvIP_EP201Pvt *pPvt;
	int i, status;
	char threadName[80] = "";

	pPvt = callocMustSucceed(1, sizeof(*pPvt), "initIP_EP200");

	/* save a pointer in the driver table, so we can get them from carrier and slot */
	for (i=0; i<MAX_DRVPVT; i++) {
		if (driverTable[i] == NULL) break;
	}
	if (i<MAX_DRVPVT) {
		driverTable[i] = pPvt;
	} else {
		printf("initIP_EP200: no room in driver table\n");
		return(-1);
	}

	pPvt->portName = epicsStrDup(portName);
	pPvt->is_fieldIO_registerSet = 1;

	pPvt->pollTime = 0.1; /* default for now */
	pPvt->msgQId = epicsMessageQueueCreate(MAX_MESSAGES, sizeof(interruptMessage));

#if DO_IPMODULE_CHECK
	if (ipmCheck(carrier, slot)) {
		printf("initIP_EP200: bad carrier or slot\n");
		return(-1);
	}
#endif

	/* Set up ID and I/O space addresses for IP module */
	pPvt->carrier = carrier;
	pPvt->slot = slot;
	pPvt->sopcAddress = sopcAddress;
	pPvt->id = (ipac_idProm_t *) ipmBaseAddr(carrier, slot, ipac_addrID);
	pPvt->io = (epicsUInt16 *) ipmBaseAddr(carrier, slot, ipac_addrIO);
	pPvt->mem = (epicsUInt16 *) ipmBaseAddr(carrier, slot, ipac_addrMem);
	printf("initIP_EP200: ID:%p, IO:%p, MEM:%p\n", pPvt->id, pPvt->io, pPvt->mem);
	/* Get address of fieldIO_registerSet */
	if (sopcAddress & 0x800000) {
		/* The component is in the module's MEM space */
		pPvt->regs = (fieldIO_registerSet *) ((char *)(pPvt->mem) + (sopcAddress & 0x7fffff));
	} else {
		/* The component is in the module's IO space */
		pPvt->regs = (fieldIO_registerSet *) ((char *)(pPvt->io) + sopcAddress);
	}

	pPvt->manufacturer = pPvt->id->manufacturerId & 0xff;
	pPvt->model = pPvt->id->modelId & 0xff;

	/* Link with higher level routines */
	pPvt->common.interfaceType = asynCommonType;
	pPvt->common.pinterface  = (void *)&IP_EP201Common;
	pPvt->common.drvPvt = pPvt;

	pPvt->asynDrvUser.interfaceType = asynDrvUserType;
	pPvt->asynDrvUser.pinterface = (void *)&drvUser;
	pPvt->asynDrvUser.drvPvt = pPvt;

	pPvt->uint32D.interfaceType = asynUInt32DigitalType;
	pPvt->uint32D.pinterface  = (void *)&IP_EP201UInt32D;
	pPvt->uint32D.drvPvt = pPvt;

	pPvt->int32.interfaceType = asynInt32Type;
	pPvt->int32.pinterface  = (void *)&IP_EP201Int32;
	pPvt->int32.drvPvt = pPvt;

	status = pasynManager->registerPort(pPvt->portName,
	                                    ASYN_MULTIDEVICE, /* multiDevice, cannot block */
	                                    1, /* autoconnect */
	                                    0, /* medium priority */
	                                    0);/* default stack size */
	if (status != asynSuccess) {
		printf("initIP_EP200 ERROR: Can't register port\n");
		return(-1);
	}
	status = pasynManager->registerInterface(pPvt->portName,&pPvt->common);
	if (status != asynSuccess) {
		printf("initIP_EP200 ERROR: Can't register common.\n");
		return(-1);
	}
	status = pasynManager->registerInterface(pPvt->portName,&pPvt->asynDrvUser);
	if (status != asynSuccess){
		printf("initIP_EP200 ERROR: Can't register asynDrvUser.\n");
		return(-1);
	}
	status = pasynUInt32DigitalBase->initialize(pPvt->portName, &pPvt->uint32D);
	if (status != asynSuccess) {
		printf("initIP_EP200 ERROR: Can't register UInt32Digital.\n");
		return(-1);
	}
	pasynManager->registerInterruptSource(pPvt->portName, &pPvt->uint32D,
	                                      &pPvt->interruptPvt);

	status = pasynInt32Base->initialize(pPvt->portName,&pPvt->int32);
	if (status != asynSuccess) {
		printf("initIP_EP200 ERROR: Can't register Int32.\n");
		return(-1);
	}

	/* Create asynUser for asynTrace */
	pPvt->pasynUser = pasynManager->createAsynUser(0, 0);
	pPvt->pasynUser->userPvt = pPvt;

	/* Connect to device */
	status = pasynManager->connectDevice(pPvt->pasynUser, pPvt->portName, 0);
	if (status != asynSuccess) {
		printf("initIP_EP200, connectDevice failed %s\n",
			pPvt->pasynUser->errorMessage);
		return(-1);
	}

	/* Start the thread to poll and handle interrupt callbacks to device support */
	strcat(threadName, "IP_EP200");
	strcat(threadName, portName);
	epicsThreadCreate(threadName, epicsThreadPriorityHigh,
		epicsThreadGetStackSize(epicsThreadStackBig),
		(EPICSTHREADFUNC)pollerThread, pPvt);
	epicsAtExit(rebootCallback, pPvt);
	return(0);
}

/* For backward compatibility, initIP_EP201().*/
int initIP_EP201(const char *portName, epicsUInt16 carrier, epicsUInt16 slot,
	int msecPoll, int dataDir, int sopcAddress, int interruptVector,
	int risingMask, int fallingMask) {

	drvIP_EP201Pvt *pPvt;
	int status;
	char threadName[80] = "";

	pPvt = callocMustSucceed(1, sizeof(*pPvt), "initIP_EP201");
	pPvt->portName = epicsStrDup(portName);
	pPvt->is_fieldIO_registerSet = 1;

	/* Default of 100 msec */
	if (msecPoll == 0) msecPoll = 100;
	pPvt->pollTime = msecPoll / 1000.;
	pPvt->msgQId = epicsMessageQueueCreate(MAX_MESSAGES, sizeof(interruptMessage));

#if DO_IPMODULE_CHECK
	if (ipmCheck(carrier, slot)) {
		printf("initIP_EP201: bad carrier or slot\n");
		return(-1);
	}
#endif

	/* Set up ID and I/O space addresses for IP module */
	pPvt->id = (ipac_idProm_t *) ipmBaseAddr(carrier, slot, ipac_addrID);
	pPvt->io = (epicsUInt16 *) ipmBaseAddr(carrier, slot, ipac_addrIO);
	pPvt->mem = (epicsUInt16 *) ipmBaseAddr(carrier, slot, ipac_addrMem);
	printf("initIP_EP201: ID:%p, IO:%p, MEM:%p\n", pPvt->id, pPvt->io, pPvt->mem);
	/* Get address of fieldIO_registerSet */
	if (sopcAddress & 0x800000) {
		/* The component is in the module's MEM space */
		pPvt->regs = (fieldIO_registerSet *) ((char *)(pPvt->mem) + (sopcAddress & 0x7fffff));
	} else {
		/* The component is in the module's IO space */
		pPvt->regs = (fieldIO_registerSet *) ((char *)(pPvt->io) + sopcAddress);
	}

	pPvt->intVector = interruptVector;

	pPvt->manufacturer = pPvt->id->manufacturerId & 0xff;
	pPvt->model = pPvt->id->modelId & 0xff;

	/* Link with higher level routines */
	pPvt->common.interfaceType = asynCommonType;
	pPvt->common.pinterface  = (void *)&IP_EP201Common;
	pPvt->common.drvPvt = pPvt;

	pPvt->asynDrvUser.interfaceType = asynDrvUserType;
	pPvt->asynDrvUser.pinterface = (void *)&drvUser;
	pPvt->asynDrvUser.drvPvt = pPvt;

	pPvt->uint32D.interfaceType = asynUInt32DigitalType;
	pPvt->uint32D.pinterface  = (void *)&IP_EP201UInt32D;
	pPvt->uint32D.drvPvt = pPvt;

	pPvt->int32.interfaceType = asynInt32Type;
	pPvt->int32.pinterface  = (void *)&IP_EP201Int32;
	pPvt->int32.drvPvt = pPvt;

	status = pasynManager->registerPort(pPvt->portName,
	                                    ASYN_MULTIDEVICE, /* multiDevice, cannot block */
	                                    1, /* autoconnect */
	                                    0, /* medium priority */
	                                    0);/* default stack size */
	if (status != asynSuccess) {
		printf("initIP_EP201 ERROR: Can't register port\n");
		return(-1);
	}
	status = pasynManager->registerInterface(pPvt->portName,&pPvt->common);
	if (status != asynSuccess) {
		printf("initIP_EP201 ERROR: Can't register common.\n");
		return(-1);
	}
	status = pasynManager->registerInterface(pPvt->portName,&pPvt->asynDrvUser);
	if (status != asynSuccess){
		printf("initIP_EP201 ERROR: Can't register asynDrvUser.\n");
		return(-1);
	}
	status = pasynUInt32DigitalBase->initialize(pPvt->portName, &pPvt->uint32D);
	if (status != asynSuccess) {
		printf("initIP_EP201 ERROR: Can't register UInt32Digital.\n");
		return(-1);
	}
	pasynManager->registerInterruptSource(pPvt->portName, &pPvt->uint32D,
	                                      &pPvt->interruptPvt);

	status = pasynInt32Base->initialize(pPvt->portName,&pPvt->int32);
	if (status != asynSuccess) {
		printf("initIP_EP201 ERROR: Can't register Int32.\n");
		return(-1);
	}

	/* Create asynUser for asynTrace */
	pPvt->pasynUser = pasynManager->createAsynUser(0, 0);
	pPvt->pasynUser->userPvt = pPvt;

	/* Connect to device */
	status = pasynManager->connectDevice(pPvt->pasynUser, pPvt->portName, 0);
	if (status != asynSuccess) {
		printf("initIP_EP201, connectDevice failed %s\n",
			pPvt->pasynUser->errorMessage);
		return(-1);
	}

	/* Set up the control register */
	pPvt->regs->controlRegister = dataDir;	/* Set Data Direction Reg IP_EP201 */ 
	pPvt->regs->writeDataRegister = 0;		/* Clear direct-write to field I/O */  
	pPvt->risingMask = risingMask;
	pPvt->fallingMask = fallingMask;
	
	/* Start the thread to poll and handle interrupt callbacks to device support */
	strcat(threadName, "IP_EP201");
	strcat(threadName, portName);
	epicsThreadCreate(threadName, epicsThreadPriorityHigh,
		epicsThreadGetStackSize(epicsThreadStackBig),
		(EPICSTHREADFUNC)pollerThread, pPvt);

	/* Interrupt support
	 * If risingMask, fallingMask, and intVector are zero, don't bother with interrupts, 
	 * since the user probably didn't pass this parameter to initIP_EP201()
	 */
	if (pPvt->intVector || pPvt->risingMask || pPvt->fallingMask) {
	
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
			printf("IP_EP201 interrupt connect failure\n");
			return(-1);
		}
		/* Enable IPAC module interrupts and set module status. */
		ipmIrqCmd(carrier, slot, 0, ipac_irqEnable);
		ipmIrqCmd(carrier, slot, 0, ipac_statActive);
	}
	epicsAtExit(rebootCallback, pPvt);
	return(0);
}

STATIC asynStatus create_asynDrvUser(void *drvPvt,asynUser *pasynUser,
	const char *drvInfo, const char **pptypeName,size_t *psize)
{
	/* printf("drvIP_EP201:create_asynDrvUser: entry drvInfo='%s'\n", drvInfo); */
	if (!drvInfo) {
		pasynUser->reason = 0;
	} else {
		if (strcmp(drvInfo, "INTEDGE") == 0) pasynUser->reason = INTERRUPT_EDGE;
		if (strcmp(drvInfo, "POLLTIME") == 0) pasynUser->reason = POLL_TIME;
		if (strcmp(drvInfo, "INT_EDGE_RESET") == 0) pasynUser->reason = INTERRUPT_EDGE_RESET;
	}
	return(asynSuccess);
}

STATIC asynStatus getType_asynDrvUser(void *drvPvt,asynUser *pasynUser,
	const char **pptypeName,size_t *psize)
{
	printf("drvIP_EP201:getType_asynDrvUser: entry\n");
	return(asynSuccess);
}

STATIC asynStatus destroy_asynDrvUser(void *drvPvt,asynUser *pasynUser)
{
	printf("drvIP_EP201:destroy_asynDrvUser: entry\n");
	return(asynSuccess);
}


/* Initialize IP module */
int initIP_EP201SingleRegisterPort(const char *portName, epicsUInt16 carrier, epicsUInt16 slot)
{
	drvIP_EP201Pvt *pPvt;
	int status;

#if DO_IPMODULE_CHECK
	if (ipmCheck(carrier, slot)) {
		printf("initIP_EP201SingleRegisterPort: bad carrier or slot\n");
		return(-1);
	}
#endif
	pPvt = callocMustSucceed(1, sizeof(*pPvt), "drvIP_EP201Pvt");
	pPvt->portName = epicsStrDup(portName);
	pPvt->is_fieldIO_registerSet = 0;

	/* Set up ID and I/O space addresses for IP module */
	pPvt->id = (ipac_idProm_t *) ipmBaseAddr(carrier, slot, ipac_addrID);
	pPvt->io = (epicsUInt16 *) ipmBaseAddr(carrier, slot, ipac_addrIO);
	pPvt->mem = (epicsUInt16 *) ipmBaseAddr(carrier, slot, ipac_addrMem);

	pPvt->manufacturer = pPvt->id->manufacturerId & 0xff;
	pPvt->model = pPvt->id->modelId & 0xff;

	/* Link with higher level routines */
	pPvt->common.interfaceType = asynCommonType;
	pPvt->common.pinterface  = (void *)&IP_EP201Common;
	pPvt->common.drvPvt = pPvt;

	pPvt->uint32D.interfaceType = asynUInt32DigitalType;
	pPvt->uint32D.pinterface  = (void *)&IP_EP201UInt32D;
	pPvt->uint32D.drvPvt = pPvt;

	pPvt->int32.interfaceType = asynInt32Type;
	pPvt->int32.pinterface  = (void *)&IP_EP201Int32;
	pPvt->int32.drvPvt = pPvt;

	status = pasynManager->registerPort(pPvt->portName,
	                                    ASYN_MULTIDEVICE, /* multiDevice, cannot block */
	                                    1, /* autoconnect */
	                                    0, /* medium priority */
	                                    0);/* default stack size */
	if (status != asynSuccess) {
		printf("initIP_EP201SingleRegisterPort ERROR: Can't register port\n");
		return(-1);
	}
	status = pasynManager->registerInterface(pPvt->portName,&pPvt->common);
	if (status != asynSuccess) {
		printf("initIP_EP201SingleRegisterPort ERROR: Can't register common.\n");
		return(-1);
	}
	status = pasynUInt32DigitalBase->initialize(pPvt->portName, &pPvt->uint32D);
	if (status != asynSuccess) {
		printf("initIP_EP201SingleRegisterPort ERROR: Can't register UInt32Digital.\n");
		return(-1);
	}
	pasynManager->registerInterruptSource(pPvt->portName, &pPvt->uint32D,
	                                      &pPvt->interruptPvt);

	status = pasynInt32Base->initialize(pPvt->portName,&pPvt->int32);
	if (status != asynSuccess) {
		printf("initIP_EP201SingleRegisterPort ERROR: Can't register Int32.\n");
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
		printf("initIP_EP201SingleRegisterPort, connectDevice failed %s\n",
			pPvt->pasynUser->errorMessage);
		return(-1);
	}
	return(0);
}

#define HEX2INT(c)	(isdigit(c) ? ((c) - (int)('0')) : (10 + (toupper(c) - (int)('A'))))
#define HEXHEX2INT(s)	(HEX2INT((int)((s)[0]))<<4 | HEX2INT((int)((s)[1])))
#define SHOW_STATUS 0

/* Initialize IP module's FPGA from file in Intel hex format.  In Quartus, select .sof file,
 * choose the following options: compressed; mode = "1-bit Passive Serial"; count up.
 */
#define MAXREAD 1000
#define MAXCHECK 30
#define BUF_SIZE 1000
#include "macLib.h"

int initIP_EP200_FPGA(epicsUInt16 carrier, epicsUInt16 slot, char *filepath)
{
	volatile epicsUInt16 *id, *io;
	volatile epicsUInt16 *pModeReg, *pStatusControlReg, *pConfigDataReg;
	epicsUInt16 c;
	int i, j, maxwait, total_bytes, line, intLevel;
	FILE *source_fd;
	unsigned char buffer[BUF_SIZE], *bp;
	char *filename;

#if DO_IPMODULE_CHECK
	if (ipmCheck(carrier, slot)) {
		printf("initIP_EP200_FPGA: bad carrier or slot\n");
		return(-1);
	}
#endif

	/* Disable interrupt level for this module.  Otherwise we may get
	 * interrupts while the FPGA is being loaded.
	 */
	intLevel = ipmIrqCmd(carrier, slot, 0, ipac_irqGetLevel);
	devDisableInterruptLevel(intVME,intLevel);

	filename = macEnvExpand(filepath); /* in case filepath = '$(SOFTGLUE)/...' */
	if (filename == NULL) {
		printf("initIP_EP200_FPGA: macEnvExpand() returned NULL.  I quit.\n");
		goto errReturn;
	}
	if ((source_fd = fopen(filename,"rb")) == NULL) {
		printf("initIP_EP200_FPGA: Can't open file '%s'.\n", filename);
		goto errReturn;
	}
	free(filename); /* macEnvExpand() allocated this for us. We're done with it now. */

	id = (epicsUInt16 *) ipmBaseAddr(carrier, slot, ipac_addrID);
	io = (epicsUInt16 *) ipmBaseAddr(carrier, slot, ipac_addrIO);
	pModeReg = id + 0x0a/2;
	if ((*pModeReg&0xff) != 0x48) {
		printf("initIP_EP200_FPGA: not in config mode.  (Already programmed?)  Nothing done.\n");
		goto errReturn;
	}
	pStatusControlReg = io;
	pConfigDataReg = io + 0x02/2;
	*pStatusControlReg = (*pStatusControlReg | 0x01);	/* start config */
	for (i=0; i<MAXREAD; i++) {
		c = *pStatusControlReg;
		if (SHOW_STATUS) printf("%x", c&0xf);
		if ((c&0x01) == 1) break; /* wait for ack */
	}
	if (SHOW_STATUS) printf("\nDone checking for ack\n");
	if (i == MAXREAD) {
		printf("initIP_EP200_FPGA: timeout entering data-transfer mode.  Nothing done.\n");
		goto errReturn;
	}

	maxwait = 0;
	total_bytes = 0;
	line = 0;
	while ((bp=fgets(buffer, BUF_SIZE, source_fd))) {
		int bytes;
		int recType;
		
		if (bp[0] != ':') {
			printf("initIP_EP200_FPGA: Bad file content.\n");
			goto errReturn;
		}

		++line;
		if (SHOW_STATUS && (line%100==0)) printf("initIP_EP200_FPGA: line %d\n", line);

#if 0
		/* diagnostic for VME bus analyzer trigger */
		if (line == 1885) {
			i = *id;	/* dummy read from base of id space */
			printf("initIP_EP200_FPGA: read %d from address %p\n", i, id);
		}
#endif

		bytes = HEXHEX2INT(bp+1);
		recType = HEXHEX2INT(bp+7);
		if (recType == 0) {
			for (bp+=9, j=0; j<bytes; j++) {
				for (i=0; i<MAXCHECK; i++) {
					c = *pStatusControlReg;
					/*if (SHOW_STATUS) printf("%x", (unsigned char)(c&0x0f));*/
					if ((c&3) == 3) break;
					if (i > maxwait) maxwait = i;
				}
				if (i==MAXCHECK) printf("\ntimeout waiting for pStatusControlReg==3\n");
				if ((*pStatusControlReg & 3) != 3) {
					printf("initIP_EP200_FPGA: Bad status abort at byte %d during data-transfer.\n", j);
					printf("buffer='%s'\n", buffer);
					goto errReturn;
				}
				c = (HEXHEX2INT(bp))&0xff;
				*pConfigDataReg = c; bp += 2;
				total_bytes++;
				/*if (SHOW_STATUS) printf(" - %d %x\n", total_bytes, c);*/
			}
		}
	}
	if (SHOW_STATUS) printf("initIP_EP200_FPGA: Sent %d bytes (maxwait=%d).\n", total_bytes, maxwait);
	for (i=0; i<MAXCHECK; i++) {
		c = *pStatusControlReg;
		if (SHOW_STATUS) printf("stat:%x mode:%x\n", c&0x0f, *pModeReg&0xff);
		if ((c&0x04) || ((*pModeReg&0xff) == 0x49)) break;
	}
	if (SHOW_STATUS) printf("stat:%x mode:%x\n", *pStatusControlReg&0x0f, *pModeReg&0xff);

	if ((*pStatusControlReg & 0x04) || ((*pModeReg&0xff) == 0x49)) {
		printf("initIP_EP200_FPGA: FPGA config done.\n");
	}
	/* issue a software reset */
	*pStatusControlReg = *pStatusControlReg | 0x80;

	devEnableInterruptLevel(intVME,intLevel);
	return(0);

errReturn:
	devEnableInterruptLevel(intVME,intLevel);
	return(-1);
}

/*
 * calcRegisterAddress - For single-register components, the sopc address is
 * specified in the EPICS record's INP or OUT field, and we need to translate
 * that into a VME address.  Part of the job was done by
 * initIP_EP201SingleRegisterPort(), which got the VME base addresses of the
 * IndustryPack module's IO and MEM spaces.
 * See "The addressing of sopc components", in comments at the top of this file
 * for a complete description of how addresses are handled.
 */
STATIC epicsUInt16 *calcRegisterAddress(void *drvPvt, asynUser *pasynUser)
{
	drvIP_EP201Pvt *pPvt = (drvIP_EP201Pvt *)drvPvt;
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
 * softGlueCalcSpecifiedRegisterAddress - For access to a single-register component by
 * other than an EPICS record (for example, by an interrupt-service routine).
 * The carrier, slot, and sopc address are specified as arguments, and we need
 * to translate that information into a VME address, as calcRegisterAddress()
 * would have done for an EPICS record.
 */
epicsUInt16 *softGlueCalcSpecifiedRegisterAddress(epicsUInt16 carrier, epicsUInt16 slot, int addr)
{
	epicsUInt16 *reg;
	epicsUInt16 *io = (epicsUInt16 *) ipmBaseAddr(carrier, slot, ipac_addrIO);
	epicsUInt16 *mem = (epicsUInt16 *) ipmBaseAddr(carrier, slot, ipac_addrMem);

	if (addr & 0x800000) {
		addr &= 0x7fffff;	/* mask the sopc MEM-space indicator bit */
		addr >>= 1;			/* convert byte-address offset to (two-byte) word-address offset */
		reg = (epicsUInt16 *) (mem+addr);
	} else {
		addr = addr >> 1;
		reg = (epicsUInt16 *) (io+addr);
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
STATIC asynStatus readUInt32D(void *drvPvt, asynUser *pasynUser,
	epicsUInt32 *value, epicsUInt32 mask)
{
	drvIP_EP201Pvt *pPvt = (drvIP_EP201Pvt *)drvPvt;
	volatile epicsUInt16 *reg;

	if (drvIP_EP201Debug >= 5) {
		printf("drvIP_EP201:readUInt32D: pasynUser->reason=%d\n", pasynUser->reason);
	}
	*value = 0;
	if (pPvt->is_fieldIO_registerSet) {
		if (pasynUser->reason == 0) {
			/* read data */
			if (drvIP_EP201Debug >= 5) printf("drvIP_EP201:readUInt32D: fieldIO reg address=%p\n", &(pPvt->regs->readDataRegister));
			*value = (epicsUInt32) (pPvt->regs->readDataRegister & mask);
		} else if (pasynUser->reason == INTERRUPT_EDGE) {
			/* read interrupt-enable edge bits.  This is a kludge.  We need to specify one of 16 I/O
			 * bits, and also to indicate which of four states applies to that bit.  To do this, we
			 * use a two bit mask, the lower bit of which specifies the I/O bit.  The four states
			 * indicate which signal edge is enabled to generate interrupts (00: no edge enabled,
			 * 01: rising edge enabled, 10: falling edge enabled, 11: both edges enabled).
			 */
			epicsUInt16 rising, falling;
			*value = 0;
			rising = (epicsUInt16) (pPvt->regs->risingIntEnable & (mask&(mask>>1)) );
			falling = (epicsUInt16) (pPvt->regs->fallingIntEnable & (mask&(mask>>1)) );
			if (rising) *value |= 1;
			if (falling) *value |= 2;
			/* Left shift the (two-bit) value so it overlaps the mask that caller gave us. */
			for (; mask && ((mask&1) == 0); mask>>=1, *value<<=1);
		} else if (pasynUser->reason == POLL_TIME) {
			*value = pPvt->pollTime*1000;
		} else if (pasynUser->reason == INTERRUPT_EDGE_RESET) {
			*value = pPvt->disabledIntMask;
		}
	} else {
		reg = calcRegisterAddress(drvPvt, pasynUser);
		if (drvIP_EP201Debug >= 5) printf("drvIP_EP201:readUInt32D: softGlue reg address=%p\n", reg);
		*value = (epicsUInt32) (*reg & mask);
	}
	asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
		"drvIP_EP201::readUInt32D, value=0x%X, mask=0x%X\n", *value,mask);
	return(asynSuccess);
}

/*
 * writeUInt32D
 * This method provides bit level write access to the writeDataRegister of a
 * fieldIO_registerSet component, and to the sopc address of a single register
 * component.  Bits of 'value' that correspond to zero bits of 'mask' will
 * be ignored -- corresponding bits of the destination register will be left
 * as they were before the write operation.  
 * Note that fieldIO_registerSet components have a control register that
 * may determine what use will be made of the data we write.
 */
STATIC asynStatus writeUInt32D(void *drvPvt, asynUser *pasynUser, epicsUInt32 value,
	epicsUInt32 mask)
{
	drvIP_EP201Pvt *pPvt = (drvIP_EP201Pvt *)drvPvt;
	volatile epicsUInt16 *reg=0;
	epicsUInt32 maskCopy;

	if (drvIP_EP201Debug >= 5) {
		printf("drvIP_EP201:writeUInt32D: pasynUser->reason=%d\n", pasynUser->reason);
	}
	if (pPvt->is_fieldIO_registerSet) {
		if (pasynUser->reason == 0) {
			/* write data */
			if (drvIP_EP201Debug >= 5) printf("drvIP_EP201:writeUInt32D: fieldIO reg address=%p\n", &(pPvt->regs->writeDataRegister));
			pPvt->regs->writeDataRegister = (pPvt->regs->writeDataRegister & ~mask) | (value & mask);
		} else if (pasynUser->reason == INTERRUPT_EDGE) {
			/* set interrupt-enable edge bits in the FPGA */
			/* move value from shifted position to unshifted position */
			for (maskCopy=mask; maskCopy && ((maskCopy&1) == 0); maskCopy>>=1, value>>=1);
			maskCopy = mask & (mask>>1); /* use lower bit only of two-bit mask for register write */
			pPvt->regs->risingIntEnable = (pPvt->regs->risingIntEnable & ~maskCopy);
			pPvt->regs->fallingIntEnable = (pPvt->regs->fallingIntEnable & ~maskCopy);
			switch (value) {
			case 0: /* disable interrupt from this bit */
				break;
			case 1: /* rising-edge only */
				pPvt->regs->risingIntEnable = (pPvt->regs->risingIntEnable & ~maskCopy) | maskCopy;
				break;
			case 2: /* falling-edge only */
				pPvt->regs->fallingIntEnable = (pPvt->regs->fallingIntEnable & ~maskCopy) | maskCopy;
				break;
			case 3: /* rising-edge and falling-edge */
				pPvt->regs->risingIntEnable = (pPvt->regs->risingIntEnable & ~maskCopy) | maskCopy;
				pPvt->regs->fallingIntEnable = (pPvt->regs->fallingIntEnable & ~maskCopy) | maskCopy;
				break;
			}
			pPvt->risingMask = pPvt->regs->risingIntEnable;
			pPvt->fallingMask = pPvt->regs->fallingIntEnable;
		} else if (pasynUser->reason == POLL_TIME) {
			pPvt->pollTime = value/1000.;
			if (pPvt->pollTime < .05) {
				pPvt->pollTime = .05;
				printf("drvIP_EP201:writeUInt32D: pollTime lower limit = %d ms\n", (int)(pPvt->pollTime*1000));
			}
			if (drvIP_EP201Debug) printf("drvIP_EP201:writeUInt32D: pPvt->pollTime=%f s\n", pPvt->pollTime);
		}
	} else {
		int addr;
		pasynManager->getAddr(pasynUser, &addr);
		reg = calcRegisterAddress(drvPvt, pasynUser);
		if (drvIP_EP201Debug >= 5) printf("drvIP_EP201:writeUInt32D: softGlue reg address=%p\n", reg);
		if (addr & 0x800000) {
			*reg = (*reg & ~mask) | (epicsUInt16) (value & mask);
		} else {
			*reg = (*reg & ~mask) | (epicsUInt16) (value & mask);
		}
	}

	asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
		"drvIP_EP201::writeUInt32D, addr=0x%X, value=0x%X, mask=0x%X, reason=%d\n",
			reg, value, mask, pasynUser->reason);
	return(asynSuccess);
}

/*
 * readInt32
 * This method reads from the readDataRegister of a fieldIO_registerSet
 * component, or from the sopc address of a single register component.  
 * Note that fieldIO_registerSet components have a control register that
 * may determine what data will be available for reading.
 */
STATIC asynStatus readInt32(void *drvPvt, asynUser *pasynUser, epicsInt32 *value)
{
	drvIP_EP201Pvt *pPvt = (drvIP_EP201Pvt *)drvPvt;
	volatile epicsUInt16 *reg;

	if (drvIP_EP201Debug >= 5) {
		printf("drvIP_EP201:readInt32: pasynUser->reason=%d\n", pasynUser->reason);
	}
	*value = 0;

	if (pasynUser->reason == 0) {
		if (pPvt->is_fieldIO_registerSet) {
			if (drvIP_EP201Debug >= 5) printf("drvIP_EP201:readInt32: fieldIO reg address=%p\n", &(pPvt->regs->readDataRegister));
			*value = (epicsUInt32) pPvt->regs->readDataRegister;
		} else {
			reg = calcRegisterAddress(drvPvt, pasynUser);
			if (drvIP_EP201Debug >= 5) printf("drvIP_EP201:readInt32: softGlue reg address=%p\n", reg);
			*value = (epicsUInt32)(*reg);
		}
	} else if (pasynUser->reason == POLL_TIME) {
		*value = (epicsUInt32) (pPvt->pollTime*1000);
	} else if (pasynUser->reason == INTERRUPT_EDGE_RESET) {
		*value = pPvt->disabledIntMask;
	}

	asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
		"drvIP_EP201::readInt32, value=0x%X\n", *value);
	return(asynSuccess);
}

/* writeInt32
 * This method writes to the writeDataRegister of a fieldIO_registerSet
 * component, and to the sopc address of a single register component.  
 * Note that fieldIO_registerSet components have a control register that
 * may determine what use will be made of the data we write.
 */
STATIC asynStatus writeInt32(void *drvPvt, asynUser *pasynUser, epicsInt32 value)
{
	drvIP_EP201Pvt *pPvt = (drvIP_EP201Pvt *)drvPvt;
	volatile epicsUInt16 *reg;

	if (drvIP_EP201Debug >= 5) {
		printf("drvIP_EP201:writeInt32: pasynUser->reason=%d\n", pasynUser->reason);
	}
	if (pasynUser->reason == 0) {
		if (pPvt->is_fieldIO_registerSet) {
			if (drvIP_EP201Debug >= 5) printf("drvIP_EP201:writeInt32: fieldIO reg address=%p\n", &(pPvt->regs->writeDataRegister));
			pPvt->regs->writeDataRegister = (epicsUInt16) value;
		} else {
			reg = calcRegisterAddress(drvPvt, pasynUser);
			if (drvIP_EP201Debug >= 5) printf("drvIP_EP201:writeInt32: softGlue reg address=%p\n", reg);
			*reg = (epicsUInt16) value;
		}
	} else if (pasynUser->reason == POLL_TIME) {
			pPvt->pollTime = value/1000;
	}

	asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
		"drvIP_EP201::writeInt32, value=0x%X, reason=%d\n",value, pasynUser->reason);
	return(asynSuccess);
}

STATIC asynStatus getBounds(void *drvPvt, asynUser *pasynUser, epicsInt32 *low, epicsInt32 *high)
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

/***********************************************************************
 * Manage a table of registered interrupt-service routines to be called
 * at interrupt level.  This is not the normal softGlue interrupt mechanism;
 * it's for interrupts that will occur at too high a frequency, or too
 * close together in time, for that mechanism to handle.
 */

/* table of registered routines for execution at interrupt level */
#define MAXROUTINES 10

typedef struct {
	ushort carrier;
	ushort slot;
	ushort mask;
	int sopcAddress;
	volatile fieldIO_registerSet *regs;
	void (*routine)(softGlueIntRoutineData *IRData);
	softGlueIntRoutineData IRData;
} intRoutineEntry;

intRoutineEntry registeredIntRoutines[MAXROUTINES] = {{0}};
int numRegisteredIntRoutines=0;

/* Register a routine to be called at interrupt level when a specified
 * I/O bit (addr/mask) from a specified carrier/slot generates an interrupt.
 * example invocation:
 *      void callMe(softGlueIntRoutineData *IRData);
 *		myDataType myData;
 *      softGlueRegisterInterruptRoutine(0, 0, 0x800000, 0x0, callMe, (void *)&myData);
 */
int softGlueRegisterInterruptRoutine(ushort carrier, ushort slot, int sopcAddress, ushort mask,
	void (*routine)(softGlueIntRoutineData *IRData), void *userPvt) {
	int i;
	drvIP_EP201Pvt *pPvt;

	if (numRegisteredIntRoutines >= MAXROUTINES-1) return(-1);
	registeredIntRoutines[numRegisteredIntRoutines].carrier = carrier;
	registeredIntRoutines[numRegisteredIntRoutines].slot = slot;
	registeredIntRoutines[numRegisteredIntRoutines].sopcAddress = sopcAddress;
	registeredIntRoutines[numRegisteredIntRoutines].mask = mask;
	registeredIntRoutines[numRegisteredIntRoutines].routine = routine;
	registeredIntRoutines[numRegisteredIntRoutines].IRData.userPvt = userPvt;

	/* Go through driverTable for all drvIP_EP201Pvt pointers assoc with this carrier/slot. */
	for (i=0; i<MAX_DRVPVT; i++) {
		pPvt = driverTable[i];
		if (drvIP_EP201Debug>5) {
			printf("softGlueRegisterInterruptRoutine: examining driverTable[%i]:\n",i);
			printf("....carrier = %d, slot = %d, sopcAddress = 0x%x, regs=%p\n",
				pPvt->carrier, pPvt->slot, pPvt->sopcAddress, pPvt->regs);
			}
		if (pPvt && (pPvt->carrier == carrier) && (pPvt->slot == slot) && (pPvt->sopcAddress == sopcAddress)) {
			registeredIntRoutines[numRegisteredIntRoutines].regs = pPvt->regs;
		}
	}
	if (drvIP_EP201Debug>5) printf("softGlueRegisterInterruptRoutine: #%d, carrier=%d, slot=%d, sopcAddress=0x%x, mask=0x%x, regs=%p",
		numRegisteredIntRoutines, carrier, slot, sopcAddress, mask, registeredIntRoutines[numRegisteredIntRoutines].regs);
	numRegisteredIntRoutines++;
	return(0);
}

/***********************************************************************/
/*
 * This is the interrupt-service routine associated with the interrupt
 * vector pPvt->intVector supplied in the drvPvt structure.
 * On interrupt, we check to see if the interrupt could have come from
 * the fieldIO_registerSet named in caller's drvPvt.  If so, we collect
 * information, and send a message to pollerThread().
 */
STATIC void intFunc(void *drvPvt)
{
	drvIP_EP201Pvt *pPvt = (drvIP_EP201Pvt *)drvPvt;
	epicsUInt16 pendingLow, pendingHigh;
	interruptMessage msg;
	int i, handled;

	/* Make sure interrupt is from this hardware.  Otherwise just leave. */
	if (pPvt->regs->risingIntStatus || pPvt->regs->fallingIntStatus) {
		if (drvIP_EP201Debug) {
			logMsg("fallingIntStatus=0x%x, risingIntStatus=0x%x\n", pPvt->regs->fallingIntStatus, pPvt->regs->risingIntStatus);
			logMsg("fallingIntEnable=0x%x, risingIntEnable=0x%x\n", pPvt->regs->fallingIntEnable, pPvt->regs->risingIntEnable);
		}
		pendingLow = pPvt->regs->fallingIntStatus;
		pendingHigh = pPvt->regs->risingIntStatus;
		msg.interruptMask = (pendingLow & pPvt->regs->fallingIntEnable);
		msg.interruptMask |= (pendingHigh & pPvt->regs->risingIntEnable);

		/* Read the current input */
		msg.bits = pPvt->regs->readDataRegister;
		if (drvIP_EP201Debug) logMsg("interruptMask=0x%x\n", msg.interruptMask);

		/* Go through registeredIntRoutines[] for a registered interrupt-level service routine. If one is found,
		 * call it and mark the interrupt as "handled", so we don't queue any EPICS processing that
		 * might also be attached to the interrupt.  (The whole purpose of this mechanism is to
		 * handle interrupts at intervals EPICS would not be able to meet.) */
		for (i=0, handled=0; i<numRegisteredIntRoutines; i++) {
			if ((registeredIntRoutines[i].regs == pPvt->regs) && (msg.interruptMask & registeredIntRoutines[i].mask)) {
				if (drvIP_EP201Debug>5) logMsg("intFunc: calling registered interrupt routine %p\n", registeredIntRoutines[i].routine);
				registeredIntRoutines[i].IRData.mask = msg.interruptMask;
				registeredIntRoutines[i].routine(&(registeredIntRoutines[i].IRData));
				handled = 1;
			}
		}

		if (!handled) {
			if (epicsMessageQueueTrySend(pPvt->msgQId, &msg, sizeof(msg)) == 0)
				pPvt->messagesSent++;
			else
				pPvt->messagesFailed++;

			/* If too many interrupts have been received, disable the offending bits. */
			if (++(pPvt->interruptCount) > MAX_IRQ) {
				pPvt->regs->risingIntEnable &= ~pendingHigh;
				pPvt->regs->fallingIntEnable &= ~pendingLow;
				pPvt->disabledIntMask = pendingHigh | pendingLow;
				if (drvIP_EP201Debug) logMsg("intFunc: disabledIntMask=0x%x\n", pPvt->disabledIntMask);
			}
		}

		/* Clear the interrupt bits we handled */
		pPvt->regs->risingIntStatus = pendingHigh;
		pPvt->regs->fallingIntStatus = pendingLow;

		/* Generate dummy read cycle for PPC */
		pendingHigh = pPvt->regs->risingIntStatus;

	}
}


/* This function runs in a separate thread.  It waits for the poll
 * time, or an interrupt, whichever comes first.  If the bits read from
 * the IP_EP201 have changed then it does callbacks to all clients that
 * have registered with registerInterruptUser
 */

STATIC void pollerThread(drvIP_EP201Pvt *pPvt)
{
	epicsUInt32 newBits32=0;
	epicsUInt16 newBits=0, changedBits=0, interruptMask=0;
	interruptMessage msg;
	ELLLIST *pclientList;
	interruptNode *pnode;
	asynUInt32DigitalInterrupt *pUInt32D;
	int hardware = 0;

	while(1) {
		/*  Wait for an interrupt or for the poll time, whichever comes first */
		if (epicsMessageQueueReceiveWithTimeout(pPvt->msgQId, 
			                                    &msg, sizeof(msg), 
			                                    pPvt->pollTime) == -1) {
			/* The wait timed out, so there was no interrupt, so we need
			 * to read the bits.  If there was an interrupt the bits got
			 * set in the interrupt routine
			 */
			readUInt32D(pPvt, pPvt->pasynUser, &newBits32, 0xffff);
			newBits = newBits32;
			hardware = 0;
			interruptMask = 0;
		} else {
			newBits = msg.bits;
			interruptMask = msg.interruptMask;
			pPvt->interruptCount--;
			if (drvIP_EP201Debug > 5)
				printf("drvIP_EP201:pollerThread: intCount=%d\n", pPvt->interruptCount);
			asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW,
				"drvIP_EP201:pollerThread, got interrupt for port %s\n", pPvt->portName);
			hardware = 1;
		}
		asynPrint(pPvt->pasynUser, ASYN_TRACEIO_DRIVER,
			"drvIP_EP201:pollerThread, bits=%x\n", newBits);

		/* We detect change both from interruptMask (which only works for
		 * hardware interrupts) and changedBits, which works for polling */
		if (drvIP_EP201Debug) printf("drvIP_EP201:pollerThread: new=0x%x, old=0x%x\n", newBits, pPvt->oldBits);
		if (hardware==0) {
			changedBits = newBits ^ pPvt->oldBits;
			interruptMask = interruptMask & changedBits;
		}
		if (drvIP_EP201Debug)
			printf("drvIP_EP201:pollerThread: hardware=%d, IntMask=0x%x\n", hardware, interruptMask);
		pPvt->oldBits = newBits;

		/*
		 * Process any records that (1) have registered with registerInterruptUser, and (2) that have a mask
		 * value that includes this bit.
		 */
		if (interruptMask) {
			pasynManager->interruptStart(pPvt->interruptPvt, &pclientList);
			pnode = (interruptNode *)ellFirst(pclientList);
			while (pnode) {
				pUInt32D = pnode->drvPvt;
				if ((pUInt32D->mask & interruptMask) && (pUInt32D->pasynUser->reason == 0)) {
					asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, "drvIP_EP201:pollerThread, calling client %p"
						" mask=%x, callback=%p\n", pUInt32D, pUInt32D->mask, pUInt32D->callback);
					pUInt32D->callback(pUInt32D->userPvt, pUInt32D->pasynUser, pUInt32D->mask & newBits);
					if (drvIP_EP201Debug) {
						printf("drvIP_EP201:pollerThread: calling client %p\n", pUInt32D);
					}
				}
				pnode = (interruptNode *)ellNext(&pnode->node);
			}
			pasynManager->interruptEnd(pPvt->interruptPvt);
		}
		/* If intFunc disabled any interrupt bits, cause them to show their new states. */
		if (pPvt->disabledIntMask) {
			epicsUInt32 maskBit;
			if (drvIP_EP201Debug)
				printf("drvIP_EP201:pollerThread: disabledIntMask=0x%x\n", pPvt->disabledIntMask);
			pasynManager->interruptStart(pPvt->interruptPvt, &pclientList);
			pnode = (interruptNode *)ellFirst(pclientList);
			while (pnode) {
				pUInt32D = pnode->drvPvt;
				if (pUInt32D->pasynUser->reason == INTERRUPT_EDGE_RESET) {
					if (drvIP_EP201Debug>10) printf("drvIP_EP201:pollerThread: reason == INTERRUPT_EDGE_RESET,mask=0x%x\n", pUInt32D->mask);
					/* The lower bit of pUInt32D->mask is the bit we're actually controlling.  pUInt32D->mask has
					 * the next higher bit also set as part of a kludge to represent states of both falling- and
					 * rising-edge enables, while still indicating the controlled bit.
					 */
					maskBit = pUInt32D->mask & ((pUInt32D->mask)>>1);
					if (maskBit & pPvt->disabledIntMask) {
						asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, "drvIP_EP201:pollerThread, calling client %p"
							" mask=%x, callback=%p\n", pUInt32D, pUInt32D->mask, pUInt32D->callback);
						/* Process the record that will show the user we disabled this bit's interrupt capability. */
						pUInt32D->callback(pUInt32D->userPvt, pUInt32D->pasynUser, pUInt32D->mask);
					}
				}
				pnode = (interruptNode *)ellNext(&pnode->node);
			}
			pasynManager->interruptEnd(pPvt->interruptPvt);
			pPvt->disabledIntMask = 0;
		}
	}
}

/* I don't know what these two functions are for.  I'm just including them because an example did. */
STATIC asynStatus setInterruptUInt32D(void *drvPvt, asynUser *pasynUser, epicsUInt32 mask,
	interruptReason reason)
{
	printf("drvIP_EP201:setInterruptUInt32D: entry mask=%d, reason=%d\n", mask, reason);
	return(0);
}

STATIC asynStatus clearInterruptUInt32D(void *drvPvt, asynUser *pasynUser, epicsUInt32 mask)
{
	printf("drvIP_EP201:clearInterruptUInt32D: entry mask=%d\n", mask);
	return(0);
}

STATIC void rebootCallback(void *drvPvt)
{
	drvIP_EP201Pvt *pPvt = (drvIP_EP201Pvt *)drvPvt;
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
STATIC void report(void *drvPvt, FILE *fp, int details)
{
	drvIP_EP201Pvt *pPvt = (drvIP_EP201Pvt *)drvPvt;
	interruptNode *pnode;
	ELLLIST *pclientList;

	fprintf(fp, "drvIP_EP201 %s: connected at base address %p\n",
		pPvt->portName, pPvt->regs);
	if (details >= 1) {
		fprintf(fp, "  controlRegister=0x%x\n", pPvt->regs->controlRegister);
		fprintf(fp, "  risingMask=0x%x\n", pPvt->risingMask);
		fprintf(fp, "  risingIntEnable=0x%x\n", pPvt->regs->risingIntEnable);
		fprintf(fp, "  fallingMask=0x%x\n", pPvt->fallingMask);
		fprintf(fp, "  fallingIntEnable=0x%x\n", pPvt->regs->fallingIntEnable);
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
STATIC asynStatus connect(void *drvPvt, asynUser *pasynUser)
{
	pasynManager->exceptionConnect(pasynUser);
	return(asynSuccess);
}

/* Connect */
STATIC asynStatus disconnect(void *drvPvt, asynUser *pasynUser)
{
	pasynManager->exceptionDisconnect(pasynUser);
	return(asynSuccess);
}

/*** iocsh functions ***/

/* int initIP_EP200(epicsUInt16 carrier, epicsUInt16 slot, char *portName1,
 *		char *portName2, char *portName3, int sopcBase);
 */
STATIC const iocshArg initEP200Arg0 = { "Carrier Number",	iocshArgInt};
STATIC const iocshArg initEP200Arg1 = { "Slot Number",		iocshArgInt};
STATIC const iocshArg initEP200Arg2 = { "portName1",		iocshArgString};
STATIC const iocshArg initEP200Arg3 = { "portName2",		iocshArgString};
STATIC const iocshArg initEP200Arg4 = { "portName3",		iocshArgString};
STATIC const iocshArg initEP200Arg5 = { "SOPC Base",		iocshArgInt};
STATIC const iocshArg * const initEP200Args[6] = {&initEP200Arg0, &initEP200Arg1, &initEP200Arg2,
	&initEP200Arg3, &initEP200Arg4, &initEP200Arg5};
STATIC const iocshFuncDef initEP200FuncDef = {"initIP_EP200",6,initEP200Args};
STATIC void initEP200CallFunc(const iocshArgBuf *args) {
	initIP_EP200(args[0].ival, args[1].ival, args[2].sval, args[3].sval, args[4].sval, args[5].ival);
}

/* int initIP_EP200_Int(epicsUInt16 carrier, epicsUInt16 slot, int intVectorBase,
 *		int risingMaskMS, int risingMaskLS, int fallingMaskMS, int fallingMaskLS);
 */
STATIC const iocshArg initEP200_IntArg0 = { "Carrier Number",	iocshArgInt};
STATIC const iocshArg initEP200_IntArg1 = { "Slot Number",		iocshArgInt};
STATIC const iocshArg initEP200_IntArg2 = { "intVectorBase",	iocshArgInt};
STATIC const iocshArg initEP200_IntArg3 = { "risingMaskMS",		iocshArgInt};
STATIC const iocshArg initEP200_IntArg4 = { "risingMaskLS",		iocshArgInt};
STATIC const iocshArg initEP200_IntArg5 = { "fallingMaskMS",	iocshArgInt};
STATIC const iocshArg initEP200_IntArg6 = { "fallingMaskLS",	iocshArgInt};
STATIC const iocshArg * const initEP200_IntArgs[7] = {&initEP200_IntArg0, &initEP200_IntArg1, &initEP200_IntArg2,
	&initEP200_IntArg3, &initEP200_IntArg4, &initEP200_IntArg5, &initEP200_IntArg6};
STATIC const iocshFuncDef initEP200_IntFuncDef = {"initIP_EP200_Int",7,initEP200_IntArgs};
STATIC void initEP200_IntCallFunc(const iocshArgBuf *args) {
	initIP_EP200_Int(args[0].ival, args[1].ival, args[2].ival, args[3].ival, args[4].ival, args[5].ival,
		args[6].ival);
}

/* int initIP_EP200_IO(epicsUInt16 carrier, epicsUInt16 slot, int moduleType, int dataDir); */
STATIC const iocshArg initEP200_IOArg0 = { "Carrier Number", iocshArgInt};
STATIC const iocshArg initEP200_IOArg1 = { "Slot Number",	 iocshArgInt};
STATIC const iocshArg initEP200_IOArg2 = { "moduleType",	 iocshArgInt};
STATIC const iocshArg initEP200_IOArg3 = { "dataDir",		 iocshArgInt};
STATIC const iocshArg * const initEP200_IOArgs[4] = {&initEP200_IOArg0, &initEP200_IOArg1, &initEP200_IOArg2,
	&initEP200_IOArg3};
STATIC const iocshFuncDef initEP200_IOFuncDef = {"initIP_EP200_IO",4,initEP200_IOArgs};
STATIC void initEP200_IOCallFunc(const iocshArgBuf *args) {
	initIP_EP200_IO(args[0].ival, args[1].ival, args[2].ival, args[3].ival);
}


/* int initIP_EP201(const char *portName, epicsUInt16 carrier, epicsUInt16 slot,
 *			int msecPoll, int dataDir, int sopcOffset, int interruptVector,
 *			int risingMask, int fallingMask)
 */
STATIC const iocshArg initEP201Arg0 = { "Port Name",			iocshArgString};
STATIC const iocshArg initEP201Arg1 = { "Carrier Number",		iocshArgInt};
STATIC const iocshArg initEP201Arg2 = { "Slot Number",			iocshArgInt};
STATIC const iocshArg initEP201Arg3 = { "msecPoll",				iocshArgInt};
STATIC const iocshArg initEP201Arg4 = { "Data Dir Reg",			iocshArgInt};
STATIC const iocshArg initEP201Arg5 = { "SOPC Offset Addr",		iocshArgInt};
STATIC const iocshArg initEP201Arg6 = { "Interrupt Vector",		iocshArgInt};
STATIC const iocshArg initEP201Arg7 = { "Rising Edge Mask",		iocshArgInt};
STATIC const iocshArg initEP201Arg8 = { "Falling Edge Mask",	iocshArgInt};
STATIC const iocshArg * const initEP201Args[9] = {&initEP201Arg0, &initEP201Arg1, &initEP201Arg2,
	&initEP201Arg3, &initEP201Arg4, &initEP201Arg5, &initEP201Arg6, &initEP201Arg7, &initEP201Arg8};
STATIC const iocshFuncDef initEP201FuncDef = {"initIP_EP201",9,initEP201Args};
STATIC void initEP201CallFunc(const iocshArgBuf *args) {
	initIP_EP201(args[0].sval, args[1].ival, args[2].ival, args[3].ival, args[4].ival, args[5].ival,
	            args[6].ival, args[7].ival, args[8].ival);
}

/* int initIP_EP201SingleRegisterPort(const char *portName, epicsUInt16 carrier, epicsUInt16 slot) */
STATIC const iocshArg initSRArg0 = { "Port name",		iocshArgString};
STATIC const iocshArg initSRArg1 = { "Carrier Number",	iocshArgInt};
STATIC const iocshArg initSRArg2 = { "Slot Number",		iocshArgInt};
STATIC const iocshArg * const initSRArgs[3] = {&initSRArg0, &initSRArg1, &initSRArg2};
STATIC const iocshFuncDef initSRFuncDef = {"initIP_EP201SingleRegisterPort",3,initSRArgs};
STATIC void initSRCallFunc(const iocshArgBuf *args) {
	initIP_EP201SingleRegisterPort(args[0].sval, args[1].ival, args[2].ival);
}

/* int initIP_EP200_FPGA(epicsUInt16 carrier, epicsUInt16 slot, char filename) */
STATIC const iocshArg initFPGAArg1 = { "Carrier Number",	iocshArgInt};
STATIC const iocshArg initFPGAArg2 = { "Slot Number",		iocshArgInt};
STATIC const iocshArg initFPGAArg3 = { "File pathStart",	iocshArgString};
STATIC const iocshArg * const initFPGAArgs[3] = {&initFPGAArg1, &initFPGAArg2, &initFPGAArg3};
STATIC const iocshFuncDef initFPGAFuncDef = {"initIP_EP200_FPGA",3,initFPGAArgs};
STATIC void initFPGACallFunc(const iocshArgBuf *args) {
	initIP_EP200_FPGA(args[0].ival, args[1].ival, args[2].sval);
}


void IP_EP201Register(void)
{
	iocshRegister(&initEP200FuncDef,initEP200CallFunc);
	iocshRegister(&initEP200_IntFuncDef,initEP200_IntCallFunc);
	iocshRegister(&initEP200_IOFuncDef,initEP200_IOCallFunc);
	iocshRegister(&initEP201FuncDef,initEP201CallFunc);
	iocshRegister(&initSRFuncDef,initSRCallFunc);
	iocshRegister(&initFPGAFuncDef,initFPGACallFunc);
}

epicsExportRegistrar(IP_EP201Register);

