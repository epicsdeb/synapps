/* derived from devAsynOctet.c */

/*

	This file provides device support for the stringout record, and uses the
	record's VAL field to determine a number to be written to a register
	implemented in the firmware of an FPGA, which we write to using some other
	asyn driver (drvIP_EP201).

    asynSoftGlue
        OUT contains <drvParams> which is passed to asynDrvUser.create (however,
			 drvIP_EP201 doesn't support this)
        VAL is used to derive the value to be sent.
*/

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <alarm.h>
#include <recGbl.h>
#include <dbAccess.h>
#include <dbDefs.h>
#include <link.h>
#include <epicsPrint.h>
#include <epicsMutex.h>
#include <epicsString.h>
#include <cantProceed.h>
#include <dbCommon.h>
#include <dbScan.h>
#include <callback.h>
#include <stringoutRecord.h>
#include <menuFtype.h>
#include <recSup.h>
#include <devSup.h>
#include <dbEvent.h>
#include <dbStaticLib.h> /* info item */

#include <epicsExport.h>
#include "asynDriver.h"
#include "asynDrvUser.h"
#include "asynUInt32Digital.h"
/* Ideally, we would use ...SyncIO, but drvIP_EP201 doesn't implement this interface. */
/* #include "asynUInt32DigitalSyncIO.h" */
#include "asynEpicsUtils.h"
#include <epicsExport.h>

volatile int devAsynSoftGlueDebug = 0;

typedef struct devPvt {
	dbCommon	*precord;
	asynUser	*pasynUser;
	char		*portName;
	int			addr;
	epicsUInt32	mask;
	asynUInt32Digital *pUInt32Digital;
	void		*UInt32DigitalPvt;
	int			canBlock;
	char		*drvParams;
	CALLBACK	callback;
	int			portInfoNum;
	int			signalNum;
	int 		isOutput;
} devPvt;

static long report(int level);
static long initCommon(dbCommon *precord, DBLINK *plink, userCallback callback);
static void initDrvUser(devPvt *pdevPvt);
static asynStatus writeIt(asynUser *pasynUser, epicsUInt32 value, epicsUInt32 mask);
/*static asynStatus readIt(asynUser *pasynUser, epicsUInt32 *value);*/
static long processCommon(dbCommon *precord);
static void finish(dbCommon *precord);

static long initSoWrite(stringoutRecord *pso);
static void callbackSoWrite(asynUser *pasynUser);
static long init(int after);
static long checkSignal(stringoutRecord *pso);


typedef struct commonDset {
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	process;
} commonDset;

commonDset asynSoftGlue = {5,report,init,initSoWrite, 0, processCommon};
epicsExportAddress(dset, asynSoftGlue);


/* signal name/number stuff */
#define MAXSIGNALS 16
#define MAXPORTS 5
#define ACCEPT_BUS_LINE_NUMBER 0

/* If signal name ends with '*', use inverted value of signal.  Only valid for inputs.
 * If implemented, output names will have trailing '*' stripped.
 */
#define IMPLEMENT_SIGNAL_NAME_STAR 1

/* a softGlue stringout record can identify itself as an input or an output either
 * by having the first character of its DESC field be 'O', or by defining the info
 * item 'softGlueIO' as a character string whose first character is 'O'.  DESC is
 * supported because this permits softGlue to be back ported to versions of EPICS
 * that don't support info items.  If USE_INFO_ITEM==1, we'll look for the info item,
 * but we'll check DESC if the item is not found. 
 */
#define USE_INFO_ITEM 1

struct sigEntry {
	char name[40];
	int numUsers;
};

static struct portInfo {
	char			portName[40];
	epicsMutexId	sig_mutex;
	struct sigEntry	sigList[MAXSIGNALS];
	ELLLIST			recordList;
} portInfo[MAXPORTS];

struct recordListItem {
    ELLNODE node;
	stringoutRecord *precord;
};

