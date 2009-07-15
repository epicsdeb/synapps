/*********************************************************************
 *
 * scan345: command.c
 * 
 * Copyright by Dr. Claudio Klein, X-Ray Research GmbH.
 *
 * Version:     4.0
 * Date:        30/10/2002
 *
 * History:
 *
 * Date		Version		Description
 * ---------------------------------------------------------------------
 * 30/10/02	4.0		com_use_center added
 * 31/10/00	3.0		CBF/imgCIF format implemented
 * 15/06/00	2.2		Added feature COMMAND SCAN ADD x ERASE y
 *
 **********************************************************************/
/*
 * Standard includes for builtins.
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * mar software include files
 */
#include "marcmd.h"
#include "config.h"
#include "marglobals.h"
#include "esd.h"

#define  TOUPPER(a)	for (j=strlen(a), i=0;i<j;i++ ) a[i] = toupper( a[i] )
#define  TOLOWER(a)	for (j=strlen(a), i=0;i<j;i++ ) a[i] = tolower( a[i] )

/*
 * Global variables
 */
extern char scan_in_progress;
/*
 * Local variables
 */
static	float 		dfree,disk;

/*
 * External functions
 */
extern float 		GetDiskSpace	();
extern int 		marTask		(int, float);

/*
 * Local functions
 */
void 			get_command	(void);

static void		mar_init_params	(void);
static int  		TestDirectory	(char *);
static int		BadInput	(int, char *);

/******************************************************************
 * Function: get_command()
 ******************************************************************/
