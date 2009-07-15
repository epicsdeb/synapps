/*
 * handel_file.c
 *
 * This file contains routines used to restore and
 * save various file formats in HanDeL.
 *
 * Created 12/12/01 -- PJF
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
#include <ctype.h>

#include "handel_generic.h"
#include "xia_handel.h"
#include "handel_errors.h"
#include "handel_file.h"

#include "xia_module.h"
#include "xia_common.h"
#include "xia_assert.h"
#include "xia_file.h"


/* For the "xerxes_sav" type.
 * This is not a documented save
 * format and should not be used
 * by anyone but XIA.
 */
#include "xerxes.h"
#include "xerxes_errors.h"


typedef int (*interfaceWrite_FP)(FILE *, Module *);

typedef struct _InterfaceWriter {

  unsigned int type;
  interfaceWrite_FP fn;

} InterfaceWriters_t;


static int writePLX(FILE *fp, Module *module);
static int writeEPP(FILE *fp, Module *module);
static int writeJ73A(FILE *fp, Module *module);
static int writeUSB(FILE *fp, Module *module);
static int writeUSB2(FILE *fp, Module *m);
static int writeInterface(FILE *fp, Module *m);

static FILE* dxp_find_file(const char *, const char *, char [MAXFILENAME_LEN]);

static char line[132];


/** GLOBAL Variables **/
static InterfaceWriters_t INTERFACE_WRITERS[] = {
  /* Sentinel */
  {0, NULL},
#ifndef EXCLUDE_PLX
  {PLX,          writePLX},
#endif /* EXCLUDE_PLX */
#ifndef EXCLUDE_EPP
  {EPP,          writeEPP},
  {GENERIC_EPP,  writeEPP},
#endif /* EXCLUDE_PLX */
#ifndef EXCLUDE_CAMAC
  {JORWAY73A,    writeJ73A},
  {GENERIC_SCSI, writeJ73A},
#endif /* EXCLUDE_CAMAC */
#ifndef EXCLUDE_USB
  {USB,          writeUSB},
#endif /* EXCLUDE_USB */
#ifndef EXCLUDE_USB2
  {USB2,         writeUSB2},
#endif /* EXCLUDE_USB2 */
};


SectionInfo sectionInfo[] = 
  {
    {xiaLoadDetector, "detector definitions"},
    {xiaLoadFirmware, "firmware definitions"},
    {xiaLoadDefaults, "default definitions"},
    {xiaLoadModule,   "module definitions"}
  };


/** @brief Loads in a save file of type @a type.
 *
 * When Handel loads a system it first must clear out the existing configuration
 * in order to allow the other configuration calls to succeed. If you load a 
 * file that is malformed, you will also lose the existing configuration.
 */
