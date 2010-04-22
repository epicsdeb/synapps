/* devTelevac.c
 Televac Vacuum Gauge Controller
 INP or OUT  has form @asyn(port address)command parameter
 port is the asyn serial port name
 address is the controller address (for futture usage in RS485)
 command is the pump command
 parameter is Station ID from 1-9 or relay ID 1-8 for the relay number
 

 Command Record Description
 0  -  AI      Read Pressure
 1  -  AI      Read relay ON pressure
 2  -  AI      Read relay OFF pressure
 3  -  BI      Read Relay status
*/

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#include <dbScan.h>
#include <dbDefs.h>
#include <dbAccess.h>
#include <dbCommon.h>
#include <alarm.h>
#include <link.h>
#include <recGbl.h>
#include <recSup.h>
#include <devSup.h>
#include <epicsString.h>
#include <errlog.h>
#include <asynDriver.h>
#include <asynEpicsUtils.h>
#include <asynOctet.h>
#include <aiRecord.h>
#include <biRecord.h>
#include <epicsExport.h>

#include "devTelevac.h"

typedef struct {
    int command;
    char *commandString;
} televacCommandStruct;

static televacCommandStruct televacCommands[MAX_Televac_COMMANDS] = {
    {GetPressure,       "GET_PRESSURE"},
    {GetRelayON,        "GET_RELAYON"},
    {GetRelayOFF,       "GET_RELAYOFF"},
    {GetRelayStatus,    "GET_RELAYSTATUS"}
};
 
typedef enum {opTypeInput, opTypeOutput} opType;
typedef enum {recTypeAi, recTypeBi} recType;

#define Televac_BUFFER_SIZE 25
#define Televac_TIMEOUT 1.0

typedef struct devTelevacPvt {
    asynUser     *pasynUser;
    asynOctet    *pasynOctet;
    void         *octetPvt;
    opType       opType;
    recType      recType;
    asynStatus   status;
    char         recBuf[Televac_BUFFER_SIZE];
    char         sendBuf[Televac_BUFFER_SIZE];
    char         address[3];
    char         parameter[2];
    int          command;
} devTelevacPvt;

typedef struct dsetTelevac{
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN io;
    DEVSUPFUN convert;
} dsetTelevac;

static long initCommon(dbCommon *pr, DBLINK *plink, opType ot, recType rt);
static long startIOCommon(dbCommon *pr);
static void devTelevacCallback(asynUser *pasynUser);
static long TelevacConvert(dbCommon* pr,int pass);

static long initAi(aiRecord *pr);
static long readAi(aiRecord *pr);

static long initBi(biRecord *pr);
static long readBi(biRecord *pr);

dsetTelevac devAiTelevac = {6,0,0,initAi,0,readAi,TelevacConvert};
epicsExportAddress(dset,devAiTelevac);

dsetTelevac devBiTelevac = {6,0,0,initBi,0,readBi,0};
epicsExportAddress(dset,devBiTelevac);


