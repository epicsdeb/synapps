/*
 *      drvMotorE500.c
 *      Motor record driver level support for DSP E500 motor controller.
 *
 *      Original Author: Mark Rivers
 *      Date: 10-May-2000
 *
 * Modification Log:
 * -----------------
 * .01  10-May-2000     mlr     initialized from drvMM4000
 * .02  16-Sept-2000    mlr     Fixed bug in start_status: if an E500 was 
 *                              missing then all higher numbered cards did 
 *                              not work.
 */


#include        <string.h>
#include        <epicsThread.h>
#include        <epicsMutex.h>
#include        <epicsExport.h>
#include        <drvSup.h>
#include        "motor.h"
#include        "drvMotorE500.h"
#include        "camacLib.h"

#define STATIC static

/* --- Local data. --- */
int E500_num_cards = 0;
volatile int drvE500Debug = 0;

/* Local data required for every driver; see "motordrvComCode.h" */
#include        "motordrvComCode.h"


/*----------------functions-----------------*/
STATIC int recv_mess(int, char *, int);
STATIC RTN_STATUS send_mess(int card, char const *com, char *c);
STATIC void start_status(int card);
STATIC void readE500Status(int card);
STATIC int set_status(int card, int signal);
static long report(int level);
static long init();
STATIC int motor_init();
STATIC void query_done(int, int, struct mess_node *);
STATIC void E500LamCallback(struct E500controller *);

/*----------------functions-----------------*/

struct driver_table E500_access =
{
    motor_init,
    motor_send,
    motor_free,
    motor_card_info,
    motor_axis_info,
    &mess_queue,
    &queue_lock,
    &free_list,
    &freelist_lock,
    &motor_sem,
    &motor_state,
    &total_cards,
    &any_motor_in_motion,
    send_mess,
    recv_mess,
    set_status,
    query_done,
    start_status,
    &initialized
};


struct
{
    long number;
#ifdef __cplusplus
    long (*report) (int);
    long (*init) (void);
#else
    DRVSUPFUN report;
    DRVSUPFUN init;
#endif
} drvE500 = {2, report, init};

epicsExportAddress(drvet, drvE500);
STATIC struct thread_args targs = {SCAN_RATE, &E500_access};

/*********************************************************
 * Print out driver status report
 *********************************************************/
static long report(int level)
{
    int card;

    if (E500_num_cards <=0)
        printf("    No E500 controllers configured.\n");
    else
    {
        for (card = 0; card < E500_num_cards; card++)
        {
            struct controller *brdptr = motor_state[card];

            if (brdptr == NULL)
                printf("    E500 controller %d connection failed.\n", card);
            else
            {
                struct E500controller *cntrl;

                cntrl = (struct E500controller *) brdptr->DevicePrivate;
                printf("    E500 controller %d, id:%s, branch=%d crate=%d slot=%d\n",
                           card, brdptr->ident, cntrl->branch, cntrl->crate,
                           cntrl->slot);
            }
        }
    }
    return (0);
}


static long init()
{
   /*
    * We cannot call motor_init() here, because that function can do CAMAC I/O,
    * and hence requires that camacLib have already been initialized.
    * That cannot be guaranteed, so we need to call motor_init from device
    * support
    */
    /* Check for setup */
    if (E500_num_cards <= 0)
    {
        if (drvE500Debug >= 1) {
            printf("%s(%d):",__FILE__,__LINE__);
            printf("init(): E500 driver disabled. E500Setup() missing from startup script.\n");
        }
    }
    return ((long) 0);
}


STATIC void query_done(int card, int axis, struct mess_node *nodeptr)
{
}


/*********************************************************
 * Read the status and position of all motors on a card
 * start_status(int card)
 *            if card == -1 then start all cards
 *********************************************************/
STATIC void start_status(int card)
{
    int itera;

    if (drvE500Debug >= 2) {
        printf("%s(%d):",__FILE__,__LINE__);
        printf("start_status: card=%d\n", card);
    }
    if (card >= 0)
    {
        readE500Status(card);
    }
    else
    {
        for (itera = 0; itera < total_cards; itera++) {
            if (motor_state[itera]) readE500Status(itera);
        }
    }
}

