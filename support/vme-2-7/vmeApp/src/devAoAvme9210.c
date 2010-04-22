/*
 * devAoAvme9210 - device support routines for the Acromag 9210 analog output module
 *
 * NOTES - Joe White, SLAC, original author.
 *         Inspired by Bob Dalesio's and Marty Kraimer's Vmi4100 device support module 
 */

/*
 * Assumptions about board configuration and module_types.h
 *
 * This routine and the associated driver routine, drvAvme9210.c, assume
 * that the straight binary(+binary offset) vs. 2-complement jumper is in
 * the straight binary position.
 *
 * The output voltage range jumpers must correspond to the EGUF and EGUL
 * fields of the Ao record for that channel.
 *
 * The various Ao arrays in module_types.h and the corresponding arrays in
 * module_types.c must be set up correctly and a #define AVME9210 ? in it
 * with the ? corresponding to the array element for this card type.
 *
 * In particular, ao_num_cards[], ao_num_channels[], and ao_addrs[] must
 * be defined correctly.
 *
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<string.h>
#include        <epicsExport.h>

#include	"dbAccess.h"
#include        "recGbl.h"
#include        "alarm.h"
#include        "errlog.h"
#include	"devSup.h"
#include	"special.h"
#include	"module_types.h"
#include	"aoRecord.h"

long avme9210_write( unsigned short, unsigned short, unsigned short);

long aoAvme9210_init(int);
long aoAvme9210_init_record(struct aoRecord *pao);
long aoAvme9210_write(struct aoRecord *);
long aoAvme9210_special_linconv(struct aoRecord *, long);

struct {
  int number;
  DEVSUPFUN report;
  DEVSUPFUN init;
  DEVSUPFUN init_record;
  DEVSUPFUN get_ioint_info;
  DEVSUPFUN write;
  DEVSUPFUN special_linconv;
} devAoAvme9210 = {
  6,
  NULL,
  aoAvme9210_init,
  aoAvme9210_init_record,
  NULL,
  aoAvme9210_write,
  aoAvme9210_special_linconv
};
epicsExportAddress(dset, devAoAvme9210);

long aoAvme9210_init(int after)
/*-----------------------------------------------------------------------------
 *  Called twice during IOC initialization for module setup.
 * after=0 called before database records are initialized
 * after=1 called after database records were initialized
 *----------------------------------------------------------------------------*/
{
  return 0;
}

long aoAvme9210_init_record(struct aoRecord *pao)
/*-----------------------------------------------------------------------------
 *  Called once for each record
 *----------------------------------------------------------------------------*/
{
  struct vmeio 	*pvmeio;
  unsigned short  value;

  pvmeio = (struct vmeio *)&(pao->out.value);
  value = pao->eguf + pao->egul ? 0 : 0x800;
  avme9210_write(pvmeio->card,pvmeio->signal,value);

  switch (pao->out.type) {
  case (VME_IO):
    pao->eslo = (pao->eguf - pao->egul)/4095.0;
    return 2;
  default:
    recGblRecordError(S_db_badField,(void *)pao,"devAoAvme9210 (init_record) Illegal OUT field");
    return(S_db_badField);
  }
}

long aoAvme9210_write(struct aoRecord *pao)
/*-----------------------------------------------------------------------------
 *
 *----------------------------------------------------------------------------*/
{
  struct vmeio 	*pvmeio;
  unsigned short  value;

  pvmeio = (struct vmeio *)&(pao->out.value);
  value = pao->rval;
  switch (avme9210_write(pvmeio->card,pvmeio->signal,value)){
  case 0:
    pao->rbv = value;
    break;
  case -1:
    if(recGblSetSevr(pao,WRITE_ALARM,INVALID_ALARM) && errVerbose&& (pao->stat!=WRITE_ALARM || pao->sevr!=INVALID_ALARM))
      recGblRecordError(-1,(void *)pao,"avme9210_write Error");
    break;
  default:
    recGblSetSevr(pao,HW_LIMIT_ALARM,INVALID_ALARM);
  }
  return 0;
}

long aoAvme9210_special_linconv(struct aoRecord *pao, long after)
/*-----------------------------------------------------------------------------
 *  set new linear conversion slope since something changed
 *----------------------------------------------------------------------------*/
{
  if(!after) return(0);
  pao->eslo = (pao->eguf - pao->egul)/4095.0;
  return(0);
}

