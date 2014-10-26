/* System includes */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#ifdef vxWorks
extern int logMsg(char *fmt, ...);
#endif

/* EPICS includes */
#include <epicsTypes.h>
#include <epicsExport.h>
#include <iocsh.h>

typedef struct {
	ushort mask;
	void *userPvt;
} softGlueIntRoutineData;

int softGlueRegisterInterruptRoutine(ushort carrier, ushort slot, int sopcAddress, ushort mask,
	void (*routine)(softGlueIntRoutineData *IRData), void *userPvt);
epicsUInt16 *softGlueCalcSpecifiedRegisterAddress(epicsUInt16 carrier, epicsUInt16 slot, int addr);
