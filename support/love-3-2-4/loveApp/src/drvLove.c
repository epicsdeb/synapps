/*

                          Argonne National Laboratory
                            APS Operations Division
                     Beamline Controls and Data Acquisition

                          Love Controller Port Driver



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
    initialize the driver, the method drvLoveInit() is called from the
    startup script with the following calling sequence.

        drvLoveInit( lovPort, serPort, serAddr )

        Where:
            lovPort - Love port driver name (i.e. "L0" )
            serPort - Serial port driver name (i.e. "S0" )
            serAddr - Serial port driver address


    Every controller must be configured prior to IOC initialization. Call
    the method drvLoveConfig() from the startup script with the following
    calling sequence.

        drvLoveConfig( lovPort, addr, model )

        Where:
            lovPort - Love port driver name (i.e. "L0" )
            addr    - Controller address on RS485.
            model   - Controller model type, either 1600 or 16A.

    Prior to initializing the drvLove driver, the serial port driver
    (drvAsynSerialPort) must be initialized.

    The method dbior can be called from the IOC shell to display the current
    status of the driver as well as individual controllers.


 Developer notes:

 Source control info:
    Modified by:    $Author: dkline $
                    $Date: 2006-05-26 18:43:37 $
                    $Revision: 1.5 $

 =============================================================================
 History:
 Author: David M. Kline
 -----------------------------------------------------------------------------
 2005-Mar-25  DMK  Derived from lovelink interpose interface.
 2005-Mar-29  DMK  Initial development of the port driver complete.
 2005-Jul-21  DMK  Modified driver to support standard Asyn interfaces.
 2006-May-19  DMK  Corrected problem in sendCommand() on a retry. Removed
                   'data' parameter since it is not really needed.
 2006-May-24  DMK  Modified 'TUNE' symbolic constant to 0.1 seconds.
                   Modified evalMessage() to evaluate the message for an
                   STX (0x02) character and message length < 7.
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


/* EPICS system related include files */
#include <iocsh.h>
#include <epicsStdio.h>
#include <cantProceed.h>
#include <epicsString.h>
#include <epicsThread.h>


/* EPICS synApps/Asyn related include files */
#include <asynDriver.h>
#include <asynInt32.h>
#include <asynOctet.h>
#include <asynDrvUser.h>
#include <asynUInt32Digital.h>
#include <epicsExport.h>


/* Define symbolic constants */
#define K_INSTRMAX ( 256 )
#define K_COMTMO   ( 1.0 )
#define K_TUNE     ( 0.1 )


/* Forward struct declarations */
typedef struct Port Port;
typedef struct Inst Inst;
typedef struct Instr Instr;
typedef struct CmdStr CmdStr;
typedef struct CmdTbl CmdTbl;
typedef struct Serport Serport;
typedef union Readback Readback;


/* Define model enum */
typedef enum {model1600,model16A} Model;


/* Declare instrument info structure */
struct Instr
{
    Model modidx;
    int   isConn;
};


/* Declare serial port structure */
struct Serport
{
    char*       name;
    int         addr;
    int         isConn;
    int         canBlock;
    int         multiDevice;
    int         autoConnect;
    asynUser*   pasynUser;
    asynOctet*  pasynOctet;
    void*       pasynOctetPvt;
};


/* Declare love port structure */
struct Port
{
    Port*         pport;

    char*         name;
    int           isConn;
    Serport*      pserport;
    asynUser*     pasynUser;
    asynInterface asynInt32;
    asynInterface asynUInt32;
    asynInterface asynCommon;
    asynInterface asynDrvUser;
    asynInterface asynLockPort;
    char          outMsg[20];
    char          inpMsg[20];
    char          tmpMsg[20];
    Instr         instr[K_INSTRMAX];
};


/* Define record instance struct */
struct Inst
{
    Instr* pinfo;
    Port* pport;
    const CmdStr* pcmd;
    asynStatus (*read)(Inst* pinst,epicsInt32* value);
    asynStatus (*write)(Inst* pinst,epicsInt32* value);
};


/* Define command strings struct */
struct CmdStr
{
    const char* read;
    const char* write;
};

struct CmdTbl
{
    const char* pname;
    asynStatus (*read)(Inst* pinst,epicsInt32* value);
    asynStatus (*write)(Inst* pinst,epicsInt32* value);
    CmdStr strings[2];
};


/* Define readback struct */
union Readback
{
    struct
    {
        char data[2];
    } State;

    struct
    {
        char info[2];
        char data[4];
    } Signed;

    struct
    {
        char stat[4];
        char data[4];
    } Value;

};


/* Define local variants */
static Port* pports = NULL;

static char* errCodes[] =
{
/* 00 */  "00 - Not used.",
/* 01 */  "01 - Undefined command. Command not within acceptable range.",
/* 02 */  "02 - Checksum error on received data from Host.",
/* 03 */  "03 - Command not performed by instrument.",
/* 04 */  "04 - Illegal ASCII characters received.",
/* 05 */  "05 - Data field error. Not enough, too many, or improper positioning.",
/* 06 */  "06 - Undefined command. Command not within acceptable range.",
/* 07 */  "07 - Not used.",
/* 08 */  "08 - Hardware fault. Return to Factory for service.",
/* 09 */  "09 - Hardware fault. Return to Factory for service.",
/* 10 */  "10 - Undefined command. Command not within acceptable range."
};


