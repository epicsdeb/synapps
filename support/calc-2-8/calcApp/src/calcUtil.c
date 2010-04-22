#ifdef vxWorks
#include <vxWorks.h>
#endif
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#define SMALL 1e-8
#define MAX(a,b) (a)>(b)?(a):(b)
#define MIN(a,b) (a)<(b)?(a):(b)

int pfit(double *x, double *y, int n,
	double *x0, double *x0_e, double *y0, double *y0_e, double *w, double *w_e,
	double *chisq, double *depth);

int lfit(double *x, double *y, int n,
	double *m, double *m_e, double *b, double *b_e, double *chisq);
int fitpoly(double *x, double *y, int n,
	double *c, double *b, double *a, double *mask);



/*
 * Caller supplies x[], y[] arrays of n pts.
 * We return derivative in d.
 */

int nderiv(double *x, double *y, int n, double *d, int npts, double *lx)
{
	int i, j, e, m;
	double c, b, a;

	m = 2*npts+1;
	/* first m/2+1 points */
	e = fitpoly(x,y,m, &c, &b, &a, NULL);
	if (e) {
		printf("nderiv: error in fitpoly\n");
		return(e);
	}
	/*
	 * y[i] = c + b*x[i] + a*(x[i])^2
	 * dy/dx = b + 2*a*(x[i])
	 */
	for (j=0; j<m/2+1; j++) d[j] = e ? 0.0 : b + 2*a*x[j];

	/* middle points */
	for (i=m/2+1; i<n-(m/2+1); i++) {
		/* make local copy of x-array segment */
		for (j=0; j<m; j++) lx[j] = x[(i-m/2)+j]-x[i-m/2];
		e = fitpoly(lx,&(y[i-m/2]),m, &c, &b, &a, NULL);
		if (e) {
			printf("nderiv: error in fitpoly\n");
			return(e);
		}
		/* d[i] = b + 2*a*x[i]; */
		d[i] = b + 2*a*lx[m/2];
		/* printf("   x,y,d = %f, %f, %f\n", x[i], y[i], d[i]); */
	}

	/* last m/2+1 points */
	/* make local copy of x-array segment */
	for (j=0; j<m; j++) lx[j] = x[(n-m)+j]-x[n-m];
	e = fitpoly(lx,&(y[n-m]),m, &c, &b, &a, NULL);
	if (e) {
		printf("nderiv: error in fitpoly\n");
		return(e);
	}
	for (j=0; j<m/2+1; j++) d[(n-(m/2+1))+j] = e ? 0.0 : b + 2*a*x[j+m/2];
	return(0);
}

int deriv(double *x, double *y, int n, double *d)
{
	double work[5]; /* must be >= 2m+1, where m is 5th arg to nderiv() */
	return(nderiv(x, y, n, d, 2, work));
}

int invert3x3(double **aa, double **bb)
{
	double a = aa[0][0], b = aa[0][1], c = aa[0][2];
	double d = aa[1][0], e = aa[1][1], f = aa[1][2];
	double g = aa[2][0], h = aa[2][1], i = aa[2][2];
	double det = a*(e*i-h*f) + b*(f*g-i*d) + c*(d*h-g*e);

	if (fabs(det) < SMALL) return(-1);
	bb[0][0] = (e*i - f*h) / det;
	bb[0][1] = (c*h - b*i) / det;
	bb[0][2] = (b*f - c*e) / det;
	bb[1][0] = (f*g - d*i) / det;
	bb[1][1] = (a*i - c*g) / det;
	bb[1][2] = (c*d - a*f) / det;
	bb[2][0] = (d*h - e*g) / det;
	bb[2][1] = (b*g - a*h) / det;
	bb[2][2] = (a*e - b*d) / det;
	return(0);
}

int invert2x2(double **aa, double **bb)
{
	double a = aa[0][0], b = aa[0][1];
	double c = aa[1][0], d = aa[1][1];
	double det = a*d - c*b;

	if (fabs(det) < SMALL) return(-1);
	bb[0][0] =  d / det;
	bb[0][1] = -b / det;
	bb[1][0] = -c / det;
	bb[1][1] =  a / det;
	return(0);
}

