/***********************************************************************
 *
 * scan345: marxf.c
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
 * 30/10/02	4.0		Support for center offset correction added
 *				Support for 32-bit nb_code added
 * 02/09/02	3.3		Write DETECTOR mar345 into header
 * 10/06/02	3.2		Bug fix from version 3.0: nstrong not updated
 *				No overflows had been written!
 * 31/10/00	3.0		CBF/imgCIF format implemented
 * 15/06/00	2.2		Added feature COMMAND SCAN ADD x ERASE y
 *
 *
 ***********************************************************************/

#include	<stdio.h>
#include	<ctype.h>
#include	<string.h>
#include	<stdlib.h>
#include	<time.h>
#include	<math.h>
#include 	<unistd.h>
#include 	<fcntl.h>
#include 	<sys/types.h>
#include 	<sys/stat.h>

#include	"marcmd.h"
#include	"esd.h"
#include	"marglobals.h"
#include	"mararrays.h"
#include	"config.h"
#include	"version.h"
#include        <mar345_header.h>
#include        <mar300_header.h>
#include        <nb_header.h>

#define 		NB_SIZE         1024
#define 		BOXSIZE    	100
#define 		BOX_OFF         100

#define ltell(a)        lseek( a, 0, SEEK_CUR )
#define min0(a,b)       if ( b < a ) a = b
#define max0(a,b)       if ( b > a ) a = b

/*
 * Typedefs
 */

typedef union {
        unsigned int    bit32[9*1024];
        unsigned short  bit16[9*1024];
} PX_REC;

/*
 * Global variables
 */
MAR345_HEADER           h345;
char   			large_mem		= 1;
char			scan_in_progress 	= 0;
char			xf_in_progress 		= 0;
char                    op_in_progress 		= 0;
char			last_image[128] 	= { "\0"} ;
char			is_data			= 0;

int			last_total		= 0;
int			bytes2xfer;

static int		n1=0,n2=0,adcavg1,adcavg2,adcavg11,adcavg21;
static int		xf_histo[65600];
static int		hist_begin, hist_end, hist_max;
static int		add_A, add_B;
int			valmax, valmin;
int                     sp_pixels, xf_pixels;
int			maximum_pixels;
int                     maximum_block;
int                     maximum_bytes;
int			data_offset;
int                     current_pixel;
int			xform_status;

int			Imax;
float			AvgI,SigI; 
/*
 * Local variables
 */

static char		adc_channel = 0;
static unsigned short 	i2_record[MAX_SIZE];
static STRONG		strong_rec[MAX_SIZE/2];
static FILE		*fpnb		  = NULL;

static int 		precision   	  = 2;
static int 		fdspiral	  = -1;
static int 		iindex  	  = 0;
static int 		i_x0   	          = 0;
static int 		j_y0   	          = 0;
static int 		max_spiral_int    = 0;
static int		spiral_offset	  = 0;
static int 		ac_size  	  = 0;
static int 		i_rec    	  = 1;
static int 		xf_rec   	  = 0;
static int 		ns_rec   	  = 0;
static int		fdxf   		  = 0;
static int              swap_nb           = 0;
static char		start_with_A	  = 1;
static int		spiral_size;
static int 		total_pixels;
static int 		last_block;
static int              last_pixel;
static int		istrong;
static int		nskip;
static int		poff;
static int              nb_index;
static int 		lin_dxy[8];
static int		saturation;
static char		spiral_only;
static float		fract_intens;

static MAR300_HEADER    h300;
static MARNB_HEADER     nb;

static char		cc_file	[128];
static signed char      x_rec	[NB_SIZE];
static signed char      y_rec	[NB_SIZE];
static unsigned char    nb_rec	[NB_SIZE];
static PX_REC		px_rec;
static unsigned char    bit[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };

/*
 * External variables
 */
extern int		adc1,adc2;
extern int              stat_blocks_sent;
extern int 		com_scanmode;
extern char		input_skip_op;
extern char		skip_op;
extern char		keep_image;
extern char		do_xform;
extern int 		fdnb;
extern int		stat_scan_add;

extern int              edit_output;
extern int              nstrong;
extern int              nsat;
extern CONFIG		cfg;

/*
 * Local functions
 */
int  			mar_start_scan_readout	(int);
void			Transform		(int, int, unsigned short *);

static void 		output_image		(void);
static void 		ImageArray		(int, unsigned int);
static void 		PrintResults		();
static float		PrintStats		(int, int, int, int,unsigned int*);
						 
static void		get_header_values	(int);
static int  		output_header		(int);
static int		ReadNB			(FILE *);
static int		HistoMinMax		(void);
static int		CorrectCenter		(int);

/*
 * External functions
 */

extern int		get_status();
extern int		net_data();
extern int  		put_pck();
extern void		marError();
extern void     	swaplong();
extern void     	swapshort();


extern int              Putmar345Header	(int, MAR345_HEADER);
extern int              Putmar300Header	(int, int, MAR300_HEADER);
extern MAR345_HEADER    Getmar345Header	(FILE *);
extern MARNB_HEADER     GetmarNBHeader	(FILE *);
extern int              PutCIFHeader    (char *, char *);
extern int              GetCIFData      (char *, char *, FILE *, unsigned int *);
extern void             mar3452CIFHeader(char *, char *, MAR345_HEADER );

/******************************************************************
 * Function: mar_start_scan_readout = starts data readout
				      Open files and read 1. nb_code record
 ******************************************************************/
