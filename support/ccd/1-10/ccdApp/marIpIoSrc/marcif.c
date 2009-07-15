/**********************************************************************
 *
 * marcif.c
 *
 * Version:     2.1
 * Date:        02/09/2002
 *
 * Version	Date		Description
 * 2.1		02/09/2002	Handle h345.detector
 * 2.0		14/12/2001	Added dtb motors: dtb2CIFHeader
 * 1.0		30/10/2000	Added CBF/imgCIF support
 *
 ***********************************************************************/

/*
 * System includes
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>

/*
 * Local includes
 */

#include "marcif_header.h"
#include "mar345_header.h"

#include <cbf.h>
#include <img.h>

#define printe(x) 	printf("%s: %s: %s\n",prg,f,x)
#undef cbf_failnez
#define cbf_failnez(x) \
{	int err; \
  	cif_error = 0; \
  	err = (x); \
  	if (err) { \
		if ( verbose > 1 ) \
    		fprintf(stdout,"CBFlib error %d\n",err); \
    		cif_error=err; \
  	} \
}


/*
 * External variables 
 */

 int			verbose;

/*
 * Local variables
 */
static char			PRINT_AXIS	= 0;
static char			PRINT_CHEMICAL	= 0;

static int			cif_error	= 0;
static CIF_HEADER		hcif;

cbf_handle		cif			= NULL;
static size_t        	cif_read		= 0;
static int              cif_id                  = 0;
static int              cif_element_signed      = 0;
static int              cif_element_unsigned    = 0;
static int              cif_minelement          = 0;
static int              cif_maxelement          = 0;
static unsigned int     cif_compression         = 0;
static size_t           cif_nelements           = 0;
static size_t           cif_element_size        = 0;

static char	*axis_name[] 	= {"phi", 		"omega", 
				   "chi", 		"2-theta",
				   "distance", 		
				   "slit_1_hor", 	"slit_1_ver",
				   "slit_2_hor", 	"slit_2_ver",
				   "beamstop",		"z-axis",
				   "trans_hor",  	"trans_ver",
				   "rot_hor",    	"rot_ver",
				   "" };
/*
 * External functions
 */

/*
 * Local functions
 */
int 			GetCIFHeader	(char *, char *, FILE *);
int 			GetCIFData  	(char *, char *, FILE *, unsigned int *);
int 			PutCIFData  	(char *, char *, char  , unsigned int *);
int 			PutCIFHeader	(char *, char *);
MAR345_HEADER		CIF2mar345Header(char *);
void 			mar3452CIFHeader(char *, char *, MAR345_HEADER );
void 			dtb2CIFHeader	(char *, char *, float *);

/******************************************************************
 * Function: GetCIFHeader
 ******************************************************************/
int GetCIFHeader( char *prg, char *f, FILE *fp)
{
const char	*sval;
float		fval;
double		dval;
int		ival;
int		i;
char		str[64];
char		axis[16] 	= {"\0"};

	rewind( fp );

	/* Initialize CIF Header */
	memset( (char *)&hcif, 0, sizeof(CIF_HEADER) );

	cbf_failnez (cbf_make_handle 		(&cif))
	if ( cif_error ) {
		printe("Cannot create handle for CIF input");
		return 0;
	}
	cbf_failnez (cbf_read_file 		(cif, fp, MSG_DIGEST))
	cbf_failnez (cbf_rewind_datablock	(cif))

	/************************************************************ 
	 * Category MARIP
	 ************************************************************/
	cbf_failnez (cbf_find_category    	(cif, "diffrn_detector_mar"))
	if ( cif_error ) {
		printe("Cannot find category: diffrn_detector_mar");
		goto CIF_MARIP_GAPS;
	}
	cbf_failnez (cbf_find_column      	(cif, "id"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_value      (cif, &sval))
		strcpy( hcif.mar_id, 		sval);
	}

	/************************************************************ 
	 * Category MARIP_GAPS
	 ************************************************************/
CIF_MARIP_GAPS:
	cbf_failnez (cbf_rewind_datablock	(cif))
	cbf_failnez (cbf_find_category    	(cif, "diffrn_detector_mar_gaps"))
	if ( !cif_error ) {
	    for (i=0;i<8;i++) {
		sprintf(str, "gap[%d]",i+1);
		cbf_failnez (cbf_find_column     (cif, str))
		if ( !cif_error ) {
			cbf_failnez (cbf_get_integervalue  (cif, &ival))
			hcif.diffrn_detector_mar_gaps.gap[i] = ival;
		}
		cbf_failnez (cbf_rewind_row       	(cif))
	    }
	}

	/************************************************************ 
	 * Category MARIP_EXPOSURE
	 ************************************************************/
CIF_MARIP_EXPOSURE:
	cbf_failnez (cbf_rewind_datablock	(cif))
	cbf_failnez (cbf_find_category    	(cif, "diffrn_detector_mar_exposure"))
	if ( cif_error ) goto CIF_MARIP_AXIS;

	cbf_failnez (cbf_find_column      	(cif, "type"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_value        (cif, &sval))
		strcpy( hcif.diffrn_detector_mar_exposure.type, sval);
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "time"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_doublevalue  (cif, &dval))
		hcif.diffrn_detector_mar_exposure.time 	= (float)dval;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "dose"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_doublevalue  (cif, &dval))
		hcif.diffrn_detector_mar_exposure.dose 	= (float)dval;
	}

	/************************************************************ 
	 * Category MARIP_AXIS
	 ************************************************************/
CIF_MARIP_AXIS:
	cbf_failnez (cbf_rewind_datablock	(cif))
	cbf_failnez (cbf_find_category    	(cif, "diffrn_detector_mar_axis"))
	if ( cif_error ) goto CIF_MARIP_EXPOSURE_DOSE;

	cbf_failnez (cbf_find_column      	(cif, "mar_id"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_value        (cif, &sval))
		strcpy( hcif.mar_id, sval);
	}

	for ( i=0; i<=AXIS_DISTANCE; i++ ) {	
	  while (cbf_find_nextrow (cif, hcif.mar_id) == 0) {
	    cbf_failnez (cbf_find_column      	(cif, "axis_id"))
	    if ( cif_error ) break;
	    cbf_failnez (cbf_get_value 	      	(cif, &sval))
	    strcpy( axis, axis_name[i] );
	    if ( !strcmp( sval, axis ) ) {
		cbf_failnez (cbf_find_column  	(cif, "angle_start"))
		if ( !cif_error ) {
			cbf_failnez (cbf_get_doublevalue (cif, &dval))
			hcif.diffrn_detector_mar_axis[i].angle_start=(float)dval;
		}
		cbf_failnez (cbf_find_column  (cif, "angle_end"))
		if ( !cif_error ) {
			cbf_failnez (cbf_get_doublevalue (cif, &dval))
			hcif.diffrn_detector_mar_axis[i].angle_end=(float)dval;
		}
		cbf_failnez (cbf_find_column  (cif, "angle_increment"))
		if ( !cif_error ) {
			cbf_failnez (cbf_get_doublevalue (cif, &dval))
			hcif.diffrn_detector_mar_axis[i].angle_increment=(float)dval;
		}
		cbf_failnez (cbf_find_column  (cif, "oscillations"))
		if ( !cif_error ) {
			cbf_failnez (cbf_get_integervalue (cif, &ival))
			hcif.diffrn_detector_mar_axis[i].oscillations = ival;
		}
		cbf_failnez (cbf_find_column  (cif, "displacement"))
		if ( !cif_error ) {
			cbf_failnez (cbf_get_doublevalue (cif, &dval))
			hcif.diffrn_detector_mar_axis[i].displacement=(float)dval;
		}
	    }
	    cbf_failnez (cbf_find_column 	(cif, "mar_id"))
	  }
	  cbf_failnez (cbf_rewind_row       	(cif))
	}

	/************************************************************ 
	 * Category MARIP_EXPOSURE_DOSE
	 ************************************************************/

CIF_MARIP_EXPOSURE_DOSE:
	cbf_failnez (cbf_rewind_datablock	(cif))
	cbf_failnez (cbf_find_category    	(cif, "diffrn_detector_mar_exposure_dose"))
	if ( cif_error ) goto CIF_MARIP_SCAN;
	cbf_failnez (cbf_find_column      	(cif, "start"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_doublevalue  (cif, &dval))
		hcif.diffrn_detector_mar_exposure_dose.start = (float)dval;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "end"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_doublevalue(cif, &dval))
		hcif.diffrn_detector_mar_exposure_dose.end = (float)dval;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "min"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_doublevalue(cif, &dval))
		hcif.diffrn_detector_mar_exposure_dose.min = (float)dval;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "max"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_doublevalue(cif, &dval))
		hcif.diffrn_detector_mar_exposure_dose.max= (float)dval;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "avg"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_doublevalue(cif, &dval))
		hcif.diffrn_detector_mar_exposure_dose.avg = (float)dval;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "esd"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_doublevalue(cif, &dval))
		hcif.diffrn_detector_mar_exposure_dose.esd = (float)dval;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "measurements"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_integervalue (cif, &ival))
		hcif.diffrn_detector_mar_exposure_dose.measurements = ival;
	}

	/************************************************************ 
	 * Category MARIP_EXPOSURE_SCAN
	 ************************************************************/
CIF_MARIP_SCAN:
	cbf_failnez (cbf_rewind_datablock	(cif))
	if ( cif_error ) goto CIF_MARIP_ADC;
	cbf_failnez (cbf_find_category    	(cif, "diffrn_detector_mar_scan"))
	cbf_failnez (cbf_find_column      	(cif, "scanmode"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_integervalue (cif, &ival))
		hcif.diffrn_detector_mar_scan.scanmode	= ival;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "number_of_pixels"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_integervalue (cif, &ival))
		hcif.diffrn_detector_mar_scan.number_of_pixels = ival;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "number_of_pixels_larger_16_bit"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_integervalue (cif, &ival))
		hcif.diffrn_detector_mar_scan.number_of_pixels_larger_16_bit  = ival;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "pixel_height"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_doublevalue (cif, &dval))
		hcif.diffrn_detector_mar_scan.pixel_height	= (float)dval;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "pixel_length"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_doublevalue (cif, &dval))
		hcif.diffrn_detector_mar_scan.pixel_length	= (float)dval;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "radial_offset"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_doublevalue (cif, &dval))
		hcif.diffrn_detector_mar_scan.radial_offset	= (float)dval;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "tangential_offset"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_doublevalue (cif, &dval))
		hcif.diffrn_detector_mar_scan.tangential_offset= (float)dval;
	}

	/************************************************************ 
	 * Category MARIP_ADC
	 ************************************************************/
CIF_MARIP_ADC:
	cbf_failnez (cbf_rewind_datablock	(cif))
	cbf_failnez (cbf_find_category    	(cif, "diffrn_detector_mar_adc"))
	if ( cif_error ) goto CIF_MARIP_DATA;
	cbf_failnez (cbf_find_column      	(cif, "channel_a"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_integervalue (cif, &ival))
		hcif.diffrn_detector_mar_adc.channel_a 	= ival;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "channel_b"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_integervalue (cif, &ival))
		hcif.diffrn_detector_mar_adc.channel_b 	= ival;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "add_channel_a"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_integervalue (cif, &ival))
		hcif.diffrn_detector_mar_adc.add_channel_a 	= ival;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "add_channel_b"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_integervalue (cif, &ival))
		hcif.diffrn_detector_mar_adc.add_channel_b 	= ival;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "oversampling"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_integervalue (cif, &ival))
		hcif.diffrn_detector_mar_adc.oversampling 	= ival;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "multiplier"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_doublevalue (cif, &dval))
		hcif.diffrn_detector_mar_adc.multiplier 	= (float)dval;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "gain"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_doublevalue (cif, &dval))
		hcif.diffrn_detector_mar_adc.gain 		= (float)dval;
	}

	/************************************************************ 
	 * Category MARIP_DATA
	 ************************************************************/
