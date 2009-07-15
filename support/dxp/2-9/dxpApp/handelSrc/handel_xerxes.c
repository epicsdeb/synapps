/*
 * handel_xerxes.c
 *
 * Created 10/25/01 -- PJF
 *
 *
 * This serves as an interface to all calls to XerXes routines.
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
#include <limits.h>

#include "xia_handel.h"
#include "xia_system.h"
#include "xia_assert.h"

#include "handel_errors.h"
#include "xia_handel_structures.h"
#include "handel_xerxes.h"
#include "handel_generic.h"
#include "xerxes.h"
#include "xerxes_errors.h"

#include "fdd.h"









HANDEL_STATIC int xia__GetSystemFPGAName(Module *module, char *detType,
										 char *sysFPGAName, char *rawFilename,
										 boolean_t *found);
HANDEL_STATIC int xia__GetSystemDSPName(Module *module, char *detType,
										char *sysDSPName, char *rawFilename,
										boolean_t *found);
HANDEL_STATIC int xia__GetDSPName(Module *module, int channel, 
								  double peakingTime, char *dspName, 
								  char *detectorType, char *rawFilename);
HANDEL_STATIC int xia__GetFiPPIAName(Module *module, char *detType,
									 char *sysDSPName, char *rawFilename,
									 boolean_t *found);
HANDEL_STATIC int xia__GetFiPPIName(Module *module, int channel,
									double peakingTime, char *fippiName,
									char *detectorType, char *rawFilename);
HANDEL_STATIC int xia__GetSystemFiPPIName(Module *module, char *detType,
										 char *sysFipName, char *rawFilename,
										 boolean_t *found);


HANDEL_STATIC int xia__CopyInterfString(Module *m, char *interf);
HANDEL_STATIC int xia__CopyMDString(Module *m, char *md);
HANDEL_STATIC int xia__CopyChanString(Module *m, char *chan);

HANDEL_STATIC int xia__AddXerxesSysItems(void);
HANDEL_STATIC int xia__AddXerxesBoardType(Module *m);
HANDEL_STATIC int xia__AddXerxesInterface(Module *m);
HANDEL_STATIC int xia__AddXerxesModule(Module *m);
HANDEL_STATIC int xia__AddXerxesParams(Module *m);

HANDEL_STATIC int xia__AddSystemFPGA(Module *module, char *sysFPGAName,
									 char *rawFilename);
HANDEL_STATIC int xia__AddSystemDSP(Module *module, char *sysDSPName,
									char *rawFilename);
HANDEL_STATIC int xia__AddFiPPIA(Module *module, char *sysDSPName,
								 char *rawFilename);
HANDEL_STATIC int xia__AddSystemFiPPI(Module *module, char *sysFipName,
                                      char *rawFilename);

HANDEL_STATIC int xia__DoMMUConfig(Module *m);
HANDEL_STATIC int xia__DoSystemFPGA(Module *m);
HANDEL_STATIC int xia__DoSystemDSP(Module *m, boolean_t *found);
HANDEL_STATIC int xia__DoDSP(Module *m);
HANDEL_STATIC int xia__DoFiPPIA(Module *m, boolean_t *found);
HANDEL_STATIC int xia__DoFiPPI(Module *m);
HANDEL_STATIC int xia__DoSystemFiPPI(Module *m, boolean_t *found);

HANDEL_STATIC int xia__GetDetStringFromDetChan(int detChan, Module *m,
											   char *type);

/*****************************************************************************
 *
 * This routine calls the XerXes fit routine. See XerXes for more details.
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaFitGauss(long data[], int lower, int upper, float *pos, float *fwhm)
{
  int statusX;

  statusX = dxp_fitgauss0(data, &lower, &upper, pos, fwhm);

  if (statusX != XIA_SUCCESS)
	{
	  return XIA_XERXES;
	}

  return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine calls the XerXes find peak routine. See XerXes for more 
 * details.
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaFindPeak(long *data, int numBins, float thresh, int *lower, int *upper)
{
  int statusX;

  statusX = dxp_findpeak(data, &numBins, &thresh, lower, upper);

  if (statusX != XIA_SUCCESS)
	{
	  return XIA_XERXES;
	}

  return XIA_SUCCESS;
}



/*****************************************************************************
 *
 * This routine calls XerXes routines in order to build a proper XerXes
 * configuration based on the HanDeL information.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaBuildXerxesConfig(void)
{
  int statusX;
  int status;

  boolean_t found;
  boolean_t isSysFip;

  Module *current = NULL;


  statusX = dxp_init_ds();

  if (statusX != DXP_SUCCESS) {
	  xiaLogError("xiaBuildXerxesConfig", "Error initializing Xerxes internal "
				  "data structures", XIA_XERXES);
	  return XIA_XERXES;
	}


  status = xia__AddXerxesSysItems();

  if (status != XIA_SUCCESS) {
	xiaLogError("xiaBuildXerxesConfig", "Error adding system items to Xerxes "
				"configuration", status);
	return status;
  }

  /* Walk through the module list and make the proper calls to XerXes */
  current = xiaGetModuleHead();

  while (current != NULL) {

	status = xia__AddXerxesBoardType(current);

	if (status != XIA_SUCCESS) {
	  sprintf(info_string, "Error adding board type for alias = '%s'",
			  current->alias);
	  xiaLogError("xiaBuildXerxesConfig", info_string, status);
	  return status;
	}

	status = xia__AddXerxesInterface(current);

	if (status != XIA_SUCCESS) {
	  sprintf(info_string, "Error adding interface for alias = '%s'",
			  current->alias);
	  xiaLogError("xiaBuildXerxesConfig", info_string, status);
	  return status;
	}

	status = xia__AddXerxesModule(current);

	if (status != XIA_SUCCESS) {
	  sprintf(info_string, "Error adding module for alias = '%s'",
			  current->alias);
	  xiaLogError("xiaBuildXerxesConfig", info_string, status);
	  return status;
	}

	/* This section begins the firmware configuration area. Not all firmwares
	 * are required for each product, but we check to see what is available
	 * and we add it to the configuration if it is found.
	 */

	status = xia__DoMMUConfig(current);

	if (status != XIA_SUCCESS) {
	  sprintf(info_string, "Error adding MMU for alias = '%s'", current->alias);
	  xiaLogError("xiaBuildXerxesConfig", info_string, status);
	  return status;
	}

  status = xia__DoSystemFiPPI(current, &isSysFip);

  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error adding System FiPPI for alias = '%s'",
            current->alias);
    xiaLogError("xiaBuildXerxesConfig", info_string, status);
    return status;
  }

	status = xia__DoSystemFPGA(current);

	if (status != XIA_SUCCESS) {
	  sprintf(info_string, "Error adding System FPGA for alias = '%s'",
			  current->alias);
	  xiaLogError("xiaBuildXerxesConfig", info_string, status);
	  return status;
	}

	status = xia__DoSystemDSP(current, &found);

	if (status != XIA_SUCCESS) {
	  sprintf(info_string, "Error adding System DSP for alias = '%s'",
			  current->alias);
	  xiaLogError("xiaBuildXerxesConfig", info_string, status);
	  return status;
	}

	/* If we didn't find a system DSP then we assume that the hardware supports
	 * a single DSP for each channel.
	 */
	if (!found && !isSysFip) {
	  status = xia__DoDSP(current);

	  if (status != XIA_SUCCESS) {
		sprintf(info_string, "Error adding DSPs for alias = '%s'",
				current->alias);
		xiaLogError("xiaBuildXerxesConfig", info_string, status);
		return status;
	  }
	}

	status = xia__DoFiPPIA(current, &found);

	if (status != XIA_SUCCESS) {
	  sprintf(info_string, "Error adding FiPPI A for alias = '%s'",
			  current->alias);
	  xiaLogError("xiaBuildXerxesConfig", info_string, status);
	  return status;
	}

	if (!found && !isSysFip) {
	  status = xia__DoFiPPI(current);

	  if (status != XIA_SUCCESS) {
		sprintf(info_string, "Error adding FiPPIs for alias = '%s'",
				current->alias);
		xiaLogError("xiaBuildXerxesConfig", info_string, status);
		return status;
	  }
	}

	status = xia__AddXerxesParams(current);

	if (status != XIA_SUCCESS) {
	  sprintf(info_string, "Error adding params for alias = '%s'",
			  current->alias);
	  xiaLogError("xiaBuildXerxesConfig", info_string, status);
	  return status;
	}

	current = getListNext(current);
  }
	
  return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * Essentially a wrapper around dxp_user_setup().
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaUserSetup(void)
{
  int statusX;
  int status;
  int detector_chan;

  unsigned int modChan;

  unsigned short polarity;
  unsigned short type;

  double typeValue;
  double gainScale;

  char boardType[MAXITEM_LEN];
  char detectorType[MAXITEM_LEN];

  char *alias;
  char *detAlias;
  char *firmAlias;

  DetChanElement *current = NULL;

  FirmwareSet *firmwareSet = NULL;

  CurrentFirmware *currentFirmware = NULL;

  Module *module = NULL;

  Detector *detector = NULL;

  XiaDefaults *defaults = NULL;

  PSLFuncs localFuncs;

  statusX = dxp_user_setup();

  if (statusX != DXP_SUCCESS)
	{
	  status = XIA_XERXES;
	  xiaLogError("xiaUserSetup", "Error downloading firmware", status);
	  return status;
	}

  /* Add calls to xiaSetAcquisitionValues() here using values from the defaults
   * list.
   */


  /* Set polarity and reset time from info in detector struct */
  current = xiaGetDetChanHead();
 	
  while (current != NULL)
	{
	  switch (xiaGetElemType(current->detChan))
		{
		case SET:
		  /* Skip SETs since all SETs are composed of SINGLES */
		  break;
		  
		case SINGLE:
		  status = xiaGetBoardType(current->detChan, boardType);

		  if (status != XIA_SUCCESS)
			{
			  sprintf(info_string, "Unable to get boardType for detChan %u", current->detChan);
			  xiaLogError("xiaUserSetup", info_string, status);
			  return status;
			}

		  alias         		= xiaGetAliasFromDetChan(current->detChan);
		  module        		= xiaFindModule(alias);
		  modChan       		= xiaGetModChan((unsigned int)current->detChan);
		  firmAlias     		= module->firmware[modChan];
		  firmwareSet   		= xiaFindFirmware(firmAlias);
		  detAlias      		= module->detector[modChan];
		  detector_chan 		= module->detector_chan[modChan];
		  detector      		= xiaFindDetector(detAlias);
		  polarity      		= detector->polarity[detector_chan];
		  type			  		= detector->type;
		  typeValue     		= detector->typeValue[detector_chan];
		  gainScale             = module->gain[modChan];
		  currentFirmware	    = &module->currentFirmware[modChan];
		  defaults      		= xiaGetDefaultFromDetChan((unsigned int)current->detChan);

		  switch (detector->type)
			{
			case XIA_DET_RESET:
			  strcpy(detectorType, "RESET");
			  break;
		
			case XIA_DET_RCFEED:
			  strcpy(detectorType, "RC");
			  break;
		
			default:
			case XIA_DET_UNKNOWN:
			  status = XIA_MISSING_TYPE;
			  sprintf(info_string, "No detector type specified for detChan %d", current->detChan);
			  xiaLogError("xiaSetAcquisitionValues", info_string, status);
			  return status;
			  break;
			}

		  status = xiaLoadPSL(boardType, &localFuncs);

		  if (status != XIA_SUCCESS)
			{
			  sprintf(info_string, "Unable to load PSL funcs for detChan %d", current->detChan);
			  xiaLogError("xiaUserSetup", info_string, status);
			  return status;
			}

		  status = localFuncs.setPolarity((int)current->detChan, detector,
										  detector_chan, defaults, module);

		  if (status != XIA_SUCCESS)
			{
			  sprintf(info_string, "Unable to set polarity for detChan %d", current->detChan);
			  xiaLogError("xiaUserSetup", info_string, status);
			  return status;
			}

		  status = localFuncs.setDetectorTypeValue((int)current->detChan, detector, detector_chan, defaults);

		  if (status != XIA_SUCCESS)
			{
			  sprintf(info_string, "Unable to set detector typeValue for detChan %d", current->detChan);
			  xiaLogError("xiaUserSetup", info_string, status);
			  return status;
			}

		  /* Now we can do the defaults */
		  status = localFuncs.userSetup((int)current->detChan, defaults, firmwareSet,
										currentFirmware, detectorType, gainScale,
										detector, detector_chan, module,
										modChan);
   
		  if (status != XIA_SUCCESS)
			{
			  sprintf(info_string, "Unable to complete user setup for detChan %d",
					  current->detChan);
			  xiaLogError("xiaUserSetup", info_string, status);
			  return status;
			}

		  /* Do any DSP parameters that are in the list */
		  status = xiaUpdateUserParams(current->detChan);

		  if (status != XIA_SUCCESS) {

			sprintf(info_string, "Unable to update user parameters for detChan %d", current->detChan);
			xiaLogError("xiaUserSetup", info_string, status);
			return status;
		  }

		  break;

		case 999:
		  status = XIA_INVALID_DETCHAN;
		  xiaLogError("xiaUserSetup", "detChan number is not in the list of valid values ", status);
		  return status;
		  break;
		default:
		  status = XIA_UNKNOWN;
		  xiaLogError("xiaUserSetup", "Should not be seeing this message", status);
		  return status;
		  break;
		}

	  current = getListNext(current);
	}

  return XIA_SUCCESS;
}


