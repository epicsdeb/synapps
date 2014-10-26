/*************************************************************************\
* Copyright (c) 2006 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devaCalcoutSoft.c */

/*
 *      Author:  Tim Mooney, based on devCalcoutSoft by Marty Kraimer.
 *      Date:    03/21/06
 *
 *      This software serves as both synchronous and asynchronous soft device
 *      support for the acalcoutRecord.c.  Asynchronous soft support means that
 *      device support will use dbCaPutLinkCallback(), with the result that
 *      the record will wait for completion of any processing started by the
 *      'put' operation before executing recGblFwdLink.  The choice (sync or
 *      async) is made with the .WAIT field of the acalcoutRecord.  However,
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
#include "aCalcoutRecord.h"
#include "epicsExport.h"
#include	<callback.h>
#include	<dbCa.h>

static long write_acalcout(acalcoutRecord *pacalcout);
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write;
} devaCalcoutSoft = {
	5,
	NULL,
	NULL,
	NULL,
	NULL,
	write_acalcout
};
epicsExportAddress(dset,devaCalcoutSoft);

volatile int devaCalcoutSoftDebug=0;
epicsExportAddress(int,devaCalcoutSoftDebug);

static long write_acalcout(acalcoutRecord *pacalcout)
{
    struct link		*plink = &pacalcout->out;
    long			status, nelm = 1, i;
	dbAddr			Addr, *pAddr = &Addr;
	void		*	pBuffer;

	if (devaCalcoutSoftDebug) printf("write_acalcout: pact=%d\n", pacalcout->pact);
	if (pacalcout->pact) return(0);

    if (plink->type==CA_LINK) {
		dbCaGetNelements(plink, &nelm);
	} else {
		if (!dbNameToAddr(plink->value.pv_link.pvname, pAddr))
			nelm = pAddr->no_elements;
	}
	if (devaCalcoutSoftDebug) printf("write_acalcout: target nelm=%ld\n", nelm);
	i = (pacalcout->nuse > 0) ? pacalcout->nuse : pacalcout->nelm;
	if (i < nelm) nelm = i;
	if ( pacalcout->dopt == acalcoutDOPT_Use_VAL )
		pBuffer = nelm == 1 ? &pacalcout->val  : pacalcout->aval;
	else
		pBuffer = nelm == 1 ? &pacalcout->oval : pacalcout->oav;

    if ((plink->type==CA_LINK) && (pacalcout->wait)) {
		/* asynchronous */
		if (devaCalcoutSoftDebug) printf("write_acalcout: calling dbCPLCB..DBR_DOUBLE\n");
		status = dbCaPutLinkCallback(	&(pacalcout->out), DBR_DOUBLE, pBuffer, nelm,
										(dbCaCallback)dbCaCallbackProcess, plink );
		if (status) {
			if (devaCalcoutSoftDebug) printf("write_acalcout: dbCaPutLinkCallback returned error\n");
			recGblSetSevr(pacalcout, LINK_ALARM, INVALID_ALARM);
   			return(status);
    	}
		pacalcout->pact = TRUE;

	} else {
		/* synchronous */
		status = dbPutLink(	&(pacalcout->out), DBR_DOUBLE, pBuffer, nelm );
		if (status) {
			if (devaCalcoutSoftDebug) printf("write_acalcout: dbPutLink returned error\n");
			recGblSetSevr(pacalcout, LINK_ALARM, INVALID_ALARM);
   			return(status);
    	}
	}
	return (status);
}


