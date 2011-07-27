// pvsimpleCC.cc,v 1.1.1.1 2000/04/04 03:23:09 wlupton Exp
//
// Very simple C++ program to demonstrate pv classes

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pv.h"

// event handler (get/put completion; monitors)
void event( void *obj, pvType type, int count, pvValue *val, void *arg,
	    pvStat stat) {
    pvVariable *var = ( pvVariable * ) obj;
    printf( "event: %s=%g\n", var->getName(), val->doubleVal[0] );
}

// main program
int main( int argc, char *argv[] ) {
    // system and variable names
    const char *sysNam = ( argc > 1 ) ? argv[1] : "ca";
    const char *varNam = ( argc > 2 ) ? argv[2] : "demo:voltage";

    // create system
    pvSystem *sys = newPvSystem( sysNam );
    if ( sys == NULL ) {
	printf( "newPvSystem( %s ) failure\n", sysNam );
	return -1;
    }

    // create variable
    pvVariable *var = sys->newVariable( varNam );
    if ( var == NULL ) {
	printf( "%s->newVariable( %s ) failure\n", sysNam, varNam );
	return -1;
    }

    // monitor variable as double (assume scalar)
    int status = var->monitorOn( pvTypeDOUBLE, 1, event );
    if ( status ) {
	printf( "%s->monitorOn() failure\n", varNam );
    }

    // shouldn't need this!
    sys->pend( 1, FALSE );

    // block for 10 seconds
    sys->pend( 10, TRUE );
printf( "after pend\n" );
//threadExitMain();
printf( "after exit\n" );

    // tidy up
//  delete var;
//  delete sys;
    return 0;
}

