/**
 * @file xia_ll.c
 * @brief XIA linked-list implementation.
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
 * xia_ll.c,v 1.1 2007/10/22 02:45:29 rivers Exp
 *
 */


#include "xia_ll_api.h"
#include "xia_ll_private.h"
#include "xia_ll_errors.h"


/**
 * @brief Creates a new, empty linked-list.
 *
 * Returns NULL if there was an error creating the list.
 */
XIA_EXPORT xia_linked_list_t XIA_API xia_ll_create(void)
{
  xia_linked_list_t ll = NULL;


  ll = XIA_LL_MALLOC(sizeof(*ll));

  if (ll != NULL) {
    ll->n_elems = 0;
    ll->head    = NULL;
    ll->tail    = NULL;
  }

  return ll;
}


/**
 * @brief Destroys a linked-list previously allocated with
 * xia_ll_create().
 */
XIA_EXPORT void XIA_API xia_ll_destroy(xia_linked_list_t *ll)
{
  xia_ll_elem_t *e    = NULL;
  xia_ll_elem_t *next = NULL;

  for (e = (*ll)->head; e != NULL; e = next) {
    next = e->next;
    XIA_LL_FREE(e);
  }

  XIA_LL_FREE((*ll));
  *ll = NULL;
}


/**
 * @brief Adds an item to the linked-list.
 */
XIA_EXPORT int XIA_API xia_ll_add(xia_linked_list_t ll, void *item)
{
  xia_ll_elem_t *e = NULL;


  if (ll == NULL) {
    return XIA_LL_NULL_LIST;
  }

  e = XIA_LL_MALLOC(sizeof(*e));

  if (e == NULL) {
    return XIA_LL_OUT_OF_MEM;
  }

  e->item = item;
  e->next = NULL;

  if (ll->tail == NULL) {
    ll->head = e;
    ll->tail = e;
    ll->iter = e;
  } else {
    ll->tail->next = e;
    ll->tail = e;
  }

  return XIA_LL_SUCCESS;
}


/**
 * @brief Retrieve the next item in the linked-list using the cached
 * iterator.
 */
XIA_EXPORT void * XIA_API xia_ll_fetch_next(xia_linked_list_t ll)
{
  xia_ll_elem_t *e = NULL;


  if (ll == NULL || ll->iter == NULL) {
    return NULL;
  } else {
    e = ll->iter;
    ll->iter = ll->iter->next;
    return e->item;
  }
}


/**
 * @brief Resets the iterator back to the beginning of the list.
 */
XIA_EXPORT void XIA_API xia_ll_reset_iterator(xia_linked_list_t ll)
{
  ll->iter = ll->head;
}
