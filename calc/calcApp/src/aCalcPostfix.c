/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id: aCalcPostfix.c,v 1.19 2009-09-09 16:39:34 mooney Exp $
 * Subroutines used to convert an infix expression to a postfix expression
 *
 *      Original Author: Bob Dalesio, as postfix.c in EPICS base
 *      Date:            12-12-86
 */

#include	<stdio.h>
#include	<string.h>
#include	<ctype.h>

#include	<dbDefs.h>
#include	<epicsStdlib.h>
#include	<epicsString.h>
#define epicsExportSharedSymbols
#include	"aCalcPostfix.h"
#include	"aCalcPostfixPvt.h"
#include <epicsExport.h>

#define DEBUG 1
volatile int aCalcPostfixDebug=0;
epicsExportAddress(int, aCalcPostfixDebug);

/* declarations for postfix */
/* element types */

typedef enum {
	OPERAND,
	LITERAL_OPERAND,
	STORE_OPERATOR,
	UNARY_OPERATOR,
	VARARG_OPERATOR,
	BINARY_OPERATOR,
	SEPARATOR,
	CLOSE_PAREN,
	CONDITIONAL,
	EXPR_TERMINATOR,
	CLOSE_BRACKET,
	CLOSE_CURLY,
	UNTIL_OPERATOR
} element_type;

/*
 * element table
 *
 * structure of an element
 */
typedef struct expression_element {
	char *name; 	 /* character representation of an element */
	char in_stack_pri;	 /* priority on translation stack */
	char in_coming_pri;  /* priority in input string */
	signed char runtime_effect; /* stack change, positive means push */
	element_type type;	 /* element type */
	aCalc_rpn_opcode code;	 /* postfix opcode */
} ELEMENT;

/*
 * NOTE: DO NOT CHANGE WITHOUT READING THIS NOTICE !!!!!!!!!!!!!!!!!!!!
 * Because the routine that looks for a match in this table works from the end
 * to the start, and takes the first match it finds, elements whose designations
 * are contained in other elements MUST come later in this list. (e.g. ABS will
 * match A if A follows ABS and then try to find BS.  Therefore ABS must come
 * after A in this list.)
 * ':' receives special handling, so if you add an operator that includes
 * ':', you must modify that special handling.
 */

/* comparison of operator precedence here and in EPICS base:
 * aCalcPostfix									postfix
 *=======================================================
 * 0, 0		constants							0, 0
 * 1, 1		||, |, OR, XOR						1, 1
 * 2, 2		AND, &&, &, >>, <<					2, 2
 * 3, 3		>?, <?								<not used>
 * 4, 4		>=, >, <=, <, #, !=, ==, =			3, 3
 * 5, 5		-|, |-, -, +						4, 4
 * 6, 6		*, /, %								5, 5
 * 7, 7		^, **								6, 6
 * 8, 9		ABS, NOT, etc.						7, 8
 * 0,10		[, {								<not used>
 */

