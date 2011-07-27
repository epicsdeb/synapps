/*+ devAvmeMRD.c

                          Argonne National Laboratory
                            APS Operations Division
                     Beamline Controls and Data Acquisition

                             MRD-100 Device Support



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
   This module provides device support for the MRD-100 VMEbus module. The
   module is found in the A32/D32 VME address space and supports the ai,
   bi, bo, longin, and mbbi record types. The VME configuration is specificed
   in the INP or OUT fields. Below shows the specific format for the given
   record types:

      ai       "Cn Sr @s,w,t"
      bi       "Cn Sr @s"
      bo       "Cn Sr @s"
      longin   "Cn Sr @s,w"
      mbbi     "Cn Sr @s"

      Where:
         n  - Card number (always set to 0).
         r  - Register number 'r'.
         s  - Data start bit number (0..31).
         w  - Data width (1..32).
         t  - Data type which can be either 0 for normal (unipolar) or
              1 (bipolar) for a twos complement signal.

   Diagnostic messages can be output by setting the variable devAvmeMRDDebug
   to the following values:

   When:
      devAvmeMRDDebug  =  0, outputs no messages.
      devAvmeMRDDebug >=  5, outputs write command messages.
      devAvmeMRDDebug >= 10, outputs read command messages.

   The method devAvmeMRDConfig is called from the startup script to specify the
   VME address and interrupt configuration. It must be called prior to iocInit().
   Below is the calling sequence:

      devAvmeMRDConfig( base, vector, level )

      Where:
         base     - VME A32/D32 address space.
         vector   - Interrupt vector (0..255).
         level    - Interrupt request level (1..7).

      For example:
         devAvmeMRDConfig(0xB0000200, 0xA0, 5)

 Developer notes:
   1) Conventions:
      - Public methods are prepended with 'devAvmeMRD'.
      - Local symbolic constants are prepended with 'MRD__' (two underscores).
        Local methods have '__' (two underscores) in the name.
      - Symbolic constants types:
        K   - constant.
        M   - mask.
        S   - size (in bytes).
        STS - status.
      - Typedefs:
        r   - is a struct.
        u   - is a union.
   2) Only one instance of the MRD-100 is supported.

 =============================================================================
 History:
 Author: David M. Kline (Derived from devA32VME)
 -----------------------------------------------------------------------------
 22-12-04   DMK   Taken from the existing devA32Vme device support. Reworked
                  driver for clarity and efficiency.
 14-01-05   DMK   Modified given 'code inspection' comments and suggestions.
 -----------------------------------------------------------------------------

-*/


/* EPICS base version-specific definitions (must be performed first) */
#include <epicsVersion.h>
#define MRD__IS_EPICSBASE(v,r,l)  \
   ((EPICS_VERSION==(v)) && (EPICS_REVISION==(r)) && (EPICS_MODIFICATION==(l)))
#define MRD__GT_EPICSBASE(v,r,l)  \
   ((EPICS_VERSION>=(v)) && (EPICS_REVISION>=(r)) && (EPICS_MODIFICATION>(l)))


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
#include <aiRecord.h>
#include <boRecord.h>
#include <biRecord.h>
#include <mbbiRecord.h>
#include <longinRecord.h>


/* EPICS base version-specific include files */
#if ( MRD__GT_EPICSBASE(3,14,6) )
   #include <epicsExit.h>
#else

   #ifdef vxWorks
      #include <rebootLib.h>
   #endif

#endif


/* Define general symbolic constants */
#define MRD__K_ACTIVE      (  1 )         /* Record active */
#define MRD__K_INACTIVE    (  0 )         /* Record inactive */

#define MRD__K_OPRREGS     ( 27 )         /* # of operational registers */
#define MRD__K_MAXREGS     ( 30 )         /* Max. # of registers */
#define MRD__K_MAXBITS     ( 32 )         /* Max. width of data */
#define MRD__K_MAXADDR     ( 0xF0000000 ) /* Max. base address */

#define MRD__K_BIPOLAR     (  1 )         /* AI/AO biplor value */
#define MRD__K_UNIPOLAR    (  0 )         /* AI/AO unipolar value */

#define MRD__K_SHOWNONE    (  0 )         /* Diagnostic level indicators */
#define MRD__K_SHOWWRITE   (  5 )
#define MRD__K_SHOWREAD    ( 10 )

#define MRD__K_MAXCOL      (  3 )         /* Max. # of columns in report */


/* Define symbolic constants for device-support method completion status */
#define MRD__STS_OK        (  0 )         /* OK */
#define MRD__STS_OKNOVAL   (  2 )         /* OK - do not modify VAL */
#define MRD__STS_ERROR     (  ERROR )     /* FAILURE */


/* Declare MRD-100 register map and symbolic constants */
#define MRD__M_IVEC        ( 0xFF )       /* Interrupt Vector mask */
#define MRD__M_ICR         ( 0x10 )       /* Interrupt Control Register mask */
#define MRD__M_IDC         ( 0x0F )       /* Interrupt Detect/Clear mask */

#define MRD__K_IVEC        ( 0xFF )       /* Interrupt Vector value */
#define MRD__K_ICR         ( 0x10 )       /* Interrupt Control Register value */
#define MRD__K_IDC         ( 0x0F )       /* Interrupt Detect/Clear value */

