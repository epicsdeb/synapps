/*

                          Argonne National Laboratory
                            APS Operations Division
                     Beamline Controls and Data Acquisition

                          EPICS Brick DIO Port Driver



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
    initialize the driver, the method drvDioInit() is called from the
    startup script with the following calling sequence.

        drvDioInit(port,type,inst,addr,regs,rate)

        Where:
            port - Port driver name (i.e. "DIO" )
            type - Board type (see boardtypes.h)
            inst - Board instance
            addr - Board address, zero for Athena
            regs - Register count
            rate - Interrupt rate (in Hz)
                   =0 - Default 20Hz
                   >0 - (in Hz)
                   <0 - Disabled
                    

    The method dbior can be called from the IOC shell to display the current
    status of the driver.


 Developer notes:
    1. Frequency is *always* disabled when a PMM module is used.
    2. Register offsets for various modules:
         ----------------------------------------------------
        |            | Athena | Ruby-MM | Onyx-MM | Poseidon |
         ----------------------------------------------------
        | Register A |   8    |   12    |   0     |   12     |
         ----------------------------------------------------
        | Register B |   9    |   13    |   1     |   13     |
         ----------------------------------------------------
        | Register C |   10   |   14    |   2     |   14     |
         ----------------------------------------------------
        | Config     |   11   |   15    |   3     |   15     |
         ----------------------------------------------------

    3. User manual ICMS documents:
       Athena     - APS_1251097
       Poseidon   - APS_1251696
       PMM-MM     - APS_1251694
       RMM-MM-416 - APS_1251695
       OMM-MM-XT  - APS_1251693
    4. Usage of hardware interrupts require the 'omm' Linux device driver.

 Source control info:
    Modified by:    dkline
                    2008/02/12 12:19:32
                    1.15

 =============================================================================
 History:
 Author: David M. Kline
 -----------------------------------------------------------------------------
 2008-Jan-25  DMK  Revamped support,removed unused support,added Poseidon.
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
#include <asynUInt32Digital.h>


/* Board definition related include files */
#include "boardtypes.h"


/* Define symbolic constants */
#define IN          (1)
#define OUT         (0)
#define TRUE        (1)
#define FALSE       (0)


/* Forward struct declarations */
typedef struct Reg Reg;
typedef struct Port Port;


/* Declare register info structure */
struct Reg
{
    int isConn;
    int cfgOff;
    int regOff;
    int regNum;
    int regDir;
    Port* pport;
    epicsUInt32 read;
    epicsUInt32 wrote;
};


/* Declare DSC port structure */
struct Port
{
    char*         name;
    int           type;
    int           inst;
    int           base;
    int           rate;
    int           isConn;
    int           isStart;
    epicsMutexId  syncLock;
    asynInterface asynUInt32;
    asynInterface asynCommon;
    asynInterface asynDrvUser;
    void*         asynUInt32CallbackPvt;

    /* Register info */
    unsigned char cfg;
    int           cnt;
    Reg*          reg;
};


/* Public interface forward references */
static int drvDioInit(const char* port,int type,int inst,int addr,int regs,int rate);


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


/* Forward references for asynUInt32Digital methods */
static asynStatus readUInt32(void* ppvt,asynUser* pasynUser,epicsUInt32* value,epicsUInt32 mask);
static asynStatus writeUInt32(void* ppvt,asynUser* pasynUser,epicsUInt32 value,epicsUInt32 mask);


/* Forward references for utility methods */
static void scanDio(Port* pport);
static epicsUInt32 dioRead(Port* pport,Reg* preg);
static void dioWrite(Port* pport,Reg* preg,epicsUInt32 value,epicsUInt32 mask);


/* Define local variants */
static Port* pports[] = {NULL,NULL,NULL,NULL,NULL,NULL};
static const int portCount=(sizeof(pports)/sizeof(Port*));


/* Define macros */
#define ISOK(s)    (asynSuccess==(s))
#define ISNOTOK(s) (!ISOK(s))



/****************************************************************************
 * Define public interface methods
 ****************************************************************************/
