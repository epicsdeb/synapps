/*

                          Argonne National Laboratory
                            APS Operations Division
                     Beamline Controls and Data Acquisition

                          EPICS Brick DAC Port Driver



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
    initialize the driver, the method drvDacInit() is called from the
    startup script with the following calling sequence.

        drvDacInit(port,type,inst,addr,chns,pol,vlt)

        Where:
            port - Port driver name (i.e. "DAC" )
            type - Board type (see boardtypes.h)
            inst - Board instance
            addr - Board address
            chns - Register count
            pol  - Polarity (0=bipolar,1=unipolar)
            vlt  - Voltage range

    The method dbior can be called from the IOC shell to display the current
    status of the driver.


 Developer notes:
    1. Differential signals for RMM module means the following as
       specfied by the user (sector 21):
       - Enter 3.0V. chan 0 is set to -3.0V and chan 1 to +3.0V.
       - Enter -3.0V. chan 0 is set to +3.0V and chan 1 to -3.0V.
    2. User manual ICMS documents:
       Athena     - APS_1251097
       Poseidon   - APS_1251696
       RMM-MM-416 - APS_1251695

 Source control info:
    Modified by:    dkline
                    2008/02/12 12:19:33
                    1.12

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


/* Board definition related include files */
#include "boardtypes.h"


/* Define symbolic constants */
#define TRUE        (1)
#define FALSE       (0)


/* Forward struct declarations */
typedef struct Port Port;
typedef struct Channel Channel;


/* Declare channel info structure */
struct Channel
{
    int isConn;
    int isFirst;
    int stsOff;
    int chnOff;
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
    int           isConn;
    int           isBipolar;
    epicsFloat64  maxval;
    epicsFloat64  minval;
    epicsFloat64  maxres;
    epicsMutexId  syncLock;

    /* Asyn info */
    asynInterface asynCommon;
    asynInterface asynDrvUser;
    asynInterface asynFloat64;

    /* Channel info */
    int           cnt;
    Channel*      chn;
};


/* Public interface forward references */
static int drvDacInit(const char* port,int type,int inst,int addr,int chns,int pol,int vlt);


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


/* Forward references for utility methods */
static epicsFloat64 dacRead(Port* pport,Channel* preg);
static void dacWrite(Port* pport,Channel* preg,epicsFloat64 value);


/* Define local variants */
static Port* pports[] = {NULL,NULL,NULL,NULL,NULL,NULL};
static const int portCount=(sizeof(pports)/sizeof(Port*));


/* Define macros */
#define ISOK(s) (asynSuccess==(s))
#define ISNOTOK(s) (!ISOK(s))


/****************************************************************************
 * Define public interface methods
 ****************************************************************************/
