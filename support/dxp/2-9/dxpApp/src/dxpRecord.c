/* dxpRecord.c - Record Support Routines for DXP record
 *
 * This record works with any XIA module supported by Handel
 *
 *      Author:         Mark Rivers
 *      Created:        1/24/97
 */

#include        <string.h>
#include        <stdio.h>
#include        <stdlib.h>
#include        <math.h>

#include        <alarm.h>
#include        <dbDefs.h>
#include        <dbAccess.h>
#include        <dbFldTypes.h>
#include        <dbEvent.h>
#include        <devSup.h>
#include        <drvSup.h>
#include        <errMdef.h>
#include        <recSup.h>
#include        <recGbl.h>
#include        <epicsExport.h>
#define GEN_SIZE_OFFSET
#include        "dxpRecord.h"
#undef GEN_SIZE_OFFSET
#include        "mca.h"
#include        "handel.h"
#include        "handel_generic.h"
#include        "xerxes_generic.h"
#include        "epicsHandelUtils.h"
#include        "devDxp.h"


/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(struct dxpRecord *pdxp, int pass);
static long process(struct dxpRecord *pdxp);
static long special(struct dbAddr *paddr, int after);
static long process_special(struct dxpRecord *pdxp);
#define get_value NULL
static long cvt_dbaddr(struct dbAddr *paddr);
static long get_array_info(struct dbAddr *paddr, long *no_elements, long *offset);
#define put_array_info NULL
#define get_units NULL
static long get_precision(struct dbAddr *paddr, long *precision);
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double(struct dbAddr *paddr, struct dbr_grDouble *pgd);
static long get_control_double(struct dbAddr *paddr, struct dbr_ctrlDouble *pcd);
#define get_alarm_double NULL


/* The dxp record contains many short integer parameters which control the
 * DXP analog signal processing and digital filtering.
 * The record .dbd file defines 3 fields for each of these parameters:
    short xxnV
        ; The value of the parameter. Default values for some parameters are
        ; set in the .dbd file.
    char  xxnL[DXP_STRING_SIZE]
        ; The label for the parameter, which is how we look up its address
        ; with the DXP software.  These labels are defined in dxp.db, since one
        ; cannot assign the initial value of DBF_STRING fields in the .dbd
        ; file, and the values are different for the DXP4C and DXP2X.
    short xxnO
        ; The offset of this parameter within the DXP memory.  These offsets are
        ; obtained during the record initialization phase from the labels
        ; described above.
        ; Given the above fields, much of the record processing is done using
        ; loops and a pointer to a structure which is this field
 */
/* The dxp record contains many long integer parameters which are used for
 *  reading out statistical information about the run.
 * The record .dbd file defines 3 fields for each of these parameters:
    long xxnV
        ; The value of the parameter.
    char  xxnL[DXP_STRING_SIZE]
        ; The label for the first 16 bit word of the parameter, which is how 
        ; we look up its address with the DXP software.  These labels are 
        ; defined in dxp.db, since one cannot assign the initial value of 
        ; DBF_STRING fields in the .dbd file, and the values are different 
        ; for the DXP4C and DXP2X.
    short xxnOL
        ; The offset of the least significant word this parameter within the DXP memory.
        ; These offsets are obtained during the record initialization phase from the labels
        ; described above.
    short xxnOH
        ; The offset of the most significant word this parameter within the DXP memory.
        ; These offsets are obtained during the record initialization phase from the labels
        ; described above.
        ; Given the above fields, much of the record processing is done using
        ; loops and a pointer to a structure which is this field
 */
/* The dxp record contains many Yes/No menu parameters which are used for 
 * defining the "Runtasks" tasks to be perfomed.  The parameters must be 
 * defined in the order in which the bits occur in the "Runtasks" word.
 * The record .dbd file defines 2 fields for each of these parameters:
    DBF_MENU TnnV
        ; The value of the parameter. 0/1 corresponding to No/Yes.
    char  TnnL[DXP_TASK_STRING_SIZE]
        ; A descriptive string for the task.
 */
#define DXP_STRING_SIZE      16
#define DXP_TASK_STRING_SIZE 25

typedef struct  {
    unsigned short   val;
    char             label[DXP_STRING_SIZE];
    short            offset;
} DXP_SHORT_PARAM;

typedef struct  {
    long     val;
    char    label[DXP_STRING_SIZE];
    short   offset_lo;
    short   offset_hi;
} DXP_LONG_PARAM;

typedef struct  {
    unsigned short   val;
    char    label[DXP_TASK_STRING_SIZE];
} DXP_TASK_PARAM;

#define NUM_ASC_PARAMS       9 /* The number of short ASC parameters described 
                                * above in the record */
#define NUM_FIPPI_PARAMS    10 /* The number of short FIPPI parameters described
                                * above in the record */
#define NUM_DSP_PARAMS      10 /* The number of short DSP parameters described
                                *  above in the record */
#define NUM_BASELINE_PARAMS  8 /* The number of short BASELINE parameters 
                                * described above in the record */
#define NUM_READONLY_PARAMS 16 /* The number of short READ-ONLY parameters 
                                * described above in the record */
#define NUM_DXP_LONG_PARAMS 14 /* The number of long (32 bit) statistics 
                                * parameters described above in the record. 
                                * These are also read-only */
#define NUM_TASK_PARAMS     16 /* The number of task parameters described above
                                *  in the record. */
#define NUM_DOUBLE_PARAMS   29 /* The number of double parameters in the record 
                                * Must start with PKTIME */
 
#define NUM_SHORT_WRITE_PARAMS \
   (NUM_ASC_PARAMS + NUM_FIPPI_PARAMS + NUM_DSP_PARAMS + NUM_BASELINE_PARAMS)
#define NUM_SHORT_READ_PARAMS (NUM_SHORT_WRITE_PARAMS +  NUM_READONLY_PARAMS)

typedef struct {
    int anySet;
    int short_param[NUM_SHORT_READ_PARAMS];
    int double_param[NUM_DOUBLE_PARAMS];
    int task_param[NUM_TASK_PARAMS];
    int scas;
    int read_params;
    int read_histogram;
    int read_trace;
    int read_history;
    int open_relay;
    int close_relay;
    int start;
    int stop;
    int base_len;
    int base_cut_enbl;
} SPECIAL_FLAGS;

