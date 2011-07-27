/* sCalcPerform.c,v 1.7 2004/08/30 19:03:40 mooney Exp */
/*
 *	Author: Julie Sander and Bob Dalesio
 *	Date:	07-27-87
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * 03-18-98 tmm Essentially rewritten to support string operators
 * 05-21-98 tmm fixed ?: operator
 * 11-12-98 tmm v4.3 added array operators @, @@ 
 * 10-27-01 mlr added BYTE operator
 * 05-13-03 tmm extensive modifications to improve performance, decrease
 *              threat of memory fragmentation, and add new operator type.
 *              Fixed-size strings are allocated in place on the value
 *              stack, rather than continually being allocated and freed.
 *              Added FETCH_A..FETCH_L symbols, in addition to FETCH followed
 *              by an argument.  Variable-argument operators (MAX, MIN) put
 *              NaN on the value stack to indicate end of args.  Use isnan
 *              from EPICS base.  Added checkXxxElement(), which compile to
 *              nothing if DEBUG=0.
 * 03-03-06 tmm Added TR_ESC function, which applies dbTranslateEscape() to
 *              its argument, and ESC function, which applies
 *              epicsStrSnPrintEscaped() to its argument.
 * 10-23-06 tmm Added CRC16 and MODBUS functions, calculate modbus 16-bit CRC
 *              from string, and either return it, or append it.
 * 10-24-06 tmm Added LRC, AMODBUS, XOR8 and ADD_XOR8 functions
 */

/* This module contains the code for processing the arithmetic
 * expressions defined in calculation records. postfix must be called
 * to convert a valid infix expression to postfix. sCalcPerform
 * calculates the postfix expression.
 *
 * Subroutines
 *
 *	Public
 *
 * sCalcPerform		perform the calculation
 *  args
 *    double *parg       address of arguments
 *    int    numArgs     number of arguments in pargs array
 *    double *psarg      address of string arguments
 *    int    numSArgs    number of string arguments in psargs array
 *    double *presult    address of double result
 *    char   *psresult   address of string-result buffer
 *    int    lenSresult  length of string-result buffer
 *    char   *rpcl       address of postfix buffer
 *  returns
 *    0   fetched successfully
 *    -1  fetch failed
 *
 * Private routine for sCalcPerform
 * local_random  random number generator
 *  returns
 *    double value between 0.00 and 1.00
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
#include	"cvtFast.h"
#include	"epicsString.h"
#include	"epicsStdio.h"	/* for  epicsSnprintf() */

#define epicsExportSharedSymbols
#include	"sCalcPostfix.h"
#include	"sCalcPostfixPvt.h"
#include <epicsExport.h>

static double	local_random();
extern void getOpString(char code, char* opString);

#define myNINT(a) ((int)((a) >= 0 ? (a)+0.5 : (a)-0.5))
#ifndef PI
#define PI 3.141592654
#endif
#define MAX(a,b) (a)>(b)?(a):(b)
#define MIN(a,b) (a)<(b)?(a):(b)

#define OVERRIDESTDCALC 0
#define DEBUG 1
volatile int sCalcPerformDebug = 0;
epicsExportAddress(int, sCalcPerformDebug);

#if DEBUG
int sCalcStackHW = 0;	/* high-water mark */
int sCalcStackLW = 0;	/* low-water mark */
#define INC(ps) {if ((int)(++(ps)-top) > sCalcStackHW) sCalcStackHW = (int)((ps)-top);}
#define DEC(ps) {if ((int)(--(ps)-top) < sCalcStackLW) sCalcStackLW = (int)((ps)-top);}
#define checkDoubleElement(pd,op) {if (isnan(*(pd))) printf("sCalcPerform: unexpected NaN in op %d\n", (op));}
#define checkStackElement(ps,op) {if (((ps)->s == NULL) && isnan((ps)->d)) printf("sCalcPerform: unexpected NaN in op %d\n", (op));}
#else
#define INC(ps) ++ps
#define DEC(ps) ps--
#define checkDoubleElement(pd,op)
#define checkStackElement(ps,op)
#endif

/* strncpy sucks (may copy extra characters, may not null-terminate) */
#define strNcpy(dest, src, N) {			\
	int ii;								\
	char *dd=dest, *ss=src;				\
	for (ii=0; *ss && ii < N-1; ii++)	\
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