#define MRD__S_REGS        ( sizeof(uMRD__REGS) )

typedef union uMRD__REGS
{
   ULONG data[MRD__K_MAXREGS];            /* Linear register map */

   struct rREGS
   {
      ULONG regs[MRD__K_OPRREGS];         /* Operational Registers   (0..26) */
      ULONG IDC;                          /* Interrupt Dectect/Clear    (27) */
      ULONG ICR;                          /* Interrupt Control          (28) */
      ULONG IVEC;                         /* Interrupt Vector           (29) */
   } rREGS;

} uMRD__REGS;


/* Declare MRD info structure */
#define MRD__S_INFO        ( sizeof(rMRD__INFO) )

typedef struct rmrd__info
{
   uMRD__REGS*    base;          /* Base address */
   ULONG          card;          /* Card number */
   epicsMutexId   lock;          /* Access sync. */
   ULONG          recReadCount;  /* Record read count */
   ULONG          recWritCount;  /* Record write count */
   ULONG          recInstCount;  /* Record instance count */
   IOSCANPVT      ioscanpvt;     /* scan IO event */

} rMRD__INFO;


/* Declare MRD record instance structure */
#define MRD__S_INST        ( sizeof(rMRD__INST) )

typedef struct rMRD__INST
{
   rMRD__INFO* pmrd;             /* Pointer to MRD info */
   ULONG*      pdata;            /* Pointer to data register */
   ULONG       dmask;            /* Data mask */
   ULONG       dsbn;             /* Data start bit number */
   ULONG       dwid;             /* Data width */
   ULONG       dtyp;             /* Data type (0=unipolar / 1=bipolar) */

} rMRD__INST;


/* Declare DSET data structure */
typedef struct rMRD__DSET
{
   long      number;             /* # of method pointers */
   DEVSUPFUN report;             /* Reports device support information */
   DEVSUPFUN init;               /* Device support initialization */
   DEVSUPFUN init_record;        /* Record support initialization */
   DEVSUPFUN get_ioint_info;     /* Associate interrupt source with record */
   DEVSUPFUN read_write;         /* Read/Write method */
   DEVSUPFUN specialLinconv;     /* Special processing for AO/AI records */

} rMRD__DSET;


/* Define local variants (one instance of MRD-100 only) */
static rMRD__INFO rMRD__info;


/* Declare local forward references for device-processing methods */
static void mrd__isr( rMRD__INFO* );
static long mrd__read( ULONG*, ULONG, ULONG* );
static long mrd__write( ULONG*, ULONG, ULONG );
static void mrd__reboot( void* );
static long mrd__report( int );


/* Declare local forward references for record-processing methods */
static rMRD__INST* mrd__inputCommon( ULONG, char*, struct vmeio* );
static long        mrd__compareCommon( short, char*, struct vmeio* );


static long ai__init( struct aiRecord* );
static long ai__read( struct aiRecord* );

static long bi__init( struct biRecord* );
static long bi__read( struct biRecord* );
static long bi__getIntInfo( int, struct biRecord*, IOSCANPVT* );

static long bo__init( struct boRecord* );
static long bo__write( struct boRecord* );

static long longin__init( struct longinRecord* );
static long longin__read( struct longinRecord* );

static long mbbi__init( struct mbbiRecord* );
static long mbbi__read( struct mbbiRecord* );


/* Define DSET structures */
static rMRD__DSET devAiAvmeMRD   =
   {6, NULL,        NULL, ai__init,      NULL,           ai__read,       NULL};
static rMRD__DSET devBiAvmeMRD   =
   {5, mrd__report, NULL, bi__init,      bi__getIntInfo, bi__read,       NULL};
static rMRD__DSET devBoAvmeMRD   =
   {5, NULL,        NULL, bo__init,      NULL,           bo__write,      NULL};
static rMRD__DSET devLiAvmeMRD   =
   {5, NULL,        NULL, longin__init,  NULL,           longin__read,   NULL};
static rMRD__DSET devMbbiAvmeMRD =
   {5, NULL,        NULL, mbbi__init,    NULL,           mbbi__read,     NULL};


/* Publish DSET structure references to EPICS */
epicsExportAddress(dset, devAiAvmeMRD);
epicsExportAddress(dset, devBiAvmeMRD);
epicsExportAddress(dset, devBoAvmeMRD);
epicsExportAddress(dset, devLiAvmeMRD);
epicsExportAddress(dset, devMbbiAvmeMRD);


/* Define global variants */
ULONG devAvmeMRDDebug = 0;


/****************************************************************************
 * Define device-specific methods
 ****************************************************************************/


/*
 * mrd__isr()
 *
 * Description:
 *    This method is executed when an interrupt occurs from the MRD-100.
 *
 * Input Parameters:
 *    pmrd  - Address of MRD information structure.
 *
 * Output Parameters:
 *    None.
 *
 * Returns:
 *    None.
 *
 * Developer notes:
 *
 */
static void mrd__isr( rMRD__INFO* pmrd )
{
   /* Process scan IO requests */
   scanIoRequest( pmrd->ioscanpvt );

   /* Reenable interrupts */
   pmrd->base->rREGS.IDC = MRD__K_IDC;
}


