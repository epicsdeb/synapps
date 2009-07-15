/* arrputCA.cc,v 1.2 2004/01/15 14:11:09 mrk Exp
 *
 * Loop putting simulated values to an array-valued PV (CA version)
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "epicsThread.h"

#include "cadef.h"

#define	 REPORT SEVCHK( status, name )

/* main program */
int main( int argc, char *argv[] ) {
    char       *name  = ( argc > 1 ) ? argv[1] : "k0:ao:wc:tl:dmActVec";
    int         cycle = ( argc > 2 ) ? atoi( argv[2] ) : 0;
    double      delay = ( argc > 3 ) ? atof( argv[3] ) : 0.05;
    double      scale = ( argc > 4 ) ? atof( argv[4] ) : 1.0 / ( 32767 * 10 );

    int		status;
    chid	chid;

    status = ca_task_initialize();
    REPORT;
    status = ca_search_and_connect( name, &chid, NULL, NULL );
    REPORT;
    status = ca_pend_io( 1.0 );
    REPORT;

    int         count = ca_element_count( chid );
    double     *value = new double[count];

    srand( time( NULL ) );
    for ( int i = 0; i < cycle || cycle == 0; i++ ) {
	for ( int j = 0; j < count; j++ )
	    value[j] = ( ( double ) rand() ) * scale;

	status = ca_array_put( DBR_DOUBLE, count, chid, value );
	REPORT;
	epicsThreadSleep( delay );
    }
}