HANDEL_EXPORT int HANDEL_API xiaLoadSystem(char *type, char *filename)
{
  int status;


	if (type == NULL) {
	  xiaLogError("xiaLoadSystem", ".INI file 'type' string is NULL",
                XIA_NULL_TYPE);
	  return XIA_NULL_TYPE;
	}
  
	if (filename == NULL) {
	  xiaLogError("xiaLoadSystem", ".INI file 'name' string is NULL",
                XIA_NO_FILENAME);
	  return XIA_NO_FILENAME;
	}

	/* If we support different output types in the future, we need to change
	 * this logic around.
	 */
	if (!STREQ(type, "handel_ini")) {
	  sprintf(info_string, "Unknown file type '%s' for target save file '%s'",
            type, filename);
	  xiaLogError("xiaLoadSystem", info_string, XIA_FILE_TYPE);
	  return XIA_FILE_TYPE;
	}

	/* We need to clear and re-initialize Handel */
	status = xiaInitHandel();

	if (status != XIA_SUCCESS) {
	  xiaLogError("xiaLoadSystem", "Error reinitializing Handel", status);
	  return status;
	}

	status = xiaReadIniFile(filename);

	if (status != XIA_SUCCESS) {
	  sprintf(info_string, "Error reading in .INI file '%s'", filename);
	  xiaLogError("xiaLoadSystem", info_string, status);
	  return status;
	}

  return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine saves the configuration to the file filename and of type type.
 * Currently, the only supported types are "handel_ini".
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaSaveSystem(char *type, char *filename)
{
  int status;


  if (STREQ(type, "handel_ini")) {

    status = xiaWriteIniFile(filename);

  } else if (STREQ(type, "xerxes_sav")) {

    status = xiaWriteSavFile(filename);

  } else {
		
    status = XIA_FILE_TYPE;
  }

  if (status != XIA_SUCCESS)
    {
      sprintf(info_string, "Error writing %s", filename);
      xiaLogError("xiaSaveSystem", info_string, status);
      return status;
    }

  return XIA_SUCCESS;
}


/**********
 * This routine writes out a Xerxes .sav file
 * vias the appropriate Xerxes routine. This 
 * type is not supported or documented for general
 * use.
 **********/
HANDEL_STATIC int HANDEL_API xiaWriteSavFile(char *filename)
{
  int statusX;
  int status;
  int lun;
  int mode = 0;

  
  statusX = dxp_open_file(&lun, filename, &mode);

  if (statusX != DXP_SUCCESS) {

    status = XIA_XERXES;
    sprintf(info_string, "Error opening file: %s", filename);
    xiaLogError("xiaWriteSavFile", info_string, status);
    return status;
  }

  statusX = dxp_save_config(&lun);

  if (statusX != DXP_SUCCESS) {

    status = XIA_XERXES;
    sprintf(info_string, "Error saving config to %s", filename);
    xiaLogError("xiaWriteSavFile", info_string, status);
    return status;
  }

  statusX = dxp_close_file(&lun);

  if (statusX != DXP_SUCCESS) {

    status = XIA_XERXES;
    sprintf(info_string, "Error closing %s", filename);
    xiaLogError("xiaWriteSavFile", info_string, status);
    return status;
  }

  return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine writes out a "handel_ini" file based on the current 
 * information in the data structures.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaWriteIniFile(char *filename)
{
  int status;	
  int i;
  
  unsigned int j;
  
  FILE *iniFile = NULL;
  
  char typeStr[MAXITEM_LEN];
  
  Detector    *detector    = xiaGetDetectorHead();
  FirmwareSet *firmwareSet = xiaGetFirmwareSetHead();
  Firmware    *firmware 	 = NULL;
  XiaDefaults *defaults    = xiaGetDefaultsHead();
  XiaDaqEntry *entry       = NULL;
  Module      *module      = xiaGetModuleHead();
  
  
  if ((filename == NULL) || (STREQ(filename, "")))
    {
      status = XIA_NO_FILENAME;
      sprintf(info_string, "Filename is either NULL or empty, illegal value");
      xiaLogError("xiaWriteIniFile", info_string, status);
      return status;
    }
  
  iniFile = xia_file_open(filename, "w");
  if (iniFile == NULL)
    {
      status = XIA_OPEN_FILE;
      sprintf(info_string, "Could not open %s", filename);
      xiaLogError("xiaWriteIniFile", info_string, status);
      return status;
    }
  
  /* Write the sections in the same order that they are read in */
  fprintf(iniFile, "[detector definitions]\n\n");
  
  for (i = 0; detector != NULL; detector = detector->next, i++)
    {
      fprintf(iniFile, "START #%d\n", i);
      fprintf(iniFile, "alias = %s\n", detector->alias);
      fprintf(iniFile, "number_of_channels = %u\n", detector->nchan);
      
      switch (detector->type)
        {
        case XIA_DET_RESET:
          strcpy(typeStr, "reset");
          break;
			
        case XIA_DET_RCFEED:
          strcpy(typeStr, "rc_feedback");
          break;

        default:
        case XIA_DET_UNKNOWN:
          xia_file_close(iniFile);
          status = XIA_MISSING_TYPE;
          sprintf(info_string, "Unknown detector type for alias %s", detector->alias);
          xiaLogError("xiaWriteIniFile", info_string, status);
          return status;
          break;
        }

      fprintf(iniFile, "type = %s\n", typeStr);
      fprintf(iniFile, "type_value = %3.3f\n", detector->typeValue[0]);
		
      for (j = 0; j < detector->nchan; j++)
        {
          fprintf(iniFile, "channel%u_gain = %3.3f\n", j, detector->gain[j]);
			
          switch (detector->polarity[j])
            {
            case 0: /* Negative */
              fprintf(iniFile, "channel%u_polarity = %s\n", j, "-");
              break;

            case 1: /* Positive */
              fprintf(iniFile, "channel%u_polarity = %s\n", j, "+");
              break;

            default:
              xia_file_close(iniFile);
              status = XIA_UNKNOWN;
              xiaLogError("xiaWriteIniFile", "Impossible polarity error", status);
              return status;
              break;
            }
        }

      fprintf(iniFile, "END #%d\n\n", i);
    }

  fprintf(iniFile, "[firmware definitions]\n\n");

  for (i = 0; firmwareSet != NULL; firmwareSet = firmwareSet->next, i++)
    {
      fprintf(iniFile, "START #%d\n", i);
      fprintf(iniFile, "alias = %s\n", firmwareSet->alias);
		
      if (firmwareSet->mmu != NULL)
        {
          fprintf(iniFile, "mmu = %s\n", firmwareSet->mmu);
        }

      if (firmwareSet->filename != NULL)
        {
          fprintf(iniFile, "filename = %s\n", firmwareSet->filename);
      
          if (firmwareSet->tmpPath != NULL) {
            fprintf(iniFile, "fdd_tmp_path = %s\n", firmwareSet->tmpPath);
          }

          fprintf(iniFile, "num_keywords = %u\n", firmwareSet->numKeywords);
			
          for (j = 0; j < firmwareSet->numKeywords; j++)
            {
              fprintf(iniFile, "keyword%u = %s\n", j, firmwareSet->keywords[j]);
            }
		
        } else {

        firmware = firmwareSet->firmware;
        while (firmware != NULL)
          {
            fprintf(iniFile, "ptrr = %u\n", firmware->ptrr);
            fprintf(iniFile, "min_peaking_time = %3.3f\n", firmware->min_ptime);
            fprintf(iniFile, "max_peaking_time = %3.3f\n", firmware->max_ptime);
				
            if (firmware->fippi != NULL)
              {
                fprintf(iniFile, "fippi = %s\n", firmware->fippi);
              } 
				
            if (firmware->user_fippi != NULL) 
              {
                fprintf(iniFile, "user_fippi = %s\n", firmware->user_fippi);
              } 
				
            if (firmware->dsp != NULL) 
              {
                fprintf(iniFile, "dsp = %s\n", firmware->dsp);
              }

            fprintf(iniFile, "num_filter = %u\n", firmware->numFilter);

            for (j = 0; j < firmware->numFilter; j++)
              {
                fprintf(iniFile, "filter_info%u = %u\n", j, firmware->filterInfo[j]);
              }

            firmware = firmware->next;
          }
      }

      fprintf(iniFile, "END #%d\n\n", i);
    }

  fprintf(iniFile, "***** Generated by Handel -- DO NOT MODIFY *****\n");

  fprintf(iniFile, "[default definitions]\n\n");
	
  for (i = 0; defaults != NULL; defaults = defaults->next, i++)
    {
      fprintf(iniFile, "START #%d\n", i);
      fprintf(iniFile, "alias = %s\n", defaults->alias);

      entry = defaults->entry;
      while (entry != NULL)
        {
          fprintf(iniFile, "%s = %3.3f\n", entry->name, entry->data);
          entry = entry->next;
        }

      fprintf(iniFile, "END #%d\n\n", i);
    }

  fprintf(iniFile, "***** End of Generated Information *****\n\n");

  fprintf(iniFile, "[module definitions]\n\n");

  for (i = 0; module != NULL; module = module->next, i++)
    {
      fprintf(iniFile, "START #%d\n", i);
      fprintf(iniFile, "alias = %s\n", module->alias);
      fprintf(iniFile, "module_type = %s\n", module->type);

      status = writeInterface(iniFile, module);

      if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error writing interface information for module "
                "'%s'", module->alias);
        xiaLogError("xiaWriteIniFile", info_string, status);
        return status;
      }

      /* 	switch (module->interface_info->type) */
      /* 	{ */
      /* #ifndef EXCLUDE_CAMAC */
      /* 	  case JORWAY73A: */
      /* 	  case GENERIC_SCSI: */
      /* 	    fprintf(iniFile, "interface = j73a\n"); */
      /* 	    fprintf(iniFile, "scsi_bus = %u\n", module->interface_info->info.jorway73a->scsi_bus); */
      /* 	    fprintf(iniFile, "crate_number = %u\n", module->interface_info->info.jorway73a->crate_number); */
      /* 	    fprintf(iniFile, "slot = %u\n", module->interface_info->info.jorway73a->slot); */
      /* 	    break; */
      /* #endif /\* EXCLUDE_CAMAC *\/ */

      /* #ifndef EXCLUDE_EPP */
      /* 	  case EPP: */
      /* 	  case GENERIC_EPP: */
      /* 	    fprintf(iniFile, "interface = epp\n"); */
      /* 	    fprintf(iniFile, "epp_address = %#x\n", module->interface_info->info.epp->epp_address); */
      /* 		/\* Bug #79, the old format for daisy_chain_id was %#x *\/ */
      /* 	    fprintf(iniFile, "daisy_chain_id = %u\n", module->interface_info->info.epp->daisy_chain_id); */
      /* 	    break; */
      /* #endif /\* EXCLUDE_EPP *\/ */

      /* #ifndef EXCLUDE_USB */
      /* 	  case USB: */
      /* 	    fprintf(iniFile, "interface = usb\n"); */
      /* 	    fprintf(iniFile, "device_number = %i\n", module->interface_info->info.usb->device_number); */
      /* 	    break; */
      /* #endif /\* EXCLUDE_USB *\/ */

      /* #ifndef EXCLUDE_SERIAL */
      /* 	  case SERIAL: */
      /* 	    fprintf(iniFile, "interface = serial\n"); */
      /* 	    fprintf(iniFile, "com_port = %u\n", module->interface_info->info.serial->com_port); */
      /* 	    fprintf(iniFile, "baud_rate = %u\n", module->interface_info->info.serial->baud_rate); */
      /* 	    break; */
      /* #endif /\* EXCLUDE_SERIAL *\/ */

      /* 	  default: */
      /* 	    xia_file_close(iniFile); */
      /* 	    status = XIA_UNKNOWN; */
      /* 	    xiaLogError("xiaWriteIniFile", "Impossible interface error", status); */
      /* 	    return status; */
      /* 	    break; */
      /* 	} */

      fprintf(iniFile, "number_of_channels = %u\n", module->number_of_channels);
		
      for (j = 0; j < module->number_of_channels; j++)
        {
          fprintf(iniFile, "channel%u_alias = %d\n", j, module->channels[j]);
          fprintf(iniFile, "channel%u_detector = %s:%u\n", j, module->detector[j], (unsigned int)module->detector_chan[j]);
          fprintf(iniFile, "channel%u_gain = %3.3f\n", j, module->gain[j]);
          fprintf(iniFile, "firmware_set_chan%u = %s\n", j, module->firmware[j]);
          fprintf(iniFile, "default_chan%u = %s\n", j, module->defaults[j]);
        }

      fprintf(iniFile, "END #%d\n\n", i);
    }

  xia_file_close(iniFile);

  return XIA_SUCCESS;
}

		
/*****************************************************************************
 *
 * Routine to read in "handel_ini" type ini files
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaReadIniFile(char *inifile)
{
  int status = XIA_SUCCESS;
  int numSections;
  int i;
  /* 
   * Pointers to keep track of the xia.ini file
   */
  FILE *fp = NULL;
	
  char newFile[MAXFILENAME_LEN];
  char tmpLine[132];

  fpos_t start;
  fpos_t end;
  fpos_t local;
  fpos_t local_end;
	
  char xiaini[8] = "xia.ini";

  /* Check if an .INI file was specified */
  if (inifile == NULL) 
    {
      inifile = xiaini;
    }

  /* Open the .ini file */
  fp = dxp_find_file(inifile, "r", newFile);
    
	if (fp == NULL) {
	  status = XIA_OPEN_FILE;
	  sprintf(info_string,"Could not open %s", inifile);
	  xiaLogError("xiaReadIniFile", info_string, status);
	  return status;
  }
	
  /* Loop over all the sections as defined in sectionInfo */
	/* XXX BUG: Should be sectionInfo / sectionInfo[0]?
	 */
  numSections = (int) (sizeof(sectionInfo) / sizeof(SectionInfo));

  for (i = 0; i < numSections; i++)
    {
      status = xiaFindEntryLimits(fp, sectionInfo[i].section, &start, &end);

      if (status != XIA_SUCCESS)
        {
          sprintf(info_string, "Section missing from ini file: %s", sectionInfo[i].section);
          xiaLogWarning("xiaReadIniFile", info_string);
          continue;
        }

      /* Here is the psuedocode for parsing in a section w/ multiple headings
       *
       * 1) Set local to line with START on it (this is a one shot thing)
       * 2) Cache line pointed to by "end" (this is because we can't do direct
       *    arithmetic comparisons with fpos_t structs). Also, actually do a
       *    comparison between local's "line" and the end's "line". If they
       *    match then we've reached the end of the section and are finished.
       *    N.b.: I pray to god that a situation never somes up that would
       *    break this comparison. In principle it shouldn't happen since
       *    end's "line" is either EOF or a section heading.
       * 3) Increment local until we run into END
       * 4) Set local_end
       * 5) xiaLoadxxxxx(local, local_end);
       * 6) Set current to local_end and jump to step (2)
       */
      status = fsetpos(fp, &end);
      status = xiaGetLine(fp, tmpLine);

      sprintf(info_string, "Cached end string = %s", tmpLine);
      xiaLogDebug("xiaReadIniFile", info_string);

      status = fsetpos(fp, &start);
      status = xiaGetLine(fp, line);

      while(!STREQ(line, tmpLine))
        {
          status = fgetpos(fp, &local);

          if (strncmp(line, "START", 5) == 0)
            {
              do
                {
                  status = fgetpos(fp, &local_end);

                  status = xiaGetLine(fp, line);

                  sprintf(info_string, "Inside START/END bracket: %s", line);
                  xiaLogDebug("xiaReadIniFile", info_string);

                } while (strncmp(line, "END", 3) != 0);

              status = sectionInfo[i].function_ptr(fp, &local, &local_end);

              if (status != XIA_SUCCESS) {
                xia_file_close(fp);
                xiaLogError("xiaReadIniFile", "Error loading information from ini file", status);
                return status;
              }
            }

          status = xiaGetLine(fp, line);

          if (status == XIA_EOF)
            {
              break;
            }

          sprintf(info_string, "Looking for START: %s", line);
          xiaLogDebug("xiaReadIniFile", info_string);
        }
    }

	xia_file_close(fp);

  return XIA_SUCCESS;
}



