/*******************************************************************************
devScalerVS.c
Device-support routines for Joerger VS (64,32,16)-channel, 32-bit
read-on-the-fly scaler.

Original Author: Tim Mooney
Date: 7/1/03

Experimental Physics and Industrial Control System (EPICS)

Copyright 2003, the University of Chicago Board of Governors.

This software was produced under U.S. Government contract
W-31-109-ENG-38 at Argonne National Laboratory.

Initial development by:
	The Beamline Controls & Data Acquisition Group
	APS Operations Division
	Advanced Photon Source
	Argonne National Laboratory

Modification Log:
-----------------
07/01/03    tmm     Development from devScaler.c
05/17/04    tmm     port to EPICS 3.14 OSI, following Kate Feng's port of
                    devScaler.c
11/17/04    tmm     Enforce min/max count times.  scalerVS_show() prints
                    software version and local address.  scalerVS_show() and
                    scalerVS_regShow() check card arg.  Changed
                    devScaler_VS_trig_retries to 10000 and added
                    devScaler_VS_trig_retry_report.		
10/26/06    tmm     v1.3 Changed dset interface functions to use precord not card.
                    CALLBACK pointer passed in init_record.
                    dpvt now holds card number.  callback structures are now in
                    rpvt, not dpvt.
*******************************************************************************/
#define VERSION 1.3

#ifdef HAS_IOOPS_H
#include        <basicIoOps.h>
#endif

typedef unsigned int uint32;
typedef unsigned short uint16;

#ifdef vxWorks
#include	<epicsExit.h>
extern int logMsg(char *fmt, ...);
#else
#define logMsg errlogPrintf
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
#include	<dbEvent.h>
#include	<recGbl.h>
#include	<recSup.h>
#include	<devSup.h>
#include	<drvSup.h>
#include	<dbScan.h>
#include	<special.h>
#include	<callback.h>

#include	"scalerRecord.h"
#include	"devScaler.h"


#define MAX(a,b) (a)>(b)?(a):(b)

/*** Debug support ***/
#ifdef NODEBUG
#define STATIC static
#define Debug(l,FMT,V) ;
#define Debug2(l,FMT,V1, V2) ;
#else
#define STATIC
#define Debug(l,FMT,V) {  if(l <= devScaler_VSDebug) \
			{ printf("%s(%d):",__FILE__,__LINE__); \
			  printf(FMT,V); } }
#define Debug2(l,FMT,V1,V2) {  if(l <= devScaler_VSDebug) \
		  	   { printf("%s(%d):",__FILE__,__LINE__); \
			     printf(FMT,V1,V2); } }
#endif
volatile int devScaler_VSDebug=0;
volatile int devScaler_VS_check_trig=1;
volatile int devScaler_VS_trig_retries = 10000;
volatile int devScaler_VS_trig_retry_report = 100;
volatile int devScaler_VS_trig_reads = 5;

#define CARD_ADDRESS_SPACE				0x800
#define READ_XFER_REG_OFFSET			0x000
#define READ_CLEAR_XFER_REG_OFFSET		0x100
#define READ_FLY_XFER_REG__OFFSET		0x200
#define STATUS_OFFSET					0x400
#define CTRL_OFFSET						0x402
#define A32BASE_HI_OFFSET				0x404
#define A32BASE_LO_OFFSET				0x406
/* Note the next three are D08(0) -- byte transfer at odd address */
#define IRQ_1_OVERFLOW_VECTOR_OFFSET		0x408
#define IRQ_2_FP_XFER_VECTOR_OFFSET			0x40A
#define IRQ_3_GATE_VECTOR_OFFSET			0x40C
#define IRQ_SETUP_OFFSET				0x40E
#define CLOCK_TRIG_MODE_OFFSET			0x410
#define INTERNAL_GATE_SIZE_OFFSET		0x412
#define REF_CLOCK_MODE_OFFSET			0x414
#define ID_OFFSET						0x41E
#define MASTER_RESET_OFFSET				0x420
#define CLOCK_XFER_REG_OFFSET			0x422
#define COUNT_ENABLE_OFFSET				0x424
#define COUNT_DISABLE_OFFSET			0x426
#define RESET_ALL_COUNTERS_OFFSET		0x428
#define ARM_OFFSET						0x42A
#define DISARM_OFFSET					0x42C
#define TRIG_GATE_OFFSET				0x42E
#define TEST_PULSE_OFFSET				0x430
#define CLEAR_INTERRUPT_2_3_OFFSET		0x432

