/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* sCalcPostfixPvt.h
 *      Original Author: Bob Dalesio
 *      Date:            9-21-88
 */

#ifndef INCpostfixh
#include <sCalcPostfix.h>
#endif
#ifndef INCpostfixPvth
#define INCpostfixPvth


#define SCALC_STACKSIZE 30
struct stackElement {
	double d;
	char *s;
	char local_string[SCALC_STRING_SIZE];
};

epicsShareFunc void
	sCalcExprDump(const unsigned char *pinst);

void sCalcPrintOp(unsigned char op);

/* RPN opcodes */
typedef enum {
	END_EXPRESSION = 0,
    /* Operands */
	LITERAL_DOUBLE, LITERAL_INT, FETCH_VAL, FETCH_SVAL,
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
	/* string calc stuff */
	CONST_S2R,
	CONST_R2S,
	LITERAL_STRING,
	SSCANF,
	LOG_2,
	PAREN,
	MAX_VAL, /* >? */
	MIN_VAL, /* <? */
	COMMA,
	TO_DOUBLE,
	PRINTF,
	SUBRANGE,
	TO_STRING,
	REPLACE,
	A_FETCH,
	A_SFETCH,
	BYTE,
	SUBLAST,
	TR_ESC,
	ESC,
	CRC16,
	MODBUS,
	LRC,
	AMODBUS,
	XOR8,
	ADD_XOR8,
	BIN_READ,
	BIN_WRITE,
	LEN,
	NORMAL_RNDM,
	A_STORE,
	A_SSTORE,
	UNTIL,
	UNTIL_END,
	NO_STRING,
	USES_STRING
} sCalc_rpn_opcode;

#endif /* INCpostfixPvth */

