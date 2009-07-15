/***********************************************************************
 *
 * mar300_header.h
 *
 * Copyright by:        Dr. Claudio Klein
 *                      X-ray Research GmbH, Hamburg
 *
 * Version:     1.1
 * Date:        08/01/2002
 *
 **********************************************************************/

typedef struct {
        int	pixels_x;
	int	pixels_y;
        int	lrecl;
	int	max_rec;
        int	high_pixels;
	int	high_records;
        int	counts_start;
	int	counts_end;
        int	exptime_sec;
        int	exptime_units;

        float	prog_time;
	float	r_max;
        float	r_min;
	float	p_r;
        float	p_l;
	float	p_x;
        float	p_y;
	float	centre_x;
        float	centre_y;
	float	lambda;
        float	distance;
	float	phi_start;
        float	phi_end;
	float	omega;
        float	multiplier;

	char	date[26];

} MAR300_HEADER;

