/* acalcout.c - Record Support Routines for array calc with output records */
/*
 *   Author : Tim Mooney
 *   Based on acalcoutRecord
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 2006, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 * Modification Log:
 * -----------------
 * 03-21-06  tmm  v1.0: created from scalcout record
 * 05-24-06  tmm  v1.1: call DSET->init_record()
 * 08-28-06  tmm  v1.2: don't allocate any array until someone tries to
 *                use or look at it.
 * 01-24-08  tmm  v1.3: Fixed check of outlink (if link to link field,
 *                or if .WAIT, then outlink attribute must be CA).
 * 04-29-08  tmm  v1.4: Peter Mueller noticed that calc records were not checking
 *                VAL against limits until after execOutput -- too late to do IVOA.
 */

#define VERSION 1.4


#include	<stdlib.h>
#include	<stdarg.h>
#include	<stdio.h>
#include	<string.h>
#include	<math.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbEvent.h>
#include	<dbScan.h>
#include	<errMdef.h>
#include	<errlog.h>
#include	<recSup.h>
#include	<devSup.h>
#include	<recGbl.h>
#include	<special.h>
#include	<callback.h>
#include	<taskwd.h>
#include	"aCalcPostfix.h"

#define GEN_SIZE_OFFSET
#include	"aCalcoutRecord.h"
#undef  GEN_SIZE_OFFSET
#include	<menuIvoa.h>
#include	<epicsExport.h>

#include	<epicsVersion.h>
#ifndef EPICS_VERSION_INT
#define VERSION_INT(V,R,M,P) ( ((V)<<24) | ((R)<<16) | ((M)<<8) | (P))
#define EPICS_VERSION_INT VERSION_INT(EPICS_VERSION, EPICS_REVISION, EPICS_MODIFICATION, EPICS_PATCH_LEVEL)
#endif
#define LT_EPICSBASE(V,R,M,P) (EPICS_VERSION_INT < VERSION_INT((V),(R),(M),(P)))

#define MIND_UNUSED_ELEMENTS 0


/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
static long special();
#define get_value NULL
static long cvt_dbaddr();
static long get_array_info();
static long put_array_info();
static long get_units();
static long get_precision();
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double();
static long get_control_double();
static long get_alarm_double();

rset acalcoutRSET={
	RSETNUMBER,
	report,
	initialize,
	init_record,
	process,
	special,
	get_value,
	cvt_dbaddr,
	get_array_info,
	put_array_info,
	get_units,
	get_precision,
	get_enum_str,
	get_enum_strs,
	put_enum_str,
	get_graphic_double,
	get_control_double,
	get_alarm_double
};

typedef struct acalcoutDSET {
    long       number;
    DEVSUPFUN  dev_report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  write;
} acalcoutDSET;

epicsExportAddress(rset, acalcoutRSET);

/* To provide feedback to the user as to the connection status of the 
 * links (.INxV and .OUTV), the following algorithm has been implemented ...
 *
 * A new PV_LINK [either in init() or special()] is searched for using
 * dbNameToAddr. If local, it is so indicated. If not, a checkLinkCb
 * callback is scheduled to check the connectivity later using 
 * dbCaIsLinkConnected(). Anytime there are unconnected CA_LINKs, another
 * callback is scheduled. Once all connections are established, the CA_LINKs
 * are checked whenever the record processes. 
 *
 */

#define NO_CA_LINKS     0
#define CA_LINKS_ALL_OK 1
#define CA_LINKS_NOT_OK 2

typedef struct rpvtStruct {
	CALLBACK	doOutCb;
	CALLBACK	checkLinkCb;
	short		wd_id_1_LOCK;
	short		caLinkStat; /* NO_CA_LINKS,CA_LINKS_ALL_OK,CA_LINKS_NOT_OK */
	short		outlink_field_type;
} rpvtStruct;

static void checkAlarms();
static void monitor();
static int fetch_values();
static void execOutput();
static void checkLinks();
static void checkLinksCallback();
static long writeValue(acalcoutRecord *pcalc);
static void call_aCalcPerform(acalcoutRecord *pcalc);
static long doCalc(acalcoutRecord *pcalc);
static void acalcPerformTask(void *parm);
volatile int aCalcoutRecordDebug = 0;
epicsExportAddress(int, aCalcoutRecordDebug);

#define MAX_FIELDS 12
#define ARRAY_MAX_FIELDS 12


static long acalcGetNumElements( acalcoutRecord *pcalc )
{
	long numElements;
	if ( (pcalc->nuse > 0) && (pcalc->nuse < pcalc->nelm) )
		numElements = pcalc->nuse;
	else
		numElements = pcalc->nelm;
	return numElements;
}


