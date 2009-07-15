/* pvtest.cc,v 1.1.1.1 2000/04/04 03:23:09 wlupton Exp
 *
 * Test pv classes
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pv.h"

#define TYPE(_type) \
    ( _type[0] == 'c' ? pvTypeTIME_CHAR   : \
      _type[0] == 'h' ? pvTypeTIME_SHORT  : \
      _type[0] == 'l' ? pvTypeTIME_LONG   : \
      _type[0] == 'f' ? pvTypeTIME_FLOAT  : \
      _type[0] == 's' ? pvTypeTIME_STRING : \
			pvTypeTIME_DOUBLE )

#define OUTPUT(_type,_val,_ind) \
    ( _type == pvTypeTIME_CHAR  ? \
		printf( "%d", _val->timeCharVal.value[_ind] & 0xff ) : \
      _type == pvTypeTIME_SHORT  ? \
		printf( "%hd", _val->timeShortVal.value[_ind] ) : \
      _type == pvTypeTIME_LONG   ? \
		printf( "%ld", _val->timeLongVal.value[_ind] ) : \
      _type == pvTypeTIME_FLOAT  ? \
		printf( "%g", _val->timeFloatVal.value[_ind] ) : \
      _type == pvTypeTIME_STRING ? \
		printf( "\"%s\"", _val->timeStringVal.value[_ind] ) : \
		printf( "%g", _val->timeDoubleVal.value[_ind] ) )

void conn( void *, int connected ) {
    printf( "conn: connected=%d\n", connected );
}

void event( void *, pvType type, int count, pvValue *val, void *arg,
	    pvStat stat) {
    if ( stat != pvStatOK ) {
	printf( "event%d: stat=%d\n", ( int ) arg, stat );
    }
    else {
	printf( "event%d: tim=%d, val=", ( int ) arg, val->timeDoubleVal.stamp.
		secPastEpoch );
	OUTPUT( type, val, 0 );
	if ( count > 1 ) {
	    printf( " ... " );
	    OUTPUT( type, val, count - 1 );
	}
	printf( "\n" );
    }
}

int main( int argc, char *argv[] ) {
    char     *snm = ( argc < 2 ) ? ( char * ) "ca" : argv[1];
    int	      deb = ( argc < 3 ) ? 1 : atoi( argv[2] );
    pvSystem *sys = newPvSystem( snm, deb );
    if ( sys == NULL ) {
	printf( "newPvSystem( %s ) failure\n", snm );
	return -1;
    }
    printf( "%s: %d %d %d %s\n", snm, sys->getStatus(), sys->getSevr(),
	    sys->getStat(), sys->getMess() );

    char     *nam = ( argc < 4 ) ? ( strcmp( snm, "ca" ) == 0 ? \
	       ( char * ) "k1:ao:sc:cpu" : ( char * ) "ao1.aosccpu" ) : argv[3];
    pvVariable *var = sys->newVariable( nam, conn, NULL, deb );
    if ( var == NULL ) {
	printf( "%s->newVariable( %s ) failure\n", snm, nam );
	return -1;
    }
    printf( "%s: %d %d %d %s\n", nam, var->getStatus(), var->getSevr(),
	    var->getStat(), var->getMess() );

    char     *typ = ( argc < 5 ) ? ( char * ) "d" : argv[4];
    int       cnt = var->getCount();
    var->monitorOn( TYPE( typ ), cnt, event, ( void * ) 0 );
    sys->pend( 1, FALSE );

    if ( var->getStat() == pvStatOK )
	printf( "%s: connected=%d, type=%d, count=%d, private=%p\n", nam,
		var->getConnected(), var->getType(), cnt, var->getPrivate() );

    pvValue *val = new pvValue[cnt];
    var->get( TYPE( typ ), cnt, val );
    sys->pend( 1, FALSE );

    printf( "%s: %d %d %d %s\n", nam, var->getStatus(), var->getSevr(),
	    var->getStat(), var->getMess() );
    if ( var->getStat() == pvStatOK ) {
	OUTPUT( TYPE( typ ), val, 0 );
	printf( "\n" );
    }

    var->getCallback( TYPE( typ ), cnt, event, ( void * ) 1 );
    sys->pend( 1, TRUE );

    printf( "%s: %d %d %d %s\n", nam, var->getStatus(), var->getSevr(),
	    var->getStat(), var->getMess() );

    sys->pend( 10, TRUE );

    printf( "%s: %d %d %d %s\n", nam, var->getStatus(), var->getSevr(),
	    var->getStat(), var->getMess() );

    delete var;
    delete sys;

    return 0;
}

