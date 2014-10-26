/* scalcout.c - Record Support Routines for calc with output records */
/*
 *   Author : Tim Mooney
 *   Based on calcoutRecord recCalc.c, by Ned Arnold
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1998, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * 03-24-98    tmm    v2.0: created from Ned Arnold's calcout record
 * 07-06-98    tmm    v3.1: prototype field-type sensitive output link
 * 08-25-98    tmm    v3.2: use new support in EPICS 3.13.0.beta12 for
 *                    field-type sensitive link.
 * 11-04-98    tmm    v3.3: Don't call dbScanLock until interruptAccept
 *                    removed some dead code re. type-sensitive links
 * 11-11-98    tmm    v3.4: Support 12 strings settable at DCT time
 * 03-23-99    tmm    v3.5: time stamp support
 * 01-18-00    tmm    v3.6: special() did not list INGG...INLL
 * 05-08-00    tmm    v3.61: changed some status messages to debug messages.
 * 08-22-00    tmm    v3.62: changed message text.
 * 04-22-03    tmm    v3.7: RPC fields now allocated in dbd file, since
 *                    sCalcPostfix doesn't allocate them anymore
 * 06-26-03    rls    Port to 3.14; alarm() conflicts with alarm declaration
 *                    in unistd.h (sCalcoutRecord.h->epicsTime.h->osdTime.h->
 *                    unistd.h) when compiled with SUNPro.
 * 09-11-04    tmm    v3.8: use device support instead of writing directly
 *                    to OUT.  Misc changes to stay closer to calcout record's
 *                    implementation.
 * 05-24-06    tmm    v3.9  Added Dirk Zimoch's fix to call DSET->init_record()
 * 01-24-08    tmm    v4.0: Fixed check of outlink (if link to link field,
 *                    or if .WAIT, then outlink attribute must be CA).
 * 04-29-08    tmm    v4.1: Peter Mueller noticed that calc records were not
 *                    checking VAL against limits until after execOutput --
 *                    too late to do IVOA.
 */

#define VERSION 4.1


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
#include	<epicsString.h>	/* for epicsStrSnPrintEscaped() */
#include	<epicsStdio.h> /* for epicsSnprintf() */
#include	"sCalcPostfix.h"

#define GEN_SIZE_OFFSET
#include	"sCalcoutRecord.h"
#undef  GEN_SIZE_OFFSET
#include	<menuIvoa.h>
#include	<epicsExport.h>

#include	<epicsVersion.h>
#ifndef EPICS_VERSION_INT
#define VERSION_INT(V,R,M,P) ( ((V)<<24) | ((R)<<16) | ((M)<<8) | (P))
#define EPICS_VERSION_INT VERSION_INT(EPICS_VERSION, EPICS_REVISION, EPICS_MODIFICATION, EPICS_PATCH_LEVEL)
#endif
#define LT_EPICSBASE(V,R,M,P) (EPICS_VERSION_INT < VERSION_INT((V),(R),(M),(P)))

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
static long special();
#define get_value NULL
static long cvt_dbaddr();
#define get_array_info NULL
#define put_array_info NULL
static long get_units();
static long get_precision();
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double();
static long get_control_double();
static long get_alarm_double();

