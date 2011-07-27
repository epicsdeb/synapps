/* @(#)subAve.c	1.2 4/27/95     */
/* subAve.c -  */
/*
 * Author:      Frank Lenkszus
 * Date:        9/29/93
 * Additional work:  Tim Mooney
 *      Experimental Physics and Industrial Control System (EPICS)
*/
/*
*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.
 
(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
Argonne National Laboratory (ANL), with facilities in the States of 
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.

Portions of this material resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from March 30, 1993, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods. 
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.

*****************************************************************
                                DISCLAIMER
*****************************************************************

NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.  

*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY (708-252-2000).
*/
/*
* Modification Log:
* -----------------
* .01  9-29-93  frl  initial
* .02  4-27-95  frl  added RESTART and MODE
* .03  9-05-02  tmm  If NUM_2_AVE (.A) is greater than allowed, set it
*                    to the maximum allowed number, so user can see it.
*                    Set restart field (.C) to zero after we use it.
*                    Report current sample via .E field.
* .03  6-23-09  tmm  Added a second algorithm: fit to line, return current
*                    value of line.  This is better for PID loops, because
*                    it estimates the target's current value, rather than
*                    the target's average value during the sampling time.
* .04  6-26-09  tmm  Changed circular buffer use to roll over at the end
*                    of the buffer, rather than at the number of samples.
*                    This permits algorithm to accept a change in the number
*                    of samples without restarting.  Changed "restart" to
*                    "clear".  (Clear doesn't add a data point.)  Added "done"
*                    field, to simplify database support for putCallback.
*/

/*  subroutine to average data */
/*  F. Lenkszus */

#ifdef vxWorks
#include <vxWorks.h>
#else
#define OK 0
#define ERROR -1
#endif

#include <stdio.h>
#include <stdlib.h>

#include <alarm.h>
#include <cvtTable.h>
#include <dbDefs.h>
#include <dbAccess.h>
#include <recGbl.h>
#include <recSup.h>
#include <devSup.h>
#include <link.h>
#include <devLib.h>
#include <errlog.h>
#include <dbEvent.h>
#include <subRecord.h>
#include <registryFunction.h>
#include <epicsExport.h>
#include <math.h>

#define NINT(f)  (int)((f)>0 ? (f)+0.5 : (f)-0.5)
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

/* User specifies buffer size by specifying the number of points to average.
 * We never allocate less than MIN_CIRBUFSIZE, or more than MAX_CIRBUFSIZE.
 * We never reduce the buffer size, but we do increase it if we must.
 */
#define	MAX_CIRBUFSIZE	100000
#define	MIN_CIRBUFSIZE	100

#define NUM_2_AVE	psub->a
#define INPUTVAL	psub->b
#define CLEAR		psub->c
#define MODE		psub->d
#define FILL		psub->e
#define ALGORITHM	psub->f
#define DONE		psub->g
#define AVERAGE		psub->h
#define LINFIT		psub->i
#define AUTOFIT		psub->j
#define SLOPE		psub->k
#define CORRELATION	psub->l

#define MODE_CONTINUOUS	0
#define MODE_ONESHOT	1

#define ALGORITHM_AVERAGE	0
#define ALGORITHM_LINFIT	1
#define ALGORITHM_AUTO		2

int	debugSubAve = 0;
/* The following statement serves to make this debugging symbol available, 
 * but more importantly to force the linker to include this entire module
 */
epicsExportAddress(int, debugSubAve);

struct	fcirBuf {
	short	num;	/* Number of values to average */
	short	fill;	/* Number of values acquired thus far */
	short	algorithm;	/* Average (0), fit-to-line (1), or automatic combination of the two (2). */
	short	mode;	/* "CONTINUOUS (0) or ONE-SHOT (1) */
	double	*yp;	/* Pointer to next y value */
	double	sumy;	/* running sum of y values */
	double	sumy2;	/* running sum of y*y values */
	double	ave;	/* running average of y values */
	double	*ybuf;
	long	bufSize;

