/*************************************************************************\
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
/* Definitions for EPICS sequencer message system-independent types.
 *
 * William Lupton, W. M. Keck Observatory
 * Ben Franksen, HZB
 */

#ifndef INCLpvTypeh
#define INCLpvTypeh

#include "epicsTime.h"		/* for time stamps */

#include "pvAlarm.h"		/* status and severity definitions */

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

#define PV_SIMPLE(type) ((type)<=pvTypeSTRING)

#define pv_type_is_valid(type) ((type)>=0&&(type)<=pvTypeTIME_STRING)
#define pv_type_is_plain(type) ((type)>=0&&(type)<=pvTypeSTRING)

/*
 * Value-related types (c.f. db_access.h)
 */
typedef epicsInt8    pvChar;
typedef epicsInt16   pvShort;
typedef epicsInt32   pvLong;
typedef epicsFloat32 pvFloat;
typedef epicsFloat64 pvDouble;
typedef char         pvString[40]; /* use sizeof( pvString ) */

#define PV_TIME_XXX(type) \
    typedef struct { \
	pvStat	  status; \
	pvSevr    severity; \
	epicsTimeStamp  stamp; \
	pv##type value[1]; \
    } pvTime##type

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

/* deprecated, use pv_value_ptr */
#define PV_VALPTR(type,value)\
    ((PV_SIMPLE(type)?(void*)(value):\
                      (void*)(&value->timeCharVal.value)))

#define pv_is_simple_type(type)\
    ((type)>=pvTypeCHAR&&(type)<=pvTypeSTRING)
#define pv_is_time_type(type)\
    ((type)>=pvTypeTIME_CHAR&&(type)<=pvTypeTIME_STRING)
#define pv_is_valid_type(type)\
    ((type)>=pvTypeCHAR&&(type)<=pvTypeTIME_STRING)

#define pv_status_ptr(pv,type)\
    (assert(pv_is_time_type(type)),\
    (pvStat *)(((char *)pv)+pv_status_offsets[(type)-pvTypeTIME_CHAR]))
#define pv_severity_ptr(pv,type)\
    (assert(pv_is_time_type(type)),\
    (pvSevr *)(((char *)pv)+pv_severity_offsets[(type)-pvTypeTIME_CHAR]))
#define pv_stamp_ptr(pv,type)\
    (assert(pv_is_time_type(type)),\
    (epicsTimeStamp *)(((char *)pv)+pv_stamp_offsets[(type)-pvTypeTIME_CHAR]))

#define pv_value_ptr(pv,type)\
    (assert(pv_is_valid_type(type)),(void *)(((char *)pv)+pv_value_offsets[type]))
#define pv_size(type)\
    (assert(pv_is_valid_type(type)),pv_sizes[type])
#define pv_size_n(type,count)\
    (assert(pv_is_valid_type(type)),\
    (count)<=0?pv_sizes[type]:pv_sizes[type]+((count)-1)*pv_value_sizes[type])

#ifdef __cplusplus
extern "C" {
#endif

epicsShareExtern const size_t pv_sizes[];
epicsShareExtern const size_t pv_value_sizes[];
epicsShareExtern const size_t pv_value_offsets[];
epicsShareExtern const size_t pv_status_offsets[];
epicsShareExtern const size_t pv_severity_offsets[];
epicsShareExtern const size_t pv_stamp_offsets[];

#ifdef __cplusplus
}
#endif

#endif /* INCLpvTypeh */
