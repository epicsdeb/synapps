/**
 * @file xia_ll.h
 * @brief Public interface to XIA Linked-List type.
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
 * xia_ll.h,v 1.2 2007/11/14 21:14:41 rivers Exp
 */

#ifndef __XIA_LL_H__
#define __XIA_LL_H__

#include "Dlldefs.h"

/**
 * Opaque pointer to linked list structure.
 */
typedef struct _xia_linked_list *xia_linked_list_t;

#ifdef __cplusplus
extern "C" {
#endif

  XIA_IMPORT xia_linked_list_t XIA_API xia_ll_create(void);
  XIA_IMPORT void XIA_API xia_ll_destroy(xia_linked_list_t *ll);
  XIA_IMPORT int XIA_API xia_ll_add(xia_linked_list_t ll, void *elem);
  XIA_IMPORT void *XIA_API xia_ll_fetch_next(xia_linked_list_t ll);
  XIA_IMPORT void XIA_API xia_ll_reset_iterator(xia_linked_list_t ll);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __XIA_LL_H__ */
