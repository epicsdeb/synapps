/*
 * xia_mem_private.h
 *
 * Copyright (c) 2005, XIA LLC
 *
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
 * $Id: xia_mem_private.h,v 1.3 2009-07-06 18:24:32 rivers Exp $
 */

#ifndef __XIA_MEM_PRIVATE_H__
#define __XIA_MEM_PRIVATE_H__

#include "Dlldefs.h"
#include "xia_common.h"


/** Constants **/
#define MAX_FILE_SIZE 256


/** Structures **/
typedef struct _mem_blk {
  
  void   *addr;
  size_t n;

  char file[MAX_FILE_SIZE];
  int  line;

  struct _mem_blk *next;

} mem_blk_t;


typedef struct _mem_point_elem {
  
  void *v;
  char *key;
  struct _mem_point_elem *next;

}  mem_point_elem_t;


typedef struct _mem_point {

  unsigned int n_slots;
  unsigned int n_elems;
  mem_point_elem_t **elems;

} mem_point_t;

typedef mem_point_t * mem_check_pt_t;


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

  XIA_EXPORT void *xia_mem_malloc(size_t n, char *file, int line);
  XIA_EXPORT void xia_mem_free(void *ptr);
  XIA_EXPORT void xia_mem_stats(unsigned long *total, unsigned long *current,
								unsigned long *peak);
  XIA_EXPORT void xia_mem_checkpoint(mem_check_pt_t *chk_pt);
  XIA_EXPORT void xia_mem_checkpoint_free(mem_check_pt_t *chk_pt);
  XIA_EXPORT void xia_mem_checkpoint_cmp(mem_check_pt_t *pt, char *out);

#ifdef __cplusplus
}
#endif /* __cplusplus */

XIA_SHARED mem_point_t *xia_mem_point_create(unsigned int n);
XIA_SHARED void xia_mem_point_insert(mem_point_t *p, char *key);
XIA_SHARED void xia_mem_point_free(mem_point_t *p);
XIA_SHARED boolean_t xia_mem_point_key_exists(mem_point_t *p, char *key);

#endif /* __XIA_MEM_PRIVATE_H__ */
