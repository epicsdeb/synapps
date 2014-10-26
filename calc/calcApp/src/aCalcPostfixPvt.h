/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* aCalcPostfixPvt.h
 *      Derived from postfixPvt.h in EPICS base
 *      Current author: Tim Mooney
 */

#ifndef INC_aCalcPostfixh
#include <aCalcPostfix.h>
#endif
#ifndef INC_aCalcPostfixPvth
#define INC_aCalcPostfixPvth

#include <shareLib.h>

#define ACALC_STACKSIZE 20

epicsShareFunc void
	aCalcExprDump(const unsigned char *pinst);

void aCalcPrintOp(unsigned char op);

typedef enum {
	END_EXPRESSION = 0,
    /* Operands */
	LITERAL_DOUBLE, LITERAL_INT, FETCH_VAL, FETCH_AVAL,
	FETCH_A, FETCH_B, FETCH_C, FETCH_D, FETCH_E, FETCH_F,
	FETCH_G, FETCH_H, FETCH_I, FETCH_J, FETCH_K, FETCH_L,
	FETCH_M, FETCH_N, FETCH_O, FETCH_P,
	FETCH_AA, FETCH_BB, FETCH_CC, FETCH_DD, FETCH_EE, FETCH_FF,
	FETCH_GG, FETCH_HH, FETCH_II, FETCH_JJ, FETCH_KK, FETCH_LL,
    /* Assignment */
	STORE_A, STORE_B, STORE_C, STORE_D, STORE_E, STORE_F,
	STORE_G, STORE_H, STORE_I, STORE_J, STORE_K, STORE_L,
	STORE_M, STORE_N, STORE_O, STORE_P,
	STORE_AA, STORE_BB, STORE_CC, STORE_DD, STORE_EE, STORE_FF,
	STORE_GG, STORE_HH, STORE_II, STORE_JJ, STORE_KK, STORE_LL,
    /* Trigonometry Constants */
	CONST_PI,
	CONST_D2R,
	CONST_R2D,
    /* Arithmetic */
	UNARY_NEG,
	ADD,
	SUB,
	MULT,
	DIV,
	MODULO,
	POWER,
    /* Algebraic */
	ABS_VAL,
	EXP,
	LOG_10,
	LOG_E,
	MAX,
	MIN,
	SQU_RT,
    /* Trigonometric */
	ACOS,
	ASIN,
	ATAN,
	ATAN2,
	COS,
	COSH,
	SIN,
	SINH,
	TAN,
	TANH,
    /* Numeric */
	CEIL,
	FLOOR,
	FINITE,
	ISINF,
	ISNAN,
	NINT,
	RANDOM,
    /* Boolean */
	REL_OR,
	REL_AND,
	REL_NOT,
    /* Bitwise */
	BIT_OR,
	BIT_AND,
	BIT_EXCL_OR,
	BIT_NOT,
	RIGHT_SHIFT,
	LEFT_SHIFT,
    /* Relationals */
	NOT_EQ,
	LESS_THAN,
	LESS_OR_EQ,
	EQUAL,
	GR_OR_EQ,
	GR_THAN,
    /* Conditional */
	COND_IF,
	COND_ELSE,
	COND_END,
    /* Misc */
	NOT_GENERATED,
	/* array calc stuff */
	CONST_S2R,
	CONST_R2S,
	CONST_IX,
	LOG_2,
	PAREN,
	MAX_VAL, /* >? */
	MIN_VAL, /* <? */
	COMMA,
	TO_DOUBLE,
	SUBRANGE,
	SUBRANGE_IP,
	TO_ARRAY,
	A_FETCH,
	A_AFETCH,
	LEN,
	NORMAL_RNDM,
	A_STORE,
	A_ASTORE,
	UNTIL,
	UNTIL_END,
	AVERAGE,
	STD_DEV,
	FWHM,
	SMOOTH,
	NSMOOTH,
	DERIV,
	NDERIV,
	ARRSUM,
	AMAX,
	AMIN,
	FITPOLY,
	FITMPOLY,
	ARANDOM,
	CUM,
	IXMAX,
	IXMIN,
	IXZ,
	IXNZ,
	FITQ,
	FITMQ,
	CAT
} aCalc_rpn_opcode;

#endif /* INC_aCalcPostfixPvth */

