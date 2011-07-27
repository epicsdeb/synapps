/* devXxEurotherm.c */
/*
 *      Author: Tim Mooney
 *      Date:   August 31, 2004
***********************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <dbScan.h>
#include <dbDefs.h>
#include <dbAccess.h>
#include <dbCommon.h>
#include <alarm.h>
#include <link.h>
#include <recGbl.h>
#include <recSup.h>
#include <errlog.h>
#include <devSup.h>
#include <asynDriver.h>
#include <asynEpicsUtils.h>
#include <asynOctet.h>
#include <aoRecord.h>
#include <stringoutRecord.h>
#include <epicsExport.h>

#ifdef NODEBUG
#define DEBUG(l,f,v) ;
#else
#define DEBUG(l,f,v) { if(l<=devEurothermDebug) printf(f,v); }
#endif
volatile int devEurothermDebug = 0;
 
#define HEXCHAR2INT(c) ((c)>'9' ? 10+tolower(c)-'a' : tolower(c)-'0')

typedef enum {opTypeInput, opTypeOutput} opType;
typedef enum {recTypeAo, recTypeSo} recType;

typedef struct devEurothermPvt {
	asynUser	*pasynUser;
	asynOctet	*pasynOctet;
	void		*octetPvt;
	opType		opType;
	recType		recType;
	asynStatus	status;
	size_t		nread;
	size_t		nwrite;
	char		buffer[100];
	int		bufferStartIndex;
	char		term[10];
	char		format[32];
	int			termlen;
	double		timeout;
	int			nchar;
	int			group_address;
	int			local_address;
} devEurothermPvt;

typedef struct dsetEurotherm{
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	io;
	DEVSUPFUN	convert;
} dsetEurotherm;

static long initCommon(dbCommon *pr, DBLINK *plink);
static long startIOCommon(dbCommon *pr);
static long completeIOCommon(dbCommon *pr);
static void devEurothermCallback(asynUser *pasynUser);

static long initAo(aoRecord *pr);
static long writeAo(aoRecord *pr);
static long initSo(stringoutRecord *pr);
static long writeSo(stringoutRecord *pr);
dsetEurotherm devAoEurotherm = {6,0,0,initAo,0,writeAo, 0};
epicsExportAddress(dset,devAoEurotherm);
dsetEurotherm devSoEurotherm = {6,0,0,initSo,0,writeSo,0};
epicsExportAddress(dset,devSoEurotherm);


static long initCommon(dbCommon *pr, DBLINK *plink)
{
	char *p, *port, *userParam;
	int i;
	int signal;
	asynUser *pasynUser=NULL;
	asynStatus status;
	asynInterface *pasynInterface;
	devEurothermPvt *pPvt = pr->dpvt;

	/* Create an asynUser */
	pasynUser = pasynManager->createAsynUser(devEurothermCallback, 0);
	pasynUser->userPvt = pr;

	/* Parse link */
	status = pasynEpicsUtils->parseLink(pasynUser, plink,
		&port, &signal, &userParam);
	if (status != asynSuccess) {
		errlogPrintf("devXxEurotherm::initCommon %s bad link %s\n",
			pr->name, pasynUser->errorMessage);
		goto bad;
	}

	status = pasynManager->connectDevice(pasynUser,port,0);
	if (status!=asynSuccess) goto bad;
	pasynInterface = pasynManager->findInterface(pasynUser,asynOctetType,1);
	if (!pasynInterface) goto bad;
	pPvt->pasynOctet = (asynOctet *)pasynInterface->pinterface;
	pPvt->octetPvt = pasynInterface->drvPvt;
	pPvt->pasynUser = pasynUser;

	/* Initialize parameters */
	pPvt->bufferStartIndex = 0;
	pPvt->timeout=3.0;
	pPvt->nchar=100;
	pPvt->group_address = 0;;
	/*
	 * Default local address for Eurotherm 800 series is 0.  For the Eurotherm 2000
	 * series, local address 0 broadcasts to all devices, and the default local
	 * address is 1.
	 */
	pPvt->local_address = 0;;

	/* Get any configurable parameters from parm field */
	if (userParam) {
		asynPrint(pasynUser, ASYN_TRACEIO_DEVICE, 
			"XxEurotherm::initCommon, userParam = '%s'\n", userParam);
		if ((p = strstr(userParam, "TERM="))) {
			pPvt->termlen = 0;
			for (p+=5,i=0; i<9 && isxdigit(p[0]) && isxdigit(p[1]); i++,p+=2) {
				pPvt->term[i] = HEXCHAR2INT(p[0])*16 + HEXCHAR2INT(p[1]);
				asynPrint(pasynUser, ASYN_TRACEIO_DEVICE, 
					"devXxEurotherm:initCommon term[i] = 0x%x\n", pPvt->term[i]);
				pPvt->termlen++;
			}
			pPvt->term[i] = 0;
		}

		if ((p = strstr(userParam, "IX="))) {
			pPvt->bufferStartIndex = atoi(&p[3]);
		}

		if ((p = strstr(userParam, "FMT="))) {
			for (i=0, p+=4; i<31 && *p && *p != ','; i++, p++) pPvt->format[i] = *p;
			pPvt->format[i] = 0;
		}

		if ((p = strstr(userParam, "TO="))) {
			pPvt->timeout = atof(&p[3]);
		}

		if ((p = strstr(userParam, "GAD="))) {
			pPvt->group_address = atoi(&p[4]);
		}

		if ((p = strstr(userParam, "LAD="))) {
			pPvt->local_address = atoi(&p[4]);
		}
	}

	asynPrint(pasynUser, ASYN_TRACEIO_DEVICE, 
		"devXxEurotherm %s term = '%s'\n", pr->name, pPvt->term);
	asynPrint(pasynUser, ASYN_TRACEIO_DEVICE, 
		"   bufferStartIndex = %d\n", pPvt->bufferStartIndex);
	asynPrint(pasynUser, ASYN_TRACEIO_DEVICE, 
		"   timeout = %d\n", pPvt->timeout);
	asynPrint(pasynUser, ASYN_TRACEIO_DEVICE, 
		"   format = %s\n", pPvt->format);
	asynPrint(pasynUser, ASYN_TRACEIO_DEVICE, 
		"   group address = %d\n", pPvt->group_address);
	asynPrint(pasynUser, ASYN_TRACEIO_DEVICE, 
		"   local address = %d\n", pPvt->local_address);
	return 0;