volatile int dxpRecordDebug = 0;
epicsExportAddress(int, dxpRecordDebug);


static MODULE_INFO moduleInfo[MAX_DXP_MODULE_TYPE+1];
static long monitor(struct dxpRecord *pdxp);
static void setDxpTasks(struct dxpRecord *pdxp);
static int setSCAs(struct dxpRecord *pdxp);
static int getParamOffset(MODULE_INFO *minfo, char *name, unsigned short *offset);

struct rset dxpRSET={
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
epicsExportAddress(rset, dxpRSET);


static long init_record(struct dxpRecord *pdxp, int pass)
{
    struct devDxpDset *pdset;
    int i;
    int detChan;
    MODULE_INFO *minfo;
    int status;
    DXP_SHORT_PARAM *short_param;
    DXP_LONG_PARAM *long_param;
    unsigned short offset;
    char label[100];
    char module_name[MAXALIAS_LEN];

    if (pass != 0) return(0);
    if (dxpRecordDebug > 5) printf("%s (init_record): entry\n", pdxp->name);

    /* Initialize miptr field to NULL, indicates not initialized */
    pdxp->miptr = NULL;

    /* must have dset defined */
    if (!(pdset = (struct devDxpDset *)(pdxp->dset))) {
        printf("dxpRecord:init_record no dset\n");
        status = S_dev_noDSET;
        goto bad;
    }
    if (dxpRecordDebug > 5) printf("%s (init_record): calling device support init_record\n", pdxp->name);
    if (pdset->init_record) {
        status=(*pdset->init_record)(pdxp, &detChan);
        if (status) {
            printf("dxpRecord:init_record %s device support initialization failure\n",
                pdxp->name);
            goto bad;
        }
    }
    if (dxpRecordDebug > 5) printf("%s (init_record): return from device support init_record\n", pdxp->name);

    /* If this is a detector set (detChan<0) then use detChan=0 for the following */
    if (detChan < 0) detChan = 0;

    /* Interlock access to Handel library */
    epicsHandelLock();

    /* Figure out what kind of module this is */
    
    pdxp->mtyp = dxpGetModuleType();
    if ((pdxp->mtyp < 0) || (pdxp->mtyp > MAX_DXP_MODULE_TYPE)) {
        printf("dxpRecord:init_record unknown module type %d\n", pdxp->mtyp);
        status = -1;
        epicsHandelUnlock();
        goto bad;
    }
        
    if (dxpRecordDebug > 5) printf("%s (init_record): module type=%d\n", pdxp->name, pdxp->mtyp);
    pdxp->miptr = minfo = &moduleInfo[pdxp->mtyp];
    minfo->moduleType = pdxp->mtyp;

    /* If minfo->nparams=0 then this is the first DXP record of this 
     * module type, allocate global (not record instance) structures */
    if (minfo->nparams == 0) {
        if (dxpRecordDebug > 5) printf("%s (init_record): allocating minfo structures\n", pdxp->name);
        xiaGetNumModules(&minfo->numModules);
        minfo->first_channels = calloc(minfo->numModules, sizeof(int));
        for (i=0; i<minfo->numModules; i++) {
            xiaGetModules_VB(i, module_name);
            xiaGetModuleItem(module_name, "channel0_alias", &minfo->first_channels[i]);
        }
        xiaGetNumParams(detChan, &minfo->nparams);
        xiaGetRunData(detChan, "baseline_length", &minfo->nbase_histogram);
        xiaGetSpecialRunData(detChan, "adc_trace_length", &minfo->ntrace);
        xiaGetSpecialRunData(detChan, "baseline_history_length", 
                             &minfo->nbase_history);
        minfo->param_names = (char **)malloc(minfo->nparams * sizeof(char *));
        for (i=0; i<minfo->nparams; i++) {
           minfo->param_names[i] = 
               (char *) malloc(MAX_DSP_PARAM_NAME_LEN * sizeof(char *));
        }
        minfo->access =
               (unsigned short *) calloc(minfo->nparams, 
                                         sizeof(unsigned short));
        minfo->lbound =
               (unsigned short *) calloc(minfo->nparams, 
                                         sizeof(unsigned short));
        minfo->ubound =
               (unsigned short *) calloc(minfo->nparams,
                                         sizeof(unsigned short));
        /* The following calls are not supported in the xMAP version of Handel, so
         * don't use them.  The only thing we really need to get is names, which we can
         * do with xiaGetParamName()
        *xiaGetParamData(detChan, "names", minfo->param_names);
        *xiaGetParamData(detChan, "access", minfo->access);
        *xiaGetParamData(detChan, "lower_bounds", minfo->lbound);
        *xiaGetParamData(detChan, "upper_bounds", minfo->ubound);
        */
        for (i=0; i<minfo->nparams; i++) {
           xiaGetParamName(detChan, i, minfo->param_names[i]);
        }
    }
    epicsHandelUnlock();

    if (dxpRecordDebug > 5) printf("%s (init_record): looking up short parameter addresses\n", pdxp->name);
    /* Look up the address of each parameter */
    /* Address of first short parameter */
    short_param = (DXP_SHORT_PARAM *)&pdxp->a01v;
    for (i=0; i<NUM_SHORT_READ_PARAMS; i++) {
       if (strcmp(short_param[i].label, "Unused") == 0)
          offset=-1;
       else {
          status = getParamOffset(minfo, short_param[i].label, &offset);
          if (status) printf("dxpRecord:init_record unknown parameter %s\n", 
                             short_param[i].label);
       }
       short_param[i].offset = offset;
    }
    /* Address of first long parameter */
    if (dxpRecordDebug > 5) printf("%s (init_record): looking up long parameter addresses\n", pdxp->name);
    long_param = (DXP_LONG_PARAM *)&pdxp->s01v;
    for (i=0; i<NUM_DXP_LONG_PARAMS; i++) {
       if (strcmp(long_param[i].label, "Unused") == 0) {
          offset=-1;
       } else {
          /* On the DXP4C2X and Saturn the parameters end with "0" and "1"
           * for the MSW and LSW word respectively.
           * On the xMAP the parameters end with "" and "A" for the LSW and
           * MSW respectively.
           */
          if (minfo->moduleType == DXP_XMAP) {
             strcpy(label, long_param[i].label);
             status = getParamOffset(minfo, label, &offset);
             if (status) printf("dxpRecord:init_record unknown parameter %s\n", label);
             long_param[i].offset_lo = offset;
             strcpy(label, long_param[i].label);
             strcat(label, "A");
             /* It is normal for some of these to not exist, don't print error */
             getParamOffset(minfo, label, &offset);
             long_param[i].offset_hi = offset;
          } else {
             strcpy(label, long_param[i].label);
             strcat(label, "0");
             status = getParamOffset(minfo, label, &offset);
             if (status) printf("dxpRecord:init_record unknown parameter %s\n", label);
             long_param[i].offset_hi = offset;
             strcpy(label, long_param[i].label);
             strcat(label, "1");
             status = getParamOffset(minfo, label, &offset);
             if (status) printf("dxpRecord:init_record unknown parameter %s\n", label);
             long_param[i].offset_lo = offset;
          }
       }
    }

    /* Look up the offsets of parameters we may need to access in the record */
    if (dxpRecordDebug > 5) printf("%s (init_record): looking up parameter offsets\n", pdxp->name);
    status = getParamOffset(minfo, "RUNTASKS", &minfo->offsets.runtasks); 
    if (status != 0) {
        printf("dxpRecord:init_record cannot find RUNTASKS\n");
        goto bad;
    }
    status = getParamOffset(minfo, "BLMIN", &minfo->offsets.blmin); 
    if (status != 0) {
        printf("dxpRecord:init_record cannot find BLMIN\n");
        goto bad;
    }
    status = getParamOffset(minfo, "BLMAX", &minfo->offsets.blmax); 
    if (status != 0) {
        printf("dxpRecord:init_record cannot find BLMAX\n");
        goto bad;
    }
    status = getParamOffset(minfo, "BASEBINNING", &minfo->offsets.basebinning); 
    if (status != 0) {
        printf("dxpRecord:init_record cannot find BASEBINNING\n");
        goto bad;
    }
    status = getParamOffset(minfo, "BASETHRESH", &minfo->offsets.basethresh); 
    if (status != 0) {
        printf("dxpRecord:init_record cannot find BASETHRESH\n");
        goto bad;
    }
    status = getParamOffset(minfo, "SLOWLEN", &minfo->offsets.slowlen); 
    if (status != 0) {
        printf("dxpRecord:init_record cannot find SLOWLEN\n");
        goto bad;
    }
    status = getParamOffset(minfo, "MAXWIDTH", &minfo->offsets.maxwidth); 
    if (status != 0) {
        printf("dxpRecord:init_record cannot find MAXWIDTH\n");
        goto bad;
    }
    if (minfo->moduleType == DXP_XMAP) {
        status = getParamOffset(minfo, "TRIGGERS", &minfo->offsets.triggers); 
        if (status != 0) {
            printf("dxpRecord:init_record cannot find TRIGGERS\n");
            goto bad;
        }
        status = getParamOffset(minfo, "TRIGGERSA", &minfo->offsets.triggersa); 
        if (status != 0) {
            printf("dxpRecord:init_record cannot find TRIGGERS\n");
            goto bad;
        }
    }

    if (dxpRecordDebug > 5) printf("%s (init_record): allocating arrays\n", pdxp->name);
    /* Allocate the space for the parameter array */
    pdxp->pptr = (unsigned short *)calloc(minfo->nparams, sizeof(short));

    /* Allocate the space for the baseline histogram array */
    pdxp->bptr = (long *)calloc(minfo->nbase_histogram, sizeof(long));

    /* Allocate the space for the baseline cut array */
    pdxp->bcptr = (long *)calloc(minfo->nbase_histogram, sizeof(long));

    /* Allocate the space for the baseline histogram X array */
    pdxp->bxptr = (float *)calloc(minfo->nbase_histogram, sizeof(float));

    /* Allocate the space for the baseline history array */
    pdxp->bhptr = (long *)calloc(minfo->nbase_history, sizeof(long));

    /* Allocate the space for the baseline history X array */
    pdxp->bhxptr = (float *)calloc(minfo->nbase_history, sizeof(float));
    /* Compute the baseline history X array */
    for (i=0; i<minfo->nbase_history; i++) {
        pdxp->bhxptr[i] = pdxp->bhist_time * i;
    }

    /* Allocate the space for the trace array */
    pdxp->tptr = (long *)calloc(minfo->ntrace, sizeof(long));

    /* Allocate the space for the trace X array */
    pdxp->txptr = (float *)calloc(minfo->ntrace, sizeof(float));
    /* Compute the trace X array */
    for (i=0; i<minfo->ntrace; i++) {
        pdxp->txptr[i] = pdxp->trace_wait * i;
    }

    /* Allocate the space for the special flags structure */
    pdxp->spfptr = (SPECIAL_FLAGS *)calloc(1, sizeof(SPECIAL_FLAGS));

    /* Some higher-level initialization can be done here, i.e. things which don't
     * require information on the current device parameters.  Things which do
     * require information on the current device parameters must be done in process,
     * after the first time the device is read */

    /* Set default SCAs.  Must do this first, else get errors because other
     * calls will try to read SCAs, which Handel will complain it cannot find */
    setSCAs(pdxp);

    /* Download the high-level parameters if PINI is true. 
     * Do sanity checks, don't download parameters if their values are zero
     * and that is not a valid value */
    if (pdxp->pini) {
        /* Initialize the tasks */
        setDxpTasks(pdxp);
        /* Wait 4 seconds before setting BASETHRESH, otherwise it gets overwritten? */
        if (minfo->moduleType != DXP_XMAP) epicsThreadSleep(4.0);
        /* Set first parameter we send is BASETHRESH.  If it is sent later
         * there is a problem because calibration runs may overwrite it? */
        if (pdxp->trig_pktim > 0.) status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0,
                   pdxp->trig_pktim, &pdxp->trig_pktim);
        status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0,
                   pdxp->trig_gaptim, &pdxp->trig_gaptim);
        if (pdxp->fast_trig > 0.) status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0,
                   pdxp->fast_trig, &pdxp->fast_trig);
        if (pdxp->pktim > 0.) status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0,
                   pdxp->pktim, &pdxp->pktim);
        status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0,
                   pdxp->gaptim, &pdxp->gaptim);
        status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0,
                   pdxp->slow_trig, &pdxp->slow_trig);
        status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0,
                   pdxp->maxwidth, &pdxp->maxwidth);
        status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0,
                   pdxp->base_cut_pct, &pdxp->base_cut_pct);
        status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0,
                   0., &pdxp->base_cut_enbl);
        status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0,
                   pdxp->base_thresh, &pdxp->base_thresh);
        status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0,
                   pdxp->base_len, &pdxp->base_len);
        if (pdxp->pgain > 0.) status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0,
                   pdxp->pgain, &pdxp->pgain);
        if (pdxp->emax > 0.) status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0,
                   pdxp->emax, &pdxp->emax);
        if (pdxp->ecal > 0.) status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0,
                   pdxp->ecal, &pdxp->ecal);
        /* Need to do adc_rule last? */
        if (pdxp->adc_rule > 0.) status = (*pdset->send_dxp_msg)
                   (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0,
                   pdxp->adc_rule, &pdxp->adc_rule);
    }

    if (dxpRecordDebug > 5) printf("%s (init_record): exit\n", pdxp->name);
    return(0);

    bad:
    pdxp->pact = 1;
    return(-1);
}