/* Define table and forward references for command response methods */
static asynStatus getValue(Inst* pinst,epicsInt32* value);
static asynStatus getStatus(Inst* pinst,epicsInt32* value);
static asynStatus getSignedValue(Inst* pinst,epicsInt32* value);
static asynStatus getData(Inst* pinst,epicsInt32* value);
static asynStatus putData(Inst* pinst,epicsInt32* value);
static asynStatus doNull(Inst* pinst,epicsInt32* value);

static const CmdTbl CmdTable[] =
{
    /*Command  Read             Write    1600              16A      */
    {"Value",  getValue,        doNull,  {{  "00",   NULL},{  "00",   NULL}}},
    {"SP1",    getSignedValue,  putData, {{"0100", "0200"},{"0101", "0200"}}},
    {"SP2",    getSignedValue,  putData, {{"0102", "0202"},{"0105", "0204"}}},
    {"AlLo",   getSignedValue,  putData, {{"0104", "0204"},{"0106", "0207"}}},
    {"AlHi",   getSignedValue,  putData, {{"0105", "0205"},{"0107", "0208"}}},
    {"Peak",   getSignedValue,  doNull,  {{"011A",   NULL},{"011D",   NULL}}},
    {"Valley", getSignedValue,  doNull,  {{"011B",   NULL},{"011E",   NULL}}},
    {"AlSts",  getStatus,       doNull,  {{  "00",   NULL},{  "00",   NULL}}},
    {"AlMode", getData,         doNull,  {{"0337",   NULL},{"031D",   NULL}}},
    {"InpTyp", getData,         doNull,  {{"0323",   NULL},{"0317",   NULL}}},
    {"ComSts", getData,         doNull,  {{"032A",   NULL},{"0324",   NULL}}},
    {"Decpts", getData,         doNull,  {{"0324",   NULL},{"031A",   NULL}}}
};
static const int cmdCount = (sizeof(CmdTable) / sizeof(CmdTbl));


/* Public forward references */
int drvLoveInit(const char* lovPort,const char* serPort,int serAddr);
int drvLoveConfig(const char* lovPort,int addr,const char *model);


/* Forward references for support methods */
static asynStatus initSerialPort(Port* plov,const char* serPort,int serAddr);
static void exceptCallback(asynUser* pasynUser,asynException exception);


static asynStatus processWriteResponse(Port* pport);
static asynStatus executeCommand(Port* pport,asynUser* pasynUser);
static asynStatus sendCommand(void* ppvt,asynUser* pasynUser,int retry);
static asynStatus recvReply(void* ppvt,asynUser* pasynUser,char* data,size_t maxchars);

static asynStatus setDefaultEos(Port* plov);
static asynStatus evalMessage(size_t* pcount,char* pinp,asynUser* pasynUser,char* pout);
static void calcChecksum(size_t count,const char* pdata,unsigned char* pcs);


/* Forward references for asynCommon methods */
static void reportIt(void* ppvt,FILE* fp,int details);
static asynStatus connectIt(void* ppvt,asynUser* pasynUser);
static asynStatus disconnectIt(void* ppvt,asynUser* pasynUser);
static asynCommon common = {reportIt,connectIt,disconnectIt};


/* Forward references for asynDrvUser methods */
static asynStatus create(void* ppvt,asynUser* pasynUser,const char* drvInfo, const char** pptypeName,size_t* psize);
static asynStatus destroy(void* ppvt,asynUser* pasynUser);
static asynStatus getType(void* ppvt,asynUser* pasynUser,const char** pptypeName,size_t* psize);
static asynDrvUser drvuser = {create,getType,destroy};


/* asynLockPortNotify methods */
static asynStatus lockPort(void *drvPvt,asynUser *pasynUser);
static asynStatus unlockPort(void *drvPvt,asynUser *pasynUser);
static asynLockPortNotify lockport = {lockPort,unlockPort};


/* Forward references for asynInt32 methods */
static asynStatus readInt32(void* ppvt,asynUser* pasynUser,epicsInt32* value);
static asynStatus writeInt32(void* ppvt,asynUser* pasynUser,epicsInt32 value);


/* Forward references for asynUInt32Digital methods */
static asynStatus readUInt32(void* ppvt,asynUser* pasynUser,epicsUInt32* value,epicsUInt32 mask);
static asynStatus writeUInt32(void* ppvt,asynUser* pasynUser,epicsUInt32 value,epicsUInt32 mask);


/* Define macros */
#define ISOK(s) (asynSuccess==(s))
#define ISNOTOK(s) (!ISOK(s))


/****************************************************************************
 * Define public interface methods
 ****************************************************************************/
