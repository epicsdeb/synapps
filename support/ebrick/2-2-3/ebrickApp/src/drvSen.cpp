/*

                          Argonne National Laboratory
                            APS Operations Division
                     Beamline Controls and Data Acquisition

                        EPICS Brick Sensory Port Driver



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
    This module provides support for a multiple device port driver. To
    initialize the driver, the method drvSenInit() is called from the
    startup script with the following calling sequence.

        drvSenInit(port,type,inst,addr,chns,rate)

        Where:
            port - Port driver name (i.e. "SEN")
            type - Board type (see boardtypes.h)
            inst - Board instance
            addr - Board address
            chns - Register count
            rate - Interrupt rate (in Hz)
                   =0 - Default 5Hz
                   >0 - (in Hz)
                   <0 - Disabled

    The method dbior can be called from the IOC shell to display the current
    status of the driver.


 Developer notes:
    1. User manual ICMS document: APS_1251088


 Source control info:
    Modified by:    dkline
                    2008/09/05 17:48:06
                    1.14

 =============================================================================
 History:
 Author: David M. Kline
 -----------------------------------------------------------------------------
 2008-Feb-08  DMK  Revamped support,removed unused support,added Poseidon.
 2008-Apr-08  DMK  Increased/tuned senReset() timeout value for Athena.
 2008-Sep-04  DMK  Corrected problem when selecting Thermister or CurrentLoop.
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
#include <sys/io.h>
#include <unistd.h>


/* EPICS system related include files */
#include <iocsh.h>
#include <epicsStdio.h>
#include <cantProceed.h>
#include <epicsString.h>
#include <epicsExport.h>
#include <epicsThread.h>


/* EPICS synApps/Asyn related include files */
#include <asynDriver.h>
#include <asynDrvUser.h>
#include <asynFloat64.h>
#include <asynUInt32Digital.h>


/* Board definition related include files */
#include "boardtypes.h"


/* Define symbolic constants */
#define TRUE        (1)
#define FALSE       (0)


/* Forward struct declarations */
typedef struct Port Port;
typedef struct Channel Channel;
typedef struct Scaling Scaling;
typedef enum {senNone,senInput,senType,senCode} Reason;


/* Declare Scaling info structure */
struct Scaling
{
    int code;
    epicsFloat64 value;
};

/* Declare channel info structure */
struct Channel
{
    int conn;
    int type;
    int code;
    int number;
    Port* pport;
    epicsFloat64 scale;
    epicsFloat64 value;
};


/* Declare DSC port structure */
struct Port
{
    char*         name;
    int           type;
    int           inst;
    int           base;
    int           rate;
    int           conn;
    int           model;
    float         version;
    epicsMutexId  syncLock;

    /* Asyn info */
    asynInterface asynCommon;
    asynInterface asynDrvUser;
    asynInterface asynUInt32;
    asynInterface asynFloat64;
    void*         asynUInt32CallbackPvt;
    void*         asynFloat64CallbackPvt;

    /* Channel info */
    int           cnt;
    Channel*      chn;
};


/* Public interface forward references */
static int drvSenInit(const char* port,int type,int inst,int addr,int chns,int rate);


/* Forward references for asynCommon methods */
static void report(void* ppvt,FILE* fp,int details);
static asynStatus connect(void* ppvt,asynUser* pasynUser);
static asynStatus disconnect(void* ppvt,asynUser* pasynUser);
static asynCommon common = {report,connect,disconnect};


/* Forward references for asynDrvUser methods */
static asynStatus create(void* ppvt,asynUser* pasynUser,const char* drvInfo, const char** pptypeName,size_t* psize);
static asynStatus destroy(void* ppvt,asynUser* pasynUser);
static asynStatus gettype(void* ppvt,asynUser* pasynUser,const char** pptypeName,size_t* psize);
static asynDrvUser drvuser = {create,gettype,destroy};


/* Forward references for asynFloat64 methods */
static asynStatus readFloat64(void* ppvt,asynUser* pasynUser,epicsFloat64* value);
static asynStatus writeFloat64(void* ppvt,asynUser* pasynUser,epicsFloat64 value);


/* Forward references for asynUInt32Digital methods */
static asynStatus readUInt32(void* ppvt,asynUser* pasynUser,epicsUInt32* value,epicsUInt32 mask);
static asynStatus writeUInt32(void* ppvt,asynUser* pasynUser,epicsUInt32 value,epicsUInt32 mask);


