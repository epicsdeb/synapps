/* devXxStrParm.c */
/*
 *      Author: Mark Rivers
 *      Date:   06-May-2004
 *      26-July-2004  MLR Convert to pasynEpicsUtils->parseLink
***********************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <dbScan.h>
#include <dbDefs.h>
#include <dbAccess.h>
#include <dbCommon.h>
#include <epicsString.h>
#include <errlog.h>
#include <alarm.h>
#include <link.h>
#include <recGbl.h>
#include <recSup.h>
#include <devSup.h>
#include <asynDriver.h>
#include <asynEpicsUtils.h>
#include <asynOctet.h>
#include <aiRecord.h>
#include <aoRecord.h>
#include <biRecord.h>
#include <boRecord.h>
#include <longinRecord.h>
#include <longoutRecord.h>
#include <stringinRecord.h>
#include <stringoutRecord.h>
#include <epicsExport.h>

#define HEXCHAR2INT(c) ((c)>'9' ? 10+tolower(c)-'a' : tolower(c)-'0')

typedef enum {opTypeInput, opTypeOutput} opType;
typedef enum {recTypeAi, recTypeAo, recTypeBi, recTypeBo,
              recTypeLi, recTypeLo, recTypeSi, recTypeSo} recType;

typedef struct devStrParmPvt {
    asynUser     *pasynUser;
    asynOctet    *pasynOctet;
    void         *octetPvt;
    opType       opType;
    recType      recType;
    asynStatus   status;
    size_t       nread;
    size_t       nwrite;
    char         buffer[100];
    int          bufferStartIndex;
    char         term[10];
    char         format[32];
    int          termlen;
    double       timeout;
    int          nchar;
    char         zerostring[32];
    char         onestring[32];
} devStrParmPvt;

typedef struct dsetStrParm{
        long      number;
        DEVSUPFUN report;
        DEVSUPFUN init;
        DEVSUPFUN init_record;
        DEVSUPFUN get_ioint_info;
        DEVSUPFUN io;
        DEVSUPFUN convert;
} dsetStrParm;

static long initCommon(dbCommon *pr, DBLINK *plink, 
                       opType ot, recType rt, char *format);
static long startIOCommon(dbCommon *pr);
static long completeIOCommon(dbCommon *pr);
static void devStrParmCallback(asynUser *pasynUser);
static long strParmConvert(dbCommon* pr,int pass);

static long initAi(aiRecord *pr);
static long readAi(aiRecord *pr);
static long initAo(aoRecord *pr);
static long writeAo(aoRecord *pr);
static long initBi(biRecord *pr);
static long readBi(biRecord *pr);
static long initBo(boRecord *pr);
static long writeBo(boRecord *pr);
static long initLi(longinRecord *pr);
static long readLi(longinRecord *pr);
static long initLo(longoutRecord *pr);
static long writeLo(longoutRecord *pr);
static long initSi(stringinRecord *pr);
static long readSi(stringinRecord *pr);
static long initSo(stringoutRecord *pr);
static long writeSo(stringoutRecord *pr);
dsetStrParm devAiStrParm = {6,0,0,initAi,0,readAi,strParmConvert};
epicsExportAddress(dset,devAiStrParm);
dsetStrParm devAoStrParm = {6,0,0,initAo,0,writeAo, strParmConvert};
epicsExportAddress(dset,devAoStrParm);
dsetStrParm devBiStrParm = {6,0,0,initBi,0,readBi,0};
epicsExportAddress(dset,devBiStrParm);
dsetStrParm devBoStrParm = {6,0,0,initBo,0,writeBo,0};
epicsExportAddress(dset,devBoStrParm);
dsetStrParm devLiStrParm = {6,0,0,initLi,0,readLi,0};
epicsExportAddress(dset,devLiStrParm);
dsetStrParm devLoStrParm = {6,0,0,initLo,0,writeLo,0};
epicsExportAddress(dset,devLoStrParm);
dsetStrParm devSiStrParm = {6,0,0,initSi,0,readSi,0};
epicsExportAddress(dset,devSiStrParm);
dsetStrParm devSoStrParm = {6,0,0,initSo,0,writeSo,0};
epicsExportAddress(dset,devSoStrParm);


static long initCommon(dbCommon *pr, DBLINK *plink, 
                       opType ot, recType rt, char *format)
{
   char *p, *port, *userParam;
   int i;
   int signal;
   asynUser *pasynUser=NULL;
   asynStatus status;
   asynInterface *pasynInterface;
   devStrParmPvt *pPvt=NULL;
   char escapeBuff[20];
   
   /* Allocate private structure */
   pPvt = calloc(1, sizeof(devStrParmPvt));
   pPvt->opType = ot;
   pPvt->recType = rt;

   /* Create an asynUser */
   pasynUser = pasynManager->createAsynUser(devStrParmCallback, 0);
   pasynUser->userPvt = pr;

   /* Parse link */
   status = pasynEpicsUtils->parseLink(pasynUser, plink,
                                       &port, &signal, &userParam);
   if (status != asynSuccess) {
      errlogPrintf("devXxStrParm::initCommon %s bad link %s\n",
                   pr->name, pasynUser->errorMessage);
      goto bad;
   }

   status = pasynManager->connectDevice(pasynUser,port,signal);
   if(status!=asynSuccess) goto bad;
   pasynInterface = pasynManager->findInterface(pasynUser,asynOctetType,1);
   if(!pasynInterface) goto bad;
   pPvt->pasynOctet = (asynOctet *)pasynInterface->pinterface;
   pPvt->octetPvt = pasynInterface->drvPvt;
   pPvt->pasynUser = pasynUser;
   pr->dpvt = pPvt;

   /* Initialize parameters */
   strcpy(pPvt->term, "\r\n");
   pPvt->termlen=2;
   /* NOTE: in the past the "signal" was used for the buffer start index.  This cannot be
    * used any more, since GPIB will use this for actual address.  Must find another way to
    * pass buffer start index
    * pPvt->bufferStartIndex = signal; */
   pPvt->bufferStartIndex = 0;
   pPvt->timeout=1.0;
   pPvt->nchar=100;
   if (pr->desc[0]) {
      strncpy(pPvt->format, pr->desc, 31);
   } else {
      strcpy(pPvt->format, format);
   }

   /* Get any configurable parameters from parm field */
   if (userParam) {
      if ((p = strstr(userParam, "TERM="))) {
         pPvt->termlen = 0;
         for (p+=5,i=0; i<9 && isxdigit(p[0]) && isxdigit(p[1]); i++,p+=2) {
            pPvt->term[i] = HEXCHAR2INT(p[0])*16 + HEXCHAR2INT(p[1]);
            pPvt->termlen++;
         }
      pPvt->term[i] = 0;
      }

      if ((p = strstr(userParam, "IX="))) {
         pPvt->bufferStartIndex = atoi(&p[3]);
      }

      if ((p = strstr(userParam, "FMT="))) {
         for (i=0, p+=4; i<31 && *p && *p != ','; i++, p++) pPvt->format[i] = *p;
         pPvt->format[i] = 0;
      }

      if ((p = strstr(userParam, "TO="))) {
         /* Timeout is specified in msec, convert to sec */
         pPvt->timeout = atof(&p[3])/1000.;
      }

      if ((p = strstr(userParam, "N="))) {
         pPvt->nchar = atoi(&p[2]);
      }

      pPvt->zerostring[0] = 0;
      pPvt->onestring[0] = 0;

      if ((p = strstr(userParam, "0STR="))) {
         for (i=0, p+=5; i<31 && *p && *p != ','; i++, p++) {
            pPvt->zerostring[i] = *p;
         }
         pPvt->zerostring[i] = 0;
      }
 
      if ((p = strstr(userParam, "1STR="))) {
         for (i=0, p+=5; i<31 && *p && *p != ','; i++, p++) {
            pPvt->onestring[i] = *p;
         }
         pPvt->onestring[i] = 0;
      }
   }

   asynPrint(pasynUser, ASYN_TRACEIO_DEVICE, 
      "devXxStrParm::initCommon %s userParam = '%s'\n", pr->name, userParam);
   epicsStrSnPrintEscaped(escapeBuff, sizeof(escapeBuff),
                          pPvt->term, pPvt->termlen);
   asynPrint(pasynUser, ASYN_TRACEIO_DEVICE, 
      "   term = %s\n", escapeBuff);
   asynPrint(pasynUser, ASYN_TRACEIO_DEVICE, 
      "   bufferStartIndex = %d\n", pPvt->bufferStartIndex);
   asynPrint(pasynUser, ASYN_TRACEIO_DEVICE, 
      "   timeout = %f\n", pPvt->timeout);
   asynPrint(pasynUser, ASYN_TRACEIO_DEVICE, 
      "   format = %s\n", pPvt->format);
   asynPrint(pasynUser, ASYN_TRACEIO_DEVICE, 
      "   nchar = %d\n", pPvt->nchar);
   return 0;

