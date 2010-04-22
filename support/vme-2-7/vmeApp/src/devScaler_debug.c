/*******************************************************************************
devScaler.c
Device-support routines for Lecroy 1151 16-channel, 32-bit scaler

Original Author: Tim Mooney
Date: 1/16/95

Experimental Physics and Industrial Control System (EPICS)

Copyright 1995, the University of Chicago Board of Governors.

This software was produced under U.S. Government contract
W-31-109-ENG-38 at Argonne National Laboratory.

Initial development by:
	The X-ray Optics Group
	Experimental Facilities Division
	Advanced Photon Source
	Argonne National Laboratory

OSI  by S. Kate Feng 3/03

Modification Log:
-----------------
 6/26/93    tmm     Lecroy scaler
 1/16/95    tmm     v1.0 Joerger scaler
 6/11/96    tmm     v1.1 fixed test distinguishing VSC16 and VSC8
 6/28/96    tmm     v1.2 use vsc_num_cards instead of MAX_SCALER_CARDS
 8/20/96    tmm     v1.3 scalerstate, localAddr no longer marked 'volatile'
10/03/96    tmm     v1.4 fix off-by-one problem
 3/03/96    tmm     v1.5 fix test distinguishing VSC16 and VSC8 for ECL/NIM
 3/03/97    tmm     v1.6 add reboot hook (disable interrupts & reset)
 4/24/98    tmm     v1.7 use callbackRequest instead of scanIoReq
 3/20/03    skf     OSI
10/22/03    tmm     v1.8 Removed 3.13 compatibility; scalerWdTimerQ moved
                    to scaler record
 7/07/04    tmm     v1.9 Some code cleanup, debug, and cosmetic changes.
10/26/06    mlr     v1.10 Changed dset interface functions to use precord not card.
                    CALLBACK pointer passed in init_record.
                    dpvt now holds card number.  callback structures are now in
                    rpvt, not dpvt.
*******************************************************************************/
/* version 1.10 */

#include	<epicsVersion.h>

#ifdef HAS_IOOPS_H
#include	<basicIoOps.h>
#endif

typedef unsigned int uint32;
typedef unsigned short uint16;

#ifdef vxWorks
#include	<rebootLib.h>
extern int logMsg(char *fmt, ...);
#else
#define printf errlogPrintf
#endif

#ifndef OK
#define OK      0
#endif

#ifndef ERROR
#define ERROR   -1
#endif

#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<math.h>

#include	<epicsTimer.h>
#include	<epicsThread.h>
#include	<epicsExport.h>
#include	<errlog.h>
#include	<devLib.h>
#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbCommon.h>
#include	<dbScan.h>
#include	<recGbl.h>
#include	<recSup.h>
#include	<devSup.h>
#include	<drvSup.h>
#include	<dbScan.h>
#include	<special.h>
#include	<callback.h>

#include	"scalerRecord.h"
#include	"devScaler.h"

/*** Debug support ***/
#ifdef NODEBUG
#define STATIC static
#define Debug(l,FMT,V) ;
#else
#define STATIC
#if 0
#define Debug(l,FMT,V) {  if(l <= devScalerDebug) \
			{ logMsg("%s(%d):",__FILE__,__LINE__); \
			  logMsg(FMT,V); } }
#endif
#define Debug(l,FMT,V) {  if (l <= devScalerDebug) { logMsg(FMT,V); } }
#endif
volatile int devScalerDebug=0;

#define CARD_0_ADDRESS 0x40000000
#define CARD_ADDRESS_SPACE 0x100
#define RESET_OFFSET 0x00
#define CTRL_OFFSET 0x04
#define DIRECTION_OFFSET 0x08
#define STATUS_ID_OFFSET 0x10
#define IRQ_VECTOR_OFFSET 0x10
#define IRQ_LEVEL_ENABLE_OFFSET 0x14
#define IRQ_MASK_OFFSET 0x18
#define IRQ_RESET_OFFSET 0x1C
#define REV_SERIAL_NO_OFFSET 0x20
#define MODULE_TYPE_OFFSET 0x24
#define MANUFACTURER_ID_OFFSET 0x28
#define DATA_0_OFFSET 0x80
#define PRESET_0_OFFSET 0xC0

