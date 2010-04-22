/* aCalcPostfix.c
 * Subroutines used to convert an infix expression to a postfix expression
 *
 *	Author: Tim Mooney - derived from code written by Bob Dalesio
 *	Date:	03-21-06
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 * Modification Log:
 * -----------------
 * 03-21-06 tmm Derived from sCalcPostfix
 */

/* 
 * Subroutines
 *
 *	Public
 *
 * aCalcPostfix		convert an algebraic expression to symbolic postfix
 *	args
 *		pinfix		the algebraic expression
 *		p_postfix	symbolic postfix expression
 *      perror      error information
 *	returns
 *		0		successful
 *		-1		not successful
 * Private routines for calcPostfix
 *
 * find_element		finds a symbolic element in the expression element tbl
 *	args
 *		pbuffer		pointer to the infix expression element
 *		pelement	pointer to the expression element table entry
 *		pno_bytes	pointer to the size of this element
 *		parg		pointer to arg (used for fetch)
 *	returns
 *		TRUE		element found
 *		FALSE		element not found
 *
 * get_element		finds the next expression element in the infix expr
 *	args
 *		pinfix		pointer into the infix expression
 *		pelement	pointer to the expression element table
 *		pno_bytes	size of the element in the infix expression
 *		parg		pointer to argument (used for fetch)
 *	returns
 *		FINE		found an expression element
 *		VARIABLE	found a database reference
 *		UNKNOWN_ELEMENT	unknown element found in the infix expression
 *
 * functions unique to array calcs:
 *     AMAX, AMIN - max/min of array
 *     ARNDM      - randon array
 *     ARR        - convert to array
 *     AVG        - average of array
 *     IX         - constant array [0,1,2,3,...]
 * functions specialized behavior in array calcs:
 *     MAX, MIN   - only 2 args
 *     []         - subrange translated to index 0
 *     {}         - subrange in place
 *     <<, >>     - move array contents by index
 */

#ifdef vxWorks
#include  <vxWorks.h>
#endif

#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<ctype.h>
#include	"dbDefs.h"
#include        <epicsString.h>
#define epicsExportSharedSymbols
#include	"aCalcPostfix.h"
#include	"aCalcPostfixPvt.h"
#include <epicsExport.h>

#define DEBUG 1
volatile int aCalcPostfixDebug=0;
epicsExportAddress(int, aCalcPostfixDebug);

/* declarations for postfix */
/* element types */
#define	OPERAND			0
#define UNARY_OPERATOR	1
#define	BINARY_OPERATOR	2
#define	EXPR_TERM		3
#define	COND			4
#define	CLOSE_PAREN		5
#define	CONDITIONAL		6
#define	ELSE			7
#define	SEPARATOR		8
#define	TRASH			9
#define	FLOAT_PT_CONST	10
#define	MINUS_OPERATOR	11
#define	CLOSE_BRACKET	13
#define	CLOSE_CURLY		14

/* parsing return values */
#define	FINE		0
#define	UNKNOWN_ELEMENT	-1
#define	END		-2

/*
 * element table
 *
 * structure of an element
 */
struct	expression_element{
	char	element[10];	/* character representation of an element */
	char	in_stack_pri;	/* priority in translation stack */
	char	in_coming_pri;	/* priority when first checking */
	char	type;		/* element type */
	char	code;		/* postfix representation */
};

/*
 * NOTE: DO NOT CHANGE WITHOUT READING THIS NOTICE !!!!!!!!!!!!!!!!!!!!
 * Because the routine that looks for a match in this table takes the first 
 * match it finds, elements whose designations are contained in other elements
 * MUST come first in this list. (e.g. ABS will match A if A preceeds ABS and
 * then try to find BS.  Therefore ABS must be first in this list.)
 * ':' receives special handling, so if you add an operator that includes
 * ':', you must modify that special handling.
 */
#define UNARY_MINUS_I_S_P  7
#define UNARY_MINUS_I_C_P  9
#define UNARY_MINUS_CODE   UNARY_NEG
#define BINARY_MINUS_I_S_P 5
#define BINARY_MINUS_I_C_P 5
#define BINARY_MINUS_CODE  SUB