bad:
   if(status!=asynSuccess) {
       asynPrint(pasynUser,ASYN_TRACE_ERROR,
           "%s asynManager error %s\n",
           pr->name,pasynUser->errorMessage);
   }
   if(pasynUser) pasynManager->freeAsynUser(pasynUser);
   if(pPvt) free(pPvt);
   pr->pact = 1;
   return 0;
}

static long initAi(aiRecord *pr)
{
   return(initCommon((dbCommon *)pr, &pr->inp, opTypeInput, recTypeAi, "%lf"));
}

static long initAo(aoRecord *pr)
{
   return(initCommon((dbCommon *)pr, &pr->out, opTypeOutput, recTypeAo, "%f"));
}

static long initBi(biRecord *pr)
{
   return(initCommon((dbCommon *)pr, &pr->inp, opTypeInput, recTypeBi, "%d"));
}

static long initBo(boRecord *pr)
{
   return(initCommon((dbCommon *)pr, &pr->out, opTypeOutput, recTypeBo, "%d"));
}

static long initLi(longinRecord *pr)
{
   return(initCommon((dbCommon *)pr, &pr->inp, opTypeInput, recTypeLi, "%ld"));
}

static long initLo(longoutRecord *pr)
{
   return(initCommon((dbCommon *)pr, &pr->out, opTypeOutput, recTypeLo, "%d"));
}

