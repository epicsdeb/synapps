/*
FILENAME...	devAiHeidND261.c
USAGE...	Heidenhain ND261 device support.

Version:	$Revision: 1.6 $
Modified By:	$Author: rivers $
Last Modified:	$Date: 2009-09-01 13:31:24 $
*/

/*
 * Author: Tim Mooney (based on code written by Mark Rivers)
 * 10/7/04
 *
 * Modification Log:
 * -----------------
 * .01 05/31/06 rls asyn R4-5 compatible; asyn allows only 2 character EOS.
 * .02 09/01/06 rls init_record() has only one input argument.
 */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <dbScan.h>
#include <dbDefs.h>
#include <dbAccess.h>
#include <dbCommon.h>
#include <errlog.h>
#include <alarm.h>
#include <link.h>
#include <recGbl.h>
#include <recSup.h>
#include <devSup.h>
#include <asynDriver.h>
#include <asynEpicsUtils.h>
#include <asynOctet.h>
#include <aiRecord.h>
#include <epicsExport.h>

static long init_record(void *);
static void devAiHeidND261Callback(asynUser *pasynUser);
static long io(dbCommon* pr);
static long completeIO(dbCommon* pr);
 
#ifdef NODEBUG
#define DEBUG(l,f,v) ;
#else
#define DEBUG(l,f,v) { if(l<=devAiHeidND261Debug) printf(f,v); }
#endif
volatile int devAiHeidND261Debug = 0;
 
#define HEXCHAR2INT(c) ((c)>'9' ? 10+tolower(c)-'a' : tolower(c)-'0')
 
#define BUFSZ 19 
typedef struct devAiHeidND261Pvt {
	asynUser	*pasynUser;
	asynOctet	*pasynOctet;
	void		*octetPvt;
	asynStatus	status;
	size_t		nread;
	size_t		nwrite;
	char		outbuf[BUFSZ], inbuf[BUFSZ];
	char		term[4];
	char		format[10];
	int		termlen;
	double		timeout;
	int		nchar;
} devAiHeidND261Pvt;

typedef struct dsetAiHeidND261 {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	io;
	DEVSUPFUN	convert;
} dsetAiHeidND261;

dsetAiHeidND261 devAiHeidND261 = {6,0,0,init_record,0,io,0};
epicsExportAddress(dset,devAiHeidND261);

static long init_record(void *pr)
{
    char *port, *userParam;
    int signal;
    asynUser *pasynUser=NULL;
    asynStatus status;
    asynInterface *pasynInterface;
    devAiHeidND261Pvt *pPvt=NULL;
    struct aiRecord *ai = (struct aiRecord *) pr;

    pPvt = calloc(1, sizeof(devAiHeidND261Pvt));
    ai->dpvt = pPvt;

    /* initialize configurable parameters */
    strcpy(pPvt->format, "%lf");  /* format string for sscanf */
    strcpy(pPvt->term, "\n\n"); /* terminator appended by ND261 to return string */
    pPvt->termlen=2;
    pPvt->timeout=1; /* 1 sec */
    pPvt->nchar=18;
    pPvt->outbuf[0] = 2;    /* Character ^B to read out ND261 */
    pPvt->outbuf[1] = '\n'; /* Terminator for write to ND261 */

    /* Create an asynUser */
    pasynUser = pasynManager->createAsynUser(devAiHeidND261Callback, 0);
    pasynUser->userPvt = ai;

    /* Parse link */
    status = pasynEpicsUtils->parseLink(pasynUser, &ai->inp,
                                        &port, &signal, &userParam);
    if (status != asynSuccess)
    {
        errlogPrintf("devAiHeidND261::init %s bad link %s\n",
                     ai->name, pasynUser->errorMessage);
        goto bad;
    }

    status = pasynManager->connectDevice(pasynUser,port,0);
    if (status!=asynSuccess)
        goto bad;
    pasynInterface = pasynManager->findInterface(pasynUser,asynOctetType,1);
    if (!pasynInterface)
        goto bad;
    pPvt->pasynOctet = (asynOctet *)pasynInterface->pinterface;
    pPvt->octetPvt = pasynInterface->drvPvt;
    pPvt->pasynUser = pasynUser;

    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE, 
              "devAiHeidND261 %s term = '%s'\n", ai->name, pPvt->term);
    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE, 
              "   timeout = %d\n", pPvt->timeout);
    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE, 
              "   format = %s\n", pPvt->format);
    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE, 
              "   nchar = %s\n", pPvt->nchar);
    return(0);

