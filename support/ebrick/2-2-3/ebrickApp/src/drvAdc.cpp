/*

                          Argonne National Laboratory
                            APS Operations Division
                     Beamline Controls and Data Acquisition

                          EPICS Brick ADC Port Driver



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
    initialize the driver, the method drvAdcInit() is called from the
    startup script with the following calling sequence.

        drvAdcInit(port,type,inst,addr,chns,pol,rate)

        Where:
            port - Port driver name (i.e. "ADC" )
            type - Board type (see boardtypes.h)
            inst - Board instance
            addr - Board address
            chns - Register count
            pol  - Polarity (0=bipolar,1=unipolar)
            rate - Interrupt rate (in Hz)
                   =0 - Default 20Hz
                   >0 - (in Hz)
                   <0 - Disabled

    The method dbior can be called from the IOC shell to display the current
    status of the driver.


 Developer notes:
    1. User manual ICMS documents:
       Athena     - APS_1251097
       Poseidon   - APS_1251696


 Source control info:
    Modified by:    dkline
                    2008/07/03 19:19:34
                    1.11

 =============================================================================
 History:
 Author: David M. Kline
 -----------------------------------------------------------------------------
 2008-Feb-01  DMK  Revamped support,removed unused support,added Poseidon.
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
typedef enum {adcNone,adcInput,adcGain,adcStatus} Reason;


/* Declare register info structure */
struct Channel
{
    int isConn;
    int chnNum;
    Port* pport;
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
    int           gain;
    int           isConn;
    int           isSingle;
    int           isBipolar;
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
static int drvAdcInit(const char* port,int type,int inst,int addr,int chns,int pol,int rate);


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
static void adcInit(Port* pport);
static void adcRead(Port* pport);
static void scanAdc(Port* pport);


/* Define local variants */
static Port* pports[] = {NULL,NULL,NULL,NULL,NULL,NULL};
static const int portCount=(sizeof(pports)/sizeof(Port*));
static epicsFloat64 athenaGain[2][4] = {{0.0,8.3,5.0,2.5},{10.0,5.0,2.5,1.25}};

/* Define macros */
#define READ_REASON(p) ((p)->reason==adcGain||(p)->reason==adcStatus)


/****************************************************************************
 * Define public interface methods
 ****************************************************************************/
static int drvAdcInit(const char* port,int type,int inst,int addr,int chns,int pol,int rate)
{
    Port* pport;
    char name[20];
    int cmax,len,ns;
    unsigned int prio;
    asynFloat64* pasynFloat64;
    asynUInt32Digital* pasynUInt32;

    if( inst >= portCount )
    {
        printf("drvAdc:init %s: max. number of instances exceeded %d\n",port,inst);
        return( -1 );
    }
    if( pports[inst] )
    {
        printf("drvAdc:init %s: board instance %d already used for type %d\n",port,inst,pports[inst]->type);
        return( -1 );
    }
    if( pol < 0 || pol > 1 )
    {
        printf("drvAdc:init %s: invalid polarization %d specified\n",port,pol);
        return( -1 );
    }

    ns    = strlen(port)+1;
    len   = sizeof(Port)+sizeof(asynFloat64)+sizeof(asynUInt32Digital)+ns+(sizeof(Channel)*chns);
    pport = (Port*)callocMustSucceed(len,sizeof(char),"drvAdcInit");

    pasynUInt32  = (asynUInt32Digital*)(pport + 1);
    pasynFloat64 = (asynFloat64*)(pasynUInt32 + 1);
    pport->name  = (char*)(pasynFloat64 + 1);
    pport->chn   = (Channel*)(pport->name + ns);

    pport->type = type;
    pport->inst = inst;
    pport->rate = rate;
    pport->cnt  = chns;

    pport->isConn    = FALSE;
    pport->isSingle  = FALSE;
    pport->isBipolar = (pol)?FALSE:TRUE;

    strcpy(pport->name,port);
    pport->syncLock = epicsMutexMustCreate();

    switch( type )
    {
    case DSC_ATHENA:
        cmax = 16;
        pport->base = addr;
        pport->gain = (pport->isBipolar)?0:1;
        pport->isSingle = ((inport(pport->base+3)&0x40)==0x40)?TRUE:FALSE;
        break;

    case DSC_POSEIDON:
        cmax = 32;
        pport->base = addr;
        pport->gain = (pport->isBipolar)?8:12;
        pport->isSingle = ((inport(pport->base+8)&0x60)==0x60)?TRUE:FALSE;
        break;

    default:
        printf("drvAdc::init %s: unknown board type %d\n",port,type);
        free(pport);
        return( - 1 );
    }

    if( chns > cmax )
    {
        printf("drvAdc::init %s: board type %d cannot have > %d channels\n",port,type,cmax);
        free(pport);
        return( - 1 );
    }

    if( !pport->isSingle )
    {
        printf("drvAdc::init %s: only single-ended supported\n",port);
        free(pport);
        return( -1 );
    }

    if( pasynManager->registerPort(port,ASYN_MULTIDEVICE|ASYN_CANBLOCK,1,0,0) )
    {
        printf("drvAdc::init %s: failure to register port\n",port);
        free(pport);
        return( -1 );
    }

    pport->asynCommon.interfaceType = asynCommonType;
    pport->asynCommon.pinterface = &common;
    pport->asynCommon.drvPvt = pport;

    if( pasynManager->registerInterface(port,&pport->asynCommon) )
    {
        printf("drvAdc::init %s: failure to register asynCommon\n",port);
        return( -1 );
    }

    pport->asynDrvUser.interfaceType = asynDrvUserType;
    pport->asynDrvUser.pinterface = &drvuser;
    pport->asynDrvUser.drvPvt = pport;

    if( pasynManager->registerInterface(port,&pport->asynDrvUser) )
    {
        printf("drvAdc::init %s: failure to register asynDrvUser\n",port);
        return( -1 );
    }

    pasynFloat64->read = readFloat64;
    pasynFloat64->write = writeFloat64;
    pport->asynFloat64.interfaceType = asynFloat64Type;
    pport->asynFloat64.pinterface = pasynFloat64;
    pport->asynFloat64.drvPvt = pport;

    if( pasynFloat64Base->initialize(port,&pport->asynFloat64) )
    {
        printf("drvAdc::init %s: failure to initialize asynFloat64Base\n",port);
        return( -1 );
    }

    if( pasynManager->registerInterruptSource(pport->name,&pport->asynFloat64,&pport->asynFloat64CallbackPvt) )
    {
        printf("drvAdc::init %s: failure to registerInterrupSource for asynFloat64Base\n",port);
        return( -1 );
    }

    pasynUInt32->read = readUInt32;
    pasynUInt32->write = writeUInt32;
    pport->asynUInt32.interfaceType = asynUInt32DigitalType;
    pport->asynUInt32.pinterface = pasynUInt32;
    pport->asynUInt32.drvPvt = pport;

    if( pasynUInt32DigitalBase->initialize(port,&pport->asynUInt32) )
    {
        printf("drvAdc::init %s: failure to initialize asynUInt32DigitalBase\n",port);
        return( -1 );
    }

    if( pasynManager->registerInterruptSource(port,&pport->asynUInt32,&pport->asynUInt32CallbackPvt) )
    {
        printf("drvAdc::init %s: failure to registerInterruptSource for asynUInt32DigitalBase\n",port);
        return( -1 );
    }

    pports[pport->inst] = pport;

    sprintf(name,"scanAdc-%s",pport->name);
    epicsThreadLowestPriorityLevelAbove(epicsThreadPriorityScanHigh,&prio);
    epicsThreadCreate(name,prio,epicsThreadGetStackSize(epicsThreadStackSmall),(EPICSTHREADFUNC)scanAdc,pport);

    return( 0 );
}


static void adcInit(Port* pport)
{
    switch( pport->type )
    {
    case DSC_ATHENA:
        // Reset FIFO
        outport(0x10,pport->base);
        while( inport(pport->base+3)&0x20 );

        // Write low/high channel
        outport(((pport->cnt-1)<<4),pport->base+2);
        while( inport(pport->base+3)&0x20 );

        // Write analog input / gain / scanen
        outport((pport->gain|0x4),pport->base+3);
        while( inport(pport->base+3)&0x20 );
        break;
    case DSC_POSEIDON:
        // Reset FIFO / scanen
        outport(0x6,pport->base+7);
        while( inport(pport->base+11)&0x80 );

        // Write low/high channel
        outport(0,pport->base+2);
        outport(pport->cnt-1,pport->base+3);
        while( inport(pport->base+11)&0x80 );

        // Write analog input / gain
        outport((inport(pport->base+11)&0x30)|(pport->gain),pport->base+11);
        while( inport(pport->base+11)&0x80 );
        break;
    }

}


static void adcRead(Port* pport)
{
    short v1,v2,val;

    switch( pport->type )
    {
    case DSC_ATHENA:
        // Reset FIFO
        outport(0x10,pport->base);
        while( inport(pport->base+3)&0x20 );

        // Write analog input / gain / scanen
        outport((pport->gain|0x4),pport->base+3);
        while( inport(pport->base+3)&0x20 );

        // Start A/D conversion
        outport(0x80,pport->base);
        while( inport(pport->base+3)&0x80 );

        for(int i=0; i<pport->cnt; ++i )
        {
            v1  = inport(pport->base);
            v2  = inport(pport->base+1);
            val = (v2 << 8) | v1;

            pport->chn[i].value = (pport->isBipolar)?val/32768.0:(val+32768.0)/65536.0;
            pport->chn[i].value *= athenaGain[pport->isBipolar][pport->gain];
        }
        break;
    case DSC_POSEIDON:
        // Reset FIFO / scanen
        outport(0x6,pport->base+7);
        while( inport(pport->base+11)&0x80 );

        // Write analog input / gain
        pport->isBipolar = (pport->gain&0x4)?FALSE:TRUE;
        outport((inport(pport->base+11)&0x30)|(pport->gain),pport->base+11);
        while( inport(pport->base+11)&0x80 );

        // Start A/D conversion
        outport(0,pport->base);
        while( inport(pport->base+8)&0x80 );

        for(int i=0; i<pport->cnt; ++i )
        {
            v1  = inport(pport->base);
            v2  = inport(pport->base+1);
            val = (v2 << 8) | v1;

            pport->chn[i].value = (pport->isBipolar)?val/32768.0:(val+32768.0)/65536.0;
            pport->chn[i].value *= (pport->gain&0x8)?10.0:5.0;
        }
        break;
    }

}


static void scanAdc(Port* pport)
{
    double rate=0.0;

    Channel* pchn;
    ELLLIST* plist;
    interruptNode* pnode;
    asynFloat64Interrupt* pintrFloat64;
    asynUInt32DigitalInterrupt* pintrUInt32;

    if( pport->rate > 0 )
        rate = 1 / pport->rate;
    else if( pport->rate == 0 )
        rate = 0.050;
    else
    {
        printf("scanAdc() - \"%s\" - rate disabled - suspending\n",pport->name);
        epicsThreadSuspendSelf();
    }

    adcInit(pport);
    while(0==0)
    {
        epicsThreadSleep( rate );

        epicsMutexMustLock(pport->syncLock);
        adcRead(pport);
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

            if( pintrFloat64->pasynUser->reason==adcInput ) pintrFloat64->callback(pintrFloat64->userPvt,pintrFloat64->pasynUser,pchn->value);
            pnode = (interruptNode*)ellNext(&pnode->node);
        }
        pasynManager->interruptEnd(pport->asynFloat64CallbackPvt);

    }

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

