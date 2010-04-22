/* devTPG261.c
 * Modifications:
 * Mohan Ramanathan   v1.0 9/20/2006
   This driver supports both TPG261 and TPG262 devices.  Only difference 
   between the two is that one supports one gauge and the other supports
   two gauges.  the protocol is the same for both. 
 Protocol:
 INP or OUT  has form @asyn(port address)command
 port is the asyn serial port name
 address has to either 1 or 2 for TPG261/TPG262 devices
 command is the pump command

 Command Record Description
 0  -  SI      Read Gauge ID
 1  -  AI      Read Pressure

 2  -  MBBI    Read Pressure Units
 3  -  MBBO    Set Pressure Units
 
 4  -  BI      Read Sensor On/Off
 5  -  BO      Set Sensor On/Off


 6  -  AI      Read Setpoint value 1
 7  -  BI      Read On/Off of setpoint 1
 8  -  AO      Set Setpoint 1

 9  -  AI      Read Setpoint value 2
 10 -  BI      Read On/Off of setpoint 2
 11 -  AO      Set Setpoint 2

*/

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

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
#include <asynDriver.h>
#include <asynEpicsUtils.h>
#include <errlog.h>
#include <asynOctet.h>
#include <aiRecord.h>
#include <biRecord.h>
#include <mbbiRecord.h>
#include <stringinRecord.h>
#include <aoRecord.h>
#include <boRecord.h>
#include <mbboRecord.h>
#include <epicsExport.h>

#include "devTPG261.h"

typedef struct {
    int command;
    char *commandString;
} tpg261CommandStruct;

static tpg261CommandStruct tpg261Commands[MAX_TPG261_COMMANDS] = {
    {GetID,         "GET_ID"},
    {GetPressure,   "GET_PRESSURE"},
    {GetUnit,       "GET_UNIT"},
    {SetUnit,       "SET_UNIT"},
    {GetSensor,     "GET_SENSOR"},
    {SetSensor,     "SET_SENSOR"},
    {GetSpVal1,     "GET_SPVAL1"},
    {GetSpS1,       "GET_SPS1"},
    {SetSp1,        "SET_SP1"},
    {GetSpVal2,     "GET_SPVAL2"},
    {GetSpS2,       "GET_SPS2"},
    {SetSp2,        "SET_SP2"}
};
/*
#define TPG261_BUFFER_SIZE 32
*/
#define TPG261_BUFFER_SIZE 64
#define TPG261_TIMEOUT 2.0

typedef struct devTPG261Pvt {
    asynUser     *pasynUser;
    asynOctet    *pasynOctet;
    void         *octetPvt;
    asynStatus   status;
    char         recBuf[TPG261_BUFFER_SIZE];
    char         sendBuf[TPG261_BUFFER_SIZE];
    int          command;
    int		 address;
} devTPG261Pvt;

typedef struct dsetTPG261{
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN io;
    DEVSUPFUN convert;
} dsetTPG261;

static long initCommon(dbCommon *pr, DBLINK *plink);
static long startIOCommon(dbCommon *pr);
static void devTPG261Callback(asynUser *pasynUser);
static long TPG261Convert(dbCommon* pr,int pass);
static int buildCommand(devTPG261Pvt *pPvt, char *pvalue);

static long initAi(aiRecord *pr);
static long readAi(aiRecord *pr);

static long initBi(biRecord *pr);
static long readBi(biRecord *pr);

static long initMbbi(mbbiRecord *pr);
static long readMbbi(mbbiRecord *pr);

static long initSi(stringinRecord *pr);
static long readSi(stringinRecord *pr);

static long initAo(aoRecord *pr);
static long writeAo(aoRecord *pr);

static long initBo(boRecord *pr);
static long writeBo(boRecord *pr);

static long initMbbo(mbboRecord *pr);
static long writeMbbo(mbboRecord *pr);

dsetTPG261 devAiTPG261 = {6,0,0,initAi,0,readAi,TPG261Convert};
epicsExportAddress(dset,devAiTPG261);

dsetTPG261 devBiTPG261 = {6,0,0,initBi,0,readBi,0};
epicsExportAddress(dset,devBiTPG261);

dsetTPG261 devMbbiTPG261 = {6,0,0,initMbbi,0,readMbbi,0};
epicsExportAddress(dset,devMbbiTPG261);

dsetTPG261 devSiTPG261 = {6,0,0,initSi,0,readSi,0};
epicsExportAddress(dset,devSiTPG261);

dsetTPG261 devAoTPG261 = {6,0,0,initAo,0,writeAo,TPG261Convert};
epicsExportAddress(dset,devAoTPG261);