static const ELEMENT operands[] = {
/* name			prio's	stack	element type		opcode */
{"@",			9, 10,	0,		UNARY_OPERATOR,		A_FETCH},     /* fetch numeric argument */
{"@@",			9, 10,	0,		UNARY_OPERATOR,		A_AFETCH},    /* fetch array argument */
{"!",			9, 10,	0,		UNARY_OPERATOR,		REL_NOT},
{"(",			0, 10,	0,		UNARY_OPERATOR,		NOT_GENERATED},
{"-",			9, 10,	0,		UNARY_OPERATOR,		UNARY_NEG},
{".",			0, 0,	1,		LITERAL_OPERAND,	LITERAL_DOUBLE},
{"0",			0, 0,	1,		LITERAL_OPERAND,	LITERAL_DOUBLE},
{"1",			0, 0,	1,		LITERAL_OPERAND,	LITERAL_DOUBLE},
{"2",			0, 0,	1,		LITERAL_OPERAND,	LITERAL_DOUBLE},
{"3",			0, 0,	1,		LITERAL_OPERAND,	LITERAL_DOUBLE},
{"4",			0, 0,	1,		LITERAL_OPERAND,	LITERAL_DOUBLE},
{"5",			0, 0,	1,		LITERAL_OPERAND,	LITERAL_DOUBLE},
{"6",			0, 0,	1,		LITERAL_OPERAND,	LITERAL_DOUBLE},
{"7",			0, 0,	1,		LITERAL_OPERAND,	LITERAL_DOUBLE},
{"8",			0, 0,	1,		LITERAL_OPERAND,	LITERAL_DOUBLE},
{"9",			0, 0,	1,		LITERAL_OPERAND,	LITERAL_DOUBLE},
{"A",			0, 0,	1,		OPERAND,			FETCH_A},
{"AA",			0, 0,	1,		OPERAND,			FETCH_AA},
{"ABS",			9, 10,	0,		UNARY_OPERATOR,		ABS_VAL},
{"ACOS",		9, 10,	0,		UNARY_OPERATOR,		ACOS},
{"ARR",			9, 10,	0,		UNARY_OPERATOR,		TO_ARRAY},   /* convert to array */
{"ARNDM",		0, 0,	1,		OPERAND,			ARANDOM},
{"ASIN",		9, 10,	0,		UNARY_OPERATOR,		ASIN},
{"ATAN",		9, 10,	0,		UNARY_OPERATOR,		ATAN},
{"ATAN2",		9, 10,	-1,		UNARY_OPERATOR,		ATAN2},
{"AVAL",		0, 0,	1,		OPERAND,			FETCH_AVAL},
{"AVG",			9, 10,	0,		UNARY_OPERATOR,		AVERAGE},
{"B",			0, 0,	1,		OPERAND,			FETCH_B},
{"BB",			0, 0,	1,		OPERAND,			FETCH_BB},
{"C",			0, 0,	1,		OPERAND,			FETCH_C},
{"CAT",			9, 10,	-1,		UNARY_OPERATOR,		CAT},
{"CC",			0, 0,	1,		OPERAND,			FETCH_CC},
{"CEIL",		9, 10,	0,		UNARY_OPERATOR,		CEIL},
{"COS",			9, 10,	0,		UNARY_OPERATOR,		COS},
{"COSH",		9, 10,	0,		UNARY_OPERATOR,		COSH},
{"CUM",			9, 10,	0,		UNARY_OPERATOR,		CUM},
{"D",			0, 0,	1,		OPERAND,			FETCH_D},
{"DD",			0, 0,	1,		OPERAND,			FETCH_DD},
{"DBL",			9, 10,	0,		UNARY_OPERATOR,		TO_DOUBLE},   /* convert to double */
{"DERIV",		9, 10,	0,		UNARY_OPERATOR,		DERIV},
{"D2R",			0, 0,	1,		OPERAND,			CONST_D2R},
{"E",			0, 0,	1,		OPERAND,			FETCH_E},
{"EE",			0, 0,	1,		OPERAND,			FETCH_EE},
{"EXP",			9, 10,	0,		UNARY_OPERATOR,		EXP},
{"F",			0, 0,	1,		OPERAND,			FETCH_F},
{"FF",			0, 0,	1,		OPERAND,			FETCH_FF},
{"FINITE",		9, 10,	0,		VARARG_OPERATOR,	FINITE},
{"FITQ",		9, 10,	0,		VARARG_OPERATOR,	FITQ},
{"FITMQ",		9, 10,	0,		VARARG_OPERATOR,	FITMQ},
{"FITPOLY",		9, 10,	0,		UNARY_OPERATOR,		FITPOLY},
{"FITMPOLY",	9, 10,	-1,		UNARY_OPERATOR,		FITMPOLY},
{"FLOOR",		9, 10,	0,		UNARY_OPERATOR,		FLOOR},
{"FWHM",		9, 10,	0,		UNARY_OPERATOR,		FWHM},
{"G",			0, 0,	1,		OPERAND,			FETCH_G},
{"GG",			0, 0,	1,		OPERAND,			FETCH_GG},
{"H",			0, 0,	1,		OPERAND,			FETCH_H},
{"HH",			0, 0,	1,		OPERAND,			FETCH_HH},
{"I",			0, 0,	1,		OPERAND,			FETCH_I},
{"II",			0, 0,	1,		OPERAND,			FETCH_II},
{"INT",			9, 10,	0,		UNARY_OPERATOR,		NINT},
{"ISINF",		9, 10,	0,		UNARY_OPERATOR,		ISINF},
{"ISNAN",		9, 10,	0,		VARARG_OPERATOR,	ISNAN},
{"IX",			0, 0,	1,		OPERAND,			CONST_IX},
{"IXMAX",		9, 10,	0,		UNARY_OPERATOR,		IXMAX},
{"IXMIN",		9, 10,	0,		UNARY_OPERATOR,		IXMIN},
{"IXZ",			9, 10,	0,		UNARY_OPERATOR,		IXZ},
{"IXNZ",		9, 10,	0,		UNARY_OPERATOR,		IXNZ},
{"J",			0, 0,	1,		OPERAND,			FETCH_J},
{"JJ",			0, 0,	1,		OPERAND,			FETCH_JJ},
{"K",			0, 0,	1,		OPERAND,			FETCH_K},
{"KK",			0, 0,	1,		OPERAND,			FETCH_KK},
{"L",			0, 0,	1,		OPERAND,			FETCH_L},
{"LL",			0, 0,	1,		OPERAND,			FETCH_LL},
{"LN",			9, 10,	0,		UNARY_OPERATOR,		LOG_E},
{"LOG",			9, 10,	0,		UNARY_OPERATOR,		LOG_10},
{"LOGE",		9, 10,	0,		UNARY_OPERATOR,		LOG_E},
{"M",			0, 0,	1,		OPERAND,			FETCH_M},
{"AMAX",		9, 10,	0,		UNARY_OPERATOR,	AMAX},
{"AMIN",		9, 10,	0,		UNARY_OPERATOR,	AMIN},
{"MAX",			9, 10,	0,		VARARG_OPERATOR,	MAX},
{"MIN",			9, 10,	0,		VARARG_OPERATOR,	MIN},
{"N",			0, 0,	1,		OPERAND,			FETCH_N},
{"NINT",		9, 10,	0,		UNARY_OPERATOR,		NINT},
{"NDERIV",		9, 10,	-1,		UNARY_OPERATOR,		NDERIV},
{"NOT",			9, 10,	0,		UNARY_OPERATOR,		BIT_NOT},
{"NRNDM",		0, 0,	1,		OPERAND,			NORMAL_RNDM},   /* Normally Distributed Random Number */
{"NSMOO",		9, 10,	-1,		UNARY_OPERATOR,		NSMOOTH},
{"O",			0, 0,	1,		OPERAND,			FETCH_O},
{"P",			0, 0,	1,		OPERAND,			FETCH_P},
{"PI",			0, 0,	1,		OPERAND,			CONST_PI},
{"R2D",			0, 0,	1,		OPERAND,			CONST_R2D},
{"R2S",			0, 0,	1,		OPERAND,			CONST_R2S},
{"RNDM",		0, 0,	1,		OPERAND,			RANDOM},
{"SIN",			9, 10,	0,		UNARY_OPERATOR,		SIN},
{"SINH",		9, 10,	0,		UNARY_OPERATOR,		SINH},
{"SMOO",		9, 10,	0,		UNARY_OPERATOR,		SMOOTH},
{"SQR",			9, 10,	0,		UNARY_OPERATOR,		SQU_RT},
{"SQRT",		9, 10,	0,		UNARY_OPERATOR,		SQU_RT},
{"STD",			9, 10,	0,		UNARY_OPERATOR,		STD_DEV},
{"SUM",			9, 10,	0,		UNARY_OPERATOR,		ARRSUM},
{"S2R",			0, 0,	1,		OPERAND,			CONST_S2R},
{"TAN",			9, 10,	0,		UNARY_OPERATOR,		TAN},
{"TANH",		9, 10,	0,		UNARY_OPERATOR,		TANH},
{"VAL",			0, 0,	1,		OPERAND,			FETCH_VAL},
{"LEN",			9, 10,	0,		UNARY_OPERATOR,		LEN},         /* Array length not implemented */
{"UNTIL",		0, 10,	0,		UNTIL_OPERATOR,		UNTIL},
{"~",			9, 10,	0,		UNARY_OPERATOR, 	BIT_NOT},
};

