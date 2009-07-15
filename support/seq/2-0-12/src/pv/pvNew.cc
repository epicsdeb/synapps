/* pvNew.cc,v 1.2 2000/04/14 21:53:29 jba Exp
 *
 * Routine to create a system object for a named message system
 * (will eventually load dynamically)
 */

#include <stdio.h>
#include <string.h>

#define epicsExportSharedSymbols
#include "pv.h"

#if defined( PVCA )
#include "pvCa.h"
#endif

#if defined( PVFILE )
#include "pvFile.h"
#endif

#if defined( PVKTL ) && !defined( vxWorks )
#include "pvKtl.h"
#endif

/*+
 * Routine:     newPvSystem
 *
 * Purpose:     return pointer to appropriate pvSystem sub-class
 *
 * Description:
 */
epicsShareFunc pvSystem * epicsShareAPI newPvSystem( const char *name, int debug ) {

#if defined( PVCA )
    if ( strcmp( name, "ca" ) == 0 )
	return new caSystem( debug );
#endif

#if defined( PVFILE )
    if ( strcmp( name, "file" ) == 0 )
	return new fileSystem( debug );
#endif

#if defined( PVKTL ) && !defined( vxWorks )
    if ( strcmp( name, "ktl" ) == 0 )
	return new ktlSystem( debug );
#endif

    return NULL;
}

