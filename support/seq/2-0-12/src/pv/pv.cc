/* pv.cc,v 1.4 2001/02/16 21:45:16 norume Exp
 *
 * Implementation of EPICS sequencer message system-independent library (pv)
 * (NB, "pv" = "process variable").
 *
 * William Lupton, W. M. Keck Observatory
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define epicsExportSharedSymbols
#include "pv.h"

////////////////////////////////////////////////////////////////////////////////
/*+
 * Routine:	pvSystem::pvSystem
 *
 * Purpose:	pvSystem constructor
 *
 * Description:	
 */
epicsShareFunc pvSystem::pvSystem( int debug ) :

    magic_( PV_MAGIC ),
    debug_( debug ),
    status_( 0 ),
    sevr_( pvSevrNONE ),
    stat_( pvStatOK ),
    mess_( NULL ),
    lock_( epicsMutexMustCreate() )
{
    if ( getDebug() > 0 )
	printf( "%8p: pvSystem::pvSystem( %d )\n", (void *)this, debug );
}

/*+
 * Routine:	pvSystem::~pvSystem
 *
 * Purpose:	pvSystem destructor
 *
 * Description:	
 */
epicsShareFunc pvSystem::~pvSystem()
{
    if ( getDebug() > 0 )
	printf( "%8p: pvSystem::~pvSystem()\n", (void *)this );
}

/*+
 * Routine:	pvSystem::lock()/unlock()
 *
 * Purpose:	Take/give lock
 *
 * Description: 
 *
 * Function value:
 */
epicsShareFunc void pvSystem::lock()
{
    epicsMutexMustLock( lock_ );

    if ( getDebug() > 1 )
	printf( "%8p: pvSystem::lock()\n", (void *)this );
}

epicsShareFunc void pvSystem::unlock()
{
    epicsMutexUnlock( lock_ );

    if ( getDebug() > 1 )
	printf( "%8p: pvSystem::unlock()\n", (void *)this );
}

/*+
 * Routine:	pvSystem::setError()
 *
 * Purpose:	Copy error information
 *
 * Description: 
 *
 * Function value:
 */
epicsShareFunc void pvSystem::setError( int status, pvSevr sevr, pvStat stat,
			 const char *mess )
{
    status_ = status;
    sevr_   = sevr;
    stat_   = stat;
    mess_   = Strdcpy( mess_, mess );
}

////////////////////////////////////////////////////////////////////////////////
/*+
 * Routine:     pvVariable::pvVariable
 *
 * Purpose:     pvVariable constructor
 *
 * Description:
 */
pvVariable::pvVariable( pvSystem *system, const char *name, pvConnFunc func,
		        void *priv, int debug ) :
    magic_( PV_MAGIC ),
    debug_( debug ),
    func_( func ),
    system_( system ),
    name_( Strdup( name ) ),
    private_( priv ),
    status_( 0 ),
    sevr_( pvSevrNONE ),
    stat_( pvStatOK ),
    mess_( NULL )
{
    if ( getDebug() == 0 )
	setDebug( system->getDebug() );

    if ( getDebug() > 0 )
	printf( "%8p: pvVariable::pvVariable( %s, %d )\n",
		(void *)this, name, debug );
}

/*+
 * Routine:     pvVariable::~pvVariable
 *
 * Purpose:     pvVariable destructor
 *
 * Description:
 */
pvVariable::~pvVariable()
{
    if ( getDebug() > 0 )
        printf( "%8p: pvVariable::~pvVariable()\n", (void *)this );

    if ( name_ != NULL )
	free( name_ );
}

/*+
 * Routine:	pvVariable::setError()
 *
 * Purpose:	Copy error information
 *
 * Description: 
 *
 * Function value:
 */
void pvVariable::setError( int status, pvSevr sevr, pvStat stat,
			   const char *mess )
{
    status_ = status;
    sevr_   = sevr;
    stat_   = stat;
    mess_   = Strdcpy( mess_, mess );
}

////////////////////////////////////////////////////////////////////////////////
/*+
 * Routine:     pvCallback::pvCallback
 *
 * Purpose:     pvCallback constructor
 *
 * Description:
 */
