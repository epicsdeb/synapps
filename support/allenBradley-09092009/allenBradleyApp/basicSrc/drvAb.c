/* drvAb.c -  Driver Support Routines for Allen Bradley */
/*
 * Interface to the Allen-Bradley Remote Serial IO
 *
 * 	Author:	Bob Dalesio
 * 	Date:	6-21-88
 *	Major Revision March 1995. Bob Dalesio and Marty Kraimer
 */
/*************************************************************************
* Copyright (c) 2003 The University of Chicago, as Operator of Argonne
* National Laboratory, the Regents of the University of California, as
* Operator of Los Alamos National Laboratory, and Leland Stanford Junior
* University, as the Operator of Stanford Linear Accelerator.
* This code is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
*************************************************************************/

#include <vxWorks.h>
#include <vxLib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sysLib.h>
#include <semLib.h>
#include <wdLib.h>
#include <vme.h> 
#include <rebootLib.h> 
#include <logLib.h>
#include <tickLib.h>

#include "task_params.h"
#include "dbScan.h"
#include "dbAccess.h"
#include "errlog.h"
#include "drvSup.h"
#include "taskwd.h"
#include "devLib.h"
#include "drvAb.h"
#include "callback.h"
#include <epicsExport.h>


#define maxCmdTrys		3

#define DEFAULT_BASE_ADDR 0xc00000 /*default base addr of first scanner*/
#define DEFAULT_VEC_BASE     0x60 /*default vector for first scanner*/
#define DEFAULT_INT_LEVEL       5 /*default interrupt level*/
#define AB_MAX_ADAPTERS         8 /* number of physical adapters on a link */
#define AB_MAX_CARDS           16 /* max number of IO cards per adapter */
#define MAX_AB_ADAPTERS		8
#define MAX_GROUPS_PER_ADAPTER	8
#define MAX_CARDS_PER_ADAPTER 	16

/*Definitions for dual ported memory on scanner interface card*/
/* Command Word */
#define AUTO_CONF	0x10
#define SCAN_LIST	0x11
#define SET_UP		0x13
#define SET_MODE	0x20
#define	LINK_STATUS	0x21
#define AB_READ		0x01
#define AB_WRITE	0x02
/* osw - operating status word definitions */
#define	PROGRAM_MODE	0x01		/* program/reset mode */
#define TEST_MODE	0x02		/* test/reset mode */
#define RUN_MODE	0x04		/* run mode */
#define DEBUG_MODE	0x08		/* we are debugging */
#define UNSOLICITED_BT	0x10		/* detected block xfer we didn't want */
#define BTS_QUEUED	0x20		/* block xfers queued */
#define ADAPTER_FAULT	0x40		/* at least one faulted adapter list */
#define ADAP_FLT_CHNG	0x80		/* Adapter status change*/
/* Confirmation Status Word */
#define SCANNER_POWERUP	0x90		/* power up status */
#define BT_ACCEPTED	0x2f		/* block trans accepted by scanner */
#define BT_QUEUE_FULL	0x14		/* Queue Full*/
#define	BT_TIMEOUT	0x23		/* block transfer timeout */
/* these are used for the SET_UP command */
#define DEF_RATE	0x01		/* default baud rate 57.6K */
#define FAST_RATE	0x02		/* 115.2 KB */
#define NO_CHANGE	0xff		/* no change */
#define DEBUG		0x01		/* debug - turn off watchdog timer */
#define AB_INT_ENABLE	0x00		/* interrupt enabled */
#define AB_SYSFAIL_DISABLE	0x01	/* disable  VMEbus SYSFAIL signal */

/* DUAL PORTED MEMORY AREA DEFINITION */
/* mail box definition */
typedef volatile struct dp_mbox{
	unsigned short	conf_stat;	/* confirmation status word */
	unsigned short	command;	/* command word */
	unsigned short	address;	/* module address */
	unsigned short	bt_tag;		/* block transfer tag number */
	unsigned short	resv[9];	/* 18 bytes reserved */
	unsigned char	dummy;
	unsigned char  	fl_lock;	/* semaphore word byte */
	unsigned short	data_len;	/* length of data sent/returned */
	unsigned short	msg[64];	/*mail box message */
}dp_mbox;

/* entire region with the mailbox */
typedef volatile struct ab_region {
	unsigned short	oit[64];	/* output image table */
	unsigned short	iit[64];	/*  input image table */
	unsigned short	osw;		/* operating status word */
	dp_mbox 	mail;		/* dual port mail box */
	unsigned short	gda[1872-66];	/* unused part gen data area */
	unsigned short	sys_fail_set1;	/* 1st byte to recover from SYSFAIL */
	unsigned short	sys_fail_set2;	/* 2nd byte to recover from SYSFAIL */
	unsigned char 	vmeid[60];	/* scanner id */
	unsigned short	sc_intr;	/* to interrupt the scanner */
	unsigned short	sc_pad;		/* last word in scanner shared mem */
}ab_region;

typedef enum {
	stateInit,stateInitBTwait,stateCountdown,stateBT,stateBTwait
}scanState;
typedef struct scanInfo{
	scanState	state;
	unsigned short	update_rate;
	unsigned short	*pread_msg;
	unsigned short	*pwrite_msg;
	unsigned short	read_msg_len;
	unsigned short	write_msg_len;
	unsigned short	state_cnt;	/* number of consecutive time in state*/
	unsigned short	down_cnt;	/* down_counter for scanning */
	unsigned short	bt_to;		/* data transfer timed out */
	unsigned short	bt_fail;	/*block transfer bad status*/
	unsigned short	init_ctr;	/* number times the card initialized*/
}scanInfo;

typedef struct ab_card{
	unsigned short	link;
	unsigned short	adapter;
	unsigned short	card;
	cardType	type;
	const char	*card_name;
	abNumBits	nBits;
	BOOL		needsUpdate;
	BOOL		active;
	abStatus	status;
	SEM_ID		card_sem;
	struct btInfo	*pbtInfo;
	scanInfo	*pscanInfo;
	void		*userPvt;
	void (*callback)(void *pcard);		/*user's callback routine*/
	void (*bocallback)(void *pcard);	/*user's callback routine*/
	unsigned long	diprev;
}ab_card;

typedef enum {twoSlot=0,oneSlot,halfSlot} addressingMode;
                      
typedef struct ab_adapter{
	unsigned short	adapter;
	addressingMode	addr_mode;
	BOOL		adapter_online;	
	BOOL		adapter_status_change;
	ab_card		**papcard; /*pointer to array of pointers*/
}ab_adapter;

typedef struct ab_config{
	void		*base_address;
	unsigned short	baud_rate;
	unsigned short	int_vector;
	unsigned short	int_level;
	unsigned short	autoconfig;
	unsigned short	scan_list_len;
	unsigned char	scan_list[64];
}ab_config;

typedef struct ab_link_status{
	unsigned short	list_len;
	unsigned short	status[32];
	unsigned char	list[64];
}ab_link_status;

typedef struct ab_link{
	unsigned short	link;
	unsigned short	ab_disable;
	SEM_ID		request_sem;	/* request talk to scanner*/
	SEM_ID		ab_cmd_sem;	/* transfer complete semaphore */
	SEM_ID		ab_data_sem;	/* transfer complete semaphore */
	int		abScanId;	/* id of the Allen-Bradley scan task */
	int		abDoneId;	/* id of the Allen-Bradley done task */
	unsigned long	sclock_to;	/* Timeout trying to lock mailbox*/
	unsigned long	sccmd_to;	/* command timeout  on semTake*/
	unsigned long	sc_fail;	/* Scanner Command Failure*/
	unsigned short	intr_cnt;	/* interrupt counter*/
	unsigned short	intrSec;	/* interrupts per second	*/
	unsigned short	bt_cnt;		/* Block Transfer counter*/
	unsigned short	btSec;		/* block transfers per second*/
	ab_config	*pconfig;
	char	firmware_info[96];	/* NOTE:  available only temporarily */
	ab_region 	*vme_addr;	/* ptr to the memory of interface */
	ab_adapter 	**papadapter;
	ab_link_status *plink_status;
	BOOL		initialCallback;
	BOOL		initialized;
        BOOL 		sc_locked; /* Does sc_lock own the lock*/
}ab_link;

/*definitions for btRead and btWrite requests from client code */
typedef enum {
	btCallbackRetry,btCallbackTimeout
}btCallbackReason;

/* definitions for btRead and btWrite requests*/
typedef struct btCallbackInfo {
    CALLBACK	callback;
    ab_card	*pcard;
} btCallbackInfo;

static void btCallback(CALLBACK *pCALLBACK);

typedef struct btInfo{
	abStatus	btStatus;
	unsigned short	cmd;
	unsigned short	msg_len;
	unsigned short	*pmsg;
	/* The following fields are only used for cardType = typeBt */
	WDOG_ID		wdId;	/*For Timeout */
	int		reTrys;
	CALLBACK	callback;
	btCallbackReason reason;
        BOOL		btActive;
}btInfo;

/*Local variables and function prototypes*/
LOCAL ab_link		**pab_links=NULL;	
LOCAL unsigned int	max_ab_6008s = 2;

/* forward references */
LOCAL void *abCalloc(size_t nobj,size_t size);
LOCAL ab_link *allocLink(unsigned short link);
LOCAL ab_adapter *allocAdapter(unsigned short link,unsigned short adapter);
LOCAL void computeGroupSlot(ab_adapter *padapter, unsigned short card,
    unsigned short *group,unsigned short *slot);
