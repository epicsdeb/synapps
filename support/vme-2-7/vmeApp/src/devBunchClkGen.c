/*+ devBunchClkGen.c

                          Argonne National Laboratory
                            APS Operations Division
                     Beamline Controls and Data Acquisition

                         BCG-100 Driver / Device Support



 -----------------------------------------------------------------------------
                                COPYRIGHT NOTICE
 -----------------------------------------------------------------------------
   Copyright (c) 2002 The University of Chicago, as Operator of Argonne
      National Laboratory.
   Copyright (c) 2002 The Regents of the University of California, as
      Operator of Los Alamos National Laboratory.
   Synapps Versions 4-5
   and higher are distributed subject to a Software License Agreement found
   in file LICENSE that is included with this distribution.
 -----------------------------------------------------------------------------

 Description
   This module provides device support for the BCG-100 VMEbus module. The
   module is found in the A16 VME address space and supports the ai, ao, bi,
   bo, and waveform record types.

   Diagnostic messages can be output by setting the variables to the following
   values:
      drvBunchClkGenDebug  = 0,  outputs no messages.
      drvBunchClkGenDebug  > 0,  outputs driver messages.

      devBunchClkGenDebug  = 0,  outputs no messages.
      devBunchClkGenDebug  > 0,  outputs device messages.

   The method BunchClkGenConfigure is called from the startup script to specify the
   card number and VME base address. It must be called prior to iocInit(). Below is
   the calling sequence:

      BunchClkGenConfigure( card, base )

      Where:
         card     - Card number.
         base     - VME A16 address space.

      For example:
         BunchClkGenConfigure(0, 0x7000)

 Developer notes:
   1) More formatting and commenting should be done as well as porting the
      driver / device support to EPICS base >= 3.14.6.

 =============================================================================
 History:
 Author: Frank Lenkszus
 -----------------------------------------------------------------------------
 14-11-95   FRL   - Initial.
 22-01-05   DMK   - Took over support for driver / device support.
                  - Reformatted and documented for clarity.
 26-01-05   DMK   - Made OS independent by employing EPICS devLib methods.
 -----------------------------------------------------------------------------

-*/

/* System related include files */
#include <math.h>
#include <types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* EPICS system related include files */
#include <alarm.h>
#include <devLib.h>
#include <devSup.h>
#include <drvSup.h>
#include <recGbl.h>
#include <epicsPrint.h>
#include <epicsExport.h>


/* EPICS record processing related include files */
#include <dbScan.h>
#include <dbAccess.h>
#include <aiRecord.h>
#include <aoRecord.h>
#include <biRecord.h>
#include <boRecord.h>
#include <waveformRecord.h>


/* Define general symbolic constants */
#define MAX_NUM_CARDS   ( 4 )
#define NO_ERR_RPT      ( -1 )
#define FBUSERR         ( -2 )
#define MAX_SIGNAL      ( 4 )
#define PWR_CYCLED_BIT  ( 0x8 )

#define FREG_READ       ( 1 )
#define FCSR_READ       ( 2 )
#define FREG_WRITE      ( 3 )
#define FREG_WRITEBITS  ( 4 )
#define FREG_READLONG   ( 5 )
#define FREG_WRITELONG  ( 6 )

int drvBunchClkGenDebug = 0;
static int* drvDebug = &drvBunchClkGenDebug;
epicsExportAddress(int, drvBunchClkGenDebug);

static long drvInitCard();
static long drvIoReport(int);
static long check_card(USHORT, USHORT);
static long drvReadCard(short, short, short, ULONG*);
static long drvWriteCardBit(USHORT, USHORT, USHORT, USHORT, USHORT);
static long drvReadRam(short);
static long drvCopyRam(short);
static long drvClearRam(short);

/* Epics driver entry point table */
drvet drvBunchClkGen = { 2, drvIoReport, drvInitCard };
epicsExportAddress(drvet, drvBunchClkGen);

static char* drvName = "drvBunchClkGen";

/* device register maps */
struct drv_regr
{
   USHORT csr;
   USHORT address;
   USHORT data;
   USHORT fineDelay;
   USHORT P0Delay;
};

struct drv_regw
{
   USHORT csr;
   USHORT address;
   USHORT data;
   USHORT fineDelay;
   USHORT P0Delay;
};

struct drvCard
{
   union
   {
      struct drv_regr r;
      struct drv_regw w;
   } reg;
};

/* Define all per-card private variables */
struct drvPrivate
{
   volatile struct drvCard* dptr;
   epicsMutexId lock;
   int init_hw;
   USHORT ramW[1296];
   USHORT ramR[1296];
};

static int NumCards           = 0; /* User configurable # of cards */
static int ConfigureLock      = 0;

static struct drvPrivate Card[MAX_NUM_CARDS];
static struct drvPrivate* dio = Card;


/****************************************************************************
 *
 * Begin public interface
 *
 ****************************************************************************/


/****************************************************************************/
int BunchClkGenConfigure( int card, ULONG base )
{
USHORT   junk;
char*    xname = "BunchClkGenConfigure";

   if( ConfigureLock != 0 )
   {
      errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "%s: Cannot change configuration after init -- Request ignored", xname );
      return( ERROR );
   }

   if( card >= MAX_NUM_CARDS || card < 0 )
   {
      errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "%s: Card number %d invalid -- must be 0 to %d", xname, card, MAX_NUM_CARDS-1 );
      return( ERROR );
   }

   if( base > 0xffff )
   {
      errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "%s: Card (%d)  address 0x%lx invalid -- must be 0 to 0xffff", xname, card, base );
      return( ERROR );
   }

   if( devRegisterAddress( "devBunchClockGen", atVMEA16, base, sizeof(struct drvCard), (volatile void**)&dio[card].dptr ) )
   {
      dio[card].dptr = NULL;
      errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "%s: A16 Address map failed for Card %d", xname, card );

      return( ERROR );
   }

   if( devReadProbe( sizeof(junk), (char*)dio[card].dptr, (char*)&junk ) )
   {
      dio[card].dptr = NULL;
      errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "%s: vxMemProbe failed for Card %d", xname, card );

      return( ERROR );
   }

   if( card >= NumCards )
   {
      NumCards = card + 1;
   }

   return( OK );

} /* BunchClkGenConfigure() */