int mar_start_scan_readout(int next_scan) 
{
int 		i, i_pix;
int             swap[2];
int             j_dj[8] = { -1, 0, 1,-1, 1,-1, 0, 1};
int             i_di[8] = { -1,-1,-1, 0, 0, 1, 1, 1};
extern char     martable_dir[128];
extern char     nbcode_file[128];

	/* 
	 * Initialize readout variables 
	 */
	n1 = n2 = adcavg1 = adcavg2 = adcavg11= adcavg21= 0;
	adc_channel		= 0;
	start_with_A		= 1;
	bytes2xfer		= 0;
	xform_status		= 0;
	scan_in_progress 	= 1;
	xf_in_progress 		= 1;
	do_xform 		= 1;
	stat_xform_msg		= 0;
	stat_blocks_sent 	= 0;
	current_pixel 		= 0;
        sp_pixels               = 0;
        xf_pixels               = 0;
        valmin                  = 999999;
        last_pixel              = -1;
	last_block    		= -1;
	poff			= 0;
	data_offset		= 0;
	add_A			= cfg.adcadd_A[ (int)stat_scanmode ];
	add_B			= cfg.adcadd_B[ (int)stat_scanmode ];
	saturation		= 1000000;
	stat_pixelsize		= cfg.pixelsize[ (int)stat_scanmode ];

	if ( strncmp( image_file, spiral_file, strlen(spiral_file) )== 0 ) 
		spiral_only = 1;
	else
		spiral_only = 0;

	fdxf   			= 0;

	if ( fdspiral >= 0 ) {
		close( fdspiral );
		fdspiral = -1;
	}
		
	/* 
	 * Initialize xform variables 
	 */
	spiral_offset	= 0;

	/* Use the ADC values from controller */
	if ( cfg.use_adc == 1 ) {
		spiral_offset = cfg.adcoff[(int)stat_scanmode];
	}

	max_spiral_int 	= 0;
	istrong        	= 0;
	xf_rec         	= 0;
	iindex         	= 1024;
        nb_index       	= 0;


	ac_size        = cfg.size[ (int)stat_scanmode ];
	total_pixels   = ac_size * ac_size;

        /*
         * Open neighbour code file
         */
        if ( fpnb != NULL ) {
		fclose( fpnb );
		fpnb = NULL;
	}

	if ( stat_scanmode > 3 ) { 
#ifdef VMS
        	sprintf( nbcode_file,"%smar3450.%s", martable_dir, scanner_no);
        	sprintf(  cc_file,"%scenter3450.%s", martable_dir, scanner_no);
#else
        	sprintf( nbcode_file,"%s/mar3450.%s", martable_dir, scanner_no);
        	sprintf(  cc_file,"%s/center3450.%s", martable_dir, scanner_no);
#endif
	}
	else {
#ifdef VMS
        	sprintf( nbcode_file,"%smar2300.%s", martable_dir, scanner_no);
        	sprintf(  cc_file,"%scenter2300.%s", martable_dir, scanner_no);
#else
        	sprintf( nbcode_file,"%s/mar2300.%s", martable_dir, scanner_no);
        	sprintf(  cc_file,"%s/center2300.%s", martable_dir, scanner_no);
#endif
	}

        if ( NULL == (fpnb  = fopen( nbcode_file, "r" ))) {
                marError( 1100, 0 );
                do_xform        = 0;
                keep_image      = 0;
        }

        /*
         * Read nb header.
         */

        nb = GetmarNBHeader( fpnb );

        /*
         * Neighbour code header okay ?
         */

        if ( nb.mode < 1 ) {
                marError( 1101, 0 );
                do_xform        = 0;
                keep_image      = 0;
        }

        /*
         * Is byte swapping required ?
         */

        if ( nb.byteorder != 1234 ) {
                swap_nb = nb.byteorder;
                swap[0] = swap_nb;
                swaplong( swap, 4 );
                swap_nb = swap[0];
                nb.byteorder = 1234;
                if ( swap_nb != 1234 ) {
                        marError( 1102, 0 );
                        do_xform        = 0;
                        keep_image      = 0;
                }
                swap_nb = 1;
        }

        /*
         * Is the serial no. of nbcode identical to the one
         * defined by $MAR_SCANNER_NO ?
         */

        if ( atoi( scanner_no ) != nb.scanner ) {
                marError( 1103, 0 );
        }

        /*
         * Is there the desired scanning mode ?
         */
        i_rec = -1;
        for ( i=0; i<=4; i++ ) {
                /* Size is identical, use this mode ... */
                if ( nb.size[i] == ac_size ) {
                        i_rec = nb.fpos[i];
                        i_x0  = nb.x[i];
                        j_y0  = nb.y[i];
			i_pix = nb.pixels[i];
                        poff  = nb.skip[i] + (int)cfg.toff[(int)stat_scanmode];
			nskip = poff-1;
                        maximum_pixels = nb.pixels[i] + nb.skip[i];
                        maximum_block  = (int)(maximum_pixels/8192. + 0.999);
                        maximum_bytes  = maximum_block * 16386;
		
			/* When dealing with odd no. of pixels in preturn
			 * the ADC offset for channels A and B must be
			 * exchanged
			 */
			if ( nb.skip[i] % 2 != 0 ) {
				adc_channel 	= 1;
				start_with_A	= 0;
			}
                        break;
                }
        }

	/* 16-bit or 32-bit code ? */
	if ( nb.scale > 65535 )
		precision	= 4;
	else
		precision	= 2;

	/* 
	 * When applying offset, take only a fraction when using multiple
	 * sampling 
	 */

        if ( i_rec < 0 ) {
                marError( 1105, 0 );
                do_xform        = 0;
                keep_image      = 0;
                i_rec           = 0;
        }

	/* 
	 * Create transformed image file 
	 */
	if ( keep_image && !spiral_only ) {

		sprintf( buf, "%s.tmp", image_file );
		if ( com_format == OUT_CIF || com_format == OUT_CBF ) {
			fdxf = 0;
		}
		else {
			fdxf = creat( 	buf,   0666  );

			if ( fdxf == -1 ) {
					marError( 1110, 0 );
					do_xform = 0;
					fdxf     = 0; 
			}
			/* Close file after creation */
			else {
				close( fdxf );
				fdxf	= 0;
			}

			/*
			 * Open created image file
			 */
			fdxf = open( buf, O_RDWR );    /* Mode = O_RDWR (=2) */
			if(  fdxf == -1 ) {
				marError( 1111, 0 );
				do_xform = 0;
			fdxf     = 0; 
			}
		}
	}
	else {
		if ( !spiral_only )
			do_xform    = 0;
		keep_spiral = 1;
	}

	/* If we write only spirals, free memory and skip next lines */
	if ( keep_image == 0 ) {
		goto NEXT_1;
	}

	for(i = 0; i < 8; i++)
		lin_dxy[i] = j_dj[i] * ac_size + i_di[i];

	/* 
	 * Initialize memory for image arrays... 
	 */
	if ( stat_scan_add == 0 ) {
		memset( (char *)u.i4_img, 0, sizeof(int)*total_pixels);
	}
	memset( (char *)xf_histo, 0, sizeof(int)*65600 );

	for ( i=0; i<MAX_SIZE; i++ ) {
		i2_record[i] = 0;
	}

	for ( i=0; i<MAX_SIZE/2; i++ ) {
		strong_rec[i].val = 0;
		strong_rec[i].add = 0;
	}

	/* 
         * Go to starting position (i_rec) in neighbor code
	 */
NEXT_1:
        if ( do_xform )
                fseek( fpnb, i_rec, SEEK_SET);

	if ( debug & 0x40 ) {
		fprintf(stdout,"\nscan345:  transformation parameters:\n");
		fprintf(stdout,"\tCurrent scan mode:       %d\n",stat_scanmode);
		fprintf(stdout,"\tStarting x:              %d\n",i_x0);
		fprintf(stdout,"\tStarting y:              %d\n",j_y0);
		fprintf(stdout,"\tSpiral offset:           %d\n",poff);
		fprintf(stdout,"\tFirst rec at:            %d\n",i_rec);
		fprintf(stdout,"\tNo. of pix/rec:          %d\n",ac_size);
		fprintf(stdout,"\tTotal no. of pixels:     %d\n",maximum_pixels);
		fprintf(stdout,"\tTotal no. of blocks:     %d\n",maximum_block);
		fprintf(stdout,"\tScale:                   %1.3f\n",nb.scale);
		fprintf(stdout,"\tPrecision:               %s\n",precision==2?"16-bit":"32-bit");
	}
#ifdef DEBUG
#endif

	/* 
	 * We cannot write transformed image, so we try to write
	 * spiral images...
	 */

	if ( do_xform == 0 && keep_image == 1 ) {
		sprintf(str, "scan345: Image cannot be transformed!\n");
		fprintf( stdout, str );
		fprintf(  fpout, str );
		sprintf(str, "scan345: Trying to write spiral files...!\n");
		fprintf( stdout, str );
		fprintf(  fpout, str );
		keep_spiral = 1;
	}

	/* 
	 * Write image header first time
	 */
	if ( do_xform == 1 && fdxf > 0 ) {
		if ( output_header( (int)1 ) != 1 ) {
			do_xform    = 0;
			i           = close( fdxf );
			keep_spiral = 1;
			fdxf        = 0;
		}
	}

	/* 
	 * In keep spiral mode, write spiral header to output
	 */

	if ( output_header( 0 ) != 1 ) {

		/* 
		 * Spiral file could not be opened, so we have to abort
		 * data collection
		 */

		if ( fdxf ) {
			i = close( fdxf );
			fdxf = 0;
		}

		i = close(fdspiral);
		fdspiral = -1;
		return( 0 );
	}

	/* 	
	 */

	spiral_size	= 4096 + maximum_block*16384;

	return( 1 );

}

