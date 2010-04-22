/* recAb1791.c */
/*
 *      Author:  Marty Kraimer
 *      Date:   Feb 3,1995
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
#include <tickLib.h>
#include <taskLib.h>
#include <semLib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <logLib.h>

#include "dbDefs.h"
#include "dbScan.h"
#include "errlog.h"
#include "dbEvent.h"
#include "alarm.h"
#include "dbStaticLib.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "errMdef.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "drvAb.h"
#define GEN_SIZE_OFFSET
#include "ab1791Record.h"
#undef  GEN_SIZE_OFFSET
#include "aiRecord.h"
#include "aoRecord.h"
#include <epicsExport.h>


/*function prototype definitions*/
static void monitor(struct ab1791Record *precord);

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
static long special();
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
#define get_units NULL
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

struct rset ab1791RSET={
	RSETNUMBER,
	report,
	initialize,
	init_record,
	process,
	special,
	get_value,
	cvt_dbaddr,
	get_array_info,
	put_array_info,
	get_units,
	get_precision,
	get_enum_str,
	get_enum_strs,
	put_enum_str,
	get_graphic_double,
	get_control_double,
	get_alarm_double };

epicsExportAddress(rset,ab1791RSET);
/*DSET for ai records*/
static long ai_init_record();
static long ai_get_ioint_info();
static long ai_read();
static long ai_special_linconv();
struct {
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	dev_init;
	DEVSUPFUN	dev_init_record;
	DEVSUPFUN	ai_get_ioint_info;
	DEVSUPFUN	ai_read;
	DEVSUPFUN	special_linconv;
}devAiAb1791={
	6,
	NULL,
	NULL,
	ai_init_record,
	ai_get_ioint_info,
	ai_read,
	ai_special_linconv};
epicsExportAddress(dset,devAiAb1791);

/*DSET for ao records*/
static long ao_init_record();
static long ao_write();
static long ao_special_linconv();
struct {
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	dev_init;
	DEVSUPFUN	dev_init_record;
	DEVSUPFUN	ao_get_ioint_info;
	DEVSUPFUN	ao_write;
	DEVSUPFUN	special_linconv;
}devAoAb1791={
	6,
	NULL,
	NULL,
	ao_init_record,
	NULL,
	ao_write,
	ao_special_linconv};
epicsExportAddress(dset,devAoAb1791);

/*Definitions for handling requests from devAiAb1791 and devAoAb1791*/
typedef struct {
	struct ab1791Record *pab1791;
	struct dbAddr	dbAddr;
	short		*pinp;	/*address if INPx */
	unsigned short	channel; /*channel 	*/
}aiDevicePvt;

typedef struct {
	struct ab1791Record *pab1791;
	struct dbAddr	dbAddr;
	short		*pout;	/*address if OUTx */
	unsigned short	channel; /*channel 	*/
}aoDevicePvt;

/*Definitions for communicating with drvAb*/
#define NUMINP 4
#define NUMOUT 2
#define MAXWORDS 64
/*Definitions for handling messages to/from dcm*/
/*1792 registers 				*/
typedef struct {
	unsigned short	status;
	unsigned short	data[MAXWORDS-1];
} btInp;
#define statusPU	0x8000
#define statusBC	0x4000
#define statusOR	0x2000
#define statusSC	0x1f00
#define statusALARM	0x00ff

typedef struct {
	unsigned short	config;
	unsigned short	data[MAXWORDS-1];
} btOut;
#define configNOVI	0x0000	/*Normal operation with voltage inputs	*/
#define configNOCI	0x1000	/*Normal operation with voltage inputs	*/
static unsigned short range[4] = {
0x0, 0x0100, 0x0200, 0x0300}; /*+-10, +-5, 0-10, 0-5*/

typedef enum{btNone,btRead,btConfig,btWrite}btType;
typedef struct {
	IOSCANPVT	ioscanpvt[NUMINP];
	btType		type;
	abStatus	status;
	abStatus	prevStatus;
	short		prevInp[NUMINP];
	short		prevOut[NUMOUT];
	short		newOut[NUMOUT];
	short		needsConfig;
	short		needsWrite;
	short		calledFromCallback;
	short		oldStatus;
	void		*drvPvt;
	short		buffer[MAXWORDS];
}recordPvt;

static void myCallback(void *drvPvt)
{
    struct ab1791Record	*precord;
    recordPvt		*precordPvt;
    struct rset		*prset;

    precord = (*pabDrv->getUserPvt)(drvPvt);
    if(!precord) {
	printf("ab1791Record myCallback precord=NULL????\n");
	return;
    }
    precordPvt = precord->dpvt;
    prset=(struct rset *)(precord->rset);
    precordPvt->status = (*pabDrv->getStatus)(drvPvt);
    dbScanLock((void *)precord);
    precordPvt->calledFromCallback = TRUE;
    (*prset->process)(precord);
    precordPvt->calledFromCallback = FALSE;
    dbScanUnlock((void *)precord);
}

