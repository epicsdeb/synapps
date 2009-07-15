/* pvKtl.cc,v 1.2 2001/02/16 18:45:39 mrk Exp
 *
 * Implementation of EPICS sequencer KTL library (pvKtl)
 *
 * William Lupton, W. M. Keck Observatory
 */

// ### check all object creation and call cantProceed on failure

// ### review locking policy (need to be clear on why, how and when)

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>

#include "gpHash.h"

#include "pvKtl.h"
#include "pvKtlCnv.h"

/* handlers */
static void connectionHandler( ktlKeyword *variable, char *name,
			       int connected );
static void accessHandler( char *name, pvCallback *callback,
			   KTL_ANYPOLY *ktlValue, KTL_CONTEXT *context );
static void monitorHandler( char *name, ktlKeyword *keyword,
			    KTL_ANYPOLY *ktlValue, KTL_CONTEXT *context );
static void commonHandler( char *name, pvCallback *callback,
			    KTL_ANYPOLY *ktlValue, KTL_CONTEXT *context );

/* utilities */
static char *serviceName( const char *name );
					/* service name from "srv.key" name */
static char *keywordName( const char *name );
					/* keyword name from "srv.key" name */
static pvSevr sevrFromKTL( int status );
					/* severity as pvSevr */
static pvStat statFromKTL( int status );
					/* status as pvStat */
static pvType typeFromKTL( int type );
					/* KTL type as pvType */
static int copyFromKTL( int ktlFlags, KTL_DATATYPE ktlType, int ktlCount,
			const KTL_ANYPOLY *ktlValue, pvType type, int count,
			pvValue *value );
					/* copy KTL value to pvValue */
static int copyToKTL( pvType type, int count, const pvValue *value,
		KTL_DATATYPE ktlType, int ktlCount, KTL_POLYMORPH *ktlValue );
					/* copy pvValue to KTL value */
static int mallocKTL( KTL_DATATYPE ktlType, int ktlCount,
					    KTL_POLYMORPH *ktlValue );
					/* alloc dynamic data ref'd by value */
static void freeKTL( KTL_DATATYPE ktlType, KTL_POLYMORPH *ktlValue );
					/* free dynamic data ref'd by value */

/* invoke KTL function and send error details to system or variable object */
#define INVOKE(_function) \
    do { \
	int _status = _function; \
	if ( _status >= 0 ) { \
	    setStatus( _status ); \
	    setStat( pvStatOK ); \
	} \
	else \
	    setError( _status, sevrFromKTL( _status ), \
		      statFromKTL( _status ), ktl_get_errtxt() ); \
    } while ( FALSE )

/* lock / unlock shorthands */
#define LOCK   getSystem()->lock()
#define UNLOCK getSystem()->unlock()

////////////////////////////////////////////////////////////////////////////////
/*+
 * Routine:	ktlSystem::ktlSystem
 *
 * Purpose:	ktlSystem constructor
 *
 * Description:	
 */
ktlSystem::ktlSystem( int debug ) :
    pvSystem( debug ),
    attach_( FALSE )
{
    if ( getDebug() > 0 )
	printf( "%8p: ktlSystem::ktlSystem( %d )\n", this, debug );
}

/*+
 * Routine:	ktlSystem::~ktlSystem
 *
 * Purpose:	ktlSystem destructor
 *
 * Description:	
 */
ktlSystem::~ktlSystem()
{
    if ( getDebug() > 0 )
	printf( "%8p: ktlSystem::~ktlSystem()\n", this );
}

/*+
 * Routine:     ktlSystem::attach
 *
 * Purpose:     ktlSystem attach routine
 *
 * Description:
 */
pvStat ktlSystem::attach()
{
    if ( getDebug() > 0 )
        printf( "%8p: ktlSystem::attach()\n", this );

    // attach all services that are already open
    for ( ktlService *service = ktlService::first(); service != NULL;
		      service = service->next() )
	( void ) ktl_ioctl( service->getHandle(), KTL_ATTACH );

    // attach all future services as they are opened
    attach_ = TRUE;

    return pvStatOK;
}

/*+
 * Routine:     ktlSystem::flush
 *
 * Purpose:     ktlSystem flush routine
 *
 * Description:
 */
pvStat ktlSystem::flush()
{
    if ( getDebug() > 1 )
        printf( "%8p: ktlSystem::flush()\n", this );

    // KTL has no way of deferring buffer flush
    return pvStatOK;
}

/*+
 * Routine:     ktlSystem::pend
 *
 * Purpose:     ktlSystem pend routine
 *
 * Description:
 */
pvStat ktlSystem::pend( double seconds, int wait )
{
    double secsAtStart;		// OSI time at start
    double secsNow;		// OSI time now
    double secsWaited;		// number of seconds waited

    if ( getDebug() > 1 )
        printf( "%8p: ktlSystem::pend( %g, %d )\n", this, seconds, wait );

    // if wait==FALSE, should wait for all operations initiated via KTL_NOWAIT
    // (assuming that the keyword library really supports KTL_NOWAIT as
    // documented in KSD/28, which is very unlikely)
    // ### for now, ignore the above issue

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

	// collect fds from all services
	fd_set *readfds = ktlService::getFdsetsAll();

	// select (read fds only)
	int nfd;
	if ( seconds < 0.0 ) {
	    nfd = select( FD_SETSIZE, readfds, NULL, NULL, NULL );
	} else {
	    // ### hard-code 0.1s pending better solution
	    //struct timeval timeout = DOUBLE_TO_TIMEVAL( seconds - secsWaited);
	    struct timeval timeout = DOUBLE_TO_TIMEVAL( 0.1 );
	    nfd = select( FD_SETSIZE, readfds, NULL, NULL, &timeout );
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

	// continue or break on timeout (depending on "wait"; not an error)
	if ( nfd == 0 ) {
	    if ( wait )
		continue;
	    else
		break;
	}

	// otherwise, dispatch all open services
	LOCK;
	pvStat stat = ktlService::dispatchAll();
	UNLOCK;
	if ( stat != pvStatOK ) {
	    return pvStatERROR;
	}
    }

    return pvStatOK;
}

