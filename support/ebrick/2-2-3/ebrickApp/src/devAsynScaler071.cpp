/*

                          Argonne National Laboratory
                            APS Operations Division
                     Beamline Controls and Data Acquisition

                   EPICS Brick 071 Scaler Asyn Device Support



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
    This module provides device support for the BC071 scaler record. To
    initialize device support, call the method below with the following
    calling sequence.

        devAsynScaler071Init(port,rate)

        Where:
            port - Scaler ASYN port name (i.e. "SCALER1")
            rate - Poll frequency (i.e. 5 is 5Hz)


 Developer notes:
    1. In method intrScaler(), the line if( pport->psr->cnt ) scalerRead(pport)
       was added to eliminate a 'burp' that ocurred when the Scaler record was
       switching to Oneshot mode.

    2. Command Protocol format:

       +-------------------------------------------------------------+
       |63               32|15                        |9    6|5     0|
       +-------------------------------------------------------------+
       | Preset (optional) |C                        |  FNC  |  CTR  |
       |                   |L                        |       |       |
       |                   |K                        |       |       |
       +-------------------------------------------------------------+


 Source control info:
    Modified by:    dkline
                    2009/02/16 21:00:33
                    1.4

 =============================================================================
 History:
 Author: David M. Kline
 -----------------------------------------------------------------------------
 2008-Sep-11  DMK  Derived from the BC-059 Scaler devices support.
 2008-Sep-23  DMK  Initial version complete.
 2008-Oct-03  DMK  Employ parallel interface for command/scaler data access.
 -----------------------------------------------------------------------------

*/


/* EPICS base version-specific definitions (must be performed first) */
#include <epicsVersion.h>
#define LT_EPICSBASE(v,r,l) (EPICS_VERSION<(v)||(EPICS_VERSION==(v)&&(EPICS_REVISION<(r)||(EPICS_REVISION==(r)&&EPICS_MODIFICATION<(l)))))


/* Evaluate EPICS base */
#if LT_EPICSBASE(3,14,6)
    #error "EPICS base must be 3.14.6 or greater"
#endif


/* System related include files */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>


/* EPICS system related include files */
#include <iocsh.h>
#include <devLib.h>
#include <epicsStdio.h>
#include <cantProceed.h>
#include <epicsString.h>
#include <epicsExport.h>
#include <epicsThread.h>
#include <epicsInterrupt.h>
#include <epicsMessageQueue.h>

#include <cadef.h>
#include <recGbl.h>
#include <devSup.h>
#include <dbEvent.h>
#include <callback.h>

#include <asynUInt32Digital.h>


/* EPICS synApps/Asyn related include files */
#include <devScaler.h>
#include <scalerRecord.h>
#include <asynEpicsUtils.h>


/* Define SOPC system symbolic constants */
#define AVALON_BASE     (0x30000000)

#define STS_PORT_VEC    (0xC1)
#define STS_PORT_MSK    (0x0001)
#define STS_PORT_OFF    (0x40)
#define STS_PORT_ADR    (AVALON_BASE+STS_PORT_OFF)
#define STS_PORT_PTR    ((epicsUInt16*)(STS_PORT_ADR))
#define STS_PORT_IRM    ((epicsUInt32*)(STS_PORT_ADR+4))

#define DLO_PORT_OFF    (0x50)
#define DLO_PORT_ADR    (AVALON_BASE+DLO_PORT_OFF)
#define DLO_PORT_PTR    ((epicsUInt16*)(DLO_PORT_ADR))

#define DHI_PORT_OFF    (0x60)
#define DHI_PORT_ADR    (AVALON_BASE+DHI_PORT_OFF)
#define DHI_PORT_PTR    ((epicsUInt16*)(DHI_PORT_ADR))

#define CMD_PORT_OFF    (0x70)
#define CMD_PORT_ADR    (AVALON_BASE+CMD_PORT_OFF)
#define CMD_PORT_PTR    ((epicsUInt16*)(CMD_PORT_ADR))

#define PLO_PORT_OFF    (0x80)
#define PLO_PORT_ADR    (AVALON_BASE+PLO_PORT_OFF)
#define PLO_PORT_PTR    ((epicsUInt16*)(PLO_PORT_ADR))

#define PHI_PORT_OFF    (0x90)
#define PHI_PORT_ADR    (AVALON_BASE+PHI_PORT_OFF)
#define PHI_PORT_PTR    ((epicsUInt16*)(PHI_PORT_ADR))


/* Define support constants */
#define ID          (71)
#define RATE        (50)
#define SCALERS     (32)


