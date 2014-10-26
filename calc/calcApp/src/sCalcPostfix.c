/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id: sCalcPostfix.c,v 1.19 2009-09-09 16:39:34 mooney Exp $
 * Subroutines used to convert an infix expression to a postfix expression
 *
 *      Original Author: Bob Dalesio
 *      Date:            12-12-86
 */

#include	<stdio.h>
#include	<string.h>
#include	<ctype.h>

#include	<dbDefs.h>
#include	<epicsStdlib.h>
#include	<epicsString.h>
#define epicsExportSharedSymbols
#include	"sCalcPostfix.h"
#include	"sCalcPostfixPvt.h"
#include	<epicsExport.h>


long test_sCalcPostfix(char *pinfix, int n);
long test_sCalcPerform(char *pinfix, int n);

#define DEBUG 1
volatile int sCalcPostfixDebug=0;
epicsExportAddress(int, sCalcPostfixDebug);

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
	STRING_OPERAND,
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
	sCalc_rpn_opcode code;	 /* postfix opcode */
} ELEMENT;

/*
 * NOTE: DO NOT CHANGE WITHOUT READING THIS NOTICE !!!!!!!!!!!!!!!!!!!!
 * Because the routine that looks for a match in this table takes the first 
 * match it finds, elements whose designations are contained in other elements
 * MUST come first in this list. (e.g. ABS will match A if A preceeds ABS and
 * then try to find BS.  Therefore ABS must be first in this list.)
 * ':' receives special handling, so if you add an operator that includes
 * ':', you must modify that special handling.
 */

