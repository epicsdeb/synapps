/*
 * xia_mem_point.c
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
 * Interface to memory checkpoints. In reality, these checkpoints are just
 * hash tables.
 *
 * $Id: xia_mem_point.c,v 1.3 2009-07-06 18:24:32 rivers Exp $
 */

#include <stdlib.h>
#include <stdio.h>

#include "Dlldefs.h"

#include "xia_mem_private.h"
#include "xia_common.h"
#include "xia_assert.h"


/** Private functions **/
XIA_STATIC unsigned int xia_mem__hash(char *key);
XIA_STATIC mem_point_elem_t *xia_mem__add_elem(mem_point_elem_t *e,
											   mem_point_elem_t *new_e);
XIA_STATIC void xia_mem__remove_elem_chain(mem_point_elem_t *e);

/** @brief Create a new memory point (aka. hashtable).
 *
 * It is an unchecked exception to pass in @a n <= 0.
 */
XIA_SHARED mem_point_t *xia_mem_point_create(unsigned int n)
{
  mem_point_t *p = NULL;


  ASSERT(n > 0);


  p = (mem_point_t *)malloc(sizeof(mem_point_t));
  ASSERT(p != NULL);

  memset(p, 0, sizeof(mem_point_t));

  p->n_slots = n;
  p->n_elems = 0;
  p->elems = (mem_point_elem_t **)calloc(n, sizeof(mem_point_elem_t *));
  ASSERT(p->elems != NULL);
  
  return p;
}


/** @brief Insert a new memory location into the specified point.
 *
 */
XIA_SHARED void xia_mem_point_insert(mem_point_t *p, char *key)
{
  mem_point_elem_t *new_e = NULL;

  unsigned int h = 0;


  ASSERT(p != NULL);
  ASSERT(key != NULL);


  new_e = (mem_point_elem_t *)malloc(sizeof(mem_point_elem_t));
  ASSERT(new_e != NULL);
  
  new_e->v    = NULL;
  new_e->key  = key;
  new_e->next = NULL;

  h = xia_mem__hash(key);
  h = h % p->n_slots;

  p->elems[h] = xia_mem__add_elem(p->elems[h], new_e);
  p->n_elems++;
}


/** @brief Free all of the memory for a mem point.
 *
 */
XIA_SHARED void xia_mem_point_free(mem_point_t *p)
{
  unsigned int i;

  
  ASSERT(p != NULL);

  
  for (i = 0; i < p->n_slots; i++) {
	if (p->elems[i]) {
	  xia_mem__remove_elem_chain(p->elems[i]);
	}
  }

  free(p);
  p = NULL;
}


/** @brief Checks to see if the specified key is in the memory point hash.
 *
 * This is, essentially, a lookup routine except that the only thing we care
 * about is the existence of the key not the actual data itself.
 *
 */
XIA_SHARED boolean_t xia_mem_point_key_exists(mem_point_t *p, char *key)
{
  mem_point_elem_t *e = NULL;

  unsigned int h = 0;

  
  ASSERT(p != NULL);
  ASSERT(key != NULL);


  h = xia_mem__hash(key);
  h = h % p->n_slots;

  for(e = p->elems[h]; e != NULL; e = e->next) {
	if (STREQ(e->key, key)) {
	  return TRUE_;
	}
  }

  return FALSE_;
}


/** @brief Removes linked-list of elements rooted at @a e.
 *
 */
XIA_STATIC void xia_mem__remove_elem_chain(mem_point_elem_t *e)
{
  mem_point_elem_t *next = NULL;

  
  ASSERT(e != NULL);


  for ( ; e != NULL; e = next) {
	next = e->next;
	free(e->key);
	free(e);
  }
}


/** @brief Generates a hash for a memory block
 *
 */
XIA_STATIC unsigned int xia_mem__hash(char *key)
{
  byte_t *c = NULL;

  unsigned int h = 0;

  
  ASSERT(key != NULL);

  for (c = (byte_t *)key; *c != '\0'; c++) {
	h = h * 33 + *c;
  }

  return h;
}


/** @brief Add a new slot element to the front of the slot linked-list.
 *
 */
XIA_STATIC mem_point_elem_t *xia_mem__add_elem(mem_point_elem_t *e,
											   mem_point_elem_t *new_e)
{
  ASSERT(new_e != NULL);


  new_e->next = e;
  return new_e;
}