rset scalcoutRSET={
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

typedef struct scalcoutDSET {
    long       number;
    DEVSUPFUN  dev_report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  write;
} scalcoutDSET;

epicsExportAddress(rset, scalcoutRSET);

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
static long writeValue(scalcoutRecord *pcalc);

volatile int    sCalcoutRecordDebug = 0;
epicsExportAddress(int, sCalcoutRecordDebug);

#define MAX_FIELDS 12
#define STRING_MAX_FIELDS 12
/*
 * Strings defined in the .dbd file are assumed to be of length STRING_SIZE.
 * Strings implemented in the .dbd file with a char * pointer (for which space
 * is allocated in init_record) are known to be of this length.
 */
#define STRING_SIZE 40

static char sFldnames[MAX_FIELDS][3] =
{"AA","BB","CC","DD","EE","FF","GG","HH","II","JJ","KK","LL"};

static long init_record(scalcoutRecord *pcalc, int pass)
{
	DBLINK *plink;
	int i;
	double *pvalue;
	unsigned short *plinkValid;
	short error_number;
	char *s, **ps;
    scalcoutDSET *pscalcoutDSET;

	dbAddr       Addr;
	dbAddr       *pAddr = &Addr;
	rpvtStruct   *prpvt;

	if (pass==0) {
		pcalc->vers = VERSION;
		pcalc->rpvt = (void *)calloc(1, sizeof(struct rpvtStruct));
		/* allocate space for previous-value strings */
		s = (char *)calloc(STRING_MAX_FIELDS, STRING_SIZE);
		for (i=0, ps=(char **)&(pcalc->paa); i<STRING_MAX_FIELDS; i++, ps++)
			*ps = &s[i*STRING_SIZE];
		/* allocate and fill in array of pointers to strings AA... */
		pcalc->strs = (char **)calloc(STRING_MAX_FIELDS, sizeof(char *));
		if (sCalcoutRecordDebug) printf("sCalcoutRecord:init_record: strs=%p\n",
			pcalc->strs);
		s = (char *)&(pcalc->aa);
		ps = (char **)(pcalc->strs);
		for (i=0; i<STRING_MAX_FIELDS; i++, s+=STRING_SIZE, ps++)
			*ps = s;
		return(0);
	}
    
	if (!(pscalcoutDSET = (scalcoutDSET *)pcalc->dset)) {
		recGblRecordError(S_dev_noDSET,(void *)pcalc,"scalcout:init_record");
		return(S_dev_noDSET);
	}
	/* must have write defined */
	if ((pscalcoutDSET->number < 5) || (pscalcoutDSET->write == NULL)) {
		recGblRecordError(S_dev_missingSup,(void *)pcalc,"calcout:init_record");
		return(S_dev_missingSup);
	}

	prpvt = (rpvtStruct *)pcalc->rpvt;
	plink = &pcalc->inpa;
	pvalue = &pcalc->a;
	plinkValid = &pcalc->inav;
	for (i=0; i<(MAX_FIELDS+STRING_MAX_FIELDS+1); i++, plink++, pvalue++, plinkValid++) {
		if (plink->type == CONSTANT) {
			/* Don't InitConstantLink the string links or the output link. */
			if (i < MAX_FIELDS) { 
				recGblInitConstantLink(plink,DBF_DOUBLE,pvalue);
				db_post_events(pcalc,pvalue,DBE_VALUE);
			}
			*plinkValid = scalcoutINAV_CON;
			if (plink == &pcalc->out)
				prpvt->outlink_field_type = DBF_NOACCESS;
        } else if (!dbNameToAddr(plink->value.pv_link.pvname, pAddr)) {
			/* the PV we're linked to resides on this ioc */
			*plinkValid = scalcoutINAV_LOC;
			if (plink == &pcalc->out) {
				prpvt->outlink_field_type = pAddr->field_type;
				if ((pAddr->field_type >= DBF_INLINK) && (pAddr->field_type <= DBF_FWDLINK)) {
					if (!(plink->value.pv_link.pvlMask & pvlOptCA)) {
						printf("sCalcoutRecord(%s):init_record:non-CA link to link field\n",
							plink->value.pv_link.pvname);
					}
				}
				if (pcalc->wait && !(plink->value.pv_link.pvlMask & pvlOptCA)) {
					printf("sCalcoutRecord(%s):init_record: Can't wait with non-CA link attribute\n",
						plink->value.pv_link.pvname);
				}
			}
		} else {
			/* pv is not on this ioc. Callback later for connection stat */
			*plinkValid = scalcoutINAV_EXT_NC;
			prpvt->caLinkStat = CA_LINKS_NOT_OK;
			if (plink == &pcalc->out)
				prpvt->outlink_field_type = DBF_NOACCESS; /* don't know field type */
		}
		db_post_events(pcalc,plinkValid,DBE_VALUE);
	}

	pcalc->clcv = sCalcPostfix(pcalc->calc,pcalc->rpcl,&error_number);
	if (pcalc->clcv) {
		recGblRecordError(S_db_badField,(void *)pcalc,
			"scalcout: init_record: Illegal CALC field");
		printf("sCalcPostfix returns: %d\n", error_number);
	}
	db_post_events(pcalc,&pcalc->clcv,DBE_VALUE);

	pcalc->oclv = sCalcPostfix(pcalc->ocal,pcalc->orpc,&error_number);
	if (pcalc->oclv) {
		recGblRecordError(S_db_badField,(void *)pcalc,
			"scalcout: init_record: Illegal OCAL field");
		printf("sCalcPostfix returns: %d\n", error_number);
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

	if (pscalcoutDSET->init_record ) {
		return (*pscalcoutDSET->init_record)(pcalc);
	}
    return(0);
}

static long process(scalcoutRecord *pcalc)
{
	rpvtStruct   *prpvt = (rpvtStruct *)pcalc->rpvt;
	short		doOutput = 0;
	long		stat;
	double		*pcurr, *pprev;
	char		**pscurr, **psprev;
	int			i;

	if (sCalcoutRecordDebug) printf("sCalcoutRecord(%s):process: strs=%p, pact=%d\n",
		pcalc->name, pcalc->strs, pcalc->pact);

	if (!pcalc->pact) {
		pcalc->pact = TRUE;
		/* if any links are CA, check connections */
		if (prpvt->caLinkStat != NO_CA_LINKS) checkLinks(pcalc);

		/* save all input-field values, so we can post any changes */
		for (i=0, pcurr=&pcalc->a, pprev=&pcalc->pa; i<MAX_FIELDS;
				i++, pcurr++, pprev++) {
			*pprev = *pcurr;
		}
		for (i=0, pscurr=pcalc->strs, psprev=&pcalc->paa; i<STRING_MAX_FIELDS;
				i++, pscurr++, psprev++) {
			strcpy(*psprev, *pscurr);
		}

		if (fetch_values(pcalc)==0) {
			stat = sCalcPerform(&pcalc->a, MAX_FIELDS, (char **)(pcalc->strs),
					STRING_MAX_FIELDS, &pcalc->val, pcalc->sval, STRING_SIZE,
					pcalc->rpcl);
			if (stat) {
				pcalc->val = -1;
				strcpy(pcalc->sval,"***ERROR***");
				recGblSetSevr(pcalc,CALC_ALARM,INVALID_ALARM);
			} else {
				pcalc->udf = FALSE;
			}
		}

		/* Check VAL against limits */
	    checkAlarms(pcalc);

		/* check for output link execution */
		switch (pcalc->oopt) {
		case scalcoutOOPT_Every_Time:
			doOutput = 1;
			break;
		case scalcoutOOPT_On_Change:
			if (fabs(pcalc->pval - pcalc->val) > pcalc->mdel) doOutput = 1;
			break;
		case scalcoutOOPT_Transition_To_Zero:
			if ((pcalc->pval != 0) && (pcalc->val == 0)) doOutput = 1;
			break;         
		case scalcoutOOPT_Transition_To_Non_zero:
			if ((pcalc->pval == 0) && (pcalc->val != 0)) doOutput = 1;
			break;
		case scalcoutOOPT_When_Zero:
			if (!pcalc->val) doOutput = 1;
			break;
		case scalcoutOOPT_When_Non_zero:
			if (pcalc->val) doOutput = 1;
			break;
		case scalcoutOOPT_Never:
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
				if (sCalcoutRecordDebug)
					printf("sCalcoutRecord(%s):process: exit, wait for delay\n",
						pcalc->name);
				return(0);
			} else {
                pcalc->pact = FALSE;
				execOutput(pcalc);
                if (pcalc->pact) {
					if (sCalcoutRecordDebug)
						printf("sCalcoutRecord(%s):process: exit, pact==1\n",
							pcalc->name);
					return(0);
				}
				pcalc->pact = TRUE;
			}
		}
	} else { /* pact == TRUE */
		/* Who invoked us ? */
		if (pcalc->dlya) {
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
    monitor(pcalc);
    recGblFwdLink(pcalc);
    pcalc->pact = FALSE;
	if (sCalcoutRecordDebug) printf("sCalcoutRecord(%s):process: exit, pact==0\n",
		pcalc->name);
	return(0);
}

static long special(dbAddr	*paddr, int after)
{
	scalcoutRecord	*pcalc = (scalcoutRecord *)(paddr->precord);
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
	case scalcoutRecordCALC:
		pcalc->clcv = sCalcPostfix(pcalc->calc, pcalc->rpcl, &error_number);
		if (pcalc->clcv) {
			recGblRecordError(S_db_badField,(void *)pcalc,
				"scalcout: special(): Illegal CALC field");
			printf("sCalcPostfix returns: %d\n", error_number);
		}
		db_post_events(pcalc,&pcalc->clcv,DBE_VALUE);
		return(0);

	case scalcoutRecordOCAL:
		pcalc->oclv = sCalcPostfix(pcalc->ocal, pcalc->orpc, &error_number);
		if (pcalc->oclv) {
			recGblRecordError(S_db_badField,(void *)pcalc,
				"scalcout: special(): Illegal OCAL field");
			printf("sCalcPostfix returns: %d\n", error_number);
		}
		db_post_events(pcalc,&pcalc->oclv,DBE_VALUE);
		return(0);

	case(scalcoutRecordINPA):
	case(scalcoutRecordINPB):
	case(scalcoutRecordINPC):
	case(scalcoutRecordINPD):
	case(scalcoutRecordINPE):
	case(scalcoutRecordINPF):
	case(scalcoutRecordINPG):
	case(scalcoutRecordINPH):
	case(scalcoutRecordINPI):
	case(scalcoutRecordINPJ):
	case(scalcoutRecordINPK):
	case(scalcoutRecordINPL):
	case(scalcoutRecordINAA):
	case(scalcoutRecordINBB):
	case(scalcoutRecordINCC):
	case(scalcoutRecordINDD):
	case(scalcoutRecordINEE):
	case(scalcoutRecordINFF):
	case(scalcoutRecordINGG):
	case(scalcoutRecordINHH):
	case(scalcoutRecordINII):
	case(scalcoutRecordINJJ):
	case(scalcoutRecordINKK):
	case(scalcoutRecordINLL):
	case(scalcoutRecordOUT):
		lnkIndex = fieldIndex - scalcoutRecordINPA;
		plink   = &pcalc->inpa + lnkIndex;
		pvalue  = &pcalc->a    + lnkIndex;
		plinkValid = &pcalc->inav + lnkIndex;

		if (plink->type == CONSTANT) {
			if (fieldIndex <= scalcoutRecordINPL) {
				recGblInitConstantLink(plink,DBF_DOUBLE,pvalue);
				db_post_events(pcalc,pvalue,DBE_VALUE);
			}
			*plinkValid = scalcoutINAV_CON;
			if (fieldIndex == scalcoutRecordOUT)
				prpvt->outlink_field_type = DBF_NOACCESS;
		} else if (!dbNameToAddr(plink->value.pv_link.pvname, pAddr)) {
			short pvlMask = plink->value.pv_link.pvlMask;
			short isCA = pvlMask & (pvlOptCA|pvlOptCP|pvlOptCPP);

			/* PV resides on this ioc */
			if ((fieldIndex <= scalcoutRecordINPL) || (fieldIndex > scalcoutRecordINLL) || !isCA) {
				/* Not a string input link or not a CA (type) link */
				*plinkValid = scalcoutINAV_LOC;
			} else {
				/*
				 * string link of type CA.  We need to check the return from dbGetLink(),
				 * because it may fail the first time we call it.  If we're connected to
				 * a DBF_ENUM PV, dbGetLink() won't succeed until it gets the enum-string
				 * values, which it needs to convert numbers to strings.
				 */
				/* lie: say the PV is not connected and from another ioc so we'll check it */
				*plinkValid = scalcoutINAV_EXT_NC;
				if (!prpvt->wd_id_1_LOCK) {
					callbackRequestDelayed(&prpvt->checkLinkCb,.5);
					prpvt->wd_id_1_LOCK = 1;
					prpvt->caLinkStat = CA_LINKS_NOT_OK;
				}
			}
			if (fieldIndex == scalcoutRecordOUT) {
				prpvt->outlink_field_type = pAddr->field_type;
				if ((pAddr->field_type >= DBF_INLINK) && (pAddr->field_type <= DBF_FWDLINK)) {
					if (!(plink->value.pv_link.pvlMask & pvlOptCA)) {
						printf("sCalcoutRecord(%s):special:non-CA link to link field\n",
							plink->value.pv_link.pvname);
					}
				}
				if (pcalc->wait && !(plink->value.pv_link.pvlMask & pvlOptCA)) {
					printf("sCalcoutRecord(%s):special: Can't wait with non-CA link attribute\n",
						plink->value.pv_link.pvname);
				}
			}
		} else {
			/* pv is not on this ioc. Callback later for connection stat */
			*plinkValid = scalcoutINAV_EXT_NC;
			/* DO_CALLBACK, if not already scheduled */
			if (!prpvt->wd_id_1_LOCK) {
				callbackRequestDelayed(&prpvt->checkLinkCb,.5);
				prpvt->wd_id_1_LOCK = 1;
				prpvt->caLinkStat = CA_LINKS_NOT_OK;
			}
			if (fieldIndex == scalcoutRecordOUT)
				prpvt->outlink_field_type = DBF_NOACCESS; /* don't know */
		}
        db_post_events(pcalc,plinkValid,DBE_VALUE);
		return(0);

	default:
		recGblDbaddrError(S_db_badChoice,paddr,"calc: special");
		return(S_db_badChoice);
	}
	return(0);
}

static long cvt_dbaddr(dbAddr *paddr)
{
	scalcoutRecord	*pcalc = (scalcoutRecord *) paddr->precord;
	char			**pfield = (char **)paddr->pfield;
	char			**paa = (char **)&(pcalc->paa);
	short			i;
    int fieldIndex = dbGetFieldIndex(paddr);

	if (sCalcoutRecordDebug > 5) printf("sCalcout: cvt_dbaddr: paddr->pfield = %p\n",
		(void *)paddr->pfield);
	if ((fieldIndex>=scalcoutRecordPAA) && (fieldIndex<=scalcoutRecordPLL)) {
		i = pfield - paa;
		paddr->pfield = paa[i];
		paddr->no_elements = STRING_SIZE;
	}
	paddr->field_type = DBF_STRING;
	paddr->field_size = STRING_SIZE;
	paddr->dbr_field_type = DBR_STRING;
	return(0);
}

static long get_units(dbAddr *paddr, char *units)
{
	scalcoutRecord	*pcalc=(scalcoutRecord *)paddr->precord;

	strncpy(units,pcalc->egu,DB_UNITS_SIZE);
	return(0);
}

static long get_precision(dbAddr *paddr, long *precision)
{
	scalcoutRecord	*pcalc=(scalcoutRecord *)paddr->precord;
	int fieldIndex = dbGetFieldIndex(paddr);

	*precision = pcalc->prec;
	if (fieldIndex == scalcoutRecordVAL) return(0);
	recGblGetPrec(paddr,precision);
	return(0);
}

static long get_graphic_double(dbAddr *paddr, struct dbr_grDouble *pgd)
{
    scalcoutRecord	*pcalc=(scalcoutRecord *)paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);

	switch (fieldIndex) {
	case scalcoutRecordVAL:
	case scalcoutRecordHIHI:
	case scalcoutRecordHIGH:
	case scalcoutRecordLOW:
	case scalcoutRecordLOLO:
		pgd->upper_disp_limit = pcalc->hopr;
		pgd->lower_disp_limit = pcalc->lopr;
		return(0);
	default:
		break;
	} 

	if (fieldIndex >= scalcoutRecordA && fieldIndex <= scalcoutRecordL) {
		pgd->upper_disp_limit = pcalc->hopr;
		pgd->lower_disp_limit = pcalc->lopr;
		return(0);
	}
	if (fieldIndex >= scalcoutRecordPA && fieldIndex <= scalcoutRecordPL) {
		pgd->upper_disp_limit = pcalc->hopr;
		pgd->lower_disp_limit = pcalc->lopr;
		return(0);
	}
	return(0);
}

static long get_control_double(dbAddr *paddr, struct dbr_ctrlDouble *pcd)
{
	scalcoutRecord	*pcalc=(scalcoutRecord *)paddr->precord;
	int fieldIndex = dbGetFieldIndex(paddr);

	switch (fieldIndex) {
	case scalcoutRecordVAL:
	case scalcoutRecordHIHI:
	case scalcoutRecordHIGH:
	case scalcoutRecordLOW:
	case scalcoutRecordLOLO:
		pcd->upper_ctrl_limit = pcalc->hopr;
		pcd->lower_ctrl_limit = pcalc->lopr;
		return(0);
	default:
		break;
    } 

	if (fieldIndex >= scalcoutRecordA && fieldIndex <= scalcoutRecordL) {
		pcd->upper_ctrl_limit = pcalc->hopr;
		pcd->lower_ctrl_limit = pcalc->lopr;
		return(0);
	}
	if (fieldIndex >= scalcoutRecordPA && fieldIndex <= scalcoutRecordPL) {
		pcd->upper_ctrl_limit = pcalc->hopr;
		pcd->lower_ctrl_limit = pcalc->lopr;
		return(0);
	}
	return(0);
}
static long get_alarm_double(dbAddr *paddr, struct dbr_alDouble *pad)
{
	scalcoutRecord	*pcalc=(scalcoutRecord *)paddr->precord;
	int fieldIndex = dbGetFieldIndex(paddr);

	if (fieldIndex == scalcoutRecordVAL) {
		pad->upper_alarm_limit = pcalc->hihi;
		pad->upper_warning_limit = pcalc->high;
		pad->lower_warning_limit = pcalc->low;
		pad->lower_alarm_limit = pcalc->lolo;
	} else
		 recGblGetAlarmDouble(paddr,pad);
	return(0);
}


static void checkAlarms(scalcoutRecord *pcalc)
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


static void execOutput(scalcoutRecord *pcalc)
{
	long	status;

	/* Determine output data */
	switch (pcalc->dopt) {
	case scalcoutDOPT_Use_VAL:
		pcalc->oval = pcalc->val;
		strcpy(pcalc->osv, pcalc->sval);
		break;

	case scalcoutDOPT_Use_OVAL:
		if (sCalcPerform(&pcalc->a, MAX_FIELDS, (char **)(pcalc->strs),
				STRING_MAX_FIELDS, &pcalc->oval, pcalc->osv, STRING_SIZE,
				pcalc->orpc)) {
			pcalc->val = -1;
			strcpy(pcalc->osv,"***ERROR***");
			recGblSetSevr(pcalc,CALC_ALARM,INVALID_ALARM);
		}
		break;
	}

	/* Check to see what to do if INVALID */
	if (pcalc->nsev < INVALID_ALARM) {
		/* Output the value */
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
				"scalcout:process Illegal IVOA field");
		}
	} 
}

