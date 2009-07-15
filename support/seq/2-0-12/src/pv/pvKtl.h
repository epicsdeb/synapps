/* pvKtl.h,v 1.2 2000/04/14 21:53:29 jba Exp
 *
 * Definitions for EPICS sequencer KTL library (pvKtl)
 *
 * William Lupton, W. M. Keck Observatory
 */

#ifndef INCLpvKtlh
#define INCLpvKtlh

#include <sys/types.h>

#include "pv.h"

#include "ellLib.h"

#include "ktl.h"
#include "ktl_keyword.h"

/*
 * Pointer to value
 */
#define KTL_SIMPLE(_type) \
    ( (_type) != KTL_STRING      && (_type) != KTL_INT_ARRAY    && \
      (_type) != KTL_FLOAT_ARRAY && (_type) != KTL_DOUBLE_ARRAY )
#define KTL_VALPTR(_type,_poly) \
    ( ( KTL_SIMPLE(_type) ? ( void * ) (_poly) : ( void * ) (_poly)->s ) )

/*
 * Forward references
 */
class ktlKeyword;
class ktlVariable;

/*
 * Linked list node (is cast to and from an ELLNODE)
 */
// ### is this portable/standard? definitely better to use an STL class
class ktlNode {

public:
    epicsShareFunc inline void setData( const void * data ) { data_ = data; }
    epicsShareFunc inline const void *getData() const { return data_; }

private:
    ELLNODE node_;
    const void *data_;
};

/*
 * System
 */
class ktlSystem : public pvSystem {

public:
    epicsShareFunc ktlSystem( int debug = 0 );
    epicsShareFunc ~ktlSystem();

    epicsShareFunc virtual pvStat attach();
    epicsShareFunc virtual pvStat flush();
    epicsShareFunc virtual pvStat pend( double seconds = 0.0, int wait = FALSE );

    epicsShareFunc virtual pvVariable *newVariable( const char *name, pvConnFunc func = NULL,
				     void *priv = NULL, int debug = 0 );

    epicsShareFunc int getAttach() const { return attach_; }

private:
    int attach_;		/* whether to attach on open */
};

/*
 * Service
 */
class ktlService {

public:
    epicsShareFunc ktlService( ktlSystem *system, const char *name, int debug = 0 );
    epicsShareFunc ~ktlService();

    epicsShareFunc static ktlService *getService( ktlSystem *system, char *name,
				   int debug = 0 );

    epicsShareFunc inline void setDebug( int debug ) { debug_ = debug; }
    epicsShareFunc inline int getDebug() const { return debug_; }

    epicsShareFunc inline char *getName() const { return name_; }
    epicsShareFunc inline KTL_HANDLE *getHandle() const { return handle_; }
    epicsShareFunc inline int getFlags() const { return flags_; }

    epicsShareFunc void add( const ktlKeyword *keyword );
    epicsShareFunc void remove( const ktlKeyword *keyword );
    epicsShareFunc ktlKeyword *find( const char *keyName );

    epicsShareFunc static ktlService *first();
    epicsShareFunc ktlService *next() const;

    epicsShareFunc static fd_set *getFdsetsAll();
    epicsShareFunc static pvStat dispatchAll();

    // provide same error-handling interface as system and variable
    epicsShareFunc void setError( int status, pvSevr sevr, pvStat stat, const char *mess );
    epicsShareFunc inline int getStatus() const { return status_; }
    epicsShareFunc inline pvSevr getSevr() const { return sevr_; }
    epicsShareFunc inline pvStat getStat() const { return stat_; }
    epicsShareFunc inline void setStatus( int status ) { status_ = status; }
    epicsShareFunc inline void setStat( pvStat stat ) { stat_ = stat; }
    epicsShareFunc inline char *getMess() const { return mess_?mess_:(char *)""; }

private:
    int		debug_;         /* debugging level (inherited from varaible) */

    static ELLLIST list_;	/* list of ktlService objects */
    ktlNode     node_;		/* linked list node */
    char	*name_;		/* service name */
    KTL_HANDLE	*handle_;	/* KTL handle */
    int		flags_;		/* KTL_SUPER | KTL_STAMP ...or 0 */
    void	*keys_;		/* hash table of ktlKeyword objects */

    int         status_;        /* message system-specific status code */
    pvSevr      sevr_;          /* severity */
    pvStat      stat_;          /* status */
    char        *mess_;         /* error message */
};

/*
 * Keyword (distributes connection and monitor events to process variables)
 */
// ### some duplicate info between keyword and variable; should move cached
//     k/w info to keyword (and maybe do all KTL calls from keyword)
class ktlKeyword {

public:
    epicsShareFunc ktlKeyword( ktlService *service, const char *keyName, int debug = 0 );
    epicsShareFunc ~ktlKeyword();

    epicsShareFunc static ktlKeyword *getKeyword( ktlService *service, const char *keyName,
				   int debug = 0 );

