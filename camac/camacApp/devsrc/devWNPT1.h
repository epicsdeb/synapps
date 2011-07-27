/* e500_sm_driver.h */
/* base/src/drv $Id: devWNPT1.h,v 1.1.1.1 2001-08-28 20:21:34 rivers Exp $ */
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
#define	MAX_WNPT1_AXIS	16


/* motor information */
struct wnpt1_motor{
short	link;
short	crate;
short	card;
int	velocity;
short	set_velocity;
short	incr;
int	final_position;
short	active;		/* flag to tell the e500_task if the motor is moving */
int	callback;	/* routine in database library to call with status  */
int	callback_arg;	/* argument to callback routine  */
struct wnpt1_motor	*pnext_axis;
struct motor_data	*pmotor_data;
};


