/* devABSLCDCM.c */
/*
 *      Original Author: Ric Claus
 *      Current Author:  Stephanie Allison
 *      Date:  1/25/96
 */

/*************************************************************************
* Copyright (c) 2003 The University of Chicago, as Operator of Argonne
* National Laboratory, the Regents of the University of California, as
* Operator of Los Alamos National Laboratory, and Leland Stanford Junior
* University, as the Operator of Stanford Linear Accelerator.
* This code is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
*************************************************************************/

#include <vxWorks.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "alarm.h"
#include "cvtTable.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "drvAb.h"
#include "dbCommon.h"
#include "longinRecord.h"
#include "longoutRecord.h"
#include "aiRecord.h"
#include "aoRecord.h"
#include <epicsExport.h>

typedef struct
{
  void		*drvPvt;
  IOSCANPVT	ioscanpvt;
}devPvt;



/* Create the dsets*/
LOCAL long ioinfo  (int cmd, struct dbCommon *prec, IOSCANPVT *ppvt);
LOCAL long read_li (struct longinRecord *prec);
LOCAL long init_li (struct longinRecord *prec);
typedef struct
{
  long		number;
  DEVSUPFUN	report;
  DEVSUPFUN	init;
  DEVSUPFUN	init_record;
  DEVSUPFUN	get_ioint_info;
  DEVSUPFUN	read_li;
}  ABLIDSET;

ABLIDSET devLiAbSlcDcm = {5, NULL, NULL, init_li, ioinfo, read_li};
epicsExportAddress(dset,devLiAbSlcDcm);

LOCAL long write_lo (struct longoutRecord *prec);
LOCAL long init_lo  (struct longoutRecord *prec);
typedef struct
{
  long		number;
  DEVSUPFUN	report;
  DEVSUPFUN	init;
  DEVSUPFUN	init_record;
  DEVSUPFUN	get_ioint_info;
  DEVSUPFUN	write_dl;
} ABLODSET;

ABLODSET devLoAbSlcDcm = {5, NULL, NULL, init_lo, NULL, write_lo};
epicsExportAddress(dset,devLoAbSlcDcm);

LOCAL long read_ai(struct aiRecord *prec);
LOCAL long init_ai(struct aiRecord *prec);
LOCAL long linconv_ai(struct aiRecord *prec, int after);
typedef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_ai;
	DEVSUPFUN	special_linconv;} ABAIDSET;

ABAIDSET devAiAbSlcDcm = {6, NULL, NULL, init_ai, ioinfo, read_ai, 
                          linconv_ai};
epicsExportAddress(dset,devAiAbSlcDcm);

LOCAL long read_signed_ai(struct aiRecord *prec);
LOCAL long init_signed_ai(struct aiRecord *prec);
ABAIDSET devAiAbSlcDcmSigned = {6, NULL, NULL, init_signed_ai, ioinfo, read_signed_ai, 
                                linconv_ai};
epicsExportAddress(dset,devAiAbSlcDcmSigned);

LOCAL long write_ao(struct aoRecord *prec);
LOCAL long init_ao (struct aoRecord *prec);
LOCAL long linconv_ao(struct aoRecord *prec, int after);
typedef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_ao;
	DEVSUPFUN	special_linconv;} ABAODSET;

ABAODSET devAoAbSlcDcm = {6, NULL, NULL, init_ao, NULL, write_ao, 
                          linconv_ao};
epicsExportAddress(dset,devAoAbSlcDcm);


LOCAL void devCallback (void * drvPvt)
{
  devPvt      *pdevPvt;

  pdevPvt = (devPvt *) (*pabDrv->getUserPvt) (drvPvt);
  if (!pdevPvt) return;
  if (!pdevPvt->ioscanpvt) return;
  scanIoRequest (pdevPvt->ioscanpvt);
}

LOCAL long ioinfo (int               cmd,
                    struct dbCommon  *prec,
                    IOSCANPVT        *ppvt)
{
  devPvt      *pdevPvt;

  pdevPvt = prec->dpvt;
  if (!pdevPvt) return(0);
  *ppvt = pdevPvt->ioscanpvt;
  return (0);
}