static long init(int after) {
	int i, j;
	if (after) return(0);
	for (i=0; i<MAXPORTS; i++) {
		portInfo[i].portName[0] = '\0';
		portInfo[i].sig_mutex = epicsMutexCreate();
		for (j=0; j<MAXSIGNALS; j++) {
			portInfo[i].sigList[j].name[0] = '\0';
			portInfo[i].sigList[j].numUsers = 0;
		}
		ellInit(&(portInfo[i].recordList));
	}
	return(0);
}

static int isBusLine(char *s) {
#if ACCEPT_BUS_LINE_NUMBER
	if (s[0] != 'B') return(0);
	if ((s[1] != '0') && (s[1] != '1')) return(0);
	if ((s[2] != '\0') && (s[3] != '\0')) return(0);
	if (atoi(s+1) >= MAXSIGNALS) return(0);
	return(1);
#else
	return(0);
#endif
}

static void initSignal(stringoutRecord *pso) {
	devPvt *pdevPvt = (devPvt *)pso->dpvt;
	const char *info_value=NULL;
#if USE_INFO_ITEM
	DBENTRY dbentry;
	DBENTRY *pdbentry = &dbentry;
#endif

#if USE_INFO_ITEM
	dbInitEntry(pdbbase,pdbentry);
	if (dbFindRecord(pdbentry, pso->name) == 0) {
		info_value = dbGetInfo(pdbentry, "softGlueIO");
		if (devAsynSoftGlueDebug)
			printf("checkSignal: info_value='%s'\n", info_value);
	}
	dbFinishEntry(pdbentry);
#endif

	if (info_value) {
		pdevPvt->isOutput = (*info_value == 'O');
	} else {
		pdevPvt->isOutput = (pso->desc[0] == 'O');
	}
}


/* Check the signal name (the val field) and the signal number (pdevPvt->signalNum).  If there is
 * a disagreement, reconcile it.  If it can't be reconciled, return -1.
 */