LOCAL unsigned short computeAddress(ab_adapter *padapter, unsigned short card);
LOCAL unsigned short computeCard(ab_adapter *padapter, unsigned short address);
void allenBradleyIntHandler(void *parg);
LOCAL int  sc_lock(ab_link *plink);
LOCAL int  sc_waitcmd(ab_link *plink);
LOCAL void sc_conferr(ab_link *plink);
LOCAL void sc_unlock(ab_link *plink);
LOCAL void uctrans (unsigned char *from,unsigned char *to,unsigned short n);
LOCAL void ustrans (volatile unsigned short *from,volatile unsigned short *to,
	unsigned short nwords);
LOCAL void di_read(ab_card *pcard,unsigned long *pvalue);
LOCAL void do_read(ab_card *pcard,unsigned long *pvalue);
LOCAL void do_write(ab_card *pcard,unsigned long value);
LOCAL abStatus  bt_queue(unsigned short command,ab_card *pcard,
		unsigned short *pmsg, unsigned short msg_len);
LOCAL int  link_status(ab_link *plink);
LOCAL int  ab_reboot_hook(int boot_type);
LOCAL void config_init(ab_link *plink);
LOCAL int  ab_driver_init();
LOCAL int  link_init(ab_link *plink);
LOCAL void abScanTask(ab_link *plink);
LOCAL void abDoneTask(ab_link *plink);
LOCAL void read_ab_adapter(ab_link *plink, ab_adapter *padapter);
LOCAL void ab_reset_task(ab_link *plink);
LOCAL long ab_io_report(int level);

/*Global variables */
int	ab_debug=0;

LOCAL char *statusMessage[] = {
	"Success","New Card","Card Conflict","No Card",
	"Card not initialized","block transfer queued",
	"Card Busy","Timeout","Adapter Down","Failure"};
char **abStatusMessage = statusMessage;

LOCAL char *numBitsMessage[] = {
	"nbits_Not_defined"," 8_Bit","16_Bit","32_Bit"};
char **abNumBitsMessage = numBitsMessage;
/*End Global variables*/

/*Beginning of DSET routines */

LOCAL long report();
LOCAL long init();
struct {
	long	number;
	DRVSUPFUN	report;
	DRVSUPFUN	init;
} drvAb={
	2,
	report,
	init
};
epicsExportAddress(drvet,drvAb);

LOCAL long report(int level)
{
   return(ab_io_report(level));
}
LOCAL long init()
{

    return(ab_driver_init());
}

LOCAL void *abCalloc(size_t nobj,size_t size)
{
    void *p;

    p=calloc(nobj,size);
    if(p) return(p);
    printf("drvAb: calloc failure\n");
    taskSuspend(0);
    return(NULL);
}
LOCAL ab_link *allocLink(unsigned short link)
{
    ab_link *plink;

    if(pab_links[link]) return(pab_links[link]);
    plink = abCalloc(1,sizeof(ab_link));
    plink->ab_disable = TRUE;
    plink->papadapter = abCalloc(MAX_AB_ADAPTERS,sizeof(ab_adapter *));
    plink->plink_status = abCalloc(1,sizeof(ab_link_status));
    plink->link = link;
    config_init(plink);
    pab_links[link] = plink;
    return(plink);
}

LOCAL ab_adapter *allocAdapter(unsigned short link,unsigned short adapter)
{
    ab_link *plink;
    ab_adapter *padapter;

    if(!pab_links[link]) allocLink(link);
    plink = pab_links[link];
    padapter = plink->papadapter[adapter];
    if(!padapter) {
	padapter = abCalloc(1,sizeof(ab_adapter));
	padapter->papcard = abCalloc(MAX_CARDS_PER_ADAPTER,sizeof(ab_card *));
	padapter->adapter = adapter;
	plink->papadapter[adapter] = padapter;
    }
    return(padapter);
}

/*compute group/slot for accessing I/O image table */
LOCAL void computeGroupSlot(ab_adapter *padapter, unsigned short card,
    unsigned short *group,unsigned short *slot)
{
    switch(padapter->addr_mode) {
        case twoSlot: *group = card/2; *slot = card - *group*2; break;
        case oneSlot: *group = card;  *slot = 0; break;
        case halfSlot: *group = card*2 ; *slot = 0; break;
        default :
            *group = 0; *slot = 0;
            printf("drvAb computeGroupSlot illegal addr_mode %u\n",
                padapter->addr_mode);
                break;
    }
    return;
}

/*compute BT address of card */
LOCAL unsigned short computeAddress(ab_adapter *padapter, unsigned short card)
{
    unsigned short address;

    address  = (padapter->adapter << 4);
    switch(padapter->addr_mode) {
        case twoSlot: address += card; break;
        case oneSlot: address += card*2; break;
        case halfSlot: address += card*4; break;
        default :
            printf("drvAb computeAddresss illegal addr_mode %u\n",
                padapter->addr_mode);
                break;
    }
    return(address);
}

/* compute card from BT address */
LOCAL unsigned short computeCard(ab_adapter *padapter, unsigned short address)
{
    unsigned short card = 0;

    address &= 0x0f;
    switch(padapter->addr_mode) {
        case twoSlot: card = address; break;
        case oneSlot: card = address/2; break;
        case halfSlot: card = address/4; break;
        default :
            logMsg("drvAb computeCard illegal addr_mode %u\n",
                padapter->addr_mode,0,0,0,0,0);
                break;
    }
    return(card);
}

/*
 * AB_INTR
 *
 * The Allen-Bradley protocol requires that an interrupt be received when
 * a block transfer request is given to the scanner board through the dual
 * ported memory and then another when the command is complete.
 * dual-ported memory lock is controlled in a much different fashion than it is.
 */
void allenBradleyIntHandler(void *parg)
{
    int                 link = (int)parg;
    ab_link		*plink;
    ab_region		*p6008;
    dp_mbox		*pmb;

    if(!pab_links) return; /*Called before init complete*/
    plink = pab_links[link];
    if(!plink) return; /*Called before init complete*/
    p6008 = plink->vme_addr;
    if(!p6008) return;
    pmb = &p6008->mail;
    if((pmb->fl_lock&0x80)==0) {/*Should NEVER be True*/
        if(ab_debug!=0)
            logMsg("allenBradleyIntHandler link %d fl_lock not locked\n",
                link,0,0,0,0,0);
        pmb->fl_lock = 0;
        return;
    }
    if(p6008->osw & UNSOLICITED_BT){
        if(ab_debug!=0)
            logMsg("allenBradleyIntHandler link %x, unsolicited_block xfer\n",
                link,0,0,0,0,0);
	/* scanner depends on us to clear some bits */
	p6008->osw = 0;
        pmb->fl_lock = 0;
        return;
    }
    plink->intr_cnt++;
    if(ab_debug!=0) {
        if((pmb->conf_stat != BT_ACCEPTED) && (pmb->conf_stat != BT_QUEUE_FULL)
        && (pmb->conf_stat != 0)) {
            logMsg("allenBradleyIntHandler link %d conf_stat %x "
                "command %x address %x bt_tag %x\n",
                link,pmb->conf_stat, pmb->command,pmb->address,pmb->bt_tag,0);
        }
    }
    if(plink->sc_locked) {
    	semGive(plink->ab_cmd_sem);
    } else {
	semGive(plink->ab_data_sem);
    }
}

/*Routines for communication with scanner*/
LOCAL int sc_lock(ab_link *plink)
{
    ab_region		*p6008 = plink->vme_addr;
    dp_mbox		*pmb = &p6008->mail;
    unsigned short	lock_stat = 0;
    int			i;

    if(ab_debug>3)printf("sc_lock\n");
    semTake(plink->request_sem,WAIT_FOREVER);
    /* try to to lock the dual port memory */
    for(i=0; i<sysClkRateGet()*3; i++) {
	if ((lock_stat = sysBusTas ((char *)&pmb->fl_lock)) == TRUE) break;
	taskDelay(1);
        if(ab_debug) {
            if((i>0) && ((i%sysClkRateGet())==0))
                printf("drvAb link %x sysBusTas failed\n",plink->link);
        }
    }
    if(lock_stat == FALSE)lock_stat = sysBusTas ((char *)&pmb->fl_lock);
    if(lock_stat == FALSE) {
	if(ab_debug) {
            printf("drvAb: link %x sc_lock failure fl_lock %x\n",
                plink->link,pmb->fl_lock);
        }
	plink->sclock_to += 1;
	pmb->fl_lock = 0; /*Unlock to attempt to recover. May not work*/
	semGive(plink->request_sem);
	return (ERROR);
    }
    plink->sc_locked=TRUE;
    return(0);
}

LOCAL int sc_waitcmd(ab_link *plink)
{
    ab_region		*p6008 = plink->vme_addr;
    dp_mbox		*pmb = &p6008->mail;
    unsigned short	savecmd;
    int			status;

    if(ab_debug>3)printf("sc_waitcmd\n");
    savecmd = pmb->command;
    pmb->conf_stat = 0x000f;
    p6008->sc_intr = 1; /*Wake up scanner*/
    status = semTake(plink->ab_cmd_sem,sysClkRateGet()*3);
    if(status!=OK) {
	plink->sccmd_to++;
	if(ab_debug) printf(
	    "drvAb: sc_waitcmd timeout link %hu command %4x\n",
	    plink->link,savecmd);
	sc_unlock(plink);
	return(ERROR);
    }
    if(pmb->command != savecmd){
	plink->sc_fail++;
	if(ab_debug) printf(
	    "drvAb: bad cmd response. link %hu sent %4x received %4x\n",
	    plink->link,savecmd,pmb->command);
	sc_unlock(plink);
	return(ERROR);
    }
    return(0);
}

