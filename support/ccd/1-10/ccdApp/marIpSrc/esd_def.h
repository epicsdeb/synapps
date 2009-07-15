/***********************************************************************
 *
 * mar345: esd_def.h
 *
 * Copyright by:        Dr. Claudio Klein
 *                      X-ray Research GmbH, Hamburg
 * Orginal version:	Chris Nielsen, ADSC
 *
 * Version:     1.0
 * Date:        07/09/1997
 *
 ***********************************************************************/

#define MAX_CMD		16	

/*
 * IPS commands
 */
#define	CMD_NULL	0
#define	CMD_STEP	1
#define	CMD_RADIAL	2
#define	CMD_XFER	3
#define	CMD_SHUTTER	4
#define	CMD_LOCK	5
#define	CMD_ABORT	6
#define	CMD_COLLECT	7
#define	CMD_ADC		8
#define	CMD_CHANGE	9
#define	CMD_SCAN	10
#define	CMD_SET		11
#define	CMD_ERASE	12
#define	CMD_MOVE	13
#define	CMD_ION_CHAMBER	14
#define	CMD_SHELL	15

#define CMD_MOVE_DISTANCE	31
#define CMD_MOVE_OMEGA		32
#define CMD_MOVE_CHI  		33
#define CMD_MOVE_THETA		34
#define CMD_MOVE_RADIAL		35
#define CMD_MOVE_PHI  		36

/*
 * CMD_MOVE arguments
 */
#define ARG_DISTANCE	1
#define ARG_OMEGA   	2
#define ARG_CHI    	3
#define ARG_THETA   	4
#define ARG_RADIAL  	5
#define ARG_PHI     	6

#define ARG_MOVE_REL	0
#define ARG_MOVE_ABS	1
#define ARG_INIT_NEAR	2
#define ARG_INIT_FAR	3
#define ARG_STOP_SMOOTH 4
#define ARG_STOP_NOW    5

#define ARG_ABORT	0
#define ARG_STOP	1
#define ARG_GO 		2
#define	ARG_SHOW	-1
#define ARG_DEFINE 	-2

#define ARG_DOSE	1
#define ARG_TIME	2

#define ARG_CLOSE	0
#define ARG_OPEN	1

#define ARG_DISABLE	0
#define ARG_ENABLE	1

#define ARG_READ	0
#define ARG_WRITE	1
#define ARG_SETDEF	-1

