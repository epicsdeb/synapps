/*
 * handel_xerxes.h
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
 * $Id: handel_xerxes.h,v 1.4 2009-07-06 18:24:30 rivers Exp $
 *
 */


#ifndef HANDEL_XERXES_H
#define HANDEL_XERXES_H

#include "handeldef.h"
#include "xia_handel_structures.h"

/** Constants **/

/* This is very, very hacky and will be made
 * more robust soon.
 */
/*
#ifdef EXCLUDE_UDXP
#define KNOWN_BOARDS  6
#else
#define KNOWN_BOARDS  8
#endif
*/



static char *SYS_NULL[1]  = { "NULL" };

static char *BOARD_LIST[] = {
#ifndef EXCLUDE_DXP4C2X
  "dxp2x",
  "dxp4c2x",
#endif /* EXCLUDE_DXP4C2X */
#ifndef EXCLUDE_DXPX10P
  "dxpx10p",
#endif /* EXCLUDE_DXPX10P */
#ifndef EXCLUDE_UDXPS
  "udxps",
#endif /* EXCLUDE_UDXPS */
#ifndef EXCLUDE_UDXP
  "udxp",
#endif /* EXCLUDE_UDXP */
#ifndef EXCLUDE_XMAP
  "xmap",
#endif /* EXCLUDE_XMAP */
#ifndef EXCLUDE_VEGA
  "vega",
#endif /* EXCLUDE_VEGA */
#ifndef EXCLUDE_MERCURY
  "mercury",
#endif /* EXCLUDE_MERCURY */
  };


/* These EXCLUDES must be kept in
 * sync with the EXCLUDES in
 * xia_module.h
 */
static char *INTERF_LIST[] = {
  "bad",
#ifndef EXCLUDE_CAMAC
  "CAMAC",
  "CAMAC",
#endif /* EXCLUDE_CAMAC */
#ifndef EXCLUDE_EPP
  "EPP",
  "EPP",
#endif /* EXCLUDE_EPP */
#ifndef EXCLUDE_SERIAL
  "SERIAL",
#endif /* EXCLUDE_SERIAL */
#ifndef EXCLUDE_USB
  "USB",
#endif /* EXCLUDE_USB */
#ifndef EXCLUDE_USB2
  "USB2",
#endif /* EXCLUDE_USB2 */
#ifndef EXCLUDE_PLX
  "PXI",
#endif /* EXCLUDE_PLX */
};

#define N_KNOWN_BOARDS  (sizeof(BOARD_LIST) / sizeof(BOARD_LIST[0]))

#define MAX_INTERF_LEN   24
#define MAX_MD_LEN       12 
#define MAX_NUM_CHAN_LEN  4
/* As far as the Xerxes configuration goes, this allows a detChan range of
 * 0 - 9999, which should be enough for anybody.
 */
#define MAX_CHAN_LEN      4


/** Prototypes **/




 
#endif /* HANDEL_XERXES_H */