static struct expression_element elements[] = {
/*
element    i_s_p i_c_p type_element     internal_rep */
{"ABS",    10,    11,    UNARY_OPERATOR,  ABS_VAL},   /* absolute value */
{"NOT",    10,    11,    UNARY_OPERATOR,  UNARY_NEG},   /* unary negate */
{"-",      10,    11,    MINUS_OPERATOR,  UNARY_NEG},   /* unary negate (or binary op) */
{"SQRT",   10,    11,    UNARY_OPERATOR,  SQU_RT},      /* square root */
{"SQR",    10,    11,    UNARY_OPERATOR,  SQU_RT},      /* square root */
{"EXP",    10,    11,    UNARY_OPERATOR,  EXP},         /* exponential function */
{"LOGE",   10,    11,    UNARY_OPERATOR,  LOG_E},       /* log E */
{"LN",     10,    11,    UNARY_OPERATOR,  LOG_E},       /* log E */
{"LOG",    10,    11,    UNARY_OPERATOR,  LOG_10},      /* log 10 */
{"ACOS",   10,    11,    UNARY_OPERATOR,  ACOS},        /* arc cosine */
{"ASIN",   10,    11,    UNARY_OPERATOR,  ASIN},        /* arc sine */
{"ATAN2",  10,    11,    UNARY_OPERATOR,  ATAN2},       /* arc tangent */
{"ATAN",   10,    11,    UNARY_OPERATOR,  ATAN},        /* arc tangent */
{"MAX",    10,    11,    UNARY_OPERATOR,  MAXFUNC},     /* 2 args */
{"MIN",    10,    11,    UNARY_OPERATOR,  MINFUNC},     /* 2 args */
{"AMAX",   10,    11,    UNARY_OPERATOR,  AMAX},        /* 1 arg */
{"AMIN",   10,    11,    UNARY_OPERATOR,  AMIN},        /* 1 args */
{"CEIL",   10,    11,    UNARY_OPERATOR,  CEIL},        /* smallest integer >= */
{"FLOOR",  10,    11,    UNARY_OPERATOR,  FLOOR},       /* largest integer <=  */
{"NINT",   10,    11,    UNARY_OPERATOR,  NINT},        /* nearest integer */
{"INT",    10,    11,    UNARY_OPERATOR,  NINT},        /* nearest integer */
{"COSH",   10,    11,    UNARY_OPERATOR,  COSH},        /* hyperbolic cosine */
{"COS",    10,    11,    UNARY_OPERATOR,  COS},         /* cosine */
{"SINH",   10,    11,    UNARY_OPERATOR,  SINH},        /* hyperbolic sine */
{"SIN",    10,    11,    UNARY_OPERATOR,  SIN},         /* sine */
{"TANH",   10,    11,    UNARY_OPERATOR,  TANH},        /* hyperbolic tangent*/
{"TAN",    10,    11,    UNARY_OPERATOR,  TAN},         /* tangent */
{"AVG",    10,    11,    UNARY_OPERATOR,  AVERAGE},     /* array average */
{"STD",    10,    11,    UNARY_OPERATOR,  STD_DEV},     /* standard deviation */
{"FWHM",   10,    11,    UNARY_OPERATOR,  FWHM},        /* full width at half max */
{"SMOO",   10,    11,    UNARY_OPERATOR,  SMOOTH},      /* smooth */
{"NSMOO",  10,    11,    UNARY_OPERATOR,  NSMOOTH},     /* smooth (npts)*/
{"DERIV",  10,    11,    UNARY_OPERATOR,  DERIV},       /* derivative */
{"NDERIV", 10,    11,    UNARY_OPERATOR,  NDERIV},      /* derivative (npts)*/
{"SUM",    10,    11,    UNARY_OPERATOR,  ARRSUM},      /* sum over array */
{"FITPOLY",10,    11,    UNARY_OPERATOR,  FITPOLY},     /* polynomial fit */
{"FITMPOLY",10,    11,    UNARY_OPERATOR,  FITMPOLY},     /* polynomial fit */
{"!=",      4,     4,    BINARY_OPERATOR, NOT_EQ},      /* not equal */
{"!",      10,    11,    UNARY_OPERATOR,  REL_NOT},     /* not */
{"~",      10,    11,    UNARY_OPERATOR,  BIT_NOT},     /* bitwise not */
{"DBL",    10,    11,    UNARY_OPERATOR,  TO_DOUBLE},   /* convert to double */
{"ARR",    10,    11,    UNARY_OPERATOR,  TO_ARRAY},    /* convert to array */
{"@@",     10,    11,    UNARY_OPERATOR,  A_AFETCH},    /* fetch array argument */
{"@",      10,    11,    UNARY_OPERATOR,  A_FETCH},     /* fetch numeric argument */
{"RNDM",    0,     0,    OPERAND,         RANDOM},      /* Random number */
{"ARNDM",   0,     0,    OPERAND,         ARANDOM},     /* Random array */
{"OR",      1,     1,    BINARY_OPERATOR, BIT_OR},      /* or */
{"AND",     2,     2,    BINARY_OPERATOR, BIT_AND},     /* and */
{"XOR",     1,     1,    BINARY_OPERATOR, BIT_EXCL_OR}, /* exclusive or */
{"PI",      0,     0,    OPERAND,         CONST_PI},    /* pi */
{"D2R",     0,     0,    OPERAND,         CONST_D2R},   /* pi/180 */
{"R2D",     0,     0,    OPERAND,         CONST_R2D},   /* 180/pi */
{"S2R",     0,     0,    OPERAND,         CONST_S2R},   /* arc-sec to radians: pi/(180*3600) */
{"R2S",     0,     0,    OPERAND,         CONST_R2S},   /* radians to arc-sec: (180*3600)/pi */
{"IX",      0,     0,    OPERAND,         CONST_IX},    /* array [0,1,2...] */
{"0",       0,     0,    FLOAT_PT_CONST,  LITERAL},     /* flt pt constant */
{"1",       0,     0,    FLOAT_PT_CONST,  LITERAL},     /* flt pt constant */
{"2",       0,     0,    FLOAT_PT_CONST,  LITERAL},     /* flt pt constant */
{"3",       0,     0,    FLOAT_PT_CONST,  LITERAL},     /* flt pt constant */
{"4",       0,     0,    FLOAT_PT_CONST,  LITERAL},     /* flt pt constant */
{"5",       0,     0,    FLOAT_PT_CONST,  LITERAL},     /* flt pt constant */
{"6",       0,     0,    FLOAT_PT_CONST,  LITERAL},     /* flt pt constant */
{"7",       0,     0,    FLOAT_PT_CONST,  LITERAL},     /* flt pt constant */
{"8",       0,     0,    FLOAT_PT_CONST,  LITERAL},     /* flt pt constant */
{"9",       0,     0,    FLOAT_PT_CONST,  LITERAL},     /* flt pt constant */
{".",       0,     0,    FLOAT_PT_CONST,  LITERAL},     /* flt pt constant */
{"?",       0,     0,    CONDITIONAL,     COND_IF},     /* conditional */
{":",       0,     0,    CONDITIONAL,     COND_ELSE},   /* else */
{"(",       0,     11,   UNARY_OPERATOR,  PAREN},       /* open paren */
{"[",       0,     10,   BINARY_OPERATOR, SUBRANGE},    /* array subrange */
{"{",       0,     10,   BINARY_OPERATOR, SUBRANGE_IP}, /* array subrange in place*/
{"^",       8,     8,    BINARY_OPERATOR, EXPON},       /* exponentiation */
{"**",      8,     8,    BINARY_OPERATOR, EXPON},       /* exponentiation */
{"+",       5,     5,    BINARY_OPERATOR, ADD},         /* addition */
#if 0 /* "-" operator is overloaded; may be unary or binary */
{"-",       5,     5,    BINARY_OPERATOR, SUB},         /* subtraction */
#endif
{"*",       6,     6,    BINARY_OPERATOR, MULT},        /* multiplication */
{"/",       6,     6,    BINARY_OPERATOR, DIV},         /* division */
{"%",       6,     6,    BINARY_OPERATOR, MODULO},      /* modulo */
{",",       0,     0,    SEPARATOR,       COMMA},       /* comma */
{")",       0,     0,    CLOSE_PAREN,     PAREN},       /* close paren */
{"]",       0,     0,    CLOSE_BRACKET,   SUBRANGE},    /* close bracket */
{"}",       0,     0,    CLOSE_CURLY,     SUBRANGE_IP}, /* close curly bracket */
{"||",      1,     1,    BINARY_OPERATOR, REL_OR},      /* logical or */
{"|",       1,     1,    BINARY_OPERATOR, BIT_OR},      /* bitwise or */
{"&&",      2,     2,    BINARY_OPERATOR, REL_AND},     /* logical and */
{"&",       2,     2,    BINARY_OPERATOR, BIT_AND},     /* bitwise and */
{">?",      3,     3,    BINARY_OPERATOR, MAX_VAL},     /* maximum of 2 args */
{">>",      2,     2,    BINARY_OPERATOR, RIGHT_SHIFT}, /* right shift */
{">=",      4,     4,    BINARY_OPERATOR, GR_OR_EQ},    /* greater or equal*/
{">",       4,     4,    BINARY_OPERATOR, GR_THAN},     /* greater than */
{"<?",      3,     3,    BINARY_OPERATOR, MIN_VAL},     /* minimum of 2 args */
{"<<",      2,     2,    BINARY_OPERATOR, LEFT_SHIFT},  /* left shift */
{"<=",      4,     4,    BINARY_OPERATOR, LESS_OR_EQ},  /* less or equal to*/
{"<",       4,     4,    BINARY_OPERATOR, LESS_THAN},   /* less than */
{"#",       4,     4,    BINARY_OPERATOR, NOT_EQ},      /* not equal */
{"==",      4,     4,    BINARY_OPERATOR, EQUAL},       /* equal */
{"=",       4,     4,    BINARY_OPERATOR, EQUAL},       /* equal */
{""}
};