LOCAL void sc_conferr(ab_link *plink)
{
    ab_region	*p6008 = plink->vme_addr;
    dp_mbox	*pmb = &p6008->mail;

    plink->sc_fail++;
    if(ab_debug) printf("drvAb: link %hu conf_stat error %4x\n",
	plink->link,pmb->conf_stat);
    sc_unlock(plink);
    return;
}

LOCAL void sc_unlock(ab_link *plink)
{
    ab_region	*p6008 = plink->vme_addr;
    dp_mbox	*pmb = &p6008->mail;

    if(ab_debug>3)printf("sc_unlock\n");
    plink->sc_locked=FALSE;
    pmb->fl_lock = 0;
    semGive(plink->request_sem);
}

/*Routines to transfer data to/from mailbox*/
/* Unsigned character transfer*/
LOCAL void uctrans (unsigned char *from,unsigned char *to,unsigned short n)
{
	int	i;
	for (i=0;i<n;i++)
		*to++ = *from++;
}

/* Unsigned short transfer*/
LOCAL void ustrans (volatile unsigned short *from,volatile unsigned short *to,
	unsigned short nwords)
{
	int	i;
	for (i=0;i<nwords;i++)
		*to++ = *from++;
}

/* read the values from the hardware into the local word */
LOCAL void di_read(ab_card *pcard,unsigned long *pvalue)
{
	unsigned short 	link = pcard->link;
	unsigned short	adapter = pcard->adapter;
	ab_link		*plink = pab_links[pcard->link];
	ab_adapter	*padapter = plink->papadapter[pcard->adapter];
	unsigned short	card = pcard->card;
	unsigned short	group,slot;
	unsigned short	rack_offset;
	unsigned long	value;
	volatile unsigned short	*pimage;

	rack_offset =  (adapter * MAX_GROUPS_PER_ADAPTER);
        computeGroupSlot(padapter,card,&group,&slot);
	pimage = &(pab_links[link]->vme_addr->iit[rack_offset + group]);
	if (pcard->nBits == abBit8) {
	    value = *pimage;
	    if(slot==1) value >>= 8;
	    value &= 0x00ff;
	    *pvalue = value;
	}else if(pcard->nBits == abBit16){
	    *pvalue = *pimage;
	} else if (pcard->nBits == abBit32) {
	    value = *(pimage+1);
	    value <<= 16;
	    value += *pimage;
	    *pvalue = value;
	}
}

/* read the values from the hardware into the local word */
LOCAL void do_read(ab_card *pcard,unsigned long *pvalue)
{
	unsigned short  link = pcard->link;
	unsigned short	adapter = pcard->adapter;
	ab_link		*plink = pab_links[pcard->link];
	ab_adapter	*padapter = plink->papadapter[pcard->adapter];
	unsigned short	card = pcard->card;
	unsigned short	group,slot;
	unsigned short	rack_offset;
	unsigned long	value;
	volatile unsigned short	*pimage;

	rack_offset =  (adapter * MAX_GROUPS_PER_ADAPTER);
        computeGroupSlot(padapter,card,&group,&slot);
	pimage = &(pab_links[link]->vme_addr->oit[rack_offset + group]);
	if (pcard->nBits == abBit8) {
	    value = *pimage;
	    if(slot==1) value >>= 8;
	    value &= 0x00ff;
	    *pvalue = value;
	}else if(pcard->nBits == abBit16){
	    *pvalue = *pimage;
	} else if (pcard->nBits == abBit32) {
            /* for 1 slot addressing 32 bit modules use even/odd pair*/
            if(group%2) --pimage;
	    value = *(pimage+1);
	    value <<= 16;
	    value += *pimage;
	    *pvalue = value;
	}
}

/* write the values to the hardware from the local word */
LOCAL void do_write(ab_card *pcard,unsigned long value)
{
	unsigned short  link = pcard->link;
	unsigned short	adapter = pcard->adapter;
	ab_link		*plink = pab_links[pcard->link];
	ab_adapter	*padapter = plink->papadapter[pcard->adapter];
	unsigned short	card = pcard->card;
	unsigned short	group,slot;
	unsigned short	rack_offset;
	volatile unsigned short	*pimage;

	rack_offset =  (adapter * MAX_GROUPS_PER_ADAPTER);
        computeGroupSlot(padapter,card,&group,&slot);
	pimage = &(pab_links[link]->vme_addr->oit[rack_offset + group]);
	if (pcard->nBits == abBit8) {
	    if(slot==0) {
		*pimage = (*pimage & 0xff00) | ((unsigned short)value & 0x00ff);
	    } else {
		value <<= 8;
		*pimage = (*pimage & 0x00ff) | ((unsigned short)value & 0xff00);
	    }
	}else if(pcard->nBits == abBit16){
	    *pimage = (unsigned short)value;
	} else if (pcard->nBits == abBit32) {
            /* for 1 slot addressing 32 bit modules use even/odd pair*/
            if(group%2) --pimage;
	    *pimage = (unsigned short)value;
	    *(pimage+1) = (unsigned short)(value >> 16);
	}
}

LOCAL abStatus  bt_queue(unsigned short command,ab_card *pcard,
		unsigned short *pmsg, unsigned short msg_len)
{
	ab_link		*plink = pab_links[pcard->link];
	ab_adapter	*padapter = plink->papadapter[pcard->adapter];
	btInfo		*pbtInfo = pcard->pbtInfo;
	ab_region	*p6008 = plink->vme_addr;
	dp_mbox 	*pmb = &p6008->mail;
	volatile unsigned short	*pmb_msg = &pmb->msg[0];
	int		status;

        if(plink->ab_disable) return(abAdapterDown);
	if(!padapter->adapter_online) return(abAdapterDown);
	status = sc_lock(plink);
	if(status) return(abFailure);
	pbtInfo->cmd = command;
	pbtInfo->pmsg = pmsg;
	pbtInfo->msg_len = msg_len;
	pmb->command = command;
	pmb->address = computeAddress(padapter,pcard->card);
	pmb->data_len = msg_len;
	if(pbtInfo->cmd==AB_WRITE) ustrans(pmsg,pmb_msg,msg_len);
	status = sc_waitcmd(plink);
	if(status) return(abFailure);
	status = pmb->conf_stat;
	if(status != BT_ACCEPTED){
	    sc_conferr(plink);
	    if (status == BT_QUEUE_FULL){
		printf("drvAb: BT_QUEUE_FULL link %hu\n",plink->link);
		taskDelay(sysClkRateGet()/10);
	    }
	    return(abFailure);
	}
	pbtInfo->btStatus = abBtqueued;
	pbtInfo->btActive = TRUE;
	sc_unlock(plink);
	return(abBtqueued);
}

/*
 * LINK_STATUS
 *
 * Fetches the status of the adapters on the specified link
 * The ab_adapter_status table is used to determine hardware communication
 * errors and convey them to the database
 */
LOCAL int link_status(ab_link *plink)
{
    short		i;
    ab_adapter	 	*padapter;
    dp_mbox		*pmb;
    ab_link_status	*plink_status = plink->plink_status;
    int			status;
    int			ntry;
    int			linkOK = TRUE;

    /* initialize the pointer to the dual ported memory */
    pmb = (dp_mbox *)(&plink->vme_addr->mail);
    /* get the link status */
    for(ntry=0; ntry<maxCmdTrys; ntry++) {
	status = sc_lock(plink);
        if(status && (ab_debug>0))
            printf("link_status: sc_lock failed %d\n",status);
	if(status) continue;
	pmb->command = LINK_STATUS;
	pmb->data_len = 0;
	status = sc_waitcmd(plink);
        if(status && (ab_debug>0))
            printf("link_status: sc_waitcmd failed %d\n",status);
	if(status) continue;
	if(pmb->conf_stat != 0) {
	    sc_conferr(plink);
	    continue;
	}
	plink_status->list_len = pmb->data_len;
	ustrans(pmb->msg,plink_status->status,32);
	uctrans((unsigned char *)(pmb->msg+32),plink_status->list,64);
	sc_unlock(plink);
	break;
    }
    if(ntry>=maxCmdTrys) {
	if(ab_debug)printf("abDrv: link_status failed link %hu\n",plink->link);
	linkOK = FALSE;
    }
    /* check each adapter on this link */
    for (i = 0; i< AB_MAX_ADAPTERS; i++){
	padapter = plink->papadapter[i];

	if(!padapter) continue;
	/* good status */
	if (linkOK && plink_status->status[i*4] & 0x70){
	    if (!padapter->adapter_online) {
		printf("link %d adapter %d change bad to good\n",
			plink->link,i);
		padapter->adapter_status_change=TRUE;
	    }
	    padapter->adapter_online = TRUE;
	}else { /* bad status */
	    if (padapter->adapter_online){
		printf("link %d adapter %d change good to bad\n",
			plink->link,i);
		padapter->adapter_status_change=TRUE;
	    }
	    padapter->adapter_online = FALSE;
	}
    }
    return(0);
}

