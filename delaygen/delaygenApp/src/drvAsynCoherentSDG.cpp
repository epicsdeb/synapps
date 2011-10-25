/*

                          Argonne National Laboratory
                       APS Engineering Support Division
                     Beamline Controls and Data Acquisition

                   Coherent Synchronization / Delay Generator
                               Asyn Port Driver



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
    initialize the driver, the method drvAsynCoherentSDG() is called
    from the startup script with the following calling sequence.

        drvAsynCoherentSDG(myport,ioport,ioaddr)

        Where:
            myport - Asyn interface port driver name (i.e. "SDG0" )
            ioport - Communication port driver name (i.e. "S0" )
            ioaddr - Communication port device addr

    The method dbior can be called from the IOC shell to display the current
    status of the driver.


 Developer notes:


 Source control info:
    Modified by:    $Author: dkline $
                    $Date: 2009-09-16 18:58:05 $
                    $Revision: 1.4 $

 =============================================================================
 History:
 Author: David M. Kline
 -----------------------------------------------------------------------------
 2009-May-09  DMK  Derived support from drvAsynEPM2000 driver.
 2009-Sep-09  DMK  Modified to eliminate RTEMS compiler warnings.
 -----------------------------------------------------------------------------

*/


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
#include <epicsThread.h>


/* EPICS synApps/Asyn related include files */
#include <asynDriver.h>
#include <asynDrvUser.h>
#include <asynInt32.h>
#include <asynFloat64.h>
#include <asynOctet.h>
#include <asynOctetSyncIO.h>
#include <asynUInt32Digital.h>

/* epicsExport.h must come last */
#include <epicsExport.h>

/* Define symbolic constants */
#define TIMEOUT         (10.0)
#define BUFFER_SIZE     (100)
#define WRITEREADDELAY  (0.100)


/* Forward struct declarations */
typedef struct Port Port;
typedef struct ioPvt ioPvt;
typedef struct Status Status;
typedef struct Command Command;
typedef enum {ifaceAsynOctet,ifaceAsynFloat64,ifaceAsynUInt32} ifaceType;


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
    int   ioaddr;
    int   discos;
    int   writeReads;
    int   outputMode[5];
    char  ident[BUFFER_SIZE];
    epicsMutexId syncLock;

    int pvs;
    int refs;
    int init;
    int conns;

    /* Asyn info */
    asynUser* pasynUser;
    asynInterface asynOctet;
    asynInterface asynCommon;
    asynInterface asynDrvUser;
    asynInterface asynUInt32;
    asynInterface asynFloat64;
};


/* Declare status message structure */
struct Status
{
    int code;
    char* msg;
};


/* Declare command structure */
struct Command
{
    int refs;
    int ident;
    int idx;
    Port* pport;

    const char* readCommand;
    asynStatus (*readFunc)(Command* pcommand,asynUser* pasynUser,char* inpBuf,int inputSize,int* eomReason,ifaceType asynIface);
    int (*readConv)(Command* pcommand,asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface);

    const char* writeCommand;
    asynStatus (*writeFunc)(Command* pcommand,asynUser* pasynUser,void* data,ifaceType asynIface);

    const char* desc;
};


/* Public interface forward references */
int drvAsynCoherentSDG(const char* myport,const char* ioport,int ioaddr);


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
static asynStatus flushOctet(void* ppvt,asynUser* pasynUser);
static asynStatus writeOctet(void* ppvt,asynUser* pasynUser,const char *data,size_t numchars,size_t* nbytes);
static asynStatus readOctet(void* ppvt,asynUser* pasynUser,char* data,size_t maxchars,size_t *nbytes,int *eom);


/* Forward references for external asynOctet interface */
static asynStatus writeRead(Port* pport,asynUser* pasynUser,const char* outBuf,char* inpBuf,int inputSize,int *eomReason);


/* Forward references for utility methods */
static Command* findCommand(int command);

/* Forward references for command table methods */
static asynStatus readSink(Command* pcommand,asynUser* pasynUser,char* inpBuf,int inputSize,int *eomReason,ifaceType asynIface);
static asynStatus readParam(Command* pcommand,asynUser* pasynUser,char* inpBuf,int inputSize,int *eomReason,ifaceType asynIface);

static asynStatus writeSink(Command* pcommand,asynUser* pasynUser,void* data,ifaceType asynIface);
static asynStatus writeIntParam(Command* pcommand,asynUser* pasynUser,void* data,ifaceType asynIface);
static asynStatus writeFloatParam(Command* pcommand,asynUser* pasynUser,void* data,ifaceType asynIface);
static asynStatus writeCmdOnly(Command* pcommand,asynUser* pasynUser,void* data,ifaceType asynIface);

