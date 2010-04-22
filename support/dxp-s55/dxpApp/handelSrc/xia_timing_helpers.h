/**
 * @file xia_timing_helpers.h
 * @brief Utilities and macros to make it easy to setup quick,
 * platform-independent timing tests.
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
 * $Id: xia_timing_helpers.h,v 1.1 2009-07-06 18:24:32 rivers Exp $
 */


#ifndef __XIA_TIMING_HELPERS_H__
#define __XIA_TIMING_HELPERS_H__

/* The actual macros, etc. are defined on a per-platform basis. If
 * the platform isn't supported then the routines are nop-d.
 */
#ifdef XIA_ENABLE_TIMING_HELPERS

#ifdef _WIN32
#include "xia_timing_helpers_w32.h"
#endif /* _WIN32 */

#else /* XIA_ENABLE_TIMING_HELPERS */

/* This is the API that needs to be implemented for any platform that wants
 * to use the timing helpers.
 */
#define INIT_TIMER_VARS
#define INIT_TIMER_FREQ
#define INIT_TIMER_LOG(log)
#define START_TIMER
#define STOP_TIMER
#define CALCULATE_TIME 0.0
#define LOG_TIME(msg, time)
#define DESTROY_TIMER

#endif /* XIA_ENABLE_TIMING_HELPERS */

#endif /* __XIA_TIMING_HELPERS_H__ */