/* Status register bits */
#define STATUS_ARM_OUT					0x1000
#define STATUS_ARM_IN					0x0800
#define STATUS_FP_GATE					0x0400
#define STATUS_PROGRAMMED_GATE			0x0200
#define STATUS_FP_RESET					0x0100
#define STATUS_IRQ_3_GATE				0x0080
#define STATUS_IRQ_2_FP_XFER			0x0040
#define STATUS_IRQ_1_OVERFLOW			0x0020
#define STATUS_IRQ_3_SOURCE_GATE		0x0010
#define STATUS_IRQ_2_SOURCE_FP_XFER		0x0008
#define STATUS_IRQ_1_SOURCE_OVERFLOW	0x0004
#define STATUS_GLOBAL_COUNT_ENABLE_FF	0x0002
#define STATUS_GLOBAL_COUNT_ENABLE		0x0001

/* Control register bits */
#define CTRL_RESET_ON_FP_XFER			0x0002
#define CTRL_RESET_ON_VME_XFER			0x0001

/* Module types */
struct VS_module_type {
	char type[16];
	char num_channels;
};

/* after subtracting 16 from (module_id>>10), we get an index into this table */
struct VS_module_type VS_module_types[14] = {
	{"VS64 TTL", 64},
	{"VS32 TTL", 32},
	{"VS16 TTL", 16},
	{"VS32 ECL", 32},
	{"VS16 ECL", 16},
	{"VS32 NIM", 32},
	{"VS16 NIM", 16},
	{"VS64D TTL", 64},
	{"VS32D TTL", 32},
	{"VS16D TTL", 16},
	{"VS32D ECL", 32},
	{"VS16D ECL", 16},
	{"VS32D NIM", 32},
	{"VS16D NIM", 16}
};

STATIC long vs_num_cards = 0;
STATIC char *vs_addrs = (char *)0xd000;
STATIC unsigned char vs_InterruptVector = 0;
STATIC int vs_InterruptLevel = 5;

/* device-support entry table */
STATIC long scalerVS_report(int level);
STATIC long scalerVS_init(int after);
STATIC long scalerVS_init_record(scalerRecord *psr, CALLBACK *pcallback);
#define scalerVS_get_ioint_info NULL
STATIC long scalerVS_reset(scalerRecord *psr);
STATIC long scalerVS_read(scalerRecord *psr, long *val);
STATIC long scalerVS_write_preset(scalerRecord *psr, int signal, long val);
STATIC long scalerVS_arm(scalerRecord *psr, int val);
STATIC long scalerVS_done(scalerRecord *psr);
void scalerVS_Setup(int num_cards, int addrs, int vector, int intlevel);
void scalerVS_show(int card, int level);
void scalerVS_regShow(int card, int level);

SCALERDSET devScaler_VS = {
	7, 
	scalerVS_report,
	scalerVS_init,
	scalerVS_init_record,
	scalerVS_get_ioint_info,
	scalerVS_reset,
	scalerVS_read,
	scalerVS_write_preset,
	scalerVS_arm,
	scalerVS_done
};
epicsExportAddress(dset, devScaler_VS);

STATIC int scalerVS_total_cards;
STATIC struct scalerVS_state {
	int card_exists;
	int num_channels;
	int card_in_use;
	int count_in_progress; /* count in progress? */
	unsigned short ident; /* identification info for this card */
	volatile char *localAddr; /* address of this card */
	int done;	/* 1: hardware has finished counting and record hasn't yet reacted */
	int preset[MAX_SCALER_CHANNELS];
	int gate_periods;
	int gate_freq_ix;
	scalerRecord *psr;
	CALLBACK *pcallback;
};
STATIC struct scalerVS_state **scalerVS_state = 0;

typedef struct {
	int card;
} devScalerPvt;

/**************************************************
* scalerVS_report()
***************************************************/
STATIC long scalerVS_report(int level)
{
	int card;

	if (vs_num_cards <= 0) {
		printf("    No Joerger VSnn scaler cards found.\n");
	} else {
		for (card = 0; card < vs_num_cards; card++) {
			if (scalerVS_state[card]) {
				printf("    Joerger VS%-2d card %d @ %p, id: 0x%x %s, vector=%d\n",
					scalerVS_state[card]->num_channels,
					card, 
					scalerVS_state[card]->localAddr, 
					(unsigned int) scalerVS_state[card]->ident,
					scalerVS_state[card]->card_in_use ? "(in use)": "(NOT in use)",
					vs_InterruptVector + card);
			}
		}
	}
	return (0);
}

/**************************************************
* scalerVS_shutdown()
***************************************************/
STATIC int scalerVS_shutdown()
{
	int i;
	for (i=0; i<scalerVS_total_cards; i++) {
		scalerVS_reset(scalerVS_state[i]->psr);
	}
	return(0);
}

