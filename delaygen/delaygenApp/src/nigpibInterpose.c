/*

                          Argonne National Laboratory
                            APS Operations Division
                     Beamline Controls and Data Acquisition

                           NI GPIB-RS232 Converter
                          Asyn Interpose Interface



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
    This module provides an interpose interface for the NI GPIB-RS232
    converter. To initialize the interface, call the below method from
    the iocsh with the following calling sequence.

        nigpibInterposeConfig(ioport,addr,bsiz,timeout)

        Where:
            ioport  - Communication port driver name (i.e. "S0" )
            addr    - GPIB device address (when conv is 1)
            bsiz    - NI GPIB-RS232 read buffer size (in byes)
            timeout - Communication timeout (in sec)

 Source control info:
    Modified by:    $Author: dkline $
                    $Date: 2009-02-26 13:18:50 $
                    $Revision: 1.3 $

 =============================================================================
 History:
 Author: David M. Kline
 -----------------------------------------------------------------------------
 2009-Jan-14  DMK  Derived support from drvAsynColby drivers.
 2009-Jan-23  DMK  Initial development complete.
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
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


/* EPICS system related include files */
#include <cantProceed.h>
#include <epicsAssert.h>
#include <epicsStdio.h>
#include <epicsString.h>
#include <epicsMutex.h>
#include <osiSock.h>
#include <iocsh.h>


/* EPICS synApps/Asyn related include files */
#include "asynDriver.h"
#include "asynOctet.h"

/* epicsExport.h must come last */
#include <epicsExport.h>


/* Define symbolic constants */
#define TIMEOUT         (3.0)
#define BUFFER_SIZE     (100)


/* Forward struct declarations */
typedef struct ioPvt ioPvt;


/* Declare interpose interface structure */
struct ioPvt
{
    char* ioport;
    int   addr;
    int   bsiz;
    float timeout;
    char  wbuf[BUFFER_SIZE];
    char  rbuf[BUFFER_SIZE];

    asynUser* pasynUser;
    asynInterface iface;

    void* octetPvt;
    asynOctet* pasynOctet;
};
    

/* Forward references for asynOctet methods */
static asynStatus writeIt(void *ppvt,asynUser *pasynUser,const char *data,size_t numchars,size_t *nbytesTransfered);
static asynStatus readIt(void *ppvt,asynUser *pasynUser,char *data,size_t maxchars,size_t *nbytesTransfered,int *eomReason);
static asynStatus flushIt(void *ppvt,asynUser *pasynUser);
static asynStatus registerInterruptUser(void *ppvt,asynUser *pasynUser,interruptCallbackOctet callback, void *userPvt,void **registrarPvt);
static asynStatus cancelInterruptUser(void *drvPvt,asynUser *pasynUser,void *registrarPvt);
static asynStatus setInputEos(void *ppvt,asynUser *pasynUser,const char *eos,int eoslen);
static asynStatus getInputEos(void *ppvt,asynUser *pasynUser,char *eos,int eossize ,int *eoslen);
static asynStatus setOutputEos(void *ppvt,asynUser *pasynUser,const char *eos,int eoslen);
static asynStatus getOutputEos(void *ppvt,asynUser *pasynUser,char *eos,int eossize,int *eoslen);

static asynOctet octet =
{
    writeIt,
    readIt,
    flushIt,
    registerInterruptUser,
    cancelInterruptUser,
    setInputEos,
    getInputEos,
    setOutputEos,
    getOutputEos
};


/* Define macros */
#define ASYN_OK(s)      (asynSuccess==(s))
#define ASYN_NOTOK(s)   (!ASYN_OK(s))
#ifndef MIN
#define MIN(a,b)        ((a)>(b)?(b):(a))
#endif


/****************************************************************************
 * Define public interface methods
 ****************************************************************************/
epicsShareFunc int nigpibInterposeConfig(const char *ioport,int addr,int bsiz,int timeout)
{
    int i,len;
    ioPvt* pioPvt;
    asynUser* pasynUser;
    asynInterface* pasynInterface;


    pasynUser = pasynManager->createAsynUser(0,0);
    if( pasynManager->connectDevice(pasynUser,ioport,0) )
    {
        printf("%s cannot connect to asyn port\n",ioport);
        pasynManager->freeAsynUser(pasynUser);

        return( -1 );
    }
    pasynInterface = pasynManager->findInterface(pasynUser,asynOctetType,1);
    if (pasynInterface==NULL)
    {
        printf("%s cannot find octet interface %s\n",ioport,pasynUser->errorMessage);
        pasynManager->freeAsynUser(pasynUser);

        return( -1 );
    }

    i = strlen(ioport)+1;
    len = i+sizeof(ioPvt);
    pioPvt = callocMustSucceed(1,len,"nigpibInterposeConfig");
    pioPvt->ioport = (char*)(pioPvt+1);

    pioPvt->addr = addr;
    pioPvt->bsiz = bsiz;
    strcpy(pioPvt->ioport,ioport);
    pioPvt->timeout = (timeout==0)?TIMEOUT:(float)(timeout*1.0);

    pioPvt->pasynUser = pasynUser;
    pioPvt->pasynUser->userPvt = pioPvt;

    pioPvt->iface.interfaceType = asynOctetType;
    pioPvt->iface.pinterface = &octet;
    pioPvt->iface.drvPvt = pioPvt;

    pioPvt->pasynOctet = (asynOctet*)pasynInterface->pinterface;
    pioPvt->octetPvt = pasynInterface->drvPvt;

    if( pasynManager->interposeInterface(ioport,0,&pioPvt->iface,&pasynInterface) )
    {
        printf("%s interposeInterface failed\n", ioport);
        pasynManager->freeAsynUser(pasynUser);
        free(pioPvt);

        return( -1 );
    }

    return( 0 );
}


