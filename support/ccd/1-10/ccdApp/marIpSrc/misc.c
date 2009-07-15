/***********************************************************************
 * 
 * scan345: misc.c
 *
 * Copyright by:	Dr. Claudio Klein
 * 			X-ray Research GmbH, Hamburg
 *
 * Version:     3.0
 * Date:        31/10/2000
 *
 * Subroutines included in this file:
 * --------------------------------- 
 * Function: rotate_i2 = rotates image clockwise 90. deg (short)
 * Function: rotate_ch = rotates image clockwise 90. deg (char)
 * Function: rotate_i4 = rotates image clockwise 90. deg (int)
 * Function: rotate_i4_anti = rotates image anticlockwise 90. deg
 * Function: rotate_i2_anti = rotates image anticlockwise 90. deg
 * Function: rotate_ch_anti = rotates image anticlockwise 90. deg
 * Function: wtext = puts message in text field of main window
 * 
 ***********************************************************************/
#include <stdio.h>
#include <math.h>
#include <ctype.h>

void 		rotate_i2		(unsigned short *, int);
void 		rotate_ch		(unsigned char  *, int);
void 		rotate_i4		(unsigned int   *, int);
void 		rotate_i4_anti		(unsigned int   *, int);
void 		rotate_i2_anti   	(unsigned short *, int);
void 		rotate_ch_anti   	(unsigned char  *, int);
void		wtext			(int             , char *);

/******************************************************************
 * Function: rotate_i4_anti = rotates image anticlockwise by 90. deg
 *
 *  			0 1 2       2 5 8
 *  			3 4 5  -->  1 4 7
 *  			6 7 8       0 3 6
 *
 *  			x,y -> y,(width-1-x)
 *
 ******************************************************************/
void rotate_i4_anti(unsigned int *data, int nx)
{
register unsigned int	*ptr1, *ptr2, *ptr3, *ptr4, temp;
register int            i, j;
int                     nx2 = (nx+1)/2;

        for ( i=nx/2; i--; ) {

                /* Set pointer addresses */

                j    = nx2 - 1;
                ptr1 = data + nx*i        + j;          /* 1. Quadrant */
                ptr2 = data + nx*j        + nx-1-i;     /* 2. Quadrant */
                ptr3 = data + nx*(nx-1-i) + nx-1-j;     /* 4. Quadrant */
                ptr4 = data + nx*(nx-1-j) + i;          /* 3. Quadrant */

                for ( j = nx2; j--; ) {

                        /* Restack: anticlockwise rotation by 90.0 */
                        temp  = *ptr1;
                        *ptr1 = *ptr2;
                        *ptr2 = *ptr3;
                        *ptr3 = *ptr4;
                        *ptr4 = temp;

                        /* Increase pointer */
                         ptr1 --;
                         ptr2 -= nx;
                         ptr3 ++;
                         ptr4 += nx;
                }
        }
}

/******************************************************************
 * Function: rotate_i2_anti = rotates image anticlockwise by 90. deg
 *
 *  			0 1 2       2 5 8
 *  			3 4 5  -->  1 4 7
 *  			6 7 8       0 3 6
 *
 *  			x,y -> y,(width-1-x)
 *
 ******************************************************************/
void rotate_i2_anti(unsigned short *data, int nx)
{
register unsigned short *ptr1, *ptr2, *ptr3, *ptr4, temp;
register int            i, j;
int                     nx2 = (nx+1)/2;

        for ( i=nx/2; i--; ) {

                /* Set pointer addresses */

                j    = nx2 - 1;
                ptr1 = data + nx*i        + j;          /* 1. Quadrant */
                ptr2 = data + nx*j        + nx-1-i;     /* 2. Quadrant */
                ptr3 = data + nx*(nx-1-i) + nx-1-j;     /* 4. Quadrant */
                ptr4 = data + nx*(nx-1-j) + i;          /* 3. Quadrant */

                for ( j = nx2; j--; ) {

                        /* Restack: anticlockwise rotation by 90.0 */
                        temp  = *ptr1;
                        *ptr1 = *ptr2;
                        *ptr2 = *ptr3;
                        *ptr3 = *ptr4;
                        *ptr4 = temp;

                        /* Increase pointer */
                         ptr1 --;
                         ptr2 -= nx;
                         ptr3 ++;
                         ptr4 += nx;
                }
        }
}

/******************************************************************
 * Function: rotate_ch_anti    = rotates image anticlockwise by 90. deg
 *
 *  			0 1 2       2 5 8
 *  			3 4 5  -->  1 4 7
 *  			6 7 8       0 3 6
 *
 *  			x,y -> y,(width-1-x)
 *
 ******************************************************************/
void rotate_ch_anti(unsigned char *data, int nx)
{
register unsigned char *ptr1, *ptr2, *ptr3, *ptr4, temp;
register int            i, j;
int                     nx2 = (nx+1)/2;

        for ( i=nx/2; i--; ) {

                /* Set pointer addresses */

                j    = nx2 - 1;
                ptr1 = data + nx*i        + j;          /* 1. Quadrant */
                ptr2 = data + nx*j        + nx-1-i;     /* 2. Quadrant */
                ptr3 = data + nx*(nx-1-i) + nx-1-j;     /* 4. Quadrant */
                ptr4 = data + nx*(nx-1-j) + i;          /* 3. Quadrant */

                for ( j = nx2; j--; ) {

                        /* Restack: anticlockwise rotation by 90.0 */
                        temp  = *ptr1;
                        *ptr1 = *ptr2;
                        *ptr2 = *ptr3;
                        *ptr3 = *ptr4;
                        *ptr4 = temp;

                        /* Increase pointer */
                         ptr1 --;
                         ptr2 -= nx;
                         ptr3 ++;
                         ptr4 += nx;
                }
        }
}

