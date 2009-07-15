/* pv.h,v 1.3 2001/02/16 18:45:39 mrk Exp
 *
 * Definitions for EPICS sequencer message system-independent library (pv)
 * (NB, "pv" = "process variable").
 *
 * This is a simple layer which is specifically designed to provide the
 * facilities needed by the EPICS sequencer. Specific message systems are
 * expected to inherit from the pv classes.
 *
 * William Lupton, W. M. Keck Observatory
 */

#ifndef INCLpvh
#define INCLpvh

#include "shareLib.h" /* reset share lib defines */
#include "epicsThread.h"	/* for thread ids */
#include "epicsMutex.h"		/* for locks */
#include "epicsTime.h"		/* for time stamps */

#include "pvAlarm.h"		/* status and severity definitions */

/*
 * Standard FALSE and TRUE macros
 */
#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

/*
 * Magic number for validating structures / versions
 */
#define PV_MAGIC 0xfeddead	/* ...a sad tale of food poisoning? */

/*
 * Enum for data types (very restricted set of types)
 */
typedef enum {
    pvTypeERROR       = -1,
    pvTypeCHAR        = 0,
    pvTypeSHORT       = 1,
    pvTypeLONG        = 2,
    pvTypeFLOAT       = 3,
    pvTypeDOUBLE      = 4,
    pvTypeSTRING      = 5,
    pvTypeTIME_CHAR   = 6,
    pvTypeTIME_SHORT  = 7,
    pvTypeTIME_LONG   = 8,
    pvTypeTIME_FLOAT  = 9,
    pvTypeTIME_DOUBLE = 10,
    pvTypeTIME_STRING = 11
} pvType;

#define PV_SIMPLE(_type) ( (_type) <= pvTypeSTRING )

/*
 * Value-related types (c.f. db_access.h)
 */
typedef char   pvChar;
typedef short  pvShort;
typedef long   pvLong;
typedef float  pvFloat;
typedef double pvDouble;
typedef char   pvString[256]; /* use sizeof( pvString ) */

#define PV_TIME_XXX(_type) \
    typedef struct { \
	pvStat	  status; \
	pvSevr    severity; \
	epicsTimeStamp  stamp; \
	pv##_type value[1]; \
    } pvTime##_type

PV_TIME_XXX( Char   );
PV_TIME_XXX( Short  );
PV_TIME_XXX( Long   );
PV_TIME_XXX( Float  );
PV_TIME_XXX( Double );
PV_TIME_XXX( String );

typedef union {
    pvChar       charVal[1];
    pvShort      shortVal[1];
    pvLong       longVal[1];
    pvFloat      floatVal[1];
    pvDouble     doubleVal[1];
    pvString     stringVal[1];
    pvTimeChar   timeCharVal;
    pvTimeShort  timeShortVal;
    pvTimeLong   timeLongVal;
    pvTimeFloat  timeFloatVal;
    pvTimeDouble timeDoubleVal;
    pvTimeString timeStringVal;
} pvValue;

#define PV_VALPTR(_type,_value) \
    ( ( PV_SIMPLE(_type) ? ( void * ) ( _value ) : \
			   ( void * ) ( &_value->timeCharVal.value ) ) )

/*
 * Connect (connect/disconnect and event (get, put and monitor) functions
 */
typedef void (*pvConnFunc)( void *var, int connected );

typedef void (*pvEventFunc)( void *var, pvType type, int count,
			     pvValue *value, void *arg, pvStat status );

/*
 * Most of the rest is C++. The C interface is at the bottom.
 */
#ifdef __cplusplus

/*
 * Forward references
 */
class pvVariable;
class pvCallback;

////////////////////////////////////////////////////////////////////////////////
/*
 * System
 *
 * This is somewhat analogous to a cdevSystem object (CA has no equivalent)
 */

class pvSystem {

public:
    epicsShareFunc pvSystem( int debug = 0 );
    epicsShareFunc virtual ~pvSystem();

    epicsShareFunc inline pvSystem *getSystem() { return this; } 

    epicsShareFunc virtual pvStat attach() { return pvStatOK; }
    epicsShareFunc virtual pvStat flush() { return pvStatOK; }
    epicsShareFunc virtual pvStat pend( double seconds = 0.0, int wait = FALSE ) = 0;