    fprintf(fp,"    EBRICK analog input @ 0x%4.4X is type %d\n",pport->base,pport->type);
    fprintf(fp,"    Has %d %s %s channels gain %d\n",pport->cnt,(pport->isBipolar)?"bipolar":"unipolar",(pport->isSingle)?"SE":"DF",pport->gain);

    pasynManager->interruptStart(pport->asynFloat64CallbackPvt,&plist);
    pnode = (interruptNode*)ellFirst(plist);
    if( pnode == NULL ) fprintf(fp, "        no Float64 callback clients\n");
    while( pnode )
    {
        pintrFloat64 = (asynFloat64Interrupt*)pnode->drvPvt;
        fprintf(fp, "        callback Float64 client addr is %d\n",pintrFloat64->addr);
        pnode = (interruptNode*)ellNext(&pnode->node);
    }
    pasynManager->interruptEnd(pport->asynFloat64CallbackPvt);

    pasynManager->interruptStart(pport->asynUInt32CallbackPvt,&plist);
    pnode = (interruptNode*)ellFirst(plist);
    if( pnode == NULL ) fprintf(fp, "        no UInt32 callback clients\n");
    while( pnode )
    {
        pintrUInt32 = (asynUInt32DigitalInterrupt*)pnode->drvPvt;
        fprintf(fp, "        callback UInt32 client addr is %d\n",pintrUInt32->addr);
        pnode = (interruptNode*)ellNext(&pnode->node);
    }
    pasynManager->interruptEnd(pport->asynUInt32CallbackPvt);

