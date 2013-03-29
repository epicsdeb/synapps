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
                Parser support routines
\*************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <errno.h>
#include <limits.h>

#define expr_type_GLOBAL
#include "types.h"
#undef expr_type_GLOBAL
#include "expr.h"
#include "main.h"

static const StateOptions default_state_options = DEFAULT_STATE_OPTIONS;

/* Expr is the generic syntax tree node */
Expr *expr(
	int	type,
	Token	tok,
	...			/* variable number of child arguments */
)
{
	va_list	argp;
	uint	i, num_children;
	Expr	*ep;

	num_children = expr_type_info[type].num_children;

	/* handle special case delay function */
	if (type == E_FUNC && strcmp(tok.str,"delay")==0)
		type = E_DELAY;

	ep = new(Expr);
	ep->next = 0;
	ep->last = ep;
	ep->type = type;
	ep->value = tok.str;
	ep->line_num = tok.line;
	ep->src_file = tok.file;
	ep->children = newArray(Expr *, num_children);
	/* allocate extra data */
	switch (type)
	{
	case D_SS:
		ep->extra.e_ss = new(StateSet);
		break;
	case D_STATE:
		ep->extra.e_state = new(State);
		ep->extra.e_state->options = default_state_options;
		break;
	case D_WHEN:
		ep->extra.e_when = new(When);
		break;
	}

#ifdef	DEBUG
	report_at_expr(ep, "expr: ep=%p, type=%s, value=\"%s\", file=%s, line=%d",
		ep, expr_type_name(ep), tok.str, tok.file, tok.line);
#endif	/*DEBUG*/
	va_start(argp, tok);
	for (i = 0; i < num_children; i++)
	{
		ep->children[i] = va_arg(argp, Expr*);
#ifdef	DEBUG
		report(", child[%d]=%p", i, ep->children[i]);
#endif	/*DEBUG*/
	}
	va_end(argp);
#ifdef	DEBUG
	report(")\n");
#endif	/*DEBUG*/

	return ep;
}

Expr *opt_defn(Token name, Token value)
{
	Expr *opt = expr(D_OPTION, name);
	opt->extra.e_option = (value.str[0] == '+');
	return opt;
}

/* Link two expression structures and/or lists.  Returns ptr to combined list.
   Note: last ptrs are correct only for 1-st element of the resulting list */
Expr *link_expr(
	Expr	*ep1,	/* 1-st structure or list */
	Expr	*ep2	/* 2-nd (append it to 1-st) */
)
{
	if (ep1 == 0)
		return ep2;
	if (ep2 == 0)
		return 0;
	ep1->last->next = ep2;
	ep1->last = ep2->last;
	ep2->last = 0;
	return ep1;
}

uint strtoui(
	char *str,		/* string representing a number */
	uint limit,		/* result should be < limit */
	uint *pnumber		/* location for result if successful */
)
{
	unsigned long result;

	errno = 0;
	result = strtoul(str, 0, 0);
	if (errno || result >= limit)
		return FALSE;
	*pnumber = result;
	return TRUE;
}