/******************************************************************************
 *
 * Routine to open a new file.  
 * Try to open the file directly first.
 * Then try to open the file in the directory pointed to 
 *     by XIAHOME.
 * Finally try to open the file as an environment variable.
 *
 ******************************************************************************/
static FILE* dxp_find_file(const char* filename, const char* mode,
                           char newFile[MAXFILENAME_LEN])
{
  FILE *fp=NULL;
  char *name=NULL, *name2=NULL;
  char *home=NULL;
	
  unsigned int len = 0;

	

	ASSERT(filename != NULL);

  /* Try to open file directly */
  if((fp=xia_file_open(filename,mode))!=NULL){
    len = MAXFILENAME_LEN>(strlen(filename)+1) ? strlen(filename) : MAXFILENAME_LEN;
    strncpy(newFile, filename, len);
    newFile[len] = '\0';
    return fp;
  }
  /* Try to open the file with the path XIAHOME */
  if ((home=getenv("XIAHOME"))!=NULL) {
    name = (char *) handel_md_alloc(sizeof(char)*
                                    (strlen(home)+strlen(filename)+2));
    sprintf(name, "%s/%s", home, filename);
    if((fp=xia_file_open(name,mode))!=NULL){
	    len = MAXFILENAME_LEN>(strlen(name)+1) ? strlen(name) : MAXFILENAME_LEN;
	    strncpy(newFile, name, len);
	    newFile[len] = '\0';
	    handel_md_free(name);
	    return fp;
    }
    handel_md_free(name);
    name = NULL;
  }
  /* Try to open the file with the path DXPHOME */
  if ((home=getenv("DXPHOME"))!=NULL) {
    name = (char *) handel_md_alloc(sizeof(char)*
                                    (strlen(home)+strlen(filename)+2));
    sprintf(name, "%s/%s", home, filename);
    if((fp=xia_file_open(name,mode))!=NULL){
	    len = MAXFILENAME_LEN>(strlen(name)+1) ? strlen(name) : MAXFILENAME_LEN;
	    strncpy(newFile, name, len);
	    newFile[len] = '\0';
	    handel_md_free(name);
	    return fp;
    }
    handel_md_free(name);
    name = NULL;
  }
  /* Try to open the file as an environment variable */
  if ((name=getenv(filename))!=NULL) {
    if((fp=xia_file_open(name,mode))!=NULL){
	    len = MAXFILENAME_LEN>(strlen(name)+1) ? strlen(name) : MAXFILENAME_LEN;
	    strncpy(newFile, name, len);
	    newFile[len] = '\0';
	    return fp;
    }
    name = NULL;
  }
  /* Try to open the file with the path XIAHOME and pointing 
   * to a file as an environment variable */
  if ((home=getenv("XIAHOME"))!=NULL) {
    if ((name2=getenv(filename))!=NULL) {
		
	    name = (char *) handel_md_alloc(sizeof(char)*
                                      (strlen(home)+strlen(name2)+2));
	    sprintf(name, "%s/%s", home, name2);
	    if((fp=xia_file_open(name,mode))!=NULL) {
        len = MAXFILENAME_LEN>(strlen(name)+1) ? strlen(name) : MAXFILENAME_LEN;
        strncpy(newFile, name, len);
        newFile[len] = '\0';
        handel_md_free(name);
        return fp;
	    }
	    handel_md_free(name);
	    name = NULL;
    }
  }
  /* Try to open the file with the path DXPHOME and pointing 
   * to a file as an environment variable */
  if ((home=getenv("DXPHOME"))!=NULL) {
    if ((name2=getenv(filename))!=NULL) {
		
	    name = (char *) handel_md_alloc(sizeof(char)*
                                      (strlen(home)+strlen(name2)+2));
	    sprintf(name, "%s/%s", home, name2);
	    if((fp=xia_file_open(name,mode))!=NULL) {
        len = MAXFILENAME_LEN>(strlen(name)+1) ? strlen(name) : MAXFILENAME_LEN;
        strncpy(newFile, name, len);
        newFile[len] = '\0';
        handel_md_free(name);
        return fp;
	    }
	    handel_md_free(name);
	    name = NULL;
    }
  }

  return NULL;
}



/******************************************************************************
 *
 * Routine that gets the first line with text after the current file 
 * position.
 *
 ******************************************************************************/
HANDEL_STATIC int HANDEL_API xiaGetLineData(char *lline, char *name, char *value)
{
  int status = XIA_SUCCESS;

  size_t loc;
  size_t begin;
  size_t end;
  size_t len;


  /* If this line is a comment then skip it.
   * See BUG ID #64.
   */
  if (lline[0] == '*') {
    strcpy(name, "COMMENT");
    strcpy(value, lline);
    return XIA_SUCCESS;
  }

  /* Start by finding the '=' within the line */
  loc = strcspn(lline, "=");
  if (loc == 0) 
    {
      status = XIA_FORMAT_ERROR;
      sprintf(info_string, "No = present in xia.ini line: \n %s", lline);
      xiaLogError("xiaGetLineData", info_string, status);
      return status;
    }

  /* Strip all the leading blanks */
  begin = 0;
  while (isspace(lline[begin])) 
    {
      begin++;
    }

  /* Strip all the leading blanks */
  end = loc - 1;
  while (isspace(lline[end])) 
    {
      end--;
    }

	/* Bug #76, prevents a bad core dump */
  if (begin > end) 
	  {
      status = XIA_FORMAT_ERROR;
      sprintf(info_string, "Invalid name found in line:  %s", lline);
      xiaLogError("xiaGetLineData", info_string, status);
      return status;
	  }

	/* copy the name */
  len = end - begin + 1;
  strncpy(name, lline + begin, len);
  name[len] = '\0';
	
  /* Strip all the leading blanks */
  begin = loc + 1;
  while (isspace(lline[begin])) 
    {
      begin++;
    }

  /* Strip all the leading blanks */
  end = strlen(lline) - 1;
  while (isspace(lline[end])) 
    {
      end--;
    }

	/* Bug #76, prevents a bad core dump */
  if (begin > end) 
	  {
      status = XIA_FORMAT_ERROR;
      sprintf(info_string, "Invalid value found in line:  %s", lline);
      xiaLogError("xiaGetLineData", info_string, status);
      return status;
	  }

  /* copy the value */
  len = end - begin + 1;
  strncpy(value, lline + begin, len);
  value[len] = '\0';

  /* Convert name to lower case */
  /*
    for (j = 0; j < (unsigned int)strlen(name); j++)
    {
    name[j] = (char) tolower(name[j]);
    }
	
  */

  return status;
}

/******************************************************************************
 *
 * Routine that gets the first line with text after the current file 
 * position.
 *
 ******************************************************************************/
HANDEL_STATIC int HANDEL_API xiaGetLine(FILE *fp, char *lline)
{
  int status = XIA_SUCCESS;
  unsigned int j;

  char *cstatus;

  /* Now fine the match to the section entry */
  do 
    {
      cstatus = handel_md_fgets(lline, 132, fp); 
      /* Check for any alphanumeric character in the line */
      for (j = 0; j < (unsigned int)strlen(lline); j++) 
        { 
          if (isgraph(lline[j]))
            {
              return status;
            }
        }
    } while (cstatus != NULL);

  return XIA_EOF;
}

/******************************************************************************
 *
 * Routine that searches thru the .ini file and finds the start and end of a 
 * specific section of the .ini file starting at [section] and ending at the 
 * next [].
 *
 ******************************************************************************/