static int drvDacInit(const char* port,int type,int inst,int addr,int chns,int pol,int vlt)
{
    Port* pport;
    int cmax,len,ns;
    asynFloat64* pasynFloat64;

    if( inst >= portCount )
    {
        printf("drvDac:init %s: max. number of instances exceeded %d\n",port,inst);
        return( -1 );
    }
    if( pports[inst] )
    {
        printf("drvDac:init %s: board instance %d already used for type %d\n",port,inst,pports[inst]->type);
        return( -1 );
    }
    if( pol < 0 || pol > 1 )
    {
        printf("drvDac:init %s: invalid polarization %d specified\n",port,pol);
        return( -1 );
    }
    if( vlt != 5 && vlt != 10 )
    {
        printf("drvDac:init %s: invalid voltage range %d specified\n",port,vlt);
        return( -1 );
    }

    ns    = strlen(port)+1;
    len   = sizeof(Port)+sizeof(asynFloat64)+ns+(sizeof(Channel)*chns);
    pport = (Port*)callocMustSucceed(len,sizeof(char),"drvDac");

    pasynFloat64 = (asynFloat64*)(pport + 1);
    pport->name  = (char*)(pasynFloat64 + 1);
    pport->chn   = (Channel*)(pport->name + ns);

    pport->type  = type;
    pport->inst  = inst;
    pport->cnt   = chns;

    pport->isConn    = FALSE;
    pport->isBipolar = (pol)?FALSE:TRUE;

    pport->maxval = (epicsFloat64)vlt;
    pport->minval = (epicsFloat64)(pport->isBipolar)?-vlt:0;

    strcpy(pport->name,port);
    pport->syncLock = epicsMutexMustCreate();

    switch( type )
    {
    case DSC_ATHENA:
    case DSC_POSEIDON:
        cmax = 4;
        pport->base = addr;
        pport->maxres = (pport->isBipolar?vlt*2:vlt)/4096.0*1000.0;
        break;

    case DSC_RMMSIG:
    case DSC_RMMDIF:
        cmax = 4;
        pport->base = addr;
        pport->maxres = 1.0/65536.0*1000.0;
        break;

    default:
        printf("drvDac::init %s: unknown board type %d\n",port,type);
        free(pport);
        return( - 1 );
    }

    if( chns > cmax )
    {
        printf("drvDac::init %s: board type %d cannot have > %d channels\n",port,type,cmax);
        free(pport);
        return( - 1 );
    }

    if( pasynManager->registerPort(port,ASYN_MULTIDEVICE|ASYN_CANBLOCK,1,0,0) )
    {
        printf("drvDac::init %s: failure to register DSC port\n",port);
        free(pport);
        return( -1 );
    }

    pport->asynCommon.interfaceType = asynCommonType;
    pport->asynCommon.pinterface = &common;
    pport->asynCommon.drvPvt = pport;

    if( pasynManager->registerInterface(port,&pport->asynCommon) )
    {
        printf("drvDac::init %s: failure to register asynCommon\n",port);
        return( -1 );
    }

    pport->asynDrvUser.interfaceType = asynDrvUserType;
    pport->asynDrvUser.pinterface = &drvuser;
    pport->asynDrvUser.drvPvt = pport;

    if( pasynManager->registerInterface(port,&pport->asynDrvUser) )
    {
        printf("drvDac::init %s: failure to register asynDrvUser\n",port);
        return( -1 );
    }

    pasynFloat64->read = readFloat64;
    pasynFloat64->write = writeFloat64;
    pport->asynFloat64.interfaceType = asynFloat64Type;
    pport->asynFloat64.pinterface = pasynFloat64;
    pport->asynFloat64.drvPvt = pport;

    if( pasynFloat64Base->initialize(port,&pport->asynFloat64) )
    {
        printf("drvDac::init %s: failure to initialize asynFloat64Base\n",port);
        return( -1 );
    }

    pports[pport->inst] = pport;
    return( 0 );
}


/****************************************************************************
 * Define private interface utility methods
 ****************************************************************************/
static epicsFloat64 dacRead(Port* pport,Channel* pchn)
{
    return( pchn->value );
}


static void dacWrite(Port* pport,Channel* pchn,epicsFloat64 value)
{
    int val=0;
    unsigned char msb=0,lsb=0,cmd=0;

    pchn->value = value;

    switch( pport->type )
    {
    case DSC_ATHENA:
        value = value/pport->maxval;
        val   = (int)(pport->isBipolar?value*2048.0+2048.5:value*4096.0);
        if( val>4095 ) val=4095;

        lsb = val & 0xFF;
        msb = (pchn->chnNum<<6)|((val>>8)&0xF);

        while( inport(pport->base+pchn->stsOff)&0x10 );
        outport(lsb,pport->base+pchn->chnOff);
        outport(msb,pport->base+pchn->chnOff+1);
        break;

    case DSC_POSEIDON:
        value = value/pport->maxval;
        val   = (int)(pport->isBipolar?value*2048.0+2048.5:value*4096.0);
        if( val>4095 ) val=4095;

        lsb = val & 0xFF;
        msb = (pchn->chnNum<<6)|((val>>8)&0xF);

        while( inport(pport->base+pchn->stsOff)&0x80 );
        outport(lsb,pport->base+pchn->chnOff);
        outport(msb,pport->base+pchn->chnOff+1);
        break;

    case DSC_RMMSIG:
        val = (int)((pport->isBipolar?value/pport->maxval:(value-5)*0.2)*32768.0);
        if( val>32767 ) val=32767;

        lsb = val&0xFF;
        msb = (val>>8)&0xFF;

        outport(lsb,pport->base+pchn->chnOff);
        outport(msb,pport->base+pchn->chnOff+1);
        cmd = inport(pport->base);
        break;

    case DSC_RMMDIF:
        value *= -1;
        val   = (int)((pport->isBipolar?value/pport->maxval:(value-5)*0.2)*32768.0);
        if( val>32767 ) val=32767;

        lsb = val&0xFF;
        msb = (val>>8)&0xFF;

        outport(lsb,pport->base+pchn->chnOff);
        outport(msb,pport->base+pchn->chnOff+1);

        value *= -1;
        val   = (int)((pport->isBipolar?value/pport->maxval:(value-5)*0.2)*32768.0);
        if( val>32767 ) val=32767;

        lsb = val&0xFF;
        msb = (val>>8)&0xFF;

        outport(lsb,pport->base+pchn->chnOff+2);
        outport(msb,pport->base+pchn->chnOff+3);

        cmd = inport(pport->base);
        break;

    }

}