static int cvtSink(Command* pcommand,asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface);
static int cvtStrInt(Command* pcommand,asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface);
static int cvtStrBin(Command* pcommand,asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface);
static int cvtStrFloat(Command* pcommand,asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface);
static int cvtIdent(Command* pcommand,asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface);


/* Define local variants */
static Port* pports = NULL;
static Command commandTable[] = 
{
//--------------------------------------------------------------------------------------------------------------------------------------------------
//   refs   ident   idx     pport   readCommand             readFunc        readConv        writeCommand            writeFunc           Desc
//--------------------------------------------------------------------------------------------------------------------------------------------------

    // Instrument management related commands
    {0,     0,      0,      NULL,   "status?",              readParam,      cvtIdent,       "",                     writeSink,          "Status"},
    {0,     1,      0,      NULL,   "read:rate?",           readParam,      cvtStrInt,      "set:rate %4.4d",       writeIntParam,      "Trigger rate"},
    {0,     2,      0,      NULL,   "read:bwd?",            readParam,      cvtStrInt,      "",                     writeSink,          "BWD switch"},
    {0,     3,      0,      NULL,   "read:sta:bwd?",        readParam,      cvtStrBin,      "reset:bwd",            writeCmdOnly,       "BWD photodiodes"},
    {0,     4,      0,      NULL,   "read:rf?",             readParam,      cvtStrInt,      "set:rf %1.1d",         writeIntParam,      "RF sync"},
    {0,     5,      0,      NULL,   "read:mode?",           readParam,      cvtStrInt,      "set:mode %1.1d",       writeIntParam,      "Trigger mode"},
    {0,     6,      0,      NULL,   "",                     readSink,       cvtSink,        "man:trig",             writeCmdOnly,       "Manual trigger"},

    // Channel 1 commands
    {0,     7,      0,      NULL,   "read:c1?",             readParam,      cvtStrInt,      "set:c1 %1.1d",         writeIntParam,      "Output 1 enable/disable"},
    {0,     8,      0,      NULL,   "read:del:c1?",         readParam,      cvtStrFloat,    "set:del:c1 %06.1f",     writeFloatParam,    "Output 1 delay"},

    // Channel 2 commands
    {0,     9,      0,      NULL,   "read:c2?",             readParam,      cvtStrInt,      "set:c2 %d",            writeIntParam,      "Output 2 enable/disable"},
    {0,    10,      0,      NULL,   "read:del:c2?",         readParam,      cvtStrFloat,    "set:del:c2 %06.1f",     writeFloatParam,    "Output 2 delay"},

    // Channel 3 commands
    {0,    11,      0,      NULL,   "read:c3?",             readParam,      cvtStrInt,      "set:c3 %d",            writeIntParam,      "Output 3 enable/disable"},
    {0,    12,      0,      NULL,   "read:del:c3?",         readParam,      cvtStrFloat,    "set:del:c3 %06.1f",     writeFloatParam,    "Output 3 delay"},
};
static int commandLen=sizeof(commandTable)/sizeof(Command);


/* Define macros */
#define ASYN_SUCCESS(s) (asynSuccess==(s))
#define ASYN_ERROR(s)   (!ASYN_SUCCESS(s))
#define MIN(a,b)        ((a)>(b)?(b):(a))


/****************************************************************************
 * Define public interface methods
 ****************************************************************************/
