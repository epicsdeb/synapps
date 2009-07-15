/* aCalcPostfix.h
 *      Author:          Tim Mooney
 *      Date:            3-21-06
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
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
 * .01  03-18-98 tmm derived from sCalcPostfix.h
 * 
 */

#ifndef INC_aCalcPostfixh
#define INC_aCalcPostfixh

#include <shareLib.h>

#define	BAD_EXPRESSION	0
#define	END_STACK		127

epicsShareFunc long epicsShareAPI aCalcPostfix(char *pinfix, char *p_postfix, short *perror);

epicsShareFunc long epicsShareAPI 
	aCalcPerform(double *p_dArg, int num_dArgs, double **pp_aArg, int num_aArgs, long arraySize, double *p_dresult, double *p_aresult, char *post);

#endif /* INC_aCalcPostfixh */