    epicsShareFunc virtual pvVariable *newVariable( const char *name, pvConnFunc func = NULL,
				     void *priv = NULL, int debug = 0 ) = 0;

    epicsShareFunc void lock();
    epicsShareFunc void unlock();

    epicsShareFunc inline int getMagic() const { return magic_; }
    epicsShareFunc inline void setDebug( int debug ) { debug_ = debug; }
    epicsShareFunc inline int getDebug() const { return debug_; }

    epicsShareFunc void setError( int status, pvSevr sevr, pvStat stat, const char *mess );
    epicsShareFunc inline int getStatus() const { return status_; }
    epicsShareFunc inline pvSevr getSevr() const { return sevr_; }
    epicsShareFunc inline pvStat getStat() const { return stat_; }
    epicsShareFunc inline void setStatus( int status ) { status_ = status; }
    epicsShareFunc inline void setStat( pvStat stat ) { stat_ = stat; }
    epicsShareFunc inline char *getMess() const { return mess_?mess_:(char *)""; }

private:
    int		magic_;		/* magic number (used for authentication) */
    int		debug_;		/* debugging level (inherited by pvs) */

    int		status_;	/* message system-specific status code */
    pvSevr	sevr_;		/* severity */
    pvStat	stat_;		/* status */
    char	*mess_;		/* error message */

    epicsMutexId	lock_;		/* prevents more than one thread in library */
};

////////////////////////////////////////////////////////////////////////////////
/*
 * Process variable
 *
 * This is somewhat analogous to a cdevDevice object (or a CA channel)
 */
class pvVariable {

public:
    // private data is constructor argument so that it is guaranteed set
    // before connection callback is invoked
    epicsShareFunc pvVariable( pvSystem *system, const char *name, pvConnFunc func = NULL,
		void *priv = NULL, int debug = 0 );
    epicsShareFunc virtual ~pvVariable();

    epicsShareFunc virtual pvStat get( pvType type, int count, pvValue *value ) = 0;
    epicsShareFunc virtual pvStat getNoBlock( pvType type, int count, pvValue *value ) = 0;
    epicsShareFunc virtual pvStat getCallback( pvType type, int count,
		pvEventFunc func, void *arg = NULL ) = 0;
    epicsShareFunc virtual pvStat put( pvType type, int count, pvValue *value ) = 0;
    epicsShareFunc virtual pvStat putNoBlock( pvType type, int count, pvValue *value ) = 0;
    epicsShareFunc virtual pvStat putCallback( pvType type, int count, pvValue *value,
		pvEventFunc func, void *arg = NULL ) = 0;
    epicsShareFunc virtual pvStat monitorOn( pvType type, int count,
		pvEventFunc func, void *arg = NULL,
		pvCallback **pCallback = NULL ) = 0;
    epicsShareFunc virtual pvStat monitorOff( pvCallback *callback = NULL ) = 0;

    epicsShareFunc virtual int getConnected() const = 0;
    epicsShareFunc virtual pvType getType() const = 0;
    epicsShareFunc virtual int getCount() const = 0;

    epicsShareFunc inline int getMagic() const { return magic_; }
    epicsShareFunc inline void setDebug( int debug ) { debug_ = debug; }
    epicsShareFunc inline int getDebug() const { return debug_; }
    epicsShareFunc inline pvConnFunc getFunc() const { return func_; }

    epicsShareFunc inline pvSystem *getSystem() const { return system_; }
    epicsShareFunc inline char *getName() const { return name_; }
    epicsShareFunc inline void setPrivate( void *priv ) { private_ = priv; }
    epicsShareFunc inline void *getPrivate() const { return private_; }

    epicsShareFunc void setError( int status, pvSevr sevr, pvStat stat, const char *mess );
    epicsShareFunc inline int getStatus() const { return status_; }
    epicsShareFunc inline pvSevr getSevr() const { return sevr_; }
    epicsShareFunc inline pvStat getStat() const { return stat_; }
    epicsShareFunc inline void setStatus( int status ) { status_ = status; }
    epicsShareFunc inline void setStat( pvStat stat ) { stat_ = stat; }
    epicsShareFunc inline char *getMess() const { return mess_?mess_:(char *)""; }

private:
    int		magic_;		/* magic number (used for authentication) */
    int		debug_;		/* debugging level (inherited from system) */
    pvConnFunc	func_;		/* connection state change function */