static void writeReg16(volatile char *a16, int offset, uint16 value)
{

#ifdef HAS_IOOPS_H
	out_be16((volatile void*)(a16+offset), value);
#else
	volatile uint16 *reg;
	Debug2(16,"devScalerVS:writeReg16: writing 0x%04x to offset 0x%04x\n", value, offset);
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
	Debug2(15,"devScalerVS:readReg16: read 0x%04x from offset 0x%04x\n", value, offset);
    return(value);
#endif
}

/*
static void writeReg32(volatile char *a32, int offset,uint32 value)
{
#ifdef HAS_IOOPS_H
	out_be32((volatile void*)(a32+offset), value);
#else
	volatile uint32 *reg;

	Debug2(16,"devScalerVS:writeReg32: writing 0x%08x to offset 0x%04x\n", value, offset);
    reg = (volatile uint32 *)(a32+offset);
    *reg = value;
#endif
}
*/


static uint32 readReg32(volatile char *a32, int offset)
{
#ifdef HAS_IOOPS_H
	return in_be32((volatile void*)(a32+offset));
#else
    volatile uint32 *reg;
    uint32 value;

    reg = (volatile uint32 *)(a32+offset);
    value = *reg;
	Debug2(17,"devScalerVS:readReg32: read 0x%08x from offset 0x%04x\n", value, offset);
    return(value);
#endif
}

/**************************************************
* scalerEndOfGateISR()
***************************************************/
STATIC void scalerEndOfGateISR(int card)
{
#ifdef vxWorks
	Debug(5, "%s", "scalerEndOfGateISR: entry\n");
#endif
	if (card >= scalerVS_total_cards) {
		return;
	} else {
		/* tell record support the hardware is done counting */
		scalerVS_state[card]->done = 1;

		/* get the record processed */
		callbackRequest(scalerVS_state[card]->pcallback);
	}
}


/**************************************************
* scalerEndOfGateISRSetup()
***************************************************/
STATIC int scalerEndOfGateISRSetup(int card)
{
	long status;
	volatile char *addr;
	volatile uint16 u16;

	Debug(5, "scalerEndOfGateISRSetup: Entry, card #%d\n", card);
	if (card >= scalerVS_total_cards) return(ERROR);
	addr = scalerVS_state[card]->localAddr;

	status = devConnectInterruptVME(vs_InterruptVector + card,
		(void *) &scalerEndOfGateISR, (void *)card);
	if (!RTN_SUCCESS(status)) {
		errPrintf(status, __FILE__, __LINE__, "Can't connect to vector %d\n",
			  vs_InterruptVector + card);
		return (ERROR);
	}

	/* write interrupt level to hardware, and tell EPICS to enable that level */
	u16 = readReg16(addr,IRQ_SETUP_OFFSET) & 0x0ff;
	/* OR in level for end-of-gate interrupt */
	writeReg16(addr,IRQ_SETUP_OFFSET, u16 | (vs_InterruptLevel << 8));
	status = devEnableInterruptLevelVME(vs_InterruptLevel);
	if (!RTN_SUCCESS(status)) {
		errPrintf(status, __FILE__, __LINE__,
			  "Can't enable enterrupt level %d\n", vs_InterruptLevel);
		return (ERROR);
	}
	Debug(5, "scalerEndOfGateISRSetup: Wrote interrupt level, %d, to hardware\n",
	      vs_InterruptLevel);

	/* Write interrupt vector to hardware */
	writeReg16(addr,IRQ_3_GATE_VECTOR_OFFSET, (uint16)(vs_InterruptVector + card));
	Debug(5, "scalerEndOfGateISRSetup: Wrote interrupt vector, %d, to hardware\n",
	      vs_InterruptVector + card);

	Debug(5, "scalerEndOfGateISRSetup: Read interrupt vector, %d, from hardware\n",
	      readReg16(addr,IRQ_3_GATE_VECTOR_OFFSET) & 0x0ff);

	Debug(5, "scalerEndOfGateISRSetup: Exit, card #%d\n", card);
	return (OK);
}


