/**
 * epplib.h
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
 * $Id: epplib.h,v 1.3 2009-07-06 18:24:29 rivers Exp $
 *
 * Header file for the epplib.dll source.
 *
 */

#ifndef __EPPLIB_H__
#define __EPPLIB_H__



#include "Dlldefs.h"

#ifdef __cplusplus
extern "C" {
#endif

  XIA_EXPORT int XIA_API DxpInitPortAddress(int );
  XIA_EXPORT int XIA_API DxpInitEPP(int );
  XIA_EXPORT int XIA_API DxpWriteWord(unsigned short,unsigned short);
  XIA_EXPORT int XIA_API DxpWriteBlock(unsigned short,unsigned short *,int);
  XIA_EXPORT int XIA_API DxpWriteBlocklong(unsigned short,unsigned long *, int);
  XIA_EXPORT int XIA_API DxpReadWord(unsigned short,unsigned short *);
  XIA_EXPORT int XIA_API DxpReadBlock(unsigned short, unsigned short *,int);
  XIA_EXPORT int XIA_API DxpReadBlockd(unsigned short, double *,int);
  XIA_EXPORT int XIA_API DxpReadBlocklong(unsigned short,unsigned long *,int);
  XIA_EXPORT int XIA_API DxpReadBlocklongd(unsigned short, double *,int);
  XIA_EXPORT void XIA_API DxpSetID(unsigned short id);
  XIA_EXPORT int XIA_API DxpWritePort(unsigned short port, unsigned short data);
  XIA_EXPORT int XIA_API DxpReadPort(unsigned short port, unsigned short *data);
  XIA_EXPORT int XIA_API set_addr(unsigned short Input_Data);

#ifdef __cplusplus
}
#endif


#endif /* __EPPLIB_H__ */