/*
 * Element-table entry for "fetch" operation.  This element is used for all
 * named variables.  Currently, letters A-Z (double) and AA-ZZ (array) are
 * allowed.  Lower and upper case letters mean the same thing.
 */
static struct expression_element	fetch_element = {
"A",		0,	0,	OPERAND,	FETCH,   /* fetch var */
};

static struct expression_element	fetch_array_element = {
"AA",		0,	0,	OPERAND,	AFETCH,   /* fetch var */
};

#if 0
#define INC(ps) {if ((int)(++ps-top)>STACKSIZE) return(-1);}
#define DEC(ps) {if ((int)(--ps-top)<0) return(-1);}
#else
#define INC(ps) ++(ps)
#define DEC(ps) --(ps)
#endif
long aCalcCheck(char *post, int forks_checked, int dir_mask)
{
	double 		stack[STACKSIZE], *top, *ps;
	int			i, this_fork = 0;
	int			dir;
	short 		got_if=0;
	char		*post_top = post;
#if DEBUG
	char		debug_prefix[10]="";

	if (aCalcPostfixDebug) {
		for (i=0; i<=forks_checked; i++)
			strcat(debug_prefix, (dir_mask&(1<<i))?"T":"F");
		printf("aCalcCheck: entry: forks_checked=%d, dir_mask=0x%x\n",
			forks_checked, dir_mask);
	}
#endif

	top = ps = &stack[1];
	DEC(ps);  /* Expression handler assumes ps is pointing to a filled element */

	/* array expressions and values handled */
	while (*post != END_STACK) {
#if DEBUG
		if (aCalcPostfixDebug) printf("aCalcCheck: %s *post=%d\n", debug_prefix, *post);
#endif

		switch (*post) {

		case FETCH_A: case FETCH_B: case FETCH_C: case FETCH_D:
		case FETCH_E: case FETCH_F: case FETCH_G: case FETCH_H:
		case FETCH_I: case FETCH_J: case FETCH_K: case FETCH_L:
			INC(ps);
			*ps = 0;
			break;

		case FETCH:
			INC(ps);
			++post;
			*ps = 0;
			break;

		case AFETCH:	/* fetch from array variable */
			INC(ps);
			++post;
			*ps=0;
			break;

		case CONST_PI:	case CONST_D2R:	case CONST_R2D:	case CONST_S2R:
		case CONST_R2S:	case RANDOM:	case ARANDOM:	case CONST_IX:
			INC(ps);
			*ps = 0;
			break;

		/* two-argument functions/operators */
		case ADD:			case SUB:		case MAX_VAL:	case MIN_VAL:
		case MULT:			case DIV:		case EXPON:		case MODULO:
		case REL_OR:		case REL_AND:	case BIT_OR:	case BIT_AND:
		case BIT_EXCL_OR:	case GR_OR_EQ:	case GR_THAN:	case LESS_OR_EQ:
		case LESS_THAN:		case NOT_EQ:	case EQUAL:		case RIGHT_SHIFT:
		case LEFT_SHIFT:	case ATAN2:		case MAXFUNC:	case MINFUNC:
		case NSMOOTH:		case NDERIV:	case FITMPOLY:
			DEC(ps);
			*ps = 0;
			break;

		/* one-argument functions/operators */
		case ABS_VAL:	case UNARY_NEG:	case SQU_RT:	case EXP:
		case LOG_10:	case LOG_E:		case ACOS:		case ASIN:
		case ATAN:		case COS:		case SIN:		case TAN:
		case COSH:		case SINH:		case TANH:		case CEIL:
		case FLOOR:		case NINT:		case REL_NOT:	case BIT_NOT:
		case A_FETCH:	case TO_DOUBLE: case AMAX:		case AMIN:
		case AVERAGE:	case STD_DEV:	case FWHM:		case SMOOTH:
		case DERIV:		case ARRSUM:	case FITPOLY:
			*ps = 0;
			break;

		case A_AFETCH:
			*ps = 0;
			break;

		case LITERAL:
			INC(ps);
			++post;
			if (post == NULL) {
				++post;
			} else {
				post += 7;
			}
			*ps = 0;
			break;

		case SUBRANGE:
		case SUBRANGE_IP:
			DEC(ps);
			DEC(ps);
			*ps = 0;
			break;

		case TO_ARRAY:
			*ps = 0;;
			break;

		case COND_IF:
			/*
			 * Recursively check all COND_IF forks:
			 * Take the condition-false path, call aCalcCheck() to check the
			 * condition-true path, giving instructions (forks_checked,
			 * dir_mask) that will bring it to this fork and cause it to take
			 * the condition-true path. 
			 */
			dir = (this_fork <= forks_checked) ? dir_mask&(1<<this_fork) : 0;
			if (this_fork == forks_checked) {
				if (dir == 0) {
					if (aCalcCheck(post_top, this_fork, dir_mask|(1<<this_fork)))
						return(-1);
				}
			} else if (this_fork > forks_checked) {
				/* New fork, so dir has been set to 0 */
				forks_checked++;
				if (aCalcCheck(post_top, this_fork, dir_mask|(1<<this_fork)))
					return(-1);
#if DEBUG
				if (aCalcPostfixDebug) strcat(debug_prefix, "F");
#endif
			}
			this_fork++;  /* assuming we do, in fact, encounter another fork */

			/* if false condition then skip true expression */
			if (dir == 0) {
				/* skip to matching COND_ELSE */
				for (got_if=1; got_if>0 && *(post+1) != END_STACK; ++post) {
					switch(post[1]) {
					case LITERAL:	post+=8; break;
					case COND_IF:	got_if++; break;
					case COND_ELSE:	got_if--; break;
					case FETCH: case AFETCH: post++; break;
					}
				}
			}
			/* remove condition from stack top */
			DEC(ps);
			break;

		case COND_ELSE:
			/* result, true condition is on stack so skip false condition  */
			/* skip to matching COND_END */
			for (got_if=1; got_if>0 && *(post+1) != END_STACK; ++post) {
				switch(post[1]) {
				case LITERAL:	post+=8; break;
				case COND_IF:	got_if++; break;
				case COND_END:	got_if--; break;
				case FETCH: case AFETCH: post++; break;
				}
			}
			break;

		case COND_END:
			break;

		default:
			break;
		}

		/* move ahead in postfix expression */
		++post;
	}

#if DEBUG
	if (ps != top) {
		if (aCalcPostfixDebug>=10) {
			printf("aCalcCheck: stack error: top=%p, ps=%p, ps-top=%ld, got_if=%d\n",
				(void *)top, (void *)ps, (long)(ps-top), got_if);
		}
	}
	if (aCalcPostfixDebug) printf("aCalcCheck: normal exit\n");
#endif

	/* If we have a stack error, and it's not attributable to '?' without ':', complain */
	if ((ps != top) && ((top-ps) != got_if)) return(-1);
	return(0);
}