/******************************************************************
 * Function: rotate_i4 = rotates INT array clockwise by 90. deg
 *
 *  			0 1 2       2 5 8
 *  			3 4 5  -->  1 4 7
 *  			6 7 8       0 3 6
 *
 *  			x,y -> y,(width-1-x)
 *
 ******************************************************************/
void rotate_i4(unsigned int *data, int nx)
{
register unsigned int 	*ptr1, *ptr2, *ptr3, *ptr4, temp;
register int 		i, j;
int			nx2 = (nx+1)/2;

	for ( i=nx/2; i--; ) {

		/* Set pointer addresses */

		j    = nx2 - 1;
		ptr1 = data + nx*i        + j;		/* 1. Quadrant */
		ptr2 = data + nx*j        + nx-1-i;	/* 2. Quadrant */
		ptr3 = data + nx*(nx-1-i) + nx-1-j;	/* 4. Quadrant */
		ptr4 = data + nx*(nx-1-j) + i;		/* 3. Quadrant */

		for ( j = nx2; j--; ) {

                        /* Restack: clockwise rotation by 90.0 */
                        temp  = *ptr4;
                        *ptr4 = *ptr3;
                        *ptr3 = *ptr2;
                        *ptr2 = *ptr1;
                        *ptr1 = temp;

			/* Increase pointer */
			 ptr1 --;
			 ptr2 -= nx;
			 ptr3 ++;
			 ptr4 += nx;
		}
	}
}

/******************************************************************
 * Function: rotate_i2 = rotates image clockwise by 90. deg
 *
 *  			0 1 2       2 5 8
 *  			3 4 5  -->  1 4 7
 *  			6 7 8       0 3 6
 *
 *  			x,y -> y,(width-1-x)
 *
 ******************************************************************/
void rotate_i2(unsigned short *data, int nx)
{
register unsigned short *ptr1, *ptr2, *ptr3, *ptr4, temp;
register int            i, j;
int                     nx2 = (nx+1)/2;

        for ( i=nx/2; i--; ) {

                /* Set pointer addresses */

                j    = nx2 - 1;
                ptr1 = data + nx*i        + j;          /* 1. Quadrant */
                ptr2 = data + nx*j        + nx-1-i;     /* 2. Quadrant */
                ptr3 = data + nx*(nx-1-i) + nx-1-j;     /* 4. Quadrant */
                ptr4 = data + nx*(nx-1-j) + i;          /* 3. Quadrant */

                for ( j = nx2; j--; ) {

                        /* Restack: clockwise rotation by 90.0 */
                        temp  = *ptr4;
                        *ptr4 = *ptr3;
                        *ptr3 = *ptr2;
                        *ptr2 = *ptr1;
                        *ptr1 = temp;

                        /* Increase pointer */
                         ptr1 --;
                         ptr2 -= nx;
                         ptr3 ++;
                         ptr4 += nx;
                }
        }
}

/******************************************************************
 * Function: rotate_ch = rotates image clockwise by 90. deg
 *
 *  			0 1 2       2 5 8
 *  			3 4 5  -->  1 4 7
 *  			6 7 8       0 3 6
 *
 *  			x,y -> y,(width-1-x)
 *
 ******************************************************************/
void rotate_ch(unsigned char  *data, int nx)
{
register unsigned char  *ptr1, *ptr2, *ptr3, *ptr4, temp;
register int            i, j;
int                     nx2 = (nx+1)/2;

        for ( i=nx/2; i--; ) {

                /* Set pointer addresses */

                j    = nx2 - 1;
                ptr1 = data + nx*i        + j;          /* 1. Quadrant */
                ptr2 = data + nx*j        + nx-1-i;     /* 2. Quadrant */
                ptr3 = data + nx*(nx-1-i) + nx-1-j;     /* 4. Quadrant */
                ptr4 = data + nx*(nx-1-j) + i;          /* 3. Quadrant */

                for ( j = nx2; j--; ) {

                        /* Restack: clockwise rotation by 90.0 */
                        temp  = *ptr4;
                        *ptr4 = *ptr3;
                        *ptr3 = *ptr2;
                        *ptr2 = *ptr1;
                        *ptr1 = temp;

                        /* Increase pointer */
                         ptr1 --;
                         ptr2 -= nx;
                         ptr3 ++;
                         ptr4 += nx;
                }
        }
}

/******************************************************************
 * Function: text = puts message in text field of main window
 ******************************************************************/
void 
wtext(int type, char *mess)
{
extern FILE *fpout;
	if ( type == 1 ) {
		fprintf( stdout, "mar345 ERROR: %s\n", mess );
		fprintf(  fpout, "mar345 ERROR: %s\n", mess );
	}
	else {
		fprintf( stdout, "mar345 WARNING: %s\n", mess );
		fprintf(  fpout, "mar345 WARNING: %s\n", mess );
	}
}
