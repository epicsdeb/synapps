/* e500_sm_driver.h */
/* base/src/drv $Id: devE500.h,v 1.1.1.1 2001-08-28 20:21:34 rivers Exp $ */
/*
 * headers that are used to interface to the 
 * DSP E500 tepper motor drivers
 *
 * 	Author:      Bob Dalesio
 * 	Date:        03-21-95
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1995, the Regents of the University of California
 *
 * Modification Log:
 * -----------------
 */
#define	MAX_E500_AXIS	8


/* motor information */
struct e500_motor{
short	link;
short	crate;
short	card;
short	active;		/* flag to tell the e500_task if the motor is moving */
short	max_cmd_wait;	/* longest wait to execute a command */
			/* greater than 0 = db scanning halted for stepper cmd to e500 */
short	cmd_waited_for;	/* command that caused the wait */
short	timed_out;
short	timed_out_cntr;
int	callback;	/* routine in database library to call with status  */
int	callback_arg;	/* argument to callback routine  */
struct e500_motor	*pnext_axis;
struct motor_data	*pmotor_data;
};


/* CSR readback bit definitions */
#define	E500_CCW_LIMIT	0x80
#define	E500_CW_LIMIT	0x40
#define	E500_BUSY	0x04

/* CSR write bit definitions */
#define	E500_STOP		0x80
#define	E500_BUILD_FILE		0x04
#define	E500_START_FILE_EXEC	0x02
#define	E500_LAM_MASK		0x01
#define	E500_START		0x07	/* all three of the above */	