/******************************************************************************
 *
 * Begin driver support
 *
 *****************************************************************************/

/****************************************************************************/
static long drvInitCard( )
{
long     status = 0;
int      i;
USHORT   val;

   ConfigureLock = 1;   /* prevent Configure calls after init */

   for( i = 0; i < NumCards; i++ )
   {
      if( check_card( i, 0 ) == OK )
      {
         dio[i].lock = epicsMutexMustCreate();
         val = dio[i].dptr->reg.r.csr;

         if( (val & PWR_CYCLED_BIT) == 0 )
         {
            /* Indicate power cycled */
            dio[i].dptr->reg.w.csr  = val | PWR_CYCLED_BIT;
            dio[i].init_hw          = 0;

            /* Clear driver & hardware RAM */
            drvClearRam( i );
         }
         else
         {
            /* Indicate not power cycled */
            dio[i].init_hw          = 1;

            /* Copy hardware RAM to driver READ RAM to driver WRITE RAM */
            drvReadRam( i );
            drvCopyRam( i );
         }

      }

   }

   return( status );

} /* drvInitCard() */


/****************************************************************************/
static long drvIoReport( int level )
{
int i;

   for( i = 0; i < MAX_NUM_CARDS; i++ )
   {
      if( dio[i].dptr )
      {
         printf( "%s:\tcard %d\tcsr = 0x%x, P0 delay = 0x%x, fine Delay = 0x%x\n",
                  drvName, i,
                  dio[i].dptr->reg.r.csr,
                  dio[i].dptr->reg.r.P0Delay,
                  dio[i].dptr->reg.r.fineDelay);
      }
   }

   return( OK );

} /* drvIoReport() */


/****************************************************************************/
static long check_card( USHORT card, USHORT signal )
{

   if( card >= MAX_NUM_CARDS )
   {
      if( *drvDebug )
      {
         errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "%s: Bad Card %d", drvName, card );
      }

      return( ERROR );
   }

   if( signal > MAX_SIGNAL )
   {
      if( *drvDebug )
      {
         errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "%s: Bad Signal %d for card %d", drvName, signal, card );
      }

      return( ERROR );
   }

   if( dio[card].dptr == NULL )
   {
      if( *drvDebug )
      {
         errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "%s: Can't access card %d", drvName, card );
      }

      return( ERROR );
   }

   return( OK );

} /* check_card() */


/***************************************************************************
 *
 *   Perform register writes
 *
 ***************************************************************************/
static long drvWriteCard( short card, short funct, short signal, ULONG* pp1 )
{
ULONG status;

   if( (status = check_card( card, signal )) != OK )
   {
      return( status );
   }

   switch( funct )
   {
   case FREG_WRITE:
      *((USHORT*)dio[card].dptr + signal) = *pp1;
      break;

   case FREG_WRITELONG:
      *((USHORT*)dio[card].dptr + signal)     = (USHORT)(*pp1);
      *((USHORT*)dio[card].dptr + signal + 1) = (USHORT)(*pp1 >> 16);
      break;

   default:
      if( *drvDebug )
      {
         errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "%s: Unknown write function", drvName );
      }
      break;
   }

   return( OK );

} /* drvWriteCard() */


/***************************************************************************
 *
 *   Perform register reads
 *
 ***************************************************************************/
static long drvReadCard( short card, short funct, short signal, ULONG* pp1 )
{
USHORT   val,
         val1;
ULONG    status;

   if( (status = check_card( card, signal )) != OK )
   {
      return( status );
   }

   switch( funct )
   {
   case FCSR_READ:
      *pp1 = *((USHORT*)dio[card].dptr);
      break;

   case FREG_READ:
      *pp1 = *((USHORT*)dio[card].dptr + signal);
      break;

   case FREG_READLONG:
      val  = *((USHORT*)dio[card].dptr + signal);
      val1 = *((USHORT*)dio[card].dptr + signal + 1);
      *pp1 = (((ULONG)val1) << 16) + val;
      break;

   default:
      if( *drvDebug )
      {
         errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "%s: Unknown read function", drvName );
      }
      break;
   }

   return( OK );

} /* drvReadCard() */


/***************************************************************************
 *
 *   write selected bits in a register
 *
 ***************************************************************************/
static long drvWriteCardBit(  USHORT card,
                              USHORT funct,
                              USHORT signal,
                              USHORT value,      /* mask */
                              USHORT mask        /* value */
                           )
{
USHORT   tmp;
ULONG    status;

   if( (status = check_card( card, signal )) != OK )
   {
      return( status );
   }

   switch( funct )
   {
   case FREG_WRITEBITS:
      epicsMutexMustLock( dio[card].lock );
      tmp = *((USHORT*)dio[card].dptr + signal);
      tmp = (tmp & ~(mask)) | value;
      *((USHORT*)dio[card].dptr + signal) = tmp;
      epicsMutexUnlock( dio[card].lock );
      break;

   default:
      if( *drvDebug )
      {
         errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "%s: Unknown read function", drvName );
      }
      return( ERROR );
   }

   if( *drvDebug )
   {
      errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "%s: Wrote 0x%x (mask = 0x%x) to card %d, signal %d", drvName, value, mask, card, signal );
   }

   return( OK );

} /* drvWriteCardBit() */


/***************************************************************************
 *
 *   write/readback  selected bits in reg
 *
 ***************************************************************************/
/* static long drvWriteReadBit( */
/*                              short card,*/    /* card number */
/*                              short signal,*/   /* signal == register # */
/*                              USHORT mask,*/    /* mask */
/*                              USHORT value,*/   /* value to write */
/*                              USHORT shift,*/   /* num to left shift */
/*                              USHORT* prbval*/  /* place to put readback */
/*                           )
{
USHORT   val;
ULONG    status;

   if( (status = check_card( card, signal )) != OK )
   {
      return(status);
   }

   epicsMutexMustLock( dio[card].lock );
   val = *((USHORT*)dio[card].dptr + signal);
   val = (val & ~(mask)) | (value << shift);
   *((USHORT*)dio[card].dptr + signal) = val;
   epicsMutexUnlock( dio[card].lock );

   *prbval = ((*((USHORT*)dio[card].dptr + signal)) & mask) >> shift;

   return( OK );

} */ /* drvWriteReadBit() */