static long checkSignal(stringoutRecord *pso) {
	devPvt *pdevPvt = (devPvt *)pso->dpvt;
	int i, needPost=0, compareLength;
	char *c;
	char signalName[60];
	struct portInfo *pi = &(portInfo[pdevPvt->portInfoNum]);
	int portNum=-1;
	struct recordListItem *pitem;
	struct sigEntry *sig;

	if (devAsynSoftGlueDebug) {
		printf("checkSignal:entry: val='%s', old signal=%d (%s)\n", pso->val,
			pdevPvt->signalNum, pdevPvt->isOutput?"output":"input");
	}

	/* Delete any leading whitespace */
	while (isspace((int)pso->val[0])) {
		for (c=&(pso->val[1]); *c; c++) *(c-1) = *c;
		*(c-1) = '\0';
		needPost = 1;
	}

#if 1
	/* Most traffic will be numeric values written to unassigned inputs.
	 * Handle with this shortcut.  Take care not to interfere with drag-and-drop to an input
	 * with a numeric value
	 */
	if ((pdevPvt->isOutput==0) && (isdigit((int)pso->val[0])) && (pdevPvt->signalNum==0) &&
			(strlen(pso->val) < 10)) {
		if (needPost) db_post_events(pso, &pso->val, DBE_VALUE);
		if (devAsynSoftGlueDebug) printf("checkSignal: shortcut.\n");
		return(0);
	}
#endif

	/* See if this signal's new name is the PVname of some other signal attached to the same port.
	 * If so, user is probably trying to use Drag-N-Drop to connect signals, and probably expects
	 * this to connect the signals.  If the source signal has a name, we can do what user wants;
	 * otherwise, we can at least make the signal names the same empty string.
	 */
	for (i=0; i<MAXPORTS; i++) {
		/* Same port?  If not, don't bother checking further */
		if (strcmp(pi->portName, portInfo[i].portName) == 0) portNum = i;
	}
	if (portNum >= 0) {
		pitem = (struct recordListItem *)ellFirst(&(portInfo[portNum].recordList));
		while (pitem) {
			if ((strlen(pso->val) > 1) && (strlen(pso->val) >= strlen(pitem->precord->name))) {
				if (strstr(pso->val, pitem->precord->name) != 0) {
					strcpy(pso->val, pitem->precord->val);
					needPost = 1;
					break;
				}
			}
			pitem = (struct recordListItem *)ellNext(&(pitem->node));
		}
	}

	compareLength = strlen(pso->val);

	/* If signal is an output, leading decimal digit is illegal */
	if (pdevPvt->isOutput) {
		/* signal is an output.  If deleted leading digit leaves a leading space, delete that too. */
		while (isdigit((int)pso->val[0]) || isspace((int)pso->val[0])) {
			for (c=&(pso->val[1]); *c; c++) *(c-1) = *c;
			*(c-1) = '\0';
			compareLength--;
			needPost = 1;
		}
#if IMPLEMENT_SIGNAL_NAME_STAR
		/* output-signal names are not permitted to end in '*' */
		if (pso->val[strlen(pso->val)-1] == '*') {
			compareLength--;
			pso->val[strlen(pso->val)-1] = '\0';
			needPost = 1;
		}
#endif
	} else {
#if IMPLEMENT_SIGNAL_NAME_STAR
		/* Check if signal name ends with '*'.  If so, it is to be regarded
		 * as the same signal as the signal name with the trailing '*' deleted.
		 */
		if (pso->val[strlen(pso->val)-1] == '*') {
			compareLength--;
		}
		if (compareLength == 0) {
			/* signal name was '*'.  Erase it. */
			pso->val[0] = '\0';
			needPost = 1;
		}
#endif
	}
	if (needPost) db_post_events(pso, &pso->val, DBE_VALUE);

	strcpy(signalName, pso->val);
	signalName[compareLength] = '\0';
	if (devAsynSoftGlueDebug) printf("checkSignal: signal='%s', compareLength=%d\n", signalName, compareLength);

	epicsMutexLock(pi->sig_mutex);
	sig = &(pi->sigList[pdevPvt->signalNum]);
	if (pdevPvt->signalNum) {
		/* We're attached to a nonzero signal.  Should we stay attached? */
		if (isdigit((int)signalName[0])) {
			/* We're a binary value; signal number should be 0.  Detach. */
			if (--(sig->numUsers) <= 0) {
				sig->numUsers = 0;
				sig->name[0] = '\0';
				/* We should tell user how many signals are still available for use. */
			}
			pdevPvt->signalNum = 0;
			if (devAsynSoftGlueDebug) printf("checkSignal: binary value\n");
			epicsMutexUnlock(pi->sig_mutex);
			return(0);
		} else if (strcmp(signalName, sig->name)==0) {
			/* The VAL field agrees with the signal we're attached to. */
			if (devAsynSoftGlueDebug) printf("checkSignal: name agrees with signalNum\n");
			epicsMutexUnlock(pi->sig_mutex);
			return(0);
		} else {
			/* The VAL field disagrees with the signal's name.  Detach. */
			if (pdevPvt->signalNum && strcmp(signalName, sig->name)) {
				if (--(sig->numUsers) <= 0) {
					sig->numUsers = 0;
					sig->name[0] = '\0';
					/* We should tell user how many signals are still available for use. */
				}
				pdevPvt->signalNum = 0;
			}
		}
	}

	/* We're not attached to a nonzero signal. */

	if (isdigit((int)signalName[0])) {
		/* shortcut above should already have handled this case */
		epicsMutexUnlock(pi->sig_mutex);
		return(0);
	}

	if (isBusLine(signalName)) {
		/* 'B0' .. 'Bnn', where nn is MAXSIGNALS-1. */
		i = atoi(signalName+1);
		pdevPvt->signalNum = i;
		if (devAsynSoftGlueDebug) printf("checkSignal: Attaching to signal %d\n", pdevPvt->signalNum);
		epicsMutexUnlock(pi->sig_mutex);
		return(0);
	} else if (signalName[0]) {
		if (devAsynSoftGlueDebug) printf("checkSignal: Signal name = '%s'\n", signalName);
		/* We have a signal name.  See if it's already assigned. */
		for (i=1; i<MAXSIGNALS; i++) {
			if (strcmp(signalName, pi->sigList[i].name) == 0) {
				pdevPvt->signalNum = i;
				pi->sigList[i].numUsers += 1;
				if (devAsynSoftGlueDebug) printf("checkSignal: Attaching to existing signal %d\n", pdevPvt->signalNum);
				epicsMutexUnlock(pi->sig_mutex);
				return(0);
			}
		}
		/* Assign busline to name */
		for (i=1; i<MAXSIGNALS; i++) {
			if (pi->sigList[i].numUsers == 0) {
				pdevPvt->signalNum = i;
				pi->sigList[i].numUsers = 1;
				strcpy(pi->sigList[i].name, signalName);
				if (devAsynSoftGlueDebug) printf("checkSignal: Assigning name '%s' to signal %d\n", signalName, pdevPvt->signalNum);
				/* We should tell user how many signals are still available for use. */

				epicsMutexUnlock(pi->sig_mutex);
				return(0);
			}
		}
		/* No available signals */
		printf("checkSignal: No available signals\n");
		epicsMutexUnlock(pi->sig_mutex);
		pso->val[0] = '\0';
		db_post_events(pso, &pso->val, DBE_VALUE);
		return(-1);
	}
	/* empty string */
	pdevPvt->signalNum = 0;
	epicsMutexUnlock(pi->sig_mutex);
	return(0);
}

