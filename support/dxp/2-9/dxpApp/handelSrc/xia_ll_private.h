/**
 * @file xia_ll_private.h
 * @brief Private structures, constants, etc. for XIA's linked-list
 * implementation.
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
 * xia_ll_private.h,v 1.1 2007/10/22 02:45:29 rivers Exp
 */

#ifndef __XIA_LL_PRIVATE_H__
#define __XIA_LL_PRIVATE_H__


/**
 * An individual element of the linked-list.
 */
typedef struct _xia_ll_elem {

  /**
   * A generic pointer to the data the user wants to store in this element.
   */
  void *item;

  /**
   * A pointer to the next element in the list.
   */
  struct _xia_ll_elem *next;

} xia_ll_elem_t;


/**
 * Wraps up the linked-list into a nice package and holds all of the metadata
 * about the list as well. This structure is aliased in xia_ll.h as an
 * opaque pointer that the user can pass into most of the linked-list routines.
 */
struct _xia_linked_list {

  /**
   * Number of elements in the linked-list.
   */
  int n_elems;

  /**
   * A pointer to the start of the linked-list.
   */
  xia_ll_elem_t *head;

  /**
   * A pointer to the end of the linked-list.
   */
  xia_ll_elem_t *tail;

  /**
   * A pointer to the current element of the linked-list to be
   * retrieved iteratively.
   */
  xia_ll_elem_t *iter;
  
};

/* This gives us the same semantics as the user, since this is how
 * the opaque pointer is defined in xia_ll.h.
 */
typedef struct _xia_linked_list *xia_linked_list_t;

/* So we can use XIA's memory manager for debugging. */
#ifdef USE_XIA_MEM_MANAGER
#include "xia_mem.h"
#define XIA_LL_MALLOC(n)  xia_mem_malloc((n), __FILE__, __LINE__)
#define XIA_LL_FREE(ptr)  xia_mem_free((ptr))
#else
#include <stdlib.h>
#define XIA_LL_MALLOC(n)  malloc((n))
#define XIA_LL_FREE(ptr)  free((ptr))
#endif /* USE_XIA_MEM_MANAGER */

#endif /* __XIA_LL_PRIVATE_H__ */
