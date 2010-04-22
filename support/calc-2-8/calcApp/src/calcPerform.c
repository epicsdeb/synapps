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
 * .01	5-18-88		lrd	modified modulo and power to avoid math library
 * .02	5-19-88		lrd	modified absolute value to avoid math library
 *				defined unary math lib routines as doubles
 *				removed include math.h
 *				stopped loading dinglers math routines (ml)
 *				wrote a random number generator to return a
 *					double between 0 and 1
 * .03	12-09-88	lrd	fixed modulo not to perform zero division
 * .04	12-12-88	lrd	lock the record while processing
 * .05	12-13-88	lrd	made an alarm for math error
 * .06	12-15-88	lrd	Process the forward scan link
 * .07  12-23-88        lrd     Alarm on locked MAX_LOCKED times
 * .08	01-11-89	lrd	Add Right and Left Shift
 * .09	02-01-89	lrd	Add Trig functions
 * .10	03-14-89	lrd	fix true on C question mark operator
 * .11	03-29-89	lrd	make hardware errors MAJOR
 *				remove hw severity spec from database
 * .12	04-06-89	lrd	add monitor detection
 * .13	05-03-89	lrd	removed process mask from arg list
 * .14	06-05-89	lrd	check for negative square root
 * .15	08-01-89	lrd	full range of exponentiation using pow(x,y) 
 * .16	04-04-90	lrd	fix post events for read and calc alarms
 *				fix neg base raised to integer exponent
 * .17	04-06-90	lrd	change conditional to check for 0 and non-zero
 *				instead of 0 and 1 (more 'C' like)
 * .18	09-10-90	lrd	add time stamps
 * .19	11-26-90	lrd	add bit not and relational not - fix RNDM
 * .20	11-29-90	lrd	conditionally process soft channels
 * .21	12-14-90	lrd	fixed post events for the variables
 * .22  03-15-91	mrk	moved code from calcRecord to here
 * .23	08-01-91	rac	don't use FETCH_G ... for V2
 * .24	02-20-92	rcz	fixed for vxWorks build
 * .25	02-24-92	jba	add EXP and fix for EXPON when *pstacktop is 0
 * .26	02-28-92	jba	added CEIL and FLOOR
 * .27  03-06-92        jba     added MAX and MIN binary functions
 * .28  03-10-92        jba     added multiple conditional expressions ?
 * .29  04-01-92        jba     allowed floating pt constants in expression
 * .30  05-01-92        jba     flt pt constant string replaced with double in postfix
 * .31  08-21-92        jba     ANSI c changes
 * .32  12-11-92	mrk	Removed include for stdioLib.h
 * .33  08-04-93	mgb	Removed V5/V4 and EPICS_V2 conditionals
 * .34  05-11-94	jba	Added support for CONST_PI, CONST_D2R, and CONST_R2D
 * .34  08-18-94	jba	Must skip over constant when looking for COND_END,COND_ELSE
 * .35	10-07-94	mda	change local random() to local_random() to
 *				avoid colliding with math library random()
 * .36  01-22-98    tmm Made number of arguments variable.  Arguments not
                    supplied at execution time have value 0.0.  Added R2S, S2R
                    (conversions between arc-seconds and radians).  Changed
                    "CONSTANT" to "LITERAL" to avoid conflict with link.h.
  $Log%

 */

/* This module contains the code for processing the arithmetic
 * expressions defined in calculation records. postfix must be called
 * to convert a valid infix expression to postfix. CalcPerform
 * calculates the postfix expression.
 *
 * Subroutines
 *
 *	Public
 *
 * calcPerform		perform the calculation
 *	    args
 *		double *pargs	address of arguments (12?)
 *      int    numArgs  number of arguments in pargs array
 *		double *presult address of result
 *		char   *rpcl	address of reverse polish buffer
 *	    returns
 *		0		fetched successfully
 *		-1		fetch failed
 *
 * Private routine for calcPerform
 *	local_random		random number generator
 *	    returns
 *		double value between 0.00 and 1.00
 */

#ifdef vxWorks
#include <vxWorks.h>
#endif

#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<math.h>

#define epicsExportSharedSymbols
#include	"dbDefs.h"
#include	"calcPostfix.h"

static double	local_random();

#define	NOT_SET		0
#define	TRUE_COND	1
#define	FALSE_COND	2

#ifndef PI
#define PI 3.141592654
#endif