/***************************************************
* initialize all software and hardware
* scalerVS_init()
****************************************************/
STATIC long scalerVS_init(int after)
{
	volatile char *localAddr;
	unsigned long status;
	char *baseAddr;
	int card, card_type;
	uint32 probeValue = 0;

	Debug(2,"scalerVS_init(): entry, after = %d\n", after);
	if (after || (vs_num_cards == 0)) return(0);

	/* allocate scalerVS_state structures, array of pointers */
	if (scalerVS_state == NULL) {
    	scalerVS_state = (struct scalerVS_state **)
				calloc(1, vs_num_cards * sizeof(struct scalerVS_state *));

		scalerVS_total_cards=0;
		for (card=0; card<vs_num_cards; card++) {
		    scalerVS_state[card] = (struct scalerVS_state *)
					calloc(1, sizeof(struct scalerVS_state));
		}
	}

	/* Check out the hardware. */
	for (card=0; card<vs_num_cards; card++) {
		baseAddr = (char *)(vs_addrs + card*CARD_ADDRESS_SPACE);

		/* Can we reserve the required block of VME address space? */
		status = devRegisterAddress(__FILE__, atVMEA16, (size_t)baseAddr,
			CARD_ADDRESS_SPACE, (volatile void **)&localAddr);
		if (!RTN_SUCCESS(status)) {
			errPrintf(status, __FILE__, __LINE__,
				"Can't register 2048-byte block in VME A16 at address %p\n", baseAddr);
			return (ERROR);
		}

		if (devReadProbe(4,(volatile void *)(localAddr+READ_XFER_REG_OFFSET),(void*)&probeValue)) {
			printf("scalerVS_init: no VSxx card at %p\n",localAddr);
			return(0);
		}
		
		/* Declare victory. */
		Debug(2,"scalerVS_init: we own 2048 bytes in VME A16 starting at %p\n", localAddr);
		scalerVS_state[card]->localAddr = localAddr;
		scalerVS_total_cards++;

		/* reset this card */
		/* any write to this address causes reset */
		writeReg16(localAddr,MASTER_RESET_OFFSET,0);
		/* get this card's type and serial number */
		scalerVS_state[card]->ident = readReg16(localAddr,ID_OFFSET);
		Debug(3,"scalerVS_init: Serial # = %d\n", scalerVS_state[card]->ident & 0x3FF);

		/* get this card's type */
		card_type = scalerVS_state[card]->ident >> 10;
		if ((card_type > 22) || (card_type < 16)) {
			errPrintf(status, __FILE__, __LINE__, "unrecognized module\n");
			scalerVS_state[card]->num_channels = 0;
			scalerVS_state[card]->card_exists = 0;
			/*
			 * Something's wrong with this card, but we still count it in scalerVS_total_cards.
			 * A bad card retains its address space; otherwise we can't talk to the next one.
			 */
		} else {
			scalerVS_state[card]->num_channels = VS_module_types[card_type-16].num_channels;
			scalerVS_state[card]->card_exists = 1;
		}
		Debug(3,"scalerVS_init: nchan = %d\n", scalerVS_state[card]->num_channels);
	}

	Debug(3,"scalerVS_init: Total cards = %d\n\n",scalerVS_total_cards);

#ifdef vxWorks
    if (epicsAtExit(scalerVS_shutdown, 0) < 0)
		epicsPrintf ("scalerVS_init: epicsAtExit() failed\n");
#endif
	Debug(3,"%s", "scalerVS_init: scalers initialized\n");
	return(0);
}

/***************************************************
* scalerVS_init_record()
****************************************************/
STATIC long scalerVS_init_record(scalerRecord *psr, CALLBACK *pcallback)
{
	int card = psr->out.value.vmeio.card;
	int status;
	devScalerPvt *dpvt;

	dpvt = (devScalerPvt *)calloc(1, sizeof(devScalerPvt));
	dpvt->card = card;
	psr->dpvt = dpvt;
	scalerVS_state[card]->psr = psr;

	/* out must be an VME_IO */
	switch (psr->out.type) {
	case (VME_IO) : break;
	default:
		recGblRecordError(S_dev_badBus,(void *)psr,
			"devScaler_VS (init_record) Illegal OUT Bus Type");
		return(S_dev_badBus);
	}

	Debug(5,"scalerVS_init_record: card %d\n", card);
	if (!scalerVS_state[card]->card_exists) {
		recGblRecordError(S_dev_badCard,(void *)psr,
		    "devScaler_VS (init_record) card does not exist!");
		return(S_dev_badCard);
    }

	if (scalerVS_state[card]->card_in_use) {
		recGblRecordError(S_dev_badSignal,(void *)psr,
		    "devScaler_VS (init_record) card already in use!");
		return(S_dev_badSignal);
    }
	scalerVS_state[card]->card_in_use = 1;
	psr->nch = scalerVS_state[card]->num_channels;
	/* set hardware-done flag */
	scalerVS_state[card]->done = 1;

	/* setup interrupt handler */
	scalerVS_state[card]->pcallback = pcallback;
	status = scalerEndOfGateISRSetup(card);

	return(0);
}


