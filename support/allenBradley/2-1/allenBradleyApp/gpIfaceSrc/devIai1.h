/* devIai1.h */

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
/* devIai1: synchronous interface to an analog input device record*/
#ifndef boolean
#define boolean char
#endif

/*Standard returns. Others are also possible*/
#define devIai1OK             0
#define devIai1HardwareFault  1
#define devIai1Underflow      2
#define devIai1Overflow       3

/*Interface definition*/
typedef struct devIai1{
    int (*connect) (void **pdevIai1Pvt,DBLINK *plink, boolean *isEng);
    char *(*get_status_message) (void *pdevIai1Pvt,int status);
    int (*get_ioint_info) (void *devIai1Pvt,int cmd, IOSCANPVT *ppvt);
    int (*get_linconv) (void *devIai1Pvt,long *range,long *roff);
    int (*get_raw) (void *devIai1Pvt,long *praw);
    int (*get_eng) (void *devIai1Pvt,double *peng);
} devIai1;