int drvAsynCoherentSDG(const char* myport,const char* ioport,int ioaddr)
{
    Port* pport;
    int len,i,j,eomReason;
    char inpBuf[BUFFER_SIZE];
    asynUser* pasynUser;
    asynOctet* pasynOctet;
    asynFloat64* pasynFloat64;
    asynUInt32Digital* pasynUInt32;

    if( pports )
    {
        printf("drvAsynCoherentSDG:init %s: interface already established\n",myport);
        return( -1 );
    }

    if( pasynOctetSyncIO->connect(ioport,ioaddr,&pasynUser,NULL) )
    {
        printf("drvAsynCoherentSDG:init %s: cannot connect to asyn port %s\n",myport,ioport);
        return( -1 );
    }

    i = strlen(myport)+1;
    j = strlen(ioport)+1;

    len = i+j+sizeof(Port)+sizeof(asynFloat64)+sizeof(asynUInt32Digital)+sizeof(asynOctet);
    pport = (Port*)callocMustSucceed(len,sizeof(char),"drvAsynCoherentSDG");

    pasynUInt32   = (asynUInt32Digital*)(pport + 1);
    pasynFloat64  = (asynFloat64*)(pasynUInt32 + 1);
    pasynOctet    = (asynOctet*)(pasynFloat64 + 1);
    pport->myport = (char*)(pasynOctet + 1);
    pport->ioport = (char*)(pport->myport + i);

    pport->ioaddr    = ioaddr;
    pport->pasynUser = pasynUser;
    pport->syncLock  = epicsMutexMustCreate();
    strcpy(pport->myport,myport);
    strcpy(pport->ioport,ioport);

    if( pasynManager->registerPort(myport,ASYN_MULTIDEVICE|ASYN_CANBLOCK,1,0,0) )
    {
        printf("drvAsynCoherentSDG::init %s: failure to register port\n",myport);

        return( -1 );
    }

    pport->asynCommon.interfaceType = asynCommonType;
    pport->asynCommon.pinterface = &common;
    pport->asynCommon.drvPvt = pport;

    if( pasynManager->registerInterface(myport,&pport->asynCommon) )
    {
        printf("drvAsynCoherentSDG::init %s: failure to register asynCommon\n",myport);

        return( -1 );
    }

    pport->asynDrvUser.interfaceType = asynDrvUserType;
    pport->asynDrvUser.pinterface = &drvuser;
    pport->asynDrvUser.drvPvt = pport;

    if( pasynManager->registerInterface(myport,&pport->asynDrvUser) )
    {
        printf("drvAsynCoherentSDG::init %s: failure to register asynDrvUser\n",myport);

        return( -1 );
    }

    pasynFloat64->read = readFloat64;
    pasynFloat64->write = writeFloat64;
    pport->asynFloat64.interfaceType = asynFloat64Type;
    pport->asynFloat64.pinterface = pasynFloat64;
    pport->asynFloat64.drvPvt = pport;

    if( pasynFloat64Base->initialize(myport,&pport->asynFloat64) )
    {
        printf("drvAsynCoherentSDG::init %s: failure to initialize asynFloat64Base\n",myport);

        return( -1 );
    }

    pasynUInt32->read = readUInt32;
    pasynUInt32->write = writeUInt32;
    pport->asynUInt32.interfaceType = asynUInt32DigitalType;
    pport->asynUInt32.pinterface = pasynUInt32;
    pport->asynUInt32.drvPvt = pport;

    if( pasynUInt32DigitalBase->initialize(myport,&pport->asynUInt32) )
    {
        printf("drvAsynCoherentSDG::init %s: failure to initialize asynUInt32DigitalBase\n",myport);

        return( -1 );
    }

    pasynOctet->flush = flushOctet;
    pasynOctet->read  = readOctet;
    pasynOctet->write = writeOctet;
    pport->asynOctet.drvPvt = pport;
    pport->asynOctet.pinterface = pasynOctet;
    pport->asynOctet.interfaceType = asynOctetType;

    if( pasynOctetBase->initialize(myport,&pport->asynOctet,0,0,0) )
    {
        printf("drvAsynCoherentSDG::init %s: failure to initialize asynOctetBase\n",myport);

        return( -1 );
    }

    /* Status query */
    if( writeRead(pport,pasynUser,"status?",inpBuf,sizeof(inpBuf),&eomReason) )
    {
        printf("drvAsynCoherentSDG::init %s: failure to acquire status\n",myport);
        strcpy(pport->ident,"*COMM FAILED*");

        return( -1 );
    }
    else
        strcpy(pport->ident,"Coherent SDG");

    /* Complete initialization */
    pport->init=1;
    for( i=0; i<commandLen; ++i ) commandTable[i].pport=pport;

    pports = pport;
    return( 0 );
}


/****************************************************************************
 * Define private utility methods
 ****************************************************************************/
static Command* findCommand(int ident)
{
    for(int i=0; i<commandLen; ++i) if( commandTable[i].ident==ident ) return( &commandTable[i] );
    return( NULL );
}


/****************************************************************************
 * Define private convert methods
 ****************************************************************************/
