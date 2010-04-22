/*+ devAvme9440.c

                          Argonne National Laboratory
                            APS Operations Division
                     Beamline Controls and Data Acquisition

                           AVME-9440 Device Support



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
   This device support routine provides EPICS support for the Acromag AVME-9440
   16 bit binary input and output board. Change of state I/O interrupts are
   available for binary input signals 0 -7 only, and only for the BI record.
   If interrupts are desired for MBBI type values, they must be done via BI
   records linked to CALC records.  As many I/O interrupt scan BI records may
   be connected to a single binary input signal as desired.

      Binary Outputs          0 - 15   "AVME9440 O" Signals  0 - 15
      Binary Input            0 - 15   "AVME9440 I" Signals  0 - 15
      Binary Output Readback  0 - 15   "AVME9440 I" Signals 16 - 31

   Diagnostic messages can be output by setting the values to the following:
      devAvme9440Debug == 0 --- no debugging messages
      devAvme9440Debug >= 5 --- hardware initialization information
      devAvme9440Debug >= 10 -- record initialization information
      devAvme9440Debug >= 15 -- write commands
      devAvme9440Debug >= 20 -- read commands

   Additional public variants:
      avme9440_int_level   - Indicates interrupt level.
      avme9440_link_count  - Indicates number of cards.

   The method devAvme9440Config is called from the startup script to specify
   the number of cards, base address, and interrupt vector. It must be called
   prior to iocInit(). Below is the calling sequence:

      devAvme9440Config( ncards, base, vector )

      Where:
         ncards   - Number of cards.
         base     - VME A16 address space.
         vector   - Interrupt vector.

      For example:
         devAvme9440Config( 1, 0x400, 0x78 )

 Developer notes:
   1) More formatting and commenting should be done as well as porting the
      driver / device support to EPICS base >= 3.14.6.

 =============================================================================
 History:
 Author: Greg Nawrocki
 -----------------------------------------------------------------------------
 XX-04-93   GN    - Initial.
 28-01-05   DMK   - Took over support for driver / device support.
                  - Reformatted and documented for clarity.
                  - Made methods and variants more encapsulated / private.
                  - Made OS independent by employing EPICS devLib methods.
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
#include <recGbl.h>
#include <epicsPrint.h>
#include <epicsExport.h>


/* EPICS record processing related include files */
#include <dbScan.h>
#include <dbAccess.h>
#include <boRecord.h>
#include <biRecord.h>
#include <mbboRecord.h>
#include <mbbiRecord.h>


/* Define symbolic constants */
#define NOCHANS         ( 8 )       /* Max. number of channels */
#define NOCARDS         ( 6 )       /* Max. number of cards */
#define IRQLEVEL        ( 5 )       /* Default IRQ Level */

#define LED_INIT        ( 0x02 )
#define LED_OKRUN       ( 0x03 )
#define LED_OKINTS      ( 0x0B )

#define ADDING          ( 0 )
#define DELETING        ( 1 )
#define INPUT_REG       ( 1 )
#define OUTPUT_REG      ( 2 )


/* Declare AVME9440 data structure */
typedef struct avme9440
{
   UCHAR  pad0[0x80];
   UCHAR  pad1;
   UCHAR  boardStatus;
   UCHAR  pad2[0x1e];
   UCHAR  pad3;
   UCHAR  intVector0;
   UCHAR  pad4;
   UCHAR  intVector1;
   UCHAR  pad5;
   UCHAR  intVector2;
   UCHAR  pad6;
   UCHAR  intVector3;
   UCHAR  pad7;
   UCHAR  intVector4;
   UCHAR  pad8;
   UCHAR  intVector5;
   UCHAR  pad9;
   UCHAR  intVector6;
   UCHAR  pada;
   UCHAR  intVector7;
   UCHAR  padb[0x10];
   UCHAR  padc;
   UCHAR  intStatus;
   UCHAR  padd;
   UCHAR  intEnable;
   UCHAR  pade;
   UCHAR  intPolarity;
   UCHAR  padf;
   UCHAR  intTypeSelect;
   UCHAR  padg;
   UCHAR  intPaternEnable;
   USHORT inputData;
   USHORT outputData;
   UCHAR  padh[0x332];
} avme9440;