pvCallback::pvCallback( pvVariable *variable, pvType type, int count,
			pvEventFunc func, void *arg, int debug ) :
    magic_( PV_MAGIC ),
    debug_( debug ),
    variable_( variable ),
    type_( type ),
    count_( count ),
    func_( func ),
    arg_( arg ),
    private_( NULL )
{
    // should be associated with a system?
    if ( getDebug() > 0 )
	printf( "%8p: pvCallback::pvCallback( %d, %d, %p, %p, %d )\n",
		(void *)this, type, count, (void *)func, arg, debug );
}

/*+
 * Routine:     pvCallback::~pvCallback
 *
 * Purpose:     pvCallback destructor
 *
 * Description:
 */
pvCallback::~pvCallback()
{
    if ( getDebug() > 0 )
        printf( "%8p: pvCallback::~pvCallback()\n", (void *)this );
}

////////////////////////////////////////////////////////////////////////////////
/* C interface */

#define SYS_CHECK(_erract) \
    pvSystem *Sys = ( pvSystem * ) sys; \
    if ( Sys == NULL || Sys->getMagic() != PV_MAGIC ) \
	_erract

epicsShareFunc pvStat epicsShareAPI pvSysCreate( const char *name, int debug, void **pSys ) {
    *pSys = newPvSystem( name, debug );
    return ( *pSys == NULL ) ? pvStatERROR : pvStatOK;
}

epicsShareFunc pvStat epicsShareAPI pvSysDestroy( void *sys ) {
    SYS_CHECK( return pvStatERROR );
    delete Sys;
    return pvStatOK;
}

epicsShareFunc pvStat epicsShareAPI pvSysAttach( void *sys ) {
    SYS_CHECK( return pvStatERROR );
    return Sys->attach();
}

epicsShareFunc pvStat epicsShareAPI pvSysFlush( void *sys ) {
    SYS_CHECK( return pvStatERROR );
    return Sys->flush();
}

epicsShareFunc pvStat epicsShareAPI pvSysPend( void *sys, double seconds, int wait ) {
    SYS_CHECK( return pvStatERROR );
    return Sys->pend( seconds, wait );
}

epicsShareFunc pvStat epicsShareAPI pvSysLock( void *sys ) {
    SYS_CHECK( return pvStatERROR );
    Sys->lock();
    return pvStatOK;
}

epicsShareFunc pvStat epicsShareAPI pvSysUnlock( void *sys ) {
    SYS_CHECK( return pvStatERROR );
    Sys->unlock();
    return pvStatOK;
}

epicsShareFunc int epicsShareAPI pvSysGetMagic( void *sys ) {
    SYS_CHECK( return pvStatERROR );
    return Sys->getMagic();
}

epicsShareFunc void epicsShareAPI pvSysSetDebug( void *sys, int debug ) {
    SYS_CHECK( ; );
    Sys->setDebug( debug );
}

epicsShareFunc int epicsShareAPI pvSysGetDebug( void *sys ) {
    SYS_CHECK( return pvStatERROR );
    return Sys->getDebug();
}

epicsShareFunc int epicsShareAPI pvSysGetStatus( void *sys ) {
    SYS_CHECK( return pvStatERROR );
    return Sys->getStatus();
}

epicsShareFunc pvSevr epicsShareAPI pvSysGetSevr( void *sys ) {
    SYS_CHECK( return pvSevrERROR );
    return Sys->getSevr();
}

epicsShareFunc pvStat epicsShareAPI pvSysGetStat( void *sys ) {
    SYS_CHECK( return pvStatERROR );
    return Sys->getStat();
}

epicsShareFunc char * epicsShareAPI pvSysGetMess( void *sys ) {
    SYS_CHECK( return ( char * ) "" );
    return Sys->getMess();
}

#define VAR_CHECK(_erract) \
    pvVariable *Var = ( pvVariable * ) var; \
    if ( Var == NULL || Var->getMagic() != PV_MAGIC ) \
	_erract

