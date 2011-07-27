/* devMPC.c
 * Modifications:
 * Mark Rivers  17-Feb-2001  Added support for TSP and auto-restart
 * Mark Rivers  26-Oct-2002  Fixed problem with reading AMPS on recent MPC
 *                           controllers, they don't send AMPS in the response
 * Mark Rivers  1-Sep-2003   Changed software to use normal mpfSerial server,
 *                           rather than custom server
 * Mark Rivers  22-Sep-2004  Changed from MPF to asyn, and from C++ to C

 INP or OUT  has form @asyn(port address)command parameter
 port is the asyn serial port name
 address is the pump address 
 command is the pump command
 parameter is 1 or 2 depending on which of the two XXX we use

 Command Record Description
 0  -  SI      Pump Status
 1  -  AI      Read Pressure
 2  -  AI      Read Current
 3  -  AI      Read Voltage
 4  -  AI      Read Pump Size
 5  -  AI      Read Setpoint value 1 or 2
 6  -  BI      Read On/Off of setpoint 1 or 2
 7  -  AI      Read Setpoint value 3 or 4
 8  -  BI      Read On/Off of setpoint 3 or 4
 9  -  AI      Read Setpoint value 5 or 6
 10 -  BI      Read On/Off of setpoint 5 or 6
 11 -  AI      Read Setpoint value 7 or 8
 12 -  BI      Read On/Off of setpoint 7 or 8
 13 -  BI      Read auto-restart status
 14 -  SI      Read TSP status

 20 -  MBBO    Set Pressure Units
 21 -  MBBO    Set Display
 22 -  AO      Set Pump Size
 23 -  AO      Set Setpoint 1 or 2
 24 -  AO      Set Setpoint 3 or 4
 25 -  AO      Set Setpoint 5 or 6
 26 -  AO      Set Setpoint 7 or 8
 27 -  BO      Start /Stop Pump
 28 -          Used by start/stop pump
 29 -  BO      Keyboard lock/unlock
 30 -          Used by keyboard lock/unlock
 31 -  BO      Auto-restart on/off
 32 -  SO      TSP timed mode on
 33 -  BO      TSP off
 34 -  MBBO    Select TSP filament
 35 -  BO      TSP filament clear
 36 -  BO      TSP filament auto-advance on/off
 37 -  BO      TSP continuous on/off
 38 -  SO      Set TSP sublimation parameters
 39 -  BO      TSP degass
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
#include <errlog.h>
#include <asynDriver.h>
#include <asynEpicsUtils.h>
#include <asynOctet.h>
#include <aiRecord.h>
#include <aoRecord.h>
#include <biRecord.h>
#include <boRecord.h>
#include <mbboRecord.h>
#include <stringinRecord.h>
#include <stringoutRecord.h>
#include <epicsExport.h>

#include "devMPC.h"

typedef struct {
    int command;
    char *commandString;
} mpcCommandStruct;

static mpcCommandStruct mpcCommands[MAX_MPC_COMMANDS] = {
    {GetStatus,         "GET_STATUS"},
    {GetPres,           "GET_PRESSURE"},
    {GetCur,            "GET_CURRENT"},
    {GetVolt,           "GET_VOLT"},
    {GetSize,           "GET_SIZE"},
    {GetSpVal12,        "GET_SPVAL12"},
    {GetSpS12,          "GET_SPS12"},
    {GetSpVal34,        "GET_SPVAL34"},
    {GetSpS34,          "GET_SPS34"},
    {GetSpVal56,        "GET_SPVAL56"},
    {GetSpS56,          "GET_SPS56"},
    {GetSpVal78,        "GET_SPVAL78"},
    {GetSpS78,          "GET_SPS78"},
    {GetAutoRestart,    "GET_AUTO_RESTART"},
    {GetTSPStat,        "GET_TSP_STATUS"},
    {SetUnit,           "SET_UNIT"},
    {SetDis,            "SET_DISPLAY"},
    {SetSize,           "SET_SIZE"},
    {SetSp12,           "SET_SP12"},
    {SetSp34,           "SET_SP34"},
    {SetSp56,           "SET_SP56"},
    {SetSp78,           "SET_SP78"},
    {SetStart,          "SET_START"},
    {SetStop,           "SET_STOP"},
    {SetLock,           "SET_LOCK"},
    {SetUnlock,         "SET_UNLOCK"},
    {SetAutoRestart,    "SET_AUTO_RESTART"},
    {SetTSPTimed,       "SET_TSP_TIMED"},
    {SetTSPOff,         "SET_TSP_OFF"},
    {SetTSPFilament,    "SET_TSP_FILAMENT"},
    {SetTSPClear,       "SET_TSP_CLEAR"},
    {SetTSPAutoAdv,     "SET_TSP_AUTO_ADVANCE"},
    {SetTSPContinuous,  "SET_TSP_CONTINUOUS"},
    {SetTSPSublimation, "SET_TSP_SUBLIMATION"},
    {SetTSPDegas,       "SET_TSP_DEGAS"}
};
 
typedef enum {opTypeInput, opTypeOutput} opType;
typedef enum {recTypeAi, recTypeAo, recTypeBi, recTypeBo,
              recTypeMbbo, recTypeSi, recTypeSo} recType;

#define MPC_BUFFER_SIZE 50
#define MPC_TIMEOUT 3.0

typedef struct devMPCPvt {
    asynUser     *pasynUser;
    asynOctet    *pasynOctet;
    void         *octetPvt;
    opType       opType;
    recType      recType;
    asynStatus   status;
    char         recBuf[MPC_BUFFER_SIZE];
    char         sendBuf[MPC_BUFFER_SIZE];
    char         address[3];
    char         parameter[2];
    int          command;
} devMPCPvt;

typedef struct dsetMPC{
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN io;
    DEVSUPFUN convert;
} dsetMPC;

static long initCommon(dbCommon *pr, DBLINK *plink, opType ot, recType rt);
static long startIOCommon(dbCommon *pr);
static void devMPCCallback(asynUser *pasynUser);
static long MPCConvert(dbCommon* pr,int pass);
static int buildCommand(devMPCPvt *pPvt, int hexCmd, char *pvalue);

static long initAi(aiRecord *pr);
static long readAi(aiRecord *pr);
static long initAo(aoRecord *pr);
static long writeAo(aoRecord *pr);
static long initBi(biRecord *pr);
static long readBi(biRecord *pr);
static long initBo(boRecord *pr);
static long writeBo(boRecord *pr);
static long initMbbo(mbboRecord *pr);
static long writeMbbo(mbboRecord *pr);
static long initSi(stringinRecord *pr);
static long readSi(stringinRecord *pr);
static long initSo(stringoutRecord *pr);
static long writeSo(stringoutRecord *pr);
dsetMPC devAiMPC = {6,0,0,initAi,0,readAi,MPCConvert};
epicsExportAddress(dset,devAiMPC);
dsetMPC devAoMPC = {6,0,0,initAo,0,writeAo,MPCConvert};
epicsExportAddress(dset,devAoMPC);
dsetMPC devBiMPC = {6,0,0,initBi,0,readBi,0};
epicsExportAddress(dset,devBiMPC);
dsetMPC devBoMPC = {6,0,0,initBo,0,writeBo,0};
epicsExportAddress(dset,devBoMPC);
dsetMPC devMbboMPC = {6,0,0,initMbbo,0,writeMbbo,0};
epicsExportAddress(dset,devMbboMPC);
dsetMPC devSiMPC = {6,0,0,initSi,0,readSi,0};
epicsExportAddress(dset,devSiMPC);
dsetMPC devSoMPC = {6,0,0,initSo,0,writeSo,0};
epicsExportAddress(dset,devSoMPC);


static long initCommon(dbCommon *pr, DBLINK *plink, opType ot, recType rt)
{
   char *port, *userParam;
   int i;
   int address;
   asynUser *pasynUser=NULL;
   asynStatus status;
   asynInterface *pasynInterface;
   devMPCPvt *pPvt=NULL;
   char command[100];
   char *pstring;
   
   /* Allocate private structure */
   pPvt = calloc(1, sizeof(devMPCPvt));
   pPvt->opType = ot;
   pPvt->recType = rt;

   /* Create an asynUser */
   pasynUser = pasynManager->createAsynUser(devMPCCallback, 0);
   pasynUser->userPvt = pr;

   /* Parse link */
   status = pasynEpicsUtils->parseLink(pasynUser, plink,
                                       &port, &address, &userParam);
   if (status != asynSuccess) {
      errlogPrintf("devXxMPC::initCommon %s bad link %s\n",
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
      errlogPrintf("devMPC::initCommon %s invalid userParam %s\n",
                   pr->name, userParam);
      goto bad;
   }
   sscanf(userParam,"%s %d",command, &i);
   sprintf(pPvt->address,"%02X",address);
   sprintf(pPvt->parameter,"%d",i);
   for (i=0; i<MAX_MPC_COMMANDS; i++) {
        pstring = mpcCommands[i].commandString;
        if (epicsStrCaseCmp(command, pstring) == 0) {
            pPvt->command = mpcCommands[i].command;
            goto found;
        }
    }
    asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "devMPC::init_common %s, unknown command=%s\n", 
              pr->name, command);
    goto bad;

   found:
   asynPrint(pasynUser, ASYN_TRACE_FLOW,
             "devMPC::initCommon name=%s; command string=%s command=%d, address=%s; parameter=%s;\n",
             pr->name, command, pPvt->command, pPvt->address, pPvt->parameter);

   if (pPvt->command<0 || 
      (pPvt->command >GetTSPStat && pPvt->command < SetUnit) ||
       pPvt->command>SetTSPDegas) {
       asynPrint(pasynUser, ASYN_TRACE_ERROR,
                 "devMPC::initCommon %s illegal command=%d\n",
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

static long initAo(aoRecord *pr)
{
   return(initCommon((dbCommon *)pr, &pr->out, opTypeOutput, recTypeAo));
}

static long initBi(biRecord *pr)
{
   return(initCommon((dbCommon *)pr, &pr->inp, opTypeInput, recTypeBi));
}

static long initBo(boRecord *pr)
{
   return(initCommon((dbCommon *)pr, &pr->out, opTypeOutput, recTypeBo));
}

static long initMbbo(mbboRecord *pr)
{
   return(initCommon((dbCommon *)pr, &pr->out, opTypeOutput, recTypeMbbo));
}

static long initSi(stringinRecord *pr)
{
   return(initCommon((dbCommon *)pr, &pr->inp, opTypeInput, recTypeSi));
}

static long initSo(stringoutRecord *pr)
{
   return(initCommon((dbCommon *)pr, &pr->out, opTypeOutput, recTypeSo));
}

static int buildCommand(devMPCPvt *pPvt, int hexCmd, char *pvalue)
{
    asynUser *pasynUser = pPvt->pasynUser;
    dbCommon *pr = (dbCommon *)pasynUser->userPvt;
/*
    The MPC commands are of the form :  "~  AA XX d cc"
    AA = Address from 00 - FF
    XX = 2 character Hex Command
    d  =  parameter or data comma seperated
    cc = 2 character checksum Hex values

    The checksum is to be calculated starting from the character after the
    start character and ending with the space after the data/parm field.
    Add the sum and divide by 0x100 or decimal 256. The reminder in hex is
    two character checksum. Follow the checksum with a terminator of CR only.
    At the current time we are not calculating the checksum due to the fact
    the device is happy with just "00"
*/
    char tempBuf[10];

    memset(pPvt->sendBuf, 0, MPC_BUFFER_SIZE);
    strcpy(pPvt->sendBuf, "~ ");
    strcat(pPvt->sendBuf, pPvt->address);
    sprintf(tempBuf, " %2.2X ", hexCmd);
    strcat(pPvt->sendBuf, tempBuf);
    strcat(pPvt->sendBuf, pvalue);
    strcat(pPvt->sendBuf, " 00"); /* checksum is set to 00 now !!!! */

    asynPrint(pPvt->pasynUser, ASYN_TRACEIO_DEVICE,
              "devMPC::buildCommand %s command 0x%X len=%d string=|%s|\n",
              pr->name, hexCmd, strlen(pPvt->sendBuf), pPvt->sendBuf);

    return(0);
}

static long checkRtnSize(dbCommon *pr, int rtnSize, int minSize)
{
    devMPCPvt *pPvt = (devMPCPvt *)pr->dpvt;
    
    if (rtnSize < minSize) { 
        asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                  "devMPC, %s rtnSize=%d, should be >%d, response=%s\n",
                  pr->name, rtnSize, minSize, pPvt->recBuf);
        return -1;
    } 
    return 0;
}

static long readAi(aiRecord *pr)
{
    devMPCPvt *pPvt = (devMPCPvt *)pr->dpvt;
    char tempparameter[2];
    int stptNo;
    int hexCmd=0;
    int rtnSize;
    char *ploc;
    char pvalue[10] = "";
    float value=0;
    char *pdata = pPvt->recBuf;
    char *llen = pdata;

    if (!pr->pact) {
        /* For setpoint readback set the correct setpoint number.
         * All odd setpoints are for pump 1 and even for pump 2 */
        switch (pPvt->command) {
        case GetSpVal12:
        case GetSpVal34:
        case GetSpVal56:
        case GetSpVal78:
            hexCmd = 0x3c;
            stptNo = (pPvt->command - GetSpVal12)/2 ;
            stptNo = stptNo * 2 + atoi(pPvt->parameter);
            sprintf(tempparameter,"%d",stptNo);
            break;
        case GetPres:
            hexCmd = 0x0b;
            strcpy(tempparameter, pPvt->parameter);
            break;
        case GetCur:
            hexCmd = 0x0a;
            strcpy(tempparameter, pPvt->parameter);
            break;
        case GetVolt:
            hexCmd = 0x0c;
            strcpy(tempparameter, pPvt->parameter);
            break;
        case GetSize:
            hexCmd = 0x11;
            strcpy(tempparameter, pPvt->parameter);
            break;
        default:
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                      "devMPC::readAi %s Wrong record type \n",
                      pr->name);
            break;
        }
        buildCommand(pPvt, hexCmd, tempparameter);
        return(startIOCommon((dbCommon *)pr));
    }

    /* Assume failure */
    pr->val = 0;
    strcpy(pr->egu, "");
    
    if (pPvt->status != asynSuccess) {
        return(2);
    }

    rtnSize = strlen(pPvt->recBuf);
    switch (pPvt->command) {
        case GetPres:
            if (checkRtnSize((dbCommon *)pr, rtnSize, 9)) return(2);
            strncpy(pvalue, pPvt->recBuf, 7);
            pvalue[7] = 0;
            value = strtod(pvalue, NULL);
            ploc=&pPvt->recBuf[8];
            strncpy(pvalue, ploc, rtnSize-8);
            pvalue[strlen(ploc)] = 0;
            break;
        case GetCur:
            if (checkRtnSize((dbCommon *)pr, rtnSize, 7)) return(2);
            strncpy(pvalue, pPvt->recBuf, 7);
            pvalue[7] = 0;
            value = strtod(pvalue, NULL);
            strcpy(pvalue, "AMPS");
            break;
        case GetVolt:
            if (checkRtnSize((dbCommon *)pr, rtnSize, 1)) return(2);
            strncpy(pvalue, pPvt->recBuf, rtnSize);
            pvalue[rtnSize] = 0;
            value = strtod(pvalue, NULL);
            strcpy(pvalue, "VOLTS");
            break;
        case GetSize:
            llen = strchr(pdata, 'L');
	        if (llen == NULL) {
	            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                    "devMPC::readAi, %s cannot find L, response=%s\n",
                    pr->name, pdata);
                return(2);
            } 
            *llen = 0;
            strcpy(pvalue, pdata);
            value = strtod(pvalue, NULL);
            strcpy(pvalue, "L/S");
            break;
        case GetSpVal12:
        case GetSpVal34:
        case GetSpVal56:
        case GetSpVal78:
            if (checkRtnSize((dbCommon *)pr, rtnSize, 11)) return(2);
            ploc=&pPvt->recBuf[4];
            strncpy(pvalue, ploc, 7);
            pvalue[7] = 0;
            value = strtod(pvalue, NULL);
            strcpy(pvalue, "TORR");
            break;
        default:
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                      "devMPC::readAi %s Wrong record type \n",
                      pr->name);
           break;
    }
    pr->val = value;
    strcpy(pr->egu, pvalue);
    pr->udf=0;
    return(2);
}