/* Declare card data structure */
typedef struct ioCard
{
   volatile avme9440* card;               /* card base address */
   int                intCnt[NOCHANS];    /* interrupt records per channel */
   epicsMutexId       lock;               /* semaphore */
   IOSCANPVT          ioscanpvt[NOCHANS]; /* records processed upon interrupt */
} ioCard;


/* Define public variants */
int avme9440_int_level  = IRQLEVEL;
int avme9440_link_count = 0;
int devAvme9440Debug    = 0;


/* Define private variants */
static USHORT vecBase;
static USHORT addrBase;
static UCHAR  initFlag = FALSE;
static ioCard cards[NOCARDS];


/* Forward references */
static long init(int);
static long checkLink(short);
static long devAvme9440Report();
static void avme9440_isr(void*);
static long write_card(short,ULONG,ULONG);
static long read_card(short,ULONG,USHORT*,int);

static long write_bo(boRecord*);
static long init_bo_record(boRecord*);

static long read_bi(biRecord*);
static long init_bi_record(biRecord*);
static long get_bi_int_info(int,biRecord*,IOSCANPVT*);

static long write_mbbo(mbboRecord*);
static long init_mbbo_record(mbboRecord*);

static long read_mbbi(mbbiRecord*);
static long write_mbbo_card(short,ULONG,ULONG);
static long init_mbbi_record(mbbiRecord*);


/* Create the dset for devBoAvme9440 */
static struct
{
   long      number;
   DEVSUPFUN report;          /* used by dbior */
   DEVSUPFUN init;            /* called 1 time before & after all records */
   DEVSUPFUN init_record;     /* called 1 time for each record */
   DEVSUPFUN get_ioint_info;  /* used for COS processing (not used for outputs)*/
   DEVSUPFUN write_bo;        /* output command goes here */
}
devBoAvme9440 = {5, devAvme9440Report, init, init_bo_record, NULL, write_bo};
epicsExportAddress( dset, devBoAvme9440 );


/* Create the dset for devBiAvme9440 */
static struct
{
   long      number;
   DEVSUPFUN report;         /* used by dbior */
   DEVSUPFUN init;           /* called 1 time before & after all records */
   DEVSUPFUN init_record;    /* called 1 time for each record */
   DEVSUPFUN get_ioint_info; /* used for COS processing */
   DEVSUPFUN read_bi;        /* input command goes here */
}
devBiAvme9440 = {5, NULL, NULL, init_bi_record, get_bi_int_info, read_bi};
epicsExportAddress( dset, devBiAvme9440 );


/* Create the dset for devMbboAvme9440 */
static struct
{
   long      number;
   DEVSUPFUN report;
   DEVSUPFUN init;
   DEVSUPFUN init_record;
   DEVSUPFUN get_ioint_info;
   DEVSUPFUN write_mbbo;
}
devMbboAvme9440 = {5, NULL, NULL, init_mbbo_record, NULL, write_mbbo};
epicsExportAddress( dset, devMbboAvme9440 );


/* Create the dset for devMbbiAvme9440 */
static struct
{
   long      number;
   DEVSUPFUN report;
   DEVSUPFUN init;
   DEVSUPFUN init_record;
   DEVSUPFUN get_ioint_info;
   DEVSUPFUN read_mbbi;
}
devMbbiAvme9440 = {5, NULL, NULL, init_mbbi_record, NULL, read_mbbi};
epicsExportAddress( dset, devMbbiAvme9440 );


/***************************************************************************
 *
 * Ultra groovy and useful reporting function called from 'dbior'.
 *
 ***************************************************************************/
