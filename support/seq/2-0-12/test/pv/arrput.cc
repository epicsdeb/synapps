/* arrput.cc,v 1.2 2004/01/15 14:11:09 mrk Exp
 *
 * Loop putting simulated values to an array-valued PV
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "epicsThread.h"

#include "pv.h"

#define  SYSNAME "ca"		/* system name (hard-coded for CA) */

#define	 REPORT \
    if ( sys.getSevr() != pvSevrNONE ) \
	printf( "%s: %d %d %d %s\n", name, sys.getStatus(), \
		sys.getSevr(), sys.getStat(), sys.getMess() )


/* main program */
int main( int argc, char *argv[] ) {
    const char *prog  = ( argc > 0 ) ? argv[0] : "prog?";
    const char *name  = ( argc > 1 ) ? argv[1] : "k0:ao:wc:tl:dmActVec";
    int         cycle = ( argc > 2 ) ? atoi( argv[2] ) : 0;
    double      delay = ( argc > 3 ) ? atof( argv[3] ) : 0.05;
    double      scale = ( argc > 4 ) ? atof( argv[4] ) : 1.0 / ( 32767 * 10 );
    double      zero  = ( argc > 5 ) ? atof( argv[5] ) : 0.0;

    printf( "%s: name=%s, cycle=%d, delay=%g, scale=%g, zero=%g\n",
	    prog, name, cycle, delay, scale, zero );

    pvSystem   &sys   = *newPvSystem( SYSNAME, 0 );
    REPORT;
    pvVariable &var   = *sys.newVariable( name );
    sys.pend( 1.0 );
    REPORT;

    int         count = var.getCount();
    pvValue    &value = * ( pvValue * ) new double[count];

    srand( time( NULL ) );
    for ( int i = 0; i < cycle || cycle == 0; i++ ) {

	for ( int j = 0; j < count; j++ )
	    value.doubleVal[j] = ( ( double ) rand() ) * scale + zero;

	var.put( pvTypeDOUBLE, count, &value );
	REPORT;

	epicsThreadSleep( delay );
    }
}