static long readBi(biRecord *pr)
{
    devMPCPvt *pPvt = (devMPCPvt *)pr->dpvt;
    char tempparameter[2];
    int stptNo;
    int hexCmd=0;
    int rtnSize;
    char *pvalue;
    int value=0;

    if (!pr->pact) {
        /* For setpoint readback set the correct setpoint number.
         * All odd setpoints are for pump 1 and even for pump 2 */
        switch (pPvt->command) {
        case GetSpS12:
        case GetSpS34:
        case GetSpS56:
        case GetSpS78:
            hexCmd = 0x3c;
            stptNo = (pPvt->command - GetSpS12) + atoi(pPvt->parameter);
            sprintf(tempparameter, "%d", stptNo);
            break;
        case GetAutoRestart:
            hexCmd = 0x34;
            strcpy(tempparameter, "");
            break;
        default:
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                      "devMPC::readBi %s Wrong record type \n",
                      pr->name);
            break;
        }
        buildCommand(pPvt, hexCmd, tempparameter);
        return(startIOCommon((dbCommon *)pr));
    }

    pr->rval = 0;
    if (pPvt->status != asynSuccess) {
        return(0);
    }

    rtnSize = strlen(pPvt->recBuf);
    if (checkRtnSize((dbCommon *)pr, rtnSize, 2)) return(0);
    switch (pPvt->command) {
        case GetSpS12:
        case GetSpS34:
        case GetSpS56:
        case GetSpS78:
            pvalue = &pPvt->recBuf[rtnSize-1];
            sscanf(pvalue, "%d", &value);
            break;
        case GetAutoRestart:
            if (strcmp(pPvt->recBuf, "YES") == 0) value=1; else value=0;
            break;
        default:
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                      "devMPC::readBi %s Wrong record type \n",
                      pr->name);
            break;
    }
    pr->rval = value;
    pr->udf=0;
    return(0);
}