/** @brief Copy the interface specfic information into the target string.
 *
 * Assumes that @a interf has already been allocated to the proper size. This
 * routine is meant to encapsulate all of the product-specific logic in it.
 */
HANDEL_STATIC int xia__CopyInterfString(Module *m, char *interf)
{
  ASSERT(m      != NULL);
  ASSERT(interf != NULL);


  switch (m->interface_info->type) {
  case NO_INTERFACE:
  default:
	sprintf(info_string, "No interface string specified for alias '%s'",
			m->alias);
	xiaLogError("xia__CopyInterfString", info_string, XIA_MISSING_INTERFACE);
	return XIA_MISSING_INTERFACE;
	break;

#ifndef EXCLUDE_CAMAC
  case JORWAY73A:
  case GENERIC_SCSI:
	strcpy(interf, "camacdll.dll");
	break;
#endif /* EXCLUDE_CAMAC */

#ifndef EXCLUDE_EPP
  case EPP:
  case GENERIC_EPP:
	sprintf(interf, "%#x", m->interface_info->info.epp->epp_address);
	break;
#endif /* EXCLUDE_EPP */

#ifndef EXCLUDE_USB
  case USB:
	sprintf(interf, "%i", m->interface_info->info.usb->device_number);
	break;
#endif /* EXCLUDE_USB */

#ifndef EXCLUDE_USB2
  case USB2:
    sprintf(interf, "%d", m->interface_info->info.usb2->device_number);
    break;
#endif /* EXCLUDE_USB2 */

#ifndef EXCLUDE_SERIAL
  case SERIAL:
	sprintf(interf, "COM%u", m->interface_info->info.serial->com_port);
	break;
#endif /* EXCLUDE_SERIAL */

#ifndef EXCLUDE_PLX
  case PLX:
	sprintf(interf, "pxi");
	break;
#endif /* EXCLUDE_PLX */

	/* XXX ARCNET stuff goes here... */
  }

  return XIA_SUCCESS;
}


