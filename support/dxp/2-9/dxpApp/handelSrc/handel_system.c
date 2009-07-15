
/*
 * handel_system.c
 *
 * Created 10/12/01 -- PJF
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


#include "xia_handel.h"
#include "xia_system.h"
#include "xia_assert.h"
#include "xia_common.h"

#include "handel_errors.h"

#include "psl.h"
#include "fdd.h"

#include "xerxes.h"
#include "xerxes_errors.h"


/* Constants */
#define MAX_TYPE_LEN  32


/* Static routines */
HANDEL_STATIC int HANDEL_API xiaValidateFirmwareSets(void);
HANDEL_STATIC int HANDEL_API xiaValidateDetector(void);
HANDEL_STATIC int HANDEL_API xiaValidateModule(PSLFuncs *funcs, unsigned int detChan);
HANDEL_STATIC int HANDEL_API xiaValidateDetChan(DetChanElement *current);
HANDEL_STATIC int HANDEL_API xiaValidateDetSet(DetChanElement *head);

HANDEL_STATIC boolean_t HANDEL_API xiaIsFDFvsFirmValid(FirmwareSet *fSet);
HANDEL_STATIC boolean_t HANDEL_API xiaArePTRsValid(Firmware **firmware);
HANDEL_STATIC boolean_t HANDEL_API xiaAreFiPPIAndDSPValid(Firmware *firmware);
HANDEL_STATIC boolean_t HANDEL_API xiaArePolaritiesValid(Detector *detector);
HANDEL_STATIC boolean_t HANDEL_API xiaAreGainsValid(Detector *detector);
HANDEL_STATIC boolean_t HANDEL_API xiaIsTypeValid(Detector *detector);

HANDEL_STATIC int HANDEL_API _parseMemoryName(char *name, char *type, boolean_t *isRead, unsigned long *addr,
											  unsigned long *len);



