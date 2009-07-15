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

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) > (b) ? (b) : (a))
#define NINT(f)  (int)((f)>0 ? (f)+0.5 : (f)-0.5)


volatile int arrayTestDebug=0;

static long arrayTest_init(aSubRecord *pasub)
{
	long *e = (long *)pasub->e;

	if (*e == 0) *e = (long)pasub->nova;
	return(0);
}

static long arrayTest_do(aSubRecord *pasub)
{
	double	*a, *valb, *vala;
	long	i, *e;

	a = (double *)pasub->a;
	valb = (double *)pasub->valb;
	vala = (double *)pasub->vala;
	e = (long *)pasub->e;
	if (*e > pasub->nova) *e = (long)pasub->nova;
	for (i=0; i<*e; i++) {
		vala[i] = *a+i;
		valb[i] = i;
		if (arrayTestDebug) printf("arrayTest: vala[%ld]=%f\n", i, vala[i]);
	}
	return(0);
}

#include <registryFunction.h>
#include <epicsExport.h>

epicsExportAddress(int, arrayTestDebug);

static registryFunctionRef arrayTestRef[] = {
	{"arrayTest_init", (REGISTRYFUNCTION)arrayTest_init},
	{"arrayTest_do", (REGISTRYFUNCTION)arrayTest_do}
};

static void arrayTestRegister(void) {
	registryFunctionRefAdd(arrayTestRef, NELEMENTS(arrayTestRef));
}

epicsExportRegistrar(arrayTestRegister);