bad:
	if (status!=asynSuccess) {
		asynPrint(pasynUser,ASYN_TRACE_ERROR,
			"%s asynManager error %s\n",
			pr->name,pasynUser->errorMessage);
	}
	if (pasynUser) pasynManager->freeAsynUser(pasynUser);
	if (pPvt) free(pPvt);
	pr->pact = 1;
	return 0;
}

static long initAo(aoRecord *pr)
{
	devEurothermPvt *pPvt=NULL;

	pPvt = calloc(1, sizeof(devEurothermPvt));
	pr->dpvt = pPvt;
	pPvt->opType = opTypeOutput;
	pPvt->recType = recTypeAo;
	pPvt->term[0] = 3; /* ETX */
	pPvt->term[1] = 0;
	pPvt->termlen = 1;
	strcpy(pPvt->format, "%f");

	return(initCommon((dbCommon *)pr, &pr->out));
}

static long initSo(stringoutRecord *pr)
{
	devEurothermPvt *pPvt=NULL;

	pPvt = calloc(1, sizeof(devEurothermPvt));
	pr->dpvt = pPvt;
	pPvt->opType = opTypeOutput;
	pPvt->recType = recTypeSo;
	pPvt->term[0] = 5; /* ENQ */
	pPvt->term[1] = 0;
	pPvt->termlen = 1;
	strcpy(pPvt->format, "%s");

	return(initCommon((dbCommon *)pr, &pr->out));
}