static long devAvme9440Report()
{
int i;
int vec  = vecBase;
int base = addrBase;


   /* Output information */
   printf( "\nAVME9440 Configuration - " );

   if( initFlag == TRUE )
   {
      printf( "%d card(s) are configured\n", avme9440_link_count );
   }
   else
   {
      printf( "no cards are configured\n" );
   }

   for( i = 0; i < avme9440_link_count; ++i )
   {

      if( cards[i].card == NULL )
      {
         continue;
      }

      printf( "\tCard %2.2d at 0x%4.4X, IRQ 0x%2.2X, input 0x%4.4X, output 0x%4.4X\n",
              (i + 1), base, vec, cards[i].card->inputData, cards[i].card->outputData );

      ++vec;
      base += sizeof(avme9440);
   }

   printf( "\n" );

   return( 0 );
}


/**************************************************************************
 *
 * Initialization of AVME9440 Binary I/O Card
 *
 ***************************************************************************/
int devAvme9440Config( int ncards, int a16base, int intvecbase )
{

   if( ncards >= NOCARDS )
   {
      printf( "devAvme9440: Max. number of cards exceeded (%d/%d)\n", ncards, NOCARDS );

      return( ERROR );
   }

   avme9440_link_count  = ncards;
   addrBase             = a16base;
   vecBase              = intvecbase;

   init( 0 );

   return( 0 );
}


/***************************************************************************
 *
 * Common initialization
 *
 ***************************************************************************/
static long init( int flag )
{
UCHAR value;
int card, chan;
volatile avme9440* p;


   /* Evaluate initialization flag */
   if( initFlag == TRUE )
   {
      return( OK );
   }

   /* Indicate initialization attempted */
   initFlag = TRUE;

   /* Register device address */
   if( devRegisterAddress( "devAvme9440", atVMEA16, addrBase, (sizeof(avme9440) * avme9440_link_count), (volatile void**)&p ) )
   {
      if( devAvme9440Debug >= 5 )
      {
         printf( "devAvme9440: can not find short address space\n" );
      }

      return( ERROR );
   }

   /* We end up here 1 time before all records are initialized */
   for( card = 0; card < avme9440_link_count; card++, p++ )
   {

      if( devAvme9440Debug >= 5 )
      {
         printf( "devAvme9440: Initializing card %d\n", card );
      }

      /* Probe module for access */
      if( devReadProbe( 1, &p->boardStatus, &value ) )
      {

         if( devAvme9440Debug >= 5 )
         {
            printf( "devAvme9440: Probe at %p failed\n", &p->boardStatus );
         }

         /* No card found */
         cards[card].card = NULL;

         /* Next instance */
         continue;
      }

      /* Sends init value to LEDs and disables interrupts */
      p->boardStatus = LED_INIT;
      value          = p->boardStatus;

      if( devAvme9440Debug >= 5 )
      {
         printf( "devAvme9440: Probe at %p success, board status = 0x%2.2X\n", &p->boardStatus, value );
         printf( "             Beginning card %d initialization\n", card );
      }

      /* Write interrupt vectors */
      value = vecBase + card;

      for( chan = 0; chan < NOCHANS; chan++ )
      {

         /* Write vector */
         *((char*)&p->intVector0 + (2 * chan)) = value;

         if( devAvme9440Debug >= 5 )
         {
            printf( "devAvme9440: Interrupt vector 0x%2.2X being written to channel %d interrupt\n", value, chan );
            printf( "             vector table entry at address %p\n", ((char*)&p->intVector0 + (2 * chan)) );

            /* Clear channel interrupt counter */
            cards[card].intCnt[chan] = 0;
         }

         /* Interrupt initialized per channel */
         scanIoInit( &cards[card].ioscanpvt[chan] );
      }

      /* Disable interrupts for channels 0-7 */
      value          = 0x00;
      p->intEnable   = value;

      if( devAvme9440Debug >= 5 )
      {
         printf("devAvme9440: Interrupts disabled for channels 0 - 7\n");
      }

      /* Set interrupt level */
      value             = 0xFF;
      p->intTypeSelect  = value;

      if( devAvme9440Debug >= 5 )
      {
         printf( "devAvme9440: Change of state interrupts set for channels 0 - 7\n" );
         printf( "             Interrupt level should be set at %d\n", avme9440_int_level );
      }

      /* Disable input pattern detection interrupts */
      value                = 0x00;
      p->intPaternEnable   = value;

      if( devAvme9440Debug >= 5 )
      {
         printf( "devAvme9440: Input pattern detection interrupts disabled for channels 0 - 7\n" );
      }

      /* Sends init value to LEDs and enables interrupts */
      value          = LED_OKRUN;
      p->boardStatus = value;

      if( devAvme9440Debug >= 5 )
      {
         printf( "devAvme9440: Card %d initialization complete\n", card );
      }

      /* Remember base address */
      cards[card].card = p;

      if( devConnectInterruptVME((vecBase + card), avme9440_isr, &cards[card] ) )
      {
         printf( "devAvme9440: Interrupt connect failed for card %d\n", card );
      }

      /* Create access semaphore */
      cards[card].lock = epicsMutexCreate();

   } /* end-for: (card number) */

   return( OK );
}


