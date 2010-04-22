/*+*****************************************************************************
 Module:	tcsSeq.stpp

 Author:	W Lupton

 Description:	TCS subsystem sequencer. This is in overall control of the
		DCS subsystems and is responsible for:

		1. selection of instrument (and therefore focal station)

		2. coordination of initialization, halt and standby

		3. presentation of overall status

		4. reaction to system events (e.g. stop guiding if axes
		   fault)

		5. system-level fault handling

		6. interception of some commands (e.g. telescope slews
		   and some offsets)

		7. command completion (e.g. completion of slew)

		Further details are given in KSD/101 "Keck II DCS high-level
		coordination".

		The standard "three state-set" model is used, with a main
		sequence that manages major state changes, a fault sequence
		that handles fault detection, and a command sequence that
		handles external commands.

		The above state-set model is defined and discussed in KSD/87,
		"Keck II EPICS standards".

 Notes:		1. This code runs only under VxWorks.

		2. Operation:
		   a) To load:	    -> ld <${version}/bin/${target}/tcsSeq.o
		   b) To execute:   -> seq &tcsSeq, "tel=k2:, version=vvv"
		      ("tel" defines a prefix which will be applied to all
		       EPICS process variable names)
		   c) To stop:	    -> td "tcsSeq"
		   d) To unload:    -> unld "tcsSeq.o"

		3. Status from channel access operations is not tested.
		   Should it be?

		4. Not done:

		   a) maintenance of stateVal and seqRun variables
		   b) control over ignoring specified subsystems
		   c) honor HALT in STANDBY (same checks)
*******************************************************************************/

program tcsSeq( "tel=k0:, version=?" )
			/* will be entry point name and primary task name */

option -c;		/* allow start before all connections are made */
option -l;		/* use C line numbers in error messages etc. */

%%#include <string.h>
%%#ifdef vxWorks
%%int strcasecmp( char *s1, char *s2 );
%%int strncasecmp( char *s1, char *s2, int n );
%%#endif          	/* Taken from etcLib for vxWorks */

%%#include <epicsPrint.h>
%%#include "knlConst.h"	/* for USE() macro (need ANSI pre-processor) */
%%RCSID( "$Id" );
			/* avoids "not used" warning */

%%#include "tcs.h"
			/* function prototypes etc. */

/*******************************************************************************
 * Include files (_only_ for #define constants)
 ******************************************************************************/

#include "knlConst.h"	/* for logging levels, status values etc. */

/* TCS version of SETSTAT macro */
#define SETSTAT( _stateStr, _stateVal, _status, _errstr ) \
        strcpy( stateStr, _stateStr ); \
	stateVal = _stateVal; \
        status = _status; \
        strcpy( errstr, _errstr ); \
        pvPut( stateStr ); \
        pvPut( stateVal ); \
        pvPut( status ); \
        pvPut( errstr )

/* TCS version of LOGMSG macro */
#undef  LOGMSG
#define LOGMSG( _level, _logmsg ) \
	if ( logLevel & ( _level ) ) epicsPrintf( "SEQ: %s", _logmsg )

/* short names for the few external DCS status values that are used */
#define S_fault	DCS_M_STATUS_FAULT
#define S_stby	DCS_M_STATUS_STNBY
#define S_init	DCS_M_STATUS_INIT
#define S_halt	DCS_M_STATUS_HALT

/*******************************************************************************
 * Process variables (shared between all state sets)
 ******************************************************************************/

#define SNCVAR( _type, _intvar, _flagvar, _pvname ) \
    MONVAR( _type, _intvar, _pvname ); evflag _flagvar; sync _intvar _flagvar

/* External commands (serviced by the sequencer) */
MONVAR( long,   init,	      "{tel}dcs:init" );
MONVAR( long,   halt,         "{tel}dcs:halt" );
MONVAR( long,   stby,         "{tel}dcs:stby" );
MONVAR( long,   bypass,       "{tel}dcs:bypass" );

evflag initFlag;
evflag haltFlag;
evflag stbyFlag;
evflag bypassFlag;
			/* set by cmd seq to inform main seq that the */
			/* various different external commands have */
			/* been received */

SNCVAR( string, simulateLevel, simulateLevelFlag, "{tel}dcs:simulate" );
			/* synched to event flag because it's a string and */
			/* normal command handling technique doesn't apply */
		
SNCVAR( long,   logLevel,      logLevelFlag,      "{tel}dcs:logLevel" );
			/* synched to event flag because command may want */
			/* to set the level to zero */

MONVAR( long,   logLevelSet,  "{tel}dcs:logLevelSet" );
MONVAR( long,   logLevelClr,  "{tel}dcs:logLevelClr" );

/* External status variables (read by the sequencer) */
string instrument;
			/* value copied from instrumentPV before it's cleared */
SNCVAR( string, instrumentPV,   instrumentFlag, "{tel}dcs:instrume" );
			/* synched to event flag so the PV can be cleared */
			/* after each setting (so will get a monitor even */
			/* if it's set to the same value next time) */

MONVAR( string, focalstation, "{tel}dcs:focalstn" );

MONVAR( double, focallength,  "{tel}dcs:pnt:fast.FLEN" );
			/* needed only for pointing origin offsets */

MONVAR( string, secModuleId,  "{tel}dcs:sec:moduleId" );
MONVAR( string, casModuleId,  "{tel}dcs:rot:cas:moduleId" );
MONVAR( string, fwdModuleId,  "{tel}dcs:rot:fwd:moduleId" );
MONVAR( string, fwdTertRbv,   "{tel}dcs:rot:tertPinPos" );

/* Internal status variables (written by the sequencer) */
MONVAR( string, telescope,    "{tel}dcs:telescop" );

ASSVAR( string, stateStr,     "{tel}dcs:state"    ); /* "state" is reserved */
ASSVAR( long,   stateVal,     "{tel}dcs:stateVal" );
ASSVAR( long,   seqRun,       "{tel}dcs:seqRun"   );
ASSVAR( long,   status,       "{tel}dcs:status"   );
ASSVAR( string, errstr,       "{tel}dcs:errstr"   );
ASSVAR( string, version,      "{tel}dcs:version"  );
ASSVAR( string, instrumentRbv, "{tel}dcs:instrumeRbv" );
ASSVAR( string, focalstationRbv, "{tel}dcs:focalstnRbv" );

ASSVAR( long,   initActive,   "{tel}dcs:initActive" );
ASSVAR( long,   haltActive,   "{tel}dcs:haltActive" );
ASSVAR( long,   stbyActive,   "{tel}dcs:stbyActive" );
ASSVAR( long,   bypassActive, "{tel}dcs:bypassActive" );

ASSVAR( long,   initDone,     "{tel}dcs:initDone" );

/* Subsystem commands and readbacks (written and monitored by the sequencer) */

#define MAXSUBSYS 6
			/* maximum number of subsystems */

/* Local utility macros for subsystem arrays (NB, have to change if
   MAXSUBSYS changes) */
#define ASSARR( _type, _intvar ) \
	_type _intvar[MAXSUBSYS]; assign _intvar to {"", "", "", "", "", ""}

#define MONARR( _type, _intvar ) \
	ASSARR( _type, _intvar ); monitor _intvar

#define SNCARR( _type, _intvar, _flagvar ) \
	MONARR( _type, _intvar); evflag _flagvar; sync _intvar _flagvar

#define QUEARR( _type, _intvar, _flagvar ) \
	MONARR( _type, _intvar); evflag _flagvar; syncQ _intvar _flagvar

ASSARR( long,	tcsNeeded );
			/* whether subsystems are needed in current config */

ASSARR( long,   tcsInit );
QUEARR( long,   tcsInitActive,  tcsInitActiveFlag );
MONVAR( double,	tcsInitTimeout, "{tel}dcs:initTimeout" );

ASSARR( long,   tcsHalt );
QUEARR( long,   tcsHaltActive,  tcsHaltActiveFlag );
MONVAR( double,	tcsHaltTimeout, "{tel}dcs:haltTimeout" );

ASSARR( long,   tcsStby );
QUEARR( long,   tcsStbyActive,  tcsStbyActiveFlag );
MONVAR( double,	tcsStbyTimeout, "{tel}dcs:stbyTimeout" );

ASSARR( long,   tcsBypass );
QUEARR( long,   tcsBypassActive,  tcsBypassActiveFlag );
MONVAR( double,	tcsBypassTimeout, "{tel}dcs:bypassTimeout" );

QUEARR( long,   tcsStatus,	tcsStatusFlag );
			/* used to detect command failure */
SNCARR( long,   tcsStatusMon,	tcsStatusMonFlag );
			/* used for fault monitoring (could use syncQ...) */

SNCARR( string, tcsState,	tcsStateFlag );
SNCARR( long,   tcsStateVal,	tcsStateValFlag );
			/* NB, have to use sync here because sequencer
			   does not re-execute event functions based on
			   array variables (could use syncQ...) */

/* Pointing origin commands and readbacks (all in the command state-set) */