/*
 * mrd__reboot()
 *
 * Description:
 *    This method is executed when a 'reboot' command is initiated. It will
 *    disable all interrupts.
 *
 * Input Parameters:
 *    None.
 *
 * Output Parameters:
 *    None.
 *
 * Returns:
 *    None.
 *
 * Developer notes:
 *
 */
static void mrd__reboot( void* pvoid )
{
   rMRD__info.base->rREGS.ICR = 0;
}


/*
 * mrd__report()
 *
 * Description:
 *    This method is called from the dbior shell command. It outputs
 *    information about the MRD-100 configuration.
 *
 * Input Parameters:
 *    level - Indicates interest level of information.
 *
 * Output Parameters:
 *    None.
 *
 * Returns:
 *    Completion status: MRD__STS_OK
 *
 * Developer notes:
 *    1) Interest level > 0 displays operational registers.
 *
 */
static long mrd__report( int level )
{

   /* Output MRD-100 information */
   printf( "\nMRD-100 Configuration\n" );
   printf( "\tBase address                    - 0x%8.8X\n", (UINT)rMRD__info.base );
   printf( "\tInterrupt vector                - 0x%4.4X\n", (UINT)rMRD__info.base->rREGS.IVEC );
   printf( "\tInterrupt control register      - 0x%4.4X\n", (UINT)rMRD__info.base->rREGS.ICR );
   printf( "\tInterrupt detect/clear register - 0x%4.4X\n", (UINT)rMRD__info.base->rREGS.IDC );
   printf( "\tRecord read count               - %d\n",      (UINT)rMRD__info.recReadCount );
   printf( "\tRecord write count              - %d\n",      (UINT)rMRD__info.recWritCount );
   printf( "\tRecord instance count           - %d\n",      (UINT)rMRD__info.recInstCount );
   printf( "\tEPICS release version           - %s\n",      epicsReleaseVersion );

   /* Output MRD-100 operational registers */
   if( level > 0 )
   {
   UINT i, r;

      printf( "\tOperational register contents:\n" );
      for( i = 0, r = 1; i < MRD__K_OPRREGS; ++i, ++r )
      {

         /* Output register contents */
         printf( "\t%2.2d - 0x%8.8X", i, (UINT)rMRD__info.base->data[i] );

         /* Evaluate row break */
         if( (r % MRD__K_MAXCOL) == 0 )
         {
            printf( "\n" );
         }

      }

      printf( "\n" );

   }

   /* Return completion status */
   return( MRD__STS_OK );

}


/*
 * mrd__write()
 *
 * Description:
 *    This method performs the longword write to the MRD-100 given the address.
 *
 * Input Parameters:
 *    paddr    - Address to write.
 *    mask     - Mask to preserve existing bits.
 *    value    - Value to write.
 *
 * Output Parameters:
 *    None.
 *
 * Returns:
 *    Completion status: MRD__STS_OK
 *
 * Developer notes:
 *
 */
static long mrd__write( ULONG* paddr, ULONG mask, ULONG value )
{

   /* Acquire access mutex */
   epicsMutexMustLock( rMRD__info.lock );

   /* Write raw value given mask */
   *paddr = (*paddr & ~mask) | (value & mask);

   /* Release access mutex */
   epicsMutexUnlock( rMRD__info.lock );

   /* Increment write count */
   ++rMRD__info.recWritCount;

   /* Output diagnostic */
   if( devAvmeMRDDebug >= MRD__K_SHOWWRITE )
   {
      epicsPrintf( "devAvmeMRD:mrd__write(): Wrote 0x%8.8X to 0x%8.8X\n", (UINT)*paddr, (UINT)paddr );
   }

   /* Return completion status */
   return( MRD__STS_OK );

} /* end-method: mrd__write() */


/*
 * mrd__read()
 *
 * Description:
 *    This method performs the longword read from the MRD-100 given the address.
 *
 * Input Parameters:
 *    paddr    - Address to read.
 *    mask     - Mask to preserve existing bits.
 *
 * Output Parameters:
 *    pvalue   - Address of value read.
 *
 * Returns:
 *    Completion status: MRD__STS_OK
 *
 * Developer notes:
 *
 */
static long mrd__read( ULONG* paddr, ULONG mask, ULONG* pvalue )
{

   /* Read raw value from MRD and mask */
   *pvalue = *paddr & mask;

   /* Increment read count */
   ++rMRD__info.recReadCount;

   /* Output diagnostic */
   if( devAvmeMRDDebug >= MRD__K_SHOWREAD )
   {
      epicsPrintf( "devAvmeMRD:mrd__read(): Read 0x%8.8X from 0x%8.8X\n", (UINT)*pvalue, (UINT)paddr );
   }

   /* Return completion status */
   return( MRD__STS_OK );

} /* end-method: mrd__read() */


/****************************************************************************
 * Define record-specific methods
 ****************************************************************************/


/*
 * mrd__inputCommon()
 *
 * Description:
 *    This method provides the common input specifications for record types.
 *
 * Input Parameters:
 *    count    - # of input specification parameters.
 *    precnam  - Pointer to record name string.
 *    pvmeio   - Pointer to VME IO information.
 *
 * Output Parameters:
 *    NONE.
 *
 * Returns:
 *    Completion status:   Pointer to rMRD__INST structure
 *                         NULL
 *
 * Developer notes:
 *
 */