/*********************************************************
 * Read the E500 for the position and status of all motors on a card
 *********************************************************/
STATIC void readE500Status(int card)
{
    struct E500controller *cntrl =
                (struct E500controller *) motor_state[card]->DevicePrivate;
    int dummy=0;
    int i;
    int status;

    /* Take a lock so that only 1 thread can be talking to the E500 at
     *  once.  This is essential so that loading and reading a set of registers
     *  is an atomic operation.  Also, we are accessing the global data
     *  structure "cntrl", and need to lock access to that.
     */
    epicsMutexLock(cntrl->E500Lock);

    /* Load the position registers, F25 A0*/
    status = E500WaitForQ(25, cntrl->bcna[0], &dummy);

    /* Read position registers */
    for (i=0; i<E500_NUM_CHANNELS; i++) {
        status = E500WaitForQ(0, cntrl->bcna8[i], &cntrl->posRegister[i]);
    }

    /* Load the CSR registers, F25 A1*/
    status = E500WaitForQ(25, cntrl->bcna[1], &dummy);

    /* Read CSR registers */
    for (i=0; i<E500_NUM_CHANNELS; i++) {
        status = E500WaitForQ(0, cntrl->bcna[i], &cntrl->csrRegister[i]);
    }

    if (status == ERROR)
        cntrl->status = E500_COMM_ERR;
    else
        cntrl->status = OK;

    /* Free the lock */
    epicsMutexUnlock(cntrl->E500Lock);
}


/**************************************************************
 * Parse status and position for a card and signal
 * set_status()
 ************************************************************/

STATIC int set_status(int card, int signal)
{
    struct E500controller *cntrl;
    struct mess_node *nodeptr;
    register struct mess_info *motor_info;

    int pos, csr;
    int rtn_state;
    double motorData;
    bool ls_active = false;
    msta_field status;

    cntrl = (struct E500controller *) motor_state[card]->DevicePrivate;
    motor_info = &(motor_state[card]->motor_info[signal]);
    nodeptr = motor_info->motor_motion;
    status.All = motor_info->status.All;

    /* Lock access to global data structure */
    epicsMutexLock(cntrl->E500Lock);

    if (drvE500Debug >= 2) {
        printf("%s(%d):",__FILE__,__LINE__);
        printf("E500:set_status entry: card=%d, signal=%d\n", card, signal);
    }
    /* Parse the status from the CSR and position */
    csr = cntrl->csrRegister[signal];
    pos = cntrl->posRegister[signal];
    if (drvE500Debug >= 2) {
        printf("%s(%d):",__FILE__,__LINE__);
        printf("E500:set_status entry: csr=%x, pos=%x(%d)\n", csr, pos, pos);
    }

    /* If the position is negative sign extend position to 32 bits */
    if (pos & 0x800000) pos = pos | 0xff000000;

    if (cntrl->direction[signal] == 1)
        status.Bits.RA_DIRECTION = 1;
    else
        status.Bits.RA_DIRECTION=0;

    if (csr & E500_BUSY)
        status.Bits.RA_DONE=0;
    else
        status.Bits.RA_DONE=1;

    status.Bits.RA_PLUS_LS=0;
    status.Bits.RA_MINUS_LS=0;
    if (csr & E500_CCW_LIMIT) {
        status.Bits.RA_MINUS_LS=1;
        ls_active = true;
    }

    if (csr & E500_CW_LIMIT) {
        status.Bits.RA_PLUS_LS=1;
        ls_active = true;
    }

    /* No home */
    status.Bits.RA_HOME=0;

    status.Bits.EA_POSITION=0;

    /* encoder status */
    status.Bits.EA_SLIP=0;
    status.Bits.EA_SLIP_STALL=0;
    status.Bits.EA_HOME=0;

    /* Motor position */
    motorData = pos;

    if (motorData == motor_info->position)
        motor_info->no_motion_count++;
    else
    {
        motor_info->position = NINT(motorData);
        if (motor_state[card]->motor_info[signal].encoder_present == YES)
            motor_info->encoder_position = (int32_t) motorData;
        else
            motor_info->encoder_position = 0;

        motor_info->no_motion_count = 0;
    }

    status.Bits.RA_PROBLEM=0;

    /* Parse motor velocity? */
    /* NEEDS WORK */

    motor_info->velocity = 0;

    if (status.Bits.RA_DIRECTION==0)
        motor_info->velocity *= -1;

    rtn_state = (!motor_info->no_motion_count || ls_active == true ||
                 (status.Bits.RA_DONE | status.Bits.RA_PROBLEM)) ? 1 : 0;

    /* E500 does not support post move string. */

    if (cntrl->status == E500_COMM_ERR)
        status.Bits.CNTRL_COMM_ERR=1;
    else
        status.Bits.CNTRL_COMM_ERR=0;

    /* Free the lock */
    epicsMutexUnlock(cntrl->E500Lock);
    motor_info->status.All = status.All;
    return(rtn_state);
}


