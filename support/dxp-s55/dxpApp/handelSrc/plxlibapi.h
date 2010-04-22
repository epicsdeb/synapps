/**
 * plxlibapi.h
 *
 * The PLX API (and all code from the SDK) is
 * Copyright (c) 2003 PLX Technology Inc
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
 * $Id: plxlibapi.h,v 1.3 2009-07-06 18:24:30 rivers Exp $
 *
 * API for plxlib
 */

#ifndef __PLXLIBAPI_H__
#define __PLXLIBAPI_H__

#include "Dlldefs.h"
#include "xia_common.h"

/* Headers from PLX SDK */
#include "PlxApi.h"

#ifdef __cplusplus
extern "C" {
#endif

  XIA_IMPORT int XIA_API plx_open_slot(unsigned short dev_id, byte_t bus,
									   byte_t slot, HANDLE *h);
  XIA_IMPORT int XIA_API plx_close_slot(HANDLE h);
  XIA_IMPORT int XIA_API plx_read_long(HANDLE h, unsigned long addr, 
									   unsigned long *data);
  XIA_IMPORT int XIA_API plx_write_long(HANDLE h, unsigned long addr,
										unsigned long data);
  XIA_IMPORT int XIA_API plx_read_block(HANDLE h, unsigned long addr,
										unsigned long len, unsigned long n_dead,
										unsigned long *data);

#ifdef PLXLIB_DEBUG
  XIA_IMPORT void XIA_API plx_set_file_DEBUG(char *f);
  XIA_IMPORT void XIA_API plx_dump_vmap_DEBUG(void);
#endif

#ifdef __cplusplus
}
#endif


#endif /* __PLXAPILIB_H__ */
