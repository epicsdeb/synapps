/* File: devE500.c                     */

/* Device Support Routines for motor record for E500 */
/*
 *      Original Author: Mark Rivers
 *
 * Modification Log:
 * -----------------
 * .01  08-May-2000    mlr     initialized from devPM304.c
 *      ...
 */


#define VERSION 1.00

#include        <string.h>
#include        <epicsMutex.h>
#include        <epicsExport.h>
#include        "motorRecord.h"
#include        "motor.h"
#include        "motordevCom.h"
#include        "drvMotorE500.h"
#include        "camacLib.h"

#define STATIC static
extern struct driver_table E500_access;

#define NINT(f) (long)((f)>0 ? (f)+0.5 : (f)-0.5)
#define MIN(a,b) ((a)<(b) ? (a) : (b))
#define ABS(f) ((f)>0 ? (f) : -(f))


/* ----------------Create the dsets for devE500----------------- */
STATIC struct driver_table *drvtabptr;
STATIC long E500_init(int);
STATIC long E500_init_record(struct motorRecord *);
STATIC long E500_start_trans(struct motorRecord *);
STATIC RTN_STATUS E500_build_trans(motor_cmnd, double *, struct motorRecord *);
STATIC RTN_STATUS E500_end_trans(struct motorRecord *);

struct motor_dset devE500 =
{
    {8, NULL, (DEVSUPFUN)E500_init, (DEVSUPFUN)E500_init_record, NULL},
    motor_update_values,
    E500_start_trans,
    E500_build_trans,
    E500_end_trans
};
epicsExportAddress(dset,devE500);


/* --------------------------- program data --------------------- */
/* This table is used to define the command types */

static msg_types E500_table[] = {
    MOTION,     /* MOVE_ABS */
    MOTION,     /* MOVE_REL */
    MOTION,     /* HOME_FOR */
    MOTION,     /* HOME_REV */
    IMMEDIATE,  /* LOAD_POS */
    IMMEDIATE,  /* SET_VEL_BASE */
    IMMEDIATE,  /* SET_VELOCITY */
    IMMEDIATE,  /* SET_ACCEL */
    IMMEDIATE,  /* GO */
    IMMEDIATE,  /* SET_ENC_RATIO */
    INFO,       /* GET_INFO */
    MOVE_TERM,  /* STOP_AXIS */
    VELOCITY,   /* JOG */
    IMMEDIATE,  /* SET_PGAIN */
    IMMEDIATE,  /* SET_IGAIN */
    IMMEDIATE,  /* SET_DGAIN */
    IMMEDIATE,  /* ENABLE_TORQUE */
    IMMEDIATE,  /* DISABL_TORQUE */
    IMMEDIATE,  /* PRIMITIVE */
    IMMEDIATE,  /* SET_HIGH_LIMIT */
    IMMEDIATE   /* SET_LOW_LIMIT */
};


static struct board_stat **E500_cards;

volatile int devE500Debug = 0;

/* --------------------------- program data --------------------- */


/* initialize device support for E500 stepper motor */
STATIC long E500_init(int after)
{
    long rtnval;

    if (devE500Debug >= 1) { 
        printf("%s(%d):",__FILE__,__LINE__);
        printf("E500_init, after=%d\n", after);
    }
    if (after == 0)
    {
        drvtabptr = &E500_access;
        if (devE500Debug >= 1) { 
            printf("%s(%d):",__FILE__,__LINE__);
            printf("E500_init, calling driver initialization\n");
        }
        (drvtabptr->init)();
    }

    rtnval = motor_init_com(after, *drvtabptr->cardcnt_ptr, drvtabptr, 
                            &E500_cards);
    return(rtnval);
}


/* initialize a record instance */
STATIC long E500_init_record(struct motorRecord *mr)
{
    long rtnval;

    rtnval = motor_init_record_com(mr, *drvtabptr->cardcnt_ptr, 
                                   drvtabptr, E500_cards);

    return(rtnval);
}


/* start building a transaction */
STATIC long E500_start_trans(struct motorRecord *mr)
{
    long rtnval;
    rtnval = motor_start_trans_com(mr, E500_cards);
    return(rtnval);
}