/* ab_reboot_hook - routine to call when IOC is rebooted with a control-x */
LOCAL int ab_reboot_hook(int boot_type)
{
	short	i;
	ab_link	*plink;
	volatile int counter;
	long ind;

	/* Stop communication to the Allen-Bradley Scanner Cards */
	/* delete the scan task stops analog input communication */
	for (i=0; i<max_ab_6008s; i++) {
	    plink = pab_links[i];
	    if(plink) {
		pab_links[i]->ab_disable = 1;
		if (plink->initialized) taskDelete(pab_links[i]->abScanId);
	    }
	}
	/* this seems to be necessary for the AB card to stop talking */
	counter = 0;
	for(ind=0; ind<10000000; ind ++) counter++;
	return(0);
}


LOCAL void config_init(ab_link *plink)
{
    ab_config	*pconfig = plink->pconfig;
    int		i;
    ab_region	*p6008;

    if(!pconfig) plink->pconfig = pconfig = abCalloc(1,sizeof(ab_config));
    p6008 = (ab_region *)DEFAULT_BASE_ADDR;
    p6008 += plink->link;
    pconfig->base_address = (void *)p6008;
    pconfig->baud_rate = DEF_RATE;
    pconfig->autoconfig = FALSE;
    pconfig->int_vector = DEFAULT_VEC_BASE + plink->link;
    pconfig->int_level = DEFAULT_INT_LEVEL;
    pconfig->scan_list_len = 8;
    for(i=0; i< AB_MAX_ADAPTERS; i++) pconfig->scan_list[i] = (i<<2);
}

int abConfigNlinks(int nlinks)
{
    if(pab_links) {
	printf("abConfigNlinks Illegal call. Must be first abDrv related call\n");
	return(-1);
    }
    max_ab_6008s = nlinks;
    pab_links = abCalloc(max_ab_6008s,sizeof(ab_link *));
    return(0);
}

int abConfigVme(int link, int base, int vector, int level)
{
    ab_link	*plink;
    ab_config	*pconfig;

    if(link<0 || link>=max_ab_6008s) return(-1);
    if(!pab_links) pab_links = abCalloc(max_ab_6008s,sizeof(ab_link *));
    plink = pab_links[link];
    if(!plink) plink = allocLink(link);
    pconfig = plink->pconfig;
    pconfig->base_address = (void *)base;
    pconfig->int_vector = vector;
    pconfig->int_level = level;
    return(0);
}
    
int abConfigBaud(int link, int baud)
{
    ab_link	*plink;
    ab_config	*pconfig;

    if(link<0 || link>=max_ab_6008s) return(-1);
    if(!pab_links) pab_links = abCalloc(max_ab_6008s,sizeof(ab_link *));
    plink = pab_links[link];
    if(!plink) plink = allocLink(link);
    pconfig = plink->pconfig;
    if(baud == 0) pconfig->baud_rate = DEF_RATE;
    else pconfig->baud_rate = FAST_RATE;
    return(0);
}
    
int abConfigAuto(int link)
{
    ab_link	*plink;
    ab_config	*pconfig;

    if(link<0 || link>=max_ab_6008s) return(-1);
    if(!pab_links) pab_links = abCalloc(max_ab_6008s,sizeof(ab_link *));
    plink = pab_links[link];
    if(!plink) plink = allocLink(link);
    pconfig = plink->pconfig;
    pconfig->autoconfig = TRUE;
    return(0);
}

int abConfigScanList(int link, int scan_list_len, char *scan_list)
{
    ab_link	*plink;
    ab_config	*pconfig;

    if(link<0 || link>=max_ab_6008s) return(-1);
    if(!pab_links) pab_links = abCalloc(max_ab_6008s,sizeof(ab_link *));
    plink = pab_links[link];
    if(!plink) plink = allocLink(link);
    pconfig = plink->pconfig;
    pconfig->scan_list_len = scan_list_len;
    memset(pconfig->scan_list,'\0',64);
    memcpy(pconfig->scan_list,scan_list,scan_list_len);
    return(0);
}

int abConfigScanListAscii(int link, char *filename,int setRackSize)
{
    FILE	*fp;
    char	*scan_list;
    char	buf[80];
    unsigned	rack,group,size;
    int		nItemsRead,scan_list_len;
    char	size_string[80];
    char	mode_string[80];
    ab_link	*plink;
    ab_adapter  *padapter;

    if(link<0 || link>=max_ab_6008s) {
	printf("link %d is out of range\n",link);
	return(0);
    }
    if(!pab_links) pab_links = abCalloc(max_ab_6008s,sizeof(ab_link *));
    plink = pab_links[link];
    if(!plink) plink = allocLink(link);
    fp = fopen(filename,"r");
    if(!fp) {
	printf("%s %s\n",filename,strerror(errno));
	return(0);
    }
    scan_list = abCalloc(64,sizeof(char));
    scan_list_len = 0;
    while(fgets(buf,80,fp)) {
	if(buf[0] == '#') continue;
	size_string[0] = 0;
	mode_string[0] = 0;
	nItemsRead = sscanf(buf,"%u %u %s %s",
	    &rack,&group,size_string,mode_string);
	if(nItemsRead<3) goto err_rtn;
	if(rack>=MAX_AB_ADAPTERS) goto err_rtn;
	if(!setRackSize) {
	    size = 0x00;
	} else if(strcmp(size_string,"1/4")==0) {
	    size = 0x00;
	} else if(strcmp(size_string,"1/2")==0) {
	    size = 0x40;
	} else if(strcmp(size_string,"3/4")==0) {
	    size = 0x80;
	} else if(strcmp(size_string,"Full")==0) {
	    size = 0xc0;
	} else {
	    goto err_rtn;
	}
	padapter = plink->papadapter[rack];
	if(!padapter) padapter = allocAdapter(link,rack);
	if(strlen(mode_string)==0) {
	    padapter->addr_mode = twoSlot;
	} else if(strcmp(mode_string,"1/2")==0) {
	    padapter->addr_mode = halfSlot;
	} else if(strcmp(mode_string,"1")==0) {
	    padapter->addr_mode = oneSlot;
	} else if(strcmp(mode_string,"2")==0) {
	    padapter->addr_mode = twoSlot;
	} else {
	    goto err_rtn;
	}
	scan_list[scan_list_len] = size | (rack<<2) | group;
	scan_list_len++;
    }
    fclose(fp);
    return(abConfigScanList(link,scan_list_len,scan_list));
err_rtn:
    printf("abConfigScanListAscii: line %d is invalid\"%s\"\n",
	scan_list_len,buf);
    fclose(fp);
    return(-1);
}

LOCAL int ab_driver_init()
{
	unsigned short	cok;
	unsigned short	link;
	ab_link		*plink;
	ab_region	*p6008;
	ab_region	*base;
	ab_config	*pconfig;
        int 		vxstatus;
	long		status;
	int		got_one;
	char		task_name[50];

	if(!pab_links) pab_links = abCalloc(max_ab_6008s,sizeof(ab_link *));
	/* check if any of the cards are there */
	got_one = 0;
	for (link = 0; link < max_ab_6008s; link++){
	    plink = pab_links[link];
            if(!plink) plink = allocLink(link);
            base = plink->pconfig->base_address;
	    status = devRegisterAddress("drvAb",atVMEA24,(size_t)base,
		sizeof(ab_region),(volatile void **)&p6008);
	    if(status) {
		errMessage(status,"drvAb");
		return(status);
	    }
	    if (vxMemProbe((char *)p6008,READ,1,(char *)&cok)==ERROR) {
		continue;
	    }
	    got_one = 1;
	    if(!plink) plink = allocLink(link);
	    plink->vme_addr = p6008;	/* set AB card address */
	    pconfig = plink->pconfig;
	    if(!(plink->request_sem = semMCreate(
            SEM_DELETE_SAFE|SEM_INVERSION_SAFE|SEM_Q_PRIORITY))){
		printf("AB_DRIVER_INIT: semBcreate failed\n");
		taskSuspend(0);
	    }
	    if(!(plink->ab_cmd_sem = semBCreate(SEM_Q_FIFO,SEM_EMPTY))){
		printf("AB_DRIVER_INIT: semBcreate failed\n");
		taskSuspend(0);
	    }
	    if(!(plink->ab_data_sem = semBCreate(SEM_Q_FIFO,SEM_EMPTY))){
		printf("AB_DRIVER_INIT: semBcreate failed\n");
		taskSuspend(0);
	    }
	    status = devConnectInterruptVME(pconfig->int_vector,
		allenBradleyIntHandler,(void *)(int)link);
	    if(status) {
		errMessage(status,"drvAb");
		return(status);
	    }
	    status = devEnableInterruptLevel(intVME,pconfig->int_level);
	    if(status) {
		errMessage(status,"drvAb");
		taskSuspend(0);
	    }
	    /* initialize the serial link */
	    if(link_init(plink)) {
		/*No use proceeding*/
		continue;
	    }
	    plink->initialized = TRUE;
	    sprintf(task_name,"%s%2.2d",ABSCAN_NAME,link);
            vxstatus  = taskSpawn(task_name,ABSCAN_PRI,ABSCAN_OPT,
		  ABSCAN_STACK,(FUNCPTR)abScanTask,(int)plink,
		  0,0,0,0,0,0,0,0,0);
	    if(vxstatus < 0){
		printf("AB_DRIVER_INIT: failed taskSpawn\n");
		taskSuspend(0);
	    }
	    plink->abScanId = vxstatus;
	    taskwdInsert((epicsThreadId)plink->abScanId,NULL,NULL);
	    sprintf(task_name,"%s%2.2d",ABDONE_NAME,link);
            vxstatus  = taskSpawn(task_name,ABDONE_PRI,ABDONE_OPT,
		ABDONE_STACK,(FUNCPTR)abDoneTask,(int)plink,
		0,0,0,0,0,0,0,0,0);
	    if(vxstatus < 0){
		printf("AB_DRIVER_INIT: failed taskSpawn\n");
		taskSuspend(0);
	    }
	    plink->abDoneId = vxstatus;
	    taskwdInsert((epicsThreadId)plink->abDoneId,NULL,NULL);
	    plink->ab_disable = FALSE;
	}
	/* put in hook for disabling communication for a reboot */
	if (got_one) rebootHookAdd(ab_reboot_hook);
        return(0);
}