static const ELEMENT operators[] = {
/* name 		prio's	stack	element type		opcode */
{"!=",			5, 5,	-1,		BINARY_OPERATOR,	NOT_EQ},
{"#",			5, 5,	-1,		BINARY_OPERATOR,	NOT_EQ},
{"%",			7, 7,	-1,		BINARY_OPERATOR,	MODULO},
{"&",			3, 3,	-1,		BINARY_OPERATOR,	BIT_AND},
{"&&",			3, 3,	-1,		BINARY_OPERATOR,	REL_AND},
{")",			0, 0,	0,		CLOSE_PAREN,		NOT_GENERATED},
{"[",			0, 11,	-1,		BINARY_OPERATOR,	SUBRANGE},    /* array subrange */
{"{",			0, 11,	-1,		BINARY_OPERATOR,	SUBRANGE_IP},      /* array subrange in place */
{"]",			0, 0,	0,		CLOSE_BRACKET,		NOT_GENERATED},
{"}",			0, 0,	0,		CLOSE_CURLY,		NOT_GENERATED}, 
{"*",			7, 7,	-1,		BINARY_OPERATOR,	MULT},
{"**",			8, 8,	-1,		BINARY_OPERATOR,	POWER},
{"+",			6, 6,	-1,		BINARY_OPERATOR,	ADD},
{",",			0, 0,	0,		SEPARATOR,			NOT_GENERATED},
{"-",			6, 6,	-1,		BINARY_OPERATOR,	SUB},
{"/",			7, 7,	-1,		BINARY_OPERATOR,	DIV},
{":",			0, 0,	-1,		CONDITIONAL,		COND_ELSE},
{":=",			1, 0,	-1,		STORE_OPERATOR,		STORE_A},
{";",			0, 0,	0,		EXPR_TERMINATOR,	NOT_GENERATED},
{"<",			5, 5,	-1,		BINARY_OPERATOR,	LESS_THAN},
{"<<",			3, 3,	-1,		BINARY_OPERATOR,	LEFT_SHIFT},
{"<=",			5, 5,	-1,		BINARY_OPERATOR,	LESS_OR_EQ},
{"=",			5, 5,	-1,		BINARY_OPERATOR,	EQUAL},
{"==",			5, 5,	-1,		BINARY_OPERATOR,	EQUAL},
{">",			5, 5,	-1,		BINARY_OPERATOR,	GR_THAN},
{">=",			5, 5,	-1,		BINARY_OPERATOR,	GR_OR_EQ},
{">>",			3, 3,	-1,		BINARY_OPERATOR,	RIGHT_SHIFT},
{"?",			0, 0,	-1,		CONDITIONAL,		COND_IF},
{"AND",			3, 3,	-1,		BINARY_OPERATOR,	BIT_AND},
{"OR",			2, 2,	-1,		BINARY_OPERATOR,	BIT_OR},
{"XOR",			2, 2,	-1,		BINARY_OPERATOR,	BIT_EXCL_OR},
{"^",			8, 8,	-1,		BINARY_OPERATOR,	POWER},
{"|",			2, 2,	-1,		BINARY_OPERATOR,	BIT_OR},
{"||",			2, 2,	-1,		BINARY_OPERATOR,	REL_OR},
{">?",			4, 4,	-1,		BINARY_OPERATOR,	MAX_VAL},     /* maximum of 2 args */
{"<?",			4, 4,	-1,		BINARY_OPERATOR,	MIN_VAL},     /* minimum of 2 args */
};

