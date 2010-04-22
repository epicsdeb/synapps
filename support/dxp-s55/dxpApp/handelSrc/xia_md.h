/*
 *  xia_md.h
 *
 * Copyright (c) 2002,2003,2004 X-ray Instrumentation Associates
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
 * $Id: xia_md.h,v 1.3 2009-07-06 18:24:31 rivers Exp $
 *
 * Prototypes for MD layer
 *
 */

#ifndef XIA_MD_H
#define XIA_MD_H

#include "xerxes_structures.h"

#include "xia_mddef.h"
#include "xia_common.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef _DXP_PROTO_ /* ANSI C Prototypes go here */
XIA_MD_EXPORT int XIA_MD_API dxp_md_init_util(Xia_Util_Functions *funcs, char *type);
XIA_MD_EXPORT int XIA_MD_API dxp_md_init_io(Xia_Io_Functions *funcs, char *type);
XIA_MD_SHARED void XIA_MD_API dxp_md_error(char *,char *, int *, char *file, int line);

/* Enhanced logging routines added 8/21/01 -- PJF */
XIA_MD_SHARED void XIA_MD_API dxp_md_warning(char *routine, char *message, char *file, int line);
XIA_MD_SHARED void XIA_MD_API dxp_md_info(char *routine, char *message, char *file, int line);
XIA_MD_SHARED void XIA_MD_API dxp_md_debug(char *routine, char *message, char *file, int line);
XIA_MD_SHARED void XIA_MD_API dxp_md_output(char *filename);
XIA_MD_SHARED int XIA_MD_API dxp_md_enable_log(void);
XIA_MD_SHARED int XIA_MD_API dxp_md_suppress_log(void);
XIA_MD_SHARED int XIA_MD_API dxp_md_set_log_level(int level);
XIA_MD_SHARED void XIA_MD_API dxp_md_log(int level, char *routine, char *message, int error, 
										 char *file, int line);

XIA_MD_SHARED void XIA_MD_API *dxp_md_alloc(size_t);
XIA_MD_SHARED void XIA_MD_API dxp_md_free(void *);


#else /* _DXP_PROTO_ */

XIA_MD_EXPORT int XIA_MD_API dxp_md_init_util();
XIA_MD_EXPORT int XIA_MD_API dxp_md_init_io();

XIA_MD_SHARED void XIA_MD_API dxp_md_error();
XIA_MD_SHARED void XIA_MD_API dxp_md_warning();
XIA_MD_SHARED void XIA_MD_API dxp_md_info();
XIA_MD_SHARED void XIA_MD_API dxp_md_debug();
XIA_MD_SHARED void XIA_MD_API dxp_md_output();
XIA_MD_SHARED int XIA_MD_API dxp_md_enable_log();
XIA_MD_SHARED int XIA_MD_API dxp_md_suppress_log();
XIA_MD_SHARED int XIA_MD_API dxp_md_set_log_level();
XIA_MD_SHARED void XIA_MD_API dxp_md_log();


#endif /* _DXP_PROTO_ */

#ifdef __cplusplus
}
#endif /* __cplusplus */

/* Current output for the logging routines. By default, this is set to stdout */
extern FILE *out_stream;

/** Logging macros **/
#define dxp_md_log_error(x, y, z)	dxp_md_log(MD_ERROR,   (x), (y), (z), __FILE__, __LINE__)
#define dxp_md_log_warning(x, y)	dxp_md_log(MD_WARNING, (x), (y), 0,   __FILE__, __LINE__)
#define dxp_md_log_info(x, y)		dxp_md_log(MD_INFO,    (x), (y), 0,   __FILE__, __LINE__)
#define dxp_md_log_debug(x, y)		dxp_md_log(MD_DEBUG,   (x), (y), 0,   __FILE__, __LINE__)

DXP_MD_ALLOC md_md_alloc;
DXP_MD_FREE  md_md_free;

/* Memory allocation macro wrappers */
#ifdef USE_XIA_MEM_MANAGER
#include "xia_mem.h"
#define md_md_alloc(n)  xia_mem_malloc((n), __FILE__, __LINE__)
#define md_md_free(ptr) xia_mem_free(ptr)
#endif /* USE_XIA_MEM_MANAGER */


#endif /* XIA_MD_H */
