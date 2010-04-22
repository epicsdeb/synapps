/*  devAiMKS.cc
  
    Author: Mark Rivers
  
    Modifications:  1995     mlr     Original version
                   11/1/97  mlr     Added debugging statements
                   08/15/99 mlr     Converted to MPF
                   07/13/04 mlr     Converted from MPF to asyn and from C++ to C
  
    This module provides EPICS AI (Analog Input) device support for the 
    MKS 937 Vacuum Gauge Controller uing HiDEOS.
    It presently uses only RS-232 communication. RS-485 may be added in the
    future.
    The PARM field is specified as follows:
    port,m
        where 
            "port" is the aysn port
            "m" is the vacuum gauge number on the MKS 937 (1-5)
  
    Remember to set the serial port settings in the vxWorks startup file to
    use the correct baud rate and even parity.
  
    This device support package does the following:
        - On record initialization:
            - If rec.PREC=0 then it sets rec.PREC=1.  PREC=1 will display
              the actual number of digits reported by the 937.
            - Sends queries the 937 to read the current gauge units (Torr, 
              Pascal, etc.) and sets rec.EGU to this string.
            - Queries the 937 to determine the gauge type (cold-cathode,
              Pirani, etc).  This information is used to determine the lower 
              and upper limits of this gauge.
            - If rec.LOPR=0 and rec.HOPR=0 sets these to the upper and lower
              limits of this gauge type.
        - On record processing
            - Reads the current pressure from the gauge.
            - If the pressure is above or below the range of this gauge, sets
              a minor alarm and sets rec.VAL= minimum or maximum value for
              this gauge type.
            - If the gauge returns any other error or if the read request
              times out, then it sets a major alarm and sets rec.VAL=0.
            - If there are no errors then rec.VAL is set to the pressure
              and device support returns 2 to tell record support not to
              convert the value, since we wrote directly to the VAL field.
*/        

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <dbAccess.h>
#include <aiRecord.h>
#include <recGbl.h>
#include <recSup.h>
#include <link.h>
#include <cantProceed.h>
#include <errlog.h>
#include <devSup.h>
#include <alarm.h>
#include <asynDriver.h>
#include <asynEpicsUtils.h>
#include <asynOctet.h>
#include <asynOctetSyncIO.h>
#include <epicsExport.h>

#define TIMEOUT  2   /* Timeout in sec */
#define MAX_RESPONSE_LEN  10   /* Maximmum length of response from device */

typedef enum {
    TYPE_CC,   /* Cold-cathode gauge */
    TYPE_PR,   /* Pirani gauge */
    TYPE_CM,   /* Capacitance manometer */
    TYPE_TC,   /* Thermocouple */
    TYPE_CV    /* Convection */
} gaugeType;

typedef enum {readUnits, readGaugeType} MKSCommand;

typedef struct {
    asynUser   *pasynUser;
    asynOctet  *pasynOctet;
    void       *asynOctetPvt;
    aiRecord   *pai;
    char       readCommand[4];
    gaugeType  gaugeType;
    int        gaugeNumber;
    double     gaugeHigh;
    double     gaugeLow;
} devAiMKSPvt;

typedef struct {
        long      number;
        DEVSUPFUN report;
        DEVSUPFUN init;
        DEVSUPFUN init_record;
        DEVSUPFUN get_ioint_info;
        DEVSUPFUN io;
        DEVSUPFUN convert;
} dsetAiMKS;

static long initAi(aiRecord *pai);
static long readAi(aiRecord *pai);
static long convertAi(aiRecord *pai, int pass);
static void callbackAi(asynUser *pasynUser);
dsetAiMKS devAiMKS = {6, 0, 0, initAi, 0, readAi, convertAi};

epicsExportAddress(dset, devAiMKS);

