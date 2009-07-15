/*

                          Argonne National Laboratory
                            APS Operations Division
                     Beamline Controls and Data Acquisition

                   EPICS Brick 063 Scaler Asyn Device Support



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
    This module provides device support support for the BC063 scaler record
    device support. To initialize device support, call the method below with
    the following calling sequence.

        devAsynScaler063Init(port,base,inst,rate)

        Where:
            port - Scaler ASYN port name (i.e. "SCALER1")
            base - Base address of the BC063 module
            inst - Module instance
            rate - Poll frequency (i.e. 5 is 5Hz)


 Developer notes:
    1. In method scanScaler(), the line if( pport->psr && pport->psr->cnt ) scalerRead(pport)
       was added to eliminate a 'burp' that ocurred when the Scaler record was switching to
       Oneshot mode.

 Source control info:
    Modified by:    dkline
                    2008/02/12 12:19:33
                    1.5

 =============================================================================
 History:
 Author: David M. Kline
 -----------------------------------------------------------------------------
 2007-Oct-09  DMK  Derived from the GDIO Scaler devices support.
 2007-Oct-11  DMK  Initial implementation complete.
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
#include <string.h>
#include <fcntl.h>
#include <sys/io.h>
#include <sys/ioctl.h>


/* EPICS system related include files */
#include <iocsh.h>
#include <epicsStdio.h>
#include <cantProceed.h>
#include <epicsString.h>
#include <epicsExport.h>
#include <epicsThread.h>

#include <cadef.h>
#include <recGbl.h>
#include <devSup.h>
#include <dbEvent.h>
#include <callback.h>


/* EPICS synApps/Asyn related include files */
#include <devScaler.h>
#include <scalerRecord.h>
#include <asynEpicsUtils.h>


/* Define support constants */
#define DATA0       (0)
#define DATA1       (1)
#define DATA2       (2)
#define DATA3       (3)

#define ID          (63)
#define SCALERS     (16)
#define INSTMAX     (4)
#define DONE        (0x1)
#define ARMED       (0x2)
#define GROUP       (0x4)


/* Define IO port access macros */
#define get063(b,o) inb((b)+(o))
#define put063(b,o,v) outb((v),(b)+(o))


/* Forward references for device support struct */
static long scal063_report(int level);
static long scal063_hdw_init(int after);
static long scal063_rec_done(scalerRecord *psr);
static long scal063_rec_reset(scalerRecord *psr);
static long scal063_rec_arm(scalerRecord *psr,int val);
static long scal063_rec_read(scalerRecord *psr,long* val);
static long scal063_rec_init(scalerRecord* psr,CALLBACK *pcallback);
static long scal063_rec_preset(scalerRecord *psr,int signal,long val);

static SCALERDSET devAsynScaler063 =
{
    8,
    (long int (*)(void*))scal063_report,
    (long int (*)(void*))scal063_hdw_init,
    (long int (*)(void*))scal063_rec_init,
    NULL,
    (long int (*)(void*))scal063_rec_reset,
    (long int (*)(void*))scal063_rec_read,
    (long int (*)(void*))scal063_rec_preset,
    (long int (*)(void*))scal063_rec_arm,
    (long int (*)(void*))scal063_rec_done,
};
epicsExportAddress(dset,devAsynScaler063);


/* Forward struct declarations */
typedef struct Port Port;
typedef struct Scaler Scaler;

/* Declare Scaler structure */
struct Scaler
{
    epicsInt32  value;
    epicsInt32  preset;
};

/* Declare Port structure */
struct Port
{
    char*         name;
    int           base;
    int           inst;
    int           rate;
    double        freq;
    unsigned char modid;
    epicsMutexId  syncLock;

    Scaler*       scaler;
    scalerRecord* psr;

    int           poll;
    int           done;
    int           scancnt;
    CALLBACK*     pcallback;
};


/* Public interface forward references */
static int devAsynScaler063Init(const char* port,int base,int inst,int rate);


/* Private methods forward references */
static void scanScaler(Port* pport);
static void scalerRead(Port* pport);
static void scalerPreset(Port* pport,int counter,epicsInt32 preset);

/* Define macros */
#define scalerArm(b)      {put063((b),4,0);put063((b),5,1);}
#define scalerDisarm(b)   {put063((b),4,0);put063((b),5,2);}
#define scalerReset(b)    {put063((b),4,0);put063((b),5,4);}
#define scalerGroup(b)    {put063((b),4,0);put063((b),5,5);}
#define scalerUngroup(b)  {put063((b),4,0);put063((b),5,6);}
#define scalerStatus(b,s) {put063((b),4,0);put063((b),5,8);(s)=get063((b),6);}

#define readID(b)         (get063((b),7))

/* Define local variants */
static Port* pports[] = {NULL,NULL,NULL,NULL};


/****************************************************************************
 * Define public interface methods
 ****************************************************************************/
