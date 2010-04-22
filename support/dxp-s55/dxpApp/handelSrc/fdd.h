/*
 *  fdd.h
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
 * $Id: fdd.h,v 1.3 2009-07-06 18:24:29 rivers Exp $
 *
 */


#ifndef XIA_FDD_H
#define XIA_FDD_H

#include "fdddef.h"
/* Define some generic constants for use by FDD */
#include "handel_generic.h"
#include "md_generic.h"

#include "xia_handel_structures.h"

/*
 *    CAMAC status Register control codes
 */
#define ALLCHAN              -1

/* If this is compiled by a C++ compiler, make it clear that these are C routines */
#ifdef __cplusplus
extern "C" {
#endif

#ifdef _FDD_PROTO_
#include <stdio.h>

/*
 * following are internal prototypes for fdd.c routines
 */
FDD_IMPORT int FDD_API xiaFddInitialize(void);
FDD_IMPORT int FDD_API xiaFddInitLibrary(void);
  FDD_IMPORT int FDD_API xiaFddGetFirmware(const char *filename, char *path,
                                           const char *ftype, 
                                           double pt, unsigned int nother, 
                                           char **others, 
                                           const char *detectorType,
                                           char newfilename[MAXFILENAME_LEN], 
					 char rawFilename[MAXFILENAME_LEN]);
FDD_IMPORT int FDD_API xiaFddAddFirmware(const char *filename, const char *ftype, 
					 double ptmin, double ptmax, 
					 unsigned short nother, const char **others, 
					 const char *ffile, unsigned short numFilter,
					 parameter_t *filterInfo);
FDD_IMPORT int FDD_API xiaFddCleanFirmware(const char *filename);
FDD_IMPORT int FDD_API xiaFddGetNumFilter(const char *filename, double peakingTime, unsigned int nKey,
					  char **keywords, unsigned short *numFilter);
FDD_IMPORT int FDD_API xiaFddGetFilterInfo(const char *filename, double peakingTime, unsigned int nKey,
					   char **keywords, parameter_t *filterInfo);

#else									/* Begin old style C prototypes */
/*
 * following are internal prototypes for fdd.c routines
 */
FDD_IMPORT int FDD_API xiaFddInitialize();
FDD_IMPORT int FDD_API xiaFddInitLibrary();
FDD_IMPORT int FDD_API xiaFddGetFirmware();
FDD_IMPORT int FDD_API xiaFddAddFirmware();
FDD_IMPORT int FDD_API xiaFddCleanFirmware();
FDD_IMPORT int FDD_API xiaFddGetNumFilter();
FDD_IMPORT int FDD_API xiaFddGetFilterInfo();

#endif                                  /*   end if _FDD_PROTO_ */

/* If this is compiled by a C++ compiler, make it clear that these are C routines */
#ifdef __cplusplus
}
#endif

#endif						/* Endif for XIA_FDD_H */