/** @brief Builds the MD string required by Xerxes.
 *
 * Assumes that @a md has already been allocated. This routine is meant to
 * encapsulate all of the hardware specific logic for building the MD string.
 *
 */
HANDEL_STATIC int xia__CopyMDString(Module *m, char *md)
{
  ASSERT(m != NULL);
  ASSERT(md != NULL);


  switch (m->interface_info->type) {
  case NO_INTERFACE:
  default:
	sprintf(info_string, "No interface string specified for alias '%s'",
		   m->alias);
	xiaLogError("xia__CopyMDString", info_string, XIA_MISSING_INTERFACE);
	return XIA_MISSING_INTERFACE;
	break;

#ifndef EXCLUDE_CAMAC
  case JORWAY73A:
  case GENERIC_SCSI:
	sprintf(md, "%u%u%02u", m->interface_info->info.jorway73a->scsi_bus, 
			m->interface_info->info.jorway73a->crate_number,
			m->interface_info->info.jorway73a->slot);
	break;
#endif /* EXCLUDE_CAMAC */

#ifndef EXCLUDE_EPP
  case EPP:
  case GENERIC_EPP:
	/* If default then dont change anything, else tack on a : in front 
	 * of the string (tells XerXes to 
	 * treat this as a multi-module EPP chain 
	 */
	if (m->interface_info->info.epp->daisy_chain_id == UINT_MAX) {
	  sprintf(md, "0");
	} else {
	  sprintf(md, ":%u", m->interface_info->info.epp->daisy_chain_id);
	}
	break;
#endif /* EXCLUDE_EPP */

#ifndef EXCLUDE_USB
  case USB:
	sprintf(md, "%i", m->interface_info->info.usb->device_number);
	break;
#endif /* EXCLUDE_USB */

#ifndef EXCLUDE_USB2
  case USB2:
    sprintf(md, "%d", m->interface_info->info.usb2->device_number);
    break;
#endif /* EXCLUDE_USB2 */

#ifndef EXCLUDE_SERIAL
  case SERIAL:
	sprintf(md, "%u", m->interface_info->info.serial->com_port);
	break;
#endif /* EXCLUDE_ARCNET */

#ifndef EXCLUDE_PLX
  case PLX:
	sprintf(md, "%u:%u", m->interface_info->info.plx->bus,
			m->interface_info->info.plx->slot);
	break;

#endif /* EXCLUDE_PLX */

	/* XXX ARCNET stuff goes here... */

  }

  return XIA_SUCCESS;
}


/** @brief Get the DSP name for the specified channel.
 *
 * If no FDD file is specified, then the DSP name is retrieved from the firmware
 * Structure. Otherwise, the DSP code is retrieved from the FDD file using
 * the FDD library.
 *
 */
HANDEL_STATIC int xia__GetDSPName(Module *module, int channel,
								  double peakingTime, char *dspName,
								  char *detectorType, char *rawFilename)
{
  char *firmAlias;
  char *tmpPath = NULL;

  int status;

  FirmwareSet *firmwareSet = NULL;


  firmAlias   = module->firmware[channel];
  firmwareSet = xiaFindFirmware(firmAlias);

  if (firmwareSet->filename == NULL) {
	status = xiaGetDSPNameFromFirmware(firmAlias, peakingTime, dspName);

	if (status != XIA_SUCCESS) {
	  sprintf(info_string, "Error getting DSP code for firmware %s @ "
			  "peaking time = %f", firmAlias, peakingTime);
	  xiaLogError("xiaGetDSPName", info_string, status);
	  return status;
	}
	
  } else {

    if (firmwareSet->tmpPath) {
      tmpPath = firmwareSet->tmpPath;
    } else {
      tmpPath = utils->funcs->dxp_md_tmp_path();
    }

    status = xiaFddGetFirmware(firmwareSet->filename, tmpPath, "dsp", peakingTime, 
                               (unsigned short)firmwareSet->numKeywords,
                               firmwareSet->keywords, detectorType, dspName,
                               rawFilename);

	if (status != XIA_SUCCESS) {
	  sprintf(info_string, "Error getting DSP code from FDD file %s @ "
			  "peaking time = %f", firmwareSet->filename, peakingTime);
	  xiaLogError("xiaGetDSPName", info_string, status);
	  return status;
	}

	/* XXX Dump the FDD data */
	xiaLogDebug("xiaGetDSPName", "***** Dump of data sent to FDD *****");
	sprintf(info_string, "firmwareSet->filename = %s", firmwareSet->filename);
	xiaLogDebug("xiaGetDSPName", info_string);
	sprintf(info_string, "peakingTime = %.3f", peakingTime);
	xiaLogDebug("xiaGetDSPName", info_string);
	sprintf(info_string, "detectorType = %s", detectorType);
	xiaLogDebug("xiaGetDSPName", info_string);
	sprintf(info_string, "Returned DSPName = %s", dspName);
	xiaLogDebug("xiaGetDSPName", info_string);
  }

  return XIA_SUCCESS;
}


/** @brief Gets the FiPPI name from the firmware configuration.
 *
 */
HANDEL_STATIC int xia__GetFiPPIName(Module *module, int channel,
									double peakingTime, char *fippiName,
									char *detectorType, char *rawFilename)
{
  char *firmAlias;
  char *tmpPath = NULL;

  int status;

  FirmwareSet *firmwareSet = NULL;

  firmAlias = module->firmware[channel];

  firmwareSet = xiaFindFirmware(firmAlias);

  if (firmwareSet->filename == NULL)
	{
	  status = xiaGetFippiNameFromFirmware(firmAlias, peakingTime, fippiName);
	
	  if (status != XIA_SUCCESS)
		{
		  sprintf(info_string, "Error getting FiPPI code for firmware %s @ peaking time = %f", firmAlias, peakingTime);
		  xiaLogError("xia__GetFiPPIName", info_string, status);
		  return status;
		}

	  /* I think that we need this! */
	  strcpy(rawFilename, fippiName);

	} else {

    if (firmwareSet->tmpPath) {
      tmpPath = firmwareSet->tmpPath;
    } else {
      tmpPath = utils->funcs->dxp_md_tmp_path();
    }

	  status = xiaFddGetFirmware(firmwareSet->filename, tmpPath, "fippi",
                               peakingTime,
                               (unsigned short)firmwareSet->numKeywords,
                               firmwareSet->keywords, detectorType, fippiName,
                               rawFilename);


	  if (status != XIA_SUCCESS)
		{
		  sprintf(info_string, "Error getting FiPPI code from FDD file %s @ peaking time = %f", firmwareSet->filename, peakingTime);
		  xiaLogError("xia__GetFiPPIName", info_string, status);
		  return status;
		}

	  /* !!DEBUG!! sequence...used to test FDD stuff */
	  xiaLogDebug("xia__GetFiPPIName", "***** Dump of data sent to FDD *****");
	  sprintf(info_string, "firmwareSet->filename = %s", firmwareSet->filename);
	  xiaLogDebug("xia__GetFiPPIName", info_string);
	  sprintf(info_string, "peakingTime = %.3f", peakingTime);
	  xiaLogDebug("xia__GetFiPPIName", info_string);
	  sprintf(info_string, "detectorType = %s", detectorType);
	  xiaLogDebug("xia__GetFiPPIName", info_string);
	  sprintf(info_string, "Returned fippiName = %s", fippiName);
	  xiaLogDebug("xia__GetFiPPIName", info_string);
	}

  return XIA_SUCCESS;
}


/** @brief Get the name of the MMU firmware, if it is defined.
 *
 * Returns status indicating if the MMU firmware was found or not.
 *
 */
