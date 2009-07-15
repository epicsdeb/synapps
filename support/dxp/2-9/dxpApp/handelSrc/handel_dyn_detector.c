/*
 * handel_dyn_detector.c
 *
 *
 * Created 10/03/01 -- PJF
 *
 * This file is nothing more then the routines
 * that were once in the (now non-existent) file
 * handel_dynamic_config.c.
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



/** @brief Creates a new detector
 *
 * Create a new detector with the name @a alias that can be reference by other
 * routines such as xiaAddDetectorItem(char *alias, char *name, void *value),
 * xiaGetDetectorItem(char *alias, char *name, void *value), 
 * xiaModifyDetectorItem(char *alias, char *name, void *value) and
 * xiaRemoveDetector(char *alias).
 *
 * @param alias Name of the new detector to be added to the system
 *
 * @return An error value indicating success (@a XIA_SUCCESS) or failure
 * (@a XIA_ALIAS_SIZE, @a XIA_ALIAS_EXISTS or @a XIA_NOMEM)
 *
 */
HANDEL_EXPORT int HANDEL_API xiaNewDetector(char *alias)
{
	int status = XIA_SUCCESS;

	Detector *current=NULL;

/* If HanDeL isn't initialized, go ahead and call it... */
	if (!isHandelInit)
	{
		status = xiaInitHandel();
		if (status != XIA_SUCCESS)
		{
			fprintf(stderr, "FATAL ERROR: Unable to load libraries.\n");
			exit(XIA_INITIALIZE);
		}

		xiaLogWarning("xiaNewDetector", "HanDeL was initialized silently");
	}


	sprintf(info_string, "Creating new detector w/ alias = %s", alias);
	xiaLogDebug("xiaNewDetector", info_string);


	if ((strlen(alias) + 1) > MAXALIAS_LEN)
	{
		status = XIA_ALIAS_SIZE;
		sprintf(info_string, "Alias contains too many characters");
		xiaLogError("xiaNewDetector", info_string, status);
		return status;
	}

/* First check if this alias exists already? */
	current = xiaFindDetector(alias);
	if (current != NULL)
	{
		status = XIA_ALIAS_EXISTS;
		sprintf(info_string,"Alias %s already in use.", alias);
		xiaLogError("xiaNewDetector", info_string, status);
		return status;
	}
/* Check that the Head of the linked list exists */
	if (xiaDetectorHead == NULL)
	{
/* Create an entry that is the Head of the linked list */
		xiaDetectorHead = (Detector *) handel_md_alloc(sizeof(Detector));
		current = xiaDetectorHead;
	} else {
/* Find the end of the linked list */
		current = xiaDetectorHead;
		while (current->next != NULL)
		{
			current = current->next;
		}
		current->next = (Detector *) handel_md_alloc(sizeof(Detector));
		current = current->next;
	}

/* Make sure memory was allocated */
	if (current == NULL)
	{
		status = XIA_NOMEM;
		sprintf(info_string,"Unable to allocate memory for Detector alias %s.",
 alias);
		xiaLogError("xiaNewDetector", info_string, status);
		return status;
	}

/* Do any other allocations, or initialize to NULL/0 */
	current->alias = (char *) handel_md_alloc((strlen(alias)+1)*sizeof(char));
	if (current->alias == NULL)
	{
		status = XIA_NOMEM;
		xiaLogError("xiaNewDetector", "Unable to allocate memory for current->alias",
 status);
		return status;
	}

	strcpy(current->alias,alias);

	current->nchan     = 0;
	current->polarity  = NULL;
	current->gain      = NULL;
	current->type      = XIA_DET_UNKNOWN;
	current->typeValue = NULL;
	current->next      = NULL;

	return XIA_SUCCESS;
}

/** @brief Adds information to a detector
 *
 * Adds information about the detector setup using the name-value
 * pairs in the detector section of @link p_nv_pairs Handel Name-Value 
 * Pairs @endlink.
 *
 * @param alias A valid detector alias
 * 
 * @param name A valid name from the detector section of @link p_nv_pairs Handel 
 * Name-Value Pairs @endlink
 * 
 * @param value Value to set the corresponding detector information to, cast
 * into a <em>void *</em>. See the Usage section below for an example.
 *
 * @return An error value indicating success (@a XIA_SUCCESS) or failure
 * (@a XIA_BAD_VALUE, @a XIA_NO_ALIAS or @a XIA_BAD_NAME)
 *
 * @par Usage:
 * @include xiaAddDetectorItem.c
 *
 */