/***************************************************************************
 *
 * Interrupt service routine
 *
 ***************************************************************************/
static void avme9440_isr( void *parg )
{
ioCard *pc = (ioCard*) parg;
unsigned int chanNum;
volatile avme9440* p          = pc->card;
volatile UCHAR intStatusLocal = p->intStatus;
volatile UCHAR intStatWrite   = 0;


   for( chanNum = 0; chanNum < NOCHANS; chanNum++ )
   {
      if( intStatusLocal & (1 << chanNum) )
      {
         scanIoRequest( pc->ioscanpvt[chanNum] );
         intStatWrite |= (1 << chanNum);
      }

   }

   p->intStatus = intStatWrite;
}


/**************************************************************************
 *
 * BO Initialization (Called one time for each BO AVME9440 card record)
 *
 **************************************************************************/
static long init_bo_record( boRecord* pbo )
{
int status = 0;
USHORT stupid;

   /* bo.out must be an VME_IO */
   switch( pbo->out.type )
   {
   case VME_IO:
      if( pbo->out.value.vmeio.signal > 15 )
      {
         pbo->pact   = 1;          /* make sure we don't process this thing */
         status      = S_db_badField;

         if( devAvme9440Debug >= 10 )
         {
            printf( "devAvme9440: Illegal SIGNAL field ->%s<- \n", pbo->name );
         }

         recGblRecordError( status, (void*)pbo, "devAvme9440 (init_record) Illegal SIGNAL field" );
      }
      else
      {
         pbo->mask = 1;
         pbo->mask <<= pbo->out.value.vmeio.signal;

         if( read_card( pbo->out.value.vmeio.card, pbo->mask, &stupid, OUTPUT_REG ) == OK )
         {
            pbo->rbv = pbo->rval = stupid;

            if( devAvme9440Debug >= 10 )
            {
               printf( "devAvme9440: ->%s<- Record initialized \n", pbo->name );
            }

         }
         else
         {
            status = 2;

            if( devAvme9440Debug >= 10 )
            {
               printf( "devAvme9440: ->%s<- Record failed to initialize \n", pbo->name );
            }

         }

      }
      break;

   default:
      pbo->pact   = 1;    /* make sure we don't process this thing */
      status      = S_db_badField;

      if( devAvme9440Debug >= 10 )
      {
         printf( "devAvme9440: Illegal OUT field ->%s<- \n", pbo->name );
      }

      recGblRecordError( status, (void*)pbo, "devAvme9440 (init_record) Illegal OUT field" );
      break;

   }

   return( status );
}


/***************************************************************************
 *
 * BI Initialization (Called one time for each BI AVME9440 card record)
 *
 ***************************************************************************/