static int drvDioInit(const char* port,int type,int inst,int addr,int regs,int rate)
{
    Port* pport;
    int rmax,len,ns;
    asynUInt32Digital* pasynUInt32;

    if( inst >= portCount )
    {
        printf("drvDio:init %s: max. number of instances exceeded %d\n",port,inst);
        return( -1 );
    }
    if( pports[inst] )
    {
        printf("drvDio:init %s: board instance %d already used for type %d\n",port,inst,pports[inst]->type);
        return( -1 );
    }

    ns = strlen(port)+1;
    len = sizeof(Port)+sizeof(asynUInt32Digital)+ns+(sizeof(Reg)*regs);
    pport = (Port*)callocMustSucceed(len,sizeof(char),"drvDioInit");

    pasynUInt32 = (asynUInt32Digital*)(pport + 1);
    pport->name = (char*)(pasynUInt32 + 1);
    pport->reg  = (Reg*)(pport->name + ns);

    pport->cfg = 0x80;
    pport->cnt = regs;
    pport->inst = inst;
    pport->type = type;
    pport->isConn = FALSE;
    pport->isStart = FALSE;
    strcpy(pport->name,port);
    pport->syncLock = epicsMutexMustCreate();

    switch( type )
    {
    case DSC_ATHENA:
    case DSC_RMMSIG:
    case DSC_OMM:
    case DSC_POSEIDON:
        rmax = 3;
        pport->rate = rate;
        pport->base = addr;
        break;

    case DSC_PMM:
        rmax = 2;
        pport->rate = -1;
        pport->base = addr;
        break;

    default:
        printf("drvDio::init %s: unknown board type %d\n",port,type);
        free(pport);
        return( -1 );
    }

    if( regs > rmax )
    {
        printf("drvDio::init %s: board type %d cannot have > %d registers\n",port,type,rmax);
        free(pport);
        return( -1 );
    }

    if( pasynManager->registerPort(port,ASYN_MULTIDEVICE|ASYN_CANBLOCK,1,0,0) )
    {
        printf("drvDio::init %s: failure to register port\n",port);
        free(pport);
        return( -1 );
    }

    pport->asynCommon.interfaceType = asynCommonType;
    pport->asynCommon.pinterface = &common;
    pport->asynCommon.drvPvt = pport;

    if( pasynManager->registerInterface(port,&pport->asynCommon) )
    {
        printf("drvDio::init %s: failure to register asynCommon\n",port);
        return( -1 );
    }

    pport->asynDrvUser.interfaceType = asynDrvUserType;
    pport->asynDrvUser.pinterface = &drvuser;
    pport->asynDrvUser.drvPvt = pport;

    if( pasynManager->registerInterface(port,&pport->asynDrvUser) )
    {
        printf("drvDio::init %s: failure to register asynDrvUser\n",port);
        return( -1 );
    }

    pasynUInt32->read = readUInt32;
    pasynUInt32->write = writeUInt32;
    pport->asynUInt32.interfaceType = asynUInt32DigitalType;
    pport->asynUInt32.pinterface = pasynUInt32;
    pport->asynUInt32.drvPvt = pport;

    if( pasynUInt32DigitalBase->initialize(port,&pport->asynUInt32) )
    {
        printf("drvDio::init %s: failure to initialize asynUInt32DigitalBase\n",port);
        return( -1 );
    }

    if( pasynManager->registerInterruptSource(port,&pport->asynUInt32,&pport->asynUInt32CallbackPvt) )
    {
        printf("drvDio::init %s: failure to registerInterruptSource for asynUInt32\n",port);
        return( -1 );
    }

    pports[pport->inst] = pport;
    return( 0 );
}


static void scanDio(Port* pport)
{
    int i;
    double rate=0.0;

    Reg* preg;
    ELLLIST* plist;
    interruptNode* pnode;
    asynUInt32DigitalInterrupt* pintrUInt32;

    if( pport->rate > 0 )
        rate = 1 / pport->rate;
    else if( pport->rate == 0 )
        rate = 0.050;
    else
    {
        printf("scanDio() - \"%s\" - rate disabled - suspending\n",pport->name);
        epicsThreadSuspendSelf();
    }

    while(0==0)
    {
        epicsThreadSleep( rate );

        epicsMutexMustLock(pport->syncLock);
        for( i=0; i<pport->cnt; ++i )
        {
            preg = &pport->reg[i];
            if( preg->regDir == OUT ) continue;

            dioRead(pport,preg);
        }
        epicsMutexUnlock(pport->syncLock);

        pasynManager->interruptStart(pport->asynUInt32CallbackPvt,&plist);
        pnode = (interruptNode*)ellFirst(plist);
        while( pnode )
        {
            pintrUInt32 = (asynUInt32DigitalInterrupt*)pnode->drvPvt;
            preg = (Reg*)pintrUInt32->pasynUser->drvUser;

            if( preg->regDir == IN ) pintrUInt32->callback(pintrUInt32->userPvt,pintrUInt32->pasynUser,preg->read);

            pnode = (interruptNode*)ellNext(&pnode->node);
        }
        pasynManager->interruptEnd(pport->asynUInt32CallbackPvt);
    }
   
}


