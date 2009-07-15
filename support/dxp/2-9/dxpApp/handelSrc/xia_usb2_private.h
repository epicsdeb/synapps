/**
 * @file xia_usb2_private.h
 * @brief Private data for the XIA USB2.0 driver. Nothing in this file should
 * be required for users of the driver.
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
 * xia_usb2_private.h,v 1.3 2007/12/20 20:04:18 rivers Exp
 */

#ifndef __XIA_USB2_API_PRIVATE_H__
#define __XIA_USB2_API_PRIVATE_H__

#ifdef WIN32
#include <windows.h>
#endif

/* Custom messages */
#define WM_CLOSE_DEVICE  (WM_USER + 1)

/* Transfer types */
#define XIA_USB2_READ     0x0


/* Endpoints */
#define XIA_USB2_SETUP_EP 0x1
#define XIA_USB2_READ_EP  0x82
#define XIA_USB2_WRITE_EP 0x6

/* XIA USB2 protocol constants */
#define XIA_USB2_SETUP_PACKET_SIZE      9
#define XIA_USB2_SMALL_READ_PACKET_SIZE 512

#define XIA_USB2_SETUP_FLAG_WRITE   0x0
#define XIA_USB2_SETUP_FLAG_READ    0x1

#endif /* __XIA_USB2_API_PRIVATE_H__ */
