/*

                          Argonne National Laboratory
                            APS Operations Division
                     Beamline Controls and Data Acquisition

                            uCDIMM PIO Port Driver



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
    This module implements an Asyn-based port driver to provide simple PIO
    support for the C5282 ColdFire processor (uCDIMM). The uCDIMM is mounted
    to the BC-071 board that is assumed to be in its standard configuration,
    which includes 8 output bits and 16 input bits. Address 0 of the driver
    represents the 8 output bits; whereas address 1 repesents the 16 input
    bits. Address 2 represents output control bits where bit0 is used to
    remotely reset the uCDIMM through EPICS.

    To initialize the driver, call the method as shown below. It is called
    from the startup script with the following sequence.

        drvAsynC5282Init(myport)

        Where:
            myport - Asyn port name (i.e. "PIO")

    The method dbior can be called from the IOC shell to display the current
    status of the driver.


 Developer notes:
    1. Taken from Eric Norum's "How to create a simple ColdFire and Altera
       FPGA IOC" (ICMS:APS_1189963) LED driver.


 Source control info:
    Modified by:    dkline
                    2008/10/17 14:41:53
                    1.10

 =============================================================================
 History:
 Author: David M. Kline
 -----------------------------------------------------------------------------
 2008-Aug-07  DMK  Initial creation.
 2008-Aug-08  DMK  Added read PIO.
 2008-Sep-03  DMK  Added control PIO.
 2008-Sep-04  DMK  Added SerialLink PIO.
 2008-Sep-09  DMK  Added interrupt handling for read PIO.
 2008-Oct-17  DMK  Corrected a problem when writing bits.
 -----------------------------------------------------------------------------

*/


/* System related include files */
#include <string.h>
#include <stdarg.h>


/* EPICS system related include files */
#include <iocsh.h>
#include <devLib.h>
#include <epicsStdio.h>
#include <cantProceed.h>
#include <epicsString.h>
#include <epicsExport.h>
#include <epicsThread.h>
#include <epicsInterrupt.h>


/* EPICS related include files */
#include <epicsVersion.h>
#include <asynDriver.h>
#include <asynDrvUser.h>
#include <asynUInt32Digital.h>
#include <epicsMessageQueue.h>


/* Define system symbolic constants */
#ifndef TRUE
#define TRUE    (1)
#endif
#ifndef FALSE
#define FALSE   (0)
#endif


/* Define SOPC system symbolic constants */
#define VEC_INP_PORT    (0xC0)

#define PIO_OUT_PORT    (0x0)
#define DAT_OUT_PORT    ((epicsUInt8*)(AVALON_BASE+PIO_OUT_PORT+1))

#define PIO_INP_PORT    (0x10)
#define MSK_INP_PORT    (0xFFFF)
#define DAT_INP_PORT    ((epicsUInt16*)(AVALON_BASE+PIO_INP_PORT))
#define IRM_INP_PORT    ((epicsUInt32*)(AVALON_BASE+PIO_INP_PORT+4))

#define PIO_CTL_PORT    (0x20)
#define DAT_CTL_PORT    ((epicsUInt8*)(AVALON_BASE+PIO_CTL_PORT+1))

#define PIO_SLC_PORT    (0x30)
#define DAT_SLC_PORT    ((epicsUInt8*)(AVALON_BASE+PIO_SLC_PORT+1))

#define AVALON_BASE     (0x30000000)


/* Forward struct declarations */
typedef struct Iop Iop;
typedef struct Conn Conn;
typedef struct Port Port;


/* Declare port structures */
struct Port
{
    char*         name;
    epicsUInt8*   outptr;
    epicsUInt8*   ctlptr;
    epicsUInt8*   slcptr;
    epicsMutexId  syncLock;

    epicsUInt16*  inpptr;
    epicsUInt32*  irmptr;

    asynInterface asynUInt32;
    asynInterface asynCommon;
    asynInterface asynDrvUser;
    void*         asynUInt32CallbackPvt;

    int sent;
    int dropped;
    epicsMessageQueueId msgQ;

    int           conn;
    int           refs;
    int           ints;
};


/* Public interface forward references */
static void intrThread(Port* pctl);
int drvAsynC5282Init(const char* myport);


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


/* Define local variants */
static Port* pports = {NULL};


/* External references */
extern "C" void printk(char *fmt, ...);


/****************************************************************************
 * Define interrupt handling methods
 ****************************************************************************/
static void interrupt_handler(void* parg)
{
    int data;
    Port* pport = (Port*)parg;

    *pport->irmptr = 0x0;
    ++pport->ints;

    data = (epicsUInt32)(*pport->inpptr);
    if( epicsMessageQueueTrySend(pport->msgQ,&data,sizeof(int))==0 )
        pport->sent++;
    else
        pport->dropped++;

    *pport->irmptr = MSK_INP_PORT<<16;
}