/* Forward references for utility methods */
static void scanSen(Port* pport);
static epicsFloat64 lookupCode(int code);


/* Forward references for sensoray support methods */
static int senReset(int base);
static short senRecvB(int base);
static short senRecvW(int base);
static int senGetModel(int base);
static float senGetVersion(int base);
static void senSendB(int data,int base);
static void senSendW(int data,int base);
static int senGetChannel(int chn,int base);
static void senSetCode(int chn,int code,int base);


/* Define local variants */
static Port* pports[] = {NULL,NULL,NULL,NULL,NULL,NULL};
static const int portCount=(sizeof(pports)/sizeof(Port*));
static Scaling scaling[] =
{
/* Voltage      */  {0x17,0.000005},{0x16,0.00002},{0x15,00.0002},{0x30,0.07808},
/* Resistence   */  {0x0A,0.020000},{0x14,0.12500},{0x20,31.0000},
/* Thermocouple */  {0x24,0.100000},{0x23,0.10000},{0x01,00.1000},{0x1B,0.10000},{0x1C,0.1000},{0x22,0.10},{0x1D,0.10},{0x1E,0.10},{0x1F,0.10},
/* RTD          */  {0x2C,0.100000},{0x18,0.05000},{0x19,00.0500},{0x2A,0.01250},{0x2B,0.0125},{0x21,0.10},
/* Thermistor   */  {0x1A,0.010000},
/* Current loop */  {0x11,0.010000},
/* Strain gage -- unsupported */
};
static const int scalingCount=(sizeof(scaling)/sizeof(Scaling));


/* Define macros */
#define READ_REASON(p) ((p)->reason==senInput)
#define WRITE_REASON(p) ((p)->reason==senType||(p)->reason==senCode)


/****************************************************************************
 * Define public interface methods
 ****************************************************************************/
static int drvSenInit(const char* port,int type,int inst,int addr,int chns,int rate)
{
    Port* pport;
    char name[20];
    int len,ns;
    unsigned int prio;
    asynFloat64* pasynFloat64;
    asynUInt32Digital* pasynUInt32;

    if( inst>=portCount )
    {
        printf("drvSen:init %s: max. number of instances exceeded %d\n",port,inst);
        return( -1 );
    }
    if( pports[inst] )
    {
        printf("drvSen:init %s: board instance %d already used for type %d\n",port,inst,pports[inst]->type);
        return( -1 );
    }
    if( rate<1 || rate>5 )
    {
        printf("drvSen:init %s: invalid sampling frequency %dHz specified\n",port,rate);
        return( -1 );
    }
    if( chns>8 )
    {
        printf("drvSen::init %s: board type %d cannot have > 8 channels\n",port,type);
        return( - 1 );
    }
    if( type!=SEN_SMARTAD )
    {
        printf("drvSen::init %s: unsupported board type %d\n",port,type);
        return( - 1 );
    }
    if( senReset(addr) )
    {
        printf("drvSen::init %s: board type %d not found\n",port,type);
        return( - 1 );
    }

    ns    = strlen(port)+1;
    len   = sizeof(Port)+sizeof(asynFloat64)+sizeof(asynUInt32Digital)+ns+(sizeof(Channel)*chns);
    pport = (Port*)callocMustSucceed(len,sizeof(char),"drvSenInit");

    pasynUInt32  = (asynUInt32Digital*)(pport + 1);
    pasynFloat64 = (asynFloat64*)(pasynUInt32 + 1);
    pport->name  = (char*)(pasynFloat64 + 1);
    pport->chn   = (Channel*)(pport->name + ns);

    pport->base = addr;
    pport->type = type;
    pport->inst = inst;
    pport->rate = rate;
    pport->cnt  = chns;

    pport->conn = FALSE;
    strcpy(pport->name,port);
    pport->syncLock = epicsMutexMustCreate();

    pport->model   = senGetModel(pport->base);
    pport->version = senGetVersion(pport->base);

    if( pasynManager->registerPort(port,ASYN_MULTIDEVICE|ASYN_CANBLOCK,1,0,0) )
    {
        printf("drvSen::init %s: failure to register port\n",port);
        free(pport);
        return( -1 );
    }

    pport->asynCommon.interfaceType = asynCommonType;
    pport->asynCommon.pinterface = &common;
    pport->asynCommon.drvPvt = pport;

    if( pasynManager->registerInterface(port,&pport->asynCommon) )
    {
        printf("drvSen::init %s: failure to register asynCommon\n",port);
        return( -1 );
    }

    pport->asynDrvUser.interfaceType = asynDrvUserType;
    pport->asynDrvUser.pinterface = &drvuser;
    pport->asynDrvUser.drvPvt = pport;

    if( pasynManager->registerInterface(port,&pport->asynDrvUser) )
    {
        printf("drvSen::init %s: failure to register asynDrvUser\n",port);
        return( -1 );
    }

    pasynFloat64->read = readFloat64;
    pasynFloat64->write = writeFloat64;
    pport->asynFloat64.interfaceType = asynFloat64Type;
    pport->asynFloat64.pinterface = pasynFloat64;
    pport->asynFloat64.drvPvt = pport;

    if( pasynFloat64Base->initialize(port,&pport->asynFloat64) )
    {
        printf("drvSen::init %s: failure to initialize asynFloat64Base\n",port);
        return( -1 );
    }

    if( pasynManager->registerInterruptSource(pport->name,&pport->asynFloat64,&pport->asynFloat64CallbackPvt) )
    {
        printf("drvSen::init %s: failure to registerInterrupSource for asynFloat64Base\n",port);
        return( -1 );
    }

    pasynUInt32->read = readUInt32;
    pasynUInt32->write = writeUInt32;
    pport->asynUInt32.interfaceType = asynUInt32DigitalType;
    pport->asynUInt32.pinterface = pasynUInt32;
    pport->asynUInt32.drvPvt = pport;

    if( pasynUInt32DigitalBase->initialize(port,&pport->asynUInt32) )
    {
        printf("drvSen::init %s: failure to initialize asynUInt32DigitalBase\n",port);
        return( -1 );
    }

    if( pasynManager->registerInterruptSource(port,&pport->asynUInt32,&pport->asynUInt32CallbackPvt) )
    {
        printf("drvSen::init %s: failure to registerInterruptSource for asynUInt32DigitalBase\n",port);
        return( -1 );
    }

    pports[pport->inst] = pport;

    sprintf(name,"scanSen-%s",pport->name);
    epicsThreadLowestPriorityLevelAbove(epicsThreadPriorityScanHigh,&prio);
    epicsThreadCreate(name,prio,epicsThreadGetStackSize(epicsThreadStackSmall),(EPICSTHREADFUNC)scanSen,pport);

    return( 0 );
}


