/*
 * psl_x10p.h
 *
 * Copyright (c) 2005, XIA LLC
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
 * $Id: psl_x10p.h,v 1.3 2009-07-06 18:24:30 rivers Exp $
 *
 */


#ifndef _PSL_X10P_H_
#define _PSL_X10P_H_


/** FUNCTION POINTERS **/
typedef int (*SetAcqValue_FP)(int detChan, void *value, FirmwareSet *fs,
                              char *detType, XiaDefaults *defs, double preampGain,
                              double gainScale, Module *m, Detector *det,
                              int detector_chan);
typedef int (*SynchAcqValue_FP)(int detChan, int det_chan, Module *m,
                                Detector *det, XiaDefaults *defs);



/** STRUCTURES **/

/* A generic acquisition value */
typedef struct _AcquisitionValue
{

  char *           name;
  boolean_t        isDefault;
  boolean_t        isSynch;
  double           def;
  SetAcqValue_FP   setFN;
  SynchAcqValue_FP synchFN;

}
AcquisitionValue_t;


/** MACROS **/

/* This saves us a lot of typing. */
#define ACQUISITION_VALUE(x) \
  PSL_STATIC int pslDo ## x (int detChan, void *value, FirmwareSet *fs, \
        char *detectorType, XiaDefaults *defs, \
        double preampGain, double gainScale, \
        Module *m, Detector *det, int detector_chan)


/** CONSTANTS **/        
#define MIN_MAXWIDTH        1
#define MAX_MAXWIDTH        255        
        
/** Memory Management **/
DXP_MD_ALLOC x10p_psl_md_alloc;
DXP_MD_FREE  x10p_psl_md_free;

#ifdef USE_XIA_MEM_MANAGER
#include "xia_mem.h"
#define x10p_psl_md_alloc(n)  xia_mem_malloc((n), __FILE__, __LINE__)
#define x10p_psl_md_free(ptr) xia_mem_free(ptr)
#endif /* USE_XIA_MEM_MANAGER */

#endif /* _PSL_X10P_H_ */