int drvLoveInit(const char* lovPort,const char* serPort,int serAddr)
{
    asynStatus sts;
    int len,attr;
    Port* plov;
    Serport* pser;
    asynUser* pasynUser;
    asynInt32* pasynInt32;
    asynUInt32Digital* pasynUInt32;

    len = sizeof(Port) + sizeof(Serport) + sizeof(asynInt32) + sizeof(asynUInt32Digital);
    len += strlen(lovPort) + strlen(serPort) + 2;
    plov = callocMustSucceed(len,sizeof(char),"drvLoveInit");

    pser = (Serport*)(plov + 1);
    pasynInt32 = (asynInt32*)(pser + 1);
    pasynUInt32 = (asynUInt32Digital*)(pasynInt32 + 1);
    plov->name = (char*)(pasynUInt32 + 1);
    pser->name = plov->name + strlen(lovPort) + 1;

    plov->isConn = 0;
    plov->pserport = pser;
    strcpy(plov->name,lovPort);

    sts = initSerialPort(plov,serPort,serAddr);
    if( ISNOTOK(sts) )
    {
        printf("drvLoveInit::failure to initialize serial port %s\n",serPort);
        free(plov);
        return( -1 );
    }

    attr = (pser->canBlock)?(ASYN_MULTIDEVICE|ASYN_CANBLOCK):ASYN_MULTIDEVICE;
    sts = pasynManager->registerPort(lovPort,attr,pser->autoConnect,0,0);
    if( ISNOTOK(sts) )
    {
        printf("drvLoveInit::failure to register love port %s\n",lovPort);
        pasynManager->disconnect(pser->pasynUser);
        pasynManager->freeAsynUser(pser->pasynUser);
        free(plov);
        return( -1 );
    }

    plov->asynCommon.interfaceType = asynCommonType;
    plov->asynCommon.pinterface = &common;
    plov->asynCommon.drvPvt = plov;

    sts = pasynManager->registerInterface(lovPort,&plov->asynCommon);
    if( ISNOTOK(sts) )
    {
        printf("drvLoveInit::failure to register asynCommon\n");
        return( -1 );
    }

    plov->asynDrvUser.interfaceType = asynDrvUserType;
    plov->asynDrvUser.pinterface = &drvuser;
    plov->asynDrvUser.drvPvt = plov;

    sts = pasynManager->registerInterface(lovPort,&plov->asynDrvUser);
    if( ISNOTOK(sts) )
    {
        printf("drvLoveInit::failure to register asynDrvUser\n");
        return( -1 );
    }

    plov->asynLockPort.interfaceType = asynLockPortNotifyType;
    plov->asynLockPort.pinterface = &lockport;
    plov->asynLockPort.drvPvt = plov;

    sts = pasynManager->registerInterface(lovPort,&plov->asynLockPort);
    if( ISNOTOK(sts) )
    {
        printf("drvLoveInit::failure to register asynLockPort\n");
        return( -1 );
    }

    pasynInt32->read = readInt32;
    pasynInt32->write = writeInt32;
    plov->asynInt32.interfaceType = asynInt32Type;
    plov->asynInt32.pinterface = pasynInt32;
    plov->asynInt32.drvPvt = plov;

    sts = pasynInt32Base->initialize(lovPort,&plov->asynInt32);
    if( ISNOTOK(sts) )
    {
        printf("drvLoveInit::failure to initialize asynInt32Base\n");
        return( -1 );
    }

    pasynUInt32->read = readUInt32;
    pasynUInt32->write = writeUInt32;
    plov->asynUInt32.interfaceType = asynUInt32DigitalType;
    plov->asynUInt32.pinterface = pasynUInt32;
    plov->asynUInt32.drvPvt = plov;

    sts = pasynUInt32DigitalBase->initialize(lovPort,&plov->asynUInt32);
    if( ISNOTOK(sts) )
    {
        printf("drvLoveInit::failure to initialize asynUInt32DigitalBase\n");
        return( -1 );
    }

    pasynUser = pasynManager->createAsynUser(NULL,NULL);
    if( pasynUser )
    {
        plov->pasynUser = pasynUser;
        pasynUser->userPvt = plov;
        pasynUser->timeout = K_COMTMO;
    }
    else
    {
        printf("drvLoveInit::create asynUser failure\n");
        return( -1 );
    }

    sts = pasynManager->connectDevice(pasynUser,lovPort,-1);
    if( ISNOTOK(sts) )
    {
        printf("drvLoveInit::failure to connect with device %s\n",lovPort);
        plov->isConn = 0;
        return( -1 );
    }

    pasynManager->exceptionCallbackAdd(pser->pasynUser,exceptCallback);

    if( pports )
        plov->pport = pports;
    pports = plov;

    sts = setDefaultEos(plov);
    if( ISNOTOK(sts) )
    {
        printf("drvLoveInit::failure to set %s EOS\n",lovPort);
        return( -1 );
    }

    return( 0 );
}


int drvLoveConfig(const char* lovPort,int addr,const char* model)
{
    Port* pport;

    for( pport = pports; pport; pport = pport->pport )
        if( epicsStrCaseCmp(pport->name,lovPort) == 0 )
        {
            if( epicsStrCaseCmp("1600",model) == 0 )
                pport->instr[addr-1].modidx = model1600;
            else if( epicsStrCaseCmp("16A",model) == 0 )
                pport->instr[addr-1].modidx = model16A;
            else
            {
                printf("drvLoveConfig::unsupported model \"%s\"",model);
                return( -1 );
            }
            return( 0 );
        }

    printf("drvLoveConfig::failure to locate port %s\n",lovPort);
    return( -1 );
}