    pvSystem	*system_;	/* associated system */
    char	*name_;		/* variable name */
    void	*private_;	/* client's private data */

    int		status_;	/* message system-specific status code */
    pvSevr	sevr_;		/* severity */
    pvStat	stat_;		/* status */
    char	*mess_;		/* error message */
};

////////////////////////////////////////////////////////////////////////////////
/*
 * Callback
 *
 * This is somewhat analogous to a cdevCallback object
 */
class pvCallback {

public:
    epicsShareFunc pvCallback( pvVariable *variable, pvType type, int count,
		pvEventFunc func, void *arg, int debug = 0);
    epicsShareFunc ~pvCallback();

    epicsShareFunc inline int getMagic() { return magic_; }
    epicsShareFunc inline void setDebug( int debug ) { debug_ = debug; }
    epicsShareFunc inline int getDebug() { return debug_; }

    epicsShareFunc inline pvVariable *getVariable() { return variable_; }
    epicsShareFunc inline pvType getType() { return type_; }
    epicsShareFunc inline int getCount() { return count_; };
    epicsShareFunc inline pvEventFunc getFunc() { return func_; };
    epicsShareFunc inline void *getArg() { return arg_; };
    epicsShareFunc inline void setPrivate( void *priv ) { private_ = priv; }
    epicsShareFunc inline void *getPrivate() { return private_; }

private:
    int		magic_;		/* magic number (used for authentication) */
    int		debug_;		/* debugging level (inherited from variable) */

    pvVariable  *variable_;	/* associated variable */
    pvType	type_;		/* variable's native type */
    int		count_;		/* variable's element count */
    pvEventFunc func_;		/* user's event function */
    void	*arg_;		/* user's event function argument */
    void	*private_;	/* message system's private data */
};

////////////////////////////////////////////////////////////////////////////////
/*
 * End of C++.
 */
#endif	/* __cplusplus */

/*
 * C interface
 */
#ifdef __cplusplus
extern "C" {
epicsShareFunc pvSystem * epicsShareAPI newPvSystem( const char *name, int debug = 0 );
#endif

epicsShareFunc pvStat epicsShareAPI pvSysCreate( const char *name, int debug, void **pSys );
epicsShareFunc pvStat epicsShareAPI pvSysDestroy( void *sys );
epicsShareFunc pvStat epicsShareAPI pvSysFlush( void *sys );
epicsShareFunc pvStat epicsShareAPI pvSysPend( void *sys, double seconds, int wait );
epicsShareFunc pvStat epicsShareAPI pvSysLock( void *sys );
epicsShareFunc pvStat epicsShareAPI pvSysUnlock( void *sys );
epicsShareFunc pvStat epicsShareAPI pvSysAttach( void *sys );
epicsShareFunc int    epicsShareAPI pvSysGetMagic( void *sys );
epicsShareFunc void   epicsShareAPI pvSysSetDebug( void *sys, int debug );
epicsShareFunc int    epicsShareAPI pvSysGetDebug( void *sys );
epicsShareFunc int    epicsShareAPI pvSysGetStatus( void *sys );
epicsShareFunc pvSevr epicsShareAPI pvSysGetSevr( void *sys );
epicsShareFunc pvStat epicsShareAPI pvSysGetStat( void *sys );
epicsShareFunc char * epicsShareAPI pvSysGetMess( void *sys );

epicsShareFunc pvStat epicsShareAPI pvVarCreate( void *sys, const char *name, pvConnFunc func, void *priv,
		    int debug, void **pVar );
epicsShareFunc pvStat epicsShareAPI pvVarDestroy( void *var );
epicsShareFunc pvStat epicsShareAPI pvVarGet( void *var, pvType type, int count, pvValue *value );
epicsShareFunc pvStat epicsShareAPI pvVarGetNoBlock( void *var, pvType type, int count, pvValue *value );
epicsShareFunc pvStat epicsShareAPI pvVarGetCallback( void *var, pvType type, int count,
		         pvEventFunc func, void *arg );
epicsShareFunc pvStat epicsShareAPI pvVarPut( void *var, pvType type, int count, pvValue *value );
epicsShareFunc pvStat epicsShareAPI pvVarPutNoBlock( void *var, pvType type, int count, pvValue *value );
epicsShareFunc pvStat epicsShareAPI pvVarPutCallback( void *var, pvType type, int count, pvValue *value,
		         pvEventFunc func, void *arg );
epicsShareFunc pvStat epicsShareAPI pvVarMonitorOn( void *var, pvType type, int count,
		       pvEventFunc func, void *arg, void **pId );
epicsShareFunc pvStat epicsShareAPI pvVarMonitorOff( void *var, void *id );
epicsShareFunc int    epicsShareAPI pvVarGetMagic( void *var );
epicsShareFunc void   epicsShareAPI pvVarSetDebug( void *var, int debug );
epicsShareFunc int    epicsShareAPI pvVarGetDebug( void *var );
epicsShareFunc int    epicsShareAPI pvVarGetConnected( void *var );
epicsShareFunc pvType epicsShareAPI pvVarGetType( void *var );
epicsShareFunc int    epicsShareAPI pvVarGetCount( void *var );
epicsShareFunc char * epicsShareAPI pvVarGetName( void *var );
epicsShareFunc void   epicsShareAPI pvVarSetPrivate( void *var, void *priv );
epicsShareFunc void * epicsShareAPI pvVarGetPrivate( void *var );
epicsShareFunc int    epicsShareAPI pvVarGetStatus( void *var );
epicsShareFunc pvSevr epicsShareAPI pvVarGetSevr( void *var );
epicsShareFunc pvStat epicsShareAPI pvVarGetStat( void *var );
epicsShareFunc char * epicsShareAPI pvVarGetMess( void *var );

/*
 * Time utilities
 */
epicsShareFunc int    epicsShareAPI pvTimeGetCurrentDouble( double *pTime );

/*
 * Misc utilities
 */
epicsShareFunc char * epicsShareAPI Strdup( const char *s );
epicsShareFunc char * epicsShareAPI Strdcpy( char *dst, const char *src );

#ifdef __cplusplus
}
#endif

