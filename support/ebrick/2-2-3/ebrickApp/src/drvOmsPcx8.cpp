/*

                          Argonne National Laboratory
                            APS Operations Division
                     Beamline Controls and Data Acquisition

                          EPICS Brick OMS Port Driver



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
    This module provides support for the PC104-based OMS motor controller
    modules PC68/PC78. To initialize the driver, the method drvOmsPcx8Init()
    is called from the startup script with the following calling sequence.

        drvOmsPcx8Init(port,device)

        Where:
            port - Port driver name (i.e. "OMS0")
            devf - Device file (i.e. "/dev/oms0")

    The method dbior can be called from the IOC shell to display the current
    status of the driver.


 Developer notes:
    1. ICMS documents:
       PC78 User's Manual - APS_1251701
       PC78 Summary Guide - APS_1251702
    2. OMS device driver ioctl() function 7 is 'wait on interrupt'


 Source control info:
    Modified by:    dkline
                    2009/03/03 20:27:21
                    1.12

 =============================================================================
 History:
 Author: David M. Kline
 -----------------------------------------------------------------------------
 2006-Sep-28  DMK  Derived from drvDio multidevice driver.
 2008-Feb-12  DMK  Remove dependency on the oms.h include file.
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
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <osiUnistd.h>
#include <sys/ioctl.h>

/* EPICS system related include files */
#include <iocsh.h>
#include <epicsStdio.h>
#include <cantProceed.h>
#include <epicsString.h>
#include <epicsExport.h>
#include <epicsThread.h>


/* EPICS synApps/Asyn related include files */
#include <asynDriver.h>
#include <asynOctet.h>


/* Define symbolic constants */
#define TRUE        (1)
#define FALSE       (0)
#define K_COMTMO    (1)


/* Forward struct declarations */
typedef struct Port Port;


/* Declare port structure */
struct Port
{
    char*         port;
    char*         devf;
    int           fd;
    int           isConn;
    epicsMutexId  syncLock;
    asynUser*     pasynUser;
    asynInterface asynCommon;
    asynInterface asynOctet;
    void*         asynOctetCallbackPvt;
};


/* Private methods forward references */
static void omsIsr(Port* pport);


/* Public interface forward references */
static int drvOmsPcx8Init(const char* port,const char* devf);


/* Forward references for asynCommon methods */
static void report(void* ppvt,FILE* fp,int details);
static asynStatus connect(void* ppvt,asynUser* pasynUser);
static asynStatus disconnect(void* ppvt,asynUser* pasynUser);
static asynCommon common = {report,connect,disconnect};


/* Forward references for asynOctet methods */
static asynStatus flushIt(void* ppvt,asynUser* pasynUser);
static asynStatus writeItRaw(void* ppvt,asynUser* pasynUser,const char *data,size_t numchars,size_t* nbytes);
static asynStatus readItRaw(void* ppvt,asynUser* pasynUser,char* data,size_t maxchars,size_t *nbytes,int *eom);


/* Define macros */
#define ISOK(s) (asynSuccess==(s))
#define ISNOTOK(s) (!ISOK(s))


/****************************************************************************
 * Define public interface methods
 ****************************************************************************/
static int drvOmsPcx8Init(const char* port,const char* devf)
{
    int len;
    uint prio;
    Port* pport;
    asynUser* pasynUser;
    asynOctet* pasynOctet;

    len = sizeof(Port)+sizeof(asynOctet)+strlen(port)+strlen(devf)+2;
    pport = (Port*)callocMustSucceed(len,sizeof(char),"drvOmsPcx8Init");

    pasynOctet = (asynOctet*)(pport + 1);
    pport->port = (char*)(pasynOctet + 1);
    pport->devf = (char*)(pport->port + strlen(port) + 1);
    pport->fd = -1;
    pport->isConn = FALSE;
    strcpy(pport->port,port);
    strcpy(pport->devf,devf);
    pport->syncLock = epicsMutexMustCreate();

    if( pasynManager->registerPort(port,ASYN_CANBLOCK,1,0,0) )
    {
        printf("drvOmsPcx8Init::init %s: failure to register port\n",port);
        free(pport);
        return( -1 );
    }

    pport->asynCommon.interfaceType = asynCommonType;
    pport->asynCommon.pinterface = &common;
    pport->asynCommon.drvPvt = pport;

    if( pasynManager->registerInterface(port,&pport->asynCommon) )
    {
        printf("drvOmsPcx8Init::init %s: failure to register asynCommon\n",port);
        free(pport);
        return( -1 );
    }

    pasynOctet->flush = flushIt;
    pasynOctet->read = readItRaw;
    pasynOctet->write = writeItRaw;
    pport->asynOctet.drvPvt = pport;
    pport->asynOctet.pinterface = pasynOctet;
    pport->asynOctet.interfaceType = asynOctetType;

    if( pasynOctetBase->initialize(port,&pport->asynOctet,0,0,0) )
    {
        printf("drvOmsPcx8Init::init %s: failure to initialize asynOctetBase\n",port);
        free(pport);
        return( -1 );
    }

    if( pasynManager->registerInterruptSource(pport->port,&pport->asynOctet,&pport->asynOctetCallbackPvt) )
    {
        printf("drvOmsPcx8Init::init %s: failure to registerInterrupSouce\n",port);
        free(pport);
        return( -1 );
    }

    if( (pasynUser = pasynManager->createAsynUser(0,0)) )
    {
        pport->pasynUser = pasynUser;
        pasynUser->userPvt = pport;
        pasynUser->timeout = K_COMTMO;
    }
    else
    {
        printf("drvOmsPcx8Init::create asynUser failure\n");
        free(pport);
        return( -1 );
    }

    if( pasynManager->connectDevice(pasynUser,port,-1) )
    {
        printf("drvOmsPcx8Init::failure to connect with device %s\n",port);
        pport->isConn = 0;
        free(pport);
        return( -1 );
    }

    epicsThreadLowestPriorityLevelAbove(epicsThreadPriorityHigh,&prio);
    epicsThreadCreate("omsIsr",prio,epicsThreadGetStackSize(epicsThreadStackSmall),(EPICSTHREADFUNC)omsIsr,pport);

    return( 0 );
}