dsetTPG261 devBoTPG261 = {6,0,0,initBo,0,writeBo,0};
epicsExportAddress(dset,devBoTPG261);

dsetTPG261 devMbboTPG261 = {6,0,0,initMbbo,0,writeMbbo,0};
epicsExportAddress(dset,devMbboTPG261);


static long initCommon(dbCommon *pr, DBLINK *plink)
{
   char *port, *userParam;
   int i;
   int address;
   asynUser *pasynUser=NULL;
   asynStatus status;
   asynInterface *pasynInterface;
   devTPG261Pvt *pPvt=NULL;
   char command[30];
   char *pstring;
   
   /* Allocate private structure */
   pPvt = calloc(1, sizeof(devTPG261Pvt));

   /* Create an asynUser */
   pasynUser = pasynManager->createAsynUser(devTPG261Callback, 0);
   pasynUser->userPvt = pr;

   /* Parse link */
   status = pasynEpicsUtils->parseLink(pasynUser, plink,
                                       &port, &address, &userParam);
   if (status != asynSuccess) {
      errlogPrintf("devTPG261::initCommon %s bad link %s\n",
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
      errlogPrintf("devTPG261::initCommon %s invalid userParam %s\n",
                   pr->name, userParam);
      goto bad;
   }
   pPvt->address = address;
   sscanf(userParam,"%s",command);
   for (i=0; i<MAX_TPG261_COMMANDS; i++) {
        pstring = tpg261Commands[i].commandString;
        if (epicsStrCaseCmp(command, pstring) == 0) {
            pPvt->command = tpg261Commands[i].command;
            goto found;
        }
   }
   asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "devTPG261::init_common %s, unknown command=%s\n", 
              pr->name, command);
   goto bad;

found:
   asynPrint(pasynUser, ASYN_TRACE_FLOW,
             "devTPG261::initCommon name=%s; command string=%s command=%d address=%d\n",
             pr->name, command, pPvt->command, pPvt->address);

   if (pPvt->command<0 || pPvt->command >MAX_TPG261_COMMANDS) {
       asynPrint(pasynUser, ASYN_TRACE_ERROR,
                 "devTPG261::initCommon %s illegal command=%d\n",
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
   return(initCommon((dbCommon *)pr, &pr->inp));
}

static long initBi(biRecord *pr)
{
   return(initCommon((dbCommon *)pr, &pr->inp));
}

static long initMbbi(mbbiRecord *pr)
{
   return(initCommon((dbCommon *)pr, &pr->inp));
}

static long initSi(stringinRecord *pr)
{
   return(initCommon((dbCommon *)pr, &pr->inp));
}

static long initAo(aoRecord *pr)
{
   return(initCommon((dbCommon *)pr, &pr->out));
}

static long initBo(boRecord *pr)
{
   return(initCommon((dbCommon *)pr, &pr->out));
}

static long initMbbo(mbboRecord *pr)
{
   return(initCommon((dbCommon *)pr, &pr->out));
}



static int buildCommand(devTPG261Pvt *pPvt, char *pvalue)
{
    asynUser *pasynUser = pPvt->pasynUser;
    dbCommon *pr = (dbCommon *)pasynUser->userPvt;
/*
    The TPG261 commands are of the form :  
    Send:  "xx<CR><LF>" ( <LF> is optional )
    Reply: "<ACK><CR><LF>" or "<NAK><CR><LF>"
    Send:  "<ENQ>"
    Reply: "yyyy<CR><LF>"
    
    for all 
    XX = 3 letter command and [,data if any]
    yyy = response data for the request
*/

    memset(pPvt->sendBuf, 0, TPG261_BUFFER_SIZE);
    strcpy(pPvt->sendBuf, pvalue);
    strcat(pPvt->sendBuf, "\r"); 

    asynPrint(pPvt->pasynUser, ASYN_TRACEIO_DEVICE,
              "devTPG261::buildCommand %s len=%d string=|%s|\n",
              pr->name, strlen(pPvt->sendBuf), pPvt->sendBuf);

    return(0);
}

static long readAi(aiRecord *pr)
{
    devTPG261Pvt *pPvt = (devTPG261Pvt *)pr->dpvt;
    char tempcmd[5];
    int stptNo;
    char *pvalue = pPvt->recBuf;
    float value=0;
    float value2=0;
    int status=0;

    if (!pr->pact) {
        /* For setpoint readback set the correct setpoint number.
         * setpoints 1 and 2 for gauge 1 and 3 and 4 for gauge 2*/
        switch (pPvt->command) {
          case GetPressure:
            sprintf(tempcmd, "PR%d", pPvt->address);
            break;
          case GetSpVal1:
            stptNo = (2* pPvt->address) - 1;
            sprintf(tempcmd,"SP%d",stptNo);
            break;
          case GetSpVal2:
            stptNo = 2* pPvt->address;
            sprintf(tempcmd,"SP%d",stptNo);
            break;
          default:
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                      "devTPG261::readAi %s Wrong record type \n",
                      pr->name);
            break;
        }
        buildCommand(pPvt, tempcmd);
        return(startIOCommon((dbCommon *)pr));
    }

    switch (pPvt->command) {
        case GetPressure:
	    sscanf(pvalue, "%d,%E", &status,&value);
	    if (status == 4) {
        	recGblSetSevr(pr, READ_ALARM, MINOR_ALARM);
	    } 
	    if ( status != 0 && status != 4) {
        	recGblSetSevr(pr, READ_ALARM, INVALID_ALARM);
        	asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                     	"devTPG261::readAi Gauge error %s status =[%d] \n",
                      	pr->name, status);
	    }
	    break;
        case GetSpVal1:
        case GetSpVal2:
	    sscanf(pvalue, "%d,%E,%E", &status,&value,&value2);
            break;
        default:
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                      "devTPG261::readAi %s Wrong record type \n",
                      pr->name);
           break;
    }
    pr->val = value;
    pr->udf=0;
    return(2);
}