LOCAL long read_li (struct longinRecord *prec)
{
  devPvt        *pdevPvt = (devPvt *) prec->dpvt;
  void          *drvPvt;
  abStatus       drvStatus;
  short          value[2];

  if (!pdevPvt)
  {
    recGblSetSevr (prec, READ_ALARM, INVALID_ALARM);
    return (2);
  }
  drvPvt    = pdevPvt->drvPvt;
  drvStatus = (*pabDrv->readBi) (drvPvt, (unsigned long *)value, 0x0000ffff);
  if (drvStatus != abSuccess)
  {
    recGblSetSevr (prec, READ_ALARM, INVALID_ALARM);
    return (2);
  }
  prec->val = (long)value[1];

  return(0);
}



LOCAL long init_li (struct longinRecord *prec)
{
  struct abio   *pabio;
  devPvt        *pdevPvt;
  abStatus       drvStatus;
  long	         status = 0;
  void          *drvPvt;

  if (prec->inp.type != AB_IO)
  {
    recGblRecordError (S_db_badField, (void *) prec, "init_li: Bad INP field");
    return (S_db_badField);
  }

  pabio = (struct abio *) &(prec->inp.value);
  drvStatus = (*pabDrv->registerCard) (pabio->link, pabio->adapter, pabio->card,
                                       typeBi, "BINARY", devCallback, &drvPvt);
  switch (drvStatus)
  {
  case abSuccess :
    pdevPvt    = (devPvt *) (*pabDrv->getUserPvt) (drvPvt);
    prec->dpvt = pdevPvt;
    break;

  case abNewCard :
    pdevPvt = calloc (1, sizeof (devPvt));
    pdevPvt->drvPvt = drvPvt;
    prec->dpvt      = pdevPvt;
    (*pabDrv->setUserPvt) (drvPvt, (void *) pdevPvt);
    scanIoInit (&pdevPvt->ioscanpvt);
    drvStatus = (*pabDrv->setNbits) (drvPvt, abBit16);
    if (drvStatus != abSuccess)
    {
      status = S_db_badField;
      recGblRecordError (status, (void *) prec, "init_li: setNbits");
    }
    break;

  default:
    status = S_db_badField;
    recGblRecordError (status, (void *)prec, "init_li: registerCard");
    break;
  }

  return (status);
}



LOCAL long write_lo (struct longoutRecord *prec)
{
  devPvt        *pdevPvt = (devPvt *) prec->dpvt;
  void          *drvPvt;
  abStatus       drvStatus;

  if (!pdevPvt)
  {
    recGblSetSevr (prec, WRITE_ALARM, INVALID_ALARM);
    return (0);
  }

  drvPvt    = pdevPvt->drvPvt;
  drvStatus = (*pabDrv->updateBo) (drvPvt, prec->val, 0x0000ffff);
  if (drvStatus != abSuccess)
  {
    recGblSetSevr (prec, WRITE_ALARM, INVALID_ALARM);
  }

  return(0);
}



LOCAL long init_lo (struct longoutRecord *prec)
{
  struct abio   *pabio;
  devPvt        *pdevPvt;
  abStatus       drvStatus;
  long           status = 0;
  void          *drvPvt;
  short          value[2];

  if (prec->out.type != AB_IO)
  {
    recGblRecordError (S_db_badField, (void *) prec, "init_lo: Bad OUT field");
    return (S_db_badField);
  }

  pabio = (struct abio *) &(prec->out.value);
  drvStatus = (*pabDrv->registerCard) (pabio->link, pabio->adapter, pabio->card,
                                       typeBo, "BINARY", NULL, &drvPvt);
  switch (drvStatus)
  {
  case abSuccess :
    pdevPvt    = (devPvt *) (*pabDrv->getUserPvt) (drvPvt);
    prec->dpvt = pdevPvt;
    break;

  case abNewCard :
    pdevPvt = calloc (1, sizeof (devPvt));
    pdevPvt->drvPvt = drvPvt;
    prec->dpvt      = pdevPvt;
    (*pabDrv->setUserPvt) (drvPvt, (void *) pdevPvt);
    drvStatus = (*pabDrv->setNbits) (drvPvt, abBit16);
    if (drvStatus != abSuccess)
    {
      status = S_db_badField;
      recGblRecordError (status, (void *) prec, "init_lo: setNbits");
      return (status);
    }
    break;

  default:
    status = S_db_badField;
    printf ("init_lo: %s\n", abStatusMessage[drvStatus]);
    recGblRecordError (status, (void *) prec, "init_lo: registerCard");
    return (status);
  }

  drvStatus = (*pabDrv->readBo) (drvPvt, (unsigned long *)value, 0x0000ffff);
  if (drvStatus == abSuccess)
  {
    prec->val = (long)value[1];
    status = 0;
  }
  else
  {
    status = 2;
  }

  return (status);
}



