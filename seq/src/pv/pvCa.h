/*************************************************************************\
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
/* Definitions for EPICS sequencer CA library (pvCa)
 *
 * William Lupton, W. M. Keck Observatory
 */

#ifndef INCLpvCah
#define INCLpvCah

#include "alarm.h"
#include "cadef.h"

#include "shareLib.h"
#include "pv.h"

/*
 * System
 */
class caSystem : public pvSystem {

public:
    caSystem( int debug = 0 );
    ~caSystem();

    virtual pvStat attach();
    virtual pvStat flush();
    virtual pvStat pend( double seconds = 0.0, int wait = FALSE );

    virtual pvVariable *newVariable( const char *name, pvConnFunc func = NULL,
				     void *priv = NULL, int debug = 0 );

private:
    ca_client_context *context_; /* channel access context of creator */
};

/*
 * Process variable
 */
class caVariable : public pvVariable {

public:
    caVariable( caSystem *system, const char *name, pvConnFunc func = NULL,
		void *priv = NULL, int debug = 0 );
    ~caVariable();

    virtual pvStat get( pvType type, unsigned count, pvValue *value );
    virtual pvStat getNoBlock( pvType type, unsigned count, pvValue *value );
    virtual pvStat getCallback( pvType type, unsigned count,
		pvEventFunc func, void *arg = NULL );
    virtual pvStat put( pvType type, unsigned count, pvValue *value );
    virtual pvStat putNoBlock( pvType type, unsigned count, pvValue *value );
    virtual pvStat putCallback( pvType type, unsigned count, pvValue *value,
		pvEventFunc func, void *arg = NULL );
    virtual pvStat monitorOn( pvType type, unsigned count,
		pvEventFunc func, void *arg = NULL,
		pvCallback **pCallback = NULL );
    virtual pvStat monitorOff( pvCallback *callback = NULL );

    virtual int getConnected() const;
    virtual pvType getType() const;
    virtual unsigned getCount() const;

private:
    chid chid_;		/* channel access id */
};

#endif /* INCLpvCah */

/*
 * pvCa.h,v
 * Revision 1.4  2001/10/15 21:41:42  jhill
 * fixed to match R3.14 CA client lib
 *
 * Revision 1.3  2001/07/05 14:42:16  mrk
 * ca changed client contect
 *
 * Revision 1.2  2000/04/14 21:53:28  jba
 * Changes for win32 build.
 *
 * Revision 1.1.1.1  2000/04/04 03:22:14  wlupton
 * first commit of seq-2-0-0
 *
 * Revision 1.11  2000/03/18 04:00:25  wlupton
 * converted to use new configure scheme
 *
 * Revision 1.10  2000/03/16 02:11:29  wlupton
 * supported KTL_ANYPOLY (plus misc other mods)
 *
 * Revision 1.9  2000/03/07 09:27:39  wlupton
 * drastically reduced use of references
 *
 * Revision 1.8  2000/02/19 01:13:03  wlupton
 * fixed log entry formatting
 *
 * Revision 1.7  2000/02/16 02:31:44  wlupton
 * merged in v1.9.5 changes
 *
 * Revision 1.6  1999/07/07 18:50:34  wlupton
 * supported full mapping from EPICS status and severity to pvStat and pvSevr
 *
 * Revision 1.5  1999/07/01 20:50:19  wlupton
 * Working under VxWorks
 *
 * Revision 1.4  1999/06/10 00:35:04  wlupton
 * demo sequencer working with pvCa
 *
 * Revision 1.3  1999/06/08 19:21:44  wlupton
 * CA version working; about to use in sequencer
 *
 * Revision 1.2  1999/06/08 03:25:22  wlupton
 * nearly complete CA implementation
 *
 * Revision 1.1  1999/06/07 21:46:46  wlupton
 * working with simple pvtest program
 *
 */
