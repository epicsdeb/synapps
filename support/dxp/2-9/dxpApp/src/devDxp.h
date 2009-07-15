/* devDXPAsyn.h -- communication between dxp record and device support */

#include <mca.h>
#include <dxpRecord.h>
#include <devSup.h>

/* This is the bit position in RUNTASKS for enable baseline cut */
#define RUNTASKS_BLCUT 0x400

typedef struct {
   unsigned short runtasks;
   unsigned short blmin;
   unsigned short blmax;
   unsigned short basebinning;
   unsigned short basethresh;
   unsigned short slowlen;
   unsigned short triggers;
   unsigned short triggersa;
   unsigned short maxwidth;
} PARAM_OFFSETS;

typedef struct {
   DXP_MODULE_TYPE moduleType;
   char **param_names;
   unsigned int numModules;
   int *first_channels;
   unsigned short *access;
   unsigned short *lbound;
   unsigned short *ubound;
   unsigned short nparams;
   unsigned int nbase_histogram;
   unsigned int nbase_history;
   unsigned int ntrace;
   PARAM_OFFSETS offsets;
} MODULE_INFO;

typedef enum {
    MSG_DXP_START_RUN,
    MSG_DXP_STOP_RUN,
    MSG_DXP_SET_SHORT_PARAM,
    MSG_DXP_SET_DOUBLE_PARAM,
    MSG_DXP_SET_SCAS,
    MSG_DXP_CONTROL_TASK,
    MSG_DXP_READ_BASELINE,
    MSG_DXP_READ_PARAMS,
    MSG_DXP_FINISH
} devDxpCommand;

/* This structure must match the order of the SCA fields in the record */
typedef struct  {
    long lo;
    long lo_rbv;
    long hi;
    long hi_rbv;
    long counts;
} DXP_SCA;

typedef struct devDxpDset {
        long            number;
        DEVSUPFUN       report;
        DEVSUPFUN       init;
        DEVSUPFUN       init_record;
        DEVSUPFUN       get_ioint_info;
        long            (*send_dxp_msg)(dxpRecord *pdxp, devDxpCommand,
                                        char *name, int param, double dvalue,
                                        void *pointer);
} devDXP_dset;