static long initSi(stringinRecord *pr)
{
   return(initCommon((dbCommon *)pr, &pr->inp, opTypeInput, recTypeSi, "%s"));
}

static long initSo(stringoutRecord *pr)
{
   return(initCommon((dbCommon *)pr, &pr->out, opTypeOutput, recTypeSo, "%s"));
}

static long readAi(aiRecord *pr)
{
   if (!pr->pact)
      return(startIOCommon((dbCommon *)pr));
   else
      return(completeIOCommon((dbCommon *)pr));
}

static long readBi(biRecord *pr)
{
   if (!pr->pact)
      return(startIOCommon((dbCommon *)pr));
   else
      return(completeIOCommon((dbCommon *)pr));
}

static long readLi(longinRecord *pr)
{
   if (!pr->pact)
      return(startIOCommon((dbCommon *)pr));
   else
      return(completeIOCommon((dbCommon *)pr));
}

static long readSi(stringinRecord *pr)
{
   if (!pr->pact)
      return(startIOCommon((dbCommon *)pr));
   else
      return(completeIOCommon((dbCommon *)pr));
}

static long writeAo(aoRecord *pr)
{
   devStrParmPvt *pPvt = (devStrParmPvt *)pr->dpvt;
    
   if (!pr->pact) {
      sprintf(pPvt->buffer, pPvt->format, pr->val);
      return(startIOCommon((dbCommon *)pr));
   } else {
      return(completeIOCommon((dbCommon *)pr));
   }
}

