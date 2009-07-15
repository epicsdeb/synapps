/***********************************************************************
 *
 * scan345: marhw.c
 *
 * Copyright by:        Dr. Claudio Klein
 *                      X-ray Research GmbH, Hamburg
 *
 * Version:     2.2
 * Date:        15/06/2000
 *
 * History:
 *
 * Date		Version		Description
 * ---------------------------------------------------------------------
 * 15/06/00	2.2		Added feature COMMAND SCAN ADD x ERASE y
 *				Get firmware params at startup (USE SPY)
 *
 *
 ***********************************************************************/

#include	<stdio.h>
#include 	<time.h>
#include 	<math.h>
#include 	<string.h>
#include 	<errno.h>
#include 	<signal.h>

#include        "esd.h"
#include        "esd_error.h"
#include        "marcmd.h"
#include        "mar_command.h"
#include	"marglobals.h"
#include        "twopower.h"
#include        "config.h"

#define min0(a,b)       if ( b < a ) a = b
#define max0(a,b)       if ( b > a ) a = b
#define MAX_DOSE	2000
/*
 * Global variables 
 */
char     		start_time[32];
char			motor_op		= 0;
char			skip_op			= 0;
char   			keep_image		= 1;
int			mar_error		= 0;
int			target_steps		= 0;
int			ict1			= 0;
int			ict2			= 0;
int			adc1			= 0;
int			adc2			= 0;

time_t			exposure_time;
time_t			exposure_start		= 0;
int			erase_start;

int			fehler_index 		= 0;
int			stat_scan_add		= 0;
int			stat_task_number;
int			stat_task_active;
int			stat_task_done;
int			stat_task_mode;
int			stat_task_error;
int			stat_pixels;
int			stat_blocks_sent;
int			stat_blocks_scanned;
int 			stat_reference_status;
int 			stat_plate_locked;
int 			stat_lock_permit;
int			stat_sending_data;
int			stat_oscil_msg		= (-1);
char			stat_home		= 1;
char			stat_scan_active	= 0;
int			com_phi_steps;

int			op_dosen;
float			op_dosebeg,op_doseend,op_dosemin,op_dosemax;
float			op_doseavg,op_dosesig;
double			target_distance,	original_distance;
double			target_phi,		original_phi;
double			target_omega,		original_omega;

static	float   	expo_dosebeg		= 99999; 
static  float           expo_doseend		= 0.0;
static  float           expo_doseavg		= 0.0;
static  float           expo_dosesig		= 0.0;
static  float           expo_dosemin		= 999999.0;
static  float           expo_dosemax		= 0.0;
static  int		expo_dosen		= 0;
                                    
/*
 * Local variables
 */
static int 		i, j;

static char		motor_moving[6]		= {0,0,0,0,0,0};
static char		change_mode		=0;
static char		scan_started		=0;
static char		stop_image 		=0;

static int 		last_mar_mode 		=0;
static int 		dist_retry		=0;
static int 		intensity_counter	=0;
static int 		open_shutter_counter	=0;
static int 		close_shutter_counter	=0;
static int		scan_error		=0;
static int		p_task_active		=0;
static int		erase_lamps_ok		=0;
static int		expo_dose[MAX_DOSE];
static time_t		now,tick;
static time_t		last=0;
static float		ftmp;
static float		erase_time=1.0;
static float		xray_start = -999.0;
static float		stat_start_phi = 0.0;

static int 		stat_erase_T,stat_erase_R,stat_erase_L;
static int 		stat_erase_T_temp,stat_erase_R_temp,stat_erase_L_temp;
static int 		stat_servo_warning,stat_laser_status;
static int		task_done	[MAX_CMD];
static int		task_error	[MAX_CMD];
static int		task_active	[MAX_CMD];
static int		task_queued	[MAX_CMD];
static int		task_started	[MAX_CMD];

/*
 * External variables
 */

extern char		op_in_progress;
extern char     	scan_in_progress;
extern int		images_per_sweep;
extern int		current_block,maximum_block;
extern int		current_pixel;
extern int		maximum_pixels;

/*
 * Local functions
 */
int		OpenMessage		(void);
int		mar_progress		(int,int,int);
int		mar_servo		(void);
int 		mar_lock 		(void);
int  		mar_erase		(void);
int 		mar_change_mode		(void);
int 		marTask			(int, float);
int		get_status		(void);
int		NextImage		(int);

void		mar_quit		(int);
void		marError		(int, int);
int  		process_status		(char *);
void		mar_abort		(int);
void		print_msg		(char *);
void		mar_initial_status	(void);
void        	PrintStatus		(void);
static int	get_error   		(int);
static int 	Modulo360		(void);
static int	NotEnoughDiskSpace	(void);
static int	StartScan		(void);
static int	mar_move_phi		(double);
static int	mar_start_expo		(void);

/*
 * External functions
 */
extern int  	mar_io			();
extern int  	mar_start_scan_readout	(int);
extern void 	Transform		();

/******************************************************************
 * Function: mar_abort = sends ABORT to current task
 ******************************************************************/
void mar_abort(int s)
{
int i;
	fprintf(stdout, "scan345: Trying to abort current task ...\n");

	i=marTask( MDC_COM_ABORT, 1.0);

	/* Reenter SIGQUIT */
	signal( SIGQUIT, mar_abort);
}

/******************************************************************
 * Function: mar_command = sends a command to mar controller
 ******************************************************************/
int	
mar_command() 
{
extern int		net_comm(int, char *);

	/* Return if scanner is not available */
	if ( netcontrol==0 ) return( 0 );
	time( &now );

	if( OpenMessage() ) {
		fprintf(fpmsg, "scan345: Task %s STARTED at %s", cmdstr[mar_number],(char *)ctime( &now ));
		fflush( fpmsg );
	}
	if ( mar_cmd == MDC_COM_COLL && mar_number != CMD_COLLECT )
		fflush( stdout );
	else
		if ( verbose )
		fprintf(stdout, "scan345: Task %s STARTED at %s", cmdstr[mar_number],(char *)ctime( &now ));
	
	esd_cmd.cmd	= mar_number;
	esd_cmd.mode	= mar_mode;
	esd_cmd.par1	= mar_par1;
	esd_cmd.par2	= mar_par2;
	esd_cmd.par3	= mar_par3;
	esd_cmd.par4	= mar_par4;
	esd_cmd.par5	= mar_par5;
	
	strcpy( esd_cmd.id , "IPS"   );
	if ( mar_number != CMD_SHELL ) mar_str[0]= '\0';
		
	strcpy( esd_cmd.str, mar_str );

	fprintf( fpout, "scan345: IPS  %d %d %d %d %d %d %d %s\n",mar_number,mar_mode,mar_par1,mar_par2,mar_par3,mar_par4,mar_par5,mar_str);
	if ( verbose  )
	fprintf( stdout, "scan345: IPS  %d %d %d %d %d %d %d %s\n",mar_number,mar_mode,mar_par1,mar_par2,mar_par3,mar_par4,mar_par5,mar_str);
	if ( net_comm( (int)1, (char *)&esd_cmd ) < 1 ) {
		marError( 1000, mar_number);
		return( 0 );
	}

	last_mar_mode = mar_mode;

	return 1;
}

/******************************************************************
 * Function: mar_move_phi = moves phi
 ******************************************************************/
static int
mar_move_phi(double d)
{

double		delta;
int		wanted,nsteps_to_move;

	if ( dc_stop > 1 )
			return( 1 );
/*
	delta = stat_phibeg + com_dphi * (com_phiosc - totimg);
*/
	delta = com_phibeg;

	while (delta <    0.0) delta += 360.;
	while (delta >= 360.0) delta -= 360.;

	target_phi = delta;

	/* Phi movement is disabled ... */
	if( !cfg.use_phi ) {
	    	stat_phi = delta;
		i = mar_start_expo();
	    	return( 1 );
	}

	wanted = (delta + 0.5 * (1. / cfg.phi_steps)) * cfg.phi_steps;
			
	nsteps_to_move = wanted - phi_steps;

	if ( d > -9999. ) {
		if ( d <  0.0 )
		nsteps_to_move = wanted = ( d - 0.5 * (1. / cfg.phi_steps)) * cfg.phi_steps;
		else
		nsteps_to_move = wanted = ( d + 0.5 * (1. / cfg.phi_steps)) * cfg.phi_steps;
	}

	while ( abs(nsteps_to_move) > 180 * cfg.phi_steps) {
	    if(nsteps_to_move < 0)
			nsteps_to_move += 360 * cfg.phi_steps;
	    else
			nsteps_to_move -= 360 * cfg.phi_steps;
	}

	/* Phi movement finished */
	if(nsteps_to_move == 0) {
		/* Start exposure */
		i = mar_start_expo( );
		return( 1 );
	}
	/* Phi movement negative: drive 250 more steps than necessary */
	else if ( nsteps_to_move < 0 ) {
		nsteps_to_move -= 125;
		mar_par4    	= 125;
		mar_par5    	= cfg.phi_speed/2;
	}
	else {
		mar_par4    	= 0;
		mar_par5    	= cfg.phi_speed;
	}

	/* Phi movement goes on */
	stat_scanner_op = CMD_MOVE_PHI;
	motor_op	= stat_scanner_op;

	mar_number  	= CMD_MOVE;
	mar_mode	= ARG_PHI;
	mar_par1    	= ARG_MOVE_REL;
	mar_par2    	= nsteps_to_move;
	mar_par3    	= cfg.phi_speed;
	mar_str[0]	= '\0';

	if( 0 == mar_command( )) return( -1 ); 

	return( 1 );
}
/******************************************************************
 * Function: mar_quit
 ******************************************************************/
void
mar_quit(int m)
{
extern int	net_close();
static char	call=0;

	if ( call > 0 ) {
		remove( msg_file );
		exit(0);
	}

	/* Enable ion chamber switching */
	mar_number      = CMD_ION_CHAMBER;
	mar_mode 	= ARG_ENABLE;
	mar_par1 	= mar_par2 = 0;
	i 		= mar_command();
	sleep( 1 );

	/* Check Modulo of PHI */
	i = Modulo360();

	/* Close socket */
	for ( i=0; i<4; i++ ) {
		if ( net_close( i ) ) {
			sprintf( str, "scan345: socket %d closed ...\n",i+1);
			fprintf(  fpout, str );
		}
	}

	/* Close mar.lp.x file */
	if ( cfg.use_stats && fpstat != NULL ) { 
		fprintf( fpstat, "_________________________________________________________________________________________________\n\n");
		fclose( fpstat );
	}
	/* Close mar.spy.x file */
	if ( cfg.use_msg && fpspy != NULL ) { 
		fclose( fpspy );
	}
	printf("scan345: Sockets have been closed! No more scanner control...\n");
	call = 1;

	sleep( 1 );

	/* Close and remove message file */
	if ( fpmsg != NULL ) {
		fclose( fpmsg );
		remove( msg_file );
	}

	/* Close status file */
	if ( fpval != NULL ) {
		fclose( fpval );
	}

	exit( 0 );
}