/*****************************************************/
/* send a message to the E500 board                  */
/* send_mess()                                       */
/*****************************************************/
STATIC RTN_STATUS send_mess(int card, char const *com, char *inchar)
{
    /* This is a no-op for the E500, but must be present */
    return (OK);
}


/*****************************************************/
/* receive a message from the E500 board           */
/* recv_mess()                                       */
/*****************************************************/
STATIC int recv_mess(int card, char *com, int flag)
{
    /* This is a no-op for the E500, but must be present */
    /* Set the returned message to a null string */
    *com = '\0';
    return (0);
}


/*****************************************************/
/* Do a CAMAC transaction with E500, wait for Q      */
/* E500WaitForQ                                      */
/*****************************************************/
int E500WaitForQ(int f, int bcna, int *data)
{
    int q;
    int cntr=0;

    cfsa(f, bcna, data, &q);
    while ((q == 0) && (cntr < E500_TIMEOUT)) {
        cntr++;
        epicsThreadSleep(.01);
        cfsa(f, bcna, data, &q);
    }
    if (drvE500Debug >= 1) {
        printf("%s(%d):",__FILE__,__LINE__);
        printf("E500WaitForQ: f=%d, bcna=%x, data=%d (%x), q=%d\n",
               f, bcna, *data, *data, q);
    }
    if (cntr >= E500_TIMEOUT)
        return(ERROR);
    else
        return(OK);
}


/*****************************************************/
/* Setup system configuration                        */
/* E500Setup()                                     */
/*****************************************************/
RTN_STATUS E500Setup(int num_cards,    /* maximum number of controllers in system.  */
            int num_channels,   /* NOT Used. */
            int scan_rate)      /* polling rate - 1/60 sec units.  */
{
    int itera;

    if (num_cards < 1)
        E500_num_cards = 1;
    else
        E500_num_cards = num_cards;

    /* Set motor polling task rate */
    if (scan_rate >= 1 && scan_rate <= 60)
        targs.motor_scan_rate = scan_rate;
    else
        targs.motor_scan_rate = SCAN_RATE;

   /* Allocate space for pointers to motor_state structures. */
    motor_state = (struct controller **) malloc(E500_num_cards *
                                                sizeof(struct controller *));

    for (itera = 0; itera < E500_num_cards; itera++)
        motor_state[itera] = (struct controller *) NULL;

    return (OK);
}


