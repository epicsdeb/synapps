/*
 * g200_psl.c
 *
 * Created 10/12/01 -- PJF
 *
 * Copyright(c) 2001
 * X-ray Instrumentation Associates
 * All rights reserved
 *
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>

#include "xia_module.h"
#include "xia_system.h"
#include "xia_psl.h"
#include "handel_errors.h"
#include "xia_handel_structures.h"
#include "xia_common.h"

#include "xerxes.h"
#include "xerxes_errors.h"
#include "xerxes_generic.h"

#include "g200_generic.h"

#include "fdd.h"

enum{
   PRESET_STD = 0,
      PRESET_RT,
      PRESET_LT
      };

PSL_EXPORT int PSL_API dgfg200_PSLInit(PSLFuncs *funcs);

PSL_STATIC int PSL_API pslDoPeakingTime(int detChan, void *value, FirmwareSet *firmwareSet, 
										CurrentFirmware *currentFirmware, char *detectorType, XiaDefaults *defaults);
PSL_STATIC int PSL_API pslDoGapTime(int detChan, void *value, FirmwareSet *firmwareSet, XiaDefaults *defaults);
PSL_STATIC int PSL_API pslDoTriggerPeakingTime(int detChan, void *value, XiaDefaults *defaults);
PSL_STATIC int PSL_API pslDoTriggerGapTime(int detChan, void *value, XiaDefaults *defaults);
PSL_STATIC int PSL_API pslDoTriggerThreshold(int detChan, void *value, XiaDefaults *defaults);
PSL_STATIC int PSL_API pslDoNumMCAChannels(int detChan, void *value, XiaDefaults *defaults);
PSL_STATIC int PSL_API pslDoADCPercentRule(int detChan, void *value, XiaDefaults *defaults, 
										   double gainScale, Detector *detector, int detector_chan);
PSL_STATIC int PSL_API pslDoBaselinePercentRule(int detChan, void *value, XiaDefaults *defaults, 
												double gainScale, Detector *detector, int detector_chan);
PSL_STATIC int PSL_API pslDoDynamicRange(int detChan, void *value, XiaDefaults *defaults, 
										 double gainScale, Detector *detector, int detector_chan);
PSL_STATIC int PSL_API pslDoEnableInterrupt(int detChan, void *value, XiaDefaults *defaults);
PSL_STATIC int PSL_API pslDoCalibrationEnergy(int detChan, void *value, XiaDefaults *defaults, 
											  double gainScale, Detector *detector, int detector_chan);
PSL_STATIC int PSL_API pslDoParam(int detChan, char *name, void *value, XiaDefaults *defaults);
PSL_STATIC int PSL_API pslDoSetChanCSRABit(int detChan, char *name, void *value, parameter_t bit, 
										   boolean_t isset, XiaDefaults *defaults);
PSL_STATIC int PSL_API pslDoSetModCSRABit(int detChan, char *name, void *value, parameter_t bit, 
										  boolean_t isset, XiaDefaults *defaults);
PSL_STATIC int PSL_API pslModifyCcsraBit(int detChan, boolean_t isset, parameter_t bit, XiaDefaults *defaults);
PSL_STATIC int PSL_API pslModifyMcsraBit(int detChan, boolean_t isset, parameter_t bit);
PSL_STATIC int PSL_API pslDoTraceLength(int detChan, void *value, XiaDefaults *defaults);
PSL_STATIC int PSL_API pslDoTraceDelay(int detChan, void *value, XiaDefaults *defaults);
PSL_STATIC int PSL_API pslDoRunType(int detChan, void *value, XiaDefaults *defaults);
PSL_STATIC int PSL_API pslSetRequestedTime(int detChan, double *time);
PSL_STATIC int PSL_API pslDoMultiplicityWidth(int detChan, void *value, XiaDefaults *defaults);
PSL_STATIC int PSL_API pslDoNumberBaselineAverages(int detChan, void *value, XiaDefaults *defaults);
PSL_STATIC int PSL_API pslSetMaxEvents(int detChan, XiaDefaults *defaults);
PSL_STATIC int PSL_API pslDoOffset(int detChan, void *value, XiaDefaults *defaults);
PSL_STATIC int PSL_API pslCalculateOffset(parameter_t TRACKDAC, double *offset);
PSL_STATIC int PSL_API pslDoPreampGain(int detChan, void *value, XiaDefaults *defaults, 
									   Detector *detector, int detector_chan, double gainScale);
PSL_STATIC int PSL_API pslDoDecayTime(int detChan, void *value, XiaDefaults *defaults, 
									  Detector *detector, int detector_chan);
PSL_STATIC int PSL_API pslDoDetectorPolarity(int detChan, void *value, XiaDefaults *defaults,
											 Detector *detector, int detector_chan);

PSL_STATIC int PSL_API pslGetMCALength(int detChan, void *value);
PSL_STATIC int PSL_API pslGetMCAData(int detChan, void *value);
PSL_STATIC int PSL_API pslGetListModeDataLength(int detChan, void *value);
PSL_STATIC int PSL_API pslGetListModeData(int detChan, void *value);
PSL_STATIC int PSL_API pslGetLivetime(int detChan, void *value);
PSL_STATIC int PSL_API pslGetRuntime(int detChan, void *value);
PSL_STATIC int PSL_API pslGetComptonCounts(int detChan, void *value);
PSL_STATIC int PSL_API pslGetRawComptonCounts(int detChan, void *value);
PSL_STATIC int PSL_API pslGetICR(int detChan, void *value);
PSL_STATIC int PSL_API pslGetOCR(int detChan, void *value);
PSL_STATIC int PSL_API pslGetEvents(int detChan, void *value);
PSL_STATIC int PSL_API pslGetTriggers(int detChan, void *value);
PSL_STATIC int PSL_API pslGetRunActive(int detChan, void *value);

PSL_STATIC int PSL_API pslDoADCTrace(int detChan, void *info);
PSL_STATIC int PSL_API pslGetSpecialRunDataLength(int detChan, short task, unsigned long *value);
PSL_STATIC int PSL_API pslGetSpecialData(int detChan, short task, void *data);
PSL_STATIC int PSL_API pslGetADCTrace(int detChan, void *value);
PSL_STATIC int PSL_API pslDoControlTask(int detChan, short task, void *info);
PSL_STATIC int PSL_API pslMeasureSystemGain(int detChan, void *info, XiaDefaults *defaults);
PSL_STATIC int PSL_API pslCalibrateSystemGain(int detChan, void *info, double preampGain, double gainScale, XiaDefaults *defaults);
PSL_STATIC int PSL_API pslAdjustOffsets(int detChan, void *info, XiaDefaults *defaults);
PSL_STATIC int PSL_API pslFitTrackingDACData(int detChan, void *info, short type, double coeff[2]);
PSL_STATIC int PSL_API pslPositiveDefiniteLinefit(double *data, double coeff[2]);

PSL_STATIC int PSL_API pslScalePreamp(int detChan, Detector *detector, int detector_chan, 
				      XiaDefaults *defaults, double deltaGain, double gainScale);
PSL_STATIC int PSL_API pslCalculateGain(double adcPercentRule, double calibEV, double preampGain, 
					parameter_t *GAINDAC, double gainScale, boolean_t *outOfRange);
PSL_STATIC int PSL_API pslCalculateCalibrationEnergy(double adcPercentRule, double *calibEV, 
						     double preampGain, parameter_t GAINDAC, double gainScale);
PSL_STATIC int PSL_API pslCalculatePreampGain(double adcPercentRule, double calibEV, 
					      double *preampGain, parameter_t GAINDAC, double gainScale);
PSL_STATIC int PSL_API pslCalculateDynamicRange(double systemGain, double preampGain, double gainScale, 
						XiaDefaults *defaults, double *dynamicRange);
PSL_STATIC double PSL_API pslCalculateSysGain(void);

PSL_STATIC boolean_t PSL_API pslIsUpperCase(char *name);

PSL_STATIC double PSL_API pslGetClockSpeed(int detChan);

PSL_STATIC int PSL_API pslUpdateFilter(int detChan, XiaDefaults *defaults, FirmwareSet *firmwareSet); 
PSL_STATIC int PSL_API pslUpdateTriggerFilter(int detChan, XiaDefaults *defaults); 
PSL_STATIC int PSL_API pslUpdateFIFO(int detChan, XiaDefaults *defaults); 

PSL_STATIC boolean_t PSL_API pslIsInterfaceValid(Module *module);
PSL_STATIC boolean_t PSL_API pslIsEPPAddressValid(Module *module);
PSL_STATIC boolean_t PSL_API pslIsNumChannelsValid(Module *module);
PSL_STATIC boolean_t PSL_API pslAreAllDefaultsPresent(XiaDefaults *defaults);
PSL_STATIC boolean_t PSL_API pslIsNumBinsInRange(XiaDefaults *defaults);
PSL_STATIC int PSL_API pslGetEVPerADC(XiaDefaults* defaults, double* eVPerADC);
PSL_STATIC int PSL_API pslDoEnableBaselineCut(int detChan, void* value, XiaDefaults* defaults);
PSL_STATIC int PSL_API pslDoBaselineCut(int detChan, void* value, XiaDefaults* defaults);
PSL_STATIC int PSL_API pslDoPreset(int detChan, void* value, unsigned short type, XiaDefaults* defaults);
PSL_STATIC int PSL_API pslDoResetDelay(int detChan, void* value, XiaDefaults* defaults, Detector* detector,
				       int detector_chan);


/* define the number of bits for the ADC */ 
#define ADC_BITS_MAX pow(2.0, ADC_BITS) 
#define PI              3.1415926535897932

char *defaultNames[] = { 
    "peaking_time",
    "gap_time",
    "trigger_peaking_time",
    "trigger_gap_time",
    "trigger_threshold",
    "number_mca_channels",
    "adc_percent_rule",
    "calibration_energy",
    "dynamic_range",
    "baseline_percent_rule",
    "trace_length",
    "trace_delay",
    "multiplicity_width",
    "write_level_one_buffer",
    "enable_group_trigger_input",
    "baseline_filter_length",
    "measure_individual_livetime",
    "good_channel",
    "read_always",
    "enable_trigger",
    "enable_group_trigger_output",
    "histogram_energies",
    "enable_ballistic_deficit_correction",
    "enable_gate",
    "enable_CFD",
    "enable_DNL_correction",
    "enable_interrupt",
    "run_type",
    "system_gain",
    "minimum_gain",
    "maximum_gain",
    "offset",
    "minimum_dynamic_range",
    "maximum_dynamic_range",
    "detector_polarity",
	"preamp_gain",
	"decay_time",
	"enable_baseline_cut",
	"baseline_cut",
	"preset_standard",
	"preset_runtime",
	"preset_livetime",
	"reset_delay",
	"actual_gap_time" };

double defaultValues[] = {8.0, 0.3, 0.1, 0.0, 40000.0, 
						  65536.0, 40.0, 600000.0, 1350000.0, 10.0,/* Start with Number MCA channels */
						  0.0, 0.0, 0.4, 1.0, 0.0,                 /* Start with trace_length */ 
						  8.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0,       /* Start with baseline_filter_length */
						  1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.1,       /* Start with enable_ballistic_deficit_correction */
						  0.1, 10.0, 0.0, 0.001, 100., 1.0,        /* Start with minium_gain */
						  0.005, 50., 1.0, 5.0, 0.0, 0.0,          /* Start with preamp_gain */
						  0.0, 50.0, 0.15                          /* Start with preset_livetime */
};

/*****************************************************************************
 *
 * This routine takes a PSLFuncs structure and points the function pointers
 * in it at the local g200 "versions" of the functions.
 *
 *****************************************************************************/
