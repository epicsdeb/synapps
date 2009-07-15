/* abDcm.h */
/*
 *      Author:  Marty Kraimer
 *      Date:   July 22, 1996
 */

/*************************************************************************
* Copyright (c) 2003 The University of Chicago, as Operator of Argonne
* National Laboratory, the Regents of the University of California, as
* Operator of Los Alamos National Laboratory, and Leland Stanford Junior
* University, as the Operator of Stanford Linear Accelerator.
* This code is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
*************************************************************************/

typedef enum {
	abDcmOK,
	abDcmConnectConflict,
	abDcmError,
	abDcmNoRecord,
	abDcmIllegalTable,
	abDcmIllegalWord,
	abDcmOutMsgActive,
	abDcmAbError
} abDcmStatus;
typedef void (*dcmPutCallback)(void *userPvt,abDcmStatus status);

/*entry table for dev to abDcm routines*/
typedef struct abDcm{
	char	**abDcmStatusMessage;
	abDcmStatus (*connect)
		(void **pabDcmPvt,char *recordname,
		unsigned short table,unsigned short word,unsigned short nwords);
	abDcmStatus (*get_ioint_info) (void *abDcmPvt,int cmd, IOSCANPVT *ppvt);
	abDcmStatus (*get_float) (void *abDcmPvt,float *pval);
	abDcmStatus (*get_ushort) (void *abDcmPvt,unsigned short *pval);
	abDcmStatus (*get_short) (void *abDcmPvt,short *pval);
	abDcmStatus (*put_float) (void *abDcmPvt,unsigned short tag,
		float val, dcmPutCallback callback,void *userPvt);
	abDcmStatus (*put_ushort) (void *abDcmPvt,unsigned short tag,
		unsigned short mask, unsigned short val,
		dcmPutCallback callback,void *userPvt);
	abDcmStatus (*put_short) (void *abDcmPvt,unsigned short tag,
		unsigned short val,
		dcmPutCallback callback,void *userPvt);
} abDcm;
/*Global name for abDcm is abDcmTable. It should be looked up at init time*/