static long cvt_dbaddr(struct dbAddr *paddr)
{
   struct dxpRecord *pdxp=(struct dxpRecord *)paddr->precord;
   MODULE_INFO *minfo = (MODULE_INFO *)pdxp->miptr;

   if (dxpRecordDebug > 5) printf("%s (cvt_dbaddr): entry\n", pdxp->name);
   if (minfo == NULL) return(-1);
   if (paddr->pfield == &(pdxp->base)) {
      paddr->pfield = (void *)(pdxp->bptr);
      paddr->no_elements = minfo->nbase_histogram;
      paddr->field_type = DBF_LONG;
      paddr->field_size = sizeof(long);
      paddr->dbr_field_type = DBF_LONG;
   } else if (paddr->pfield == &(pdxp->base_cut)) {
      paddr->pfield = (void *)(pdxp->bcptr);
      paddr->no_elements = minfo->nbase_histogram;
      paddr->field_type = DBF_LONG;
      paddr->field_size = sizeof(long);
      paddr->dbr_field_type = DBF_LONG;
   } else if (paddr->pfield == &(pdxp->base_x)) {
      paddr->pfield = (void *)(pdxp->bxptr);
      paddr->no_elements = minfo->nbase_histogram;
      paddr->field_type = DBF_FLOAT;
      paddr->field_size = sizeof(float);
      paddr->dbr_field_type = DBF_FLOAT;
   } else if (paddr->pfield == &(pdxp->parm)) {
      paddr->pfield = (void *)(pdxp->pptr);
      paddr->no_elements = minfo->nparams;
      paddr->field_type = DBF_USHORT;
      paddr->field_size = sizeof(short);
      paddr->dbr_field_type = DBF_USHORT;
   } else if (paddr->pfield == &(pdxp->bhist)) {
      paddr->pfield = (void *)(pdxp->bhptr);
      paddr->no_elements = minfo->nbase_history;
      paddr->field_type = DBF_LONG;
      paddr->field_size = sizeof(long);
      paddr->dbr_field_type = DBF_LONG;
   } else if (paddr->pfield == &(pdxp->bhist_x)) {
      paddr->pfield = (void *)(pdxp->bhxptr);
      paddr->no_elements = minfo->nbase_history;
      paddr->field_type = DBF_FLOAT;
      paddr->field_size = sizeof(float);
      paddr->dbr_field_type = DBF_FLOAT;
   } else if (paddr->pfield == &(pdxp->trace)) {
      paddr->pfield = (void *)(pdxp->tptr);
      paddr->no_elements = minfo->ntrace;
      paddr->field_type = DBF_LONG;
      paddr->field_size = sizeof(long);
      paddr->dbr_field_type = DBF_LONG;
   } else if (paddr->pfield == &(pdxp->trace_x)) {
      paddr->pfield = (void *)(pdxp->txptr);
      paddr->no_elements = minfo->ntrace;
      paddr->field_type = DBF_FLOAT;
      paddr->field_size = sizeof(float);
      paddr->dbr_field_type = DBF_FLOAT;
   } else {
      if (dxpRecordDebug > 1) printf("%s (cvt_dbaddr): field=unknown\n", pdxp->name);
   }
   /* Limit size to 4000 for now because of EPICS CA limitations in clients */
   if (paddr->no_elements > 4000) paddr->no_elements = 4000;
   if (dxpRecordDebug > 5) printf("%s (cvt_dbaddr): exit\n", pdxp->name);
   return(0);
}


