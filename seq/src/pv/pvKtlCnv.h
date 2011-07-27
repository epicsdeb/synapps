/* pvKtlCnv.h,v 1.1.1.1 2000/04/04 03:22:15 wlupton Exp
 *
 * Definitions for EPICS sequencer KTL type conversion (pvKtlCnv)
 *
 * Uses large amounts of code from the cdevData class
 *
 * William Lupton, W. M. Keck Observatory
 */

#ifndef INCLpvKtlCnvh
#define INCLpvKtlCnvh

#include <sys/types.h>

#include "pv.h"

#include "ktl.h"
#include "ktl_keyword.h"

// ktlCnv class (static members/methods only)
class ktlCnv
{
public:

    // type (index into conversion table */
    enum type { INVALID, CHAR, SHORT, LONG, FLOAT, DOUBLE, STRING, NUMBER };

    // map types to ktlCnv types (indices into conversion table)
    static type getType( pvType t );
    static type getType( KTL_DATATYPE t );

    // conversion method
    static int convert( type inpType, int inpCount, const void *inpValue,
		        type outType, int outCount, void *outValue );

private:

    // constructors
    ktlCnv() {}
};

/*
 * pvKtlCnv.h,v
 * Revision 1.1.1.1  2000/04/04 03:22:15  wlupton
 * first commit of seq-2-0-0
 *
 * Revision 1.1  2000/03/06 19:21:04  wlupton
 * misc error reporting and type conversion mods
 *
 */

#endif /* INCLpvKtlCnvh */

