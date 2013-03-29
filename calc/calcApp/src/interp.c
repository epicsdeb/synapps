/* Interp - Manage and use interpolation table.
 * aSub record fields:
 *  vala[]  independent variable
 *  valb[]  dependent variable 1
 *  valc[]  dependent variable 2
 *  a       new value of independent variable, for interp or adding entry
 *  valn    number of entries
 *  f       interpolation order (1: linear; >1: polynomial)
 *  g       mode (0: interpolate; 1: add point to table
 *  b       new value for dependent variable 1 array
 *  c       new value for dependent variable 2 array
 *  vale    successful interpolation
 *  valf    interpolation result for dependent variable 1
 *  valg    interpolation result for dependent variable 2
 *  nova    max number of entries in a array - limits table size
 *  novb    max number of entries in b array - limits table size
 *  novc    max number of entries in c array - limits table size
 *
 *  Note: the code uses only n entries, where n = MIN(nova, novb, novc)
 *
 *  New fields for array interpolation
 *  h[]     new values of independent variable, for interp
 *  valh[]  interpolation result for dependent variable 1
 *  vali[]  interpolation result for dependent variable 2
 *  noh     max number of entries in h array, will be coerced to <= novh, novi
 *  novh    max number of entries in valh array
 *  novi    max number of entries in valh array
 *
 * Tim Mooney
 */

#include <stddef.h>
#include <stdlib.h>
/* #include <ctype.h> */
#include <math.h>
#include <stdio.h>

/* #include <dbEvent.h> */
#include <dbDefs.h>
#include <dbCommon.h>
#include <recSup.h>
#include <aSubRecord.h>

#define MAXORDER 10
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) > (b) ? (b) : (a))
#define NINT(f)  (int)((f)>0 ? (f)+0.5 : (f)-0.5)
#define SMALL 1.e-6
#define SAME(A,B) (fabs((A)-(B)) < SMALL)

static long polyInterp(double *x, double *y, int n, double xx, double *yy);
static double find_index(long n, double *a, double *d, long *lo, long *hi);

volatile int interpDebug=0;

static long interp_init(aSubRecord *pasub)
{
	int *order;
	long *valn;

	order = (int *)pasub->f;
	if (*order > MAXORDER) *order = MAXORDER;

	valn = (long *)pasub->valn;
	if (*valn > pasub->noa) *valn = pasub->nova;
	if (*valn > pasub->nob) *valn = pasub->novb;
	if (*valn > pasub->noc) *valn = pasub->novc;

	if (pasub->noh > pasub->novh) pasub->noh = pasub->novh;
	if (pasub->noh > pasub->novi) pasub->noh = pasub->novi;
	return(0);
}