static long get_array_info(struct dbAddr *paddr, long *no_elements, long *offset)
{
   struct dxpRecord *pdxp=(struct dxpRecord *)paddr->precord;
   MODULE_INFO *minfo = (MODULE_INFO *)pdxp->miptr;

   if (dxpRecordDebug > 5) printf("%s (get_array_info): entry\n", pdxp->name);
   if (minfo == NULL) return(-1);
   if (paddr->pfield == pdxp->bptr) {
      *no_elements = minfo->nbase_histogram;
      *offset = 0;
   } else if (paddr->pfield == pdxp->bcptr) {
      *no_elements = minfo->nbase_histogram;
      *offset = 0;
   } else if (paddr->pfield == pdxp->bxptr) {
      *no_elements = minfo->nbase_histogram;
      *offset = 0;
   } else if (paddr->pfield == pdxp->bhptr) {
      *no_elements =  minfo->nbase_history;
      *offset = 0;
   } else if (paddr->pfield == pdxp->bhxptr) {
      *no_elements =  minfo->nbase_history;
      *offset = 0;
   } else if (paddr->pfield == pdxp->pptr) {
      *no_elements =  minfo->nparams;
      *offset = 0;
   } else if (paddr->pfield == pdxp->tptr) {
      *no_elements =  minfo->ntrace;
      *offset = 0;
   } else if (paddr->pfield == pdxp->txptr) {
      *no_elements =  minfo->ntrace;
      *offset = 0;
   } else {
     if (dxpRecordDebug > 1)
         printf("%s (get_array_info):field=unknown,paddr->pfield=%p,pdxp->bptr=%p\n",
                pdxp->name, paddr->pfield, pdxp->bptr);
   }
   /* Limit EPICS size to 4000 longs for now */
   if (*no_elements > 4000) *no_elements = 4000;
   if (dxpRecordDebug > 5) printf("%s (get_array_info): exit\n", pdxp->name);
   return(0);
}


