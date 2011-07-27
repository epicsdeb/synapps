/* devE500.c */
/* base/src/drv $Id: devE500.c,v 1.1.1.1 2001-08-28 20:21:34 rivers Exp $ */
/*
 * subroutines and tasks that are used to interface to the 
 * DSP E500 stepper motor drivers
 *
 * 	Author:      Bob Dalesio
 * 	Date:        03-21-95
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1995, the Regents of the University of California
 *
 *
 * BIG IMPORTANT NOTE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!:
 *	As there is only one thread setting the mode of a card (issuing an F25) and then
 *	reading the mode dependent registers, this will work. If there is a F25 put into
 *	a different thread of execution, then a mutual exclusion semapohore will be needed
 *	per card (branch,crate,card) so that the mode command (F25) and the read commands
 *	(F0) are protected from another thread overwriting the (F25) for this thread.
 *
 * Modification Log
 * -----------------
 * 08-Jan-1996	Bjo	Ansified for EPICS R3.12
 */

/* 
 * data requests are made from the E500_task at 10Hz when a motor is active
 */
int e500_debug = 0;
int e500_sign = 0;

/* drvE500.c -  Driver Support Routines for E500 */
#include	<vxWorks.h>
#include	<stdioLib.h>
#include 	<sysLib.h>             /* library for task  support */
#include	<taskLib.h>
#include	<semLib.h>

#include	<dbDefs.h>
#include        <recSup.h>
#include        <devSup.h>
#include        <link.h>
#include	<taskwd.h>
#include	<devE500.h>
#include	<steppermotor.h>
#include        <steppermotorRecord.h>
#include	<camacLib.h>

/* Create the dset for  */
long e500_command();
long e500_device_init();
long e500_rec_init();

struct {
        long            number;
        DEVSUPFUN       report;
        DEVSUPFUN       init;
        DEVSUPFUN       init_record;
        DEVSUPFUN       get_ioint_info;
        DEVSUPFUN       sm_command;
}devSmE500={
        6,
        NULL,
        e500_device_init,
        NULL,
        NULL,
        e500_command};


/* CAMAC command defines */
#define A0	0
#define A1 	1
#define	A3	3
#define A4	4
#define F0	0
#define F16	16
#define F17	17
#define F25	25
#define	BANK_2	8

/* some performance statistics */
short		max_delay_per_motor;
short		max_motors_active;
short		max_delay;

/* function prototypes */
int wait_for_q (int, int, int*, struct e500_motor*);


/* pointers to a linked list of motor axis to control - one list per axis */
/* new axis are allocated as they are referred to by the database */
struct e500_motor	*pe500s[MAX_E500_AXIS];


/* scan task parameters */
LOCAL SEM_ID		e500_wakeup;	/* e500_task wakeup semaphore */


/* scan task for the e500
 */