static long init_record(acalcoutRecord *pcalc, int pass)
{
	DBLINK *plink;
	int i;
	double *pvalue;
	unsigned short *plinkValid;
	short error_number;
    acalcoutDSET *pacalcoutDSET;

	dbAddr       Addr;
	dbAddr       *pAddr = &Addr;
	rpvtStruct   *prpvt;

	if (pass==0) {
		pcalc->vers = VERSION;
		pcalc->rpvt = (void *)calloc(1, sizeof(struct rpvtStruct));
		if ((pcalc->nuse < 0) || (pcalc->nuse > pcalc->nelm)) {
			pcalc->nuse = pcalc->nelm;
			db_post_events(pcalc,&pcalc->nuse,DBE_VALUE|DBE_LOG);
		}
		return(0);
	}

	if (!(pacalcoutDSET = (acalcoutDSET *)pcalc->dset)) {
		recGblRecordError(S_dev_noDSET,(void *)pcalc,"acalcout:init_record");
		return(S_dev_noDSET);
	}
	/* must have write defined */
	if ((pacalcoutDSET->number < 5) || (pacalcoutDSET->write == NULL)) {
		recGblRecordError(S_dev_missingSup,(void *)pcalc,"acalcout:init_record");
		return(S_dev_missingSup);
	}

	prpvt = (rpvtStruct *)pcalc->rpvt;
	plink = &pcalc->inpa;
	pvalue = &pcalc->a;
	plinkValid = &pcalc->inav;
	for (i=0; i<(MAX_FIELDS+ARRAY_MAX_FIELDS+1); i++, plink++, pvalue++, plinkValid++) {
		if (plink->type == CONSTANT) {
			/* Don't InitConstantLink the array links or the output link. */
			if (i < MAX_FIELDS) { 
				recGblInitConstantLink(plink,DBF_DOUBLE,pvalue);
				db_post_events(pcalc,pvalue,DBE_VALUE);
			}
			*plinkValid = acalcoutINAV_CON;
			if (plink == &pcalc->out)
				prpvt->outlink_field_type = DBF_NOACCESS;
		} else if (!dbNameToAddr(plink->value.pv_link.pvname, pAddr)) {
			/* the PV we're linked to resides on this ioc */
			*plinkValid = acalcoutINAV_LOC;
			if (plink == &pcalc->out) {
				prpvt->outlink_field_type = pAddr->field_type;
				if ((pAddr->field_type >= DBF_INLINK) && (pAddr->field_type <= DBF_FWDLINK)) {
					if (!(plink->value.pv_link.pvlMask & pvlOptCA)) {
						printf("aCalcoutRecord(%s):init_record:non-CA link to link field\n",
							plink->value.pv_link.pvname);
					}
				}
				if (pcalc->wait && !(plink->value.pv_link.pvlMask & pvlOptCA)) {
					printf("aCalcoutRecord(%s):init_record: Can't wait with non-CA link attribute\n",
						plink->value.pv_link.pvname);
				}
			}
		} else {
			/* pv is not on this ioc. Callback later for connection stat */
			*plinkValid = acalcoutINAV_EXT_NC;
			prpvt->caLinkStat = CA_LINKS_NOT_OK;
			if (plink == &pcalc->out)
				prpvt->outlink_field_type = DBF_NOACCESS; /* don't know field type */
		}
		db_post_events(pcalc,plinkValid,DBE_VALUE);
	}

	pcalc->clcv = aCalcPostfix(pcalc->calc,pcalc->rpcl,&error_number);
	if (pcalc->clcv) {
		recGblRecordError(S_db_badField,(void *)pcalc,
			"acalcout: init_record: Illegal CALC field");
		if (aCalcoutRecordDebug >= 10)
			printf("acalcPostfix returns: %d\n", error_number);
	}
	db_post_events(pcalc,&pcalc->clcv,DBE_VALUE);

	pcalc->oclv = aCalcPostfix(pcalc->ocal, pcalc->orpc,&error_number);
	if (pcalc->oclv) {
		recGblRecordError(S_db_badField,(void *)pcalc,
			"acalcout: init_record: Illegal OCAL field");
		if (aCalcoutRecordDebug >= 10)
			printf("acalcPostfix returns: %d\n", error_number);
	}
	db_post_events(pcalc,&pcalc->oclv,DBE_VALUE);

	callbackSetCallback(checkLinksCallback, &prpvt->checkLinkCb);
	callbackSetPriority(0, &prpvt->checkLinkCb);
	callbackSetUser(pcalc, &prpvt->checkLinkCb);
	prpvt->wd_id_1_LOCK = 0;

	if (prpvt->caLinkStat == CA_LINKS_NOT_OK) {
		callbackRequestDelayed(&prpvt->checkLinkCb,1.0);
		prpvt->wd_id_1_LOCK = 1;
	}

	if (pacalcoutDSET->init_record ) {
		return (*pacalcoutDSET->init_record)(pcalc);
	}
	return(0);
}

#define ASYNC 1
#define SYNC 0
static long afterCalc(acalcoutRecord *pcalc) {
	rpvtStruct   *prpvt = (rpvtStruct *)pcalc->rpvt;
	short		doOutput = 0;
	long		i, j;
	double		**panew;

	i = acalcGetNumElements( pcalc );
#if MIND_UNUSED_ELEMENTS
	if (i < pcalc->nelm) {
		for (; i<pcalc->nelm; i++) pcalc->aval[i] = 0;
	}
#endif
	/* post array fields that aCalcPerform wrote to. */
	for (j=0, panew=&pcalc->aa; j<ARRAY_MAX_FIELDS; j++, panew++) {
		if (*panew && (pcalc->amask & (1<<j))) {
			db_post_events(pcalc, *panew, DBE_VALUE|DBE_LOG);
		}
	}

	if (aCalcoutRecordDebug >= 5) {
		printf("acalcoutRecord(%s):aCalcPerform returns val=%f, aval=[%f %f...]\n",
			pcalc->name, pcalc->val, pcalc->aval[0], pcalc->aval[1]);
	}
	if (pcalc->cstat)
		recGblSetSevr(pcalc,CALC_ALARM,INVALID_ALARM);
	else
		pcalc->udf = FALSE;

	/* Check VAL against limits */
	checkAlarms(pcalc);

	/* check for output link execution */
	switch (pcalc->oopt) {
	case acalcoutOOPT_Every_Time:
		doOutput = 1;
		break;
	case acalcoutOOPT_On_Change:
		if (fabs(pcalc->pval - pcalc->val) > pcalc->mdel) doOutput = 1;
		break;
	case acalcoutOOPT_Transition_To_Zero:
		if ((pcalc->pval != 0) && (pcalc->val == 0)) doOutput = 1;
		break;         
	case acalcoutOOPT_Transition_To_Non_zero:
		if ((pcalc->pval == 0) && (pcalc->val != 0)) doOutput = 1;
		break;
	case acalcoutOOPT_When_Zero:
		if (!pcalc->val) doOutput = 1;
		break;
	case acalcoutOOPT_When_Non_zero:
		if (pcalc->val) doOutput = 1;
		break;
	case acalcoutOOPT_Never:
		doOutput = 0;
		break;
	}
	pcalc->pval = pcalc->val;

	if (doOutput) {
		if (pcalc->odly > 0.0) {
			pcalc->dlya = 1;
			db_post_events(pcalc,&pcalc->dlya,DBE_VALUE);
				callbackRequestProcessCallbackDelayed(&prpvt->doOutCb,
					pcalc->prio, pcalc, (double)pcalc->odly);
			if (aCalcoutRecordDebug >= 5)
				printf("acalcoutRecord(%s):process: exit, wait for delay\n", pcalc->name);
			return(ASYNC);
		} else {
			if (aCalcoutRecordDebug >= 5)
				printf("acalcoutRecord(%s):calling execOutput\n", pcalc->name);
			pcalc->pact = FALSE;
			execOutput(pcalc);
			if (pcalc->pact) {
				if (aCalcoutRecordDebug >= 5)
					printf("acalcoutRecord(%s):process: exit, pact==1\n", pcalc->name);
				return(ASYNC);
			}
			pcalc->pact = TRUE;
		}
	}
	return(SYNC);
}