/****************************************************************************
 * Define private interface suppport methods
 ****************************************************************************/
static asynStatus initSerialPort(Port* plov,const char* serPort,int serAddr)
{
    asynStatus sts;
    asynUser* pasynUser;
    Serport* pser = plov->pserport;
    asynInterface* pasynIface;

    pasynUser = pasynManager->createAsynUser(NULL,NULL);
    if( pasynUser == NULL )
        return( asynError );

    sts = pasynManager->connectDevice(pasynUser,serPort,serAddr);
    if( ISNOTOK(sts) )
    {
        printf("initSerialPort::failure to connect with device %s @ %d\n",serPort,serAddr);
        pasynManager->freeAsynUser(pasynUser);
        return( asynError );
    }

    sts = pasynManager->isMultiDevice(pasynUser,serPort,&pser->multiDevice);
    if( ISNOTOK(sts) )
    {
        printf("initSerialPort::failure to determine if %s @ %d is multi device\n",serPort,serAddr);
        pasynManager->disconnect(pasynUser);
        pasynManager->freeAsynUser(pasynUser);
        return( asynError );
    }

    if(pser->multiDevice)
    {
        printf("initSerialPort::%s cannot be configured as multi device\n",serPort);
        pasynManager->disconnect(pasynUser);
        pasynManager->freeAsynUser(pasynUser);
        return( asynError );
    }

    sts = pasynManager->canBlock(pasynUser,&pser->canBlock);
    if( ISNOTOK(sts) )
    {
        printf("initSerialPort::failure to determine if %s @ %d can block\n",serPort,serAddr);
        pasynManager->disconnect(pasynUser);
        pasynManager->freeAsynUser(pasynUser);
        return( asynError );
    }

    sts = pasynManager->isAutoConnect(pasynUser,&pser->autoConnect);
    if( ISNOTOK(sts) )
    {
        printf("initSerialPort::failure to determine if %s @ %d is autoconnect\n",serPort,serAddr);
        pasynManager->disconnect(pasynUser);
        pasynManager->freeAsynUser(pasynUser);
        return( asynError );
    }

    pasynIface = pasynManager->findInterface(pasynUser,asynOctetType,1);
    if( pasynIface )
    {
        pser->pasynOctet = (asynOctet*)pasynIface->pinterface;
        pser->pasynOctetPvt = pasynIface->drvPvt;
    }
    else
    {
        printf("initSerialPort::failure to find interface %s\n",asynOctetType);
        pasynManager->disconnect(pasynUser);
        pasynManager->freeAsynUser(pasynUser);
        return( asynError );
    }

    pser->addr = serAddr;
    pser->pasynUser = pasynUser;
    pser->isConn = 1;
    strcpy(pser->name,serPort);
    pasynUser->userPvt = plov;
    pasynUser->timeout = K_COMTMO;

    return( asynSuccess );
}


static asynStatus evalMessage(size_t* pcount,char* pinp,asynUser* pasynUser,char* pout)
{
    size_t len;
    asynStatus sts;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvLove::evalMessage\n");

    /* Evaluate message contents,length,... */
    if( *pinp != '\002' )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvLove::evalMessage start char missing\n");
        return( asynError );
    }

    if( *pcount < 7 )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvLove::evalMessage message length (%d) error\n",*pcount);
        return( asynError );
    }

    /* Process message given size */
    if( *pcount == 7 )
    {
        int errNum;

        len = *pcount - 4;      /* Minus STX, FILTER, ADDR */
        errNum = atol( pinp + 5 );
        sts = asynError;

        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvLove::evalMessage error message received \"%s\"\n",errCodes[errNum]);
    }
    else
    {
        unsigned int  csMsg;
        unsigned char csPos, csVal;

        len   = *pcount - 3;    /* Minus STX and CHECKSUM */
        csPos = *pcount - 2;    /* Checksum position */
        calcChecksum(len,&pinp[1],&csVal);
        sscanf(&pinp[csPos],"%2x",&csMsg);

        if( (unsigned int)csMsg != (unsigned int)csVal )
        {
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvLove::evalMessage checksum failed\n");
            return( asynError );
        }

        len = len - 3;
        sts = asynSuccess;

        asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvLove::evalMessage message received\n");
    }

    memcpy(pout,&pinp[4],len);
    pout[len] = '\0';
    *pcount = len;

    return( sts );
}


static void calcChecksum(size_t count,const char* pdata,unsigned char* pcs)
{
    int i;
    unsigned long cs;

    cs = 0;
    for( i = 0; i < count; ++i )
        cs += pdata[i];
    *pcs = (unsigned char)(cs & 0xFF);

    return;
}


static asynStatus setDefaultEos(Port* plov)
{
    asynStatus sts;
    char inpEos = '\006';
    char outEos = '\003';
    Serport* pser = plov->pserport;

    sts = pser->pasynOctet->setInputEos(pser->pasynOctetPvt,pser->pasynUser,&inpEos,1);
    if( ISOK(sts) )
        printf("drvLove::setDefaultEos Input EOS set to \\0%d\n",inpEos);
    else
        printf("drvLove::setDefaultEos Input EOS set failed to \\0%d\n",inpEos);

    sts = pser->pasynOctet->setOutputEos(pser->pasynOctetPvt,pser->pasynUser,&outEos,1);
    if( ISOK(sts) )
        printf("drvLove::setDefaultEos Output EOS set to \\0%d\n",outEos);
    else
        printf("drvLove::setDefaultEos Output EOS set failed to \\0%d\n",outEos);

    return( sts );
}