LOCAL long read_ai (struct aiRecord *prec)
{
  devPvt        *pdevPvt = (devPvt *) prec->dpvt;
  void          *drvPvt;
  abStatus       drvStatus;
  short          value[2];

  if (!pdevPvt)
  {
    recGblSetSevr (prec, READ_ALARM, INVALID_ALARM);
    return (2);
  }
  drvPvt    = pdevPvt->drvPvt;
  drvStatus = (*pabDrv->readBi) (drvPvt, (unsigned long *)value, 0x0000ffff);
  if (drvStatus != abSuccess)
  {
    recGblSetSevr (prec, READ_ALARM, INVALID_ALARM);
    return (2);
  }
  prec->rval = (long)value[1];

  return(0);
}

LOCAL long linconv_ai(struct aiRecord *prec, int after)
{

    if(!after) return(0);
    /* set linear conversion slope*/
    prec->eslo = (prec->eguf -prec->egul)/65535.0;
    return(0);
}



LOCAL long init_ai (struct aiRecord *prec)
{
  struct abio   *pabio;
  devPvt        *pdevPvt;
  abStatus       drvStatus;
  long	         status = 0;
  void          *drvPvt;

  if (prec->inp.type != AB_IO)
  {
    recGblRecordError (S_db_badField, (void *) prec, "init_ai: Bad INP field");
    return (S_db_badField);
  }

  /* set linear conversion slope*/
  prec->eslo = (prec->eguf -prec->egul)/65535.0;
  /* pointer to the data addess structure */
  pabio = (struct abio *) &(prec->inp.value);
  drvStatus = (*pabDrv->registerCard) (pabio->link, pabio->adapter, pabio->card,
                                       typeBi, "BINARY", devCallback, &drvPvt);
  switch (drvStatus)
  {
  case abSuccess :
    pdevPvt    = (devPvt *) (*pabDrv->getUserPvt) (drvPvt);
    prec->dpvt = pdevPvt;
    break;

  case abNewCard :
    pdevPvt = calloc (1, sizeof (devPvt));
    pdevPvt->drvPvt = drvPvt;
    prec->dpvt      = pdevPvt;
    (*pabDrv->setUserPvt) (drvPvt, (void *) pdevPvt);
    scanIoInit (&pdevPvt->ioscanpvt);
    drvStatus = (*pabDrv->setNbits) (drvPvt, abBit16);
    if (drvStatus != abSuccess)
    {
      status = S_db_badField;
      recGblRecordError (status, (void *) prec, "init_ai: setNbits");
    }
    break;

  default:
    status = S_db_badField;
    recGblRecordError (status, (void *)prec, "init_ai: registerCard");
    break;
  }

  return (status);
}



LOCAL long write_ao (struct aoRecord *prec)
{
  devPvt        *pdevPvt = (devPvt *) prec->dpvt;
  void          *drvPvt;
  abStatus       drvStatus;

  if (!pdevPvt)
  {
    recGblSetSevr (prec, WRITE_ALARM, INVALID_ALARM);
    return (0);
  }

  drvPvt    = pdevPvt->drvPvt;
  drvStatus = (*pabDrv->updateBo) (drvPvt, prec->rval, 0x0000ffff);
  if (drvStatus != abSuccess)
  {
    recGblSetSevr (prec, WRITE_ALARM, INVALID_ALARM);
  }

  return(0);
}

LOCAL long linconv_ao(struct aoRecord *prec, int after)
{

    if(!after) return(0);
    /* set linear conversion slope*/
    prec->eslo = (prec->eguf -prec->egul)/32767.0;
    return(0);
}