static rMRD__INST* mrd__inputCommon( ULONG count, char* precnam, struct vmeio* pvmeio )
{
int         cnt;
UINT        start,
            width,
            type;
rMRD__INST* pinst;


   /* Initialize local variants */
   cnt   = 0;
   start = 0;
   width = 0;
   type  = 0;

   /* Acquire data start bit number, width, and data type */
   switch( count )
   {
   case 1:
      cnt = sscanf( pvmeio->parm, "%d", &start );
      break;

   case 2:
      cnt = sscanf( pvmeio->parm, "%d,%d", &start, &width );
      break;

   case 3:
      cnt = sscanf( pvmeio->parm, "%d,%d,%d", &start, &width, &type );
      break;
   }

   /* Evaluate return count */
   if( cnt != count )
   {
      printf( "devAvmeMRD: Unexpected returned count (%d/%d) from sscanf() for %s\n", cnt, (UINT)count, precnam );

      return( NULL );
   }

   /* Evaluate starting bit number */
   if( count >= 1 )
   {

      if( start >= MRD__K_MAXBITS )
      {
         printf( "devAvmeMRD: Invalid start bit number %d specified for %s\n", start, precnam );

         return( NULL );
      }

   }

   /* Evaluate bit width */
   if( count >= 2 )
   {

      if( (width <= 0) || ((start + width) > MRD__K_MAXBITS) )
      {
         printf( "devAvmeMRD: Invalid bit width %d specified for %s\n", width, precnam );

         return( NULL );
      }

   }

   /* Evaluate bit type */
   if( count >= 3 )
   {

      if( type < 0 || type > 1 )
      {
         epicsPrintf( "devAvmeMRD: Invalid bit type %d specified for %s\n", type, precnam );

         return( NULL );
      }

   }

   /* Allocate memory for record instance */
   pinst = calloc( 1, MRD__S_INST );
   if( pinst == NULL )
   {
      printf( "devAvmeMRD: Failure to allocate instance memory for %s\n", precnam );

      return( NULL );
   }

   /* Initialize MRD record instance structure */
   pinst->pmrd    = &rMRD__info;
   pinst->pdata   = &rMRD__info.base->data[pvmeio->signal];
   pinst->dsbn    = start;
   pinst->dtyp    = type;
   pinst->dwid    = width;

   /* Return instance pointer */
   return( pinst );

} /* end-method: mrd__inputCommon() */


/*
 * mrd__compareCommon()
 *
 * Description:
 *    This method provides the common comparisons for record types.
 *
 * Input Parameters:
 *    type     - Input type.
 *    precnam  - Pointer to record name string.
 *    pvmeio   - Pointer to VME IO information.
 *
 * Output Parameters:
 *    NONE.
 *
 * Returns:
 *    Completion status:   MRD__STS_OK
 *                         MRD__STS_ERROR
 *
 * Developer notes:
 *
 */
static long mrd__compareCommon( short type, char* precnam, struct vmeio* pvmeio )
{

   /* Evaluate input specification type */
   if( type != VME_IO )
   {
      printf( "devAvmeMRD: Invalid input specification type %s\n", precnam );

      return( MRD__STS_ERROR );
   }

   /* Evaluate base address */
   if( rMRD__info.base == NULL )
   {
      printf( "devAvmeMRD: Base address not specified for %s\n", precnam );

      return( MRD__STS_ERROR );
   }

   /* Evaluate card number */
   if( pvmeio->card != rMRD__info.card )
   {
      printf( "devAvmeMRD: Invalid card number %d for %s\n", (UINT)pvmeio->card, precnam );

      return( MRD__STS_ERROR );
   }

   /* Evaluate register number */
   if( pvmeio->signal >= MRD__K_MAXREGS )
   {
      printf( "devAvmeMRD: Invalid register number %d specified for %s\n", pvmeio->signal, precnam );

      return( MRD__STS_ERROR );
   }

   /* Return completion status */
   return( MRD__STS_OK );

} /* end-method: mrd__compareCommon() */


/*
 * ai__init()
 *
 * Description:
 *    This method performs the initialization for an AI record.
 *
 * Input Parameters:
 *    pai   - Address of aiRecord.
 *
 * Output Parameters:
 *    None.
 *
 * Returns:
 *    Completion status:   MRD__STS_OK
 *                         MRD__STS_ERROR
 *
 * Developer notes:
 *
 */
static long ai__init( struct aiRecord* pai )
{
long        sts = MRD__STS_OK;
rMRD__INST* pinst;


   /* Initialize device private pointer */
   pai->dpvt = NULL;

   /* Evaluate common comparisons */
   sts = mrd__compareCommon( pai->inp.type, pai->name, &pai->inp.value.vmeio );
   if( sts != MRD__STS_OK )
   {
      pai->pact   = MRD__K_ACTIVE;

      return( sts );
   }

   /* Evaluate common input specification */
   pinst = mrd__inputCommon( 3, pai->name, &pai->inp.value.vmeio );
   if( pinst == NULL )
   {
      pai->pact   = MRD__K_ACTIVE;

      return( MRD__STS_ERROR );
   }

   /* Update record instance structure */
   pinst->dmask   = ((1 << pinst->dwid) - 1) << pinst->dsbn;

   /* Update AI record structure */
   pai->dpvt      = pinst;
   pai->eslo      = (pai->eguf - pai->egul) / (pow(2, pinst->dwid) - 1);

   if( pinst->dtyp == MRD__K_BIPOLAR )
   {
      pai->roff = pow(2, (pinst->dwid - 1));
   }

   /* Increment record instance count */
   ++rMRD__info.recInstCount;

   /* Return completion status */
   return( sts );

} /* end-method: ai__init() */


