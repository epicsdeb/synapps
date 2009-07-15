/* johng.st,v 1.1.1.1 2000/04/04 03:22:42 wlupton Exp
 *
 * Demo Unix/KTL EPICS sequencer for John G (doesn't do anything useful)
 */

/* select KTL message system */
program johng( "pvsys=ktl" )

/* associate "utc" variable with keyword and monitor it */
double  utc;
assign  utc to "dcs0.utc";
monitor utc;

/* arrange to set event flag on "utc" monitor delivery */
evflag  utcFlag;
sync    utc utcFlag;

/* associate "telfocus" variable with keyword and monitor it */
double  telfocus;
assign  telfocus to "dcs0.telfocus";
monitor telfocus;

/* variable in which to save telfocus (variables are module-local) */
double  telfocusSave;

/* counter */
int     count = 0;

/* another counter */
int     count2 = 1;

/* first state set (each state set is a thread) */
ss utc {

    /* initial state */
    state start {

	/* report UTC on change */
	when ( efTestAndClear( utcFlag ) ) {
	    printf( "utc = %.3f\n", utc );
	    count++;
	} state start

	/* when counter hits limit, note telfocus and change state */
	when ( count == 10 ) {
	    printf( "count has hit limit (telfocus = %g)\n", telfocus );
	    telfocusSave = telfocus;
	    count        = 0;
	} state next
    }

    /* next state */
    state next {

	/* if telfocus increases, report */
	when ( telfocus > telfocusSave ) {
	    printf( "telfocus = %g\n", telfocus );
	    telfocusSave = telfocus;
	} state next

	/* if nothing happens for 5 seconds, back to start */
	when ( delay ( 5 ) ) {
	    printf( "timeout (telfocus didn't increase)\n" );
	} state start
    }
}

/* second state set */
ss ring {
    state start {
	when ( delay( 1 ) ) {
	    printf( "ring...\n" );
	} state start
    }
}

/* third state set */
ss RING {
    state start {
	when ( delay( count2 ) ) {
	    printf( "RING...\n" );
	    if ( count2 < 16 ) count2 = 2 * count2; else count2 = 1;
	} state start
    }
}