void e500_task()
{
    short		number_active;
    struct e500_motor	*pe500;
    int			cntr;
    int			bank1_id, bank2_id,a0_id,a1_id;
    int			axis;
    int			csr, abs_encoder, dummy;
    int			(*psmcb_routine)();

    while(1){
	semTake(e500_wakeup, WAIT_FOREVER);
	number_active = -1;
	while (number_active != 0){
	    cntr = 0;
if(e500_debug) printf("%d active motors\n",number_active);
	    number_active = 0;

	    /* write to each axis */
	    for (axis = 0; axis < 8; axis++){
		pe500 = *(pe500s+axis);
		while (pe500){
			if (pe500->active != 0){
if (e500_debug) printf("motor %d %d %d %d\n",pe500->link, pe500->crate, pe500->card,axis);
				number_active++;
				/* NOTE: read the status before the position */
				/* the big delay between instructions could  */
				/* cause faulty readbacks if the position is */
				/* read before the status                    */
	    	        	/* set mode to read axis status */
				cdreg(&a1_id, pe500->link, pe500->crate, pe500->card, A1);
		        	cntr += wait_for_q(F25, a1_id, &dummy, pe500);

			}
			pe500 = pe500->pnext_axis;
		}

		pe500 = *(pe500s+axis);
		while (pe500){
			if (pe500->active != 0){
	    	        	/* read and convert axis status */
				cdreg(&bank1_id,pe500->link,pe500->crate,pe500->card,axis);
		       		cntr += wait_for_q(F0,  bank1_id, &csr, pe500);
		        	pe500->pmotor_data->ccw_limit = (csr & E500_CCW_LIMIT) > 0;
		        	pe500->pmotor_data->cw_limit = (csr & E500_CW_LIMIT) > 0;
				if ((csr & E500_BUSY) == 0){
		        		pe500->pmotor_data->moving = 0;
				}else{
		        		pe500->pmotor_data->moving = 1;
				}
if (e500_debug) printf("\tcsr %x",csr);
			}
			pe500 = pe500->pnext_axis;
		}

		pe500 = *(pe500s+axis);
		while (pe500){
			if (pe500->active != 0){
				/* set mode to read absolute encoder */
				cdreg(&a0_id, pe500->link, pe500->crate, pe500->card, A0);
				cntr += wait_for_q(F25, a0_id, &dummy, pe500);
			}
			pe500 = pe500->pnext_axis;
		}

		pe500 = *(pe500s+axis);
		while (pe500){
			if (pe500->active != 0){
				/* read absolute encoder */
				cdreg(&bank2_id,pe500->link,pe500->crate,pe500->card,axis+8);
				cntr += wait_for_q(F0,  bank2_id, &abs_encoder, pe500);
				if (abs_encoder & 0x800000) abs_encoder |= 0xff000000;
		        	pe500->pmotor_data->encoder_position = abs_encoder * 4;
		        	pe500->pmotor_data->motor_position = abs_encoder;

				/* is the motor still active */
				pe500->active = pe500->pmotor_data->moving;

				/* callback the record support */
 				if (pe500->callback != 0){
		    			psmcb_routine = (FUNCPTR)pe500->callback;
					(*psmcb_routine)(pe500->pmotor_data,pe500->callback_arg);
				}
if (e500_debug) printf("\tposition %x\n",abs_encoder);
			}
			pe500 = pe500->pnext_axis;
		}
	    }

	    /* well - here is another important note!!!!!!!!!!!!!!!!!!!!*/
	    /* some of these commands take ~500 usec to execute - so the */
	    /* wait_for_q call may do a task delay of 1 (or more). We    */
	    /* will accumulate these delays and take them off the 10Hz   */
	    /* delay between cycles of reads                             */
	    /* what we are seeing is about 2 ticks per active motor      */
	    /* if there are more than 3 motors moving this will become   */
	    /* slower than 10 Hz on the updates.			 */
	    if (number_active > 0){
		    if ((cntr / number_active) > max_delay_per_motor) 
			max_delay_per_motor = cntr / number_active;
		    if (number_active > max_motors_active)
			max_motors_active = number_active;
		    if (cntr > max_delay)
			max_delay = cntr;
	    }
	    cntr = (vxTicksPerSecond/10) - cntr;
if (e500_debug) printf("sleep %d\n",cntr);
	    if (cntr > 0) taskDelay(cntr);

	} /* while motor active */
    } /* forever */
}

/*
 * E500_DEVICE_INIT
 *
 * initialize all e500 drivers present
 */
long e500_device_init(pass)
int	pass;
{
        short                   i;
	int			taskId;

	if (pass != 0) return 0;

	/* initialize axis link list to all 0's - no axis defined */
	for (i = 0; i < MAX_E500_AXIS; i++) pe500s[i] = 0;

	/* intialize the data request wakeup semaphore */
	if(!(e500_wakeup=semBCreate(SEM_Q_FIFO,SEM_EMPTY)))
		errMessage(0,"semBcreate failed in e500_driver_init");

	/* spawn the motor data request task */
	taskId = taskSpawn("e500_task",42,VX_FP_TASK,8000,(FUNCPTR)e500_task,0,0,0,0,0,0,0,0,0,0);
	taskwdInsert(taskId,NULL,NULL);
	return 0;
}

/* locate e500 axis */
int locate_e500_axis(link,crate,card,axis,ret_pe500)
short	link,crate,card,axis;
struct e500_motor	**ret_pe500;
{
	struct e500_motor	*last_pe500,*new_pe500,*pe500;

	/* does it exist */
	pe500 = *(pe500s+axis);
	last_pe500 = pe500;
	while(pe500 != 0){
		if ((pe500->link == link) && (pe500->crate == crate) && (pe500->card == card)){
			*ret_pe500 = pe500;
			return(0);
		}
		last_pe500 = pe500;
		pe500 = last_pe500->pnext_axis;
	}

	/* create one */
	new_pe500 = (struct e500_motor *)calloc(1,sizeof(struct e500_motor));
	if (new_pe500 == 0) return(-1);
	new_pe500->pmotor_data = (struct motor_data *)calloc(1,sizeof(struct motor_data));
	if (new_pe500->pmotor_data == 0) return(-1);