/******************************************************************************
 *
 * Place any user-required functions here.
 *
 ******************************************************************************/

#define BCGDisable   ( 0x04 )
#define RamSize      ( 1296 / 16 )

/***********************************************************************
 *
 * Write Ram location
 *
 * Any sanity checking must be done prior to calling this routine
 *
 ***********************************************************************/
static long drvWriteRamLoc( short card, int location, USHORT val )
{
USHORT csr;

   epicsMutexMustLock(  dio[card].lock );
   csr                           = dio[card].dptr->reg.r.csr;
   dio[card].dptr->reg.w.csr     = csr | BCGDisable;
   dio[card].dptr->reg.w.address = location;
   dio[card].dptr->reg.w.data    = val;
   dio[card].dptr->reg.w.csr     = csr;
   epicsMutexUnlock( dio[card].lock );

   if( *drvDebug )
   {
      errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "%s:drvWriteRamLoc: address = %d, val = 0x%x\n", drvName, location, val );
   }

   return( 0 );

} /* drvWriteRamLoc() */


/***********************************************************************
 *
 * Write Ram
 *
 ***********************************************************************/
static long drvWriteRam( short card )
{
int      i;
USHORT   csr;
ULONG    status;

   if( (status = check_card( card, 0 )) != OK )
   {
      return( status );
   }

   epicsMutexMustLock( dio[card].lock );
   csr                           = dio[card].dptr->reg.r.csr;
   dio[card].dptr->reg.w.csr     = csr | BCGDisable;
   dio[card].dptr->reg.w.address = 0;

   for( i = 0; i < RamSize; i++ )
   {
      dio[card].dptr->reg.w.data = dio[card].ramW[i];
   }

   dio[card].dptr->reg.w.csr     = csr;
   epicsMutexUnlock( dio[card].lock );

   return( OK );

} /* drvWriteRam() */


/***********************************************************************
 *
 * Read Ram
 *
 ***********************************************************************/
static long drvReadRam( short card )
{
int      i;
USHORT   csr;
ULONG    status;

   if( (status = check_card( card, 0 )) != OK )
   {
      return( status );
   }

   epicsMutexMustLock( dio[card].lock );
   csr = dio[card].dptr->reg.r.csr;
   dio[card].dptr->reg.w.csr = csr | BCGDisable;
   dio[card].dptr->reg.w.address = 0;

   for( i = 0; i < RamSize; i++ )
   {
      dio[card].ramR[i] = dio[card].dptr->reg.r.data;
   }

   dio[card].dptr->reg.w.csr = csr;
   epicsMutexUnlock( dio[card].lock );

   return( OK );

} /* drvReadRam() */


/***********************************************************************
 *
 *  Copy Read Ram contents to Write Ram Contents
 *
 ***********************************************************************/
static long drvCopyRam( short card )
{
int   i;
ULONG status;

   if( (status = check_card( card, 0 )) != OK )
   {
      return( status );
   }

   for( i = 0; i < RamSize; i++ )
   {
      dio[card].ramW[i] = dio[card].ramR[i];
   }

   return( OK );

} /* drvCopyRam() */


/***********************************************************************
 *
 * Clear Ram
 *
 ***********************************************************************/
static long drvClearRam( short card )
{
int      i;
USHORT   csr;
ULONG    status;

   if( (status = check_card( card, 0 )) != OK )
   {
      return( status );
   }

   epicsMutexMustLock( dio[card].lock );
   csr                           = dio[card].dptr->reg.r.csr;
   dio[card].dptr->reg.w.csr     = csr | BCGDisable;
   dio[card].dptr->reg.w.address = 0;

   for( i = 0; i < RamSize; i++ )
   {
      dio[card].ramW[i]          = 0;
      dio[card].ramR[i]          = 0;
      dio[card].dptr->reg.w.data = 0;
   }

   dio[card].dptr->reg.w.csr     = csr;
   epicsMutexUnlock( dio[card].lock );

   return( OK );

} /* drvClearRam() */


/**************************************************************************/
static long drvListBuckets( int card, short* buf, int max, int* actual )
{
int      num,
         j,
         ramloc,
         notDone;
short    bucket;
USHORT   val;

   notDone  = 1;
   ramloc   = num = bucket = 0;

   while( notDone )
   {
      val = dio[card].ramR[ramloc++];

      for( j = 0; j < 16; j++ )
      {
         if( val & 0x01 )
         {
            *buf++ = bucket;
            num++;
         }

         if( num >= max || ++bucket >= 1296 )
         {
            notDone = 0;
            break;
         }

         val >>= 1;
      }
   }

   *actual = num;

   return( 0 );

} /* drvListBuckets() */


/**************************************************************************/
static long drvShowPattern( int card, short* buf, int max, int *actual )
{
int      num,
         j,
         ramloc,
         notDone;
USHORT   val;

   notDone  = 1;
   ramloc   = num = 0;

   while( notDone )
   {
      val = dio[card].ramR[ramloc];

      for( j = 0; j < 16; j++ )
      {
         if( val & 0x01 )
         {
            *buf++ = 1;
         }
         else
         {
            *buf++ = 0;
         }

         if( ++num >= max || ramloc >= RamSize )
         {
            notDone = 0;
            break;
         }

         val >>= 1;
      }

      ramloc++;
   }

   *actual = num;

   return( 0 );

} /* drvShowPattern() */


#define PERLINE   ( 16 )