HANDEL_EXPORT int HANDEL_API xiaAddDetectorItem(char *alias, char *name,
												void *value)
{
	int status = XIA_SUCCESS;

	unsigned int i, slen;

	long chan = 0;

	unsigned short j;

	char strtemp[MAXALIAS_LEN];

	Detector *chosen = NULL;

/* Check that the value is not NULL */
	if (value == NULL)
	{
		status = XIA_BAD_VALUE;
		sprintf(info_string,"Value can not be NULL");
		xiaLogError("xiaAddDetectorItem", info_string, status);
		return status;
	}

/* Locate the Detector entry first */
	chosen = xiaFindDetector(alias);
	if (chosen == NULL)
	{
		status = XIA_NO_ALIAS;
		sprintf(info_string,"Alias %s has not been created.", alias);
		xiaLogError("xiaAddDetectorItem", info_string, status);
		return status;
	}

/* convert the name to lower case */
	for (i = 0; i < (unsigned int)strlen(name); i++)
	{
		strtemp[i] = (char)tolower(name[i]);
	}
	strtemp[strlen(name)] = '\0';

/* Switch thru the possible entries */
	if (STREQ(strtemp, "number_of_channels"))
	{
/* make sure the number of channels is less than max, this is mostly a check for
 * poorly passed type of value...i.e. if value is passed as a double, it will
 likely
 * look like a bogus number of channels */
		if (*((unsigned short *)value) > MAXDETECTOR_CHANNELS)
		{
			status = XIA_BAD_VALUE;
			sprintf(info_string,"Number of channels too large: %u.", *((unsigned short
 *)value));
			xiaLogError("xiaAddDetectorItem", info_string, status);
			return status;
		}
		chosen->nchan    		= *((unsigned short *) value);
		chosen->polarity 		= (unsigned short *)
 handel_md_alloc(chosen->nchan*sizeof(unsigned short));
		chosen->gain     		= (double *) handel_md_alloc(chosen->nchan*sizeof(double));
		chosen->typeValue 	= (double *)handel_md_alloc(sizeof(double) * chosen->nchan);

		if ((chosen->polarity  == NULL) ||
		    (chosen->gain      == NULL) ||
			(chosen->typeValue == NULL))
		{
			status = XIA_NOMEM;
			xiaLogError("xiaAddDetectorItem", "Unable to allocate memory for detector info", status);
			return status;
		}

		return XIA_SUCCESS;
	}


	/* The number of channels must be set first before we can do any of the
	 * other values. Why? Because memory is allocated once the number of channels
	 * is known.
	 */
	if (chosen->nchan == 0) {
	  sprintf(info_string, "Detector '%s' must set its number of channels before "
			  "setting '%s'", chosen->alias, strtemp);
	  xiaLogError("xiaAddDetectorItem", info_string, XIA_NO_CHANNELS);
	  return XIA_NO_CHANNELS;
	}

	if (STREQ(strtemp, "type")) {

		if (STREQ((char *)value, "reset"))
		{
			chosen->type = XIA_DET_RESET;

		} else if (STREQ((char *)value, "rc_feedback")) {

			chosen->type = XIA_DET_RCFEED;

		} else {

			status = XIA_BAD_VALUE;
			sprintf(info_string, "Error setting detector type for %s", chosen->alias);
			xiaLogError("xiaAddDetectorItem", info_string, status);
			return status;
		}

	} else if (STREQ(strtemp, "type_value")) {

/* This, effectively, constrains us to a "single" det type value for now.
 * If that isn't good enough for some customers, we can upgrade it at a
 * later date to allow for each channel to be set individually.
 */
		for (j = 0; j < chosen->nchan; j++)
		{
			chosen->typeValue[j] = *((double *)value);
		}

	} else if (strncmp(strtemp, "channel", 7) == 0) {
/* Determine the quantity to define: gain or polarity */
		slen = (unsigned int)strlen(strtemp);
		if (STREQ(strtemp + slen - 5, "_gain"))
		{
			strtemp[slen - 5] = ' ';
			chan = atol(strtemp + 7);
/* Now store the value for the gain if the channel number is value */
			if (chan >= (long)chosen->nchan)
			{
				status = XIA_BAD_VALUE;
				sprintf(info_string,"Channel number invalid for %s.", name);
				xiaLogError("xiaAddDetectorItem", info_string, status);
				return status;
			}

			chosen->gain[chan] = *((double *) value);

		} else if (STREQ(strtemp + slen - 9, "_polarity")) {
/* Now get the channel number, insert space, then use the standard C routine */
			strtemp[slen - 9] = ' ';
			chan = atol(strtemp + 7);
/* Now store the value for the gain if the channel number is value */
			if (chan >= (long)chosen->nchan)
			{
				status = XIA_BAD_VALUE;
				sprintf(info_string,"Channel number invalid for %s.", name);
				xiaLogError("xiaAddDetectorItem", info_string, status);
				return status;
			}

			if ((STREQ((char *) value, "pos")) ||
			    (STREQ((char *) value, "+"))   ||
				(STREQ((char *) value, "positive")))
			{
				chosen->polarity[chan] = 1;

			} else if ((STREQ((char *) value, "neg")) ||
			           (STREQ((char *) value, "-"))   ||
					   (STREQ((char *) value, "negative")))
			{
				chosen->polarity[chan] = 0;

			} else {

				status = XIA_BAD_VALUE;
				sprintf(info_string,"Invalid polarity %s.", (char *) value);
				xiaLogError("xiaAddDetectorItem", info_string, status);
				return status;
			}

		} else {

			status = XIA_BAD_NAME;
			sprintf(info_string,"Invalid name %s.", name);
			xiaLogError("xiaAddDetectorItem", info_string, status);
			return status;
		}

	} else {

		status = XIA_BAD_NAME;
		sprintf(info_string,"Invalid name %s.", name);
		xiaLogError("xiaAddDetectorItem", info_string, status);
		return status;
	}

	return XIA_SUCCESS;
}


