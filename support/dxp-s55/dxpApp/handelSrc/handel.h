/*
 *  handel.h
 *
 *  Modified 2-Feb-97 EO: add prototype for dxp_primitive routines
 *      dxp_read_long and dxp_write_long; added various parameters
 *  Major Mods 3-17-00 JW: Complete revamping of libraries
 *  Copied 6-25-01 JW: copied xia_xerxes.h to xia_handel.h
 *  Major Mods 8-21-01: renamed to handel.h and added more prototypes
 *                      from specification.
 *
 *
 * Copyright (c) 2002,2003,2004 X-ray Instrumentation Associates
 *               2005 XIA LLC
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
 *  Following are prototypes for HanDeL library routines
 *
 * $Id: handel.h,v 1.8 2009-07-06 18:24:29 rivers Exp $
 *
 */

/** @file handel.h */

#ifndef HANDEL_H
#define HANDEL_H

#include "xia_common.h"

#include "handeldef.h"


#define XIA_RUN_HARDWARE                0x01
#define XIA_RUN_HANDEL                  0x02
#define XIA_RUN_CT                      0x04

#define XIA_BEFORE		0
#define XIA_AFTER		1

/* Acquisition value membership constants */
#define AV_MEM_PARSET 0x04
#define AV_MEM_GENSET 0x08


