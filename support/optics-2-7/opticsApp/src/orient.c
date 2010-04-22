/* set your tab width to 4 */

#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <epicsMath.h>
#include "matrix3.h"
#include "orient.h"

volatile int orientDebug = 0;

#ifndef M_PI
#define M_PI 3.14159265359
#endif
#define D2R (M_PI/180.)

/* avoid division by zero */
double checkSmall(double x) {
	if (fabs(x) < SMALL) x = (x < 0) ? -SMALL : SMALL;
	return(x);
}

/*** rotation matrices ***/
/* r = rot_z(a) */
void calc_rotZ(double a, double r[3][3]) {
	r[0][0] = cos(a);	r[0][1] = sin(a);	r[0][2] = 0;
	r[1][0] = -sin(a);	r[1][1] = cos(a);	r[1][2] = 0;
	r[2][0] = 0;		r[2][1] = 0;		r[2][2] = 1;
}

/* r = rot_y(a) */
void calc_rotY(double a, double r[3][3]) {
	r[0][0] = cos(a);	r[0][1] = 0;	r[0][2] = sin(a);
	r[1][0] = 0;		r[1][1] = 1;	r[1][2] = 0;
	r[2][0] = -sin(a);	r[2][1] = 0;	r[2][2] = cos(a);
}

/* calc HKL from angles (in degrees) */
int angles_to_HKL(double angles_arg[4], double omtx_inv[3][3], double a0_inv[3][3],
		double hkl[3]) {
	double vec[3];
	double r1[3][3], r2[3][3], r3[3][3], rot[3][3], rot_i[3][3];
	double tmp[3][3];
	double angles[4];
	int i;

	for (i=0; i<4; i++) angles[i] = angles_arg[i]*D2R;

	vec[0] = sin(angles[TTH_INDEX]/2); vec[1] = 0; vec[2] = 0;

	calc_rotZ(angles[TH_INDEX]-angles[TTH_INDEX]/2, r1);
	if (orientDebug) printArray(r1, "angles_to_HKL:rotZ");
	calc_rotY(angles[CHI_INDEX], r2);
	if (orientDebug) printArray(r2, "angles_to_HKL:rotY");
	calc_rotZ(angles[PHI_INDEX], r3);
	if (orientDebug) printArray(r3, "angles_to_HKL:rotZ");
	multArrayArray(r1, r2, rot);
	multArrayArray(rot, r3, rot);
	if (orientDebug) printArray(rot, "angles_to_HKL:rot");

	if (invertArray(rot, rot_i)) {
		if (orientDebug) {
			printf("angles_to_HKL: can't invert rotation matrix\n");
			printArray(rot, "rot");
		}
		hkl[0] = hkl[1] = hkl[2] = 0;
		return(-1);
	}
	multArrayArray(a0_inv, omtx_inv, tmp);
	if (orientDebug) printArray(tmp, "angles_to_HKL:a0_i X o_i");
	multArrayArray(tmp, rot_i, tmp);
	if (orientDebug) printArray(tmp, "angles_to_HKL:a0_i X o_i X rot_i");
	multArrayVector(tmp, vec, hkl);
	return(0);
}

/* 
 * Calc angles (in degrees) from HKL, A0, and OMTX, according to constraint.
 * If constraint == PHI_CONST, this routine uses angles[PHI_INDEX] as supplied.
 */
