/* aCalcPerform.c
 *
 *	Author: Tim Mooney - derived from code written by Julie Sander and Bob Dalesio
 *	Date:	03-21-06
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 * Modification Log:
 * -----------------
 * 03-21-06 tmm Derived from sCalcPerform
 *
 */

/* This module contains the code for processing the arithmetic
 * expressions defined in calculation records. sCalcPostfix must be called
 * to convert a valid infix expression to postfix. aCalcPerform
 * calculates the postfix expression.
 *
 * Subroutines
 *
 *	Public
 *
 * aCalcPerform		perform the calculation
 *  args
 *  	
 *		double	*p_dArg			address of arguments
 *		int		num_dArgs		number of arguments in p_dArgs array
 *		double	**pp_aArg		address of array arguments
 *		int		num_aArgs		number of array arguments
 *		long	arraySize		length of arrays
 *		double	*p_dresult		address of double result
 *		double	*p_aresult		address of array result
 *		char	*post			address of postfix buffer
 *
 *  returns
 *		0   fetched successfully
 *		-1  fetch failed
 *
 * Private routine for aCalcPerform
 * local_random  random number generator
 *  returns
 *    double value between 0.00 and 1.00
 */

#ifdef vxWorks
#include <vxWorks.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "dbDefs.h"
#include "cvtFast.h"
#include "epicsString.h"
#define epicsExportSharedSymbols
#include "aCalcPostfix.h"
#include "aCalcPostfixPvt.h"
#include <epicsExport.h>
#include <freeList.h>

/* Note value much larger than this breaks MEDM's plot */
#define	myMAXFLOAT	((float)1e+35)

static double local_random();

#define myNINT(a) ((int)((a) >= 0 ? (a)+0.5 : (a)-0.5))
#ifndef PI
#define PI 3.141592654
#endif
#define MAX(a,b) (a)>(b)?(a):(b)
#define MIN(a,b) (a)<(b)?(a):(b)
#define SMALL 1.e-9

extern int deriv(double *x, double *y, int n, double *d);
extern int nderiv(double *x, double *y, int n, double *d, int m, double *work);
int fitpoly(double *x, double *y, int n,
	double *a0, double *a1, double *a2, double *mask);

#define DEBUG 1
volatile int aCalcPerformDebug = 0;
epicsExportAddress(int, aCalcPerformDebug);

#if DEBUG
int aCalcStackHW = 0;	/* high-water mark */
int aCalcStackLW = 0;	/* low-water mark */
#define INC(ps) {if ((int)(++(ps)-top) > aCalcStackHW) aCalcStackHW = (int)((ps)-top); if ((ps-top)>STACKSIZE) {printf("aCalcPerform:underflow\n"); stackInUse=0;return(-1);}}
#define DEC(ps) {if ((int)(--(ps)-top) < aCalcStackLW) aCalcStackLW = (int)((ps)-top); if ((ps-top)<-1) {printf("aCalcPerform:underflow\n"); stackInUse=0;return(-1);}}
#define checkDoubleElement(pd,op) {if (isnan(*(pd))) printf("aCalcPerform: unexpected NaN in op %d\n", (op));}
#define checkStackElement(ps,op) {if (((ps)->a == NULL) && isnan((ps)->d)) printf("aCalcPerform: unexpected NaN in op %d\n", (op));}
#else
#define INC(ps) ++ps
#define DEC(ps) ps--
#define checkDoubleElement(pd,op)
#define checkStackElement(ps,op)
#endif

#define isDouble(ps) ((ps)->a==NULL)
#define isArray(ps) ((ps)->a)

/* convert stack element of unknown type to double */
#define toDouble(ps) {if (isArray(ps)) to_double(ps);}

/* convert array-valued stack element to double */
#define to_double(ps) {(ps)->d = (ps)->a[0]; (ps)->a = NULL;}

/* convert stack element of unknown type to array */
#define toArray(ps) {if (isDouble(ps)) to_array(ps);}