/*
 * ai__read()
 *
 * Description:
 *    This method performs the read for an AI record.
 *
 * Input Parameters:
 *    pai   - Address of aiRecord.
 *
 * Output Parameters:
 *    None.
 *
 * Returns:
 *    Completion status:   MRD__STS_OK
 *                         MRD__STS_OKNOVAL
 *
 * Developer notes:
 *
 */
static long ai__read( struct aiRecord* pai )
{

   /* Validate device private pointer */
   if( pai->dpvt == NULL)
   {
      pai->pact = MRD__K_ACTIVE;

      return( MRD__STS_OKNOVAL );
   }

   /* start-block: process read */
   {
   long        sts;
   ULONG       readback;
   rMRD__INST* pinst;

      /* Initialize local variants */
      pinst = (rMRD__INST*)pai->dpvt;

      /* Read register value */
      sts = mrd__read( pinst->pdata, pinst->dmask, &readback );
      if( sts == MRD__STS_OK )
      {
         pai->rval   = readback >> pinst->dsbn;

         /* Process sign extensions for bipolar */
         if( pinst->dtyp == MRD__K_BIPOLAR )
         {
         ULONG value = ( 2 << (pinst->dwid - 2) );

            if( pai->rval & value )
            {
               pai->rval   |= ((2 << 31) - value) * 2;
            }

         }

      }
      else
      {
         sts   = MRD__STS_OKNOVAL;

         /* Set an alarm for the record */
         recGblSetSevr( pai, READ_ALARM, INVALID_ALARM );
      }

      /* Return completion status */
      return( sts );

   } /* end-block: process read */

} /* end-method: ai__read() */


/*
 * bi__init()
 *
 * Description:
 *    This method performs the initialization for a BI record.
 *
 * Input Parameters:
 *    pbi   - Address of the BI record.
 *
 * Output Parameters:
 *    None.
 *
 * Returns:
 *    Completion status:   MRD__STS_OK
 *                         MRD__STS_ERROR
 *
 * Developer notes:
 *
 */
static long bi__init( struct biRecord* pbi )
{
long        sts = MRD__STS_OK;
ULONG       readback;
rMRD__INST* pinst;


   /* Initialize device private pointer */
   pbi->dpvt = NULL;

   /* Evaluate common comparisons */
   sts = mrd__compareCommon( pbi->inp.type, pbi->name, &pbi->inp.value.vmeio );
   if( sts != MRD__STS_OK )
   {
      pbi->pact   = MRD__K_ACTIVE;

      return( sts );
   }

   /* Evaluate common input specification */
   pinst = mrd__inputCommon( 1, pbi->name, &pbi->inp.value.vmeio );
   if( pinst == NULL )
   {
      pbi->pact   = MRD__K_ACTIVE;

      return( MRD__STS_ERROR );
   }

   /* Update record instance structure */
   pinst->dmask   = 1 << pinst->dsbn;

   /* Update BI record structure */
   pbi->dpvt      = pinst;
   pbi->mask      = pinst->dmask;

   /* Initialize record raw input value */
   sts = mrd__read( pinst->pdata, pinst->dmask, &readback );
   if( sts == MRD__STS_OK )
   {
      pbi->rval   = readback;
   }
   else
   {
      sts         = MRD__STS_OKNOVAL;
   }

   /* Increment record instance count */
   ++rMRD__info.recInstCount;

   /* Return completion status */
   return( sts );

} /* end-method: bi__init() */


/*
 * bi__read()
 *
 * Description:
 *    This method performs the read for a BI record.
 *
 * Input Parameters:
 *    pbi   - Address of the BI record.
 *
 * Output Parameters:
 *    None.
 *
 * Returns:
 *    Completion status:   MRD__STS_OK
 *                         MRD__STS_OKNOVAL
 *
 * Developer notes:
 *
 */
static long bi__read( struct biRecord* pbi )
{

   /* Validate device private pointer */
   if( pbi->dpvt == NULL)
   {
      pbi->pact = MRD__K_ACTIVE;

      return( MRD__STS_OKNOVAL );
   }

   /* start-block: proccess read */
   {
   long        sts;
   ULONG       readback;
   rMRD__INST* pinst;


      /* Initialize local variants */
      pinst = (rMRD__INST*)pbi->dpvt;

      /* Read register value */
      sts = mrd__read( pinst->pdata, pinst->dmask, &readback );
      if( sts == MRD__STS_OK )
      {
         pbi->rval   = readback;
      }
      else
      {
         sts         = MRD__STS_OKNOVAL;

         /* Set an alarm for the record */
         recGblSetSevr( pbi, READ_ALARM, INVALID_ALARM );
      }

      /* Return completion status */
      return( sts );

   } /* end-block: process read */

} /* end-method: bi__read() */


