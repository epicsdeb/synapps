/* pvKtlCnv.cc,v 1.1.1.1 2000/04/04 03:22:15 wlupton Exp
 *
 * Implementation of EPICS sequencer KTL type conversion (pvKtlCnv)
 *
 * Uses large amounts of code from the cdevData class
 *
 * William Lupton, W. M. Keck Observatory
 */

#include <assert.h>

#include "pvKtlCnv.h"

/*
 * typedef for all conversion functions (pointers to the functions are
 * in the conversion matrix - see below; the functions should be
 * only via the "get" methods)
 */
typedef int ( *converter ) ( int inpCount, const void *inpValue,
			     int outCount, void *outValue );

/*
 * conversion matrix (elements are filled in at the bottom of the file, after
 * all the conversion functions have been defined)
 */
extern converter ktlCnvMatrix[ktlCnv::NUMBER][ktlCnv::NUMBER];

/*
 * map pvType and KTL_DATATYPE types to "type" enumeration for use in indexing
 * the conversion matrix
 */
ktlCnv::type ktlCnv::getType( pvType t )
{
    switch ( t ) {
	case pvTypeCHAR:        return CHAR;
	case pvTypeSHORT:       return SHORT;
	case pvTypeLONG:        return LONG;
	case pvTypeFLOAT:       return FLOAT;
	case pvTypeDOUBLE:      return DOUBLE;
	case pvTypeSTRING:      return STRING;
	case pvTypeTIME_CHAR:   return CHAR;
	case pvTypeTIME_SHORT:  return SHORT;
	case pvTypeTIME_LONG:   return LONG;
	case pvTypeTIME_FLOAT:  return FLOAT;
	case pvTypeTIME_DOUBLE: return DOUBLE;
	case pvTypeTIME_STRING: return STRING;
	default:		return INVALID;
    }
}

ktlCnv::type ktlCnv::getType( KTL_DATATYPE t )
{
    // KTL doesn't support a "long" type; handle ints and longs the same, but
    // check that they are indeed the same size
    assert( sizeof( int ) == sizeof( long ) );

    switch ( t ) {
	case KTL_INT:          return LONG;
	case KTL_BOOLEAN:      return LONG;
	case KTL_ENUM:         return LONG;
	case KTL_ENUMM:        return LONG;
	case KTL_MASK:         return LONG;
	case KTL_FLOAT:        return FLOAT;
	case KTL_DOUBLE:       return DOUBLE;
	case KTL_STRING:       return STRING;
	case KTL_INT_ARRAY:    return LONG;
	case KTL_FLOAT_ARRAY:  return FLOAT;
	case KTL_DOUBLE_ARRAY: return DOUBLE;
        default:               return INVALID;
    }
}

/*
 * convert method
 */
int ktlCnv::convert( ktlCnv::type inpType, int inpCount, const void *inpValue,
                     ktlCnv::type outType, int outCount, void *outValue )
{
    return ktlCnvMatrix[(int)inpType][(int)outType]
		     ( inpCount, inpValue, outCount, outValue );
}

// *********************************************************************

// the remainder of this file implements the various conversion functions;
// functions can assume that the to/from variables _do_ contain values of
// the expected types

// typedefs so "type" names can be used in declarations
typedef char    ktlCnvCHAR;
typedef short   ktlCnvSHORT;
typedef long    ktlCnvLONG;
typedef float   ktlCnvFLOAT;
typedef double  ktlCnvDOUBLE;
typedef char    ktlCnvSTRING; /* stet */

// macro for generating a conversion involving an invalid type

#define invfunc(INP,OUT) \
static int convert_##INP##_to_##OUT( int, const void *, \
			             int, void *) \
{ \
    return -1; \
}

// macro for generating a numeric -> numeric conversion function

#define numfunc(INP,OUT) \
static int convert_##INP##_to_##OUT( int inpCount, const void *inpValue, \
                                     int outCount, void *outValue ) \
{ \
    ktlCnv##INP *inp = ( ktlCnv##INP * ) inpValue; \
    ktlCnv##OUT *out = ( ktlCnv##OUT * ) outValue; \
    int cpyCount = ( inpCount < outCount ) ? inpCount : outCount; \
    int i; \
    for ( i = 0; i < cpyCount; i++ ) \
	out[i] = ( ktlCnv##OUT ) inp[i]; \
    for ( ; i < outCount; i++ ) \
	out[i] = ( ktlCnv##OUT ) 0; \
    return 0; \
}

// macro for generating a numeric -> string conversion function
// (only works if inpCount and outCount are both 1 (checked via assert())
// (can assume string is of size sizeof( pvString ))

#define strfunc(INP,OUT,set) \
static int convert_##INP##_to_##OUT( int inpCount, const void *inpValue, \
                                     int outCount, void *outValue ) \
{ \
    ktlCnv##INP *inp = ( ktlCnv##INP * ) inpValue; \
    char *out = ( char * ) outValue; \
    assert( inpCount == 1 && outCount == 1 ); \
    return ( set ); \
}

// macro for generating a string -> numeric conversion function
// (only works if inpCount and outCount are both 1 (checked via assert())

#define funcstr(INP,OUT,set) \
static int convert_##INP##_to_##OUT( int inpCount, const void *inpValue, \
                                     int outCount, void *outValue ) \
{ \
    char *inp = ( char * ) inpValue; \
    ktlCnv##OUT *out = ( ktlCnv##OUT * ) outValue; \
    assert( inpCount == 1 && outCount == 1 ); \
    return ( set ); \
}

// INVALID conversions

