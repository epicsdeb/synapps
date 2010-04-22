/*

                          Argonne National Laboratory
                            APS Operations Division
                     Beamline Controls and Data Acquisition

                       Colby Instrument Asyn Port Driver



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
    initialize the driver, the method drvAsynColby() is called from the
    startup script with the following calling sequence.

        drvAsynColby(myport,ioport,addr,units,iface)

        Where:
            myport - Colby Asyn interface port driver name (i.e. "COLBY" )
            ioport - Communication port driver name (i.e. "S0" )
            addr   - Communication port device addr
            units  - Default units string, support strings are "ps" or "ns"
            iface  - Communication interface (0=Ethernet,1=Serial)

    The method dbior can be called from the IOC shell to display the current
    status of the driver.


 Developer notes:
    Asyn
    addr    Function
 --------------------------------------
    0       Write delay
    1       Read delay
    2       Increment step
    3       Decrement step
    4       Write step
    5       Read step
    6       Write units (ns,ps)
    7       Read identification
    8       Read IP address
    9       Read Gateway address
    10      Read network mask
    11      Read TCP/IP port number
    12      Read DHCP status
    13      Read MAC address
    14      Reset
    15      Calibrate
 --------------------------------------


 Source control info:
    Modified by:    $Author: dkline $
                    $Date: 2009-09-16 18:58:06 $
                    $Revision: 1.6 $

 =============================================================================
 History:
 Author: David M. Kline
 -----------------------------------------------------------------------------
 2008-Dec-14  DMK  Derived support from drvAdc and drvIAXselAsyn drivers.
 2008-Dec-17  DMK  Initial development complete.
 2009-Jan-07  DMK  Removed FALSE symbol from conn assignment.
 2009-Jan-09  DMK  Added 'iface' parameter to initialization for handling
                   responses from either the Ethernet or Serial.
 2009-Jan-13  DMK  Added support for serial interface.
 2009-Sep-09  DMK  Modified to eliminate RTEMS compiler warnings.
 -----------------------------------------------------------------------------

*/


/* EPICS base version-specific definitions (must be performed first) */
#include <epicsVersion.h>
#define LT_EPICSBASE(v,r,l) (EPICS_VERSION<(v)||(EPICS_VERSION==(v)&&(EPICS_REVISION<(r)||(EPICS_REVISION==(r)&&EPICS_MODIFICATION<(l)))))


/* Evaluate EPICS base */
#if LT_EPICSBASE(3,14,7)
    #error "EPICS base must be 3.14.7 or greater"
#endif

/* System related include files */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


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
#include <asynInt32.h>
#include <asynFloat64.h>
#include <asynOctet.h>
#include <asynOctetSyncIO.h>
#include <asynUInt32Digital.h>


/* Define symbolic constants */
#define TIMEOUT         (3.0)
#define BUFFER_SIZE     (100)
#define IFACE_ETHER     (0)
#define IFACE_SERIAL    (1)
#define IFACE_TERM      ("\r\n")


/* Forward struct declarations */
typedef struct Port Port;
typedef struct ioPvt ioPvt;


/* Struct declaration (taken from asynOctetSyncIO.c) */
struct ioPvt
{
   asynCommon  *pasynCommon;
   void        *pcommonPvt;
   asynInt32   *pasynInt32;
   void        *int32Pvt;
   asynDrvUser *pasynDrvUser;
   void        *drvUserPvt;
};


/* Declare port driver structure */
struct Port
{
    char* myport;
    char* ioport;
    char* units;
    int   addr;
    int   iface;
    char  ident[60];
    epicsMutexId  syncLock;

    int pvs;
    int conn;

    /* Asyn info */
    asynUser* pasynUser;
    asynInterface asynOctet;
    asynInterface asynCommon;
    asynInterface asynDrvUser;
    asynInterface asynUInt32;
    asynInterface asynFloat64;
};


/* Public interface forward references */
int drvAsynColby(const char* myport,const char* ioport,int addr,const char* units,int iface);


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


/* Forward references for asynOctet methods */
static asynStatus flushIt(void* ppvt,asynUser* pasynUser);
static asynStatus writeItRaw(void* ppvt,asynUser* pasynUser,const char *data,size_t numchars,size_t* nbytes);
static asynStatus readItRaw(void* ppvt,asynUser* pasynUser,char* data,size_t maxchars,size_t *nbytes,int *eom);

/* Forward references for external asynOctet interface */
static int writeOnly(asynUser* pasynUser,const char* outBuf,int iface);
static int writeRead(asynUser* pasynUser,const char* outBuf,char* inpBuf,int inputSize,int iface);


