/*
FILENAME...	drvMMCom.h
USAGE... This file contains Newport Motion Master (MM) driver "include"
	    information that is specific to Motion Master models 3000/4000.

Version:	$Revision: 1.10 $
Modified By:	$Author: rivers $
Last Modified:	$Date: 2004-08-17 21:28:22 $
*/

/*
 *      Original Author: Mark Rivers
 *      Current Author: Mark Rivers
 *      Date: 10/16/97
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *	      The Controls and Automation Group (AT-8)
 *	      Ground Test Accelerator
 *	      Accelerator Technology Division
 *	      Los Alamos National Laboratory
 *
 *      Co-developed with
 *	      The Controls and Computing Group
 *	      Accelerator Systems Division
 *	      Advanced Photon Source
 *	      Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01  01-18-93 mlr initialized from drvOms58
 * .02  06-16-03 rls Converted to R3.14.x.
 */

#ifndef	INCdrvMMComh
#define	INCdrvMMComh 1

#include "motor.h"
#include "motordrvCom.h"
#include "asynDriver.h"
#include "asynOctetSyncIO.h"

enum MM_model
{
    MM4000,
    MM4005
};

enum MM_motor_type
{
    UNUSED,
    STEPPER,
    DC
};

#ifndef __cplusplus
typedef enum MM_model MM_model;
typedef enum MM_motor_type MM_motor_type;
#endif

/* Motion Master specific data is stored in this structure. */
struct MMcontroller
{
    asynUser *pasynUser;  	/* For RS-232 */
    int asyn_address;		/* Use for GPIB or other address with asyn */
    char asyn_port[80];     	/* asyn port name */
    char status_string[80];     /* String containing status of motors */
    char position_string[80];   /* String containing position of motors */
    MM_model model;		/* Motion Master Model. */
    MM_motor_type type[4];	/* For MM3000 only; Motor type array. */
    /* For MM4000/5 only; controller resolution array (from TU command).
     * Units are in [Controller EGU's / Record RAW units].
     */
    double drive_resolution[MAX_AXIS];
    int res_decpts[MAX_AXIS];	/* Drive resolution significant dec. pts. */
    double home_preset[MAX_AXIS]; /* Controller's home preset position (XF command). */
    CommStatus status;		/* Controller communication status. */
};


/* Motor status response for MM[3000/4000/4005]. */
typedef union
{
    epicsUInt8 All;
    struct
    {
#ifdef MSB_First
	bool bit7	:1;	/* Bit #7 N/A. */
	bool bit6	:1;	/* Bit #6 N/A. */
	bool homels	:1;	/* Home LS. */
	bool minusTL	:1;	/* Minus Travel Limit. */
	bool plustTL	:1;	/* Plus Travel Limit. */
	bool direction	:1;	/* Motor direction: 0 - minus; 1 - plus. */
	bool NOT_power	:1;	/* Motor power 0 - ON; 1 - OFF. */
	bool inmotion	:1;	/* In-motion indicator. */
#else
	bool inmotion	:1;	/* In-motion indicator. */
	bool NOT_power	:1;	/* Motor power 0 - ON; 1 - OFF. */
	bool direction	:1;	/* Motor direction: 0 - minus; 1 - plus. */
	bool plustTL	:1;	/* Plus Travel Limit. */
	bool minusTL	:1;	/* Minus Travel Limit. */
	bool homels	:1;	/* Home LS. */
	bool bit6	:1;	/* Bit #6 N/A. */
	bool bit7	:1;	/* Bit #7 N/A. */
#endif
    } Bits;
} MOTOR_STATUS;


#endif	/* INCdrvMMComh */