/******************************************************************
 * Function: mar_change_mode
 ******************************************************************/
int 	
mar_change_mode()
{
int		i;
static int	previous_mode= 99;

	/* Mode change not required , go ahead */
	if ( netcontrol == 0 || change_mode == 0 || cur_mode == previous_mode || cur_mode == stat_scanmode ){

		if ( mar_cmd != MDC_COM_MODE && mar_cmd != MDC_COM_STARTUP ) {

			/* We are doing an ERASE automatically ... */
			sprintf(str,"scan345: Diameter increased for next scan mode!\nscan345: Plate will be erased first ...\n");
			if ( mar_cmd != MDC_COM_ERASE && cur_diameter > stat_diameter ) {
				fprintf(stdout, str);
			}
			i = mar_erase();
			return 1;
		}
	}

	/* Mode change required!!! */
	previous_mode 	= cur_mode;
	stat_scanner_msg= 0;
	stat_scanner_op = CMD_CHANGE;

	/* New scanning mode requested */
	mar_number 	= CMD_CHANGE;
	mar_mode 	= cur_mode+1;
	mar_par1 	= mar_par2 = mar_par3 = mar_par4 = mar_par5 = 0;
	mar_str[0]	= '\0';

	mar_command() ;

	return 1;
}

/******************************************************************
 * Function: mar_erase
 ******************************************************************/
int 	
mar_erase()
{
int		i;

	if ( netcontrol == 0 ) return 1;

	if ( mar_cmd == MDC_COM_SCAN ) {
		i = StartScan();
		return 1;
	}
	
	/* Send ERASE command to scanner */
	erase_start	= time(NULL);
	erase_time	= cfg.erasetime[ stat_scanmode ];
	stat_scanner_op = CMD_ERASE; 
	stat_scanner_msg= 0;

	/* New scanning mode requested */
	mar_number 	= CMD_ERASE;
	mar_mode 	= stat_scanmode+1;
	mar_par1 	= mar_par2 = mar_par3 = mar_par4 = mar_par5 = 0;
	mar_str[0]	= '\0';

	i= mar_command();

	return 1;
}

/******************************************************************
 * Function: NotEnoughDiskSpace
 ******************************************************************/
static int  NotEnoughDiskSpace()
{
int		i;
extern float	GetDiskSpace();
float		disk,dfree;

	/* 
	 * Is there enough disk space to produce this image ?
	 */
	dfree 	= GetDiskSpace( stat_dir );
	i 	= cfg.size[ run[SET].scanmode ];
	disk 	= (i*i*2 + 2*i)/1000000.;

	if ( ( com_format == OUT_PCK || com_format == OUT_MAR || 
	       com_format == OUT_CBF || com_format == OUT_CIF ) && keep_image )
		disk *= 0.5;            /* PCK: 50 % compression */

	/* Keep spiral active: add space for spiral */
	if ( keep_spiral )
		disk += (i*i*2 + 2*i)/1000000.;

	if ( disk > dfree ) {
		/* Try to continue writing in working_dir */
		sprintf( str, "scan345: Disk %s is full!\n", stat_dir );
		fprintf( stdout, str );
		fprintf(  fpout, str );

		/* In working dir there is more room, so continue here */
		if ( disk < dfree ) {
			strcpy( stat_dir, working_dir );
			sprintf( str, "scan345: Continue to write data into %s\n", stat_dir );
			fprintf( stdout, str );
			fprintf(  fpout, str );
		}
	}

	return 0;
}

/******************************************************************
 * Function: marTask = starts a scanner command
 ******************************************************************/
