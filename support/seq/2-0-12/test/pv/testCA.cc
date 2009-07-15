// testCA.cc,v 1.2 2004/01/15 14:11:09 mrk Exp
//
// CA test program illustrating R3.14 behavior

#include <stdio.h>
#include <stdlib.h>

#include "cadef.h"

#define	 REPORT SEVCHK( status, name )

void event( struct event_handler_args ) {
    printf( "event\n" );
}

int main( int argc, char *argv[] ) {
    int pend = ( argc > 1 ) ? atoi( argv[1] ) : 0;
    const char *name  = ( argc > 2 ) ? argv[2] : "demo:voltage";
    int status;
    chid chid;

    // R3.14: need threadInit
    threadInit();

    status = ca_task_initialize();
    REPORT;
    status = ca_search_and_connect( name, &chid, NULL, NULL );
    REPORT;
    status = ca_add_event( DBR_DOUBLE, chid, event, NULL, NULL );
    REPORT;

    // R3.14: need this to receive monitors
    if ( pend ) {
	status = ca_pend_io( 10.0 );
	REPORT;
    }

    ca_pend_event( 2.0 );

    // R3.14: need explicit exit
    exit( 0 );
}