/* get_element
 *
 * find the next expression element in the infix expression
 */
static int
	get_element(int opnd, const char **ppsrc, const ELEMENT **ppel)
{
	const ELEMENT *ptable, *pel;

	*ppel = NULL;

	while (isspace((int) (unsigned char) **ppsrc)) ++*ppsrc;
	if (**ppsrc == '\0') return FALSE;

	if (opnd) {
		ptable = operands;
		pel = ptable + NELEMENTS(operands) - 1;
	} else {
		ptable = operators;
		pel = ptable + NELEMENTS(operators) - 1;
	}

	while (pel >= ptable) {
		size_t len = strlen(pel->name);

		if (epicsStrnCaseCmp(*ppsrc, pel->name, len) == 0) {
			*ppel = pel;
			*ppsrc += len;
			return TRUE;
		}
		--pel;
	}

	return FALSE;
}

static const char *opcodes[] = {
	"End Expression",
/* Operands */
	"LITERAL_DOUBLE", "LITERAL_INT", "VAL", "AVAL",
	"FETCH_A", "FETCH_B", "FETCH_C", "FETCH_D", "FETCH_E", "FETCH_F",
	"FETCH_G", "FETCH_H", "FETCH_I", "FETCH_J", "FETCH_K", "FETCH_L",
	"FETCH_M", "FETCH_N", "FETCH_O", "FETCH_P",
	"FETCH_AA", "FETCH_BB", "FETCH_CC", "FETCH_DD", "FETCH_EE", "FETCH_FF",
	"FETCH_GG", "FETCH_HH", "FETCH_II", "FETCH_JJ", "FETCH_KK", "FETCH_LL",
/* Assignment */
	"STORE_A", "STORE_B", "STORE_C", "STORE_D", "STORE_E", "STORE_F",
	"STORE_G", "STORE_H", "STORE_I", "STORE_J", "STORE_K", "STORE_L",
	"STORE_M", "STORE_N", "STORE_O", "STORE_P",
	"STORE_AA", "STORE_BB", "STORE_CC", "STORE_DD", "STORE_EE", "STORE_FF",
	"STORE_GG", "STORE_HH", "STORE_II", "STORE_JJ", "STORE_KK", "STORE_LL",
/* Trigonometry Constants */
	"CONST_PI", /* 61 */
	"CONST_D2R",
	"CONST_R2D",
/* Arithmetic */
	"UNARY_NEG",
	"ADD",
	"SUB",
	"MULT",
	"DIV",
	"MODULO",
	"POWER",
/* Algebraic */
	"ABS_VAL",
	"EXP",
	"LOG_10",
	"LOG_E",
	"MAX",
	"MIN",
	"SQU_RT",
/* Trigonometric */
	"ACOS", /* 78 */
	"ASIN",
	"ATAN",
	"ATAN2",
	"COS",
	"COSH",
	"SIN",
	"SINH",
	"TAN",
	"TANH",
/* Numeric */
	"CEIL", /* 88 */
	"FLOOR",
	"FINITE",
	"ISINF",
	"ISNAN",
	"NINT",
	"RANDOM",
/* Boolean */
	"REL_OR",
	"REL_AND",
	"REL_NOT",
/* Bitwise */
	"BIT_OR",
	"BIT_AND",
	"BIT_EXCL_OR",
	"BIT_NOT",
	"RIGHT_SHIFT",
	"LEFT_SHIFT",
/* Relationals */
	"NOT_EQ", /* 104 */
	"LESS_THAN",
	"LESS_OR_EQ",
	"EQUAL",
	"GR_OR_EQ",
	"GR_THAN",
/* Conditional */
	"COND_IF", /* 110 */
	"COND_ELSE",
	"COND_END",
/* Misc */
	"NOT_GENERATED",
/* array calc stuff */
	"CONST_S2R", /* 114 */
	"CONST_R2S",
	"CONST_IX",
	"LOG_2",
	"PAREN",
	"MAX_VAL", /* >? */
	"MIN_VAL", /* <? */
	"COMMA",
	"TO_DOUBLE",
	"SUBRANGE",
	"SUBRANGE_IP",
	"TO_ARRAY",
	"A_FETCH",
	"A_AFETCH",
	"LEN",
	"NORMAL_RNDM",
	"A_STORE",
	"A_ASTORE",
	"UNTIL",
	"UNTIL_END",
	"AVERAGE",
	"STD_DEV",
	"FWHM",
	"SMOOTH",
	"NSMOOTH",
	"DERIV",
	"NDERIV",
	"ARRSUM",
	"AMAX",
	"AMIN",
	"FITPOLY",
	"FITMPOLY",
	"ARANDOM",
	"CUM",
	"IXMAX",
	"IXMIN",
	"IXZ",
	"IXNZ",
	"FITQ",
	"FITMQ",
	"CAT"
};