HANDEL_STATIC int HANDEL_API xiaFindEntryLimits(FILE *fp, const char *section, fpos_t *start, fpos_t *end)
{
  int status = XIA_SUCCESS;
  unsigned int j;

  boolean_t isStartFound = FALSE_;

  char *cstatus;

  /* Zero the start and end variables */
  /*	*start=*end=0; */

  /* First rewind the file to the beginning */
  rewind(fp);

  /* Now fine the match to the section entry */
  do {
    do {
	    cstatus = handel_md_fgets(line, 132, fp);
	    /* 
	       sprintf(info_string, "Line read: %s", line);
	       xiaLogDebug("xiaFindEntryLimits", info_string);
	    */

    } while ((line[0]!='[') &&
             (cstatus!=NULL));
 
    if (cstatus == NULL) 
      {
        status = XIA_NOSECTION;
        sprintf(info_string,"Unable to find section %s",section);
        /* This isn't an error since the user has the option of specifying the missing
         * information using the dynamic configuration routines.
         */
        xiaLogWarning("xiaFindEntryLimits", info_string);
        return status;
      }
    /* Find the terminating ] to this section */
    for (j = 1; j < (strlen(line) + 1); j++) 
      { 
        if (line[j]==']')
          {
            break;
          }
      }
    /* Did we not find the terminating ]? */
    if (j == (unsigned int)strlen(line)) 
      {
        status = XIA_FORMAT_ERROR;
        sprintf(info_string,"Syntax error in Init file, no terminating ] found");
        xiaLogError("xiaFindEntryLimits", info_string, status);
        return status;
      }

    if (strncmp(line + 1, section, j - 1) == 0) 
      {
        /* Recorcd the starting position) */
        fgetpos(fp, start);
			
        isStartFound = TRUE_;
      }
    /* Else look for the next section entry */
  } while (!isStartFound);

  do 
    {
      /* Get the current file position before the next read.  If the file
       * ends or we find a '[' then we are done and want to sent the ending 
       * position to the location of the previous read
       */
      fgetpos(fp, end);
 
      cstatus = handel_md_fgets(line, 132, fp);
      /*
        sprintf(info_string, "cstatus == %s", (cstatus == NULL) ? "NULL" : cstatus);
        xiaLogDebug("xiaFindEntryStart", info_string);
      */

    } while ((line[0] != '[') && 
             (cstatus != NULL));

  /* No real way to detect error at this point, either the file will end 
   * or a '[' will be found.
   */
  return status;
}


/*****************************************************************************
 *
 * This routine parses data in from fp (and bounded by start & end) as 
 * detector information. If it fails, then it fails hard and the user needs 
 * to fix their inifile.
 *
 *****************************************************************************/