/* end building a transaction */
STATIC RTN_STATUS E500_end_trans(struct motorRecord *mr)
{
    RTN_STATUS rtnval;
    rtnval = motor_end_trans_com(mr, drvtabptr);
    return(rtnval);

}


/* add a part to the transaction */
STATIC RTN_STATUS E500_build_trans(motor_cmnd command, double *parms, struct motorRecord *mr)
{
    struct motor_trans *trans = (struct motor_trans *) mr->dpvt;
    struct mess_node *motor_call;
    struct controller *brdptr;
    struct E500controller *cntrl;
    double dval;
    int ival;
    RTN_STATUS rtnval=OK;
    int card, signal;
    int bcna, bcna8;
    int base, slew, csr;
    int speed, ibase, accel, ijog;
    double djog;


    dval = parms[0];
    ival = NINT(parms[0]);

    motor_call = &(trans->motor_call);
    card = motor_call->card;
    signal = motor_call->signal;
    if (devE500Debug >= 1) { 
        printf("%s(%d):",__FILE__,__LINE__);
        printf("E500_build_trans: card=%d, signal=%d, command=%d, data=%d\n", 
               card, signal, command, ival);
    }
    brdptr = (*trans->tabptr->card_array)[card];
    if (brdptr == NULL)
        return(rtnval = ERROR);

    cntrl = (struct E500controller *) brdptr->DevicePrivate;

    if (E500_table[command] > motor_call->type)
        motor_call->type = E500_table[command];

    if (trans->state != BUILD_STATE)
        return(rtnval = ERROR);

    /* Take a lock so that only 1 thread can be talking to the E500 at
     *  once.  This is essential so that loading and reading a set of registers
     *  is an atomic operation, and to interlock access to global structure
     * "cntrl".
     */
    epicsMutexLock(cntrl->E500Lock);

    /* No need to deal with initialization, premove or postmove strings, 
       E500 does not support */

    csr = cntrl->csr[signal];
    bcna = cntrl->bcna[signal];
    bcna8 = cntrl->bcna8[signal];
    if (devE500Debug >= 1) { 
        printf("%s(%d):",__FILE__,__LINE__);
        printf("E500_build_trans: csr=%x, bcna=%x, bcna8=%x\n", csr, bcna, bcna8);
    }

    switch (command)
    {
    case MOVE_ABS:
        /* The E500 does not have a move absolute command, only move relative.
         * Compute relative move from current position and desired position.
         * We return immediately if the motor is already moving.
         * The position and status are obtained from the cached values from the
         * last poll, which should be valid. */
        if (cntrl->csrRegister[signal] & E500_BUSY) goto done;
        /* Compute amount to move */
        ival = ival - cntrl->posRegister[signal];
        if (ival > 0) 
            cntrl->direction[signal] = 1; 
        else
            cntrl->direction[signal] = -1;
        E500WaitForQ(16, bcna, &ival);
        break;
    case MOVE_REL:
        /* Note that this command is only used in special cases (maybe never)
         * since normal "move relative" commands are turnred into 
         * "move absolute" commands by the record */
        if (cntrl->csrRegister[signal] & E500_BUSY) goto done;
        if (ival > 0) 
            cntrl->direction[signal] = 1; 
        else
            cntrl->direction[signal] = -1;
        E500WaitForQ(16, bcna, &ival);
        break;
    case HOME_FOR:
    case HOME_REV:
        /* The E500 does not have home commands */
        break;
    case LOAD_POS:
        E500WaitForQ(16, bcna8, &ival);
        break;
    case SET_VEL_BASE:
        /* It is very important that the base speed be no more than the slew 
         * speed divided by 2 or the motor won't move.  We can't know what to
         * write for the base speed until the slew speed is set.  Thus we don't
         * write the out the base speed here, we just remember it and program
         * it when the slew speed changes.
         */
        cntrl->base[signal] = ival;
        break;
    case SET_VELOCITY:
        /* Save velocity, needed when programming acceleration */
        slew = ival;
        /* Limit to 16 bits */
        if (slew > 65535) slew=65535;
        cntrl->slew[signal] = slew;
        /* Compute the base velocity.  Can't be more than slew/2 or E500
         * won't move the motor */
        base = (int)MIN(cntrl->base[signal], slew/2.);
        /* The base velocity is programmed in units of 10 steps/second
         * won't move the motor */
        ibase = NINT(base / 10.);
        /* Limit to 8 bits */
        if (ibase > 255) ibase=255;
        /* The base velocity is packed into the CSR.  We don't actually write
         * the CSR here, but we store it so it will be set the next time a GO
         * command is received */
        cntrl->csr[signal] = (ibase  << 16) | (E500_CORRECTION_LIMIT << 8);
        /* The velocity register on the E500 actually contains both slew and
         * acceleration information.  Pack them and write them out */
        accel = cntrl->accel[signal];
        speed = (accel << 16) | slew;
        E500WaitForQ(17, bcna8, &speed);
        break;
    case SET_ACCEL:
        /* Units of acceleration are units/sec/sec = velocity / accel time
         * E500 wants to be programmed in 10 msec units of time
         * We compute time here.  It does not matter if motor record sets
         * slew or acceleration first, since we save both and program each
         * time. */
        slew = cntrl->slew[signal];
        accel = NINT((1000. * slew / dval) / 10.);
        /* Limit to 8 bits */
        if (accel > 255) accel = 255;
        cntrl->accel[signal] = accel;
        speed = (accel << 16) | slew;
        E500WaitForQ(17, bcna8, &speed);
        break;
    case GO:
        /* Build + move. Don't mask LAM */
        csr |= E500_BUILD_FILE | E500_START_FILE_EXEC;
        E500WaitForQ(17, bcna, &csr);
        break;
    case SET_ENC_RATIO:
        /* The E500 does not have the concept of encoder ratio */
        break;
    case GET_INFO:
        /* These commands are not actually done by sending a message, but
           rather they will indirectly cause the driver to read the status
           of all motors */
        break;
    case STOP_AXIS:
        csr |= E500_SOFT_ABORT;
        E500WaitForQ(17, bcna, &csr);
        break;
    case JOG:
        /* E500 does not have a jog command.  Simulate with move absolute
           to the appropriate software limit. */
        /* If motor is already moving return immediately */
        if (cntrl->csrRegister[signal] & E500_BUSY) goto done;
        /* First program jog velocity, which was passed */
        speed = (cntrl->accel[signal] << 16) | ABS(ival);
        E500WaitForQ(17, bcna8, &speed);
        /* Select appropriate limit (in dial units) as the target, convert to
           steps. Use double since these could be very big numbers */
        if (ival > 0) djog = mr->dhlm / mr->mres;
        else          djog = mr->dllm / mr->mres;
        /* We now have the target absolute position in E500 units, but E500
           does not have move absolute command, convert to relative move */
        djog = djog - cntrl->posRegister[signal];
        /* Make sure this is a valid 24 bit number */
        if (djog > 8388607) djog = 8388607;
        if (djog < -8388608) djog = -8388608;
        ijog = (int)djog;
        if (devE500Debug >= 1) { 
            printf("%s(%d):",__FILE__,__LINE__);
            printf("E500_build_trans: jogp=%d\n", ijog);
        }
        /* Program number of steps to move */
        E500WaitForQ(16, bcna, &ijog);
        /* Build + move */
        csr |= E500_BUILD_FILE | E500_START_FILE_EXEC;
        E500WaitForQ(17, bcna, &csr);
        break;
    case SET_PGAIN:
    case SET_IGAIN:
    case SET_DGAIN:
    case ENABLE_TORQUE:
    case DISABL_TORQUE:
        /* The E500 does not support gain or torque commands */
        break;
    case SET_HIGH_LIMIT:
    case SET_LOW_LIMIT:
        /* The E500 does not support limits */
        trans->state = IDLE_STATE;
        break;

    default:
        rtnval = ERROR;
    }

done:
    /* Free the lock */
    epicsMutexUnlock(cntrl->E500Lock);

    return (rtnval);
}
