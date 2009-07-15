/*********************************************************************
 *
 * nb_header.c
 * 
 * Author:  Claudio Klein, marresearch GmbH.
 * Version: 1.2
 * Date:    26/05/2003
 *
 * 1.2		26/05/03	unistd.h added for ppc
 *
 *********************************************************************/
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#ifndef __sgi
#include <stdlib.h>
#endif

/*
 * mar software include files
 */
#include "nb_header.h"

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
MARNB_HEADER	GetmarNBHeader(FILE *);
MARNB_HEADER	SetmarNBHeader(void);
int 		PutmarNBHeader(int  , MARNB_HEADER);

/******************************************************************
 * Function: GetmarNBHeader
 ******************************************************************/
MARNB_HEADER 
GetmarNBHeader(FILE *fp)
{
MARNB_HEADER	h;
int 		i,j, itmp, size,fpos, ntok=0;
char 		str[64], buf[128], *key, *token[20];

	/*
	 * Set defaults
	 */

	h = SetmarNBHeader();


	if( fp == NULL ) return( h );

	fseek( fp, 0, SEEK_SET );

	if ( fread( &h.byteorder, sizeof(int), 1, fp ) < 1 ) {
		return( h );
	}
	fgets( buf, 60, fp );
	fpos = 64;

	/* First ASCII line (bytes 128 to 192 contains: mar research */
	/* Ignore it...                                              */

	fseek( fp, 192, SEEK_SET );
	fpos = 192;

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
		else if(strstr(buf,"REMARK") ) 
			continue;
/*
*/

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
				if ( isalnum( str[j] ) ) str[j] = toupper( str[j] );
			strcpy( token[i] , str );
			RemoveBlanks( token[i] );
		}

		/* Keyword: PROGRAM */
		if(!strncmp(key,"PROG",4) && ntok >= 2 ) {
			strcpy( h.program, token[0] );
			strcpy( h.version, token[2] );
		}

		/* Keyword: SCALE */
		else if(!strncmp(key,"SCAL",4) && ntok >= 0 )
			if ( InputType( token[0] ) >= INTEGER ) 
				h.scale = atof( token[0] );
			else
				WrongType( FLOAT, key, token[0] );

		/* Keyword: PHIOFF */
		else if(!strncmp(key,"PHI",3) && ntok >= 0 )
			if ( InputType( token[0] ) >= INTEGER ) 
				h.phioff = atof( token[0] );
			else
				WrongType( FLOAT, key, token[0] );

		/* Keyword: CUTOFF */
		else if(!strncmp(key,"CUT",3) && ntok >= 0 )
			if ( InputType( token[0] ) >= INTEGER ) 
				h.cutoff = atof( token[0] );
			else
				WrongType( FLOAT, key, token[0] );

		/* Keyword: GAIN */
		else if(!strncmp(key,"GAIN",4) && ntok >= 0 )
			if ( InputType( token[0] ) >= INTEGER ) 
				h.gain = atof( token[0] );
			else
				WrongType( FLOAT, key, token[0] );

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
			    else if ( strstr( token[i], "SUB" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.subpixels = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			}
		}


		/* Keyword: SCANNER */
		else if(!strncmp(key,"SCAN",4) && ntok >= 0 )
			if ( InputType( token[0] ) == INTEGER ) 
				h.scanner = atoi( token[0] );
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

		/* Keyword: MODE   */
		else if(!strncmp(key,"MODE",4) && ntok >= 0 ) {
		    if ( InputType( token[0] ) != INTEGER ) 
			WrongType( INTEGER, key, token[0] );
		    else {
			size = atoi( token[0] );

			for ( j=0, i=0; i<MAX_NBMODE; i++ ) {
				if ( h.size[i] == 0 && h.mode <= MAX_NBMODE ) {
					h.size[h.mode] = size;
					j = h.mode;
					h.mode++;
					break;
				}
				else if ( h.size[i] == size ) {
					j = i;
					break;
				}
			}

			for ( i=1; i<ntok; i++ ) {
				if ( strstr( token[i], "POS" ) ) {
					i++;
					if ( InputType( token[i] ) == INTEGER ) 
						h.fpos[j] = atoi( token[i] );
					else
						WrongType( INTEGER, key, token[i] );
				}
				else if ( strstr( token[i], "PIX" ) ) {
					i++;
					if ( InputType( token[i] ) == INTEGER ) 
						h.pixels[j] = atoi( token[i] );
					else
						WrongType( INTEGER, key, token[i] );
				}
				else if ( strstr( token[i], "X" ) ) {
					i++;
					if ( InputType( token[i] ) == INTEGER ) 
						h.x[j] = atoi( token[i] );
					else
						WrongType( INTEGER, key, token[i] );
				}
				else if ( strstr( token[i], "Y" ) ) {
					i++;
					if ( InputType( token[i] ) == INTEGER ) 
						h.y[j] = atoi( token[i] );
					else
						WrongType( INTEGER, key, token[i] );
				}
				else if ( strstr( token[i], "SK" ) ) {
					i++;
					if ( InputType( token[i] ) == INTEGER ) 
						h.skip[j] = atoi( token[i] );
					else
						WrongType( INTEGER, key, token[i] );
				}
				else if ( strstr( token[i], "RO" ) ) {
					i++;
					if ( InputType( token[i] ) == INTEGER ) 
						h.roff[j] = atoi( token[i] );
					else
						WrongType( INTEGER, key, token[i] );
				}
			 }
		    }
		}

		/* Keyword: NUMBER */
		else if(!strncmp(key,"NUMB",3) && ntok >= 1 ) {
			for(i=0;i<ntok;i++) {
			    if ( strstr( token[i], "PIX" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.tot_pixels = atoi( token[i] );
				}
			        else
					WrongType( INTEGER, key, token[i] );
			    }
			    else if ( strstr( token[i], "NEI" ) ) {
				i++;
			    	if ( InputType( token[i] ) == INTEGER ) {
					h.nbs = atoi( token[i] );
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

	fseek( fp, 4096, SEEK_SET );

	return( h );

}

/******************************************************************
 * Function: SetmarNBHeader
 ******************************************************************/
MARNB_HEADER 
SetmarNBHeader()
{
MARNB_HEADER	h;
int		i;

	h.byteorder		= 1234;
	h.pixel_length 		= 150.0;
	h.pixel_height 		= 150.0;
	h.subpixels    		= 0;
	h.scanner		= 25;
	h.scale     		= 65535.0;
	h.phioff		= 0.0;
	h.cutoff		= 0.0;
	h.gain  		= 1.0;

	h.mode			= 0;
	h.tot_pixels		= 0;
	h.nbs   		= 0;

	h.date[0]		= '\0';
	h.program[0]		= '\0';
	h.version[0]		= '\0';

	for ( i=0; i<MAX_NBMODE; i++ ) {
		h.x[i]		= 0;
		h.y[i]		= 0;
		h.size[i]	= 0;
		h.fpos[i]	= 0;
		h.pixels[i]	= 0;
		h.roff[i]	= 0;
		h.skip[i]	= 0;
	}

	return( h );
}

/******************************************************************
 * Function: PutmarNBHeader
 ******************************************************************/
int
PutmarNBHeader(int fd, MARNB_HEADER h)
{
int		i,j,k=0;
int		io;
int             byte_order=1234;
time_t		clock;
char		str[128];
char		c=' ';

    	if ( fd < 0 )return(0);

    	time( &clock );	
        sprintf( h.date, "%s", (char *)ctime( &clock ));

	lseek( fd, 0, SEEK_SET );

	/* First 4 bytes: integer with 1234 */
        io = write( fd, &byte_order, sizeof(int));

	while( lseek( fd, 0, SEEK_CUR ) < 128) {
        	i = write( fd, &c, 1); 
		if ( i< 1) break;
		io += i;
	}

	lseek( fd, 128, SEEK_SET );

	sprintf( str, "mar research NEIGHBOUR CODE"); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64); k++;

	sprintf( str, "PROGRAM  %s Version %s\n",h.program,h.version); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64); k++;

	sprintf( str, "DATE     %s",h.date); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64); k++;

	sprintf( str, "SCANNER  %d\n",h.scanner); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64); k++;

	sprintf( str, "PIXEL    LENGTH %1.0f  HEIGHT %1.0f  SUBPIXELS %d\n",h.pixel_length,h.pixel_height,h.subpixels); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64); k++;

	sprintf( str, "PHIOFF   %1.3f\n",h.phioff); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64); k++;

	sprintf( str, "CUTOFF   %1.3f\n",h.cutoff); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64); k++;

	sprintf( str, "SCALE    %1.3f\n",h.scale); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64); k++;

	sprintf( str, "GAIN     %1.3f\n",h.gain); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64); k++;

	sprintf( str, "NUMBER   PIXELS %d  NEIGHBOURS %d\n",h.tot_pixels,h.nbs); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64); k++;

	for ( j=0; j<MAX_NBMODE; j++ ) {
/*
printf("xxx MODE     %4d X %4d Y %4d ROFF %4d SKIP %-d\n",h.size[j],h.x[j],h.y[j],h.roff[j],h.skip[j]); 
*/
		if ( h.size[j] == 0 ) continue;

		sprintf( str, "MODE     %4d X %4d Y %4d ROFF %4d SKIP %-d\n",h.size[j],h.x[j],h.y[j],h.roff[j],h.skip[j]); 
		for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
		io += write( fd, str, 64); k++;
	}

        for ( j=0; j<MAX_NBMODE; j++ ) {
                if ( h.size[j] == 0 ) continue;
                sprintf( str, "MODE     %4d PIX %-8d  POS %-d\n",h.size[j],h.pixels[j],h.fpos[j]);
                for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
                io += write( fd, str, 64); k++;
        }

	sprintf( str, "END OF HEADER\n"); 
        for ( i=strlen(str); i<63; i++ ) str[i]=' '; str[63]='\n';
        io += write( fd, str, 64); k++;

/*
*/
	while( lseek( fd, 0, SEEK_CUR ) < 4096) {
        	i = write( fd, &c, 1); 
		if ( i< 1) break;
		io += i;
	}

	if ( io<4096 ) {
		fprintf( stderr, "ERROR: only %d out of 4096 written into nb header!!!\n",io);
		return 0;
	}

	return 1;

}