/******************************************************************
 * Function: Transform = transforms spiral data
 ******************************************************************/
void Transform( int i_sp, int block_no, unsigned short *spiral )
{
register unsigned int	adc,adcadd;
register int            sp_index, lin_pointer, nb_pointer;    
register int            pixel_intensity;                      
register int            i;                                    

static   unsigned short *s_p;                   
static   unsigned char  *n_p, nb_byte;                        
static   unsigned int   *v_p;                                 
static   signed   char 	dx, dy, *x_p, *y_p;

int			sp_offset;
static int		n_nb;
double			dtmp;

extern int              marTask();                            

	/*
	 * Used parameters and their values:
	 * --------------------------------- 
	 * stat_blocks_sent   = block counter read by this program 
	 * current_pixel   = pixel counter read by this program 
	 */

	if ( scan_in_progress == 0 || op_in_progress == 1 ) return;

	/* 
	 * Block of data has been read successfully... 
	 */

	/* Pixel offset in some modes 5, 6, 7 ... */
	if ( poff > 0 ) {
		sp_offset = poff;
		if ( i_sp < poff ) {
			poff -= i_sp;
			current_pixel += i_sp;
			goto OUTPUT;
		}
		i_sp -= poff;
		poff  = 0;
		current_pixel = nskip;
	}
	else
		sp_offset = 0;

	last_block     = stat_blocks_sent;
	last_pixel     = current_pixel;

	/*
	 * Use new ADC values for this scan. Use first data block
	 */
	if ( block_no <= 1 ) {
		adc1 = esd_stb.adc1;
		adc2 = esd_stb.adc2;
	}

        /*                                                    
         * Swap bytes                                         
         */                                                   
                                                              
#if ( __sgi111 || HPUX111 )                              
        swapshort( spiral+sp_offset, i_sp*sizeof(short) );              
#endif
                                                              
#ifdef DEBUG111
	s_p = spiral + sp_offset;
	for ( i=0; i<i_sp; i++, s_p++ )
		*s_p = 100;
	s_p = spiral + sp_offset;
	for ( i=0; i<i_sp; i++, s_p++ )
		if ( *s_p > 10000 ) fprintf( fpout, "#=%8d: %5d  bl=%6d lk=%d\n",current_pixel+i,*s_p,last_block,stat_plate_locked?1:0);
#endif

	/* Write data block into spiral file... */
	if (keep_spiral) {
		i =  write(fdspiral, spiral+sp_offset, i_sp*sizeof(short) );

		if ( i == -1 ) {
			/* Error writing spiral record... */
			fprintf( fpout, "scan345: Error writing spiral block %d\n", stat_blocks_sent );

			if ( do_xform == 0 ) {
				close( fdspiral );
				fdspiral = -1;
				xform_status = -1;
				return;
			}
		}
		if ( !keep_image )
			current_pixel += i_sp;
	} 

	/* Ignore first 2 pixels (set to value of third) */
	if ( sp_pixels == 0 ) {
		for ( i=0; i<2; i++ ) { 
			spiral[i+sp_offset] = spiral[2+sp_offset];
		}
	}
	

	/*
	 * Monitor progress of transformation
	 */
#ifdef __osf__
	dtmp = block_no/(double)maximum_block;
	stat_xform_msg = (int)( 100*dtmp);
#else
	stat_xform_msg = (int)( 100*(block_no/(float)maximum_block));
	stat_xform_msg = (int)( 100*(current_pixel/(float)maximum_pixels));
#endif

	/******************************************************
	 ******************************************************
	 ** 						     **
	 ** Transform pixels for the current spiral_buffer   **
	 ** 						     **
	 ******************************************************
	 ******************************************************/

	/* Reset xform_status to 1 */
	xform_status   = 1;

	/* If we don't want to write transformed image, skip while loop... */
	if ( do_xform == 0 ) {
		xform_status = 2;
		if ( last_pixel >= maximum_pixels ) {
			xform_status = 3;
		}
	}

        if ( xform_status != 1 || sp_pixels >= maximum_pixels ) goto OUTPUT;
                                                              
        /* Set pointer to first element in spiral array  and reset counter */
        s_p = spiral + sp_offset;               /* Spiral pixel value */  
        sp_index = 0;                                         

        /*                                                    
         * Read neighbour code for 1024 pixels                
         */                                                   
NEXT_NB:                                                      
        if ( nb_index == 0 ) {                                
                if ( ( nb_index = ReadNB( fpnb ) ) < 1 ) {
                        xform_status = 3;                     
                        goto OUTPUT;                          
                }                                             
                                                              
                /* Set pointer to first element in array */   
                x_p = x_rec;                /* Pixel x coordinate */
                y_p = y_rec;                /* Pixel y coordinate */
                n_p = nb_rec;               /* Neighbour byte */
                v_p = px_rec.bit32;         /* Neighbour contributions */
		n_nb = 0;
        }                                                     
                                                              
        /*                                                    
         * Loop 1: Transform pixels for this data block       
         */                                                   
                                                              
        while( sp_index < i_sp ) {                            
                                                              
                /* Dereference pointers */                    
		if ( n_nb < 0 ) n_nb = 0;
                pixel_intensity = (int)*s_p;                          
                dx           	= *x_p;
                dy           	= *y_p;                          
                nb_byte      	= *n_p;                          
                                                              
                /*                                            
                 * Get maximum intensity of raw data          
                 */                                           
                                                              
                max0( max_spiral_int, pixel_intensity );      

                /*                                            
                 * Push out values we cannot trust, scale and add offset 
                 */                                           
                                                              
                if ( pixel_intensity > cfg.spiral_max ) 
			pixel_intensity *= 100;
		else {
			/* Apply ADC offsets as obtained from STATUS block */
			if ( cfg.use_adc ) {
				if ( adc_channel == 1 ) {
					adc = adc2;
					adcadd = add_B;
				}
				else {
					adc = adc1;
					adcadd = add_A;
#ifdef DEBUG1
					n1++;
					adcavg1 += ( pixel_intensity - adc );
					adcavg11+= ( pixel_intensity - adc2);
					if ( n1 > 100000 ) {
						n1 = adcavg1 = adcavg11= 0;
					}
#endif
				}

				pixel_intensity += (spiral_offset + adcadd - adc);
				
			}
			else {
				if ( adc_channel == 1 )
					adcadd = add_B;
				else 
					adcadd = add_A;
				pixel_intensity += adcadd;
			}

			adc_channel = adc_channel ^ 0x01;

			pixel_intensity *= cfg.spiral_scale;
			if ( pixel_intensity < 0 ) pixel_intensity = 0;
		}
                                                              
                /*                                            
                 * In neighbor code, nb.scale is 100% contribution.
                 */                                           
                                                              
                fract_intens = pixel_intensity / nb.scale;    
                                                              
                /*                                            
                 * Actual x, y coordinates are ...            
                 */                                           
                                                              
                i_x0 += dx;                                   
                j_y0 += dy;                                   
                                                              
                /*                                            
                 * Main contribution: position in linear array
                 */                                           
                                                              
                lin_pointer = i_x0 * ac_size + j_y0 - 1;      
                                                              
                /*                                            
                 * Put pixel value into image array           
                 */                                           
                                                              
                if ( lin_pointer >  0 && lin_pointer <= total_pixels)
                        ImageArray( lin_pointer, *v_p );      
                                                              
                /* Next contrib pointer ... */                
                v_p++;                                        
                                                              
                /*                                            
                 * Cartesian neighbours:                      
                 *                                            
                 *         0  1  2                            
                 * bit j:  3     4                            
                 *         5  6  7                            
                 */                                           
                                                              
                for(i = 0; i < 8; i++) {                      
                                                              
                        /* Neighbour does NOT contribute */   
                        if ( (nb_byte & bit[i] ) == 0 ) continue;
                                                              
                        /* Neighbour does contribute */       
                        nb_pointer = lin_dxy[i] + lin_pointer;
                        if ( nb_pointer>0 && nb_pointer<=total_pixels)
                                ImageArray(  nb_pointer, *v_p );
                                                              
                        /* Next contrib pointer ... */        
                        v_p++;                                
                }                                             
                                                              
                /* Increase counters */                       
                sp_index++;                                   
                sp_pixels++;                                  
		current_pixel++;
                                                              
                /* Increase array pointers */                 
                s_p++;                                        
                x_p++;                                        
                y_p++;                                        
                n_p++;                                        

                /* Decrease counters */                       
		n_nb++;
                nb_index--;                                   

                if ( nb_index == 0 || n_nb >= NB_SIZE ) goto NEXT_NB;            

		if ( current_pixel >= maximum_pixels ) {
			xform_status = 3;
			goto OUTPUT;
		}
                                                              
        } /* End of loop 1: while ( sp_index < i_sp ) */      
        
OUTPUT:                                                      
	/* 
	 * Output image when last record of nb_code reached.
	 */
	if ( xform_status == 3 || ( current_pixel >= maximum_pixels && stat_blocks_sent > 0) ) {

#ifdef DEBUG
	if ( debug & 0x80 )
	printf("debug (marxf): start of output image xf=%d current_pixel=%d (%d) block=%d\n",
	xform_status,current_pixel,maximum_pixels,stat_blocks_sent);
#endif

		if ( current_pixel < maximum_pixels ) {
			current_pixel = maximum_pixels;
		}
		
		if (keep_spiral) {
			i = ltell( fdspiral );
			memset( (char *)spiral, 0, 8192);
			while ( ltell( fdspiral ) < spiral_size )
				i=write(fdspiral, spiral, 1*sizeof(short) );
		}
		output_image();
	}

	return;
}