/* comparison of operator precedence here and in EPICS base:
 * sCalcPostfix									postfix
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
/* name		prio's	stack	element type		opcode */
{"\"",		0, 0,	1,		STRING_OPERAND,		LITERAL_STRING},    /* string constant */
{"'",		0, 0,	1,		STRING_OPERAND,		LITERAL_STRING},    /* string constant */
{"@",		9, 10,	0,		UNARY_OPERATOR,		A_FETCH},     /* fetch numeric argument */
{"@@",		9, 10,	0,		UNARY_OPERATOR,		A_SFETCH},    /* fetch string argument */
{"!",		9, 10,	0,		UNARY_OPERATOR,		REL_NOT},
{"(",		0, 10,	0,		UNARY_OPERATOR,		NOT_GENERATED},
{"-",		9, 10,	0,		UNARY_OPERATOR,		UNARY_NEG},
{".",		0, 0,	1,		LITERAL_OPERAND,	LITERAL_DOUBLE},
{"0",		0, 0,	1,		LITERAL_OPERAND,	LITERAL_DOUBLE},
{"1",		0, 0,	1,		LITERAL_OPERAND,	LITERAL_DOUBLE},
{"2",		0, 0,	1,		LITERAL_OPERAND,	LITERAL_DOUBLE},
{"3",		0, 0,	1,		LITERAL_OPERAND,	LITERAL_DOUBLE},
{"4",		0, 0,	1,		LITERAL_OPERAND,	LITERAL_DOUBLE},
{"5",		0, 0,	1,		LITERAL_OPERAND,	LITERAL_DOUBLE},
{"6",		0, 0,	1,		LITERAL_OPERAND,	LITERAL_DOUBLE},
{"7",		0, 0,	1,		LITERAL_OPERAND,	LITERAL_DOUBLE},
{"8",		0, 0,	1,		LITERAL_OPERAND,	LITERAL_DOUBLE},
{"9",		0, 0,	1,		LITERAL_OPERAND,	LITERAL_DOUBLE},
{"A",		0, 0,	1,		OPERAND,			FETCH_A},
{"AA",		0, 0,	1,		OPERAND,			FETCH_AA},
{"ABS",		9, 10,	0,		UNARY_OPERATOR,		ABS_VAL},
{"ACOS",	9, 10,	0,		UNARY_OPERATOR,		ACOS},
{"ASIN",	9, 10,	0,		UNARY_OPERATOR,		ASIN},
{"ATAN",	9, 10,	0,		UNARY_OPERATOR,		ATAN},
{"ATAN2",	9, 10,	-1,		UNARY_OPERATOR,		ATAN2},
{"B",		0, 0,	1,		OPERAND,			FETCH_B},
{"BB",		0, 0,	1,		OPERAND,			FETCH_BB},
{"BYTE",	9, 10,	0,		UNARY_OPERATOR,		BYTE},        /* string[0] to byte */
{"C",		0, 0,	1,		OPERAND,			FETCH_C},
{"CC",		0, 0,	1,		OPERAND,			FETCH_CC},
{"CEIL",	9, 10,	0,		UNARY_OPERATOR,		CEIL},
{"COS",		9, 10,	0,		UNARY_OPERATOR,		COS},
{"COSH",	9, 10,	0,		UNARY_OPERATOR,		COSH},
{"D",		0, 0,	1,		OPERAND,			FETCH_D},
{"DD",		0, 0,	1,		OPERAND,			FETCH_DD},
{"DBL",		9, 10,	0,		UNARY_OPERATOR,		TO_DOUBLE},   /* convert to double */
{"D2R",		0, 0,	1,		OPERAND,			CONST_D2R},
{"E",		0, 0,	1,		OPERAND,			FETCH_E},
{"EE",		0, 0,	1,		OPERAND,			FETCH_EE},
{"$E",		9, 10,	0,		UNARY_OPERATOR,		ESC},         /* translate escape */
{"ESC",		9, 10,	0,		UNARY_OPERATOR,		ESC},         /* translate escape */
{"EXP",		9, 10,	0,		UNARY_OPERATOR,		EXP},
{"F",		0, 0,	1,		OPERAND,			FETCH_F},
{"FF",		0, 0,	1,		OPERAND,			FETCH_FF},
{"FINITE",	9, 10,	0,		VARARG_OPERATOR,	FINITE},
{"FLOOR",	9, 10,	0,		UNARY_OPERATOR,		FLOOR},
{"G",		0, 0,	1,		OPERAND,			FETCH_G},
{"GG",		0, 0,	1,		OPERAND,			FETCH_GG},
{"H",		0, 0,	1,		OPERAND,			FETCH_H},
{"HH",		0, 0,	1,		OPERAND,			FETCH_HH},
{"I",		0, 0,	1,		OPERAND,			FETCH_I},
{"II",		0, 0,	1,		OPERAND,			FETCH_II},
{"INF",		0, 0,	1,		LITERAL_OPERAND,	LITERAL_DOUBLE},
{"INT",		9, 10,	0,		UNARY_OPERATOR,		NINT},
{"ISINF",	9, 10,	0,		UNARY_OPERATOR,		ISINF},
{"ISNAN",	9, 10,	0,		VARARG_OPERATOR,	ISNAN},
{"J",		0, 0,	1,		OPERAND,			FETCH_J},
{"JJ",		0, 0,	1,		OPERAND,			FETCH_JJ},
{"K",		0, 0,	1,		OPERAND,			FETCH_K},
{"KK",		0, 0,	1,		OPERAND,			FETCH_KK},
{"L",		0, 0,	1,		OPERAND,			FETCH_L},
{"LL",		0, 0,	1,		OPERAND,			FETCH_LL},
{"LN",		9, 10,	0,		UNARY_OPERATOR,		LOG_E},
{"LOG",		9, 10,	0,		UNARY_OPERATOR,		LOG_10},
{"LOGE",	9, 10,	0,		UNARY_OPERATOR,		LOG_E},
{"M",		0, 0,	1,		OPERAND,			FETCH_M},
{"MAX",		9, 10,	0,		VARARG_OPERATOR,	MAX},
{"MIN",		9, 10,	0,		VARARG_OPERATOR,	MIN},
{"N",		0, 0,	1,		OPERAND,			FETCH_N},
{"NINT",	9, 10,	0,		UNARY_OPERATOR,		NINT},
{"NAN",		0, 0,	1,		LITERAL_OPERAND,	LITERAL_DOUBLE},
{"NOT",		9, 10,	0,		UNARY_OPERATOR,		BIT_NOT},
{"NRNDM",	0, 0,	1,		OPERAND,			NORMAL_RNDM},   /* Normally Distributed Random Number */
{"O",		0, 0,	1,		OPERAND,			FETCH_O},
{"P",		0, 0,	1,		OPERAND,			FETCH_P},
{"PI",		0, 0,	1,		OPERAND,			CONST_PI},
{"$P",		9, 10,	-1,		UNARY_OPERATOR,		PRINTF},      /* formatted print to string */
{"PRINTF",	9, 10,	-1,		UNARY_OPERATOR,		PRINTF},      /* formatted print to string */
{"R2D",		0, 0,	1,		OPERAND,			CONST_R2D},
{"R2S",		0, 0,	1,		OPERAND,			CONST_R2S},
{"$R",		9, 10,	-1,		UNARY_OPERATOR,		BIN_READ},    /* binary read from raw string */
{"READ",	9, 10,	-1,		UNARY_OPERATOR,		BIN_READ},    /* binary read from raw string */
{"RNDM",	0, 0,	1,		OPERAND,			RANDOM},
{"SIN",		9, 10,	0,		UNARY_OPERATOR,		SIN},
{"SINH",	9, 10,	0,		UNARY_OPERATOR,		SINH},
{"SQR",		9, 10,	0,		UNARY_OPERATOR,		SQU_RT},
{"SQRT",	9, 10,	0,		UNARY_OPERATOR,		SQU_RT},
{"S2R",		0, 0,	1,		OPERAND,			CONST_S2R},
{"$S",		9, 10,	-1,		UNARY_OPERATOR,		SSCANF},      /* scan string argument */
{"SSCANF",	9, 10,	-1,		UNARY_OPERATOR,		SSCANF},      /* scan string argument */
{"STR",		9, 10,	0,		UNARY_OPERATOR,		TO_STRING},   /* convert to string */
{"SVAL",	0, 0,	1,		OPERAND,			FETCH_SVAL},
{"TAN",		9, 10,	0,		UNARY_OPERATOR,		TAN},
{"TANH",	9, 10,	0,		UNARY_OPERATOR,		TANH},
{"$T",		9, 10,	0,		UNARY_OPERATOR,		TR_ESC},      /* translate escape */
{"TR_ESC",	9, 10,	0,		UNARY_OPERATOR,		TR_ESC},      /* translate escape */
{"VAL",		0, 0,	1,		OPERAND,			FETCH_VAL},
{"$W",		9, 10,	-1,		UNARY_OPERATOR,		BIN_WRITE},   /* binary write to string */
{"WRITE",	9, 10,	-1,		UNARY_OPERATOR,		BIN_WRITE},   /* binary write to string */
{"CRC16",	9, 10,	0,		UNARY_OPERATOR,		CRC16},       /* CRC16 */
{"MODBUS",	9, 10,	0,		UNARY_OPERATOR,		MODBUS},      /* MODBUS (append CRC16) */
{"LRC",		9, 10,	0,		UNARY_OPERATOR,		LRC},         /* LRC */
{"AMODBUS",	9, 10,	0,		UNARY_OPERATOR,		AMODBUS},     /* Ascii Modbus (append LRC) */
{"XOR8",	9, 10,	0,		UNARY_OPERATOR,		XOR8},        /* XOR8 checksum */
{"ADD_XOR8",9, 10,	0,		UNARY_OPERATOR,		ADD_XOR8},    /* Append XOR8 to string */
{"LEN",		9, 10,	0,		UNARY_OPERATOR,		LEN},         /* String length */
{"UNTIL",	0, 10,	0,		UNTIL_OPERATOR,		UNTIL},
{"~",		9, 10,	0,		UNARY_OPERATOR, 	BIT_NOT},
};

