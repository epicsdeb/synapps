/**
 * plxlib.h
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
 * $Id: plxlib.h,v 1.4 2009-07-06 18:24:30 rivers Exp $
 *
 * This driver serves as the interface between the MD layer and the PLX
 * chip on our PXI/cPCI hardware.
 */

#ifndef __PLXLIB_H__
#define __PLXLIB_H__

#include "Dlldefs.h"
#include "xia_common.h"

/* Headers from PLX SDK */
#include "PlxApi.h"

#ifdef __cplusplus
extern "C" {
#endif

  XIA_EXPORT int XIA_API plx_open_slot(unsigned short id, byte_t bus,
									   byte_t slot, HANDLE *h);
  XIA_EXPORT int XIA_API plx_close_slot(HANDLE h);
  XIA_EXPORT int XIA_API plx_read_long(HANDLE h, unsigned long addr,
									   unsigned long *data);
  XIA_EXPORT int XIA_API plx_write_long(HANDLE h, unsigned long addr,
										unsigned long data);
  XIA_EXPORT int XIA_API plx_read_block(HANDLE h, unsigned long addr,
										unsigned long len, unsigned long n_dead,
										unsigned long *data);

#ifdef PLXLIB_DEBUG
  XIA_EXPORT void XIA_API plx_set_file_DEBUG(char *f);
  XIA_EXPORT void XIA_API plx_dump_vmap_DEBUG(void);
#endif

#ifdef __cplusplus
}
#endif

/** STRUCT(S) **/
typedef struct _virtual_map {
  
  unsigned long     *addr;
  PLX_DEVICE_OBJECT *device;
  PLX_NOTIFY_OBJECT *events;
  PLX_INTERRUPT     *intrs;
  boolean_t         *registered;

  unsigned long n;

} virtual_map_t;

typedef struct _API_ERRORS
{
    PLX_STATUS  code;
    char       *text;
} API_ERRORS;

/** CONSTANTS **/
#define PLX_PCI_SPACE_0  2
#define EXTERNAL_MEMORY_LOCAL_ADDR  0x100000

/** MACROS **/
#define ACCESS_VADDR(i, address)  \
*((unsigned long *)(V_MAP.addr[(i)] + (address)))

#endif /* __PLXLIB_H__ */