CIF_MARIP_DATA:
	cbf_failnez (cbf_rewind_datablock	(cif))
	cbf_failnez (cbf_find_category    	(cif, "diffrn_detector_mar_data"))
	if ( cif_error ) goto CIF_DIFFRN_FRAME_DATA;
	cbf_failnez (cbf_find_column      	(cif, "intensity_avg"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_doublevalue (cif, &dval))
		hcif.diffrn_detector_mar_data.intensity_avg = (float)dval;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "intensity_esd"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_doublevalue (cif, &dval))
		hcif.diffrn_detector_mar_data.intensity_esd = (float)dval;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "intensity_min"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_integervalue (cif, &ival))
		hcif.diffrn_detector_mar_data.intensity_min = ival;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "intensity_max"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_integervalue (cif, &ival))
		hcif.diffrn_detector_mar_data.intensity_max = ival;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "histogram_start"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_integervalue (cif, &ival))
		hcif.diffrn_detector_mar_data.histogram_start = ival;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "histogram_end"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_integervalue (cif, &ival))
		hcif.diffrn_detector_mar_data.histogram_end = ival;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "histogram_max"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_integervalue (cif, &ival))
		hcif.diffrn_detector_mar_data.histogram_max = ival;
	}

	/************************************************************ 
	 * Category DIFFRN_FRAME_DATA 
	 ************************************************************/
CIF_DIFFRN_FRAME_DATA:
	cbf_failnez (cbf_rewind_datablock	(cif))
	cbf_failnez (cbf_find_category    	(cif, "diffrn_frame_data"))
	if ( cif_error ) {
		printe("Cannot find category: diffrn_frame_data ");
		goto CIF_DIFFRN_DETECTOR_ELEMENT;
	}
	cbf_failnez (cbf_find_column      	(cif, "array_id"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_value      (cif, &sval))
		strcpy( hcif.array_id, 		sval);
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "id"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_value      (cif, &sval))
		strcpy( hcif.data_id, 		sval);
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "detector_element_id"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_value     (cif, &sval))
		strcpy( hcif.element_id, 	sval);
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "binary_id"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_integervalue (cif, &hcif.binary_id))
	}

	/************************************************************ 
	 * Category DIFFRN_DETECTOR_ELEMENT
	 ************************************************************/
CIF_DIFFRN_DETECTOR_ELEMENT:
	cbf_failnez (cbf_rewind_datablock	(cif))
	cbf_failnez (cbf_find_category    	(cif, "diffrn_detector_element"))
	if ( cif_error )goto CIF_ARRAY_STRUCTURE_LIST;
		
	cbf_failnez (cbf_find_column      	(cif, "center[1]"))
	if ( !cif_error ) {
	    cbf_failnez (cbf_get_doublevalue  	(cif, &dval))
	    hcif.diffrn_detector_element.center_x	= (float)dval;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "center[2]"))
	if ( !cif_error ) {
	    cbf_failnez (cbf_get_doublevalue  	(cif, &dval))
	    hcif.diffrn_detector_element.center_y	= (float)dval;
	}

	/************************************************************ 
	 * Category ARRAY_STRUCTURE_LIST
	 ************************************************************/
CIF_ARRAY_STRUCTURE_LIST:
	/* Get the image dimensions (2nd = fast, 1st = slow) */
	cbf_failnez (cbf_rewind_datablock	(cif))
	cbf_failnez (cbf_find_category    	(cif, "array_structure_list"))
	if ( cif_error ) goto CIF_ARRAY_ELEMENT_SIZE;
	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "array_id"))

	i=0;	
	while (cbf_find_nextrow (cif, hcif.array_id) == 0) {
	    cbf_failnez (cbf_find_column      	(cif, "precedence"))
	    cbf_failnez (cbf_get_integervalue 	(cif, &hcif.array_structure_list[i].precedence))

	    if ( hcif.array_structure_list[i].precedence >= 1 && 
		 hcif.array_structure_list[i].precedence <= 2) {
		cbf_failnez (cbf_find_column 	(cif, "dimension"))
		cbf_failnez (cbf_get_integervalue (cif, 
			&hcif.array_structure_list[2-hcif.array_structure_list[i].precedence].dimension))
	    }
	    else {
		printe("Cannot find CIF dimensions");
		return 0;
	    }

	    i++;
	    if ( i == 2 ) break;
	    cbf_failnez (cbf_find_column 	(cif, "array_id"))
	}

	if ( hcif.array_structure_list[0].dimension*hcif.array_structure_list[1].dimension == 0 ) {
		printe("Something wrong with CIF dimensions");
		return 0;
	}

	/************************************************************ 
	 * Category ARRAY_ELEMENT_SIZE
	 ************************************************************/
CIF_ARRAY_ELEMENT_SIZE:
	cbf_failnez (cbf_rewind_datablock(cif))
	cbf_failnez (cbf_find_category   	(cif, "array_element_size"))
	
	if ( cif_error ) {
		printe("Cannot find category: array_element_size");
		goto CIF_DIFFRN_RADIATION;
	}
	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "array_id"))

	i = 0;	
	while (cbf_find_nextrow (cif, hcif.array_id) == 0) {
	    cbf_failnez (cbf_find_column      	(cif, "size"))
	    cbf_failnez (cbf_get_doublevalue  	(cif, &dval))
	    if ( !cif_error ) 
		    hcif.array_element_size.pixelsize[i] = (float)dval;
	    cbf_failnez (cbf_find_column (cif, "array_id"))
	    i++;
	    if ( i == 2 ) break;
	}

	/************************************************************ 
	 * Category DIFFRN_RADIATION
	 ************************************************************/
CIF_DIFFRN_RADIATION:	
	cbf_failnez (cbf_rewind_datablock(cif))
	cbf_failnez (cbf_find_category    	(cif, "diffrn_radiation"))
	if ( cif_error ) goto CIF_DIFFRN_RADIATION_WAVELENGTH;

	cbf_failnez (cbf_find_column      	(cif, "monochromator"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_value     (cif, &sval))
		strcpy( hcif.diffrn_radiation.monochromator, sval);
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "wavelength"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_doublevalue (cif, &dval))
		hcif.diffrn_radiation_wavelength.wavelength = (float)dval;
	}

	/************************************************************ 
	 * Category DIFFRN_RADIATION_WAVELENGTH
	 ************************************************************/
CIF_DIFFRN_RADIATION_WAVELENGTH:	
	cbf_failnez (cbf_rewind_datablock(cif))
	cbf_failnez (cbf_find_category    	(cif, "diffrn_radiation_wavelength"))
	if ( cif_error ) goto CIF_DIFFRN_MEASUREMENT;
	cbf_failnez (cbf_find_column      	(cif, "wavelength"))
	if ( cif_error ) {
			printe("Cannot find data item: wavelength");
			goto CIF_DIFFRN_MEASUREMENT;
	} 
	else {
		cbf_failnez (cbf_get_doublevalue (cif, &dval))
		hcif.diffrn_radiation_wavelength.wavelength = (float)dval;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "polarisn_ratio"))
	if ( !cif_error ) {
		hcif.diffrn_radiation_wavelength.polarisn_ratio = (float)dval;
	}

	/************************************************************ 
	 * Category DIFFRN_MEASUREMENT
	 ************************************************************/
CIF_DIFFRN_MEASUREMENT:
	cbf_failnez (cbf_rewind_datablock(cif))
	cbf_failnez (cbf_find_category    	(cif, "diffrn_measurement"))
	if ( cif_error ) {
		printe("Cannot find category: diffrn_measurement");
		goto CIF_DIFFRN_SOURCE;
	}
	cbf_failnez (cbf_find_column      	(cif, "sample_detector_distance"))
	if ( cif_error == 0 ) {
		cbf_failnez (cbf_get_doublevalue (cif, &dval))
		hcif.diffrn_measurement.sample_detector_distance = (float)dval;
	}

	/************************************************************ 
	 * Category DIFFRN_SOURCE
	 ************************************************************/
CIF_DIFFRN_SOURCE:
	cbf_failnez (cbf_rewind_datablock(cif))
	cbf_failnez (cbf_find_category    	(cif, "diffrn_source"))
	if ( cif_error ) goto CIF_DIFFRN_SCAN_FRAME_AXIS;
	cbf_failnez (cbf_find_column      	(cif, "source"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_value 	(cif, &sval))
		strcpy( hcif.diffrn_source.source, sval);
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "type"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_value 	(cif, &sval))
		strcpy( hcif.diffrn_source.type, sval);
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "current"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_doublevalue (cif, &dval))
		hcif.diffrn_source.current	= (float)dval;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "voltage"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_doublevalue (cif, &dval))
		hcif.diffrn_source.voltage	= (float)dval;
	}

	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "power"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_doublevalue (cif, &dval))
		hcif.diffrn_source.power  	= (float)dval;
	}

	/************************************************************ 
	 * Category DIFFRN_SCAN_FRAME_AXIS
	 ************************************************************/
CIF_DIFFRN_SCAN_FRAME_AXIS:
	cbf_failnez (cbf_rewind_datablock(cif))
	cbf_failnez (cbf_find_category    	(cif, "diffrn_scan_frame_axis"))
	if ( cif_error ) {
		printe("Cannot find category: diffrn_scan_frame_axis");
		goto CIF_DIFFRN_SCAN;
	}
	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "scan_id"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_value 	(cif, &sval))
		strcpy( hcif.scan_id,		sval);
	}

	for ( i=0; i<N_SCAN_FRAME_AXES; i++ ) {	
	  while (cbf_find_nextrow (cif, hcif.scan_id) == 0) {
	    cbf_failnez (cbf_find_column      	(cif, "axis_id"))
	    if ( cif_error ) break;
	    cbf_failnez (cbf_get_value 	      	(cif, &sval))
	    strcpy( axis, axis_name[i] );
	    if ( !strcmp( sval, axis ) ) {
		cbf_failnez (cbf_find_column  	(cif, "angle"))
		if ( !cif_error ) {
			cbf_failnez (cbf_get_doublevalue (cif, &dval))
			hcif.diffrn_scan_frame_axis[i].angle=(float)dval;
		}
		cbf_failnez (cbf_find_column  (cif, "displacement"))
		if ( !cif_error ) {
			cbf_failnez (cbf_get_doublevalue (cif, &dval))
			hcif.diffrn_scan_frame_axis[i].displacement=(float)dval; 
		}
	    }
	    cbf_failnez (cbf_find_column 	(cif, "scan_id"))
	  }
	  cbf_failnez (cbf_rewind_row       	(cif))
	}

	/************************************************************ 
	 * Category DIFFRN_SCAN + DIFFRN+SCAN_AXIS
	 ************************************************************/