static void omsIsr(Port* pport)
{
    int fd,len,stat;
    ELLLIST* plist;
    interruptNode* pnode;
    asynOctetInterrupt* pintr;

    fd = open(pport->devf,O_RDONLY,0);
    if( fd < 0 )
    {
        printf("omsIsr() - \"%s\" - failed to open - suspending\n",pport->devf);
        epicsThreadSuspendSelf();
    }

    while(0==0)
    {
        len = ioctl(fd,7,&stat);

//        printf("omsIsr() - \"%s\" - %s interrupt received - 0x%8.8X - notify clients\n",pport->devf,pport->port,stat);

        pasynManager->interruptStart(pport->asynOctetCallbackPvt,&plist);
        pnode = (interruptNode*)ellFirst(plist);
        while( pnode )
        {
            pintr = (asynOctetInterrupt*)pnode->drvPvt;
            pintr->callback(pintr->userPvt,pintr->pasynUser,(char*)&stat,sizeof(stat),0);
            pnode = (interruptNode*)ellNext(&pnode->node);
        }
        pasynManager->interruptEnd(pport->asynOctetCallbackPvt);

    }

}


/****************************************************************************
 * Define private interface asynCommon methods
 ****************************************************************************/
static void report(void* ppvt,FILE* fp,int details)
{
    Port* pport = (Port*)ppvt;
    fprintf(fp,"    EPICS Brick OMS %s is %sonnected\n",pport->devf,(pport->fd >= 0 ? "c" : "disc"));
}


static asynStatus connect(void* ppvt,asynUser* pasynUser)
{
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvOmsPcx8::connect %s:\n",pport->port);

    if( pport->fd >= 0 )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvOmsPcx8::connect %s: already opened %s\n",pport->port,pport->devf);
        return( asynError );
    }

    pport->fd = open(pport->devf,O_RDWR,0);
    if( pport->fd < 0 )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvOmsPcx8::connect %s: failure to open %s\n",pport->port,pport->devf);
        return( asynError );
    }

    pport->isConn = TRUE;
    pasynManager->exceptionConnect(pasynUser);
    return( asynSuccess );
}


static asynStatus disconnect(void* ppvt,asynUser* pasynUser)
{
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvOmsPcx8::disconnect %s:\n",pport->port);

    close(pport->fd);
    pport->fd = -1;
    pport->isConn = FALSE;
    pasynManager->exceptionDisconnect(pasynUser);

    return( asynSuccess );
}


/****************************************************************************
 * Define private interface asynOctet methods
 ****************************************************************************/
static asynStatus flushIt(void *ppvt,asynUser* pasynUser)
{
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvOmsPcx8::flushing %s: on %s\n",pport->port,pport->devf);
    return( asynSuccess );
}

static asynStatus writeItRaw(void *ppvt,asynUser *pasynUser,const char *data,size_t numchars,size_t *nbytes)
{
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvOmsPcx8::writeItRaw %s: write\n",pport->port);

    if( pport->fd < 0 )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvOmsPcx8::writeItRaw %s: has been disconnected from %s\n",pport->port,pport->devf);
        return( asynError );
    }

    *nbytes = write(pport->fd,(char*)data,numchars);
    asynPrint(pasynUser,ASYN_TRACEIO_DRIVER,"drvOmsPcx8::writeItRaw %s: wrote %.*s to %s\n",pport->port,*nbytes,data,pport->devf);

    return( asynSuccess );
}


static asynStatus readItRaw(void* ppvt,asynUser* pasynUser,char* data,size_t maxchars,size_t* nbytes,int* eom)
{
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvOmsPcx8::readItRaw %s: read\n",pport->port);

    if( pport->fd < 0 )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvOmsPcx8::readItRaw %s: has been disconnected from %s\n",pport->port,pport->devf);
        return( asynError );
    }

    *eom = 0;
    *nbytes = read(pport->fd,data,maxchars);
    asynPrint(pasynUser,ASYN_TRACEIO_DRIVER,"drvOmsPcx8::readItRaw %s: read %.*s from %s\n",pport->port,*nbytes,data,pport->devf);

    return( asynSuccess );
}


/****************************************************************************
 * Register public methods
 ****************************************************************************/

/* Initialization method definitions */
static const iocshArg arg0 = {"port",iocshArgString};
static const iocshArg arg1 = {"devf",iocshArgString};
static const iocshArg* args[]= {&arg0,&arg1};
static const iocshFuncDef funcDef = {"drvOmsPcx8Init",2,args};
static void callFunc(const iocshArgBuf* args)
{
    drvOmsPcx8Init(args[0].sval,args[1].sval);
}

/* Registration method */
static void drvOmsPcx8Register(void)
{
    static int firstTime = 1;

    if( firstTime )
    {
        firstTime = 0;
        iocshRegister(&funcDef,callFunc);
    }
}
epicsExportRegistrar(drvOmsPcx8Register);
