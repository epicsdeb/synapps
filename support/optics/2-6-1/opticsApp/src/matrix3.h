/* matrix3.h - general 3x3-matrix/3-vector functions */
void printVector(double v[3], char *name);
void print4Vector(double v[4], char *name);
void printArray(double a[3][3], char *name);
double dot(double a[3], double b[3]);
void cross(double a[3], double b[3], double r[3]);
double dotcross(double a[3], double b[3], double c[3]);
void multArrayArray(double a[3][3], double b[3][3], double r[3][3]);
void multArrayVector(double a[3][3], double v[3], double r[3]);
double determinant(double a[3][3]);
int invertArray(double a[3][3], double r[3][3]);

#define SMALL 1.e-11
