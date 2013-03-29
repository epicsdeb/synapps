/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * Test proper rejection of variables whose type is too long
 * to allow faithfull representation in CA, i.e. 64 bit types.
 * On 64 bit target systems this should give error
 * messages for the l and ul variables.
 */
program tooLong

int i;
assign i to "";

unsigned int ui;
assign ui to "";

long l;
assign l to "";

unsigned long ul;
assign ul to "";

#include "simple.st"