/*************************************************************************/
long drvBunchClkGenDump( int card )
{
int      i,
         j,
         bucket,
         num,
         ramloc,
         notDone;
USHORT   val;
USHORT   buckets[PERLINE];


   if( drvReadRam( card ) != OK )
   {
      printf( "Error on accessing card\n" );
      return( -1 );
   }

   printf( "P0 Delay = 0x%x, Fine Delay = 0x%x\n", dio[card].dptr->reg.r.P0Delay, dio[card].dptr->reg.r.fineDelay );

   notDone  = 1;
   bucket   = 0;
   ramloc   = 0;
   j        = 16;
   val      = 0;

   while( notDone )
   {
      num = 0;

      while( num < PERLINE && notDone )
      {
         if( j >= 16 )
         {
            val = dio[card].ramR[ramloc++];
            j = 0;
         }

         for( ; j < 16 && num < PERLINE; j++, bucket++ )
         {
            if( val & 0x01 )
            {
               buckets[num++] = bucket;
            }

            val >>= 1;
         }

         if( ramloc >= RamSize && j >= 16 )
         {
            notDone = 0;
         }

      }

      for( i = 0; i < num; i++ )
      {
         printf( "%4.4d ", buckets[i] );
      }

      printf( "\n" );
   }

   return( 0 );

} /* drvBunchClkGenDump() */


#define PERLINE1  ( 10 )

/*************************************************************************/
long drvBunchClkGenDumpRam( int card )
{
int      i,
         ramloc,
         notDone;
USHORT   val;


   if( drvReadRam( card ) != OK )
   {
      printf( "Error on accessing card\n" );
      return( -1 );
   }

   printf( "P0 Delay = 0x%x, Fine Delay = 0x%x\n", dio[card].dptr->reg.r.P0Delay, dio[card].dptr->reg.r.fineDelay );

   notDone  = 1;
   ramloc   = 0;
   val      = 0;

   while( notDone )
   {
      printf( "%4.4d  ", ramloc );

      for( i = 0; (i < PERLINE1) && notDone; i++, ramloc++ )
      {
         val = dio[card].ramR[ramloc++];
         printf( "0x%4.4x ", val );

         if( ramloc >= RamSize )
         {
            notDone = 0;
         }

      }

      printf( "\n" );
   }

   return( 0 );

} /* drvBunchClkGenDumpRam() */


/***************************************************************************
 *
 *  Write a bucket in Ram
 *
 **************************************************************************/
static long drvWriteBucket( short card, short bucket, short value )
{
int      ramloc,
         bit;
USHORT   mask,
         val,
         tmp;
ULONG    status;

   if( (status = check_card( card, 0 )) != OK )
   {
      return( status );
   }

   if( bucket >= 1296 )
   {
      if( *drvDebug )
      {
         errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "%s: bucket number >=1296\n", drvName );
      }
   }

   ramloc   = bucket / 16;
   bit      = bucket % 16;
   mask     = 0x01 << bit;

   if( value )
   {
      val = mask;
   }
   else
   {
      val = 0;
   }

   tmp = (dio[card].ramW[ramloc] & ~mask) | val;

   if( *drvDebug )
   {
      printf( "value = %d, val = %d tmp = 0x%x\n", value, val, tmp );
   }

   dio[card].ramW[ramloc] = tmp;
   drvWriteRamLoc( card, ramloc, dio[card].ramW[ramloc] );

   return( 0 );

} /* drvWriteBucket() */


/*************************************************************************/
static int drvGetPwrOnStatus( short card )
{

   if( check_card( card, 0 ) != OK )
   {
      return( -1 );
   }
   else
   {
      return( dio[card].init_hw );
   }

} /* drvGetPwrOnStatus() */


/******************************************************************************
 *
 * Begin device support
 *
 *****************************************************************************/

/* create the dsets */
#define PARAM_BIT_FIELD    ( 1 )
#define PARAM_MASK         ( 2 )
#define PARAM_ASCII        ( 3 )

#define PARAM_BIT          ( 1 )
#define PARAM_SHORT        ( 2 )
#define PARAM_LONG         ( 3 )
#define PARAM_MBB          ( 4 )

#define PARAM_RW           ( 1 )
#define PARAM_RO           ( 2 )
#define PARAM_WO           ( 3 )

#define NUMCHANNELS        ( 0 )
#define CHANNELREGS        ( 1 )

int devBunchClkGenDebug = 0;
static int* devDebug = &devBunchClkGenDebug;
epicsExportAddress(int, devBunchClkGenDebug);

static long initBiRecord(struct biRecord*);
static long readBi(struct biRecord*);

static long initBoRecord(struct boRecord*);
static long writeBo(struct boRecord*);

static long initAoRecord(struct aoRecord*);
static long writeAo(struct aoRecord*);
static long specialLinconvAo(struct aoRecord*, int);

static long initAiRecord(struct aiRecord*);
static long readAi(struct aiRecord*);
static long specialLinconvAi(struct aiRecord*, int);

static long initWfRecord(struct waveformRecord*);
static long readWf(struct waveformRecord*);

typedef struct DSET
{
   long        number;
   DEVSUPFUN   report;
   DEVSUPFUN   init;
   DEVSUPFUN   init_record;
   DEVSUPFUN   get_ioint_info;
   DEVSUPFUN   read_write;
} DSET;

DSET devBiBunchClkGen = { 5, NULL, NULL, initBiRecord, NULL, readBi  };
DSET devBoBunchClkGen = { 5, NULL, NULL, initBoRecord, NULL, writeBo };

epicsExportAddress(dset, devBiBunchClkGen);
epicsExportAddress(dset, devBoBunchClkGen);

typedef struct DSETA
{
   long        number;
   DEVSUPFUN   report;
   DEVSUPFUN   init;
   DEVSUPFUN   init_record;
   DEVSUPFUN   get_ioint_info;
   DEVSUPFUN   read_write;
   DEVSUPFUN   special_linconv;
} DSETA;

DSETA devAoBunchClkGen = { 6, NULL, NULL, initAoRecord, NULL, writeAo, specialLinconvAo };
DSETA devAiBunchClkGen = { 6, NULL, NULL, initAiRecord, NULL, readAi,  specialLinconvAi };
DSETA devWfBunchClkGen = { 6, NULL, NULL, initWfRecord, NULL, readWf,  NULL             };

epicsExportAddress(dset, devAoBunchClkGen);
epicsExportAddress(dset, devAiBunchClkGen);
epicsExportAddress(dset, devWfBunchClkGen);

