/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
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
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
#define special NULL
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
#define get_units NULL
static long get_precision();
static long get_enum_str();
static long get_enum_strs();
static long put_enum_str();
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

static void checkAlarms();
static void monitor();
static long writeValue();

static void myCallbackFunc(CALLBACK *arg)
{
    myCallback *pcallback;
    struct busyRecord *pbusy;

    callbackGetUser(pcallback,arg);
    pbusy=(struct busyRecord *)pcallback->precord;
    dbScanLock((struct dbCommon *)pbusy);
    if(pbusy->pact) {
	if((pbusy->val==1) && (pbusy->high>0)){
	    myCallback *pcallback;
	    pcallback = (myCallback *)(pbusy->rpvt);
            callbackSetPriority(pbusy->prio, &pcallback->callback);
            callbackRequestDelayed(&pcallback->callback,(double)pbusy->high);
	}
    } else {
	pbusy->val = 0;
	dbProcess((struct dbCommon *)pbusy);
    }
    dbScanUnlock((struct dbCommon *)pbusy);
}

static long init_record(pbusy,pass)
    struct busyRecord	*pbusy;
    int pass;
{
    struct busydset *pdset;
    long status=0;
    myCallback *pcallback;

    if (pass==0) return(0);

    /* busy.siml must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (pbusy->siml.type == CONSTANT) {
	recGblInitConstantLink(&pbusy->siml,DBF_USHORT,&pbusy->simm);
    }

    if(!(pdset = (struct busydset *)(pbusy->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)pbusy,"busy: init_record");
	return(S_dev_noDSET);
    }
    /* must have  write_busy functions defined */
    if( (pdset->number < 5) || (pdset->write_busy == NULL) ) {
	recGblRecordError(S_dev_missingSup,(void *)pbusy,"busy: init_record");
	return(S_dev_missingSup);
    }
    /* get the initial value */
    if (pbusy->dol.type == CONSTANT) {
	unsigned short ival = 0;

	if(recGblInitConstantLink(&pbusy->dol,DBF_USHORT,&ival)) {
	    if (ival  == 0)  pbusy->val = 0;
	    else  pbusy->val = 1;
	    pbusy->udf = FALSE;
	}
    }

    pcallback = (myCallback *)(calloc(1,sizeof(myCallback)));
    pbusy->rpvt = (void *)pcallback;
    callbackSetCallback(myCallbackFunc,&pcallback->callback);
    callbackSetUser(pcallback,&pcallback->callback);
    pcallback->precord = (struct dbCommon *)pbusy;

    if( pdset->init_record ) {
	status=(*pdset->init_record)(pbusy);
	if(status==0) {
		if(pbusy->rval==0) pbusy->val = 0;
		else pbusy->val = 1;
		pbusy->udf = FALSE;
	} else if (status==2) status=0;
    }
    /* convert val to rval */
    if ( pbusy->mask != 0 ) {
	if(pbusy->val==0) pbusy->rval = 0;
	else pbusy->rval = pbusy->mask;
    } else pbusy->rval = (unsigned long)pbusy->val;
    return(status);
}

static long process(pbusy)
	struct busyRecord     *pbusy;
{
	struct busydset	*pdset = (struct busydset *)(pbusy->dset);
	long		 status=0;
	unsigned char    pact=pbusy->pact;

	if( (pdset==NULL) || (pdset->write_busy==NULL) ) {
		pbusy->pact=TRUE;
		recGblRecordError(S_dev_missingSup,(void *)pbusy,"write_busy");
		return(S_dev_missingSup);
	}
        if (!pbusy->pact) {
		if ((pbusy->dol.type != CONSTANT) && (pbusy->omsl == menuOmslclosed_loop)){
			unsigned short val;

			pbusy->pact = TRUE;
			status=dbGetLink(&pbusy->dol,DBR_USHORT, &val,0,0);
			pbusy->pact = FALSE;
			if(status==0){
				pbusy->val = val;
				pbusy->udf = FALSE;
			}else {
       				recGblSetSevr(pbusy,LINK_ALARM,INVALID_ALARM);
			}
		}

		/* convert val to rval */
		if ( pbusy->mask != 0 ) {
			if(pbusy->val==0) pbusy->rval = 0;
			else pbusy->rval = pbusy->mask;
		} else pbusy->rval = (unsigned long)pbusy->val;
	}

	/* check for alarms */
	checkAlarms(pbusy);

        if (pbusy->nsev < INVALID_ALARM )
                status=writeValue(pbusy); /* write the new value */
        else {
                switch (pbusy->ivoa) {
                    case (menuIvoaContinue_normally) :
                        status=writeValue(pbusy); /* write the new value */
                        break;
                    case (menuIvoaDon_t_drive_outputs) :
                        break;
                    case (menuIvoaSet_output_to_IVOV) :
                        if(pbusy->pact == FALSE){
				/* convert val to rval */
                                pbusy->val=pbusy->ivov;
				if ( pbusy->mask != 0 ) {
					if(pbusy->val==0) pbusy->rval = 0;
					else pbusy->rval = pbusy->mask;
				} else pbusy->rval = (unsigned long)pbusy->val;
			}
                        status=writeValue(pbusy); /* write the new value */
                        break;
                    default :
                        status=-1;
                        recGblRecordError(S_db_badField,(void *)pbusy,
                                "busy:process Illegal IVOA field");
                }
        }

	/* check if device support set pact */
	if ( !pact && pbusy->pact ) return(0);
	pbusy->pact = TRUE;

