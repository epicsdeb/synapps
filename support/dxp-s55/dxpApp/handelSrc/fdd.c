/*
 * fdd.c
 *
 *   Created        15-Dec-2001  by John Wahl
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
 *   This file contains routine to extract information from a Firmware
 *   Definition Database (FDD).  The library will create temporary files
 *   for a xerxes or handel application to use.  The library will also
 *   allow inclusion of new files in the database.
 *
 */


#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>

#include "handel_errors.h"
#include "xia_fdd.h"

#include "xia_common.h"
#include "xia_assert.h"
#include "xia_file.h"

FDD_STATIC void fdd__StringChomp(char *str);

static char info_string[INFO_LEN],line[LINE_LEN],*token,*delim=" ,=\t\r\n";

static char *section = "$$$NEW SECTION$$$\n";


/******************************************************************************
 *
 * Global initialization routine.  Should be called before performing get and/or
 * put routines to the database.
 *
 ******************************************************************************/
FDD_EXPORT int FDD_API xiaFddInitialize()
{
  int status=XIA_SUCCESS;

  /* First thing is to load the utils structure.  This is needed
   * to call allocation of any memory in general as well as any
   * error reporting.  It is absolutely essential that this work
   * with no errors. 
   */

  /* Initialize the data structures and pointers of the library */
  if ((status=xiaFddInitLibrary())!=XIA_SUCCESS) {
    sprintf(info_string,"Unable to initialize FDD");
    printf("%s\n",info_string);
    return status;
  }

  return status;
}

/******************************************************************************
 *
 * Global initialization routine.  Should be called before performing get and/or
 * put routines to the database.
 *
 ******************************************************************************/
FDD_EXPORT int FDD_API xiaFddInitLibrary()
{
  int status=XIA_SUCCESS;

  Xia_Util_Functions util_funcs;

  /* Call the MD layer init function for the utility routines */
  dxp_md_init_util(&util_funcs, NULL);
  /* Now build up some information...slowly */
  fdd_md_alloc = util_funcs.dxp_md_alloc;
  fdd_md_free  = util_funcs.dxp_md_free;
  fdd_md_error = util_funcs.dxp_md_error;
  fdd_md_warning = util_funcs.dxp_md_warning;
  fdd_md_info  = util_funcs.dxp_md_info;
  fdd_md_debug = util_funcs.dxp_md_debug;
  fdd_md_output = util_funcs.dxp_md_output;
  fdd_md_log  = util_funcs.dxp_md_log;
  fdd_md_wait  = util_funcs.dxp_md_wait;
  fdd_md_puts  = util_funcs.dxp_md_puts;
  fdd_md_fgets  = util_funcs.dxp_md_fgets;
  fdd_md_path_separator = util_funcs.dxp_md_path_separator;

  return status;
}

/******************************************************************************
 *
 * Routine to create a file(s) of firmware based on criteria specified by the
 * calling routine.
 *
 * currently recognized firmware types:
 *    fippi
 *    dsp
 *    system
 *
 ******************************************************************************/
FDD_EXPORT int FDD_API xiaFddGetFirmware(const char *filename, char *path,
                                         const char *ftype,
                                         double pt, unsigned int nother,
                                         const char **others,
                                         const char *detectorType,
                                         char newfilename[],
                                         char rawFilename[])