/*****************************************************************************
 *
 * This routine does the following:
 * 1) Validates the information in HanDeL's data structures
 * 2) Builds XerXes data structures from it's own
 * 3) Downloads firmware to specified detChans
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaStartSystem(void)
{
    int status;

    DetChanElement *current = NULL;


    /* Validate system-wide LLs */
    status = xiaValidateFirmwareSets();
    if (status != XIA_SUCCESS)
    {
	xiaLogError("xiaStartSystem", "Error validating FirmwareSet data", status);
	return status;
    }

    status = xiaValidateDetector();
    if (status != XIA_SUCCESS)
    {
	xiaLogError("xiaStartSystem", "Error validating Detector data", status);
	return status;
    }

    /* Now we have to start dealing with stuff specific to the individual detChans */

    current = xiaGetDetChanHead();

    /* Add this check since having the detChanHead == NULL bypasses the while loop
     * and convinces the system that it needs to try and download firmware to 
     * a non-existant system. I suspect that this problem is due to the fact that
     * XerXes isn't as careful about checking parameters. 
     */
    if (current == NULL)
    {
	status = XIA_NO_DETCHANS;
	xiaLogError("xiaStartSystem", "No detChans are defined", status);
	return status;
    }


    while (current != NULL)
    {
		
	switch (xiaGetElemType(current->detChan))
	{
	  case SET:
	    xiaClearTags();
	    status = xiaValidateDetSet(current);
	    break;

	  case SINGLE:
	    status = xiaValidateDetChan(current);
	    break;

	  case 999:
		status = XIA_INVALID_DETCHAN;
		xiaLogError("xiaStartSystem", "detChan number is not in the list of valid values ", status);
		return status;
		break;
	  default:
	    status = XIA_UNKNOWN;
	    break;
	}

	if (status != XIA_SUCCESS)
	{
	    sprintf(info_string, "Error validating detChan %u", current->detChan);
	    xiaLogError("xiaStartSystem", info_string, status);
	    return status;
	}

	current = getListNext(current);
    }

    /* [with baited breath] Now everything should be verified so that we can talk to 
     * XerXes.
     */
    status = xiaBuildXerxesConfig();

    if (status != XIA_SUCCESS)
    {
	sprintf(info_string, "Error building Xerxes configuration");
	xiaLogError("xiaStartSystem", info_string, status);
	return status;
    }

    status = xiaUserSetup();

    if (status != XIA_SUCCESS)
    {
	xiaLogError("xiaStartSystem", "Error downloading firmware", status);
	return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine downloads the firmware specified by type to the detChan. NOTE:
 * -1 is not an acceptable argument for detChan here. The whole reason we 
 * invented the DetChanSets is to avoid the -1 issue.
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaDownloadFirmware(int detChan, char *type)
{
    int status;
    int elemType;

    unsigned int modChan;

    double peakingTime;

    char boardType[MAXITEM_LEN];
    char fileName[MAX_PATH_LEN];
    char rawFilename[MAXFILENAME_LEN];
    char detType[MAXITEM_LEN];

    char *alias;
    char *firmAlias;
    char *defAlias;
    char *tmpPath = NULL;

    FirmwareSet *firmwareSet = NULL;

    Module *module = NULL;

    Detector *detector = NULL;

    DetChanElement *detChanElem = NULL;
	
    DetChanSetElem *detChanSetElem = NULL;

    CurrentFirmware *currentFirmware = NULL;

    XiaDefaults *defs = NULL;

    PSLFuncs localFuncs;


    elemType = xiaGetElemType((unsigned int)detChan);

    switch (elemType)
	  {
      case SINGLE:
		alias  = xiaGetAliasFromDetChan(detChan);
		/* Might want to check for NULL here but should be okay since I believe
		 * that xiaGetElemType returns an error value if the detChan doesn't 
		 * exist.
		 */
		module  			= xiaFindModule(alias);
		modChan 			= xiaGetModChan((unsigned int)detChan);
		detector            = xiaFindDetector(module->detector[modChan]);
		firmAlias 			= module->firmware[modChan];
		defAlias  			= module->defaults[modChan];
		
		currentFirmware 	= &module->currentFirmware[modChan]; 
		
		peakingTime = xiaGetValueFromDefaults("peaking_time", defAlias);
		
    defs = xiaGetDefaultFromDetChan(detChan);
    ASSERT(defs != NULL);

		firmwareSet = xiaFindFirmware(firmAlias);
		
		if (firmwareSet->filename == NULL) 
		  {
			status = xiaGetValueFromFirmware(firmAlias, peakingTime, type, fileName);
			if (status != XIA_SUCCESS) 
			  {
				sprintf(info_string, "Error getting %s from %s", type, firmAlias);
				xiaLogError("xiaDownloadFirmware", info_string, status);
				return status;
			  }
			
			/* For now use the filename as the rawFilename for the non-FDD case. 
			 * I don't really think that it is a problem since the currentFirm.
			 * struct just wants a unique ID of some sort and the filenames in
			 * the Firmware struct are unique enough. (It's just that they 
			 * aren't in the case of the FDD since the FDD DLL spits out files
			 * with basically the same name.
			 */
			strcpy(rawFilename, fileName);
			
		  } else {
			
			switch (detector->type) 
			  {
			  case XIA_DET_RESET:
				strcpy(detType, "RESET");
				break;
				
			  case XIA_DET_RCFEED:
				strcpy(detType, "RC_FEEDBACK");
				break;
				
			  default:
				status = XIA_UNKNOWN;
				xiaLogError("xiaDownloadFirmware", "Should not be seeing this message", status);
				return status;
				break;
			  }
			
      if (firmwareSet->tmpPath) {
        tmpPath = firmwareSet->tmpPath;
      } else {
        tmpPath = utils->funcs->dxp_md_tmp_path();
      }

	    /* Use the FDD here */
			status = xiaFddGetFirmware(firmwareSet->filename, tmpPath, type,
                                 peakingTime, firmwareSet->numKeywords,
                                 firmwareSet->keywords, detType, fileName,
                                 rawFilename);

			if (status != XIA_SUCCESS) 
			  {
				xiaLogError("xiaDownloadFirmware", "Error getting firmware from FDD", status);
				return status;
			  }
			
		  }		  
		
		status = xiaGetBoardType(detChan, boardType);
		
		if (status != XIA_SUCCESS)
		  {
			sprintf(info_string, "Unable to get boardType for detChan %d", detChan);
			xiaLogError("xiaDownloadFirmware", info_string, status);
			return status;
		  }
		
		status = xiaLoadPSL(boardType, &localFuncs);
		
		if (status != XIA_SUCCESS)
		  {
			sprintf(info_string, "Unable to load PSL functions for boardType %s", boardType);
			xiaLogError("xiaDownloadFirmware", info_string, status);
			return status;
		  }
		
		status = localFuncs.downloadFirmware(detChan, type, fileName, module,
                                         rawFilename, defs);
		
		if (status != XIA_SUCCESS)
		  {
			sprintf(info_string, "Unable to download Firmware for detChan %d", detChan);
			xiaLogError("xiaDownloadFirmware", info_string, status);
			return status;
		  }
		
		/* Sync up the current firmware structure here */
		if (STREQ(type, "fippi")) {
          strcpy(module->currentFirmware[modChan].currentFiPPI, rawFilename);

        } else if (STREQ(type, "dsp")) {
          strcpy(module->currentFirmware[modChan].currentDSP, rawFilename);

        } else if (STREQ(type, "user_fippi")) {
          strcpy(module->currentFirmware[modChan].currentUserFiPPI, rawFilename);

        } else if (STREQ(type, "user_dsp")) {
          strcpy(module->currentFirmware[modChan].currentUserDSP, rawFilename);

        } else if (STREQ(type, "system_fpga")) {
          strcpy(module->currentFirmware[modChan].currentSysFPGA, rawFilename);
        }
		break;
		
      case SET:
		/* We've already verified that there are no infinte loops in the detChan sets 
		 * by this point, so we don't need to check the isTagged field
		 */
		detChanElem = xiaGetDetChanPtr((unsigned int)detChan);
		
		/* We know that it is a SET... */
		detChanSetElem = detChanElem->data.detChanSet;
		
		while (detChanSetElem != NULL)
		  {
			status = xiaDownloadFirmware((int)detChanSetElem->channel, type);
			
			if (status != XIA_SUCCESS)
			  {
				sprintf(info_string, "Error downloading firmware to detChan %u", detChanSetElem->channel);
				xiaLogError("xiaDownloadFirmware", info_string, status);
				return status;
			  }
			
			detChanSetElem = getListNext(detChanSetElem);
		  }
		
		break;
		
		
	  case 999:
		status = XIA_INVALID_DETCHAN;
		xiaLogError("xiaDownloadFirmware", "detChan number is not in the list of valid values ", status);
		return status;
		break;
      default:
		/* Better not get here */
		status = XIA_UNKNOWN;
		xiaLogError("xiaDownloadFirmware", "Should not be seeing this message", XIA_UNKNOWN);
		return status;
		break;
	  }
	
    return XIA_SUCCESS;
}

/*****************************************************************************
 * 
 * This routine loops over all of the elements of the FirmwareSets LL and 
 * checks that the data is valid. In the case of a misconfiguration, the 
 * returned error code should indicate what part of the FirmwareSet is 
 * invalid.
 *
 * The current logic is as follows:
 * 1) FirmwareSet must either define an FDF file OR a "Firmware" (LL)
 * 
 * For the Firmware Elements within the FirmwareSet:
 * 1) The peaking time ranges may not overlap between different PTRRs. This 
 *    creates an unacceptably ambiguous situation about which PTRR to use
 *    as the firmware definition for a given peaking time in the overlapped
 *    range.
 * 2) A (FiPPI OR User FiPPI) AND DSP must be defined for each element. 
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaValidateFirmwareSets(void)
{
    int status;

    FirmwareSet *current = NULL;

    current = xiaGetFirmwareSetHead();

    while (current != NULL)
    {
	if (!xiaIsFDFvsFirmValid(current))
	{
	    status = XIA_FIRM_BOTH;
	    sprintf(info_string, "Firmware alias %s contains both an FDF and Firmware definitions", current->alias);
	    xiaLogError("xiaValidateFirmwareSets", info_string, status);
	    return status;
	}

	/* If we only have an FDF file then we don't need to do anything else */
	if (current->filename != NULL)
	{
	    return XIA_SUCCESS;
	}

	if (!xiaArePTRsValid(&(current->firmware)))
	{
	    status = XIA_PTR_OVERLAP;
	    sprintf(info_string, "Firmware definitions in alias %s have overlapping peaking times", current->alias);
	    xiaLogError("xiaVAlidateFirmwareSets", info_string, status);
	    return status;
	}

	if (!xiaAreFiPPIAndDSPValid(current->firmware))
	{
	    status = XIA_MISSING_FIRM;
	    sprintf(info_string, "Firmware definition(s) in alias %s is/are missing FiPPI and DSP files", current->alias);
	    xiaLogError("xiaValidateFirmwareSets", info_string, status);
	    return status;
	}

	current = getListNext(current);
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine loops over all of the elements of the Detector LL and checks 
 * that the data is valid. In the case of misconfiguration, the returned error
 * code should indicate what part of the Detector is invalid.
 *
 * The current logic is as follows:
 * 1) Check that all polarities are valid from 0...nchan - 1
 * 2) Check that all gains are within a valid range from 0...nchan - 1
 * 3) Check that the type is defined beyond XIA_DET_UNKNOWN
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaValidateDetector(void)
{
    int status;

    Detector *current = NULL;

    current = xiaGetDetectorHead();
	
    while (current != NULL)
    {
	if (!xiaArePolaritiesValid(current))
	{
	    status = XIA_MISSING_POL;
	    sprintf(info_string, "Missing polarity in alias %s\n", current->alias);
	    xiaLogError("xiaValidateDetector", info_string, status);
	    return status;
	}

	if (!xiaAreGainsValid(current))
	{
	    status = XIA_MISSING_GAIN;
	    sprintf(info_string, "Missing gain in alias %s\n", current->alias);
	    xiaLogError("xiaValidateDetector", info_string, status);
	    return status;
	}

	if (!xiaIsTypeValid(current))
	{
	    status = XIA_MISSING_TYPE;
	    sprintf(info_string, "Missing type in alias %s\n", current->alias);
	    xiaLogError("xiaValidateDetector", info_string, status);
	    return status;
	}

	current = getListNext(current);
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine uses the product specific pointer to validate the module 
 * data for the specified detChan. Basically, this routine just passes the
 * module information straight to the PSL layer since there is no point in 
 * doing any "partial" verification here.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaValidateModule(PSLFuncs *funcs, unsigned int detChan)
{
    int status;

    Module *current = NULL;

    XiaDefaults *defaults = NULL;

    current = xiaFindModule(xiaGetAliasFromDetChan(detChan));

    /* !!DONT'T USE YET
       if (current->isValidated)
       {
       return XIA_SUCCESS;
       }
    */
    status = funcs->validateModule(current);

    if (status != XIA_SUCCESS)
    {
	sprintf(info_string, "Error validating module");
	xiaLogError("xiaValidateModule", info_string, status);
	return status;
    }
		
    defaults = xiaGetDefaultFromDetChan(detChan);

    status = funcs->validateDefaults(defaults);

    if (status != XIA_SUCCESS)
    {
	sprintf(info_string, "Error validating defaults for module");
	xiaLogError("xiaValidateModule", info_string, status);
	return status;
    }

    /* !!DON'T USE YET
       current->isValidated = TRUE_;
    */

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine contains the logic to check if and FDF and Firmware 
 * definitions are defined in the FirmwareSet fSet.
 *
 *****************************************************************************/
HANDEL_STATIC boolean_t HANDEL_API xiaIsFDFvsFirmValid(FirmwareSet *fSet)
{
    if ((fSet->filename != NULL) &&
	(fSet->firmware != NULL))
    {
	return FALSE_;
    }

    if ((fSet->filename == NULL) &&
	(fSet->firmware == NULL))
    {
	return FALSE_;
    }

    return TRUE_;
}


/*****************************************************************************
 *
 * This routine sorts the Firmware LL by min peaking time (which should
 * already be verified at the insertion point) and then checks the peaking
 * times for overlap. Assumes the firmware is non-NULL.
 *
 *****************************************************************************/
HANDEL_STATIC boolean_t HANDEL_API xiaArePTRsValid(Firmware **firmware)
{
    Firmware *current = NULL;
    Firmware *lookAhead = NULL;

    if (xiaInsertSort(firmware, xiaFirmComp) < 0)
    {
	return FALSE_;
    }

    current = *firmware;
    while (current != NULL)
    {
	/* The basic theory here is that since the Firmware LL is sorted based on 
	 * min peaking time, we can check that the max peaking time for a given element
	 * does not overlap with any of the other min peaking times that are "past" it
	 * in the list.
	 */
	lookAhead = getListNext(current);
		
	while (lookAhead != NULL)
	{
	    if (current->max_ptime > lookAhead->min_ptime)
	    {
		return FALSE_;
	    }

	    lookAhead = getListNext(lookAhead);
	}

	current = getListNext(current);
    }

    return TRUE_;
}


/*****************************************************************************
 *
 * This routine checks that a FiPPI and DSP are defined
 *
 *****************************************************************************/
HANDEL_STATIC boolean_t HANDEL_API xiaAreFiPPIAndDSPValid(Firmware *firmware)
{
    if (firmware->dsp == NULL)
    {
	return FALSE_;
    }

    if ((firmware->fippi == NULL) &&
	(firmware->user_fippi == NULL))
    {
	return FALSE_;
    }

    return TRUE_;
}


/*****************************************************************************
 *
 * This routine verifies that all of the polarities in detector have a valid
 * value. (Essentially, all that this *really* checks is that all of the
 * values in the polarity array are set to something. This is the case 
 * because when the polarity is set via a call to xiaAddDetectorItem(), the 
 * value is verified, therefore the value can only be out-of-range if it
 * hasn't been set yet.
 *
 *****************************************************************************/
HANDEL_STATIC boolean_t HANDEL_API xiaArePolaritiesValid(Detector *detector)
{
    unsigned int i;

    for (i = 0; i < detector->nchan; i++)
    {
	if ((detector->polarity[i] != 1) &&
	    (detector->polarity[i] != 0))
	{
	    return FALSE_;
	}
    }

    return TRUE_;
}


/*****************************************************************************
 *
 * This routine verifies that all of the gains in detector are within a valid
 * range. This routine has the same caveats as xiaArePolaritiesValid() in the
 * sense that the value should really only be out-of-range if it hasn't been 
 * set yet.
 *
 *****************************************************************************/
HANDEL_STATIC boolean_t HANDEL_API xiaAreGainsValid(Detector *detector)
{
    unsigned int i;

    for (i = 0; i < detector->nchan; i++)
    {
	if ((detector->gain[i] < XIA_GAIN_MIN) ||
	    (detector->gain[i] > XIA_GAIN_MAX))
	{
	    return FALSE_;
	}
    }

    return TRUE_;
}


/*****************************************************************************
 *
 * This routine verifies that the type isn't XIA_DET_UNKNOWN (since that is
 * what it is initialized to.
 *
 *****************************************************************************/
HANDEL_STATIC boolean_t HANDEL_API xiaIsTypeValid(Detector *detector)
{
    if (detector->type == XIA_DET_UNKNOWN)
    {
	return FALSE_;
    }

    return TRUE_;
}


/*****************************************************************************
 *
 * This routine validates information for a SINGLE detChan.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaValidateDetChan(DetChanElement *current)
{
    int status;

    char boardType[MAXITEM_LEN];

    PSLFuncs localFuncs;	
	
    status = xiaGetBoardType(current->detChan, boardType);
	
    if (status != XIA_SUCCESS)
    {
	xiaLogError("xiaValidateDetChan", "Error getting board type for specified detChan", status);
	return status;
    }

    status = xiaLoadPSL(boardType, &localFuncs);

    if (status != XIA_SUCCESS)
    {
	xiaLogError("xiaValidateDetChan", "Error loading PSL functions", status);
	return status;
    }

    status = xiaValidateModule(&localFuncs, current->detChan);
	
    if (status != XIA_SUCCESS)
    {
	sprintf(info_string, "Error validating Module data for detChan %u", current->detChan);
	xiaLogError("xiaValidateDetChan", info_string, status);
	return status;
    }

    return XIA_SUCCESS;

}


/*****************************************************************************
 *
 * This routine checks a detChanSet for infinite loops. Assumes that head is
 * a set.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaValidateDetSet(DetChanElement *head)
{
    int status = XIA_SUCCESS;
	
    DetChanElement *current = NULL;
	
    DetChanSetElem *element = NULL;

    /* We only want to tag detChans that are sets since multiple sets can
     * include references to SINGLE detChans.
     */
    head->isTagged = TRUE_;

    element = head->data.detChanSet;

    while (element != NULL)
    {
	current = xiaGetDetChanPtr(element->channel);

	switch (current->type)
	{
	  case SINGLE:
	    status = XIA_SUCCESS;
	    break;

	  case SET:
	    if (current->isTagged)
	    {
		status = XIA_INFINITE_LOOP;
		sprintf(info_string, "Infinite loop detected involving detChan %u", current->detChan);
		xiaLogError("xiaValidateDetSet", info_string, status);
		return status;
	    }
	    status = xiaValidateDetSet(current);
	    break;

	  default:
	    status = XIA_UNKNOWN;
	    break;
	}

	if (status != XIA_SUCCESS)
	{
	    sprintf(info_string, "Error validating detChans");
	    xiaLogError("xiaValidateDetSet", info_string, status);
	    return status;
	}

	element = getListNext(element);
    }

    return status;
}


/*****************************************************************************
 *
 * This routine initializes funcs to be of the proper type.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaLoadPSL(char *boardType, PSLFuncs *funcs)
{
    int status;


	/* XXX TODO Use a list of function pointers
	 * to call these functions.
	 */

	/* We need this so that we can use the
	 * #ifndefs to conditionally remove
	 * some of the supported board types.
	 */
	if (FALSE_) {

#ifndef EXCLUDE_DXPX10P
    } else if (STREQ(boardType, "dxpx10p")) {

	status = dxpx10p_PSLInit(funcs);
#endif /* EXCLUDE_DXPX10P */   

#ifndef EXCLUDE_DXP4C2X
    } else if (STREQ(boardType, "dxp4c2x") ||
	       STREQ(boardType, "dxp2x4c") ||
	       STREQ(boardType, "dxp2x")) {

	status = dxp4c2x_PSLInit(funcs);
#endif /* EXCLUDE_DXP4C2X */

#ifndef EXCLUDE_UDXPS
    } else if (STREQ(boardType, "udxps")) {

	status = udxps_PSLInit(funcs);
#endif /* EXCLUDE_UDXPS */

#ifndef EXCLUDE_UDXP	
    } else if (STREQ(boardType, "udxp")) {
      
	status = udxp_PSLInit(funcs);
#endif /* EXCLUDE_UDXP */   

#ifndef EXCLUDE_XMAP
	} else if (STREQ(boardType, "xmap")) {
	  
	  status = xmap_PSLInit(funcs);
#endif /* EXCLUDE_XMAP */

#ifndef EXCLUDE_MERCURY
  } else if (STREQ(boardType, "mercury")) {

    status = mercury_PSLInit(funcs);
#endif /* EXCLUDE_MERCURY */

#ifndef EXCLUDE_VEGA
  } else if (STREQ(boardType, "vega")) {

    status = vega_PSLInit(funcs);
#endif /* EXCLUDE_VEGA */

    } else {
	      
	funcs = NULL;
	status = XIA_UNKNOWN_BOARD;
	      
    }

	if (status == XIA_UNKNOWN_BOARD) {
	  sprintf(info_string, "Board type '%s' is not supported in this version of the library",
			  boardType);
	  xiaLogError("xiaLoadPSL", info_string, status);
	  return status;

	} else if (status != XIA_SUCCESS) {
	  xiaLogError("xiaLoadPSL", "Error initializing PSL functions", status);
	  return status;
    }		
		
    return XIA_SUCCESS;
}
	

/**********
 * Performs non-persistent operations on the board. Mostly
 * used with the microDXP.
 **********/
HANDEL_EXPORT int HANDEL_API xiaBoardOperation(int detChan, char *name, void *value)
{
    int status;
    int elemType;

    char boardType[MAXITEM_LEN];

	XiaDefaults *defs = NULL;

    PSLFuncs localFuncs;


	if (name == NULL) {
	  xiaLogError("xiaBoardOperation", "'name' can not be NULL", XIA_NULL_NAME);
	  return XIA_NULL_NAME;
	}

	if (value == NULL) {
	  xiaLogError("xiaBoardOperation", "'value' can not be NULL", XIA_NULL_VALUE);
	  return XIA_NULL_VALUE;
	}

    elemType = xiaGetElemType((unsigned int)detChan);

    switch (elemType)
	  {
      case SINGLE:
		status = xiaGetBoardType(detChan, boardType);
		if (status != XIA_SUCCESS)
		  {
			sprintf(info_string, "Unable to get boardType for detChan %d", detChan);
			xiaLogError("xiaBoardOperation", info_string, status);
			return status;
		  }
		
		defs = xiaGetDefaultFromDetChan(detChan);

		if (!defs) {
		  sprintf(info_string, "Error getting defaults for detChan %d", detChan);
		  xiaLogError("xiaBoardOperation", info_string, XIA_BAD_CHANNEL);
		  return XIA_BAD_CHANNEL;
		}

		status = xiaLoadPSL(boardType, &localFuncs);
		if (status != XIA_SUCCESS)
		  {
			sprintf(info_string, "Unable to load PSL functions for boardType %s", boardType);
			xiaLogError("xiaBoardOperation", info_string, status);
			return status;
		  }
		
		status = localFuncs.boardOperation(detChan, name, value, defs);
		if (status != XIA_SUCCESS)
		  {
			sprintf(info_string, 
					"Unable to do board operation (%s) for detChan %d", 
					name, detChan);
			xiaLogError("xiaBoardOperation", info_string, status);
			return status;
		  }
		
		break;
		
		
      case SET:
		status = XIA_BAD_TYPE;
		xiaLogError("xiaBoardOperation",
					"This routine only supports single detChans",
					status);
		return status;
		break;
		
	  case 999:
		status = XIA_INVALID_DETCHAN;
		xiaLogError("xiaBoardOperation", "detChan number is not in the list of valid values ", status);
		return status;
		break;
      default:
		/* Better not get here */
		status = XIA_UNKNOWN;
		xiaLogError("xiaBoardOperation", "Should not be seeing this message", XIA_UNKNOWN);
		return status;
		break;
	  }
	
    return XIA_SUCCESS;
}


/** @brief Performs a raw memory operation on a module.
 *
 * This is an advanced routine that does some simple parsing
 * of the name and then passes the result down to Xerxes
 * directly.
 *
 * Name format: "[type]:[r|w]:[addr]:[len]"
 *
 */
HANDEL_EXPORT int HANDEL_API xiaMemoryOperation(int detChan, char *name, void *value)
{
  int statusX;
  int status;

  char *nameX = NULL;
  
  char type[MAX_TYPE_LEN];
  
  boolean_t isRead = FALSE_;

  unsigned long addr = 0x00000000;
  unsigned long len  = 0x00000000;

  
  if (name == NULL) {
	xiaLogError("xiaMemoryOperation", "'name' may not be 'NULL'", XIA_NULL_NAME);
	return XIA_NULL_NAME;
  }

  if (value == NULL) {
	xiaLogError("xiaMemoryOperation", "'value' may not be 'NULL'", XIA_NULL_VALUE);
	return XIA_NULL_VALUE;
  }

  sprintf(info_string, "memory = %s", name);
  xiaLogDebug("xiaMemoryOperation", info_string);

  status = _parseMemoryName(name, type, &isRead, &addr, &len);

  if (status != XIA_SUCCESS) {
	xiaLogError("xiaMemoryOperation", "Error parsing memory name", status);
	return status;
  }

  sprintf(info_string, "type = '%s', isRead = '%u', addr = '%lx', len = '%lu'",
		  type, isRead, addr, len);
  xiaLogDebug("xiaMemoryOperation", info_string);

  /* The choice of malloc size may seem unusual, but we can easily promise that
   * the length of the Handel 'name' string is longer then the Xerxes equivalant
   * since the Xerxes string is subset of the Handel string.
   */
  nameX = (char *)handel_md_alloc(strlen(name) + 1);

  if (nameX == NULL) {
	xiaLogError("xiaMemoryOperation", "Out-of-memory creating 'nameX'", XIA_NOMEM);
	return XIA_NOMEM;
  }

  sprintf(nameX, "%s:%lx:%lu", type, addr, len);

  if (isRead) {
	statusX = dxp_read_memory(&detChan, nameX, (unsigned long *)value);

  } else {
	statusX = dxp_write_memory(&detChan, nameX, (unsigned long *)value);
  }

  handel_md_free(nameX);

  if (statusX != DXP_SUCCESS) {
	sprintf(info_string, "Error reading/writing memory ('%s') for detChan '%d'",
			name, detChan);
	xiaLogError("xiaMemoryOperation", info_string, XIA_XERXES);
	return XIA_XERXES;
  }

  return XIA_SUCCESS;
}


/** @brief Executes a command directly on supported hardware
 *
 * @param send The data portion of the command. The rest of the command
 * will be added to this array by Handel.
 *
 * @param recv The entire return command including any headers or checksums.
 *
 */
HANDEL_EXPORT int HANDEL_API xiaCommandOperation(int detChan, byte_t cmd,
												 unsigned int lenS, byte_t *send,
												 unsigned int lenR, byte_t *recv)
{
  int statusX;

  if (lenS > 0) {
	ASSERT(send != NULL);
  }

  if (lenR > 0) {
	ASSERT(recv != NULL);
  }

  statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, recv);

  if (statusX != DXP_SUCCESS) {
	xiaLogError("xiaCommandOperation", "Error executing command", XIA_XERXES);
	return XIA_XERXES;
  }

  return XIA_SUCCESS;
}