static long readBi(biRecord *pr)
{
    devTPG261Pvt *pPvt = (devTPG261Pvt *)pr->dpvt;
    char tempcmd[5];
    int stptNo;
    char *pvalue = pPvt->recBuf;
    int vals[3];
    int value=0;

    if (!pr->pact) {
        /* For setpoint readback set the correct setpoint number.
         * setpoints 1 and 2 for gauge 1 and 3 and 4 for gauge 2*/
        switch (pPvt->command) {
          case GetSpS1:
          case GetSpS2:
            strcpy(tempcmd, "SPS");
            break;
          case GetSensor:
            strcpy(tempcmd, "SEN");
            break;
          default:
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                      "devTPG261::readBi %s Wrong record type \n",
                      pr->name);
            break;
        }
        buildCommand(pPvt, tempcmd);
        return(startIOCommon((dbCommon *)pr));
    }

    switch (pPvt->command) {
        case GetSpS1:
            stptNo = (2* pPvt->address) - 2;
            sscanf(pvalue,"%d,%d,%d,%d",&vals[0],&vals[1],&vals[2],&vals[3]);
            value = vals[stptNo];
            break;
        case GetSpS2:
            stptNo = (2* pPvt->address) - 1;
            sscanf(pvalue,"%d,%d,%d,%d",&vals[0],&vals[1],&vals[2],&vals[3]);
            value = vals[stptNo];
            break;
        case GetSensor:
            sscanf(pvalue,"%d,%d",&vals[0],&vals[1]);
            value = vals[pPvt->address - 1];
            break;
        default:
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                      "devTPG261::readBi %s Wrong record type \n",
                      pr->name);
            break;
    }
    pr->rval = value;
    pr->udf=0;
    return(0);
}

static long readMbbi(mbbiRecord *pr)
{
    devTPG261Pvt *pPvt = (devTPG261Pvt *)pr->dpvt;
    char tempcmd[5];
    char *pvalue = pPvt->recBuf;
    int value=0;

    if (!pr->pact) {
        switch (pPvt->command) {
          case GetUnit:
            strcpy(tempcmd, "UNI");
            break;
          default:
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                      "devTPG261::readMbbi %s Wrong record type \n",
                      pr->name);
            break;
        }
        buildCommand(pPvt, tempcmd);
        return(startIOCommon((dbCommon *)pr));
    }
    switch (pPvt->command) {
        case GetUnit:
            sscanf(pvalue,"%d",&value);
            break;
        default:
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                      "devTPG261::readMbbi %s Wrong record type \n",
                      pr->name);
            break;
    }
    pr->rval = value;
    pr->udf=0;
    return(0);
}