HANDEL_STATIC int xia__GetMMUName(Module *m, int channel, char *mmuName,
								  char *rawFilename)
{
  char *firmAlias = NULL;

  FirmwareSet *firmware = NULL;


  ASSERT(m != NULL);
  ASSERT(m->firmware != NULL);
  ASSERT(m->firmware[channel] != NULL);


  firmAlias = m->firmware[channel];

  firmware = xiaFindFirmware(firmAlias);
  ASSERT(firmware != NULL);

  if (firmware->filename == NULL) {

	if (firmware->mmu == NULL) {
	  return XIA_NO_MMU;
	}

	strcpy(mmuName, firmware->mmu);
	strcpy(rawFilename, firmware->mmu); 

  } else {
	/* Do something with the FDD here */
	return XIA_NO_MMU;
  }

  return XIA_SUCCESS;
}


/** @brief Adds a system FPGA to the Xerxes configuration.
 *
 */
HANDEL_STATIC int xia__AddSystemFPGA(Module *module, char *sysFPGAName,
									 char *rawFilename)
{
  int statusX;

  char *sysFPGAStr[1];


  ASSERT(module != NULL);
  ASSERT(sysFPGAName != NULL);
  ASSERT(rawFilename != NULL);


  sysFPGAStr[0] = (char *)handel_md_alloc(strlen(sysFPGAName) + 1);

  if (!sysFPGAStr[0]) {
	sprintf(info_string, "Unable to allocate %d bytes for 'sysFPGAStr[0]'",
			strlen(sysFPGAName) + 1);
	xiaLogError("xia__AddSystemFPGA", info_string, XIA_NOMEM);
	return XIA_NOMEM;
  }

  strncpy(sysFPGAStr[0], sysFPGAName, strlen(sysFPGAName) + 1);
  
  statusX = dxp_add_board_item("system_fpga", (char **)sysFPGAStr);

  handel_md_free(sysFPGAStr[0]);
  
  if (statusX != DXP_SUCCESS) {
	xiaLogError("xia__AddSystemFPGA", "Error adding 'system_fpga' board item",
				XIA_XERXES);
	return XIA_XERXES;
  }

  return XIA_SUCCESS;
}


/** @brief Retrieves the system FPGA name from the firmware, if it has
 * been defined.
 *
 * Currently, this routine only returns success if a system FPGA is 
 * defined in an FDD file. All other conditions return false, including
 * the case where an FDD file is not used.
 *
 * Future versions of this routine should also include calling parameters
 * for the length of @a sysFPGAName and @a rawFilename to prevent buffer
 * overflows from occuring.
 *
 * The FDD file associates a peaking time range with each entry, but some
 * firmware types, such as the system FPGA, are global so we pass in a fake
 * peaking time instead.
 *
 * The System FPGA is always assumed to be defined in the firmware for 
 * channel 0 in the module.
 *
 */
HANDEL_STATIC int xia__GetSystemFPGAName(Module *module, char *detType,
										 char *sysFPGAName, char *rawFilename,
										 boolean_t *found)
{
  int status;

  double fake_pt = 1.0;

  char *tmpPath = NULL;

  FirmwareSet *fs = NULL;


  ASSERT(module != NULL);
  ASSERT(detType != NULL);
  ASSERT(sysFPGAName != NULL);
  ASSERT(rawFilename != NULL);
  ASSERT(found != NULL);


  *found = FALSE_;

  fs = xiaFindFirmware(module->firmware[0]);

  if (!fs) {
	sprintf(info_string, "No firmware set defined for alias '%s'",
			module->firmware[0]);
	xiaLogError("xia__GetSystemFPGAName", info_string, XIA_NULL_FIRMWARE);
	return XIA_NULL_FIRMWARE;
  }

  if (!fs->filename) {
	sprintf(info_string, "No FDD defined in the firmware set '%s'",
			module->firmware[0]);
	xiaLogError("xia__GetSystemFPGAName", info_string, XIA_NO_FDD);
	return XIA_NO_FDD;
  }

  if (fs->tmpPath) {
    tmpPath = fs->tmpPath;
  } else {
    tmpPath = utils->funcs->dxp_md_tmp_path();
  }

  status = xiaFddGetFirmware(fs->filename, tmpPath, "system_fpga", fake_pt, 0,
                             NULL, detType, sysFPGAName, rawFilename);

  if (status != XIA_SUCCESS) {
	sprintf(info_string, "Error finding system_fpga in %s", fs->filename);
	xiaLogError("xia__GetSystemFPGAName", info_string, status);
	return status;
  }

  *found = TRUE_;
  return XIA_SUCCESS;
}


/** @brief Get the system DSP name from the FDD file, if either exists.
 *
 * Currently, this routine only returns success if a system DSP is 
 * defined in an FDD file. All other conditions return false, including
 * the case where an FDD file is not used.
 *
 * Future versions of this routine should also include calling parameters
 * for the length of @a sysDSPName and @a rawFilename to prevent buffer
 * overflows from occuring.
 *
 * XXX: This routine contains a hard-coded peaking time since most FDDs only 
 * contain one DSP for the entire range of peaking times. This is a hack and 
 * must be fixed in future versions.
 *
 * The System DSP is always assumed to be defined in the firmware for 
 * channel 0 in the module.
 */
HANDEL_STATIC int xia__GetSystemDSPName(Module *module, char *detType,
										char *sysDSPName, char *rawFilename,
										boolean_t *found)
{
  int status;

  double fake_pt = 1.0;

  char *tmpPath = NULL;

  FirmwareSet *fs = NULL;


  ASSERT(module != NULL);
  ASSERT(detType != NULL);
  ASSERT(sysDSPName != NULL);
  ASSERT(rawFilename != NULL);
  ASSERT(found != NULL);


  *found = FALSE_;

  fs = xiaFindFirmware(module->firmware[0]);

  if (!fs) {
	sprintf(info_string, "No firmware set defined for alias '%s'",
			module->firmware[0]);
	xiaLogError("xia__GetSystemDSPName", info_string, XIA_NULL_FIRMWARE);
	return XIA_NULL_FIRMWARE;
  }

  if (!fs->filename) {
	sprintf(info_string, "No FDD defined in the firmware set '%s'",
			module->firmware[0]);
	xiaLogError("xia__GetSystemDSPName", info_string, XIA_NO_FDD);
	return XIA_NO_FDD;
  }

  if (fs->tmpPath) {
    tmpPath = fs->tmpPath;
  } else {
    tmpPath = utils->funcs->dxp_md_tmp_path();
  }

  status = xiaFddGetFirmware(fs->filename, tmpPath, "system_dsp", fake_pt, 0, NULL,
							 detType, sysDSPName, rawFilename);

  if (status != XIA_SUCCESS) {
	sprintf(info_string, "Error finding system_dsp in %s", fs->filename);
	xiaLogError("xia__GetSystemDSPName", info_string, status);
	return status;
  }

  *found = TRUE_;
  return XIA_SUCCESS;

}


/** Add the specified system DSP to the Xerxes configuration.
 *
 */
HANDEL_STATIC int xia__AddSystemDSP(Module *module, char *sysDSPName,
									char *rawFilename)
{
  int statusX;

  char *sysDSPStr[1];


  ASSERT(module != NULL);
  ASSERT(sysDSPName != NULL);
  ASSERT(rawFilename != NULL);


  sysDSPStr[0] = (char *)handel_md_alloc(strlen(sysDSPName) + 1);

  if (!sysDSPStr[0]) {
	sprintf(info_string, "Unable to allocate %d bytes for 'sysDSPStr[0]'",
			strlen(sysDSPName) + 1);
	xiaLogError("xiaAddSystemDSP", info_string, XIA_NOMEM);
	return XIA_NOMEM;
  }

  strncpy(sysDSPStr[0], sysDSPName, strlen(sysDSPName) + 1);
  
  statusX = dxp_add_board_item("system_dsp", (char **)sysDSPStr);

  handel_md_free(sysDSPStr[0]);
  
  if (statusX != DXP_SUCCESS) {
	xiaLogError("xiaAddSystemDSP", "Error adding 'system_dsp' board item",
				XIA_XERXES);
	return XIA_XERXES;
  }

  return XIA_SUCCESS;
}


