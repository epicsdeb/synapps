/* devDigitelPump.c
* DigitelPump Vacuum Gauge Controller
* INP has form @asyn(port address) station
* TYPE has ("devType")
* port is the asyn serial port name
* address is the controller address (0-255 for MPC)
* station for Digitel500/1500 is no. of spts (0-3)
* station for MPC is either 1 or 2 for the two pump controllers
* devType is "MPC" or "D500" or "D1500" 
*
*  Author: Mohan Ramanathan
*  July 2007  
*  A common one for Digitel 500, Digitel 1500 and MPC/LPC/SPC
*
*   revision:  01
*	01-07-2010   Fixed the code to work with D500 bitbus bug
*
*
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
#include <errlog.h>
#include <link.h>
#include <recGbl.h>
#include <recSup.h>
#include <devSup.h>
#include <epicsString.h>
#include <asynDriver.h>
#include <asynEpicsUtils.h>
#include <asynOctet.h>
#include <digitelRecord.h>
#include <epicsExport.h>

#include "choiceDigitel.h"
#include "devDigitelPump.h"

#define DigitelPump_BUFFER_SIZE 250
#define DigitelPump_SIZE 50
#define DigitelPump_TIMEOUT 1.0
#define MAX_CONSEC_ERRORS 2

typedef struct devDigitelPumpPvt {
    asynUser    *pasynUser;
    asynOctet   *pasynOctet;
    void        *octetPvt;
    asynStatus  status;
    char        recBuf[DigitelPump_BUFFER_SIZE];
    char	sendBuf[DigitelPump_SIZE];
    char	*PortName;
    int		devType;
    cmdType     command;
    char	cmdPrefix[5];
    int		pumpNo;
    int		noSPT;
    int		cv1;
    int		cv2;
    int		errCount;
} devDigitelPumpPvt;

typedef struct dsetDigitelPump{
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN readWrite_dg;
} dsetDigitelPump;


static long init(digitelRecord *pr);
static long readWrite_dg(digitelRecord *pr);

static void buildCommand(devDigitelPumpPvt *pPvt, char *pvalue);

static void devDigitelPumpCallback(asynUser *pasynUser);
static void devDigitelPumpProcess(asynUser *pasynUser,  
				  char *readBuffer, int *nread);

dsetDigitelPump devDigitelPump = {5,0,0,init,0,readWrite_dg};
epicsExportAddress(dset,devDigitelPump);


static long init(digitelRecord *pr)
{
    asynUser *pasynUser=NULL;
    asynStatus status;
    asynInterface *pasynInterface;
    devDigitelPumpPvt *pPvt=NULL;
    DBLINK *plink  = &pr->inp;
    int address;
    char *port, *userParam;
    int station;
   
    /* Allocate private structure */
    pPvt = calloc(1, sizeof(devDigitelPumpPvt));

    /* Create an asynUser */
    pasynUser = pasynManager->createAsynUser(devDigitelPumpCallback, 0);
    pasynUser->userPvt = pr;

    /* Parse input link */
    status = pasynEpicsUtils->parseLink(pasynUser, plink, &port, &address, 
    					&userParam);

    strcpy(pPvt->cmdPrefix,"");
    pPvt->PortName = port;
    pPvt->devType = pr->type;
    sscanf(userParam,"%d",&station);
    pPvt->noSPT = 3;
    /* set ERR to non zero for initialization for Digitel 500/1500 */
    if (pPvt->devType)
    	pPvt->errCount = 1;

/*
* devType needed to be set to device as followsin record "TYPE":
*	MPC	=	0
*	D500	=	1
*	D1500	=	2
*
*  for digitel 500/1500 the address is "0"
*  for MPC the default address is 5 and something has to be set between 1-255
* 
*/

/*  
*    All MPC commands are of the form :  "~ AA XX d cc\r"
*    AA = Address from 00 - FF
*    XX = 2 character Hex Command
*    d  =  parameter or data comma seperated
*    cc = 2 character checksum Hex values     
*
*   The checksum is to be calculated starting from the character after the
*   start character and ending with the space after the data/parm field.
*   Add the sum and divide by 0x100 or decimal 256 and reminder in hex is 
*   two character checksum ( just and it with 0xff). Follow the checksum 
*   with a terminator of CR only.
*/