#endif /* INCLpvh */

/*
 * pv.h,v
 * Revision 1.3  2001/02/16 18:45:39  mrk
 * changes for latest version of 3.14
 *
 * Revision 1.2  2000/04/14 21:53:28  jba
 * Changes for win32 build.
 *
 * Revision 1.1.1.1  2000/04/04 03:22:13  wlupton
 * first commit of seq-2-0-0
 *
 * Revision 1.18  2000/03/31 23:00:42  wlupton
 * added default attach and flush implementations; added setStatus
 *
 * Revision 1.17  2000/03/29 01:58:48  wlupton
 * split off pvAlarm.h; added pvVarGetName
 *
 * Revision 1.16  2000/03/18 04:00:25  wlupton
 * converted to use new configure scheme
 *
 * Revision 1.15  2000/03/16 02:10:24  wlupton
 * added newly-needed debug argument
 *
 * Revision 1.14  2000/03/07 09:27:39  wlupton
 * drastically reduced use of references
 *
 * Revision 1.13  2000/03/07 08:46:29  wlupton
 * created ktlKeyword class (works but a bit messy)
 *
 * Revision 1.12  2000/03/06 19:19:43  wlupton
 * misc type conversion and error reporting mods
 *
 * Revision 1.11  2000/03/01 02:07:14  wlupton
 * converted to use new OSI library
 *
 * Revision 1.10  2000/02/19 01:09:51  wlupton
 * added PV_SIMPLE() and prototypes for var-level error info
 *
 * Revision 1.9  2000/02/16 02:31:44  wlupton
 * merged in v1.9.5 changes
 *
 * Revision 1.8  1999/09/07 20:42:59  epics
 * removed unnecessary comment
 *
 * Revision 1.7  1999/07/07 18:50:33  wlupton
 * supported full mapping from EPICS status and severity to pvStat and pvSevr
 *
 * Revision 1.6  1999/07/01 20:50:18  wlupton
 * Working under VxWorks
 *
 * Revision 1.5  1999/06/10 00:35:03  wlupton
 * demo sequencer working with pvCa
 *
 * Revision 1.4  1999/06/08 19:21:43  wlupton
 * CA version working; about to use in sequencer
 *
 * Revision 1.3  1999/06/08 03:25:21  wlupton
 * nearly complete CA implementation
 *
 * Revision 1.2  1999/06/07 21:46:44  wlupton
 * working with simple pvtest program
 *
 * Revision 1.1  1999/06/04 20:48:27  wlupton
 * initial version of pv.h and pv.cc
 *
 */
