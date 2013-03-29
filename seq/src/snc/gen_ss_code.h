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
                State set code generation
\*************************************************************************/
#ifndef INCLgensscodeh
#define INCLgensscodeh

#include "types.h"

void init_gen_ss_code(Program *program);
void gen_ss_code(Program *program);
void gen_ss_user_var_init(Expr *ssp, int level);
void gen_var_init(Var *vp, int level);

#endif	/*INCLgensscodeh*/