static long vsc_num_cards = 0;
STATIC void *vsc_addrs = (void *)0x00900000;
STATIC long	vsc_InterruptVector = 0;

/* device-support entry table */
STATIC long scaler_report(int level);
STATIC long scaler_init(int after);
STATIC long scaler_init_record(struct scalerRecord *psr, CALLBACK *pcallback);
#define scaler_get_ioint_info NULL
STATIC long scaler_reset(scalerRecord *psr);
STATIC long scaler_read(scalerRecord *psr, long *val);
STATIC long scaler_write_preset(scalerRecord *psr, int signal, long val);
STATIC long scaler_arm(scalerRecord *psr, int val);
STATIC long scaler_done(scalerRecord *psr);

SCALERDSET devScaler = {
	7, 
	scaler_report,
	scaler_init,
	scaler_init_record,
	scaler_get_ioint_info,
	scaler_reset,
	scaler_read,
	scaler_write_preset,
	scaler_arm,
	scaler_done
};
epicsExportAddress(dset, devScaler);

STATIC int scaler_total_cards;
STATIC struct scaler_state {
	int card_exists;
	int num_channels;
	int card_in_use;
	int count_in_progress; /* count in progress? */
	unsigned short ident; /* identification info for this card */
	volatile char *localAddr; /* address of this card */
	IOSCANPVT ioscanpvt;
	int done;
	int preset[MAX_SCALER_CHANNELS];
	scalerRecord *psr;
	CALLBACK *pcallback;
};

typedef struct {
	int card;
} devScalerPvt;

STATIC struct scaler_state **scaler_state = 0;

/**************************************************
* scaler_report()
***************************************************/
STATIC long scaler_report(int level)
{
	int card;

	if ((vsc_num_cards <=0) || (scaler_state[0]->card_exists == 0)) {
		printf("    No Joerger VSCxx scaler cards found.\n");
	} else {
		for (card = 0; card < vsc_num_cards; card++) {
			if (scaler_state[card] && scaler_state[card]->card_exists) {
				printf("    Joerger VSC%-2d card %d @ %p, id: %d %s\n",
					scaler_state[card]->num_channels,
					card, 
					scaler_state[card]->localAddr, 
					(unsigned int) scaler_state[card]->ident,
					scaler_state[card]->card_in_use ? "(in use)": "(NOT in use)");
			}
		}
	}
	return (0);
}

/**************************************************
* scaler_shutdown()
***************************************************/
STATIC int scaler_shutdown()
{
	int i;
	for (i=0; i<scaler_total_cards; i++) {
		if (scaler_reset(scaler_state[i]->psr) <0) return(ERROR);
	}
	return(0);
}

static void writeReg16(volatile char *a16, int offset,uint16 value)
{
#ifdef HAS_IOOPS_H
	out_be16((volatile void*)(a16+offset), value);
#else
	volatile uint16 *reg;

	reg = (volatile uint16 *)(a16+offset);
	*reg = value;
#endif
}

static uint16 readReg16(volatile char *a16, int offset)
{
#ifdef HAS_IOOPS_H
	return in_be16((volatile void*)(a16+offset));
#else
	volatile uint16 *reg;
	uint16 value;

	reg = (volatile uint16 *)(a16+offset);
	value = *reg;
	return(value);
#endif
}

static void writeReg32(volatile char *a32, int offset,uint32 value)
{
#ifdef HAS_IOOPS_H
	out_be32((volatile void*)(a32+offset), value);
#else
	volatile uint32 *reg;

	reg = (volatile uint32 *)(a32+offset);
	*reg = value;
#endif
}

static uint32 readReg32(volatile char *a32, int offset)
{
#ifdef HAS_IOOPS_H
	return in_be32((volatile void*)(a32+offset));
#else
	volatile uint32 *reg;
	uint32 value;

	reg = (volatile uint32 *)(a32+offset);
	value = *reg;
	return(value);
#endif
}