CIF_DIFFRN_SCAN:
	cbf_failnez (cbf_rewind_datablock(cif))
	cbf_failnez (cbf_find_category    	(cif, "diffrn_scan_axis"))
	if ( cif_error ) {
		printe("Cannot find category: diffrn_scan_axis");
		goto CIF_CAT4;
	}
	cbf_failnez (cbf_rewind_row       	(cif))
	cbf_failnez (cbf_find_column      	(cif, "scan_id"))
	if ( !cif_error ) {
		cbf_failnez (cbf_get_value 	  	(cif, &sval))
		strcpy( hcif.scan_id, sval);
	}

	for ( i=0; i<N_SCAN_AXES; i++ ) {	
	  while (cbf_find_nextrow (cif, hcif.scan_id) == 0) {
	    cbf_failnez (cbf_find_column      	(cif, "axis_id"))
	    if ( cif_error ) break;
	    cbf_failnez (cbf_get_value 	      	(cif, &sval))
	    strcpy( axis, axis_name[i] );
	    if ( !strcmp( sval, axis ) ) {
		cbf_failnez (cbf_find_column  	(cif, "angle_start"))
		if ( !cif_error ) {
			cbf_failnez (cbf_get_doublevalue (cif, &dval))
			hcif.diffrn_scan_axis[i].angle_start=(float)dval;
		}
		cbf_failnez (cbf_find_column  	(cif, "angle_increment"))
		if ( !cif_error ) {
			cbf_failnez (cbf_get_doublevalue (cif, &dval))
			hcif.diffrn_scan_axis[i].angle_increment=(float)dval; 
		}
	    }
	    cbf_failnez (cbf_find_column 	(cif, "scan_id"))
	  }
	  cbf_failnez (cbf_rewind_row       	(cif))
	}

	/************************************************************ 
	 * Category ARRAY_DATA
	 ************************************************************/
CIF_CAT4:

CIF_END:

	if ( verbose > 1 ) {
	    printf("%s: CIF-Header values:\n",prg);
	    printf("         Pixels       :\t %d x %d\n",hcif.array_structure_list[0].dimension,hcif.array_structure_list[1].dimension);
	    printf("         Pixelsize    :\t %11.6f  [m]\n",hcif.array_element_size.pixelsize[0]);

	    if ( strlen( hcif.mar_id ) ) {
		printf("         Distance     :\t %11.3f  [mm ]\n",hcif.diffrn_detector_mar_axis[AXIS_DISTANCE].displacement);
		printf("         PHI-start    :\t %11.3f  [deg]\n",hcif.diffrn_detector_mar_axis[AXIS_PHI].angle_start);
		printf("         Delta-PHI    :\t %11.3f  [deg]\n",hcif.diffrn_detector_mar_axis[AXIS_PHI].angle_increment);
	    }
	    else {
		printf("         Distance     :\t %11.6f  [m]\n",hcif.diffrn_scan_frame_axis[AXIS_DISTANCE].displacement);
		printf("         Wavelength   :\t %11.6f  [Ang]\n",hcif.diffrn_radiation_wavelength.wavelength);
		printf("         PHI-start    :\t %11.3f  [deg]\n",hcif.diffrn_scan_axis[AXIS_PHI].angle_start);
		printf("         Delta-PHI    :\t %11.3f  [deg]\n",hcif.diffrn_scan_axis[AXIS_PHI].angle_increment);
	    }
	    printf("         Wavelength   :\t %11.6f  [Ang]\n",hcif.diffrn_radiation_wavelength.wavelength);
	}

	return 1;
}

/******************************************************************
 * Function: CIF2mar345Header
 ******************************************************************/
MAR345_HEADER CIF2mar345Header(char *prg)
{
char		*c,*d, *sval;
int		i,j,k;
MAR345_HEADER	h345;

	/* Defaults ... */
	memset( (char *)&h345, 0, sizeof(MAR345_HEADER) );

	h345.wave  	= hcif.diffrn_radiation_wavelength.wavelength;
	h345.byteorder 	= 1234;
	h345.format 	= 1;

	/* Center of detector ... */
	h345.xcen	= hcif.diffrn_detector_element.center_x;
	h345.ycen	= hcif.diffrn_detector_element.center_y;

	/* Serial number may be stored in element_id ... */
	if ( (sval=strchr( hcif.element_id, '_' )) != NULL ) {
		i=sscanf( sval+1, "%d", &k);
		if ( i==1 )
			h345.scanner = k;
	}

	/* Has this CBF been created by mar software ? ... */
	if ( strlen( hcif.mar_id ) ) {
		/* Axis setting */
		h345.phibeg 	= hcif.diffrn_detector_mar_axis[AXIS_PHI].angle_start;
		h345.phiend 	= hcif.diffrn_detector_mar_axis[AXIS_PHI].angle_end;
		h345.phiosc 	= hcif.diffrn_detector_mar_axis[AXIS_PHI].oscillations;

		h345.omebeg 	= hcif.diffrn_detector_mar_axis[AXIS_OMEGA].angle_start;
		h345.omeend 	= hcif.diffrn_detector_mar_axis[AXIS_OMEGA].angle_end;
		h345.omeosc 	= hcif.diffrn_detector_mar_axis[AXIS_OMEGA].oscillations;

		h345.dist  	= hcif.diffrn_detector_mar_axis[AXIS_DISTANCE].displacement;
		h345.theta 	= hcif.diffrn_detector_mar_axis[AXIS_THETA].angle_end;
		h345.chi   	= hcif.diffrn_detector_mar_axis[AXIS_CHI].angle_end;

		h345.time	= hcif.diffrn_detector_mar_exposure.time;

		/* Internals ... */
		h345.multiplier	= hcif.diffrn_detector_mar_adc.multiplier;
		h345.gain	= hcif.diffrn_detector_mar_adc.gain;
		h345.adc_A	= hcif.diffrn_detector_mar_adc.channel_a;
		h345.adc_B	= hcif.diffrn_detector_mar_adc.channel_b;
		h345.add_A	= hcif.diffrn_detector_mar_adc.add_channel_a;
		h345.add_B	= hcif.diffrn_detector_mar_adc.add_channel_b;

		h345.roff  	= hcif.diffrn_detector_mar_scan.radial_offset;
		h345.toff  	= hcif.diffrn_detector_mar_scan.tangential_offset;

		/* Image descriptors */
		h345.pixel_height = hcif.diffrn_detector_mar_scan.pixel_height;
		h345.pixel_length = hcif.diffrn_detector_mar_scan.pixel_length;

		h345.size 	= hcif.diffrn_detector_mar_scan.scanmode;
		h345.pixels 	= hcif.diffrn_detector_mar_scan.number_of_pixels;
		h345.high	= hcif.diffrn_detector_mar_scan.number_of_pixels_larger_16_bit;

		/* Exposure dose statistics  */
		h345.dosebeg    = hcif.diffrn_detector_mar_exposure_dose.start;
		h345.doseend    = hcif.diffrn_detector_mar_exposure_dose.end;
		h345.dosemin    = hcif.diffrn_detector_mar_exposure_dose.min;
		h345.dosemax    = hcif.diffrn_detector_mar_exposure_dose.max;
		h345.doseavg    = hcif.diffrn_detector_mar_exposure_dose.avg;
		h345.dosesig    = hcif.diffrn_detector_mar_exposure_dose.esd;
		h345.dosen      = hcif.diffrn_detector_mar_exposure_dose.measurements;

		/* Image statistics  */
		h345.valmin	= hcif.diffrn_detector_mar_data.intensity_min;
		h345.valmax	= hcif.diffrn_detector_mar_data.intensity_max;
		h345.valavg	= hcif.diffrn_detector_mar_data.intensity_avg;
		h345.valsig	= hcif.diffrn_detector_mar_data.intensity_esd;
		h345.histbeg	= hcif.diffrn_detector_mar_data.histogram_start;
		h345.histend	= hcif.diffrn_detector_mar_data.histogram_end;
		h345.histmax	= hcif.diffrn_detector_mar_data.histogram_max;

		/* Gaps */
		for (i=0;i<8;i++) {
			h345.gap[i] = hcif.diffrn_detector_mar_gaps.gap[i];
		}

		/* Type of exposure: DOSE or TIME */	
		if ( strstr( hcif.diffrn_detector_mar_exposure.type, "dose" ) ) 
			h345.mode = 0;
		else
			h345.mode = 1;

		/* Serial number is stored in diffrn_detector_mar_id ... */
		if ( (sval=strchr( hcif.mar_id, '_' )) != NULL ) {
			i=sscanf( sval+1, "%d", &k);
			if ( i==1 )
				h345.scanner = k;
		}

	}

	/* So this CBF has not been created by mar software ... */
	/* Some parameters may have been guessed ...*/
	else {
		/* Axis setting */
		h345.phibeg 	= hcif.diffrn_scan_axis[AXIS_PHI].angle_start;
		h345.phiend 	= h345.phibeg + hcif.diffrn_scan_axis[AXIS_PHI].angle_increment;
		h345.phiosc 	= 1;

		h345.omebeg 	= hcif.diffrn_scan_axis[AXIS_OMEGA].angle_start;
		h345.omeend 	= h345.omebeg + hcif.diffrn_scan_axis[AXIS_OMEGA].angle_increment;
		h345.omeosc 	= 0;

		/* Distance is in m, so divide by 1000 to get mm */
		h345.dist  	= hcif.diffrn_scan_frame_axis[AXIS_DISTANCE].displacement;
		h345.theta 	= hcif.diffrn_scan_frame_axis[AXIS_THETA].angle;
		h345.chi   	= hcif.diffrn_scan_frame_axis[AXIS_CHI].angle;
		
		/* Image descriptors */
		h345.pixel_height = hcif.array_element_size.pixelsize[0];
		h345.pixel_length = hcif.array_element_size.pixelsize[1];

		h345.size 	= hcif.array_structure_list[0].dimension;
		h345.pixels 	= h345.size*h345.size;
		h345.multiplier	= hcif.array_intensities.scaling;
		h345.gain	= hcif.array_intensities.gain;
		h345.mode	= 1;	/* Time mode = default */

		/* Serial number is stored in element_id ... */
		if ( (sval=strchr( hcif.element_id, '_' )) != NULL ) {
			i=sscanf( sval+1, "%d", &k);
			if ( i==1 )
				h345.scanner = k;
		}

	}

	/* Make sure pixesize is in microm */
	i=0;
	while (h345.pixel_height < 1.f) {	/* Pixelsize in micron */
		h345.pixel_height *= 1000.;
		h345.pixel_length *= 1000.;
		i++;
		if  (i==6) break;
	}

	/* Convert distance from m to mm */
	if ( h345.dist < 0.1 ) h345.dist *= 1000.;

	/* Convert IP-center from mm to pixels */
	if ( h345.pixel_height > 0.01 ) {
		h345.xcen /= ( h345.pixel_height/1000. ); 
		h345.ycen /= ( h345.pixel_height/1000. ); 
	}

	/* Generator setup */
	strcpy( h345.source, 	hcif.diffrn_source.source);
	h345.kV			= hcif.diffrn_source.voltage;
	h345.mA			= hcif.diffrn_source.current;

	/* Beam setup */
	strcpy( h345.filter, 	hcif.diffrn_radiation.monochromator);
	h345.polar		= hcif.diffrn_radiation_wavelength.polarisn_ratio;

	strcpy( h345.date,	hcif.audit.creation_date);

	return h345;	
}

/******************************************************************
 * Function: PutCIFHeader
 ******************************************************************/