/* If this is compiled by a C++ compiler, make it clear that these are C routines */
#ifdef __cplusplus
extern "C" {
#endif

#ifdef _HANDEL_PROTO_
#include <stdio.h>

/*
 * following are internal prototypes for HANDEL.c routines
 */



HANDEL_IMPORT int HANDEL_API xiaInit(char *iniFile);
HANDEL_IMPORT int HANDEL_API xiaInitHandel(void);
HANDEL_IMPORT int HANDEL_API xiaNewDetector(char *alias);
HANDEL_IMPORT int HANDEL_API xiaAddDetectorItem(char *alias, char *name, void *value);
HANDEL_IMPORT int HANDEL_API xiaModifyDetectorItem(char *alias, char *name, void *value);
HANDEL_IMPORT int HANDEL_API xiaGetDetectorItem(char *alias, char *name, void *value);
HANDEL_IMPORT int HANDEL_API xiaGetNumDetectors(unsigned int *numDet);
HANDEL_IMPORT int HANDEL_API xiaGetDetectors(char *detectors[]);
HANDEL_IMPORT int HANDEL_API xiaGetDetectors_VB(unsigned int index, char *alias);
HANDEL_IMPORT int HANDEL_API xiaRemoveDetector(char *alias);
  HANDEL_IMPORT int HANDEL_API xiaDetectorFromDetChan(int detChan, char *alias);
HANDEL_IMPORT int HANDEL_API xiaNewFirmware(char *alias);
HANDEL_IMPORT int HANDEL_API xiaAddFirmwareItem(char *alias, char *name, void *value);
HANDEL_IMPORT int HANDEL_API xiaModifyFirmwareItem(char *alias, unsigned short decimation, 
						   char *name, void *value);
HANDEL_IMPORT int HANDEL_API xiaGetFirmwareItem(char *alias, unsigned short decimation, 
						char *name, void *value);
HANDEL_IMPORT int HANDEL_API xiaGetNumFirmwareSets(unsigned int *numFirmware);
HANDEL_IMPORT int HANDEL_API xiaGetFirmwareSets(char *firmware[]);
HANDEL_IMPORT int HANDEL_API xiaGetFirmwareSets_VB(unsigned int index, char *alias);
HANDEL_IMPORT int HANDEL_API xiaGetNumPTRRs(char *alias, unsigned int *numPTRR);
HANDEL_IMPORT int HANDEL_API xiaRemoveFirmware(char *alias);
HANDEL_IMPORT int HANDEL_API xiaNewModule(char *alias);
HANDEL_IMPORT int HANDEL_API xiaAddModuleItem(char *alias, char *name, void *value);
HANDEL_IMPORT int HANDEL_API xiaModifyModuleItem(char *alias, char *name, void *value);
HANDEL_IMPORT int HANDEL_API xiaGetModuleItem(char *alias, char *name, void *value);
HANDEL_IMPORT int HANDEL_API xiaGetNumModules(unsigned int *numModules);
HANDEL_IMPORT int HANDEL_API xiaGetModules(char *modules[]);
HANDEL_IMPORT int HANDEL_API xiaGetModules_VB(unsigned int index, char *alias);
HANDEL_IMPORT int HANDEL_API xiaRemoveModule(char *alias);
  HANDEL_IMPORT int HANDEL_API xiaModuleFromDetChan(int detChan, char *alias);
HANDEL_IMPORT int HANDEL_API xiaAddChannelSetElem(unsigned int detChanSet, unsigned int newChan);
HANDEL_IMPORT int HANDEL_API xiaRemoveChannelSetElem(unsigned int detChan, unsigned int chan);
HANDEL_IMPORT int HANDEL_API xiaRemoveChannelSet(unsigned int detChan);
HANDEL_IMPORT int HANDEL_API xiaStartSystem(void);
HANDEL_IMPORT int HANDEL_API xiaDownloadFirmware(int detChan, char *type);
HANDEL_IMPORT int HANDEL_API xiaSetAcquisitionValues(int detChan, char *name, void *value);
HANDEL_IMPORT int HANDEL_API xiaGetAcquisitionValues(int detChan, char *name, void *value);
HANDEL_IMPORT int HANDEL_API xiaRemoveAcquisitionValues(int detChan, char *name);
HANDEL_IMPORT int HANDEL_API xiaUpdateUserParams(int detChan);
HANDEL_IMPORT int HANDEL_API xiaGainOperation(int detChan, char *name, void *value);
HANDEL_IMPORT int HANDEL_API xiaGainChange(int detChan, double deltaGain);
HANDEL_IMPORT int HANDEL_API xiaGainCalibrate(int detChan, double deltaGain);
HANDEL_IMPORT int HANDEL_API xiaStartRun(int detChan, unsigned short resume);
HANDEL_IMPORT int HANDEL_API xiaStopRun(int detChan);
HANDEL_IMPORT int HANDEL_API xiaGetRunData(int detChan, char *name, void *value);
HANDEL_IMPORT int HANDEL_API xiaDoSpecialRun(int detChan, char *name, void *info);
HANDEL_IMPORT int HANDEL_API xiaGetSpecialRunData(int detChan, char *name, void *value);
HANDEL_IMPORT int HANDEL_API xiaLoadSystem(char *type, char *filename);
HANDEL_IMPORT int HANDEL_API xiaSaveSystem(char *type, char *filename);
HANDEL_IMPORT int HANDEL_API xiaGetParameter(int detChan, const char *name, unsigned short *value);
HANDEL_IMPORT int HANDEL_API xiaSetParameter(int detChan, const char *name, unsigned short value);
HANDEL_IMPORT int HANDEL_API xiaGetNumParams(int detChan, unsigned short *numParams);
HANDEL_IMPORT int HANDEL_API xiaGetParamData(int detChan, char *name, void *value);
HANDEL_IMPORT int HANDEL_API xiaGetParamName(int detChan, unsigned short index, char *name);
HANDEL_IMPORT int HANDEL_API xiaBoardOperation(int detChan, char *name, void *value);
  HANDEL_IMPORT int HANDEL_API xiaMemoryOperation(int detChan, char *name, void *value);
HANDEL_IMPORT int HANDEL_API xiaCommandOperation(int detChan, byte_t cmd,
												unsigned int lenS, byte_t *send,
												unsigned int lenR, byte_t *recv);

HANDEL_IMPORT int HANDEL_API xiaFitGauss(long data[], int lower, int upper, float *pos, 
					 float *fwhm);
HANDEL_IMPORT int HANDEL_API xiaFindPeak(long *data, int numBins, float thresh, int *lower, 
					 int *upper);
HANDEL_IMPORT int HANDEL_API xiaExit(void);

HANDEL_IMPORT int HANDEL_API xiaEnableLogOutput(void);
HANDEL_IMPORT int HANDEL_API xiaSuppressLogOutput(void);
HANDEL_IMPORT int HANDEL_API xiaSetLogLevel(int level);
HANDEL_IMPORT int HANDEL_API xiaSetLogOutput(char *fileName);

  HANDEL_IMPORT int HANDEL_API xiaSetIOPriority(int pri);

  HANDEL_IMPORT void HANDEL_API xiaGetVersionInfo(int *rel, int *min, int *maj,
												  char *pretty);

  HANDEL_IMPORT int HANDEL_API xiaMemStatistics(unsigned long *total,
												unsigned long *current,
												unsigned long *peak);
  HANDEL_EXPORT void HANDEL_API xiaMemSetCheckpoint(void);
  HANDEL_EXPORT void HANDEL_API xiaMemLeaks(char *);

#ifdef __MEM_DBG__

#include <crtdbg.h>

HANDEL_IMPORT void xiaSetReportMode(void);
HANDEL_IMPORT void xiaMemCheckpoint(int pass);
HANDEL_IMPORT void xiaReport(char *message);
HANDEL_IMPORT void xiaMemDumpAllObjectsSince(void);
HANDEL_IMPORT void xiaDumpMemoryLeaks(void);
HANDEL_IMPORT void xiaEndMemDbg(void);

#endif /* __MEM_DBG__ */

#ifdef _DEBUG
  HANDEL_IMPORT void HANDEL_API xiaUnitTests(unsigned short tests);
#endif




#else									/* Begin old style C prototypes */
/*
 * following are internal prototypes for handel layer subset of xerxes.c routines
 */
HANDEL_IMPORT int HANDEL_API xiaInit();
HANDEL_IMPORT int HANDEL_API xiaInitHandel();
HANDEL_IMPORT int HANDEL_API xiaNewDetector();
HANDEL_IMPORT int HANDEL_API xiaAddDetectorItem();
HANDEL_IMPORT int HANDEL_API xiaModifyDetectorItem();
HANDEL_IMPORT int HANDEL_API xiaGetDetectorItem();
HANDEL_IMPORT int HANDEL_API xiaGetNumDetectors();
HANDEL_IMPORT int HANDEL_API xiaGetDetectors();
HANDEL_IMPORT int HANDEL_API xiaGetDetectors_VB();
HANDEL_IMPORT int HANDEL_API xiaRemoveDetector();
HANDEL_IMPORT int HANDEL_API xiaNewFirmware();
HANDEL_IMPORT int HANDEL_API xiaAddFirmwareItem();
HANDEL_IMPORT int HANDEL_API xiaModifyFirmwareItem();
HANDEL_IMPORT int HANDEL_API xiaGetFirmwareItem();
HANDEL_IMPORT int HANDEL_API xiaGetNumFirmwareSets();
HANDEL_IMPORT int HANDEL_API xiaGetFirmwareSets();
HANDEL_IMPORT int HANDEL_API xiaGetFirmwareSets_VB();
HANDEL_IMPORT int HANDEL_API xiaGetNumPTRRs();
HANDEL_IMPORT int HANDEL_API xiaRemoveFirmware();
HANDEL_IMPORT int HANDEL_API xiaNewModule();
HANDEL_IMPORT int HANDEL_API xiaAddModuleItem();
HANDEL_IMPORT int HANDEL_API xiaModifyModuleItem();
HANDEL_IMPORT int HANDEL_API xiaGetModuleItem();
HANDEL_IMPORT int HANDEL_API xiaGetNumModules();
HANDEL_IMPORT int HANDEL_API xiaGetModules();
HANDEL_IMPORT int HANDEL_API xiaGetModules_VB();
HANDEL_IMPORT int HANDEL_API xiaRemoveModule();
HANDEL_IMPORT int HANDEL_API xiaAddChannelSetElem();
HANDEL_IMPORT int HANDEL_API xiaRemoveChannelSetElem();
HANDEL_IMPORT int HANDEL_API xiaRemoveChannelSet();
HANDEL_IMPORT int HANDEL_API xiaStartSystem();
HANDEL_IMPORT int HANDEL_API xiaDownloadFirmware();
HANDEL_IMPORT int HANDEL_API xiaSetAcquisitionValues();
HANDEL_IMPORT int HANDEL_API xiaGetAcquisitionValues();
HANDEL_IMPORT int HANDEL_API xiaRemoveAcquisitionValues();
HANDEL_IMPORT int HANDEL_API xiaUpdateUserParams();
HANDEL_IMPORT int HANDEL_API xiaGainChange();
HANDEL_IMPORT int HANDEL_API xiaGainCalibrate();
HANDEL_IMPORT int HANDEL_API xiaStartRun();
HANDEL_IMPORT int HANDEL_API xiaStopRun();
HANDEL_IMPORT int HANDEL_API xiaGetRunData();
HANDEL_IMPORT int HANDEL_API xiaDoSpecialRun();
HANDEL_IMPORT int HANDEL_API xiaGetSpecialRunData();
HANDEL_IMPORT int HANDEL_API xiaLoadSystem();
HANDEL_IMPORT int HANDEL_API xiaSaveSystem();
HANDEL_IMPORT int HANDEL_API xiaGetParameter();
HANDEL_IMPORT int HANDEL_API xiaSetParameter();
HANDEL_IMPORT int HANDEL_API xiaGetNumParams();
HANDEL_IMPORT int HANDEL_API xiaGetParamData();
HANDEL_IMPORT int HANDEL_API xiaGetParamName();
HANDEL_IMPORT int HANDEL_API xiaBoardOperation();
  HANDEL_IMPORT int HANDEL_API xiaMemoryOperation();
  HANDEL_IMPORT int HANDEL_API xiaCommandOperation();
HANDEL_IMPORT int HANDEL_API xiaExit();

HANDEL_IMPORT int HANDEL_API xiaFitGauss();
HANDEL_IMPORT int HANDEL_API xiaFindPeak();

HANDEL_IMPORT int HANDEL_API xiaEnableLogOutput();
HANDEL_IMPORT int HANDEL_API xiaSuppressLogOutput();
HANDEL_IMPORT int HANDEL_API xiaSetLogLevel();
HANDEL_IMPORT int HANDEL_API xiaSetLogOutput();

  HANDEL_IMPORT int HANDEL_API xiaSetIOPriority();

  HANDEL_IMPORT void HANDEL_API xiaGetVersionInfo();

  HANDEL_IMPORT int HANDEL_API xiaMemStatistics();
  HANDEL_EXPORT void HANDEL_API xiaMemSetCheckpoint();
  HANDEL_EXPORT void HANDEL_API xiaMemLeaks();

#endif                                  /*   end if _HANDEL_PROTO_ */

/* If this is compiled by a C++ compiler, make it clear that these are C routines */
#ifdef __cplusplus
}
#endif

#endif						/* Endif for HANDEL_H */