/*
 * link_init
 *
 * establish the communication link with the AB scanner
 */
LOCAL int link_init(ab_link *plink)
{
    ab_region	*p6008 = plink->vme_addr;
    dp_mbox 	*pmb = &p6008->mail;
    ab_config	*pconfig=plink->pconfig;
    int		status;
    int		ntry;

    /* the scanner comes up with the dual ported memory locked */
    pmb->fl_lock = 0;		/* so unlock it */
    /*clear request semaphore*/
    semGive(plink->request_sem);
    if(pmb->conf_stat != SCANNER_POWERUP){
	/* This link must already be initialized.  We're done */
	if (ab_debug){
	    printf("Link %x already initialized\n", plink->link);
	}
	return(0);
    }
    if(ab_debug) printf("drvAb: link %x, powerup...\n",plink->link);
    /* on initialization the scanner puts its firmware revision info*/
    /* into the general data are of the dual-port.  We save it here. */
    /* (The most current revision is Series A, Revision D.)	*/
    strcpy(plink->firmware_info,(char *)&pmb->msg[0]);
    /* setup scanner */
    /* Wake up scanner for the first time*/
    p6008->sc_intr = 1; /*Wake up scanner*/
    taskDelay(1); /*Give it time to initialize*/
    for(ntry=0; ntry<maxCmdTrys; ntry++) {
        status = sc_lock(plink);
        if(status) continue;
        pmb->command = SET_UP;
        pmb->data_len = 4;
        pmb->msg[0] = (pconfig->baud_rate<<8) | 0;
	pmb->msg[1] = (DEBUG<<8) | pconfig->int_level;
	pmb->msg[2] = (pconfig->int_vector<<8) | AB_INT_ENABLE;
	pmb->msg[3] = (AB_SYSFAIL_DISABLE<<8)  | 0;
	status = sc_waitcmd(plink);
	if(status) continue;
	if(pmb->conf_stat != 0) {
	    sc_conferr(plink);
	    continue;
	}
	break;
    }
    if(ntry>=maxCmdTrys) {
	printf("abDrv: SET_UP failed link %hu\n",plink->link);
	return(ERROR);
    }
    sc_unlock(plink);
    /* Once scanner has been placed in RUN_MODE, putting it back into
     * PROGRAM_MODE will disable binary outputs until it is placed back in
     * RUN_MODE.  Some scanner commands, such as SCAN_LIST, can only be
     * performed in PROGRAM_MODE.  These commands should only be issued*
     * immediately after initialization. 
     * Re-booting an IOC (without powering it down) is the presently
     * the only way of getting it into PROGRAM_MODE
     * without disabling binary outputs */

    /* initialize scan list for each link present */
    /* A promised firmware change will allow us to RESET
       the scanner over the vmeBus.  For now, the only way
       to get the scanner into prog-mode without having
       the BO's glitch is with the vmeBus SYSRESET signal,
       which occurs when the RESET switch on the VME chassis 
       is used.  Until the f/w change is made, changing the
       scanner from run mode to program mode (to modify the
       scan list, for instance) will cause the BO's to turn 
       off until the scanner is returned to run mode.  It's 
       not nice, but for now we'll have to assume that all
       adapters are needed and put them all in the scan list. */
    /* set scan list*/
    if(pconfig->autoconfig) for(ntry=0; ntry<maxCmdTrys; ntry++) {
	status = sc_lock(plink);
	if(status) continue;
	pmb->command = AUTO_CONF;
	pmb->data_len = 0;
	status = sc_waitcmd(plink);
	if(status) continue;
	if(pmb->conf_stat != 0) {
	    sc_conferr(plink);
	    continue;
	}
	break;
    }else for(ntry=0; ntry<maxCmdTrys; ntry++) {
	status = sc_lock(plink);
	if(status) continue;
	pmb->command = SCAN_LIST;
	pmb->data_len = pconfig->scan_list_len;
	uctrans(pconfig->scan_list,(unsigned char *)pmb->msg,
    	pconfig->scan_list_len);
	status = sc_waitcmd(plink);
	if(status) continue;
	if(pmb->conf_stat != 0) {
	    sc_conferr(plink);
	    continue;
	}
	break;
    }
    if(ntry>=maxCmdTrys) {
	printf("abDrv: AUTO_CONFIG or SCAN_LIST failed link %hu\n",plink->link);
	return(ERROR);
    }
    sc_unlock(plink);
    /* place the scanner into run mode */
    for(ntry=0; ntry<maxCmdTrys; ntry++) {
	status = sc_lock(plink);
	if(status)  continue;
	pmb->command = SET_MODE;
	pmb->msg[0] = (RUN_MODE<<8);
	status = sc_waitcmd(plink);
	if(status) continue;
	if(pmb->conf_stat != 0) {
	    sc_conferr(plink);
	    continue;
	}
	break;
    }
    if(ntry>=maxCmdTrys) {
	printf("abDrv: SET_MODE failed link %hu\n",plink->link);
	return(ERROR);
    }
    sc_unlock(plink);
    return(0);
}

/*
 * abScanTask
 *
 * Scans the AB IO according to the AB configuration table.
 * Entries are made in the AB configuration table when an IO
 * interface is attempted from the database scan tasks.
 * The sleep time assures that there is at least 1/10 second between the passes.
 * The time through the scan loop seems to be minimal so there are no provisions
 * for excluding the scan time from the sleep time.
 */
LOCAL void abScanTask(ab_link *plink)
{
    unsigned short	adapter;
    ab_adapter		*padapter;
    unsigned int	pass = 0;
    BOOL		madeInitialCallback = FALSE;
    unsigned long	tickNow,tickBeg,tickDiff;

    tickBeg = tickGet();
    while(TRUE){
	/* run every 1/10 second */
	taskDelay(sysClkRateGet()/10);
	if(plink->ab_disable) continue;
	/* Every second perform a link check to see if any adapters */
	/* have  changed state.  (Don't want to queue up requests if*/
	/* they're off) */
	if((pass % 10) == 0){ 
	    if (link_status(plink) != 0){
		if(ab_debug) printf("%x link_stat error\n",plink->link);
	    }
	}
	if(!madeInitialCallback && interruptAccept)
		plink->initialCallback = TRUE;
	pass++;
	/*recompute intrSec and btSec about every 10 seconds*/
	if((pass %100) == 0) {
	    tickNow = tickGet();
	    if(tickNow > tickBeg){/*skip overflows*/
		tickDiff = tickNow - tickBeg;
		/*round to nearest counts/sec */
		plink->intrSec=((plink->intr_cnt*sysClkRateGet())+5)/tickDiff;
		plink->btSec=((plink->bt_cnt*sysClkRateGet())+5)/tickDiff;
	    }
	    tickBeg = tickNow;
	    plink->intr_cnt = 0;
	    plink->bt_cnt = 0;
	}
	for (adapter = 0; adapter < AB_MAX_ADAPTERS; adapter++){
		padapter = plink->papadapter[adapter];
		if(!padapter) continue;
		read_ab_adapter(plink,padapter);
		padapter->adapter_status_change = FALSE;
	}
	if(plink->initialCallback) {
	    plink->initialCallback = FALSE;
	    madeInitialCallback = TRUE;
	}
    }
}

