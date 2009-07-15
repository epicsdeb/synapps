/*********************************************************************
 *
 * mar345_header.c
 * 
 * Author:  	Claudio Klein, marresearch GmbH.
 * Version: 	1.9.0
 * Date:        26/05/2003
 *
 * History:
 * Version    Date    	Changes
 * ______________________________________________________________________
 *
 * 1.9.0      26/05/03	unistd.h added for ppc
 * 1.8.0      18/11/02	Implemented FORMAT PCK4 -> 32-bit pck array
 * 1.7.1      25/10/02	Version number should be ntok-1'th string on PROGRAM
 * 1.7 	      02/09/02	h.detector introduced
 * 1.6	      08/01/02	Truncate h.date at byte 24 to \0
 * 1.5	      06/09/99  Element gap extended (8 values: gaps 1-8)
 * 1.4        14/05/98  Element gap introduced 
 *
 *********************************************************************/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>
#ifndef __sgi
#include <stdlib.h>
#endif
#include <time.h>

/*
 * mar software include files
 */
#include "mar345_header.h"

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

/*
 * Local functions
 */
MAR345_HEADER	Getmar345Header(FILE *);
MAR345_HEADER	Setmar345Header(void  );
int 		Putmar345Header(int   , MAR345_HEADER);

/******************************************************************
 * Function: Getmar345Header
 ******************************************************************/