    for( int i=0; i<pport->cnt;++i ) fprintf(fp,"        chan %i connected %s\n",i,(pport->chn[i].isConn==TRUE)?"Yes":"No");
}


static asynStatus connect(void* ppvt,asynUser* pasynUser)
{
    int addr;
    asynStatus sts;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAdc::connect %s:\n",pport->name);

    if( (sts = pasynManager->getAddr(pasynUser,&addr)) )
        return( sts );

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvAdc::connect %s: asyn - 0x%8.8X, addr - %d\n",pport->name,(int)pasynUser,(int)addr);

    if( addr > pport->cnt )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvAdc::connect %s: illegal addr %d\n",pport->name,addr);
        return( asynError );
    }

    if( addr >= 0 )
    {
        if( pport->chn[addr].isConn == TRUE )
        {
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvAdc::connect %s: already connected to %d\n",pport->name,addr);
            return( asynError );
        }
    
        pport->chn[addr].isConn = TRUE;
        pasynManager->exceptionConnect(pasynUser);
        return( asynSuccess );
    }

    if( pport->isConn )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvAdc::connect %s: device %d already connected\n",pport->name,addr);
        return( asynError );
    }

    pport->isConn = TRUE;
    pasynManager->exceptionConnect(pasynUser);
    return( asynSuccess );
}