static struct paramEntrys
{
   char* param;
   short type;
   short access;
   short signal;
   short bit;
   short shift;
   short special;
   ULONG mask;
}
paramEntry[] =
{
   { "Running",   PARAM_BIT, PARAM_RO, 0, 0, 0, 0, 0x0    },
   { "P0_Detect", PARAM_BIT, PARAM_RO, 0, 1, 0, 0, 0x0    },
   { "Disable",   PARAM_BIT, PARAM_RW, 0, 2, 0, 0, 0xfff4 },
   { "PwrCycle",  PARAM_BIT, PARAM_RW, 0, 3, 0, 0, 0xfff8 },
   { "InitRam",   PARAM_BIT, PARAM_WO, 1, 0, 0, 0, 0x1    },
   { "WriteRam",  PARAM_BIT, PARAM_WO, 2, 0, 0, 0, 0x1    }
};

static struct paramTbls
{
   int                  num;
   struct paramEntrys*  pentry;
}
paramTbl =
{
   sizeof(paramEntry) / sizeof(struct paramEntrys),
   paramEntry
};

struct PvtBi
{
   short signal;
   short special;
};

struct PvtBo
{
   USHORT mask;
   short  signal;
};


/********************************************************************/
static long lookUpParam( char *parm, ULONG *pval )
{
int   i;
char* xname="lookUpParam";

   if( parm == NULL)
   {
      errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "%s: NULL pointer encounterd for param", xname );
      return( ERROR );
   }

   for( i = 0; i < paramTbl.num; i++ )
   {
      if( strcmp( parm, paramTbl.pentry[i].param ) == 0 )
      {
         *pval = i;
         break;
      }

   }

   if( i >= paramTbl.num )
   {
      return( ERROR );
   }

   return( OK );

} /* lookUpParam() */


/********************************************************************/
static long initParam( char* parm, ULONG* pval, USHORT mode )
{
USHORT   val;
ULONG    val1;
char*    xname="initParam";

   if( parm == NULL )
   {
      if( *devDebug )
      {
         errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "%s: NULL pointer for Parameter String", xname );
      }

      return( ERROR );
   }

   val = 1;
   switch( mode )
   {
   case PARAM_BIT_FIELD:
      if( *parm >= '0' && *parm <= '9' )
      {
         *pval = val << (*parm - '0');
         return( OK );
      }
      else if( *parm >= 'a' && *parm <= 'f' )
      {
         *pval = val << (*parm - 'a' + 10);
         return( OK );
      }
      else if( *parm >= 'A' && *parm <= 'F' )
      {
         *pval = val << (*parm - 'A' + 10);
         return( OK );
      }
      else
      {
         return( ERROR );
      }
      break;

   case PARAM_MASK:
      if( sscanf( parm, "%lx", &val1 ) != 1 )
      {
         if( *devDebug )
         {
            errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "%s: Bad Parameter = %s ", xname, parm );
         }

         return( ERROR );
      }
      *pval = val1;
      return( OK );

   case PARAM_ASCII:
      return( lookUpParam( parm, pval ) );

   default:
      return( ERROR );
   }

} /* initParam() */


/*********************************************************************/
static long initBiRecord( struct biRecord* pbi )
{
struct vmeio*  pvmeio;
struct PvtBi*  ptr;
long           status;
USHORT*        pReg;
USHORT         val;
ULONG          lval,
               lval1;


   /* bi.inp must be an VME_IO */
   switch( pbi->inp.type )
   {
   case VME_IO:
      pvmeio = (struct vmeio*)&(pbi->inp.value);
      break;

   default:
      recGblRecordError( S_dev_badInpType, (void*)pbi,"devBiBunchClkGen (initBiRecord): not a VME device!!!" );
      return( S_dev_badInpType );
   }

   /* call driver so that it configures card */
   if( (status = check_card( pvmeio->card, pvmeio->signal )) != OK )
   {
      recGblRecordError( S_dev_badCard, (void*)pbi, "devBiBunchClkGen(initBiRecord): init failed!!!" );
      return( S_dev_badCard );
   }

   /* check param */
   if( initParam( pvmeio->parm, &lval, PARAM_ASCII ) != OK )
   {
      if( *devDebug )
      {
         errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "devBiBunchClkGen: card = 0x%x param = %s (BAD PARAM)\n", pvmeio->card, pvmeio->parm );
      }

      recGblRecordError( S_dev_badSignal, (void*)pbi, "devBiBunchClkGen(initBiRecord): init failed!!!" );
      return( S_dev_badSignal );
   }

   lval1       = 1;
   pbi->mask   = (lval1 << paramTbl.pentry[lval].bit);

   if( (pbi->dpvt = malloc( sizeof(struct PvtBi) ) ) == NULL )
   {
      if( *devDebug )
      {
         errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "devBiBunchClkGen: card = %d  sig = %d (Malloc failed)\n", pvmeio->card, pvmeio->signal );
      }

      recGblRecordError( S_dev_noMemory, (void*)pbi, "devBiBunchClkGen(initBiRecord): init failed!!!" );
      return( S_dev_noMemory );
   }

   ptr = (struct PvtBi*)pbi->dpvt;

   if( pvmeio->signal < NUMCHANNELS  )
   {
      ptr->signal = (paramTbl.pentry[lval].signal + pvmeio->signal * CHANNELREGS);
   }
   else
   {
      ptr->signal = paramTbl.pentry[lval].signal;
   }

   ptr->special = paramTbl.pentry[lval].special;

   /* Check Register Exists */
   pReg = (USHORT*)dio[pvmeio->card].dptr + ptr->signal;
   if( devReadProbe( sizeof(val), (char*)pReg, (char*)&val ) )
   {
      if( *devDebug )
      {
         errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "devBiBunchClkGen: vxMemProbe failed!!! card = 0x%x, sig = 0x%x\n", pvmeio->card, pvmeio->signal );
      }

      recGblRecordError( S_dev_badSignal, (void*)pbi, "devBiBunchClkGen(initBiRecord): init failed!!!" );
      pbi->dpvt = NULL;

      return( S_dev_badSignal );
   }

   return( 0 );

} /* initBiRecord() */


