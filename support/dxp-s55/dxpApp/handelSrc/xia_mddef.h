/*
 *  xiadef.h
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
 * $Id: xia_mddef.h,v 1.3 2009-07-06 18:24:31 rivers Exp $
 *
 *
 *    Following are prototypes for dxp driver routines
 */


#ifndef XIA_MDDEF_H
#define XIA_MDDEF_H

/* Have we allready defined EXPORTS and IMPORTS? */
#ifndef XIA_MD_PORTING_DEFINED
#define XIA_MD_PORTING_DEFINED

#define XIA_MD_STATIC static

#define XIA_MD_SHARED


#ifdef XIA_MD_USE_DLL		/* Linking to a DLL libraries */

#ifdef WIN32			/* If we are on a Windoze platform */

#ifdef XIA_MD_MAKE_DLL
#define XIA_MD_EXPORT __declspec(dllexport)
#define XIA_MD_IMPORT __declspec(dllimport)
#define XIA_MD_API
#else					/* Then we are making a static link library */
#define XIA_MD_EXPORT 	
#define XIA_MD_IMPORT __declspec(dllimport)
#define XIA_MD_API
#endif					/* Endif for XIA_MD_MAKE_DLL */

#else					/* Not on a Windoze platform */

#ifdef XIA_MD_MAKE_DLL
#define XIA_MD_EXPORT 
#define XIA_MD_IMPORT extern
#define XIA_MD_API
#else					/* Then we are making a static link library */
#define XIA_MD_EXPORT 	
#define XIA_MD_IMPORT extern
#define XIA_MD_API
#endif					/* Endif for XIA_MD_MAKE_DLL */

#endif					/* Endif for WIN32 */

#else					/* We are using static libraries */

#ifdef WIN32			/* If we are on a Windoze platform */

#ifdef XIA_MD_MAKE_DLL
#define XIA_MD_EXPORT __declspec(dllexport)
#define XIA_MD_IMPORT extern
#define XIA_MD_API
#else					/* Then we are making a static link library */
#define XIA_MD_EXPORT 	
#define XIA_MD_IMPORT extern
#define XIA_MD_API
#endif					/* Endif for XIA_MD_MAKE_DLL */

#else					/* Not on a Windoze platform */

#ifdef XIA_MD_MAKE_DLL
#define XIA_MD_EXPORT 
#define XIA_MD_IMPORT extern
#define XIA_MD_API
#else					/* Then we are making a static link library */
#define XIA_MD_EXPORT 	
#define XIA_MD_IMPORT extern
#define XIA_MD_API
#endif					/* Endif for XIA_MD_MAKE_DLL */

#endif					/* Endif for WIN32 */

#endif					/* Endif for XIA_MD_USE_DLL */

#endif					/* Endif for XIA_MD_DXP_DLL_DEFINED */

#ifndef _DXP_SWITCH_
#define _DXP_SWITCH_ 1

#ifdef __STDC__
#define _DXP_PROTO_  1
#endif                /* end of __STDC__    */

#ifdef _MSC_VER
#ifndef _DXP_PROTO_
#define _DXP_PROTO_  1
#endif
#endif                /* end of _MSC_VER    */

#ifdef _DXP_PROTO_
#define VOID void
#else
#define VOID
#endif               /* end of _DXP_PROTO_ */

#endif               /* end of _DXP_SWITCH_*/

#endif				 /* Endif for XIA_MDDEF_H */