/******************************************************************
 * Function: ImageArray = 
 ******************************************************************/
static void ImageArray( int addr, unsigned int i4_val)
{
	/*
	 * Get 32 bit intensity of this pixel
	 */

	u.i4_img[ addr ] = u.i4_img[ addr ] +
		 (int) ( (float)i4_val * fract_intens + 0.5 ); 

}

/******************************************************************
 * Function: output_image
 ******************************************************************/
static void output_image(void)
{
register unsigned int   *i4_arr;
register unsigned int	i4_val;
register unsigned int	xx,yy;
time_t			now;
float			avgN,avgW,avgO,avgS;
int			k;
unsigned int		cenmax,cenmin,no, ns, reclen, len, i,j, nhist, pos;
double			Isum, diff;
extern void		rotate_i4	(unsigned int *, int);
extern void		rotate_i4_anti	(unsigned int *, int);

#ifdef DEBUG
	if (debug & 0x40  )
	printf("debug (marxf): output image started %d\n",scan_in_progress);
#endif

	if ( scan_in_progress == 0 || op_in_progress ) return;

	xf_in_progress	= 0;
	op_in_progress 	= 1;

#ifdef FORMAT_TEST
	for ( istrong=0, i=0; i<80; i++ ) {
		pos = (i+605)*ac_size+500+i;
		for ( j=0; j<100; j++, pos++ ) {
			i4_val = 70000+i*10000+j*100;
			while ( i4_val > 65535 ) {
				if ( ch_image[pos] < 250 ) 
					ch_image[ pos ]++;
				i4_val -= 65535;
			}
			i2_image[ pos ] = i4_val; 
			istrong++;
		}
	}
#endif
	nstrong = istrong;

	/* 
	 * Only spiral file has been produced, print result and return
	 */
	if ( do_xform == 0 || keep_image == 0 ) {
		if ( keep_spiral )
			PrintResults( 0 );
		goto ALL_DONE;
	}

	/* 
	 * Everything is fine, so go ahead
	 * and write image to output ...
	 */

	/* Rotate image by 90 deg. when working in old image format */
	if ( com_format == OUT_IMAGE ) {
		rotate_i4( u.i4_img, ac_size );
	}

	/* 
	 * Apply offset
         */
	i = CorrectCenter( ac_size );

	/* 
	 * Use pointers instead of arrays. This is a bit faster...
	 */
	
	i4_arr = u.i4_img;

	/* 
	 * Init. some variables
	 */
	cenmin 		= ac_size/2 - 5;
	cenmax 		= ac_size/2 + 5;
	Isum        	= 0.0;
	Imax        	= 0;
	nhist	    	= 0;
	nsat = ns_rec = no = ns = 0;

	/*
	 * The overflow record is written in standard image format
	 * In PCK mode, the overflow records follow the header, i.e.
	 * the first overflow record is: 2
	 * In IMAGE mode, the overflow records follow the image array,
	 * i.e. the first overflow record is: 1+ac_size 
	 */
	if ( com_format == OUT_MAR ) {
                reclen = 8;                                   
		if ( fdxf )
			lseek(fdxf, 4096, SEEK_SET);                  
        }                                                     
        else {                                                
                reclen = ac_size/4;                           
		if ( fdxf ) {
			if ( com_format == OUT_PCK )
				lseek(fdxf, 2*ac_size, SEEK_SET);
			else
				lseek(fdxf, 2*ac_size + 2*total_pixels, SEEK_SET);
		}
        }                                                     
        
	/* 
	 * Loop 1: Go through image and look for values exceeding 16 bits
	 */
	for ( i=j=0; i< total_pixels ; i++, i4_arr++ ) {

		i4_val = *i4_arr;

		/*
	 	 * The maximum intensity in the transformed image is...
	 	 */

		if ( i4_val > Imax && i4_val < 250000 ) {
			xx = i%ac_size;
			yy = i/ac_size;
			/* Exclude inner 5 pixels from Imax calculation */
			if ( xx<cenmin || xx >cenmax || yy<cenmin || yy > cenmax )
				Imax = i4_val;
		}

		/*
		 * Add to intensity histogram
		 */

		if ( i4_val > 0 && i4_val <= 65535 ) {
			Isum += i4_val;
			nhist++;
			xf_histo[ i4_val ]++;
                        min0( valmin, (int)i4_val );          
		}

		/*
	 	 * Is overflow counter for pixel i > 0 ?
	 	 */

		if ( i4_val <= 65535 ) continue;
		
		/* 
		 * The next pixel value is > 16 bits
		 */

		strong_rec[no].add = i;
		strong_rec[no].val = i4_val;


		/* In IMAGE output mode, add +1 to address!!! */
		if ( com_format == OUT_IMAGE || com_format == OUT_PCK )
			strong_rec[no].add++;

		/*
		 * Saturated pixels and sum up intensities
		 */
		if ( strong_rec[no].val > 250000 ) {
			nsat++;
			strong_rec[no].val = 999999;
			*i4_arr = 999999;
		}
		else {
			nhist++;
			xx = i%ac_size;
			yy = i/ac_size;
			if ( xx<cenmin || xx >cenmax || yy<cenmin || yy > cenmax )
				max0( Imax, strong_rec[no].val );
			Isum += strong_rec[no].val;
		}


		/*
		 * Increment counters
		 */

		no++;
		ns++;

		/*
		 * Record is full, so write it to output...
		 */
		if ( no == reclen && do_xform && keep_image ) {
			ns_rec++;
			len = reclen*sizeof(STRONG);
			if ( fdxf ) {
				if ( write( fdxf, (char *)strong_rec, len) < len ) {
					marError( 1112, ltell( fdxf) );
					do_xform = 0;
				}
			}
			no = 0;
		}
	}

	/* Version 3.2: bug for overflows fixed: nstrong needs to be updated*/
	nstrong = ns;

	/*
	 * Fill last oveflow record with zeroes and write to output
	 */
	if ( no > 0 && do_xform && keep_image && !spiral_only ) {
		for ( i=no; i<reclen; i++ ) {
			strong_rec[i].add = 0;
			strong_rec[i].val = 0;
		}
		ns_rec++;
		len = reclen*sizeof(STRONG);
		if ( fdxf ) {
			if ( write( fdxf, (char *)strong_rec, len) < len ) {
				marError( 1112, ltell( fdxf) );
				do_xform = 0;
			}
		}
	}
	if ( fdxf )
		pos = ltell( fdxf );


	/* Get average I */
	if ( nhist > 0 ) 
		AvgI = Isum/(double)nhist;
	else
		AvgI = 0.0;

	/* Get Sigma if Intensity */
	i4_arr = u.i4_img;
	SigI   = 0.0;

	/*
	 * Go through high intensity array to get Avg, sigma 
	 */
	for ( i=j=0; i< total_pixels ; i++, i4_arr++ ) {
		i4_val = *i4_arr;
		if ( i4_val == 0 ) continue;
		if ( i4_val > 250000 ) {
			if ( stat_scanmode < 4 )
				i4_val = 131072;
			else
				i4_val = 65535;
		}
		diff 	= i4_val - AvgI;
		diff   *= diff;
		SigI   += diff;
		j++;	
	}
	if ( j>1 )
		SigI = sqrt( SigI/(double)(j - 1.0) );

	/*
	 * Since istrong and ns_rec are known by now, we
	 * can write the image header, next...
	 */
	if ( fdxf )
		if ( output_header( 1 ) != 1 ) {
			do_xform = 0;
		}

	PrintResults( 1 );

        if ( !do_xform ) goto ALL_DONE;                       
        if ( input_skip_op || skip_op || spiral_only ) goto SKIP1;                       
	/* Print STATS, if configured */
	if ( cfg.use_stats && do_xform ) {
		sprintf( str, "%6d %7.1f%6.1f  ", Imax, AvgI, SigI);
		if ( fpstat != NULL )
			fprintf( fpstat, str);

		j = (int)(cos( 3.14159 / 4.0 ) * (ac_size/2 - BOX_OFF) ); 

		/* NORDWEST */
		xx = yy = ac_size/2. - BOXSIZE/2 - j;

		avgW = PrintStats( 0, xx, yy, BOXSIZE, u.i4_img);

		/* WEST */
		xx  = BOX_OFF   - BOXSIZE/2;
		yy  = ac_size/2 - BOXSIZE/2;
		avgW = PrintStats( 1, xx, yy, BOXSIZE, u.i4_img);

		/* NORD */
		xx  = ac_size/2 - BOXSIZE/2;
		yy  = BOX_OFF   - BOXSIZE/2;
		avgN = PrintStats( 1, xx, yy, BOXSIZE, u.i4_img);

		/* OST  */
		xx  = ac_size   - BOXSIZE/2 - BOX_OFF;
		yy  = ac_size/2 - BOXSIZE/2;
		avgO = PrintStats( 1, xx, yy, BOXSIZE, u.i4_img);

		/* SUED */
		xx  = ac_size/2 - BOXSIZE/2;
		yy  = ac_size   - BOXSIZE/2 - BOX_OFF;
		avgS = PrintStats( 1, xx, yy, BOXSIZE, u.i4_img);

		if ( avgO > 0.0 ) avgW /= avgO;
		else		  avgW  = 0.0;
		if ( avgS > 0.0 ) avgN /= avgS;
		else		  avgN  = 0.0;
/*
		sprintf( str, "%8.4f%8.4f  ", avgW, avgN);
*/
		avgW = 100.0-avgW*100.;
		avgN = 100.0-avgN*100.;
		if ( avgW < 0.0 )
			sprintf( str, "%8.3f" , avgW);
		else
			sprintf( str, " +%6.3f", avgW);
		if ( fpstat != NULL )
			fprintf( fpstat, str);

		if ( avgN < 0.0 )
			sprintf( str, "%8.3f  " , avgN);
		else
			sprintf( str, " +%6.3f  ", avgN);

		if ( fpstat != NULL )
			fprintf( fpstat, str);
		avgS = PrintStats(99, xx, yy, BOXSIZE, u.i4_img);
	}

        /* From image display we got some new information for the
         * header which we may will be written again into header */

	i=HistoMinMax();

	/*
	 * Write CBF/imgCIF output
	 */

	if ( com_format == OUT_CIF || com_format == OUT_CBF ) {

		/* Fill CIF header structure */
		get_header_values( 1 );
		sprintf( str, "scan345 (V %s)",MAR_VERSION);
		mar3452CIFHeader( str, image_file, h345);

		/* Write CIF header */
		if ( PutCIFHeader("scan345", image_file) < 1 ) {
			marError( 1115, 0 );
			do_xform = 0;
		}
		else {
			/* Write CIF data array */
			i= PutCIFData( "scan345", image_file, (char)(com_format-OUT_CIF), u.i4_img);
			if ( i == 0 ) {
				marError( 1112, i );
				do_xform = 0;
			}
		}

	}

	/* All other formats: convert 32-bit to 16-bit array */
	else {
		i4_arr = u.i4_img;
		for ( k=0; k<total_pixels ; k++) {
			i4_val = *i4_arr++;
			if ( i4_val > 65535 )
				u.i2_img[k] = 65535;
			else 
				u.i2_img[k] = (unsigned short)i4_val;
		}
	}

	/*
	 * Write out the 16 bit image array.
	 * ---------------------------------
         * IMAGE format...                        
	 */

	if ( com_format == OUT_IMAGE ) {	

                lseek(fdxf, 2*ac_size, SEEK_SET);     

		j = write( fdxf, u.i2_img, 2*total_pixels );
		if ( (j < 2*total_pixels) == -1) {
			marError( 1112, j );
			do_xform = 0;
		}
                xf_pixels = j/2;                     
	}

	/*
	 * MAR345 format...
	 */

	else if ( com_format == OUT_MAR || com_format == OUT_PCK ) {
		/*
		 * Write PCK header following the overflow record...
		 */ 
                lseek(fdxf, pos , SEEK_SET);     

	    	sprintf(str,"\nCCP4 packed image, X: %04d, Y: %04d\n",ac_size,ac_size);
	    	if ( write(fdxf, str, strlen( str )) == -1 ) {
			marError( 1112, 2);
		}

		j = put_pck( u.i2_img, ac_size, ac_size, fdxf );

		if ( j == 0 ) {
			marError( 1112, 3);
			do_xform = 0;
		}
	}


SKIP1:
                                                              
        if ( fdxf && ( com_format != OUT_IMAGE && com_format != OUT_PCK ) ) 
                i = output_header( 1 );                  

	/*
	 * All done: close file, reset markers
	 */
ALL_DONE:
	/* When adding scans, we have to reverse some action
         * if there are overflows. We also have to rotate image
	 * arrays when working in mar300 formats 
	 */
	if ( com_scan_add > 0 && do_xform && fdxf ) {

		/* Convert 16-bits to 32-bits array */
		k=total_pixels-1;
		while ( k>=0 ) {
			i4_val 	= (unsigned int)u.i2_img[k];
			u.i4_img[k] = i4_val;
			k--;
		}

		/* High intensity pixels ? */
		if ( com_format == OUT_IMAGE )
			lseek(fdxf, 2*ac_size + 2*total_pixels, SEEK_SET);
		else if ( com_format == OUT_PCK )
			lseek(fdxf, 2*ac_size, SEEK_SET);
		else                                                 
			lseek(fdxf, 4096, SEEK_SET);                  

		for ( no=0; no<=nstrong; no++ ) { 
			i =  read(fdxf, strong_rec, 8 );
			if ( i != 8 ) { 
				break;
			}

			i 	= strong_rec[0].add;	
			i4_val 	= strong_rec[0].val;
								  
			/* In IMAGE output mode, take off -1 from address!!! */
			if ( com_format != OUT_MAR ) i--;

			if ( i < 0 || i >= (total_pixels-1) ) continue;

			if ( i4_val < 65535 )
				continue;
			u.i4_img[i] = i4_val;
		}

		/* Rotate image back when working in old image format */
		if ( com_format == OUT_IMAGE || com_format == OUT_PCK )
			rotate_i4_anti( u.i4_img, ac_size );
	} /* End of : if ( com_scan_add > 0 )  */

	/*
	 * Close and rename files
	 */
	if ( keep_image && !spiral_only ) {

		if ( fdxf ) {
			i = close( fdxf );
			fdxf = 0;
		}
		sprintf( buf, "%s.tmp", image_file );

		/* Remove temporary file */
		if ( input_skip_op || skip_op ) 
			remove( buf );

		/* Rename temporary file to image_file */
		else {
			rename( buf, image_file );
			strcpy( last_image, image_file );
		}
	}
	else if ( spiral_only ) {
		strcpy( last_image, image_file );
	}

	scan_in_progress = do_xform = 0;
	op_in_progress 	 = 0;

	/* 
	 * Close spiral file, if in use at all...
	 */

	if (keep_spiral) {
                i = output_header( 0 );                  
		close(fdspiral);

		/* Rename tmp to spiral file */
		sprintf( buf, "%s.tmp", spiral_file );
		rename(  buf, spiral_file );

		/* Compress spiral file */
		if ( cfg.use_image == 3 ) {
			sprintf( str, "packspiral %s &\n",spiral_file);
			system( str ); 
		}
		fdspiral = -1;
	}

#ifdef DEBUG
	if ( debug & 0x40 )
	printf("debug (marwh): output image all done\n");
#endif

	/* Open message file */
	time( &now );
	if ( fpmsg != NULL ) {
		fprintf(fpmsg,"scan345: OUTPUT IMAGE ENDED at %s",(char *)ctime( &now ));
		fflush( fpmsg );
	}

}