/*****************************************************************/
static long readBi( struct biRecord* pbi )
{
ULONG          value;
struct vmeio*  pvmeio = (struct vmeio*)&(pbi->inp.value);
struct PvtBi*  ptr;


   if( pbi->dpvt == NULL )
   {
      return( S_dev_NoInit );
   }

   ptr = (struct PvtBi*)pbi->dpvt;

   if( drvReadCard( pvmeio->card, FREG_READ, ptr->signal, &value ) != OK )
   {

      if( *devDebug )
      {
         errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "devBiBunchClkGen(readBi): read Error, card = %d, sig = %d", pvmeio->card, pvmeio->signal );
      }

      recGblSetSevr( pbi, READ_ALARM, INVALID_ALARM );
      return( 2 ); /* don't convert */
   }

   pbi->rval = value & pbi->mask;

   if( *devDebug )
   {
      printf( "devBiBunchClkGen(readBi): card = %d, signal = %d, raw read value = 0x%x\n", pvmeio->card, ptr->signal,  pbi->rval );
   }

   return( 0 );

} /* readBi() */


/************************************************************************/
static long initBoRecord( struct boRecord* pbo )
{
USHORT         value;
ULONG          lval,
               lval1;
USHORT*        pReg;
long           status;
struct vmeio*  pvmeio;
struct PvtBo*  ptr;


   /* bo.out must be an VME_IO */
   switch( pbo->out.type )
   {
   case VME_IO:
      pvmeio = (struct vmeio*)&(pbo->out.value);
      break;

   default:
      recGblRecordError( S_dev_badOutType, (void*)pbo, "devBoBunchClkGen (initBorecord)" );
      return( S_dev_badOutType );
   }

   /* call driver so that it configures card */
   if( (status = check_card( pvmeio->card, pvmeio->signal )) != OK )
   {
      recGblRecordError( S_dev_badCard,(void*)pbo, "devBoBunchClkGen(initBorecord): init failed!!!" );
      return( S_dev_badCard );
   }

   /* check param */
   if( initParam(pvmeio->parm, &lval, PARAM_ASCII) != OK )
   {
      if( *devDebug )
      {
         errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "devBoBunchClkGen: card = %d Sig = %d  PARM = %s (BAD PARAM)\n", pvmeio->card, pvmeio->signal,  pvmeio->parm );
      }

      recGblRecordError( S_dev_badSignal,(void*)pbo, "devBoBunchClkGen(initBoRecord): init failed!!!" );
      return( S_dev_badSignal );
   }

   lval1       = 1;
   pbo->mask   = (lval1 << paramTbl.pentry[lval].bit);

   if( *devDebug )
   {
      errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "devBoBunchClkGen: card = %d sig = %d  PARM = %s mask = %d", pvmeio->card, pvmeio->signal,  pvmeio->parm, pbo->mask );
   }

   if( (pbo->dpvt = malloc(sizeof( struct PvtBo)) ) == NULL )
   {
      if( *devDebug )
      {
         errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "devBoBunchClkGen: card = %d  sig = %d (Malloc failed)\n", pvmeio->card, pvmeio->signal );
      }

      recGblRecordError( S_dev_noMemory,(void*)pbo, "devBoBunchClkGen(initBoRecord): init failed!!!" );
      return( S_dev_noMemory );
   }

   ptr       = (struct PvtBo*)pbo->dpvt;
   ptr->mask = (USHORT)(paramTbl.pentry[lval].mask);

   if( pvmeio->signal < NUMCHANNELS  )
   {
      ptr->signal = (paramTbl.pentry[lval].signal + pvmeio->signal * CHANNELREGS);
   }
   else
   {
      ptr->signal = paramTbl.pentry[lval].signal;
   }

   /* Check Register Exists */
   pReg = (USHORT*)dio[pvmeio->card].dptr + ptr->signal;

   if( devReadProbe( sizeof(value), (char*)pReg, (char*)&value ) )
   {
      if( *devDebug )
      {
         errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "devBoBunchClkGen: vxMemProbe failed!!! card = %d, sig = %d\n", pvmeio->card, pvmeio->signal );
      }

      recGblRecordError( S_dev_badSignal, (void*)pbo, "devBoBunchClkGen(initBorecord): init failed!!!" );
      pbo->dpvt=NULL;

      return( S_dev_badSignal );
   }

   /* Only signal 0 corresponds to a hardware register, signals 1 and 2 are software */
   if( ptr->signal > 0 )
   {
      pbo->rval = pbo->rbv = 0;
   }
   else
   {
      pbo->rval = pbo->rbv = (ULONG)value & pbo->mask;
   }

   if( drvGetPwrOnStatus( pvmeio->card ) == 1 )
   {
      return( 0 );  /* initialize to readback value */
   }
   else
   {
      return( 2 );  /* don't convert */
   }

} /* initBoRecord() */


/**************************************************************************/
static long writeBo( struct boRecord *pbo )
{
USHORT         value,
               mask;
USHORT*        pReg;
struct vmeio*  pvmeio = (struct vmeio*)&(pbo->out.value);
struct PvtBo*  ptr;


   if( pbo->dpvt == NULL )
   {
      return( S_dev_NoInit );
   }

   ptr   = (struct PvtBo*)pbo->dpvt;
   value = (USHORT) pbo->rval;
   mask  = ptr->mask;

   if( ptr->signal == 1 )
   {
      drvClearRam( pvmeio->card );
      return( 0 );
   }
   else if( ptr->signal == 2 )
   {
      drvWriteRam( pvmeio->card );
      return( 0 );
   }

   if( drvWriteCardBit( pvmeio->card, FREG_WRITEBITS, ptr->signal, value, mask ) != OK )
   {
      if( *devDebug )
      {
         errPrintf( NO_ERR_RPT, __FILE__, __LINE__,"devBoBunchClkGen(writeBo): Write Error, card = %d", pvmeio->card );
      }

      recGblSetSevr( pbo, WRITE_ALARM, INVALID_ALARM );
      return( 2 );   /* don't convert */
   }

   pReg     = (USHORT*)dio[pvmeio->card].dptr + ptr->signal;
   value    = *pReg;
   pbo->rbv = (ULONG)value & pbo->mask;

   if( *devDebug )
   {
      printf( "devBoBunchClkGen(writeBo): card = %d, signal = %d, write value = 0x%x\n", pvmeio->card, ptr->signal,  pbo->rval );
   }

   return( 0 );

} /* writeBo() */


