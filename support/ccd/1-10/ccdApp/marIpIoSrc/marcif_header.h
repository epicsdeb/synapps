/***********************************************************************
 *
 * cif_header.h
 *
 * Copyright by:        Dr. Claudio Klein
 *                      X-ray Research GmbH, Hamburg
 *
 * Version:     2.0
 * Date:        14/12/2001
 *
 * History:
 * Version    Date    	Changes
 * ______________________________________________________________________
 * 2.0          14/12/2001      Added dtb motors
 * 1.0          30/10/2000      Added CBF/imgCIF support
 *
 *
 ***********************************************************************/

#define N_SCAN_AXES		4

#define N_SCAN_FRAME_AXES	15
#define	AXIS_PHI		0
#define	AXIS_OMEGA		1
#define	AXIS_CHI		2
#define	AXIS_THETA		3
#define	AXIS_DISTANCE 		4
#define	AXIS_SLIT_1_HOR		5
#define	AXIS_SLIT_1_VER		6
#define	AXIS_SLIT_2_HOR		7
#define	AXIS_SLIT_2_VER		8
#define	AXIS_BEAMSTOP 		9
#define	AXIS_Z        		10
#define	AXIS_TRANS_HOR 		11
#define	AXIS_TRANS_VER 		12
#define	AXIS_ROT_HOR 		13
#define	AXIS_ROT_VER 		14

/*
 * General mmCIF items 
 */

typedef struct {
	char			creation_date		[32];
	char			creation_method		[32];
} AUDIT;

typedef struct {
	float			angle_alpha;
	float			angle_beta;
	float			angle_gamma;
	float			length_a;
	float			length_b;
	float			length_c;
	float			length_alpha_esd;
	float			length_beta_esd;
	float			length_gamma_esd;
	float			length_a_esd;
	float			length_b_esd;
	float			length_c_esd;
	float			volume;
	float			volume_esd;
	int			formula_units_Z;
} CELL;

typedef struct {
	char 			name_common		[32];
} CHEMICAL;

typedef struct {
	char			cell_refinement		[32];
	char			data_collection		[32];
	char			data_reduction		[32];
} COMPUTING;

typedef struct {
	char			colour			[32];
	char			description		[64];
	float			size_min;
	float			size_max;
	float			size_mid;
	float			size_rad;
} EXPTL_CRYSTAL;

typedef struct {
	float			ambient_temp;
	float			ambient_pressure;
} DIFFRN;

typedef struct {
	char			detector		[32];
	char			collimation  		[32];
	char			monochromator		[32];
	char			type         		[32];
} DIFFRN_RADIATION;

typedef struct {
	char			id			[16];
	float			polarisn_norm;
	float			polarisn_ratio;
	float			wavelength;
	float			wavelength_wt;
} DIFFRN_RADIATION_WAVELENGTH;

typedef struct {
	char			source			[32];
	char			type			[32];
	float			current;
	float			power;
	float			voltage;
} DIFFRN_SOURCE;


/*
 * CBF/imgCIF specific items
 */

typedef struct {
	char			compression_type	[32];
	char			encoding_type		[32];
	char			byte_order		[16];
} ARRAY_STRUCTURE;

typedef struct {
	int 			dimension;
	int 			index;
	int 			precedence;
	char 			direction		[16];
} ARRAY_STRUCTURE_LIST;

typedef struct {
	int 			index			[ 2];
	float			pixelsize		[ 2];
} ARRAY_ELEMENT_SIZE;

typedef struct {
	float			gain;
	char			linearity		[20];
	float			offset;
	float			scaling;
	float  			overload;
	float			undefined_value;
} ARRAY_INTENSITIES;

typedef struct {
	char			id			[32];
	char			type			[32];
	char			equipment		[32];
	char			depends_on		[32];
	float			vector			[ 3];
	float			offset			[ 3];
} AXIS;

typedef struct {
	char			detector 		[32];
	char			type     		[32];
	char			details  		[64];
	int			number_of_axes;
	float			deadtime;
} DIFFRN_DETECTOR;

typedef struct {
	float			center_x;
	float			center_y;
} DIFFRN_DETECTOR_ELEMENT;

typedef struct {
	char			details			[128];
	char			device        		[32];
	char			device_type   		[32];
	char			method        		[32];
	int			number_of_axes;
	float			sample_detector_distance;
} DIFFRN_MEASUREMENT;

