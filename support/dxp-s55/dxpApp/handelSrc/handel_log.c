/*
 * handel_log.c
 *
 * Routines used for controlling the logging functionality
 * in XerXes.
 *
 * Created 8/22/01 -- PJF
 *
 * Major upgrade 12/3/01 -- JEW
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


#include <stdio.h>

#include "handel_generic.h"
#include "handeldef.h"
#include "xia_handel.h"
#include "xerxes_errors.h"
#include "handel_errors.h"


/*****************************************************************************
 *
 * This routine enables the logging output
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaEnableLogOutput(void)
{
	int status;

	if (handel_md_enable_log == NULL) 
	{ 
		xiaInitHandel();
	}

	status = handel_md_enable_log();

	if (status != DXP_SUCCESS)
	{
		return XIA_MD;
	}

	return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine disables the logging output
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaSuppressLogOutput(void)
{
	int status;

	if (handel_md_suppress_log == NULL) 
	{ 
		xiaInitHandel();
	}

	status = handel_md_suppress_log();

	if (status != DXP_SUCCESS)
	{
		return XIA_MD;
	}

	return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets the maximum level at which log messages will be 
 * displayed.
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaSetLogLevel(int level)
/* int level;							Input: Level to set the logging to   */
{
	int status;

	if (handel_md_set_log_level == NULL) 
	{
		xiaInitHandel();
	}

	status = handel_md_set_log_level(level);
	
	if (status != DXP_SUCCESS)
	{
		return XIA_MD;
	}

	return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets the output stream for the logging routines. By default,
 * the output is sent to stdout.
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaSetLogOutput(char *filename)
/* char *filename;					Input: name of file to redirect reporting */
{
  if (handel_md_output == NULL) 
	{
	  xiaInitHandel();
	}
  
  handel_md_output(filename);
  
  return XIA_SUCCESS;
}	
