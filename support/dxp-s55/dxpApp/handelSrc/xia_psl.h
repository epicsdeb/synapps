/*
 * xia_psl.h
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
 * $Id: xia_psl.h,v 1.3 2009-07-06 18:24:32 rivers Exp $
 *
 */


#ifndef XIA_PSL_H
#define XIA_PSL_H

#include "xia_system.h"
#include "xia_handel_structures.h"
#include "psldef.h"
#include "xerxes.h"
#include "xia_common.h"


/** Globals **/
static double CLOCK_SPEED;

#ifdef _cplusplus
extern "C" {
#endif /* _cplusplus */
PSL_STATIC int PSL_API pslValidateModule(Module *module);
PSL_STATIC int PSL_API pslValidateDefaults(XiaDefaults *defaults);
PSL_STATIC int PSL_API pslDownloadFirmware(int detChan, char *type, char *file, 
                                           Module *m, char *rawFilename,
                                           XiaDefaults *defs);
PSL_STATIC int PSL_API pslSetAcquisitionValues(int detChan, char *name, void *value, 
					       XiaDefaults *defaults, 
					       FirmwareSet *firmwareSet, 
					       CurrentFirmware *currentFirmware, 
					       char *detectorType, double gainScale, 
					       Detector *detector, int detector_chan, Module *m,
											   int modChan);
PSL_STATIC int PSL_API pslGetAcquisitionValues(int detChan, char *name,
											   void *value, 
											   XiaDefaults *defaults);
PSL_STATIC int PSL_API pslGainOperation(int detChan, char *name, void *value, Detector *detector, 
					int detector_chan, XiaDefaults *defaults, double gainScale, 
					CurrentFirmware *currentFirmware, char *detectorType, Module *m);
PSL_STATIC int PSL_API pslGainChange(int detChan, double deltaGain, 
									 XiaDefaults *defaults, 
									 CurrentFirmware *currentFirmware, 
									 char *detectorType, double gainScale,
									 Detector *detector, int detector_chan,
									 Module *m, int modChan);
PSL_STATIC int PSL_API pslGainCalibrate(int detChan, Detector *det, int modChan,
										Module *m, XiaDefaults *defs,
										double deltaGain, double gainScale);
PSL_STATIC int pslStartRun(int detChan, unsigned short resume, XiaDefaults *defs,
						   Module *m);
PSL_STATIC int pslStopRun(int detChan, Module *m);
PSL_STATIC int PSL_API pslGetRunData(int detChan, char *name, void *value,
									 XiaDefaults *defaults, Module *m);
PSL_STATIC int pslSetPolarity(int detChan, Detector *det, int detectorChannel,
							  XiaDefaults *defs, Module *m);
PSL_STATIC int PSL_API pslSetDetectorTypeValue(int detChan, Detector *detector, int detectorChannel, XiaDefaults *defaults);
PSL_STATIC int PSL_API pslGetDefaultAlias(char *alias, char **names, double *values);
PSL_STATIC unsigned int PSL_API pslGetNumDefaults(void);
PSL_STATIC int PSL_API pslGetParameter(int detChan, const char *name, unsigned short *value);
PSL_STATIC int PSL_API pslSetParameter(int detChan, const char *name, unsigned short value);
PSL_STATIC int PSL_API pslUserSetup(int detChan, XiaDefaults *defaults, FirmwareSet *firmwareSet, 
									CurrentFirmware *currentFirmware, char *detectorType, 
									double gainScale, Detector *detector, int detector_chan,
									Module *m, int modChan);
PSL_STATIC int PSL_API pslDoSpecialRun(int detChan, char *name, double gainScale, void *info, 
				       XiaDefaults *defaults, Detector *detector, int detector_chan);
PSL_STATIC int PSL_API pslGetSpecialRunData(int detChan, char *name, void *value, XiaDefaults *defaults);
PSL_STATIC int PSL_API pslGetNumParams(int detChan, unsigned short *numParams);
PSL_STATIC int PSL_API pslGetParamData(int detChan, char *name, void *value);
PSL_STATIC int PSL_API pslGetParamName(int detChan, unsigned short index, char *name);
PSL_STATIC int pslBoardOperation(int detChan, char *name, void *value,
								 XiaDefaults *defs);

PSL_STATIC boolean_t PSL_API pslCanRemoveName(char *name);

PSL_STATIC int PSL_API pslUnHook(int detChan);
  
  /* Shared routines */
  PSL_SHARED int PSL_API pslGetDefault(char *name, void *value,
									   XiaDefaults *defaults);
  PSL_SHARED int PSL_API pslSetDefault(char *name, void *value,
									   XiaDefaults *defaults);
  PSL_SHARED int PSL_API pslGetModChan(int detChan, Module *m,
									   unsigned int *modChan);
  PSL_SHARED int PSL_API pslDestroySCAs(Module *m, unsigned int modChan);
  PSL_SHARED XiaDaqEntry * pslFindEntry(char *name, XiaDefaults *defs);
  PSL_SHARED int pslInvalidate(char *name, XiaDefaults *defs);
  PSL_SHARED void pslDumpDefaults(XiaDefaults *defs);
  PSL_SHARED double pslU64ToDouble(unsigned long *u64);
  PSL_SHARED int pslRemoveDefault(char *name, XiaDefaults *defs,
								  XiaDaqEntry **removed);

#ifdef _cplusplus
}
#endif /* _cplusplus */

/* Logging macro wrappers */
#define pslLogError(x, y, z)	utils->funcs->dxp_md_log(MD_ERROR, (x), (y), (z), __FILE__, __LINE__)
#define pslLogWarning(x, y)	utils->funcs->dxp_md_log(MD_WARNING, (x), (y), 0, __FILE__, __LINE__)
#define pslLogInfo(x, y)	utils->funcs->dxp_md_log(MD_INFO, (x), (y), 0, __FILE__, __LINE__)
#define pslLogDebug(x, y)	utils->funcs->dxp_md_log(MD_DEBUG, (x), (y), 0, __FILE__, __LINE__)


/* Memory allocation wrappers */

/* This is obviously a major hack, but the problem that I'm not dealing with here
 * is that this code is shared across all of the PSLs. Each PSL has it's own 
 * memory naming convention. In the future, we probably shouldn't allow psl.c
 * to do any memory management, but that is neither here nor there.
 */
#ifdef USE_XIA_MEM_MANAGER
#include "xia_mem.h"
#define MALLOC(n) xia_mem_malloc((n), __FILE__, __LINE__)
#define FREE(ptr) xia_mem_free(ptr)
#else
#define MALLOC(n) utils->funcs->dxp_md_alloc(n)
#define FREE(ptr) utils->funcs->dxp_md_free(ptr)
#endif /* USE_XIA_MEM_MANAGER */


/* Wrappers around other MD utility routines. */
#define FGETS(s, size, stream) utils->funcs->dxp_md_fgets((s), (size), (stream))


#endif /* XIA_PSL_H */
