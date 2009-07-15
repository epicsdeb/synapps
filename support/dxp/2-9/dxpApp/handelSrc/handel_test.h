/*
 * handel_test.h
 *
 * Header file w/ various macros and prototypes that
 * support Handel unit tests.
 *
 * Created 08/05/03 -- PJF
 *
 * Copyright (c) 2003 X-ray Instrumentation Associates
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
 */

#ifdef _DEBUG

#ifndef HANDEL_TEST_H
#define HANDEL_TEST_H

#include <stdio.h>
#include <stdlib.h>

#include "handel_errors.h"
#include "handeldef.h"

#include "xia_handel.h"


/** Structures **/
typedef struct _GlobalCtx {
  Detector       *det;
  FirmwareSet    *fs;
  XiaDefaults    *defs;
  Module         *mod;
  DetChanElement *chan;
} Ctx_t;


/** Helper Function Prototypes **/
HANDEL_SHARED void HANDEL_API xiaSaveGlobalCtx(Ctx_t *ctx);
HANDEL_SHARED void HANDEL_API xiaRestoreGlobalCtx(Ctx_t *ctx);
HANDEL_SHARED void HANDEL_API xiaCreateSaturnCfg(char *ini, char *fdd);
HANDEL_SHARED void HANDEL_API xiaCreateDxp4c2xCfg(char *ini, char *fdd);
HANDEL_SHARED void HANDEL_API xiaCreateMicroDXPCfg(char *ini);
HANDEL_SHARED void HANDEL_API xiaDestroyCfg(char *ini);
HANDEL_SHARED void HANDEL_API xiaStart(char *ini);


/** Unit Test Prototypes **/
HANDEL_SHARED void xiaHandelTest(void);
HANDEL_SHARED void xiaHandelDynModuleTest(void);
HANDEL_SHARED void xiaHandelFileTest(void);
HANDEL_SHARED void xiaHandelRunParamsTest(void);
HANDEL_SHARED void xiaHandelRunControlTest(void);
HANDEL_SHARED void xiaHandelSystemTest(void);


/** Macros **/
#define RUN(x)                                       \
sprintf(info_string, "***STARTING TEST: %s", #x);    \
xiaLogDebug("RUN MACRO", info_string);               \
status = x();                                        \
if (status == XIA_SUCCESS) {                         \
  printf("%s ...Completed.\n\n", #x);                \
} else                                               \
  printf("%s ...Completed\n", #x)

#define TEST(x)                    \
do {                               \
  fflush(NULL);                    \
  if (x) {                         \
                                   \
  } else {                         \
	printf("%s...FAILED! [%s]:%d\n", #x, __FILE__, __LINE__);  \
  }                                \
} while (FALSE_)


#define TEST_FAIL(x)                    \
do {                               \
  fflush(NULL);                    \
  if (x) {                         \
                                   \
  } else {                         \
	printf("TERMINATED: %s...FAILED! [%s]:%d\n", #x, __FILE__, __LINE__);  \
    exit(1);                                                   \
  }                                \
} while (FALSE_)

#endif /* HANDEL_TEST_H */

#endif /* _DEBUG */