/* Define serial link symbolic constants */
#define MAJVER      (2)
#define MINVER      (1)
#define VERSION     ((MAJVER<<4)|MINVER)

#define DONE        (0x1)
#define ARMED       (0x2)


/* Define IO port access macros */
#define get071(b,o)     (*((epicsUInt16*)((b)+(o))))
#define put071(b,o,d)   (*((epicsUInt16*)((b)+(o)))=(d))


/* Forward references for device support struct */
static long scal071_report(int level);
static long scal071_hdw_init(int after);
static long scal071_rec_done(scalerRecord *psr);
static long scal071_rec_reset(scalerRecord *psr);
static long scal071_rec_arm(scalerRecord *psr,int val);
static long scal071_rec_read(scalerRecord *psr,long* val);
static long scal071_rec_init(scalerRecord* psr,CALLBACK *pcallback);
static long scal071_rec_preset(scalerRecord *psr,int signal,long val);

static SCALERDSET devAsynScaler071 =
{
    8,
    (long int (*)(void*))scal071_report,
    (long int (*)(void*))scal071_hdw_init,
    (long int (*)(void*))scal071_rec_init,
    NULL,
    (long int (*)(void*))scal071_rec_reset,
    (long int (*)(void*))scal071_rec_read,
    (long int (*)(void*))scal071_rec_preset,
    (long int (*)(void*))scal071_rec_arm,
    (long int (*)(void*))scal071_rec_done,
};
epicsExportAddress(dset,devAsynScaler071);


/* Forward struct declarations */
typedef struct Port Port;
typedef struct Scaler Scaler;

/* Declare Scaler structure */
struct Scaler
{
    epicsInt32  value;
    epicsInt32  preval;
    epicsInt32  preset;
};

/* Declare Port structure */
struct Port
{
    char*         name;
    int           rate;
    double        freq;
    unsigned char modid;
    epicsMutexId  syncLock;

    int ints;
    int sent;
    int dropped;
    int received;
    epicsMessageQueueId msgQ;

    epicsUInt32* irmptr;

    Scaler* scaler;
    scalerRecord* psr;

    int           poll;
    int           done;
    int           scancnt;
    CALLBACK*     pcallback;
};


/* Public interface forward references */
static int devAsynScaler071Init(const char* myport,int rate);


/* Private methods forward references */
static void intrScaler(Port* pport);
static void scalerRead(Port* pport);
static void sendCommand(Port* pport,int ctr,int fnc,epicsInt32 preset);


/* Define macros */
#define readDataDL(b)       (get071((b),DLO_PORT_OFF))
#define readDataDH(b)       (get071((b),DHI_PORT_OFF))
#define readDataSD(b)       (get071((b),STS_PORT_OFF))
#define writeDataCM(b,d)    (put071((b),CMD_PORT_OFF,(d)))
#define writeDataPL(b,d)    (put071((b),PLO_PORT_OFF,(d)))
#define writeDataPH(b,d)    (put071((b),PHI_PORT_OFF,(d)))
#define scalerNull(b)       {sendCommand((b),0,0,0);}
#define scalerArm(b)        {sendCommand((b),0,1,0);}
#define scalerDisarm(b)     {sendCommand((b),0,2,0);}
#define scalerPreset(b,c,p) {sendCommand((b),(c),3,(p));}
#define scalerReset(b)      {sendCommand((b),0,4,0);}
#define scalerData(b,c)     {sendCommand((b),(c),7,0);}


/* Define local variants */
static Port* pports = NULL;


/* External references */
extern "C" void printk(char *fmt, ...);


/****************************************************************************
 * Define interrupt handling methods
 ****************************************************************************/
static void interrupt_handler(void* parg)
{
    int data;
    Port* pport = (Port*)parg;

    ++pport->ints;

    *pport->irmptr = 0x0;
    data = readDataSD(AVALON_BASE);

//printk("\ninterrupt_handler()\n");

    if( epicsMessageQueueTrySend(pport->msgQ,&data,sizeof(int))==0 )
        pport->sent++;
    else
        pport->dropped++;

    *pport->irmptr = STS_PORT_MSK<<16;

}