static void exceptCallback(asynUser* pasynUser,asynException exception)
{
    asynStatus sts;
    int isConn;
    Port* plov = pasynUser->userPvt;
    Serport* pser = plov->pserport;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvLove::exceptionCallback\n");

    sts = pasynManager->isConnected(pasynUser,&isConn);
    if( ISNOTOK(sts) )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvLove::exceptionCallback failure to determine if %s is connected to %s\n",plov->name,pser->name);
        return;
    }

    if( isConn )
        return;

    if( plov->isConn == 0 )
        return;

    plov->isConn = 0;
    pasynManager->exceptionDisconnect(plov->pasynUser);
}


static asynStatus lockPort(void* drvPvt,asynUser* pasynUser)
{
    asynStatus sts;
    Port* plov = (Port*)drvPvt;
    Serport* pser = plov->pserport;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvLove::lockPort\n");

    sts = pasynManager->lockPort(pser->pasynUser);
    if( ISNOTOK(sts) )
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,"%s error %s",pser->name,pser->pasynUser->errorMessage);

    return( sts );
}


static asynStatus unlockPort(void* drvPvt,asynUser* pasynUser)
{
    asynStatus sts;
    Port* plov = (Port*)drvPvt;
    Serport* pser = plov->pserport;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvLove::unlockPort\n");

    sts = pasynManager->unlockPort(pser->pasynUser);
    if( ISNOTOK(sts) )
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvLove::unlockPort %s error %s\n",pser->name,pser->pasynUser->errorMessage);

    return( asynSuccess );
}


static asynStatus executeCommand(Port* pport,asynUser* pasynUser)
{
    int i;
    asynStatus sts;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvLove::executeCommand\n");
    pasynUser->timeout = K_COMTMO;

    for( i = 0; i < 3; ++i )
    {
        epicsThreadSleep( K_TUNE );

        sts = sendCommand(pport,pasynUser,i);
        if( ISOK(sts) )
            asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvLove::executeCommand write \"%s\"\n",pport->outMsg);
        else
        {
            if( sts == asynTimeout )
            {
                asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvLove::executeCommand write timeout, retrying\n");
                continue;
            }

            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvLove::executeCommand write failure - Sent \"%s\" \n",pport->outMsg);
            return( sts );
        }

        sts = recvReply(pport,pasynUser,pport->inpMsg,sizeof(pport->inpMsg));
        if( ISOK(sts) )
            asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvLove::executeCommand read \"%s\"\n",pport->inpMsg);
        else
        {
            if( sts == asynTimeout )
            {
                asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvLove::executeCommand read timeout, retrying\n");
                continue;
            }

            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvLove::executeCommand read failure - Sent \"%s\" Rcvd \"%s\" \n",pport->outMsg,pport->inpMsg);
            return( sts );
        }

        return( asynSuccess );
    }

    asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvLove::executeCommand retries exceeded\n");
    return( asynError );
}


static asynStatus processWriteResponse(Port* pport)
{
    int resp;


    asynPrint(pport->pasynUser,ASYN_TRACE_FLOW,"drvLove::processWriteResponse\n" );

    sscanf(pport->inpMsg,"%2d",&resp);
    if( resp )
    {
        asynPrint(pport->pasynUser,ASYN_TRACE_ERROR,"drvLove::processWriteResponse write command failed\n" );
        return( asynError );
    }

    return( asynSuccess );
}


static asynStatus sendCommand(void* ppvt,asynUser* pasynUser,int retry)
{
    unsigned char cs;
    asynStatus sts;
    int addr;
    size_t len,bytesXfer;
    Port* plov = (Port*)ppvt;
    Serport* pser = plov->pserport;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvLove::sendCommand - retries(%d)\n",retry);

    if( retry == 0 )
    {
        sts = pasynManager->getAddr(pasynUser,&addr);
        if( ISNOTOK(sts) )
            return( sts );

        sprintf(plov->tmpMsg,"%02X%s",addr,plov->outMsg);
        calcChecksum(strlen(plov->tmpMsg),plov->tmpMsg,&cs);
        sprintf(plov->outMsg,"\002L%s%2X",plov->tmpMsg,cs);
    }

    len = strlen(plov->outMsg);
    sts = pser->pasynOctet->write(pser->pasynOctetPvt,pser->pasynUser,plov->outMsg,len,&bytesXfer);
    if( ISOK(sts) )
        asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvLove::sendCommand - retries(%d),data \"%s\" \"%s\"\n",retry,plov->outMsg);
    else
    {
        if( sts == asynTimeout )
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvLove::sendCommand - retries(%d) asynTimeout\n",retry);
        else if( sts == asynOverflow )
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvLove::sendCommand - retries(%d) asynOverflow\n",retry);
        else if( sts == asynError )
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvLove::sendCommand - retries(%d) asynError\n",retry);
        else
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvLove::sendCommand - retries(%d) failed - unknown Asyn error\n",retry);
    }

    return( sts );
}