static long process(acalcoutRecord *pcalc)
{
	rpvtStruct   *prpvt = (rpvtStruct *)pcalc->rpvt;
	long		i;
	double		*pnew, *pprev;

	if (aCalcoutRecordDebug) printf("acalcoutRecord(%s):process: pact=%d, cact=%d, dlya=%d\n",
		pcalc->name, pcalc->pact, pcalc->cact, pcalc->dlya);

	/* Make sure.  Autosave is capable of setting NUSE to an illegal value. */
	if ((pcalc->nuse < 0) || (pcalc->nuse > pcalc->nelm)) {
		pcalc->nuse = pcalc->nelm;
		db_post_events(pcalc,&pcalc->nuse, DBE_VALUE|DBE_LOG);
	}

	/* If we're getting processed, we can no longer put off allocating memory */
	if (pcalc->aval == NULL) pcalc->aval = (double *)calloc(pcalc->nelm, sizeof(double));
	if (pcalc->oav == NULL) pcalc->oav = (double *)calloc(pcalc->nelm, sizeof(double));

	if (!pcalc->pact) {
		pcalc->pact = TRUE;

		/* record scalar-field values so we can tell which ones aCalcPerform wrote to. */
		for (i=0, pnew=&pcalc->a, pprev=&pcalc->pa; i<MAX_FIELDS;  i++, pnew++, pprev++) {
			*pprev = *pnew;
		}

		/* if some links are CA, check connections */
		if (prpvt->caLinkStat != NO_CA_LINKS) checkLinks(pcalc);
		if (fetch_values(pcalc)==0) {
			long stat;

			if (aCalcoutRecordDebug >= 5) printf("acalcoutRecord(%s):process: queueing aCalcPerform\n", pcalc->name);

			pcalc->cact = 0;
			stat = doCalc(pcalc);
			if (stat) printf("%s:process: doCalc failed.\n", pcalc->name);
			if (stat == 0 && pcalc->cact == 1) {
				pcalc->pact = 1;
				/* we'll get processed again when the calculation is done */
				return(0);
			} else {
				if (afterCalc(pcalc) == ASYNC) return(0);
			}
		}
	} else { /* pact == TRUE */

		/* Who invoked us ? */
		if (pcalc->cact) {
			pcalc->cact = 0;
			if (afterCalc(pcalc) == ASYNC) return(0);
		} else if (pcalc->dlya) {
			/* callbackRequestProcessCallbackDelayed() called us */
			pcalc->dlya = 0;
			db_post_events(pcalc,&pcalc->dlya,DBE_VALUE);

			/* Must set pact 0 so that asynchronous device support works */
			pcalc->pact = 0;
			execOutput(pcalc);
			if (pcalc->pact) return(0);
			pcalc->pact = TRUE;
		} else {
			/* We must have been called by asynchronous device support */
			writeValue(pcalc);
		}
	}
	/*checkAlarms(pcalc); This is too late; IVOA might have vetoed output */
	recGblGetTimeStamp(pcalc);

	if (aCalcoutRecordDebug >= 5) {
		printf("acalcoutRecord(%s):process:calling monitor \n", pcalc->name);
	}

    monitor(pcalc);
    recGblFwdLink(pcalc);
    pcalc->pact = FALSE;

	if (aCalcoutRecordDebug >= 5) {
		printf("acalcoutRecord(%s):process-done\n", pcalc->name);
	}
	if (aCalcoutRecordDebug) printf("acalcoutRecord(%s):process: exit, pact==0\n",
		pcalc->name);
	return(0);
}

static long special(dbAddr	*paddr, int after)
{
	acalcoutRecord	*pcalc = (acalcoutRecord *)(paddr->precord);
	rpvtStruct		*prpvt = (struct rpvtStruct *)pcalc->rpvt;
	dbAddr			Addr;
	dbAddr			*pAddr = &Addr;
	short			error_number;
	int				fieldIndex = dbGetFieldIndex(paddr);
	int				lnkIndex;
	DBLINK			*plink;
	double			*pvalue;
	unsigned short	*plinkValid;

	if (!after) return(0);
	switch (fieldIndex) {
	case acalcoutRecordCALC:
		pcalc->clcv = aCalcPostfix(pcalc->calc, pcalc->rpcl, &error_number);
		if (pcalc->clcv) {
			recGblRecordError(S_db_badField,(void *)pcalc,
				"acalcout: special(): Illegal CALC field");
			if (aCalcoutRecordDebug >= 10)
				printf("acalcPostfix returns: %d\n", error_number);
		}
		db_post_events(pcalc,&pcalc->clcv,DBE_VALUE);
		return(0);
		break;

	case acalcoutRecordOCAL:
		pcalc->oclv = aCalcPostfix(pcalc->ocal, pcalc->orpc, &error_number);
		if (pcalc->oclv) {
			recGblRecordError(S_db_badField,(void *)pcalc,
				"acalcout: special(): Illegal OCAL field");
			if (aCalcoutRecordDebug >= 10)
				printf("acalcPostfix returns: %d\n", error_number);
		}
		db_post_events(pcalc,&pcalc->oclv,DBE_VALUE);
		return(0);
		break;

	case acalcoutRecordNUSE:
		if ((pcalc->nuse < 0) || (pcalc->nuse > pcalc->nelm)) {
			pcalc->nuse = pcalc->nelm;
			db_post_events(pcalc,&pcalc->nuse,DBE_VALUE);
			return(-1);
		}
		return(0);
		break;

	case(acalcoutRecordINPA):
	case(acalcoutRecordINPB):
	case(acalcoutRecordINPC):
	case(acalcoutRecordINPD):
	case(acalcoutRecordINPE):
	case(acalcoutRecordINPF):
	case(acalcoutRecordINPG):
	case(acalcoutRecordINPH):
	case(acalcoutRecordINPI):
	case(acalcoutRecordINPJ):
	case(acalcoutRecordINPK):
	case(acalcoutRecordINPL):
	case(acalcoutRecordINAA):
	case(acalcoutRecordINBB):
	case(acalcoutRecordINCC):
	case(acalcoutRecordINDD):
	case(acalcoutRecordINEE):
	case(acalcoutRecordINFF):
	case(acalcoutRecordINGG):
	case(acalcoutRecordINHH):
	case(acalcoutRecordINII):
	case(acalcoutRecordINJJ):
	case(acalcoutRecordINKK):
	case(acalcoutRecordINLL):
	case(acalcoutRecordOUT):
		lnkIndex = fieldIndex - acalcoutRecordINPA;
		plink   = &pcalc->inpa + lnkIndex;
		pvalue  = &pcalc->a    + lnkIndex;
		plinkValid = &pcalc->inav + lnkIndex;

		if (plink->type == CONSTANT) {
			if (fieldIndex <= acalcoutRecordINPL) {
				recGblInitConstantLink(plink,DBF_DOUBLE,pvalue);
				db_post_events(pcalc,pvalue,DBE_VALUE);
			}
			*plinkValid = acalcoutINAV_CON;
			if (fieldIndex == acalcoutRecordOUT)
				prpvt->outlink_field_type = DBF_NOACCESS;
		} else if (!dbNameToAddr(plink->value.pv_link.pvname, pAddr)) {
			/* PV resides on this ioc */
			*plinkValid = acalcoutINAV_LOC;
			if (fieldIndex == acalcoutRecordOUT) {
				prpvt->outlink_field_type = pAddr->field_type;
				if ((pAddr->field_type >= DBF_INLINK) && (pAddr->field_type <= DBF_FWDLINK)) {
					if (!(plink->value.pv_link.pvlMask & pvlOptCA)) {
						printf("aCalcoutRecord(%s):special:non-CA link to link field\n",
							plink->value.pv_link.pvname);
					}
				}
				if (pcalc->wait && !(plink->value.pv_link.pvlMask & pvlOptCA)) {
					printf("aCalcoutRecord(%s):special: Can't wait with non-CA link attribute\n",
						plink->value.pv_link.pvname);
				}
			}
		} else {
			/* pv is not on this ioc. Callback later for connection stat */
			*plinkValid = acalcoutINAV_EXT_NC;
			/* DO_CALLBACK, if not already scheduled */
			if (!prpvt->wd_id_1_LOCK) {
				callbackRequestDelayed(&prpvt->checkLinkCb,.5);
				prpvt->wd_id_1_LOCK = 1;
				prpvt->caLinkStat = CA_LINKS_NOT_OK;
			}
			if (fieldIndex == acalcoutRecordOUT)
				prpvt->outlink_field_type = DBF_NOACCESS; /* don't know */
		}
        db_post_events(pcalc,plinkValid,DBE_VALUE);
		return(0);
		break;

	default:
		recGblDbaddrError(S_db_badChoice,paddr,"calc: special");
		return(S_db_badChoice);
	}
	return(0);
}

