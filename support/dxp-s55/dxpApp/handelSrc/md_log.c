/*
 * md_log.c
 *
 * Routines used for controlling the logging functionality
 * in XerXes.
 *
 * Created 12/3/01 -- JEW
 *
 * Copyright (c) 2002,2003,2004, X-ray Instrumentation Associates
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
 */


#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

#include "xerxes_errors.h"
#include "xerxes_structures.h"
#include "md_generic.h"
#include "xia_md.h"
#include "xia_common.h"

#define INFO_LEN	400

/* Current output for the logging routines. By default, this is set to stdout
 * in dxp_md_log().
 */
FILE *out_stream = NULL;

/* Static variables */
static boolean_t isSuppressed = FALSE_;
static int	   logLevel     = MD_ERROR;

/*****************************************************************************
 *
 * This routine enables the logging output
 *
 *****************************************************************************/
XIA_MD_SHARED int XIA_MD_API dxp_md_enable_log(void)
{
	isSuppressed = FALSE_;

	return DXP_SUCCESS;
}

/*****************************************************************************
 *
 * This routine disables the logging output
 *
 *****************************************************************************/
XIA_MD_SHARED int XIA_MD_API dxp_md_suppress_log(void)
{
	isSuppressed = TRUE_;

	return DXP_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets the maximum level at which log messages will be 
 * displayed.
 *
 *****************************************************************************/
XIA_MD_SHARED int XIA_MD_API dxp_md_set_log_level(int level)
/* int level;							Input: Level to set the logging to   */
{

/* Validate level */
	if ((level > MD_DEBUG) || (level < MD_ERROR)) {
/* Leave level at previous setting and return an error code */
		return DXP_LOG_LEVEL;
	}

	logLevel = level;

	return DXP_SUCCESS;
}

/*****************************************************************************
 *
 * This routine is the main logging routine. It shouldn't be called directly.
 * Use the macros provided in xerxes_generic.h.
 *
 *****************************************************************************/
XIA_MD_SHARED void XIA_MD_API dxp_md_log(int level, char *routine, char *message, int error, char *file, int line)
/* int level;							Input: The level of this log message */
/* char *routine;						Input: Name of routine calling dxp_log*/
/* char *message;						Input: Log message to send to output */
/* int error;							Input: Only used if this is an ERROR */
{

  /* Ordinarily, we'd set this in the globals section,
   * but on Linux 'stdout' isn't a constant, so it
   * can't be used as an initializer.
   */
  if (out_stream == NULL) {
    out_stream = stdout;
  }

/* If logging is disabled or we aren't set
 * to log this message level then return gracefully, NOW! 
 */
	if (isSuppressed || (level > logLevel)) {
		return;
	}

/* Validate level */
	if ((level > MD_DEBUG) || (level < MD_ERROR)) {
/* Don't log the message */
		return;
	}

	switch (level) {
	case MD_ERROR:
		dxp_md_error(routine, message, &error, file, line);
		break;
	case MD_WARNING:
		dxp_md_warning(routine, message, file, line);
		break;
	case MD_INFO:
		dxp_md_info(routine, message, file, line);
		break;
	case MD_DEBUG:
		dxp_md_debug(routine, message, file, line);
		break;
	default:
/* If the code gets here then something is really jacked-up */
		break;
	}
}
		
/*****************************************************************************
 * 
 * Routine to handle error reporting.  Allows the user to pass errors to log
 * files or report the information in whatever fashion is desired.
 * 
 *****************************************************************************/
XIA_MD_SHARED void dxp_md_error(char* routine, char* message, int* error_code, char *file, int line)
/* char *routine;					Input: Name of the calling routine		*/
/* char *message;					Input: Message to report					*/
/* int *error_code;					Input: Error code denoting type of error	*/
{

/* If the error_code is larger than DXP_DEBUG, then print the error only 
 * if debugging is requested */

/* I'm going to disable this for now and see how things work out with the new
 * logging routines. We can re-enable it if we don't like the new method.
 * 8/21/01 -- PJF
 */

/*    if(*error_code>=DXP_DEBUG){
        if(print_debug!=0) printf("%s : %s\n",routine,message);
        return;
     } */

/* Else print the error */

/*    printf("%s:[%d] %s\n",routine,*error_code,message); */

	time_t current = time(NULL);
	struct tm *localTime = localtime(&current);
	char logTimeFormat[50];

	strftime(logTimeFormat, 50, "%c", localTime);

/*	printf("%s [ERROR] [%d] %s: %s\n", logTimeFormat, *error_code, routine, message); */
	fprintf(out_stream, "[ERROR] [%d] %s %s, line = %d, %s: %s\n", *error_code, logTimeFormat, file, line, routine, message);
	fflush(out_stream);
}


/*****************************************************************************
 *
 * Routine to handle reporting warnings. Messages are written to the output
 * defined in out_stream.
 *
 *****************************************************************************/
XIA_MD_SHARED void dxp_md_warning(char *routine, char *message, char *file, int line)
/* char *routine;					Input: Name of the calling routine		 */
/* char *message;					Input: Message to report				 */
{
	time_t current = time(NULL);
	struct tm *localTime = localtime(&current);
	char logTimeFormat[50];

	strftime(logTimeFormat, 50, "%c", localTime);

	fprintf(out_stream, "[WARN] %s %s, line = %d, %s: %s\n", logTimeFormat, file, line, routine, message);
	fflush(out_stream);
}

/*****************************************************************************
 *
 * Routine to handle reporting info messages. Messages are written to the 
 * output defined in out_stream.
 *
 *****************************************************************************/
XIA_MD_SHARED void dxp_md_info(char *routine, char *message, char *file, int line)
/* char *routine;					Input: Name of the calling routine		 */
/* char *message;					Input: Message to report				 */
{
	/* No time displayed for info messages */

	fprintf(out_stream, "[INFO] %s, line = %d, %s: %s\n", file, line, routine, message);
	fflush(out_stream);
}

/*****************************************************************************
 *
 * Routine to handle reporting debug messages. Messages are written to the 
 * output defined in out_stream.
 *
 *****************************************************************************/
XIA_MD_SHARED void dxp_md_debug(char *routine, char *message, char *file, int line)
/* char *routine;					Input: Name of the calling routine		 */
/* char *message;					Input: Message to report				 */
{
	/* No time displayed for debug messages */
	fprintf(out_stream, "[DEBUG] %s, line = %d, %s: %s\n", file, line, routine, message);
	fflush(out_stream);
}

/*****************************************************************************
 *
 * Routine to set the logging output to whatever FILE * the user would like.
 * By default, the output stream is set to stdout.
 *
 *****************************************************************************/
XIA_MD_SHARED void dxp_md_output(char *filename)
/* char *filename;		Input: Name of the stream or file to redirect error output */
{
	int status;
	char *strtmp = NULL;
	unsigned int i;
	char info_string[INFO_LEN];

/* First close the currently opened stream, iff it is a file */
	if ((out_stream!=stdout) && (out_stream!=stderr)) {
/* close the stream */
		fclose(out_stream);
	}
/* change the input name to all lower case to check for predefined streams */
	strtmp = (char *) dxp_md_alloc((strlen(filename)+1) * sizeof(char));
	for (i=0;i<strlen(filename);i++) strtmp[i] = (char) tolower(filename[i]);
	strtmp[strlen(filename)]='\0';

/* if filename is stdin, then default to stdout */
	if (STREQ(strtmp,"stdin")) {
		dxp_md_log_warning("dxp_md_output", "Output filename can't be stdin; reset to stdout.");
		out_stream = stdout;
		dxp_md_free(strtmp);
		return;
	}

/* Check if the filename is stdout, NULL or stderr */
	if ((STREQ(strtmp,"stdout")) || (filename==NULL) || (STREQ(filename, ""))) {
		out_stream = stdout;
		dxp_md_free(strtmp);
		return;
	}
	if (STREQ(strtmp,"stderr")) {
		out_stream = stderr;
		dxp_md_free(strtmp);
		return;
	}
/* The filename must be for a "real" file */
	out_stream = fopen(filename,"w");

	if (out_stream==NULL) {
		status = DXP_MDFILEIO;
		sprintf(info_string,"Unable to open filename: %s, no action performed",filename);
		dxp_md_log_error("dxp_md_output",info_string,status);
		dxp_md_free(strtmp);
		return;
	}
	dxp_md_free(strtmp);
	return;

}
