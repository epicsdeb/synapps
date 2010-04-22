/*
 *-----------------------------------------------------------
 *
 *	Parameter lists for detcon_entry.c
 *
 *-----------------------------------------------------------
 *
 */

enum {
	HWP_BIN = 0,		/* 1 for no binning, 2 for 2x2 binning */
	HWP_ADC,		/* 0 for slow, 1 for fast adc */
	HWP_SAVE_RAW,		/* 1 to save raw images */
	HWP_DARK,		/* 1 if this is a dark */
	HWP_STORED_DARK,	/* 1 for stored dark, else normal darks */
	HWP_NO_XFORM,		/* 1 fo no transform */
	HWP_LOADFILE,		/* 1 to read the image from disk instead of collecting it */
	HWP_TEMP_COLD,		/* 1 to make the detector COLD (set), or return 1 if the detector is cold (get) */
	HWP_TEMP_WARM,		/* 1 to make the detector WARM (set), or return 1 if the detector is warm (get) */
	HWP_TEMP_MODE,		/* 1 if detector temperature changes are done with SETS, 0 for RAMP (default) */
	HWP_TEMP_STATUS,	/* returns the detector temperature status string */
	HWP_TEMP_VALUE		/* set or get the current "detector temperature" */
     };

enum {
	FLP_PHI = 0,		/* phi value */
	FLP_OMEGA,		/* omega */
	FLP_KAPPA,		/* kappa */
	FLP_TWOTHETA,		/* two theta */
	FLP_DISTANCE,		/* distance */
	FLP_WAVELENGTH,		/* wavelength */
	FLP_AXIS,		/* 1 for phi, 0 for omega */
	FLP_OSC_RANGE,		/* frame size */
	FLP_TIME,		/* time, if used */
	FLP_DOSE,		/* dose, if used */
	FLP_BEAM_X,		/* beam center, x */
	FLP_BEAM_Y,		/* beam center, y */
	FLP_COMPRESS,		/* 1 to compress output images */
	FLP_KIND,		/* "kind" sequence number */
	FLP_FILENAME,		/* filename */
	FLP_COMMENT,		/* comment to add to header */
	FLP_LASTIMAGE,		/* 1 for last image, 0 otherwise, -1 for flush */
	FLP_SUFFIX,		/* returns or sets the image suffix */
	FLP_IMBYTES,		/* image number of bytes */
	FLP_READ_FILENAME,	/* filename to read from for HWP_LOADFILE=1 */
	FLP_USERDEF_STR,	/* user defined command */
	FLP_USERRET,		/* user defined command return */
	FLP_HEADER,		/* for SMV, a header to be merged with input header on image output */
				/* for CBF, a header template to be used on image output */
	FLP_JPEG1_NAME,		/* file name for first jpeg file (if specified) */
	FLP_JPEG1_SIZE,		/* size of first jpeg file, as a string such as "colsxrow" (e,g. "128x128") */
	FLP_JPEG2_NAME,		/* file name for second jpeg file (if specified) */
	FLP_JPEG2_SIZE,		/* size of second jpeg file, as a string such as "colsxrow" (e,g. "128x128") */
	FLP_OUTFILE_TYPE,	/* output file type: 0 for .img, 1 for just .cbf, and 2 for both .img and .cbf */
     };