LOCAL long init_ao (struct aoRecord *prec)
{
  struct abio   *pabio;
  devPvt        *pdevPvt;
  abStatus       drvStatus;
  long           status = 0;
  void          *drvPvt;
  short          value[2];

  if (prec->out.type != AB_IO)
  {
    recGblRecordError (S_db_badField, (void *) prec, "init_ao: Bad OUT field");
    return (S_db_badField);
  }

  /* set linear conversion slope*/
  prec->eslo = (prec->eguf -prec->egul)/32767.0;
  /* pointer to the data addess structure */
  pabio = (struct abio *) &(prec->out.value);
  drvStatus = (*pabDrv->registerCard) (pabio->link, pabio->adapter, pabio->card,
                                       typeBo, "BINARY", NULL, &drvPvt);
  switch (drvStatus)
  {
  case abSuccess :
    pdevPvt    = (devPvt *) (*pabDrv->getUserPvt) (drvPvt);
    prec->dpvt = pdevPvt;
    break;

  case abNewCard :
    pdevPvt = calloc (1, sizeof (devPvt));
    pdevPvt->drvPvt = drvPvt;
    prec->dpvt      = pdevPvt;
    (*pabDrv->setUserPvt) (drvPvt, (void *) pdevPvt);
    drvStatus = (*pabDrv->setNbits) (drvPvt, abBit16);
    if (drvStatus != abSuccess)
    {
      status = S_db_badField;
      recGblRecordError (status, (void *) prec, "init_ao: setNbits");
      return (status);
    }
    break;

  default:
    status = S_db_badField;
    printf ("init_ao: %s\n", abStatusMessage[drvStatus]);
    recGblRecordError (status, (void *) prec, "init_ao: registerCard");
    return (status);
  }

  drvStatus = (*pabDrv->readBo) (drvPvt, (unsigned long *)value, 0x0000ffff);
  if (drvStatus == abSuccess)
  {
    prec->rval = (long)value[1];
    status = 0;
  }
  else
  {
    status = 2;
  }

  return (status);
}



LOCAL long read_signed_ai (struct aiRecord *prec)
{
  devPvt        *pdevPvt = (devPvt *) prec->dpvt;
  void          *drvPvt;
  abStatus       drvStatus;
  unsigned short value[2];

  if (!pdevPvt)
  {
    recGblSetSevr (prec, READ_ALARM, INVALID_ALARM);
    return (2);
  }
  drvPvt    = pdevPvt->drvPvt;
  drvStatus = (*pabDrv->readBi) (drvPvt, (unsigned long *)value, 0x0000ffff);
  if (drvStatus != abSuccess)
  {
    recGblSetSevr (prec, READ_ALARM, INVALID_ALARM);
    return (2);
  }
  prec->rval = ((long)value[1])^0x8000;

  return(0);
}



LOCAL long init_signed_ai (struct aiRecord *prec)
{
  struct abio   *pabio;
  devPvt        *pdevPvt;
  abStatus       drvStatus;
  long	         status = 0;
  void          *drvPvt;

  if (prec->inp.type != AB_IO)
  {
    recGblRecordError (S_db_badField, (void *) prec, "init_signed_ai: Bad INP field");
    return (S_db_badField);
  }

  /* set linear conversion slope*/
  prec->eslo = (prec->eguf -prec->egul)/65535.0;
  /* pointer to the data addess structure */
  pabio = (struct abio *) &(prec->inp.value);
  drvStatus = (*pabDrv->registerCard) (pabio->link, pabio->adapter, pabio->card,
                                       typeBi, "BINARY", devCallback, &drvPvt);
  switch (drvStatus)
  {
  case abSuccess :
    pdevPvt    = (devPvt *) (*pabDrv->getUserPvt) (drvPvt);
    prec->dpvt = pdevPvt;
    break;

  case abNewCard :
    pdevPvt = calloc (1, sizeof (devPvt));
    pdevPvt->drvPvt = drvPvt;
    prec->dpvt      = pdevPvt;
    (*pabDrv->setUserPvt) (drvPvt, (void *) pdevPvt);
    scanIoInit (&pdevPvt->ioscanpvt);
    drvStatus = (*pabDrv->setNbits) (drvPvt, abBit16);
    if (drvStatus != abSuccess)
    {
      status = S_db_badField;
      recGblRecordError (status, (void *) prec, "init_signed_ai: setNbits");
    }
    break;

  default:
    status = S_db_badField;
    recGblRecordError (status, (void *)prec, "init_signed_ai: registerCard");
    break;
  }

  return (status);
}


