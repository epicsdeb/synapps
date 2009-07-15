/* vsRecord.c - Record Support Routines for vacuum sensor Device  */

/*
 ****************************************************************
 *
 *      This is based on old gp307 record.
 *	Made a generic record for Vacuum Sensors
 *	Currently used with GP307, GP350 and Televac MM200
 *	Tested with asyn drivers under 3.14
 *      for backward compatability we will still keep field PRES 
 *		while the new one is VAL
 *	Mohan Ramanathan 07-20-2007
 *
 *****************************************************************
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
#define GEN_SIZE_OFFSET
#include "vsRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"


#define OFF             0
#define ON              1
#define IG1_FIELD	0x0001
#define IG2_FIELD	0x0002
#define DGS_FIELD	0x0004
#define SP1_FIELD	0x0010
#define SP2_FIELD	0x0020
#define SP3_FIELD	0x0040
#define SP4_FIELD	0x0080

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
static long special();  /* needed for special */
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
#define get_units NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_precision();
static long get_graphic_double();
static long get_control_double();
static long get_alarm_double();

rset vsRSET = {
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
epicsExportAddress(rset,vsRSET);


typedef struct vsdset {	/* vs dset */
    long number;
    DEVSUPFUN dev_report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN readWrite_vs;
}vsdset;

 
static void checkAlarms(vsRecord *pvs);
static void monitor(vsRecord *pvs);

/*****************************************************************************
 *
 * Init the record, then call the device support module to let it know that
 * this record intends on using it.
 *
 ******************************************************************************/
static long init_record(void *precord, int pass)
{
    vsRecord *pvs = (vsRecord *)precord;
    vsdset *pdset;
    long status;
        
    if (pass == 0) return (0);
    
    pvs->chgc = 0;

    if ( (pdset = (vsdset *)(pvs->dset) )== NULL) {
	recGblRecordError(S_dev_noDSET, (void *)pvs, "vs: init_record");
	return (S_dev_noDSET);
    }

    /*** must have readWrite_vs function defined ***/
    if ((pdset->number < 5) || (pdset->readWrite_vs == NULL)) {
	recGblRecordError(S_dev_missingSup, (void *)pvs, "vs: init_record");
	return (S_dev_missingSup);
    }
    
    
    /*** if everything is OK init the record ***/
    if (pdset->init_record) {
	if ((status = (*pdset->init_record) (pvs))) {
	    return (status);
	}
    }
    return (0);
}

/*****************************************************************************
 *
 * This is called to "Process" the record.
 *
 * When we process a record for the vs, we write out the new values for any
 * field whose value(s) might have changed, then read back the operating status
 * from the machine.
 *
 ******************************************************************************/
static long process(void *precord)
{
    vsRecord *pvs = (vsRecord *)precord;
    vsdset *pdset = (vsdset *)(pvs->dset);
    long status;
    unsigned char pact = pvs->pact;

    if ((pdset == NULL) || (pdset->readWrite_vs == NULL)) {
	pvs->pact = TRUE;
	recGblRecordError(S_dev_missingSup, (void *) pvs, "readWrite_vs");
	return (S_dev_missingSup);
    }

    /*** call device support for processing ***/
    status = (*pdset->readWrite_vs) (pvs);
    /*** Device support is in asynchronous mode, let it finish ***/
    if (!pact && pvs->pact)
        return (0);
    pvs->pact = TRUE;
    recGblGetTimeStamp(pvs);

    /*** check for alarms ***/
    checkAlarms(pvs);
    /*** check event list ***/
    monitor(pvs);
    /*** process the forward scan link record ***/
    recGblFwdLink(pvs);
    pvs->chgc = 0;
    pvs->pact = FALSE;
    return (status);
}

/*****************************************************************************
 *
 * This function is called before and after any field that is labeled (SPC_MOD)
 * is altered by a dbPutfield().  The use of this function is to simply take
 * note of what fields have been altered since the last time the record was
 * processed.  The record of what fields have been altered is kept in
 * precord->flgs.
 *
 ******************************************************************************/
static long special(DBADDR *paddr, int after)
{
    vsRecord *pvs = (vsRecord *)(paddr->precord);
    
    if (!after)
	return (0);
		
    if (paddr->pfield == &(pvs->ig1s))
	pvs->chgc |= IG1_FIELD;
    if (paddr->pfield == &(pvs->ig2s))
	pvs->chgc |= IG2_FIELD;
    if (paddr->pfield == &(pvs->dgss))
	pvs->chgc |= DGS_FIELD;
    if (paddr->pfield == &(pvs->sp1s))
	pvs->chgc |= SP1_FIELD;
    if (paddr->pfield == &(pvs->sp2s))
	pvs->chgc |= SP2_FIELD;
    if (paddr->pfield == &(pvs->sp3s))
	pvs->chgc |= SP3_FIELD;
    if (paddr->pfield == &(pvs->sp4s))
	pvs->chgc |= SP4_FIELD;
    return (0);
}

/*****************************************************************************
 *
 * Allow for only sensible values for displays
 *
 ******************************************************************************/
static long get_graphic_double(DBADDR *paddr, struct dbr_grDouble *pgd)
{
    vsRecord *pvs = (vsRecord *)paddr->precord;
    int	fieldIndex = dbGetFieldIndex(paddr);
    
    if (fieldIndex == vsRecordVAL || fieldIndex == vsRecordPRES) {
	pgd->upper_disp_limit = pvs->hopr;
	pgd->lower_disp_limit = pvs->lopr;
    } else if (fieldIndex == vsRecordCGAP) {
	pgd->upper_disp_limit = pvs->hapr;
	pgd->lower_disp_limit = pvs->lapr;
    } else if (fieldIndex == vsRecordCGBP) {
	pgd->upper_disp_limit = pvs->hbpr;
	pgd->lower_disp_limit = pvs->lbpr;
    } else if (fieldIndex == vsRecordLPRS) {
	pgd->upper_disp_limit = pvs->hlpr;
	pgd->lower_disp_limit = pvs->llpr;
    } else if (fieldIndex == vsRecordLCAP) {
	pgd->upper_disp_limit = pvs->halr;
	pgd->lower_disp_limit = pvs->lalr;
    } else if (fieldIndex == vsRecordLCBP) {
	pgd->upper_disp_limit = pvs->hblr;
	pgd->lower_disp_limit = pvs->lblr;
    } else
	recGblGetGraphicDouble(paddr, pgd);
    return (0);
}

/*****************************************************************************
*
* Allow for only sensible values for entries
*
******************************************************************************/
static long get_control_double(DBADDR *paddr, struct dbr_ctrlDouble * pcd)
{
    vsRecord *pvs = (vsRecord *)paddr->precord;
    int	fieldIndex = dbGetFieldIndex(paddr);
    
    if (fieldIndex == vsRecordVAL || fieldIndex == vsRecordPRES
      || fieldIndex == vsRecordCGAP
      || fieldIndex == vsRecordCGBP) {
	pcd->upper_ctrl_limit = pvs->hopr;
	pcd->lower_ctrl_limit = pvs->lopr;
    } else
        recGblGetControlDouble(paddr, pcd);
    return (0);
}

/*****************************************************************************
 *
 * This function sets the precision for the Ion and Convectron Gauge
 * pressures, as well as the Ion and Convectron Gauge log pressure
 *                              fields
 *
 ******************************************************************************/
static long get_precision(DBADDR *paddr, long *precision)
{
    int	fieldIndex = dbGetFieldIndex(paddr);

    if (fieldIndex == vsRecordVAL || fieldIndex == vsRecordPRES
      || fieldIndex == vsRecordCGAP || fieldIndex == vsRecordCGBP
      || fieldIndex == vsRecordSP1R || fieldIndex == vsRecordSP2R
      || fieldIndex == vsRecordSP3R || fieldIndex == vsRecordSP4R) {
	*precision = 1;
	return (0);
    }
    if (fieldIndex == vsRecordLPRS
      || fieldIndex == vsRecordLCAP
      || fieldIndex == vsRecordLCBP) {
	*precision = 2;
	return (0);
    }
    *precision = 0;
    recGblGetPrec(paddr, precision);
    return (0);
}

/*****************************************************************************
 *
 *****************************************************************************/
static long get_alarm_double(DBADDR *paddr, struct dbr_alDouble *pad)
{
    vsRecord *pvs = (vsRecord *)paddr->precord;
    int	fieldIndex = dbGetFieldIndex(paddr);
    
    if (fieldIndex == vsRecordVAL || fieldIndex == vsRecordPRES) {
	if ((pvs->ig1s == ON) || (pvs->ig2s == ON)) {
	    pad->upper_alarm_limit = pvs->hihi;
	    pad->upper_warning_limit = pvs->high;
	    pad->lower_warning_limit = pvs->low;
	    pad->lower_alarm_limit = pvs->lolo;
	} else {
	    pad->upper_alarm_limit = 0;
	    pad->upper_warning_limit = 0;
	    pad->lower_warning_limit = 0;
	    pad->lower_alarm_limit = 0;
	}
    } else
	recGblGetAlarmDouble(paddr, pad);
    return (0);
}

/*****************************************************************************
 *
 *****************************************************************************/
static void checkAlarms(vsRecord * pvs)
{
    double val;
    double hyst, lalm;
    float hihi, high, lolo, low;
    unsigned short hhsv, llsv, hsv, lsv;
    	
    if (pvs->udf) {
	recGblSetSevr(pvs, UDF_ALARM, INVALID_ALARM);
	return;
    }
    hihi = pvs->hihi;
    lolo = pvs->lolo;
    high = pvs->high;
    low = pvs->low;
    hhsv = pvs->hhsv;
    llsv = pvs->llsv;
    hsv = pvs->hsv;
    lsv = pvs->lsv;
    val = pvs->val;
    val = pvs->pres;  /* need to be removed someday */
    hyst = pvs->hyst;
    lalm = pvs->lalm;
    
    /* Check the PRESSURE */
    /* alarm condition hihi */
    if (hhsv && (val >= hihi || ((lalm == hihi) && (val >= hihi - hyst)))) {
	if (recGblSetSevr(pvs, HIHI_ALARM, pvs->hhsv))
	    pvs->lalm = hihi;
	return;
    }
    /* alarm condition lolo */
    if (llsv && (val <= lolo || ((lalm == lolo) && (val <= lolo + hyst)))) {
	if (recGblSetSevr(pvs, LOLO_ALARM, pvs->llsv))
	    pvs->lalm = lolo;
	return;
    }
    /* alarm condition high */
    if (hsv && (val >= high || ((lalm == high) && (val >= high - hyst)))) {
	if (recGblSetSevr(pvs, HIGH_ALARM, pvs->hsv))
	    pvs->lalm = high;
	return;
    }
    /* alarm condition low */
    if (lsv && (val <= low || ((lalm == low) && (val <= low + hyst)))) {
	if (recGblSetSevr(pvs, LOW_ALARM, pvs->lsv))
	    pvs->lalm = low;
	return;
    }
    /* we get here only if val is out of alarm by at least hyst */
    pvs->lalm = val;
    return;
}

/*****************************************************************************
 *
 * Post any change-of-state events.  Note that the alarm status is applied
 * to all the fields in the record.
 *
 ******************************************************************************/
static void monitor(vsRecord * pvs)
{
    unsigned short monitor_mask;
    int alrm_chg_flg;
    alrm_chg_flg = 0;
    monitor_mask = 0;
    
    
    monitor_mask = alrm_chg_flg = recGblResetAlarms(pvs);
    monitor_mask |= (DBE_VALUE | DBE_LOG);
    /*** check for value change ***/
    if ((pvs->pval != pvs->val) || (alrm_chg_flg)) {
	db_post_events(pvs, &pvs->val, monitor_mask);
	pvs->pval = pvs->val;
    }
    if ((pvs->ppre != pvs->pres) || (alrm_chg_flg)) {
	db_post_events(pvs, &pvs->pres, monitor_mask);
	pvs->ppre = pvs->pres;
    }
    /********** SET VALUES **********/
    if ((pvs->chgc & IG1_FIELD) || (pvs->pi1s != pvs->ig1s)) {
	db_post_events(pvs, &pvs->ig1s, monitor_mask);
	pvs->pi1s = pvs->ig1s;
    }
    if ((pvs->chgc & IG2_FIELD) || (pvs->pi2s != pvs->ig2s)) {
	db_post_events(pvs, &pvs->ig2s, monitor_mask);
	pvs->pi2s = pvs->ig2s;
    }
    if ((pvs->chgc & DGS_FIELD) || (pvs->pdss != pvs->dgss)) {
	db_post_events(pvs, &pvs->dgss, monitor_mask);
	pvs->pdss = pvs->dgss;
    }
    if ((pvs->chgc & SP1_FIELD) || (pvs->ps1s != pvs->sp1s)) {
	db_post_events(pvs, &pvs->sp1s, monitor_mask);
	pvs->ps1s = pvs->sp1s;
    }
    if ((pvs->chgc & SP2_FIELD) || (pvs->ps2s != pvs->sp2s)) {
	db_post_events(pvs, &pvs->sp2s, monitor_mask);
	pvs->ps2s = pvs->sp2s;
    }
    if ((pvs->chgc & SP3_FIELD) || (pvs->ps3s != pvs->sp3s)) {
	db_post_events(pvs, &pvs->sp3s, monitor_mask);
	pvs->ps3s = pvs->sp3s;
    }
    if ((pvs->chgc & SP4_FIELD) || (pvs->ps4s != pvs->sp4s)) {
	db_post_events(pvs, &pvs->sp4s, monitor_mask);
	pvs->ps4s = pvs->sp4s;
    }

    /********** READBACK VALUES **********/
    if ((pvs->pig1 != pvs->ig1r) || (alrm_chg_flg)) {
	db_post_events(pvs, &pvs->ig1r, monitor_mask);
	pvs->pig1 = pvs->ig1r;
    }
    if ((pvs->pig2 != pvs->ig2r) || (alrm_chg_flg)) {
	db_post_events(pvs, &pvs->ig2r, monitor_mask);
	pvs->pig2 = pvs->ig2r;
    }
    if ((pvs->pdgs != pvs->dgsr) || (alrm_chg_flg)) {
	db_post_events(pvs, &pvs->dgsr, monitor_mask);
	pvs->pdgs = pvs->dgsr;
    }
    if ((pvs->pflt != pvs->fltr) || (alrm_chg_flg)) {
	db_post_events(pvs, &pvs->fltr, monitor_mask);
	pvs->pflt = pvs->fltr;
    }
    if ((pvs->psp1 != pvs->sp1) || (alrm_chg_flg)) {
	db_post_events(pvs, &pvs->sp1, monitor_mask);
	pvs->psp1 = pvs->sp1;
    }
    if ((pvs->psp2 != pvs->sp2) || (alrm_chg_flg)) {
	db_post_events(pvs, &pvs->sp2, monitor_mask);
	pvs->psp2 = pvs->sp2;
    }
    if ((pvs->psp3 != pvs->sp3) || (alrm_chg_flg)) {
	db_post_events(pvs, &pvs->sp3, monitor_mask);
	pvs->psp3 = pvs->sp3;
    }
    if ((pvs->psp4 != pvs->sp4) || (alrm_chg_flg)) {
	db_post_events(pvs, &pvs->sp4, monitor_mask);
	pvs->psp4 = pvs->sp4;
    }
    if ((pvs->psp5 != pvs->sp5) || (alrm_chg_flg)) {
	db_post_events(pvs, &pvs->sp5, monitor_mask);
	pvs->psp5 = pvs->sp5;
    }
    if ((pvs->psp6 != pvs->sp6) || (alrm_chg_flg)) {
	db_post_events(pvs, &pvs->sp6, monitor_mask);
	pvs->psp6 = pvs->sp6;
    }    
    if ((pvs->ps1r != pvs->sp1r) || (alrm_chg_flg)) {
	db_post_events(pvs, &pvs->sp1r, monitor_mask);
	pvs->ps1r = pvs->sp1r;
    }
    if ((pvs->ps2r != pvs->sp2r) || (alrm_chg_flg)) {
	db_post_events(pvs, &pvs->sp2r, monitor_mask);
	pvs->ps2r = pvs->sp2r;
    }
    if ((pvs->ps3r != pvs->sp3r) || (alrm_chg_flg)) {
	db_post_events(pvs, &pvs->sp3r, monitor_mask);
	pvs->ps3r = pvs->sp3r;
    }
    if ((pvs->ps4r != pvs->sp4r) || (alrm_chg_flg)) {
	db_post_events(pvs, &pvs->sp4r, monitor_mask);
	pvs->ps4r = pvs->sp4r;
    }
    if ((pvs->plpe != pvs->lprs) || (alrm_chg_flg)) {
	db_post_events(pvs, &pvs->lprs, monitor_mask);
	pvs->plpe = pvs->lprs;
    }
    if ((pvs->pcga != pvs->cgap) || (alrm_chg_flg)) {
	db_post_events(pvs, &pvs->cgap, monitor_mask);
	pvs->pcga = pvs->cgap;
    }
    if ((pvs->plca != pvs->lcap) || (alrm_chg_flg)) {
	db_post_events(pvs, &pvs->lcap, monitor_mask);
	pvs->plca = pvs->lcap;
    }
    if ((pvs->pcgb != pvs->cgbp) || (alrm_chg_flg)) {
	db_post_events(pvs, &pvs->cgbp, monitor_mask);
	pvs->pcgb = pvs->cgbp;
    }
    if ((pvs->plcb != pvs->lcbp) || (alrm_chg_flg)) {
	db_post_events(pvs, &pvs->lcbp, monitor_mask);
	pvs->plcb = pvs->lcbp;
    }
    return;
}