	recGblGetTimeStamp(pbusy);
	if((pbusy->val==1) && (pbusy->high>0)){
	    myCallback *pcallback;
	    pcallback = (myCallback *)(pbusy->rpvt);
            callbackSetPriority(pbusy->prio, &pcallback->callback);
            callbackRequestDelayed(&pcallback->callback,(double)pbusy->high);
	}
	/* check event list */
	monitor(pbusy);
	/* process the forward scan link record */
	if (pbusy->val == 0) recGblFwdLink(pbusy);

	pbusy->pact=FALSE;
	return(status);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct busyRecord	*pbusy=(struct busyRecord *)paddr->precord;

    if(paddr->pfield == (void *)&pbusy->high) *precision=2;
    else recGblGetPrec(paddr,precision);
    return(0);
}

static long get_enum_str(paddr,pstring)
    struct dbAddr *paddr;
    char	  *pstring;
{
    struct busyRecord	*pbusy=(struct busyRecord *)paddr->precord;
    int                 index;
    unsigned short      *pfield = (unsigned short *)paddr->pfield;


    index = dbGetFieldIndex(paddr);
    if(index!=busyRecordVAL) {
	strcpy(pstring,"Illegal_Value");
    } else if(*pfield==0) {
	strncpy(pstring,pbusy->znam,sizeof(pbusy->znam));
	pstring[sizeof(pbusy->znam)] = 0;
    } else if(*pfield==1) {
	strncpy(pstring,pbusy->onam,sizeof(pbusy->onam));
	pstring[sizeof(pbusy->onam)] = 0;
    } else {
	strcpy(pstring,"Illegal_Value");
    }
    return(0);
}

static long get_enum_strs(paddr,pes)
    struct dbAddr *paddr;
    struct dbr_enumStrs *pes;
{
    struct busyRecord	*pbusy=(struct busyRecord *)paddr->precord;

    /*SETTING no_str=0 breaks channel access clients*/
    pes->no_str = 2;
    memset(pes->strs,'\0',sizeof(pes->strs));
    strncpy(pes->strs[0],pbusy->znam,sizeof(pbusy->znam));
    if(*pbusy->znam!=0) pes->no_str=1;
    strncpy(pes->strs[1],pbusy->onam,sizeof(pbusy->onam));
    if(*pbusy->onam!=0) pes->no_str=2;
    return(0);
}
static long put_enum_str(paddr,pstring)
    struct dbAddr *paddr;
    char          *pstring;
{
    struct busyRecord     *pbusy=(struct busyRecord *)paddr->precord;

    if(strncmp(pstring,pbusy->znam,sizeof(pbusy->znam))==0) pbusy->val = 0;
    else  if(strncmp(pstring,pbusy->onam,sizeof(pbusy->onam))==0) pbusy->val = 1;
    else return(S_db_badChoice);
    return(0);
}


static void checkAlarms(pbusy)
    struct busyRecord	*pbusy;
{
	unsigned short val = pbusy->val;

        /* check for udf alarm */
        if(pbusy->udf == TRUE ){
			recGblSetSevr(pbusy,UDF_ALARM,INVALID_ALARM);
        }

        /* check for  state alarm */
        if (val == 0){
		recGblSetSevr(pbusy,STATE_ALARM,pbusy->zsv);
        }else{
		recGblSetSevr(pbusy,STATE_ALARM,pbusy->osv);
        }

        /* check for cos alarm */
	if(val == pbusy->lalm) return;
	recGblSetSevr(pbusy,COS_ALARM,pbusy->cosv);
	pbusy->lalm = val;
        return;
}

static void monitor(pbusy)
    struct busyRecord	*pbusy;
{
	unsigned short	monitor_mask;

        monitor_mask = recGblResetAlarms(pbusy);
        /* check for value change */
        if (pbusy->mlst != pbusy->val){
                /* post events for value change and archive change */
                monitor_mask |= (DBE_VALUE | DBE_LOG);
                /* update last value monitored */
                pbusy->mlst = pbusy->val;
        }

        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(pbusy,&pbusy->val,monitor_mask);
        }
	if(pbusy->oraw!=pbusy->rval) {
		db_post_events(pbusy,&pbusy->rval,
		    monitor_mask|DBE_VALUE|DBE_LOG);
		pbusy->oraw = pbusy->rval;
	}
	if(pbusy->orbv!=pbusy->rbv) {
		db_post_events(pbusy,&pbusy->rbv,
		    monitor_mask|DBE_VALUE|DBE_LOG);
		pbusy->orbv = pbusy->rbv;
	}
        return;
}

static long writeValue(pbusy)
	struct busyRecord	*pbusy;
{
	long		status;
        struct busydset 	*pdset = (struct busydset *) (pbusy->dset);

	if (pbusy->pact == TRUE){
		status=(*pdset->write_busy)(pbusy);
		return(status);
	}

	status=dbGetLink(&pbusy->siml,DBR_USHORT, &pbusy->simm,0,0);
	if (status)
		return(status);

	if (pbusy->simm == NO){
		status=(*pdset->write_busy)(pbusy);
		return(status);
	}
	if (pbusy->simm == YES){
		status=dbPutLink(&(pbusy->siol),DBR_USHORT, &(pbusy->val),1);
	} else {
		status=-1;
		recGblSetSevr(pbusy,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(pbusy,SIMM_ALARM,pbusy->sims);

	return(status);
}
