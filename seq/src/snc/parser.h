/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
/*************************************************************************\
                    Interface to lemon generated parser
\*************************************************************************/
#ifndef INCLparserh
#define INCLparserh

#include "types.h"

Expr *parse_program(const char *src_file);

void snlParser(
	void    *yyp,		/* the parser */
	int     yymajor,	/* the major token code number */
	Token   yyminor,	/* the value for the token */
	Expr    **presult	/* extra argument */
);

void *snlParserAlloc(void *(*mallocProc)(size_t));

void snlParserFree(
	void *p,		/* the parser to be deleted */
	void (*freeProc)(void*)	/* function used to reclaim memory */
);

void snlParserTrace(FILE *TraceFILE, char *zTracePrompt);

#endif	/*INCLparserh*/