LOCAL void read_ab_adapter(ab_link *plink, ab_adapter *padapter)
{
    unsigned short 	card;
    unsigned long	value;
    ab_card		*pcard;
    scanInfo		*pscanInfo;
    btInfo		*pbtInfo;
    abStatus		btStatus=0;

    /* each card */
    for (card = 0; card < AB_MAX_CARDS; card++){
	pcard = padapter->papcard[card];
	if (!pcard) continue;
	if(!pcard->active) continue;
	if(pcard->type==typeBt)  continue;
	if(pcard->type==typeBi || pcard->type==typeBo || pcard->type==typeBiBo){
	    if(pcard->type==typeBi || pcard->type==typeBiBo) {
		di_read(pcard,&value);
		if ((value != pcard->diprev)||padapter->adapter_status_change){
		    pcard->diprev = value;
		    if(pcard->callback) (*pcard->callback)((void *)pcard);
		}
	    }
	    if(pcard->type==typeBo || pcard->type==typeBiBo) {
		if(padapter->adapter_status_change) {
		    if(pcard->bocallback) (*pcard->bocallback)((void *)pcard);
		}
	    }
	    if(plink->initialCallback) {
	        if(pcard->callback) (*pcard->callback)((void *)pcard);
	        if(pcard->bocallback) (*pcard->bocallback)((void *)pcard);
	    }
	    continue;
	}
	/*If we get here card is typeAi or typeAo*/
	pscanInfo = pcard->pscanInfo;
	if(!pscanInfo) continue;
	pbtInfo = pcard->pbtInfo;
	if(!pbtInfo) continue;
	if(padapter->adapter_status_change){
	    if(!padapter->adapter_online) {
		pcard->status = abNotInitialized;
		if(pcard->callback) (*pcard->callback)((void *)pcard);
		continue;
	    }
	    pscanInfo->state = stateInit;
	}
	if(!padapter->adapter_online) continue;
	switch(pscanInfo->state) {
	case stateInit:
	    if(pcard->status !=abNotInitialized) {
	        pcard->status = abNotInitialized;
		if(pcard->callback) (*pcard->callback)((void *)pcard);
	    }
	    if(pcard->type==typeAo) {
	   	btStatus = bt_queue(AB_READ,pcard,pscanInfo->pread_msg,
			pscanInfo->read_msg_len);
	    }else {
		btStatus = bt_queue(AB_WRITE,pcard,pscanInfo->pwrite_msg,
			pscanInfo->write_msg_len);
	    }
	    if(btStatus==abBtqueued) {
		pscanInfo->state_cnt = 0;
		pscanInfo->state = stateInitBTwait;
		pscanInfo->init_ctr++;
	    } else {
		pscanInfo->bt_fail++;
		pscanInfo->state_cnt++;
	    }
	    break;
	case stateInitBTwait:
	    if(pbtInfo->btStatus==abBtqueued) {
		pscanInfo->state_cnt++;
		if(pscanInfo->state_cnt < 80) break;
	    }
	    if(pbtInfo->btStatus!=abSuccess) {
		if(pbtInfo->btStatus==abTimeout) pscanInfo->bt_to++;
		else pscanInfo->bt_fail++;
                pbtInfo->btActive = FALSE;
	        pscanInfo->state_cnt = 0;
	        pscanInfo->state = stateInit;
		break;
	    }
	    pscanInfo->state_cnt = 0;
	    pscanInfo->state = stateBT;
	    if(pcard->type==typeAi) {
	    	pcard->needsUpdate = TRUE;
	    } else {
		/*Give device support time to set output words*/
		pscanInfo->down_cnt = pscanInfo->update_rate;
	    }
	    break;
	case stateCountdown:
	    if(pcard->needsUpdate) {
		pscanInfo->down_cnt = 0;
		pcard->needsUpdate = FALSE;
	    }
	    if(pscanInfo->down_cnt>0) pscanInfo->down_cnt--;
	    if(pscanInfo->down_cnt>0) break;
	    pscanInfo->state_cnt = 0;
	    pscanInfo->state = stateBT;
	    /*break left out on purpose*/
	case stateBT:
	    if(pcard->type==typeAi) {
	   	btStatus = bt_queue(AB_READ,pcard,pscanInfo->pread_msg,
			pscanInfo->read_msg_len);
	    }else {
		btStatus = bt_queue(AB_WRITE,pcard,pscanInfo->pwrite_msg,
			pscanInfo->write_msg_len);
	    }
	    if(btStatus!=abBtqueued) {
		pscanInfo->state_cnt++;
		/*After 15 trys reinitialize*/
		if(pscanInfo->state_cnt > 15) {
                    if(ab_debug>0) {
                        printf("state_cnt >15 after bt_queue L%d A%d C%d\n",
                            plink->link,padapter->adapter,card);
                    }
		    pcard->status = abFailure;
		    pscanInfo->bt_fail++;
		    pscanInfo->state_cnt = 0;
		    pscanInfo->state = stateInit;
		}
		break;
	    }
	    pscanInfo->state_cnt = 0;
	    pscanInfo->state = stateBTwait;
	    break;
	case stateBTwait:
	    if(pbtInfo->btStatus==abBtqueued) {
		pscanInfo->state_cnt++;
		if(pscanInfo->state_cnt < 80) break;
	    }
	    if(pbtInfo->btStatus!=abSuccess) {
                if(ab_debug>0) {
                    printf("stateBTwait failure btStatus %d L%d A%d C%d\n",
                        pbtInfo->btStatus, plink->link,padapter->adapter,card);
                }
		pcard->status = abFailure;
		if(pbtInfo->btStatus==abTimeout) pscanInfo->bt_to++;
		else pscanInfo->bt_fail++;
                pbtInfo->btActive = FALSE;
	        pscanInfo->state_cnt = 0;
	        pscanInfo->state = stateInit;
		break;
	    }
	    pcard->status = abSuccess;
	    pscanInfo->state_cnt = 0;
	    pscanInfo->down_cnt = pscanInfo->update_rate;
	    pscanInfo->state = stateCountdown;
	    if(pcard->callback) (*pcard->callback)((void *)pcard);
	}
    }
}

LOCAL void abDoneTask(ab_link *plink)
{
    unsigned short	adapter;
    unsigned short	card;
    ab_adapter		*padapter;		/* adapter data structure */
    ab_card		*pcard;			/* card data structure */
    btInfo		*pbtInfo;
    dp_mbox		*pmb = &plink->vme_addr->mail;
    volatile unsigned short *pmb_msg = &pmb->msg[0];

    while(TRUE) {
	semTake(plink->ab_data_sem,WAIT_FOREVER);
	/* Must check all data returned by hardware interface*/
	adapter = (pmb->address & 0x70) >> 4;
	padapter = plink->papadapter[adapter];
	if(!padapter) {
	    if(ab_debug)
		printf("abDrv:abDoneTask padapter=NULL link=%d adapter=%d\n",
		plink->link,adapter);
	    pmb->fl_lock = 0;
	    continue;
	}
	card = computeCard(padapter,pmb->address);
	if(card >= MAX_CARDS_PER_ADAPTER || adapter>=MAX_AB_ADAPTERS) {
	    if(ab_debug)
		printf("drvAb: scanner returned bad address %4x link=%d\n",
		plink->link,pmb->address);
	    pmb->fl_lock = 0;
	    continue;
	}
	pcard = padapter->papcard[card];
	if(!pcard) {
	    if(ab_debug)
		printf("abDrv: abDoneTask pcard=NULL L%d A%d C%d\n",
	    	plink->link,adapter,card);
	    pmb->fl_lock = 0;
	    continue;
	}
	plink->bt_cnt++;
	pbtInfo = pcard->pbtInfo;
        if(!pbtInfo->btActive) {
	    if(ab_debug)
                printf("abDrv: abDoneTask but !btActive %x L%d A%d C%d\n",
                    pmb->conf_stat,plink->link,adapter,card);
	    pmb->fl_lock = 0;
            continue;
        }
        pbtInfo->btActive = FALSE;
	/* block transfer failure */
	if (pmb->conf_stat != 0){
	    if(ab_debug)
                printf("abDrv: abDoneTask conf_stat %x L%d A%d C%d\n",
                    pmb->conf_stat,plink->link,adapter,card);
	    if (pmb->conf_stat == BT_TIMEOUT) {
		pbtInfo->btStatus = abTimeout;
	    }else{
		pbtInfo->btStatus = abFailure;
	    }
	}else{
	    /* successful */
	    pbtInfo->btStatus = abSuccess;
	    /* was it the response to a read command */
	    if (pbtInfo->cmd == AB_READ){
		    ustrans(pmb_msg,pbtInfo->pmsg,pbtInfo->msg_len);
	    }
	}
	pmb->fl_lock = 0;
	if(pcard->type==typeBt) {
	    semTake(pcard->card_sem,WAIT_FOREVER);
	    if(pcard->active) {
		wdCancel(pbtInfo->wdId);
		if(pbtInfo->btStatus == abSuccess) {
		    pcard->active = FALSE;
		    pcard->status = pbtInfo->btStatus;
		    if(pcard->callback) (*pcard->callback) ((void *)pcard);
		} else {
		    CALLBACK	*pcallback = &pbtInfo->callback;
    		    STATUS	wdStatus;

		    pbtInfo->reason = btCallbackRetry;
		    wdStatus = wdStart(pcard->pbtInfo->wdId,sysClkRateGet()/30,
			(FUNCPTR)callbackRequest,(int)pcallback);
		}	
	    }
	    semGive(pcard->card_sem);
	}
    }
}

LOCAL void ab_reset_task(ab_link *plink)
{
	unsigned short	link,adapter,card,ab_reset_wait;
	ab_adapter	*padapter;
	ab_region	*pab_region=0;
	ab_card		*pcard;
	btInfo		*pbtInfo;

	link = plink->link;
	pab_region = plink->vme_addr;
	plink->ab_disable = 1;
	printf("Disabled AB Scanner Task\n");
	taskDelay(sysClkRateGet()*2);
	/* Signal the Scanner to Reset */
	pab_region->sys_fail_set2 = 0xa0a0;
	pab_region->sys_fail_set1 = 0x0080;
	printf("Card %d Reset\n",link);
	/*mark all block transfer cards for initialization*/
	for(adapter = 0; adapter < AB_MAX_ADAPTERS; adapter++){
	    padapter = plink->papadapter[adapter];
	    if(!padapter) continue;
	    for (card = 0; card < AB_MAX_CARDS; card++){
		pcard = padapter->papcard[card];
		if(!pcard) continue;
		pbtInfo = pcard->pbtInfo;
		if(pcard->type==typeAo || pcard->type==typeAi)
		    pcard->needsUpdate = TRUE;
	    }
	}
	ab_reset_wait = 0;
	while((pab_region->mail.conf_stat != SCANNER_POWERUP)
	&& (ab_reset_wait < 600)){
		taskDelay(1);
		ab_reset_wait++;
	}
	if (ab_reset_wait < 600)
	    	printf("Link %d Power Up After %d Ticks\n",link,ab_reset_wait);
	else
	    	printf("Link %d Failed to Reinitialize After %d Ticks\n",
			link,ab_reset_wait);

	link_init(plink);
	/* enable the scanner */
	plink->ab_disable = 0;
}

