/***********************************************************************
 *
 * mar345_header.h
 *
 * Copyright by:        Dr. Claudio Klein
 *                      X-ray Research GmbH, Hamburg
 *
 * Version:     1.6
 * Date:        08/01/2002
 *
 * History:
 * Version    Date    	Changes
 * ______________________________________________________________________
 *
 * 1.7 	      02/09/02	h.detector introduced
 * 1.6	      08/01/02	date extended to 26 bytes
 * 1.5	      06/09/99  Element gap extended (8 values: gap 1-2 )
 * 1.4        14/05/98  Element gap introduced 
 *
 ***********************************************************************/

#define	N_GAPS		8

typedef struct {

	int	byteorder;		/* Always = 1234 */
        char    version[8];		/* Program version           */
        char    program[16];		/* Program name              */

	/* Scanner specific things */
	short	scanner;		/* Scanner serial no. */
	short	size;  			/* No. of pixels in 1 dimension */
	char	format;			/* Image format */
	char	mode;			/* Exposure mode */
	int	high;			/* No. high intensity pixels */
        int     pixels;			/* No. of pixels in image */
	int	adc_A;			/* Offset from channel A of ADC */
	int	adc_B;			/* Offset from channel B of ADC */
	int	add_A;			/* ADD to channel A of ADC */
	int	add_B;			/* ADD to channel B of ADC */
	int	gap[N_GAPS];		/* GAP1+2 position seen by controller */

        float	pixel_length;		/* Length of 1 pixel */
        float	pixel_height;		/* Height of 1 pixel */
        float   multiplier;     	/* Multiplication factor */
        float   xcen;			/* Center x of transf. image */
        float   ycen;			/* Center y of transf. image */
        float   roff;			/* Radial offset             */
        float   toff;			/* Tangential offset         */
        float   gain;			/* Gain of detector          */

	/* Experimental conditions for this image */
        float   time;			/* Exposure time in secs */
        float   dosebeg;		/* Dose at start of expose */
        float   doseend;		/* Dose at end   of expose */
        float   dosemin;		/* Min. dose during expose */
        float   dosemax;		/* Max. dose during expose */
        float   doseavg;		/* Avg. dose during expose */
        float   dosesig;		/* Sig. dose during expose */
        float   wave;  			/* Wavelength [Ang.] */
        float   dist;			/* Distance [mm] */
        float   resol;			/* Max. resolution */
        float   phibeg;			/* Starting PHI */
        float   phiend;			/* Ending   PHI */
        float   omebeg;			/* Starting Omega */
        float   omeend;			/* Ending   Omega */
        float   theta;			/* Two theta */
        float   chi;			/* Chi */
	int	phiosc;			/* Phi oscillations */
	int	omeosc;			/* Omega oscillations */
        int     dosen;  		/* No. of X-ray readings   */

	/* Generator settings */
	char	source[32];		/* Type of source */
        float   kV;  			/* Generator: kV */
        float   mA;  			/* Generator: mA */
	
	/* Monochromator */
	char	filter[32];		/* Type of monochromator */
        float   polar; 			/* Beam polarization factor */
        float   slitx; 			/* Slit width               */
        float   slity; 			/* Slit height              */

	/* Image statistics  */
	int	valmin;			/* Min. pixel value */
	int	valmax;			/* Max. pixel value */
	float	valavg;			/* Avg. pixel value */
	float	valsig;			/* Sig. pixel value */
	int	histbeg;		/* Start of histogram */
	int	histend;		/* End   of histogram */
	int	histmax;		/* Max.  of histogram */

	/* Remark             */
	char	remark[56];		/* Remark */

	/* Detector           */
	char	detector[32];		/* Detector */

	/* Time of production */
	char	date[26];		/* Creation date */

} MAR345_HEADER;