static long readSi(stringinRecord *pr)
{
    devMPCPvt *pPvt = (devMPCPvt *)pr->dpvt;
    int hexCmd=0;
    int rtnSize;

    if (!pr->pact) {
        switch (pPvt->command) {
        case GetStatus:
            hexCmd = 0x0d;
            buildCommand(pPvt, hexCmd, pPvt->parameter);
            break;
        case GetTSPStat:
            hexCmd = 0x2a;
            buildCommand(pPvt, hexCmd, "");
            break;
        default:
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                      "devMPC::readSi %s Wrong record type \n",
                      pr->name);
            break;
        }
        return(startIOCommon((dbCommon *)pr));
    }

    strcpy(pr->val, "");
    if (pPvt->status != asynSuccess) {
        return(0);
    }

    rtnSize = strlen(pPvt->recBuf);
    if (rtnSize > 39) {
        recGblSetSevr(pr, READ_ALARM, INVALID_ALARM);
        asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                  "devMPC::readSi message too big in %s \n",
                  pr->name);
        return(0);
    }

    strcpy(pr->val, pPvt->recBuf);
    pr->udf=0;
    return(0);
}

static long writeAo(aoRecord *pr)
{
    devMPCPvt *pPvt = (devMPCPvt *)pr->dpvt;
    char tempparameter[25]="";
    char pvalue[10]="";
    int stptNo;
    int hexCmd=0;
    int rtnSize;
    
    if (!pr->pact) {
        /* For setting setpoint the command is of the form :n,s,x.xE-yy,x.xE-yy
         * n - setpoint number.
         * s - supply here 1 for pump1 and 2 for pump 2
         * values are for On and Off and will be set the same as pr->val field.
         * All odd setpoints are for pump 1 and even for pump 2 */

        switch (pPvt->command) {
        case SetSize:
            hexCmd = 0x12;
            strcpy(tempparameter, pPvt->parameter);
            strcat(tempparameter, ",");
            sprintf(pvalue, "%d", (int) pr->val);
            strcat(tempparameter, pvalue);  /* Pump Size */
            break;
        case SetSp12:
        case SetSp34:
        case SetSp56:
        case SetSp78:
            hexCmd = 0x3d;
            stptNo = (pPvt->command - SetSp12) *2 + atoi(pPvt->parameter);
            sprintf(tempparameter, "%1.1d", stptNo);
            strcat(tempparameter, ",");
            strcat(tempparameter, pPvt->parameter);  /* for supply number */
            strcat(tempparameter, ",");
            sprintf(pvalue, "%7.1E", pr->val);
            strcat(tempparameter, pvalue);  /* for On pressure */
            strcat(tempparameter, ",");
            strcat(tempparameter, pvalue);  /* for Off pressure */
            break;
        default:
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                      "devMPC::writeAo %s Wrong record type \n",
                      pr->name);
            break;
        }
        buildCommand(pPvt, hexCmd, tempparameter);
        return(startIOCommon((dbCommon *)pr));
    }
    if (pPvt->status != asynSuccess) return(2);
    rtnSize = strlen(pPvt->recBuf);
    if (rtnSize > 2) {
        recGblSetSevr(pr, READ_ALARM, INVALID_ALARM);
        asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR, 
                  "devMPC::writeAo message too big in %s\n",
                  pr->name);
        return(2);
    }
    pr->udf=0;
    return(2);
}