/**************************************************
* scalerISR()
***************************************************/
STATIC void scalerISR(int card)
{
	volatile char *addr;
	uint16 value;	

	Debug(5, "%s", "scalerISR: entry\n");
	if ((card+1) > scaler_total_cards) return;

	addr = scaler_state[card]->localAddr;
	/* disable interrupts during processing */
	value = readReg16(addr, IRQ_LEVEL_ENABLE_OFFSET) & 0x7f;
	writeReg16(addr, IRQ_LEVEL_ENABLE_OFFSET, value);

	/* clear interrupt */
	writeReg16(addr,IRQ_RESET_OFFSET, 0);

	/* tell record support the hardware is done counting */
	scaler_state[card]->done = 1;

	/* get the record processed */
	callbackRequest(scaler_state[card]->pcallback);

	/* enable interrupts */
	value = readReg16(addr, IRQ_LEVEL_ENABLE_OFFSET) | 0x80;
	writeReg16(addr, IRQ_LEVEL_ENABLE_OFFSET, value);

	return;
}


/**************************************************
* scalerISRSetup()
***************************************************/
STATIC int scalerISRSetup(int card)
{
	long status;
	volatile char *addr;
	int intLevel;
	
	Debug(5, "scalerISRSetup: Entry, card #%d\n", card);
	if ((card+1) > scaler_total_cards) return(ERROR);
	addr = scaler_state[card]->localAddr;

	status = devConnectInterrupt(intVME, vsc_InterruptVector + card,
		(void *) &scalerISR, (void *) card);
	if (!RTN_SUCCESS(status)) {
		errPrintf(status, __FILE__, __LINE__, "Can't connect to vector %ld\n",
			  vsc_InterruptVector + card);
		return (ERROR);
	}

	/* get interrupt level from hardware, and enable that level in EPICS */
	intLevel = readReg16(addr,IRQ_LEVEL_ENABLE_OFFSET) & 3;
	Debug(5, "scalerISRSetup: Interrupt level %d\n", intLevel);
	status = devEnableInterruptLevel(intVME, intLevel);
	if (!RTN_SUCCESS(status)) {
		errPrintf(status, __FILE__, __LINE__,
			  "Can't enable enterrupt level %d\n", intLevel);
		return (ERROR);
	}
	/* Write interrupt vector to hardware */
	writeReg16(addr,IRQ_VECTOR_OFFSET,(unsigned short) (vsc_InterruptVector + card));
	Debug(5, "scalerISRSetup: Exit, card #%d\n", card);
	return (OK);
}