/****************************************************************************
 * Define private interface utility methods
 ****************************************************************************/
static epicsUInt32 dioRead(Port* pport,Reg* preg)
{
    switch( pport->type )
    {
    case DSC_POSEIDON:
        outport(1,(pport->base+8));

    case DSC_ATHENA:
    case DSC_RMMSIG:
    case DSC_OMM:
        preg->read  = (epicsUInt32)inport(pport->base+preg->regOff);
        break;

    case DSC_PMM:
        preg->read  = preg->wrote;
        break;
    }

    return( preg->read );
}


static void dioWrite(Port* pport,Reg* preg,epicsUInt32 value,epicsUInt32 mask)
{
    switch( pport->type )
    {
    case DSC_POSEIDON:
        outport(1,(pport->base+8));

    case DSC_ATHENA:
    case DSC_RMMSIG:
    case DSC_OMM:
        preg->read = (epicsUInt32)inport(pport->base+preg->regOff);
        break;

    case DSC_PMM:
        preg->read = preg->wrote;
        break;
    }

    preg->wrote = preg->read & ~(mask ^ value);
    preg->wrote |= (mask & value);
    outport(preg->wrote,(pport->base+preg->regOff));
}


/****************************************************************************
 * Define private interface asynCommon methods
 ****************************************************************************/
static void report(void* ppvt,FILE* fp,int details)
{
    int i;
    Reg* preg;
    ELLLIST* plist;
    interruptNode* pnode;
    Port* pport = (Port*)ppvt;
    asynUInt32DigitalInterrupt* pintrUInt32;

    fprintf(fp,"    EBRICK digital I/O @ 0x%4.4X is type %d and has %d registers\n",pport->base,pport->type,pport->cnt);

    pasynManager->interruptStart(pport->asynUInt32CallbackPvt,&plist);
    pnode = (interruptNode*)ellFirst(plist);
    if( pnode == NULL ) fprintf(fp, "        no callback clients\n");
    while( pnode )
    {
        pintrUInt32 = (asynUInt32DigitalInterrupt*)pnode->drvPvt;
        fprintf(fp,"        callback client addr is %d\n",pintrUInt32->addr);
        pnode = (interruptNode*)ellNext(&pnode->node);
    }
    pasynManager->interruptEnd(pport->asynUInt32CallbackPvt);

    for( i=0; i<pport->cnt; ++i )
    {
        preg = &pport->reg[i];
        fprintf(fp,"        reg %d connected %s dir %s\n",preg->regNum,(preg->isConn==TRUE)?"Yes":"No",(preg->regDir)?"Inp":"Out");
    }

}


