/* pvFile.cc,v 1.1.1.1 2000/04/04 03:22:15 wlupton Exp
 *
 * Implementation of demonstration EPICS sequencer file library (pvFile)
 *
 * William Lupton, W. M. Keck Observatory
 */

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <sys/time.h>
#include <sys/types.h>

#include "pvFile.h"

////////////////////////////////////////////////////////////////////////////////
/*+
 * Routine:	fileSystem::fileSystem
 *
 * Purpose:	fileSystem constructor
 *
 * Description:	
 */
fileSystem::fileSystem( int debug ) :
    pvSystem( debug ),
    ifd_( fopen( "iFile", "r" ) ),
    ofd_( fopen( "oFile", "a" ) )
{
    if ( getDebug() > 0 )
	printf( "%8p: fileSystem::fileSystem( %d )\n", this, debug );

    // check for file open failure
    if ( ifd_ == NULL || ofd_ == NULL ) {
	setError( -1, pvSevrERROR, pvStatERROR, "failed to open iFile or "
		  "oFile" );
	return;
    }

    // initialize fd_set for select()
    FD_ZERO( &readfds_ );
    FD_SET( fileno( ifd_ ), &readfds_ );
}

/*+
 * Routine:	fileSystem::~fileSystem
 *
 * Purpose:	fileSystem destructor
 *
 * Description:	
 */
fileSystem::~fileSystem()
{
    if ( getDebug() > 0 )
	printf( "%8p: fileSystem::~fileSystem()\n", this );

    if ( ifd_ != NULL ) fclose( ifd_ );
    if ( ofd_ != NULL ) fclose( ofd_ );
}

/*+
 * Routine:     fileSystem::pend
 *
 * Purpose:     fileSystem pend routine
 *
 * Description:
 */
pvStat fileSystem::pend( double seconds, int wait )
{
    double secsAtStart;         // OSI time at start
    double secsNow;             // OSI time now
    double secsWaited;          // number of seconds waited

    if ( getDebug() > 0 )
        printf( "%8p: fileSystem::pend( %g, %d )\n", this, seconds, wait );

    // if not asked to wait, return immediately (there is no async activity)
    if ( !wait )
	return pvStatOK;

    // utility macro for setting a timeval structure (not very portable)
#define DOUBLE_TO_TIMEVAL(_t) { ( time_t ) (_t), \
                                ( long )   ( 1e6 * ( (_t) - ( int ) (_t) ) ) }

    // loop waiting for activity and handling events until the requested
    // timeout has elapsed (always go through the loop at least once in case
    // timeout is zero, which is a poll; negative timeout means for ever)
    for ( pvTimeGetCurrentDouble( &secsAtStart ), secsWaited = 0.0;
          secsWaited == 0.0 || seconds < 0.0 || secsWaited < seconds;
          pvTimeGetCurrentDouble( &secsNow ),
                                        secsWaited = secsNow - secsAtStart ) {

        // collect fds (copy because select() modifies them)
        fd_set readfds = readfds_;

        // select (read fds only)
        int nfd;
        if ( seconds < 0.0 ) {
            nfd = select( FD_SETSIZE, &readfds, NULL, NULL, NULL );
        } else {
            struct timeval timeout = DOUBLE_TO_TIMEVAL( seconds - secsWaited );
            nfd = select( FD_SETSIZE, &readfds, NULL, NULL, &timeout );
        }

        // ignore EINTR; fail on other errors
        if ( nfd < 0 ) {
            if ( errno == EINTR ) {
                continue;
            } else {
                setError( -errno, pvSevrMAJOR, pvStatERROR, "select failure" );
                return pvStatERROR;
            }
        }

        // continue on timeout
        if ( nfd == 0 )
	    continue;

        // otherwise, read and parse file
	char line[132];
	while ( fgets( line, sizeof( line ), ifd_ ) != NULL ) {
	    char key[132];
	    pvValue val;
	    if ( sscanf( line, "%s %s", key, val.stringVal[0] ) != 2 ) {
		fprintf( stderr, "invalid line ignored: %s", line );
	    } else {

		// ### create and set variable (very inefficient!)
		pvVariable *var = newVariable( key );
		var->put( pvTypeSTRING, 1, &val );
		delete var;
	    }
	}
    }

    return pvStatOK;
}

/*+
 * Routine:     fileSystem::newVariable
 *
 * Purpose:     fileSystem variable creation routine
 *
 * Description:
 */
pvVariable *fileSystem::newVariable( const char *name, pvConnFunc func,
				     void *priv, int debug )
{
    if ( getDebug() > 0 )
        printf( "%8p: fileSystem::newVariable( %s, %p, %p, %d )\n",
		this, name, func, priv, debug );

    return new fileVariable( this, name, func, priv, debug );
}

////////////////////////////////////////////////////////////////////////////////
/*+
 * Routine:     fileVariable::fileVariable
 *
 * Purpose:     fileVariable constructor
 *
 * Description:
 */
