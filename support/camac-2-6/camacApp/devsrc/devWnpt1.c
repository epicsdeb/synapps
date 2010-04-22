/* devWNPT1.c */
/* base/src/drv $Id: devWnpt1.c,v 1.1.1.1 2001-08-28 20:21:34 rivers Exp $ */
/*
 * subroutines and tasks that are used to interface to the 
 * DSP WNPT1 stepper motor drivers
 *
 * 	Author:      Bob Dalesio
 * 	Date:        03-21-95
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1995, the Regents of the University of California
 *
 *
 * BIG IMPORTANT NOTE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!:
 * The WNPT1 has no support for position readback. We will simulate the readback
 * as a function of the velocity. There is a busy indicator that gives us a
 * readback that it is moving.
 *
 * Modification Log
 * -----------------
 * 08-Jan-1996	Bjo	Ansified for EPICS R3.12
 */

/* 
 * data requests are made from the WNPT1_task at 10Hz when a motor is active
 */
int wnpt1_debug = 0;

/* drvWNPT1.c -  Driver Support Routines for WNPT1 */
#include	<vxWorks.h>
#include	<stdioLib.h>
#include 	<sysLib.h>             /* library for task  support */
#include	<taskLib.h>
#include	<semLib.h>

#include	<dbDefs.h>
#include        <recSup.h>
#include        <devSup.h>
#include        <link.h>
#include	<devWNPT1.h>
#include	<steppermotor.h>
#include        <steppermotorRecord.h>
#include	<taskwd.h>
#include	<camacLib.h>

/* Create the dset for  */
long wnpt1_command();
long wnpt1_device_init();

struct {
        long            number;
        DEVSUPFUN       report;
        DEVSUPFUN       init;
        DEVSUPFUN       init_record;
        DEVSUPFUN       get_ioint_info;
        DEVSUPFUN       sm_command;
}devSmWNPT1={
        6,
        NULL,
        wnpt1_device_init,
        NULL,
        NULL,
        wnpt1_command};


/* CAMAC command defines */
#define A0	0
#define A1 	1
#define F1	01
#define F16	16
#define F17	17
#define F27	27

/* some performance statistics */
short		max_delay_per_motor;
short		max_motors_active;
short		max_delay;



/* pointers to a linked list of motor axis to control - one list per axis */
/* new axis are allocated as they are referred to by the database */
struct wnpt1_motor	*pwnpt1s[MAX_WNPT1_AXIS];


/* scan task parameters */
LOCAL SEM_ID		wnpt1_wakeup;	/* wnpt1_task wakeup semaphore */

/* 
 * this motor is updated at 5 Hz. This is done, in part to make the simulated
 * readback easier. The minimum motor rate is 5 pulses per second. The read
 * back is integer - so this makes the minimum increment 1 - there will be
 * rounding errors that are handled when the move is done
 */
#define WNPT1_TASK_RATE	5

/* scan task for the wnpt1
 */
void wnpt1_task()
{
    short		number_active;
    struct wnpt1_motor	*pwnpt1;
    int			bank1_id;
    int			axis;
    int			q, dummy;
    int			(*psmcb_routine)();

    while(1){
	semTake(wnpt1_wakeup, WAIT_FOREVER);
	number_active = -1;
	while (number_active != 0){
if(wnpt1_debug) printf("%d active motors\n",number_active);
	    number_active = 0;

	    /* give the motor time to move - we are estimating readback */
	    taskDelay(vxTicksPerSecond/WNPT1_TASK_RATE);

	    /* write to each axis */
	    for (axis = 0; axis < MAX_WNPT1_AXIS; axis++){
		pwnpt1 = *(pwnpt1s+axis);
		while (pwnpt1){
			if (pwnpt1->active == 0){ 
				pwnpt1 = pwnpt1->pnext_axis;
				continue;
			}

if (wnpt1_debug) printf("motor %d %d %d %d\n",pwnpt1->link, pwnpt1->crate, pwnpt1->card,axis);
			number_active++;
			/* NOTE: read the status before the position */
			/* the big delay between instructions could  */
			/* cause faulty readbacks if the position is */
			/* read before the status                    */
			cdreg(&bank1_id, pwnpt1->link, pwnpt1->crate, pwnpt1->card,axis);
			cfsa(F27,bank1_id,&dummy,&q);

	    	        /* simulate readback of position data */
		        if (q){ /* not moving */
				pwnpt1->active = pwnpt1->pmotor_data->moving = 0;
				pwnpt1->pmotor_data->motor_position = pwnpt1->final_position;
		        }else{ /* moving */
				pwnpt1->pmotor_data->moving = 1;
		        	pwnpt1->pmotor_data->motor_position += pwnpt1->incr;
			}
		        pwnpt1->pmotor_data->encoder_position = pwnpt1->pmotor_data->motor_position * 4;
if (wnpt1_debug) printf("\tmoving %d position %x\n",pwnpt1->pmotor_data->moving,pwnpt1->pmotor_data->motor_position);
			/* callback the record support */
 			if (pwnpt1->callback != 0){
		    		psmcb_routine = (FUNCPTR)pwnpt1->callback;
				(*psmcb_routine)(pwnpt1->pmotor_data,pwnpt1->callback_arg);
			}
			pwnpt1 = pwnpt1->pnext_axis;
		}
	    }


	} /* while motor active */
    } /* forever */
}

