/* devIao1.c */

/*
 *      Author: Marty Kraimer
 *      Date:  02JUL1997
 *
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
#include "callback.h"
#include "dbScan.h"
#include "errlog.h"
#include "cvtTable.h"
#include "dbStaticLib.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "devIao1.h"
#include "aoRecord.h"
#include <epicsExport.h>

/* Create the dset*/
LOCAL long init_ao(struct aoRecord *precord);
LOCAL long linconv_ao(struct aoRecord *precord, int after);
LOCAL long read_ao(struct aoRecord *precord);
LOCAL long write_ao(struct aoRecord *precord);


typedef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_ao;
	DEVSUPFUN	special_linconv;} AODSET;

AODSET devInterfaceAO1={6, 0, 0, init_ao, 0, write_ao, linconv_ao};
epicsExportAddress(dset,devInterfaceAO1);

typedef struct devPvt {
    devIao1 *pdevIao1;   /*address of interface*/
    void    *devIao1Pvt; /*private for interface implementation*/
    boolean isEng;
}devPvt;

LOCAL long read_ao(struct aoRecord *precord)
{
    devPvt  *pdevPvt= (devPvt *)precord->dpvt;
    devIao1 *pdevIao1;
    int     devIao1Status;
    double  engValue;
    long    rawValue;

    if(precord->pact) return(0); /*just return and leave active*/
    if(!pdevPvt || !(pdevIao1 = pdevPvt->pdevIao1)) {
                recGblSetSevr(precord,READ_ALARM,INVALID_ALARM);
		return(2); /*dont convert*/
    }
    if(pdevPvt->isEng) {
        devIao1Status = (*pdevIao1->get_eng)(pdevPvt->devIao1Pvt,&engValue);
        if(devIao1Status==devIao1OK) precord->val = engValue;
    } else {
        devIao1Status = (*pdevIao1->get_raw)(pdevPvt->devIao1Pvt,&rawValue);
        if(devIao1Status==devIao1OK) precord->rval = rawValue;
    }
    switch(devIao1Status) {
        case devIao1OK:
            return(((pdevPvt->isEng)?2:0));
        case devIao1HardwareFault:
            recGblSetSevr(precord,READ_ALARM,INVALID_ALARM);
            break;
        default:
            recGblSetSevr(precord,READ_ALARM,INVALID_ALARM);
            errlogPrintf("%s %s\n",precord->name,(*pdevIao1->get_status_message)
		(pdevPvt->devIao1Pvt,devIao1Status));
            precord->pact = TRUE;
            break;
    }
    return(2); /*dont convert*/
}

LOCAL long write_ao(struct aoRecord *precord)
{
    devPvt  *pdevPvt= (devPvt *)precord->dpvt;
    int     devIao1Status;
    devIao1 *pdevIao1;

    if(precord->pact) return(0); /*just return and leave active*/
    if(!pdevPvt || !(pdevIao1 = pdevPvt->pdevIao1)) {
                recGblSetSevr(precord,READ_ALARM,INVALID_ALARM);
		return(2); /*dont convert*/
    }
    if(pdevPvt->isEng) {
        devIao1Status = (*pdevIao1->put_eng)(pdevPvt->devIao1Pvt,precord->val);
    } else {
        devIao1Status = (*pdevIao1->put_raw)(pdevPvt->devIao1Pvt,precord->rval);
    }
    switch(devIao1Status) {
        case devIao1OK:
            break;
        case devIao1HardwareFault:
            recGblSetSevr(precord,READ_ALARM,INVALID_ALARM);
            break;
        default:
            recGblSetSevr(precord,READ_ALARM,INVALID_ALARM);
            errlogPrintf("%s %s\n",precord->name,
		(*pdevIao1->get_status_message)
		(pdevPvt->devIao1Pvt,devIao1Status));
            precord->pact = TRUE;
            break;
    }
    return(0);
}

LOCAL long linconv_ao(struct aoRecord *precord, int after)
{
    devPvt  *pdevPvt= (devPvt *)precord->dpvt;
    devIao1 *pdevIao1;
    int     devIao1Status;
    long    range;
    long    offset;

    if(!after) return(0);
    if(!pdevPvt || !(pdevIao1 = pdevPvt->pdevIao1)) return(0);
    if(pdevPvt->isEng) return(0);
    offset = precord->roff;
    devIao1Status = (*pdevIao1->get_linconv)
	(pdevPvt->devIao1Pvt,&range,&offset);
    precord->roff = offset;
    if(devIao1Status==devIao1OK)
        precord->eslo = (precord->eguf -precord->egul)/(float)range;
    return(0);
}

#define MAX_BUFFER 80
static char *IfieldName = ".IAO1";
LOCAL long init_ao(struct aoRecord *precord)
{
    DBLINK  *plink;
    devPvt  *pdevPvt;
    int     devIao1Status;
    DBADDR  dbAddr;
    long    status;
    char    buffer[MAX_BUFFER];
    char    *pstring;
    devIao1 *pdevIao1;
    size_t  prefix;
 
    plink = &precord->out;
    if(plink->type!=INST_IO) {
        errlogPrintf("%s Illegal link type\n",precord->name);
        precord->pact = TRUE;
        return(2);
    }
    pstring = plink->value.instio.string;
    strcpy(buffer,pstring);
    prefix = strcspn(pstring,".[");
    if(prefix) buffer[prefix] = 0;
    if((strlen(buffer) + strlen(IfieldName) + 1) >= MAX_BUFFER) {
        errlogPrintf("%s OUT value is too long\n",precord->name);
        precord->pact = TRUE;
        return(2);
    }
    strcat(buffer,IfieldName);
    status = dbNameToAddr(buffer,&dbAddr);
    if(status) {
        errlogPrintf("%s dbNameToAddr failed for %s\n",precord->name,buffer);
        precord->pact = TRUE;
        return(2);
    }
    pdevPvt = dbCalloc(1,sizeof(devPvt));
    pdevPvt->pdevIao1 = pdevIao1 = (devIao1 *)dbAddr.pfield;
    precord->dpvt = (devPvt *)pdevPvt;
    devIao1Status = (*pdevIao1->connect)
        (&pdevPvt->devIao1Pvt,&precord->out,&pdevPvt->isEng);
    if(devIao1Status!=devIao1OK) {
        errlogPrintf("%s device init error %s\n",precord->name,
	    (*pdevIao1->get_status_message)
	     (pdevPvt->devIao1Pvt,devIao1Status));
        /*Leave record active so it doesnt get processed*/
        precord->pact = TRUE;
        return(2);
    }
    linconv_ao(precord,TRUE);
    return(read_ao(precord));
}
