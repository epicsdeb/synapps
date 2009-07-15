/* pvCa.cc,v 1.7 2001/10/04 18:33:25 jhill Exp
 *
 * Implementation of EPICS sequencer CA library (pvCa)
 *
 * William Lupton, W. M. Keck Observatory
 */

#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "cadef.h"

#define epicsExportSharedSymbols
#include "pvCa.h"

/* handlers */
extern "C" {
void pvCaConnectionHandler( struct connection_handler_args args );
void pvCaAccessHandler( struct event_handler_args args );
void pvCaMonitorHandler( struct event_handler_args args );
}

/* utilities */
static pvSevr sevrFromCA( int status );	/* CA severity as pvSevr */
static pvSevr sevrFromEPICS( int sevr );
					/* EPICS severity as pvSevr */
static pvStat statFromCA( int status );	/* CA status as pvStat */
static pvStat statFromEPICS( int stat );
					/* EPICS status as pvStat */
static pvType typeFromCA( int type );	/* DBR type as pvType */
static int typeToCA( pvType type );	/* pvType as DBR type */
static void copyToCA( pvType type, int count,
		      const pvValue *value, union db_access_val *caValue );
					/* copy pvValue to DBR value */
static void copyFromCA( int type, int count,
			const union db_access_val *caValue, pvValue *value );
					/* copy DBR value to pvValue */

/* invoke CA function and send error details to system or variable object */
#define INVOKE(_function) \
    do { \
	int _status = _function; \
	if ( _status & CA_M_SUCCESS ) \
	    setStat( pvStatOK ); \
	else \
	    setError( _status, sevrFromCA( _status ), \
		      statFromCA( _status ), ca_message( _status ) ); \
    } while ( FALSE )

////////////////////////////////////////////////////////////////////////////////
/*+
 * Routine:	caSystem::caSystem
 *
 * Purpose:	caSystem constructor
 *
 * Description:	
 */
epicsShareFunc caSystem::caSystem( int debug ) :
    pvSystem( debug ),
    context_( NULL )
{
    if ( getDebug() > 0 )
	printf( "%8p: caSystem::caSystem( %d )\n", this, debug );

    INVOKE( ca_context_create(ca_enable_preemptive_callback) );
    this->context_ = ca_current_context ();
}

/*+
 * Routine:	caSystem::~caSystem
 *
 * Purpose:	caSystem destructor
 *
 * Description:	
 */
epicsShareFunc caSystem::~caSystem()
{
    if ( getDebug() > 0 )
	printf( "%8p: caSystem::~caSystem()\n", this );

    INVOKE( ca_task_exit() );
}

/*+
 * Routine:     caSystem::attach
 *
 * Purpose:     caSystem attach to context of creator of caSystem
 *
 * Description:
 */
epicsShareFunc pvStat caSystem::attach()
{
    if ( getDebug() > 0 )
        printf( "%8p: caSystem::attach()\n", this );

    INVOKE( ca_attach_context( context_ ) );
    return getStat();
}

/*+
 * Routine:     caSystem::flush
 *
 * Purpose:     caSystem flush routine
 *
 * Description:
 */
epicsShareFunc pvStat caSystem::flush()
{
    if ( getDebug() > 0 )
        printf( "%8p: caSystem::flush()\n", this );

    INVOKE( ca_flush_io() );
    return getStat();
}

/*+
 * Routine:     caSystem::pend
 *
 * Purpose:     caSystem pend routine
 *
 * Description:
 */
epicsShareFunc pvStat caSystem::pend( double seconds, int wait )
{
    if ( getDebug() > 1 )
        printf( "%8p: caSystem::pend( %g, %d )\n", this, seconds, wait );

    if ( seconds <= 0.0 || !wait ) seconds = 1e-8;
    INVOKE( ca_pend_event( seconds ) );
    return getStat();
}

/*+
 * Routine:     caSystem::newVariable
 *
 * Purpose:     caSystem variable creation routine
 *
 * Description:
 */
epicsShareFunc pvVariable *caSystem::newVariable( const char *name, pvConnFunc func, void *priv,
				   int debug )
{
    if ( getDebug() > 0 )
        printf( "%8p: caSystem::newVariable( %s, %p, %p, %d )\n",
		this, name, func, priv, debug );

    return new caVariable( this, name, func, priv, debug );
}

