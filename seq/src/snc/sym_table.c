/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
/*************************************************************************\
                                Symbol table
\*************************************************************************/
#include <stdio.h>	/* gpHash.h mentions FILE but does not include stdio */
#include <gpHash.h>
#include <assert.h>

#include "types.h"
#include "sym_table.h"

/* Invariant: all values stored in the table are non-zero. */

SymTable sym_table_create(void)
{
	SymTable st;
	gphInitPvt(&st.table, 256);
	return st;
}

void *sym_table_lookup(const SymTable st, const char *name, void *type)
{
	GPHENTRY *pentry;

	assert(st.table);
	pentry = gphFind(st.table, name, type);
	if (pentry)
	{
		assert(pentry->userPvt);	/* invariant! */
		return pentry->userPvt;
	}
	else
	{
		return 0;
	}
}

void *sym_table_insert(SymTable st, const char *name, void *type, void *value)
{
	GPHENTRY *pentry;

	assert(st.table);
	if (!value)
	{
		return 0;
	}
	pentry = gphAdd(st.table, name, type);
	if (pentry)	/* success, value was inserted */
	{
		pentry->userPvt = value;	/* maintain invariant */
		return pentry->userPvt;
	}
	else		/* failed, there is already such a key */
	{
		return 0;
	}
}

void sym_table_destroy(SymTable st)
{
	gphFreeMem(st.table);
}
