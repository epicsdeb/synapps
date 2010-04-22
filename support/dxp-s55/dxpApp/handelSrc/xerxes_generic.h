/*
 *  xerxes_generic.h
 *
 * Copyright (c) 2004, X-ray Instrumentation Associates
 *               2005, XIA LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, 
 * with or without modification, are permitted provided 
 * that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer.
 *   * Redistributions in binary form must reproduce the 
 *     above copyright notice, this list of conditions and the 
 *     following disclaimer in the documentation and/or other 
 *     materials provided with the distribution.
 *   * Neither the name of X-ray Instrumentation Associates 
 *     nor the names of its contributors may be used to endorse 
 *     or promote products derived from this software without 
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF 
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 *
 *
 * $Id: xerxes_generic.h,v 1.3 2009-07-06 18:24:31 rivers Exp $
 *
 *    Generic constant definitions
 */


#ifndef XERXES_GENERIC_H
#define XERXES_GENERIC_H


/* GLOBAL constants needed by both xia_xerxes.h and xerxes.h */

#define MAXDET				500
#define MAXSYM              500
#define MAXSYMBOL_LEN        15
#define MAXFILENAME_LEN     200
#define MAXDXP				100		/* Maximum number of DXP modules in system */
#define MAXBOARDNAME_LEN	 20
/* MAXSYMBOL_LEN is kept around for compatibility purposes,
 * but it should be converted to MAX_DSP_PARAM_NAME_LEN
 */
#define MAX_DSP_PARAM_NAME_LEN 30

/* Definitions for DXP4C configurations */

/*
 *    Control Task codes
 */
#define CT_DXP4C_RESETASC	100			/* Reset Slope Generator */
#define CT_DXP4C_TRKDAC		102			/* TrackDAC calibration	*/
#define CT_DXP4C_DACSET		104			/* Set the tracking, slope and gain DACs */
#define CT_DXP4C_ADCTEST	105			/* Test the ADC Linearity */
#define CT_DXP4C_ASCMON		106			/* Monitor ASC interrupts */
#define CT_DXP4C_RESET		107			/* Reset calibration */
#define CT_DXP4C_FIPTRACE	109			/* Acquire Decimated Trace data */
#define CT_DXP4C_ADC        110			/* Acquire an ADC trace */
/* Kept for backwards compatibility */
#define CALIB_TRKDAC		  1
#define CALIB_RESET			  2

/* 
 * Definitions for DXP4C2X configurations 
 */
#define CT_DXP2X_SET_ASCDAC        200
#define CT_DXP2X_TRKDAC            202
#define CT_DXP2X_SLOPE_CALIB       203
#define CT_DXP2X_SLEEP_DSP         206
#define CT_DXP2X_PROGRAM_FIPPI     211
#define CT_DXP2X_SET_POLARITY      212
#define CT_DXP2X_CLOSE_INPUT_RELAY 213
#define CT_DXP2X_OPEN_INPUT_RELAY  214
#define CT_DXP2X_RC_BASELINE       215
#define CT_DXP2X_RC_EVENT	       216
#define CT_DXP2X_ADC		       217
#define CT_DXP2X_BASELINE_HIST     218
#define CT_DXP2X_CHECK_MEMORY      219
#define CT_DXP2X_READ_MEMORY       220
#define CT_DXP2X_RESET             299

/* 
 * Definitions for DXPX10P configurations 
 */
#define CT_DXPX10P_SET_ASCDAC        300
#define CT_DXPX10P_TRKDAC            302
#define CT_DXPX10P_SLOPE_CALIB       303
#define CT_DXPX10P_SLEEP_DSP         306
#define CT_DXPX10P_PROGRAM_FIPPI     311
#define CT_DXPX10P_SET_POLARITY      312
#define CT_DXPX10P_CLOSE_INPUT_RELAY 313
#define CT_DXPX10P_OPEN_INPUT_RELAY  314
#define CT_DXPX10P_RC_BASELINE       315
#define CT_DXPX10P_RC_EVENT          316
#define CT_DXPX10P_ADC               317
#define CT_DXPX10P_BASELINE_HIST     318	/* Special run for Baseline History		*/
#define CT_DXPX10P_READ_MEMORY       321
#define CT_DXPX10P_WRITE_MEMORY      323
#define CT_DXPX10P_RESET             399

/* 
 * Definitions for DGFG200 configurations 
 */
#define CT_DGFG200_SETDACS            400
#define CT_DGFG200_OPEN_INPUT_RELAY   401
#define CT_DGFG200_CLOSE_INPUT_RELAY  402
#define CT_DGFG200_RAMP_OFFSET_DAC    403
#define CT_DGFG200_ADC		      404
#define CT_DGFG200_PROGRAM_FIPPI      405
#define CT_DGFG200_READ_MEMORY_FIRST  409
#define CT_DGFG200_READ_MEMORY_NEXT   410
#define CT_DGFG200_WRITE_MEMORY_FIRST 411
#define CT_DGFG200_WRITE_MEMORY_NEXT  412
#define CT_DGFG200_ISOLATED_RAMP_OFFSET_DAC  413
#define CT_DGFG200_MEASURE_NOISE      419
#define CT_DGFG200_ADC_CALIB_FIRST    420
#define CT_DGFG200_ADC_CALIB_NEXT     421
#define CT_DGFG200_ADJUST_OFFSETS     425
#define CT_DGFG200_DACLOW_DACHIGH     426
#define CT_DGFG200_COLLECT_BASELINES  406

/* Generic Special Run Definitions (Applicable to all boards) */

#define CT_ADC             5000		/* Special run for ADC trace readout	*/

#endif						/* Endif for XERXES_GENERIC_H */