static abStatus configCard(struct ab1791Record *precord)
{
    recordPvt	*precordPvt = precord->dpvt;
    btOut	*pbuffer = (btOut *)precordPvt->buffer;
    int		i;
    abStatus	status;
    
    if(precord->type==0) {
	pbuffer->config =  configNOVI;
    } else {
	pbuffer->config =  configNOCI;
    }
    pbuffer->config |= range[precord->rang];
    if(precord->filt > 0) pbuffer->config |= (precord->filt + 1);
    pbuffer->data[0] = precord->out0;
    pbuffer->data[1] = precord->out1;
    for(i=2; i<MAXWORDS-1; i++) pbuffer->data[i] = 0;
    precordPvt->type = btConfig;
    precordPvt->newOut[0] = FALSE;
    precordPvt->newOut[1] = FALSE;
    status = (*pabDrv->btWrite)(precordPvt->drvPvt,
	(unsigned short *)pbuffer,MAXWORDS);
    return(status);;
}

static abStatus readCard(struct ab1791Record *precord)
{
    recordPvt	*precordPvt = precord->dpvt;
    abStatus	status;

    precordPvt->type = btRead;
    status = (*pabDrv->btRead)(precordPvt->drvPvt,
	(unsigned short *)precordPvt->buffer,5);
    return(status);
}

static abStatus writeCard(struct ab1791Record *precord)
{
    recordPvt	*precordPvt = precord->dpvt;
    btOut	*pbuffer = (btOut *)precordPvt->buffer;
    abStatus	status;
    
    if(precord->type==0) {
	pbuffer->config =  configNOVI;
    } else {
	pbuffer->config =  configNOCI;
    }
    pbuffer->config |= range[precord->rang];
    if(precord->filt > 0) pbuffer->config |= (precord->filt + 1);
    pbuffer->data[0] = precord->out0;
    pbuffer->data[1] = precord->out1;
    precordPvt->type = btWrite;
    precordPvt->newOut[0] = FALSE;
    precordPvt->newOut[1] = FALSE;
    status = (*pabDrv->btWrite)(precordPvt->drvPvt,
	(unsigned short *)pbuffer,3);
    return(status);
}

static long init_record(struct ab1791Record *precord,int pass)
{
    recordPvt	*precordPvt;
    int		i;
    abStatus	status;
    void	*drvPvt;


    if(pass!=0) return(0);
    precord->dpvt = precordPvt = dbCalloc(1,sizeof(recordPvt));
    status = (*pabDrv->registerCard)(precord->link,precord->rack,
	precord->slot,typeBt,"AB1791",myCallback,&drvPvt);
    if(status!=abNewCard) {
	if(status==abSuccess)
	    errlogPrintf("record %s slot already used\n",precord->name);
	else
	    errlogPrintf("record %s init error %s\n",
                precord->name,abStatusMessage[status]);
	precord->pact = TRUE; /*leave record active*/
    }
    (*pabDrv->setUserPvt)(drvPvt,(void *)precord);
    precordPvt->drvPvt = drvPvt;
    precordPvt->needsConfig = TRUE;
    precordPvt->oldStatus = -1;
    for(i=0; i<NUMINP; i++) {
	scanIoInit(&precordPvt->ioscanpvt[i]);
	precordPvt->prevInp[i] = -1;
    }
    for(i=0; i<NUMOUT; i++) {
	precordPvt->prevOut[i] = -1;
    }
    return(0);
}

static long special(struct dbAddr *paddr,int after)
{
    struct ab1791Record *precord = ( struct ab1791Record *)paddr->precord;
    recordPvt		*precordPvt = precord->dpvt;

    precordPvt->needsConfig = TRUE;
    return(0);
}

