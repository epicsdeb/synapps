/* pvsimpleC.c,v 1.1.1.1 2000/04/04 03:23:09 wlupton Exp
 *
 * Very simple C program to demonstrate pv classes
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pv.h"

/* event handler (get/put completion; monitors) */
void event( void *var, pvType type, int count, pvValue *val, void *arg,
	    pvStat stat) {
    printf( "event: %s=%g\n", pvVarGetName( var ), val->doubleVal[0] );
}

/* main program */
int main( int argc, char *argv[] ) {
    /* system and variable names */
    const char *sysNam = ( argc > 1 ) ? argv[1] : "ca";
    const char *varNam = ( argc > 2 ) ? argv[2] : "demo:voltage";
    void *sys;
    void *var;
    int status;

    /* create system */
    status = pvSysCreate( sysNam, 0, &sys );
    if ( status ) {
	printf( "pvSysCreate( %s ) failure\n", sysNam );
	return -1;
    }

    /* create variable */
    status = pvVarCreate( sys, varNam, NULL, NULL, 0, &var );
    if ( status ) {
	printf( "pvVarCreate( %s, %s ) failure\n", sysNam, varNam );
	return -1;
    }

    /* monitor variable as double (assume scalar) */
    status = pvVarMonitorOn( var, pvTypeDOUBLE, 1, event, NULL, NULL );
    if ( status ) {
	printf( "pvVarMonitorOn( %s ) failure\n", varNam );
    }

    /* shouldn't need this! */
    pvSysPend( sys, 1, FALSE );

    /* block for 10 seconds */
    pvSysPend( sys, 10, TRUE );

    /* tidy up */
    pvVarDestroy( var );
    pvSysDestroy( sys );
    return 0;
}