static long writeBo(boRecord *pr)
{
   devStrParmPvt *pPvt = (devStrParmPvt *)pr->dpvt;
    
   if (!pr->pact) {
      sprintf(pPvt->buffer, pPvt->format, pr->val);
      return(startIOCommon((dbCommon *)pr));
   } else {
      return(completeIOCommon((dbCommon *)pr));
   }
}

static long writeLo(longoutRecord *pr)
{
   devStrParmPvt *pPvt = (devStrParmPvt *)pr->dpvt;
    
   if (!pr->pact) {
      sprintf(pPvt->buffer, pPvt->format, pr->val);
      return(startIOCommon((dbCommon *)pr));
   } else { 
      return(completeIOCommon((dbCommon *)pr));
   }
}

static long writeSo(stringoutRecord *pr)
{
   devStrParmPvt *pPvt = (devStrParmPvt *)pr->dpvt;
    
   if (!pr->pact) {
      strncpy(pPvt->buffer, &(pr->val[pPvt->bufferStartIndex]), 
              sizeof(pr->val));
      return(startIOCommon((dbCommon *)pr));
   } else { 
      return(completeIOCommon((dbCommon *)pr));
   }
}


static long startIOCommon(dbCommon* pr)
{
   devStrParmPvt *pPvt = (devStrParmPvt *)pr->dpvt;
   asynUser *pasynUser = pPvt->pasynUser;
   int status;

   pr->pact = 1;
   status = pasynManager->queueRequest(pasynUser, 0, 0);
   if (status != asynSuccess) status = -1;
   return(status);
}

static void devStrParmCallback(asynUser *pasynUser)
{
   dbCommon *pr = (dbCommon *)pasynUser->userPvt;
   devStrParmPvt *pPvt = (devStrParmPvt *)pr->dpvt;
   struct rset *prset = (struct rset *)(pr->rset);
   int eomReason;
   asynStatus status;

   pPvt->pasynUser->timeout = pPvt->timeout;
   switch(pPvt->opType) {
      case opTypeInput:
         status = pPvt->pasynOctet->setInputEos(pPvt->octetPvt, pasynUser, 
                                                pPvt->term, pPvt->termlen);
         if (status != asynSuccess) {
            asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                      "devStrParmCallback, %s error calling setInputEos %s\n",
                      pr->name, pasynUser->errorMessage);
         }
         pPvt->status = pPvt->pasynOctet->read(pPvt->octetPvt, pasynUser, 
                                               pPvt->buffer, pPvt->nchar, 
                                               &pPvt->nread, &eomReason);
         break;
      case opTypeOutput:
         status = pPvt->pasynOctet->setOutputEos(pPvt->octetPvt, pasynUser, 
                                        pPvt->term, pPvt->termlen);
         if (status != asynSuccess) {
            asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                      "devStrParmCallback, %s error calling setOutputEos %s\n",
                      pr->name, pasynUser->errorMessage);
         }
         pPvt->status = pPvt->pasynOctet->write(pPvt->octetPvt, pasynUser, 
                                                pPvt->buffer, strlen(pPvt->buffer), 
                                                &pPvt->nwrite);
         break;
   }
   /* Process the record. This will result in the readXi or writeXi routine
      being called again, but with pact=1 */
   dbScanLock(pr);
   (*prset->process)(pr);
   dbScanUnlock(pr);
}


