/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
#ifndef INCLbuiltinh
#define INCLbuiltinh

#include "sym_table.h"

struct const_symbol
{
    const char  *name;
    int         type;
};

enum const_type
{
    CT_NONE,
    CT_BOOL,
    CT_SYNCFLAG,
    CT_EVFLAG,
    CT_PVSTAT,
    CT_PVSEVR
};

enum func_type
{
    FT_DELAY,
    FT_EVENT,
    FT_PV,
    FT_OTHER
};

struct func_symbol
{
    const char  *name;
    uint        type:2;
    uint        add_length:1;
    uint        default_args:2;
    uint        ef_action_only:1;
    uint        ef_args:1;
};

/* Insert builtin constants into symbol table */
void register_builtin_consts(SymTable sym_table);

/* Insert builtin functions into symbol table */
void register_builtin_funcs(SymTable sym_table);

/* Look up a builtin function from the symbol table */
struct func_symbol *lookup_builtin_func(SymTable sym_table, const char *func_name);

/* Look up a builtin constant from the symbol table */
struct const_symbol *lookup_builtin_const(SymTable sym_table, const char *const_name);

#endif /*INCLbuiltinh */