static long process(struct dxpRecord *pdxp)
{
    struct devDxpDset *pdset = (struct devDxpDset *)(pdxp->dset);
    int pact=pdxp->pact;
    int status;
    SPECIAL_FLAGS *specialFlags = (SPECIAL_FLAGS *)pdxp->spfptr;

    if (dxpRecordDebug > 5) printf("%s dxpRecord(process): entry\n", pdxp->name);

    if (!pact) {
        /* See if special has been called.  If so call process_special, which will send messages
         * to device support */
        if (specialFlags->anySet) process_special(pdxp);
    }
    /* Send message indicating no more messages */
    status = (*pdset->send_dxp_msg)
             (pdxp,  MSG_DXP_FINISH, NULL, 0, 0., NULL);
    /* See if device support set pact=true, meaning it will call us back */
    if (!pact && pdxp->pact) return(0);
    pdxp->pact = TRUE;
    
    recGblGetTimeStamp(pdxp);
    monitor(pdxp);
    recGblFwdLink(pdxp);
    pdxp->pact=FALSE;

    if (dxpRecordDebug > 5) printf("%s dxpRecord(process): exit\n", pdxp->name);
    return(0);
}

static long monitor(struct dxpRecord *pdxp)
{
   int i;
   int offset;
   DXP_SHORT_PARAM *short_param;
   DXP_LONG_PARAM *long_param;
   long long_val;
   unsigned short short_val;
   unsigned short monitor_mask = recGblResetAlarms(pdxp) | DBE_VALUE | DBE_LOG;
   DXP_TASK_PARAM *task_param;
   int runtasks;
   MODULE_INFO *minfo = (MODULE_INFO *)pdxp->miptr;

   if (dxpRecordDebug > 5) printf("%s dxpRecord(monitor): entry\n", pdxp->name);
   if (minfo == NULL) return(-1);
   runtasks       = pdxp->pptr[minfo->offsets.runtasks];
   /* Get the value of each parameter, post monitor if it is different
    * from current value */
   /* Address of first short parameter */
   short_param = (DXP_SHORT_PARAM *)&pdxp->a01v;
   for (i=0; i<NUM_SHORT_READ_PARAMS; i++) {
      short_val = 0;
      offset = short_param[i].offset;
      if (offset < 0) continue;
      short_val = pdxp->pptr[offset];
      if(short_param[i].val != short_val) {
         if (dxpRecordDebug > 5) printf("%s dxpRecord: New value of short parameter %s\n",
            pdxp->name, short_param[i].label);
         if (dxpRecordDebug > 5) printf("  old (record)=%d\n", short_param[i].val);
         if (dxpRecordDebug > 5) printf("  new (dxp)=%d\n",short_val);
         short_param[i].val = short_val;

         db_post_events(pdxp,&short_param[i].val, monitor_mask);
      }
   }
   /* Address of first long parameter */
   long_param = (DXP_LONG_PARAM *)&pdxp->s01v;
   for (i=0; i<NUM_DXP_LONG_PARAMS; i++) {
      long_val = 0;
      offset = long_param[i].offset_lo;
      if (offset == -1) continue;
      long_val = pdxp->pptr[offset];
      offset = long_param[i].offset_hi;
      if (offset != -1) long_val += pdxp->pptr[offset]<<16;
      if(long_param[i].val != long_val) {
         if (dxpRecordDebug > 5) printf("%s dxpRecord: New value of long parameter %s\n",
            pdxp->name, long_param[i].label);
         if (dxpRecordDebug > 5) printf("  old (record)=%ld", long_param[i].val);
         if (dxpRecordDebug > 5) printf("  new (dxp)=%ld\n", long_val);
         long_param[i].val = long_val;
         db_post_events(pdxp,&long_param[i].val, monitor_mask);
      }
   }

   /* Set the task menus based on the value of runtasks readback */
   task_param = (DXP_TASK_PARAM *)&pdxp->t01v;
   for (i=0; i<NUM_TASK_PARAMS; i++) {
      short_val = ((runtasks & (1 << i)) != 0);
      if (dxpRecordDebug > 5) printf("%s dxpRecord: short_val=%d\n", pdxp->name, short_val);
      if (task_param[i].val != short_val) {
         if (dxpRecordDebug > 5) printf("%s dxpRecord: New value of task parameter %s\n",
            pdxp->name, task_param[i].label);
         if (dxpRecordDebug > 5) printf("  old (record)=%d", task_param[i].val);
         if (dxpRecordDebug > 5) printf("  new (dxp)=%d\n", short_val);
         task_param[i].val = short_val;
         db_post_events(pdxp,&task_param[i].val, monitor_mask);
      }
   }

   /* Clear special run flags, they are done */
   if (pdxp->read_params) {
      pdxp->read_params=0;
      db_post_events(pdxp,&pdxp->read_params, monitor_mask);
   }
   if (pdxp->read_histogram) {
      pdxp->read_histogram=0;
      db_post_events(pdxp,&pdxp->read_histogram, monitor_mask);
   }
   if (pdxp->read_trace) {
      pdxp->read_trace=0;
      db_post_events(pdxp,&pdxp->read_trace, monitor_mask);
   }
   if (pdxp->read_history) {
      pdxp->read_history=0;
      db_post_events(pdxp,&pdxp->read_history, monitor_mask);
   }
   if (pdxp->open_relay) {
      pdxp->open_relay=0;
      db_post_events(pdxp,&pdxp->open_relay, monitor_mask);
   }
   if (pdxp->close_relay) {
      pdxp->close_relay=0;
      db_post_events(pdxp,&pdxp->close_relay, monitor_mask);
   }

   if (dxpRecordDebug > 5) printf("%s dxpRecord(monitor): exit\n", pdxp->name);
   return(0);
}