	epicsTimeStamp	startTime_A;
	double	*tp_A;
	double	*tbuf_A;
	double	sumt_A;
	double	sumt2_A;
	double	sumty_A;
	short	tfill_A;

	epicsTimeStamp	startTime_B;
	double	*tp_B;
	double	*tbuf_B;
	double	sumt_B;
	double	sumt2_B;
	double	sumty_B;
	short	currBuf;
	short	tfill_B;
};

#define BUFFER_A 0
#define BUFFER_B 1
#define INC(p, buf, size) if (++(p) >= (buf) + (size)) (p) = (buf);
#define DEC(p, buf, size) if (--(p) < (buf)) (p) += (size);

long	initSubAve(struct subRecord *psub)
{
	char	*xname="initSubAve";
	struct	fcirBuf	*p;

	if ((psub->dpvt = calloc(1, sizeof(struct fcirBuf))) == NULL) {
		errPrintf(S_dev_noMemory, __FILE__, __LINE__,
			"%s: couldn't allocate memory for %s", xname, psub->name);
		return(S_dev_noMemory);
	}
	p = (struct fcirBuf *)psub->dpvt;
	if (debugSubAve)
		printf("%s: Init completed for Subroutine Record %s\n", xname, psub->name);
	p->num = 1;
	p->fill = 0;
	p->ave = p->sumy = p->sumy2 = 0;

	p->sumt_A = p->sumt2_A = p->sumty_A = 0;
	p->tfill_A = 0;

	p->sumt_B = p->sumt2_B = p->sumty_B = 0;
	p->tfill_B = 0;

	p->currBuf = BUFFER_A;
	p->algorithm = (short)ALGORITHM;
	p->mode = (short)MODE;
	return(OK);
}