/****************************************************************************
 * Define private interface asynCommon methods
 ****************************************************************************/
static void report(void* ppvt,FILE* fp,int details)
{
    Port* pport = (Port*)ppvt;

    fprintf(fp,"    EBRICK analog output @ 0x%4.4X is type %d\n",pport->base,pport->type);
    fprintf(fp,"    Has %d %s channels with min/max %.2fV/%.2fV range and %.2fmV resolution\n",pport->cnt,(pport->isBipolar)?"bipolar":"unipolar",pport->minval,pport->maxval,pport->maxres);
    for( int i=0; i<pport->cnt;++i ) fprintf(fp,"        chan %i connected %s\n",i,(pport->chn[i].isConn==TRUE)?"Yes":"No");

}


static asynStatus connect(void* ppvt,asynUser* pasynUser)
{
    int addr;
    asynStatus sts;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvDac::connect %s:\n",pport->name);

    if( (sts = pasynManager->getAddr(pasynUser,&addr)) )
        return( sts );

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvDac::connect %s: asyn - 0x%8.8X, addr - %d\n",pport->name,(int)pasynUser,(int)addr);

    if( addr > pport->cnt )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDac::connect %s: illegal addr %d\n",pport->name,addr);
        return( asynError );
    }

    if( addr >= 0 )
    {
        if( pport->chn[addr].isConn == TRUE )
        {
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDac::connect %s: already connected to %d\n",pport->name,addr);
            return( asynError );
        }

        pport->chn[addr].isConn = TRUE;
        pasynManager->exceptionConnect(pasynUser);
        return( asynSuccess );
    }

    if( pport->isConn )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDac::connect %s: device %d already connected\n",pport->name,addr);
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

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvDac::disconnect %s:\n",pport->name);

    if( (sts = pasynManager->getAddr(pasynUser,&addr)) )
        return( sts );

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvDac::disconnect %s: asyn - 0x%8.8X, addr - %d\n",pport->name,(int)pasynUser,(int)addr);

    if( addr > pport->cnt )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDac::disconnect %s: illegal addr %d\n",pport->name,addr);
        return( asynError );
    }

    if( addr >= 0 )
    {
        if( pport->chn[addr].isConn == FALSE )
        {
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDac::disconnect %s: not connected to %d\n",pport->name,addr);
            return( asynError );
        }

        pport->chn[addr].isConn = FALSE;
        pasynManager->exceptionDisconnect(pasynUser);
        return( asynSuccess );
    }

    if( pport->isConn == FALSE )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDac::disconnect %s: not connected to addr %d\n",pport->name,addr);
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

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvDac::create %s:\n",pport->name);

    if( pasynManager->getAddr(pasynUser,&addr) == asynSuccess )
    {
        if( addr < pport->cnt )
            pchn = &pport->chn[addr];
        else
        {
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDac::create %s: invalid register number %d\n",pport->name,addr);
            return( asynError );
        }
    }
    else
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDac::create %s: getAddr failure\n",pport->name);
        return( asynError );
    }

    if( epicsStrCaseCmp("Output",drvInfo) )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDac::create %s: failure to determine register %d direction\n",pport->name,addr);
        return( asynError );
    }

    pchn->pport   = pport;
    pchn->chnNum  = addr;
    pchn->isFirst = FALSE;
    pasynUser->drvUser = pchn;

    switch( pport->type )
    {
    case DSC_ATHENA:
        if( pchn->chnNum > 3 )
        {
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDac::create %s: invalid channel number %d\n",pport->name,pchn->chnNum);
            return( asynError );
        }

        pchn->stsOff = 3;
        pchn->chnOff = 6;
        break;

    case DSC_RMMSIG:
        if( pchn->chnNum > 3 )
        {
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDac::create %s: invalid register number %d\n",pport->name,pchn->chnNum);
            return( asynError );
        }

        pchn->stsOff = -1;
        if( pchn->chnNum == 0 ) pchn->chnOff = 0;
        if( pchn->chnNum == 1 ) pchn->chnOff = 2;
        if( pchn->chnNum == 2 ) pchn->chnOff = 4;
        if( pchn->chnNum == 3 ) pchn->chnOff = 6;
        break;

    case DSC_RMMDIF:
        if( pchn->chnNum != 0 && pchn->chnNum != 2 )
        {
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDac::create %s: invalid register number %d\n",pport->name,pchn->chnNum);
            return( asynError );
        }

        pchn->stsOff = -1;
        if( pchn->chnNum == 0 ) pchn->chnOff = 0;
        if( pchn->chnNum == 2 ) pchn->chnOff = 4;
        break;

    case DSC_POSEIDON:
        if( pchn->chnNum > 3 )
        {
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDac::create %s: invalid channel number %d\n",pport->name,pchn->chnNum);
            return( asynError );
        }

        pchn->stsOff = 4;
        pchn->chnOff = 4;
        break;
    }

    return( asynSuccess );
}