int HKL_to_angles(double hkl[3], double a0[3][3], double omtx[3][3],
		double angles_arg[4], int constraint) {
	double hklp[3], tmp[3];
	double ry[3][3], rz[3][3];
	double R;
	double angles[4];
	double xx;
	int i, err=0;

	errno = 0;
	for (i=0; i<4; i++) angles[i] = angles_arg[i]*D2R;
	multArrayVector(a0, hkl, hklp);
	if (orientDebug) printVector(hklp, "HKL_to_angles:A0 X HKL");
	multArrayVector(omtx, hklp, hklp);
	if (orientDebug) printVector(hklp, "HKL_to_angles:OMTX X A0 X HKL");
	/* length of HKL vector */
	R = sqrt(hklp[H_INDEX]*hklp[H_INDEX]+hklp[K_INDEX]*hklp[K_INDEX]+hklp[L_INDEX]*hklp[L_INDEX]);
	angles[TTH_INDEX] = 2 * asin(R);
	switch (constraint) {
	case MIN_CHI_PHIm90:
		angles[PHI_INDEX] = atan(-hklp[H_INDEX]*hklp[K_INDEX] / (hklp[L_INDEX]*hklp[L_INDEX] + hklp[K_INDEX]*hklp[K_INDEX]));
		/* fall through */
	case PHI_CONST:
		if (orientDebug) printf("HKL_to_angles:PHI = %f\n", angles_arg[PHI_INDEX]);
		xx = checkSmall(hklp[L_INDEX]);
		if (orientDebug) printf("HKL_to_angles:arg of atan = %f\n",
			-(hklp[H_INDEX]*cos(angles[PHI_INDEX]) + hklp[K_INDEX]*sin(angles[PHI_INDEX])) / xx);
		angles[CHI_INDEX] = M_PI/2 + atan(-(hklp[H_INDEX]*cos(angles[PHI_INDEX]) + hklp[K_INDEX]*sin(angles[PHI_INDEX])) / xx);
		calc_rotY(angles[CHI_INDEX], ry);
		calc_rotZ(angles[PHI_INDEX], rz);
		multArrayVector(rz, hklp, tmp);
		multArrayVector(ry, tmp, tmp);
		if (orientDebug) printVector(tmp, "HKL_to_angles:Ry X Rz X OMTX X A0 X HKL");
		angles[TH_INDEX] = atan2(tmp[K_INDEX], tmp[H_INDEX]) + angles[TTH_INDEX]/2;
		break;
	case OMEGA_ZERO:
	default:
		angles[TH_INDEX] = angles[TTH_INDEX]/2;
		xx = checkSmall(sqrt(hklp[H_INDEX]*hklp[H_INDEX]+hklp[K_INDEX]*hklp[K_INDEX]));
		angles[CHI_INDEX] = atan(hklp[L_INDEX]/xx);
		xx = checkSmall(hklp[H_INDEX]);
		angles[PHI_INDEX] = atan2(hklp[K_INDEX], xx); /* i.e., atan(K/H) in [-PI, PI] */
		break;
	}
	for (i=0; i<4; i++) {
		angles_arg[i] = angles[i]/D2R;
		if (isnan(angles_arg[i])) err = 1;
	}
	if (orientDebug) print4Vector(angles_arg, "HKL_to_angles:angles (degrees)");
	return (errno || err);
}

/*
 * Calc A0 matrix from lattice parameters and wavelength, and return it as r[][].
 * Lattice spacings a,b,c and wavelength lambda are in same (arbitrary) units.
 * Lattice angles alpha, beta, gamma are in units of degrees.
 */
int calc_A0(double a, double b, double c,
              double alpha_arg, double beta_arg, double gamma_arg,
			  double lambda, double r[3][3], double r_i[3][3]) {
	double A[3], B[3], C[3];
	double AxB[3], BxC[3], CxA[3];
	double tmp;
	double alpha = alpha_arg * D2R;
	double beta = beta_arg * D2R;
	double gamma = gamma_arg * D2R;

	if (orientDebug) printf("calc_A0: a=%f,b=%f,c=%f,alpha=%f,beta=%f,gamma=%f,lambda=%f\n",
			a,b,c,alpha,beta,gamma,lambda);
	A[0] = a;
	A[1] = 0;
	A[2] = 0;
	if (orientDebug) printVector(A, "calc_A0:A");

	B[0] = b * cos(gamma);
	B[1] = b * sin(gamma);
	B[2] = 0;
	if (orientDebug) printVector(B, "calc_A0:B");

	tmp = (cos(alpha) - cos(beta)*cos(gamma))/sin(gamma);
	C[0] = c * cos(beta);
	C[1] = c * tmp;
	C[2] = c * sqrt(1 - cos(beta)*cos(beta) - tmp*tmp);
	if (orientDebug) printVector(C, "calc_A0:C");

	/* r = factor * |BxC, CxA, AxB| */
	tmp = lambda/(2 * dotcross(A,B,C));
	if (orientDebug) printf("calc_A0: lambda/(2*AXB.C) = %f\n", tmp);
	cross(A, B, AxB);
	cross(B, C, BxC);
	cross(C, A, CxA);
	if (orientDebug) {
		printVector(AxB, "calc_A0:AxB");
		printVector(BxC, "calc_A0:BxC");
		printVector(CxA, "calc_A0:CxA");
	}
	r[0][0] = tmp * BxC[0];
	r[0][1] = tmp * CxA[0];
	r[0][2] = tmp * AxB[0];
	r[1][0] = tmp * BxC[1];
	r[1][1] = tmp * CxA[1];
	r[1][2] = tmp * AxB[1];
	r[2][0] = tmp * BxC[2];
	r[2][1] = tmp * CxA[2];
	r[2][2] = tmp * AxB[2];
	if (invertArray(r, r_i)) {
		int i, j;
		/* error */
		if (orientDebug) {
			printf("calc_A0: can't invert A0 matrix\n");
			printArray(r, "A0");
		}
		for (i=0; i<3; i++) {
			for (j=0; j<3; j++) {
				r[i][j] = r_i[i][j] = (i==j ? 1 : 0);
			}
		}
		return(-1);
	}
	return(0);
}