int PutCIFHeader( char *prg, char *f)
{
const char	*sval;
float		fval;
double		dval;
int		ival;
int		i,j;


	cbf_failnez (cbf_make_handle (&cif))
	
	/* Make a new data block */
	cbf_failnez (cbf_new_datablock (cif, "image_1"))

	/********************************************************************
	 * AUDIT
	 ********************************************************************/

	cbf_failnez (cbf_force_new_category (cif, "audit"))
	cbf_failnez (cbf_new_column   (cif, "creation_date"))
	cbf_failnez (cbf_set_value    (cif, hcif.audit.creation_date))
	cbf_failnez (cbf_new_column   (cif, "creation_method"))
	cbf_failnez (cbf_set_value    (cif, hcif.audit.creation_method))

	/********************************************************************
	 * ENTRY, EXPTL_CRYSTAL & CHEMICAL
	 ********************************************************************/

	cbf_failnez (cbf_force_new_category (cif, "entry_id"))
	cbf_failnez (cbf_new_column   (cif, "id"))
	cbf_failnez (cbf_set_value    (cif, "img1"))

	cbf_failnez (cbf_force_new_category (cif, "exptl_crystal"))
	cbf_failnez (cbf_new_column   (cif, "id"))
	cbf_failnez (cbf_set_value    (cif, hcif.crystal_id))

	if ( PRINT_CHEMICAL ) {
		cbf_failnez (cbf_force_new_category (cif, "chemical"))
		cbf_failnez (cbf_new_column   (cif, "entry_id"))
		cbf_failnez (cbf_set_value    (cif, "img1"))
	}

	/********************************************************************
	 * DIFFRN
	 ********************************************************************/

	cbf_failnez (cbf_force_new_category (cif, "diffrn"))
	cbf_failnez (cbf_new_column   (cif, "id"))
	cbf_failnez (cbf_set_value    (cif, hcif.diffrn_id ))
	cbf_failnez (cbf_new_column   (cif, "crystal_id"))
	cbf_failnez (cbf_set_value    (cif, hcif.crystal_id))

	/********************************************************************
	 * DIFFRN_SOURCE
	 ********************************************************************/

	cbf_failnez (cbf_force_new_category (cif, "diffrn_source"))
	cbf_failnez (cbf_new_column   (cif, "diffrn_id"))
	cbf_failnez (cbf_set_value    (cif, hcif.diffrn_id ))
	cbf_failnez (cbf_new_column   (cif, "source"))
	cbf_failnez (cbf_set_value    (cif, hcif.diffrn_source.source))
	cbf_failnez (cbf_new_column   (cif, "type"))
	cbf_failnez (cbf_set_value    (cif, hcif.diffrn_source.type))
	cbf_failnez (cbf_new_column   (cif, "current"))
	cbf_failnez (cbf_set_doublevalue (cif, "%.1f", hcif.diffrn_source.current))
	cbf_failnez (cbf_new_column   (cif, "voltage"))
	cbf_failnez (cbf_set_doublevalue (cif, "%.1f", hcif.diffrn_source.voltage))

	/********************************************************************
	 * DIFFRN_MEASUREMENT
	 ********************************************************************/

	cbf_failnez (cbf_force_new_category (cif, "diffrn_measurement"))
	cbf_failnez (cbf_new_column   (cif, "diffrn_id"))
	cbf_failnez (cbf_set_value    (cif, hcif.diffrn_id ))
	cbf_failnez (cbf_new_column   (cif, "method"))
	cbf_failnez (cbf_set_value    (cif, hcif.diffrn_measurement.method))
	cbf_failnez (cbf_new_column   (cif, "device"))
	cbf_failnez (cbf_set_value    (cif, hcif.diffrn_measurement.device))
	cbf_failnez (cbf_new_column   (cif, "device_type"))
	cbf_failnez (cbf_set_value    (cif, hcif.diffrn_measurement.device_type))
	cbf_failnez (cbf_new_column   (cif, "sample_detector_distance"))
	cbf_failnez (cbf_set_doublevalue (cif, "%.6f", hcif.diffrn_measurement.sample_detector_distance))

	/********************************************************************
	 * DIFFRN_RADIATION
	 ********************************************************************/

	cbf_failnez (cbf_force_new_category (cif, "diffrn_radiation"))
	cbf_failnez (cbf_new_column   (cif, "diffrn_id"))
	cbf_failnez (cbf_set_value    (cif, hcif.diffrn_id ))
	cbf_failnez (cbf_new_column   (cif, "detector"))
	cbf_failnez (cbf_set_value    (cif, hcif.diffrn_radiation.detector))
	cbf_failnez (cbf_new_column   (cif, "collimation"))
	cbf_failnez (cbf_set_value    (cif, hcif.diffrn_radiation.collimation))
	cbf_failnez (cbf_new_column   (cif, "monochromator"))
	cbf_failnez (cbf_set_value    (cif, hcif.diffrn_radiation.monochromator))

	/********************************************************************
	 * DIFFRN_RADIATION_WAVELENGTH
	 ********************************************************************/

	cbf_failnez (cbf_force_new_category (cif, "diffrn_radiation_wavelength"))
	cbf_failnez (cbf_new_column   (cif, "id"))
	cbf_failnez (cbf_set_value    (cif, "WL1"))
	cbf_failnez (cbf_new_column   (cif, "wavelength"))
	cbf_failnez (cbf_set_doublevalue (cif, "%.6f", hcif.diffrn_radiation_wavelength.wavelength))
	cbf_failnez (cbf_new_column   (cif, "wavelength_wt"))
	cbf_failnez (cbf_set_doublevalue (cif, "%.3f", hcif.diffrn_radiation_wavelength.wavelength_wt))
	cbf_failnez (cbf_new_column   (cif, "polarisn_ratio"))
	cbf_failnez (cbf_set_doublevalue (cif, "%.3f", hcif.diffrn_radiation_wavelength.polarisn_ratio))
	cbf_failnez (cbf_new_column   (cif, "polarisn_norm"))
	cbf_failnez (cbf_set_doublevalue (cif, "%.3f", hcif.diffrn_radiation_wavelength.polarisn_norm))


	/********************************************************************
	 * DIFFRN_DETECTOR 
	 ********************************************************************/

	cbf_failnez (cbf_force_new_category (cif, "diffrn_detector"))
	cbf_failnez (cbf_new_column   (cif, "id"))
	cbf_failnez (cbf_set_value    (cif, hcif.detector_id ))
	cbf_failnez (cbf_new_column   (cif, "diffrn_id"))
	cbf_failnez (cbf_set_value    (cif, hcif.diffrn_id ))
	cbf_failnez (cbf_new_column   (cif, "detector"))
	cbf_failnez (cbf_set_value    (cif, hcif.diffrn_detector.detector))
	cbf_failnez (cbf_new_column   (cif, "type"))
	cbf_failnez (cbf_set_value    (cif, hcif.diffrn_detector.type))

	/********************************************************************
	 * DIFFRN_DETECTOR_ELEMENT
	 ********************************************************************/

	cbf_failnez (cbf_force_new_category (cif, "diffrn_detector_element"))
	cbf_failnez (cbf_new_column   (cif, "id"))
	cbf_failnez (cbf_set_value    (cif, hcif.element_id ))
	cbf_failnez (cbf_new_column   (cif, "detector_id"))
	cbf_failnez (cbf_set_value    (cif, hcif.detector_id ))
	cbf_failnez (cbf_new_column   (cif, "center[1]"))
	cbf_failnez (cbf_set_doublevalue (cif, "%.3f", hcif.diffrn_detector_element.center_x))
	cbf_failnez (cbf_new_column   (cif, "center[2]"))
	cbf_failnez (cbf_set_doublevalue (cif, "%.3f", hcif.diffrn_detector_element.center_y))

	/********************************************************************
	 * MARIP
	 ********************************************************************/

	cbf_failnez (cbf_force_new_category (cif, "diffrn_detector_mar"))
	cbf_failnez (cbf_new_column   (cif, "id"))
	cbf_failnez (cbf_set_value    (cif, hcif.mar_id ))
	cbf_failnez (cbf_new_column   (cif, "detector_id"))
	cbf_failnez (cbf_set_value    (cif, hcif.detector_id ))

	/********************************************************************
	 * MARIP_ADC
	 ********************************************************************/

	cbf_failnez (cbf_force_new_category (cif, "diffrn_detector_mar_adc"))
	cbf_failnez (cbf_new_column   (cif, "mar_id"))
	cbf_failnez (cbf_set_value    (cif, hcif.mar_id ))
	cbf_failnez (cbf_new_column   (cif, "channel_a"))
	cbf_failnez (cbf_set_integervalue (cif, hcif.diffrn_detector_mar_adc.channel_a))
	cbf_failnez (cbf_new_column   (cif, "channel_b"))
	cbf_failnez (cbf_set_integervalue (cif, hcif.diffrn_detector_mar_adc.channel_b))
	cbf_failnez (cbf_new_column   (cif, "add_channel_a"))
	cbf_failnez (cbf_set_integervalue (cif, hcif.diffrn_detector_mar_adc.add_channel_a))
	cbf_failnez (cbf_new_column   (cif, "add_channel_b"))
	cbf_failnez (cbf_set_integervalue (cif, hcif.diffrn_detector_mar_adc.add_channel_b))
	cbf_failnez (cbf_new_column   (cif, "oversampling"))
	cbf_failnez (cbf_set_integervalue (cif, hcif.diffrn_detector_mar_adc.oversampling))
	cbf_failnez (cbf_new_column   (cif, "multiplier"))
	cbf_failnez (cbf_set_doublevalue (cif, "%.3f", hcif.diffrn_detector_mar_adc.multiplier))
	cbf_failnez (cbf_new_column   (cif, "gain"))
	cbf_failnez (cbf_set_doublevalue (cif, "%.3f", hcif.diffrn_detector_mar_adc.gain))

	/********************************************************************
	 * MARIP_GAPS
	 ********************************************************************/

	cbf_failnez (cbf_force_new_category (cif, "diffrn_detector_mar_gaps"))
	cbf_failnez (cbf_new_column   (cif, "mar_id"))
	cbf_failnez (cbf_set_value    (cif, hcif.mar_id ))
	cbf_failnez (cbf_new_column   (cif, "gap[1]"))
	cbf_failnez (cbf_set_integervalue    (cif, hcif.diffrn_detector_mar_gaps.gap[0]))
	cbf_failnez (cbf_new_column   (cif, "gap[2]"))
	cbf_failnez (cbf_set_integervalue    (cif, hcif.diffrn_detector_mar_gaps.gap[1]))
	cbf_failnez (cbf_new_column   (cif, "gap[3]"))
	cbf_failnez (cbf_set_integervalue    (cif, hcif.diffrn_detector_mar_gaps.gap[2]))
	cbf_failnez (cbf_new_column   (cif, "gap[4]"))
	cbf_failnez (cbf_set_integervalue    (cif, hcif.diffrn_detector_mar_gaps.gap[3]))
	cbf_failnez (cbf_new_column   (cif, "gap[5]"))
	cbf_failnez (cbf_set_integervalue    (cif, hcif.diffrn_detector_mar_gaps.gap[4]))
	cbf_failnez (cbf_new_column   (cif, "gap[6]"))
	cbf_failnez (cbf_set_integervalue    (cif, hcif.diffrn_detector_mar_gaps.gap[5]))
	cbf_failnez (cbf_new_column   (cif, "gap[7]"))
	cbf_failnez (cbf_set_integervalue    (cif, hcif.diffrn_detector_mar_gaps.gap[6]))
	cbf_failnez (cbf_new_column   (cif, "gap[8]"))
	cbf_failnez (cbf_set_integervalue    (cif, hcif.diffrn_detector_mar_gaps.gap[7]))

	/********************************************************************
	 * MARIP_SCAN
	 ********************************************************************/

	cbf_failnez (cbf_force_new_category (cif, "diffrn_detector_mar_scan"))
	cbf_failnez (cbf_new_column   (cif, "mar_id"))
	cbf_failnez (cbf_set_value    (cif, hcif.mar_id ))
	cbf_failnez (cbf_new_column   (cif, "scanmode"))
	cbf_failnez (cbf_set_integervalue (cif, hcif.diffrn_detector_mar_scan.scanmode))
	cbf_failnez (cbf_new_column   (cif, "number_of_pixels"))
	cbf_failnez (cbf_set_integervalue (cif, hcif.diffrn_detector_mar_scan.number_of_pixels))
	cbf_failnez (cbf_new_column   (cif, "number_of_pixels_larger_16_bit"))
	cbf_failnez (cbf_set_integervalue (cif, hcif.diffrn_detector_mar_scan.number_of_pixels_larger_16_bit))
	cbf_failnez (cbf_new_column   (cif, "radial_offset"))
	cbf_failnez (cbf_set_doublevalue (cif, "%1.3f", hcif.diffrn_detector_mar_scan.radial_offset))
	cbf_failnez (cbf_new_column   (cif, "tangential_offset"))
	cbf_failnez (cbf_set_doublevalue (cif, "%1.3f", hcif.diffrn_detector_mar_scan.tangential_offset))
	cbf_failnez (cbf_new_column   (cif, "pixel_height"))
	cbf_failnez (cbf_set_doublevalue (cif, "%1.3f", hcif.diffrn_detector_mar_scan.pixel_height))
	cbf_failnez (cbf_new_column   (cif, "pixel_length"))
	cbf_failnez (cbf_set_doublevalue (cif, "%1.3f", hcif.diffrn_detector_mar_scan.pixel_length))

	/********************************************************************
	 * MARIP_EXPOSURE
	 ********************************************************************/

	cbf_failnez (cbf_force_new_category (cif, "diffrn_detector_mar_exposure"))
	cbf_failnez (cbf_new_column   (cif, "mar_id"))
	cbf_failnez (cbf_set_value    (cif, hcif.mar_id ))
	cbf_failnez (cbf_new_column   (cif, "type"))
	cbf_failnez (cbf_set_value    (cif, hcif.diffrn_detector_mar_exposure.type))
	cbf_failnez (cbf_new_column   (cif, "time"))
	cbf_failnez (cbf_set_doublevalue (cif, "%1.2f", hcif.diffrn_detector_mar_exposure.time))
	cbf_failnez (cbf_new_column   (cif, "dose"))
	cbf_failnez (cbf_set_doublevalue (cif, "%1.2f", hcif.diffrn_detector_mar_exposure.dose))

	/********************************************************************
	 * MARIP_EXPOSURE_DOSE
	 ********************************************************************/

	cbf_failnez (cbf_force_new_category (cif, "diffrn_detector_mar_exposure_dose"))
	cbf_failnez (cbf_new_column   (cif, "mar_id"))
	cbf_failnez (cbf_set_value    (cif, hcif.mar_id ))
	cbf_failnez (cbf_new_column   (cif, "dose"))
	cbf_failnez (cbf_set_doublevalue (cif, "%1.2f", hcif.diffrn_detector_mar_exposure.dose))
	cbf_failnez (cbf_new_column   (cif, "avg"))
	cbf_failnez (cbf_set_doublevalue (cif, "%1.2f", hcif.diffrn_detector_mar_exposure_dose.avg))
	cbf_failnez (cbf_new_column   (cif, "esd"))
	cbf_failnez (cbf_set_doublevalue (cif, "%1.2f", hcif.diffrn_detector_mar_exposure_dose.esd))
	cbf_failnez (cbf_new_column   (cif, "min"))
	cbf_failnez (cbf_set_doublevalue (cif, "%1.2f", hcif.diffrn_detector_mar_exposure_dose.min))
	cbf_failnez (cbf_new_column   (cif, "max"))
	cbf_failnez (cbf_set_doublevalue (cif, "%1.2f", hcif.diffrn_detector_mar_exposure_dose.max))
	cbf_failnez (cbf_new_column   (cif, "start"))
	cbf_failnez (cbf_set_doublevalue (cif, "%1.2f", hcif.diffrn_detector_mar_exposure_dose.start))
	cbf_failnez (cbf_new_column   (cif, "end"))
	cbf_failnez (cbf_set_doublevalue (cif, "%1.2f", hcif.diffrn_detector_mar_exposure_dose.end))
	cbf_failnez (cbf_new_column   (cif, "measurements"))
	cbf_failnez (cbf_set_integervalue (cif, hcif.diffrn_detector_mar_exposure_dose.measurements))

	/********************************************************************
	 * MARIP_DATA
	 ********************************************************************/

	cbf_failnez (cbf_force_new_category (cif, "diffrn_detector_mar_data"))
	cbf_failnez (cbf_new_column   (cif, "mar_id"))
	cbf_failnez (cbf_set_value    (cif, hcif.mar_id ))
	cbf_failnez (cbf_new_column   (cif, "intensity_avg"))
	cbf_failnez (cbf_set_doublevalue (cif, "%1.3f", hcif.diffrn_detector_mar_data.intensity_avg))
	cbf_failnez (cbf_new_column   (cif, "intensity_esd"))
	cbf_failnez (cbf_set_doublevalue (cif, "%1.3f", hcif.diffrn_detector_mar_data.intensity_esd))
	cbf_failnez (cbf_new_column   (cif, "intensity_min"))
	cbf_failnez (cbf_set_integervalue (cif, hcif.diffrn_detector_mar_data.intensity_min))
	cbf_failnez (cbf_new_column   (cif, "intensity_max"))
	cbf_failnez (cbf_set_integervalue (cif, hcif.diffrn_detector_mar_data.intensity_max))
	cbf_failnez (cbf_new_column   (cif, "histogram_start"))
	cbf_failnez (cbf_set_integervalue (cif, hcif.diffrn_detector_mar_data.histogram_start))
	cbf_failnez (cbf_new_column   (cif, "histogram_end"))
	cbf_failnez (cbf_set_integervalue (cif, hcif.diffrn_detector_mar_data.histogram_end))
	cbf_failnez (cbf_new_column   (cif, "histogram_max"))
	cbf_failnez (cbf_set_integervalue (cif, hcif.diffrn_detector_mar_data.histogram_max))

	/********************************************************************
	 * MARIP_AXIS
	 ********************************************************************/

	cbf_failnez (cbf_force_new_category (cif, "diffrn_detector_mar_axis"))
	cbf_failnez (cbf_new_column   (cif, "mar_id"))

	j = AXIS_DISTANCE;
	if (strstr(prg, "dtb" ) )  
		j = AXIS_ROT_VER;

	for ( i=0; i<=j; i++ ) {
		cbf_failnez (cbf_set_value    (cif, hcif.mar_id ))
		if ( i==j ) break;
		cbf_failnez (cbf_new_row      (cif))
	}

	cbf_failnez (cbf_new_column   (cif, "axis_id"))
	cbf_failnez (cbf_rewind_row   (cif))
	for ( i=0; i<=j; i++ ) {
		cbf_failnez (cbf_set_value    (cif, hcif.diffrn_detector_mar_axis[i].axis_id))
		if ( i==j) break;
		cbf_failnez (cbf_next_row     (cif))
	}

	cbf_failnez (cbf_new_column   (cif, "angle_start"))
	cbf_failnez (cbf_rewind_row   (cif))
	for ( i=0; i<=j; i++ ) {
		cbf_failnez (cbf_set_doublevalue (cif, "%.3f", hcif.diffrn_detector_mar_axis[i].angle_start))
		if ( i==j) break;
		cbf_failnez (cbf_next_row     (cif))
	}

	cbf_failnez (cbf_new_column   (cif, "angle_end"))
	cbf_failnez (cbf_rewind_row   (cif))
	for ( i=0; i<=j; i++ ) {
		cbf_failnez (cbf_set_doublevalue (cif, "%.3f", hcif.diffrn_detector_mar_axis[i].angle_end))
		if ( i==j) break;
		cbf_failnez (cbf_next_row     (cif))
	}

	cbf_failnez (cbf_new_column   (cif, "angle_increment"))
	cbf_failnez (cbf_rewind_row   (cif))
	for ( i=0; i<=j; i++ ) {
		cbf_failnez (cbf_set_doublevalue (cif, "%.3f", hcif.diffrn_detector_mar_axis[i].angle_increment))
		if ( i==j) break;
		cbf_failnez (cbf_next_row     (cif))
	}

	cbf_failnez (cbf_new_column   (cif, "oscillations"))
	cbf_failnez (cbf_rewind_row   (cif))
	for ( i=0; i<=j; i++ ) {
		cbf_failnez (cbf_set_integervalue (cif, hcif.diffrn_detector_mar_axis[i].oscillations))
		if ( i==j) break;
		cbf_failnez (cbf_next_row     (cif))
	}

	cbf_failnez (cbf_new_column   (cif, "displacement"))
	cbf_failnez (cbf_rewind_row   (cif))
	for ( i=0; i<=j; i++ ) {
		cbf_failnez (cbf_set_doublevalue (cif, "%.3f", hcif.diffrn_detector_mar_axis[i].displacement))
		if ( i==j) break;
		cbf_failnez (cbf_next_row     (cif))
	}

	/********************************************************************
	 * DIFFRN_FRAME_DATA
	 ********************************************************************/

	cbf_failnez (cbf_force_new_category (cif, "diffrn_frame_data"))
	cbf_failnez (cbf_new_column   (cif, "id"))
	cbf_failnez (cbf_set_value    (cif, hcif.frame_id ))
	cbf_failnez (cbf_new_column   (cif, "detector_element_id"))
	cbf_failnez (cbf_set_value    (cif, hcif.element_id ))
	cbf_failnez (cbf_new_column   (cif, "array_id"))
	cbf_failnez (cbf_set_value    (cif, hcif.array_id ))
	cbf_failnez (cbf_new_column   (cif, "binary_id"))
	cbf_failnez (cbf_set_integervalue (cif, hcif.binary_id))

	/********************************************************************
	 * DIFFRN_SCAN
	 ********************************************************************/

	cbf_failnez (cbf_force_new_category (cif, "diffrn_scan"))
	cbf_failnez (cbf_new_column   (cif, "scan_id"))
	cbf_failnez (cbf_set_value    (cif, hcif.scan_id ))
	cbf_failnez (cbf_new_column   (cif, "frame_id_start"))
	cbf_failnez (cbf_set_value    (cif, hcif.diffrn_scan.frame_id_start))
	cbf_failnez (cbf_new_column   (cif, "frame_id_end"))
	cbf_failnez (cbf_set_value    (cif, hcif.diffrn_scan.frame_id_end))
	cbf_failnez (cbf_new_column   (cif, "frames"))
	cbf_failnez (cbf_set_integervalue (cif, hcif.diffrn_scan.frames))

	/********************************************************************
	 * AXIS
	 ********************************************************************/

        if ( !PRINT_AXIS ) goto CIF_DIFFRN_SCAN_AXIS;

	cbf_failnez (cbf_force_new_category (cif, "axis"))

	cbf_failnez (cbf_new_column   (cif, "id"))
	cbf_failnez (cbf_set_value    (cif, "phi" ))
	cbf_failnez (cbf_new_row      (cif))
	cbf_failnez (cbf_set_value    (cif, "chi" ))
	cbf_failnez (cbf_new_row      (cif))
	cbf_failnez (cbf_set_value    (cif, "omega" ))
	cbf_failnez (cbf_new_row      (cif))
	cbf_failnez (cbf_set_value    (cif, "twotheta" ))
	cbf_failnez (cbf_new_row      (cif))
	cbf_failnez (cbf_set_value    (cif, "distance" ))

	cbf_failnez (cbf_new_column   (cif, "type"))
	cbf_failnez (cbf_rewind_row   (cif))
	cbf_failnez (cbf_set_value    (cif, "rotation" ))
	cbf_failnez (cbf_next_row     (cif))
	cbf_failnez (cbf_set_value    (cif, "rotation" ))
	cbf_failnez (cbf_next_row     (cif))
	cbf_failnez (cbf_set_value    (cif, "rotation" ))
	cbf_failnez (cbf_next_row     (cif))
	cbf_failnez (cbf_set_value    (cif, "rotation" ))
	cbf_failnez (cbf_next_row     (cif))
	cbf_failnez (cbf_set_value    (cif, "translation" ))

	cbf_failnez (cbf_new_column   (cif, "equipment"))
	cbf_failnez (cbf_rewind_row   (cif))
	cbf_failnez (cbf_set_value    (cif, "detector" ))
	cbf_failnez (cbf_next_row     (cif))
	cbf_failnez (cbf_set_value    (cif, "detector" ))
	cbf_failnez (cbf_next_row     (cif))
	cbf_failnez (cbf_set_value    (cif, "detector" ))
	cbf_failnez (cbf_next_row     (cif))
	cbf_failnez (cbf_set_value    (cif, "detector" ))
	cbf_failnez (cbf_next_row     (cif))
	cbf_failnez (cbf_set_value    (cif, "detector" ))

	cbf_failnez (cbf_new_column   (cif, "vector[1]"))
	cbf_failnez (cbf_rewind_row   (cif))
	cbf_failnez (cbf_set_doublevalue (cif, "%.0f", 1.0))
	cbf_failnez (cbf_next_row   (cif))
	cbf_failnez (cbf_set_doublevalue (cif, "%.0f", 0.0))
	cbf_failnez (cbf_next_row   (cif))
	cbf_failnez (cbf_set_doublevalue (cif, "%.0f", 0.0))
	cbf_failnez (cbf_next_row   (cif))
	cbf_failnez (cbf_set_doublevalue (cif, "%.0f", 1.0))
	cbf_failnez (cbf_next_row   (cif))
	cbf_failnez (cbf_set_doublevalue (cif, "%.0f", 0.0))

	cbf_failnez (cbf_new_column   (cif, "vector[2]"))
	cbf_failnez (cbf_rewind_row   (cif))
	cbf_failnez (cbf_set_doublevalue (cif, "%.0f", 0.0))
	cbf_failnez (cbf_next_row   (cif))
	cbf_failnez (cbf_set_doublevalue (cif, "%.0f", 1.0))
	cbf_failnez (cbf_next_row   (cif))
	cbf_failnez (cbf_set_doublevalue (cif, "%.0f", 0.0))
	cbf_failnez (cbf_next_row   (cif))
	cbf_failnez (cbf_set_doublevalue (cif, "%.0f", 0.0))
	cbf_failnez (cbf_next_row   (cif))
	cbf_failnez (cbf_set_doublevalue (cif, "%.0f", 0.0))

	cbf_failnez (cbf_new_column   (cif, "vector[3]"))
	cbf_failnez (cbf_rewind_row   (cif))
	cbf_failnez (cbf_set_doublevalue (cif, "%.0f", 0.0))
	cbf_failnez (cbf_next_row   (cif))
	cbf_failnez (cbf_set_doublevalue (cif, "%.0f", 0.0))
	cbf_failnez (cbf_next_row   (cif))
	cbf_failnez (cbf_set_doublevalue (cif, "%.0f", 1.0))
	cbf_failnez (cbf_next_row   (cif))
	cbf_failnez (cbf_set_doublevalue (cif, "%.0f", 0.0))
	cbf_failnez (cbf_next_row   (cif))
	cbf_failnez (cbf_set_doublevalue (cif, "%.0f", -1.0))

	cbf_failnez (cbf_new_column   (cif, "offset[1]"))
	cbf_failnez (cbf_rewind_row   (cif))
	for (i=0;i<=AXIS_DISTANCE;i++) {
		cbf_failnez (cbf_set_doublevalue (cif, "%.0f", 0.0))
		if ( i==AXIS_DISTANCE ) break;
		cbf_failnez (cbf_next_row   (cif))
	}

	cbf_failnez (cbf_new_column   (cif, "offset[2]"))
	cbf_failnez (cbf_rewind_row   (cif))
	for (i=0;i<=AXIS_DISTANCE;i++) {
		cbf_failnez (cbf_set_doublevalue (cif, "%.0f", 0.0))
		if ( i==AXIS_DISTANCE ) break;
		cbf_failnez (cbf_next_row   (cif))
	}

	cbf_failnez (cbf_new_column   (cif, "offset[3]"))
	cbf_failnez (cbf_rewind_row   (cif))
	for (i=0;i<=AXIS_DISTANCE;i++) {
		cbf_failnez (cbf_set_doublevalue (cif, "%.0f", 0.0))
		if ( i==AXIS_DISTANCE ) break;
		cbf_failnez (cbf_next_row   (cif))
	}

	/********************************************************************
	 * DIFFRN_SCAN_AXIS
	 ********************************************************************/
CIF_DIFFRN_SCAN_AXIS:
	cbf_failnez (cbf_force_new_category (cif, "diffrn_scan_axis"))
	cbf_failnez (cbf_new_column   (cif, "scan_id"))
	cbf_failnez (cbf_set_value    (cif, hcif.scan_id ))
	cbf_failnez (cbf_new_column   (cif, "axis_id"))
	cbf_failnez (cbf_set_value    (cif, hcif.diffrn_scan_axis[AXIS_PHI].axis_id))
	cbf_failnez (cbf_new_column   (cif, "angle_start"))
	cbf_failnez (cbf_set_doublevalue (cif, "%.3f", hcif.diffrn_scan_axis[AXIS_PHI].angle_start))
	cbf_failnez (cbf_new_column   (cif, "angle_increment"))
	cbf_failnez (cbf_set_doublevalue (cif, "%.3f", hcif.diffrn_scan_axis[AXIS_PHI].angle_increment))

	/********************************************************************
	 * DIFFRN_SCAN_FRAME
	 ********************************************************************/

	cbf_failnez (cbf_force_new_category (cif, "diffrn_scan_frame"))
	cbf_failnez (cbf_new_column   (cif, "scan_id"))
	cbf_failnez (cbf_set_value    (cif, hcif.scan_id ))
	cbf_failnez (cbf_new_column   (cif, "frame_id"))
	cbf_failnez (cbf_set_value    (cif, hcif.frame_id ))

	/********************************************************************
	 * DIFFRN_SCAN_FRAME_AXIS
	 ********************************************************************/

	cbf_failnez (cbf_force_new_category (cif, "diffrn_scan_frame_axis"))

	cbf_failnez (cbf_new_column   (cif, "frame_id"))
	for ( i=0; i<=AXIS_DISTANCE; i++ ) {
		cbf_failnez (cbf_set_value    (cif, hcif.frame_id ))
		if ( i==AXIS_DISTANCE ) break;
		cbf_failnez (cbf_new_row      (cif))
	}

	cbf_failnez (cbf_new_column   (cif, "axis_id"))
	cbf_failnez (cbf_rewind_row   (cif))
	for ( i=0; i<=AXIS_DISTANCE; i++ ) {
		cbf_failnez (cbf_set_value    (cif, hcif.diffrn_scan_frame_axis[i].axis_id))
		if ( i==AXIS_DISTANCE ) break;
		cbf_failnez (cbf_next_row     (cif))
	}

	cbf_failnez (cbf_new_column   (cif, "angle"))
	cbf_failnez (cbf_rewind_row   (cif))
	for ( i=0; i<=AXIS_DISTANCE; i++ ) {
		cbf_failnez (cbf_set_doublevalue (cif, "%.3f", hcif.diffrn_scan_frame_axis[i].angle))
		if ( i==AXIS_DISTANCE ) break;
		cbf_failnez (cbf_next_row     (cif))
	}

	cbf_failnez (cbf_new_column   (cif, "displacement"))
	cbf_failnez (cbf_rewind_row   (cif))
	for ( i=0; i<=AXIS_DISTANCE; i++ ) {
		cbf_failnez (cbf_set_doublevalue (cif, "%.3f", hcif.diffrn_scan_frame_axis[i].displacement))
		if ( i==AXIS_DISTANCE ) break;
		cbf_failnez (cbf_next_row     (cif))
	}

	/********************************************************************
	 * ARRAY_STRUCTURE
	 ********************************************************************/

	cbf_failnez (cbf_force_new_category (cif, "array_structure"))
	cbf_failnez (cbf_new_column   (cif, "array_id"))
	cbf_failnez (cbf_set_value    (cif, hcif.array_id ))

	/********************************************************************
	 * ARRAY_INTENSITIES
	 ********************************************************************/

	cbf_failnez (cbf_force_new_category (cif, "array_intensities"))
	cbf_failnez (cbf_new_column   (cif, "array_id"))
	cbf_failnez (cbf_set_value    (cif, hcif.array_id ))
	cbf_failnez (cbf_new_column   (cif, "binary_id"))
	cbf_failnez (cbf_set_integervalue (cif, hcif.binary_id))
	cbf_failnez (cbf_new_column   (cif, "linearity"))
	cbf_failnez (cbf_set_value    (cif, hcif.array_intensities.linearity))
	cbf_failnez (cbf_new_column   (cif, "undefined_value"))
	cbf_failnez (cbf_set_doublevalue (cif, "%.0f", hcif.array_intensities.undefined_value))
	cbf_failnez (cbf_new_column   (cif, "overload"))
	cbf_failnez (cbf_set_doublevalue (cif, "%.0f", hcif.array_intensities.overload))
	cbf_failnez (cbf_new_column   (cif, "scaling"))
	cbf_failnez (cbf_set_doublevalue (cif, "%.3f", hcif.array_intensities.scaling))
	cbf_failnez (cbf_new_column   (cif, "gain"))
	cbf_failnez (cbf_set_doublevalue (cif, "%.3f", hcif.array_intensities.gain))

	/********************************************************************
	 * ARRAY_STRUCTURE_LIST
	 ********************************************************************/

	cbf_failnez (cbf_force_new_category (cif, "array_structure_list"))

	cbf_failnez (cbf_new_column   (cif, "array_id"))
	for ( i=0; i<2; i++ ) {
		cbf_failnez (cbf_set_value    (cif, hcif.array_id ))
		if ( i==1) break;
		cbf_failnez (cbf_new_row      (cif))
	}

	cbf_failnez (cbf_new_column   (cif, "index"))
	cbf_failnez (cbf_rewind_row   (cif))
	for ( i=0; i<2; i++ ) {
		cbf_failnez (cbf_set_integervalue (cif, hcif.array_structure_list[i].index))
		if ( i==1) break;
		cbf_failnez (cbf_next_row     (cif))
	}

	cbf_failnez (cbf_new_column   (cif, "dimension"))
	cbf_failnez (cbf_rewind_row   (cif))
	for ( i=0; i<2; i++ ) {
		cbf_failnez (cbf_set_integervalue (cif, hcif.array_structure_list[i].dimension))
		if ( i==1) break;
		cbf_failnez (cbf_next_row     (cif))
	}

	cbf_failnez (cbf_new_column   (cif, "precedence"))
	cbf_failnez (cbf_rewind_row   (cif))
	for ( i=0; i<2; i++ ) {
		cbf_failnez (cbf_set_integervalue (cif, hcif.array_structure_list[i].precedence))
		if ( i==1) break;
		cbf_failnez (cbf_next_row     (cif))
	}

	cbf_failnez (cbf_new_column   (cif, "direction"))
	cbf_failnez (cbf_rewind_row   (cif))
	for ( i=0; i<2; i++ ) {
		cbf_failnez (cbf_set_value (cif, hcif.array_structure_list[i].direction))
		if ( i==1) break;
		cbf_failnez (cbf_next_row     (cif))
	}

	/********************************************************************
	 * ARRAY_ELEMENT_SIZE
	 ********************************************************************/

	cbf_failnez (cbf_force_new_category (cif, "array_element_size"))

	cbf_failnez (cbf_new_column   (cif, "array_id"))
	for ( i=0; i<2; i++ ) {
		cbf_failnez (cbf_set_value    (cif, hcif.array_id ))
		if ( i==1) break;
		cbf_failnez (cbf_new_row      (cif))
	}

	cbf_failnez (cbf_new_column   (cif, "index"))
	cbf_failnez (cbf_rewind_row   (cif))
	for ( i=0; i<2; i++ ) {
		cbf_failnez (cbf_set_integervalue (cif, hcif.array_element_size.index[i]))
		if ( i==1) break;
		cbf_failnez (cbf_next_row     (cif))
	}

	cbf_failnez (cbf_new_column   (cif, "size"))
	cbf_failnez (cbf_rewind_row   (cif))
	for ( i=0; i<2; i++ ) {
		cbf_failnez (cbf_set_doublevalue (cif, "%1.4e", hcif.array_element_size.pixelsize[i]))
		if ( i==1) break;
		cbf_failnez (cbf_next_row     (cif))
	}

	/********************************************************************
	 * ARRAY_DATA
	 ********************************************************************/

	cbf_failnez (cbf_force_new_category     (cif, "array_data"))
	cbf_failnez (cbf_new_column       (cif, "array_id"))
	cbf_failnez (cbf_set_value        (cif, "image_1"))
	cbf_failnez (cbf_new_column       (cif, "binary_id"))
	cbf_failnez (cbf_set_integervalue (cif, hcif.binary_id))
	cbf_failnez (cbf_new_column       (cif, "data"))

	return 1;
}

