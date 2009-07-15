
/*
 * handel_dyn_firmware.c
 *
 * Created 10/03/01 -- PJF
 *
 * This file is nothing more then the routines 
 * that were once in the (now non-existent) file
 * handel_dynamic_config.c. 
 *
 * Copyright (c) 2002,2003,2004 X-ray Instrumentation Associates
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
#include <string.h>
#include <ctype.h>

#include "xerxes.h"
#include "xerxes_errors.h"
#include "xerxes_structures.h"
#include "xia_handel.h"
#include "handel_generic.h"
#include "handeldef.h"
#include "handel_errors.h"
#include "xia_handel_structures.h"
#include "xia_common.h"
#include "xia_assert.h"


HANDEL_STATIC int HANDEL_API xiaSetFirmwareItem(FirmwareSet *fs, Firmware *f, 
						char *name, void *value);
HANDEL_STATIC boolean_t HANDEL_API xiaIsPTRRFree(Firmware *firmware, 
					       unsigned short pttr);


/*****************************************************************************
 *
 * This routine creates a new Firmware entry
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaNewFirmware(char *alias)
{
    int status = XIA_SUCCESS;

    FirmwareSet *current=NULL;

    /* If HanDeL isn't initialized, go ahead and call it... */
    if (!isHandelInit)
    {
	status = xiaInitHandel();
	if (status != XIA_SUCCESS)
	{
	    fprintf(stderr, "FATAL ERROR: Unable to load libraries.\n");
	    exit(XIA_INITIALIZE);
	}

	xiaLogWarning("xiaNewFirmware", "HanDeL was initialized silently");
    }

    if ((strlen(alias) + 1) > MAXALIAS_LEN)
    {
	status = XIA_ALIAS_SIZE;
	sprintf(info_string, "Alias contains too many characters");
	xiaLogError("xiaFirmwareDetector", info_string, status);
	return status;
    }
    
    sprintf(info_string, "alias = %s", alias);
    xiaLogDebug("xiaNewFirmware", info_string);

    /* First check if this alias exists already? */
    current = xiaFindFirmware(alias);

    if (current != NULL)
    {
	status = XIA_ALIAS_EXISTS;
	sprintf(info_string,"Alias %s already in use.", alias);
	xiaLogError("xiaNewFirmware", info_string, status);
	return status;
    }

    /* Check that the Head of the linked list exists */
    if (xiaFirmwareSetHead == NULL)
    {
	/* Create an entry that is the Head of the linked list */
	xiaFirmwareSetHead = (FirmwareSet *) handel_md_alloc(sizeof(FirmwareSet));
	current = xiaFirmwareSetHead;

    } else {
	/* Find the end of the linked list */
	current= xiaFirmwareSetHead;

	while (current->next != NULL)
	{
	    current= current->next;
	}
	current->next = (FirmwareSet *) handel_md_alloc(sizeof(FirmwareSet));
	current= current->next;
    }

    /* Make sure memory was allocated */
    if (current == NULL)
    {
	status = XIA_NOMEM;
	sprintf(info_string,"Unable to allocate memory for Firmware alias %s.", alias);
	xiaLogError("xiaNewFirmware", info_string, status);
	return status;
    }

    /* Do any other allocations, or initialize to NULL/0 */
    current->alias = (char *) handel_md_alloc((strlen(alias) + 1) * sizeof(char));
    if (current->alias == NULL)
    {
	status = XIA_NOMEM;
	xiaLogError("xiaNewFirmware", "Unable to allocate memory for current->alias", status);
	return status;
    }

    strcpy(current->alias,alias);

    current->filename    = NULL;
    current->keywords    = NULL;
    current->numKeywords = 0;
    current->tmpPath     = NULL;
    current->mmu         = NULL;
    current->firmware    = NULL;
    current->next        = NULL;

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine adds information about a Firmware Item entry
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaAddFirmwareItem(char *alias, char *name, void *value)
{
    int status = XIA_SUCCESS;

    unsigned int i;

    char strtemp[MAXALIAS_LEN];

    FirmwareSet *chosen = NULL;

    /* Have to keep a pointer for the current Firmware ptrr */
    static Firmware *current = NULL;

    /* Locate the FirmwareSet entry first */
    chosen = xiaFindFirmware(alias);
    if (chosen == NULL)
    {
	status = XIA_NO_ALIAS;
	sprintf(info_string,"Alias %s has not been created.", alias);
	xiaLogError("xiaAddFirmwareItem", info_string, status);
	return status;
    }

    /* convert the name to lower case */
    for (i = 0; i < (unsigned int)strlen(name); i++)
    {
	strtemp[i] = (char)tolower(name[i]);
    }
    strtemp[strlen(name)] = '\0';

    /* Check that the value is not NULL */
    if (value == NULL)
    {
	status = XIA_BAD_VALUE;
	sprintf(info_string,"Value for item '%s' can not be NULL", name);
	xiaLogError("xiaAddFirmwareItem", info_string, status);
	return status;
    }

    /* Switch thru the possible entries */
    if (STREQ(strtemp, "filename"))
    {
	status = xiaSetFirmwareItem(chosen, NULL, strtemp, value);

	if (status != XIA_SUCCESS)
	{
	    sprintf(info_string,"Failure to set Firmware data: %s", name);
	    xiaLogError("xiaAddFirmwareItem", info_string, status);
	    return status;
	}

	/* Specifying the ptrr? */
    } else if (STREQ(strtemp, "ptrr")) {
	/* Create a new firmware structure */

	if (!xiaIsPTRRFree(chosen->firmware, *((unsigned short *)value)))
	{
	    status = XIA_BAD_PTRR;
	    sprintf(info_string, "PTRR %u already exists", *((unsigned short *)value));
	    xiaLogError("xiaAddFirmwareItem", info_string, status);
	    return status;
	}

	if (chosen->firmware == NULL)
	{
	    chosen->firmware = (Firmware *) handel_md_alloc(sizeof(Firmware));
	    current = chosen->firmware;
	    current->prev = NULL;

	} else {

	    current->next = (Firmware *) handel_md_alloc(sizeof(Firmware));
	    current->next->prev = current;
	    current = current->next;
	}

	if (current == NULL)
	{
	    status = XIA_NOMEM;
	    xiaLogError("xiaAddFirmwareItem", "Unable to allocate memory for firmware", status);
	    return status;
	}

	current->ptrr = *((unsigned short *) value);
	/* Initialize the elements */
	current->dsp        = NULL;
	current->fippi      = NULL;
	current->max_ptime  = 0.;
	current->min_ptime  = 0.;
	current->user_fippi = NULL;
	current->next       = NULL;
	current->numFilter  = 0;
	current->filterInfo = NULL;

	/* one of the FPGA values? */
    } else {

	status = xiaSetFirmwareItem(chosen, current, strtemp, value);

	if (status != XIA_SUCCESS)
	{
	    status = XIA_BAD_VALUE;
	    sprintf(info_string,"Failure to set Firmware data: %s", name);
	    xiaLogError("xiaAddFirmwareItem", info_string, status);
	    return status;
	}
    }

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine modifies information about a Firmware Item entry
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaModifyFirmwareItem(char *alias, unsigned short ptrr, char *name, void *value)
{
    int status = XIA_SUCCESS;

    unsigned int i;

    char strtemp[MAXALIAS_LEN];

    FirmwareSet *chosen = NULL;

    Firmware *current = NULL;

    /* Check that the value is not NULL */
    if (value == NULL)
    {
	status = XIA_BAD_VALUE;
	sprintf(info_string,"Value can not be NULL");
	xiaLogError("xiaModifyFirmwareItem", info_string, status);
	return status;
    }

    /* convert the name to lower case */
    for (i = 0; i < (unsigned int)strlen(name); i++)
    {
	strtemp[i] = (char)tolower(name[i]);
    }
    strtemp[strlen(name)] = '\0';

    /* Locate the FirmwareSet entry first */
    chosen = xiaFindFirmware(alias);
    if (chosen == NULL)
    {
	status = XIA_NO_ALIAS;
	sprintf(info_string,"Alias %s was not found.", alias);
	xiaLogError("xiaModifyFirmwareItem", info_string, status);
	return status;
    }

    /* Check to see if the name is a ptrr-invariant name since some
     * users will probably set ptrr to NULL under these circumstances
     * which breaks the code if a ptrr check is performed
     */
    if (STREQ(name, "filename") ||
        STREQ(name, "mmu") ||
        STREQ(name, "fdd_tmp_path"))
    {
      status = xiaSetFirmwareItem(chosen, current, name, value);
      
      if (status != XIA_SUCCESS) {
        sprintf(info_string, "Failure to set '%s' for '%s'", name, alias);
        xiaLogError("xiaModifyFirmwareItem", info_string, status);
        return status;
      }

      return status;
    }


    /* Now find the ptrr only if the name being modified requires it */
    current = chosen->firmware;
    while ((current != NULL) && (current->ptrr != ptrr))
    {
	current=current->next;
    }

    if (current == NULL)
    {
	status = XIA_BAD_VALUE;
	sprintf(info_string,"ptrr (%u) not found.", ptrr);
	xiaLogError("xiaModifyFirmwareItem", info_string, status);
	return status;
    }

    /* Now modify the value */
    status = xiaSetFirmwareItem(chosen, current, strtemp, value);
    if (status != XIA_SUCCESS)
    {
	status = XIA_BAD_VALUE;
	sprintf(info_string,"Failure to set Firmware data: %s", name);
	xiaLogError("xiaModifyFirmwareItem", info_string, status);
	return status;
    }

    return status;
}


/*****************************************************************************
 *
 * This routine retrieves data from a Firmware Set
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaGetFirmwareItem(char *alias, unsigned short ptrr, char *name, void *value)
{
    int status = XIA_SUCCESS;

    unsigned int i;

    unsigned short j;

    char strtemp[MAXALIAS_LEN];

    unsigned short *filterInfo = NULL;

    FirmwareSet *chosen = NULL;

    Firmware    *current = NULL;


    /* Find Firmware corresponding to alias */
    chosen = xiaFindFirmware(alias);

    if (chosen == NULL) {

	status = XIA_NO_ALIAS;
	sprintf(info_string, "Alias %s has not been created", alias);
	xiaLogError("xiaGetFirmwareItem", info_string, status);
	return status;
    }

    /* Convert name to lower case */
    for (i = 0; i < (unsigned int)strlen(name); i++) {

	strtemp[i] = (char)tolower(name[i]);
    }

    strtemp[strlen(name)] = '\0';

    /* Decide which value to return. Start with the ptrr-invariant values */
    if (STREQ(strtemp, "filename")) {

	/* Reference: BUG ID #13
	 * Reference: BUG ID #69
	 * The new default behavior is to return
	 * a blank string in place of the filename
	 * and to not error out.
	 */
	if (chosen->filename == NULL) {
	    sprintf(info_string, "No filename defined for firmware with alias %s", chosen->alias);
	    xiaLogInfo("xiaGetFirmwareItem", info_string);
	    
	    strcpy((char *)value, "");

	} else { 
	    strcpy((char *)value, chosen->filename);
	}

    } else if (STREQ(strtemp, "fdd_tmp_path")) {

      if (chosen->filename == NULL) {
        sprintf(info_string, "No FDD file for '%s'", chosen->alias);
        xiaLogError("xiaGetFirmwareItem", info_string, XIA_NO_FILENAME);
        return XIA_NO_FILENAME;
      }

      if (chosen->tmpPath == NULL) {
        sprintf(info_string, "FDD temporary file path never defined for '%s'",
                chosen->alias);
        xiaLogError("xiaGetFirmwareItem", info_string, XIA_NO_TMP_PATH);
        return XIA_NO_TMP_PATH;
      }

      ASSERT((strlen(chosen->tmpPath) + 1) < MAXITEM_LEN);

      strcpy((char *)value, chosen->tmpPath);

    } else if (STREQ(strtemp, "mmu")) {

	/* Reference: BUG ID #12 */
	if (chosen->mmu == NULL) {

	    status = XIA_NO_FILENAME;
	    sprintf(info_string, "No MMU file defined for firmware with alias %s", chosen->alias);
	    xiaLogError("xiaGetFirmwareItem", info_string, status);
	    return status;
	}

	strcpy((char *)value, chosen->mmu);

    } else {
	/* Should be branching into names that require the ptrr value...if not
	 * we'll still catch it at the end and everything will be fine
	 */
	current = chosen->firmware;
	/* Have to check to see if current is initally NULL as well. While this should
	 * be a rare case and was discovered my a malicious test that I wrote, we need
	 * to protect against it.
	 */
	if (current == NULL)
	{
	    status = XIA_BAD_VALUE;
	    sprintf(info_string, "No ptrr(s) defined for this alias: %s", alias);
	    xiaLogError("xiaGetFirmwareItem", info_string, status);
	    return status;
	}

	while (current->ptrr != ptrr)
	{
	    /* Check to see if we ran into the end of the list here... */
	    current = current->next;

	    if (current == NULL)
	    {
		status = XIA_BAD_PTRR;
		sprintf(info_string, "ptrr %u is not valid for this alias", ptrr);
		xiaLogError("xiaGetFirmwareItem", info_string, status);
		return status;
	    }
	}


	if (STREQ(strtemp, "min_peaking_time"))
	{
	    *((double *)value) = current->min_ptime;

	} else if (STREQ(strtemp, "max_peaking_time")) {

	    *((double *)value) = current->max_ptime;

	} else if (STREQ(strtemp, "fippi")) {

	    strcpy((char *)value, current->fippi);

	} else if (STREQ(strtemp, "dsp")) {

	    strcpy((char *)value, current->dsp);

	} else if (STREQ(strtemp, "user_fippi")) {

	    strcpy((char *)value, current->user_fippi);

	} else if (STREQ(strtemp, "num_filter")) {

	    /* Reference: BUG ID #8 */

	    *((unsigned short *)value) = current->numFilter;
		
	} else if (STREQ(strtemp, "filter_info")) {

	    /* Do a full copy here
	     * Reference: BUG ID #8
	     */
	    filterInfo = (unsigned short *)value;

	    for (j = 0; j < current->numFilter; j++) {

		filterInfo[j] = current->filterInfo[j];
	    }

	} else {
	    /* Bad name */
	    status = XIA_BAD_NAME;
	    sprintf(info_string, "Invalid Name: %s", name);
	    xiaLogError("xiaGetFirmwareItem", info_string, status);
	    return status;
	}
	/* Bad names propogate all the way to the end so there is no reason
	 * to report an error message here.
	 */
    }

    return status;
}