/*
 * FIND_ELEMENT
 *
 * find the pointer to an entry in the element table
 */
static int find_element(pbuffer, pelement, pno_bytes, parg)
 register char	*pbuffer;
 register struct expression_element	**pelement;
 register short	*pno_bytes, *parg;
 {
	*parg = 0;

 	/* compare the string to each element in the element table */
 	*pelement = &elements[0];
 	while ((*pelement)->element[0] != 0){
 		if (epicsStrnCaseCmp(pbuffer,(*pelement)->element, strlen((*pelement)->element)) == 0){
 			*pno_bytes += strlen((*pelement)->element);
 			return(TRUE);
 		}
 		*pelement += 1;
 	}

	/* look for a variable reference */
	/* double variables: ["a" - "z"], numbered 1-26 */
	if (isalpha((int)*pbuffer)) {
		*pelement = &fetch_element; /* fetch means "variable reference" (fetch or store) */
		*parg = *pbuffer - (isupper((int)*pbuffer) ? 'A' : 'a');
		*pno_bytes += 1;
		/* array variables: ["aa" - "zz"], numbered 1-26 */
		if (pbuffer[1] == pbuffer[0]) {
			*pelement = &fetch_array_element;
			*pno_bytes += 1;
		}
 		return(TRUE);
	}
#if DEBUG
	if (aCalcPostfixDebug) printf("find_element: can't find '%s'\n", pbuffer);
#endif
 	return(FALSE);
 }
 