static long report(int level) {
	int i, j;

	if (devAsynSoftGlueDebug) printf("devAsynSoftGlue:report:entry\n");
	for (i=0; i<MAXPORTS; i++) {
		printf("portName '%s'\n", portInfo[i].portName);
		if ((level > 1) && (strlen(portInfo[i].portName)>0)) {
			for (j=0; j<MAXSIGNALS; j++) {
				printf("signal %d name '%s'; %d users\n", j,
					portInfo[i].sigList[j].name, portInfo[i].sigList[j].numUsers);
			}
		}
	}
	return(0);
}

static long initCommon(dbCommon *precord, DBLINK *plink, userCallback callback)
{
	devPvt			*pdevPvt;
	asynStatus		status;
	asynUser		*pasynUser;
	asynInterface	*pasynInterface;
	int				i;

	/*
	 * Allocate a devPvt structure for record specific information.  We'll attach
	 * this to the record, so we'll have it when record support calls us, and to
	 * the asynUser structure, so we'll have it when asyn calls us.
	 */
	pdevPvt = callocMustSucceed(1,sizeof(*pdevPvt),"devAsynSoftGlue:initCommon");
	precord->dpvt = pdevPvt;

	/* Create an asynUser, and keep a pointer to it in devPvt. */
	pasynUser = pasynManager->createAsynUser(callback, 0);
	pdevPvt->pasynUser = pasynUser;

	/*
	 * When asyn calls us back to do I/O, the call will go to callbackSoWrite().  The
	 * only context information we're going to get will be the asynUser we just created,
	 * so we'd better attach our devPvt structure to the asynUser structure, and we'll
	 * have to be able to get the record pointer from this, so attach it to devPvt.
	 */
	pasynUser->userPvt = pdevPvt;
	pdevPvt->precord = precord;

	/*
	 * Parse the record's output link to find out who we're supposed to
	 * talk to (and maybe what command we're supposed to send).
	 * This device support supports an INST_IO output link (this is specified
	 * in the .dbd file).  There are two preferred choices for an INST_IO link:
	 *
	 *       field(OUT,"@asyn(portName,addr,timeout) drvParams")
	 * e.g.: field(OUT,"@asyn(myPort,0,0.1) SETGAIN")
	 *
	 * or
	 *
	 *       field(OUT,"@asynMask(portName,addr,mask,timeout) drvParams")
	 * e.g.: field(OUT,"@asynMask(myPort,0,0xff,0.1) SET_ADDR_BITS")
	 *
	 * If we can use one of these syntaxes, we don't have to write parsing code,
	 * because asyn provides code for these two syntaxes in asynEpicsUtils.c,
	 * which we use via asynEpicsUtils.h.  Note that timeout is recorded in the
	 * asynUser for us, and is not returned to us.  drvParams is whatever
	 * follows the closing ')', which may be nothing.
	 */

	/*
	 * example of syntax with no mask value:
	 * status = pasynEpicsUtils->parseLink(pasynUser, plink,
	 *     &pdevPvt->portName, &pdevPvt->addr,&pdevPvt->drvParams);
	 */

	status = pasynEpicsUtils->parseLinkMask(pasynUser, plink, 
		&pdevPvt->portName, &pdevPvt->addr, &pdevPvt->mask,&pdevPvt->drvParams);

	if (status != asynSuccess) {
		printf("%s devAsynSoftGlue:initCommon: error in link %s\n",
			precord->name, pasynUser->errorMessage);
		goto bad;
	}

	/* Connect to the device specified in the link we just parsed */
	status = pasynManager->connectDevice(pasynUser,
		pdevPvt->portName, pdevPvt->addr);
	if (status != asynSuccess) {
		printf("%s devAsynSoftGlue:initCommon: connectDevice failed %s\n",
			precord->name, pasynUser->errorMessage);
		goto bad;
	}

	/*
	 * We're not going to talk directly to the device.  Instead, we're going
	 * to use an existing driver, and talk through the interface that the
	 * driver implements.  The driver will have registered all the interfaces
	 * it implements, so we tell asyn to go through its list of the registered
	 * interfaces for the device we want to talk to, and find one of the type
	 * we'd like to use.  In this case, we want to talk through an interface
	 * of type asynUInt32DigitalType.
	 */
	pasynInterface = pasynManager->findInterface(pasynUser,asynUInt32DigitalType,1);
	if (!pasynInterface) {
		printf("%s devAsynSoftGlue:initCommon: interface %s not found\n",
			precord->name,asynUInt32DigitalType);
		goto bad;
	}

	/*
	 * Attach copies of the interface pointer, and the driver-private pointer,
	 * to our per-record structure.
	 */
	pdevPvt->pUInt32Digital = pasynInterface->pinterface;
	pdevPvt->UInt32DigitalPvt = pasynInterface->drvPvt;
	/* Determine if device can block */
	pasynManager->canBlock(pasynUser, &pdevPvt->canBlock);


	/* Assign port name to an element of our private array of portInfo structures. */
	for (i=0, pdevPvt->portInfoNum=-1; (i<MAXPORTS) && (pdevPvt->portInfoNum==-1); i++) {
		epicsMutexLock(portInfo[i].sig_mutex);
		if (portInfo[i].portName[0]) {
			/* This index is in use.  See if it's for our port. */
			if (strcmp(pdevPvt->portName, portInfo[i].portName) == 0) {
				pdevPvt->portInfoNum = i;
			}
		} else {
			/* Our port name is not represented in the array. */
			strcpy(portInfo[i].portName, pdevPvt->portName);
			pdevPvt->portInfoNum = i;
		}
		epicsMutexUnlock(portInfo[i].sig_mutex);
	}
	if (pdevPvt->portInfoNum == -1) {
		printf("%s devAsynSoftGlue:initCommon: Can't assign port name to portInfo index\n",
			precord->name);
		goto bad;
	}

	/* initialize the signal */
	initSignal((stringoutRecord *)precord);

	/* Add the record to the port's recordList */
    {
		struct recordListItem *pitem = (struct recordListItem *) malloc(sizeof(struct recordListItem));
		pitem->precord = (stringoutRecord *)(pdevPvt->precord);
		ellAdd(&(portInfo[pdevPvt->portInfoNum].recordList), &pitem->node);
	}
	return(0);

bad:
	/*
	 * If there was a problem during the initialization, disable the record
	 * by making it appear to be processing continuously.
	 */
	precord->pact=1;
	return(-1);
}




