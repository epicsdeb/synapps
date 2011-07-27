/* sCalcPostfixPvt.h
 *      Author:          Bob Dalesio
 *      Date:            9-21-88
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01  03-18-98 tmm derived from postfix.h
 * .02  10-27-01 mlr added BYTE
 * .03  03-20-03 tmm added VARGS support, stackElement (now also used by
 *                   sCalcPostfix.c), MAXV_VAL, MINV_VAL, FETCH_*, 3.14 ready
 *      03-03-06 tmm Added TR_ESC (apply dbTranslateEscape() to arg) and ESC
 *                   (apply epicsStrSnPrintEscaped() to arg).
 *      10-23-06 tmm Added CRC16 and MODBUS functions, calculate modbus 16
 *                   -bit CRC from string, and either return it, or append it.
 *      10-24-06 tmm Added LRC, AMODBUS, XOR8 and ADD_XOR8 functions
 * 
 */

#ifndef INCpostfixh
#include <sCalcPostfix.h>
#endif
#ifndef INCpostfixPvth
#define INCpostfixPvth

#include <shareLib.h>

#include	"epicsVersion.h"
#if EPICS_REVISION < 14
#include <math.h>
#ifdef vxWorks
#include	<private/mathP.h>
#define isnan(D) isNan(D)
#define isinf(D) isInf(D)
#endif
#else
#include "epicsMath.h"
#endif

#define MAX_VARGS 20
typedef union { epicsInt32 l[2]; epicsFloat64 d; } DOUBLE64_2LONG32;

#define STACKSIZE 30
#define LOCAL_STRING_SIZE 40
struct stackElement {
	double d;
	char *s;
	char local_string[LOCAL_STRING_SIZE];
};

/*	defines for element table      */
/* #define	BAD_EXPRESSION	0 */
#define 	FETCH		1
#define 	SFETCH		2
#define		CONST_PI	3
#define		CONST_D2R	4
#define		CONST_R2D	5
#define		CONST_S2R	6
#define		CONST_R2S	7
#define		RANDOM		8
#define		LITERAL		9
#define		SLITERAL	10
#define		SSCANF		11
#define		ACOS		12
#define		ASIN		13
#define		ATAN		14
#define		COS			15
#define		COSH		16
#define		SIN			17
#define		RIGHT_SHIFT	18
#define		LEFT_SHIFT	19
#define		SINH		20
#define		TAN			21
#define		TANH		22
#define		LOG_2		23
#define		COND_ELSE	24
#define		ABS_VAL		25
#define		UNARY_NEG	26
#define		SQU_RT		27
#define		EXP			28
#define		CEIL		29
#define		FLOOR		30
#define		LOG_10		31
#define		LOG_E		32
#define		ADD			33
#define		SUB			34
#define		MULT		35
#define		DIV			36
#define		EXPON		37
#define		MODULO		38
#define		BIT_OR		39
#define		BIT_AND		40
#define		BIT_EXCL_OR	41
#define		GR_OR_EQ	42
#define		GR_THAN		43
#define		LESS_OR_EQ	44
#define		LESS_THAN	45
#define		NOT_EQ		46
#define		EQUAL		47
#define		REL_OR		48
#define		REL_AND		49
#define		REL_NOT		50
#define		BIT_NOT		51
#define		PAREN		52
#define		MAX_VAL		53
#define		MIN_VAL		54
#define		COMMA		55
#define		COND_IF		56
#define		COND_END	57
#define		NINT		58
#define		ATAN2		59
#define		STORE		60
#define		TO_DOUBLE	61
#define		PRINTF		62
#define		SUBRANGE	63
#define		TO_STRING	64
#define		REPLACE		65
#define		A_FETCH		66
#define		A_SFETCH	67
#define		BYTE		68
#define		VARG_TERM	69
#define		MAXV_VAL	70
#define		MINV_VAL	71
#define		SUBLAST		72
#define		TR_ESC		73
#define		ESC			74
#define		CRC16		75
#define		MODBUS		76
#define		LRC			77
#define		AMODBUS		78
#define		XOR8		79
#define		ADD_XOR8	80
#define		BIN_READ	81
#define		BIN_WRITE	82
#define		LEN			83
#define		NORMAL_RNDM	84


/* NOTE: FETCH_A .. FETCH_L must be contiguous and in alphabetical order */
#define		FETCH_A		90
#define		FETCH_B		91
#define		FETCH_C		92
#define		FETCH_D		93
#define		FETCH_E		94
#define		FETCH_F		95
#define		FETCH_G		96
#define		FETCH_H		97
#define		FETCH_I		98
#define		FETCH_J		99
#define		FETCH_K		100
#define		FETCH_L		101

#define NO_STRING		125
#define USES_STRING		126
/* #define		END_STACK	127 */

#endif /* INCpostfixPvth */