static long writeAo(aoRecord *pr)
{
	devEurothermPvt *pPvt = (devEurothermPvt *)pr->dpvt;
	char checksum;
	int i;
    
	if (!pr->pact) {

		/* message preamble */
		pPvt->buffer[0] = 4; /* EOT */
		pPvt->buffer[1] = '0' + pPvt->group_address; /* group address */
		pPvt->buffer[2] = '0' + pPvt->group_address; /* group address repeated */
		pPvt->buffer[3] = '0' + pPvt->local_address; /* local address */
		pPvt->buffer[4] = '0' + pPvt->local_address; /* local address repeated */
		pPvt->buffer[5] = 2; /* STX */
		
		/* message payload */
		sprintf(&pPvt->buffer[6], pPvt->format, pr->val);

		/* message terminator */
		strncat(pPvt->buffer, pPvt->term, pPvt->termlen);

		/* add checksum */
		for (i=7, checksum=pPvt->buffer[6]; pPvt->buffer[i] && i < 98; i++) {
			checksum ^= pPvt->buffer[i];
		}
		pPvt->buffer[i] = checksum;
		pPvt->buffer[i+1] = 0;

		pPvt->nchar = i+1;
		return(startIOCommon((dbCommon *)pr));

	} else {

		return(completeIOCommon((dbCommon *)pr));

	}
}

static long writeSo(stringoutRecord *pr)
{
	devEurothermPvt *pPvt = (devEurothermPvt *)pr->dpvt;
    
	if (!pr->pact) {

		/* message preamble */
		pPvt->buffer[0] = 4; /* EOT */
		pPvt->buffer[1] = '0' + pPvt->group_address; /* group address */
		pPvt->buffer[2] = '0' + pPvt->group_address; /* group address repeated */
		pPvt->buffer[3] = '0' + pPvt->local_address; /* local address */
		pPvt->buffer[4] = '0' + pPvt->local_address; /* local address repeated */
		pPvt->buffer[5] = 0;

		/* message payload */
		strncat(pPvt->buffer, &(pr->val[pPvt->bufferStartIndex]),
			sizeof(pPvt->buffer)-strlen(pPvt->buffer));

		/* message terminator */
		strncat(pPvt->buffer, pPvt->term, pPvt->termlen);

		pPvt->nchar = strlen(pPvt->buffer);
		return(startIOCommon((dbCommon *)pr));

	} else {

		return(completeIOCommon((dbCommon *)pr));

	}
}


static long startIOCommon(dbCommon* pr)
{
	devEurothermPvt *pPvt = (devEurothermPvt *)pr->dpvt;
	asynUser *pasynUser = pPvt->pasynUser;
	int status;

	pr->pact = 1;
	status = pasynManager->queueRequest(pasynUser, 0, 0);
	if (status != asynSuccess) status = -1;
	return(status);
}

static void devEurothermCallback(asynUser *pasynUser)
{
	dbCommon *pr = (dbCommon *)pasynUser->userPvt;
	devEurothermPvt *pPvt = (devEurothermPvt *)pr->dpvt;
	struct rset *prset = (struct rset *)(pr->rset);

	pPvt->pasynUser->timeout = pPvt->timeout;

	pPvt->status = pPvt->pasynOctet->write(pPvt->octetPvt, pasynUser, 
		pPvt->buffer, pPvt->nchar, &pPvt->nwrite);

	/* Process the record. This will result in the readXi or writeXi routine
	 * being called again, but with pact=1
	 */
	dbScanLock(pr);
	(*prset->process)(pr);
	dbScanUnlock(pr);
}


static long completeIOCommon(dbCommon *pr)
{
	devEurothermPvt *pPvt = (devEurothermPvt *)pr->dpvt;
	return(pPvt->status ? 0 : -1);
}