static epicsFloat64 lookupCode(int code)
{
    int i;
    for( i=0;i<scalingCount;++i ) if( scaling[i].code==code ) break;
    return((i==scalingCount)?-1.0:scaling[i].value);
}


static void scanSen(Port* pport)
{
    double rate=0.0;

    Channel* pchn;
    ELLLIST* plist;
    interruptNode* pnode;
    asynFloat64Interrupt* pintrFloat64;
    asynUInt32DigitalInterrupt* pintrUInt32;

    if( pport->rate>0 )
        rate = 1 / pport->rate;
    else if( pport->rate==0 )
        rate = 0.200;
    else
    {
        printf("scanSen() - \"%s\" - rate disabled - suspending\n",pport->name);
        epicsThreadSuspendSelf();
    }

    while(0==0)
    {
        epicsThreadSleep( rate );

        epicsMutexMustLock(pport->syncLock);
        for(int i=0; i<pport->cnt; ++i )
        {
            if( pport->chn[i].code==0x13 ) continue;
            pport->chn[i].value = (epicsFloat64)(senGetChannel(pport->chn[i].number,pport->base)*pport->chn[i].scale);
        }
        epicsMutexUnlock(pport->syncLock);

        pasynManager->interruptStart(pport->asynUInt32CallbackPvt,&plist);
        pnode = (interruptNode*)ellFirst(plist);
        while( pnode )
        {
            pintrUInt32 = (asynUInt32DigitalInterrupt*)pnode->drvPvt;
            pchn = (Channel*)pintrUInt32->pasynUser->drvUser;

            pintrUInt32->callback(pintrUInt32->userPvt,pintrUInt32->pasynUser,0);
            pnode = (interruptNode*)ellNext(&pnode->node);
        }
        pasynManager->interruptEnd(pport->asynUInt32CallbackPvt);

        pasynManager->interruptStart(pport->asynFloat64CallbackPvt,&plist);
        pnode = (interruptNode*)ellFirst(plist);
        while( pnode )
        {
            pintrFloat64 = (asynFloat64Interrupt*)pnode->drvPvt;
            pchn = (Channel*)pintrFloat64->pasynUser->drvUser;

            if( pintrFloat64->pasynUser->reason==senInput ) pintrFloat64->callback(pintrFloat64->userPvt,pintrFloat64->pasynUser,pchn->value);
            pnode = (interruptNode*)ellNext(&pnode->node);
        }
        pasynManager->interruptEnd(pport->asynFloat64CallbackPvt);

    }

}