/* line: y = m*x +b */
int lfit(double *x, double *y /*, double *e*/, int n,
	double *m, double *m_e, double *b, double *b_e, double *chisq)
{
	double beta[2], a[2], ea[2];
	double aa[4], *alpha[2];
	double ia[4], *ialpha[2];
	double e2, q;
	int i;

	if (n<2) return(-1);
	alpha[0] = &aa[0]; alpha[1] = &aa[2];
	ialpha[0] = &ia[0]; ialpha[1] = &ia[2];

	beta[0] = beta[1] = 0;
	alpha[0][0] = alpha[0][1] = alpha[1][0] = alpha[1][1] = 0;
	for (i=0; i<n; i++) {
		e2 = 1 /* e[i]*e[i] */;
		beta[0] += y[i]/e2;
		beta[1] += (y[i]*x[i])/e2;
		alpha[0][0] += 1/e2;
		alpha[0][1] += x[i]/e2;
		alpha[1][1] += (x[i]*x[i])/e2;
	}
	alpha[1][0] = alpha[0][1];

	if (invert2x2(alpha, ialpha)) {
		printf("lfit: error in invert2x2\n");
		return(-1);
	}
	*b = a[0] = beta[0]*ialpha[0][0] + beta[1]*ialpha[1][0];
	*m = a[1] = beta[0]*ialpha[0][1] + beta[1]*ialpha[1][1];

	ea[0] = ea[1] = 0; 
	for (i=0; i<n; i++) {
		q = ialpha[0][0] + ialpha[1][0]*x[i];
		ea[0] += q*q;
		q = ialpha[0][1] + ialpha[1][1]*x[i];
		ea[1] += q*q;
	}
	*b_e = ea[0] = sqrt(ea[0]);
	*m_e = ea[1] = sqrt(ea[1]);

	*chisq = 0;
	for (i=0; i<n; i++) {
		q = y[i] - (a[0] + a[1] * x[i]);
		/* q /= e[i]; */
		*chisq += q*q;
	}
	if (n>2) *chisq /= (n-2);	/* per degree of freedom */

	return(0);
}

