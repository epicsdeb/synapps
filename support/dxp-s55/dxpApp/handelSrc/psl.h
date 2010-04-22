/*
 * psl.h
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
 * $Id: psl.h,v 1.4 2009-07-06 18:24:30 rivers Exp $
 *
 */


#ifndef PSL_H
#define PSL_H

#include "psldef.h"

#ifndef EXCLUDE_DXPX10P
PSL_IMPORT int PSL_API dxpx10p_PSLInit(PSLFuncs *funcs);
#endif /* EXCLUDE_DXPX10P */
#ifndef EXCLUDE_DXP4C2X
PSL_IMPORT int PSL_API dxp4c2x_PSLInit(PSLFuncs *funcs);
#endif /* EXCLUDE_DXP4C2X */
#ifndef EXCLUDE_UDXPS
PSL_IMPORT int PSL_API udxps_PSLInit(PSLFuncs *funcs);
#endif /* EXCLUDE_UDXPS */
#ifndef EXCLUDE_UDXP
PSL_IMPORT int PSL_API udxp_PSLInit(PSLFuncs *funcs);
#endif /* EXCLUDE_UDXP */
#ifndef EXCLUDE_XMAP
PSL_IMPORT int PSL_API xmap_PSLInit(PSLFuncs *funcs);
#endif /* EXCLUDE_XMAP */
#ifndef EXCLUDE_VEGA
PSL_IMPORT int PSL_API vega_PSLInit(PSLFuncs *funcs);
#endif /* EXCLUDE_VEGA */
#ifndef EXCLUDE_MERCURY
PSL_IMPORT int PSL_API mercury_PSLInit(PSLFuncs *funcs);
#endif /* EXCLUDE_MERCURY */

#endif /* PSL_H */