int ab_reset_link(int firstLink,int lastLink)
{
    ab_link *plink;
    int link;

    if(firstLink<0 || firstLink>=max_ab_6008s || lastLink>=max_ab_6008s) {
        printf("ab_reset_link illegal arguments\n");
        return(1);
    }
    if(lastLink<firstLink) lastLink = firstLink;
    for(link=firstLink; link<=lastLink; ++link) {
        plink = pab_links[link];
        if(!plink) return(0);
        taskSpawn("ab_reset",38,0,8000,(FUNCPTR)ab_reset_task,(int)plink,
	    0,0,0,0,0,0,0,0,0);
    }
    return(0);
}

int ab_reset(void)
{
        return(ab_reset_link(0,max_ab_6008s-1));
}

LOCAL char *activeMessage[2] = {"notScanning","Scanning"};
LOCAL char *modeMessage[3] = {"2 slot","1 slot","1/2 slot"};
LOCAL long ab_io_report(int level)
{
    ab_link		*plink;
    ab_adapter		*padapter;
    ab_card		*pcard;
    scanInfo		*pscanInfo;
    unsigned short	link, adapter, card;
    int			i;
    unsigned long	divalue,dovalue;

    /* link data */
    for (link = 0; link < max_ab_6008s; link++){
	plink = pab_links[link];
	if(!plink) continue;
	if(!plink->initialized) {
	    printf("AB-6008SV link %hu not initialized\n",link);
	    continue;
	}
	printf("AB-6008SV link: %hu osw %4.4x vme: %p\n",
		link,plink->vme_addr->osw,plink->vme_addr);
	if(plink->firmware_info[0]) printf("    %s\n",plink->firmware_info);
	printf("         Mailbox lock timeouts %lu\n",plink->sclock_to);
	printf("              Command timeouts %lu\n", plink->sccmd_to);
	printf("               Command Failure %lu\n", plink->sc_fail);
	printf("         Interrupts per second %hu\n", plink->intrSec);
	printf("    Block Transfers per second %hu\n", plink->btSec);
	if(level>2) {
	    ab_link_status *plink_status = plink->plink_status;

	    printf("    Adapter Status Words");
	    for(i=0; i<32; i++) {
		if((i%8)==0) printf("\n    ");
		printf("%4.4x ",plink_status->status[i]);
	    }
	    printf("\n    scan list");
	    for(i=0; i<plink_status->list_len; i++) {
		if((i%16)==0) printf("\n    ");
		printf("%2.2x ",plink_status->list[i]);
	    }
	    printf("\n");
	}
	/* adapter information */
	for (adapter = 0; adapter < AB_MAX_ADAPTERS; adapter++){
	    padapter = plink->papadapter[adapter];
	    if (!padapter) continue;
	    if (padapter->adapter_online)
		printf("  Adapter %hu ONLINE",adapter);
	    else
		printf("  Adapter %hu OFFLINE",adapter);

	    printf(" %s addressing\n",modeMessage[padapter->addr_mode]);
	    /* card information */
	    for (card = 0; card < AB_MAX_CARDS; card++){
		pcard = padapter->papcard[card];
		if(!pcard) continue;
		pscanInfo = pcard->pscanInfo;
 		switch (pcard->type){
		case (typeBi):
		    di_read(pcard,&divalue);
		    printf("    CARD %hu: BI %s %s 0x%lx\n",
			card,abNumBitsMessage[pcard->nBits],
			activeMessage[pcard->active],divalue);
		    break;
		case (typeBo):
		    do_read(pcard,&dovalue);
		    printf("    CARD %hu: BO %s %s 0x%lx\n",
			card,abNumBitsMessage[pcard->nBits],
			activeMessage[pcard->active],dovalue);
		    break;
		case (typeBiBo):
		    di_read(pcard,&divalue);
		    do_read(pcard,&dovalue);
		    printf("    CARD %hu: BIBO %s %s in 0x%lx out 0x%lx\n",
			card,abNumBitsMessage[pcard->nBits],
			activeMessage[pcard->active],divalue,dovalue);
		    break;
		case (typeAi):
		case (typeAo):
		    if(pcard->type==typeAi)
			printf("    CARD %hu: AI ",card);
		    else
			printf("    CARD %hu: AO ",card);
		    if(!pscanInfo || pscanInfo->state==stateInit)
			printf(" NOT INITIALIZED ");
		    printf(" %s %s",activeMessage[pcard->active],
			pcard->card_name);
		    /* error reporting */
		    if(!pscanInfo) {
			printf("\n");
			break;
		    }
		    if (pscanInfo->bt_to)
		    	printf(" bt timeout: %hu",pscanInfo->bt_to);
		    if (pscanInfo->bt_fail)
		    	printf(" bt fail: %hu",pscanInfo->bt_fail);
		    if (pscanInfo->init_ctr)
			printf(" initialized %hu times",pscanInfo->init_ctr);
		    printf("\n");
		    if (level > 0){
			if(pcard->type==typeAo || level>1) {
			    printf("\tWrite");
			    for (i=0; i<pscanInfo->write_msg_len; i++) {
				if((i%10)==0) printf("\n\t");
				printf("%4.4x ",pscanInfo->pwrite_msg[i]);
			    }
			    printf("\n");
			}
			if(pcard->type==typeAi || level>1) {
			    printf("\tRead");
			    for (i=0; i<pscanInfo->read_msg_len; i++) {
			        if((i%10)==0) printf("\n\t");
			        printf("%4.4x ",pscanInfo->pread_msg[i]);
			    }
			    printf("\n");
			}
		    }
		    break;
		case (typeBt):
		    printf("    CARD %hu: BT %s %s\n", card,
			activeMessage[pcard->active],pcard->card_name);
		    break;
		default:
		    continue;
		}
	    }
	}
    }
    return(0);

}

LOCAL abStatus registerCard(
	unsigned short link,unsigned short adapter, unsigned short card,
	cardType type, const char *card_name, void (*callback)(void *drvPvt),
	void **pdrvPvt)
{
    ab_adapter	*padapter;
    ab_card	*pcard = NULL;
    ab_link	*plink;
    ab_card	**ppcard = (ab_card **)pdrvPvt;


    if(link>=max_ab_6008s) {
	if(ab_debug>0)
	    printf("abDrv(registerCard) bad link %hu\n",link);
	return(abNoCard);
    }
    if(adapter>=MAX_AB_ADAPTERS) {
	if(ab_debug>0)
	    printf("abDrv(registerCard) bad adapter %hu\n",adapter);
	return(abNoCard);
    }
    if(card>=MAX_CARDS_PER_ADAPTER) {
	if(ab_debug>0)
	    printf("abDrv(registerCard) bad card %hu\n",card);
	return(abNoCard);
    }
    if(!pab_links || !(plink = pab_links[link]) || !plink->initialized) {
	if(ab_debug>0)
	    printf("abDrv(registerCard) link %hu not initialized\n",link);
	return(abNoCard);
    }
    padapter = plink->papadapter[adapter];
    if(padapter) pcard = padapter->papcard[card];
    if(pcard) {
	if(strcmp(pcard->card_name,card_name)!=0) return(abCardConflict);
	if(pcard->type==type) {
	    *ppcard = pcard;
	    return(abSuccess);
	}
	if(type==typeBi || type==typeBo) {
	    if(pcard->type==typeBo || pcard->type==typeBi
	    || pcard->type==typeBiBo) {
		if(!pcard->callback && type==typeBi )
			pcard->callback = callback;
		if(!pcard->bocallback && type==typeBo )
			pcard->bocallback = callback;
		pcard->type = typeBiBo;
		*ppcard = pcard;
		return(abSuccess);
	    }
	}
	return(abCardConflict);
    }
    /*New Card*/
    pcard = abCalloc(1,sizeof(ab_card));
    pcard->link = link;
    pcard->adapter = adapter;
    pcard->card = card;
    pcard->type = type;
    pcard->card_name = card_name;
    if(!(pcard->card_sem = semBCreate(SEM_Q_FIFO,SEM_FULL))){
	printf("abDrv register card: semBcreate failed\n");
	taskSuspend(0);
    }
    if(type==typeBo) pcard->bocallback = callback;
    else pcard->callback = callback;
    if(type==typeAi || type ==typeAo || type == typeBt) {
	pcard->pbtInfo = abCalloc(1,sizeof(btInfo));
	if(type == typeBt) {
	    btInfo *pbtInfo = pcard->pbtInfo;

	    if(!(pbtInfo->wdId = wdCreate()) ) {
		printf("abDrv register card: wdCreate failed\n");
		taskSuspend(0);
	    }
	    callbackSetCallback(btCallback,&pbtInfo->callback);
	    callbackSetUser(pcard,&pbtInfo->callback);
	    /*use default priority */
	}
    }
    *ppcard = pcard;
    if(!padapter) padapter = allocAdapter(link,adapter);
    padapter->papcard[card] = pcard;
    link_status(plink);
    return(abNewCard);
}

