/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* sCalcPerform.c,v 1.7 2004/08/30 19:03:40 mooney Exp */
/*
 *	Original Author: Julie Sander and Bob Dalesio
 *	Date:	         07-27-87
 */

#ifdef vxWorks
#include <vxWorks.h>
#endif

#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<math.h>
#include	<ctype.h>	/* for isdigit() */

#include	"dbDefs.h"
#include "epicsMath.h"
#include	"cvtFast.h"
#include	"epicsString.h"
#include	"epicsStdio.h"	/* for  epicsSnprintf() */

#define epicsExportSharedSymbols
#include	"sCalcPostfix.h"
#include	"sCalcPostfixPvt.h"
#include <epicsExport.h>

static double	local_random();
static int cond_search(const unsigned char **ppinst, int match);

#define myNINT(a) ((int)((a) >= 0 ? (a)+0.5 : (a)-0.5))
#ifndef PI
#define PI 3.14159265358979323
#endif
#define myMAX(a,b) (a)>(b)?(a):(b)
#define myMIN(a,b) (a)<(b)?(a):(b)
#define SMALL 1.e-11

#define DEBUG 1
#define INIT_STACK 1
volatile int sCalcPerformDebug = 0;
epicsExportAddress(int, sCalcPerformDebug);
volatile int sCalcLoopMax = 1000;
epicsExportAddress(int, sCalcLoopMax);

int sCalcStackHW = 0;	/* high-water mark */
int sCalcStackLW = 0;	/* low-water mark */
epicsExportAddress(int, sCalcStackHW);
epicsExportAddress(int, sCalcStackLW);
#if DEBUG
#define INC(ps) {if ((int)(++(ps)-top) > sCalcStackHW) sCalcStackHW = (int)((ps)-top);}
#define DEC(ps) {if ((int)(--(ps)-top) < sCalcStackLW) sCalcStackLW = (int)((ps)-top);}
#else
#define INC(ps) ++ps
#define DEC(ps) ps--
#endif

/* strncpy sucks (may copy extra characters, may not null-terminate) */
#define strNcpy(dest, src, N) {			\
	int ii;								\
	char *dd=(dest), *ss=(src);				\
	for (ii=0; *ss && ii < (N)-1; ii++)	\
		*dd++ = *ss++;					\
	*dd = '\0';							\
}

#define isDouble(ps) ((ps)->s==NULL)
#define isString(ps) ((ps)->s)

/* convert stack element of unknown type to double */
#define toDouble(ps) {if (isString(ps)) to_double(ps);}

/* convert string-valued stack element to double */
#define to_double(ps) {(ps)->d = atof((ps)->s);	(ps)->s = NULL;}

/* convert stack element of unknown type to string */
#define toString(ps) {if (isDouble(ps)) to_string(ps);}

/* convert double-valued stack element to string */
/* Note cvtDoubleToString(x, x, prec)  results in (slow) sprintf call if prec > 8 */
#define to_string(ps) {									\
	(ps)->s = &((ps)->local_string[0]);					\
	if (isnan((ps)->d))									\
		strcpy((ps)->s,"NaN");							\
	else												\
		(void)cvtDoubleToString((ps)->d, (ps)->s, 8);	\
}

/*
 * Find first conversion indicator in format string that is not assign suppressed,
 * and return a pointer to it.
 * Examples:
 *    "%f"     the conversion indicator is 'f'
 *    "%*2f%c" the conversion indicator is 'c'
 */
static char *findConversionIndicator(char *s)
{
	char *cc=NULL, *s1, *retval;

	while (s && *s) {
		if ((s1 = strstr(s, "%%")) != NULL) {
			/* not a conversion/assignment indicator; skip over */
			s = s1+2; continue;
		}
		if ((s = strchr(s, (int)'%')) == NULL) {
			return(NULL);
		}
		if ((cc = strpbrk(s, "pwn$c[deEfgGiousxX")) == NULL) {
			return(NULL);
		}
		/*
		 * (*cc) is a conversion character; look for suppressed assignment
		 * ('*' occurring after '%' and before conversion character)
		 */
		s1 = strchr(s, (int)'*');
		if (s1 && (s1 < cc)) {
			/* suppressed assignment; skip past conversion character */
			s = cc+1;
			if (*cc == '[') {
				/* skip character set ([..], []..], or [^]..]) */
				if (cc[1] == ']') {
					s = &(cc[2]);
				} else if ((cc[1] == '^') && (cc[2] == ']')) {
					s = &(cc[3]);
				}
				s = strchr(s, (int)']');
				if (s == NULL) {
					/* bad character-set syntax */
					return(NULL);
				}
				s++; /* skip past ']' */
			}
			/* keep looking for conversion/assignment character */

			continue;
		} else {
			/* (*cc) is a conversion/assignment character */
			break;
		}
	}
	if (cc == NULL) return(NULL);
	retval = cc;
	/*
	 * (*cc) is a conversion/assignment indicator.  Make sure there
	 * aren't any more in the format string.
	 */
	s = cc+1;
	while (s && *s) {
		if ((s1 = strstr(s, "%%")) != NULL) {
			/* not a conversion/assignment indicator; skip over */
			s = s1+2; continue;
		}
		if ((s = strchr(s, (int)'%')) == NULL) return(retval);
		if ((cc = strpbrk(s, "pwn$c[deEfgGiousxX")) == NULL) return(retval);
		/*
		 * (*cc) is a conversion character; look for suppressed assignment
		 * ('*' occurring after '%' and before conversion character)
		 */
		s1 = strchr(s, (int)'*');
		if (s1 && (s1 < cc)) {
			/* suppressed assignment; skip past conversion character */
			s = cc+1;
			if (*cc == '[') {
				/* skip character set ([..], []..], or [^]..]) */
				if (cc[1] == ']') {
					s = &(cc[2]);
				} else if ((cc[1] == '^') && (cc[2] == ']')) {
					s = &(cc[3]);
				}
				s = strchr(s, (int)']');
				if (s == NULL) return(NULL); /* bad character-set syntax */
				s++; /* skip past ']' */
			}
			continue;
		} else {
			/* (*cc) assignment is not suppressed */
			return(NULL);
		}
	}
	return(retval);
}

#define POLYNOMIAL 0x0A001;
int crc16(char *output, char *rawInput)
{
	int i, j, len;
	unsigned int crc;
	char tranInput[100];

	len = dbTranslateEscape(tranInput, rawInput);
	if (len == 0) return(-1);
	if (sCalcPerformDebug>=5) {
		printf("input string(len=%d): ", len);
		for (i=0; i<len; i++) printf("0x%02x ", tranInput[i]);
		printf("\n");
	}
	crc = 0xffff;
	if (sCalcPerformDebug>=10) printf("crc=0x%04x\n", crc);
	/* Loop through the translated input string */
	for (i=0; i<len; i++) {
		/* Exclusive-OR the next input byte with the low byte of the CRC */
		/* crc = (crc&0x0ff00) | ((crc&0x0ff) ^ (unsigned int)tranInput[i]);*/
		crc ^= (unsigned int)tranInput[i];
		if (sCalcPerformDebug>=10) printf("crc=0x%04x\n", crc);
		/* Loop through all 8 data bits */
		for (j=0; j<8; j++) {
			if (crc & 0x0001) {
				crc >>= 1;
				/* If the LSB is 1, XOR the polynomial mask with the CRC */
				crc ^= POLYNOMIAL;
			} else {
				crc >>= 1;
			}
		if (sCalcPerformDebug>=10) printf("crc=0x%04x\n", crc);
		}
	}
	/* put the CRC (low byte first) into the output string, escaped */
	sprintf(output, "\\x%02x\\x%02x", crc&0xff, (crc&0xff00)>>8);
	return(0);
}