////////////////////////////////////////////////////////////////////////////////
/*+
 * Routine:     caVariable::caVariable
 *
 * Purpose:     caVariable constructor
 *
 * Description:
 */
epicsShareFunc caVariable::caVariable( caSystem *system, const char *name, pvConnFunc func,
		        void *priv, int debug ) :
    pvVariable( system, name, func, priv, debug ),
    chid_( NULL )
{
    if ( getDebug() > 0 )
	printf( "%8p: caVariable::caVariable( %s, %d )\n",
		this, name, debug );

    if ( getFunc() != NULL )
	INVOKE( ca_search_and_connect( name, &chid_, pvCaConnectionHandler, this ));
    else
	INVOKE( ca_search_and_connect( name, &chid_, NULL, NULL ));
}

/*+
 * Routine:     caVariable::~caVariable
 *
 * Purpose:     caVariable destructor
 *
 * Description:
 */
epicsShareFunc caVariable::~caVariable()
{
    if ( getDebug() > 0 )
        printf( "%8p: caVariable::~caVariable()\n", this );

    INVOKE( ca_clear_channel( chid_ ) );
}

/*+
 * Routine:     caVariable::get
 *
 * Purpose:     caVariable blocking get routine
 *
 * Description:
 */
epicsShareFunc pvStat caVariable::get( pvType type, int count, pvValue *value )
{
    if ( getDebug() > 0 )
        printf( "%8p: caVariable::get( %d, %d )\n", this, type, count );

    int caType = typeToCA( type );
    char *caValue = new char[dbr_size_n( caType, count )];
    INVOKE( ca_array_get( caType, count, chid_, caValue ) );
    // ### must block so can convert value; should use ca_get_callback()
    if ( getStat() == pvStatOK )
	INVOKE( ca_pend_io( 5.0 ) );
    if ( getStat() == pvStatOK )
	copyFromCA( caType, count, ( union db_access_val * ) caValue, value );
    delete [] caValue;
	
    return getStat();
}

/*+
 * Routine:     caVariable::getNoBlock
 *
 * Purpose:     caVariable non-blocking get routine
 *
 * Description:
 */
epicsShareFunc pvStat caVariable::getNoBlock( pvType type, int count, pvValue *value )
{
    if ( getDebug() > 0 )
        printf( "%8p: caVariable::getNoBlock( %d, %d )\n", this,
		type, count );

    // ### must block so can convert value; should use ca_get_callback()
    return get( type, count, value );
}


/*+
 * Routine:     caVariable::getCallback
 *
 * Purpose:     caVariable get with callback routine
 *
 * Description:
 */
epicsShareFunc pvStat caVariable::getCallback( pvType type, int count,
			        pvEventFunc func, void *arg )
{
    if ( getDebug() > 0 )
        printf( "%8p: caVariable::getCallback( %d, %d )\n",
		this, type, count );

    pvCallback *callback = new pvCallback( this, type, count, func, arg,
					   getDebug() );

    INVOKE( ca_array_get_callback( typeToCA( type ), count, chid_,
				   pvCaAccessHandler, callback ) );
    return getStat();
}

/*+
 * Routine:     caVariable::put
 *
 * Purpose:     caVariable blocking put routine
 *
 * Description:
 */
epicsShareFunc pvStat caVariable::put( pvType type, int count, pvValue *value )
{
    if ( getDebug() > 0 )
        printf( "%8p: caVariable::put( %d, %d )\n", this, type, count );

    int caType = typeToCA( type );
    char *caValue = new char[dbr_size_n( caType, count )];
    copyToCA( type, count, value, ( union db_access_val * ) caValue );
    INVOKE( ca_array_put( caType, count, chid_, caValue ) );
    if ( getStat() == pvStatOK )
	INVOKE( ca_pend_io( 5.0 ) );
    delete [] caValue;
    return getStat();
}

/*+
 * Routine:     caVariable::putNoBlock
 *
 * Purpose:     caVariable non-blocking put routine
 *
 * Description:
 */