HANDEL_STATIC int xiaLoadDetector(FILE *fp, fpos_t *start, fpos_t *end)
{
  int status;
	
  unsigned short i;
  unsigned short numChans;

  double gain;
  double typeValue;

  char value[MAXITEM_LEN];
  char alias[MAXALIAS_LEN];
  char name[MAXITEM_LEN];

  /* We need to load things in a certain order since some information must be
   * specified to HanDeL before others. The following order should work well:
   * 1) alias
   * 2) number of channels
   * 3) rest of the detector information
   */

  status = xiaFileRA(fp, start, end, "alias", value); 
	
  if (status != XIA_SUCCESS)
    {
      xiaLogError("xiaLoadDetector", "Unable to load alias information", status);
      return status;
    }

  sprintf(info_string, "alias = %s", value);
  xiaLogDebug("xiaLoadDetector", info_string);

  strcpy(alias, value);

  status = xiaNewDetector(alias);

  if (status != XIA_SUCCESS)
    {
      xiaLogError("xiaLoadDetector", "Error creating new detector", status);
      return status;
    }

  status = xiaFileRA(fp, start, end, "number_of_channels", value);

  if (status != XIA_SUCCESS)
    {
      xiaLogError("xiaLoadDetector", "Unable to find number_of_channels", status);
      return status;
    }

  sscanf(value, "%hu", &numChans);

  sprintf(info_string, "number_of_channels = %u", numChans);
  xiaLogDebug("xiaLoadDetector", info_string);

  status = xiaAddDetectorItem(alias, "number_of_channels", (void *)&numChans);

  if (status != XIA_SUCCESS)
    {
      sprintf(info_string, "Error adding number_of_channels to detector %s", alias);
      xiaLogError("xiaLoadDetector", info_string, status);
      return status;
    }

  status = xiaFileRA(fp, start, end, "type", value);

  if (status != XIA_SUCCESS)
    {
      sprintf(info_string, "Unable to find type for detector %s", alias);
      xiaLogError("xiaLoadDetector", info_string, status);
      return status;
    }
 
  status = xiaAddDetectorItem(alias, "type", (void *)value);

  if (status != XIA_SUCCESS)
    {
      sprintf(info_string, "Error adding type to detector %s", alias);
      xiaLogError("xiaLoadDetector", info_string, status);
      return status;
    }

  status = xiaFileRA(fp, start, end, "type_value", value);

  if (status != XIA_SUCCESS)
    {
      sprintf(info_string, "Unable to find type_value for detector %s", alias);
      xiaLogError("xiaLoadDetector", info_string, status);
      return status;
    }

  sscanf(value, "%lf", &typeValue);
 
  status = xiaAddDetectorItem(alias, "type_value", (void *)&typeValue);

  if (status != XIA_SUCCESS)
    {
      sprintf(info_string, "Error adding type_value to detector %s", alias);
      xiaLogError("xiaLoadDetector", info_string, status);
      return status;
    }

  for (i = 0; i < numChans; i++)
    {
      sprintf(name, "channel%u_gain", i);
      status = xiaFileRA(fp, start, end, name, value);

      if (status == XIA_FILE_RA)
        {
          sprintf(info_string, "Current configuration file missing %s", name);
          xiaLogWarning("xiaLoadDetector", info_string);
		
        } else {

        if (status != XIA_SUCCESS)
          {
            xiaLogError("xiaLoadDetector", "Unable to load channel gain", status);
            return status;
          }

        sscanf(value, "%6lf", &gain);

        sprintf(info_string, "%s = %f", name, gain);
        xiaLogDebug("xiaLoadDetector", info_string);

        status = xiaAddDetectorItem(alias, name, (void *)&gain);

        if (status != XIA_SUCCESS)
          {
            sprintf(info_string, "Error adding %s to detector %s", name, alias);
            xiaLogError("xiaLoadDetector", info_string, status);
            return status;
          }
      }

      sprintf(name, "channel%u_polarity", i);
      status = xiaFileRA(fp, start, end, name, value);

      if (status == XIA_FILE_RA)
        {
          sprintf(info_string, "Current configuration file missing %s", name);
          xiaLogWarning("xiaLoadDetector", info_string);

        } else {

        if (status != XIA_SUCCESS)
          {
            xiaLogError("xiaLoadDetector", "Unable to load channel polarity", status);
            return status;
          }

        /*sscanf(value, "%s", polarity);*/
 
        sprintf(info_string, "%s = %s", name, value);
        xiaLogDebug("xiaLoadDetector", info_string);

        status = xiaAddDetectorItem(alias, name, (void *)value);

        if (status != XIA_SUCCESS)
          {
            sprintf(info_string, "Error adding %s to detector %s", name, alias);
            xiaLogError("xiaLoadDetector", info_string, status);
            return status;
          }
      }
    }

  return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine parses data in from fp (and bounded by start & end) as 
 * module information. If it fails, then it fails hard and the user needs 
 * to fix their inifile.
 *
 *****************************************************************************/ 
HANDEL_STATIC int xiaLoadModule(FILE *fp, fpos_t *start, fpos_t *end)
{
  int status;
  int chanAlias;
  unsigned int itemp;

  byte_t pciSlot;
  byte_t pciBus;
	
  unsigned int numChans;
  unsigned int scsiBus;
  unsigned int crate;
  unsigned int slot;
  unsigned int eppAddr;
  unsigned int daisyID;
  unsigned int i;
  unsigned int comPort;
  unsigned int baudRate;
  unsigned int deviceNumber;

  double chanGain;

  char value[MAXITEM_LEN];
  char alias[MAXALIAS_LEN];
  char interface[MAXITEM_LEN];
  char moduleType[MAXITEM_LEN];
  char name[MAXITEM_LEN];
  char detAlias[MAXALIAS_LEN];
  char firmAlias[MAXALIAS_LEN];
  char defAlias[MAXALIAS_LEN];

  status = xiaFileRA(fp, start, end, "alias", value);

  if (status != XIA_SUCCESS)
    {
      xiaLogError("xiaLoadModule", "Unable to load alias information", status);
      return status;
    }
	
  sprintf(info_string, "alias = %s", value);
  xiaLogDebug("xiaLoadModule", info_string);
 
  strcpy(alias, value);

  status = xiaNewModule(alias);

  if (status != XIA_SUCCESS)
    {
      xiaLogError("xiaLoadModule", "Error creating new module", status);
      return status;
    }

  status = xiaFileRA(fp, start, end, "module_type", value);

  if (status != XIA_SUCCESS)
    {
      xiaLogError("xiaLoadModule", "Unable to load module type", status);
      return status;
    }

  sscanf(value, "%s", moduleType);

  sprintf(info_string, "moduleType = %s", moduleType);
  xiaLogDebug("xiaLoadModule", info_string);

  status = xiaAddModuleItem(alias, "module_type", (void *)moduleType);

  if (status != XIA_SUCCESS)
    {
      sprintf(info_string, "Error adding module type to module %s", alias);
      xiaLogError("xiaLoadModule", info_string, status);
    }

  status = xiaFileRA(fp, start, end, "number_of_channels", value);

  if (status != XIA_SUCCESS)
    {
      xiaLogError("xiaLoadModule", "Unable to load number of channels", status);
      return status;
    }

  sscanf(value, "%u", &numChans);

  sprintf(info_string, "number_of_channels = %u", numChans);
  xiaLogDebug("xiaLoadModule", info_string);

  status = xiaAddModuleItem(alias, "number_of_channels", (void *)&numChans);

  if (status != XIA_SUCCESS)
    {
      sprintf(info_string, "Error adding number_of_channels to module %s", alias);
      xiaLogError("xiaLoadModule", info_string, status);
      return status;
    }

  /* Deal with interface here */
  status = xiaFileRA(fp, start, end, "interface", value);

  sscanf(value, "%s", interface);

  sprintf(info_string, "interface = %s", interface);
  xiaLogDebug("xiaLoadModule", info_string);

  if (status != XIA_SUCCESS)
    {
      xiaLogError("xiaLoadModule", "Unable to load interface", status);
      return status;
    }

  if ((STREQ(interface, "j73a")) ||
      (STREQ(interface, "genericSCSI")))
    {
      status = xiaAddModuleItem(alias, "interface", (void *)interface);

      if (status != XIA_SUCCESS)
        {
          sprintf(info_string, "Error adding interface to module %s", alias);
          xiaLogError("xiaLoadModule", info_string, status);
          return status;
        }

      status = xiaFileRA(fp, start, end, "scsibus_number", value);

      if (status != XIA_SUCCESS)
        {
          xiaLogError("xiaLoadModule", "Unable to load SCSI bus number", status);
          return status;
        }

      sscanf(value, "%u", &scsiBus);

      sprintf(info_string, "scsiBus = %u", scsiBus);
      xiaLogDebug("xiaLoadModule", info_string);

      status = xiaAddModuleItem(alias, "scsibus_number", (void *)&scsiBus);

      if (status != XIA_SUCCESS)
        {
          sprintf(info_string, "Error adding scsi bus number to module %s", alias);
          xiaLogError("xiaLoadModule", info_string, status);
          return status;
        }

      status = xiaFileRA(fp, start, end, "crate_number", value);

      if (status != XIA_SUCCESS)
        {
          xiaLogError("xiaLoadModule", "Unable to load crate number", status);
          return status;
        }

      sscanf(value, "%u", &crate);

      sprintf(info_string, "crate = %u", crate);
      xiaLogDebug("xiaLoadModule", info_string);

      status = xiaAddModuleItem(alias, "crate_number", (void *)&crate);

      if (status != XIA_SUCCESS)
        {
          sprintf(info_string, "Error adding crate number to module %s", alias);
          xiaLogError("xiaLoadModule", info_string, status);
          return status;
        }

      status = xiaFileRA(fp, start, end, "slot", value);

      if (status != XIA_SUCCESS)
        {
          xiaLogError("xiaLoadModule", "Unable to load slot number", status);
          return status;
        }

      sscanf(value, "%u", &slot);

      sprintf(info_string, "slot = %u", slot);
      xiaLogDebug("xiaLoadModule", info_string);

      status = xiaAddModuleItem(alias, "slot", (void *)&slot);

      if (status != XIA_SUCCESS)
        {
          sprintf(info_string, "Error adding slot number to module %s", alias);
          xiaLogError("xiaLoadModule", info_string, status);
          return status;
        }

    } else if ((STREQ(interface, "epp")) ||
               (STREQ(interface, "genericEPP")))
    {
      status = xiaFileRA(fp, start, end, "epp_address", value);

      if (status != XIA_SUCCESS)
        {
          xiaLogError("xiaLoadModule", "Unable to load EPP address", status);
          return status;
        }

      sscanf(value, "%x", &eppAddr);

      sprintf(info_string, "EPP Address = %#x", eppAddr);
      xiaLogDebug("xiaLoadModule", info_string);

      status = xiaAddModuleItem(alias, "epp_address", (void *)&eppAddr);

      if (status != XIA_SUCCESS)
        {
          sprintf(info_string, "Error adding EPP address to module %s", alias);
          xiaLogError("xiaLoadModule", info_string, status);
          return status;
        }

      status = xiaFileRA(fp, start, end, "daisy_chain_id", value);

      /* This is an extremely optional setting, so we really only want to do 
       * anything if the value actually shows up in the section.
       */
      if (status == XIA_SUCCESS)
        {
          sscanf(value, "%u", &daisyID);
 
          sprintf(info_string, "Daisy Chain ID = %#x", daisyID);
          xiaLogDebug("xiaLoadModule", info_string);

          status = xiaAddModuleItem(alias, "daisy_chain_id", (void *)&daisyID);

          if (status != XIA_SUCCESS)
            {
              sprintf(info_string, "Error adding daisy chain id to module %s", alias);
              xiaLogError("xiaLoadModule", info_string, status);
              return status;
            }
        }

    } else if (STREQ(interface, "usb")) {
	  
    status = xiaAddModuleItem(alias, "interface", interface);

    if (status != XIA_SUCCESS) {
      sprintf(info_string, "Error setting interface to '%s' for module "
              "with alias '%s'", interface, alias);
      xiaLogError("xiaLoadModule", info_string, status);
      return status;
    }

	  status = xiaFileRA(fp, start, end, "device_number", value);
	  
	  if (status != XIA_SUCCESS) {
		
	    xiaLogError("xiaLoadModule", "Unable to load device number", status);
	    return status;
	  }

	  sscanf(value, "%u", &deviceNumber);
	  
	  sprintf(info_string, "Device Number = %u", deviceNumber);
	  xiaLogDebug("xiaLoadModule", info_string);
	  
	  status = xiaAddModuleItem(alias, "device_number", (void *)&deviceNumber);
	  
	  if (status != XIA_SUCCESS) {
		
	    sprintf(info_string, "Error adding Device Number to module %s", alias);
	    xiaLogError("xiaLoadModule", info_string, status);
	    return status;
	  }
    
  } else if (STREQ(interface, "usb2")) {

    status = xiaAddModuleItem(alias, "interface", interface);

    if (status != XIA_SUCCESS) {
      sprintf(info_string, "Error setting interface to '%s' for module "
              "with alias '%s'", interface, alias);
      xiaLogError("xiaLoadModule", info_string, status);
      return status;
    }
	  
	  status = xiaFileRA(fp, start, end, "device_number", value);
	  
	  if (status != XIA_SUCCESS) {
		
	    xiaLogError("xiaLoadModule", "Unable to load device number", status);
	    return status;
	  }

	  sscanf(value, "%u", &deviceNumber);
	  
	  sprintf(info_string, "Device Number = %u", deviceNumber);
	  xiaLogDebug("xiaLoadModule", info_string);
	  
	  status = xiaAddModuleItem(alias, "device_number", (void *)&deviceNumber);
	  
	  if (status != XIA_SUCCESS) {
		
	    sprintf(info_string, "Error adding Device Number to module %s", alias);
	    xiaLogError("xiaLoadModule", info_string, status);
	    return status;
	  }
	 
	} else if (STREQ(interface, "pxi")) {
	  
	  status = xiaFileRA(fp, start, end, "pci_slot", value);

	  if (status != XIA_SUCCESS) {
      xiaLogError("xiaLoadModule", "Unable to load 'pci_slot'", status);
      return status;
	  }

	  sscanf(value, "%u", &itemp);
          pciSlot = itemp;

	  sprintf(info_string, "PCI Slot = %u", pciSlot);
	  xiaLogDebug("xiaLoadModule", info_string);

	  status = xiaAddModuleItem(alias, "pci_slot", (void *)&pciSlot);

	  if (status != XIA_SUCCESS) {
      sprintf(info_string, "Error adding PCI slot to module %s", alias);
      xiaLogError("xiaLoadModule", info_string, status);
      return status;
	  } 

	  status = xiaFileRA(fp, start, end, "pci_bus", value);

	  if (status != XIA_SUCCESS) {
      xiaLogError("xiaLoadModule", "Unable to load 'pci_bus'", status);
      return status;
	  }

	  sscanf(value, "%u", &itemp);
          pciBus = itemp;

	  sprintf(info_string, "PCI Bus = %u", pciBus);
	  xiaLogDebug("xiaLoadModule", info_string);

	  status = xiaAddModuleItem(alias, "pci_bus", (void *)&pciBus);

	  if (status != XIA_SUCCESS) {
      sprintf(info_string, "Error adding PCI bus to module %s", alias);
      xiaLogError("xiaLoadModule", info_string, status);
      return status;
	  }

  } else if (STREQ(interface, "serial")) {
	  
	  status = xiaFileRA(fp, start, end, "com_port", value);
	  
	  if (status != XIA_SUCCESS) {
		
	    xiaLogError("xiaLoadModule", "Unable to load COM port", status);
	    return status;
	  }

	  sscanf(value, "%u", &comPort);
	  
	  sprintf(info_string, "COM Port = %u", comPort);
	  xiaLogDebug("xiaLoadModule", info_string);
	  
	  status = xiaAddModuleItem(alias, "com_port", (void *)&comPort);
	  
	  if (status != XIA_SUCCESS) {
		
	    sprintf(info_string, "Error adding COM port to module %s", alias);
	    xiaLogError("xiaLoadModule", info_string, status);
	    return status;
	  }
	  
	  status = xiaFileRA(fp, start, end, "baud_rate", value);
	  
	  if (status != XIA_SUCCESS) {
		
	    xiaLogError("xiaLoadModule", "Unable to load baud rate", status);
	    return status;
	  }
	  
	  sscanf(value, "%u", &baudRate);
	  
	  sprintf(info_string, "Baud Rate = %u", baudRate);
	  xiaLogDebug("xiaLoadModule", info_string);
	  
	  status = xiaAddModuleItem(alias, "baud_rate", (void *)&baudRate);
	  
	  if (status != XIA_SUCCESS) {

	    sprintf(info_string, "Error adding baud rate to module %s", alias);
	    xiaLogError("xiaLoadModule", info_string, status);
	    return status;
	  }

  } else {
	  
    status = XIA_BAD_INTERFACE;
    sprintf(info_string, "The interface defined for %s does not exist", alias);
    xiaLogError("xiaLoadModule", info_string, status);
    return status;
  }

  for (i = 0; i < numChans; i++)
    {
      sprintf(name, "channel%u_alias", i);
      status = xiaFileRA(fp, start, end, name, value);

      if (status != XIA_SUCCESS)
        {
          sprintf(info_string, "Unable to load %s from %s", name, alias);
          xiaLogError("xiaLoadModule", info_string, status);
          return status;
        }

      sscanf(value, "%d", &chanAlias);
 
      sprintf(info_string, "%s = %d", name, chanAlias);
      xiaLogDebug("xiaLoadDetector", info_string);

      status = xiaAddModuleItem(alias, name, (void *)&chanAlias);

      if (status != XIA_SUCCESS)
        {
          sprintf(info_string, "Error adding %s to module %s", name, alias);
          xiaLogError("xiaLoadModule", info_string, status);
          return status;
        }

      sprintf(name, "channel%u_detector", i);
      status = xiaFileRA(fp, start, end, name, value);

      if (status == XIA_FILE_RA)
        {
          sprintf(info_string, "Current configuration file missing %s", name);
          xiaLogWarning("xiaLoadModule", info_string);

        } else {

        if (status != XIA_SUCCESS)
          {
            xiaLogError("xiaLoadModule", "Unable to load channel detector alias", status);
            return status;
          }

        sscanf(value, "%s", detAlias);
 
        sprintf(info_string, "%s = %s", name, detAlias);
        xiaLogDebug("xiaLoadModule", info_string);

        status = xiaAddModuleItem(alias, name, (void *)detAlias);

        if (status != XIA_SUCCESS)
          {
            sprintf(info_string, "Error adding %s to module %s", name, alias);
            xiaLogError("xiaLoadModule", info_string, status);
            return status;
          }
      }

      sprintf(name, "channel%u_gain", i);
      status = xiaFileRA(fp, start, end, name, value);

      if (status == XIA_FILE_RA)
        {
          sprintf(info_string, "Current configuration file missing %s", name);
          xiaLogWarning("xiaLoadModule", info_string);

        } else {

        if (status != XIA_SUCCESS)
          {
            xiaLogError("xiaLoadModule", "Unable to load channel gain", status);
            return status;
          }

        sscanf(value, "%lf", &chanGain);

        sprintf(info_string, "%s = %.3f", name, chanGain);
        xiaLogDebug("xiaLoadModule", info_string);

        status = xiaAddModuleItem(alias, name, (void *)&chanGain);

        if (status != XIA_SUCCESS)
          {
            sprintf(info_string, "Error adding %s to module %s", name, alias);
            xiaLogError("xiaLoadModule", info_string, status);
            return status;
          }
      }
    }

  /* Need a little extra logic to determine how to load the firmware
   * and defaults. Check for *_all first and if that isn't found then
   * try and find ones for individual channels.
   */
  status = xiaFileRA(fp, start, end, "firmware_set_all", value);

  if (status != XIA_SUCCESS)
    {
      for (i = 0; i < numChans; i++)
        {
          sprintf(name, "firmware_set_chan%u", i);
          status = xiaFileRA(fp, start, end, name, value);

          if (status == XIA_FILE_RA)
            {
              sprintf(info_string, "Current configuration file missing %s", name);
              xiaLogWarning("xiaLoadModule", info_string);

            } else {

            if (status != XIA_SUCCESS)
              {
                xiaLogError("xiaLoadModule", "Unable to load channel firmware information", status);
                return status;
              }

            strcpy(firmAlias, value);

            sprintf(info_string, "%s = %s", name, firmAlias);
            xiaLogDebug("xiaLoadModule", info_string);

            status = xiaAddModuleItem(alias, name, (void *)firmAlias);

            if (status != XIA_SUCCESS)
              {
                sprintf(info_string, "Error adding %s to module %s", name, alias);
                xiaLogError("xiaLoadModule", info_string, status);
                return status;
              }
          }
        }
	
    } else {

    strcpy(firmAlias, value);

    status = xiaAddModuleItem(alias, "firmware_set_all", (void *)firmAlias);

    if (status != XIA_SUCCESS)
      {
        sprintf(info_string, "Error adding firmware_set_all to module %s", alias);
        xiaLogError("xiaLoadModule", info_string, status);
        return status;
      }
  }

  status = xiaFileRA(fp, start, end, "default_all", value);

  if (status != XIA_SUCCESS)
    {
      for (i = 0; i < numChans; i++)
        {
          sprintf(name, "default_chan%u", i);
          status = xiaFileRA(fp, start, end, name, value);

          if (status == XIA_FILE_RA)
            {
              sprintf(info_string, "Current configuration file missing %s", name);
              xiaLogWarning("xiaLoadModule", info_string);

            } else {

            if (status != XIA_SUCCESS)
              {
                xiaLogError("xiaLoadModule", "Unable to load channel default information", status);
                return status;
              }

            strcpy(defAlias, value);

            sprintf(info_string, "%s = %s", name, defAlias);
            xiaLogDebug("xiaLoadModule", info_string);

            status = xiaAddModuleItem(alias, name, (void *)defAlias);

            if (status != XIA_SUCCESS)
              {
                sprintf(info_string, "Error adding %s to module %s", name, alias);
                xiaLogError("xiaLoadModule", info_string, status);
                return status;
              }
          }
        }
	
    } else {

    strcpy(defAlias, value);

    status = xiaAddModuleItem(alias, "default_all", (void *)defAlias);

    if (status != XIA_SUCCESS)
      {
        sprintf(info_string, "Error adding firmware_set_all to module %s", alias);
        xiaLogError("xiaLoadModule", info_string, status);
        return status;
      }
  }

  return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine parses data in from fp (and bounded by start & end) as 
 * firmware information. If it fails, then it fails hard and the user needs 
 * to fix their inifile.
 *
 *****************************************************************************/
HANDEL_STATIC int xiaLoadFirmware(FILE *fp, fpos_t *start, fpos_t *end)
{
  int status;

  unsigned short i;
  unsigned short numKeywords;

  char value[MAXITEM_LEN];
  char alias[MAXALIAS_LEN];
  char file[MAXFILENAME_LEN];
  char mmu[MAXFILENAME_LEN];
  char path[MAXITEM_LEN];
  /* k-e-y-w-o-r-d + (possibly) 2 numeric digits + \0
   * N.b. Better not be more then 100 keywords 
   */
  char keyword[10];

  status = xiaFileRA(fp, start, end, "alias", value);

  if (status != XIA_SUCCESS)
    {
      xiaLogError("xiaLoadFirmware", "Unable to load alias information", status);
      return status;
    }

  sprintf(info_string, "alias = %s", value);
  xiaLogDebug("xiaLoadFirmware", info_string);

  strcpy(alias, value);

  status = xiaNewFirmware(alias);

  if (status != XIA_SUCCESS)
    {
      xiaLogError("xiaLoadFirmware", "Error creating new firmware", status);
      return status;
    }

  /* Check for an MMU first since we'll be exiting if we find a filename */
  status = xiaFileRA(fp, start, end, "mmu", value);

  if ((status != XIA_FILE_RA) &&
      (status == XIA_SUCCESS))
    {
      sprintf(info_string, "mmu = %s", value);
      xiaLogDebug("xiaLoadFirmware", info_string);

      strcpy(mmu, value);
      status = xiaAddFirmwareItem(alias, "mmu", (void *)mmu);

      if (status != XIA_SUCCESS)
        {
          sprintf(info_string, "Error adding MMU to alias %s", alias);
          xiaLogError("xiaLoadFirmware", info_string, status);
          return status;
        }
    }

  /* If we find a filename, then we are done and can return */
  status = xiaFileRA(fp, start, end, "filename", value);

  if (status == XIA_SUCCESS)
    {
      sprintf(info_string, "filename = %s", value);
      xiaLogDebug("xiaLoadFirmware", info_string);

      strcpy(file, value);
      status = xiaAddFirmwareItem(alias, "filename", (void *)file);

      if (status != XIA_SUCCESS)
        {
          sprintf(info_string, "Error adding filename to alias %s", alias); 
          xiaLogError("xiaLoadFirmware", info_string, status);
          return status;
        }

      status = xiaFileRA(fp, start, end, "fdd_tmp_path", value);

      if (status == XIA_SUCCESS) {
        strcpy(path, value);
    
        status = xiaAddFirmwareItem(alias, "fdd_tmp_path", (void *)path);

        if (status != XIA_SUCCESS) {
          sprintf(info_string, "Error adding FDD temporary path to '%s'", alias);
          xiaLogError("xiaLoadFirmware", info_string, status);
          return status;
        }
      }

      /* Check for keywords, if any...no need to really warn since the most 
       * important "keywords" are generated by Handel.
       */
      status = xiaFileRA(fp, start, end, "num_keywords", value);
	
      if ((status != XIA_FILE_RA) &&
          (status == XIA_SUCCESS))
        {
          sprintf(info_string, "num_keywords = %s", value);
          xiaLogDebug("xiaLoadFirmware", info_string);

          sscanf(value, "%hu", &numKeywords);
          for (i = 0; i < numKeywords; i++)
            {
              sprintf(keyword, "keyword%u", i);
              status = xiaFileRA(fp, start, end, keyword, value);

              if (status != XIA_SUCCESS)
                {
                  xiaLogError("xiaLoadFirmware", "Unable to load keyword", status);
                  return status;
                }
		
              sprintf(info_string, "%s = %s", keyword, value);
              xiaLogDebug("xiaLoadFirmware", info_string);

              status = xiaAddFirmwareItem(alias, "keyword", (void *)keyword);

              if (status != XIA_SUCCESS)
                {
                  sprintf(info_string, "Error adding keyword, %s, to alias %s", keyword, alias);
                  xiaLogError("xiaLoadFirmware", info_string, status);
                  return status;
                }
            }
        }
      /* Don't even bother trying to parse in more information */
      return XIA_SUCCESS;
    }

  /* Need to be a little careful here about how we parse in the PTRR chunks.
   * Start slowly by getting the number of PTRRs first.
   */
  status = xiaReadPTRRs(fp, start, end, alias);

  if (status != XIA_SUCCESS)
    {
      sprintf(info_string, "Error loading PTRR information for alias %s", alias);
      xiaLogError("xiaLoadFirmware", info_string, status);
      return status;
    }

  return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine parses in the information specified in the defaults 
 * definitions.
 *
 *****************************************************************************/
HANDEL_STATIC int xiaLoadDefaults(FILE *fp, fpos_t *start, fpos_t *end)
{
  int status;

  char value[MAXITEM_LEN];
  char alias[MAXALIAS_LEN];
  char tmpName[MAXITEM_LEN];
  char tmpValue[MAXITEM_LEN];
  char endLine[132];
  char line[132];

  double defValue;

  fpos_t dataStart;


  status = xiaFileRA(fp, start, end, "alias", value);
 
  if (status != XIA_SUCCESS)
    {
      xiaLogError("xiaLoadDefaults", "Unable to load alias information", status);
      return status;
    }

  sprintf(info_string, "alias = %s", value);
  xiaLogDebug("xiaLoadDefaults", info_string);

  strcpy(alias, value);

  status = xiaNewDefault(alias);

  if (status != XIA_SUCCESS)
    {
      xiaLogError("xiaLoadDefaults", "Error creating new default", status);
      return status;
    }

  /* Want a position after the alias line so that we can just read in line-
   * by-line until we reach endLine
   */
  xiaSetPosOnNext(fp, start, end, "alias", &dataStart, TRUE_);

  fsetpos(fp, end);
  status = xiaGetLine(fp, endLine);

  fsetpos(fp, &dataStart);
  status = xiaGetLine(fp, line);
 
  while (!STREQ(line, endLine))
	  {
      status = xiaGetLineData(line, tmpName, tmpValue);
      if (status != XIA_SUCCESS)
        {
          sprintf(info_string, "Error getting data for entry %s", tmpName);
          xiaLogError("xiaLoadDefaults", info_string, status);
          return status;
        }
		
      sscanf(tmpValue, "%lf", &defValue);
		
      status = xiaAddDefaultItem(alias, tmpName, (void *)&defValue);
		
      if (status != XIA_SUCCESS)
        {
          sprintf(info_string, "Error adding %s (value = %.3f) to alias %s", tmpName, defValue, alias);
          xiaLogError("xiaLoadDefaults", info_string, status);
          return status;
        }
		

      sprintf(info_string, "Added %s (value = %.3f) to alias %s", tmpName, defValue, alias);
      xiaLogDebug("xiaLoadDefaults", info_string);

      status = xiaGetLine(fp, line);
    }

  return XIA_SUCCESS;
}

	


/*****************************************************************************
 *
 * This routine will read in several PTRRs(*) and add them to the Firmware
 * indicated by alias.
 *
 * (*) -- Actually, it will read in the number specified by number_of_ptrrs.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaReadPTRRs(FILE *fp, fpos_t *start, fpos_t *end, char *alias)
{
  int status;	

  unsigned short i;
  unsigned short ptrr;
  unsigned short numFilter;
  unsigned short filterInfo;

  double min_peaking_time;
  double max_peaking_time;

  /* f+i+l+t+e+r+_+i+n+f+o+ 2 digits + \0 */
  char filterName[14];
  char value[MAXITEM_LEN];

  fpos_t newStart;
  fpos_t newEnd;
  fpos_t lookAheadStart;

  boolean_t isLast = FALSE_;


  xiaLogDebug("xiaReadPTRRs", "Starting parse of PTRRs");

  /* This assumes that there is at least one PTRR for a specified alias */
  newEnd = *start;
  while (!isLast)
    {
      xiaSetPosOnNext(fp, &newEnd, end, "ptrr", &lookAheadStart, TRUE_);
      xiaSetPosOnNext(fp, &newEnd, end, "ptrr", &newStart, FALSE_);

      /* Find the end here: either the END or another ptrr */
      status = xiaSetPosOnNext(fp, &lookAheadStart, end, "ptrr", &newEnd, FALSE_);

      if (status == XIA_END)
        {
          isLast = TRUE_;
        }

      /* Do the actual actions here */
      status = xiaFileRA(fp, &newStart, &newEnd, "ptrr", value);

      if (status != XIA_SUCCESS)
        {
          xiaLogError("xiaReadPTRRs", "Unable to read ptrr from file", status);
          return status;
        }

      sscanf(value, "%hu", &ptrr);

      sprintf(info_string, "ptrr = %u", ptrr);
      xiaLogDebug("xiaReadPTRRs", info_string);

      status = xiaAddFirmwareItem(alias, "ptrr", (void *)&ptrr);

      if (status != XIA_SUCCESS)
        {
          sprintf(info_string, "Error adding ptrr to alias %s", alias);
          xiaLogError("xiaReadPTRRs", info_string, status);
          return status;
        }

      status = xiaFileRA(fp, &newStart, &newEnd, "min_peaking_time", value);

      if (status != XIA_SUCCESS)
        {
          sprintf(info_string, "Unable to read min_peaking_time from ptrr = %u", ptrr);
          xiaLogError("xiaReadPTRRs", info_string, status);
          return status;
        }

      sscanf(value, "%lf", &min_peaking_time);

      status = xiaAddFirmwareItem(alias, "min_peaking_time", (void *)&min_peaking_time);

      if (status != XIA_SUCCESS)
        {
          sprintf(info_string, "Error adding min_peaking_time to alias %s", alias);
          xiaLogError("xiaReadPTRRs", info_string, status);
          return status;
        }

      status = xiaFileRA(fp, &newStart, &newEnd, "max_peaking_time", value);

      if (status != XIA_SUCCESS)
        {
          sprintf(info_string, "Unable to read max_peaking_time from ptrr = %u", ptrr);
          xiaLogError("xiaReadPTRRs", info_string, status);
          return status;
        }

      sscanf(value, "%lf", &max_peaking_time);

      status = xiaAddFirmwareItem(alias, "max_peaking_time", (void *)&max_peaking_time);

      if (status != XIA_SUCCESS)
        {
          sprintf(info_string, "Error adding max_peaking_time to alias %s", alias);
          xiaLogError("xiaReadPTRRs", info_string, status);
          return status;
        }

      status = xiaFileRA(fp, &newStart, &newEnd, "fippi", value);

      if (status != XIA_SUCCESS)
        {
          sprintf(info_string, "Unable to read fippi from ptrr = %u", ptrr);
          xiaLogError("xiaReadPTRRs", info_string, status);
          return status;
        }

      status = xiaAddFirmwareItem(alias, "fippi", (void *)value);

      if (status != XIA_SUCCESS)
        {
          sprintf(info_string, "Error adding fippi to alias %s", alias);
          xiaLogError("xiaReadPTRRs", info_string, status);
          return status;
        }

      status = xiaFileRA(fp, &newStart, &newEnd, "dsp", value);

      if (status != XIA_SUCCESS)
        {
          sprintf(info_string, "Unable to read dsp from ptrr = %u", ptrr);
          xiaLogError("xiaReadPTRRs", info_string, status);
          return status;
        }

      status = xiaAddFirmwareItem(alias, "dsp", (void *)value);

      if (status != XIA_SUCCESS)
        {
          sprintf(info_string, "Error adding dsp to alias %s", alias);
          xiaLogError("xiaReadPTRRs", info_string, status);
          return status;
        }

      /* Check for the quite optional "user_fippi"... */
      status = xiaFileRA(fp, &newStart, &newEnd, "user_fippi", value);

      if (status == XIA_SUCCESS)
        {
          status = xiaAddFirmwareItem(alias, "user_fippi", (void *)value);

          if (status != XIA_SUCCESS)
            {
              sprintf(info_string, "Error adding user_fippi to alias %s", alias);
              xiaLogError("xiaReadPTRRs", info_string, status);
              return status;
            }

        } else if (status == XIA_FILE_RA) {

        xiaLogInfo("xiaReadPTRRs", "No user_fippi present in .ini file");

      } else {

        sprintf(info_string, "Unable to read user_fippi from ptrr = %u", ptrr);
        xiaLogError("xiaReadPTRRs", info_string, status);
        return status;
      }
	
      status = xiaFileRA(fp, &newStart, &newEnd, "num_filter", value);

      if (status != XIA_SUCCESS)
        {
          sprintf(info_string, "Unable to read num_filter from ptrr = %u", ptrr);
          xiaLogError("xiaReadPTRRs", info_string, status);
          return status;
        }

      sscanf(value, "%hu", &numFilter);

      sprintf(info_string, "numFilter = %u", numFilter);
      xiaLogDebug("xiaReadPTRRs", info_string);

      for (i = 0; i < numFilter; i++)
        {
          sprintf(filterName, "filter_info%u", i);
          status = xiaFileRA(fp, &newStart, &newEnd, filterName, value);

          if (status != XIA_SUCCESS)
            {
              sprintf(info_string, "Unable to read %s from ptrr = %u", filterName, ptrr);
              xiaLogError("xiaReadPTRRs", info_string, status);
              return status;
            }

          sscanf(value, "%hu", &filterInfo);

          sprintf(info_string, "filterInfo = %u", filterInfo);
          xiaLogDebug("xiaReadPTRRs", info_string);

          status = xiaAddFirmwareItem(alias, "filter_info", (void *)&filterInfo);

          if (status != XIA_SUCCESS)
            {
              sprintf(info_string, "Error adding filter_info to alias %s", alias);
              xiaLogError("xiaReadPTRRs", info_string, status);
              return status;
            }
        }

    }

  return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine searches between start and end for name. If it finds a name,
 * it sets newPos to that location. If not, it returns a value of XIA_END
 * and sets newPos to end. There are a couple of important caveats here:
 * You can't use fpos_t pointers in direct arithmetic comparisons. They just
 * don't work that way. Instead, I set the file to the position and then read
 * a line from the file. This line serves as the "unique" identifier from
 * which all future comparisons are done. There is a finite probability that
 * the same string may appear elsewhere in the file. Hopefully the same string
 * won't appear twice between start and end.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaSetPosOnNext(FILE *fp, fpos_t *start, fpos_t *end, char *name, fpos_t *newPos, boolean_t after)
{
  int status;

  char endLine[132];
  char line[132];
  char tmpValue[132];
  char tmpName[132];

  fsetpos(fp, end);
  status = xiaGetLine(fp, endLine);

  fsetpos(fp, start);
  fgetpos(fp, newPos);
  status = xiaGetLine(fp, line);

  sprintf(info_string, "endLine: %s", endLine);
  xiaLogDebug("xiaSetPosOnNext", info_string);
  sprintf(info_string, "startLine: %s", line);
  xiaLogDebug("xiaSetPosOnNext", info_string);

  while (!STREQ(line, endLine))
    {
      status = xiaGetLineData(line, tmpName, tmpValue);
 
      if (status != XIA_SUCCESS)
        {
          sprintf(info_string, "Error trying to find %s", name);
          xiaLogError("xiaSetPosOnNext", info_string, status);
          return status;
        }


      if (STREQ(name, tmpName))
        {
          if (after)
            {
              fgetpos(fp, newPos);
            }

          fsetpos(fp, newPos);
          status = xiaGetLine(fp, line);
          sprintf(info_string, "newPos set to line: %s", line);
          xiaLogDebug("xiaSetPosOnNext", info_string);

          return XIA_SUCCESS;
        }

      fgetpos(fp, newPos);

      status = xiaGetLine(fp, line);
    }

  /* Okay, we must have made it to the end of the file */
  return XIA_END;
}


/*****************************************************************************
 *
 * This routine will attempt to find the value from the specified name-value
 * pair. Returns XIA_FILE_RA if it couldn't find anything. It calls 
 * xiaGetLine() until the name is matched or the end condition is satisfied.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaFileRA(FILE *fp, fpos_t *start, fpos_t *end, char *name, char *value)
{
  int status;
	
  /* Use this to hold the "value" of value until we're sure that this is the
   * right name and we can return.
   */
  char tmpValue[132];
  char tmpName[132];
  char endLine[132];

  fsetpos(fp, end);
  status = xiaGetLine(fp, endLine);

  fsetpos(fp, start);
  status = xiaGetLine(fp, line);
  /* 
     sprintf(info_string, "endLine: %s", endLine);
     xiaLogDebug("xiaFileRA", info_string);
     sprintf(info_string, "startLine: %s", line);
     xiaLogDebug("xiaFileRA", info_string);
  */

  while (!STREQ(line, endLine))
	  {
      status = xiaGetLineData(line, tmpName, tmpValue);
      /*
        sprintf(info_string, "tmpName = %s", tmpName);
        xiaLogDebug("xiaFileRA", info_string);
		  
        sprintf(info_string, "tmpValue = %s", tmpValue);
        xiaLogDebug("xiaFileRA", info_string);
      */
		
      if (status != XIA_SUCCESS)
        {
          sprintf(info_string, "Error trying to find value for %s", name);
          xiaLogError("xiaFileRA", info_string, status);
          return status;
        }
		
      if (STREQ(name, tmpName))
        {
          strcpy(value, tmpValue);
			
          return XIA_SUCCESS;
        }
		
      status = xiaGetLine(fp, line);
	  }
	
  return XIA_FILE_RA;
}


/** @brief Writes the interface portion of the module configuration to the
 * .ini file.
 *
 */
static int writeInterface(FILE *fp, Module *module)
{
  int i;
  int status;


  ASSERT(fp != NULL);
  ASSERT(module != NULL);


  for (i = 0; i < N_ELEMS(INTERFACE_WRITERS); i++) {
    if (module->interface_info->type == INTERFACE_WRITERS[i].type) {
	  
      status = INTERFACE_WRITERS[i].fn(fp, module);

      if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error writing interface data for type '%u'",
                module->interface_info->type);
        xiaLogError("writeInterface", info_string, status);
        return status;
      }

      return XIA_SUCCESS;
    }
  }

  sprintf(info_string, "Unknown interface type: '%u'",
          module->interface_info->type);
  xiaLogError("writeInterface", info_string, XIA_BAD_INTERFACE);
  return XIA_BAD_INTERFACE;
}