/*+
 * Routine:     ktlSystem::newVariable
 *
 * Purpose:     ktlSystem variable creation routine
 *
 * Description:
 */
pvVariable *ktlSystem::newVariable( const char *name, pvConnFunc func, void *priv,
				    int debug )
{
    if ( getDebug() > 0 )
        printf( "%8p: ktlSystem::newVariable( %s, %p, %p, %d )\n",
		this, name, func, priv, debug );

    return new ktlVariable( this, name, func, priv, debug );
}

////////////////////////////////////////////////////////////////////////////////
/*
 * list of ktlService objects (statically initialized)
 */
ELLLIST ktlService::list_ = {{NULL,NULL}, 0};

/*+
 * Routine:     ktlService::ktlService
 *
 * Purpose:     ktlService constructor
 *
 * Description:
 */
ktlService::ktlService( ktlSystem *system, const char *name, int debug ) :
    debug_( debug ),
    name_( strdup( name ) ),
    flags_( 0 ),
    keys_( NULL ),
    status_( 0 ),
    sevr_( pvSevrNONE ),
    stat_( pvStatOK ),
    mess_( NULL )
{
    if ( getDebug() > 0 )
	printf( "%8p: ktlService::ktlService( %s, %d )\n",
		this, name, debug );

    handle_ = NULL;
    INVOKE( ktl_open( name_, "keyword", 0, &handle_ ) );

    if ( system->getAttach() )
	( void ) ktl_ioctl( handle_, KTL_ATTACH );

    // use KTL_SUPERSUPP to see whether KTL_SUPER is supported
    ( void ) ktl_ioctl( handle_, KTL_SUPERSUPP, &flags_ );
    flags_ = flags_ ? ( KTL_SUPER | KTL_STAMP ) : 0;

    // create keyword hash table (aborts on failure)
    gphInitPvt( &keys_, 256 );

    // ### not calling FDREG but perhaps should try to?

    // link into list of services
    node_.setData( this );
    ellAdd( &list_, ( ELLNODE * ) &node_ );
}

/*+
 * Routine:     ktlService::~ktlService
 *
 * Purpose:     ktlService destructor
 *
 * Description:
 */
ktlService::~ktlService()
{
    if ( getDebug() > 0 )
        printf( "%8p: ktlService::~ktlService()\n", this );

    // remove from list of services
    ellDelete( &list_, ( ELLNODE * ) &node_ );

    // free keyword hash table memory
    gphFreeMem( keys_ );

    // close KTL connection (ignore failure)
    INVOKE( ktl_close( handle_ ) );

    // free name
    if ( name_ != NULL )
	free( name_ );
}

/*+
 * Routine:     ktlService::getService
 *
 * Purpose:     ktlService static accessor routine
 *
 * Description:
 */
ktlService *ktlService::getService( ktlSystem *system, char *name, int debug )
{
    // search for service with this name; return if found; create if not
    ktlService *service;
    for ( service = ktlService::first(); service != NULL;
					 service = service->next() ) {
	if ( strcmp( service->name_, name ) == 0 )
	    break;
    }

    return ( service != NULL ) ? service : new ktlService( system, name, debug);
}

/*+
 * Routine:     ktlService::first
 *
 * Purpose:     ktlService first service routine
 *
 * Description:
 */
ktlService *ktlService::first()
{
    ktlNode *node = ( ktlNode * ) ellFirst( &list_ );
    return ( node != NULL ) ? ( ktlService * ) node->getData() : NULL; 
}

/*+
 * Routine:     ktlService::next
 *
 * Purpose:     ktlService next service routine
 *
 * Description:
 */
ktlService *ktlService::next() const
{
    ktlNode *node = ( ktlNode * ) ellNext( ( ELLNODE * ) &node_ );
    return ( node != NULL ) ? ( ktlService * ) node->getData() : NULL; 
}

/*+
 * Routine:     ktlService::add
 *
 * Purpose:     ktlService add keyword to hash table routine
 *
 * Description:
 */
void ktlService::add( const ktlKeyword *keyword )
{
    // ignore failure (means keyword had already been added)
    GPHENTRY *ent = gphAdd( keys_, keyword->getKeyName(), NULL );
    if ( ent != NULL ) ent->userPvt = ( void * ) keyword;
}

/*+
 * Routine:     ktlService::remove
 *
 * Purpose:     ktlService remove keyword from hash table routine
 *
 * Description:
 */
void ktlService::remove( const ktlKeyword *keyword )
{
    gphDelete( keys_, keyword->getKeyName(), NULL );
}

/*+
 * Routine:     ktlService::find
 *
 * Purpose:     ktlService find keyword in hash table routine
 *
 * Description:
 */