static long init_bi_record( biRecord* pbi )
{
int status = 0;
USHORT stupid;

   switch( pbi->inp.type )
   {
   case VME_IO:
      if( pbi->inp.value.vmeio.signal > 31 )
      {
         pbi->pact   = 1;          /* make sure we don't process this thing */
         status      = S_db_badField;

         if( devAvme9440Debug >= 10 )
         {
            printf( "devAvme9440: Illegal SIGNAL field ->%s<-\n", pbi->name );
         }

         recGblRecordError( status, (void*)pbi, "devAvme9440 (init_record) Illegal SIGNAL field" );
      }
      else
      {
         pbi->mask = 1;

         if( pbi->inp.value.vmeio.signal <= 15 )
         {
            pbi->mask <<= pbi->inp.value.vmeio.signal;
         }
         else
         {
            pbi->mask <<= pbi->inp.value.vmeio.signal - 16;
         }

         if( read_card( pbi->inp.value.vmeio.card, pbi->mask, &stupid, INPUT_REG ) == OK )
         {
            pbi->rval = stupid;

            if( devAvme9440Debug >= 10 )
            {
               printf(" devAvme9440: ->%s<- Record initialized\n", pbi->name );
            }

         }
         else
         {
            status = 2;

            if( devAvme9440Debug >= 10 )
            {
               printf( "devAvme9440: ->%s<- Record failed to initialize\n", pbi->name );
            }

         }

      }
      break;

   default:
      pbi->pact   = 1;     /* make sure we don't process this thing */
      status      = S_db_badField;

      if( devAvme9440Debug >= 10 )
      {
         printf( "devAvme9440: Illegal INP field ->%s<- \n", pbi->name );
      }

      recGblRecordError( status, (void*)pbi, "devAvme9440 (init_record) Illegal INP field" );
      break;

   }

   return( status );
}


/***************************************************************************
 *
 * MBBO Initialization (Called one time for each MBBO AVME9440 card record)
 *
 ***************************************************************************/
static long init_mbbo_record( mbboRecord* pmbbo )
{
USHORT stupid;
int status = 0;
struct vmeio* pvmeio;

   /* mbbo.out must be an VME_IO */
   switch( pmbbo->out.type )
   {
   case VME_IO:
      if( pmbbo->out.value.vmeio.signal > 15 )
      {
         pmbbo->pact = 1;          /* make sure we don't process this thing */
         status = S_db_badField;

         if( devAvme9440Debug >= 10 )
         {
            printf( "devAvme9440: Illegal SIGNAL field ->%s<- \n", pmbbo->name );
         }

         recGblRecordError( status, (void*)pmbbo, "devAvme9440 (init_record) Illegal SIGNAL field" );
      }
      else
      {
         pvmeio = &pmbbo->out.value.vmeio;
         pmbbo->shft = pvmeio->signal;
         pmbbo->mask <<= pmbbo->shft;

         if( read_card( pmbbo->out.value.vmeio.card, pmbbo->mask, &stupid, OUTPUT_REG ) == OK )
         {
            pmbbo->rbv = pmbbo->rval = stupid;

            if( devAvme9440Debug >= 10 )
            {
               printf( "devAvme9440: ->%s<- Record initialized \n", pmbbo->name );
            }

         }
         else
         {
            status = 2;

            if( devAvme9440Debug >= 10 )
            {
               printf( "devAvme9440: ->%s<- Record failed to initialize \n", pmbbo->name );
            }

         }

      }
      break;

   default:
      pmbbo->pact = 1;          /* make sure we don't process this thing */
      status = S_db_badField;

      if( devAvme9440Debug >= 10 )
      {
         printf( "devAvme9440: Illegal OUT field ->%s<- \n", pmbbo->name );
      }

      recGblRecordError( status, (void*)pmbbo, "devAvme9440 (init_record) Illegal OUT field" );
      break;

   }

   return( status );
}


/***************************************************************************
 *
 * MBBI Initialization (Called one time for each MBBI AVME9440 card record)
 *
 ***************************************************************************/
