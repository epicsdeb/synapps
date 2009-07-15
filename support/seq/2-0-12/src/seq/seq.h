/*	/share/epicsH  %W%     %G%
 *
 *	DESCRIPTION: Definitions for the run-time sequencer.
 *
 *      Author:         Andy Kozubal
 *      Date:           
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991,2,3, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * 07mar91,ajk	Changed SSCB semaphore id names.
 * 05jul91,ajk	Added function prototypes.
 * 16dec91,ajk	Setting up for VxWorks version 5.
 * 27apr92,ajk	Changed to new event flag mode (SSCB & PROG changed).
 * 27apr92,ajk	Removed getSemId from CHAN.
 * 28apr92,ajk	Implemented new event flag mode.
 * 17feb93,ajk	Fixed some functions prototypes.
 * 10jun93,ajk	Removed VxWorks V4/V5 conditional compile.
 * 20jul93,ajk	Removed non-ANSI function definitions.
 * 21mar94,ajk	Implemented new i/f with snc (see seqCom.h).
 * 21mar94,ajk	Implemented assignment of array elements to db.  Also,
 *		allow "unlimited" number of channels.
 * 28mar94,ajk	Added time stamp support.
 * 29mar94,ajk	Added dbOffset in db_channel structure; allows faster processing
 *		of values returned with monitors and pvGet().
 * 09aug96,wfl	Added syncQ queue support.
 * 30apr99,wfl	Replaced VxWorks dependencies with OSI.
 * 17may99,wfl	Changed FUNCPTR etc to SEQFUNCPTR; corrected sequencer() proto.
 * 07sep99,wfl	Added putComplete, message and putSemId to SSCB.
 * 22sep99,grw  Supported entry and exit actions; supported state options.
 * 18feb00,wfl	Added 'auxiliary_args' typedef (for seqAuxThread).
 * 29feb00,wfl	Converted to new OSI; defs for new thread death scheme etc.
 * 06mar00,wfl	Added function prototypes for global routines.
 * 31mar00,wfl	Made caSemId a mutex; added seqFindProgXxx() prototypes.
 */
#ifndef	INCLseqh
#define	INCLseqh

#include        <stdio.h>
#include        <stdlib.h>
#include	<ctype.h>

#include        "shareLib.h" /* reset share lib defines */
#include        "pvAlarm.h"     /* status and severity defs */
#include        "epicsThread.h" /* time stamp defs */
#include        "epicsTime.h"   /* time stamp defs */
#include	"epicsMutex.h"
#include	"epicsEvent.h"
#include	"epicsThread.h"
#include	"epicsTypes.h"
#include	"ellLib.h"
#include	"errlog.h"
#include	"taskwd.h"
#include	"pv.h"
#include	"shareLib.h"
#define SEQ_UGLY_WINDOWS_HACK
#define epicsExportSharedSymbols
#include	"seqCom.h"
#include	"seqPvt.h"
#endif	/*INCLseqh*/