/*
 * WNPT1_DEVICE_INIT
 *
 * initialize all wnpt1 drivers present
 */
long wnpt1_device_init(pass)
int	pass;
{
        short                   i;
	int			taskId;

	if (pass != 0) return 0;

	/* initialize axis link list to all 0's - no axis defined */
	for (i = 0; i < MAX_WNPT1_AXIS; i++) pwnpt1s[i] = 0;

	/* intialize the data request wakeup semaphore */
	if(!(wnpt1_wakeup=semBCreate(SEM_Q_FIFO,SEM_EMPTY)))
		errMessage(0,"semBcreate failed in wnpt1_driver_init");

	/* spawn the motor data request task */
	taskId = taskSpawn("wnpt1_task",42,VX_FP_TASK,8000,(FUNCPTR)wnpt1_task,0,0,0,0,0,0,0,0,0,0);
	taskwdInsert(taskId,NULL,NULL);
	return 0;
}

/* locate wnpt1 axis */
int locate_wnpt1_axis(link,crate,card,axis,ret_pwnpt1)
short	link,crate,card,axis;
struct wnpt1_motor	**ret_pwnpt1;
{
	struct wnpt1_motor	*last_pwnpt1,*new_pwnpt1,*pwnpt1;

	/* does it exist */
	if ((axis < 1) || (axis > 15)) return(-1);
	pwnpt1 = *(pwnpt1s+axis);
	last_pwnpt1 = pwnpt1;
	while(pwnpt1 != 0){
		if ((pwnpt1->link == link) && (pwnpt1->crate == crate) && (pwnpt1->card == card)){
			*ret_pwnpt1 = pwnpt1;
			return(0);
		}
		last_pwnpt1 = pwnpt1;
		pwnpt1 = last_pwnpt1->pnext_axis;
	}

	/* create one */
	new_pwnpt1 = (struct wnpt1_motor *)calloc(1,sizeof(struct wnpt1_motor));
	if (new_pwnpt1 == 0) return(-1);
	new_pwnpt1->pmotor_data = (struct motor_data *)calloc(1,sizeof(struct motor_data));
	if (new_pwnpt1->pmotor_data == 0) return(-1);

	/* link it into the list for this axis */
	if (pwnpt1 == last_pwnpt1){
		*(pwnpt1s+axis) = new_pwnpt1;
	}else{
		last_pwnpt1->pnext_axis = new_pwnpt1;
	}
	*ret_pwnpt1 = new_pwnpt1;

	/* initialize it */
	new_pwnpt1->link = link;
	new_pwnpt1->crate = crate;
	new_pwnpt1->card = card;
	new_pwnpt1->active = 0;
	new_pwnpt1->callback = 0;
	new_pwnpt1->callback_arg = 0;
	new_pwnpt1->pnext_axis = 0;

	return(0);
}


/*
 * WNPT1_DRIVER
 *
 * interface routine called from the database library
 */
