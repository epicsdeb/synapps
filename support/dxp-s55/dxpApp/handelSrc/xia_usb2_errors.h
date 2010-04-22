/**
 * @file xia_usb2.h
 * @brief Error codes returned by XIA USB2.0 routines.
 */

/*
 * Copyright (c) 2006, XIA LLC
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
 * $Id: xia_usb2_errors.h,v 1.2 2009-07-06 18:24:32 rivers Exp $
 */

#ifndef __XIA_USB2_ERRORS_H__
#define __XIA_USB2_ERRORS_H__

#define XIA_USB2_SUCCESS          0

#define XIA_USB2_GET_CLASS_DEVS   1
#define XIA_USB2_ENUM_DEV_INTFC   2
#define XIA_USB2_DEV_INTFC_DETAIL 3
#define XIA_USB2_NO_MEM           4
#define XIA_USB2_CLOSE_HANDLE     5
#define XIA_USB2_XFER             6
#define XIA_USB2_NULL_HANDLE      7
#define XIA_USB2_ZERO_BYTES       8
#define XIA_USB2_NULL_BUFFER      9
#define XIA_USB2_INVALID_HANDLE   10

#endif /* __XIA_USB2_ERRORS_H__ */
