/* Copyright (c) 2004, X-ray Instrumentation Associates
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
 * $Id: camac_errors.h,v 1.1 2009-07-06 18:24:28 rivers Exp $
 *
 */

#ifndef _CAMAC_ERRORS_H_
#define _CAMAC_ERRORS_H_

#define XIA_CAMAC_SUCCESS         0
#define XIA_CAMAC_CDCHN           1 /** Call to cdchn() failed; Linux only */
#define XIA_CAMAC_CDREG           2 /** Call to cdreg() failed; Linux only */
#define XIA_CAMAC_CCCTYPE         3 /** Call to ccctype() failed: Linux only */
#define XIA_CAMAC_CCCBYP          4 /** Call to cccbyp() failed: Linux only */
#define XIA_CAMAC_CCCOFF          5 /** Call to cccoff() failed: Linux only */
#define XIA_CAMAC_CCCI            6 /** Call to ccci() failed: Linux only */
#define XIA_CAMAC_CCCC            7 /** Call to cccc() failed: Linux only */
#define XIA_CAMAC_CCCZ            8 /** Call to cccz() failed: Linux only */
#define XIA_CAMAC_CSSA            9 /** Call to cssa() failed: Linux only */
#define XIA_CAMAC_CSUBC          10 /** Call to csubc() failed: Linux only */
#define XIA_CAMAC_INCOMPLETE_BLK 11 /** Block transfer failed */
#define XIA_CAMAC_BUS            12 /** Specified SCSI Bus is out-of-range */
#define XIA_CAMAC_CRATE          13 /** Specified Crate ID is out-of-range */
#define XIA_CAMAC_SLOT           14 /** Specified slot # is out-of-range */
#define XIA_CAMAC_LEN            15 /** Specified transfer length is invalid */

#endif /* _CAMAC_ERRORS_H_ */
