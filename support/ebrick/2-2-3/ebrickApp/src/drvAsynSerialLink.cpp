/*

                          Argonne National Laboratory
                            APS Operations Division
                     Beamline Controls and Data Acquisition

                       Serial-Link Protocol Port Driver



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
    This module implements an Asyn-based multiple device port driver to provide
    support for the Serial-Link Protocol. To initialize the driver. The method
    drvAsynSerialLinkInit() is called from the startup script with the following
    sequence.

        drvAsynSerialLinkInit(myport,ioport,ioaddr,inst,datoff,clkoff,enaoff,clear)

        Where:
            myport - Asyn port name (i.e. "DIO")
            ioport - Asyn IO port name (i.e. "Unidig")
            ioaddr - Asyn IO port address
            inst   - Driver instance
            datoff - Data chan/bit number (start/w 0)
            clkoff - Clock chan/bit number (start/w 0)
            enaoff - Enable chan/bit number (start/w 0)
            clear  - Clear cahns/bits on startup


    The method dbior can be called from the IOC shell to display the current
    status of the driver.


 Developer notes:
    1. Original protocol concept was developed by Dr.Steve Ross.
    2. Serial-Link Protocol V2.1 format:

       +--------------------------------------+
       |63               32|31               0|
       +--------------------------------------+
       |  User (optional)  |  Cntl (required) |
       +--------------------------------------+

    3. Asyn addresses:
       0 - Write/Read control data.
       1 - Write/Read user data
       ** Writing control data initiates serial-link transfer.


 Source control info:
    Modified by:    dkline
                    2008/03/18 15:41:42
                    1.3

 =============================================================================
 History:
 Author: David M. Kline
 -----------------------------------------------------------------------------
 2008-Feb-22  DMK  Taken from EBRICK drvDio implementation.
 2008-Feb-25  DMK  Initial implementation complete.
 2008-Mar-14  DMK  Modified timing in serialLink().
 -----------------------------------------------------------------------------

*/


/* System related include files */
#include <string.h>


/* EPICS system related include files */
#include <iocsh.h>
#include <epicsStdio.h>
#include <cantProceed.h>
#include <epicsString.h>
#include <epicsExport.h>
#include <epicsThread.h>


/* EPICS related include files */
#include <epicsVersion.h>
#include <asynDriver.h>
#include <asynDrvUser.h>
#include <asynUInt32Digital.h>


/* Define symbolic constants */
#ifndef TRUE
#define TRUE    (1)
#endif
#ifndef FALSE
#define FALSE   (0)
#endif


/* Define serial link symbolic constants */
#define MAJVER      (2)
#define MINVER      (1)
#define VERSION     ((MAJVER<<4)|MINVER)

#define VER_BIT     (8)
#define CTL_BIT     (24)
#define DAT_BIT     (32)
#define DAT_SIZ     (DAT_BIT)
#define SLD_SIZ     (VER_BIT+CTL_BIT)


/* Forward struct declarations */
typedef struct Iop Iop;
typedef struct Conn Conn;
typedef struct Port Port;


/* Declare port structures */
struct Iop
{
    Port*              pport;
    char*              name;
    int                conn;
    int                addr;

    asynUser*          pasynUser;
    asynUInt32Digital* pasynUInt32;
    void*              pasynUInt32Pvt;
};
struct Conn
{
    int conn;
    int addr;
    int refs;
};
struct Port
{
    char*         name;
    int           inst;
    int           cntl;
    int           data;
    int           send;
    epicsMutexId  syncLock;

    int           mask;
    int           datoff;
    int           clkoff;
    int           enaoff;
    int           datmsk;
    int           clkmsk;
    int           enamsk;

    asynInterface asynUInt32;
    asynInterface asynCommon;
    asynInterface asynDrvUser;

    int           conn;
    int           addr;
    int           refs;
    Conn          conns[2];

    Iop           iop;
};