static void monitor(scalcoutRecord *pcalc)
{
	unsigned short	monitor_mask;
	double			delta;
	double			*pnew, *pprev;
	char			**psnew, **psprev;
	int				i;

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

	if (strcmp(pcalc->sval, pcalc->psvl)) {
		db_post_events(pcalc, pcalc->sval, monitor_mask|DBE_VALUE|DBE_LOG);
		strcpy(pcalc->psvl, pcalc->sval);
	}
	if (strcmp(pcalc->osv, pcalc->posv)) {
		db_post_events(pcalc, pcalc->osv, monitor_mask|DBE_VALUE|DBE_LOG);
		strcpy(pcalc->posv, pcalc->osv);
	}

	/* check all input fields for changes */
	for (i=0, pnew=&pcalc->a, pprev=&pcalc->pa; i<MAX_FIELDS;  i++, pnew++, pprev++) {
		if ((*pnew != *pprev) || (monitor_mask&DBE_ALARM)) {
			db_post_events(pcalc,pnew,monitor_mask|DBE_VALUE|DBE_LOG);
		}
	}
	for (i=0, psnew=pcalc->strs, psprev=&pcalc->paa; i<STRING_MAX_FIELDS;
			i++, psnew++, psprev++) {
		if (strcmp(*psnew, *psprev)) {
			db_post_events(pcalc, *psnew, monitor_mask|DBE_VALUE|DBE_LOG);
		}
	}
	/* Check OVAL field */
	if (pcalc->povl != pcalc->oval) {
		db_post_events(pcalc,&pcalc->oval, monitor_mask|DBE_VALUE|DBE_LOG);
		pcalc->povl = pcalc->oval;
	}
	return;
}

