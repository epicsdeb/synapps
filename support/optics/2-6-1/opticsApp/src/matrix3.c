#include <math.h>
#include <stdio.h>
#include "matrix3.h"

volatile int matrix3Debug = 0;

void printVector(double v[3], char *name) {
	printf("%s = (%.5f,%.5f,%.5f)\n",name,v[0],v[1],v[2]);
}
void print4Vector(double v[4], char *name) {
	printf("%s = (%.5f,%.5f,%.5f,%.5f)\n",name,v[0],v[1],v[2],v[3]);
}
void printArray(double a[3][3], char *name) {
	printf("\n%s =\n", name);
	printf("    | %10.5f  %10.5f  %10.5f |\n", a[0][0], a[0][1], a[0][2]);
	printf("    | %10.5f  %10.5f  %10.5f |\n", a[1][0], a[1][1], a[1][2]);
	printf("    | %10.5f  %10.5f  %10.5f |\n\n", a[2][0], a[2][1], a[2][2]);
}

/* a . b */
double dot(double a[3], double b[3]) {
	return(a[0]*b[0]+a[1]*b[1]+a[2]*b[2]);
}

/* r = a X b */
void cross(double a[3], double b[3], double r[3]) {
	double t[3];
	int i;
	t[0] = a[1]*b[2]-a[2]*b[1];
	t[1] = a[2]*b[0]-a[0]*b[2];
	t[2] = a[0]*b[1]-a[1]*b[0];
	for (i=0; i<3; i++) {r[i] = t[i];}
}

/* a . b X c (same as 'a X b . c') */
double dotcross(double a[3], double b[3], double c[3]) {
	return(a[0]*(b[1]*c[2]-b[2]*c[1]) +
	        a[1]*(b[2]*c[0]-b[0]*c[2]) +
			a[2]*(b[0]*c[1] - b[1]*c[0]));
}

void multArrayArray(double a[3][3], double b[3][3], double r[3][3]) {
	double t[3][3];
	int i, j, k;

	for (i=0; i<3; i++) {
		for (j=0; j<3; j++) {
			t[i][j] = 0;
			for (k=0; k<3; k++)
				t[i][j] += a[i][k]*b[k][j];
			if (matrix3Debug) printf("mult: t[%1d][%1d]=%f\n", i,j,t[i][j]);
		}
	}
	for (i=0; i<3; i++) {
		for (j=0; j<3; j++) {
			r[i][j] = t[i][j];
		}
	}
	if (matrix3Debug) printArray(r,"mult:r=");
}

void multArrayVector(double a[3][3], double v[3], double r[3]) {
	double t[3];
	int i, j;

	for (i=0; i<3; i++) {
		t[i] = 0;
		for (j=0; j<3; j++) {
			t[i] += a[i][j]*v[j];
		}
	}
	for (i=0; i<3; i++) {
		r[i] = t[i];
	}
}

double determinant(double a[3][3]) {
	return(a[0][0] * (a[1][1]*a[2][2] - a[2][1]*a[1][2]) +
			a[0][1] * (a[1][2]*a[2][0] - a[1][0]*a[2][2]) +
			a[0][2] * (a[1][0]*a[2][1] - a[1][1]*a[2][0]));
}

/* Use Kramer's Rule invert a 3X3 matrix */
int invertArray(double a[3][3], double r[3][3]) {
	double t[3][3];
	double det = determinant(a);
	int i, j;

	if (fabs(det) > SMALL) {
		t[0][0] = a[1][1]*a[2][2]-a[1][2]*a[2][1];
		t[0][1] = -(a[0][1]*a[2][2]-a[2][1]*a[0][2]);
		t[0][2] = a[0][1]*a[1][2]-a[1][1]*a[0][2];
		t[1][0] = -(a[1][0]*a[2][2]-a[1][2]*a[2][0]);
		t[1][1] = a[0][0]*a[2][2]-a[2][0]*a[0][2];
		t[1][2] = -(a[0][0]*a[1][2]-a[1][0]*a[0][2]);
		t[2][0] = a[1][0]*a[2][1]-a[2][0]*a[1][1];
		t[2][1] = -(a[0][0]*a[2][1]-a[2][0]*a[0][1]);
		t[2][2] = a[0][0]*a[1][1]-a[0][1]*a[1][0];
	} else {
		if (matrix3Debug) printf("invertArray: determinant %f is too small\n", det);
		return(-1);
	}

	for (i=0; i<3; i++) {
		for (j=0; j<3; j++) {
			r[i][j] = t[i][j]/det;
		}
	}

	return(0);
}