static long initCommon(dbCommon *pr, DBLINK *plink, opType ot, recType rt)
{
   char *port, *userParam;
   int i;
   int address;
   asynUser *pasynUser=NULL;
   asynStatus status;
   asynInterface *pasynInterface;
   devTelevacPvt *pPvt=NULL;
   char command[30];
   char *pstring;
   
   /* Allocate private structure */
   pPvt = calloc(1, sizeof(devTelevacPvt));
   pPvt->opType = ot;
   pPvt->recType = rt;

   /* Create an asynUser */
   pasynUser = pasynManager->createAsynUser(devTelevacCallback, 0);
   pasynUser->userPvt = pr;

   /* Parse link */
   status = pasynEpicsUtils->parseLink(pasynUser, plink,
                                       &port, &address, &userParam);
   if (status != asynSuccess) {
      errlogPrintf("devTelevac::initCommon %s bad link %s\n",
                   pr->name, pasynUser->errorMessage);
      goto bad;
   }

   status = pasynManager->connectDevice(pasynUser,port,0);
   if(status!=asynSuccess) goto bad;
   
   pasynInterface = pasynManager->findInterface(pasynUser,asynOctetType,1);
   if(!pasynInterface) goto bad;
   
   pPvt->pasynOctet = (asynOctet *)pasynInterface->pinterface;
   pPvt->octetPvt = pasynInterface->drvPvt;
   pPvt->pasynUser = pasynUser;
   pr->dpvt = pPvt;

   if ((userParam == NULL) || strlen(userParam) == 0) {
      errlogPrintf("devTelevac::initCommon %s invalid userParam %s\n",
                   pr->name, userParam);
      goto bad;
   }
   sscanf(userParam,"%s %d",command, &i);
   sprintf(pPvt->address,"%02X",address);
   sprintf(pPvt->parameter,"%d",i);
   for (i=0; i<MAX_Televac_COMMANDS; i++) {
        pstring = televacCommands[i].commandString;
        if (epicsStrCaseCmp(command, pstring) == 0) {
            pPvt->command = televacCommands[i].command;
            goto found;
        }
    }
    asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "devTelevac::init_common %s, unknown command=%s\n", 
              pr->name, command);
    goto bad;

   found:
   asynPrint(pasynUser, ASYN_TRACE_FLOW,
             "devTelevac::initCommon name=%s; command string=%s command=%d, address=%s; parameter=%s;\n",
             pr->name, command, pPvt->command, pPvt->address, pPvt->parameter);

   if (pPvt->command<0) {
       asynPrint(pasynUser, ASYN_TRACE_ERROR,
                 "devTelevac::initCommon %s illegal command=%d\n",
                 pr->name, pPvt->command);
       goto bad;
   }

   return 0;

bad:
   if(pasynUser) pasynManager->freeAsynUser(pasynUser);
   if(pPvt) free(pPvt);
   pr->pact = 1;
   return 0;
}

static long initAi(aiRecord *pr)
{
   return(initCommon((dbCommon *)pr, &pr->inp, opTypeInput, recTypeAi));
}


static long initBi(biRecord *pr)
{
   return(initCommon((dbCommon *)pr, &pr->inp, opTypeInput, recTypeBi));
}



static long readAi(aiRecord *pr)
{
    devTelevacPvt *pPvt = (devTelevacPvt *)pr->dpvt;
    int rtnSize;
    int exp;
    char sign;
    char unit;
    float value=0;
    int temp;
/*
    char *pdata = pPvt->recBuf;
*/
    if (!pr->pact) {

	memset(pPvt->sendBuf, 0, Televac_BUFFER_SIZE);
        switch (pPvt->command) {
        case GetPressure:
            strcpy(pPvt->sendBuf, "R");
            strcat(pPvt->sendBuf, pPvt->parameter);
            break;
        case GetRelayON:
            strcpy(pPvt->sendBuf, "SP");
            strcat(pPvt->sendBuf, pPvt->parameter);
            strcat(pPvt->sendBuf, "N");
            break;
        case GetRelayOFF:
            strcpy(pPvt->sendBuf, "SP");
            strcat(pPvt->sendBuf, pPvt->parameter);
            strcat(pPvt->sendBuf, "F");
            break;
        default:
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                      "devTelevac::readAi %s Wrong record type \n",
                      pr->name);
            break;
        }
        asynPrint(pPvt->pasynUser, ASYN_TRACEIO_DEVICE,
              "devTelevac::readAi %s command %d len=%d string=|%s|\n",
              pr->name, pPvt->command, strlen(pPvt->sendBuf), pPvt->sendBuf);

        return(startIOCommon((dbCommon *)pr));
    }

    rtnSize = strlen(pPvt->recBuf);
    switch (pPvt->command) {
        case GetPressure:
        case GetRelayON:
        case GetRelayOFF:
	    if (rtnSize == 9) {
	        /*  the reply is n=n.nn-(+)eT */
	        sscanf(pPvt->recBuf,"%d=%f%c%x%c",&temp,&value,&sign,&exp,&unit);
	    } else if (rtnSize == 6 || rtnSize == 5) {
	        /*  the reply is n.nn-(+)e  or n.n-(+)e */
	        sscanf(pPvt->recBuf,"%f%c%x",&value,&sign,&exp);
	    } else {
	        value = 9.9;
	        sign = '+';
	        exp=9; 
	    }
	    if (exp == 11) {
	        value = 9.9;
	        sign = '+';
	        exp=9; 
	    }
	
            if ( sign == '-')
  	        exp -= exp*2;

    	    pr->val = (double) value * pow(10,exp);
            break;
        default:
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                      "devTelevac::readAi %s Wrong record type \n",
                      pr->name);
            break;
    }
    pr->udf=0;
    return(2);
}


