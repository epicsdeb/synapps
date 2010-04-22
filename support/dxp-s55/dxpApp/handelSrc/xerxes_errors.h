/*
 *  xerxes_errors.h
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
 * $Id: xerxes_errors.h,v 1.3 2009-07-06 18:24:31 rivers Exp $
 *
 */


#ifndef XERXES_ERROR_H
#define XERXES_ERROR_H

#include "xerxesdef.h"

/*
 *  some error codes
 */
#define DXP_SUCCESS            0

/* I/O level error codes 1-100*/
#define DXP_MDOPEN             1
#define DXP_MDIO               2
#define DXP_MDINITIALIZE       3
#define DXP_MDLOCK			   4
#define DXP_MDFILEIO		   5
#define DXP_MDTIMEOUT          6
#define DXP_MDSIZE             7
#define DXP_MDOVERFLOW         8
#define DXP_MDUNKNOWN          9
#define DXP_MDCLOSE           10
#define DXP_MDNOHANDLE        11
#define DXP_MDINVALIDPRIORITY 12
#define DXP_MDPRIORITY        13
#define DXP_MDINVALIDNAME     14
#define DXP_MDNOMEM           15
#define DXP_RW_MISMATCH       16
#define DXP_REWRITE_FAILURE   17 /** Couldn't set parameter even after n iterations. */
#define DXP_MD_TARGET_ADDR    18

/*  primitive level error codes (due to mdio failures) 101-200*/
#define DXP_WRITE_TSAR       101
#define DXP_WRITE_CSR        102
#define DXP_WRITE_WORD       103
#define DXP_READ_WORD        104
#define DXP_WRITE_BLOCK      105
#define DXP_READ_BLOCK       106
#define DXP_DISABLE_LAM      107

/* changed from DXP_CLEAR_LAM to DXP_CLR_LAM due to a conflict with an existing
 * function pointer
 */
#define DXP_CLR_LAM          108
#define DXP_TEST_LAM         109
#define DXP_READ_CSR         110
#define DXP_WRITE_FIPPI      111
#define DXP_WRITE_DSP        112
#define DXP_WRITE_DATA       113
#define DXP_READ_DATA        114
#define DXP_ENABLE_LAM       115
#define DXP_READ_GSR         116
#define DXP_WRITE_GCR        117
#define DXP_WRITE_WCR        118
#define DXP_READ_WCR         119
#define DXP_WRITE_MMU        120
#define DXP_CHECKSUM         121
#define DXP_BAD_ADDRESS      122
#define DXP_BAD_BIT          123 /** Requested bit is out-of-range */

/*  DSP/FIPPI level error codes 201-300  */
#define DXP_MEMERROR         201
#define DXP_DSPRUNERROR      202
#define DXP_FPGADOWNLOAD     203
#define DXP_DSPDOWNLOAD      204
#define DXP_INTERNAL_GAIN    205
#define DXP_RESET_WARNING    206
#define DXP_DETECTOR_GAIN    207
#define DXP_NOSYMBOL         208
#define DXP_SPECTRUM         209
#define DXP_DSPLOAD          210
#define DXP_DSPPARAMS        211
#define DXP_DSPACCESS        212
#define DXP_DSPPARAMBOUNDS   213
#define DXP_ADC_RUNACTIVE    214
#define DXP_ADC_READ		 215
#define DXP_ADC_TIMEOUT      216
#define DXP_ALLOCMEM         217
#define DXP_NOCONTROLTYPE    218
#define DXP_NOFIPPI			 219
#define DXP_DSPSLEEP		 220
#define DXP_TIMEOUT          221
#define DXP_DSP_RETRY        222 /** DSP failed to download after multiple attempts */


/*  configuration errors  301-400  */
#define DXP_BAD_PARAM        301
#define DXP_NODECIMATION     302
#define DXP_OPEN_FILE        303
#define DXP_NORUNTASKS       304
#define DXP_OUTPUT_UNDEFINED 305
#define DXP_INPUT_UNDEFINED  306
#define DXP_ARRAY_TOO_SMALL  307
#define DXP_NOCHANNELS       308
#define DXP_NODETCHAN        309
#define DXP_NOIOCHAN         310
#define DXP_NOMODCHAN        311
#define DXP_NEGBLOCKSIZE     312
#define DXP_FILENOTFOUND     313
#define DXP_NOFILETABLE		 314
#define DXP_INITIALIZE	     315
#define DXP_UNKNOWN_BTYPE    316
#define DXP_NOMATCH          317
#define DXP_BADCHANNEL       318
#define DXP_DSPTIMEOUT       319
#define DXP_INITORDER        320
#define DXP_UDXPS            321
#define DXP_NOSUPPORT        322
#define DXP_WRONG_FIRMWARE   323
#define DXP_UDXP             324
#define DXP_NULL             325
#define DXP_BUF_LEN          326
#define DXP_UNKNOWN_MEM      327
#define DXP_UNKNOWN_ELEM     328
#define DXP_UNKNOWN_FPGA     329
#define DXP_FPGA_TIMEOUT     330
#define DXP_MALFORMED_FILE   331
#define DXP_UNKNOWN_CT       332 /** Unknown control task */
#define DXP_APPLY_STATUS     333
#define DXP_INVALID_LENGTH   334 /** Specified length is invalid */
#define DXP_NO_SCA           335 /** No SCAs defined for the specified channel */
#define DXP_FPGA_CRC         336 /** CRC error after FPGA downloaded */
#define DXP_UNKNOWN_REG      337 /** Unknown register */

/*  host machine errors codes:  401-500 */
#define DXP_NOMEM            401
#define DXP_CLOSE_FILE       403
#define DXP_INDEXOOB         404
#define DXP_RUNACTIVE		 405
#define DXP_MEMINUSE         406
#define DXP_INVALID_STRING   407
#define DXP_UNIMPLEMENTED    408
#define DXP_MISSING_ESC      409
#define DXP_MEMORY_OOR       410
#define DXP_MEMORY_LENGTH    411
#define DXP_MEMORY_BLK_SIZE  412
#define DXP_WIN32_API        413

/*  misc error codes:  501-600 */
#define DXP_LOG_LEVEL		 501	/** Log level invalid */

#endif						/* Endif for XERXES_ERRORS_H */