/*****************************************************/
/* Configure a controller                            */
/* E500Config()                                    */
/*****************************************************/
int E500Config(int card,        /* card being configured */
            int branch,         /* CAMAC branch */
            int crate,          /* CAMAC crate */
            int slot)           /* CAMAC slot */
{
    struct E500controller *cntrl;
    int i;

    if (drvE500Debug >= 1) {
        printf("%s(%d):",__FILE__,__LINE__);
        printf("E500Config: card=%d, branch=%d, crate=%d, slot=%d\n",
               card, branch, crate, slot);
    }
    if (card < 0 || card >= E500_num_cards)
        return (ERROR);

    motor_state[card] = (struct controller *) malloc(sizeof(struct controller));
    motor_state[card]->DevicePrivate = malloc(sizeof(struct E500controller));
    cntrl = (struct E500controller *) motor_state[card]->DevicePrivate;

    cntrl->branch = branch;
    cntrl->crate = crate;
    cntrl->slot = slot;
    /* Create packed addresses for each motor */
    for (i=0; i<E500_NUM_CHANNELS; i++) {
        cdreg(&cntrl->bcna[i], branch, crate, slot, i);
        cdreg(&cntrl->bcna8[i], branch, crate, slot, i+8);
    }
    if (drvE500Debug >= 1) {
        printf("%s(%d):",__FILE__,__LINE__);
        printf("E500Config: bcna[0]=%x\n", cntrl->bcna[0]);
    }
    return (0);
}



/*****************************************************/
/* initialize all software and hardware              */
/* This is called from the initialization routine in */
/* device support.                                   */
/* motor_init()                                      */
/*****************************************************/
STATIC int motor_init()
{
    struct controller *brdptr;
    struct E500controller *cntrl;
    int card_index, motor_index;
    int dummy=0;
    int status;
    lamParams lamParams;
    void *inta[2];
    bool errind;

    initialized = true;   /* Indicate that driver is initialized. */

    /* Check for setup */
    if (drvE500Debug >= 1) {
        printf("%s(%d):",__FILE__,__LINE__);
        printf("E500:motor_init: num_cards=%d\n", E500_num_cards);
    }
    if (E500_num_cards <= 0)
        return (ERROR);

    for (card_index = 0; card_index < E500_num_cards; card_index++)
    {
        if (!motor_state[card_index])
            continue;

        brdptr = motor_state[card_index];
        total_cards = card_index + 1;
        cntrl = (struct E500controller *) brdptr->DevicePrivate;
        cntrl->E500Lock = epicsMutexCreate();

        /* Initialize communications channel */
        /* Read module to make sure it exists - make sure it is really an
         * E500 by reading the module ID. It should be 500. */
        errind = false;
        status = E500WaitForQ(25, cntrl->bcna[2], &dummy);
        status = E500WaitForQ(1, cntrl->bcna[0], &dummy);
        if ((status != OK) || ((dummy & 0xfff) != 500)) errind = true;
        if (drvE500Debug >= 1) {
            printf("%s(%d):",__FILE__,__LINE__);
            printf("E500:motor_init: card_index=%d, errind=%d\n",
                   card_index, errind);
        }

        if (errind == false)
        {
            brdptr->localaddr = (char *) NULL;
            brdptr->motor_in_motion = 0;

            brdptr->total_axis = E500_NUM_CHANNELS;
            for (motor_index = 0; motor_index<E500_NUM_CHANNELS; motor_index++)
                    brdptr->motor_info[motor_index].motor_motion = NULL;

            strcpy(brdptr->ident, "E500 controller");

            /* Set up LAMs for this controller */
            inta[0] = &lamParams;
            inta[1] = cntrl;
            lamParams.a_test = 0;
            lamParams.f_test = -1;
            lamParams.a_clear = 0;
            lamParams.f_clear = -1;
            lamParams.a_enable = 0;
            lamParams.f_enable = -1;
            lamParams.mask_enable = 0;
            lamParams.a_disable = 0;
            lamParams.f_disable = -1;
            lamParams.mask_disable = 0;
            /* Initialize LAM */
            cdlam(&cntrl->lam, cntrl->branch, cntrl->crate, cntrl->slot,
                            0, inta);
            /* Link callback function */
            cclnk(cntrl->lam, (FUNCPTR) E500LamCallback);
            /* Enable LAM */
            cclm(cntrl->lam, 1);

            start_status(card_index);
            if (drvE500Debug >= 1) {
                printf("%s(%d):",__FILE__,__LINE__);
                printf("E500:motor_init: called start_status OK\n");
            }
            for (motor_index = 0; motor_index<E500_NUM_CHANNELS; motor_index++)
            {
                struct mess_info *motor_info = &brdptr->motor_info[motor_index];

                motor_info->status.All = 0;
                motor_info->no_motion_count = 0;
                motor_info->encoder_position = 0;
                motor_info->position = 0;
                /* Initialize reasonable defaults for cached values. The
                   record should overwrite these  */
                cntrl->csr[motor_index]=0;
                cntrl->slew[motor_index] = 100;
                cntrl->accel[motor_index] = 50;
                /* Read status of each motor */
                if (drvE500Debug >= 1) {
                    printf("%s(%d):",__FILE__,__LINE__);
                    printf("  E500:motor_init: calling set_status for motor %d\n",
                           motor_index);
                }
                set_status(card_index, motor_index);
            }
        }
        else
            motor_state[card_index] = (struct controller *) NULL;
    }
    if (drvE500Debug >= 1) {
        printf("%s(%d):",__FILE__,__LINE__);
        printf("E500:motor_init: done with start_status and set_status\n");
    }

    any_motor_in_motion = 0;

    mess_queue.head = (struct mess_node *) NULL;
    mess_queue.tail = (struct mess_node *) NULL;

    free_list.head = (struct mess_node *) NULL;
    free_list.tail = (struct mess_node *) NULL;

    if (drvE500Debug >= 1) {
        printf("%s(%d):",__FILE__,__LINE__);
        printf("E500:motor_init: spawning E500_motor task\n");
    }
    epicsThreadCreate((char *) "E500_motor", epicsThreadPriorityMedium,
                       epicsThreadGetStackSize(epicsThreadStackSmall), 
                       (EPICSTHREADFUNC) motor_task, (void *) &targs);

    if (drvE500Debug >= 1) {
        printf("%s(%d):",__FILE__,__LINE__);
        printf("E500:motor_init: returning\n");
    }
    return (OK);
}