LOCAL void getLocation(void *drvPvt,unsigned short *link,
	unsigned short *adapter,unsigned short *card)
{
    ab_card *pcard = drvPvt;

    *link = pcard->link;
    *adapter = pcard->adapter;
    *card = pcard->card;
    return;
}

LOCAL abStatus setNbits(void *drvPvt, abNumBits nBits)
{
    ab_card *pcard = drvPvt;

    if(pcard->nBits == abBitNotdefined) {
	pcard->nBits = nBits;
	pcard->active = TRUE;
	return(abSuccess);
    }
    if(pcard->nBits == nBits) return(abSuccess);
    return(abFailure);
}

LOCAL void setUserPvt(void *drvPvt,void *userPvt)
{
    ab_card *pcard = drvPvt;

    pcard->userPvt = userPvt;
    return;
}

LOCAL void *getUserPvt(void *drvPvt)
{
    ab_card *pcard = drvPvt;

    return(pcard->userPvt);
}

LOCAL abStatus getStatus(void *drvPvt)
{
    ab_card 	*pcard = drvPvt;

    unsigned short	link = pcard->link;
    unsigned short	adapter = pcard->adapter;
    ab_adapter		*padapter = pab_links[link]->papadapter[adapter];

    if(!padapter->adapter_online) return(abAdapterDown);
    return(pcard->status);
}

LOCAL abStatus startScan(void *drvPvt, unsigned short update_rate,
    unsigned short *pwrite_msg, unsigned short write_msg_len,
    unsigned short *pread_msg, unsigned short read_msg_len)
{
    ab_card 	*pcard = drvPvt;
    scanInfo	*pscanInfo;

    pscanInfo = pcard->pscanInfo;
    if(!pscanInfo) pcard->pscanInfo = pscanInfo = abCalloc(1,sizeof(scanInfo));
    pscanInfo->update_rate = update_rate;
    pscanInfo->pwrite_msg = pwrite_msg;
    pscanInfo->write_msg_len = write_msg_len;
    pscanInfo->pread_msg = pread_msg;
    pscanInfo->read_msg_len = read_msg_len;
    pscanInfo->state = stateInit;
    pcard->active = TRUE;
    pcard->status = abNotInitialized;
    return(abSuccess);
}

LOCAL abStatus updateAo(void *drvPvt)
{
    ab_card *pcard = drvPvt;

    pcard->needsUpdate = TRUE;
    return(getStatus(drvPvt));
}

LOCAL abStatus updateBo(void *drvPvt,unsigned long value,unsigned long mask)
{
    ab_card		*pcard = drvPvt;
    unsigned long	imagevalue;

    semTake(pcard->card_sem,WAIT_FOREVER);
    do_read(pcard,&imagevalue);
    imagevalue = (imagevalue & ~mask) | (value & mask);
    do_write(pcard,imagevalue);
    semGive(pcard->card_sem);
    if(pcard->bocallback) (*pcard->bocallback)((void *)pcard);
    return(getStatus(drvPvt));
}

LOCAL abStatus readBo(void *drvPvt,unsigned long *pvalue,unsigned long mask)
{
    ab_card		*pcard = drvPvt;
    unsigned long	value;

    do_read(pcard,&value);
    *pvalue = value & mask;
    return(getStatus(drvPvt));
}

LOCAL abStatus readBi(void *drvPvt,unsigned long *pvalue,unsigned long mask)
{
    ab_card		*pcard = drvPvt;
    unsigned long	value;

    di_read(pcard,&value);
    *pvalue = value & mask;
    return(getStatus(drvPvt));
}

LOCAL abStatus btRequest(ab_card *pcard);

static void btCallback(CALLBACK *pCALLBACK)
{
    ab_card	*pcard;
    btInfo	*pbtInfo;
    int		btStatus;

    callbackGetUser(pcard,pCALLBACK);
    semTake(pcard->card_sem,WAIT_FOREVER);
    pbtInfo = pcard->pbtInfo;
    switch(pbtInfo->reason) {
        case btCallbackRetry :
	    wdCancel(pbtInfo->wdId);
	    if(pbtInfo->reTrys++ <= 10) {
	        btStatus = btRequest(pcard);
    		if(btStatus==abBtqueued) break;
	    }
	    /*break left out on purpose*/
        case btCallbackTimeout :
	    pcard->active = FALSE;
	    pcard->status = abTimeout;
            pbtInfo->btActive = FALSE;
	    (*pcard->callback) ((void *)pcard);
	    break;
        default:
	    printf("abDrv: btCallback logic error\n");
	    
    }
    semGive(pcard->card_sem);
}

LOCAL abStatus btRequest(ab_card *pcard)
{
    int		btStatus;
    STATUS	wdStatus;
    btInfo	*pbtInfo = pcard->pbtInfo;
    CALLBACK	*pcallback = &pbtInfo->callback;

    btStatus=bt_queue(pbtInfo->cmd,pcard,pbtInfo->pmsg,pbtInfo->msg_len);
    if(btStatus==abBtqueued) {
	/* Set timeout for 20 seconds which is 5 times the 68008 SV block */
	/* transfer timout of 4 seconds		 */
	pbtInfo->reason = btCallbackTimeout;
	wdStatus = wdStart(pcard->pbtInfo->wdId,sysClkRateGet()*20,
		(FUNCPTR)callbackRequest,(int)pcallback);
	if(wdStatus==ERROR) {
	    printf("abDrv btRequest : wdStart failure\n");
	    btStatus = abFailure;
	}
    } else if(btStatus!=abAdapterDown) {
	CALLBACK	*pcallback = &pbtInfo->callback;
	STATUS	wdStatus;

	pbtInfo->reason = btCallbackRetry;
	wdStatus = wdStart(pcard->pbtInfo->wdId,sysClkRateGet()/10,
		(FUNCPTR)callbackRequest,(int)pcallback);
	if(wdStatus==ERROR) {
	    printf("abDrv btRequest : wdStart failure\n");
	    btStatus = abFailure;
	} else {
	    btStatus = abBtqueued;
	}
    }
    if(btStatus!=abBtqueued) pcard->active = FALSE;
    return(btStatus);
}

LOCAL abStatus btReadWrite(unsigned short cmd, void *drvPvt,
	unsigned short *pmsg, unsigned short msg_len)
{
    ab_card 	*pcard = drvPvt;
    int		btStatus;
    btInfo	*pbtInfo;

    if(!pcard) return(abFailure);
    pbtInfo = pcard->pbtInfo;
    if(!pbtInfo) return(abFailure);
    semTake(pcard->card_sem,WAIT_FOREVER);
    if(pcard->active) {
	btStatus = abBusy;
    } else {
	pcard->active = TRUE;
	pbtInfo->cmd = cmd;
	pbtInfo->pmsg = pmsg;
	pbtInfo->msg_len = msg_len;
	pbtInfo->reTrys = 0;
	btStatus = btRequest(pcard);
    }
    semGive(pcard->card_sem);
    return(btStatus);
}

LOCAL abStatus btRead(void *drvPvt,
	unsigned short *pread_msg, unsigned short read_msg_len)
{
    return(btReadWrite(AB_READ,drvPvt,pread_msg,read_msg_len));
}

LOCAL abStatus btWrite(void *drvPvt,
	unsigned short *pwrite_msg, unsigned short write_msg_len)
{
    return(btReadWrite(AB_WRITE,drvPvt,pwrite_msg,write_msg_len));
}

LOCAL abStatus adapterStatus(unsigned short link,unsigned short adapter)
{
    ab_adapter	*padapter;
    ab_link	*plink;

    if(link>=max_ab_6008s) return(abFailure);
    if(adapter>=MAX_AB_ADAPTERS) return(abFailure);
    plink = pab_links[link];
    if(!plink || !plink->initialized) return(abFailure);
    padapter = plink->papadapter[adapter];
    if(!padapter || !padapter->adapter_online) return(abAdapterDown);
    return(abSuccess);
}
    
LOCAL abStatus cardStatus(
    unsigned short link,unsigned short adapter,unsigned short card)
{
    ab_adapter	*padapter;
    ab_link	*plink;
    ab_card	*pcard;

    if(link>=max_ab_6008s) return(abFailure);
    if(adapter>=MAX_AB_ADAPTERS) return(abFailure);
    if(card>=MAX_CARDS_PER_ADAPTER) return(abFailure);
    plink = pab_links[link];
    if(!plink || !plink->initialized) return(abFailure);
    padapter = plink->papadapter[adapter];
    if(!padapter || !padapter->adapter_online) return(abAdapterDown);
    pcard = padapter->papcard[card];
    if(!pcard) return(abNoCard);
    if(!padapter->adapter_online) return(abAdapterDown);
    return(pcard->status);
}

LOCAL abDrv abDrvTable= {
	registerCard,getLocation,setNbits,setUserPvt,getUserPvt,
	getStatus,startScan,updateAo,updateBo,readBo,readBi,btRead,btWrite,
	adapterStatus,cardStatus
};
abDrv *pabDrv = &abDrvTable;


/*Time how long it takes to issue link_status */
int ab_time_link_status(int link)
{
    ab_link *plink = pab_links[link];
    int startTicks;
    int stopTicks;
    int pass;
    double tdiff;

    if(!plink) return(-1);
    startTicks = tickGet();
    for (pass=0; pass<100; pass++) link_status(plink);
    stopTicks = tickGet();
    tdiff = (stopTicks - startTicks)/60.0;
    printf("pass/sec %f\n",(float)pass/tdiff);
    return(0);
}
