/*************************************************************************\
Copyright (c) 1989-1993 The Regents of the University of California
                        and the University of Chicago.
                        Los Alamos National Laboratory
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
/*************************************************************************\
                Parser support routines
\*************************************************************************/
#ifndef INCLparseh
#define INCLparseh

#include "types.h"

Expr *expr(
	int	type,		/* E_BINOP, E_ASGNOP, etc */
	Token	tok,		/* "==", "+=", var name, constant, etc. */
	...
);

Expr *opt_defn(
	Token	name,
	Token	value
);

Expr *link_expr(
	Expr	*ep1,		/* beginning of 1-st structure or list */
	Expr	*ep2		/* beginning 2-nd (append it to 1-st) */
);

uint strtoui(
	char *str,		/* string representing a number */
	uint limit,		/* result should be < limit */
	uint *pnumber		/* location for result if successful */
);

Expr *decl_add_base_type(Expr *ds, uint tag);
Expr *decl_add_init(Expr *d, Expr *init);
Expr *decl_create(Token name);
Expr *decl_postfix_array(Expr *d, char *s);
Expr *decl_prefix_pointer(Expr *d);

#endif	/*INCLparseh*/