/***************************************************
* initialize all software and hardware
* scaler_init()
****************************************************/
STATIC long scaler_init(int after)
{
	volatile char *localAddr;
	unsigned long status;
	void *baseAddr;
	int card, i;
	uint32 probeValue = 0;
 
	Debug(2,"scaler_init(): entry, after = %d\n", after);
	if (after || (vsc_num_cards == 0)) return(0);

	/* allocate scaler_state structures, array of pointers */
	if (scaler_state == NULL) {
	scaler_state = (struct scaler_state **)
				calloc(1, vsc_num_cards * sizeof(struct scaler_state *));

		scaler_total_cards=0;
		for (card=0; card<vsc_num_cards; card++) {
		    scaler_state[card] = (struct scaler_state *)
					calloc(1, sizeof(struct scaler_state));
		}
	}

	/* Check out the hardware. */
	for (card=0; card<vsc_num_cards; card++) {
		baseAddr = (void *)(vsc_addrs + card*CARD_ADDRESS_SPACE);

		/* Can we reserve the required block of VME address space? */
		status = devRegisterAddress(__FILE__, atVMEA32, (size_t)baseAddr,
			CARD_ADDRESS_SPACE, (volatile void **)&localAddr);
		if (!RTN_SUCCESS(status)) {
			errPrintf(status, __FILE__, __LINE__,
				"Can't register 0x%x-byte block at address %p\n", CARD_ADDRESS_SPACE, baseAddr);
			return (ERROR);
		}

		if (devReadProbe(4,(volatile void *)(localAddr+DATA_0_OFFSET),(void*)&probeValue)) {
			printf("no VSC card at %p\n",localAddr);
			return(0);
		}
		
		/* Declare victory. */
		Debug(2,"scaler_init: we own 256 bytes starting at %p\n",localAddr);
		scaler_state[card]->localAddr = localAddr;
		scaler_total_cards++;

		/* reset this card */
		writeReg16(localAddr,RESET_OFFSET,0);

		/* get this card's identification */
		scaler_state[card]->ident = readReg16(localAddr,REV_SERIAL_NO_OFFSET);
		Debug(3,"scaler_init: Serial # = %d\n", scaler_state[card]->ident);
		scaler_state[card]->card_exists = 1;

		/* get this card's type (8 or 16 channels?) */
		Debug(2,"scaler_init:Base Address=0x%8.8x\n",(int)baseAddr);
		Debug(2,"scaler_init:Local Address=0x%8.8x\n",(int)localAddr);
		scaler_state[card]->num_channels =  readReg16(localAddr,MODULE_TYPE_OFFSET) & 0x18;
		Debug(3,"scaler_init: nchan = %d\n", scaler_state[card]->num_channels);
		if (scaler_state[card]->num_channels < 8) {
		    scaler_state[card]->card_exists = 0;
		    continue;
		}
		for (i=0; i<MAX_SCALER_CHANNELS; i++) {
			scaler_state[card]->preset[i] = 0;
		}
	}

	Debug(3,"scaler_init: Total cards = %d\n\n",scaler_total_cards);

#ifdef vxWorks
    if (rebootHookAdd(scaler_shutdown) < 0)
		epicsPrintf ("scaler_init: rebootHookAdd() failed\n"); 
#endif

	Debug(3, "%s", "scaler_init: scalers initialized\n");
	return(0);
}

/***************************************************
* scaler_init_record()
****************************************************/
STATIC long scaler_init_record(struct scalerRecord *psr, CALLBACK *pcallback)
{
	int card = psr->out.value.vmeio.card;
	int status;
	devScalerPvt *dpvt;

	Debug(5,"scaler_init_record: card %d\n", card);
	dpvt = (devScalerPvt *)calloc(1, sizeof(devScalerPvt));
	dpvt->card = card;
	psr->dpvt = dpvt;
	if (scaler_state == NULL) {
		recGblRecordError(S_dev_noDevSup,(void *)psr, "");
		return(S_dev_noDevSup);
	}
	scaler_state[card]->psr = psr;

	/* out must be an VME_IO */
	switch (psr->out.type)
	{
	case (VME_IO) : break;
	default:
		recGblRecordError(S_dev_badBus,(void *)psr,
			"devScaler (init_record) Illegal OUT Bus Type");
		return(S_dev_badBus);
	}

	Debug(5,"VME scaler: card %d\n", card);
	if (!scaler_state[card]->card_exists)
	{
		recGblRecordError(S_dev_badCard,(void *)psr,
		    "devScaler (init_record) card does not exist!");
		return(S_dev_badCard);
    }

	if (scaler_state[card]->card_in_use)
	{
		recGblRecordError(S_dev_badSignal,(void *)psr,
		    "devScaler (init_record) card already in use!");
		return(S_dev_badSignal);
    }
	scaler_state[card]->card_in_use = 1;
	psr->nch = scaler_state[card]->num_channels;

	/* setup interrupt handler */
	scaler_state[card]->pcallback = pcallback;
	status = scalerISRSetup(card);

	return(0);
}