/** @brief Get the FiPPI A name from the FDD file, if either exists.
 *
 * Currently, this routine only returns success if a FiPPI A is 
 * defined in an FDD file. All other conditions return false, including
 * the case where an FDD file is not used.
 *
 * Future versions of this routine should also include calling parameters
 * for the length of @a sysFippiAName and @a rawFilename to prevent buffer
 * overflows from occuring.
 *
 * The Fippi A is always assumed to be defined in the firmware for 
 * channel 0 in the module.
 */
HANDEL_STATIC int xia__GetFiPPIAName(Module *module, char *detType,
									 char *sysFippiAName, char *rawFilename,
									 boolean_t *found)
{
  int status;

  double fake_pt = 1.0;
  
  char *tmpPath = NULL;

  FirmwareSet *fs = NULL;


  ASSERT(module != NULL);
  ASSERT(detType != NULL);
  ASSERT(sysFippiAName != NULL);
  ASSERT(rawFilename != NULL);
  ASSERT(found != NULL);


  *found = FALSE_;

  fs = xiaFindFirmware(module->firmware[0]);

  if (!fs) {
	sprintf(info_string, "No firmware set defined for alias '%s'",
			module->firmware[0]);
	xiaLogError("xia__GetFiPPIAName", info_string, XIA_NULL_FIRMWARE);
	return XIA_NULL_FIRMWARE;
  }

  if (!fs->filename) {
	sprintf(info_string, "No FDD defined in the firmware set '%s'",
			module->firmware[0]);
	xiaLogError("xia__GetFiPPIAName", info_string, XIA_NO_FDD);
	return XIA_NO_FDD;
  }

  if (fs->tmpPath) {
    tmpPath = fs->tmpPath;
  } else {
    tmpPath = utils->funcs->dxp_md_tmp_path();
  }

  status = xiaFddGetFirmware(fs->filename, tmpPath, "fippi_a", fake_pt, 0, NULL,
							 detType, sysFippiAName, rawFilename);

  if (status != XIA_SUCCESS) {
	sprintf(info_string, "Error finding fippi_a in %s", fs->filename);
	xiaLogError("xia__GetFiPPIAName", info_string, status);
	return status;
  }

  *found = TRUE_;
  return XIA_SUCCESS;
}


/** @brief Add FiPPI A to the Xerxes configuration.
 *
 */
HANDEL_STATIC int xia__AddFiPPIA(Module *module, char *sysFippiAName,
								 char *rawFilename)
{
  int statusX;

  char *sysFippiAStr[1];


  ASSERT(module != NULL);
  ASSERT(sysFippiAName != NULL);
  ASSERT(rawFilename != NULL);


  sysFippiAStr[0] = (char *)handel_md_alloc(strlen(sysFippiAName) + 1);

  if (!sysFippiAStr[0]) {
	sprintf(info_string, "Unable to allocate %d bytes for 'sysFippiAStr[0]'",
			strlen(sysFippiAName) + 1);
	xiaLogError("xiaAddSystemFippiA", info_string, XIA_NOMEM);
	return XIA_NOMEM;
  }

  strncpy(sysFippiAStr[0], sysFippiAName, strlen(sysFippiAName) + 1);
  
  statusX = dxp_add_board_item("fippi_a", (char **)sysFippiAStr);

  handel_md_free(sysFippiAStr[0]);
  
  if (statusX != DXP_SUCCESS) {
	xiaLogError("xiaAddSystemFippiA", "Error adding 'fippi_a' board item",
				XIA_XERXES);
	return XIA_XERXES;
  }

  return XIA_SUCCESS;
}


/** @brief Add all of the system items to the Xerxes configuration
 *
 */
HANDEL_STATIC int xia__AddXerxesSysItems(void)
{
  int i;
  int statusX;


  /* Xerxes requires us to add all of allowed/known board types to the
   * system. The list of known boards is controlled via preprocessor macros
   * passed in to the build. For more information, see the top-level Makefile.
   */
  for (i = 0; i < N_KNOWN_BOARDS; i++) {
	statusX = dxp_add_system_item(BOARD_LIST[i], (char **)SYS_NULL);

	if (statusX != DXP_SUCCESS) {
	  sprintf(info_string, "Error adding Xerxes system item '%s'",
			  BOARD_LIST[i]);
	  xiaLogError("xia__AddXerxesSysItems", info_string, XIA_XERXES);
	  return XIA_XERXES;
	}
  }

  statusX = dxp_add_system_item("preamp", (char **)SYS_NULL);

  if (statusX != DXP_SUCCESS) {
	xiaLogError("xia__AddXerxesSysItems", "Error adding Xerxes system item "
				"'preamp'", XIA_XERXES);
	return XIA_XERXES;
  }

  return XIA_SUCCESS;
}


/** @brief Add the board type of the specified module to the Xerxes
 * configuration.
 *
 */
HANDEL_STATIC int xia__AddXerxesBoardType(Module *m)
{
  int statusX;

  char **type = NULL;


  ASSERT(m != NULL);


  type = (char **)handel_md_alloc(sizeof(char *));
  
  if (!type) {
	sprintf(info_string, "Error allocating %d bytes for 'type'", sizeof(char *));
	xiaLogError("xia__AddXerxesBoardType", info_string, XIA_NOMEM);
	return XIA_NOMEM;
  }

  type[0] = (char *)handel_md_alloc(strlen(m->type) + 1);

  if (!type[0]) {
	handel_md_free(type);
	
	sprintf(info_string, "Error allocating %d bytes for 'type[0]'",
			strlen(m->type) + 1);
	xiaLogError("xia__AddXerxesBoardType", info_string, XIA_NOMEM);
	return XIA_NOMEM;
  }

  strcpy(type[0], m->type);

  statusX = dxp_add_board_item("board_type", type);

  handel_md_free(type[0]);
  handel_md_free(type);

  if (statusX != DXP_SUCCESS) {
	sprintf(info_string, "Error adding board_type '%s' to Xerxes for alias '%s'",
			m->type, m->alias);
	xiaLogError("xia__AddXerxesBoardType", info_string, XIA_XERXES);
	return XIA_XERXES;
  }

  return XIA_SUCCESS;
}


/** @brief Add the required interface string to the Xerxes configuration
 *
 */