epicsShareFunc pvStat epicsShareAPI pvVarCreate( void *sys, const char *name, pvConnFunc func, void *priv,
		    int debug, void **pVar ) {
    SYS_CHECK( return pvStatERROR );
    *pVar = Sys->newVariable( name, func, priv, debug );
    return ( *pVar == NULL ) ? pvStatERROR : pvStatOK;
}

epicsShareFunc pvStat epicsShareAPI pvVarDestroy( void *var ) {
    VAR_CHECK( return pvStatERROR );
    delete Var;
    return pvStatOK;
}

epicsShareFunc pvStat epicsShareAPI pvVarGet( void *var, pvType type, int count, pvValue *value ) {
    VAR_CHECK( return pvStatERROR );
    return Var->get( type, count, value );
}

epicsShareFunc pvStat epicsShareAPI pvVarGetNoBlock( void *var, pvType type, int count, pvValue *value ) {
    VAR_CHECK( return pvStatERROR );
    return Var->getNoBlock( type, count, value );
}

epicsShareFunc pvStat epicsShareAPI pvVarGetCallback( void *var, pvType type, int count,
		         pvEventFunc func, void *arg ) {
    VAR_CHECK( return pvStatERROR );
    return Var->getCallback( type, count, func, arg );
}

epicsShareFunc pvStat epicsShareAPI pvVarPut( void *var, pvType type, int count, pvValue *value ) {
    VAR_CHECK( return pvStatERROR );
    return Var->put( type, count, value );
}

epicsShareFunc pvStat epicsShareAPI pvVarPutNoBlock( void *var, pvType type, int count, pvValue *value ) {
    VAR_CHECK( return pvStatERROR );
    return Var->putNoBlock( type, count, value );
}

epicsShareFunc pvStat epicsShareAPI pvVarPutCallback( void *var, pvType type, int count, pvValue *value,
                         pvEventFunc func, void *arg ) {
    VAR_CHECK( return pvStatERROR );
    return Var->putCallback( type, count, value, func, arg );
}

epicsShareFunc pvStat epicsShareAPI pvVarMonitorOn( void *var, pvType type, int count,
                       pvEventFunc func, void *arg, void **pCallback ) {
    VAR_CHECK( return pvStatERROR );
    return Var->monitorOn( type, count, func, arg, ( pvCallback ** ) pCallback);
}

epicsShareFunc pvStat epicsShareAPI pvVarMonitorOff( void *var, void *callback ) {
    VAR_CHECK( return pvStatERROR );
    return Var->monitorOff( ( pvCallback * ) callback );
}

epicsShareFunc int epicsShareAPI pvVarGetMagic( void *var ) {
    VAR_CHECK( return pvStatERROR );
    return Var->getMagic();
}

epicsShareFunc void epicsShareAPI pvVarSetDebug( void *var, int debug ) {
    VAR_CHECK( ; );
    Var->setDebug( debug );
}

epicsShareFunc int epicsShareAPI pvVarGetDebug( void *var ) {
    VAR_CHECK( return pvStatERROR );
    return Var->getDebug();
}

epicsShareFunc int epicsShareAPI pvVarGetConnected( void *var ) {
    VAR_CHECK( return pvStatERROR );
    return Var->getConnected();
}

epicsShareFunc pvType epicsShareAPI pvVarGetType( void *var ) {
    VAR_CHECK( return pvTypeERROR );
    return Var->getType();
}

epicsShareFunc int epicsShareAPI pvVarGetCount( void *var ) {
    VAR_CHECK( return pvStatERROR );
    return Var->getCount();
}

epicsShareFunc void epicsShareAPI pvVarSetPrivate( void *var, void *priv ) {
    VAR_CHECK( ; );
    Var->setPrivate( priv );
}

epicsShareFunc char *epicsShareAPI pvVarGetName( void *var ) {
    VAR_CHECK( return NULL );
    return Var->getName();
}

epicsShareFunc void *epicsShareAPI pvVarGetPrivate( void *var ) {
    VAR_CHECK( return NULL );
    return Var->getPrivate();
}