typedef struct {
	int			frames;
	char			frame_id_start		[64];
	char			frame_id_end		[64];
	float			integration_time;
} DIFFRN_SCAN;

typedef struct {
	char			axis_id			[32];
	float			angle_start;
	float			angle_range;
	float			angle_increment;
	float			displacement_start;
	float			displacement_range;
	float			displacement_increment;
} DIFFRN_SCAN_AXIS;

typedef struct {
	float			integration_time;
	int  			frame_number;
} DIFFRN_SCAN_FRAME;

typedef struct {
	char			axis_id			[32];
	float			angle;
	float			displacement;
} DIFFRN_SCAN_FRAME_AXIS;

/* marresearch MARIP category */
typedef struct {
	char			id			[16];
	char			detector_id		[32];
} MARIP;

typedef struct {
	int 			gap			[8];
} MARIP_GAPS;

typedef struct {
	int			channel_a;
	int			channel_b;
	int			add_channel_a;
	int			add_channel_b;
	int			oversampling;
	float			multiplier;
	float			gain;
} MARIP_ADC;

typedef struct {
	int			scanmode;
	int			number_of_pixels;
	int			number_of_pixels_larger_16_bit;
	float			pixel_height;
	float			pixel_length;
	float			radial_offset;
	float			tangential_offset;
} MARIP_SCAN;

typedef struct {
	char			type 			[16];
	float			time;
	float			dose;
} MARIP_EXPOSURE;

typedef struct {
	char			axis_id			[32];
	float			angle_start;
	float			angle_end;
	float			angle_increment;
	int			oscillations;
	float			displacement;
} MARIP_AXIS;

typedef struct {
	float			start;
	float			end;
	float			min;
	float			max;
	float			avg;
	float			esd;
	int  			measurements;
} MARIP_EXPOSURE_DOSE;

typedef struct {
	float			intensity_avg;
	float			intensity_esd;
	int			intensity_min;
	int			intensity_max;
	int			histogram_start;
	int			histogram_end;
	int			histogram_max;
} MARIP_DATA;

typedef struct {

	char			entry_id		[32];
	char			array_id		[32];
	char			crystal_id		[32];
	char			diffrn_id		[32];
	char			detector_id		[32];
	char			scan_id			[32];
	char			element_id		[32];
	char			data_id			[32];
	char			mar_id			[32];
	char			frame_id		[64];
	int			binary_id;

	/* Category */		/* Element */	
	AUDIT			audit;
	CELL			cell;
	CHEMICAL		chemical;
	COMPUTING		computing;
	DIFFRN			diffrn;
	DIFFRN_RADIATION	diffrn_radiation;
	DIFFRN_RADIATION_WAVELENGTH diffrn_radiation_wavelength;
	DIFFRN_SOURCE   	diffrn_source;
	EXPTL_CRYSTAL		exptl_crystal;

	ARRAY_STRUCTURE		array_structure;
	ARRAY_STRUCTURE_LIST	array_structure_list[2];
	ARRAY_ELEMENT_SIZE	array_element_size;
	ARRAY_INTENSITIES	array_intensities;
	AXIS			axis			[N_SCAN_FRAME_AXES];
	DIFFRN_DETECTOR		diffrn_detector;
	DIFFRN_DETECTOR_ELEMENT	diffrn_detector_element;
	DIFFRN_MEASUREMENT	diffrn_measurement;
	DIFFRN_SCAN		diffrn_scan;
	DIFFRN_SCAN_AXIS	diffrn_scan_axis	[N_SCAN_AXES];
	DIFFRN_SCAN_FRAME	diffrn_scan_frame;
	DIFFRN_SCAN_FRAME_AXIS	diffrn_scan_frame_axis	[N_SCAN_FRAME_AXES];

	MARIP			diffrn_detector_mar;
	MARIP_ADC      		diffrn_detector_mar_adc;
	MARIP_GAPS		diffrn_detector_mar_gaps;
	MARIP_DATA		diffrn_detector_mar_data;
	MARIP_SCAN		diffrn_detector_mar_scan;
	MARIP_AXIS		diffrn_detector_mar_axis[N_SCAN_FRAME_AXES+1];
	MARIP_EXPOSURE		diffrn_detector_mar_exposure;
	MARIP_EXPOSURE_DOSE	diffrn_detector_mar_exposure_dose;
} CIF_HEADER;