/*
 * aCalcPostFix
 *
 * convert an infix expression to a postfix expression
 */
epicsShareFunc long
	aCalcPostfix(const char *psrc, unsigned char * const ppostfix, short *perror)
{
	ELEMENT stack[80];
	ELEMENT *pstacktop = stack, *ps1;
	const ELEMENT *pel;
	int operand_needed = TRUE;
	int runtime_depth = 0;
	int cond_count = 0;
	unsigned char *pout = ppostfix;
	char *pnext;
	double lit_d;
	int lit_i;
	int handled;

#if DEBUG
	if (aCalcPostfixDebug) printf("aCalcPostfix: entry\n");
#endif

	if (psrc == NULL || pout == NULL || perror == NULL) {
		if (perror) *perror = CALC_ERR_NULL_ARG;
		if (pout) *pout = END_EXPRESSION;
		return -1;
	}
	if (*psrc == '\0') {
		if (pout) *pout = END_EXPRESSION;
		return 0;
	}

	/* place the expression elements into postfix */
	*pout = END_EXPRESSION;
	*perror = CALC_ERR_NONE;

	while (get_element(operand_needed, &psrc, &pel)) {
		if (aCalcPostfixDebug) printf("\tget_element:%s (%s) runtime_depth=%d\n",
			opcodes[(int) pel->code], pel->name, runtime_depth);

		switch (pel->type) {

		case OPERAND:
			*pout++ = pel->code;
			runtime_depth += pel->runtime_effect;
			operand_needed = FALSE;
			break;

		case LITERAL_OPERAND:
			runtime_depth += pel->runtime_effect;

			psrc -= strlen(pel->name);
			lit_d = epicsStrtod(psrc, &pnext);
			if (pnext == psrc) {
				if (aCalcPostfixDebug) printf("***LITERAL_OPERAND***\n");
				*perror = CALC_ERR_BAD_LITERAL;
				goto bad;
			}
			psrc = pnext;
			lit_i = lit_d;
			if (lit_d != (double) lit_i) {
				*pout++ = pel->code;
				memcpy(pout, (void *)&lit_d, sizeof(double));
				pout += sizeof(double);
			} else {
				*pout++ = LITERAL_INT;
				memcpy(pout, (void *)&lit_i, sizeof(int));
				pout += sizeof(int);
			}

			operand_needed = FALSE;
			break;

		case STORE_OPERATOR:
			handled = 0;
			/* search stack for A_FETCH (@) or A_SFETCH (@@) */
			for (ps1=pstacktop; ps1>stack; ps1--) {
				if (aCalcPostfixDebug) printf("STORE_OPERATOR:stacktop code=%s (%d)\n",
					opcodes[(int) ps1->code], ps1->code);
				if ((ps1->code == A_FETCH) || (ps1->code == A_AFETCH)) break;
			}
			if (ps1->code == A_FETCH) {
				handled = 1;
				*ps1 = *pel; ps1->code = A_STORE;
			} else if (ps1->code == A_AFETCH) {
				handled = 1;
				*ps1 = *pel; ps1->code = A_ASTORE;
			}

			if (handled) {
				/* Move operators of >= priority to the output, but stop before ps1 */
				while ((pstacktop > ps1) && (pstacktop > stack) &&
					   (pstacktop->in_stack_pri >= pel->in_coming_pri)) {
					*pout++ = pstacktop->code;
					if (aCalcPostfixDebug>=5) printf("put %s to postfix\n", opcodes[(int) pstacktop->code]);
					if (pstacktop->type == VARARG_OPERATOR) {
						*pout++ = 1 - pstacktop->runtime_effect;
						if (aCalcPostfixDebug>=5) printf("put run-time effect %d to postfix\n", 1 - pstacktop->runtime_effect);
					}
					runtime_depth += pstacktop->runtime_effect;
					pstacktop--;
				}
			} else {
				/* convert FETCH_x or FETCH_xx (already posted to postfix string) */
				if (pout > ppostfix && pout[-1] >= FETCH_A && pout[-1] <= FETCH_P) {
					/* Convert fetch into a store on the stack */
					pout--;
					*++pstacktop = *pel;
					pstacktop->code = STORE_A + *pout - FETCH_A;
				} else if (pout > ppostfix && pout[-1] >= FETCH_AA && pout[-1] <= FETCH_LL) {
					pout--;
					*++pstacktop = *pel;
					pstacktop->code = STORE_AA + *pout - FETCH_AA;
				} else {
					if (aCalcPostfixDebug) printf("***STORE_OPERATOR***\n");
					*perror = CALC_ERR_BAD_ASSIGNMENT;
					goto bad;
				}
			}
			runtime_depth -= 1;
			operand_needed = TRUE;
			break;

		case UNARY_OPERATOR:
		case VARARG_OPERATOR:
			/* Move operators of >= priority to the output */
			while ((pstacktop > stack) &&
				   (pstacktop->in_stack_pri >= pel->in_coming_pri)) {
				*pout++ = pstacktop->code;
				if (aCalcPostfixDebug) printf("UNARY/VARARG op '%s': moved '%s' from stack\n", pel->name, pstacktop->name);
				if (pstacktop->type == VARARG_OPERATOR) {
					*pout++ = 1 - pstacktop->runtime_effect;
				}
				runtime_depth += pstacktop->runtime_effect;
				pstacktop--;
			}

			/* Push new operator onto stack */
			pstacktop++;
			*pstacktop = *pel;
			break;

		case BINARY_OPERATOR:
			/* Move operators of >= priority to the output */
			while ((pstacktop > stack) &&
				   (pstacktop->in_stack_pri >= pel->in_coming_pri)) {
				*pout++ = pstacktop->code;
				if (aCalcPostfixDebug) printf("BINARY op '%s': moved '%s' from stack\n", pel->name, pstacktop->name);
				if (pstacktop->type == VARARG_OPERATOR) {
					*pout++ = 1 - pstacktop->runtime_effect;
				}
				runtime_depth += pstacktop->runtime_effect;
				pstacktop--;
			}

			/* Push new operator onto stack */
			pstacktop++;
			*pstacktop = *pel;

			operand_needed = TRUE;
			break;

		case SEPARATOR:
			/* Move operators to the output until open paren or bracket or curly */
			while ((pstacktop->name[0] != '(') && (pstacktop->name[0] != '[')
					&& (pstacktop->name[0] != '{')) {
				if (pstacktop <= stack+1) {
					if (aCalcPostfixDebug) printf("***SEPARATOR***\n");
					*perror = CALC_ERR_BAD_SEPARATOR;
					goto bad;
				}
				*pout++ = pstacktop->code;
				if (pstacktop->type == VARARG_OPERATOR) {
					*pout++ = 1 - pstacktop->runtime_effect;
				}
				runtime_depth += pstacktop->runtime_effect;
				pstacktop--;
			}
			operand_needed = TRUE;
			pstacktop->runtime_effect -= 1;
			break;

		case CLOSE_PAREN:
			/* Move operators to the output until matching paren */
			while (pstacktop->name[0] != '(') {
				if (pstacktop <= stack+1) {
					if (aCalcPostfixDebug) printf("***CLOSE_PAREN***\n");
					*perror = CALC_ERR_PAREN_NOT_OPEN;
					goto bad;
				}
				*pout++ = pstacktop->code;
				if (pstacktop->type == VARARG_OPERATOR) {
					*pout++ = 1 - pstacktop->runtime_effect;
				}
				runtime_depth += pstacktop->runtime_effect;
				pstacktop--;
			}
			pstacktop--;	/* remove ( from stack */
			/* if there is a vararg operator before the opening paren,
			   it inherits the (opening) paren's stack effect */
			if ((pstacktop > stack) &&
				pstacktop->type == VARARG_OPERATOR) {
				pstacktop->runtime_effect = (pstacktop+1)->runtime_effect;
				/* check for no arguments */
				if (pstacktop->runtime_effect > 0) {
					if (aCalcPostfixDebug) printf("***CLOSE_PAREN_1***\n");
					*perror = CALC_ERR_INCOMPLETE;
					goto bad;
				}
			}
			break;

		case CLOSE_BRACKET:
			if (aCalcPostfixDebug) printf("CLOSE_BRACKET: \n");
			/* Move operators to the output until matching paren */
			while (pstacktop->name[0] != '[') {
				if (aCalcPostfixDebug) printf("CLOSE_BRACKET: stacktop code=%s (%d)\n",
					opcodes[(int) pstacktop->code], pstacktop->code);
				if (pstacktop <= stack+1) {
					if (aCalcPostfixDebug) printf("***CLOSE_BRACKET***\n");
					*perror = CALC_ERR_BRACKET_NOT_OPEN;
					goto bad;
				}
				*pout++ = pstacktop->code;
				if (pstacktop->type == VARARG_OPERATOR) {
					*pout++ = 1 - pstacktop->runtime_effect;
				}
				runtime_depth += pstacktop->runtime_effect;
				pstacktop--;
			}
			/* add SUBRANGE operator to postfix */
			*pout++ = pstacktop->code;
			runtime_depth += pstacktop->runtime_effect;
			pstacktop--;

			if (aCalcPostfixDebug) printf("CLOSE_BRACKET: stacktop code=%s (%d)\n",
					opcodes[(int) pstacktop->code], pstacktop->code);
			break;

		case CLOSE_CURLY:
			/* Move operators to the output until matching paren */
			while (pstacktop->name[0] != '{') {
				if (pstacktop <= stack+1) {
					if (aCalcPostfixDebug) printf("***CLOSE_CURLY***\n");
					*perror = CALC_ERR_CURLY_NOT_OPEN;
					goto bad;
				}
				*pout++ = pstacktop->code;
				if (aCalcPostfixDebug) printf("CLOSE_CURLY op '%s': moved '%s' from stack\n", pel->name, pstacktop->name);
				if (pstacktop->type == VARARG_OPERATOR) {
					*pout++ = 1 - pstacktop->runtime_effect;
				}
				runtime_depth += pstacktop->runtime_effect;
				pstacktop--;
			}
			/* add REPLACE operator to postfix */
			*pout++ = pstacktop->code;
			runtime_depth += pstacktop->runtime_effect;
			pstacktop--;
			break;

		case CONDITIONAL:
			/* Move operators of > priority to the output */
			while ((pstacktop > stack) &&
				   (pstacktop->in_stack_pri > pel->in_coming_pri)) {
				*pout++ = pstacktop->code;
				if (aCalcPostfixDebug) printf("CONDITIONAL op '%s': moved '%s' from stack\n", pel->name, pstacktop->name);
				if (pstacktop->type == VARARG_OPERATOR) {
					*pout++ = 1 - pstacktop->runtime_effect;
				}
				runtime_depth += pstacktop->runtime_effect;
				pstacktop--;
			}

			/* Add new element to the output */
			*pout++ = pel->code;
			runtime_depth += pel->runtime_effect;

			/* For : operator, also push COND_END code to stack */
			if (pel->name[0] == ':') {
				if (--cond_count < 0) {
					if (aCalcPostfixDebug) printf("***CONDITIONAL(:)***\n");
					*perror = CALC_ERR_CONDITIONAL;
					goto bad;
				}
				pstacktop++;
				*pstacktop = *pel;
				pstacktop->code = COND_END;
				pstacktop->runtime_effect = 0;
			} else {
				cond_count++;
			}

			operand_needed = TRUE;
			break;

		case UNTIL_OPERATOR:
			/* Move operators of >= priority to the output */
			while ((pstacktop > stack) &&
				   (pstacktop->in_stack_pri >= pel->in_coming_pri)) {
				*pout++ = pstacktop->code;
				if (aCalcPostfixDebug) printf("UNTIL_OPERATOR op '%s': moved '%s' from stack\n", pel->name, pstacktop->name);
				if (pstacktop->type == VARARG_OPERATOR) {
					*pout++ = 1 - pstacktop->runtime_effect;
				}
				runtime_depth += pstacktop->runtime_effect;
				pstacktop--;
			}

			/* Push UNTIL to output */
			*pout++ = pel->code;
			runtime_depth += pel->runtime_effect;

			/* Push UNTIL_END code to stack */
			pstacktop++;
			*pstacktop = *pel;
			pstacktop->code = UNTIL_END;
			pstacktop->runtime_effect = 0;
			break;

		case EXPR_TERMINATOR:
			/* Move everything from stack to the output */
			while ((pstacktop > stack) && (pstacktop->name[0] != '(')) {
				*pout++ = pstacktop->code;
				if (aCalcPostfixDebug) printf("EXPR_TERMINATOR op '%s': moved '%s' from stack\n", pel->name, pstacktop->name);
				if (pstacktop->type == VARARG_OPERATOR) {
					*pout++ = 1 - pstacktop->runtime_effect;
				}
				runtime_depth += pstacktop->runtime_effect;
				pstacktop--;
			}
			operand_needed = TRUE;
			break;

		default:
			if (aCalcPostfixDebug) printf("***default***\n");
			*perror = CALC_ERR_INTERNAL;
			goto bad;
		}

		if (runtime_depth < 0) {
			if (aCalcPostfixDebug) printf("***runtime_depth<0***\n");
			*perror = CALC_ERR_UNDERFLOW;
			goto bad;
		}
		if (runtime_depth >= ACALC_STACKSIZE) {
			if (aCalcPostfixDebug) printf("***runtime_depth>=ACALC_STACKSIZE***\n");
			*perror = CALC_ERR_OVERFLOW;
			goto bad;
		}
	}

	if (*psrc != '\0') {
		if (aCalcPostfixDebug) printf("*** *psrc != 0 ***\n");
		*perror = CALC_ERR_SYNTAX;
		goto bad;
	}

	/* Move everything from stack to the output */
	while (pstacktop > stack) {
		if (pstacktop->name[0] == '(') {
			if (aCalcPostfixDebug) printf("*** pstacktop->name[0] == ( ***\n");
			*perror = CALC_ERR_PAREN_OPEN;
			goto bad;
		}
		*pout++ = pstacktop->code;
		if (aCalcPostfixDebug) printf("done parsing: moved '%s' from stack\n", pstacktop->name);
		if (pstacktop->type == VARARG_OPERATOR) {
			*pout++ = 1 - pstacktop->runtime_effect;
		}
		runtime_depth += pstacktop->runtime_effect;
		pstacktop--;
	}
	*pout = END_EXPRESSION;

	if (cond_count != 0) {
		if (aCalcPostfixDebug) printf("*** cond_count != 0 ***\n");
		*perror = CALC_ERR_CONDITIONAL;
		goto bad;
	}
	if (operand_needed) {
		if (aCalcPostfixDebug) printf("*** operand_needed ***\n");
		*perror = CALC_ERR_INCOMPLETE;
		goto bad;
	}
	if (runtime_depth != 1) {
		if (aCalcPostfixDebug) printf("*** runtime_depth!=1 (==%d) ***\n", runtime_depth);
		*perror = CALC_ERR_INCOMPLETE;
		goto bad;
	}
	if (aCalcPostfixDebug) printf("\naCalcPostfix: returning success\n");
	return 0;

bad:
	if (aCalcPostfixDebug) {
		printf("\n***error*** '%s'\n", aCalcErrorStr(*perror));
		aCalcExprDump(ppostfix);
	}
	*ppostfix = END_EXPRESSION;
	return -1;
}