static long init_mbbi_record( mbbiRecord* pmbbi )
{
int status = 0;
USHORT stupid;

   /* mbbi.inp must be an VME_IO */
   switch( pmbbi->inp.type )
   {
   case VME_IO:
      if( pmbbi->inp.value.vmeio.signal > 31 )
      {
         pmbbi->pact = 1;          /* make sure we don't process this thing */
         status      = S_db_badField;

         if( devAvme9440Debug >= 10 )
         {
            printf( "devAvme9440: Illegal SIGNAL field ->%s<- \n", pmbbi->name );
         }

         recGblRecordError( status, (void*)pmbbi, "devAvme9440 (init_record) Illegal SIGNAL field" );
      }
      else
      {
         if( pmbbi->inp.value.vmeio.signal <= 15 )
         {
            pmbbi->shft = pmbbi->inp.value.vmeio.signal;
         }
         else
         {
            pmbbi->shft = pmbbi->inp.value.vmeio.signal - 16;
         }

         pmbbi->mask <<= pmbbi->shft;

         if( read_card( pmbbi->inp.value.vmeio.card, pmbbi->mask, &stupid, INPUT_REG ) == OK )
         {
            pmbbi->rval = stupid;

            if( devAvme9440Debug >= 10 )
            {
               printf( "devAvme9440: ->%s<- Record initialized \n", pmbbi->name );
            }

         }
         else
         {
            status = 2;

            if( devAvme9440Debug >= 10 )
            {
               printf( "devAvme9440: ->%s<- Record failed to initialize \n", pmbbi->name );
            }

         }

      }
      break;

   default:
      pmbbi->pact = 1;              /* make sure we don't process this thing */
      status      = S_db_badField;

      if( devAvme9440Debug >= 10 )
      {
         printf( "devAvme9440: Illegal INP field ->%s<- \n", pmbbi->name );
      }

      recGblRecordError( status, (void*)pmbbi, "devAvme9440 (init_record) Illegal INP field" );
      return( status );
   }

return( 0 );
}


/***************************************************************************
 *
 * Perform a write operation from a BO record
 *
 ***************************************************************************/
static long write_bo( boRecord* pbo )
{
USHORT stupid;

   if( write_card( pbo->out.value.vmeio.card, pbo->mask, pbo->rval ) == OK )
   {

      if( read_card( pbo->out.value.vmeio.card, pbo->mask, &stupid, OUTPUT_REG ) == OK )
      {
         pbo->rbv = stupid;
         return( 0 );
      }

   }

   /* Set an alarm for the record */
   recGblSetSevr( pbo, WRITE_ALARM, INVALID_ALARM );

   return( 2 );
}


/***************************************************************************
 *
 * Perform a read operation from a BI record
 *
 ***************************************************************************/
static long read_bi( biRecord* pbi )
{
unsigned int reg;
USHORT stupid;

   if( pbi->inp.value.vmeio.signal <= 15 )
   {
      reg = INPUT_REG;
   }
   else
   {
      reg = OUTPUT_REG;
   }

   if( read_card( pbi->inp.value.vmeio.card, pbi->mask, &stupid, reg ) == OK )
   {
      pbi->rval = stupid;
      return( 0 );
   }

   /* Set an alarm for the record */
   recGblSetSevr( pbi, READ_ALARM, INVALID_ALARM );

   return( 2 );
}


/***************************************************************************
 *
 * Perform a write operation from a MBBO record
 *
 ***************************************************************************/
static long write_mbbo( mbboRecord* pmbbo )
{
USHORT stupid;

   if( write_mbbo_card( pmbbo->out.value.vmeio.card, pmbbo->mask, pmbbo->rval ) == OK )
   {

      if( read_card( pmbbo->out.value.vmeio.card, pmbbo->mask, &stupid, OUTPUT_REG ) == OK )
      {
         pmbbo->rbv = stupid;
         return( 0 );
      }

   }

   /* Set an alarm for the record */
   recGblSetSevr( pmbbo, WRITE_ALARM, INVALID_ALARM );

   return( 2 );
}


/***************************************************************************
 *
 * Perform a read operation from a MBBI record
 *
 ***************************************************************************/
