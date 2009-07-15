/*
 *  g200_generic.h
 *
 *  Created 4-Apr-02 JEW: created to contain definitions common to both 
 *       Xerxes and Handel
 *
 * Copyright (c) 2002, X-ray Instrumentation Associates
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
 */


#ifndef G200_GENERIC_H
#define G200_GENERIC_H

/*
 * Misc parameters
 */
#define FIFO_LENGTH             4096
#define INTERNAL_BUFFER_LENGTH  2048
#define TRIGGER_THRESHOLD_MAX    511

#define FIFO_DECIMATION            4

/* Timing information */
#define DSP_TIMER_TICK_TIME 1.6384e-3l

/* 
 * Control words for the RUNTASKS parameter
 */
#define CONTROL_TASK_RUN      0
#define DAQ_RUN               1
#define FAST_LIST_MODE_RUN    2
#define MCA_RUN               3
#define SHORT_PSA_RUN         4
#define UNDEFINED_RUN         5

/*
 * ASC parameters:
 */
#define ADC_RANGE	 1000.0         /* Input range in mV           */
#define ADC_BITS	     14         /* Number of digitization bits */
#define GINPUT              1.0		/* Input attenuator Gain       */
#define GINPUT_BUFF	    1.0		/* Input buffer Gain           */
#define GINVERTING_AMP 3240./499.	/* Inverting Amp Gain          */
#define GV_DIVIDER  124.9/498.9		/* Voltage Divider after 
					   Inverting AMP               */
#define GGAINDAC_BUFF       1.0		/* GainDAC buffer Gain         */
#define GNYQUIST      422./613.		/* Nyquist Filter Gain         */
#define GADC_BUFF           2.0		/* ADC buffer Gain             */
#define GADC          250./350.		/* ADC Input Gain              */
#define GAINDAC_RANGE	 3000.0
#define GAINDAC_RANGE_DB    40.         /* Full range of the DAC in dB */
#define GAINDAC_BITS	     16         /* Number of dig. bits in GAINDAC */
#define GAINDAC_MIN	    -6.         /* Minimum GAIN in dB          */
#define GAINDAC_MAX	    30.         /* Maximum GAIN in dB          */

#define TDAC_GAIN   3240./3739.         /* Resistor Divider for the Tracking DAC */
#define TDAC_MIN            -3.         /* Minimum Tracking DAC output in Volts  */
#define TDAC_MAX             3.         /* Maximum Tracking DAC output in Volts  */
/*
 *     CAMAC status Register bit positions
 */
#define MASK_RUNENABLE    0x0001
#define MASK_RESETMCA     0x0002
#define MASK_UNUSED2      0x0004
#define MASK_LAM_ENABLE   0x0008
#define MASK_DSPRESET     0x0010
#define MASK_FIPRESET     0x0020
#define MASK_MMURESET     0x0040
#define MASK_SYNCH_FLAG   0x0080
#define MASK_FIPPI_DONE   0x0100
#define MASK_UNUSED9      0x0200
#define MASK_UNUSED10     0x0400
#define MASK_MMU_DONE     0x0800
#define MASK_DSP_ERROR    0x1000
#define MASK_RUN_ACTIVE   0x2000
#define MASK_LAM_REQUEST  0x4000
#define MASK_G200_LIVE    0x8000

/*
 *     CHANCSRA parameter bits
 */
#define CCSRA_EXTTRIG     0x0001
#define CCSRA_LIVETIME    0x0002
#define CCSRA_GOOD_CHAN   0x0004
#define CCSRA_READ_ALWAYS 0x0008
#define CCSRA_ENABLE_TRIG 0x0010
#define CCSRA_POLARITY    0x0020
#define CCSRA_GATE        0x0040
#define CCSRA_HISTOGRAM   0x0080
#define CCSRA_SPARE8      0x0100
#define CCSRA_BDEFICIT    0x0200
#define CCSRA_CFD         0x0400
#define CCSRA_MULT        0x0800
#define CCSRA_PRESET_LIVE 0x1000
#define CCSRA_DNL         0x2000
#define CCSRA_SPARE14     0x4000
#define CCSRA_BIPOLAR     0x8000
/*
 * MODCSRA parameter Bits
 */
#define MCSRA_LEVEL_ONE   0x0001


#endif						/* Endif for G200_GENERIC_H */