static long writeBo(boRecord *pr)
{
    devMPCPvt *pPvt = (devMPCPvt *)pr->dpvt;
    int hexCmd=0;
    int rtnSize;
    
    if (!pr->pact) {
        switch (pPvt->command) {
        case SetStart:
        case SetStop:
            if (pr->val == 0) hexCmd = 0x37; else hexCmd = 0x38;
            buildCommand(pPvt, hexCmd, pPvt->parameter);
            break;
        case SetLock:
        case SetUnlock:
            if (pr->val == 0) hexCmd = 0x44; else hexCmd = 0x45;
            buildCommand(pPvt, hexCmd, pPvt->parameter);
            break;
        case SetAutoRestart:
            hexCmd = 0x33;
            if (pr->val)
                buildCommand(pPvt, hexCmd, "YES");
            else
                buildCommand(pPvt, hexCmd, "NO");
            break;
        case SetTSPAutoAdv:
            hexCmd = 0x2c;
            if (pr->val)
                buildCommand(pPvt, hexCmd, "YES");
            else
                buildCommand(pPvt, hexCmd, "NO");
            break;
        case SetTSPOff:
            hexCmd = 0x28;
            buildCommand(pPvt, hexCmd, "");
            break;
        case SetTSPClear:
            hexCmd = 0x2b;
            buildCommand(pPvt, hexCmd, "");
            break;
        case SetTSPContinuous:
            hexCmd = 0x2d;
            buildCommand(pPvt, hexCmd, "");
            break;
        case SetTSPDegas:
            hexCmd = 0x2f;
            buildCommand(pPvt, hexCmd, "");
            break;
        default:
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                      "devMPC::writeBo %s Wrong record type \n",
                      pr->name);
            break;
        }
        return(startIOCommon((dbCommon *)pr));
    }
    if (pPvt->status != asynSuccess) return(0);
    rtnSize = strlen(pPvt->recBuf);
    if (rtnSize > 2) {
        recGblSetSevr(pr, READ_ALARM, INVALID_ALARM);
        asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR, 
                  "devMPC::writeBo message too big in %s\n",
                  pr->name);
        return(0);
    }
    pr->udf=0;
    return(0);
}

