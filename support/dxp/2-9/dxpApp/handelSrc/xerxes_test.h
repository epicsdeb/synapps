/*
 *
 * Header file w/ various macros and prototypes that
 * support Xerxes unit tests.
 *
 * Created 11/11/03 -- PJF
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

#ifndef XERXES_TEST_H
#define XERXES_TEST_H

#include <stdio.h>
#include <stdlib.h>

#include "xerxes_errors.h"
#include "xerxesdef.h"


/** Unit Test Prototypes **/
XERXES_SHARED void XERXES_API dxp_test_memory(void);


/** Macros **/
#define RUN(x)                          \
status = x();                           \
if (status == DXP_SUCCESS) {            \
  printf("%s ...Completed.\n\n", #x);  \
} else                                  \
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

#endif /* XERXES_TEST_H */

#endif /* _DEBUG */




