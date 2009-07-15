/*
 * handel_test.c
 *
 * Created 10/01/03 -- PJF
 *
 * Copyright (c) 2003, X-ray Instrumentation Associates
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


#include "handeldef.h"
#include "xia_handel.h"
#include "xia_common.h"
#include "handel_test.h"
#include "handel_constants.h"

#include "psl.h"

#include "xia_assert.h"

#include "xerxes.h"


/* This routine wrappers around the imported
 * dxp_unit_test() routine. The MSVC compiler
 * (and possibly others) will warn about
 * using a function w/ a non-confirmed
 * address, if you try and use dxp_unit_test()
 * directly in the 'testers' array.
 */
HANDEL_STATIC void xiaXerxesTest(void);


typedef void (*Test_func_t)(void);

typedef struct _Test {
  unsigned short mask;
  Test_func_t f;

} Test_t;


static Test_t testers[] = {
  { XIA_HANDEL_TEST_MASK,             xiaHandelTest},
  { XIA_HANDEL_DYN_MODULE_TEST_MASK,  xiaHandelDynModuleTest},
  { XIA_HANDEL_FILE_TEST_MASK,        xiaHandelFileTest},
  { XIA_HANDEL_RUN_PARAMS_TEST_MASK,  xiaHandelRunParamsTest},
  { XIA_HANDEL_RUN_CONTROL_TEST_MASK, xiaHandelRunControlTest},
  { XIA_XERXES_TEST_MASK,             xiaXerxesTest},
  { XIA_HANDEL_SYSTEM_TEST_MASK,      xiaHandelSystemTest},
};

#define NUM_TESTERS  (sizeof(testers) / sizeof(testers[0]))


/** @brief Runs the embedded unit test routines
 *
 * Routine is used to start and control execution of the \n
 * unit tests embedded in Handel and the associated PSL \n
 * layer. The tests are controlled by ORing together the \n
 * flags of the tests that should be run. The test flags \n
 * are defined in handel_constants.h
 *
 * @arg tests A bit-field representing the tests that should be run
 *
 */
HANDEL_EXPORT void HANDEL_API xiaUnitTests(unsigned short tests)
{
  size_t i;


  for (i = 0; i < NUM_TESTERS; i++) {
	if (tests & testers[i].mask) {
	  testers[i].f();
	}
  }

}


HANDEL_SHARED void HANDEL_API xiaSaveGlobalCtx(Ctx_t *ctx)
{
  ctx->det  = xiaDetectorHead;
  ctx->fs   = xiaFirmwareSetHead;
  ctx->defs = xiaDefaultsHead;
  ctx->mod  = xiaModuleHead;
  ctx->chan = xiaDetChanHead;

  xiaDetectorHead    = NULL;
  xiaFirmwareSetHead = NULL;
  xiaDefaultsHead    = NULL;
  xiaModuleHead      = NULL;
  xiaDetChanHead     = NULL;
}


HANDEL_SHARED void HANDEL_API xiaRestoreGlobalCtx(Ctx_t *ctx)
{
  xiaDetectorHead    = ctx->det;
  xiaFirmwareSetHead = ctx->fs;
  xiaDefaultsHead    = ctx->defs;
  xiaModuleHead      = ctx->mod;
  xiaDetChanHead     = ctx->chan;

  ctx->det  = NULL;
  ctx->fs   = NULL;
  ctx->defs = NULL;
  ctx->mod  = NULL;
  ctx->chan = NULL;
}


/** @brief Creates a Saturn cfg using the supplied INI file name and FDD file.
 *
 */
HANDEL_SHARED void HANDEL_API xiaCreateSaturnCfg(char *ini, char *fdd)
{
  FILE *fp = NULL;

  
  ASSERT(ini != NULL);
  ASSERT(fdd != NULL);


  fp = fopen(ini, "w");
  ASSERT(fp != NULL);

  /* Write out a simple Saturn INI file.
   * ASSUME that the FDD file is present
   * in the working directory.
   */
  fprintf(fp, "[detector definitions]\n");
  fprintf(fp, "START #1\n");
  fprintf(fp, "alias = detector1\n");
  fprintf(fp, "number_of_channels = 1\n");
  fprintf(fp, "type = reset\n");
  fprintf(fp, "type_value = 10.0\n");
  fprintf(fp, "channel0_gain = 6.6\n");
  fprintf(fp, "channel0_polarity = +\n");
  fprintf(fp, "END #1\n\n");
  
  fprintf(fp, "[firmware definitions]\n");
  fprintf(fp, "START #1\n");
  fprintf(fp, "alias = firmware1\n");
  fprintf(fp, "filename = %s\n", fdd);
  fprintf(fp, "END #1\n\n");

  fprintf(fp, "[module definitions]\n");
  fprintf(fp, "START #1\n");
  fprintf(fp, "alias = module1\n");
  fprintf(fp, "module_type = dxpx10p\n");
  fprintf(fp, "number_of_channels = 1\n");
  fprintf(fp, "interface = epp\n");
  fprintf(fp, "epp_address = 0x378\n");
  fprintf(fp, "channel0_alias = 0\n");
  fprintf(fp, "channel0_detector = detector1:0\n");
  fprintf(fp, "channel0_gain = 1.0\n");
  fprintf(fp, "firmware_set_all = firmware1\n");
  fprintf(fp, "END #1\n\n");

  fclose(fp);
}