static int cvtSink(Command* pcommand,asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface)
{
    switch( asynIface )
    {
    case ifaceAsynOctet:
        strcpy((char*)outBuf,"");
        break;
    case ifaceAsynUInt32:
        *(epicsUInt32*)outBuf = 0;
        break;
    case ifaceAsynFloat64:
        *(epicsFloat64*)outBuf = 0.0;
        break;
    }

    return( 0 );
}
static int cvtStrInt(Command* pcommand,asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface)
{
    *(epicsUInt32*)outBuf = atoi(inpBuf);
    return( 0 );
}
static int cvtStrBin(Command* pcommand,asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface)
{
    epicsUInt32 i,j,k=0,l=strlen(inpBuf);

    for(i=0,j=l-1; i<l; ++i,--j ) if( inpBuf[i]=='1' ) k+=(1<<j);
    *(epicsUInt32*)outBuf=k;

    return( 0 );
}
static int cvtStrFloat(Command* pcommand,asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface)
{
    *(epicsFloat64*)outBuf = atof(inpBuf);

    return( 0 );
}
static int cvtIdent(Command* pcommand,asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface)
{
    strcpy((char*)outBuf,"Coherent SDG");

    return( MIN((int)strlen((char*)outBuf),maxchars) );
}


/****************************************************************************
 * Define private read parameter methods
 ****************************************************************************/
static asynStatus readSink(Command* pcommand,asynUser* pasynUser,char* inpBuf,int inputSize,int *eomReason,ifaceType asynIface)
{
    return( asynSuccess );
}
static asynStatus readParam(Command* pcommand,asynUser* pasynUser,char* inpBuf,int inputSize,int* eomReason,ifaceType asynIface)
{
    return( writeRead(pcommand->pport,pasynUser,pcommand->readCommand,inpBuf,inputSize,eomReason) );
}


/****************************************************************************
 * Define private write parameter methods
 ****************************************************************************/
static asynStatus writeSink(Command* pcommand,asynUser* pasynUser,void* data,ifaceType asynIface)
{
    return( asynSuccess );
}
static asynStatus writeIntParam(Command* pcommand,asynUser* pasynUser,void* data,ifaceType asynIface)
{
    int eomReason;
    asynStatus status;
    char inpBuf[BUFFER_SIZE],outBuf[BUFFER_SIZE];

    sprintf(outBuf,pcommand->writeCommand,*(epicsUInt32*)data);
    writeRead(pcommand->pport,pasynUser,outBuf,inpBuf,sizeof(inpBuf),&eomReason);
    status = (epicsStrCaseCmp(inpBuf,"OK")==0 ) ? asynSuccess : asynError;

    return( status );
}
static asynStatus writeFloatParam(Command* pcommand,asynUser* pasynUser,void* data,ifaceType asynIface)
{
    int eomReason;
    asynStatus status;
    char inpBuf[BUFFER_SIZE],outBuf[BUFFER_SIZE];

    sprintf(outBuf,pcommand->writeCommand,*(epicsFloat64*)data);
    writeRead(pcommand->pport,pasynUser,outBuf,inpBuf,sizeof(inpBuf),&eomReason);
    status = (epicsStrCaseCmp(inpBuf,"OK")==0 ) ? asynSuccess : asynError;

    return( status );
}
static asynStatus writeCmdOnly(Command* pcommand,asynUser* pasynUser,void* data,ifaceType asynIface)
{
    int eomReason;
    asynStatus status;
    char inpBuf[BUFFER_SIZE];

    writeRead(pcommand->pport,pasynUser,pcommand->writeCommand,inpBuf,sizeof(inpBuf),&eomReason);
    status = (epicsStrCaseCmp(inpBuf,"OK")==0 ) ? asynSuccess : asynError;

    return( status );
}


/****************************************************************************
 * Define private interface asynCommon methods
 ****************************************************************************/
static void report(void* ppvt,FILE* fp,int details)
{
    int i;
    Port* pport = (Port*)ppvt;

    fprintf(fp,"    %s\n",pport->ident);
    fprintf(fp,"    conns %d refs %d pvs %d discos %d writeReads %d\n",pport->conns,pport->refs,pport->pvs,pport->discos,pport->writeReads);
    fprintf(fp,"    support %s initialized\n",(pport->init)?"IS":"IS NOT");
    fprintf(fp,"    myport \"%s\" ioport \"%s\"\n",pport->myport,pport->ioport);
    fprintf(fp,"    total # of commands %d\n",commandLen);
    for( i=0; i<commandLen; ++i ) if( commandTable[i].refs ) fprintf(fp,"    %d refs for \"%s\" command\n",commandTable[i].refs,commandTable[i].desc);

}
static asynStatus connect(void* ppvt,asynUser* pasynUser)
{
    int addr;
    Port* pport = (Port*)ppvt;

    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );
    ++pport->conns;

    pasynManager->exceptionConnect(pasynUser);
    return( asynSuccess );
}
static asynStatus disconnect(void* ppvt,asynUser* pasynUser)
{
    int addr;
    Port* pport = (Port*)ppvt;

    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );
    --pport->conns;

    pasynManager->exceptionDisconnect(pasynUser);
    return( asynSuccess );
}