/******************************************************************
 * Function: output_header = writes header 
 ******************************************************************/
static int output_header(int mode) 
{
int	i;
int	fd;

	/* Spiral file output */

	if ( mode == 0 && fdspiral < 0 ) {

		/*
	 	 * Return, if spiral image should not be written 
	 	 */

		if ( keep_spiral == 0 ) return( 1 );

		sprintf( buf, "%s.tmp", spiral_file );

		fdspiral = creat( buf, 0666 ); 

		if ( fdspiral == -1 ) {
			marError( 1115, 0 );
			return( 0 );
		}
		else 
			close( fdspiral );

        	/*
         	 * Open created spiral file
         	 */

        	fdspiral = open( buf, 2 );    /* Mode = O_RDWR (=2) */

        	if(  fdspiral == -1 ) {
			marError( 1120, 0 );
			return( 0 );
        	}

                                                              
                /* Set file descriptor to spiral */           
                fd = fdspiral;                                
                                                              
        } /* End spiral section */                            

	else if ( mode == 0 && fdspiral >= 0 ) {
                /* Set file descriptor to spiral */           
                fd = fdspiral;                                
	}
                                                              
        /* Image file output */                               
        else {                                                
		if ( do_xform == 0 || keep_image == 0 || spiral_only ) return( 1 );

		fd = fdxf;
	}

	/*
	 * Get values for image header
	 */

	get_header_values( mode );

	/* 
	 * Rewind file
	 */

	 i = lseek( fd, 0, SEEK_SET );

        if ( com_format == OUT_IMAGE || com_format == OUT_PCK ) {                             
                i = Putmar300Header( fd, nb.scanner, h300 );  
                lseek( fd, 2*ac_size, SEEK_SET );             
        }                                                     
        else {                                                
                i = Putmar345Header( fd, h345 );              
                lseek( fd, 4096, SEEK_SET );                  
        }                                                     

        /* Error occured ? */                                 
        if ( i < 1 ) {                                        
                if ( mode == 0 )                              
                        marError( 1121, 0 );                   
                else {                                        
                        marError( 1115, 0 );                   
                        do_xform = 0;                         
                }                                             
                return( 0 );                                  
        }                                      
                                                    
	return( (int)1 );
}