static asynStatus connect(void* ppvt,asynUser* pasynUser)
{
    int addr;
    asynStatus sts;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvDio::connect %s:\n",pport->name);

    if( (sts = pasynManager->getAddr(pasynUser,&addr)) )
        return( sts );

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvDio::connect %s: asyn - 0x%8.8X, addr - %d\n",pport->name,(int)pasynUser,(int)addr);

    if( addr > pport->cnt )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDio::connect %s: illegal addr %d\n",pport->name,addr);
        return( asynError );
    }

    if( addr >= 0 )
    {
        if( pport->reg[addr].isConn == TRUE )
        {
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDio::connect %s: already connected to %d\n",pport->name,addr);
            return( asynError );
        }
    
        pport->reg[addr].isConn = TRUE;
        pasynManager->exceptionConnect(pasynUser);
        return( asynSuccess );
    }

    if( pport->isConn )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDio::connect %s: already connected to %d\n",pport->name,addr);
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

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvDio::disconnect %s:\n",pport->name);

    if( (sts = pasynManager->getAddr(pasynUser,&addr)) )
        return( sts );

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvDio::disconnect %s: asyn - 0x%8.8X, addr - %d\n",pport->name,(int)pasynUser,(int)addr);

    if( addr > pport->cnt )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDio::disconnect %s: illegal addr %d\n",pport->name,addr);
        return( asynError );
    }

    if( addr >= 0 )
    {
        if( pport->reg[addr].isConn == FALSE )
        {
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDio::disconnect %s: not connected to %d\n",pport->name,addr);
            return( asynError );
        }

        pport->reg[addr].isConn = FALSE;
        pasynManager->exceptionDisconnect(pasynUser);
        return( asynSuccess );
    }

    if( pport->isConn == FALSE )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDio::disconnect %s: not connected to %d\n",pport->name,addr);
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
    Reg* preg;
    unsigned char mask=0;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvDio::create %s:\n",pport->name);

    if( pasynManager->getAddr(pasynUser,&addr) == asynSuccess )
    {
        if( addr < pport->cnt )
            preg = &pport->reg[addr];
        else
        {
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDio::create %s: invalid register number %d\n",pport->name,addr);
            return( asynError );
        }
    }
    else
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDio::create %s: getAddr failure\n",pport->name);
        return( asynError );
    }

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvDio::create %s: asyn - 0x%8.8X, addr - %d\n",pport->name,(int)pasynUser,(int)addr);

    if( epicsStrCaseCmp("Input",drvInfo) == 0 )
        preg->regDir = IN;
    else if( epicsStrCaseCmp("Output",drvInfo) == 0 )
        preg->regDir = OUT;
    else
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDio::create %s: failure to determine register %d direction\n",pport->name,addr);
        return( asynError );
    }

    preg->pport = pport;
    preg->regNum = addr;
    pasynUser->drvUser = preg;

    switch( pport->type )
    {
    case DSC_ATHENA:
        if( preg->regNum > 2 )
        {
            asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvDio::create %s: invalid register number %d\n",pport->name,preg->regNum);
            return( asynError );
        }

        preg->cfgOff = 11;
        if( preg->regNum == 0 ) {mask = 0x10;preg->regOff = 8;}
        if( preg->regNum == 1 ) {mask = 0x02;preg->regOff = 9;}
        if( preg->regNum == 2 ) {mask = 0x09;preg->regOff = 10;}
        break;

    case DSC_RMMSIG:
        if( preg->regNum > 2 )
        {
            asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvDio::create %s: invalid register number %d\n",pport->name,preg->regNum);
            return( asynError );
        }

        preg->cfgOff = 15;
        if( preg->regNum == 0 ) {mask = 0x10;preg->regOff = 12;}
        if( preg->regNum == 1 ) {mask = 0x02;preg->regOff = 13;}
        if( preg->regNum == 2 ) {mask = 0x09;preg->regOff = 14;}
        break;

    case DSC_PMM:
        if( epicsStrCaseCmp("Input",drvInfo) == 0 )
        {
            asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvDio::create %s: invalid register %d direction\n",pport->name,preg->regNum);
            return( asynError );
        }
        if( preg->regNum > 1 )
        {
            asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvDio::create %s: invalid register number %d\n",pport->name,addr);
            return( asynError );
        }

        preg->cfgOff = -1;
        if( preg->regNum == 0 ) preg->regOff = 0;
        if( preg->regNum == 1 ) preg->regOff = 1;
        break;

    case DSC_OMM:
        if( preg->regNum > 2 )
        {
            asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvDio::create %s: invalid register number %d\n",pport->name,preg->regNum);
            return( asynError );
        }

        preg->cfgOff = 3;
        if( preg->regNum == 0 ) {mask = 0x10;preg->regOff = 0;}
        if( preg->regNum == 1 ) {mask = 0x02;preg->regOff = 1;}
        if( preg->regNum == 2 ) {mask = 0x09;preg->regOff = 2;}
        break;

    case DSC_POSEIDON:
        if( preg->regNum > 2 )
        {
            asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvDio::create %s: invalid register number %d\n",pport->name,preg->regNum);
            return( asynError );
        }

        preg->cfgOff = 15;
        if( preg->regNum == 0 ) {mask = 0x10;preg->regOff = 12;}
        if( preg->regNum == 1 ) {mask = 0x02;preg->regOff = 13;}
        if( preg->regNum == 2 ) {mask = 0x09;preg->regOff = 14;}

        outport(1,(pport->base+8));
        break;
    }

    if( preg->cfgOff < 0 ) return( asynSuccess );

    (preg->regDir == IN) ? pport->cfg |= mask : pport->cfg &= ~mask;
    outport(pport->cfg,(pport->base+preg->cfgOff));

    if( pport->isStart == FALSE && preg->regDir == IN )
    {
        char name[20];
        unsigned int prio;

        sprintf(name,"scanDio-%s",pport->name);
        epicsThreadLowestPriorityLevelAbove(epicsThreadPriorityScanHigh,&prio);
        epicsThreadCreate(name,prio,epicsThreadGetStackSize(epicsThreadStackSmall),(EPICSTHREADFUNC)scanDio,pport);

        pport->isStart = TRUE;
    } 

    return( asynSuccess );
}


