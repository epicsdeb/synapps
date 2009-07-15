/***********************************************************************
 *
 * nb_header.h
 *
 * Copyright by:        Dr. Claudio Klein
 *                      X-ray Research GmbH, Hamburg
 *
 * Version:     1.0
 * Date:        05/07/1996
 *
 ***********************************************************************/

#define MAX_NBMODE	4

typedef struct {

	int	byteorder;		/* Always 1234 */

	short	scanner;		/* Scanner serial no. */
	short	mode;   		/* No. of scan modes  */
	short	size  [MAX_NBMODE];  	/* No. of pixels in 1 dimension */
	short	x     [MAX_NBMODE];  	/* X-coordinate of 1. pixel */
	short	y     [MAX_NBMODE];  	/* Y-coordinate of 1. pixel */
	short	skip  [MAX_NBMODE];  	/* offset of scan           */
	short	roff  [MAX_NBMODE];  	/* offset of scan           */
	int  	fpos  [MAX_NBMODE];  	/* File position at 1. pixel */
	int  	pixels[MAX_NBMODE];  	/* Current pixel in mode */

        int     subpixels   ;		/* No. of subpixels/pixel */
        int     tot_pixels  ;		/* No. of pixels total */
        int     nbs         ;		/* No. of neighbours total */
        float	pixel_length;		/* Length of 1 pixel */
        float	pixel_height;		/* Height of 1 pixel */

        float   scale;     		/* Contrib. scale factor */
        float   phioff;			/* PHI offset                */
        float   cutoff;			/* NB  offset                */
        float   gain  ;			/* Gain of detector          */

	/* Program of production */
	char	program[16];		/* Program       */
        char    version[8];   		/* Program version */

	/* Time of production */
	char	date[24];		/* Creation date */

} MARNB_HEADER;