/*****************************************************/
/* Callback when E500 gets a LAM                     */
/* E500LamCallback()                                 */
/*****************************************************/
STATIC void E500LamCallback(struct E500controller *cntrl)
{
    int i, csr, csr0, status, dummy, mask;

    /* Lock the global data */
    epicsMutexLock(cntrl->E500Lock);

    if (drvE500Debug >= 1) {
        printf("%s(%d):",__FILE__,__LINE__);
        printf("E500LamCallback entry\n");
    }

    /* Load the CSR registers, F25 A1*/
    status = E500WaitForQ(25, cntrl->bcna[1], &dummy);

    /* Read CSR register 0.  This will tell us which channel(s) generated
     * the LAM.  Loop through the channels.  If a channel is asserting LAM
     * but is not busy then it must have completed a motion.  Mask the LAM for
     * this channel */
    status = E500WaitForQ(0, cntrl->bcna[0], &csr0);
    if (drvE500Debug >= 1) {
        printf("%s(%d):",__FILE__,__LINE__);
        printf("E500LamCallback csr[0]=%x\n", csr0);
    }
    mask = 1<<16;
    for (i=0; i<E500_NUM_CHANNELS; i++) {
        if (csr0 & mask) {
            /* This channel is asserting a LAM.
             * Read its CSR and see if it is busy. */
            if (drvE500Debug >= 1) {
                printf("%s(%d):",__FILE__,__LINE__);
                printf("E500LamCallback LAM asserted on channel %d\n", i);
            }
            status = E500WaitForQ(0, cntrl->bcna[i], &csr);
            if ((csr & E500_BUSY) == 0) {
                /* Not busy, so this motor must have stopped. Mask its LAM */
                if (drvE500Debug >= 1) {
                    printf("%s(%d):",__FILE__,__LINE__);
                    printf("E500LamCallback LAM masked on channel %d\n", i);
                }
                csr = cntrl->csr[i];
                csr |= E500_LAM_MASK;
                E500WaitForQ(17, cntrl->bcna[i], &csr);
            }
        }
        mask <<= 1;
    }
    /* Unlock the global data */
    epicsMutexUnlock(cntrl->E500Lock);
}