static long cvt_dbaddr(dbAddr *paddr)
{
	acalcoutRecord	*pcalc = (acalcoutRecord *) paddr->precord;
	double			**ppd;
	short			i;
    int				fieldIndex = dbGetFieldIndex(paddr);

	if (aCalcoutRecordDebug >= 20) printf("acalcoutRecord(%s):cvt_dbaddr: paddr->pfield = %p\n",
		pcalc->name, (void *)paddr->pfield);
	if ((fieldIndex>=acalcoutRecordAA) && (fieldIndex<=acalcoutRecordLL)) {
		ppd = &(pcalc->aa);
		i = fieldIndex-acalcoutRecordAA;
		if (ppd[i] == NULL) {
			if (aCalcoutRecordDebug) printf("acalcoutRecord(%s):cvt_dbaddr: allocating for field %c%c\n",
				pcalc->name, (int)('A'+i), (int)('A'+i));
			ppd[i] = (double *)calloc(pcalc->nelm, sizeof(double));
		}
		paddr->pfield = ppd[i];
	} else if (fieldIndex==acalcoutRecordAVAL) {
		if (pcalc->aval == NULL) pcalc->aval = (double *)calloc(pcalc->nelm, sizeof(double));
		paddr->pfield = pcalc->aval;
	} else if (fieldIndex==acalcoutRecordOAV) {
		if (pcalc->oav == NULL) pcalc->oav = (double *)calloc(pcalc->nelm, sizeof(double));
		paddr->pfield = pcalc->oav;
	}

	/* What size should we report to CA clients?  Before EPICS 3.14.12, arrays
	 * were always sent in full to clients, regardless of the number of elements
	 * actually in use.  To work around this problem, user can specify
	 * SIZE="NUSE", and the acalcout record will tell the client that the array
	 * size is NUSE, rather than NELM.  But this is a problem for clients that
	 * connect to a record whose NUSE increases, because they won't see the
	 * additional elements until they disconnect and reconnect to the array.
	 */
	if (pcalc->size == acalcoutSIZE_NUSE) {
		paddr->no_elements = acalcGetNumElements( pcalc );
	} else {
		paddr->no_elements = pcalc->nelm;
	}

	paddr->field_type = DBF_DOUBLE;
	paddr->field_size = sizeof(double);
	paddr->dbr_field_type = DBF_DOUBLE;
	return(0);
}

static long get_array_info(struct dbAddr *paddr, long *no_elements, long *offset)
{
	acalcoutRecord	*pcalc = (acalcoutRecord *) paddr->precord;
    int				i, fieldIndex = dbGetFieldIndex(paddr);
	double			**ppd;

	if (aCalcoutRecordDebug >= 20) printf("acalcoutRecord(%s):get_array_info: paddr->pfield = %p\n",
		pcalc->name, (void *)paddr->pfield);

	if ((fieldIndex>=acalcoutRecordAA) && (fieldIndex<=acalcoutRecordLL)) {
		i = fieldIndex-acalcoutRecordAA;
		ppd = &(pcalc->aa);
		if (ppd[i] == NULL) {
			if (aCalcoutRecordDebug) printf("acalcoutRecord(%s):get_array_info: allocating for field %c%c\n",
				pcalc->name, (int)('A'+i), (int)('A'+i));
			ppd[i] = (double *)calloc(pcalc->nelm, sizeof(double));
		}
	}
	if ((fieldIndex==acalcoutRecordAVAL) && (pcalc->aval == NULL)) {
		pcalc->aval = (double *)calloc(pcalc->nelm, sizeof(double));
	}
	if ((fieldIndex==acalcoutRecordOAV) && (pcalc->oav == NULL)) {
		pcalc->oav = (double *)calloc(pcalc->nelm, sizeof(double));
	}
    *no_elements = acalcGetNumElements( pcalc );
    *offset = 0;
    return(0);
}

