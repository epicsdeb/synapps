/*
 * handel_constants.h
 *
 * This header file contains external constants that
 * may be required by users of Handel. Internal constants
 * do not belong in this file.
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
 * $Id: handel_constants.h,v 1.8 2009-07-06 18:24:29 rivers Exp $
 *
 */


#ifndef HANDEL_CONSTANTS_H
#define HANDEL_CONSTANTS_H

/** Preset Run Types **/
#define XIA_PRESET_NONE           0.0
#define XIA_PRESET_FIXED_REAL     1.0
#define XIA_PRESET_FIXED_LIVE     2.0
#define XIA_PRESET_FIXED_EVENTS   3.0
#define XIA_PRESET_FIXED_TRIGGERS 4.0

/** Preamplifier type **/
#define XIA_PREAMP_RESET 0.0
#define XIA_PREAMP_RC    1.0

/** Peak mode **/
#define XIA_PEAK_SENSING_MODE     0
#define XIA_PEAK_SAMPLING_MODE    1

/** Mapping Mode Point Control Types **/
#define XIA_MAPPING_CTL_GATE 1.0
#define XIA_MAPPING_CTL_SYNC 2.0

/** Old Test Constants **/
#define XIA_HANDEL_TEST_MASK             0x1
#define XIA_HANDEL_DYN_MODULE_TEST_MASK  0x2
#define XIA_HANDEL_FILE_TEST_MASK        0x4
#define XIA_HANDEL_RUN_PARAMS_TEST_MASK  0x8
#define XIA_HANDEL_RUN_CONTROL_TEST_MASK 0x10
#define XIA_XERXES_TEST_MASK             0x20
#define XIA_HANDEL_SYSTEM_TEST_MASK      0x40

#endif /* HANDEL_CONSTANTS_H */
