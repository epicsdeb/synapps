/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* aCalcPerform.c,v 1.7 2004/08/30 19:03:40 mooney Exp */
/*
 *	Original Author: Julie Sander and Bob Dalesio
 *	Date:	         07-27-87
 */
#ifdef vxWorks
#include <vxWorks.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "dbDefs.h"
#include "epicsMath.h"
#include "cvtFast.h"
#include "epicsString.h"

#define epicsExportSharedSymbols
#include "aCalcPostfix.h"
#include "aCalcPostfixPvt.h"
#include <epicsExport.h>
#include <epicsMutex.h>
#include <epicsTime.h>

/* base currently (3.15.0.1) does not have the freeList behavior needed to
 * manage a list of freeLists as aCalcPerform needs.  If it ever gets the
 * needed behavior, we should use it and get rid of myFreeList*.
 */
#define USE_BASE_FREELIST 0
#if USE_BASE_FREELIST
#include <freeList.h>
#else
#include <myFreeList.h>
#endif

/* Note value much larger than this breaks MEDM's plot */
#define	myMAXFLOAT	((float)1e+35)
#define myNINT(a) ((int)((a) >= 0 ? (a)+0.5 : (a)-0.5))
#ifndef PI
#define PI 3.14159265358979323
#endif
#define myMAX(a,b) (a)>(b)?(a):(b)
#define myMIN(a,b) (a)<(b)?(a):(b)
#define SMALL 1.e-9

static double local_random();
static int cond_search(const unsigned char **ppinst, int match);

/* from calcUtil */
extern int deriv(double *x, double *y, int n, double *d);
extern int nderiv(double *x, double *y, int n, double *d, int m, double *work);
int fitpoly(double *x, double *y, int n,
	double *a0, double *a1, double *a2, double *mask);

#define DEBUG 1
volatile int aCalcPerformDebug = 0;
epicsExportAddress(int, aCalcPerformDebug);
volatile int aCalcLoopMax = 1000;
epicsExportAddress(int, aCalcLoopMax);

typedef struct {
	double d;
	double *a;
	double *array;
	int firstEl;
	int numEl;
	int sourceDouble; /* number of double argument from which this stack element was copied */
} stackElement;

#if DEBUG
int aCalcStackHW = 0;	/* high-water mark */
int aCalcStackLW = 0;	/* low-water mark */
#define INC(ps) {							\
	++ps;									\
	if ((int)((ps)-top) > aCalcStackHW)		\
		aCalcStackHW = (int)((ps)-top);		\
	if ((ps-top)>ACALC_STACKSIZE) {			\
		printf("aCalcPerform:stack overflow\n");	\
		freeStack(flp, stack); return(-1);	\
	} else {								\
		(ps)->numEl = -1;					\
		(ps)->sourceDouble=-1;				\
	}										\
}
#define DEC(ps) {							\
	--ps;									\
	if ((int)((ps)-top) < aCalcStackLW)		\
		aCalcStackLW = (int)((ps)-top);		\
	if ((ps-top)<-1) {						\
		printf("aCalcPerform:stack underflow\n");	\
		freeStack(flp, stack); return(-1);	\
	}										\
}

#else
#define INC(ps) {++ps; (ps)->numEl = -1; (ps)->sourceDouble=-1;}
#define DEC(ps) ps--
#endif

/*** begin convert stack element between array and double ***/

#define isDouble(ps) ((ps)->a==NULL)
#define isArray(ps) ((ps)->a != NULL)

/* convert stack element of unknown type to double */
#define toDouble(ps) {if (isArray(ps)) to_double(ps);}

/* convert array-valued stack element to double */
#define to_double(ps) {(ps)->d = (ps)->a[0]; (ps)->a = NULL;}

/* convert double-valued stack element to array */
int to_array(void *flp, stackElement *ps, int arraySize, int setValues) {
	int ii;
	if (ps->array == NULL) {
		ps->array = (double *)freeListMalloc(flp);
		if (ps->array == NULL) {
			return(-1);
		}
	}
	ps->a = &(ps->array[0]);
	ps->numEl = -1;

	if (setValues) {
		if (isnan(ps->d))
			for(ii=0; ii<arraySize; ii++) ps->a[ii]=0.;
		else
			for(ii=0; ii<arraySize; ii++) ps->a[ii]=ps->d;
	}

	return(0);
}

/* convert stack element of unknown type to array */
#define toArray(ps, setValues) {									\
	if (isDouble(ps)) {												\
		if (to_array(flp, (ps), arraySize, (setValues)) == -1) {	\
			printf("aCalcPerform: Can't allocate array.\n");		\
			freeStack(flp, stack);									\
			return(-1);												\
		}															\
	}																\
}

/*** end convert stack element between array and double ***/

/*** begin manage an array of freeLists ***/

/* an fList is a list of freeLists */
epicsMutexId fListLock=0;

typedef struct {
	void *freeListPvt;
	int numDoubles;
} fListElement;

#define NLISTS 5
fListElement fList[NLISTS] = {{0}};

void *get_freeList(int nuse) {
	int i, j, n;
	void *flp;
	epicsTimeStamp ts, ts1;

	epicsMutexMustLock(fListLock);

	/* exact size match? */
	for (i=0; i<NLISTS; i++) {
		if (fList[i].numDoubles == nuse) {
			if (aCalcPerformDebug>10) printf("aCalcPerform:get_freeList found list of size %d\n", nuse);
			epicsMutexUnlock(fListLock);
			return(fList[i].freeListPvt);			
		}
	}

	/* good enough size match? */
	for (i=0; i<NLISTS; i++) {
		n = fList[i].numDoubles;
		if (n >= nuse && n <= nuse*2) {
			if (aCalcPerformDebug>10) printf("aCalcPerform:get_freeList wanted size %d, found %d\n", nuse, n);
			epicsMutexUnlock(fListLock);
			return(fList[i].freeListPvt);			
		}
	}

	/* if there is an unused fListElement, take it */
	for (i=0; i<NLISTS; i++) {
		if (fList[i].freeListPvt == 0) {
			if (aCalcPerformDebug>10) printf("aCalcPerform:get_freeList new list of size %d\n", nuse);
			freeListInitPvt(&fList[i].freeListPvt, nuse*sizeof(double), 1);
			fList[i].numDoubles = nuse;
			epicsMutexUnlock(fListLock);
			return(fList[i].freeListPvt);
		}
	}

	/* if there is an list with no outstanding memory, clean it up and alloc for caller */
	/* if more than one, take least recently used */
	epicsTimeGetCurrent(&ts);
	for (i=0, j=-1; i<NLISTS; i++) {
		flp = fList[i].freeListPvt;
		if (flp && (freeListItemsAvail(flp) == freeListItemsTotal(flp))) {
			ts1 = freeListTimeLastUsed(flp);
			if (epicsTimeGreaterThan(&ts, &ts1)) {
				j = i;
				ts = ts1;
			}
		}
	}
	if (j>=0) {
		if (aCalcPerformDebug>10)
			printf("aCalcPerform:get_freeList delete old list of %d; alloc new list of %d\n",
				fList[j].numDoubles, nuse);
		flp = fList[j].freeListPvt;
		freeListCleanup(flp);
		freeListInitPvt(&fList[j].freeListPvt, nuse*sizeof(double), 1);
		fList[j].numDoubles = nuse;
		epicsMutexUnlock(fListLock);
		return(fList[j].freeListPvt);
	}

	/* size match that will at least work? */
	for (i=0; i<NLISTS; i++) {
		n = fList[i].numDoubles;
		if (n >= nuse) {
			if (aCalcPerformDebug>10) printf("aCalcPerform:get_freeList wanted size %d, found %d\n", nuse, n);
			epicsMutexUnlock(fListLock);
			return(fList[i].freeListPvt);			
		}
	}

	/* caller is out of luck */
	epicsMutexUnlock(fListLock);
	return(0);
}

/*** end manage an array of freeLists ***/

/*******************************************************/

struct until_struct {
	const unsigned char *until_loc;
	const unsigned char *until_end_loc;
	double *pd;
	stackElement *ps;
};

