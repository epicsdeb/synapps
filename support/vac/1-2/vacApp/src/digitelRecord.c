/*****************************************************************
 *
 *      Author :                     Greg Nawrocki
 *      Date:                        3-8-95
 *
 *      This code is based heavily upon code originally written
 *      by John Winans and modified extensively by Greg Nawrocki
 *      it was originally  meant specifically for the Perkin Elmer
 *      Digitel 500 Ion Pump Controller, but will now function
 *      with either the Digitel 500, or 1500, and all associated
 *      options
 *	It also works with the Gamma One MPC and SPC.
  *
 *****************************************************************
 *                         COPYRIGHT NOTIFICATION
 *****************************************************************

 * THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
 * AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
 * AND IN ALL SOURCE LISTINGS OF THE CODE.

 * (C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO

 * Argonne National Laboratory (ANL), with facilities in the States of
 * Illinois and Idaho, is owned by the United States Government, and
 * operated by the University of Chicago under provision of a contract
 * with the Department of Energy.

 * Portions of this material resulted from work developed under a U.S.
 * Government contract and are subject to the following license:  For
 * a period of five years from March 30, 1993, the Government is
 * granted for itself and others acting on its behalf a pdgd-up,
 * nonexclusive, irrevocable worldwide license in this computer
 * software to reproduce, prepare derivative works, and perform
 * publicly and display publicly.  With the approval of DOE, this
 * period may be renewed for two additional five year periods.
 * Following the expiration of this period or periods, the Government
 * is granted for itself and others acting on its behalf, a pdgd-up,
 * nonexclusive, irrevocable worldwide license in this computer
 * software to reproduce, prepare derivative works, distribute copies
 * to the public, perform publicly and display publicly, and to permit
 * others to do so.

 *****************************************************************
 *                               DISCLAIMER
 *****************************************************************

 * NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
 * THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
 * MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
 * LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
 * USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
 * DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
 * OWNED RIGHTS.

 *****************************************************************
 * LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
 * DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY (708-252-2000).
 *****************************************************************

 * Modification Log:
 * -----------------
 * .01  3-95         gjn     created
 * .02  11-4-96      nda     converted to R3.13 compatibility
 * .03  2-20-01      mr      Added additional field to keep track of errors
 *	Modified by Mohan Ramanathan 8/15/07 for ASYN on R3.14
#include <limits.h>

 *
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <math.h>

#include "alarm.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "dbEvent.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "devSup.h"
#include "errMdef.h"
#include "recSup.h"
#include "special.h"

#include "choiceDigitel.h"
#define GEN_SIZE_OFFSET
#include "digitelRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

#define DIGITEL_MAXHY	1e-3
#define DIGITEL_MINHY	0
#define DIGITEL_MAXSP   1e-3
#define DIGITEL_MINSP	0

int recDigitelsimFlag = 0;
int recDigitelDebug = 0;
/***** recDigitelDebug information *****/
/** recDigitelDebug >= 0 --- initialization information **/
/** recDigitelDebug >= 5 --- simulation mode **/
/** recDigitelDebug >= 10 -- alarm condition **/
/** recDigitelDebug >= 15 -- changed fields due to command output **/
/** recDigitelDebug >= 20 -- alarms that are set **/
/** recDigitelDebug >= 25 -- function calls **/

/* Create RSET - Record Support Entry Table */
#define report NULL
#define initialize NULL
static long init_record();
static long process();
static long special();
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
#define get_units NULL
static long get_precision();
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double();
static long get_control_double();
static long get_alarm_double();
 
