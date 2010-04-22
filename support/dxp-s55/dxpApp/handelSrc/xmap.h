/*
 * xmap.h
 *
 * Public interface for xMAP device driver.
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
 * $Id: xmap.h,v 1.3 2009-07-06 18:24:32 rivers Exp $
 *
 */


#ifndef XMAP_H
#define XMAP_H

/* This value is in words. The maximum length of the DSP in 24-bit program
 * memory values is 0x8000.
 */
#define MAXDSP_LEN      0x10000
#define MAXFIP_LEN   0x00100000
#define MAX_FIPPI			  5

/* Control Tasks */
enum {
  XMAP_CT_ADC = 0,
  XMAP_CT_APPLY,
  XMAP_CT_MEMFILL1,
  XMAP_CT_BASE_HIST,
  /* Fast filter raw output scaled by 2^(-FSCALE) */
  XMAP_CT_FAST_FILTER,
  /* Fast baseline instantaneous samples scaled by 2^(-FSCALE) */
  XMAP_CT_FAST_BASE,
  /* Fast filter baseline running average scaled by 2^(-FSCALE) */
  XMAP_CT_FAST_BASE_AVG,
  /* Fast filter baseline-subtracted output scaled by 2^(-FSCALE) */
  XMAP_CT_FAST_BASE_SUB,
  /* Baseline instantaneous scamples scaled by 2^(-ESCALE) */
  XMAP_CT_BASE_INST,
  /* Baseline running average scaled by 2^(-ESCALE) */
  XMAP_CT_BASE_AVG,
  /* Baseline filter baseline-subtracted ouput scaled by 2^(-ESCALE) */
  XMAP_CT_BASE_SUB,
  /* Slow filter raw output scaled by 2^(-ESCALE) */
  XMAP_CT_SLOW_FILTER,
  /* Slow filter baseline-subtracted ouput scaled by 2^(-ESCALE) */
  XMAP_CT_SLOW_BASE_SUB,
  /* Event samples scaled by 2^(-ESCALE) */
  XMAP_CT_EVENTS,
  /* Wake the DSP up */
  XMAP_CT_WAKE_DSP,
};

#endif /* XMAP_H */
