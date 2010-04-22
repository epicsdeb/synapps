/* devIai1.c */

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
#include "devIai1.h"
#include "aiRecord.h"
#include <epicsExport.h>

/* Create the dset*/
LOCAL long ioinfo_ai(int cmd,struct aiRecord  *precord,IOSCANPVT *ppvt);
LOCAL long init_ai(struct aiRecord *precord);
LOCAL long linconv_ai(struct aiRecord *precord, int after);
LOCAL long read_ai(struct aiRecord *precord);

typedef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_ai;
	DEVSUPFUN	special_linconv;} AIDSET;

AIDSET devInterfaceAI1={6, 0, 0, init_ai, ioinfo_ai, read_ai, linconv_ai};
epicsExportAddress(dset,devInterfaceAI1);


typedef struct devPvt {
    devIai1 *pdevIai1;   /*address of interface*/
    void    *devIai1Pvt; /*private for interface implementation*/
    boolean isEng;
}devPvt;

LOCAL long ioinfo_ai(int cmd,struct aiRecord  *precord,IOSCANPVT *ppvt)
{
    devPvt	*pdevPvt;
    devIai1     *pdevIai1;

    pdevPvt = (devPvt *)precord->dpvt;
    if(!pdevPvt || !(pdevIai1 = pdevPvt->pdevIai1)) return(0);
    (*pdevIai1->get_ioint_info)(pdevPvt->devIai1Pvt,cmd,ppvt);
    return(0);
}

LOCAL long linconv_ai(struct aiRecord *precord, int after)
{
    devPvt  *pdevPvt= (devPvt *)precord->dpvt;
    devIai1 *pdevIai1;
    int     devIai1Status;
    long    range;
    long    offset;

    if(!after) return(0);
    if(!pdevPvt || !(pdevIai1 = pdevPvt->pdevIai1)) return(0);
    if(pdevPvt->isEng) return(0);
    offset = precord->roff;
    devIai1Status = (*pdevIai1->get_linconv)
        (pdevPvt->devIai1Pvt,&range,&offset);
    precord->roff = offset;
    if(devIai1Status==devIai1OK)
	precord->eslo = (precord->eguf -precord->egul)/(float)range;
    return(0);
}

LOCAL long read_ai(struct aiRecord *precord)
{
    devPvt  *pdevPvt= (devPvt *)precord->dpvt;
    devIai1 *pdevIai1;
    int     devIai1Status;
    double  engValue;
    long    rawValue;

    if(precord->pact) return(0); /*just return and leave active*/
    if(!pdevPvt || !(pdevIai1 = pdevPvt->pdevIai1)) {
        recGblSetSevr(precord,READ_ALARM,INVALID_ALARM);
	return(2); /*dont convert*/
    }
    if(pdevPvt->isEng) {
        devIai1Status = (*pdevIai1->get_eng)(pdevPvt->devIai1Pvt,&engValue);
        if(devIai1Status<=devIai1Overflow) precord->val = engValue;
    } else {
        devIai1Status = (*pdevIai1->get_raw)(pdevPvt->devIai1Pvt,&rawValue);
        if(devIai1Status<=devIai1Overflow) precord->rval = rawValue;
    }
    switch(devIai1Status) {
        case devIai1OK:
	    precord->udf = FALSE;
            return(((pdevPvt->isEng)?2:0));
        case devIai1Underflow:
        case devIai1Overflow:
            recGblSetSevr(precord,HW_LIMIT_ALARM,INVALID_ALARM);
            return(((pdevPvt->isEng)?2:0));
        case devIai1HardwareFault:
            recGblSetSevr(precord,READ_ALARM,INVALID_ALARM);
            break;
        default: 
            recGblSetSevr(precord,READ_ALARM,INVALID_ALARM);
            errlogPrintf("%s %s\n",precord->name,(*pdevIai1->get_status_message)
		(pdevPvt->devIai1Pvt,devIai1Status));
            precord->pact = TRUE;
            break;
    }
    return(2); /*dont let it convert*/
}

#define MAX_BUFFER 80
static char *IfieldName = ".IAI1";
LOCAL long init_ai(struct aiRecord *precord)
{
    DBLINK  *plink;
    devPvt  *pdevPvt;
    int     devIai1Status;
    DBADDR  dbAddr;
    long    status;
    char    buffer[MAX_BUFFER];
    char    *pstring;
    devIai1 *pdevIai1;
    size_t  prefix;

    plink = &precord->inp;
    if(plink->type!=INST_IO) {
	errlogPrintf("%s Illegal link type\n",precord->name);
	precord->pact = TRUE;
	return(2);
    }
    pstring = plink->value.instio.string;
    if(strlen(pstring)>=MAX_BUFFER) {
	errlogPrintf("%s INP value is too long\n",precord->name);
	precord->pact = TRUE;
	return(2);
    }
    strcpy(buffer,pstring);
    prefix = strcspn(pstring,".[");
    if(prefix) buffer[prefix] = 0;
    if((strlen(buffer) + strlen(IfieldName) + 1) >= MAX_BUFFER) {
	errlogPrintf("%s INP value is too long\n",precord->name);
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
    pdevPvt->pdevIai1 = pdevIai1 = (devIai1 *)dbAddr.pfield;
    precord->dpvt = (devPvt *)pdevPvt;
    devIai1Status = (*pdevIai1->connect)
        (&pdevPvt->devIai1Pvt,&precord->inp,&pdevPvt->isEng);
    if(devIai1Status!=devIai1OK) {
        errlogPrintf("%s device init error %s\n",precord->name,
	    (*pdevIai1->get_status_message)
	    (pdevPvt->devIai1Pvt,devIai1Status));
	/*Leave record active so it doesnt get processed*/
	precord->pact = TRUE;
	return(2);
    }
    linconv_ai(precord,TRUE);
    read_ai(precord);
    return(0);
}