/**********
 * This routine gets the number of firmwares in the system.
 **********/
HANDEL_EXPORT int HANDEL_API xiaGetNumFirmwareSets(unsigned int *numFirmware)
{
    unsigned int count = 0;

    FirmwareSet *current = xiaFirmwareSetHead;


    while (current != NULL) {

	count++;
	current = getListNext(current);
    }

    *numFirmware = count;

    return XIA_SUCCESS;
}


/**********
 * This routine returns a list of the firmware aliases
 * currently defined in the system. Assumes that the
 * calling routine has allocated enough memory in
 * "firmwares".
 **********/
HANDEL_EXPORT int HANDEL_API xiaGetFirmwareSets(char *firmwares[])
{
    int i;

    FirmwareSet *current = xiaFirmwareSetHead;


    for (i = 0; current != NULL; current = getListNext(current)) {

	strcpy(firmwares[i], current->alias);
    }

    return XIA_SUCCESS;
}


/**********
 * This routine is similar to xiaGetFirmwareSets() but
 * VB can't pass string arrays to a DLL, so it only
 * returns a single alias string.
 **********/
HANDEL_EXPORT int HANDEL_API xiaGetFirmwareSets_VB(unsigned int index, char *alias)
{
    int status;

    unsigned int curIdx;

    FirmwareSet *current = xiaFirmwareSetHead;


    for (curIdx = 0; current != NULL; current = getListNext(current), curIdx++) {

	if (curIdx == index) {

	    strcpy(alias, current->alias);

	    return XIA_SUCCESS;
	}
    }

    status = XIA_BAD_INDEX;
    sprintf(info_string, "Index = %u is out of range for the firmware set list", index);
    xiaLogError("xiaGetFirmwareSets_VB", info_string, status);
    return status;
}