static void initDrvUser(devPvt *pdevPvt)
{
	asynUser		*pasynUser = pdevPvt->pasynUser;
	asynStatus		status;
	asynInterface	*pasynInterface;
	dbCommon		*precord = pdevPvt->precord;

	/*call drvUserCreate*/
	pasynInterface = pasynManager->findInterface(pasynUser,asynDrvUserType,1);
	if (pasynInterface && pdevPvt->drvParams) {
		asynDrvUser *pasynDrvUser;
		void *drvPvt;

		pasynDrvUser = (asynDrvUser *)pasynInterface->pinterface;
		drvPvt = pasynInterface->drvPvt;
		status = pasynDrvUser->create(drvPvt,pasynUser,pdevPvt->drvParams,0,0);
		if (status!=asynSuccess) {
			printf("%s devAsynSoftGlue drvUserCreate failed %s\n",
				precord->name, pasynUser->errorMessage);
		}
	}
}


static asynStatus writeIt(asynUser *pasynUser, epicsUInt32 value, epicsUInt32 mask)
{
	devPvt		*pdevPvt = (devPvt *)pasynUser->userPvt;
	dbCommon	*precord = pdevPvt->precord;
	asynStatus	status;

	status = pdevPvt->pUInt32Digital->write(pdevPvt->UInt32DigitalPvt, pasynUser, value, mask);
	if (status!=asynSuccess) {
		asynPrint(pasynUser,ASYN_TRACE_ERROR,
			"%s devAsynSoftGlue: writeIt failed %s\n",
			precord->name,pasynUser->errorMessage);
		recGblSetSevr(precord, WRITE_ALARM, INVALID_ALARM);
		return status;
	}
	asynPrint(pasynUser,ASYN_TRACEIO_DEVICE,
		"%s devAsynSoftGlue:writeIt: value=%lu\n", precord->name, value);
	return status;
}