/* try this for modbus/Ascii*/
int hex(char c) {
	if (isxdigit((int) c)) {
		if (isdigit((int)c))
			return((int)c - (int)'0');
		else
			return(10 + toupper((int)c) - (int)'A');
	}
	return(0);
}

int lrc(char *output, char *rawInput)
{
	int i;
	unsigned int lrc;

	for (i=0, lrc=0; i<strlen(rawInput)-1; i+=2) {
		lrc += hex(rawInput[i])*0x10 + hex(rawInput[i+1]);
		if (sCalcPerformDebug>=20) printf("lrc: adding %d\n", rawInput[i]*0x10 + rawInput[i+1]);
	}
	lrc &= 0xff;
	lrc = -lrc;
	if (sCalcPerformDebug>=10) printf("lrc=0x%04x\n", lrc);
	/* put the LRC into the output string, escaped */
	sprintf(output, "%02X", lrc&0xff);
	return(0);
}

int xor8(char *output, char *rawInput)
{
	int i, len;
	unsigned int xor8;
	char tranInput[100];

	len = dbTranslateEscape(tranInput, rawInput);
	if (len == 0) return(-1);
	if (sCalcPerformDebug>=5) {
		printf("input string(len=%d): ", len);
		for (i=0; i<len; i++) printf("0x%02x ", tranInput[i]);
		printf("\n");
	}
	xor8 = 0;
	/* Loop through the translated input string */
	for (i=0; i<len; i++) {
		xor8 ^= (unsigned int)tranInput[i];
	}
	if (sCalcPerformDebug>=10) printf("xor8=0x%04x\n", xor8);
	/* put XOR8 into the output string, escaped */
	sprintf(output, "\\x%02x", xor8&0xff);
	return(0);
}

void showStack_usesString(struct stackElement *ps) {
	int i;
	printf("stack: ");
	for (i=0; i<3; i++, ps--) {
		if (isDouble(ps))
			printf("%f ", ps->d);
		else 
			printf("'%s' ", ps->s);
	}
	printf("\n");
}

void showStack_noString(double *pd) {
	int i;
	printf("stack: ");
	for (i=0; i<3; i++, pd--) {
		printf("%f ", *pd);
	}
	printf("\n");
}

struct until_struct {
	const unsigned char *until_loc;
	const unsigned char *until_end_loc;
	double *pd;
	struct stackElement *ps;
};