/* const char *filename;     Input: name of the file that is the fdd        */
/* const char *ftype;      Input: firmware type to retrieve           */
/* unsigned short nother;     Input: number of elements in the array of other specifiers */
/* const char **others;      Input: array of stings containing firmware options    */
/* const char *detectorType    Input: detector type to be added to the keywords list       */
/* char newfilename[MAXFILENAME_LEN] Output: filename of the temporary firmware file     */
/* char rawFilename[MACFILENAME_LEN]   Output: filename for the raw filename. For ID purposes in PSL */
{
  int status=XIA_SUCCESS;
  int len;
  int completePathLen = 0;

  unsigned int i;

  unsigned short numFilter;
  unsigned short j;

  /* Store the file pointer of the FDD file and the new temporary file */
  FILE *fp=NULL, *ofp=NULL;

  boolean_t exact   = FALSE_;
  boolean_t isFound = FALSE_;

  char relativeName[MAXFILENAME_LEN];
  char postTok[MAXFILENAME_LEN];

  char *cstatus      = NULL;
  char *start        = NULL;
  char *pathSep      = fdd_md_path_separator();
  char *completePath = NULL;

  char **keywords = NULL;


  if (path == NULL) {
    sprintf(info_string, "Temporary path may not be NULL for '%s'",
            filename);
    xiaFddLogError("xiaFddGetFirmware", info_string, XIA_NULL_PATH);
    return XIA_NULL_PATH;
  }

  /* Add the detector type to the keywords list here */
  keywords = (char **)fdd_md_alloc((nother + 1) * sizeof(char *));
  for (i = 0; i < nother; i++) {
    len = strlen(others[i]);
    keywords[i] = (char *)fdd_md_alloc((len + 1) * sizeof(char));
    strcpy(keywords[i], others[i]);
  }

  len = strlen(detectorType);
  keywords[nother] = (char *)fdd_md_alloc((len + 1) * sizeof(char));
  strcpy(keywords[nother], detectorType);


  /* First find and open the FDD file */
  isFound = xiaFddFindFirmware(filename, ftype, pt, -1.0, (unsigned short) (nother + 1),
                               keywords, "r", &fp, &exact, rawFilename);

  for (i = 0; i < (nother + 1); i++) {
    fdd_md_free((void *)keywords[i]);
  }

  fdd_md_free((void *)keywords);

  if (!isFound)  {
    sprintf(info_string,"Cannot find '%s' in '%s': pt = %f, det = '%s'",
            ftype, filename, pt, detectorType);
    xiaFddLogDebug("xiaFddGetFirmware", info_string);            

    if (fp != NULL) {
      xia_file_close(fp);
    }

    return XIA_FILEERR;
  }

  /* Manipulate the rawFilename and just rip off the last name...
   * DANGER! DANGER! This only works for Windows. Must add some 
   * sort of global to deal with OS-dependent separators...
   */
  start = strrchr(rawFilename, '\\');
  start++;
  strcpy(postTok, start);
  start = strtok(postTok, " \n");
  strcpy(relativeName, start);

  sprintf(info_string, "relativeName = %s", relativeName);
  xiaFddLogDebug("xiaFddGetFirmware", info_string);


  /* Located the location in the FDD file, now write out the temporary file */
  /* Open the new file */
  sprintf(newfilename, "xia%s", relativeName);

  sprintf(info_string, "newfilename = %s", newfilename);
  xiaFddLogDebug("xiaFddGetFirmware", info_string);

  /* Build a path to the temporary file and directory */
  completePathLen = strlen(path) + strlen(newfilename) + 1;

  if (path[strlen(path) - 1] != *pathSep) {
    completePathLen++;
  }

  /* This is a bit of a leaky abstraction. We want to encourage callers to
   * make newfilename as big as MAX_PATH_LEN.
   */
  ASSERT(completePathLen < MAX_PATH_LEN);

  completePath = fdd_md_alloc(completePathLen);

  if (!completePath) {
    sprintf(info_string, "Error allocating %d bytes for 'completePath'",
            completePathLen);
    xiaFddLogError("xiaFddGetFirmware", info_string, XIA_NOMEM);
    xia_file_close(fp);
    return XIA_NOMEM;
  }

  strcpy(completePath, path);

  if (path[strlen(path) - 1] != *pathSep) {
    completePath = strcat(completePath, pathSep);
  }

  completePath = strcat(completePath, newfilename);

  strcpy(newfilename, completePath);

  ofp = xia_file_open(completePath, "w");

  if (ofp == NULL) {
    sprintf(info_string,"Error opening the temporary file: %s", completePath);
    xiaFddLogError("xiaFddGetFirmware", info_string, XIA_OPEN_FILE);
    xia_file_close(fp);
    fdd_md_free(completePath);
    return XIA_OPEN_FILE;
  }

  fdd_md_free(completePath);

  /* Need to skip past filter info */
  cstatus = fdd_md_fgets(line, LINE_LEN, fp);
  numFilter = (unsigned short)strtol(line, NULL, 10);

  sprintf(info_string, "numFilter = %u", numFilter);
  xiaFddLogDebug("xiaFddGetFirmware", info_string);

  for (j = 0; j < numFilter; j++) {
    cstatus = fdd_md_fgets(line, LINE_LEN, fp);
  }


  cstatus = fdd_md_fgets(line, LINE_LEN, fp);
  while ((!STREQ(line, section)) && (cstatus!=NULL)) {
    fprintf(ofp, "%s", line);
    cstatus = fdd_md_fgets(line, LINE_LEN, fp);
  }

  xia_file_close(ofp);
  xia_file_close(fp);

  return status;
}


