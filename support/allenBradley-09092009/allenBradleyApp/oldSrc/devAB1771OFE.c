/* devAB1771OFE.c */
/*
 *      Original Author: Bob Dalesio
 *      Current Author: Bob Dalesio, Marty Kraimer
 *      Date:  3/6/91
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sysLib.h>
#include <taskLib.h>

#include "dbDefs.h"
#include "alarm.h"
#include "cvtTable.h"
#include "dbAccess.h"
#include "errlog.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "drvAb.h"
#include "aoRecord.h"
#include <epicsExport.h>

/* Create the dsets*/
LOCAL long init_1771Ofe(struct aoRecord *pao);
LOCAL long write_1771Ofe(struct aoRecord *pao);
LOCAL long linconv_1771Ofe(struct aoRecord *pao, int after);
typedef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_ao;
	DEVSUPFUN	special_linconv;
}ABAODSET;

ABAODSET devAoAb1771Ofe={ 6, NULL, NULL, init_1771Ofe, NULL,
			write_1771Ofe, linconv_1771Ofe};
epicsExportAddress(dset,devAoAb1771Ofe);

#define UPDATE_RATE	100
#define	READ_MSG_LEN	5
#define	WRITE_MSG_LEN	5

	
/* defines and structures for analog outputs	*/
/* configuration word 5 for the OFE module			*/
/*	FxxxHHHHLLLLPPPP					*/
/*		F - Data Format					*/
/*			0x0 specifies BCD			*/
/*			0x1 specifies Binary			*/
/*		HHHH - Max Scaling Polarity			*/
/*		LLLL - Min Scaling Polarity			*/
/*		PPPP - Value Polarity				*/
#define	OFE_BINARY	0x8000		/* talk binary instead of BCD	*/
#define	OFE_SCALING	0x0000		/* all positive			*/

typedef struct {
	void		*drvPvt;
	unsigned short	read_msg[READ_MSG_LEN];
	unsigned short	write_msg[WRITE_MSG_LEN];
}devPvt;

LOCAL long init_1771Ofe(struct aoRecord *prec)
{
	struct abio 	*pabio;
	devPvt		*pdevPvt;
	abStatus 	drvStatus;
	long		status=0;
	void		*drvPvt;
	int		failed;
	int		i;

	if (prec->out.type != AB_IO){
		recGblRecordError(S_db_badField,(void *)prec,
			"devAoAb1771Ofe (init_record) Illegal INP field");
		return(S_db_badField);
	}
	/* set linear conversion slope*/
	prec->eslo = (prec->eguf -prec->egul)/4095.0;
	/* pointer to the data addess structure */
	pabio = (struct abio *)&(prec->out.value);
	drvStatus = (*pabDrv->registerCard)(pabio->link,pabio->adapter,
		pabio->card,typeAo,"OFE",NULL,&drvPvt);
	switch(drvStatus) {
	case abSuccess :
	    pdevPvt = (devPvt *)(*pabDrv->getUserPvt)(drvPvt);
	    prec->dpvt = pdevPvt; 
	    drvStatus = (*pabDrv->getStatus)(drvPvt);
	    if(drvStatus==abSuccess) {
		prec->rval = (unsigned short)pdevPvt->read_msg[pabio->signal];
	    } else {
		status = 2;
	    }
	    break;
	case abNewCard :
	    pdevPvt = calloc(1,sizeof(devPvt));
	    pdevPvt->drvPvt = drvPvt;
	    prec->dpvt = pdevPvt; 
	    (*pabDrv->setUserPvt)(drvPvt,(void *)pdevPvt);
	    pdevPvt->write_msg[4] = OFE_BINARY | OFE_SCALING;
	    drvStatus = (*pabDrv->startScan)(drvPvt,UPDATE_RATE,
		pdevPvt->write_msg,WRITE_MSG_LEN,
		pdevPvt->read_msg ,READ_MSG_LEN);
	    if(drvStatus!=abSuccess) {
		status = S_db_badField;
		recGblRecordError(status,(void *)prec,
			"devAoAb1771Ofe (init_record) startScan");
		break;
	    }
	    /*wait for up to 3 seconds*/
	    for(failed=0; failed<30; failed++) {
		taskDelay(sysClkRateGet()/10);
	        drvStatus = (*pabDrv->getStatus)(drvPvt);
		if(drvStatus==abSuccess) {
		    prec->rval=(unsigned short)pdevPvt->read_msg[pabio->signal];
		    for(i=0;i<4;i++) {
			pdevPvt->write_msg[i] = pdevPvt->read_msg[i];
		    }
		    write_1771Ofe(prec);
		    status = 2;
		    break;
		}
	    }
	    status = 0;
	    break;
	default:
	    status = S_db_badField;
	    recGblRecordError(status,(void *)prec,
			"devAoAb1771Ofe (init_record) registerCard");
	    break;
	}
	return(status);
}

LOCAL long write_1771Ofe(struct aoRecord *prec)
{
	struct abio	*pabio;
	devPvt		*pdevPvt = (devPvt *)prec->dpvt;
	abStatus	drvStatus;
	void		*drvPvt;
	
	if(!pdevPvt) {
	    recGblSetSevr(prec,READ_ALARM,INVALID_ALARM);
	    return(2); /*dont convert*/
	}
	drvPvt = pdevPvt->drvPvt;
	pabio = (struct abio *)&(prec->out.value);
	pdevPvt->write_msg[pabio->signal] = prec->rval;
	drvStatus = (*pabDrv->updateAo)(drvPvt);
	if(drvStatus!=abSuccess) {
                if(recGblSetSevr(prec,WRITE_ALARM,INVALID_ALARM) && errVerbose
		&& (prec->stat!=WRITE_ALARM || prec->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)prec,"abDrv(updateAo)");
	}
	return(0);
}


LOCAL long linconv_1771Ofe(struct aoRecord *prec, int after)
{

    if(!after) return(0);
    /* set linear conversion slope*/
    prec->eslo = (prec->eguf -prec->egul)/4095.0;
    return(0);
}
