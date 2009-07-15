/* devAB1771IXE.c */
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

#include "dbDefs.h"
#include "alarm.h"
#include "cvtTable.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "drvAb.h"
#include "aiRecord.h"
#include "menuConvert.h"
#include <epicsExport.h>

/* Create the dsets*/
LOCAL long ioinfo_ai(int cmd,struct aiRecord  *prec,IOSCANPVT *ppvt);
LOCAL long init_1771Ixe(struct aiRecord *prec);
LOCAL long linconv_1771Ixe(struct aiRecord *prec, int after);
LOCAL long read_1771Ixe(struct aiRecord *prec);

typedef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_ai;
	DEVSUPFUN	special_linconv;} ABAIDSET;

ABAIDSET devAiAb1771Ixe={6, 0, 0, init_1771Ixe,
	ioinfo_ai, read_1771Ixe, linconv_1771Ixe};
epicsExportAddress(dset,devAiAb1771Ixe);

#define IXE_HALFSEC_RATE	5
#define IXE_1SEC_RATE		10
#define	IXE_INITMSG_LENGTH	19
#define	IXE_READMSG_LENGTH	12
#define IXE_NUM_CVTTYPES	13

typedef struct {
	void		*drvPvt;
	IOSCANPVT	ioscanpvt;
	unsigned short	init_msg[IXE_INITMSG_LENGTH];
	unsigned short	read_msg[IXE_READMSG_LENGTH];
	unsigned short	indCvt;
} devPvt;

/* xxxxxxxxxxxxxTTT - Thermocouple Types			*/
#define IXE_MILLI	0x0000		/* Millivolt input */
#define IXE_E		0x0001		/* "E" Thermocouple */
#define IXE_J		0x0002		/* "J" Thermocouple */
#define IXE_K		0x0003		/* "K" Thermocouple */
#define IXE_T		0x0004		/* "T" Thermocouple */
#define IXE_R		0x0005		/* "R" Thermocouple */
#define IXE_S		0x0006		/* "S" Thermocouple */
/* xxxxxxxCxxxxxxxx - Conversion into degrees F or C */
#define	IXE_DEGC	0x0000
#define IXE_DEGF	0x0100
/* xxxxxFFxxxxxxxxx - Data Format */
#define IXE_SIGNED	0x0400		/* signed magnitude  "     "   */
/* SSSSSxxxxxxxxxxx - Scan Rate */
#define IXE_HALFSEC		0x2800		/* sample time = 0.5 seconds */
#define IXE_1SEC		0x5000		/* sample time = 1.0 seconds */
#define IXE_2SECS		0xa000		/* sample time = 2.0 seconds */
#define IXE_3SECS		0xf000		/* sample time = 3.0 seconds */

#define IXE_STATUS	0xff
struct ab1771Ixe_read {
	unsigned short	pol_stat;	/* status - polarity word */
	unsigned short	out_of_range;	/* under - over range channels */
	unsigned short	alarms;		/* inputs outside alarm limits */
	short		data[8];	/* current values */
	unsigned short	cjcw;		/* cold junction cal word */
};

LOCAL unsigned short ixe_cvt[IXE_NUM_CVTTYPES] = {
	IXE_MILLI | IXE_SIGNED | IXE_HALFSEC,
	IXE_K | IXE_DEGF | IXE_SIGNED | IXE_HALFSEC,
	IXE_K | IXE_DEGC | IXE_SIGNED | IXE_HALFSEC,
	IXE_J | IXE_DEGF | IXE_SIGNED | IXE_HALFSEC,
	IXE_J | IXE_DEGC | IXE_SIGNED | IXE_HALFSEC,
	IXE_E | IXE_DEGF | IXE_SIGNED | IXE_HALFSEC,
	IXE_E | IXE_DEGC | IXE_SIGNED | IXE_HALFSEC,
	IXE_T | IXE_DEGF | IXE_SIGNED | IXE_HALFSEC,
	IXE_T | IXE_DEGC | IXE_SIGNED | IXE_HALFSEC,
	IXE_R | IXE_DEGF | IXE_SIGNED | IXE_HALFSEC,
	IXE_R | IXE_DEGC | IXE_SIGNED | IXE_HALFSEC,
	IXE_S | IXE_DEGF | IXE_SIGNED | IXE_HALFSEC,
	IXE_S | IXE_DEGC | IXE_SIGNED | IXE_HALFSEC
};
LOCAL const char* cardName[IXE_NUM_CVTTYPES] = {
	"IXE_MV","IXE_KDEGF","IXE_KDEGC",
	"IXE_JDEGF","IXE_JDEGC","IXE_EDEGF","IXE_EDEGC",
	"IXE_TDEGF","IXE_TDEGC","IXE_RDEGF","IXE_RDEGC",
	"IXS_TDEGF","IXE_SDEGC"
};
LOCAL unsigned short scanRate[IXE_NUM_CVTTYPES] = {
	IXE_HALFSEC_RATE,IXE_HALFSEC_RATE,IXE_HALFSEC_RATE,
	IXE_HALFSEC_RATE,IXE_HALFSEC_RATE,IXE_HALFSEC_RATE,IXE_HALFSEC_RATE,
	IXE_HALFSEC_RATE,IXE_HALFSEC_RATE,
	IXE_HALFSEC_RATE,IXE_HALFSEC_RATE,IXE_HALFSEC_RATE,IXE_HALFSEC_RATE
};
LOCAL int mapMenuConvertToType[IXE_NUM_CVTTYPES-1][2] = {
    {menuConverttypeKdegF,1},{menuConverttypeKdegC,2},
    {menuConverttypeJdegF,3},{menuConverttypeJdegC,4},
    {menuConverttypeEdegF,5},{menuConverttypeEdegC,6},
    {menuConverttypeTdegF,7},{menuConverttypeTdegC,8},
    {menuConverttypeRdegF,9},{menuConverttypeRdegC,10},
    {menuConverttypeSdegF,11},{menuConverttypeSdegC,12}
};
    