/****************************************************************************
 * Define private interface asynDrvUser methods
 ****************************************************************************/
static asynStatus create(void* ppvt,asynUser* pasynUser,const char* drvInfo, const char** pptypeName,size_t* psize)
{
    int addr;
    Command* pcommand;
    Port* pport=(Port*)ppvt;

    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );

    pcommand = findCommand(addr);
    if( pcommand==NULL ) return( asynError );

    ++pport->refs;
    if( addr>=0 ) ++pport->pvs;

    ++pcommand->refs;
    pasynUser->drvUser = pcommand;

    return( asynSuccess );
}
static asynStatus gettype(void* ppvt,asynUser* pasynUser,const char** pptypeName,size_t* psize)
{
    if( pptypeName ) *pptypeName = NULL;
    if( psize ) *psize = 0;

    return( asynSuccess );
}
static asynStatus destroy(void* ppvt,asynUser* pasynUser)
{
    Port* pport=(Port*)ppvt;
    Command* pcommand=(Command*)pasynUser->drvUser;

    if( pport ) --pport->pvs;
    if( pcommand ) --pcommand->refs;

    return( asynSuccess );
}


/****************************************************************************
 * Define private interface asynFloat64 methods
 ****************************************************************************/
static asynStatus writeFloat64(void* ppvt,asynUser* pasynUser,epicsFloat64 value)
{
    int addr;
    asynStatus status;
    Port* pport=(Port*)ppvt;
    Command* pcommand=(Command*)pasynUser->drvUser;

    if( pport->init==0 ) return( asynError );
    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );

    epicsMutexMustLock(pport->syncLock);
    status = pcommand->writeFunc(pcommand,pport->pasynUser,&value,ifaceAsynFloat64);
    epicsMutexUnlock(pport->syncLock);

    return( status );
}
static asynStatus readFloat64(void* ppvt,asynUser* pasynUser,epicsFloat64* value)
{
    int addr,eom;
    asynStatus status;
    char inpBuf[BUFFER_SIZE];
    Port* pport=(Port*)ppvt;
    Command* pcommand=(Command*)pasynUser->drvUser;

    if( pport->init==0 ) return( asynError );
    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );

    epicsMutexMustLock(pport->syncLock);
    status = pcommand->readFunc(pcommand,pport->pasynUser,inpBuf,sizeof(inpBuf),&eom,ifaceAsynFloat64);

    if( ASYN_SUCCESS(status) ) pcommand->readConv(pcommand,pasynUser,inpBuf,sizeof(epicsFloat64),value,ifaceAsynFloat64);
    epicsMutexUnlock(pport->syncLock);

    return( status );
}


/****************************************************************************
 * Define private interface asynUInt32Digital methods
 ****************************************************************************/
static asynStatus writeUInt32(void* ppvt,asynUser* pasynUser,epicsUInt32 value,epicsUInt32 mask)
{
    int addr;
    asynStatus status;
    Port* pport=(Port*)ppvt;
    Command* pcommand=(Command*)pasynUser->drvUser;

    if( pport->init==0 ) return( asynError );
    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );

    epicsMutexMustLock(pport->syncLock);
    status = pcommand->writeFunc(pcommand,pport->pasynUser,&value,ifaceAsynUInt32);
    epicsMutexUnlock(pport->syncLock);

    return( status );
}
static asynStatus readUInt32(void* ppvt,asynUser* pasynUser,epicsUInt32* value,epicsUInt32 mask)
{
    int addr,eom;
    asynStatus status;
    char inpBuf[BUFFER_SIZE];
    Port* pport=(Port*)ppvt;
    Command* pcommand=(Command*)pasynUser->drvUser;

    if( pport->init==0 ) return( asynError );
    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );

    epicsMutexMustLock(pport->syncLock);
    status = pcommand->readFunc(pcommand,pport->pasynUser,inpBuf,sizeof(inpBuf),&eom,ifaceAsynUInt32);

    if( ASYN_SUCCESS(status) )
    {
        pcommand->readConv(pcommand,pasynUser,inpBuf,sizeof(epicsUInt32),value,ifaceAsynUInt32);
        *value &= mask;
    }
    epicsMutexUnlock(pport->syncLock);

    return( status );
}