/**********
 * This routine returns the # of PTRRs defined for the
 * specified FirmwareSet alias. If a FDD file is defined
 * then an error is reported since the information 
 * in the FDD doesn't translate directly into PTRRs.
 **********/
HANDEL_EXPORT int HANDEL_API xiaGetNumPTRRs(char *alias, unsigned int *numPTRR)
{
    int status;

    unsigned int count = 0;

    FirmwareSet *chosen = NULL;
  
    Firmware *current = NULL;


    chosen = xiaFindFirmware(alias);

    if (chosen == NULL) {

	status = XIA_NO_ALIAS;
	sprintf(info_string, "Alias %s has not been created yet", alias);
	xiaLogError("xiaGetNumPTRRs", info_string, status);
	return status;
    }

    if (chosen->filename != NULL) {

	status = XIA_LOOKING_PTRR;
	sprintf(info_string, 
		"Looking for PTRRs and found an FDD file for alias %s", alias);
	xiaLogError("xiaGetNumPTRRs", info_string, status);
	return status;
    }

    current = chosen->firmware;

    while (current != NULL) {

	count++;

	current = getListNext(current);
    }

    *numPTRR = count;

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine modifies information about a Firmware Item entry
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaSetFirmwareItem(FirmwareSet *fs, Firmware *f, char *name, void *value)
{
    int status = XIA_SUCCESS;

    unsigned int i;
    unsigned int len;

    char **oldKeywords = NULL;

    parameter_t *oldFilterInfo = NULL;


    /* Specify the mmu */
    if (STREQ(name, "mmu"))
    {
	/* If allocated already, then free */
	if (fs->mmu != NULL)
	{
	    handel_md_free((void *)fs->mmu);
	}
	/* Allocate memory for the filename */
	len = (unsigned int)strlen((char *) value);
	fs->mmu = (char *) handel_md_alloc((len + 1) * sizeof(char));
	if (fs->mmu == NULL)
	{
	    status = XIA_NOMEM;
	    xiaLogError("xiaSetFirmwareItem", "Unable to allocate memory for fs->mmu", status);
	    return status;
	}

	/* Copy the filename into the firmware structure */
	strcpy(fs->mmu,(char *) value);

    } else if (STREQ(name, "filename")) {

	if (fs->filename != NULL) {

	    handel_md_free((void *)fs->filename);
	}
	len = (unsigned int)strlen((char *)value);
	fs->filename = (char *)handel_md_alloc((len + 1) * sizeof(char));
	if (fs->filename == NULL)
	{
	    status = XIA_NOMEM;
	    xiaLogError("xiaSetFirmwareItem", "Unable to allocate memory for fs->filename", status);
	    return status;
	}

	strcpy(fs->filename, (char *)value);
	
    } else if (STREQ(name, "fdd_tmp_path")) {

      len = strlen((char *)value);

      fs->tmpPath = handel_md_alloc(len + 1);

      if (!fs->tmpPath) {
        sprintf(info_string, "Unable to allocated %d bytes for 'fs->tmpPath' with "
                "alias = '%s'", len + 1, fs->alias);
        xiaLogError("xiaSetFirmwareItem", info_string, XIA_NOMEM);
        return XIA_NOMEM;
      }

      strcpy(fs->tmpPath, (char *)value);
	
    } else if (STREQ(name, "keyword")) {
	/* Check to see if the filename exists, because if it doesn't, then there is
	 * no reason to be entering keywords. N.b. There is no _real_ reason that the
	 * user should have to enter the filename first...it's just that it makes sense
	 * from a "logic" standpoint. This restriction _may_ be eased in the future.
	 */
	/* This should work, even if the number of keywords is zero */
	oldKeywords = (char **)handel_md_alloc(fs->numKeywords * sizeof(char *));

	for (i = 0; i < fs->numKeywords; i++)
	{
	    oldKeywords[i] = (char *)handel_md_alloc((strlen(fs->keywords[i]) + 1) * sizeof(char));
	    strcpy(oldKeywords[i], fs->keywords[i]);
	    handel_md_free((void *)(fs->keywords[i]));
	}

	handel_md_free((void *)fs->keywords);
	fs->keywords = NULL;

	/* The original keywords array should be free (if it previously held keywords)
	 */
	fs->numKeywords++;
	fs->keywords = (char **)handel_md_alloc(fs->numKeywords * sizeof(char *));
	if (fs->keywords == NULL)
	{
	    status = XIA_NOMEM;
	    xiaLogError("xiaSetFirmwareItem", "Unable to allocate memory for fs->keywords", status);
	    return status;
	}

	for (i = 0; i < (fs->numKeywords - 1); i++)
	{
	    fs->keywords[i] = (char *)handel_md_alloc((strlen(oldKeywords[i]) + 1) * sizeof(char));
	    if (fs->keywords[i] == NULL)
	    {
		status = XIA_NOMEM;
		xiaLogError("xiaSetFirmwareItem", "Unable to allocate memory for fs->keywords[i]", status);
		return status;
	    }

	    strcpy(fs->keywords[i], oldKeywords[i]);
	    handel_md_free((void *)oldKeywords[i]);
	}

	handel_md_free((void *)oldKeywords);

	len = (unsigned int)strlen((char *)value);
	fs->keywords[fs->numKeywords - 1] = (char *)handel_md_alloc((len + 1) * sizeof(char));
	if (fs->keywords[fs->numKeywords - 1] == NULL)
	{
	    status = XIA_NOMEM;
	    xiaLogError("xiaSetFirmwareItem", "Unable to allocate memory for fs->keywords[fs->numKeywords - 1]", status);
	    return status;
	}

	strcpy(fs->keywords[fs->numKeywords - 1], (char *)value);

    } else {
	/* Check to make sure a valid Firmware structure exists */
	if (f == NULL)
	{
	    status = XIA_BAD_VALUE;
	    sprintf(info_string,"PTRR not specified, no Firmware object exists");
	    xiaLogError("xiaAddFirmwareItem", info_string, status);
	    return status;
	}

	if (STREQ(name, "min_peaking_time"))
	{
	    /* HanDeL doesn't have enough information to validate the peaking
	     * time values here. It only has enough information to check that they
	     * are correct relative to one another. We consider "0" to signify that
	     * the min/max_peaking_times are not defined yet and, therefore, we only
	     * check when one of them is changed and they are both non-zero.
	     * In the future I will try and simplify this logic a little bit. For now,
	     * brute force!
	     */
	    f->min_ptime = *((double *)value);
	    if ((f->min_ptime != 0) && (f->max_ptime != 0))
	    {
		if (f->min_ptime > f->max_ptime)
		{
		    status = XIA_BAD_VALUE;
		    sprintf(info_string, "Min. peaking time = %f not smaller then max. peaking time", f->min_ptime);
		    xiaLogError("xiaSetFirmwareItem", info_string, status);
		    return status;
		}
	    }

	} else if (STREQ(name, "max_peaking_time")) {

	    f->max_ptime = *((double *)value);
	    if ((f->min_ptime != 0) && (f->max_ptime != 0)) 
	    {
		if (f->max_ptime < f->min_ptime) 
		{
		    status = XIA_BAD_VALUE;
		    sprintf(info_string, "Max. peaking time = %f not larger then min. peaking time", f->max_ptime);
		    xiaLogError("xiaSetFirmwareItem", info_string, status);
		    return status;
		}
	    }

	} else if (STREQ(name, "fippi")) {
	    /* If allocated already, then free */
	    if (f->fippi != NULL) 
	    {
		handel_md_free(f->fippi);
	    }
	    /* Allocate memory for the filename */
	    len = (unsigned int)strlen((char *) value);
	    f->fippi = (char *) handel_md_alloc((len + 1) * sizeof(char));
	    if (f->fippi == NULL)
	    {
		status = XIA_NOMEM;
		xiaLogError("xiaSetFirmwareItem", "Unable to allocate memory for f->fippi", status);
		return status;
	    }

	    strcpy(f->fippi, (char *) value);

	} else if (STREQ(name, "user_fippi")) {

	    if (f->user_fippi != NULL) 
	    {
		handel_md_free(f->user_fippi);
	    }

	    len = (unsigned int)strlen((char *) value);
	    f->user_fippi = (char *) handel_md_alloc((len + 1) * sizeof(char));
	    if (f->user_fippi == NULL)
	    {
		status = XIA_NOMEM;
		xiaLogError("xiaSetFirmwareItem", "Unable to allocate memory for f->user_fippi", status);
		return status;
	    }

	    strcpy(f->user_fippi, (char *) value);

	} else if (STREQ(name, "dsp")) {

	    if (f->dsp != NULL) 
	    {
		handel_md_free(f->dsp);
	    }

	    /* Allocate memory for the filename */
	    len = (unsigned int)strlen((char *) value);
	    f->dsp = (char *) handel_md_alloc((len + 1) * sizeof(char));
	    if (f->dsp == NULL)
	    {
		status = XIA_NOMEM;
		xiaLogError("xiaSetFirmwareItem", "Unable to allocate memory for f->dsp", status);
		return status;
	    }

	    strcpy(f->dsp, (char *) value);

	} else if (STREQ(name, "filter_info")) {

	    oldFilterInfo = (parameter_t *)handel_md_alloc(f->numFilter * sizeof(parameter_t));

	    for (i = 0; i < f->numFilter; i++)
	    {
		oldFilterInfo[i] = f->filterInfo[i];
	    }

	    handel_md_free((void *)f->filterInfo);
	    f->filterInfo = NULL;

	    f->numFilter++;
	    f->filterInfo = (parameter_t *)handel_md_alloc(f->numFilter * sizeof(parameter_t));

	    for (i = 0; i < (unsigned short)(f->numFilter - 1); i++)
	    {
		f->filterInfo[i] = oldFilterInfo[i];
	    }
	    f->filterInfo[f->numFilter - 1] = *((parameter_t *)value);

	    handel_md_free((void *)oldFilterInfo);
	    oldFilterInfo = NULL;

	} else {

	    status = XIA_BAD_NAME;
	    sprintf(info_string,"Invalid name %s.", name);
	    xiaLogError("xiaSetFirmwareItem", info_string, status);
	    return status;
	}
    }

    return status;
}



/*****************************************************************************
 *
 * This routine removes a Firmware entry
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaRemoveFirmware(char *alias)
{
    int status = XIA_SUCCESS;

    unsigned int i;

    char strtemp[MAXALIAS_LEN];

    FirmwareSet *prev = NULL; 
    FirmwareSet *current = NULL;
    FirmwareSet *next = NULL;

	
    if (isListEmpty(xiaFirmwareSetHead)) 
    {
	status = XIA_NO_ALIAS;
	sprintf(info_string, "Alias %s does not exist", alias);
	xiaLogError("xiaRemoveFirmware", info_string, status);
	return status;
    }

    /* Turn the alias into lower case version, and terminate with a null char */
    for (i = 0; i < (unsigned int)strlen(alias); i++) 
    {
	strtemp[i] = (char)tolower(alias[i]);
    }
    strtemp[strlen(alias)] = '\0';

    /* First check if this alias exists already? */
    prev = NULL;										 
    current = xiaFirmwareSetHead;
    next = current->next;

    while (!STREQ(strtemp, current->alias)) 
    {
	prev = current;
	current = next;
	next = current->next;
    }

    /* Check if we found nothing */
    if ((next == NULL) && 
	(current == NULL)) 
    {
	status = XIA_NO_ALIAS;
	sprintf(info_string,"Alias %s does not exist.", alias);
	xiaLogError("xiaRemoveFirmware", info_string, status);
	return status;
    }

    /* Check if match is the head of the list */
    if (current == xiaFirmwareSetHead) 
    {
	xiaFirmwareSetHead = next;

    } else {

	prev->next = next;
    }

    xiaFreeFirmwareSet(current);

    return status;
}