/***************************************************************************/
static long initAoRecord( struct aoRecord* pao )
{
USHORT         value;
USHORT*        pReg;
struct vmeio*  pvmeio;
long           status;


   /* ao.out must be an VME_IO */
   switch( pao->out.type )
   {
   case VME_IO:
      pvmeio = (struct vmeio*)&(pao->out.value);
      break;

   default:
      recGblRecordError( S_dev_badOutType, (void*)pao, "devAoBunchClkGen (initAoRecord)" );
      return( S_dev_badOutType );
   }

   /* call driver so that it configures card */
   if( (status = check_card( pvmeio->card, pvmeio->signal )) != OK )
   {
      recGblRecordError( S_dev_badCard,(void*)pao, "devAoBunchClkGen(initAoRecord) : init failed!!!" );
      return( S_dev_badCard );
   }

   if( (pvmeio->signal > 4) || (pvmeio->signal < 0) )
   {
      if( *devDebug )
      {
         errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "devAoBunchClkGen: Signal out of range!!! card = %d, sig = %d\n", pvmeio->card, pvmeio->signal );
      }

      recGblRecordError( S_dev_badSignal,(void*)pao, "devAoBunchClkGen(initAoRecord): init failed!!!" );
      return( S_dev_badSignal );
   }

   /*  Check Card Exists */
   pReg = (USHORT*)dio[pvmeio->card].dptr;
   if( devReadProbe( sizeof(value), (char*)pReg, (char*)&value ) )
   {
      if( *devDebug )
      {
         errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "devAoBunchClkGen: vxMemProbe failed!!! card = %d, sig = %d\n", pvmeio->card, pvmeio->signal );
      }

      recGblRecordError( S_dev_badSignal,(void*)pao, "devAoBunchClkGen(initAoRecord): init failed!!!" );
      return( S_dev_badSignal );
   }

   pReg  = (USHORT*)dio[pvmeio->card].dptr + pvmeio->signal;
   value = *pReg;

   /* set linear conversion slope*/
   switch( pvmeio->signal )
   {
   case 3:        /* fine delay */
      pao->eslo = (pao->eguf - pao->egul) / 255;
      pao->rval = pao->rbv = (long)(0xff & value);
      break;

   case 4:        /* P0 Delay */
      pao->eslo = 1;
      pao->rval = pao->rbv = (long)(65535 - value);
      break;

   default:
      pao->rval = pao->rbv = 0;
      pao->eslo =  1;
   }

   if( *devDebug )
   {
      errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "devAoBunchClkGen: card = %d sig = %d", pvmeio->card, pvmeio->signal );
   }

   pao->dpvt = pao;

   if( drvGetPwrOnStatus( pvmeio->card ) == 1 )
   {
      return( 0 );   /* initialize to readback value */
   }
   else
   {
      return( 2 );   /* don't convert */
   }

} /* initAoRecord() */


/*****************************************************************/
static long writeAo( struct aoRecord *pao )
{
USHORT         value;
ULONG          lval;
short          val;
USHORT*        pReg;
struct vmeio*  pvmeio = (struct vmeio*)&(pao->out.value);


   if( pao->dpvt == NULL )
   {
      return( S_dev_NoInit );
   }

   if( pvmeio->signal == 0 || pvmeio->signal == 1 )
   {
      if( pvmeio->signal == 0 )
      {
         val = 1;
      }
      else
      {
         val = 0;
      }

      if( drvWriteBucket( pvmeio->card, pao->val, val ) != OK )
      {
         if( *devDebug )
         {
            errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "devAoBunchClkGen(writeAo): Write Error, card = %d, signal = %d", pvmeio->card, pvmeio->signal );
         }

         recGblSetSevr( pao, WRITE_ALARM, INVALID_ALARM );
         return( 2 );   /* don't convert */
      }

      pao->rbv = pao->rval;
   }
   else if( pvmeio->signal == 4 )
   {
      lval = (ULONG)(65535 - pao->rval);

      if( drvWriteCard( pvmeio->card, FREG_WRITE, pvmeio->signal, &lval ) != OK )
      {
         if( *devDebug )
         {
            errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "devAoBunchClkGen(writeAo): Write Error, card = %d", pvmeio->card );
         }

         recGblSetSevr( pao, WRITE_ALARM, INVALID_ALARM );
         return( 2 );   /* don't convert */
      }

      pReg     = (USHORT*)dio[pvmeio->card].dptr + pvmeio->signal;
      value    = *pReg;
      pao->rbv = (long)(65535 - value);
   }
   else
   {
      lval = (ULONG)pao->rval;

      if( drvWriteCard( pvmeio->card, FREG_WRITE, pvmeio->signal, &lval ) != OK )
      {
         if( *devDebug )
         {
            errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "devAoBunchClkGen(writeAo): Write Error, card = %d", pvmeio->card );
         }

         recGblSetSevr( pao, WRITE_ALARM, INVALID_ALARM );
         return( 2 );   /* don't convert */
      }

      pReg  = (USHORT*)dio[pvmeio->card].dptr + pvmeio->signal;
      value = *pReg;

      if( pvmeio->signal == 3 )
      {
         pao->rbv = (long)(0xff & value);
      }
      else
      {
         pao->rbv = (long)value;
      }

   }

   if( *devDebug )
   {
      printf( "devAoBunchClkGen(writeAo): card = %d, signal = %d, write value = 0x%x\n", pvmeio->card, pvmeio->signal,  pao->rval );
   }

   return( 0 );

} /* writeAo() */


/*********************************************************************/
static long specialLinconvAo( struct aoRecord* pao, int after )
{
struct vmeio* pvmeio = (struct vmeio*)&(pao->out.value);

   if( !after )
   {
      return( 0 );
   }

   /* set linear conversion slope */
   switch( pvmeio->signal )
   {
   case 0:
   case 1:
   case 3:
      pao->eslo = 1;
      break;

   case 4:
      pao->eslo = (pao->eguf - pao->egul) / 255;
      break;

   default:
      pao->eslo = 1;
   }

   return( 0 );

} /* specialLinconvAo() */