static long process(struct ab1791Record *precord)
{
    recordPvt	*precordPvt = precord->dpvt;
    abStatus	status=abSuccess;
    int		needsScanOnce = FALSE;

    if(!precordPvt->calledFromCallback) {
	if(precordPvt->needsConfig) {
	    status = configCard(precord);
	} else if(precordPvt->needsWrite) {
	    status = writeCard(precord);
	} else {
	    status = readCard(precord);
	}
	if(status==abBtqueued) {
	    precord->pact = TRUE;
		return(0);
	}
	precordPvt->status = status;
    } else if(precordPvt->status==abSuccess) {
       /*Called by myCallback successfully */
        switch(precordPvt->type) {
	case btRead: {
		btInp		*pinput = (btInp *)precordPvt->buffer;
		unsigned short	card_status = pinput->status;
	    
		precordPvt->type = btNone;
		if(card_status&statusPU) {
		    precordPvt->needsConfig = TRUE;
		    needsScanOnce = TRUE;
		} else if(card_status) {
		    recGblRecordError(status,(void *)precord,"Logic Error 1");
		    precord->pact = TRUE;
		    return(0);
		} else {
		    int	i;

		    for(i=0; i<NUMINP; i++)
			*(&precord->inp0 + i) = pinput->data[i];
		}
	    }
	    break;
	case btConfig:
	    precordPvt->type = btNone;
	    precordPvt->needsConfig = FALSE;
	    needsScanOnce = TRUE;
	    if(precordPvt->newOut[0]==FALSE && precordPvt->newOut[1]==FALSE) {
		precordPvt->needsWrite = FALSE;
	    }
	    break;
	case btWrite:
	    precordPvt->type = btNone;
	    if(precordPvt->newOut[0]==FALSE && precordPvt->newOut[1]==FALSE) {
		precordPvt->needsWrite = FALSE;
	    }
	    needsScanOnce = TRUE;
	    break;
	default:
	    recGblRecordError(-1,(void *)precord,"Logic Error 2");
        }
    }
    if(status) precordPvt->status = status;
    if(needsScanOnce) {
        scanOnce((dbCommon *)precord);
        precord->pact = FALSE;
        return(0);
    }
    if(precordPvt->type==btNone || precordPvt->status!=abBtqueued){
	precordPvt->type = btNone;
	if(precordPvt->status!=abSuccess)
	    recGblSetSevr(precord,READ_ALARM,INVALID_ALARM);
	monitor(precord);
	recGblFwdLink(precord);
	precord->pact = FALSE;
    }
    return(0);
}

static void monitor(struct ab1791Record *precord)
{
    recordPvt		*precordPvt = precord->dpvt;
    unsigned short	monitor_mask;
    int			i;

    monitor_mask = recGblResetAlarms(precord);
    monitor_mask |= (DBE_LOG|DBE_VALUE);
    recGblGetTimeStamp(precord);
    if(precordPvt->oldStatus != precordPvt->status) {
	precordPvt->oldStatus = precordPvt->status;
	strcpy(&precord->lkst[0],abStatusMessage[precordPvt->status]);
	db_post_events(precord,&precord->lkst[0],monitor_mask);
    }
    if(precordPvt->status != abSuccess) {
	precord->nfai++;
	db_post_events(precord,&precord->nfai,DBE_VALUE);
    }
    if(monitor_mask) db_post_events(precord,&precord->lkst[0],monitor_mask);
    for(i=0; i<NUMINP; i++) {
	short	*pfield = (&precord->inp0 + i);

	if(monitor_mask || precordPvt->prevInp[i] != *pfield){
	    db_post_events(precord,pfield,monitor_mask);
	    if((precordPvt->prevInp[i] != *pfield)
	    || (precord->loca && (precordPvt->prevStatus!=precordPvt->status)))
		scanIoRequest(precordPvt->ioscanpvt[i]);
	    precordPvt->prevInp[i] = *pfield;
	}
    }
    for(i=0; i<NUMOUT; i++) {
	short	*pfield = (&precord->out0 + i);
	if(monitor_mask || precordPvt->prevOut[i] != *pfield){
	    db_post_events(precord,pfield,monitor_mask);
	    precordPvt->prevOut[i] = *pfield;
	}
    }
    precordPvt->prevStatus = precordPvt->status;
    return;
}

static long ai_init_record(struct aiRecord *precord)
{
    struct link 	*plink = &precord->inp;
    char		parm[100];
    aiDevicePvt		*pdevicePvt;
    unsigned short	channel;
    char		*period;
    long		status;
    struct ab1791Record *pab1791;
    recordPvt		*precordPvt;

    pdevicePvt = dbCalloc(1,sizeof(aiDevicePvt));
    /*parm has the form record.field where field id INPi*/
    strcpy(parm,plink->value.instio.string);
    period = strchr(parm,'.');
    if(!period) goto error_return;
    if(sscanf(period+1,"INP%hu",&channel)!=1) goto error_return;
    status = dbNameToAddr(parm,&pdevicePvt->dbAddr);
    if(status) {
	recGblRecordError(status,(void *)precord,parm);
	goto error_return;
    }
    pab1791 = (struct ab1791Record *)pdevicePvt->dbAddr.precord;
    precordPvt = pab1791->dpvt;
    pdevicePvt->pab1791 = pab1791;
    pdevicePvt->channel = channel;
    pdevicePvt->pinp = &(pab1791->inp0) + channel;
    precord->dpvt = pdevicePvt;
    precord->eslo = (precord->eguf -precord->egul)/16535.0;
    return(0);
error_return:
    free((void *)pdevicePvt);
    recGblRecordError(S_db_badField,(void *)precord,
	"devAiAb1791 (init_record) Illegal INP field");
    return(S_db_badField);
    
}