void
get_command(void)
{
static int	frm=1;
FILE 		*fp;
int 		i,j, k, command=0, ntok;
char 		s1[64], s2[64], s3[64], s4[64],s5[64],s6[64],s7[64],s8[32],key[8];
char		coll_phi=0,coll_time=0;
float		val=1.0;
extern int	stat_task_active;
extern int	get_status();


	if ( stat_task_active || scan_in_progress) return;

	/* Open method file */
	if((fp=fopen(com_file, "r" ))==NULL){
		return;
	}

	/* Initialize parameters */
	mar_init_params();

	/* Read command file */
	while( fgets(buf,1024,fp)!= NULL){

		/* Scip comment lines */
		if( buf[0] == '#' || buf[0]=='!')continue;

		/* Convert keyword to uppercase */
		key[0] = '\0';
		for (k=0, j=0, i=0; i<strlen( buf ); i++ ) {
			if ( isspace( buf[i] ) && j==0 ) continue;
			if ( isspace( buf[i] ) && j==1 ) break; 
			j = 1;
			key[k] = toupper( buf[i] );
			k++;
			if (k>3) break;
		}
		key[4]='\0';

		/* Get rid of any new lines */
		for(i=0;i<strlen(buf);i++)if(buf[i]=='\n')buf[i]='\0';

		/* Copy buf into s1 (=keyword) and s2 (=argument) */
		s1[0] = s2[0] = s3[0] = s4[0] = s5[0] = s6[0] = s7[0] = s8[0] = '\0';
		ntok = sscanf( buf , "%s%s%s%s%s%s%s%s", s1, s2, s3, s4, s5, s6, s7, s8);

		/* Keyword: DIRECTORY */
		if(strstr(key,"DIR"))
			strcpy( com_dir, s2 );

		/* Keyword: ROOT */
		else if(strstr(key,"ROOT")) 
			strcpy( com_root, s2 );

		/* Keyword: PHI start  end   osc*/
		else if(strstr(key,"PHI")) {
			if ( BadInput( 2, s2 ) ) 
				com_phibeg = 0.0;
			else
				com_phibeg = atof( s2 );
			if ( BadInput( 2, s3 ) ) 
				com_phiend = 0.0;
			else
				com_phiend = atof( s3 );
			if ( ntok > 3 ) {
				if ( BadInput( 1, s4 ) ) 
					com_phiosc = 1;
				else
					com_phiosc = atoi( s4 );
			}
			while ( com_phibeg <   0.0) com_phibeg += 360.0;
			while ( com_phibeg >=360.0) com_phibeg -= 360.0;
			while ( com_phiend <   0.0) com_phiend += 360.0;
			while ( com_phiend >=360.0) com_phiend -= 360.0;
			if ( com_phiend < com_phibeg )
				com_dphi = com_phiend + 360.0 - com_phibeg;
			else
			    com_dphi = com_phiend - com_phibeg;

			coll_phi = 1;
		}

		/* Keyword: OMEGA start  end   osc*/
		else if(strstr(key,"OMEG")) {
			if ( BadInput( 2, s2 ) ) 
				com_omebeg = 0.0;
			else
				com_omebeg = atof( s2 );
			if ( BadInput( 2, s3 ) ) 
				com_omeend = 0.0;
			else
				com_omeend = atof( s3 );
			if ( ntok > 3 ) {
				if ( BadInput( 1, s4 ) ) 
					com_omeosc = 0;
				else
					com_omeosc = atoi( s4 );
			}
			while ( com_omebeg <   0.0) com_omebeg += 360.0;
			while ( com_omebeg >=360.0) com_omebeg -= 360.0;
			while ( com_omeend <   0.0) com_omeend += 360.0;
			while ( com_omeend >=360.0) com_omeend -= 360.0;

			com_dome = com_omeend - com_omebeg;
		}

		/* Keyword: time/dose mode */
		else if(strstr(key,"COLL")) {
			TOUPPER( s2 );		
			if ( strstr( s2, "DOSE" ) )
				com_mode = ARG_DOSE;
			else
				com_mode = ARG_TIME;
			/* Third argument: time or dose */
			if ( ntok > 2 ) {
				if ( !BadInput( 2, s3 ) ) { 
					com_time = atof( s3 );
					coll_time = 1;
				}
			}
		}

		/* Keyword: (output) format */
		else if(strstr(key,"FORM")) {
			TOUPPER( s2 );
			if ( strstr( s2, "IMAGE" ) )
				com_format = OUT_IMAGE;
			else if ( strstr( s2, "SPIRAL" ) )
				com_format = OUT_SPIRAL;
			else if ( strstr( s2, "CBF" ) )
				com_format = OUT_CBF;
			else if ( strstr( s2, "CIF" ) )
				com_format = OUT_CIF;
			else
				com_format = OUT_MAR;
		}

		/* Keyword: OSCILLATIONS */
		else if(strstr(key,"OSCI") ) {
			if ( BadInput( 1, s2 ) ) 
				com_phiosc = 1;
			else 
				com_phiosc = atoi( s2 );
		}

		/* Keyword: MODE mode roff */
		else if(strstr(key,"MODE") ) {
			if ( BadInput( 1, s2 ) ) 
				com_scanmode = 0;
			else {
				com_scanmode = atoi( s2 );
				if ( com_scanmode == 2300 ) 
					com_scanmode = 0;
				else if ( com_scanmode == 2000 ) 
					com_scanmode = 1;
				else if ( com_scanmode == 1600 ) 
					com_scanmode = 2;
				else if ( com_scanmode == 1200 ) 
					com_scanmode = 3;
				else if ( com_scanmode == 3450 ) 
					com_scanmode = 4;
				else if ( com_scanmode == 3000 ) 
					com_scanmode = 5;
				else if ( com_scanmode == 2400 ) 
					com_scanmode = 6;
				else if ( com_scanmode == 1800 ) 
					com_scanmode = 7;
				if ( com_scanmode < 0 || com_scanmode > 7 )
					com_scanmode = 0;
			}
		
			com_diam     = cfg.diameter [com_scanmode];
			com_pixelsize= cfg.pixelsize[com_scanmode];
			com_size     = cfg.size     [com_scanmode];
		}

		/* Keyword: COMMAND */
		else if(strstr(key,"COMM") && command != MDC_COM_IPS ) {
			TOUPPER( s2 );
			command = MDC_COM_IDLE;
			if ( strstr( s2, "SCAN" ) )  {
				command = MDC_COM_SCAN;
				/* Are there additional params for SCAN, e.g.
				 * COMMAND  SCAN  ADD x  ERASE y ??? */
				if ( ntok > 3 ) {
					TOUPPER( s3 );
					if ( strstr( s3, "ERAS" ) || strstr( s3, "CLEAN" ) ) {
						if ( !BadInput( 1, s4 ) ) { 
							com_scan_erase = atoi( s4 );
						}
					}
					else if ( strstr( s3, "ADD" ) ) {
						if ( !BadInput( 1, s4 ) ) { 
							com_scan_add   = atoi( s4 );
						}
					}
				}
				if ( ntok > 5 ) {
					TOUPPER( s5 );
					if ( strstr( s5, "ADD" ) ) {
						if ( !BadInput( 1, s6 ) ) { 
							com_scan_add   = atoi( s6 );
						}
					}
					else if ( strstr( s5, "ERAS" ) || strstr( s5, "CLEAN" ) ) {
						if ( !BadInput( 1, s6 ) ) { 
							com_scan_erase = atoi( s6 );
						}
					}
				}
			}
			else if ( strstr( s2, "ERAS" ) ) 
				command = MDC_COM_ERASE;
			else if ( strstr( s2, "STAR" ) ) 
				command = MDC_COM_STARTUP;
			else if ( strstr( s2, "STOP" ) || strstr( s2, "ABOR" )) 
				command = MDC_COM_ABORT;
			else if ( strstr( s2, "CHAN" ) ) 
				command = MDC_COM_MODE;
			else if ( strstr( s2, "QUIT" ) ) 
				command = MDC_COM_QUIT;
			else if ( strstr( s2, "TEST" ) ) 
				command = -1;
			else if ( strstr( s2, "SHUT" ) ) {
				TOUPPER( s3 );
				if ( ntok < 3 ) {
					fprintf( stdout, "scan345: COMMAND SHUTTER must be followed by 'OPEN' or 'CLOSE' ...\n");
					continue;
				}
				if ( strstr( s3, "OP" ) )
					val = 1.0;
				if ( strstr( s3, "CL" ) )
					val = 0.0;
				command = MDC_COM_SHUT;
			}
			else if ( strstr( s2, "INIT" ) ) {
				TOUPPER( s3 );
				/* Default: initialize at far end ... */
				val = cfg.dist_max;
				if ( strstr( s3, "MIN" ) )
					val = cfg.dist_min;
				command = MDC_COM_INIT;
			}
			else if ( strstr( s2, "EXPO" ) ) {

				/* Optional further arguments: COMMAND EXPO [dphi] [units] [oscillations] */
				if ( ntok > 2 ) {
					if ( !BadInput( 2, s3 ) ) { 
						com_dphi = atof( s3 );
						com_phibeg	= stat_phi;
						com_phiend	= com_phibeg + com_dphi;
						coll_phi = 1;
					}
				}
				if ( ntok > 3 ) {
					if ( !BadInput( 2, s4 ) ) { 
						com_time = atof( s4 );
						coll_time = 1;
					}
				}
				if ( ntok > 4 ) {
					if ( !BadInput( 1, s5 ) ) { 
						com_phiosc = atoi( s5 );
					}
				}
				command = MDC_COM_COLL;
			}
			else if ( strstr( s2, "PHI" ) ) {
				if ( ntok < 4 ) {
					fprintf( stdout, "scan345: COMMAND PHI must be followed by 'DEFINE' or 'MOVE' phi ...\n");
					continue;
				}
				TOUPPER( s3 );
				if ( strstr( s3, "DEF" ) )
					command = MDC_COM_PSET;
				else if ( strstr( s3, "MOV" ) )
					command = MDC_COM_PHI;
				else {
					fprintf( stdout, "scan345: COMMAND PHI must be followed by 'DEFINE' or 'MOVE' phi ...\n");
					continue;
				}
				if ( BadInput( 2, s4 ) ) 
					command = MDC_COM_IDLE;
				else
					val = (double)atof( s4 );
			}
			else if ( strstr( s2, "DIST" ) ) {
				if ( ntok < 4 ) {
					fprintf(stdout, "scan345: COMMAND DISTANCE must be followed by 'DEFINE' or 'MOVE' distance ...\n");
					continue;
				}
				TOUPPER( s3 );
				if ( strstr( s3, "DEF" ) )
					command = MDC_COM_DSET;
				else if ( strstr( s3, "MOV" ) )
					command = MDC_COM_DISTANCE;
				else {
					fprintf( stdout, "scan345: COMMAND DISTANCE must be followed by 'DEFINE' or 'MOVE' distance ...\n");
					continue;
				}
				if ( BadInput( 2, s4 ) ) 
					command = MDC_COM_IDLE;
				else
					val = (double)atof( s4 );
			}
			else if ( strstr( s2, "OMEG" ) ) {
				if ( ntok < 4 ) {
					fprintf(stdout, "scan345: COMMAND OMEGA must be followed by 'DEFINE' or 'MOVE' omega ...\n");
					continue;
				}
				TOUPPER( s3 );
				if ( strstr( s3, "DEF" ) )
					command = MDC_COM_OSET;
				else if ( strstr( s3, "MOV" ) )
					command = MDC_COM_OMOVE;
				else {
					fprintf( stdout, "scan345: COMMAND OMEGA must be followed by 'DEFINE' or 'MOVE' omega ...\n");
					continue;
				}
				if ( BadInput( 2, s4 ) ) 
					command = MDC_COM_IDLE;
				else
					val = (double)atof( s4 );
			}
		}

		/* Keyword: IPS command*/
		else if(strstr(key,"IPS")) {
			strcpy( ips_command, buf );
			if ( ntok > 1 ) mar_number = atoi( s2 );
			if ( ntok > 2 ) mar_mode   = atoi( s3 );
			if ( ntok > 3 ) mar_par1   = atoi( s4 );
			if ( ntok > 4 ) mar_par2   = atoi( s5 );
			if ( ntok > 5 ) mar_par3   = atoi( s6 );
			if ( ntok > 6 ) mar_par4   = atoi( s7 );
			if ( ntok > 7 ) mar_par5   = atoi( s8 );
			if ( ntok > 8 ) strcpy( mar_str, s8 );

			if ( mar_number == CMD_SCAN ) {
				sprintf( com_root, "scan_%03d", frm++);
				if ( frm > 999 ) frm = 1;
			}
			if ( mar_number < 0 || mar_number > 15 ) {
				fprintf(stdout, "scan345: Invalid IPS number: %d\n",mar_number);
				fprintf(stdout, "         IPS command range is 0 to 15.  ...\n");
			}
			else
					command = MDC_COM_IPS;
		}

		/* Keyword: SYSTEM command*/
		else if(strstr(key,"SYS")) {
			strcpy( ips_command, buf + strlen( s1 ) + 1);
			ips_command[ strlen(ips_command) ] = '\r';
			strcat( ips_command, "\0" );
			if ( strlen( ips_command ) > 28 ) {
				fprintf( stdout, "scan345: SYSTEM command must have <= 28 chars. Ignored ...\n");
			}
			else {
				command = MDC_COM_SHELL;
			}
		}

		/* Keyword: DISTANCE */
		else if(strstr(key,"DIST")) {
			if ( BadInput( 2, s2 ) ) 
				com_dist = 100.0;
			else
				com_dist = atof( s2 );
		}

		/* Keyword: USE SPIRAL | CENTER */
		else if(strstr(key,"USE")) {
			if(strstr(s2,"SPI"))
				com_use_spiral = 1;
			if(strstr(s2,"CEN"))
				com_use_center = 1;
		}

		/* Keyword: IGN SPIRAL | CENTER */
		else if(strstr(key,"IGN")) {
			if(strstr(s2,"SPI"))
				com_use_spiral = 0;
			if(strstr(s2,"CEN"))
				com_use_center = 0;
		}

		/* Keyword: CHI */
		else if(strstr(key,"CHI")) {
			if ( BadInput( 2, s2 ) ) 
				com_chi = 1.0;
			else
				com_chi = atof( s2 );
		}

		/* Keyword: THETA */
		else if(strstr(key,"THET")) {
			if ( BadInput( 2, s2 ) ) 
				com_theta = 1.0;
			else
				com_theta = atof( s2 );
		}

		/* Keyword: WAVE */
		else if( strstr(key,"WAVE")) {
			if ( BadInput( 2, s2 ) ) 
				com_wavelength = 1.541789;
			else
				com_wavelength = atof( s2 );
		}
		
		/* Keyword: TIME */
		else if(strstr(key,"TIME") || strstr(key, "DOSE") ) {
			if ( BadInput( 2, s2 ) ) 
				com_time = 60.0;
			else
				com_time = atof( s2 );
		}
  
		/* Keyword: SOURCE */
		else if(strstr(key,"SOUR")) {
			for ( i=6; i<strlen(buf); i++ ) if ( buf[i] != ' ' )break;
			strcpy( com_source, buf+i );
		}
  
		/* Keyword: FILTER */
		else if(strstr(key,"FILT")) {
			for ( i=6; i<strlen(buf); i++ ) if ( buf[i] != ' ' )break;
			strcpy( com_filter, buf+i );
		}
  
		/* Keyword: REMARK */
		else if(strstr(key,"REMA")) {
			for ( i=6; i<strlen(buf); i++ ) if ( buf[i] != ' ' )break;
			strcpy( com_remark, buf+i);
		}

		/* Keyword: BEAM */
		else if(strstr(key,"BEAM")) {
			if ( !BadInput( 2, s2 ) ) 
				com_slitx = atof( s2 );
			if ( !BadInput( 2, s3 ) ) 
				com_slity = atof( s3 );
		}

		/* Keyword: POLA */
		else if(strstr(key,"POLA")) {
			if ( !BadInput( 2, s2 ) ) 
				com_polar = atof( s2 );
		}

		/* Keyword: POWER */
		else if(strstr(key,"POWE")) {
			if ( !BadInput( 2, s2 ) ) 
				com_kV = atof( s2 );
			if ( !BadInput( 2, s3 ) ) 
				com_mA = atof( s3 );
		}
	}

	fclose(fp);
	remove( com_file );

	/* Tests... */
	if ( command == MDC_COM_SCAN ) {
		/* UNIX: append '/' at end of directory */
		strcpy( str, com_dir );
		if ( str[0] == '\0' || str[ strlen(str) - 1 ] == '/' )
			sprintf(com_dir,"%s", str);
		else
			sprintf(com_dir,"%s/", str);

		if ( TestDirectory( com_dir ) < 1 ) {
			sprintf(str, "scan345: No access to directory %s\n",com_dir);
			fprintf(stdout, str); fprintf( fpout, str);
			goto DONE;
		}
		dfree   = GetDiskSpace( com_dir );
		disk	= (com_size*com_size + 2*com_size)/1000000.;
		if ( com_format == OUT_MAR || com_format == OUT_CIF || com_format == OUT_CBF )
			disk *= 0.4;
		if ( com_use_spiral )
			disk = disk + 1.6*disk; 

		if ( disk > dfree ) {
			sprintf( str, "scan345: Not enough disk space in %s!\n", com_dir );
			fprintf(stdout, str); fprintf( fpout, str);
			goto DONE;
		}
	}

	/* Here we deal with a native IPS command (for ESD controllers only) */
	else if ( command == MDC_COM_IPS ) {
		j = sscanf( ips_command, "%s%s", s1,s2);
		if ( j != 2 || BadInput( 1, s2) ) {
			command = 0;
			sprintf(str, "scan345: Wrong argument 1 (%s) on IPS command\n",s2);
			fprintf(stdout, str); fprintf( fpout, str);
			command = 0;
		}
	}

	/* Here we deal with an EXPOSURE command: keywords PHI, COLLECT and
	 * TIME must have been given
	 */
	else if ( command == MDC_COM_COLL ) {
		if ( coll_phi  == 0 ) {
			fprintf( stdout, "scan345: With COMMAND EXPOSURE, also keyword PHI phi_start phi_end [oscillations] must be given ...\n");
			command = MDC_COM_IDLE;
			return;
		}
		if ( coll_time  == 0 ) {
			fprintf( stdout, "scan345: With COMMAND EXPOSURE, also keyword TIME/DOSE exposure_time/xray_dose must be given ...\n");
			command = MDC_COM_IDLE;
			return;
		}
	}

	if ( command >= 0 )
		marTask( command , val );
DONE:
	return;
}

