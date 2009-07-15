/* devIao1.h */

/*
 *      Author:  Marty Kraimer
 *      Date:   July 2,1997
*/
/*************************************************************************
* Copyright (c) 2003 The University of Chicago, as Operator of Argonne
* National Laboratory, the Regents of the University of California, as
* Operator of Los Alamos National Laboratory, and Leland Stanford Junior
* University, as the Operator of Stanford Linear Accelerator.
* This code is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
*************************************************************************/
/* devIao1: synchronous interface to an analog output device record*/
#ifndef boolean
#define boolean char
#endif

/*Standard returns. Others are also possible*/
#define devIao1OK             0
#define devIao1HardwareFault  1

/*Interface definition*/
typedef struct devIao1{
    int (*connect) (void **pdevIao1Pvt,DBLINK *plink, boolean *isEng);
    char *(*get_status_message) (void *pdevIao1Pvt,int status);
    int (*get_linconv) (void *devIao1Pvt,long *range,long *roff);
    int (*get_raw) (void *devIao1Pvt,long *praw);
    int (*get_eng) (void *devIao1Pvt,double *peng);
    int (*put_raw) (void *devIao1Pvt,long raw);
    int (*put_eng) (void *devIao1Pvt,double eng);
} devIao1;