epicsShareFunc pvStat caVariable::putNoBlock( pvType type, int count, pvValue *value )
{
    if ( getDebug() > 0 )
        printf( "%8p: caVariable::putNoBlock( %d, %d )\n", this,
		type, count );

    int caType = typeToCA( type );
    char *caValue = new char[dbr_size_n( caType, count )];
    copyToCA( type, count, value, ( union db_access_val * ) caValue );
    INVOKE( ca_array_put( caType, count, chid_, caValue ) );
    delete [] caValue;
    return getStat();
}

/*+
 * Routine:     caVariable::putCallback
 *
 * Purpose:     caVariable put with callback routine
 *
 * Description:
 */
epicsShareFunc pvStat caVariable::putCallback( pvType type, int count, pvValue *value,
			        pvEventFunc func, void *arg )
{
    if ( getDebug() > 0 )
        printf( "%8p: caVariable::putCallback( %d, %d )\n",
		this, type, count );

    pvCallback *callback = new pvCallback( this, type, count, func, arg,
					   getDebug() );

    INVOKE( ca_array_put_callback( typeToCA( type ), count, chid_, value,
				   pvCaAccessHandler, callback ) );
    return getStat();
}

/*+
 * Routine:     caVariable::monitorOn
 *
 * Purpose:     caVariable monitor enable routine
 *
 * Description:
 */
epicsShareFunc pvStat caVariable::monitorOn( pvType type, int count, pvEventFunc func,
			      void *arg, pvCallback **pCallback )
{
    if ( getDebug() > 0 )
        printf( "%8p: caVariable::monitorOn( %d, %d )\n",
		this, type, count );

    pvCallback *callback = new pvCallback( this, type, count, func, arg,
					   getDebug() );

    evid id = NULL;
    INVOKE( ca_add_masked_array_event( typeToCA( type ), count, chid_,
			pvCaMonitorHandler, callback, 0.0, 0.0, 0.0,
			&id, DBE_VALUE|DBE_ALARM  ) );
    callback->setPrivate( id );

    if ( pCallback != NULL )
	*pCallback = callback;
    return getStat();
}

/*+
 * Routine:     caVariable::monitorOff
 *
 * Purpose:     caVariable monitor disable routine
 *
 * Description:
 */
epicsShareFunc pvStat caVariable::monitorOff( pvCallback *callback )
{
    if ( getDebug() > 0 )
        printf( "%8p: caVariable::monitorOff()\n", this );

    if ( callback != NULL ) {
	evid id = ( evid ) callback->getPrivate();
	INVOKE( ca_clear_event( id ) );
	delete callback;
	return getStat();
    } else {
        return pvStatOK;
    }
}

/*+
 * Routine:     caVariable::getConnected
 *
 * Purpose:     caVariable "are we connected?" routine
 *
 * Description:
 */
epicsShareFunc int caVariable::getConnected() const
{
    if ( getDebug() > 1 )
        printf( "%8p: caVariable::getConnected()\n", this );

    return ( ca_state( chid_ ) == cs_conn );
}

/*+
 * Routine:     caVariable::getType
 *
 * Purpose:     caVariable "what type are we?" routine
 *
 * Description:
 */
epicsShareFunc pvType caVariable::getType() const
{
    if ( getDebug() > 1 )
        printf( "%8p: caVariable::getType()\n", this );

    return typeFromCA( ca_field_type( chid_ ) );
}

/*+
 * Routine:     caVariable::getCount
 *
 * Purpose:     caVariable "what count do we have?" routine
 *
 * Description:
 */
epicsShareFunc int caVariable::getCount() const
{
    if ( getDebug() > 1 )
        printf( "%8p: caVariable::getCount()\n", this );

    return ca_element_count( chid_ );
}

////////////////////////////////////////////////////////////////////////////////
/*+
 * Routine:     pvCaConnectionHandler
 *
 * Purpose:     CA connection handler
 *
 * Description:
 */
void pvCaConnectionHandler( struct connection_handler_args args )
{
    pvVariable *variable = ( pvVariable * ) ca_puser( args.chid );
    pvConnFunc func = variable->getFunc();

    ( *func ) ( ( void * ) variable, ( args.op == CA_OP_CONN_UP ) );
}

/*+
 * Routine:     pvCaAccessHandler
 *
 * Purpose:     CA get/put callback event handler
 *
 * Description:
 */
void pvCaAccessHandler( struct event_handler_args args )
{
    pvCaMonitorHandler( args );

    pvCallback *callback = ( pvCallback * ) args.usr;
    delete callback;
}

