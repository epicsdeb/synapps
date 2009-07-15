/* calcRegister.c */

#include <iocsh.h>
#include <epicsExport.h>

/* This is where any registration calls that are not in their respective
 * source code modules should be placed
 */

void calcRegister(void)
{
}

epicsExportRegistrar(calcRegister);