/******************************************************************
 * Function: BadInput
 ******************************************************************/
static int
BadInput(int type, char *str1)
{
int   		i,k;
extern void	RemoveBlanks();

   RemoveBlanks( str1 );

   k=strlen( str1 );
   if ( strlen( str1 ) < 1 ) return 1;

   for ( i=k-1; i>=0; i-- ) {
	/* Integer (32-bit: signed int) */
	if ( type == 1 && isdigit( str1[i] ) ) 
		continue;

	/* Float>=0.0   (32-bit: float)      */
	else if ( type == 2 && ( isdigit( str1[i] ) || str1[i] == '.' ) )
		continue;

	/* Float   (32-bit: float)      */
	else if ( type == 3 && ( isdigit( str1[i] ) || str1[i] == '.' || str1[i] == '-' ) )
		continue;

    	return( 1 );

   }

   /* Whole string is okay ... */
   return( 0 );
}

/******************************************************************
 * Function: TestDirectory = checks if directory is accessible
 ******************************************************************/
static int
TestDirectory(char *dir)
{
FILE 		*fp;
extern void	RemoveBlanks();

#ifdef VMS
		return 1;
#endif
	/* If directory starts with . or .. we expand it to working_dir */ 
	if ( ( strlen( dir ) == 1 && dir[0] != '/' ) || 
	     ( strlen( dir ) == 2 && dir[0] == '.' && dir[1] == '/' ) ) {
			strcpy( dir, working_dir );
	}
	
	if ( dir[ strlen(dir) - 1] == '/' )
			dir[ strlen(dir) - 1] = '\0';
	sprintf(buf,"%s/test.dir",dir); 

	if((fp=fopen(buf,"w+"))==NULL){
		return 0;
	}
	else {
		fclose(fp);

		remove(buf);

		return 1;
	}
}
/********************************************************************
 * Function: mar_init_params = initializes scanner global variables
 ********************************************************************/
