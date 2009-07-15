/***********************************************************************
 *
 * mar345: mar_command.h
 *
 * Copyright by:        Dr. Claudio Klein
 *                      X-ray Research GmbH, Hamburg
 *
 * Version:     2.0
 * Date:        04/11/1998
 *
 ***********************************************************************/

/*
 *	Definitions for the mar controller hardware commands.
 */
static char *cmdstr[] = { "NULL", 	"STEPPER", 	"RADIAL", 	"XFER",
 			  "SHUTTER", 	"LOCK",		"ABORT", 	"EXPOSE",
			  "ADJUST", 	"CHANGE", 	"SCAN", 	"SET",
			  "ERASE", 	"MOVE", 	"CHAMBER", 	"SHELL",
 			  NULL };

#define	CMD_MODE_MOVE_REL	0
#define	CMD_MODE_MOVE_ABS	1
#define	CMD_MODE_MIN     	2
#define	CMD_MODE_MAX     	3
#define	CMD_MODE_SET     	4

#define	CMD_MODE_CLOSE		0
#define	CMD_MODE_OPEN 		1

#define	CMD_MODE_DISABLE	0
#define	CMD_MODE_ENABLE 	1

#define	CMD_MODE_DOSE		0
#define	CMD_MODE_TIME		1