/***************************************************
* scalerVS_reset()
****************************************************/
STATIC long scalerVS_reset(scalerRecord *psr)
{
	devScalerPvt *dpvt = psr->dpvt;
	int card = dpvt->card;

	Debug(5,"scalerVS_reset: card %d\n", card);
	if (card >= scalerVS_total_cards) return(ERROR);

	/* reset board */
	writeReg16(scalerVS_state[card]->localAddr, MASTER_RESET_OFFSET, 0);

	/* clear hardware-done flag */
	scalerVS_state[card]->done = 0;

	return(0);
}

/***************************************************
* scalerVS_read()
* return pointer to array of scaler values (on the card)
****************************************************/
STATIC long scalerVS_read(scalerRecord *psr, long *val)
{
	devScalerPvt *dpvt = psr->dpvt;
	int card = dpvt->card;
	volatile char *addr;
	int i, offset;

	Debug(8,"scalerVS_read: card %d\n", card);
	if (card >= scalerVS_total_cards) return(ERROR);
	addr = scalerVS_state[card]->localAddr;

	/* any write clocks all transfer registers */
	writeReg16(addr,CLOCK_XFER_REG_OFFSET,1);

	for (i=0, offset=READ_XFER_REG_OFFSET; i < scalerVS_state[card]->num_channels; i++, offset+=4) {
		val[i] = readReg32(addr,offset);
		if (i==0) {
			Debug2(11,"scalerVS_read: ...(chan %d = %ld)\n", i, val[i]);
		} else {
			Debug2(20,"scalerVS_read: ...(chan %d = %ld)\n", i, val[i]);
		}
	}

	Debug2(10,"scalerVS_read: status=0x%x; irq vector=0x%x\n",
		readReg16(scalerVS_state[card]->localAddr,STATUS_OFFSET),
		readReg16(addr,IRQ_3_GATE_VECTOR_OFFSET)&0xff);

	return(0);
}

#define GATE_FREQ_TABLE_LENGTH 12

double gate_freq_table[GATE_FREQ_TABLE_LENGTH] = {
1e7, 5e6, 2.5e6, 1e6, 5e5, 2.5e5, 1e5, 5e4, 2.5e4, 1e4, 1e3, 100
};

int gate_freq_bits[GATE_FREQ_TABLE_LENGTH] = {
0,   3,   4,     5,   6,   7,     8,   9,   10,    11,   12,  13
};