epicsShareFunc int epicsShareAPI pvVarGetStatus( void *var ) {
    VAR_CHECK( return pvStatERROR );
    return Var->getStatus();
}

epicsShareFunc pvSevr epicsShareAPI pvVarGetSevr( void *var ) {
    VAR_CHECK( return pvSevrERROR );
    return Var->getSevr();
}

epicsShareFunc pvStat epicsShareAPI pvVarGetStat( void *var ) {
    VAR_CHECK( return pvStatERROR );
    return Var->getStat();
}

epicsShareFunc char *epicsShareAPI pvVarGetMess( void *var ) {
    VAR_CHECK( return ( char * ) "" );
    return Var->getMess();
}

/*
 * Time utilities
 */
epicsShareFunc int epicsShareAPI pvTimeGetCurrentDouble( double *pTime ) {
    epicsTimeStamp stamp;

    *pTime = 0.0;
    if ( epicsTimeGetCurrent( &stamp ) == epicsTimeERROR )
	return pvStatERROR;

    *pTime = ( double ) stamp.secPastEpoch +  ( ( double ) stamp.nsec / 1e9 );
    return pvStatOK;
}

/*
 * Misc utilities
 */
epicsShareFunc char * epicsShareAPI Strdup( const char *s ) {
    char *p = ( char * ) malloc( strlen( s ) + 1 );
    if ( p != NULL )
	strcpy( p, s );
    return p;
}

epicsShareFunc char * epicsShareAPI Strdcpy( char *dst, const char *src ) {
    if ( dst != NULL && strlen( src ) > strlen( dst ) ) {
	free( dst );
	dst = NULL;
    }
    if ( dst == NULL )
	dst = ( char * ) malloc( strlen( src ) + 1 );
    if ( dst != NULL )
	strcpy( dst, src );
    return dst;
}

/*
 * pv.cc,v
 * Revision 1.4  2001/02/16 21:45:16  norume
 * Many 3.14-related changes.
 *
 * Revision 1.3  2001/02/16 18:45:39  mrk
 * changes for latest version of 3.14
 *
 * Revision 1.2  2000/04/14 21:53:28  jba
 * Changes for win32 build.
 *
 * Revision 1.1.1.1  2000/04/04 03:22:13  wlupton
 * first commit of seq-2-0-0
 *
 * Revision 1.16  2000/03/29 01:59:14  wlupton
 * added pvVarGetName
 *
 * Revision 1.15  2000/03/18 04:00:25  wlupton
 * converted to use new configure scheme
 *
 * Revision 1.14  2000/03/16 02:10:24  wlupton
 * added newly-needed debug argument
 *
 * Revision 1.13  2000/03/07 09:27:39  wlupton
 * drastically reduced use of references
 *
 * Revision 1.12  2000/03/06 19:19:13  wlupton
 * avoided compilation warnings
 *
 * Revision 1.11  2000/03/01 02:07:14  wlupton
 * converted to use new OSI library
 *
 * Revision 1.10  2000/02/19 01:08:55  wlupton
 * added dynamic allocation of message string (and support for both sys & var)
 *
 * Revision 1.9  2000/02/16 02:31:43  wlupton
 * merged in v1.9.5 changes
 *
 * Revision 1.8  1999/07/07 03:14:36  wlupton
 * corrected get/putNoBlock (mis-edit)
 *
 * Revision 1.7  1999/07/01 20:50:18  wlupton
 * Working under VxWorks
 *
 * Revision 1.6  1999/06/15 10:11:02  wlupton
 * demo sequence mostly working with KTL
 *
 * Revision 1.5  1999/06/10 00:35:03  wlupton
 * demo sequencer working with pvCa
 *
 * Revision 1.4  1999/06/08 19:21:43  wlupton
 * CA version working; about to use in sequencer
 *
 * Revision 1.3  1999/06/08 03:25:20  wlupton
 * nearly complete CA implementation
 *
 * Revision 1.2  1999/06/07 21:46:44  wlupton
 * working with simple pvtest program
 *
 * Revision 1.1  1999/06/04 20:48:27  wlupton
 * initial version of pv.h and pv.cc
 *
 */
