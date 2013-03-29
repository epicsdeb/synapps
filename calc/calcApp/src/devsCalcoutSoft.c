/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devsCalcoutSoft.c */

/*
 *      Author:  Tim Mooney, based on devCalcoutSoft by Marty Kraimer.
 *      Date:    11/9/04
 *
 *      This software serves as both synchronous and asynchronous soft device
 *      support for the scalcoutRecord.c.  Asynchronous soft support means that
 *      device support will use dbCaPutLinkCallback(), with the result that
 *      the record will wait for completion of any processing started by the
 *      'put' operation before executing recGblFwdLink.  The choice (sync or
 *      async) is made with the .WAIT field of the scalcoutRecord.  However,
 *      if the OUT link is not a CA link, device support must fall back to
 *      synchronous behavior, because it's illegal to use dbCaPutLinkCallback()
 *      on a database link.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "special.h"
#include "sCalcPostfix.h"	/* needed for SCALC_INFIX_TO_POSTFIX_SIZE in sCalcoutRecord.h */
#include "sCalcoutRecord.h"
#include "epicsExport.h"
#include	<callback.h>
#include	<dbCa.h>

static long write_scalcout(scalcoutRecord *pscalcout);
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write;
} devsCalcoutSoft = {
	5,
	NULL,
	NULL,
	NULL,
	NULL,
	write_scalcout
};
epicsExportAddress(dset,devsCalcoutSoft);

volatile int devsCalcoutSoftDebug=0;
epicsExportAddress(int, devsCalcoutSoftDebug);

static long write_scalcout(scalcoutRecord *pscalcout)
{
    struct link		*plink = &pscalcout->out;
    /* epicsInt32		status, n_elements=1;*/
    long			status, n_elements=1;
	short			field_type = 0;
	dbAddr			Addr;
	dbAddr			*pAddr = &Addr;

	if (devsCalcoutSoftDebug) printf("write_scalcout: pact=%d\n", pscalcout->pact);
	if (pscalcout->pact) return(0);

    if ((plink->type==CA_LINK) && (pscalcout->wait)) {

		/* asynchronous */
		field_type = dbCaGetLinkDBFtype(plink);
		if (devsCalcoutSoftDebug) printf("write_scalcout: field_type=%d\n", field_type);
		switch (field_type) {
		case DBF_STRING: case DBF_ENUM: case DBF_MENU: case DBF_DEVICE:
		case DBF_INLINK: case DBF_OUTLINK: case DBF_FWDLINK: 
			if (devsCalcoutSoftDebug) printf("write_scalcout: calling dbCPLCB..DBR_STRING\n");
			status = dbCaPutLinkCallback(&(pscalcout->out), DBR_STRING,
				&(pscalcout->osv), 1, (dbCaCallback)dbCaCallbackProcess, plink);
			break;
		default:
			dbCaGetNelements(plink, &n_elements);
			if (n_elements>sizeof(pscalcout->sval)) n_elements = sizeof(pscalcout->sval);
			if (((field_type==DBF_CHAR) || (field_type==DBF_UCHAR)) && (n_elements>1)) {
				if (devsCalcoutSoftDebug) printf("write_scalcout: dbCaPutLinkCallback %ld characters\n", n_elements);
				status = dbCaPutLinkCallback(&(pscalcout->out), DBF_CHAR,
					&(pscalcout->osv), n_elements, (dbCaCallback)dbCaCallbackProcess, plink);
			} else {
				if (devsCalcoutSoftDebug) printf("write_scalcout: calling dbCPLCB..DBR_DOUBLE\n");
				status = dbCaPutLinkCallback(&(pscalcout->out), DBR_DOUBLE,
					&(pscalcout->oval), 1, (dbCaCallback)dbCaCallbackProcess, plink);
			}
		}
		if (status) {
			if (devsCalcoutSoftDebug) printf("write_scalcout: dbCPLCB returned error\n");
			recGblSetSevr(pscalcout, LINK_ALARM, INVALID_ALARM);
   			return(status);
    	}
		pscalcout->pact = TRUE;

	} else {

		/* synchronous */
		switch (plink->type) {
		case CA_LINK:
			field_type = dbCaGetLinkDBFtype(plink);
			dbCaGetNelements(plink, &n_elements);
			break;
		case DB_LINK:
			if (!dbNameToAddr(plink->value.pv_link.pvname, pAddr)) {
				field_type = pAddr->field_type;
				n_elements = pAddr->no_elements;
			}
			break;
		default:
			break;
		}

		switch (field_type) {
		case DBF_STRING: case DBF_ENUM: case DBF_MENU: case DBF_DEVICE:
		case DBF_INLINK: case DBF_OUTLINK: case DBF_FWDLINK:
			status = dbPutLink(&(pscalcout->out), DBR_STRING,&(pscalcout->osv),1);
			break;
		default:
			if (n_elements>sizeof(pscalcout->sval)) n_elements = sizeof(pscalcout->sval);
			if (((field_type==DBF_CHAR) || (field_type==DBF_UCHAR)) && (n_elements>1)) {
				if (devsCalcoutSoftDebug) printf("write_scalcout: dbPutLink %ld characters\n", n_elements);
				status = dbPutLink(&(pscalcout->out), DBF_CHAR, &(pscalcout->sval), n_elements);
			} else {
				status = dbPutLink(&(pscalcout->out), DBR_DOUBLE,&(pscalcout->oval),1);
			}
		}

	}
	return (status);
}