static long readSi(stringinRecord *pr)
{
    devTPG261Pvt *pPvt = (devTPG261Pvt *)pr->dpvt;
    char tempcmd[5];
    int rtnSize;
    char pvalue[10];
    char *ploc;
    char *cloc;

    if (!pr->pact) {
        switch (pPvt->command) {
          case GetID:
            strcpy(tempcmd, "TID");
            break;
          default:
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                      "devTPG261::readSi %s Wrong record type \n",
                      pr->name);
            break;
        }
        buildCommand(pPvt, tempcmd);
        return(startIOCommon((dbCommon *)pr));
    }
    rtnSize = strlen(pPvt->recBuf);
    switch (pPvt->command) {
        case GetID:
            cloc =strchr(pPvt->recBuf, ',');
            if (pPvt->address == 1) {
                ploc = pPvt->recBuf;
                *cloc = 0;
                strcpy(pvalue,ploc);
            } else {
                ploc  = cloc+1;
                strcpy(pvalue,ploc);
            }
        break;
    }
    strcpy(pr->val, pvalue);
    pr->udf=0;
    return(0);
}


static long writeAo(aoRecord *pr)
{
    devTPG261Pvt *pPvt = (devTPG261Pvt *)pr->dpvt;
    char tempcmd[30]="";
    char pvalue[15]="";
    int stptNo;
    int rtnSize;
    
    if (!pr->pact) {
        /* For setting setpoint the command is of the form :SPn,s,x.xxxxE-yy,x.xxxxE-yy
         * n - setpoint number 1-4.
         * s - 0 or 1 for measued channel 1 or 2
         * values are for lower and upper and will be set the same as pr->val field.
        */

        switch (pPvt->command) {
          case SetSp1:
            stptNo = (2* pPvt->address) - 1;
            sprintf(tempcmd,"SP%d,%d,",stptNo,(pPvt->address-1));
            sprintf(pvalue, "%8.4E", pr->val);
            strcat(tempcmd, pvalue);  /* for lower pressure */
            strcat(tempcmd, ",");
            strcat(tempcmd, pvalue);  /* for upper pressure */
            break;
          case SetSp2:
            stptNo = (2* pPvt->address);
            sprintf(tempcmd,"SP%d,%d,",stptNo,(pPvt->address-1));
            sprintf(pvalue, "%8.4E", pr->val);
            strcat(tempcmd, pvalue);  /* for lower pressure */
            strcat(tempcmd, ",");
            strcat(tempcmd, pvalue);  /* for upper pressure */
            break;
          default:
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                      "devTPG261::writeAo %s Wrong record type \n",
                      pr->name);
            break;
        }
        buildCommand(pPvt, tempcmd);
        return(startIOCommon((dbCommon *)pr));
    }
    rtnSize = strlen(pPvt->recBuf);
    if (rtnSize < 1) {
        recGblSetSevr(pr, READ_ALARM, INVALID_ALARM);
        asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR, 
                  "devTPG261::writeAo reply message error %s\n",
                  pr->name);
        return(2);
    }
    pr->udf=0;
    return(2);
}

static long writeBo(boRecord *pr)
{
    devTPG261Pvt *pPvt = (devTPG261Pvt *)pr->dpvt;
    char tempcmd[10]="";
    int rtnSize;
    
    if (!pr->pact) {
        switch (pPvt->command) {
          case SetSensor:
	    strcpy(tempcmd,"SEN,");
	    if (pPvt->address == 1) {
		sprintf(tempcmd,"SEN,%d,0",pr->val+1);
	    } else {
		sprintf(tempcmd,"SEN,0,%d",pr->val+1);
	    }
            break;
          default:
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                      "devTPG261::writeBo %s Wrong record type \n",
                      pr->name);
            break;
        }
        buildCommand(pPvt, tempcmd);
        return(startIOCommon((dbCommon *)pr));
    }
    rtnSize = strlen(pPvt->recBuf);
    if (rtnSize < 1) {
        recGblSetSevr(pr, READ_ALARM, INVALID_ALARM);
        asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR, 
                  "devTPG261::writeBo reply message error %s\n",
                  pr->name);
        return(0);
    }
    pr->udf=0;
    return(0);
}

static long writeMbbo(mbboRecord *pr)
{
    devTPG261Pvt *pPvt = (devTPG261Pvt *)pr->dpvt;
    char tempcmd[8];
    int rtnSize;
    
    if (!pr->pact) {
        switch (pPvt->command) {
          case SetUnit:
            sprintf(tempcmd, "UNI,%d", pr->rval);
            break;
          default:
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                      "devTPG261::writeMbbo %s Wrong record type \n",
                      pr->name);
            break;
        }
        buildCommand(pPvt, tempcmd);
        return(startIOCommon((dbCommon *)pr));
    }
    rtnSize = strlen(pPvt->recBuf);
    if (rtnSize < 1) {
        recGblSetSevr(pr, READ_ALARM, INVALID_ALARM);
        asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR, 
                  "devTPG261::writeMbbo reply message error %s\n",
                  pr->name);
        return(0);

    }
    pr->udf=0;
    return(0);
}