/******************************************************************
 * Function: get_header_values
 ******************************************************************/
static void get_header_values(int mode)
{
time_t		clock;                                        
extern int      hist_begin, hist_end, hist_max;               
extern float    GetResol(float, float, float);
extern float	op_dosebeg,op_doseend;


	memcpy( (char *)&h345.gap[0], (char *)stat_gap, 8*sizeof(int));

        /*                                                    
         * Fill header values: mar345 header                  
         */                                                   
        h345.byteorder          = 1234;                       
        h345.high               = nstrong;                    
        h345.scanner            = nb.scanner;
        h345.size               = (short)ac_size;             
	if ( com_format == OUT_IMAGE )
		h345.format     = 0;     /* IMAGE */
	else 
		h345.format     = 1;     /* PCK */
        h345.mode               = (char)com_mode;            
        h345.high               = nstrong;                    
        h345.pixels             = ac_size*ac_size;                  
        h345.pixel_length       = stat_pixelsize*1000.;        
        h345.pixel_height       = stat_pixelsize*1000.;        
        h345.xcen               = ac_size/2;
        h345.ycen               = ac_size/2;
        h345.roff               = cfg.roff[(int)stat_scanmode];
        h345.toff               = cfg.toff[(int)stat_scanmode];
        h345.gain               = 1.0;
        h345.time               = com_time;                  

        h345.dosebeg            = com_dosebeg;               
        h345.doseend            = com_doseend;
        h345.dosemin            = com_dosemin;               
        h345.dosemax            = com_dosemax;               
        h345.doseavg            = com_doseavg;               
        h345.dosesig            = com_dosesig;               
        h345.dosen              = com_dosen;                 
        h345.wave               = com_wavelength;             

	if ( start_with_A ) {
		h345.adc_A		= adc1;
		h345.adc_B		= adc2;
		h345.add_A		= add_A;
		h345.add_B		= add_B;
	}
	else {
		h345.adc_A		= adc2;
		h345.adc_B		= adc1;
		h345.add_A		= add_B;
		h345.add_B		= add_A;
	}

	h345.dist       	= com_dist;
        h345.resol              = GetResol( (float)(ac_size*stat_pixelsize/2.0),
 					    (float)h345.dist,(float)h345.wave);
	h345.phibeg     	= com_phibeg;    
	h345.phiend     	= com_phiend;                
        h345.omebeg             = com_omebeg;
        h345.omeend             = com_omeend;
        h345.theta              = com_theta;
        h345.chi                = com_chi;                   
        h345.phiosc             = com_phiosc;                
        h345.omeosc             = com_omeosc;

        h345.valavg             = AvgI;                       
        h345.valmin             = valmin;                     
        h345.valmax             = Imax;                       
        h345.valsig             = SigI;                       
                                                              
        h345.histbeg            = hist_begin;                 
        h345.histend            = hist_end;                   
        h345.histmax            = hist_max;

	h345.multiplier		= cfg.spiral_scale;

/*
	h345.slitx		= slitx;
	h345.slity		= slity;
*/
	h345.polar		= com_polar;
	h345.kV			= com_kV;
	h345.mA			= com_mA;

        strcpy( h345.version, 	MAR_VERSION );                      
        strcpy( h345.program, 	"scan345 (ESD)" );                     

        strcpy( h345.source, 	com_source );                     
        strcpy( h345.filter, 	com_filter );                     
        strcpy( h345.remark, 	com_remark );                     
        strcpy( h345.detector, 	"mar345");                     
                                                              
        time( &clock );                                       
        sprintf( str, "%s",(char *) ctime(&clock) );    
	memcpy(  h345.date, str, 24 );
        memcpy(  h300.date, str, 24 );                      

        /* Specials for spirals ... */                        
        if ( mode == 0 ) {                                    
		if ( stat_scanmode > 3 ) 
        		h345.pixel_length = stat_pixelsize*500.;        
                h345.pixels    	= maximum_pixels - poff;                  
                h345.format     = 2;	/* SPIRAL */
        }                                                     
        /*                                                    
         * Fill header values: mar300 header                  
         */                                           

        h300.pixels_x           = ac_size;                    
        h300.pixels_y           = ac_size;                    
        h300.lrecl              = 2*ac_size;                  
        h300.max_rec            = ac_size+1;                  
        h300.high_pixels        = h345.high;                    
        h300.high_records       = ns_rec;                     
        h300.counts_start       = (int)op_dosebeg;                          
        h300.counts_end         = (int)op_doseend;                          
        h300.exptime_sec        = stat_time;                  
        h300.exptime_units      = (int)sum_xray_units;        
        h300.prog_time          = com_time;                  
	h300.r_max	    	= cfg.diameter[stat_scanmode]/2.;
        h300.r_min              = 0;                          
        h300.p_r                = nb.pixel_height;                          
        h300.p_l                = nb.pixel_length;
        h300.p_x                = nb.pixel_height;                          
        h300.p_y                = nb.pixel_height;
        h300.centre_x           = h345.xcen;                 
        h300.centre_y           = h345.ycen;                 
        h300.lambda             = h345.wave;
        h300.distance           = h345.dist;
        h300.phi_start          = h345.phibeg;
        h300.phi_end            = h345.phiend;                
        h300.omega              = h345.omebeg;                          
        h300.multiplier         = h345.multiplier;                 
}