int	
marTask(int task, float val )
{
int			wanted,nsteps_to_move;
struct tm               *cur_time;
char                    curtime[16];
int			i,sys_param[5] = { 279, 97, 278, 95, 0 };
double			delta,fabs();
extern char   		input_keep_spiral;
extern char   		input_keep_image ;

	dc_stop				= 0;
	stat_scanner_control 		= MAR_CONTROL_ACTIVE;

	/* Set MAR command to 0 */
	if ( task != MDC_COM_SHELL && task != MDC_COM_IPS ) {
		mar_number 	= mar_mode  	= 0;
		mar_par1  	= mar_par2 	= 0;
		mar_par3  	= mar_par4 	= 0;
		mar_par5  	           	= 0;
		mar_str[0]			= '\0';
	}

	/* Open message file */
	if ( task != MDC_COM_IDLE ) {
		i = OpenMessage();
	}

	switch( (int)task ) {

	case MDC_COM_IDLE:			/* Send NULL command */
		if( 0 == mar_command( )) return( 0 ); 
		break;

	case MDC_COM_IPS:			/* General IPS command */
		mar_cmd		= MDC_COM_IPS;
		if( 0 == mar_command( )) return( 0 ); 
		break;

	case MDC_COM_SHELL:			/* Shell command */
		mar_cmd		= MDC_COM_SHELL;
		mar_number 	= CMD_SHELL;
		mar_mode	= 0;
		mar_par1	= 0;
		mar_par2	= 0;
		mar_par3	= 0;
		mar_par4	= 0;
		mar_par5	= 0;
		strcpy( mar_str, ips_command );
		if( 0 == mar_command( )) return( 0 ); 
		break;

	case MDC_COM_RESTART:			/* Reset on controller */
		return( 1 );
		break;

	case MDC_COM_QUIT:
		mar_quit(0);
		return( 1 );
		break;

	case MDC_COM_STARTUP:			/* Abort any active commands */
	case MDC_COM_INIT:		
		mar_cmd 	= MDC_COM_INIT;
		stop_image 	= 0;
		target_distance = val;

		/* Close shutter , if open */
		if ( stat_xray_shutter == SHUTTER_IS_OPEN ) {
			mar_cmd 	= (int)task;
			stat_scanner_op = CMD_SHUTTER;
			mar_number    	= CMD_SHUTTER;
			mar_mode      	= ARG_CLOSE;
			if( 0 == mar_command( )) return( 0 ); 
			sleep ( 1 );
		}

		if ( task == MDC_COM_STARTUP ) {
			mar_cmd 	= MDC_COM_STARTUP;

			if ( esd_stb.task[CMD_SCAN] & 0x02 ) {
				fprintf( stdout, "scan345: Task SCAN found active ...\n");
				fprintf( stdout, "scan345: Please wait and restart program after SCAN has finished!\n");
				mar_quit( 0 );
				exit( 0 );
			}

			/* Check for active stepper task */
			for ( i=13; i<13; i++ ) {
				if ( esd_stb.task[i] &  0x02 ) {
					sprintf( str, "scan345: Task %2d found active ...\n",i );
					fprintf( fpout, str );
					fprintf(stdout, str );
					if ( i == CMD_MOVE ) {
					    for ( j=0; j<6; j++ ) {
						if ( motor_moving[j] == 0 || (j+1)==ARG_RADIAL ) continue;

						mar_number    	= CMD_ABORT;
						mar_mode	= i;
						mar_par1	= 0;
						if( 0 == mar_command( )) return( 0 );
						sleep( 1 );
					    }
					}
					else {
						mar_number    	= CMD_ABORT;
						mar_mode      	= i;
						if( 0 == mar_command( )) return( 0 );
						sleep( 1 );
					}
				}
			}

			time( &now );
			cur_time = localtime( &now );

			/* Synchronize time and date */
			mar_number    	= CMD_SHELL;
			strftime( curtime, 16, "%H:%M:%S\0", cur_time);
			sprintf ( mar_str, "clockset %s\r", curtime);
			if( 0 == mar_command( )) return( 0 ); 
			sleep( 1 );

			strftime( curtime, 16, "%d-%m-%Y\0", cur_time);
			sprintf ( mar_str, "dateset %s\r", curtime);
			if( 0 == mar_command( )) return( 0 ); 
			sleep( 1 );

			/* Enable ion chambers */
			mar_number      = CMD_ION_CHAMBER;
			mar_mode 	= ARG_ENABLE;
			if( 0 == mar_command( )) return( 0 ); 


			/* Get some more system params (USE SPY only) */
			if ( fpspy != NULL ) {
				for ( i=0; i<4; i++ ) {
					mar_number	= CMD_SET;
					mar_mode	= ARG_READ;
					mar_par1	= sys_param[ i ];
					if( 0 == mar_command( )) return( 0 );
					sleep( 1 );
				}
			}

			/* Initialize servo system */
			if ( esd_stb.servo_state != 0 ) {
				fprintf( stdout, "scan345: Initializing SERVO system ...\n");
				sprintf ( mar_str, "SERVO_INIT 1,3,5\r");
				if( 0 == mar_command( )) return( 0 ); 
				sleep( 1 );
				i = mar_servo();
				break;
			}
			/* Lock plate */
			mar_lock();

			/* Are we done ? */
			if ( esd_stb.scanmode > 0 && esd_stb.scanmode < 9 ) {
				/* Adjust PHI to modulo 360.0 deg */
				i = Modulo360();
			}

			/* ... not yet: go to defined scan mode */
			sprintf( str, "scan345: Current scan mode: %d ...\n", stat_scanmode );
			fprintf( fpout, str );
			fprintf(stdout, str );

			break;
		}

		mar_number    	= CMD_MOVE;
		mar_mode	= ARG_DISTANCE;
                if (target_distance == cfg.dist_max) {
                	mar_par1  = ARG_INIT_FAR;
			mar_par2  = (cfg.dist_max + 0.5 * (1. / cfg.dist_steps)) * cfg.dist_steps;
                }
                else {
                        mar_par1  = ARG_INIT_NEAR;
			mar_par2  = (cfg.dist_min + 0.5 * (1. / cfg.dist_steps)) * cfg.dist_steps;
                }
		mar_par3 = cfg.dist_speed;
		mar_par4 = 0;
		mar_par5 = cfg.dist_speed/10;	/* CHECK !!! */
		if( 0 == mar_command( )) return( 0 ); 

		stat_scanner_op = CMD_MOVE_DISTANCE;
		motor_op	= stat_scanner_op;

		break;

	case MDC_COM_ABORT:			/* Abort all active commands*/

		/* We do not want to interrupt scan or erase... */
		if ( scan_in_progress && ( stat_task_number == CMD_SCAN || stat_task_number == CMD_ERASE ) ) {
			break;
		}

		/* Set up MAR command */
		mar_number    	= CMD_ABORT;
		mar_par1 	= mar_par2 = 0;
		for ( i=j=0; j<6; j++ ) {
			if ( motor_moving[j] == 0 || (j+1)==ARG_RADIAL ) continue;

			if ( (j+1) == ARG_DISTANCE ) {
				sprintf( str ,"scan345: Abort DRIVE DETECTOR\n");
				if ( stat_scanner_op != CMD_MOVE_DISTANCE ) continue;
			}
			else if ( (j+1) == ARG_OMEGA ) { 
				sprintf( str ,"scan345: Abort DRIVE OMEGA\n");
				if ( stat_scanner_op != CMD_MOVE_OMEGA ) continue;
			}
			else if ( (j+1) == ARG_CHI   ) {
				sprintf( str ,"scan345: Abort DRIVE CHI\n");
				if ( stat_scanner_op != CMD_MOVE_CHI) continue;
			}
			else if ( (j+1) == ARG_THETA ) {
				sprintf( str ,"scan345: Abort DRIVE THETA\n");
				if ( stat_scanner_op != CMD_MOVE_THETA ) continue;
			}
			else if ( (j+1)== ARG_PHI )  {
				sprintf( str ,"scan345: Abort DRIVE PHI\n");
				if ( stat_scanner_op != CMD_MOVE_PHI ) continue;
			}
			fprintf( fpout, str);
			fprintf(stdout, str);

			mar_number	= CMD_ABORT;
			mar_mode   	= CMD_MOVE;
			mar_par1	= 0;
			dc_stop		= 2;

			if( 0 == mar_command( )) return( 0 );
			i++;

			if ( (j+1) == ARG_PHI ) {
				/* Adjust PHI to modulo 360.0 deg */
				Modulo360();
			}

		}
		if ( mar_cmd != MDC_COM_COLL) break;

		fprintf(fpout, "\nscan345: Abort EXPOSURE\n");
		fprintf(stdout,"\nscan345: Abort EXPOSURE\n");

		/* Abort exposure... */
		mar_number 	= CMD_ABORT;
		mar_mode   	= CMD_COLLECT; 

                if( 0 == mar_command( )) return( 0 );

		/* Adjust PHI to modulo 360.0 deg */
		Modulo360();
		
		break;

	case MDC_COM_SHUT:			/* Operate shutter */
		mar_cmd 	= (int)task;
		stat_scanner_op = CMD_SHUTTER;
		mar_number      = CMD_SHUTTER;
		mar_par1 	= mar_par2 = 0;
		/* Open shutter */
		if( val == 1.0 ) {
			mar_mode = ARG_OPEN;
		}
		/* Close shutter */
		else {
			mar_mode = ARG_CLOSE;
		}

		if( 0 == mar_command( )) return( 0 ); 
		break;

	case MDC_COM_CHAMBER:			/* (De-)select ion chamber */
		stop_image 	= 0;
		dc_stop		= 0;
		mar_number      = CMD_ION_CHAMBER;
		mar_par1 	= mar_par2 = 0;
		/* Enable switching */
		if( val == 1.0 )
			mar_mode = ARG_ENABLE;
		/* Disable switching */
		else
			mar_mode = ARG_DISABLE;
		if( 0 == mar_command( )) return( 0 ); 
		break;

	case MDC_COM_DSET:			/* Define new distance */
		mar_cmd 	= (int)task;
		wanted 		= (val+0.5*(1./cfg.dist_steps))*cfg.dist_steps;
		distance_steps 	= wanted;

		mar_number   	= CMD_SET;
		mar_mode   	= ARG_WRITE;
		mar_par1   	= 75;
		mar_par2   	= wanted;
		stat_scanner_op	= CMD_MOVE_DISTANCE;
		motor_op	= stat_scanner_op;
 
		fprintf(stdout, "scan345: Defining DISTANCE as %1.3f\n",val);
		if( 0 == mar_command( )) return( 0 ); 

		stat_dist      = val;
		break;

	case MDC_COM_PSET:			/* Define new phi */
		mar_cmd 	= (int)task;
		delta          	= val;
		while(delta <    0.0) 
				delta += 360.;
		while(delta >= 360.0) 
				delta -= 360.;
		phi_steps      	= (delta+0.5 *(1./cfg.phi_steps))*cfg.phi_steps;

		mar_number   	= CMD_SET;
		mar_mode   	= ARG_WRITE;
		mar_par1   	= 80;
		mar_par2   	= phi_steps;

		stat_scanner_op	= CMD_SET;
		motor_op	= stat_scanner_op;

		fprintf(stdout, "scan345: Defining PHI as %1.3f\n",val);
		if( 0 == mar_command( )) return( 0 ); 
		stat_phi       = delta;

		break;

	case MDC_COM_OSET:			/* Define new omega */
		mar_cmd 	= (int)task;
		delta          	= val;
		while(delta <    0.0) 
				delta += 360.;
		while(delta >= 360.0) 
				delta -= 360.;
		omega_steps 	= (delta+0.5*(1./cfg.ome_steps))*cfg.ome_steps;

		mar_number   	= CMD_SET;
		mar_mode   	= ARG_WRITE;
		mar_par1   	= 76;
		mar_par2   	= omega_steps;
		stat_scanner_op	= CMD_MOVE_OMEGA;
		motor_op	= stat_scanner_op;

		fprintf(stdout, "scan345: Defining OMEGA as %1.3f\n",val);
		if( 0 == mar_command( )) return( 0 ); 
		stat_omega     = delta;

		break;

	case MDC_COM_DISTANCE:			/* Move distance */
		mar_cmd 	= (int)task;
		/* Distance movement disabled... */
		if( ! cfg.use_dist ) {
		    stat_dist = val;
		    break;
		}
		/* Distance movement  enabled... */
		wanted 		= (val+0.5*(1./cfg.dist_steps))*cfg.dist_steps;
		target_steps	= wanted;
		if(wanted < distance_steps) {
#ifdef TRANS_BACK
			/* Use backlash during distance translation ... */
			wanted 		-= 100;
			mar_par4	= wanted + 100;
			mar_par5      	= cfg.dist_speed;
#else
			mar_par4	= wanted;
#endif
		}
		else {
			mar_par4	= wanted;
		}

		dist_retry		= 0;
		original_distance 	= stat_dist;
		target_distance 	= val;
		stat_scanner_op		= CMD_MOVE_DISTANCE;
		motor_op		= stat_scanner_op;
		mar_number     		= CMD_MOVE;
		mar_mode   		= ARG_DISTANCE;
		mar_par1   		= ARG_MOVE_ABS;
		mar_par2      		= wanted;
		mar_par3      		= cfg.dist_speed;
		mar_par5      		= cfg.dist_speed;

		fprintf(stdout, "scan345: Moving DISTANCE from %1.3f to %1.3f\n",stat_dist,target_distance);

		if( 0 == mar_command( )) return( 0 ); 

		break;

	case MDC_COM_PHI:			/* Move phi */
		mar_cmd 	= (int)task;
		delta           = val;
		if(delta <    0.0 ) delta += 360.;
		if(delta >  360.0 ) delta -= 360.;
		/* Phi movement disabled... */
		if( ! cfg.use_phi ) {
		    stat_phi = delta;
		    break;
		}

		/* Phi movement enabled... */
		wanted 		= (delta+0.5*(1./cfg.phi_steps))*cfg.phi_steps;
		nsteps_to_move 	= wanted - phi_steps;
		while ( abs(nsteps_to_move) >  180 * cfg.phi_steps) {
		    if(nsteps_to_move < 0)
			nsteps_to_move += 360 * cfg.phi_steps;
		      else
			nsteps_to_move = nsteps_to_move - 360 * cfg.phi_steps;
		}

		/* Phi movement negative: drive 250 more steps than necessary */
		if ( nsteps_to_move < 0 ) {
			nsteps_to_move -= 125;
			mar_par4	= 125;
			mar_par5     	= cfg.phi_speed/2;
		}
		else {
			mar_par4	= 0;
			mar_par5     	= cfg.phi_speed;
		}

		target_phi	= delta;
		stat_scanner_op = CMD_MOVE_PHI;
		motor_op	= stat_scanner_op;
		mar_number    	= CMD_MOVE;
		mar_mode	= ARG_PHI;
		mar_par1   	= ARG_MOVE_REL;
		mar_par2     	= nsteps_to_move;
		mar_par3     	= cfg.phi_speed;

		fprintf(stdout, "scan345: Moving PHI from %1.3f to %1.3f\n",stat_phi,target_phi);
		if( 0 == mar_command( )) return( 0 ); 
		
		break;

	case MDC_COM_OMOVE:			/* Move omega */
		mar_cmd 	= (int)task;
		target_omega	= val;
		if( !cfg.use_ome ) {
		    stat_omega = val;
		    break;
		}
		/* This is for MPI-Hamburg only */
		nsteps_to_move 	= val*cfg.ome_steps;

		if(nsteps_to_move == omega_steps && cfg.use_zaxis == 0 ) {
			break;
		}
		else if(nsteps_to_move < omega_steps ) {
			nsteps_to_move	-= 250;
			mar_par4	=  250;
			mar_par5       	= cfg.ome_speed;
		}
		else {
			mar_par4	= 0;
			mar_par5       	= 0;
		}

		stat_scanner_op = CMD_MOVE_OMEGA;
		motor_op	= stat_scanner_op;
		mar_number      = CMD_MOVE;
		mar_mode        = ARG_OMEGA;
		mar_par1        = ARG_MOVE_REL;
		mar_par2       	= nsteps_to_move;
		mar_par3       	= cfg.ome_speed;

		fprintf(stdout, "scan345: Moving OMEGA from %1.3f to %1.3f\n",stat_omega,target_omega);
		if( 0 == mar_command( )) return( 0 );
		
		break;

	case MDC_COM_ERASE:			/* Erase plate */
		cur_mode	= com_scanmode;
		mar_cmd 	= (int)task;
		erase_start	= -1;
		erase_lamps_ok  = 0;
		erase_time	= cfg.erasetime[ stat_scanmode ];

		/* Change mode */
		change_mode 	= 0;
		fprintf(stdout, "scan345: Erasing ...\n");
		i = mar_change_mode();
		break;

	case MDC_COM_SCAN:			/* Do a scan */

		mar_cmd = (int)task;

		/* Decide if we want spiral op */
		if ( input_keep_spiral || com_use_spiral )
			keep_spiral = 1;
		else
			keep_spiral = 0;

		/* Decide if we want xform */
		if ( input_keep_image )
			keep_image  = 1;
		else {
			keep_image  = 0;
			keep_spiral = 1;
		}

		stat_xray_units = 0.0;
		skip_op		= 0;
		erase_start	= -1;

		stat_scan_add	= 0;
		stat_scanner_op = CMD_SCAN;
		mar_number      = CMD_SCAN;
		current_pixel   = 0;
		cur_mode	= com_scanmode;
                cur_pixelsize   = cfg.pixelsize	[cur_mode];
                cur_diameter    = cfg.diameter 	[cur_mode];
		erase_lamps_ok  = 0;
		/* Change mode */
		change_mode 	= 1;
		i 		= mar_change_mode();
		break;

	case MDC_COM_COLL:		/* Collect */
		mar_cmd 	= (int)task;

		/* Store starting time */
		time(&tick);
		strcpy( start_time, (char *)ctime(&tick) );

		/* Initialize DOSE counters */
		expo_doseavg    = expo_dosesig = 0.0;
		expo_dosemin    = expo_dosebeg = 99999.0;
		expo_dosemax    = expo_doseend = 0.0;
		expo_dosen      = 0;
		memset( (char *)expo_dose, 0, sizeof(int)*MAX_DOSE );

		/* Initialize progress         */
		exposure_start	   = 0;

		/* Initialize status variables */
		stat_time       = com_time;
		stat_phibeg     = com_phibeg;
		stat_dphi       = com_dphi;
		stat_n_passes   = com_phiosc;
		stat_mode       = com_mode;
		stat_scanner_op = CMD_MOVE;
		totpass         = stat_n_passes;
		stat_phiend  	= stat_phibeg + stat_dphi;
		stat_start_phi	= stat_phi;

		/* Avoid rounding errors when driving PHI */
		com_phi_steps	= (int)( cfg.phi_steps * (stat_dphi * 100000. + 1 )/100000.);

		/* Time mode */
		if(stat_mode == ARG_TIME ) {
			if ( com_phi_steps == 0 || cfg.use_phi == 0 ) 
				stat_units = (cfg.units_time * stat_time);
			else
	    			stat_units = (cfg.units_time * stat_time)/(com_phi_steps*stat_n_passes);
		}
	
		/* Dose mode */
		else {
			stat_units = stat_time;
		}

		/* Send command to scanner */
		if ( stat_units >  0 ) {
			if ( com_phibeg != stat_phi )
				i = mar_move_phi( -9999. );
			else
				i = mar_start_expo();
		}

		break;

	case MDC_COM_MODE:			/* New scanning mode */
		mar_cmd = (int)task;
		cur_mode	= com_scanmode;	
		stat_scanner_op = CMD_CHANGE;
		change_mode 	= 1;
		i 		= mar_change_mode();
		break;

	default:
		return( 0 );
	}

#ifdef DEBUG
if ( debug & 0x02 )
printf("debug (marhw:marTask): MAR  %d, %d, %d, %d, %d, %d %d\n",
mar_number,mar_mode,mar_par1,mar_par2,mar_par3,mar_par4,mar_par5);
#endif

	return( 1 );
}