/*
 * GET_ELEMENT
 *
 * get an expression element
 */
static int get_element(pinfix, pelement, pno_bytes, parg)
register char	*pinfix;
register struct expression_element	**pelement;
register short	*pno_bytes, *parg;
{

	/* get the next expression element from the infix expression */
	if (*pinfix == 0) return(END);
	*pno_bytes = 0;
	while (*pinfix == 0x20){
		*pno_bytes += 1;
		pinfix++;
	}
	if (*pinfix == 0) return(END);
	if (!find_element(pinfix, pelement, pno_bytes, parg))
		return(UNKNOWN_ELEMENT);
#if DEBUG
	if (aCalcPostfixDebug > 5) printf("get_element: found element '%s', arg=%d\n", (*pelement)->element, *parg);
#endif
	return(FINE);

	
}

/*
 * aCalcPostFix
 *
 * convert an infix expression to a postfix expression
 */
long epicsShareAPI aCalcPostfix(char *pinfix, char *ppostfix, short *perror)
{
	short no_bytes, operand_needed, new_expression;
	struct expression_element stack[80], *pelement, *pstacktop;
	double constant;
	char in_stack_pri, in_coming_pri, code, c;
	char *pposthold, *ppostfixStart;
	short arg;
	int badExpression;

#if DEBUG
	if (aCalcPostfixDebug) printf("aCalcPostfix: entry\n");
#endif

	if (ppostfix == NULL) {
		printf("aCalcPostfix: Caller did not provide a postfix buffer.\n");
		return(-1);
	}

	ppostfixStart = ppostfix;

	*ppostfixStart = BAD_EXPRESSION;
	*(++ppostfix) = END_STACK;

	operand_needed = TRUE;
	new_expression = TRUE;
	*perror = 0;
	if (*pinfix == 0) return(0);
	pstacktop = &stack[0];

	/*** place the expression elements into postfix ***/

	while (get_element(pinfix, &pelement, &no_bytes, &arg) != END){
		pinfix += no_bytes;

		switch (pelement->type){

	    case OPERAND:
			if (!operand_needed){
				*perror = 5;
				*ppostfixStart = BAD_EXPRESSION; return(-1);
			}

			/* add operand to the expression */
			if (pelement->code == (char)FETCH) {
				/*
				 * Args A..L are required to exist, so we can code for an
				 * optimized fetch.  For args M..Z, we code a parameterized
				 * fetch; aCalcPerform() should check that the arg exists.
				 */
				if (arg < 12) {
					*ppostfix++ = FETCH_A + arg;
				} else {
					*ppostfix++ = FETCH;
					*ppostfix++ = arg;
				}
			} else {
				*ppostfix++ = pelement->code;
			}

			/* if this is an array variable reference, append variable number */
			if (pelement->code == (char)AFETCH) {
				*ppostfix++ = arg;
			}

			operand_needed = FALSE;
			new_expression = FALSE;
			break;

		case FLOAT_PT_CONST:
			if (!operand_needed){
				*perror = 5;
				*ppostfixStart = BAD_EXPRESSION; return(-1);
			}

			/* add constant to postfix expression */
			*ppostfix++ = pelement->code;
			pposthold = ppostfix;

			pinfix -= no_bytes;
			while (*pinfix == ' ') *ppostfix++ = *pinfix++;
			while (TRUE) {
				if ( ( *pinfix >= '0' && *pinfix <= '9' ) || *pinfix == '.' ) {
					*ppostfix++ = *pinfix;
					pinfix++;
				} else if ( *pinfix == 'E' || *pinfix == 'e' ) {
					*ppostfix++ = *pinfix;
					pinfix++;
						if (*pinfix == '+' || *pinfix == '-' ) {
							*ppostfix++ = *pinfix;
							pinfix++;
						}
				} else break;
			}
			*ppostfix++ = '\0';

			ppostfix = pposthold;
			if (sscanf(ppostfix,"%lg",&constant) != 1) {
				*ppostfix = '\0';
			} else {
				memcpy(ppostfix,(void *)&constant,8);
			}
			ppostfix+=8;

			operand_needed = FALSE;
			new_expression = FALSE;
			break;

		case BINARY_OPERATOR:
			if (operand_needed){
				*perror = 4;
				*ppostfixStart = BAD_EXPRESSION; return(-1);
			}

			/* add operators of higher or equal priority to	postfix expression */
			while ((pstacktop->in_stack_pri >= pelement->in_coming_pri)
					&& (pstacktop >= &stack[1])){
				*ppostfix++ = pstacktop->code;
				pstacktop--;
			}

			/* add new operator to stack */
			pstacktop++;
			*pstacktop = *pelement;

			operand_needed = TRUE;
			break;

		case UNARY_OPERATOR:
			if (!operand_needed){
				*perror = 5;
				*ppostfixStart = BAD_EXPRESSION; return(-1);
			}

			/* add operators of higher or equal priority to	postfix expression */
			while ((pstacktop->in_stack_pri >= pelement->in_coming_pri)
					&& (pstacktop >= &stack[1])){
				*ppostfix++ = pstacktop->code;
				pstacktop--;
			}

			/* add new operator to stack */
			pstacktop++;
			*pstacktop = *pelement;

			new_expression = FALSE;
			break;

		case MINUS_OPERATOR:
			if (operand_needed) {
				/* then assume minus was intended as a unary operator */
				in_coming_pri = UNARY_MINUS_I_C_P;
				in_stack_pri = UNARY_MINUS_I_S_P;
				code = UNARY_MINUS_CODE;
				new_expression = FALSE;
			} else {
				/* then assume minus was intended as a binary operator */
				in_coming_pri = BINARY_MINUS_I_C_P;
				in_stack_pri = BINARY_MINUS_I_S_P;
				code = BINARY_MINUS_CODE;
				operand_needed = TRUE;
			}

			/* add operators of higher or equal priority to	postfix expression */
			while ((pstacktop->in_stack_pri >= in_coming_pri)
					&& (pstacktop >= &stack[1])){
				*ppostfix++ = pstacktop->code;
				pstacktop--;
			}

			/* add new operator to stack */
			pstacktop++;
			*pstacktop = *pelement;
			pstacktop->in_stack_pri = in_stack_pri;
			pstacktop->code = code;

			break;

		case SEPARATOR:
			if (operand_needed){
				*perror = 4;
				*ppostfixStart = BAD_EXPRESSION; return(-1);
			}

			/* add operators to postfix until open paren */
			while ((pstacktop->element[0] != '(') && (pstacktop->element[0] != '[')
					&& (pstacktop->element[0] != '{')) {
				if (pstacktop == &stack[1] || pstacktop == &stack[0]){
					*perror = 6;
					*ppostfixStart = BAD_EXPRESSION; return(-1);
				}
				*ppostfix++ = pstacktop->code;
				pstacktop--;
			}
			operand_needed = TRUE;
			break;

		case CLOSE_PAREN:
			if (operand_needed){
				*perror = 4;
				*ppostfixStart = BAD_EXPRESSION; return(-1);
			}

			/* add operators to postfix until matching paren */
			while (pstacktop->element[0] != '(') {
				if (pstacktop == &stack[1] || pstacktop == &stack[0]) {
					*perror = 6;
					*ppostfixStart = BAD_EXPRESSION; return(-1);
				}
				*ppostfix++ = pstacktop->code;
				pstacktop--;
			}
			pstacktop--;	/* remove ( from stack */
			break;

		case CLOSE_BRACKET:
		case CLOSE_CURLY:
			c = (pelement->type == CLOSE_BRACKET) ? '[' : '{';
			if (operand_needed){
				*perror = 4;
				*ppostfixStart = BAD_EXPRESSION; return(-1);
			}

			/* add operators to postfix until matching bracket */
			while (pstacktop->element[0] != c) {
				if (pstacktop == &stack[1] || pstacktop == &stack[0]) {
					*perror = 6;
					*ppostfixStart = BAD_EXPRESSION; return(-1);
				}
				*ppostfix++ = pstacktop->code;
				pstacktop--;
			}
			/* add SUBRANGE operator to postfix */
			if (pstacktop == &stack[0]) {
				*perror = 6;
				*ppostfixStart = BAD_EXPRESSION; return(-1);
			}
			*ppostfix++ = pstacktop->code;
			pstacktop--;
			break;

		case CONDITIONAL:
			if (operand_needed){
				*perror = 4;
				*ppostfixStart = BAD_EXPRESSION; return(-1);
			}

			/* add operators of higher priority to postfix expression */
			while ((pstacktop->in_stack_pri > pelement->in_coming_pri)
					&& (pstacktop >= &stack[1])){
				*ppostfix++ = pstacktop->code;
				pstacktop--;
			}

			/* add new element to the postfix expression */
			*ppostfix++ = pelement->code;

			/* add : operator with COND_END code to stack */
			if (pelement->element[0] == ':'){
				pstacktop++;
				*pstacktop = *pelement;
				pstacktop->code = COND_END;
			}

			operand_needed = TRUE;
			break;

		case EXPR_TERM:
			if (operand_needed && !new_expression){
				*perror = 4;
				*ppostfixStart = BAD_EXPRESSION; return(-1);
			}

			/* add all operators on stack to postfix */
			while (pstacktop >= &stack[1]){
				if (pstacktop->element[0] == '('){
					*perror = 6;
					*ppostfixStart = BAD_EXPRESSION; return(-1);
				}
				*ppostfix++ = pstacktop->code;
				pstacktop--;
			}

			/* add new element to the postfix expression */
			*ppostfix++ = pelement->code;

			operand_needed = TRUE;
			new_expression = TRUE;
			break;

		default:
			*perror = 8;
			*ppostfixStart = BAD_EXPRESSION; return(-1);
		}
	}
	if (operand_needed){
		*perror = 4;
		*ppostfixStart = BAD_EXPRESSION; return(-1);
	}

	/* add all operators on stack to postfix */
	while (pstacktop >= &stack[1]){
		if (pstacktop->element[0] == '('){
			*perror = 6;
			*ppostfixStart = BAD_EXPRESSION; return(-1);
		}
		*ppostfix++ = pstacktop->code;
		pstacktop--;
	}
	*ppostfix++ = END_STACK;
	*ppostfix = '\0';

	if ((ppostfixStart[1] == END_STACK) || aCalcCheck(ppostfixStart, 0, 0)) {
		*ppostfixStart = BAD_EXPRESSION;
		badExpression = 1;
	} else {
		*ppostfixStart = GOOD_EXPRESSION;
		badExpression = 0;
	}

#if DEBUG
	if (aCalcPostfixDebug) {
		printf("aCalcPostfix: buf-used=%d\n", (int)(1+ppostfix-ppostfixStart));
	}
#endif
	return(badExpression);
}