/** @brief Modify the detector information
 *
 * Modify a subset of the total detector information. The allowed name-value
 * pairs that can be modified are @a channel{n}_gain, @a channel{n}_polarity and
 * @a type_value. Note: If xiaStartSystem(void) has already been called, 
 * then it must be called again after this routine.
 *
 * @param alias A valid detector alias
 *
 * @param name A valid name from the detector items listed above
 *
 * @param value Value to set the corresponding detector information to, cast
 * into a <em>void *</em>. See the Usage section below for an example.
 *
 * @return An error value indicating success (@a XIA_SUCCESS) or failure
 * (@a XIA_BAD_VALUE, @a XIA_BAD_NAME or @a XIA_NO_ALIAS)
 * 
 * @par Usage:
 * @include xiaModifyDetectorItem.c
 *
 */
HANDEL_EXPORT int HANDEL_API xiaModifyDetectorItem(char *alias, char *name, void
 *value)
{
	int status = XIA_SUCCESS;

	unsigned int i, slen;
	char strtemp[MAXALIAS_LEN];

	
	if (alias == NULL) {
	  status = XIA_NULL_ALIAS;
	  xiaLogError("xiaModifyDetectorItem", "Alias can not be NULL", status);
	  return status;
	}

/* Check that the value is not NULL */
	if (value == NULL)
	{
		status = XIA_BAD_VALUE;
		sprintf(info_string,"Value can not be NULL");
		xiaLogError("xiaModifyDetectorItem", info_string, status);
		return status;
	}

/* convert the name to lower case */
	for (i = 0; i < (unsigned int)strlen(name); i++)
  	{
		strtemp[i] = (char)tolower(name[i]);
	}
	strtemp[strlen(name)] = '\0';

	if (strncmp(strtemp, "channel", 7) == 0)
	{
/* Make sure that the name is either gain or polarity, no others are allowed
 modification. Addendum: type_value is also allowed now. See BUG ID #58. */
		slen = (unsigned int)strlen(strtemp);
		if ((STREQ(strtemp + slen - 5, "_gain")) ||
		    (STREQ(strtemp + slen - 9,"_polarity")))
		{
/* Try to modify the value */
			status = xiaAddDetectorItem(alias, name, value);

			if (status != XIA_SUCCESS)
			{
				sprintf(info_string,"Unable to modify detector value");
				xiaLogError("xiaModifyDetectorItem", info_string, status);
				return status;
			}

			return status;
		}
	}

	if (STREQ(strtemp, "type_value")) {

	  status = xiaAddDetectorItem(alias, name, value);

	  if (status != XIA_SUCCESS) {

		sprintf(info_string, "Unable to modify detector value");
		xiaLogError("xiaModifyDetectorItem", info_string, status);
		return status;
	  }

	  return XIA_SUCCESS;
	}

/* Not a valid name to modify */
	status = XIA_BAD_NAME;
	sprintf(info_string,"Can not modify the name:%s", name);
	xiaLogError("xiaModifyDetectorItem", info_string, status);
	return status;
}