/*
*    All commands sent to digitel 500/1500 end with "\r"  
*    the read commands are "RD\r", "RC\r" and "RSx\r"
*    the control commands are of the form:
*    "Mx\r" where x is between 1-9.
*    "SNPxxyy\r" where N is 1-3 P=1-4 and xxyy if the value.
*
*    The digitel 500 upon power up and also when communication is initiallized
*    automatically sets it up to send exception reporting and timed reporting
*    at 10 minute intervals.  These needs to be turned off with "SL3\r" and 
*    "SL4\r" commands.  We check for ERR and then send these reset commands.   
*
*    Digitel 500/1500 uses  a unique scheme for data back.
*    It echos all the characters sent and then starts with a linefeed.
*    All monitors end with a linefeed & carriage return (\n\r). 
*    For control commands it ends with just "*".
*
*    In the case of BITBUS BUGS for D500 the device strips out the command echos.
*    So the reply from the device in this case is no echos of commands.
*/ 

    /*  MPC controller*/
    if ( pPvt->devType == 0) {
	if (address < 0 || address > 255) {
	    errlogPrintf("devDigitelPump::init %s address out of range %d\n",
                   pr->name, address);
	    goto bad;
	}
	if (station < 1 || station > 2) {
	    errlogPrintf("devDigitelPump::init %s station out of range %d\n",
                   pr->name, station);
	    goto bad;
	}
	sprintf(pPvt->cmdPrefix,"~ %02X",address);
	pPvt->pumpNo = station;

    /*  Digitel 500/1500 controller*/
    } else {
	if (station < 0 || station > 3) {
	    errlogPrintf("devDigitelPump::init %s station out of range %d\n",
                   pr->name, station);
	    goto bad;
	}
        pPvt->noSPT = station;
    }

    if (status != asynSuccess) {
	errlogPrintf("devDigitelPump::init %s bad link %s\n",
                   pr->name, pasynUser->errorMessage);
	goto bad;
    }

    status = pasynManager->connectDevice(pasynUser,pPvt->PortName,0);
    if(status!=asynSuccess) goto bad;

    pasynInterface = pasynManager->findInterface(pasynUser,asynOctetType,1);
    if(!pasynInterface) goto bad;
   
    pPvt->pasynOctet = (asynOctet *)pasynInterface->pinterface;
    pPvt->octetPvt = pasynInterface->drvPvt;
    pPvt->pasynUser = pasynUser;
    pr->dpvt = pPvt;

    asynPrint(pasynUser, ASYN_TRACE_FLOW,
      "devDigitelPump::init name=%s port=%s address=%d Device=%d station=%d\n", 
		pr->name, pPvt->PortName, address, pPvt->devType, station);
    return 0;

bad:
    if(pasynUser) pasynManager->freeAsynUser(pasynUser);
    if(pPvt) free(pPvt);
    pr->pact = 1;
    return -1;
}


static void buildCommand(devDigitelPumpPvt *pPvt, char *pvalue)
{
    asynUser *pasynUser = pPvt->pasynUser;
    dbCommon *pr = (dbCommon *)pasynUser->userPvt;
    int chkSum =0;
    unsigned int i;
    
    memset(pPvt->sendBuf, 0, DigitelPump_SIZE);
    strcpy(pPvt->sendBuf, pPvt->cmdPrefix);
    strcat(pPvt->sendBuf, pvalue);

    /* For MPC only*/
    if (pPvt->devType == 0) {
    	strcat(pPvt->sendBuf, " ");
    	/* Now calculate the checksum.   */
    	for(i=1;i<strlen(pPvt->sendBuf);i++)
        	chkSum += pPvt->sendBuf[i];
    	chkSum &= 0xff;
    	sprintf(&pPvt->sendBuf[i], "%2.2X",chkSum);
    }
    asynPrint(pPvt->pasynUser, ASYN_TRACEIO_DEVICE,
              "devDigitelPump::buildCommand %s len=%d string=|%s|\n",
              pr->name, strlen(pPvt->sendBuf), pPvt->sendBuf);

    return;
}