static long interp_do(aSubRecord *pasub)
{
	double	*a, *b, *c, ix;
	double	*vala, *valb, *valc, *valf, *valg, *h, *valh, *vali;
	long	hi, lo, n, ii, iplace, first, *order, mode, *valn, *vale, err=0;
	int		same_X=0, i;

	a = (double *)pasub->a;
	b = (double *)pasub->b;
	c = (double *)pasub->c;
	mode = *((long *)(pasub->g));
	vala = (double *)pasub->vala;
	valb = (double *)pasub->valb;
	valc = (double *)pasub->valc;
	vale = (long *)pasub->vale;
	valn = (long *)pasub->valn;
	valf = (double *)pasub->valf;
	valg = (double *)pasub->valg;
	h = (double *)pasub->h;
	valh = (double *)pasub->valh;
	vali = (double *)pasub->vali;
	if (*valn > pasub->nova) *valn = pasub->nova;
	if (*valn > pasub->novb) *valn = pasub->novb;
	if (*valn > pasub->novc) *valn = pasub->novc;
	if (*valn < 0) *valn = 0;
	n = *valn;

	if (interpDebug) printf("interp: x=%f, cmd=%ld", *a, mode);
	switch (mode) {
	case 0:
		/* interpolate */
		*vale = 0;  /* presume failure */
		order = (long *)pasub->f;
		if (*order > MAXORDER) *order = MAXORDER;
		/* if arrays haven't been set up yet, output is same as input */
		if (n <= 1) {
			*valf = *valg = *a;
			return(0);
		}
		if (*a < vala[0]) {*valf = valb[0]; *valg = valc[0]; return(-1);}
		if (*a > vala[n-1]) {*valf = valb[n-1]; *valg = valc[n-1]; return(-1);}

		ix = find_index(n, vala, a, &lo, &hi);

		if (*order > 1) {
			/* polynomial */
			ii = MIN(*order+1, n);	/* number of points required */
			/* arrange that we switch from one set of points to the next */
			/* when *d crosses a point, and not halfway between */
			first = (int)(ix - ii/2) + ((ii%2)?0:1);
			first = MAX(0, MIN(n-ii, first));
			err = polyInterp(&vala[first], &valb[first], ii, *a, valf);
			err += polyInterp(&vala[first], &valc[first], ii, *a, valg);
			if (interpDebug >= 10) {
				printf("poly (O%ld, first=%ld) valf = %f; err=%ld\n",
					*order, first, *valf, err);
			}
		} else {
			/* linear interpolation */
			*valf = valb[lo] + (ix-lo)*(valb[hi]-valb[lo]);
			*valg = valc[lo] + (ix-lo)*(valc[hi]-valc[lo]);
			if (interpDebug) printf("linear (%ld:%f:%ld) valf = %f, err=%ld\n",
				lo, ix, hi, *valf, err);
		}
		if (!err) *vale = 1;
		break;

	case 1:
		/* Add point to tables.  Maintain a[] increasing with index.*/
		*vale = 0;  /* tell everyone there's no interpolation result */
		if (*valn == pasub->nova) {
			/* Can't add any more points */
			return(-1);
		}
		if (n == 0) {
			iplace = 0;
		} else if (*a < vala[0]) {
			iplace = 0;
			same_X = SAME(*a, vala[0]);
		} else if (*a > vala[n-1]) {
			if (SAME(*a, vala[n-1])) {
				iplace = n-1;
				same_X = 1;
			} else {
				iplace = n;
			}
		} else {
			ix = find_index(n, vala, a, &lo, &hi);
			if (SAME(ix, floor(ix))) {
				same_X = 1;
				iplace = floor(ix);
			} else if (SAME(ix, ceil(ix))) {
				same_X = 1;
				iplace = ceil(ix);		
			} else {
				iplace = ceil(ix);
			}
		}
		if (iplace >= 0) {
			if (same_X) {
				/* replace existing entry */
				vala[iplace] = *a;
				valb[iplace] = *valf = *b;
				valc[iplace] = *valg = *c;
			} else {
				for (ii=n; ii>=iplace && ii>0; ii--) {
					vala[ii] = vala[ii-1];
					valb[ii] = valb[ii-1];
					valc[ii] = valc[ii-1];
				}
				vala[iplace] = *a;
				valb[iplace] = *valf = *b;
				valc[iplace] = *valg = *c;
				n++;
				*valn = n;
			}
		}
		break;

	case 2:
		/* Clear tables */
		*vale = 0;  /* tell everyone there's no interpolation result */
		for (ii=0; ii < pasub->nova; ii++) {
			vala[ii] = valb[ii] = valc[ii] = 0.0;
		}
		*valn = 0;
		break;

	case 3:
		/* interpolate array */
		*vale = 0;  /* presume failure */
		order = (long *)pasub->f;
		if (*order > MAXORDER) *order = MAXORDER;
		/* if arrays haven't been set up yet, output is same as input */
		if (n <= 1) {
			for (i=0; i<pasub->noh; i++) {
				valh[i] = h[i];
				vali[i] = h[i];
			}
			return(0);
		}

		for (i=0; i<pasub->noh; i++) {
			if (h[i] < vala[0]) {
				valh[i] = valb[0];
				vali[i] = valb[0];
			}
			if (h[i] > vala[n-1]) {
				valh[i] = valb[n-1];
				vali[i] = valb[n-1];
			}

			ix = find_index(n, vala, &h[i], &lo, &hi);

			if (*order > 1) {
				/* polynomial */
				ii = MIN(*order+1, n);	/* number of points required */
				/* arrange that we switch from one set of points to the next */
				/* when *d crosses a point, and not halfway between */
				first = (int)(ix - ii/2) + ((ii%2)?0:1);
				first = MAX(0, MIN(n-ii, first));
				err = polyInterp(&vala[first], &valb[first], ii, h[i], &valh[i]);
				err += polyInterp(&vala[first], &valc[first], ii, h[i], &vali[i]);
				if (interpDebug >= 10) {
					printf("poly (O%ld, first=%ld) valh[i] = %f; err=%ld\n",
						*order, first, valh[i], err);
				}
			} else {
				/* linear interpolation */
				valh[i] = valb[lo] + (ix-lo)*(valb[hi]-valb[lo]);
				vali[i] = valc[lo] + (ix-lo)*(valc[hi]-valc[lo]);
				if (interpDebug) printf("linear (%ld:%f:%ld) valh[i] = %f, err=%ld\n",
					lo, ix, hi, valh[i], err);
			}
		}
		if (!err) *vale = 1;
		break;
	}

	return(0);
}

/* Lagrange interpolation */
static long polyInterp(double *x, double *y, int n, double xx, double *yy)
{
	int i, j;
	double p[MAXORDER+2];

	if (interpDebug >= 20) {
		printf("interp:polyInterp: x[0..%d] = [ ", n-1);
		for (i=0; i<=n; i++) printf("%f ", x[i]);
		printf("]\n");
	}
	for (i=0; i<n; i++) {
		p[i] = y[i];
		for (j=0; j<n; j++) {
			if (i != j) {
				if (x[i] == x[j]) {
					if (interpDebug)
						printf("Error in polynomial interpolation");
					return(-1);
				}
				p[i] *= (xx-x[j])/(x[i]-x[j]);
			}
		}
	}
	*yy = 0;
	for (i=0; i<n; i++) {
		if (interpDebug >= 20) printf("interp:polyInterp: p[i]=%f\n", p[i]);
		*yy += p[i];
	}
	return(0);
}

/*
 * find_index: find the array index, ix, at which *d would occur in a[].
 * ix is a real number, and its distance from neighboring integers
 * corresponds with *d's distance from neighboring array values.
 */
static double find_index(long n, double *a, double *d, long *lo, long *hi)
{
	double	ix;
	long   	mid;


	/* binary search for indexes of a[] bracketing *d */
	for (*lo=0, *hi=n-1, mid = (*hi+*lo)/2; abs(*hi-*lo)>1;) {
		if (*d > a[mid]) {
			*lo = mid;
		} else {
			*hi = mid;
		}
		mid = (*hi+*lo)/2;
	}
	/* real-number 'index' at which *d would occur in a[] */
	ix = *lo + (*d-a[*lo])/(a[*hi]-a[*lo]);
	if (interpDebug) printf(" ix=%f, ", ix);
	return(ix);
}


#include <registryFunction.h>
#include <epicsExport.h>

epicsExportAddress(int, interpDebug);

static registryFunctionRef interpRef[] = {
	{"interp_init", (REGISTRYFUNCTION)interp_init},
	{"interp_do", (REGISTRYFUNCTION)interp_do}
};

static void interpRegister(void) {
	registryFunctionRefAdd(interpRef, NELEMENTS(interpRef));
}

epicsExportRegistrar(interpRegister);