static void intrScaler(Port* pport)
{
    int sts,msg;
    epicsUInt16 status;
    double freq = 1.0 / pport->rate;

    // Wait for record to initialize
    while( pport->psr == NULL ) epicsThreadSleep(freq);

    // Process Scaler
    while(0==0)
    {
        sts = epicsMessageQueueReceiveWithTimeout(pport->msgQ,&msg,sizeof(int),freq);
        if( pport->poll==FALSE ) continue;
        if( sts==sizeof(int) ) {++pport->received; status=(epicsUInt16)msg;}
        if( sts<0 ) if( pport->psr->cnt ) {scalerRead(pport);status=readDataSD(AVALON_BASE);} else continue;

        if( status & DONE ) ++pport->scancnt; else continue;

        scalerRead(pport);
        pport->done = 1;
        pport->poll = FALSE;
        callbackRequest(pport->pcallback);

//printf("3) intrScaler(%s) - done scancnt %d status 0x%X\n",pport->name,pport->scancnt,status);
    }
   
}


/****************************************************************************
 * Define public interface methods
 ****************************************************************************/
static int devAsynScaler071Init(const char* myport,int rate)
{
    int len;
    Port* pport;

    if( pports )
    {
        printf("devAsynScaler071Init::init %s: max. number of instances exceeded\n",myport);
        return( -1 );
    }

    len = strlen(myport)+1+sizeof(Port)+(sizeof(Scaler)*SCALERS);
    pport = (Port*)callocMustSucceed(len,sizeof(char),"devAsynScaler071Init");

    pport->scaler   = (Scaler*)(pport+1);
    pport->name     = (char*)(pport->scaler+SCALERS);

    strcpy(pport->name,myport);
    pport->modid    = ID;
    pport->rate     = (rate)?rate:RATE;
    pport->syncLock = epicsMutexMustCreate();

    pport->irmptr   = STS_PORT_IRM;

    pport->sent     = 0;
    pport->dropped  = 0;
    pport->received = 0;
    pport->msgQ     = epicsMessageQueueCreate(100,sizeof(int));

    scalerReset(pport);
    epicsThreadCreate("intrScaler071",epicsThreadPriorityMedium,epicsThreadGetStackSize(epicsThreadStackSmall),(EPICSTHREADFUNC)intrScaler,pport);

    *pport->irmptr  = STS_PORT_MSK<<16;
    devConnectInterruptVME(STS_PORT_VEC,interrupt_handler,pport);

    pports = pport;

    return( 0 );
}


/****************************************************************************
 * Define device support private interface
 ****************************************************************************/
static void scalerRead(Port* pport)
{
    epicsUInt16 b0,b1;

    epicsMutexMustLock(pport->syncLock);
    for( int i=0; i<SCALERS; ++i )
    {
        scalerData(pport,(i+1))
        b0=readDataDL(AVALON_BASE);
        b1=readDataDH(AVALON_BASE);

        pport->scaler[i].preval = pport->scaler[i].value;
        pport->scaler[i].value  = (b1 << 16) | b0;
    }
    epicsMutexUnlock(pport->syncLock);
}


static void sendCommand(Port* pport,int ctr,int fnc,epicsInt32 preset)
{
    epicsUInt16 data;
    static epicsUInt16 mask=0xFFFF;

    if( preset )
    {
        data = preset & mask;
        writeDataPL(AVALON_BASE,data);

        data = (preset>>16) & mask;
        writeDataPH(AVALON_BASE,data);
    }

    data = (fnc<<6) | ctr;
    writeDataCM(AVALON_BASE,data);
    writeDataCM(AVALON_BASE,(0x8000|data));
    writeDataCM(AVALON_BASE,data);
}


static long scal071_report(int level)
{
    int j;
    epicsUInt16 status;

//printf("scal071_report(%d)\n",level);

    epicsMutexMustLock(pports->syncLock);
    status=readDataSD(AVALON_BASE);
    epicsMutexUnlock(pports->syncLock);

    printf("%s-%3.3d %dch scaler,%.3fMHz,poll %d/sec\n",pports->name,pports->modid,SCALERS,(pports->freq/1000000.0),pports->rate);
    printf("    SerialLink version %1d.%1d\n",MAJVER,MINVER);
    printf("    SerialLink status %s,%s (0x%0X)\n",(status&DONE)?"done":"!done",(status&ARMED)?"armed":"!armed",status);
    printf("    pport status flags %s,%s,%s\n",(pports->poll)?"polling":"!polling",(pports->done)?"done":"!done",(pports->psr->cnt)?"cnt":"!cnt");
    printf("    pport scancnt %d ints %d sent %d dropped %d received %d\n",pports->scancnt,pports->ints,pports->sent,pports->dropped,pports->received);
    for( j=0; j<SCALERS; ++j ) printf("    Scaler %2.2d preset %d, value %d\n",(j+1),pports->scaler[j].preset,pports->scaler[j].value);

    return( 0 );
}