static void intrThread(Port *pport)
{
    int status,msg;
    ELLLIST* plist;
    interruptNode* pnode;
    asynUInt32DigitalInterrupt* pintrUInt32;

    while(1)
    {
        status = epicsMessageQueueReceive(pport->msgQ,&msg,sizeof(int));

        pasynManager->interruptStart(pport->asynUInt32CallbackPvt,&plist);
        pnode = (interruptNode*)ellFirst(plist);
        while( pnode )
        {
            pintrUInt32 = (asynUInt32DigitalInterrupt*)pnode->drvPvt;
            pintrUInt32->callback(pintrUInt32->userPvt,pintrUInt32->pasynUser,msg);

            pnode = (interruptNode*)ellNext(&pnode->node);
        }
        pasynManager->interruptEnd(pport->asynUInt32CallbackPvt);
    }
}


/****************************************************************************
 * Define public interface methods
 ****************************************************************************/
int drvAsynC5282Init(const char* myport)
{
    int len;
    Port* pport;
    char thread[20]="uCDIMMcmd";
    asynUInt32Digital* pasynUInt32;

    if( pports )
    {
        printf("drvAsynC5282:init %s: max. number of instances exceeded\n",myport);
        return( -1 );
    }

    len = sizeof(Port);
    len += strlen(myport)+1;
    len += sizeof(asynUInt32Digital);
    pport = (Port*)callocMustSucceed(len,sizeof(char),"drvAsynC5282Init");

    pasynUInt32 = (asynUInt32Digital*)(pport + 1);
    pport->name = (char*)(pasynUInt32 + 1);
    strcpy(pport->name,myport);

    pport->outptr   = DAT_OUT_PORT;
    pport->ctlptr   = DAT_CTL_PORT;
    pport->slcptr   = DAT_SLC_PORT;
    pport->inpptr   = DAT_INP_PORT;
    pport->irmptr   = IRM_INP_PORT;

    pport->syncLock = epicsMutexMustCreate();

    pport->sent     = 0;
    pport->dropped  = 0;
    pport->msgQ     = epicsMessageQueueCreate(10,sizeof(int));

    if( pasynManager->registerPort(myport,ASYN_MULTIDEVICE|ASYN_CANBLOCK,1,0,0) )
    {
        printf("drvAsynC5282::init %s: failure to register port\n",myport);
        free(pport);
        return( -1 );
    }

    pport->asynCommon.interfaceType = asynCommonType;
    pport->asynCommon.pinterface = &common;
    pport->asynCommon.drvPvt = pport;

    if( pasynManager->registerInterface(myport,&pport->asynCommon) )
    {
        printf("drvAsynC5282::init %s: failure to register asynCommon\n",myport);
        free(pport);
        return( -1 );
    }

    pport->asynDrvUser.interfaceType = asynDrvUserType;
    pport->asynDrvUser.pinterface = &drvuser;
    pport->asynDrvUser.drvPvt = pport;

    if( pasynManager->registerInterface(myport,&pport->asynDrvUser) )
    {
        printf("drvAsynC5282::init %s: failure to register asynDrvUser\n",myport);
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
        printf("drvAsynC5282::init %s: failure to initialize asynUInt32DigitalBase\n",myport);
        free(pport);
        return( -1 );
    }

    if( pasynManager->registerInterruptSource(myport,&pport->asynUInt32,&pport->asynUInt32CallbackPvt) )
    {
        printf("drvAsynC5282::init %s: failure to registerInterruptSource for asynUInt32\n",myport);
        free(pport);
        return( -1 );
    }

    /* Create interrupt thread */
    epicsThreadCreate(thread,epicsThreadPriorityMedium,epicsThreadGetStackSize(epicsThreadStackMedium),(EPICSTHREADFUNC)intrThread,(void*)pport);

    /* Initialize uCDIMM interrupt / handling */
    *pport->irmptr = MSK_INP_PORT<<16;
    devConnectInterruptVME(VEC_INP_PORT,interrupt_handler,pport);

    pports = pport;

    return( 0 );
}


/****************************************************************************
 * Define private interface asynCommon methods
 ****************************************************************************/
static void report(void* ppvt,FILE* fp,int details)
{
    int i=0;
    ELLLIST* plist;
    interruptNode* pnode;
    Port* pport = (Port*)ppvt;

    fprintf(fp,"drvAsynC5282 %s: uCDIMM PIO support\n",pport->name);
    pasynManager->interruptStart(pport->asynUInt32CallbackPvt,&plist);
    pnode = (interruptNode*)ellFirst(plist);
    if( pnode == NULL ) fprintf(fp, "        no callback clients\n");
    while( pnode )
    {
        ++i;
        pnode = (interruptNode*)ellNext(&pnode->node);
    }
    pasynManager->interruptEnd(pport->asynUInt32CallbackPvt);
    fprintf(fp,"    %s\n",EPICS_VERSION_STRING);
    fprintf(fp,"    pport connected %s refs %d\n",(pport->conn==TRUE)?"Yes":"No",pport->refs);
    fprintf(fp,"    pport %d callback clients\n",i);
    fprintf(fp,"    pport pio interrupt mask/edge 0x%X\n",*pport->irmptr);
    fprintf(fp,"    pport ints %d sent %d dropped %d\n",pport->ints,pport->sent,pport->dropped);
}