static long special(struct dbAddr *paddr, int after)
/* Called whenever a field is changed 
 * This routine just sets a flag indicating that a field has changed.
 * All fields that are SPC_MOD are also PP, so process() will be called
 * process() calls process_special, which sends messages to device support 
 */
{
    struct dxpRecord *pdxp=(struct dxpRecord *)paddr->precord;
    int i;
    DXP_SHORT_PARAM *short_param;
    DXP_TASK_PARAM *task_param;
    double *double_param;
    DXP_SCA *sca;
    MODULE_INFO *minfo = (MODULE_INFO *)pdxp->miptr;
    SPECIAL_FLAGS *specialFlags = (SPECIAL_FLAGS *)pdxp->spfptr;

    if (minfo == NULL) return(-1);
    if (!after) return(0);  /* Don't do anything if field not yet changed */

    if (dxpRecordDebug > 5) printf("%s dxpRecord(special): entry\n", pdxp->name);

    /* Loop through seeing which field was changed.  Set the flag */
    /* Address of first short parameter */
    short_param = (DXP_SHORT_PARAM *)&pdxp->a01v;
    for (i=0; i<NUM_SHORT_READ_PARAMS; i++) {
       if (paddr->pfield == (void *) &short_param[i].val) {
          specialFlags->short_param[i] = 1;
          goto found_param;
       }
    }

    /* Address of first double parameter */
    double_param = &pdxp->pktim;
    for (i=0; i<NUM_DOUBLE_PARAMS; i++) {
       if (paddr->pfield == (void *) &double_param[i]) {
          specialFlags->double_param[i] = 1;
          goto found_param;
       }
    }

    /* Check if the tasks have changed */
    task_param = (DXP_TASK_PARAM *)&pdxp->t01v;
    for (i=0; i<NUM_TASK_PARAMS; i++) {
       if (paddr->pfield == (void *) &task_param[i].val) {
          specialFlags->task_param[i] = 1;
          goto found_param;
       }
    }

    /* Check if the SCAS have changed */
    sca = (DXP_SCA *)&pdxp->sca0_lo;
    for (i=0; i<NUM_DXP_SCAS; i++) {
       if ((paddr->pfield == (void *) &sca[i].lo) ||
           (paddr->pfield == (void *) &sca[i].hi) ||
           (paddr->pfield == (void *) &pdxp->num_scas)) {
          specialFlags->scas = 1;
          goto found_param;
       }
    }

    /* Special runs */
    if (paddr->pfield == (void *) &pdxp->read_params) {
         if (pdxp->read_params)
             specialFlags->read_params = 1; 
         goto found_param;
    }
    if (paddr->pfield == (void *) &pdxp->read_histogram) {
         if (pdxp->read_histogram)
             specialFlags->read_histogram = 1; 
         goto found_param;
    }
    if (paddr->pfield == (void *) &pdxp->read_trace) {
         if (pdxp->read_trace) 
             specialFlags->read_trace = 1; 
         goto found_param;
    }
    if (paddr->pfield == (void *) &pdxp->read_history) {
         if (pdxp->read_history) 
             specialFlags->read_history = 1; 
         goto found_param;
    }
    if (paddr->pfield == (void *) &pdxp->open_relay) {
         if (pdxp->open_relay) 
             specialFlags->open_relay = 1; 
         goto found_param;
    }
    if (paddr->pfield == (void *) &pdxp->close_relay) {
         if (pdxp->close_relay) 
             specialFlags->close_relay = 1; 
         goto found_param;
    }
    if (paddr->pfield == (void *) &pdxp->strt) {
         if (pdxp->strt) 
             specialFlags->start = 1; 
         goto found_param;
    }
    if (paddr->pfield == (void *) &pdxp->stop) {
         if (pdxp->stop) 
             specialFlags->stop = 1; 
         goto found_param;
    }
    if (paddr->pfield == (void *) &pdxp->base_len) {
         specialFlags->base_len = 1; 
         goto found_param;
    }
    if (paddr->pfield == (void *) &pdxp->base_cut_enbl) {
         specialFlags->base_cut_enbl = 1; 
         goto found_param;
    }

found_param:
    specialFlags->anySet = 1;
    if (dxpRecordDebug > 5) printf("%s dxpRecord(special) : exit\n", pdxp->name);
    return(0);
}


