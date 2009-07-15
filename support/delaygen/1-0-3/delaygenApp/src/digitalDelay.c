/******

digital delay

Given a table of delay values (in ps) corresponding to each of twelve delay lines, calculate
the delay values of all possible combinations of those delay lines. When user requests a delay
value, find the closest achievable delay, and output the bit pattern that achieves it.

aSub fields:
  A - the desired delay
  VALA - the closest achievable delay
  VALB - the bit pattern that will produce VALA
  VALC - actual delay - desired delay
  C - delays corresponding to each bit of pattern (array)
  I - index array, used to sort VALA
  D - achievable delays (array)
  B - bit patterns corresponding to D delays (array)

******/



#include <stddef.h>
#include <stdlib.h>
/* #include <ctype.h> */
#include <math.h>
#include <stdio.h>

/* #include <dbEvent.h> */
#include <dbDefs.h>
#include <dbCommon.h>
#include <recSup.h>
#include <epicsVersion.h>       /* for LT_EPICSBASE macro */
#include <aSubRecord.h>

#define GE_EPICSBASE(v,r,l) ((EPICS_VERSION>=(v)) && (EPICS_REVISION>=(r)) && (EPICS_MODIFICATION>=(l)))

void indexx(long n, double *arrin, long *indx);

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) > (b) ? (b) : (a))
#define NINT(f)  (int)((f)>0 ? (f)+0.5 : (f)-0.5)

volatile int digitalDelayDebug = 0;

static double delayValue(int pattern, int nbits, double *delay_line_values)
{
	int i;
	double value = 0.;
	for (i=0; i<nbits; i++) {
		if (pattern & (1<<i)) value += delay_line_values[i];
	}
	return (value);
}

#define SMALL 1.e-6
static void do_init(aSubRecord *psub)
{
	long pattern, s, t;
	double *delay_line_values = (double *)psub->c;
	double *delay_ps = (double *)psub->d;
	long *bitPattern = (long *)psub->b;
	long *index = (long *)psub->i;
	long npatterns = psub->nod;
	long nbits = psub->noc;
	long *reinit = (long *)psub->e;

	for (pattern=0; pattern<npatterns; pattern++) {
		index[pattern] = pattern;
		delay_ps[pattern] = delayValue(pattern, nbits, delay_line_values);

		/* sort delay-table index so that delay_ps[index[s]] increases with s */
		for (s=pattern; s>0 && (delay_ps[index[s]] <= delay_ps[index[s-1]]); s--) {
			if (delay_ps[index[s]] == delay_ps[index[s-1]]) {
				/* trouble: we need this to be monotonic, so we can step through it in a reproducible order; make it so */
				delay_ps[index[s]] = delay_ps[index[s-1]] + SMALL;
			} else {
				if (digitalDelayDebug) printf ("swap %f for %f\n", delay_ps[index[s]], delay_ps[index[s-1]]);
				t = index[s]; index[s] = index[s-1]; index[s-1] = t;
			}
		}

		bitPattern[pattern] = pattern;
		if (digitalDelayDebug) printf("0x%lx %f\n", pattern, delay_ps[pattern]);
	}
	if (digitalDelayDebug) {
		for (pattern=0; pattern<npatterns; pattern++) {
			printf("%ld, 0x%lx %f\n", index[pattern], pattern, delay_ps[index[pattern]]);
		}
	}
	*reinit = 0;
	return;
}

static long digitalDelay_init(aSubRecord *psub)
{
	double *delay_line_values = (double *)psub->c;
	long nbits = psub->noc;

	/* Initialize delay values.  User can write their own values to the C array before or after iocInit.
	 * If C is written after iocInit, user must set E and wait for it to be reset to 0.
	 */
	if (nbits>0)  delay_line_values[0]  = 5.;
	if (nbits>1)  delay_line_values[1]  = 14.5;
	if (nbits>2)  delay_line_values[2]  = 25.;
	if (nbits>3)  delay_line_values[3]  = 81.6;
	if (nbits>4)  delay_line_values[4]  = 101.5;
	if (nbits>5)  delay_line_values[5]  = 176.4;
	if (nbits>6)  delay_line_values[6]  = 352.7;
	if (nbits>7)  delay_line_values[7]  = 672.7;
	if (nbits>8)  delay_line_values[8]  = 1298.2;
	if (nbits>9)  delay_line_values[9]  = 2545.5;
	if (nbits>10) delay_line_values[10] = 5047.4;
	if (nbits>11) delay_line_values[11] = 10088.;

	do_init(psub);
	return(0);
}

static long digitalDelay_do(aSubRecord *psub)
{
	double *a = (double *)psub->a;
	double *vala = (double *)psub->vala;
	long *valb = (long *)psub->valb;
	double *valc = (double *)psub->valc;
	double *vald = (double *)psub->vald;
	double *vale = (double *)psub->vale;
	double *delay_ps = (double *)psub->d;
	long *bitPattern = (long *)psub->b;
	long *index = (long *)psub->i;
	long *reinit = (long *)psub->e;
	long npatterns = psub->nod;
	double diff;
	long bestIx, lowerIx, higherIx;
	int i, ix;


	if (digitalDelayDebug) printf("digitalDelay_do: a=%f\n", *a);

	if (*reinit) do_init(psub);

	diff = 1000.;
	bestIx = 0;
	lowerIx = 0;
	higherIx = 0;
	for (i=0; i<npatterns; i++) {
		ix = index[i];
		if (fabs(*a-delay_ps[ix]) < diff) {
			diff = fabs(*a-delay_ps[ix]);
			bestIx = ix;
			lowerIx = index[MAX(0,i-1)];
			higherIx = index[MIN(npatterns-1,i+1)];
		}
	}
	*vala = delay_ps[bestIx];
	*valb = bitPattern[bestIx];
	*valc = *vala - *a;

	*vald = delay_ps[lowerIx];
	*vale = delay_ps[higherIx];
	if (digitalDelayDebug) {
		printf("digitalDelay_do: bestIx=%ld, lowerVal=%f, bestVal=%f, higherVal=%f, pattern=0x%lx\n", bestIx, *vald, *vala, *vale, *valb);
	}
	return(0);
}

#if GE_EPICSBASE(3,14,0)
#include <registryFunction.h>
#include <epicsExport.h>

epicsExportAddress(int, digitalDelayDebug);

static registryFunctionRef digitalDelayRef[] = {
	{"digitalDelay_init", (REGISTRYFUNCTION)digitalDelay_init},
	{"digitalDelay_do", (REGISTRYFUNCTION)digitalDelay_do}
};

static void digitalDelayRegister(void) {
	registryFunctionRefAdd(digitalDelayRef, NELEMENTS(digitalDelayRef));
}

epicsExportRegistrar(digitalDelayRegister);

#endif
