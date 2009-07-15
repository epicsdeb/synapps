/* control.st,v 1.1.1.1 2000/04/04 03:23:14 wlupton Exp
 *
 * Demo KTL EPICS sequence; illustrates and tests parallel control
 */

program control( "pvsys=ktl" )

/* options */
option  +r;

/* control variables */
#define N 6
long    init[N];
assign  init to {"dcs.axeinit", "dcs.chpinit", "dcs.domeinit", "dcs.rotinit",
		 "dcs.secinit", "dcs.tertinit"};

/* ordinary variables */
long    done[N];

/* main state-set */
ss main {
    state inactive {
	when () {
%%	    long i;
	    for ( i = 0; i < N; i++ ) {
		init[i] = 1;
		pvPut( init[i], ASYNC );
	    }
	    printf( "all init commands sent\n" );
	} state active
    }

    state active {
	when ( pvPutComplete( init, TRUE, done ) ) {
%%	    long i;
	    printf( "init commands not all done:" );
	    for ( i = 0; i < N; i++ )
		printf( " %ld", done[i] );
	    printf( "\n" );
	} state active

	when ( pvPutComplete( init ) ) {
	    printf( "init commands all done\n" );
	} state done

	when ( delay( 5 ) ) {
	    printf( "timeout waiting for inits to complete\n" );
	} state done
    }

    state done {
	when () {
%%	    long i;
	    for ( i = 0; i < N; i++ ) {
		printf( "%s: done,stat,sevr,mess = %ld,%ld,%ld,%s\n",
		    pvName( init[i] ),
		    pvPutComplete( init[i] ),
		    pvStatus( init[i] ),
		    pvSeverity( init[i] ),
		    pvMessage( init[i] ) ? pvMessage( init[i] ) : "" );
	    }
	} state wait
    }

    state wait {
	when ( delay( 5 ) ) {
	} state inactive
    }
}

