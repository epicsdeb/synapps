/*
 *  md_vxWorks.h
 *
 *
 * Copyright (c) 2004, X-ray Instrumentation Associates
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
 * Converted from md_win95.c by Mark Rivers, University of Chicago
 */


/* NOTE: md_vxWorks.c currently only supports the DXP4C2X on CAMAC.  
 * If other support is added then take the prototypes and starting code from md_win95.c
 */

/* Note: we replace dxp_md_log and dxp_md_output from md_log.c with versions specific to vxWorks
 * This is because on vxWorks stdout and stderr can change their values when the startup script ends,
 * and we must reassign them each time if they are being used. 
 */

/* System include files */
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <string.h>

/* EPICS include files */
#include <epicsThread.h>

/* XIA include files */
#include "xerxes_errors.h"
#include "xerxes_structures.h"
#include "md_vxWorks.h"
#include "md_generic.h"
#include "xia_md.h"
#include "xia_common.h"
#include "xia_assert.h"

#define MODE 4


/* total number of modules in the system */
static unsigned int numMod    = 0;

/* error string used as a place holder for calls to dxp_md_error() */
static char error_string[132];

/* are we in debug mode? */
static int print_debug=0;

/* maximum number of words able to transfer in a single call to dxp_md_io() */
static unsigned int maxblk=0;


#if !(defined(EXCLUDE_DXP4C2X))
#include <camacLib.h>
static unsigned int numDXP    = 0;
/* variables to store the IO channel information, later accessed via a pointer*/
static int *branch=NULL,*crate=NULL,*station=NULL;
#endif

typedef unsigned int	DWORD;

#define NORMAL_PRIORITY_CLASS 0
#define REALTIME_PRIORITY_CLASS -20
#define HIGH_PRIORITY_CLASS -10
#define ABOVE_NORMAL_PRIORITY_CLASS -5
#define BELOW_NORMAL_PRIORITY_CLASS 5

#define INFO_LEN	400

static char *TMP_PATH = ".";
static char *PATH_SEP = "/";
static int use_stdout = 0;
static int use_stderr = 0;

unsigned int GetCurrentProcess()		/* This only works for SetPriorityClass, not a true	*/
{					/* replacement for Microsoft Windows GetCurrentProcess.	*/
    return(0);
}

BOOL SetPriorityClass(unsigned int hProcess, DWORD dwPriorityClass)
{
    return(0);
}

/******************************************************************************
 *
 * Routine to create pointers to the MD utility routines
 * 
 ******************************************************************************/