ktlKeyword *ktlService::find( const char *keyName )
{
    GPHENTRY *ent = gphFind( keys_, keyName, NULL );
    return ( ent != NULL ) ? ( ktlKeyword * ) ent->userPvt : NULL;
}

/*+
 * Routine:     ktlService::getFdsetsAll
 *
 * Purpose:     ktlService routine to return fd_set combining fds from all
 *		open services
 *
 * Description:
 */
fd_set *ktlService::getFdsetsAll()
{
    static fd_set fdset;
    FD_ZERO( &fdset );

    for ( ktlService *service = ktlService::first(); service != NULL;
		      service = service->next() ) {

	if ( service->handle_ == NULL )
	    continue;

	fd_set temp;
	( void ) ktl_ioctl( service->handle_, KTL_FDSET, &temp );
	for ( int fd = 0; fd < FD_SETSIZE; fd++ ) {
	    if ( FD_ISSET( fd, &temp ) ) {
		FD_SET( fd, &fdset );
	    }
	}
    }

    return &fdset;
}

/*+
 * Routine:     ktlService::dispatchAll
 *
 * Purpose:     ktlService routine to dispatch events on all open services
 *
 * Description:
 */
pvStat ktlService::dispatchAll()
{
    for ( ktlService *service = ktlService::first(); service != NULL;
		      service = service->next() ) {

	if ( service->handle_ == NULL )
	    continue;

	if ( ktl_dispatch( service->handle_ ) < 0 ) {
	    service->setError( -1, pvSevrMAJOR, pvStatERROR,
			       "dispatch failure" );
	    return pvStatERROR;
	}
    }

    return pvStatOK;
}

/*+
 * Routine:     ktlService::setError()
 *
 * Purpose:     Copy error information
 *
 * Description:
 *
 * Function value:
 */
void ktlService::setError( int status, pvSevr sevr, pvStat stat,
                           const char *mess )
{
    status_ = status;
    sevr_   = sevr;
    stat_   = stat;
    mess_   = Strdcpy( mess_, mess );
}

////////////////////////////////////////////////////////////////////////////////
/*+
 * Routine:     ktlKeyword::ktlKeyword
 *
 * Purpose:     ktlKeyword constructor
 *
 * Description:
 */
ktlKeyword::ktlKeyword( ktlService *service, const char *keyName, int debug ) :
    debug_( debug ),
    service_( service ),
    keyName_( strdup( keyName ) ),
    monitored_( FALSE ),
    status_( 0 ),
    sevr_( pvSevrNONE ),
    stat_( pvStatOK ),
    mess_( NULL )
{
    if ( getDebug() > 0 )
	printf( "%8p: ktlKeyword::ktlKeyword( %d )\n", this, debug );

    // add to service's hash table of keywords
    service->add( this );

    // initialize list of associated variables
    ellInit( &list_ );
}

/*+
 * Routine:     ktlKeyword::~ktlKeyword
 *
 * Purpose:     ktlKeyword destructor
 *
 * Description:
 */
ktlKeyword::~ktlKeyword()
{
    if ( getDebug() > 0 )
        printf( "%8p: ktlKeyword::~ktlKeyword()\n", this );

    // remove any remaining associated variables from list and free list
    for ( ktlVariable *variable = first(); variable != NULL;
					   variable = next( variable ) ) {
	remove( variable );
    }
    ellFree( &list_ );

    // remove from service's hash table of keywords
    service_->remove( this );

    // free keyword name
    if ( keyName_ != NULL )
	free( ( char * ) keyName_ );
}

/*+
 * Routine:     ktlKeyword::getKeyword
 *
 * Purpose:     ktlKeyword static accessor routine
 *
 * Description:
 */
ktlKeyword *ktlKeyword::getKeyword( ktlService *service, const char *keyName,
				    int debug )
{
    // search for keyword with this name; return if found; create if not
    ktlKeyword *keyword = service->find( keyName );
    return ( keyword != NULL ) ? keyword :
				 new ktlKeyword( service, keyName, debug );
}

/*+
 * Routine:     ktlKeyword::add
 *
 * Purpose:     ktlKeyword add variable routine
 *
 * Description:
 */
int ktlKeyword::add( ktlVariable *variable )
{
    if ( getDebug() > 0 )
        printf( "%8p: ktlKeyword::add( %p )\n", this, variable );

    ktlNode *node = variable->getNode();
    node->setData( variable );
    ellAdd( &list_, ( ELLNODE * ) node );

    // attempt to enable connection handler
    INVOKE( ktl_ioctl( variable->getHandle(), KTL_KEYCONNREG,
		       variable->getKeyName(), connectionHandler, this ) );
    return getStatus();
}

/*+
 * Routine:     ktlKeyword::remove
 *
 * Purpose:     ktlKeyword remove variable routine
 *
 * Description:
 */
void ktlKeyword::remove( ktlVariable *variable )
{
    if ( getDebug() > 0 )
        printf( "%8p: ktlKeyword::remove( %p )\n", this, variable );

    ( void ) monitorOff( variable );
    ellDelete( &list_, ( ELLNODE * ) variable->getNode() );
}

/*+
 * Routine:     ktlKeyword::monitorOn
 *
 * Purpose:     ktlKeyword monitor on variable routine
 *
 * Description:
 */