/* Public interface forward references */
int drvAsynSerialLinkInit(const char* myport,const char* ioport,int ioaddr,int inst,int datoff,int clkoff,int enaoff,int clear);


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
static int ioportInit(Iop* piop,const char* ioname,int ioaddr,int clear);
static void serialLink(Port* pport,Iop* piop,int ver);


/* Define local variants */
static Port* pports[] = {NULL,NULL,NULL,NULL,NULL,NULL};
static const int portCount=(sizeof(pports)/sizeof(Port*));


/****************************************************************************
 * Define public interface methods
 ****************************************************************************/
int drvAsynSerialLinkInit(const char* myport,const char* ioport,int ioaddr,int inst,int datoff,int clkoff,int enaoff,int clear)
{
    int len;
    Port* pport;
    asynUInt32Digital* pasynUInt32;

    if( inst >= portCount )
    {
        printf("drvAsynSerialLink:init %s: max. number of instances exceeded %d\n",myport,inst);
        return( -1 );
    }
    if( pports[inst] )
    {
        printf("drvAsynSerialLink:init %s: board instance %d already used\n",myport,inst);
        return( -1 );
    }
    if( (datoff==clkoff)||(datoff==enaoff)||(clkoff==enaoff) )
    {
        printf("drvAsynSerialLink:init %s: invalid bit offsets specified |D|C|E| %d,%d,%d\n",myport,datoff,clkoff,enaoff);
        return( -1 );
    }

    len = sizeof(Iop);
    len += sizeof(Port);
    len += strlen(myport)+1;
    len += sizeof(asynUInt32Digital);
    pport = (Port*)callocMustSucceed(len,sizeof(char),"drvAsynSerialLinkInit");

    pasynUInt32 = (asynUInt32Digital*)(pport + 1);
    pport->name = (char*)(pasynUInt32 + 1);
    strcpy(pport->name,myport);

    pport->inst      = inst;
    pport->iop.pport = pport;
    pport->syncLock  = epicsMutexMustCreate();

    pport->datoff = datoff;
    pport->clkoff = clkoff;
    pport->enaoff = enaoff;
    pport->datmsk = 1<<datoff;
    pport->clkmsk = 1<<clkoff;
    pport->enamsk = 1<<enaoff;
    pport->mask   = pport->datmsk|pport->clkmsk|pport->enamsk;

    if( pasynManager->registerPort(myport,ASYN_MULTIDEVICE|ASYN_CANBLOCK,1,0,0) )
    {
        printf("drvAsynSerialLink::init %s: failure to register port\n",myport);
        free(pport);
        return( -1 );
    }

    pport->asynCommon.interfaceType = asynCommonType;
    pport->asynCommon.pinterface = &common;
    pport->asynCommon.drvPvt = pport;

    if( pasynManager->registerInterface(myport,&pport->asynCommon) )
    {
        printf("drvAsynSerialLink::init %s: failure to register asynCommon\n",myport);
        free(pport);
        return( -1 );
    }

    pport->asynDrvUser.interfaceType = asynDrvUserType;
    pport->asynDrvUser.pinterface = &drvuser;
    pport->asynDrvUser.drvPvt = pport;

    if( pasynManager->registerInterface(myport,&pport->asynDrvUser) )
    {
        printf("drvAsynSerialLink::init %s: failure to register asynDrvUser\n",myport);
        free(pport);
        return( -1 );
    }

    pasynUInt32->read = readUInt32;
    pasynUInt32->write = writeUInt32;
    pport->asynUInt32.interfaceType = asynUInt32DigitalType;
    pport->asynUInt32.pinterface = pasynUInt32;
    pport->asynUInt32.drvPvt = pport;

    if( pasynUInt32DigitalBase->initialize(myport,&pport->asynUInt32) )
    {
        printf("drvAsynSerialLink::init %s: failure to initialize asynUInt32DigitalBase\n",myport);
        free(pport);
        return( -1 );
    }

    if( ioportInit(&pport->iop,ioport,ioaddr,clear) )
    {
        printf("drvAsynSerialLink::init %s: failure to initialize ioport %s\n",myport,ioport);
        free(pport);
        return( -1 );
    }

    pports[pport->inst] = pport;
    return( 0 );
}