/* Most of the code in this module intends to support a variety of record types.
 * Here, we specialize to stringout, even though we're called processCommon().
 */
static long processCommon(dbCommon *precord)
{
	devPvt		*pdevPvt = (devPvt *)precord->dpvt;
	stringoutRecord *pso = (stringoutRecord *)precord;
	asynStatus	status;

	if (checkSignal(pso)) {
		asynPrint(pdevPvt->pasynUser, ASYN_TRACE_ERROR,
			"%s devAsynSoftGlue:processCommon: signal name can't be assigned %s\n", 
			precord->name,pdevPvt->pasynUser->errorMessage);
		recGblSetSevr(precord,WRITE_ALARM,INVALID_ALARM);
		return(-1);
	}

	if (pdevPvt->canBlock) precord->pact = 1;
	status = pasynManager->queueRequest(
		pdevPvt->pasynUser, asynQueuePriorityMedium, 0.0);
	if ((status==asynSuccess) && pdevPvt->canBlock) return 0;
	if (pdevPvt->canBlock) precord->pact = 0;
	if (status != asynSuccess) {
		asynPrint(pdevPvt->pasynUser, ASYN_TRACE_ERROR,
			"%s devAsynSoftGlue:processCommon: error queuing request %s\n", 
			precord->name,pdevPvt->pasynUser->errorMessage);
		recGblSetSevr(precord,WRITE_ALARM,INVALID_ALARM);
	}
	return(0);
}

static void finish(dbCommon *pr)
{
	devPvt	*pPvt = (devPvt *)pr->dpvt;

	if (pr->pact) callbackRequestProcessCallback(&pPvt->callback,pr->prio,pr);
}