static long readBi(biRecord *pr)
{
    devTelevacPvt *pPvt = (devTelevacPvt *)pr->dpvt;
    int rtnSize;
    int id;
    char *pvalue;
    int value=0;

    if (!pr->pact) {

	memset(pPvt->sendBuf, 0, Televac_BUFFER_SIZE);
        switch (pPvt->command) {
        case GetRelayStatus:
            strcpy(pPvt->sendBuf, "RY");
            break;
        default:
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                      "devTelevac::readBi %s Wrong record type \n",
                      pr->name);
            break;
        }
        asynPrint(pPvt->pasynUser, ASYN_TRACEIO_DEVICE,
              "devTelevac::readBi %s command %d len=%d string=|%s|\n",
              pr->name, pPvt->command, strlen(pPvt->sendBuf), pPvt->sendBuf);
        return(startIOCommon((dbCommon *)pr));
    }
    rtnSize = strlen(pPvt->recBuf);
    sscanf(pPvt->parameter,"%d",&id);
    
    switch (pPvt->command) {
        case GetRelayStatus:
            if(pPvt->recBuf[0] == 'n')
  		pPvt->recBuf[0] = '0';
            if(pPvt->recBuf[1] == 'n')
  		pPvt->recBuf[0] = '0';
            pvalue = &pPvt->recBuf[0];
            sscanf(pvalue, "%x", &value);
            pr->rval = value & (1 << (id-1));
            break;

        default:
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                      "devTelevac::readBi %s Wrong record type \n",
                      pr->name);
            break;
    }
    pr->udf=0;
    return(0);
}



static long startIOCommon(dbCommon* pr)
{
   devTelevacPvt *pPvt = (devTelevacPvt *)pr->dpvt;
   asynUser *pasynUser = pPvt->pasynUser;
   int status;

   pr->pact = 1;
   status = pasynManager->queueRequest(pasynUser, 0, 0);
   if (status != asynSuccess) status = -1;
   return(status);
}

static void devTelevacCallback(asynUser *pasynUser)
{
    dbCommon *pr = (dbCommon *)pasynUser->userPvt;
    devTelevacPvt *pPvt = (devTelevacPvt *)pr->dpvt;
    char readBuffer[Televac_BUFFER_SIZE];
    struct rset *prset = (struct rset *)(pr->rset);
    size_t nread, nwrite; 
    int eomReason;

    pPvt->pasynUser->timeout = Televac_TIMEOUT;
    pPvt->status = pPvt->pasynOctet->write(pPvt->octetPvt, pasynUser, 
                                           pPvt->sendBuf, strlen(pPvt->sendBuf), 
                                           &nwrite);
    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
              "devTelevac::devTelevacCallback %s nwrite=%d, output=%s\n",
              pr->name, nwrite, pPvt->sendBuf);
    pPvt->status = pPvt->pasynOctet->read(pPvt->octetPvt, pasynUser, 
                                          readBuffer, Televac_BUFFER_SIZE, 
                                          &nread, &eomReason);
    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
              "devTelevac::devTelevacCallback %s nread=%d, input=%s\n",
              pr->name, nwrite, readBuffer);
    if (nread < 1) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devTelevac::devTelevacCallback %s message too small=%d\n", 
                  pr->name, nread);
        recGblSetSevr(pr, READ_ALARM, INVALID_ALARM);
    }

    memset(pPvt->recBuf, 0, Televac_BUFFER_SIZE);
    strcpy(pPvt->recBuf,readBuffer);
    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
              "devTelevac: %s command (%d) received (after processing) |%s|\n",
              pr->name, pPvt->command, pPvt->recBuf);

    /* Process the record. This will result in the readX or writeX routine
       being called again, but with pact=1 */
    dbScanLock(pr);
    (*prset->process)(pr);
    dbScanUnlock(pr);
}


static long TelevacConvert(dbCommon* pr,int pass)
{
   aiRecord* pai = (aiRecord*)pr;
   pai->eslo=1.0;
   pai->roff=0;
   return 0;
}