#define TMPSTR_SIZE 1000
epicsShareFunc long 
	sCalcPerform(double *parg, int numArgs, char **psarg, int numSArgs, double *presult, char *psresult,
	int lenSresult, const unsigned char *postfix)
{
	struct stackElement stack[SCALC_STACKSIZE], *top;
	struct stackElement *ps, *ps1, *ps2;
	char				*s2, tmpstr[TMPSTR_SIZE], tmpstr10[10];
	char				*s, *s1, c;
	int					i, j, k;
	long				l = 0L;
	unsigned short		ui;
	unsigned long		ul = 0UL;
	float				f;
	double				d;
	double				*topd, *pd, *pd1, *save_pd;
	short 				h;
	int					op, nargs;
	const unsigned char *post = postfix;
	struct until_struct	until_scratch[10];
	int					loopsDone = 0;

	for (i=0; i<10; i++) {
		until_scratch[i].until_loc = NULL;
		until_scratch[i].until_end_loc = NULL;
		until_scratch[i].ps = NULL;
		until_scratch[i].pd = NULL;
	}
	/* find all UNTIL operators in postfix, noting their locations */
	for (i=0, post=postfix; *post != END_EXPRESSION; post++) {
		if (sCalcPerformDebug > 10) printf("\tsCalcPerform: *post=%d\n", *post);
		switch (*post) {
		case LITERAL_DOUBLE:
			post += sizeof(double);
			break;
		case LITERAL_INT:
			post += sizeof(int);
			break;
		case LITERAL_STRING:
			++post;
			post += strlen((char *)post)+1;
			break;
		case UNTIL:
			/*printf("sCalcPerform: UNTIL at index %d\n", (int)(post-postfix));*/
			until_scratch[i].until_loc = post;
			i++;
			if (i>9) {
				printf("sCalcPerform: too many UNTILs\n");
				return(-1);
			}
			break;
		case UNTIL_END:
			for (k=i-1; k>=0; k--) {
				if (until_scratch[k].until_end_loc == NULL) {
					/* found unclaimed UNTIL */
					/* printf("sCalcPerform: UNTIL_END at index %d, matches UNTIL at index %d (k=%d)\n",
						(int)(post-postfix), (int)(until_scratch[k].until_loc-postfix), k);*/
					until_scratch[k].until_end_loc = post;
					break;
				}
			}
			if (k<0) {
				printf("unmatched UNTIL_END\n");
				return(-1);
			}
			break;
		}
	}

	post = postfix;

#if DEBUG
	if (sCalcPerformDebug>=10) {
		printf("sCalcPerform: postfix:\n");
		sCalcExprDump(post);

		printf("\nsCalcPerform: args:\n");
		for (i=0; i<numArgs; i++) {
			if (i%4 == 0) printf("     ");
			printf("%c=%f\t", 'a'+i, parg[i]);
			if (i%4 == 3) printf("\n");
		}
	}
#endif

	if (*post == END_EXPRESSION) return(-1);


	if (*post++ != USES_STRING) {
		/* Don't use stack as an array of struct stackElement; just use the memory as an array of double. */

#if INIT_STACK
		for (i=0, pd = (double *)stack; i<SCALC_STACKSIZE; i++, pd++) {
			*pd = 0;
		}
#endif

		topd = pd = save_pd = (double *)&stack[1];
		pd--;

		/* No string expressions */
	    while ((op = *post++) != END_EXPRESSION){
			if (sCalcPerformDebug) sCalcPrintOp(op);
			if (sCalcPerformDebug>=15) showStack_noString(pd);

			switch (op) {

			case FETCH_A: case FETCH_B: case FETCH_C: case FETCH_D: case FETCH_E: case FETCH_F:
			case FETCH_G: case FETCH_H: case FETCH_I: case FETCH_J: case FETCH_K: case FETCH_L:
			case FETCH_M: case FETCH_N: case FETCH_O: case FETCH_P: 
				if (numArgs > (op - FETCH_A)) {
			    	*++pd = parg[op - FETCH_A];
				} else {
					/* caller didn't supply a large enough array */
					*++pd = 0.;
				}
				break;

			case STORE_A: case STORE_B: case STORE_C: case STORE_D: case STORE_E: case STORE_F:
			case STORE_G: case STORE_H: case STORE_I: case STORE_J: case STORE_K: case STORE_L:
			case STORE_M: case STORE_N: case STORE_O: case STORE_P:
				if (numArgs > (op - STORE_A)) {
	    			parg[op - STORE_A] = *pd--;
				} else {
					/* caller didn't supply a large enough array */
					pd--;
				}
			    break;

			case A_STORE:
				pd1 = pd--;
				d = *pd--;
				i = myNINT(d);
				if (i >= numArgs || i < 0) {
					printf("sCalcPerform: fetch index, %d, out of range.\n", i);
				} else {
	    			parg[i] = *pd1;
				}
				break;

			case FETCH_VAL:
				*++pd = *presult;
				break;

			case CONST_PI:
				++pd;
				*pd = PI;
				break;

			case CONST_D2R:
				++pd;
				*pd = PI/180.;
				break;

			case CONST_R2D:
				++pd;
				*pd = 180./PI;
				break;

			case CONST_S2R:
				++pd;
				*pd = PI/(180.*3600);
				break;

			case CONST_R2S:
				++pd;
				*pd = (180.*3600)/PI;
				break;

			case ADD:
				--pd;
				*pd = *pd + pd[1];
				break;

			case SUB:
				--pd;
				*pd = *pd - pd[1];
				break;

			case MULT:
				--pd;
				*pd = *pd * pd[1];
				break;

			case DIV:
				--pd;
				if (pd[1] == 0) /* can't divide by zero */
					return(-1);
				*pd = *pd / pd[1];
				break;

			case COND_IF:
				if (*pd-- == 0.0 &&	cond_search(&post, COND_ELSE)) return -1;
				break;
				
			case COND_ELSE:
				if (cond_search(&post, COND_END)) return -1;
				break;

			case COND_END:
				break;

			case ABS_VAL:
				if (*pd < 0 ) *pd *= -1;
				break;

			case UNARY_NEG:
				*pd *= -1;
				break;

			case SQU_RT:
				/* check for neg number */
				if (*pd < 0) return(-1);	
				*pd = sqrt(*pd);
				break;

			case EXP:
				*pd = exp(*pd);
				break;

			case LOG_10:
				/* check for neg number */
				if (*pd < 0) return(-1);
				*pd = log10(*pd);
				break;

			case LOG_E:
				/* check for neg number */
				if (*pd < 0) return(-1);
				*pd = log(*pd);
				break;

			case RANDOM:	/* Uniformly distributed in (0,1] (i.e., never zero). */
				++pd;
				*pd = local_random();
				break;

			case NORMAL_RNDM:	/* Normally distributed about zero, with std dev = 1. */
				++pd;
				*pd = sqrt(-2*log(local_random())) * cos(2*PI*local_random());
				break;

			case POWER:
				--pd;
				*pd = pow(*pd, pd[1]);
				break;

			case MODULO:
				--pd;
				if ((int)(pd[1]) == 0)
					return(-1);
				*pd = (double)((int)(*pd) % (int)(pd[1]));
				break;

			case REL_OR:
				--pd;
				*pd = *pd || pd[1];
				break;

			case REL_AND:
				--pd;
				*pd = *pd && pd[1];
				break;

			case BIT_OR:
				/* force double values into integers and or them */
				--pd;
				*pd = (long)(pd[1]) | (long)(*pd);
				break;

			case BIT_AND:
				/* force double values into integers and and them */
				--pd;
				*pd = (long)(pd[1]) & (long)(*pd);
				break;

			case BIT_EXCL_OR:
				/* force double values to integers to exclusive or them */
				--pd;
				*pd = (long)(pd[1]) ^ (long)(*pd);
				break;

			case GR_OR_EQ:
				--pd;
				*pd = (fabs(*pd-pd[1]) < SMALL) || (*pd > pd[1]);
				break;

			case GR_THAN:
				--pd;
				*pd = (*pd - pd[1]) > SMALL;
				break;

			case LESS_OR_EQ:
				--pd;
				*pd = (fabs(*pd-pd[1]) < SMALL) || (*pd < pd[1]);
				break;

			case LESS_THAN:
				--pd;
				*pd = (pd[1] - *pd) > SMALL;
				break;

			case NOT_EQ:
				--pd;
				*pd = (fabs(*pd-pd[1]) > SMALL);
				break;

			case EQUAL:
				--pd;
				*pd = (fabs(*pd-pd[1]) < SMALL);
				break;

			case RIGHT_SHIFT:
				--pd;
				*pd = (long)(*pd) >> (long)(pd[1]);
				break;

			case LEFT_SHIFT:
				--pd;
				*pd = (long)(*pd) << (long)(pd[1]);
				break;

			case MAX_VAL:
				--pd;
				if (*pd < pd[1]) *pd = pd[1];
				break;
 
			case MIN_VAL:
				--pd;
				if (*pd > pd[1]) *pd = pd[1];
				break;

			case ACOS:
				*pd = acos(*pd);
				break;

			case ASIN:
				*pd = asin(*pd);
				break;

			case ATAN:
				*pd = atan(*pd);
				break;

	 		case ATAN2:
				--pd;
	 			*pd = atan2(pd[1], *pd);
	 			break;

			case COS:
				*pd = cos(*pd);
				break;

			case SIN:
				*pd = sin(*pd);
				break;

			case TAN:
				*pd = tan(*pd);
				break;

			case COSH:
				*pd = cosh(*pd);
				break;

			case SINH:
				*pd = sinh(*pd);
				break;

			case TANH:
				*pd = tanh(*pd);
				break;

			case CEIL:
				*pd = ceil(*pd);
				break;

			case FLOOR:
				*pd = floor(*pd);
				break;

			case FINITE:
				nargs = *post++;
				d = finite(*pd);
				while (--nargs) {
					--pd;
					d = d && finite(*pd);
				}
				*pd = d;
				break;

			case ISINF:
				*pd = isinf(*pd);
				break;

			case ISNAN:
				nargs = *post++;
				d = isnan(*pd);
				while (--nargs) {
					--pd;
					d = d || isnan(*pd);
				}
				*pd = d;
				break;

			case NINT:
				d = *pd;
				*pd = (double)(long)(d >= 0 ? d+0.5 : d-0.5);
				break;

			case REL_NOT:
				*pd = (*pd ? 0 : 1);
				break;

			case BIT_NOT:
				*pd = ~(long)(*pd);
				break;

			case A_FETCH:
				d = *pd;
				i = myNINT(d);
				if (i >= numArgs || i < 0) {
					printf("sCalcPerform: fetch index, %d, out of range.\n", i);
					*pd = 0;
				} else {
					*pd = parg[i];
				}
				break;

			case LITERAL_DOUBLE:
				++pd;
				memcpy((void *)&(*pd),post,sizeof(double));
				post += sizeof(double);
				break;

			case LITERAL_INT:
				++pd;
				memcpy((void *)&i,post,sizeof(int));
				*pd = (double)i;
				post += sizeof(int);
				break;

			case MAX:
				nargs = *post++;
				while (--nargs) {
					d = *pd--;
					if (*pd < d || isnan(d))
						*pd = d;
				}
				break;
 
			case MIN:
				nargs = *post++;
				while (--nargs) {
					d = *pd--;
					if (*pd > d || isnan(d))
						*pd = d;
				}
				break;

			case UNTIL:
				if (sCalcPerformDebug) printf("\tUNTIL:*pd=%f\n", *pd);
				if (sCalcPerformDebug) printf("\tpost-1=%p\n", post-1);
				for (i=0; i<10; i++) {
					/* find ourselves in post, remembering that post was incremented at loop top */
					if (sCalcPerformDebug > 10) printf("\tuntil_scratch[i].until_loc=%p\n", until_scratch[i].until_loc);
					if (until_scratch[i].until_loc == post-1) {
						until_scratch[i].pd = pd;
						break;
					}
				}
				if (i==10) {
					printf("sCalcPerform: UNTIL not found\n");
					return(-1);
				}
				break;

			case UNTIL_END:
				if (sCalcPerformDebug) printf("\tUNTIL_END:*pd=%f\n", *pd);
				if (++loopsDone > sCalcLoopMax)
					break;
				if (*pd==0) {
					/* reset postfix to matching UNTIL code, stack to its loc at that time */
					--post;
					for (i=0; i<10; i++) {
						if (until_scratch[i].until_end_loc == post) {
							pd = until_scratch[i].pd;
							post = until_scratch[i].until_loc;
							if (sCalcPerformDebug) printf("--loop--\n");
							break;
						}
					}
					if (i==10) {
						printf("sCalcPerform: UNTIL not found\n");
						return(-1);
					}
					break;
				}
				break;

			default:
				break;
			}

		}

		/* if everything is peachy,the stack should end at its first position */
		if (pd != topd) {
#if DEBUG
			if (sCalcPerformDebug>=10)
				printf("sCalcPerform: stack error (pd-topd=%d)\n", (int)(pd-topd));
#endif
			return(-1);
		}

		*presult = *pd;
		if (psresult && (lenSresult > 15)) {
			if (isnan(*pd))
				strcpy(psresult,"NaN");
			else
				(void)cvtDoubleToString(*pd, psresult, 8);
		}

		return(((isnan(*presult)||isinf(*presult)) ? -1 : 0));

	} else {

		/*** expression requires string operations ***/
#if INIT_STACK
		for (i=0, ps=stack; i<SCALC_STACKSIZE; i++, ps++) {
			ps->d = 0;
			ps->s = NULL;
		}
#endif
		top = ps = &stack[10];
		ps--;  /* Expression handler assumes ps is pointing to a filled element */
		ps->d = 1.23456; ps->s = NULL;	/* telltale */

		/* string expressions and values handled */
	    while ((op = *post++) != END_EXPRESSION){
			if (sCalcPerformDebug) sCalcPrintOp(op);

			switch (op) {

			case FETCH_A: case FETCH_B: case FETCH_C: case FETCH_D: case FETCH_E: case FETCH_F:
			case FETCH_G: case FETCH_H: case FETCH_I: case FETCH_J: case FETCH_K: case FETCH_L:
			case FETCH_M: case FETCH_N: case FETCH_O: case FETCH_P: 
				if (numArgs > (op - FETCH_A)) {
					INC(ps); ps->s = NULL; ps->d = parg[op - FETCH_A];
				} else {
					/* caller didn't supply a large enough array */
					INC(ps);
				}
				break;

			case FETCH_AA: case FETCH_BB: case FETCH_CC: case FETCH_DD: case FETCH_EE: case FETCH_FF:
			case FETCH_GG: case FETCH_HH: case FETCH_II: case FETCH_JJ: case FETCH_KK: case FETCH_LL:
				INC(ps);
				ps->s = &(ps->local_string[0]);
				ps->s[0] = '\0';
				if (numSArgs > (op - FETCH_AA)) {
					strncpy(ps->s, psarg[op - FETCH_AA], SCALC_STRING_SIZE);
				} else {
					/* caller didn't supply a large enough array */
				}
				break;

			case STORE_A: case STORE_B: case STORE_C: case STORE_D: case STORE_E: case STORE_F:
			case STORE_G: case STORE_H: case STORE_I: case STORE_J: case STORE_K: case STORE_L:
			case STORE_M: case STORE_N: case STORE_O: case STORE_P:
				toDouble(ps);
				if (numArgs > (op - STORE_A)) {
	    			parg[op - STORE_A] = ps->d;
				}
				DEC(ps);
			    break;

			case STORE_AA: case STORE_BB: case STORE_CC: case STORE_DD: case STORE_EE: case STORE_FF:
			case STORE_GG: case STORE_HH: case STORE_II: case STORE_JJ: case STORE_KK: case STORE_LL:
				toString(ps);
				if (numSArgs > (op - STORE_AA)) {
					strncpy(psarg[op - STORE_AA], ps->s, SCALC_STRING_SIZE);
				}
				DEC(ps);
				break;

			case A_STORE:
				toDouble(ps);
				ps1 = ps; DEC(ps);
				toDouble(ps);
				i = myNINT(ps->d); DEC(ps);
				if (i >= numArgs || i < 0) {
					printf("sCalcPerform: fetch index, %d, out of range.\n", i);
				} else {
	    			parg[i] = ps1->d;
				}
				break;

			case A_SSTORE:
				toString(ps);
				ps1 = ps; DEC(ps);
				toDouble(ps);
				i = myNINT(ps->d); DEC(ps);
				if (i >= numSArgs || i < 0) {
					printf("sCalcPerform: fetch index, %d, out of range.\n", i);
				} else {
					strncpy(psarg[i], ps1->s, SCALC_STRING_SIZE);
				}
				break;

			case FETCH_VAL:
				INC(ps);
				ps->s = NULL;
				ps->d = *presult;
				break;

			case FETCH_SVAL:
				INC(ps);
				ps->s = &(ps->local_string[0]);
				ps->s[0] = '\0';
				strncpy(ps->s, psresult, SCALC_STRING_SIZE);
				break;

			case CONST_PI:
				INC(ps);
				ps->s = NULL;
				ps->d = PI;
				break;

			case CONST_D2R:
				INC(ps);
				ps->s = NULL;
				ps->d = PI/180.;
				break;

			case CONST_R2D:
				INC(ps);
				ps->s = NULL;
				ps->d = 180./PI;
				break;

			case CONST_S2R:
				INC(ps);
				ps->s = NULL;
				ps->d = PI/(180.*3600);
				break;

			case CONST_R2S:
				INC(ps);
				ps->s = NULL;
				ps->d = (180.*3600)/PI;
				break;

			case ADD:
				ps1 = ps;
				DEC(ps);
				if (isDouble(ps)) {
					toDouble(ps1);
					ps->d = ps->d + ps1->d;
				} else if (isDouble(ps1)) {
					to_double(ps);
					ps->d = ps->d + ps1->d;
				} else {
					/* concatenate two strings */
					strncat(ps->s, ps1->s, strlen(ps->s)-SCALC_STRING_SIZE-1);
				}
				break;

			case SUB:
			case SUBLAST:
				ps1 = ps;
				DEC(ps);
				if (isDouble(ps)) {
					toDouble(ps1);
					ps->d = ps->d - ps1->d;
				} else if (isDouble(ps1)) {
					to_double(ps);
					ps->d = ps->d - ps1->d;
				} else {
					/* subtract ps1->s from ps->s */
					if (ps1->s[0]) {
						if (op == SUB) {
							/* first occurrence of ps1->s */
							s = strstr(ps->s, ps1->s);
						} else {
							/* last occurrence of ps1->s */
							s1 = ps->s + strlen(ps->s) - strlen(ps1->s);
							for (s = NULL; (s == NULL) && (s1 > ps->s); s1--) {
								printf("comparing '%s' with '%s'\n", s1, ps1->s);
								if (strncmp(s1, ps1->s, strlen(ps1->s))==0) {
									s = s1;
								}
							}
						}
						if (s) {
							for (s1=s+strlen(ps1->s); *s1; ) 
								*s++ = *s1++;
							*s = '\0';
						}
					}
				}
				break;

			case MULT:
				ps1 = ps;
				DEC(ps);
				toDouble(ps1);
				toDouble(ps);
				ps->d = ps->d * ps1->d;
				break;

			case DIV:
				ps1 = ps;
				DEC(ps);
				toDouble(ps1);
				toDouble(ps);
				if (ps1->d == 0) /* can't divide by zero */
					return(-1);
				ps->d = ps->d / ps1->d;
				break;

			case COND_IF:
				toDouble(ps);
				d = ps->d;
				DEC(ps);
 				if (d == 0.0 &&	cond_search(&post, COND_ELSE)) return -1;
				break;
				
			case COND_ELSE:
				if (cond_search(&post, COND_END)) return -1;
				break;

			case COND_END:
				break;

			case ABS_VAL:
				toDouble(ps);
				if (ps->d < 0 ) ps->d *= -1;
				break;

			case UNARY_NEG:
				toDouble(ps);
				ps->d *= -1;
				break;

			case SQU_RT:
				toDouble(ps);
				/* check for neg number */
				if (ps->d < 0) return(-1);	
				ps->d = sqrt(ps->d);
				break;

			case EXP:
				toDouble(ps);
				ps->d = exp(ps->d);
				break;

			case LOG_10:
				toDouble(ps);
				/* check for neg number */
				if (ps->d < 0) return(-1);
				ps->d = log10(ps->d);
				break;

			case LOG_E:
				toDouble(ps);
				/* check for neg number */
				if (ps->d < 0) return(-1);
				ps->d = log(ps->d);
				break;

			case RANDOM:
				INC(ps);
				ps->d = local_random();
				ps->s = NULL;
				break;

			case NORMAL_RNDM:				
				INC(ps);
				ps->d = sqrt(-2*log(local_random())) * cos(2*PI*local_random());
				ps->s = NULL;
				break;

			case POWER:
				ps1 = ps;
				DEC(ps);
				toDouble(ps1);
				toDouble(ps);
				ps->d = pow(ps->d, ps1->d);
				break;

			case MODULO:
				ps1 = ps;
				DEC(ps);
				toDouble(ps1);
				toDouble(ps);
				if ((long)ps1->d == 0)
					return(-1);
				ps->d = (double)((long)ps->d % (long)ps1->d);
				break;

			case REL_OR:
				ps1 = ps;
				DEC(ps);
				toDouble(ps1);
				toDouble(ps);
				ps->d = ps->d || ps1->d;
				break;

			case REL_AND:
				ps1 = ps;
				DEC(ps);
				toDouble(ps1);
				toDouble(ps);
				ps->d = ps->d && ps1->d;
				break;

			case BIT_OR:
				/* force double values into integers and or them */
				ps1 = ps;
				DEC(ps);
				toDouble(ps1);
				toDouble(ps);
				ps->d = (long)(ps1->d) | (long)(ps->d);
				break;

			case BIT_AND:
				/* force double values into integers and and them */
				ps1 = ps;
				DEC(ps);
				toDouble(ps1);
				toDouble(ps);
				ps->d = (long)(ps1->d) & (long)(ps->d);
				break;

			case BIT_EXCL_OR:
				/* force double values to integers to exclusive or them */
				ps1 = ps;
				DEC(ps);
				toDouble(ps1);
				toDouble(ps);
				ps->d = (long)(ps1->d) ^ (long)(ps->d);
				break;

			case GR_OR_EQ:
				ps1 = ps;
				DEC(ps);
				if (isDouble(ps)) {
					toDouble(ps1);
					/* ps->d = ps->d >= ps1->d; */
					ps->d = (fabs(ps->d - ps1->d) < SMALL) || (ps->d > ps1->d);
				} else if (isDouble(ps1)) {
					to_double(ps);
					/* ps->d = ps->d >= ps1->d; */
					ps->d = (fabs(ps->d - ps1->d) < SMALL) || (ps->d > ps1->d);
				} else {
					/* compare ps->s to ps1->s */
					ps->d = (double)(strcmp(ps->s, ps1->s) >= 0);
					ps->s = NULL;
				}
				break;

			case GR_THAN:
				ps1 = ps;
				DEC(ps);
				if (isDouble(ps)) {
					toDouble(ps1);
					/* ps->d = ps->d > ps1->d; */
					ps->d = (ps->d - ps1->d) > SMALL;
				} else if (isDouble(ps1)) {
					to_double(ps);
					/* ps->d = ps->d > ps1->d; */
					ps->d = (ps->d - ps1->d) > SMALL;
				} else {
					/* compare ps->s to ps1->s */
					ps->d = (double)(strcmp(ps->s, ps1->s) > 0);
					ps->s = NULL;
				}
				break;

			case LESS_OR_EQ:
				ps1 = ps;
				DEC(ps);
				if (isDouble(ps)) {
					toDouble(ps1);
					/* ps->d = ps->d <= ps1->d; */
					ps->d = (fabs(ps->d - ps1->d) < SMALL) || (ps->d < ps1->d);
				} else if (isDouble(ps1)) {
					to_double(ps);
					/* ps->d = ps->d <= ps1->d; */
					ps->d = (fabs(ps->d - ps1->d) < SMALL) || (ps->d < ps1->d);
				} else {
					/* compare ps->s to ps1->s */
					ps->d = (double)(strcmp(ps->s, ps1->s) <= 0);
					ps->s = NULL;
				}
				break;

			case LESS_THAN:
				ps1 = ps;
				DEC(ps);
				if (isDouble(ps)) {
					toDouble(ps1);
					/* ps->d = ps->d < ps1->d; */
					ps->d = (ps1->d - ps->d) > SMALL;
				} else if (isDouble(ps1)) {
					to_double(ps);
					/* ps->d = ps->d < ps1->d; */
					ps->d = (ps1->d - ps->d) > SMALL;
				} else {
					/* compare ps->s to ps1->s */
					ps->d = (double)(strcmp(ps->s, ps1->s) < 0);
					ps->s = NULL;
				}
				break;

			case NOT_EQ:
				ps1 = ps;
				DEC(ps);
				if (isDouble(ps)) {
					toDouble(ps1);
					/* ps->d = ps->d != ps1->d; */
					ps->d = (fabs(ps->d - ps1->d) > SMALL);
				} else if (isDouble(ps1)) {
					to_double(ps);
					/* ps->d = ps->d != ps1->d; */
					ps->d = (fabs(ps->d - ps1->d) > SMALL);
				} else {
					/* compare ps->s to ps1->s */
					ps->d = (double)(strcmp(ps->s, ps1->s) != 0);
					ps->s = NULL;
				}
				break;

			case EQUAL:
				ps1 = ps;
				DEC(ps);
				if (isDouble(ps)) {
					toDouble(ps1);
					/* ps->d = ps->d == ps1->d; */
					ps->d = (fabs(ps->d - ps1->d) < SMALL);
				} else if (isDouble(ps1)) {
					to_double(ps);
					/* ps->d = ps->d == ps1->d; */
					ps->d = (fabs(ps->d - ps1->d) < SMALL);
				} else if ((isString(ps)) && (isString(ps1))) {
					/* compare ps->s to ps1->s */
					ps->d = (double)(strcmp(ps->s, ps1->s) == 0);
					ps->s = NULL;
				}
				break;

			case RIGHT_SHIFT:
			case LEFT_SHIFT:
				ps1 = ps;
				toDouble(ps1);
				j = myNINT(ps1->d);
				j = myMIN(j,SCALC_STRING_SIZE);
				DEC(ps);
				if (isDouble(ps)) {
					/* numeric variable: bit shift by integer amount */
					if (op == RIGHT_SHIFT) {
						ps->d = (int)(ps->d) >> (int)(ps1->d);
					} else {
						ps->d = (int)(ps->d) << (int)(ps1->d);
					}
				} else {
					/* string variable: shift array elements */
					if (op == RIGHT_SHIFT) {
						for (i=SCALC_STRING_SIZE-1; i>=0; i--) {
							ps->s[i] = (i>=j)?ps->s[i-j]:' ';
						}
						ps->s[SCALC_STRING_SIZE-1] = '\0';
					} else {
						if (j==SCALC_STRING_SIZE) {
							ps->s[0] = '\0';
						} else {
							for (i=0; i < (SCALC_STRING_SIZE-j); i++) {
								ps->s[i] = ps->s[i+j];
							}
						}
					}
				}
				break;

			case MAX_VAL:
				ps1 = ps;
				DEC(ps);
				if (isDouble(ps)) {
					toDouble(ps1);
					if (ps->d < ps1->d) ps->d = ps1->d;
				} else if (isDouble(ps1)) {
					to_double(ps);
					if (ps->d < ps1->d) ps->d = ps1->d;
				} else {
					/* compare ps->s to ps1->s */
					if (strcmp(ps->s, ps1->s) < 0) {
						strcpy(ps->s, ps1->s);
					}
				}
				break;
 
			case MIN_VAL:
				ps1 = ps;
				DEC(ps);
				if (isDouble(ps)) {
					toDouble(ps1);
					if (ps->d > ps1->d) ps->d = ps1->d;
				} else if (isDouble(ps1)) {
					to_double(ps);
					if (ps->d > ps1->d) ps->d = ps1->d;
				} else {
					/* compare ps->s to ps1->s */
					if (strcmp(ps->s, ps1->s) > 0) {
						strcpy(ps->s, ps1->s);
					}
				}
				break;

			case ACOS:
				toDouble(ps);
				ps->d = acos(ps->d);
				break;

			case ASIN:
				toDouble(ps);
				ps->d = asin(ps->d);
				break;

			case ATAN:
				toDouble(ps);
				ps->d = atan(ps->d);
				break;

	 		case ATAN2:
				ps1 = ps;
				DEC(ps);
				toDouble(ps1);
				toDouble(ps);
	 			ps->d = atan2(ps1->d, ps->d);
	 			break;

			case COS:
				toDouble(ps);
				ps->d = cos(ps->d);
				break;

			case SIN:
				toDouble(ps);
				ps->d = sin(ps->d);
				break;

			case TAN:
				toDouble(ps);
				ps->d = tan(ps->d);
				break;

			case COSH:
				toDouble(ps);
				ps->d = cosh(ps->d);
				break;

			case SINH:
				toDouble(ps);
				ps->d = sinh(ps->d);
				break;

			case TANH:
				toDouble(ps);
				ps->d = tanh(ps->d);
				break;

			case CEIL:
				toDouble(ps);
				ps->d = ceil(ps->d);
				break;

			case FLOOR:
				toDouble(ps);
				ps->d = floor(ps->d);
				break;

			case FINITE:
				nargs = *post++;
				toDouble(ps);
				d = finite(ps->d);
				while (--nargs) {
					DEC(ps);
					toDouble(ps);
					d = d && finite(ps->d);
				}
				ps->d = d;
				break;

			case ISINF:
				toDouble(ps);
				ps->d = isinf(ps->d);
				break;

			case ISNAN:
				nargs = *post++;
				toDouble(ps);
				d = isnan(ps->d);
				while (--nargs) {
					DEC(ps);
					toDouble(ps);
					d = d || isnan(ps->d);
				}
				ps->d = d;
				break;

			case NINT:
				if (isDouble(ps)) {
					d = ps->d;
				} else {
					/* hunt down number and convert */
					s = strpbrk(ps->s,"0123456789");
					if ((s > ps->s) && (s[-1] == '.')) s--;
					if ((s > ps->s) && (s[-1] == '-')) s--;
					d = s ? atof(s) : 0.0;
					ps->s = NULL;
				}
				ps->d = (double)(long)(d >= 0 ? d+0.5 : d-0.5);
				break;

			case REL_NOT:
				toDouble(ps);
				ps->d = (ps->d ? 0 : 1);
				break;

			case BIT_NOT:
				toDouble(ps);
				ps->d = ~(int)(ps->d);
				break;

			case A_FETCH:
				if (isDouble(ps)) {
					d = ps->d;
				} else {
					d = atof(ps->s);
					ps->s = NULL;
				}
				i = myNINT(d);
				if (i >= numArgs || i < 0) {
					printf("sCalcPerform: fetch index, %d, out of range.\n", i);
					ps->d = 0;
				} else {
					ps->d = parg[i];
				}
				break;

			case A_SFETCH:
				if (isDouble(ps)) {
					d = ps->d;
				} else {
					d = atof(ps->s);
				}
				ps->s = &(ps->local_string[0]);
				ps->s[0] = '\0';
				i = myNINT(d);
				if (i >= numSArgs || i < 0) {
					printf("sCalcPerform: fetch index, %d, out of range.\n", i);
				} else {
					strNcpy(ps->s, psarg[i], SCALC_STRING_SIZE);
				}
				break;

			case LITERAL_DOUBLE:
				INC(ps);
				memcpy((void *)&(ps->d),post,sizeof(double));
				ps->s = NULL;
				post += sizeof(double);
				break;

			case LITERAL_INT:
				INC(ps);
				memcpy((void *)&i,post,sizeof(int));
				ps->d = (double)i;
				ps->s = NULL;
				post += sizeof(int);
				break;

			case LITERAL_STRING:
				INC(ps);
				ps->s = &(ps->local_string[0]);
				s = ps->s;
				for (i=0; (i<SCALC_STRING_SIZE-1) && *post; )
					*s++ = (char)*post++;
				*s = '\0';
				/* skip to end of string, if we haven't already */
				while (*post) post++;
				post++;
				break;

			case TO_DOUBLE:
				if (isString(ps)) {
					/* hunt down number and convert */
					s = strpbrk(ps->s,"0123456789");
					if ((s > ps->s) && (s[-1] == '.')) s--;
					if ((s > ps->s) && (s[-1] == '-')) s--;
					ps->d = s ? atof(s) : 0.0;
					ps->s = NULL;
				}
				break;

			case TO_STRING:
				toString(ps);
				break;

			case LEN:
				toString(ps);
				for (i=0; (i < SCALC_STRING_SIZE) && ps->s[i]; i++)
					;
				ps->d = (double)i;
				ps->s = NULL;
				break;

			case BYTE:
				if (isString(ps)) {
					ps->d = ps->s[0];
					ps->s = NULL;
				}
				break;

	 		case PRINTF:
				ps1 = ps;
				DEC(ps);
				if (isDouble(ps))
					return(-1);
				s = ps->s;
				while ((s1 = strstr(s, "%%"))) {s = s1+2;}
				if (((s = strpbrk(s, "%")) == NULL) ||
					((s = strpbrk(s+1, "*cdeEfgGiousxX")) == NULL)) {
					/* no printf arguments needed */
		 			sprintf(tmpstr, ps->s);
				} else {
					switch (*s) {
					default: case '*':
						return(-1);
					case 'c': case 'd': case 'i': case 'o':
					case 'u': case 'x': case 'X':
						toDouble(ps1);
						l = myNINT(ps1->d);
	 					sprintf(tmpstr, ps->s, l);
						break;
					case 'e': case 'E': case 'f': case 'g': case 'G':
						toDouble(ps1);
	 					sprintf(tmpstr, ps->s, ps1->d);
						break;
					case 's':
						toString(ps1);
	 					sprintf(tmpstr, ps->s, ps1->s);
						break;
					}
				}
				strNcpy(ps->s, tmpstr, SCALC_STRING_SIZE-1);
				break;

	 		case BIN_WRITE:
				ps1 = ps;
				DEC(ps);
				if (isDouble(ps))
					return(-1);
				s = ps->s;
				while ((s1 = strstr(s, "%%"))) {s = s1+2;}
				if (((s = strpbrk(s, "%")) == NULL) ||
					((s = strpbrk(s+1, "*cdeEfgGiousxX")) == NULL)) {
					/* no printf arguments needed */
		 			return(-1);
				} else {
					switch (*s) {
					default: case '*':
						return(-1);
					case 'c':
						toDouble(ps1);
						c = myNINT(ps1->d);
						memcpy(ps->s, &c, 1);
						j = 1;
						break;
					case 'd': case 'i':
						toDouble(ps1);
						if (s[-1] == 'h') {
							h = myNINT(ps1->d);
							memcpy(ps->s, &h, 2);
							j = 2;
						} else {
							l = myNINT(ps1->d);
							memcpy(ps->s, &l, 4);
							j = 4;
						}
						break;
					case 'o': case 'u': case 'x': case 'X':
						toDouble(ps1);
						if (s[-1] == 'h') {
							ui = myNINT(ps1->d);
							memcpy(ps->s, &ui, 2);
							j = 2;
						} else {
							ul = myNINT(ps1->d);
							memcpy(ps->s, &ul, 4);
							j = 4;
						}
						break;
					case 'e': case 'E': case 'f': case 'g': case 'G':
						toDouble(ps1);
						if (s[-1] == 'l') {
							memcpy(ps->s, &(ps1->d), 8);
							j = 8;
						} else {
							f = ps1->d;
							memcpy(ps->s, &f, 4);
							j = 4;
						}
						break;
					case 's':
						return(-1);
					}
				}
		 		i = epicsStrSnPrintEscaped(tmpstr, SCALC_STRING_SIZE-1, ps->s, j);
				i = myMIN(i, SCALC_STRING_SIZE);
				tmpstr[i] = '\0'; /* make sure it's terminated */
				strNcpy(ps->s, tmpstr, SCALC_STRING_SIZE-1);
				break;

	 		case SSCANF:
				ps1 = ps;
				DEC(ps);
				if (isDouble(ps) || isDouble(ps1))
					return(-1);
				s = findConversionIndicator(ps1->s);
				if (s == NULL)
					return(-1);
				i = 1; /* successful return value from sscanf */
				switch (*s) {
				default: case 'p': case 'w': case 'n': case '$':
					return(-1);
				case 'd': case 'i':
					if (s[-1] == 'h') {
						h = 0;
		 				i = sscanf(ps->s, ps1->s, &h);
						ps->d = (double)h;
					} else if (s[-1] == 'l') {
						l = 0L;
		 				i = sscanf(ps->s, ps1->s, &l);
						ps->d = (double)l;
					} else {
						j = 0;
		 				i = sscanf(ps->s, ps1->s, &j);
						ps->d = (double)j;
					}
					ps->s = NULL;
					break;
				case 'o': case 'u': case 'x': case 'X':
					if (s[-1] == 'h') {
		 				i = sscanf(ps->s, ps1->s, &ui);
						ps->d = (double)ui;
					} else {
		 				i = sscanf(ps->s, ps1->s, &ul);
						ps->d = (double)ul;
					}
					ps->s = NULL;
					break;
				case 'e': case 'E': case 'f': case 'g': case 'G':
					if (s[-1] == 'l') {
		 				i = sscanf(ps->s, ps1->s, &(ps->d));
					} else {
		 				i = sscanf(ps->s, ps1->s, &f);
						ps->d = (double)f;
					}
					ps->s = NULL;
					break;
				case 'c': case '[': case 's':
		 			i = sscanf(ps->s, ps1->s, tmpstr);
					strNcpy(ps->s, tmpstr, SCALC_STRING_SIZE-1);
					break;
				}
				if (i != 1) {
					/* sscanf error */
					return(-1); 
				}
				break;

	 		case BIN_READ:
				ps1 = ps;
				DEC(ps);
				if (isDouble(ps) || isDouble(ps1))
					return(-1);
				/* find first conversion indicator that is not assign suppressed */
				s = findConversionIndicator(ps1->s);
				if (s == NULL)
					return(-1);
		 		i = dbTranslateEscape(tmpstr, ps->s);
				s1 = tmpstr;

				/*
				 * See if we have to skip over any bytes before copying data.
				 * I.e., check for conversion-suppression character upstream of s
				 */
				s2 = strchr(ps1->s, (int)'*');
				if (s2 && s2 < s) {
					/* Determine how many bytes we have to skip over.  Note
					 * we are permitting, e.g.,
					 *     "%*2f" 	skip over 2 4-byte floats
					 *     "%*2hd" 	skip over 2 2-byte shorts
					 *     "%*2c"   skip over 2 bytes
					 *     "%*2"    skip over 2 bytes (sscanf would not allow this)
					 */
					s2++;
					i = 1;
					if (isdigit((int)*s2)) {
						i = atoi(s2);
						while (isdigit((int)*s2)) s2++;
					}
					switch (*s2) {
						case 'h':
							i *= 2;
							break;
						case 'l':
							if (strpbrk(s2, "diouxX")) {
								i *= 4;
							} else {
								/* assume some kind of float */
								i *= 8;
							}
							break;
						case 'd': case 'i': case 'o': case 'u': case 'x': case 'X':
							if (s2[-1] == 'h') {
								i *= 2;
							} else {
								i *= 4;
							}
							break;
							
						case 'e': case 'E': case 'f': case 'g': case 'G':
							if (s2[-1] == 'l') {
								i *= 8;
							} else {
								i *= 4;
							}
							break;
					}
					/* skip past i bytes of the string to be read */
					s1 += i;
				}

				/* Do the read */
				switch (*s) {
				default: case 'p': case 'w': case 'n': case '$': case '[': case 's':
					/* unsupported conversion indicator */
					return(-1);
				case 'd': case 'i':
					if (s[-1] == 'h') {
						memcpy(&h, s1, 2);
						ps->d = (double)h;
					} else {
						memcpy(&l, s1, 4);
						ps->d = (double)l;
					}
					ps->s = NULL;
					break;
				case 'o': case 'u': case 'x': case 'X':
					if (s[-1] == 'h') {
						memcpy(&ui, s1, 2);
						ps->d = (double)ui;
					} else {
						memcpy(&ul, s1, 4);
						ps->d = (double)ul;
					}
					ps->s = NULL;
					break;
				case 'e': case 'E': case 'f': case 'g': case 'G':
					if (s[-1] == 'l') {
						memcpy(&(ps->d), s1, 8);
					} else {
						memcpy(&f, s1, 4);
						ps->d = (double)f;
					}
					ps->s = NULL;
					break;
				case 'c':
					memcpy(&c, s1, 1);
					ps->d = (double)c;
					ps->s = NULL;
					break;
				}
				break;

			case TR_ESC:
				if (isString(ps)) {
		 			i = dbTranslateEscape(tmpstr, ps->s);
					strNcpy(ps->s, tmpstr, SCALC_STRING_SIZE-1);
				}
				break;

			case ESC:
				if (isString(ps)) {
					/* Find length of input string.  Null terminates string.*/
					for (j=0; j<SCALC_STRING_SIZE && ps->s[j] != '\0'; j++) {
						;
					}
		 			i = epicsStrSnPrintEscaped(tmpstr, SCALC_STRING_SIZE-1, ps->s, j);
					i = myMIN(i, SCALC_STRING_SIZE);
					tmpstr[i] = '\0'; /* make sure it's terminated */
					strNcpy(ps->s, tmpstr, SCALC_STRING_SIZE-1);
				}
				break;

			case CRC16:
			case MODBUS:
				if (isString(ps)) {
		 			if (crc16(tmpstr, ps->s) == 0) {
						if (op==CRC16) {
							strNcpy(ps->s, tmpstr, SCALC_STRING_SIZE-1);
						} else {
							strncat(ps->s, tmpstr, strlen(ps->s)-SCALC_STRING_SIZE-1);
						}
					}
				}
				break;

			case LRC:
			case AMODBUS:
				/* Ascii modbus has to have its LRC computed on the binary command string, then the
				 * (binary) LRC is converted to hex characters and appended, then ':' is prepended.
				 * Example: To send the binary command "F7031389000A" (six hex bytes),
				 * you calculate the LRC (0x60), append to the command string,
				 * and convert hex character by character to ASCII, yielding
				 * "46 37 30 33 31 33 38 39 30 30 30 41 36 30"
				 * then you prepend ':' (0x3a) and append <cr><lf> (0x0d0a), yielding
				 * "3A 46 37 30 33 31 33 38 39 30 30 30 41 36 30 0D 0A"
				 */
				if (isString(ps)) {
		 			if (lrc(tmpstr10, ps->s) == 0) {
						if (op==LRC) {
							strNcpy(ps->s, tmpstr10, SCALC_STRING_SIZE-1);
						} else {
							strcpy(tmpstr, ":");
							strcat(tmpstr, ps->s);
							strNcpy(ps->s, tmpstr, strlen(ps->s)-SCALC_STRING_SIZE-1);
							strncat(ps->s, tmpstr10, strlen(ps->s)-SCALC_STRING_SIZE-1);
						}
					}
				}
				break;

			case XOR8:
			case ADD_XOR8:
				if (isString(ps)) {
		 			if (xor8(tmpstr, ps->s) == 0) {
						if (op==XOR8) {
							strNcpy(ps->s, tmpstr, SCALC_STRING_SIZE-1);
						} else {
							strncat(ps->s, tmpstr, strlen(ps->s)-SCALC_STRING_SIZE-1);
						}
					}
				}
				break;

			case SUBRANGE:
				ps2 = ps;
				DEC(ps);
				ps1 = ps;
				DEC(ps);
				toString(ps);
				k = strlen(ps->s);
				if (isDouble(ps1)) {
					i = (int)ps1->d;
					if (i < 0) i += k;
				} else {
					s = strstr(ps->s, ps1->s);
					i = s ? (s - ps->s) + strlen(ps1->s) : 0;
				}
				if (isDouble(ps2)) {
					j = (int)ps2->d;
					if (j < 0) j += k;
				} else {
					if (*(ps2->s)) {
						s = strstr(ps->s, ps2->s);
						j = s ? (s - ps->s) - 1 : k;
					} else {
						j = k;
					}
				}
				i = myMAX(myMIN(i,k),0);
				j = myMIN(j,k);
				s = ps->s;
				for (s1=s+i, s2=s+j ; *s1 && s1 <= s2; ) {
					*s++ = *s1++;
				}
				*s = 0;
				break;
 
			case REPLACE:
				ps2 = ps;
				DEC(ps);
				ps1 = ps;
				DEC(ps);
				toString(ps);					/* host string */
				toString(ps1);					/* text to be replaced */
				toString(ps2);					/* replacement text */
				s1 = strstr(ps->s, ps1->s);		/* first char of host to be replaced */
				if (s1 >= ps->s) {
					i = s1 - ps->s; 			/* running tab of chars in host string */
					s = s1 + strlen(ps1->s);	/* first char in host following replaced text */
					if (strlen(ps2->s) > strlen(ps1->s)) {
						strcpy(tmpstr, s);
						s = tmpstr;
					}
					for (s2 = ps2->s; *s2 && (i < (SCALC_STRING_SIZE-1)); i++)
						*s1++ = *s2++;
					while (*s && (i++ < (SCALC_STRING_SIZE-1)))
						*s1++ = *s++;
					*s1 = '\0';
				}
				break;
 

			case MAX:
				nargs = *post++;
				for(i=0, j=0; i<nargs; j |= isDouble(ps-i), i++);
				if (j) {
					/* an arg is double.  coerce all to double */
					toDouble(ps);
					while (--nargs) {
						d = ps->d;
						DEC(ps);
						toDouble(ps);
						if (ps->d < d || isnan(d))
							ps->d = d;
					}
				} else {
					/* all args are string */
					while (--nargs) {
						ps1 = ps;
						DEC(ps);
						if (strcmp(ps->s, ps1->s) < 0) {
							strcpy(ps->s, ps1->s);
						}
					}
				}
				break;

			case MIN:
				nargs = *post++;
				for(i=0, j=0; i<nargs; j |= isDouble(ps-i), i++);
				if (j) {
					/* an arg is double.  coerce all to double */
					toDouble(ps);
					while (--nargs) {
						d = ps->d;
						DEC(ps);
						toDouble(ps);
						if (ps->d > d || isnan(d))
							ps->d = d;
					}
				} else {
					/* all args are string */
					while (--nargs) {
						ps1 = ps;
						DEC(ps);
						if (strcmp(ps->s, ps1->s) > 0) {
							strcpy(ps->s, ps1->s);
						}
					}
				}
				break;
 
 			case UNTIL:
				if (sCalcPerformDebug) printf("\tUNTIL:ps->d=%f\n", ps->d);
				if (sCalcPerformDebug) printf("\tpost-1=%p\n", post-1);
				for (i=0; i<10; i++) {
					/* find ourselves in post, remembering that post was incremented at loop top */
					if (sCalcPerformDebug > 10) printf("\tuntil_scratch[i].until_loc=%p\n", until_scratch[i].until_loc);
					if (until_scratch[i].until_loc == post-1) {
						until_scratch[i].ps = ps;
						break;
					}
				}
				if (i==10) {
					printf("sCalcPerform: UNTIL not found\n");
					return(-1);
				}
				break;

			case UNTIL_END:
				if (sCalcPerformDebug) printf("\tUNTIL_END:ps->d=%f\n", ps->d);
				if (++loopsDone > sCalcLoopMax)
					break;
				if (ps->d==0) {
					/* reset postfix to matching UNTIL code, stack to its loc at that time */
					--post;
					for (i=0; i<10; i++) {
						if (until_scratch[i].until_end_loc == post) {
							ps = until_scratch[i].ps;
							post = until_scratch[i].until_loc;
							if (sCalcPerformDebug) printf("--loop--\n");
							break;
						}
					}
					if (i==10) {
						printf("sCalcPerform: UNTIL not found\n");
						return(-1);
					}
					break;
				}
				break;

			default:
				break;
			}
		}

		/* if everything is peachy,the stack should end at its first position */
		if (ps != top) {
#if DEBUG
			if (sCalcPerformDebug>=10) {
				printf("sCalcPerform: stack error,ps=%p,top=%p\n", ps, top);
				printf("sCalcPerform: ps->d=%f\n", ps->d);
			}
#endif
			return(-1);
		}
	
		if (isDouble(ps)) {
			if (presult) *presult = ps->d;
			if (psresult) {
				to_string(ps);
				s = ps->s;
				for (i=0, s1=psresult; *s && i<(lenSresult-1); i++)
					*s1++ = *s++;
				*s1 = 0;
			}
		} else {
			if (psresult) {
				s = ps->s;
				for (i=0, s1=psresult; *s && i<(lenSresult-1); i++)
					*s1++ = *s++;
				*s1 = 0;
			}
			if (presult) {
				to_double(ps);
				*presult = ps->d;
			}
		}

		return(((isnan(*presult)||isinf(*presult)) ? -1 : 0));
	} /* if (*post++ != USES_STRING) {} else */
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
extern double simple_random(void);
static double local_random()
{
	double  randy;

	/* random number */
	seed = (seed * multy) + addy;
	/* randy = (float) seed / 65535.0; */
	randy = (float) (seed+1) / 65536.0;	/* exclude zero */
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

	if (sCalcPerformDebug) {
		printf("cond_search:entry:\n");
		sCalcExprDump(pinst);
		printf("\t-----\n");
	}
	while ((op = *pinst++) != END_EXPRESSION) {
		if (op == match && --count == 0) {
			if (sCalcPerformDebug) {
				printf("cond_search:exit:\n");
				sCalcExprDump(pinst);
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
		case LITERAL_STRING:
			pinst += strlen((char *)pinst)+1;
			if (0 && sCalcPerformDebug) {
				printf("cond_search:LITERAL_STRING:\n");
				sCalcExprDump(pinst);
				printf("\t-----\n");
			}
			break;
		case MIN:
		case MAX:
		case FINITE:
		case ISNAN:
			pinst++;
			break;
		case COND_IF:
			count++;
			break;
		}
	}
	return 1;
}
