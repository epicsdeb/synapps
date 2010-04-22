/*
 * psldef.h
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
 * $Id: psldef.h,v 1.3 2009-07-06 18:24:30 rivers Exp $
 *
 */


#ifndef PSLDEF_H
#define PSLDEF_H


#define PSL_STATIC	static

#define PSL_SHARED 

#ifdef PSL_USE_DLL		/* Linking to a DLL libraries */

#ifdef WIN32			/* If we are on a Windoze platform */

#ifdef  PSL_MAKE_DLL
#define PSL_EXPORT __declspec(dllexport)
#define PSL_IMPORT __declspec(dllimport)

#ifndef WIN32_PSL_VBA		/* Libraries for Visual Basic require STDCALL */
#define PSL_API
#else
#define PSL_API    _stdcall
#endif					/* Endif for WIN32_VBA */

#else					/* Then we are making a static link library */
#define PSL_EXPORT 	
#define PSL_IMPORT __declspec(dllimport)

#ifndef WIN32_PSL_VBA		/* Libraries for Visual Basic require STDCALL */
#define PSL_API
#else
#define PSL_API    _stdcall
#endif					/* Endif for WIN32_VBA */

#endif					/* Endif for PSL_MAKE_DLL */

#else					/* Not on a Windoze platform */

#ifdef  PSL_MAKE_DLL
#define PSL_EXPORT 
#define PSL_IMPORT extern
#define PSL_API
#else					/* Then we are making a static link library */
#define PSL_EXPORT 	
#define PSL_IMPORT extern
#define PSL_API
#endif					/* Endif for PSL_MAKE_DLL */

#endif					/* Endif for WIN32 */

#else					/* We are using static libraries */

#ifdef WIN32			/* If we are on a Windoze platform */

#ifdef  PSL_MAKE_DLL
#define PSL_EXPORT __declspec(dllexport)
#define PSL_IMPORT extern
#define PSL_API    
#else					/* Then we are making a static link library */
#define PSL_EXPORT 	
#define PSL_IMPORT extern
#define PSL_API    
#endif					/* Endif for PSL_MAKE_DLL */

#else					/* Not on a Windoze platform */

#ifdef  PSL_MAKE_DLL
#define PSL_EXPORT 
#define PSL_IMPORT extern
#define PSL_API    
#else					/* Then we are making a static link library */
#define PSL_EXPORT 	
#define PSL_IMPORT extern
#define PSL_API
#endif					/* Endif for PSL_MAKE_DLL */

#endif					/* Endif for WIN32 */

#endif					/* Endif for PSL_USE_DLL */


#endif /* PSLDEF_H */