static asynStatus connect(void* ppvt,asynUser* pasynUser)
{
    int addr;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAsynC5282::connect %s:\n",pport->name);

    if( pasynManager->getAddr(pasynUser,&addr) ) return( asynError );

    ++pport->refs;
    pport->conn = TRUE;

    pasynManager->exceptionConnect(pasynUser);
    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvAsynC5282::connect %s: asyn - 0x%8.8X, addr - %d\n",pport->name,(int)pasynUser,(int)addr);

    return( asynSuccess );
}


static asynStatus disconnect(void* ppvt,asynUser* pasynUser)
{
    int addr;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAsynC5282::disconnect %s:\n",pport->name);

    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );

    --pport->refs;
    pport->conn = FALSE;

    pasynManager->exceptionDisconnect(pasynUser);
    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvAsynC5282::disconnect %s: asyn - 0x%8.8X, addr - %d\n",pport->name,(int)pasynUser,(int)addr);

    return( asynSuccess );
}


/****************************************************************************
 * Define private interface asynDrvUser methods
 ****************************************************************************/
static asynStatus create(void* ppvt,asynUser* pasynUser,const char* drvInfo, const char** pptypeName,size_t* psize)
{
    int addr;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAsynC5282::create %s:\n",pport->name);

    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );
    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvAsynC5282::create %s: asyn - 0x%8.8X, addr - %d\n",pport->name,(int)pasynUser,(int)addr);

    return( asynSuccess );
}


static asynStatus gettype(void* ppvt,asynUser* pasynUser,const char** pptypeName,size_t* psize)
{
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAsynC5282::gettype %s:\n",pport->name);

    if( pptypeName )
        *pptypeName = NULL;
    if( psize )
        *psize = 0;

    return( asynSuccess );
}


static asynStatus destroy(void* ppvt,asynUser* pasynUser)
{
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAsynC5282::destroy %s:\n",pport->name);

    if( pport )
    {
        pasynUser->drvUser = NULL;
        return( asynSuccess );
    }
    else
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvAsynC5282::destroy %s: called before create\n",pport->name);
        return( asynError );
    }

}


/****************************************************************************
 * Define private interface asynUInt32Digital methods
 ****************************************************************************/
static asynStatus writeUInt32(void* ppvt,asynUser* pasynUser,epicsUInt32 value,epicsUInt32 mask)
{
    int addr;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAsynC5282::writeUInt32 %s:\n",pport->name);

    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvAsynC5282::writeUInt32 %s: asyn - 0x%8.8X, addr - %d, mask - 0x%8.8X, value - 0x%8.8X\n",pport->name,(int)pasynUser,(int)addr,(int)mask,(int)value);

    epicsMutexMustLock(pport->syncLock);
    switch( addr )
    {
    case 0:
        *pport->outptr = (epicsUInt8)((*pport->outptr & ~(mask^value)) | (mask&value));
        break;
    case 2:
        *pport->ctlptr = (epicsUInt8)value;
        break;
    case 3:
        *pport->slcptr = (epicsUInt8)value;
        break;
    default:
        break;
    }
    epicsMutexUnlock(pport->syncLock);

    return( asynSuccess );
}


static asynStatus readUInt32(void* ppvt,asynUser* pasynUser,epicsUInt32* value,epicsUInt32 mask)
{
    int addr;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAsynC5282::readUInt32 %s:\n",pport->name);

    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );

    epicsMutexMustLock(pport->syncLock);
    switch( addr )
    {
    case 0:
        *value = (epicsUInt32)(*pport->outptr);
        break;
    case 1:
        *value = (epicsUInt32)(*pport->inpptr);
        break;
    case 2:
        *value = 0;
        break;
    case 3:
        *value = 0;
        break;
    default:
        *value = 0;
        break;
    }
    epicsMutexUnlock(pport->syncLock);

    *value &= mask;
    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvAsynC5282::readUInt32 %s: asyn - 0x%8.8X, addr - %d, mask - 0x%8.8X, value - 0x%8.8X\n",pport->name,(int)pasynUser,(int)addr,(int)mask,(int)*value);

    return( asynSuccess );
}


/****************************************************************************
 * Register public methods
 ****************************************************************************/

/* Initialization method definitions */
static const iocshArg arg0 = {"myport",iocshArgString};
static const iocshArg* args[]= {&arg0};
static const iocshFuncDef drvAsynC5282InitFuncDef = {"drvAsynC5282Init",1,args};
static void drvAsynC5282InitCallFunc(const iocshArgBuf* args)
{
    drvAsynC5282Init(args[0].sval);
}

/* Registration method */
static void drvAsynC5282Registrar(void)
{
    static int firstTime = 1;

    if( firstTime )
    {
        firstTime = 0;
        iocshRegister( &drvAsynC5282InitFuncDef,drvAsynC5282InitCallFunc );
    }
}
epicsExportRegistrar( drvAsynC5282Registrar );
