/*
 * handel_dbg.c
 *
 * Created 10/09/01 -- PJF
 *
 * Provides some nice debugging routines which
 * I hope will "disappear" in the production
 * version. They only have to disappear since
 * they are exported to the DLL!
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


#ifdef _DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xia_handel.h"
#include "xia_handel_structures.h"

#include "xerxes.h"
#include "xerxes_generic.h"


/*****************************************************************************
 *
 * This routine will walk through the global DetChan array and dump it to 
 * the file (in ASCII format) specified by fileName.
 *
 *****************************************************************************/
HANDEL_EXPORT void HANDEL_API xiaDumpDetChanStruct(char *fileName)
{
  DetChanElement *current = NULL;

  DetChanSetElem *curElem = NULL;

  FILE *log;

  int i;

  log = fopen(fileName, "w");
  if (log == NULL)
	{
	  return;
	}

  fprintf(log, "****Dump of Global DetChanElement LL****\n\n");

  current = xiaDetChanHead;

  for (i = 0; current != NULL; current = current->next, i++)
	{
	  fprintf(log, "Element at position %d, Address = %p\n", i, current);
	  fprintf(log, "detChan = %d\n", current->detChan);
		
	  switch (current->type) 
		{
		case SINGLE:
		  fprintf(log, "type = SINGLE\n");
		  fprintf(log, "modAlias = %s\n", current->data.modAlias);
		  break;
			
		case SET:
		  fprintf(log, "type = SET\n");
		  curElem = current->data.detChanSet;

		  while (curElem != NULL)
			{
			  fprintf(log, "%d   ", curElem->channel);
			  curElem = curElem->next;
			}

		  fprintf(log, "\n");

		  break;

		default:
		  fprintf(log, "BAD ELEMENT\n");
		  break;
		}

	  fprintf(log, "isTagged = %s\n", current->isTagged == TRUE_ ? "TRUE" : "FALSE");
	  fprintf(log, "next = %p\n\n", current->next);
	}

  fclose(log);

}


/*****************************************************************************
 *
 * This routine dumps the DSP Parameters for a given channel.
 *
 *****************************************************************************/
HANDEL_EXPORT void HANDEL_API xiaDumpDSPParameters(int detChan, char *fileName)
{
  int statusX;
  int i;

  unsigned short nSymbols;
  unsigned short value;

  char **paramNames;

  FILE *paramFile = NULL;

  paramFile = fopen(fileName, "w");
  if (paramFile == NULL)
	{
	  return;
	}

  statusX = dxp_max_symbols(&detChan, &nSymbols);

  paramNames = (char **)malloc(sizeof(char *) * nSymbols);
  for (i = 0; i < nSymbols; i++)
	{
	  paramNames[i] = (char *)malloc(sizeof(char) * MAX_DSP_PARAM_NAME_LEN);
	}

  statusX = dxp_symbolname_list(&detChan, paramNames);

  for (i = 0; i < nSymbols; i++)
	{
	  statusX = dxp_get_one_dspsymbol(&detChan, paramNames[i], &value);
 
	  fprintf(paramFile, "%s: %u\n", paramNames[i], value);
	}

  fclose(paramFile);
  for (i = 0; i < nSymbols; i++)
	{
	  free(paramNames[i]);
	}
  free(paramNames);
}