static long put_array_info(struct dbAddr *paddr, long nNew)
{
	acalcoutRecord	*pcalc = (acalcoutRecord *) paddr->precord;
	double			**ppd, *pd = NULL;
	long			i;
	long			numElements;
    int				fieldIndex = dbGetFieldIndex(paddr);

	if (aCalcoutRecordDebug >= 20) {
		printf("acalcoutRecord(%s):put_array_info: paddr->pfield = %p, pcalc->aa=%p, nNew=%ld\n",
			pcalc->name, (void *)paddr->pfield, (void *)pcalc->aa, nNew);
	}

	if ((fieldIndex>=acalcoutRecordAA) && (fieldIndex<=acalcoutRecordLL)) {
		i = fieldIndex-acalcoutRecordAA;
		ppd = &(pcalc->aa);
		if (ppd[i] == NULL) {
			if (aCalcoutRecordDebug) printf("acalcoutRecord(%s):put_array_info: allocating for field %c%c\n",
				pcalc->name, (int)('A'+i), (int)('A'+i));
			ppd[i] = (double *)calloc(pcalc->nelm, sizeof(double));
		}
		pd = ppd[i];
	} else if (fieldIndex==acalcoutRecordAVAL) {
		if (pcalc->aval == NULL) pcalc->aval = (double *)calloc(pcalc->nelm, sizeof(double));
		pd = pcalc->aval;
	} else if (fieldIndex==acalcoutRecordOAV) {
		if (pcalc->oav == NULL) pcalc->oav = (double *)calloc(pcalc->nelm, sizeof(double));
		pd = pcalc->oav;
	}

	if (aCalcoutRecordDebug >= 20) {
		printf("acalcoutRecord(%s):put_array_info: pd=%p\n", pcalc->name, (void *)pd);
	}

#if MIND_UNUSED_ELEMENTS
	numElements = pcalc->nelm;
#else
	numElements = acalcGetNumElements( pcalc );
#endif
	if ( pd && (nNew < numElements) )
		for (i=nNew; i<numElements; i++)
			pd[i] = 0.;

	/* We could set nuse to the number of elements just written, but that would also
	 * affect the other arrays.  For now, with all arrays sharing a single value of nuse,
	 * it seems better to require that nuse be set explicitly.  Currently, I'm leaving
	 * unanswered the question of whether each array should have its own 'nuse'.  The
	 * array-calc engine currently doesn't support per-array 'nuse'.
	 */

    return(0);
}

static long get_units(dbAddr *paddr, char *units)
{
	acalcoutRecord	*pcalc=(acalcoutRecord *)paddr->precord;

	strncpy(units,pcalc->egu,DB_UNITS_SIZE);
	return(0);
}

static long get_precision(dbAddr *paddr, long *precision)
{
	acalcoutRecord	*pcalc=(acalcoutRecord *)paddr->precord;
	int fieldIndex = dbGetFieldIndex(paddr);

	*precision = pcalc->prec;
	if (fieldIndex == acalcoutRecordVAL) return(0);
	recGblGetPrec(paddr,precision);
	return(0);
}

static long get_graphic_double(dbAddr *paddr, struct dbr_grDouble *pgd)
{
    acalcoutRecord	*pcalc=(acalcoutRecord *)paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);

	switch (fieldIndex) {
	case acalcoutRecordVAL:
	case acalcoutRecordHIHI:
	case acalcoutRecordHIGH:
	case acalcoutRecordLOW:
	case acalcoutRecordLOLO:
		pgd->upper_disp_limit = pcalc->hopr;
		pgd->lower_disp_limit = pcalc->lopr;
		return(0);
	default:
		break;
	} 

	if (fieldIndex >= acalcoutRecordA && fieldIndex <= acalcoutRecordL) {
		pgd->upper_disp_limit = pcalc->hopr;
		pgd->lower_disp_limit = pcalc->lopr;
		return(0);
	}
	if (fieldIndex >= acalcoutRecordPA && fieldIndex <= acalcoutRecordPL) {
		pgd->upper_disp_limit = pcalc->hopr;
		pgd->lower_disp_limit = pcalc->lopr;
		return(0);
	}
	return(0);
}

static long get_control_double(dbAddr *paddr, struct dbr_ctrlDouble *pcd)
{
	acalcoutRecord	*pcalc=(acalcoutRecord *)paddr->precord;
	int fieldIndex = dbGetFieldIndex(paddr);

	switch (fieldIndex) {
	case acalcoutRecordVAL:
	case acalcoutRecordHIHI:
	case acalcoutRecordHIGH:
	case acalcoutRecordLOW:
	case acalcoutRecordLOLO:
		pcd->upper_ctrl_limit = pcalc->hopr;
		pcd->lower_ctrl_limit = pcalc->lopr;
		return(0);
	default:
		break;
    } 

	if (fieldIndex >= acalcoutRecordA && fieldIndex <= acalcoutRecordL) {
		pcd->upper_ctrl_limit = pcalc->hopr;
		pcd->lower_ctrl_limit = pcalc->lopr;
		return(0);
	}
	if (fieldIndex >= acalcoutRecordPA && fieldIndex <= acalcoutRecordPL) {
		pcd->upper_ctrl_limit = pcalc->hopr;
		pcd->lower_ctrl_limit = pcalc->lopr;
		return(0);
	}
	return(0);
}
static long get_alarm_double(dbAddr *paddr, struct dbr_alDouble *pad)
{
	acalcoutRecord	*pcalc=(acalcoutRecord *)paddr->precord;
	int fieldIndex = dbGetFieldIndex(paddr);

	if (fieldIndex == acalcoutRecordVAL) {
		pad->upper_alarm_limit = pcalc->hihi;
		pad->upper_warning_limit = pcalc->high;
		pad->lower_warning_limit = pcalc->low;
		pad->lower_alarm_limit = pcalc->lolo;
	} else
		 recGblGetAlarmDouble(paddr,pad);
	return(0);
}