/*****************************************************************************
 *
 * This routine returns the entry of the Firmware linked list that matches 
 * the alias.  If NULL is returned, then no match was found.
 *
 *****************************************************************************/
HANDEL_SHARED FirmwareSet* HANDEL_API xiaFindFirmware(char *alias)
{
    unsigned int i;

    char strtemp[MAXALIAS_LEN];

    FirmwareSet *current = NULL;


    /* Turn the alias into lower case version, and terminate with a null char */
    for (i = 0; i < (unsigned int)strlen(alias); i++) 
    {
	strtemp[i] = (char)tolower(alias[i]);
    }
    strtemp[strlen(alias)] = '\0';

    /* First check if this alias exists already? */
    current= xiaFirmwareSetHead;
    while (current != NULL) 
    {
	/* If the alias matches, return a pointer to the detector */
	if (STREQ(strtemp, current->alias))
	{
	    return current;
	}

	current = current->next;
    }

    return NULL;
}


/*****************************************************************************
 *
 * Searches the Firmware LL and returns TRUE if the specified PTTR already
 * is in the list.
 *
 *****************************************************************************/
HANDEL_STATIC boolean_t HANDEL_API xiaIsPTRRFree(Firmware *firmware, unsigned short pttr)
{
    while (firmware != NULL)
    {
	if (firmware->ptrr == pttr)
	{
	    return FALSE_;
	}

	firmware = firmware->next;
    }

    return TRUE_;
}


