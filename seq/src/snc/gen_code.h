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
                Code generation
\*************************************************************************/
#ifndef INCLgencodeh
#define INCLgencodeh

#include "types.h"

void generate_code(Program *p);
void gen_defn_c_code(Expr *scope, int level);
void gen_var_decl(Var *vp);
void indent(int level);

#define VAR_PREFIX "UserVar"

#endif	/*INCLgencodeh*/