static long scal071_hdw_init(int after)
{

//printf("scal071_hdw_init(%d)\n",after);

    if( after==0 ) return( 0 );

    pports->done = 0;
    pports->poll = FALSE;

    epicsMutexMustLock(pports->syncLock);
    scalerReset(pports);
    epicsMutexUnlock(pports->syncLock);

    return( 0 );
}


static long scal071_rec_init(scalerRecord* psr,CALLBACK *pcallback)
{
    int addr;
    Port* pport;
    asynStatus sts;
    asynUser* pasynUser;
    char *port,*userParam;

//printf("scal071_rec_init(psr)\n");

    switch( psr->out.type )
    {
    case INST_IO:
        pasynUser = pasynManager->createAsynUser(0,0);
        sts = pasynEpicsUtils->parseLink(pasynUser,&psr->out,&port,&addr,&userParam);
        pport = pports;
        break;
    default:
        recGblRecordError(S_dev_badBus,(void*)psr,"devAsynScaler: scal071_rec_init - bad bus type");
        return(S_dev_badBus);
    }

    psr->nch = SCALERS;
    psr->dpvt = pport;

    pport->pcallback = pcallback;
    pport->freq = psr->freq;
    pport->psr = psr;

    return( 0 );
}


static long scal071_rec_reset(scalerRecord* psr)
{
    Port* pport = (Port*)psr->dpvt;

//printf("scal071_rec_reset(%s)\n",pport->name);

    pport->done = 0;
    pport->poll = FALSE;
    for( int i=0; i<SCALERS; ++i ) pport->scaler[i].preset=0;

    epicsMutexMustLock(pport->syncLock);
    scalerReset(pport);
    epicsMutexUnlock(pport->syncLock);

    return( 0 );
}


static long scal071_rec_read(scalerRecord* psr,long* val)
{
    int i;
    Port* pport = (Port*)psr->dpvt;

//printf("scal071_rec_read(%s)\n",pport->name);

    for( i=0; i<SCALERS; ++i ) val[i] = (long)pport->scaler[i].preset + (long)pport->scaler[i].value;
    return( 0 );
}


static long scal071_rec_preset(scalerRecord* psr,int signal,long val)
{
    epicsInt32 preset;
    Port* pport = (Port*)psr->dpvt;

//printf("scal071_rec_preset(%s,%d,%d)\n",pport->name,signal,(int)val);

    preset = (epicsInt32)val * -1;

    epicsMutexMustLock(pport->syncLock);
    scalerPreset(pport,(signal+1),preset);
    epicsMutexUnlock(pport->syncLock);

    pport->scaler[signal].preset = (epicsInt32)val;

    return( 0 );
}


static long scal071_rec_arm(scalerRecord* psr,int val)
{
    Port* pport = (Port*)psr->dpvt;

//printf("scal071_rec_arm(%s,%d)\n",pport->name,val);

    epicsMutexMustLock(pport->syncLock);
    if( val )
    {
        scalerArm(pport);

        pport->done = 0;
        pport->poll = TRUE;
    }
    else
    {
        pport->poll = FALSE;
        pport->done = 1;

        scalerDisarm(pport);
    }
    epicsMutexUnlock(pport->syncLock);

    return( 0 );
}


static long scal071_rec_done(scalerRecord* psr)
{
    Port* pport = (Port*)psr->dpvt;

//printf("scal071_rec_done(%s) - done %d - cnt %d - cont %d\n",pport->name,pport->done,pport->psr->cnt,pport->psr->cont);

    if( pport->done )
    {
        pport->done = 0;
        pport->poll = FALSE;

        epicsMutexMustLock(pports->syncLock);
        scalerDisarm(pport);
        epicsMutexUnlock(pports->syncLock);

        return( 1 );
    }

    return( 0 );
}


/****************************************************************************
 * Register public methods
 ****************************************************************************/
/* Initialization method definitions */
static const iocshArg initArg0 = {"port",iocshArgString};
static const iocshArg initArg1 = {"rate",iocshArgInt};
static const iocshArg* initArgs[]= {&initArg0,&initArg1};
static const iocshFuncDef initFuncDef = {"devAsynScaler071Init",2,initArgs};
static void initCallFunc(const iocshArgBuf* args)
{
    devAsynScaler071Init(args[0].sval,args[2].ival);
}

/* Registration method */
static void devAsynScaler071Register(void)
{
    static int firstTime = 1;

    if( firstTime )
    {
        firstTime = 0;
        iocshRegister(&initFuncDef,initCallFunc);
    }
}
epicsExportRegistrar( devAsynScaler071Register );