static void checkAlarms(acalcoutRecord *pcalc)
{
	double			val;
	double			hyst, lalm, hihi, high, low, lolo;
	unsigned short	hhsv, llsv, hsv, lsv;

	if (pcalc->udf == TRUE) {
#if LT_EPICSBASE(3,15,0,2)
		recGblSetSevr(pcalc,UDF_ALARM,INVALID_ALARM);
#else
		recGblSetSevr(pcalc,UDF_ALARM,pcalc->udfs);
#endif
		return;
	}
	hihi = pcalc->hihi; 
	lolo = pcalc->lolo; 
	high = pcalc->high;  
	low = pcalc->low;
	hhsv = pcalc->hhsv; 
	llsv = pcalc->llsv; 
	hsv = pcalc->hsv; 
	lsv = pcalc->lsv;
	val = pcalc->val; 
	hyst = pcalc->hyst; 
	lalm = pcalc->lalm;

	/* alarm condition hihi */
	if (hhsv && (val >= hihi || ((lalm==hihi) && (val >= hihi-hyst)))) {
		if (recGblSetSevr(pcalc,HIHI_ALARM,pcalc->hhsv)) pcalc->lalm = hihi;
		return;
	}

	/* alarm condition lolo */
	if (llsv && (val <= lolo || ((lalm==lolo) && (val <= lolo+hyst)))) {
		if (recGblSetSevr(pcalc,LOLO_ALARM,pcalc->llsv)) pcalc->lalm = lolo;
		return;
	}

	/* alarm condition high */
	if (hsv && (val >= high || ((lalm==high) && (val >= high-hyst)))) {
		if (recGblSetSevr(pcalc,HIGH_ALARM,pcalc->hsv)) pcalc->lalm = high;
		return;
	}

	/* alarm condition low */
	if (lsv && (val <= low || ((lalm==low) && (val <= low+hyst)))) {
		if (recGblSetSevr(pcalc,LOW_ALARM,pcalc->lsv)) pcalc->lalm = low;
		return;
	}

	/* we get here only if val is out of alarm by at least hyst */
	pcalc->lalm = val;
	return;
}


static void execOutput(acalcoutRecord *pcalc)
{
	long		status;

	/* Determine output data */
	if (aCalcoutRecordDebug >= 10)
		printf("acalcoutRecord(%s):execOutput:entry\n", pcalc->name);

	/* Check to see what to do if INVALID */
	if (pcalc->nsev < INVALID_ALARM) {
		/* Output the value */
		if (aCalcoutRecordDebug >= 10)
			printf("acalcoutRecord(%s):execOutput:calling writeValue\n", pcalc->name);
		status = writeValue(pcalc);
		/* post event if output event != 0 */
		if (pcalc->oevt > 0) post_event((int)pcalc->oevt);
	} else {
		switch (pcalc->ivoa) {
		case menuIvoaContinue_normally:
			/* write the new value */
			status = writeValue(pcalc);
			/* post event if output event != 0 */
			if (pcalc->oevt > 0) post_event((int)pcalc->oevt);
			break;

		case menuIvoaDon_t_drive_outputs:
			break;

		case menuIvoaSet_output_to_IVOV:
			pcalc->oval=pcalc->ivov;
			status = writeValue(pcalc);
			/* post event if output event != 0 */
			if (pcalc->oevt > 0) post_event((int)pcalc->oevt);
			break;

		default:
			status=-1;
			recGblRecordError(S_db_badField,(void *)pcalc,
				"acalcout:process Illegal IVOA field");
		}
	} 
}

static void monitor(acalcoutRecord *pcalc)
{
	unsigned short	monitor_mask;
	double			delta;
	double			*pnew, *pprev;
	double			**panew;
	int				i, diff;
	long			numElements;

	if (aCalcoutRecordDebug >= 10)
		printf("acalcoutRecord(%s):monitor:entry\n", pcalc->name);
	monitor_mask = recGblResetAlarms(pcalc);
	/* check for value change */
	delta = pcalc->mlst - pcalc->val;
	if (delta < 0.0) delta = -delta;
	if (delta > pcalc->mdel) {
		/* post events for value change */
		monitor_mask |= DBE_VALUE;
		/* update last value monitored */
		pcalc->mlst = pcalc->val;
	}
	/* check for archive change */
	delta = pcalc->alst - pcalc->val;
	if (delta < 0.0) delta = -delta;
	if (delta > pcalc->adel) {
		/* post events on value field for archive change */
		monitor_mask |= DBE_LOG;
		/* update last archive value monitored */
		pcalc->alst = pcalc->val;
	}
	/* send out monitors connected to the value field */
	if (monitor_mask) db_post_events(pcalc,&pcalc->val,monitor_mask);

	/* If we haven't allocated previous-value fields, do it now. */
	if (pcalc->pavl == NULL)
		pcalc->pavl = (double *)calloc(pcalc->nelm, sizeof(double));
	if (pcalc->poav == NULL)
		pcalc->poav = (double *)calloc(pcalc->nelm, sizeof(double));

#if MIND_UNUSED_ELEMENTS
	numElements = pcalc->nelm;
#else
	numElements = acalcGetNumElements( pcalc );
#endif

	for (i=0, diff=0; i<numElements; i++) {
		if (pcalc->aval[i] != pcalc->pavl[i]) {diff = 1;break;}
	}

	if (diff) {
		if (aCalcoutRecordDebug >= 1)
			printf("acalcoutRecord(%s):posting .AVAL\n", pcalc->name);
		db_post_events(pcalc, pcalc->aval, monitor_mask|DBE_VALUE|DBE_LOG);
		for (i=0; i<numElements; i++) pcalc->pavl[i] = pcalc->aval[i];
	}

	for (i=0, diff=0; i<numElements; i++) {
		if (pcalc->oav[i] != pcalc->poav[i]) {diff = 1;break;}
	}

	if (diff) {
		db_post_events(pcalc, pcalc->oav, monitor_mask|DBE_VALUE|DBE_LOG);
		for (i=0; i<numElements; i++) pcalc->poav[i] = pcalc->oav[i];
	}

	/* check all input fields for changes */
	for (i=0, pnew=&pcalc->a, pprev=&pcalc->pa; i<MAX_FIELDS;  i++, pnew++, pprev++) {
		if ((*pnew != *pprev) || (monitor_mask&DBE_ALARM)) {
			db_post_events(pcalc,pnew,monitor_mask|DBE_VALUE|DBE_LOG);
			*pprev = *pnew;
		}
	}

	for (i=0, panew=&pcalc->aa; i<ARRAY_MAX_FIELDS; i++, panew++) {
		if (*panew && (pcalc->newm & (1<<i))) {
			db_post_events(pcalc, *panew, monitor_mask|DBE_VALUE|DBE_LOG);
		}
	}
	pcalc->newm = 0;

	/* Check OVAL field */
	if (pcalc->povl != pcalc->oval) {
		db_post_events(pcalc, &pcalc->oval, monitor_mask|DBE_VALUE|DBE_LOG);
		pcalc->povl = pcalc->oval;
	}
	return;
}

