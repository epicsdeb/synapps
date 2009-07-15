/* File to interlock access to Handel library in a multi-threaded environment */
#include <epicsMutex.h>

#define NUM_DXP_SCAS 16
typedef enum {
   DXP_SATURN,
   DXP_4C2X,
   DXP_XMAP
} DXP_MODULE_TYPE;
/* This must be the last value in the enum */
#define MAX_DXP_MODULE_TYPE DXP_XMAP

/* Global mutex */
extern epicsMutexId epicsHandelMutexId;

/* Function prototypes */
int epicsHandelLock();
int epicsHandelUnlock();
int dxpGetModuleType();

