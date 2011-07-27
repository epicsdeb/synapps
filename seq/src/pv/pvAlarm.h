/* pvAlarm.h,v 1.1.1.1 2000/04/04 03:22:15 wlupton Exp
 *
 * Definitions for EPICS sequencer message system-independent status and
 * severity (alarms).
 *
 * William Lupton, W. M. Keck Observatory
 */

#ifndef INCLpvAlarmh
#define INCLpvAlarmh

/*
 * Status.
 */
typedef enum {
    /* generic OK and error statuses */
    pvStatOK           = 0,
    pvStatERROR        = -1,
    pvStatDISCONN      = -2,

    /* correspond to EPICS statuses */
    pvStatREAD	       = 1,
    pvStatWRITE        = 2,
    pvStatHIHI         = 3,
    pvStatHIGH         = 4,
    pvStatLOLO         = 5,
    pvStatLOW          = 6,
    pvStatSTATE	       = 7,
    pvStatCOS          = 8,
    pvStatCOMM         = 9,
    pvStatTIMEOUT      = 10,
    pvStatHW_LIMIT     = 11,
    pvStatCALC         = 12,
    pvStatSCAN         = 13,
    pvStatLINK         = 14,
    pvStatSOFT         = 15,
    pvStatBAD_SUB      = 16,
    pvStatUDF          = 17,
    pvStatDISABLE      = 18,
    pvStatSIMM         = 19,
    pvStatREAD_ACCESS  = 20,
    pvStatWRITE_ACCESS = 21
} pvStat;

/*
 * Severity.
 */
typedef enum {
    /* generic OK and error severities */
    pvSevrOK      = 0,
    pvSevrERROR   = -1,

    /* correspond to EPICS severities */
    pvSevrNONE    = 0,
    pvSevrMINOR   = 1,
    pvSevrMAJOR   = 2,
    pvSevrINVALID = 3
} pvSevr;

#endif /* INCLpvAlarmh */

/*
 * pvAlarm.h,v
 * Revision 1.1.1.1  2000/04/04 03:22:15  wlupton
 * first commit of seq-2-0-0
 *
 * Revision 1.1  2000/03/29 01:58:59  wlupton
 * initial insertion
 *
 */