/******************************************************************
 * Function: PrintResults
 ******************************************************************/
static void PrintResults( int mode )
{
	/*
	 * Print most important results
	 */

	if ( mode == 1 && !(input_skip_op || skip_op ) )
	    sprintf( str, "         -> File:      %s\n", image_file);
	else {
	    if ( spiral_only )
		    sprintf( str, "         -> File:      %s\n", spiral_file);
	    else
		    sprintf( str, "     !!! -> File:      %s\n", spiral_file);
	}

	fprintf( stdout, str );
	fprintf(  fpout, str );

	if ( verbose && com_use_center ) {
		sprintf( str, "            Center corrections applied\n");
		fprintf( stdout, str );
		fprintf(  fpout, str );
	}

	if ( mar_cmd != MDC_COM_SCAN ) {
	    sprintf( str, "            Phi range     [degrees]: %8.3f --> %1.3f\n",com_phibeg,com_phiend);
	    if ( cfg.use_phi ) {
		    fprintf( stdout, str );
		    fprintf(  fpout, str );
            }
	    sprintf( str, "            Exposure time     [sec]: %8.0f\n",com_time);
	    fprintf( stdout, str );
	    fprintf(  fpout, str );

#ifdef MAR345
	    sprintf( str, "            Average X-ray intensity: %8.2f\n", sum_xray_units);
	    fprintf( stdout, str );
	    fprintf(  fpout, str );
#endif
	}

	if ( mode != 0 || input_skip_op || skip_op || spiral_only ) {

	    if ( cfg.use_stats ) {
	    sprintf( str, "            Average image intensity: %8.1f\n",AvgI);
	    fprintf( stdout, str );
	    fprintf(  fpout, str );
	    }
	    sprintf( str, "            Maximum valid intensity: %8d\n", Imax );
	    fprintf( stdout, str );
	    fprintf(  fpout, str );
	    sprintf( str, "            No. of pixels > 65535:   %8d\n",nstrong);
	    if ( istrong > 0 ) {
		fprintf( stdout, str );
		fprintf(  fpout, str );
	    }
	    sprintf( str, "            No. of saturated pixels: %8d\n",nsat);
	    if ( nsat    > 0 ) {
		fprintf( stdout, str );
		fprintf(  fpout, str );
	    }
	}

	fflush ( stdout );
	fflush (  fpout );
}

/******************************************************************
 * Function: ReadNB
 ******************************************************************/
static int ReadNB(FILE *fp)
{                                                             
int             i,j,n,n_nb;                                     
unsigned short  r_rec[4];                                     
static int      n_rec=0;                                      
                                                              
        /* Read record with index counters */                 
        i  = fread( (unsigned short *)r_rec, sizeof(short), 2, fp );            
        /* Swap bytes if required */                          
        if (swap_nb) swapshort( r_rec, 2*sizeof(short) );     
                                                              
        n    = r_rec[0];                                      
        n_nb = r_rec[1];                                      
                                                              
        /* Read record with x coordinate */                   
        i += fread( x_rec, sizeof(char), n, fp );             
                                                              
        /* Read record with x coordinate */                   
        i += fread( y_rec, sizeof(char), n, fp );             
                                                              
        /* Read record with nb_byte */                        
        i += fread(nb_rec, sizeof(char), n, fp );             
                                                              
        if ( i != (3*n+2) ) {                                 
/*
                fprintf( stdout, "scan345: only %d out of %d bytes read from nb after pixel %d\n",i,3*n+2,sp_pixels+1);
*/
                return( 0 );                                  
        }                                                     
                                                              
        /* Read record with n_nb nb contributions */          
        if ( n_nb > 0 ) {                                     
		if ( precision == 2 ) {
			i = fread( px_rec.bit16, sizeof(unsigned short), n_nb, fp ); 
			/* Swap bytes if required */                  
			if (swap_nb) swapshort( px_rec.bit16, n_nb*sizeof(short) );
			for (j=n_nb-1;j>=0;j--) px_rec.bit32[j] = px_rec.bit16[j];
		}
		else {
			i = fread( px_rec.bit32, sizeof(unsigned int), n_nb, fp ); 
			/* Swap bytes if required */                  
			if (swap_nb) swaplong( px_rec.bit32, n_nb*sizeof(int) );
		}
                                                              
                if ( i != n_nb ) {                            
                    fprintf( stdout, "scan345: ERROR reading nb at pixel %d in record %d\n",sp_pixels+1,ftell(fp) );
                    return( 0 );                              
                }                                             
        }                                                     
                                                              
        n_rec++;                                              
                                                              
        return( n );                                          
}

/*********************************************************************
 * Function: PrintStats
 *********************************************************************/