static int fetch_values(acalcoutRecord *pcalc)
{
	DBLINK	*plink;	/* structure of the link field  */
	double	*pvalue;
	double	**pavalue;
	long	status = 0;
	int		i, j;
	unsigned short *plinkValid;
	long numElements;

#if MIND_UNUSED_ELEMENTS
	numElements = pcalc->nelm;
#else
	numElements = acalcGetNumElements( pcalc );
#endif
	if (aCalcoutRecordDebug >= 10)
		printf("acalcoutRecord(%s):fetch_values: entry\n", pcalc->name);
	for (i=0, plink=&pcalc->inpa, pvalue=&pcalc->a; i<MAX_FIELDS; 
			i++, plink++, pvalue++) {
		status = dbGetLink(plink, DBR_DOUBLE, pvalue, 0, 0);
		if (!RTN_SUCCESS(status)) return(status);
	}

	if (aCalcoutRecordDebug >= 10) printf("acalcoutRecord(%s):fetch_values: arrays\n", pcalc->name);
	plinkValid = &pcalc->iaav;
	for (i=0, plink=&pcalc->inaa, pavalue=(double **)(&pcalc->aa); i<ARRAY_MAX_FIELDS; 
			i++, plink++, pavalue++, plinkValid++) {
		if ((*plinkValid==acalcoutINAV_EXT) || (*plinkValid==acalcoutINAV_LOC)) {
			long	nRequest;
			if (aCalcoutRecordDebug >= 10) printf("acalcoutRecord(%s):fetch_values: field %c%c, pointer=%p\n",
				pcalc->name, (int)('A'+i), (int)('A'+i), *pavalue);
			if (*pavalue == NULL) {
				if (aCalcoutRecordDebug) printf("acalcoutRecord(%s): allocating for field %c%c\n",
					pcalc->name, (int)('A'+i), (int)('A'+i));
				*pavalue = (double *)calloc(pcalc->nelm, sizeof(double));
			}
			/* save current array value */
			if (pcalc->paa == NULL) {
				if (aCalcoutRecordDebug) printf("acalcoutRecord(%s): allocating for field PAA\n",
					pcalc->name);
				pcalc->paa = (double *)calloc(pcalc->nelm, sizeof(double));
			}
			for (j=0; j<numElements; j++) pcalc->paa[j] = (*pavalue)[j];
			/* get new value */
			nRequest = acalcGetNumElements( pcalc );
			status = dbGetLink(plink, DBR_DOUBLE, *pavalue, 0, &nRequest);
			if (!RTN_SUCCESS(status)) return(status);
			if (nRequest<numElements) {
				for (j=nRequest; j<numElements; j++) (*pavalue)[j] = 0;
			}
			/* compare new array value with saved value */
			for (j=0; j<numElements; j++) {
				if (pcalc->paa[j] != (*pavalue)[j]) {pcalc->newm |= 1<<i; break;}
			}
		}
	}
	if (aCalcoutRecordDebug >= 10)
		printf("acalcoutRecord(%s):fetch_values: returning\n", pcalc->name);
	return(0);
}

static void checkLinksCallback(CALLBACK *pcallback)
{
    acalcoutRecord	*pcalc;
    rpvtStruct		*prpvt;

    callbackGetUser(pcalc, pcallback);
    prpvt = (rpvtStruct *)pcalc->rpvt;
    
	if (!interruptAccept) {
		/* Can't call dbScanLock yet.  Schedule another CALLBACK */
		prpvt->wd_id_1_LOCK = 1;  /* make sure */
		callbackRequestDelayed(&prpvt->checkLinkCb,.5);
	} else {
	    dbScanLock((struct dbCommon *)pcalc);
	    prpvt->wd_id_1_LOCK = 0;
	    checkLinks(pcalc);
	    dbScanUnlock((struct dbCommon *)pcalc);
	}
}


static void checkLinks(acalcoutRecord *pcalc)
{
	DBLINK			*plink;
	rpvtStruct		*prpvt = (rpvtStruct *)pcalc->rpvt;
	int i;
	int				isCaLink   = 0;
	int				isCaLinkNc = 0;
	unsigned short	*plinkValid;
	dbAddr			Addr;
	dbAddr			*pAddr = &Addr;

	if (aCalcoutRecordDebug >= 10) printf("checkLinks() for %p\n", (void *)pcalc);

	plink   = &pcalc->inpa;
	plinkValid = &pcalc->inav;

	for (i=0; i<MAX_FIELDS+ARRAY_MAX_FIELDS+1; i++, plink++, plinkValid++) {
		if (plink->type == CA_LINK) {
			isCaLink = 1;
			if (dbCaIsLinkConnected(plink)) {
				if (*plinkValid == acalcoutINAV_EXT_NC) {
					*plinkValid = acalcoutINAV_EXT;
					db_post_events(pcalc,plinkValid,DBE_VALUE);
				}
				/* If this is the outlink, get the type of field it's connected to.  If it's connected
				 * to a link field, and the outlink is not a CA link, complain, because this won't work.
				 * Also, if .WAIT, then the link must be a CA link.
				 */
				if (plink == &pcalc->out) {
					prpvt->outlink_field_type = dbCaGetLinkDBFtype(plink);
					if (aCalcoutRecordDebug >= 10)
						printf("acalcout:checkLinks: outlink type = %d\n",
							prpvt->outlink_field_type);
					if (!dbNameToAddr(plink->value.pv_link.pvname, pAddr)) {
						if ((pAddr->field_type >= DBF_INLINK) &&
								(pAddr->field_type <= DBF_FWDLINK)) {
							if (!(plink->value.pv_link.pvlMask & pvlOptCA)) {
								printf("aCalcoutRecord(%s):checkLinks:non-CA link to link field\n",
									plink->value.pv_link.pvname);
							}
						}
					}
					if (pcalc->wait && !(plink->value.pv_link.pvlMask & pvlOptCA)) {
						printf("aCalcoutRecord(%s):checkLinks: Can't wait with non-CA link attribute\n",
							plink->value.pv_link.pvname);
					}
				}
			} else {
				if (*plinkValid == acalcoutINAV_EXT_NC) {
					isCaLinkNc = 1;
				}
				else if (*plinkValid == acalcoutINAV_EXT) {
					*plinkValid = acalcoutINAV_EXT_NC;
					db_post_events(pcalc,plinkValid,DBE_VALUE);
					isCaLinkNc = 1;
				}
				if (plink == &pcalc->out)
					prpvt->outlink_field_type = DBF_NOACCESS; /* don't know type */
			} 
		}
	}
	if (isCaLinkNc)
		prpvt->caLinkStat = CA_LINKS_NOT_OK;
	else if (isCaLink)
		prpvt->caLinkStat = CA_LINKS_ALL_OK;
	else
		prpvt->caLinkStat = NO_CA_LINKS;

	if (!prpvt->wd_id_1_LOCK && isCaLinkNc) {
		/* Schedule another CALLBACK */
		prpvt->wd_id_1_LOCK = 1;
		callbackRequestDelayed(&prpvt->checkLinkCb,.5);
	}
}