/*
 * bi__getIntInfo()
 *
 * Description:
 *    This method is called by the IO interrupt scan task. It is passed a
 *    cmd value of 0 or 1 when the associated record is being put in or
 *    taken out of an IO scan list.
 *
 * Input Parameters:
 *    cmd   - Command.
 *    pbi   - Associated BI record.
 *    ppvt  - Address pointer to IO scan structure.
 *
 * Output Parameters:
 *    None.
 *
 * Developer notes:
 *
 */
static long bi__getIntInfo( int cmd, struct biRecord* pbi, IOSCANPVT* ppvt )
{
long sts;

   /* Acquire IO scan address */
   if( rMRD__info.ioscanpvt )
   {
      sts   = MRD__STS_OK;

      *ppvt = rMRD__info.ioscanpvt;
   }
   else
   {
      sts   = MRD__STS_ERROR;
   }

   /* Return completion status */
   return( sts );

} /* end-method: bi__getIntInfo() */


/*
 * bo__init()
 *
 * Description:
 *    This method performs the initialization for a BO record.
 *
 * Input Parameters:
 *    pbo   - Address of BO record.
 *
 * Output Parameters:
 *    None.
 *
 * Returns:
 *    Completion status:   MRD__STS_OK
 *                         MRD__STS_ERROR
 *
 * Developer notes:
 *
 */
static long bo__init( struct boRecord* pbo )
{
long        sts = MRD__STS_OK;
ULONG       readback;
rMRD__INST* pinst;


   /* Initialize device private pointer */
   pbo->dpvt = NULL;

   /* Evaluate common comparisons */
   sts = mrd__compareCommon( pbo->out.type, pbo->name, &pbo->out.value.vmeio );
   if( sts != MRD__STS_OK )
   {
      pbo->pact   = MRD__K_ACTIVE;

      return( sts );
   }

   /* Evaluate common output specification */
   pinst = mrd__inputCommon( 1, pbo->name, &pbo->out.value.vmeio );
   if( pinst == NULL )
   {
      pbo->pact   = MRD__K_ACTIVE;

      return( MRD__STS_ERROR );
   }

   /* Update record instance structure */
   pinst->dmask   = 1 << pinst->dsbn;

   /* Update BO record structure */
   pbo->dpvt      = pinst;
   pbo->mask      = pinst->dmask;

   /* Initialize record raw input value */
   sts = mrd__read( pinst->pdata, pinst->dmask, &readback );
   if( sts == MRD__STS_OK )
   {
      pbo->rbv = readback;
   }
   else
   {
      sts      = MRD__STS_OKNOVAL;
   }

   /* Increment record instance count */
   ++rMRD__info.recInstCount;

   /* Return completion status */
   return( sts );

} /* end-method: bo__init() */


/*
 * bo__write()
 *
 * Description:
 *    This method performs the write for a BO record.
 *
 * Input Parameters:
 *    pbo   - Address of BO record.
 *
 * Output Parameters:
 *    None.
 *
 * Returns:
 *    Completion status:   MRD__STS_OK
 *                         MRD__STS_OKNOVAL
 *
 * Developer notes:
 *
 */
static long bo__write( struct boRecord* pbo )
{

   /* Validate device private pointer */
   if( pbo->dpvt == NULL)
   {
      pbo->pact = MRD__K_ACTIVE;

      return( MRD__STS_OKNOVAL );
   }

   /* start-block: process write */
   {
   long        sts;
   rMRD__INST* pinst;


      /* Initialize local variants */
      pinst = (rMRD__INST*)pbo->dpvt;

      /* Write register value */
      sts = mrd__write( pinst->pdata, pinst->dmask, pbo->rval );
      if( sts == MRD__STS_OK )
      {
      ULONG  readback;

         /* Read register value */
         sts = mrd__read( pinst->pdata, pinst->dmask, &readback );
         if( sts == MRD__STS_OK )
         {

            /* Compare read / write data */
            if( (pinst->dmask & pbo->rval) != readback )
            {
               pbo->pact = MRD__K_ACTIVE;
               recGblSetSevr( pbo, READ_ALARM, INVALID_ALARM );

               return( MRD__STS_OKNOVAL );
            }

            /* Assign readback to record */
            pbo->rbv = readback;

            /* Return completion status */
            return( MRD__STS_OK );
         }

      }

      /* Set an alarm for the record */
      recGblSetSevr( pbo, WRITE_ALARM, INVALID_ALARM );

      /* Return completion status */
      return( MRD__STS_OKNOVAL );

   } /* end-block: process write */

} /* end-method: bo__write() */


/*
 * longin__init()
 *
 * Description:
 *    This method performs the initialization for a LONGIN record.
 *
 * Input Parameters:
 *    pli   - Address of LOGIN record.
 *
 * Output Parameters:
 *    None.
 *
 * Returns:
 *    Completion status:   MRD__STS_OK
 *                         MRD__STS_OKNOVAL
 *
 * Developer notes:
 *
 */