static long readWrite_dg(digitelRecord *pr)
{
    devDigitelPumpPvt *pPvt = (devDigitelPumpPvt *)pr->dpvt;
    asynUser *pasynUser = pPvt->pasynUser;
    asynStatus status;
    char pvalue[30];
    int t1;
    int t2;
    int t3;
    int pumpType;
    float val1;
    float val2;
    
    memset(pvalue, 0, 30);
    if (!pr->pact) {

	memset(pPvt->sendBuf, 0, DigitelPump_SIZE);
/*
*     We have to check to see if any record fields changed.
*     We cannot send all the changes so for each processing only one command
*     change will be sent. 
*     If no record change was done then read the values from the record....
*/
       if (pr->flgs) {
            if (pr->flgs & MOD_DSPL) {
             	if (pPvt->devType)
            	    sprintf(pvalue,"M%d",3+pr->dspl);
             	else 
            	    sprintf(pvalue," %s %d,%s",ctlCmdString[0],
            				  pPvt->pumpNo, displayStr[pr->dspl]);

            } else if (pr->flgs & MOD_MODS) {
             	if (pPvt->devType)
            	    sprintf(pvalue,"M%d",1+pr->mods);
             	else 
            	    sprintf(pvalue," %s %d",ctlCmdString[1+pr->mods],
            	    				pPvt->pumpNo);

            } else if (pr->flgs & MOD_KLCK) {
             	if (pPvt->devType)
            	    sprintf(pvalue,"M%d",8+pr->klck);
             	else 
            	    sprintf(pvalue," %s %d",ctlCmdString[3+pr->klck],
            	    				pPvt->pumpNo);
        
            } else if ( (pr->flgs & MOD_BAKE) && pr->bkin) {
             	if (pPvt->devType)
            	    sprintf(pvalue,"M%d",7-pr->baks);

            } else if (pr->flgs & MOD_SETP) {
             	if (pPvt->devType) {
             	    switch ( pPvt->noSPT) {
             	        case 3:
                  	    if ((pr->spfg & MOD_SP3S) && pr->bkin) {
                    		/* format Snmxe-0x converted to Snmxy */
                    		sprintf(pvalue,"S31%.0e",pr->sp3s);
                        	pvalue[4] = pvalue[7];
                    		pvalue[5] = '\0';
                    
                	    } else if ((pr->spfg & MOD_S3HS) && pr->bkin) {
                    		sprintf(pvalue,"S32%.0e",pr->sp3s);
                        	pvalue[4] = pvalue[7];
                    		pvalue[5] = '\0';

                	    } else if ((pr->spfg & MOD_S3MS) && pr->bkin) {
                    		sprintf(pvalue,"S33%d0%d0",pr->s3ms,
                    					1-pr->s3vr);
 
                	    } else if ((pr->spfg & MOD_S3VS) && pr->bkin) {
                    		sprintf(pvalue,"S33%d0%d0",pr->s3mr,
                    					1-pr->s3vs);
                	    }            
            	        
             	        case 2:
                  	    if (pr->spfg & MOD_SP2S ) {
                    		sprintf(pvalue,"S21%.0e",pr->sp2s);
                        	pvalue[4] = pvalue[7];
                    		pvalue[5] = '\0';
                
                	    } else if (pr->spfg & MOD_S2HS ) {
                    		sprintf(pvalue,"S22%.0e",pr->s2hs);
                        	pvalue[4] = pvalue[7];
                    		pvalue[5] = '\0';

                	    } else if (pr->spfg & MOD_S2MS ) {
                    		sprintf(pvalue,"S23%d0%d0",pr->s2ms,
                    					1-pr->s2vr);
 
                	    } else if (pr->spfg & MOD_S2VS ) {
                    		sprintf(pvalue,"S23%d0%d0",pr->s2mr,
                    					1-pr->s2vs);
                	    }
            	        
             	        case 1:
                	    if (pr->spfg & MOD_SP1S ) {
                    		sprintf(pvalue,"S11%.0e",pr->sp1s);
                        	pvalue[4] = pvalue[7];
                    		pvalue[5] = '\0';

                	    } else if (pr->spfg & MOD_S1HS ) {
                    		sprintf(pvalue,"S12%.0e",pr->s1hs);
                        	pvalue[4] = pvalue[7];
                    		pvalue[5] = '\0';
	
                	    } else if (pr->spfg & MOD_S1MS ) {
                    		sprintf(pvalue,"S13%d0%d0",pr->s1ms,
                    					1-pr->s1vr);
 
                	    } else if (pr->spfg & MOD_S1VS ) {
                    		sprintf(pvalue,"S13%d0%d0",pr->s1mr,
                    					1-pr->s1vs);
                	    }
		    }

             	} else {
            	    if (pr->spfg & MOD_SP1S) {
                        t1 = pPvt->pumpNo;
                        val1 = pr->sp1s;
                        val2 = pr->s1hr;
          	    } else if (pr->spfg & MOD_S2HS) {
                        t1 = pPvt->pumpNo;
                        val1 = pr->sp1r;
                        val2 = pr->s1hs;
                        
               	    } else if (pr->spfg & MOD_SP2S) {
                        t1 = 2 + pPvt->pumpNo;
                        val1 = pr->sp2s;
                        val2 = pr->s2hr;
           	    } else if (pr->spfg & MOD_S2HS) {
                        t1 = 2 + pPvt->pumpNo;
                        val1 = pr->sp2r;
                        val2 = pr->s2hs;

           	    } else if (pr->spfg & MOD_SP3S) {
                        t1 = 4 + pPvt->pumpNo;
                        val1 = pr->sp3s;
                        val2 = pr->s3hr;

            	    } else if (pr->spfg & MOD_S3HS) {
                        t1 = 4 + pPvt->pumpNo;
                        val1 = pr->sp3r;
                        val2 = pr->s3hs;
            	    }
            	    sprintf(pvalue," %s %d,%d,%7.1E,%7.1E",ctlCmdString[5],
            				t1,pPvt->pumpNo,val1,val2);
		}

            	pr->spfg = 0;
            }
            pr->flgs = 0;
	    pPvt->command = cmdControl;
	    buildCommand(pPvt, pvalue);	    

	/* if record has error for Digitel then  send the reset command */
        } else if (pPvt->errCount != 0 && pPvt->devType) {
	    pPvt->command = cmdReset;

        } else {
	    pPvt->command = cmdRead;
        }
 
        asynPrint(pPvt->pasynUser, ASYN_TRACEIO_DEVICE,
              "devDigitelPump::readWrite_dg %s command %d len=%d string=|%s|\n",
              pr->name, pPvt->command, strlen(pPvt->sendBuf), pPvt->sendBuf);
	
	pr->pact = 1;
	status = pasynManager->queueRequest(pasynUser, 0, 0);
	if (status != asynSuccess) status = -1;
        return(status);
    }
/*  
*	Now process the data back from the device during callback 
*	check the error count.
*	if ERR = 0 then process normally.  
*	If ERR > allowed Error then set the invalid read alarm and return.
*	if ERR >0  & <= allowed Error then skip writing to fields.
*/
    pr->err = pPvt->errCount;
    if (pr->err > MAX_CONSEC_ERRORS) {
        recGblSetSevr(pr, READ_ALARM, INVALID_ALARM);
        pr->udf=0;
        return(0);
    }
    if ((pr->err > 0) && (pr->err <= MAX_CONSEC_ERRORS)) {
    	pr->udf=0;
    	return(0);
    } 

/*  now decode the returned data and load in database fields. */
 
/* set to safe value initially ( mostly 0) */
    pr->modr = 0;
    pr->cmor = 0;
    pr->bakr = 0;
    pr->set1 = 0;
    pr->set2 = 0;
    pr->set3 = 0;
    pr->ptyp = 0;
    pr->s1mr = 0;
    pr->s1vr = 1;
    pr->s2mr = 0;
    pr->s2vr = 1;
    pr->s3mr = 0;
    pr->s3vr = 1;
    pr->s3br = 0;
    pr->val = 9.9e+9;

/*  for Digitel commands */
    if (pPvt->devType ) {
	/*  Decode the time online, pump voltage & current */
	strncpy(pvalue,&pPvt->recBuf[0],22);
	sscanf(pvalue,"%d %d:%d %lfV%leI",&t1,&t2,&t3,&pr->volt,&pr->crnt);
	/* Time online in minutes.*/
	pr->tonl = t1*1440 + t2*60 + t3;
  
	/*   Decode the status of High Voltage */
    	if (pPvt->recBuf[23] == 'H')
            pr->modr = 1;
    
	/*   Decode the status of Cooldown */
    	if (pPvt->recBuf[24] == 'C')
            pr->cmor = 1;
   
	/*   Decode the status of Bakeout option */
    	if (pPvt->recBuf[25] == 'B')
            pr->bakr = 1;

	/*   Decode the status of setpoints if exists */
    	if (pPvt->recBuf[26] == '1' && pPvt->noSPT >= 1)
            pr->set1 = 1;

    	if (pPvt->recBuf[27] == '2' && pPvt->noSPT >= 2)
            pr->set2 = 1;

    	if (pPvt->recBuf[28] == '3' && pPvt->noSPT == 3)
            pr->set3 = 1;

	/* Get the accumulated power, current, pumpsize  cooldown cycle */
    	strncpy(pvalue,&pPvt->recBuf[30],13);
    	sscanf(pvalue,"%dP %dI %dC %dS",&t1,&t2,&t3,&pumpType);
    	pr->accw = t1 * 0.444;
    	pr->acci = t2 * 1.11;
    	pr->cool = t3;

	/*  Check to make sure the pump size is correct.. */
    	if (pumpType <1 || pumpType >32)
    	    pumpType =1;
	/* If controller is Digitel 1500 */
    	if(pr->type ==2) 
    	    pumpType *= 4;
    	   
    	switch(pumpType) {
            case 1:
            	pr->ptyp = 0;
            	break;
            case 2:
            	pr->ptyp = 1;
            	break;
            case 4:
            	pr->ptyp = 2;
            	break;
            case 8:
            	pr->ptyp = 3;
            	break;
            case 16:
            	pr->ptyp = 4;
            	break;
            case 32:
            	pr->ptyp = 5;
            	break;
    	}

	/*  calculate the pressure from the size of the pump. (Torr) */
    	if ( (pr->modr == 1) && (pr->volt !=0) ) 
            pr->val = 0.005 * pr->crnt/pumpType;

	/*  Decode the Setpoints if enabled ..... */
	switch (pPvt->noSPT) {
	    case 3:
        	strncpy(pvalue,&pPvt->recBuf[120],18);
        	if (pvalue[0] == 'E' && pvalue[1] == 'R') {
            	    pr->bkin = 0;
        	} else {
            	    pr->bkin = 1;
            	    sscanf(pvalue,"%le %le ",&pr->sp3r,&pr->s3hr);
        	    if (pvalue[14] == '1')
            	    	pr->s2mr = 1;
        	    if (pvalue[16] == '1')
            	    	pr->s2vr = 0;
            	    if (pvalue[17] == '1')
                    	pr->s3br = 1;

	    	    strncpy(pvalue,&pPvt->recBuf[139],2);
	    	    sscanf(pvalue,"%lf",&pr->s3tr);
        	}

	    case 2:
        	strncpy(pvalue,&pPvt->recBuf[90],18);
        	sscanf(pvalue,"%le %le ",&pr->sp2r,&pr->s2hr);
        	if (pvalue[14] == '1')
            	    pr->s2mr = 1;
        	if (pvalue[16] == '1')
            	    pr->s2vr = 0;
	    
	    case 1:
        	strncpy(pvalue,&pPvt->recBuf[60],18);
        	sscanf(pvalue,"%le %le ",&pr->sp1r,&pr->s1hr);
        	if (pvalue[14] == '1')
            	    pr->s1mr = 1;
        	if (pvalue[16] == '1')
            	    pr->s1vr = 0;
	}   	

/*  for MPC   */
    } else {

	/*  get the status first and the mode and cool down mode properly. */
    	strncpy(pvalue,&pPvt->recBuf[0],20);
    	if (strncmp(pvalue,"RUNNING",7) == 0)
            pr->modr = 1;
    	else if (strncmp(pvalue,"COOL DOWN",9) ==0)
            pr->cmor = 1;
    
	/*  read the pressure */
    	strncpy(pvalue,&pPvt->recBuf[30],8);
    	sscanf(pvalue,"%e",&val1);
    	pr->val = val1;

	/*  read the voltage */
    	strncpy(pvalue,&pPvt->recBuf[90],4);
    	sscanf(pvalue,"%d",&t1);
    	pr->volt = t1;

	/*  read the current */
    	strncpy(pvalue,&pPvt->recBuf[60],7);
    	sscanf(pvalue,"%e",&val2);
    	pr->crnt = val2;

    
	/* When pump is turned off */
    	if (pr->volt < 1000 && pr->crnt < 1e-6)
    	    pr->val = 9.9e9;

/*
*  	read the Pump Size - the record has a list of old pump sizes in
*    	30,60,120,220,400,700 l/s so change the value to fit one of these.
*/
    	strncpy(pvalue,&pPvt->recBuf[120],4);
    	sscanf(pvalue,"%d ",&pumpType);
    	if (pumpType < 45)
    	    pr->ptyp = 0;
    	else if (pumpType < 75)
    	    pr->ptyp = 1;
    	else if (pumpType < 170)
    	    pr->ptyp = 2;
    	else if (pumpType < 300)
    	    pr->ptyp = 3;
    	else if (pumpType < 500)
    	    pr->ptyp = 4;
    	else
    	    pr->ptyp = 5;
   
	/*  read the Setpoint 1 or 2 */
	strncpy(pvalue,&pPvt->recBuf[150],25);
    	sscanf(pvalue,"%d,%d,%e,%e,%d",&t1,&t2,&val1,&val2,&t3);
    	pr->sp1r = val1;
    	pr->s1hr = val2;
    	pr->set1 = t3;

	/*  read the Setpoint 3 or 4 */
	strncpy(pvalue,&pPvt->recBuf[180],25);
    	sscanf(pvalue,"%d,%d,%e,%e,%d",&t1,&t2,&val1,&val2,&t3);
    	pr->sp2r = val1;
    	pr->s2hr = val2; 
    	pr->set2 = t3;

	/*  read the Setpoint 5 or 6  */
	strncpy(pvalue,&pPvt->recBuf[210],25);
    	sscanf(pvalue,"%d,%d,%e,%e,%d",&t1,&t2,&val1,&val2,&t3);
    	pr->sp3r = val1;
    	pr->s3hr = val2;
    	pr->set3 = t3;
    }

    pr->lval = log10(pr->val);
    if (pr->val < 1e-12 ) pr->lval = -12;
    pr->udf=0;
    return(0);
}