/* parabola: y[i] = y0 + w*(x[i]-x0)*(x[i]-x0) */
int pfit(double *x, double *y /*, double *e */, int n,
	double *x0, double *x0_e, double *y0, double *y0_e, double *w, double *w_e,
	double *chisq, double *depth)
{
	double beta[3], a[3], ea[3];
	double aa[9],  *alpha[3];
	double ia[9], *ialpha[3];
	double e1, e2, q, x2, q1;
	int i, j;

	if (n<3) return(-1);
	alpha[0] =&aa[0]; alpha[1] =&aa[3], alpha[2] =&aa[6];
	ialpha[0]=&ia[0]; ialpha[1]=&ia[3], ialpha[2]=&ia[6];

	for (i=0; i<3; i++) {
		beta[i] = 0; for (j=0; j<3; j++) {alpha[i][j] = 0;}
	}

	for (i=0, e1=0.; i<n; i++) {
		e1 += 1 /* e[i] */ ;
		e2 = 1 /* e[i]*e[i] */ ;
		beta[0] += y[i]/e2;
		beta[1] += (y[i]*x[i])/e2;
		beta[2] += (y[i]*x[i]*x[i])/e2;
		alpha[0][0] += 1/e2;
		alpha[0][1] += (q = x[i])/e2;
		alpha[1][1] += (q *= x[i])/e2;
		alpha[1][2] += (q *= x[i])/e2;
		alpha[2][2] += (q *= x[i])/e2;
	}
	e1 /= n-1;
	alpha[1][0] = alpha[0][1];
	alpha[2][0] = alpha[0][2] = alpha[1][1];
	alpha[2][1] = alpha[1][2];

	if (invert3x3(alpha, ialpha)) {
		printf("pfit: error in invert3x3\n");
		return(-1);
	}

	for (i=0; i<3; i++) {
		a[i] = ea[i] = 0;
		for (j=0; j<3; j++) {a[i] += beta[j]*ialpha[j][i];}
	}

	for (i=0; i<n; i++) {
		x2 = x[i]*x[i];
		q = ialpha[0][0] + ialpha[1][0]*x[i] + ialpha[2][0]*x2;
		ea[0] += q*q;
		q = ialpha[0][1] + ialpha[1][1]*x[i] + ialpha[2][1]*x2;
		ea[1] += q*q;
		q = ialpha[0][2] + ialpha[1][2]*x[i] + ialpha[2][2]*x2;
		ea[2] += q*q;
	}
	ea[0] = sqrt(ea[0]);
	ea[1] = sqrt(ea[1]);
	ea[2] = sqrt(ea[2]);

	*chisq = 0;
	for (i=0; i<n; i++) {
		q = y[i] - (a[0] + (a[1] + a[2]*x[i]) * x[i]);
		/* q /= e[i]; */
		*chisq += q*q;
	}
	*chisq /= (n-3);	/* per degree of freedom */

	/* printf("pfit: a0=%g(%g); a1=%g(%g); a2=%g(%g); chisq=%g\n",
		a[0], ea[0], a[1], ea[1], a[2], ea[2], *chisq); */

	*x0 = -a[1]/(2*a[2]);
	q = (-1)/(2*a[2]); q *= q;
	q1 = a[1]/(2*a[2]); q1 *= q1;
	*x0_e = ea[1]*ea[1]*q + ea[2]*ea[2]*q1;
	*x0_e = sqrt(*x0_e);
	*y0 = a[0] - a[1]*a[1]/(4*a[2]);
	q = (-a[1])/(2*a[2]); q *= q;
	q1 = a[1]*a[1]/(4*a[2]*a[2]);  q1 *= q1;
	*y0_e = ea[0]*ea[0] + ea[1]*ea[1]*q + ea[2]*ea[2]*q1;
	*y0_e = sqrt(*y0_e);
	*w = a[2];
	*w_e = ea[2];
	*chisq = 0;
	for (i=0; i<n; i++) {
		q = y[i] - (*y0 + *w * (x[i] - *x0) * (x[i] - *x0));
		*chisq += q*q/1 /* (e[i]*e[i]) */ ;
	}
	if (n>3) *chisq /= (n-3);	/* per degree of freedom */

	*depth = *w * ((x[0] - *x0) * (x[0] - *x0) + (x[n-1] - *x0) * (x[n-1] - *x0)) / (2*e1);

	/* printf("pfit: x0=%7f(%7f); y0=%7f(%7f);  w=%7f(%7f); chi=%7f; depth=%f\n",
		*x0, *x0_e, *y0, *y0_e, *w, *w_e, *chisq, *depth);*/

	return(0);
}

/* fit: y[i] = c + b*(x[i]-x0) + a*(x[i]-x0)*(x[i]-x0) */
int fitpoly(double *x, double *y, int n,
	double *c, double *b, double *a, double *mask)
{
	double beta[3];
	double aa[9],  *alpha[3];
	double ia[9], *ialpha[3];
	int i, j;

	if (n<3) return(-1);
	alpha[0] =&aa[0]; alpha[1] =&aa[3], alpha[2] =&aa[6];
	ialpha[0]=&ia[0]; ialpha[1]=&ia[3], ialpha[2]=&ia[6];

	for (i=0; i<3; i++) {
		beta[i] = 0; for (j=0; j<3; j++) {alpha[i][j] = 0;}
	}

	for (i=0; i<n; i++) {
		if ((mask==NULL) || (mask[i]>SMALL)) {
			beta[0] += y[i];
			beta[1] += y[i]*x[i];
			beta[2] += (y[i]*x[i]*x[i]);
			alpha[0][0] += 1;
			alpha[0][1] += x[i];
			alpha[1][1] += x[i]*x[i];
			alpha[1][2] += x[i]*x[i]*x[i];
			alpha[2][2] += x[i]*x[i]*x[i]*x[i];
		}
	}
	alpha[1][0] = alpha[0][1];
	alpha[2][0] = alpha[0][2] = alpha[1][1];
	alpha[2][1] = alpha[1][2];

	if (invert3x3(alpha, ialpha)) {
		printf("fitpoly: error in invert3x3\n");
		return(-1);
	}

	*c = *b = *a = 0.;
	for (j=0; j<3; j++) {
		*c += beta[j]*ialpha[j][0];
		*b += beta[j]*ialpha[j][1];
		*a += beta[j]*ialpha[j][2];
	}
	return(0);
}