	/* link it into the list for this axis */
	if (pe500 == last_pe500){
		*(pe500s+axis) = new_pe500;
	}else{
		last_pe500->pnext_axis = new_pe500;
	}
	*ret_pe500 = new_pe500;

	/* initialize it */
	new_pe500->link = link;
	new_pe500->crate = crate;
	new_pe500->card = card;
	new_pe500->active = 0;
	new_pe500->callback = 0;
	new_pe500->callback_arg = 0;
	new_pe500->pnext_axis = 0;

	return(0);
}


/*
 * E500_DRIVER
 *
 * interface routine called from the database library
 */
int fred;
long e500_command(psm,command,arg1,arg2)
    struct steppermotorRecord   *psm;
    short command;
    int arg1;
    int arg2;
{
	int			branch,crate,card,axis;
	struct e500_motor	*pe500 = 0;
	short			count;
	int			bank1_id, bank2_id;
	int			slew_accel, csr, abs_encoder;

	branch = psm->out.value.camacio.b;
	crate = psm->out.value.camacio.c;
	card = psm->out.value.camacio.n;
	axis = atoi(psm->out.value.camacio.parm);

	/* find the axis */
	if (locate_e500_axis (branch, crate, card, axis, &pe500) != 0) return(-1);

/*	(is it there and communicating ) */

	count = 0;
	switch (command){
	case (SM_MODE):			/* only supports positional mode */
		break;
	case (SM_VELOCITY):
if (e500_debug) printf("velocity");

		/* set the velocity and acceleration */
		/* another note of importance:							*/
		/* arg1 is the pulses per second - range is between 5 and 20,000 pps	 	*/
		/* arg2 is the pulses/second/second- and psm->accl is the seconds to velocity 	*/
		/* we can either divide arg2 by motor resolution (MRES) to get seconds to	*/
		/* velocity - then multiply by 100 as the e500 wants 10's of milliseconds to	*/
		/* velocity or just use the database value (seconds to velocity) * 100		*/
		slew_accel = ((int)(psm->accl*100) << 16) + (arg1);    /* pack and order correctly */
		cdreg(&bank2_id, branch, crate, card, axis+8);
		count = wait_for_q(F17, bank2_id, &slew_accel,pe500);
		if (pe500->timed_out) return(-1);

		pe500->pmotor_data->velocity = arg1;
		pe500->pmotor_data->accel = psm->accl * 100;
if (e500_debug) printf("%d %f successful\n",arg1,psm->accl);
		break;

	case (SM_MOVE):
if (e500_debug) printf("move");
		if (arg1 != 0){
			/* move the motor */
			cdreg(&bank1_id, branch, crate, card, axis);

			/* write the new position */
			/* cw */
			if (arg1 > 0){
				pe500->pmotor_data->direction = 1;
			/* ccw */
			}else{
				if (e500_sign==1){
					arg1 = -arg1;
					arg1 |= 0x800000;
				} else if (e500_sign == 2){
					arg1 = -arg1;
					arg1 |= 0xff800000;
				}

				pe500->pmotor_data->direction = 0;
			}
			count += wait_for_q(F16, bank1_id, &arg1, pe500);
			if (pe500->timed_out) return(-1);

			/* wake up through the csr - build file/start file/LAM mask */
			csr = 0x7;
			count += wait_for_q(F17, bank1_id, &csr, pe500);
			if (pe500->timed_out) return(-1);
		}
if (e500_debug) printf("%x successful\n",arg1);
	
		/* set the motor to active */
		pe500->active = TRUE;

		/* wakeup the e500 task */
		semGive(e500_wakeup);

		break;

	case (SM_MOTION):

if (e500_debug) printf("stop");
		/* stop motor through the csr - soft abort */
		csr = E500_STOP;
		cdreg(&bank1_id, branch, crate, card, axis);
		count = wait_for_q(F17, bank1_id, &csr, pe500);
		if (pe500->timed_out) return(-1);
	
		/* wakeup the e500 task */
if (e500_debug) printf("successful\n");
		pe500->active = TRUE;
		semGive(e500_wakeup);
		break;

	case (SM_CALLBACK):
if (e500_debug) printf("callback");
		if (pe500->callback != 0) return(-1);
		pe500->callback = arg1;
		pe500->callback_arg = arg2;
if (e500_debug) printf("successful\n");
		break;

	/* reset encoder and motor positions to zero */
	case (SM_SET_HOME):
if (e500_debug) printf("sethome");
		/* set the absolute position to 0 */
		cdreg(&bank2_id, branch, crate, card, axis+8);
		abs_encoder = 0;
		count = wait_for_q(F16, bank2_id, &abs_encoder, pe500);
		if (pe500->timed_out) return(-1);

if (e500_debug) printf("successful\n");
		/* set the motor to active */
		pe500->active = TRUE;

		/* wakeup the e500 task */
		semGive(e500_wakeup);

		break;
		
	case (SM_ENCODER_RATIO):
		/* set the encoder ratio */
		/* The "ER" command changes how far a pulse will move the */
		/* motor. 						  */
		/* As this is not the desired action this command is not  */
		/* implemented here.					  */
		break;

	case (SM_READ):
if (e500_debug) printf("read");
		/* set the motor to active */
		pe500->active = TRUE;

if (e500_debug) printf("successful\n");
		/* wakeup the e500 task */
		semGive(e500_wakeup);

		break;
	}
	if (count > pe500->max_cmd_wait){
		pe500->max_cmd_wait = count;
		pe500->cmd_waited_for = command;
	}
	return(0);
}


