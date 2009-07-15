/*********************************************************************
 *
 * scan345: marconfig.c
 * 
 * Author:  Claudio Klein, X-Ray Research GmbH.
 *
 * Version: 4.0
 * Date:    30/10/2002
 *
 * Version	Date		Mods
 * 4.0		30/10/2002	use_center + center_imin added
 *
 *********************************************************************/
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/*
 * mar software include files
 */
#include "config.h"

/*
 * Definitions
 */
#define STRING		0
#define INTEGER		1
#define FLOAT		2

/*
 * External variables
 */
extern int	InputType();
extern void	WrongType();	
extern void	RemoveBlanks();	

extern char	str[1024];
extern int      status_interval;

/*
 * Local functions
 */
CONFIG		GetConfig(FILE *);
CONFIG		SetConfig(void);
void		PutConfig(CONFIG);

/******************************************************************
 * Function: GetConfig
 ******************************************************************/
CONFIG
GetConfig(FILE *fp)
{
CONFIG		h;
int 		i,j,k, size, ntok=0, nmode=0;
char 		buf[128], *key, *token[20];
extern int	input_priority;
extern float	stat_wavelength;

	/* Set defaults */
	h = SetConfig();

	fseek( fp, 0, SEEK_SET );

	/*
	 * Read input lines
	 */

	while( fgets(buf,128,fp)!= NULL){

		if ( strlen(buf) < 2 ) continue;

		/* Scip comment lines */
		if( buf[0] == '#' || buf[0]=='!' ) continue;

		/* Tokenize input string */
		/* ntok  = number of items on input line - 1 (key) */
		ntok = -1;	

		for(i=0;i<128;i++) {
			/* Convert TAB to SPACE */
			if( buf[i] == '\t') buf[i] = ' ';
			if( buf[i] == '\f') buf[i] = ' '; 
			if( buf[i] == '\n') buf[i] = '\0'; 
			if( buf[i] == '\r') buf[i] = '\0'; 
			if( buf[i] == '\0') break;
		}

		for(i=0;i<strlen(buf);i++) {
			if( buf[i] == ' ' ) continue;
			ntok++; 
			for (j=i;j<strlen(buf);j++) 
				if( buf[j] == ' ') break;
			i=j;
		}
		if (strlen(buf) < 3 ) continue; 

		key = strtok( buf, " ");

		/* Convert keyword to uppercase */
		for ( i=0; i<strlen(key); i++ )
			if ( isalnum( key[i] ) ) key[i] = toupper( key[i] );

		for(i=0;i<ntok;i++) {
			token[i] = strtok( NULL, " ");
			strcpy( str, token[i] );

			if ( !strstr( key, "NET" ) )
			for ( j=0; j<strlen( str ); j++ )
				if ( isalnum( str[j] ) ) str[j] = toupper( str[j] );
			strcpy( token[i] , str );
			RemoveBlanks( token[i] );
		}

		/* Keyword: GAIN */
		if(!strncmp(key,"GAIN",4) && ntok >= 0 )
			if ( InputType( token[0] ) >= INTEGER ) 
				h.gain = atof( token[0] );
			else
				WrongType( FLOAT, key, token[0] );

                else if( !strncmp(key,"MONO",4) && ntok >=0 ) {
                    for ( i=0; i<ntok; i++ ) {
                        if ( strstr(token[i],"POLA" ) ) {
			    i++;
                            if ( InputType( token[i] ) >= INTEGER )
                                h.polar = atof( token[i] );
			    else
				WrongType( FLOAT, key, token[i] );
                        }
			else {
				strcat( h.filter, token[i] );
			}
                    }
                }

		/* Keyword: SHUTTER_DELAY */
		else if(!strncmp(key,"SHUT",4) && ntok >= 0 )
			if ( InputType( token[0] ) == INTEGER ) 
				h.shutter_delay = atoi( token[0] );
			else
				WrongType( INTEGER, key, token[0] );

		/* Keyword: SETS */
		else if(!strncmp(key,"SETS",4) && ntok >= 0 )
			if ( InputType( token[0] ) == INTEGER ) 
				h.sets = (unsigned char)atoi( token[0] );
			else
				WrongType( INTEGER, key, token[0] );


		/* Keyword: GENERATOR */
		else if( !strncmp(key,"GENE",4) && ntok >= 0 ) {
		    for(i=0;i<ntok;i++) {
		    	if ( strstr( token[i], "MA" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.mA = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			}
		    	else if ( strstr( token[i], "KV" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.kV = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			}
			else
				strcat( h.source, token[i] );
		    }
		}

		/* Keyword: COLORS */
		else if(!strncmp(key,"COLO",4) && ntok >= 0 )
			if ( InputType( token[0] ) == INTEGER ) 
				h.colors = (unsigned char)atoi( token[0] );
			else
				WrongType( INTEGER, key, token[0] );

		/* Keyword: PRIORITY */
		else if(!strncmp(key,"PRIO",4) && ntok >= 0 && input_priority < 0 )
			if ( InputType( token[0] ) == INTEGER ) 
				input_priority = atoi( token[0] );
			else
				WrongType( INTEGER, key, token[0] );

		/* Keyword: FLAGS */
		else if(!strncmp(key,"FLAG",4) && ntok >= 0 )
			if ( InputType( token[0] ) == INTEGER ) 
				h.flags = atoi( token[0] );
			else
				WrongType( INTEGER, key, token[0] );

		/* Keyword: SPIRAL */
		else if(!strncmp(key,"SPIR",4) && ntok >= 0 )
			for(i=0;i<ntok;i++) {
			    if ( strstr( token[i], "MAX" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.spiral_max = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			    else if ( strstr( token[i], "SCA" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.spiral_scale = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			}

		/* Keyword: INTENSITY */
		else if(!strncmp(key,"INTE",4) && ntok >= 0 ) {
			for(i=0;i<ntok;i++) {
			    if ( strstr( token[i], "MIN" ) ) {
				i++;
				if ( InputType( token[i] ) >= INTEGER ) 
					h.intensmin = atof( token[i] );
				else
					WrongType( FLOAT, key, token[i] );
			    }
			    else if ( strstr( token[i], "WAR" ) ) {
				i++;
				if ( InputType( token[i] ) >= INTEGER ) 
					h.intenswarn = atof( token[i] );
				else
					WrongType( FLOAT, key, token[i] );
			    }
                            else if ( strstr( token[i], "DOS" ) ) {
                                i++;
                                if ( InputType( token[i] ) >= INTEGER )
                                        h.dosemin = (float)atof( token[i] );
                                else
                                        WrongType( FLOAT, key, token[i] );
                            }
			}
		}

		/* Keyword: COLLIMATOR */
		else if( !strncmp(key,"COLL",4) && ntok >= 1 ) {
			for(i=0;i<ntok;i++) {
			    if ( strstr( token[i], "WID" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.slitx= atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			    else if ( strstr( token[i], "HEI" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.slity = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			}
		}
		
		/* Keyword: PHI */
		else if(!strncmp(key,"PHI",3) && ntok >= 0 ) {
			for(i=0;i<ntok;i++) {
			    if ( strstr( token[i], "SPE" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.phi_speed = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			    else if ( strstr( token[i], "STE" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.phi_steps = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			    else if ( strstr( token[i], "DEF" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.phi_def = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			}
		}

		/* Keyword: OMEGA */
		else if(!strncmp(key,"OME",3) && ntok >= 0 ) {
			for(i=0;i<ntok;i++) {
			    if ( strstr( token[i], "SPE" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.ome_speed = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			    else if ( strstr( token[i], "STE" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.ome_steps = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			    else if ( strstr( token[i], "MIN" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.ome_min = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			    else if ( strstr( token[i], "MAX" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.ome_max = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			    else if ( strstr( token[i], "DEF" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.ome_def = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			}
		}

		/* Keyword: CHI */
		else if(!strncmp(key,"CHI",3) && ntok >= 0 ) {
			for(i=0;i<ntok;i++) {
			    if ( strstr( token[i], "SPE" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.chi_speed = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			    else if ( strstr( token[i], "STE" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.chi_steps = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			    else if ( strstr( token[i], "MIN" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.chi_min = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			    else if ( strstr( token[i], "MAX" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.chi_max = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			    else if ( strstr( token[i], "DEF" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.chi_def = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			}
		}

		/* Keyword: THETA */
		else if(!strncmp(key,"THET",4) && ntok >= 0 ) {
			for(i=0;i<ntok;i++) {
			    if ( strstr( token[i], "SPE" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.thet_speed = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			    else if ( strstr( token[i], "STE" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.thet_steps = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			    else if ( strstr( token[i], "MIN" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.thet_min = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			    else if ( strstr( token[i], "MAX" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.thet_max = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			    else if ( strstr( token[i], "DEF" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.thet_def = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			}
		}

		/* Keyword: DISTANCE */
		else if(!strncmp(key,"DIST",4) && ntok >= 0 ) {
			for(i=0;i<ntok;i++) {
			    if ( strstr( token[i], "SPE" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.dist_speed = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			    else if ( strstr( token[i], "STE" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.dist_steps = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			    else if ( strstr( token[i], "DEF" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.dist_def = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			    else if ( strstr( token[i], "USEMIN" ) )
					h.use_distmin = 1;
			    else if ( strstr( token[i], "USEMAX" ) )
					h.use_distmax = 1;
			    else if ( strstr( token[i], "MIN" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.dist_min = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			    else if ( strstr( token[i], "MAX" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.dist_max = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			}
		}

		/* Keyword: CENTER */
		else if(!strncmp(key,"CENT",4) && ntok >= 0 ) {
			for(i=0;i<ntok;i++) {
			    if ( strstr( token[i], "USE" ) )
					h.use_center = 1;
			    else if ( strstr( token[i], "IGN" ) )
					h.use_center = 0;
			    else if ( strstr( token[i], "MIN" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.center_imin = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			}
		}

		/* Keyword: LOCK */
		else if(!strncmp(key,"LOCK",4) && ntok >= 0 ) {
			for(i=0;i<ntok;i++) {
			    if ( strstr( token[i], "PRE" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.prelock_speed = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			    else if ( strstr( token[i], "LOC" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.lock_speed = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			}
		}

		/* Keyword: WAELENGTH */
		else if(!strncmp(key,"WAVE",4) && ntok >= 0 ) {
			    if ( strstr( token[0], "VAR" ) )
				h.lambda_var = 1;
			    else {
				h.lambda_var = 0;
			    	if ( InputType( token[0] ) >= INTEGER )
					h.wavelength = stat_wavelength = atof( token[0] );
			        else
					WrongType( FLOAT, key, token[0] );
			    }
		}

		/* Keyword: MODE   */
		else if(!strncmp(key,"MODE",4) && ntok >= 0 ) {
			for ( i=nmode=0; i<ntok; i++ ) {
				if ( strstr( token[i], "SCAN" ) ) {
					i++;
					if ( InputType( token[i] ) >= INTEGER ) 
						h.scantime[nmode] = atof( token[i] );
					else
						WrongType( FLOAT, key, token[i] );
				}
				else if ( strstr( token[i], "ERAS" ) ) {
					i++;
					if ( InputType( token[i] ) >= INTEGER ) 
						h.erasetime[nmode] = atof( token[i] );
					else
						WrongType( FLOAT, key, token[i] );
				}
				else if ( strstr( token[i], "ROFF" ) ) {
					i++;
					if ( InputType( token[i] ) >= INTEGER) 
						h.roff[nmode] = atof( token[i] );
					else
						WrongType( FLOAT, key, token[i] );
				}
				else if ( strstr( token[i], "TOFF" ) ) {
					i++;
					if ( InputType( token[i] ) >= INTEGER) 
						h.toff[nmode] = atof( token[i] );
					else
						WrongType( FLOAT, key, token[i] );
				}
				else if ( strstr( token[i], "XC" ) ) {
					i++;
					if ( InputType( token[i] ) >= INTEGER) 
						h.xcen = atof( token[i] );
					else
						WrongType( FLOAT, key, token[i] );
				}
				else if ( strstr( token[i], "YC" ) ) {
					i++;
					if ( InputType( token[i] ) >= INTEGER) 
						h.ycen = atof( token[i] );
					else
						WrongType( FLOAT, key, token[i] );
				}
				else if ( strstr( token[i], "ADC" ) ) {
					i++;
					if ( InputType( token[i] ) == INTEGER) 
						h.adcoff[nmode] = atoi( token[i] );
					else
						WrongType( INTEGER, key, token[i] );
				}
				else if ( strstr( token[i], "AADD" ) ) {
					i++;
					if ( InputType( token[i] ) == INTEGER) 
						h.adcadd_A[nmode] = atoi( token[i] );
					else
						WrongType( INTEGER, key, token[i] );
				}
				else if ( strstr( token[i], "BADD" ) ) {
					i++;
					if ( InputType( token[i] ) == INTEGER) 
						h.adcadd_B[nmode] = atoi( token[i] );
					else
						WrongType( INTEGER, key, token[i] );
				}
				else if ( strstr( token[i], "ADD" ) ) {
					i++;
					if ( InputType( token[i] ) == INTEGER) 
						h.adcadd[nmode] = atoi( token[i] );
					else
						WrongType( INTEGER, key, token[i] );
				}
				else {
				    if ( InputType( token[i] ) == INTEGER ){
					size = atoi( token[i] );
					for ( nmode=0; nmode<MAX_SCANMODE; nmode++ ) {
						if ( size == h.size[nmode] ) break;
					}
				    }
				    else {
					WrongType( INTEGER, key, token[i] );
					break;
				    }
				}
			}
		}

		/* Keyword: UNITS */
		else if(!strncmp(key,"UNIT",4) && ntok >= 0 )
			for(i=0;i<ntok;i++) {
			    if ( strstr( token[i], "TIME" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.units_time = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			    else if ( strstr( token[i], "DOS" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.units_dose = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			}
		
		/* Keyword: INITIALIZE */
		else if(!strncmp(key,"INIT",4) && ntok >= 0 )
			for(i=0;i<ntok;i++) {
			    if ( strstr( token[i], "MIN" ) ) {
				h.init_maxdist=0;
			    }
			    else if ( strstr( token[i], "MAX" ) ) {
				h.init_maxdist=1;
			    }
			}

		/* Keyword: IGNORE */
		else if(!strncmp(key,"IGNO",4) && ntok >= 0 )
			for(i=0;i<ntok;i++) {
			    if ( strstr( token[i], "PHI" ) )
				h.use_phi = 0;
			    else if ( strstr( token[i], "OME" ) ) 
				h.use_ome = 0;
			    else if ( strstr( token[i], "CHI" ) )
				h.use_chi = 0;
			    else if ( strstr( token[i], "THE" ) )
				h.use_thet = 0;
			    else if ( strstr( token[i], "DIS" ) )
				h.use_dist = 0;
			    else if ( strstr( token[i], "ERA" ) )
				h.use_erase = 0;
			    else if ( strstr( token[i], "XRA" ) )
				h.use_xray  = 0;
			    else if ( strstr( token[i], "SOU" ) )
				h.use_sound = 0;
			    else if ( strstr( token[i], "BAS" ) )
				h.use_base  = 0;
			    else if ( strstr( token[i], "IMA" ) )
				h.use_image = 0;
			    else if ( strstr( token[i], "DOS" ) )
				h.use_dose  = 0;
                            else if ( strstr( token[i], "RUN" ) )
                                h.use_run   = 0;
                            else if ( strstr( token[i], "STA" ) )
                                h.use_stats = 0;
                            else if ( strstr( token[i], "Z-A" ) )
                                h.use_zaxis = 0;
                            else if ( strstr( token[i], "WAV" ) )
                                h.use_wave  = 0;
                            else if ( strstr( token[i], "SHE" ) )
                                h.use_shell= 0;
                            else if ( strstr( token[i], "ERR" ) )
                                h.use_error[0] = 1;
                            else if ( h.use_error[0] > 0 ) {
                                strcpy( str, token[i] );
                                for ( j=k=0; k<strlen(str); k++ ) {
                                        if ( !isdigit( str[k] ) ) {
                                                j=1;
                                                break;
                                        }
                                }
                                if ( j==0 && h.use_error[0] < MAX_IGNORE_ERR ) {
                                        j = atoi(str);
                                        h.use_error[h.use_error[0]] = atoi(str);
                                        h.use_error[0] ++;
                                }
                            }
                            else if ( strstr( token[i], "SPY" ) )
                                h.use_msg  = 0;
                            else if ( strstr( token[i], "ADC" ) )
                                h.use_adc = 0;
                            else if ( strstr( token[i], "HTM" ) )
                                h.use_html = 0;
                            else if ( strstr( token[i], "SUM" ) )
                                h.use_txt = 0;
			    else if ( strstr( token[i], "CENT" ) )
					h.use_center = 0;
			}

		/* Keyword: USE */
		else if(!strncmp(key,"USE",3) && ntok >= 0 )
			for(i=0;i<ntok;i++) {
			    if ( strstr( token[i], "PHI" ) )
				h.use_phi = 1;
			    else if ( strstr( token[i], "OME" ) )
				h.use_ome = 1;
			    else if ( strstr( token[i], "CHI" ) )
				h.use_chi = 1;
			    else if ( strstr( token[i], "THE" ) )
				h.use_thet = 1;
			    else if ( strstr( token[i], "DIS" ) )
				h.use_dist = 1;
			    else if ( strstr( token[i], "ERA" ) )
				h.use_erase = 1;
			    else if ( strstr( token[i], "XRA" ) )
				h.use_xray  = 1;
			    else if ( strstr( token[i], "SOU" ) )
				h.use_sound = 1;
			    else if ( strstr( token[i], "BAS" ) )
				h.use_base  = 1;
			    else if ( strstr( token[i], "IMA" ) )
				h.use_image = 1;
			    else if ( strstr( token[i], "DOS" ) )
				h.use_dose  = 1;
                            else if ( strstr( token[i], "SPI" ) )
                                h.use_image = 2;
                            else if ( strstr( token[i], "SPK" ) )
                                h.use_image = 3;
                            else if ( strstr( token[i], "RUN" ) )
                                h.use_run   = 1;
                            else if ( strstr( token[i], "STATS" ) )
                                h.use_stats = 1;
                            else if ( strstr( token[i], "STATUS" ) )
                                status_interval = 1;
                            else if ( strstr( token[i], "Z-A" ) )
                                h.use_zaxis = 1;
                            else if ( strstr( token[i], "WAV" ) )
                                h.use_wave  = 1;
                            else if ( strstr( token[i], "SHE" ) )
                                h.use_shell= 1;
                            else if ( strstr( token[i], "SPY" ) )
                                h.use_msg  = 1;
                            else if ( strstr( token[i], "ADC" ) )
                                h.use_adc  = 1;
                            else if ( strstr( token[i], "HTM" ) )
                                h.use_html = 1;
                            else if ( strstr( token[i], "SUM" ) )
                                h.use_txt = 1;
			    else if ( strstr( token[i], "CENT" ) )
				h.use_center = 1;
			}

		/* Keyword: NETWORK */
		else if(!strncmp(key,"NET",3) && ntok >= 1 )
			for(i=0;i<ntok;i++) {
			    if ( strstr( token[i], "PORT" ) || strstr( token[i], "port") ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.port = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			    else if ( strstr( token[i], "HOST" ) || strstr( token[i], "host") ) {
				i++;
				strcpy( h.host, token[i] );
			    }
			    /* Host */
			    else {
				strcpy( h.host, token[i] );
			    }
			}

		/* Keyword: MEMORY */
		else if(!strncmp(key,"MEMO",4) && ntok >= 0 )
			    if ( strstr( token[0], "SMA" ) )
				h.memory = 0;

	} /* End of while loop */

	/*
	 * End of input lines (while loop)
	 */

	/* Ignore all base functions requested */
	if ( h.use_base == 0 ) {
		h.use_phi 	= 0;
		h.use_chi 	= 0;
		h.use_ome 	= 0;
		h.use_thet 	= 0;
		h.use_dist 	= 0;
		h.use_xray 	= 0;
	}

	if ( h.use_wave ) h.lambda_var = 1;

	return( h );

}

/******************************************************************
 * Function: SetConfig
 ******************************************************************/
CONFIG
SetConfig()
{
CONFIG		h;
int		i;

	memset( &h, 0, sizeof( CONFIG ) );

	h.phi_speed		= 2000;
	h.ome_speed		= 2000;
	h.chi_speed		= 2000;
	h.thet_speed		= 2000;
	h.dist_speed		= 1000;

	h.phi_steps		= 500;
	h.chi_steps		= 500;
	h.ome_steps		= 500;
	h.thet_steps		= 500;
	h.dist_steps		= 100;

	h.dist_min		= 80.0;
	h.thet_min		= -360.0;
	h.chi_min		= -360.0;
	h.ome_min		= -360.0;
	h.dist_max		= 2000.0;
	h.thet_max		= 360.0;
	h.chi_max		= 360.0;
	h.ome_max		= 360.0;
	h.chi_def		= 0.0;
	h.phi_def		= 0.0;
	h.ome_def		= 0.0;
	h.thet_def		= 0.0;
	h.dist_def		= 100.0;

	h.use_phi		= 1;
	h.use_chi		= 0;
	h.use_ome		= 0;
	h.use_thet		= 0;
	h.use_dist		= 1;
	h.use_xray		= 1;
	h.use_base		= 1;
	h.use_erase		= 1;
	h.use_sound		= 0;
	h.use_image		= 0;
	h.use_dose 		= 1;
	h.use_distmin		= 0;
	h.use_distmax		= 0;
        h.use_msg               = 1;
        h.use_adc               = 0;
        h.use_run               = 1;
        h.use_stats             = 0;
        h.use_zaxis             = 0;
        h.use_wave              = 0;
        h.use_shell             = 0;
	h.use_center		= 0;
	h.center_imin		= 5;

	h.init_maxdist		= 1;

	h.units_time		= 4000;
	h.units_dose		= 1000;
	h.dosemin		= 1.0;

	h.spiral_max		= 65500;
	h.spiral_scale		= 1.0;

	h.intensmin		= -999.;
	h.intenswarn		= 20.0;
	h.gain			= -1.0;

	h.mA			= 20;
	h.kV			= 10;
	h.polar			= 0.0;
	h.slitx			= 0.3;
	h.slity			= 0.3;

	h.wavelength		= 1.541789;
	h.lambda_var		= 0;
	h.memory		= 1;
	h.colors		= 32;
	h.flags 		= 0;
        h.lock_speed            = 10;
        h.prelock_speed         = 20;
        h.shutter_delay         = 0;
        h.xcen                  = 0.0;
        h.ycen                  = 0.0;

	/*
	 * Defaults for different scan modi:
	 * Scan/erase times as measured.
	 */
	h.scantime[0]		= 80;
	h.scantime[1]		= 66;
	h.scantime[2]		= 48;
	h.scantime[3]		= 34;
	h.scantime[4]		=108;
	h.scantime[5]		= 87;
	h.scantime[6]		= 62;
	h.scantime[7]		= 42;

        h.erasetime[0]          = 20;
        h.erasetime[1]          = 20;
        h.erasetime[2]          = 20;
        h.erasetime[3]          = 20;
        h.erasetime[4]          = 20;
        h.erasetime[5]          = 20;
        h.erasetime[6]          = 20;
        h.erasetime[7]          = 20;

	h.diameter[0] = h.diameter[4]	= 345;
	h.diameter[1] = h.diameter[5]	= 300;
	h.diameter[2] = h.diameter[6]	= 240;
	h.diameter[3] = h.diameter[7]	= 180;

	for( i=0; i<4; i++ ) {
		h.pixelsize[i]		= 0.15;
		h.pixelsize[i+4]	= 0.10;
		h.size[i]		= (short)(h.diameter[i  ] / h.pixelsize[i  ] + 0.99);
		h.size[i+4]		= (short)(h.diameter[i+4] / h.pixelsize[i+4] + 0.99);
		h.roff[i] = h.roff[i+4]	= 0;
	}

        for( i=0; i<8; i++ ) {
                h.roff[i]               = 0;
                h.toff[i]               = 0;
                h.adcoff[i]             = 100;
                h.adcadd[i]             = 0;
                h.adcadd_A[i]           = 0;
                h.adcadd_B[i]           = 0;
        }

	for ( i=0; i<MAX_IGNORE_ERR; i++ ) h.use_error[i] = 0;

	strcpy( h.source, "\0" );
	strcpy( h.filter, "\0" );
	return( h );
}

/******************************************************************
 * Function: PutConfig
 ******************************************************************/
void
PutConfig(CONFIG h)
{
int		i,j;
char            num[10];
extern FILE 	*fpout;	
extern int 	verbose;

	status_interval = 0;

        for ( i=0; i<MAX_SCANMODE; i++ ) {
                if ( h.adcadd[i] != 0 )
                        h.adcadd_A[i] = h.adcadd_B[i] = h.adcadd[i];
        }

	for ( i=0; i<MAX_SCANMODE; i++ ) {
		sprintf( str, "scan345: MODE       %d  SCAN %1.1f  ERAS %1.1f\n",h.size[i],h.scantime[i],h.erasetime[i]);  
		if ( verbose > 0 ) fprintf( fpout,  str );
		if ( verbose > 2 ) fprintf( stdout, str );
		sprintf( str, "scan345: MODE       %d  ROFF %1.1f  TOFF %1.1f ADC %d  AADD %d  BADD %d\n",h.size[i],h.roff[i],h.toff[i],h.adcoff[i],h.adcadd_A[i],h.adcadd_B[i]);  
		if ( verbose > 0 ) fprintf( fpout,  str );
		if ( verbose > 2 ) fprintf( stdout, str );
	}

	if ( h.use_phi ) {
		sprintf( str, "scan345: PHI        SPEED %d STEPS %d\n",h.phi_speed,h.phi_steps);  
		if ( verbose > 0 ) fprintf( fpout,  str );
		if ( verbose > 2 ) fprintf( stdout, str );
	}
	else {
		sprintf( str, "scan345: PHI        DEFAULT %1.3f\n",h.phi_def);  
		if ( verbose > 0 ) fprintf( fpout,  str );
		if ( verbose > 2 ) fprintf( stdout, str );
	}

	if ( h.use_ome ) {
		sprintf( str, "scan345: OMEGA      SPEED %d STEPS %d MIN %1.3f MAX %1.3f\n",h.ome_speed,h.ome_steps,h.ome_min,h.ome_max);  
		if ( verbose > 0 ) fprintf( fpout,  str );
		if ( verbose > 2 ) fprintf( stdout, str );
	}
	else {
		sprintf( str, "scan345: OMEGA      DEFAULT %1.3f\n",h.ome_def);  
		if ( verbose > 0 ) fprintf( fpout,  str );
		if ( verbose > 2 ) fprintf( stdout, str );
	}


	if ( h.use_chi ) {
		sprintf( str, "scan345: CHI        SPEED %d STEPS %d MIN %1.3f MAX %1.3f\n",h.chi_speed,h.chi_steps,h.chi_min,h.chi_max);  
		if ( verbose > 0 ) fprintf( fpout,  str );
		if ( verbose > 2 ) fprintf( stdout, str );

	}
	else {
		sprintf( str, "scan345: CHI        DEFAULT %1.3f\n",h.chi_def);  
		if ( verbose > 0 ) fprintf( fpout,  str );
		if ( verbose > 2 ) fprintf( stdout, str );
	}

	if ( h.use_thet ) {
		sprintf( str, "scan345: THETA      SPEED %d STEPS %d MIN %1.3f MAX %1.3f\n",h.thet_speed,h.thet_steps,h.thet_min,h.thet_max);  
		if ( verbose > 0 ) fprintf( fpout,  str );
		if ( verbose > 2 ) fprintf( stdout, str );
	}
	else {
		sprintf( str, "scan345: THETA      DEFAULT %1.3f\n",h.thet_def);
		if ( verbose > 0 ) fprintf( fpout,  str );
		if ( verbose > 2 ) fprintf( stdout, str );
	}

	if ( h.use_dist ) {
		sprintf( str, "scan345: DISTANCE   SPEED %d STEPS %d MIN %1.3f MAX %1.3f",h.dist_speed,h.dist_steps,h.dist_min,h.dist_max);  
		if ( h.use_distmin ) strcat( str, " USEMIN " );
		if ( h.use_distmax ) strcat( str, " USEMAX " );
		strcat( str, "\n");
		if ( verbose > 0 ) fprintf( fpout,  str );
		if ( verbose > 2 ) fprintf( stdout, str );
	}
	else {
		sprintf( str, "scan345: DISTANCE   DEFAULT %1.3f\n",h.dist_def); 
		if ( verbose > 0 ) fprintf( fpout,  str );
		if ( verbose > 2 ) fprintf( stdout, str );
	}

	if ( h.use_center ) {
		sprintf( str, "scan345: CENTER     USE  MIN %d\n",h.center_imin);
		if ( verbose > 0 ) fprintf( fpout,  str );
		if ( verbose > 2 ) fprintf( stdout, str );
	}

	sprintf( str, "scan345: UNITS      TIME  %d DOSE  %d\n",h.units_time,h.units_dose);  
	if ( verbose > 0 ) fprintf( fpout,  str );
	if ( verbose > 2 ) fprintf( stdout, str );

	sprintf( str, "scan345: IGNORE     ");
	if ( h.use_phi  ==0 )	strcat(str, "PHI ");
	if ( h.use_ome  ==0 )	strcat(str, "OMEGA ");
	if ( h.use_chi  ==0 )	strcat(str, "CHI ");
	if ( h.use_thet ==0 )	strcat(str, "THETA ");
	if ( h.use_zaxis==0 )	strcat(str, "Z-AXIS ");
	if ( h.use_dist ==0 )	strcat(str, "DISTANCE ");
	strcat( str, "\n");
	if ( strlen( str ) > 20 ) {
		if ( verbose > 0 ) fprintf( fpout,  str );
		if ( verbose > 2 ) fprintf( stdout, str );
	}

	sprintf( str, "scan345: IGNORE     ");
	if ( h.use_xray ==0 )	strcat(str, "XRAY ");
	if ( h.use_erase==0 )	strcat(str, "ERASE ");
	if ( h.use_adc  ==0 )	strcat(str, "ADC ");
	if ( h.use_sound==0 )	strcat(str, "SOUND ");
	if ( h.use_shell==0 )	strcat(str, "SHELL ");
	strcat( str, "\n");
	if ( strlen( str ) > 20 ) {
		if ( verbose > 0 ) fprintf( fpout,  str );
		if ( verbose > 2 ) fprintf( stdout, str );
	}

	sprintf( str, "scan345: IGNORE     ");
	if ( h.use_msg  ==0 )	strcat(str, "SPY ");
	if ( h.use_wave ==0 )	strcat(str, "WAVE ");
	if ( h.use_run  ==0 )	strcat(str, "RUN ");
	if ( h.use_html ==0 )	strcat(str, "HTML ");
	if ( h.use_txt  ==0 )	strcat(str, "SUMMARY ");
	strcat( str, "\n");
	if ( strlen( str ) > 20 ) {
		if ( verbose > 0 ) fprintf( fpout,  str );
		if ( verbose > 2 ) fprintf( stdout, str );
	}

	if ( h.use_error[0] > 1 ) {
		sprintf( str, "scan345: IGNORE     ERROR");
		for ( j=1; j<h.use_error[0]; j++ ) {
			sprintf( num, " %d", h.use_error[j] );
			strcat ( str, num );
		}
		strcat( str, "\n");
		if ( verbose > 0 ) fprintf( fpout,  str );
		if ( verbose > 2 ) fprintf( stdout, str );
	}

	sprintf( str, "scan345: USE        ");
	if ( h.use_phi  ==1 )	strcat(str, "PHI ");
	if ( h.use_ome  ==1 )	strcat(str, "OMEGA ");
	if ( h.use_chi  ==1 )	strcat(str, "CHI ");
	if ( h.use_thet ==1 )	strcat(str, "THETA ");
	if ( h.use_zaxis==1 )	strcat(str, "Z-AXIS ");
	if ( h.use_dist ==1 )	strcat(str, "DISTANCE ");
	strcat( str, "\n");
	if ( strlen( str ) > 20 ) {
		if ( verbose > 0 ) fprintf( fpout,  str );
		if ( verbose > 2 ) fprintf( stdout, str );
	}

        sprintf( str, "scan345: USE        ");
	if ( h.use_xray ==1 )	strcat(str, "XRAY ");
	if ( h.use_erase==1 )	strcat(str, "ERASE ");
	if ( h.use_sound==1 )	strcat(str, "SOUND ");
	if ( h.use_shell==1 )	strcat(str, "SHELL ");
	if ( h.use_adc  ==1 )	strcat(str, "ADC ");
	if ( status_interval)	strcat(str, "STATUS ");
	strcat( str, "\n");
	if ( strlen( str ) > 20 ) {
		if ( verbose > 0 ) fprintf( fpout,  str );
		if ( verbose > 2 ) fprintf( stdout, str );
	}


        sprintf( str, "scan345: USE        ");
	if ( h.use_wave ==1 )	strcat(str, "WAVE ");
	if ( h.use_msg  ==1 )	strcat(str, "SPY ");
	if ( h.use_run  ==1 )	strcat(str, "RUN ");
	if ( h.use_html ==1 )	strcat(str, "HTML ");
	if ( h.use_txt  ==1 )	strcat(str, "SUMMARY ");
	if ( h.use_image==1 )	strcat(str, "IMAGE ");
	if ( h.use_image==2 )	strcat(str, "SPIRAL ");
	if ( h.use_image==3 )	strcat(str, "SPK ");
	strcat( str, "\n");
	if ( strlen( str ) > 20 ) {
		if ( verbose > 0 ) fprintf( fpout,  str );
		if ( verbose > 2 ) fprintf( stdout, str );
	}

	if ( h.init_maxdist ) 
		sprintf( str, "scan345: INITIAL    MAXDIST\n");
	else
		sprintf( str, "scan345: INITIAL    MINDIST\n");
	if ( verbose > 0 ) fprintf( fpout,  str );
	if ( verbose > 2 ) fprintf( stdout, str );
		
	sprintf( str, "scan345: NETWORK    HOST %s   PORT %d\n",h.host,h.port);
	if ( verbose > 0 ) fprintf( fpout,  str );
	if ( verbose > 2 ) fprintf( stdout, str );

	sprintf( str, "scan345: SPIRAL     SCALE %1.3f  MAX %d\n",h.spiral_scale, h.spiral_max);
	if ( verbose > 0 ) fprintf( fpout,  str );
	if ( verbose > 2 ) fprintf( stdout, str );

	sprintf( str, "scan345: FLAGS      %d\n",h.flags);  
	if ( verbose > 0 ) fprintf( fpout,  str );
	if ( verbose > 2 ) fprintf( stdout, str );

	sprintf( str, "scan345: COLORS     %d\n",h.colors);  
	if ( verbose > 0 ) fprintf( fpout,  str );
	if ( verbose > 2 ) fprintf( stdout, str );

	if ( h.sets < 4 ) h.sets = 4;
	if ( h.sets > 4 ) h.sets = 8;
	sprintf( str, "scan345: SETS       %d\n",h.sets);  
	if ( verbose > 0 ) fprintf( fpout,  str );
	if ( verbose > 2 ) fprintf( stdout, str );

	if ( h.memory )
		sprintf( str, "scan345: MEMORY     LARGE\n");  
	else
		sprintf( str, "scan345: MEMORY     SMALL\n");  
	if ( verbose > 0 ) fprintf( fpout,  str );
	if ( verbose > 2 ) fprintf( stdout, str );

	if ( h.gain != 1.0 ) {
		sprintf( str, "scan345: GAIN       %1.0f\n",h.gain);  
		if ( verbose > 0 ) fprintf( fpout,  str );
		if ( verbose > 2 ) fprintf( stdout, str );
	}

	sprintf( str, "scan345: INTENSITY  MIN %1.3f  WARNING %1.1f  DOSE %1.3f\n",h.intensmin,h.intenswarn,h.dosemin);  
	if ( verbose > 0 ) fprintf( fpout,  str );
	if ( verbose > 2 ) fprintf( stdout, str );

	sprintf( str, "scan345: WAVELENGTH %1.5f\n",h.wavelength);  
	if ( verbose > 0 ) fprintf( fpout,  str );
	if ( verbose > 2 ) fprintf( stdout, str );

	sprintf( str, "scan345: MONOCHROM  %s  POLAR %1.3f\n",h.filter, h.polar);
	if ( verbose > 0 ) fprintf( fpout,  str );
	if ( verbose > 2 ) fprintf( stdout, str );

	sprintf( str, "scan345: COLLIMATOR WIDTH %1.3f  HEIGHT %1.3f\n",h.slitx,h.slity);
	if ( verbose > 0 ) fprintf( fpout,  str );
	if ( verbose > 2 ) fprintf( stdout, str );

	sprintf( str, "scan345: GENERATOR  %s  mA %1.1f  kV %1.1f\n",h.source, h.mA,h.kV);
	if ( verbose > 0 ) fprintf( fpout,  str );
	if ( verbose > 2 ) fprintf( stdout, str );

	if ( status_interval ) {
		sprintf( str, "scan345: STATUS     %d\n",status_interval);
		if ( verbose > 0 ) fprintf( fpout,  str );
		if ( verbose > 2 ) fprintf( stdout, str );
	}

	if ( h.use_zaxis ) h.use_ome=1;

}