int ktlKeyword::monitorOn( ktlVariable *variable )
{
    if ( getDebug() > 0 )
        printf( "%8p: ktlKeyword::monitorOn( %p, %d )\n", this, variable,
		monitored_ );

    if ( ! monitored_++ ) {
	KTL_CONTEXT *context;
	INVOKE( ktl_context_create( variable->getHandle(), ( int(*)() )
				    monitorHandler, NULL, NULL, &context ) );
	if ( getStat() == pvStatOK ) {
	    int flags = KTL_ENABLEREADCONT | KTL_NOPRIME | variable->getFlags();
	    INVOKE( ktl_read( variable->getHandle(), flags, variable->
			      getKeyName(), ( void * ) this, NULL, context ) );
	    ( void ) ktl_context_delete( context );
	}
    }
    return getStatus();
}

/*+
 * Routine:     ktlKeyword::monitorOff
 *
 * Purpose:     ktlKeyword monitor off variable routine
 *
 * Description:
 */
int ktlKeyword::monitorOff( ktlVariable *variable )
{
    if ( getDebug() > 0 )
        printf( "%8p: ktlKeyword::monitorOff( %p, %d )\n", this, variable,
		monitored_ );

    if ( ! --monitored_ ) {
	int flags = KTL_DISABLEREADCONT | variable->getFlags();
	INVOKE( ktl_read( variable->getHandle(), flags, variable->getKeyName(),
			  NULL, NULL, NULL ) );
    }
    return getStatus();
}

/*+
 * Routine:     ktlKeyword::first
 *
 * Purpose:     ktlKeyword first variable routine
 *
 * Description:
 */
ktlVariable *ktlKeyword::first()
{
    ktlNode *node = ( ktlNode * ) ellFirst( &list_ );
    return ( node != NULL ) ? ( ktlVariable * ) node->getData() : NULL; 
}

/*+
 * Routine:     ktlKeyword::next
 *
 * Purpose:     ktlKeyword next variable routine
 *
 * Description:
 */
ktlVariable *ktlKeyword::next( ktlVariable *variable )
{
    ktlNode *node = ( ktlNode * ) ellNext( ( ELLNODE * ) variable->getNode() );
    return ( node != NULL ) ? ( ktlVariable * ) node->getData() : NULL; 
}

/*+
 * Routine:     ktlKeyword::setError()
 *
 * Purpose:     Copy error information
 *
 * Description:
 *
 * Function value:
 */
void ktlKeyword::setError( int status, pvSevr sevr, pvStat stat,
                           const char *mess )
{
    status_ = status;
    sevr_   = sevr;
    stat_   = stat;
    mess_   = Strdcpy( mess_, mess );
}

////////////////////////////////////////////////////////////////////////////////
/*+
 * Routine:     ktlVariable::ktlVariable
 *
 * Purpose:     ktlVariable constructor
 *
 * Description:
 */
ktlVariable::ktlVariable( ktlSystem *system, const char *name, pvConnFunc func,
		          void *priv, int debug ) :
    pvVariable( system, name, func, priv, debug ),
    service_( ktlService::getService( system, serviceName( name ), getDebug())),
    keyName_( strdup( keywordName( name ) ) ),
    keyword_( ktlKeyword::getKeyword( service_, keyName_, getDebug() ) ),
    type_( KTL_DOUBLE ),
    count_( 1 ),
    readNot_( FALSE ),
    writeNot_( FALSE ),
    readCont_( FALSE ),
    connected_( FALSE ),
    monitor_( NULL )
{
    if ( getDebug() > 0 )
	printf( "%8p: ktlVariable::ktlVariable( %s, %d )\n",
		this, name, debug );

    // use KTL_FLAGS to check whether keyword exists
    // ### ignored at the moment (don't want to abort part of constructor
    //     because complicates destructor)
    int flags;
    INVOKE( ktl_ioctl( getHandle(), KTL_FLAGS, keyName_, &flags ) );

    // cache keyword attributes
    INVOKE( ktl_ioctl( getHandle(), KTL_TYPE | KTL_BINOUT, keyName_,
		       &type_, &count_ ) );

    KTL_IMPL_CAPS impl_caps;
    INVOKE( ktl_ioctl( getHandle(), KTL_IMPLCAPS, &impl_caps ) );
    // ### will be FALSE (should have per keyword inquiry)
    readNot_ = impl_caps.read_notify;

    INVOKE( ktl_ioctl( getHandle(), KTL_CANWRITENOTIFY, keyName_,
		       &writeNot_ ) );
    INVOKE( ktl_ioctl( getHandle(), KTL_CANREADCONTINUOUS, keyName_,
		       &readCont_ ) );

    // monitor the keyword (won't result in any user callbacks being called
    // because no callback object has yet been allocated; is done to force
    // a connection request to be sent to the control system)
    INVOKE( keyword_->monitorOn( this ) );

    // add variable to keyword object's list; this enables the connection
    // handler (interpret error as meaning that KTL_KEYCONNREG is not
    // supported, so mark connected and invoke function, if supplied)
    INVOKE( keyword_->add( this ) );
    if ( getStat() != pvStatOK ) {
	connected_ = TRUE;
	if ( func != NULL )
	    ( *func ) ( ( void * ) this, connected_ );
    }
}

/*+
 * Routine:     ktlVariable::~ktlVariable
 *
 * Purpose:     ktlVariable destructor
 *
 * Description:
 */
