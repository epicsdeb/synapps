/*
 * handel_file.h
 *
 * This file contains the appropriate header information for the
 * the source in handel_file.c
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
 * $Id: handel_file.h,v 1.2 2009-07-06 18:24:29 rivers Exp $
 *
 *
 */


#ifndef HANDEL_FILE_H
#define HANDEL_FILE_H

/** Includes **/
#include <stdio.h>

/** Structures **/

/* This structure exists so that we can re-use the
 * section of the code that parses in the sections
 * of the ini files
 */
typedef struct 
{
    /* Pointer to the proper xiaLoadRoutine */
    int (*function_ptr)(FILE *, fpos_t *, fpos_t *);

    /* Section heading name: the part in brackets */
    char *section;
	
} SectionInfo; 


/** Prototypes **/
HANDEL_STATIC int HANDEL_API xiaWriteIniFile(char *filename);
HANDEL_STATIC int HANDEL_API xiaWriteSavFile(char *filename);

HANDEL_STATIC int HANDEL_API xiaFindEntryLimits(FILE *fp, const char *section, 
						fpos_t *start, fpos_t *end);
HANDEL_STATIC int HANDEL_API xiaGetLine(FILE *fp, char *line);
HANDEL_STATIC int HANDEL_API xiaGetLineData(char *line, char *name, char *value);
HANDEL_STATIC int HANDEL_API xiaFileRA(FILE *fp, fpos_t *start, fpos_t *end, char *name, 
				       char *value);
HANDEL_STATIC int HANDEL_API xiaSetPosOnNext(FILE *fp, fpos_t *start, fpos_t *end, 
					     char *name, fpos_t *newPos, boolean_t after);

HANDEL_STATIC int xiaLoadDetector(FILE *fp, fpos_t *start, fpos_t *end);
HANDEL_STATIC int xiaLoadModule(FILE *fp, fpos_t *start, fpos_t *end);
HANDEL_STATIC int xiaLoadFirmware(FILE *fp, fpos_t *start, fpos_t *end);
HANDEL_STATIC int xiaLoadDefaults(FILE *fp, fpos_t *start, fpos_t *end);

HANDEL_STATIC int HANDEL_API xiaReadPTRRs(FILE *fp, fpos_t *start, fpos_t *end, char *alias);


#endif /* HANDEL_FILE_H */