/* Forward references for utility methods */


/* Define local variants */
static Port* pports = NULL;


/* Define macros */
#define ISOK(s)		(asynSuccess==(s))
#define ISNOTOK(s)	(!ISOK(s))


/****************************************************************************
 * Define public interface methods
 ****************************************************************************/
int drvAsynColby(const char* myport,const char* ioport,int addr,const char* units,int iface)
{
    Port* pport;
    int len,i,j,k;
    char inpBuf[BUFFER_SIZE];
    asynUser* pasynUser;
    asynOctet* pasynOctet;
    asynFloat64* pasynFloat64;
    asynUInt32Digital* pasynUInt32;

    if( pports )
    {
        printf("drvAsynColby:init %s: interface already estamblished\n",myport);
        return( -1 );
    }

    switch( iface )
    {
    case IFACE_ETHER:
    case IFACE_SERIAL:
        break;
    default:
        printf("drvAsynColby:init %s: invalid interface type %d specified\n",myport,iface);
        return( -1 );
    }

    if( pasynOctetSyncIO->connect(ioport,addr,&pasynUser,NULL) )
    {
        printf("drvAsynColby:init %s: cannot connect to asyn port %s\n",myport,ioport);
        return( -1 );
    }

    i = strlen(myport)+1;
    j = strlen(ioport)+1;
    k = strlen(units)+1;

    len = i+j+k+sizeof(Port)+sizeof(asynFloat64)+sizeof(asynUInt32Digital)+sizeof(asynOctet);
    pport = (Port*)callocMustSucceed(len,sizeof(char),"drvAsynColby");

    pasynUInt32   = (asynUInt32Digital*)(pport + 1);
    pasynFloat64  = (asynFloat64*)(pasynUInt32 + 1);
    pasynOctet    = (asynOctet*)(pasynFloat64 + 1);
    pport->myport = (char*)(pasynOctet + 1);
    pport->ioport = (char*)(pport->myport + i);
    pport->units  = (char*)(pport->ioport + j);

    pport->addr   = addr;
    pport->conn   = 0;
    pport->pasynUser = pasynUser;
    pport->iface     = (iface==0)?IFACE_ETHER:IFACE_SERIAL;

    strcpy(pport->myport,myport);
    strcpy(pport->ioport,ioport);
    strcpy(pport->units,units);
    pport->syncLock = epicsMutexMustCreate();

    if( pasynManager->registerPort(myport,ASYN_MULTIDEVICE|ASYN_CANBLOCK,1,0,0) )
    {
        printf("drvAsynColby::init %s: failure to register port\n",myport);
        free(pport);
        return( -1 );
    }

    pport->asynCommon.interfaceType = asynCommonType;
    pport->asynCommon.pinterface = &common;
    pport->asynCommon.drvPvt = pport;

    if( pasynManager->registerInterface(myport,&pport->asynCommon) )
    {
        printf("drvAsynColby::init %s: failure to register asynCommon\n",myport);
        return( -1 );
    }

    pport->asynDrvUser.interfaceType = asynDrvUserType;
    pport->asynDrvUser.pinterface = &drvuser;
    pport->asynDrvUser.drvPvt = pport;

    if( pasynManager->registerInterface(myport,&pport->asynDrvUser) )
    {
        printf("drvAsynColby::init %s: failure to register asynDrvUser\n",myport);
        return( -1 );
    }

    pasynFloat64->read = readFloat64;
    pasynFloat64->write = writeFloat64;
    pport->asynFloat64.interfaceType = asynFloat64Type;
    pport->asynFloat64.pinterface = pasynFloat64;
    pport->asynFloat64.drvPvt = pport;

    if( pasynFloat64Base->initialize(myport,&pport->asynFloat64) )
    {
        printf("drvAsynColby::init %s: failure to initialize asynFloat64Base\n",myport);
        return( -1 );
    }

    pasynUInt32->read = readUInt32;
    pasynUInt32->write = writeUInt32;
    pport->asynUInt32.interfaceType = asynUInt32DigitalType;
    pport->asynUInt32.pinterface = pasynUInt32;
    pport->asynUInt32.drvPvt = pport;

    if( pasynUInt32DigitalBase->initialize(myport,&pport->asynUInt32) )
    {
        printf("drvAsynColby::init %s: failure to initialize asynUInt32DigitalBase\n",myport);
        return( -1 );
    }

    pasynOctet->flush = flushIt;
    pasynOctet->read  = readItRaw;
    pasynOctet->write = writeItRaw;
    pport->asynOctet.drvPvt = pport;
    pport->asynOctet.pinterface = pasynOctet;
    pport->asynOctet.interfaceType = asynOctetType;

    if( pasynOctetBase->initialize(myport,&pport->asynOctet,0,0,0) )
    {
        printf("drvAsynColby::init %s: failure to initialize asynOctetBase\n",myport);
        return( -1 );
    }

    /* Identification query */
    if( writeRead(pasynUser,"*IDN?",inpBuf,sizeof(inpBuf),pport->iface) )
        return( -1 );
    else
        strcpy(pport->ident,inpBuf);

    pports = pport;
    return( 0 );
}