/******************************************************************
 * Function: mar3452CIFHeader
 ******************************************************************/
void mar3452CIFHeader(char *prg, char *f, MAR345_HEADER h345)
{
char	*c,str[64],num[16];
int	i;
float	pixelsize;
time_t	now;

	time( &now );

	/* Initialize CIF Header */
	memset( (char *)&hcif, 0, sizeof(CIF_HEADER) );

	pixelsize = h345.pixel_height;
	if ( pixelsize > 1.0 ) pixelsize /= 1000.;

	hcif.binary_id					= 1;

	strcpy(hcif.entry_id, 				"img1");
	strcpy(hcif.scan_id, 				"scan1");
	strcpy(hcif.array_id, 				"img1");
	strcpy(hcif.crystal_id, 			"xtal1");
	sprintf(str, "SN_%03d",h345.scanner);
	strcpy(hcif.element_id, 			str);
	strcpy(hcif.mar_id, 				str);
	strcpy(hcif.diffrn_id, 				"diffrn1");
	strcpy(hcif.data_id, 				"data1");
	if ( strlen( h345.detector) )
		strcpy(hcif.detector_id, 		h345.detector);
	else
		strcpy(hcif.detector_id, 		"mar345");
	strcpy(hcif.frame_id,    			"frm1");

	sprintf(str, "%s",(char *)ctime( &now ) );
	str[ strlen( str ) -1 ] = '\0';
	if ( strlen( h345.date ) > 5 ) 
	strcpy(hcif.audit.creation_date,		h345.date);
	else
	strcpy(hcif.audit.creation_date,		str);
	
	sprintf(str, "Created by %s", 			prg);
	strcpy(hcif.audit.creation_method,		str);


	if ( strstr(hcif.detector_id, "ccd" )  ||  strstr(hcif.detector_id, "CCD" ) )
	strcpy(hcif.diffrn_radiation.detector,		"CCD");
	else
	strcpy(hcif.diffrn_radiation.detector,		"Image Plate");
	strcpy(hcif.diffrn_radiation.collimation,	"double slits");
	strcpy(hcif.diffrn_radiation.monochromator,	h345.filter);
	hcif.diffrn_radiation_wavelength.polarisn_ratio	= h345.polar;
	hcif.diffrn_radiation_wavelength.polarisn_norm 	= 0.0;
	hcif.diffrn_radiation_wavelength.wavelength    	= h345.wave;
	hcif.diffrn_radiation_wavelength.wavelength_wt  = 1.0;

	if ( strlen( h345.source ) )	
	strcpy(hcif.diffrn_source.source,       	h345.source);
	else
	strcpy(hcif.diffrn_source.source,       	"unknown");
	strcpy(hcif.diffrn_source.type,         	"unknown");
	hcif.diffrn_source.current          		= h345.mA;
	hcif.diffrn_source.voltage          		= h345.kV;
	hcif.diffrn_source.power            		= h345.kV*h345.mA;

	strcpy(hcif.array_structure.compression_type,	"x-CBF_PACKED");
	strcpy(hcif.array_structure.encoding_type,	"unsigned 32-bit integer");
	strcpy(hcif.array_structure.byte_order,		"little_endian");

	hcif.array_structure_list[0].dimension		= h345.size;
	hcif.array_structure_list[1].dimension		= h345.size;
	hcif.array_structure_list[0].index		= 1;
	hcif.array_structure_list[1].index		= 2;
	hcif.array_structure_list[0].precedence		= 2;
	hcif.array_structure_list[1].precedence		= 1;
	strcpy(hcif.array_structure_list[0].direction, 	"increasing");
	strcpy(hcif.array_structure_list[1].direction, 	"increasing");

	hcif.array_element_size.pixelsize[0]		= pixelsize/1000.;
	hcif.array_element_size.pixelsize[1]		= pixelsize/1000.;
	hcif.array_element_size.index[0]		= 1;
	hcif.array_element_size.index[1]		= 2;

	hcif.array_intensities.gain     		= h345.gain;
	hcif.array_intensities.overload			= 250000;
	hcif.array_intensities.undefined_value		= 999999;
	strcpy(hcif.array_intensities.linearity,     	"linear");
	hcif.array_intensities.offset         		= 0;
	hcif.array_intensities.scaling        		= h345.multiplier;

	strcpy(hcif.axis[AXIS_PHI].id,			"phi" );
	strcpy(hcif.axis[AXIS_PHI].type,		"rotation" );
	strcpy(hcif.axis[AXIS_PHI].equipment,		"detector" );
	strcpy(hcif.axis[AXIS_PHI].depends_on,		"." );
	hcif.axis[AXIS_PHI].vector[0]			= 1.0;
	hcif.axis[AXIS_PHI].vector[1]			= 0.0;
	hcif.axis[AXIS_PHI].vector[2]			= 0.0;

	strcpy(hcif.axis[AXIS_OMEGA].id,		"omega" );
	strcpy(hcif.axis[AXIS_OMEGA].type,		"rotation" );
	strcpy(hcif.axis[AXIS_OMEGA].equipment,		"detector" );
	strcpy(hcif.axis[AXIS_OMEGA].depends_on,	"." );
	hcif.axis[AXIS_OMEGA].vector[0]			= 0.0;
	hcif.axis[AXIS_OMEGA].vector[1]			= 1.0;
	hcif.axis[AXIS_OMEGA].vector[2]			= 0.0;

	strcpy(hcif.axis[AXIS_CHI].id,			"chi" );
	strcpy(hcif.axis[AXIS_CHI].type,		"rotation" );
	strcpy(hcif.axis[AXIS_CHI].equipment,		"detector" );
	strcpy(hcif.axis[AXIS_CHI].depends_on,		"." );
	hcif.axis[AXIS_CHI].vector[0]			= 0.0;
	hcif.axis[AXIS_CHI].vector[1]			= 0.0;
	hcif.axis[AXIS_CHI].vector[2]			= 1.0;

	strcpy(hcif.axis[AXIS_THETA].id,		"twotheta" );
	strcpy(hcif.axis[AXIS_THETA].type,		"rotation" );
	strcpy(hcif.axis[AXIS_THETA].equipment,		"detector" );
	strcpy(hcif.axis[AXIS_THETA].depends_on,	"." );
	hcif.axis[AXIS_THETA].vector[0]			= 1.0;
	hcif.axis[AXIS_THETA].vector[1]			= 0.0;
	hcif.axis[AXIS_THETA].vector[2]			= 0.0;

	strcpy(hcif.axis[AXIS_DISTANCE].id,		"distance" );
	strcpy(hcif.axis[AXIS_DISTANCE].type,		"translation" );
	strcpy(hcif.axis[AXIS_DISTANCE].equipment,	"detector" );
	strcpy(hcif.axis[AXIS_DISTANCE].depends_on,	"." );
	hcif.axis[AXIS_DISTANCE].vector[0]		= 0.0;
	hcif.axis[AXIS_DISTANCE].vector[1]		= 0.0;
	hcif.axis[AXIS_DISTANCE].vector[2]		= -1.0;

	if ( strstr(hcif.detector_id, "ccd" )  ||  strstr(hcif.detector_id, "CCD" )) {
		strcpy(hcif.diffrn_detector.detector,		"CCD" );
		strcpy(hcif.diffrn_detector.type,		"marCCD system" );
	}
	else {
		strcpy(hcif.diffrn_detector.detector,		"Image Plate" );
		strcpy(hcif.diffrn_detector.type,		"mar345 IP system" );
	}

	hcif.diffrn_detector_element.center_x		= h345.xcen*pixelsize;
	hcif.diffrn_detector_element.center_y		= h345.ycen*pixelsize;

	/* marresearch extensions */
	if ( h345.mode == 0 )
		strcpy(hcif.diffrn_detector_mar_exposure.type, 	"dose");
	else
		strcpy(hcif.diffrn_detector_mar_exposure.type, 	"time");
	hcif.diffrn_detector_mar_exposure.time				= h345.time;
	hcif.diffrn_detector_mar_exposure.dose				= h345.doseavg;

	for (i=0; i<=AXIS_ROT_VER; i++ ) {
		hcif.diffrn_detector_mar_axis[i].angle_start 	= 
		hcif.diffrn_detector_mar_axis[i].angle_end 	= 
		hcif.diffrn_detector_mar_axis[i].angle_increment= 
		hcif.diffrn_detector_mar_axis[i].displacement	= 0.0; 
		hcif.diffrn_detector_mar_axis[i].oscillations	= 0;
	}

	hcif.diffrn_detector_mar_axis[AXIS_PHI].angle_start		= h345.phibeg;
	hcif.diffrn_detector_mar_axis[AXIS_PHI].angle_end  		= h345.phiend;
	hcif.diffrn_detector_mar_axis[AXIS_OMEGA].angle_start		= h345.omebeg;
	hcif.diffrn_detector_mar_axis[AXIS_OMEGA].angle_end  		= h345.omeend;
	hcif.diffrn_detector_mar_axis[AXIS_CHI].angle_start  		= h345.chi;
	hcif.diffrn_detector_mar_axis[AXIS_CHI].angle_end  		= h345.chi;
	hcif.diffrn_detector_mar_axis[AXIS_THETA].angle_end  		= h345.theta;
	hcif.diffrn_detector_mar_axis[AXIS_THETA].angle_start  		= h345.theta;
	hcif.diffrn_detector_mar_axis[AXIS_OMEGA].oscillations		= h345.omeosc;
	hcif.diffrn_detector_mar_axis[AXIS_PHI  ].oscillations		= h345.phiosc;

	hcif.diffrn_detector_mar_axis[AXIS_DISTANCE].angle_start    	= 0.0;
	hcif.diffrn_detector_mar_axis[AXIS_DISTANCE].angle_end      	= 0.0;
	hcif.diffrn_detector_mar_axis[AXIS_DISTANCE].displacement   	= h345.dist;

	for (i=0; i<=AXIS_DISTANCE; i++ ) {
		strcpy( hcif.diffrn_detector_mar_axis[i].axis_id,	hcif.axis[i].id);
		hcif.diffrn_detector_mar_axis[i].angle_increment	= hcif.diffrn_detector_mar_axis[i].angle_end -
									  hcif.diffrn_detector_mar_axis[i].angle_start;\
	}

	hcif.diffrn_detector_mar_exposure_dose.start			= h345.dosebeg;
	hcif.diffrn_detector_mar_exposure_dose.end			= h345.doseend;
	hcif.diffrn_detector_mar_exposure_dose.min			= h345.dosemin;
	hcif.diffrn_detector_mar_exposure_dose.max			= h345.dosemax;
	hcif.diffrn_detector_mar_exposure_dose.avg			= h345.doseavg;
	hcif.diffrn_detector_mar_exposure_dose.esd			= h345.dosesig;
	hcif.diffrn_detector_mar_exposure_dose.measurements		= h345.dosen;

	hcif.diffrn_detector_mar_adc.channel_a				= h345.adc_A;
	hcif.diffrn_detector_mar_adc.channel_b				= h345.adc_B;
	hcif.diffrn_detector_mar_adc.add_channel_a			= h345.add_A;
	hcif.diffrn_detector_mar_adc.add_channel_b			= h345.add_B;
	hcif.diffrn_detector_mar_adc.gain         			= h345.gain;
	hcif.diffrn_detector_mar_adc.multiplier         		= h345.multiplier;
	if ( h345.pixel_height == 100.f || h345.pixel_height == 0.1f )
		hcif.diffrn_detector_mar_adc.oversampling		= 1;
	else
		hcif.diffrn_detector_mar_adc.oversampling		= 2;

	hcif.diffrn_detector_mar_scan.scanmode        			= h345.size;
	hcif.diffrn_detector_mar_scan.number_of_pixels			= h345.pixels;
	hcif.diffrn_detector_mar_scan.number_of_pixels_larger_16_bit 	= h345.high;
	hcif.diffrn_detector_mar_scan.radial_offset			= h345.roff;
	hcif.diffrn_detector_mar_scan.tangential_offset			= h345.toff;
	hcif.diffrn_detector_mar_scan.pixel_height     			= h345.pixel_height;
	hcif.diffrn_detector_mar_scan.pixel_length     			= h345.pixel_length;

	hcif.diffrn_detector_mar_data.intensity_min			= h345.valmin;
	hcif.diffrn_detector_mar_data.intensity_max			= h345.valmax;
	hcif.diffrn_detector_mar_data.intensity_avg			= h345.valavg;
	hcif.diffrn_detector_mar_data.intensity_esd			= h345.valsig;
	hcif.diffrn_detector_mar_data.histogram_start    		= h345.histbeg;
	hcif.diffrn_detector_mar_data.histogram_end      		= h345.histend;
	hcif.diffrn_detector_mar_data.histogram_max      		= h345.histmax;

	for ( i=0; i<8; i++ )
		hcif.diffrn_detector_mar_gaps.gap[i]			= h345.gap[i];

	/* From the entire file name path, cut off directory */
	c = strrchr( f, '/' );
	if ( c == NULL ) 
		c = f;
	else	
		c++;

	/* From the remaining filename root + extension, cut off extension */
	strcpy( str, c);
	c = strrchr ( str, '.' );
	if ( c!=NULL ) {
		i=strlen( c );
		str[ strlen(str) - i ] = '\0';
	}
	strcpy( hcif.diffrn_scan.frame_id_start,	str);
	strcpy( hcif.diffrn_scan.frame_id_end,		str);

	for (i=strlen(str)-1;i>=0; i--) {
		if ( str[i] == '_' ) {
			str[i] = '\0';
			break;
		}
	}
	strcpy(hcif.crystal_id, 			str);
	hcif.diffrn_scan.frames 			= 1;

	strcpy(hcif.diffrn_measurement.device,		"oscillation camera");
	strcpy(hcif.diffrn_measurement.device_type,	"mar detector");
	strcpy(hcif.diffrn_measurement.method,		"phi scan");
	hcif.diffrn_measurement.sample_detector_distance = h345.dist;
	hcif.diffrn_measurement.number_of_axes		= 1;

	strcpy(hcif.diffrn_scan_axis[AXIS_PHI].axis_id,	"phi" );
	hcif.diffrn_scan_axis[AXIS_PHI].angle_start	= h345.phibeg;
	hcif.diffrn_scan_axis[AXIS_PHI].angle_range	= h345.phiend-h345.phibeg;
	hcif.diffrn_scan_axis[AXIS_PHI].angle_increment	= h345.phiend-h345.phibeg;

	strcpy(hcif.diffrn_scan_axis[AXIS_OMEGA].axis_id,"omega" );
	hcif.diffrn_scan_axis[AXIS_OMEGA].angle_start	= h345.omebeg;
	hcif.diffrn_scan_axis[AXIS_OMEGA].angle_range	= h345.omeend-h345.omebeg;
	hcif.diffrn_scan_axis[AXIS_OMEGA].angle_increment= h345.omeend-h345.omebeg;

	strcpy(hcif.diffrn_scan_frame_axis[AXIS_PHI].axis_id,	"phi" );
	hcif.diffrn_scan_frame_axis[AXIS_PHI].angle      	= h345.phiend;
	hcif.diffrn_scan_frame_axis[AXIS_PHI].displacement	= 0.0;

	strcpy(hcif.diffrn_scan_frame_axis[AXIS_OMEGA].axis_id,	"omega" );
	hcif.diffrn_scan_frame_axis[AXIS_OMEGA].angle      	= h345.omeend;
	hcif.diffrn_scan_frame_axis[AXIS_OMEGA].displacement	= 0.0;

	strcpy(hcif.diffrn_scan_frame_axis[AXIS_CHI].axis_id,	"chi" );
	hcif.diffrn_scan_frame_axis[AXIS_CHI].angle      	= h345.chi;
	hcif.diffrn_scan_frame_axis[AXIS_CHI].displacement	= 0.0;

	strcpy(hcif.diffrn_scan_frame_axis[AXIS_THETA].axis_id,	"twotheta" );
	hcif.diffrn_scan_frame_axis[AXIS_THETA].angle      	= h345.theta;
	hcif.diffrn_scan_frame_axis[AXIS_THETA].displacement	= 0.0;

	strcpy(hcif.diffrn_scan_frame_axis[AXIS_DISTANCE].axis_id,"distance" );
	hcif.diffrn_scan_frame_axis[AXIS_DISTANCE].angle      	= 0.0;
	hcif.diffrn_scan_frame_axis[AXIS_DISTANCE].displacement	= h345.dist;

}