/*+
 * Routine:     pvCaMonitorHandler
 *
 * Purpose:     CA monitor event handler
 *
 * Description:
 */
void pvCaMonitorHandler( struct event_handler_args args )
{
    pvCallback *callback = ( pvCallback * ) args.usr;
    pvEventFunc func = callback->getFunc();
    pvVariable *variable = callback->getVariable();
    pvType type = callback->getType();
    int count = callback->getCount();
    void *arg = callback->getArg();

    // put completion messages pass a NULL value
    if ( args.dbr == NULL ) {
	( *func ) ( ( void * ) variable, type, count, NULL, arg,
		    statFromCA( args.status ) );
    } else {
	pvValue *value = new pvValue[count]; // ### larger than needed
	copyFromCA( args.type, args.count, ( union db_access_val * ) args.dbr,
		    value );
	// ### should assert args.type is equiv to type and args.count is count
	( *func ) ( ( void * ) variable, type, count, value, arg,
		    statFromCA( args.status ) );
	delete [] value;
    }
}

////////////////////////////////////////////////////////////////////////////////
/*+
 * Routine:     sevrFromCA
 *
 * Purpose:     extract pvSevr from CA status
 *
 * Description:
 */
static pvSevr sevrFromCA( int status )
{
    switch ( CA_EXTRACT_SEVERITY( status ) ) {
      case CA_K_INFO:    return pvSevrNONE;
      case CA_K_SUCCESS: return pvSevrNONE;
      case CA_K_WARNING: return pvSevrMINOR;
      case CA_K_ERROR:   return pvSevrMAJOR;
      case CA_K_SEVERE:  return pvSevrINVALID;
      default:           return pvSevrERROR;
    }
}

/*+
 * Routine:     sevrFromEPICS
 *
 * Purpose:     extract pvSevr from EPICS severity
 *
 * Description:
 */
static pvSevr sevrFromEPICS( int sevr )
{
    switch ( sevr ) {
      case NO_ALARM:      return pvSevrNONE;
      case MINOR_ALARM:   return pvSevrMINOR;
      case MAJOR_ALARM:   return pvSevrMAJOR;
      case INVALID_ALARM: return pvSevrINVALID;
      default:            return pvSevrERROR;
    }
}

/*+
 * Routine:     statFromCA
 *
 * Purpose:     extract pvStat from CA status
 *
 * Description:
 */
static pvStat statFromCA( int status )
{
    pvSevr sevr = sevrFromCA( status );
    return ( sevr == pvSevrNONE || sevr == pvSevrMINOR ) ?
		pvStatOK : pvStatERROR;
}

/*+
 * Routine:     statFromEPICS
 *
 * Purpose:     extract pvStat from EPICS status
 *
 * Description:
 */
static pvStat statFromEPICS( int stat )
{
    switch ( stat ) {
      case NO_ALARM:           return pvStatOK;
      case READ_ALARM:         return pvStatREAD;
      case WRITE_ALARM:        return pvStatWRITE;
      case HIHI_ALARM:         return pvStatHIHI;
      case HIGH_ALARM:         return pvStatHIGH;
      case LOLO_ALARM:         return pvStatLOLO;
      case LOW_ALARM:          return pvStatLOW;
      case STATE_ALARM:        return pvStatSTATE;
      case COS_ALARM:          return pvStatCOS;
      case COMM_ALARM:         return pvStatCOMM;
      case TIMEOUT_ALARM:      return pvStatTIMEOUT;
      case HW_LIMIT_ALARM:     return pvStatHW_LIMIT;
      case CALC_ALARM:         return pvStatCALC;
      case SCAN_ALARM:         return pvStatSCAN;
      case LINK_ALARM:         return pvStatLINK;
      case SOFT_ALARM:         return pvStatSOFT;
      case BAD_SUB_ALARM:      return pvStatBAD_SUB;
      case UDF_ALARM:          return pvStatUDF;
      case DISABLE_ALARM:      return pvStatDISABLE;
      case SIMM_ALARM:         return pvStatSIMM;
      case READ_ACCESS_ALARM:  return pvStatREAD_ACCESS;
      case WRITE_ACCESS_ALARM: return pvStatWRITE_ACCESS;
      default:                 return pvStatERROR;
    }
}