MAR345_HEADER
Getmar345Header(FILE *fp)
{
MAR345_HEADER	h;
int 		i,j, ntok=0,fpos=0;
int		ngaps = 0;
char 		str[64], buf[128], *key, *token[20];
int		head[32];


	/*
	 * Set defaults
	 */

	h = Setmar345Header();

	if( fp == NULL ) return( h ); 

	fseek( fp, 0, SEEK_SET );

	if ( fread( head, sizeof(int), 32, fp ) < 32) {
		return( h );
	}

	/* First 32 longs contain: */
	h.byteorder 	= (int  )head[ 0];
	h.size 		= (short)head[ 1];
	h.high 		= (int  )head[ 2];
	h.format	= (char )head[ 3];
	h.mode		= (char )head[ 4];
	h.pixels	= (int  )head[ 5];
	h.pixel_length 	= (float)head[ 6];
	h.pixel_height 	= (float)head[ 7];
	h.wave      	= (float)head[ 9]/1000000.;
	h.dist    	= (float)head[ 8]/1000.;
	h.phibeg	= (float)head[10]/1000.;
	h.phiend	= (float)head[11]/1000.;
	h.omebeg	= (float)head[12]/1000.;
	h.omeend	= (float)head[13]/1000.;
	h.chi   	= (float)head[14]/1000.;
	h.theta 	= (float)head[15]/1000.;

	/* First ASCII line (bytes 128 to 192 contains: mar research */
	/* Ignore it...                                              */

	fpos = fseek( fp, 192, SEEK_SET );

	/*
	 * Read input lines
	 */

	while( fgets(buf,64,fp)!= NULL){

		/* Always add 64 bytes to current filemarker after 1 read */
		fpos += 64;
		fseek( fp, fpos, SEEK_SET );

		/* Keyword: END OF HEADER*/
		if(strstr(buf,"END OF HEADER") ) 
			break;
		else if ( strstr( buf, "SKIP" ) ) continue;

		if ( strlen(buf) < 2 ) continue;

		/* Scip comment lines */
		if( buf[0] == '#' || buf[0]=='!' ) continue;

		/* Tokenize input string */
		/* ntok  = number of items on input line - 1 (key) */
		ntok = -1;	

		for(i=0;i<64;i++) {
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

			for ( j=0; j<strlen( str ); j++ )
				if ( isalnum( str[j] ) && !strstr(key,"PROG") ) str[j] = toupper( str[j] );
			strcpy( token[i] , str );
			RemoveBlanks( token[i] );
		}

		/* Keyword: PROGRAM */
		if(!strncmp(key,"PROG",4) && ntok >= 2 ) {
			strcpy( h.program, token[0] );
			strcpy( h.version, token[ntok-1] );
		}

		/* Keyword: OFFSET */
		else if(!strncmp(key,"OFFS",4) && ntok >= 1 ) {
		    	for ( i=0; i<ntok; i++ ) {
			    if ( strstr( token[i], "ROF" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.roff = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			    else if ( strstr( token[i], "TOF" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.toff = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			    /* Compatibility with previous versions for GAP entries: */
			    else if ( strstr( token[i], "GAP" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.gap[1]  = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			}
		}
		/* Keyword: GAP */
		else if(!strncmp(key,"GAPS",4) && ntok >= 1 ) {
		    	for ( i=0; i<ntok; i++ ) {
			    	if ( InputType( token[i] ) == INTEGER ) {
					if ( ngaps < N_GAPS ) 
						h.gap[ngaps++]  = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			}
		}

		/* Keyword: ADC */
		else if(!strncmp(key,"ADC",3) && ntok >= 1 ) {
		    	for ( i=0; i<ntok; i++ ) {
			    if ( strstr( token[i], "A" ) && strlen( token[i] ) == 1 ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.adc_A = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			    else if ( strstr( token[i], "B" ) && strlen( token[i] ) == 1 ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.adc_B = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			    else if ( strstr( token[i], "ADD_A" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.add_A = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			    else if ( strstr( token[i], "ADD_B" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.add_B = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			}
		}

		/* Keyword: MULTIPLIER */
		else if(!strncmp(key,"MULT",4) && ntok >= 0 )
			if ( InputType( token[0] ) >= INTEGER ) 
				h.multiplier = atof( token[0] );
			else
				WrongType( FLOAT, key, token[0] );

		/* Keyword: GAIN */
		else if(!strncmp(key,"GAIN",4) && ntok >= 0 )
			if ( InputType( token[0] ) >= INTEGER ) 
				h.gain = atof( token[0] );
			else
				WrongType( FLOAT, key, token[0] );

		/* Keyword: COUNTS */
		else if(!strncmp(key,"COUN",4) && ntok >= 1 )
			for(i=0;i<ntok;i++) {
			    if ( strstr( token[i], "STA" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.dosebeg = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			    else if ( strstr( token[i], "END" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.doseend = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			    else if ( strstr( token[i], "MIN" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.dosemin = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			    else if ( strstr( token[i], "MAX" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.dosemax = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			    else if ( strstr( token[i], "AVE" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.doseavg = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			    else if ( strstr( token[i], "SIG" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.dosesig = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			    else if ( strstr( token[i], "NME" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.dosen = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			}

		/* Keyword: MODE */
		else if( !strncmp(key,"MODE",4) && ntok >= 0 ) 
			if ( strstr( token[0], "TIME" ) )
				h.mode  = 1;
			else if ( strstr( token[0], "DOSE" ) )
				h.mode  = 0;
			else
				WrongType( STRING, key, token[0] );

		/* Keyword: DISTANCE */
		else if(!strncmp(key,"DIST",4) && ntok >= 0 )
			if ( InputType( token[0] ) >= INTEGER ) 
				h.dist = atof( token[0] );
			else
				WrongType( FLOAT, key, token[0] );

		/* Keyword: PIXELSIZE */
		else if(!strncmp(key,"PIXE",4) && ntok >= 0 ) {
			for(i=0;i<ntok;i++) {
			    if ( strstr( token[i], "LEN" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.pixel_length = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			    else if ( strstr( token[i], "HEI" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.pixel_height= atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			}
		}


		/* Keyword: SCANNER */
		else if(!strncmp(key,"SCAN",4) && ntok >= 0 )
			if ( InputType( token[0] ) == INTEGER ) 
				h.scanner = atoi( token[0] );
			else
				WrongType( INTEGER, key, token[0] );

		/* Keyword: HIGH */
		else if(!strncmp(key,"HIGH",4) && ntok >= 0 )
			if ( InputType( token[0] ) == INTEGER ) 
				h.high    = atoi( token[0] );
			else
				WrongType( INTEGER, key, token[0] );

		/* Keyword: DATE */
		else if(!strncmp(key,"DATE",4) && ntok >= 0 ) {
			for ( i=0; i<strlen( buf ); i++ ) 
				if ( buf[i] == ' ' ) break;
			for ( j=i; j<strlen( buf ); j++ ) 
				if ( buf[j] != ' ' ) break;
			strcpy( h.date, buf+j );
		}

		/* Keyword: REMARK */
		else if(!strncmp(key,"REMA",4) && ntok >= 0 ) {
			for ( i=0; i<strlen( buf ); i++ ) 
				if ( buf[i] == ' ' ) break;
			for ( j=i; j<strlen( buf ); j++ ) 
				if ( buf[j] != ' ' ) break;
			strcpy( h.remark, buf+j );
		}

		/* Keyword: Detector */
		else if(!strncmp(key,"DETE",4) && ntok >= 0 ) {
			if ( strlen( buf ) > 31 ) buf[31] = '\0';
			strcpy( h.detector, buf );
		}

		/* Keyword: FORMAT */
		else if(!strncmp(key,"FORM",4) && ntok >= 1 )  {
			if ( InputType( token[0] ) == INTEGER ) 
				h.size = atoi( token[0] );
			else
				WrongType( INTEGER, key, token[0] );
			for ( i=1; i<ntok; i++ ) {
				if ( strstr( token[i], "PCK4" ) ) 
					h.format = 3;
				else if ( strstr( token[i], "IMA" ) ) 
					h.format = 0;
				else if ( strstr( token[i], "PCK" ) ) 
					h.format = 1;
				else if ( strstr( token[i], "SPI" ) )
					h.format = 2;
				else {
					if ( InputType( token[i] ) == INTEGER ) 
						h.pixels = atoi( token[i] );
					else
						WrongType( INTEGER, key, token[i] );
				}
			}
		}

		/* Keyword: LAMBDA or WAVELENGTH */
		else if( (!strncmp(key,"LAMB",4) || !strncmp(key,"WAVE",4) ) && ntok >= 0 ) 
			if ( InputType( token[0] ) >= INTEGER ) 
				h.wave = atof( token[0] );
			else
				WrongType( FLOAT, key, token[0] );
		

		/* Keyword: MONOCHROMATOR */
		else if( !strncmp(key,"MONO",4) && ntok >=0 ) { 
		    for ( i=0; i<ntok; i++ ) {
			 if ( strstr( token[i], "POLA" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.polar = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			}
			else {
				strcat( h.filter, token[i] );
			}
		    }
		}


		/* Keyword: PHI */
		else if(!strncmp(key,"PHI",3) && ntok >= 1 )
			for(i=0;i<ntok;i++) {
			    if ( strstr( token[i], "STA" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.phibeg = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			    else if ( strstr( token[i], "END" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.phiend = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			    else if ( strstr( token[i], "OSC" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.phiosc = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			}

		/* Keyword: OMEGA */
		else if(!strncmp(key,"OMEG",4) && ntok >= 1 )
			for(i=0;i<ntok;i++) {
			    if ( strstr( token[i], "STA" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.omebeg = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			    else if ( strstr( token[i], "END" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.omeend = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			    else if ( strstr( token[i], "OSC" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.omeosc = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			}

		/* Keyword: TWOTHETA */
		else if( !strncmp(key,"TWOT",4) && ntok >= 0 ) 
			if ( InputType( token[0] ) >= INTEGER ) 
				h.theta = atof( token[0] );
			else
				WrongType( FLOAT, key, token[0] );

		/* Keyword: CHI */
		else if( !strncmp(key,"CHI",3) && ntok >= 0 ) 
			if ( InputType( token[0] ) >= INTEGER ) 
				h.chi   = atof( token[0] );
			else
				WrongType( FLOAT, key, token[0] );

		/* Keyword: RESOLUTION */
		else if( !strncmp(key,"RESO",4) && ntok >= 0 ) 
			if ( InputType( token[0] ) >= INTEGER ) 
				h.resol = atof( token[0] );
			else
				WrongType( FLOAT, key, token[0] );

		/* Keyword: TIME */
		else if( !strncmp(key,"TIME",4) && ntok >= 0 ) 
			if ( InputType( token[0] ) >= INTEGER ) 
				h.time  = atof( token[0] );
			else
				WrongType( FLOAT, key, token[0] );

		/* Keyword: CENTER */
		else if( !strncmp(key,"CENT",4) && ntok >= 1 ) {
			for(i=0;i<ntok;i++) {
			    if ( strstr( token[i], "X" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.xcen = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			    else if ( strstr( token[i], "Y" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.ycen = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			    }
			}
		}
		
		/* Keyword: COLLIMATOR, SLITS */
		else if( ( !strncmp(key,"COLL",4) || !strncmp(key,"SLIT",4) )&& ntok >= 1 ) {
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

		/* Keyword: INTENSITY */
		else if( !strncmp(key,"INTE",4) && ntok >= 0 ) {
		    for(i=0;i<ntok;i++) {
		    	if ( strstr( token[i], "MIN" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.valmin = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			}
		    	else if ( strstr( token[i], "MAX" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.valmax = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			}
		    	else if ( strstr( token[i], "AVE" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.valavg = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			}
		    	else if ( strstr( token[i], "SIG" ) ) {
				i++;
			    	if ( InputType( token[i] ) >= INTEGER ) {
					h.valsig = atof( token[i] );
				}
			        else
					WrongType( FLOAT, key, token[i] );
			}
		    }
		}
		
		/* Keyword: HISTOGRAM */
		else if( !strncmp(key,"HIST",4) && ntok >= 0 ) {
		    for(i=0;i<ntok;i++) {
		    	if ( strstr( token[i], "STA" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.histbeg = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			}
		    	else if ( strstr( token[i], "END" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.histend = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			}
		    	else if ( strstr( token[i], "MAX" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.histmax = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			}
		    }
		}
		
	} /* End of while loop */

	/*
	 * End of input lines (while loop)
	 */

	return( h );

}

/******************************************************************
 * Function: Setmar345Header
 ******************************************************************/
MAR345_HEADER
Setmar345Header()
{
MAR345_HEADER 	h;
int		i;

	h.byteorder		= 1234;
	h.wave      		= 1.541789;
	h.polar    		= 0.0;
	h.pixel_length 		= 150.0;
	h.pixel_height 		= 150.0;
	h.scanner		= 1;
	h.format		= 1;
	h.high  		= 0;
	h.size			= 0;
	h.dist			= 70.0;
	h.multiplier		= 1.0;
	h.mode			= 1;
	h.time			= 0.0;
	h.dosebeg		= 0.0;
	h.doseend		= 0.0;
	h.dosemin		= 0.0;
	h.dosemax		= 0.0;
	h.doseavg		= 0.0;
	h.dosesig		= 0.0;
	h.dosen  		= 0;
	h.phibeg		= 0.0;
	h.phiend		= 0.0;
	h.omebeg		= 0.0;
	h.omeend		= 0.0;
	h.phiosc		= 0;
	h.omeosc		= 0;
	h.theta  		= 0.0;
	h.chi			= 0.0;
	h.gain			= 1.0;
	h.xcen			= 600.;
	h.ycen			= 600.;
	h.kV			= 40.0;
	h.mA			= 50.0;
	h.valmin		= 0;
	h.valmax		= 0;
	h.valavg		= 0.0;
	h.valsig		= 0.0;
	h.histbeg		= 0;
	h.histend		= 0;
	h.histmax		= 0;
	h.roff			= 0.0;
	h.toff			= 0.0;
	h.slitx 		= 0.3;
	h.slity 		= 0.3;
	h.pixels		= 0;
   	h.adc_A			= -1;
   	h.adc_B			= -1;
	h.add_A			= 0;
	h.add_B			= 0;

	h.date[0]		= '\0';
	h.remark[0]		= '\0';
	h.detector[0]		= '\0';
	strcpy( h.source, "\0" );
	strcpy( h.filter, "\0" );

	for ( i=0; i<N_GAPS; i++ )
		h.gap[i]	= 0;

	return( h );
}

/******************************************************************
 * Function: Putmar345Header
 ******************************************************************/
int
Putmar345Header(int fd, MAR345_HEADER h)
{
int		i,k=0;
int		io;
int		head[32];
int             byte_order=1234;
time_t		now;
char		c=' ', str[128];
char		*mode[2]  ={"DOSE","TIME"};
char		*format[4]={"IMAGE","PCK","SPIRAL", "PCK4"};
char		*source[3]={"SEALED TUBE","ROTATING ANODE","SYNCHROTRON"};
char		*filter[3]={"GRAPHITE",   "MIRRORS"       ,"FILTER"     };

    	if ( fd < 0 )return(0);

	h.byteorder = 1234;

    	time( &now);	
        sprintf( str, "%s\0", (char *)ctime( &now ));
	memcpy( h.date, str, 24 );
	h.date[24] = '\0';

	lseek( fd, 0, SEEK_SET );

	/* Make some necessary checks */
	if ( h.mode   > 1 ) h.mode = 1;
	if ( h.format > 3 ) h.format = 1;

	/* First 32 longs contain: */
	memset( head, 0, 32*sizeof(int) );

	head[ 0]	= (int)( h.byteorder );
	head[ 1]	= (int)( h.size );
	head[ 2]	= (int)( h.high );
	head[ 3]	= (int)( h.format );
	head[ 4]	= (int)( h.mode );
	head[ 5]	= (int)( h.pixels );
	head[ 6]	= (int)( h.pixel_length );
	head[ 7]	= (int)( h.pixel_height );
	head[ 8]	= (int)( h.wave   * 1000000. );
	head[ 9]	= (int)( h.dist   * 1000. );
	head[10]	= (int)( h.phibeg * 1000. );
	head[11]	= (int)( h.phiend * 1000. );
	head[12]	= (int)( h.omebeg * 1000. );
	head[13]	= (int)( h.omeend * 1000. );
	head[14]	= (int)( h.chi    * 1000. );
	head[15]	= (int)( h.theta  * 1000. );


	/* First 4 bytes: integer with 1234 */
        io = write( fd, head, 32*sizeof(int) );

	sprintf( str, "mar research"); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64); k++;

	sprintf( str, "PROGRAM        %s Version %s",h.program,h.version); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64); k++;

	sprintf( str, "DATE           %s\0",h.date); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "SCANNER        %d\n",h.scanner); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "FORMAT         %d  %s %d\n",h.size,format[(int)h.format],h.pixels); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "HIGH           %d\n",h.high); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "PIXEL          LENGTH %1.0f  HEIGHT %1.0f\n",h.pixel_length,h.pixel_height); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "OFFSET         ROFF %1.0f  TOFF %1.0f\n",h.roff,h.toff); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "GAPS           %d %d %d %d %d %d %d %d\n",
		h.gap[0],h.gap[1],h.gap[2],h.gap[3],h.gap[4],h.gap[5],h.gap[6],h.gap[7]);
	
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	if ( h.adc_A >= 0 ) {
		sprintf( str, "ADC            A %d  B %d  ADD_A %d  ADD_B %d\n",h.adc_A,h.adc_B,h.add_A,h.add_B); 
		for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
		io += write( fd, str, 64 ); k++;
	}

	sprintf( str, "MULTIPLIER     %1.3f\n",h.multiplier); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "GAIN           %1.3f\n",h.gain); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "WAVELENGTH     %1.5f\n",h.wave); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "DISTANCE       %1.3f\n",h.dist); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "RESOLUTION     %1.3f\n",h.resol); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "PHI            START %1.3f  END %1.3f  OSC %d\n",h.phibeg,h.phiend,h.phiosc); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "OMEGA          START %1.3f  END %1.3f  OSC %d\n",h.omebeg,h.omeend,h.omeosc); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "CHI            %1.3f\n",h.chi); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "TWOTHETA       %1.3f\n",h.theta); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "CENTER         X %1.3f  Y %1.3f\n",h.xcen,h.ycen); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "MODE           %s\n",mode[(int)h.mode]); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "TIME           %1.2f\n",h.time); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "COUNTS         START %1.2f END %1.2f  NMEAS %d\n",h.dosebeg,h.doseend,h.dosen); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "COUNTS         MIN %1.2f  MAX %1.2f\n",h.dosemin,h.dosemax); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "COUNTS         AVE %1.2f  SIG %1.2f\n",h.doseavg,h.dosesig); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "INTENSITY      MIN %d  MAX %d  AVE %1.1f  SIG %1.2f\n",h.valmin,h.valmax,h.valavg,h.valsig); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "HISTOGRAM      START %d  END %d  MAX %d\n",h.histbeg,h.histend,h.histmax); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "GENERATOR      %s  kV %1.1f  mA %1.1f\n",h.source,h.kV, h.mA); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "MONOCHROMATOR  %s  POLAR %1.3f\n",h.filter,h.polar); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "COLLIMATOR     WIDTH %1.2f  HEIGHT %1.2f\n",h.slitx,h.slity); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "REMARK         %s\n",h.remark); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "DETECTOR       %s\n",h.detector); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	sprintf( str, "END OF HEADER\n"); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64 ); k++;

	/*
	 * Fill rest of header (4096 bytes) with blanks:
	 */
	k = 4096;
	while ( lseek( fd, 0, SEEK_CUR ) < k ) {
        	i = write( fd, &c, 1 ); 
		if ( i < 1 ) break;
		io += i;
	}

	if ( io < k ) {
		fprintf( stderr, "ERROR: only %d out of %d bytes written into image header!!!\n",io,k);
		return 0;
	}

	return 1;

}
