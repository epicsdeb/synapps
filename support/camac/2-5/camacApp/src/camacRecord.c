/* camacRecord.c - Record Support Routines for Generic CAMAC record */
/*
 *      Author: 	Mark Rivers
 *      Date:   	4/19/95
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
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
 * .01  06-03-97 tmm  Conversion to EPICS 3.13
 */


#include        <vxWorks.h>
#include        <types.h>
#include        <stdlib.h>
#include        <stdioLib.h>
#include        <lstLib.h>
#include        <string.h>

#include        <alarm.h>
#include        <dbDefs.h>
#include        <dbAccess.h>
#include        <dbFldTypes.h>
#include        <dbEvent.h>
#include        <devSup.h>
#include        <errMdef.h>
#include        <recSup.h>
#include        <recGbl.h>
#include        <epicsExport.h>
#include 	<camacLib.h>
#define GEN_SIZE_OFFSET
#include        <camacRecord.h>
#undef GEN_SIZE_OFFSET

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
#define special NULL
#define get_value NULL
static long cvt_dbaddr();
static long get_array_info();
static long put_array_info();
#define get_units NULL
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

/* Define macro to save previous value of a field */
#define SAVE_PREV(A) \
	prev_values.A = pcamac->A;
	
/* Define macro to post event if value has changed */
#define POST_IF_NEW(A) \
	if (pcamac->A != prev_values->A) {\
		db_post_events(pcamac,&pcamac->A,monitor_mask); \
	}

struct rset camacRSET={
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
	get_alarm_double };
epicsExportAddress(rset, camacRSET);

struct prev_values {
	int	q;
	int	x;
	int	nact;
	int	ccmd;
	int	inhb;
	int	nuse;
};

static void monitor(struct camacRecord*, struct prev_values*);
static void doCamacIo(struct camacRecord*);


static long init_record(pcamac, pass)
	struct camacRecord     *pcamac;
	int pass;
{
	if (pass==0) {
		if (pcamac->nmax <= 0) pcamac->nmax=1;
		pcamac->bptr = (char *)calloc(pcamac->nmax, sizeof(long));
		pcamac->nact = 0;
		return(OK);
	}
	return(OK);
}


static long process(pcamac)
	struct camacRecord     *pcamac;
{
	struct prev_values	prev_values;

        SAVE_PREV(q);
        SAVE_PREV(x);
        SAVE_PREV(nact);
        SAVE_PREV(ccmd);
        SAVE_PREV(inhb);
        SAVE_PREV(nuse);

        doCamacIo(pcamac); /* Do CAMAC transfer */

	recGblGetTimeStamp(pcamac);

	/* check event list */
	monitor(pcamac, &prev_values);
	/* process the forward scan link record */
	recGblFwdLink(pcamac);

	pcamac->pact=FALSE;
	return(OK);
}


static long cvt_dbaddr(paddr)
struct dbAddr *paddr;
{
	struct camacRecord *pcamac=(struct camacRecord *)paddr->precord;

	paddr->pfield = (void *)(pcamac->bptr);
	paddr->no_elements = pcamac->nmax;
	paddr->field_type = DBF_LONG;
	paddr->field_size = sizeof(long);
	paddr->dbr_field_type = DBF_LONG;
	return(OK);
}

static long get_array_info(paddr,no_elements,offset)
struct dbAddr *paddr;
long *no_elements;
long *offset;
{
	struct camacRecord *pcamac=(struct camacRecord *)paddr->precord;

	*no_elements =  pcamac->nact;
	*offset = 0;
	return(OK);
}

static long put_array_info(paddr,nNew)
struct dbAddr *paddr;
long nNew;
{
struct camacRecord *pcamac=(struct camacRecord *)paddr->precord;

	pcamac->nact = nNew;
	if (pcamac->nact > pcamac->nmax) pcamac->nact = pcamac->nmax;
	return(OK);
}


static void monitor(pcamac, prev_values)
    struct camacRecord	*pcamac;
    struct prev_values 	*prev_values;
{
	unsigned short		monitor_mask;

