
/*********************************************************************
 *
 * config.h
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

#define MAX_SCANMODE	8
#define MAX_IGNORE_ERR	20

typedef struct {

	float			dist_min;	/* Movement limits (steps) */
	float			dist_max;
	float			dist_def;
	float			thet_min;
	float			thet_max;
	float			thet_def;
	float			ome_min;
	float			ome_max; 
	float			ome_def;
	float			chi_min;
	float			chi_max;
	float			chi_def;
	float			phi_def;

	unsigned short		phi_speed;	/* Motor speeds */
	unsigned short		chi_speed;
	unsigned short		ome_speed;
	unsigned short		thet_speed;
	unsigned short		dist_speed;

	unsigned short		phi_steps;	/* Steps per deg */
	unsigned short		chi_steps;
	unsigned short		ome_steps;
	unsigned short		thet_steps;
	unsigned short		dist_steps;	/* Steps per mm */

	unsigned short		lock_speed;	/* Speed during lock plate */
	unsigned short		prelock_speed;	/* Speed before lock plate */

	unsigned short		units_time;
	unsigned short		units_dose;

	unsigned short		port;


	char			use_phi;	/* Flags */
	char			use_chi;
	char			use_ome;
	char			use_thet;
	char			use_dist;
	char			use_base;
	char			use_erase;
	char			use_xray;
	char			use_sound;
	char			use_image;
	char			use_dose;
	char			use_run;
	char			use_wave;
	char			use_shell;
	char			use_distmin;
	char			use_distmax;
	char			use_stats;
	char			use_zaxis;
	char			use_msg;
	char			use_html;
	char			use_txt;
	char			lambda_var;
	char			init_maxdist;
	char			memory;
	char			use_adc;
  	char			use_center;
	unsigned char		sets;
	unsigned char		colors;

	int			shutter_delay;
	int			spiral_max;
	int			center_imin;
	int 			flags;

	float			intensmin;
	float			intenswarn;
	float			dosemin;
	float			gain;
	float			spiral_scale;
	
	float			wavelength;
	float			kV;
	float			mA;
	float			polar;
	float			slitx;
	float			slity;
	float			xcen;
	float			ycen;
	char			filter[32];
	char			source[32];
	char			host[32];
	unsigned short		use_error	[ MAX_IGNORE_ERR ];

	/*
	 * Scan modi:
	 */


	unsigned short		size         [ MAX_SCANMODE ];
	unsigned short		diameter     [ MAX_SCANMODE ];
	unsigned short		adcoff	     [ MAX_SCANMODE ];
	int			adcadd       [ MAX_SCANMODE ];
	int			adcadd_A     [ MAX_SCANMODE ];
	int			adcadd_B     [ MAX_SCANMODE ];
	float        		scantime     [ MAX_SCANMODE ];
	float			erasetime    [ MAX_SCANMODE ];
	float			pixelsize    [ MAX_SCANMODE ];
	float			roff	     [ MAX_SCANMODE ];
	float			toff	     [ MAX_SCANMODE ];

} CONFIG;

#ifdef CONFIGGLOBAL
CONFIG	cfg;
#else
extern	CONFIG	cfg;
#endif