rset digitelRSET={
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
epicsExportAddress(rset,digitelRSET);

typedef struct digiteldset {	/* digitel dset */
    long number;
    DEVSUPFUN dev_report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;	/* (-1,0)=>(failure,success) */
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN proc_xact;	/* Always return 2, (don't convert) */
} digiteldset;

static void checkAlarms(digitelRecord *pdg);
static void monitor(digitelRecord *pdg);

/*****************************************************************************
 *
 * Init the record, then call the device support module to let it know that
 * this record intends on using it.
 *
 ******************************************************************************/
static long init_record(void *precord,int pass)
{
    digitelRecord *pdg = (digitelRecord *)precord;
    digiteldset *pdset;
    long status;

    if (recDigitelDebug)
	printf("recDigitel.c: Digitel 500 record initialization\n");

    if (pass == 0)
	return (0);

    pdg->flgs = 0;

    /********** Check Simulation Links **********/
    /* dg.siml must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (pdg->siml.type == CONSTANT) {
	recGblInitConstantLink(&pdg->siml, DBF_USHORT, &pdg->simm);
    }
    /* dg.slmo must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (pdg->slmo.type == CONSTANT) {
	recGblInitConstantLink(&pdg->slmo, DBF_USHORT, &pdg->svmo);
    }
    /* dg.sls1 must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (pdg->sls1.type == CONSTANT) {
	recGblInitConstantLink(&pdg->sls1, DBF_USHORT, &pdg->svs1);
    }
    /* dg.sls2 must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (pdg->sls2.type == CONSTANT) {
	recGblInitConstantLink(&pdg->sls2, DBF_USHORT, &pdg->svs2);
    }
    /* dg.slcr must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (pdg->slcr.type == CONSTANT) {
	recGblInitConstantLink(&pdg->slcr, DBF_DOUBLE, &pdg->svcr);
    }
    /********** End Check Simulation Links **********/

    /********** Initialization of Record **********/
    if (recDigitelDebug)
	printf("recDigitel.c: Look for device support\n");

    /*** look for device support ***/
    if ( (pdset = (digiteldset *) (pdg->dset) )== NULL) {
	recGblRecordError(S_dev_noDSET, (void*)pdg, "digitel: init_record");
	if (recDigitelDebug)
	    printf("recDigitel.c: Missing device support entry table\n");
	return (S_dev_noDSET);
    }

    /*** must have proc_xact function defined ***/
    if ((pdset->number < 5) || (pdset->proc_xact == NULL)) {
	recGblRecordError(S_dev_missingSup, (void *)pdg, "digitel: init_record");
	if (recDigitelDebug)
	    printf("recDigitel.c: Missing device support\n");
	return (S_dev_missingSup);
    }

    /*** if everything is OK init the record ***/
    if (pdset->init_record) {
	if ((status = (*pdset->init_record) (pdg))) {
	    if (recDigitelDebug)
		printf("recDigitel.c: Failure in calling record initialization\n");
	    return (status);
	}
    }
    return (0);
    /********** End Initialization of Record **********/
}

/*****************************************************************************
 *
 * This is called to "Process" the record.
 *
 * When we process a record for the digitel, we write out the new values for any
 * field whos'e value(s) might have changed, then read back the operating status
 * from the machine.
 *
 ******************************************************************************/
static long process(void *precord)
{
    digitelRecord *pdg = (digitelRecord *)precord;
    digiteldset *pdset = (digiteldset *) (pdg->dset);
    long status;
    unsigned char pact = pdg->pact;

    long nRequest = 1;
    long options = 0;

    if (recDigitelDebug >= 25)
	printf("recDigitel.c: Process(%s)\n", pdg->name);

    if ((pdset == NULL) || (pdset->proc_xact == NULL)) {
	pdg->pact = TRUE;
	recGblRecordError(S_dev_missingSup, (void *)pdg, "read_digitel");
	return (S_dev_missingSup);
    }

    /*** if not currently active... save initial values for pressure, voltage... ***/
    if (!pact) {
	pdg->ival = pdg->val;
	pdg->ilva = pdg->lval;
	pdg->imod = pdg->modr;
	pdg->ibak = pdg->bakr;
	pdg->icol = pdg->cool;
	pdg->isp1 = pdg->set1;
	pdg->isp2 = pdg->set2;
	pdg->isp3 = pdg->set3;
	pdg->iacw = pdg->accw;
	pdg->iaci = pdg->acci;
	pdg->ipty = pdg->ptyp;
	pdg->ibkn = pdg->bkin;
	pdg->is1 = pdg->sp1r;
	pdg->ih1 = pdg->s1hr;
	pdg->im1 = pdg->s1mr;
	pdg->ii1 = pdg->s1vr;
	pdg->is2 = pdg->sp2r;
	pdg->ih2 = pdg->s2hr;
	pdg->im2 = pdg->s2mr;
	pdg->ii2 = pdg->s2vr;
	pdg->is3 = pdg->sp3r;
	pdg->ih3 = pdg->s3hr;
	pdg->im3 = pdg->s3mr;
	pdg->ii3 = pdg->s3vr;
	pdg->ib3 = pdg->s3br;
	pdg->it3 = pdg->s3tr;
	pdg->iton = pdg->tonl;
	pdg->icrn = pdg->crnt;
	pdg->ivol = pdg->volt;
	pdg->ierr = pdg->err;
    }
    /*** check to see if simulation mode is being called for from the link ***/
    status = dbGetLink(&(pdg->siml),
		       DBR_USHORT, &(pdg->simm), &options, &nRequest);
    if (status == 0) {
	if (pdg->simm == YES) {
	    recDigitelsimFlag = 1;
	    if (recDigitelDebug >= 5)
		printf("recDigitel.c: Record being processed in simulation mode\n");
	} else
	    recDigitelsimFlag = 0;
    }

    /*** if not in simulation mode allow device support to be called ***/
    if (!(recDigitelsimFlag)) {
	/*** call device support for processing ***/
	status = (*pdset->proc_xact) (pdg);
	/*** Device support is in asynchronous mode, let it finish ***/
	if (!pact && pdg->pact)
	    return (0);
	pdg->pact = TRUE;
	recGblGetTimeStamp(pdg);
    }
    /*** if in simulation mode assign simulated values to fields ***/
    else {
	/*** simulation *** mode set  ***/
	/*** provides ability to simulate mode (OPERATE/ STANDBY) status of ***/
	/*** pump, if set to 1 (OPERATE) make pump voltage appear to be at  ***/
	/*** an "on" (6000V) level ***/
	status = dbGetLink(&(pdg->slmo),
			   DBR_USHORT, &(pdg->svmo), &options, &nRequest);
	if (status == 0)
	    pdg->modr = pdg->svmo;
	if (pdg->modr == 1)
	    pdg->volt = 6000;
	else
	    pdg->volt = 0;
	/*** simulation *** setpoint 1 set ***/
	status = dbGetLink(&(pdg->sls1),
			   DBR_USHORT, &(pdg->svs1), &options, &nRequest);
	if (status == 0)
	    pdg->set1 = pdg->svs1;
	/*** simulation *** setpoint 2 set ***/
	status = dbGetLink(&(pdg->sls2),
			   DBR_USHORT, &(pdg->svs2), &options, &nRequest);
	if (status == 0)
	    pdg->set2 = pdg->svs2;
	/*** simulation *** current level ***/
	/*** adjust current, and make pressure look like there is a 220 l/s ***/
	/*** pump being controlled ***/
	status = dbGetLink(&(pdg->slcr),
			   DBR_DOUBLE, &(pdg->svcr), &options, &nRequest);
	if (status == 0)
	    pdg->crnt = pdg->svcr;
	if (pdg->modr == 0)
	    pdg->crnt = 0;
	pdg->val = .005 * (pdg->crnt / 8.0);
	if (pdg->val <= 0)
	    pdg->lval = -10;
	else
	    pdg->lval = log10(pdg->val);
	pdg->udf = 0;	/* reset udf */
    }

    /*** check for alarms ***/
    checkAlarms(pdg);
    /*** check event list ***/
    monitor(pdg);
    /*** process the forward scan link record ***/
    recGblFwdLink(pdg);
    pdg->pact = FALSE;
    return (status);
}

/*****************************************************************************
 *
 * This function sets the precision for the current (CRNT), pressure (VAL), and
 *                         log pressure (LVAL) fields
 *
 ******************************************************************************/
static long get_precision(DBADDR *paddr, long *precision)
{
    digitelRecord *pdg = (digitelRecord *) paddr->precord;
    if (paddr->pfield == (void *) &pdg->val) {
	*precision = 1;
	return (0);
    }
    if (paddr->pfield == (void *) &pdg->lval) {
	*precision = 2;
	return (0);
    }
    if (paddr->pfield == (void *) &pdg->crnt) {
	*precision = 1;
	return (0);
    }
    *precision = 0;
    recGblGetPrec(paddr, precision);
    return (0);
}

/*****************************************************************************
 *
 * This function is called before and after any field that is labeled (SPC_MOD)
 * is altered by a dbPutfield().  The use of this function is to simply take
 * note of what fields have been altered since the last time the record was
 * processed.  The record of what fields have been altered is kept in
 * precord->flgs.
 *
 * A secondary purpose of this function is to clamp the limits of the
 * SP1, SP2, and SP3 setpoint and hysteresis fields.
 *
 ******************************************************************************/
static long special(DBADDR *paddr, int after)
{
    void *p;
    digitelRecord *pdg = (digitelRecord *) (paddr->precord);

    if (!after)
	return (0);

    if (recDigitelDebug >= 25)
	printf("recDigitel.c: special()\n");

    /*** Make sure we have the proper special flag type spec'd ***/
    if (paddr->special != SPC_MOD) {
	recGblDbaddrError(S_db_badChoice, paddr, "dg: special");
	return (S_db_badChoice);
    }
    p = (void *) (paddr->pfield);
    /*** Figure out which field has been changed ***/
    if (p == &(pdg->dspl)) {
	pdg->flgs |= MOD_DSPL;
	if (recDigitelDebug >= 15)
	    printf("recDigitel.c: >%s< DSPL changed to %d\n", pdg->name, pdg->dspl);
    } else if (p == &(pdg->klck)) {
	pdg->flgs |= MOD_KLCK;
	if (recDigitelDebug >= 15)
	    printf("recDigitel.c: >%s< KLCK changed to %d\n", pdg->name, pdg->klck);
    } else if (p == &(pdg->mods)) {
	pdg->flgs |= MOD_MODS;
	if (recDigitelDebug >= 15)
	    printf("recDigitel.c: >%s< MODS changed to %d\n", pdg->name, pdg->mods);
    } else if (p == &(pdg->baks)) {
	pdg->flgs |= MOD_BAKE;
	if (recDigitelDebug >= 15)
	    printf("recDigitel.c: >%s< BAKS changed to %d\n", pdg->name, pdg->baks);
    } else if (p == &(pdg->sp1s)) {
	if (pdg->sp1s > DIGITEL_MAXSP)
	    pdg->sp1s = DIGITEL_MAXSP;
	else if (pdg->sp1s < DIGITEL_MINSP)
	    pdg->sp1s = DIGITEL_MINSP;
	if (recDigitelDebug >= 15)
	    printf("recDigitel.c: >%s< SP1S changed to %e\n", pdg->name, pdg->sp1s);
	pdg->spfg |= MOD_SP1S;
	pdg->flgs |= MOD_SETP;
    } else if (p == &(pdg->s1hs)) {
	if (pdg->s1hs > DIGITEL_MAXHY)
	    pdg->s1hs = DIGITEL_MAXHY;
	else if (pdg->s1hs < DIGITEL_MINHY)
	    pdg->s1hs = DIGITEL_MINHY;
	if (recDigitelDebug >= 15)
	    printf("recDigitel.c: >%s< S1HS changed to %e\n", pdg->name, pdg->s1hs);
	pdg->spfg |= MOD_S1HS;
	pdg->flgs |= MOD_SETP;
    } else if (p == &(pdg->s1ms)) {
	pdg->spfg |= MOD_S1MS;
	pdg->flgs |= MOD_SETP;
	if (recDigitelDebug >= 15)
	    printf("recDigitel.c: >%s< S1MS changed to %d\n", pdg->name, pdg->s1ms);
    } else if (p == &(pdg->s1vs)) {
	pdg->spfg |= MOD_S1VS;
	pdg->flgs |= MOD_SETP;
	if (recDigitelDebug >= 15)
	    printf("recDigitel.c: >%s< S1VS changed to %d\n", pdg->name, pdg->s1vs);
    } else if (p == &(pdg->sp2s)) {
	if (pdg->sp2s > DIGITEL_MAXSP)
	    pdg->sp2s = DIGITEL_MAXSP;
	else if (pdg->sp2s < DIGITEL_MINSP)
	    pdg->sp2s = DIGITEL_MINSP;
	if (recDigitelDebug >= 15)
	    printf("recDigitel.c: >%s< SP2S changed to %e\n", pdg->name, pdg->sp2s);
	pdg->spfg |= MOD_SP2S;
	pdg->flgs |= MOD_SETP;
    } else if (p == &(pdg->s2hs)) {
	if (pdg->s2hs > DIGITEL_MAXHY)
	    pdg->s2hs = DIGITEL_MAXHY;
	else if (pdg->s2hs < DIGITEL_MINHY)
	    pdg->s2hs = DIGITEL_MINHY;
	if (recDigitelDebug >= 15)
	    printf("recDigitel.c: >%s< S2HS changed to %e\n", pdg->name, pdg->s2hs);
	pdg->spfg |= MOD_S2HS;
	pdg->flgs |= MOD_SETP;
    } else if (p == &(pdg->s2ms)) {
	pdg->spfg |= MOD_S2MS;
	pdg->flgs |= MOD_SETP;
	if (recDigitelDebug >= 15)
	    printf("recDigitel.c: >%s< S2MS changed to %d\n", pdg->name, pdg->s2ms);
    } else if (p == &(pdg->s2vs)) {
	pdg->spfg |= MOD_S2VS;
	pdg->flgs |= MOD_SETP;
	if (recDigitelDebug >= 15)
	    printf("recDigitel.c: >%s< S2VS changed to %d\n", pdg->name, pdg->s2vs);
    } else if (p == &(pdg->sp3s)) {
	if (pdg->sp3s > DIGITEL_MAXSP)
	    pdg->sp3s = DIGITEL_MAXSP;
	else if (pdg->sp3s < DIGITEL_MINSP)
	    pdg->sp3s = DIGITEL_MINSP;
	if (recDigitelDebug >= 15)
	    printf("recDigitel.c: >%s< SP3S changed to %e\n", pdg->name, pdg->sp3s);
	pdg->spfg |= MOD_SP3S;
	pdg->flgs |= MOD_SETP;
    } else if (p == &(pdg->s3hs)) {
	if (pdg->s3hs > DIGITEL_MAXHY)
	    pdg->s3hs = DIGITEL_MAXHY;
	else if (pdg->s3hs < DIGITEL_MINHY)
	    pdg->s3hs = DIGITEL_MINHY;
	if (recDigitelDebug >= 15)
	    printf("recDigitel.c: >%s< S3HS changed to %e\n", pdg->name, pdg->s3hs);
	pdg->spfg |= MOD_S3HS;
	pdg->flgs |= MOD_SETP;
    } else if (p == &(pdg->s3ms)) {
	pdg->spfg |= MOD_S3MS;
	pdg->flgs |= MOD_SETP;
	if (recDigitelDebug >= 15)
	    printf("recDigitel.c: >%s< S3MS changed to %d\n", pdg->name, pdg->s3ms);
    } else if (p == &(pdg->s3vs)) {
	pdg->spfg |= MOD_S3VS;
	pdg->flgs |= MOD_SETP;
	if (recDigitelDebug >= 15)
	    printf("recDigitel.c: >%s< S3VS changed to %d\n", pdg->name, pdg->s3vs);
    } else if (p == &(pdg->s3bs)) {
	pdg->spfg |= MOD_S3BS;
	pdg->flgs |= MOD_SETP;
	if (recDigitelDebug >= 15)
	    printf("recDigitel.c: >%s< S3BS changed to %d\n", pdg->name, pdg->s3bs);
    } else if (p == &(pdg->s3ts)) {
	pdg->spfg |= MOD_S3TS;
	pdg->flgs |= MOD_SETP;
	if (recDigitelDebug >= 15)
	    printf("recDigitel.c: >%s< S3TS changed to %f\n", pdg->name, pdg->s3ts);
    }
    return (0);
}
/*****************************************************************************
 *
 * Allow for only sensible values for entries and displays
 *
 ******************************************************************************/
static long get_graphic_double(DBADDR *paddr, struct dbr_grDouble *pgd)
{
    digitelRecord *pdg = (digitelRecord *) (paddr->precord);
    void *p;
    p = (void *) (paddr->pfield);
    if ((p == &(pdg->s1hs)) || (p == &(pdg->s2hs)) || (p == &(pdg->s3hs)) ||
	(p == &(pdg->s1hr)) || (p == &(pdg->s2hr)) || (p == &(pdg->s3hr))) {
	pgd->upper_disp_limit = DIGITEL_MAXHY;
	pgd->lower_disp_limit = DIGITEL_MINHY;
    } else if ((p == &(pdg->sp1s)) || (p == &(pdg->sp2s)) || 
               (p == &(pdg->sp3s)) || (p == &(pdg->sp1r)) || 
               (p == &(pdg->sp2r)) || (p == &(pdg->sp3r))) {
	pgd->upper_disp_limit = DIGITEL_MAXSP;
	pgd->lower_disp_limit = DIGITEL_MINSP;
    } else if (p == &(pdg->val)) {
	pgd->upper_disp_limit = pdg->hopr;
	pgd->lower_disp_limit = pdg->lopr;
    } else if (p == &(pdg->crnt)) {
	pgd->upper_disp_limit = pdg->hctr;
	pgd->lower_disp_limit = pdg->lctr;
    } else if (p == &(pdg->lval)) {
	pgd->upper_disp_limit = pdg->hlpr;
	pgd->lower_disp_limit = pdg->llpr;
    } else if (p == &(pdg->volt)) {
	pgd->upper_disp_limit = pdg->hvtr;
	pgd->lower_disp_limit = pdg->lvtr;
    } else
	recGblGetGraphicDouble(paddr, pgd);
    return (0);
}
/*****************************************************************************
 *
 * Allow for only sensible values for setpoint entries and displays
 *
 ******************************************************************************/
static long get_control_double(DBADDR *paddr, struct dbr_ctrlDouble * pcd)
{
    digitelRecord *pdg = (digitelRecord *) (paddr->precord);
    void *p;
    p = (void *) (paddr->pfield);
    if ((p == &(pdg->s1hs)) || (p == &(pdg->s2hs)) || (p == &(pdg->s3hs))) {
	pcd->upper_ctrl_limit = DIGITEL_MAXHY;
	pcd->lower_ctrl_limit = DIGITEL_MINHY;
    } else if ((p == &(pdg->sp1s)) || (p == &(pdg->sp2s)) || 
    	       (p == &(pdg->sp3s))) {
	pcd->upper_ctrl_limit = DIGITEL_MAXSP;
	pcd->lower_ctrl_limit = DIGITEL_MINSP;
    } else
	recGblGetControlDouble(paddr, pcd);
    return (0);
}
/*****************************************************************************
 *
 ******************************************************************************/
static long get_alarm_double(DBADDR *paddr, struct dbr_alDouble *pad)
{
    digitelRecord *pdg = (digitelRecord *) (paddr->precord);
    void *p;
    p = (void *) (paddr->pfield);
    if (p == &(pdg->val)) {
	pad->upper_alarm_limit = pdg->hihi;
	pad->upper_warning_limit = pdg->high;
	pad->lower_warning_limit = pdg->low;
	pad->lower_alarm_limit = pdg->lolo;
    } else {
	pad->upper_alarm_limit = 0;
	pad->upper_warning_limit = 0;
	pad->lower_warning_limit = 0;
	pad->lower_alarm_limit = 0;
	recGblGetAlarmDouble(paddr, pad);
    }
    return (0);
}
/*****************************************************************************
 *
 ******************************************************************************/
static void checkAlarms(digitelRecord *pdg)
{
    double val;
    float hyst, lalm, hihi, high, lolo, low;
    unsigned short hhsv, llsv, hsv, lsv;
    if (recDigitelDebug >= 25)
	printf("recDigitel.c: checkAlarms()\n");
    if (pdg->udf == TRUE) {
	if (recDigitelDebug >= 20)
	    printf("recDigitel.c: udf set true... valid alarm\n");
	recGblSetSevr(pdg, UDF_ALARM, INVALID_ALARM);
	return;
    }
    hihi = pdg->hihi;
    lolo = pdg->lolo;
    high = pdg->high;
    low = pdg->low;
    hhsv = pdg->hhsv;
    llsv = pdg->llsv;
    hsv = pdg->hsv;
    lsv = pdg->lsv;
    val = pdg->val;
    hyst = pdg->hyst;
    lalm = pdg->lalm;
    /* Check the PRESSURE */
    /* alarm condition hihi */
    if (hhsv && (val >= hihi || ((lalm == hihi) && (val >= hihi - hyst)))) {
	if (recGblSetSevr(pdg, HIHI_ALARM, pdg->hhsv))
	    pdg->lalm = hihi;
	return;
    }
    /* alarm condition lolo */
    if (llsv && (val <= lolo || ((lalm == lolo) && (val <= lolo + hyst)))) {
	if (recGblSetSevr(pdg, LOLO_ALARM, pdg->llsv))
	    pdg->lalm = lolo;
	return;
    }
    /* alarm condition high */
    if (hsv && (val >= high || ((lalm == high) && (val >= high - hyst)))) {
	if (recGblSetSevr(pdg, HIGH_ALARM, pdg->hsv))
	    pdg->lalm = high;
	return;
    }
    /* alarm condition low */
    if (lsv && (val <= low || ((lalm == low) && (val <= low + hyst)))) {
	if (recGblSetSevr(pdg, LOW_ALARM, pdg->lsv))
	    pdg->lalm = low;
	return;
    }
    /* we get here only if val is out of alarm by at least hyst */
    pdg->lalm = val;
    return;
}
/*****************************************************************************
 *
 * Post any change-of-state events.  Note that the alarm status is applied
 * to all the fields in the record.
 *
 ******************************************************************************/
static void monitor(digitelRecord *pdg)
{
    unsigned short monitor_mask;
    int alrm_chg_flg;
    alrm_chg_flg = 0;
    monitor_mask = 0;
    
    if (recDigitelDebug >= 25)
	printf("recDigitel.c: monitor()\n");
	
    monitor_mask = alrm_chg_flg = recGblResetAlarms(pdg);
    monitor_mask |= (DBE_VALUE | DBE_LOG);
    /*** check for value change ***/
    if ((pdg->ival != pdg->val) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->val, monitor_mask);
    if ((pdg->ilva != pdg->lval) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->lval, monitor_mask);
    if ((pdg->imod != pdg->modr) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->modr, monitor_mask);
    if ((pdg->ibak != pdg->bakr) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->bakr, monitor_mask);
    if ((pdg->icol != pdg->cool) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->cool, monitor_mask);
    if ((pdg->ibkn != pdg->bkin) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->bkin, monitor_mask);
    if ((pdg->isp1 != pdg->set1) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->set1, monitor_mask);
    if ((pdg->isp2 != pdg->set2) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->set2, monitor_mask);
    if ((pdg->isp3 != pdg->set3) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->set3, monitor_mask);
    if ((pdg->iacw != pdg->accw) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->accw, monitor_mask);
    if ((pdg->iaci != pdg->acci) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->acci, monitor_mask);
    if ((pdg->ipty != pdg->ptyp) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->ptyp, monitor_mask);
    if ((pdg->is1 != pdg->sp1r) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->sp1r, monitor_mask);
    if ((pdg->ih1 != pdg->s1hr) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->s1hr, monitor_mask);
    if ((pdg->im1 != pdg->s1mr) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->s1mr, monitor_mask);
    if ((pdg->ii1 != pdg->s1vr) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->s1vr, monitor_mask);
    if ((pdg->is2 != pdg->sp2r) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->sp2r, monitor_mask);
    if ((pdg->ih2 != pdg->s2hr) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->s2hr, monitor_mask);
    if ((pdg->im2 != pdg->s2mr) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->s2mr, monitor_mask);
    if ((pdg->ii2 != pdg->s2vr) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->s2vr, monitor_mask);
    if ((pdg->is3 != pdg->sp3r) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->sp3r, monitor_mask);
    if ((pdg->ih3 != pdg->s3hr) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->s3hr, monitor_mask);
    if ((pdg->im3 != pdg->s3mr) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->s3mr, monitor_mask);
    if ((pdg->ii3 != pdg->s3vr) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->s3vr, monitor_mask);
    if ((pdg->ib3 != pdg->s3br) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->s3br, monitor_mask);
    if ((pdg->it3 != pdg->s3tr) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->s3tr, monitor_mask);
    if ((pdg->iton != pdg->tonl) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->tonl, monitor_mask);
    if ((pdg->icrn != pdg->crnt) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->crnt, monitor_mask);
    if ((pdg->ivol != pdg->volt) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->volt, monitor_mask);
    if ((pdg->ierr != pdg->err) || (alrm_chg_flg))
	db_post_events(pdg, &pdg->err, monitor_mask);
    return;
}
