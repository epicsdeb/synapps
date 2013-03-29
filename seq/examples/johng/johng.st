/*************************************************************************\
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
/* Demo Unix/KTL EPICS sequencer for John G (doesn't do anything useful) */

/* select KTL message system */
program johng/* ( "pvsys=ktl" ) */

/* These variable could all be local to state set utc. */

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

/* just to test initialization of a global variable */
char    *ring_str = "ring...\n";

option +s;
option -c;

/* first state set (each state set is a thread) */
ss utc {
    /* variable in which to save telfocus (variables are module-local) */
    double telfocusSave;

    /* initial state */
    state start {
        /* counter */
        int count = 0;

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
            printf( "%s", ring_str );
        } state start
    }
}

/* third state set */
ss RING {
    state start {
        int count2 = 1;
        entry {
            /* demonstrate isolation of variables in safe mode:
            changing ring_str here does not affect its value in
            other state sets */
            ring_str = "RING...\n";
        }
        when ( delay( count2 ) ) {
            printf( "%s", ring_str );
            if ( count2 < 16 ) count2 = 2 * count2; else count2 = 1;
        } state start
    }
}