/****************************************************************************
 * Define private interface asynCommon methods
 ****************************************************************************/
static void report(void* ppvt,FILE* fp,int details)
{
    Port* pport = (Port*)ppvt;

    fprintf(fp,"    %s units %s\n",pport->ident,pport->units);
}


static asynStatus connect(void* ppvt,asynUser* pasynUser)
{
    int addr;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAsynColby::connect %s:\n",pport->myport);

    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvAsynColby::connect %s: asyn - 0x%8.8X, addr - %d\n",pport->myport,pasynUser,(int)addr);

    ++pport->conn;

    pasynManager->exceptionConnect(pasynUser);
    return( asynSuccess );
}


static asynStatus disconnect(void* ppvt,asynUser* pasynUser)
{
    int addr;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAsynColby::disconnect %s:\n",pport->myport);

    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvAsynColby::disconnect %s: asyn - 0x%8.8X, addr - %d\n",pport->myport,pasynUser,(int)addr);

    --pport->conn;

    pasynManager->exceptionDisconnect(pasynUser);
    return( asynSuccess );
}


/****************************************************************************
 * Define private interface asynDrvUser methods
 ****************************************************************************/
static asynStatus create(void* ppvt,asynUser* pasynUser,const char* drvInfo, const char** pptypeName,size_t* psize)
{
    int addr;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAsynColby::create %s:\n",pport->myport);

    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );

    pasynUser->drvUser = pport;
    ++pport->pvs;

    return( asynSuccess );
}


static asynStatus gettype(void* ppvt,asynUser* pasynUser,const char** pptypeName,size_t* psize)
{
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAsynColby::gettype %s:\n",pport->myport);

    if( pptypeName ) *pptypeName = NULL;
    if( psize ) *psize = 0;

    return( asynSuccess );
}


static asynStatus destroy(void* ppvt,asynUser* pasynUser)
{
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAsynColby::destroy %s:\n",pport->myport);

    if( pport ) --pport->pvs;
    return( asynSuccess );
}


/****************************************************************************
 * Define private interface asynFloat64 methods
 ****************************************************************************/
static asynStatus writeFloat64(void* ppvt,asynUser* pasynUser,epicsFloat64 value)
{
    int addr,status;
    const char* pcmd;
    char outBuf[BUFFER_SIZE];
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAsynColby::writeFloat64 %s:\n",pport->myport);

    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );

    switch( addr )
    {
    case 0:
        pcmd = "DEL";
        break;
    case 4:
        pcmd = "STEP";
        break;
    default:
        return( asynError );
    }

    epicsMutexMustLock(pport->syncLock);
    sprintf(outBuf,"%s %-.3f %s",pcmd,value,pport->units);
    status = writeOnly(pport->pasynUser,outBuf,pport->iface);
    epicsMutexUnlock(pport->syncLock);

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvAsynColby::writeFloat64 %s: asyn - 0x%8.8X, addr - %d, value - %-.3f\n",pport->myport,pasynUser,addr,value);

    if( status ) return( asynError ); else return( asynSuccess );
}


static asynStatus readFloat64(void* ppvt,asynUser* pasynUser,epicsFloat64* value)
{
    float readback;
    int addr,status;
    const char* pcmd;
    char inpBuf[BUFFER_SIZE];
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAsynColby::readFloat64 %s: reason - %d\n",pport->myport,pasynUser->reason);

    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );

    switch( addr )
    {
    case 0:
    case 1:
        pcmd = "DEL?";
        break;
    case 4:
    case 5:
        pcmd = "STEP?";
        break;
    default:
        return( asynError );
    }

    epicsMutexMustLock(pport->syncLock);
    status = writeRead(pport->pasynUser,pcmd,inpBuf,sizeof(inpBuf),pport->iface);
    epicsMutexUnlock(pport->syncLock);

    if( status ) return( asynError );
    sscanf(inpBuf,"%e",&readback);
    if( epicsStrCaseCmp(pport->units,"ns")==0 ) *value=(epicsFloat64)readback/1.0E-9; else *value=(epicsFloat64)readback/1.0E-12;

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvAsynColby::readFloat64 %s: asyn - 0x%8.8X, addr - %d, value - %f\n",pport->myport,pasynUser,addr,*value);
    return( asynSuccess );
}