/***************************************************
* scaler_reset()
****************************************************/
STATIC long scaler_reset(scalerRecord *psr)
{
	volatile char *addr;
	int i;
	uint16 value;
	devScalerPvt *dpvt = psr->dpvt;
	int card = dpvt->card;

	Debug(5,"scaler_reset: card %d\n", card);
	if ((card+1) > scaler_total_cards) return(ERROR);
	if (!scaler_state[card]->card_exists) return(ERROR);
	addr = scaler_state[card]->localAddr;

	/* disable interrupt */
	value = readReg16(addr, IRQ_LEVEL_ENABLE_OFFSET) & 0x7f;
	writeReg16(addr, IRQ_LEVEL_ENABLE_OFFSET, value);

	/* reset board */
	writeReg16(addr,RESET_OFFSET, 0);

	/* zero local copy of scaler presets */
	for (i=0; i<MAX_SCALER_CHANNELS; i++) {
		scaler_state[card]->preset[i] = 0;
	}

	/* clear hardware-done flag */
	scaler_state[card]->done = 0;

	return(0);
}

/***************************************************
* scaler_read()
* return pointer to array of scaler values (on the card)
****************************************************/
STATIC long scaler_read(scalerRecord *psr, long *val)
{
	devScalerPvt *dpvt = psr->dpvt;
	int card = dpvt->card;
	long preset;
	unsigned long rawval;
	volatile char *addr= scaler_state[card]->localAddr;
	int i, offset;
	unsigned short mask;

	Debug(8,"scaler_read: card %d\n", card);
	if ((card+1) > scaler_total_cards) return(ERROR);
	if (!scaler_state[card]->card_exists) return(ERROR);

	mask = readReg16(addr,DIRECTION_OFFSET);
	Debug(5,"scaler_read: readback of up/down mask: 0x%x\n", mask);
	mask = readReg16(addr,IRQ_MASK_OFFSET);
	Debug(5,"scaler_read: readback of IRQ mask: 0x%x\n", mask);
	for (i=0, offset=DATA_0_OFFSET; i < scaler_state[card]->num_channels; i++, offset+=4) {
	    preset = scaler_state[card]->preset[i];
		rawval = readReg32(addr,offset);
	    val[i] = (mask & (1<<i)) ? preset-rawval:rawval;
		if (i <= (devScalerDebug-19)) {
			logMsg("scaler_read: channel %d\n", i);
	    	logMsg("scaler_read: ...(dir i = %s)\n", (mask & (1<<i)) ? "DN":"UP");
	    	logMsg("scaler_read: ...(preset i = %d)\n", (unsigned int)preset);
	    	logMsg("scaler_read: ...(data i = %d)\n",rawval);
	    	logMsg("scaler_read: ...(chan i = %d)\n\n", (unsigned int)val[i]);
		}
	}
	return(0);
}

/***************************************************
* scaler_write_preset()
****************************************************/
STATIC long scaler_write_preset(scalerRecord *psr, int signal, long val)
{
	devScalerPvt *dpvt = psr->dpvt;
	int card = dpvt->card;
	volatile char *addr= scaler_state[card]->localAddr;
	unsigned short mask;
	int offset= PRESET_0_OFFSET+signal*4;
	int i;

	Debug(5,"scaler_write_preset: card %d\n", card);
	Debug(5,"scaler_write_preset: signal %d\n", signal);
	Debug(5,"scaler_write_preset: val = %d\n", (int)val);

	if ((card+1) > scaler_total_cards) return(ERROR);
	if (!scaler_state[card]->card_exists) return(ERROR);
	if ((signal+1) > MAX_SCALER_CHANNELS) return(ERROR);

	/* write preset; save a copy in scaler_state */
	writeReg32(addr,offset,val-1);
	scaler_state[card]->preset[signal] = val-1;

	/* make the preset scaler count down */
	mask = readReg16(addr,DIRECTION_OFFSET);
	mask |= 1<<signal;
	Debug(5,"scaler_write_preset: setting up/down mask = 0x%x\n", mask);
	writeReg16(addr,DIRECTION_OFFSET,mask);
	if (devScalerDebug >=5) {
		for (i=0; i<10; i++) {
			mask = readReg16(addr,DIRECTION_OFFSET);
			logMsg("scaler_write_preset: readback of up/down mask: 0x%x\n", mask);
		}
	}
	/* enable IRQ from preset channel */
	mask = readReg16(addr,IRQ_MASK_OFFSET);
	Debug(5,"scaler_write_preset: readback of IRQ mask: 0x%x\n", mask);
	mask |= 1<<signal;
	Debug(5,"scaler_write_preset: setting IRQ mask = 0x%x\n", mask);
	writeReg16(addr,IRQ_MASK_OFFSET,mask);
	if (devScalerDebug >=5) {
		for (i=0; i<10; i++) {
			mask = readReg16(addr,IRQ_MASK_OFFSET);
			logMsg("scaler_write_preset: readback of IRQ mask: 0x%x\n", mask);
		}
	}
	return(0);
}