epicsShareFunc long epicsShareAPI 
	newcalcPerform(double *parg, int numArgs, double *presult, char *post)
{
	double *pstacktop;	/* stack of values	*/
	double		stack[80];
	short		temp1;
	short	i;
	double 		*top;
	int 		itop;		/* integer top value	*/
	int 		inexttop;	/* ineteger next to top value 	*/
	short 		cond_flag;	/* conditional else flag	*/
	short 		got_if;

	/* initialize flag  */
	cond_flag = NOT_SET;
	pstacktop = &stack[0];

    if(*post == BAD_EXPRESSION) return(-1);

	/* set post to postfix expression in calc structure */
	top = pstacktop;

	/* polish calculator loop */
	while (*post != END_STACK){

		switch (*post){
		case FETCH:
			++pstacktop;
			++post;
			if (*post < numArgs) {
				*pstacktop = parg[*post];
			} else {
				*pstacktop = 0.;
			}
			break;

		case CONST_PI:
			++pstacktop;
			*pstacktop = PI;
			break;

		case CONST_D2R:
			++pstacktop;
			*pstacktop = PI/180.;
			break;

		case CONST_R2D:
			++pstacktop;
			*pstacktop = 180./PI;
			break;

		case CONST_S2R:
			++pstacktop;
			*pstacktop = PI/(180.*3600);
			break;

		case CONST_R2S:
			++pstacktop;
			*pstacktop = (180.*3600)/PI;
			break;

		case ADD:
			--pstacktop;
			*pstacktop = *pstacktop + *(pstacktop+1);
			break;

		case SUB:
			--pstacktop;
			*pstacktop = *pstacktop - *(pstacktop+1);
			break;

		case MULT:
			--pstacktop;
			*pstacktop = *pstacktop * *(pstacktop+1);
			break;

		case DIV:
			--pstacktop;
			if (*(pstacktop+1) == 0) /* can't divide by zero */
				return(-1);
			*pstacktop = *pstacktop / *(pstacktop+1);
			break;

		case COND_IF:
			/* if false condition then skip true expression */
			if (*pstacktop == 0.0) {
				/* skip to matching COND_ELSE */
				for (got_if=1; got_if>0 && *(post+1) != END_STACK; ++post) {
					if (*(post+1) == LITERAL  ) post+=8;
					else if (*(post+1) == COND_IF  ) got_if++;
					else if (*(post+1) == COND_ELSE) got_if--;
				}
			}
			/* remove condition from stack top */
			--pstacktop;
			break;
				
		case COND_ELSE:
			/* result, true condition is on stack so skip false condition  */
			/* skip to matching COND_END */
			for (got_if=1; got_if>0 && *(post+1) != END_STACK; ++post) {
				if (*(post+1) == LITERAL  ) post+=8;
				else if (*(post+1) == COND_IF ) got_if++;
				else if (*(post+1) == COND_END) got_if--;
			}
			break;

		case COND_END:
			break;

		case ABS_VAL:
			if (*pstacktop < 0) *pstacktop = -*pstacktop;
			break;

		case UNARY_NEG:
			*pstacktop = -1* (*pstacktop);
			break;

		case SQU_RT:
			if (*pstacktop < 0) return(-1);	/* undefined */
			*pstacktop = sqrt(*pstacktop);
			break;

		case EXP:
			*pstacktop = exp(*pstacktop);
			break;

		case LOG_10:
			*pstacktop = log10(*pstacktop);
			break;

		case LOG_E:
			*pstacktop = log(*pstacktop);
			break;

		case RANDOM:
			++pstacktop;
			*pstacktop = local_random();
			break;

		case EXPON:
			--pstacktop;
			if (*pstacktop == 0) break;
			if (*pstacktop < 0){
				temp1 = (int) *(pstacktop+1);
				/* is exponent an integer */
				if ((*(pstacktop+1) - (double)temp1) != 0) return (-1);
        			*pstacktop = exp(*(pstacktop+1) * log(-*pstacktop));
				/* is value negative */
				if ((temp1 % 2) > 0) *pstacktop = -*pstacktop;
			}else{
        			*pstacktop = exp(*(pstacktop+1) * log(*pstacktop));
			}
			break;

		case MODULO:
			--pstacktop;
			itop = (int)*pstacktop;
			inexttop = (int)*(pstacktop+1);
			if (inexttop == 0)
				return(-1);
			i =  itop % inexttop;
			*pstacktop = i;
			break;

		case REL_OR:
			--pstacktop;
			*pstacktop = (*pstacktop || *(pstacktop+1));
			break;

		case REL_AND:
			--pstacktop;
			*pstacktop = (*pstacktop && *(pstacktop+1));
			break;

		case BIT_OR:
			/* force double values into integers and or them */
			itop = (int)*pstacktop;
			inexttop = (int)*(pstacktop-1);
			--pstacktop;
			*pstacktop = (inexttop | itop);
			break;

		case BIT_AND:
			/* force double values into integers and and them */
			itop = (int)*pstacktop;
			inexttop = (int)*(pstacktop-1);
			--pstacktop;
			*pstacktop = (inexttop & itop);
			break;

		case BIT_EXCL_OR:
			/*force double values to integers to exclusive or them*/
			itop = (int)*pstacktop;
			inexttop = (int)*(pstacktop-1);
			--pstacktop;
			*pstacktop = (inexttop ^ itop);
			break;

		case GR_OR_EQ:
			--pstacktop;
			*pstacktop = *pstacktop >= *(pstacktop+1);
			break;

		case GR_THAN:
			--pstacktop;
			*pstacktop = *pstacktop > *(pstacktop+1);
			break;

		case LESS_OR_EQ:
			--pstacktop;
			*pstacktop = *pstacktop <= *(pstacktop+1);
			break;

		case LESS_THAN:
			--pstacktop;
			*pstacktop = *pstacktop < *(pstacktop+1);
			break;

		case NOT_EQ:
			--pstacktop;
			*pstacktop = *pstacktop != *(pstacktop+1);
			break;

		case EQUAL:
			--pstacktop;
			*pstacktop = (*pstacktop == *(pstacktop+1));
			break;

		case RIGHT_SHIFT:
			itop = (int)*pstacktop;
			inexttop = (int)*(pstacktop-1);
			--pstacktop;
			*pstacktop = (inexttop >> itop);
			break;

		case LEFT_SHIFT:
			itop = (int)*pstacktop;
			inexttop = (int)*(pstacktop-1);
			--pstacktop;
			*pstacktop = (inexttop << itop);
			break;

                case MAX:
                        --pstacktop;
                        if (*pstacktop < *(pstacktop+1))
                                *pstacktop = *(pstacktop+1);
                        break;
 
                case MIN:
                        --pstacktop;
                        if (*pstacktop > *(pstacktop+1))
                                *pstacktop = *(pstacktop+1);
                        break;
 

		case ACOS:
			*pstacktop = acos(*pstacktop);
			break;

		case ASIN:
			*pstacktop = asin(*pstacktop);
			break;

		case ATAN:
			*pstacktop = atan(*pstacktop);
			break;

 		case ATAN2:
 			--pstacktop;
 			*pstacktop = atan2(*(pstacktop+1), *pstacktop);
 			break;

		case COS:
			*pstacktop = cos(*pstacktop);
			break;

		case SIN:
			*pstacktop = sin(*pstacktop);
			break;

		case TAN:
			*pstacktop = tan(*pstacktop);
			break;

		case COSH:
			*pstacktop = cosh(*pstacktop);
			break;

		case SINH:
			*pstacktop = sinh(*pstacktop);
			break;

		case TANH:
			*pstacktop = tanh(*pstacktop);
			break;

		case CEIL:
			*pstacktop = ceil(*pstacktop);
			break;

		case FLOOR:
			*pstacktop = floor(*pstacktop);
			break;

		case NINT:
			*pstacktop = (double)(long)((*pstacktop) >= 0 ? (*pstacktop)+0.5 : (*pstacktop)-0.5);
			break;

		case REL_NOT:
			*pstacktop = ((*pstacktop)?0:1);
			break;

		case BIT_NOT:
			itop = (int)*pstacktop;
			*pstacktop = ~itop;
			break;

		case LITERAL:
			++pstacktop;
			++post;
			if ( post == NULL ) {
				++post;
				printf("%.7s bad constant in expression\n",post);
				break;
			}
			memcpy((void *)pstacktop,post,8);
			post+=7;
			break;

		default:
			printf("calcPerform: %d bad expression element\n",*post);
			break;
		}

		/* move ahead in postfix expression */
		++post;
	}

	/* if everything is peachy,the stack should end at its first position */
	if (++top == pstacktop)
		*presult = *pstacktop;
	else
		return(-1);
	return(0);
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
