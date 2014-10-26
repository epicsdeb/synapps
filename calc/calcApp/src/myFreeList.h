/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* share/epicsH/myFreeList.h	*/
/* share/epicsH Revision-Id: anj@aps.anl.gov-20101005192737-disfz3vs0f3fiixd */
/* Author:  Marty Kraimer Date:    04-19-94	*/

#ifndef INCmyFreeListh
#define INCmyFreeListh

#include <stddef.h>
#include <epicsTime.h>
#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

void myFreeListInitPvt(void **ppvt,int size,int nmalloc);
void *myFreeListCalloc(void *pvt);
void *myFreeListMalloc(void *pvt);
void myFreeListFree(void *pvt,void*pmem);
void myFreeListCleanup(void *pvt);
size_t myFreeListItemsAvail(void *pvt);
size_t myFreeListItemsTotal(void *pvt);
epicsTimeStamp myFreeListTimeLastUsed(void *pvt);

#define freeListInitPvt(a,b,c)	myFreeListInitPvt((a),(b),(c))
#define freeListCalloc(a)		myFreeListCalloc((a))
#define freeListMalloc(a)		myFreeListMalloc((a))
#define freeListFree(a,b)		myFreeListFree((a),(b))
#define freeListCleanup(a)		myFreeListCleanup((a))
#define freeListItemsAvail(a)	myFreeListItemsAvail((a))
#define freeListItemsTotal(a)	myFreeListItemsTotal((a))
#define freeListTimeLastUsed(a)	myFreeListTimeLastUsed((a))

#ifdef __cplusplus
}
#endif

#endif /*INCmyFreeListh*/