ktlVariable::~ktlVariable()
{
    if ( getDebug() > 0 )
        printf( "%8p: ktlVariable::~ktlVariable( %s )\n", this, keyName_ );

    // remove variable from keyword object's list
    keyword_->remove( this );

    // ### if this is last variable for this keyword, destroy keyword

    // ### if this is last variable for this service, destroy service

    // free keyword name
    if ( keyName_ != NULL )
	free( ( void * ) keyName_ );
}

/*+
 * Routine:     ktlVariable::get
 *
 * Purpose:     ktlVariable blocking get routine
 *
 * Description:
 */
pvStat ktlVariable::get( pvType type, int count, pvValue *value )
{
    if ( getDebug() > 0 )
        printf( "%8p: ktlVariable::get( %d, %d )\n", this, type, count );

    KTL_ANYPOLY ktlValue;

    LOCK;
    INVOKE( ktl_read( getHandle(), KTL_WAIT | getFlags() , keyName_, NULL,
		      &ktlValue, NULL ) );
    UNLOCK;

    if ( getStat() == pvStatOK ) {
	INVOKE( copyFromKTL( getFlags(), type_, count_, &ktlValue,
			     type, count, value ) );
	freeKTL( type_, KTL_POLYDATA( getFlags(), &ktlValue ) );
    }

    return getStat();
}

/*+
 * Routine:     ktlVariable::getNoBlock
 *
 * Purpose:     ktlVariable non-blocking get routine
 *
 * Description:
 */
pvStat ktlVariable::getNoBlock( pvType type, int count, pvValue *value )
{
    if ( getDebug() > 0 )
        printf( "%8p: ktlVariable::getNoBlock( %d, %d )\n", this,
		type, count );

    KTL_ANYPOLY ktlValue;

    LOCK;
    INVOKE( ktl_read( getHandle(), KTL_NOWAIT | getFlags(), keyName_, NULL,
		      &ktlValue, NULL ) );
    UNLOCK;

    if ( getStat() == pvStatOK ) {
        INVOKE( copyFromKTL( getFlags(), type_, count_, &ktlValue,
			     type, count, value ) );
	freeKTL( type_, KTL_POLYDATA( getFlags(), &ktlValue ) );
    }

    return getStat();
}

/*+
 * Routine:     ktlVariable::getCallback
 *
 * Purpose:     ktlVariable get with callback routine
 *
 * Description:
 */
pvStat ktlVariable::getCallback( pvType type, int count,
			         pvEventFunc func, void *arg )
{
    if ( getDebug() > 0 )
        printf( "%8p: ktlVariable::getCallback( %d, %d, %p, %p )\n",
		this, type, count, func, arg );

    if ( !readNot_ ) {
	// ### larger than needed (status not checked)
	pvValue *value = new pvValue[count];
	pvStat stat = get( type, count, value );
	( *func ) ( ( void * ) this, type, count, value, arg, stat );
	delete [] value;

    } else {
	pvCallback *callback = new pvCallback( this, type, count, func, arg,
					       getDebug() );

	KTL_CONTEXT *context;
	INVOKE( ktl_context_create( getHandle(), ( int(*)() ) accessHandler,
				    NULL, NULL, &context ) );
	if ( getStat() == pvStatOK ) {
	    LOCK;
	    INVOKE( ktl_read( getHandle(), KTL_NOTIFY | getFlags(), keyName_,
			      ( void * ) callback, NULL, context ) );
	    context->state = KTL_MESSAGE_STATE;
	    UNLOCK;
	}
    }

    return getStat();
}

/*+
 * Routine:     ktlVariable::put
 *
 * Purpose:     ktlVariable blocking put routine
 *
 * Description:
 */
pvStat ktlVariable::put( pvType type, int count, pvValue *value )
{
    if ( getDebug() > 0 )
        printf( "%8p: ktlVariable::put( %d, %d )\n", this, type, count );

    KTL_POLYMORPH ktlValue;

    INVOKE( copyToKTL( type, count, value, type_, count_, &ktlValue ) );
    if ( getStat() == pvStatOK ) {
	LOCK;
	INVOKE( ktl_write( getHandle(), KTL_WAIT, keyName_, NULL, &ktlValue,
			   NULL ) );
	UNLOCK;
	freeKTL( type_, &ktlValue );
    }

    return getStat();
}

/*+
 * Routine:     ktlVariable::putNoBlock
 *
 * Purpose:     ktlVariable non-blocking put routine
 *
 * Description:
 */
pvStat ktlVariable::putNoBlock( pvType type, int count, pvValue *value )
{
    if ( getDebug() > 0 )
        printf( "%8p: ktlVariable::putNoBlock( %d, %d )\n", this,
		type, count );

    KTL_POLYMORPH ktlValue;

    INVOKE( copyToKTL( type, count, value, type_, count_, &ktlValue ) );
    if ( getStat() == pvStatOK ) {
	LOCK;
	INVOKE( ktl_write( getHandle(), KTL_NOWAIT, keyName_, NULL, &ktlValue,
			   NULL ) );
	UNLOCK;
	freeKTL( type_, &ktlValue );
    }

    return getStat();
}

/*+
 * Routine:     ktlVariable::putCallback
 *
 * Purpose:     ktlVariable put with callback routine
 *
 * Description:
 */
