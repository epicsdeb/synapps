/* File: drvMotorE500.h          */

/* Device Driver Support definitions for motor
 *
 *      Original Author: Mark Rivers
 *      Current Author: Mark Rivers
 *
 * Modification Log:
 * -----------------
 * .01  08-May-2000  mlr  initialized from drvPM304.h
 */

#ifndef INCdrvMotorE500h
#define INCdrvMotorE500h 1

#include "motordrvCom.h"

#define E500_NUM_CHANNELS        8
#define E500_CORRECTION_LIMIT  100  /* Hardcoded correction limit */

/* CSR readback bit definitions */
#define E500_CCW_LIMIT          0x80
#define E500_CW_LIMIT           0x40
#define E500_BUSY               0x04

/* CSR write bit definitions */
#define E500_STOP               0x80
#define E500_SOFT_ABORT         0x40
#define E500_BUILD_FILE         0x04
#define E500_START_FILE_EXEC    0x02
#define E500_LAM_MASK           0x01

#define E500_COMM_ERR           0x01

#define E500_TIMEOUT            5  /* Clock ticks to wait for Q=1 */

struct E500controller
{
    epicsMutexId E500Lock;
    int branch;
    int crate;
    int slot;
    int lam;
    int bcna[E500_NUM_CHANNELS];   /* Packed address */
    int bcna8[E500_NUM_CHANNELS];  /* Packed address, A+8 */
    int posRegister[E500_NUM_CHANNELS];  /* Position registers */
    int csrRegister[E500_NUM_CHANNELS];  /* CSR registers */
    int csr[E500_NUM_CHANNELS];    /* Cached CSR */
    int base[E500_NUM_CHANNELS];   /* Cached base velocity */
    int slew[E500_NUM_CHANNELS];   /* Cached velocity */
    int accel[E500_NUM_CHANNELS];  /* Cached acceleration */
    int direction[E500_NUM_CHANNELS];  /* +1 or -1 for sign of last motion */
    int status;
};

/* Global function, used by both driver and device support */
int E500WaitForQ(int f, int ext, int *val);

#endif  /* INCdrvE500h */
