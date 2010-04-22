/*
 * handel_generic.h
 *
 * Header file w/ various constants and macros in it.
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
 * $Id: handel_generic.h,v 1.10 2009-07-16 17:55:06 rivers Exp $
 *
 */


#ifndef HANDEL_GENERIC_H
#define HANDEL_GENERIC_H

#include <string.h>
#include <math.h>

#include "xia_common.h"

/* Define some maximum lengths for strings and arrays */
#define MAXALIAS_LEN		  200
#define MAXDETECTOR_CHANNELS  200
#define MAXFILENAME_LEN		  200
#define MAX_NUM_VALUES		  200
#define MAXITEM_LEN		      200
#define MAX_PATH_LEN        1024

/* This is a little sketchy right now. I only added it to provide _some_
 * mechanism for verifying the gain info. at the PSL layer. Should probably
 * remove it and come up with a better way...
 */
#define XIA_GAIN_MIN		0.0
#define XIA_GAIN_MAX		99.99

#define XIA_NULL_STRING_LEN  (strlen(XIA_NULL_STRING) + 1)

/* XiaDaqEntry states */
#define AV_STATE_UNKNOWN  0x01
#define AV_STATE_MODIFIED 0x02
#define AV_STATE_SYNCD    0x04

#endif /* HANDEL_GENERIC_H */
