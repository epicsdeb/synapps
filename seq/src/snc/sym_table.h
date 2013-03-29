/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
/*************************************************************************\
                                Symbol table
\*************************************************************************/
#ifndef INCLsymtableh
#define INCLsymtableh

#include "types.h"

/* A symbol table is a finite map between keys and values. Here, the
   keys are (name,type) pairs with types (char*,void*), and values are
   just pointers (void*).

   Implementation uses EPICS gpHash module (see libCom)
   
   NOTE: In contrast to the gpHash API, we make no difference between a
   zero value and no value at all. Thus, inserting something with value
   zero is a no-op. */

/* Create a new symbol table. If this fails, return 0. */
SymTable sym_table_create(void);

/* Lookup a symbol: given a symbol table, a name and a type,
   return the value or 0 if not found. */
void *sym_table_lookup(const SymTable st, const char *name, void *type);

/* Insert a symbol.

   If the value is non-zero, try to insert it into the table under the
   given (name,type) key, otherwise do nothing.

   Return the value if successfully inserted, otherwise return zero. */
void *sym_table_insert(SymTable st, const char *name, void *type, void *value);

/* Free all memory associated with a table. */
void sym_table_destroy(SymTable st);

#endif /*INCLsymtableh*/
