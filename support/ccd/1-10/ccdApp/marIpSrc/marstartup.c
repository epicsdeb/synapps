/*********************************************************************
 *
 * scan345: marstartup.c
 * 
 * Copyright by Dr. Claudio Klein, X-Ray Research GmbH.
 *
 * Version:     3.1
 * Date:        14/05/2002

 * Version	Date		Mods
 * 3.1		14/05/2002	Make compatible to VMS
 *
 *********************************************************************/
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>

#include "esd.h"
#include "marglobals.h"
#include "version.h"
#define CONFIGGLOBAL
#include "config.h"

/*
 * Machine specific calls: __sgi
 */
#ifdef __sgi 
#include <sys/stat.h>

#ifndef DEGRADING_PRIORITIES
#include <limits.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>
#endif

#endif

/*
 * Machine specific calls: __osf__ (DEC UNIX)
 */
#ifdef __osf__
#include <sys/types.h>
#include <sys/stat.h>
#endif

/*
 * Machine specific calls: LINUX
 */
#ifdef __linux__
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>
#endif

/*
 * Global variables
 */
char		martable_dir	[128];
char		nbcode_file	[128];
char		marlog_dir	[128];
char		marstatus_file	[128];

/*
 * External variables
 */
extern int 	lines;
extern int	status_interval;

/*
 * Declaration of external functions
 */
extern int  	net_open();

/*
 * Declaration of local functions
 */
int 		marStartup();
int   		trnlog();
extern CONFIG   GetConfig();
extern void     PutConfig(CONFIG);

/******************************************************************
 * Function: marStartup
 ******************************************************************/