static asynStatus gettype(void* ppvt,asynUser* pasynUser,const char** pptypeName,size_t* psize)
{
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvDac::gettype %s:\n",pport->name);

    if( pptypeName )
        *pptypeName = NULL;
    if( psize )
        *psize = 0;

    return( asynSuccess );
}


static asynStatus destroy(void* ppvt,asynUser* pasynUser)
{
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvDac::destroy %s:\n",pport->name);

    if( pport == NULL )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvDac::destroy %s: called before create\n",pport->name);
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

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvDac::writeFloat64 %s: asyn - 0x%8.8X, addr - %d, value - %.3f\n",pport->name,(int)pasynUser,addr,value);

    if( value < pport->minval || value > pport->maxval ) return( asynError );

    if( pasynManager->getAddr(pasynUser,&addr) == asynSuccess )
        pchn = &pport->chn[addr];
    else
        return( asynError );

    epicsMutexMustLock(pport->syncLock);
    dacWrite(pport,pchn,value);
    epicsMutexUnlock(pport->syncLock);

    return( asynSuccess );
}


static asynStatus readFloat64(void* ppvt,asynUser* pasynUser,epicsFloat64* value)
{
    int addr;
    Channel* pchn;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvDac::readFloat64 %s:\n",pport->name);

    if( pasynManager->getAddr(pasynUser,&addr) == asynSuccess )
        pchn = &pport->chn[addr];
    else
        return( asynError );

    if( pchn->isFirst == FALSE ) {pchn->isFirst=TRUE;return( asynError );}

    epicsMutexMustLock(pport->syncLock);
    *value = dacRead(pport,pchn);
    epicsMutexUnlock(pport->syncLock);

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvDac::readFloat64 %s: asyn - 0x%8.8X, addr - %d, mask - 0x%8.8X, value - %.3f\n",pport->name,(int)pasynUser,(int)pchn->chnNum,*value);

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
static const iocshArg arg6 = {"vlt",iocshArgInt};
static const iocshArg* args[]= {&arg0,&arg1,&arg2,&arg3,&arg4,&arg5,&arg6};
static const iocshFuncDef drvDacInitFuncDef = {"drvDacInit",7,args};
static void drvDacInitCallFunc(const iocshArgBuf* args)
{
    drvDacInit(args[0].sval,args[1].ival,args[2].ival,args[3].ival,args[4].ival,args[5].ival,args[6].ival);
}

/* Registration method */
static void drvDacRegister(void)
{
    static int firstTime = 1;

    if( firstTime )
    {
        firstTime = 0;
        iocshRegister( &drvDacInitFuncDef,drvDacInitCallFunc );
    }
}
epicsExportRegistrar( drvDacRegister );