/****************************************************************************
 * Define private interface asynUInt32Digital methods
 ****************************************************************************/
static asynStatus writeUInt32(void* ppvt,asynUser* pasynUser,epicsUInt32 value,epicsUInt32 mask)
{
    int addr,status;
    const char* pcmd;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvAsynColby::writeUInt32 %s: asyn - 0x%8.8X, mask - 0x%8.8X, value - 0x%8.8X\n",pport->myport,pasynUser,(int)mask,(int)value);
    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );

    switch( addr )
    {
    case 2:
        pcmd = "INC";
        break;
    case 3:
        pcmd = "DEC";
        break;
    case 6:
        if( value==0 ) strcpy(pport->units,"ns"); else strcpy(pport->units,"ps");
        return( asynSuccess );
    case 14:
        pcmd = "*RST";
        break;
    case 15:
        pcmd = "*CAL";
        break;
    default:
        return( asynError );
    }

    epicsMutexMustLock(pport->syncLock);
    status = writeOnly(pport->pasynUser,pcmd,pport->iface);
    epicsMutexUnlock(pport->syncLock);

    if( status ) return( asynError ); else return( asynSuccess );
}


static asynStatus readUInt32(void* ppvt,asynUser* pasynUser,epicsUInt32* value,epicsUInt32 mask)
{
    int addr;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAsynColby::readUInt32 %s:\n",pport->myport);
    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );

    switch( addr )
    {
    case 2:
    case 3:
        *value = 0;
        return( asynSuccess );
    case 6:
        if( epicsStrCaseCmp(pport->units,"ns")==0 ) *value=0; else *value=1;
        break;
    default:
        return( asynError );
    }

    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvAsynColby::readUInt32 %s: asyn - 0x%8.8X, mask - 0x%8.8X, value - 0x%8.8X\n",pport->myport,pasynUser,(int)mask,(int)*value);
    return( asynSuccess );
}


/****************************************************************************
 * Define private interface asynOctet methods
 ****************************************************************************/
static asynStatus flushIt(void *ppvt,asynUser* pasynUser)
{
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAsynColby::flushing %s: on %s\n",pport->myport,pport->ioport);
    return( asynSuccess );
}

static asynStatus writeItRaw(void *ppvt,asynUser *pasynUser,const char *data,size_t numchars,size_t *nbytes)
{
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAsynColby::writeItRaw %s: write\n",pport->myport);

    *nbytes = 0;
    asynPrint(pasynUser,ASYN_TRACEIO_DRIVER,"drvAsynColby::writeItRaw %s: wrote %.*s to %s\n",pport->myport,*nbytes,data,pport->ioport);

    return( asynSuccess );
}


static asynStatus readItRaw(void* ppvt,asynUser* pasynUser,char* data,size_t maxchars,size_t* nbytes,int* eom)
{
    int i,addr,status;
    const char *pcmd;
    char *token,*next;
    Port* pport = (Port*)ppvt;
    char inpBuf[BUFFER_SIZE],strings[5][24];

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvAsynColby::readItRaw %s: read\n",pport->myport);
    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );

    switch( addr )
    {
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
        pcmd = "NET?";
        break;
    case 13:
        pcmd = "NETM?";
        break;
    default:
        return( asynError );
    }

    epicsMutexMustLock(pport->syncLock);
    status = writeRead(pport->pasynUser,pcmd,inpBuf,sizeof(inpBuf),pport->iface);
    epicsMutexUnlock(pport->syncLock);

    if( status ) {*nbytes=0;*eom=0;return( asynError );};

    if( addr!=13 )
    {
        token = epicsStrtok_r(inpBuf,",",&next);
        for( i=0; token; ++i ) {strcpy(strings[i],token);token=epicsStrtok_r(NULL,",",&next);};
    }

    *eom = ASYN_EOM_END;

    switch( addr )
    {
    case 8:
        strcpy(data,strings[0]);
        *nbytes=strlen(strings[0]);
        break;
    case 9:
        strcpy(data,strings[1]);
        *nbytes=strlen(strings[1]);
        break;
    case 10:
        strcpy(data,strings[2]);
        *nbytes=strlen(strings[2]);
        break;
    case 11:
        strcpy(data,strings[3]);
        *nbytes=strlen(strings[3]);
        break;
    case 12:
        strcpy(data,strings[4]);
        *nbytes=strlen(strings[4]);
        break;
    case 13:
        strcpy(data,inpBuf);
        *nbytes=strlen(inpBuf);
        break;
    }
    asynPrint(pasynUser,ASYN_TRACEIO_DRIVER,"drvAsynColby::readItRaw %s: read %.*s from %s\n",pport->myport,*nbytes,data,pport->ioport);

    return( asynSuccess );
}