invfunc( INVALID, INVALID )
invfunc( INVALID, CHAR    )
invfunc( INVALID, SHORT   )
invfunc( INVALID, LONG    )
invfunc( INVALID, FLOAT   )
invfunc( INVALID, DOUBLE  )
invfunc( INVALID, STRING  )

// CHAR conversions

invfunc( CHAR, INVALID )
numfunc( CHAR, CHAR    )
numfunc( CHAR, SHORT   )
numfunc( CHAR, LONG    )
numfunc( CHAR, FLOAT   )
numfunc( CHAR, DOUBLE  )
strfunc( CHAR, STRING  , sprintf( out, "%c", *inp ) > 0 ? 0 : -1 )

// SHORT conversions

invfunc( SHORT, INVALID )
numfunc( SHORT, CHAR    )
numfunc( SHORT, SHORT   )
numfunc( SHORT, LONG    )
numfunc( SHORT, FLOAT   )
numfunc( SHORT, DOUBLE  )
strfunc( SHORT, STRING  , sprintf( out, "%hd", *inp ) > 0 ? 0 : -1 )

// LONG conversions

invfunc( LONG, INVALID )
numfunc( LONG, CHAR    )
numfunc( LONG, SHORT   )
numfunc( LONG, LONG    )
numfunc( LONG, FLOAT   )
numfunc( LONG, DOUBLE  )
strfunc( LONG, STRING  , sprintf( out, "%ld", *inp ) > 0 ? 0 : -1 )

// FLOAT conversions

invfunc( FLOAT, INVALID )
numfunc( FLOAT, CHAR    )
numfunc( FLOAT, SHORT   )
numfunc( FLOAT, LONG    )
numfunc( FLOAT, FLOAT   )
numfunc( FLOAT, DOUBLE  )
strfunc( FLOAT, STRING  , sprintf( out, "%g", *inp ) > 0 ? 0 : -1 )

// DOUBLE conversions

invfunc( DOUBLE, INVALID )
numfunc( DOUBLE, CHAR    )
numfunc( DOUBLE, SHORT   )
numfunc( DOUBLE, LONG    )
numfunc( DOUBLE, FLOAT   )
numfunc( DOUBLE, DOUBLE  )
strfunc( DOUBLE, STRING  , sprintf( out, "%g", *inp ) > 0 ? 0 : -1 )

// STRING conversions

invfunc( STRING, INVALID )
funcstr( STRING, CHAR    , sscanf( inp, "%c",  out ) == 1 ? 0 : -1 )
funcstr( STRING, SHORT   , sscanf( inp, "%hd", out ) == 1 ? 0 : -1 )
funcstr( STRING, LONG    , sscanf( inp, "%ld", out ) == 1 ? 0 : -1 )
funcstr( STRING, FLOAT   , sscanf( inp, "%f",  out ) == 1 ? 0 : -1 )
funcstr( STRING, DOUBLE  , sscanf( inp, "%lf", out ) == 1 ? 0 : -1 )
funcstr( STRING, STRING  , ( strcpy( out, inp ), 0 ) )

// contents of conversion matrix defined earlier
converter ktlCnvMatrix[ktlCnv::NUMBER][ktlCnv::NUMBER] =
{
    {
	convert_INVALID_to_INVALID,
	convert_INVALID_to_CHAR, 
	convert_INVALID_to_SHORT, 
	convert_INVALID_to_LONG, 
	convert_INVALID_to_FLOAT, 
	convert_INVALID_to_DOUBLE, 
	convert_INVALID_to_STRING,
    },
    {
	convert_CHAR_to_INVALID,
	convert_CHAR_to_CHAR, 
	convert_CHAR_to_SHORT, 
	convert_CHAR_to_LONG, 
	convert_CHAR_to_FLOAT, 
	convert_CHAR_to_DOUBLE, 
	convert_CHAR_to_STRING, 
    },
    {
	convert_SHORT_to_INVALID,
	convert_SHORT_to_CHAR, 
	convert_SHORT_to_SHORT, 
	convert_SHORT_to_LONG, 
	convert_SHORT_to_FLOAT, 
	convert_SHORT_to_DOUBLE, 
	convert_SHORT_to_STRING,
    },
    {
	convert_LONG_to_INVALID,
	convert_LONG_to_CHAR, 
	convert_LONG_to_SHORT, 
	convert_LONG_to_LONG, 
	convert_LONG_to_FLOAT, 
	convert_LONG_to_DOUBLE, 
	convert_LONG_to_STRING, 
    },	
    {
	convert_FLOAT_to_INVALID,
	convert_FLOAT_to_CHAR, 
	convert_FLOAT_to_SHORT, 
	convert_FLOAT_to_LONG, 
	convert_FLOAT_to_FLOAT, 
	convert_FLOAT_to_DOUBLE, 
	convert_FLOAT_to_STRING,
    },	
    {
	convert_DOUBLE_to_INVALID,
	convert_DOUBLE_to_CHAR, 
	convert_DOUBLE_to_SHORT, 
	convert_DOUBLE_to_LONG, 
	convert_DOUBLE_to_FLOAT, 
	convert_DOUBLE_to_DOUBLE, 
	convert_DOUBLE_to_STRING,
    },	
    {
	convert_STRING_to_INVALID,
	convert_STRING_to_CHAR, 
	convert_STRING_to_SHORT, 
	convert_STRING_to_LONG, 
	convert_STRING_to_FLOAT, 
	convert_STRING_to_DOUBLE, 
	convert_STRING_to_STRING, 
    }
};

/*
 * pvKtlCnv.cc,v
 * Revision 1.1.1.1  2000/04/04 03:22:15  wlupton
 * first commit of seq-2-0-0
 *
 * Revision 1.1  2000/03/06 19:21:04  wlupton
 * misc error reporting and type conversion mods
 *
 */