/*****************************************************************************
 *
 * Returns the head of the FirmwareSet LL
 *
 *****************************************************************************/
HANDEL_SHARED FirmwareSet* HANDEL_API xiaGetFirmwareSetHead(void)
{
    return xiaFirmwareSetHead;
}

/*****************************************************************************
 *
 * This routine returns the number of firmware in a Firmware LL
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaGetNumFirmware(Firmware *firmware)
{
    int numFirm;

    Firmware *current = NULL;


    current = firmware;
    numFirm = 0;

    while (current != NULL)
    {
	numFirm++;
	current = current->next;
    }

    return numFirm;
}


/*****************************************************************************
 *
 * This routine compares two Firmware elements (actually, the min peaking time
 * value of each) and returns 1 if key1 > key2, 0 if key1 == key2, and -1 if
 * key1 < key2.
 *
 *****************************************************************************/
HANDEL_SHARED int xiaFirmComp(const void *key1, const void *key2)
{
    Firmware *key1_f = (Firmware *)key1;
    Firmware *key2_f = (Firmware *)key2;


    if (key1_f->min_ptime > key2_f->min_ptime)
    {
	return 1;

    } else if (key1_f->min_ptime == key2_f->min_ptime) {

	return 0;

    } else if (key1_f->min_ptime < key2_f->min_ptime) {

	return -1;
    }

    /* Compiler wants this... */
    return 0;
}