HANDEL_STATIC int xia__AddXerxesInterface(Module *m)
{
  int status;
  int statusX;
  int interfLen = 0;

  char *interf[2];

  
  ASSERT(m != NULL);


  interfLen = strlen(INTERF_LIST[m->interface_info->type]) + 1;

  interf[0] = (char *)handel_md_alloc(interfLen);

  if (!interf[0]) {
	sprintf(info_string, "Error allocating %d bytes for 'interf[0]'", interfLen);
	xiaLogError("xia__AddXerxesInterface", info_string, XIA_NOMEM);
	return XIA_NOMEM;
  }

  interf[1] = (char *)handel_md_alloc(MAX_INTERF_LEN);
  
  if (!interf[1]) {
	handel_md_free(interf[0]);

	sprintf(info_string, "Error allocating %d bytes for 'interf[1]'",
			MAX_INTERF_LEN);
	xiaLogError("xia___AddXerxesInterface", info_string, XIA_NOMEM);
	return XIA_NOMEM;
  }

  strcpy(interf[0], INTERF_LIST[m->interface_info->type]);

  sprintf(info_string, "type = %d, name = '%s'", m->interface_info->type,
          INTERF_LIST[m->interface_info->type]);
  xiaLogDebug("xia__AddXerxesInterface", info_string);

  status = xia__CopyInterfString(m, interf[1]);

  if (status != XIA_SUCCESS) {
	handel_md_free(interf[0]);
	handel_md_free(interf[1]);

	sprintf(info_string, "Error getting interface string for alias '%s'",
			m->alias);
	xiaLogError("xia__AddXerxesInterface", info_string, status);
	return status;
  }

  statusX = dxp_add_board_item("interface", (char **)interf);

  if (statusX != DXP_SUCCESS) {
	sprintf(info_string, "Error adding interface '%s, %s' to Xerxes for alias "
			"'%s'", interf[0], interf[1], m->alias);
	xiaLogError("xia__AddXerxesInterface", info_string, XIA_XERXES);

	handel_md_free(interf[0]);
	handel_md_free(interf[1]);

	return XIA_XERXES;
  }

  handel_md_free(interf[0]);
  handel_md_free(interf[1]);

  return XIA_SUCCESS;
}


/** @brief Add the module board item to the Xerxes configuration.
 *
 */
HANDEL_STATIC int xia__AddXerxesModule(Module *m)
{
  int status;
  int statusX;

  unsigned int i;
  unsigned int j;

  char **modStr = NULL;
  
  char mdStr[MAX_MD_LEN];
  char chanStr[MAX_NUM_CHAN_LEN];


  ASSERT(m != NULL);


  modStr = (char **)handel_md_alloc((m->number_of_channels + 2) *
									sizeof(char *));

  if (!modStr) {
	sprintf(info_string, "Error allocating %d bytes for 'modStr'",
			(m->number_of_channels + 2) * sizeof(char *));
	xiaLogError("xia__AddXerxesModule", info_string, XIA_NOMEM);
	return XIA_NOMEM;
  }

  /* The first two module string elements always refer to the MD string and
   * the "number of channels" for this module string.
   */
  modStr[0] = &(mdStr[0]);
  modStr[1] = &(chanStr[0]);

  status = xia__CopyMDString(m, modStr[0]);

  if (status != XIA_SUCCESS) {
	handel_md_free(modStr);

	sprintf(info_string, "Error copying MD string to modules string for alias "
			"'%s'", m->alias);
	xiaLogError("xia__AddXerxesModule", info_string, status);
	return status;
  }

  status = xia__CopyChanString(m, modStr[1]);

  if (status != XIA_SUCCESS) {
	handel_md_free(modStr);

	sprintf(info_string, "Error copying channel string to modules string for "
			"alias '%s'", m->alias);
	xiaLogError("xia__AddXerxesModule", info_string, status);
	return status;
  }

  for (i = 0; i < m->number_of_channels; i++) {
	modStr[i + 2] = (char *)handel_md_alloc(MAX_CHAN_LEN);

	if (!modStr[i + 2]) {
	  /* Need to unwind the previous allocations */
	  for (j = i - 1; j >= 0; j--) {
		handel_md_free(modStr[j + 2]);
	  }
	  
	  handel_md_free(modStr);

	  sprintf(info_string, "Error allocating %d bytes for 'modStr[%d]'",
			  MAX_CHAN_LEN, i + 2);
	  xiaLogError("xia__AddXerxesModule", info_string, XIA_NOMEM);
	  return XIA_NOMEM;
	}
	
	sprintf(modStr[i + 2], "%d", m->channels[i]);
  }

  statusX = dxp_add_board_item("module", modStr);

  for (i = 0; i < m->number_of_channels; i++) {
	handel_md_free(modStr[i + 2]);
  }

  handel_md_free(modStr);

  if (statusX != DXP_SUCCESS) {
	sprintf(info_string, "Error adding module to Xerxes for alias '%s'",
			m->alias);
	xiaLogError("xia__AddXerxesModule", info_string, XIA_XERXES);
	return XIA_XERXES;
  }

  return XIA_SUCCESS;
}


/** @brief Copys the "number of channels" string in the format required by
 * Xerxes.
 *
 * Assumes that @a chan is already allocated.
 *
 */
HANDEL_STATIC int xia__CopyChanString(Module *m, char *chan)
{
  ASSERT(m != NULL);
  ASSERT(chan != NULL);


  sprintf(chan, "%u", m->number_of_channels);
  
  return XIA_SUCCESS;
}


/** @brief Checks for the presence of the MMU firmware and configures
 * Xerxes with it -- if found.
 *
 */
HANDEL_STATIC int xia__DoMMUConfig(Module *m)
{
  int status;
  int statusX;
  
  char name[MAXFILENAME_LEN];
  char rawName[MAXFILENAME_LEN];

  char **mmu = NULL;


  ASSERT(m != NULL);

  
  status = xia__GetMMUName(m, 0, name, rawName);

  if (status != XIA_NO_MMU) {

	if (status != XIA_SUCCESS) {
	  sprintf(info_string, "Error trying to get MMU nume for alias '%s'",
			  m->alias);
	  xiaLogError("xia__DoMMUConfig", info_string, status);
	  return status;
	}

	strcpy(m->currentFirmware[0].currentMMU, rawName);

	mmu = (char **)handel_md_alloc(sizeof(char **));

	if (!mmu) {
	  sprintf(info_string, "Error allocating %d bytes for 'mmu'",
			  sizeof(char **));
	  xiaLogError("xia__DoMMUConfig", info_string, XIA_NOMEM);
	  return XIA_NOMEM;
	}

	mmu[0] = (char *)handel_md_alloc(strlen(name) + 1);

	if (!mmu[0]) {
	  handel_md_free(mmu);

	  sprintf(info_string, "Error allocating %d bytes for 'mmu[0]'",
			  strlen(name) + 1);
	  xiaLogError("xia__DoMMUConfig", info_string, XIA_NOMEM);
	  return XIA_NOMEM;
	}

	strcpy(mmu[0], name);

	statusX = dxp_add_board_item("mmu", mmu);

	handel_md_free(mmu[0]);
	handel_md_free(mmu);

	if (statusX != DXP_SUCCESS) {
	  sprintf(info_string, "Error adding MMU to Xerxes for alias '%s'",
			  m->alias);
	  xiaLogError("xia__DoMMUConfig", info_string, XIA_XERXES);
	  return XIA_XERXES;
	}
  }

  return XIA_SUCCESS;
}


/** @brief Checks for the presence of a system FPGA and adds it to the
 * configuration.
 *
 */
HANDEL_STATIC int xia__DoSystemFPGA(Module *m)
{
  int status;
  int detChan = 0;

  unsigned int i;

  char detType[MAXITEM_LEN];
  char sysFPGAName[MAX_PATH_LEN];
  char rawName[MAXFILENAME_LEN];

  boolean_t found = FALSE_;

  ASSERT(m != NULL);


  /* Assume that the first enabled detChan defines the correct detector type. */
  for (i = 0; i < m->number_of_channels; i++) {
	detChan = m->channels[i];

	if (detChan != -1) {
	  break;
	}
  }

  status = xia__GetDetStringFromDetChan(detChan, m, detType);

  if (status != XIA_SUCCESS) {
	sprintf(info_string, "Error getting detector type string for alias '%s'",
			m->alias);
	xiaLogError("xia__DoSystemFPGA", info_string, status);
	return status;
  }

  status = xia__GetSystemFPGAName(m, detType, sysFPGAName, rawName, &found);

  if ((status != XIA_SUCCESS) && found) {
	sprintf(info_string, "Error getting System FPGA for alias '%s'", m->alias);
	xiaLogError("xia__DoSystemFPGA", info_string, status);
	return status;
  }

  if (found) {
    for (i = 0; i < m->number_of_channels; i++) {
      strcpy(m->currentFirmware[i].currentSysFPGA, rawName);
    }

    status = xia__AddSystemFPGA(m, sysFPGAName, rawName);

    if (status != XIA_SUCCESS) {
      sprintf(info_string, "Error adding System FPGA '%s' to Xerxes",
              sysFPGAName);
      xiaLogError("xia__DoSystemFPGA", info_string, status);
      return status;
    }
  }

  return XIA_SUCCESS;
}


