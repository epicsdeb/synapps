/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* Revision-Id: anj@aps.anl.gov-20101005192737-disfz3vs0f3fiixd */
/* Author:  Marty Kraimer Date:    04-19-94 */


#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#define epicsExportSharedSymbols
#include "cantProceed.h"
#include "epicsMutex.h"
#include "myFreeList.h"
#include "adjustment.h"

typedef struct allocMem {
    struct allocMem	*next;
    void		*memory;
}allocMem;

typedef struct {
    int		size;
    int		nmalloc;
    void	*head;
    allocMem	*mallochead;
    size_t	nBlocksAvailable;
    size_t	nTotalBlocks;
    epicsTimeStamp timeLastUsed;
    epicsMutexId lock;
} MYFREELISTPVT;

void myFreeListInitPvt(void **ppvt,int size,int nmalloc)
{
    MYFREELISTPVT	*pfl;

    pfl = callocMustSucceed(1,sizeof(MYFREELISTPVT), "myFreeListInitPvt");
    pfl->size = adjustToWorstCaseAlignment(size);
    pfl->nmalloc = nmalloc;
    pfl->head = NULL;
    pfl->mallochead = NULL;
    pfl->nBlocksAvailable = 0u;
    pfl->nTotalBlocks = 0u;
    pfl->lock = epicsMutexMustCreate();
    *ppvt = (void *)pfl;
    epicsTimeGetCurrent(&pfl->timeLastUsed);
    return;
}

void *myFreeListCalloc(void *pvt)
{
    MYFREELISTPVT *pfl = pvt;
#   ifdef EPICS_MYFREELIST_DEBUG
        return callocMustSucceed(1,pfl->size,"myFreeList Debug Calloc");
#   else
        void	*ptemp;

        ptemp = myFreeListMalloc(pvt);
        if(ptemp) memset((char *)ptemp,0,pfl->size);
        return(ptemp);
#   endif
}


void *myFreeListMalloc(void *pvt)
{
    MYFREELISTPVT *pfl = pvt;
#   ifdef EPICS_MYFREELIST_DEBUG
        return callocMustSucceed(1,pfl->size,"myFreeList Debug Malloc");
#   else
        void	*ptemp;
        void	**ppnext;
        allocMem	*pallocmem;
        int		i;

        epicsMutexMustLock(pfl->lock);
        ptemp = pfl->head;
        if(ptemp==0) {
	    ptemp = (void *)malloc(pfl->nmalloc*pfl->size);
	    if(ptemp==0) {
	        epicsTimeGetCurrent(&pfl->timeLastUsed);
	        epicsMutexUnlock(pfl->lock);
	        return(0);
	    }
	    pallocmem = (allocMem *)calloc(1,sizeof(allocMem));
	    if(pallocmem==0) {
	        epicsTimeGetCurrent(&pfl->timeLastUsed);
	        epicsMutexUnlock(pfl->lock);
	        free(ptemp);
	        return(0);
	    }
	    pallocmem->memory = ptemp;
	    if(pfl->mallochead)
	        pallocmem->next = pfl->mallochead;
	    pfl->mallochead = pallocmem;
	    for(i=0; i<pfl->nmalloc; i++) {
	        ppnext = ptemp;
	        *ppnext = pfl->head;
	        pfl->head = ptemp;
	        ptemp = ((char *)ptemp) + pfl->size;
	    }
	    ptemp = pfl->head;
            pfl->nBlocksAvailable += pfl->nmalloc;
            pfl->nTotalBlocks += pfl->nmalloc;
        }
        ppnext = pfl->head;
        pfl->head = *ppnext;
        pfl->nBlocksAvailable--;
        epicsTimeGetCurrent(&pfl->timeLastUsed);
        epicsMutexUnlock(pfl->lock);
        return(ptemp);
#   endif
}

void myFreeListFree(void *pvt,void*pmem)
{
    MYFREELISTPVT	*pfl = pvt;
#   ifdef EPICS_MYFREELIST_DEBUG
        memset ( pmem, 0xdd, pfl->size );
        free(pmem);
#   else
        void	**ppnext;

        epicsMutexMustLock(pfl->lock);
        ppnext = pmem;
        *ppnext = pfl->head;
        pfl->head = pmem;
        pfl->nBlocksAvailable++;
        epicsTimeGetCurrent(&pfl->timeLastUsed);
        epicsMutexUnlock(pfl->lock);
#   endif
}

void myFreeListCleanup(void *pvt)
{
    MYFREELISTPVT *pfl = pvt;
    allocMem	*phead;
    allocMem	*pnext;

    phead = pfl->mallochead;
    while(phead) {
	pnext = phead->next;
	free(phead->memory);
	free(phead);
	phead = pnext;
    }
    epicsMutexDestroy(pfl->lock);
    free(pvt);
}

size_t myFreeListItemsAvail(void *pvt)
{
    MYFREELISTPVT *pfl = pvt;
    size_t n;

    epicsMutexMustLock(pfl->lock);
    n = pfl->nBlocksAvailable;
    epicsMutexUnlock(pfl->lock);
    return n;
}

size_t myFreeListItemsTotal(void *pvt)
{
    MYFREELISTPVT *pfl = pvt;
    size_t n;

    epicsMutexMustLock(pfl->lock);
    n = pfl->nTotalBlocks;
    epicsMutexUnlock(pfl->lock);
    return n;
}

epicsTimeStamp myFreeListTimeLastUsed(void *pvt)
{
    MYFREELISTPVT *pfl = pvt;
    epicsTimeStamp ts;

    epicsMutexMustLock(pfl->lock);
    ts = pfl->timeLastUsed;
    epicsMutexUnlock(pfl->lock);
    return ts;
}