static long writeValue(acalcoutRecord *pcalc)
{
    acalcoutDSET	*pacalcoutDSET = (acalcoutDSET *)pcalc->dset;

	if (aCalcoutRecordDebug >= 10)
		printf("acalcoutRecord(%s):writeValue:entry\n", pcalc->name);

    if (!pacalcoutDSET || !pacalcoutDSET->write) {
        errlogPrintf("%s DSET write does not exist\n",pcalc->name);
        recGblSetSevr(pcalc,SOFT_ALARM,INVALID_ALARM);
        pcalc->pact = TRUE;
        return(-1);
    }
	if (aCalcoutRecordDebug >= 10)
		printf("acalcoutRecord(%s):writeValue:calling device support\n", pcalc->name);
	return pacalcoutDSET->write(pcalc);
}

/************************************************************/
#include <epicsMessageQueue.h>
#include <epicsThread.h>

static epicsThreadId		acalcThreadId = NULL;
static epicsMessageQueueId	acalcMsgQueue = NULL;

typedef struct {
	acalcoutRecord *pcalc;
} calcMessage;

#define MAX_MSG  100                   /* max # of messages in queue    */
#define MSG_SIZE sizeof(calcMessage)   /* size in bytes of the messages */
#define PRIORITY epicsThreadPriorityMedium
volatile int aCalcAsyncThreshold = 10000; /* array sizes larger than this get queued */
epicsExportAddress(int, aCalcAsyncThreshold);

static void call_aCalcPerform(acalcoutRecord *pcalc) {
	long numElements;
	epicsUInt32 amask;

	if (aCalcoutRecordDebug >= 10) printf("call_aCalcPerform:entry\n");

	/* Note that we want to permit nuse == 0 as a way of saying "use nelm". */
	numElements = acalcGetNumElements( pcalc );
	pcalc->cstat = aCalcPerform(&pcalc->a, MAX_FIELDS, &pcalc->aa,
		ARRAY_MAX_FIELDS, numElements, &pcalc->val, pcalc->aval, pcalc->rpcl,
		pcalc->nelm, &pcalc->amask);
	
	if (pcalc->dopt == acalcoutDOPT_Use_OVAL) {
		pcalc->cstat |= aCalcPerform(&pcalc->a, MAX_FIELDS, &pcalc->aa,
			ARRAY_MAX_FIELDS, numElements, &pcalc->oval, pcalc->oav, pcalc->orpc,
			pcalc->nelm, &amask);
		pcalc->amask |= amask;
	}
}

static long doCalc(acalcoutRecord *pcalc) {
	calcMessage msg;
	int doAsync = 0;

	if (aCalcoutRecordDebug >= 10)
		printf("acalcoutRecord(%s):doCalc\n", pcalc->name);

	if ( acalcGetNumElements(pcalc) > aCalcAsyncThreshold )
		doAsync = 1;

	/* if required infrastructure doesn't yet exist, create it */
	if (doAsync && acalcMsgQueue == NULL) {
		acalcMsgQueue = epicsMessageQueueCreate(MAX_MSG, MSG_SIZE);
		if (acalcMsgQueue==NULL) {
			printf("aCalcoutRecord: Unable to create message queue\n");
			return(-1);
		}

		acalcThreadId = epicsThreadCreate("acalcPerformTask", PRIORITY,
			epicsThreadGetStackSize(epicsThreadStackBig),
			(EPICSTHREADFUNC)acalcPerformTask, (void *)epicsThreadGetIdSelf());

		if (acalcThreadId == NULL) {
			printf("aCalcoutRecord: Unable to create acalcPerformTask\n");
			epicsMessageQueueDestroy(acalcMsgQueue);
			acalcMsgQueue = NULL;
			return(-1);
		}
	}
	
	/* Ideally, we should do short calculations in this thread, and queue long calculations.
	 * But aCalcPerform is not reentrant (global value stack), so for now we queue everything.
	 */
	if (doAsync) {
		if (aCalcoutRecordDebug >= 2) printf("acalcoutRecord(%s):doCalc async\n", pcalc->name);
		pcalc->cact = 1; /* Tell caller that we went asynchronous */
		msg.pcalc = pcalc;
		epicsMessageQueueSend(acalcMsgQueue, (void *)&msg, MSG_SIZE);
		return(0);
	} else {
		if (aCalcoutRecordDebug >= 2) printf("acalcoutRecord(%s):doCalc sync\n", pcalc->name);
		call_aCalcPerform(pcalc);
	}
	return(0);
}

static void acalcPerformTask(void *parm) {
	calcMessage msg;
	acalcoutRecord *pcalc;
	struct rset *prset;

	if (aCalcoutRecordDebug >= 10)
		printf("acalcPerformTask:entry\n");

	while (1) {
		/* waiting for messages */
		if (epicsMessageQueueReceive(acalcMsgQueue, &msg, MSG_SIZE) != MSG_SIZE) {
			printf("acalcPerformTask: epicsMessageQueueReceive returned wrong size\n");
			break;
		}

		pcalc = msg.pcalc;
		prset = (struct rset *)(pcalc->rset);

		dbScanLock((struct dbCommon *)pcalc);

		if (aCalcoutRecordDebug >= 10)
			printf("acalcPerformTask:message from '%s'\n", pcalc->name);
		call_aCalcPerform(pcalc);
		if (aCalcoutRecordDebug >= 10)
			printf("acalcPerformTask:processing '%s'\n", pcalc->name);

		(*prset->process)(pcalc);
		dbScanUnlock((struct dbCommon *)pcalc);
	}
}


