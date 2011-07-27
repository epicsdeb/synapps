/* local.st,v 1.1.1.1 2000/04/04 03:22:43 wlupton Exp
 *
 * Sequence to illustrate / test local variable declarations
 */

program local( "pvsys=ktl" )

/* options */
#if 0
option  +r;
#endif

/* control variables */

/* ordinary variables */
int i = 1;

/* only state-set */
ss start {
    state first {
	int i = 0;
	when () {
	    printf( "i = %d\n", i );
	} state last
    }

    state last {
	when ( delay( 2.0 ) ) {
	} state first
    }
}

/* exit handler */
exit {
    printf( "exit: i = %d\n", i );
}