static long writeMbbo(mbboRecord *pr)
{
    devMPCPvt *pPvt = (devMPCPvt *)pr->dpvt;
    char tempparameter[10];
    int hexCmd=0;
    int rtnSize;
    
    if (!pr->pact) {
        switch (pPvt->command) {
        case SetUnit:
            hexCmd = 0x0e;
            strcpy(tempparameter, pPvt->parameter);
            strcat(tempparameter, DisplayStr[pr->rval]);
            break;
        case SetDis:
            hexCmd = 0x25;
            strcpy(tempparameter, pPvt->parameter);
            strcat(tempparameter, DisplayStr[pr->rval]);
            break;
        case SetTSPFilament:
            hexCmd = 0x29;
            sprintf(tempparameter, "%d", pr->rval);
            break;
        default:
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                      "devMPC::writeMbbo %s Wrong record type \n",
                      pr->name);
            break;
        }
        buildCommand(pPvt, hexCmd, tempparameter);
        return(startIOCommon((dbCommon *)pr));
    }
    if (pPvt->status != asynSuccess) return(0);
    rtnSize = strlen(pPvt->recBuf);
    if (rtnSize > 2) {
        recGblSetSevr(pr, READ_ALARM, INVALID_ALARM);
        asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR, 
                  "devMPC::writeMbbo message too big in %s\n",
                  pr->name);
        return(0);
    }
    pr->udf=0;
    return(0);
}