static asynStatus disconnect(void* ppvt,asynUser* pasynUser)
{
    int addr;
    asynStatus sts;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAdc::disconnect %s:\n",pport->name);

    if( (sts = pasynManager->getAddr(pasynUser,&addr)) )
        return( sts );

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvAdc::disconnect %s: asyn - 0x%8.8X, addr - %d\n",pport->name,(int)pasynUser,(int)addr);

    if( addr > pport->cnt )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvAdc::disconnect %s: illegal addr %d\n",pport->name,addr);
        return( asynError );
    }

    if( addr >= 0 )
    {
        if( pport->chn[addr].isConn == FALSE )
        {
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvAdc::disconnect %s: not connected to %d\n",pport->name,addr);
            return( asynError );
        }

        pport->chn[addr].isConn = FALSE;
        pasynManager->exceptionDisconnect(pasynUser);
        return( asynSuccess );
    }

    if( pport->isConn == FALSE )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvAdc::disconnect %s: not connected to %d\n",pport->name,addr);
        return( asynError );
    }

    pport->isConn = FALSE;
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

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAdc::create %s:\n",pport->name);

    if( pasynManager->getAddr(pasynUser,&addr) == asynSuccess )
    {
        if( addr < 0 )
        {
            if( epicsStrCaseCmp("Gain",drvInfo) == 0 )
                pasynUser->reason = adcGain;
            else if( epicsStrCaseCmp("Status",drvInfo) == 0 )
                pasynUser->reason = adcStatus;
            else
            {
                asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDio::create %s: invalid %s function found\n",pport->name,drvInfo);
                return( asynError );
            }

            return( asynSuccess );
        }

        if( epicsStrCaseCmp("Input",drvInfo) == 0 )
            pasynUser->reason = adcInput;
        else
        {
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDio::create %s: failure to determine channel %d function\n",pport->name,addr);
            return( asynError );
        }

        if( addr < pport->cnt )
            pchn = &pport->chn[addr];
        else
        {
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvAdc::create %s: invalid register number %d\n",pport->name,addr);
            return( asynError );
        }
    }
    else
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvAdc::create %s: getAddr failure\n",pport->name);
        return( asynError );
    }

    pchn->pport   = pport;
    pchn->chnNum  = addr;
    pasynUser->drvUser = pchn;

    switch( pport->type )
    {
    case DSC_ATHENA:
        if( pchn->chnNum > 15 )
        {
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvAdc::create %s: invalid channel number %d\n",pport->name,pchn->chnNum);
            return( asynError );
        }
        break;

    case DSC_POSEIDON:
        if( pchn->chnNum > 31 )
        {
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvAdc::create %s: invalid register number %d\n",pport->name,pchn->chnNum);
            return( asynError );
        }
        break;
    }

    return( asynSuccess );
}


static asynStatus gettype(void* ppvt,asynUser* pasynUser,const char** pptypeName,size_t* psize)
{
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAdc::gettype %s:\n",pport->name);

    if( pptypeName ) *pptypeName = NULL;
    if( psize ) *psize = 0;

    return( asynSuccess );
}


