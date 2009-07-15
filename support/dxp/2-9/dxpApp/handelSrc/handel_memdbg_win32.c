
/*
 * handel_memdbg_win32.c
 *
 * Essentially a wrapper around the Visual Studio dbg routines.
 *
 * Created 10/08/01 -- PJF
 *
 * Copyright (c) 2002, X-ray Instrumentation Associates
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


#ifdef __MEM_DBG__

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <time.h>

#include <crtdbg.h>

#pragma warning(disable : 4115)
#include <windows.h>

#include "xia_handel.h"

#pragma warning(disable : 4127)

static HANDLE memLog;
static _CrtMemState before;
static _CrtMemState after;
static _CrtMemState difference;

HANDEL_EXPORT void xiaSetReportMode(void)
{
  time_t my_time;
  struct tm *tstr = NULL;

  char fname[100];


  time(&my_time);
  tstr = localtime(&my_time);

  sprintf(fname, "memory_%02d%02d%02d.log", tstr->tm_year - 100, tstr->tm_mon + 1, tstr->tm_mday); 

	memLog = CreateFile(fname, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
		                FILE_ATTRIBUTE_NORMAL, NULL);

	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_WARN, memLog);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ERROR, memLog);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT, memLog);

	return;
}


HANDEL_EXPORT void xiaMemCheckpoint(int pass)
{
	switch (pass) {

	case XIA_BEFORE:
		_CrtMemCheckpoint(&before);
		break;

	case XIA_AFTER:
		_CrtMemCheckpoint(&after);
		_RPT0(_CRT_WARN, "****Dumping Memory Changes Between Checkpoints****\n");
		if (!_CrtMemDifference(&difference, &before, &after)) {
			_CrtMemDumpStatistics(&difference);
		}
		break;
	}

	return;
}


HANDEL_EXPORT void xiaReport(char *message)
{
	_RPT0(_CRT_WARN, message);

	return;
}


HANDEL_EXPORT void xiaMemDumpAllObjectsSince(void)
{
	_CrtMemDumpAllObjectsSince(&before);

	return;
}

HANDEL_EXPORT int xiaDumpMemoryLeaks(void)
{
  int status;

  status = _CrtDumpMemoryLeaks();

  return status;
}

HANDEL_EXPORT void xiaEndMemDbg(void)
{
	CloseHandle(memLog);

	return;
}


HANDEL_EXPORT int xiaCheckMemory(void)
{
  return _CrtCheckMemory();
}

#endif /* __MEM_DBG__ */