int 
marStartup()
{
int    		i=0;
int    		fdtest;
time_t		now;
int		log_no=0;
extern int 	trnlog();
extern int	input_priority;
extern float	input_scale;
FILE		*fp;

#ifdef __linux__
	if ( input_priority == -1 ) 
		input_priority = -20;
	/* Set priority to ... */
	i=setpriority(PRIO_PROCESS, (int)getpid(), input_priority);
	if ( i == -1 )
		input_priority = -1;
#elif __sgi
	if ( input_priority == -1 ) 
		input_priority = NDPHIMAX;
	
	while ( (i = schedctl( NDPRI, 0, input_priority ) ) == -1 ) {
		input_priority++;
		if ( input_priority > NDPNORMMIN ) break;
	}
	if ( i == -1 )
		input_priority = -1;
#endif

#ifndef __osf__
	if ( i < 0 ) {
		fprintf(stdout,"scan345: Cannot increase PRIORITY of process %d\n",getpid());
	}
#endif

    	/*
     	 * Get environment variables MARTABLEDIR, MARLOGDIR and MAR_SCANNER_NO
     	 */

	if(0 == trnlog(trntable,"MARTABLEDIR",str)) {
	    fprintf(stdout,
	      "scan345: Please set the logical name or environment variable MARTABLEDIR.\n");
	    fprintf(stdout,
	      "        Then re-execute scan345.\n");
	    exit(-1);
	}
	strcpy( martable_dir, str );

	if(0 == trnlog(trntable,"MAR_SCANNER_NO",scanner_no)) {
	    fprintf(stdout,
	      "scan345: Please set the logical name or environment variable MAR_SCANNER_NO.\n");
	    fprintf(stdout,
	      "        Then re-execute scan345.\n");
	    exit(-1);
	}

	if(0 == trnlog(trntable,"MARHELPDIR",marhelp_dir)) {
		cfg.use_sound = 0;
	}

	if(0 == trnlog(trntable,"MARLOGDIR",marlog_dir)) {
		fprintf(stdout, "scan345: Environment variable MARLOGDIR not set!\n"); 
		fprintf(stdout, "        The current directory will be assumed... \n");
		strcpy( marlog_dir, "./\0" );
	}

#if ( __osf__ )
	strcpy( working_dir , (char *)getcwd( NULL, 256, 1 ) ) ;
#else
	strcpy( working_dir , (char *)getcwd( NULL, 256 ) ) ;
#endif

#ifdef VMS
	sprintf(  config_file,"%sconfig.%s\0",  martable_dir, scanner_no );
	sprintf(  output_file,"%slast.log\0",   marlog_dir );
	sprintf(     msg_file,"%smar.message\0",marlog_dir );
#else
	sprintf(  config_file,"%s/config.%s\0",  martable_dir, scanner_no );
	sprintf(  output_file,"%s/last.log\0",   marlog_dir );
	sprintf(     msg_file,"%s/mar.message\0",marlog_dir );
#endif

	/* Open last.log file to get last log file no. ... */	
	if(NULL == (fpout = fopen( output_file, "r" ))) {
		log_no = 0;
	}
	else {
		if ( fgets(buf, 80,fpout)== NULL){
			log_no = 0;
		}
		else {
			log_no = atoi( buf );
		}
		fclose( fpout );
		remove( output_file );
	}

	/* Increase no. but keep within 1 and 99 */
	log_no++;
	while( log_no > 99 ) log_no -= 99;

	/* Write this no. right away into last.log */
	if(NULL != (fpout = fopen( output_file, "w+"))) {
		fprintf( fpout, "%d\n", log_no );
		fclose( fpout );
	}

	/* Set file name of currently used log file*/
#ifdef VMS
	sprintf(  output_file, "%smar.log\0", marlog_dir);
#else
	sprintf(  output_file, "%s/log/mar.log.%d\0", marlog_dir, log_no );
#endif

	if(NULL == (fpout = fopen( output_file, "w+"))) {
	    	fprintf(stdout,"scan345: Cannot open %s as log file\n",output_file);
	    	exit( -1 );
	}

	/* On UNIX, link actual mar.log_X to mar.log */
	else { 
#ifdef VMS
		sprintf( str, "purge/keep=99 %smar.log\0", marlog_dir);
#else
		sprintf( str, "%s/mar.log\0", marlog_dir);
		remove( str );

		sprintf( str, "ln -s %s/log/mar.log.%d  %s/mar.log\0", marlog_dir, log_no,marlog_dir);
#endif
		system( str );
	}

	/* Open neighbour code file... */	
#ifdef VMS
	sprintf(  nbcode_file,"%smar2300.%s\0", martable_dir, scanner_no);
#else
	sprintf(  nbcode_file,"%s/mar2300.%s\0", martable_dir, scanner_no);
#endif
	if( -1 == (fdtest =  open( nbcode_file, 0 ))) {
	    	fprintf(stdout,"scan345: Cannot open %s as neighbour code file\n", nbcode_file);
	    	exit( -1 );
	}
	close( fdtest );

#ifdef VMS
	sprintf(  nbcode_file,"%smar3450.%s\0", martable_dir, scanner_no);
#else
	sprintf(  nbcode_file,"%s/mar3450.%s\0", martable_dir, scanner_no);
#endif
	if( -1 == (fdtest=  open( nbcode_file, 0))) {
	    	fprintf(stdout,"scan345: Cannot open %s as neighbour code file\n", nbcode_file);
	    	exit( -1 );
	}

#ifdef VMS
	sprintf( com_file, "%smar.com", marlog_dir );
#else
	sprintf( com_file, "%s/mar.com", marlog_dir );
#endif

	/***************************************************************** 
	 * Get configuration parameters
     	 *****************************************************************/

        /* Open configuration file ... */
        if(NULL == (fp = fopen( config_file, "r"))) {
                fprintf(stdout, "scan345: Cannot open file %s\n",config_file);
        }                                                     
        else {                                                
                cfg = GetConfig( fp );
		fclose( fp );
        }                                       

	/***************************************************************** 
	 * Open network connection
     	 *****************************************************************/

        if ( strlen( mar_host ) < 1 )
        	strcpy( mar_host, cfg.host );
	else
        	strcpy( cfg.host, mar_host );
		
        if ( !mar_port )
        	mar_port = cfg.port;
	else
		cfg.port = mar_port;

	if ( input_scale > -1.0 )
		cfg.spiral_scale = input_scale;

        PutConfig( cfg );                        

	for ( i=0; i<4; i++ ) {
		if ( cfg.use_msg == 0 && i == 3 ) break;
		netcontrol += net_open(i);
	}
	if ( cfg.use_msg == 0 ) i--;

    	if(netcontrol < 1 ) {
		fprintf(stdout,"scan345: Cannot connect to '%s' at port %d\n",mar_host,mar_port);
		fprintf(stdout,"scan345: No SCANNER control \n");
    	}

	/* Display messages in marstart */
	if ( !netcontrol ) {
		exit(0); 
	}

	/* Look for MARSTATUSFILE if requested */
	if ( status_interval ) {
	    if(0 == trnlog(trntable,"MARSTATUSFILE",marstatus_file)) {
		fprintf(stdout, "scan345: Environment variable MARSTATUSFILE not set!\n"); 
		sprintf( marstatus_file, "%s/mar345.status", marlog_dir );
		fprintf(stdout, "        %s will be used ... \n",marstatus_file);
	    }
	    if ( (fpval=fopen( marstatus_file, "w+" ) ) == NULL ) {
		fprintf(stdout, "scan345: ERROR: no write permission for %s!\n",marstatus_file); 
		fprintf(stdout, "        No STATUS will be reported!\n"); 
		status_interval = 0;
	    }
	}
	else
		fpval = NULL;

	/* Open mar.lp file */
	fpstat = NULL;
	if ( cfg.use_stats ) {
		if(0 == trnlog(trntable,"MARSTATSDIR",buf)) {
			strcpy( buf, marlog_dir );
		}
#ifdef VMS
		sprintf( str, "%smar.lp",  buf);
#else
		sprintf( str, "%s/lp/mar.lp.%d",  buf, log_no );
#endif
		if(NULL == (fpstat= fopen( str, "w+"))) {
			fprintf(stdout,"scan345: Cannot open %s as STATS file\n",str);
			cfg.use_stats = 0;
		}
		else {
#ifdef VMS
			sprintf( str, "purge/keep=99 %smar.lp", buf);
#else
			setlinebuf( fpstat );

			sprintf( str, "%s/mar.lp", buf);
			remove( str );

			sprintf( str, "ln -s %s/lp/mar.lp.%d  %s/mar.lp", buf, log_no,buf);
#endif
			system( str );
		}
	}

	/* Open mar.spy file */
	fpspy  = NULL;
	if ( cfg.use_msg ) {
		if(0 == trnlog(trntable,"MARSPYDIR",buf)) {
			strcpy( buf, marlog_dir );
		}
#ifdef VMS
		sprintf( str, "%smar.spy",  buf);
#else
		sprintf( str, "%s/spy/mar.spy.%d",  buf, log_no );
#endif
		if(NULL == (fpspy= fopen( str, "w+"))) {
			fprintf(stdout,"scan345: Cannot open %s as SPY file\n",str);
			cfg.use_msg = 0;
		}
		else {
#ifdef VMS
			sprintf( str, "purge/keep=99 %smar.spy", buf);
#else
			setlinebuf( fpspy );
			sprintf( str, "%s/mar.spy", buf);
			remove( str );

			sprintf( str, "ln -s %s/spy/mar.spy.%d  %s/mar.spy", buf, log_no,buf);
#endif
			system( str );
		}
	}

	/***************************************************************** 
	 * Initialize variables 
     	 *****************************************************************/

	cur_mode = 0;

	/* Log the time to the errorlog for startup */
	time(&now);
	
	sprintf( str ,"\n=============================================================\n");
	fprintf(stdout, str );
	fprintf( fpout, str );
	sprintf( str ,"            Program     :  scan345\n");
	fprintf(stdout, str );
	fprintf( fpout, str );
	sprintf( str ,"            Version     :  %s  (%s)\n",MAR_VERSION,__DATE__);
	fprintf(stdout, str );
	fprintf( fpout, str );
	sprintf( str ,"            Scanner no. :  %s\n",scanner_no);
	fprintf(stdout, str );
	fprintf( fpout, str );
	sprintf( str ,"            Started on  :  %s",(char *) ctime(&now));
	fprintf(stdout, str );
	fprintf( fpout, str );
	sprintf( str ,"            LOG file is :  %s\n",output_file);
	fprintf(stdout, str );
	fprintf( fpout, str );
	if (fpstat!=NULL) fprintf(fpstat, str );
	if ( cfg.use_msg ) {
		if(0 == trnlog(trntable,"MARSPYDIR",buf)) {
			strcpy( buf, marlog_dir );
		}
#ifdef VMS
		sprintf( str ,"            SPY file is :  %smar.spy\n",buf);
#else
		sprintf( str ,"            SPY file is :  %s/spy/mar.spy.%d\n",buf,log_no);
#endif
		fprintf(stdout, str );fprintf( fpout, str );
	}
	if ( cfg.use_stats ) {
		if(0 == trnlog(trntable,"MARSTATSDIR",buf)) {
			strcpy( buf, marlog_dir );
		}
#ifdef VMS
		sprintf( str ,"            STAT file is:  %smar.lp\n",buf);
#else
		sprintf( str ,"            STAT file is:  %s/lp/mar.lp.%d\n",buf,log_no);
#endif
		fprintf(stdout, str );fprintf( fpout, str );
		if (fpstat!=NULL) fprintf(fpstat, str );
	}
	sprintf( str ,"=============================================================\n\n");
	fprintf(stdout, str );
	fprintf( fpout, str );

	fprintf(stdout, "scan345: Running tasks can only be aborted with ^\\ (SIGQUIT)\n\n");

	/* Log to STATS file */	
	if ( fpstat != NULL ) {
		fprintf(fpstat, str );

		fprintf( fpstat, "        TOTAL             NORDWEST     W/O     N/S      TIME      DIFF  GAP   ADC1 ADC2  IMAGE\n");
		fprintf( fpstat, "  Max     Avg. Sigma     Avg. Sigma                                                      NAME\n");
		fprintf( fpstat, "_________________________________________________________________________________________________\n\n");
		fflush ( fpstat );
	}

	if ( input_priority != -1 ) {
		sprintf(str, "scan345: NON-DEGRADING PRIORITY set to %d\n", input_priority);
		fprintf( fpout, str );
	}

	fflush( fpout );

    	return( netcontrol );
}

/********************************************************************
 * Function: trnlog = translates logical names
 ********************************************************************/
int 
trnlog(char *table, char *logical_name, char *name)
{
	char	*tr;
	
	if(NULL == (tr = (char *) getenv(logical_name))) {
		name[0] = '\0';
		return 0;
	}
	else {
		strcpy(name,tr);
		return 1;
	}
}

