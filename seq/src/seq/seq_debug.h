/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
#ifndef INCLseq_debugh
#define INCLseq_debugh

#include <stdio.h>
#include "errlog.h"

static int nothing(const char *format,...) {return 0;}

/* To enable debug messages in a region of code, say e.g. */

#undef DEBUG
#define DEBUG printf

/* ... code with debug messages enabled... */

#undef DEBUG
#define DEBUG nothing

#endif