/****************************************************************************
 * Define private interface sensoray support methods
 ****************************************************************************/
static short senRecvB(int base)
{
    while((inport(base+1)&0x40)==0);
    return( inport(base) );
}
static void senSendB(int data,int base)
{
    while((inport(base+1)&0x80)==0);
    outport(data,base);
}


static short senRecvW(int base)
{
    return( (senRecvB(base)<<8) | senRecvB(base) );
}
static void senSendW(int data,int base)
{
    senSendB(data>>8,base);
    senSendB(data,base);
}


static int senReset(int base)
{
    int i;
    outport(0,base+1);
    for(i=0;i<100000;++i) if( (inport(base+1)&0x10)==0 ) break;
    return((i==100000)?-1:0);
}
static int senGetModel(int base)
{
    senSendB(240,base);
    senSendB(4,base);
    senSendB(0,base);

    return( senRecvW(base) );
}
static float senGetVersion(int base)
{
    senSendB(240,base);
    senSendB(5,base);
    senSendB(0,base);

    return( (float)(senRecvW(base)/100.0) );
}
static int senGetChannel(int chn,int base)
{
    senSendB(chn,base);
    return( senRecvW(base) );    
}
static void senSetCode(int chn,int code,int base)
{
    senSendB(16+chn,base);
    senSendB(code,base);
}


/****************************************************************************
 * Define private interface asynCommon methods
 ****************************************************************************/
static void report(void* ppvt,FILE* fp,int details)
{
    ELLLIST* plist;
    interruptNode* pnode;
    Port* pport = (Port*)ppvt;
    asynFloat64Interrupt* pintrFloat64;
    asynUInt32DigitalInterrupt* pintrUInt32;

    fprintf(fp,"    EBRICK sensor input @ 0x%4.4X is type %d\n",pport->base,pport->type);
    fprintf(fp,"    Has %d channels,sampling @ %dHz\n",pport->cnt,pport->rate);
    fprintf(fp,"    Sensory model %d version V%.2f\n",pport->model,pport->version);

    pasynManager->interruptStart(pport->asynFloat64CallbackPvt,&plist);
    pnode = (interruptNode*)ellFirst(plist);
    if( pnode==NULL ) fprintf(fp, "        no Float64 callback clients\n");
    while( pnode )
    {
        pintrFloat64 = (asynFloat64Interrupt*)pnode->drvPvt;
        fprintf(fp, "        callback Float64 client addr is %d\n",pintrFloat64->addr);
        pnode = (interruptNode*)ellNext(&pnode->node);
    }
    pasynManager->interruptEnd(pport->asynFloat64CallbackPvt);

    pasynManager->interruptStart(pport->asynUInt32CallbackPvt,&plist);
    pnode = (interruptNode*)ellFirst(plist);
    if( pnode==NULL ) fprintf(fp, "        no UInt32 callback clients\n");
    while( pnode )
    {
        pintrUInt32 = (asynUInt32DigitalInterrupt*)pnode->drvPvt;
        fprintf(fp, "        callback UInt32 client addr is %d\n",pintrUInt32->addr);
        pnode = (interruptNode*)ellNext(&pnode->node);
    }
    pasynManager->interruptEnd(pport->asynUInt32CallbackPvt);

    for( int i=0; i<pport->cnt;++i ) fprintf(fp,"        chan %i connected %s type %d code 0x%2.2X\n",i,(pport->chn[i].conn==TRUE)?"Yes":"No",pport->chn[i].type,pport->chn[i].code);
}


static asynStatus connect(void* ppvt,asynUser* pasynUser)
{
    int addr;
    asynStatus sts;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvSen::connect %s:\n",pport->name);

    if( (sts=pasynManager->getAddr(pasynUser,&addr)) )
        return( sts );

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvSen::connect %s: asyn - 0x%8.8X, addr - %d\n",pport->name,(int)pasynUser,(int)addr);

    if( addr>pport->cnt )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvSen::connect %s: illegal addr %d\n",pport->name,addr);
        return( asynError );
    }

    if( addr>=0 )
    {
        if( pport->chn[addr].conn==TRUE )
        {
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvSen::connect %s: already connected to %d\n",pport->name,addr);
            return( asynError );
        }
    
        pport->chn[addr].conn = TRUE;
        pasynManager->exceptionConnect(pasynUser);
        return( asynSuccess );
    }

    if( pport->conn )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvSen::connect %s: device %d already connected\n",pport->name,addr);
        return( asynError );
    }

    pport->conn = TRUE;
    pasynManager->exceptionConnect(pasynUser);
    return( asynSuccess );
}