XIA_MD_EXPORT int XIA_MD_API dxp_md_init_util(Xia_Util_Functions* funcs, char* type)
/* Xia_Util_Functions *funcs;	*/
/* char *type;					*/
{
    /* Check to see if we are intializing the library with this call in addition
     * to assigning function pointers. 
     */

    if (type != NULL) 
    {
	if (STREQ(type, "INIT_LIBRARY")) { numMod = 0; }
    }

    funcs->dxp_md_error_control = dxp_md_error_control;
    funcs->dxp_md_alloc         = dxp_md_alloc;
    funcs->dxp_md_free          = dxp_md_free;
    funcs->dxp_md_puts          = dxp_md_puts;
    funcs->dxp_md_wait          = dxp_md_wait;

    funcs->dxp_md_error         = dxp_md_error;
    funcs->dxp_md_warning       = dxp_md_warning;
    funcs->dxp_md_info          = dxp_md_info;
    funcs->dxp_md_debug         = dxp_md_debug;
    funcs->dxp_md_output        = dxp_md_vx_output;
    funcs->dxp_md_suppress_log  = dxp_md_suppress_log;
    funcs->dxp_md_enable_log    = dxp_md_enable_log;
    funcs->dxp_md_set_log_level = dxp_md_set_log_level;
    funcs->dxp_md_log	        = dxp_md_vx_log;
    funcs->dxp_md_set_priority  = dxp_md_set_priority;
    funcs->dxp_md_fgets         = dxp_md_fgets;
    funcs->dxp_md_tmp_path      = dxp_md_tmp_path;
    funcs->dxp_md_clear_tmp     = dxp_md_clear_tmp;
    funcs->dxp_md_path_separator = dxp_md_path_separator;

    if (out_stream == NULL)
    {
	out_stream = stdout;
        use_stdout = 1;
    }

    return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Routine to create pointers to the MD utility routines
 * 
 ******************************************************************************/
XIA_MD_EXPORT int XIA_MD_API dxp_md_init_io(Xia_Io_Functions* funcs, char* type)
/* Xia_Io_Functions *funcs;		*/
/* char *type;					*/
{
    unsigned int i;
	
    for (i = 0; i < strlen(type); i++) {

	type[i]= (char)tolower(type[i]);
    }


#if !(defined(EXCLUDE_DXP4C2X))
    if (STREQ(type, "camac")) {

	funcs->dxp_md_io         = dxp_md_io;
	funcs->dxp_md_initialize = dxp_md_initialize;
	funcs->dxp_md_open       = dxp_md_open;
	funcs->dxp_md_close      = dxp_md_close;
    } 
#endif

    funcs->dxp_md_get_maxblk = dxp_md_get_maxblk;
    funcs->dxp_md_set_maxblk = dxp_md_set_maxblk;
    funcs->dxp_md_lock_resource = dxp_md_lock_resource;


    return DXP_SUCCESS;
}

#if !(defined(EXCLUDE_DXP4C2X))
/*****************************************************************************
 * 
 * Initialize the system.  Alloocate the space for the library arrays, define
 * the pointer to the CAMAC library and the IO routine.
 * 
 *****************************************************************************/
static int dxp_md_initialize(unsigned int* maxMod, char* dllname)
/* unsigned int *maxMod;        Input: maximum number of dxp modules allowed */
/* char *dllname;               Input: name of the DLL                    */
{
  int status = DXP_SUCCESS;

/* Ensure that the library arrays do not exist */
  if (branch!=NULL) {
    dxp_md_free(branch);
  }
  if (crate!=NULL) {
    dxp_md_free(crate);
  }
  if (station!=NULL) {
    dxp_md_free(station);
  }

/* Allocate memory for the library arrays */
  branch  = (int *) dxp_md_alloc(*maxMod * sizeof(int));
  crate   = (int *) dxp_md_alloc(*maxMod * sizeof(int));
  station = (int *) dxp_md_alloc(*maxMod * sizeof(int));

/* check if all the memory was allocated */
  if ((branch==NULL)||(crate==NULL)||(station==NULL)) {
    status = DXP_NOMEM;
    dxp_md_log_error("dxp_md_initialize",
                 "Unable to allocate branch, crate and station memory", status);
    return status;
  }

/* Zero out the number of modules currently in the system */
  numDXP = 0;

  return status;
}
/*****************************************************************************
 * 
 * Routine is passed the user defined configuration string *name.  This string
 * contains all the information needed to point to the proper IO channel by 
 * future calls to dxp_md_io().  In the case of a simple CAMAC crate, the string 
 * should contain a branch number (representing the SCSI bus number in this case),
 * a crate number (single SCSI can control multiple crates) and a station 
 * number (better known as a slot number).
 * 
 *****************************************************************************/
static int dxp_md_open(char* name, int* camChan)
/* char *name;                        Input:  string used to specify this IO
                                                                     channel */
/* int *camChan;                      Output: returns a reference number for
                                                                  this module */
{
  int branch,crate,station,status;

/* pull out the branch, crate and station number and call the internal routine*/

  sscanf(name,"%1d%1d%2d",&branch,&crate,&station);
  status = dxp_md_open_bcn(&branch,&crate,&station,camChan);

  return status;
}

/*****************************************************************************
 *
 * Internal routine to assign a new pointer to this branch, crate and station
 * combination.  Information is stored in global static arrays.
 *
 *****************************************************************************/
static int dxp_md_open_bcn(int* new_branch, int* new_crate, int* new_station,
                                                   int* camChan)
/* int *new_branch;         Input: branch number                 */
/* int *new_crate;          Input: crate number                  */
/* int *new_station;        Input: station number                */
/* int *camChan;            Output: pointer to this bra and ch and crate */

{
  unsigned int i;
  int status;

/* First loop over the existing branch, crate and station numbers to make
 * sure this module was not already configured?  Don't want/need to perform
 * this operation 2 times. */

  for(i=0;i<numDXP;i++){
    if( (*new_branch==branch[i])&&
        (*new_crate==crate[i])&&
        (*new_station==station[i])){
/*                 sprintf(error_string,
                   " branch = %d  crate = %d   station = %d already defined",
                   *new_branch,*new_crate,*new_station);
                   dxp_md_puts("dxp_md_open",error_string,&status);*/
      status=DXP_SUCCESS;
      *camChan = i;
      return status;
    }
  }

/* Got a new one.  Increase the number existing and assign the global
 * information */

  branch[numDXP]=*new_branch;
  crate[numDXP]=*new_crate;
  station[numDXP]=*new_station;

  *camChan = numDXP++;


  status=DXP_SUCCESS;

  return status;
}

/*****************************************************************************
 * 
 * This routine performs the IO call to read or write data.  The pointer to 
 * the desired IO channel is passed as camChan.  The address to write to is
 * specified by function and address.  The data length is specified by 
 * length.  And the data itself is stored in data.
 * 
 *****************************************************************************/
static int dxp_md_io(int* camChan, unsigned int* function,
                                   unsigned long* address, void* data,
                                   unsigned int* length)
/* int *camChan;             Input: pointer to IO channel to access          */
/* unsigned int *function;   Input: function number to access (CAMAC F)      */
/* unsigned int *address;    Input: address to access        (CAMAC A)       */
/* unsigned short *data;     I/O:  data read or written                      */
/* unsigned int *length;     Input: how much data to read or write           */
{

  int ext;
  int cb[4];
  int status;
  int q;

/* First encode the transfer information in an array using
 * an SJY routine. */
  cdreg(&ext, branch[*camChan], crate[*camChan],
        station[*camChan], (int) *address);

  if (*length == 1) {
    cssa(*function, ext, (short *) data, &q);
    if (q==0) {
      status = DXP_MDIO;
      dxp_md_log_error("dxp_md_io","cssa transfer error: Q=0", status);
      return status;
    }
  } else {
    cb[0] = *length;
    cb[1] = 0;
    cb[2] = 0;
    cb[3] = 0;

    csubc(*function, ext, (short *) data, cb);

    if (cb[1]!=*length){
      status = DXP_MDIO;
      sprintf(error_string,
      "camac transfer error: bytes transferred %d vs expected %d",cb[1],cb[0]);
      dxp_md_log_error("dxp_md_io",error_string,status);
      return status;
    }
  }

  status=DXP_SUCCESS;
  return status;
}


/**********
 * This routine is used to "close" the CAMAC connection.
 * For CAMAC, nothing needs to be done...
 **********/
XIA_MD_STATIC int XIA_MD_API dxp_md_close(int *camChan)
{
    UNUSED(camChan);

    return DXP_SUCCESS;
}

#endif


/*****************************************************************************
 * 
 * Routine to control the error reporting operation.  Currently the only 
 * keyword available is "print_debug".  Then whenever dxp_md_error() is called
 * with an error_code=DXP_DEBUG, the message is printed.  
 * 
 *****************************************************************************/
XIA_MD_STATIC void XIA_MD_API dxp_md_error_control(char* keyword, int* value)
/* char *keyword;						Input: keyword to set for future use by dxp_md_error()	*/
/* int *value;							Input: value to set the keyword to					*/
{

    /* Enable debugging */

    if (strstr(keyword,"print_debug")!=NULL){
	print_debug=(*value);
	return;
    }

    /* Else we have a problem */

    dxp_md_puts("dxp_md_error_control: keyword %s not recognized..\n");
}

/*****************************************************************************
 * 
 * Routine to get the maximum number of words that can be block transfered at 
 * once.  This can change from system to system and from controller to 
 * controller.  A maxblk size of 0 means to write all data at once, regardless 
 * of transfer size.
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_get_maxblk(void)
{

    return maxblk;

}

/*****************************************************************************
 * 
 * Routine to set the maximum number of words that can be block transfered at 
 * once.  This can change from system to system and from controller to 
 * controller.  A maxblk size of 0 means to write all data at once, regardless 
 * of transfer size.
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_set_maxblk(unsigned int* blksiz)
/* unsigned int *blksiz;			*/
{
    int status; 

    if (*blksiz > 0) {
	maxblk = *blksiz;
    } else {
	status = DXP_NEGBLOCKSIZE;
	dxp_md_log_error("dxp_md_set_maxblk","Block size must be positive or zero",status);
	return status;
    }
    return DXP_SUCCESS;

}

/*****************************************************************************
 * 
 * Routine to wait a specified time in seconds.  This allows the user to call
 * routines that are as precise as required for the purpose at hand.
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_wait(float* time)
/* float *time;							Input: Time to wait in seconds	*/
{
    double dtime = *time;

#ifdef vxWorks
    /* On vxWorks there is no guarantee that epicsThreadSleep will sleep for at least the
     * requested time, there is only the guarantee that it will sleep until the next clock tick
     * To guarantee we add 2 clock ticks to the requested time */
    /* dtime += 2.*epicsThreadSleepQuantum(); */
    /* This seems to fix a problem with initializing the DXP-2X.  Need to track down! */
    if (dtime < 0.1) dtime = 0.1;
#endif

    epicsThreadSleep(dtime);

    return DXP_SUCCESS;
}

/*****************************************************************************
 * 
 * Routine to allocate memory.  The calling structure is the same as the 
 * ANSI C standard routine malloc().  
 * 
 *****************************************************************************/
XIA_MD_SHARED void* XIA_MD_API dxp_md_alloc(size_t length)
/* size_t length;							Input: length of the memory to allocate
   in units of size_t (defined to be a 
   byte on most systems) */
{

    return malloc(length);

}

/*****************************************************************************
 * 
 * Routine to free memory.  Same calling structure as the ANSI C routine 
 * free().
 * 
 *****************************************************************************/
XIA_MD_SHARED void XIA_MD_API dxp_md_free(void* array)
/* void *array;							Input: pointer to the memory to free */
{

    free(array);

}

/*****************************************************************************
 * 
 * Routine to print a string to the screen or log.  No direct calls to 
 * printf() are performed by XIA libraries.  All output is directed either
 * through the dxp_md_error() or dxp_md_puts() routines.
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_puts(char* s)
/* char *s;								Input: string to print or log	*/
{

    return printf("%s", s);

}

XIA_MD_STATIC int XIA_MD_API dxp_md_lock_resource(int *ioChan, int *modChan, short *lock)
{
  UNUSED(ioChan);
  UNUSED(modChan);
  UNUSED(lock);

  return DXP_SUCCESS;
}


/** @brief Safe version of fgets() that can handle both UNIX and DOS
 * line-endings.
 *
 * If the trailing two characters are '\r' + \n', they are replaced by a
 * single '\n'.
 */
XIA_MD_STATIC char * dxp_md_fgets(char *s, int length, FILE *stream)
{
  int buf_len = 0;

  char *buf     = NULL;
  char *cstatus = NULL;


  ASSERT(s != NULL);
  ASSERT(stream != NULL);
  ASSERT(length > 0);


  buf = dxp_md_alloc(length + 1);

  if (!buf) {
    return NULL;
  }

  cstatus = fgets(buf, (length + 1), stream);

  if (!cstatus) {
    dxp_md_free(buf);
    return NULL;
  }

  buf_len = strlen(buf);

  if ((buf[buf_len - 2] == '\r') && (buf[buf_len - 1] == '\n')) {
    buf[buf_len - 2] = '\n';
    buf[buf_len - 1] = '\0';
  }

  ASSERT(strlen(buf) < length);

  strcpy(s, buf);

  free(buf);

  return s;
}


/** @brief Get a safe temporary directory path.
 *
 */
XIA_MD_STATIC char * dxp_md_tmp_path(void)
{
  return TMP_PATH;
}


/** @brief Clears the temporary path cache.
 *
 */
XIA_MD_STATIC void dxp_md_clear_tmp(void)
{
  return;
}


/** @brief Returns the path separator
 *
 */
XIA_MD_STATIC char * dxp_md_path_separator(void)
{
  return PATH_SEP;
}


/** @brief Sets the priority of the current process 
 *
 */
XIA_MD_STATIC int XIA_MD_API dxp_md_set_priority(int *priority)
{
  BOOL status;

  DWORD pri;

  unsigned int h;

  char pri_str[5];


  h = GetCurrentProcess();

  switch(*priority) {
  
  default:
	sprintf(error_string, "Invalid priority type: %#x", *priority);
	dxp_md_log_error("dxp_md_set_priority", error_string, DXP_MDINVALIDPRIORITY);
	return DXP_MDINVALIDPRIORITY;
	break;

  case MD_IO_PRI_NORMAL:
	pri = NORMAL_PRIORITY_CLASS;
	strncpy(pri_str, "NORM", 5);
	break;

  case MD_IO_PRI_HIGH:
	pri = REALTIME_PRIORITY_CLASS;
	strncpy(pri_str, "HIGH", 5);
	break;
  }

  status = SetPriorityClass(h, pri);

  if (!status) {
	sprintf(error_string,
			"Error setting priority class (%s) for current process",
			pri_str);
	dxp_md_log_error("dxp_md_set_priority", error_string, DXP_MDPRIORITY);
	return DXP_MDPRIORITY;
  }

  sprintf(error_string, "Priority class set to '%s' for current process",
		  pri_str);
  dxp_md_log_debug("dxp_md_set_priority", error_string);

  return DXP_SUCCESS;
}

/*****************************************************************************
 *
 * This routine is the main logging routine. It shouldn't be called directly.
 * Use the macros provided in xerxes_generic.h.
 *
 *****************************************************************************/
XIA_MD_STATIC void XIA_MD_API dxp_md_vx_log(int level, char *routine, char *message, int error, char *file, int line)
/* int level;							Input: The level of this log message */
/* char *routine;						Input: Name of routine calling dxp_log*/
/* char *message;						Input: Log message to send to output */
/* int error;							Input: Only used if this is an ERROR */
{

  if (out_stream == NULL) {
    out_stream = stdout;
    use_stdout = 1;
  }
  if (use_stdout) out_stream = stdout;
  if (use_stderr) out_stream = stderr;
  
  /* Call the original function in md_log.c */
  dxp_md_log(level, routine, message, error, file, line);
}
		
/*****************************************************************************
 *
 * Routine to set the logging output to whatever FILE * the user would like.
 * By default, the output stream is set to stdout.
 *
 *****************************************************************************/
XIA_MD_STATIC void dxp_md_vx_output(char *filename)
/* char *filename;		Input: Name of the stream or file to redirect error output */
{
	int status;
	char *strtmp = NULL;
	unsigned int i;
	char info_string[INFO_LEN];
        
        use_stderr = 0;
        use_stdout = 0;

/* First close the currently opened stream, iff it is a file */
	if ((out_stream!=stdout) && (out_stream!=stderr)) {
/* close the stream */
		fclose(out_stream);
	}
/* change the input name to all lower case to check for predefined streams */
	strtmp = (char *) dxp_md_alloc((strlen(filename)+1) * sizeof(char));
	for (i=0;i<strlen(filename);i++) strtmp[i] = (char) tolower(filename[i]);
	strtmp[strlen(filename)]='\0';

/* if filename is stdin, then default to stdout */
	if (STREQ(strtmp,"stdin")) {
		dxp_md_log_warning("dxp_md_output", "Output filename can't be stdin; reset to stdout.");
		out_stream = stdout;
                use_stdout = 1;
		dxp_md_free(strtmp);
		return;
	}

/* Check if the filename is stdout, NULL or stderr */
	if ((STREQ(strtmp,"stdout")) || (filename==NULL) || (STREQ(filename, ""))) {
		out_stream = stdout;
                use_stdout = 1;
		dxp_md_free(strtmp);
		return;
	}
	if (STREQ(strtmp,"stderr")) {
		out_stream = stderr;
                use_stderr = 1;
		dxp_md_free(strtmp);
		return;
	}
/* The filename must be for a "real" file */
	out_stream = fopen(filename,"w");

	if (out_stream==NULL) {
		status = DXP_MDFILEIO;
		sprintf(info_string,"Unable to open filename: %s, no action performed",filename);
		dxp_md_log_error("dxp_md_output",info_string,status);
		dxp_md_free(strtmp);
		return;
	}
	dxp_md_free(strtmp);
	return;

}