/** @brief Retrieve current information from detector configuration
 *
 * Retrieve current information from detector configuration. All of the names
 * listed in the detector section of @link p_nv_pairs Handel Name-Value Pairs
 * @endlink may be retrieved using this routine.
 *
 * @param alias A valid detector alias
 *
 * @param name A valid name from the detector section of @link p_nv_pairs Handel
 * Name-Value Pairs @endlink
 * 
 * @param value Value to store the retrieved value in cast into a
 * <em>void *</em>. See the Usage section below for an example.
 *
 * @return An error value indicating success (@a XIA_SUCCESS) or failure
 * (@a XIA_NO_ALIAS, @a XIA_BAD_VALUE or @a XIA_BAD_NAME)
 *
 * @par Usage:
 * @include xiaGetDetectorItem.c
 *
 */
HANDEL_EXPORT int HANDEL_API xiaGetDetectorItem(char *alias, char *name, void
 *value)
{
	int status = XIA_SUCCESS;
	int slen;
	int idx;

	unsigned int i;

	char *sidx;

	char strtemp[MAXALIAS_LEN];

	Detector *chosen = NULL;

/* Try and find the alias */
	chosen = xiaFindDetector(alias);
	if (chosen == NULL)
	{
		status = XIA_NO_ALIAS;
		sprintf(info_string,"Alias %s has not been created.", alias);
		xiaLogError("xiaGetDetectorItem", info_string, status);
		return status;
	}

/* Convert name to lowercase */
	for (i = 0; i < (unsigned int)strlen(name); i++)
	{
		strtemp[i] = (char)tolower(name[i]);
	}
	strtemp[strlen(name)] = '\0';

/* Decide which data should be returned */
	if (STREQ(name, "number_of_channels"))
	{

		*((unsigned short *)value) = chosen->nchan;

	} else if (STREQ(name, "type")) {

		switch (chosen->type)
		{
		case XIA_DET_RESET:
			strcpy((char *)value, "reset");
			break;

		case XIA_DET_RCFEED:
			strcpy((char *)value, "rc_feedback");
			break;

		default:
		case XIA_DET_UNKNOWN:
			status = XIA_BAD_VALUE;
			sprintf(info_string, "Detector %s currently is not assigned a valid type",
 chosen->alias);
			xiaLogError("xiaGetDetectorItem", info_string, status);
			return status;
			break;
		}

	} else if (STREQ(name, "type_value")) {

/* Since all values are the same for the typeValue, this is an acceptable
 * thing to do.
 */
		*((double *)value) = chosen->typeValue[0];

	} else if (strncmp(name, "channel", 7) == 0) {
/* Is it a gain or a polarity? */
		slen = (int)strlen(strtemp);
/* The #-may-be-greater-then-one-digit hack */
		sidx = strchr(strtemp, '_');
		sidx[0] = '\0';
		idx = atoi(&strtemp[7]);

/* Sanity Check: This *is* a valid channel, right?? */
		if (idx >= (int)chosen->nchan)
		{
			status = XIA_BAD_VALUE;
			sprintf(info_string, "Channel #: %d is invalid for %s", idx, name);
			xiaLogError("xiaGetDetectorItem", info_string, status);
			return status;
		}

		if (STREQ(sidx + 1, "gain"))
		{
			*((double *)value) = chosen->gain[idx];

		} else if (STREQ(sidx + 1, "polarity")) {

			switch (chosen->polarity[idx]) {
			default:
/* Bad Trouble */
				status = XIA_BAD_VALUE;
				sprintf(info_string, "Internal polarity value inconsistient");
				xiaLogError("xiaGetDetectorItem", info_string, status);
				return status;
				break;
			case 0:
				strcpy((char *)value, "neg");
				break;
			case 1:
				strcpy((char *)value, "pos");
				break;
			}

		} else {

			status = XIA_BAD_NAME;
			sprintf(info_string, "Invalid name: %s", name);
			xiaLogError("xiaGetDetectorItem", info_string, status);
			return status;
		}

	} else {

		status = XIA_BAD_NAME;
		sprintf(info_string, "Invalid name: %s", name);
		xiaLogError("xiaGetDetectorItem", info_string, status);
		return status;
	}

	return status;
}


/**********
 * This routine returns the total # of detectors currently
 * defined in the system. Unfortunately, our design doesn't
 * currently have a LL-manager for the Detector LL, so we
 * have to do this the hard (and slow way). Luckily, we
 * have encapsulated all of this in our routine so that if we 
 * change to a smarter LL-manager no one will be the wiser.
 **********/
