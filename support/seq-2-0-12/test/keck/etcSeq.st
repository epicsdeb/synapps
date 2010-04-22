/*+*****************************************************************************
 Module:	etcSeq.stpp

 Author:	W Lupton

 Description:	ETC subsystem sequencer (for demo purposes)

 Notes:		1. This code runs only under VxWorks.

		2. Operation:
		   a) To load:	    -> ld <${version}/bin/${target}/etcSeq.o
		   b) To execute:   -> seq &etcSeq, "tel=k2:,sub=etc:"
		      ("tel" and "sub" are macros which will be used in all
		       EPICS process variable names)
		   c) To stop:	    -> td "etcSeq"
		   d) To unload:    -> unld "etcSeq.o"

		3. Status from channel access operations is not tested.
		   Should it be?
*******************************************************************************/

program etcSeq		/* will be entry point name and primary task name */

option -c;		/* allow start before all connections are made */
option +r;		/* permit recursive use of multiple copies */

%%#include "knlConst.h"	/* for USE() macro (need ANSI pre-processor) */
%%#include "epicsPrint.h" /* for epicsPrintf() */
%%RCSID( "$Id" );
			/* avoids "not used" warning */

/*******************************************************************************
 * Include files (_only_ for #define constants)
 ******************************************************************************/

#include "knlConst.h"	/* for logging levels, status values etc */

/* ETC version of SETSTAT macro */
#define SETSTAT( _ss, _state, _stateVal, _status, _errstr ) \
        strcpy( etcState, _state ); \
	etcStateVal = _stateVal; \
        etcStatus = _status; \
        strcpy( etcErrstr, _errstr ); \
        pvPut( etcState ); \
        pvPut( etcStateVal ); \
        pvPut( etcStatus ); \
        pvPut( etcErrstr )

#ifdef LOGMSG		/* need to redeclare to use pVar */
#undef LOGMSG
#define LOGMSG( _level, _logmsg ) \
        %%do { \
            if ( pVar->logLevel & ( _level ) ) epicsPrintf( "%s", _logmsg ); \
        } while(0)
                        /* for logging messages to standard output */
#endif

/*******************************************************************************
 * Event flags
 ******************************************************************************/

/*
 * Fault checking
 */
evflag faultEnabled;
			/* set by main seq to enable fault seq's fault */
			/* checking; cleared by main seq to disable */
evflag faultDetected;
			/* set by fault seq to inform main seq that a */
			/* fault has been detected; cleared by fault seq */
			/* when main seq acknowledges fault */
evflag faultAck;
			/* set by main seq to acknowledge the setting of */
			/* faultDetected */

/*
 * Command processing
 */
evflag simulate;
			/* needs flag because command is a string */
evflag init;
evflag slew;
evflag halt;
evflag stby;
evflag bypass;
			/* set by cmd seq to inform main seq that the */
			/* various different external commands have */
			/* been received */
evflag commandEnabled;
			/* set by main seq to enable command processing */
			/* (currently never cleared) */

/*******************************************************************************
 * Process variables (shared between all state sets)
 ******************************************************************************/

/*
 * Status variables (internally generated so no need to monitor them)
 */
ASSVAR( string, etcState, "{tel}{sys}{sub}state"  );
ASSVAR( int, etcStateVal, "{tel}{sys}{sub}stateVal"  );
ASSVAR( int, etcStatus, "{tel}{sys}{sub}status" );
ASSVAR( string, etcErrstr, "{tel}{sys}{sub}errstr" );

ASSVAR( string, etcVersion, "{tel}{sys}{sub}version" );

ASSVAR( int, initActive, "{tel}{sys}{sub}initActive" );
ASSVAR( int, slewActive, "{tel}{sys}{sub}slewActive" );
ASSVAR( int, haltActive, "{tel}{sys}{sub}haltActive" );
ASSVAR( int, stbyActive, "{tel}{sys}{sub}stbyActive" );
ASSVAR( int, bypassActive, "{tel}{sys}{sub}bypassActive" );

/*
 * Fault variables (most would usually map directly to hardware)
 */
MONVAR( int, etcFault, "{tel}{sys}{sub}fault" );

/*
 * External commands (most of the standard ones)
 */