static asynStatus recvReply(void* ppvt,asynUser* pasynUser,char* data,size_t maxchars)
{
    int eom;
    asynStatus sts;
    size_t bytesXfer;
    Port* plov = (Port*)ppvt;
    Serport* pser = plov->pserport;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvLove::recvReplay\n");

    sts = pser->pasynOctet->read(pser->pasynOctetPvt,pser->pasynUser,data,maxchars,&bytesXfer,&eom);
    if( ISOK(sts) )
    {
        if( eom != ASYN_EOM_EOS )
        {
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvLove::recvReply eos failure\n");
            return( asynError );
        }

        sts = evalMessage(&bytesXfer,plov->inpMsg,pasynUser,data);
        asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvLove::recvReply %d \"%s\"\n",bytesXfer,data);
    }
    else
    {
        if( sts == asynTimeout )
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvLove::recvReply asynTimeout\n");
        else if( sts == asynOverflow )
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvLove::recvReply asynOverflow\n");
        else if( sts == asynError )
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvLove::recvReply asynError\n");
        else
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvLove::recvReply failed - unknown Asyn error\n");
    }

    return( sts );
}


/****************************************************************************
 * Define private command / response methods
 ****************************************************************************/
static asynStatus getValue(Inst* pinst,epicsInt32* value)
{
    int sign,data;
    Port* pport = pinst->pport;
    Readback* prb = (Readback*)pport->inpMsg;

    asynPrint(pport->pasynUser,ASYN_TRACE_FLOW,"drvLove::getValue\n" );

    sscanf(prb->Value.stat,"%4x",&sign);
    sscanf(prb->Value.data,"%4d",&data);
    *value = (epicsInt32)data;
    if( sign & 0x0001 )
        *value *= -1;

    return( asynSuccess );
}


static asynStatus getStatus(Inst* pinst,epicsInt32* value)
{
    int sts;
    Port* pport = pinst->pport;
    Readback* prb = (Readback*)pport->inpMsg;

    asynPrint(pport->pasynUser,ASYN_TRACE_FLOW,"drvLove::getStatus\n" );

    sscanf(prb->Value.stat,"%4x",&sts);
    *value = (epicsInt32)sts;

    return( asynSuccess );
}


static asynStatus getSignedValue(Inst* pinst,epicsInt32* value)
{
    int sts,data;
    Port* pport = pinst->pport;
    Readback* prb = (Readback*)pport->inpMsg;

    asynPrint(pport->pasynUser,ASYN_TRACE_FLOW,"drvLove::getSignedValue\n" );

    sscanf(prb->Signed.data,"%4d",&data);
    *value = (epicsInt32)data;

    if( pinst->pinfo->modidx == model1600 )
    {
        sscanf(prb->Signed.info,"%2d",&sts);
        if( sts )
            *value *= -1;
    }
    else
    {
        sscanf(prb->Signed.info,"%2x",&sts);
        if( sts & 0x0001 )
            *value *= -1;
    }

    return( asynSuccess );
}


static asynStatus getData(Inst* pinst,epicsInt32* value)
{
    int data;
    Port* pport = pinst->pport;
    Readback* prb = (Readback*)pport->inpMsg;

    asynPrint(pport->pasynUser,ASYN_TRACE_FLOW,"drvLove::getData\n" );

    sscanf(prb->State.data,"%2x",&data);
    *value = (epicsInt32)data;

    return( asynSuccess );
}


static asynStatus putData(Inst* pinst,epicsInt32* value)
{
    int sign,data;
    Port* pport = pinst->pport;

    asynPrint(pport->pasynUser,ASYN_TRACE_FLOW,"drvLove::putData\n" );

    if( *value < 0 )
    {
        sign = 0xFF;
        *value *= -1;
    }
    else
        sign = 0;

    data = (int)(*value);
    sprintf(pport->outMsg,"%s%4.4d%2.2X",pinst->pcmd->write,data,sign);

    return( asynSuccess );
}


static asynStatus doNull(Inst* pinst,epicsInt32* value)
{
    return( asynError );
}


/****************************************************************************
 * Define private interface asynCommon methods
 ****************************************************************************/
static void reportIt(void* ppvt,FILE* fp,int details)
{
    int i;
    Port* plov = (Port*)ppvt;
    Serport* pser = plov->pserport;

    fprintf(fp, "    %s is connected to %s\n",plov->name,pser->name);

    for( i = 0; i < K_INSTRMAX; ++i )
        if( plov->instr[i].isConn )
            fprintf(fp, "        Addr %d is connected\n",(i + 1));
}