/* Calculate orientation matrix */
int calc_OMTX(double v1_hkl[3], double v1_angles[4], double v2_hkl[3], double v2_angles[4],
                double a0[3][3], double a0_i[3][3], double o[3][3], double o_i[3][3]) {
	double v1p[3], v2p[3], v3p[3];
	double v1pp[3], v2pp[3], v3pp[3];
	double tmp[3];
	double Vp[3][3], Vpp[3][3], Vpp_i[3][3];
	double I[3][3] = {{1,0,0},{0,1,0},{0,0,1}};
	int i, j;

	if (orientDebug) {
		printVector(v1_hkl, "calc_OMTX: v1_hkl");
		print4Vector(v1_angles, "calc_OMTX: v1_angles");
		printVector(v2_hkl, "calc_OMTX: v2_hkl");
		print4Vector(v2_angles, "calc_OMTX: v2_angles");
		printArray(a0, "calc_OMTX: a0\n");
	}

	/** calc Vp[3][3] **/
	multArrayVector(a0, v1_hkl, v1p);
	if (orientDebug) printVector(v1p, "calc_OMTX: v1p");
	multArrayVector(a0, v2_hkl, tmp);
	cross(v1p, tmp, v2p);
	if (orientDebug) printVector(v2p, "calc_OMTX: v2p");
	cross(v2p, v1p, v3p);
	if (orientDebug) printVector(v3p, "calc_OMTX: v3p");
	/* Vp array columns are v<i>p */
	for (i=0; i<3; i++) {
		Vp[0][i] = v1p[i];
		Vp[1][i] = v2p[i];
		Vp[2][i] = v3p[i];
	}
	if (orientDebug) printArray(Vp, "calc_OMTX: Vp\n");

	/** calc Vpp[3][3] **/
	angles_to_HKL(v1_angles, I, a0_i, v1pp);
	multArrayVector(a0, v1pp, v1pp);
	if (orientDebug) printVector(v1pp, "calc_OMTX: v1pp");

	angles_to_HKL(v2_angles, I, a0_i, v2pp);
	multArrayVector(a0, v2pp, v2pp);
	if (orientDebug) printVector(v2pp, "calc_OMTX: A0 X HKL(v2)");
	cross(v1pp, v2pp, v2pp);
	if (orientDebug) printVector(v2pp, "calc_OMTX: v2pp");

	cross(v2pp, v1pp, v3pp);

	/* Vpp array columns are v<i>pp */
	for (i=0; i<3; i++) {
		Vpp[0][i] = v1pp[i];
		Vpp[1][i] = v2pp[i];
		Vpp[2][i] = v3pp[i];
	}
	if (orientDebug) printArray(Vp, "calc_OMTX: Vp");

	if (invertArray(Vpp, Vpp_i)) {
		if (orientDebug) {
			printf("calc_OMTX: can't invert Vpp matrix\n");
			printArray(Vpp, "Vpp");
		}
		goto error;
	}
	/** o = Vpp_i Vpp **/
	multArrayArray(Vpp_i, Vp, o);
	if (orientDebug) printArray(o, "calc_OMTX: OMTX");
	if (invertArray(o, o_i)) {
		if (orientDebug) {
			printf("angles_to_HKL: can't invert OMTX\n");
			printArray(o, "o");
		}
		goto error;
	}
	return(0);
	
error:
	for (i=0; i<3; i++) {
		for (j=0; j<3; j++) {
			o[i][j] = o_i[i][j] = (i==j ? 1 : 0);
		}
	}
	return(-1);
}

/* Check orientation matrix. */
double checkOMTX(double v2_hkl[3], double v2_angles_arg[4], double a0[3][3],
	double a0_inv[3][3], double o_inv[3][3])
{
	double v2p[3], v2pp[3];
	double norm, err_angles;
	int i;

	multArrayVector(a0, v2_hkl, v2p);
	norm = sqrt(dot(v2p,v2p));
	for (i=0; i<3; i++) v2p[i] /= norm;

	i = angles_to_HKL(v2_angles_arg, o_inv, a0_inv, v2pp);
	multArrayVector(a0, v2pp, v2pp);
	norm = sqrt(dot(v2pp,v2pp));
	for (i=0; i<3; i++) v2pp[i] /= norm;

	if (orientDebug) {
		printVector(v2p, "checkOMTX:v2p");
		printVector(v2pp, "checkOMTX:v2pp");
	}

	err_angles = acos(dot(v2p, v2pp)) / D2R;
	return(err_angles);
}