/***************************************************
* scaler_arm()
* Make scaler ready to count.  If ARM output is connected
* to ARM input, and GATE input permits, the scaler will
* actually start counting.
****************************************************/
STATIC long scaler_arm(scalerRecord *psr, int val)
{
	devScalerPvt *dpvt = psr->dpvt;
	int card = dpvt->card;
	volatile char *addr=scaler_state[card]->localAddr;
	short ctrl_data;
	uint16  value;

	Debug(5,"scaler_arm: card %d\n", card);
	Debug(5,"scaler_arm: val = %d\n", val);
	if ((card+1) > scaler_total_cards) return(ERROR);
	if (!scaler_state[card]->card_exists) return(ERROR);

	/* disable interrupt */
	value = readReg16(addr,IRQ_LEVEL_ENABLE_OFFSET) & 0x7f;
	writeReg16(addr,IRQ_LEVEL_ENABLE_OFFSET,value);

	if (val) {
	   /* write the interrupt vector to the board */
	   writeReg16(addr,IRQ_VECTOR_OFFSET,(unsigned short)(vsc_InterruptVector+card));

	   /* enable interrupt-when-done */
	   value = readReg16(addr,IRQ_LEVEL_ENABLE_OFFSET) | 0x80;
	   writeReg16(addr,IRQ_LEVEL_ENABLE_OFFSET,value);
	}

	/* clear hardware-done flag */
	scaler_state[card]->done = 0;

	/* arm scaler */
	ctrl_data = readReg16(addr,CTRL_OFFSET);
	if (val) ctrl_data |= 1; else ctrl_data &= 0x0E;
	writeReg16(addr,CTRL_OFFSET,ctrl_data);
	Debug(5,"scaler_arm: ctrl reg => 0x%x\n",readReg16(addr,CTRL_OFFSET) & 0xf);
	return(0);
}


/***************************************************
* scaler_done()
****************************************************/
STATIC long scaler_done(scalerRecord *psr)
{
	devScalerPvt *dpvt = psr->dpvt;
	int card = dpvt->card;
	if ((card+1) > scaler_total_cards) return(ERROR);

	if (scaler_state[card]->done) {
		/* clear hardware-done flag */
		scaler_state[card]->done = 0;
		return(1);
	} else {
		return(0);
	}
}


/*****************************************************
* VSCSetup()
* User (startup file) calls this function to configure
* us for the hardware.
*****************************************************/
void VSCSetup(int num_cards,	/* maximum number of cards in crate */
	   void *addrs,		/* Base Address(0x100-0xffffff00, 256-byte boundary) */
	   unsigned vector)	/* noninterrupting(0), valid vectors(64-255) */
{
	vsc_num_cards = num_cards;
	vsc_addrs = addrs;
	vsc_InterruptVector = vector;
}

