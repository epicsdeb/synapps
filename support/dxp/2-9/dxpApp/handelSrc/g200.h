/*
 *  g200.h
 *
 *  Modified 2-Feb-97 EO: add prototype for dxp_primitive routines
 *      dxp_read_long and dxp_write_long; added various parameters
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
 *    Following are prototypes for g200.c routines
 */


#ifndef G200_H
#define G200_H

#ifndef XERXESDEF_H
#include <xerxesdef.h>
#endif

#ifndef XERXES_GENERIC_H
#include <xerxes_generic.h>
#endif

#include <g200_generic.h>

#define CODE_VERSION					   0
#define CODE_REVISION		  			   1
#define LATEST_BOARD_TYPE		  "DGF-G200"
#define LATEST_BOARD_VERSION		     "E"
/*
 *    
 */
#define MAXDET				500
#define PROGRAM_BASE     0x0000
#define DATA_BASE        0x4000
#define START_PARAMS     0x4000
#define MAXDSP_LEN       0x8000
#define MAXFIP_LEN   0x00020000
#define MAX_FIPPI			  5
/*
 *    Calibration control codes
 */
#define CONTROLTASK_SETDACS                   0
#define CONTROLTASK_OPEN_INPUT_RELAY          1
#define CONTROLTASK_CLOSE_INPUT_RELAY         2
#define CONTROLTASK_RAMP_OFFSET_DAC           3
#define CONTROLTASK_ADC_TRACE                 4
#define CONTROLTASK_PROGRAM_FIPPI             5
#define CONTROLTASK_READ_MEMORY_FIRST         9
#define CONTROLTASK_READ_MEMORY_NEXT         10
#define CONTROLTASK_WRITE_MEMORY_FIRST       11
#define CONTROLTASK_WRITE_MEMORY_NEXT        12
#define CONTROLTASK_ISOLATED_RAMP_OFFSET_DAC 13
#define CONTROLTASK_MEASURE_NOISE            19
#define CONTROLTASK_ADC_CALIB_FIRST          20
#define CONTROLTASK_ADC_CALIB_NEXT           21
/*
 *    CAMAC status Register control codes
 */
#define ALLCHAN              -1
#define IGNOREGATE            1
#define USEGATE               0
#define CLEARMCA              0
#define UPDATEMCA             1

/* Definitions of EPP addresses for the G200 boards 
 * F code for write is 1, read is 0
 * A code is the EPP register for the xfer
 *    0=Data register,    1=Address register
 *    2=Control register, 3=Status register
 */
#define G200_TSAR_F_WRITE         1
#define G200_TSAR_A_WRITE         1
#define G200_TSAR_F_READ          0
#define G200_TSAR_A_READ          1
#define G200_CSR_F_WRITE          1
#define G200_CSR_A_WRITE          0
#define G200_CSR_F_READ           0
#define G200_CSR_A_READ           0
#define G200_DATA_F_READ          0
#define G200_DATA_A_READ          0
#define G200_DATA_F_WRITE         1
#define G200_DATA_A_WRITE         0
#define G200_WCR_F_READ           0
#define G200_WCR_A_READ           0
#define G200_WCR_F_WRITE          1
#define G200_WCR_A_WRITE          0
#define G200_FIPPI_F_WRITE        1
#define G200_FIPPI_A_WRITE        0
#define G200_MMU_F_WRITE        1
#define G200_MMU_A_WRITE        0

/* Now define the addresses */
#define G200_CSR_ADDRESS     0x8000
#define G200_FIPPI_ADDRESS   0x8003
#define G200_WCR_ADDRESS     0x8004
#define G200_MMU_ADDRESS     0x8006

#endif						/* Endif for X10P_H */
