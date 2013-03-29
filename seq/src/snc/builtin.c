/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
#include "builtin.h"

static struct const_symbol const_symbols[] =
{
    {"TRUE",                CT_BOOL},
    {"FALSE",               CT_BOOL},
    {"SYNC",                CT_SYNCFLAG},
    {"ASYNC",               CT_SYNCFLAG},
    {"NOEVFLAG",            CT_EVFLAG},
    {"pvStatOK",            CT_PVSTAT},
    {"pvStatERROR",         CT_PVSTAT},
    {"pvStatDISCONN",       CT_PVSTAT},
    {"pvStatREAD",          CT_PVSTAT},
    {"pvStatWRITE",         CT_PVSTAT},
    {"pvStatHIHI",          CT_PVSTAT},
    {"pvStatHIGH",          CT_PVSTAT},
    {"pvStatLOLO",          CT_PVSTAT},
    {"pvStatLOW",           CT_PVSTAT},
    {"pvStatSTATE",         CT_PVSTAT},
    {"pvStatCOS",           CT_PVSTAT},
    {"pvStatCOMM",          CT_PVSTAT},
    {"pvStatTIMEOUT",       CT_PVSTAT},
    {"pvStatHW_LIMIT",      CT_PVSTAT},
    {"pvStatCALC",          CT_PVSTAT},
    {"pvStatSCAN",          CT_PVSTAT},
    {"pvStatLINK",          CT_PVSTAT},
    {"pvStatSOFT",          CT_PVSTAT},
    {"pvStatBAD_SUB",       CT_PVSTAT},
    {"pvStatUDF",           CT_PVSTAT},
    {"pvStatDISABLE",       CT_PVSTAT},
    {"pvStatSIMM",          CT_PVSTAT},
    {"pvStatREAD_ACCESS",   CT_PVSTAT},
    {"pvStatWRITE_ACCESS",  CT_PVSTAT},
    {"pvSevrOK",            CT_PVSEVR},
    {"pvSevrERROR",         CT_PVSEVR},
    {"pvSevrNONE",          CT_PVSEVR},
    {"pvSevrMINOR",         CT_PVSEVR},
    {"pvSevrMAJOR",         CT_PVSEVR},
    {"pvSevrINVALID",       CT_PVSEVR},
    {0,                     CT_NONE}
};

static struct func_symbol func_symbols[] =
{
    {"delay",           FT_DELAY,   FALSE,  0,  FALSE,  FALSE},
    {"efClear",         FT_EVENT,   FALSE,  0,  TRUE,   FALSE},
    {"efSet",           FT_EVENT,   FALSE,  0,  TRUE,   FALSE},
    {"efTest",          FT_EVENT,   FALSE,  0,  FALSE,  FALSE},
    {"efTestAndClear",  FT_EVENT,   FALSE,  0,  FALSE,  FALSE},
    {"macValueGet",     FT_OTHER,   FALSE,  0,  FALSE,  FALSE},
    {"optGet",          FT_OTHER,   FALSE,  0,  FALSE,  FALSE},
    {"pvAssign",        FT_PV,      FALSE,  0,  FALSE,  FALSE},
    {"pvAssignCount",   FT_OTHER,   FALSE,  0,  FALSE,  FALSE},
    {"pvAssigned",      FT_PV,      FALSE,  0,  FALSE,  FALSE},
    {"pvChannelCount",  FT_OTHER,   FALSE,  0,  FALSE,  FALSE},
    {"pvConnectCount",  FT_OTHER,   FALSE,  0,  FALSE,  FALSE},
    {"pvConnected",     FT_PV,      FALSE,  0,  FALSE,  FALSE},
    {"pvCount",         FT_PV,      FALSE,  0,  FALSE,  FALSE},
    {"pvDisconnect",    FT_PV,      FALSE,  0,  FALSE,  FALSE},
    {"pvFlush",         FT_OTHER,   FALSE,  0,  FALSE,  FALSE},
    {"pvFlushQ",        FT_PV,      FALSE,  0,  FALSE,  FALSE},
    {"pvFreeQ",         FT_PV,      FALSE,  0,  FALSE,  FALSE},
    {"pvGet",           FT_PV,      FALSE,  1,  FALSE,  FALSE},
    {"pvGetComplete",   FT_PV,      FALSE,  0,  FALSE,  FALSE},
    {"pvGetQ",          FT_PV,      FALSE,  0,  FALSE,  FALSE},
    {"pvIndex",         FT_PV,      FALSE,  0,  FALSE,  FALSE},
    {"pvMessage",       FT_PV,      FALSE,  0,  FALSE,  FALSE},
    {"pvMonitor",       FT_PV,      FALSE,  0,  FALSE,  FALSE},
    {"pvName",          FT_PV,      FALSE,  0,  FALSE,  FALSE},
    {"pvPut",           FT_PV,      FALSE,  1,  FALSE,  FALSE},
    {"pvPutComplete",   FT_PV,      TRUE,   2,  FALSE,  FALSE},
    {"pvSeverity",      FT_PV,      FALSE,  0,  FALSE,  FALSE},
    {"pvStatus",        FT_PV,      FALSE,  0,  FALSE,  FALSE},
    {"pvStopMonitor",   FT_PV,      FALSE,  0,  FALSE,  FALSE},
    {"pvSync",          FT_PV,      FALSE,  0,  FALSE,  TRUE },
    {"pvTimeStamp",     FT_PV,      FALSE,  0,  FALSE,  FALSE},
    {"seqLog",          FT_OTHER,   FALSE,  0,  FALSE,  FALSE},
    {0,                 FT_OTHER,   FALSE,  0,  FALSE,  FALSE}
};


/* Insert builtin constants into symbol table */
void register_builtin_consts(SymTable sym_table)
{
    struct const_symbol *sym;

    for (sym = const_symbols; sym->name; sym++) {
        /* use address of const_symbols array as the symbol type */
        sym_table_insert(sym_table, sym->name, const_symbols, sym);
    }
}

/* Insert builtin functions into symbol table */
void register_builtin_funcs(SymTable sym_table)
{
    struct func_symbol *sym;

    for (sym = func_symbols; sym->name; sym++) {
        /* use address of func_symbols array as the symbol type */
        sym_table_insert(sym_table, sym->name, func_symbols, sym);
    }
}

/* Look up a builtin function from the symbol table */
struct func_symbol *lookup_builtin_func(SymTable sym_table, const char *func_name)
{
    /* use address of func_symbols array as the symbol type */
    return (struct func_symbol *)sym_table_lookup(sym_table, func_name, func_symbols);
}

/* Look up a builtin constant from the symbol table */
struct const_symbol *lookup_builtin_const(SymTable sym_table, const char *const_name)
{
    /* use address of const_symbols array as the symbol type */
    return (struct const_symbol *)sym_table_lookup(sym_table, const_name, const_symbols);
}