void e500_io_report(level)
short int level;
{
	struct e500_motor	*pe500;
	struct motor_data	*pmotor_data;
	short			any_present;
	short			axis;
	
	/* write to each axis that is active */
	any_present = 0;
	for (axis = 0; axis < MAX_E500_AXIS; axis++){
		pe500 = *(pe500s+axis);
		while (pe500){
			any_present = 1;

			pmotor_data = pe500->pmotor_data;
	
			printf("SM: E500: B:%d C:%d N:%d A:%d for %s\n",
			    pe500->link,pe500->crate,pe500->card,axis,(char *)pe500->callback_arg);

			if (level > 0){
				printf("\tCW limit = %d\tCCW limit = %d\tMoving = %d\tDir = %d\n",
				    pmotor_data->cw_limit,
			            pmotor_data->ccw_limit,
			            pmotor_data->moving,
			            pmotor_data->direction);

				printf("\tVel = %ld pulses/sec(ps)\tTime to velocity %d seconds \n",
				    pmotor_data->velocity,
				    pmotor_data->accel/100);

				printf("\tEncoder Pos = %x\tMotor Pos = %x\n", 
         			    pmotor_data->encoder_position,
				    pmotor_data->motor_position);
			}

			if (level > 1){
				printf("\tMax Wait = %d for CMD %d\n",
				    pe500->max_cmd_wait,pe500->cmd_waited_for);
				printf("\nTimed Out %d times  Currently Time Out is %d\n",
				    pe500->timed_out_cntr,pe500->timed_out);
	
			}

			pe500 = pe500->pnext_axis;
		}
	}
	if (any_present && (level > 0)){
		printf("Max motors active: %d \n",max_motors_active);
		printf("Maximum delay per motor: %d \n",max_delay_per_motor);
		printf("Maximum total delay: %d\n",max_delay);
	}
 }


/*
 * Dalesio
 *
 * e500_test.c
 */