/** @brief Gets the appropriate detector type string for the specified detChan.
 *
 * Assumes that memory for @a type has already been allocated.
 *
 */
HANDEL_STATIC int xia__GetDetStringFromDetChan(int detChan, Module *m,
											   char *type)
{
  int modChan;
  
  char *detAlias = NULL;
  
  Detector *det = NULL;



  /* If a disabled channel is passed in, we just use module channel 0. */
  if (detChan != -1) {
	modChan  = xiaGetModChan((unsigned int)detChan);
  } else {
	modChan  = 0;
  }

  detAlias = m->detector[modChan];
  det      = xiaFindDetector(detAlias);

  ASSERT(det != NULL);

  switch (det->type) {
  case XIA_DET_RESET:
	strcpy(type, "RESET");
	break;

  case XIA_DET_RCFEED:
	strcpy(type, "RC");
	break;

  default:
  case XIA_DET_UNKNOWN:
	sprintf(info_string, "No detector type specified for detChan %d", detChan);
	xiaLogError("xia__GetDetStringFromDetChan", info_string, XIA_MISSING_TYPE);
	return XIA_MISSING_TYPE;
	break;
  }

  return XIA_SUCCESS;
}


/** @brief Checks for the presence of a system DSP and adds it to the
 * configuration.
 *
 */
HANDEL_STATIC int xia__DoSystemDSP(Module *m, boolean_t *found)
{
  int detChan = -1;
  int status;

  unsigned int i;

  char detType[MAXITEM_LEN];
  char sysDSPName[MAX_PATH_LEN];
  char rawName[MAXFILENAME_LEN];


  ASSERT(m != NULL);
  ASSERT(found != NULL);


  for (i = 0; i < m->number_of_channels; i++) {
	detChan = m->channels[i];
	
	if (detChan != -1) {
	  break;
	}
  }

  status = xia__GetDetStringFromDetChan(detChan, m, detType);

  if (status != XIA_SUCCESS) {
	sprintf(info_string, "Error getting detector type string for alias '%s'",
			m->alias);
	xiaLogError("xia__DoSystemDSP", info_string, status);
	return status;
  }

  status = xia__GetSystemDSPName(m, detType, sysDSPName, rawName, found);

  if ((status != XIA_SUCCESS) && *found) {
	sprintf(info_string, "Error getting System DSP for alias '%s'", m->alias);
	xiaLogError("xia__DoSystemDSP", info_string, status);
	return status;
  }

  if (*found) {
	status = xia__AddSystemDSP(m, sysDSPName, rawName);

	if (status != XIA_SUCCESS) {
	  sprintf(info_string, "Error adding System DSP '%s' to Xerxes",
			  sysDSPName);
	  xiaLogError("xia__DoSystemDSP", info_string, status);
	  return status;
	}
  }

  return XIA_SUCCESS;
}


/** @brief Adds the DSP info for each channel to the Xerxes configuration.
 *
 */
HANDEL_STATIC int xia__DoDSP(Module *m)
{
  int status;
  int statusX;
  int detChan;

  unsigned int i;

  double pt = 0.0;

  char detType[MAXITEM_LEN];
  char dspName[MAX_PATH_LEN];
  char rawName[MAXFILENAME_LEN];
  char chan[MAX_CHAN_LEN];

  char *dspStr[2];


  ASSERT(m != NULL);


  /* We skip channels that are disabled (detChan = -1) */
  for (i = 0; i < m->number_of_channels; i++) {
	detChan = m->channels[i];

	if (detChan == -1) {
	  continue;
	}

	status = xia__GetDetStringFromDetChan(detChan, m, detType);

	if (status != XIA_SUCCESS) {
	  sprintf(info_string, "Error getting detector type string for alias '%s'",
			  m->alias);
	  xiaLogError("xia__DoDSP", info_string, status);
	  return status;
	}

	pt = xiaGetValueFromDefaults("peaking_time", m->defaults[i]);

	status = xia__GetDSPName(m, i, pt, dspName, detType, rawName);

	if (status != XIA_SUCCESS) {
	  sprintf(info_string, "Error getting DSP name for alias '%s'", m->alias);
	  xiaLogError("xia__DoDSP", info_string, status);
	  return status;
	}

	/* Update the current firmware cache */
	strcpy(m->currentFirmware[i].currentDSP, rawName);

	dspStr[0] = &(chan[0]);
	dspStr[1] = &(dspName[0]);

	sprintf(dspStr[0], "%d", i);

	statusX = dxp_add_board_item("dsp", (char **)dspStr);

	if (statusX != DXP_SUCCESS) {
	  sprintf(info_string, "Error adding 'dsp' for alias '%s'", m->alias);
	  xiaLogError("xia__DoDSP", info_string, XIA_XERXES);
	  return XIA_XERXES;
	}
  }

  return XIA_SUCCESS;
}


/** @brief Checks for the presence of FiPPI A and adds it to the configuration.
 *
 */
HANDEL_STATIC int xia__DoFiPPIA(Module *m, boolean_t *found)
{
  int status;
  int detChan = 0;

  unsigned int i;

  char detType[MAXITEM_LEN];
  char fippiAName[MAX_PATH_LEN];
  char rawName[MAXFILENAME_LEN];

  
  ASSERT(m != NULL);
  ASSERT(found != NULL);


  for (i = 0; i < m->number_of_channels; i++) {
	detChan = m->channels[i];

	if (detChan != -1) {
	  break;
	}
  }

  status = xia__GetDetStringFromDetChan(detChan, m, detType);

  if (status != XIA_SUCCESS) {
	sprintf(info_string, "Error getting detector type string for alias '%s'",
			m->alias);
	xiaLogError("xia__DoFiPPIA", info_string, status);
	return status;
  }

  status = xia__GetFiPPIAName(m, detType, fippiAName, rawName, found);

  if ((status != XIA_SUCCESS) && *found) {
	sprintf(info_string, "Error getting FiPPI A for alias '%s'", m->alias);
	xiaLogError("xia__DoFiPPIA", info_string, status);
	return status;
  }

  /* Set FiPPIA as the FiPPI for all of the channels */
  for (i = 0; i < m->number_of_channels; i++) {
	strcpy(m->currentFirmware[i].currentFiPPI, rawName);
  }

  if (*found) {
	status = xia__AddFiPPIA(m, fippiAName, rawName);
	
	if (status != XIA_SUCCESS) {
	  sprintf(info_string, "Error adding FiPPI A '%s' to Xerxes for alias '%s'",
			  fippiAName, m->alias);
	  xiaLogError("xia__DoFiPPIA", info_string, status);
	  return status;
	}
  }

  return XIA_SUCCESS;
}


/** @brief Adds the FiPPI data for each enabled channel to the Xerxes
 * configuration.
 *
 */
