/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1991, The Regents of the University of California.
		         Los Alamos National Laboratory
	seq_prog.c,v 1.2 1995/06/27 15:26:00 wright Exp


	DESCRIPTION: Seq_prog.c: state program list management functions.
	All active state programs are inserted into the list when created
	and removed from the list when deleted.

	ENVIRONMENT: VxWorks

	HISTORY:
09dec91,ajk	original
29apr92,ajk	Added mutual exclusion locks	
17Jul92,rcz	Changed semBCreate call for V5 vxWorks; should be semMCreate?
18feb00,wfl	Avoided memory leak.
29feb00,wfl	Supported new OSI.
31mar00,wfl	Added seqFindProgByName().
***************************************************************************/
/*#define	DEBUG*/

#define epicsExportSharedSymbols
#include	<string.h>
#include	"seq.h"

LOCAL	epicsMutexId seqProgListSemId;
LOCAL	int	    seqProgListInited = FALSE;
LOCAL	ELLLIST	    seqProgList;
LOCAL	void	    seqProgListInit();

typedef struct prog_node
{
	ELLNODE		node;
	SPROG		*pSP;
} PROG_NODE;

#define	seqListFirst(pList)	(PROG_NODE *)ellFirst((ELLLIST *)pList)

#define	seqListNext(pNode)	(PROG_NODE *)ellNext((ELLNODE *)pNode)

/*
 * seqFindProg() - find a program in the state program list from thread id.
 */
SPROG *seqFindProg(epicsThreadId threadId)
{
	PROG_NODE	*pNode;
	SPROG		*pSP;
	SSCB		*pSS;
	int		n;

	if (!seqProgListInited || threadId == 0)
		return NULL;

	epicsMutexMustLock(seqProgListSemId);

	for (pNode = seqListFirst(&seqProgList); pNode != NULL;
	     pNode = seqListNext(pNode) )
	{
		pSP = pNode->pSP;
		if (pSP->threadId == threadId)
		{
			epicsMutexUnlock(seqProgListSemId);
			return pSP;
		}
		pSS = pSP->pSS;
		for (n = 0; n < pSP->numSS; n++, pSS++)
		{
			if (pSS->threadId == threadId)
			{
				epicsMutexUnlock(seqProgListSemId);
				return pSP;
			}
		}
	}
	epicsMutexUnlock(seqProgListSemId);

	return NULL; /* not in list */
}

/*
 * seqFindProgByName() - find a program in the state program list by name.
 */
epicsShareFunc SPROG *epicsShareAPI seqFindProgByName(char *pProgName)
{
	PROG_NODE	*pNode;
	SPROG		*pSP;

	if (!seqProgListInited || pProgName == 0)
		return NULL;

	epicsMutexMustLock(seqProgListSemId);

	for (pNode = seqListFirst(&seqProgList); pNode != NULL;
	     pNode = seqListNext(pNode) )
	{
		pSP = pNode->pSP;
		if (strcmp(pSP->pProgName, pProgName) == 0)
		{
			epicsMutexUnlock(seqProgListSemId);
			return pSP;
		}
	}
	epicsMutexUnlock(seqProgListSemId);

	return NULL; /* not in list */
}

/*
 * seqTraverseProg() - travers programs in the state program list and
 * call the specified routine or function.  Passes one parameter of
 * pointer size.
 */
epicsShareFunc epicsStatus seqTraverseProg(pFunc, param)
void		(*pFunc)();	/* function to call */
void		*param;		/* any parameter */
{
	PROG_NODE	*pNode;
	SPROG		*pSP;

	if (!seqProgListInited)
		return ERROR;

	epicsMutexMustLock(seqProgListSemId);
	for (pNode = seqListFirst(&seqProgList); pNode != NULL;
	     pNode = seqListNext(pNode) )
	{
		pSP = pNode->pSP;
		pFunc(pSP, param);
	}

	epicsMutexUnlock(seqProgListSemId);
	return OK;
}

/*
 * seqAddProg() - add a program to the state program list.
 * Returns ERROR if program is already in list, else TRUE.
 */
epicsShareFunc epicsStatus seqAddProg(pSP)
SPROG		*pSP;
{
	PROG_NODE	*pNode;

	if (!seqProgListInited)
		seqProgListInit(); /* Initialize list */

	epicsMutexMustLock(seqProgListSemId);
	for (pNode = seqListFirst(&seqProgList); pNode != NULL;
	     pNode = seqListNext(pNode) )
	{

		if (pSP == pNode->pSP)
		{
			epicsMutexUnlock(seqProgListSemId);
#ifdef DEBUG
			errlogPrintf("Thread %d already in list\n",
				     pSP->threadId);
#endif /*DEBUG*/
			return ERROR; /* already in list */
		}
	}

	/* Insert at head of list */
	pNode = (PROG_NODE *)malloc(sizeof(PROG_NODE) );
	if (pNode == NULL)
	{
		epicsMutexUnlock(seqProgListSemId);
		return ERROR;
	}

	pNode->pSP = pSP;
	ellAdd((ELLLIST *)&seqProgList, (ELLNODE *)pNode);
	epicsMutexUnlock(seqProgListSemId);
#ifdef DEBUG
	errlogPrintf("Added thread %d to list.\n", pSP->threadId);
#endif /*DEBUG*/

	return OK;
}

/* 
 *seqDelProg() - delete a program from the program list.
 * Returns TRUE if deleted, else FALSE.
 */
epicsShareFunc epicsStatus seqDelProg(pSP)
SPROG		*pSP;
{
	PROG_NODE	*pNode;

	if (!seqProgListInited)
		return ERROR;

	epicsMutexMustLock(seqProgListSemId);
	for (pNode = seqListFirst(&seqProgList); pNode != NULL;
	     pNode = seqListNext(pNode) )
	{
		if (pNode->pSP == pSP)
		{
			ellDelete((ELLLIST *)&seqProgList, (ELLNODE *)pNode);
			free(pNode);
			epicsMutexUnlock(seqProgListSemId);

#ifdef DEBUG
			errlogPrintf("Deleted thread %d from list.\n",
				     pSP->threadId);
#endif /*DEBUG*/
			return OK;
		}
	}	

	epicsMutexUnlock(seqProgListSemId);
	return ERROR; /* not in list */
}

/*
 * seqProgListInit() - initialize the state program list.
 */
LOCAL void seqProgListInit()
{
	/* Init linked list */
	ellInit(&seqProgList);

	/* Create a semaphore for mutual exclusion */
	seqProgListSemId = epicsMutexMustCreate();
	seqProgListInited = TRUE;
}