static void devDigitelPumpCallback(asynUser *pasynUser)
{
    dbCommon *pr = (dbCommon *)pasynUser->userPvt;
    devDigitelPumpPvt *pPvt = (devDigitelPumpPvt *)pr->dpvt;
    char readBuffer[DigitelPump_SIZE];
    char responseBuffer[DigitelPump_BUFFER_SIZE];
    struct rset *prset = (struct rset *)(pr->rset);
    int i, nread;
    char pvalue[30]="";
    char *pstartdata=0;

    pPvt->pasynUser->timeout = DigitelPump_TIMEOUT;
    memset(responseBuffer, 0, DigitelPump_BUFFER_SIZE);

/*
*   DigitelPump on normal cycle should get status from all the values.
*   For commands issued the sendBuf will have a finite value.
*/

/*
*     The digitel 500 upon power up and also when communication is initiallized
*     automatically sets it up to send exception reporting and timed reporting
*     at 10 minute intervals.  These needs to be turned off with SL3 and SL4
*     commands. The SL commands reply with the standard "*" and in addition 
*     gives some bogues characters like 30 and sometimes #30 followed by LF CR.
*/

    /*  for sending write commands  */
    if (pPvt->command == cmdControl) {
        devDigitelPumpProcess(pasynUser,readBuffer,&nread);

        if (nread < 1 ) {
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  	"devDigitelPumpCallback %s Cmd reply too small=%d\n", 
                  	pr->name, nread);
            goto finish;
        }

	/* for Digitel */
	if (pPvt->devType) {	
            if (readBuffer[0] !='M' && readBuffer[0] != 'S') {
            	asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  	"devDigitelPumpCallback %s Cmd reply has error=[%s]\n", 
                  	pr->name, readBuffer);
	    	recGblSetSevr(pr, READ_ALARM, INVALID_ALARM);
            	goto finish;;
            }
	/* for MPC */
	} else  {	
	    if(readBuffer[3]!='O' || readBuffer[4] != 'K') {
            	asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  	"devDigitelPumpCallback %s Cmd reply has error=[%s]\n", 
                  	pr->name, readBuffer);
	    	recGblSetSevr(pr, READ_ALARM, INVALID_ALARM);
            	goto finish;
            }
	} 

    /*  for Digitel 500/1500 issue reset commands  */
    } else if (pPvt->command == cmdReset && pPvt->devType) {
   	strcpy( pPvt->sendBuf,"SL3");
        devDigitelPumpProcess(pasynUser,readBuffer,&nread);
       	strcpy( pPvt->sendBuf,"SL4");
        devDigitelPumpProcess(pasynUser,readBuffer,&nread);
        
    }

