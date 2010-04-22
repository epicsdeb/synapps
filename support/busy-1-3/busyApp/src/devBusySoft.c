/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devBusySoft.c */

/* devBusySoft.c - Device Support Routines for  Soft Busy */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "busyRecord.h"
#include "epicsExport.h"

static long init_record();

/* Create the dset for devBusySoft */
static long write_busy();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_busy;
}devBusySoft={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	write_busy
};
epicsExportAddress(dset,devBusySoft);

static long init_record(busyRecord *pbusy)
{
 
   long status=0;
 
    /* dont convert */
   status=2;
   return status;
 
} /* end init_record() */

static long write_busy(busyRecord *pbusy)
{
    long status;

    status = dbPutLink(&pbusy->out,DBR_USHORT,&pbusy->val,1);

    return(status);
}
