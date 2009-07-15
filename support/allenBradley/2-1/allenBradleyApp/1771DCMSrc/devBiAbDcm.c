/* devBiAbDcm.c */
/*
 *      Author:  Marty Kraimer
 *      Date:   June 9, 1995
 *
 *	Major revision July 22, 1996
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <logLib.h>

#include "dbDefs.h"
#include "epicsDynLink.h"
#include "errlog.h"
#include "dbScan.h"
#include "alarm.h"
#include "dbDefs.h"
#include "dbStaticLib.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "errMdef.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "abDcm.h"
#include "biRecord.h"
#include <epicsExport.h>

/*DSET for bi records*/
static long bi_init();
static long bi_init_record();
static long bi_get_ioint_info();
static long bi_read();
struct {
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	dev_init;
	DEVSUPFUN	dev_init_record;
	DEVSUPFUN	bi_get_ioint_info;
	DEVSUPFUN	bi_read;
}devBiAbDcm={
	5,
	NULL,
	bi_init,
	bi_init_record,
	bi_get_ioint_info,
	bi_read};
epicsExportAddress(dset,devBiAbDcm);
static abDcm *pabDcm=NULL;

typedef struct devicePvt {
	void		*pabDcmPvt;
	unsigned short	bit;
}devicePvt;

static long bi_init(int after)
{
    if(!after) {
	STATUS		rtn;
	SYM_TYPE	type;

	rtn = symFindByNameEPICS(sysSymTbl,"_abDcmTable",(void *)&pabDcm,&type);
	if(rtn!=OK) errlogPrintf("devBiAbDcm: cant locate abDcmTable\n");
    }
    return(0);
}

static long bi_init_record(struct biRecord *precord)
{
    struct link 	*plink = &precord->inp;
    char		*parm=NULL;
    unsigned short	table,word,bit;
    char		*period;
    devicePvt		*pdevicePvt;
    abDcmStatus		dcmStatus;
    char                parmSmall[80];
    int                 parmSize=0;

    if(!pabDcm) {
	precord->pact = TRUE; goto done;
    }
    pdevicePvt = dbCalloc(1,sizeof(devicePvt));
    precord->dpvt = (void *)pdevicePvt;
    parmSize = strlen(plink->value.instio.string)+1;
    parm = ((parmSize<=80) ? &parmSmall[0] : (char *)malloc(parmSize));
    strcpy(parm,plink->value.instio.string);
    /*parm has the form pvname.Tx[word,bit]*/
    period = strchr(parm,'.');
    if(!period) {
	errlogPrintf("record %s INP %s is illegal\n",precord->name,parm);
	precord->pact = TRUE; goto done;
    }
    if(sscanf(period+1,"T%hu[%hu,%hu]",&table,&word,&bit)!=3) {
	errlogPrintf("record %s INP %s is illegal\n",precord->name,period+1);
	precord->pact = TRUE; goto done;
    }
    *period = 0;
    if(bit>=16) {
	errlogPrintf("record %s INP %s has illegal bit (>=16)\n",
	    precord->name,period+1);
	precord->pact = TRUE; goto done;
    }
    pdevicePvt->bit = bit;
    dcmStatus = (*pabDcm->connect)(&pdevicePvt->pabDcmPvt,
	parm,table,word,1);
    if(dcmStatus!=abDcmOK) {
	errlogPrintf("record %s abDcm.connect error %s\n",
	    precord->name,pabDcm->abDcmStatusMessage[dcmStatus]);
	precord->pact = TRUE; goto done;
    }
done:
    if(parmSize>80) free((void *)parm);
    return(0);
}

static long bi_get_ioint_info(int cmd,struct biRecord *precord,
    IOSCANPVT *ppvt)
{
    devicePvt		*pdevicePvt;

    if(!pabDcm) return(0);
    pdevicePvt = (devicePvt *)(precord->dpvt);
    if(!pdevicePvt || !pdevicePvt->pabDcmPvt) return(0);
    (*pabDcm->get_ioint_info)(pdevicePvt->pabDcmPvt,cmd,ppvt);
    return(0);
}

static long bi_read(struct biRecord *precord)
{
    devicePvt		*pdevicePvt;
    unsigned short	mask,value;
    abDcmStatus		dcmStatus;

    pdevicePvt = (devicePvt *)(precord->dpvt);
    if(!pabDcm || !pdevicePvt || !pdevicePvt->pabDcmPvt) {
	recGblSetSevr(precord,READ_ALARM,INVALID_ALARM);
	return(2);
    }
    dcmStatus = (*pabDcm->get_ushort)(pdevicePvt->pabDcmPvt,&value);
    if(dcmStatus!=abDcmOK) {
	recGblSetSevr(precord,READ_ALARM,INVALID_ALARM);
	return(2);
    }
    mask = 1;
    mask <<= pdevicePvt->bit;
    value &= mask;
    precord->val = (value ? TRUE : FALSE);
    precord->udf = FALSE;
    return(2);
}