static const ELEMENT operators[] = {
/* name 	prio's	stack	element type		opcode */
{"!=",		5, 5,	-1,		BINARY_OPERATOR,	NOT_EQ},
{"#",		5, 5,	-1,		BINARY_OPERATOR,	NOT_EQ},
{"%",		7, 7,	-1,		BINARY_OPERATOR,	MODULO},
{"&",		3, 3,	-1,		BINARY_OPERATOR,	BIT_AND},
{"&&",		3, 3,	-1,		BINARY_OPERATOR,	REL_AND},
{")",		0, 0,	0,		CLOSE_PAREN,		NOT_GENERATED},
{"[",		0, 11,	-1,		BINARY_OPERATOR,	SUBRANGE},    /* string subrange */
{"{",		0, 11,	-1,		BINARY_OPERATOR,	REPLACE},      /* string replace */
{"]",		0, 0,	0,		CLOSE_BRACKET,		NOT_GENERATED},
{"}",		0, 0,	0,		CLOSE_CURLY,		NOT_GENERATED}, 
{"*",		7, 7,	-1,		BINARY_OPERATOR,	MULT},
{"**",		8, 8,	-1,		BINARY_OPERATOR,	POWER},
{"+",		6, 6,	-1,		BINARY_OPERATOR,	ADD},
{",",		0, 0,	0,		SEPARATOR,			NOT_GENERATED},
{"-",		6, 6,	-1,		BINARY_OPERATOR,	SUB},
{"/",		7, 7,	-1,		BINARY_OPERATOR,	DIV},
{":",		0, 0,	-1,		CONDITIONAL,		COND_ELSE},
{":=",		1, 0,	-1,		STORE_OPERATOR,		STORE_A},
{";",		0, 0,	0,		EXPR_TERMINATOR,	NOT_GENERATED},
{"<",		5, 5,	-1,		BINARY_OPERATOR,	LESS_THAN},
{"<<",		3, 3,	-1,		BINARY_OPERATOR,	LEFT_SHIFT},
{"<=",		5, 5,	-1,		BINARY_OPERATOR,	LESS_OR_EQ},
{"=",		5, 5,	-1,		BINARY_OPERATOR,	EQUAL},
{"==",		5, 5,	-1,		BINARY_OPERATOR,	EQUAL},
{">",		5, 5,	-1,		BINARY_OPERATOR,	GR_THAN},
{">=",		5, 5,	-1,		BINARY_OPERATOR,	GR_OR_EQ},
{">>",		3, 3,	-1,		BINARY_OPERATOR,	RIGHT_SHIFT},
{"?",		0, 0,	-1,		CONDITIONAL,		COND_IF},
{"AND",		3, 3,	-1,		BINARY_OPERATOR,	BIT_AND},
{"OR",		2, 2,	-1,		BINARY_OPERATOR,	BIT_OR},
{"XOR",		2, 2,	-1,		BINARY_OPERATOR,	BIT_EXCL_OR},
{"^",		8, 8,	-1,		BINARY_OPERATOR,	POWER},
{"|",		2, 2,	-1,		BINARY_OPERATOR,	BIT_OR},
{"||",		2, 2,	-1,		BINARY_OPERATOR,	REL_OR},
{"-|",		6, 6,	-1,		BINARY_OPERATOR,	SUB},         /* subtract first occurrence */
{"|-",		6, 6,	-1,		BINARY_OPERATOR,	SUBLAST},     /* subtract last occurrence */
{">?",		4, 4,	-1,		BINARY_OPERATOR,	MAX_VAL},     /* maximum of 2 args */
{"<?",		4, 4,	-1,		BINARY_OPERATOR,	MIN_VAL},     /* minimum of 2 args */
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
	"LITERAL_DOUBLE", "LITERAL_INT", "VAL", "SVAL",
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
/* string calc stuff */
	"CONST_S2R", /* 114 */
	"CONST_R2S",
	"LITERAL_STRING",
	"SSCANF",
	"LOG_2",
	"PAREN",
	"MAX_VAL", /* >? */
	"MIN_VAL", /* <? */
	"COMMA",
	"TO_DOUBLE",
	"PRINTF",
	"SUBRANGE",
	"TO_STRING",
	"REPLACE",
	"A_FETCH",
	"A_SFETCH",
	"BYTE",
	"SUBLAST",
	"TR_ESC",
	"ESC",
	"CRC16",
	"MODBUS",
	"LRC",
	"AMODBUS",
	"XOR8",
	"ADD_XOR8",
	"BIN_READ",
	"BIN_WRITE",
	"LEN",
	"NORMAL_RNDM",
	"A_STORE",
	"A_SSTORE",
	"UNTIL",
	"UNTIL_END",
	"NO_STRING",
	"USES_STRING"
};


