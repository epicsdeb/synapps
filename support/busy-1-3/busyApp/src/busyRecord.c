/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* busyRecord.c */
 
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "alarm.h"
#include "callback.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "devSup.h"
#include "errMdef.h"
#include "recSup.h"
#include "recGbl.h"
#include "special.h"
#define GEN_SIZE_OFFSET
#include "busyRecord.h"
#undef  GEN_SIZE_OFFSET
#include "menuIvoa.h"
#include "menuOmsl.h"
#include "menuYesNo.h"
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(busyRecord *, int);
static long process(busyRecord *);
#define special NULL
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
#define get_units NULL
static long get_precision(DBADDR *, long *);
static long get_enum_str(DBADDR *, char *);
static long get_enum_strs(DBADDR *, struct dbr_enumStrs *);
static long put_enum_str(DBADDR *, char *);
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

rset busyRSET={
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
epicsExportAddress(rset,busyRSET);

struct busydset { /* busyRecord dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;  /*returns:(0,2)=>(success,success no convert*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_busy;/*returns: (-1,0)=>(failure,success)*/
};


/* control block for callback*/
typedef struct myCallback {
        CALLBACK        callback;
        struct dbCommon *precord;
}myCallback;

static void checkAlarms(busyRecord *);
static void monitor(busyRecord *);
static long writeValue(busyRecord *);

static void myCallbackFunc(CALLBACK *arg)
{
    myCallback *pcallback;
    busyRecord *prec;

    callbackGetUser(pcallback,arg);
    prec=(busyRecord *)pcallback->precord;
    dbScanLock((struct dbCommon *)prec);
    if(prec->pact) {
	if((prec->val==1) && (prec->high>0)){
	    myCallback *pcallback;
	    pcallback = (myCallback *)(prec->rpvt);
            callbackSetPriority(prec->prio, &pcallback->callback);
            callbackRequestDelayed(&pcallback->callback,(double)prec->high);
	}
    } else {
	prec->val = 0;
	dbProcess((struct dbCommon *)prec);
    }
    dbScanUnlock((struct dbCommon *)prec);
}

static long init_record(busyRecord *prec, int pass)
{
    struct busydset *pdset;
    long status=0;
    myCallback *pcallback;

    if (pass==0) return(0);

    /* busy.siml must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (prec->siml.type == CONSTANT) {
	recGblInitConstantLink(&prec->siml,DBF_USHORT,&prec->simm);
    }

    if(!(pdset = (struct busydset *)(prec->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)prec,"busy: init_record");
	return(S_dev_noDSET);
    }
    /* must have  write_busy functions defined */
    if( (pdset->number < 5) || (pdset->write_busy == NULL) ) {
	recGblRecordError(S_dev_missingSup,(void *)prec,"busy: init_record");
	return(S_dev_missingSup);
    }
    /* get the initial value */
    if (prec->dol.type == CONSTANT) {
	unsigned short ival = 0;

	if(recGblInitConstantLink(&prec->dol,DBF_USHORT,&ival)) {
	    if (ival  == 0)  prec->val = 0;
	    else  prec->val = 1;
	    prec->udf = FALSE;
	}
    }

    pcallback = (myCallback *)(calloc(1,sizeof(myCallback)));
    prec->rpvt = (void *)pcallback;
    callbackSetCallback(myCallbackFunc,&pcallback->callback);
    callbackSetUser(pcallback,&pcallback->callback);
    pcallback->precord = (struct dbCommon *)prec;

    if( pdset->init_record ) {
	status=(*pdset->init_record)(prec);
	if(status==0) {
		if(prec->rval==0) prec->val = 0;
		else prec->val = 1;
		prec->udf = FALSE;
	} else if (status==2) status=0;
    }
    /* convert val to rval */
    if ( prec->mask != 0 ) {
	if(prec->val==0) prec->rval = 0;
	else prec->rval = prec->mask;
    } else prec->rval = (epicsUInt32)prec->val;
    return(status);
}

static long process(busyRecord *prec)
{
	struct busydset	*pdset = (struct busydset *)(prec->dset);
	long		 status=0;
	unsigned char    pact=prec->pact;

	if( (pdset==NULL) || (pdset->write_busy==NULL) ) {
		prec->pact=TRUE;
		recGblRecordError(S_dev_missingSup,(void *)prec,"write_busy");
		return(S_dev_missingSup);
	}
        if (!prec->pact) {
		if ((prec->dol.type != CONSTANT) && (prec->omsl == menuOmslclosed_loop)){
			unsigned short val;

			prec->pact = TRUE;
			status=dbGetLink(&prec->dol,DBR_USHORT, &val,0,0);
			prec->pact = FALSE;
			if(status==0){
				prec->val = val;
				prec->udf = FALSE;
			}else {
       				recGblSetSevr(prec,LINK_ALARM,INVALID_ALARM);
			}
		}

		/* convert val to rval */
		if ( prec->mask != 0 ) {
			if(prec->val==0) prec->rval = 0;
			else prec->rval = prec->mask;
		} else prec->rval = (epicsUInt32)prec->val;
	}

	/* check for alarms */
	checkAlarms(prec);

        if (prec->nsev < INVALID_ALARM )
                status=writeValue(prec); /* write the new value */
        else {
                switch (prec->ivoa) {
                    case (menuIvoaContinue_normally) :
                        status=writeValue(prec); /* write the new value */
                        break;
                    case (menuIvoaDon_t_drive_outputs) :
                        break;
                    case (menuIvoaSet_output_to_IVOV) :
                        if(prec->pact == FALSE){
				/* convert val to rval */
                                prec->val=prec->ivov;
				if ( prec->mask != 0 ) {
					if(prec->val==0) prec->rval = 0;
					else prec->rval = prec->mask;
				} else prec->rval = (epicsUInt32)prec->val;
			}
                        status=writeValue(prec); /* write the new value */
                        break;
                    default :
                        status=-1;
                        recGblRecordError(S_db_badField,(void *)prec,
                                "busy:process Illegal IVOA field");
                }
        }

	/* check if device support set pact */
	if ( !pact && prec->pact ) return(0);
	prec->pact = TRUE;

	recGblGetTimeStamp(prec);
	if((prec->val==1) && (prec->high>0)){
	    myCallback *pcallback;
	    pcallback = (myCallback *)(prec->rpvt);
            callbackSetPriority(prec->prio, &pcallback->callback);
            callbackRequestDelayed(&pcallback->callback,(double)prec->high);
	}
	/* check event list */
	monitor(prec);
	/* process the forward scan link record */
	if (prec->val == 0) recGblFwdLink(prec);

	prec->pact=FALSE;
	return(status);
}

static long get_precision(DBADDR *paddr, long *precision)
{
    busyRecord	*prec=(busyRecord *)paddr->precord;

    if(paddr->pfield == (void *)&prec->high) *precision=2;
    else recGblGetPrec(paddr,precision);
    return(0);
}

static long get_enum_str(DBADDR *paddr, char *pstring)
{
    busyRecord	*prec=(busyRecord *)paddr->precord;
    int                 index;
    unsigned short      *pfield = (unsigned short *)paddr->pfield;


    index = dbGetFieldIndex(paddr);
    if(index!=busyRecordVAL) {
	strcpy(pstring,"Illegal_Value");
    } else if(*pfield==0) {
	strncpy(pstring,prec->znam,sizeof(prec->znam));
	pstring[sizeof(prec->znam)] = 0;
    } else if(*pfield==1) {
	strncpy(pstring,prec->onam,sizeof(prec->onam));
	pstring[sizeof(prec->onam)] = 0;
    } else {
	strcpy(pstring,"Illegal_Value");
    }
    return(0);
}

static long get_enum_strs(DBADDR *paddr,struct dbr_enumStrs *pes)
{
    busyRecord	*prec=(busyRecord *)paddr->precord;

    /*SETTING no_str=0 breaks channel access clients*/
    pes->no_str = 2;
    memset(pes->strs,'\0',sizeof(pes->strs));
    strncpy(pes->strs[0],prec->znam,sizeof(prec->znam));
    if(*prec->znam!=0) pes->no_str=1;
    strncpy(pes->strs[1],prec->onam,sizeof(prec->onam));
    if(*prec->onam!=0) pes->no_str=2;
    return(0);
}
static long put_enum_str(DBADDR *paddr, char *pstring)
{
    busyRecord     *prec=(busyRecord *)paddr->precord;

    if(strncmp(pstring,prec->znam,sizeof(prec->znam))==0) prec->val = 0;
    else  if(strncmp(pstring,prec->onam,sizeof(prec->onam))==0) prec->val = 1;
    else return(S_db_badChoice);
    return(0);
}


static void checkAlarms(busyRecord *prec)
{
	unsigned short val = prec->val;

        /* check for udf alarm */
        if(prec->udf == TRUE ){
			recGblSetSevr(prec,UDF_ALARM,INVALID_ALARM);
        }

        /* check for  state alarm */
        if (val == 0){
		recGblSetSevr(prec,STATE_ALARM,prec->zsv);
        }else{
		recGblSetSevr(prec,STATE_ALARM,prec->osv);
        }

        /* check for cos alarm */
	if(val == prec->lalm) return;
	recGblSetSevr(prec,COS_ALARM,prec->cosv);
	prec->lalm = val;
        return;
}

static void monitor(busyRecord *prec)
{
	unsigned short	monitor_mask;

        monitor_mask = recGblResetAlarms(prec);
        /* check for value change */
        if (prec->mlst != prec->val){
                /* post events for value change and archive change */
                monitor_mask |= (DBE_VALUE | DBE_LOG);
                /* update last value monitored */
                prec->mlst = prec->val;
        }

        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(prec,&prec->val,monitor_mask);
        }
	if(prec->oraw!=prec->rval) {
		db_post_events(prec,&prec->rval,
		    monitor_mask|DBE_VALUE|DBE_LOG);
		prec->oraw = prec->rval;
	}
	if(prec->orbv!=prec->rbv) {
		db_post_events(prec,&prec->rbv,
		    monitor_mask|DBE_VALUE|DBE_LOG);
		prec->orbv = prec->rbv;
	}
        return;
}

static long writeValue(busyRecord *prec)
{
	long		status;
        struct busydset 	*pdset = (struct busydset *) (prec->dset);

	if (prec->pact == TRUE){
		status=(*pdset->write_busy)(prec);
		return(status);
	}

	status=dbGetLink(&prec->siml,DBR_USHORT, &prec->simm,0,0);
	if (status)
		return(status);

	if (prec->simm == menuYesNoNO){
		status=(*pdset->write_busy)(prec);
		return(status);
	}
	if (prec->simm == menuYesNoYES){
		status=dbPutLink(&(prec->siol),DBR_USHORT, &(prec->val),1);
	} else {
		status=-1;
		recGblSetSevr(prec,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(prec,SIMM_ALARM,prec->sims);

	return(status);
}