MONVAR( string, simulateCommand, "{tel}{sys}{sub}simulate" );
sync simulateCommand simulate;
			/* synched to event flag because it's a string and */
			/* normal command handling technique doesn't apply */
		
MONVAR( int, logLevelCommand, "{tel}{sys}{sub}logLevel" );
MONVAR( int, logLevelSetCommand, "{tel}{sys}{sub}logLevelSet" );
MONVAR( int, logLevelClrCommand, "{tel}{sys}{sub}logLevelClr" );

MONVAR( int, initCommand, "{tel}{sys}{sub}init" );
MONVAR( int, slewCommand, "{tel}{sys}{sub}slew" );
MONVAR( int, haltCommand, "{tel}{sys}{sub}halt" );
MONVAR( int, stbyCommand, "{tel}{sys}{sub}stby" );
MONVAR( int, bypassCommand, "{tel}{sys}{sub}bypass" );

/*******************************************************************************
 * Local variables (shared between all state sets)
 ******************************************************************************/

string simulateLevel;
			/* current simulation level */
int logLevel;
			/* current logging level */
int fullInit;
			/* whether init will be "full" (hardware init) */

/*+*****************************************************************************
 Function:	etcMain

 Author:	W Lupton

 Abstract:	ETC main sequence

 Description:	This sequence operates in close collaboration with the fault
		sequence and the command sequence. It is responsible for
		managing the transitions between the "main" (user-visible)
		states. Refer to KSD/87 for a description of these states
		and the acceptable transitions between them.

		Once enabled (by the main sequence) the fault sequence checks
		for s/w faults (process variable disconnect) and h/w faults
		(monitoring of appropriate database variables). When a fault
		is detected, the fault sequence sets the faultDetected event
		flag, expecting the main sequence to set the faultAck event
		flag. The fault sequence then clears the faultDetected flag
		and the main sequence proceeds with fault recovery and a
		return to its STANDBY state.

		Once enabled (by the main sequence) the command sequence
		checks for incoming commands, either by checking the values
		of monitored variables or else by testing event flags which
		will be set when commands arrive (see the description of the
		command sequence for more details). It then either handles
		the commands itself (for simple internal commands like setting
		logging levels) or else sets an event flag (e.g. the init flag
		for the init command) and an "active" process variable. The
		event flag and variable stay set until the command is complete.

		The main sequence wakes up as appropriate when these command
		event flags are set, and initiates the appropriate actions.
		If more than one command flag is set, the order of checking
		in the states is such that sensible behavior results (e.g. if
		the halt and stby flags are set, the halt flag is tested
		first, so the system halts before it shuts down).
*******************************************************************************/