long SubAve(struct subRecord *psub)
{
	char	*xname="SubAve";
	long	num;
	short	clear, doCalc=0;
	unsigned short monitor_mask;
	struct	fcirBuf	*p;
	double dt=0, dt_A=0, dt_B=0;
	epicsTimeStamp	currTime;
	double *yoldest_p=NULL;

	if ((p = (struct fcirBuf *)psub->dpvt) == NULL) {
		if (debugSubAve)
			errPrintf(S_dev_noMemory, __FILE__, __LINE__,
				"%s: dpvt is NULL for %s", xname, psub->name);
		return(ERROR);
	}

	clear = CLEAR;
	if (CLEAR) {
		CLEAR = 0;
		db_post_events(psub, &(CLEAR), DBE_VALUE);
	}

	num = MIN(MAX_CIRBUFSIZE, MAX(1, (long)NINT(NUM_2_AVE)));
	if (fabs(num - NUM_2_AVE) > .001) {
		NUM_2_AVE = num;
		db_post_events(psub, &(NUM_2_AVE), DBE_VALUE);
	}

	if (num > p->bufSize) {
		/* prepare to reallocate buffer memory */
		if (p->ybuf) {free(p->ybuf); p->ybuf = NULL;}
		if (p->tbuf_A) {free(p->tbuf_A); p->tbuf_A = NULL;}
		if (p->tbuf_B) {free(p->tbuf_B); p->tbuf_B = NULL;}
		p->bufSize = num;
	}

	if (p->ybuf == NULL) {
		if (p->bufSize < MIN_CIRBUFSIZE) p->bufSize = MIN_CIRBUFSIZE;
		p->yp = p->ybuf = calloc(p->bufSize, sizeof(double));
		p->tp_A = p->tbuf_A = calloc(p->bufSize, sizeof(double));
		p->tp_B = p->tbuf_B = calloc(p->bufSize, sizeof(double));
		if (p->ybuf == NULL) {
			errPrintf(S_dev_noMemory, __FILE__, __LINE__,
				"%s: ybuf is NULL for %s", xname, psub->name);
			return(ERROR);
		}
		clear = 1;
	}


	if (p->algorithm != ALGORITHM) {
		p->algorithm = ALGORITHM;
	}

	if (p->mode != MODE) {
		if (p->mode == MODE_ONESHOT) clear = 1;
		p->mode = MODE;
	}

	if ( ((num != p->num) && (MODE == MODE_CONTINUOUS)) || clear ) {
	/* if (clear) { */
		/* User changed number of values to average, or user said 'clear'. */
		p->yp = p->ybuf;
		p->fill = 0;
		p->num = num;
		p->ave = p->sumy = p->sumy2 = 0;
		psub->val = 0;

		p->tp_A = p->tbuf_A;
		p->sumt_A = p->sumt2_A = p->sumty_A = 0;
		p->tfill_A = 0;

		p->tp_B = p->tbuf_B;
		p->sumt_B = p->sumt2_B = p->sumty_B = 0;
		p->tfill_B = 0;

		p->currBuf = BUFFER_A;

		recGblSetSevr(psub, SOFT_ALARM, MAJOR_ALARM);
		DONE = 0;
	}

	if (!clear) {

		if (p->fill == p->num) {
			if (MODE == MODE_CONTINUOUS) {
				/* Add new value, subtract oldest */
				yoldest_p = p->yp - p->num;
				if (yoldest_p < p->ybuf) yoldest_p += p->bufSize;
				p->sumy += INPUTVAL - *yoldest_p;
				p->ave = p->sumy/(double)(p->num);
				p->sumy2 += INPUTVAL*INPUTVAL - (*yoldest_p) * (*yoldest_p);
				*p->yp = INPUTVAL;
				INC(p->yp, p->ybuf, p->bufSize);
				doCalc = 1;
			}
		} else {
			p->sumy += INPUTVAL;
			p->ave = p->sumy/(double)(++p->fill);
			p->sumy2 += INPUTVAL * INPUTVAL;
			*p->yp = INPUTVAL;
			INC(p->yp, p->ybuf, p->bufSize);
			doCalc = 1;
		}

		epicsTimeGetCurrent(&currTime);
		if (p->tfill_A == p->num) {
			if (MODE == MODE_CONTINUOUS) {
				/* Add new value, subtract oldest */
				double *toldest_p = p->tp_A - p->num;
				if (toldest_p < p->tbuf_A) toldest_p += p->bufSize;
				dt_A = epicsTimeDiffInSeconds(&currTime, &(p->startTime_A));
				p->sumt_A += dt_A - *toldest_p;
				p->sumt2_A += dt_A*dt_A - (*toldest_p) * (*toldest_p);
				p->sumty_A += dt_A*INPUTVAL - (*toldest_p) * (*yoldest_p);
				*p->tp_A = dt_A;
				INC(p->tp_A, p->tbuf_A, p->bufSize);
			}
		} else {
			if (p->tfill_A == 0) {
				dt_A = 0;
				epicsTimeGetCurrent(&(p->startTime_A));
			} else {
				dt_A = epicsTimeDiffInSeconds(&currTime, &(p->startTime_A));
			}
			p->sumt_A += dt_A;
			p->sumt2_A += dt_A*dt_A;
			p->sumty_A += dt_A*INPUTVAL;
			*p->tp_A = dt_A;
			INC(p->tp_A, p->tbuf_A, p->bufSize);
			if (++p->tfill_A == p->num) {
				/* A is good, we can restart B */
				p->currBuf = BUFFER_A;
				p->tp_B = p->tbuf_B;
				p->sumt_B = p->sumt2_B = p->sumty_B = 0;
				p->tfill_B = 0;
			}
		}

		if (p->tfill_B == p->num) {
			if (MODE == MODE_CONTINUOUS) {
				/* Add new value, subtract oldest */
				double *toldest_p = p->tp_B - p->num;
				if (toldest_p < p->tbuf_B) toldest_p += p->bufSize;
				dt_B = epicsTimeDiffInSeconds(&currTime, &(p->startTime_B));
				p->sumt_B += dt_B - *toldest_p;
				p->sumt2_B += dt_B*dt_B - (*toldest_p) * (*toldest_p);
				p->sumty_B += dt_B*INPUTVAL - (*toldest_p) * (*yoldest_p);
				*p->tp_B = dt_B;
				INC(p->tp_B, p->tbuf_B, p->bufSize);
			}
		} else {
			if (p->tfill_B == 0) {
				dt_B = 0;
				epicsTimeGetCurrent(&(p->startTime_B));
			} else {
				dt_B = epicsTimeDiffInSeconds(&currTime, &(p->startTime_B));
			}
			p->sumt_B += dt_B;
			p->sumt2_B += dt_B*dt_B;
			p->sumty_B += dt_B*INPUTVAL;
			*p->tp_B = dt_B;
			INC(p->tp_B, p->tbuf_B, p->bufSize);
			if (++p->tfill_B == p->num) {
				/* B is good, we can restart A */
				p->currBuf = BUFFER_B;
				p->tp_A = p->tbuf_A;
				p->sumt_A = p->sumt2_A = p->sumty_A = 0;
				p->tfill_A = 0;
			}
		}

		if (doCalc) {
			double sumt, sumt2, sumty;

			if (debugSubAve>=10) {
				int i, nA, nB;
				nA = p->tp_A - p->tbuf_A;
				nB = p->tp_B - p->tbuf_B;
				printf("\n%s: A=[", psub->name);
				for (i=0; i<nA; i++) printf("%3.0f,", p->tbuf_A[i]);
				printf("]\n");

				printf("%s: B=[", psub->name);
				for (i=0; i<nB; i++) printf("%3.0f,", p->tbuf_B[i]);
				printf("]\n");
			}

			if (p->currBuf == BUFFER_A) {
				sumt = p->sumt_A; sumt2 = p->sumt2_A; sumty = p->sumty_A; dt = dt_A;
			} else {
				sumt = p->sumt_B; sumt2 = p->sumt2_B; sumty = p->sumty_B; dt = dt_B;
			}

			psub->val = psub->j = psub->i = psub->h = p->ave;
			psub->k = psub->l = 0;
			/* calculate line fit */
			if (p->fill >= 3) {
				int N;
				double m, mm, b, r2;
				N = p->fill;
				m = (sumty - sumt*p->sumy/N)/(sumt2 - sumt*sumt/N);
				psub->k = m;
				b = (p->sumy - m * sumt)/N;
				r2 = (N*sumty - sumt*p->sumy)*(N*sumty - sumt*p->sumy);
				r2 /= ((N*sumt2 - sumt*sumt) * (N*p->sumy2 - p->sumy*p->sumy));
				psub->l = sqrt(r2);
				mm = m * sqrt(r2);
				psub->i = m*dt + (p->sumy - m * sumt)/N;
				psub->j = mm*dt + (p->sumy - mm * sumt)/N;
				if (p->algorithm == ALGORITHM_LINFIT) {
					psub->val = psub->i;
				} else if (p->algorithm == ALGORITHM_AUTO) {
					psub->val = psub->j;
				}
			}
		}

		if (p->fill == p->num) {
			monitor_mask = recGblResetAlarms(psub);
			db_post_events(psub, &psub->val, monitor_mask);
			DONE=1;
		} else {
			recGblSetSevr(psub, SOFT_ALARM, MAJOR_ALARM);
			DONE=0;
		}
	}


	FILL = p->fill;
	db_post_events(psub, &(FILL), DBE_VALUE);

	return(OK);
}

static registryFunctionRef subAveRef[] = {
	{"initSubAve", (REGISTRYFUNCTION)initSubAve},
	{"SubAve", (REGISTRYFUNCTION)SubAve}
};

static void subAveRegister(void) {
	registryFunctionRefAdd(subAveRef, NELEMENTS(subAveRef));
}

epicsExportRegistrar(subAveRegister);