static asynStatus disconnect(void* ppvt,asynUser* pasynUser)
{
    int addr;
    asynStatus sts;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvSen::disconnect %s:\n",pport->name);

    if( (sts=pasynManager->getAddr(pasynUser,&addr)) )
        return( sts );

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvSen::disconnect %s: asyn - 0x%8.8X, addr - %d\n",pport->name,(int)pasynUser,(int)addr);

    if( addr>pport->cnt )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvSen::disconnect %s: illegal addr %d\n",pport->name,addr);
        return( asynError );
    }

    if( addr>=0 )
    {
        if( pport->chn[addr].conn==FALSE )
        {
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvSen::disconnect %s: not connected to %d\n",pport->name,addr);
            return( asynError );
        }

        pport->chn[addr].conn = FALSE;
        pasynManager->exceptionDisconnect(pasynUser);
        return( asynSuccess );
    }

    if( pport->conn==FALSE )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvSen::disconnect %s: not connected to %d\n",pport->name,addr);
        return( asynError );
    }

    pport->conn = FALSE;
    pasynManager->exceptionDisconnect(pasynUser);
    return( asynSuccess );
}


/****************************************************************************
 * Define private interface asynDrvUser methods
 ****************************************************************************/
static asynStatus create(void* ppvt,asynUser* pasynUser,const char* drvInfo, const char** pptypeName,size_t* psize)
{
    int addr;
    Channel* pchn;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvSen::create %s:\n",pport->name);

    pasynUser->reason = senNone;
    if( pasynManager->getAddr(pasynUser,&addr)==asynSuccess )
    {
        if( addr<pport->cnt )
            pchn = &pport->chn[addr];
        else
        {
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvSen::create %s: invalid register number %d\n",pport->name,addr);
            return( asynError );
        }
    }
    else
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvSen::create %s: getAddr failure\n",pport->name);
        return( asynError );
    }

    if( epicsStrCaseCmp("Input",drvInfo)==0 )
        pasynUser->reason = senInput;
    else if( epicsStrCaseCmp("Type",drvInfo)==0 )
        pasynUser->reason = senType;
    else if( epicsStrCaseCmp("Code",drvInfo)==0 )
        pasynUser->reason = senCode;
    else
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDio::create %s: failure to determine channel %d function\n",pport->name,addr);
        return( asynError );
    }

    pchn->pport  = pport;
    pchn->number = addr;
    pasynUser->drvUser = pchn;

    return( asynSuccess );
}


static asynStatus gettype(void* ppvt,asynUser* pasynUser,const char** pptypeName,size_t* psize)
{
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvSen::gettype %s:\n",pport->name);

    if( pptypeName ) *pptypeName = NULL;
    if( psize ) *psize = 0;

    return( asynSuccess );
}


static asynStatus destroy(void* ppvt,asynUser* pasynUser)
{
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvSen::destroy %s:\n",pport->name);

    if( pport==NULL )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvSen::destroy %s: called before create\n",pport->name);
        return( asynError );
    }

    return( asynSuccess );
}


/****************************************************************************
 * Define private interface asynFloat64 methods
 ****************************************************************************/
static asynStatus writeFloat64(void* ppvt,asynUser* pasynUser,epicsFloat64 value)
{
    int addr;
    Channel* pchn;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvSen::writeFloat64 %s:\n",pport->name);
    if( pasynUser->reason!=senInput ) return( asynError );

    if( pasynManager->getAddr(pasynUser,&addr)==asynSuccess )
        pchn = &pport->chn[addr];
    else
        return( asynError );

    epicsMutexMustLock(pport->syncLock);
    epicsMutexUnlock(pport->syncLock);

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvSen::writeFloat64 %s: asyn - 0x%8.8X, addr - %d, value - %.3f\n",pport->name,(int)pasynUser,addr,value);
    return( asynSuccess );
}