pvStat ktlVariable::putCallback( pvType type, int count, pvValue *value,
			         pvEventFunc func, void *arg )
{
    if ( getDebug() > 0 )
        printf( "%8p: ktlVariable::putCallback( %d, %d )\n",
		this, type, count );

    if ( !writeNot_ ) {
	pvStat stat = put( type, count, value ); 
	( *func ) ( ( void * ) this, type, count, value, arg, stat );

    } else {
	pvCallback *callback = new pvCallback( this, type, count, func, arg,
					       getDebug() );

	KTL_POLYMORPH ktlValue;

	INVOKE( copyToKTL( type, count, value, type_, count_, &ktlValue ) );
	if ( getStat() == pvStatOK ) {
	    KTL_CONTEXT *context;
	    INVOKE( ktl_context_create( getHandle(), ( int(*)() ) accessHandler,
					NULL, NULL, &context ) );
	    if ( getStat() == pvStatOK ) {
		LOCK;
		INVOKE( ktl_write( getHandle(), KTL_NOTIFY, keyName_,
				   ( void * ) callback, &ktlValue, context ) );
		context->state = KTL_MESSAGE_STATE;
		UNLOCK;
	    }
	}
	freeKTL( type_, &ktlValue );
    }

    return getStat();
}

/*+
 * Routine:     ktlVariable::monitorOn
 *
 * Purpose:     ktlVariable monitor enable routine
 *
 * Description:
 */
pvStat ktlVariable::monitorOn( pvType type, int count, pvEventFunc func,
			       void *arg, pvCallback **pCallback )
{
    if ( getDebug() > 0 )
        printf( "%8p: ktlVariable::monitorOn( %s, %d, %d )\n",
		this, keyName_, type, count );

    if ( !readCont_ ) {
	setError( -1, pvSevrMAJOR, pvStatERROR, "monitoring not supported" );
	if ( pCallback != NULL )
	    *pCallback = NULL;

    // ### can only have single monitor enabled per variable (can have
    //     multiple variables per keyword though)
    } else if ( monitor_ != NULL ) {
	setError( -1, pvSevrMAJOR, pvStatERROR, "monitor already enabled" );
	if ( pCallback != NULL )
	    *pCallback = NULL;
    } else {
	monitor_ = new pvCallback( this, type, count, func, arg, getDebug() );
	LOCK;
	INVOKE( keyword_->monitorOn( this ) );
	UNLOCK;
	if ( pCallback != NULL )
	    *pCallback = monitor_;
    }

    return getStat();
}

/*+
 * Routine:     ktlVariable::monitorOff
 *
 * Purpose:     ktlVariable monitor disable routine
 *
 * Description:
 */
pvStat ktlVariable::monitorOff( pvCallback *callback )
{
    if ( getDebug() > 0 )
        printf( "%8p: ktlVariable::monitorOff()\n", this );

    LOCK;
    keyword_->monitorOff( this );
    UNLOCK;

    // ### are we sure that no further monitors can be delivered?
    if ( monitor_ != NULL ) {
	delete monitor_;
	monitor_ = NULL;
    }

    return getStat();
}

/*+
 * Routine:     ktlVariable::getType
 *
 * Purpose:     ktlVariable "what type are we?" routine
 *
 * Description:
 */
pvType ktlVariable::getType() const
{
    if ( getDebug() > 1 )
        printf( "%8p: ktlVariable::getType()\n", this );

    return typeFromKTL( type_ ); 
}

////////////////////////////////////////////////////////////////////////////////
/*+
 * Routine:     connectionHandler
 *
 * Purpose:     KTL connection handler
 *
 * Description:
 */
static void connectionHandler( ktlKeyword *keyword, char *, int connected )
{
    if ( keyword->getDebug() > 0 )
	printf( "%8p: ktlKeyword::connectionHandler( %d )\n", keyword,
		connected );

    for ( ktlVariable *variable = keyword->first(); variable != NULL;
				       variable = keyword->next( variable ) ) {

	if ( variable->getDebug() > 0 )
	    printf( "%8p: ktlVariable::connectionHandler( %s, %d )\n", variable,
		    variable->getKeyName(), connected );

	variable->setConnected( connected );

	pvConnFunc func = variable->getFunc();
	if ( func != NULL ) 
	    ( *func ) ( ( void * ) variable, connected );
    }
}

/*+
 * Routine:     accessHandler
 *
 * Purpose:     KTL get/put completion event handler
 *
 * Description:
 */
static void accessHandler( char *keyName, pvCallback *callback,
			   KTL_ANYPOLY *ktlValue, KTL_CONTEXT *context )
{
    ktlVariable *variable = ( ktlVariable * ) callback->getVariable();
    int ktlFlags = variable->getFlags();

    if ( variable->getDebug() > 0 )
	printf( "%8p: ktlVariable::accessHandler( %s, %d )\n", variable,
		variable->getKeyName(), KTL_POLYDATA(ktlFlags,ktlValue)->i );

    commonHandler( keyName, callback, ktlValue, context );
    delete callback;
}

/*+
 * Routine:     monitorHandler
 *
 * Purpose:     KTL monitor event handler
 *
 * Description:
 */
static void monitorHandler( char *keyName, ktlKeyword *keyword,
			    KTL_ANYPOLY *ktlValue, KTL_CONTEXT *context )
{
    int ktlFlags = keyword->getFlags();

    if ( keyword->getDebug() > 0 )
	printf( "%8p: ktlKeyword::monitorHandler( %d )\n", keyword,
		KTL_POLYDATA(ktlFlags,ktlValue)->i );

    for ( ktlVariable *variable = keyword->first(); variable != NULL;
				       variable = keyword->next( variable ) ) {

	if ( variable->getDebug() > 0 )
	    printf( "%8p: ktlVariable::monitorHandler( %s )\n", variable,
		    variable->getKeyName() );

	pvCallback *monitor = variable->getMonitor();
	if ( monitor != NULL )
	    commonHandler( keyName, monitor, ktlValue, context );
    }
}

