/*************************************************************************\
Copyright (c) 1990      The Regents of the University of California
                        and the University of Chicago.
                        Los Alamos National Laboratory
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
/*************************************************************************\
                Analysis of parse tree
\*************************************************************************/
#ifndef INCLanalysish
#define INCLanalysish

#include "types.h"

/* Iteratee ("what gets iterated") for traverse_expr_tree */
typedef int expr_iter(Expr *ep, Expr *scope, void *parg);

/* Pre-order traversal of the expression tree. Call the supplied iteratee whenever
 * call_mask has the (ep->type)'th bit set. The function is called with the current
 * ep, the current scope, and an additional user defined argument (argp). Afterwards
 * recurse into all child nodes except those whose type'th bit is set in stop_mask,
 * but only if the iteratee returns a non-zero value.
 * The traversal starts at the first argument. The 4th argument is the current
 * scope; 0 may be supplied for it, in which case it will be set to a valid scope as
 * soon as the traversal encounters one.
 * NOTE: next pointer of the start expression is ignored,
 * this functions does NOT descend into sibling list elements. */

void traverse_expr_tree(
	Expr		*ep,		/* start expression */
	int		call_mask,	/* when to call iteratee */
	int		stop_mask,	/* when to stop descending */
	Expr		*scope,		/* current scope, 0 at top-level */
	expr_iter	*iteratee,	/* function to call */
	void		*parg		/* argument to pass to function */
);

VarList **pvar_list_from_scope(Expr *scope);
Expr *defn_list_from_scope(Expr *scope);

Program *analyse_program(Expr *ep, Options options);

#endif	/*INCLanalysish*/