ss etcMain {

    /*
     * All sequences start off in their first state.
     */
    state START {

	/*
	 * All EPICS process variables have connected. Disable fault
	 * detection (enabled after initialization), enable command
	 * processing, set state, status and version variables, and
	 * force the first initialization to be full (hardware init).
	 * Then enter STANDBY state awaiting an initialization command.
	 */
	when ( pvConnectCount() == pvChannelCount() ) {
	    LOGMSG( LOG_MAIN, "etcMain: START -> STANDBY [connected]\n" );

	    efClear( faultEnabled );

	    EFPVCLEAR( init, initActive );
	    EFPVCLEAR( halt, haltActive );
	    EFPVCLEAR( slew, slewActive );
	    EFPVCLEAR( stby, stbyActive );
	    EFPVCLEAR( bypass, bypassActive );
	    efSet( commandEnabled );

	    SETSTAT( "Main", "STBY", GEN_M_STATUS_STNBY, S_knl_ok, "OK" );
	    strcpy( etcVersion, "42.0" );
	    pvPut( etcVersion );

	    fullInit = TRUE;
	} state STANDBY

	/*
	 * 30 seconds has passed and still not all EPICS process
	 * variables have connected. Set bad status and continue
	 * waiting.
	 */
	when ( delay( 30 ) ) {
	    LOGMSG(LOG_DEBUG, "etcMain: START -> START [connect t/o]\n");
	    SETSTAT( "Main",  "START", GEN_M_STATUS_STNBY, S_knl_timeout,
		     "connect timeout" );
	} state START
    }

    /*
     * In the STANDBY state we are awaiting a command. The order in which
     * commands are checked is crucial! Note that command event flags
     * are not cleared until the command is complete.
     */
    state STANDBY {

	/*
	 * HALT command received. No op. Then clear the halt flag.
	 */
	when ( efTest( halt ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: STANDBY -> STANDBY [halt cmd]\n" );
	    EFPVCLEAR( halt, haltActive );
	} state STANDBY

	/*
	 * STANDBY command received. No op. Then clear the stby flag.
	 */
	when ( efTest( stby ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: STANDBY -> STANDBY [stby cmd]\n" );
	    EFPVCLEAR( stby, stbyActive );
	} state STANDBY

	/*
	 * INIT command received. Enter INITING state.
	 */
	when ( efTest( init ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: STANDBY -> INIT [init cmd]\n" );
	    SETSTAT( "Main",  "INIT", GEN_M_STATUS_INIT, S_knl_ok, "OK" );
	} state INITING

	/*
	 * BYPASS command received. Clear "fullInit" flag and enter
	 * INITING state.
	 */
	when ( efTest( bypass ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: STANDBY -> INIT [bypass cmd]\n" );
	    SETSTAT( "Main",  "INIT", GEN_M_STATUS_INIT, S_knl_ok, "OK" );
	    fullInit = FALSE;
	} state INITING

	/*
	 * SLEW command received. Set the init flag to force initialization.
	 */
	when ( efTest( slew ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: STANDBY -> INITING [slew cmd]\n" );
	    EFPVSET( init, initActive );
	    SETSTAT( "Main",  "INIT", GEN_M_STATUS_INIT, S_knl_ok, "OK" );
	} state INITING
    }

    /*
     * In the INITING state, subsystem initialization is performed. Only
     * STANDBY is handled immediately.
     */
    state INITING {

	/*
	 * STANDBY command received. Set the halt flag to force a halt.
	 */
	when ( efTest( stby ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: INITING -> HALTING [stby cmd]\n" );
	    EFPVSET( halt, haltActive );
	} state HALTING

	/*
	 * 1 sec delay to simulate initialization. Then enable fault checking
	 * and clear the init and bypass flags.
	 */
	when ( delay( 1.0 ) ) {
	    if ( fullInit )
		LOGMSG( LOG_MAIN, "etcMain: INITING -> HALTED [full init]\n" );
	    else
		LOGMSG( LOG_MAIN, "etcMain: INITING -> HALTED [quick init]\n" );
	    SETSTAT( "Main",  "HALT", GEN_M_STATUS_HALT, S_knl_ok, "OK" );
	    efSet( faultEnabled );
	    EFPVCLEAR( init, initActive );
	    EFPVCLEAR( bypass, bypassActive );
	    fullInit = FALSE;
	} state HALTED
    }

    /*
     * In the HALTING state, the subsystem is in the process of moving to the
     * HALTED state. There's no point handling STANDBY immediately, since it
     * will halt in any case.
     */
    state HALTING {

	/*
	 * Fault detected. Set fault flag and enter FAULT_ACK state.
	 */
	when ( efTest( faultDetected ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: HALTING -> FAULT_ACK [fault]\n" );
	    SETSTAT( "Main",  "FAULT", GEN_M_STATUS_FAULT, S_knl_fault,
		     "faulted while halting" );
	} state FAULT_ACK

	/*
	 * 1 sec delay to simulate halting. Then clear the halt flag.
	 */
	when ( delay( 1.0 ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: HALTING -> HALTED [1 sec delay]\n" );
	    SETSTAT( "Main",  "HALT", GEN_M_STATUS_HALT, S_knl_ok, "OK" );
	    EFPVCLEAR( halt, haltActive );
	} state HALTED
    }

    /*
     * In the HALTED state, the subsystem is halted.
     */
    state HALTED {

	/*
	 * Fault detected. Set fault flag and enter FAULT_ACK state.
	 */
	when ( efTest( faultDetected ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: HALTED -> FAULT_ACK [fault]\n" );
	    SETSTAT( "Main",  "FAULT", GEN_M_STATUS_FAULT, S_knl_fault,
		     "faulted while halted" );
	} state FAULT_ACK

	/*
	 * HALT command received. No op. Then clear the halt flag.
	 */
	when ( efTest( halt ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: HALTED -> HALTED [halt cmd]\n" );
	    EFPVCLEAR( halt, haltActive );
	} state HALTED

	/*
	 * STANDBY command received. Enter STBYING state.
	 */
	when ( efTest( stby ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: HALTED -> STBYING [stby cmd]\n" );
	    SETSTAT( "Main",  "STBY", GEN_M_STATUS_STNBY, S_knl_ok, "OK" );
	} state STBYING

	/*
	 * INIT command received. Enter INITING state.
	 */
	when ( efTest( init ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: HALTED -> INITING [init cmd]\n" );
	    SETSTAT( "Main",  "INIT", GEN_M_STATUS_INIT, S_knl_ok, "OK" );
	} state INITING

	/*
	 * BYPASS command received. Clear "fullInit" flag and enter
	 * INITING state.
	 */
	when ( efTest( bypass ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: STANDBY -> INIT [bypass cmd]\n" );
	    SETSTAT( "Main",  "INIT", GEN_M_STATUS_INIT, S_knl_ok, "OK" );
	    fullInit = FALSE;
	} state INITING

	/*
	 * SLEW command received. Enter SLEWING state.
	 */
	when ( efTest( slew ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: STANDBY -> SLEWING [slew cmd]\n" );
	    SETSTAT( "Main",  "SLEW", GEN_M_STATUS_CALIB, S_knl_ok, "OK" );
	} state SLEWING
    }

    /*
     * In the SLEWING state, the subsystem is slewing to its requested
     * position.
     */
    state SLEWING {

	/*
	 * Fault detected. Set fault flag and enter FAULT_ACK state.
	 */
	when ( efTest( faultDetected ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: SLEWING -> FAULT_ACK [fault]\n" );
	    SETSTAT( "Main",  "FAULT", GEN_M_STATUS_FAULT, S_knl_fault,
		     "faulted while slewing" );
	} state FAULT_ACK

	/*
	 * HALT, STANDBY or INIT command received. Set halt flag to force
	 * halt and enter HALTING state.
	 */
	when ( efTest( halt ) || efTest( stby ) || efTest( init ) ||
						   efTest( bypass ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: SLEWING -> HALTING [h/s/i/b cmd]\n" );
	    EFPVSET( halt, haltActive );
	} state HALTING

	/*
	 * 1 sec delay to simulate slewing. Then clear the slew flag.
	 */
	when ( delay( 1.0 ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: SLEWING -> TRACKING [1 sec delay]\n" );
	    SETSTAT( "Main",  "TRACK", GEN_M_STATUS_CALIB, S_knl_ok, "OK" );
	    EFPVCLEAR( slew, slewActive );
	} state TRACKING
    }

    /*
     * In the TRACKING state, the subsystem is tracking at its requested
     * position.
     */
    state TRACKING {

	/*
	 * Fault detected. Set fault flag and enter FAULT_ACK state.
	 */
	when ( efTest( faultDetected ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: TRACKING -> FAULT_ACK [fault]\n" );
	    SETSTAT( "Main",  "FAULT", GEN_M_STATUS_FAULT, S_knl_fault,
		     "faulted while tracking");
	} state FAULT_ACK

	/*
	 * HALT, STANDBY or INIT command received. Set halt flag to force
	 * halt and enter HALTING state.
	 */
	when ( efTest( halt ) || efTest( stby ) || efTest( init ) ||
						   efTest( bypass ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: TRACKING -> HALTING [h/s/i/b cmd]\n" );
	    EFPVSET( halt, haltActive );
	} state HALTING

	/*
	 * SLEW command. Enter SLEWING state.
	 */
	when ( efTest( slew ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: STANDBY -> SLEWING [slew cmd]\n" );
	    SETSTAT( "Main",  "SLEW", GEN_M_STATUS_CALIB, S_knl_ok, "OK" );
	} state SLEWING

	/*
	 * 10 sec delay to simulate tracking errors. Then enter SLEWING state.
	 */
	when ( delay( 10.0 ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: TRACKING -> SLEWING [10 sec delay]\n" );
	    SETSTAT( "Main",  "SLEW", GEN_M_STATUS_CALIB, S_knl_ok, "OK" );
	} state SLEWING
    }
 
   /*
    * In the STBYING state, the subsystem is in the process of shutting down.
    */
    state STBYING {

	/*
	 * Fault detected. Set fault flag and enter FAULT_ACK state.
	 */
	when ( efTest( faultDetected ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: STBYING -> FAULT_ACK [fault]\n" );
	    SETSTAT( "Main",  "FAULT", GEN_M_STATUS_FAULT, S_knl_fault,
		     "faulted while shutting");
	} state FAULT_ACK

	/*
	 * 1 sec delay to simulate shutting down. Then clear the stby flag.
	 */
	when ( delay( 1.0 ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: STBYING -> STANDBY [1 sec delay]\n" );
	    SETSTAT( "Main",  "STBY", GEN_M_STATUS_STNBY, S_knl_ok, "OK" );
	    EFPVCLEAR( stby, stbyActive );
	    fullInit = TRUE;
	} state STANDBY
    }

    /*
     * In the FAULT* states, do whatever is necessary to recover from the
     * fault, then disable fault checking, acknowledge it, set the halt
     * and fault flags, and await a command.
     */
    state FAULT_ACK {
	when () {
	    LOGMSG(LOG_MAIN, "etcMain: FAULT_ACK -> FAULT_RESET [fix fault]\n");
	    efSet( faultAck );
	    efClear( faultEnabled );
	    SETSTAT( "Main",  "FAULT", GEN_M_STATUS_FAULT, S_knl_fault,
		     "faulted" );
	} state FAULT_RESET
    }

    state FAULT_RESET {
	when ( ! efTest( faultDetected ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: FAULT_RESET -> FAULTED [ack acked]\n" );
	    EFPVCLEAR( init, initActive );
	    EFPVCLEAR( slew, slewActive );
	    EFPVSET( halt, haltActive );
	    EFPVCLEAR( stby, stbyActive );
	    EFPVCLEAR( bypass, bypassActive );
	} state FAULTED
    }

    state FAULTED {

	/*
	 * HALT command received. No op. Then clear the halt flag.
	 */
	when ( efTest( halt ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: FAULTED -> FAULTED [halt cmd]\n" );
	    EFPVCLEAR( halt, haltActive );
	} state FAULTED

	/*
	 * STANDBY command received. Enter STANDBY state.
	 */
	when ( efTest( stby ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: FAULTED -> STANDBY [stby cmd]\n" );
	    SETSTAT( "Main",  "STBY", GEN_M_STATUS_STNBY, S_knl_ok, "OK" );
	} state STANDBY

	/*
	 * INIT command received. Enter INITING state.
	 */
	when ( efTest( init ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: FAULTED -> INIT [init cmd]\n" );
	    SETSTAT( "Main",  "INIT", GEN_M_STATUS_INIT, S_knl_ok, "OK" );
	} state INITING

	/*
	 * BYPASS command received. Clear "fullInit" flag and enter
	 * INITING state.
	 */
	when ( efTest( bypass ) ) {
	    LOGMSG( LOG_MAIN, "etcMain: FAULTED -> INIT [bypass cmd]\n" );
	    SETSTAT( "Main",  "INIT", GEN_M_STATUS_INIT, S_knl_ok, "OK" );
	    fullInit = FALSE;
	} state INITING
    }
}

/*+*****************************************************************************
 Function:	etcFault

 Author:	W Lupton

 Abstract:	ETC fault sequence

 Description:	This sequence is responsible for monitoring the underlying
		subsystem for faults. Two classes of faults are supported:

		a) s/w faults are currently only process variable disconnects.
		b) h/w faults can be triggered by any change of state of any
		   database field

		The sequence is controlled by the main sequence. Initially,
		fault checking is disabled. It is enabled when the main
		sequence sets the faultEnabled event flag.

		When a fault is detected, the fault sequence sets the
		faultDetected event flag and expects the main sequence
		to react by setting the faultAck event flag, meaning "I
		have noted the fault". At this point, the fault sequence
		clears the faultDetected event flag and resumes checking
		(if checking is still enabled, which it normally won't
		be).

		The normal behavior of the main sequence, once the faultAck
		exchange is complete, will be to halt the subsystem and return
		to the IDLE state. Once the underlying reason for the fault
		has been determined and the fault rectified, a soft initialize
		will return the system to the HALTED state.

 Bugs:		Handling of s/w faults is not well thought out. The faultAck
		protocol is not honored in this case and probably should be.
		The rationale for separate treatment of s/w and h/w faults
		should be re-evaluated.

		The nature of the fault is not communicated back to the
		main sequence. Real subsystems will probably use variables
		for this purpose.
*******************************************************************************/

ss etcFault {

    /*
     * All sequences start off in their first state.
     */
    state START {

	/*
	 * When main sequence enables fault checking, enter CHECKING state.
	 */
	when ( efTest( faultEnabled ) ) {
	    LOGMSG( LOG_FAULT, "etcFault: START -> CHECKING [fault enabled]\n");
	    efClear( faultDetected );
	    efClear( faultAck );
	} state CHECKING
    }

    /*
     * In the CHECKING state, EPICS process variable disconnect or hardware
     * faults are detected.
     */
    state CHECKING {

	/*
	 * Fault checking has been disabled. Enter START state.
	 */
	when ( ! efTest( faultEnabled ) ) {
	    LOGMSG( LOG_FAULT, "etcFault: CHECKING -> START [disable]\n" );
	} state START

	/*
	 * Process variable has disconnected. Set "fault detected" and enter
	 * SW_FAULT state.
	 */
	when ( pvConnectCount() != pvChannelCount() ) {
	    LOGMSG( LOG_FAULT, "etcFault: CHECKING -> SW_FAULT [disconnect]\n");
	    efSet( faultDetected );
	} state SW_FAULT

	/*
	 * Hardware fault has occurred. Set "fault detected" and enter
	 * HW_FAULT state.
	 */
	when ( etcFault ) {
	    LOGMSG( LOG_FAULT, "etcFault: CHECKING -> HW_FAULT [fault]\n" );
	    efSet( faultDetected );
	    PVCLEAR( etcFault );
	} state HW_FAULT
    }

    /*
     * In the SW_FAULT state, we await re-connection of EPICS process
     * variables.
     */
    state SW_FAULT {

	/*
	 * All process variables have reconnected. Enter CHECKING state.
	 */
	when ( pvConnectCount() == pvChannelCount() ) {
	    LOGMSG( LOG_FAULT, "etcFault: SW_FAULT -> CHECKING [connect]\n" );
	    efClear( faultDetected );
	} state CHECKING	

	/*
	 * 30 seconds has passed and not all process variables have
	 * re-connected.
	 */
	when ( delay( 30 ) ) {
	    LOGMSG(LOG_DEBUG, "etcFault: SW_FAULT -> SW_FAULT [connect t/o]\n");
	} state SW_FAULT
    }

    /*
     * In the HW_FAULT state, we await either acknowledgment of the fault
     * or else disable of fault checking.
     */
    state HW_FAULT {

	/*
	 * Fault has been acknowledged. Clear detected flag and enter
	 * CHECKING state.
	 */
	when ( efTestAndClear( faultAck ) ) {
	    LOGMSG( LOG_FAULT, "etcFault: HW_FAULT -> HW_FAULT [ack]\n" );
	    efClear( faultDetected );
	} state HW_FAULT

	/*
	 * Fault checking has been disabled. Enter START state.
	 */
	when ( ! efTest( faultEnabled ) ) {
	    LOGMSG( LOG_FAULT, "etcFault: HW_FAULT -> START [disable]\n" );
	} state START
    }
}

/*+*****************************************************************************
 Function:	etcCommand

 Author:	W Lupton

 Abstract:	ETC command sequence

 Description:	This very simple sequence is enabled and disabled by the main
		sequence by setting and clearing the commandEnabled event
		flag. When enabled, it checks for incoming commands and sets
		event flags which wake up the main sequence and cause command
		processing.

		The handling of most commands is identical and is illustrated
		by the init command:

		when( initCommand ) {
	    	    LOGMSG( LOG_COMMAND, "etcCommand: IDLE -> IDLE [init]\n" );
	    	    EFPVSET( init, initActive );
	    	    PVCLEAR( initCommand );
		} state IDLE

		An init command is sent by setting the EPICS PV associated
		with initCommand to 1. This causes the above code to execute
		and the init event flag and the associated initActive EPICS
		PV are set to 1. Then initCommand is set back to 0, both to
		acknowledge to the caller and to prevent the above code from
		immediately re-executing.

		The init event flag and the initActive variable stay set to
		1 until the init action is complete, at which point they are
		set to 0. Clients which care can monitor the command and active
		variables. If command queueing is required, it must be
		implemented in database processing between clients and
		sequences.
*******************************************************************************/

ss etcCommand {

    /*
     * All sequences start off in their first state.
     */
    state START {

	/*
	 * Command processing enabled. Enter IDLE state.
	 */
	when ( efTest( commandEnabled ) ) {
	    LOGMSG( LOG_COMMAND, "etcCommand: START -> IDLE [enable]\n" );
	} state IDLE
    }

    /*
     * In the idle state, commands are accepted. Internal commands (e.g.
     * setting logging levels) are handled immediately. Other commands are
     * handled by the main sequence. Currently, these are all binary outputs
     * which cause event flags to be set and are then reset. This area is
     * under discussion.
     */
    state IDLE {

	/*
	 * Command processing disabled. Enter START state.
	 */
	when ( ! efTest( commandEnabled ) ) {
	    LOGMSG( LOG_COMMAND, "etcCommand: IDLE -> START [disable]\n" );
	} state START

	/*
	 * Internal simulation and logLevel commands are handled directly.
	 * Simulation is handled via an event flag because the variable
	 * which causes the simulation level to be set is a string and
	 * so can't be used in the "set to one; clear to ack" mode that
	 * is used for the rest of the commands.
	 */
	when ( efTestAndClear( simulate ) ) {
	    LOGMSG( LOG_COMMAND, "etcCommand: IDLE -> IDLE [simulate]\n" );
	    strcpy( simulateLevel, simulateCommand );
	} state IDLE

	when ( logLevelCommand ) {
	    LOGMSG( LOG_COMMAND, "etcCommand: IDLE -> IDLE [logLevel]\n" );
	    logLevel = logLevelCommand;
	    PVCLEAR( logLevelCommand );
	} state IDLE

	when ( logLevelSetCommand ) {
	    LOGMSG( LOG_COMMAND, "etcCommand: IDLE -> IDLE [logLevelSet]\n" );
	    logLevel |= logLevelSetCommand;
	    PVCLEAR( logLevelSetCommand );
	} state IDLE

	when ( logLevelClrCommand ) {
	    LOGMSG( LOG_COMMAND, "etcCommand: IDLE -> IDLE [logLevelClr]\n");
	    logLevel &= ~logLevelClrCommand;
	    PVCLEAR( logLevelClrCommand );
	} state IDLE

	/*
	 * External commands are handled as described above.
	 */
	when( initCommand ) {
	    LOGMSG( LOG_COMMAND, "etcCommand: IDLE -> IDLE [init]\n" );
	    EFPVSET( init, initActive );
	    PVCLEAR( initCommand );
	} state IDLE

	when( slewCommand ) {
	    LOGMSG( LOG_COMMAND, "etcCommand: IDLE -> IDLE [slew]\n" );
	    EFPVSET( slew, slewActive );
	    PVCLEAR( slewCommand );
	} state IDLE

	when( haltCommand ) {
	    LOGMSG( LOG_COMMAND, "etcCommand: IDLE -> IDLE [halt]\n" );
	    EFPVSET( halt, haltActive );
	    PVCLEAR( haltCommand );
	} state IDLE

	when( stbyCommand ) {
	    LOGMSG( LOG_COMMAND, "etcCommand: IDLE -> IDLE [stby]\n" );
	    EFPVSET( stby, stbyActive );
	    PVCLEAR( stbyCommand );
	} state IDLE

	when( bypassCommand ) {
	    LOGMSG( LOG_COMMAND, "etcCommand: IDLE -> IDLE [bypass]\n" );
	    EFPVSET( bypass, bypassActive );
	    PVCLEAR( bypassCommand );
	} state IDLE
    }
}