PSL_EXPORT int PSL_API dgfg200_PSLInit(PSLFuncs *funcs)
{
    funcs->validateDefaults     = pslValidateDefaults;
    funcs->validateModule       = pslValidateModule;
    funcs->downloadFirmware     = pslDownloadFirmware;
    funcs->setAcquisitionValues = pslSetAcquisitionValues;
    funcs->getAcquisitionValues = pslGetAcquisitionValues;
    funcs->gainOperation        = pslGainOperation;
    funcs->gainChange           = pslGainChange;
    funcs->gainCalibrate        = pslGainCalibrate;
    funcs->startRun             = pslStartRun;
    funcs->stopRun	          = pslStopRun;
    funcs->getRunData           = pslGetRunData;
    funcs->setPolarity	      = pslSetPolarity;
    funcs->doSpecialRun	      = pslDoSpecialRun;
    funcs->getSpecialRunData    = pslGetSpecialRunData;
    funcs->setDetectorTypeValue = pslSetDetectorTypeValue;
    funcs->getDefaultAlias      = pslGetDefaultAlias;
    funcs->getParameter         = pslGetParameter;
    funcs->setParameter	      = pslSetParameter;
    funcs->userSetup            = pslUserSetup;
    funcs->canRemoveName        = pslCanRemoveName;
    funcs->getNumDefaults       = pslGetNumDefaults;
    funcs->getNumParams         = pslGetNumParams;
    funcs->getParamData         = pslGetParamData;
    funcs->getParamName         = pslGetParamName;
	funcs->boardOperation   = pslBoardOperation;
	funcs->freeSCAs             = pslDestroySCAs;
	funcs->unHook           = pslUnHook;

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine validates module information specific to the dxpg200
 * product:
 *
 * 1) interface should be of type genericEPP or epp
 * 2) epp_address should be 0x278 or 0x378
 * 3) number_of_channels = 1
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslValidateModule(Module *module)
{
    int status;

    if (!pslIsInterfaceValid(module))
    {
	status = XIA_MISSING_INTERFACE;
	pslLogError("pslValidateModule", "Interface is not valid", status);
	return status;
    }

    if (!pslIsEPPAddressValid(module))
    {
	status = XIA_MISSING_ADDRESS;
	pslLogError("pslValidateModule", "Address is not valid", status);
	return status;
    }

    if (!pslIsNumChannelsValid(module))
    {
	status = XIA_INVALID_NUMCHANS;
	pslLogError("pslValidateModule", "Number of Channels is not valid", status);
	return status;
    }
		
    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine validates defaults information specific to the dxpg200
 * product.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslValidateDefaults(XiaDefaults *defaults)
{
    int status;

	
    if (!pslAreAllDefaultsPresent(defaults))
    {
	status = XIA_INCOMPLETE_DEFAULTS;
	sprintf(info_string, "Defaults with alias %s does not contain all defaults", defaults->alias);
	pslLogError("pslValidateDefaults", info_string, status);
	return status;
    }


    if (!pslIsNumBinsInRange(defaults))
    {
	status = XIA_BINS_OOR;
	sprintf(info_string, "The number of bins for defaults with alias %s is not in range", defaults->alias);
	pslLogError("pslValidateDefaults", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine verifies that the interface for this module is consistient
 * with a dxpg200.
 *
 *****************************************************************************/
PSL_STATIC boolean_t PSL_API pslIsInterfaceValid(Module *module)
{
    int status;

    if ((module->interface_info->type != EPP) &&
	(module->interface_info->type != GENERIC_EPP) &&
	(module->interface_info->type != USB))
    {
	status = XIA_MISSING_INTERFACE;
	sprintf(info_string, "Invalid Interface Type = %u", module->interface_info->type);
	pslLogError("pslIsInterfaceValid", info_string, status);
	return FALSE_;
    }

    return TRUE_;
}


/*****************************************************************************
 *
 * This routine verifies the the epp_address is consistient with the
 * dxpg200.
 *
 *****************************************************************************/
PSL_STATIC boolean_t PSL_API pslIsEPPAddressValid(Module *module)
{
    int status;

    if ((module->interface_info->type == EPP) ||
		(module->interface_info->type == GENERIC_EPP)) 
	  {
		if ((module->interface_info->info.epp->epp_address != 0x378) &&
			(module->interface_info->info.epp->epp_address != 0x278))
		  {
			status = XIA_MISSING_ADDRESS;
			sprintf(info_string, "Invalid EPP Address = 0x%x", module->interface_info->info.epp->epp_address);
			pslLogError("pslIsEPPAddressValid", info_string, status);
			return FALSE_;
		  }
	  } else if (module->interface_info->type == USB) {
		/* Do nothing for now */
	  }

    return TRUE_;
}


/*****************************************************************************
 *
 * This routine verfies that the number of channels is consistient with the 
 * dxpg200.
 *
 *****************************************************************************/
PSL_STATIC boolean_t PSL_API pslIsNumChannelsValid(Module *module)
{
    if (module->number_of_channels != 1)
    {
	return FALSE_;
    }

    return TRUE_;
}


/*****************************************************************************
 *
 * This routine checks that all of the defaults are present in the defaults
 * associated with this dxpg200 channel.
 *
 *****************************************************************************/
PSL_STATIC boolean_t PSL_API pslAreAllDefaultsPresent(XiaDefaults *defaults)
{
    int status = XIA_SUCCESS;

    unsigned int len;
    unsigned int i;

    double ddummy = 0.0;

    len = sizeof(defaultNames) / sizeof(defaultNames[0]);

    /* Loop over all the defaults, making sure each has a entry...use the return
     * code to verify that they exist */
    for (i = 0; i < len; i++) 
    {
	status += pslGetDefault(defaultNames[i], (void *) &ddummy, defaults);
    }

    /* Check if they all exist */
    if (status != XIA_SUCCESS) 
    {
	pslLogError("pslValidateDefaults", "Some default acquisition values are missing.", status);
	return FALSE_;
    }
  
    return TRUE_;
}


/*****************************************************************************
 *
 * This routine checks that the number_mca_bins is consistient with the 
 * allowed values for a dxpg200.
 *
 *****************************************************************************/
PSL_STATIC boolean_t PSL_API pslIsNumBinsInRange(XiaDefaults *defaults)
{
    int status; 

    XiaDaqEntry *current = NULL;

    double numChans = 0.0;

    current = defaults->entry;

    /* get the entry from the defaults list */
    status = pslGetDefault("number_mca_channels", (void *) &numChans, defaults);

    if ((numChans > 65536.0) || (numChans <= 0.0))
    {
	return FALSE_;
    }

    return TRUE_;
}


/*****************************************************************************
 *
 * This routine handles downloading the requested kind of firmware through
 * XerXes.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslDownloadFirmware(int detChan, char *type, char *file, 
					   CurrentFirmware *currentFirmware, 
					   char *rawFilename)
{
    int status;
    int statusX;

    /* Immediately dismiss the types that aren't supported (currently) by the
     * G200
     */
    if (STREQ(type, "user_fippi"))
    {
	status = XIA_NOSUPPORT_FIRM;
	pslLogError("pslDownloadFirmware", "Type user_fippi not supported by the G200", status);
	return status;
    }

    if (STREQ(type, "dsp"))
    {
	sprintf(info_string, "currentFirmware->currentDSP = %s", currentFirmware->currentDSP);
	pslLogDebug("pslDownloadFirmware", info_string);
	sprintf(info_string, "rawFilename = %s", rawFilename);
	pslLogDebug("pslDownloadFirmware", info_string);
	sprintf(info_string, "file = %s", file);
	pslLogDebug("pslDownloadFirmware", info_string);


	if (!STREQ(rawFilename, currentFirmware->currentDSP))
	{
	    statusX = dxp_replace_dspconfig(&detChan, file);

	    sprintf(info_string, "dspFile = %s", rawFilename);
	    pslLogDebug("pslDownloadFirmware", info_string); 

	    if (statusX != DXP_SUCCESS)
	    {
		status = XIA_XERXES;
		sprintf(info_string, "Error replacing DSP program with file: %s",file);
		pslLogError("pslDownloadFirmware", info_string, status);
		return status;
	    }

	    strcpy(currentFirmware->currentDSP, rawFilename);
	}
		
    } else if (STREQ(type, "fippi")) {

	sprintf(info_string, "currentFirmware->currentFiPPI = %s", currentFirmware->currentFiPPI);
	pslLogDebug("pslDownloadFirmware", info_string);
	sprintf(info_string, "rawFilename = %s", rawFilename);
	pslLogDebug("pslDownloadFirmware", info_string);
	sprintf(info_string, "file = %s", file);
	pslLogDebug("pslDownloadFirmware", info_string);


	if (!STREQ(rawFilename, currentFirmware->currentFiPPI))
	{
	    statusX = dxp_replace_fpgaconfig(&detChan, "fippi", file);

	    sprintf(info_string, "fippiFile = %s", rawFilename);
	    pslLogDebug("pslDownloadFirmware", info_string); 

	    if (statusX != DXP_SUCCESS)
	    {
		status = XIA_XERXES;
		sprintf(info_string, "Error replacing FIPPI program with file: %s",rawFilename);
		pslLogError("pslDownloadFirmware", info_string, status);
		return status;
	    }

	    strcpy(currentFirmware->currentFiPPI, rawFilename);
	}

    } else if (STREQ(type, "mmu")) {
	sprintf(info_string, "currentFirmware->currentMMU = %s", currentFirmware->currentMMU);
	pslLogDebug("pslDownloadFirmware", info_string);
	sprintf(info_string, "rawFilename = %s", rawFilename);
	pslLogDebug("pslDownloadFirmware", info_string);
	sprintf(info_string, "file = %s", file);
	pslLogDebug("pslDownloadFirmware", info_string);
	  
	  
	if (!STREQ(rawFilename, currentFirmware->currentMMU))
	{
	    statusX = dxp_replace_fpgaconfig(&detChan, "mmu", file);
	      
	    sprintf(info_string, "mmuFile = %s", rawFilename);
	    pslLogDebug("pslDownloadFirmware", info_string); 
	      
	    if (statusX != DXP_SUCCESS)
	    {
		status = XIA_XERXES;
		sprintf(info_string, "Error replacing MMU program with file: %s",rawFilename);
		pslLogError("pslDownloadFirmware", info_string, status);
		return status;
	    }
	      
	    strcpy(currentFirmware->currentMMU, rawFilename);
	}
	  
    } else {
	status = XIA_UNKNOWN_FIRM;
	sprintf(info_string, "Unable to identify the firmware type (%s)", type);
	pslLogError("pslDownloadFirmware", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine calculates the appropriate DSP parameter(s) from the name and
 * then downloads it/them to the board. 
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslSetAcquisitionValues(int detChan, char *name, void *value, 
                                               XiaDefaults *defaults, 
                                               FirmwareSet *firmwareSet, 
                                               CurrentFirmware *currentFirmware, 
                                               char *detectorType, double gainScale,
											   Detector *detector, int detector_chan,
											   Module *m)
{
    int status = XIA_SUCCESS;
    int statusX;

	
	UNUSED(m);


    /* All of the calculations are dispatched to the appropriate routine. This 
     * way, if the calculation ever needs to change, which it will, we don't
     * have to search in too many places to find the implementation.
     */
    if (STREQ(name, "peaking_time"))
    {
	status = pslDoPeakingTime(detChan, value, firmwareSet, currentFirmware, detectorType, defaults);
	
    } else if (STREQ(name, "gap_time")) {

	status = pslDoGapTime(detChan, value, firmwareSet, defaults);

    } else if (STREQ(name, "trigger_peaking_time")) {

	status = pslDoTriggerPeakingTime(detChan, value, defaults);

    } else if (STREQ(name, "trigger_gap_time")) {

	status = pslDoTriggerGapTime(detChan, value, defaults);

    } else if (STREQ(name, "trigger_threshold")) {

	status = pslDoTriggerThreshold(detChan, value, defaults);

    } else if (STREQ(name, "number_mca_channels")) {

	status = pslDoNumMCAChannels(detChan, value, defaults);

    } else if (STREQ(name, "baseline_percent_rule")) {

	status = pslDoBaselinePercentRule(detChan, value, defaults, gainScale, detector, detector_chan);

    } else if (STREQ(name, "dynamic_range")) {

	status = pslDoDynamicRange(detChan, value, defaults, gainScale, detector, detector_chan);

    } else if (STREQ(name, "adc_percent_rule")) {

	status = pslDoADCPercentRule(detChan, value, defaults, gainScale, detector, detector_chan);

    } else if (STREQ(name, "trace_length")) {

	status = pslDoTraceLength(detChan, value, defaults);

    } else if (STREQ(name, "trace_delay")) {

	status = pslDoTraceDelay(detChan, value, defaults);

    } else if (STREQ(name, "multiplicity_width")) {

	status = pslDoMultiplicityWidth(detChan, value, defaults);

    } else if (STREQ(name, "baseline_filter_length")) {

	status = pslDoNumberBaselineAverages(detChan, value, defaults);

    } else if (STREQ(name, "write_level_one_buffer")) {

	status = pslDoSetModCSRABit(detChan, name, value, MCSRA_LEVEL_ONE, (boolean_t) (*((double *) value) != 0.), defaults);

    } else if (STREQ(name, "enable_group_trigger_input")) {

	status = pslDoSetChanCSRABit(detChan, name, value, CCSRA_EXTTRIG, (boolean_t) (*((double *) value) != 0.), defaults);

    } else if (STREQ(name, "measure_individual_livetime")) {

	status = pslDoSetChanCSRABit(detChan, name, value, CCSRA_LIVETIME, (boolean_t) (*((double *) value) != 0.), defaults);

    } else if (STREQ(name, "good_channel")) {

	status = pslDoSetChanCSRABit(detChan, name, value, CCSRA_GOOD_CHAN, (boolean_t) (*((double *) value) != 0.), defaults);

    } else if (STREQ(name, "read_always")) {

	status = pslDoSetChanCSRABit(detChan, name, value, CCSRA_READ_ALWAYS, (boolean_t) (*((double *) value) != 0.), defaults);

    } else if (STREQ(name, "enable_trigger")) {

	status = pslDoSetChanCSRABit(detChan, name, value, CCSRA_ENABLE_TRIG, (boolean_t) (*((double *) value) != 0.), defaults);

    } else if (STREQ(name, "enable_group_trigger_output")) {

	status = pslDoSetChanCSRABit(detChan, name, value, CCSRA_MULT, (boolean_t) (*((double *) value) != 0.), defaults);

    } else if (STREQ(name, "histogram_energies")) {

	status = pslDoSetChanCSRABit(detChan, name, value, CCSRA_HISTOGRAM, (boolean_t) (*((double *) value) != 0.), defaults);

    } else if (STREQ(name, "enable_ballistic_deficit_correction")) {

	status = pslDoSetChanCSRABit(detChan, name, value, CCSRA_BDEFICIT, (boolean_t) (*((double *) value) != 0.), defaults);

    } else if (STREQ(name, "enable_DNL_correction")) {

	status = pslDoSetChanCSRABit(detChan, name, value, CCSRA_DNL, (boolean_t) (*((double *) value) != 0.), defaults);

    } else if (STREQ(name, "enable_CFD")) {

	status = pslDoSetChanCSRABit(detChan, name, value, CCSRA_CFD, (boolean_t) (*((double *) value) != 0.), defaults);

    } else if (STREQ(name, "enable_gate")) {

	status = pslDoSetChanCSRABit(detChan, name, value, CCSRA_GATE, (boolean_t) (*((double *) value) != 0.), defaults);

    } else if (STREQ(name, "enable_interrupt")) {

	status = pslDoEnableInterrupt(detChan, value, defaults);

    } else if (STREQ(name, "calibration_energy")) {

	status = pslDoCalibrationEnergy(detChan, value, defaults, gainScale, detector, detector_chan);

    } else if (STREQ(name, "run_type")) {

	status = pslDoRunType(detChan, value, defaults);
	  
    } else if (STREQ(name, "offset")) {

	status = pslDoOffset(detChan, value, defaults);

    } else if (STREQ(name, "preamp_gain")) {

	  status = pslDoPreampGain(detChan, value, defaults, detector, detector_chan, gainScale);

    } else if (STREQ(name, "decay_time")) {

	  status = pslDoDecayTime(detChan, value, defaults, detector, detector_chan);

      } else if (STREQ(name, "enable_baseline_cut")) {
	 
	 status = pslDoEnableBaselineCut(detChan, value, defaults);
	 
      } else if (STREQ(name, "baseline_cut")) {
	 
	 status = pslDoBaselineCut(detChan, value, defaults);
	 
      } else if (STREQ(name, "preset_standard")) {
	 
	 status = pslDoPreset(detChan, value, PRESET_STD, defaults);
	 
      } else if (STREQ(name, "preset_runtime")) {
	 
	 status = pslDoPreset(detChan, value, PRESET_RT, defaults);
	 
      } else if (STREQ(name, "preset_livetime")) {
	 
	 status = pslDoPreset(detChan, value, PRESET_LT, defaults);
	 
      } else if (STREQ(name, "reset_delay")) {
	 
	 status = pslDoResetDelay(detChan, value, defaults, detector, detector_chan);
	 
    } else if (STREQ(name, "detector_polarity")) {

	  status = pslDoDetectorPolarity(detChan, value, defaults, detector, detector_chan);
	  
	} else if ((STREQ(name,  "system_gain")) ||
			   (STREQ(name, "minimum_gain")) ||
			   (STREQ(name, "maximum_gain")) ||
			   (STREQ(name, "minimum_dynamic_range")) ||
			   (STREQ(name, "maximum_dynamic_range")) ||
			   (STREQ(name, "actual_gap_time"))) {
/* Do Nothing, these are read-only acquisition values */

    } else if ((strncmp(name, "peakint", 7) == 0) ||
	       (strncmp(name, "peaksam", 7) == 0)) 
    {
	status = pslSetDefault("name", value, defaults);
	status = pslUpdateFilter(detChan, defaults, firmwareSet);

    } else if (pslIsUpperCase(name)) {

	status = pslDoParam(detChan, name, value, defaults);
	
    } else {

	status = XIA_UNKNOWN_VALUE;
    }

    if (status != XIA_SUCCESS) 
    {
	sprintf(info_string, "Error processing Acquisition Value = %s", name);
	pslLogError("pslSetAcquisitionValues", info_string, status);
	return status;
    }

    statusX = dxp_upload_dspparams(&detChan);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	pslLogError("pslSetAcquisitionValues", "Error uploading params through Xerxes", status);
	return status;
    }

    return XIA_SUCCESS;	
}

/*****************************************************************************
 *
 * This routine does all of the steps required in modifying the peaking time
 * for a given G200 detChan:
 * 
 * 1) Change FiPPI if necessary
 * 2) Update Filter Parameters
 * 3) Return "calculated" Peaking Time
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoPeakingTime(int detChan, void *value, FirmwareSet *firmwareSet, 
					CurrentFirmware *currentFirmware, char *detectorType, 
					XiaDefaults *defaults)
{
    int status;

    double peakingTime;
    double requestedPeakingTime;

    char fippi[MAXFILENAME_LEN];
    char rawFilename[MAXFILENAME_LEN];

    Firmware *firmware = NULL;

    CLOCK_SPEED = pslGetClockSpeed(detChan);

    peakingTime = *((double *) value);
    requestedPeakingTime = peakingTime;

    /* The code below is replacing an old algorithm that used to check the decimation
     * instead of the filename to determine if firmware needs to be downloaded or not.
     * 
     * All of the comparison code is going to be in pslDownloadFirmware().
     *
     */
    if (firmwareSet->filename == NULL)
    {
	firmware = firmwareSet->firmware;
	while (firmware != NULL)
	{
	    if ((peakingTime > firmware->min_ptime) &&
		(peakingTime <= firmware->max_ptime))
	    {
		strcpy(fippi, firmware->fippi);
		strcpy(rawFilename, firmware->fippi);
		break;
	    }
	
	    firmware = getListNext(firmware);
	}

    } else {
	/* filename is actually defined in this case */
	sprintf(info_string, "peakingTime = %lf", peakingTime);
	pslLogDebug("pslDoPeakingTime", info_string);

	status = xiaFddGetFirmware(firmwareSet->filename, "fippi", peakingTime,
				   (unsigned short)firmwareSet->numKeywords, 
				   firmwareSet->keywords, detectorType,
				   fippi, rawFilename);

	if (status != XIA_SUCCESS)
	{
	    sprintf(info_string, "Error retrieving FDD Firmware in file %s", firmwareSet->filename);
	    pslLogError("pslDoPeakingTime", info_string, status);
	    return status;
	}
    }

    status = pslDownloadFirmware(detChan, "fippi", fippi, currentFirmware, rawFilename);
    if (status != XIA_SUCCESS)
    {
	sprintf(info_string, "Error downloading FIPPI file %s", rawFilename);
	pslLogError("pslDoPeakingTime", info_string, status);
	return status;
    }
  
    /* Set the peaking time in the defaults */
    status = pslSetDefault("peaking_time", value, defaults);
  
    /* Update the total filter */
    status = pslUpdateFilter(detChan, defaults, firmwareSet);

    if (status != XIA_SUCCESS) 
    {
	pslLogError("pslDoPeakingTime", "Error updating filter information", status);
	return status;
    }

    /* Get the peaking time in the defaults, in case the user chosen value isn't 
     * the exact one in use */
    status = pslGetDefault("peaking_time", (void *) &peakingTime, defaults);
    *((double *)value) = peakingTime;

    /* If the peaking time changed from the requested value, we need to recursively call DoPeakingTime()
     * in order to be sure we did not cross a firmware set boundry when we artificially changed the peaking
     * time to match a decimator clock boundry.  This is very subtle but could lead to not using the 
     * decimation firmware set that the user thinks she is using.
     */
    if (peakingTime != requestedPeakingTime) 
    {
	status = pslDoPeakingTime(detChan, value, firmwareSet, currentFirmware, 
				  detectorType, defaults);
	  
	if (status != XIA_SUCCESS) 
	{
	    pslLogError("pslDoPeakingTime", "Error during a recursive call to pslDoPeakingTime()", status);
	    return status;
	}
    }
  
    sprintf(info_string, "New peaking time = %lf\n", peakingTime);
    pslLogDebug("pslDoPeakingTime", info_string);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine modifies the gap time for the slow filter
 * 
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoGapTime(int detChan, void *value, FirmwareSet *firmwareSet, 
									XiaDefaults *defaults)
{
    int statusX;
    int status;

    parameter_t decimation = 0;
    parameter_t SLOWGAP       = 0;

    CLOCK_SPEED = pslGetClockSpeed(detChan);

    /* Get the decimation from the board */
    status = dxp_get_one_dspsymbol(&detChan, "DECIMATION", &decimation);

    if (status != DXP_SUCCESS)
	  {
		status = XIA_XERXES;
		sprintf(info_string, "Error getting DECIMATION for detChan %d", detChan);
		pslLogError("pslDoGapTime", info_string, status);
		return status;
    }

    /* Set the gap time in the defaults */
    status = pslSetDefault("gap_time", value, defaults);

    /* Update the total filter */
    status = pslUpdateFilter(detChan, defaults, firmwareSet);

    if (status != XIA_SUCCESS) {
	pslLogError("pslDoGapTime", "Error updating filter information", status);
	return status;
    }

    /* Need to get new SLOWGAP value from board */
    statusX = dxp_get_one_dspsymbol(&detChan, "SLOWGAP0", &SLOWGAP);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting SLOWGAP for detChan %d", detChan);
	pslLogError("pslDoGapTime", info_string, status);
	return status;
    }

    /* Set value equal to the new "real" peaking time... */
    *((double *)value) = (((double) SLOWGAP) / CLOCK_SPEED) * pow(2, (double) decimation);

	status = pslSetDefault("actual_gap_time", value, defaults);
	
	if (status != XIA_SUCCESS) 
	  {
		sprintf(info_string, "Error setting actual_gap_time for detChan %d", detChan);
		pslLogError("pslDoGapTime", info_string, status);
		return status;
	  }
  
    sprintf(info_string, "New gap time = %lf\n", *((double *) value));
    pslLogDebug("pslDoGapTime", info_string);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine modifies the trigger peaking time for the fast filter
 * 
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoTriggerPeakingTime(int detChan, void *value, XiaDefaults *defaults)
{
    int status;

    /* Set the trigger peaking time entry */
    status = pslSetDefault("trigger_peaking_time", value, defaults);

    /* Update the trigger filter */
    status = pslUpdateTriggerFilter(detChan, defaults);

    if (status != XIA_SUCCESS) {
	pslLogError("pslDoTriggerPeakingTime", "Error updating trigger filter information", status);
	return status;
    }

    /* Get the updated trigger peaking time entry */
    status = pslGetDefault("trigger_peaking_time", value, defaults);

    sprintf(info_string, "New trigger peaking time = %lf\n", *((double *)value));
    pslLogDebug("pslDoTriggerPeakingTime", info_string);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine modifies the trigger gap time for the fast filter
 * 
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoTriggerGapTime(int detChan, void *value, XiaDefaults *defaults)
{
    int status;

    /* Set the trigger gap time entry */
    status = pslSetDefault("trigger_gap_time", value, defaults);

    /* Update the trigger filter */
    status = pslUpdateTriggerFilter(detChan, defaults);

    if (status != XIA_SUCCESS) {
	pslLogError("pslDoTriggerGapTime", "Error updating trigger filter information", status);
	return status;
    }

    /* Get the updated trigger gap time entry */
    status = pslGetDefault("trigger_gap_time", value, defaults);

    sprintf(info_string, "New trigger gap time = %lf\n", *((double *)value));
    pslLogDebug("pslDoTriggerGapTime", info_string);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets the baseline point of the detector signal, which changes'
 * the gain of the system.  The dynamic range is defined as the energy of a 
 * pulse that spans the full range of the ADC minus the offset introduced due
 * to a baseline calibration.  This is applicable to RC feedback preamps where
 * the baseline to which a decaying exponential returns is fixed.
 * 
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoBaselinePercentRule(int detChan, void *value, 
												XiaDefaults *defaults, double gainScale,
												Detector *detector, int detector_chan)
{
    int status;

    double baselinePercentRule;
    double adcPercentRule;
    double calibrationEnergy;
    double dynamicRange;

    /* Set the baseline percent rule entry */
    status = pslSetDefault("baseline_percent_rule", value, defaults);
    if (status != XIA_SUCCESS)
    {
	pslLogError("pslDoBaselinePercentRule", "Error setting the baseline_percent_rule in the defaults", status);
	return status;
    }

    baselinePercentRule = *((double *) value);

    /* Get the ADC percent rule and the dynamic range */
    status = pslGetDefault("adc_percent_rule", (void *) &adcPercentRule, defaults);
    if (status != XIA_SUCCESS)
    {
	pslLogError("pslDoBaselinePercentRule", "Error getting the adc_percent_rule from the defaults", status);
	return status;
    }

    status = pslGetDefault("dynamic_range", (void *) &dynamicRange, defaults);
    if (status != XIA_SUCCESS)
    {
	pslLogError("pslDoBaselinePercentRule", "Error getting the dynamic_range from the defaults", status);
	return status;
    }


    sprintf(info_string, "ADCPercentRule = %lf, DynamicRange = %lf, BaselinePercentRule = %lf", 
	    adcPercentRule, dynamicRange, baselinePercentRule);
    pslLogDebug("pslDoBaselinePercentRule", info_string);

    /* Calculate the new calibration energy */
    calibrationEnergy = dynamicRange * (adcPercentRule/100.) / ((100. - baselinePercentRule)/100.);

    /* Set the calibration energy and the gain along with it */
    status = pslDoCalibrationEnergy(detChan, (void *) &calibrationEnergy, defaults, 
									gainScale, detector, detector_chan);
  
    if (status != XIA_SUCCESS) {
	pslLogError("pslDoBaselinePercentRule", "Error setting the calibration energy", status);
	return status;
    }

    sprintf(info_string, "New baseline percent rule = %lf%%\n", *((double *)value));
    pslLogDebug("pslDoBaselinePercentRule", info_string);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets the dynamic range of the module, which changes the gain.
 * The dynamic range is defined as the energy of a 
 * pulse that spans the full range of the ADC minus the offset introduced due
 * to a baseline calibration.  This is applicable to RC feedback preamps where
 * the baseline to which a decaying exponential returns is fixed.
 * 
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoDynamicRange(int detChan, void *value, 
										 XiaDefaults *defaults, double gainScale,
										 Detector *detector, int detector_chan)
{
    int status;

    double baselinePercentRule;
    double adcPercentRule;
    double calibrationEnergy;
    double dynamicRange;

    /* Set the dynamic range entry */
    status = pslSetDefault("dynamic_range", value, defaults);
    if (status != XIA_SUCCESS)
    {
	pslLogError("pslDoDynamicRange", "Error setting the dynamic_range in the defaults", status);
	return status;
    }

    dynamicRange = *((double *) value);

    /* Get the ADC percent rule and the baseline percent rule */
    status = pslGetDefault("adc_percent_rule", (void *) &adcPercentRule, defaults);
    if (status != XIA_SUCCESS)
    {
	pslLogError("pslDoDynamicRange", "Error getting the adc_percent_rule from the defaults", status);
	return status;
    }

    status = pslGetDefault("baseline_percent_rule", (void *) &baselinePercentRule, defaults);
    if (status != XIA_SUCCESS)
    {
	pslLogError("pslDoDynamicRange", "Error getting the baseline_percent_rule from the defaults", status);
	return status;
    }

    sprintf(info_string, "ADCPercentRule = %lf, DynamicRange = %lf, BaselinePercentRule = %lf", 
			adcPercentRule, dynamicRange, baselinePercentRule);
    pslLogDebug("pslDoDynamicRange", info_string);

    /* Calculate the new calibration energy */
    calibrationEnergy = dynamicRange * (adcPercentRule/100.) / ((100. - baselinePercentRule)/100.);

    /* Set the calibration energy and the gain along with it */
    status = pslDoCalibrationEnergy(detChan, (void *) &calibrationEnergy, defaults, 
									gainScale, detector, detector_chan);
  
    if (status != XIA_SUCCESS) {
	pslLogError("pslDoDynamicRange", "Error setting the calibration energy", status);
	return status;
    }

    /* Get the dynamic range entry, which will change when the actual gain is set, return this to the user */
    status = pslGetDefault("dynamic_range", value, defaults);
    if (status != XIA_SUCCESS)
    {
	pslLogError("pslDoDynamicRange", "Error getting the dynamic_range from the defaults", status);
	return status;
    }
    sprintf(info_string, "New dynamic range = %lfMeV\n", *((double *)value));
    pslLogDebug("pslDoDynamicRange", info_string);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets the Trigger Threshold parameter on the DSP code based on
 * calculations from values in the defaults (when required) or those that
 * have been passed in.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoTriggerThreshold(int detChan, void *value, XiaDefaults *defaults)
{
    int status;
    int statusX;

    double eVPerADC = 0.0;
    double adcPercentRule =0.0;
    double dTHRESHOLD = 0.0;
    double dADCTHRESHOLD = 0.0;
    double thresholdEV = 0.0;
    double calibEV = 0.0;

    parameter_t FASTLEN;
    parameter_t THRESHOLD;
	
    /* N.b.: Both the adc_percent_rule and the calibration_energy have been 
     * verified in the defaults, so we can be especially cavalier in our
     * searching method.
     */
    status = pslGetDefault("adc_percent_rule", (void *) &adcPercentRule, defaults);
    if (status != XIA_SUCCESS)
    {
	pslLogError("pslDoTriggerThreshold", "Unable to retrieve adc_percent_rule acquisition value", status);
	return status;
    }

    status = pslGetDefault("calibration_energy", (void *) &calibEV, defaults);
    if (status != XIA_SUCCESS)
    {
	pslLogError("pslDoTriggerThreshold", "Unable to retrieve calibration_energy acquisition value", status);
	return status;
    }

	/* Get the ev/ADC value */
	status = pslGetEVPerADC(defaults, &eVPerADC);
	if (status != XIA_SUCCESS) 
      {
		sprintf(info_string, "Unable to get the eVPerADC");
		pslLogError("pslDoTriggerThreshold", info_string, status);
		return status;
      }
	/* The threshold is based on a 14 bit value, not 16 bit value */
	eVPerADC *= 4;
	
    statusX = dxp_get_one_dspsymbol(&detChan, "FASTLENGTH0", &FASTLEN);

    if (statusX != DXP_SUCCESS)
	  {
		status = XIA_XERXES;
		pslLogError("pslDoTriggerThreshold", "Error getting the FASTLENGTH from XerXes", status);
		return status;
	  }
	
	thresholdEV = *((double *)value);
	
	dADCTHRESHOLD  = thresholdEV / eVPerADC;
	
	/* Threshold is in 12 bit ADC units, so divide by 16 to get from the evPerADC value in 16 bit units */
	dTHRESHOLD  = ((double) FASTLEN) * dADCTHRESHOLD / 16.0;
	/* The DSP will divide by 8 our desired threshold, so we need to round the lower 3 bits */
	THRESHOLD    = (parameter_t) (8.0 * ROUND(dTHRESHOLD / 8.0));
	
    sprintf(info_string, "thresholdEV = %lfeV, eVPerADC = %lf, THRESHOLD = 0x%x", thresholdEV, eVPerADC, THRESHOLD);
    pslLogDebug("pslDoTriggerThreshold", info_string);
	
    /* The actual range to use is 0 < THRESHOLD < MAX_THRESHOLD
     * If the threshold is greater, then set to max.
     */
    if (THRESHOLD > TRIGGER_THRESHOLD_MAX)
    {
	  /* set the parameters to max */
	  THRESHOLD = TRIGGER_THRESHOLD_MAX;
	  /*      ADCTHRESHOLD = (parameter_t) ROUND((THRESHOLD / FASTLEN) - 0.5);*/
    }
	
	statusX = dxp_set_one_dspsymbol(&detChan, "FASTTHRESH0", &THRESHOLD);
	if (statusX != DXP_SUCCESS)
	  {
		pslLogError("pslDoTriggerThreshold", "Error setting the FASTTHRESH in XerXes", status);
		return XIA_XERXES;
	  }

    /* Re-"calculate" the actual threshold. This _is_ a deterministic process since the
     * specified value of the threshold is only modified due to rounding...
     */
	thresholdEV = ((double)THRESHOLD * eVPerADC) / ((double) FASTLEN) * 16.;
	*((double *)value) = thresholdEV;

    /* Set the threshold within the defaults */
    status = pslSetDefault("trigger_threshold", (void *) &thresholdEV, defaults);
    if (status != XIA_SUCCESS)
	  {
		pslLogError("pslDoTriggerThreshold", "Unable to set trigger_threshold in the defaults list", status);
		return status;
	  }
	
	/* And update the FiPPi Settings */
	status = pslDoControlTask(detChan, CT_DGFG200_PROGRAM_FIPPI, NULL);
	if (status != XIA_SUCCESS)
	  {
		pslLogError("pslDoTriggerThreshold", "Unable to execute the program_fippi control task", status);
		return status;
	  }
	
    sprintf(info_string, "New trigger threshold = %lfeV, in bits = 0x%x\n", *((double *)value), THRESHOLD);
    pslLogDebug("pslDoTriggerThreshold", info_string);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets the value of LOG2EBIN.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoNumMCAChannels(int detChan, void *value, XiaDefaults *defaults)
{
    int status;
    int statusX;

    double numMCAChans;

    parameter_t LOG2EBIN;

    numMCAChans = *((double *)value);

    if ((numMCAChans < 1024.) ||
	(numMCAChans > 65536.))
    {
	status = XIA_BINS_OOR;
	sprintf(info_string, "Requested number of bins (%lf) is not allowed", numMCAChans);
	pslLogError("pslDoNumMCAChannels", info_string, status);
	return status;
    }

    /* check that number of bins is a factor of 2 */
    if ((((unsigned int) numMCAChans)%2) != 0) 
    {
	status = XIA_BINS_OOR;
	sprintf(info_string, "Requested number of bins (%lf) is a factor of 2", numMCAChans);
	pslLogError("pslDoNumMCAChannels", info_string, status);
	return status;
    }

    /* Now translate the number of bins to LOG2EBIN */
    LOG2EBIN = (parameter_t) (65530. + ROUND((log(numMCAChans)/log(2.)) - 10.));
    /* check for the special case of 64K */
    if (LOG2EBIN == 65536) LOG2EBIN = 0;

    statusX = dxp_set_one_dspsymbol(&detChan, "LOG2EBIN0", &LOG2EBIN);

    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	pslLogError("pslDoNumMCAChannels", "Unable to set the DSP symbol LOG2EBIN0 using XerXes", status);
	return status;
    }

    /* Set the number of channels in the defaults */
    status = pslSetDefault("number_mca_channels", (void *) &numMCAChans, defaults);

    sprintf(info_string, "New number of MCA bins = %lf\n", *((double *)value));
    pslLogDebug("pslDoNumMCAChannels", info_string);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * Actually changes the value of the defaults setting AND recalculates the
 * parameters that depend on the percent rule. This is a little different 
 * then if you just modified the percent rule through the defaults, since
 * that doesn't recalculate any of the other acquisition parameters.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoADCPercentRule(int detChan, void *value, 
                                           XiaDefaults *defaults, 
                                           double gainScale, 
										   Detector *detector, int detector_chan)
{	
    int status;

    double calibrationEnergy;
    double baselinePercentRule;
    double dynamicRange;
    double adcPercentRule;

    /* Like every other time when we use the defaults within the acquisition 
     * values context, they have already been validated and we can search 
     * in a very reckless manner.
     */
    status = pslSetDefault("adc_percent_rule", value, defaults);
    adcPercentRule = *((double *) value);

    /* Get the calibration energy and use the DoCalibrationEnergy() routine to set the gain */
    status = pslGetDefault("calibration_energy", (void *) &calibrationEnergy, defaults);
    /* Now use the calibration energy routine */
    status = pslDoCalibrationEnergy(detChan, (void *) &calibrationEnergy, 
									defaults, gainScale, detector, detector_chan);

    if (status != XIA_SUCCESS)
    {
	pslLogError("pslDoADCPercentRule", "Unable to set the calibration energy.", status);
	return status;
    }

    /* Get the calibration energy and the baseline percent rule in order to recalculate the dynamic range */
    status = pslGetDefault("calibration_energy", (void *) &calibrationEnergy, defaults);
    status = pslGetDefault("baseline_percent_rule", (void *) &baselinePercentRule, defaults);

    /* Calculate the new calibration energy */
    dynamicRange = calibrationEnergy / (adcPercentRule/100.) * ((100. - baselinePercentRule)/100.);

    /* Save the dynamic range in the defaults */
    status = pslSetDefault("dynamic_range", (void *) &dynamicRange, defaults);
  
    sprintf(info_string, "New ADC percent rule = %lf, dynamic range = %lfeV\n", *((double *)value), dynamicRange);
    pslLogDebug("pslDoADCPercentRule", info_string);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine essentially sets the CHANCSRA bit for the compton veto.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoEnableInterrupt(int detChan, void *value, XiaDefaults *defaults)
{
    int statusX;
    int status;

    double enableInterrupt;

    enableInterrupt = *((double *)value);

    /* Set the proper bit for Look-At-Me (LAM) to be enabled */
    if (enableInterrupt != 0) 
    {
	statusX = dxp_enable_LAM(&detChan);
    } else {
	statusX = dxp_disable_LAM(&detChan);
    }

    if (statusX != DXP_SUCCESS) 
    {
	status = XIA_XERXES;
	pslLogError("pslDoEnableInterrupt", "Unable to either enable or disable the Interrupt using XerXes", status);
	return status;
    }

    /* set the value of the variable in the defaults */
    status = pslSetDefault("enable_interrupt", value, defaults);
    if (status != XIA_SUCCESS)
	  {
		sprintf(info_string, "Unable to set enable_interrupt to %lf in the defaults", *((double *) value));
		pslLogError("pslDoEnableInterrupt", info_string, status);
		return status;
	  }
	
    /* Set the compton veto state in the defaults */
	/*    status = pslSetDefault("enable_gate", (void *) &enableInterrupt, defaults);
	 */
    sprintf(info_string, "Interrupt enable now is %lf\n", *((double *)value));
    pslLogDebug("pslDoEnableInterrupt", info_string);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * Like pslDoADCPercentRule(), this routine recalculates the gain from the
 * calibration energy point of view.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoCalibrationEnergy(int detChan, void *value, 
                                              XiaDefaults *defaults, 
                                              double gainScale, 
											  Detector *detector, int detector_chan)
{
    int status;
    int statusX;

    unsigned int i;
    unsigned int nloops = 10;
  
    double maxChange = 0.01;
    double change = 100.0;

    double adcPercentRule = 0.0;
    double threshold = 0.0;
    double dynamicRange = 0.0;
    double calibrationEnergy = 0.0;
    double baselinePercentRule = 0.0;

    boolean_t outOfRange = FALSE_;

    parameter_t GAINDAC = 65535;
    parameter_t OLDGAINDAC = 0;

    /* Like every other time when we use the defaults within the acquisition 
     * values context, they have already been validated and we can search 
     * in a very reckless manner.
     */
    calibrationEnergy = *((double *)value);

    /* Get the ADC percent rule */
    status = pslGetDefault("adc_percent_rule", (void *) &adcPercentRule, defaults);

    if (status != XIA_SUCCESS)
	  {
		pslLogError("pslDoCalibrationEnergy", "Unable to retrieve the adc_percent_rule from the defaults", status);
		return status;
	  }
	
    /* Get the current value of the GAINDAC */
    statusX = dxp_get_one_dspsymbol(&detChan, "GAINDAC0", &OLDGAINDAC);
	
    if (statusX != DXP_SUCCESS)
	  {
		status = XIA_XERXES;
		pslLogError("pslDoCalibrationEnergy", "Unable to get the parameter GAINDAC0 from XerXes", status);
		return status;
	  }
	
    /* Calculate and set the gain iteratively to protect against the case 
     * where we change all of these other parameters
     * and then find out the gain is out-of-range and, therefore, the whole
     * system is out-of-sync.
     */
    i = 0;
    while (i < nloops) {
	  
	  /* Calculate and set the gain */
	  status = pslCalculateGain(adcPercentRule, calibrationEnergy, detector->gain[detector_chan], &GAINDAC, gainScale, &outOfRange); 
	  
	  if (status != XIA_SUCCESS)
		{
		  pslLogError("pslDoCalibrationEnergy", "Failed to calculate the gain", status);
		  return status;
		}
	  
	  statusX = dxp_set_one_dspsymbol(&detChan, "GAINDAC0", &GAINDAC);
	  
	  if (statusX != DXP_SUCCESS)
		{
		  status = XIA_XERXES;
		  pslLogError("pslDoCalibrationEnergy", "Unable to set the parameter GAINDAC0 in XerXes", status);
		  return status;
		}
	  
	  /* Call routines that depend on the calibration_energy so that
	   * their calculations will now be correct.
	   */
	  status = pslGetDefault("trigger_threshold", (void *) &threshold, defaults);
	  if (status != XIA_SUCCESS)
		{
		  pslLogError("pslDoCalibrationEnergy", "Unable to retrieve the trigger_threshold from the defaults", status);
		  return status;
		}
	  
	  /* use the set acquisition values routine to set the new threshold */
	  status = pslDoTriggerThreshold(detChan, (void *) &threshold, defaults);
	  
	  if (status != XIA_SUCCESS)
		{
		  pslLogError("pslDoCalibrationEnergy", "Failed to set the trigger threshold", status);
		  return status;
		}
	  
	  sprintf(info_string, "Setting GAINDAC = 0x%x, old value was 0x%x", GAINDAC, OLDGAINDAC);
	  pslLogDebug("pslDoCalibrationEnergy", info_string);
	  
	  if (OLDGAINDAC != 0) 
		{
		  change = ((double) OLDGAINDAC - (double) GAINDAC)/((double) OLDGAINDAC);
		  if (change < 0) change = -change;
		  if (change < maxChange) break;
		} else if (GAINDAC == 0) {
		  break;
		} 
	  
	  OLDGAINDAC = GAINDAC;
	  /* Increment the loop counter */
	  i++;
    }
	
    if (i == nloops)
	  {
		status = XIA_NO_GAIN_FOUND;
		pslLogError("pslDoCalibrationEnergy", "The Gain never settled, unable to set the calibration energy", status);
		return status;
	  }
	
    /* Now determine what actual calibration energy was chosen */
    status = pslCalculateCalibrationEnergy(adcPercentRule, &calibrationEnergy, detector->gain[detector_chan], GAINDAC, gainScale); 
	
    if (status != XIA_SUCCESS)
	  {
		pslLogError("pslDoCalibrationEnergy", "Failed to calculate the calibration energy", status);
		return status;
	  }
	
    /* Set the return value of the calibrationEnergy */
    *((double *) value) = calibrationEnergy;
	
    /* Set the new calibration energy in the defaults array */
    status = pslSetDefault("calibration_energy", value, defaults);
    if (status != XIA_SUCCESS)
	  {
		pslLogError("pslDoCalibrationEnergy", "Unable to set the calibration_energy in the defaults", status);
		return status;
	  }
	
    /* Use the calibration energy, adc percent rule and the baseline percent rule in order to recalculate the dynamic range */
    status = pslGetDefault("baseline_percent_rule", (void *) &baselinePercentRule, defaults);
    if (status != XIA_SUCCESS)
	  {
		pslLogError("pslDoCalibrationEnergy", "Unable to retrieve the baseline_percent_rule from the defaults", status);
		return status;
	  }
	
    /* Calculate the new calibration energy */
    dynamicRange = calibrationEnergy / (adcPercentRule/100.) * ((100. - baselinePercentRule)/100.);
	
    /* Save the dynamic range in the defaults */
    status = pslSetDefault("dynamic_range", (void *) &dynamicRange, defaults);
    if (status != XIA_SUCCESS)
	  {
		pslLogError("pslDoCalibrationEnergy", "Unable to set the dynamic_range in the defaults", status);
		return status;
	  }
	
    /* Finally tell the G200 to update its GAIN DACs */
    status = pslDoSpecialRun(detChan, "set_dacs", gainScale, NULL, defaults, detector, detector_chan);
    if (status != XIA_SUCCESS)
	  {
		pslLogError("pslDoCalibrationEnergy", "Unable to execute the set DACs special run", status);
		return status;
	  }
	
    /* And update the system gain measurement */
	/*    status = pslDoSpecialRun(detChan, "measure_system_gain", gainScale, NULL, defaults, detector, detector_chan);
    if (status != XIA_SUCCESS)
	  {
		pslLogError("pslDoCalibrationEnergy", "Unable to execute the measure_system_gain special run", status);
		return status;
		}*/
	
    sprintf(info_string, "New calibration energy is %lf, dynamic range is %lf\n", *((double *)value), dynamicRange);
    pslLogDebug("pslDoCalibrationEnergy", info_string);
	
    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets the REQTIME on the G200 according to the requested runtime.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslSetRequestedTime(int detChan, double *time)
{
    int status;
    int statusX;

    double ticks;
    double maxTime = 0.;

    parameter_t REQTIMEA;
    parameter_t REQTIMEB;
    parameter_t REQTIMEC;

    /* Bounds checking first */
    maxTime = (65536.*65536. - 1.) * DSP_TIMER_TICK_TIME;
    if (*time > maxTime) *time = maxTime;

    ticks = *time / DSP_TIMER_TICK_TIME;
    REQTIMEA = (parameter_t) floor(ticks / 65536.);
    REQTIMEB = (parameter_t) (((unsigned long) ticks) % 65536);
    REQTIMEC = (parameter_t) floor((ticks - floor(ticks)) * 65536.);
  
    /* set the DSP parameters */
    statusX = dxp_set_one_dspsymbol(&detChan, "REQTIMEA", &REQTIMEA);
    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	pslLogError("pslSetRequstedTime", "Unable to set the DSP parameter REQTIMEA with XerXes", status);
	return status;
    }
    statusX = dxp_set_one_dspsymbol(&detChan, "REQTIMEB", &REQTIMEB);
    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	pslLogError("pslSetRequstedTime", "Unable to set the DSP parameter REQTIMEB with XerXes", status);
	return status;
    }
    statusX = dxp_set_one_dspsymbol(&detChan, "REQTIMEC", &REQTIMEC);
    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	pslLogError("pslSetRequstedTime", "Unable to set the DSP parameter REQTIMEC with XerXes", status);
	return status;
    }

    /* Tell the user the good/bad news, reconstruct the true time */
    *time = (((double) REQTIMEA * 65536.) + (double) REQTIMEB + ((double) REQTIMEC / 65536.)) * DSP_TIMER_TICK_TIME;

    sprintf(info_string, "Set REQTIMEA = %u, REQTIMEB = %u, REQTIMEC = %u", REQTIMEA, REQTIMEB, REQTIMEC);
    pslLogDebug("pslSetRequestedTime", info_string);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets parameters related to the Trace Length recorded in the FIFO.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoTraceLength(int detChan, void *value, XiaDefaults *defaults)
{
    int status;
    int statusX;

    double traceLength;

    parameter_t TRACELENGTH;

    traceLength = *((double *)value);

    CLOCK_SPEED = pslGetClockSpeed(detChan);

    TRACELENGTH = (parameter_t) ROUND(traceLength * (CLOCK_SPEED / pow(2., (double) FIFO_DECIMATION)));

    /* 
     * Check that the trace length is not bigger than allowed by FIFO, 
     * if so, then set it to the maximum length 
     */
    if (TRACELENGTH > FIFO_LENGTH) TRACELENGTH = FIFO_LENGTH;
  
    statusX = dxp_set_one_dspsymbol(&detChan, "TRACELENGTH0", &TRACELENGTH);

    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	pslLogError("pslDoTraceLength", "Unable to set the DSP symbol TRACELENGTH0 using XerXes", status);
	return status;
    }

    traceLength = TRACELENGTH / (CLOCK_SPEED / pow(2., (double) FIFO_DECIMATION));
    *((double *) value) = traceLength;
    /* set the default for the trace length */
    status = pslSetDefault("trace_length", value, defaults);
    if (status != XIA_SUCCESS)
	  {
		pslLogError("pslDoTraceLength", "Unable to set the trace_length in the defaults", status);
		return status;
	  }

    /* Also update MAXEVENTS */
    status = pslSetMaxEvents(detChan, defaults);
    if (status != XIA_SUCCESS)
	  {
		pslLogError("pslDoTraceLength", "Unable to set the maximum events", status);
		return status;
	  }

    /* Update all FIFO parameters */
    status = pslUpdateFIFO(detChan, defaults);
    if (status != XIA_SUCCESS)
	  {
		pslLogError("pslDoTraceLength", "Error updating the FIFO information", status);
		return status;
	  }

    sprintf(info_string, "New Trace Length is %lfus\n", *((double *)value));
    pslLogDebug("pslDoTraceLength", info_string);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets parameters related to the Trace Delay recorded in the FIFO.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoTraceDelay(int detChan, void *value, XiaDefaults *defaults)
{
    int status;

    double traceDelay;

    traceDelay = *((double *) value);

    CLOCK_SPEED = pslGetClockSpeed(detChan);

    /* Round it to an integer number of samples, then convert back to a time */
    traceDelay = ROUND(traceDelay * (CLOCK_SPEED / pow(2., (double) FIFO_DECIMATION)));
    traceDelay = traceDelay / (CLOCK_SPEED / pow(2., (double) FIFO_DECIMATION));

    /* Let the user know the new value */
    *((double *) value) = traceDelay;

    /* set the value of trace delay in the defaults */
    status = pslSetDefault("trace_delay", value, defaults);
    if (status != XIA_SUCCESS)
	  {
		pslLogError("pslDoTraceDelay", "Error setting the trace_delay in the defaults", status);
		return status;
	  }

    /* Update all FIFO parameters */
    status = pslUpdateFIFO(detChan, defaults);
    if (status != XIA_SUCCESS)
	  {
		pslLogError("pslDoTraceDelay", "Error updating the FIFO information", status);
		return status;
	  }

    /* retrieve the potentially corrected trace delay from the defaults */
    status = pslGetDefault("trace_delay", value, defaults);
    if (status != XIA_SUCCESS)
	  {
		pslLogError("pslDoTraceDelay", "Error getting the trace_delay from the defaults", status);
		return status;
	  }

    sprintf(info_string, "New Trace Delay is %lfus\n", *((double *)value));
    pslLogDebug("pslDoTraceDelay", info_string);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets parameter controlling the fast trigger pulse (FTP) output width.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoMultiplicityWidth(int detChan, void *value, XiaDefaults *defaults)
{
    int status;
    int statusX;

    double ftpWidth;
	/*    double ddummy;*/

    parameter_t FTPWIDTH;

    ftpWidth = *((double *) value);

    CLOCK_SPEED = pslGetClockSpeed(detChan);

    /* Round it to an integer number of samples, then convert back to a time */
    FTPWIDTH = (parameter_t) ROUND(ftpWidth * CLOCK_SPEED);

    /* Bounds checking */
    if (FTPWIDTH > 63) 
	  {
		FTPWIDTH = 63;
		ftpWidth = ((double) FTPWIDTH) / CLOCK_SPEED;
	  }
	
    /* Let the user know the actual value */
    *((double *) value) = ftpWidth;

    /* set the value of the fast trigger pulse (FTP) width in the defaults */
    status = pslSetDefault("multiplicity_width", value, defaults);

    /* Write the value to the module */
    statusX = dxp_set_one_dspsymbol(&detChan, "FTPWIDTH0", &FTPWIDTH);

    if (statusX != DXP_SUCCESS)
    {
	  status = XIA_XERXES;
	  pslLogError("pslDoMultiplicityWidth", "Unable to set the DSP symbol FTPWIDTH0 using XerXes", status);
	  return status;
    }

    /* Of course, if we are setting a multiplicity width, we want to enable the multiplicity */
	/*    ddummy = 1.0;
    status = pslDoSetChanCSRABit(detChan, "enable_group_trigger_output", (void *) &ddummy, CCSRA_MULT, TRUE_, defaults);
	*/

    sprintf(info_string, "New multiplicity width is %lfus\n", *((double *)value));
    pslLogDebug("pslDoMultiplicityWidth", info_string);
 
    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets parameter controlling the number of baseline averages.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoNumberBaselineAverages(int detChan, void *value, XiaDefaults *defaults)
{
    int status;
    int statusX;

    double numBase;

    parameter_t LOG2BWEIGHT;

    numBase = *((double *) value);

    /* Make some corrections and guesses for the user */
    if (numBase < 1.) numBase = 1.;
    if (numBase > pow(2.,15.)) numBase = pow(2.,15.);

    /* currently the number of averages must be a factor of 2 for the G200 */
    LOG2BWEIGHT = (parameter_t) ROUND(log(numBase) / log(2.));

    /* Let the user know the corrected value */
    *((double *) value) = pow(2., (double) LOG2BWEIGHT);

    /* set the value of the baseline averages in the defaults */
    status = pslSetDefault("baseline_filter_length", value, defaults);

    /* Now do the calculation for the DSP parameter */
    if (LOG2BWEIGHT != 0)
	  {
		LOG2BWEIGHT = (parameter_t) (65536 - LOG2BWEIGHT);
	  }
	
    /* Write the value to the module */
    statusX = dxp_set_one_dspsymbol(&detChan, "LOG2BWEIGHT0", &LOG2BWEIGHT);
    if (statusX != DXP_SUCCESS)
	  {
		status = XIA_XERXES;
		pslLogError("pslDoNumberbaselineAverages", "Unable to set the DSP symbol LOG2BWEIGHT0 using XerXes", status);
		return status;
	  }

    sprintf(info_string, "New baseline averages is %lf\n", *((double *)value));
    pslLogDebug("pslDoNumberBaselineAverages", info_string);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets a parameter bit of the CHANCSRA.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoSetChanCSRABit(int detChan, char *name, void *value, 
					   parameter_t bit, boolean_t isset, XiaDefaults *defaults)
{
    int status;

    status = pslModifyCcsraBit(detChan, isset, bit, defaults);
    if (status != XIA_SUCCESS)
    {
	sprintf(info_string, "Unable to modify bit 0x%x in the defaults", bit);
	pslLogError("pslDoSetChanCSRABit", info_string, status);
	return status;
    }

    /* set the value of the variable in the defaults */
    status = pslSetDefault(name, value, defaults);
    if (status != XIA_SUCCESS)
    {
	sprintf(info_string, "Unable to set %s to %lf in the defaults", name, *((double *) value));
	pslLogError("pslDoSetChanCSRABit", info_string, status);
	return status;
    }

    /* If the level one buffer bit changed, then update MAXEVENTS */
    if (bit == MCSRA_LEVEL_ONE) 
    {
	status = pslSetMaxEvents(detChan, defaults);
	if (status != XIA_SUCCESS)
	{
	    pslLogError("pslDoSetChanCSRABit", "Unable to set the maximum events", status);
	    return status;
	}
    }

    sprintf(info_string, "Modifying CHANCSRA bit 0x%x to value %u\n", bit, isset);
    pslLogDebug("pslDoSetChanCSRABit", info_string);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets a parameter bit of the MODCSRA.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoSetModCSRABit(int detChan, char *name, void *value, 
					  parameter_t bit, boolean_t isset, XiaDefaults *defaults)
{
    int status;

    status = pslModifyMcsraBit(detChan, isset, bit);
    if (status != XIA_SUCCESS)
    {
	sprintf(info_string, "Unable to modify bit 0x%x in the defaults", bit);
	pslLogError("pslDoSetModCSRABit", info_string, status);
	return status;
    }

    /* set the value of the varaible in the defaults */
    status = pslSetDefault(name, value, defaults);
    if (status != XIA_SUCCESS)
    {
	sprintf(info_string, "Unable to set %s to %lf in the defaults", name, *((double *) value));
	pslLogError("pslDoSetModCSRABit", info_string, status);
	return status;
    }

    /* If the level one buffer bit changed, then update MAXEVENTS */
    if (bit == MCSRA_LEVEL_ONE) 
	  {
		status = pslSetMaxEvents(detChan, defaults);
		if (status != XIA_SUCCESS)
		  {
			pslLogError("pslDoSetModCSRABit", "Unable to set the maximum events", status);
			return status;
		  }
	  }
  
    sprintf(info_string, "Modifying MODCSR bit 0x%x to value %u\n", bit, isset);
    pslLogDebug("pslDoSetModCSRABit", info_string);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets the run type (RUNTASK) and the MAXEVENTS.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoRunType(int detChan, void *value, XiaDefaults *defaults)
{
    int status;
    int statusX;
    int runType;
    int runTypeMax = 4;

    parameter_t RUNTASK;
    parameter_t MAXEVENTS;
  
    double ddummy;

    boolean_t levelOneBuffer;

    runType = (int) *((double *) value);
    /* Check for a valid run type */
    if ((runType < 0) && (runType > runTypeMax)) 
	  { 
		status = XIA_UNDEFINED_RUN_TYPE;
		sprintf(info_string, "Unknown run type requested (runType = %i)", (int) runType);
		pslLogError("pslDoRunType", info_string, status);
		return status;
	  }
	
    /* Type ok, set the default list member */
    status = pslSetDefault("run_type", value, defaults);
    if (status != XIA_SUCCESS)
	  { 
		pslLogError("pslDoRunType", "Unable to set run_type in the defaults list", status);
		return status;
	  }
	
    /* Set the RUNTASK parameter */
    switch (runType) 
	  {
	  case 0:
		/* MCA mode */
		RUNTASK = MCA_RUN;
		levelOneBuffer = TRUE_;
		MAXEVENTS = 0;
		break;
	  case 1:
		/* LIST mode */
		RUNTASK = DAQ_RUN;
		levelOneBuffer = FALSE_;
		break;
	  case 2:
		/* Compressed LIST mode */
		RUNTASK = DAQ_RUN;
		levelOneBuffer = TRUE_;
		break;
	  case 3:
		/* Fast LIST mode */
		RUNTASK = FAST_LIST_MODE_RUN;
		levelOneBuffer = FALSE_;
		break;
	  case 4:
		/* Compressed Fast LIST mode */
		RUNTASK = FAST_LIST_MODE_RUN;
		levelOneBuffer = TRUE_;
		break;
	  default:
		status = XIA_UNDEFINED_RUN_TYPE;
		sprintf(info_string, "Unknown run type requested (runType = %i) (bounds check should catch this!)", 
				(int) runType);
		pslLogError("pslDoRunType", info_string, status);
		return status;
		break;
	  }
	
    /* Set the RUNTASK parameter on the modules */
    statusX = dxp_set_one_dspsymbol(&detChan, "RUNTASK", &RUNTASK);
    if (statusX != DXP_SUCCESS)
	  {
		status = XIA_XERXES;
		pslLogError("pslDoRunType", "Error setting RUNTASK with XerXes", status);
		return status;
	  }

    /* Set the Module CSR bit */
    ddummy = 0.0;
    if (levelOneBuffer) ddummy = 1.0;
    status = pslDoSetModCSRABit(detChan, "write_level_one_buffer", (void *) &ddummy, MCSRA_LEVEL_ONE, 
								levelOneBuffer, defaults);
    if (status != XIA_SUCCESS)
	  {
		pslLogError("pslDoRunType", "Unable to set the Module CSR bit", status);
		return status;
	  }
	
    sprintf(info_string, "Modifying runType to %i\n", runType);
    pslLogDebug("pslDoRunType", info_string);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine computes the TRACKDAC value from an offset in V and sets the 
 * appropriate DACs.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoOffset(int detChan, void *value, XiaDefaults *defaults)
{
    int status;
    int statusX;

    double tdacGain = TDAC_GAIN;
    double trackDAC;
    double offset;
    double minTDAC;
    double maxTDAC;

    parameter_t TRACKDAC;

    /* Convenience */
    offset = *((double *) value);

    /* Calculate MIN and MAX offsets (Note that the Volt limits are inverse to the TRACKDAC limits) */
    minTDAC = (1. - (TDAC_MAX / (2. * 3. * tdacGain))) * 32768.;
    maxTDAC = (1. - (TDAC_MIN / (2. * 3. * tdacGain))) * 32768.;

    /* Calculate the TRACKDAC setting */
    trackDAC = (1. - (offset / (2. * 3. * tdacGain))) * 32768.;

    /* Perform limits checks */
    if (trackDAC > maxTDAC) trackDAC = maxTDAC;
    if (trackDAC < minTDAC) trackDAC = minTDAC;

    TRACKDAC = (parameter_t) ROUND(trackDAC);

    /* Set the DSP Parameter */
    statusX = dxp_set_one_dspsymbol(&detChan, "TRACKDAC0", &TRACKDAC);

    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	pslLogError("pslDoOffset", "Unable to set the parameter TRACKDAC0 using XerXes", status);
	return status;
    }

    /* Now calculate the actual value of the Offset (could change due to limits testing) */
    status = pslCalculateOffset(TRACKDAC, &offset);

    if (status != XIA_SUCCESS)
    {
	sprintf(info_string, "Error calculating the offset from a TRACKDAC value of %#x", TRACKDAC);
	pslLogError("pslDoOffset", info_string, status);
	return status;
    }

    /* Set the return value */
    *((double *) value) = offset;

    /* Set the acquisition Value */
    status = pslSetDefault("offset", value, defaults);
    if (status != XIA_SUCCESS)
    {
	pslLogError("pslDoOffset", "Unable to set the offset in the defaults", status);
	return status;
    }

    /* Perform the SETDACs special task to actually change the offset */
    status = pslDoControlTask(detChan, CT_DGFG200_SETDACS, NULL);
    if (status != XIA_SUCCESS) 
    {
	sprintf(info_string, "Unable to set the DAC values on the module");
	pslLogError("pslDoOffset", info_string, status);
	return status;
    }

    sprintf(info_string, "New Offset is %lf, TRACKDAC = %#x", *((double *)value), TRACKDAC);
    pslLogDebug("pslDoOffset", info_string);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets the preamplifier gain and recomputes the dynamic range.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoPreampGain(int detChan, void *value, XiaDefaults *defaults, 
									   Detector *detector, int detector_chan, double gainScale)
{
    int status;

	double calibrationEnergy = 0.0;

	/* Set the value in the detector structure */
	detector->gain[detector_chan] = *((double *) value);

    /* Set the acquisition Value */
    status = pslSetDefault("preamp_gain", value, defaults);
    if (status != XIA_SUCCESS)
	  {
		pslLogError("pslDoPreampGain", "Unable to set the preamp_gain in the defaults", status);
		return status;
	  }

    /* Get the current calibration energy */
    status = pslGetDefault("calibration_energy", (void *) &calibrationEnergy, defaults);
    if (status != XIA_SUCCESS)
	  {
		pslLogError("pslDoPreampGain", "Unable to get the calibration_energy in the defaults", status);
		return status;
	  }

    /* Set the calibration energy in order to adjust the gain for the new preamp gain */
    status = pslDoCalibrationEnergy(detChan, (void *) &calibrationEnergy, defaults, 
									gainScale, detector, detector_chan);
    if (status != XIA_SUCCESS) 
	  {
		pslLogError("pslDoPreampGain", "Error setting the calibration energy", status);
		return status;
	  }

    sprintf(info_string, "New preamp_gain is %lf", *((double *)value));
    pslLogDebug("pslDoPreampGain", info_string);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets the decay time.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoDecayTime(int detChan, void *value, XiaDefaults *defaults, 
									  Detector *detector, int detector_chan)
{
    int status;

	/* Set the value in the detector structure */
	detector->typeValue[detector_chan] = *((double *) value);

    /* Set the acquisition Value */
    status = pslSetDefault("decay_time", value, defaults);
    if (status != XIA_SUCCESS)
	  {
		pslLogError("pslDoDecayTime", "Unable to set the decay_time in the defaults", status);
		return status;
	  }

    /* Set the DSP parameters */
    status = pslSetDetectorTypeValue(detChan, detector, detector_chan, defaults);
    if (status != XIA_SUCCESS)
	  {
		pslLogError("pslDoDecayTime", "Unable to set the type value for decay_time", status);
		return status;
	  }

    sprintf(info_string, "New decay time is %lf", *((double *)value));
    pslLogDebug("pslDoDecayTime", info_string);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets the detector polarity.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoDetectorPolarity(int detChan, void *value, XiaDefaults *defaults,
											 Detector *detector, int detector_chan)
{
    int status;

	if ((*((double *) value) <0) || (*((double *) value) >1)) 
	  {
		status = XIA_UNKNOWN_VALUE;
		sprintf(info_string, "Illegal value for the polarity: %f", *((double *) value));
		pslLogError("pslDoDetectorPolarity", info_string, status);
		return status;
	  }

	/* Set the value in the detector structure */
	detector->polarity[detector_chan] = (unsigned short) ROUND(*((double *) value));

    /* Set the Polarity */
    status = pslSetPolarity(detChan, detector, detector_chan, defaults);
    if (status != XIA_SUCCESS)
	  {
		pslLogError("pslDoDetectorPolarity", "Unable to set the polarity", status);
		return status;
	  }

    /* Set the acquisition Value */
    status = pslSetDefault("detector_polarity", value, defaults);
    if (status != XIA_SUCCESS)
	  {
		pslLogError("pslDoDetectorPolarity", "Unable to set the detector_polarity in the defaults", status);
		return status;
	  }

    sprintf(info_string, "New detector polarity is %lf", *((double *)value));
    pslLogDebug("pslDoDetectorPolarity", info_string);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine computes the offset in Volts from a TRACKDAC value .
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslCalculateOffset(parameter_t TRACKDAC, double *offset)
{
    double tdacGain = TDAC_GAIN;

    /* Calculate the offset */
    *offset = (1. - (((double) TRACKDAC) / 32768.)) * 2. * 3. * tdacGain; 

    /* Bounds checking */
    if (*offset > TDAC_MAX) *offset = TDAC_MAX;
    if (*offset < TDAC_MIN) *offset = TDAC_MIN;

    sprintf(info_string, "New Offset is %lf, TRACKDAC = %#x", *offset, TRACKDAC);
    pslLogDebug("pslCalculateOffset", info_string);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine computes the maximum number of events allowed per run.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslSetMaxEvents(int detChan, XiaDefaults *defaults)
{
    int status;
    int statusX;
  
    unsigned int eventLen;
    unsigned int length;
    unsigned int chanHeadLen;
  
    double goodChannel;
    double levelOneBuffer;

    parameter_t TRACELENGTH;
    parameter_t MAXEVENTS;
    parameter_t RUNTASK;
    parameter_t EVENTHEADLEN;
    parameter_t BUFHEADLEN;

    /* First need to retrieve the event buffer length */
    statusX = dxp_nevent(&detChan, &eventLen);
    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	pslLogError("pslSetMaxEvents", "Unable to retrieve the Event Buffer Length", status);
	return status;
    }

    /* Retrieve the level one buffer state */
    status = pslGetDefault("write_level_one_buffer", (void *) &levelOneBuffer, defaults);
    if (status != XIA_SUCCESS)
    {
	pslLogError("pslSetMaxEvents", "Unable to retrieve write_level_one_buffer from defaults", status);
	return status;
    }

    /* Retrieve the good channel state */
    status = pslGetDefault("good_channel", (void *) &goodChannel, defaults);
    if (status != XIA_SUCCESS)
    {
	pslLogError("pslSetMaxEvents", "Unable to retrieve good_channel from defaults", status);
	return status;
    }
  
    /* Get the bit in the TRACELENGTH on the module */
    statusX = dxp_get_one_dspsymbol(&detChan, "TRACELENGTH0", &TRACELENGTH);
    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	pslLogError("pslComputeMaxEvents", "Error getting TRACELENGTH with XerXes", status);
	return status;
    }

    /* Get the bit in the RUNTASK on the module */
    statusX = dxp_get_one_dspsymbol(&detChan, "RUNTASK", &RUNTASK);
    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	pslLogError("pslComputeMaxEvents", "Error getting RUNTASK with XerXes", status);
	return status;
    }

    /* Get the bit in the EVENTHEADLEN on the module */
    statusX = dxp_get_one_dspsymbol(&detChan, "EVENTHEADLEN", &EVENTHEADLEN);
    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	pslLogError("pslComputeMaxEvents", "Error getting EVENTHEADLEN with XerXes", status);
	return status;
    }

    /* Get the bit in the BUFHEADLEN on the module */
    statusX = dxp_get_one_dspsymbol(&detChan, "BUFHEADLEN", &BUFHEADLEN);
    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	pslLogError("pslComputeMaxEvents", "Error getting BUFHEADLEN with XerXes", status);
	return status;
    }

    /* If RUNTASK is set for MCA run, then MAXEVENTS = 0, skip the whole calculation */
    MAXEVENTS = 0;
    if (RUNTASK != MCA_RUN)
    {
	/* Start the length calculation with the event header length */
	length = EVENTHEADLEN;
      
	/* If this is a good channel (always should be true for G200) and we are not using the 
	 * level one buffers, then add the header and trace length to the used buffer space
	 */
	if ((goodChannel != 0.0) && (levelOneBuffer == 0.0))
	{
	    /* Set the channel header length */
	    chanHeadLen = 9;
	    if (levelOneBuffer != 0.0) chanHeadLen = 2;
	  
	    /* increment the length */
	    length += chanHeadLen;
	    length += (unsigned int) TRACELENGTH;
	}
      
	/* First check for overrun when using the level one buffer */
	if ((levelOneBuffer != 0.0)  &&  (length >= (unsigned int) INTERNAL_BUFFER_LENGTH))
	{
	    status = XIA_INTERNAL_BUFFER_OVERRUN;
	    sprintf(info_string, "Overran internal buffer with an event length of %i, buffer space = %i",
		    length, INTERNAL_BUFFER_LENGTH);
	    pslLogError("pslComputeMaxEvents", info_string, status);
	    return status;
	}
      
	/* Now check for overruns in the event buffer */
	if ((levelOneBuffer == 0.0)  &&  (length > eventLen))
	{
	    status = XIA_EVENT_BUFFER_OVERRUN;
	    sprintf(info_string, "Overran event buffer with an event length of %i, buffer space = %i",
		    length, eventLen);
	    pslLogError("pslComputeMaxEvents", info_string, status);
	    return status;
	}
      
	/* Compute the maximum number of allowed events */
	MAXEVENTS = (parameter_t) floor((eventLen - BUFHEADLEN) / length);
    } 

    /* Set the MAXEVENTS parameter on the modules */
    statusX = dxp_set_one_dspsymbol(&detChan, "MAXEVENTS", &MAXEVENTS);
    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	pslLogError("pslSetMaxEvents", "Error setting MAXEVENTS with XerXes", status);
	return status;
    }

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine modifies the value of the CHANCSRA variable.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslModifyCcsraBit(int detChan, boolean_t isset, parameter_t bit, XiaDefaults *defaults)
{
    int status;
    int statusX;

    parameter_t CHANCSRA;

	UNUSED(defaults);

    /* Set the bit in the CHANCSRA on the module */
    statusX = dxp_get_one_dspsymbol(&detChan, "CHANCSRA0", &CHANCSRA);

    if (statusX != DXP_SUCCESS)
	  {
		status = XIA_XERXES;
		pslLogError("pslModifyCcsraBit", "Error getting CHANCSRA with XerXes", status);
		return status;
	  }

    sprintf(info_string, "prior: set? = %u, bit = %u, chancsra = %x", isset, bit, CHANCSRA);
    pslLogDebug("pslModifyCcsraBit", info_string);

    if (isset) 
	  {
		CHANCSRA |= bit;
	  } else {
		CHANCSRA &= ~bit;
	  }
	
    sprintf(info_string, "post: set? = %u, bit = %u, chancsra = %x", isset, bit, CHANCSRA);
    pslLogDebug("pslModifyCcsraBit", info_string);

    /* Write the value to the module */
    statusX = dxp_set_one_dspsymbol(&detChan, "CHANCSRA0", &CHANCSRA);
	
    if (statusX != DXP_SUCCESS)
	  {
		status = XIA_XERXES;
		pslLogError("pslModifyCcsraBit", "Error setting CHANCSRA0 with XerXes", status);
		return status;
	  }
	
	/* And update the FiPPi Settings */
	status = pslDoControlTask(detChan, CT_DGFG200_PROGRAM_FIPPI, NULL);
	if (status != XIA_SUCCESS)
	 {
	   pslLogError("pslModifyCcsraBit", "Unable to execute the program_fippi control task", status);
	   return status;
	 }

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine modifies the value of the MODCSRA variable.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslModifyMcsraBit(int detChan, boolean_t isset, parameter_t bit)
{
    int status;
    int statusX;

    parameter_t MODCSRA;

    /* Set the bit in the MODCSRA on the module */
    statusX = dxp_get_one_dspsymbol(&detChan, "MODCSRA", &MODCSRA);
    if (statusX != DXP_SUCCESS)
	  {
		status = XIA_XERXES;
		pslLogError("pslModifyMcsraBit", "Error getting MODCSRA with XerXes", status);
		return status;
	  }
	
    if (isset) 
	  {
		MODCSRA |= bit;
	  } else {
		MODCSRA &= ~bit;
	  }
	
    /* Write the value to the module */
    statusX = dxp_set_one_dspsymbol(&detChan, "MODCSRA", &MODCSRA);
    if (statusX != DXP_SUCCESS)
	  {
		status = XIA_XERXES;
		pslLogError("pslModifyMcsraBit", "Error setting MODCSRA with XerXes", status);
		return status;
	  }

	/* And update the FiPPi Settings */
	status = pslDoControlTask(detChan, CT_DGFG200_PROGRAM_FIPPI, NULL);
	if (status != XIA_SUCCESS)
	 {
	   pslLogError("pslModifyMcsraBit", "Unable to execute the program_fippi control task", status);
	   return status;
	 }

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine gets the specified acquisition value from either the defaults
 * or from on-board parameters. I anticipate that this routine will be 
 * much less complicated then setAcquisitionValues so I will do everything
 * in a single routine.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetAcquisitionValues(int detChan, char *name,
											   void *value, 
											   XiaDefaults *defaults)
{
    int status;


	UNUSED(detChan);


    status = pslGetDefault(name, value, defaults);

    if (status != XIA_SUCCESS) 
    {
	status = XIA_UNKNOWN_VALUE;
	sprintf(info_string, "Unable to find the Acquisition value = %s", name);
	pslLogError("pslGetAcquisitionValues", info_string, status);
	return status;
    }
  
    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine computes the value of GAINDAC based on the input values. 
 * Also handles scaling due to the "discreteness" of BINFACT1. I'll try and 
 * illustrate the equations as we go along, but the best reference is 
 * probably found external to the program.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslCalculateGain(double adcPercentRule, double calibEV, 
                                        double preampGain, parameter_t *GAINDAC, 
					double gainScale, boolean_t *outOfRange)
{
    double gSystem;
    double gTotal;
    double gBase = 1.0;
    double gVar;
    double gDB;
    double inputRange = ADC_RANGE;
    double gaindacDB = GAINDAC_RANGE_DB;
    double gGAINDAC;
    double gaindacBits = (double) GAINDAC_BITS;

    /* Initialize */
    *outOfRange = FALSE_;

    gSystem = pslCalculateSysGain();

    gSystem *= gainScale;

    gTotal = ((adcPercentRule / 100.0) * inputRange) / ((calibEV / 1000.0) * preampGain);

    gVar = gTotal / (gSystem * gBase);

    /* Now we can start converting to GAINDAC */
    gDB = (20.0 * log10(gVar));
	
    /* Keep the GAINDAC within the allowed range */
    if (gDB < GAINDAC_MIN) 
    {
	gDB = GAINDAC_MIN;
	*outOfRange = TRUE_;
    }
    if (gDB > GAINDAC_MAX) 
    {
	gDB = GAINDAC_MAX;
	*outOfRange = TRUE_;
    }

    gDB += 10.0;

    gGAINDAC = gDB * ((double)(pow(2., gaindacBits) / gaindacDB));

    if (gGAINDAC > 65535.) gGAINDAC = 65535;

    *GAINDAC = (parameter_t) (0xFFFF - (parameter_t) ROUND(gGAINDAC));

    sprintf(info_string, "Calculated gain: adcPercentRule = %lf, inputRange = %lf, calibEV = %lf, preampGain = %lf", 
	    adcPercentRule, inputRange, calibEV, preampGain);
    pslLogDebug("pslCalculateGain", info_string);

    sprintf(info_string, "Calculated gain: gTotal = %lf, gVar = %lf, gDB = %lf, GAINDAC = 0x%x", 
	    gTotal, gVar, gDB, *GAINDAC);
    pslLogDebug("pslCalculateGain", info_string);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine converts a GAINDAC value to a dynamic range
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslCalculateDynamicRange(double systemGain, double preampGain, 
						double gainScale, XiaDefaults *defaults, 
						double *dynamicRange)
{
  int status;
  
  double inputRange = ADC_RANGE;
  double baselinePercentRule = 10.;
  
  double dtemp;
  
  /* Quiet the compiler */
  dtemp = gainScale;
  
  /* Grab the baseline percent rule from the defaults */
  status = pslGetDefault("baseline_percent_rule", (void *) &baselinePercentRule, defaults);
  if (status != XIA_SUCCESS) 
    {
	  sprintf(info_string, "Unable to retrieve the baseline_percent_rule from the defaults");
	  pslLogError("pslCalculateDynamicRange", info_string, status);
	  return status;
    }
  
  *dynamicRange = (1. - (baselinePercentRule / 100.)) / systemGain * inputRange / (preampGain / 1000.);
  
  sprintf(info_string, "baseline percent: %lf, systemgain: %lf, input range: %lf, pamp: %lf", 
	  baselinePercentRule, systemGain, inputRange, preampGain);
  pslLogDebug("pslCalculateDynamicRange", info_string);
  sprintf(info_string, "Calculated dynamic range: %lf", *dynamicRange);
  pslLogDebug("pslCalculateDynamicRange", info_string);
  
  return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine calculates the Calibration Energy given a GAINDAC setting,
 * this is the inverse routine for calculateGain, which allows Handel to 
 * determine what the final value of CalibrationEnergy/DynamicRange was 
 * chosen
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslCalculateCalibrationEnergy(double adcPercentRule, double *calibEV, 
						     double preampGain, parameter_t GAINDAC, 
						     double gainScale)
{
    double gSystem;
    double gTotal;
    double gBase = 1.0;
    double gVar;
    double gDB;
    double inputRange = ADC_RANGE;
    double gaindacDB = GAINDAC_RANGE_DB;
    double gaindacBits = (double) GAINDAC_BITS;

    /* Determine the gain in DB */
    gDB = (0xFFFF - GAINDAC) / ((double) (pow(2., gaindacBits) / gaindacDB));

    /* Offset DB back to natural range of -10 to 30 */
    gDB -= 10.0;

    /* Calculate the variable linear gain */
    gVar = pow(10., (gDB / 20.));

    /* Get the system gain value */
    gSystem = pslCalculateSysGain();
    gSystem *= gainScale;

    /* Calculate the Total gain */
    gTotal = gVar * gSystem * gBase;

    /* Pull the calibration energy out */
    *calibEV = ((adcPercentRule / 100.0) * inputRange) / (gTotal * (preampGain / 1000.) );

    sprintf(info_string, "Calculated gain: adcPercentRule = %lf, inputRange = %lf, calibEV = %lf, preampGain = %lf", 
	    adcPercentRule, inputRange, *calibEV, preampGain);
    pslLogDebug("pslCalculateCalibrationEnergy", info_string);

    sprintf(info_string, "Calculated gain: gTotal = %lf, gVar = %lf, gDB = %lf, GAINDAC = 0x%x", 
	    gTotal, gVar, gDB, GAINDAC);
    pslLogDebug("pslCalculateCalibrationEnergy", info_string);

    sprintf(info_string, "Calculated calibration energy: calibEV = %lf", *calibEV);
    pslLogDebug("pslCalculateCalibrationEnergy", info_string);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine calculates the preampGain given the GAINDAC value and 
 * calibration energy
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslCalculatePreampGain(double adcPercentRule, double calibEV, 
					      double *preampGain, parameter_t GAINDAC, 
					      double gainScale)
{
    double gSystem;
    double gTotal;
    double gBase = 1.0;
    double gVar;
    double gDB;
    double inputRange = ADC_RANGE;
    double gaindacDB = GAINDAC_RANGE_DB;
    double gaindacBits = (double) GAINDAC_BITS;

    /* Determine the gain in DB */
    gDB = (0xFFFF - GAINDAC) / ((double) (pow(2., gaindacBits) / gaindacDB));

    /* Offset DB back to natural range of -10 to 30 */
    gDB -= 10.0;

    /* Calculate the variable linear gain */
    gVar = pow(10., (gDB / 20.));

    /* Get the system gain value */
    gSystem = pslCalculateSysGain();
    gSystem *= gainScale;

    /* Calculate the Total gain */
    gTotal = gVar * gSystem * gBase;

    /* Pull the calibration energy out */
    *preampGain = ((adcPercentRule / 100.0) * inputRange) / (gTotal * (calibEV / 1000.) );

    sprintf(info_string, "Calculated pampgain: adcPercentRule = %lf, inputRange = %lf, calibEV = %lf, preampGain = %lf", 
	    adcPercentRule, inputRange, calibEV, *preampGain);
    pslLogDebug("pslCalculatePreampGain", info_string);

    sprintf(info_string, "Calculated pampgain: gTotal = %lf, gVar = %lf, gDB = %lf, GAINDAC = 0x%x", 
	    gTotal, gVar, gDB, GAINDAC);
    pslLogDebug("pslCalculatePreampGain", info_string);

    sprintf(info_string, "Calculated Preamp Gain: preampGain = %lf", *preampGain);
    pslLogDebug("pslCalculatePreampGain", info_string);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This calculates the system gain due to the analog portion of the system.
 *
 *****************************************************************************/
PSL_STATIC double PSL_API pslCalculateSysGain(void)
{
    double gInputBuffer    = GINPUT_BUFF;
    double gInvertingAmp   = GINVERTING_AMP;
    double gVoltageDivider = GV_DIVIDER;
    double gGaindacBuffer  = GGAINDAC_BUFF;
    double gNyquist        = GNYQUIST;
    double gADCBuffer      = GADC_BUFF;
    double gADC            = GADC;

    double gSystem;

    gSystem = gInputBuffer * gInvertingAmp * gVoltageDivider * 
	gGaindacBuffer * gNyquist * gADCBuffer * gADC;

    return gSystem;
}

/*****************************************************************************
 *
 * This routine sets the clock speed for the G200 board. Eventually, we'd 
 * like to read this info. from the interface. For now, we will set it
 * statically.
 *
 *****************************************************************************/
PSL_STATIC double PSL_API pslGetClockSpeed(int detChan)
{
  /*  int statusX = DXP_SUCCESS;
  
  parameter_t SYSMICROSEC = 0;*/

  UNUSED(detChan);

  /* Get the clock speed from the DSP */
  /*  statusX = dxp_get_one_dspsymbol(&detChan, "SYSMICROSEC", &SYSMICROSEC);*/
  
  /*  if (statusX != DXP_SUCCESS)
    {
      sprintf(info_string, "Error getting SYSMICROSEC for detChan %d", detChan);
      pslLogError("pslGetClockSpeed", info_string, XIA_XERXES);*/
      /* Default the clock speed to 40 MHz for the G200 */
      return 40.;
	  /*    }*/
  
	  /*  return (double) SYSMICROSEC;*/
}


/*****************************************************************************
 *
 * This is a generic gain operation routine, it passes information out to 
 * other routines.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGainOperation(int detChan, char *name, void *value, Detector *detector, 
										int detector_chan, XiaDefaults *defaults, double gainScale, 
										CurrentFirmware *currentFirmware, char *detectorType,
										Module *m)
{
    int status = XIA_SUCCESS;

    /* This routine just calls other routines as determined by name */
    if (STREQ(name, "scale_preamp_gain")) 
	  {
		status = pslScalePreamp(detChan, detector, detector_chan, defaults, 
								*((double *) value), gainScale);
		
	  } else if (STREQ(name, "calibrate_gain")) {
		
		status = pslGainCalibrate(detChan, detector, detector_chan, defaults, 
								  *((double *) value), gainScale);
		
	  } else if (STREQ(name, "change_gain")) {
		
		status = pslGainChange(detChan, *((double *) value), defaults, 
							   currentFirmware, detectorType, gainScale, 
							   detector, detector_chan, m);
		
	  } else {
		
		status = XIA_BAD_NAME;
		sprintf(info_string, "%s is either an unsupported gain operation or a bad name for detChan %d",
				name, detChan);
		pslLogError("pslGainOperation", info_string, status);
		return status;
		
	  }
	
    if (status != XIA_SUCCESS)
	  {
		sprintf(info_string, "Unable to perform the following gain operation: %s on detChan %d", name, detChan);
		pslLogError("pslGainOperation", info_string, status);
	  }
	
    return status;
}

/*****************************************************************************
 *
 * This routine adjusts the gain by keeping the GAINDAC constant, but changing 
 * the preamp gain and the dynamic range.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslScalePreamp(int detChan, Detector *detector, int detector_chan, 
				      XiaDefaults *defaults, double deltaGain, double gainScale)
{
    int status;

    double preampGain = 0.0;
    double dynamicRange = 0.0;
	/*    double systemGain = 0.0;*/
	double threshold = 0.0;

	UNUSED(gainScale);

    /* scale the preamp Gain by the desired factor */
    detector->gain[detector_chan] /= deltaGain;
    preampGain = detector->gain[detector_chan];
	
    /* Set the preamp gain */
    status = pslSetDefault("preamp_gain", (void *) &preampGain, defaults);
    if (status != XIA_SUCCESS)
	  {
		pslLogError("pslScalePreamp", "Unable to set the preamp_gain acquisition value", status);
		return status;
	  }
	
    /* Now measure the gain */
    /*    status = pslMeasureSystemGain(detChan, NULL, defaults);
    if (status != XIA_SUCCESS) 
    {
	sprintf(info_string, "Unable to measure the system Gain");
	pslLogError("pslScalePreamp", info_string, status);
	return status;
    }
    */
      
    /* Retrieve the System Gain */
    /*status = pslGetDefault("system_gain", (void *) &systemGain, defaults);
    if (status != XIA_SUCCESS)
	  {
		pslLogError("pslScalePreamp", "Unable to get the system_gain acquisition value", status);
		return status;
	  }
    */	
    /* Now calculate the new Dynamic Range */
    /*    status = pslCalculateDynamicRange(systemGain, preampGain, gainScale, defaults, &dynamicRange);
     */
    /* Get the Dynamic Range */
    status = pslGetDefault("dynamic_range", (void *) &dynamicRange, defaults);
    if (status != XIA_SUCCESS)
      {
		pslLogError("pslScalePreamp", "Unable to get the dynamic_range acquisition value", status);
		return status;
      }
	
    dynamicRange *= deltaGain;

    /* Set the Dynamic Range */
    status = pslSetDefault("dynamic_range", (void *) &dynamicRange, defaults);
    if (status != XIA_SUCCESS)
      {
		pslLogError("pslScalePreamp", "Unable to set the dynamic_range acquisition value", status);
		return status;
      }
	
	/* Want to recalculate the trigger threshold as well now.
	 */
	status = pslGetDefault("trigger_threshold", (void *) &threshold, defaults);
	if (status != XIA_SUCCESS)
	  {
		pslLogError("pslScalePreamp", "Unable to retrieve the trigger_threshold from the defaults", status);
		return status;
	  }
	
	threshold *= deltaGain;

	/* use the set acquisition values routine to set the new threshold */
	status = pslDoTriggerThreshold(detChan, (void *) &threshold, defaults);
	
	if (status != XIA_SUCCESS)
	  {
		pslLogError("pslScalePreamp", "Failed to set the trigger threshold", status);
		return status;
	  }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine adjusts the percent rule by deltaGain.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGainChange(int detChan, double deltaGain, 
                                     XiaDefaults *defaults, 
									 CurrentFirmware *currentFirmware, 
                                     char *detectorType, double gainScale,
									 Detector *detector, int detector_chan, Module *m)
{
    int status;

    double oldADCPercentRule;
    double newADCPercentRule;

    FirmwareSet *nullSet = NULL;

    /* Get the current ADC percent rule */
    status = pslGetDefault("adc_percent_rule", (void *) &oldADCPercentRule, defaults);
    /* modify and reset */
    newADCPercentRule = oldADCPercentRule * deltaGain;

    /* set the acquisition value using the standard channels */
    status = pslSetAcquisitionValues(detChan, "adc_percent_rule", (void *)&newADCPercentRule, 
									 defaults, nullSet, currentFirmware, 
									 detectorType, gainScale, detector, detector_chan, m);

    if (status != XIA_SUCCESS)
    {
	/* Try to reset the gain. If it doesn't work then you aren't really any
	 * worse off then you were before.
	 */
	pslSetAcquisitionValues(detChan, "adc_percent_rule", (void *)&oldADCPercentRule, 
							defaults, nullSet, currentFirmware, detectorType, 
							gainScale, detector, detector_chan, m);

      
	pslLogError("pslGainChange", "Unable to change the gain, tried to reset to old value", status);
	return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine adjusts the gain via. the preamp gain. As the name suggests, 
 * this is mostly for situations where you are trying to calibrate the gain
 * with a fixed eV/ADC, which should cover 99% of the situations, if not the
 * full 100%.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGainCalibrate(int detChan, Detector *detector, int detector_chan, 
					XiaDefaults *defaults, double deltaGain, double gainScale)
{
    int status;
    int statusX;

    double adcPercentRule = 0.0;
    double calibEV = 0.0;
    double preampGain = 0.0;

    boolean_t outOfRange = FALSE_;

    parameter_t GAINDAC;

    /* Retrieve the ADC percent rule and calibration energy */
    status = pslGetDefault("adc_percent_rule", (void *) &adcPercentRule, defaults);
    if (status != XIA_SUCCESS)
    {
	pslLogError("pslGainCalibrate", "Unable to get the adc_percent_rule acquisition value", status);
	return status;
    }

    status = pslGetDefault("calibration_energy", (void *) &calibEV, defaults);
    if (status != XIA_SUCCESS)
    {
	pslLogError("pslGainCalibrate", "Unable to get the calibration_energy acquisition value", status);
	return status;
    }

    /* n scaled preamp gain keeping in mind that it is an inverse
     * relationship.
     */
    detector->gain[detector_chan] /= deltaGain;
    preampGain = detector->gain[detector_chan];

    status = pslCalculateGain(adcPercentRule, calibEV, preampGain, &GAINDAC, gainScale, &outOfRange);

    if (status != XIA_SUCCESS)
    {
	/* If there is a problem here, then we probably need a way to 
	 * revert back to the old gain...
	 */
	pslLogError("pslGainCalibrate", "Problems calculating the gain, no resolution currently", status);
	return status;
    }

    statusX = dxp_set_one_dspsymbol(&detChan, "GAINDAC0", &GAINDAC);

    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	pslLogError("pslGainCalibrate", "Unable to set GAINDAC using XerXes", status);
	return status;
    }

    /* Now check to see if the GAINDAC was set out of range, if so, we need to correct the P-Amp gain
     * such that the Dynamic Range remains a constant after this calculation */
    if (outOfRange) 
    {
	status = pslCalculatePreampGain(adcPercentRule, calibEV, &preampGain, GAINDAC, gainScale);
	  
	if (status != XIA_SUCCESS)
	{
	    pslLogError("pslGainCalibrate", "Unable to determine the Preamp Gain", status);
	    return status;
	}
    }

    detector->gain[detector_chan] = preampGain;

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine is responsible for calling the XerXes version of start run.
 * There may be a problem here involving multiple calls to start a run on a 
 * module since this routine is iteratively called for each channel. May need
 * a way to start runs that takes the "state" of the module into account.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslStartRun(int detChan, unsigned short resume, XiaDefaults *defaults)
{
    int status;
    int statusX;

	double tmpGate = 0.0;

    unsigned short gate;
	

	status = pslGetDefault("enable_gate", (void *)&tmpGate, defaults);

    if (status != XIA_SUCCESS) 
	  {
		sprintf(info_string, 
				"Error getting enable_gate information for run start on detChan %d",
				detChan);
		pslLogError("pslStartRun", info_string, status);
		return status;
	  }

    gate = (unsigned short) ((tmpGate==1.0) ? 0 : 1);
	
    statusX = dxp_start_one_run(&detChan, &gate, &resume);

    if (statusX != DXP_SUCCESS)
	  {
		status = XIA_XERXES;
		pslLogError("pslStartRun", "Unable to start a run using XerXes", status);
		return status;
	  }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine is responsible for calling the XerXes version of stop run.
 * With some hardware, all channels on a given module may be "stopped" 
 * together. Not sure if calling stop multiple times do to the detChan 
 * iteration procedure will cause problems or not.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslStopRun(int detChan)
{
    int status;
    int statusX;
    int i;
    int runActive = 0;
    int timeout = 4000;

    /* Since dxp_md_wait() wants a time in seconds */
    float wait = 1.0f / 1000.0f;

    statusX = dxp_stop_one_run(&detChan);

    if (statusX != DXP_SUCCESS)
	  {
		status = XIA_XERXES;
		pslLogError("pslStopRun", "Unable to stop a run using XerXes", status);
		return status;
	  }

    /* Allow time for run to end */
    statusX = utils->funcs->dxp_md_wait(&wait);

    /* If run is truly ended, then BUSY should equal 0 */
    for (i = 0; i < timeout; i++)
    {
	  statusX = dxp_isrunning(&detChan, &runActive);
	  if (statusX != DXP_SUCCESS)
		{
		  status = XIA_XERXES;
		  pslLogError("pslStopRun", "Unable to determine if the run is active from XerXes", status);
		  return status;
		}
	  
	  if (runActive == 0)
		{
		  return XIA_SUCCESS;
		}
	  statusX = utils->funcs->dxp_md_wait(&wait);
	  if (statusX != DXP_SUCCESS)
		{
		  status = XIA_XERXES;
		  sprintf(info_string, "Unable to execute the MD function dxp_md_wait() for time = %f", wait);
		  pslLogError("pslStopRun", info_string, status);
		  return status;
		}
	  
    }
	
    /* decode the active state at the end, try to help the user */
    if ((runActive & 0x01) != 0)
	  {
		/* This means the DSP still has the run active */
		status = XIA_HARDWARE_RUN_ACTIVE;
		pslLogError("pslStopRun", "The G200 still has the run Active", status);
		return status;  
	  }
	
    if ((runActive & 0x02) != 0)
	  {
		/* This means that XerXes thinks a normal run is active */
		status = XIA_XERXES_NORMAL_RUN_ACTIVE;
		pslLogError("pslStopRun", "XerXes thinks a normal run is active", status);
		return status;  
	  }
	
    if ((runActive & 0x04) != 0)
	  {
		/* This means the DSP still has the run active */
		status = XIA_XERXES_CONTROL_RUN_ACTIVE;
		pslLogError("pslStopRun", "XerXes thinks a control run is active", status);
		return status;  
	  }
	
    /* Unknown cause */
    status = XIA_TIMEOUT;
    sprintf(info_string, "Timeout limit reached, unknown error: XerXes says RUNACTIVE = %i, detChan = %i", 
			runActive, detChan);
    pslLogError("pslStopRun", info_string, status);
    return status;  
}


/*****************************************************************************
 *
 * This routine retrieves the specified data from the board. In the case of
 * the G200 a run does not need to be stopped in order to retrieve the 
 * specified information.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetRunData(int detChan, char *name, void *value,
				     XiaDefaults *defaults)
{
    int status = XIA_SUCCESS;

    UNUSED(defaults);


    /* This routine just calls other routines as determined by name */
    if (STREQ(name, "mca_length")) 
    {
	status = pslGetMCALength(detChan, value);
      
    } else if (STREQ(name, "mca")) {
      
	status = pslGetMCAData(detChan, value);
      
    } else if (STREQ(name, "livetime")) {
      
	status = pslGetLivetime(detChan, value);
      
    } else if (STREQ(name, "runtime")) {
      
	status = pslGetRuntime(detChan, value);
      
    } else if (STREQ(name, "input_count_rate")) {
      
	status = pslGetICR(detChan, value);
      
    } else if (STREQ(name, "output_count_rate")) {
      
	status = pslGetOCR(detChan, value);
      
    } else if (STREQ(name, "compton_counts")) {
      
	status = pslGetComptonCounts(detChan, value);
      
    } else if (STREQ(name, "raw_compton_count")) {
      
	status = pslGetRawComptonCounts(detChan, value);
      
    } else if (STREQ(name, "events_in_run")) {

	status = pslGetEvents(detChan, value);

    } else if (STREQ(name, "triggers")) {
      
	status = pslGetTriggers(detChan, value);
      
    } else if (STREQ(name, "run_active")) {
      
	status = pslGetRunActive(detChan, value);
      
    } else if (STREQ(name, "list_mode_data_length")) {
       
	status = pslGetListModeDataLength(detChan, value);
      
    } else if (STREQ(name, "list_mode_data")) {
      
	status = pslGetListModeData(detChan, value);

    } else {

	status = XIA_BAD_NAME;
	sprintf(info_string, "%s is either an unsupported run data type or a bad name for detChan %d",
		name, detChan);
	pslLogError("pslGetRunData", info_string, status);
	return status;
      
    }
  
    if (status != XIA_SUCCESS)
    {
	sprintf(info_string, "Unable to get the following run data: %s from detChan %d", name, detChan);
	pslLogError("pslGetRunData", info_string, status);
    }

    return status;
}


/*****************************************************************************
 *
 * This routine sets value to the length of the MCA spectrum.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetMCALength(int detChan, void *value)
{
    int status;
    int statusX;

    statusX = dxp_nspec(&detChan, (unsigned int *) value);

    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	pslLogError("pslGetMCALength", "Unable to retrieve the MCA length from XerXes", status);
	return status;
    }

    sprintf(info_string, "Number of MCA channels = %u\n", *((unsigned int *) value));
    pslLogDebug("pslGetMCALength", info_string);

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine gets the MCA spectrum through XerXes.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetMCAData(int detChan, void *value)
{
    int status;
    int statusX;

    statusX = dxp_readout_detector_run(&detChan, NULL, NULL, (unsigned long *)value);

    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	pslLogError("pslGetMCAData", "Error retrieving the Spectrum from XerXes", status);
	return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine gets the length of the list mode data from XerXes.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetListModeDataLength(int detChan, void *value)
{
    int status;
    int statusX;

    /* List mode Data is stored in the same place as the ADC trace data */
    short task = CT_ADC;

    int info[3];

    statusX = dxp_control_task_info(&detChan, &task, info);
    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	pslLogError("pslGetListModeDataLength", "Unable to get length of List Mode Data from XerXes", status);
	return status;
    }

    *((unsigned long *) value) = (unsigned long) info[0];

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine gets the list mode data from XerXes.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetListModeData(int detChan, void *value)
{
    int status;
    int statusX;

    /* List mode Data is stored in the same place as the ADC trace data */
    short task = CT_ADC;

    statusX = dxp_get_control_task_data(&detChan, &task, value);

    if (statusX != DXP_SUCCESS)
	  {
		status = XIA_XERXES;
		pslLogError("pslGetListModeData", "Unable to get the List Mode Data from XerXes", status);
		return status;
	  }

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine reads the livetime back from the board.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetLivetime(int detChan, void *value)
{
   int status;
   int statusX;
   
   double livetime = 0.0;
   double icr      = 0.0;
   double ocr      = 0.0;
   
   unsigned long nevent = 0;
   
   statusX = dxp_get_statistics(&detChan, &livetime, &icr, &ocr,
				&nevent);
   
   if (statusX != DXP_SUCCESS) 
	 {
	   status = XIA_XERXES;
	   sprintf(info_string, "Error getting Run Statistics for detChan %d", detChan);
	   pslLogError("pslGetLivetime", info_string, status);
	   return status;
	 }
   
   *((double *)value) = livetime;

   sprintf(info_string, "Livetime = %lf\n", *((double *) value));
   pslLogDebug("pslGetLivetime", info_string);
   
   return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine reads the compton events.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetComptonCounts(int detChan, void *value)
{
    int status;
    int statusX;

    double dCOMPTON;

    statusX = dxp_get_dspsymbol(&detChan, "COMPTON", &dCOMPTON);

    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	pslLogError("pslGetComptonCounts", "Error retrieving the COMPTON information", status);
	return status;
    }

    *((double *)value) = dCOMPTON;

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine reads the compton events.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetRawComptonCounts(int detChan, void *value)
{
    int status;
    int statusX;

    double dRAWCOMPT;

    statusX = dxp_get_dspsymbol(&detChan, "RAWCOMPT", &dRAWCOMPT);

    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	pslLogError("pslGetRawComptonCounts", "Error retrieving the RAWCOMPT information", status);
	return status;
    }

    *((double *)value) = dRAWCOMPT;

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine reads the runtime back from the board.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetRuntime(int detChan, void *value)
{
  int status;
  int statusX;
  
  double runtime;
  
  parameter_t RUNTIMEA;
  parameter_t RUNTIMEB;
  parameter_t RUNTIMEC;
  
  /* If any of these fail then we can
   * return an error. However, the
   * failures shouldn't depend on each
   * other so it's okay to hold off on the
   * error checking until the end.
   */
  statusX  = dxp_get_one_dspsymbol(&detChan, "RUNTIMEA", &RUNTIMEA);
  statusX += dxp_get_one_dspsymbol(&detChan, "RUNTIMEB", &RUNTIMEB);
  statusX += dxp_get_one_dspsymbol(&detChan, "RUNTIMEC", &RUNTIMEC);
  
  if (statusX != DXP_SUCCESS) 
	{
	  status = XIA_XERXES;
	  sprintf(info_string, "Error getting runtime parameters for detChan %d",
			  detChan);
	  pslLogError("pslGetRuntime", info_string, status);
	  return status;
	}
  
  runtime = (double)((double)(RUNTIMEA * 65536. * 65536.) + 
					 (double)(RUNTIMEB * 65536.) + 
					 (double) RUNTIMEC) * 1.e-6 / pslGetClockSpeed(detChan);
  
  *((double *)value) = runtime;
  
  sprintf(info_string, "Runtime = %lf\n", *((double *) value));
  pslLogDebug("pslGetRuntime", info_string);
  
  return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine gets the Input Count Rate (ICR)
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetICR(int detChan, void *value)
{
  int status;
  int statusX;
  
  double livetime = 0.0;
  double ocr      = 0.0;
  double icr      = 0.0;
  
  unsigned long nevent = 0;
  
  
  statusX = dxp_get_statistics(&detChan, &livetime, &icr, &ocr, &nevent);
  
  if (statusX != DXP_SUCCESS) 
	{
	  status = XIA_XERXES;
	  sprintf(info_string, 
			  "Error geting Run Statistics info for detChan %d",
			  detChan);
	  pslLogError("pslGetICR", info_string, status);
	  return status;
	}
  
  *((double *)value) = icr;
  
  sprintf(info_string, "ICR = %lf\n", *((double *) value));
  pslLogDebug("pslGetICR", info_string);
  
  return XIA_SUCCESS;
}


/*****************************************************************************
 * 
 * This routine gets the Output Count Rate (OCR)
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetOCR(int detChan, void *value)
{
  int status;
  int statusX;
  
  double livetime = 0.0;
  double ocr      = 0.0;
  double icr      = 0.0;
  
  unsigned long nevent = 0;
  
  statusX = dxp_get_statistics(&detChan, &livetime, &icr, &ocr, &nevent);
  
  if (statusX != DXP_SUCCESS) 
	{
	  status = XIA_XERXES;
	  sprintf(info_string, 
			  "Error geting Run Statistics info for detChan %d",
			  detChan);
	  pslLogError("pslGetOCR", info_string, status);
	  return status;
	}
  
  *((double *)value) = ocr;
  
  sprintf(info_string, "OCR = %lf\n", *((double *) value));
  pslLogDebug("pslGetOCR", info_string);
  
  return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine gets the number of events in the run
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetEvents(int detChan, void *value)
{
   int status;
   int statusX;
   
   double livetime = 0.0;
   double ocr      = 0.0;
   double icr      = 0.0;
   
   unsigned long nevent = 0;
   
   statusX = dxp_get_statistics(&detChan, &livetime, &icr, &ocr, &nevent);
   
   if (statusX != DXP_SUCCESS) 
	 {
	   status = XIA_XERXES;
	   sprintf(info_string, 
			   "Error geting Run Statistics info for detChan %d",
			   detChan);
	   pslLogError("pslGetEvents", info_string, status);
	   return status;
	 }

   *((unsigned long *)value) = nevent;

   sprintf(info_string, "Events = %ld\n", *((unsigned long *) value));
   pslLogDebug("pslGetEvents", info_string);
  
   return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine gets the number of triggers (FASTPEAKS) in the run
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetTriggers(int detChan, void *value)
{
    int status;
    int statusX;

    double dFASTPEAKS;

    statusX = dxp_get_dspsymbol(&detChan, "FASTPEAKS", &dFASTPEAKS);

    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	pslLogError("pslGetTriggers", "Error retrieving the FASTPEAKS information", status);
	return status;
    }

    *((unsigned long *)value) = (unsigned long) dFASTPEAKS;

    return XIA_SUCCESS;
}

/********** 
 * This routine sets the run active information as retrieved
 * from Xerxes. The raw bitmask (from Xerxes) is actually 
 * returned to the user with the appropriate constants
 * defined in handel.h.
 **********/
PSL_STATIC int PSL_API pslGetRunActive(int detChan, void *value)
{
    int status;
    int statusX;
    int runActiveX;

    statusX = dxp_isrunning(&detChan, &runActiveX);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting run active information for detChan %d",
		detChan);
	pslLogError("pslGetRunActive", info_string, status);
	return status;
    }

    *((unsigned long *)value) = (unsigned long)runActiveX;

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine dispatches calls to the requested special run routine, when
 * that special run is supported by the g200.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoSpecialRun(int detChan, char *name, double gainScale, 
									   void *info, XiaDefaults *defaults,
									   Detector *detector, int detector_chan)
{
    int status;

    if (STREQ(name, "adc_trace"))
    {
	status = pslDoADCTrace(detChan, info);

    } else if (STREQ(name, "open_input_relay")) {

	status = pslDoControlTask(detChan, CT_DGFG200_OPEN_INPUT_RELAY, info);

    } else if (STREQ(name, "close_input_relay")) {

	status = pslDoControlTask(detChan, CT_DGFG200_CLOSE_INPUT_RELAY, info);
		
    } else if (STREQ(name, "program_firmware")) {

	status = pslDoControlTask(detChan, CT_DGFG200_PROGRAM_FIPPI, info);

    } else if (STREQ(name, "set_dacs")) {

	status = pslDoControlTask(detChan, CT_DGFG200_SETDACS, info);

    } else if (STREQ(name, "adjust_baseline")) {

	status = pslAdjustOffsets(detChan, info, defaults);

    } else if (STREQ(name, "ramp_offset_dac")) {

	status = pslDoControlTask(detChan, CT_DGFG200_RAMP_OFFSET_DAC, info);

    } else if (STREQ(name, "measure_system_gain")) {

	  status = pslMeasureSystemGain(detChan, info, defaults);

    } else if (STREQ(name, "calibrate_system_gain")) {

	  status = pslCalibrateSystemGain(detChan, info, detector->gain[detector_chan], gainScale, defaults);
	  
    } else {

	status = XIA_BAD_SPECIAL;
	sprintf(info_string, "%s is either an unsupported special run or a bad name for detChan %d",
		name, detChan);
	pslLogError("pslDoSpecialRun", info_string, status);
	return status;
      
    }
  
    if (status != XIA_SUCCESS)
    {
	sprintf(info_string, "Unable to execute the following special run: %s on detChan %d", name, detChan);
	pslLogError("pslDoSpecialRun", info_string, status);
    }

    return status;
}


/*****************************************************************************
 *
 * This routine does all of the work necessary to start and stop a special
 * run for the ADC data. A seperate call must be made to read the data out.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoADCTrace(int detChan, void *info)
{
    int status;
    int statusX;
    int infoInfo[3];
    int infoStart[2];
    int i;
    int timeout = 10000;
    int busy = 0;
	
    unsigned int len = sizeof(infoStart) / sizeof(infoStart[0]);

    short task = CT_ADC;

    parameter_t XWAIT;

    double *dInfo = (double *)info;
    double tracewait;
    /* In nanosec */
    double minTracewait = 75.0;
    float waitTime;
    float pollTime;

    statusX = dxp_control_task_info(&detChan, &task, infoInfo);
   
    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	pslLogError("pslDoADCTrace", "Error retrieving information about the ADC Trace Acquisition Task", status);
	return status;
    }

    waitTime = (float)((float)infoInfo[1] / 1000.0);
    pollTime = (float)((float)infoInfo[2] / 1000.0);

    infoStart[0] = (int)dInfo[0];
	
    /* Compute XWAIT keeping in mind that the minimum value for the 
     * G200 is 75 ns. 
     */
    sprintf(info_string, "dInfo[1] = %f", dInfo[1]);
    pslLogDebug("pslDoADCTrace", info_string);

    tracewait = dInfo[1];

    if (tracewait < minTracewait) 
    {
	tracewait = 75.0;
    }

    CLOCK_SPEED = pslGetClockSpeed(detChan);

    sprintf(info_string, "tracewait = %f minTracewait = %f", tracewait, minTracewait);
    pslLogDebug("pslDoADCTrace", info_string);

    XWAIT = (parameter_t) ROUND(tracewait * CLOCK_SPEED / 1000.);

    sprintf(info_string, "Setting XWAIT to %u", XWAIT);
    pslLogDebug("pslDoADCTrace", info_string);

    infoStart[1] = (int) XWAIT;

    statusX = dxp_start_control_task(&detChan, &task, &len, infoStart);
 
    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	pslLogError("pslDoADCTrace", "Error starting the ADC Trace Acquisition Task", status);
	return status;
    }

    /* Let the user know the actual value used for the delay */
    *(((double *) info) + 1) = ((double) infoStart[1]) / CLOCK_SPEED * 1000.;

    /* Wait for the specified amount of time before starting to poll board
     * to see if the special run is finished yet.
     */

    sprintf(info_string, "Preparing to wait: waitTime = %f", waitTime);
    pslLogDebug("pslDoADCTrace", info_string);

    utils->funcs->dxp_md_wait(&waitTime);

    pslLogDebug("pslDoADCTrace", "Finished waiting"); 
    pslLogDebug("pslDoADCTrace", "Preparing to poll board");

    for (i = 0; i < timeout; i++)
    {
	statusX = dxp_isrunning(&detChan, &busy);

	if (statusX != DXP_SUCCESS)
	{
	    status = XIA_XERXES;
	    pslLogError("pslDoADCTrace", "Error checking the run status (during ADC Trace Acquisition) of the G200", status);
	    return status;
	}

	/* Check if the module still has an active run, mask off what XerXes thinks */
	if ((busy&0x1) == 0)
	{
	    break;
	}

	if (i == (timeout - 1))
	{
	    status = XIA_TIMEOUT;
	    sprintf(info_string, "Timeout waiting to acquire ADC trace (timeout = %f)", timeout * pollTime);
	    pslLogError("pslDoADCTrace", info_string, status);
	    return status;
	}

	utils->funcs->dxp_md_wait(&pollTime);
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine parses out the actual data gathering to other routines.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetSpecialRunData(int detChan, char *name, void *value, XiaDefaults *defaults)
{
    int status;

    UNUSED(defaults);

    if (STREQ(name, "adc_trace_length"))
    {
	status = pslGetSpecialRunDataLength(detChan, CT_ADC, (unsigned long *) value);

    } else if (STREQ(name, "adc_trace")) {

	status = pslGetADCTrace(detChan, value);

    } else if (STREQ(name, "offset_dac_length")) {

	status = pslGetSpecialRunDataLength(detChan, CT_DGFG200_RAMP_OFFSET_DAC, (unsigned long *) value);

    } else if (STREQ(name, "offset_dac_data")) {

	status = pslGetSpecialData(detChan, CT_DGFG200_RAMP_OFFSET_DAC, (unsigned short *) value);

    } else {

	status = XIA_BAD_SPECIAL;
	sprintf(info_string, "%s is either an unsupported special run data type or a bad name for detChan %d",
		name, detChan);
	pslLogError("pslDoSpecialRunData", info_string, status);
	return status;
      
    }
  
    if (status != XIA_SUCCESS)
    {
	sprintf(info_string, "Unable to retrieve special run data: %s on detChan %d", name, detChan);
	pslLogError("pslDoSpecialRunData", info_string, status);
    }

    return status;
}

/*****************************************************************************
 *
 * This routine get the size of the special run data (what is stored in location
 * 0 of the info array returned from xerxes.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetSpecialRunDataLength(int detChan, short task, unsigned long *value)
{
    int status;
    int statusX;
    int info[3];

    statusX = dxp_control_task_info(&detChan, &task, info);
    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	pslLogError("pslGetSpecialRunDataLength", "Unable to get length of special run data from XerXes", status);
	return status;
    }

    *value = (unsigned long) info[0];

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine gets the ADC trace. It ASSUMES that the user has allocated
 * the proper amount of memory for value, preferably by making a call to 
 * getSpecialRunData() w/ "adc_trace_info".
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetADCTrace(int detChan, void *value)
{
    int status;
    int statusX;

    short task = CT_ADC;


    statusX = dxp_get_control_task_data(&detChan, &task, value);

    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	pslLogError("pslGetADCTrace", "Unable to get ADC trace from XerXes", status);
	return status;
    }

    statusX = dxp_stop_control_task(&detChan);

    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	pslLogError("pslGetADCTrace", "Unable to stop the control task with XerXes", status);
	return status;
    }

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine gets arbitrary special run data.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetSpecialData(int detChan, short task, void *value)
{
    int status;
    int statusX;

    statusX = dxp_get_control_task_data(&detChan, &task, value);
    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	sprintf(info_string, "Unable to get special run data for task %i from XerXes", task);
	pslLogError("pslGetSpecialRunData", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine performs a control task specified by the variable task.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslDoControlTask(int detChan, short task, void *info)
{
    int status = XIA_SUCCESS;
    int statusX;

    int infoInfo[3];
    int active;

    float waitTime;

    unsigned short timeout = 200;
    unsigned short loopNumber = 0;

    unsigned int len = 1;

    statusX = dxp_control_task_info(&detChan, &task, infoInfo);

    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	sprintf(info_string, "Unable to retrieve information about control task #%i", task);
	pslLogError("pslDoControlTask", info_string, status);
	return status;
    }

    statusX = dxp_start_control_task(&detChan, &task, &len, (int *)info);
    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	sprintf(info_string, "Unable to start task #%i", task);
	pslLogError("pslDoControlTask", info_string, status);
	return status;
    }

    /* set the waitTime to the initial wait time */
    waitTime = (float) ((float)infoInfo[1] / 1000.0);

    do {
	utils->funcs->dxp_md_wait(&waitTime);
    
	statusX = dxp_isrunning(&detChan, &active);
	if (statusX != DXP_SUCCESS)
	{
	    status = XIA_XERXES;
	    sprintf(info_string, "Unable to retrieve run status while waiting for task %i to finish", task);
	    pslLogError("pslDoControlTask", info_string, status);
	    return status;
	}

	loopNumber++;

	/* Change the waitTime to the polling time */
	waitTime = (float) ((float)infoInfo[2] / 1000.0);
    } while (((active & 0x1) != 0) && (loopNumber < timeout));

    /* If timeout, then report error and set status value, but still try to stop the run */
    if (loopNumber == timeout)
    {
	status = XIA_TIMEOUT;
	sprintf(info_string, "Timeout waiting on end of task %i to finish", task);
	pslLogError("pslDoControlTask", info_string, status);
    }

    statusX = dxp_stop_control_task(&detChan);
    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	sprintf(info_string, "Unable to end task #%i", task);
	pslLogError("pslDoControlTask", info_string, status);
	return status;
    }

    return status;
}

/*****************************************************************************
 *
 * This routine sets the proper detector type specific information (either 
 * RESETINT or TAURC) for detChan.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslSetDetectorTypeValue(int detChan, Detector *detector, 
											   int detectorChannel, XiaDefaults *defaults)
{
    int statusX;
    int status;

    parameter_t value;
    parameter_t value2;

    double resetint;

    switch(detector->type)
	  {
	  case XIA_DET_RESET:
		resetint = 4.0 * detector->typeValue[detectorChannel];
		value    = (parameter_t) ROUND(resetint);
		
		statusX = dxp_set_one_dspsymbol(&detChan, "RESETINT", &value);
		
		if (statusX != DXP_SUCCESS)
		  {
			status = XIA_XERXES;
			pslLogError("pslSetDetectorTypeValue", "Unable to set the DSP symbol RESETINT using XerXes", status);
			return status;
		  }
		
		break;
		
      case XIA_DET_RCFEED:
		value       = (parameter_t) floor(detector->typeValue[detectorChannel]);
		value2      = (parameter_t) ROUND((detector->typeValue[detectorChannel] - (double) value) * 65536);
      
		statusX = dxp_set_one_dspsymbol(&detChan, "PREAMPTAUA0", &value);
		if (statusX != DXP_SUCCESS)
		  {
			status = XIA_XERXES;
			pslLogError("pslSetDetectorTypeValue", "Unable to set the DSP symbol PREAMPTAUA0 using XerXes", status);
			return status;
		  }
		
		statusX = dxp_set_one_dspsymbol(&detChan, "PREAMPTAUB0", &value2);
		if (statusX != DXP_SUCCESS)
		  {
			status = XIA_XERXES;
			pslLogError("pslSetDetectorTypeValue", "Unable to set the DSP symbol PREAMPTAUB0 using XerXes", status);
			return status;
		  }
		
		/* Set the acquisition Value */
		status = pslSetDefault("decay_time", (void *) &(detector->typeValue[detectorChannel]), defaults);
		if (status != XIA_SUCCESS)
		  {
			pslLogError("pslSetDetectorTypeValue", "Unable to set the decay_time in the defaults", status);
			return status;
		  }

		break;
      
      default:
      case XIA_DET_UNKNOWN:
		
		status = XIA_UNKNOWN;
		sprintf(info_string, "Unknown detector type: %d", detector->type);
		pslLogError("pslSetDetectorTypeValue", info_string, status);
		return status;
		
		break;
	  }
	
    return XIA_SUCCESS;
}

/******************************************************************************
 *
 * This routine sets the polarity for a detChan. 
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslSetPolarity(int detChan, Detector *detector, 
									  int detectorChannel, XiaDefaults *defaults)
{
    int status;

	status = pslModifyCcsraBit(detChan, (boolean_t) (detector->polarity[detectorChannel] != 0), CCSRA_POLARITY, defaults);
    if (status != DXP_SUCCESS)
	  {
		pslLogError("pslSetPolarity", "Unable to set the polarity bit of the ChanCSRA variable", status);
		return status;
	  }

	/* Set the acquisition Value */
	status = pslSetDefault("detector_polarity", (void *) &(detector->polarity[detectorChannel]), defaults);
	if (status != XIA_SUCCESS)
	  {
		pslLogError("pslSetPolarity", "Unable to set the detector_polarity in the defaults", status);
		return status;
	  }

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine creates a default w/ information specific to the g200 in it.
 * 
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetDefaultAlias(char *alias, char **names, double *values)
{
    int len;
    int i;

    char *aliasName = "defaults_dxpg200";

    pslLogDebug("pslGetDefaultAlias", "Preparing to copy default defaults");
	
    len = sizeof(defaultValues) / sizeof(defaultValues[0]);
    for (i = 0; i < len; i++)
    {
	sprintf(info_string, "defNames[%d] = %s, defValues[%d] = %3.3lf", i, defaultNames[i], i, defaultValues[i]);
	pslLogDebug("pslGetDefaultAlias", info_string);

	strcpy(names[i], defaultNames[i]);
	values[i] = defaultValues[i];
    }

    strcpy(alias, aliasName);

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine retrieves the value of the DSP parameter name from detChan.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslGetParameter(int detChan, const char *name, unsigned short *value)
{
    int statusX;
    int status;

    char *noconstName = NULL;


    noconstName = (char *)utils->funcs->dxp_md_alloc((strlen(name) + 1) * sizeof(char));
  
    if (noconstName == NULL) {

	status = XIA_NOMEM;
	pslLogError("pslGetParameter", "Out-of-memory trying to create tmp. string", status);
	return status;
    }

    strcpy(noconstName, name);

    statusX = dxp_get_one_dspsymbol(&detChan, noconstName, value);

    if (statusX != DXP_SUCCESS) { 

	utils->funcs->dxp_md_free((void *)noconstName);
	noconstName = NULL;

	status = XIA_XERXES;
	sprintf(info_string, "Error trying to get DSP parameter %s from detChan %d", name, detChan);
	pslLogError("pslGetParameter", info_string, status);
	return status;
    }

    utils->funcs->dxp_md_free((void *)noconstName);
    noconstName = NULL;

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine sets the value of the DSP parameter name for detChan.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslSetParameter(int detChan, const char *name, unsigned short value)
{
    int statusX;
    int status;

    char *noconstName = NULL;


    noconstName = (char *)utils->funcs->dxp_md_alloc((strlen(name) + 1) * sizeof(char));
  
    if (noconstName == NULL) {

	status = XIA_NOMEM;
	pslLogError("pslSetParameter", "Out-of-memory trying to create tmp. string", status);
	return status;
    }

    strcpy(noconstName, name);

    statusX = dxp_set_one_dspsymbol(&detChan, noconstName, &value);

    if (statusX != DXP_SUCCESS) { 

	utils->funcs->dxp_md_free((void *)noconstName);
	noconstName = NULL;

	status = XIA_XERXES;
	sprintf(info_string, "Error trying to set DSP parameter %s for detChan %d", name, detChan);
	pslLogError("pslSetParameter", info_string, status);
	return status;
    }

    utils->funcs->dxp_md_free((void *)noconstName);
    noconstName = NULL;

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * The whole point of this routine is to make the PSL layer start things
 * off by calling pslSetAcquistionValues() for the acquisition values it 
 * thinks are appropriate for the G200.
 *
 *****************************************************************************/
PSL_STATIC int PSL_API pslUserSetup(int detChan, XiaDefaults *defaults, 
                                    FirmwareSet *firmwareSet, 
                                    CurrentFirmware *currentFirmware, 
                                    char *detectorType, double gainScale,
									Detector *detector, int detector_chan, Module *m)
{
    int status;
    int numNames;
    int i;

	/*    int info[2] = {1,1};
	 */
    double ddummy = 0.0;

	/* Update a couple of defaults with values from system configuration */

	/* Pre-Amplifier Gain */
	ddummy = detector->gain[detector_chan];
	status = pslSetDefault("preamp_gain", (void *) &ddummy, defaults);
	if (status != XIA_SUCCESS)
	  {
		sprintf(info_string, "Error setting preamp_gain in the defaults detChan %d", detChan);
		pslLogError("pslUserSetup", info_string, status);
		return status;
	  }

	/* Detector Polarity */
	ddummy = (double) detector->polarity[detector_chan];
	status = pslSetDefault("detector_polarity", (void *) &ddummy, defaults);
	if (status != XIA_SUCCESS)
	  {
		sprintf(info_string, "Error setting detector_polarity in the defaults detChan %d", detChan);
		pslLogError("pslUserSetup", info_string, status);
		return status;
	  }
      
	/* Decay Time */
	ddummy = (double) detector->typeValue[detector_chan];
	status = pslSetDefault("decay_time", (void *) &ddummy, defaults);
	if (status != XIA_SUCCESS)
	  {
		sprintf(info_string, "Error setting decay_time in the defaults detChan %d", detChan);
		pslLogError("pslUserSetup", info_string, status);
		return status;
	  }
      

	/* On to other Acquisition values */
    numNames = sizeof(defaultNames) / sizeof(defaultNames[0]);

    for (i = 0; i < numNames; i++)
    {
	  /* Skip these default definitions - done already */
	  if (STREQ(defaultNames[i], "preamp_gain") || 
		  STREQ(defaultNames[i], "detector_polarity") ||
		  STREQ(defaultNames[i], "decay_time"))
		{
		  continue;
		}

	  /* The above defaults are already validated as being in the defaults
	   * by xiaStartSystem()
	   */
	  status = pslGetDefault(defaultNames[i], (void *) &ddummy, defaults);
	  if (status != XIA_SUCCESS)
		{
		  sprintf(info_string, "Error getting default %s from the defaults list for detChan %d", defaultNames[i], detChan);
		  pslLogError("pslUserSetup", info_string, status);
		  return status;
		}
      
	  status = pslSetAcquisitionValues(detChan, defaultNames[i], (void *) &ddummy, 
									   defaults, firmwareSet, currentFirmware, 
									   detectorType, gainScale, detector, 
									   detector_chan, m);
      
	  if (status != XIA_SUCCESS)
		{
		  sprintf(info_string, "Error setting acquisition value %s for detChan %d", defaultNames[i], detChan);
		  pslLogError("pslUserSetup", info_string, status);
		  return status;
		}
    }
	
    /* Perform the gain limit calculations here */
	/*    status = pslCalibrateSystemGain(detChan, info, preampGain, gainScale, defaults);
	if (status != XIA_SUCCESS) 
	  {
		sprintf(info_string, "Error calibrating the system gain");
		pslLogError("pslUserSetup", info_string, status);
		return status;
	  }
	*/
    return XIA_SUCCESS;
}


/****
 * This routine updates the filter parameters for the specified detChan using
 * information from the defaults and the firmware.
 */
PSL_STATIC int PSL_API pslUpdateFilter(int detChan, XiaDefaults *defaults, FirmwareSet *firmwareSet)
{
    int status;
    int statusX;

    unsigned short numFilter;
    unsigned short i;
    unsigned short ptrr = 0;

    char piStr[100];
    char psStr[100];

    double peakingTime;
    double gapTime;
    double ddummy;

    Firmware *current = NULL;

    parameter_t SLOWLEN = 0;
    parameter_t SLOWGAP = 0;
    parameter_t PEAKSEP = 0;
    parameter_t PEAKSAMP = 0;
    parameter_t DECIMATION;

    parameter_t *filterInfo = NULL;

    CLOCK_SPEED = pslGetClockSpeed(detChan);

    /* Retrieve the peaking and gap times from the defaults */
    status = pslGetDefault("peaking_time", (void *) &peakingTime, defaults);
    status = pslGetDefault("gap_time", (void *) &gapTime, defaults);

    /* Get the decimation from the board */
    statusX = dxp_get_one_dspsymbol(&detChan, "DECIMATION", &DECIMATION);

    if (statusX != DXP_SUCCESS)
    {
	status = XIA_XERXES;
	pslLogError("pslUpdateFilter", "Error getting DECIMATION from Xerxes", status);
	return status;
    }

    sprintf(info_string, "Trying to update Filter information: PT = %f, GT = %f, DEC = %i", 
	    peakingTime, gapTime, DECIMATION);
    pslLogDebug("pslUpdateFilter", info_string);

    /* Calculate the slow length from peaking time and decimation */
    SLOWLEN     = (parameter_t) ROUND((peakingTime * CLOCK_SPEED)/pow(2, (double) DECIMATION));
    /* Do a stupidity check for SLOWLEN > 28 (max since SLOWGAP >=3) */
    if (SLOWLEN > 28) SLOWLEN = 28;
    if (SLOWLEN < 2) SLOWLEN = 2;
    peakingTime = (((double) SLOWLEN) / CLOCK_SPEED) * pow(2., (double) DECIMATION);

    /* Calculate the gap length from the gap_time in the acquisition values */
    /* Truncate the gap time to a length */
    SLOWGAP = (parameter_t) ceil((gapTime * CLOCK_SPEED)/pow(2, (double) DECIMATION));
    /* Make sure the gap length is greater than 3 (hardware limitation for good results) */
    if (SLOWGAP < 3) SLOWGAP = 3;
    if (SLOWGAP > 29) SLOWGAP = 29;
  
    /* Now check for hard limit of gap and slow length versus 31 */
    if ((SLOWGAP+SLOWLEN)>31)
	  {
		/* Reduce Slowgap by enough to make it fit within this decimation */
		SLOWGAP = (parameter_t) (31 - SLOWLEN);
		sprintf(info_string, "SLOWLEN+SLOWGAP>32, setting SLOWGAP = %u with SLOWLEN = %u", SLOWGAP, SLOWLEN);
		pslLogInfo("pslUpdateFilter", info_string);
		
		/*status = XIA_SLOWFILTER_OOR;
		  sprintf(info_string, "Unable to set parameters, reduce peaking time or gap time, SLOWLEN = %i, SLOWGAP %i", 
		  SLOWLEN, SLOWGAP);
		  pslLogError("pslUpdateFilter", info_string, status);
		  return status;*/
	  }
 
    /* Set the peaking time in the defaults, the value may have changed */
    status = pslSetDefault("peaking_time", (void *) &peakingTime, defaults);
  
    if (firmwareSet->filename != NULL) {
	
	status = xiaFddGetNumFilter(firmwareSet->filename, peakingTime, firmwareSet->numKeywords,
				    firmwareSet->keywords, &numFilter);
	
	if (status != XIA_SUCCESS) 
	{
	    pslLogError("pslUpdateFilter", "Error getting number of filter params", status);
	    return status;
	}
	
	filterInfo = (parameter_t *)utils->funcs->dxp_md_alloc(numFilter * sizeof(parameter_t));

	status = xiaFddGetFilterInfo(firmwareSet->filename, peakingTime, firmwareSet->numKeywords,
				     firmwareSet->keywords, filterInfo);
	
	if (status != XIA_SUCCESS) 
	{
	    pslLogError("pslUpdateFilter", "Error getting filter information from FDD", status);
	    return status;
	}
	
	/* Override the values loaded in from the FDD with values from the
	 * defaults. Remember that when the user is using an FDD file they
	 * don't need the _ptrr{n} specifier on their default
	 * name.
	 */
	/* Find the entries in the defaults list */
	status = pslGetDefault("peaking_separation_offset", (void *) &ddummy, defaults);
	if (status == XIA_SUCCESS) filterInfo[0] = (parameter_t) ddummy;
	status = pslGetDefault("peak_sample_offset", (void *) &ddummy, defaults);
	if (status == XIA_SUCCESS) filterInfo[1] = (parameter_t) ddummy;
	
    } else {
	
	/* Fill the filter information in here using the FirmwareSet */
	current = firmwareSet->firmware;
	
	while (current != NULL) 
	  {
		if ((peakingTime > current->min_ptime) &&
			(peakingTime <= current->max_ptime))
		  {
			filterInfo = (parameter_t *) utils->funcs->dxp_md_alloc(current->numFilter * sizeof(parameter_t));
			for (i = 0; i < current->numFilter; i++) 
			  {
				filterInfo[i] = current->filterInfo[i];
			  }
			
			ptrr = current->ptrr;
		  }
		
	    current = current->next;
		
	  }
	
	if (filterInfo == NULL) 
	  {
	    status = XIA_BAD_FILTER;
	    pslLogError("pslUpdateFilter", "Error loading filter information", status);
	    return status;
	  }
	
	sprintf(piStr, "peak_separation_offset_ptrr%u", ptrr);
	sprintf(psStr, "peak_sample_offset_ptrr%u", ptrr);

	/* Find the entries in the defaults list */
	status = pslGetDefault(piStr, (void *) &ddummy, defaults);
	if (status == XIA_SUCCESS) filterInfo[0] = (parameter_t) ddummy;
	status = pslGetDefault(psStr, (void *) &ddummy, defaults);
	if (status == XIA_SUCCESS) filterInfo[1] = (parameter_t) ddummy;
    }

    /* The G200 PSL interprets the filterInfo as follows:
     * filterInfo[0] = positive peak separation offset
     * filterInfo[1] = negative peak sample offset
     */
    statusX = dxp_set_one_dspsymbol(&detChan, "SLOWLENGTH0", &SLOWLEN);

    if (statusX != DXP_SUCCESS) 
    {
	status = XIA_XERXES;
	pslLogError("pslUpdateFilter", "Error setting SLOWLENGTH from Xerxes", status);
	return status;
    }

    statusX = dxp_set_one_dspsymbol(&detChan, "SLOWGAP0", &SLOWGAP);

    if (statusX != DXP_SUCCESS) 
    {
	status = XIA_XERXES;
	pslLogError("pslUpdateFilter", "Error setting SLOWGAP", status);
	return status;
    }
  
    PEAKSAMP = (parameter_t) (SLOWLEN + SLOWGAP - filterInfo[0]);
  
    statusX = dxp_set_one_dspsymbol(&detChan, "PEAKSAMPLE0", &PEAKSAMP);

    if (statusX != DXP_SUCCESS) 
    {
	status = XIA_XERXES;
	pslLogError("pslUpdateFilter", "Error setting PEAKSAMPLE", status);
	return status;
    }

    PEAKSEP = (parameter_t)(PEAKSAMP + filterInfo[1]);

    statusX = dxp_set_one_dspsymbol(&detChan, "PEAKSEP0", &PEAKSEP);

    if (statusX != DXP_SUCCESS) 
    {
	status = XIA_XERXES;
	pslLogError("pslUpdateFilter", "Error setting PEAKSEP", status);
	return status;
    }

    /* Update the FIFO information as this depends on the PEAKSEP */
    status = pslUpdateFIFO(detChan, defaults);
    if (status != XIA_SUCCESS)
    {
	pslLogError("pslUpdateFilter", "Error updating the FIFO information", status);
	return status;
    }

    statusX = dxp_upload_dspparams(&detChan);
   
    if (statusX != DXP_SUCCESS) 
    {
	status = XIA_XERXES;
	pslLogError("pslUpdateFilter", "Error uploading DSP parameters to internal memory", status);
	return status;
    }
  
    sprintf(info_string, "Done Updating the Filter parameters: SLOWLENGTH=%u, SLOWGAP=%u, PEAKSEP=%u, PEAKSAMP=%u", 
	    SLOWLEN, SLOWGAP, PEAKSEP, PEAKSAMP);
    pslLogDebug("pslUpdateFilter", info_string);

    return XIA_SUCCESS;
}

/****
 * This routine updates the trigger filter parameters for the specified detChan using
 * information from the defaults.
 */
PSL_STATIC int PSL_API pslUpdateTriggerFilter(int detChan, XiaDefaults *defaults)
{
    int status;
    int statusX;

    double triggerPeakingTime;
    double triggerGapTime;

    parameter_t FASTLEN = 0;
    parameter_t FASTGAP = 0;

    CLOCK_SPEED = pslGetClockSpeed(detChan);

    /* Get the trigger peaking and gap times */
    status = pslGetDefault("trigger_peaking_time", (void *) &triggerPeakingTime, defaults);
    status = pslGetDefault("trigger_gap_time", (void *) &triggerGapTime, defaults);

    /* Round the trigger length to an integer */
    FASTLEN = (parameter_t) ROUND(triggerPeakingTime * CLOCK_SPEED);
    /* Make sure the trigger length is greater than 2 (hardware limitation for good results) */
    if (FASTLEN < 2) FASTLEN = 2;
    if (FASTLEN > 30) FASTLEN = 30;

    /* Truncate the gap time to a length */
    FASTGAP = (parameter_t) ceil(triggerGapTime * CLOCK_SPEED);
    /* Make sure the trigger gap length is greater than 3 (hardware limitation for good results) */
    if (FASTGAP > 29) FASTGAP = 29;

    /* Now check for hard limit of gap and slow length versus 31 */
    if ((FASTGAP+FASTLEN)>31)
    {
	FASTLEN = (parameter_t) (31 - FASTGAP);
    }

    /* Reset the default parameters to match the downloaded values */
    triggerPeakingTime = ((double) FASTLEN / CLOCK_SPEED);
    triggerGapTime = ((double) FASTGAP / CLOCK_SPEED);

    /* Set the trigger peaking and gap times */
    status = pslSetDefault("trigger_peaking_time", (void *) &triggerPeakingTime, defaults);
    status = pslSetDefault("trigger_gap_time", (void *) &triggerGapTime, defaults);

    /* Set the DSP parameters */
    statusX = dxp_set_one_dspsymbol(&detChan, "FASTLENGTH0", &FASTLEN);

    if (statusX != DXP_SUCCESS) 
    {
	status = XIA_XERXES;
	pslLogError("pslUpdateTriggerFilter", "Error setting FASTLENGTH in Xerxes", status);
	return status;
    }

    statusX = dxp_set_one_dspsymbol(&detChan, "FASTGAP0", &FASTGAP);

    if (statusX != DXP_SUCCESS) 
    {
	status = XIA_XERXES;
	pslLogError("pslUpdateTriggerFilter", "Error setting FASTGAP in XerXes", status);
	return status;
    }
  
    statusX = dxp_upload_dspparams(&detChan);

    if (statusX != DXP_SUCCESS) 
    {
	status = XIA_XERXES;
	pslLogError("pslUpdateTriggerFilter", "Error uploading DSP parameters to internal memory", status);
	return status;
    }
  
    sprintf(info_string, "Done Updating the Trigger Filter parameters: FASTLENGTH=%u, FASTGAP=%u", 
	    FASTLEN, FASTGAP);
    pslLogDebug("pslUpdateTriggerFilter", info_string);
  
    return XIA_SUCCESS;
}

/****
 * This routine updates the FIFO information.
 */
PSL_STATIC int PSL_API pslUpdateFIFO(int detChan, XiaDefaults *defaults)
{
    int status;
    int statusX;

    double traceLength;
    double traceDelay;

    parameter_t COINCWAIT;
    parameter_t DECIMATION;
    parameter_t TRIGGERDELAY;
    parameter_t PEAKSEP;
    parameter_t PAFLENGTH;

    CLOCK_SPEED = pslGetClockSpeed(detChan);

    /* Get the DSP parameters */
    statusX = dxp_get_one_dspsymbol(&detChan, "DECIMATION", &DECIMATION);

    if (statusX != DXP_SUCCESS) 
    {
	status = XIA_XERXES;
	pslLogError("pslUpdateFIFO", "Error getting DECIMATION in Xerxes", status);
	return status;
    }

    statusX = dxp_get_one_dspsymbol(&detChan, "PEAKSEP0", &PEAKSEP);

    if (statusX != DXP_SUCCESS) 
    {
	status = XIA_XERXES;
	pslLogError("pslUpdateFIFO", "Error getting PEAKSEP in Xerxes", status);
	return status;
    }

    /* Get Information from the defaults list */
    status = pslGetDefault("trace_length", (void *) &traceLength, defaults);
    status = pslGetDefault("trace_delay", (void *) &traceDelay, defaults);

    /* Calculate the TRIGGERDELAY first */
    TRIGGERDELAY = 1;
    if (traceLength > 0.0) 
    {
	switch (DECIMATION) {
	  case 0:
	    TRIGGERDELAY = (parameter_t) (PEAKSEP + 6);
	    break;
	  case 1:
	    TRIGGERDELAY = (parameter_t) ((PEAKSEP + 1)<<DECIMATION);
	    break;
	  case 2:
	    TRIGGERDELAY = (parameter_t) ((PEAKSEP + 1)<<DECIMATION);
	    break;
	  case 3:
	    TRIGGERDELAY = (parameter_t) ((PEAKSEP)<<DECIMATION);
	    break;
	  case 4:
	    TRIGGERDELAY = (parameter_t) (((PEAKSEP)<<DECIMATION) + 40);
	    break;
	  case 5:
	    TRIGGERDELAY = (parameter_t) ((PEAKSEP)<<DECIMATION);
	    break;
	  case 6:
	    TRIGGERDELAY = (parameter_t) ((PEAKSEP)<<DECIMATION);
	    break;
	  default:
	    status = XIA_UNKNOWN_DECIMATION;
	    sprintf(info_string, "Unknown value of DECIMATION = %u", DECIMATION);
	    pslLogError("pslUpdateFIFO", info_string, status);
	    return status;
	}
    }

    TRIGGERDELAY = (parameter_t) (ROUND(TRIGGERDELAY / pow(2., (double) FIFO_DECIMATION)));
  
    /* Set the trigger delay on the module */
    statusX = dxp_set_one_dspsymbol(&detChan, "TRIGGERDELAY0", &TRIGGERDELAY);

    if (statusX != DXP_SUCCESS) 
	  {
		status = XIA_XERXES;
		pslLogError("pslUpdateFIFO", "Error setting TRIGGERDELAY in Xerxes", status);
		return status;
	  }

    /* now calculate the programmable almost full (PAF) register value */
    PAFLENGTH = (parameter_t) (TRIGGERDELAY + (ROUND(traceDelay * (CLOCK_SPEED / pow(2., (double) FIFO_DECIMATION)))) + 8);

    /* Bounds checking */
    if (PAFLENGTH > (FIFO_LENGTH - 4)) 
	  {
		PAFLENGTH = FIFO_LENGTH - 4;
		traceDelay = ((double)((PAFLENGTH - TRIGGERDELAY) - 8));
		traceDelay = traceDelay / (CLOCK_SPEED / pow(2., (double) FIFO_DECIMATION));

		/* Store the new value of traceDelay */
		status = pslSetDefault("trace_delay", (void *) &traceDelay, defaults);
	  }

    /* Write the PAFLENGTH to the module */
    statusX = dxp_set_one_dspsymbol(&detChan, "PAFLENGTH0", &PAFLENGTH);
	if (statusX != DXP_SUCCESS) 
	  {
		status = XIA_XERXES;
		pslLogError("pslUpdateFIFO", "Error setting PAFLENGTH in Xerxes", status);
		return status;
	  }
  
    /* Determine Coincidence wait window */
    COINCWAIT = (parameter_t) MAX(1., ceil(1.5 * pow(2, (double) DECIMATION) - 70.));
    COINCWAIT = (parameter_t) MAX(COINCWAIT, PAFLENGTH - TRIGGERDELAY - 70);

    /* Write COINCWAIT to the module */
    statusX = dxp_set_one_dspsymbol(&detChan, "COINCWAIT", &COINCWAIT);
    if (statusX != DXP_SUCCESS) 
	  {
		status = XIA_XERXES;
		pslLogError("pslUpdateFIFO", "Error setting COINCWAIT in Xerxes", status);
		return status;
	  }

	/* And update the FiPPi Settings */
	status = pslDoControlTask(detChan, CT_DGFG200_PROGRAM_FIPPI, NULL);
	if (status != XIA_SUCCESS)
	  {
		pslLogError("pslUpdateFIFO", "Unable to execute the program_fippi control task", status);
		return status;
	  }
   
    sprintf(info_string, "Done Updating the FIFO parameters: TRIGGERDELAY=%u, PAFLENGTH=%u, COINCWAIT=%u", 
	    TRIGGERDELAY, PAFLENGTH, COINCWAIT);
    pslLogDebug("pslUpdateFIFO", info_string);
  
    return XIA_SUCCESS;
}

/**
 * This routine checks to see if the specified string is
 * all uppercase or not.
 */
PSL_STATIC boolean_t PSL_API pslIsUpperCase(char *name)
{
    int len;
    int i;


    len = strlen(name);

    for (i = 0; i < len; i++) 
    {
	if (!isupper(name[i]) &&
	    !isdigit(name[i])) 
	{
	    return FALSE_;
	}
    }
  
    return TRUE_;
}


/**
 * This routine updates the value of the parameter_t in the 
 * defaults and then writes it to the board.
 */
PSL_STATIC int PSL_API pslDoParam(int detChan, char *name, void *value, XiaDefaults *defaults)
{
    int status;
    int statusX;

    double temp;

    /* The parameter_t is definitely in the
     * defaults list due to the behavior of
     * xiaSetAcquisitionValue().
     */
    temp = (double) *((parameter_t *) value);
    status = pslSetDefault(name, (void *) &temp, defaults);
    if (status != XIA_SUCCESS) 
    {
	sprintf(info_string, "Unable to set the value of %s to %u", name, *((unsigned short *)value));
	pslLogError("pslDoParam", info_string, status);
	return status;
    }

    statusX = dxp_set_one_dspsymbol(&detChan, name, (parameter_t *) value);
    if (statusX != DXP_SUCCESS) 
    {
	status = XIA_XERXES;
	sprintf(info_string, "Xerxes reported an error trying to set %s to %u", name, *((parameter_t *)value));
	pslLogError("pslDoParam", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}

/**
 * Checks to see if the specified name is on the list of 
 * required acquisition values for the Saturn.
 */
PSL_STATIC boolean_t PSL_API pslCanRemoveName(char *name)
{
    int numNames = (int)(sizeof(defaultNames) / sizeof(defaultNames[0]));
    int i;

    for (i = 0; i < numNames; i++) 
    {
	if (STREQ(defaultNames[i], name)) 
	{
	    return FALSE_;
	}
    }

    return TRUE_;
}

PSL_STATIC unsigned int PSL_API pslGetNumDefaults(void)
{
    return (unsigned int) (sizeof(defaultNames) / sizeof(defaultNames[0]));
} 

/**********
 * This routine gets the number of DSP parameters
 * for the specified detChan.
 **********/
PSL_STATIC int PSL_API pslGetNumParams(int detChan, unsigned short *numParams)
{
    int status;
    int statusX;

    statusX = dxp_max_symbols(&detChan, numParams);
    if (statusX != DXP_SUCCESS) 
    {
	status = XIA_XERXES;
	sprintf(info_string, "Error getting number of DSP params for detChan %d", 
		detChan);
	pslLogError("pslGetNumParams", info_string, status);
	return status;
    }
  
    return XIA_SUCCESS;
}


/**********
 * This routine returns the parameter data
 * requested. Assumes that the proper 
 * amount of memory has been allocated for
 * value.
 **********/
PSL_STATIC int PSL_API pslGetParamData(int detChan, char *name, void *value)
{
    int status;
    int statusX;

    unsigned short numSymbols = 0;

    unsigned short *tmp1 = NULL;
    unsigned short *tmp2 = NULL;


    statusX = dxp_max_symbols(&detChan, &numSymbols);
  
    if (statusX != DXP_SUCCESS) 
    {
	status = XIA_XERXES;
	sprintf(info_string, "Error getting number of DSP symbols for detChan %d",
		detChan);
	pslLogError("pslGetParamData", info_string, status);
	return status;
    }
  

    /* Allocate these arrays in case we need them for
     * dxp_symbolname_limits().
     */
    tmp1 = (unsigned short *)malloc(numSymbols * sizeof(unsigned short));
    tmp2 = (unsigned short *)malloc(numSymbols * sizeof(unsigned short));

    if ((tmp1 == NULL) || (tmp2 == NULL)) 
    {
	status = XIA_NOMEM;
	pslLogError("pslGetParaData", "Out-of-memory allocating tmp. arrays", status);
	return status;
    }
  
    if (STREQ(name, "names")) 
    {
	  
	statusX = dxp_symbolname_list(&detChan, (char **)value);
	  
    } else if (STREQ(name, "values")) {
	  
	statusX = dxp_readout_detector_run(&detChan, (unsigned short *)value, NULL, NULL);
	  
    } else if (STREQ(name, "access")) {
	  
	statusX = dxp_symbolname_limits(&detChan, (unsigned short *)value, tmp1, tmp2);
	  
    } else if (STREQ(name, "lower_bounds")) {
	  
	statusX = dxp_symbolname_limits(&detChan, tmp1, (unsigned short *)value, tmp2);
	
    } else if (STREQ(name, "upper_bounds")) {
	  
	statusX = dxp_symbolname_limits(&detChan, tmp1, tmp2, (unsigned short *)value);
	  
    } else {
	  
	free((void *)tmp1);
	free((void *)tmp2);
	  
	status = XIA_BAD_NAME;
	sprintf(info_string, "%s is not a valid name argument", name);
	pslLogError("pslGetParamData", info_string, status);
	return status;
    }

    free((void *)tmp1);
    free((void *)tmp2);

    if (statusX != DXP_SUCCESS) {

	status = XIA_XERXES;
	sprintf(info_string, "Error getting DSP parameter data (%s) for detChan %d",
		name, detChan);
	pslLogError("pslGetParamData", info_string, status);
	return status;
    }

    return XIA_SUCCESS;
}


/**********
 * This routine is mainly a wrapper around dxp_symbolname_by_index()
 * since VB can't pass a string array into a DLL and, therefore, 
 * is unable to use pslGetParams() to retrieve the parameters list.
 **********/
PSL_STATIC int PSL_API pslGetParamName(int detChan, unsigned short index, char *alias)
{
    int statusX;
    int status;


    statusX = dxp_symbolname_by_index(&detChan, &index, alias);

    if (statusX != DXP_SUCCESS) 
    {
	status = XIA_XERXES;
	sprintf(info_string, "Error getting DSP parameter name at index %u for detChan %d",
		index, detChan);
	pslLogError("pslGetParamName", info_string, status);
	return status;
    }
  
    return XIA_SUCCESS;
}

/******************************************************************************** 
 * This is the master routine that adjusts the DC level of the G200 to sit at the 
 * position chosen by the acquisition value baseline_percent_rule
 ********************************************************************************/
PSL_STATIC int PSL_API pslAdjustOffsets(int detChan, void *info, XiaDefaults *defaults)
{
    int status;
    int statusX;

    parameter_t TRACKDAC = 32767;

    /* constants related to the ADC range as interpreted by the OFFSET DAC control task */
    unsigned short adcMax = 4095;

    double coeff[2];

    double baselinePercentRule;
    double baselineLevel;
    double offset;

    /* First perform a linear fit to the tracking DAC ramp data, use the coefficients to set
     * the offset DAC value */
    status = pslFitTrackingDACData(detChan, info, CT_DGFG200_RAMP_OFFSET_DAC, coeff);
    if (status != XIA_SUCCESS) 
	  {
		sprintf(info_string, "Error performing linear fit(s) to the Tracking DAC data for detChan %i", detChan);
		pslLogError("pslAdjustOffsets", info_string, status);
		return status;
	  }
      
    /* If successful, then set the new trackdac value, else set it to default value */
    if (status == XIA_SUCCESS) 
	  {
		/* Retrieve the baseline percent rule from the acquisition values */
		status = pslGetDefault("baseline_percent_rule", (void *) &baselinePercentRule, defaults);
		if (status != XIA_SUCCESS) 
		  {
			sprintf(info_string, "Unable to find the baseline_percent_rule Acquisition value");
			pslLogError("pslAdjustOffsets", info_string, status);
			return status;
		  }
		
		baselineLevel = adcMax * baselinePercentRule / 100.;
		TRACKDAC = (parameter_t) ROUND((32. / coeff[1]) * (baselineLevel - coeff[0]));
	  } 
    /* Else use the TRACKDAC default that is set at start of this function */
    statusX = dxp_set_one_dspsymbol(&detChan, "TRACKDAC0", &TRACKDAC);
    if (statusX != DXP_SUCCESS) 
	  {
		status = XIA_XERXES;
		sprintf(info_string, "Xerxes reported an error trying to set TRACKDAC0 to %u", TRACKDAC);
		pslLogError("pslAdjustOffsets", info_string, status);
		return status;
	  }
	
    /* Update the Offset Acquisition Value */
    status = pslCalculateOffset(TRACKDAC, &offset);
    if (status != XIA_SUCCESS) 
	  {
		sprintf(info_string, "Unable to determine the offset value from TRACKDAC = %#x", TRACKDAC);
		pslLogError("pslAdjustOffsets", info_string, status);
		return status;
	  }
	
    /* Set the offset acquisition value */
    status = pslSetDefault("offset", (void *) &offset, defaults);
    if (status != XIA_SUCCESS) 
	  {
		sprintf(info_string, "Unable to set the offset Acquisition value");
		pslLogError("pslAdjustOffsets", info_string, status);
		return status;
	  }
	
    /* Update the onboard DACs */
    status = pslDoControlTask(detChan, CT_DGFG200_SETDACS, NULL);
    if (status != XIA_SUCCESS) 
	  {
		sprintf(info_string, "Unable to set the DAC values on the module");
		pslLogError("pslAdjustOffsets", info_string, status);
		return status;
	  }

    return status;
}

/******************************************************************************** 
 * This routine measures the system gain according to the current setting of the
 * GAINDAC.
 ********************************************************************************/
PSL_STATIC int PSL_API pslMeasureSystemGain(int detChan, void *info, XiaDefaults *defaults)
{
    int status;

    double coeff[2];
    double systemGain;

    /* First perform a linear fit to the tracking DAC ramp data, use the coefficients to determine the 
     * slope of the ramp, which is related to the gain of the system.
     */
    status = pslFitTrackingDACData(detChan, info, CT_DGFG200_ISOLATED_RAMP_OFFSET_DAC, coeff);
    if (status != XIA_SUCCESS) 
    {
	sprintf(info_string, "Unable to fit the Tracking DAC data while measuring the system gain for detChan %i", detChan);
	pslLogError("pslMeasureSystemGain", info_string, status);
	return status;      
    }
      
    /* If successful, then set the store the new measured system Gain, else let the calling routine 
     * know of a problem 
     */
    systemGain = coeff[1] / 32. * 4. * 120e-6 / (91.6e-6 * 0.86631 * 2.);

    /* Retrieve the baseline percent rule from the acquisition values */
    status = pslSetDefault("system_gain", (void *) &systemGain, defaults);
    if (status != XIA_SUCCESS) 
    {
	status = XIA_UNKNOWN_VALUE;
	sprintf(info_string, "Unable to set the system_gain Acquisition value");
	pslLogError("pslMeasureSystemGain", info_string, status);
	return status;
    }

    return status;
}

/******************************************************************************** 
 * This routine measures the system gain at its two endpoints, GAINDAC=0 and 
 * GAINDAC=65535 and records them in acquisition values
 ********************************************************************************/
PSL_STATIC int PSL_API pslCalibrateSystemGain(int detChan, void *info, 
					      double preampGain, double gainScale, 
					      XiaDefaults *defaults)
{
    int status;
    int statusX;

    double systemGain;
    double dynamicRange;

    parameter_t OLDGAINDAC;
    parameter_t GAINDAC;

    /* First retrieve the current setting for the GAINDAC, so that we can reset the system
     * after we are done messing with it */
    statusX = dxp_get_one_dspsymbol(&detChan, "GAINDAC0", &OLDGAINDAC);
    if (statusX != DXP_SUCCESS) 
    {
	status = XIA_XERXES;
	sprintf(info_string, "Xerxes reported an error trying to retrieve GAINDAC0");
	pslLogError("pslCalibrateSystemGain", info_string, status);
	return status;
    }

    /* Now proceed to set the GAINDAC to its minimum */
    GAINDAC = 0;
    statusX = dxp_set_one_dspsymbol(&detChan, "GAINDAC0", &GAINDAC);
    if (statusX != DXP_SUCCESS) 
    {
	status = XIA_XERXES;
	sprintf(info_string, "Xerxes reported an error trying to set GAINDAC0 to 0x%x", GAINDAC);
	pslLogError("pslCalibrateSystemGain", info_string, status);
	return status;
    }

    /* Update the onboard DACs */
    status = pslDoControlTask(detChan, CT_DGFG200_SETDACS, info);
    if (status != XIA_SUCCESS) 
    {
	sprintf(info_string, "Unable to set the DAC values on the module");
	pslLogError("pslCalibrateSystemGain", info_string, status);
	return status;
    }

    /* Now measure the gain */
    status = pslMeasureSystemGain(detChan, info, defaults);
    if (status != XIA_SUCCESS) 
    {
	sprintf(info_string, "Unable to measure the system Gain");
	pslLogError("pslCalibrateSystemGain", info_string, status);
	return status;
    }
      
    /* Copy the value of the system_gain into maximum_gain */
    status = pslGetDefault("system_gain", (void *) &systemGain, defaults);
    if (status != XIA_SUCCESS) 
    {
	sprintf(info_string, "Unable to retrieve the system_gain from the defaults");
	pslLogError("pslCalibrateSystemGain", info_string, status);
	return status;
    }

    status = pslSetDefault("maximum_gain", (void *) &systemGain, defaults);
    if (status != XIA_SUCCESS) 
    {
	sprintf(info_string, "Unable to set the maximum_gain to %f in the defaults", systemGain);
	pslLogError("pslCalibrateSystemGain", info_string, status);
	return status;
    }
  
    /* Update the minimum dynamic range using the maximum gain */
    status = pslCalculateDynamicRange(systemGain, preampGain, gainScale, defaults, &dynamicRange);
    if (status != XIA_SUCCESS)
    {
	sprintf(info_string, "Unable to determine the dynamic range for a gain of %f", systemGain);
	pslLogError("pslCalibrateSystemGain", info_string, status);
	return status;
    }

    status = pslSetDefault("minimum_dynamic_range", (void *) &dynamicRange, defaults);
    if (status != XIA_SUCCESS) 
    {
	sprintf(info_string, "Unable to set the minimum_dynamic_range to %f in the defaults", systemGain);
	pslLogError("pslCalibrateSystemGain", info_string, status);
	return status;
    }

    /* Now proceed to set the GAINDAC to its maximum */
    GAINDAC = (parameter_t) (pow(2., GAINDAC_BITS) - 1.);
    statusX = dxp_set_one_dspsymbol(&detChan, "GAINDAC0", &GAINDAC);
    if (statusX != DXP_SUCCESS) 
    {
	status = XIA_XERXES;
	sprintf(info_string, "Xerxes reported an error trying to set GAINDAC0 to 0x%x", GAINDAC);
	pslLogError("pslCalibrateSystemGain", info_string, status);
	return status;
    }

    /* Update the onboard DACs */
    status = pslDoControlTask(detChan, CT_DGFG200_SETDACS, info);
    if (status != XIA_SUCCESS) 
    {
	sprintf(info_string, "Unable to set the DAC values on the module");
	pslLogError("pslCalibrateSystemGain", info_string, status);
	return status;
    }

    /* Now measure the gain */
    status = pslMeasureSystemGain(detChan, info, defaults);
    if (status != XIA_SUCCESS) 
    {
	sprintf(info_string, "Unable to measure the system Gain");
	pslLogError("pslCalibrateSystemGain", info_string, status);
	return status;
    }
      
    /* Copy the value of the system_gain into minimum_gain */
    status = pslGetDefault("system_gain", (void *) &systemGain, defaults);
    if (status != XIA_SUCCESS) 
    {
	sprintf(info_string, "Unable to retrieve the system_gain from the defaults");
	pslLogError("pslCalibrateSystemGain", info_string, status);
	return status;
    }

    status = pslSetDefault("minimum_gain", (void *) &systemGain, defaults);
    if (status != XIA_SUCCESS) 
    {
	sprintf(info_string, "Unable to set the minimum_gain to %f in the defaults", systemGain);
	pslLogError("pslCalibrateSystemGain", info_string, status);
	return status;
    }
  
    /* Update the maximum dynamic range using the minimum gain */
    status = pslCalculateDynamicRange(systemGain, preampGain, gainScale, defaults, &dynamicRange);
    if (status != XIA_SUCCESS)
    {
	sprintf(info_string, "Unable to determine the dynamic range for a gain of %f", systemGain);
	pslLogError("pslCalibrateSystemGain", info_string, status);
	return status;
    }

    status = pslSetDefault("maximum_dynamic_range", (void *) &dynamicRange, defaults);
    if (status != XIA_SUCCESS) 
    {
	sprintf(info_string, "Unable to set the maximum_dynamic_range to %f in the defaults", systemGain);
	pslLogError("pslCalibrateSystemGain", info_string, status);
	return status;
    }

    /* Now coverup our work */
    statusX = dxp_set_one_dspsymbol(&detChan, "GAINDAC0", &OLDGAINDAC);
    if (statusX != DXP_SUCCESS) 
    {
	status = XIA_XERXES;
	sprintf(info_string, "Xerxes reported an error trying to set GAINDAC0 to 0x%x", GAINDAC);
	pslLogError("pslCalibrateSystemGain", info_string, status);
	return status;
    }

    /* Update the onboard DACs */
    status = pslDoControlTask(detChan, CT_DGFG200_SETDACS, info);
    if (status != XIA_SUCCESS) 
    {
	sprintf(info_string, "Unable to set the DAC values on the module");
	pslLogError("pslCalibrateSystemGain", info_string, status);
	return status;
    }

    /* Now measure the gain
     */
    status = pslMeasureSystemGain(detChan, info, defaults);
    if (status != XIA_SUCCESS) 
    {
	sprintf(info_string, "Unable to measure the system Gain");
	pslLogError("pslCalibrateSystemGain", info_string, status);
	return status;
    }

    return status;
}

/******************************************************************************** 
 * This routine performs a ramping of the tracking DAC and then fits the 
 * resulting data to a line
 ********************************************************************************/
PSL_STATIC int PSL_API pslFitTrackingDACData(int detChan, void *info, short type, double coeff[2])
{
    int status;

    unsigned long i;

    double a,b,abdiff,abmid;
  
    /* constants related to the ADC range as interpreted by the OFFSET DAC control task */
    unsigned short adcMax = 4095;

    unsigned short *stdacData;
    unsigned long tdacLen;

    double *tdacData;

    /* Execute a OFFSET DAC ramp control task */
    status = pslDoControlTask(detChan, type, info);
    if (status != XIA_SUCCESS) 
	  {
		sprintf(info_string, "Error performing the OFFSET DAC control task for detChan %i", detChan);
		pslLogError("pslFitTrackingDACData", info_string, status);
		return status;
	  }

    /* retrieve the task data using the psl routine */
    status = pslGetSpecialRunData(detChan, "offset_dac_length", (void *) &tdacLen, NULL);
    if (status != XIA_SUCCESS) 
	  {
		sprintf(info_string, "Error getting the length of the OFFSET DAC data for detChan %i", detChan);
		pslLogError("pslFitTrackingDACData", info_string, status);
		return status;
	  }
    if (tdacLen == 0) 
	  {
		status = XIA_BAD_DATA_LENGTH;
		sprintf(info_string, "Length of the OFFSET DAC data is ZERO for detChan %i", detChan);
		pslLogError("pslFitTrackingDACData", info_string, status);
		return status;
	  }

    /* Allocate memory for the tdacWave */
    stdacData = (unsigned short *) utils->funcs->dxp_md_alloc(tdacLen * sizeof(unsigned short));
    tdacData = (double *) utils->funcs->dxp_md_alloc(tdacLen * sizeof(double));

    /* Now get the data */
    status = pslGetSpecialRunData(detChan, "offset_dac_data", (void *) stdacData, NULL);
    if (status != XIA_SUCCESS) 
	  {
		sprintf(info_string, "Error getting the OFFSET DAC data for detChan %i", detChan);
		pslLogError("pslFitTrackingDACData", info_string, status);
		utils->funcs->dxp_md_free(stdacData);
		utils->funcs->dxp_md_free(tdacData);
		return status;
	  }
  
    /* cast the tdac data into doubles */
    for (i = 0; i < tdacLen; i++)	
	  {
		tdacData[i] = (double) stdacData[i];
	  }
    /* Done with the array of shorts */
    utils->funcs->dxp_md_free(stdacData);
	
    /* clean up the array by removing entries that are out of bounds, or on a constant level */
    for (i = tdacLen - 1; i > 0; i--)
	  {
		if ((tdacData[i] > adcMax) || (tdacData[i] == tdacData[i-1]))
		  {
			tdacData[i] = 0;
		  }
	  }
    /* take care of the 0th element last, always lose it */
    tdacData[0] = 0.0;

    /* Another pass through the array, removing any element that is surrounded by ZEROs */
    for(i = 1; i < tdacLen - 1; i++)
	  {
		if(tdacData[i] != 0)	/* remove out-of-range points and failed measurements */
		  {
			if ((tdacData[i - 1] == 0) && (tdacData[i + 1] == 0)) 
			  {
				tdacData[i] = 0;
			  }
		  }
	  }
	
    /* Perform a linear fit to these data */
    coeff[0] = 0;	
    coeff[1] = tdacLen - 1;
    status = pslPositiveDefiniteLinefit(tdacData, coeff);
    if (status == XIA_SUCCESS)
	  {
		a = -coeff[0] / coeff[1];
		a = MIN(MAX(0, a), tdacLen - 1);
		b = (adcMax - coeff[0]) / coeff[1];
		b = MIN(MAX(0, b), tdacLen - 1);
		abdiff = (float) (fabs(b - a) / 2.);	
		abmid = (b + a) / 2.;
		a = (float) (ceil(abmid - (0.5 * abdiff)));	
		b = (float) (floor(abmid + (0.5 * abdiff)));
		coeff[0] = a;	
		coeff[1] = b;
		
		status = pslPositiveDefiniteLinefit(tdacData, coeff);
		if (status == XIA_SUCCESS)
		  {
			a = -coeff[0] / coeff[1];
			a = MIN(MAX(0, a), tdacLen - 1);
			b = (adcMax - coeff[0]) / coeff[1];
			b = MIN(MAX(0, b), tdacLen - 1);
			abdiff = (float) (fabs(b - a) / 2.);	
			abmid  = (b + a) / 2.;
			a = (float) (ceil(abmid - (0.9 * abdiff)));	
			b = (float) (floor(abmid + (0.9 * abdiff)));
			coeff[0] = a;	
			coeff[1] = b;
			
			status = pslPositiveDefiniteLinefit(tdacData, coeff);
		  }
	  } 

    /* Finished with the tdacData array, free up the memory */
    utils->funcs->dxp_md_free(tdacData);
	
    if (status != XIA_SUCCESS)
	  {
		sprintf(info_string, "Error performing linear fit(s) to the Tracking DAC data for detChan %i", detChan);
		pslLogError("pslFitTrackingDACData", info_string, status);
		return status;
	  }

    return status; 
}

/******************************************************************************** 
 * This routine performs a linear fit to positive definite data, any negative or 
 * zero point will be ignored!
 ********************************************************************************/
PSL_STATIC int PSL_API pslPositiveDefiniteLinefit(double *data, double coeff[2])
{
    int status = XIA_SUCCESS; 

    unsigned long i;
    unsigned long ndata;

    double sxx, sx, sy, syx;
  
    sxx   = 0.;	
    sx    = 0.;	
    sy    = 0.;	
    syx   = 0.; 
    ndata = 0;
  
    sprintf(info_string, "Fit Limits: low = %f, high = %f", coeff[0], coeff[1]);
    pslLogDebug("pslPositiveDeiniteLinefit", info_string);

    for(i = (unsigned long) coeff[0]; i <= (unsigned long) coeff[1]; i++)
    {
	if(data[i] <= 0) 
	{
	    continue;
	}
	sx  += i;	
	sxx += i*i;
	sy  += data[i];
	syx += data[i]*i;
	ndata++;
    }

    sprintf(info_string, "Number of points used in the linear fit = %lu", ndata);
    pslLogDebug("pslPositiveDeiniteLinefit", info_string);
  
    if(ndata > 1)
    {
	coeff[1] = (syx - ((sx * sy) / ((double) ndata))) / (sxx - ((sx * sx) / ((double) ndata)));
	coeff[0] = (sy - (coeff[1] * sx)) / ((double) ndata);
	status = XIA_SUCCESS;
    }
    else
    {
	status = XIA_NO_LINEAR_FIT;
	sprintf(info_string, "Error performing linear fit: too few data points (npoints = %lu)", ndata);
	pslLogError("pslPositiveDefiniteLinefit", info_string, status);
    }
  
    return status;
}


PSL_STATIC int pslBoardOperation(int detChan, char *name, void *value,
								 XiaDefaults *defs) 
{
  int status = XIA_SUCCESS;
  int statusX = DXP_SUCCESS;

  static char *delim = ":";
  char *rw = "r";
  char *tok;

  /* Parsed values from the name */
  char *operation;
  boolean_t read = TRUE_;
  unsigned long len = 1;

  
  UNUSED(defs);


  /* Determine what type of operation to perform */

  /* Currently all operations get passed down to read/write register, in future we will 
   * include memory read/writes as well */
  
  /* use a tokenizer to pull out the name, read/write and length */
  tok = strtok(name, delim);
  if (tok != NULL)
    {
      /* Allocate memory for the operation */
      operation = (char *) utils->funcs->dxp_md_alloc((strlen(tok) + 1) * sizeof(char));
      strcpy(operation, tok);
      
      /* Now determine if we are reading or writing */
      tok = strtok(NULL, delim);
      if (tok != NULL) 
	{
	  /* Determine read/write operation type */
	  rw[0] = (char) tolower(tok[0]);
	  if (STREQ(rw, "r")) 
	    {
	      read = TRUE_;
	    } else if (STREQ(rw, "w")) {  
	      read = FALSE_;
	    } else {
	      utils->funcs->dxp_md_free(operation);

	      status = XIA_UNKNOWN_VALUE;
	      sprintf(info_string, "Error in access type: %s, use r/w to indicate read/write", name);
	      pslLogError("pslBoardOperation", info_string, status);
	      
	      return status;
	    }
	  
	  /* Determine the length */
	  tok = strtok(NULL, delim);
	  if (tok != NULL) 
	    {
	      len = strtoul(tok, NULL, 10);
	      /* Check the result */
	      if ((len == 0) || (len == ULONG_MAX))
		{
		  utils->funcs->dxp_md_free(operation);
		  
		  status = XIA_UNKNOWN_VALUE;
		  sprintf(info_string, "Error in length: %s, value must be between 0 and %ul", name, ULONG_MAX);
		  pslLogError("pslBoardOperation", info_string, status);
		  
		  return status;
		}
	    }
	  /* if no length, then default to what is defined at start of routine */
	}
      /* If no token, then default is read and len=1 as defined at start of routine */

      /* Now parse out the result to the proper routine */
      if (len == 1) 
	{
	  if (read) 
	    {
	      statusX = dxp_read_register(&detChan, operation, value);

	      if (statusX != DXP_SUCCESS) 
		{
		  utils->funcs->dxp_md_free(operation);
		  
		  status = XIA_XERXES;
		  sprintf(info_string, "Error reading register in XerXes: operation = %s", operation);
		  pslLogError("pslBoardOperation", info_string, status);
		  
		  return status;
		}
	    } else {
	      statusX = dxp_write_register(&detChan, operation, value);

	      if (statusX != DXP_SUCCESS) 
		{
		  utils->funcs->dxp_md_free(operation);
		  
		  status = XIA_XERXES;
		  sprintf(info_string, "Error writing register in XerXes: operation = %s", operation);
		  pslLogError("pslBoardOperation", info_string, status);
		  
		  return status;
		}
	    }
	} else {
	  /* Here is code for reading/writing memory of length > 1 */
	}
    } else {
      status = XIA_UNKNOWN_VALUE;
      sprintf(info_string, "Error in name: %s, use : to delimit", name);
      pslLogError("pslBoardOperation", info_string, status);
      
      return status;
    }
  /* Free up memory */
  utils->funcs->dxp_md_free(operation);
  
  return XIA_SUCCESS;
}


/**********
 * Calls the associated Xerxes exit routine as part of 
 * the board-specific shutdown procedures.
 **********/
PSL_STATIC int PSL_API pslUnHook(int detChan)
{
    int statusX;
    int status;

    
    statusX = dxp_exit(&detChan);

    if (statusX != DXP_SUCCESS) {
	status = XIA_XERXES;
	sprintf(info_string, "Error shutting down detChan %d", detChan);
	pslLogError("pslUnHook", info_string, status);
	return status;
    } 

    return XIA_SUCCESS;
}

/*******************************************************************************
 *
 *   DoEnableBaselineCut
 *
 *******************************************************************************/

PSL_STATIC int PSL_API pslDoEnableBaselineCut(int detChan, void* value, XiaDefaults* defaults)
{
   int status = XIA_SUCCESS;

   double blCut = 0.0;
   
   /* Set acquisition value enable_baseline_cut, *value = 1 is enabled, *value = 0 is disabled */ 
   status = pslSetDefault("enable_baseline_cut", value, defaults);
   if (status != XIA_SUCCESS) 
      {
	 sprintf(info_string, "Unable to execute pslSetDefault, enable_baseline_cut");
	 pslLogError("pslDoEnableBaselineCut", info_string, status);
	 return status;
      }

   /* Get the default value of baseline_cut, returned in value */
   status = pslGetDefault("baseline_cut", ((void *) &blCut), defaults);
   if (status != XIA_SUCCESS) 
      {
	 sprintf(info_string, "Unable to execute pslGetDefault, baseline_cut");
	 pslLogError("pslDoEnableBaselineCut", info_string, status);
	 return status;
      }
   
   /* Call pslDoBaselineCut, pass default value of baseline_cut, passed in *value */
   status = pslDoBaselineCut(detChan, ((void *) &blCut), defaults);
   if (status != XIA_SUCCESS) 
      {
	 sprintf(info_string, "Unable to execute pslDoBaselineCut");
	 pslLogError("pslDoEnableBaselineCut", info_string, status);
	 return status;
      }
   
   sprintf(info_string, "New EnableBaselineCut value = %d\n", *((int*)value));
   pslLogDebug("pslDoEnableBaselineCut", info_string);
   
   return XIA_SUCCESS;
}

/*******************************************************************************
 *
 *   pslDoEnableBaselineCut
 * 
 * This routine will return the eV per ADC bit for the polaris
 *
 *******************************************************************************/
PSL_STATIC int PSL_API pslGetEVPerADC(XiaDefaults* defaults, double* eVPerADC)
{
   int status = XIA_SUCCESS;

   double adcPercentRule = 0.0;
   double calibEV = 0.0;
   
   /* Calculate the evPerADC value for converting the BLCUT to energy */
   status = pslGetDefault("adc_percent_rule", (void *) &adcPercentRule, defaults);
   if (status != XIA_SUCCESS)
      {
	 pslLogError("pslGetEVPerADC", "Unable to retrieve adc_percent_rule acquisition value", status);
	 return status;
      }
   
   status = pslGetDefault("calibration_energy", (void *) &calibEV, defaults);
   if (status != XIA_SUCCESS)
      {
	 pslLogError("pslGetEVPerADC", "Unable to retrieve calibration_energy acquisition value", status);
	 return status;
      }
   
   if (adcPercentRule > 0.0) 
      {
	 *eVPerADC = (double) (calibEV / ((adcPercentRule / 100.0) * ADC_BITS_MAX));
      } else {
	 status = XIA_GAIN_OOR;
	 sprintf(info_string, "ADC Percent Rule is not legal = %lf", adcPercentRule);
	 pslLogError("pslGetEVPerADC", info_string, status);
	 return status;
      }

   sprintf(info_string, "eVPerADC = %lf\n", *eVPerADC);
   pslLogDebug("pslGetEVPerADC", info_string);

   return status;
}
/**************************************************************************************
 *
 *   DoBaselineCut
 *
 **************************************************************************************/

PSL_STATIC int PSL_API pslDoBaselineCut(int detChan, void* value, XiaDefaults* defaults)
{
   int status = XIA_SUCCESS;
   int statusX = DXP_SUCCESS;
   parameter_t BLCUT = 0;
   parameter_t BLCUT0 = 0;
   double dSLOWLENGTH = 0;
   
   double value2 = 0.0;
   double keVPerADC = 0.0;

   /* Get the eVPerADC */
   status = pslGetEVPerADC(defaults, &keVPerADC);
   if (status != XIA_SUCCESS) 
      {
	 sprintf(info_string, "Unable to determine eVPerADC");
	 pslLogError("pslDoBaselineCut", info_string, status);
	 return status;
      }
   /* Actual return value is in eV, convert to keV */
   keVPerADC /= 1000.;
   
   sprintf(info_string, "BLCUT = %d, keVPerADC = %lf\n", BLCUT, keVPerADC);
   pslLogDebug("pslDoBaselineCut", info_string);

   statusX = dxp_get_dspsymbol(&detChan, "SLOWLENGTH0", &dSLOWLENGTH);
   if(statusX != DXP_SUCCESS)
      {
	 status = XIA_XERXES;
	 pslLogError("pslDoBaselineCut", "Error getting the dsp parameter, SLOWLENGTH0", status);
	 return status;
      }
   /* The DSP expects the BLCUT to be in filter values (multiplied by SLOWLEN) */
   /* Convert the BLCUT value to DSP units */
   BLCUT = (parameter_t) ROUND(*((double *) value) / keVPerADC * dSLOWLENGTH);

   sprintf(info_string, "BLCUT = %d, keVPerADC = %lf, SLOWLENGTH = %lf", BLCUT, keVPerADC, dSLOWLENGTH);
   pslLogDebug("pslDoBaselineCut", info_string);

   /* Now reset the acquisition value to account for rounding */
   *((double *) value) = ((double) BLCUT) * keVPerADC / dSLOWLENGTH;

   sprintf(info_string, "value = %lf, BLCUT = %d, keVPerADC = %lf", *((double *) value), BLCUT, keVPerADC);
   pslLogDebug("pslDoBaselineCut", info_string);

   /* Get the default value of enable_baseline_cut, returned in value */
   status = pslGetDefault("enable_baseline_cut", (void *) &value2, defaults);
   if (status != XIA_SUCCESS) 
      {
	 sprintf(info_string, "Unable to execute pslGetDefault, enable_baseline_cut ");
	 pslLogError("pslDoBaselineCut", info_string, status);
	 return status;
      }
   
   /* If enable_baseline_cut is 1 then set BLCUT0 parameter to the value passed into this function */
   /* Else set BLCUT0 parameter to zero */
   if(value2 == 1.0)
      statusX = dxp_set_one_dspsymbol(&detChan, "BLCUT0", &BLCUT);
   else
      statusX = dxp_set_one_dspsymbol(&detChan, "BLCUT0", &BLCUT0);
   
   if(statusX != DXP_SUCCESS)
      {
	 status = XIA_XERXES;
	 pslLogError("pslDoBaselineCut", "Error setting dsp parameter, BLCUT", status);
	 return status;
      }
   
   /* Regardless of the value of enable_baseline_cut we still set the default value of baseline_cut */
   /* to the value that was passed into this function */
   status = pslSetDefault("baseline_cut", value, defaults);
   if (status != XIA_SUCCESS) 
      {
	 sprintf(info_string, "Unable to set baseline_cut in the defaults list");
	 pslLogError("pslDoBaselineCut", info_string, status);
	 return status;
      }

   sprintf(info_string, "New BaselineCut value = %lf\n", *((double *) value));
   pslLogDebug("pslDoBaselineCut", info_string);
     
   return XIA_SUCCESS;
}

/***************************************************************************************
 *
 *   DoPreset
 *
 ***************************************************************************************/

PSL_STATIC int PSL_API pslDoPreset(int detChan, void* value, unsigned short type, XiaDefaults* defaults)
{
   int status;
   double presetTime = 0.0;

   /* runtime in seconds */
   presetTime = *((double *)value);
   
   switch (type) {
      /* If PRESET_STD is chosen we always set the default value to zero, infinite runtime */
   case PRESET_STD:
      presetTime = 0;
      status = pslSetDefault("preset_standard", (void*)&presetTime, defaults);
      if (status != XIA_SUCCESS) {
	 sprintf(info_string, "Error setting PRESET_STD to %f\n", presetTime);
	 pslLogError("pslDoPreset", info_string, status);
	 return status;
      }
      break; 
      /* If PRESET_RT is chosen we clear bit 12 of CHANCSRA0 which sets realtime mode */
      /* then set the realtime default value to the specified runtime in seconds */
   case PRESET_RT:
      status = pslModifyCcsraBit(detChan, FALSE_, CCSRA_PRESET_LIVE, defaults);
      if (status != XIA_SUCCESS) {
	 sprintf(info_string, "Error clearing CCSRA_PRESET_LIVE bit");
	 pslLogError("pslDoPreset", info_string, status);
	 return status;
      }
      status = pslSetDefault("preset_runtime", (void*)&presetTime, defaults);
      if (status != XIA_SUCCESS) {
	 sprintf(info_string, "Error setting PRESET_RT to %f\n", presetTime);
	 pslLogError("pslDoPreset", info_string, status);
	 return status;
      }
      break;
      /* If PRESET_LT is chosen we set bit 12 of CHANCSRA0 which sets livetime mode */
      /* then set the livetime default value to the specified runtime in seconds */
   case PRESET_LT:
      status = pslModifyCcsraBit(detChan, TRUE_, CCSRA_PRESET_LIVE, defaults);
      if (status != XIA_SUCCESS) {
	 
	 sprintf(info_string, "Error setting CCSRA_PRESET_LIVE bit");
	 pslLogError("pslDoPreset", info_string, status);
	 return status;
      }
      status = pslSetDefault("preset_livetime", (void*)&presetTime, defaults);
      if (status != XIA_SUCCESS) {
	 sprintf(info_string, "Error setting PRESET_LT to %f\n", presetTime);
	 pslLogError("pslDoPreset", info_string, status);
	 return status;
      }
      break;   
   default:
      status = XIA_UNKNOWN;
      sprintf(info_string, "PRESET, this is an impossible error");
      pslLogError("pslDoPreset", info_string, status);
      return status;
   }

   /* We call pslSetRequestedTime to convert time to units that the dsp understands */
   /* and save that time in REQTIMEA, REQTIMEB, and REQTIMEC dsp parameters*/
   status = pslSetRequestedTime(detChan, &presetTime);
   if (status != XIA_SUCCESS) {
      sprintf(info_string, "Error setting Requested Time to %lf\n", presetTime);
      pslLogError("pslDoPreset", info_string, status);
      return status;
   }
  
   return XIA_SUCCESS;
}

/************************************************************************************
 *
 *   DoResetDelay
 *
 *   Only used if your detector has an RC type preamplifier
 *
 ************************************************************************************/

PSL_STATIC int PSL_API pslDoResetDelay(int detChan, void* value, XiaDefaults* defaults, Detector* detector, 
				       int detector_chan)
{
   int status;
   int statusX;
   
   parameter_t RESETINT;
   
   /* Only set the DSP parameter if the detector type is correct */
   if (detector->type == XIA_DET_RESET) 
      {
	 /* DSP stores the reset interval in 0.25us ticks */
	 RESETINT = (parameter_t) ROUND(4.0 * *((double *) value));
	 
	 /* Write the new delay time to the DSP */
	 statusX = dxp_set_one_dspsymbol(&detChan, "RESETINT", &RESETINT);
	 
	 if (statusX != DXP_SUCCESS) 
	    {
	       status = XIA_XERXES;
	       sprintf(info_string, "Error setting RESETINT for detChan %d", detChan);
	       pslLogError("pslDoResetDelay", info_string, status);
	       return status;
	    }
	 
	 /* Modify the detector structure with the new delay time */
	 detector->typeValue[detector_chan] = *((double *) value);
      }
   
   /* Set the reset_delay entry */
   status = pslSetDefault("reset_delay", value, defaults);
   if (status != XIA_SUCCESS)
      {
	 pslLogError("pslDoResetDelay", "Error setting reset_delay in the defaults", status);
	 return status;
      }
   
   return XIA_SUCCESS;
}