    epicsShareFunc inline void setDebug( int debug ) { debug_ = debug; }
    epicsShareFunc inline int getDebug() { return debug_; }

    epicsShareFunc inline const ktlService *getService() const { return service_; }
    epicsShareFunc inline int getFlags() const { return getService()->getFlags(); }
    epicsShareFunc inline const char *getKeyName() const { return keyName_; }
    epicsShareFunc inline int getMonitored() { return monitored_; }

    epicsShareFunc int add( ktlVariable *variable );
    epicsShareFunc void remove( ktlVariable *variable );
    epicsShareFunc int monitorOn( ktlVariable *variable );
    epicsShareFunc int monitorOff( ktlVariable *variable );
    epicsShareFunc ktlVariable *first();
    epicsShareFunc ktlVariable *next( ktlVariable *variable );

    // provide same error-handling interface as system and variable
    epicsShareFunc void setError( int status, pvSevr sevr, pvStat stat, const char *mess );
    epicsShareFunc inline int getStatus() const { return status_; }
    epicsShareFunc inline pvSevr getSevr() const { return sevr_; }
    epicsShareFunc inline pvStat getStat() const { return stat_; }
    epicsShareFunc inline void setStatus( int status ) { status_ = status; }
    epicsShareFunc inline void setStat( pvStat stat ) { stat_ = stat; }
    epicsShareFunc inline char *getMess() const { return mess_?mess_:(char *)""; }

private:
    int		debug_;         /* debugging level (inherited from variable) */

    ktlService	*service_;	/* associated KTL service */
    const char  *keyName_;	/* keyword name (not including service name) */
    ELLLIST	list_;		/* list of associated variables */
    int		monitored_;	/* whether currently monitored */

    int         status_;        /* message system-specific status code */
    pvSevr      sevr_;          /* severity */
    pvStat      stat_;          /* status */
    char        *mess_;         /* error message */
};

/*
 * Process variable (several process variables may map to the same keyword)
 */
class ktlVariable : public pvVariable {

public:
    ktlVariable( ktlSystem *system, const char *name, pvConnFunc func = NULL,
		 void *priv = NULL, int debug = 0 );
    ~ktlVariable();

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

    inline void setConnected( int connected ) { connected_ = connected; }
    inline virtual int getConnected() const { return connected_; }
    virtual pvType getType() const;
    inline KTL_DATATYPE getKtlType() const { return type_; }
    inline virtual int getCount() const { return count_; }

    inline ktlNode *getNode() { return &node_; }
    inline const ktlService *getService() const { return service_; }
    inline KTL_HANDLE *getHandle() const { return getService()->getHandle(); }
    inline int getFlags() const { return getService()->getFlags(); }
    inline const char *getKeyName() const { return keyName_; }
    inline pvCallback *getMonitor() { return monitor_; }

private:
    ktlNode	node_;		/* linked list node */
    ktlService	*service_;	/* associated KTL service */
    const char	*keyName_;	/* keyword name (not including service name) */
    ktlKeyword	*keyword_;	/* associated KTL keyword */
    KTL_DATATYPE type_;		/* native KTL type */
    int		count_;		/* native KTL number of elements (fixed) */
    int		readNot_;	/* can read with notify */
    int		writeNot_;	/* can write with notify */
    int		readCont_;	/* can read continuously (monitor) */
    int		connected_;	/* whether keyword is connected */
    pvCallback	*monitor_;	/* monitor callback object */
};

#endif /* INCLpvKtlh */

/*
 * pvKtl.h,v
 * Revision 1.2  2000/04/14 21:53:29  jba
 * Changes for win32 build.
 *
 * Revision 1.1.1.1  2000/04/04 03:22:15  wlupton
 * first commit of seq-2-0-0
 *
 * Revision 1.11  2000/03/31 23:01:29  wlupton
 * supported setStatus
 *
 * Revision 1.10  2000/03/18 04:00:25  wlupton
 * converted to use new configure scheme
 *
 * Revision 1.9  2000/03/16 02:11:29  wlupton
 * supported KTL_ANYPOLY (plus misc other mods)
 *
 * Revision 1.8  2000/03/07 19:55:32  wlupton
 * nearly sufficient tidying up
 *
 * Revision 1.7  2000/03/07 09:27:39  wlupton
 * drastically reduced use of references
 *
 * Revision 1.6  2000/03/07 08:46:30  wlupton
 * created ktlKeyword class (works but a bit messy)
 *
 * Revision 1.5  2000/03/06 19:21:04  wlupton
 * misc error reporting and type conversion mods
 *
 * Revision 1.4  2000/03/01 02:07:15  wlupton
 * converted to use new OSI library
 *
 * Revision 1.3  1999/07/01 20:50:20  wlupton
 * Working under VxWorks
 *
 * Revision 1.2  1999/06/15 10:11:04  wlupton
 * demo sequence mostly working with KTL
 *
 * Revision 1.1  1999/06/11 02:20:33  wlupton
 * nearly working with KTL
 *
 */