static asynStatus connectIt(void* ppvt,asynUser* pasynUser)
{
    asynStatus sts;
    int addr,isConn;
    Port* plov = (Port*)ppvt;
    Serport* pser = plov->pserport;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvLove::connectIt\n");

    sts = pasynManager->isConnected(pser->pasynUser,&isConn);
    if( ISNOTOK(sts) )
    {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,"port %s isConn error %s\n",pser->name,pser->pasynUser->errorMessage);
        return( asynError );
    }

    if( isConn == 0 )
    {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize, "port %s not connected to %s\n",plov->name,pser->name);
        return( asynError );
    }

    sts = pasynManager->getAddr(pasynUser,&addr);
    if( ISNOTOK(sts) )
        return( sts );

    if( (addr == 0) || (addr > K_INSTRMAX) )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvLove::connectIt %s illegal addr %d\n",plov->name,addr);
        return( asynError );
    }

    if( addr > 0 )
    {
        Instr* prInstr = &plov->instr[addr - 1];

        if( prInstr->isConn )
        {
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvLove::connectIt %s device %d already connected\n",plov->name,addr);
            return( asynError );
        }

        prInstr->isConn = 1;
    }
    else
    {
        if( plov->isConn )
        {
            asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvLove::connectIt %s device %d already connected\n",plov->name,addr);
            return( asynError );
        }

        plov->isConn = 1;
    }

    pasynManager->exceptionConnect(pasynUser);

    return( asynSuccess );
}


static asynStatus disconnectIt(void* ppvt,asynUser* pasynUser)
{
    asynStatus sts;
    int addr;
    Instr* prInstr;
    Port* plov = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvLove::disconnectIt\n");

    sts = pasynManager->getAddr(pasynUser,&addr);
    if( ISNOTOK(sts) )
        return( sts );

    if( (addr == 0) || (addr > K_INSTRMAX) )
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"drvLove::disconnectIt %s illegal addr %d\n",plov->name,addr);
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,"illegal addr %d",addr);
        return( asynError );
    }

    if( addr < 0 )
    {
        if( plov->isConn == 0 )
        {
            epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,"not connected");
            return( asynError );
        }

        pasynManager->exceptionDisconnect(pasynUser);
        plov->isConn = 0;
        return( asynSuccess );
    }

    prInstr = &plov->instr[addr - 1];
    if( prInstr->isConn )
        prInstr->isConn = 0;
    else
    {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,"not connected");
        return( asynError );
    }

    pasynManager->exceptionDisconnect(pasynUser);

    return( asynSuccess );
}


/****************************************************************************
 * Define private interface asynDrvUser methods
 ****************************************************************************/
static asynStatus create(void* ppvt,asynUser* pasynUser,const char* drvInfo, const char** pptypeName,size_t* psize)
{
    int i,addr;
    asynStatus sts;
    Inst* pinst;
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvLove::create\n");

    sts = pasynManager->getAddr(pasynUser,&addr);
    if( ISNOTOK(sts) )
        return( sts );

    for( i = 0; i < cmdCount; ++i )
    {
        if( epicsStrCaseCmp(CmdTable[i].pname,drvInfo) == 0 )
        {
            pinst = callocMustSucceed(sizeof(Inst),sizeof(char),"drvLove::create");
            pinst->pport = pport;
            pinst->pinfo = &pport->instr[addr-1];
            pinst->read = CmdTable[i].read;
            pinst->write = CmdTable[i].write;
            pinst->pcmd = &CmdTable[i].strings[pinst->pinfo->modidx];

            pasynUser->drvUser = (void*)pinst;

            return( asynSuccess );
        }
    }

    epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,"failure to find command %s",drvInfo);
    return( asynError );
}


static asynStatus getType(void* ppvt,asynUser* pasynUser,const char** pptypeName,size_t* psize)
{
    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvLove::getType\n");

    if( pptypeName )
        *pptypeName = NULL;
    if( psize )
        *psize = 0;

    return( asynSuccess );
}


static asynStatus destroy(void* ppvt,asynUser* pasynUser)
{
    Port* pport = (Port*)ppvt;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvLove::destroy\n");

    if( pport )
    {
        free(pasynUser->drvUser);
        pasynUser->drvUser = NULL;

        return( asynSuccess );
    }
    else
    {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,"%s destroy called before create\n",pport->name);
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,"%s destroy called before create\n",pport->name);

        return( asynError );
    }

}


/****************************************************************************
 * Define private interface asynInt32 methods
 ****************************************************************************/
static asynStatus writeInt32(void* ppvt,asynUser* pasynUser,epicsInt32 value)
{
    asynStatus sts;
    Port* pport = (Port*)ppvt;
    Inst* pinst = (Inst*)pasynUser->drvUser;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvLove::writeInt32\n");

    sts = pinst->write(pinst,&value);
    if( ISNOTOK(sts) )
    {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,"%s error %s",pport->name,pport->pasynUser->errorMessage);
        return( sts );
    }

    lockPort(pport,pasynUser);
    sts = executeCommand(pport,pasynUser);
    unlockPort(pport,pasynUser);

    if( ISNOTOK(sts) )
    {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,"%s error %s",pport->name,pport->pasynUser->errorMessage);
        return( sts );
    }

    sts = processWriteResponse(pport);
    if( ISNOTOK(sts) )
    {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,"%s error %s",pport->name,pport->pasynUser->errorMessage);
        return( sts );
    }

    return( asynSuccess );
}


