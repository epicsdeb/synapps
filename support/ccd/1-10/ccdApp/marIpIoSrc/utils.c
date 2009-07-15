
/*********************************************************************
 *
 * io_utils.c
 * 
 * Author:  Claudio Klein, X-Ray Research GmbH.
 * Version: 1.0
 * Date:    05/07/1996
 *
 *********************************************************************/
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#ifndef __sgi
#include <stdlib.h>
#endif

/*
 * Definitions
 */
#define STRING		0
#define INTEGER		1
#define FLOAT		2

int		InputType();
void		WrongType();
float		GetResol(float, float, float);
void		RemoveBlanks();
void            swapshort();
void            swaplong();

/******************************************************************
 * Function: GetResol
 ******************************************************************/
float
GetResol(float radius, float distance, float wave)
{
float	res;

	if ( distance < 1.0 ) return 0.0;

	res = atan( radius/distance); 	/* Two theta !!! */

	if ( res < 0.0001 ) return 0.0;

	res = wave/(2.0*sin(res/2.0)); 
	
	return( res );
}
	
/******************************************************************
 * Function: InputType 
 ******************************************************************/
int 
InputType(char *string)
{
int		i,j=0,k=0,l=0;

   	for ( i=0 ; i<strlen(string)-1; i++ ) {
		if ( isspace( string[i] ) ) {
			string[i] = ' ';
			j++;
		}
		else if ( string[i] == '.' ) {
			k++;
			continue;
		}

		else if ( string[i] == '-' || string[i] == '+' ) {
			l++;
			continue;
		}
		else if ( isdigit( string[i] ) ) {
			j++;
			continue;
		}
	}

	if ( k == 1 && l == 0 && j == i-1 )
		return( FLOAT );
	else if ( k == 1 && l == 1 && j == i-2 )
		return( FLOAT );
	else if ( k == 0 && l == 1 && j == i-1 )
		return( INTEGER );
	else if ( k == 0 && l == 0 && j == i )
		return( INTEGER );
	else
		return( STRING  );

}

/******************************************************************
 * Function: WrongType 
 ******************************************************************/
void
WrongType(int type, char *key, char *string)
{

	fprintf(stderr,"INPUT ERROR at key '%s' and argument '%s'!\n",key,string);

	if ( type == FLOAT )
		fprintf(stderr,"            Expected data type is: FLOAT...\n");
	else if ( type == INTEGER )
		fprintf(stderr,"            Expected data type is: INTEGER...\n");

}

/******************************************************************
 * Function: RemoveBlanks = remove white space from string
 ******************************************************************/
void
RemoveBlanks(char *str)
{
int i, j=0, len=strlen(str);

        for(i=0;i<len;i++) {
                if (!isspace(str[i]))
                        str[j++] = str[i];
        }
        if ( j>0)
                str[j] = 0;
}

/******************************************************************
 * Function: swaplong = swaps bytes of 32 bit integers
 ******************************************************************/
void
swaplong(data, nbytes)
register char *data;
int nbytes;
{
        register int i, t1, t2, t3, t4;

        for(i=nbytes/4;i--;) {
                t1 = data[i*4+3];
                t2 = data[i*4+2];
                t3 = data[i*4+1];
                t4 = data[i*4+0];
                data[i*4+0] = t1;
                data[i*4+1] = t2;
                data[i*4+2] = t3;
                data[i*4+3] = t4;
        }
}

/******************************************************************
 * Function: swapshort = swaps the two 8-bit halves of a 16-bit word
 ******************************************************************/
void
swapshort(data, n)
register unsigned short *data;
int n;
{
        register int i;
        register unsigned short t;

        for(i=(n>>1)+1;--i;) {
                /*t = (( (*data)&0xFF) << 8 ) | (( (*data)&0xFF00) >> 8);*/
                t = (((*data) << 8) | ((*data) >> 8));
                *data++ = t;
        }
}

