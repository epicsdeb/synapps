/*
 * xia_module.h
 *
 * Contains information specific to the module dynamic
 * configuration code.
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
 * $Id: xia_module.h,v 1.6 2009-07-16 17:01:47 rivers Exp $
 *
 *
 */


#ifndef XIA_MODULE_H
#define XIA_MODULE_H

#include "handeldef.h"
#include "xia_handel.h"
#include "xia_common.h"


/** Typedefs **/
typedef unsigned short Token;
typedef int (*ModItemFunc_t)(Module *, void *, char *);
typedef int (*ModInitFunc_t)(Module *);
typedef int (*AddChanTypeFunc_t)(Module *, int, void *);

/** Structs **/
typedef struct _ModItemFunc {
  char *name;
  ModItemFunc_t f;
  boolean_t needsBT;

} ModItem_t;


/* This structure represents a module_type
 * name and allows for multiple aliases to
 * the same name. This reduces the amount
 * of code that needs to be modified elsewhere
 * in Handel.
 */
typedef struct _ModName {
  char *alias;
  char *actual;

} ModName_t;


typedef struct _AddChanType {
  char *name;
  AddChanTypeFunc_t f;

} AddChanType_t;

/* Interface */
enum {
  NO_INTERFACE = 0,
#ifndef EXCLUDE_CAMAC
  JORWAY73A,
  GENERIC_SCSI,
#endif /* EXCLUDE_CAMAC */
#ifndef EXCLUDE_EPP
  EPP,
  GENERIC_EPP,
#endif /* EXCLUDE_EPP */
#ifndef EXCLUDE_SERIAL
  SERIAL,
#endif /* EXCLUDE_SERIAL */
#ifndef EXCLUDE_USB
  USB,
#endif /* EXCLUDE_USB */
#ifndef EXCLUDE_USB2
  USB2,
#endif /* EXCLUDE_USB2 */
#ifndef EXCLUDE_PLX
  PLX,
#endif /* EXCLUDE_PLX */
};


/* Module-Name Tokens */
enum {
  BAD_TOK = 0,
  MODTYP_TOK,
  INTERFACE_TOK,
  LIBRARY_TOK,
  NUMCHAN_TOK,
  CHANNEL_TOK,
  FIRMWARE_TOK,
  DEFAULT_TOK 
};


/* Channel index offset in strings like channel{n}_alias, etc */
#define CHANNEL_IDX_OFFSET  7


#endif /* XIA_MODULE_H */