static int devAsynScaler063Init(const char* port,int base,int inst,int rate)
{
    int len;
    uint prio;
    Port* pport;
    unsigned char id;

    if( inst > INSTMAX )
    {
        printf("devAsynScaler063Init::init %s: invalid instance count %d\n",port,inst);
        return( -1 );
    }

    if( pports[inst] )
    {
        printf("devAsynScaler063Init::init %s: instance %d already defined\n",port,inst);
        return( -1 );
    }

    if( base )
    {
        id = readID(base);
        if( id == ID )
        {
            printf("devAsynScaler063Init::init %s: found (%3.3d)\n",port,id);
            scalerReset(base);
        }
        else
        {
            printf("devAsynScaler063Init::init %s: wrong id read\n",port);
            return( -1 );
        }
    }
    else
    {
        printf("devAsynScaler063Init::init %s: base must be defined\n",port);
        return( -1 );
    }

    len = strlen(port)+1+sizeof(Port)+(sizeof(Scaler)*SCALERS);
    pport = (Port*)callocMustSucceed(len,sizeof(char),"devAsynScaler063Init");

    pport->scaler = (Scaler*)(pport+1);
    pport->name   = (char*)(pport->scaler+SCALERS);

    pport->base   = base;
    pport->inst   = inst;
    pport->rate   = rate;
    pport->modid  = id;
    strcpy(pport->name,port);
    pport->syncLock = epicsMutexMustCreate();

    pports[inst] = pport;
    epicsThreadLowestPriorityLevelAbove(epicsThreadPriorityHigh,&prio);
    epicsThreadCreate("scanScaler063",prio,epicsThreadGetStackSize(epicsThreadStackSmall),(EPICSTHREADFUNC)scanScaler,pport);

    return( 0 );
}


static void scanScaler(Port* pport)
{
    unsigned char status;
    double freq = (1.0 / pport->rate);

    while(0==0)
    {
        epicsThreadSleep(freq);
        if( pport->poll == FALSE ) continue;
        if( pport->psr && pport->psr->cnt ) scalerRead(pport);

        epicsMutexMustLock(pport->syncLock);
        scalerStatus(pport->base,status);
        epicsMutexUnlock(pport->syncLock);

        if( status & DONE ) ++pport->scancnt; else continue;

//printf("scanScaler() - \"%s\" - group done - (%d)\n",pport->name,pport->scancnt);

        scalerRead(pport);
        pport->done = 1;
        pport->poll = FALSE;
        callbackRequest(pport->pcallback);
    }
   
}


/****************************************************************************
 * Define device support private interface
 ****************************************************************************/