static float PrintStats( int mode, int x, int y, int NP, unsigned int *i4_arr) 
{
register unsigned int	i,j,*i4;
register unsigned int	i4_val;
static time_t		now,last=0 ;
struct 	tm 		*time_ptr;
int			n, k,l;
double 			diff,sum,avg,sig;
char 			clock_time[32];

	/* Last call: print time and image name */
	if ( mode == 99 ) {
		now 	= time( NULL );
		time_ptr= localtime( &now);
		n    	= now - last;
		if ( last==0 ) n = 0;

		/* Print the date: */
		strftime( clock_time, 16, "%a %H:%M.%S\0", time_ptr );

		sprintf( str, "%s%5d%6d%6.1f%6.1f  %s\n",
			clock_time,n,stat_gap,adc1/1000.,adc2/1000.,image_file);

		if ( fpstat != NULL ) {
			fprintf( fpstat, str);
			fflush ( fpstat );
		}
		last	= now;
		return(1.0);
	}

	sig = sum = avg = 0.0;

	for ( i=k=l=0; i<NP; i++ ) {
		n = ac_size*( y+i ) + x;
		i4 = i4_arr + n ;
		for (  j=0; j<NP; j++, i4++ ) {
			i4_val = *i4;
			if ( i4_val == 0 || i4_val > 250000 ) {
				continue;
			}
			k++;
			sum += i4_val;
		}
	}
	sig = 0.0;

	if ( k > 1 )
		avg = sum/(double)k;
	else 
		return( avg);

	for ( i=0; i<NP; i++ ) {
		n = ac_size*( y+i ) + x;
		i4 = i4_arr + n ;
		for (  j=0; j<NP; j++, i4++ ) {
			i4_val = *i4;
			if ( i4_val == 0 || i4_val > 250000 ) continue;
			diff = i4_val - avg;
			diff *= diff;
			sig += diff;
		}
	}

	sig = sqrt( sig/(double)(k - 1.0) );

	if ( mode == 0 ) {
		sprintf( str, "%7.1f%6.1f", avg, sig);
		if ( fpstat != NULL )
			fprintf( fpstat, str);
	}

	return avg;
}

/******************************************************************
 * Function: HistoMinMax = finds limits of histogram
 ******************************************************************/
static int HistoMinMax(void)
{
register int	*hist=xf_histo;
register int 	i;
float 		dh, hsumr, hsuml;

	/* 
	 * Integrate whole histogram 
	 */

	hsuml=hist_max=0;
	for (i=1; i<=65535; i++ ) {
		hsuml = hsuml + hist[i];
		max0( hist_max, hist[i] );
        }

	/* From high intensity end go down stepwise, until right integral
	 * is 0.002 % from left  integral */

	hist_begin = 0;
	hsumr 	   = 0;
	for (i=65535; i>0; i-- ) {
		hsumr = hsumr + hist[i];
		hsuml = hsuml - hist[i];
		if (hsuml>(float)0.0) dh = hsumr/hsuml;
		if (dh>0.002) break;
	}

	hist_end = i;

	if (hist_end < 250) hist_end = 250;

	return( hist_end );

}

/******************************************************************
 * Function: CorrectCenter
 ******************************************************************/
static int CorrectCenter(int nx )
{
MAR345_HEADER	cc;
int		x,y,j,i,swap[2];
int		off		= 0;
int		ny		= 0;
int		cxo		= 0;
int		cyo		= 0;
int		i4		= 0;
int		pi4		= 0;
static int     	swap_cc		= 0;
unsigned short	*cci		= NULL;
unsigned short	*cc_image	= NULL;
unsigned int  	*p4		= NULL;
FILE		*fpcc		= NULL;


	if ( strlen( cc_file ) < 1 || com_use_center == 0) return 0;

	/* 
	 * Open center file 
         */
	if ( NULL == (fpcc  = fopen( cc_file, "r" ))) {
		fprintf(stdout,"scan345: cannot open cc_file %s\n",cc_file);
		cc_file[0] = '\0';
		return (-1);
	}

	/* 
	 * Read header of spiral image file: part in common with image header 
	 */
	cc = Getmar345Header( fpcc ); 

	/* Read ok ? */
	if ( cc.pixels == 0 ) {
		fprintf(stdout, "scan345: ERROR: 0 pixels in cc header\n");
		fclose( fpcc );
		cc_file[0] = '\0';
		return( -1 );
	}

	/* Correct mode ? */
	if ( cc.pixel_height != h345.pixel_height ) {
		fprintf(stdout, "scan345: ERROR: Pixel height of cc_file (%1.3f) and image (%1.3f) don't match\n",cc.pixel_height,h345.pixel_height);
		fclose( fpcc );
		cc_file[0] = '\0';
		return( -1 );
	}

	/* Byte swapping required ? */
	if ( cc.byteorder != 1234 ) {
		swap_cc = cc.byteorder;
		swap[0] = swap_cc;
		swaplong( swap, 4 );
		swap_cc = swap[0];
		cc.byteorder = 1234;
		if ( swap_cc != 1234 ) {
			fprintf( stdout, "scan345: ERROR: Cannot swap bytes in cc header\n");
			fclose( fpcc );
			cc_file[0] = '\0';
			return( -1 );
		}
		swap_cc = 1;
	}

	/* Allocate space for center correction */
	if(NULL == (cc_image = (unsigned short int *) calloc(cc.pixels, sizeof (unsigned short int)))) {
		fprintf( stdout, "scan345: ERROR: Cannot allocate memory for cc_image\n");
		fclose( fpcc );
		cc_file[0] = '\0';
		return( -1 );
	}

	/* Reset data */
	memset( (char *)cc_image, 0, sizeof(short)*cc.pixels );

	if ( cc.format == 1 ) {
		/* Read core */
		if ( get_pck( fpcc, cc_image) == 0 ) {
			printf("scan345: ERROR: Cannot read cc pck core!\n");
			cc_file[0] = '\0';
			return( -1 );
		}
	}
	else {
		/* Read core */
		if ( ( i=fread( cc_image, sizeof(unsigned short), cc.pixels, fpcc ) ) < cc.pixels ) {
			printf("scan345: ERROR: Only %d out of %d pixels read!\n", i, cc.pixels);
			cc_file[0] = '\0';
			return( -1 );
		}
		/* Swap bytes */
		if ( swap_cc ) swapshort( cc_image, cc.pixels*sizeof(unsigned short) );
	}
	fclose( fpcc );

	/* No. of pixels in spirals exceeds the ones in center file */
	if ( nx >= cc.size ) {
		off 	= nx/2 - (cc.size-1)/2;
		ny  	= cc.size;
		cci 	= cc_image;
		if ( nx == cc.size ) off=0;
	}
	/* */
	else {
		ny  	= nx;
		cxo	= (cc.size-nx)/2;
		cyo	= cc.size*cxo;
		if ( nx == 1200 || nx == 1800 || nx == 2400 || nx == 3000 ) cxo--;
		cci 	= cc_image + cyo + cxo;
	}

	/* Go through the offset image array and use the found value to
	 * subtract it from the pixel value in the transformed image
	 * Take care with the high intensity pixels!
	 */

	for ( y=j=0; y<ny; y++ ) {
		i = (off+y)*nx+off;
		p4= u.i4_img + i;
		cci = cc_image + cyo + cxo + y*cc.size;

		for ( x=0; x<ny; x++, i++, cci++, p4++ ) {
			pi4= (*p4);
			i4 = pi4 - (*cci);
			if ( i4 < 1 ) j++;
			if ( i4 < cfg.center_imin ) 
				i4 = cfg.center_imin;
			if ( pi4 < cfg.center_imin )
				i4 = pi4;
			(*p4) = i4;
		}
	}

	free( cc_image );

	return j;
}