fileVariable::fileVariable( fileSystem *system, const char *name, pvConnFunc
			    func, void *priv, int debug ) :
    pvVariable( system, name, func, priv, debug )
{
    if ( getDebug() > 0 )
	printf( "%8p: fileVariable::fileVariable( %s, %d )\n",
		this, name, debug );
}

/*+
 * Routine:     fileVariable::~fileVariable
 *
 * Purpose:     fileVariable destructor
 *
 * Description:
 */
fileVariable::~fileVariable()
{
    if ( getDebug() > 0 )
        printf( "%8p: fileVariable::~fileVariable()\n", this );
}

/*+
 * Routine:     fileVariable::get
 *
 * Purpose:     fileVariable blocking get routine
 *
 * Description:
 */
pvStat fileVariable::get( pvType type, int count, pvValue *value )
{
    if ( getDebug() > 0 )
        printf( "%8p: fileVariable::get( %d, %d )\n", this, type, count );

    // ### not implemented (assumes string)
    printf( "would read %s\n", getName() );
    strcpy( value->stringVal[0], "string" );
    return pvStatOK;
}

/*+
 * Routine:     fileVariable::getNoBlock
 *
 * Purpose:     fileVariable non-blocking get routine
 *
 * Description:
 */
pvStat fileVariable::getNoBlock( pvType type, int count, pvValue *value )
{
    if ( getDebug() > 0 )
        printf( "%8p: fileVariable::getNoBlock( %d, %d )\n", this,
		type, count );

    // ### must block so can convert value
    return get( type, count, value );
}


/*+
 * Routine:     fileVariable::getfilellback
 *
 * Purpose:     fileVariable get with filellback routine
 *
 * Description:
 */
pvStat fileVariable::getCallback( pvType type, int count,
			          pvEventFunc func, void *arg )
{
    if ( getDebug() > 0 )
        printf( "%8p: fileVariable::getCallback( %d, %d )\n",
		this, type, count );

    // ### larger than needed (status not checked)
    pvValue *value = new pvValue[count];
    pvStat stat = get( type, count, value );
    ( *func ) ( ( void * ) this, type, count, value, arg, stat );
    delete [] value;
    return stat;
}

/*+
 * Routine:     fileVariable::put
 *
 * Purpose:     fileVariable blocking put routine
 *
 * Description:
 */
pvStat fileVariable::put( pvType type, int count, pvValue *value )
{
    if ( getDebug() > 0 )
        printf( "%8p: fileVariable::put( %d, %d )\n", this, type, count );

    // ### not implemented
    printf( "would write %s\n", getName() );
    return pvStatOK;
}

/*+
 * Routine:     fileVariable::putNoBlock
 *
 * Purpose:     fileVariable non-blocking put routine
 *
 * Description:
 */
pvStat fileVariable::putNoBlock( pvType type, int count, pvValue *value )
{
    if ( getDebug() > 0 )
        printf( "%8p: fileVariable::putNoBlock( %d, %d )\n", this,
		type, count );

    return put( type, count, value );
}

/*+
 * Routine:     fileVariable::putfilellback
 *
 * Purpose:     fileVariable put with filellback routine
 *
 * Description:
 */
pvStat fileVariable::putCallback( pvType type, int count, pvValue *value,
			          pvEventFunc func, void *arg )
{
    if ( getDebug() > 0 )
        printf( "%8p: fileVariable::putCallback( %d, %d )\n",
		this, type, count );

    pvStat stat = put( type, count, value );
    ( *func ) ( ( void * ) this, type, count, value, arg, stat );
    return stat;
}

/*+
 * Routine:     fileVariable::monitorOn
 *
 * Purpose:     fileVariable monitor enable routine
 *
 * Description:
 */
pvStat fileVariable::monitorOn( pvType type, int count, pvEventFunc func,
			        void *arg, pvCallback **pCallback )
{
    if ( getDebug() > 0 )
        printf( "%8p: fileVariable::monitorOn( %d, %d )\n",
		this, type, count );

    // ### not implemented
    printf( "would monitor %s\n", getName() );
    return pvStatOK;
}

/*+
 * Routine:     fileVariable::monitorOff
 *
 * Purpose:     fileVariable monitor disable routine
 *
 * Description:
 */
pvStat fileVariable::monitorOff( pvCallback *callback )
{
    if ( getDebug() > 0 )
        printf( "%8p: fileVariable::monitorOff()\n", this );

    // ### not implemented
    printf( "would cancel monitor on %s\n", getName() );
    return pvStatOK;
}

/*
 * pvFile.cc,v
 * Revision 1.1.1.1  2000/04/04 03:22:15  wlupton
 * first commit of seq-2-0-0
 *
 * Revision 1.1  2000/03/31 23:00:06  wlupton
 * initial insertion
 *
 */