static void scalerRead(Port* pport)
{
unsigned char b0,b1,b2,b3;

    epicsMutexMustLock(pport->syncLock);
    for( int i=0; i<SCALERS; ++i )
    {
        put063(pport->base,4,(i+1));
        put063(pport->base,5,7);

        b0 = get063(pport->base,DATA0);
        b1 = get063(pport->base,DATA1);
        b2 = get063(pport->base,DATA2);
        b3 = get063(pport->base,DATA3);

        pport->scaler[i].value = (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
    }
    epicsMutexUnlock(pport->syncLock);
}


static void scalerPreset(Port* pport,int counter,epicsInt32 preset)
{
unsigned char b0,b1,b2,b3;

    b0 = (preset & 0xFF);
    b1 = (preset & 0xFF00) >> 8;
    b2 = (preset & 0xFF0000) >> 16;
    b3 = (preset & 0xFF000000) >> 24;

    epicsMutexMustLock(pport->syncLock);
    put063(pport->base,DATA0,b0);
    put063(pport->base,DATA1,b1);
    put063(pport->base,DATA2,b2);
    put063(pport->base,DATA3,b3);

    put063(pport->base,4,counter);
    put063(pport->base,5,3);
    epicsMutexUnlock(pport->syncLock);
}

static long scal063_report(int level)
{
    int i,j;
    unsigned char status;

//printf("scal063_report(%d)\n",level);

    for( i=0; pports[i]; ++i )
    {
        epicsMutexMustLock(pports[i]->syncLock);
        scalerStatus(pports[i]->base,status);
        epicsMutexUnlock(pports[i]->syncLock);

        printf("%s-%d is a %3.3d 16ch scaler, %fMHz clock freq, poll rate of %d/sec, status=0x%0X\n",pports[i]->name,pports[i]->inst,pports[i]->modid,(pports[i]->freq/1000000.0),pports[i]->rate,status);
        for( j=0; j<SCALERS; ++j ) printf("    Scaler %2.2d preset %d, value %d\n",(j+1),pports[i]->scaler[j].preset,pports[i]->scaler[j].value);
    }

    return( 0 );
}


static long scal063_hdw_init(int after)
{
    int i;

//printf("scal063_hdw_init(%d)\n",after);

    for( i=0; pports[i]; ++i )
    {
        pports[i]->done = 0;
        pports[i]->poll = FALSE;

        epicsMutexMustLock(pports[i]->syncLock);
        scalerReset(pports[i]->base);
        epicsMutexUnlock(pports[i]->syncLock);
    }

    return( 0 );
}


static long scal063_rec_init(scalerRecord* psr,CALLBACK *pcallback)
{
    int addr;
    Port* pport;
    asynStatus sts;
    asynUser* pasynUser;
    char *port,*userParam;

//printf("scal063_rec_init(psr)\n");

    switch( psr->out.type )
    {
    case INST_IO:
        pasynUser = pasynManager->createAsynUser(0,0);
        sts = pasynEpicsUtils->parseLink(pasynUser,&psr->out,&port,&addr,&userParam);
        pport = pports[addr];
        break;
    default:
        recGblRecordError(S_dev_badBus,(void*)psr,"devAsynScaler: scal063_rec_init - bad bus type");
        return(S_dev_badBus);
    }

    psr->nch = SCALERS;
    psr->dpvt = pport;

    pport->pcallback = pcallback;
    pport->freq = psr->freq;
    pport->psr = psr;

    return( 0 );
}


static long scal063_rec_reset(scalerRecord* psr)
{
    Port* pport = (Port*)psr->dpvt;

//printf("scal063_rec_reset(%s)\n",pport->name);

    pport->done = 0;
    pport->poll = FALSE;
    for( int i=0; i<SCALERS; ++i ) pport->scaler[i].preset=0;

    epicsMutexMustLock(pport->syncLock);
    scalerReset(pport->base);
    epicsMutexUnlock(pport->syncLock);

    return( 0 );
}


static long scal063_rec_read(scalerRecord* psr,long* val)
{
    int i;
    Port* pport = (Port*)psr->dpvt;

//printf("scal063_rec_read(%s)\n",pport->name);

    for( i=0; i<SCALERS; ++i ) val[i] = (long)pport->scaler[i].preset + (long)pport->scaler[i].value;
    return( 0 );
}


static long scal063_rec_preset(scalerRecord* psr,int signal,long val)
{
    epicsInt32 preset;
    Port* pport = (Port*)psr->dpvt;

//printf("scal063_rec_preset(%s,%d,%d)\n",pport->name,signal,(int)val);

    preset = (epicsInt32)val * -1;
    scalerPreset(pport,(signal+1),preset);
    pport->scaler[signal].preset = (epicsInt32)val;

    return( 0 );
}


static long scal063_rec_arm(scalerRecord* psr,int val)
{
    Port* pport = (Port*)psr->dpvt;

//printf("scal063_rec_arm(%s,%d)\n",pport->name,val);

    epicsMutexMustLock(pport->syncLock);
    put063(pport->base,4,0);
    if( val )
    {
        put063(pport->base,5,5);
        put063(pport->base,5,1);

        pport->done = 0;
        pport->poll = TRUE;
    }
    else
    {
        pport->poll = FALSE;
        pport->done = 1;

        put063(pport->base,5,2);
        put063(pport->base,5,6);
    }
    epicsMutexUnlock(pport->syncLock);

    return( 0 );
}


static long scal063_rec_done(scalerRecord* psr)
{
    Port* pport = (Port*)psr->dpvt;

//printf("scal063_rec_done(%s) - done %d - cnt %d - cont %d\n",pport->name,pport->done,pport->psr->cnt,pport->psr->cont);

    if( pport->done )
    {
        pport->done = 0;
        pport->poll = FALSE;
        scalerDisarm(pport->base);

        return( 1 );
    }

    return( 0 );
}


/****************************************************************************
 * Register public methods
 ****************************************************************************/
/* Initialization method definitions */
static const iocshArg devAsynScaler063InitArg0 = {"port",iocshArgString};
static const iocshArg devAsynScaler063InitArg1 = {"base",iocshArgInt};
static const iocshArg devAsynScaler063InitArg2 = {"inst",iocshArgInt};
static const iocshArg devAsynScaler063InitArg3 = {"rate",iocshArgInt};
static const iocshArg* devAsynScaler063InitArgs[]= {&devAsynScaler063InitArg0,&devAsynScaler063InitArg1,&devAsynScaler063InitArg2,&devAsynScaler063InitArg3};
static const iocshFuncDef devAsynScaler063InitFuncDef = {"devAsynScaler063Init",4,devAsynScaler063InitArgs};
static void devAsynScaler063InitCallFunc(const iocshArgBuf* args)
{
    devAsynScaler063Init(args[0].sval,args[1].ival,args[2].ival,args[3].ival);
}

/* Registration method */
static void devAsynScaler063Register(void)
{
    static int firstTime = 1;

    if( firstTime )
    {
        firstTime = 0;
        iocshRegister(&devAsynScaler063InitFuncDef,devAsynScaler063InitCallFunc);
    }
}
epicsExportRegistrar( devAsynScaler063Register );