static long longin__init( struct longinRecord* pli )
{
long        sts = MRD__STS_OK;
rMRD__INST* pinst;


   /* Initialize device private pointer */
   pli->dpvt = NULL;

   /* Evaluate common comparisons */
   sts = mrd__compareCommon( pli->inp.type, pli->name, &pli->inp.value.vmeio );
   if( sts != MRD__STS_OK )
   {
      pli->pact   = MRD__K_ACTIVE;

      return( sts );
   }

   /* Evaluate common input specification */
   pinst = mrd__inputCommon( 2, pli->name, &pli->inp.value.vmeio );
   if( pinst == NULL )
   {
      pli->pact   = MRD__K_ACTIVE;

      return( MRD__STS_ERROR );
   }

   /* Update record instance structure */
   pinst->dmask   = ((1 << pinst->dwid) - 1) << pinst->dsbn;

   /* Update LONGIN record structure */
   pli->dpvt      = pinst;

   /* Increment record instance count */
   ++rMRD__info.recInstCount;

   /* Return completion status */
   return( sts );

} /* end-method: longin__init() */


/*
 * longin_read()
 *
 * Description:
 *    This method performs the read for a LONGIN record.
 *
 * Input Parameters:
 *    pli   - Address of LONGIN record.
 *
 * Output Parameters:
 *    None.
 *
 * Returns:
 *    Completion status:   MRD__STS_OK
 *                         MRD__STS_OKNOVAL
 *
 * Developer notes:
 *
 */
static long longin__read( struct longinRecord* pli )
{

   /* Validate device private pointer */
   if( pli->dpvt == NULL)
   {
      pli->pact = MRD__K_ACTIVE;

      return( MRD__STS_OKNOVAL );
   }

   /* start-block: process read */
   {
   long        sts;
   ULONG       readback;
   rMRD__INST* pinst;


      /* Initialize local variants */
      pinst = (rMRD__INST*)pli->dpvt;

      /* Read register value */
      sts = mrd__read( pinst->pdata, pinst->dmask, &readback );
      if( sts == MRD__STS_OK )
      {
         pli->val = readback >> pinst->dsbn;
         pli->udf = 0;
      }
      else
      {
         sts      = MRD__STS_OKNOVAL;

         /* Set an alarm for the record */
         recGblSetSevr( pli, READ_ALARM, INVALID_ALARM );
      }

      /* Return completion status */
      return( sts );

   } /* end-block: process read */

} /* end-method: longin__read() */


/*
 * mbbi__init()
 *
 * Description:
 *    This method performs the initialization for a MBBI record.
 *
 * Input Parameters:
 *    pmbbi - Address of MBBI record.
 *
 * Output Parameters:
 *    None.
 *
 * Returns:
 *    Completion status:   MRD__STS_OK
 *                         MRD__STS_ERROR
 *
 * Developer notes:
 *    1) The mask (data width) is determined from the NOBT field.
 *
 */
static long mbbi__init( struct mbbiRecord* pmbbi )
{
long        sts = MRD__STS_OK;
ULONG       readback;
rMRD__INST* pinst;


   /* Initialize device private pointer */
   pmbbi->dpvt = NULL;

   /* Evaluate common comparisons */
   sts = mrd__compareCommon( pmbbi->inp.type, pmbbi->name, &pmbbi->inp.value.vmeio );
   if( sts != MRD__STS_OK )
   {
      pmbbi->pact = MRD__K_ACTIVE;

      return( sts );
   }

   /* Evaluate common input specification */
   pinst = mrd__inputCommon( 1, pmbbi->name, &pmbbi->inp.value.vmeio );
   if( pinst == NULL )
   {
      pmbbi->pact = MRD__K_ACTIVE;

      return( MRD__STS_ERROR );
   }

   /* Update record instance structure */
   pinst->dmask   = pmbbi->mask << pinst->dsbn;

   /* Update MBBI record structure */
   pmbbi->dpvt    = pinst;
   pmbbi->shft    = pinst->dsbn;
   pmbbi->mask    = pinst->dmask;

   /* Initialize record raw input value */
   sts = mrd__read( pinst->pdata, pinst->dmask, &readback );
   if( sts == MRD__STS_OK )
   {
      pmbbi->rval = readback;
   }
   else
   {
      sts         = MRD__STS_OKNOVAL;
   }

   /* Increment record instance count */
   ++rMRD__info.recInstCount;

   /* Return completion status */
   return( sts );

} /* end-method: mbbi__init() */


/*
 * mbbi__read()
 *
 * Description:
 *    This method performs the read for a MBBI record.
 *
 * Input Parameters:
 *    pmbbi    - Address of MBBI record.
 *
 * Output Parameters:
 *    None.
 *
 * Returns:
 *    Completion status:   MRD__STS_OK
 *                         MRD__STS_OKNOVAL
 *
 * Developer notes:
 *
 */
static long mbbi__read( struct mbbiRecord* pmbbi )
{

   /* Validate device private pointer */
   if( pmbbi->dpvt == NULL)
   {
      pmbbi->pact = MRD__K_ACTIVE;

      return( MRD__STS_OKNOVAL );
   }

   /* start-block: process read */
   {
   long        sts;
   ULONG       readback;
   rMRD__INST* pinst;


      /* Initialize local variants */
      pinst = (rMRD__INST*)pmbbi->dpvt;

      /* Initialize local variants */
      if( pmbbi->dpvt )
      {
         pinst     = (rMRD__INST*)pmbbi->dpvt;
      }
      else
      {
         pmbbi->pact = MRD__K_ACTIVE;

         return( MRD__STS_OKNOVAL );
      }

      /* Read register value */
      sts = mrd__read( pinst->pdata, pinst->dmask, &readback );
      if( sts == MRD__STS_OK )
      {
         pmbbi->rval = readback;
      }
      else
      {
         sts         = MRD__STS_OKNOVAL;

         /* Set an alarm for the record */
         recGblSetSevr( pmbbi, READ_ALARM, INVALID_ALARM );
      }

      /* Return completion status */
      return( sts );

   } /* end-block: process read */

} /* end-method: mbbi__read() */