static int ioportInit(Iop* piop,const char* ioport,int ioaddr,int clear)
{
    asynUser *pasynUser;
    asynInterface* pasynIface;

    piop->name = (char*)callocMustSucceed(strlen(ioport)+1,sizeof(char),"ioportInit");

    if( (pasynUser=pasynManager->createAsynUser(NULL,NULL))==NULL )
    {
        printf("ioportInit::failure to create asynUser for %s\n",ioport);
        free(piop->name);
        return( -1 );
    }

    if( pasynManager->connectDevice(pasynUser,ioport,ioaddr) )
    {
        printf("ioportInit::failure to connect with device %s @ %d\n",ioport,ioaddr);
        pasynManager->freeAsynUser(pasynUser);
        free(piop->name);

        return( -1 );
    }

    if( (pasynIface=pasynManager->findInterface(pasynUser,asynUInt32DigitalType,1))==NULL )
    {
        printf("initIoport::failure to find interface %s\n",asynUInt32DigitalType);
        pasynManager->disconnect(pasynUser);
        pasynManager->freeAsynUser(pasynUser);
        free(piop->name);

        return( -1 );
    }

    pasynUser->userPvt = piop->pport;
    pasynUser->timeout = 1.0;

    piop->conn = TRUE;
    piop->addr = ioaddr;
    strcpy(piop->name,ioport);

    piop->pasynUser = pasynUser;
    piop->pasynUInt32 = (asynUInt32Digital*)pasynIface->pinterface;
    piop->pasynUInt32Pvt = pasynIface->drvPvt;

    if( clear ) piop->pasynUInt32->write(piop->pasynUInt32Pvt,piop->pasynUser,0,piop->pport->mask);

    return( 0 );
}


static void serialLink(Port* pport,Iop* piop,int ver)
{
    int i,dat;
    unsigned int sld,msk,mask = pport->mask;

    // Disable / Enable serial-link
    piop->pasynUInt32->write(piop->pasynUInt32Pvt,piop->pasynUser,0,mask);
    piop->pasynUInt32->write(piop->pasynUInt32Pvt,piop->pasynUser,pport->enamsk,mask);

    // Send data
    if( pport->send )
    {
        msk = 1 << (DAT_SIZ-1);
        sld = pport->data;

        for(i=0; i<DAT_SIZ; ++i)
        {
            dat = pport->enamsk|(((msk & sld)?1:0)<<pport->datoff);
            piop->pasynUInt32->write(piop->pasynUInt32Pvt,piop->pasynUser,dat,mask);

            dat |= pport->clkmsk;
            piop->pasynUInt32->write(piop->pasynUInt32Pvt,piop->pasynUser,dat,mask);

            dat &= ~pport->clkmsk;
            piop->pasynUInt32->write(piop->pasynUInt32Pvt,piop->pasynUser,dat,mask);

            sld <<= 1;
        }
    }

    // Send control
    msk = 1<<(SLD_SIZ-1);
    sld = (pport->cntl<<VER_BIT)|ver;

    for(i=0; i<SLD_SIZ; ++i)
    {
        dat = pport->enamsk|(((msk & sld)?1:0)<<pport->datoff);
        piop->pasynUInt32->write(piop->pasynUInt32Pvt,piop->pasynUser,dat,mask);

        dat |= pport->clkmsk;
        piop->pasynUInt32->write(piop->pasynUInt32Pvt,piop->pasynUser,dat,mask);

        dat &= ~pport->clkmsk;
        piop->pasynUInt32->write(piop->pasynUInt32Pvt,piop->pasynUser,dat,mask);

        sld <<= 1;
    }

    // Disable serial-link
    piop->pasynUInt32->write(piop->pasynUInt32Pvt,piop->pasynUser,0,mask);
}