/*+
 * Routine:     commonHandler
 *
 * Purpose:     KTL completion/monitor event handler (work routine)
 *
 * Description:
 */
static void commonHandler( char *, pvCallback *callback,
			   KTL_ANYPOLY *ktlValue, KTL_CONTEXT *context )
{
    pvEventFunc func = callback->getFunc();
    ktlVariable *variable = ( ktlVariable * ) callback->getVariable();
    int ktlFlags = variable->getFlags();
    KTL_DATATYPE ktlType = variable->getKtlType();
    int ktlCount = variable->getCount();
    pvType type = callback->getType();
    int count = callback->getCount();
    void *arg = callback->getArg();

    // ### larger than needed (status not checked)
    pvValue *value = new pvValue[count];
    // ### function not called if copy from KTL failed
    if ( copyFromKTL( ktlFlags, ktlType, ktlCount, ktlValue,
		      type, count, value ) >= 0 ) {
	( *func ) ( ( void * ) variable, type, count, value, arg,
		    statFromKTL( context->status ) );
    }
    delete [] value;
}

////////////////////////////////////////////////////////////////////////////////
/*+
 * Routine:     serviceName()
 *
 * Purpose:     service name from "srv.key" name
 *
 * Description:
 */
static char *serviceName( const char *name )
{
    static char temp[256];
    size_t len = strcspn( name, "." );
    strncpy( temp, name, len );
    temp[255] = '\0';
    
    return temp;
}

/*+
 * Routine:     keywordName
 *
 * Purpose:     keyword name from "srv.key" name
 *
 * Description:
 */
static char *keywordName( const char *name )
{
    static char temp[256];
    size_t len = strcspn( name, "." );
    strncpy( temp, name + len + 1, 256 - len - 1 );
    temp[255] = '\0';
    
    return temp;
}

/*+
 * Routine:     sevrFromKTL
 *
 * Purpose:     extract pvSevr from KTL status
 *
 * Description:
 */
static pvSevr sevrFromKTL( int status )
{
    return ( status < 0 ) ? pvSevrERROR : pvSevrNONE;
}

/*+
 * Routine:     statFromKTL
 *
 * Purpose:     extract pvStat from KTL status
 *
 * Description:
 */
static pvStat statFromKTL( int status )
{
    pvSevr sevr = sevrFromKTL( status );
    return ( sevr == pvSevrNONE || sevr == pvSevrMINOR ) ?
		pvStatOK : pvStatERROR;
}

/*+
 * Routine:     typeFromKTL
 *
 * Purpose:     extract pvType from KTL type
 *
 * Description:
 */
static pvType typeFromKTL( int type )
{
    switch ( type ) {
      case KTL_INT:          return pvTypeLONG;
      case KTL_BOOLEAN:      return pvTypeLONG;
      case KTL_ENUM:         return pvTypeLONG;
      case KTL_ENUMM:        return pvTypeLONG;
      case KTL_MASK:         return pvTypeLONG;
      case KTL_FLOAT:        return pvTypeFLOAT;
      case KTL_DOUBLE:       return pvTypeDOUBLE;
      case KTL_STRING:       return pvTypeSTRING;
      case KTL_INT_ARRAY:    return pvTypeLONG;
      case KTL_FLOAT_ARRAY:  return pvTypeFLOAT;
      case KTL_DOUBLE_ARRAY: return pvTypeDOUBLE;
      default:               return pvTypeERROR;
    }
}

/*+
 * Routine:     copyFromKTL
 *
 * Purpose:     copy KTL value to pvValue
 *
 * Description:
 */
static int copyFromKTL( int ktlFlags, KTL_DATATYPE ktlType, int ktlCount,
			const KTL_ANYPOLY *ktlValue, pvType type, int count,
			pvValue *value )
{
    // map types to ktlCnv's types (indices into conversion table)
    ktlCnv::type inpType = ktlCnv::getType( ktlType );
    ktlCnv::type outType = ktlCnv::getType( type );

    // if type is not simple, copy status, severity and time stamp
    // (note assumption that STAT and SEVR can be cast; this is in fact true)
    if ( !PV_SIMPLE( type ) ) {
	epicsTimeStamp stamp;
	if ( ktlFlags & KTL_SUPER ) {
	    value->timeCharVal.status = ( pvStat ) ktlValue->super.stat;
	    value->timeCharVal.severity = ( pvSevr ) ktlValue->super.sevr;
	    if ( ktlFlags & KTL_STAMP ) {
		epicsTimeFromTimeval( &value->timeCharVal.stamp,
				    &ktlValue->super.stamp );
	    }
	    else {
		( void ) epicsTimeGetCurrent( &stamp );
		value->timeCharVal.stamp = stamp;
	    }
	} else {
	    ( void ) epicsTimeGetCurrent( &stamp );
	    value->timeCharVal.status = pvStatOK;
	    value->timeCharVal.severity = pvSevrNONE;
	    value->timeCharVal.stamp = stamp;
	}
    }

    // convert data
    const KTL_POLYMORPH *ktlData = KTL_POLYDATA( ktlFlags, ktlValue );
    return ktlCnv::convert( inpType, ktlCount, KTL_VALPTR( ktlType, ktlData ),
			    outType, count,    PV_VALPTR( type, value ) );
}