static long completeIOCommon(dbCommon *pr)
{
   devStrParmPvt *pPvt = (devStrParmPvt *)pr->dpvt;
   asynUser *pasynUser = (asynUser *)pPvt->pasynUser;
   int rc=0;
   int i;

   switch(pPvt->opType) {
      case opTypeInput:
         asynPrint(pasynUser, ASYN_TRACE_FLOW, 
                   "devXxStrParm::completeIOCommon (%s) status=%d nread=%d\n",
                   pr->name, pPvt->status, pPvt->nread);
         asynPrintIO(pasynUser, ASYN_TRACEIO_DEVICE,
                   pPvt->buffer, pPvt->nread,
                   "devXxStrParm::completeIOCommon\n");
         /* Timeout is expected if no terminator */
         if (pPvt->termlen && pPvt->status) rc=-1;
         pPvt->buffer[pPvt->nread]='\0';
         switch(pPvt->recType) {
            case recTypeAi: {
               aiRecord *pai = (aiRecord *)pr;
               if (pPvt->nread && !rc && (pPvt->bufferStartIndex < pPvt->nread)) {
                  i = sscanf(&pPvt->buffer[pPvt->bufferStartIndex], 
                             pPvt->format, &pai->val);
                  asynPrint(pasynUser, ASYN_TRACE_FLOW, 
                     "devXxStrParm: record=%s, sscanf returns=%d, val=%f\n",
                     pai->name, i, pai->val);
                  pai->udf = (i!=1);
                  rc = 2;
               } else {
                  pai->val=0.0;
                  pai->udf = 1;
                  rc = 2;
               }
               break;
            }
            case recTypeBi: {
               biRecord *pbi = (biRecord *)pr;
               if (pPvt->nread && !rc && (pPvt->bufferStartIndex < pPvt->nread)) {
                  if (*pPvt->zerostring && 
                      !strncmp(&pPvt->buffer[pPvt->bufferStartIndex], 
                               pPvt->zerostring, strlen(pPvt->zerostring))) {
                     pbi->val = 0;
                     pbi->udf = 0;
                  }
                  if (pbi->udf && *pPvt->onestring &&
                      !strncmp(&pPvt->buffer[pPvt->bufferStartIndex], 
                               pPvt->onestring, strlen(pPvt->onestring))) {
                     pbi->val = 1;
                     pbi->udf = 0;
                  }
                  if (pbi->udf) {
                     i = sscanf(&pPvt->buffer[pPvt->bufferStartIndex], 
                                pPvt->format, &pbi->val);
                     asynPrint(pasynUser, ASYN_TRACE_FLOW, 
                        "devXxStrParm: record=%s, sscanf returns=%d, val=%d\n",
                        pbi->name, i, pbi->val);
                     pbi->udf = (i!=1);
		  }
		  rc = 0;
               } else {
                  pbi->val=0;
                  pbi->udf = 1;
                  rc = 0;
               }
               break;
            }
            case recTypeLi: {
               longinRecord *pli = (longinRecord *)pr;
               if (pPvt->nread && !rc && (pPvt->bufferStartIndex < pPvt->nread)) {
                  i = sscanf(&pPvt->buffer[pPvt->bufferStartIndex], 
                             pPvt->format, &pli->val);
                  asynPrint(pasynUser, ASYN_TRACE_FLOW, 
                     "devXxStrParm: record=%s, sscanf returns=%d, val=%f\n",
                     pli->name, i, pli->val);
                  pli->udf = (i!=1);
                  rc = 0;
               } else {
                  pli->val=0;
                  pli->udf = 1;
                  rc = 0;
               }
               break;
            }
            case recTypeSi: {
               stringinRecord *psi = (stringinRecord *)pr;
               if (pPvt->nread && !rc && (pPvt->bufferStartIndex < pPvt->nread)) {
                  char *source = &pPvt->buffer[pPvt->bufferStartIndex];
                  size_t len = strlen(source);
                  if (len >= sizeof(psi->val)) {
                      len = sizeof(psi->val)-1;
                      source[len] = 0;
                  }
                  strcpy(psi->val, source);
                  psi->udf=0;
               } else {
                  psi->val[0]='\0';
                  psi->udf=1;
               }
               break;
            }
            default:
               break;
         }
      case opTypeOutput:
         if (pPvt->status != 0) rc = -1;
         break;
   }
   return(rc);
}


static long strParmConvert(dbCommon* pr,int pass)
{
   aiRecord* pai = (aiRecord*)pr;
   pai->eslo=1.0;
   pai->roff=0;
   return 0;
}