/** @brief Find the requested firmware in the specified FDD file.
 *
 */
FDD_STATIC boolean_t xiaFddFindFirmware(const char *filename, const char *ftype,
                                        double ptmin, double ptmax,
                                        unsigned int nother, char **others,
                                        const char *mode, FILE **fp,
                                        boolean_t *exact,
                                        char rawFilename[MAXFILENAME_LEN])
/* Returns TRUE_ if the file was found FALSE_ otherwise                */
/* const char *filename;     Input: name of the file that is the fdd        */
/* const char *ftype;      Input: firmware type to retrieve           */
/* double ptmin;        Input: min peaking time for this firmware       */
/* double ptmax;        Input: max peaking time for this firmware       */
/* unsigned short nother;     Input: number of elements in the array of other specifiers */
/* const char **others;      Input: array of stings contianing firmware options    */
/* const char *mode;       Input: what mode to open the FDD file        */
/* FILE **fp;         Output: pointer to the file, if found        */
/* boolean_t *exact;       Output: was this an exact match to the types?     */
{

  char newFile[MAXFILENAME_LEN];

  char *cstatus = NULL;

  boolean_t found = FALSE_;

  int status;

  unsigned int i;
  unsigned int j;
  unsigned int nmatch;

  /* local variable used to decrement the number of type matches */
  unsigned short nkey;

  /* Min and max ptime read from fdd */
  double fddmin;
  double fddmax;


  ASSERT(filename != NULL);



  *fp = dxp_find_file(filename, mode, newFile);

  if (!*fp) {
    sprintf(info_string,"Error finding the FDD file: %s", filename);
    xiaFddLogError("xiaFddFindFirmware", info_string, XIA_OPEN_FILE);
    return FALSE_;
  }

  rewind(*fp);

  /* Skip past anything that doesn't equal the first section line. */
  do {
    cstatus = fdd_md_fgets(line, LINE_LEN, *fp);
  } while (!STRNEQ(line, section));

  /* Skip over the original filename entry */
  cstatus = fdd_md_fgets(rawFilename, LINE_LEN, *fp);

  fdd__StringChomp(rawFilename);

  sprintf(info_string, "rawFilename = %s", rawFilename);
  xiaFddLogDebug("xiaFddFindFirmware", info_string);

  /* Located the FDD file, now start the search */
  cstatus = fdd_md_fgets(line, LINE_LEN, *fp);

  while ((!found)&&(cstatus!=NULL)) {
    /* Second line contains the firmware type, strip off delimiters */
    token = strtok(line, delim);

    if (STREQ(token, ftype)) {
      sprintf(info_string, "Matched token: '%s'", token);
      xiaFddLogDebug("xiaFddFindFirmware", info_string);

      /* Read in the number of keywords */
      cstatus = fdd_md_fgets(line, LINE_LEN, *fp);
      nkey = (unsigned short) strtol(line, NULL, 10);

      /* Reset the number of matches */
      nmatch = nother;
      for (i=0; i < nkey; i++) {
        /* Read in the keywords and check for matches */
        cstatus = fdd_md_fgets(line, LINE_LEN, *fp);
        /* Strip off spaces and endline characters */
        token = strtok(line, delim);
        for (j = 0; j < nother; j++) {
          if STREQ(token,others[j]) {
            nmatch--;
            break;
          }
        }
        if (nmatch==0) {
          found = TRUE_;
          if (nkey == nother) *exact = TRUE_;
          break;
        }
      }
      /* case of no other keywords */
      if (nkey == 0) {
        found = TRUE_;
        *exact = TRUE_;
      }
    } else {
      /* Read in the number of keywords */
      cstatus = fdd_md_fgets(line, LINE_LEN, *fp);
      nkey = (unsigned short) strtol(line, NULL, 10);
      /* Step past the other keywords line */
      for (i=0; i < nkey; i++) {
        cstatus = fdd_md_fgets(line, LINE_LEN, *fp);
      }
    }
    /* If we found a keyword match, proceed to check the peaking times */
    if (found) {
      /* Reset found to false for the next comparison */
      found = FALSE_;
      /* Found the type, now match the peaking time, read min and max from fdd */
      cstatus = fdd_md_fgets(line, LINE_LEN, *fp);
      fddmin = strtod(line, NULL);
      cstatus = fdd_md_fgets(line, LINE_LEN, *fp);
      fddmax = strtod(line, NULL);
      if (ptmax < 0.0) {
        /* Case where we are locating a firmware file for download, only need to match the range */
        if ((ptmin > fddmin) && (ptmin <= fddmax)) {
          found = TRUE_;
        }
      } else {
        /* check for overlap of the peaking time ranges */
        if (!((ptmin >= fddmax) || (ptmax <= fddmin))) {
          /* Overlap with the current entry */
          found = TRUE_;
          status = XIA_UNKNOWN;
          sprintf(info_string,"Peaking time and keyword overlap with member of FDD");
          xiaFddLogError("xiaFddFindFirmware",info_string,status);
          xia_file_close(*fp);
          return found;
        }
      }
    }
    /* If no match, jump to the next section */
    while ((!found)&&(cstatus!=NULL)) {
      cstatus = fdd_md_fgets(line, LINE_LEN, *fp);
      if (STREQ(line,section)) {
        cstatus = fdd_md_fgets(rawFilename, LINE_LEN, *fp);

        fdd__StringChomp(rawFilename);

        sprintf(info_string, "rawFilename = %s", rawFilename);
        xiaFddLogDebug("xiaFddFindFirmware", info_string);

        cstatus = fdd_md_fgets(line, LINE_LEN, *fp);
        break;
      }
    }
  }

  return found;
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
static FILE* dxp_find_file(const char* filename, const char* mode, char newFile[MAXFILENAME_LEN])
/* const char *filename;   Input: filename to open   */
/* const char *mode;    Input: Mode to use when opening */
/* char *newFile;     Output: Full filename of file (translated env vars) */
{
  FILE *fp=NULL;
  char *name=NULL, *name2=NULL;
  char *home=NULL;

  unsigned int len = 0;


  /* Try to open file directly */
  if ((fp=xia_file_open(filename,mode))!=NULL) {
    len = MAXFILENAME_LEN>(strlen(filename)+1) ? strlen(filename) : MAXFILENAME_LEN;
    strncpy(newFile, filename, len);
    newFile[len] = '\0';
    return fp;
  }
  /* Try to open the file with the path XIAHOME */
  if ((home=getenv("XIAHOME"))!=NULL) {
    name = (char *) fdd_md_alloc(sizeof(char)*
                                 (strlen(home)+strlen(filename)+2));
    sprintf(name, "%s/%s", home, filename);
    if ((fp=xia_file_open(name,mode))!=NULL) {
      len = MAXFILENAME_LEN>(strlen(name)+1) ? strlen(name) : MAXFILENAME_LEN;
      strncpy(newFile, name, len);
      newFile[len] = '\0';
      fdd_md_free(name);
      return fp;
    }
    fdd_md_free(name);
    name = NULL;
  }
  /* Try to open the file with the path DXPHOME */
  if ((home=getenv("DXPHOME"))!=NULL) {
    name = (char *) fdd_md_alloc(sizeof(char)*
                                 (strlen(home)+strlen(filename)+2));
    sprintf(name, "%s/%s", home, filename);
    if ((fp=xia_file_open(name,mode))!=NULL) {
      len = MAXFILENAME_LEN>(strlen(name)+1) ? strlen(name) : MAXFILENAME_LEN;
      strncpy(newFile, name, len);
      newFile[len] = '\0';
      fdd_md_free(name);
      return fp;
    }
    fdd_md_free(name);
    name = NULL;
  }
  /* Try to open the file as an environment variable */
  if ((name=getenv(filename))!=NULL) {
    if ((fp=xia_file_open(name,mode))!=NULL) {
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

      name = (char *) fdd_md_alloc(sizeof(char)*
                                   (strlen(home)+strlen(name2)+2));
      sprintf(name, "%s/%s", home, name2);
      if ((fp=xia_file_open(name,mode))!=NULL) {
        len = MAXFILENAME_LEN>(strlen(name)+1) ? strlen(name) : MAXFILENAME_LEN;
        strncpy(newFile, name, len);
        newFile[len] = '\0';
        fdd_md_free(name);
        return fp;
      }
      fdd_md_free(name);
      name = NULL;
    }
  }
  /* Try to open the file with the path DXPHOME and pointing
   * to a file as an environment variable */
  if ((home=getenv("DXPHOME"))!=NULL) {
    if ((name2=getenv(filename))!=NULL) {

      name = (char *) fdd_md_alloc(sizeof(char)*
                                   (strlen(home)+strlen(name2)+2));
      sprintf(name, "%s/%s", home, name2);
      if ((fp=xia_file_open(name,mode))!=NULL) {
        len = MAXFILENAME_LEN>(strlen(name)+1) ? strlen(name) : MAXFILENAME_LEN;
        strncpy(newFile, name, len);
        newFile[len] = '\0';
        fdd_md_free(name);
        return fp;
      }
      fdd_md_free(name);
      name = NULL;
    }
  }

  return NULL;
}


/******************************************************************************
 *
 * Routine to add a firmware file to a FDD.
 *
 * currently recognized firmware types:
 *    fippi
 *    dsp
 *    system
 *
 ******************************************************************************/
FDD_EXPORT int FDD_API xiaFddAddFirmware(const char *filename, const char *ftype,
                                         double ptmin, double ptmax,
                                         unsigned short nother, char **others,
                                         const char *ffile, unsigned short numFilter,
                                         parameter_t *filterInfo)
/* const char *filename;     Input: name of the file that is the fdd        */
/* const char *ftype;      Input: firmware type to retrieve           */
/* double ptmin;        Input: min peaking time for this firmware       */
/* double ptmax;        Input: max peaking time for this firmware       */
/* unsigned short nother;     Input: number of elements in the array of other specifiers */
/* const char **others;      Input: array of stings contianing firmware options    */
/* char *ffile;        Input: filename of the firmware file to be added    */
{
  int status=XIA_SUCCESS;

  unsigned int i;

  unsigned short j;

  char newFile[MAXFILENAME_LEN];
  char rawFilename[MAXFILENAME_LEN];

  /* Store the file pointer of the FDD file and the new temporary file */
  FILE *fp=NULL, *ofp=NULL;

  boolean_t found, exact = FALSE_;

  char *cstatus=NULL;

  /* First find and open the FDD file */
  found = xiaFddFindFirmware(filename, ftype, ptmin, ptmax, nother, others, "a+", &fp, &exact, rawFilename);

  if (found && exact) {
    status = XIA_UNKNOWN;
    sprintf(info_string,"This firmware definition already exists");
    xiaFddLogError("xiaFddAddFirmware",info_string,status);
    xia_file_close(fp);
    return status;
  }

  /* Move the file pointer back to the end of the file */
  fseek(fp, 0, SEEK_END);

  /* Find the firmware file */
  /* Check that the filename is not NULL */
  if (ffile==NULL) {
    status = XIA_FILEERR;
    sprintf(info_string,"Must specify a firmware filename, NULL request is not valid.");
    xiaFddLogError("xiaFddAddFirmware",info_string,status);
    xia_file_close(fp);
    return status;
  }

  /* Now open the firmware file */
  if ((ofp = dxp_find_file(ffile,"r",newFile))==NULL) {
    status = XIA_OPEN_FILE;
    sprintf(info_string,"Error finding the FDD file: %s.",ffile);
    xiaFddLogError("xiaFddAddFirmware",info_string,status);
    xia_file_close(fp);
    return status;
  }

  /* Write the length of the firmware file */
  fprintf(fp, section);

  /* Write the original filename */
  fprintf(fp, "%s\n", ffile);

  /* Write the firmware type */
  fprintf(fp, "%s\n", ftype);

  /* Write the other types */
  fprintf(fp, "%d\n", nother);
  for (i=0; i < nother; i++) {
    fprintf(fp, "%s\n",others[i]);
  }

  /* Write out the Ptmin and Ptmax */
  fprintf(fp, "%10.4f\n", ptmin);
  fprintf(fp, "%10.4f\n", ptmax);

  /* Write out filter info */
  fprintf(fp, "%hu\n", numFilter);
  for (j = 0; j < numFilter; j++) {

    fprintf(fp, "%hu\n", filterInfo[j]);
  }

  rewind(ofp);
  cstatus = fdd_md_fgets(line, LINE_LEN, ofp);
  while (cstatus!=NULL) {

    cstatus = fdd_md_fgets(line, LINE_LEN, ofp);
  }

  /* Now loop over the firmware file, copying */
  rewind(ofp);
  cstatus = fdd_md_fgets(line, LINE_LEN, ofp);
  while (cstatus!=NULL) {
    fprintf(fp, "%s", line);
    cstatus = fdd_md_fgets(line, LINE_LEN, ofp);
  }

  xia_file_close(ofp);
  xia_file_close(fp);

  return status;
}

/******************************************************************************
 *
 * Routine to delete a temporary file.
 *
 ******************************************************************************/
FDD_EXPORT int FDD_API xiaFddCleanFirmware(const char *filename)
/* char *ffile;        Input: filename of the firmware file to be deleted */
{
  int status = XIA_SUCCESS;

  if (remove(filename) != 0) {
    status = XIA_FILEERR;
    sprintf(info_string,"Error removing the file: %s",filename);
    xiaFddLogError("xiaFddCleanFirmware",info_string,status);
    return status;
  }

  return status;
}


/*****************************************************************************
 *
 * Returns the number of filter parameters that are present for a given
 * peaking time and keywords.
 *
 *****************************************************************************/
FDD_EXPORT int FDD_API xiaFddGetNumFilter(const char *filename,
                                          double peakingTime,
                                          unsigned int nKey,
                                          const char **keywords,
                                          unsigned short *numFilter)
{
  int status;

  unsigned int numToMatch;

  unsigned short numKeys;
  unsigned short i;
  unsigned short j;

  double ptMin;
  double ptMax;

  char *cstatus = NULL;

  char newFile[MAXFILENAME_LEN];

  boolean_t isFound = FALSE_;

  FILE *fp = NULL;


  if (filename == NULL) {
    status = XIA_FILEERR;
    xiaFddLogError("xiaFddGetNumFilter", "Must specify a non-NULL FDD filename", status);
    return status;
  }

  /* Open the file */
  fp = dxp_find_file(filename, "r", newFile);
  if (fp == NULL) {
    status = XIA_OPEN_FILE;
    sprintf(info_string, "Error finding the FDD file: %s", filename);
    xiaFddLogError("xiaFddGetNumFilter", info_string, status);
    return status;
  }


  /* Start the hard-parsing here. Wish that we could make this into a
   * little more robust of an engine so that multiple FDD routines 
   * could share it...
   */

  /* 1) Read in $$$NEW SECTION$$$ line */
  cstatus = fdd_md_fgets(line, LINE_LEN, fp);

  /* 2) Read in raw filename */
  cstatus = fdd_md_fgets(line, LINE_LEN, fp);

  /* 3) Read in type */
  cstatus = fdd_md_fgets(line, LINE_LEN, fp);

  /* If this file is not of type "fippi", then we can fo ahead and skip ahead to
   * the next $$$NEW SECTION$$$. Do this until we match the proper peaking time
   * and keywords.
   */
  while ((!isFound) &&
         (cstatus != NULL)) {
    token = strtok(line, delim);

    if (STREQ(token, "fippi") || STREQ(token, "fippi_a")) {
      sprintf(info_string, "token = %s", token);
      xiaFddLogDebug("xiaFddGetNumFilter", info_string);

      cstatus = fdd_md_fgets(line, LINE_LEN, fp);

      numKeys = (unsigned short)strtol(line, NULL, 10);

      sprintf(info_string, "numKeys = %u", numKeys);
      xiaFddLogDebug("xiaFddGetNumFilter", info_string);

      numToMatch = nKey;
      for (i = 0; i < numKeys; i++) {
        cstatus = fdd_md_fgets(line, LINE_LEN, fp);

        token = strtok(line, delim);
        for (j = 0; j < nKey; j++) {
          if (STREQ(token, keywords[j])) {
            xiaFddLogDebug("xiaFddGetNumFilter", "Matched a keyword");

            numToMatch--;
            break;
          }
        }

        if (numToMatch == 0) {
          xiaFddLogDebug("xiaFddGetNumFilter", "Matched all keywords");

          isFound = TRUE_;
          break;
        }
      }

      /* In case there were no keywords in the first place...Is this possible? */
      if (nKey == 0) {
        isFound = TRUE_;
      }

    } else {

      /* Skip over keywords here since this isn't a "fippi" and we don't really
       * care about it. Remember that the line position (in this branch only)
       * is still sitting right after the type variable...
       */
      cstatus = fdd_md_fgets(line, LINE_LEN, fp);

      numKeys = (unsigned short)strtol(line, NULL, 10);
      for (i = 0; i < numKeys; i++) {
        cstatus = fdd_md_fgets(line, LINE_LEN, fp);
      }
    }

    if (isFound) {
      isFound = FALSE_;

      cstatus = fdd_md_fgets(line, LINE_LEN, fp);
      ptMin = strtod(line, NULL);
      cstatus = fdd_md_fgets(line, LINE_LEN, fp);
      ptMax = strtod(line, NULL);

      sprintf(info_string, "ptMin = %.3f, ptMax = %.3f", ptMin, ptMax);
      xiaFddLogDebug("xiaFddGetNumFilter", info_string);

      if ((peakingTime > ptMin) &&
          (peakingTime <= ptMax)) {
        isFound = TRUE_;

        cstatus = fdd_md_fgets(line, LINE_LEN, fp);
        *numFilter = (unsigned short)strtol(line, NULL, 10);

        sprintf(info_string, "numFilter = %u\n", *numFilter);
        xiaFddLogDebug("xiaFddGetNumFilter", info_string);

      }
    }

    while ((!isFound) &&
           (cstatus != NULL)) {
      cstatus = fdd_md_fgets(line, LINE_LEN, fp);
      if (STREQ(line, section)) {
        /* This should be the raw filename */
        cstatus = fdd_md_fgets(line, LINE_LEN, fp);

        /* This should be the type...so start all over again */
        cstatus = fdd_md_fgets(line, LINE_LEN, fp);
        break;
      }
    }
  }

  xia_file_close(fp);

  return XIA_SUCCESS;
}


/**
 * This routine returns a list of values for the filters in filterInfo.
 * It is the responsibility of the calling routine to allocate the
 * right amount of memory for filterInfo, preferably using the size
 * returned from xiaFddGetNumFilter().
 */
FDD_EXPORT int FDD_API xiaFddGetFilterInfo(const char *filename, double peakingTime, unsigned int nKey,
                                           const char **keywords, parameter_t *filterInfo)
{
  int status;

  unsigned int numToMatch;

  unsigned short numKeys;
  unsigned short i;
  unsigned short j;
  unsigned short k;
  unsigned short numFilter;

  double ptMin;
  double ptMax;

  char *cstatus = NULL;

  char newFile[MAXFILENAME_LEN];

  boolean_t isFound = FALSE_;

  FILE *fp = NULL;


  if (filename == NULL) {

    status = XIA_FILEERR;
    xiaFddLogError("xiaFddGetFilterInfo", "Must specify a non-NULL FDD filename", status);
    return status;
  }

  fp = dxp_find_file(filename, "r", newFile);

  if (fp == NULL) {

    status = XIA_OPEN_FILE;
    sprintf(info_string, "Error finding the FDD file: %s", filename);
    xiaFddLogError("xiaFddGetFilterInfo", info_string, status);
    return status;
  }

  /* Start the hard-parsing here. Wish that we could make this into a
   * little more robust of an engine so that multiple FDD routines 
   * could share it...
   */

  /* 1) Read in $$$NEW SECTION$$$ line */
  cstatus = fdd_md_fgets(line, LINE_LEN, fp);

  /* 2) Read in raw filename */
  cstatus = fdd_md_fgets(line, LINE_LEN, fp);

  /* 3) Read in type */
  cstatus = fdd_md_fgets(line, LINE_LEN, fp);

  /* If this file is not of type "fippi", then we can fo ahead and skip ahead to
   * the next $$$NEW SECTION$$$. Do this until we match the proper peaking time
   * and keywords.
   */
  while ((!isFound) &&
         (cstatus != NULL)) {
    token = strtok(line, delim);

    if (STREQ(token, "fippi") || STREQ(token, "fippi_a")) {
      sprintf(info_string, "token = %s", token);
      xiaFddLogDebug("xiaFddGetFilterInfo", info_string);

      cstatus = fdd_md_fgets(line, LINE_LEN, fp);

      numKeys = (unsigned short)strtol(line, NULL, 10);

      sprintf(info_string, "numKeys = %u", numKeys);
      xiaFddLogDebug("xiaFddGetFilterInfo", info_string);

      numToMatch = nKey;
      for (i = 0; i < numKeys; i++) {
        cstatus = fdd_md_fgets(line, LINE_LEN, fp);

        token = strtok(line, delim);
        for (j = 0; j < nKey; j++) {
          if (STREQ(token, keywords[j])) {
            xiaFddLogDebug("xiaFddGetFilterInfo", "Matched a keyword");

            numToMatch--;
            break;
          }
        }

        if (numToMatch == 0) {
          xiaFddLogDebug("xiaFddGetFilterInfo", "Matched all keywords");

          isFound = TRUE_;
          break;
        }
      }

      /* In case there were no keywords in the first place...Is this possible? */
      if (nKey == 0) {
        isFound = TRUE_;
      }

    } else {

      /* Skip over keywords here since this isn't a "fippi" and we don't really
       * care about it. Remember that the line position (in this branch only)
       * is still sitting right after the type variable...
       */
      cstatus = fdd_md_fgets(line, LINE_LEN, fp);

      numKeys = (unsigned short)strtol(line, NULL, 10);
      for (i = 0; i < numKeys; i++) {
        cstatus = fdd_md_fgets(line, LINE_LEN, fp);
      }
    }

    if (isFound) {
      isFound = FALSE_;

      cstatus = fdd_md_fgets(line, LINE_LEN, fp);
      ptMin = strtod(line, NULL);
      cstatus = fdd_md_fgets(line, LINE_LEN, fp);
      ptMax = strtod(line, NULL);

      sprintf(info_string, "ptMin = %.3f, ptMax = %.3f", ptMin, ptMax);
      xiaFddLogDebug("xiaFddGetFilterInfo", info_string);

      if ((peakingTime > ptMin) &&
          (peakingTime <= ptMax)) {
        isFound = TRUE_;

        cstatus = fdd_md_fgets(line, LINE_LEN, fp);
        numFilter = (unsigned short)strtol(line, NULL, 10);

        sprintf(info_string, "numFilter = %u\n", numFilter);
        xiaFddLogDebug("xiaFddGetFilterInfo", info_string);

        for (k = 0; k < numFilter; k++) {

          cstatus = fdd_md_fgets(line, LINE_LEN, fp);
          filterInfo[k] = (parameter_t)strtol(line, NULL, 10);
        }

      }
    }

    while ((!isFound) &&
           (cstatus != NULL)) {
      cstatus = fdd_md_fgets(line, LINE_LEN, fp);
      if (STREQ(line, section)) {
        /* This should be the raw filename */
        cstatus = fdd_md_fgets(line, LINE_LEN, fp);

        /* This should be the type...so start all over again */
        cstatus = fdd_md_fgets(line, LINE_LEN, fp);
        break;
      }
    }
  }

  xia_file_close(fp);

  return XIA_SUCCESS;
}


/** @brief Attempts to remove any of the standard combinations of EOL
 * characters in existence.
 *
 * Specifically, this routine will remove \n, \r\n and \r.
 *
 * Modifies @a str in place since the chomped length is guaranteed to be
 * less then or equal to the original length.
 */
FDD_STATIC void fdd__StringChomp(char *str)
{
  int len = 0;

  char *chomped = NULL;


  ASSERT(str != NULL);


  len = strlen(str);
  chomped = fdd_md_alloc(len + 1);
  ASSERT(chomped != NULL);

  strcpy(chomped, str);

  if (chomped[len - 1] == '\n') {
    chomped[len - 1] = '\0';

    if (chomped[len - 2] == '\r') {
      chomped[len - 2] = '\0';
    }

  } else if (chomped[len - 1] == '\r') {
    chomped[len - 1] = '\0';
  }

  strcpy(str, chomped);
  fdd_md_free(chomped);
}
