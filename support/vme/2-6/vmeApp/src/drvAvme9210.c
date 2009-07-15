/* $ID$ */
/*
 * drvAvme9210 - driver routines for the AVME 9210 Analog Output VME Module
 *
 * NOTES - Joe White, SLAC, original author.
 *         Inspired by Bob Dalesio's Vmi4100 driver module.
 *   
 * HISTORY
 *   Mepics - Jan 18, 1995: Created.
 *   See Changelog.
 *   Mark Rivers, July 23, 2001  Removed module_types.h, added configuration function
 *                               drvAvme9210Config(maxCards)
 *   Mark Rivers, October 22, 2001 Changed code so no cards are assumed to be
 *                               present unless drvAvms9201Config is called
 */

#include <vxWorks.h>
#include <vxLib.h>
#include <vme.h>
#include <sysLib.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <epicsExport.h>

#include "dbDefs.h"
#include "drvSup.h"

/* led status bit definitions for the csr on each board */
LOCAL const char AVME9210_RESET = 0x10;
LOCAL const char AVME9210_INACT = 1;
LOCAL const char AVME9210_TEST  = 2;
LOCAL const char AVME9210_OK    = 3;

/* global routines called from startup script */
void avme9210Config(int mCards, int mChan, int address);

/* global routines called directly by device support */
long avme9210_write( unsigned short, unsigned short, unsigned short);
long avme9210_read( unsigned short, unsigned short, unsigned short*);

/* global routines called implicitly through driver entry table */
long avme9210_report(long);
long avme9210_init();
long avme9210_reboot();

struct {
   long number;
   DRVSUPFUN report;
   DRVSUPFUN init;
   DRVSUPFUN reboot;
} drvAvme9210 ={
  3,
  avme9210_report,
  avme9210_init,
  avme9210_reboot
};
epicsExportAddress(drvet, drvAvme9210);

/* memory structure of the 9210, total length is 1 KB */
typedef struct avme9210_s {
  unsigned short ModuleIdProm[32];   /* module ID string (ascii loaded into odd bytes only) */
  unsigned short undefined1[32];     /* byte offsets 40->7F are unused */
  unsigned char  dummy;
  unsigned char  csr;                /* control and status register */
  unsigned short data[8];            /* data registers in channel order */
  unsigned short undefined2[439];    /* rounds out module's footprint to 1 kByte */
} avme9210_t;

LOCAL avme9210_t **pCardList;
LOCAL avme9210_t *avme9210_addrs;
LOCAL char *initString = "VMEIDACR9210   1 20 ";

/* variables which can be configures */
LOCAL maxCards=0;
LOCAL maxChan=8;
LOCAL moduleAddress=0x3000;

/* module scope routines to do some useful work */
/* LOCAL long avme9210_zeroOutputs( avme9210_t*); */
LOCAL long avme9210_testCard(avme9210_t*);

void avme9210Config(int mCards, int mChan, int address)
{
   maxCards = mCards;
   maxChan = mChan;
   moduleAddress = address;
}

long avme9210_init(void)
/*----------------------------------------------------------------------------*
 * standard EPICS driver support initialization routine for the acromag 9210
 * 8 channel, 12 bit dac analog output VME card.
 *----------------------------------------------------------------------------*/
{
  register avme9210_t** pcl;
  register avme9210_t* pcard;
  register int i;

  if(pCardList == NULL){
    pCardList = (avme9210_t **) calloc(maxCards,sizeof( avme9210_t*));
    if(pCardList==NULL){
      /* might want a memory allocation error message here */
      return ERROR;
    }
  }
  pcl = pCardList;
  
  if(sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO,
      (char*)moduleAddress, (char**)&avme9210_addrs) != OK){
    printf("Addressing error in avme9210 driver initialization \n");
    return ERROR;
  }
  pcard = avme9210_addrs;

  for( i=0; i< maxCards; i++, pcard++, pcl++){
    if(avme9210_testCard(pcard)){
      *pcl = pcard;
      pcard->csr = AVME9210_OK;
/*      avme9210_zeroOutputs(pcard); */
    }
    else
      *pcl = NULL;
  }
  return OK;  
}

long avme9210_report(long L)
/*----------------------------------------------------------------------------*
 *  loop over cards and report how many are present and if init was ok
 *----------------------------------------------------------------------------*/
{
  register int i;

  if(L < 1)
    return OK;

  for ( i=0; i < maxCards; i++){
    if( pCardList[i] ){
      printf("Avme9210:\tcard %d, addr=0x%08x\n", i, (unsigned)pCardList[i]);
      if(L > 1)
        printf("\t\tCSR: 0x%02x\n", pCardList[i]->csr);
    }
  }
  return OK;
}

long avme9210_reboot(void)
/*----------------------------------------------------------------------------*
 *  loop over cards and do a reset on each one
 *  also zero the output voltages
 *----------------------------------------------------------------------------*/
{
  register int i;
  register volatile avme9210_t *p;
  for ( i=0; i < maxCards; i++){
    if( (p=pCardList[i]) ){
      p->csr=AVME9210_RESET;
/*      avme9210_zeroOutputs(p); */
    }
  }
  return OK;
}

long avme9210_write( unsigned short card,unsigned short chan, unsigned short value)
/*----------------------------------------------------------------------------*
 * Drive the output of a particular channel. This card has 12 bit dac's but uses
 * the upper 12 bits of the data word so the value is shifted in this routine.
 * The routine should be called with the unshifted dac count. device support
 * should handle the conversion between voltage values and dac counts.
 *----------------------------------------------------------------------------*/
{
  register volatile avme9210_t *p = pCardList[card];
  if(p == NULL) return -1;
  p->data[chan] = value <<4;
  return 0;
}

long avme9210_read( unsigned short card, unsigned short chan,unsigned short *pval)
/*----------------------------------------------------------------------------*
 *  Note: This card does not return a meaningful value when it is read.
 *  all data channels will return 0xFFFF on reads except for channel 0 after
 *  reset and before it is changed. it will read 0xFFF3 at those times.
 *----------------------------------------------------------------------------*/
{
  register avme9210_t *p = pCardList[card];
  if(pval == NULL) return -1;
  if(p == NULL) {
    *pval = 0;
    return -1;
  }
  *pval = p->data[chan];
  return 0; 
}

#if 0
LOCAL long avme9210_zeroOutputs( avme9210_t *pc)
/*----------------------------------------------------------------------------*
 * Set the voltage output to zero for all channels in a module
 * note this routine is for internal use only and references the modules by
 * address pointer and not module number
 *----------------------------------------------------------------------------*/
{
  register i;
  if(pc==NULL) return -1;
  for(i=0; i<maxChan; i++)
    pc->data[i] = 0;
  return 0;
}
#endif

LOCAL long avme9210_testCard(avme9210_t *pcard)
/*----------------------------------------------------------------------------*
 * see if this card exists and is the correct kind of card by testing the
 * module's ID prom
 *----------------------------------------------------------------------------*/
{
  register char saveLED=AVME9210_INACT;
  register int i=0;
  char id[32];
  if( vxMemProbe( (char*)pcard, VX_READ, 2, id) == ERROR )
    return 0;
  saveLED = pcard->csr & 3;
  pcard->csr = AVME9210_TEST;
  while ( (id[i] = (char)(pcard->ModuleIdProm[i]) )) {
    i++;
  }
  if(strncmp(id, initString, (size_t)13))
    return 0;
  pcard->csr = saveLED;
  return 1;
}