/*   Now start the various reads ......
 *   make sure to check the devType and send the correct set of commands
 *   devType ->  0 = MPC , 1 & 2 Digitel 500 and 1500
 *   MPC has 8 commands while Digitel has either 2-5 based on noSPT
 *   The data will be packed into responseBuf 

 *   The locations are as follows for MPC:
 *   responseBuf[0-29]    = Pump Status
 *   responseBuf[30-59]   = Pressure
 *   responseBuf[60-89]   = Current
 *   responseBuf[90-119]   = Voltage
 *   responseBuf[120-149] = Pump Size
 *   responseBuf[150-179] = SetPoint 1 or 2
 *   responseBuf[180-209] = SetPoint 3 or 4
 *   responseBuf[210-239] = SetPoint 5 or 6 
 *
 *   The locations are as follows for Digitel:
 *   responseBuf[0-29]    = Pump Status
 *   responseBuf[30-59]   = AutoRun and Pump Size
 *   responseBuf[60-89]   = SetPoint 1
 *   responseBuf[90-119]  = SetPoint 2
 *   responseBuf[120-149] = SetPoint 3
*/   


    for (i=0;i<8;i++) {
	/*  check for Digitel setpoints and exit when commands are done */	
        if (pPvt->devType && pPvt->noSPT == 0 && i > 1) continue;
        if (pPvt->devType && pPvt->noSPT == 1 && i > 2) continue;
        if (pPvt->devType && pPvt->noSPT == 2 && i > 3) continue;
        if (pPvt->devType && pPvt->noSPT == 3 && i > 4) continue;
	
	/* for Digitel 500/1500 the read commands are offset by 10 */
	if (pPvt->devType) {
	    strcpy(pvalue,readCmdString[10 + i]);
	} else {
	    if ( i < 5) 
                sprintf(pvalue," %s %d",readCmdString[i], pPvt->pumpNo);
	    else 
            	/* set the setpoint based on pump being odd/even */
            	sprintf(pvalue," %s %d",readCmdString[i], 
            	    			((i-5)*2 + pPvt->pumpNo));
	}
	buildCommand(pPvt,pvalue);
	devDigitelPumpProcess(pasynUser,readBuffer,&nread);

        if (nread < 1) {
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devDigitelPumpCallback %s Read reply too small=%d\n", 
                  pr->name, nread);
	    pPvt->errCount++;
            goto finish;
        }