HANDEL_STATIC int xia__DoFiPPI(Module *m)
{
  int status;
  int statusX;
  int detChan;

  unsigned int i;

  double pt = 0.0;

  char detType[MAXITEM_LEN];
  char fippiName[MAX_PATH_LEN];
  char rawName[MAXFILENAME_LEN];
  char chan[MAX_CHAN_LEN];

  char *fippiStr[2];


  ASSERT(m != NULL);


  /* We skip channels that are disabled (detChan = -1) */
  for (i = 0; i < m->number_of_channels; i++) {
	detChan = m->channels[i];

	if (detChan == -1) {
	  continue;
	}

	status = xia__GetDetStringFromDetChan(detChan, m, detType);

	if (status != XIA_SUCCESS) {
	  sprintf(info_string, "Error getting detector type string for alias '%s'",
			  m->alias);
	  xiaLogError("xia__DoFiPPI", info_string, status);
	  return status;
	}

	pt = xiaGetValueFromDefaults("peaking_time", m->defaults[i]);

	status = xia__GetFiPPIName(m, i, pt, fippiName, detType, rawName);

	if (status != XIA_SUCCESS) {
	  sprintf(info_string, "Error getting FiPPI name for alias '%s'", m->alias);
	  xiaLogError("xia__DoFiPPI", info_string, status);
	  return status;
	}

	/* Update the current firmware cache */
	strcpy(m->currentFirmware[i].currentFiPPI, rawName);

	fippiStr[0] = &(chan[0]);
	fippiStr[1] = &(fippiName[0]);

	sprintf(fippiStr[0], "%d", i);

	statusX = dxp_add_board_item("fippi", (char **)fippiStr);

	if (statusX != DXP_SUCCESS) {
	  sprintf(info_string, "Error adding 'fippi' for alias '%s'", m->alias);
	  xiaLogError("xia__DoFiPPI", info_string, XIA_XERXES);
	  return XIA_XERXES;
	}
  }

  return XIA_SUCCESS;
}


/** @brief Adds the params configuration to Xerxes.
 *
 */
HANDEL_STATIC int xia__AddXerxesParams(Module *m)
{
  int statusX;


  ASSERT(m != NULL);


  statusX = dxp_add_board_item("default_param", (char **)SYS_NULL);

  if (statusX != DXP_SUCCESS) {
	sprintf(info_string, "Error adding 'default_param' to Xerxes for alias '%s'",
			m->alias);
	xiaLogError("xiaBuildXerxesConfig", info_string, XIA_XERXES);
	return XIA_XERXES;
  }
  
  return XIA_SUCCESS;
}


/**
 * @brief Checks for the presence of a System FiPPI and adds it to the
 * configuration.
 */
HANDEL_STATIC int xia__DoSystemFiPPI(Module *m, boolean_t *found)
{
  int detChan=0;
  int status;

  unsigned int i;

  char detType[MAXITEM_LEN];
  char sysFipName[MAX_PATH_LEN];
  char rawName[MAXFILENAME_LEN];


  ASSERT(m != NULL);


  *found = FALSE_;

  /* Find the first enabled channel and use that to get the detector type. */
  for (i = 0; i < m->number_of_channels; i++) {
    detChan = m->channels[i];

    if (detChan != -1) {
      break;
    }
  }

  status = xia__GetDetStringFromDetChan(detChan, m, detType);

  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error getting detector type string for alias '%s'",
            m->alias);
    xiaLogError("xia__DoSystemFiPPI", info_string, status);
    return status;
  }

  status = xia__GetSystemFiPPIName(m, detType, sysFipName, rawName, found);

  if ((status != XIA_SUCCESS) && *found) {
    sprintf(info_string, "Error getting System FiPPI for alias '%s'",
            m->alias);
    xiaLogError("xia__DoSystemFiPPI", info_string, status);
    return status;
  }

  if (*found) {
    for (i = 0; i < m->number_of_channels; i++) {
      strcpy(m->currentFirmware[i].currentSysFiPPI, rawName);
    }
    
    status = xia__AddSystemFiPPI(m, sysFipName, rawName);

    if (status != XIA_SUCCESS) {
      sprintf(info_string, "Error adding System FiPPI '%s' to Xerxes",
              sysFipName);
      xiaLogError("xia__DoSystemFiPPI", info_string, status);
      return status;
    }
  }

  return XIA_SUCCESS;
}


/**
 * @brief Retrieves the System FiPPI name from the firmware, if it exists.
 *
 * @a found is only set to TRUE if a System FiPPI is defined in the FDD file. If
 * the module does not use an FDD file, FALSE will be returned.
 *
 * System FiPPIs are used on hardware that only have a single FPGA and no
 * DSP. We use a fake peaking time since the System FiPPI will be defined over
 * the entire peaking time range of the product.
 */
HANDEL_STATIC int xia__GetSystemFiPPIName(Module *m, char *detType,
                                          char *sysFipName, char *rawFilename,
                                          boolean_t *found)
{
  int status;

  double fakePT = 1.0;

  char *tmpPath = NULL;

  FirmwareSet *fs = NULL;


  ASSERT(m != NULL);
  ASSERT(detType != NULL);
  ASSERT(sysFipName != NULL);
  ASSERT(rawFilename != NULL);
  ASSERT(found != NULL);
  ASSERT(m->firmware[0] != NULL);


  *found = FALSE_;

  fs = xiaFindFirmware(m->firmware[0]);
  ASSERT(fs != NULL);

  if (fs->filename == NULL) {
    sprintf(info_string, "No FDD defined in the firmware set '%s'",
            m->firmware[0]);
    xiaLogError("xia__GetSystemFiPPIName", info_string, XIA_NO_FDD);
    return XIA_NO_FDD;
  }

  if (fs->tmpPath != NULL) {
    tmpPath = fs->tmpPath;
  } else {
    tmpPath = utils->funcs->dxp_md_tmp_path();
  }

  status = xiaFddGetFirmware(fs->filename, tmpPath, "system_fippi", fakePT, 0,
                             NULL, detType, sysFipName, rawFilename);

  /* This is not necessarily an error. We will still pass the error value
   * up to the top-level but only use an informational message. For products
   * without system_fippis defined in their FDD files this message will always
   * appear and we don't want them to be confused by spurious ERRORs.
   */
  if (status == XIA_FILEERR) {
    sprintf(info_string, "Unable to locate system_fippi in '%s'",
            fs->filename);
    xiaLogInfo("xia__GetSystemFiPPIName", info_string);
    return status;
  }
  
  /* These are "real" errors, not just missing file problems. */
  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error finding system_fippi in '%s'",
            fs->filename);
    xiaLogError("xia__GetSystemFiPPIName", info_string, status);
    return status;
  }

  *found = TRUE_;
  return XIA_SUCCESS;
}


/**
 * @brief Adds a System FiPPI to the Xerxes configuration.
 */
HANDEL_STATIC int xia__AddSystemFiPPI(Module *m, char *sysFipName,
                                      char *rawFilename)
{
  int statusX;
  
  /* Xerxes requires items as lists of strings. */
  char *sysFipStr[1];


  ASSERT(m != NULL);
  ASSERT(sysFipName != NULL);
  ASSERT(rawFilename != NULL);


  sysFipStr[0] = handel_md_alloc(strlen(sysFipName) + 1);

  if (sysFipStr[0] == NULL) {
    sprintf(info_string, "Unable to allocated %d bytes for 'sysFipStr[0]'",
            strlen(sysFipName) + 1);
    xiaLogError("xia__AddSystemFiPPI", info_string, XIA_NOMEM);
    return XIA_NOMEM;
  }

  strcpy(sysFipStr[0], sysFipName);

  statusX = dxp_add_board_item("system_fippi", (char **)sysFipStr);

  handel_md_free(sysFipStr[0]);

  if (statusX != DXP_SUCCESS) {
    sprintf(info_string, "Error adding 'system_fippi', '%s', board item to "
            "Xerxes configuration",
            sysFipName);
    xiaLogError("xia__AddSystemFiPPI", info_string, XIA_XERXES);
    return XIA_XERXES;
  }

  return XIA_SUCCESS;
}