/****************************************************************************
 * Define public interface methods
 ****************************************************************************/


/*
 * devAvmeMRDReport()
 *
 * Description:
 *    This method calls the underlying report method mrd__report(). Please see
 *    its description.
 *
 * Input Parameters:
 *    None.
 *
 * Output Parameters:
 *    None.
 *
 * Returns:
 *    Completion status:   mrd__report()
 *
 * Developer notes:
 *
 */
int devAvmeMRDReport( )
{
   return( mrd__report( 1 ) );
}

/*
 * devAvmeMRDConfig()
 *
 * Description:
 *    This method is called from the startup script to initialize
 *    the MRD-100. It should be called prior to any usage of the
 *    module.
 *
 * Input Parameters:
 *    base     - Base A32/D32 VMEbus address.
 *    vector   - Interrupt vector (0..255).
 *    level    - Interrupt request level (1..7)
 *
 * Output Parameters:
 *    None.
 *
 * Returns:
 *    Completion status:   MRD__STS_OK
 *                         MRD__STS_ERROR
 *
 * Developer notes:
 *
 */
int devAvmeMRDConfig( ULONG base, ULONG vector, ULONG level )
{
STATUS         sts;
ULONG          value;
static UCHAR   init = FALSE;


   /* Evaluate card initialization */
   if( init == FALSE )
   {
      init = TRUE;
   }
   else
   {
      epicsPrintf( "\ndevAvmeMRDConfig(): MRD-100 already initialized\n" );

      return( MRD__STS_OK );
   }

   /* Evaluate passed base address */
   if( base >= MRD__K_MAXADDR )
   {
      epicsPrintf( "devAvmeMRDConfig(): Invalid base address 0x%8.8X\n", (UINT)base );

      return( MRD__STS_ERROR );
   }

   /* Evaluate passed interrupt vector */
   if( (vector == 0) || (vector > MRD__K_IVEC) )
   {
      epicsPrintf( "devAvmeMRDConfig(): Invalid vector 0x%8.8X\n", (UINT)vector );

      return( MRD__STS_ERROR );
   }

   /* Evaluate passed interrupt level */
   if( (level < 1) || (level > 7) )
   {
      epicsPrintf( "devAvmeMRDConfig(): Invalid Interrupt level %d\n", (UINT)level );

      return( MRD__STS_ERROR );
   }

   /* Map base address into local space */
   sts = devRegisterAddress( "devAvmeMRD", atVMEA32, base, MRD__S_REGS, (volatile void**)&rMRD__info.base );
   if( sts )
   {
      memset( &rMRD__info, 0, MRD__S_INFO );
      epicsPrintf( "devAvmeMRDConfig(): Failure to map address 0x%8.8X\n", (UINT)base );

      return( MRD__STS_ERROR );
   }

   /* Probe (read) base address */
   sts = devReadProbe( sizeof(value), (char*)rMRD__info.base, (char*)&value );
   if( sts )
   {
      memset( &rMRD__info, 0, MRD__S_INFO );
      epicsPrintf( "devAvmeMRDConfig(): Failure to probe address 0x%8.8X\n", (UINT)base );

      return( MRD__STS_ERROR );
   }

   /* Initialize MRD INFO structure */
   rMRD__info.card = 0;
   rMRD__info.lock = epicsMutexMustCreate();

   /* Initialize EPICS IO scan */
   scanIoInit( &rMRD__info.ioscanpvt );

   /* Connect with interrupt mechanism */
   sts = devConnectInterrupt( intVME, vector, (void (*)())mrd__isr, &rMRD__info );
   if( sts )
   {
      memset( &rMRD__info, 0, MRD__S_INFO );
      epicsPrintf( "devAvmeMRDConfig(): Failure to connect with interrupt\n" );

      return( MRD__STS_ERROR );
   }

   /* Initialize MRD */
   rMRD__info.base->rREGS.IVEC  = vector;
   rMRD__info.base->rREGS.IDC   = MRD__K_IDC;
   rMRD__info.base->rREGS.ICR   = MRD__K_ICR;

   /* Enable VME interrupt (IRQ) level */
   sts = devEnableInterruptLevel( intVME, level );
   if( sts )
   {
      epicsPrintf( "devAvmeMRDConfig(): Failure to enable IRQ%1.1d\n", (UINT)level );

      return( MRD__STS_ERROR );
   }

   /* Add to the list of routines called when IOC rebooted */
#if ( MRD__GT_EPICSBASE(3,14,6) )
   sts = epicsAtExit( mrd__reboot, NULL );
#else

   #ifdef vxWorks
      sts = rebootHookAdd( (FUNCPTR)mrd__reboot );
   #endif

#endif

   if( sts )
   {
      epicsPrintf( "devAvmeMRDConfig(): Failure to add reboot hook\n" );

      return( MRD__STS_ERROR );
   }

   /* Return completion status */
   return( MRD__STS_OK );

} /* end-method: devAvmeMRDConfig() */