/*****************************************************************************
 *
 * This routine returns the name of the dsp code associated with the alias
 * and peakingTime.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaGetDSPNameFromFirmware(char *alias, double peakingTime, char *dspName)
{
    FirmwareSet *current = NULL;

    Firmware *firmware = NULL;

    int status;


    current = xiaFindFirmware(alias);
    if (current == NULL)
    {
	status = XIA_NO_ALIAS;
	sprintf(info_string, "Unable to find firmware %s", alias);
	xiaLogError("xiaGetDSPNameFromFirmware", info_string, status);
	return status;
    }

    firmware = current->firmware;
    while (firmware != NULL)
    {
	if ((peakingTime >= firmware->min_ptime) &&
	    (peakingTime <= firmware->max_ptime))
	{
	    strcpy(dspName, firmware->dsp);
	    return XIA_SUCCESS;
	}

	firmware = getListNext(firmware);
    }

    status = XIA_BAD_VALUE;
    sprintf(info_string, "peakingTime %f does not match any of the PTRRs in %s", peakingTime, alias);
    xiaLogError("xiaGetDSPNameFromFirmware", info_string, status);
    return status;
}


/*****************************************************************************
 *
 * This returns the name of the FiPPI code associated with the alias and
 * peaking time.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaGetFippiNameFromFirmware(char *alias, double peakingTime, char *fippiName)
{
    FirmwareSet *current = NULL;

    Firmware *firmware = NULL;

    int status;


    current = xiaFindFirmware(alias);
    if (current == NULL)
    {
	status = XIA_NO_ALIAS;
	sprintf(info_string, "Unable to find firmware %s", alias);
	xiaLogError("xiaGetFippiNameFromFirmware", info_string, status);
	return status;
    }

    firmware = current->firmware;

    while (firmware != NULL)
    {
	if ((peakingTime >= firmware->min_ptime) &&
	    (peakingTime <= firmware->max_ptime))
	{
	    strcpy(fippiName, firmware->fippi);
	    return XIA_SUCCESS;
	}

	firmware = getListNext(firmware);
    }

    status = XIA_BAD_VALUE;
    sprintf(info_string, "peakingTime %f does not match any of the PTRRs in %s", peakingTime, alias);
    xiaLogError("xiaGetFippiNameFromFirmware", info_string, status);
    return status;
}


/*****************************************************************************
 *
 * This routine looks to replace the get*FromFirmware() routines by using 
 * a generic search based on peakingTime.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaGetValueFromFirmware(char *alias, double peakingTime, char *name, char *value)
{
    int status;

    FirmwareSet *current = NULL;

    Firmware *firmware = NULL;


    current = xiaFindFirmware(alias);
    if (current == NULL)
    {
	status = XIA_NO_ALIAS;
	sprintf(info_string, "Unable to find firmware %s", alias);
	xiaLogError("xiaGetValueFromFirmware", info_string, status);
	return status;
    }


    if (STREQ(name, "mmu"))
    {
	if (current->mmu == NULL)
	{
	    status = XIA_BAD_VALUE;
	    xiaLogError("xiaGetValueFromFirmware", "MMU is NULL", status);
	    return status;
	}

	strcpy(value, current->mmu);
	return XIA_SUCCESS;
    }

    /* Hacky way of dealing with the
     * special uDXP FiPPI types.
     */
    if (STREQ(name, "fippi0") ||
	STREQ(name, "fippi1") ||
	STREQ(name, "fippi2"))
    {
	strcpy(value, name);
	return XIA_SUCCESS;
    }
    
	


    firmware = current->firmware;
    while (firmware != NULL)
    {
	if ((peakingTime >= firmware->min_ptime) &&
	    (peakingTime <= firmware->max_ptime))
	{
	    if (STREQ(name, "fippi"))
	    {
		if (firmware->fippi == NULL)
		{
		    status = XIA_BAD_VALUE;
		    xiaLogError("xiaGetValueFromFirmware", "FiPPI is NULL", status);
		    return status;
		}

		strcpy(value, firmware->fippi);

		return XIA_SUCCESS;

	    } else if (STREQ(name, "user_fippi")) {

		if (firmware->user_fippi == NULL)
		{
		    status = XIA_BAD_VALUE;
		    xiaLogError("xiaGetValueFromFirmware", "User FiPPI is NULL", status);
		    return status;
		}

		strcpy(value, firmware->user_fippi);

		return XIA_SUCCESS;

	    } else if (STREQ(name, "dsp")) {

		if (firmware->dsp == NULL)
		{
		    status = XIA_BAD_VALUE;
		    xiaLogError("xiaGetValueFromFirmware", "DSP is NULL", status);
		    return status;
		}

		strcpy(value, firmware->dsp);

		return XIA_SUCCESS;
			
	    }  
	}

	firmware = getListNext(firmware);
    }

    status = XIA_BAD_VALUE;
    sprintf(info_string, "Error getting %s from %s", name, alias);
    xiaLogError("xiaGetValueFromFirmware", info_string, status);
    return status;
}