static long startIOCommon(dbCommon* pr)
{
   devTPG261Pvt *pPvt = (devTPG261Pvt *)pr->dpvt;
   asynUser *pasynUser = pPvt->pasynUser;
   int status;

   pr->pact = 1;
   status = pasynManager->queueRequest(pasynUser, 0, 0);
   if (status != asynSuccess) status = -1;
   return(status);
}

static void devTPG261Callback(asynUser *pasynUser)
{
    dbCommon *pr = (dbCommon *)pasynUser->userPvt;
    devTPG261Pvt *pPvt = (devTPG261Pvt *)pr->dpvt;
    char readBuffer[TPG261_BUFFER_SIZE];
    struct rset *prset = (struct rset *)(pr->rset);

    size_t nread, nwrite;
    int eomReason;


    /*  for reply of data we need <CR><LF> and for enquiring about
    the data we need to send <ENQ> ( ascii 5) */
    
    char inputEos[5];
    char enq[2];
    char nak[2];
    char ack[2];
    strcpy(inputEos,"\r\n");
    strcpy(enq,"\05");
    strcpy(nak,"\21");   
    strcpy(ack,"\06");
    
    pPvt->status = pPvt->pasynOctet->setInputEos(pPvt->octetPvt, pasynUser, 
                            inputEos, strlen(inputEos));

    pPvt->pasynUser->timeout = TPG261_TIMEOUT;

    pPvt->status = pPvt->pasynOctet->write(pPvt->octetPvt, pasynUser, 
                                           pPvt->sendBuf, strlen(pPvt->sendBuf), 
                                           &nwrite);
    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
              "devTPG261::devTPG261Callback Cmd %s nwrite=%d, output=[%s]\n",
              pr->name, nwrite, pPvt->sendBuf);
    pPvt->status = pPvt->pasynOctet->read(pPvt->octetPvt, pasynUser, 
                                          readBuffer, TPG261_BUFFER_SIZE, 
                                          &nread, &eomReason);
    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
              "devTPG261::devTPG261Callback Cmd %s nread=%d, input=[%s]\n",
              pr->name, nread, readBuffer);
    if (nread < 1 ) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devTPG261::devTPG261Callback Cmd %s message too small=%d\n", 
                  pr->name, nread);
        recGblSetSevr(pr, READ_ALARM, INVALID_ALARM);
	goto finish;
    }

/*
    if (strcmp(readBuffer,ack) == 0) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devTPG261::devTPG261Callback Cmd %s message Good \n", 
                  pr->name);
    }
*/

/*  if <NAK> is received then error in command */
    if (strcmp(readBuffer,nak) == 0) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devTPG261::devTPG261Callback Cmd %s message Error \n", 
                  pr->name);
        recGblSetSevr(pr, READ_ALARM, INVALID_ALARM);
	goto finish;
    }

/*  Now send the <ENQ> command to get the data */

    pPvt->status = pPvt->pasynOctet->write(pPvt->octetPvt, pasynUser, 
                                          enq, strlen(enq), &nwrite);
    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
              "devTPG261::devTPG261Callback Val %s nwrite=%d, output=[%c]\n",
              pr->name, nwrite, enq);
    pPvt->status = pPvt->pasynOctet->read(pPvt->octetPvt, pasynUser, 
                                          readBuffer, TPG261_BUFFER_SIZE, 
                                          &nread, &eomReason);
    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
              "devTPG261::devTPG261Callback Val %s nread=%d, input=[%s]\n",
              pr->name, nread, readBuffer);

    if (nread < 1 ) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devTPG261::devTPG261Callback Val %s message too small=%d\n", 
                  pr->name, nread);
        recGblSetSevr(pr, READ_ALARM, INVALID_ALARM);
	goto finish;
    }
    memset(pPvt->recBuf, 0, TPG261_BUFFER_SIZE);
    strcpy(pPvt->recBuf,readBuffer);
    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
              "devTPG261: %s command (%d) received (after processing) |%s|\n",
              pr->name, pPvt->command, pPvt->recBuf);

    /* Process the record. This will result in the readX or writeX routine
       being called again, but with pact=1 */
finish:
    dbScanLock(pr);
    (*prset->process)(pr);
    dbScanUnlock(pr);
}


static long TPG261Convert(dbCommon* pr,int pass)
{
   aiRecord* pai = (aiRecord*)pr;
   pai->eslo=1.0;
   pai->roff=0;
   return 0;
}