/*+
 * Routine:     typeFromCA
 *
 * Purpose:     extract pvType from DBR type
 *
 * Description:
 */
static pvType typeFromCA( int type )
{
    switch ( type ) {
      case DBR_CHAR:        return pvTypeCHAR;
      case DBR_SHORT:       return pvTypeSHORT;
      case DBR_ENUM:        return pvTypeSHORT;
      case DBR_LONG:        return pvTypeLONG;
      case DBR_FLOAT:       return pvTypeFLOAT;
      case DBR_DOUBLE:      return pvTypeDOUBLE;
      case DBR_STRING:      return pvTypeSTRING;
      case DBR_TIME_CHAR:   return pvTypeTIME_CHAR;
      case DBR_TIME_SHORT:  return pvTypeTIME_SHORT;
      case DBR_TIME_ENUM:   return pvTypeTIME_SHORT;
      case DBR_TIME_LONG:   return pvTypeTIME_LONG;
      case DBR_TIME_FLOAT:  return pvTypeTIME_FLOAT;
      case DBR_TIME_DOUBLE: return pvTypeTIME_DOUBLE;
      case DBR_TIME_STRING: return pvTypeTIME_STRING;
      default:              return pvTypeERROR;
    }
}

/*+
 * Routine:     typeToCA
 *
 * Purpose:     extract DBR type from pvType
 *
 * Description:
 */
static int typeToCA( pvType type )
{
    switch ( type ) {
      case pvTypeCHAR:        return DBR_CHAR;
      case pvTypeSHORT:       return DBR_SHORT;
      case pvTypeLONG:        return DBR_LONG;
      case pvTypeFLOAT:       return DBR_FLOAT;
      case pvTypeDOUBLE:      return DBR_DOUBLE;
      case pvTypeSTRING:      return DBR_STRING;
      case pvTypeTIME_CHAR:   return DBR_TIME_CHAR;
      case pvTypeTIME_SHORT:  return DBR_TIME_SHORT;
      case pvTypeTIME_LONG:   return DBR_TIME_LONG;
      case pvTypeTIME_FLOAT:  return DBR_TIME_FLOAT;
      case pvTypeTIME_DOUBLE: return DBR_TIME_DOUBLE;
      case pvTypeTIME_STRING: return DBR_TIME_STRING;
      default:	              return -1;
    }
}

/*+
 * Routine:     copyToCA
 *
 * Purpose:     copy pvValue to DBR value
 *
 * Description:
 */
static void copyToCA( pvType type, int count,
		      const pvValue *value, union db_access_val *caValue )
{
    // ### inefficient to do all this here
    dbr_char_t  *charval  = (dbr_char_t   *) dbr_value_ptr(caValue, DBR_CHAR  );
    dbr_short_t *shrtval  = (dbr_short_t  *) dbr_value_ptr(caValue, DBR_SHORT );
    dbr_long_t  *longval  = (dbr_long_t   *) dbr_value_ptr(caValue, DBR_LONG  );
    dbr_float_t *fltval   = (dbr_float_t  *) dbr_value_ptr(caValue, DBR_FLOAT );
    dbr_double_t*doubleval= (dbr_double_t *) dbr_value_ptr(caValue, DBR_DOUBLE);
    dbr_string_t*strval   = (dbr_string_t *) dbr_value_ptr(caValue, DBR_STRING);

    int s = sizeof( dbr_string_t );
    int i;

    switch ( type ) {
      case pvTypeCHAR:
	for ( i = 0; i < count; i++ )
	    charval[i] = value->charVal[i];
	break;
      case pvTypeSHORT:
	for ( i = 0; i < count; i++ )
	    shrtval[i] = value->shortVal[i];
	break;
      case pvTypeLONG:
	for ( i = 0; i < count; i++ )
	    longval[i] = value->longVal[i];
	break;
      case pvTypeFLOAT:
	for ( i = 0; i < count; i++ )
	    fltval[i] = value->floatVal[i];
	break;
      case pvTypeDOUBLE:
	for ( i = 0; i < count; i++ )
	    doubleval[i] = value->doubleVal[i];
	break;
      case pvTypeSTRING:
	for ( i = 0; i < count; i++ ) {
	    strncpy( strval[i], value->stringVal[i], s );
	    strval[i][s-1] = '\0';
	}
	break;
      default:
	// ### no check for invalid types
	// ### assume that no TIME_XXX types are written to CA
	break;
    }
}

