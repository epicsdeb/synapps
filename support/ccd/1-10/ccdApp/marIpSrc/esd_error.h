/***********************************************************************
 *
 * esd_errors.h
 *
 * Copyright by:        Dr. Claudio Klein
 *                      X-ray Research GmbH, Hamburg
 *
 * Version:     1.2.3
 * Date:        02/10/1998
 *
 ***********************************************************************/

#define	MAX_MSG		100

typedef struct _stb_msg {
#ifdef __sgi
	unsigned char	task;
	unsigned char	class;
	unsigned short	number;
#else
	unsigned short	number;
	unsigned char	class;
	unsigned char	task;
#endif
} STB_MSG;

typedef struct _err_msg {
	unsigned char	task;
	unsigned char	class;
	unsigned short	number;
	char		msg[32];
} ERR_MSG;

ERR_MSG	err_msg[] = {
/*   Task no. Class  Msg. no.	Message */
{ 	 0,	1,	  0,	"Unknown ERROR." },

/* CMD_RADIAL */
{	 2,	1,	 25,	"Timeout gap open." },
{	 2,	1,	 28,	"Stepper ERROR." },
{	 2,	1,	 30,	"Lower Endswitch NOT reached." },
{	 2,	1,	 32,	"Homeposition NOT found." },
{	 2,	1,	 33,	"Upper Endswitch reached." },
{	 2,	1,	 34,	"Timeout. Channel busy." },
{	 2,	1,	 36,	"Upper Endswitch reached." },
{	 2,	1,	 37,	"Not on target." },

/* CMD_XFER   */
{	 3,	1,	 401,	"Timeout." },

/* CMD_SHUTTER */
{	 4,	1,	202,	"EXPOSURE task active." },
{	 4,	1,	205,	"Cannot OPEN shutter." },
{	 4,	1,	207,	"Shutter is already OPEN." },
{	 4,	1,	210,	"Cannot CLOSE shutter." },
{	 4,	1,	212,	"Unknown mode." },

/* CMD_LOCK   */
{	 5,	1,	 57,	"Timeout rotation sense." },
{	 5,	1,	 58,	"Wrong parameters." },
{	 5,	1,	 60,	"Cannot LOCK plate." },
{	 5,	1,	 63,	"Cannot UNLOCK plate." },
{	 5,	1,	105,	"Servo warning." },

/* CMD_ABORT */
{	 6,	1,	304,	"Out of range." },

/* CMD_COLLECT */
{	 7,	1,	215,	"Cannot OPEN shutter." },
{	 7,	1,	234,	"Shutter is already OPEN." },
{	 7,	1,	238,	"Cannot CLOSE shutter." },
{	 7,	1,	241,	"Cannot OPEN  shutter." },
{	 7,	1,	239,	"Stepper ERROR." },

/* CMD_ADJUST */
{	 8,	1,	704,	"Flash restore error." },
{	 8,	1,	713,	"Flash restore error." },
{	 8,	1,	721,	"Diff 1 error." },
{	 8,	1,	722,	"Diff 2 error." },
{	 8,	1,	723,	"Diff 3 error." },
{	 8,	1,	724,	"Diff 4 error." },
{	 8,	1,	725,	"Diff 5 error." },
{	 8,	1,	726,	"Diff 6 error." },
/*
{	 8,	3,	 10,	"ERROR in ADJUST." },
*/

/* CMD_CHANGE */
{	 9,	1,	 67,	"Profile conversion error." },
{	 9,	1,	 70,	"Cannot OPEN profile." },
{	 9,	1,	 71,	"Invalid mode." },

/* CMD_SCAN   */
{	10,	1,	 12,	"Laser OFF." },
{	10,	1,	 13,	"Laser overcurrent." },
{	10,	1,	 74,	"Timeout round event." },
{	10,	1,	 77,	"Unknown mode." },
{	10,	1,	 82,	"Invalid profile table." },
{	10,	1,	 83,	"Unlocked plate." },
{	10,	1,	 86,	"Premovement error." },
{	10,	1,	 87,	"ADJUST not finished." },
{	10,	1,	 92,	"Gear connect error." },
{	10,	1,	 93,	"No radial movement in round." },
{	10,	1,	 94,	"Lower Endswitch reached." },
{	10,	1,	 95,	"Upper Endswitch reached." },
{	10,	1,	 100,	"Lost data." },
{	10,	1,	 101,	"Lower Endswitch reached." },
{	10,	1,	 109,	"Number of data does not match." },

/* CMD_SET    */
{	11,	1,	 20,	"Error: Flash write." },
{	11,	1,	 22,	"Error: Flash write." },

/* CMD_ERASE  */
{	12,	1,	 43,	"LEFT erase lamp OFF." },
{	12,	1,	 44,	"LEFT erase lamp OVERTEMP." },
{	12,	1,	 45,	"RIGHT erase lamp OFF." },
{	12,	1,	 46,	"RIGHT erase lamp OVERTEMP." },
{	12,	1,	 47,	"TOP  erase lamp OFF." },
{	12,	1,	 48,	"TOP  erase lamp OVERTEMP." },
{	12,	1,	 54,	"Unknown mode." },
{	12,	1,	 56,	"No profile loaded." },
{	12,	1,	108,	"Speed too low." },

/* CMD_MOVE */
{	13,	1,	217,	"Timeout." },
{	13,	1,	218,	"???" },
{	13,	1,	219,	"Stepper ERROR."},
{	13,	1,	225,	"Reference ERROR." },
{	13,	1,	233,	"Unknown mode." },

/* All tasks */
{	 0,	1,	  5,	"Command overflow." },
{	 0,	1,	  6,	"Queue overflow." },
{	 0,	1,	  7,	"Command overflow." },

/* Intern tasks: */
{	 0,	1,	104,	"Can not allocate memory..." },
{	 0,	1,	511,	"Servo: Initialize port ERROR." },
{	 0,	1,	601,	"Homeposition not found." },
{	 0,	1,	605,	"GAP not found." },

{	99,	0,	0,	"\0" },
};