/***************************************************
* scalerVS_write_preset()
* This hardware has no preset capability, but we can set a time gate.
* What we do here is put the hardware in "trigger" mode, set the gate-clock
* frequency and the number of gate-clock periods to count for.  We're going to
* get called once for each channel, but we do all the real work on the first call.
* From then on, we just make sure the presets are zero, and fix them if they aren't.
****************************************************/
STATIC long scalerVS_write_preset(scalerRecord *psr, int signal, long val)
{
 	devScalerPvt *dpvt = psr->dpvt;
	int card = dpvt->card;
	epicsInt32 *ppreset;
	unsigned short *pgate;
	volatile char *addr;
	unsigned short gate_freq_ix;
	double gate_time, gate_freq, gate_periods;

	if (devScaler_VSDebug >= 5) {
		printf("%s(%d):",__FILE__,__LINE__);
		printf("scalerVS_write_preset: card %d, signal %d, val %ld\n", card, signal, val);
	}

	if (card >= scalerVS_total_cards) return(ERROR);
	if (signal >= MAX_SCALER_CHANNELS) return(ERROR);

	addr = scalerVS_state[card]->localAddr;
	callbackGetUser(psr, scalerVS_state[card]->pcallback);

	if (signal > 0) {
		if (val != 0) {
			ppreset = &(psr->pr1);
			ppreset[signal] = 0;
			db_post_events(psr,&(ppreset[signal]),DBE_VALUE);
			pgate = &(psr->g1);
			pgate[signal] = 0;
			db_post_events(psr,&(pgate[signal]),DBE_VALUE);
		}
		return(0);
	}

	if (psr->g1 != 1) {
		psr->g1 = 1;
		db_post_events(psr,&(psr->g1),DBE_VALUE);
	}

	/*** set count time ***/
	gate_time = val / psr->freq;
	/*
	 * find largest gate-clock frequency that will allow us to specify the
	 * requested count time with the 16-bit gate-preset register.  Note that
	 * the scaler counts for one extra clock cycle, so we allow 0xffff + 1
	 * = 65536 cycles.
	 */
	gate_freq_ix = 0;
	do {
		gate_freq = gate_freq_table[gate_freq_ix];
		gate_periods =  gate_time * gate_freq;
		if (devScaler_VSDebug >= 10) {
		        printf("%s(%d):",__FILE__,__LINE__);
			printf("scalerVS_write_preset: try f=%.0f, n=%.0f, ix=%d\n",
				gate_freq, gate_periods, gate_freq_ix);	
		}
	} while ((gate_periods > 65536) && (++gate_freq_ix < GATE_FREQ_TABLE_LENGTH));

	if ((gate_periods < 4) && (gate_freq_ix == 0)) {
		/* The scaler can't count this short.  Just count as short as possible */
		printf("devScaler_VS: min. counting time is 4E-7 seconds.\n");
	}

	/* docs recommend min of 4 periods; we're going to subtract 1 before writing. */
	if (gate_periods < 5) gate_periods = 5;

	if ((gate_periods > 65536) && (gate_freq_ix >= GATE_FREQ_TABLE_LENGTH)) {
		/* The scaler can't count this long.  Just count as long as possible */
		printf("devScaler_VS: max. counting time is 655.36 seconds.\n");
		gate_periods = 65536;
		gate_freq_ix = GATE_FREQ_TABLE_LENGTH - 1;
	}

	/* set clock frequency, and specify that software should trigger gate-start */
	writeReg16(addr, CLOCK_TRIG_MODE_OFFSET, gate_freq_bits[gate_freq_ix] | 0x10);

	/* Set the gate-size register to the number of clock periods to count. */
	/* Scaler must be in trigger mode at the time gate size is written. */
	/* Docs say to specify (desired_clock_periods - 1) */
	writeReg16(addr, INTERNAL_GATE_SIZE_OFFSET, (uint16)(gate_periods-1));

	/* save preset and frequency mask in scalerVS_state */
	scalerVS_state[card]->gate_periods = gate_periods;
	scalerVS_state[card]->gate_freq_ix = gate_freq_ix;

	/* tell record what preset and clock rate we're using  */
	psr->pr1 = gate_periods;
	psr->freq = gate_freq_table[gate_freq_ix];

	Debug2(10,"scalerVS_write_preset: gate_periods=%f, gate_freq=%f\n",
		gate_periods, gate_freq);

	return(0);
}