static long writeSo(stringoutRecord *pr)
{
    devMPCPvt *pPvt = (devMPCPvt *)pr->dpvt;
    int hexCmd=0;
    int rtnSize;
    
    if (!pr->pact) {
        switch (pPvt->command) {
        case SetTSPTimed:
            hexCmd = 0x27;
            break;
        case SetTSPSublimation:
            hexCmd = 0x2e;
            break;
        default:
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                      "devMPC::writeSo %s Wrong record type \n",
                      pr->name);
            break;
        }
        buildCommand(pPvt, hexCmd, pr->val);
        return(startIOCommon((dbCommon *)pr));
    }
    if (pPvt->status != asynSuccess) return(0);
    rtnSize = strlen(pPvt->recBuf);
    if (rtnSize > 2) {
        recGblSetSevr(pr, READ_ALARM, INVALID_ALARM);
        asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR, 
                  "devMPC::writeSo message too big in %s\n",
                  pr->name);
        return(0);
    }
    pr->udf=0;
    return(0);
}


static long startIOCommon(dbCommon* pr)
{
   devMPCPvt *pPvt = (devMPCPvt *)pr->dpvt;
   asynUser *pasynUser = pPvt->pasynUser;
   int status;

   pr->pact = 1;
   status = pasynManager->queueRequest(pasynUser, 0, 0);
   if (status != asynSuccess) status = -1;
   return(status);
}