/*
*     For MPC check whether status is "OK" for good
*     For Digitel sends out message with the words ERROR at the beginning
*     Lets look for these and set the alarm on the record if problems.
*
*     For MPC lets strip the leading 9 characters all the way to "d"
*     For MPC the reply "AA XX d cc\r" (letters are same as controls)
*
*     Digitel echos all the characters sent and then starts with a linefeed.
*     For digitel the reply 
*		RD  = "RD\r\nDD HH:MM XXXXV x.xE-xI HCB123\n\r"
*		RC  = "RC\r\nXXP XXI XC XS\n\r"
*		RSx = "RSx\r\nX.0E-X Y.0E-Y ZZZZ HH\n\r"
*
*	For BITBUS and Digitel the driver does not receive the echos 
*		of the commands and strips the leading \n	
*/              
	/* for Digitel */
	if (pPvt->devType) {
            if (readBuffer[4] =='E' || readBuffer[5] =='E' || readBuffer[0] =='E') {
            	asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  	"devDigitelPumpCallback %s Read reply has error=[%s]\n", 
                  	pr->name, readBuffer);
	    	pPvt->errCount++;
            	goto finish;
	    }

 	    /*  find the echoed command followed by \n  (\r\n) */
 	    /*  No "R" as first character for bitbus case  */
            if (readBuffer[0] == 'R') {
            	if ( i < 2 )
            		pstartdata = &readBuffer[4];
           	else  
            		pstartdata = &readBuffer[5]; 
            } else {
            	pstartdata = &readBuffer[0];
            }

	/* for MPC */	
	} else  {    
    	    if(readBuffer[3]=='O' && readBuffer[4] == 'K') {
        	if (nread < 12 ) {
            	    strcpy(readBuffer, "OK");
            	    pstartdata = &readBuffer[0];
        	} else {
            	    /* strip off the header cmds */
            	    pstartdata = &readBuffer[9];  
       		}
    	    } else {
            	asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  	"devDigitelPumpCallback %s Read reply has error=[%s]\n", 
                  	 pr->name, readBuffer);
	    	pPvt->errCount++;
            	goto finish;
    	    
    	    }
        }

 	strcpy(&responseBuffer[30*i],pstartdata);
    }
    /* for successful read set err field=0 */
    pPvt->errCount=0;    