/****************************************************************************
 * Define private Colby external interface asynOctet methods
 ****************************************************************************/
static int writeOnly(asynUser* pasynUser,const char* outBuf,int iface)
{
    asynStatus status;
    size_t nActual,nRequested=strlen(outBuf);
    ioPvt* pioPvt = (ioPvt*)pasynUser->userPvt;

    status = pasynOctetSyncIO->write(pasynUser,outBuf,nRequested,TIMEOUT,&nActual);
    if( nActual!=nRequested ) status = asynError;
    if( status!=asynSuccess )
    {
        pasynManager->lockPort(pasynUser);
        pioPvt->pasynCommon->disconnect(pioPvt->pcommonPvt,pasynUser);
        pioPvt->pasynCommon->connect(pioPvt->pcommonPvt,pasynUser);
        pasynManager->unlockPort(pasynUser);

        asynPrint(pasynUser,ASYN_TRACE_ERROR,"writeOnly: error sending %s,sent=%d,err=%d\n",outBuf,nActual,status); 
    }
    if( iface==IFACE_SERIAL )
    {
        size_t nRead;
        int eomReason;
        char inpBuf[BUFFER_SIZE];

        pasynOctetSyncIO->read(pasynUser,inpBuf,sizeof(inpBuf),TIMEOUT,&nRead,&eomReason);
    }
    return( status );
}


static int writeRead(asynUser* pasynUser,const char* outBuf,char* inpBuf,int inputSize,int iface)
{
    int eomReason;
    asynStatus status;
    size_t nWrite,nRead,nWriteRequested=strlen(outBuf);
    ioPvt* pioPvt = (ioPvt*)pasynUser->userPvt;

    status = pasynOctetSyncIO->writeRead(pasynUser,outBuf,nWriteRequested,inpBuf,inputSize,TIMEOUT,&nWrite,&nRead,&eomReason);
    if( nWrite!=nWriteRequested ) status = asynError;
    if( status!=asynSuccess )
    {
        pasynManager->lockPort(pasynUser);
        pioPvt->pasynCommon->disconnect(pioPvt->pcommonPvt,pasynUser);
        pioPvt->pasynCommon->connect(pioPvt->pcommonPvt,pasynUser);
        pasynManager->unlockPort(pasynUser);

        asynPrint(pasynUser,ASYN_TRACE_ERROR,"writeRead: error sending %s,sent=%d,recv=%d,err=%d\n",outBuf,nWrite,nRead,status);
    }
    if( iface==IFACE_SERIAL )
    {
        char *token,*next;

        token = epicsStrtok_r(&inpBuf[strlen(outBuf)+2],IFACE_TERM,&next);
        strcpy(inpBuf,token);
    }

    return( status );
}


/****************************************************************************
 * Register public methods
 ****************************************************************************/

/* Initialization method definitions */
static const iocshArg arg0 = {"myport",iocshArgString};
static const iocshArg arg1 = {"ioport",iocshArgString};
static const iocshArg arg2 = {"addr",iocshArgInt};
static const iocshArg arg3 = {"units",iocshArgString};
static const iocshArg arg4 = {"iface",iocshArgInt};
static const iocshArg* args[]= {&arg0,&arg1,&arg2,&arg3,&arg4};
static const iocshFuncDef drvAsynColbyFuncDef = {"drvAsynColby",5,args};
static void drvAsynColbyCallFunc(const iocshArgBuf* args)
{
    drvAsynColby(args[0].sval,args[1].sval,args[2].ival,args[3].sval,args[4].ival);
}

/* Registration method */
static void drvAsynColbyRegister(void)
{
    static int firstTime = 1;

    if( firstTime )
    {
        firstTime = 0;
        iocshRegister( &drvAsynColbyFuncDef,drvAsynColbyCallFunc );
    }
}
extern "C" {
epicsExportRegistrar( drvAsynColbyRegister );
}
