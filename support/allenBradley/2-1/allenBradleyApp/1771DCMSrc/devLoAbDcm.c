/* devLoAbDcm.c */
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
#include "longoutRecord.h"
#include <epicsExport.h>

/*DSET for ao records*/
static long lo_init();
static long lo_init_record();
static long lo_write();
struct {
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	dev_init;
	DEVSUPFUN	dev_init_record;
	DEVSUPFUN	lo_get_ioint_info;
	DEVSUPFUN	lo_write;
}devLoAbDcm={
	6,
	NULL,
	lo_init,
	lo_init_record,
	NULL,
	lo_write};
epicsExportAddress(dset,devLoAbDcm);
static abDcm *pabDcm=NULL;

typedef struct devicePvt {
	void		*pabDcmPvt;
	unsigned short	tag;
}devicePvt;

static long lo_init(int after)
{
    if(!after) {
	STATUS		rtn;
	SYM_TYPE	type;

	rtn = symFindByNameEPICS(sysSymTbl,"_abDcmTable",(void *)&pabDcm,&type);
	if(rtn!=OK) errlogPrintf("devLoAbDcm: cant locate abDcmTable\n");
    }
    return(0);
}

static long lo_init_record(longoutRecord *precord)
{
    struct link 	*plink = &precord->out;
    abDcmStatus		dcmStatus;
    char		*parm=NULL;
    unsigned short	tag;
    char		*bracket;
    devicePvt		*pdevicePvt;
    char                parmSmall[80];
    int                 parmSize=0;

    if(!pabDcm) {
	precord->pact = TRUE; 
	return(0);
    }
    pdevicePvt = dbCalloc(1,sizeof(devicePvt));
    precord->dpvt = (void *)pdevicePvt;
    parmSize = strlen(plink->value.instio.string)+1;
    parm = ((parmSize<=80) ? &parmSmall[0] : (char *)malloc(parmSize));
    strcpy(parm,plink->value.instio.string);
    /*parm has the form pvname[tag]*/
    bracket = strchr(parm,'[');
    if(!bracket) {
	errlogPrintf("record %s OUT %s is illegal\n",precord->name,parm);
	precord->pact = TRUE; goto done;
    }
    if(sscanf(bracket,"[%hu]",&tag)!=1) {
	errlogPrintf("record %s OUT has illegal tag\n",precord->name);
	precord->pact = TRUE; goto done;
    }
    pdevicePvt->tag = tag;
    *bracket = 0;
    dcmStatus = (*pabDcm->connect)(&pdevicePvt->pabDcmPvt,parm,0,0,1);
    if(dcmStatus!=abDcmOK) {
	errlogPrintf("record %s abDcm.connect error %s\n",
	    precord->name,pabDcm->abDcmStatusMessage[dcmStatus]);
	precord->pact = TRUE;
    }
done:
    if(parmSize>80) free((void *)parm);
    return(0);
}


static void myCallback(void *userPvt,abDcmStatus status)
{
    longoutRecord *precord = (longoutRecord *)userPvt;
    struct rset	*prset=(struct rset *)(precord->rset);

    dbScanLock((struct dbCommon *)precord);
    if(status) recGblSetSevr(precord,COMM_ALARM,INVALID_ALARM);
    (*prset->process)(precord);
    dbScanUnlock((struct dbCommon *)precord);
}
    
static long lo_write(longoutRecord *precord)
{
    devicePvt	*pdevicePvt = precord->dpvt;
    short	value;
    abDcmStatus	dcmStatus;

    pdevicePvt = (devicePvt *)(precord->dpvt);
    if(!pabDcm || !pdevicePvt || !pdevicePvt->pabDcmPvt) {
	recGblSetSevr(precord,READ_ALARM,INVALID_ALARM);
	return(0);
    }
    if(precord->pact) {/*all done*/
	return(0);
    }
    value = (short)precord->val;
    dcmStatus = (*pabDcm->put_short)(pdevicePvt->pabDcmPvt,
	pdevicePvt->tag,(unsigned short)value,myCallback,(void *)precord);
    if(dcmStatus!=abDcmOK) {
	recGblSetSevr(precord,WRITE_ALARM,INVALID_ALARM);
	return(0);
    }
    precord->pact = TRUE;
    return(0);
}