/*+
 * Routine:     copyFromCA
 *
 * Purpose:     copy DBR value to pvValue
 *
 * Description:
 */
static void copyFromCA( int type, int count,
			const union db_access_val *caValue, pvValue *value )
{
    // ### inefficient to do all this here
    dbr_char_t  *charval  = (dbr_char_t   *) dbr_value_ptr(caValue, DBR_CHAR  );
    dbr_short_t *shrtval  = (dbr_short_t  *) dbr_value_ptr(caValue, DBR_SHORT );
    dbr_long_t  *longval  = (dbr_long_t   *) dbr_value_ptr(caValue, DBR_LONG  );
    dbr_float_t *fltval   = (dbr_float_t  *) dbr_value_ptr(caValue, DBR_FLOAT );
    dbr_double_t*doubleval= (dbr_double_t *) dbr_value_ptr(caValue, DBR_DOUBLE);
    dbr_string_t*strval   = (dbr_string_t *) dbr_value_ptr(caValue, DBR_STRING);

    dbr_char_t  *tchrval  = (dbr_char_t   *) dbr_value_ptr(caValue, DBR_TIME_CHAR  );
    dbr_short_t *tshrtval = (dbr_short_t  *) dbr_value_ptr(caValue, DBR_TIME_SHORT );
    dbr_long_t  *tlngval  = (dbr_long_t   *) dbr_value_ptr(caValue, DBR_TIME_LONG  );
    dbr_float_t *tfltval  = (dbr_float_t  *) dbr_value_ptr(caValue, DBR_TIME_FLOAT );
    dbr_double_t*tdblval  = (dbr_double_t *) dbr_value_ptr(caValue, DBR_TIME_DOUBLE);
    dbr_string_t*tstrval  = (dbr_string_t *) dbr_value_ptr(caValue, DBR_TIME_STRING);

    int s = sizeof( pvString );
    int i;

    switch ( type ) {
      case DBR_CHAR:
	for ( i = 0; i < count; i++ )
	    value->charVal[i] = charval[i];
	break;
      case DBR_SHORT:
      case DBR_ENUM:
	for ( i = 0; i < count; i++ )
	    value->shortVal[i] = shrtval[i];
	break;
      case DBR_LONG:
	for ( i = 0; i < count; i++ )
	    value->longVal[i] = longval[i];
	break;
      case DBR_FLOAT:
	for ( i = 0; i < count; i++ )
	    value->floatVal[i] = fltval[i];
	break;
      case DBR_DOUBLE:
	for ( i = 0; i < count; i++ )
	    value->doubleVal[i] = doubleval[i];
	break;
      case DBR_STRING:
	for ( i = 0; i < count; i++ ) {
	    strncpy( value->stringVal[i], strval[i], s );
	    value->stringVal[i][s-1] = '\0';
	}
	break;
      case DBR_TIME_CHAR:
	value->timeCharVal.status = statFromEPICS( caValue->tchrval.status );
	value->timeCharVal.severity = sevrFromEPICS( caValue->tchrval.severity);
	value->timeCharVal.stamp = caValue->tchrval.stamp;
	for ( i = 0; i < count; i++ )
	    value->timeCharVal.value[i] = tchrval[i];
	break;
      case DBR_TIME_SHORT:
      case DBR_TIME_ENUM:
	value->timeShortVal.status = statFromEPICS( caValue->tshrtval.status );
	value->timeShortVal.severity =sevrFromEPICS(caValue->tshrtval.severity);
	value->timeShortVal.stamp = caValue->tshrtval.stamp;
	for ( i = 0; i < count; i++ )
	    value->timeShortVal.value[i] = tshrtval[i];
	break;
      case DBR_TIME_LONG:
	value->timeLongVal.status = statFromEPICS( caValue->tlngval.status );
	value->timeLongVal.severity = sevrFromEPICS( caValue->tlngval.severity);
	value->timeLongVal.stamp = caValue->tlngval.stamp;
	for ( i = 0; i < count; i++ )
	    value->timeLongVal.value[i] = tlngval[i];
	break;
      case DBR_TIME_FLOAT:
	value->timeFloatVal.status = statFromEPICS( caValue->tfltval.status );
	value->timeFloatVal.severity = sevrFromEPICS(caValue->tfltval.severity);
	value->timeFloatVal.stamp = caValue->tfltval.stamp;
	for ( i = 0; i < count; i++ )
	    value->timeFloatVal.value[i] = tfltval[i];
	break;
      case DBR_TIME_DOUBLE:
	value->timeDoubleVal.status = statFromEPICS( caValue->tdblval.status );
	value->timeDoubleVal.severity =sevrFromEPICS(caValue->tdblval.severity);
	value->timeDoubleVal.stamp = caValue->tdblval.stamp;
	for ( i = 0; i < count; i++ )
	    value->timeDoubleVal.value[i] = tdblval[i];
	break;
      case DBR_TIME_STRING:
	value->timeStringVal.status = statFromEPICS( caValue->tstrval.status );
	value->timeStringVal.severity =sevrFromEPICS(caValue->tstrval.severity);
	value->timeStringVal.stamp = caValue->tstrval.stamp;
	for ( i = 0; i < count; i++ ) {
	    strncpy( value->timeStringVal.value[i], tstrval[i], s );
	    value->timeStringVal.value[i][s-1] = '\0';
	}
	break;
      default:
	// ### no check for invalid types
	break;
    }
}