SNCVAR( double, pntPoXpos, pntPoXposFlag, "{tel}dcs:pnt:fast.XIM"  );
SNCVAR( double, pntPoYpos, pntPoYposFlag, "{tel}dcs:pnt:fast.YIM"  );
SNCVAR( double, pntPoXoff, pntPoXoffFlag, "{tel}dcs:pnt:fast.XIMO" );
SNCVAR( double, pntPoYoff, pntPoYoffFlag, "{tel}dcs:pnt:fast.YIMO" );
SNCVAR( double, pntPoXinc, pntPoXincFlag, "{tel}dcs:pnt:fast.XIIN" );
SNCVAR( double, pntPoYinc, pntPoYincFlag, "{tel}dcs:pnt:fast.YIIN" );
SNCVAR( double, pntLoad  , pntLoadFlag,   "{tel}dcs:pnt:fast.LOAD" );
			/* current pointing origin/offset (PNT variables) */

#define MAXPTGORG 9
			/* maximum number of pointing origins (also limited
			   by corresponding definition in ptgorg.c) */

MONVAR( string, poName,		"{tel}dcs:poName" );
MONVAR( double, poXpos,		"{tel}dcs:poXpos" );
MONVAR( double, poYpos,		"{tel}dcs:poYpos" );
			/* parameters (used when "go" flags are set) */

SNCVAR( double, poXoff, poXoffFlag, "{tel}dcs:poXoff" );
SNCVAR( double, poYoff, poYoffFlag, "{tel}dcs:poYoff" );
			/* immediate action variables */

SNCVAR( long,   poReset,  poResetFlag,	"{tel}dcs:poReset"  );
SNCVAR( long,   poPreset, poPresetFlag,	"{tel}dcs:poPreset" );
SNCVAR( long,   poSelect, poSelectFlag,	"{tel}dcs:poSelect" );
SNCVAR( long,   poCancel, poCancelFlag,	"{tel}dcs:poCancel" );
SNCVAR( long,   poOff,    poOffFlag,	"{tel}dcs:poOff"    );
			/* commands ("go" flags) */

ASSVAR( long,	poTotalRbv,	"{tel}dcs:poTotalRbv");
			/* total number of defined pointing origins */

ASSVAR( string,	poNameRbv,	"{tel}dcs:poNameRbv" );
ASSVAR( long, 	poStatRbv,	"{tel}dcs:poStatRbv" );
ASSVAR( double,	poXposRbv,	"{tel}dcs:poXposRbv" );
ASSVAR( double,	poYposRbv,	"{tel}dcs:poYposRbv" );
ASSVAR( double,	poXoffRbv,	"{tel}dcs:poXoffRbv" );
ASSVAR( double,	poYoffRbv,	"{tel}dcs:poYoffRbv" );
			/* current pointing origin details */

string poNameArr[MAXPTGORG]; assign poNameArr to {
    "{tel}dcs:poName1", "{tel}dcs:poName2", "{tel}dcs:poName3",
    "{tel}dcs:poName4", "{tel}dcs:poName5", "{tel}dcs:poName6",
    "{tel}dcs:poName7", "{tel}dcs:poName8", "{tel}dcs:poName9"
};			/* defined pointing origin names */

long poStatArr[MAXPTGORG]; assign poStatArr to {
    "{tel}dcs:poStat1", "{tel}dcs:poStat2", "{tel}dcs:poStat3",
    "{tel}dcs:poStat4", "{tel}dcs:poStat5", "{tel}dcs:poStat6",
    "{tel}dcs:poStat7", "{tel}dcs:poStat8", "{tel}dcs:poStat9"
};			/* defined pointing origin statuses */

double poXposArr[MAXPTGORG]; assign poXposArr to {
    "{tel}dcs:poXpos1", "{tel}dcs:poXpos2", "{tel}dcs:poXpos3",
    "{tel}dcs:poXpos4", "{tel}dcs:poXpos5", "{tel}dcs:poXpos6",
    "{tel}dcs:poXpos7", "{tel}dcs:poXpos8", "{tel}dcs:poXpos9"
};			/* defined pointing origin X positions */

double poYposArr[MAXPTGORG]; assign poYposArr to {
    "{tel}dcs:poYpos1", "{tel}dcs:poYpos2", "{tel}dcs:poYpos3",
    "{tel}dcs:poYpos4", "{tel}dcs:poYpos5", "{tel}dcs:poYpos6",
    "{tel}dcs:poYpos7", "{tel}dcs:poYpos8", "{tel}dcs:poYpos9"
};			/* defined pointing origin Y positions */

/*******************************************************************************
 * Event flags
 ******************************************************************************/

/* Fault checking */
evflag faultEnabledFlag;
			/* set by main seq to enable fault seq's fault */
			/* checking; cleared by main seq to disable */
evflag faultDetectedFlag;
			/* set by fault seq to inform main seq that a */
			/* fault has been detected; cleared by fault seq */
			/* when main seq acknowledges fault */
evflag faultAckFlag;
			/* set by main seq to acknowledge the setting of */
			/* faultDetectedFlag */
evflag pvDisconnectedFlag;
			/* set by fault seq to indicate that a PV disconnected */

/* Command processing */
evflag commandEnabledFlag;
			/* set by main seq to enable command processing */

evflag readConfigFlag;
			/* set by command seq to request reading of */
			/* instrument configuration flags; cleared once */
			/* they have been read */

evflag allDoneFlag;
evflag anyFailFlag;
			/* used by main seq for managing parallel subsystem */
			/* commands */

/*******************************************************************************
 * Local variables (shared between all state sets)
 ******************************************************************************/

long i;
long j;
			/* general-purpose counters */
char *tel;
			/* telescope name prefix in form "k1:" or "k2:" */
char *newState;
			/* new state (used in HALTED state) */
long newStateVal;
			/* corresponding numeric state value */
long allDone;
			/* whether set of parallel commands are all done */
long anyFail;
			/* whether any of set of parallel commands failed */
char *initState;
			/* state from which initialized */
char buff[80];
			/* re-usable work buffer */
long alreadyInit;
			/* already successfully initialized? */
long nSubsys;
			/* number of subsystems */
long active[MAXSUBSYS];
			/* whether current subsystem commands are active */

/*+*****************************************************************************
 Function:	tcsMain

 Author:	W Lupton

 Abstract:	TCS main sequence

 Description:   This sequence manages the main TCS state transitions.
		Refer to inline comments (and code) for details. The
		following is a summary only:

		1. On entry to the START state, wait for all process
		   variables to connect

		2. Attempt to identify the current instrument and / or
		   focal station by reading EPICS variables (module ids
		   etc). Enter the STANDBY state

		3. Whenever the instrument name is set, check whether
		   that instrument (and focal station) can be achieved
		   in the current hardware configuration. Instrument
		   selection only actually occurs on the next
		   initialization

		4. Initialization fails immediately if the selected
		   instrument is not achievable. Assuming it is (via a
		   tertiary rotation), a list of subsystems to be
		   initialized is derived from the instrument name

		5. The bypass command may have been issued, in which
		   case full initialization of subsystems will be
		   bypassed. For each subsystem, force it to its standby
		   state and then, if appropriate, send bypass commands

		6. Now initialize all the subsytems in parallel. On
		   completion, rotate the tertiary (if necessary) to
		   select the instrument

		7. The final stage of initialization is to use pvload()
		   to read instrument-specific parameters. These can
		   include nominal positions for mechanisms (e.g. focus)
		   and these mechanisms are moved in parallel with the
		   tertiary move (if any). Enable fault checking and
		   enter the HALTED state

		8. Initialization from an initialized state (i.e. from a
		   state other than STANDBY) is similar but does not
		   force the subsystems into any particular state. It
		   will therefore be quick for all subsystems which do
		   not need to re-home (or whatever a full
		   initialization involves)

		9. When in the "normal" states (HALTED, SLEWING etc),
		   subsystem states are monitored and combined to
		   produce an overall state

		10. Requests to move mechanisms (e.g. slew the
		    telescope) are not handled by the sequence. They are
		    sent directly to the subsystems in question.
		    However, the TCS sequence state will adjust as
		    appropriate to SLEWING, MOVING etc as a result of
		    monitors on subsystem state variables

*******************************************************************************/

