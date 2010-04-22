/*
 *  md_generic.h
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
 * $Id: md_generic.h,v 1.7 2009-07-06 18:24:30 rivers Exp $
 *
 *    Generic constant definitions related to the MD layer
 */


#ifndef MD_GENERIC_H
#define MD_GENERIC_H

#define MD_ERROR		1
#define MD_WARNING		2
#define MD_INFO			3
#define	MD_DEBUG		4

/* IO Flags for serial port communications
   These are used by udxp(s) drivers exclusively */
#define IO_NORMAL    0x01
#define IO_OPEN      0x02
#define IO_CRITICAL  0x04
#define IO_CLOSE     0x08
#define IO_SNIFF_ON  0x10
#define IO_SNIFF_OFF 0x20
#define IO_USB       0x40 /* Special flag to signal dxp_command for USB comm */

#define MD_INVALID_ADDR -1

/* I/O Priority flags */
enum {
  MD_IO_PRI_NORMAL = 0,
  MD_IO_PRI_HIGH,
};

#endif						/* Endif for MD_GENERIC_H */