LOCAL void devCallback(void * drvPvt)
{
    devPvt	*pdevPvt;

    pdevPvt = (devPvt *)(*pabDrv->getUserPvt)(drvPvt);
    if(!pdevPvt) return;
    scanIoRequest(pdevPvt->ioscanpvt);
}

LOCAL long ioinfo_ai(int cmd,struct aiRecord  *prec,IOSCANPVT *ppvt)
{
    devPvt	*pdevPvt;

    pdevPvt = (devPvt *)prec->dpvt;
    if(!pdevPvt) return(0);
    *ppvt = pdevPvt->ioscanpvt;
    return(0);
}

LOCAL long read_1771Ixe(struct aiRecord *prec)
{
	struct abio 		*pabio;
	devPvt			*pdevPvt= (devPvt *)prec->dpvt;
	abStatus		drvStatus;
        struct ab1771Ixe_read	*pdata;
	long			status;
	unsigned short		indCvt;
	short			rval;

	if(!pdevPvt) {
                recGblSetSevr(prec,READ_ALARM,INVALID_ALARM);
		return(2); /*dont convert*/
	}
	indCvt = pdevPvt->indCvt;
	pabio = (struct abio *)&(prec->inp.value);
	drvStatus = (*pabDrv->getStatus)(pdevPvt->drvPvt);
	pdata = (struct ab1771Ixe_read *)&pdevPvt->read_msg[0];
	if(drvStatus != abSuccess) {
                recGblSetSevr(prec,READ_ALARM,INVALID_ALARM);
		return(2); /*dont convert*/
	}
	rval = pdata->data[pabio->signal];	
	if((pdata->pol_stat&(0x100<<pabio->signal))) rval = -rval;
	if((pdata->out_of_range&(1<<pabio->signal))
	|| (pdata->out_of_range&(0x100<<pabio->signal)) ) {
	     recGblSetSevr(prec,HW_LIMIT_ALARM,INVALID_ALARM);
	}
	if(indCvt!=0) {
	    prec->val = prec->rval = rval;
	    prec->udf = FALSE;
	    status=2; /*don't convert*/
	} else {
	    prec->rval = rval + 10000;
	    status = 0;
	}
	return(status);
}

static long linconv_1771Ixe(struct aiRecord *prec, int after)
{

    /* set linear conversion slope*/
    prec->eslo = (prec->eguf -prec->egul)/20000.0;
    return(0);
}

LOCAL long init_1771Ixe(struct aiRecord	*prec)
{
    struct abio 	*pabio;
    devPvt		*pdevPvt;
    abStatus 		drvStatus;
    long		status=0;
    void		*drvPvt;
    unsigned short	indCvt,ind;

    /* ai.inp must be an AB_IO */
    if (prec->inp.type != AB_IO){
    	recGblRecordError(S_db_badField,(void *)prec,
    		"devAiAb1771Ixe (init_record) Illegal INP field");
    	return(S_db_badField);
    }
    /* THE FOLLOWING ASSUMES THE APPLICATION HAS THE SAME menuConvert,h*/
    /* As allenBradley. NOT NICE */
    /* check menu choice */
    indCvt = 0;
    for(ind=0; ind<IXE_NUM_CVTTYPES-1; ind++) {
        if(prec->linr == mapMenuConvertToType[ind][0]) {
            indCvt = mapMenuConvertToType[ind][1];
            break;
	    prec->linr = 0;
        }
    }
    prec->eslo = (prec->eguf -prec->egul)/20000.0;
    /* pointer to the data addess structure */
    pabio = (struct abio *)&(prec->inp.value);
    drvStatus = (*pabDrv->registerCard)(pabio->link,pabio->adapter,
    	pabio->card,typeAi,cardName[indCvt],devCallback,&drvPvt);
    switch(drvStatus) {
    case abSuccess :
        pdevPvt = (devPvt *)(*pabDrv->getUserPvt)(drvPvt);
        prec->dpvt = pdevPvt; 
	break;
    case abNewCard :
        pdevPvt = calloc(1,sizeof(devPvt));
        pdevPvt->drvPvt = drvPvt;
        prec->dpvt = pdevPvt; 
	pdevPvt->indCvt = indCvt;
        (*pabDrv->setUserPvt)(drvPvt,(void *)pdevPvt);
        scanIoInit(&pdevPvt->ioscanpvt);
        pdevPvt->init_msg[0] = ixe_cvt[indCvt];
        drvStatus = (*pabDrv->startScan)(drvPvt,scanRate[indCvt],
		&pdevPvt->init_msg[0],IXE_INITMSG_LENGTH,
		&pdevPvt->read_msg[0],IXE_READMSG_LENGTH);
	if(drvStatus != abSuccess) {
	    status = S_db_badField;
    	    recGblRecordError(status,(void *)prec,
    		"devAiAb1771Ixe (init_record) startScan");
	}
	break;
    default:
	status = S_db_badField;
    	recGblRecordError(status,(void *)prec,
    		"devAiAb1771Ixe (init_record) registerCard");
	break;
    }
    return(status);
}