/****************************************************************************
 * Define private interface asynOctet methods
 ****************************************************************************/
static asynStatus writeIt(void *ppvt,asynUser *pasynUser,const char *data,size_t numchars,size_t *nbytesTransfered)
{
    asynStatus status;
    ioPvt* pioPvt=(ioPvt*)ppvt;

    pasynUser->timeout = pioPvt->timeout;
    sprintf(pioPvt->wbuf,"wrt %d\n%s\r",pioPvt->addr,data);
    status = pioPvt->pasynOctet->write(pioPvt->octetPvt,pasynUser,pioPvt->wbuf,strlen(pioPvt->wbuf),nbytesTransfered);
    *nbytesTransfered=numchars;

    return( status );
}

static asynStatus readIt(void *ppvt,asynUser *pasynUser,char *data,size_t maxchars,size_t *nbytesTransfered,int *eomReason)
{
    asynStatus status;
    ioPvt* pioPvt=(ioPvt*)ppvt;

    pasynUser->timeout = pioPvt->timeout;

    sprintf(pioPvt->wbuf,"rd #%d %d\r",pioPvt->bsiz,pioPvt->addr);
    status = pioPvt->pasynOctet->write(pioPvt->octetPvt,pasynUser,pioPvt->wbuf,strlen(pioPvt->wbuf),nbytesTransfered);
    if( ASYN_NOTOK(status) ) return( status );

    status = pioPvt->pasynOctet->read(pioPvt->octetPvt,pasynUser,pioPvt->rbuf,(pioPvt->bsiz+4),nbytesTransfered,eomReason);
    if( ASYN_NOTOK(status) ) return( status );

    *strstr(pioPvt->rbuf,"\r\n")='\0';
    pioPvt->rbuf[*nbytesTransfered=MIN(strlen(pioPvt->rbuf),maxchars)]='\0';
    strcpy(data,pioPvt->rbuf);

    return( asynSuccess );
}

static asynStatus flushIt(void *ppvt,asynUser *pasynUser)
{
    ioPvt* pioPvt=(ioPvt*)ppvt;

    return( pioPvt->pasynOctet->flush(pioPvt->octetPvt,pasynUser) );
}

static asynStatus registerInterruptUser(void *ppvt,asynUser *pasynUser,interruptCallbackOctet callback,void *userPvt,void **registrarPvt)
{
    ioPvt* pioPvt=(ioPvt*)ppvt;

    return( pioPvt->pasynOctet->registerInterruptUser(pioPvt->octetPvt,pasynUser,callback,userPvt,registrarPvt) );
} 

static asynStatus cancelInterruptUser(void *ppvt,asynUser *pasynUser,void *registrarPvt)
{
    ioPvt* pioPvt=(ioPvt*)ppvt;

    return( pioPvt->pasynOctet->cancelInterruptUser(pioPvt->octetPvt,pasynUser,registrarPvt) );
} 

static asynStatus setInputEos(void *ppvt,asynUser *pasynUser,const char *eos,int eoslen)
{
    ioPvt *pioPvt=(ioPvt *)ppvt;

    return( pioPvt->pasynOctet->setInputEos(pioPvt->octetPvt,pasynUser,eos,eoslen) );
}

static asynStatus getInputEos(void *ppvt,asynUser *pasynUser,char *eos,int eossize,int *eoslen)
{
    ioPvt* pioPvt=(ioPvt*)ppvt;

    return( pioPvt->pasynOctet->getInputEos(pioPvt->octetPvt,pasynUser,eos,eossize,eoslen) );
}
static asynStatus setOutputEos(void *ppvt,asynUser *pasynUser,const char *eos,int eoslen)
{
    ioPvt* pioPvt=(ioPvt*)ppvt;

    return( pioPvt->pasynOctet->setOutputEos(pioPvt->octetPvt,pasynUser,eos,eoslen) );
}

static asynStatus getOutputEos(void *ppvt, asynUser *pasynUser,char *eos,int eossize,int *eoslen)
{
    ioPvt* pioPvt=(ioPvt*)ppvt;

    return( pioPvt->pasynOctet->getOutputEos(pioPvt->octetPvt,pasynUser,eos,eossize,eoslen) );
}


/****************************************************************************
 * Register public methods
 ****************************************************************************/

/* register method */
static const iocshArg arg0 = { "ioport",  iocshArgString };
static const iocshArg arg1 = { "addr",    iocshArgInt };
static const iocshArg arg2 = { "bsiz",    iocshArgInt };
static const iocshArg arg3 = { "timeout", iocshArgInt };
static const iocshArg *configArgs[] = {&arg0,&arg1,&arg2,&arg3};
static const iocshFuncDef configFuncDef = {"nigpibInterposeConfig",4,configArgs};
static void nigpibInterposeConfigCallFunc(const iocshArgBuf *args)
{
    nigpibInterposeConfig(args[0].sval,args[1].ival,args[2].ival,args[3].ival);
}

static void nigpibInterposeRegister(void)
{
    static int firstTime = 1;

    if( firstTime )
    {
        firstTime = 0;
        iocshRegister(&configFuncDef,nigpibInterposeConfigCallFunc);
    }
}
epicsExportRegistrar(nigpibInterposeRegister);
