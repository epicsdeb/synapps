/*
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
 */

/* Some header files are Copyright (c) 2001, Fermi National Accelerator
 * Laboratory (FNAL). See vendor/sjyLX-2.2/README for a complete description
 * of the Terms And Conditions.
 */

/* CAMAC driver wrapper built on the FNAL CAMAC driver sjyLX v2.2.
 *
 * This wrapper uses the same interface as camacdll.c so that as much code
 * as possible can be shared between md_win95.c and md_linux.c.
 */

#include "ieee_fun_types.h"
#include "camac_murmur_msg_c.h"

#include "camacso.h"
#include "camac_errors.h"

#include "xia_common.h"



static int _xia_camblock_transfer(int branch, int crate, int slot, short cam_f,
				  short cam_a, long len, short *buf);
static int _xia_camword_transfer(int branch, int crate, int slot, short cam_f,
				 short cam_a, short *word);



/** @brief Initializes the CAMAC interface
 *
 * NOT USED on Linux
 */
long xia_caminit(short* camadr)
{
  unsigned int status;

  int branch;


  branch = (int)((8 * camadr[0]) + camadr[1]);

  /* As near as I can tell from sjy_cdchn.c, 'route' isn't used, nor is
   * it mentioned in the documentation. qxtab.c sets it to 0, which is what
   * I will do as well.
   */
  status = cdchn(branch, LFLAG_OPEN, 0);

  if (status != CAM_S_SUCCESS) {
    return XIA_CAMAC_CDCHN;
  }

  return 0;
}


/** @brief Performs the request CAMAC transfer
 *
 * @param camadr An array of information describing the CAMAC address:
 * @arg camadr[0] = SCSI bus
 * @arg camadr[1] = Crate ID
 * @arg camadr[2] = Slot
 *
 * @param cam_f The CAMAC F function for this command
 *
 * @param cam_a The CAMAC A address for this command
 *
 * @param len The number of bytes to be transmitted or received
 *
 * @param mode Transfer mode (Q-Stop or Single Word) (UNUSED)
 *
 * @param buf Array of data (16-bit) to be sent or received
 */
long xia_camxfr(short* camadr, short cam_f, short cam_a, long len,
		short mode, short* buf)
{
  int branch = 0;
  int crate  = 0;
  int slot   = 0;
  int ext    = 0;
  int subad  = 0;
  int bus    = 0;

  unsigned int status;


  UNUSED(mode);



  if (len < 0) {
    return XIA_CAMAC_LEN;
  }

  bus    = (int)camadr[0];
  crate  = (int)camadr[1];
  slot   = (int)camadr[2];

  if ((bus < 0) ||
      (bus > 3))
    {
      return XIA_CAMAC_BUS;
    }

  if ((crate < 0) ||
      (crate > 7))
    {
      return XIA_CAMAC_CRATE;
    }

  if ((slot < 0) ||
      (slot > 31))
    {
      return XIA_CAMAC_SLOT;
    }

  branch = (int)((8 * bus) + crate); 

  /* As near as I can tell from the specification and sample code, 'subad'
   * is unused.
   */
  status = cdreg(&ext, branch, crate, slot, subad);

  if (status != CAM_S_SUCCESS) {
    fprintf(stderr, "cdreg status = %d\n", status);
    return XIA_CAMAC_CDREG;
  }

  /* Jorway 73A is considered 'parallel' for this routine */
  status = ccctype(branch, 0, 1);
  
  if (status != CAM_S_SUCCESS) {
    return XIA_CAMAC_CCCTYPE;
  }

  status = cccbyp(ext, 0);

  if ((status != CAM_S_SUCCESS) &&
      (status != CAM_S_PARALLEL))
    {
      return XIA_CAMAC_CCCBYP;
    }

  status = cccoff(ext, 0);

  if ((status != CAM_S_SUCCESS) &&
      (status != CAM_S_PARALLEL))
    {
      return XIA_CAMAC_CCCOFF;
    }

  status = ccci(ext, 0);

  if (status != CAM_S_SUCCESS) {
    return XIA_CAMAC_CCCI;
  }

  status = cccc(ext);

  if (status != CAM_S_SUCCESS) {
    return XIA_CAMAC_CCCC;
  }

  status = cccz(ext);

  if (status != CAM_S_SUCCESS) {
    return XIA_CAMAC_CCCZ;
  }

  status = ccci(ext, 0);

  if (status != CAM_S_SUCCESS) {
    return XIA_CAMAC_CCCI;
  }

  if (len > 1) {
    status = _xia_camblock_transfer(branch, crate, slot, cam_f, cam_a, len,
				    buf);
  } else {
    status = _xia_camword_transfer(branch, crate, slot, cam_f, cam_a, &buf[0]);
  }

  if (status != CAM_S_SUCCESS) {
    return status;
  }

  return XIA_CAMAC_SUCCESS;
}


/** @brief Transfer a block of data using the specified F & A command
 *
 */
static int _xia_camblock_transfer(int branch, int crate, int slot, short cam_f,
				  short cam_a, long len, short *buf)
{
  int status;
  int ext;
  int i;
  int transfer_len_words;

  unsigned int left_to_transfer;
  
  int control_blk[4];

  short *data = NULL;


  status = cdreg(&ext, branch, crate, slot, cam_a);

  if (status != CAM_S_SUCCESS) {
    return XIA_CAMAC_CDREG;
  }

  left_to_transfer = len;
  data             = buf;
  control_blk[3]   = 0;

  while (left_to_transfer > 0) {

    if (left_to_transfer > MAX_BLOCK_SIZE) {
      transfer_len_words = (int)(MAX_BLOCK_SIZE / 2);
    } else {
      transfer_len_words = (int)(left_to_transfer / 2);
    }

    control_blk[0] = transfer_len_words;

    status = csubc(cam_f, ext, data, control_blk);

    if (status != CAM_S_SUCCESS) {
      return XIA_CAMAC_CSUBC;
    }

    data = buf + transfer_len_words;
    left_to_transfer -= (transfer_len_words * 2);
  }

  return XIA_CAMAC_SUCCESS;
}


/** @brief Transfer a single 16-bit word
 *
 */
static int _xia_camword_transfer(int branch, int crate, int slot, short cam_f,
				 short cam_a, short *word)
{
  int status;
  int ext   = 0;
  int Q_res = 0;

  status = cdreg(&ext, branch, crate, slot, cam_a);

  if (status != CAM_S_SUCCESS) {
    return XIA_CAMAC_CDREG;
  }

  status = cssa(cam_f, ext, word, &Q_res);

  if (status != CAM_S_SUCCESS) {
    return XIA_CAMAC_CSSA;
  }

  return XIA_CAMAC_SUCCESS;
}
