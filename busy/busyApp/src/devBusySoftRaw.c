#include <dbAccess.h>
#include <recGbl.h>
#include <recSup.h>
#include <devSup.h>
#include <busyRecord.h>
#include <epicsExport.h>

static long init_record();
static long write_busy();

struct {
   long 	   number;
   DEVSUPFUN   report;
   DEVSUPFUN   init;
   DEVSUPFUN   init_record;
   DEVSUPFUN   get_ioint_info;
   DEVSUPFUN   write_busy;
}devBusySoftRaw={
   5,
   NULL,
   NULL,
   init_record,
   NULL,
   write_busy
};
epicsExportAddress(dset,devBusySoftRaw);

static long init_record(busyRecord *pbusy)
{
	return 2; /* dont convert */
}

static long write_busy(busyRecord *pbusy)
{
	return dbPutLink(&pbusy->out,DBR_LONG,&pbusy->rval,1);
}