HANDEL_EXPORT void HANDEL_API xiaDumpFirmwareStruct(char *fileName)
{
  unsigned int i;

  FILE *file = NULL;

  FirmwareSet *firmwareSet = xiaGetFirmwareSetHead();
  Firmware *firmware = NULL;


  file = fopen(fileName, "w");
  if (file == NULL) { return; }

  fprintf(file, "***Dump of FirmwareSet LL***\n");

  while (firmwareSet != NULL)
	{
	  fprintf(file, "\nAddress = %p\n", firmwareSet);
	  fprintf(file, "alias = %s\n", firmwareSet->alias);
	  fprintf(file, "filename = %s\n", firmwareSet->filename == NULL ? "NULL" : firmwareSet->filename);
	  fprintf(file, "numKeywords = %u\n", firmwareSet->numKeywords);
		
	  for (i = 0; i < firmwareSet->numKeywords; i++)
		{
		  fprintf(file, "keywords[%u] = %s\n", i, firmwareSet->keywords[i]);
		}

	  fprintf(file, "mmu = %s\n", firmwareSet->mmu == NULL ? "NULL" : firmwareSet->mmu);
		
	  firmware = firmwareSet->firmware;
	  while (firmware != NULL)
		{
		  fprintf(file, "\tptrr = %u\n", firmware->ptrr);
		  fprintf(file, "\tmin_peaking_time = %.3f\n", firmware->min_ptime);
		  fprintf(file, "\tmax_peaking_time = %.3f\n", firmware->max_ptime);
		  fprintf(file, "\tfippi = %s\n", firmware->fippi == NULL ? "NULL" : firmware->fippi);
		  fprintf(file, "\tuser_fippi = %s\n", firmware->user_fippi == NULL ? "NULL" : firmware->user_fippi);
		  fprintf(file, "\tdsp = %s\n", firmware->dsp == NULL ? "NULL" : firmware->dsp);
		  fprintf(file, "\tnumFilter = %u\n", firmware->numFilter);

		  for (i = 0; i < firmware->numFilter; i++)
			{
			  fprintf(file, "\tfilterInfo[%u] = %u\n", i, firmware->filterInfo[i]);
			}

		  firmware = firmware->next;
		}

	  firmwareSet = firmwareSet->next;
	}

  fclose(file);
}

 
HANDEL_EXPORT void HANDEL_API xiaDumpModuleStruct(char *fileName)
{
  unsigned int i;

  FILE *file = NULL;

  Module *module = xiaGetModuleHead();


  file = fopen(fileName, "w");
  if (file == NULL) { return; }

  fprintf(file, "***Dump of Module LL***\n\n");

  while (module != NULL) {

	fprintf(file, "Address = %p\n", module);
	fprintf(file, "alias = %s\n", module->alias);
	fprintf(file, "type = %s\n", module->type);
	fprintf(file, "interface_type = %u\n", module->interface_info->type);
	fprintf(file, "number_of_channels = %u\n", module->number_of_channels);
	
	for (i = 0; i < module->number_of_channels; i++) {

	  fprintf(file, "channels[%u] = %d\n", i, module->channels[i]);
	}

	for (i = 0; i < module->number_of_channels; i++) {

	  fprintf(file, "detector[%u] = %s\n", i, module->detector[i]);
	}

	for (i = 0; i < module->number_of_channels; i++) {

	  fprintf(file, "detector_chan[%u] = %d\n", i, module->detector_chan[i]);
	}

	for (i = 0; i < module->number_of_channels; i++) {

	  fprintf(file, "gain[%u] = %.3f\n", i, module->gain[i]);
	}

	for (i = 0; i < module->number_of_channels; i++) {

	  fprintf(file, "firmware[%u] = %s\n", i, module->firmware[i]);
	}

	for (i = 0; i < module->number_of_channels; i++) {

	  fprintf(file, "defaults[%u] = %s\n", i, module->defaults[i]);
	}

	for (i = 0; i < module->number_of_channels; i++) {
	  
	  fprintf(file, "currentFirmware[%u] (addr) = %p\n", i, &(module->currentFirmware[i]));
	}

	fprintf(file, "\n");

	module = module->next;
  }
	  
  fclose(file);
}
	
	
HANDEL_EXPORT void HANDEL_API xiaDumpDefaultsStruct(char *fileName)
{
  XiaDefaults *defaults = xiaGetDefaultsHead();
  XiaDaqEntry *entry    = NULL;

  FILE *file = NULL;


  file = fopen(fileName, "w");
  if (file == NULL) { return; }

  fprintf(file, "*** Dump of Defaults LL ***\n\n");

  while (defaults != NULL) {

	fprintf(file, "Address = %p\n", defaults);
	fprintf(file, "alias = %s\n", defaults->alias);

	entry = defaults->entry;

	while (entry != NULL) {

	  fprintf(file, "\t%s = %.3f\n", entry->name, entry->data);
	
	  entry = entry->next;
	}

	fprintf(file, "\n");

	defaults = defaults->next;
  }

}
		

#endif /* _DEBUG */		