int lrc(char *output, char *rawInput)
{
	int i, len;
	unsigned int lrc;
	unsigned char tranInput[100];

	len = dbTranslateEscape((char *)tranInput, rawInput);
	if (len == 0) return(-1);
	if (sCalcPerformDebug>=5) {
		printf("input string(len=%d): ", len);
		for (i=0; i<len; i++) printf("0x%02x ", tranInput[i]);
		printf("\n");
	}
	lrc = 0;
	/* Loop through the translated input string */
	for (i=0; i<len; i++) {
		lrc += (unsigned int)tranInput[i];
	}
	lrc = -lrc;
	if (sCalcPerformDebug>=10) printf("lrc=0x%04x\n", lrc);
	/* put the LRC into the output string, escaped */
	sprintf(output, "\\x%02x", lrc&0xff);
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


#if OVERRIDESTDCALC
/* Override standard EPICS expression evaluator (if we're loaded after it) */
long epicsShareAPI 
	calcPerform(double *parg, double *presult, char *post)
{
	return(sCalcPerform(parg, 12, NULL, 0, presult, NULL, 0, post));
}
#endif

#define TMPSTR_SIZE 1000
long epicsShareAPI 
	sCalcPerform(double *parg, int numArgs, char **psarg, int numSArgs, double *presult, char *psresult, int lenSresult, char *post)
{
	struct stackElement stack[STACKSIZE], *top;
	struct stackElement *ps, *ps1, *ps2;
	char				*s2, tmpstr[TMPSTR_SIZE], currSymbol;
	char				*s, *s1, c;
	int					i, j, k;
	long				l;
	unsigned short		ui;
	unsigned long		ul;
	float				f;
	double				d;
	double				*topd, *pd;
	short 				h, got_if, cvt_to_double;
	DOUBLE64_2LONG32	*pu;

#if DEBUG
	if (sCalcPerformDebug>=10) {
		int	more;
		printf("sCalcPerform: postfix:");
		s = post;
		for (more=1; more;) {
			if (*s >= FETCH_A && *s <= FETCH_L) {
				printf("%c ", 'a' + (*s-FETCH_A));
				s++;
			} else {
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
				case SLITERAL:
					s++; /* point past code */
					printf("'");
					while (*s) printf("%c", *s++);
					printf("' ");
					s++;
					break;
				case FETCH:
					s++; /* point past code */
					printf("@%d ", *s++);
					break;
				case SFETCH:
					s++; /* point past code */
					printf("$%d ", *s++);
					break;
				default:
					getOpString(*s, tmpstr);
					printf("%s ", tmpstr);
					if (*s == BAD_EXPRESSION) more=0;
					s++;
					break;
				}
			}
		}

		printf("\nsCalcPerform: args:\n");
		for (i=0; i<12; i++) {
			if (i%4 == 0) printf("     ");
			printf("%c=%f\t", 'a'+i, parg[i]);
			if (i%4 == 3) printf("\n");
		}
	}
#endif

	if (*post == BAD_EXPRESSION) return(-1);


	if (*post++ != USES_STRING) {

		topd = pd = (double *)&stack[10];
		pd--;

		/* No string expressions */
		while (*post != END_STACK) {

			switch (*post){

			case FETCH:
				++pd;
				++post;
				*pd = (*post < numArgs) ? parg[(int)*post] : 0;
				break;

			case FETCH_A: *(++pd) = parg[0]; break;
			case FETCH_B: *(++pd) = parg[1]; break;
			case FETCH_C: *(++pd) = parg[2]; break;
			case FETCH_D: *(++pd) = parg[3]; break;
			case FETCH_E: *(++pd) = parg[4]; break;
			case FETCH_F: *(++pd) = parg[5]; break;
			case FETCH_G: *(++pd) = parg[6]; break;
			case FETCH_H: *(++pd) = parg[7]; break;
			case FETCH_I: *(++pd) = parg[8]; break;
			case FETCH_J: *(++pd) = parg[9]; break;
			case FETCH_K: *(++pd) = parg[10]; break;
			case FETCH_L: *(++pd) = parg[11]; break;

			case STORE:
				/* not implemented */
				return(-1);

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
				checkDoubleElement(pd, *post);
				--pd;
				checkDoubleElement(pd, *post);
				*pd = *pd + pd[1];
				break;

			case SUB:
				checkDoubleElement(pd, *post);
				--pd;
				checkDoubleElement(pd, *post);
				*pd = *pd - pd[1];
				break;

			case MULT:
				checkDoubleElement(pd, *post);
				--pd;
				checkDoubleElement(pd, *post);
				*pd = *pd * pd[1];
				break;

			case DIV:
				checkDoubleElement(pd, *post);
				--pd;
				checkDoubleElement(pd, *post);
				if (pd[1] == 0) /* can't divide by zero */
					return(-1);
				*pd = *pd / pd[1];
				break;

			case COND_IF:
				/* if false condition then skip true expression */
				checkDoubleElement(pd, *post);
				if (*pd == 0.0) {
					/* skip to matching COND_ELSE */
					for (got_if=1; got_if>0 && post[1] != END_STACK; ++post) {
						switch(post[1]) {
						case LITERAL: post+=8; break;
						case COND_IF: got_if++; break;
						case COND_ELSE: got_if--; break;
						case FETCH: post++; break;
						}
					}
					if (got_if) {
#if DEBUG
						if (sCalcPerformDebug)
							printf("sCalcPerform: '?' without matching ':'\n");
#endif
						return(-1);
					}
				}
				/* remove condition from stack top */
				--pd;
				break;
				
			case COND_ELSE:
				/* result, true condition is on stack so skip false condition  */
				/* skip to matching COND_END */
				for (got_if=1; got_if>0 && post[1] != END_STACK; ++post) {
					switch(post[1]) {
					case LITERAL: post+=8; break;
					case COND_IF: got_if++; break;
					case COND_END: got_if--; break;
					case FETCH: post++; break;
					}
				}
				break;

			case COND_END:
				break;

			case ABS_VAL:
				checkDoubleElement(pd, *post);
				if (*pd < 0 ) *pd *= -1;
				break;

			case UNARY_NEG:
				checkDoubleElement(pd, *post);
				*pd *= -1;
				break;

			case SQU_RT:
				/* check for neg number */
				checkDoubleElement(pd, *post);
				if (*pd < 0) return(-1);	
				*pd = sqrt(*pd);
				break;

			case EXP:
				checkDoubleElement(pd, *post);
				*pd = exp(*pd);
				break;

			case LOG_10:
				/* check for neg number */
				checkDoubleElement(pd, *post);
				if (*pd < 0) return(-1);
				*pd = log10(*pd);
				break;

			case LOG_E:
				/* check for neg number */
				checkDoubleElement(pd, *post);
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

			case EXPON:
				checkDoubleElement(pd, *post);
				--pd;
				checkDoubleElement(pd, *post);
				if (*pd == 0) break;
				if (*pd < 0) {
					i = (int) pd[1];
					/* is exponent an integer? */
					if ((pd[1] - (double)i) != 0) return (-1);
        			*pd = exp(pd[1] * log(-(*pd)));
					/* is value negative */
					if ((i % 2) > 0) *pd = -(*pd);
				} else {
	        		*pd = exp(pd[1] * log(*pd));
				}
				break;

			case MODULO:
				checkDoubleElement(pd, *post);
				--pd;
				checkDoubleElement(pd, *post);
				if ((int)(pd[1]) == 0)
					return(-1);
				*pd = (double)((int)(*pd) % (int)(pd[1]));
				break;

			case REL_OR:
				checkDoubleElement(pd, *post);
				--pd;
				checkDoubleElement(pd, *post);
				*pd = *pd || pd[1];
				break;

			case REL_AND:
				checkDoubleElement(pd, *post);
				--pd;
				checkDoubleElement(pd, *post);
				*pd = *pd && pd[1];
				break;

			case BIT_OR:
				/* force double values into integers and or them */
				checkDoubleElement(pd, *post);
				--pd;
				checkDoubleElement(pd, *post);
				*pd = (int)(pd[1]) | (int)(*pd);
				break;

			case BIT_AND:
				/* force double values into integers and and them */
				checkDoubleElement(pd, *post);
				--pd;
				checkDoubleElement(pd, *post);
				*pd = (int)(pd[1]) & (int)(*pd);
				break;

			case BIT_EXCL_OR:
				/* force double values to integers to exclusive or them */
				checkDoubleElement(pd, *post);
				--pd;
				checkDoubleElement(pd, *post);
				*pd = (int)(pd[1]) ^ (int)(*pd);
				break;

			case GR_OR_EQ:
				checkDoubleElement(pd, *post);
				--pd;
				checkDoubleElement(pd, *post);
				*pd = *pd >= pd[1];
				break;

			case GR_THAN:
				checkDoubleElement(pd, *post);
				--pd;
				checkDoubleElement(pd, *post);
				*pd = *pd > pd[1];
				break;

			case LESS_OR_EQ:
				checkDoubleElement(pd, *post);
				--pd;
				checkDoubleElement(pd, *post);
				*pd = *pd <= pd[1];
				break;

			case LESS_THAN:
				checkDoubleElement(pd, *post);
				--pd;
				checkDoubleElement(pd, *post);
				*pd = *pd < pd[1];
				break;

			case NOT_EQ:
				checkDoubleElement(pd, *post);
				--pd;
				checkDoubleElement(pd, *post);
				*pd = *pd != pd[1];
				break;

			case EQUAL:
				checkDoubleElement(pd, *post);
				--pd;
				checkDoubleElement(pd, *post);
				*pd = *pd == pd[1];
				break;

			case RIGHT_SHIFT:
				checkDoubleElement(pd, *post);
				--pd;
				checkDoubleElement(pd, *post);
				*pd = (int)(*pd) >> (int)(pd[1]);
				break;

			case LEFT_SHIFT:
				checkDoubleElement(pd, *post);
				--pd;
				checkDoubleElement(pd, *post);
				*pd = (int)(*pd) << (int)(pd[1]);
				break;

			case MAX_VAL:
				checkDoubleElement(pd, *post);
				--pd;
				checkDoubleElement(pd, *post);
				if (*pd < pd[1]) *pd = pd[1];
				break;
 
			case MIN_VAL:
				checkDoubleElement(pd, *post);
				--pd;
				checkDoubleElement(pd, *post);
				if (*pd > pd[1]) *pd = pd[1];
				break;

			case ACOS:
				checkDoubleElement(pd, *post);
				*pd = acos(*pd);
				break;

			case ASIN:
				checkDoubleElement(pd, *post);
				*pd = asin(*pd);
				break;

			case ATAN:
				checkDoubleElement(pd, *post);
				*pd = atan(*pd);
				break;

	 		case ATAN2:
				checkDoubleElement(pd, *post);
				--pd;
				checkDoubleElement(pd, *post);
	 			*pd = atan2(pd[1], *pd);
	 			break;

			case COS:
				checkDoubleElement(pd, *post);
				*pd = cos(*pd);
				break;

			case SIN:
				checkDoubleElement(pd, *post);
				*pd = sin(*pd);
				break;

			case TAN:
				checkDoubleElement(pd, *post);
				*pd = tan(*pd);
				break;

			case COSH:
				checkDoubleElement(pd, *post);
				*pd = cosh(*pd);
				break;

			case SINH:
				checkDoubleElement(pd, *post);
				*pd = sinh(*pd);
				break;

			case TANH:
				checkDoubleElement(pd, *post);
				*pd = tanh(*pd);
				break;

			case CEIL:
				checkDoubleElement(pd, *post);
				*pd = ceil(*pd);
				break;

			case FLOOR:
				checkDoubleElement(pd, *post);
				*pd = floor(*pd);
				break;

			case NINT:
				checkDoubleElement(pd, *post);
				d = *pd;
				*pd = (double)(long)(d >= 0 ? d+0.5 : d-0.5);
				break;

			case REL_NOT:
				checkDoubleElement(pd, *post);
				*pd = (*pd ? 0 : 1);
				break;

			case BIT_NOT:
				checkDoubleElement(pd, *post);
				*pd = ~(int)(*pd);
				break;

			case A_FETCH:
				checkDoubleElement(pd, *post);
				d = *pd;
				i = (int)(d >= 0 ? d+0.5 : 0);
				*pd = (i < numArgs) ? parg[i] : 0;
				break;

			case LITERAL:
				++pd;
				++post;
				if (post == NULL) {
					++post;
					printf("%.7s bad constant in expression\n", post);
					*pd = 0.;
					break;
				}
				memcpy((void *)&(*pd),post,8);
				post += 7;
				break;

			case MAXV_VAL:
#if DEBUG
				if (sCalcPerformDebug>=10) {
					printf("val stack:");
					for (i=0; topd+i <= pd; i++) printf("%f ", topd[i]);
					printf("\n");
				}
#endif
				checkDoubleElement(pd, *post);
				--pd;
				checkDoubleElement(pd, *post);
				for (i=0; i<MAX_VARGS && !isnan(*pd); --pd) {
					if (*pd < pd[1]) *pd = pd[1];
				}
				if (i == MAX_VARGS) {
					printf("sCalcPerform: too many arguments to MAX\n");
					return(-1);
				}
				*pd = pd[1];	/* replace NaN marker with result */
				break;
 
			case MINV_VAL:
				checkDoubleElement(pd, *post);
				--pd;
				checkDoubleElement(pd, *post);
				for (i=0; i<MAX_VARGS && !isnan(*pd); --pd) {
					if (*pd > pd[1]) *pd = pd[1];
				}
				if (i == MAX_VARGS) {
					printf("sCalcPerform: too many arguments to MAX\n");
					return(-1);
				}
				*pd = pd[1];	/* replace NaN marker with result */
				break;

			case VARG_TERM:
				/* put a NaN on the value stack to mark the end of arguments */
				++pd;
				pu = (DOUBLE64_2LONG32 *)pd;
				pu->l[0] = pu->l[1] = 0xffffffff;
				break;

			default:
				break;
			}

			/* move ahead in postfix expression */
			++post;
		}

		/* if everything is peachy,the stack should end at its first position */
		if (pd != topd) {
#if DEBUG
			if (sCalcPerformDebug>=10) printf("sCalcPerform: stack error\n");
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

		top = ps = &stack[10];
		ps--;  /* Expression handler assumes ps is pointing to a filled element */
		ps->d = 1.23456; ps->s = NULL;	/* telltale */

		/* string expressions and values handled */
		while (*post != END_STACK) {

			currSymbol = *post;
			switch (currSymbol) {

			case FETCH:
				INC(ps);
				++post;
				ps->s = NULL;
				ps->d = (*post < numArgs) ? parg[(int)*post] : 0;
				break;

			case FETCH_A: INC(ps); ps->s = NULL; ps->d = parg[0]; break;
			case FETCH_B: INC(ps); ps->s = NULL; ps->d = parg[1]; break;
			case FETCH_C: INC(ps); ps->s = NULL; ps->d = parg[2]; break;
			case FETCH_D: INC(ps); ps->s = NULL; ps->d = parg[3]; break;
			case FETCH_E: INC(ps); ps->s = NULL; ps->d = parg[4]; break;
			case FETCH_F: INC(ps); ps->s = NULL; ps->d = parg[5]; break;
			case FETCH_G: INC(ps); ps->s = NULL; ps->d = parg[6]; break;
			case FETCH_H: INC(ps); ps->s = NULL; ps->d = parg[7]; break;
			case FETCH_I: INC(ps); ps->s = NULL; ps->d = parg[8]; break;
			case FETCH_J: INC(ps); ps->s = NULL; ps->d = parg[9]; break;
			case FETCH_K: INC(ps); ps->s = NULL; ps->d = parg[10]; break;
			case FETCH_L: INC(ps); ps->s = NULL; ps->d = parg[11]; break;

			case SFETCH:	/* fetch from string variable */
				INC(ps);
				++post;
				ps->s = &(ps->local_string[0]);
				ps->s[0] = '\0';
				if (*post < numSArgs) strncpy(ps->s, psarg[(int)*post], LOCAL_STRING_SIZE);
				break;

			case STORE:
				/* not implemented */
				return(-1);

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
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
				if (isDouble(ps)) {
					toDouble(ps1);
					ps->d = ps->d + ps1->d;
				} else if (isDouble(ps1)) {
					to_double(ps);
					ps->d = ps->d + ps1->d;
				} else {
					/* concatenate two strings */
					strncat(ps->s, ps1->s, strlen(ps->s)-LOCAL_STRING_SIZE-1);
				}
				break;

			case SUB:
			case SUBLAST:
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
				if (isDouble(ps)) {
					toDouble(ps1);
					ps->d = ps->d - ps1->d;
				} else if (isDouble(ps1)) {
					to_double(ps);
					ps->d = ps->d - ps1->d;
				} else {
					/* subtract ps1->s from ps->s */
					if (ps1->s[0]) {
						if (currSymbol == SUB) {
							/* first occurrence of ps1->s */
							s = strstr(ps->s, ps1->s);
						} else {
							/* last occurrence of ps1->s */
							s1 = ps->s+strlen(ps->s)-strlen(ps1->s);
							for (s = NULL; s == NULL && s1 > ps->s; s1--) {
								if (strncmp(s1, ps1->s, strlen(ps1->s))) {
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
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
				toDouble(ps1);
				toDouble(ps);
				ps->d = ps->d * ps1->d;
				break;

			case DIV:
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
				toDouble(ps1);
				toDouble(ps);
				if (ps1->d == 0) /* can't divide by zero */
					return(-1);
				ps->d = ps->d / ps1->d;
				break;

			case COND_IF:
				/* if false condition then skip true expression */
				checkStackElement(ps, *post);
				toDouble(ps);
				if (ps->d == 0.0) {
					/* skip to matching COND_ELSE */
					for (got_if=1; got_if>0 && post[1] != END_STACK; ++post) {
						switch(post[1]) {
						case LITERAL:	post+=8; break;
						case SLITERAL:	post++; while (post[1]) post++; break;
						case COND_IF:	got_if++; break;
						case COND_ELSE: got_if--; break;
						case FETCH: case SFETCH: post++; break;
						}
					}
					if (got_if) {
#if DEBUG
						if (sCalcPerformDebug)
							printf("sCalcPerform: '?' without matching ':'\n");
#endif
						return(-1);
					}
				}
				/* remove condition from stack top */
				DEC(ps);
				break;
				
			case COND_ELSE:
				/* result, true condition is on stack so skip false condition  */
				/* skip to matching COND_END */
				for (got_if=1; got_if>0 && post[1] != END_STACK; ++post) {
					switch(post[1]) {
					case LITERAL:	post+=8; break;
					case SLITERAL:	post++; while (post[1]) post++; break;
					case COND_IF:	got_if++; break;
					case COND_END:	got_if--; break;
					case FETCH: case SFETCH: post++; break;
					}
				}
				break;

			case COND_END:
				break;

			case ABS_VAL:
				checkStackElement(ps, *post);
				toDouble(ps);
				if (ps->d < 0 ) ps->d *= -1;
				break;

			case UNARY_NEG:
				checkStackElement(ps, *post);
				toDouble(ps);
				ps->d *= -1;
				break;

			case SQU_RT:
				checkStackElement(ps, *post);
				toDouble(ps);
				/* check for neg number */
				if (ps->d < 0) return(-1);	
				ps->d = sqrt(ps->d);
				break;

			case EXP:
				checkStackElement(ps, *post);
				toDouble(ps);
				ps->d = exp(ps->d);
				break;

			case LOG_10:
				checkStackElement(ps, *post);
				toDouble(ps);
				/* check for neg number */
				if (ps->d < 0) return(-1);
				ps->d = log10(ps->d);
				break;

			case LOG_E:
				checkStackElement(ps, *post);
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

			case EXPON:
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
				toDouble(ps1);
				toDouble(ps);
				if (ps->d == 0) break;
				if (ps->d < 0) {
					i = (int) ps1->d;
					/* is exponent an integer? */
					if ((ps1->d - (double)i) != 0) return (-1);
        			ps->d = exp(ps1->d * log(-(ps->d)));
					/* is value negative */
					if ((i % 2) > 0) ps->d = -ps->d;
				} else {
					ps->d = exp(ps1->d * log(ps->d));
				}
				break;

			case MODULO:
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
				toDouble(ps1);
				toDouble(ps);
				if ((int)ps1->d == 0)
					return(-1);
				ps->d = (double)((int)ps->d % (int)ps1->d);
				break;

			case REL_OR:
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
				toDouble(ps1);
				toDouble(ps);
				ps->d = ps->d || ps1->d;
				break;

			case REL_AND:
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
				toDouble(ps1);
				toDouble(ps);
				ps->d = ps->d && ps1->d;
				break;

			case BIT_OR:
				/* force double values into integers and or them */
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
				toDouble(ps1);
				toDouble(ps);
				ps->d = (int)(ps1->d) | (int)(ps->d);
				break;

			case BIT_AND:
				/* force double values into integers and and them */
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
				toDouble(ps1);
				toDouble(ps);
				ps->d = (int)(ps1->d) & (int)(ps->d);
				break;

			case BIT_EXCL_OR:
				/* force double values to integers to exclusive or them */
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
				toDouble(ps1);
				toDouble(ps);
				ps->d = (int)(ps1->d) ^ (int)(ps->d);
				break;

			case GR_OR_EQ:
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
				if (isDouble(ps)) {
					toDouble(ps1);
					ps->d = ps->d >= ps1->d;
				} else if (isDouble(ps1)) {
					to_double(ps);
					ps->d = ps->d >= ps1->d;
				} else {
					/* compare ps->s to ps1->s */
					ps->d = (double)(strcmp(ps->s, ps1->s) >= 0);
					ps->s = NULL;
				}
				break;

			case GR_THAN:
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
				if (isDouble(ps)) {
					toDouble(ps1);
					ps->d = ps->d > ps1->d;
				} else if (isDouble(ps1)) {
					to_double(ps);
					ps->d = ps->d > ps1->d;
				} else {
					/* compare ps->s to ps1->s */
					ps->d = (double)(strcmp(ps->s, ps1->s) > 0);
					ps->s = NULL;
				}
				break;

			case LESS_OR_EQ:
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
				if (isDouble(ps)) {
					toDouble(ps1);
					ps->d = ps->d <= ps1->d;
				} else if (isDouble(ps1)) {
					to_double(ps);
					ps->d = ps->d <= ps1->d;
				} else {
					/* compare ps->s to ps1->s */
					ps->d = (double)(strcmp(ps->s, ps1->s) <= 0);
					ps->s = NULL;
				}
				break;

			case LESS_THAN:
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
				if (isDouble(ps)) {
					toDouble(ps1);
					ps->d = ps->d < ps1->d;
				} else if (isDouble(ps1)) {
					to_double(ps);
					ps->d = ps->d < ps1->d;
				} else {
					/* compare ps->s to ps1->s */
					ps->d = (double)(strcmp(ps->s, ps1->s) < 0);
					ps->s = NULL;
				}
				break;

			case NOT_EQ:
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
				if (isDouble(ps)) {
					toDouble(ps1);
					ps->d = ps->d != ps1->d;
				} else if (isDouble(ps1)) {
					to_double(ps);
					ps->d = ps->d != ps1->d;
				} else {
					/* compare ps->s to ps1->s */
					ps->d = (double)(strcmp(ps->s, ps1->s) != 0);
					ps->s = NULL;
				}
				break;

			case EQUAL:
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
				if (isDouble(ps)) {
					toDouble(ps1);
					ps->d = ps->d == ps1->d;
				} else if (isDouble(ps1)) {
					to_double(ps);
					ps->d = ps->d == ps1->d;
				} else if ((isString(ps)) && (isString(ps1))) {
					/* compare ps->s to ps1->s */
					ps->d = (double)(strcmp(ps->s, ps1->s) == 0);
					ps->s = NULL;
				}
				break;

			case RIGHT_SHIFT:
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
				toDouble(ps1);
				toDouble(ps);
				ps->d = (int)(ps->d) >> (int)(ps1->d);
				break;

			case LEFT_SHIFT:
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
				toDouble(ps1);
				toDouble(ps);
				ps->d = (int)(ps->d) << (int)(ps1->d);
				break;

			case MAX_VAL:
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
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
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
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
				checkStackElement(ps, *post);
				toDouble(ps);
				ps->d = acos(ps->d);
				break;

			case ASIN:
				checkStackElement(ps, *post);
				toDouble(ps);
				ps->d = asin(ps->d);
				break;

			case ATAN:
				checkStackElement(ps, *post);
				toDouble(ps);
				ps->d = atan(ps->d);
				break;

	 		case ATAN2:
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
				toDouble(ps1);
				toDouble(ps);
	 			ps->d = atan2(ps1->d, ps->d);
	 			break;

			case COS:
				checkStackElement(ps, *post);
				toDouble(ps);
				ps->d = cos(ps->d);
				break;

			case SIN:
				checkStackElement(ps, *post);
				toDouble(ps);
				ps->d = sin(ps->d);
				break;

			case TAN:
				checkStackElement(ps, *post);
				toDouble(ps);
				ps->d = tan(ps->d);
				break;

			case COSH:
				checkStackElement(ps, *post);
				toDouble(ps);
				ps->d = cosh(ps->d);
				break;

			case SINH:
				checkStackElement(ps, *post);
				toDouble(ps);
				ps->d = sinh(ps->d);
				break;

			case TANH:
				checkStackElement(ps, *post);
				toDouble(ps);
				ps->d = tanh(ps->d);
				break;

			case CEIL:
				checkStackElement(ps, *post);
				toDouble(ps);
				ps->d = ceil(ps->d);
				break;

			case FLOOR:
				checkStackElement(ps, *post);
				toDouble(ps);
				ps->d = floor(ps->d);
				break;

			case NINT:
				checkStackElement(ps, *post);
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
				checkStackElement(ps, *post);
				toDouble(ps);
				ps->d = (ps->d ? 0 : 1);
				break;

			case BIT_NOT:
				checkStackElement(ps, *post);
				toDouble(ps);
				ps->d = ~(int)(ps->d);
				break;

			case A_FETCH:
				checkStackElement(ps, *post);
				if (isDouble(ps)) {
					d = ps->d;
				} else {
					d = atof(ps->s);
					ps->s = NULL;
				}
				i = (int)(d >= 0 ? d+0.5 : 0);
				ps->d = (i < numArgs) ? parg[i] : 0;
				break;

			case A_SFETCH:
				checkStackElement(ps, *post);
				if (isDouble(ps)) {
					d = ps->d;
				} else {
					d = atof(ps->s);
				}
				ps->s = &(ps->local_string[0]);
				ps->s[0] = '\0';
				i = (int)(d >= 0 ? d+0.5 : 0);
				if (i < numSArgs)
					strNcpy(ps->s, psarg[i], LOCAL_STRING_SIZE);
				break;

			case LITERAL:
				INC(ps);
				++post;
				if (post == NULL) {
					++post;
					printf("%.7s bad constant in expression\n",post);
					ps->s = NULL;
					ps->d = 0.;
					break;
				}
				memcpy((void *)&(ps->d),post,8);
				ps->s = NULL;
				post += 7;
				break;

			case SLITERAL:
				INC(ps);
				++post;
				if (post == NULL) {
					++post;
					printf("%.7s bad constant in expression\n",post);
					ps->s = NULL;
					ps->d = 0.;
					break;
				}
				ps->s = &(ps->local_string[0]);
				s = ps->s;
				for (i=0; (i<LOCAL_STRING_SIZE-1) && *post; )
					*s++ = *post++;
				*s = '\0';
				/* skip to end of string */
				while (*post) post++;
				break;

			case TO_DOUBLE:
				checkStackElement(ps, *post);
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
				checkStackElement(ps, *post);
				toString(ps);
				break;

			case LEN:
				checkStackElement(ps, *post);
				toString(ps);
				for (i=0; (i < LOCAL_STRING_SIZE) && ps->s[i]; i++)
					;
				ps->d = (double)i;
				ps->s = NULL;
				break;

			case BYTE:
				checkStackElement(ps, *post);
				if (isString(ps)) {
					ps->d = ps->s[0];
					ps->s = NULL;
				}
				break;

	 		case PRINTF:
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
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
				strNcpy(ps->s, tmpstr, LOCAL_STRING_SIZE-1);
				break;

	 		case BIN_WRITE:
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
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
		 		i = epicsStrSnPrintEscaped(tmpstr, LOCAL_STRING_SIZE-1, ps->s, j);
				i = MIN(i, LOCAL_STRING_SIZE);
				tmpstr[i] = '\0'; /* make sure it's terminated */
				strNcpy(ps->s, tmpstr, LOCAL_STRING_SIZE-1);
				break;

	 		case SSCANF:
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
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
		 				i = sscanf(ps->s, ps1->s, &h);
						ps->d = (double)h;
					} else {
		 				i = sscanf(ps->s, ps1->s, &l);
						ps->d = (double)l;
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
					strNcpy(ps->s, tmpstr, LOCAL_STRING_SIZE-1);
					break;
				}
				if (i != 1) {
					/* sscanf error */
					return(-1); 
				}
				break;

	 		case BIN_READ:
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
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
				checkStackElement(ps, *post);
				if (isString(ps)) {
		 			i = dbTranslateEscape(tmpstr, ps->s);
#if 0 /* No use doing this, since no function reads past end-of-string */
					if (i > LOCAL_STRING_SIZE-1) i = LOCAL_STRING_SIZE-1;
					memcpy(ps->s, tmpstr, i);
					ps->s[i] = '\0';
#else
					strNcpy(ps->s, tmpstr, LOCAL_STRING_SIZE-1);
#endif
				}
				break;

			case ESC:
				checkStackElement(ps, *post);
				if (isString(ps)) {
					/* Find length of input string.  Null terminates string.*/
					for (j=0; j<LOCAL_STRING_SIZE && ps->s[j] != '\0'; j++) {
						;
					}
		 			i = epicsStrSnPrintEscaped(tmpstr, LOCAL_STRING_SIZE-1, ps->s, j);
					i = MIN(i, LOCAL_STRING_SIZE);
					tmpstr[i] = '\0'; /* make sure it's terminated */
					strNcpy(ps->s, tmpstr, LOCAL_STRING_SIZE-1);
				}
				break;

			case CRC16:
			case MODBUS:
				checkStackElement(ps, *post);
				if (isString(ps)) {
		 			if (crc16(tmpstr, ps->s) == 0) {
						if (currSymbol==CRC16) {
							strNcpy(ps->s, tmpstr, LOCAL_STRING_SIZE-1);
						} else {
							strncat(ps->s, tmpstr, strlen(ps->s)-LOCAL_STRING_SIZE-1);
						}
					}
				}
				break;

			case LRC:
			case AMODBUS:
				/* Note that this isn't complete:  Ascii modbus has to have its LRC
				 * computed while the command string is still binary, then the
				 * (binary) LRC is appended, then ':' is prepended and <cr><lf> is appended
				 * Example: To send the binary command string "F7 03 13 89 00 0A" (hex,
				 * obviously), you calculate the LRC (0x60), append to the command string,
				 * and convert hex character by character to ASCII, yielding
				 * "46 37 30 33 31 33 38 39 30 30 30 41 36 30"
				 * then you prepend ':' (0x3a) and append <cr><lf> (0x0d0a), yielding
				 * "3A 46 37 30 33 31 33 38 39 30 30 30 41 36 30 0D 0A"
				 */
				checkStackElement(ps, *post);
				if (isString(ps)) {
		 			if (lrc(tmpstr, ps->s) == 0) {
						if (currSymbol==LRC) {
							strNcpy(ps->s, tmpstr, LOCAL_STRING_SIZE-1);
						} else {
							strncat(ps->s, tmpstr, strlen(ps->s)-LOCAL_STRING_SIZE-1);
						}
					}
				}
				break;

			case XOR8:
			case ADD_XOR8:
				checkStackElement(ps, *post);
				if (isString(ps)) {
		 			if (xor8(tmpstr, ps->s) == 0) {
						if (currSymbol==XOR8) {
							strNcpy(ps->s, tmpstr, LOCAL_STRING_SIZE-1);
						} else {
							strncat(ps->s, tmpstr, strlen(ps->s)-LOCAL_STRING_SIZE-1);
						}
					}
				}
				break;

			case SUBRANGE:
				checkStackElement(ps, *post);
				ps2 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
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
				i = MAX(MIN(i,k),0);
				j = MIN(j,k);
				s = ps->s;
				for (s1=s+i, s2=s+j ; *s1 && s1 <= s2; ) {
					*s++ = *s1++;
				}
				*s = 0;
				break;
 
			case REPLACE:
				checkStackElement(ps, *post);
				ps2 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
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
					for (s2 = ps2->s; *s2 && (i < (LOCAL_STRING_SIZE-1)); i++)
						*s1++ = *s2++;
					while (*s && (i++ < (LOCAL_STRING_SIZE-1)))
						*s1++ = *s++;
					*s1 = '\0';
				}
				break;
 

			case MAXV_VAL:
			case MINV_VAL:
#if DEBUG
				if (sCalcPerformDebug>=10) {
					printf("val stack:");
					for (ps1=top; ps1 <= ps; ps1++) {
						if (isString(ps1))
							printf("%s ", ps1->s);
						else
							printf("%f ", ps1->d);
					}
					printf("\n");
				}
#endif
				/* make sure we have >= 2 args before NaN marker */
				checkStackElement(ps, *post);
				ps1 = ps;
				DEC(ps);
				checkStackElement(ps, *post);
				for (i=0, ps=ps1, cvt_to_double=0; i<MAX_VARGS && !isnan(ps->d); ) {
					if (isDouble(ps)) cvt_to_double = 1;
					DEC(ps);
				}
				if (i == MAX_VARGS)
					return(-1);
				/* convert all operands to the same type */
				for (ps=ps1; !isnan(ps->d); ) {
					if (cvt_to_double) {
						toDouble(ps);
					} else {
						toString(ps);
					}
					DEC(ps);
				}
				/* find max or min */
				for (ps=ps1-1; !isnan(ps->d); ) {
					if (cvt_to_double) {
						if ((ps->d < ps[1].d) == (currSymbol == MAXV_VAL))
							ps->d = ps[1].d;
					} else {
						/* compare ps->s to ps[1].s */
						if ((strcmp(ps->s, ps[1].s) < 0) == (currSymbol == MAXV_VAL)) {
							s = ps->s;
							ps->s = ps[1].s;
							ps[1].s = s;
						}
					}
					DEC(ps);
				}
				/* replace NaN marker with result */
				if (cvt_to_double) {
					ps->d = ps[1].d;
				} else {
					ps->s = ps[1].s;
					ps->d = 0.;
					ps[1].s = NULL;
				}
				break;
 
			case VARG_TERM:
				/* put a NaN on the value stack to mark the end of arguments */
				INC(ps);
				ps->s = NULL;
				pu = (DOUBLE64_2LONG32 *)&ps->d;
				pu->l[0] = pu->l[1] = 0xffffffff;
				break;

			default:
				break;
			}

			/* move ahead in postfix expression */
			++post;
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