/***************************************************
* scalerVS_arm()
* Make scaler ready to count.  If ARM output is connected
* to ARM input, and GATE permits, the scaler will
* actually start counting.
****************************************************/
STATIC long scalerVS_arm(scalerRecord *psr, int val)
{
	devScalerPvt *dpvt = psr->dpvt;
	int card = dpvt->card;
	volatile char *addr;
	volatile uint16 u16;
	int i, j, retry, read_again, numBad, numGood;

	Debug2(1, "scalerVS_arm: card %d, val %d\n", card, val);

	if (card >= scalerVS_total_cards) return(ERROR);
	addr = scalerVS_state[card]->localAddr;
	callbackGetUser(psr, scalerVS_state[card]->pcallback);

	/* disable end-of-gate interrupt */
	u16 = readReg16(addr,IRQ_SETUP_OFFSET);
	writeReg16(addr,IRQ_SETUP_OFFSET, u16 & 0x07ff);

	if (val) {
		/*** start counting ***/

		/* reset counters, overflows, and overflow-IRQ source */
		writeReg16(addr,RESET_ALL_COUNTERS_OFFSET, (uint16)1);
		/* clear other IRQ's */
		writeReg16(addr,CLEAR_INTERRUPT_2_3_OFFSET, (uint16)3);

		/** Set up and enable interrupts **/
		/* Write interrupt vector to hardware */
		writeReg16(addr,IRQ_3_GATE_VECTOR_OFFSET, (uint16)(vs_InterruptVector + card));
		Debug(10,"scalerVS_arm: irq vector=%d\n", (int)readReg16(addr,IRQ_3_GATE_VECTOR_OFFSET) & 0x00ff);

		/* set end-of-gate interrupt level, and enable the interrupt */
 		u16 = readReg16(addr, IRQ_SETUP_OFFSET);
		u16 = (u16 & 0x0ff) | (vs_InterruptLevel << 8) | 0x800;
		writeReg16(addr, IRQ_SETUP_OFFSET, u16);


		/*
		 * How do I make sure the internal-gate counter is zero, and that it
		 * won't start counting as soon as ARM goes TRUE?
		 */

		/* clear hardware-done flag */
		scalerVS_state[card]->done = 0;

		/* enable all channels */
		writeReg16(addr, COUNT_ENABLE_OFFSET, 1);	/* any write enables */

		/* arm scaler */
		writeReg16(addr, ARM_OFFSET, 1);	/* any write sets ARM */

		/* Make sure trigger mode is set for internal gate. */
		/* (This was already done in write_preset().  It's ok to do it again.) */
 		u16 = readReg16(addr, CLOCK_TRIG_MODE_OFFSET);
		writeReg16(addr, CLOCK_TRIG_MODE_OFFSET, u16 | 0x0010);

		/* trigger gate */
		if (devScaler_VS_check_trig) {
			for (i=0, retry=1; retry && i<devScaler_VS_trig_retries; i++) {
				writeReg16(addr, TRIG_GATE_OFFSET, 1); /* any write triggers gate */
				/*
				 * Check status register bit 9 to make sure internal gate is open.
				 * Repeat reading until we get the same value devScaler_VS_trig_reads
				 * times in a row.
				 */
				for (read_again = 1; read_again; ) {
					for (j=0, numBad=numGood=0; j<devScaler_VS_trig_reads; j++) {
						u16 = readReg16(addr, STATUS_OFFSET);
						if (u16 & 0x0200) numGood++; else numBad++;
					}
					if (numBad == devScaler_VS_trig_reads) {
						/* we believe the gate did NOT get triggered */
						retry = 1; read_again = 0;
					} else if (numGood == devScaler_VS_trig_reads) {
						/* we believe the gate got triggered */
						retry = 0; read_again = 0;
					}
				}
			}
			if (retry) {
				printf("scalerVS_arm: %d trigger attempts apparently failed\n", i);
			} else if (i >= devScaler_VS_trig_retry_report) {
				Debug(1,"scalerVS_arm: trigger succeeded after %d retries.\n", i);
			}
		} else {
			writeReg16(addr, TRIG_GATE_OFFSET, 1); /* any write triggers gate */
		}

		Debug2(5,"scalerVS_arm: gate open; SR=0x%x; irq vector=%d\n",
			readReg16(addr, STATUS_OFFSET),
			(int)readReg16(addr,IRQ_3_GATE_VECTOR_OFFSET) & 0x00ff);

	} else {
		/*** stop counting ***/
		/* disarm scaler */
		writeReg16(addr, DISARM_OFFSET, 1);	/* any write resets ARM */

		/*
		 * Stop counter (change trigger mode from internal gate to external gate
		 * (external gate should be 1?)
		 */
		u16 = readReg16(addr, CLOCK_TRIG_MODE_OFFSET);
		writeReg16(addr, CLOCK_TRIG_MODE_OFFSET, u16 & 0x000f);

		/* set hardware-done flag */
		scalerVS_state[card]->done = 1;

	}

	return(0);
}


/***************************************************
* scalerVS_done()
* On the first call to this function after the scaler stops counting, we return 1.
* else, return 0.
****************************************************/
STATIC long scalerVS_done(scalerRecord *psr)
{
	devScalerPvt *dpvt = psr->dpvt;
	int card = dpvt->card;

	if (card >= scalerVS_total_cards) return(ERROR);

	if (scalerVS_state[card]->done) {
		/* clear hardware-done flag */
		scalerVS_state[card]->done = 0;
		return(1);
	} else {
		return(0);
	}
}


/*****************************************************
* scalerVS_Setup()
* User (startup file) calls this function to configure
* us for the hardware.
*****************************************************/
void scalerVS_Setup(int num_cards,	/* maximum number of cards in crate */
	int addrs,		/* Base Address(0x800-0xf800, 2048-byte boundary) */
	int vector,	/* valid vectors(64-255) */
	int intlevel
)	
{
	vs_num_cards = num_cards;
	vs_addrs = (char *)addrs;
	vs_InterruptVector = (unsigned char) vector;
	vs_InterruptLevel = intlevel & 0x07;
}

