/**
 * @file xia_timing_helpers_w32.h
 * @brief Win32 implementation of the timer API in xia_timing_helpers.h.
 */

/*
 * Copyright (c) 2007, XIA LLC
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
 * $Id: xia_timing_helpers_w32.h,v 1.1 2009-07-06 18:24:32 rivers Exp $
 */


#ifndef __XIA_TIMING_HELPERS_W32_H__
#define __XIA_TIMING_HELPERS_W32_H__

#include <stdio.h>

#include "windows.h"

#define INIT_TIMER_VARS  \
  LARGE_INTEGER freq;    \
  LARGE_INTEGER start;   \
  LARGE_INTEGER stop;    \
  FILE *timer_log_fp = NULL

#define INIT_TIMER_FREQ  \
  QueryPerformanceFrequency(&freq)

#define INIT_TIMER_LOG(log)       \
  timer_log_fp = fopen((log), "a")

#define START_TIMER \
  QueryPerformanceCounter(&start)

#define STOP_TIMER \
  QueryPerformanceCounter(&stop)

#define CALCULATE_TIME \
  ((double)(stop.QuadPart - start.QuadPart) / (double)freq.QuadPart)

#define LOG_TIME(msg, time)                                         \
  do {                                                              \
    if (timer_log_fp != NULL) {                                     \
      fprintf(timer_log_fp, "%s : %0.6f seconds\n", (msg), (time)); \
    }                                                               \
  } while(0)

#define DESTROY_TIMER           \
  do {                          \
    if (timer_log_fp != NULL) { \
      fclose(timer_log_fp);     \
    }                           \
  } while(0)

#endif /* __XIA_TIMING_HELPERS_W32_H__ */
