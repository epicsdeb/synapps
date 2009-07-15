/***********************************************************************
 *
 * scan345: marglobals.h
 *
 * Copyright by:        Dr. Claudio Klein
 *                      X-ray Research GmbH, Hamburg
 *
 * Version:     4.0
 * Date:        30/10/2002
 *
 * History:
 *
 * Date		Version		Description
 * ---------------------------------------------------------------------
 * 30/10/02	4.0		com_use_center added
 * 15/06/00	2.2		Added feature COMMAND SCAN ADD x ERASE y
 *
 ***********************************************************************/

#define MAX_MODE 	8
#define MAX_SET 	4

#define OUT_SPIRAL	0
#define OUT_PCK         1
#define OUT_MAR		2
#define OUT_IMAGE	3
#define OUT_CIF  	4
#define OUT_CBF  	5

#define SINGLE_RUN      0
#define INDEX_RUN       1
#define MAD_RUN		2
#define MULTI_RUN       3
#define TOTAL_RUN	(MULTI_RUN+MAX_SET)

typedef struct _run_params {
	short int		stat;
	short int         	scanmode;
	unsigned short int	ffrm;
	unsigned short int	nfrm;
	unsigned short int	nosc;
	unsigned short int	timemode;
	unsigned short int	format;
	unsigned short int	diam;
	float			time;
	float			phis;
	float			dphi;
	float			iphi;
	float			dist;
	float			pixelsize;
	float			wave;
	char			dire		[128];
	char			root		[128];
} RUN_PARAMS;

typedef struct  {
        int add;
        int val;
} STRONG;


#ifdef MAR_GLOBAL
#define EXT
#else
#define EXT extern
#endif

EXT int		mar_number;
EXT int         mar_mode;
EXT int         mar_par1;
EXT int         mar_par2;
EXT int         mar_par3;
EXT int         mar_par4;
EXT int         mar_par5;
EXT int         mar_par6;
EXT int         mar_par7;
EXT int		stat_gap[8];
EXT char        mar_str[28];
EXT char	ips_command[128];

EXT char	buf[2048], str[1024];
EXT char        mar_host[32];

EXT char	working_dir[128];
EXT char	marhelp_dir[128];

EXT char	config_file[128];
EXT char	output_file[128];
EXT char	save_file[128];
EXT char	scanner_no[8];

EXT char	msg_file[128];
EXT char        image_root[64];
EXT char        image_dir[128];
EXT char        spiral_file[128];
EXT char        image_file[128];


EXT FILE	*fpout;
EXT FILE	*fpval;
EXT FILE	*fpstat;
EXT FILE	*fpspy;
EXT FILE	*fpmsg;
EXT char	*trntable;

EXT int		fdnd;
EXT int		mar_port;
EXT int		debug;
EXT int         total_run;

/*
 * The following are variables used as run parameters
 */
EXT	int		SET;
EXT	int		NSET;
EXT	RUN_PARAMS	run[TOTAL_RUN];
EXT 	struct {
	  short int	  	  stat;
	  unsigned short int	  nfrm;
	  unsigned short int	  ffrm;
	  unsigned short int	  fspi;
	  float	  phi;
	}               progress[TOTAL_RUN];

/*
 * The following are status indicators passed to GUI
 */

EXT	float	stat_pixelsize;
EXT	float	stat_xray_units;

EXT	float	stat_dist;
EXT	float	stat_omega;
EXT	float	stat_phi;
EXT     float   stat_theta;
EXT     float   stat_chi;

EXT	float	stat_phibeg;
EXT	float	stat_phiend;
EXT	float	stat_dphi;

EXT     float   stat_omebeg;
EXT     float   stat_omeend;
EXT     float   stat_dome;

EXT     int     stat_phiosc;
EXT     int     stat_omeosc;

EXT	float	stat_time;
EXT	float	stat_units;
EXT	float	stat_intensity;

EXT	float	stat_wavelength;
EXT	float	stat_multiplier;
EXT	int	stat_mode;
EXT	int	stat_max_count;
EXT	int	stat_n_images;
EXT	int	stat_n_passes;
EXT	int	stat_sweeps;

EXT	int 	stat_scanmode;
EXT	int 	stat_scanner_op;
EXT	int 	stat_scanner_msg;
EXT	int 	stat_scanner_control;
EXT	int 	stat_xray_shutter;
EXT	int 	stat_xform_msg;
EXT     short	stat_diameter;

EXT	char	stat_dir[80];
EXT	char	stat_fname[80];


EXT	float	com_kV;
EXT	float	com_mA;
EXT	float	com_slitx;
EXT	float	com_slity;
EXT	float	com_dist;
EXT     float   com_theta;
EXT     float   com_chi;

EXT	float	com_phibeg;
EXT	float	com_phiend;
EXT	float	com_dphi;

EXT     float   com_omebeg;
EXT     float   com_omeend;
EXT     float   com_dome;

EXT     int     com_phiosc;
EXT     int     com_omeosc;

EXT     float   com_dosebeg;                                 
EXT     float   com_doseend;                                 
EXT     float   com_doseavg;                                 
EXT     float   com_dosesig;                                 
EXT     float   com_dosemin;                                 
EXT     float   com_dosemax;                                 
EXT     int     com_dosen;                                   
                                    
EXT	float	com_time;
EXT	float	com_units;
EXT	float	com_intensity;
EXT	float	com_polar;

EXT	float	com_wavelength;
EXT	float	com_multiplier;
EXT	float	com_pixelsize;
EXT	float	com_diam;
EXT	int	com_mode;
EXT	int	com_format;
EXT	int	com_size;
EXT	int 	com_scanmode;
EXT	int 	com_scan_add;
EXT	int 	com_scan_erase;
EXT	char	com_use_spiral;
EXT	char	com_use_center;

EXT	char	com_filter[80];
EXT	char	com_source[80];
EXT	char	com_dir[80];
EXT	char	com_root[80];
EXT	char	com_remark[80];
EXT	char	com_file[80];

/*
 * The following are counters, variables, etc.  used during data collection
 */

EXT 	int	nb_size;
EXT	int	incontrol;
EXT	int	netcontrol;
EXT	int	mar_cmd;

EXT	int	dcop;
EXT	int	retrycnt;
EXT     int     totimg;
EXT     int     totpass;
EXT     int     init_op;
EXT	int	dc_stop;
EXT	float	delta;	
EXT	float	sum_xray_units;

EXT	int	fdmar;

/*
 * The following are hardware status bits from controller: Output Register
 */

EXT	int	ion_chamber;
EXT	int	distance_steps;
EXT	int	phi_steps;
EXT	int	omega_steps;

/*
 * The following are status indicators from controller Input Register
 */

EXT	int	xrayshutter_status;
EXT	int	lock_status;
EXT	int	erase_status;
EXT	int	position_status;
EXT	int	upper_status;
EXT	int	lower_status;
EXT	int	laser_status;
EXT	int	lasershutter_status;
EXT	int	transref_status;
EXT	char	erase_lamp_on_ok;

/*
 * The following are variables read from config file
 */

EXT     int     cur_mode;
EXT     short	cur_size;
EXT     short	cur_diameter;
EXT	short	cur_nfrm;
EXT     float	cur_pixelsize;
EXT     float	cur_intensmin;
EXT     int     cur_scantime;



#ifdef MAR_GLOBAL

int	nstrong		= 0;
int	nsat		= 0;
int    	verbose     	= 0;
char   	keep_spiral 	= 0;
char	big_scanner	= 1;

#else

EXT	char    keep_spiral;
EXT	char    big_scanner;
EXT	int     verbose;

#endif