static long ai_get_ioint_info(
    int 			cmd,
    struct aiRecord		*precord,
    IOSCANPVT		*ppvt)
{
    aiDevicePvt		*pdevicePvt = precord->dpvt;
    struct ab1791Record *pab1791;
    recordPvt		*precordPvt;

    if(!pdevicePvt) return(0);
    if(!(pab1791 = pdevicePvt->pab1791)) return(0);
    if(!(precordPvt = pab1791->dpvt)) return(0);
    *ppvt = precordPvt->ioscanpvt[pdevicePvt->channel];
    return(0);
}

static long ai_read(struct aiRecord *precord)
{
    aiDevicePvt		*pdevicePvt = precord->dpvt;
    struct ab1791Record *pab1791;
    recordPvt		*precordPvt;

    if(!pdevicePvt || !(pab1791 = pdevicePvt->pab1791)
    || !(precordPvt = pab1791->dpvt)) {
	recGblSetSevr(precord,READ_ALARM,INVALID_ALARM);
	return(0);
    }
    precord->rval = *pdevicePvt->pinp;
    if((precordPvt->status!=abSuccess) && (precordPvt->status != abBtqueued))
	recGblSetSevr(precord,READ_ALARM,INVALID_ALARM);
    return(0);
}

static long ai_special_linconv(struct aiRecord *precord,int	after)
{
    if(!after) return(0);
    /* set linear conversion slope*/
    precord->eslo = (precord->eguf -precord->egul)/16535.0;
    return(0);
}

static long ao_init_record(struct aoRecord *precord)
{
    struct link 	*plink = &precord->out;
    char		parm[100];
    aoDevicePvt		*pdevicePvt;
    unsigned short	channel;
    char		*period;
    long		status;
    struct ab1791Record *pab1791;
    recordPvt		*precordPvt;

    pdevicePvt = dbCalloc(1,sizeof(aoDevicePvt));
    /*parm has the form record.field where field id INPi*/
    strcpy(parm,plink->value.instio.string);
    period = strchr(parm,'.');
    if(!period) goto error_return;
    if(sscanf(period+1,"OUT%hu",&channel)!=1) goto error_return;
    status = dbNameToAddr(parm,&pdevicePvt->dbAddr);
    if(status) {
	recGblRecordError(status,(void *)precord,parm);
	goto error_return;
    }
    pab1791 = (struct ab1791Record *)pdevicePvt->dbAddr.precord;
    precordPvt = pab1791->dpvt;
    pdevicePvt->pab1791 = pab1791;
    pdevicePvt->channel = channel;
    pdevicePvt->pout = &(pab1791->out0) + channel;
    precord->dpvt = pdevicePvt;
    precord->eslo = (precord->eguf -precord->egul)/16535.0;
    return(0);
error_return:
    free((void *)pdevicePvt);
    recGblRecordError(S_db_badField,(void *)precord,
	"devAiAb1791 (init_record) Illegal INP field");
    return(S_db_badField);
    
}

static long ao_write(struct aoRecord *precord)
{
    aoDevicePvt		*pdevicePvt = precord->dpvt;
    struct ab1791Record *pab1791;
    recordPvt		*precordPvt;
    short		rval;

    if(!pdevicePvt || !(pab1791 = pdevicePvt->pab1791)
    || !(precordPvt = pab1791->dpvt)) {
	recGblSetSevr(precord,READ_ALARM,INVALID_ALARM);
	return(0);
    }
    precordPvt->needsWrite = TRUE;
    precordPvt->newOut[pdevicePvt->channel] = TRUE;
    rval = precord->rval;
    if(rval<0) rval = 0;
    if(rval>16535) rval = 16535;
    *(pdevicePvt->pout) = rval;
    if((precordPvt->status!=abSuccess) && (precordPvt->status != abBtqueued))
	recGblSetSevr(precord,WRITE_ALARM,INVALID_ALARM);
    return(0);
}

static long ao_special_linconv(struct aoRecord *precord,int after)
{
    if(!after) return(0);
    /* set linear conversion slope*/
    precord->eslo = (precord->eguf -precord->egul)/16535.0;
    return(0);
}