	/* get previous stat and sevr and new stat and sevr*/
        monitor_mask = recGblResetAlarms(pcamac);
        monitor_mask |= (DBE_LOG|DBE_VALUE);
        /* Always post monitor on VAL field, since it can be big array
         * and we don't want to check each value to see if it changed
         */
        db_post_events(pcamac,pcamac->bptr,monitor_mask);
        /* On all of the other fields, only post monitors if they changed */
        POST_IF_NEW(q);
        POST_IF_NEW(x);
        POST_IF_NEW(nact);
        POST_IF_NEW(ccmd);
        POST_IF_NEW(inhb);
        POST_IF_NEW(nuse);
	return;
}

static void doCamacIo(pcamac)
	struct camacRecord	*pcamac;
{
	int ext, extb[2], status, value, q, cb[4]={0,0,0,0}, inhb;

	/* Make sure nuse <= nmax */
	if (pcamac->nuse > pcamac->nmax) pcamac->nuse = pcamac->nmax;
	
	/* Should we perform a crate control function ? */
	if (pcamac->ccmd) {
	  cdreg(&ext, pcamac->b, pcamac->c, 1, 0);
	  switch(pcamac->ccmd) {
	  case 1:
		/* Clear inhibit */
	  	ccci(ext, 0);
		break;
	  case 2:
		/* Set inhibit */
	  	ccci(ext, 1);
		break;
	  case 3:
		/* Clear (C) */
	  	cccc(ext);
		break;
	  case 4:
		/* Initialize (Z) */
	  	cccz(ext);
		break;
	  }
	  ctstat(&status);
	  /* Set the crate command back to none */
	  pcamac->ccmd = 0;
	} else {
	
	cdreg(&ext, pcamac->b, pcamac->c, pcamac->n, pcamac->a);
	switch(pcamac->tmod) {
	case camacTMOD_Single:
		/* Single cycle */
		if ((pcamac->f >= 16) && (pcamac->f <= 23)) 
			value=*(long *)pcamac->bptr;
		cfsa(pcamac->f, ext, &value, &q);
		if (pcamac->f <= 7) *(long *)pcamac->bptr = value;
		pcamac->nact = 1;
		break;
	case camacTMOD_Q_Stop:
		/* QSTP transfer */
		cb[0]=pcamac->nuse;
		cfubc(pcamac->f, ext, pcamac->bptr, cb);
		pcamac->nact = cb[1];
		break;	
	case camacTMOD_Q_Repeat:
		/* QRPT transfer */
		cb[0]=pcamac->nuse;
		cfubr(pcamac->f, ext, pcamac->bptr, cb);
		pcamac->nact = cb[1];
		break;	
	case camacTMOD_Q_Scan:
		/* QSCN transfer */
		/* Routine cfmad needs a 2 element extb[] array. 	*/
		/* extb[0] is the first bcna, extb[1] is the last bcna 	*/
		/* Some hardware only uses the first value (e.g. KSC 3922) */
		/* so we supply the same value for both 		*/
		cdreg(&extb[0], pcamac->b, pcamac->c, pcamac->n, pcamac->a);
		cdreg(&extb[1], pcamac->b, pcamac->c, pcamac->n, pcamac->a);
		cb[0]=pcamac->nuse;
		cfmad(pcamac->f, extb, pcamac->bptr, cb);
		pcamac->nact = cb[1];
		break;	
	}
        }
        /*
         * Raise alarms. We are using the actual error codes of camacLib, not
	 * just the X and Q response portions of those.
        */
        ctstat(&status);
        switch (status) {
          case OK:
        	break;
          case S_camacLib_noX:
          case S_camacLib_noQ_noX:
        	recGblSetSevr(pcamac, COMM_ALARM, MINOR_ALARM);
        	break;
          default:
        	recGblSetSevr(pcamac, COMM_ALARM, MAJOR_ALARM);
        }
        pcamac->q = Q_STATUS(status);
        pcamac->x = X_STATUS(status);
	/* Check inhibit status */
	ctci(ext, &inhb);
	pcamac->inhb = inhb;
}
