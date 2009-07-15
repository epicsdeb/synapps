/***********************************************************************
 *
 * mar345: esd.h
 *
 * Copyright by:        Dr. Claudio Klein
 *                      X-ray Research GmbH, Hamburg
 *
 * Version:     1.0
 * Date:        02/10/1997
 *
 ***********************************************************************/

/*
 */
#define MAX_CMD          16

/*
 * IPS commands
 */
#define  CMD_NULL        0
#define  CMD_STEP        1
#define  CMD_RADIAL      2
#define  CMD_XFER        3
#define  CMD_SHUTTER     4
#define  CMD_LOCK        5
#define  CMD_ABORT       6
#define  CMD_COLLECT     7
#define  CMD_ADC         8
#define  CMD_CHANGE      9
#define  CMD_SCAN        10
#define  CMD_SET         11
#define  CMD_ERASE       12
#define  CMD_MOVE        13
#define  CMD_ION_CHAMBER 14
#define  CMD_SHELL       15

#define CMD_MOVE_DISTANCE        31
#define CMD_MOVE_OMEGA           32
#define CMD_MOVE_CHI             33
#define CMD_MOVE_THETA           34
#define CMD_MOVE_RADIAL          35
#define CMD_MOVE_PHI             36

/*
 * CMD_MOVE arguments
 */
#define ARG_DISTANCE     1
#define ARG_OMEGA        2
#define ARG_CHI          3
#define ARG_THETA        4
#define ARG_RADIAL       5
#define ARG_PHI          6

#define ARG_MOVE_REL     0
#define ARG_MOVE_ABS     1
#define ARG_INIT_NEAR    2
#define ARG_INIT_FAR     3
#define ARG_STOP_SMOOTH  4
#define ARG_STOP_NOW     5
#define ARG_DEFINE       6

#define ARG_ABORT        0
#define ARG_STOP 	 1
#define ARG_GO           2

#define ARG_DOSE 	 1
#define ARG_TIME 	 2

#define ARG_CLOSE        0
#define ARG_OPEN 	 1

#define ARG_DISABLE      0
#define ARG_ENABLE       1

#define ARG_READ 	 0
#define ARG_WRITE        1
#define ARG_SETDEF      -1

#define STEPPER_IST      0
#define STEPPER_SOLL     1
#define STEPPER_STATUS   2

typedef struct _esd_cmd { 
         char    id[4];
         int     cmd;
         int     mode;
         int     par1;
         int     par2;
         int     par3;
         int     par4;
         int     par5;
         char    str[28];
} ESD_CMD;

typedef struct _esd_msg {
         char    id[4];
        char    str[80];
} ESD_MSG;

/*
 *       ESD controller status block definition.
 */

typedef struct _esd_stb {

         int     stepper[6][3]; /*  1-18: Stepper motors                      * 
                                 *        6 x IST,SOLL,STATUS fuer:           *
                                 *        DISTANCE, OMEGA, CHI,               * 
				 *        THETA, RADIAL, PHI                  */
         int     hw_status1;    /*    19: Hardware status bits                */
         int     task[20];      /* 20-39: Task bit list                       */
         int     valid_data;    /*    40: Scanned data                        */
         int     intensity;	/*    41: X-ray intensity                     */
	 int	 scanmode;	/*    42: Current scan mode                   */
	 int	 adc1;		/*    43: ADC offset 1                        */
	 int	 adc2;		/*    44: ADC offset 2                        */
         int     delay_open;	/*    45: Shutter open delay time             */
         int     delay_close;   /*    46: Shutter close delay time            */
         int     servo_state;   /*    47: Status of SERVO system              */
         int     gaps[12];      /* 48-59: GAP positions                       */
	 int	 servo_error; 	/*    60: Servo error status                  */
	 int	 servo_opmode;	/*    61: Servo opmode input                  */
	 int	 counter;     	/*    62: Status counter                      */
         int     unused1[ 7];   /* 63-69: Unused                              */
         int     errors [10];   /* 70-79: Error message buffer                */
         int     unused2[21];   /*80-100: Unused                              */
} ESD_STB;

#ifdef MAR_GLOBAL
ESD_CMD                 esd_cmd;
ESD_MSG                 esd_msg;
ESD_STB          	esd_stb;
#else
extern ESD_CMD          esd_cmd;
extern ESD_MSG          esd_msg;
extern ESD_STB          esd_stb;
#endif