/** @brief Sets the priority of the IO process.
 *
 */
HANDEL_EXPORT int HANDEL_API xiaSetIOPriority(int pri)
{
  int statusX;


  statusX = dxp_set_io_priority(&pri);
  
  if (statusX != DXP_SUCCESS) {
	sprintf(info_string, "Error setting priority '%#x'", pri);
	xiaLogError("xiaSetIOPriority", info_string, XIA_XERXES);
	return XIA_XERXES;
  }

  return XIA_SUCCESS;
}


/** @brief Parses in a memory string of the format defined for xiaMemoryOperation().
 *
 */
HANDEL_STATIC int HANDEL_API _parseMemoryName(char *name, char *type, boolean_t *isRead, unsigned long *addr,
											  unsigned long *len)
{
  int n_matched = 0;

  char *n     = NULL;
  char *tok   = NULL;
  char *delim = ":";


  ASSERT(name != NULL);
  ASSERT(type != NULL);
  ASSERT(isRead != NULL);
  ASSERT(addr != NULL);
  ASSERT(len != NULL);


  n = (char *)handel_md_alloc(strlen(name) + 1);
  
  if (n == NULL) {
	xiaLogError("_parseMemoryName", "Out-of-memory trying to create 'n' string",
				XIA_NOMEM);
	return XIA_NOMEM;
  }

  strncpy(n, name, strlen(name) + 1);

  tok = strtok(n, delim);

  if (tok == NULL) {
	handel_md_free(n);
	sprintf(info_string, "'%s' is not a valid memory string: missing 'type'",
			name);
	xiaLogError("_parseMemoryName", info_string, XIA_INVALID_STR);
	return XIA_INVALID_STR;
  }

  strncpy(type, tok, strlen(tok) + 1);

  tok = strtok(NULL, delim);

  if (tok == NULL) {
	handel_md_free(n);
	sprintf(info_string, "'%s' is not a valid memory string: missing 'r/w'",
			name);
	xiaLogError("_parseMemoryName", info_string, XIA_INVALID_STR);
	return XIA_INVALID_STR;
  }  

  if (STREQ(tok, "r")) {
	*isRead = TRUE_;
  } else if (STREQ(tok, "w")) {
	*isRead = FALSE_;
  } else {
	handel_md_free(n);
	sprintf(info_string, "'%s' is not a valid r/w access specifier", tok);
	xiaLogError("_parseMemoryName", info_string, XIA_INVALID_STR);
	return XIA_INVALID_STR;
  }

  tok = strtok(NULL, delim);

  if (tok == NULL) {
	handel_md_free(n);
	sprintf(info_string, "'%s' is not a valid memory string: missing 'address'",
			name);
	xiaLogError("_parseMemoryName", info_string, XIA_INVALID_STR);
	return XIA_INVALID_STR;
  }    

  n_matched = sscanf(tok, "%lx", addr);

  if (n_matched == 0) {
	handel_md_free(n);
	sprintf(info_string, "'%s' is not a valid memory string: bad address token '%s'",
			name, tok);
	xiaLogError("_parseMemoryName", info_string, XIA_INVALID_STR);
	return XIA_INVALID_STR;
  }

  tok = strtok(NULL, delim);

  if (tok == NULL) {
	handel_md_free(n);
	sprintf(info_string, "'%s' is not a valid memory string: missing 'length'",
			name);
	xiaLogError("_parseMemoryName", info_string, XIA_INVALID_STR);
	return XIA_INVALID_STR;
  }    

  n_matched = sscanf(tok, "%lu", len);  

  if (n_matched == 0) {
	handel_md_free(n);
	sprintf(info_string, "'%s' is not a valid memory string: bad length token '%s'",
			name, tok);
	xiaLogError("_parseMemoryName", info_string, XIA_INVALID_STR);
	return XIA_INVALID_STR;
  }

  handel_md_free(n);
  return XIA_SUCCESS;
}