static asynStatus readInt32(void* ppvt,asynUser* pasynUser,epicsInt32* value)
{
    asynStatus sts;
    Port* pport = (Port*)ppvt;
    Inst* pinst = (Inst*)pasynUser->drvUser;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvLove::readInt32\n");

    if( pinst->pcmd->read )
        sprintf(pport->outMsg,"%s",pinst->pcmd->read);
    else
    {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,"%s error %s",pport->name,pport->pasynUser->errorMessage);
        return( asynError );
    }

    lockPort(pport,pasynUser);
    sts = executeCommand(pport,pasynUser);
    unlockPort(pport,pasynUser);

    if( ISNOTOK(sts) )
    {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,"%s error %s",pport->name,pport->pasynUser->errorMessage);
        return( sts );
    }

    sts = pinst->read(pinst,value);
    if( ISNOTOK(sts) )
    {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,"%s error %s",pport->name,pport->pasynUser->errorMessage);
        return( sts );
    }

    if( ISOK(sts) )
        asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvLove::readInt32 readback from %s is %d\n",pport->name,*value);

    return( asynSuccess );
}


/****************************************************************************
 * Define private interface asynUInt32Digital methods
 ****************************************************************************/
static asynStatus writeUInt32(void* ppvt,asynUser* pasynUser,epicsUInt32 value,epicsUInt32 mask)
{
    asynStatus sts;
    Port* pport = (Port*)ppvt;
    Inst* pinst = (Inst*)pasynUser->drvUser;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvLove::writeUInt32\n");

    sts = pinst->write(pinst,(epicsInt32*)&value);
    if( ISNOTOK(sts) )
    {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,"%s error %s",pport->name,pport->pasynUser->errorMessage);
        return( sts );
    }

    lockPort(pport,pasynUser);
    sts = executeCommand(pport,pasynUser);
    unlockPort(pport,pasynUser);

    if( ISNOTOK(sts) )
    {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,"%s error %s",pport->name,pport->pasynUser->errorMessage);
        return( sts );
    }

    sts = processWriteResponse(pport);
    if( ISNOTOK(sts) )
    {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,"%s error %s",pport->name,pport->pasynUser->errorMessage);
        return( sts );
    }

    return( asynSuccess );
}


static asynStatus readUInt32(void* ppvt,asynUser* pasynUser,epicsUInt32* value,epicsUInt32 mask)
{
    asynStatus sts;
    Port* pport = (Port*)ppvt;
    Inst* pinst = (Inst*)pasynUser->drvUser;

    asynPrint(pasynUser,ASYN_TRACE_FLOW,"drvLove::readUInt32\n");

    if( pinst->pcmd->read )
        sprintf(pport->outMsg,"%s",pinst->pcmd->read);
    else
    {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,"%s error %s",pport->name,pport->pasynUser->errorMessage);
        return( asynError );
    }

    lockPort(pport,pasynUser);
    sts = executeCommand(pport,pasynUser);
    unlockPort(pport,pasynUser);

    if( ISNOTOK(sts) )
    {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,"%s error %s",pport->name,pport->pasynUser->errorMessage);
        return( sts );
    }

    sts = pinst->read(pinst,(epicsInt32*)value);
    if( ISNOTOK(sts) )
    {
        epicsSnprintf(pasynUser->errorMessage,pasynUser->errorMessageSize,"%s error %s",pport->name,pport->pasynUser->errorMessage);
        return( sts );
    }

    if( ISOK(sts) )
        asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"drvLove::readUInt32 readback from %s is 0x%X,mask=0x%X\n",pport->name,*value,mask);

    return( asynSuccess );
}


/****************************************************************************
 * Register public methods
 ****************************************************************************/

/* Initialization method definitions */
static const iocshArg drvLoveInitArg0 = {"lovPort",iocshArgString};
static const iocshArg drvLoveInitArg1 = {"serPort",iocshArgString};
static const iocshArg drvLoveInitArg2 = {"serAddr",iocshArgInt};
static const iocshArg* drvLoveInitArgs[]= {&drvLoveInitArg0,&drvLoveInitArg1,&drvLoveInitArg2};
static const iocshFuncDef drvLoveInitFuncDef = {"drvLoveInit",3,drvLoveInitArgs};
static void drvLoveInitCallFunc(const iocshArgBuf* args)
{
    drvLoveInit(args[0].sval,args[1].sval,args[2].ival);
}

static const iocshArg drvLoveConfigArg0 = {"lovPort",iocshArgString};
static const iocshArg drvLoveConfigArg1 = {"addr",iocshArgInt};
static const iocshArg drvLoveConfigArg2 = {"model",iocshArgString};
static const iocshArg* drvLoveConfigArgs[]= {&drvLoveConfigArg0,&drvLoveConfigArg1,&drvLoveConfigArg2};
static const iocshFuncDef drvLoveConfigFuncDef = {"drvLoveConfig",3,drvLoveConfigArgs};
static void drvLoveConfigCallFunc(const iocshArgBuf* args)
{
    drvLoveConfig(args[0].sval,args[1].ival,args[2].sval);
}

/* Registration method */
static void drvLoveRegister(void)
{
    static int firstTime = 1;

    if( firstTime )
    {
        firstTime = 0;
        iocshRegister( &drvLoveInitFuncDef, drvLoveInitCallFunc );
        iocshRegister( &drvLoveConfigFuncDef, drvLoveConfigCallFunc );
    }
}
epicsExportRegistrar( drvLoveRegister );