/****************************************************************************
 * Define private interface asynCommon methods
 ****************************************************************************/
static void report(void* ppvt,FILE* fp,int details)
{
    Port* pport = (Port*)ppvt;

    fprintf(fp,"drvAsynSerialLink %s: Serial-Link protocol V%1d.%1d\n",pport->name,MAJVER,MINVER);
    fprintf(fp,"    %s\n",EPICS_VERSION_STRING);
    fprintf(fp,"    |D|C|E| bits %d,%d,%d mask 0x%8.8X\n",pport->datoff,pport->clkoff,pport->enaoff,pport->mask);
    fprintf(fp,"    pport  connected %s addr %d refs %d\n",(pport->conn==TRUE)?"Yes":"No",pport->addr,pport->refs);
    fprintf(fp,"    conn0  connected %s addr %d refs %d\n",(pport->conns[0].conn==TRUE)?"Yes":"No",pport->conns[0].addr,pport->conns[0].refs);
    fprintf(fp,"    conn1  connected %s addr %d refs %d\n",(pport->conns[1].conn==TRUE)?"Yes":"No",pport->conns[1].addr,pport->conns[1].refs);
    fprintf(fp,"    ioport %s connected %s addr %d\n",pport->iop.name,(pport->iop.conn==TRUE)?"Yes":"No",pport->iop.addr);
}


static asynStatus connect(void* ppvt,asynUser* pasynUser)
{
    int addr;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAsynSerialLink::connect %s:\n",pport->name);

    if( pasynManager->getAddr(pasynUser,&addr) ) return( asynError );

    if( (addr==1)||(addr==0) )
    {
        ++pport->conns[addr].refs;
        pport->conns[addr].conn = TRUE;
        pport->conns[addr].addr = addr;
    }
    else if( addr==-1 )
    {
        ++pport->refs;
        pport->conn = TRUE;
        pport->addr = addr;
    }
    else
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvAsynSerialLink::connect %s: illegal addr %d\n",pport->name,addr);
        return( asynError );
    }

    pasynManager->exceptionConnect(pasynUser);
    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvAsynSerialLink::connect %s: asyn - 0x%8.8X, addr - %d\n",pport->name,(int)pasynUser,(int)addr);

    return( asynSuccess );
}


static asynStatus disconnect(void* ppvt,asynUser* pasynUser)
{
    int addr;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAsynSerialLink::disconnect %s:\n",pport->name);

    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );

    if( (addr==1)||(addr==0) )
    {
        --pport->conns[addr].refs;
        pport->conns[addr].conn = FALSE;
        pport->conns[addr].addr = -2;
    }
    else if( addr==-1 )
    {
        --pport->refs;
        pport->conn = FALSE;
        pport->addr = -2;
    }
    else
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvAsynSerialLink::connect %s: illegal addr %d\n",pport->name,addr);
        return( asynError );
    }

    pasynManager->exceptionDisconnect(pasynUser);
    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvAsynSerialLink::disconnect %s: asyn - 0x%8.8X, addr - %d\n",pport->name,(int)pasynUser,(int)addr);

    return( asynSuccess );
}


/****************************************************************************
 * Define private interface asynDrvUser methods
 ****************************************************************************/
static asynStatus create(void* ppvt,asynUser* pasynUser,const char* drvInfo, const char** pptypeName,size_t* psize)
{
    int addr;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAsynSerialLink::create %s:\n",pport->name);

    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );
    if( (addr<-1)||(addr>2) ) return( asynError );
    pasynUser->drvUser = &pport->iop;

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvAsynSerialLink::create %s: asyn - 0x%8.8X, addr - %d\n",pport->name,(int)pasynUser,(int)addr);

    return( asynSuccess );
}