static void
mar_init_params()
{
	/* Initialize parameters from config file */

	cur_scantime 		= cfg.scantime	  [stat_scanmode];
	cur_diameter  		= cfg.diameter 	  [stat_scanmode];
	cur_pixelsize 		= cfg.pixelsize	  [stat_scanmode];

	com_size		= cfg.size	  [stat_scanmode];
	com_pixelsize		= cur_pixelsize;
	com_mode 		= ARG_TIME;
	com_scanmode		= stat_scanmode;
	com_scan_erase		= 0;
	com_scan_add		= 0;

	com_wavelength		= cfg.wavelength;
	com_dist		= 100.;
	com_time		= 60.;
	com_phiosc		= 1;
	com_omeosc		= 0;
	com_phibeg		= cfg.phi_def;
	com_phiend		= cfg.phi_def;
	com_dphi		= 1.0;
	com_dome		= 0.0;
	com_omebeg		= cfg.ome_def;
	com_omeend		= cfg.ome_def;
	com_chi			= cfg.chi_def;
	com_theta		= cfg.thet_def;
	com_dosebeg		= 0.0;
	com_doseend		= 0.0;
	com_doseavg		= 0.0;
	com_dosesig		= 0.0;
	com_dosemin		= 0.0;
	com_dosemax		= 0.0;
	com_dosen  		= 0;
	com_use_spiral		= 0;
	com_format		= OUT_MAR;
	com_polar		= cfg.polar;
	com_slitx		= cfg.slitx;
	com_slity		= cfg.slity;
	com_kV   		= cfg.kV;
	com_mA   		= cfg.mA;
	com_use_center		= 0;
	if ( cfg.use_center ) com_use_center = 1;
	
	strcpy( com_filter, cfg.filter );
	strcpy( com_source, cfg.source );
	strcpy( com_remark, "\0"    );
	strcpy( com_root, "xtal"    );
	strcpy( com_dir,  working_dir );
}