static asynStatus gettype(void* ppvt,asynUser* pasynUser,const char** pptypeName,size_t* psize)
{
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvDio::gettype %s:\n",pport->name);

    if( pptypeName )
        *pptypeName = NULL;
    if( psize )
        *psize = 0;

    return( asynSuccess );
}


static asynStatus destroy(void* ppvt,asynUser* pasynUser)
{
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvDio::destroy %s:\n",pport->name);

    if( pport )
    {
        pasynUser->drvUser = NULL;
        return( asynSuccess );
    }
    else
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDio::destroy %s: called before create\n",pport->name);
        return( asynError );
    }

}


/****************************************************************************
 * Define private interface asynUInt32Digital methods
 ****************************************************************************/
static asynStatus writeUInt32(void* ppvt,asynUser* pasynUser,epicsUInt32 value,epicsUInt32 mask)
{
    int addr;
    Reg* preg;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvDio::writeUInt32 %s:\n",pport->name);

    if( pasynManager->getAddr(pasynUser,&addr) == asynSuccess )
        preg = &pport->reg[addr];
    else
        return( asynError );

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvDio::writeUInt32 %s: asyn - 0x%8.8X, addr - %d, mask - 0x%8.8X, value - 0x%8.8X\n",pport->name,(int)pasynUser,(int)preg->regNum,(int)mask,(int)value);

    epicsMutexMustLock(pport->syncLock);
    dioWrite(pport,preg,value,mask);
    epicsMutexUnlock(pport->syncLock);

    return( asynSuccess );
}


static asynStatus readUInt32(void* ppvt,asynUser* pasynUser,epicsUInt32* value,epicsUInt32 mask)
{
    int addr;
    Reg* preg;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvDio::readUInt32 %s:\n",pport->name);

    if( pasynManager->getAddr(pasynUser,&addr) == asynSuccess )
        preg = &pport->reg[addr];
    else
        return( asynError );

    epicsMutexMustLock(pport->syncLock);
    *value = dioRead(pport,preg);
    epicsMutexUnlock(pport->syncLock);

    *value &= mask;
    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvDio::readUInt32 %s: asyn - 0x%8.8X, addr - %d, mask - 0x%8.8X, value - 0x%8.8X\n",pport->name,(int)pasynUser,(int)preg->regNum,(int)mask,(int)*value);

    return( asynSuccess );
}


/****************************************************************************
 * Register public methods
 ****************************************************************************/

/* Initialization method definitions */
static const iocshArg drvDioInitArg0 = {"port",iocshArgString};
static const iocshArg drvDioInitArg1 = {"type",iocshArgInt};
static const iocshArg drvDioInitArg2 = {"inst",iocshArgInt};
static const iocshArg drvDioInitArg3 = {"addr",iocshArgInt};
static const iocshArg drvDioInitArg4 = {"regs",iocshArgInt};
static const iocshArg drvDioInitArg5 = {"rate",iocshArgInt};
static const iocshArg* drvDioInitArgs[]= {&drvDioInitArg0,&drvDioInitArg1,&drvDioInitArg2,&drvDioInitArg3,&drvDioInitArg4,&drvDioInitArg5};
static const iocshFuncDef drvDioInitFuncDef = {"drvDioInit",6,drvDioInitArgs};
static void drvDioInitCallFunc(const iocshArgBuf* args)
{
    drvDioInit(args[0].sval,args[1].ival,args[2].ival,args[3].ival,args[4].ival,args[5].ival);
}

/* Registration method */
static void drvDioRegister(void)
{
    static int firstTime = 1;

    if( firstTime )
    {
        firstTime = 0;
        iocshRegister( &drvDioInitFuncDef, drvDioInitCallFunc );
    }
}
epicsExportRegistrar( drvDioRegister );