static int fetch_values(scalcoutRecord *pcalc)
{
	DBLINK	*plink;	/* structure of the link field  */
	double	*pvalue;
	char	**psvalue, tmpstr[STRING_SIZE];
	long	status=0, nelm=1;
	int		i, j;
	short	field_type = 0;
	dbAddr	Addr;
	dbAddr	*pAddr = &Addr;

	for (i=0, plink=&pcalc->inpa, pvalue=&pcalc->a; i<MAX_FIELDS; 
			i++, plink++, pvalue++) {
		status = dbGetLink(plink, DBR_DOUBLE, pvalue, 0, 0);
		if (!RTN_SUCCESS(status)) return(status);
	}

	for (i=0, plink=&pcalc->inaa, psvalue=pcalc->strs; i<STRING_MAX_FIELDS; 
			i++, plink++, psvalue++) {
		status = 0;
		field_type = 0;
		nelm = 1;
		switch (plink->type) {
		case CA_LINK:
			field_type = dbCaGetLinkDBFtype(plink);
			dbCaGetNelements(plink, &nelm);
			break;
		case DB_LINK:
			if (!dbNameToAddr(plink->value.pv_link.pvname, pAddr)) {
				field_type = pAddr->field_type;
				nelm = pAddr->no_elements;
			} else {
				field_type = DBR_STRING;
				nelm = 1;
			}
			break;
		default:
			break;
		}
		if ((plink->type==CA_LINK) || (plink->type==DB_LINK)) {
			if (nelm > STRING_SIZE-1) nelm = STRING_SIZE-1;
			if (((field_type==DBR_CHAR) || (field_type==DBR_UCHAR)) && nelm>1) {
				for (j=0; j<STRING_SIZE; j++) (*psvalue)[j]='\0';
				status = dbGetLink(plink, field_type, tmpstr, 0, &nelm);
				if (sCalcoutRecordDebug > 1)
					printf("fetch_values('%s'): dbGetLink(%d) field_type %d, returned %ld\n", pcalc->name, i, field_type, status);
				if (nelm>0) {
					epicsStrSnPrintEscaped(*psvalue, STRING_SIZE-1, tmpstr, nelm);
					(*psvalue)[STRING_SIZE-1] = '\0';
				} else {
					(*psvalue)[0] = '\0';
				}
			} else {
				status = dbGetLink(plink, DBR_STRING, *psvalue, 0, 0);
				if (sCalcoutRecordDebug > 1)
					printf("fetch_values('%s'): dbGetLink(%d) DBR_STRING, returned %ld\n", pcalc->name, i, status);
			}
		}
		if (!RTN_SUCCESS(status)) {
			epicsSnprintf(*psvalue, STRING_SIZE-1, "%s:fetch(%s) failed", pcalc->name, sFldnames[i]);
		}
	}
	return(0);
}

