/* pvFile.h,v 1.2 2001/02/23 20:45:22 jba Exp
 *
 * Definitions for demonstration EPICS sequencer file library (pvFile)
 *
 * William Lupton, W. M. Keck Observatory
 */

#ifndef INCLpvFileh
#define INCLpvFileh
#ifndef _WIN32
#include <sys/time.h>
#endif
#include <sys/types.h>

#include "pv.h"

/*
 * System
 */
class fileSystem : public pvSystem {

public:
    fileSystem( int debug = 0 );
    ~fileSystem();

    virtual pvStat pend( double seconds = 0.0, int wait = FALSE );

    virtual pvVariable *newVariable( const char *name, pvConnFunc func = NULL,
				     void *priv = NULL, int debug = 0 );

private:
    FILE *ifd_;		/* input file descriptor */
    FILE *ofd_;		/* output file descriptor */
    fd_set readfds_;	/* read file descriptor (contains fileno(ifd_)) */
};

/*
 * Process variable
 */
class fileVariable : public pvVariable {

public:
    fileVariable( fileSystem *system, const char *name, pvConnFunc func = NULL,
		  void *priv = NULL, int debug = 0 );
    ~fileVariable();

    virtual pvStat get( pvType type, int count, pvValue *value );
    virtual pvStat getNoBlock( pvType type, int count, pvValue *value );
    virtual pvStat getCallback( pvType type, int count,
		pvEventFunc func, void *arg = NULL );
    virtual pvStat put( pvType type, int count, pvValue *value );
    virtual pvStat putNoBlock( pvType type, int count, pvValue *value );
    virtual pvStat putCallback( pvType type, int count, pvValue *value,
		pvEventFunc func, void *arg = NULL );
    virtual pvStat monitorOn( pvType type, int count,
		pvEventFunc func, void *arg = NULL,
		pvCallback **pCallback = NULL );
    virtual pvStat monitorOff( pvCallback *callback = NULL );

    virtual int getConnected() const { return TRUE; }
    virtual pvType getType() const { return pvTypeSTRING; }
    virtual int getCount() const { return 1; }

private:
    char *value_;	/* current value */
};

#endif /* INCLpvFileh */

/*
 * pvFile.h,v
 * Revision 1.2  2001/02/23 20:45:22  jba
 * Change for win32.
 *
 * Revision 1.1.1.1  2000/04/04 03:22:15  wlupton
 * first commit of seq-2-0-0
 *
 * Revision 1.1  2000/03/31 23:00:07  wlupton
 * initial insertion
 *
 */