HANDEL_SHARED void HANDEL_API xiaCreateDxp4c2xCfg(char *ini, char *fdd)
{
  FILE *fp = NULL;

  
  ASSERT(ini != NULL);
  ASSERT(fdd != NULL);


  fp = fopen(ini, "w");
  ASSERT(fp != NULL);

  /* Write out a simple DXP4C2X INI file.
   * ASSUME that the FDD file is present
   * in the working directory.
   */
  fprintf(fp, "[detector definitions]\n");
  fprintf(fp, "START #1\n");
  fprintf(fp, "alias = detector1\n");
  fprintf(fp, "number_of_channels = 1\n");
  fprintf(fp, "type = reset\n");
  fprintf(fp, "type_value = 10.0\n");
  fprintf(fp, "channel0_gain = 6.6\n");
  fprintf(fp, "channel0_polarity = +\n");
  fprintf(fp, "END #1\n\n");
  
  fprintf(fp, "[firmware definitions]\n");
  fprintf(fp, "START #1\n");
  fprintf(fp, "alias = firmware1\n");
  fprintf(fp, "filename = %s\n", fdd);
  fprintf(fp, "END #1\n\n");

  fprintf(fp, "[module definitions]\n");
  fprintf(fp, "START #1\n");
  fprintf(fp, "alias = module1\n");
  fprintf(fp, "module_type = dxp4c2x\n");
  fprintf(fp, "number_of_channels = 4\n");
  fprintf(fp, "interface = j73a\n");
  fprintf(fp, "scsibus_number = 0\n");
  fprintf(fp, "crate_number = 2\n");
  fprintf(fp, "slot = 6\n");
  fprintf(fp, "channel0_alias = 0\n");
  fprintf(fp, "channel0_detector = detector1:0\n");
  fprintf(fp, "channel0_gain = 1.0\n");
  fprintf(fp, "channel1_alias = 1\n");
  fprintf(fp, "channel1_detector = detector1:0\n");
  fprintf(fp, "channel1_gain = 1.0\n");
  fprintf(fp, "channel2_alias = 2\n");
  fprintf(fp, "channel2_detector = detector1:0\n");
  fprintf(fp, "channel2_gain = 1.0\n");
  fprintf(fp, "channel3_alias = 3\n");
  fprintf(fp, "channel3_detector = detector1:0\n");
  fprintf(fp, "channel3_gain = 1.0\n");
  fprintf(fp, "firmware_set_all = firmware1\n");
  fprintf(fp, "END #1\n\n");

  fclose(fp);

}


HANDEL_SHARED void HANDEL_API xiaCreateMicroDXPCfg(char *ini)
{
  FILE *fp = NULL;

  
  ASSERT(ini != NULL);


  fp = fopen(ini, "w");
  ASSERT(fp != NULL);

  /* Write out a simple microDXP INI file.
   */
  fprintf(fp, "[detector definitions]\n");
  fprintf(fp, "START #1\n");
  fprintf(fp, "alias = detector1\n");
  fprintf(fp, "number_of_channels = 1\n");
  fprintf(fp, "type = reset\n");
  fprintf(fp, "type_value = 10.0\n");
  fprintf(fp, "channel0_gain = 6.6\n");
  fprintf(fp, "channel0_polarity = +\n");
  fprintf(fp, "END #1\n\n");
  
  fprintf(fp, "[firmware definitions]\n");
  fprintf(fp, "START #1\n");
  fprintf(fp, "alias = firmware1\n");
  fprintf(fp, "ptrr = 1\n");
  fprintf(fp, "min_peaking_time = 0.000\n");
  fprintf(fp, "max_peaking_time = 1.000\n");
  fprintf(fp, "fippi = f01x2p0g.fip\n");
  fprintf(fp, "dsp = udxptest.hex\n");
  fprintf(fp, "num_filter = 0\n");
  fprintf(fp, "END #1\n\n");

  fprintf(fp, "[module definitions]\n");
  fprintf(fp, "START #1\n");
  fprintf(fp, "alias = module1\n");
  fprintf(fp, "module_type = udxp\n");
  fprintf(fp, "number_of_channels = 1\n");
  fprintf(fp, "interface = serial\n");
  fprintf(fp, "com_port = 4\n");
  fprintf(fp, "baud_rate = 115200\n");
  fprintf(fp, "channel0_alias = 0\n");
  fprintf(fp, "channel0_detector = detector1:0\n");
  fprintf(fp, "channel0_gain = 1.0\n");
  fprintf(fp, "firmware_set_all = firmware1\n");
  fprintf(fp, "END #1\n\n");

  fclose(fp);
}


HANDEL_SHARED void HANDEL_API xiaDestroyCfg(char *ini)
{
  int status;


  ASSERT(ini != NULL);
  

  status = remove(ini);
  ASSERT(status != -1);
}


HANDEL_SHARED void HANDEL_API xiaStart(char *ini)
{
  int status;


  ASSERT(ini != NULL);


  status = xiaInit(ini);
  TEST_FAIL(status == XIA_SUCCESS);
  status = xiaStartSystem();
  TEST_FAIL(status == XIA_SUCCESS);
}


HANDEL_STATIC void xiaXerxesTest(void)
{
  dxp_unit_test();
}