static void checkLinksCallback(CALLBACK *pcallback)
{
    scalcoutRecord	*pcalc;
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


static void checkLinks(scalcoutRecord *pcalc)
{
	DBLINK			*plink;
	rpvtStruct		*prpvt = (rpvtStruct *)pcalc->rpvt;
	int i;
	int				isCaLink   = 0;
	int				isCaLinkNc = 0;
	unsigned short	*plinkValid;
	dbAddr			Addr;
	dbAddr			*pAddr = &Addr;
	char 			tmpstr[100];
	int isString, linkWorks;

	if (sCalcoutRecordDebug) printf("checkLinks() for %s\n", pcalc->name);

	plink   = &pcalc->inpa;
	plinkValid = &pcalc->inav;

	for (i=0; i<MAX_FIELDS+STRING_MAX_FIELDS+1; i++, plink++, plinkValid++) {
		if (plink->type == CA_LINK) {
			isCaLink = 1;

			/* See if link is fully functional. (CA link to ENUM must wait for enum strings.) */
			isString = 0;
			linkWorks = 0;
			if (dbCaIsLinkConnected(plink)) {
				if (i >= MAX_FIELDS && i < MAX_FIELDS+STRING_MAX_FIELDS) {
					/* this is a string link, do a trial dbGetLink() */
					long	status;
					isString = 1;
					status = dbGetLink(plink, DBR_STRING, tmpstr, 0, 0);
					if (RTN_SUCCESS(status)) {
						linkWorks = 1;
					} else {
						if (sCalcoutRecordDebug)
							printf("checkLinks: dbGetLink returned %ld\n", status);
					}
				}
			}

			if (dbCaIsLinkConnected(plink) && (isString == linkWorks)) {
				if (*plinkValid == scalcoutINAV_EXT_NC) {
					if (!dbNameToAddr(plink->value.pv_link.pvname, pAddr)) {
						/* PV resides on this ioc */
						*plinkValid = scalcoutINAV_LOC;
					} else {
						*plinkValid = scalcoutINAV_EXT;
					}
					db_post_events(pcalc,plinkValid,DBE_VALUE);
				}
				/* If this is the outlink, get the type of field it's connected to.  If it's connected
				 * to a link field, and the outlink is not a CA link, complain, because this won't work.
				 * Also, if .WAIT, then the link must be a CA link.
				 */
				if (plink == &pcalc->out) {
					prpvt->outlink_field_type = dbCaGetLinkDBFtype(plink);
					if (sCalcoutRecordDebug)
						printf("sCalcout:checkLinks: outlink type = %d\n", prpvt->outlink_field_type);
					if (!dbNameToAddr(plink->value.pv_link.pvname, pAddr)) {
						if ((pAddr->field_type >= DBF_INLINK) &&
								(pAddr->field_type <= DBF_FWDLINK)) {
							if (!(plink->value.pv_link.pvlMask & pvlOptCA)) {
								printf("sCalcoutRecord(%s):checkLinks:non-CA link to link field\n",
									plink->value.pv_link.pvname);
							}
						}
					}
					if (pcalc->wait && !(plink->value.pv_link.pvlMask & pvlOptCA)) {
						printf("sCalcoutRecord(%s):checkLinks: Can't wait with non-CA link attribute\n",
							plink->value.pv_link.pvname);
					}
				}
			} else {
				if (*plinkValid == scalcoutINAV_EXT_NC) {
					isCaLinkNc = 1;
				}
				else if (*plinkValid == scalcoutINAV_EXT) {
					*plinkValid = scalcoutINAV_EXT_NC;
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


static long writeValue(scalcoutRecord *pcalc)
{
    scalcoutDSET	*pscalcoutDSET = (scalcoutDSET *)pcalc->dset;

    if (!pscalcoutDSET || !pscalcoutDSET->write) {
        errlogPrintf("%s DSET write does not exist\n",pcalc->name);
        recGblSetSevr(pcalc,SOFT_ALARM,INVALID_ALARM);
        pcalc->pact = TRUE;
        return(-1);
    }
	return pscalcoutDSET->write(pcalc);
}