/******************************************************************
 * Function: dtb2CIFHeader
 ******************************************************************/
void dtb2CIFHeader(char *prg, char *f, float *p)
{
int	i;

	for ( i=AXIS_SLIT_1_HOR;i<=AXIS_ROT_VER;i++) {
		strcpy(hcif.diffrn_detector_mar_axis[i].axis_id,axis_name[i]);
		hcif.diffrn_detector_mar_axis[i].displacement = p[i-1];
	}
}

/******************************************************************
 * Function: GetCIFData
 ******************************************************************/
int GetCIFData 	(char *prg, char *f, FILE *fp, unsigned int *i4_img)
{
	cif_read = 0;
	cbf_failnez (cbf_rewind_datablock(cif))
	cbf_failnez (cbf_find_category    	(cif, "array_data"))
	if ( cif_error ) {
		printe("Cannot find category: array_data");
		goto CIF_END;
	}
	cbf_failnez (cbf_find_column   		(cif, "array_id"))
	cbf_failnez (cbf_find_row      		(cif, hcif.array_id))
	cbf_failnez (cbf_find_column   		(cif, "data"))
        cbf_failnez (cbf_get_integerarrayparameters(
                                cif,
                                &cif_compression,
                                &cif_id,
                                &cif_element_size,
                                &cif_element_signed,
                                &cif_element_unsigned,
                                &cif_nelements,
                                &cif_minelement,
                                &cif_maxelement))
	if ( cif_error ) { 
		printe("Cannot get data array parameters");
		goto CIF_END;
	}

	cbf_failnez (cbf_get_integerarray (
			cif, 
			&cif_id, 
			(void *)i4_img,
			cif_element_size,
			cif_element_signed,
			cif_nelements,
			&cif_read))
	if ( cif_error ) { 
		printe("Cannot read data array");
		goto CIF_END;
	}

CIF_END:
	if ( verbose > 1 ) {
		printf("\n");
		printf("         Identifier   :\t %11d\n",cif_id);
		printf("         Compression  :\t %11d\n",cif_compression);
		printf("         Elements:    :\t %11d\n",cif_nelements);
		printf("         Min. element :\t %11d\n",cif_minelement);
		printf("         Max. element :\t %11d\n",cif_maxelement);
		printf("         Element size :\t %11d\n",cif_element_size);
		printf("         Signed       :\t %11d\n",cif_element_signed);
		printf("         Unsigned     :\t %11d\n",cif_element_unsigned);
		printf("         Elements read:\t %11d\n",cif_read);
	}

	/* Free the cbf */
	cbf_failnez (cbf_free_handle (cif))

	/* Input file has been closed my cbf_free_handle! */
	fp = NULL;

	return cif_read;
}