/* 
* 	Process the record. This will result in the readWrite_dg routine
*       being called again, but with pact=1 
*/
finish:
    memset(pPvt->recBuf, 0, DigitelPump_BUFFER_SIZE);
    memcpy(pPvt->recBuf, responseBuffer, DigitelPump_BUFFER_SIZE);
    dbScanLock(pr);
    (*prset->process)(pr);
    dbScanUnlock(pr);
}


static void devDigitelPumpProcess(asynUser *pasynUser, 
			char *readBuffer, int *nread)
{
    dbCommon *pr = (dbCommon *)pasynUser->userPvt;
    devDigitelPumpPvt *pPvt = (devDigitelPumpPvt *)pr->dpvt;
    size_t  nwrite; 
    int eomReason;
                                           
    pPvt->pasynUser->timeout = DigitelPump_TIMEOUT;

/*
*	These are set in startup file:
*	the default EOS character for output to device is "\r"	
*
*	the default EOS character for input from device for MPC is "\r" 
*	the default EOS character for input from device for Digitel is "\n\r" 
*/

/*   	for Digitel 500/1500 for commands set the return EOS to "*"
 *    	this code is not used. it is better to let this timeout after command
 *    	and read the values than change the terminator 
    
    char inputEos[3];
    if ((pPvt->devType > 0) && (pPvt->command == cmdControl)) {
	strcpy(inputEos,"*");
        pPvt->status = pPvt->pasynOctet->setInputEos(pPvt->octetPvt, pasynUser, 
                            inputEos, strlen(inputEos));
    }
 */
    pPvt->status = pPvt->pasynOctet->write(pPvt->octetPvt, pasynUser, 
                            pPvt->sendBuf, strlen(pPvt->sendBuf), &nwrite);

    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
             	"devDigitelPumpProcess %s nwrite=%d output=[%s]\n",
              	pr->name, nwrite, pPvt->sendBuf);
    
    pPvt->status = pPvt->pasynOctet->read(pPvt->octetPvt, pasynUser, 
                            readBuffer, DigitelPump_SIZE, &nwrite, &eomReason);

    *nread = nwrite;
    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
              	"devDigitelPumpProcess %s nread=%d input=[%s]\n",
              	pr->name, *nread, readBuffer);

/*
 *  	for Digitel 500/1500 the reply for commands is only "\n*" 
 *	for successful write and proper reply lets set the return to *
 *	also reset the EOS back to "\n\r"
 *	this code is not used now. better not change these inside the code
    if ((pPvt->devType > 0) && (pPvt->command == cmdControl)) {
	strcpy(inputEos,"\n\r");
        pPvt->status = pPvt->pasynOctet->setInputEos(pPvt->octetPvt, pasynUser, 
                            inputEos, strlen(inputEos));
    }
*/
}

