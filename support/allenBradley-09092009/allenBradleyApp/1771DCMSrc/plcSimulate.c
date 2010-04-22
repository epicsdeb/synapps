/*************************************************************************
* Copyright (c) 2003 The University of Chicago, as Operator of Argonne
* National Laboratory, the Regents of the University of California, as
* Operator of Los Alamos National Laboratory, and Leland Stanford Junior
* University, as the Operator of Stanford Linear Accelerator.
* This code is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
*************************************************************************/
#include <vxWorks.h>
#include <tickLib.h>
#include <taskLib.h>
#include <semLib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <logLib.h>

#include "dbDefs.h"
#include "errlog.h"
#include "subRecord.h"
#include "drvAb.h"
	

#define NUM_DCM_WORDS	64
static unsigned long numberGet=0;
static SEM_ID getSem;
static SEM_ID waitSem;
static abStatus waitStatus;
static unsigned short *tables;
static volatile int initialized=FALSE;

int subPlcInit(subRecord *precord)
{
	return(0);
}

int subPlcProcess(subRecord *precord)
{
    while(!initialized) {
	printf("subPlcProcess delay\n");
	taskDelay(300);
    }
    semTake(getSem,WAIT_FOREVER);
    numberGet++;
    semGive(getSem);
    return(0);
}

static void myCallback(void *drvPvt)
{

    waitStatus = (*pabDrv->getStatus)(drvPvt);
    semGive(waitSem);
}

int plcStart(int link,int rack, int slot, int ntables, int toff)
{
    abStatus 		status;
    void		*drvPvt;
    unsigned int	table,word;
    unsigned short 	getMessage[4];
    unsigned short	*ptable;

    if(ntables<=0 || toff<=0) {
	printf("usage: plcStart(link,rack,slot,ntables,toff)\n");
	return(-11);
    }
    if((getSem = semBCreate(SEM_Q_FIFO,SEM_FULL))==NULL) {
	printf("plcStart semBCreate failed\n");
	return(-1);
    }
    if((waitSem = semBCreate(SEM_Q_FIFO,SEM_EMPTY))==NULL) {
	printf("plcStart semBCreate failed\n");
	return(-1);
    }
    tables = calloc(ntables*NUM_DCM_WORDS, sizeof(unsigned short));
    for(table=0; table<ntables; table++) {
	ptable = tables + table*NUM_DCM_WORDS;
	for(word=0; word<NUM_DCM_WORDS; word++) ptable[word] = 0;
	ptable[toff] = table;
    }
    status = (*pabDrv->registerCard)(link,rack,slot,
	typeBt,"abDcm",myCallback,&drvPvt);
    if(status!=abNewCard) {
	if(status==abSuccess) {
	    errlogPrintf("plcStart: card already in use\n");
	 } else {
	    errlogPrintf("plcStart: registerCard error %s\n",
		abStatusMessage[status]);
	    return(-1);
	}
    }
    initialized = TRUE;
    while(TRUE) {
	int	gotInput;

	while(TRUE) {
	    semTake(getSem,WAIT_FOREVER);
	    if(numberGet>0) {
		--numberGet;
		gotInput = TRUE;
	    } else {
		gotInput = FALSE;
	    }
	    semGive(getSem);
	    if(!gotInput) break;
	    status = (*pabDrv->btRead)(drvPvt,&getMessage[0],4);
	    if(status!=abBtqueued) {
		printf("btRead failed status = %s\n",abStatusMessage[status]);
	    } else {
		semTake(waitSem,WAIT_FOREVER);
		if(waitStatus!=abSuccess) {
		    printf("btRead failed status = %s\n",
			abStatusMessage[waitStatus]);
		} else {
		    table = getMessage[1]/100;
		    word = getMessage[1]%100;
		    if(table>1 || word<4 || word>=NUM_DCM_WORDS) {
		 	printf("plcStart: Illegal message table %d word %d\n",
			    table,word);
		    } else {
			if(table==0) {
			    ptable = tables + table*NUM_DCM_WORDS;
			    ptable[word] = getMessage[2];
			    ptable[word+1] = getMessage[3];
			} else {
			    unsigned short value,mask;

			    mask = getMessage[2];
			    value = getMessage[3];
			    value = (value & ~mask) | (value & mask);
			    ptable = tables + table*NUM_DCM_WORDS;
			    ptable[word] = value;
			}
		    }
		}
	    }
	}
	for(table=0; table<ntables; table++) {
	    ptable = tables + table*NUM_DCM_WORDS;
	    status = (*pabDrv->btWrite)(drvPvt,ptable,NUM_DCM_WORDS);
	    if(status!=abBtqueued) {
	 	printf("btWrite failed status = %s\n",abStatusMessage[status]);
	    } else {
		semTake(waitSem,WAIT_FOREVER);
		if(waitStatus!=abSuccess) {
		    printf("btWrite failed status = %s\n",
			abStatusMessage[waitStatus]);
		}
	    }
	}
    }
    return(0);
}