static long initSoWrite(stringoutRecord *pso)
{
	asynStatus	status;
	devPvt		*pdevPvt;
	char		*s;
	
	/* First, see if this is a buslist display record.  If so, just cache its pointer */
	if (devAsynSoftGlueDebug) {
		printf("devAsynSoftGlue:initSoWrite: OUT='%s'\n", pso->out.value.instio.string);
	}
	s = strstr(pso->out.value.instio.string, "softGlueBusList");
	if (s != NULL) {
		int i, j=0;
		for (i=0; i<MAXPORTS; i++) {
			if (strlen(portInfo[i].portName) == 0)
				continue;
			if (devAsynSoftGlueDebug) printf("devAsynSoftGlue:initSoWrite: i=%d, portName='%s'\n",
				i, portInfo[i].portName);
			if (strstr(s, portInfo[i].portName) != NULL) {
				/* We should tell user how many signals are still available for use. */
				if (portInfo[i].sigList[j].numUsers >= 1) {
				}
				return 0;
			}
		}
		return 0;
	}

	status = initCommon((dbCommon *)pso,&pso->out,callbackSoWrite);
	if (status!=asynSuccess) return 0;
	pdevPvt = (devPvt *)pso->dpvt;
	initDrvUser((devPvt *)pso->dpvt);
	/* write at init time */
	status = processCommon((dbCommon *)pso);
	if (status) printf("devAsynSoftGlue: init-time write failed (%s).\n", pso->name);
	return 0;
}

#define NINT(f)  (int)((f)>0 ? (f)+0.5 : (f)-0.5)

static void callbackSoWrite(asynUser *pasynUser)
{
	devPvt			*pdevPvt = (devPvt *)pasynUser->userPvt;
	stringoutRecord	*pso = (stringoutRecord *)pdevPvt->precord;
	asynStatus		status;
	epicsUInt32		value=0;
	double			nvalue;
#if IMPLEMENT_SIGNAL_NAME_STAR
	epicsUInt32		mask=0x6f;
#else
	epicsUInt32		mask=0x2f;
#endif

	/* See if string value begins with a number or something else */
	if (isdigit((int)pso->val[0])) {
		/*
		 * It's a number.  Cause FPGA register to connect user-write bit
		 * (which is wired to mux input 0) to the device, by setting the mux
		 * address to zero, and writing '0' or '1' to register bit 5.
		 */
		nvalue = atof(pso->val);
		value = NINT(nvalue);
		if (value == 0) {
			value = 0;
		} else {
			value = 0x20;
		}
	} else if (pso->val[0] == 0) {
		/*
		 * String is empty.  We want unconnected inputs to default to '1'. 
		 * If this is an output, the value 0x20 will have no effect in the circuit.
		 */
		value = 0x20;
	} else {
		/* It's not a number.  Use assigned signalNum. */
		value = pdevPvt->signalNum;

#if IMPLEMENT_SIGNAL_NAME_STAR
		/* If signal name end with '*', set bit in control register that tells
		 * the FPGA component to negate the signal.  We're just thinking about adding
		 * this feature.
		 */
		if (pso->val[strlen(pso->val)-1] == '*') {
			value |= 0x40;
		}
#endif

#if ACCEPT_BUS_LINE_NUMBER
		/* If 'B0*' or 'B1*', use number directly as signal number. */
		if ((pso->val[0] == 'B') && ((pso->val[0] == '0')||(pso->val[0] == '1'))) {
			/* e.g., the string "B13" produces the integer 13 */
			value = atoi(&pso->val[1]);
		}
#endif
	}
	if (devAsynSoftGlueDebug) printf("devAsynSoftGlue:callbackSoWrite: writing 0x%x 0x%x\n", value, mask);

	status = writeIt(pasynUser, value, mask);

	/* Implement "1!" for a positive going pulse; "0!" for a negative going pulse */
	if (isdigit((int)pso->val[0]) && (pso->val[1] == '!')) {
		status = writeIt(pasynUser, (epicsUInt32)(value?0:0x20), mask);
	}
	finish((dbCommon *)pso);
}