ss tcsMain {

    /*
     * START state. This is the initial state. Set the default logging
     * level (and do anything else that needs to happen before variables
     * connect)
     */
    state START {
	when () {
	    LOGMSG( LOG_MAIN, "tcsMain: START -> START_CONNECT [OK]\n" );
	    SETSTAT( "START", S_stby, S_knl_ok, "(re-)connecting" );

	    /* Disable fault checking until all channels have connected */
	    efClear( faultEnabledFlag );

	    /* initially enable all logging (TBD) */
	    logLevel = LOG_DEBUG | LOG_MAIN | LOG_COMMAND | LOG_FAULT;
	    pvPut( logLevel );
	} state START_CONNECT
    }

    /*
     * START_CONNECT state. Awaiting connection of EPICS variables
     */
    state START_CONNECT {

	/*
	 * All EPICS process variables have connected. Enter START_ACTIVE state
	 */
	when ( pvConnectCount() == pvAssignCount() ) {
	    LOGMSG( LOG_MAIN, "tcsMain: START_CONNECT -> START_ACTIVE [OK]\n" );
	    SETSTAT( "START", S_stby, S_knl_ok, "OK" );

	    /* clear "PV disconnected" flag */
	    efClear( pvDisconnectedFlag );
	} state START_ACTIVE

	/*
	 * 30 seconds has passed and still not all EPICS process
	 * variables have connected. Continue waiting
	 *
	 * Should add finer control / reporting here
	 */
	when ( delay( 30 ) ) {
#if FALSE
	    LOGMSG(LOG_DEBUG,"tcsMain: START_CONNECT -> START_CONNECT [t/o]\n");
#endif
	    SETSTAT( "START", S_stby, S_knl_timeout, "connect timeout" );
	} state START_CONNECT
    }

    /*
     * START_ACTIVE state. Attempt to identify instrument and focal station,
     * and set initial values
     */
    state START_ACTIVE {

	when () {
	    LOGMSG( LOG_MAIN, "tcsMain: START_ACTIVE -> STANDBY [OK]\n" );
	    SETSTAT( "STBY", S_stby, S_knl_ok, "OK" );

	    /* Set telescope prefix variable */
	    tel = macValueGet( "tel" );

	    /* Convert to name for TELESCOP FITS keyword */
	    tcsTelescopeName( tel, telescope );
	    pvPut( telescope );

	    /* Set version variable */
	    strcpy( version, macValueGet( "version" ) );
	    pvPut( version );

	    /* Set the instrument and focal station "readback" PVs
	       unconditionally to "<unk>" (unknown) */
	    strcpy( instrumentRbv, "<unk>" );
	    pvPut( instrumentRbv );
	    strcpy( focalstationRbv, "<unk>" );
	    pvPut( focalstationRbv );

	    /* Attempt to identify instrument and focal station */
	    tcsInstrumentIdentify(
		    tel,			/* telescope name */
		    secModuleId,		/* secondary module */
		    casModuleId,		/* cassegrain module */
		    fwdModuleId,		/* forward cass module */
		    fwdTertRbv,			/* tertiary position */
		    instrumentRbv,		/* instrument */
		    focalstationRbv );		/* focal station */

	    /* Set the "request" PVs if not already set (setting a "known"
	       instrument will cause the command sequence to request that
	       instrument configuration files be read) */
	    if ( tcsUnknown( instrument ) ) {
		strcpy( instrumentPV, instrumentRbv );
		pvPut( instrumentPV );
	    }
	    if ( tcsUnknown( focalstation ) ) {
		strcpy( focalstation, focalstationRbv );
		pvPut( focalstation );
	    }

	    /* Request that configuration files should be read if and only if
	       the instrument is not unknown */
	    if ( tcsUnknown( instrument ) )
		efClear( readConfigFlag );
	    else
		efSet( readConfigFlag );

	    /* Enable fault checking now all channels have connected */
	    efSet( faultEnabledFlag );

	    /* Clear command event and "active" flags */
	    EFPVCLEAR( initFlag, initActive );
	    EFPVCLEAR( haltFlag, haltActive );
	    EFPVCLEAR( stbyFlag, stbyActive );
	    EFPVCLEAR( bypassFlag, bypassActive );

	    /* Clear "init done" variable */
	    PVCLEAR( initDone );

	    /* Enable command processing */	
	    efSet( commandEnabledFlag );
	} state STANDBY
    }

    /*
     * STANDBY_SEND state. Request all subsystems to enter their standby
     * states. Enter STANDBY_ACTIVE state
     */
    state STANDBY_SEND {

	/*
	 * If have never initialized, don't know which subsystems to place
	 * in standby. Enter STANDBY_DONE state
	 */
	when ( nSubsys == 0 ) {
	    LOGMSG( LOG_MAIN,"tcsMain: STANDBY_SEND -> STANDBY_DONE [no ss]\n");
	    SETSTAT( "STBY", S_stby, S_knl_fault, "never initialized" );
	} state STANDBY_DONE

	/*
	 * Otherwise, send standby commands and enter STANDBY_ACTIVE state
	 */
	when () {
	    LOGMSG( LOG_MAIN, "tcsMain: STANDBY_SEND -> STANDBY_ACTIVE [OK]\n");
	    SETSTAT( "STBY", S_stby, S_knl_ok, "OK" );

	    /* Ensure syncQ queues are empty */
	    pvFreeQ( tcsStbyActive );
	    pvFreeQ( tcsStatus );

	    /* Ensure event flags are clear */
	    efClear( allDoneFlag );
	    efClear( anyFailFlag );

	    /* Request all subsystems to enter their standby states */
	    tcsResetActive( nSubsys, tcsNeeded, tcsStbyActive, active );
	    for ( i = 0; i < nSubsys; i++ ) {
		PVSET( tcsStby[i] );
	    }
	} state STANDBY_ACTIVE
    }	

    /*
     * STANDBY_ACTIVE state. Wait until all subsystems are in their standby
     * states
     */
    state STANDBY_ACTIVE {

	/*
	 * Fault detected. Set fault flag and enter FAULT_ACK state.
	 */
	when ( efTest( faultDetectedFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: STANDBY_ACTIVE -> FAULT_ACK [fault]\n" );
	    SETSTAT( "FAULT", S_fault, S_knl_fault, "faulted" );
	} state FAULT_ACK

	/*
	 * HALT command received. Clear flag and enter STANDBY state (don't
	 * do this on STANDBY command because it's already active)
	 */
	when ( efTest( haltFlag ) ) {
	    LOGMSG(LOG_MAIN,"tcsMain: STANDBY_ACTIVE -> STANDBY_DONE [halt]\n");
	    SETSTAT( "STBY", S_stby, S_knl_ok, "OK" );
	    EFPVCLEAR( haltFlag, haltActive );
	} state STANDBY_DONE

	/*
	 * A "standby active" variable has changed value
	 */
	when ( pvGetQ( tcsStbyActive ) || pvGetQ( tcsStatus ) ) {

	    /* Check for completion */
	    tcsUpdateActive( nSubsys, tcsStbyActive, tcsStatus,
			     active, &allDone, &anyFail );

	    /* If all subsystems are done, set the "all done" event flag
	       and, if any command failed, the "any failed" event flag */
	    if ( allDone ) {
		efSet( allDoneFlag );
		if ( anyFail ) efSet( anyFailFlag );
	    }
	} state STANDBY_ACTIVE

	/*
	 * All standby commands are done and at least one failed. For now,
	 * report this but continue with initialization (allow the subsystems
	 * to recover if they can)
	 */
	when ( efTestAndClear( anyFailFlag ) ) {
	    LOGMSG(LOG_MAIN,"tcsMain: STANDBY_ACTIVE -> STANDBY_ACTIVE [ig]\n");
	} state STANDBY_ACTIVE

	/*
	 * All standby commands are done. Enter STANDBY state
	 */
	when ( efTestAndClear( allDoneFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: STANDBY_ACTIVE -> STANDBY_DONE [OK]\n");
	    SETSTAT( "STBY", S_stby, S_knl_ok, "OK" );
	} state STANDBY_DONE

	/*
	 * Timeout on standby. Ignore and enter STANDBY state
	 */
	when ( delay( tcsStbyTimeout ) ) {
	    LOGMSG( LOG_MAIN,"tcsMain: STANDBY_ACTIVE -> STANDBY_DONE [t/o]\n");
	    SETSTAT( "STBY", S_stby, S_knl_timeout, "standby timeout" );
	} state STANDBY_DONE
    }

    /*
     * STANDBY_DONE state. Clear all flags and enter STANDBY state (need
     * to clear all because STANDBY may have been used to abort any command)
     */
    state STANDBY_DONE {
	when () {
	    LOGMSG(LOG_MAIN,"tcsMain: STANDBY_DONE -> STANDBY [OK]\n");
	    /* SETSTAT() called above */
	    EFPVCLEAR( initFlag, initActive );
	    EFPVCLEAR( haltFlag, haltActive );
	    EFPVCLEAR( stbyFlag, stbyActive );
	    EFPVCLEAR( bypassFlag, bypassActive );
	} state STANDBY
    }

    /*
     * STANDBY state. Awaiting a command
     */
    state STANDBY {

	/*
	 * Fault detected. Set fault flag and enter FAULT_ACK state.
	 */
	when ( efTest( faultDetectedFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: STANDBY -> FAULT_ACK [fault]\n" );
	    SETSTAT( "FAULT", S_fault, S_knl_fault, "faulted" );
	} state FAULT_ACK

	/*
	 * Instrument set and read of configuration files requested.
	 * Refer to notes to INIT_START state
	 */
	when ( efTestAndClear( readConfigFlag ) ) {
	    SETSTAT( "STBY", S_stby, S_knl_ok, "reading config files" );
	    if ( tcsReadConfigFiles(
			tel,			/* telescope name */
			secModuleId,		/* secondary module */
			casModuleId,		/* cassegrain module */
			version,		/* software version */
			instrument,		/* instrument */
			focalstation ) < 0 ) {	/* focal station */
		LOGMSG( LOG_MAIN,
			"tcsMain: STANDBY -> STANDBY [config?]\n" );
		SETSTAT( "STBY", S_stby, S_knl_fault, "sec id? (missing file)" );
	    }
	    else {
		LOGMSG( LOG_MAIN, "tcsMain: STANDBY -> STANDBY [OK]\n" );
		SETSTAT( "STBY", S_stby, S_knl_ok, "OK" );

		/* Set the "readback" instrument and focal station  */
		strcpy( instrumentRbv, instrument );
		pvPut( instrumentRbv );
		strcpy( focalstationRbv, focalstation );
		pvPut( focalstationRbv );
	    }
	} state STANDBY

	/*
	 * STANDBY command received. Enter STANDBY_SEND state
	 */
	when ( efTest( stbyFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: STANDBY -> STANDBY_SEND [stby]\n" );
	    SETSTAT( "STBY", S_stby, S_knl_ok, "OK" );
	} state STANDBY_SEND

	/*
	 * HALT command received. No op
	 */
	when ( efTest( haltFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: STANDBY -> STANDBY [halt]\n" );
	    EFPVCLEAR( haltFlag, haltActive );
	} state STANDBY

	/*
	 * BYPASS command received and already been initialized. Enter
	 * INIT_START state
	 */
	when ( efTest( bypassFlag ) && alreadyInit ) {
	    LOGMSG( LOG_MAIN, "tcsMain: STANDBY -> INIT_START [bypass]\n" );
	    SETSTAT( "BPASS", S_init, S_knl_ok, "OK" );
	    initState = "STANDBY";
	} state INIT_START

	/*
	 * BYPASS (if failed the above) or INIT command received. Enter
	 * INIT_START state
	 */
	when ( efTest( bypassFlag) || efTest( initFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: STANDBY -> INIT_START [init]\n" );
	    SETSTAT( "INIT", S_init, S_knl_ok, "OK" );
	    initState = "STANDBY";
	} state INIT_START
    }

    /*
     * INIT_START state. Check instrument and focal station are achievable
     * and identify needed subsystems
     */
    state INIT_START {

	/*
	 * Determine whether the instrument and focal station are
	 * achievable in the current hardware configuration. If not,
	 * and if BYPASS is not active, enter INIT_FAIL state (so
	 * BYPASS ignores "not achievable" errors)
	 */
	when ( !efTest( bypassFlag ) && !tcsInstrumentAchievable(
		        tel,			/* telescope name */
			secModuleId,		/* secondary module */
			casModuleId,		/* cassegrain module */
			fwdModuleId,		/* forward cass module */
			instrument,		/* instrument */
			focalstation ) ) {	/* focal station */
	    LOGMSG( LOG_MAIN, "tcsMain: INIT_START -> STANDBY [achievable?]\n");
	    SETSTAT( "STBY", S_stby, S_knl_fault, "wrong config for instr" );

	    /* pretend never initialized so bypass will start from scratch */
	    alreadyInit = FALSE;
	} state INIT_FAIL

	/*
	 * Otherwise, determine which subsystems are needed for the
	 * instrument and focal station. If there's a problem,
	 * enter INIT_FAIL state
	 */
	when ( ( nSubsys = tcsSubsystemsNeeded(
		    	secModuleId,		/* secondary module */
		    	casModuleId,		/* cassegrain module */
		    	fwdModuleId,		/* forward cass module */
			instrument,		/* instrument */
			focalstation,		/* focal station */
			MAXSUBSYS,		/* max #subsystems */
			tcsNeeded ) ) < 0 ) {	/* "needed" array */
	    LOGMSG( LOG_MAIN, "tcsMain: INIT_START -> STANDBY [needed?]\n" );
	    SETSTAT( "STBY", S_stby, S_knl_fault, "can't determine subsystems");
	} state INIT_FAIL

	/*
	 * Otherwise, if the instrument has changed, read the instrument
	 * configuration files (using pvload, which uses CA to set initially
	 * configured values but should _not_ actually control any hardware).
	 * This should not be done from an event function (i.e. in a when()
	 * clause because delivery of CA events to this sequence is disabled
	 * then). If there's a problem, report it but continue (the system is
	 * compromised but not completely so). If not, set the instrument and
	 * focal station
	 */
	when ( efTestAndClear( readConfigFlag ) ) {
	    SETSTAT( "INIT", S_init, S_knl_ok, "reading config files" );
	    if ( tcsReadConfigFiles(
			tel,			/* telescope name */
			secModuleId,		/* secondary module */
			casModuleId,		/* cassegrain module */
			version,		/* software version */
			instrument,		/* instrument */
			focalstation ) < 0 ) {	/* focal station */
		LOGMSG( LOG_MAIN,
			"tcsMain: INIT_START -> INIT_DELAY [config?]\n" );
		SETSTAT( "INIT", S_init, S_knl_fault, "sec id? (missing file)");
	    }
	    else {
		LOGMSG( LOG_MAIN, "tcsMain: INIT_START -> INIT_DELAY [OK]\n" );
		SETSTAT( "INIT", S_init, S_knl_ok, "OK" );

		/* Set the "readback" instrument and focal station  */
		strcpy( instrumentRbv, instrument );
		pvPut( instrumentRbv );
		strcpy( focalstationRbv, focalstation );
		pvPut( focalstationRbv );
	    }
	} state INIT_DELAY

	/*
	 * Otherwise, configuration files do not need to be read again
	 * (because the instrument has not changed)
	 */
	when () {
	    LOGMSG( LOG_MAIN, "tcsMain: INIT_START -> INIT_DELAY [OK]\n" );
	    SETSTAT( "INIT", S_init, S_knl_ok, "OK" );

	    /* Note that an init command has been received (used to
	       control DCS GUI row visibility) */
	    PVSET( initDone );
	} state INIT_DELAY
    }

    /*
     * INIT_DELAY state. This exists just to delay before proceeding. This is
     * necessary because of a suspected sequencer bug that surfaces when
     * connections are being made at the same time that events are occurring
     */
    state INIT_DELAY {

	/*
	 * Make / break connections between sequencer and EPICS variables.
	 * Enter INIT_CONNECT state awaiting connections
	 */
	when ( delay( 5.0 ) ) {
	    sprintf( buff, "tcsMain: INIT_DELAY -> INIT_CONNECT [nSubsys=%ld]\n", nSubsys);
	    LOGMSG( LOG_MAIN, buff );
	    SETSTAT( "INIT", S_init, S_knl_ok, "OK" );

	    /* Make / break connections with EPICS variables */
	    for ( i = 0; i < nSubsys; i++ ) {
		pvAssign( tcsNeeded[i],
			      tcsCmdVar( tel, i, "needed" ) );
		if ( tcsNeeded[i] ) {
		    pvAssign( tcsInit[i],
			      tcsCmdVar( tel, i, "init" ) );
		    pvAssign( tcsHalt[i],
			      tcsCmdVar( tel, i, "halt" ) );
		    pvAssign( tcsStby[i],
			      tcsCmdVar( tel, i, "stby" ) );
		    pvAssign( tcsBypass[i],
			      tcsCmdVar( tel, i, "bypass" ) );

		    pvAssign( tcsInitActive[i],
			      tcsCmdVar( tel, i, "initActive" ) );
		    pvAssign( tcsHaltActive[i],
			      tcsCmdVar( tel, i, "haltActive" ) );
		    pvAssign( tcsStbyActive[i],
			      tcsCmdVar( tel, i, "stbyActive" ) );
		    pvAssign( tcsBypassActive[i],
			      tcsCmdVar( tel, i, "bypassActive" ) );

		    pvAssign( tcsStatus[i],
			      tcsCmdVar( tel, i, "status" ) );
		    pvAssign( tcsStatusMon[i],
			      tcsCmdVar( tel, i, "status" ) ); /* same var */
		    pvAssign( tcsState[i],
			      tcsCmdVar( tel, i, "state" ) );
		    pvAssign( tcsStateVal[i],
			      tcsCmdVar( tel, i, "stateVal" ) );
		}
		else {
		    pvAssign( tcsInit[i], "" );
		    pvAssign( tcsHalt[i], "" );
		    pvAssign( tcsStby[i], "" );
		    pvAssign( tcsBypass[i], "" );

		    pvAssign( tcsInitActive[i], "" );
		    pvAssign( tcsHaltActive[i], "" );
		    pvAssign( tcsStbyActive[i], "" );
		    pvAssign( tcsBypassActive[i], "" );

		    pvAssign( tcsStatus[i], "" );
		    pvAssign( tcsStatusMon[i], "" );
		    pvAssign( tcsState[i], "" );
		    pvAssign( tcsStateVal[i], "" );
		}
	    }
	} state INIT_CONNECT
    }

    /*
     * INIT_CONNECT. Wait until all process variables have connected. Enter
     * INIT_SEND state
     */
    state INIT_CONNECT {

	/*
	 * STANDBY command received. Enter STANDBY_SEND state
	 */
	when ( efTest( stbyFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: INIT_CONNECT -> STANDBY_SEND [stby]\n");
	    SETSTAT( "STBY", S_stby, S_knl_ok, "OK" );
	} state STANDBY_SEND

	/*
	 * HALT command received. Enter HALT_SEND state
	 */
	when ( efTest( haltFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: INIT_CONNECT -> HALT_SEND [halt]\n" );
	    SETSTAT( "HALTG", S_init, S_knl_ok, "OK" );
	} state HALT_SEND

	/*
	 * All EPICS process variables have connected. Enter INIT_SEND state
	 */
	when ( pvConnectCount() == pvAssignCount() ) {
	    LOGMSG( LOG_MAIN, "tcsMain: INIT_CONNECT -> INIT_SEND [OK]\n" );
	    SETSTAT( "INIT", S_init, S_knl_ok, "OK" );

	    /* Set all the "needed" variables (used by GUIs) */
	    for ( i = 0; i < nSubsys; i++ ) {
		pvPut( tcsNeeded[i] );
	    }

	    /* Variables have connected but can't be sure "initial value"
	       monitor has been received on the "active" variables so
	       read them synchronously (messy) */
	    for ( i = 0; i < nSubsys; i++ ) {
		pvGet( tcsInitActive[i] );
		pvGet( tcsHaltActive[i] );
		pvGet( tcsStbyActive[i] );
		pvGet( tcsBypassActive[i] );
		pvGet( tcsStatus[i] );
	    }
	} state INIT_SEND

	/*
	 * 30 seconds has passed and still not all EPICS process
	 * variables have connected. Continue waiting
	 *
	 * Should add finer control / reporting here
	 */
	when ( delay( 30 ) ) {
	    LOGMSG(LOG_DEBUG, "tcsMain: INIT_CONNECT -> INIT_CONNECT [t/o]\n" );
	    SETSTAT( "INIT", S_stby, S_knl_timeout, "connect timeout" );
	} state INIT_CONNECT
    }

    /*
     * INIT_SEND state. Request all subsystems to bypass initialization
     * or to initialize, depending on whether the command was BYPASS or not
     */
    state INIT_SEND {

	/*
	 * BYPASS command is active. Request all subsystems to bypass
	 * initialization and enter BYPASS_ACTIVE state
	 */
	when ( efTest( bypassFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: INIT_SEND -> BYPASS_ACTIVE [OK]\n" );
	    SETSTAT( "BPASS", S_init, S_knl_ok, "OK" );

	    /* Ensure syncQ queues are empty */
	    pvFreeQ( tcsBypassActive );
	    pvFreeQ( tcsStatus );

	    /* Ensure event flags are clear */
	    efClear( allDoneFlag );
	    efClear( anyFailFlag );

	    /* Request all subsystems to bypass initialization */
	    tcsResetActive( nSubsys, tcsNeeded, tcsBypassActive, active );
	    for ( i = 0; i < nSubsys; i++ ) {
		PVSET( tcsBypass[i] );
	    }
	} state BYPASS_ACTIVE

	/*
	 * Other command is active (should be INIT). Initiate initialization
	 * and enter INIT_ACTIVE state
	 */
	when () {
	    LOGMSG( LOG_MAIN, "tcsMain: INIT_SEND -> INIT_ACTIVE [OK]\n" );
	    SETSTAT( "INIT", S_init, S_knl_ok, "OK" );

	    /* Ensure syncQ queues are empty */
	    pvFreeQ( tcsInitActive );
	    pvFreeQ( tcsStatus );

	    /* Ensure event flags are clear */
	    efClear( allDoneFlag );
	    efClear( anyFailFlag );

	    /* Initiate initialization of all subsystems */
	    tcsResetActive( nSubsys, tcsNeeded, tcsInitActive, active );
	    for ( i = 0; i < nSubsys; i++ ) {
		PVSET( tcsInit[i] );
	    }
	} state INIT_ACTIVE
    }

    /*
     * BYPASS_ACTIVE state. Wait until all subsystems are initialized. Then
     * enter INIT_DONE state
     */
    state BYPASS_ACTIVE {

	/*
	 * Fault detected. Set fault flag and enter FAULT_ACK state.
	 */
	when ( efTest( faultDetectedFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: BYPASS_ACTIVE -> FAULT_ACK [fault]\n" );
	    SETSTAT( "FAULT", S_fault, S_knl_fault, "faulted" );
	} state FAULT_ACK

	/*
	 * STANDBY command received. Enter STANDBY_SEND state
	 */
	when ( efTest( stbyFlag ) ) {
	    LOGMSG( LOG_MAIN,"tcsMain: BYPASS_ACTIVE -> STANDBY_SEND [stby]\n");
	    SETSTAT( "STBY", S_stby, S_knl_ok, "OK" );
	} state STANDBY_SEND

	/*
	 * HALT command received. Enter HALT_SEND state
	 */
	when ( efTest( haltFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: BYPASS_ACTIVE -> HALT_SEND [halt]\n" );
	    SETSTAT( "HALTG", S_init, S_knl_ok, "OK" );
	} state HALT_SEND

	/*
	 * A "bypass active" variable has changed value
	 */
	when ( pvGetQ( tcsBypassActive ) || pvGetQ( tcsStatus ) ) {

	    /* Check for completion */
	    tcsUpdateActive( nSubsys, tcsBypassActive, tcsStatus,
			     active, &allDone, &anyFail );

	    /* If all subsystems are done, set the "all done" event flag
	       and, if any command failed, the "any failed" event flag */
	    if ( allDone ) {
		efSet( allDoneFlag );
		if ( anyFail ) efSet( anyFailFlag );
	    }
	} state BYPASS_ACTIVE

	/*
	 * All bypass commands are done and at least one failed. For now,
	 * report this but continue with initialization (allow the subsystems
	 * to recover if they can)
	 */
	when ( efTestAndClear( anyFailFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: BYPASS_ACTIVE -> BYPASS_ACTIVE [ig]\n");
	} state BYPASS_ACTIVE

	/*
	 * All bypass commands are done. Enter INIT_DONE state
	 */
	when ( efTestAndClear( allDoneFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: BYPASS_ACTIVE -> INIT_DONE [OK]\n" );
	    SETSTAT( "INIT", S_init, S_knl_ok, "OK" );
	} state INIT_DONE

	/*
	 * Timeout on bypass. Enter INIT_FAIL state.
	 */
	when ( delay( tcsBypassTimeout ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: BYPASS_ACTIVE -> STANDBY [t/o]\n" );
	    SETSTAT( "STBY", S_stby, S_knl_timeout, "bypass timeout" );
	} state INIT_FAIL
    }

    /*
     * INIT_ACTIVE state. Wait until all subsystems are initialized.
     * Enter INIT_DONE state
     */
    state INIT_ACTIVE {

	/*
	 * Fault detected. Set fault flag and enter FAULT_ACK state.
	 */
	when ( efTest( faultDetectedFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: INIT_ACTIVE -> FAULT_ACK [fault]\n" );
	    SETSTAT( "FAULT", S_fault, S_knl_fault, "faulted" );
	} state FAULT_ACK

	/*
	 * STANDBY command received. Enter STANDBY_SEND state
	 */
	when ( efTest( stbyFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: INIT_ACTIVE -> STANDBY_SEND [stby]\n" );
	    SETSTAT( "STBY", S_stby, S_knl_ok, "OK" );
	} state STANDBY_SEND

	/*
	 * HALT command received. Enter HALT_SEND state
	 */
	when ( efTest( haltFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: INIT_ACTIVE -> HALT_SEND [halt]\n" );
	    SETSTAT( "HALTG", S_init, S_knl_ok, "OK" );
	} state HALT_SEND

	/*
	 * An "init active" variable has changed value
	 */
	when ( pvGetQ( tcsInitActive ) || pvGetQ( tcsStatus ) ) {

	    /* Check for completion */
	    tcsUpdateActive( nSubsys, tcsInitActive, tcsStatus,
			     active, &allDone, &anyFail );

	    /* If all subsystems are done, set the "all done" event flag
	       and, if any command failed, the "any failed" event flag */
	    if ( allDone ) {
		efSet( allDoneFlag );
		if ( anyFail ) efSet( anyFailFlag );
	    }
	} state INIT_ACTIVE

	/*
	 * All initialization commands are done and at least one failed. For
	 * now, report this but continue with initialization (allow the
	 * subsystems to recover if they can)
	 */
	when ( efTestAndClear( anyFailFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: INIT_ACTIVE -> INIT_ACTIVE [ig]\n" );
	} state INIT_ACTIVE

	/*
	 * All initialization commands are done. Enter INIT_DONE state
	 */
	when ( efTestAndClear( allDoneFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: INIT_ACTIVE -> INIT_DONE [OK]\n" );
	    SETSTAT( "INIT", S_init, S_knl_ok, "OK" );
	} state INIT_DONE

	/*
	 * Timeout on initialization. Enter INIT_FAIL state
	 */
	when ( delay( tcsInitTimeout ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: INIT_ACTIVE -> STANDBY [t/o]\n" );
	    SETSTAT( "STBY", S_stby, S_knl_timeout, "init timeout" );
	} state INIT_FAIL
    }

    /*
     * INIT_DONE state. Enable fault checking and enter HALTED state
     */
    state INIT_DONE {

	when () {
	    LOGMSG( LOG_MAIN, "tcsMain: INIT_DONE -> HALTED [OK]\n" );
	    SETSTAT( "RUN", S_halt, S_knl_ok, "OK" );

	    /* Note already initialized */
	    alreadyInit = TRUE;

	    /* Note that initialization and / or bypass are complete */
	    EFPVCLEAR( initFlag, initActive );
	    EFPVCLEAR( bypassFlag, bypassActive );
	} state HALTED
    }

    /*
     * INIT_FAIL state. Clear flags and enter the state from which the
     * initialization command was issued
     */
    state INIT_FAIL {
	when ( strcmp( initState, "STANDBY" ) == 0 ) {
	    EFPVCLEAR( initFlag, initActive );
	    EFPVCLEAR( bypassFlag, bypassActive );
	} state STANDBY

	when ( strcmp( initState, "HALTED" ) == 0 ) {
	    EFPVCLEAR( initFlag, initActive );
	    EFPVCLEAR( bypassFlag, bypassActive );
	} state HALTED

	when ( strcmp( initState, "FAULTED" ) == 0 ) {
	    EFPVCLEAR( initFlag, initActive );
	    EFPVCLEAR( bypassFlag, bypassActive );
	} state FAULTED
    }

/*+*****************************************************************************
 Note:	For the remaining "main" states, the system is initialized and
	fault checking is enabled (exception: halting during initialization)
*******************************************************************************/

    /*
     * HALT_SEND state. Initiate halting of all subsystems. Enter HALT_ACTIVE
     * state
     */
    state HALT_SEND {

	when () {
	    LOGMSG( LOG_MAIN, "tcsMain: HALT_SEND -> HALT_ACTIVE [OK]\n" );
	    SETSTAT( "HALTG", S_halt, S_knl_ok, "OK" );

	    /* Ensure syncQ queues are empty */
	    pvFreeQ( tcsHaltActive );
	    pvFreeQ( tcsStatus );

	    /* Ensure event flags are clear */
	    efClear( allDoneFlag );
	    efClear( anyFailFlag );

	    /* Initiate halting of all subsystems */
	    tcsResetActive( nSubsys, tcsNeeded, tcsHaltActive, active );
	    for ( i = 0; i < nSubsys; i++ ) {
		PVSET( tcsHalt[i] );
	    }
	} state HALT_ACTIVE
    }

    /*
     * HALT_ACTIVE state. Wait until all subsystems are halted. Enter
     * HALT_DONE state
     */
    state HALT_ACTIVE {

	/*
	 * Fault detected. Set fault flag and enter FAULT_ACK state.
	 */
	when ( efTest( faultDetectedFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: HALT_ACTIVE -> FAULT_ACK [fault]\n" );
	    SETSTAT( "FAULT", S_fault, S_knl_fault, "faulted" );
	} state FAULT_ACK

	/*
	 * STANDBY command received. Enter STANDBY_SEND state
	 */
	when ( efTest( stbyFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: HALT_ACTIVE -> STANDBY_SEND [stby]\n" );
	    SETSTAT( "STBY", S_stby, S_knl_ok, "OK" );
	} state STANDBY_SEND

	/*
	 * A "halt active" variable has changed value
	 */
	when ( pvGetQ( tcsHaltActive ) || pvGetQ( tcsStatus ) ) {

	    /* Check for completion */
	    tcsUpdateActive( nSubsys, tcsHaltActive, tcsStatus,
			     active, &allDone, &anyFail );

	    /* If all subsystems are done, set the "all done" event flag
	       and, if any command failed, the "any failed" event flag */
	    if ( allDone ) {
		efSet( allDoneFlag );
		if ( anyFail ) efSet( anyFailFlag );
	    }
	} state HALT_ACTIVE

	/*
	 * All halt commands are done and at least one failed. For
	 * now, report this but continue with halting (allow the
	 * subsystems to recover if they can)
	 */
	when ( efTestAndClear( anyFailFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: HALT_ACTIVE -> HALT_ACTIVE [ig]\n" );
	} state HALT_ACTIVE

	/*
	 * All halt commands are done. Enter HALT_DONE state
	 */
	when ( efTestAndClear( allDoneFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: HALT_ACTIVE -> HALT_DONE [OK]\n" );
	    SETSTAT( "RUN", S_halt, S_knl_ok, "OK" );
	} state HALT_DONE

	/*
	 * Timeout on halting. For now, ignore and enter HALT_DONE state
	 * anyway
	 */
	when ( delay( tcsHaltTimeout ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: HALT_ACTIVE -> HALT_DONE [t/o]\n" );
	    SETSTAT( "RUN", S_halt, S_knl_timeout, "halt timeout" );
	} state HALT_DONE
    }

    /*
     * In the HALT_DONE state, if BYPASS or INIT is active, HALT was
     * issued while initializing, so enter STANDBY state. Otherwise
     * proceed to HALTED state
     */
    state HALT_DONE {

	/*
	 * BYPASS or INIT is active. Clear flags and enter STANDBY state
	 */
	when ( efTest( bypassFlag ) || efTest( initFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: HALT_DONE -> STANDBY [OK]\n" );
	    SETSTAT( "STBY", S_stby, S_knl_ok, "OK" );
	    EFPVCLEAR( haltFlag, haltActive );
	    EFPVCLEAR( initFlag, initActive   );
	    EFPVCLEAR( bypassFlag, bypassActive );
	} state STANDBY

	/*
	 * Otherwise, clear flag and enter HALTED state
	 */
	when () {
	    LOGMSG( LOG_MAIN, "tcsMain: HALT_DONE -> HALTED [OK]\n" );
	    /* SETSTAT() called above */
	    EFPVCLEAR( haltFlag, haltActive );
	} state HALTED
    }

    /*
     * In the HALTED state, all subsystems are halted
     */
    state HALTED {

	/*
	 * Fault detected. Set fault flag and enter FAULT_ACK state.
	 */
	when ( efTest( faultDetectedFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: HALTED -> FAULT_ACK [fault]\n" );
	    SETSTAT( "FAULT", S_fault, S_knl_fault, "faulted" );
	} state FAULT_ACK

	/*
	 * Instrument set and read of configuration files requested.
	 * Refer to notes to INIT_START state
	 */
	when ( efTestAndClear( readConfigFlag ) ) {
	    SETSTAT( "RUN", S_halt, S_knl_ok, "reading config files" );
	    if ( tcsReadConfigFiles(
			tel,			/* telescope name */
			secModuleId,		/* secondary module */
			casModuleId,		/* cassegrain module */
			version,		/* software version */
			instrument,		/* instrument */
			focalstation ) < 0 ) {	/* focal station */
		LOGMSG( LOG_MAIN,
			"tcsMain: HALTED -> HALTED [config?]\n" );
		SETSTAT( "RUN", S_halt, S_knl_fault, "sec id? (missing file)" );
	    }
	    else {
		LOGMSG( LOG_MAIN, "tcsMain: HALTED -> HALTED [OK]\n" );
		SETSTAT( "RUN", S_halt, S_knl_ok, "OK" );

		/* Set the "readback" instrument and focal station  */
		strcpy( instrumentRbv, instrument );
		pvPut( instrumentRbv );
		strcpy( focalstationRbv, focalstation );
		pvPut( focalstationRbv );
    	    }
	} state HALTED

	/*
	 * HALT command received. Enter HALT_SEND state
	 */
	when ( efTest( haltFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: HALTED -> HALT_SEND [halt]\n" );
	    SETSTAT( "HALTG", S_halt, S_knl_ok, "OK" );
	} state HALT_SEND

	/*
	 * STANDBY command received. Enter STANDBY_SEND state
	 */
	when ( efTest( stbyFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: HALTED -> STANDBY_SEND [stby]\n" );
	    SETSTAT( "STBY", S_stby, S_knl_ok, "OK" );
	} state STANDBY_SEND

	/*
	 * BYPASS command received. Enter INIT_START state
	 */
	when ( efTest( bypassFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: HALTED -> INIT_START [bypass]\n" );
	    SETSTAT( "BPASS", S_init, S_knl_ok, "OK" );
	    initState = "HALTED";
	} state INIT_START

	/*
	 * INIT command received. Enter INIT_START state
	 */
	when ( efTest( initFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: HALTED -> INIT_START [init]\n" );
	    SETSTAT( "INIT", S_init, S_knl_ok, "OK" );
	    initState = "HALTED";
	} state INIT_START

	/*
	 * Check for changes in system state based on monitoring of
	 * subsystem variables. Don't actually change state from HALTED
 	 * but do change the "state" and "stateVal" variables
	 */
	when ( efTestAndClear( tcsStateFlag ) ||
					   efTestAndClear( tcsStateValFlag ) ) {
	    tcsStateChanged(
			nSubsys,		/* #subsystems */
			tcsNeeded,		/* "needed" array */
			tcsState,		/* subsystem states */
			tcsStateVal,		/* subsystem state values */
			&newState,		/* new state string */
			&newStateVal );		/* new state value */
#if FALSE   /* suppressed at KTT's request! */
	    LOGMSG( LOG_MAIN, "tcsMain: HALTED -> HALTED [state]\n" );
#endif
	    SETSTAT( newState, newStateVal, S_knl_ok, "OK" );
	} state HALTED
    }

    /*
     * In the FAULT_ACK state, acknowledge the fault and disable fault
     * detection. Enter FAULT_RESET state
     */
    state FAULT_ACK {
	when () {
	    LOGMSG( LOG_MAIN, "tcsMain: FAULT_ACK -> FAULT_RESET [acked]\n" );
	    efSet( faultAckFlag );
	    efClear( faultEnabledFlag );
	    SETSTAT( "FAULT", S_fault, S_knl_fault, "faulted" );
	} state FAULT_RESET
    }

    /*
     * In the FAULT_RESET state, once the fault sequence acknowledges,
     * clear all command variables and enter FAULTED state (10sec timeout)
     */
    state FAULT_RESET {
	when ( !efTest( faultDetectedFlag ) || delay( 10.0 ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: FAULT_RESET -> FAULTED [ack acked]\n" );
	    EFPVCLEAR( initFlag, initActive );
	    EFPVCLEAR( haltFlag, haltActive );
	    EFPVCLEAR( stbyFlag, stbyActive );
	    EFPVCLEAR( bypassFlag, bypassActive );
	    SETSTAT( "FAULT", S_fault, S_knl_fault, "faulted" );
	} state FAULTED
    }

    /*
     * In the FAULTED state, can only issue STANDBY, BYPASS (?) or INIT
     */
    state FAULTED {

	/*
	 * Reason for fault was PV disconnect. Enter START_CONNECT state (i.e.
	 * complete soft restart of the sequence)
	 */
	when ( efTest( pvDisconnectedFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: FAULTED -> START_CONNECT [disconnect]\n" );
	    SETSTAT( "FAULT", S_fault, S_knl_fault, "(re-)connecting" );
	} state START_CONNECT

	/*
	 * HALT command received. Clear flag and don't change state
	 */
	when ( efTest( haltFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: FAULTED -> FAULTED [halt]\n" );
	    EFPVCLEAR( haltFlag, haltActive );
	} state FAULTED

	/*
	 * STANDBY command received. Enter STANDBY_SEND state
	 */
	when ( efTest( stbyFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: FAULTED -> STANDBY_SEND [stby]\n" );
	    SETSTAT( "STBY", S_stby, S_knl_ok, "OK" );
	} state STANDBY_SEND

	/*
	 * BYPASS command received. Enter INIT_START state
	 */
	when ( efTest( bypassFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: FAULTED -> INIT_START [bypass]\n" );
	    SETSTAT( "BPASS", S_init, S_knl_ok, "OK" );
	    initState = "FAULTED";
	} state INIT_START

	/*
	 * INIT command received. Enter INIT_START state
	 */
	when ( efTest( initFlag ) ) {
	    LOGMSG( LOG_MAIN, "tcsMain: FAULTED -> INIT_START [init]\n" );
	    SETSTAT( "INIT", S_init, S_knl_ok, "OK" );
	    initState = "FAULTED";
	} state INIT_START
    }
}

/*+*****************************************************************************
 Function:	tcsFault

 Author:	W Lupton

 Abstract:	TCS fault sequence

 Description:	This sequence is responsible for monitoring the underlying
		subsystem for faults. Two classes of faults are supported:

		a) s/w faults are currently only process variable disconnects.
		b) h/w faults can be triggered by any change of state of any
		   database field

		The sequence is controlled by the main sequence. Initially,
		fault checking is disabled. It is enabled when the main
		sequence sets the faultEnabledFlag event flag.

		When a fault is detected, the fault sequence sets the
		faultDetectedFlag event flag and expects the main sequence
		to react by setting the faultAckFlag event flag, meaning "I
		have noted the fault". At this point, the fault sequence
		clears the faultDetectedFlag event flag and resumes checking
		(if checking is still enabled, which it normally won't
		be).

		The normal behavior of the main sequence, once the faultAckFlag
		exchange is complete, will be to halt the subsystem and return
		to the IDLE state. Once the underlying reason for the fault
		has been determined and the fault rectified, a soft initialize
		will return the system to the HALTED state.

 Bugs:		Handling of s/w faults is not well thought out. The faultAckFlag
		protocol is not honored in this case and probably should be.
		The rationale for separate treatment of s/w and h/w faults
		should be re-evaluated.

		The nature of the fault is not communicated back to the
		main sequence. Real subsystems will probably use variables
		for this purpose.
*******************************************************************************/

ss tcsFault {

    /*
     * All sequences start off in their first state
     */
    state START {

	/*
	 * When main sequence enables fault checking, enter CHECKING state.
	 */
	when ( efTest( faultEnabledFlag ) ) {
	    LOGMSG( LOG_FAULT, "tcsFault: START -> CHECKING [fault enabled]\n");
	    efClear( faultDetectedFlag );
	    efClear( faultAckFlag );
	} state CHECKING
    }

    /*
     * In the CHECKING state, EPICS process variable disconnect or hardware
     * faults are detected
     */
    state CHECKING {

	/*
	 * Fault checking has been disabled. Enter START state
	 */
	when ( ! efTest( faultEnabledFlag ) ) {
	    LOGMSG( LOG_FAULT, "tcsFault: CHECKING -> START [disable]\n" );
	} state START

	/*
	 * Process variable has disconnected. Set "PV disconnected" and "fault detected"
	 * flags and enter FAULTED state. Ignore when initializing or bypassing
	 * because channels are re-assigned then so could get a false fault
	 */
	when ( !efTest( initFlag ) && !efTest( bypassFlag ) &&
				      pvConnectCount() != pvAssignCount() ) {
	    LOGMSG( LOG_FAULT, "tcsFault: CHECKING -> FAULTED [disconnect]\n");
	    efSet( pvDisconnectedFlag );
	    efSet( faultDetectedFlag );
	} state FAULTED

	/*
	 * Reschedule every 5 seconds because state-set is not woken up when the
	 * connect count or assign count changes
	 */
 	when ( delay( 5.0 ) ) {
	} state CHECKING
    }

    /*
     * In the FAULTED state, we await either acknowledgment of the fault
     * or else disable of fault checking
     */
    state FAULTED {

	/*
	 * Fault has been acknowledged. Clear detected flag and enter
	 * CHECKING state
	 */
	when ( efTestAndClear( faultAckFlag ) ) {
	    LOGMSG( LOG_FAULT, "tcsFault: FAULTED -> FAULTED [ack]\n" );
	    efClear( faultDetectedFlag );
	} state FAULTED

	/*
	 * Fault checking has been disabled. Enter START state
	 */
	when ( ! efTest( faultEnabledFlag ) ) {
	    LOGMSG( LOG_FAULT, "tcsFault: FAULTED -> START [disable]\n" );
	} state START
    }
}

/*+*****************************************************************************
 Function:	tcsCommand

 Author:	W Lupton

 Abstract:	TCS command sequence

 Description:   This very simple sequence is enabled and disabled by the
		main sequence by setting and clearing the
		commandEnabledFlag event flag. When enabled, it checks
		for incoming commands and sets event flags which wake up
		the main sequence and cause command processing.

		The handling of most commands is identical and is
		illustrated by the init command:

		when( init ) {
	    	    LOGMSG( LOG_COMMAND, "tcsCommand: IDLE -> IDLE [init]\n" );
	    	    EFPVSET( initFlag, initActive );
	    	    PVCLEAR( init );
		} state IDLE

		An init command is sent by setting the EPICS PV
		associated with init to 1. This causes the above code to
		execute and the initFlag event flag and the associated
		initActive EPICS PV are set to 1. Then init is set back
		to 0, both to acknowledge to the caller and to prevent
		the above code from immediately re-executing.

		The initFlag event flag and the initActive variable stay
		set to 1 until the init action is complete, at which
		point they are set to 0. Clients which care can monitor
		the command and active variables. If command queueing is
		required, it must be implemented in database processing
		between clients and sequences.

		The command sequence also handles various "immediate action"
		commands, including those necessary to handle pointing
		origins.
*******************************************************************************/

ss tcsCommand {

    /*
     * All sequences start off in their first state
     */
    state START {

	/*
	 * Command processing enabled. Enter IDLE state
	 */
	when ( efTest( commandEnabledFlag ) ) {
	    LOGMSG( LOG_COMMAND, "tcsCommand: START -> IDLE [enable]\n" );
	} state IDLE
    }

    /*
     * In the IDLE state, commands are accepted. Internal commands (e.g.
     * setting logging levels) are handled immediately. Standard commands are
     * handled by the main sequence. Currently, these are all binary outputs
     * which cause event flags to be set and are then reset. Pointing origin
     * commands are handled immediately
     */
    state IDLE {

	/*
	 * Command processing disabled. Enter START state
	 */
	when ( ! efTest( commandEnabledFlag ) ) {
	    LOGMSG( LOG_COMMAND, "tcsCommand: IDLE -> START [disable]\n" );
	} state START

	/*
	 * Internal simulateLevel and logLevel commands are handled directly
	 */
	when ( efTestAndClear( simulateLevelFlag ) ) {
	    LOGMSG( LOG_COMMAND, "tcsCommand: IDLE -> IDLE [simulate]\n" );
	} state IDLE

	when ( efTestAndClear( logLevelFlag ) ) {
	    LOGMSG( LOG_COMMAND, "tcsCommand: IDLE -> IDLE [logLevel]\n" );
	} state IDLE

	when ( logLevelSet ) {
	    LOGMSG( LOG_COMMAND, "tcsCommand: IDLE -> IDLE [logLevelSet]\n" );
	    logLevel |= logLevelSet;
	    PVCLEAR( logLevelSet );
	} state IDLE

	when ( logLevelClr ) {
	    LOGMSG( LOG_COMMAND, "tcsCommand: IDLE -> IDLE [logLevelClr]\n" );
	    logLevel &= ~logLevelClr;
	    PVCLEAR( logLevelClr );
	} state IDLE

	/*
	 * External commands are handled as described above
	 */
	when( init ) {
	    LOGMSG( LOG_COMMAND, "tcsCommand: IDLE -> IDLE [init]\n" );
	    EFPVSET( initFlag, initActive );
	    PVCLEAR( init );
	} state IDLE

	when( halt ) {
	    LOGMSG( LOG_COMMAND, "tcsCommand: IDLE -> IDLE [halt]\n" );
	    EFPVSET( haltFlag, haltActive );
	    PVCLEAR( halt );
	} state IDLE

	when( stby ) {
	    LOGMSG( LOG_COMMAND, "tcsCommand: IDLE -> IDLE [stby]\n" );
	    EFPVSET( stbyFlag, stbyActive );
	    PVCLEAR( stby );
	} state IDLE

	when( bypass ) {
	    LOGMSG( LOG_COMMAND, "tcsCommand: IDLE -> IDLE [bypass]\n" );
	    EFPVSET( bypassFlag, bypassActive );
	    PVCLEAR( bypass );
	} state IDLE

	/*
	 * Unfortunately it's not possible to get a monitor posted whenever a
	 * stringout record processes, so always clear the instrument PV when
	 * it's modified. Whenever the instrument is set, request that instru-
	 * ment configuration files be re-read. Check achievable just to set the
	 * focal station (if it can be determined from the instrument)
	 */
	when( efTestAndClear( instrumentFlag ) &&
			      strcmp( instrumentPV, "" ) != 0 ) {
	    strcpy( instrument, instrumentPV );
	    strcpy( instrumentPV, "" );
	    pvPut ( instrumentPV );
	    if ( tcsUnknown( instrument ) ) {
		LOGMSG( LOG_COMMAND, "tcsCommand: IDLE -> IDLE [instr?]\n" );
		efClear( readConfigFlag );
	    }
	    else {
		LOGMSG( LOG_COMMAND, "tcsCommand: IDLE -> IDLE [instr]\n" );
		efSet( readConfigFlag );
	    }
	    tcsInstrumentAchievable(
		        tel,			/* telescope name */
			secModuleId,		/* secondary module */
			casModuleId,		/* cassegrain module */
			fwdModuleId,		/* forward cass module */
			instrument,		/* instrument */
			focalstation );		/* focal station */
	} state IDLE

	/*
	 * The remaining commands handle pointing origins and all use synched
	 * event flags. Note that the command variables are not cleared back
	 * to zero, so their records should be ao's with MDEL < 0 (could
	 * revert to standard processing in fact) ???? TBD ????
	 */

	/*
 	 * Reset resets (undefines) all pointing origin definitions and tells
	 * the pointing code that the current pointing origin is at (0,0)
	 */
 	when( efTestAndClear( poResetFlag ) && poReset ) {
	    LOGMSG( LOG_COMMAND, "tcsCommand: IDLE -> IDLE [poReset]\n" );
	    tcsPtgorgReset();
	    PVCLEAR( pntPoXpos );
	    PVCLEAR( pntPoYpos );
	    /* PVCLEAR( poReset ); */
	} state PTGORG_UPDATE

	/*
	 * Preset associates a new position with a named pointing origin. It
	 * does nothing to the pointing _unless_ the pointing origin being
	 * preset is the current one, in which case it forwards the new
	 * position to the pointing code
	 */
 	when( efTestAndClear( poPresetFlag ) && poPreset ) {
#if FALSE
	    LOGMSG( LOG_COMMAND, "tcsCommand: IDLE -> IDLE [poPreset]\n" );
#endif
	    tcsPtgorgPreset( poName, poXpos, poYpos );
	    tcsPtgorgCurrent( poNameRbv, &poStatRbv, &pntPoXpos, &pntPoYpos );
	    if ( strcasecmp( poName, poNameRbv ) == 0 ) {
		pvPut( pntPoXpos );
		pvPut( pntPoYpos );
	    }
	    /* have to clear (poXpos,poYpos) in case used for an offset */
	    PVCLEAR( poXpos );
	    PVCLEAR( poYpos );
	    /* PVCLEAR( poPreset ); */
	} state PTGORG_UPDATE

	/*
	 * Select selects a named pointing origin and, if successful, forwards
	 * its (X,Y) to the pointing code
	 */
 	when( efTestAndClear( poSelectFlag ) && poSelect ) {
	    LOGMSG( LOG_COMMAND, "tcsCommand: IDLE -> IDLE [poSelect]\n" );
	    if ( tcsPtgorgSelect( poName ) == 0 ) {
		tcsPtgorgCurrent( poNameRbv, &poStatRbv,
				  &pntPoXpos, &pntPoYpos );
		pvPut( pntPoXpos );
		pvPut( pntPoYpos );
	    }
	    /* PVCLEAR( poSelect ); */
	} state PTGORG_UPDATE

	/*
	 * Cancel cancels (deletes) a named pointing origin. It does nothing
	 * to the pointing (even if that pointing origin was the current one)
	 */
 	when( efTestAndClear( poCancelFlag ) && poCancel ) {
	    LOGMSG( LOG_COMMAND, "tcsCommand: IDLE -> IDLE [poCancel]\n" );
	    tcsPtgorgCancel( poName );
	    /* PVCLEAR( poCancel ); */
	} state PTGORG_UPDATE

	/*
	 * Offset sets a global pointing origin offset that applies to all
	 * pointing origins; note that it uses (poXpos,poYpos), not
	 * the immediate-action (poXoff,poYoff)
	 */
 	when( efTestAndClear( poOffFlag ) && poOff ) {
	    LOGMSG( LOG_COMMAND, "tcsCommand: IDLE -> IDLE [poOff]\n" );
	    pntPoXoff = poXpos;
	    pntPoYoff = poYpos;
	    pvPut( pntPoXoff );
	    pvPut( pntPoYoff );
	    PVSET( pntLoad );
	    /* have to clear (poXpos,poYpos) in case used for a preset */
	    PVCLEAR( poXpos );
	    PVCLEAR( poYpos );
	    /* PVCLEAR( poOff ); */
	} state PTGORG_UPDATE

	/*
	 * When current pointing origin position changes (autonomously)
	 * update (X,Y) of selected pointing origin (if any)
	 */
	when ( efTestAndClear( pntPoXposFlag ) ||
	       efTestAndClear( pntPoYposFlag ) ) {
#if FALSE
	    LOGMSG( LOG_COMMAND, "tcsCommand: IDLE -> IDLE [pntPo[XY]pos]\n" );
#endif
	    if ( tcsPtgorgCurrent( poNameRbv, &poStatRbv,
					      &poXposRbv, &poYposRbv ) >= 0 )
		tcsPtgorgPreset( poNameRbv, pntPoXpos, pntPoYpos );
	} state PTGORG_UPDATE

	/*
	 * When pointing origin offset variables change (c.f. pointing
	 * origin offset as a result of guider "center by adjusting
	 * pointing origin"), convert from meters to radians and forward
	 * to the pointing (have to handle X and Y independently because
	 * otherwise can apply offsets twice)
	 */
	when ( efTestAndClear( poXoffFlag ) ) {
	    LOGMSG( LOG_COMMAND, "tcsCommand: IDLE -> IDLE [poXoff]\n" );
	    if ( focallength <= 0.0 ) {
		LOGMSG( LOG_COMMAND, "tcsCommand: focal length invalid!\n" );
	    }
	    else {
		pntPoXinc = poXoff / focallength;
		poXoffRbv = poXoff;
		pvPut( pntPoXinc );
	    }
	} state PTGORG_UPDATE

	when ( efTestAndClear( poYoffFlag ) ) {
	    LOGMSG( LOG_COMMAND, "tcsCommand: IDLE -> IDLE [poYoff]\n" );
	    if ( focallength <= 0.0 ) {
		LOGMSG( LOG_COMMAND, "tcsCommand: focal length invalid!\n" );
	    }
	    else {
		pntPoYinc = poYoff / focallength;
		poYoffRbv = poYoff;
		pvPut( pntPoYinc );
	    }
	} state PTGORG_UPDATE
    }

    /*
     * In the PRGORG_UPDATE state, all pointing origin variables are updated
     * and IDLE state is returned to
     */
    state PTGORG_UPDATE {
	when () {
	    tcsPtgorgCurrent( poNameRbv, &poStatRbv, &poXposRbv, &poYposRbv );
	    poTotalRbv = tcsPtgorgReport( MAXPTGORG, poNameArr, poStatArr,
						     poXposArr, poYposArr );
	    pvPut( poNameRbv  );
	    pvPut( poStatRbv  );
	    pvPut( poXposRbv  );
	    pvPut( poYposRbv  );
	    pvPut( poXoffRbv  );
	    pvPut( poYoffRbv  );
	    pvPut( poTotalRbv );
	    for ( j = 0; j < MAXPTGORG; j++ ) {
		pvPut( poNameArr[j] );
		pvPut( poStatArr[j] );
		pvPut( poXposArr[j] );
		pvPut( poYposArr[j] );
	    }
	} state IDLE
    } 
}