int wnpt1_cmd_debug = 0;
long wnpt1_command(psm,command,arg1,arg2)
    struct steppermotorRecord   *psm;
    short command;
    int arg1;
    int arg2;
{
	int			branch,crate,card,axis;
	struct wnpt1_motor	*pwnpt1 = 0;
	int			abs_val_change;
	int			ext;	/* packed camac crate address */
	int			q, value;

	branch = psm->out.value.camacio.b;
	crate = psm->out.value.camacio.c;
	card = psm->out.value.camacio.n;
	axis = atoi(psm->out.value.camacio.parm);

	/* find the axis */
	if (locate_wnpt1_axis (branch, crate, card, axis, &pwnpt1) != 0) return(-1);
if (wnpt1_cmd_debug) printf("\n cmd %d b:%d c:%d n:%d s:%d a1:%x a2:%x ptr %x\n",command,branch,crate,card,axis,arg1,arg2,(int)pwnpt1);

/*	(is it there and communicating ) */

	switch (command){
	case (SM_MODE):			/* only supports positional mode */
		break;
	case (SM_VELOCITY):
		/* set the velocity and acceleration */
		/* another note of importance:	     */
		/* - slew and acceleration are set with position 	*/
		/* - there are only 15 rates - determine the closest one that is lower */
		arg1 = pwnpt1->pmotor_data->velocity = psm->velo;
		pwnpt1->pmotor_data->accel = psm->accl;
		if      (arg1 <=    9){
			pwnpt1->velocity = 0x000000;
			pwnpt1->set_velocity = 5;
		}else if (arg1 <=   19){
			 pwnpt1->velocity = 0x010000;
			pwnpt1->set_velocity = 10;
		}else if (arg1 <=   39) {
			pwnpt1->velocity = 0x020000;
			pwnpt1->set_velocity = 20;
		}else if (arg1 <=   79) {
			pwnpt1->velocity = 0x030000;
			pwnpt1->set_velocity = 40;
		}else if (arg1 <=  159) {
			pwnpt1->velocity = 0x040000;
			pwnpt1->set_velocity = 80;
		}else if (arg1 <=  319) {
			pwnpt1->velocity = 0x050000;
			pwnpt1->set_velocity = 160;
		}else if (arg1 <=  639) {
			pwnpt1->velocity = 0x060000;
			pwnpt1->set_velocity = 320;
		}else if (arg1 <=  999) {
			pwnpt1->velocity = 0x070000;
			pwnpt1->set_velocity = 640;
		}else if (arg1 <= 1499) {
			pwnpt1->velocity = 0x800000;
			pwnpt1->set_velocity = 1000;
		}else if (arg1 <= 1999) {
			pwnpt1->velocity = 0x810000;
			pwnpt1->set_velocity = 1500;
		}else if (arg1 <= 2499) {
			pwnpt1->velocity = 0x820000;
			pwnpt1->set_velocity = 2000;
		}else if (arg1 <= 2999) {
			pwnpt1->velocity = 0x830000;
			pwnpt1->set_velocity = 2500;
		}else if (arg1 <= 3499) {
			pwnpt1->velocity = 0x840000;
			pwnpt1->set_velocity = 3000;
		}else if (arg1 <= 3999) {
			pwnpt1->velocity = 0x850000;
			pwnpt1->set_velocity = 3500;
		}else if (arg1 <= 4999) {
			pwnpt1->velocity = 0x860000;
			pwnpt1->set_velocity = 4000;
		}else		      {
			pwnpt1->velocity = 0x870000;
			pwnpt1->set_velocity = 5000;
		}

		break;

	case (SM_MOVE):
		if (arg1 != 0){
			/* card limits teh pulses per command */
			if (arg1 > 9999) arg1 = 9999;
			else if (arg1 < -9999) arg1 = -9999;

			/* build the card command */
			value = 0;

			/* direction */
			abs_val_change = arg1;
			if (abs_val_change < 0){
				abs_val_change = -abs_val_change;
				value |= 0x80000;
			}
		
			/* velocity */
			value |= pwnpt1->velocity ;

			/* change in position */
			value |= (abs_val_change & 0xffff);

			/* send it */
			cdreg(&ext, branch, crate, card, axis);
			cfsa(F16, ext, &value, &q);

			/* compute position change per scan */
			pwnpt1->incr = pwnpt1->set_velocity / WNPT1_TASK_RATE;
			if (arg1 < 0) pwnpt1->incr = -pwnpt1->incr;

			/* compute final position */
			pwnpt1->final_position = pwnpt1->pmotor_data->motor_position + arg1;

		}else{
			pwnpt1->final_position = pwnpt1->pmotor_data->motor_position;
			pwnpt1->incr = 0;
		}

		/* set the motor to active */
		pwnpt1->active = TRUE;

		/* wakeup the wnpt1 task */
		semGive(wnpt1_wakeup);

		if (q == 0) return(-1);
		break;

	case (SM_MOTION):
		/* stop motor through the csr - soft abort */
		value = 0;
		cdreg(&ext, branch, crate, card, axis);
		cfsa(F17, ext, &value, &q);
	
		/* wakeup the wnpt1 task */
		pwnpt1->active = TRUE;
		semGive(wnpt1_wakeup);
		break;

	case (SM_CALLBACK):
		if (pwnpt1->callback != 0) return(-1);
		pwnpt1->callback = arg1;
		pwnpt1->callback_arg = arg2;
		break;

	/* reset encoder and motor positions to zero */
	case (SM_SET_HOME):
		/* no hardware support - set faked readback to 0 */
		pwnpt1->pmotor_data->motor_position = 
		   pwnpt1->pmotor_data->encoder_position = 0;
		pwnpt1->final_position = pwnpt1->incr = 0;

		/* set the motor to active */
		pwnpt1->active = TRUE;

		/* wakeup the wnpt1 task */
		semGive(wnpt1_wakeup);

		break;
		
	case (SM_ENCODER_RATIO):
		/* set the encoder ratio */
		/* not needed implemented here.	*/
		break;

	case (SM_READ):
		/* fix parameters for the readback - no change in position */
		pwnpt1->final_position = pwnpt1->pmotor_data->motor_position;
		pwnpt1->incr = 0;

		/* set the motor to active */
		pwnpt1->active = TRUE;

		/* wakeup the wnpt1 task */
		semGive(wnpt1_wakeup);

		break;
	}
	return(0);
}