/******************************************************************
 * Function: get_status
 ******************************************************************/
int
get_status()
{
int		i,status=0;
static  time_t  before2=0;
extern int	status_interval;
extern int	net_stat();
extern int	net_data();
extern int	net_msg();
extern int	net_comm(int, char *);
extern void     get_command     ();


        /* Is there a new command ? */
        get_command();

	/* Get current time */
	now = time( NULL );
	
#ifdef SIMUL
	goto UPDATE;
#endif

	/* Return if scanner is not available */
	if ( netcontrol==0 ) goto UPDATE;

	i = net_data();

	/* Reading network status */
	while ( 1 ) {
		status = net_stat();
		if ( status < 200 ) break;
	}
	

#ifdef DEBUG
	if ( debug & 0x04 )
	printf("debug (marhw:get_status) read=%d\n",status);
#endif

	i = net_msg ( );

	if ( status_interval )
		PrintStatus();

UPDATE:
	/* Update status window */
	if ( (now-before2)> 1 ) {
		/* Set new time */
		before2 = time(NULL);
	}

	if ( !scan_in_progress ) {
		sleep( 1 );
	}

	return( status );
}

/******************************************************************
 * Function: process_status 
 ******************************************************************/
int
process_status( char *buf )
{
int		i,j,k;
static int 	p_active	[MAX_CMD];
static int	p_error		[MAX_CMD];
static int	p_task 		[MAX_CMD] = {1234,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
static char	first_time	= 1;
static int	p_gap		= 999;
static int 	p_counter	= -1;
/* The following gives the sequence of tasks to be handled by mar_process */
static int 	idx[8] = { CMD_SCAN, 	CMD_COLLECT, 	CMD_MOVE,
		           CMD_ERASE, 	CMD_SHUTTER, 	CMD_CHANGE,
			   CMD_LOCK,	CMD_ION_CHAMBER};


	memcpy( (char *)&esd_stb, buf, sizeof( ESD_STB ) );

	if ( esd_stb.counter <= p_counter ) {
		sprintf( str, "scan345: ERROR at STATUS block # %d (last was %d)\n",
			esd_stb.counter,p_counter);
		fprintf( stdout, str );
		return 0;
	}

	p_counter = esd_stb.counter;

	stat_pixels 	= esd_stb.valid_data;
	stat_scanmode 	= esd_stb.scanmode - 1;
	if ( stat_scanmode < 0 || stat_scanmode > 7 ) {
		stat_scanmode = 0;
	}
        stat_diameter    = cfg.diameter 	[stat_scanmode];

	/* Read status first time: set defaults for p_task to avoid action */
	if ( p_task[0] == 1234 ) {
		for ( i=0; i<MAX_CMD; i++ ) p_task[i] = esd_stb.task[i];
	}

	
	/*
	 * Resolve bits in hw_status1 
	 */
	stat_xray_shutter 	= esd_stb.hw_status1 & two32[ 11 ]; /* 20 */
	stat_reference_status  	= esd_stb.hw_status1 & two32[  9 ]; /* 22 */

	stat_erase_T		= esd_stb.hw_status1 & two32[ 21 ]; /* 10 */
	stat_erase_R		= esd_stb.hw_status1 & two32[ 20 ]; /* 11 */
	stat_erase_L		= esd_stb.hw_status1 & two32[ 19 ]; /* 12 */

	stat_erase_T_temp	= esd_stb.hw_status1 & two32[ 31 ]; /*  0 */
	stat_erase_R_temp	= esd_stb.hw_status1 & two32[ 25 ]; /*  6 */
	stat_erase_L_temp	= esd_stb.hw_status1 & two32[ 24 ]; /*  7 */

	stat_servo_warning 	= esd_stb.hw_status1 & two32[ 23 ]; /*  8 */
	stat_laser_status 	= esd_stb.hw_status1 & two32[ 22 ]; /*  9 */
	stat_lock_permit	= esd_stb.hw_status1 & two32[ 18 ]; /* 15 */

	/* Is the plate locked ? */
	if ( (esd_stb.hw_status1 & two32[16]) == 0)
		stat_plate_locked = 1;
	else
		stat_plate_locked = 0;

	/* Is the scanning head moving ? */
	if (  esd_stb.task[CMD_RADIAL] & 0x01 ) 
		stat_home = 1;
	else
		stat_home = 0;

	if (  esd_stb.task[CMD_SCAN] & 0x02  ) 
		stat_scan_active = 1;
	else
		stat_scan_active = 0;

	distance_steps  = esd_stb.stepper[0][STEPPER_IST];
	omega_steps     = esd_stb.stepper[1][STEPPER_IST];
	phi_steps       = esd_stb.stepper[5][STEPPER_IST];

	for ( i=0; i<6; i++ ) {
		if ( esd_stb.stepper[i][STEPPER_IST] != 
		     esd_stb.stepper[i][STEPPER_SOLL] ) 
			motor_moving[i] = 1;
		else
			motor_moving[i] = 0;
	}
			
	stat_intensity 	= esd_stb.intensity / 1000.;

	if ( cfg.use_phi ) {
		stat_phi   = (float)((float)phi_steps 	  /cfg.phi_steps);
		while( stat_phi < 0.0 ) {
			stat_phi	+= 360.;
		}
		while( stat_phi > 359.999 ) {
			stat_phi	-= 360.;
		}
	}
	if ( cfg.use_dist && distance_steps > 0 )
		stat_dist  = (float)((float)distance_steps/cfg.dist_steps);
	if ( cfg.use_ome )
		stat_omega = (float)((float) omega_steps  /cfg.ome_steps);

	erase_lamps_ok = 1;

	if ( stat_xray_shutter ) 
		stat_xray_shutter = SHUTTER_IS_CLOSED;
	else
		stat_xray_shutter = SHUTTER_IS_OPEN;

	/* Go through all tasks that we want to handle ( in array idx ) */
	for ( k=0; k<8; k++ ) {
		i  = idx[k];

		task_done	[i]  	= esd_stb.task[i] &  two32[  0 ];
		task_active	[i]	= esd_stb.task[i] &  two32[  1 ];
		task_queued	[i]	= esd_stb.task[i] &  two32[  4 ];
		task_started	[i]  	= esd_stb.task[i] &  two32[  2 ];
		task_error	[i]  	= esd_stb.task[i] &  two32[  8 ];

/*
		if ( stat_scan_active && i != CMD_SCAN ) goto SKIP1;
*/
		/* Do not process queued tasks */
		if ( !(esd_stb.task[i] & 0x10)  ) {

		    if ( ( (esd_stb.task[i] & 0x02 ) || esd_stb.task[i] != p_task[i] ) && i != CMD_ABORT ) {
			stat_task_number	= i;
			stat_task_active 	= task_active	[i];
			stat_task_done   	= task_done	[i];
			stat_task_error		= task_error	[i];

			if ( stat_task_error ) {
			    if ( first_time ) {
				stat_task_number = 0;
				goto SKIP1;
				continue;
				
			    }
			    fprintf(stdout,"scan345: ERROR ending of task %d\n",i);
			    fprintf( fpout,"scan345: ERROR ending of task %d\n",i);
			}

			j = mar_progress( stat_task_number, stat_task_active, stat_task_error );
	 	    }
		    else
			stat_task_number	= 0;
		}
SKIP1:
		p_active[i] = task_active[i];
		p_error [i] = task_error[i]; 
		p_task	[i] = esd_stb.task[i];
	}

	first_time = 0;

#ifdef OLDGAPS
	if ( p_gap == 0 && esd_stb.gaps[0] != 0 ) {
		stat_gap = esd_stb.gaps[0];
	}
	p_gap = esd_stb.gaps[0];
#endif

        /* Always keep non-zero values for GAPs: Reset only at start of scan */
        for (i=0;i<8;i++) {
                if ( esd_stb.gaps[i] != 0 )
                        stat_gap[i] = esd_stb.gaps[i];
        }

	return 1;
}

/******************************************************************
 * Function: mar_progress = keeps track of progress of data col
 ******************************************************************/
int	
mar_progress(int task, int active, int error)
{
char    		done=0;
int			fehler = 0;
float			ftmp;
static int		last_xray_shutter;
static int 		ntry=0;
static int    		pdone;
static char		p_move = 0;
static char		expo_first = 1;
extern void		output_image();

	/* Reset "done" */
	done = 0;

	/*
	 * Are we in error recovery mode ?
	 */

#ifdef DEBUG
	if ( debug & 0x08 )
	printf("debug (marhw:mar_progress): task=%d   act=%d  err=%d\n",task,active,error);
#endif

	/* Task is the most recent in the controllers task list */
	switch( task ) {

	case CMD_SHELL:

	   if ( active ) break;

	   done = 1;
	   /* Get error number */
	   if ( error ) {
		fehler = get_error( 0 );
		if ( fehler < 0 ) error = 0;
	   }
	   break;

	case CMD_MOVE:

	   /* Note: I have seen that this has been called twice after
	    * activity with active = 0. This prevents going on, especially
	    * when doing multiple oscillations. Therefore: react only once
 	    * to: task done!!!
	    */

	   if ( active == 0 && p_move == 0 ) break;
	   p_move = active;

	   /* Completeness */
	   if( active ) {
		stat_scanner_msg = 0; 
		if ( mar_mode == ARG_DISTANCE || mar_mode == ARG_THETA ) {
			if ( target_distance != original_distance )
				ftmp = 100 * (stat_dist-original_distance)/(original_distance - target_distance );
			else
				ftmp = 100.0;

			stat_scanner_msg = (int)(ftmp+0.5);
			if ( stat_scanner_msg < 0 ) stat_scanner_msg *= -1;
		}
		else if ( mar_mode == ARG_PHI || mar_mode == ARG_CHI ) {
			i = esd_stb.stepper[mar_mode-1][STEPPER_SOLL] - esd_stb.stepper[mar_mode-1][STEPPER_IST];
			if ( mar_par2 != 0 )
				ftmp = 100. * i/ (float)mar_par2; 
			stat_scanner_msg = 100-(int)(ftmp+0.5);
			if ( stat_scanner_msg <  0 ) stat_scanner_msg *= -1;
		}
		break;
	   }

	   /* Get error number */
	   if ( error ) {
		fehler = get_error( 0 );
		if ( fehler < 0 ) error = 0;
	   }

	   /* Distance movement */
	   if ( mar_mode == ARG_DISTANCE ) {
		if( cfg.use_dist == 0 ) {
			done = 1;
			break;
		}

		/* Task error */
		if ( error ) {

			stat_scanner_op = CMD_MOVE_DISTANCE;

			/* Cannot leave reference switch */
			if ( fehler == 225 && 
			    (distance_steps == (int)(cfg.dist_min *cfg.dist_steps) || distance_steps == (int)(cfg.dist_max *cfg.dist_steps) ) ) {
				marError(225, 0 );
				done = 1;
				mar_number 	= CMD_MOVE;
				mar_mode	= ARG_DISTANCE;
				mar_par3  	= cfg.dist_speed;
				mar_par1 	= 2*(( cfg.dist_max - cfg.dist_min )*cfg.dist_steps/cfg.dist_speed);
				if ( distance_steps <= (int)(cfg.dist_min *cfg.dist_steps )) {
				   if ( dist_retry < 2 ) {
					mar_par1   	= ARG_INIT_NEAR;
					mar_par2  	= cfg.dist_min*cfg.dist_steps;
				   }
				   else {
					mar_par1   	= ARG_INIT_FAR;
					mar_par2  	= cfg.dist_max*cfg.dist_steps;
				   }
				}
				else {
				   if ( dist_retry < 2 ) {
					mar_par1   	= ARG_INIT_FAR;
					mar_par2  	= cfg.dist_max*cfg.dist_steps;
				   }
				   else {
					mar_par1   	= ARG_INIT_NEAR;
					mar_par2  	= cfg.dist_min*cfg.dist_steps;
				   }
				}

			}

			/* Reference switch touched */
			else if ( fehler == 225 ) {
				marError( 225, 1);
				done = 1;
				break;
			}

			/* Cannot FIND reference switch */
			else if ( fehler == 217 ) {
				marError( 217, 0);
				done = 1;
				break;
			}

			dist_retry++;

			done = 1;
			if ( dist_retry < 4 ) { 
				i = mar_command( );
			}
			else {
				done = 1;
			}

			break;
		}

		/* End of task */
		else {
			dist_retry = 0;
			done = 1;
			break;
		}
	   }


	   /* OMEGA movement */
	   else if ( mar_mode == ARG_OMEGA ) {

		/* OMEGA movement disabled... */
		if( cfg.use_ome == 0 ) {
			done = 1;
		    	break;
		}

		if ( active ) break;

		/* Task ended. */
		done = 1;
		break;
	   }

	   /* PHI movement */
	   else if ( mar_mode == ARG_PHI ) {

		/* Phi movement disabled... */
		if( cfg.use_phi == 0 ) {
			done = 1;
		    	break;
		}

		/* Task in progress */
		if ( active ) break;

		/* Task ended.
		 */
		stat_start_phi	= stat_phi;

		/* In data collection mode, start next exposure */
		if ( mar_cmd == MDC_COM_COLL ) {
			done = 0;
			i=mar_start_expo( );
		}
		else {
			done = 1;
			/* Adjust PHI to modulo 360.0 deg */
			i = Modulo360();
		}
	    }

	    break;

	case CMD_ABORT:
		if ( active ) break;
		if ( active == 0 && p_task_active == 0 ) break;

		if ( error ) {
			fehler = get_error(1);
			if ( fehler < 0 ) error = 0;
		}

		if ( scan_in_progress ) {
			done=0;
			break;
		}
		else {
			marTask( MDC_COM_CHAMBER, 1.0 );

			/* Adjust PHI to modulo 360.0 deg */
			i = Modulo360();

			sleep( 1 );
			done = 1;
		}
		break;

	case CMD_SHUTTER:

		/* Task in progress */
		if ( active ) break;

		/* Task done */
		if ( error ) {

			fehler = get_error(0);
			if ( fehler < 0 ) 
				error = 0;
			else {
#ifdef SKIP_RECOVER
				printf("scan345: Shutter error\n");
				done = 1;
				break;
#endif

				ntry =0;
				if ( fehler == 205 ) {
					/* Cannot open shutter, close it !*/
					marError( 205, ntry ); 
					stat_scanner_op = CMD_SHUTTER;
					mar_number    	= CMD_SHUTTER;
					mar_mode      	= ARG_CLOSE;
					mar_par1 	= mar_par2 = 0;
					mar_par3 	= mar_par4 = mar_par5 = 0;
					if( 0 == mar_command( )) return( 0 ); 
				}
				else if ( fehler == 210 ) {
					/* Cannot close shutter */
					marError( 210, ntry ); 
				}
			}
		}
		else {
			if ( mar_cmd == MDC_COM_SHUT ) {
				done = 1;
				ntry = 0;
			}
			else 
				done = 0;
		}

		/* Initialize of startup: move distance */
		if( mar_cmd == MDC_COM_INIT && cfg.use_dist ) {
			mar_number    	= CMD_MOVE;
			mar_mode	= ARG_DISTANCE;
                        if (target_distance == cfg.dist_max) {
                                mar_par1  = ARG_INIT_FAR;
				mar_par2  = (cfg.dist_max + 0.5 * (1. / cfg.dist_steps)) * cfg.dist_steps;
                        }
                        else {
                                mar_par1  = ARG_INIT_NEAR;
				mar_par2  = (cfg.dist_min + 0.5 * (1. / cfg.dist_steps)) * cfg.dist_steps;
                        }
			mar_par3 = cfg.dist_speed;
			mar_par4 = 0;
			mar_par5 = cfg.dist_speed/10;	/* CHECK !!! */
			if( 0 == mar_command( )) return( 0 ); 

			done		= 0;
			stat_scanner_op = CMD_MOVE_DISTANCE;
			motor_op	= stat_scanner_op;
		}

		break;

	case CMD_ERASE:		/* Erase plate */

		if ( erase_start > 0 && erase_time > 0.00 )
			stat_scanner_msg = 100 * ( time(NULL) - erase_start)/erase_time;
		else
			stat_scanner_msg = 0;

		/* Task still in progress */
		if ( active ) break;

		/* 
		 * While IP is erasing, the erase_status bit tells us
		 * if the lamps are on
		 */

		/* Erase finished... */
		if( error ) {
			fehler = get_error(1);
			if ( fehler < 0 ) 
				error = 0;
			else {
				done = 1;
#ifdef SKIP_RECOVER
				printf("scan345: ERASE error\n");
				break;
#endif

				break;
			}
		}

		pdone= 0;
		done = 1;
		break;

	case CMD_LOCK:

		if( active ) break;
		
		if( error ) {

			fehler = get_error(1);
			if ( fehler < 0 ) 
				error = 0;
			else {
#ifdef SKIP_RECOVER
				/* SKIP RECOVERY */
				printf("scan345: LOCK error\n");
				done = 1;
				break;
#endif
				/* Cannot lock */
				if ( fehler == 60 ) {
					done = 1;
					break;
				}
			}
		}
		if ( mar_cmd == MDC_COM_STARTUP ) {
			done = 1;
		}
		break;

	case CMD_CHANGE:
		done = 0;
		if ( active ) break;

		/* Simple mode change ... */
		if ( mar_cmd == MDC_COM_MODE || mar_cmd == MDC_COM_STARTUP ) {

			/* Adjust PHI to modulo 360.0 deg */
			i = Modulo360();

			done = 1;
		}

		if ( error ) { 
			fehler = get_error( 1 );
			if ( fehler < 0 ) 
				error = 0;
			else {
				done = 1;
				scan_in_progress = 0;
				break;
			}
		}

		/* Single scan: start scan ... */
		if ( mar_cmd == MDC_COM_SCAN ) {
			/* Send command to controller */
			i = StartScan();
			done = 0;
		}
		/* COLLECT or ERASE: erase plate  ... */
		else if ( mar_cmd == MDC_COM_COLL ) {
			i = mar_erase();
			break;
		}
		break;

	case CMD_COLLECT:
		if ( exposure_start == 0 && totpass == stat_n_passes ) {
			exposure_start = time(NULL);
		}

		/* Task in progress */
		if ( active ) {
		    /* Keep first xray_intensity */
		    if ( expo_first ) {
			xray_start = stat_intensity;
			expo_first = 0;
		    }

		    done 	= 0;

		    /* 
		     * Delta phi = 0.0 in TIME mode: 
		     * follow progress using the elapsed time.
		     */
		    if ( com_phi_steps <  1 || cfg.use_phi == 0 ) {
			if ( exposure_start > 0 )
				pdone = 100 * ( time(NULL) - exposure_start )/stat_time;
			else
				pdone = 0;
		    }
		    /* 
		     * Delta phi > 0.0 and TIME or DOSE mode:
		     * follow progress using the phi movement.
		     */
		    else {
			if ( stat_phi >= stat_start_phi )
			    ftmp = stat_dphi * ( stat_n_passes - totpass ) + stat_phi - stat_start_phi;
			else
			    ftmp = stat_dphi * ( stat_n_passes - totpass );
			pdone = 100 * ftmp / ( stat_dphi * stat_n_passes );
					
		    }
		    stat_scanner_msg 	= pdone;
		
		    if ( stat_n_passes > 1 ) {
			stat_oscil_msg = stat_scanner_msg;
		    }
		   
		    /* 
		     * Abort data collection if X-rays drop below
		     * minimum intensity level  
		     */
		    if ( stat_intensity < cur_intensmin &&
			 cfg.use_xray == 0 ) {
				if ( dc_stop == 0 )
					marError( 1060, 0 );
				dc_stop = 1;
		    }
			
		    /* Sum up current intensity */
		    if ( expo_dosen < MAX_DOSE )
		    expo_dose[expo_dosen]= stat_intensity;
                    expo_dosen++;
                    expo_doseavg    	+= stat_intensity;
		    stat_xray_units 	+= stat_intensity;
		    expo_doseend 	=  stat_intensity;
		    if ( expo_dosebeg > 99990 ) 
		    expo_dosebeg 	=  stat_intensity;
                    max0( expo_dosemax, stat_intensity);
                    min0( expo_dosemin, stat_intensity);
		    intensity_counter++;
		    break;
		}

		expo_first = 1;

		/* Task ended with an error */
		if ( error ) {

		    fehler = get_error( 0 );

		    if ( fehler < 0 ) 
			error = 0;
		    else {

			/* Cannot CLOSE shutter... */
			if ( fehler == 238 ) {
				marError( fehler, 0);
			}
			/* Cannot OPEN shutter... */
			else if ( fehler == 215 || fehler == 241 ) {
				marError( fehler, 0);
			}
			/* Shutter is already OPEN... */
			else if ( fehler == 234 ) {
				marError( fehler, 0);
			}
			/* Stepper error */
			else if ( fehler == 239 ) {
				marError( fehler, 0);
			}

			done = 1;
			break;
		    } /* End of fehler */
		}

		/*
		 * Exposure finished... 
		 */
		totpass--;

		/* This was not the last oscillation */
		if ( totpass > 0 && cfg.use_phi ) {
			/* Drive PHI back to start and redo exposure */
			if ( stop_image > 1 ) {
				done = 1;
				break;
			}

			stat_scanner_op = CMD_MOVE_PHI;
			i = mar_move_phi( (double)-stat_dphi );
			done = 0;
			break;
		}

		done = 1;
		break;

	case CMD_SCAN:				/* Do a scan */
/*
printf("task=%x active=%d err=%d\n",esd_stb.task[CMD_SCAN],active,stat_task_error);
*/

		/* Task in progress */
		if ( active ) {
			if ( maximum_pixels > 0 && stat_pixels < maximum_pixels ) {
				pdone = (int)(100*stat_pixels/(float)maximum_pixels);
			}
			else if ( stat_pixels >= maximum_pixels && pdone > 50 )
				pdone = 100;
			else
				pdone = 0;

			stat_scanner_msg = pdone;
			break;
		}

		/* SCAN finished ... */

		scan_started 		= 0;

		/* ERROR ending ... */	
		if ( error ) {
			fehler = get_error( 0 );
			
			if ( fehler < 0 ) {
				error = 0;
			}
			else {

				/* Terminate data collection! */
				marError( fehler, 0 );
				scan_in_progress= 0;
				done 		= 1;
				if ( mar_cmd == MDC_COM_SCAN )  break;
				/* Abort queued or active exposure */
				mar_number    	= CMD_ABORT;
				mar_mode	= CMD_COLLECT;
				mar_par1 	= mar_par2 = 0;
				if( 0 == mar_command( )) return( 0 );
				break;
		    	} /* End of fehler */
		}

		done 		= 1;
		pdone 		= 0;
		stat_scanner_msg= 100;

		break;

	case CMD_SET:				/* Commands without actions */
	case CMD_ION_CHAMBER:
		done = 1;
		break;

	default:
		marError( 999, task );
		done = 1;
		break;
	}

	/*
	 * End of cases
	 */

	last_xray_shutter = stat_xray_shutter;
				
	time( &now );	
	if ( done ) {
		stat_scanner_msg     = MAR_CONTROL_IDLE;
		stat_scanner_op      = MAR_CONTROL_IDLE;
		stat_scanner_control = MAR_CONTROL_IDLE;
		if ( fpmsg != NULL ) {
			fprintf(fpmsg,"scan345: Task %s ENDED   at %s",
				cmdstr[ stat_task_number ], (char *)ctime( &now ));
			fflush( fpmsg );
		}
		if ( verbose )
		fprintf(stdout,"scan345: Task %s ENDED   at %s\n",
			cmdstr[ stat_task_number ], (char *)ctime( &now ));
	}
	else {
		stat_scanner_control = MAR_CONTROL_ACTIVE;
		if ( time(NULL) - last > 1 ) {
		    if ( fpmsg != NULL ) {
			fprintf(fpmsg,"scan345: Task %s %3d %% complete\n",
				cmdstr[ stat_task_number ], stat_scanner_msg);
			fflush( fpmsg );
		    }
		    if ( verbose > 1 )
			fprintf(stdout,"scan345: Task %s %3d %% complete\n",
				cmdstr[ stat_task_number ], stat_scanner_msg);
		    last = time( NULL );
		}

	}

	/* 
	 * Version 2.2: Are there some more erase cycles?
	 */
	if ( done && com_scan_add && task == CMD_SCAN ) {
		if ( fpmsg != NULL ) {
			fprintf(fpmsg,"scan345: %d more SCAN%s to add ...\n", com_scan_add, com_scan_add > 1 ? "S":"");
			fflush( fpmsg );
		}
		if ( verbose )
			fprintf(stdout,"scan345: %d more SCAN%s to add ...\n", com_scan_add, com_scan_add > 1 ? "S":"");
		com_scan_add--;
		mar_cmd = MDC_COM_SCAN;
		done = 0;
		/* Start next erase command */
		i = mar_erase();
	}
	if ( done && com_scan_erase && ( task == CMD_SCAN || task == CMD_ERASE ) ) {
		if ( fpmsg != NULL ) {
			fprintf(fpmsg,"scan345: %d more ERASE cycle%s to go ...\n", com_scan_erase, com_scan_erase > 1 ? "s":"" );
			fflush( fpmsg );
		}
		if ( verbose )
			fprintf(stdout,"scan345: %d more ERASE cycle%s to go ...\n", com_scan_erase, com_scan_erase > 1 ? "s":"" );
		com_scan_erase--;
		mar_cmd = MDC_COM_ERASE;
		done = 0;
		/* Start next erase command */
		i = mar_erase();
	}

	return ( (int)done); 
}
 
/******************************************************************
 * Function: mar_lock
 ******************************************************************/
int
mar_lock()
{
int		i;

	if ( stat_plate_locked ) return 1;

	/* Try SERVO_SPEED 0 */
	if ( stat_lock_permit != 0 ) {
		sprintf( str, "scan345: IP still spinning. Halting plate ...\n" );
		fprintf( fpout, str );
		fprintf(stdout, str );
		sprintf ( mar_str, "servo_speed 0\r");
		if( 0 == mar_command( )) return( 0 ); 
		for ( i=0; i<2; i++ ) {
			sleep ( 1 );
			get_status();
		}
	}

	if ( stat_lock_permit != 0 ) {
		sprintf( str, "scan345: IP still spinning. Cannot lock plate ...\n" );
		return 0;
	}

	sprintf( str, "scan345: Locking plate...\n" );
	fprintf( fpout, str );
	fprintf(stdout, str );

	mar_number    	= CMD_LOCK;
	mar_mode	= 1;
	mar_par1	= cfg.prelock_speed;
	mar_par2	= cfg.lock_speed;
	mar_par3	= 0;
	mar_par4	= 0;
	mar_par5	= 0;
	mar_str[0]	= '\0';
	if( 0 == mar_command( )) return( 0 ); 

	sleep( 1 );

	return 1;
}

/******************************************************************
 * Function: mar_servo
 ******************************************************************/
int
mar_servo()
{
int		i;
static int	ntimes = 0;
extern int	net_close();

	/* Servo system is not yet ready, keep looking ... */
	if ( esd_stb.servo_state != 0 && ntimes < 30 ) {
		ntimes++;
		if ( ntimes < 30 ) {
			sprintf( str, "scan345: Waiting for SERVO system ...\n");
			fprintf( stdout, str);
			fprintf( fpout , str);
			sleep( 1 );
			i=mar_servo();
			return 0;
		}
		/* After 30 seconds not READY: here we have a problem */
		ntimes = 0;

		marError( 1001, esd_stb.servo_state);

		/* Close socket */
		for ( i=0; i<4; i++ ) {
			if ( net_close( i ) ) {
				sprintf( str, "scan345: socket %d closed ...\n",i+1);
				fprintf(  fpout, str );
				fprintf( stdout, str );
			}
		}
		netcontrol = 0;
		return 0;
	}

	sprintf( str, "scan345: SERVO system READY ...\n");
	fprintf( stdout, str);
	fprintf( fpout , str);

	/* SERVO system was successfully initialized, proceed to:
	 * a) LOCK IP
	 * b) Change mode to default
	 */

	/* Lock plate */
	i=mar_lock();

	/* Are we done ? */
	if ( esd_stb.scanmode > 0 && esd_stb.scanmode < 9 ) {

		/* Adjust PHI to modulo 360.0 */
		i = Modulo360();
		return 1;
	}

	/* ... not yet: go to defined scan mode */
	sprintf( str, "scan345: Initializing default scanmode ...\n" );
	fprintf( fpout, str );
	fprintf(stdout, str );

	return 1;
}

/******************************************************************
 * Function: mar_initial_status
 ******************************************************************/
void
mar_initial_status()
{
	stat_scanmode		= 0;
	stat_omega     		= 0.0;
	stat_phibeg 		= 0.0;
	stat_dphi 		= 1.0;
	stat_n_images  		= 1;
	stat_n_passes  		= 1;
	stat_time      		= 60;
	stat_scanner_op  	= MAR_CONTROL_IDLE;
	stat_scanner_msg    	= MAR_CONTROL_IDLE;
	stat_scanner_control 	= MAR_CONTROL_IDLE;
	stat_xray_shutter 	= SHUTTER_IS_CLOSED;
	stat_fname[0]   	= '\0';
	strcpy( stat_dir, 	  working_dir );
        memset( (char *)stat_gap, 0, 8*sizeof(int) );

}

/******************************************************************
 * Function: marError = prints error numbers of errors
 ******************************************************************/
void
marError( int err_no, int idata ) 
{
char    s1[128],s2[128], s3[64], s4[128];
char    *cptr;
char    timestr[26];
int     nl=1;
int     status = 0;

	time(&tick);
	cptr = (char *) ctime(&tick);

	/* Clear s1, s2 ...*/
	sprintf( s1, "\0" );
	sprintf( timestr, "\0" );

	/* Write error number... */
	sprintf( s1, "scan345: Error no. %d\0",err_no );
	fprintf(  fpout, "%s\n", s1 );
	fprintf( stdout, "%s\n", s1 );

	/* Write current time ... */
	strncpy( timestr, cptr, 24);
	timestr[24]='\0';
	timestr[3]='-';
	timestr[7]='-';
        sprintf( s1, "scan345: Current time  is: %s\0", timestr );
	fprintf(  fpout, "%s\n", s1 );
	fprintf( stdout, "%s\n", s1 );

	/* Write current image (during data collection only) ... */
	if ( mar_cmd == MDC_COM_COLL || 
	     mar_cmd == MDC_COM_SCAN ) {
		sprintf( s4, "%s", stat_fname );
		sprintf( s1, "scan345: Current image is: %s\0", s4 );
		fprintf(  fpout, "%s\n", s1 );
		fprintf( stdout, "%s\n", s1 );
	}
	else
		sprintf( s4, "\0" );

	/* Reset strings ... */
	sprintf( s1, "\0" );
	sprintf( s2, "\0" );
	sprintf( s3, "\0" );

	/* Write error message ... */
	if ( err_no > 998 ) goto OTHER_ERRORS;

	/*
	 * ESD errors ...
	 */

	if ( err_no == 225 ) {
	    if ( idata == 0 )
		sprintf(s1, "Cannot LEAVE distance reference position !!!\0");
	    else
		sprintf(s1, "Distance reference position touched ...\0");
	}
	else {
		sprintf(s1, "%s", err_msg[fehler_index].msg);
	}
	nl 	= 1;
	status 	= 1;

	goto END_ERRORS;

	/*
	 * Miscellaneous other errors 
	 */

OTHER_ERRORS:

	switch( err_no ) {

	/* 
	 * 	Fatal errors...
	 */
	case 1000:
	    	sprintf(s1,"Error sending command to mar controller.\0");
		sprintf(s2,"Command number: %d\0", idata );
		status = 1;
		nl     = 2;
		break;

	case 1001:
		sprintf(s1, "SERVO system cannot be INITIALIZED\0");
		sprintf(s2 ,"Giving up. NO more scanner CONTROL\0"); 
		sprintf(s3 ,"Please retry by switching off the scanner ...\0"); 
		status = 1;
		nl     = 3;
		break;

	case 1112:
		sprintf(s1 ,"Error writing image array\0");
		status = 1;
		break;

	case 1115:
		sprintf(s1 ,"Cannot write image header\0");
		status = 1;
		break;

	case 1050:
	    	sprintf(s1,"SHUTTER did not work properly.\0");
		sprintf(s2,"Abandoning data collection...\0");
		status = 1;
		nl     = 2;
		break;

	case 1070:
	    	sprintf(s1,"Could not recover from previous errors\0");
		sprintf(s2,"after 5 trials.\0");
		status = 1;
		nl     = 2;
		break;

	case 1020:
		sprintf(s1, "Not enough disk space left in %s!\0", stat_dir);
		sprintf(s2, "Aborting data collection ...\0");
		status = 1;
		nl     = 2;
		break;

	/* 
	 * 	WARNINGS ...
	 */

	case 1080:
		if (idata==0)
	 		sprintf(s1,"Shutter did not open during exposure\0");
		else
	 		sprintf(s1,"Shutter did not close at end of exposure\0");
		sprintf(s2, "Trying to fix shutter by doing 5 x open/close\0");
		nl     = 2;
		status = 0;
		break;

	case 1120:
		sprintf(s1 ,"Cannot open spiral file %s\0",spiral_file);
		status = 0;
		break;

	case 1121:
		sprintf(s1 ,"Cannot write spiral header\0");
		status = 0;
		break;

	/* 
	 * 	Informations...
	 */
        case 1030:
                sprintf(s1, "The Image Plate has been exposed to X-rays!\0");
                sprintf(s2, "Please, ERASE plate before next exposure...\0");
                status = 2;
                nl     = 2;
                break;

	case 1010:
		sprintf(s1, "Waiting for X-rays...\0");
		status = 2;
		nl     = 1;
		break;

	case 1060:
		sprintf(s1 ,"X-ray reading too low (%1.2f). \0", stat_intensity);
		sprintf(s2 ,"Check generator or beam shutter!!!\0");
		sprintf(s3 ,"Data collection will end after current image...\0");
		status = 2;
		nl     = 3;
		break;

	case 1100:
		sprintf(s1, "Cannot open nb_code!\0");
		status = 1;
		nl     = 1;
		break;

	case 1101:
		sprintf(s1, "No scan modes found in nb_cde...\0");
		status = 1;
		nl     = 1;
		break;

	case 1102:
		sprintf(s1, "Something wrong with byteorder in nb_code\0");
		status = 1;
		nl     = 1;
		break;

	case 1103:
		sprintf(s1, "Scanner serial number in nb_code differs from config\0");
		status = 2;
		nl     = 1;
		break;

	case 1105:
		sprintf(s1, "No suitable scanning mode found in nb_code\0");
		status = 1;
		nl     = 1;
		break;

	case 1110:
		sprintf(s1 ,"Cannot create image file %s\0",image_file);
		status = 2;
		break;

	case 1111:
		sprintf(s1 ,"Cannot open image file %s\0",image_file);
		status = 2;
		break;
	/* 
	 * 	Error sending commands to controller ... 
	 */
	case 999:
		sprintf(s1 ,"Task %d NOT implemented !!!\0",idata);
		break;
	default:
		break;
	}

END_ERRORS:

	/* Write to output ... */
	fprintf( stdout, "scan345: %s\n", s1 );
	fprintf(  fpout, "scan345: %s\n", s1 );

	if (nl>1) {
		fprintf( stdout, "        %s\n", s2 );
		fprintf(  fpout, "        %s\n", s2 );
	}
	if (nl>2) {
		fprintf( stdout, "        %s\n", s3 );
		fprintf(  fpout, "        %s\n", s3 );
	}

	fflush(  fpout      );

	mar_error = err_no;
}
				  
/******************************************************************
 * Function: StartScan
 ******************************************************************/
static int
StartScan()
{
int 	i;

	/* Is there enough disk space ? */
	if ( NotEnoughDiskSpace() ) {
			return 0;
	}

	if ( intensity_counter > 0 )
		sum_xray_units = stat_xray_units/intensity_counter;

	/* Set to 0 only in last scan after adding previous ones */
	if ( com_scan_add == 0 ) {
		stat_xray_units   = 0.0;
		intensity_counter =   0;

		if ( expo_dosen > 0 ) {
			expo_doseavg /= expo_dosen;

			expo_dosesig = 0.0;
			/* Get sigma for DOSE */
			for ( i=0; i<expo_dosen; i++ ) {
				ftmp =  expo_dose[i]/1000. - expo_doseavg;
				expo_dosesig += ( ftmp*ftmp ); 
			}
			expo_dosesig = sqrt( expo_dosesig / expo_dosen );
		}

		/* Keep last X-ray reading */
		if ( stat_intensity > 0.0 && xray_start > -999. && cfg.use_xray == 0) {
			ftmp = xray_start/stat_intensity;

			/* TIME mode: reset exposure time to used time */
			if ( stat_mode == ARG_TIME )
				exposure_time = (int)stat_time;
			else {
				exposure_time = time(NULL)-exposure_start;
			}
		}
	}

	memset( (char *)stat_gap, 0, sizeof(int)*8);

	totpass   	= stat_n_passes;
	stat_phiend  	= stat_phi;
	stat_start_phi	= stat_phi;
	scan_error   	= 0;
	current_pixel	= 0;
	stat_scanner_msg= 100;
	stat_scanner_msg= 0;
	exposure_start  = 0;


	/* Add final slash to directory path */
	if ( stat_dir[ strlen(com_dir) - 1] != '/' )
		strcat( com_dir, "/\0" );

	sprintf(str,"%s%s",com_dir,com_root);

	/* When summing up scans, append a,b,c... to image number */
	if ( com_scan_add || stat_scan_add ) {
		sprintf(spiral_file,"%s.%c.s"  , str, (char)(stat_scan_add+97));
	}
	else
		sprintf(spiral_file,"%s.s"  , str);

	if ( com_format == OUT_MAR )
		sprintf(image_file, "%s.mar", str );
	else if ( com_format == OUT_CBF )
		sprintf(image_file, "%s.cbf", str );
	else if ( com_format == OUT_CIF )
		sprintf(image_file, "%s.cif", str );
	else if ( com_format == OUT_PCK )
		sprintf(image_file, "%s.pck", str );
	else if ( com_format == OUT_IMAGE )
		sprintf(image_file, "%s.image", str );

	sprintf( str, "%1.0f", (float)(cur_diameter/cur_pixelsize));
	if ( com_format == OUT_MAR || com_format == OUT_CIF || com_format == OUT_CBF )
		strcat( image_file, str );
	strcat(spiral_file, str );

	if ( expo_dosen < 1  ) {
		sum_xray_units 	= stat_intensity;
		expo_doseavg 	= stat_intensity;
		expo_dosemin 	= expo_doseavg;
		expo_dosemax 	= expo_doseavg;
		expo_dosesig 	= 0.0;
		expo_dosen   	= 1;
	}

	erase_lamps_ok 	= 0;
	stat_scanner_msg= 0;
	scan_started	= 1;

	/* Send command to controller */
	com_scanmode 	= stat_scanmode;
	stat_scanner_op = CMD_SCAN;
	mar_number 	= CMD_SCAN;
	mar_mode 	= cfg.roff[ stat_scanmode ];
	mar_par1   	= (int)cfg.flags;
	mar_par2	= (int)cfg.adcoff[ stat_scanmode ];/* ADC offset */
	mar_par3	= 0;

	i = mar_command( );

	/* Start data readout */   
	if ( mar_start_scan_readout(stat_scan_add) == 0 ) {
	    printf("scan345: ERROR: SCAN could not be started\n");
	    marTask( MDC_COM_ABORT, 0.0 );
	}

	op_doseavg	= expo_doseavg;
	op_dosemin	= expo_dosemin;
	op_dosemax	= expo_dosemax;
	op_dosesig	= expo_dosesig;
	op_dosebeg	= expo_dosebeg;
	op_doseend	= expo_doseend;
	op_dosen	= expo_dosen;

	stat_scan_add++;

	/* Reset DOSE counters for exposure */
	if ( com_scan_add == 0 ) {
		expo_doseavg    = expo_dosesig = 0.0;
		expo_dosemin    = expo_dosebeg = 99999.0;
		expo_dosemax    = expo_doseend = 0.0;
		expo_dosen      = 0;
		memset( (char *)expo_dose, 0, sizeof(int)*MAX_DOSE );
	}

	ict1 		= ict2 = 0;

	return i;
}

/******************************************************************
 * Function: Modulo360
 ******************************************************************/
static int 
Modulo360()
{
int 	i;
int	steps;
float	ftmp, real_phi;

	real_phi   = (float)((float)esd_stb.stepper[5][STEPPER_IST]/ cfg.phi_steps);

	/* Adjust PHI to modulo 360.0 deg */
	if ( real_phi < 0.0 || real_phi >= 360.0 ) {
		ftmp           	= real_phi;
		while( ftmp <    0.0) 
				ftmp += 360.;
		while( ftmp >= 360.0) 
				ftmp -= 360.;

		sprintf(str, "scan345: Adjusting PHI (%1.3f -> %1.3f)\n",real_phi,ftmp);
		fprintf( stdout, str );
		fprintf(  fpout, str );
		steps      	= (ftmp+0.5 *(1./cfg.phi_steps))*cfg.phi_steps;

		mar_number   	= CMD_SET;
		mar_mode   	= ARG_WRITE;
		mar_par1   	= 80;
		mar_par2   	= steps;
		i = mar_command( );

		stat_phi = ftmp;

		/* Waste some time */
		i = 0;
		sleep(1);
		return 1;
	}

	return 0;
}

/******************************************************************
 * Function: print_msg = prints output on MESSAGE port
 ******************************************************************/
void print_msg(char *s)
{
int             i,j,ival,jval;
char            b[8],op[128];
char            *sp;
static char     erledigt[4] = { 0, 0, 0, 0 };


	/* Return if scanner is not available */
	if ( fpspy == NULL ) return;

	op[0] = '\0';

        sp = strstr( s, "MESS" );
        if ( sp == NULL )
                strcpy( op, s );
        else {
                j = strlen( s ) - strlen( sp );
                /* There was text before string MESS: print it first */
                if ( j > 0 ) {
                        strcpy( op, s);
                        op[ j ] = '\n';
                        op[ j+1 ] = '\0';
                        fprintf( fpspy, "%s", op);
                        fflush ( fpspy );
                }
                /* Chop of MESS of remaining part of the string */
                strcpy( op, sp+4);
        }

	/* Chop off blanks at end */
	j = strlen( op )-1;
	for ( i=j; i>0; i-- ) {
		if ( op[i] != '\n' && op[i] != ' ' ) break;
		op[i] = '\0';
	} 
	op[i+1] = '\n';
	op[i+2] = '\0';

	/* Print */
	fprintf( fpspy, "%s", op);
	fflush ( fpspy );

	/* Check for SETPARAMETER */
	if (  ( sp = strstr( op, "SETPARAM" ) ) == NULL ) return;
	if ( strlen( sp ) < 44 ) return;

	sscanf( sp+17, "%d", &ival ); 
	sscanf( sp+34, "%d", &jval ); 

	op[0] = '\0';
	if ( ival == 95 && !erledigt[0]) {
		sprintf( op, "scan345: ESD Controller no.\t\t%d\n",jval);
		erledigt[0] = 1;
	}
	else if ( ival == 97 && !erledigt[1]) {
		sprintf( op, "scan345: ESD Firmware version\t\t%d\n",jval);
		erledigt[1] = 1;
	}
	else if ( ival == 278 && !erledigt[2]) {
		sprintf( op, "scan345: ESD RT-OS    version\t\t%d\n",jval);
		erledigt[2] = 1;
	}
	else if ( ival == 279 && !erledigt[3]) {
		memcpy( b, (char *)&jval, 4);
#if ( __linux__ || __osf__ )
		swaplong( b, 4 );
#endif
		b[4] = '\0'; 
		sprintf( op, "scan345: ESD Servo    version\t\t%s\n",b);
		erledigt[3] = 1;
	}
	if ( strlen( op ) ) {
		fprintf( stdout, op ); 
		fprintf(  fpout, op ); 
	}
}

/******************************************************************
 * Function: get_error = prints error numbers from status block
 ******************************************************************/
static int get_error(int mode)
{
int 		j,i,k,e;
int		result=0;
STB_MSG		msg;

	fehler_index = 0;

	for ( i=0; i<10; i++ ) {
		if ( esd_stb.errors[i] == 0 ) break;

		e = esd_stb.errors[i];
		memcpy ( (char *)&msg, (char *)&e, 4 );
		if ( msg.class == 3 ) continue;

		j = 1;
		while ( err_msg[j].task < 99  ) { 
			if ( msg.number == err_msg[j].number ) break;
			j++;
		}
		if ( err_msg[j].task == 99 ) {
			sprintf( str, "scan345: ERROR %4d %4d %4d\n",
				msg.task,msg.class,msg.number );
			fprintf( stdout, str );
			fprintf(  fpout, str );
		}
		else {
			fehler_index = j;
			sprintf( str, "scan345: ERROR %4d %4d %4d: %s\n",
				msg.task,msg.class,msg.number,err_msg[j].msg );
			fprintf( stdout, str );
			fprintf(  fpout, str );
		}

		/* Get any errors excluded ? */
		if ( cfg.use_error[0] > 0 ) {
			for ( k=1; k<cfg.use_error[0]; k++ ) {
				if ( msg.number == cfg.use_error[k] ) {
					sprintf( str, "scan345: Ignoring error # %d ...\n",msg.number);
					fprintf( stdout, str );
					fprintf(  fpout, str );
					result = -1;
					break;
				}
			}
		}

		if ( result != -1 ) {
			if ( msg.class == 1 ) 
				result = msg.number;
			if ( result == 0 && msg.class == 3 ) 
				result = msg.number;
		}
	}

	if ( result > 0 && mode ) {
		marError( result, 0 );
	}

	return result;
}

/******************************************************************
 * Function: OpenMessage()
 ******************************************************************/
int OpenMessage()
{
	if ( fpmsg != NULL ) {
		fclose(fpmsg); 
		fpmsg=NULL;
	}

	if(NULL == (fpmsg = fopen( msg_file, "w+"))) {
		fprintf(stdout,"scan345: Cannot open message file (%s)\n",msg_file);
		return 0;
	}
	else
		return 1;
}

/******************************************************************
 * Function: mar_start_expo = starts EXPOSURE
 ******************************************************************/
int 	
mar_start_expo() 
{
       time( &now );

	/* Exposure is already active: do not go on */
	if ( esd_stb.task[CMD_COLLECT] & 0x02 ) {
		fprintf( stdout, "scan345: Exposure is still active. Igoring command ...\n");
		return 1;
	}

	/* One exposure is already queued: do not go on */
	if ( esd_stb.task[CMD_COLLECT] & 0x10 ) {
		fprintf( stdout, "scan345: Exposure is already queued. Igoring command ...\n");
		return 1;
	}

	/* Start with an exposure */
	mar_cmd		= MDC_COM_COLL;

	/* Compose IPS command */
	mar_number 	= CMD_COLLECT;
	mar_mode   	= com_mode;
	mar_par1  	= com_phi_steps;
	mar_par2   	= stat_units;
	mar_par3	= cfg.shutter_delay;	
	if ( mar_mode == ARG_DOSE ) mar_par3 = 0;
	mar_par4 	= 0;
	mar_par5 	= 0;
	mar_str[0]	= '\0';
	if ( cfg.use_phi == 0 )
		mar_par2 = 1;

	open_shutter_counter 	= 0;
	close_shutter_counter 	= 0;
	erase_start	    	= -1;
	stat_xray_units 	= 0.0;

	if ( totpass == stat_n_passes ) {
		/* Set exposure time to stat_time*/
		exposure_time = (int)stat_time;
		if ( stat_mode == ARG_TIME )
			fprintf(stdout, "scan345: EXPOSE: %d * PHI = %1.3f -> %1.3f @ %1.0f sec.\n",com_phiosc,stat_phi,stat_phi+com_dphi,com_time);
		else
			fprintf(stdout, "scan345: EXPOSE: %d * PHI = %1.3f -> %1.3f @ %1.0f counts\n",com_phiosc,stat_phi,stat_phi+com_dphi,stat_units);
		if ( stat_n_passes > 1 )
		fprintf(stdout, "scan345:  1. oscillation\n");
	}
	else
		fprintf(stdout, "scan345: %2d. oscillation\n",stat_n_passes - totpass+1);

	stat_scanner_op = CMD_COLLECT;

	/* Send command to scanner */
	if ( stat_units >  0 ) {
		if( 0 == mar_command( )) return( -1 ); 
	}

	return(1);
}

/******************************************************************
 * Function: PrintStatus
 ******************************************************************/
void
PrintStatus() 
{
static int	ctr=0;
char		hw[64], tacti[32], tdone[32], terro[32];

	if ( netcontrol==0 || fpval == NULL ) return;

	rewind( fpval );

	time( &now );
	fprintf( fpval, "TIME           %s", (char *)ctime( &now) );
	fprintf( fpval, "COUNTER        %d (%d)\n", esd_stb.counter,ctr++);
	if ( cfg.use_dist )
	fprintf( fpval, "DISTANCE       %1.3f\n"	,stat_dist);
	if ( cfg.use_phi )
	fprintf( fpval, "PHI            %1.3f\n"	,stat_phi);
	if ( cfg.use_ome )
	fprintf( fpval, "OMEGA          %1.3f\n"	,stat_omega);
	fprintf( fpval, "SCANMODE       %d\n"		,stat_scanmode);
	fprintf( fpval, "PIXELS         %d\n"		,stat_pixels);


	sprintf( hw,    "\0" );
	sprintf( tacti, "\0" );
	sprintf( tdone, "\0" );
	sprintf( terro, "\0" );

	for ( j=1, i=0;i<32; i++ ) {
		if (  esd_stb.hw_status1 & two32[i] )
			strcat( hw,"1" );
		else
			strcat( hw,"0" );

		if ( j == 8 ) {
			strcat( hw, "  " );
			j = 1;
		}
		else
			j++;
	}

	for ( j=1, i=0;i<MAX_CMD; i++ ) {
		if (  esd_stb.task[i] & two32[0] )
			strcat( tdone,"1" );
		else
			strcat( tdone,"0" );
		if (  esd_stb.task[i] & two32[1] )
			strcat( tacti,"1" );
		else
			strcat( tacti,"0" );
		if (  esd_stb.task[i] & two32[8] )
			strcat( terro,"1" );
		else
			strcat( terro,"0" );
		if ( j == 8 ) {
			strcat( tacti, "  " );
			strcat( tdone, "  " );
			strcat( terro, "  " );
			j = 1;
		}
		else
			j++;
	}
	strcat( hw,    "\0" );
	strcat( tacti, "\0" );
	strcat( tdone, "\0" );
	strcat( terro, "\0" );

	fprintf( fpval, "TASK ACTIVE    %s\n"		,tacti);
	fprintf( fpval, "TASK DONE      %s\n"		,tdone);
	fprintf( fpval, "TASK ERROR     %s\n"		,terro);
	fprintf( fpval, "HARDWARE       %s\n"		,hw);
}