static asynStatus destroy(void* ppvt,asynUser* pasynUser)
{
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAdc::destroy %s:\n",pport->name);

    if( pport == NULL )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvAdc::destroy %s: called before create\n",pport->name);
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

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAdc::writeFloat64 %s:\n",pport->name);
    if( pasynUser->reason != adcInput ) return( asynError );

    if( pasynManager->getAddr(pasynUser,&addr) == asynSuccess )
        pchn = &pport->chn[addr];
    else
        return( asynError );

    epicsMutexMustLock(pport->syncLock);
    epicsMutexUnlock(pport->syncLock);

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvAdc::writeFloat64 %s: asyn - 0x%8.8X, addr - %d, value - %.3f\n",pport->name,(int)pasynUser,addr,value);

    return( asynSuccess );
}


static asynStatus readFloat64(void* ppvt,asynUser* pasynUser,epicsFloat64* value)
{
    int addr;
    Channel* pchn;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAdc::readFloat64 %s: reason - %d\n",pport->name,pasynUser->reason);
    if( pasynUser->reason != adcInput ) return( asynError );

    if( pasynManager->getAddr(pasynUser,&addr) == asynSuccess )
        pchn = &pport->chn[addr];
    else
        return( asynError );

    epicsMutexMustLock(pport->syncLock);
    *value = pchn->value;
    epicsMutexUnlock(pport->syncLock);

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvAdc::readFloat64 %s: asyn - 0x%8.8X, addr - %d, value - %.3f\n",pport->name,(int)pasynUser,addr,*value);
    return( asynSuccess );
}


/****************************************************************************
 * Define private interface asynUInt32Digital methods
 ****************************************************************************/
static asynStatus writeUInt32(void* ppvt,asynUser* pasynUser,epicsUInt32 value,epicsUInt32 mask)
{
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvAdc::writeUInt32 %s: asyn - 0x%8.8X, mask - 0x%8.8X, value - 0x%8.8X\n",pport->name,(int)pasynUser,(int)mask,(int)value);
    if( pasynUser->reason != adcGain ) return( asynError );

    epicsMutexMustLock(pport->syncLock);
    switch( pport->type )
    {
    case DSC_ATHENA:
        if( pasynUser->reason==adcGain ) pport->gain=(value&mask);
        break;
    case DSC_POSEIDON:
        if( pasynUser->reason==adcGain ) pport->gain=(value&mask);
        break;
    }
    epicsMutexUnlock(pport->syncLock);

    return( asynSuccess );
}


static asynStatus readUInt32(void* ppvt,asynUser* pasynUser,epicsUInt32* value,epicsUInt32 mask)
{
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAdc::readUInt32 %s:\n",pport->name);
    if( !READ_REASON(pasynUser) ) return( asynError );

    epicsMutexMustLock(pport->syncLock);
    switch( pport->type )
    {
    case DSC_ATHENA:
        if( pasynUser->reason==adcGain ) *value=(epicsUInt32)pport->gain & mask;
        break;
    case DSC_POSEIDON:
        if( pasynUser->reason==adcGain ) *value=(epicsUInt32)pport->gain & mask;
        break;
    }
    epicsMutexUnlock(pport->syncLock);

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvAdc::readUInt32 %s: asyn - 0x%8.8X, mask - 0x%8.8X, value - 0x%8.8X\n",pport->name,(int)pasynUser,(int)mask,(int)*value);

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
static const iocshArg arg5 = {"pol",iocshArgInt};
static const iocshArg arg6 = {"rate",iocshArgInt};
static const iocshArg* args[]= {&arg0,&arg1,&arg2,&arg3,&arg4,&arg5,&arg6};
static const iocshFuncDef drvAdcInitFuncDef = {"drvAdcInit",7,args};
static void drvAdcInitCallFunc(const iocshArgBuf* args)
{
    drvAdcInit(args[0].sval,args[1].ival,args[2].ival,args[3].ival,args[4].ival,args[5].ival,args[6].ival);
}

/* Registration method */
static void drvAdcRegister(void)
{
    static int firstTime = 1;

    if( firstTime )
    {
        firstTime = 0;
        iocshRegister( &drvAdcInitFuncDef,drvAdcInitCallFunc );
    }
}
epicsExportRegistrar( drvAdcRegister );