void wnpt1_io_report(level)
short int level;
{
	struct wnpt1_motor	*pwnpt1;
	struct motor_data	*pmotor_data;
	short			any_present;
	short			axis;
	
	/* write to each axis that is active */
	any_present = 0;
	for (axis = 1; axis < MAX_WNPT1_AXIS; axis++){
		pwnpt1 = *(pwnpt1s+axis);
		while (pwnpt1){
			any_present = 1;

			pmotor_data = pwnpt1->pmotor_data;
	
			printf("SM: WNPT1: B:%d C:%d N:%d A:%d for %s\n",
			    pwnpt1->link,pwnpt1->crate,pwnpt1->card,axis,(char *)pwnpt1->callback_arg);

			if (level > 0){
				printf("\tMoving = %d\tDir = %d\n\t Active %d",
			            pmotor_data->moving,
			            pmotor_data->direction,
				    pwnpt1->active);

				printf("\tVel = %ld pulses/sec(ps) desired: %ld\n",
				    pwnpt1->set_velocity,
				    pmotor_data->velocity);

				printf("\tEncoder Pos = %ld\tMotor Pos = %ld\n", 
         			    pmotor_data->encoder_position,
				    pmotor_data->motor_position);
			}

			pwnpt1 = pwnpt1->pnext_axis;
		}
	}
 }


/* 
 * test routines for the wnpt1 stepper motor driver
 */
void wnpt1_test( int branch, int crate, int card, int axis, int change )
{

	int	a0_id, a1_id, bank1_id, move_id;
	int	dummy, q;
	int	value;

	if (change != 0){
		printf ("read %d,%d,%d axis %d and move %d\n",
			branch, crate, card, axis, change);
	}else{
		printf ("read %d,%d,%d axis %d\n",
			branch, crate, card, axis);
	}

	/* create camac variables */
	cdreg(&a0_id, branch, crate, card, A0);
	cdreg(&a1_id, branch, crate, card, A1);
	cdreg(&move_id, branch, crate, card, axis);

	/* check busy */
	cfsa(27,a0_id,&dummy,&q);
	printf("f27,a0: card ");
	if (q == 0) printf("busy\n");
	else printf("idle\n");
	printf("f27,a(i): axis status:");
	for (axis = 1; axis<=15; axis++){
		cdreg(&bank1_id, branch, crate, card, axis);
		cfsa(27,bank1_id,&dummy,&q);
		if (q == 0) printf(" busy");
		else printf(" idle");
	}
	printf("\n");

	/* reset all axis */
/*	cfsa(F17, a0_id, &dummy, &q);
*/
	/* move it */
	if (change){
		value = 0;

		/* direction */
		if (change < 0){
			change = -change;
			value |= 0x80000;
		}
		
		/* velocity */
		value |= 0x30000;	/* half fast */
		value |= (change & 0xffff);
		cfsa(F16, move_id, &value, &q);
		printf("f16,a(i): change 0x%x q %x\n",value,q);

		/* check busy */
		printf("f27,a(i): axis status:");
		for (axis = 1; axis<=15; axis++){
			cdreg(&bank1_id, branch, crate, card, axis);
			cfsa(27,bank1_id,&dummy,&q);
			if (q == 0) printf(" busy");
			else printf(" idle");
		}
		cfsa(27,a0_id,&dummy,&q);
		printf("\nf27,a0: card ");
		if (q == 0) printf("busy\n");
		else printf("idle\n");
	}
}

void wnpt1_func( int f, int b, int c, int n, int a, int arg )
{

	int	ext, q;
	
	printf ("func %d to: %d,%d,%d axis %d arg %d\n",
			f, b, c, n, a, arg);

	/* create camac variables */
	cdreg(&ext, b, c, n, a);
	cfsa(f,ext,&arg,&q);

}