static long process_special(dxpRecord *pdxp)
/* Called whenever a field is changed */
{
    struct devDxpDset *pdset = (struct devDxpDset *)(pdxp->dset);
    short offset;
    int i;
    int status;
    DXP_SHORT_PARAM *short_param;
    DXP_TASK_PARAM *task_param;
    double *double_param;
    MODULE_INFO *minfo = (MODULE_INFO *)pdxp->miptr;
    SPECIAL_FLAGS *specialFlags = (SPECIAL_FLAGS *)pdxp->spfptr;

    if (minfo == NULL) return(-1);

    if (dxpRecordDebug > 5) printf("%s dxpRecord(process_special): entry\n", pdxp->name);

    /* Loop through seeing which field was changed.  Write the new value.*/
    /* Address of first short parameter */
    short_param = (DXP_SHORT_PARAM *)&pdxp->a01v;
    for (i=0; i<NUM_SHORT_READ_PARAMS; i++) {
       if (specialFlags->short_param[i]) {
          specialFlags->short_param[i] = 0;
          offset = short_param[i].offset;
          if (offset == -1) continue;
          status = (*pdset->send_dxp_msg)
                    (pdxp,  MSG_DXP_SET_SHORT_PARAM, short_param[i].label, 
                    short_param[i].val, 0., NULL);
       }
    }

    /* Address of first double parameter */
    double_param = &pdxp->pktim;
    for (i=0; i<NUM_DOUBLE_PARAMS; i++) {
       if (specialFlags->double_param[i]) {
          specialFlags->double_param[i] = 0;
          status = (*pdset->send_dxp_msg)
                    (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0, 
                    *(&double_param[i]), &double_param[i]);
       }
    }

    /* Check if the tasks have changed */
    task_param = (DXP_TASK_PARAM *)&pdxp->t01v;
    for (i=0; i<NUM_TASK_PARAMS; i++) {
       if (specialFlags->task_param[i]) {
          specialFlags->task_param[i] = 0;
          if (strcmp(task_param[i].label, "Unused") == 0) continue;
          setDxpTasks(pdxp);
       }
    }

    /* Check if the SCAS have changed */
    if (specialFlags->scas) {
       specialFlags->scas = 0;
       setSCAs(pdxp);
    }

    /* Special runs */
    if (specialFlags->read_params) {
        specialFlags->read_params = 0;
        status = (*pdset->send_dxp_msg) (pdxp, MSG_DXP_READ_PARAMS, 
                   NULL, 0, 0., NULL);
    }
    if (specialFlags->read_histogram) {
        specialFlags->read_histogram = 0;
        status = (*pdset->send_dxp_msg) (pdxp, MSG_DXP_READ_BASELINE, 
                   NULL, 0, 0., NULL);
    }
    if (specialFlags->read_trace) {
        specialFlags->read_trace = 0;
        status = (*pdset->send_dxp_msg) (pdxp, MSG_DXP_CONTROL_TASK, 
                  "adc_trace", 0, pdxp->trace_wait*1000., NULL);
    }
    if (specialFlags->read_history) {
        specialFlags->read_history = 0;
             status = (*pdset->send_dxp_msg) (pdxp, MSG_DXP_CONTROL_TASK, 
                       "baseline_history", 0, pdxp->bhist_time*1000., NULL);
    }
    if (specialFlags->open_relay) {
        specialFlags->open_relay = 0;
        status = (*pdset->send_dxp_msg) (pdxp, MSG_DXP_CONTROL_TASK, 
                  "open_input_relay", 0, 0., NULL);
    }
    if (specialFlags->close_relay) {
        specialFlags->close_relay = 0;
        status = (*pdset->send_dxp_msg) (pdxp, MSG_DXP_CONTROL_TASK, 
                  "close_input_relay", 0, 0., NULL);
    }
    if (specialFlags->start) {
        specialFlags->start = 0;
        status = (*pdset->send_dxp_msg) (pdxp, MSG_DXP_START_RUN, 
                  NULL, 0, 0., NULL);
    }
    if (specialFlags->stop) {
        specialFlags->stop = 0;
        status = (*pdset->send_dxp_msg) (pdxp, MSG_DXP_STOP_RUN, 
                  NULL, 0, 0., NULL);
    }
    if (specialFlags->base_len) {
        specialFlags->base_len = 0;
        status = (*pdset->send_dxp_msg)
                  (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0, 
                  0., &pdxp->base_len);
    }
    if (specialFlags->base_cut_enbl) {
        specialFlags->base_cut_enbl = 0;
        status = (*pdset->send_dxp_msg)
                  (pdxp,  MSG_DXP_SET_DOUBLE_PARAM, NULL, 0, 
                  0., &pdxp->base_cut_enbl);
    }

    specialFlags->anySet = 0;
    if (dxpRecordDebug > 5) printf("%s dxpRecord(process_special): exit\n", pdxp->name);
    return(0);
}


static void setDxpTasks(struct dxpRecord *pdxp)
{
   struct devDxpDset *pdset = (struct devDxpDset *)(pdxp->dset);
   DXP_TASK_PARAM *task_param;
   unsigned short runtasks;
   int i;
   int status;
   MODULE_INFO *minfo = (MODULE_INFO *)pdxp->miptr;

   if (minfo == NULL) return;
   if (minfo->moduleType == DXP_XMAP) return;  /* xMAP does not support RUNTASKS */
   if (dxpRecordDebug > 5) printf("%s dxpRecord(setDxpTasks): entry\n", pdxp->name);
   task_param = (DXP_TASK_PARAM *)&pdxp->t01v;
   runtasks = 0;
   for (i=0; i<NUM_TASK_PARAMS; i++) {
      if (strcmp(task_param[i].label, "Unused") == 0) continue;
      if (task_param[i].val) runtasks |= (1 << i);
   }
   if (dxpRecordDebug > 5) printf("%s dxpRecord(setDxpTasks): runtasks=%d, offset=%d\n", 
      pdxp->name, runtasks, minfo->offsets.runtasks);
   /* Copy new value to parameter array in case other routines need it
    * before record processes again */
    pdxp->pptr[minfo->offsets.runtasks] = runtasks;
    status = (*pdset->send_dxp_msg)
         (pdxp,  MSG_DXP_SET_SHORT_PARAM, minfo->param_names[minfo->offsets.runtasks], 
         runtasks, 0., NULL);
   if (dxpRecordDebug > 5) printf("%s dxpRecord(setDxpTasks): exit\n", pdxp->name);
}


static int setSCAs(struct dxpRecord *pdxp)
{
    int i;
    DXP_SCA *sca = (DXP_SCA *)&pdxp->sca0_lo;
    unsigned short monitor_mask = DBE_VALUE | DBE_LOG;
    struct devDxpDset *pdset = (struct devDxpDset *)(pdxp->dset);
    int status;

    if (dxpRecordDebug > 5) printf("%s dxpRecord(setSCAs): entry\n", pdxp->name);
    pdxp->num_scas = 0;
    for (i=0; i<NUM_DXP_SCAS; i++) {
        /* We would like to only program actual SCAs, but Handel has bug,
         * must set them all. */
        /* if ((sca[i].lo < 0) || (sca[i].hi < 0)) break; */
        if (sca[i].lo < 0) {
            sca[i].lo = 0;
            db_post_events(pdxp,&sca[i].lo,monitor_mask);
        }
        if (sca[i].hi < 0) {
            sca[i].hi = 0;
            db_post_events(pdxp,&sca[i].hi,monitor_mask);
        }
        if (sca[i].hi < sca[i].lo) { 
            sca[i].hi = sca[i].lo;
            db_post_events(pdxp,&sca[i].hi,monitor_mask);
        }
        pdxp->num_scas++;
    }
    status = (*pdset->send_dxp_msg)
              (pdxp,  MSG_DXP_SET_SCAS, 
              NULL, 0, (double)pdxp->num_scas, NULL);
    if (dxpRecordDebug > 5) printf("%s (setSCAs): exit\n", pdxp->name);
    return(status);
}