/* aCalcErrorStr
 *
 * Return a message string appropriate for the given error code
 */
epicsShareFunc const char *
	aCalcErrorStr(short error)
{
	static const char *errStrs[] = {
		"No error",
		"Too many results returned",
		"Badly formed numeric literal",
		"Bad assignment target",
		"Comma without enclosing parentheses",
		"Close parenthesis found without open",
		"Parenthesis still open at end of expression",
		"Unbalanced conditional ?: operators",
		"Incomplete expression, operand missing",
		"Not enough operands provided",
		"Runtime stack would overflow",
		"Syntax error, unknown operator/operand",
		"NULL or empty input argument to postfix()",
		"Internal error, unknown element type",
		"Close bracket without open",
		"Close curly bracket without open"
	};
	
	if (error < CALC_ERR_NONE || error > CALC_ERR_INTERNAL)
		return NULL;
	return errStrs[error];
}

/* aCalcExprDump
 *
 * Disassemble the given postfix instructions to stdout
 */
epicsShareFunc void
	aCalcExprDump(const unsigned char *pinst)
{
	unsigned char op;
	double lit_d;
	int lit_i;
	
	while ((op = *pinst) != END_EXPRESSION) {
		switch (op) {
		case LITERAL_DOUBLE:
			memcpy((void *)&lit_d, ++pinst, sizeof(double));
			printf("\tDouble %g\n", lit_d);
			pinst += sizeof(double);
			break;
		case LITERAL_INT:
			memcpy((void *)&lit_i, ++pinst, sizeof(int));
			printf("\tInteger %d\n", lit_i);
			pinst += sizeof(int);
			break;
		case MIN:
		case MAX:
		case FINITE:
		case ISNAN:
			printf("\t%s, %d arg(s)\n", opcodes[(int) op], *++pinst);
			pinst++;
			break;
		default:
			printf("\t%s (%d)\n", opcodes[(int) op], op);
			pinst++;
		}
	}
}

void aCalcPrintOp(unsigned char op) {
	printf("%s\n", opcodes[(int) op]);
}