HANDEL_EXPORT int HANDEL_API xiaGetNumDetectors(unsigned int *numDetectors)
{
  unsigned int count = 0;

  Detector *current = xiaDetectorHead;


  while (current != NULL) {

	count++;
	current = getListNext(current);
  }

  *numDetectors = count;

  return XIA_SUCCESS;
}


/**********
 * This routine returns a list of the detector aliases
 * that are currently defined in the system. Assumes that the user
 * has already allocated enough memory in "detectors".
 **********/
HANDEL_EXPORT int HANDEL_API xiaGetDetectors(char *detectors[])
{
  int i;

  Detector *current = xiaDetectorHead;

  
  for (i = 0; current != NULL; current = getListNext(current)) {

	strcpy(detectors[i], current->alias);
  }

  return XIA_SUCCESS;
}


/**********
 * This routine is similar to xiaGetDetectors() but only
 * returns a single detector since VB can't pass string
 * arrays to a DLL.
 **********/
HANDEL_EXPORT int HANDEL_API xiaGetDetectors_VB(unsigned int index, char *alias)
{
  int status;

  unsigned int curIdx;

  Detector *current = xiaDetectorHead;

  
  for (curIdx = 0; current != NULL; current = getListNext(current), curIdx++) {

	if (curIdx == index) {

	  strcpy(alias, current->alias);
	  
	  return XIA_SUCCESS;
	}
  }

  status = XIA_BAD_INDEX;
  sprintf(info_string, "Index = %u is out of range for the detectors list", index);
  xiaLogError("xiaGetDetectors_VB", info_string, status);
  return status;
}
	

/** @brief Removes a detector from the system
 *
 * @param alias A valid detector alias
 *
 * @return An error value indicating success (@a XIA_SUCCESS) or failure
 * (@a XIA_NO_ALIAS)
 *
 * @par Usage:
 * @include xiaRemoveDetector.c
 *
 */
HANDEL_EXPORT int HANDEL_API xiaRemoveDetector(char *alias)
{
	int status = XIA_SUCCESS;
	int i;

	char strtemp[MAXALIAS_LEN];

	Detector *prev    = NULL;
	Detector *current = NULL;
	Detector *next    = NULL;

	if (isListEmpty(xiaDetectorHead))
	{
		status = XIA_NO_ALIAS;
		sprintf(info_string, "Alias %s does not exist", alias);
		xiaLogError("xiaRemoveDetector", info_string, status);
		return status;
	}

/* Turn the alias into lower case version, and terminate with a null char */
	for (i = 0; i < (int)strlen(alias); i++)
	{
		strtemp[i] = (char)tolower(alias[i]);
	}
	strtemp[strlen(alias)] = '\0';

/* First check if this alias exists already? */
	prev = NULL;
	current = xiaDetectorHead;
	next = current->next;
	while (!STREQ(strtemp,current->alias))
	{
/* Move to the next element */
		prev = current;
		current = next;
		next = current->next;
	}

/* Check if we found nothing */
	if ((next==NULL) &&
	    (current==NULL))
	{
		status = XIA_NO_ALIAS;
		sprintf(info_string,"Alias %s does not exist.", alias);
		xiaLogError("xiaRemoveDetector", info_string, status);
		return status;
	}

/* Check if match is the head of the list */
	if (current == xiaDetectorHead)
	{
/* Move the next element into the Head position. */
		xiaDetectorHead = next;

	} else {
/* Element is inside the list, change the pointers to skip the matching element
 */
		prev->next = next;
	}
/* Free up the memory associated with this element */
	xiaFreeDetector(current);

	return status;
}

/*****************************************************************************
 *
 * This routine returns the entry of the Detector linked list that matches
 * the alias.  If NULL is returned, then no match was found.
 *
 *****************************************************************************/
HANDEL_SHARED Detector* HANDEL_API xiaFindDetector(char *alias)
{
	int i;

	char strtemp[MAXALIAS_LEN];

	Detector *current = NULL;

/* Turn the alias into lower case version, and terminate with a null char */
	for (i = 0; i < (int)strlen(alias); i++)
	{
		strtemp[i] = (char)tolower(alias[i]);
	}
	strtemp[strlen(alias)] = '\0';

/* First check if this alias exists already? */
	current = xiaDetectorHead;
	while (current != NULL)
	{
		if (STREQ(strtemp,current->alias))
		{
			return current;
		}

		current = current->next;
	}

	return NULL;
}


/*****************************************************************************
 *
 * This routine returns the head of the Detector LL
 *
 *****************************************************************************/
HANDEL_SHARED Detector* HANDEL_API xiaGetDetectorHead(void)
{
	return xiaDetectorHead;
}