static long initAi(aiRecord *pai)
{
    char *port, *userParam;
    asynUser *pasynUser;
    asynStatus status;
    asynInterface *pasynInterface;
    size_t nwrite, nread;
    int eomReason;
    devAiMKSPvt *pPvt;
    char response[MAX_RESPONSE_LEN];
    char gauge_str[3];  /*String to hold gauge type */
    double mult;        /* Multiplier for different pressure units */
  
    /* Allocate private structure */
    pPvt = callocMustSucceed(1, sizeof(*pPvt), "devAiMKS::init_record");
    /* Create an asynUser */
    pasynUser = pasynManager->createAsynUser(callbackAi, 0);
    pasynUser->userPvt = pPvt;
    pPvt->pai = pai;

    status = pasynEpicsUtils->parseLink(pasynUser, &pai->inp, 
                                        &port, &pPvt->gaugeNumber,
                                        &userParam);
    if(status != asynSuccess) {
        errlogPrintf("devAiMKS::initAi %s bad link %s \n",
                     pai->name, pasynUser->errorMessage);
        goto bad;
    }
    if(pPvt->gaugeNumber < 1 || pPvt->gaugeNumber > 5) {
        errlogPrintf("devAiMKS::init_record invalid gauge %d\n",
                     pPvt->gaugeNumber);
        goto bad;
    } else {
      sprintf(pPvt->readCommand, "R%d", pPvt->gaugeNumber);
    }

    /* Connect to port */
    status = pasynManager->connectDevice(pasynUser, port, 0);
    if (status != asynSuccess) {
        errlogPrintf("devAiMKS::initAi %s cannot connect to device %s\n",
                     pai->name, port);
        goto bad;
    }
    /* Get asynOctet interface */
    pasynInterface = pasynManager->findInterface(pasynUser, asynOctetType, 1);
    if (!pasynInterface) {
        errlogPrintf("devAiMKS::initAi %s cannot find asynOctet interface\n",
                     pai->name);
        goto bad;
    }
    pPvt->pasynOctet = (asynOctet *)pasynInterface->pinterface;
    pPvt->asynOctetPvt = pasynInterface->drvPvt;
    pPvt->pasynUser = pasynUser;
    pai->dpvt = pPvt;

    /* If the .PREC field is 0, set it to 1, since this is the actual
     * resolution of the device. */
    if (pai->prec == 0) pai->prec = 1;
                      
    /* Read the pressure units (Torr, Pascal, etc.) from the controller
     * Use synchronous I/O since we are in iocInit and it's much simpler */
    status = pasynOctetSyncIO->writeReadOnce(port, 0, "SU", 2,
                                   response, sizeof(response),
                                   TIMEOUT, &nwrite, &nread, &eomReason, NULL);
    if ((status != asynSuccess) || (nread != 7)) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devAiMKS ERROR, record=%s, nread=%d, response=\n%s\n",
                  pai->name, nread, response);
        recGblSetSevr(pai,READ_ALARM,MAJOR_ALARM);
        goto bad;
    }
    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
              "devAiMKS::initAi record=%s, nread=%d, response=\n%s\n",
              pai->name, nread, response);
    response[7]=0;
    strcpy(pai->egu, response);
           
    /* Read the gauge type from the controller
     * Note that three values are output, one for each slot.  The first
     * slot is either empty or a cold-cathode controller.  The other two
     * slots can hold any type of controller board, and for all types 
     * except cold-cathode each board may control one or two gauges. */
    status = pasynOctetSyncIO->writeReadOnce(port, 0, "SG", 2,
                                   response, sizeof(response),
                                   TIMEOUT, &nwrite, &nread, &eomReason, NULL);
    if ((status != asynSuccess) || (nread != 7)) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devAiMKS ERROR, record=%s, nread=%d, response=\n%s\n",
                  pai->name, nread, response);
        recGblSetSevr(pai,READ_ALARM,MAJOR_ALARM);
        goto bad;
    }
    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
              "devAiMKS::initAi record=%s, nread=%d, response=\n%s\n",
              pai->name, nread, response);
    response[7]=0;
    gauge_str[2] = '\0';
    switch (pPvt->gaugeNumber) {
        case 1:
            strncpy(gauge_str,&response[0],2);
            break;
        case 2:
        case 3:
            strncpy(gauge_str,&response[2],2);
            break;
        case 4:
        case 5:
            strncpy(gauge_str,&response[4],2);
            break;
    }
    if (!strcmp(gauge_str, "Cc")) {
        pPvt->gaugeType = TYPE_CC;
        pPvt->gaugeLow = 1.0e-11;
        pPvt->gaugeHigh = 1.0e-2;
    } else if (!strcmp(gauge_str, "Pr")) {
        pPvt->gaugeType = TYPE_PR;
        pPvt->gaugeLow = 5.e-4;
        pPvt->gaugeHigh = 760.;
    } else if (!strcmp(gauge_str, "Cm")) {
        pPvt->gaugeType = TYPE_CM;
        pPvt->gaugeLow = 1.e-3;  /* These values depend on head model */
        pPvt->gaugeHigh = 1.e0;
    } else if (!strcmp(gauge_str, "Tc")) {
        pPvt->gaugeType = TYPE_TC;
        pPvt->gaugeLow = 1.e-3;
        pPvt->gaugeHigh = 1.e0;
    } else if (!strcmp(gauge_str, "Cv")) {
        pPvt->gaugeType = TYPE_CV;
        pPvt->gaugeLow = 1.e-3;
        pPvt->gaugeHigh = 1.e+3;
    } else {
         asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devAiMKS, unknown gauge type %s\n", gauge_str);
        recGblSetSevr(pai,READ_ALARM,MAJOR_ALARM);
    }

    /* The values for gaugeLow and guage_high assigned above are
     * in Torr.  If the gauge is not reading in Torr then we need to
     * convert to the appropriate units. */
    mult = 1.0;
    if (!strncmp(pai->egu, "mbar", 4)) mult=1.3;
    else if (!strncmp(pai->egu, "Pascal", 6)) mult=130.;
    else if (!strncmp(pai->egu, "micron", 6)) mult=1000.;
    pPvt->gaugeLow = pPvt->gaugeLow * mult;
    pPvt->gaugeHigh = pPvt->gaugeHigh * mult;
    /* If LOPR and HOPR are both zero then set them to the gauge
     * limits */
    if ((pai->lopr==0.) && (pai->hopr==0.)) {
        pai->lopr = pPvt->gaugeLow;
        pai->hopr = pPvt->gaugeHigh;
    }
    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
              "devAiMKS record=%s, gauge_str=%s, lopr=%f, hopr=%f\n",
              pai->name, gauge_str, pai->lopr, pai->hopr);

    return 0;

    bad:
    pai->pact = 1;
    return 0;
}
        