/** @brief Writes the PLX interface info to the passed in file pointer.
 *
 * Assumes that the file has been advanced to the proper location. Also assumes
 * that the module is using the PLX communication interface.
 *
 */
static int writePLX(FILE *fp, Module *module)
{
  ASSERT(fp != NULL);
  ASSERT(module != NULL);


  fprintf(fp, "interface = pxi\n");
  fprintf(fp, "pci_bus = %u\n", module->interface_info->info.plx->bus);
  fprintf(fp, "pci_slot = %u\n", module->interface_info->info.plx->slot);

  return XIA_SUCCESS;
}


/** @brief Writes the EPP interface info to the passed in file pointer.
 *
 */
static int writeEPP(FILE *fp, Module *m)
{
  ASSERT(fp != NULL);
  ASSERT(m != NULL);

  
  fprintf(fp, "interface = epp\n");
  fprintf(fp, "epp_address = %#x\n", m->interface_info->info.epp->epp_address);
  fprintf(fp, "daisy_chain_id = %u\n",
          m->interface_info->info.epp->daisy_chain_id);

  return XIA_SUCCESS;
}


/** @brief Writes the Jorway 73A interface info to the passed in file pointer.
 *
 */
static int writeJ73A(FILE *fp, Module *m)
{
  ASSERT(fp != NULL);
  ASSERT(m != NULL);

  
  fprintf(fp, "interface = j73a\n");
  fprintf(fp, "scsibus_number = %u\n",
          m->interface_info->info.jorway73a->scsi_bus);
  fprintf(fp, "crate_number = %u\n",
          m->interface_info->info.jorway73a->crate_number);
  fprintf(fp, "slot = %u\n", m->interface_info->info.jorway73a->slot);

  return XIA_SUCCESS;
}


/** @brief Writes the USB interface info to the passed in file pointer.
 *
 */
static int writeUSB(FILE *fp, Module *m)
{
  ASSERT(fp != NULL);
  ASSERT(m != NULL);


  fprintf(fp, "interface = usb\n");
  fprintf(fp, "device_number = %u\n", m->interface_info->info.usb->device_number);

  return XIA_SUCCESS;
}


#ifndef EXCLUDE_USB2
static int writeUSB2(FILE *fp, Module *m)
{
  ASSERT(fp != NULL);
  ASSERT(m != NULL);
  ASSERT(m->interface_info->type == USB2);


  fprintf(fp, "interface = usb2\n");
  fprintf(fp, "device_number = %u\n",
          m->interface_info->info.usb->device_number);

  return XIA_SUCCESS;
}
#endif /* EXCLUDE_USB2 */