/******************************************************************
 * Function: PutCIFData
 ******************************************************************/
int PutCIFData 	(char *prg, char *op, char op_cbf, unsigned int *i4_img)
{
FILE	*fp		= NULL;
int	success		= 0;
int	mime	 	= MIME_HEADERS;
int	digest		= MSG_DIGEST;
int	encoding	= ENC_NONE;
int	bytedir		= 0;
int	cbforcif	= CBF;
int	term		= ENC_CRTERM | ENC_LFTERM;

	if ( op_cbf == 1 ) {
		cbforcif	= CBF;
		encoding	= ENC_NONE;
		term		= ENC_CRTERM | ENC_LFTERM;
	}
	else {
		cbforcif	= CIF;
		encoding	= ENC_BASE64;
		term		= ENC_LFTERM;
	}

	cif_compression 	= CBF_PACKED;
	cif_id			= hcif.binary_id;
	cif_element_size	= sizeof(int);
	cif_element_signed	= 0;
	cif_nelements		= hcif.array_structure_list[0].dimension *
				  hcif.array_structure_list[1].dimension;

	/* Save the binary data */
	cbf_failnez (cbf_set_integerarray (
			cif, 
			cif_compression, 
			cif_id, 
			(void *)i4_img,
			cif_element_size,
			cif_element_signed,
                        cif_nelements))

	fp = fopen (op, "w+b");
	if ( fp == NULL ) return 0;

	cbf_failnez (cbf_write_file (
			cif, 
			fp,
			1,
			cbforcif,
			mime | digest,
                        encoding | bytedir | term))
	if ( !cif_error ) success = 1;

	/* Free the cbf */
	cbf_failnez (cbf_free_handle (cif))

	if ( verbose > 1 ) {
		printf("\n");
		printf("%s: CIF-Header values:\n",prg);
		printf("         Pixels       :\t %d x %d\n",hcif.array_structure_list[0].dimension,hcif.array_structure_list[1].dimension);
		printf("         Identifier   :\t %11d\n",cif_id);
		printf("         Compression  :\t %11d\n",cif_compression);
		printf("         Elements:    :\t %11d\n",cif_nelements);
		printf("         Element size :\t %11d\n",cif_element_size);
		printf("         Signed       :\t %11d\n",cif_element_signed);
	}


	return success;
}
