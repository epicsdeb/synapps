/*
 * xia_mem.c
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
 * $Id:
 */

#include <stdlib.h>
#include <stdio.h>

#include "Dlldefs.h"

#include "xia_mem_private.h"
#include "xia_common.h"
#include "xia_assert.h"


/** Private functions **/
static void xia__add_blk(void *ptr, size_t n, char *file, int line);
static void xia__remove_blk(void *ptr);


static unsigned long TOTAL_BYTES_ALLOCATED   = 0;
static unsigned long CURRENT_BYTES_ALLOCATED = 0;
static unsigned long PEAK_BYTES_ALLOCATED    = 0;

/* Global doubly-linked-list of the current memory blocks */
static mem_blk_t *BLK_HEAD = NULL;


/** @brief Allocates @a n bytes of memory and returns it to the user.
 *
 * If memory allocation fails, a NULL pointer is returned. The @a file
 * and @a line information is stored as metadata so that the library knows
 * which code allocated which pointers. A typical use of xia_mem_alloc() is
 * to wrap it in a macro in your own code:
 *
 * #define my_malloc(n) xia_mem_alloc((n), __FILE__, __LINE__)
 *
 */
XIA_EXPORT void *xia_mem_malloc(size_t n, char *file, int line)
{
  void *ptr = NULL;

  UNUSED(file);
  UNUSED(line);

  
  ptr = malloc(n);

  if (ptr) {
	xia__add_blk(ptr, n, file, line);
  }

  return ptr;
}


/** @brief Frees the memory pointed to by @a ptr.
 *
 */
XIA_EXPORT void xia_mem_free(void *ptr)
{
  xia__remove_blk(ptr);
  free(ptr);
}


/** @brief Return a snapshot of the current memory statistics.
 *
 */
XIA_EXPORT void xia_mem_stats(unsigned long *total, unsigned long *current,
							  unsigned long *peak)
{
  *total   = TOTAL_BYTES_ALLOCATED;
  *current = CURRENT_BYTES_ALLOCATED;
  *peak    = PEAK_BYTES_ALLOCATED;
}


/** @brief Adds a new memory block to the manager list.
 *
 * Any memory allocation failures are treated as unchecked exceptions.
 */
static void xia__add_blk(void *ptr, size_t n, char *file, int line)
{
  mem_blk_t *new_blk = NULL;


  new_blk = (mem_blk_t *)malloc(sizeof(mem_blk_t));
  ASSERT(new_blk != NULL);

  new_blk->addr  = ptr;
  new_blk->n     = n;
  new_blk->line  = line;
  strncpy(new_blk->file, file, MAX_FILE_SIZE);

  new_blk->next = BLK_HEAD;
  BLK_HEAD = new_blk;

  TOTAL_BYTES_ALLOCATED   += (unsigned long)n;
  CURRENT_BYTES_ALLOCATED += (unsigned long)n;

  if (CURRENT_BYTES_ALLOCATED > PEAK_BYTES_ALLOCATED) {
	PEAK_BYTES_ALLOCATED = CURRENT_BYTES_ALLOCATED;
  }

}


/** @brief removes an existing 
 *
 */
static void xia__remove_blk(void *ptr)
{
  mem_blk_t *b    = NULL;
  mem_blk_t *prev = NULL;


  /* NULL pointers are allowed since the C standard states that free()
   * takes no action if passed a NULL pointer.
   */
  if (!ptr) {
	return;
  }

  prev = NULL;
  for (b = BLK_HEAD; b != NULL; b = b->next) {
	if (b->addr == ptr) {
	  CURRENT_BYTES_ALLOCATED -= (unsigned long)(b->n);

	  if (prev == NULL) {
		BLK_HEAD = b->next;
	  
	  } else {
		prev->next = b->next;
	  }

	  free(b);
	  b = NULL;
	  return;
	}

	prev = b;
  }

  /* This means that we couldn't find the pointer in our list of blocks. */
  ASSERT(FALSE_);
}


/** @brief Create a memory checkpoint based on the currently allocated
 * values.
 *
 */
XIA_EXPORT void xia_mem_checkpoint(mem_check_pt_t *chk_pt)
{
  mem_point_t *p = NULL;

  mem_blk_t *b = NULL;

  char *key = NULL;


  ASSERT(chk_pt != NULL);


  p = xia_mem_point_create(256);
  ASSERT(p !=  NULL);

  /* Add all of the currently allocated blocks to the checkpoint hash. */
  for (b = BLK_HEAD; b != NULL; b = b->next) {

	key = (char *)malloc(256);
	ASSERT(key != NULL);

	sprintf(key, "%s:%u:%p", b->file, b->line, b->addr);
	xia_mem_point_insert(p, key);
	key = NULL;
  }

  *chk_pt = (mem_check_pt_t)p;
}


/** @brief Free a previously allocated memory checkpoint.
 *
 */
XIA_EXPORT void xia_mem_checkpoint_free(mem_check_pt_t *chk_pt)
{
  ASSERT(chk_pt != NULL);


  xia_mem_point_free((mem_point_t *)*chk_pt);
}


/** @brief Compares a memory checkpoint previously created with
 * xia_mem_checkpoint() to the current memory.
 *
 * @a out defines where the results of the comparison are printed. The
 * special string "stdout" may be used to write the output to the stdout FILE
 * descriptor. Otherwise, a file with the specified name will be opened and
 * used.
 *
 */
XIA_EXPORT void xia_mem_checkpoint_cmp(mem_check_pt_t *pt, char *out)
{
  mem_blk_t *b = NULL;

  FILE *fp = NULL;

  char key[256];


  ASSERT(pt != NULL);
  ASSERT(out != NULL);


  if (STREQ(out, "stdout")) {
	fp = stdout;
  } else {
	fp = fopen(out, "w");
	ASSERT(fp != NULL);
  }

  fprintf(fp, "\n**********MEMORY LEAK START**********\n");

  for (b = BLK_HEAD; b != NULL; b = b->next) {
	sprintf(key, "%s:%u:%p", b->file, b->line, b->addr);
	
	if (!xia_mem_point_key_exists((mem_point_t *)*pt, key)) {
	  fprintf(fp, "\n");
	  fprintf(fp, "Allocation: %s:%d\n", b->file, b->line);
	  fprintf(fp, "Size: %d bytes\n", b->n);
	}
  }
  
  fprintf(fp, "**********MEMORY LEAK END**********\n");

  fflush(NULL);

  if (!STREQ(out, "stdout")) {
	fclose(fp);
  }
}