static long get_precision(struct dbAddr *paddr, long *precision)
{
    struct dxpRecord   *pdxp=(struct dxpRecord *)paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);

    if (dxpRecordDebug > 5) printf("%s (get_precision): entry\n", pdxp->name);
    switch (fieldIndex) {
        case dxpRecordTRACE_WAIT:
        case dxpRecordBHIST_TIME:
        case dxpRecordICR:
        case dxpRecordOCR:
            *precision = 1;
            break;
        case dxpRecordFAST_TRIG:
        case dxpRecordFAST_TRIG_RBV:
        case dxpRecordSLOW_TRIG:
        case dxpRecordSLOW_TRIG_RBV:
        case dxpRecordPKTIM:
        case dxpRecordPKTIM_RBV:
        case dxpRecordTRIG_PKTIM:
        case dxpRecordTRIG_PKTIM_RBV:
        case dxpRecordTRIG_GAPTIM:
        case dxpRecordTRIG_GAPTIM_RBV:
        case dxpRecordBASE_THRESH:
        case dxpRecordBASE_THRESH_RBV:
        case dxpRecordADC_RULE:
        case dxpRecordADC_RULE_RBV:
        case dxpRecordGAPTIM:
        case dxpRecordGAPTIM_RBV:
        case dxpRecordBASE_CUT_PCT:
        case dxpRecordBASE_CUT_PCT_RBV:
        case dxpRecordMAXWIDTH:
        case dxpRecordMAXWIDTH_RBV:
            *precision = 2;
            break;
        case dxpRecordPGAIN:
        case dxpRecordPGAIN_RBV:
        case dxpRecordEMAX:
        case dxpRecordEMAX_RBV:
            *precision = 3;
            break;
        default:
            recGblGetPrec(paddr,precision);
            break;
    }
    if (dxpRecordDebug > 5) printf("%s (get_precision): exit\n", pdxp->name);
    return(0);
}

static long get_graphic_double(struct dbAddr *paddr, struct dbr_grDouble *pgd)
{
   struct dxpRecord   *pdxp=(struct dxpRecord *)paddr->precord;
   int fieldIndex = dbGetFieldIndex(paddr);
   int offset, i;
   DXP_SHORT_PARAM *short_param;
   DXP_LONG_PARAM *long_param;
   MODULE_INFO *minfo = (MODULE_INFO *)pdxp->miptr;

   if (dxpRecordDebug > 5) printf("%s (get_graphic_double): entry\n", pdxp->name);
   if (minfo == NULL) return(-1);
   if (fieldIndex == dxpRecordPGAIN) {
      pgd->upper_disp_limit = 50.;
      pgd->lower_disp_limit = 0;
      return(0);
   }
   if (fieldIndex == dxpRecordEMAX) {
      pgd->upper_disp_limit = 200.;
      pgd->lower_disp_limit = 0;
      return(0);
   }
   if ((fieldIndex < dxpRecordA01V) || (fieldIndex > dxpRecordS13V)) {
      recGblGetGraphicDouble(paddr,pgd);
      return(0);
   } else {
      short_param = (DXP_SHORT_PARAM *)&pdxp->a01v;
      for (i=0; i<NUM_SHORT_READ_PARAMS; i++) {
         if (paddr->pfield == (void *) &short_param[i].val) {
            pgd->upper_disp_limit = 0;
            pgd->lower_disp_limit = 0;
            offset = short_param[i].offset;
            if (offset < 0) continue;
            pgd->upper_disp_limit = minfo->ubound[offset];
            pgd->lower_disp_limit = minfo->lbound[offset];
            return(0);
         }
      }
      long_param = (DXP_LONG_PARAM *)&pdxp->s01v;
      for (i=0; i<NUM_DXP_LONG_PARAMS; i++) {
         if (paddr->pfield == (void *) &long_param[i].val) {
            pgd->upper_disp_limit = 0;
            pgd->lower_disp_limit = 0;
            offset = long_param[i].offset_lo;
            if (offset < 0) continue;
            pgd->upper_disp_limit = minfo->ubound[offset];
            pgd->lower_disp_limit = minfo->lbound[offset];
            return(0);
         }
      }
   }
   return(0);
}


static long get_control_double(struct dbAddr *paddr, struct dbr_ctrlDouble *pcd)
{
   struct dxpRecord   *pdxp=(struct dxpRecord *)paddr->precord;
   int fieldIndex = dbGetFieldIndex(paddr);
   int offset, i;
   DXP_SHORT_PARAM *short_param;
   DXP_LONG_PARAM *long_param;
   MODULE_INFO *minfo = (MODULE_INFO *)pdxp->miptr;

   if (dxpRecordDebug > 5) printf("%s (get_control_double): entry\n", pdxp->name);
   if (minfo == NULL) return(-1);
   if (fieldIndex == dxpRecordPGAIN) {
      pcd->upper_ctrl_limit = 50.;
      pcd->lower_ctrl_limit = 0;
      return(0);
   }
   if (fieldIndex == dxpRecordEMAX) {
      pcd->upper_ctrl_limit = 200.;
      pcd->lower_ctrl_limit = 0;
      return(0);
   }
   if ((fieldIndex < dxpRecordA01V) || (fieldIndex > dxpRecordS13V)) {
      recGblGetControlDouble(paddr,pcd);
      return(0);
   } else {
      short_param = (DXP_SHORT_PARAM *)&pdxp->a01v;
      for (i=0; i<NUM_SHORT_READ_PARAMS; i++) {
         if (paddr->pfield == (void *) &short_param[i].val) {
            pcd->upper_ctrl_limit = 0;
            pcd->lower_ctrl_limit = 0;
            offset = short_param[i].offset;
            if (offset < 0) continue;
            pcd->upper_ctrl_limit = minfo->ubound[offset];
            pcd->lower_ctrl_limit = minfo->lbound[offset];
            return(0);
         }
      }
      long_param = (DXP_LONG_PARAM *)&pdxp->s01v;
      for (i=0; i<NUM_DXP_LONG_PARAMS; i++) {
         if (paddr->pfield == (void *) &long_param[i].val) {
            pcd->upper_ctrl_limit = 0;
            pcd->lower_ctrl_limit = 0;
            offset = long_param[i].offset_lo;
            if (offset < 0) continue;
            pcd->upper_ctrl_limit = minfo->ubound[offset];
            pcd->lower_ctrl_limit = minfo->lbound[offset];
            return(0);
         }
      }
   }
   return(0);
}


static int getParamOffset(MODULE_INFO *minfo, char *label, unsigned short *offset)
{
    int i;

    *offset = 0;
    for (i=0; i<minfo->nparams; i++) {
        if (strcmp(label, minfo->param_names[i]) == 0) {
            *offset = i;
            return(0);
        }
    }
    return(-1);
}
