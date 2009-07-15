/****************************** epicsExport ****************************/
#include "epicsExport.h"
#include "iocsh.h"


volatile int debug_saveData = 0;
volatile int debug_saveDataMsg = 0;
volatile int saveData_MessagePolicy = 0;

epicsExportAddress(int, debug_saveData);
epicsExportAddress(int, debug_saveDataMsg);
epicsExportAddress(int, saveData_MessagePolicy);

/* void saveData_Init(char* fname, char* macros) */
static const iocshArg saveData_Init_Arg0 = { "fname", iocshArgString};
static const iocshArg saveData_Init_Arg1 = { "macros", iocshArgString};
static const iocshArg * const saveData_Init_Args[2] = {&saveData_Init_Arg0, &saveData_Init_Arg1};
static const iocshFuncDef saveData_Init_FuncDef = {"saveData_Init", 2, saveData_Init_Args};
static void saveData_Init_CallFunc(const iocshArgBuf *args) {printf("ERROR: saveData_Init not supported on WIN32\n");}

/* void saveData_PrintScanInfo(char* name) */
static const iocshArg saveData_PrintScanInfo_Arg0 = { "name", iocshArgString};
static const iocshArg * const saveData_PrintScanInfo_Args[1] = {&saveData_PrintScanInfo_Arg0};
static const iocshFuncDef saveData_PrintScanInfo_FuncDef = {"saveData_PrintScanInfo", 1, saveData_PrintScanInfo_Args};
static void saveData_PrintScanInfo_CallFunc(const iocshArgBuf *args) {printf("ERROR: saveData_PrintScanInfo not supported on WIN32\n");}

/* void saveData_Priority(int p) */
static const iocshArg saveData_Priority_Arg0 = { "priority", iocshArgInt};
static const iocshArg * const saveData_Priority_Args[1] = {&saveData_Priority_Arg0};
static const iocshFuncDef saveData_Priority_FuncDef = {"saveData_Priority", 1, saveData_Priority_Args};
static void saveData_Priority_CallFunc(const iocshArgBuf *args) {printf("ERROR: saveData_Priority not supported on WIN32\n");}

/* void saveData_SetCptWait_ms(int ms) */
static const iocshArg saveData_SetCptWait_ms_Arg0 = { "ms", iocshArgInt};
static const iocshArg * const saveData_SetCptWait_ms_Args[1] = {&saveData_SetCptWait_ms_Arg0};
static const iocshFuncDef saveData_SetCptWait_ms_FuncDef = {"saveData_SetCptWait_ms", 1, saveData_SetCptWait_ms_Args};
static void saveData_SetCptWait_ms_CallFunc(const iocshArgBuf *args) {printf("ERROR: saveData_SetCptWait_ms not supported on WIN32\n");}

/* void saveData_Version(void) */
static const iocshFuncDef saveData_Version_FuncDef = {"saveData_Version", 0, NULL};
static void saveData_Version_CallFunc(const iocshArgBuf *args) {printf("ERROR: saveData_Version not supported on WIN32\n");}

/* void saveData_CVS(void) */
static const iocshFuncDef saveData_CVS_FuncDef = {"saveData_CVS", 0, NULL};
static void saveData_CVS_CallFunc(const iocshArgBuf *args) {printf("ERROR: saveData_CVS not supported on WIN32\n");}

/* void saveData_Info(void) */
static const iocshFuncDef saveData_Info_FuncDef = {"saveData_Info", 0, NULL};
static void saveData_Info_CallFunc(const iocshArgBuf *args) {printf("ERROR: saveData_Info not supported on WIN32\n");}

/* collect all functions */
void saveDataRegistrar(void)
{
    iocshRegister(&saveData_Init_FuncDef, saveData_Init_CallFunc);
    iocshRegister(&saveData_PrintScanInfo_FuncDef, saveData_PrintScanInfo_CallFunc);
    iocshRegister(&saveData_Priority_FuncDef, saveData_Priority_CallFunc);
    iocshRegister(&saveData_SetCptWait_ms_FuncDef, saveData_SetCptWait_ms_CallFunc);
    iocshRegister(&saveData_Version_FuncDef, saveData_Version_CallFunc);
    iocshRegister(&saveData_CVS_FuncDef, saveData_CVS_CallFunc);
    iocshRegister(&saveData_Info_FuncDef, saveData_Info_CallFunc);
}

epicsExportRegistrar(saveDataRegistrar);

