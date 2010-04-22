/*
 *
 * handel_mem.c
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
 * $Id: handel_mem.c,v 1.2 2009-07-06 18:24:29 rivers Exp $
 */


#ifdef USE_XIA_MEM_MANAGER
#include "xia_mem.h"

static mem_check_pt_t CHECK_POINT;
#endif /* USE_XIA_MEM_MANAGER */

#include "handeldef.h"

#include "handel_errors.h"

#include "xia_handel.h"


/** @brief Return the specified memory statistics.
 *
 * Requires XIA's custom memory manager.
 *
 */
HANDEL_EXPORT int HANDEL_API xiaMemStatistics(unsigned long *total,
											  unsigned long *current,
											  unsigned long *peak)
{
#ifdef USE_XIA_MEM_MANAGER

  if (!total) {
	xiaLogError("xiaMemStatistics", "'total' must be a non-NULL pointer",
				XIA_NULL_VALUE);
	return XIA_NULL_VALUE;
  }

  if (!current) {
	xiaLogError("xiaMemStatistics", "'current' must be a non-NULL pointer",
				XIA_NULL_VALUE);
	return XIA_NULL_VALUE;
  }

  if (!peak) {
	xiaLogError("xiaMemStatistics", "'peak' must be a non-NULL pointer",
				XIA_NULL_VALUE);
	return XIA_NULL_VALUE;
  }

  xia_mem_stats(total, current, peak);

  return XIA_SUCCESS;

#else

  UNUSED(total);
  UNUSED(peak);
  UNUSED(current);

  return XIA_UNIMPLEMENTED;

#endif /* USE_XIA_MEM_MANAGER */

}


/** @brief Sets a memory checkpoint
 *
 */
HANDEL_EXPORT void HANDEL_API xiaMemSetCheckpoint(void)
{
#ifdef USE_XIA_MEM_MANAGER

  xia_mem_checkpoint(&CHECK_POINT);

#else

  return;

#endif /* USE_XIA_MEM_MANAGER */
}


/** @brief Compare the current memory to the last checkpoint set.
 *
 */
HANDEL_EXPORT void HANDEL_API xiaMemLeaks(char *out)
{
#ifdef USE_XIA_MEM_MANAGER

  xia_mem_checkpoint_cmp(&CHECK_POINT, out);

#else

  UNUSED(out);
  return;

#endif /* USE_XIA_MEM_MANAGER */
}