static long read_mbbi( mbbiRecord* pmbbi )
{
unsigned int reg;
USHORT stupid;

   if( pmbbi->inp.value.vmeio.signal <= 15 )
   {
      reg = INPUT_REG;
   }
   else
   {
      reg = OUTPUT_REG;
   }

   if( read_card( pmbbi->inp.value.vmeio.card, pmbbi->mask, &stupid, reg ) == OK )
   {
      pmbbi->rval = stupid;
      return( 0 );
   }

   /* Set an alarm for the record */
   recGblSetSevr( pmbbi, READ_ALARM, INVALID_ALARM );

   return( 2 );
}


/***************************************************************************
 *
 * Raw read a bitfield from the card
 *
 ***************************************************************************/
static long read_card( short card, ULONG mask, USHORT* value, int reg )
{

   if( checkLink( card ) == ERROR )
   {
      return( ERROR );
   }

   if( reg == INPUT_REG )
   {
      *value = cards[card].card->inputData & mask;
   }
   else
   {
      *value = cards[card].card->outputData & mask;
   }

   if( devAvme9440Debug >= 20 )
   {
      printf( "devAvme9440: read 0x%4.4X from card %d\n", *value, card );
   }

   return( OK );
}


/***************************************************************************
 *
 * Raw write a bitfield to the card
 *
 ***************************************************************************/
static long write_card( short card, ULONG mask, ULONG value )
{

   if( checkLink( card ) == ERROR )
   {
      return( ERROR );
   }

   epicsMutexMustLock( cards[card].lock );
   cards[card].card->outputData = (cards[card].card->outputData & ~mask) | value;
   epicsMutexUnlock( cards[card].lock );

   if( devAvme9440Debug >= 15 )
   {
      printf( "devAvme9440: wrote 0x%4.4X to card %d\n", cards[card].card->outputData, card );
   }

   return( 0 );
}


/***************************************************************************
 *
 * Raw write a bitfield to the card for MBBO records
 *
 ***************************************************************************/
static long write_mbbo_card( short card, ULONG mask, ULONG value )
{

   if( checkLink( card ) == ERROR )
   {
      return( ERROR );
   }

   epicsMutexMustLock( cards[card].lock );
   cards[card].card->outputData = ((cards[card].card->outputData & ~mask) | (value & mask));
   epicsMutexUnlock( cards[card].lock );

   if( devAvme9440Debug >= 15 )
   {
      printf( "devAvme9440: wrote 0x%4.4X to card %d\n", cards[card].card->outputData, card );
   }

   return( 0 );
}


/***************************************************************************
 *
 * Make sure card number is valid
 *
 ***************************************************************************/
static long checkLink( short card )
{

   if( card >= avme9440_link_count )
   {
      return( ERROR );
   }

   if( cards[card].card == NULL )
   {
      return( ERROR );
   }

   return( OK );
}


/***************************************************************************
 *
 * BI record interrupt routine
 *
 ***************************************************************************/
static long get_bi_int_info( int cmd, biRecord* pbi, IOSCANPVT* ppvt )
{
struct vmeio* pvmeio;
volatile avme9440* pc;

   pvmeio   = (struct vmeio*)&pbi->inp.value;

   pc       = cards[pvmeio->card].card;
   *ppvt    = cards[pvmeio->card].ioscanpvt[pvmeio->signal];

   if( cmd == ADDING )
   {
      /* enable interrupts */
      devEnableInterruptLevel( intVME, avme9440_int_level );

      pc->boardStatus   = LED_OKINTS;                 /* Initialize global board interrupts */
      pc->intEnable     |= (1 << pvmeio->signal);     /* Initialize individual channel interrupts */
      cards[pvmeio->card].intCnt[pvmeio->signal]++;
   }
   else
   {

      if( !(--(cards[pvmeio->card].intCnt[pvmeio->signal])) )
      {
         pc->intEnable  &= ~(1 << pvmeio->signal);    /* Disable individual channel interrupts */
      }

   }

   return( 0 );
}