/* debugging function */
void VSCscaler_show(int card)
{
	volatile char *addr = scaler_state[card]->localAddr;
	int i, offset;

	if (vsc_num_cards == 0) {
		printf("VSCscaler_show: No Joerger VSC cards\n");
		return;
	}

	printf("VSCscaler_show: card %d %s\n", card, scaler_state[card]->card_exists ? "exists" : "not found");
	if (!scaler_state[card]->card_exists) return;
	printf("VSCscaler_show: ctrl reg = 0x%x\n", readReg16(addr,CTRL_OFFSET) &0xf);
	printf("VSCscaler_show: dir reg = 0x%x\n",readReg16(addr,DIRECTION_OFFSET) );
	printf("VSCscaler_show: irq vector = 0x%x\n",readReg16(addr,STATUS_ID_OFFSET) &0xff);
	printf("VSCscaler_show: irq level/enable = 0x%x\n",readReg16(addr,IRQ_LEVEL_ENABLE_OFFSET) &0xff);
	printf("VSCscaler_show: irq mask reg = 0x%x\n", readReg16(addr,IRQ_MASK_OFFSET));
	printf("VSCscaler_show: module type = 0x%x\n",readReg16(addr,MODULE_TYPE_OFFSET) &0xff);
	offset = DATA_0_OFFSET;
	for (i=0; i<scaler_state[card]->num_channels; i++, offset+=4 ) {
		printf("    VSCscaler_show: channel %d counts = %d\n", i,readReg32(addr,offset) );
	}
	printf("VSCscaler_show: scaler_state[card]->done = %d\n", scaler_state[card]->done);
}

/* debugging function */
void VSCscaler_debug(int card, int numReads, int waitLoops)
{
	volatile char *addr = scaler_state[card]->localAddr;
	int i, j, offset;

	if (vsc_num_cards == 0) {
		printf("VSCscaler_debug: No Joerger VSC cards\n");
		return;
	}

	printf("VSCscaler_debug: card %d %s\n", card, scaler_state[card]->card_exists ? "exists" : "not found");
	if (!scaler_state[card]->card_exists) return;
	for (i=0; i<numReads; i++) {
		printf("VSCscaler_debug: ctrl reg = 0x%x\n", readReg16(addr,CTRL_OFFSET) &0xf);
		for (j=0; j<waitLoops; j++);
	}
	for (i=0; i<numReads; i++) {
		printf("VSCscaler_debug: dir reg = 0x%x\n",readReg16(addr,DIRECTION_OFFSET) );
		for (j=0; j<waitLoops; j++);
	}
	for (i=0; i<numReads; i++) {
		printf("VSCscaler_debug: irq vector = 0x%x\n",readReg16(addr,STATUS_ID_OFFSET) &0xff);
		for (j=0; j<waitLoops; j++);
	}
	for (i=0; i<numReads; i++) {
		printf("VSCscaler_debug: irq level/enable = 0x%x\n",readReg16(addr,IRQ_LEVEL_ENABLE_OFFSET) &0xff);
		for (j=0; j<waitLoops; j++);
	}
	for (i=0; i<numReads; i++) {
		printf("VSCscaler_debug: irq mask reg = 0x%x\n", readReg16(addr,IRQ_MASK_OFFSET));
		for (j=0; j<waitLoops; j++);
	}
	for (i=0; i<numReads; i++) {
		printf("VSCscaler_debug: rev. ser# reg = 0x%x\n", readReg16(addr,REV_SERIAL_NO_OFFSET));
		for (j=0; j<waitLoops; j++);
	}
	for (i=0; i<numReads; i++) {
		printf("VSCscaler_debug: module type = 0x%x\n",readReg16(addr,MODULE_TYPE_OFFSET) &0xff);
		for (j=0; j<waitLoops; j++);
	}
	for (i=0; i<numReads; i++) {
		printf("VSCscaler_debug: manuf. ID = 0x%x\n",readReg16(addr,MANUFACTURER_ID_OFFSET) &0xff);
		for (j=0; j<waitLoops; j++);
	}
	offset = DATA_0_OFFSET;
	for (i=0; i<2; i++, offset+=4 ) {
		for (j=0; j<numReads; j++) {
			printf("    VSCscaler_debug: channel %d counts = %d\n", i,readReg32(addr,offset) );
		}
	}
	/* Note reading from PRESET_0_OFFSET reads and clears data at DATA_0_OFFSET */
}