/* convert double-valued stack element to array */
#define to_array(ps) {										\
	int ii;													\
	(ps)->a = &((ps)->array[0]);						\
	if (isnan((ps)->d))										\
		for(ii=0; ii<arraySize; ii++) (ps)->a[ii]=0.;		\
	else													\
		for(ii=0; ii<arraySize; ii++) (ps)->a[ii]=ps->d;	\
}

volatile int aCalcArraySize = 1000;
epicsExportAddress(int, aCalcArraySize);
struct stackElement {
	double d;
	double *a;
	double *array;
};
/* static struct stackElement stack[STACKSIZE];*/
static struct stackElement *stack = 0;

static int stackInUse=0;

long epicsShareAPI 
	aCalcPerform(double *p_dArg, int num_dArgs, double **pp_aArg,
		int num_aArgs, long arraySize, double *p_dresult, double *p_aresult,
		char *post)

{
	struct stackElement *top;
	struct stackElement *ps, *ps1, *ps2, *ps3;
	char				*s, currSymbol;
	int					i, j, k, found, status;
	double				d, e, f;
	short 				got_if;

	if (aCalcPerformDebug>=10) {
		printf("aCalcPerform:array-arg addresses: %p %p...\n", (void *)pp_aArg[0], (void *)pp_aArg[1]);
	}

	if (stack == NULL) {
		stack = malloc(STACKSIZE * sizeof(struct stackElement));
		if (stack == NULL) {
			printf("aCalcPerform: Can't allocate stack.\n");
			return(-1);
		}
		/* If aCalcArraySize wasn't specified, use arraySize from first call. */
		if (aCalcArraySize < arraySize) aCalcArraySize = arraySize;
		for (i=0; i<STACKSIZE; i++) {
			stack[i].array = (double *)malloc(aCalcArraySize * sizeof(double));
			if (stack[i].array == NULL) {
				printf("aCalcPerform: Can't allocate array.\n");
				if (i>0) {
					for (i--;i>=0; i--) free(stack[i].array);
					free(stack);
				}
				return(-1);
			}
		}
#if 0
		printf("aCalcPerform: stack=%p\n", stack);
		printf("aCalcPerform: &(stack[0])=%p\n", &(stack[0]));
		printf("aCalcPerform: &(stack[0].d)=%p\n", &(stack[0].d));
		printf("aCalcPerform: &(stack[0].a)=%p\n", &(stack[0].a));
		printf("aCalcPerform: &(stack[0].array)=%p\n", &(stack[0].array));
		printf("aCalcPerform: &(stack[0].array[0])=%p\n", &(stack[0].array[0]));

		printf("aCalcPerform: &(stack[1])=%p\n", &(stack[1]));
		printf("aCalcPerform: &(stack[1].d)=%p\n", &(stack[1].d));
		printf("aCalcPerform: &(stack[1].a)=%p\n", &(stack[1].a));
		printf("aCalcPerform: &(stack[1].array)=%p\n", &(stack[1].array));
		printf("aCalcPerform: &(stack[1].array[1])=%p\n", &(stack[1].array[1]));
#endif
	}

	if (stackInUse) {
		printf("aCalcPerform: stack in use.  Nothing done\n");
		return(-1);
	}
	stackInUse = 1;

	for (i=0; i<STACKSIZE; i++) {
		stack[i].d = 0.;
		stack[i].a = NULL;
		for (j=0; j<aCalcArraySize; j++) stack[i].array[j] = 0.;
	}
	if (arraySize > aCalcArraySize) {
		printf("aCalcPerform: I've only allocated for %d-element arrays\n", aCalcArraySize);
		stackInUse = 0;
		return(-1);
	}

#if DEBUG
	if (aCalcPerformDebug>=10) {
		int	more;
		printf("aCalcPerform: postfix:");
		s = post;
		for (more=1; more;) {
			if (*s >= FETCH_A && *s <= FETCH_L) {
				printf("%c ", 'a' + (*s-FETCH_A));
			} else {
				printf("%2d ", *s);
			}
			switch (*s) {
			case END_STACK:
				more = 0;
				break;
			case LITERAL:
				printf("(0x");
				for (i=0, s++; i<8; i++, s++)
					printf("%2x ", (unsigned int)(unsigned char)*s);
				printf(") ");
				break;
			case FETCH:
				s++; /* point past code */
				printf("@%d ", *s++);
				break;
			case AFETCH:
				s++; /* point past code */
				printf("$%d ", *s++);
				break;
			default:
				if (*s == BAD_EXPRESSION) more=0;
				s++;
				break;
			}
		}

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

	if (*post == BAD_EXPRESSION) {
		if (aCalcPerformDebug) printf("aCalcPerform: bad expression\n");
		stackInUse = 0;
		return(-1);
	}
	post++; /* skip past indicator */

	top = ps = &stack[1];
	ps--;  /* Expression handler assumes ps is pointing to a filled element */
	ps->d = 1.23456; ps->a = NULL;	/* telltale */

	status = 0;
	while ((*post != END_STACK) && (status == 0)) {

		currSymbol = *post;
		if (aCalcPerformDebug>=20) printf("aCalcPerform: currSymbol=%d\n", currSymbol);

		switch (currSymbol) {

		case FETCH:
			INC(ps);
			++post;
			ps->a = NULL;
			ps->d = (*post < num_dArgs) ? p_dArg[(int)*post] : 0;
			break;

		case FETCH_A:
			INC(ps); ps->a = NULL; ps->d = p_dArg[0];
			if (aCalcPerformDebug>=20) printf("aCalcPerform:arg=%f\n", ps->d);

		break;
		case FETCH_B: INC(ps); ps->a = NULL; ps->d = p_dArg[1]; break;
		case FETCH_C: INC(ps); ps->a = NULL; ps->d = p_dArg[2]; break;
		case FETCH_D: INC(ps); ps->a = NULL; ps->d = p_dArg[3]; break;
		case FETCH_E: INC(ps); ps->a = NULL; ps->d = p_dArg[4]; break;
		case FETCH_F: INC(ps); ps->a = NULL; ps->d = p_dArg[5]; break;
		case FETCH_G: INC(ps); ps->a = NULL; ps->d = p_dArg[6]; break;
		case FETCH_H: INC(ps); ps->a = NULL; ps->d = p_dArg[7]; break;
		case FETCH_I: INC(ps); ps->a = NULL; ps->d = p_dArg[8]; break;
		case FETCH_J: INC(ps); ps->a = NULL; ps->d = p_dArg[9]; break;
		case FETCH_K: INC(ps); ps->a = NULL; ps->d = p_dArg[10]; break;
		case FETCH_L: INC(ps); ps->a = NULL; ps->d = p_dArg[11]; break;

		case AFETCH:	/* fetch from array variable */
			INC(ps);
			++post;
			ps->a = &(ps->array[0]);
			ps->a[0] = 0.;
			if (*post < num_aArgs) {
				if (pp_aArg[(int)*post]) {
					for (i=0; i<arraySize; i++) ps->a[i] = pp_aArg[(int)*post][i];
				} else {
					for (i=0; i<arraySize; i++) ps->a[i] = 0.0;
				}
				if (aCalcPerformDebug>=20)
					printf("aCalcPerform:fetch array %d = [%f %f...]\n",
						(int)*post, ps->a[0], ps->a[1]);
			}
			break;

		case STORE:
			/* not implemented */
			stackInUse = 0;
			return(-1);

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
			ps->a = &(ps->array[0]);
			for (i=0; i<arraySize; i++) {
				ps->a[i] = i;
			}
			break;

		case NSMOOTH:
			checkStackElement(ps, *post);
			j = ps->d; /* get npts (ignore it for now, because NSMOOTH is not implemented yet) */
			DEC(ps);
			checkStackElement(ps, *post);
			for(k=0; k<j; k++) {
				d = ps->a[0]; e = ps->a[1]; f=ps->a[2];
				for (i=2; i<arraySize-2; i++) {
					ps->a[i] = d/16 + e/4 + 3*f/8 + ps->a[i+1]/4 + ps->a[i+2]/16;
					d=e; e=f; f=ps->a[i+1];
				}
			}
			break;

		case NDERIV:
			checkStackElement(ps, *post);
			toDouble(ps);
			j = MIN((arraySize-1)/2, ps->d);  /* points on either side of value for fit */
			DEC(ps);
			checkStackElement(ps, *post);
			toArray(ps);
			ps1 = ps; /* y array */
			INC(ps); ps->a = &(ps->array[0]);
			ps2 = ps; /* place in which to make an x array */
			for (i=0; i<arraySize; i++) {ps2->a[i] = i;}
			INC(ps); ps->a = &(ps->array[0]); /* place in which to calc derivative */
			ps3 = ps;
			INC(ps); ps->a = &(ps->array[0]); /* workspace for nderiv */
			status = nderiv(ps2->a, ps1->a, arraySize, ps3->a, j, ps->a);
			for (i=0; i<arraySize; i++) {ps1->a[i] = ps3->a[i];}
			DEC(ps); DEC(ps); DEC(ps);
			break;

		/* normal two-argument functions/operators (either arg can be array or scalar) */
		case ADD:
		case SUB:
		case MULT:
		case DIV:
		case MODULO:
		case MAXFUNC:
		case MINFUNC:
		
			checkStackElement(ps, *post);
			ps1 = ps;
			DEC(ps);
			checkStackElement(ps, *post);
			if (isArray(ps) || isArray(ps1)) {
				toArray(ps);
				toArray(ps1);
				switch (currSymbol) {
				case ADD: for (i=0; i<arraySize; i++) {ps->a[i] += ps1->a[i];} break;
				case SUB: for (i=0; i<arraySize; i++) {ps->a[i] -= ps1->a[i];} break;
				case MULT: for (i=0; i<arraySize; i++) {ps->a[i] *= ps1->a[i];} break;
				case DIV:
					for (i=0; i<arraySize; i++) {
						if (ps1->a[i]==0) {
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
				case MAXFUNC: for (i=0; i<arraySize; i++) {if (ps1->a[i] > ps->a[i]) {ps->a[i] = ps1->a[i];}} break;
				case MINFUNC: for (i=0; i<arraySize; i++) {if (ps1->a[i] < ps->a[i]) {ps->a[i] = ps1->a[i];}} break;
				}
				if (aCalcPerformDebug>=20) {
					printf("aCalcPerform:binary array op result = [\n");
					for (i=0; i<arraySize; i++) printf("%f ", ps->a[i]);
					printf("]\n");
				}
			} else {
				switch (currSymbol) {
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
				case MAXFUNC: if (ps1->d > ps->d) ps->d = ps1->d; break;
				case MINFUNC: if (ps1->d < ps->d) ps->d = ps1->d; break;
				}
			}
			break;

		case COND_IF:
			/* if false condition then skip true expression */
			checkStackElement(ps, *post);
			toDouble(ps);
			if (aCalcPerformDebug>=20) {printf("aCalcPerform:cond_if: ps->d=%f, ps-top=%ld\n", ps->d, (long)(ps-top));}
			if (ps->d == 0.0) {
				/* skip to matching COND_ELSE */
				for (got_if=1; got_if>0 && post[1] != END_STACK; ++post) {
					if (aCalcPerformDebug>=20) {printf("aCalcPerform:cond_if:*post=%d\n", *post);}
					switch (post[1]) {
					case LITERAL:	post+=8; break;
					case COND_IF:	got_if++; break;
					case COND_ELSE: got_if--; break;
					case FETCH: case AFETCH: post++; break;
					}
				}
				if (got_if) {
#if DEBUG
					if (aCalcPerformDebug)
						printf("aCalcPerform: '?' without matching ':'\n");
#endif
					stackInUse=0;
					return(-1);
				}
				if (aCalcPerformDebug>=20) {printf("aCalcPerform:skipped to else: *post=%d\n", *post);}

			}
			/* remove condition from stack top */
			DEC(ps);
			break;
				
		case COND_ELSE:
			/* result, true condition is on stack so skip false condition  */
			/* skip to matching COND_END */
			if (aCalcPerformDebug>=20) {printf("aCalcPerform:cond_ else\n");}
			for (got_if=1; got_if>0 && post[1] != END_STACK; ++post) {
				if (aCalcPerformDebug>=20) {printf("aCalcPerform:cond_else: *post=%d\n", *post);}
				switch(post[1]) {
				case LITERAL:	post+=8; break;
				case COND_IF:	got_if++; break;
				case COND_END:	got_if--; break;
				case FETCH: case AFETCH: post++; break;
				}
			}
			if (aCalcPerformDebug>=20) {printf("aCalcPerform:skipped to cond_end: *post=%d\n", *post);}

			break;

		case COND_END:
			break;

		/* Normal one-argument functiona and operators */
		case ABS_VAL:
		case UNARY_NEG:
		case SQU_RT:
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
		case NINT:
		case AMAX:
		case AMIN:
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
			checkStackElement(ps, *post);
			if (isArray(ps)) {
				switch (currSymbol) {
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
				case NINT: for (i=0; i<arraySize; i++) {
								ps->a[i] = (double)(long)(ps->a[i] >= 0 ? ps->a[i]+0.5 : ps->a[i]-0.5);
							}
							break;
				case AMAX:
					for (i=1, d=ps->a[0]; i<arraySize; i++) {if (ps->a[i]>d) d = ps->a[i];}
					toDouble(ps);
					ps->d = d;
					break;
				case AMIN:
					for (i=1, d=ps->a[0]; i<arraySize; i++) {if (ps->a[i]<d) d = ps->a[i];}
					toDouble(ps);
					ps->d = d;
					break;
				case REL_NOT: for (i=0; i<arraySize; i++) {ps->a[i] = (ps->a[i] ? 0 : 1);} break;
				case BIT_NOT: for (i=0; i<arraySize; i++) {ps->a[i] = ~(int)(ps->a[i]);} break;
				case AVERAGE:
					for (i=1, d=ps->a[0]; i<arraySize; i++) {d += ps->a[i];}
					toDouble(ps);
					ps->d = d/arraySize;
					break;
				case STD_DEV:
					for (i=1, d=ps->a[0]; i<arraySize; i++) {d += ps->a[i];}
					d /= arraySize;
					for (i=0, e=0.; i<arraySize; i++) {e += (ps->a[i]-d)*(ps->a[i]-d);}
					toDouble(ps);
					if (arraySize > 1)
						ps->d = sqrt(e/(arraySize-1));
					else
						ps->d = sqrt(e/arraySize);
					break;
				case FWHM:
					/* find max (d), min (e) values, and index (j) of max value */
					d = ps->a[0];
					e = ps->a[0];
					for (i=1, j=0; i<arraySize; i++) {
						if (ps->a[i] > d) {
							d = ps->a[i];
							j = i;
						}
						if (ps->a[i] < e) {
							e = ps->a[i];
						}
					}
					if (aCalcPerformDebug) {printf("max=%f, at %d; min=%f\n", d, j, e);}
					d = e + (d-e)/2;
					/* walk forwards from peak */
					for (i=j+1, found=0; i<arraySize; i++) {
						if (ps->a[i] < d) {
							found = 1;
							e = (i-1) + (d - ps->a[i-1])/(ps->a[i] - ps->a[i-1]);
							if (aCalcPerformDebug) {printf("halfmax at index %f\n", e);}
							break;
						}
					}
					if (!found) e = arraySize-1;
					/* walk backwards from peak */
					for (i=j-1, found=0; i>=0; i--) {
						if (ps->a[i] < d) {
							found = 1;
							d = i + (d - ps->a[i])/(ps->a[i+1] - ps->a[i]);
							if (aCalcPerformDebug) {printf("halfmax at index %f\n", d);}
							break;
						}
					}
					if (!found) d = 0;
					toDouble(ps);
					ps->d = e-d;
					break;
				case SMOOTH:
					d = ps->a[0]; e = ps->a[1]; f=ps->a[2];
					for (i=2; i<arraySize-2; i++) {
						ps->a[i] = d/16 + e/4 + 3*f/8 + ps->a[i+1]/4 + ps->a[i+2]/16;
						d=e; e=f; f=ps->a[i+1];
					}
					break;
				case DERIV:
					ps1 = ps; /* y values */
					INC(ps);
					ps->a = &(ps->array[0]);
					ps2 = ps; /* x values */
					for (i=0; i<arraySize; i++) {ps2->a[i] = i;}
					INC(ps);
					ps->a = &(ps->array[0]); /* place for deriv */
					status = deriv(ps2->a, ps1->a, arraySize, ps->a);
					for (i=0; i<arraySize; i++) {ps1->a[i] = ps->a[i];}
					DEC(ps); DEC(ps);
					break;
				case ARRSUM:
					for (i=0, d=0.0; i<arraySize; i++) {
						d += ps->a[i];
					}
					toDouble(ps);
					ps->d = d;
					break;
				case FITPOLY:
					ps1 = ps; /* y values */
					INC(ps);
					ps->a = &(ps->array[0]);
					ps2 = ps; /* x values */
					for (i=0; i<arraySize; i++) {ps2->a[i] = i;}
					INC(ps);
					ps->a = &(ps->array[0]); /* place for deriv */
					status = fitpoly(ps2->a, ps1->a, arraySize, &d, &e, &f, NULL);
					for (i=0; i<arraySize; i++) {
						ps1->a[i] = d + e*ps2->a[i] + f*(ps2->a[i])*(ps2->a[i]);
					}
					DEC(ps); DEC(ps);
					break;
				case FITMPOLY:
					ps3 = ps; /* mask array */
					DEC(ps);
					ps1 = ps; /* y values */
					INC(ps); INC(ps); /* point to unused value-stack element */
					ps->a = &(ps->array[0]);
					ps2 = ps; /* x values */
					for (i=0; i<arraySize; i++) {ps2->a[i] = i;}
					INC(ps); /* point to unused value-stack element */
					ps->a = &(ps->array[0]); /* place for deriv */
					status = fitpoly(ps2->a, ps1->a, arraySize, &d, &e, &f, ps3->a);
					for (i=0; i<arraySize; i++) {
						ps1->a[i] = d + e*ps2->a[i] + f*(ps2->a[i])*(ps2->a[i]);
					}
					DEC(ps); DEC(ps); DEC(ps);
					break;
				}
			} else {
				switch (currSymbol) {
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
				case NINT: ps->d = (double)(long)(ps->d >= 0 ? ps->d+0.5 : ps->d-0.5); break;
				case AMAX: break;
				case AMIN: break;
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


		case ARANDOM:
			INC(ps);
			ps->a = &(ps->array[0]);
			for (i=0; i<arraySize; i++) ps->a[i] = local_random();
			break;

		case RANDOM:
			INC(ps);
			ps->d = local_random();
			ps->a = NULL;
			break;

		case EXPON:
			checkStackElement(ps, *post);
			ps1 = ps;
			DEC(ps);
			checkStackElement(ps, *post);
			toDouble(ps1);
			/* if exponent is not integer, use nearest integer */
			j = myNINT(ps1->d);
			if (isArray(ps)) {
				for (i=0; i<arraySize; i++) {
					if (ps->a[i] == 0) continue;
					if (ps->a[i] < 0) {
       					ps->a[i] = exp(j * log(-(ps->a[i])));
						/* is value negative */
						if ((j % 2) > 0) ps->a[i] = -ps->a[i];
					} else {
						ps->a[i] = exp(j * log(ps->a[i]));
					}
				}
			} else {
				if (ps->d == 0) break;
				if (ps->d < 0) {
       				ps->d = exp(j * log(-(ps->d)));
					/* is value negative */
					if ((j % 2) > 0) ps->d = -ps->d;
				} else {
					ps->d = exp(j * log(ps->d));
				}
			}
			break;

		/* Normal two-argument functiona and operators */
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
			checkStackElement(ps, *post);
			ps1 = ps;
			DEC(ps);
			checkStackElement(ps, *post);
			if (isArray(ps) || isArray(ps1)) {
				toArray(ps);
				toArray(ps1);
				switch (currSymbol) {
				case GR_OR_EQ: for (i=0; i<arraySize; i++) ps->a[i] = ps->a[i] >= ps1->a[i]; break;
				case GR_THAN: for (i=0; i<arraySize; i++) ps->a[i] = ps->a[i] > ps1->a[i]; break;
				case LESS_OR_EQ: for (i=0; i<arraySize; i++) ps->a[i] = ps->a[i] <= ps1->a[i]; break;
				case LESS_THAN: for (i=0; i<arraySize; i++) ps->a[i] = ps->a[i] < ps1->a[i]; break;
				case NOT_EQ: for (i=0; i<arraySize; i++) ps->a[i] = ps->a[i] != ps1->a[i]; break;
				case EQUAL: for (i=0; i<arraySize; i++) ps->a[i] = ps->a[i] == ps1->a[i]; break;
				case MAX_VAL: for (i=0; i<arraySize; i++) if (ps->a[i] < ps1->a[i]) ps->a[i] = ps1->a[i]; break;
				case MIN_VAL: for (i=0; i<arraySize; i++) if (ps->a[i] > ps1->a[i]) ps->a[i] = ps1->a[i]; break;
				case REL_OR: for (i=0; i<arraySize; i++) ps->a[i] = ps->a[i] || ps1->a[i]; break;
				case REL_AND: for (i=0; i<arraySize; i++) ps->a[i] = ps->a[i] && ps1->a[i]; break;
				case BIT_OR: for (i=0; i<arraySize; i++) ps->a[i] = (int)ps->a[i] | (int)ps1->a[i]; break;
				case BIT_AND: for (i=0; i<arraySize; i++) ps->a[i] = (int)ps->a[i] & (int)ps1->a[i]; break;
				case BIT_EXCL_OR: for (i=0; i<arraySize; i++) ps->a[i] = (int)ps->a[i] ^ (int)ps1->a[i]; break;
		 		case ATAN2: for (i=0; i<arraySize; i++) ps->a[i] = atan2(ps1->a[i], ps->a[i]); break;
				}
			} else {
				switch (currSymbol) {
				case GR_OR_EQ: ps->d = ps->d >= ps1->d; break;
				case GR_THAN: ps->d = ps->d > ps1->d; break;
				case LESS_OR_EQ: ps->d = ps->d <= ps1->d; break;
				case LESS_THAN: ps->d = ps->d < ps1->d; break;
				case NOT_EQ: ps->d = ps->d != ps1->d; break;
				case EQUAL: ps->d = ps->d == ps1->d; break;
				case MAX_VAL: if (ps->d < ps1->d) ps->d = ps1->d; break;
				case MIN_VAL: if (ps->d > ps1->d) ps->d = ps1->d; break;
				case REL_OR: ps->d = ps->d || ps1->d; break;
				case REL_AND: ps->d = ps->d && ps1->d; break;
				case BIT_OR: ps->d = (int)ps->d | (int)ps1->d; break;
				case BIT_AND: ps->d = (int)ps->d & (int)ps1->d; break;
				case BIT_EXCL_OR: ps->d = (int)ps->d ^ (int)ps1->d; break;
				case ATAN2: ps->d = atan2(ps1->d, ps->d);
				}
			}
			break;

		case RIGHT_SHIFT:
		case LEFT_SHIFT:
			checkStackElement(ps, *post);
			ps1 = ps;
			DEC(ps);
			checkStackElement(ps, *post);
			toDouble(ps1);
			if (isDouble(ps)) {
				/* scalar variable: bit shift by integer amount */
				if (currSymbol == RIGHT_SHIFT) {
					ps->d = (int)(ps->d) >> (int)(ps1->d);
				} else {
					ps->d = (int)(ps->d) << (int)(ps1->d);
				}
			} else {
				/* array variable: shift array elements */
				e = ps1->d;	/* num channels to shift */
				if (currSymbol == LEFT_SHIFT)  e = -e;
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
			checkStackElement(ps, *post);
			if (isDouble(ps)) {
				d = ps->d;
			} else {
				d = ps->a[0];
				ps->a = NULL;
			}
			i = (int)(d >= 0 ? d+0.5 : 0);
			ps->d = (i < num_dArgs) ? p_dArg[i] : 0;
			break;

		case A_AFETCH:
			checkStackElement(ps, *post);
			toDouble(ps);
			d = ps->d;
			ps->a = &(ps->array[0]);
			ps->a[0] = '\0';
			j = (int)(d >= 0 ? d+0.5 : 0);
			if (j < num_aArgs) {
				if (pp_aArg[j]) {
					for (i=0; i<arraySize; i++) ps->a[i] = pp_aArg[j][i];
				} else {
					for (i=0; i<arraySize; i++) ps->a[i] = 0.0;
				}
			}
			break;

		case LITERAL:
			INC(ps);
			++post;
			if (post == NULL) {
				++post;
				printf("%.7s bad constant in expression\n",post);
				ps->a = NULL;
				ps->d = 0.;
				break;
			}
			memcpy((void *)&(ps->d),post,8);
			ps->a = NULL;
			post += 7;
			break;

		case TO_DOUBLE:
			checkStackElement(ps, *post);
			toDouble(ps);
			break;

		case TO_ARRAY:
			checkStackElement(ps, *post);
			toArray(ps);
			break;

		case SUBRANGE:
		case SUBRANGE_IP:
			checkStackElement(ps, *post);
			ps2 = ps;
			DEC(ps);
			checkStackElement(ps, *post);
			ps1 = ps;
			DEC(ps);
			checkStackElement(ps, *post);
			toArray(ps);
			toDouble(ps1);
			i = (int)ps1->d;
			if (i < 0) i += arraySize;
			toDouble(ps2);
			j = (int)ps2->d;
			if (j < 0) j += arraySize;
			i = MAX(MIN(i,arraySize),0);
			j = MIN(j,arraySize);
			if (currSymbol == SUBRANGE) {
				for(k=0; i<=j; k++, i++) ps->a[k] = ps->a[i];
				for( ; k<arraySize; k++) ps->a[k] = 0.;
			} else {
				for(k=0; k<i; k++) ps->a[k] = 0.;
				for(k=j; k<arraySize; k++) ps->a[k] = 0.;
			}
			break;

		default:
			break;
		}

		/* move ahead in postfix expression */
		++post;
		if (aCalcPerformDebug>=20) printf("aCalcPerform:bottom of switch *post=%d\n", *post);

	}

	if (aCalcPerformDebug>=20) printf("aCalcPerform:done with expression, status=%d\n", status);
	if (status) {
		stackInUse=0;
		return(status);
	}

	/* if everything is peachy,the stack should end at its first position */
	if (ps != top) {
#if DEBUG
		if (aCalcPerformDebug>=10) {
			printf("aCalcPerform: stack error,ps=%p,top=%p\n", (void *)ps, (void *)top);
			printf("aCalcPerform: ps->d=%f\n", ps->d);
		}
#endif
		stackInUse=0;
		return(-1);
	}
	
	if (isDouble(ps)) {
		if (aCalcPerformDebug>=20) printf("aCalcPerform:double result=%f\n", ps->d);
		if (p_dresult) *p_dresult = ps->d;
		if (p_aresult) {
			to_array(ps);
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

	stackInUse=0;
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