bad:
    if (status!=asynSuccess)
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR, "%s asynManager error %s\n",
                  ai->name,pasynUser->errorMessage);
    }
    if (pasynUser)
        pasynManager->freeAsynUser(pasynUser);
    if (pPvt)
        free(pPvt);
    ai->pact = 1;
    return(0);
}


static void devAiHeidND261Callback(asynUser *pasynUser)
{
	dbCommon *pr = (dbCommon *)pasynUser->userPvt;
	aiRecord* pai = (aiRecord*) pr;
	devAiHeidND261Pvt *pPvt = (devAiHeidND261Pvt *)pr->dpvt;
	struct rset *prset = (struct rset *)(pr->rset);
    int eomReason;

	pPvt->pasynUser->timeout = pPvt->timeout;
	pPvt->status = pPvt->pasynOctet->write(pPvt->octetPvt, pasynUser, 
		pPvt->outbuf, 2, &pPvt->nwrite);
	pPvt->pasynOctet->setInputEos(pPvt->octetPvt, pasynUser, pPvt->term, pPvt->termlen);
	pPvt->status = pPvt->pasynOctet->read(pPvt->octetPvt, pasynUser, 
		pPvt->inbuf, pPvt->nchar, &pPvt->nread, &eomReason);


    pai->udf = 0;

	dbScanLock(pr);
	(*prset->process)(pr);
	dbScanUnlock(pr);
}


static long io(dbCommon* pr)
{
	devAiHeidND261Pvt *pPvt = (devAiHeidND261Pvt *)pr->dpvt;
	asynUser *pasynUser = pPvt->pasynUser;
	int status;

	DEBUG(2,"devAiHeidND261:io: entry%c\n", '.');


	if (!pr->pact) {
		/* record is calling us to start a write/read operation */
		pr->pact = 1;
		status = pasynManager->queueRequest(pasynUser, 0, 0);
		if (status != asynSuccess) status = -1;
		return(status);
	} else {
		/*
		 * asyn called devAiHeidND261Callback(), which did the write/read
		 * and processed the record.  Now the record is calling us for the
		 * result of the write/read operation.
		 */
		return(completeIO((dbCommon *)pr));
	}
}


static long completeIO(dbCommon* pr)
{
	aiRecord* pai = (aiRecord*) pr;
	devAiHeidND261Pvt *pPvt = (devAiHeidND261Pvt *)pr->dpvt;

	unsigned char *p;
	double x;
 	int i, sz = pPvt->nread, dlen = pPvt->termlen;

	DEBUG(2,"devAiND261:CompleteIO(%s)\n", pr->name);
	DEBUG(2," - data size=%d\n", sz);
	DEBUG(2," - return code=%d\n", pPvt->status);
	DEBUG(3," - buffer=>%s<\n", pPvt->inbuf);
	DEBUG(2," - %c", '<'); 
	for (i=0, p=(unsigned char*)(pPvt->inbuf); i<BUFSZ; p++, i++) 
		{ DEBUG(2,"%2.2x ", (unsigned int) *p); }
	DEBUG(2,"%c\n", '>'); 
 
	if ((pPvt->status != asynSuccess) || (sz < 14)) {
		pai->val = 99999.0;
		pai->udf = 1;
		return(-1); 
	}
 
 	pPvt->inbuf[sz]='\0';
	/* remove delimiter */
	if (((unsigned int)sz < sizeof(pPvt->inbuf)) && (sz >= dlen))
		pPvt->inbuf[sz-dlen] = '\0';

	sscanf(&(pPvt->inbuf[1]), pPvt->format, &x);
	pai->val = x * ((pPvt->inbuf[0] == '-') ? -1 : 1);
	pai->udf = 0;
	DEBUG(2," - pai->val=%f\n", pai->val);
 	return(2);
}

