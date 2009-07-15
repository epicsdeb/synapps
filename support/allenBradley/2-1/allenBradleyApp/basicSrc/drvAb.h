/* drvAb.h */
/* header file for the Allen-Bradley Remote Serial IO
 * This defines interface between driver and device support
 *
 * Author:	Marty Kraimer
 * Date:	03-06-95
 */
/*************************************************************************
* Copyright (c) 2003 The University of Chicago, as Operator of Argonne
* National Laboratory, the Regents of the University of California, as
* Operator of Los Alamos National Laboratory, and Leland Stanford Junior
* University, as the Operator of Stanford Linear Accelerator.
* This code is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
*************************************************************************/

#ifndef INCdrvAbh
#define INCdrvAbh 1
#include "dbScan.h"


/* interface types */
typedef enum {typeNotAssigned,typeBi,typeBo,typeBiBo,typeAi,typeAo,typeBt}
	cardType;
/* status values*/
typedef enum{abSuccess,abNewCard,abCardConflict,abNoCard,abNotInitialized,
	abBtqueued,abBusy,abTimeout,abAdapterDown,abFailure} abStatus;
extern char **abStatusMessage;

typedef enum{abBitNotdefined,abBit8,abBit16,abBit32} abNumBits;
extern char **abNumBitsMessage;

/*entry table for dev to drv routines*/
typedef struct {
    abStatus (*registerCard)
	(unsigned short link,unsigned short adapter, unsigned short card,
	cardType type, const char *card_name,
	void (*callback)(void *drvPvt),
	void  **drvPvt);
    void (*getLocation)
	(void *drvPvt,
	unsigned short *link, unsigned short *adapter,unsigned short *card);
    abStatus (*setNbits)(void *drvPvt, abNumBits nbits);
    void (*setUserPvt)(void *drvPvt, void *userPvt);
    void *(*getUserPvt)(void *drvPvt);
    abStatus (*getStatus)(void *drvPvt);
    abStatus(*startScan)
	(void *drvPvt, unsigned short update_rate,
	unsigned short *pwrite_msg, unsigned short write_msg_len,
	unsigned short *pread_msg, unsigned short read_msg_len);
    abStatus(*updateAo)(void *drvPvt);
    abStatus(*updateBo) (void *drvPvt,unsigned long value,unsigned long mask);
    abStatus(*readBo) (void *drvPvt,unsigned long *value,unsigned long mask);
    abStatus(*readBi) (void *drvPvt,unsigned long *value,unsigned long mask);
    abStatus(*btRead)(void *drvPvt,unsigned short *pread_msg,
	unsigned short read_msg_len);
    abStatus(*btWrite)(void *drvPvt,unsigned short *pwrite_msg,
	unsigned short write_msg_len);
    abStatus (*adapterStatus)
	(unsigned short link,unsigned short adapter);
    abStatus (*cardStatus)
	(unsigned short link,unsigned short adapter, unsigned short card);
}abDrv; 

extern abDrv *pabDrv;

int ab_reset(void);
int ab_reset_link(int firstLink,int lastLink);
int abConfigNlinks(int nlinks);
int abConfigVme(int link, int base, int vector, int level);
int abConfigBaud(int link, int baud);
int abConfigScanList(int link, int scan_list_len, char *scan_list);
int abConfigScanListAscii(int link, char *filename,int setRackSize);
int abConfigAuto(int link);

#endif /*INCdrvAbh*/