static void devMPCCallback(asynUser *pasynUser)
{
    dbCommon *pr = (dbCommon *)pasynUser->userPvt;
    devMPCPvt *pPvt = (devMPCPvt *)pr->dpvt;
    char readBuffer[MPC_BUFFER_SIZE];
    struct rset *prset = (struct rset *)(pr->rset);
    int eomReason;
    size_t nread, nwrite;

    memset(pPvt->recBuf, 0, MPC_BUFFER_SIZE);
    pPvt->pasynUser->timeout = MPC_TIMEOUT;
    pPvt->status = pPvt->pasynOctet->write(pPvt->octetPvt, pasynUser, 
                                           pPvt->sendBuf, strlen(pPvt->sendBuf), 
                                           &nwrite);
    if (pPvt->status != asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devMPC::devMPCCallback write error, status=%d error= %s\n",
                  pPvt->status, pasynUser->errorMessage);
        recGblSetSevr(pr, WRITE_ALARM, INVALID_ALARM);
        goto done;
    }
    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
              "devMPC::devMPCCallback %s nwrite=%d, output=%s\n",
              pr->name, nwrite, pPvt->sendBuf);
    pPvt->status = pPvt->pasynOctet->read(pPvt->octetPvt, pasynUser, 
                                          readBuffer, MPC_BUFFER_SIZE, 
                                          &nread, &eomReason);
    if (pPvt->status != asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devMPC::devMPCCallback %s read error, status=%d error= %s\n",
                  pr->name, pPvt->status, pasynUser->errorMessage);
        recGblSetSevr(pr, READ_ALARM, INVALID_ALARM);
        goto done;
    }
    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
              "devMPC::devMPCCallback %s nread=%d, input=%s\n",
              pr->name, nread, readBuffer);
    if (nread < 4) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devMPC::devMPCCallback %s message too small=%d\n", 
                  pr->name, nread);
        recGblSetSevr(pr, READ_ALARM, INVALID_ALARM);
        pPvt->status = asynError;
        goto done;
    }

    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
              "devMPC: %s command (%d) received (before processing) len=%d, |%s|\n",
              pr->name, pPvt->command, nread, readBuffer);
    if(readBuffer[3]=='O' && readBuffer[4] == 'K') {
        if (nread < 12 ) {
            strcpy(pPvt->recBuf, "OK");
        } else {
            char *pdata = &readBuffer[9]; /* strip off the header cmds */
            /* strip off 3 trailing character (space, checksum) */
            strncpy(pPvt->recBuf, pdata, nread-12);  
        }
    }
    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
              "devMPC: %s command (%d) received (after processing) |%s|\n",
              pr->name, pPvt->command, pPvt->recBuf);

    done:
    /* Process the record. This will result in the readX or writeX routine
       being called again, but with pact=1 */
    dbScanLock(pr);
    (*prset->process)(pr);
    dbScanUnlock(pr);
}


static long MPCConvert(dbCommon* pr,int pass)
{
   aiRecord* pai = (aiRecord*)pr;
   pai->eslo=1.0;
   pai->roff=0;
   return 0;
}