static asynStatus readFloat64(void* ppvt,asynUser* pasynUser,epicsFloat64* value)
{
    int addr;
    Channel* pchn;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvSen::readFloat64 %s: reason - %d\n",pport->name,pasynUser->reason);
    if( pasynUser->reason!=senInput ) return( asynError );

    if( pasynManager->getAddr(pasynUser,&addr)==asynSuccess )
        pchn = &pport->chn[addr];
    else
        return( asynError );

    epicsMutexMustLock(pport->syncLock);
    *value = pchn->value;
    epicsMutexUnlock(pport->syncLock);

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvSen::readFloat64 %s: asyn - 0x%8.8X, addr - %d, value - %.3f\n",pport->name,(int)pasynUser,addr,*value);
    return( asynSuccess );
}


/****************************************************************************
 * Define private interface asynUInt32Digital methods
 ****************************************************************************/
static asynStatus writeUInt32(void* ppvt,asynUser* pasynUser,epicsUInt32 value,epicsUInt32 mask)
{
    int addr;
    Channel* pchn;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvSen::writeUInt32 %s: asyn - 0x%8.8X, mask - 0x%8.8X, value - 0x%8.8X\n",pport->name,(int)pasynUser,(int)mask,(int)value);
    if( !WRITE_REASON(pasynUser) ) return( asynError );

    if( pasynManager->getAddr(pasynUser,&addr)==asynSuccess )
        pchn = &pport->chn[addr];
    else
        return( asynError );

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvSen::writeUInt32 %s: asyn - 0x%8.8X, addr - %d, mask - 0x%8.8X, value - 0x%8.8X\n",pport->name,(int)pasynUser,(int)pchn->number,(int)mask,(int)value);

    epicsMutexMustLock(pport->syncLock);
    if( pasynUser->reason==senType )
    {
        pchn->type=(value&mask);
        if(pchn->type==0)
        {
            pchn->code=0x13;
            pchn->scale=0.0;
            senSetCode(pchn->number,pchn->code,pport->base);
        }
        else if( pchn->type==5 )
        {
            pchn->code=0x1A;
            pchn->scale=0.01;
            senSetCode(pchn->number,pchn->code,pport->base);
        }
        else if( pchn->type==6 )
        {
            pchn->code=0x11;
            pchn->scale=0.01;
            senSetCode(pchn->number,pchn->code,pport->base);
        }
    }
    else if( pasynUser->reason==senCode )
    {
        pchn->code=(value&mask);
        pchn->scale=lookupCode(pchn->code);
        if( pchn->scale>-1.0) senSetCode(pchn->number,pchn->code,pport->base);
    }
    epicsMutexUnlock(pport->syncLock);

    return( asynSuccess );
}


static asynStatus readUInt32(void* ppvt,asynUser* pasynUser,epicsUInt32* value,epicsUInt32 mask)
{
    int addr;
    Channel* pchn;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvSen::readUInt32 %s:\n",pport->name);
    if( !READ_REASON(pasynUser) ) return( asynError );

    if( pasynManager->getAddr(pasynUser,&addr)==asynSuccess )
        pchn = &pport->chn[addr];
    else
        return( asynError );

    epicsMutexMustLock(pport->syncLock);
    epicsMutexUnlock(pport->syncLock);

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvSen::readUInt32 %s: asyn - 0x%8.8X, mask - 0x%8.8X, value - 0x%8.8X\n",pport->name,(int)pasynUser,(int)mask,(int)*value);

    return( asynSuccess );
}


/****************************************************************************
 * Register public methods
 ****************************************************************************/

/* Initialization method definitions */
static const iocshArg arg0 = {"port",iocshArgString};
static const iocshArg arg1 = {"type",iocshArgInt};
static const iocshArg arg2 = {"inst",iocshArgInt};
static const iocshArg arg3 = {"addr",iocshArgInt};
static const iocshArg arg4 = {"chns",iocshArgInt};
static const iocshArg arg5 = {"rate",iocshArgInt};
static const iocshArg* args[]= {&arg0,&arg1,&arg2,&arg3,&arg4,&arg5};
static const iocshFuncDef drvSenInitFuncDef = {"drvSenInit",6,args};
static void drvSenInitCallFunc(const iocshArgBuf* args)
{
    drvSenInit(args[0].sval,args[1].ival,args[2].ival,args[3].ival,args[4].ival,args[5].ival);
}

/* Registration method */
static void drvSenRegister(void)
{
    static int firstTime = 1;

    if( firstTime )
    {
        firstTime = 0;
        iocshRegister( &drvSenInitFuncDef,drvSenInitCallFunc );
    }
}
epicsExportRegistrar( drvSenRegister );