/* debugging function */
void scalerVS_show(int card, int level)
{
	volatile char *addr;
	int i, num_channels, offset, saveDebug;
	char module_type[16];

	printf("scalerVS_show: software version: %f\n", VERSION);
	printf("scalerVS_show: %ld Joerger VS cards in use.\n", vs_num_cards);

	if ((vs_num_cards == 0) || (card >= vs_num_cards)) return;

	addr = scalerVS_state[card]->localAddr;
	saveDebug = devScaler_VSDebug;
	devScaler_VSDebug = 0;
	
	printf("scalerVS_show: card %d\n", card);
	printf("scalerVS_show: local address %p\n", addr);
	printf("scalerVS_show: control reg = 0x%x\n", readReg16(addr,CTRL_OFFSET) & 0x3);
	printf("scalerVS_show: status reg = 0x%x\n", readReg16(addr,STATUS_OFFSET) & 0x1ff);
	printf("scalerVS_show: irq level/enable = 0x%x\n", readReg16(addr,IRQ_SETUP_OFFSET) & 0xfff);
	printf("scalerVS_show: irq 3 (end-of-gate) interrupt vector = %d\n", readReg16(addr,IRQ_3_GATE_VECTOR_OFFSET) & 0xff);

	i = (readReg16(addr,ID_OFFSET) & 0xfc00) >> 10;
	if ((i >= 16) && (i <= 29)) {
		strncpy(module_type, VS_module_types[i-16].type, 15);
		num_channels = VS_module_types[i-16].num_channels;
	} else {
		sprintf(module_type, "unknown(%d)", i);
		num_channels = 0;
	}
	printf("scalerVS_show: module type = %d ('%s'), %d channels, serial # %d\n",
			i, module_type, num_channels, readReg16(addr,ID_OFFSET) & 0x3ff);

	num_channels = level ? scalerVS_state[card]->num_channels : 1;
	for (i=0, offset=READ_XFER_REG_OFFSET; i<num_channels; i++, offset+=4) {
		printf("    scalerVS_show: channel %d xfer-reg counts = %u\n", i, readReg32(addr,offset));
	}
	printf("scalerVS_show: scalerVS_state[card]->done = %d\n", scalerVS_state[card]->done);

	devScaler_VSDebug = saveDebug;
	return;
}

void scalerVS_regShow(int card, int level)
{
	volatile char *addr;
	volatile uint16 stat, ctrl, a32_1, a32_0, irq, clkt, gate, ref;
	volatile unsigned char irq1, irq2, irq3;
	int saveDebug;

	if ((vs_num_cards == 0) || (card >= vs_num_cards)) {
		printf("scalerVS_regShow: no such card.\n");
		return;
	}

	addr = scalerVS_state[card]->localAddr;
	saveDebug = devScaler_VSDebug;
	devScaler_VSDebug = 0;

	stat = readReg16(addr,0x400);
	ctrl = readReg16(addr,0x402);
	a32_1 = readReg16(addr,0x404);
	a32_0 = readReg16(addr,0x406);
	irq1 = readReg16(addr,0x408)&0xff;
	irq2 = readReg16(addr,0x40A)&0xff;
	irq3 = readReg16(addr,0x40C)&0xff;
	irq = readReg16(addr,0x40E);
	clkt = readReg16(addr,0x410);
	gate = readReg16(addr,0x412);
	ref = readReg16(addr,0x414);
	
	if (level>1) printf("scalerVS_regShow: 0400 0402 0404 0406 0408 040A 040C 040E 0410 0412 0414\n");
	if (level>0) printf("scalerVS_regShow: STAT CTRL A321 A320 IRQ1 IRQ2 IRQ3 IRQ* CLKT GATE  REF\n");
	printf("scalerVS_regShow: %04hX %04hX %04hX %04hX %04hX %04hX %04hX %04hX %04hX %04hX %04hX\n",
		stat, ctrl, a32_1, a32_0, (uint16)irq1, (uint16)irq2, (uint16)irq3, irq, clkt, gate, ref);
	
	if (level>2) {
		printf("   STAT         CTRL                IRQ*       CLKT      REF\n");
		printf("15 -            -                   -          -        -\n");
		printf("14 -            -                   -          -        -\n");
		printf("13 -            -                   -          -        -\n");
		printf("12 FPArmOut     -                   -          -        -\n");
		printf("\n");
		printf("11 FPArmIn      -                   GateEnbl   -        -\n");
		printf("10 FPGateIn     -                   Gate_2     -        -\n");
		printf("09 ProgGate     -                   Gate_1     -        -\n");
		printf("08 FPReset      -                   Gate_0     -        -\n");
		printf("\n");
		printf("07 Gate         -                   FPXferEnbl -        -\n");
		printf("06 FPXfer       -                   FPXfer_2   -        -\n");
		printf("05 Ovflo        -                   FPXfer_1   -        -\n");
		printf("04 GateSrc      -                   IFPXfer_0  TrigMode -\n");
		printf("\n");
		printf("03 FPXferSrc    -                   OfloEnbl   Freq_3   EnblRefClk\n");
		printf("02 OvfloSrc     -                   OfloLev_2  Freq_2   Freq_2\n");
		printf("01 GblCntEnblFF EnblGblResOnFPXClk  OfloLev_1  Freq_1   Freq_1\n");
		printf("00 GblCntEnbl   EnblGblResOnVMEXClk OfloLev_0  Freq_0   Freq_0\n");
	}
	devScaler_VSDebug = saveDebug;

}
