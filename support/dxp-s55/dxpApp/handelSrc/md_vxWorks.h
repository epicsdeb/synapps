/*
 *  md_vxWorks.h
 *
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
 * Converted from md_win95.h by Mark Rivers, University of Chicago
 */


#ifndef DXP_MD_H
#define DXP_MD_H


#include "xia_mddef.h"
#include "xia_common.h"


/* Some constants */
#define MAXMOD 100

/* If this is compiled by a C++ compiler, make it clear that these are C routines */
#ifdef __cplusplus
extern "C" {
#endif

/* NOTE: md_vxWorks.c currently only supports the DXP4C2X on CAMAC.  
 * If other support is added then take the prototypes and starting code from md_win95.c
 */
 
#if !(defined(EXCLUDE_DXP4C2X))
XIA_MD_STATIC int XIA_MD_API dxp_md_initialize(unsigned int *, char *);
XIA_MD_STATIC int XIA_MD_API dxp_md_open(char *, int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_open_bcn(int *, int *, int *, int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_io(int *,unsigned int *,unsigned long *,void  *,unsigned int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_close(int *camChan);
XIA_MD_STATIC void XIA_MD_API dxp_md_vx_log(int level, char *routine, char *message, int error, char *file, int line);
XIA_MD_STATIC void dxp_md_vx_output(char *filename);
#endif /* EXCLUDE DXP4C2X */

XIA_MD_STATIC int XIA_MD_API dxp_md_lock_resource(int *ioChan, int *modChan, short *lock);
XIA_MD_STATIC int XIA_MD_API dxp_md_wait(float *);
XIA_MD_STATIC void XIA_MD_API dxp_md_error_control(char *,int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_get_maxblk(void);
XIA_MD_STATIC int XIA_MD_API dxp_md_set_maxblk(unsigned int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_puts(char *);
XIA_MD_STATIC int XIA_MD_API dxp_md_set_priority(int *priority);
XIA_MD_STATIC char * dxp_md_fgets(char *s, int length, FILE *stream);
XIA_MD_STATIC char * dxp_md_tmp_path(void);
XIA_MD_STATIC void dxp_md_clear_tmp(void);
XIA_MD_STATIC char * dxp_md_path_separator(void);
XIA_MD_IMPORT int XIA_MD_API xia_camxfr(short *camadr, short func, long nbytes, short mode, short *buf);
XIA_MD_IMPORT int XIA_MD_API xia_caminit(short *buf);

#ifdef __cplusplus
}
#endif


#endif					/* End if for DXP_MD_H */