/*
 * pvCa.cc,v
 * Revision 1.7  2001/10/04 18:33:25  jhill
 * context_ variable wasnt initialized
 *
 * Revision 1.6  2001/07/05 14:42:15  mrk
 * ca changed client contect
 *
 * Revision 1.5  2001/03/21 19:42:45  mrk
 * handlers must be C callable and external to satisfy all compilers
 *
 * Revision 1.4  2001/03/21 15:03:35  mrk
 * declare extern "C"
 *
 * Revision 1.3  2001/03/09 21:11:51  mrk
 * ca_pend no longer exists
 *
 * Revision 1.2  2000/04/14 21:53:28  jba
 * Changes for win32 build.
 *
 * Revision 1.1.1.1  2000/04/04 03:22:14  wlupton
 * first commit of seq-2-0-0
 *
 * Revision 1.17  2000/03/29 01:59:38  wlupton
 * accounted for possibility of NULL args.dbr in callback
 *
 * Revision 1.16  2000/03/18 04:00:25  wlupton
 * converted to use new configure scheme
 *
 * Revision 1.15  2000/03/16 02:11:28  wlupton
 * supported KTL_ANYPOLY (plus misc other mods)
 *
 * Revision 1.14  2000/03/07 19:55:32  wlupton
 * nearly sufficient tidying up
 *
 * Revision 1.13  2000/03/07 09:27:39  wlupton
 * drastically reduced use of references
 *
 * Revision 1.12  2000/03/07 08:46:29  wlupton
 * created ktlKeyword class (works but a bit messy)
 *
 * Revision 1.11  2000/03/06 19:20:22  wlupton
 * only set error on failure; tidy error reporting
 *
 * Revision 1.10  2000/03/01 02:07:14  wlupton
 * converted to use new OSI library
 *
 * Revision 1.9  2000/02/19 01:11:33  wlupton
 * changed INVOKE to operate on current object (sys or var)
 *
 * Revision 1.8  2000/02/16 02:31:44  wlupton
 * merged in v1.9.5 changes
 *
 * Revision 1.7  1999/07/07 18:50:33  wlupton
 * supported full mapping from EPICS status and severity to pvStat and pvSevr
 *
 * Revision 1.6  1999/07/01 20:50:19  wlupton
 * Working under VxWorks
 *
 * Revision 1.5  1999/06/15 10:11:03  wlupton
 * demo sequence mostly working with KTL
 *
 * Revision 1.4  1999/06/10 00:35:04  wlupton
 * demo sequencer working with pvCa
 *
 * Revision 1.3  1999/06/08 19:21:44  wlupton
 * CA version working; about to use in sequencer
 *
 * Revision 1.2  1999/06/08 03:25:21  wlupton
 * nearly complete CA implementation
 *
 * Revision 1.1  1999/06/07 21:46:45  wlupton
 * working with simple pvtest program
 *
 */