void freeStack(void *flp, stackElement *stack) {
	int i;
	for (i=0; i<ACALC_STACKSIZE; i++) {
		if (stack[i].array) freeListFree(flp, stack[i].array);
	}
	free(stack);
}

void calcFirstLast(stackElement *ps, int *firstEl, int *lastEl, int arraySize) {
	if (ps->numEl != -1) {
		*firstEl = ps->firstEl; *lastEl = ps->firstEl + ps->numEl - 1;
	} else {
		*firstEl = 0; *lastEl = arraySize-1;
	}
}

#define MAX_UNTIL_OP 10
long aCalcPerform(double *p_dArg, int num_dArgs, double **pp_aArg,
	int num_aArgs, int arraySize, double *p_dresult, double *p_aresult,
	const unsigned char *postfix, const int allocSize, epicsUInt32 *amask) {

	stackElement *stack, *top;
	stackElement *ps, *ps1, *ps2, *ps3;
	int					i, j, k, found, status, op, nargs;
	double				d, e, f, *pd;
	const unsigned char *post = postfix;
	struct until_struct	until_scratch[MAX_UNTIL_OP];
	int					loopsDone = 0;
	void *flp;
	int firstEl, lastEl, firstEl1, lastEl1;

	if (*postfix == END_EXPRESSION) {
		return(-1);
	}

	if (fListLock==0) fListLock = epicsMutexMustCreate();
	flp = get_freeList(arraySize);
	if (flp == 0) {
		printf("aCalcPerform: Can't allocate value stack\n");
		return(-1);
	}

	if (aCalcPerformDebug>10) printf("aCalcPerform: freeListItemsAvail=%ld of: %ld\n",
		(long)freeListItemsAvail(flp), (long)freeListItemsTotal(flp));

	*amask = 0; /* init bit mask that will record the array fields we wrote to. */

	stack = calloc(ACALC_STACKSIZE, sizeof(stackElement));
	if (stack == NULL) {
		printf("aCalcPerform: Can't allocate stack.\n");
		return(-1);
	}

#if 0
	printf("aCalcPerform: stack=%p\n", stack);
	printf("aCalcPerform: &(stack[0])=%p\n", &(stack[0]));
	printf("aCalcPerform: &(stack[0].d)=%p\n", &(stack[0].d));
	printf("aCalcPerform: &(stack[0].a)=%p\n", &(stack[0].a));
	printf("aCalcPerform: &(stack[0].array)=%p\n", &(stack[0].array));
	/*printf("aCalcPerform: &(stack[0].array[0])=%p\n", &(stack[0].array[0]));*/

	printf("aCalcPerform: &(stack[1])=%p\n", &(stack[1]));
	printf("aCalcPerform: &(stack[1].d)=%p\n", &(stack[1].d));
	printf("aCalcPerform: &(stack[1].a)=%p\n", &(stack[1].a));
	printf("aCalcPerform: &(stack[1].array)=%p\n", &(stack[1].array));
	/*printf("aCalcPerform: &(stack[1].array[1])=%p\n", &(stack[1].array[1]));*/
#endif

	for (i=0; i<MAX_UNTIL_OP; i++) {
		until_scratch[i].until_loc = NULL;
		until_scratch[i].until_end_loc = NULL;
		until_scratch[i].ps = NULL;
		until_scratch[i].pd = NULL;
	}
	/* find all UNTIL operators in postfix, noting their locations */
	for (i=0, post=postfix; *post != END_EXPRESSION; post++) {
		if (aCalcPerformDebug > 10) printf("\tsCalcPerform: *post=%d\n", *post);
		switch (*post) {
		case LITERAL_DOUBLE:
			post += sizeof(double);
			break;
		case LITERAL_INT:
			post += sizeof(int);
			break;
		case UNTIL:
			/*printf("sCalcPerform: UNTIL at index %d\n", (int)(post-postfix));*/
			until_scratch[i].until_loc = post;
			i++;
			if (i > (MAX_UNTIL_OP-1)) {
				printf("sCalcPerform: too many UNTILs\n");
				freeStack(flp, stack);
				return(-1);
			}
			break;
		case UNTIL_END:
			for (k=i-1; k>=0; k--) {
				if (until_scratch[k].until_end_loc == NULL) {
					/* found unclaimed UNTIL */
					if (aCalcPerformDebug > 10) {
						printf("sCalcPerform: UNTIL_END at index %d, matches UNTIL at index %d (k=%d)\n",
							(int)(post-postfix), (int)(until_scratch[k].until_loc-postfix), k);
					}
					until_scratch[k].until_end_loc = post;
					break;
				}
			}
			if (k<0) {
				printf("unmatched UNTIL_END\n");
				freeStack(flp, stack);
				return(-1);
			}
			break;
		}
	}

	post = postfix;

#if DEBUG
	if (aCalcPerformDebug>=10) {
		printf("aCalcPerform: postfix:\n");
		aCalcExprDump(post);

		printf("\naCalcPerform: args:\n");
		for (i=0; i<num_dArgs; i++) {
			if (i%4 == 0) printf("     ");
			printf("%c=%f\t", 'a'+i, p_dArg[i]);
			if (i%4 == 3) printf("\n");
		}
		for (i=0; i<num_aArgs; i++) {
			if (pp_aArg[i]) {
				printf("%c%c=[%f %f %f...]\n",
					'a'+i, 'a'+i, pp_aArg[i][0], pp_aArg[i][1], pp_aArg[i][2]);
			}
		}
	}
#endif

	top = ps = &stack[1];
	ps--;  /* Expression handler assumes ps is pointing to a filled element */
	ps->d = 1.23456; ps->a = NULL;	/* telltale */

	status = 0;
	while ((op = *post++) != END_EXPRESSION){

		if (aCalcPerformDebug>=20) printf("aCalcPerform: op=%d\n", op);

		switch (op) {

		case FETCH_A: case FETCH_B: case FETCH_C: case FETCH_D: case FETCH_E: case FETCH_F:
		case FETCH_G: case FETCH_H: case FETCH_I: case FETCH_J: case FETCH_K: case FETCH_L:
		case FETCH_M: case FETCH_N: case FETCH_O: case FETCH_P: 
			if (num_dArgs > (op - FETCH_A)) {
				INC(ps); ps->a = NULL; ps->d = p_dArg[op - FETCH_A];
				ps->sourceDouble = op - FETCH_A;
			} else {
				/* caller didn't supply a large enough array */
				INC(ps); ps->a = NULL; ps->d = 0.;
			}
			break;

		case FETCH_AA: case FETCH_BB: case FETCH_CC: case FETCH_DD: case FETCH_EE: case FETCH_FF:
		case FETCH_GG: case FETCH_HH: case FETCH_II: case FETCH_JJ: case FETCH_KK: case FETCH_LL:
			INC(ps);
			toArray(ps,0);
			ps->a[0] = 0.;
			if (num_aArgs > (op - FETCH_AA)) {
				if (pp_aArg[op - FETCH_AA]) {
					for (i=0; i<arraySize; i++) ps->a[i] = pp_aArg[op - FETCH_AA][i];
				} else {
					for (i=0; i<arraySize; i++) ps->a[i] = 0.0;
				}
				if (aCalcPerformDebug>=20)
					printf("aCalcPerform:fetch array %d = [%f %f...]\n",
						(int)*post, ps->a[0], ps->a[1]);
			}
			break;

		case STORE_A: case STORE_B: case STORE_C: case STORE_D: case STORE_E: case STORE_F:
		case STORE_G: case STORE_H: case STORE_I: case STORE_J: case STORE_K: case STORE_L:
		case STORE_M: case STORE_N: case STORE_O: case STORE_P:
			toDouble(ps);
			if (num_dArgs > (op - STORE_A)) {
    			p_dArg[op - STORE_A] = ps->d;
			}
			DEC(ps); 
		    break;

		case STORE_AA: case STORE_BB: case STORE_CC: case STORE_DD: case STORE_EE: case STORE_FF:
		case STORE_GG: case STORE_HH: case STORE_II: case STORE_JJ: case STORE_KK: case STORE_LL:
			i = op - STORE_AA;
			if (num_aArgs > i) {
				/* Careful.  It's possible the record has not allocated the array */
				if (pp_aArg[i] == NULL) {
					pp_aArg[i] = (double *)calloc(allocSize, sizeof(double));
				}
				pd = pp_aArg[i];
				if (aCalcPerformDebug>=10) {
					printf("aCalcPerform:store array to pointer %p \n", pd);
				}
				if (pd) {
					if (isArray(ps)) {
						for (j=0; j<arraySize; j++) pd[j] = ps->a[j];
					} else {
						for (j=0; j<arraySize; j++) pd[j] = ps->d;
					}
					/* Mark this array so caller knows it has been changed. */
					*amask |= 1<<i;
					if (aCalcPerformDebug>=10) printf("amask=%x\n", *amask);
				}
			}
			DEC(ps);
			break;

		case A_STORE:
			toDouble(ps);
			ps1 = ps; DEC(ps);
			toDouble(ps);
			i = myNINT(ps->d); DEC(ps);
			if (i >= num_dArgs || i < 0) {
				printf("aCalcPerform: fetch index, %d, out of range.\n", i);
			} else {
    			p_dArg[i] = ps1->d;
			}
			break;

		case A_ASTORE:
			ps1 = ps; DEC(ps);
			toDouble(ps);
			i = myNINT(ps->d); DEC(ps);
			if (i >= num_aArgs || i < 0) {
				printf("aCalcPerform: fetch index, %d, out of range.\n", i);
			} else {
				/* Careful.  It's possible the record has not allocated the array */
				if (pp_aArg[i] == NULL) {
					pp_aArg[i] = (double *)calloc(allocSize, sizeof(double));
				}
				if (pp_aArg[i]) {
					if (isArray(ps1)) {
						for (j=0; j<arraySize; j++) pp_aArg[i][j] = ps1->a[j];
					} else {
						for (j=0; j<arraySize; j++) pp_aArg[i][j] = ps1->d;
					}
				}
				*amask |= 1<<i;
			}
			break;

		case FETCH_VAL:
			INC(ps);
			ps->a = NULL;
			ps->d = *p_dresult;
			break;

		case FETCH_AVAL:
			INC(ps);
			toArray(ps,0);
			ps->a[0] = 0.;
			for (i=0; i<arraySize; i++) ps->a[i] = p_aresult[i];
			break;

		case CONST_PI:
			INC(ps);
			ps->a = NULL;
			ps->d = PI;
			break;

		case CONST_D2R:
			INC(ps);
			ps->a = NULL;
			ps->d = PI/180.;
			break;

		case CONST_R2D:
			INC(ps);
			ps->a = NULL;
			ps->d = 180./PI;
			break;

		case CONST_S2R:
			INC(ps);
			ps->a = NULL;
			ps->d = PI/(180.*3600);
			break;

		case CONST_R2S:
			INC(ps);
			ps->a = NULL;
			ps->d = (180.*3600)/PI;
			break;

		case CONST_IX:
			INC(ps);
			toArray(ps,0);
			for (i=0; i<arraySize; i++) {
				ps->a[i] = i;
			}
			break;

		case NSMOOTH:
			calcFirstLast(ps, &firstEl, &lastEl, arraySize);
			j = ps->d; /* get npts */
			DEC(ps);
			for (k=firstEl; k<j+firstEl; k++) {
				d = ps->a[firstEl]; e = ps->a[firstEl+1]; f=ps->a[firstEl+2];
				for (i=firstEl+2; i<=lastEl-2; i++) {
					ps->a[i] = d/16 + e/4 + 3*f/8 + ps->a[i+1]/4 + ps->a[i+2]/16;
					d=e; e=f; f=ps->a[i+1];
				}
			}
			for (i=0; i<firstEl; i++) {ps->a[i] = 0;}
			for (i=lastEl+1; i<arraySize; i++) {ps->a[i] = 0;}
			break;

		case NDERIV:
			toDouble(ps);
			j = ps->d;  /* points on either side of value for fit */

			DEC(ps);
			toArray(ps,1);
			calcFirstLast(ps, &firstEl, &lastEl, arraySize);
			j = myMIN((lastEl-firstEl)/2, j);  /* constrain #points on either side to half of all points */
			ps1 = ps; /* y array */

			INC(ps); toArray(ps,0);
			ps2 = ps; /* place in which to make an x array */
			for (i=firstEl; i<=lastEl; i++) {ps2->a[i] = i;}

			INC(ps); toArray(ps,0); /* place in which to calc derivative */
			ps3 = ps;

			INC(ps); toArray(ps,0); /* workspace for nderiv */

			status = nderiv(&(ps2->a[firstEl]), &(ps1->a[firstEl]), 1+lastEl-firstEl, ps3->a, j, &(ps->a[firstEl]));
			for (i=firstEl; i<=lastEl; i++) {ps1->a[i] = ps3->a[i];}
			for (i=0; i<firstEl; i++) {ps1->a[i] = 0;}
			for (i=lastEl+1; i<arraySize; i++) {ps1->a[i] = 0;}
			DEC(ps); DEC(ps); DEC(ps);
			break;

		/* normal two-argument functions/operators (either arg can be array or scalar) */
		case ADD:
		case SUB:
		case MULT:
		case DIV:
		case MODULO:
		
			ps1 = ps;
			DEC(ps);
			if (isArray(ps) || isArray(ps1)) {
				toArray(ps,1);
				if (isArray(ps1)) {
					switch (op) {
					case ADD: for (i=0; i<arraySize; i++) {ps->a[i] += ps1->a[i];} break;
					case SUB: for (i=0; i<arraySize; i++) {ps->a[i] -= ps1->a[i];} break;
					case MULT: for (i=0; i<arraySize; i++) {ps->a[i] *= ps1->a[i];} break;
					case DIV:
						for (i=0; i<arraySize; i++) {
							if (ps1->a[i] == 0) {
								ps->a[i] = myMAXFLOAT;
							} else {
								ps->a[i] /= ps1->a[i];
							}
						}
						break;
					case MODULO:
						for (i=0; i<arraySize; i++) {
							if ((int)ps1->a[i] == 0) {
								ps->a[i] = myMAXFLOAT;
							} else {
								ps->a[i] = (double)((int)ps->a[i] % (int)ps1->a[i]);
							}
						}
						break;
					}
				} else {
					switch (op) {
					case ADD: for (i=0; i<arraySize; i++) {ps->a[i] += ps1->d;} break;
					case SUB: for (i=0; i<arraySize; i++) {ps->a[i] -= ps1->d;} break;
					case MULT: for (i=0; i<arraySize; i++) {ps->a[i] *= ps1->d;} break;
					case DIV:
						for (i=0; i<arraySize; i++) {
							if (ps1->d==0) {
								ps->a[i] = myMAXFLOAT;
							} else {
								ps->a[i] /= ps1->d;
							}
						}
						break;
					case MODULO:
						for (i=0; i<arraySize; i++) {
							if ((int)ps1->d == 0) {
								ps->a[i] = myMAXFLOAT;
							} else {
								ps->a[i] = (double)((int)ps->a[i] % (int)ps1->d);
							}
						}
						break;
					}
				}
				if (aCalcPerformDebug>=20) {
					printf("aCalcPerform:binary array op result = [\n");
					for (i=0; i<arraySize; i++) printf("%f ", ps->a[i]);
					printf("]\n");
				}
			} else { /* if (isArray(ps) || isArray(ps1)) */
				switch (op) {
				case ADD: ps->d += ps1->d; break;
				case SUB: ps->d -= ps1->d; break;
				case MULT: ps->d *= ps1->d; break;
				case DIV:
					if (ps1->d == 0) {
						ps->d = myMAXFLOAT;
					} else {
						ps->d = ps->d / ps1->d;
					}
					break;
				case MODULO:
					if ((int)ps1->d == 0) {
						ps->d = myMAXFLOAT;
					} else {
						ps->d = (double)((int)ps->d % (int)ps1->d);
					}
					break;
				}
			}
			break;

		case COND_IF:
			toDouble(ps);
			d = ps->d;
			DEC(ps);
			if (d == 0.0 &&	cond_search(&post, COND_ELSE)) {
				freeStack(flp, stack);
				return -1;
			}
			break;

				
		case COND_ELSE:
			if (cond_search(&post, COND_END)) {
				freeStack(flp, stack);
				return -1;
			}
			break;


		case COND_END:
			break;

		/* Normal one-argument function and operators */
		case ABS_VAL:
		case UNARY_NEG:
		case SQU_RT:
		case CUM:
		case EXP:
		case LOG_10:
		case LOG_E:
		case ACOS:
		case ASIN:
		case ATAN:
		case COS:
		case SIN:
		case TAN:
		case COSH:
		case SINH:
		case TANH:
		case CEIL:
		case FLOOR:
		case ISINF:
		case NINT:
		case AMAX:
		case AMIN:
		case IXMAX:
		case IXMIN:
		case IXZ:
		case IXNZ:
		case REL_NOT:
		case BIT_NOT:
		case AVERAGE:
		case STD_DEV:
		case FWHM:
		case SMOOTH:
		case DERIV:
		case ARRSUM:
		case FITPOLY:
		case FITMPOLY:
			if (isArray(ps)) {
				switch (op) {
				case ABS_VAL: for (i=0; i<arraySize; i++) {if (ps->a[i] < 0) ps->a[i] *= -1;} break;
				case UNARY_NEG: for (i=0; i<arraySize; i++) {ps->a[i] *= -1;} break;
				case SQU_RT:
					status = 0;
					for (i=0; i<arraySize; i++) {
						if (ps->a[i] < 0) {
							ps->a[i] = 0;
							status = -1;
						} else {
							ps->a[i] = sqrt(ps->a[i]);
						}
					}
					if (status)	printf("aCalcPerform: attempt to take sqrt of negative number\n");
					break;
				case CUM:
					for (i=1; i<arraySize; i++) {ps->a[i] += ps->a[i-1];}
					break;
				case EXP: for (i=0; i<arraySize; i++) {ps->a[i] = exp(ps->a[i]);} break;
				case LOG_10:
					status = 0;
					for (i=0; i<arraySize; i++) {
						if (ps->a[i] < 0) {
							ps->a[i] = 0;
							status = -1;
						} else {
							ps->a[i] = log10(ps->a[i]);
						}
					}
					if (status) printf("aCalcPerform: attempt to take log of negative number\n");
					break;
				case LOG_E:
					status = 0;
					for (i=0; i<arraySize; i++) {
						if (ps->a[i] < 0)  {
							ps->a[i] = 0;
							status = -1;
						} else {
							ps->a[i] = log(ps->a[i]);
						}
					}
					if (status) printf("aCalcPerform: attempt to take log of negative number\n");
					break;
				case ACOS: for (i=0; i<arraySize; i++) {ps->a[i] = acos(ps->a[i]);} break;
				case ASIN: for (i=0; i<arraySize; i++) {ps->a[i] = asin(ps->a[i]);} break;
				case ATAN: for (i=0; i<arraySize; i++) {ps->a[i] = atan(ps->a[i]);} break;
				case COS: for (i=0; i<arraySize; i++) {ps->a[i] = cos(ps->a[i]);} break;
				case SIN: for (i=0; i<arraySize; i++) {ps->a[i] = sin(ps->a[i]);} break;
				case TAN: for (i=0; i<arraySize; i++) {ps->a[i] = tan(ps->a[i]);} break;
				case COSH: for (i=0; i<arraySize; i++) {ps->a[i] = cosh(ps->a[i]);} break;
				case SINH: for (i=0; i<arraySize; i++) {ps->a[i] = sinh(ps->a[i]);} break;
				case TANH: for (i=0; i<arraySize; i++) {ps->a[i] = tanh(ps->a[i]);} break;
				case CEIL: for (i=0; i<arraySize; i++) {ps->a[i] = ceil(ps->a[i]);} break;
				case FLOOR: for (i=0; i<arraySize; i++) {ps->a[i] = floor(ps->a[i]);} break;
				case ISINF: for (i=0; i<arraySize; i++) {ps->a[i] = isinf(ps->a[i]);} break;
				case NINT: for (i=0; i<arraySize; i++) {
								ps->a[i] = (double)(long)(ps->a[i] >= 0 ? ps->a[i]+0.5 : ps->a[i]-0.5);
							}
							break;
				case AMAX:
					calcFirstLast(ps, &firstEl, &lastEl, arraySize);
					for (i=firstEl+1, d=ps->a[firstEl]; i<=lastEl; i++) {if (ps->a[i]>d) d = ps->a[i];}
					toDouble(ps);
					ps->d = d;
					break;

				case AMIN:
					/* for (i=1, d=ps->a[0]; i<arraySize; i++) {if (ps->a[i]<d) d = ps->a[i];} */
					calcFirstLast(ps, &firstEl, &lastEl, arraySize);
					for (i=firstEl+1, d=ps->a[firstEl]; i<=lastEl; i++)  {if (ps->a[i]<d) d = ps->a[i];}
					toDouble(ps);
					ps->d = d;
					break;

				case IXMAX:
					/* for (i=1, j=0, d=ps->a[0]; i<arraySize; i++) {if (ps->a[i]>d) {d = ps->a[i]; j = i;}} */
					calcFirstLast(ps, &firstEl, &lastEl, arraySize);
					if (aCalcPerformDebug>=10) printf("first=%d, last=%d", firstEl, lastEl);
					for (i=firstEl+1, j=firstEl, d=ps->a[firstEl]; i<=lastEl; i++) {
						if (ps->a[i]>d) {d = ps->a[i]; j = i;}
					}
					toDouble(ps);
					ps->d = j;
					break;

				case IXMIN:
					/* for (i=1, j=0, d=ps->a[0]; i<arraySize; i++) {if (ps->a[i]<d) {d = ps->a[i]; j = i;}} */
					calcFirstLast(ps, &firstEl, &lastEl, arraySize);
					for (i=firstEl+1, j=firstEl, d=ps->a[firstEl]; i<=lastEl; i++) {
						if (ps->a[i]<d) {d = ps->a[i]; j = i;}
					}
					toDouble(ps);
					ps->d = j;
					break;
#if 0
				/* integer index of first element whose value is zero */
				case IXZ:
					for (i=0, j=-1; i<arraySize; i++) {
						if (fabs(ps->a[i]) < SMALL) {
							j = i;
							break;
						}
					}
					toDouble(ps);
					ps->d = j;
					break;
#endif
				/* real index of first zero crossing */
				case IXZ:
					calcFirstLast(ps, &firstEl, &lastEl, arraySize);
					for (i=firstEl+1, j=-1, d=0.; i<=lastEl; i++) {
						if ((ps->a[i]>0) != (ps->a[firstEl]>0)) {
							j = i-1;
							d = fabs(ps->a[j])/fabs(ps->a[j]-ps->a[j+1]);
							break;
						}
					}
					toDouble(ps);
					ps->d = j+d;
					break;

				case IXNZ:
					calcFirstLast(ps, &firstEl, &lastEl, arraySize);
					for (i=firstEl, j=-1; i<=lastEl; i++) {
						if (fabs(ps->a[i]) > SMALL) {
							j = i;
							if (aCalcPerformDebug>1) printf("aCalcPerform:IXNZ at index %d\n", j);
							break;
						}
					}
					toDouble(ps);
					ps->d = j;
					break;

				case REL_NOT: for (i=0; i<arraySize; i++) {ps->a[i] = (ps->a[i] ? 0 : 1);} break;
				case BIT_NOT: for (i=0; i<arraySize; i++) {ps->a[i] = ~(int)(ps->a[i]);} break;

				case AVERAGE:
					calcFirstLast(ps, &firstEl, &lastEl, arraySize);
					for (i=firstEl+1, d=ps->a[firstEl]; i<=lastEl; i++) {d += ps->a[i];}
					toDouble(ps);
					ps->d = d/(1+lastEl-firstEl);
					break;

				case STD_DEV:
					calcFirstLast(ps, &firstEl, &lastEl, arraySize);
					for (i=firstEl+1, d=ps->a[firstEl]; i<=lastEl; i++) {d += ps->a[i];}
					d /= 1+lastEl-firstEl;
					for (i=firstEl, e=0.; i<=lastEl; i++) {e += (ps->a[i]-d)*(ps->a[i]-d);}
					toDouble(ps);
					if (lastEl-firstEl > 0)
						ps->d = sqrt(e/(lastEl-firstEl)); /* sum(err^2)/(n-1) */
					else
						ps->d = sqrt(e);
					break;

				case FWHM:
					calcFirstLast(ps, &firstEl, &lastEl, arraySize);
					/* find max (d), min (e) values, and index (j) of max value */
					d = ps->a[firstEl];
					e = ps->a[firstEl];
					for (i=firstEl+1, j=firstEl; i<=lastEl; i++) {
						if (ps->a[i] > d) {
							d = ps->a[i];
							j = i;
						}
						if (ps->a[i] < e) {
							e = ps->a[i];
						}
					}
					if (aCalcPerformDebug>5) {printf("max=%f, at %d; min=%f\n", d, j, e);}
					d = e + (d-e)/2;
					/* walk forwards from peak */
					for (i=j+1, found=0; i<=lastEl; i++) {
						if (ps->a[i] < d) {
							found = 1;
							e = (i-1) + (d - ps->a[i-1])/(ps->a[i] - ps->a[i-1]);
							if (aCalcPerformDebug>5) {printf("halfmax at index %f\n", e);}
							break;
						}
					}
					if (!found) e = lastEl;
					/* walk backwards from peak */
					for (i=j-1, found=0; i>=firstEl; i--) {
						if (ps->a[i] < d) {
							found = 1;
							d = i + (d - ps->a[i])/(ps->a[i+1] - ps->a[i]);
							if (aCalcPerformDebug>5) {printf("halfmax at index %f\n", d);}
							break;
						}
					}

					if (!found) d = 0;
					toDouble(ps);
					ps->d = e-d;
					break;
				case SMOOTH:
					calcFirstLast(ps, &firstEl, &lastEl, arraySize);
					d = ps->a[firstEl]; e = ps->a[firstEl+1]; f=ps->a[firstEl+2];
					for (i=firstEl+2; i<=lastEl-2; i++) {
						ps->a[i] = d/16 + e/4 + 3*f/8 + ps->a[i+1]/4 + ps->a[i+2]/16;
						d=e; e=f; f=ps->a[i+1];
					}
					break;
				case DERIV:
					calcFirstLast(ps, &firstEl, &lastEl, arraySize);
					ps1 = ps; /* y values */
					INC(ps);
					toArray(ps,0);
					ps2 = ps; /* x values */
					for (i=firstEl; i<=lastEl; i++) {ps2->a[i] = i;}
					INC(ps);
					toArray(ps,0); /* place for deriv */
					status = deriv(&(ps2->a[firstEl]), &(ps1->a[firstEl]), 1+lastEl-firstEl, &(ps->a[firstEl]));
					for (i=firstEl; i<=lastEl; i++) {ps1->a[i] = ps->a[i];}
					for (i=0; i<firstEl; i++) {ps1->a[i] = 0;}
					for (i=lastEl+1; i<arraySize; i++) {ps1->a[i] = 0;}
					DEC(ps); DEC(ps);
					break;
				case ARRSUM:
					calcFirstLast(ps, &firstEl, &lastEl, arraySize);
					for (i=firstEl, d=0.0; i<=lastEl; i++) {
						d += ps->a[i];
					}
					toDouble(ps);
					ps->d = d;
					break;
				case FITPOLY:
					calcFirstLast(ps, &firstEl, &lastEl, arraySize);
					ps1 = ps; /* y values */
					INC(ps);
					toArray(ps,0);
					ps2 = ps; /* x values */
					for (i=firstEl; i<=lastEl; i++) {ps2->a[i] = i;}
					INC(ps);
					toArray(ps,0); /* place for deriv */
					status = fitpoly(&(ps2->a[firstEl]), &(ps1->a[firstEl]), 1+lastEl-firstEl, &d, &e, &f, NULL);
					for (i=firstEl; i<=lastEl; i++) {
						ps1->a[i] = d + e*ps2->a[i] + f*(ps2->a[i])*(ps2->a[i]);
					}
					for (i=0; i<firstEl; i++) {ps1->a[i] = 0;}
					for (i=lastEl+1; i<arraySize; i++) {ps1->a[i] = 0;}
					DEC(ps); DEC(ps);
					break;

				case FITMPOLY:
					calcFirstLast(ps, &firstEl, &lastEl, arraySize);

					ps3 = ps; /* mask array */
					DEC(ps);
					ps1 = ps; /* y values */
					INC(ps); INC(ps); /* point to unused value-stack element */
					toArray(ps,0);
					ps2 = ps; /* x values */
					for (i=firstEl; i<=lastEl; i++) {ps2->a[i] = i;}
					INC(ps); /* point to unused value-stack element */
					toArray(ps,0); /* place for deriv */
					status = fitpoly(&(ps2->a[firstEl]), &(ps1->a[firstEl]), 1+lastEl-firstEl, &d, &e, &f, &(ps3->a[firstEl]));
					for (i=firstEl; i<=lastEl; i++) {
						ps1->a[i] = d + e*ps2->a[i] + f*(ps2->a[i])*(ps2->a[i]);
					}
					for (i=0; i<firstEl; i++) {ps1->a[i] = 0;}
					for (i=lastEl+1; i<arraySize; i++) {ps1->a[i] = 0;}
					DEC(ps); DEC(ps); DEC(ps);
					break;
				}
			} else { /* if (isArray(ps)) */
				switch (op) {
				case ABS_VAL: if (ps->d < 0) {ps->d *= -1;} break;
				case UNARY_NEG: ps->d *= -1; break;
				case SQU_RT:
					if (ps->d < 0) {
						ps->d = 0;
						printf("aCalcPerform: attempt to take sqrt of negative number\n");
					} else {
						ps->d = sqrt(ps->d);
					}
					break;
				case CUM:
					break;
				case EXP:
					ps->d = exp(ps->d);
					break;
				case LOG_10:
					if (ps->d < 0) {
						ps->d = 0;
						printf("aCalcPerform: attempt to take log of negative number\n");
					} else {
						ps->d = log10(ps->d);
					}
					break;
				case LOG_E:
					if (ps->d < 0) {
						ps->d = 0;
						printf("aCalcPerform: attempt to take log of negative number\n");
					} else {
						ps->d = log(ps->d);
					}
					break;
				case ACOS: {ps->d = acos(ps->d);} break;
				case ASIN: {ps->d = asin(ps->d);} break;
				case ATAN: {ps->d = atan(ps->d);} break;
				case COS: {ps->d = cos(ps->d);} break;
				case SIN: {ps->d = sin(ps->d);} break;
				case TAN: {ps->d = tan(ps->d);} break;
				case COSH: {ps->d = cosh(ps->d);} break;
				case SINH: {ps->d = sinh(ps->d);} break;
				case TANH: {ps->d = tanh(ps->d);} break;
				case CEIL: {ps->d = ceil(ps->d);} break;
				case FLOOR: {ps->d = floor(ps->d);} break;
				case ISINF:	{ps->d = isinf(ps->d);} break;
				case NINT: ps->d = (double)(long)(ps->d >= 0 ? ps->d+0.5 : ps->d-0.5); break;
				case AMAX: break;
				case AMIN: break;
				case IXMAX: ps->d = 0; break;
				case IXMIN: ps->d = 0; break;
				case IXZ: ps->d = fabs(ps->d)<SMALL?0:-1; break;
				case IXNZ: ps->d = fabs(ps->d)>SMALL?0:-1; break;
				case REL_NOT: ps->d = (ps->d ? 0 : 1); break;
				case BIT_NOT: ps->d = ~(int)(ps->d); break;
				case AVERAGE: break;
				case STD_DEV: ps->d = 0; break;
				case FWHM: ps->d = 0; break;
				case SMOOTH: break;
				case DERIV: ps->d = 0; break;
				case ARRSUM: break;
				case FITPOLY: ps->d = 0; break;
				case FITMPOLY: ps->d = 0; break;
				}
			}
			break;

/* begin VARARGS functions: Note that all VARARGS functions must be considered in cond_search(), below. */

		case FINITE:
			nargs = *post++;
			if (isDouble(ps)) {
				j = finite(ps->d);
			} else {
				for (i=0, j=1; i<arraySize; i++) {
					j = j && finite(ps->a[i]);
					if (aCalcPerformDebug>=10) printf("j=%d ", j);
				}
			}
			while (--nargs) {
				DEC(ps);
				if (isDouble(ps)) {
					j = j && finite(ps->d);
				} else {
					for (i=0; i<arraySize; i++) {
						j = j && finite(ps->a[i]);
					}
				}
			}
			toDouble(ps);
			ps->d = j;
			break;

		case ISNAN:
			nargs = *post++;
			if (isDouble(ps)) {
				j = isnan(ps->d);
			} else {
				for (i=0, j=0; i<arraySize; i++) {
					j = j || isnan(ps->a[i]);
				}
			}
			while (--nargs) {
				DEC(ps);
				if (isDouble(ps)) {
					j = j || isnan(ps->d);
				} else {
					for (i=0; i<arraySize; i++) {
						j = j || isnan(ps->a[i]);
					}
				}
			}
			toDouble(ps);
			ps->d = j;
			break;

		case MAX:
		case MIN:
			/* for now, don't use array extents for these functions */
			nargs = *post++;
			for (i=0, j=0; i<nargs; j |= isArray(ps-i), i++);
			if (j) {
				ps1 = ps - (nargs-1); /* coerce bottommost stack element to array */
				toArray(ps1,1);
				while (--nargs) {
					if (isArray(ps)) {
						if (op == MAX) {
							for (i=0; i<arraySize; i++) if (ps->a[i] > ps1->a[i]) ps1->a[i] = ps->a[i];
						} else {
							for (i=0; i<arraySize; i++) if (ps->a[i] < ps1->a[i]) ps1->a[i] = ps->a[i];
						}
					} else {
						if (op == MAX) {
							for (i=0; i<arraySize; i++) if (ps->d > ps1->a[i]) ps1->a[i] = ps->d;
						} else {
							for (i=0; i<arraySize; i++) if (ps->d < ps1->a[i]) ps1->a[i] = ps->d;
						}
					}
					DEC(ps);
				}
			} else {
				/* all args are double */
				while (--nargs) {
					d = ps->d;
					DEC(ps);
					if (op == MAX) {
						if (ps->d < d || isnan(d)) ps->d = d;
					} else {
						if (ps->d > d || isnan(d)) ps->d = d;
					}
				}
			}
			break;

		case FITQ:
			{
				int argc=-1, argb=-1, arga=-1;
	
				nargs = *post++;
				while (nargs>4) {DEC(ps); nargs--;}	/* discard extra arguments */
				switch (nargs) {
				case 4:
					argc = ps->sourceDouble;
					DEC(ps); nargs--;
				case 3:
					argb = ps->sourceDouble;
					DEC(ps); nargs--;
				case 2:
					arga = ps->sourceDouble;
					DEC(ps); nargs--;
					break;
				default:
					break;
				}

				toArray(ps, 1);
				calcFirstLast(ps, &firstEl, &lastEl, arraySize);
				ps1 = ps; /* y values */
				INC(ps);
				toArray(ps,0);
				ps2 = ps; /* x values */
				for (i=firstEl; i<=lastEl; i++) {ps2->a[i] = i;}
				status = fitpoly(&(ps2->a[firstEl]), &(ps1->a[firstEl]), 1+lastEl-firstEl, &d, &e, &f, NULL);
				for (i=firstEl; i<=lastEl; i++) {
					ps1->a[i] = d + e*ps2->a[i] + f*(ps2->a[i])*(ps2->a[i]);
				}
				for (i=0; i<firstEl; i++) {ps1->a[i] = 0;}
				for (i=lastEl+1; i<arraySize; i++) {ps1->a[i] = 0;}
				DEC(ps);

				/* if user specified valid args for c, b, a coefficients, store to them */
				if ((arga != -1) && (arga < num_dArgs)) p_dArg[arga] = d;
				if ((argb != -1) && (argb < num_dArgs)) p_dArg[argb] = e;
				if ((argc != -1) && (argc < num_dArgs)) p_dArg[argc] = f;

				break;
			}

		case FITMQ:
			{
				int argc=-1, argb=-1, arga=-1;
	
				nargs = *post++;
				while (nargs>5) {DEC(ps); nargs--;}	/* discard extra arguments */
				switch (nargs) {
				case 5:
					argc = ps->sourceDouble;
					DEC(ps); nargs--;
				case 4:
					argb = ps->sourceDouble;
					DEC(ps); nargs--;
				case 3:
					arga = ps->sourceDouble;
					DEC(ps); nargs--;
					break;
				default:
					break;
				}
				if (nargs < 2) {
					printf("aCalcPerform: FITMQ: need at least two arguments\n");
					return(-1);
				}
				toArray(ps, 1);
				calcFirstLast(ps, &firstEl, &lastEl, arraySize);
				ps3 = ps; /* mask array */
				DEC(ps);
				ps1 = ps; /* y values */
				INC(ps); INC(ps);
				toArray(ps,0);
				ps2 = ps; /* x values */
				for (i=firstEl; i<=lastEl; i++) {ps2->a[i] = i;}
				status = fitpoly(&(ps2->a[firstEl]), &(ps1->a[firstEl]), 1+lastEl-firstEl, &d, &e, &f, &(ps3->a[firstEl]));
				for (i=firstEl; i<=lastEl; i++) {
					ps1->a[i] = d + e*ps2->a[i] + f*(ps2->a[i])*(ps2->a[i]);
				}
				for (i=0; i<firstEl; i++) {ps1->a[i] = 0;}
				for (i=lastEl+1; i<arraySize; i++) {ps1->a[i] = 0;}
				DEC(ps);  DEC(ps);

				/* if user specified valid args for c, b, a coefficients, store to them */
				if ((arga != -1) && (arga < num_dArgs)) p_dArg[arga] = d;
				if ((argb != -1) && (argb < num_dArgs)) p_dArg[argb] = e;
				if ((argc != -1) && (argc < num_dArgs)) p_dArg[argc] = f;

				break;
			}

/* end varargs functions */

		case ARANDOM:
			INC(ps);
			toArray(ps,0);
			for (i=0; i<arraySize; i++) ps->a[i] = local_random();
			break;

		case RANDOM:
			INC(ps);
			ps->d = local_random();
			ps->a = NULL;
			break;

		case NORMAL_RNDM:				
			INC(ps);
			ps->d = sqrt(-2*log(local_random())) * cos(2*PI*local_random());
			ps->a = NULL;
			break;

		case POWER:
			ps1 = ps;
			DEC(ps);
			toDouble(ps1);
			if (isArray(ps)) {
				for (i=0; i<arraySize; i++) {
					ps->a[i] = pow(ps->a[i], ps1->d);
				}
			} else {
				ps->d = pow(ps->d, ps1->d);
			}
			break;

		/* Normal two-argument functions and operators */
		case GR_OR_EQ:
		case GR_THAN:
		case LESS_OR_EQ:
		case LESS_THAN:
		case NOT_EQ:
		case EQUAL:
		case MAX_VAL:
		case MIN_VAL:
		case REL_OR:
		case REL_AND:
		case BIT_OR:
		case BIT_AND:
		case BIT_EXCL_OR:
 		case ATAN2:
 		case CAT:
			ps1 = ps;
			DEC(ps);
			if (isArray(ps) || isArray(ps1)) {
				toArray(ps,1);
				calcFirstLast(ps, &firstEl, &lastEl, arraySize);
				if (aCalcPerformDebug>=10) {printf("two-arg: firstEl=%d, lastEl=%d\n", firstEl, lastEl);}

				if (isArray(ps1)) {
					calcFirstLast(ps1, &firstEl1, &lastEl1, arraySize);
					switch (op) {
					case GR_OR_EQ:		for (i=0; i<arraySize; i++) ps->a[i] = ps->a[i] >= ps1->a[i]; break;
					case GR_THAN:		for (i=0; i<arraySize; i++) ps->a[i] = ps->a[i] > ps1->a[i]; break;
					case LESS_OR_EQ:	for (i=0; i<arraySize; i++) ps->a[i] = ps->a[i] <= ps1->a[i]; break;
					case LESS_THAN:		for (i=0; i<arraySize; i++) ps->a[i] = ps->a[i] < ps1->a[i]; break;
					case NOT_EQ:		for (i=0; i<arraySize; i++) ps->a[i] = ps->a[i] != ps1->a[i]; break;
					case EQUAL:			for (i=0; i<arraySize; i++) ps->a[i] = ps->a[i] == ps1->a[i]; break;
					case MAX_VAL:		for (i=0; i<arraySize; i++) if (ps->a[i] < ps1->a[i]) ps->a[i] = ps1->a[i]; break;
					case MIN_VAL:		for (i=0; i<arraySize; i++) if (ps->a[i] > ps1->a[i]) ps->a[i] = ps1->a[i]; break;
					case REL_OR:		for (i=0; i<arraySize; i++) ps->a[i] = ps->a[i] || ps1->a[i]; break;
					case REL_AND:		for (i=0; i<arraySize; i++) ps->a[i] = ps->a[i] && ps1->a[i]; break;
					case BIT_OR:		for (i=0; i<arraySize; i++) ps->a[i] = (int)ps->a[i] | (int)ps1->a[i]; break;
					case BIT_AND:		for (i=0; i<arraySize; i++) ps->a[i] = (int)ps->a[i] & (int)ps1->a[i]; break;
					case BIT_EXCL_OR:	for (i=0; i<arraySize; i++) ps->a[i] = (int)ps->a[i] ^ (int)ps1->a[i]; break;
			 		case ATAN2:			for (i=0; i<arraySize; i++) ps->a[i] = atan2(ps1->a[i], ps->a[i]); break;
			 		case CAT:
						if (aCalcPerformDebug>=10) {
							printf("CAT(array, array); array[0]=%f, double=%f\n", ps->a[0], ps1->a[0]);
						}
						for (i=lastEl+1, j=firstEl1; i<arraySize && j <=lastEl1; i++, j++)
							ps->a[i] = ps1->a[j];
						ps->numEl = i - ps->firstEl;
						break;
					}
				} else {
					switch (op) {
					case GR_OR_EQ:		for (i=0; i<arraySize; i++) ps->a[i] = ps->a[i] >= ps1->d; break;
					case GR_THAN:		for (i=0; i<arraySize; i++) ps->a[i] = ps->a[i] > ps1->d; break;
					case LESS_OR_EQ:	for (i=0; i<arraySize; i++) ps->a[i] = ps->a[i] <= ps1->d; break;
					case LESS_THAN:		for (i=0; i<arraySize; i++) ps->a[i] = ps->a[i] < ps1->d; break;
					case NOT_EQ:		for (i=0; i<arraySize; i++) ps->a[i] = ps->a[i] != ps1->d; break;
					case EQUAL:			for (i=0; i<arraySize; i++) ps->a[i] = ps->a[i] == ps1->d; break;
					case MAX_VAL:		for (i=0; i<arraySize; i++) if (ps->a[i] < ps1->d) ps->a[i] = ps1->d; break;
					case MIN_VAL:		for (i=0; i<arraySize; i++) if (ps->a[i] > ps1->d) ps->a[i] = ps1->d; break;
					case REL_OR:		for (i=0; i<arraySize; i++) ps->a[i] = ps->a[i] || ps1->d; break;
					case REL_AND:		for (i=0; i<arraySize; i++) ps->a[i] = ps->a[i] && ps1->d; break;
					case BIT_OR:		for (i=0; i<arraySize; i++) ps->a[i] = (int)ps->a[i] | (int)ps1->d; break;
					case BIT_AND:		for (i=0; i<arraySize; i++) ps->a[i] = (int)ps->a[i] & (int)ps1->d; break;
					case BIT_EXCL_OR:	for (i=0; i<arraySize; i++) ps->a[i] = (int)ps->a[i] ^ (int)ps1->d; break;
			 		case ATAN2:			for (i=0; i<arraySize; i++) ps->a[i] = atan2(ps1->d, ps->a[i]); break;
			 		case CAT:
						if (aCalcPerformDebug>=10) {
							printf("CAT(array, double); array[0]=%f, double=%f\n", ps->a[0], ps1->d);
						}
						if (lastEl+1 < arraySize) {ps->a[lastEl+1] = ps1->d; ps->numEl += 1;}
						if (aCalcPerformDebug>=10) {
							printf("CAT; array[0]=%f, array[1]=%f\n", ps->a[0], ps->a[1]);
						}
						break;
					}
				}
			} else {
				switch (op) {
				case GR_OR_EQ:		ps->d = ps->d >= ps1->d; break;
				case GR_THAN:		ps->d = ps->d > ps1->d; break;
				case LESS_OR_EQ:	ps->d = ps->d <= ps1->d; break;
				case LESS_THAN:		ps->d = ps->d < ps1->d; break;
				case NOT_EQ:		ps->d = ps->d != ps1->d; break;
				case EQUAL:			ps->d = ps->d == ps1->d; break;
				case MAX_VAL:		if (ps->d < ps1->d) ps->d = ps1->d; break;
				case MIN_VAL:		if (ps->d > ps1->d) ps->d = ps1->d; break;
				case REL_OR:		ps->d = ps->d || ps1->d; break;
				case REL_AND:		ps->d = ps->d && ps1->d; break;
				case BIT_OR:		ps->d = (int)ps->d | (int)ps1->d; break;
				case BIT_AND:		ps->d = (int)ps->d & (int)ps1->d; break;
				case BIT_EXCL_OR:	ps->d = (int)ps->d ^ (int)ps1->d; break;
				case ATAN2:			ps->d = atan2(ps1->d, ps->d); break;
				case CAT:			break;
				}
			}
			break;

		case RIGHT_SHIFT:
		case LEFT_SHIFT:
			ps1 = ps;
			DEC(ps);
			toDouble(ps1);
			if (isDouble(ps)) {
				/* scalar variable: bit shift by integer amount */
				if (op == RIGHT_SHIFT) {
					ps->d = (int)(ps->d) >> (int)(ps1->d);
				} else {
					ps->d = (int)(ps->d) << (int)(ps1->d);
				}
			} else {
				/* array variable: shift array elements */
				e = ps1->d;	/* num channels to shift */
				if (op == LEFT_SHIFT)  e = -e;
				j = myNINT(e);
				if (j > 0) {
					for (i=arraySize-1; i>=j; i--) ps->a[i] = ps->a[i-j];
					for ( ; i>=0; i--) ps->a[i] = 0.;
				} else if (j < 0) {
					for (i=0; i<arraySize+j; i++) ps->a[i] = ps->a[i-j];
					for ( ; i<arraySize; i++) ps->a[i] = 0.;
				}
				d = fabs(e - j);
				/* printf("aCalcPerform:shift: d=%f\n", d);*/
				if (d > SMALL) {
					/* shift by delta-index of less than .5 */
					if (e < j) {
						for (i=0; i<arraySize-1; i++) {
							ps->a[i] += d * (ps->a[i+1] - ps->a[i]);
						}
						/* extrapolate for last data point */
						ps->a[i] += d * (ps->a[i] - ps->a[i-1]);
					} else {
						for (i=arraySize-1; i>0; i--) {
							ps->a[i] += d * (ps->a[i-1] - ps->a[i]);
						}
						/* extrapolate for last data point */
						ps->a[i] += d * (ps->a[i] - ps->a[i+1]);
					}
				}
			}
			break;

		case A_FETCH:
			if (isDouble(ps)) {
				d = ps->d;
			} else {
				d = ps->a[0];
				ps->a = NULL;
			}
			i = myNINT(d);
			if (i >= num_dArgs || i < 0) {
				printf("aCalcPerform: fetch index, %d, out of range.\n", i);
				ps->d = 0;
				ps->sourceDouble = -1;
			} else {
				ps->d = p_dArg[i];
				ps->sourceDouble = i;
			}
			break;

		case A_AFETCH:
			toDouble(ps);
			d = ps->d;
			toArray(ps,0);
			ps->a[0] = '\0';
			j = myNINT(d);
			if (j >= num_aArgs || j < 0) {
				printf("aCalcPerform: fetch index, %d, out of range.\n", j);
			} else {
				/* Careful.  It's possible the record has not allocated the array */
				if (pp_aArg[j]) {
					for (i=0; i<arraySize; i++) ps->a[i] = pp_aArg[j][i];
				} else {
					for (i=0; i<arraySize; i++) ps->a[i] = 0.0;
				}
			}
			break;

		case LITERAL_DOUBLE:
			INC(ps);
			memcpy((void *)&(ps->d),post,sizeof(double));
			ps->a = NULL;
			post += sizeof(double);
			break;

		case LITERAL_INT:
			INC(ps);
			memcpy((void *)&i,post,sizeof(int));
			ps->d = (double)i;
			ps->a = NULL;
			post += sizeof(int);
			break;

		case TO_DOUBLE:
			toDouble(ps);
			break;

		case TO_ARRAY:
			toArray(ps,1);
			break;

		case SUBRANGE:
		case SUBRANGE_IP:
			ps2 = ps;
			DEC(ps);
			ps1 = ps;
			DEC(ps);
			toArray(ps,1);
			toDouble(ps1);
			i = (int)ps1->d;
			if (i < 0) i += arraySize;
			toDouble(ps2);
			j = (int)ps2->d;
			if (j < 0) j += arraySize;
			i = myMAX(myMIN(i,arraySize),0);
			j = myMIN(j,arraySize);
			if (aCalcPerformDebug > 20) printf("\tSUBRANGE*: ix1=%d, ix2=%d\n", i, j);
			if (op == SUBRANGE) {
				ps->firstEl = 0;
				ps->numEl = 1+j-i;
				for (k=0; i<=j; k++, i++) ps->a[k] = ps->a[i];
				for ( ; k<arraySize; k++) ps->a[k] = 0.;
				if (aCalcPerformDebug > 20) printf("\tSUBRANGE: firstEl=%d, numEl=%d\n", ps->firstEl, ps->numEl);
			} else {
				ps->firstEl = 0;
				ps->numEl = j+1;
				for (k=0; k<i; k++) ps->a[k] = 0.;
				for (k=j+1; k<arraySize; k++) ps->a[k] = 0.;
				if (aCalcPerformDebug > 20) printf("\tSUBRANGE_IP: firstEl=%d, numEl=%d\n", ps->firstEl, ps->numEl);
			}
			break;

 		case UNTIL:
			if (aCalcPerformDebug > 20) printf("\tUNTIL:ps->d=%f\n", ps->d);
			if (aCalcPerformDebug > 20) printf("\tpost-1=%p\n", post-1);
			for (i=0; i<MAX_UNTIL_OP; i++) {
				/* find ourselves in post, remembering that post was incremented at loop top */
				if (aCalcPerformDebug > 20) printf("\tuntil_scratch[i].until_loc=%p\n", until_scratch[i].until_loc);
				if (until_scratch[i].until_loc == post-1) {
					until_scratch[i].ps = ps;
					break;
				}
			}
			if (i==MAX_UNTIL_OP) {
				printf("aCalcPerform: UNTIL not found\n");
				freeStack(flp, stack);
				return(-1);
			}
			break;

		case UNTIL_END:
			if (aCalcPerformDebug > 20) printf("\tUNTIL_END:ps->d=%f\n", ps->d);
			if (++loopsDone > aCalcLoopMax)
				break;
			if (ps->d==0) {
				/* reset postfix to matching UNTIL code, stack to its loc at that time */
				--post;
				for (i=0; i<MAX_UNTIL_OP; i++) {
					if (until_scratch[i].until_end_loc == post) {
						ps = until_scratch[i].ps;
						post = until_scratch[i].until_loc;
						if (aCalcPerformDebug > 20) printf("--loop--\n");
						break;
					}
				}
				if (i==MAX_UNTIL_OP) {
					printf("aCalcPerform: UNTIL not found\n");
					freeStack(flp, stack);
					return(-1);
				}
				break;
			}
			break;



		default:
			break;
		}

	}

	if (aCalcPerformDebug>=20) printf("aCalcPerform:done with expression, status=%d\n", status);
	if (status) {
		freeStack(flp, stack);
		return(status);
	}

	/* if everything is peachy,the stack should end at its first position */
	if (ps != top) {
#if DEBUG
		if (aCalcPerformDebug>=1) {
			printf("aCalcPerform: stack error,ps=%p,top=%p\n", (void *)ps, (void *)top);
			printf("aCalcPerform: stack error (ps-top=%d)\n", (int)(ps-top));
			printf("aCalcPerform: ps->d=%f\n", ps->d);
		}
#endif
		freeStack(flp, stack);
		return(-1);
	}
	
	if (isDouble(ps)) {
		if (aCalcPerformDebug>=20) printf("aCalcPerform:double result=%f\n", ps->d);
		if (p_dresult) *p_dresult = ps->d;
		if (p_aresult) {
			toArray(ps,1);
			for (i=0; i<arraySize; i++) p_aresult[i] = ps->a[i];
		}
	} else {
		if (aCalcPerformDebug>=20) printf("aCalcPerform:array result a[0]=%f, a[1]=%f\n",
			ps->a[0], ps->a[1]);

		if (p_aresult) {
			for (i=0; i<arraySize; i++) p_aresult[i] = ps->a[i];
		}
		if (p_dresult) {
			to_double(ps);
			*p_dresult = ps->d;
		}
	}

	if (aCalcPerformDebug) printf("aCalcPerform:stack lo=%d, hi=%d\n",
		aCalcStackLW, aCalcStackHW);

	freeStack(flp, stack);
	return(((isnan(*p_dresult)||isinf(*p_dresult)) ? -1 : 0));
}