/* sCalcPostfix
 *
 * convert an infix expression to a postfix expression
 */
epicsShareFunc long
	sCalcPostfix(const char *psrc, unsigned char * const ppostfix, short *perror)
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
	char c;
	int handled;

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
	*pout++ = NO_STRING;
	*pout = END_EXPRESSION;
	*perror = CALC_ERR_NONE;

	while (get_element(operand_needed, &psrc, &pel)) {

		if (sCalcPostfixDebug) printf("\tget_element:%s (%s) runtime_depth=%d\n",
			opcodes[(int) pel->code], pel->name, runtime_depth);

		if (*ppostfix != USES_STRING) {
			switch (pel->code) {

			case FETCH_AA: case FETCH_BB: case FETCH_CC: case FETCH_DD: case FETCH_EE: case FETCH_FF:
			case FETCH_GG: case FETCH_HH: case FETCH_II: case FETCH_JJ: case FETCH_KK: case FETCH_LL:
			case FETCH_SVAL:
			case TO_STRING:
			case PRINTF:
			case BIN_WRITE:
			case SSCANF:
	 		case BIN_READ:
			case LITERAL_STRING:
			case SUBRANGE:
			case REPLACE:
			case A_SFETCH:
			case TR_ESC:
			case ESC:
			case CRC16:
			case MODBUS:
			case LRC:
			case AMODBUS:
			case XOR8:
			case ADD_XOR8:
			case LEN:
				*ppostfix = USES_STRING;
				break;
			default:
				break;
			}
		}


		switch (pel->type) {

		case OPERAND:
			*pout++ = pel->code;
			if (sCalcPostfixDebug>=5) printf("put %s to postfix\n", opcodes[(int) pel->code]);
			runtime_depth += pel->runtime_effect;
			operand_needed = FALSE;
			break;

		case LITERAL_OPERAND:
			runtime_depth += pel->runtime_effect;

			psrc -= strlen(pel->name);
			lit_d = epicsStrtod(psrc, &pnext);
			if (pnext == psrc) {
				if (sCalcPostfixDebug) printf("***LITERAL_OPERAND***\n");
				*perror = CALC_ERR_BAD_LITERAL;
				goto bad;
			}
			psrc = pnext;
			lit_i = lit_d;
			if (lit_d != (double) lit_i) {
				*pout++ = pel->code;
				if (sCalcPostfixDebug>=5) printf("put %s to postfix\n", opcodes[(int) pel->code]);
				memcpy(pout, (void *)&lit_d, sizeof(double));
				pout += sizeof(double);
			} else {
				*pout++ = LITERAL_INT;
				if (sCalcPostfixDebug>=5) printf("put %s to postfix\n", opcodes[(int) LITERAL_INT]);
				memcpy(pout, (void *)&lit_i, sizeof(int));
				pout += sizeof(int);
			}

			operand_needed = FALSE;
			break;

		case STORE_OPERATOR:
			handled = 0;
			/* search stack for A_FETCH (@) or A_SFETCH (@@) */
			for (ps1=pstacktop; ps1>stack; ps1--) {
				if (sCalcPostfixDebug) printf("STORE_OPERATOR:stacktop code=%s (%d)\n",
					opcodes[(int) ps1->code], ps1->code);
				if ((ps1->code == A_FETCH) || (ps1->code == A_SFETCH)) break;
			}
			if (ps1->code == A_FETCH) {
				handled = 1;
				*ps1 = *pel; ps1->code = A_STORE;
			} else if (ps1->code == A_SFETCH) {
				handled = 1;
				*ps1 = *pel; ps1->code = A_SSTORE;
			}

			if (handled) {
				/* Move operators of >= priority to the output, but stop before ps1 */
				while ((pstacktop > ps1) && (pstacktop > stack) &&
					   (pstacktop->in_stack_pri >= pel->in_coming_pri)) {
					*pout++ = pstacktop->code;
					if (sCalcPostfixDebug>=5) printf("put %s to postfix\n", opcodes[(int) pstacktop->code]);
					if (pstacktop->type == VARARG_OPERATOR) {
						*pout++ = 1 - pstacktop->runtime_effect;
						if (sCalcPostfixDebug>=5) printf("put run-time effect %d to postfix\n", 1 - pstacktop->runtime_effect);
					}
					runtime_depth += pstacktop->runtime_effect;
					pstacktop--;
				}
			} else {
				/* convert FETCH_x or FETCH_xx (already posted to postfix string) */
				if (pout > ppostfix && pout[-1] >= FETCH_A && pout[-1] <= FETCH_P) {
					if (sCalcPostfixDebug) printf("STORE_OPERATOR:pout[-1] is a scalar fetch\n");
					/* Convert fetch into a store on the stack */
					pout--;
					if (sCalcPostfixDebug>=5) printf("retracted %s from postfix\n", opcodes[(int) pout[-1]]);
					*++pstacktop = *pel;
					pstacktop->code = STORE_A + *pout - FETCH_A;
				} else if (pout > ppostfix && pout[-1] >= FETCH_AA && pout[-1] <= FETCH_LL) {
					if (sCalcPostfixDebug) printf("STORE_OPERATOR:pout[-1] is a string fetch\n");
					pout--;
					if (sCalcPostfixDebug>=5) printf("retracted %s from postfix\n", opcodes[(int) pout[-1]]);
					*++pstacktop = *pel;
					pstacktop->code = STORE_AA + *pout - FETCH_AA;
				} else {
					if (sCalcPostfixDebug) printf("***STORE_OPERATOR***\n");
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
				if (sCalcPostfixDebug>=5) printf("put %s to postfix\n", opcodes[(int) pstacktop->code]);
				if (sCalcPostfixDebug) printf("UNARY/VARARG op '%s': moved '%s' from stack\n", pel->name, pstacktop->name);
				if (pstacktop->type == VARARG_OPERATOR) {
					*pout++ = 1 - pstacktop->runtime_effect;
					if (sCalcPostfixDebug>=5) printf("put run-time effect %d to postfix\n", 1 - pstacktop->runtime_effect);
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
				if (sCalcPostfixDebug>=5) printf("put %s to postfix\n", opcodes[(int) pstacktop->code]);
				if (sCalcPostfixDebug) printf("BINARY op '%s': moved '%s' from stack\n", pel->name, pstacktop->name);
				if (pstacktop->type == VARARG_OPERATOR) {
					*pout++ = 1 - pstacktop->runtime_effect;
					if (sCalcPostfixDebug>=5) printf("put run-time effect %d to postfix\n", 1 - pstacktop->runtime_effect);
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
					if (sCalcPostfixDebug) printf("***SEPARATOR***\n");
					*perror = CALC_ERR_BAD_SEPARATOR;
					goto bad;
				}
				*pout++ = pstacktop->code;
				if (sCalcPostfixDebug>=5) printf("put %s to postfix\n", opcodes[(int) pstacktop->code]);
				if (pstacktop->type == VARARG_OPERATOR) {
					*pout++ = 1 - pstacktop->runtime_effect;
					if (sCalcPostfixDebug>=5) printf("put run-time effect %d to postfix\n", 1 - pstacktop->runtime_effect);
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
					if (sCalcPostfixDebug) printf("***CLOSE_PAREN***\n");
					*perror = CALC_ERR_PAREN_NOT_OPEN;
					goto bad;
				}
				*pout++ = pstacktop->code;
				if (sCalcPostfixDebug>=5) printf("put %s to postfix\n", opcodes[(int) pstacktop->code]);
				if (pstacktop->type == VARARG_OPERATOR) {
					*pout++ = 1 - pstacktop->runtime_effect;
					if (sCalcPostfixDebug>=5) printf("put run-time effect %d to postfix\n", 1 - pstacktop->runtime_effect);
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
					if (sCalcPostfixDebug) printf("***CLOSE_PAREN_1***\n");
					*perror = CALC_ERR_INCOMPLETE;
					goto bad;
				}
			}
			break;

		case CLOSE_BRACKET:
			if (sCalcPostfixDebug) printf("CLOSE_BRACKET: \n");
			/* Move operators to the output until matching paren */
			while (pstacktop->name[0] != '[') {
				if (sCalcPostfixDebug) printf("CLOSE_BRACKET: stacktop code=%s (%d)\n",
					opcodes[(int) pstacktop->code], pstacktop->code);
				if (pstacktop <= stack+1) {
					if (sCalcPostfixDebug) printf("***CLOSE_BRACKET***\n");
					*perror = CALC_ERR_BRACKET_NOT_OPEN;
					goto bad;
				}
				*pout++ = pstacktop->code;
				if (sCalcPostfixDebug>=5) printf("put %s to postfix\n", opcodes[(int) pstacktop->code]);
				if (pstacktop->type == VARARG_OPERATOR) {
					*pout++ = 1 - pstacktop->runtime_effect;
					if (sCalcPostfixDebug>=5) printf("put run-time effect %d to postfix\n", 1 - pstacktop->runtime_effect);
				}
				runtime_depth += pstacktop->runtime_effect;
				pstacktop--;
			}
			/* add SUBRANGE operator to postfix */
			*pout++ = pstacktop->code;
			if (sCalcPostfixDebug>=5) printf("put %s to postfix\n", opcodes[(int) pstacktop->code]);
			runtime_depth += pstacktop->runtime_effect;
			pstacktop--;

			if (sCalcPostfixDebug) printf("CLOSE_BRACKET: stacktop code=%s (%d)\n",
					opcodes[(int) pstacktop->code], pstacktop->code);
			break;

		case CLOSE_CURLY:
			/* Move operators to the output until matching paren */
			while (pstacktop->name[0] != '{') {
				if (pstacktop <= stack+1) {
					if (sCalcPostfixDebug) printf("***CLOSE_CURLY***\n");
					*perror = CALC_ERR_CURLY_NOT_OPEN;
					goto bad;
				}
				*pout++ = pstacktop->code;
				if (sCalcPostfixDebug>=5) printf("put %s to postfix\n", opcodes[(int) pstacktop->code]);
				if (sCalcPostfixDebug) printf("CLOSE_CURLY op '%s': moved '%s' from stack\n", pel->name, pstacktop->name);
				if (pstacktop->type == VARARG_OPERATOR) {
					*pout++ = 1 - pstacktop->runtime_effect;
					if (sCalcPostfixDebug>=5) printf("put run-time effect %d to postfix\n", 1 - pstacktop->runtime_effect);
				}
				runtime_depth += pstacktop->runtime_effect;
				pstacktop--;
			}
			/* add REPLACE operator to postfix */
			*pout++ = pstacktop->code;
			if (sCalcPostfixDebug>=5) printf("put %s to postfix\n", opcodes[(int) pstacktop->code]);
			runtime_depth += pstacktop->runtime_effect;
			pstacktop--;
			break;

		case CONDITIONAL:
			/* Move operators of > priority to the output */
			while ((pstacktop > stack) &&
				   (pstacktop->in_stack_pri > pel->in_coming_pri)) {
				*pout++ = pstacktop->code;
				if (sCalcPostfixDebug>=5) printf("put %s to postfix\n", opcodes[(int) pstacktop->code]);
				if (sCalcPostfixDebug) printf("CONDITIONAL op '%s': moved '%s' from stack\n", pel->name, pstacktop->name);
				if (pstacktop->type == VARARG_OPERATOR) {
					*pout++ = 1 - pstacktop->runtime_effect;
					if (sCalcPostfixDebug>=5) printf("put run-time effect %d to postfix\n", 1 - pstacktop->runtime_effect);
				}
				runtime_depth += pstacktop->runtime_effect;
				pstacktop--;
			}

			/* Add new element to the output */
			*pout++ = pel->code;
			if (sCalcPostfixDebug>=5) printf("put %s to postfix\n", opcodes[(int) pel->code]);
			runtime_depth += pel->runtime_effect;

			/* For : operator, also push COND_END code to stack */
			if (pel->name[0] == ':') {
				if (--cond_count < 0) {
					if (sCalcPostfixDebug) printf("***CONDITIONAL(:)***\n");
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
				if (sCalcPostfixDebug>=5) printf("put %s to postfix\n", opcodes[(int) pstacktop->code]);
				if (sCalcPostfixDebug) printf("UNTIL_OPERATOR op '%s': moved '%s' from stack\n", pel->name, pstacktop->name);
				if (pstacktop->type == VARARG_OPERATOR) {
					*pout++ = 1 - pstacktop->runtime_effect;
					if (sCalcPostfixDebug>=5) printf("put run-time effect %d to postfix\n", 1 - pstacktop->runtime_effect);
				}
				runtime_depth += pstacktop->runtime_effect;
				pstacktop--;
			}

			/* Push UNTIL to output */
			*pout++ = pel->code;
			if (sCalcPostfixDebug>=5) printf("put %s to postfix\n", opcodes[(int) pel->code]);
			runtime_depth += pel->runtime_effect;

			/* Push UNTIL_END code to stack */
			pstacktop++;
			*pstacktop = *pel;
			pstacktop->code = UNTIL_END;
			pstacktop->runtime_effect = 0;
			break;

		case EXPR_TERMINATOR:
			/* Move everything from stack to the output */
			/* while (pstacktop > stack) { */
			while ((pstacktop > stack) && (pstacktop->name[0] != '(')) {
				*pout++ = pstacktop->code;
				if (sCalcPostfixDebug>=5) printf("put %s to postfix\n", opcodes[(int) pstacktop->code]);
				if (sCalcPostfixDebug) printf("EXPR_TERMINATOR op '%s': moved '%s' from stack\n", pel->name, pstacktop->name);
				if (pstacktop->type == VARARG_OPERATOR) {
					*pout++ = 1 - pstacktop->runtime_effect;
					if (sCalcPostfixDebug>=5) printf("put run-time effect %d to postfix\n", 1 - pstacktop->runtime_effect);
				}
				runtime_depth += pstacktop->runtime_effect;
				pstacktop--;
			}
			operand_needed = TRUE;
			break;

		case STRING_OPERAND:
			runtime_depth += pel->runtime_effect;
			*pout++ = pel->code;
			if (sCalcPostfixDebug>=5) printf("put %s to postfix\n", opcodes[(int) pel->code]);
			c = psrc[-1]; /* " or ' character */
			while (*psrc != c && *psrc) *pout++ = *psrc++;
			*pout++ = '\0';
			if (*psrc) psrc++;
			operand_needed = FALSE;
			break;

		default:
			if (sCalcPostfixDebug) printf("***default***\n");
			*perror = CALC_ERR_INTERNAL;
			goto bad;
		}

		if (runtime_depth < 0) {
			if (sCalcPostfixDebug) printf("***runtime_depth<0***\n");
			*perror = CALC_ERR_UNDERFLOW;
			goto bad;
		}
		if (runtime_depth >= SCALC_STACKSIZE) {
			if (sCalcPostfixDebug) printf("***runtime_depth>=SCALC_STACKSIZE***\n");
			*perror = CALC_ERR_OVERFLOW;
			goto bad;
		}
	}

	if (*psrc != '\0') {
		if (sCalcPostfixDebug) printf("*** *psrc != 0 ***\n");
		*perror = CALC_ERR_SYNTAX;
		goto bad;
	}

	/* Move everything from stack to the output */
	while (pstacktop > stack) {
		if (pstacktop->name[0] == '(') {
			if (sCalcPostfixDebug) printf("*** pstacktop->name[0] == ( ***\n");
			*perror = CALC_ERR_PAREN_OPEN;
			goto bad;
		}
		*pout++ = pstacktop->code;
			if (sCalcPostfixDebug>=5) printf("put %s to postfix\n", opcodes[(int) pstacktop->code]);
		if (sCalcPostfixDebug) printf("done parsing: moved '%s' from stack\n", pstacktop->name);
		if (pstacktop->type == VARARG_OPERATOR) {
			*pout++ = 1 - pstacktop->runtime_effect;
			if (sCalcPostfixDebug>=5) printf("put run-time effect %d to postfix\n", 1 - pstacktop->runtime_effect);
		}
		runtime_depth += pstacktop->runtime_effect;
		pstacktop--;
	}
	*pout = END_EXPRESSION;

	if (cond_count != 0) {
		if (sCalcPostfixDebug) printf("*** cond_count != 0 ***\n");
		*perror = CALC_ERR_CONDITIONAL;
		goto bad;
	}
	if (operand_needed) {
		if (sCalcPostfixDebug) printf("*** operand_needed ***\n");
		*perror = CALC_ERR_INCOMPLETE;
		goto bad;
	}
	if (runtime_depth != 1) {
		if (sCalcPostfixDebug) printf("*** runtime_depth!=1 (==%d) ***\n", runtime_depth);
		*perror = CALC_ERR_INCOMPLETE;
		goto bad;
	}
	if (sCalcPostfixDebug) printf("\nsCalcPostfix: returning success\n");
	return 0;

bad:
	if (sCalcPostfixDebug) {
		printf("\n***error*** '%s'\n", sCalcErrorStr(*perror));
		sCalcExprDump(ppostfix);
	}
	*ppostfix = END_EXPRESSION;
	return -1;
}


/* sCalcErrorStr
 *
 * Return a message string appropriate for the given error code
 */
epicsShareFunc const char *
	sCalcErrorStr(short error)
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

/* sCalcExprDump
 *
 * Disassemble the given postfix instructions to stdout
 */
epicsShareFunc void
	sCalcExprDump(const unsigned char *pinst)
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
		case LITERAL_STRING:
			++pinst;
			printf("\tString \"%s\"\n", pinst);
			pinst += strlen((char *)pinst)+1;
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

void sCalcPrintOp(unsigned char op) {
	printf("%s\n", opcodes[(int) op]);
}