/* test routines for the e500 stepper motor driver
*/
void e500_test( int branch, int crate, int card, int axis, int change )
{

	int	a0_id, a1_id, a3_id, a4_id;
	int	bank1_id, bank2_id;
	int	dummy, config, csr;
	int	abs_encoder, last_value;
	int	slew_accel, cntr;
	struct e500_motor	e500_mtr;
	
	cntr = 0;
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
	cdreg(&a3_id, branch, crate, card, A3);
	cdreg(&a4_id, branch, crate, card, A4);
	cdreg(&bank1_id, branch, crate, card, axis);
	cdreg(&bank2_id, branch, crate, card, axis+8);

	/* read absolute encoder */
	cntr += wait_for_q(F25, a0_id,    &dummy,	&e500_mtr);
	cntr += wait_for_q(F0,  bank2_id, &abs_encoder,	&e500_mtr);
	printf("f25,a0->f0,a(i+8): pos=%x\n",abs_encoder);

	/* read csr */
	cntr += wait_for_q(F25, a1_id,    &dummy,&e500_mtr);
	cntr += wait_for_q(F0,  bank1_id, &csr,	&e500_mtr);
	printf("f25,a0->f0,a(i): csr %x\n",csr);

	/* write the slew and acceleration rate */
	slew_accel = (100 << 16) + 10;
	cntr += wait_for_q(F17, bank2_id, &slew_accel, &e500_mtr);

	/* read slew and acceleration */
	cntr += wait_for_q(F25, a1_id,    &dummy,	&e500_mtr);
	cntr += wait_for_q(F0,  bank2_id, &config,	&e500_mtr);
	printf("f25,a1->f0,ai+8: slew rb %x\n",config);

	/* write the base rate */
	dummy = 0x100;
	cntr += wait_for_q(F17, bank1_id, &dummy, &e500_mtr);
	printf("f17,a(i): base rate %x\n",dummy);
	
	/* read base rate */
	cntr += wait_for_q(F25, a4_id,    &dummy, &e500_mtr);
	cntr += wait_for_q(F0,  bank1_id, &dummy, &e500_mtr);
	printf("f25,a4->f0,a(i): base rate %d\n",dummy);

	/* move it */
	if (change){

		if ((e500_sign==1) && (change < 0)){
			change = -change;
			change |= 0x800000;
		} else if ((e500_sign == 2) && (change < 0)){
			change = -change;
			change |= 0xff800000;
		}

		/* move the motor */
		cntr += wait_for_q(F16, bank1_id, &change, &e500_mtr);
		printf("f16,a(i): change 0x%x\n",change);

		/* write the csr */
		dummy = 0x7;
		cntr += wait_for_q(F17, bank1_id, &dummy, &e500_mtr);
		printf("f17,a(i): start through csr %x\n",dummy);
	
		/* monitor value */
		do {
			last_value = abs_encoder;

			/* read pulses to go */
			cntr += wait_for_q(F25, a0_id,    &dummy,	&e500_mtr);
			cntr += wait_for_q(F0,  bank1_id, &dummy,	&e500_mtr);
			printf("f25,a0->f0,a(i): pulses to go %d\n",dummy);

			/* read the csr */
			cntr += wait_for_q(F25, a1_id,    &dummy,	&e500_mtr);
			cntr += wait_for_q(F0,  bank1_id, &csr,	&e500_mtr);
			printf("f25,a1->f0,a(i): csr %x\n",csr);

			/* read the absolute encoder */
			cntr += wait_for_q(F25, a0_id,    &dummy,	&e500_mtr);
			cntr += wait_for_q(F0,  bank2_id, &abs_encoder,	&e500_mtr);
			printf("f25,a0->f0,a(i+8): position %x\n",abs_encoder);

			taskDelay(2);
		}while (csr & 0x4);
	}
	printf("cntr=%d\n",cntr);
}

void e500_func( int f, int b, int c, int n, int a, int arg )
{

	int	ext, cntr;
	struct e500_motor	e500_mtr;
	
	cntr = 0;
	printf ("func %d to: %d,%d,%d axis %d arg %d\n",
			f, b, c, n, a, arg);

	/* create camac variables */
	cdreg(&ext, b, c, n, a);
	cntr = wait_for_q(f,ext,&arg,&e500_mtr);
	printf("- %d tries for q\n",cntr);
}

/* wait_for_q 
 *
 * This routine exists because the stepper motor E500 is not all that fast.
 * You might say that it is half-fast. It appears as though some commands
 * will take about 600 usecs to complete. Some are instantaneous (depends
 * on the command and how long it has been since the last one). So, we try
 * to write the new command immediately and if it fails we will give up the
 * processor for 1 tick. This is currently about 1/60th of a second (15 milliseconds).
 * As these occur during readbacks and readbacks are scheduled every 1/10th,
 * we return the number of ticks waited so that the scan task can remove these
 * delays from the 10Hz scan.
 */
#define	E500_TIMEOUT	5
int e500_cfsa_echo = 0;
int wait_for_q(f, ext, pval,pe500)
int			f,ext,*pval;
struct e500_motor	*pe500;
{
	int	q;
	int	cntr = 0;

	cfsa(f,ext,pval,&q);
	while ((q == 0) && (cntr < E500_TIMEOUT)){
		cntr++;
		cfsa(f,ext,pval,&q);
		taskDelay(1);
	}
if (e500_cfsa_echo) printf("f:%d ext:0x%x arg:0x%x",f,ext,*pval);
	if (cntr >= E500_TIMEOUT){
		pe500->timed_out = 1;
		pe500->timed_out_cntr++;
if (e500_cfsa_echo) printf(" - timed out\n");
	}else{
		pe500->timed_out = 0;
if (e500_cfsa_echo) printf(" - successful\n");
	}
	return(cntr);
}