/**************************************************************************/
static long initWfRecord( struct waveformRecord* pRec )
{
struct vmeio*  pvmeio;
USHORT         value;
USHORT*        pReg;


   /* ao.out must be an VME_IO */
   switch( pRec->inp.type )
   {
   case VME_IO:
      pvmeio = (struct vmeio*)&(pRec->inp.value);
      break;

   default:
      recGblRecordError( S_dev_badInpType, (void*)pRec, "devWfBunchClkGen (initWfRecord)" );
      return( S_dev_badInpType );
   }

   /* check signal  in range */
   if( pvmeio->signal < 0 || pvmeio->signal > 1 )
   {
      if( *devDebug )
      {
         errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "devWfBunchClkGen: Signal out of range!!! card = %d, sig = %d\n", pvmeio->card, pvmeio->signal );
      }

      recGblRecordError( S_dev_badSignal, (void*)pRec, "devWfBunchClkGen(initWfRecord): init failed!!!" );
      return( S_dev_badSignal );
   }

   /* Check Card Exists */
   pReg = (USHORT*)dio[pvmeio->card].dptr;
   if( devReadProbe( sizeof(value), (char*)pReg, (char*)&value ) )
   {
      if( *devDebug )
      {
         errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "devWfBunchClkGen: vxMemProbe failed!!! card = %d, sig = %d\n", pvmeio->card, pvmeio->signal );
      }

      recGblRecordError( S_dev_badSignal, (void*)pRec, "devWfBunchClkGen(initWfRecord): init failed!!!" );
      return( S_dev_badSignal );
   }

   /* check for proper data type */
   if( pRec->ftvl != DBF_SHORT )
   {
      recGblRecordError( S_db_badField, (void*)pRec, "devWfBunchClkGen: init failed: ftvl not a short!!!" );
      return( S_db_badField );
   }

   pRec->dpvt = pRec;

   return( 0 );

} /* initWfRecord() */


/*************************************************************************/
static long readWf( struct waveformRecord* pRec )
{
int            num;
struct vmeio*  pvmeio = (struct vmeio*)&(pRec->inp.value);

   if( pRec->dpvt == NULL )
   {
      return( 0 );
   }

   if( drvReadRam( pvmeio->card ) )
   {
      return( 0 );
   }

   if( pvmeio->signal == 0 )
   {
      drvListBuckets( pvmeio->card, pRec->bptr, pRec->nelm, &num );
   }
   else if( pvmeio->signal == 1 )
   {
      drvShowPattern( pvmeio->card, pRec->bptr, pRec->nelm, &num );
   }

   pRec->nord = num;

   return( 0 );

} /* readWf() */


/**************************************************************************/
static long initAiRecord( struct aiRecord* pai )
{
long           status;
struct vmeio*  pvmeio;


   /* ao.out must be an VME_IO */
   switch( pai->inp.type )
   {
   case VME_IO:
      pvmeio = (struct vmeio*)&(pai->inp.value);
      break;

   default:
      recGblRecordError( S_dev_badInpType, (void*)pai, "devAiBunchClkGen (initAiRecord)" );
      return( S_dev_badInpType );
   }

   if( pvmeio->signal > 4 || pvmeio->signal < 3 )
   {
      recGblRecordError( S_dev_badSignal, (void*)pai, "devAiBunchClkGen (initAiRecord) : init failed!!!" );
      return( S_dev_badSignal );
   }

   /* call driver so that it configures card */
   if( (status = check_card( pvmeio->card, pvmeio->signal )) != OK )
   {
      recGblRecordError( S_dev_badCard, (void*)pai, "devAiBunchClkGen (initAiRecord): init failed!!!" );
      return( S_dev_badCard );
   }

   /* set linear conversion slope*/
   switch( pvmeio->signal )
   {
   case 3:     /* fine delay */
      pai->eslo = (pai->eguf - pai->egul) / 255.0;
      break;

   case 4:     /* P0/coarse dealy */
      pai->eslo = 1;
      break;
   }

   if( *devDebug )
   {
      errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "devAiBunchClkGen: card = %d sig = %d", pvmeio->card, pvmeio->signal );
   }

   pai->dpvt = pai;

   return( 0 );

} /* initAiRecord() */


/*****************************************************************/
static long readAi( struct aiRecord* pai )
{
ULONG          lval;
struct vmeio*  pvmeio = (struct vmeio*)&(pai->inp.value);


   if( pai->dpvt == NULL )
   {
      return( S_dev_NoInit );
   }

   if( drvReadCard( pvmeio->card, FREG_READ, pvmeio->signal, &lval ) != OK )
   {
      if( *devDebug )
      {
         errPrintf( NO_ERR_RPT, __FILE__, __LINE__, "devAiBunchClkGen(readAi): Write Error, card = %d", pvmeio->card );
      }

      recGblSetSevr( pai, WRITE_ALARM, INVALID_ALARM );
      return( 2 ); /* don't convert */
   }

   if( pvmeio->signal == 4 )
   {
      pai->rval = (long)(65535 - lval);
   }
   else if( pvmeio->signal == 3 )
   {
      pai->rval = (long)(0xff & lval);
   }
   else
   {
      pai->rval = (long)lval;
   }

   if( *devDebug )
   {
      printf( "devAiBunchClkGen(readAi): card = %d, signal = %d, write value = 0x%x\n", pvmeio->card, pvmeio->signal,  pai->rval );
   }

   return( 0 );

} /* readAi() */


/*********************************************************************/
static long specialLinconvAi( struct aiRecord* pai, int after )
{
struct vmeio*  pvmeio = (struct vmeio*)&(pai->inp.value);


   if( !after )
   {
      return( 0 );
   }

   /* set linear conversion slope */
   switch( pvmeio->signal )
   {
   case 4:  /* P0 / coarse delay */
      pai->eslo = (pai->eguf - pai->egul) / 255.0;
      break;

   case 3:  /* Fine delay */
      pai->eslo = 1;
      break;

   default:
      break;
   }

   return( 0 );

} /* specialLinconvAi() */