long readAi(aiRecord *pai)
{
    devAiMKSPvt *pPvt = (devAiMKSPvt *)pai->dpvt;

    if (pai->pact == 0) { 
        /* This is a call to do I/O */
        pai->pact = 1;
        pasynManager->queueRequest(pPvt->pasynUser, 0, 0);
        return(0);
    } else {
        /* This is a callback from device support.
         * The data have already been put in the record by callback.
         * Note - we always return "2" to record support.  This indicates
         * success, but don't convert value, since this routine writes
         * directly to the VAL field. */
        return(2);
    }
}

static void callbackAi(asynUser *pasynUser)
{
    devAiMKSPvt *pPvt = (devAiMKSPvt *)pasynUser->userPvt;
    aiRecord *pai = pPvt->pai;
    char response[MAX_RESPONSE_LEN];
    asynStatus status;
    size_t nwrite, nread;
    int eomReason;
    struct rset *prset = (struct rset *)(pai->rset);

    pasynUser->timeout = TIMEOUT;
    status = pPvt->pasynOctet->write(pPvt->asynOctetPvt, pasynUser,
                                     pPvt->readCommand, 
                                     strlen(pPvt->readCommand), &nwrite);
    status = pPvt->pasynOctet->read(pPvt->asynOctetPvt, pasynUser, response,
                                    MAX_RESPONSE_LEN, &nread, &eomReason);

    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE, 
              "devAiMKS record=%s, len=%d, response=\n%s\n", 
              pai->name, nread, response);
    if ((status != asynSuccess) || nread != 7) {
        recGblSetSevr(pai,READ_ALARM,MAJOR_ALARM);
        goto finish;
    }

    pai->val = 0.;   /* Set value to 0. in case of errors */
    response[7] = 0;

    /* Decode pressure string
     * If the first character of the response is a digit or blank then
     * we got a number, which is a good response */
    if( (isdigit(response[0])) || (response[0]==' ') )
        pai->val = atof(response);
    else {
        if (!strncmp(response,"H ",2) || !strncmp(response,"A ",2)) {
            /* Pressure is above valid range for gauge */
            pai->val = pPvt->gaugeHigh;
            recGblSetSevr(pai,READ_ALARM,MINOR_ALARM);
        } else if (!strncmp(response,"L ",2)) {
            /* Pressure is below valid range for gauge */
            pai->val = pPvt->gaugeLow;
            recGblSetSevr(pai,READ_ALARM,MINOR_ALARM);
        } else if (!strncmp(response,"MI",2)  ||  /* Misconnected */
                   !strncmp(response,"NO",2)  ||  /* No gauge */
                   !strncmp(response,"HV",2)) {   /* High voltage off */
            asynPrint(pasynUser, ASYN_TRACE_FLOW,
                      "devAiMKS record=%s, no gauge, etc.\n", 
                      pai->name);
            recGblSetSevr(pai,READ_ALARM,MAJOR_ALARM);
        /* There appears to be a problem with the MKS.  Every once in a while
         * it will return a response "NotCMD!" or "SYNTAX!", even though I am
         * quite certain it is only being sent valid commands.  For now
         * we suppress error messages and alarms when this happens so that
         * we don't fill up log files and put displays in alarm. */
        } else if (!strncmp(response,"SYNTAX!",7)  ||
                   !strncmp(response,"NotCMD!",7)) {
            asynPrint(pasynUser, ASYN_TRACE_FLOW,
                      "devAiMKS record=%s, SYNTAX or NotCMD response.\n", 
                      pai->name);
        } else {      /* Unexpected response from MKS */
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                      "devAiMKS record=%s, unknown response len=%d response=%s\n", 
                      pai->name, strlen(response), response);
            recGblSetSevr(pai,READ_ALARM,MAJOR_ALARM);
        }
    }
    finish:
    pai->udf = 0;
   /* Process the record. This will result in the readAi routine
      being called again, but with pact=1 */
   dbScanLock((dbCommon *)pai);
   (*prset->process)(pai);
   dbScanUnlock((dbCommon *)pai);
}

static long convertAi(aiRecord *pai, int pass)
{
    /* Does nothing because we read directly into VAL */
    return(0);
}