/*
 * RAND
 *
 * generates a random number between 0 and 1 using the
 * seed = (multy * seed) + addy         Random Number Generator by Knuth
 *                                              SemiNumerical Algorithms
 *                                              Chapter 1
 * randy = seed / 65535.0          To normalize the number between 0 - 1
 */
static unsigned short seed = 0xa3bf;
static unsigned short multy = 191 * 8 + 5;  /* 191 % 8 == 5 */
static unsigned short addy = 0x3141;
static double local_random()
{
        double  randy;

        /* random number */
        seed = (seed * multy) + addy;
        randy = (float) seed / 65535.0;

        /* between 0 - 1 */
        return(randy);
}

/* Search the instruction stream for a matching operator, skipping any
 * other conditional instructions found, and leave *ppinst pointing to
 * the next instruction to be executed.
 */
static int cond_search(const unsigned char **ppinst, int match)
{
	const unsigned char *pinst = *ppinst;
	int count = 1;
	int op;

	if (aCalcPerformDebug>5) {
		printf("cond_search:entry:\n");
		aCalcExprDump(pinst);
		printf("\t-----\n");
	}
	while ((op = *pinst++) != END_EXPRESSION) {
		if (op == match && --count == 0) {
			if (aCalcPerformDebug>5) {
				printf("cond_search:exit:\n");
				aCalcExprDump(pinst);
				printf("\t-----\n");
			}
			*ppinst = pinst;
			return 0;
		}
		switch (op) {
		case LITERAL_DOUBLE:
			pinst += sizeof(double);
			break;
		case LITERAL_INT:
			pinst += sizeof(int);
			break;
		case MIN:
		case MAX:
		case FINITE:
		case ISNAN:
		case FITQ:
		case FITMQ:
			/* variable argument function.  Skip numArgs */
			pinst++;
			break;
		case COND_IF:
			count++;
			break;
		}
	}
	return 1;
}