static asynStatus gettype(void* ppvt,asynUser* pasynUser,const char** pptypeName,size_t* psize)
{
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAsynSerialLink::gettype %s:\n",pport->name);

    if( pptypeName )
        *pptypeName = NULL;
    if( psize )
        *psize = 0;

    return( asynSuccess );
}


static asynStatus destroy(void* ppvt,asynUser* pasynUser)
{
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAsynSerialLink::destroy %s:\n",pport->name);

    if( pport )
    {
        pasynUser->drvUser = NULL;
        return( asynSuccess );
    }
    else
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvAsynSerialLink::destroy %s: called before create\n",pport->name);
        return( asynError );
    }

}


/****************************************************************************
 * Define private interface asynUInt32Digital methods
 ****************************************************************************/
static asynStatus writeUInt32(void* ppvt,asynUser* pasynUser,epicsUInt32 value,epicsUInt32 mask)
{
    int addr;
    Iop* piop;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAsynSerialLink::writeUInt32 %s:\n",pport->name);

    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );
    piop = &pport->iop;

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvAsynSerialLink::writeUInt32 %s: asyn - 0x%8.8X, addr - %d, mask - 0x%8.8X, value - 0x%8.8X\n",pport->name,(int)pasynUser,(int)addr,(int)mask,(int)value);

    epicsMutexMustLock(pport->syncLock);
    if( addr==0 )
    {
        pport->cntl = value;
        serialLink(pport,piop,VERSION);
        pport->send = FALSE;
    }
    else if( addr==1 )
    {
        pport->data = value;
        pport->send = TRUE;
    }
    epicsMutexUnlock(pport->syncLock);

    return( asynSuccess );
}


static asynStatus readUInt32(void* ppvt,asynUser* pasynUser,epicsUInt32* value,epicsUInt32 mask)
{
    int addr;
    Iop* piop;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAsynSerialLink::readUInt32 %s:\n",pport->name);

    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );
    piop = &pport->iop;

    epicsMutexMustLock(pport->syncLock);
    if( addr==0 )
        *value = pport->cntl;
    else if( addr==1 )
        *value = pport->data;
    epicsMutexUnlock(pport->syncLock);

    *value &= mask;
    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvAsynSerialLink::readUInt32 %s: asyn - 0x%8.8X, addr - %d, mask - 0x%8.8X, value - 0x%8.8X\n",pport->name,(int)pasynUser,(int)addr,(int)mask,(int)*value);

    return( asynSuccess );
}


/****************************************************************************
 * Register public methods
 ****************************************************************************/

/* Initialization method definitions */
static const iocshArg arg0 = {"myport",iocshArgString};
static const iocshArg arg1 = {"ioport",iocshArgString};
static const iocshArg arg2 = {"ioaddr",iocshArgInt};
static const iocshArg arg3 = {"inst",iocshArgInt};
static const iocshArg arg4 = {"datoff",iocshArgInt};
static const iocshArg arg5 = {"clkoff",iocshArgInt};
static const iocshArg arg6 = {"enaoff",iocshArgInt};
static const iocshArg arg7 = {"clear",iocshArgInt};
static const iocshArg* args[]= {&arg0,&arg1,&arg2,&arg3,&arg4,&arg5,&arg6,&arg7};
static const iocshFuncDef drvAsynSerialLinkInitFuncDef = {"drvAsynSerialLinkInit",8,args};
static void drvAsynSerialLinkInitCallFunc(const iocshArgBuf* args)
{
    drvAsynSerialLinkInit(args[0].sval,args[1].sval,args[2].ival,args[3].ival,args[4].ival,args[5].ival,args[6].ival,args[7].ival);
}

/* Registration method */
static void drvAsynSerialLinkRegistrar(void)
{
    static int firstTime = 1;

    if( firstTime )
    {
        firstTime = 0;
        iocshRegister( &drvAsynSerialLinkInitFuncDef,drvAsynSerialLinkInitCallFunc );
    }
}
epicsExportRegistrar( drvAsynSerialLinkRegistrar );
