/*
 *  handeldef.h
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
 * $Id: handeldef.h,v 1.7 2009-07-06 18:24:30 rivers Exp $
 *
 */


#ifndef HANDELDEF_H
#define HANDELDEF_H


/* Have we defined the EXPORT and IMPORT macros? */
#ifndef HANDEL_PORTING_DEFINED
#define HANDEL_PORTING_DEFINED



#define HANDEL_STATIC static

/* Since we are using multiple source files now, we need to have a new class
 * of functions that aren't exported or static, just shared across different
 * files.
 */
#define HANDEL_SHARED


#define DXP_API
#define MD_API

#ifdef HANDEL_USE_DLL		/* Linking to a DLL libraries */

#ifdef WIN32			/* If we are on a Windoze platform */

#ifdef HANDEL_MAKE_DLL
#define HANDEL_EXPORT __declspec(dllexport)
#define HANDEL_IMPORT __declspec(dllimport)

#ifndef WIN32_HANDEL_VBA		/* Libraries for Visual Basic require STDCALL */
#define HANDEL_API
#else
#define HANDEL_API    _stdcall
#endif					/* Endif for WIN32_VBA */

#else					/* Then we are making a static link library */
#define HANDEL_EXPORT 	
#define HANDEL_IMPORT __declspec(dllimport)

#ifndef WIN32_HANDEL_VBA		/* Libraries for Visual Basic require STDCALL */
#define HANDEL_API
#else
#define HANDEL_API    _stdcall
#endif					/* Endif for WIN32_VBA */

#endif					/* Endif for HANDEL_MAKE_DLL */

#else					/* Not on a Windoze platform */

#ifdef HANDEL_MAKE_DLL
#define HANDEL_EXPORT 
#define HANDEL_IMPORT extern
#define HANDEL_API
#else					/* Then we are making a static link library */
#define HANDEL_EXPORT 	
#define HANDEL_IMPORT extern
#define HANDEL_API
#endif					/* Endif for HANDEL_MAKE_DLL */

#endif					/* Endif for WIN32 */

#else					/* We are using static libraries */

#ifdef WIN32			/* If we are on a Windoze platform */

#ifdef HANDEL_MAKE_DLL
#define HANDEL_EXPORT __declspec(dllexport)
#define HANDEL_IMPORT extern
#define HANDEL_API    
#else					/* Then we are making a static link library */
#define HANDEL_EXPORT 	
#define HANDEL_IMPORT extern
#define HANDEL_API    
#endif					/* Endif for HANDEL_MAKE_DLL */

#else					/* Not on a Windoze platform */

#ifdef HANDEL_MAKE_DLL
#define HANDEL_EXPORT 
#define HANDEL_IMPORT extern
#define HANDEL_API    
#else					/* Then we are making a static link library */
#define HANDEL_EXPORT 	
#define HANDEL_IMPORT extern
#define HANDEL_API
#endif					/* Endif for HANDEL_MAKE_DLL */

#endif					/* Endif for WIN32 */

#endif					/* Endif for HANDEL_USE_DLL */

#endif					/* Endif for HANDEL_DXP_DLL_DEFINED */

#ifndef _HANDEL_SWITCH_
#define _HANDEL_SWITCH_ 1

#ifdef __STDC__
#define _HANDEL_PROTO_  1
#endif                /* end of __STDC__    */

#ifdef _MSC_VER
#ifndef _HANDEL_PROTO_
#define _HANDEL_PROTO_  1
#endif
#endif                /* end of _MSC_VER    */

#ifdef _HANDEL_PROTO_
#define VOID void
#else
#define VOID
#endif               /* end of _HANDEL_PROTO_ */

#endif               /* end of _HANDEL_SWITCH_*/

#endif				 /* Endif for HANDELDEF_H */