/*+
 * Routine:     copyToKTL
 *
 * Purpose:     copy pvValue to KTL value
 *
 * Description:
 */
static int copyToKTL( pvType type, int count, const pvValue *value,
		KTL_DATATYPE ktlType, int ktlCount, KTL_POLYMORPH *ktlValue )
{
    // map types to ktlCnv's types (indices into conversion table)
    ktlCnv::type inpType = ktlCnv::getType( type );
    ktlCnv::type outType = ktlCnv::getType( ktlType );

    // allocate memory for KTL array or string
    if ( mallocKTL( ktlType, ktlCount, ktlValue ) < 0 )
	return -1;

    // convert data
    return ktlCnv::convert( inpType, count,    PV_VALPTR( type, value ),
			    outType, ktlCount, KTL_VALPTR( ktlType, ktlValue ));
}

/*+
 * Routine:     mallocKTL
 *
 * Purpose:     allocate dynamic data referenced from a polymorph
 *
 * Description:
 */
static int mallocKTL( KTL_DATATYPE ktlType, int ktlCount,
					    KTL_POLYMORPH *ktlValue )
{
    switch ( ktlType ) {
      case KTL_STRING:
	ktlValue->s = new char[sizeof(pvString)];
	if ( ktlValue->s == NULL ) goto error;
	break;
      case KTL_INT_ARRAY:
	ktlValue->ia = new int[ktlCount];
	if ( ktlValue->ia == NULL ) goto error;
	break;
      case KTL_FLOAT_ARRAY:
	ktlValue->fa = new float[ktlCount];
	if ( ktlValue->fa == NULL ) goto error;
	break;
      case KTL_DOUBLE_ARRAY:
	ktlValue->da = new double[ktlCount];
	if ( ktlValue->da == NULL ) goto error;
	break;
      default:
	break;
    }
    return 0;
error:
    ktl_set_errtxt( "memory allocation failure" );
    return -1;
}

/*+
 * Routine:     freeKTL
 *
 * Purpose:     free data referenced from a polymorph
 *
 * Description:
 */
static void freeKTL( KTL_DATATYPE ktlType, KTL_POLYMORPH *ktlValue )
{
    // do nothing if pointer (this is also ia, fa and da) is NULL
    if ( ktlValue->s == NULL )
	return;

    switch ( ktlType ) {
      case KTL_STRING:
	delete [] ktlValue->s;
	break;
      case KTL_INT_ARRAY:
	delete [] ktlValue->ia;
	break;
      case KTL_FLOAT_ARRAY:
	delete [] ktlValue->fa;
	break;
      case KTL_DOUBLE_ARRAY:
	delete [] ktlValue->da;
	break;
      default:
	break;
    }
}

/*
 * pvKtl.cc,v
 * Revision 1.2  2001/02/16 18:45:39  mrk
 * changes for latest version of 3.14
 *
 * Revision 1.1.1.1  2000/04/04 03:22:14  wlupton
 * first commit of seq-2-0-0
 *
 * Revision 1.19  2000/03/31 23:01:41  wlupton
 * supported setStatus
 *
 * Revision 1.18  2000/03/29 23:28:05  wlupton
 * corrected comment typo
 *
 * Revision 1.17  2000/03/29 02:00:45  wlupton
 * monitor all keywords (forces connect request to be sent)
 *
 * Revision 1.16  2000/03/18 04:00:25  wlupton
 * converted to use new configure scheme
 *
 * Revision 1.15  2000/03/16 02:11:29  wlupton
 * supported KTL_ANYPOLY (plus misc other mods)
 *
 * Revision 1.14  2000/03/08 01:32:29  wlupton
 * cut out some early error exits in constructors (to avoid crashes in destructors)
 *
 * Revision 1.13  2000/03/07 19:55:32  wlupton
 * nearly sufficient tidying up
 *
 * Revision 1.12  2000/03/07 09:27:39  wlupton
 * drastically reduced use of references
 *
 * Revision 1.11  2000/03/07 08:46:29  wlupton
 * created ktlKeyword class (works but a bit messy)
 *
 * Revision 1.10  2000/03/06 19:21:04  wlupton
 * misc error reporting and type conversion mods
 *
 * Revision 1.9  2000/03/01 02:07:14  wlupton
 * converted to use new OSI library
 *
 * Revision 1.8  2000/02/19 01:12:22  wlupton
 * misc tidy-up and bug fixes
 *
 * Revision 1.7  2000/02/14 21:33:07  wlupton
 * fixed workshop 5.0 compilation error
 *
 * Revision 1.6  1999/10/12 02:53:13  wlupton
 * fixed KTL_NOTIFY support (cannot have been tested)
 *
 * Revision 1.5  1999/09/07 20:43:21  epics
 * increased debug level for pend() call
 *
 * Revision 1.4  1999/07/01 20:50:20  wlupton
 * Working under VxWorks
 *
 * Revision 1.3  1999/06/29 01:56:25  wlupton
 * always use 0.1s select() timeout because of startup problem seeing fds open
 *
 * Revision 1.2  1999/06/15 10:11:03  wlupton
 * demo sequence mostly working with KTL
 *
 * Revision 1.1  1999/06/11 02:20:32  wlupton
 * nearly working with KTL
 *
 */