/****************************************************************************
 * Define private interface asynOctet methods
 ****************************************************************************/
static asynStatus flushOctet(void *ppvt,asynUser* pasynUser)
{
    return( asynSuccess );
}
static asynStatus writeOctet(void *ppvt,asynUser *pasynUser,const char *data,size_t numchars,size_t *nbytes)
{
    int addr;
    asynStatus status;
    Port* pport=(Port*)ppvt;
    Command* pcommand=(Command*)pasynUser->drvUser;

    if( pport->init==0 ) return( asynError );
    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );

    epicsMutexMustLock(pport->syncLock);
    status = pcommand->writeFunc(pcommand,pport->pasynUser,(char*)data,ifaceAsynOctet);
    epicsMutexUnlock(pport->syncLock);

    if( ASYN_ERROR(status) ) *nbytes=0; else *nbytes=strlen(data);

    return( status );
}
static asynStatus readOctet(void* ppvt,asynUser* pasynUser,char* data,size_t maxchars,size_t* nbytes,int* eom)
{
    int addr;
    asynStatus status;
    char inpBuf[BUFFER_SIZE];
    Port* pport=(Port*)ppvt;
    Command* pcommand=(Command*)pasynUser->drvUser;

    if( pport->init==0 ) return( asynError );
    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );

    epicsMutexMustLock(pport->syncLock);
    status = pcommand->readFunc(pcommand,pport->pasynUser,inpBuf,sizeof(inpBuf),eom,ifaceAsynOctet);

    if( ASYN_ERROR(status) )
        *nbytes=0;
    else
        *nbytes=pcommand->readConv(pcommand,pasynUser,inpBuf,maxchars,data,ifaceAsynOctet);
    epicsMutexUnlock(pport->syncLock);

    return( status );
}


/****************************************************************************
 * Define private external interface asynOctet methods
 ****************************************************************************/
static asynStatus writeRead(Port* pport,asynUser* pasynUser,const char* outBuf,char* inpBuf,int inputSize,int* eomReason)
{
    asynStatus status;
    size_t nWrite,nRead,nWriteRequested=strlen(outBuf);
    ioPvt* pioPvt = (ioPvt*)pasynUser->userPvt;

    status = pasynOctetSyncIO->writeRead(pasynUser,outBuf,nWriteRequested,inpBuf,inputSize,TIMEOUT,&nWrite,&nRead,eomReason);
    if( nWrite!=nWriteRequested ) status = asynError;
    if( status!=asynSuccess )
    {
        ++pport->discos;

        pasynManager->lockPort(pasynUser);
        pioPvt->pasynCommon->disconnect(pioPvt->pcommonPvt,pasynUser);
        pioPvt->pasynCommon->connect(pioPvt->pcommonPvt,pasynUser);
        pasynManager->unlockPort(pasynUser);

        asynPrint(pasynUser,ASYN_TRACE_ERROR,"%s writeRead: error %d wrote \"%s\"\n",pport->myport,status,outBuf);
    }

    if( status==asynSuccess ) {inpBuf[nRead]='\0';++pport->writeReads;}
    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"%s writeRead: wrote \"%s\" read \"%s\"\n",pport->myport,outBuf,inpBuf);

    epicsThreadSleep( WRITEREADDELAY );
    return( status );
}


/****************************************************************************
 * Register public methods
 ****************************************************************************/

/* Initialization method definitions */
static const iocshArg arg0 = {"myport",iocshArgString};
static const iocshArg arg1 = {"ioport",iocshArgString};
static const iocshArg arg2 = {"ioaddr",iocshArgInt};
static const iocshArg* args[]= {&arg0,&arg1,&arg2};
static const iocshFuncDef drvAsynCoherentSDGFuncDef = {"drvAsynCoherentSDG",3,args};
static void drvAsynCoherentSDGCallFunc(const iocshArgBuf* args)
{
    drvAsynCoherentSDG(args[0].sval,args[1].sval,args[2].ival);
}

/* Registration method */
static void drvAsynCoherentSDGRegister(void)
{
    static int firstTime = 1;

    if( firstTime )
    {
        firstTime = 0;
        iocshRegister( &drvAsynCoherentSDGFuncDef,drvAsynCoherentSDGCallFunc );
    }
}
extern "C" {
epicsExportRegistrar( drvAsynCoherentSDGRegister );
}
