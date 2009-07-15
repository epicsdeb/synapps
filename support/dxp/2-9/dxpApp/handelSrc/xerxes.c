/*
 * xerxes.c
 *
 *   Created        25-Sep-1996  by Ed Oltman
 *   Extensive mods 19-Dec-1996  EO
 *   Modified:      03-Feb-1997  EO: Added "static" qualifier to variable defs
 *        Initialize some variables that may be undefined, add getenv to fopen
 *        for unix compatibility, replace '#include <dxp_area/...>' with
 *        '#include <...>' (unix compatability) -->compiler now requires
 *        path for include files; fixed bugs in dxp_initialize_ASC,
 *        dxp_assign_channel,dxp_det_to_elec(); replaced constants used in 
 *        dxp_initialize_ASC with parameters defined in dxp.h
 *    Modified      05-Feb-1997 EO: introduce FNAME replacement for VAX
 *        compatability (getenv crashes if logical not defined)
 *    Modified      17-Feb-1997 EO: fix bug in dxp_det_to_elec
 *    Modified      17-Jul-1997 EO: fix bug in dxp_initialize_ASC
 *    Modified      04-Oct-1997 EO: Make function declarations compatable
 *        compatable w/traditional C; Replace LITTLE_ENDIAN parameter with call
 *        to dxp_little_endian, fix bug in dxp_write_spectrua,Changed file 
 *        system:new file dxp_filename: Now get all file names from single list 
 *        of file names pointed to by "dxpfile_cfg"
 *    09-May-2000 JEW: Complete rework of library to allow use of both a
 *        DXP4C and DXP4C2X simultaneously.
 *
 *
 *   This file contains mid-level DXP control routines for handling a multi-
 *   channel array.  Most of these routines can be used as is or can serve as 
 *   examples for how the primitive routines are used. 
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
#include <stdlib.h>
#include <string.h> 
#include <ctype.h>
#include <limits.h>
#include <math.h> 

/* General DXP information */
#include "md_generic.h"
#include "xerxes_structures.h"
#include "xia_xerxes_structures.h"
#include "xia_xerxes.h"
#include "xerxes_errors.h"


#include "xia_assert.h"
#include "xia_version.h"
#include "xia_common.h"
#include "xia_file.h"


/* Private routines */
XERXES_STATIC int dxp_get_btype(char *name, Board_Info **current);
XERXES_STATIC int dxp_add_btype_library(Board_Info *current);
XERXES_STATIC int dxp_init_iface_ds(void);
XERXES_STATIC int dxp_init_dsp_ds(void);
XERXES_STATIC int dxp_init_fippi_ds(void);
XERXES_STATIC int dxp_init_defaults_ds(void);
XERXES_STATIC int dxp_init_system_fippi_ds(void);
XERXES_STATIC int dxp_read_modules(FILE *);
XERXES_STATIC int dxp_free_board(Board *board);
XERXES_STATIC int dxp_free_iface(Interface *iface);
XERXES_STATIC int dxp_free_dsp(Dsp_Info *dsp);
XERXES_STATIC int dxp_free_fippi(Fippi_Info *fippi);
XERXES_STATIC int dxp_free_params(Dsp_Params *params);
XERXES_STATIC int dxp_free_defaults(Dsp_Defaults *defaults);
XERXES_STATIC int dxp_free_binfo(Board_Info *binfo);
XERXES_STATIC int dxp_free_system_fippi(System_FiPPI_Info *fippi);
XERXES_STATIC int dxp_add_fippi(char *, Board_Info *, Fippi_Info **);
XERXES_STATIC int dxp_add_dsp(char *, Board_Info *, Dsp_Info **);
XERXES_STATIC int dxp_add_defaults(char *, Board_Info *, Dsp_Defaults **);
XERXES_STATIC int dxp_add_system_fippi(char *filename, Board_Info *b,
                                       System_FiPPI_Info **fippi);
XERXES_STATIC int dxp_add_iface(char *dllname, char *iolib, Interface **iface);
XERXES_STATIC int dxp_pick_filename(unsigned int, char *, char *);
XERXES_STATIC int dxp_strnstrm(char *, char *, unsigned int *, unsigned int *);
XERXES_STATIC int dxp_readout_run(Board *board, int *modChan,
                                  unsigned short params[],
                                  unsigned long baseline[],
                                  unsigned long spectrum[]);
XERXES_STATIC int dxp_do_readout(Board *board, int *modchan,
                                 unsigned short params[],
                                 unsigned long baseline[],
                                 unsigned long spectrum[]);
XERXES_STATIC int dxp_parse_memory_str(char *name, char *type, unsigned long *base, unsigned long *offset);

static FILE* dxp_find_file(const char *, const char *, char [MAXFILENAME_LEN]);


/* Define the length of the error reporting string info_string */
#define INFO_LEN 400
/* Define the length of the line string used to read in files */
#define LINE_LEN 132

static char *delim=" ,=\t\r\n";
/* Register definition for zero and one */
static int zero=0;
/* Shorthand notation telling routines to act on all channels of the DXP (-1 currently). */
static int allChan=ALLCHAN;
/* Maximum number of files that can be open at a time */
#define MAXFILE 10
static FILE *filpnt[MAXFILE];
static int  dxpopen=0;

static int numDxpMod=0;
static int numDxpChan=0;

/* 8/21/01--Modified variable declarations to match xia_xerxes.h PJF */

/*
 *  Head of Linked list of Interfaces
 */
Interface *iface_head = NULL;

/*
 *  Structure containing pointers to machine dependent routines 
 */
Utils *utils = NULL;

/*
 *  Structure to contain the board types in the system
 */
Board_Info *btypes_head = NULL;

/*
 *	Define the Head of the linked list of items in the system
 */
Board *system_head = NULL;
  
/*
 *	Define a global structure for system level information
 */
System_Info *info = NULL;
  
/*
 *	Define the head of the DSP linked list
 */
Dsp_Info *dsp_head = NULL;
  
/*
 *	Define the head of the FIPPI linked list
 */
Fippi_Info *fippi_head = NULL;
  
/*
 *	Define the head of the DSP parameter defaults linked list
 */
Dsp_Defaults *defaults_head = NULL;  

/* The head of the System FiPPI linked-list. */
System_FiPPI_Info *system_fippi_head = NULL;

/*
 * Define global static variables to hold the information 
 * used to assign new board information.
 */
static Board_Info        *working_btype        = NULL;  
static Board             *working_board        = NULL;  
static Interface         *working_iface        = NULL;  
static Dsp_Info          *working_dsp          = NULL;  
static Fippi_Info        *working_fippi        = NULL;  
static Fippi_Info        *working_mmu          = NULL;  
static Fippi_Info        *working_user_fippi   = NULL;  
static Fippi_Info        *working_system_fpga  = NULL;
static Fippi_Info        *working_fippi_a      = NULL;
static Dsp_Defaults      *working_defaults     = NULL;  
static Dsp_Info          *working_system_dsp   = NULL;
static System_FiPPI_Info *working_system_fippi = NULL;


/********------******------*****------******-------******-------******------*****
 * Routines to perform global initialization functions.  Read in configuration 
 * files, download data to all modules, etc...
 *
 ********------******------*****------******-------******-------******------*****/

/******************************************************************************
 *
 * Routine to initial data structures and pointers for the XerXes 
 * library.
 *
 ******************************************************************************/
XERXES_EXPORT int XERXES_API dxp_init_library(void)
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];


  /* First thing is to load the utils structure.  This is needed 
   * to call allocation of any memory in general as well as any
   * error reporting.  It is absolutely essential that this work
   * with no errors.  */

  /* Install the utility functions of the XerXes Library */
  if((status=dxp_install_utils("INIT_LIBRARY"))!=DXP_SUCCESS){
	sprintf(info_string,"Unable to install utilities in XerXes");
	printf("%s\n",info_string);
	return status;
  }

  /* Initialize the global system */
  if((status=dxp_init_ds())!=DXP_SUCCESS){
	sprintf(info_string,"Unable to initialize data structures.");
	dxp_log_error("dxp_init_library",info_string,status);
	return status;
  }

  return status;
}
	
/******************************************************************************
 *
 * Global initialization routine.  Should be called at the start of all
 * programs.
 *
 ******************************************************************************/
XERXES_EXPORT int XERXES_API dxp_initialize(void)
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];


  /* First thing is to load the utils structure.  This is needed 
   * to call allocation of any memory in general as well as any
   * error reporting.  It is absolutely essential that this work
   * with no errors.  */ 

  /* Initialize the data structures and pointers of the library */
  if((status=dxp_init_library())!=DXP_SUCCESS){
	sprintf(info_string,"Unable to initialize XerXes");
	printf("%s\n",info_string);
	return status;
  }

  dxp_log_debug("dxp_initialize", "Preparing to do init. tasks");

  /* Read the configuration file */
  if((status=dxp_read_config("XIA_CONFIG"))!=DXP_SUCCESS){
	sprintf(info_string,"Error reading configuration file.");
	dxp_log_error("dxp_initialize",info_string,status);
	return status;
  }

  dxp_log_info("dxp_initialize", "Assigning Channel definitions");

  /* Setup the modules by assigning them IO and module channel numbers */
  if ((status=dxp_assign_channel())!=DXP_SUCCESS){
	status = DXP_INITIALIZE;
	dxp_log_error("dxp_initialize","Could not assign channels for this config.",status);
	return status;
  }

  dxp_log_info("dxp_initialize", "Performing User Setup (downloading firmware, starting system)");

  /* Setup the modules by assigning them IO and module channel numbers */
  if ((status=dxp_user_setup())!=DXP_SUCCESS){
	status = DXP_INITIALIZE;
	dxp_log_error("dxp_initialize",
				  "Could not perform user setup(downloading and calibrations)",status);
	return status;
  }

  dxp_log_info("dxp_initialize", "Done with Initialization, exiting dxp_initialize()");

  return status;
}

/******************************************************************************
 *
 * Routine to install pointers to the machine dependent
 * utility routines.
 *
 ******************************************************************************/
XERXES_EXPORT int XERXES_API dxp_install_utils(const char *utilname)
{
  Xia_Util_Functions util_funcs;
  char lstr[133];

  if (utilname != NULL) {
	strncpy(lstr, utilname, 132);
	lstr[strlen(utilname)] = '\0';
  } else {
	strcpy(lstr, "NULL");
  }

  /* Call the MD layer init function for the utility routines */
  dxp_md_init_util(&util_funcs, lstr);

  /* Now build up some information...slowly */
  xerxes_md_alloc          = util_funcs.dxp_md_alloc;
  xerxes_md_free           = util_funcs.dxp_md_free;
  xerxes_md_error_control  = util_funcs.dxp_md_error_control;
  xerxes_md_error          = util_funcs.dxp_md_error;
  xerxes_md_warning        = util_funcs.dxp_md_warning;
  xerxes_md_info           = util_funcs.dxp_md_info;
  xerxes_md_debug          = util_funcs.dxp_md_debug;
  xerxes_md_output         = util_funcs.dxp_md_output;
  xerxes_md_suppress_log   = util_funcs.dxp_md_suppress_log;
  xerxes_md_enable_log	   = util_funcs.dxp_md_enable_log;
  xerxes_md_set_log_level  = util_funcs.dxp_md_set_log_level;
  xerxes_md_log            = util_funcs.dxp_md_log;
  xerxes_md_wait           = util_funcs.dxp_md_wait;
  xerxes_md_puts           = util_funcs.dxp_md_puts;
  xerxes_md_set_priority   = util_funcs.dxp_md_set_priority;
  xerxes_md_fgets          = util_funcs.dxp_md_fgets;
  xerxes_md_tmp_path       = util_funcs.dxp_md_tmp_path;
  xerxes_md_clear_tmp      = util_funcs.dxp_md_clear_tmp;
  xerxes_md_path_separator = util_funcs.dxp_md_path_separator;

  /* Now for the Utils structure */
  if (utils!=NULL) {

	xerxes_md_free(utils->dllname);
	xerxes_md_free(utils->funcs);
	xerxes_md_free(utils);
	utils = NULL;
  }
  /* Allocate and assign memory for the Utils structure */
  utils = (Utils *) xerxes_md_alloc(sizeof(Utils));
  utils->dllname = (char *) xerxes_md_alloc((strlen(lstr)+1)*sizeof(char));
  strcpy(utils->dllname, lstr);
  utils->funcs = (Xia_Util_Functions *) xerxes_md_alloc(sizeof(Xia_Util_Functions));
	
#ifdef XIA_SPECIAL_MEM	
  utils->funcs->dxp_md_alloc         	= xerxes_md_alloc;
  utils->funcs->dxp_md_free          	= xerxes_md_free;

#else /* XIA_SPECIAL_MEM */

  utils->funcs->dxp_md_alloc			  	= malloc;
  utils->funcs->dxp_md_free			  	= free;
#endif /* XIA_SPECIAL_MEM */

  utils->funcs->dxp_md_error_control 	= xerxes_md_error_control;
  utils->funcs->dxp_md_error         	= xerxes_md_error;
  utils->funcs->dxp_md_warning       	= xerxes_md_warning;
  utils->funcs->dxp_md_info          	= xerxes_md_info;
  utils->funcs->dxp_md_debug         	= xerxes_md_debug;
  utils->funcs->dxp_md_output        	= xerxes_md_output;
  utils->funcs->dxp_md_suppress_log  	= xerxes_md_suppress_log;
  utils->funcs->dxp_md_enable_log	  	= xerxes_md_enable_log;
  utils->funcs->dxp_md_set_log_level 	= xerxes_md_set_log_level;
  utils->funcs->dxp_md_log           	= xerxes_md_log;
  utils->funcs->dxp_md_wait          	= xerxes_md_wait;
  utils->funcs->dxp_md_puts          	= xerxes_md_puts;
  utils->funcs->dxp_md_fgets          = xerxes_md_fgets;
  utils->funcs->dxp_md_tmp_path       = xerxes_md_tmp_path;
  utils->funcs->dxp_md_clear_tmp      = xerxes_md_clear_tmp;
  utils->funcs->dxp_md_path_separator = xerxes_md_path_separator;

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Initialize the data structures to an empty state
 *
 ******************************************************************************/
XERXES_EXPORT int XERXES_API dxp_init_ds(void)
{
  int status = DXP_SUCCESS;
  char info_string[INFO_LEN];

  Board_Info *current = btypes_head;
  Board_Info *next = NULL;

  /* CLEAR BTYPES LINK LIST */
	
  /* First make sure there is not an allocated btypes_head */
  while (current != NULL) {
	next = current->next;
	/* Clear the memory first */
	dxp_free_binfo(current);
	current = next;
  }
  /* And clear out the pointer to the head of the list(the memory is freed) */
  btypes_head = NULL;
	
  /* NULL the current board type in the static variable, to ensure we dont point 
   * free memory */
  working_btype = NULL;
	
  /* CLEAR INFO STRUCTURE */
	
  /* Make sure the info structure is empty */
  if (info!=NULL) {
	if (info->preamp  != NULL) xerxes_md_free(info->preamp);
	if (info->modules != NULL) xerxes_md_free(info->modules);
	xerxes_md_free(info);
	info = NULL;
  }
  /* Now allocate memory for the system level information */
  info = (System_Info*) xerxes_md_alloc(sizeof(System_Info));
  info->preamp = NULL;
  info->modules= NULL;
  info->btypes = NULL;
  info->utils = utils;
  info->status = 0;

  /* CLEAR BOARDS,DSP,FIPPI,DEFAULTS LINK LIST */
  if((status=dxp_init_boards_ds())!=DXP_SUCCESS){
	sprintf(info_string,"Failed to initialize the boards structures");
	dxp_log_error("dxp_init_ds",info_string,status);
	return status;
  }

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Initialize the BOARD data structures to an empty state
 *
 ******************************************************************************/
XERXES_EXPORT int XERXES_API dxp_init_boards_ds(void)
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];
	
  Board *next;
  Board *current = system_head;

  /* Initialize the system_head linked list */
  if (current!=NULL) {
	/* Clear out the linked list...deallocating all memory */
	do {
	  next = current->next;
	  dxp_free_board(current);
	  current = next;
	} while (next!=NULL);
	system_head = NULL;
  }

  /* NULL the current board in the static variable, to ensure we dont point 
   * free memory */
  working_board = NULL;
	
  /* CLEAR INTERFACE LINK LIST */
  if((status=dxp_init_iface_ds())!=DXP_SUCCESS){
	sprintf(info_string,"Failed to initialize the Interface structures");
	dxp_log_error("dxp_init_ds",info_string,status);
	return status;
  }
	
  /* CLEAR DSP LINK LIST */
  if((status=dxp_init_dsp_ds())!=DXP_SUCCESS){
	sprintf(info_string,"Failed to initialize the dsp structures");
	dxp_log_error("dxp_init_boards_ds",info_string,status);
	return status;
  }

  /* CLEAR FIPPI LINK LIST */
  if((status=dxp_init_fippi_ds())!=DXP_SUCCESS){
	sprintf(info_string,"Failed to initialize the fippi structures");
	dxp_log_error("dxp_init_boards_ds",info_string,status);
	return status;
  }

  status = dxp_init_system_fippi_ds();

  if (status != DXP_SUCCESS) {
    dxp_log_error("dxp_init_boards_ds", "Failed to initialize System FiPPI "
                  "list", status);
    return status;
  }

  /* CLEAR DEFAULTS LINK LIST */
  if((status=dxp_init_defaults_ds())!=DXP_SUCCESS){
	sprintf(info_string,"Failed to initialize the defaults structures");
	dxp_log_error("dxp_init_boards_ds",info_string,status);
	return status;
  }

  /* Initialize the number of modules and channels */
  numDxpMod = 0;
  numDxpChan= 0;

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Initialize the INTERFACE data structures to an empty state
 *
 ******************************************************************************/
static int XERXES_API dxp_init_iface_ds(VOID)
{
  Interface *next;
  Interface *current = iface_head;

  /* Search thru the iface LL and clear them out */
  while (current != NULL) {
	next = current->next;
	/* Clear the memory first */
	dxp_free_iface(current);
	current = next;
  }
  /* And clear out the pointer to the head of the list(the memory is freed) */
  iface_head = NULL;
	
  /* NULL the current board type in the static variable, to ensure we dont point 
   * free memory */
  working_iface = NULL;

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Initialize the DSP data structures to an empty state
 *
 ******************************************************************************/
static int XERXES_API dxp_init_dsp_ds(void)
{
  Dsp_Info *next;
  Dsp_Info *current = dsp_head;

  /* Search thru the iface LL and clear them out */
  while (current != NULL) {
	next = current->next;
	/* Clear the memory first */
	dxp_free_dsp(current);
	current = next;
  }
  /* And clear out the pointer to the head of the list(the memory is freed) */
  dsp_head = NULL;
	
  /* NULL the current dsp in the static variable, to ensure we dont point 
   * free memory */
  working_dsp = NULL;

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Initialize the FIPPI data structures to an empty state
 *
 ******************************************************************************/
static int XERXES_API dxp_init_fippi_ds(void)
{
  Fippi_Info *next;
  Fippi_Info *current = fippi_head;

  /* Search thru the iface LL and clear them out */
  while (current != NULL) {
	next = current->next;
	/* Clear the memory first */
	dxp_free_fippi(current);
	current = next;
  }
  /* And clear out the pointer to the head of the list(the memory is freed) */
  fippi_head = NULL;
	
  /* NULL the current fippi in the static variable, to ensure we dont point 
   * free memory */
  working_fippi = NULL;

  return DXP_SUCCESS;
}


/**
 * @brief Initialize the System FiPPI list to an empty state.
 */
static int dxp_init_system_fippi_ds(void)
{
  System_FiPPI_Info *next;
  System_FiPPI_Info *current = system_fippi_head;


  while (current != NULL) {
    next = current->next;
    dxp_free_system_fippi(current);
    current = next;
  }

  system_fippi_head    = NULL;
  working_system_fippi = NULL;

  return DXP_SUCCESS;
}


/******************************************************************************
 *
 * Initialize the DEFAULTS data structures to an empty state
 *
 ******************************************************************************/
static int XERXES_API dxp_init_defaults_ds(void)
{
  Dsp_Defaults *next;
  Dsp_Defaults *current = defaults_head;

  /* Search thru the iface LL and clear them out */
  while (current != NULL) {
	next = current->next;
	/* Clear the memory first */
	dxp_free_defaults(current);
	current = next;
  }
  /* And clear out the pointer to the head of the list(the memory is freed) */
  defaults_head = NULL;
	
  /* NULL the current defaults in the static variable, to ensure we dont point 
   * free memory */
  working_defaults = NULL;

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Read the global configuration file.
 *
 ******************************************************************************/
int XERXES_API dxp_read_config(char *cname)
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];
  char line[LINE_LEN],*token;
  int len, start, finish;
  unsigned int i;

  FILE *fp;
  char *cstatus=NULL;
  char *filename;
  char newFile[MAXFILENAME_LEN];

  char *values[1];

  dxp_log_debug("dxp_read_config", "Preapring to read config");

  /* Find the file that contains the XIA system configuration information */
  if((fp=dxp_find_file(cname,"r",newFile))==NULL){
	/* Try to find the file by double indirection....just in case */
	if ((filename=getenv(cname))!=NULL) {
	  if((fp=dxp_find_file(filename,"r",newFile))==NULL){ 
		/* Could not find the XIA_CONFIG file! */
		status = DXP_OPEN_FILE;
		sprintf(info_string,
				"Can not find Config File: %s", PRINT_NON_NULL(filename));
		dxp_log_error("dxp_read_config",info_string,status);
		return status;
	  }
	  xia_file_close(fp);
	} else {
	  status = DXP_OPEN_FILE;
	  sprintf(info_string,
			  "Could not find the environment variable: %s", PRINT_NON_NULL(cname));
	  dxp_log_error("dxp_read_config",info_string,status);
	  return status;
	}
  }

  /*
   * Count the number of board types in the configuration file.  Look
   * for a 'dxp' string at the beginning of the line, this signifies
   * a new board type definition.  Use this count to allocate memory.
   */ 
  for(i=0;;i++){ 
	do {
	  cstatus = xerxes_md_fgets(line,132,fp); 
	} while (((line[0]=='*')||(line[0]=='\r')||(line[0]=='\n'))&&(cstatus!=NULL)); 
	if(cstatus==NULL) break;
	/* What kind of entry do we have?
	 * Supported types:  dxp* = new board type
	 *				     library = DLL for the most recent board type
	 */
	/* Get the length of the line */
	len = strlen(line);
	/* Find the first token deliminated by a space, this is the entry type */
	token = strtok(line, delim);
	/* Unfort, need to allow spaces in the filenames.
	 * so count forward in the line till out of spaces, this is the start of the 
	 * file name */
	for(start=strlen(token)+1;start<len;start++) 
	  if(isspace(line[start])==0) break;				/* find first character in path */ 
	/* Now count backwards from the end of the line till you hit a non-space */
	for(finish=len;finish>start;finish--) 
	  if(isgraph(line[finish])!=0) break;				/* find last character in name  */ 
	/* Allocate memory for the 2nd token */
	len = finish-start+1;
	/* Now copy the value associated with this item */
	values[0] = (char *) xerxes_md_alloc((len+1)*sizeof(char));
	/* Copy the value into the array. End the string with a NULL character. */
	strncpy(values[0], line+start, len);
	values[0][len] = '\0';

	/* Add this item to the system information */
	if ((status=dxp_add_system_item(token,values))!=DXP_SUCCESS){
	  status = DXP_INITIALIZE;
	  sprintf(info_string,"Problems adding item: %s",PRINT_NON_NULL(token));
	  dxp_log_error("dxp_read_config",info_string,status);
	  return status;
	}
  }

  return status;
}

/******************************************************************************
 *
 * Add an item specified by token to the system configuration.
 *
 ******************************************************************************/
int XERXES_API dxp_add_system_item(char *ltoken, char **values)
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];
  int len;
  unsigned int j;

  char *strTmp = '\0';

  strTmp = (char *)xerxes_md_alloc((strlen(ltoken) + 1) * sizeof(char));
  strcpy(strTmp, ltoken);

  /* Now Proceed to process the entry */

  /* Get the length of the value parameters used by this routine */
  len = strlen(values[0]);

  /* Convert the token to lower case for comparison */
  for (j=0;j<strlen(strTmp);j++) {
		
	strTmp[j] = (char) tolower(strTmp[j]);
  }

  if ((strncmp(strTmp, "dxp",3)==0) || 
	  (STREQ(strTmp,   "xmap"))       ||
      (STREQ(strTmp, "vega"))       ||
      (STREQ(strTmp, "mercury"))    ||
	  (strncmp(strTmp, "udxp", 4) == 0)) {
	/* Load function pointers for local use */
	if ((status=dxp_add_btype(strTmp, values[0], NULL))!=DXP_SUCCESS) {
	  sprintf(info_string,"Problems adding board type %s",PRINT_NON_NULL(strTmp));
	  dxp_log_error("dxp_add_system_item",info_string,status);
	  xerxes_md_free((void *)strTmp);
	  strTmp = NULL;
	  return DXP_SUCCESS;
	}
  } else if (STREQ(strTmp,"preamp")) {
	/* Now copy the filename for the preamp configuration file */
	info->preamp = (char *) xerxes_md_alloc((len+1)*sizeof(char));
	/* Copy the filename into the structure.  End the string with 
	 * a NULL character. */
	info->preamp = strncpy(info->preamp, values[0], len);
	info->preamp[len] = '\0';
  } else if (STREQ(strTmp,"modules")) {
	/* Now copy the filename for the modules configuration file */
	info->modules = (char *) xerxes_md_alloc((len+1)*sizeof(char));
	/* Copy the filename into the structure.  End the string with 
	 * a NULL character. */
	info->modules = strncpy(info->modules, values[0], len);
	info->modules[len] = '\0';
  } else {
	status = DXP_INPUT_UNDEFINED;
	sprintf(info_string,"Unable to add token: %s", strTmp);
	dxp_log_error("dxp_add_system_item",info_string,status);
	xerxes_md_free((void *)strTmp);
	strTmp = NULL;
	return status;
  }

  xerxes_md_free((void *)strTmp);
  strTmp = NULL;

  return status;
}

/******************************************************************************
 *
 * Add an item specified by token to the board configuration.
 *
 ******************************************************************************/
int dxp_add_board_item(char *ltoken, char **values)
{
  int status           = DXP_SUCCESS;
  int param_array_size = 0;
  int total_syms       = 0;

  char info_string[INFO_LEN];

  unsigned int len;
  unsigned int j;
  unsigned int nchan;

  unsigned short used;

  int ioChan;
  int chanid;

  boolean_t found = FALSE_;

  int *detChan;

  char *board_type = NULL;

  /* Get the length of the values parameter used by this routine */
  len = strlen(values[0]);

  /* 
   * Now Identify the entry type 
   */
  if (STREQ(ltoken, "board_type")) {

	board_type = values[0];
	MAKE_LOWER_CASE(board_type, j);

	/* Search thru board types to match find the structure 
	   in the linked list */
	sprintf(info_string, "board_type = '%s'", board_type);
	dxp_log_debug("dxp_add_board_item", info_string);

	working_btype = btypes_head;

	while ((working_btype != NULL) && !found) {
	  if (STREQ(working_btype->name, board_type)) {
		found = TRUE_;
		break;
	  }

	  working_btype = working_btype->next;
	}

	if (!found) {
	  sprintf(info_string, "Unknown board type '%s'", board_type);
	  dxp_log_error("dxp_add_board_item", info_string, DXP_UNKNOWN_BTYPE);
	  return DXP_UNKNOWN_BTYPE;
	}

	sprintf(info_string, "working_btype->name = '%s'", working_btype->name);
	dxp_log_debug("dxp_add_board_item", info_string);


  } else if (STREQ(ltoken, "interface")) {
	/* Define the interface tag, this will typically be the information 
	 * that the DXP_MD_INITIALIZE() will use to determine what MD routines
	 * are needed */

	if ((values[0] == NULL) || (values[1] == NULL)) {
	  dxp_log_error("dxp_add_board_item", "Interface 'tag' and 'type' may not "
					"be NULL.", DXP_INITIALIZE);
	  return DXP_INITIALIZE;
	}

	MAKE_LOWER_CASE(values[0], j);

	status = dxp_add_iface(values[0], values[1], &working_iface);

	if(status != DXP_SUCCESS) {
	  sprintf(info_string, "Unable to load '%s' interface %s",
			  PRINT_NON_NULL(values[1]),PRINT_NON_NULL(values[0]));
	  dxp_log_error("dxp_add_board_item", info_string, status);
	  return status;
	}

	if (working_board) {
	  working_board->iface = working_iface;
	}

  } else if (STREQ(ltoken, "module")) {

	sprintf(info_string, "New module '%d': interface = '%s', # channels = %s",
			numDxpMod, values[0], values[1]);
	dxp_log_info("dxp_add_board_item", info_string);

	if(numDxpMod == MAXDXP){
	  sprintf(info_string, "Too many modules specified: only %d are allowed",
			  MAXDXP);
	  dxp_log_error("dxp_add_board_item", info_string, DXP_ARRAY_TOO_SMALL);
	  return DXP_ARRAY_TOO_SMALL;
	}

	if (working_btype== NULL) {
	  sprintf(info_string, "No board type defined for module '%d'", numDxpMod);
	  dxp_log_error("dxp_add_board_item", info_string, DXP_UNKNOWN_BTYPE);
	  return DXP_UNKNOWN_BTYPE;
	}

	if (working_iface==NULL) {
	  sprintf(info_string, "No interface define for module '%d'", numDxpMod);
	  dxp_log_error("dxp_add_board_item", info_string, DXP_INITIALIZE);
	  return DXP_INITIALIZE;
	}

	/* Set the NumDxpMod'th to have none of the channels used */
	used = 0;

	/* Pull out the information for where in the system the module sits 
	 * and then open the crate via the dxp_md_open() routine.  This routine is 
	 * expected to return a number for ioChan that identifies this module's
	 * location in the array used by the machine dependent functions to 
	 * identify how to talk to this module. */

	status = working_iface->funcs->dxp_md_open(values[0], &ioChan);

	if(status != DXP_SUCCESS){
	  sprintf(info_string, "Error opening module '%d' with interface '%s'",
			  numDxpMod, PRINT_NON_NULL(values[0]));
	  dxp_log_error("dxp_add_board_item", info_string, status);
	  return status;
	}

	/* Now loop over the next MAXCHAN numbers to define a detector element number
	 * associated with each DXP channel.  If positive then the channel is 
	 * considered instrumented and the number is stored in the Used[] array
	 * for future reference.  Else if the number is negative or 0, the channel
	 * is ignored */
        
	nchan = (unsigned int)strtol(values[1], NULL, 10);

	detChan = (int *)xerxes_md_alloc(nchan * sizeof(int));

	for(j = 0; j < nchan; j++) {
	  chanid = (int)strtol(values[j + 2], NULL, 10);

	  if (chanid >= 0) {
		used |= 1 << j;
		detChan[j] = chanid;
		numDxpChan++;

	  } else {
		detChan[j] = -1;
	  }
	}

	/* Find the last entry in the Linked list and add to the end */
	working_board = system_head;

	if (working_board != NULL) {

	  while (working_board->next != NULL) {
		working_board = working_board->next;
	  }

	  working_board->next = (Board *)xerxes_md_alloc(sizeof(Board));
	  working_board = working_board->next;

	} else {
	  system_head = (Board *)xerxes_md_alloc(sizeof(Board));
	  working_board = system_head;
	}

	working_board->ioChan  = ioChan;
	working_board->used	   = used;
	working_board->detChan = detChan;
	working_board->mod	   = numDxpMod;
	working_board->nchan   = nchan;
	working_board->btype   = working_btype;
	working_board->iface   = working_iface;

	/* Allocate memory for the iostring containing slot information, etc... */
	working_board->iostring = (char *)xerxes_md_alloc(strlen(values[0]) + 1);
	strcpy(working_board->iostring, values[0]);

	memset(working_board->state, 0, sizeof(working_board->state));

	working_board->btype->funcs->dxp_init_driver(working_iface);
	working_board->next	= NULL;

	/* More memory assignments */
	working_board->params =
	  (unsigned short **)xerxes_md_alloc(nchan * sizeof(unsigned short *));
	
	for (j = 0; j < working_board->nchan; j++) {
	  working_board->params[j] = NULL;
	}

	working_board->chanstate =
	  (Chan_State *)xerxes_md_alloc(nchan * sizeof(Chan_State));
	memset(working_board->chanstate, 0, nchan * sizeof(Chan_State));

	working_board->dsp =
	  (Dsp_Info **)xerxes_md_alloc(nchan * sizeof(Dsp_Info *));
	working_board->fippi=
	  (Fippi_Info **)xerxes_md_alloc(nchan * sizeof(Fippi_Info *));
	working_board->user_fippi=
	  (Fippi_Info **)xerxes_md_alloc(nchan * sizeof(Fippi_Info *));
	working_board->defaults =
	  (Dsp_Defaults **)xerxes_md_alloc(nchan * sizeof(Dsp_Defaults *));
	working_board->preamp =
	  (Preamp_Info *)xerxes_md_alloc(nchan * sizeof(Preamp_Info));

	working_board->mmu          = working_mmu;
	working_board->system_dsp   = working_system_dsp;
	working_board->fippi_a      = working_fippi_a;
	working_board->system_fpga  = working_system_fpga;
  working_board->system_fippi = working_system_fippi;

	/* Fill in some default information */
	for (j = 0; j < working_board->nchan; j++) {

	  if (working_board->used && (1<<j)) {
		working_board->dsp[j]        = working_dsp;
		working_board->fippi[j]      = working_fippi;
		working_board->user_fippi[j] = working_user_fippi;
		working_board->defaults[j]   = working_defaults;

		if (working_dsp != NULL) {
		  param_array_size =
			working_board->dsp[j]->params->nsymbol * sizeof(unsigned short);
		  working_board->params[j] =
			(unsigned short *)xerxes_md_alloc(param_array_size);
		  memset(working_board->params[j], 0, param_array_size);
		}

	  } else {
		working_board->dsp[j]        = NULL;
		working_board->params[j]     = NULL;
		working_board->fippi[j]      = NULL;
		working_board->user_fippi[j] = NULL;
		working_board->defaults[j]   = NULL;
	  }
	}

	numDxpMod++;

  } else if (STREQ(ltoken, "default_dsp")) {

	if(working_btype == NULL){
	  dxp_log_error("dxp_add_board_item", "A board type must be defined in "
					"order to process the 'default_dsp' tag", DXP_UNKNOWN_BTYPE);
	  return DXP_UNKNOWN_BTYPE;
	}

	status = dxp_add_dsp(values[0], working_btype, &working_dsp);

	if(status != DXP_SUCCESS) {
	  sprintf(info_string, "Unable to load DSP '%s'", PRINT_NON_NULL(values[0]));
	  dxp_log_error("dxp_add_board_item", info_string, status);
	  return status;
	}

	/* Set this as the DSP code for each channel of the current module */

	if (working_board != NULL) {
	  for (j = 0; j < working_board->nchan; j++) {
		working_board->dsp[j] = working_dsp;

		if (working_board->params[j] != NULL) {
		  xerxes_md_free(working_board->params[j]);
		  working_board->params[j] = NULL;
		}

		param_array_size =
		  working_board->dsp[j]->params->nsymbol * sizeof(unsigned short);

		working_board->params[j] =
		  (unsigned short *)xerxes_md_alloc(param_array_size);
		memset(working_board->params[j], 0, param_array_size);

		/* Set the dspstate value to 2, indicating that the DSP needs update */
		working_board->chanstate[j].dspdownloaded = 2;
	  }
	}

  } else if (STREQ(ltoken, "dsp")) {

	if (working_btype == NULL){
	  dxp_log_error("dxp_add_board_item", "Unable to process 'dsp' unless a "
					"board type has been defined", DXP_UNKNOWN_BTYPE);
	  return DXP_UNKNOWN_BTYPE;
	}

	chanid = (int)strtol(values[0] , NULL, 10);

	if ((chanid < 0) || (chanid >= (int)working_board->nchan)) {
	  sprintf(info_string, "Selected channel (%d) for DSP code is not valid for "
			  "the current module, which has %d channels", chanid,
			  (int)working_board->nchan);
	  dxp_log_error("dxp_add_board_item", info_string, DXP_BADCHANNEL);
	  return DXP_BADCHANNEL;
	}

	status = dxp_add_dsp(values[1], working_btype,
						 &(working_board->dsp[chanid]));

	if (status != DXP_SUCCESS) {
	  sprintf(info_string, "Unable to add DSP code: '%s'",
			  PRINT_NON_NULL(values[1]));
	  dxp_log_error("dxp_add_board_item", info_string, status);
	  return status;
	}

	working_board->chanstate[chanid].dspdownloaded = 2;

	if (working_board->params[chanid]) {
	  xerxes_md_free(working_board->params[chanid]);
	  working_board->params[chanid] = NULL;
	}

	param_array_size =
	  working_board->dsp[chanid]->params->nsymbol * sizeof(unsigned short);

	working_board->params[chanid] = (unsigned short *) 
	  xerxes_md_alloc(param_array_size);

	memset(working_board->params[chanid], 0, param_array_size);

  } else if (STREQ(ltoken, "default_fippi")) {

	if (working_btype == NULL){
	  dxp_log_error("dxp_add_board_item", "Unable to process 'default_fippi' "
					"unless a board type has been defined", DXP_UNKNOWN_BTYPE);
	  return DXP_UNKNOWN_BTYPE;
	}

	status = dxp_add_fippi(values[0], working_btype, &working_fippi);

	if (status != DXP_SUCCESS) {
	  sprintf(info_string,"Unable to add FiPPI: '%s'",
			  PRINT_NON_NULL(values[0]));
	  dxp_log_error("dxp_add_board_item", info_string, status);
	  return status;
	}

	if (working_board != NULL) {
	  for (j = 0; j < working_board->nchan; j++)
		working_board->fippi[j] = working_fippi;
	}

  } else if (STREQ(ltoken, "fippi")) {

	if (working_btype == NULL){
	  dxp_log_error("dxp_add_board_item", "Unable to process 'default_fippi' "
					"unless a board type has been defined", DXP_UNKNOWN_BTYPE);
	  return DXP_UNKNOWN_BTYPE;
	}

	chanid = (int)strtol(values[0], NULL, 10);

	if ((chanid < 0) || (chanid >= (int)working_board->nchan)) {
	  sprintf(info_string, "Selected channel (%d) for FiPPI code is not valid "
			  "for the current module, which has %d channels", chanid,
			  (int)working_board->nchan);
	  dxp_log_error("dxp_add_board_item", info_string, DXP_BADCHANNEL);
	  return DXP_BADCHANNEL;
	}

	status = dxp_add_fippi(values[1], working_btype,
						   &(working_board->fippi[chanid]));

	if (status != DXP_SUCCESS) {
	  sprintf(info_string, "Unable to add FiPPI: '%s'",
			  PRINT_NON_NULL(values[1]));
	  dxp_log_error("dxp_add_board_item", info_string, status);
	  return status;
	}

  } else if (STREQ(ltoken, "system_fippi")) {

    if (working_btype == NULL) {
      dxp_log_error("dxp_add_board_item", "Unable to process 'system_fippi' "
                    "unless a board type has been defined", DXP_UNKNOWN_BTYPE);
      return DXP_UNKNOWN_BTYPE;
    }

    status = dxp_add_system_fippi(values[0], working_btype,
                                  &(working_board->system_fippi));

    if (status != DXP_SUCCESS) {
      sprintf(info_string, "Unable to add System FiPPI: '%s'",
              PRINT_NON_NULL(values[0]));
      dxp_log_error("dxp_add_board_item", info_string, status);
      return status;
    }

    working_system_fippi = working_board->system_fippi;

  } else if (STREQ(ltoken, "system_fpga")) {

	if (working_btype == NULL) {
	  dxp_log_error("dxp_add_board_item", "Unable to process 'system_fpga' "
					"unless a board type has been defined", DXP_UNKNOWN_BTYPE);
	  return DXP_UNKNOWN_BTYPE;
	}

	status = dxp_add_fippi(values[0], working_btype,
						   &(working_board->system_fpga));

	if (status != DXP_SUCCESS) {
	  sprintf(info_string, "Unable to add System FPGA: '%s'",
			  PRINT_NON_NULL(values[0]));
	  dxp_log_error("dxp_add_board_item", info_string, status);
	  return status;
	}

	working_system_fpga = working_board->system_fpga;

  } else if (STREQ(ltoken, "fippi_a")) {

	if (working_btype == NULL) {
	  dxp_log_error("dxp_add_board_item", "Unable to process 'fippi_a' "
					"unless a board type has been defined", DXP_UNKNOWN_BTYPE);
	  return DXP_UNKNOWN_BTYPE;
	}

	status = dxp_add_fippi(values[0], working_btype,
						   &(working_board->fippi_a));

	if (status != DXP_SUCCESS) {
	  sprintf(info_string, "Unable to add FiPPI A: '%s'",
			  PRINT_NON_NULL(values[0]));
	  dxp_log_error("dxp_add_board_item", info_string, status);
	  return status;
	}

	working_fippi_a = working_board->fippi_a;

  } else if (STREQ(ltoken, "system_dsp")) {
	
	if (working_btype == NULL) {
	  dxp_log_error("dxp_add_board_item", "Unable to process 'system_dsp' "
					"unless a board type has been defined", DXP_UNKNOWN_BTYPE);
	  return DXP_UNKNOWN_BTYPE;
	}

	sprintf(info_string, "Processing System DSP: %s", values[0]);
	dxp_log_debug("dxp_add_board_item", info_string);

	status = dxp_add_dsp(values[0], working_btype, &(working_board->system_dsp));

	if (status != DXP_SUCCESS) {
	  sprintf(info_string, "Unable to add System DSP: '%s'",
			  PRINT_NON_NULL(values[0]));
	  dxp_log_error("dxp_add_board_item", info_string, status);
	  return status;
	}

	if (working_board != NULL) {
	  total_syms = working_board->system_dsp->params->nsymbol +
		working_board->system_dsp->params->n_per_chan_symbols;

	  for (j = 0; j < working_board->nchan; j++) {

		if (working_board->system_dsp != NULL) {
		  param_array_size = total_syms * sizeof(unsigned short);
		  working_board->params[j] =
			(unsigned short *)xerxes_md_alloc(param_array_size);
		  memset(working_board->params[j], 0, param_array_size);
		}
	  }
	}

	working_system_dsp = working_board->system_dsp;

  } else if (STREQ(ltoken, "mmu")) {

	if (working_btype == NULL){
	  dxp_log_error("dxp_add_board_item", "Unable to process 'mmu' unless a "
					"board type has been defined", DXP_UNKNOWN_BTYPE);
	  return DXP_UNKNOWN_BTYPE;
	}

	status = dxp_add_fippi(values[0], working_btype, &(working_board->mmu));

	if (status != DXP_SUCCESS) {
	  sprintf(info_string, "Unable to add MMU: '%s'", PRINT_NON_NULL(values[0]));
	  dxp_log_error("dxp_add_board_item", info_string, status);
	  return status;
	}

	working_mmu = working_board->mmu;

  } else if (STREQ(ltoken, "default_user_fippi")) {

	if (working_btype == NULL){
	  dxp_log_error("dxp_add_board_item", "Unable to process "
					"'default_user_fippi' unless a board type has been defined",
					DXP_UNKNOWN_BTYPE);
	  return DXP_UNKNOWN_BTYPE;
	}

	status = dxp_add_fippi(values[0], working_btype, &working_user_fippi);

	if (status != DXP_SUCCESS) {
	  sprintf(info_string, "Unable to add default FiPPI: '%s'",
			  PRINT_NON_NULL(values[0]));
	  dxp_log_error("dxp_add_board_item", info_string, status);
	  return status;
	}

	/* Now reload all the FIPPI pointers for the current board type */
	if (working_board != NULL) {
	  for (j = 0; j < working_board->nchan; j++)
		working_board->user_fippi[j] = working_user_fippi;
	}

  } else if (STREQ(ltoken, "user_fippi")) {

	if (working_btype == NULL){
	  dxp_log_error("dxp_add_board_item", "Unable to process 'user_fippi' "
					"unless a board type has been defined", DXP_UNKNOWN_BTYPE);
	  return DXP_UNKNOWN_BTYPE;
	}

	chanid = (int)strtol(values[0], NULL, 10);

	if ((chanid < 0) || (chanid >= (int)working_board->nchan)) {
	  sprintf(info_string, "Selected channel (%d) for User FiPPI code is not "
			  "valid for the current module, which has %d channels", chanid,
			  (int)working_board->nchan);
	  dxp_log_error("dxp_add_board_item", info_string, DXP_BADCHANNEL);
	  return DXP_BADCHANNEL;
	}

	status = dxp_add_fippi(values[1], working_btype,
						   &(working_board->user_fippi[chanid]));

	if (status != DXP_SUCCESS) {
	  sprintf(info_string, "Unable to add User FiPPI code: '%s'",
			  PRINT_NON_NULL(values[1]));
	  dxp_log_error("dxp_add_board_item", info_string, status);
	  return status;
	}

  } else if (STREQ(ltoken, "default_param")) {

	if (working_btype == NULL){
	  dxp_log_error("dxp_add_board_item", "Unable to process 'default_param' "
					"unless a board type has been defined", DXP_UNKNOWN_BTYPE);
	  return DXP_UNKNOWN_BTYPE;
	}

	status = dxp_add_defaults(values[0], working_btype, &working_defaults);

	if (status != DXP_SUCCESS) {
	  sprintf(info_string, "Unable to add default parameters: '%s'",
			  PRINT_NON_NULL(values[0]));
	  dxp_log_error("dxp_add_board_item", info_string, status);
	  return status;
	}

	for (j = 0; j < working_board->nchan; j++) {
	  working_board->defaults[j] = working_defaults;
	}

  } else if (STREQ(ltoken, "param")) {

	if (working_btype == NULL){
	  dxp_log_error("dxp_add_board_item", "Unable to process 'param' "
					"unless a board type has been defined", DXP_UNKNOWN_BTYPE);
	  return DXP_UNKNOWN_BTYPE;
	}

	chanid = (int)strtol(values[0], NULL, 10);

	if ((chanid < 0) || (chanid >= (int)working_board->nchan)) {
	  sprintf(info_string, "Selected channel (%d) for parameters is not "
			  "valid for the current module, which has %d channels", chanid,
			  (int)working_board->nchan);	  
	  dxp_log_error("dxp_add_board_item", info_string, DXP_BADCHANNEL);
	  return DXP_BADCHANNEL;
	}

	status = dxp_add_defaults(values[1], working_btype,
							  &(working_board->defaults[chanid]));

	if (status != DXP_SUCCESS) {
	  sprintf(info_string, "Unable to add parameters: '%s'",
			  PRINT_NON_NULL(values[1]));
	  dxp_log_error("dxp_add_board_item", info_string, status);
	  return status;
	}

  } else if (STREQ(ltoken, "end")) {

	working_board        = NULL;
	working_btype        = NULL;
	working_dsp          = NULL;
	working_fippi        = NULL;
	working_iface        = NULL;
	working_defaults     = NULL;
	working_system_fpga  = NULL;
	working_fippi_a      = NULL;
	working_system_dsp   = NULL;
  working_system_fippi = NULL;

  } else {
	sprintf(info_string, "Unknown board item '%s' to add",
			PRINT_NON_NULL(ltoken));
	dxp_log_error("dxp_add_board_item", info_string, DXP_NOMATCH);
	return DXP_NOMATCH;
  }

  return DXP_SUCCESS;
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
	 /* const char *filename;			Input: filename to open			*/
	 /* const char *mode;				Input: Mode to use when opening	*/
	 /* char *newFile;					Output: Full filename of file (translated env vars)	*/
{
  FILE *fp=NULL;
  char *name=NULL, *name2=NULL;
  char *home=NULL;
	
  unsigned int len = 0;


  /* Try to open file directly */
  if((fp=xia_file_open(filename,mode))!=NULL){
	len = MAXFILENAME_LEN>(strlen(filename)+1) ? strlen(filename) : MAXFILENAME_LEN;
	strncpy(newFile, filename, len);
	newFile[len] = '\0';
	return fp;
  }
  /* Try to open the file with the path XIAHOME */
  if ((home=getenv("XIAHOME"))!=NULL) {
	name = (char *) xerxes_md_alloc(sizeof(char)*
									(strlen(home)+strlen(filename)+2));
	sprintf(name, "%s/%s", home, filename);
	if((fp=xia_file_open(name,mode))!=NULL){
	  len = MAXFILENAME_LEN>(strlen(name)+1) ? strlen(name) : MAXFILENAME_LEN;
	  strncpy(newFile, name, len);
	  newFile[len] = '\0';
	  xerxes_md_free(name);
	  name = NULL;
	  return fp;
	}
	xerxes_md_free(name);
	name = NULL;
  }
  /* Try to open the file with the path DXPHOME */
  if ((home=getenv("DXPHOME"))!=NULL) {
	name = (char *) xerxes_md_alloc(sizeof(char)*
									(strlen(home)+strlen(filename)+2));
	sprintf(name, "%s/%s", home, filename);
	if((fp=xia_file_open(name,mode))!=NULL){
	  len = MAXFILENAME_LEN>(strlen(name)+1) ? strlen(name) : MAXFILENAME_LEN;
	  strncpy(newFile, name, len);
	  newFile[len] = '\0';
	  xerxes_md_free(name);
	  name = NULL;
	  return fp;
	}
	xerxes_md_free(name);
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
		
	  name = (char *) xerxes_md_alloc(sizeof(char)*
									  (strlen(home)+strlen(name2)+2));
	  sprintf(name, "%s/%s", home, name2);
	  if((fp=xia_file_open(name,mode))!=NULL) {
		len = MAXFILENAME_LEN>(strlen(name)+1) ? strlen(name) : MAXFILENAME_LEN;
		strncpy(newFile, name, len);
		newFile[len] = '\0';
		xerxes_md_free(name);
		name = NULL;
		return fp;
	  }
	  xerxes_md_free(name);
	  name = NULL;
	}
  }
  /* Try to open the file with the path DXPHOME and pointing 
   * to a file as an environment variable */
  if ((home=getenv("DXPHOME"))!=NULL) {
	if ((name2=getenv(filename))!=NULL) {
		
	  name = (char *) xerxes_md_alloc(sizeof(char)*
									  (strlen(home)+strlen(name2)+2));
	  sprintf(name, "%s/%s", home, name2);
	  if((fp=xia_file_open(name,mode))!=NULL) {
		len = MAXFILENAME_LEN>(strlen(name)+1) ? strlen(name) : MAXFILENAME_LEN;
		strncpy(newFile, name, len);
		newFile[len] = '\0';
		xerxes_md_free(name);
		name = NULL;
		return fp;
	  }
	  xerxes_md_free(name);
	  name = NULL;
	}
  }

  return NULL;
}


/******************************************************************************
 *
 * Routine to get the full path name for a file, as seen by the XIA libraries.
 * This will add on the proper path as found with environment variables 
 * XIAHOME/DXPHOME, etc...
 *
 ******************************************************************************/
XERXES_EXPORT int XERXES_API dxp_find_filename(const char* filename, const char* mode, char newFile[MAXFILENAME_LEN])
	 /* const char *filename;			Input: filename to open			*/
	 /* const char *mode;				Input: Mode to use when opening	*/
	 /* char *newFile;					Output: Full filename of file (translated env vars)	*/
{
  FILE *fp=NULL;
  int status = DXP_SUCCESS;
  char info_string[INFO_LEN];
	
  unsigned int len = 0;

  /* Try to open file */
  if((fp=dxp_find_file(filename, mode, newFile))!=NULL){
	len = MAXFILENAME_LEN>(strlen(filename)+1) ? strlen(filename) : MAXFILENAME_LEN;
	strncpy(newFile, filename, len);
	newFile[len] = 0;
	/* Since files can not be opened across libraries (at least I don't know how), only return name
	 * and let the calling routine open.  Else return failure to find file */
	xia_file_close(fp);
	return status;
  }

  /* No file found, assert error */
  status = DXP_FILENOTFOUND;
  sprintf(info_string,"Error finding file: %s",PRINT_NON_NULL(filename));
  dxp_log_error("dxp_find_filename",info_string,status);
  return status;
}
/******************************************************************************
 *
 * Routine to return the list of system files currently in use.
 * These filenames have decoded any environment variables used and included
 * them in the path names.
 *
 * The variable maxlen is the maximum length of a filename, this ensures that 
 * the routine will not overwrite memory space that it doesnt have access to
 * 
 * files[0] = XIA_CONFIG file
 * files[1] = modules configuration file
 * files[2] = preamp configuration file
 *
 ******************************************************************************/
int XERXES_API dxp_locate_system_files(unsigned int *maxlen, char **files)
	 /* unsigned short *maxlen;		Input: Maximum length of allocated memory	*/
	 /* char *files;					Output: List of filenames used by XerXes	*/
{
  int status = DXP_SUCCESS;
  char info_string[INFO_LEN];
  FILE *fp=NULL;
  char *filename=NULL;
  char newFile[200];
  char tmpname[5];

  unsigned int i;
  unsigned int lindex = 0;
  unsigned int len = 0;
  unsigned int start = 0;

  /* Try to find the system configuration file	*/
  if ((filename=getenv("XIA_CONFIG"))!=NULL) {
	if((fp=dxp_find_file(filename,"r",newFile))==NULL){ 
	  status = DXP_OPEN_FILE;
	  sprintf(info_string,"Unable to locate the XIA_CONFIG File");
	  dxp_log_error("dxp_locate_files",info_string,status);
	} else {
	  xia_file_close(fp);
	  if (strlen(newFile)>(*maxlen+1)) {
		len = *maxlen;
		start = strlen(newFile) - *maxlen;
	  } else {
		len = strlen(newFile);
		start = 0;
	  }
	  strncpy(files[lindex], newFile+start, len);
	  files[lindex][len] = 0;
	  lindex++;
	}
  } else {
	status = DXP_OPEN_FILE;
	sprintf(info_string,"Unable to locate the XIA_CONFIG environment variable");
	dxp_log_error("dxp_locate_files",info_string,status);
  }

  /* Copy 4 characters of filename for NULL check */
  if (info->modules==NULL) {
	strcpy(tmpname,"NULL");
  } else {
	len = strlen(info->modules)<4 ? strlen(info->modules)+1 : 5;
	for (i=0;i<len;i++) tmpname[i] = (char) toupper(info->modules[i]);
  }
  /* Try to find the modules configuration file	*/
  if((fp=dxp_find_file(info->modules,"r",newFile))!=NULL){ 
	xia_file_close(fp);
	if (strlen(newFile)>(*maxlen+1)) {
	  len = *maxlen;
	  start = strlen(newFile) - *maxlen;
	} else {
	  len = strlen(newFile);
	  start = 0;
	}
	strncpy(files[lindex], newFile+start, len);
	files[lindex][len] = 0;
	lindex++;
  } else if (STREQ(tmpname,"NULL")){ 
	strcpy(files[lindex], tmpname);
	lindex++;
  } else {
	status = DXP_OPEN_FILE;
	sprintf(info_string,"Unable to locate the Modules configuration File");
	dxp_log_error("dxp_locate_files",info_string,status);
  }

  /* Copy 4 characters of filename for NULL check */
  if (info->preamp==NULL) {
	strcpy(tmpname,"NULL");
  } else {
	len = strlen(info->preamp)<4 ? strlen(info->preamp)+1 : 5;
	for (i=0;i<len;i++) tmpname[i] = (char) toupper(info->preamp[i]);
  }
  /* Try to find the preamp configuration file	*/
  if ((fp=dxp_find_file(info->preamp,"r",newFile))!=NULL){ 
	xia_file_close(fp);
	if (strlen(newFile)>(*maxlen+1)) {
	  len = *maxlen;
	  start = strlen(newFile) - *maxlen;
	} else {
	  len = strlen(newFile);
	  start = 0;
	}
	strncpy(files[lindex], newFile+start, len);
	files[lindex][len] = 0;
	lindex++;
  } else if (STREQ(tmpname,"NULL")){ 
	strcpy(files[lindex], tmpname);
	lindex++;
  } else {
	status = DXP_OPEN_FILE;
	sprintf(info_string,"Unable to locate the preamp configuration File");
	dxp_log_error("dxp_locate_files",info_string,status);
  }
	
  return status;
}

/******************************************************************************
 *
 * Routine to return the list of channel configuration files currently in use.
 * These filenames have decoded any environment variables used and included
 * them in the path names.
 *
 * The variable maxlen is the maximum length of a filename, this ensures that 
 * the routine will not overwrite memory space that it doesnt have access to
 * 
 * files[0] = fippi firmware file
 * files[1] = dsp firmware file
 * files[2] = default dsp parameter file
 *
 ******************************************************************************/
int XERXES_API dxp_locate_channel_files(int *detChan, unsigned int *maxlen, char **files)
	 /* int *detChan;				Input: Detector Channel to download new DSP code */
	 /* unsigned short *maxlen;		Input: Maximum length of allocated memory	*/
	 /* char *files;					Output: List of filenames used by XerXes	*/
{
  int status = DXP_SUCCESS, i;
  char info_string[INFO_LEN];
  FILE *fp=NULL;
  char newFile[200];
  char tmpname[5];

  unsigned int lindex = 0;
  unsigned int len = 0;
  unsigned int start = 0;

  int modChan;
  /* Pointer to the chosen Board */
  Board *chosen=NULL;

  /* Get the ioChan and modChan number that matches the detChan */
  if((status=dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS){
	sprintf(info_string,"Error finding detector number %d",*detChan);
	dxp_log_error("dxp_nspec",info_string,status);
	return status;
  }

  /* Try to find the fippi configuration file	*/
  if((fp=dxp_find_file(chosen->fippi[modChan]->filename,"r",newFile))==NULL){ 
	status = DXP_OPEN_FILE;
	sprintf(info_string,"Unable to locate the FIPPI firmware file");
	dxp_log_error("dxp_locate_files",info_string,status);
  } else {
	xia_file_close(fp);
	if (strlen(newFile)>(*maxlen+1)) {
	  len = *maxlen;
	  start = strlen(newFile) - *maxlen;
	} else {
	  len = strlen(newFile);
	  start = 0;
	}
	strncpy(files[lindex], newFile+start, len);
	files[lindex][len] = 0;
	lindex++;
  }

  /* Try to find the dsp file	*/
  if((fp=dxp_find_file(chosen->dsp[modChan]->filename,"r",newFile))==NULL){ 
	status = DXP_OPEN_FILE;
	sprintf(info_string,"Unable to locate the DSP firmware file");
	dxp_log_error("dxp_locate_files",info_string,status);
  } else {
	xia_file_close(fp);
	if (strlen(newFile)>(*maxlen+1)) {
	  len = *maxlen;
	  start = strlen(newFile) - *maxlen;
	} else {
	  len = strlen(newFile);
	  start = 0;
	}
	strncpy(files[lindex], newFile+start, len);
	files[lindex][len] = 0;
	lindex++;
  }

  /* Copy 4 characters of filename for NULL check */
  if (chosen->defaults[modChan]->filename==NULL) {
	strcpy(tmpname,"NULL");
  } else {
	for (i=0;i<5;i++) tmpname[i] = (char) toupper(chosen->defaults[modChan]->filename[i]);
  }
  /* Try to find the defaults file	*/
  if((fp=dxp_find_file(chosen->defaults[modChan]->filename,"r",newFile))!=NULL){ 
	xia_file_close(fp);
	if (strlen(newFile)>(*maxlen+1)) {
	  len = *maxlen;
	  start = strlen(newFile) - *maxlen;
	} else {
	  len = strlen(newFile);
	  start = 0;
	}
	strncpy(files[lindex], newFile+start, len);
	files[lindex][len] = 0;
	lindex++;
  } else if (STREQ(tmpname,"NULL")){ 
	strcpy(files[lindex], tmpname);
	lindex++;
  } else {
	status = DXP_OPEN_FILE;
	sprintf(info_string,"Unable to locate the default DSP parameters file");
	dxp_log_error("dxp_locate_files",info_string,status);
  }
	
  return status;
}

/******************************************************************************
 *
 * Routine to read the modules configuration file.
 *
 ******************************************************************************/
static int XERXES_API dxp_read_modules(FILE* fp)
{
  int status;

  char info_string[INFO_LEN];
  char line[LINE_LEN];

  char *entry_type = NULL;
  char *cstatus = " ";

  unsigned int j;
  unsigned int n;
  unsigned int m;
  unsigned int nchan;

  char strtemp[132];
  char temp_line[132];
  char ioname[132] = "";

  char *values[10];

  boolean_t is_comment = FALSE_;
  boolean_t is_newline = FALSE_;
	
  
  /* Should this be an assertion? Does it indicate that the library
   * hasn't done something or that the user hasn't done something?
   */
  if (btypes_head == NULL) {
	dxp_log_error("dxp_read_modules", "Board types structure is not "
				  "initialized", DXP_INITIALIZE);
	return DXP_INITIALIZE;
  }

  status = dxp_init_boards_ds();

  if (status != DXP_SUCCESS) {
	dxp_log_error("dxp_read_modules", "Error initializing board structure "
				  "prior to reading 'modules' file", status);
	return status;
  }

  while (cstatus != NULL) {

	/* Skip ahead in the file until we encounter some real data. */
	do {
	  cstatus = xerxes_md_fgets(line,132,fp);

	  if (line[0] == '*') {
		is_comment = TRUE_;

	  } else if ((line[0] == '\r') || (line[1] == '\n')) {
		is_newline = TRUE_;
	  }
			
 
	} while ((is_comment || is_newline) && (cstatus != NULL)); 
		
	if (cstatus == NULL) {
	  continue;
	}

	/* TODO: Move this comment to the function
	 * description. (And the documentation.)
	 */
	
	/* What kind of entry do we have?
	 * Supported types:  "module" = new module
	 *				     "mmu" = memory manager filename
	 *                   "system_fpga" = system fpga
	 *				     "default_dsp" = default filename for DSP code
	 *				     "default_fippi" = default filename for FIPPI code
	 *				     "default_param" = default filename for parameters
	 *				     "dsp" channel = DSP filename for this channel
	 *				     "fippi" channel = FIPPI filename for this channel
	 *				     "param" channel = default parameter setting for this
	 *                                     filename
	 */

	strcpy(temp_line, line);
	entry_type = strtok(temp_line, delim);

	MAKE_LOWER_CASE(entry_type, j);

	if (STREQ(entry_type, "board_type")) {

	  values[0] = strtok(NULL, delim);

	  status = dxp_add_board_item(entry_type, values);

	  if (status != DXP_SUCCESS) {
		sprintf(info_string, "Unable to add board type: '%s'",
				PRINT_NON_NULL(values[0]));
		dxp_log_error("dxp_read_modules", info_string, status);
		return status;
	  }

	} else if (STREQ(entry_type, "iolibrary")) {
	  dxp_pick_filename(strlen(entry_type)+1, line, ioname);

	} else if (STREQ(entry_type, "interface")) {
	  /* Every 'interface' requires an 'iolibrary' to be defined
	   * first.
	   */

	  dxp_pick_filename(strlen(entry_type)+1, line, strtemp);

	  values[0] = strtemp;

	  if (STREQ(ioname,"")) {
		dxp_log_error("dxp_read_modules", "Must define an 'iolibrary' tag "
					  "before every 'interface' tag", DXP_INITIALIZE);
		return DXP_INITIALIZE;
	  }

	  values[1] = ioname;

	  status = dxp_add_board_item(entry_type, values);

	  if(status != DXP_SUCCESS) {
		sprintf(info_string, "Error adding '%s' interface",
				PRINT_NON_NULL(values[0]));
		dxp_log_error("dxp_read_modules", info_string, status);
		return status;
	  }

	  /* Reset 'iolibrary' */
	  strcpy(ioname,"");

	} else if (STREQ(entry_type, "module")) {

	  values[0] = strtok(NULL, delim);
			
	  /* Now loop over the remaining entries to determine channel ids and total 
	   * number of channels
	   */
	  nchan = 0;
	 
	  values[2 + nchan] = strtok(NULL, delim);

	  /* Since values[] only goes up to 10, that should be checked as
	   * well.
	   */
	  while (values[2 + nchan] != NULL) {
		nchan++;
		values[2 + nchan] = strtok(NULL, delim);
	  }

	  sprintf(strtemp, "%i", nchan);
	  values[1] = strtemp;

	  status = dxp_add_board_item(entry_type, values);

	  if (status != DXP_SUCCESS) {
		sprintf(info_string, "Unable to add board: '%s'",
				PRINT_NON_NULL(values[0]));
		dxp_log_error("dxp_read_modules", info_string, status);
		return status;
	  }

	} else if (STREQ(entry_type, "default_dsp")) {

	  dxp_pick_filename(strlen(entry_type) + 1, line, strtemp);
	  values[0] = strtemp;

	  status = dxp_add_board_item(entry_type, values);

	  if (status != DXP_SUCCESS) {
		sprintf(info_string, "Unable to add DSP: '%s'",
				PRINT_NON_NULL(values[0]));
		dxp_log_error("dxp_read_modules", info_string, status);
		return status;
	  }

	} else if (STREQ(entry_type, "dsp")) {

	  values[0] = strtok(NULL, delim);
			
	  n = 0;
	  dxp_strnstrm(line, entry_type, &n, &m);

	  n = m;
	  dxp_strnstrm(line, values[0], &n, &m);

	  /* Now n contains the location in the line after the 2nd entry_type */			
	  /* Now search for the filename starting at m+1 */

	  dxp_pick_filename(m + 1, line, strtemp);

	  values[1] = strtemp;

	  status = dxp_add_board_item(entry_type, values);

	  if (status != DXP_SUCCESS) {
		sprintf(info_string, "Unable to add DSP '%s' to channel %s", values[1],
				values[0]);
		dxp_log_error("dxp_read_modules", info_string, status);
		return status;
	  }

	} else if (STREQ(entry_type, "system_fpga")) {

	  dxp_pick_filename(strlen(entry_type) + 1, line, strtemp);
	  values[0] = strtemp;

	  status = dxp_add_board_item(entry_type, values);

	  if (status != DXP_SUCCESS) {
		sprintf(info_string, "Unable to add System FPGA: '%s'",
				PRINT_NON_NULL(values[0]));
		dxp_log_error("dxp_read_modules", info_string, status);
		return status;
	  }

	} else if (STREQ(entry_type, "fippi_a")) {
	  
	  dxp_pick_filename(strlen(entry_type) + 1, line, strtemp);
	  values[0] = strtemp;

	  status = dxp_add_board_item(entry_type, values);

	  if (status != DXP_SUCCESS) {
		sprintf(info_string, "Unable to add FiPPI A: '%s'",
				PRINT_NON_NULL(values[0]));
		dxp_log_error("dxp_read_modules", info_string, status);
		return status;
	  }

	} else if (STREQ(entry_type, "system_dsp")) {

	  dxp_pick_filename(strlen(entry_type) + 1, line, strtemp);
	  values[0] = strtemp;

	  status = dxp_add_board_item(entry_type, values);

	  if (status != DXP_SUCCESS) {
		sprintf(info_string, "Unable to add System DSP: '%s'",
				PRINT_NON_NULL(values[0]));
		dxp_log_error("dxp_read_modules", info_string, status);
		return status;
	  }

	} else if (STREQ(entry_type, "mmu")) {

	  dxp_pick_filename(strlen(entry_type) + 1, line, strtemp);
	  values[0] = strtemp;

	  status = dxp_add_board_item(entry_type, values);

	  if (status != DXP_SUCCESS) {
		sprintf(info_string, "Unable to add MMU: '%s'",
				PRINT_NON_NULL(values[0]));
		dxp_log_error("dxp_read_modules", info_string, status);
		return status;
	  } 

	} else if (STREQ(entry_type, "default_fippi")) {

	  dxp_pick_filename(strlen(entry_type)+1, line, strtemp);
	  values[0] = strtemp;

	  status = dxp_add_board_item(entry_type, values);

	  if (status != DXP_SUCCESS) {
		sprintf(info_string, "Unable to add FIPPI: '%s'",
				PRINT_NON_NULL(values[0]));
		dxp_log_error("dxp_read_modules", info_string, status);
		return status;
	  }

	} else if (STREQ(entry_type, "fippi")) {

	  values[0] = strtok(NULL, delim);
			
	  n = 0;
	  dxp_strnstrm(line, entry_type, &n, &m);

	  /* Now m contains the location in the line after the 2nd entry_type */
	  n = m;
	  dxp_strnstrm(line, values[0], &n, &m);
			
	  /* Now search for the filename starting at m+1 */
	  dxp_pick_filename(m+1, line, strtemp);
	  values[1] = strtemp;

	  status = dxp_add_board_item(entry_type, values);
	  
	  if (status != DXP_SUCCESS) {
		sprintf(info_string, "Unable to add FIPPI '%s' to channel %s", 
				PRINT_NON_NULL(values[1]), PRINT_NON_NULL(values[0]));
		dxp_log_error("dxp_read_modules", info_string, status);
		return status;
	  }

	} else if (STREQ(entry_type, "default_param")) {

	  dxp_pick_filename(strlen(entry_type)+1, line, strtemp);
	  values[0] = strtemp;

	  status = dxp_add_board_item(entry_type, values);

	  if (status != DXP_SUCCESS) {
		sprintf(info_string, "Unable to add DSP Defaults: '%s'",
				PRINT_NON_NULL(values[0]));
		dxp_log_error("dxp_read_modules", info_string, status);
		return status;
	  }

	} else if (STREQ(entry_type, "param")) {

	  values[0] = strtok(NULL, delim);
			
	  n=0;
	  dxp_strnstrm(line, entry_type, &n, &m);

	  /* Now m contains the location in the line after the 2nd entry_type */
	  n = m;
	  dxp_strnstrm(line, values[0], &n, &m);
			
	  /* Now search for the filename starting at m+1 */
	  dxp_pick_filename(m+1, line, strtemp);
	  values[1] = strtemp;

	  status = dxp_add_board_item(entry_type, values);

	  if (status != DXP_SUCCESS) {
		sprintf(info_string, "Unable to add DSP Defaults '%s' to channel %s", 
				PRINT_NON_NULL(values[1]), PRINT_NON_NULL(values[0]));
		dxp_log_error("dxp_read_modules", info_string, status);
		return status;
	  }

	} else if (STREQ(entry_type, "end")) {

	  return DXP_SUCCESS;

	} else {

	  sprintf(info_string, "Unrecognized entry in 'modules' file: '%s'",
			  PRINT_NON_NULL(entry_type));
	  dxp_log_error("dxp_read_modules", info_string, DXP_UNKNOWN_ELEM);
	  return DXP_UNKNOWN_ELEM;
	}
  }
	
  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Read the file pointed to by DXP_MODULE:  This should have one line
 * for each module in the system (excluding comment lines -- those that
 * begin with a *)  The first field in the line is a 4 digit number that 
 * points to branch, crate and slot number of the module.  The second thru fifth
 * fields are non-negative integers for those channnels that are instrumented
 * (or to be readout)  These numbers are interpreted as detector elements.
 * A 0 or negative number in the second thru fifth fields indicates that the DXP
 * is unused (or non-existant)   This routine also fills CamChan[],Used[],
 * DetChan[], NumDxpMod and NumDxpChan these are global to this file.
 *
 ******************************************************************************/
int XERXES_API dxp_assign_channel(VOID)
{
  int status;
  char info_string[INFO_LEN];
  FILE *fp=NULL;

  unsigned int i, len;
  char strtmp[5];

  char newFile[MAXFILENAME_LEN];

  /* Get a file pointer to the modules configuration file */

  if (info->modules==NULL) {
	strcpy(strtmp,"NULL");
  } else {
	len = strlen(info->modules)<4 ? strlen(info->modules)+1 : 5;
	for (i=0;i<len;i++) strtmp[i] = (char) toupper(info->modules[i]);
  }

  if (!STREQ(strtmp,"NULL")) {

	if ((fp=dxp_find_file(info->modules,"r",newFile))==NULL){
	  status = DXP_OPEN_FILE;
	  sprintf(info_string,"could not open module configuration file : %s",
			  PRINT_NON_NULL(info->modules));
	  dxp_log_error("dxp_assign_channel",info_string,status);
	  return status;
	}

	sprintf(info_string, "fp = %p", fp);
	dxp_log_debug("dxp_assign_channel", info_string);

	/* Read in the configuration */

	if ((status=dxp_read_modules(fp))!=DXP_SUCCESS){
	  status = DXP_INITIALIZE;
	  sprintf(info_string,"Could not read in modules file : %s", PRINT_NON_NULL(info->modules));
	  dxp_log_error("dxp_assign_channel",info_string,status);
	  return status;
	}
  }

  sprintf(info_string, "fp = %p", fp);
  dxp_log_debug("dxp_assign_channel", info_string);

  if (fp != NULL) { xia_file_close(fp); }

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Routine to free the memory allocated to a Board Structure
 *
 ******************************************************************************/
static int XERXES_API dxp_free_board(Board* board)
	 /* Board *board;							Input: pointer to structure to free	*/
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];
  unsigned short i;

  if (board==NULL) {
	status = DXP_NOMEM;
	sprintf(info_string,"Board object unallocated:  can not free");
	dxp_log_error("dxp_free_board",info_string,status);
	return status;
  }

  /* Free the detChan array */
  if (board->detChan!=NULL)
	xerxes_md_free(board->detChan);

  /* Free the iostring entry */
  if (board->iostring!=NULL) 
	xerxes_md_free(board->iostring);

  /* Free the params array */
  if (board->params!=NULL) {
	for (i=0; i<board->nchan; i++) {
	  if (board->params[i]!=NULL) {
		xerxes_md_free(board->params[i]);
		board->params[i] = NULL;
	  }
	}
	xerxes_md_free(board->params);
  }

  /* Free the Chan_State Array*/
  if (board->chanstate!=NULL)
	xerxes_md_free(board->chanstate);

  /* Free the Dsp Array*/
  if (board->dsp!=NULL)
	xerxes_md_free(board->dsp);

  /* Free the Fippi Array*/
  if (board->fippi!=NULL)
	xerxes_md_free(board->fippi);

  /* Free the Defaults Array*/
  if (board->defaults!=NULL)
	xerxes_md_free(board->defaults);

  if (board->preamp != NULL) {
    xerxes_md_free(board->preamp);
  }

  if (board->user_fippi != NULL) {
    xerxes_md_free(board->user_fippi);
  }

  /* Free the Board structure */
  xerxes_md_free(board);
  board = NULL;

  return status;
}

/******************************************************************************
 *
 * Routine to free the memory allocated to an Interface Structure
 *
 ******************************************************************************/
static int XERXES_API dxp_free_iface(Interface *iface)
	 /* Interface *iface;					Input: pointer to structure to free	*/
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];

  if (iface ==NULL) {
	status = DXP_NOMEM;
	sprintf(info_string,"Interface object unallocated:  can not free");
	dxp_log_error("dxp_free_iface",info_string,status);
	return status;
  }

  if (iface->dllname != NULL) xerxes_md_free(iface->dllname);
  if (iface->ioname  != NULL) xerxes_md_free(iface->ioname);
  if (iface->funcs   != NULL) xerxes_md_free(iface->funcs);

  /* Free the Interface structure */
  xerxes_md_free(iface);
  iface = NULL;

  return status;
}

/******************************************************************************
 *
 * Routine to free the memory allocated to an Board_Info Structure
 *
 ******************************************************************************/
static int XERXES_API dxp_free_binfo(Board_Info *binfo)
	 /* Board_Info *binfo;					Input: pointer to structure to free	*/
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];

  if (binfo==NULL) {
	status = DXP_NOMEM;
	sprintf(info_string,"Board_Info object unallocated:  can not free");
	dxp_log_error("dxp_free_binfo",info_string,status);
	return status;
  }

  if (binfo->pointer != NULL) xerxes_md_free(binfo->pointer);
  if (binfo->name    != NULL) xerxes_md_free(binfo->name);
  if (binfo->funcs   != NULL) xerxes_md_free(binfo->funcs);

  /* Free the Board_Info structure */
  xerxes_md_free(binfo);
  binfo = NULL;

  return status;
}

/******************************************************************************
 *
 * Routine to free the memory allocated to an Dsp Structure
 *
 ******************************************************************************/
static int XERXES_API dxp_free_dsp(Dsp_Info *dsp)
	 /* Dsp_Info *dsp;					Input: pointer to structure to free	*/
{
  int status = DXP_SUCCESS;

  char info_string[INFO_LEN];


  if (dsp == NULL) {

	status = DXP_NOMEM;
	sprintf(info_string,"Dsp object unallocated:  can not free");
	dxp_log_error("dxp_free_dsp",info_string,status);
	return status;
  }

  sprintf(info_string, "dsp = %p", dsp);
  dxp_log_debug("dxp_free_dsp", info_string);

  if (dsp->filename != NULL) 
	xerxes_md_free(dsp->filename);
  if (dsp->data != NULL) 
	xerxes_md_free(dsp->data);

  if (dsp->params != NULL)
	dxp_free_params(dsp->params);

  /* Free the Dsp_Info structure */
  xerxes_md_free(dsp);
  dsp = NULL;

  return status;
}

/******************************************************************************
 *
 * Routine to free the memory allocated to an Fippi Structure
 *
 ******************************************************************************/
static int XERXES_API dxp_free_fippi(Fippi_Info *fippi)
	 /* Fippi_Info *fippi;					Input: pointer to structure to free	*/
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];

  if (fippi==NULL) {
	status = DXP_NOMEM;
	sprintf(info_string,"Fippi object unallocated:  can not free");
	dxp_log_error("dxp_free_fippi",info_string,status);
	return status;
  }

  if (fippi->filename!=NULL) 
	xerxes_md_free(fippi->filename);
  if (fippi->data!=NULL) 
	xerxes_md_free(fippi->data);

  /* Free the Fippi_Info structure */
  xerxes_md_free(fippi);
  fippi = NULL;

  return status;
}


/**
 * @brief Frees a System FiPPI structure previously allocated.
 */
XERXES_STATIC int dxp_free_system_fippi(System_FiPPI_Info *fippi)
{
  ASSERT(fippi != NULL);


  if (fippi->filename != NULL) {
    xerxes_md_free(fippi->filename);
  }

  if (fippi->data != NULL) {
    xerxes_md_free(fippi->data);
  }

  if (fippi->params != NULL) {
    dxp_free_params(fippi->params);
  }

  xerxes_md_free(fippi);

  return DXP_SUCCESS;
}


/******************************************************************************
 *
 * Routine to free the memory allocated to an Fippi Structure
 *
 ******************************************************************************/
static int XERXES_API dxp_free_params(Dsp_Params *params)
	 /* Dsp_Params *params;					Input: pointer to structure to free	*/
{
  int j,status=DXP_SUCCESS;
  char info_string[INFO_LEN];

  if (params==NULL) {
	status = DXP_NOMEM;
	sprintf(info_string,"Dsp_Params object unallocated:  can not free");
	dxp_log_error("dxp_free_params",info_string,status);
	return status;
  }

  if (params->parameters!=NULL) {
    for (j = 0; j < params->maxsym; j++) {
      if (params->parameters[j].pname!=NULL)
        xerxes_md_free(params->parameters[j].pname);
    }
    xerxes_md_free(params->parameters);
  }

  if (params->per_chan_parameters != NULL) {
    for (j = 0; j < params->maxsym; j++) {
      if (params->per_chan_parameters[j].pname != NULL) {
        xerxes_md_free(params->per_chan_parameters[j].pname);
      }
    }

  if (params->chan_offsets != NULL) {  
    xerxes_md_free(params->chan_offsets);
    }
    
    xerxes_md_free(params->per_chan_parameters);
  }

  /* Free the Dsp_Params structure */
  xerxes_md_free(params);
  params = NULL;

  return status;
}

/******************************************************************************
 *
 * Routine to free the memory allocated to an Dsp_Defaults Structure
 *
 ******************************************************************************/
static int XERXES_API dxp_free_defaults(Dsp_Defaults *defaults)
	 /* Dsp_Defaults *defaults;					Input: pointer to structure to free	*/
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];

  if (defaults==NULL) {
	status = DXP_NOMEM;
	sprintf(info_string,"Dsp_Defaults object unallocated:  can not free");
	dxp_log_error("dxp_free_defaults",info_string,status);
	return status;
  }

  if (defaults->filename!=NULL)
	xerxes_md_free(defaults->filename);
  if (defaults->data!=NULL)
	xerxes_md_free(defaults->data);
  if (defaults->params!=NULL) {
	dxp_free_params(defaults->params);
  }

  /* Free the Dsp_Defaults structure */
  xerxes_md_free(defaults);
  defaults = NULL;

  return status;
}

/******************************************************************************
 *
 * Routine to remove a board from the linked list.
 *
 ******************************************************************************/
int XERXES_API dxp_del_board(char* type, char* iolib, char* ifacelib, char* iostring)
	 /* char *type;					Input: board type, must match btype entry	*/
	 /* char *iolib;					Input: Library to use to perform IO			*/
	 /* char *ifacelib;				Input: Library to use for user spec routines	*/
	 /* char *iostring;				Input: string to pass to the iface library	*/
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];

  Board *current = system_head, *prev = NULL;

  /* First search thru the linked list and see if this Board exists
   * already exists */
  while (current!=NULL) {

	if (STREQ(current->btype->name, type)) {
	  /* Do the interface and iostring match? */
	  if (STREQ(current->iface->dllname, ifacelib)) {
		if (STREQ(current->iface->ioname, iolib)) {
		  if (STREQ(current->iostring, iostring)) {
			/* Found the board entry....now remove */
			if (current==system_head) {
			  system_head = current->next;
			} else {
			  prev->next = current->next;
			}
			dxp_free_board(current);
			return DXP_SUCCESS;
		  }
		}
	  }
	}
	prev = current;
	current = current->next;
  }

  status = DXP_NOMATCH;
  sprintf(info_string,"Board Type: %s using %s with id %s unknown",
		  PRINT_NON_NULL(type), PRINT_NON_NULL(ifacelib), PRINT_NON_NULL(iostring));
  dxp_log_error("dxp_del_board",info_string,status);
  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Routine to remove a board type from the linked list.
 *
 ******************************************************************************/
int XERXES_API dxp_del_btype(char* name)
	 /* char *name;					Input: board type	*/
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];

  Board_Info *current = btypes_head, *prev = NULL;

  /* First search thru the linked list and find the board type */
  while (current!=NULL) {

	if (STREQ(current->name, name)) {
	  /* Found the board type entry....now remove */
	  if (current==btypes_head) {
		btypes_head = current->next;
	  } else {
		prev->next = current->next;
	  }
	  dxp_free_binfo(current);
	  return DXP_SUCCESS;
	}
	prev = current;
	current = current->next;
  }

  status = DXP_NOMATCH;
  sprintf(info_string,"Board Type %s is unknown", PRINT_NON_NULL(name));
  dxp_log_error("dxp_del_btype",info_string,status);
  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Routine to remove a DSP structure from the linked list.
 *
 ******************************************************************************/
int XERXES_API dxp_del_dsp(char* filename)
	 /* char *filename;			Input: filename associated with DPS	*/
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];

  Dsp_Info *current = dsp_head, *prev = NULL;

  /* First search thru the linked list and find the DSP structure */
  while (current!=NULL) {

	if (STREQ(current->filename, filename)) {
	  /* Found the dsp entry....now remove */
	  if (current==dsp_head) {
		dsp_head = current->next;
	  } else {
		prev->next = current->next;
	  }
	  dxp_free_dsp(current);
	  return DXP_SUCCESS;
	}
	prev = current;
	current = current->next;
  }

  status = DXP_NOMATCH;
  sprintf(info_string,"DSP %s is unknown", PRINT_NON_NULL(filename));
  dxp_log_error("dxp_del_dsp",info_string,status);
  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Routine to remove a FIPPI structure from the linked list.
 *
 ******************************************************************************/
int XERXES_API dxp_del_fippi(char* filename)
	 /* char *filename;			Input: filename associated with FIPPI	*/
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];

  Fippi_Info *current = fippi_head, *prev = NULL;

  /* First search thru the linked list and find the FIPPI structure */
  while (current!=NULL) {

	if (STREQ(current->filename, filename)) {
	  /* Found the fippi entry....now remove */
	  if (current==fippi_head) {
		fippi_head = current->next;
	  } else {
		prev->next = current->next;
	  }
	  dxp_free_fippi(current);
	  return DXP_SUCCESS;
	}
	prev = current;
	current = current->next;
  }

  status = DXP_NOMATCH;
  sprintf(info_string,"FIPPI %s is unknown", PRINT_NON_NULL(filename));
  dxp_log_error("dxp_del_fippi",info_string,status);
  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Routine to remove a Defaults structure from the linked list.
 *
 ******************************************************************************/
int XERXES_API dxp_del_defaults(char* filename)
	 /* char *filename;			Input: filename associated with Defaults	*/
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];

  Dsp_Defaults *current = defaults_head, *prev = NULL;

  /* First search thru the linked list and find the FIPPI structure */
  while (current!=NULL) {

	if (STREQ(current->filename, filename)) {
	  /* Found the fippi entry....now remove */
	  if (current==defaults_head) {
		defaults_head = current->next;
	  } else {
		prev->next = current->next;
	  }
	  dxp_free_defaults(current);
	  return DXP_SUCCESS;
	}
	prev = current;
	current = current->next;
  }

  status = DXP_NOMATCH;
  sprintf(info_string,"Defaults %s is unknown", PRINT_NON_NULL(filename));
  dxp_log_error("dxp_del_defaults",info_string,status);
  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Routine to Load a new board into the linked list.  
 *
 ******************************************************************************/
int XERXES_API dxp_add_board(char* type, char* iolib, char* ifacelib, char* iostring, 
							 unsigned short* nchan, int* detChan)
	 /* char *type;					Input: board type, must match btype entry	*/
	 /* char *iolib;					Input: Tag specifying how to do IO w/ driver	*/
	 /* char *ifacelib;				Input: Tag to specify what MD to use			*/
	 /* char *iostring;				Input: string to pass to the iface library	*/
	 /* unsigned short *nchan;		Input: Number of channels on board			*/
	 /* int *detChan;				Input: Array of detector channel numbers		*/
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];
  unsigned short used;
  int ioChan;
  unsigned short j;

  Board_Info *btype = NULL;
  Interface *iface = NULL;
  Board *current = system_head, *prev = NULL;

  /* First search thru the linked list and see if this Board exists
   * already exists */
  while (current!=NULL) {

	/* Do the interface and iostring match? */
	if (STREQ(current->iface->dllname, ifacelib)) {
	  if (STREQ(current->iface->ioname, iolib)) {
		if (STREQ(current->iostring, iostring)) {
		  /* If we get here then this Board is already defined */
		  sprintf(info_string,"Board Type: %s using %s with id %s already exists",
				  PRINT_NON_NULL(type), PRINT_NON_NULL(ifacelib), PRINT_NON_NULL(iostring));
		  dxp_log_error("dxp_add_board",info_string,status);
		  return DXP_SUCCESS;
		}
	  }
	}
	prev = current;
	current = current->next;
  }

  /* Pack an information string for debugging purposes and error reporting */
  /* Make sure we haven't tried to allocate too many modules */
  sprintf(info_string," mod %i: %s with %i channels",numDxpMod,PRINT_NON_NULL(iostring),*nchan);
  if(numDxpMod == MAXDXP){
	sprintf(info_string,"Too many modules in config:MAXDXP = %i",
			MAXDXP);
	status = DXP_ARRAY_TOO_SMALL;
	dxp_log_error("dxp_add_board",info_string,status);
	return status;
  }
  dxp_log_info("dxp_add_board",info_string);

  /* 
   * Retrieve Pointer to the Board_Type structure
   */
  if ((status=dxp_get_btype(type, &btype))!=DXP_SUCCESS) {
	sprintf(info_string,"Board Type %s not defined", PRINT_NON_NULL(type));
	status = DXP_UNKNOWN_BTYPE;
	dxp_log_error("dxp_add_board",info_string,status);
	return status;
  }

  /* 
   * Add or Retrieve Pointer to the Interface structure
   */
  if ((status=dxp_add_iface(ifacelib, iolib, &iface))!=DXP_SUCCESS) {
	sprintf(info_string,"Unable to allocate Interface %s and iolib %s", 
			PRINT_NON_NULL(ifacelib), PRINT_NON_NULL(iolib));
	status = DXP_UNKNOWN_BTYPE;
	dxp_log_error("dxp_add_board",info_string,status);
	return status;
  }

  /* Set the NumDxpMod'th to have none of the channels used */
  used = 0;

  /* Pull out the information for where in the system the module sits 
   * and then open the crate via the xerxes_md_open() routine.  This routine is 
   * expected to return a number for ioChan that identifies this module's
   * location in the array used by the machine dependent functions to 
   * identify how to talk to this module. */
  if((status=iface->funcs->dxp_md_open(iostring,&ioChan))!=DXP_SUCCESS){
	sprintf(info_string,"Could not open Module %s",PRINT_NON_NULL(iostring));
	dxp_log_error("dxp_add_board",info_string,status);
	return status;
  }

  /* Now loop over the detector numbers to determine how many active channels
   * we have.  If detector number is positive then the channel is 
   * considered instrumented and the bit is set in the used value
   * for future reference.  Else if the number is negative the channel
   * is ignored */
        
  for(j=0;j<*nchan;j++){
	if (*detChan>=0) {
	  used |= 1<<j;
	  numDxpChan++;
	}
  }

  /* Create an entry in the linked list, checking to see if it is the first */
  if (system_head==NULL) {
	system_head = (Board *) xerxes_md_alloc(sizeof(Board));
	current = system_head;
  } else {
	current->next = (Board *) xerxes_md_alloc(sizeof(Board));
	current = current->next;
  }
  current->ioChan= ioChan;
  current->used	= used;
  memcpy(current->detChan, detChan, *nchan*sizeof(int));
  current->mod	= numDxpMod;
  current->btype	= btype;
  current->iface	= iface;
  current->iostring = (char *)
	xerxes_md_alloc((strlen(iostring)+1)*sizeof(char));
  strcpy(current->iostring, iostring);
  /* 
   * Initialize the interface.  Note that if multiple boards use the same
   * Interface, then this call is redundant on 2nd, etc...instances...but, 
   * make the call anyway, in case in the future we wish to enable different
   * interfaces for each board rather than the current limitation of essentially
   * one interface per board type...there are many implications to implementing 
   * this feature though...
   */
  current->btype->funcs->dxp_init_driver(iface);
  current->next	= NULL;

  for (j=0;j<*nchan;j++) {
	current->dsp[j] = NULL;
	current->params[j] = NULL;
	current->fippi[j] = NULL;
	current->defaults[j] = NULL;
  }

  /* We seem to have a new module in the system...increment the global counter */
  numDxpMod++;

  return status;
}

/******************************************************************************
 *
 * Routine to Retrieve the correct Board_Info structure from the 
 * global linked list...match the type name.
 *
 ******************************************************************************/
static int XERXES_API dxp_get_btype(char* name, Board_Info** match)
	 /* char *name;					Input: name of the board type			*/
	 /* Board_Info **match;			Output: pointer the correct structure	*/
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];

  Board_Info *current = btypes_head;

  /* First search thru the linked list and see if this configuration 
   * already exists */
  while (current!=NULL) {
	/* Does the filename match? */
	if (STREQ(current->name, name)) {
	  *match = current;
	  return status;
	}
	current = current->next;
  }

  status = DXP_UNKNOWN_BTYPE;
  sprintf(info_string,"Board Type %s unknown, please define",PRINT_NON_NULL(name));
  dxp_log_error("dxp_get_btype",info_string,status);
  return status;
}

/******************************************************************************
 *
 * Routine to Load a new board Type into the linked list.  Return pointer
 * to the member corresponding to the new board type.
 *
 ******************************************************************************/
int XERXES_API dxp_add_btype(char* name, char* pointer, char* dllname)
	 /* char *name;							Input: name of the new board type	*/
	 /* char *pointer;						Input: extra information filename	*/
	 /* char *dllname;						Input: name of the board type library*/
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];

  Board_Info *prev = NULL;
  Board_Info *current = btypes_head;
  char *ctemp;


  sprintf(info_string, "name = '%s', pointer = '%s', dllname = '%s'",
		  PRINT_NON_NULL(name), PRINT_NON_NULL(pointer), PRINT_NON_NULL(dllname));
  dxp_log_debug("dxp_add_btype", info_string);

  /* Assign unused inputs to get rid of compiler warnings */
  ctemp = dllname;

  /* First search thru the linked list and see if this configuration 
   * already exists */
  while (current!=NULL) {
	/* Does the filename match? */
	if (STREQ(current->name, name)) {
	  sprintf(info_string,"Board Type: %s already exists",PRINT_NON_NULL(name));
	  dxp_log_error("dxp_add_btype",info_string,status);
	  return DXP_SUCCESS;
	}
	prev = current;
	current = current->next;
  }

  /* No match in the existing list, add a new entry */
  if (btypes_head==NULL) {
	/* Initialize the Header entry in the btypes linked list */
	btypes_head = (Board_Info*) xerxes_md_alloc(sizeof(Board_Info));
	btypes_head->type = 0;
	current = btypes_head;
	/* Assign the head entry of board types to the global structure */
	info->btypes = btypes_head;
  } else {
	/* Not First time, allocate memory for next member of linked list */
	prev->next = (Board_Info*) xerxes_md_alloc(sizeof(Board_Info));
	current = prev->next;
	current->type = prev->type+1;
  }
  /* Set funcs pointer to NULL, will be set by add_btype_library() */
  current->funcs = NULL;
  /* Set the next pointer to NULL */
  current->next = NULL;
  /* This a new board type. */
  current->name = (char *) xerxes_md_alloc((strlen(name)+1)*sizeof(char));
  /* Copy the tag into the structure.  End the string with 
   * a NULL character. */
  strcpy(current->name, name);

  /* Check to make sure a pointer was defined */
  if (pointer!=NULL) {
	/* Now copy the pointer to use for this type configuration */
	current->pointer = (char *) xerxes_md_alloc((strlen(pointer)+1)*sizeof(char));
	/* Copy the filename into the structure.  End the string with 
	 * a NULL character. */
	strcpy(current->pointer, pointer);
  } else {
	current->pointer = NULL;
  }

  /* Load function pointers for local use */
  if ((status=dxp_add_btype_library(current))!=DXP_SUCCESS) {
	sprintf(info_string,"Problems loading functions for board type %s",PRINT_NON_NULL(current->name));
	dxp_log_error("dxp_add_btype",info_string,status);
	return DXP_SUCCESS;
  }
  /* Initialize the utilty routines in the library */
  current->funcs->dxp_init_utils(utils);

  /* All done */
  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Initialize the FIPPI data structures to an empty state
 *
 ******************************************************************************/
int XERXES_API dxp_get_board_type(int *detChan, char *name)
{
  int status;
  char info_string[INFO_LEN];
  int modChan;
  /* Pointer to the chosen Board */
  Board *chosen=NULL;

  /* Get the ioChan and modChan number that matches the detChan */
  if((status=dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS){
	sprintf(info_string,"Error finding detector number %d",*detChan);
	dxp_log_error("dxp_get_board_type",info_string,status);
	return status;
  }
	
  strncpy(name, chosen->btype->name, MAXBOARDNAME_LEN);
	
  return status;
}

/******************************************************************************
 *
 * Routine to Load a board library and create pointers to the 
 * routines in the board library.
 *
 ******************************************************************************/
static int XERXES_API dxp_add_btype_library(Board_Info* current)
	 /* Board_Info *current;			Input: pointer to structure	*/
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];

  /* Allocate the memory for the function pointer structure */
  current->funcs = (Functions *) xerxes_md_alloc(sizeof(Functions));

#ifndef EXCLUDE_DXP4C2X
	if ((STREQ(current->name,"dxp4c2x")) || (STREQ(current->name,"dxp2x"))) {
	  dxp_init_dxp4c2x(current->funcs);
	} else 
#endif
#ifndef EXCLUDE_DXPX10P
	  if (STREQ(current->name,"dxpx10p")) {
		dxp_init_dxpx10p(current->funcs);
	  } else
#endif
#ifndef EXCLUDE_UDXPS
		  if (STREQ(current->name, "udxps")) {
			dxp_init_udxps(current->funcs);
		  } else
#endif
#ifndef EXCLUDE_UDXP
			if (STREQ(current->name, "udxp")) {
			  dxp_init_udxp(current->funcs);
			} else
#endif
#ifndef EXCLUDE_XMAP
				if (STREQ(current->name, "xmap")) {
				  dxp_init_xmap(current->funcs);
				} else
#endif /* EXCLUDE_XMAP */
#ifndef EXCLUDE_MERCURY
          if (STREQ(current->name, "mercury")) {
            dxp_init_mercury(current->funcs);
          } else
#endif /* EXCLUDE_MERCURY */
#ifndef EXCLUDE_VEGA
          if (STREQ(current->name, "vega")) {
            dxp_init_vega(current->funcs);
          } else
#endif /* EXCLUDE_VEGA */
				{
				  status = DXP_UNKNOWN_BTYPE;
				  sprintf(info_string, "Unknown board type %s: unable to "
						  "load pointers", PRINT_NON_NULL(current->name));
				  dxp_log_error("dxp_add_btype_library", info_string, status);
				  return status;
				}
	
  /* All done */
  return DXP_SUCCESS;
}


/** @brief Create a new DSP object and initialize it.
 *
 * This routine creates a new DSP structure representing the data in @a filename
 * and then assigns it to the global DSP list and the passed in pointer.
 */
XERXES_STATIC int dxp_add_dsp(char* filename, Board_Info* board,
							  Dsp_Info** passed)
{
  int status;
  
  char info_string[INFO_LEN];
  
  unsigned int i;

  Dsp_Info *new_dsp = NULL;
  Dsp_Info *prev    = NULL;
  Dsp_Info *current = dsp_head;


  ASSERT(filename != NULL);
  ASSERT(board    != NULL);
  ASSERT(passed   != NULL);


  /* If this DSP has already been loaded then we can just point at it
   * instead of re-loading it.
   */
  while (current!=NULL) {

	if (STREQ(current->filename, filename)) {
    sprintf(info_string, "Skipping creation of new DSP entry for '%s', since "
            "we already have a copy loaded at %p", filename, current);
    dxp_log_info("dxp_add_dsp", info_string);

	  *passed = current;
	  return DXP_SUCCESS;
	}

	prev = current;
	current = current->next;
  }

  new_dsp = (Dsp_Info *)xerxes_md_alloc(sizeof(Dsp_Info));

  if (!new_dsp) {
	sprintf(info_string, "Error allocating %d bytes for 'new_dsp'",
			sizeof(Dsp_Info));
	dxp_log_error("dxp_add_dsp", info_string, DXP_NOMEM);
	return DXP_NOMEM;
  }

  new_dsp->params = (Dsp_Params *)xerxes_md_alloc(sizeof(Dsp_Params));

  if (!new_dsp->params) {
	dxp_free_dsp(new_dsp);

	sprintf(info_string, "Error allocating %d bytes for 'new_dsp->params'",
			sizeof(Dsp_Params));
	dxp_log_error("dxp_add_dsp", info_string, DXP_NOMEM);
	return DXP_NOMEM;
  }

  new_dsp->filename = (char *)xerxes_md_alloc(strlen(filename) + 1);

  if (!new_dsp->filename) {
	dxp_free_dsp(new_dsp);

	sprintf(info_string, "Error allocating %d bytes for 'new_dsp->filename'",
			strlen(filename) + 1);
	dxp_log_error("dxp_add_dsp", info_string, DXP_NOMEM);
	return DXP_NOMEM;
  }

  strncpy(new_dsp->filename, filename, strlen(filename) + 1);

  status = board->funcs->dxp_get_dspinfo(new_dsp);

  if (status != DXP_SUCCESS) {
	dxp_free_dsp(new_dsp);

	sprintf(info_string, "Unable to get DSP information for %s", filename);
	dxp_log_error("dxp_add_dsp", info_string, status);
	return status;
  }

  new_dsp->data = (unsigned short *)xerxes_md_alloc(new_dsp->maxproglen *
													sizeof(unsigned short));

  if (!new_dsp->data) {
	dxp_free_dsp(new_dsp);

	sprintf(info_string, "Error allocating %d bytes for 'new_dsp->data'",
			new_dsp->maxproglen * sizeof(unsigned short));
	dxp_log_error("dxp_add_dsp", info_string, DXP_NOMEM);
	return DXP_NOMEM;
  }

  new_dsp->params->parameters = (Parameter *)xerxes_md_alloc(
													   new_dsp->params->maxsym *
													   sizeof(Parameter));

  if (!new_dsp->params->parameters) {
	dxp_free_dsp(new_dsp);

	sprintf(info_string, "Error allocating %d bytes for 'new_dsp->params->"
			"parameter'", new_dsp->params->maxsym * sizeof(Parameter));
	dxp_log_error("dxp_add_dsp", info_string, DXP_NOMEM);
	return DXP_NOMEM;
  }
							
  new_dsp->params->per_chan_parameters = (Parameter *)xerxes_md_alloc(
													   new_dsp->params->maxsym *
													   sizeof(Parameter));

  if (!new_dsp->params->per_chan_parameters) {
	dxp_free_dsp(new_dsp);

	sprintf(info_string, "Error allocating %d bytes for 'new_dsp->params->"
			"per_chan_parameters'", new_dsp->params->maxsym * sizeof(Parameter));
	dxp_log_error("dxp_add_dsp", info_string, DXP_NOMEM);
	return DXP_NOMEM;
  }
								 
  for (i = 0; i < new_dsp->params->maxsym; i++) {
	new_dsp->params->parameters[i].pname  = (char *)xerxes_md_alloc(
													 new_dsp->params->maxsymlen);

	if (!new_dsp->params->parameters[i].pname) {
	  dxp_free_dsp(new_dsp);

	  sprintf(info_string, "Error allocating %d bytes for 'new_dsp->params->"
			  "parameters[%d].pname'", new_dsp->params->maxsymlen, i);
	  dxp_log_error("dxp_add_dsp", info_string, DXP_NOMEM);
	}

	new_dsp->params->parameters[i].address = 0;
	new_dsp->params->parameters[i].access  = 0;
	new_dsp->params->parameters[i].lbound  = 0;
	new_dsp->params->parameters[i].ubound  = 0;

	new_dsp->params->per_chan_parameters[i].pname = (char *)xerxes_md_alloc(
													 new_dsp->params->maxsymlen);
	
	if (!new_dsp->params->per_chan_parameters[i].pname) {
	  dxp_free_dsp(new_dsp);

	  sprintf(info_string, "Error allocating %d bytes for 'new_dsp->params->"
			  "parameters[%d].pname'", new_dsp->params->maxsymlen, i);
	  dxp_log_error("dxp_add_dsp", info_string, DXP_NOMEM);
	  return DXP_NOMEM;
	}

  }

  /* Initialize pointers not used by all devices */
  new_dsp->params->chan_offsets = NULL;
  new_dsp->params->n_per_chan_symbols = 0;

  sprintf(info_string, "Preparing to get DSP configuration: %s",
		  new_dsp->filename);
  dxp_log_debug("dxp_add_dsp", info_string);

  status = board->funcs->dxp_get_dspconfig(new_dsp);

  if (status != DXP_SUCCESS) {
	new_dsp->params->nsymbol = new_dsp->params->maxsym;
	dxp_free_dsp(new_dsp);

	sprintf(info_string, "Error loading DSP file %s", filename);
	dxp_log_error("dxp_add_dsp", info_string, status);
	return status;
  }

  /* Add the new DSP code to the global list. */
  if (!dsp_head) {
	dsp_head = new_dsp;

  } else {
	prev->next = new_dsp;
  }

  new_dsp->next = NULL;

  sprintf(info_string, "n_per_chan_symbols = %u",
          new_dsp->params->n_per_chan_symbols);
  dxp_log_debug("dxp_add_dsp", info_string);

  *passed = new_dsp;

  return DXP_SUCCESS;
}


/******************************************************************************
 *
 * Routine to Load a FIPPI configuration file into the Fippi_Info linked list
 * or find the matching member of the list and return a pointer to it
 *
 ******************************************************************************/
static int XERXES_API dxp_add_fippi(char* filename, Board_Info* board, Fippi_Info** passed)
	 /* char *filename;					Input: filename of the FIPPI program	*/
	 /* Board_Info *board;				Input: need the type of board		*/
	 /* Fippi_Info **passed;				Output: Pointer to the structure		*/
{
  int status;
  char info_string[INFO_LEN];

  Fippi_Info *temp_fippi = NULL, *prev= NULL;
  Fippi_Info *current = fippi_head;

  /* First search thru the linked list and see if this configuration 
   * already exists */
  while (current!=NULL) {
	/* Does the filename match? */
	if (STREQ(current->filename, filename)) {
	  *passed = current;
	  return DXP_SUCCESS;
	}
	prev = current;
	current = current->next;
  }

  /* No match in the existing list, add a new entry */
  if (fippi_head==NULL) {
	/* First time, allocate memory for head of linked list */
	fippi_head = (Fippi_Info *) xerxes_md_alloc(sizeof(Fippi_Info));
	current = fippi_head;
  } else {
	/* Not First time, allocate memory for next member of linked list */
	prev->next = (Fippi_Info *) xerxes_md_alloc(sizeof(Fippi_Info));
	current = prev->next;
  }
  /* Set the next pointer to NULL */
  current->next = NULL;
  current->filename = (char *) xerxes_md_alloc((strlen(filename)+1)*sizeof(char));
  strcpy(current->filename, filename);

  /* Now allocate memory and fill the program information 
   * with the driver routine */
  temp_fippi = (Fippi_Info *) xerxes_md_alloc(sizeof(Fippi_Info));
  temp_fippi->filename = (char *) xerxes_md_alloc((strlen(filename)+1)*sizeof(char));
  strcpy(temp_fippi->filename, filename);

  /* Fill in the default lengths */
  if ((status=board->funcs->dxp_get_fipinfo(temp_fippi))!=DXP_SUCCESS) {
	/* Free up memory, since there was an error */
	xerxes_md_free(temp_fippi);
	xerxes_md_free(current->filename);
	xerxes_md_free(current);
	current = NULL;

	if (prev != NULL) prev->next = NULL;
	if (prev == NULL) fippi_head = NULL;

	/* Write out error message */
	sprintf(info_string,"Unable to get FIPPI information");
	dxp_log_error("dxp_add_fippi",info_string,status);
	return status;
  }

  /* Finish allocating memory */
  temp_fippi->data = (unsigned short *) 
	xerxes_md_alloc((temp_fippi->maxproglen) * sizeof(unsigned short));

  if (!temp_fippi->data) {
	sprintf(info_string, "Error allocating %u bytes of memory for "
			"temp_fippi->data", temp_fippi->maxproglen * sizeof(unsigned short));
	dxp_log_error("dxp_add_fippi", info_string, DXP_NOMEM);

	dxp_free_fippi(temp_fippi);
	xerxes_md_free(current->filename);
	xerxes_md_free(current);

	if (prev != NULL) {
	  prev->next = NULL;
	}

	if (prev == NULL) {
	  fippi_head = NULL;
	}

	return DXP_NOMEM;
  }

  status = board->funcs->dxp_get_fpgaconfig(temp_fippi);

  if (status != DXP_SUCCESS) {
	dxp_free_fippi(temp_fippi);
	xerxes_md_free(current->filename);
	xerxes_md_free(current);
	current = NULL;

	if (prev != NULL) prev->next = NULL;
	if (prev == NULL) fippi_head = NULL;

	/* Write out error message */
	sprintf(info_string,"Unable to load new FIPPI file");
	dxp_log_error("dxp_add_fippi",info_string,status);
	return status;
  }

  /* Now Copy the fippi_temp structure into the fippi structure, this will
   * move the memory allocated in the driver layer into the permanent
   * midlevel layer */
  current->maxproglen = temp_fippi->maxproglen;
  current->proglen = temp_fippi->proglen;
  current->data = (unsigned short *) xerxes_md_alloc((temp_fippi->proglen)*sizeof(unsigned short));
  memcpy(current->data, temp_fippi->data, (temp_fippi->proglen)*sizeof(unsigned short));

  /* Ok, the memory is copied into the local library memory space, now free the memory
   * used by temp_fippi */
  dxp_free_fippi(temp_fippi);

  /* Now assign the passed variable to the proper memory */
  *passed = current;

  /* All done */
  return DXP_SUCCESS;
}


/**
 * @brief Allocates memory for a new System FiPPI and returns it.
 */
XERXES_STATIC int dxp_add_system_fippi(char *filename, Board_Info *b,
                                       System_FiPPI_Info **fippi)
{
  int status;

  unsigned short i;

  System_FiPPI_Info *current = system_fippi_head;
  System_FiPPI_Info *prev    = NULL;

  char info_string[INFO_LEN];


  ASSERT(filename != NULL);
  ASSERT(b != NULL);
  ASSERT(fippi != NULL);

  
  while (current != NULL) {
    if (STREQ(current->filename, filename)) {
      *fippi = current;
      return DXP_SUCCESS;
    }

    prev    = current;
    current = current->next;
  }

  /* Create new System FiPPI. */
  *fippi = xerxes_md_alloc(sizeof(System_FiPPI_Info));

  if (*fippi == NULL) {
    sprintf(info_string, "Error allocating %d bytes for '*fippi'",
            sizeof(System_FiPPI_Info));
    dxp_log_error("dxp_add_system_fippi", info_string, DXP_NOMEM);
    return DXP_NOMEM;
  }

  (*fippi)->next = NULL;

  (*fippi)->filename = xerxes_md_alloc(strlen(filename) + 1);
  
  if ((*fippi)->filename == NULL) {
    sprintf(info_string, "Error allocating %d bytes for '*fippi->filename'",
            strlen(filename) + 1);
    dxp_log_error("dxp_add_system_fippi", info_string, DXP_NOMEM);
    return DXP_NOMEM;
  }
  
  strcpy((*fippi)->filename, filename);

  (*fippi)->params = xerxes_md_alloc(sizeof(Dsp_Params));

  if ((*fippi)->params == NULL) {
    dxp_free_system_fippi(*fippi);
    sprintf(info_string, "Error allocating %d bytes for '*fippi->params'",
            sizeof(Dsp_Params));
    dxp_log_error("dxp_add_system_fippi", info_string, DXP_NOMEM);
    return DXP_NOMEM;
  }

  (*fippi)->params->per_chan_parameters = NULL;
  (*fippi)->params->n_per_chan_symbols  = 0;

  status = b->funcs->dxp_get_fipinfo(*fippi);

  if (status != DXP_SUCCESS) {
    dxp_free_system_fippi(*fippi);
    dxp_log_error("dxp_add_system_fippi", "Error getting basic FiPPI "
                  "parameters for the device", status);
    return status;
  }
  
  (*fippi)->data = xerxes_md_alloc((*fippi)->max_data_len);

  if ((*fippi)->data == NULL) {
    dxp_free_system_fippi(*fippi);
    sprintf(info_string, "Error allocating %ld bytes for '*fippi->data'",
            (*fippi)->max_data_len);
    dxp_log_error("dxp_add_system_fippi", info_string, DXP_NOMEM);
    return DXP_NOMEM;
  }

  (*fippi)->params->parameters = xerxes_md_alloc((*fippi)->params->maxsym *
                                                 sizeof(Parameter));

  if ((*fippi)->params->parameters == NULL) {
    dxp_free_system_fippi(*fippi);
    sprintf(info_string, "Error allocating %d bytes for "
            "'*fippi->params->parameters'", (*fippi)->params->maxsym *
            sizeof(Parameter));
    dxp_log_error("dxp_add_system_fippi", info_string, DXP_NOMEM);
    return DXP_NOMEM;
  }

  for (i = 0; i < (*fippi)->params->maxsym; i++) {
    (*fippi)->params->parameters[i].pname =
      xerxes_md_alloc((*fippi)->params->maxsymlen);

    if ((*fippi)->params->parameters[i].pname == NULL) {
      dxp_free_system_fippi(*fippi);
      sprintf(info_string, "Error allocating %d bytes for "
              "'*fippi->params->parameters[%d].pname'",
              (*fippi)->params->maxsymlen, i);
      dxp_log_error("dxp_add_system_fippi", info_string, DXP_NOMEM);
      return DXP_NOMEM;
    }
  }
                                                 
  status = b->funcs->dxp_get_fpgaconfig(*fippi);

  if (status != DXP_SUCCESS) {
    dxp_free_system_fippi(*fippi);
    sprintf(info_string, "Error adding '%s' as a System FiPPI to the system",
            filename);
    dxp_log_error("dxp_add_system_fippi", info_string, status);
    return status;
  }

  /* Update global linked list with the new System FiPPI. */
  if (system_fippi_head == NULL) {
    system_fippi_head = *fippi;
  } else {
    prev->next = *fippi;
  }

  return DXP_SUCCESS;
}


/******************************************************************************
 *
 * Routine to Load a DSP parameter defaults file into the Dsp_Defaults linked 
 * list or find the matching member of the list and return a pointer to it
 *
 ******************************************************************************/
static int dxp_add_defaults(char* filename, Board_Info* board,
                            Dsp_Defaults** passed)
{
  int status;
  char info_string[INFO_LEN];
  unsigned int i, len;

  Dsp_Defaults *temp_defaults = NULL, *prev = NULL;
  Dsp_Defaults *current = defaults_head;

  char strtmp[5];

  /* First search thru the linked list and see if this configuration 
   * already exists */
  while (current!=NULL) {
    /* Does the filename match? */
    if (STREQ(current->filename, filename)) {
      *passed = current;
      return DXP_SUCCESS;
    }
    prev = current;
    current = current->next;
  }

  /* No match in the existing list, add a new entry */
  if (defaults_head==NULL) {
    /* First time, allocate memory for head of linked list */
    defaults_head = (Dsp_Defaults *) xerxes_md_alloc(sizeof(Dsp_Defaults));
    current = defaults_head;
  } else {
    /* Not First time, allocate memory for next member of linked list */
    prev->next = (Dsp_Defaults *) xerxes_md_alloc(sizeof(Dsp_Defaults));
    current = prev->next;
  }
  /* Set the next pointer to NULL */
  current->next = NULL;
  current->filename = (char *) xerxes_md_alloc((strlen(filename)+1)*sizeof(char));
  current->filename = strcpy(current->filename, filename);

  /* Now allocate memory and fill the program information 
   * with the driver routine */
  temp_defaults = (Dsp_Defaults *) xerxes_md_alloc(sizeof(Dsp_Defaults));
  temp_defaults->params = (Dsp_Params *) xerxes_md_alloc(sizeof(Dsp_Params));
  temp_defaults->filename = (char *) xerxes_md_alloc((strlen(filename)+1)*sizeof(char));
  strcpy(temp_defaults->filename, filename);

  /* Fill in the default lengths */
  if ((status=board->funcs->dxp_get_defaultsinfo(temp_defaults))!=DXP_SUCCESS) {
    /* Free up memory, since there was an error */
    dxp_free_defaults(temp_defaults);
    xerxes_md_free(current->filename);
    xerxes_md_free(current);
    current = NULL;

    if (prev != NULL) prev->next    = NULL;
    if (prev == NULL) defaults_head = NULL;

    /* Write out error message */
    sprintf(info_string,"Unable to get DSP Defaults information");
    dxp_log_error("dxp_add_defaults",info_string,status);
    return status;
  }

  /* Finish allocating memory */
  temp_defaults->data = (unsigned short *) xerxes_md_alloc(temp_defaults->params->maxsym*sizeof(unsigned short));

  /** temp_defaults->data should probably be set to something!! 11/13/01--PJF */
  memset((void *)temp_defaults->data, 0x0000, sizeof(temp_defaults->data)); 

  temp_defaults->params->parameters = (Parameter *) 
    xerxes_md_alloc(temp_defaults->params->maxsym*sizeof(Parameter));
  for (i=0;i<temp_defaults->params->maxsym;i++)
    temp_defaults->params->parameters[i].pname = (char *) xerxes_md_alloc(temp_defaults->params->maxsymlen*sizeof(char));

  if (filename==NULL) {
    strcpy(strtmp, "NULL");
  } else {
    len = strlen(filename)<4 ? strlen(filename)+1 : 5;
    for (i=0;i<len;i++) strtmp[i] = (char) toupper(filename[i]);
  }
  if (!STREQ(strtmp,"NULL")) {
    /* If not a NULL entry, then call the primitive function */
    if ((status=board->funcs->dxp_get_dspdefaults(temp_defaults))!=DXP_SUCCESS) {
      /* Free up memory, since there was an error */
      temp_defaults->params->nsymbol = temp_defaults->params->maxsym;
      dxp_free_defaults(temp_defaults);
      xerxes_md_free(current->filename);
      xerxes_md_free(current);
      current = NULL;
	  
      if (prev != NULL)	prev->next = NULL;
      if (prev == NULL) defaults_head = NULL;

      sprintf(info_string,"Unable to load new Defaults file");
      dxp_log_error("dxp_add_defaults",info_string,status);
      return status;
    }
  } else {
    temp_defaults->params->nsymbol = 0;
  }

  /* Now Copy the temp_defaults structure into the defaults structure, this will
   * move the memory allocated in the driver layer into the permanent
   * midlevel layer */
  current->params = (Dsp_Params *) xerxes_md_alloc(sizeof(Dsp_Params));
  current->params->nsymbol   = temp_defaults->params->nsymbol;
  current->params->maxsymlen = temp_defaults->params->maxsymlen;
  current->params->maxsym    = temp_defaults->params->maxsym;
  if (temp_defaults->params->nsymbol != 0) {
    current->params->parameters = (Parameter *) 
      xerxes_md_alloc((temp_defaults->params->nsymbol)*sizeof(Parameter));
    for (i=0;i<current->params->nsymbol;i++) {
      current->params->parameters[i].pname = (char *) 
        xerxes_md_alloc((strlen(temp_defaults->params->parameters[i].pname)+1)*sizeof(char));
      strcpy(current->params->parameters[i].pname, temp_defaults->params->parameters[i].pname);
      current->params->parameters[i].address = 0;
      current->params->parameters[i].access = 0;
      current->params->parameters[i].lbound = 0;
      current->params->parameters[i].ubound = 0;
    }
    current->data = (unsigned short *) xerxes_md_alloc((temp_defaults->params->nsymbol)*sizeof(unsigned short));
    memcpy(current->data, temp_defaults->data, (temp_defaults->params->nsymbol)*sizeof(unsigned short));
	
  } else {
    current->params->parameters = NULL;
    current->data = NULL;
    current->params->per_chan_parameters = NULL;
  }

  /* Ok, the memory is copied into the local library memory space, now free the memory
   * used by temp_defaults_dsp */
  temp_defaults->params->nsymbol = temp_defaults->params->maxsym;
  temp_defaults->params->per_chan_parameters = NULL;
  dxp_free_defaults(temp_defaults);

  /* Now assign the passed variable to the proper memory */
  *passed = current;

  /* All done */
  return DXP_SUCCESS;
}


/******************************************************************************
 *
 * Routine to Load a new DLL library and return a pointer to the proper
 * Libs structure
 *
 ******************************************************************************/
static int XERXES_API dxp_add_iface(char* dllname, char* iolib, Interface** iface)
	 /* char *dllname;					Input: filename of the interface DLL		*/
	 /* char *iolib;						Input: name of lib to talk to the board	*/
	 /* Interface **iface;				Output: Pointer to the structure			*/
{
  int status;
  char info_string[INFO_LEN];
  unsigned int maxMod = MAXDXP;

  Interface *prev = NULL;
  Interface *current = iface_head;

  /* First search thru the linked list and see if this configuration 
   * already exists */
  while (current!=NULL) {
	/* Does the filename match? */
	if (STREQ(current->dllname, dllname)) {
	  *iface = current;
	  return DXP_SUCCESS;
	}
	prev = current;
	current = current->next;
  }

  /* No match in the existing list, add a new entry */
  if (iface_head==NULL) {
	/* First time, allocate memory for head of linked list */
	iface_head = (Interface *) xerxes_md_alloc(sizeof(Interface));
	current = iface_head;
  } else {
	/* Not First time, allocate memory for next member of linked list */
	prev->next = (Interface *) xerxes_md_alloc(sizeof(Interface));
	current = prev->next;
  }
  /* Set the next pointer to NULL */
  current->next = NULL;
  /* Copy the name of the DLL into the structure */
  current->dllname = (char *) xerxes_md_alloc((strlen(dllname)+1)*sizeof(char));
  strcpy(current->dllname, dllname);
  /* Copy in the ioname variable for future reference */
  current->ioname = (char *) xerxes_md_alloc((strlen(iolib)+1)*sizeof(char));
  strcpy(current->ioname, iolib);

  /* Allocate memory for the function structure */
  current->funcs = (Xia_Io_Functions *) xerxes_md_alloc(sizeof(Xia_Io_Functions));
  /* Retrieve the function pointers from the MD appropriate MD routine */
  dxp_md_init_io(current->funcs, current->dllname);
	
  /* Now initialize the library */
  if ((status = 
	   current->funcs->dxp_md_initialize(&maxMod, 
										 current->ioname))!=DXP_SUCCESS){
	status = DXP_INITIALIZE;
	sprintf(info_string,"Could not initialize %s",PRINT_NON_NULL(current->dllname));
	dxp_log_error("dxp_add_iface",info_string,status);
	return status;
  }

  /* Now assign the passed variable to the proper memory */
  *iface = current;

  /* All done */
  return DXP_SUCCESS;
}
/******************************************************************************
 *
 * Routine to search out the filename in a string
 *
 ******************************************************************************/
static int XERXES_API dxp_pick_filename(unsigned int start, char* lline, char* filename)
	 /* unsigned int start;				*/
	 /* char *lline;						*/
	 /* char *filename;					*/
{
  unsigned int finish, len;
  int status=DXP_SUCCESS;

  /* Get the length of the line */
  len = strlen(lline);
  /* Unfort, need to allow spaces in the filenames.
   * so count forward in the line till out of spaces, this is the start of the 
   * file name */
  for(;start<len;start++) 
	if(isspace(lline[start])==0) break;				/* find first character in path */ 
  /* Now count backwards from the end of the line till you hit a non-space */
  for(finish=len;finish>start;finish--) 
	if(isgraph(lline[finish])!=0) break;				/* find last character in name  */ 
  /* Determine Length so i can NULL terminate the string */
  len = finish-start+1;
  /* Copy the filename into the structure.  End the string with 
   * a NULL character. */
  filename = strncpy(filename, lline+start, len);
  filename[len] = '\0';

  return status;
}
/******************************************************************************
 * 
 * Routine to search for a token within string, starting at location
 * n and returning the location of the last character match in string
 * as m.
 * 
 ******************************************************************************/
static int XERXES_API dxp_strnstrm(char* string, char* ltoken, unsigned int* n, 
								   unsigned int* m)
	 /* char *string;			Input: String to be searched			*/
	 /* char *ltoken;				Input: String to match				*/
	 /* unsigned int *n;			Input: Starting location of search	*/
	 /* unsigned int *m;			Output: Ending location of the search*/
{
  unsigned int i;
  char info_string[INFO_LEN];
  int status = DXP_SUCCESS;

  /* start the search */
  for (i=*n;i<strlen(string);i++) {
	if (string[i]==ltoken[0]) {
	  /* Found a match for the first character, now match rest */
	  if (strncmp(string+i, ltoken, strlen(ltoken))==0) {
		/* Found match for string, return the ending position */
		*m = i + strlen(ltoken);
		return status;
	  }
	}
  }

  status = DXP_NOMATCH;
  sprintf(info_string,"Could not match string: %s",PRINT_NON_NULL(ltoken));
  dxp_log_error("dxp_strnstrm",info_string,status);
  return status;

}


/** @brief Download all FPGAs, DSPs and known parameter sets to the hardware.
 *
 */
XERXES_EXPORT int XERXES_API dxp_user_setup(void)
{

  int status;
  int chan;
  int detChan;

  char info_string[INFO_LEN];



  Board *current = system_head;

  dxp_log_info("dxp_user_setup", "Preparing to download FPGAs");

  status = dxp_fipconfig();

  if (status != DXP_SUCCESS) {
	dxp_log_error("dxp_user_setup", "Error downloading FPGAs", status);
	return status;
  }

  dxp_log_info("dxp_user_setup", "Preparing to download DSP code");

  status = dxp_dspconfig();

  if (status != DXP_SUCCESS) {
	dxp_log_error("dxp_user_setup", "Error downloading DSP code", status);
	return status;
  }

  /* The per-module configurations are done here. Only configurations that
   * are applicable to every module should be done here. Hardware specific
   * procedures should be farmed out to the individual device driver.
   */
  while (current != NULL) {
	  
	/* Loop over the current->nchan channels */
	for (chan = 0; chan < (int)current->nchan; chan++) {
	  detChan = current->detChan[chan];

	  /* Ignore disabled channels */
	  if (detChan < 0) continue;
		  
	  status = dxp_upload_dspparams(&detChan);

	  if (status != DXP_SUCCESS) {
		sprintf(info_string, "Error uploading DSP parameters for detChan %d",
				detChan);
		dxp_log_error("dxp_user_setup", info_string, status);
		return status;
	  }

	  status = dxp_dspdefaults(&detChan);

	  if (status != DXP_SUCCESS) {
		sprintf(info_string, "Error loading default DSP parameters for detChan "
				"%d", detChan);
		dxp_log_error("dxp_user_setup", info_string, status);
		return status;
	  }

	  status = dxp_replace_dspparams(&detChan);

	  if (status != DXP_SUCCESS) {
		sprintf(info_string, "Error downloading DSP parameters for detChan %d",
				detChan);
		dxp_log_error("dxp_user_setup", info_string, status);
		return status;
	  }
	}

	current = current->next;
  }


  return status; 
}


/*****************************************************************************
 *
 * Routine to transfer the default DSP parameters from the defaults structure
 * for this channel, to the downloadable parameters.
 *
 ******************************************************************************/
int XERXES_API dxp_dspdefaults(int* detChan)
	 /* int *detChan;					Input: detector channel to set defaults	*/
{
  unsigned int i;
  char info_string[INFO_LEN];
  unsigned short addr;
  int status=DXP_SUCCESS;
  int ioChan, modChan;
  /* Pointer to the chosen Board */
  Board *chosen=NULL;

  /* Get the ioChan and dxpChan matching the detChan */

  if((status=dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS){
	sprintf(info_string,"Unknown Detector Channel %d",*detChan);
	dxp_log_error("dxp_dspdefaults",info_string,status);
	return status;
  }
  ioChan = chosen->ioChan;
	
  /*
   * Now replace all the symbols loaded from the defaults file
   * in the database.
   */
  for (i=0;i<(chosen->defaults[modChan]->params->nsymbol);i++) {
	if ((status=chosen->btype->funcs->dxp_loc(chosen->defaults[modChan]->params->parameters[i].pname, 
											  chosen->dsp[modChan], &addr))!=DXP_SUCCESS) {
	  sprintf(info_string,"Unknown parameter name in defaults file: %s",
			  PRINT_NON_NULL(chosen->defaults[modChan]->params->parameters[i].pname));
	  dxp_log_error("dxp_dspdefaults",info_string,status);
	  return status;	
	}
	chosen->params[modChan][addr] = chosen->defaults[modChan]->data[i];
  }

  return status;

}


/** @brief Downloads all of the known FPGA configurations to the hardware.
 *
 * Past versions of this routine encoded lots of incorrect logic into
 * the procedure. This version delegates most of the download work to
 * routines in the device driver layer since some of the old assumptions
 * are no longer correct.
 *
 * Loop over each module and call the dxp_download_fpgaconfig() w/ 'all'. This
 * delegates the responsibility for determining the exact methodology to use
 * to the Device Driver layer and, as a consequence, dxp_fipconfig() can remain
 * hardware agnostic.
 */
XERXES_EXPORT int XERXES_API dxp_fipconfig(void)
{
  int status;

  char info_string[INFO_LEN];

  Board *current = system_head;


  while (current != NULL) {

	status = current->btype->funcs->dxp_download_fpgaconfig(&(current->ioChan),
															&allChan, "all",
															current);

	if (status != DXP_SUCCESS) {
	  sprintf(info_string, "Error Broadcasting fpga configurations to module %d",
			  current->mod);
	  dxp_log_error("dxp_fipconfig", info_string, status);
	  return status;
	}

	current = current->next;
  }
  
  return DXP_SUCCESS;
}


/******************************************************************************
 *
 * Replaces the FiPPi configuration for the board, the routine replace_fpgaconfig()
 * now supercedes this routine, but this remains for legacy code.
 *
 ******************************************************************************/
int XERXES_API dxp_replace_fipconfig(int* detChan, char* filename)
	 /* int *detChan;		Input: detector channel to load     */
	 /* char *filename;		Input: location of the new FiPPi    */
{
  int status;
  char info_string[INFO_LEN];

  /* Get the ioChan and dxpChan matching the detChan */
  status=dxp_replace_fpgaconfig(detChan, "fippi", filename);
  if (status != DXP_SUCCESS)
    {
	  sprintf(info_string,"Unable to download FiPPi configuration to Detector Channel %d",*detChan);
	  dxp_log_error("dxp_replace_fipconfig",info_string,status);
	  return status;
    }

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Replaces an FPGA configuration.
 *
 ******************************************************************************/
int XERXES_API dxp_replace_fpgaconfig(int* detChan, char *name, char* filename)
	 /* int *detChan;		Input: detector channel to load     */
	 /* char *name;                  Input: Type of FPGA to replace      */
	 /* char *filename;		Input: location of the new FiPPi    */
{
  int status;
  char info_string[INFO_LEN];
  int ioChan, modChan;
  unsigned short used;
  /* Pointer to the chosen Board */
  Board *chosen=NULL;

  /* Get the ioChan and dxpChan matching the detChan */

  if((status=dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS){
	sprintf(info_string,"Unknown Detector Channel %d",*detChan);
	dxp_log_error("dxp_replace_fpgaconfig",info_string,status);
	return status;
  }
  ioChan = chosen->ioChan;
  used = chosen->used;
	
  /* Disable the LAMs for each module before downloading */

  if((status=chosen->btype->funcs->dxp_look_at_me(&ioChan, &allChan))!=DXP_SUCCESS){
	sprintf(info_string,"Error disabling LAM for detector %d",*detChan);
	dxp_log_error("dxp_replace_fpgaconfig",info_string,status);
	return status;
  }

  /* Load the Fippi configuration into the structure */

  sprintf(info_string, "filename = %s", PRINT_NON_NULL(filename));
  dxp_log_debug("dxp_replace_fipconfig", info_string);

  /* XXX If the board has defined any of the FiPPIs used by the xMAP product,
   * then we need to pass in a different Fippi_Info structure.
   */
  if (chosen->fippi_a || chosen->system_fpga) {
    if (STRNEQ(name, "a_and_b") || STREQ(name, "a")) {
      status = dxp_add_fippi(filename, chosen->btype, &(chosen->fippi_a));
    } else if (STREQ(name, "system_fpga")) {
      status = dxp_add_fippi(filename, chosen->btype, &(chosen->system_fpga));
    }
  } else if (chosen->system_fippi != NULL) {
    status = dxp_add_system_fippi(filename, chosen->btype,
                                  &(chosen->system_fippi));
  } else {
    status = dxp_add_fippi(filename, chosen->btype, &(chosen->fippi[modChan]));
  }

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error loading FPGA %s", PRINT_NON_NULL(filename));
	dxp_log_error("dxp_replace_fpgaconfig", info_string, status);
	return status;
  }

  /* Download the FiPPi program to the channel */

  if((status=chosen->btype->funcs->dxp_download_fpgaconfig(&ioChan, 
														   &modChan, name, chosen))!=DXP_SUCCESS){
	sprintf(info_string,"Error downloading %s FPGA(s) to detector %d", PRINT_NON_NULL(name), *detChan);
	dxp_log_error("dxp_replace_fpgaconfig",info_string,status);
	return status;
  }

  /* Determine if the FiPPi was downloaded successfully */
  if((status=chosen->btype->funcs->dxp_download_fpga_done(&modChan, name, chosen))!=DXP_SUCCESS){
	sprintf(info_string,"Failed to replace %s FPGA(s) succesfully for detector %d", PRINT_NON_NULL(name), *detChan);
	dxp_log_error("dxp_replace_fpgaconfig",info_string,status);
	return status;
  }
		
  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Download currently loaded FIPPI firmware to a single channel.  
 *
 ******************************************************************************/
int XERXES_API dxp_reset_fipconfig(int* detChan)
	 /* int *detChan;		Input: Detector Channel number	*/
{
  int status;
  char info_string[INFO_LEN];
	
  status = dxp_reset_fpgaconfig(detChan, "fippi");
  if (status != DXP_SUCCESS)
    {
	  sprintf(info_string,"Error downloading FIPPI to Detector Channel %d", *detChan);
	  dxp_log_error("dxp_reset_fipconfig",info_string,status);
	  return status;
    }

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Download currently loaded FPGA firmware to a single channel.  
 *
 ******************************************************************************/
int XERXES_API dxp_reset_fpgaconfig(int* detChan, char *name)
	 /* int *detChan;		Input: Detector Channel number	    */
	 /* char *name;                  Input: Type of firmware to download */
{
  int status;
  char info_string[INFO_LEN];
  int modChan, ioChan;
  Board *chosen = NULL;
	
  /* Get the ioChan and dxpChan matching the detChan */

  if((status=dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS){
	sprintf(info_string,"Unknown Detector Channel %d",*detChan);
	dxp_log_error("dxp_reset_fpgaconfig",info_string,status);
	return status;
  }
  ioChan = chosen->ioChan;

  /* Disable the LAMs for each module before downloading */

  if((status=chosen->btype->funcs->dxp_look_at_me(&ioChan, &allChan))!=DXP_SUCCESS){
	sprintf(info_string,"Error disabling LAM for detector %d",*detChan);
	dxp_log_error("dxp_reset_fpgaconfig",info_string,status);
	return status;
  }

  /* Download the FiPPi program to the channel */

  if((status=chosen->btype->funcs->dxp_download_fpgaconfig(&ioChan, &modChan, 
														   name, chosen))!=DXP_SUCCESS){
	sprintf(info_string,"Error downloading %s FPGA(s) to detector %d", name, *detChan);
	dxp_log_error("dxp_reset_fpgaconfig",info_string,status);
	return status;
  }

  /* Determine if the FiPPi was downloaded successfully */
  if((status=chosen->btype->funcs->dxp_download_fpga_done(&modChan, name, chosen))!=DXP_SUCCESS){
	sprintf(info_string,"Failed to replace %s FPGA(s) succesfully for detector %d", PRINT_NON_NULL(name), *detChan);
	dxp_log_error("dxp_reset_fpgaconfig",info_string,status);
	return status;
  }
	
  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Download DSP firmware to a single channel.
 *
 ******************************************************************************/
int XERXES_API dxp_reset_dspconfig(int* detChan)
	 /* int *detChan;					Input: Detector Channel number	*/
{
    int status;
    char info_string[INFO_LEN];
    int modChan, ioChan;
    Board *chosen = NULL;
    /* Time to wait for the initialization of the DSP program */
    float one = 1.;
	
  /* Get the ioChan and dxpChan matching the detChan */

  if((status=dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS){
	sprintf(info_string,"Unknown Detector Channel %d",*detChan);
	dxp_log_error("dxp_reset_dspconfig",info_string,status);
	return status;
  }
  ioChan = chosen->ioChan;

  /* Download the DSP code to this channel of this module. */
  if((status=chosen->btype->funcs->dxp_download_dspconfig(&ioChan,
														  &modChan, chosen))!=DXP_SUCCESS){
	sprintf(info_string,"Error downloading to module %d, channel %d",chosen->mod,modChan);
	dxp_log_error("dxp_reset_dspconfig",info_string,status);
	return status;

    }
    /* DSP is downloaded, set its state */
    chosen->chanstate[modChan].dspdownloaded = 1;

	xerxes_md_wait(&one);

    /* Read out the DSP parameter memory, these contain the defaults defined
     * at the DSP compile time. */
    if((status=chosen->btype->funcs->dxp_read_dspparams(&ioChan, &modChan, 
							chosen, chosen->params[modChan]))!=DXP_SUCCESS){

	sprintf(info_string,"Error reading parameters from module %d channel %d",chosen->mod,modChan);
	dxp_log_error("dxp_reset_dspconfig",info_string,status);
	return status;
  }

  return DXP_SUCCESS;
}


/** @brief Download the DSP code to all of the modules.
 *
 *
 *
 */
XERXES_EXPORT int XERXES_API dxp_dspconfig(void)
{
  int status;
  int ioChan;

  char info_string[INFO_LEN];

  Board *current = system_head;
	

  dxp_log_debug("dxp_dspconfig", "Preapring to call dxp_download_dspconfig");

  while (current != NULL) {
	ioChan = current->ioChan;

	status = current->btype->funcs->dxp_download_dspconfig(&ioChan, &allChan,
														   current);

	if (status != DXP_SUCCESS) {
	  sprintf(info_string, "Error downloading DSP code to ioChan %d", ioChan);
	  dxp_log_error("dxp_dspconfig", info_string, status);
	  return status;
	}

	current = current->next;
  }

  dxp_log_debug("dxp_dspconfig", "Returning from dxp_dspconfig");

  return DXP_SUCCESS;
}


/******************************************************************************
 *
 * Download DSP firmware to one specific detector channel from the given file.
 * It is currently essential that all symbol tables in all DSP programs be 
 * arranged in the same way.  Test download with calls to 
 * dxp_test_spectrum_memory().
 *
 ******************************************************************************/
int XERXES_API dxp_replace_dspconfig(int* detChan, char* filename)
	 /* int *detChan;			Input: Detector Channel to download new DSP code */
	 /* char *filename;			Input: Filename to obtain the new DSP config	 */
{

  int ioChan, modChan, status;
  char info_string[INFO_LEN];
  /* Pointer to the chosen Board */
  Board *chosen=NULL;

  /* Get the ioChan and dxpChan number that matches the detChan */

  if((status=dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS){
	sprintf(info_string,"Error finding detector number %d",*detChan);
	dxp_log_error("dxp_replace_dspconfig",info_string,status);
	return status;
  }
  ioChan = chosen->ioChan;
	
  /* If the board has a system DSP, then we want to update that instead of
   * the "normal" DSP.
   */
  if (chosen->system_dsp != NULL) {
    status = dxp_add_dsp(filename, chosen->btype, &(chosen->system_dsp));
  } else {
    status = dxp_add_dsp(filename, chosen->btype, &(chosen->dsp[modChan]));
  }

  if (status != DXP_SUCCESS){
    sprintf(info_string,"Error loading Dsp %s",PRINT_NON_NULL(filename));
    dxp_log_error("dxp_replace_dspconfig",info_string,status);
    return status;
  }

  /* Set the dspstate value to 2, indicating that the DSP needs update */
  chosen->chanstate[modChan].dspdownloaded = 2;

  /* Download the DSP code to the IO channel and DXP channel. */
        
  if((status=chosen->btype->funcs->dxp_download_dspconfig(&ioChan, &modChan, chosen))!=DXP_SUCCESS){
	sprintf(info_string,"Error downloading DSP to detector number %d",*detChan);
	dxp_log_error("dxp_replace_dspconfig",info_string,status);
	return status;
  }
  /* DSP is downloaded for this channel, set its state */
  chosen->chanstate[modChan].dspdownloaded = 1;

  /* Now test reading and writing of data into the memory space of each DSP. */

  if((status=chosen->btype->funcs->dxp_test_spectrum_memory(&ioChan,&modChan,&zero, 
															chosen)) !=DXP_SUCCESS){
	sprintf(info_string, "Failure testing detector channel %d",*detChan);
	dxp_log_error("dxp_replace_dspconfig",info_string,status);
	return status;
  }

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 *  Download parameters stored in memory for all channels
 *
 ******************************************************************************/
int XERXES_API dxp_put_dspparams(VOID)
{

  int status;
  char info_string[INFO_LEN];
  int ioChan; 
  unsigned short used;
  unsigned int chan;
  int ichan;
  /* Pointer to the current Board */
  Board *current = system_head;

  /* Loop over all modules in the system */
	
  while (current!=NULL) {
	ioChan = current->ioChan;
	used = current->used;

	for(chan=0;chan<current->nchan;chan++){
	  if ((used&(1<<chan))==0) continue;
	  /* Loop over the parameters requested, writing each value[] to addr[]. */
        
	  ichan = (int) chan;
	  if((status=current->btype->funcs->dxp_write_dspparams(&ioChan,&ichan, current->dsp[chan], current->params[chan]))!=DXP_SUCCESS){
		sprintf(info_string,"Error writing parameters to module %d",current->mod);
		dxp_log_error("dxp_put_params",info_string,status);
		return status;
	  }
	}
	current = current->next;
  }

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Download previously loaded by dxp_dspdefaults() params to designated detector 
 * channel.
 *
 ******************************************************************************/
int XERXES_API dxp_replace_dspparams(int* detChan)
	 /* int *detChan;					Input: detector channel to download to	*/
{

  int status;
  char info_string[INFO_LEN];
  int ioChan, modChan;
  /* Pointer to chosen Board */
  Board *chosen=NULL;

  /* Get the ioChan and modChan number that matches the detChan */

  if((status=dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS){
	sprintf(info_string,"Error finding detector number %d",*detChan);
	dxp_log_error("dxp_replace_dspparams",info_string,status);
	return status;
  }
  ioChan = chosen->ioChan;
	
  /* Loop over the parameters requested, writing each value[] to addr[]. */
        
  if((status=chosen->btype->funcs->dxp_write_dspparams(&ioChan, &modChan, chosen->dsp[modChan], chosen->params[modChan]))!=DXP_SUCCESS){
	sprintf(info_string,"Error writing parameters to detector %d",*detChan);
	dxp_log_error("dxp_replace_params",info_string,status);
	return status;
  }

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Upload DSP params from designated detector channel.
 *
 ******************************************************************************/
int XERXES_API dxp_upload_dspparams(int* detChan)
	 /* int *detChan;					Input: detector channel to download to	*/
{
  int status;
  char info_string[INFO_LEN];
  int ioChan, modChan;
  /* Pointer to the chosen Board */
  Board *chosen=NULL;

  /* Get the ioChan and modChan number that matches the detChan */
  if((status=dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS){
	sprintf(info_string,"Error finding detector number %d",*detChan);
	dxp_log_error("dxp_upload_dspparams",info_string,status);
	return status;
  }
  ioChan = chosen->ioChan;
	
  /* Make sure that a params array in Board structure exists */
  if (chosen->params==NULL) {
	status = DXP_NOMEM;
	sprintf(info_string,
			"Something wrong: %d detector channel has no allocated parameter memory", *detChan);
	dxp_log_error("dxp_upload_dspparams",info_string,status);
	return status;
  }

  /* Loop over the parameters requested, writing each value[] to addr[]. */
        
  if((status=chosen->btype->funcs->dxp_read_dspparams(&ioChan, &modChan, 
													  chosen, chosen->params[modChan]))!=DXP_SUCCESS){
	sprintf(info_string,"Error reading parameters from detector %d",*detChan);
	dxp_log_error("dxp_upload_dspparams",info_string,status);
	return status;
  }

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Routine to echo the global static arrays.  Returns the number of DXP modules
 * in the system, the array of CAMAC modules, and the array designating which 
 * channels are in use.
 *
 ******************************************************************************/
int XERXES_API dxp_get_electronics(int* totMod, int ioChan[], int used[])
	 /* int *totMod;					*/
	 /* int ioChan[];				*/
	 /* int used[];					*/
{

  int mod,status;
  /* Pointer to the current Board */
  Board *current = system_head;

  /* If no channels, then nothing to report. */
	
  if (numDxpMod==0){
	status = DXP_NOCHANNELS;
	dxp_log_error("dxp_get_electronics",
				  "No DXP modules defined: call dxp_assign_channel, define DXP_MODULE",
				  status);
	return status;
  }

  /* Copy all the data for the user */

  *totMod = numDxpMod;
  mod = 0;
  while (current != NULL) {
	ioChan[mod] = current->ioChan;
	used[mod] = current->used;
	/* Bump the pointers */
	mod++;
	current = current->next;
  }

  return DXP_SUCCESS;
}

/******************************************************************************
 * 
 * Return the number of DXP channels in the system and the array of 
 * user-defined detector channel numbers.
 * 
 ******************************************************************************/
int XERXES_API dxp_get_detectors(int* totChan,int detChan[])
	 /* int *totChan;						*/
	 /* int detChan[];						*/
{

  unsigned int i;
  int echan, status;
  /* Pointer to the current Board */
  Board *current = system_head;

  /* If there are no channels, then why are we here? */

  if (numDxpChan==0){
	status = DXP_NOCHANNELS;
	dxp_log_error("dxp_get_detectors",
				  "No detectors defined: call dxp_assign_channel, define DXP_MODULE",
				  status);
	return status;
  }

  /* Pull out the information and pass it back to the caller. */

  *totChan = numDxpChan;
  echan = 0;
  while (current != NULL) {
	for (i=0; i<current->nchan; i++) {
	  if ((current->detChan[i])>-1) {
		detChan[echan] = current->detChan[i];
		echan++;
	  }
	}
	current = current->next;
  }

  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to return the length of the spectrum memory.
 *
 * For 4C-2X boards, this value is stored in the DSP and dynamic.  
 * For 4C boards, it is fixed.
 * 
 ******************************************************************************/
int XERXES_API dxp_nspec(int* detChan, unsigned int* nspec)
	 /* int *detChan;				Input: Detector channel number				*/
	 /* unsigned int *nspec;			Output: The number of bins in the spectrum	*/
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];
  int modChan;
  /* Pointer to the chosen Board */
  Board *chosen=NULL;

  /* Get the ioChan and modChan number that matches the detChan */
  if((status=dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS){
	sprintf(info_string,"Error finding detector number %d",*detChan);
	dxp_log_error("dxp_nspec",info_string,status);
	return status;
  }

  status = chosen->btype->funcs->dxp_get_spectrum_length(&chosen->ioChan,
														 &modChan, chosen,
														 nspec);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error reading spectrum length for detChan = %d",
			*detChan);
	dxp_log_error("dxp_nspec", info_string, status);
	return status;
  }

  return DXP_SUCCESS;
}


/** @brief Returns the size of the SCA data buffer
 *
 */
XERXES_EXPORT int XERXES_API dxp_nsca(int *detChan, unsigned short *nSCA)
{
  int status;
  int modChan;

  char info_string[INFO_LEN];

  Board *chosen = NULL;


  if (detChan == NULL) {
	dxp_log_error("dxp_nsca", "'detChan' may not be NULL", DXP_NULL);
	return DXP_NULL;
  }

  if (nSCA == NULL) {
	dxp_log_error("dxp_nsca", "'nSCA' may not be NULL", DXP_NULL);
	return DXP_NULL;
  }

  status = dxp_det_to_elec(detChan, &chosen, &modChan);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error finding detChan '%d'", *detChan);
	dxp_log_error("dxp_nsca", info_string, status);
	return status;
  }

  *nSCA = (unsigned short)chosen->btype->funcs->dxp_get_sca_length(chosen->dsp[modChan], chosen->params[modChan]);

  return DXP_SUCCESS;
}


/******************************************************************************
 * Routine to return the length of the base memory.
 *
 * For 4C-2X boards, this value is stored in the DSP and dynamic.  
 * For 4C boards, it is fixed.
 * 
 ******************************************************************************/
int XERXES_API dxp_nbase(int* detChan, unsigned int* nbase)
	 /* int *detChan;				Input: Detector channel number				*/
	 /* unsigned int *nbase;			Output: The number of bins in the baseline */
{
  int status;
  int modChan;

  char info_string[INFO_LEN];

  Board *chosen = NULL;

  /* Get the ioChan and modChan number that matches the detChan */
  status = dxp_det_to_elec(detChan, &chosen, &modChan);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error finding detector number %d", *detChan);
	dxp_log_error("dxp_nbase", info_string, status);
	return status;
  }

  status = chosen->btype->funcs->dxp_get_baseline_length(&modChan, chosen,
														 nbase);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error getting baseline length for detChan %d",
			*detChan);
	dxp_log_error("dxp_nbase", info_string, status);
	return status;
  }

  return DXP_SUCCESS;
}


/******************************************************************************
 * Routine to return the length of the event memory.
 *
 * For 4C-2X boards, this value is stored in the DSP and dynamic.  
 * For 4C boards, it is fixed.
 * 
 ******************************************************************************/
int XERXES_API dxp_nevent(int* detChan, unsigned int* nevent)
	 /* int *detChan;					Input: Detector channel number				*/
	 /* unsigned int *nevent;			Output: The number of samples in event memory*/
{
  int status = DXP_SUCCESS;
  char info_string[INFO_LEN];
  int modChan;
  /* Pointer to the chosen Board */
  Board *chosen=NULL;

  /* Get the ioChan and modChan number that matches the detChan */
  if((status=dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS){
	sprintf(info_string,"Error finding detector number %d",*detChan);
	dxp_log_error("dxp_nspec",info_string,status);
	return status;
  }

  *nevent = chosen->btype->funcs->dxp_get_event_length(chosen->dsp[modChan], chosen->params[modChan]);

  return status;
}

/******************************************************************************
 * Routine to return the number of DXP modules in the system.
 *
 ******************************************************************************/
int XERXES_API dxp_ndxp(int* ndxp)
	 /* int *ndxp;						Output: The number of DXP modules */
{
  int status = DXP_SUCCESS;

  *ndxp = numDxpMod;

  return status;
}

/******************************************************************************
 * Routine to return the number of DXP modules in the system.
 *
 ******************************************************************************/
int XERXES_API dxp_ndxpchan(int* ndxpchan)
	 /* int *ndxpchan;					Output: The number of detector channels */
{
  int status = DXP_SUCCESS;

  *ndxpchan = numDxpChan;

  return status;
}

/********------******------*****------******-------******-------******------*****
 * Routines to control the taking of data and get the data from the modules.
 * These include starting and stopping runs and reading spectra from the system.
 *
 ********------******------*****------******-------******-------******------*****/

/******************************************************************************
 *
 * Enable the LAM (Look At Me) mechanism for this channel.  
 *
 ******************************************************************************/
int XERXES_API dxp_enable_LAM(int* detChan)
	 /* int *detChan;					Input: Detector Channel number	*/
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];
  int modChan, ioChan;
  Board *chosen = NULL;
	

  /* If the detector channel is not -1, then process only that channel */
  if (*detChan != -1) {	
	/* Get the ioChan and dxpChan matching the detChan */

	if((status=dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS){
	  sprintf(info_string,"Unknown Detector Channel %d",*detChan);
	  dxp_log_error("dxp_enable_LAM",info_string,status);
	  return status;
	}
	ioChan = chosen->ioChan;

	/* Enable the LAM for this module */

	if((status=chosen->btype->funcs->dxp_look_at_me(&ioChan, &modChan))!=DXP_SUCCESS){
	  sprintf(info_string,"Error enabling the LAM for detector %d",*detChan);
	  dxp_log_error("dxp_enable_LAM",info_string,status);
	  return status;
	}
  } else {
	/* Else enable LAMs on all boards */
	chosen = system_head;
	/* Check that there are any boards defined in the system */
	if (chosen==NULL) {
	  status = DXP_INITIALIZE;
	  sprintf(info_string,"No Boards defined in the system, please initialize first");
	  dxp_log_error("dxp_enable_LAM",info_string,status);
	  return status;
	}
	/* Loop over all boards */
	while (chosen) {
	  if((status=chosen->btype->funcs->dxp_look_at_me(&(chosen->ioChan), &allChan))!=DXP_SUCCESS){
		sprintf(info_string,"Error enabling the LAM for module %d",chosen->mod);
		dxp_log_error("dxp_enable_LAM",info_string,status);
		return status;
	  }
	  chosen = chosen->next;
	}
  }


  return status;
}
	
/******************************************************************************
 *
 * Disable the LAM (Look At Me) mechanism for this channel.  
 *
 ******************************************************************************/
int XERXES_API dxp_disable_LAM(int* detChan)
	 /* int *detChan;					Input: Detector Channel number	*/
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];
  int modChan, ioChan;
  Board *chosen = NULL;
	
  /* If the detector channel is not -1, then process only that channel */
  if (*detChan != -1) {	
	/* Get the ioChan and dxpChan matching the detChan */

	if((status=dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS){
	  sprintf(info_string,"Unknown Detector Channel %d",*detChan);
	  dxp_log_error("dxp_disable_LAM",info_string,status);
	  return status;
	}
	ioChan = chosen->ioChan;

	/* Disable the LAM for this module */

	if((status=chosen->btype->funcs->dxp_ignore_me(&ioChan, &modChan))!=DXP_SUCCESS){
	  sprintf(info_string,"Error disabling the LAM for detector %d",*detChan);
	  dxp_log_error("dxp_disable_LAM",info_string,status);
	  return status;
	}
  } else {
	/* Else disable LAMs on all boards */
	chosen = system_head;
	/* Check that there are any boards defined in the system */
	if (chosen==NULL) {
	  status = DXP_INITIALIZE;
	  sprintf(info_string,"No Boards defined in the system, please initialize first");
	  dxp_log_error("dxp_disable_LAM",info_string,status);
	  return status;
	}
	/* Loop over all boards */
	while (chosen) {
	  if((status=chosen->btype->funcs->dxp_ignore_me(&(chosen->ioChan), &allChan))!=DXP_SUCCESS){
		sprintf(info_string,"Error disabling the LAM for module %d",chosen->mod);
		dxp_log_error("dxp_disable_LAM",info_string,status);
		return status;
	  }
	  chosen = chosen->next;
	}
  }

  return status;
}

/******************************************************************************
 *
 * Clear the LAM (Look At Me) mechanism for this channel.  
 *
 ******************************************************************************/
int XERXES_API dxp_reset_LAM(int* detChan)
	 /* int *detChan;					Input: Detector Channel number	*/
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];
  int modChan, ioChan;
  Board *chosen = NULL;
	

  /* If the detector channel is not -1, then process only that channel */
  if (*detChan != -1) {	
	/* Get the ioChan and dxpChan matching the detChan */

	if((status=dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS){
	  sprintf(info_string,"Unknown Detector Channel %d",*detChan);
	  dxp_log_error("dxp_reset_LAM",info_string,status);
	  return status;
	}
	ioChan = chosen->ioChan;

	/* Clear the LAM for this module */

	if((status=chosen->btype->funcs->dxp_clear_LAM(&ioChan, &modChan))!=DXP_SUCCESS){
	  sprintf(info_string,"Error reseting the LAM for detector %d",*detChan);
	  dxp_log_error("dxp_reset_LAM",info_string,status);
	  return status;
	}
  } else {
	/* Else enable LAMs on all boards */
	chosen = system_head;
	/* Check that there are any boards defined in the system */
	if (chosen==NULL) {
	  status = DXP_INITIALIZE;
	  sprintf(info_string,"No Boards defined in the system, please initialize first");
	  dxp_log_error("dxp_reset_LAM",info_string,status);
	  return status;
	}
	/* Loop over all boards */
	while (chosen) {
	  if((status=chosen->btype->funcs->dxp_clear_LAM(&(chosen->ioChan), &allChan))!=DXP_SUCCESS){
		sprintf(info_string,"Error reseting the LAM for module %d",chosen->mod);
		dxp_log_error("dxp_reset_LAM",info_string,status);
		return status;
	  }
	  chosen = chosen->next;
	}
  }


  return status;
}
	
/******************************************************************************
 *
 * This routine initiates a data-taking run on all DXP channels.  If *gate=0,
 * the run does not actually begin until the external gate goes to a TTL high
 * state.  If *resume=0, the MCA is first cleared; if *resume=1, the spectrum
 * is not cleared.
 *
 ******************************************************************************/
int XERXES_API dxp_start_run(unsigned short* gate, unsigned short* resume)
	 /* unsigned short *gate;			Input: 0 --> use extGate 1--> ignore gate*/
	 /* unsigned short *resume;			Input: 0 --> clear MCA first; 1--> update*/
{

  int status = DXP_SUCCESS;
  int id     = 0;

  char info_string[INFO_LEN];
  int ioChan, detChan, runactive;
  /* Pointer to the current Board */
  Board *current = system_head;

  /* gate value, if NULL is passed, use the XerXes stored value */
  unsigned short my_gate;

  /* Check for active runs in the system */
  runactive = 0;
  if((status=dxp_isrunning_any(&detChan, &runactive))!=DXP_SUCCESS){
	sprintf(info_string,"Error checking run active status");
	dxp_log_error("dxp_start_run",info_string,status);
	return status;
  }
  if (runactive != 0) {
	status = DXP_RUNACTIVE;
	if ((runactive&0x1)!=0) {
	  sprintf(info_string,"Must stop run in detector %d before beginning a new run.", detChan);
	} else if ((runactive&0x2)!=0) {
	  sprintf(info_string,"XerXes thinks there is an active run.  Please execute dxp_stop_run().");
	} else {
	  sprintf(info_string,"Unknown reason why run is active.  This should never occur.");
	}
	dxp_log_error("dxp_start_run",info_string,status);
	return status;
  }

	
  /* Nice simple wrapper routine to loop over all modules in the system */
  while (current!=NULL) {
	ioChan = current->ioChan;
	/* Check on gate, if not defined, pick up from XerXes */
	if (gate == NULL) {
	  my_gate = (unsigned short) current->state[1];
	} else {
	  my_gate = *gate;
	}
	/* Start the run */
	status = DD_FUNC(current)->dxp_begin_run(&ioChan, &allChan, &my_gate, resume,
											 current, &id);

	if (status != DXP_SUCCESS) {
	  sprintf(info_string,"error beginning run for module %d",current->mod);
	  dxp_log_error("dxp_start_run",info_string,status);
	  return status;
	}

  sprintf(info_string, "Started run id = %d on all channels", id);
  dxp_log_debug("dxp_start_run", info_string);	

	/* Change the system status */
	current->state[0] = 1;				/* Run is active		*/
	current->state[1] = my_gate;		/* Store gate status	*/
	/* Point to the next board */
	current = current->next;
  }

  return status;
}

/******************************************************************************
 *
 * This routine allows the user to resume a run that was previously stopped.
 * This will not clear the currently stored histogram in the firmware.
 *
 * If a run is active, no action is performed.
 *
 ******************************************************************************/
int XERXES_API dxp_resume_run()
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];
  /* Set resume to true */
  unsigned short resume = 1;

  /* Nice simple wrapper routine to loop over all modules in the system */

  if((status=dxp_start_run(NULL, &resume))!=DXP_SUCCESS){
	sprintf(info_string,"Error resuming run");
	dxp_log_error("dxp_resume_run",info_string,status);
	return status;
  }

  return status;
}

/******************************************************************************
 *
 * Stops runs on all DXP channels.
 *
 ******************************************************************************/
int XERXES_API dxp_stop_run(VOID)
{

  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];
  int ioChan;
  /* Pointer to the current Board */
  Board *current = system_head;
    
  /* Wrapper to loop over all modules and stop the runs. */
    
  while (current!=NULL) {
	if (current->state[0] == 1) {
	  /* Change the run active status */
	  current->state[0] = 0;		/* Run is not active now */
	  /* Get the ioChan pointer */
	  ioChan = current->ioChan;
	  if((status=current->btype->funcs->dxp_end_run(&ioChan, &allChan, current))!=DXP_SUCCESS){
		sprintf(info_string,"error ending run for module %d",current->mod);
		dxp_log_error("dxp_stop_run",info_string,status);
		return status;
	  }
	}
	current = current->next;
  }

	sprintf(info_string, "Stopped run on all channels");
	dxp_log_debug("dxp_stop_run", info_string);
  
  return status;

}

/******************************************************************************
 *
 * This routine initiates a data-taking run on all DXP channels.  If *gate=0,
 * the run does not actually begin until the external gate goes to a TTL high
 * state.  If *resume=0, the MCA is first cleared; if *resume=1, the spectrum
 * is not cleared.
 *
 ******************************************************************************/
int XERXES_API dxp_start_one_run(int *detChan, unsigned short* gate, unsigned short* resume)
	 /* int *detChan;					Input: Detector channel to write to			*/
	 /* unsigned short *gate;			Input: 0 --> use extGate 1--> ignore gate*/
	 /* unsigned short *resume;			Input: 0 --> clear MCA first; 1--> update*/
{

  int status = DXP_SUCCESS;
  int id     = 0;

  char info_string[INFO_LEN];
  int ioChan, modChan;		/* IO channel and module channel numbers */
  /* Pointer to the Chosen Channel */
  Board *chosen=NULL;
  /* Run status */
  int active;

  /* Translate the detector channel number to IO and channel numbers for writing. */

  if ((status  = dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) {
	sprintf(info_string, "Failed to locate detector channel %d", *detChan);
	dxp_log_error("dxp_start_one_run", info_string, status);
	return status;
  }
  ioChan = chosen->ioChan;

  /* Determine if there is already a run active */
  if ((status  = dxp_isrunning(detChan, &active))!=DXP_SUCCESS) {
	sprintf(info_string, "Failed to determine run status of detector channel %d", *detChan);
	dxp_log_error("dxp_start_one_run", info_string, status);
	return status;
  }

  if(active == 0) {
	/* Continue with starting new run */
	ioChan = chosen->ioChan;

	status = DD_FUNC(chosen)->dxp_begin_run(&ioChan, &modChan, gate, resume,
											chosen, &id);

	if (status != DXP_SUCCESS) {
	  sprintf(info_string,"error beginning run for module %d",chosen->mod);
	  dxp_log_error("dxp_start_one_run",info_string,status);
	  return status;
	}

	sprintf(info_string, "Started run id = %d on ioChan = %d", id, ioChan);
	dxp_log_debug("dxp_start_one_run", info_string);

	/* Change the system status */
	chosen->state[0] = 1;				/* Run is active		*/
	chosen->state[1] = (short) *gate;	/* Store gate status	*/
  } else if ((active&0x1)!=0) {
	status = DXP_RUNACTIVE;
	sprintf(info_string,"Run already active in module %d, please end before starting new run", 
			chosen->mod);
	dxp_log_error("dxp_start_one_run",info_string,status);
  } else if ((active&0x4)!=0) {
	status = DXP_RUNACTIVE;
	sprintf(info_string,"Xerxes thinks a control task is active in module %d, please end before starting new run", 
			chosen->mod);
	dxp_log_error("dxp_start_one_run",info_string,status);
  } else if ((active&0x2)!=0) {
	status = DXP_RUNACTIVE;
	sprintf(info_string,"Xerxes thinks a run is active in module %d, please end before starting new run", 
			chosen->mod);
	dxp_log_error("dxp_start_one_run",info_string,status);
  } else {
	status = DXP_RUNACTIVE;
	sprintf(info_string,"Unknown reason run is active in module %d.  This should never occur.", 
			chosen->mod);
	dxp_log_error("dxp_start_one_run",info_string,status);
  }

  return status;
}

/******************************************************************************
 *
 * This routine allows the user to resume a run that was previously stopped.
 * This will not clear the currently stored histogram in the firmware.
 *
 ******************************************************************************/
int XERXES_API dxp_resume_one_run(int *detChan)
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];
  /* Set resume to true */
  unsigned short resume = 1;

  /* Nice simple wrapper routine to loop over all modules in the system */

  if((status=dxp_start_one_run(detChan, NULL, &resume))!=DXP_SUCCESS){
	sprintf(info_string,"Error resuming run for detector %d", *detChan);
	dxp_log_error("dxp_resume_one_run",info_string,status);
	return status;
  }

  return status;
}

/******************************************************************************
 *
 * Stops runs on all DXP channels.
 *
 ******************************************************************************/
int XERXES_API dxp_stop_one_run(int *detChan)
{

  int status;
  int ioChan;
  int modChan;

  char info_string[INFO_LEN];

  Board *chosen = NULL;


  status = dxp_det_to_elec(detChan, &chosen, &modChan);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Failed to locate detector channel %d", *detChan);
    dxp_log_error("dxp_stop_one_run", info_string, status);
    return status;
  }

  ioChan = chosen->ioChan;

  if (chosen->state[0] == 1) {
    /* Change the run active status */
    chosen->state[0] = 0;

    status = chosen->btype->funcs->dxp_end_run(&ioChan, &modChan, chosen);

    if (status != DXP_SUCCESS) {
      sprintf(info_string,"Error ending run for ioChan = %d", ioChan);
      dxp_log_error("dxp_stop_one_run", info_string, status);
      return status;
    }
  }

  sprintf(info_string, "Stopped a run on ioChan = %d", ioChan);
	dxp_log_debug("dxp_stop_one_run", info_string);

  return DXP_SUCCESS;
}


/******************************************************************************
 *
 * This routine returns non-zero if the specified module has a run active.
 *
 * Return value has bit 1 set if the board thinks a run is active
 * bit 2 is set if XerXes thinks a run is active
 *
 ******************************************************************************/
int XERXES_API dxp_isrunning(int *detChan, int *ret)
{

  int status = DXP_SUCCESS;
  char info_string[INFO_LEN];
  int ioChan = 0; 
  int modChan = 0;
  int runactive = 0x0;

  /* Pointer to the Chosen Channel */
  Board *chosen = NULL;

  /* Initialize */
  *ret = 0;

  /* Translate the detector channel number to IO and channel numbers for writing. */

  if ((status  = dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) {
	sprintf(info_string, "Failed to locate detector channel %d", *detChan);
	dxp_log_error("dxp_isrunning", info_string, status);
	return status;
  }
  ioChan = chosen->ioChan;

  /* Does XerXes think a run is active? */
  if (chosen->state[0] == 1) *ret = 2;
  /* Does XerXes think a control task is active? */
  if (chosen->state[4] != 0) *ret = 4;

  /* Query the hardware for its status */

  if((status=chosen->btype->funcs->dxp_run_active(&ioChan, &modChan, &runactive))!=DXP_SUCCESS){
    sprintf(info_string,"Error checking run status for module %d",chosen->mod);
    dxp_log_error("dxp_isrunning",info_string,status);
    return status;
  }

  if (runactive == 1) *ret |= 1;

  return status;
}

/******************************************************************************
 *
 * This routine returns the first detector channel that registers as running.
 * If detChan=-1, then no channel was running, ret will contain if a XerXes thinks
 * any board has an active run.
 *
 * Return value has bit 1 set if the board thinks a run is active
 * bit 2 is set if XerXes thinks a run is active
 *
 ******************************************************************************/
int XERXES_API dxp_isrunning_any(int *detChan, int *ret)
{

  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];
  int runactive, i;
  /* Pointer to the Chosen Channel */
  Board *current=system_head;

  /* Initialize */
  *detChan = -1;
  *ret = 0;

  while (current!=NULL) {
	for (i=0;i < (int) current->nchan;i++) {
	  if ((current->used&(1<<i))!=0) {
		runactive = 0;
		/* Get the run status for this detector channel */
		if((status=dxp_isrunning(&(current->detChan[i]), &runactive))!=DXP_SUCCESS){
		  sprintf(info_string,"Error determining run status for detector %d",current->detChan[i]);
		  dxp_log_error("dxp_isrunning_any",info_string,status);
		  return status;
		}
		*ret |= runactive;
		/* Does this channel think a run is active on the physical board.  Return from the routine on the first
		 * detector that has an active run */
		if ((runactive&0x1)!=0) {
		  *detChan = current->detChan[i];
		  return status;
		}
	  }
	}
	current = current->next;
  }

  return status;
}

/******************************************************************************
 *
 * This routine starts a control run on the specified channel.  Control runs
 * include calibrations, sleep, adc acquisition, external memory reading, etc...
 * The data in info gives the module extra information needed to perform the
 * control task.
 *
 ******************************************************************************/
int XERXES_API dxp_start_control_task(int *detChan, short *type, 
									  unsigned int *length, int *info)
	 /* int *detChan;					Input: Detector channel to write to			*/
	 /* short *type;						Input: Type of control run, defined in .h	*/
	 /* unsigned int *length;			Input: Length of info array					*/
	 /* int *info;						Input: Data needed by the control task		*/
{

  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];
  int ioChan, modChan;		/* IO channel and module channel numbers */
  /* Pointer to the Chosen Channel */
  Board *chosen=NULL;
  /* Run status */
  int active;

  dxp_log_debug("dxp_start_control_task", "Entering dxp_start_control_task()");

  /* Translate the detector channel number to IO and channel numbers for writing. */

  if ((status  = dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) {
	sprintf(info_string, "Failed to locate detector channel %d", *detChan);
	dxp_log_error("dxp_start_control_task", info_string, status);
	return status;
  }
  ioChan = chosen->ioChan;

  /* Determine if there is already a run active */
  if ((status  = dxp_isrunning(detChan, &active))!=DXP_SUCCESS) {
	sprintf(info_string, "Failed to determine run status of detector channel %d", *detChan);
	dxp_log_error("dxp_start_control_task", info_string, status);
	return status;
  }

  if((active == 0) || ((active & 0x1) != 0)) {
	/* Continue with starting new run */
	ioChan = chosen->ioChan;
	if((status=chosen->btype->funcs->dxp_begin_control_task(&ioChan, &modChan, type, 
															length, info, chosen))!=DXP_SUCCESS){
	  sprintf(info_string,"error beginning run for module %d",chosen->mod);
	  dxp_log_error("dxp_start_control_task",info_string,status);
	  return status;
	}
	/* Change the system status */
	chosen->state[0] = 1;				/* Run is active				*/

	dxp_log_debug("dxp_start_control_task", "Setting chosen->state[4]");

	chosen->state[4] = *type;			/* Store the control task type	*/
  } else if ((active&0x4)!=0) {
	status = DXP_RUNACTIVE;
	sprintf(info_string,"Xerxes thinks a control task is active in module %d, please end before starting new run", 
			chosen->mod);
	dxp_log_error("dxp_start_control_task",info_string,status);
  } else if ((active&0x2)!=0) {
	status = DXP_RUNACTIVE;
	sprintf(info_string,"Xerxes thinks a run is active in module %d, please end before starting new run", 
			chosen->mod);
	dxp_log_error("dxp_start_control_task",info_string,status);
  } else {
	status = DXP_RUNACTIVE;
	sprintf(info_string,"Unknown reason run is active in module %d.  This should never occur.", 
			chosen->mod);
	dxp_log_error("dxp_start_control_task",info_string,status);
  }
	
  return status;
}

/******************************************************************************
 *
 * This routine starts a control run on the specified channel.  Control runs
 * include calibrations, sleep, adc acquisition, external memory reading, etc...
 *
 ******************************************************************************/
int XERXES_API dxp_stop_control_task(int *detChan)
	 /* int *detChan;					Input: Detector channel to write to			*/
	 /* int *type;						Input: Type of control run, defined in .h	*/
{

  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];
  int ioChan, modChan;		/* IO channel and module channel numbers */
  /* Pointer to the Chosen Channel */
  Board *chosen=NULL;

  /* Translate the detector channel number to IO and channel numbers for writing. */

  if ((status  = dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) {
	sprintf(info_string, "Failed to locate detector channel %d", *detChan);
	dxp_log_error("dxp_stop_control_task", info_string, status);
	return status;
  }
  ioChan = chosen->ioChan;

  /* Stop the control task */
  if((status=chosen->btype->funcs->dxp_end_control_task(&ioChan, &modChan, chosen))!=DXP_SUCCESS){
	sprintf(info_string,"Error stopping control task for module %d",chosen->mod);
	dxp_log_error("dxp_stop_control_task",info_string,status);
	return status;
  }

  /* Change the system status */
  chosen->state[0] = 0;				/* Run is no longer active		*/
  chosen->state[4] = 0;				/* Clear the control task value */
	
  return status;
}

/******************************************************************************
 *
 * This routine retrieves information about the specified control task.  It
 * returns the information in a 20 word array.  The information can vary from 
 * control task type to type.  First two words are always length of data to be 
 * returned and estimated execution time in milliseconds.
 *
 ******************************************************************************/
int XERXES_API dxp_control_task_info(int *detChan, short *type, int *info)
	 /* int *detChan;					Input: Detector channel to write to					*/
	 /* short *type;						Input: Type of control run, defined in .h			*/
	 /* int *info;						Output: Array of information about the control task */
{

  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];
  int ioChan, modChan;		/* IO channel and module channel numbers */
  /* Pointer to the Chosen Channel */
  Board *chosen=NULL;

  /* Translate the detector channel number to IO and channel numbers for writing. */

  if ((status  = dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) {
	sprintf(info_string, "Failed to locate detector channel %d", *detChan);
	dxp_log_error("dxp_control_task_info", info_string, status);
	return status;
  }
  ioChan = chosen->ioChan;

  /* Get information about this control task */
  if((status=chosen->btype->funcs->dxp_control_task_params(&ioChan, &modChan, type, chosen, info))!=DXP_SUCCESS){
	sprintf(info_string,"Error retrieving information about control task %d for detector %d",*type,*detChan);
	dxp_log_error("dxp_control_task_info",info_string,status);
	return status;
  }

  return status;
}

/******************************************************************************
 *
 * This routine retrieves information about the specified control task.
 *
 ******************************************************************************/
int XERXES_API dxp_get_control_task_data(int *detChan, short *type, void *data)
	 /* int *detChan;					Input: Detector channel to write to			*/
	 /* short *type;					Input: Type of control run, defined in .h	*/
	 /* void *data;						Output: Data read back from the module		*/
{

  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];
  int ioChan, modChan;		/* IO channel and module channel numbers */
  /* Pointer to the Chosen Channel */
  Board *chosen=NULL;
  /* Run status */
  int active;
   
  /* Translate the detector channel number to IO and channel numbers for writing. */

  if ((status  = dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) {
	sprintf(info_string, "Failed to locate detector channel %d", *detChan);
	dxp_log_error("dxp_get_control_task_data", info_string, status);
	return status;
  }
  ioChan = chosen->ioChan;
    
  /* Determine if there is already a run active */
  if ((status  = dxp_isrunning(detChan, &active))!=DXP_SUCCESS) {
	sprintf(info_string, "Failed to determine run status of detector channel %d", *detChan);
	dxp_log_error("dxp_get_control_task_data", info_string, status);
	return status;
  }

  sprintf(info_string, "active = %#x", active);
  dxp_log_debug("dxp_get_control_task_data", info_string);

  if ((active == 0)||(active==4) || (active == 2)) {
	/* Continue with starting new run */
	if((status=chosen->btype->funcs->dxp_control_task_data(&ioChan, &modChan, type, chosen, data))!=DXP_SUCCESS){
	  sprintf(info_string,"error beginning run for module %d",chosen->mod);
	  dxp_log_error("dxp_get_control_task_data",info_string,status);
	  return status;
	}  
  } else if ((active&0x1)!=0) {
	status = DXP_RUNACTIVE;
	sprintf(info_string,"Run active in module %d, run must end before reading control task data", 
			chosen->mod);
	dxp_log_error("dxp_get_control_task_data",info_string,status);
	/*  } else if ((active&0x2)!=0) { */
	/*
	status = DXP_RUNACTIVE;
	sprintf(info_string,"Xerxes thinks a run is active in module %d, please end before reading data", 
			chosen->mod);
	dxp_log_error("dxp_get_control_task_data",info_string,status);
	*/
  } else {
	status = DXP_RUNACTIVE;
	sprintf(info_string,"Unknown reason run is active in module %d.  This should never occur.", 
			chosen->mod);
	dxp_log_error("dxp_get_control_task_data",info_string,status);
  }
    
  return status;
}

/******************************************************************************
 *
 * Get the index of the symbol defined by name, from within the list of 
 * DSP parameters.  This index can be used to reference the symbol value
 * in the parameter array returned by various routines
 *
 ******************************************************************************/
int XERXES_API dxp_get_symbol_index(int* detChan, char* name, unsigned short* symindex)
	 /* int *detChan;				Input: Detector channel to write to			*/
	 /* char *name;					Input: user passed name of symbol			*/
	 /* unsigned short *symindex;	Output: Index of the symbol within parameter array	*/
{
	
  int status;
  char info_string[INFO_LEN];
  int ioChan, modChan;		/* IO channel and module channel numbers */
  /* Pointer to the Chosen Channel */
  Board *chosen=NULL;

  /* Translate the detector channel number to IO and channel numbers for writing. */

  if ((status  = dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) 
	{
	  sprintf(info_string, "Failed to locate detector channel %d", *detChan);
	  dxp_log_error("dxp_set_one_dspsymbol", info_string, status);
	  return status;
	}
  ioChan = chosen->ioChan;

  /* Retrieve the index number */

  if((status=chosen->btype->funcs->dxp_loc(name, chosen->dsp[modChan], symindex))!=DXP_SUCCESS)
	{
	  sprintf(info_string, "Error searching for parameter %s", name);
	  dxp_log_error("dxp_get_symbol_index",info_string,status);
	  return status;
	}

  return status;

}

/******************************************************************************
 *
 * Get a parameter value from the DSP .  Given a symbol name, return its
 * value from a given detector number.
 *
 ******************************************************************************/
int XERXES_API dxp_get_one_dspsymbol(int* detChan, char* name, unsigned short* value)
	 /* int *detChan;					Input: Detector channel to write to			*/
	 /* char *name;						Input: user passed name of symbol			*/
	 /* unsigned short *value;			Output: returned value of the parameter		*/
{
	
  int status;
  char info_string[INFO_LEN];
  int ioChan, modChan;		/* IO channel and module channel numbers */
  /* Pointer to the Chosen Channel */
  Board *chosen=NULL;
  /* Temporary Storage for the double return value */
  double dtemp;

  /* Translate the detector channel number to IO and channel numbers for writing. */

  if ((status  = dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) 
	{
	  sprintf(info_string, "Failed to locate detector channel %d", *detChan);
	  dxp_log_error("dxp_get_one_dspsymbol", info_string, status);
	  return status;
	}
  ioChan = chosen->ioChan;

  /* Allow driver to do neede ops to prepare channel for readout */

  if((status=chosen->btype->funcs->dxp_prep_for_readout(&(chosen->ioChan), &modChan))!=DXP_SUCCESS)
	{
	  dxp_log_error("dxp_get_one_dspsymbol","Error Preparing board for readout",status);
	  return status;
	}

  /* Read the value of the symbol from DSP memory */

  if((status=chosen->btype->funcs->dxp_read_dspsymbol(&ioChan, &modChan, 
													  name, chosen, &dtemp))!=DXP_SUCCESS)
	{
	  sprintf(info_string, "Error reading parameter %s", name);
	  dxp_log_error("dxp_get_one_dspsymbol",info_string,status);
	  return status;
	}

  /* Allow driver to return board to previous state */

  if((status=chosen->btype->funcs->dxp_done_with_readout(&(chosen->ioChan), 
														 &modChan, chosen))!=DXP_SUCCESS)
	{
	  dxp_log_error("dxp_get_one_dspsymbol","Error Preparing board for readout",status);
	  return status;
	}

  /* Convert to short */
  if (dtemp > (double) USHRT_MAX) 
	{
	  sprintf(info_string, "This routine is deprecated for reading parameters >16bits \nPlease use dxp_get_dspsymbol()");
	  dxp_log_error("dxp_get_one_dspsymbol",info_string,status);
	  return status;
	}
  *value = (unsigned short) dtemp;

  return status;

}

/******************************************************************************
 *
 * Get a parameter value from the DSP .  Given a symbol name, return its
 * value (as a long) from a given detector number.
 *
 ******************************************************************************/
int XERXES_API dxp_get_dspsymbol(int* detChan, char* name, double* value)
	 /* int *detChan;					Input: Detector channel to write to			*/
	 /* char *name;						Input: user passed name of symbol			*/
	 /* double *value;			Output: returned value of the parameter		*/
{
	
  int status;
  char info_string[INFO_LEN];
  int ioChan, modChan;		/* IO channel and module channel numbers */
  /* Pointer to the Chosen Channel */
  Board *chosen=NULL;

  /* Translate the detector channel number to IO and channel numbers for writing. */

  if ((status  = dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) 
	{
	  sprintf(info_string, "Failed to locate detector channel %d", *detChan);
	  dxp_log_error("dxp_get_dspsymbol", info_string, status);
	  return status;
	}
  ioChan = chosen->ioChan;

  /* Allow driver to do neede ops to prepare channel for readout */

  if((status=chosen->btype->funcs->dxp_prep_for_readout(&(chosen->ioChan), &modChan))!=DXP_SUCCESS)
	{
	  dxp_log_error("dxp_get_dspsymbol","Error Preparing board for readout",status);
	  return status;
	}

  /* Read the value of the symbol from DSP memory */

  if((status=chosen->btype->funcs->dxp_read_dspsymbol(&ioChan, &modChan, 
													  name, chosen, value))!=DXP_SUCCESS)
	{
	  sprintf(info_string, "Error reading parameter %s", PRINT_NON_NULL(name));
	  dxp_log_error("dxp_get_dspsymbol",info_string,status);
	  return status;
	}
	
  /* Allow driver to return board to previous state */

  if((status=chosen->btype->funcs->dxp_done_with_readout(&(chosen->ioChan), 
														 &modChan, chosen))!=DXP_SUCCESS)
	{
	  dxp_log_error("dxp_get_dspsymbol","Error Preparing board for readout",status);
	  return status;
	}
	
  return status;

}

/******************************************************************************
 *
 * Set a parameter of the DSP.  Pass the symbol name, value to set and detector
 * channel number.
 *
 ******************************************************************************/
int XERXES_API dxp_set_one_dspsymbol(int* detChan, char* name, unsigned short* value)
	 /* int *detChan;				Input: Detector channel to write to			*/
	 /* char *name;					Input: user passed name of symbol			*/
	 /* unsigned short *value;		Input: Value to set the symbol to			*/
{
	
  int status;
  char info_string[INFO_LEN];
  int ioChan, modChan;		/* IO channel and module channel numbers */
  /* Pointer to the Chosen Channel */
  Board *chosen=NULL;
  int runstat = 0;

  /* Translate the detector channel number to IO and channel numbers for writing. */

  if ((status  = dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) 
	{
	  sprintf(info_string, "Failed to locate detector channel %d", *detChan);
	  dxp_log_error("dxp_set_one_dspsymbol", info_string, status);
	  return status;
	}
  ioChan = chosen->ioChan;

  /* Check if there is a run in progress on this board */
  if ((status = dxp_isrunning(detChan, &runstat))!=DXP_SUCCESS) 
	{
	  sprintf(info_string, "Failed to determine the run status of detChan %d", *detChan);
	  dxp_log_error("dxp_set_one_dspsymbol", info_string, status);
	  return status;
	}

  if (runstat!=0) 
	{
	  status = DXP_RUNACTIVE;
	  sprintf(info_string, "You must stop the run before modifying DSP parameters for detChan %d"
			  ", runstat = %#x", *detChan, runstat);
	  dxp_log_error("dxp_set_one_dspsymbol", info_string, status);
	  return status;
	}
	
  /* Write the value of the symbol into DSP memory */

  if((status=chosen->btype->funcs->dxp_modify_dspsymbol(&ioChan, &modChan, 
														name, value, chosen))!=DXP_SUCCESS)
	{
	  sprintf(info_string, "Error writing parameter %s", PRINT_NON_NULL(name));
	  dxp_log_error("dxp_set_one_dspsymbol",info_string,status);
	  return status;
	}
	
  return status;
}

/******************************************************************************
 *
 * Set a parameter of the DSP in all channels.  Pass the symbol name 
 * and value to set.
 *
 ******************************************************************************/
int XERXES_API dxp_set_dspsymbol(char* name, unsigned short* value)
	 /* char *name;					Input: user passed name of symbol			*/
	 /* unsigned short *value;		Input: Value to set the symbol to			*/
{

  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];
  int i;
  /* Pointer to the Current Channel */
  Board *current = system_head;
  int runstat = 0;

  /* Loop over the detector channels and set the value */

  while (current!=NULL) 
	{
	  for (i=0; i<(int) current->nchan; i++) 
		{
		  /* Check if there is a run in progress on this board */
		  if ((status = dxp_isrunning(&(current->detChan[i]), &runstat))!=DXP_SUCCESS) 
			{
			  sprintf(info_string, "Failed to determine the run status of detChan %d", 
					  current->detChan[i]);
			  dxp_log_error("dxp_set_one_dspsymbol", info_string, status);
			  return status;
			}
			
		  if (runstat!=0) 
			{
			  status = DXP_RUNACTIVE;
			  sprintf(info_string, "You must stop the run before modifying DSP parameters for detChan %d", 
					  current->detChan[i]);
			  dxp_log_error("dxp_set_one_dspsymbol", info_string, status);
			  return status;
			}
	
		  if ((status  = dxp_set_one_dspsymbol(&(current->detChan[i]), 
											   name, value))!=DXP_SUCCESS) 
			{
			  sprintf(info_string, "Failed to set %s in detector channel %d", 
					  PRINT_NON_NULL(name), current->detChan[i]);
			  dxp_log_error("dxp_set_dspsymbol", info_string, status);
			  return status;
			}
		}
	  current = current->next;
	}

  return status;

}

/******************************************************************************
 * Routine to return a list of DSP symbol names to the user.
 *
 * The user must allocate the memory for the list of symbols and the 
 * integer containing the number of symbols.  If dynamic allocation 
 * is desired, then a call to dxp_num_symbols() should be made and 
 * the memory allocated accordingly.  All symbols have a maximum size
 * of MAXSYMBOL_LEN characters.
 *
 ******************************************************************************/
int XERXES_API dxp_symbolname_list(int* detChan, char** list)
	 /* int *detChan;					Input: detector channel number	*/
	 /* char **list;						Output: List of DSP symbol names */
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];
  int modChan;
  int i;	
  /* Pointer to the Chosen Channel */
  Board *chosen=NULL;

  /* Translate the detector channel number to IO and channel numbers for writing. */

  if ((status = dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) 
	{
	  sprintf(info_string, "Failed to locate detector channel %d", *detChan);
	  dxp_log_error("dxp_symbolname_list", info_string, status);
	  return status;
	}
	
  if (list==NULL) 
	{
	  status = DXP_NOMEM;
	  sprintf(info_string,"No Memory Allocated for symbolnames");
	  dxp_log_error("dxp_symbolname_list",info_string,status);
	  return status;
	}

  /* Copy the list of parameter names */
  for (i=0;i<chosen->dsp[modChan]->params->nsymbol;i++) 
	{
	  strcpy(list[i], chosen->dsp[modChan]->params->parameters[i].pname);
	}

  return status;
}


/** @brief Returns the DSP parameter name located at the specified index.
 *
 * @a name needs to be at least MAXSYMBOL_LEN characters long.
 *
 */
int XERXES_API dxp_symbolname_by_index(int* detChan, unsigned short *lindex,
									   char* name)
{
  int status;
  int modChan;

  char info_string[INFO_LEN];

  Board *chosen = NULL;


  if (!detChan) {
	dxp_log_error("dxp_symbolname_by_index", "Passed in 'detChan' reference is "
				  "NULL", DXP_NULL);
	return DXP_NULL;
  }

  if (!lindex) {
	sprintf(info_string, "Passed in 'lindex' reference is NULL for detChan %d",
			*detChan);
	dxp_log_error("dxp_symbolname_by_index", info_string, DXP_NULL);
	return DXP_NULL;
  }

  if (!name) {
	sprintf(info_string, "Passed in 'name' string is NULL for detChan %d",
			*detChan);
	dxp_log_error("dxp_symbolname_by_index", info_string, DXP_NULL);
	return DXP_NULL;
  }

  status = dxp_det_to_elec(detChan, &chosen, &modChan);

  if (status != DXP_SUCCESS) {
	  sprintf(info_string, "Failed to locate detector channel %d", *detChan);
	  dxp_log_error("dxp_symbolname_by_index", info_string, status);
	  return status;
  }

  status = chosen->btype->funcs->dxp_get_symbol_by_index(modChan, *lindex, chosen,
														 name);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error getting name of symbol located at index %u for "
			"detChan %d", *lindex, *detChan);
	dxp_log_error("dxp_symbolname_by_index", info_string, status);
	return status;
  }

  return DXP_SUCCESS;
}


/******************************************************************************
 * Routine to return a list of DSP symbol information to the user.
 *
 * The user must allocate the memory for the list of read-write codes,
 * lower and uppper bounds.  If dynamic allocation is desired, then a 
 * call to dxp_num_symbols() should be made and 
 * the memory allocated accordingly.  
 *
 ******************************************************************************/
int XERXES_API dxp_symbolname_limits(int* detChan, unsigned short *access, 
									 unsigned short *lbound, unsigned short *ubound)
	 /* int *detChan;				Input: detector channel number						*/
	 /* unsigned short *access;		Output: Access information for each DSP parameter	*/
	 /* unsigned short *lbound;		Output: lower bound for each DSP parameter			*/
	 /* unsigned short *ubound;		Output: upper bound for each DSP parameter			*/
{
  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];
  int modChan;
  unsigned short i;
  /* Pointer to the Chosen Channel */
  Board *chosen=NULL;

  /* Translate the detector channel number to IO and channel numbers for writing. */

  if ((status = dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) 
	{
	  sprintf(info_string, "Failed to locate detector channel %d", *detChan);
	  dxp_log_error("dxp_symbolname_limits", info_string, status);
	  return status;
	}

  /* Was Memory allocated for the access list? */
  if (access != NULL) 
	{
	  /* Copy the list of access information */
	  for (i=0; i < chosen->dsp[modChan]->params->nsymbol ;i++) 
		access[i] = chosen->dsp[modChan]->params->parameters[i].access;
	}

  /* Was Memory allocated for the lower bounds list? */
  if (lbound != NULL) 
	{
	  /* Copy the list of lower bounds information */
	  for (i=0; i < chosen->dsp[modChan]->params->nsymbol; i++) 
		lbound[i] = chosen->dsp[modChan]->params->parameters[i].lbound;
	}

  /* Was Memory allocated for the upper bounds list? */
  if (ubound != NULL) 
	{
	  /* Copy the list of upper bounds information */
	  for (i=0; i < chosen->dsp[modChan]->params->nsymbol; i++) 
		ubound[i] = chosen->dsp[modChan]->params->parameters[i].ubound;
	}

  return status;
}


/** @brief Returns the number of DSP parameters for the specificed channel.
 *
 */
XERXES_EXPORT int XERXES_API dxp_max_symbols(int* detChan,
											 unsigned short* nsymbols)
{
  int status;
  int modChan;

  char info_string[INFO_LEN];

  Board *chosen = NULL;


  if (!detChan) {
	dxp_log_error("dxp_max_symbols", "'detChan' can not be NULL",
				  DXP_NULL);
	return DXP_NULL;
  }

  if (!nsymbols) {
	sprintf(info_string, "'nsymbols' can not be NULL trying to "
			"get the number of DSP parameters for detChan = %d", *detChan);
	return DXP_NULL;
  }

  status = dxp_det_to_elec(detChan, &chosen, &modChan);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Failed to locate detector channel %d", *detChan);
	dxp_log_error("dxp_max_symbols", info_string, status);
	return status;
  }

  status = chosen->btype->funcs->dxp_get_num_params(modChan, chosen, nsymbols);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error getting number of parameters for detChan = %d",
			*detChan);
	dxp_log_error("dxp_max_symbols", info_string, status);
	return status;
  }

  return DXP_SUCCESS;
}


/******************************************************************************
 *
 * This routine reads out the paramter memory, the baseline memory and the
 * MCA spectrum for a single DXP channel.  The error words in the parameter
 * memory are first decoded and checked.  If OK, the baseline and spectrum
 * memories are read, otherwise the error is cleared and this routine is
 * exited.  Note: spectrum words are 32 bits, wheras params and baseline are
 * only 16 bits.
 *
 ******************************************************************************/
int XERXES_API dxp_readout_detector_run(int* detChan, unsigned short params[],
										unsigned long baseline[], unsigned long spectrum[])
	 /* int *detChan;						Input: detector channel number      */
	 /* unsigned short params[];				Output: parameter memory            */
	 /* unsigned short baseline[];			Output: baseline histogram          */
	 /* unsigned long spectrum[];			Output: spectrum                    */
{

  int status;
  char info_string[INFO_LEN];

  int ioChan, modChan;
  /* Pointer to the Chosen Board */
  Board *chosen=NULL;

  /* Get the ioChan and modChan number that matches the detChan */

  if((status=dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS){
	sprintf(info_string,"Error finding detector number %d",*detChan);
	dxp_log_error("dxp_readout_detector_run",info_string,status);
	return status;
  }
  ioChan = chosen->ioChan;
	
  /* wrapper for the real readout routine */

  if((status=dxp_readout_run(chosen, &modChan, params, baseline, spectrum))!=DXP_SUCCESS){
	dxp_log_error("dxp_readout_detector_run","error reading out run data",status);
	return status;
  }

  return status;
}

/** @brief Retrieves the SCA buffer data
 *
 */
XERXES_EXPORT int XERXES_API dxp_readout_sca(int *detChan, unsigned long *sca)
{
  int status;
  int modChan;

  char info_string[INFO_LEN];

  Board *chosen = NULL;

 
  status = dxp_det_to_elec(detChan, &chosen, &modChan);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error finding channel information for detChan '%d'", *detChan);
	dxp_log_error("dxp_readout_sca", info_string, status);
	return status;
  }

  status = chosen->btype->funcs->dxp_read_sca(&(chosen->ioChan), &modChan, chosen, sca);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error reading SCA data for detChan '%d'", *detChan);
	dxp_log_error("dxp_readout_sca", info_string, status);
	return status;
  }

  return DXP_SUCCESS;
}


/******************************************************************************
 *
 * This routine reads out the paramter memory, the baseline memory and the
 * MCA spectrum for a single DXP channel.  The error words in the parameter
 * memory are first decoded and checked.  If OK, the baseline and spectrum
 * memories are read, otherwise the error is cleared and this routine is
 * exited.  Note: spectrum words are 32 bits, wheras params and baseline are
 * only 16 bits.
 *
 ******************************************************************************/
static int dxp_readout_run(Board* board, int* modChan, unsigned short params[],
						   unsigned long baseline[], unsigned long spectrum[])
{

  int status;

  /* Allow driver to do neede ops to prepare channel for readout */

  if((status=board->btype->funcs->dxp_prep_for_readout(&(board->ioChan), &allChan))!=DXP_SUCCESS){
	dxp_log_error("dxp_readout_run","Error Preparing board for readout",status);
	return status;
  }

  /* Perform the read */

  if((status=dxp_do_readout(board, modChan, params, baseline, spectrum))!=DXP_SUCCESS){
	dxp_log_error("dxp_readout_run","error reading data",status);
	return status;
  }


  /* Allow driver to return board to previous state */

  if((status=board->btype->funcs->dxp_done_with_readout(&(board->ioChan), 
														&allChan, board))!=DXP_SUCCESS){
	dxp_log_error("dxp_readout_run","Error Preparing board for readout",status);
	return status;
  }

  return status;
}

/******************************************************************************
 *
 * This routine reads out the paramter memory, the baseline memory and the
 * MCA spectrum for a single DXP channel.  The error words in the parameter
 * memory are first decoded and checked.  If OK, the baseline and spectrum
 * memories are read, otherwise the error is cleared and this routine is
 * exited.  Note: spectrum words are 32 bits, wheras params and baseline are
 * only 16 bits.
 *
 ******************************************************************************/
static int dxp_do_readout(Board* board, int* modChan, unsigned short params[],
						  unsigned long baseline[], unsigned long spectrum[])
{
  int total_syms = 0;
  unsigned int i;
  char info_string[INFO_LEN];
  int status, error;
  unsigned short runerror,errinfo; 

  /* Read out the parameters from the DSP memory */
  status = board->btype->funcs->dxp_read_dspparams(&(board->ioChan), modChan, 
												   board, 
												   board->params[*modChan]);
  if (status != DXP_SUCCESS)
	{
	  dxp_log_error("dxp_do_readout","error reading parameters",status);
	  return status;
	}


  /* If a System DSP is defined, used that instead of the normal DSP. */
  if (params != NULL) {
	if (board->system_dsp) {

	  total_syms = board->system_dsp->params->nsymbol +
		board->system_dsp->params->n_per_chan_symbols;
	} else {

	  total_syms = board->dsp[*modChan]->params->nsymbol;
	}

	for (i = 0; i < (unsigned int)total_syms; i++) {
	  params[i] = board->params[*modChan][i];
	}
  }

  /* Pull out the RUNERROR and ERRINFO parameters from the DSP parameter list
   * and act on it accordingly */
  status = board->btype->funcs->dxp_decode_error(board->params[*modChan], board->dsp[*modChan], 
												   &runerror, &errinfo);
  if (status != DXP_SUCCESS)
	{
	  dxp_log_error("dxp_do_readout","Error decoding error information",status);
	  return status;
	}

  if(runerror!=0)
	{
	  /* Continue */
	  error = DXP_DSPRUNERROR;
	  sprintf(info_string, "RUNERROR = %d ERRINFO = %d", runerror, errinfo);
	  dxp_log_error("dxp_readout_run", info_string, error);

	  status = board->btype->funcs->dxp_clear_error(&(board->ioChan), modChan, 
													board);
	  if (status != DXP_SUCCESS)
		{
		  dxp_log_error("dxp_do_readout","Error clearing error",status);
		  return status;
		}

	  return error;
	}

  /* Read out the spectrum memory now (also known as MCA memory) */

  /* Only read spectrum if there is allocated memory */
  if (spectrum!=NULL) 
	{
	  status = board->btype->funcs->dxp_read_spectrum(&(board->ioChan),modChan,
													  board, spectrum);
	  if (status != DXP_SUCCESS)
		{
		  dxp_log_error("dxp_do_readout", "Error reading out spectrum", status);
		  return status;
		}
	}

  /* Read out the basline histogram. */

  /* Only read baseline if there is allocated memory */
  if (baseline!=NULL) 
	{
	  status = board->btype->funcs->dxp_read_baseline(&(board->ioChan),modChan,
													  board, baseline);
	  if (status != DXP_SUCCESS)
		{
		  dxp_log_error("dxp_do_readout","error reading out baseline",status);
		  return status;
		}
	}
  
  return status;
}


/******************************************************************************
 *
 * This routine writes the DSP parameter list read from the specified 
 * detector channel to the file pointed to by lun.  
 *
 * WARNING: dxp_open_file (DXP_OPEN_FILE_FORTRAN) must be called prior to 
 * this routine.  
 *
 ******************************************************************************/
int XERXES_API dxp_save_dspparams(int* detChan, int* lun)
	 /* int *detChan;						Input: detector channel to get params  */
	 /* int *lun;							Input: logical unit number to write to */
{
  int status = DXP_SUCCESS;
  char info_string[INFO_LEN];
  int ioChan, modChan, nsymb;
  char *symbolname=NULL;
  FILE *fout;

  unsigned short par, i;
  /* Pointer to the chosen Board */
  Board *chosen=NULL;

  /* Get the ioChan and dxpChan number that matches the detChan */

  if((status=dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS){
	sprintf(info_string,"Error finding detector number %d",*detChan);
	dxp_log_error("dxp_save_dspparams",info_string,status);
	return status;
  }
  ioChan = chosen->ioChan;
	
  /* Read the parameters into memory */
        
  if((status=chosen->btype->funcs->dxp_read_dspparams(&ioChan, &modChan, 
													  chosen, chosen->params[modChan]))!=DXP_SUCCESS){
	sprintf(info_string,"Error writing parameters to detector %d",*detChan);
	dxp_log_error("dxp_save_dspparams",info_string,status);
	return status;
  }

  /* Write the parameters to the file */

  /*
   * Make sure the file is previously opened
   */
  fout = filpnt[*lun];
  if(fout==NULL){
	status=DXP_OUTPUT_UNDEFINED;
	dxp_log_error("dxp_save_dspparams","output file not opened",status);
	return status;
  }
  /* 
   * Now attach a header to the file 
   */
  fprintf(fout,"* \r\n");
  fprintf(fout,"* Save file for detector channel %d\r\n", *detChan);
  fprintf(fout,"* \r\n");
  fprintf(fout,"* This file contains the contents of all DSP\r\n");
  fprintf(fout,"* parameters.\r\n");
  fprintf(fout,"* \r\n");
  /*
   * Now write out the list
   */
  /* Allocate memory for the symbolname */
  symbolname = (char *) xerxes_md_alloc(MAXSYMBOL_LEN*sizeof(char));

  nsymb = chosen->dsp[modChan]->params->nsymbol;
  for (i=0; i < nsymb; i++) {

	/* Get the symbolname */
	if((status=chosen->btype->funcs->dxp_symbolname(&i, chosen->dsp[modChan], symbolname))!=DXP_SUCCESS){
	  xerxes_md_free(symbolname);
	  symbolname = NULL;
	  sprintf(info_string,"Error getting symbol #%d", i);
	  dxp_log_error("dxp_save_dspparams",info_string,status);
	  return status;
	}
	/* Write the symbolname-value pair to the file */
	par = chosen->dsp[modChan]->data[i];
	fprintf(fout,"%s\t\t%du\r\n", symbolname, par);
  }

  /* Free memory */
  xerxes_md_free(symbolname);
  symbolname = NULL;

  /* Write an ender section */
  fprintf(fout,"* \r\n");
  fprintf(fout,"* This file was generated by the XIA DXP Library\r\n");
  fprintf(fout,"* END OF FILE\r\n");

  return status;
}

/******************************************************************************
 *
 * This routine reads the parameter memory of all DXP channels and writes
 * the data in hexidecimal format to the file pointed to by lun.  
 *
 * WARNING: dxp_open_file (DXP_OPEN_FILE_FORTRAN) must be called prior to 
 * this routine.  
 *
 ******************************************************************************/
int XERXES_API dxp_save_config(int* lun)
	 /* int *lun;							Input: logical unit number to write to */
{

  int chan;
  char info_string[INFO_LEN];
  int status;
  unsigned short i, nsymb;
  unsigned short nsymbols;
  char symbol[MAXSYMBOL_LEN];
  FILE *fout;

  Board_Info *current_btype = NULL;
  char current_ifacename[132];
  char current_dllname[132];
  Fippi_Info *current_fippi = NULL;
  Dsp_Info *current_dsp = NULL;

  unsigned short par;
  /* Pointer to the current Board */
  Board *current=system_head;

  /*
   * Make sure the file is previously opened
   */
  fout = filpnt[*lun];
  if(fout==NULL){
	status=DXP_OUTPUT_UNDEFINED;
	dxp_log_error("dxp_save_config","output file not opened",status);
	return status;
  }
  /* First put the preamp configuration file at the top of the save file */
  if (info->preamp==NULL) {
	fprintf(fout, "preamp          NULL\n");
  } else {
	fprintf(fout, "preamp          %s\n", info->preamp);
  }

  /* Now loop over boards in the system to define all entries */
  while (current!=NULL) {
	if (current->btype != current_btype) {
	  current_btype = current->btype;
	  fprintf(fout, "board_type      %s\n", current_btype->name);
	}
	if (!STREQ(current->iface->ioname,current_dllname)) {
	  strcpy(current_dllname, current->iface->ioname);
	  fprintf(fout, "iolibrary       %s\n", current_dllname);
	}
	if (!STREQ(current->iface->dllname,current_ifacename)) {
	  strcpy(current_ifacename, current->iface->dllname);
	  fprintf(fout, "interface       %s\n", current_ifacename);
	}
	/* Print out the module number and information */
	fprintf(fout, "module          %s ", current->iostring);
	for (i=0;i<current->nchan;i++) {
	  fprintf(fout,"%i ", current->detChan[i]);
	}
	fprintf(fout, "\n");
	/* Now do the FIPPI information */
	current_fippi = current->fippi[0];
	fprintf(fout, "default_fippi   %s\n", current->fippi[0]->filename);
	for (i=1;i<current->nchan;i++) {
	  if (current_fippi != current->fippi[i]) {
		fprintf(fout, "fippi           %i %s\n", i, current->fippi[i]->filename);
	  }
	}
	/* Now do the DSP information */
	current_dsp = current->dsp[0];
	fprintf(fout, "default_dsp     %s\n", current->dsp[0]->filename);
	for (i=1;i<current->nchan;i++) {
	  if (current_dsp != current->dsp[i]) {
		fprintf(fout, "dsp             %i %s\n", i, current->dsp[i]->filename);
	  }
	}
	/* Now do the DSP information */
	/*		current_defaults = current->defaults[0];
			fprintf(fout, "default_param   %s\n", current->defaults[0]->filename);
			for (i=1;i<current->nchan;i++) {
			if (current_defaults != current->defaults[i]) {
			fprintf(fout, "param           %i %s\n", i, current->defaults[i]->filename);
			}
			}*/
	fprintf(fout, "default_param   NULL\n");
	current = current->next;
  }
  fprintf(fout,"END\n");
  /*
   *    Now, loop over channels and write parameter memory to configuration
   */
  current = system_head;
  while (current!=NULL) {
	for(chan=0;chan<(int) current->nchan;chan++){
	  if( ((current->used)&(1<<chan))==0) continue;
	  fprintf(fout,"********  detector channel %d ********\n",
			  current->detChan[chan]);
	  /* Now write all the symbols in the DSP memory to the file. */
	  nsymbols = current->dsp[chan]->params->nsymbol;
	  fprintf(fout,"Symbols = %d\n",nsymbols);
	  nsymb = 0;
	  for(i = 0;i < nsymbols;i++){
		if (nsymb==0) {
		  fprintf(fout,"%2i:",i/6);
		}
		current->btype->funcs->dxp_symbolname(&i, current->dsp[chan], symbol);
		fprintf(fout,"%12s ",symbol);
		nsymb++;
		if(nsymb==6){
		  fprintf(fout,"\n");
		  nsymb=0;
		}
	  }
	  if(nsymb!=0) fprintf(fout,"\n");

	  /* Read in the parameter values from the hardware */
	  if((status=current->btype->funcs->dxp_read_dspparams(&(current->ioChan),
														   &chan, current, current->params[chan]))!=DXP_SUCCESS){
		sprintf(info_string,
				"Error reading out parameters for mod %d chan %d",current->mod,chan);
		dxp_log_error("dxp_save_config",info_string,status);
		return status;
	  }
	  nsymb = 0;
	  for(i=0;i<nsymbols;i++){
		if (nsymb==0) {
		  fprintf(fout,"%2i:",i/6);
		}
		/* Equate to an unsigned short to avoid sign-extending the parameter */
		par = current->params[chan][i];
		fprintf(fout,"%12X ", par);
		current->btype->funcs->dxp_symbolname(&i, current->dsp[chan], symbol);
		nsymb++;
		if(nsymb==6){
		  fprintf(fout,"\n");
		  nsymb=0;
		}
	  }
	  if(nsymb!=0) fprintf(fout,"\n");
	}
	current = current->next;
  }

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * This routine reads a configuration file (written by dxp_save_config) and  
 * restores the parameter memory of all DXP channels   
 *
 * WARNING: dxp_open_file (DXP_OPEN_FILE_FORTRAN) must be called prior to 
 * this routine.  
 *
 ******************************************************************************/
int XERXES_API dxp_restore_config(int* lun)
	 /* int *lun;				Input: logical unit number configuration file  */
{
   
  char info_string[INFO_LEN];
  char line[LINE_LEN];
  char temp_line[LINE_LEN];
  char strtmp[LINE_LEN];

  char *token   = NULL;
  char *cstatus = NULL;
  /* Local Delimiter to fix problem
   * associated with BUG ID 42.
   */
  char *localDelim = " ,=:\t\r\n";

  FILE *fp;  

  int status;
  int nsymbols;
  int ioChan;
  int modChan;
  int detChan = 0;

  unsigned short i, j;
  unsigned short nparam;
  unsigned short par;
  unsigned short new_nsymbol;

  unsigned short *addr   = NULL;
  unsigned short *params = NULL;
  
  /* Pointer to the chosen Board */
  Board *chosen = NULL;


  fp = filpnt[*lun];
    
  if(fp == NULL){  
	status = DXP_INPUT_UNDEFINED;  
	dxp_log_error("dxp_restore_config","Input file not opened", status);  
	return status;  
  }
  
  /*  
   *   Read configuration file:  num mods    
   */        
  do {  
    /* skip comments and blank lines */
    cstatus = xerxes_md_fgets(line, 132, fp);
  
  } while((line[0] == '*') ||
		  (line[0] == '\r')||
		  (line[0] == '\n'));  
  
  /* Make a copy of the line, since strtok buggers it up */
  strcpy(temp_line, line);

  token = strtok(line, delim);  
  
  for (i = 0; i < strlen(token); i++) { 
	token[i] = (char) tolower(token[i]);
  }

  if (!STREQ(token, "preamp")) {
	status = DXP_INPUT_UNDEFINED;
	sprintf(info_string, "Save files must start with a preamp entry");
	dxp_log_error("dxp_restore_config", info_string, status);
	return status;
  }

  /* Now search for the filename starting at m+1 */
  dxp_pick_filename(strlen(token) + 1, temp_line, strtmp);

  /* Now assign the preamp file name to the global structure */
  if (info->preamp != NULL) {
	xerxes_md_free(info->preamp);
  }

  info->preamp = (char *) xerxes_md_alloc((strlen(strtmp) + 1) * sizeof(char));
  strcpy(info->preamp, strtmp);

  /*  
   *   Read previously saved modules portion of the configuration file
   */
  status = dxp_read_modules(fp);
  
  if (status != DXP_SUCCESS) {
	/* Use the status returned by dxp_read_modules() */
	/*status = DXP_INITIALIZE;*/
	sprintf(info_string, "Could not read in modules file : %s", info->modules);
	dxp_log_error("dxp_restore_config", info_string, status);
	return status;
  }

  /*
   *  Download the FiPPI firmware
   */
  dxp_log_info("dxp_restore_config","calling dxp_fipconfig");
  
  status = dxp_fipconfig();

  if(status != DXP_SUCCESS) {
	dxp_log_error("dxp_restore_config", "Error Downloading FiPPi(s)", status);
	return status;
  }

  /*
   *  Download the DSP firmware
   */
  dxp_log_info("dxp_restore_config","calling dxp_dspconfig");

  status = dxp_dspconfig();

  if(status != DXP_SUCCESS) {
	dxp_log_error("dxp_restore_config", "Error Downloading DSP(s)", status);
	return status;
  }

  for(i = 0; i < numDxpChan; i++){  
	/*  
	 *   Read configuration file: Symbols  
	 *      store their in-DXP addresses.  
	 *    addr[file symbol position] = (symbol in dxp) ? symbol address in DXP: -1  
	 */
  
	/* Get the Detector number */
	xerxes_md_fgets(line, 132, fp);
  
	token = strtok(line, delim);

	for(j = 0; j < 3; j++) {
	  token = strtok(NULL, delim);
	}
  
	detChan = (int) strtol(token, NULL, 10);  

	/* Find Detector number in the system list */
	status = dxp_det_to_elec(&detChan, &chosen, &modChan);

	if(status != DXP_SUCCESS) {
	  sprintf(info_string, "Error finding detector number %d", detChan);
	  dxp_log_error("dxp_restore_config", info_string, status);
	  return status;
	}

	ioChan = chosen->ioChan;

	/*  
	 * Read the param block and substitite the values from the file.  Then write the params  
	 * back in.
	 */  

	/* Read in the parameter names */

	/* NEW: Read them in from the board, first */
	sprintf(info_string, "chosen->dsp[modChan]->params->nsymbol = %u",
			chosen->dsp[modChan]->params->nsymbol);
	dxp_log_debug("dxp_restore_config", info_string);

	/* Allocate memory for the DSP arrays here using the values
	 * from the new DSP code.
	 */
	new_nsymbol = chosen->dsp[modChan]->params->nsymbol;

	if (addr != NULL) {
	  xerxes_md_free(addr);
	}

	addr = (unsigned short *)xerxes_md_alloc(sizeof(unsigned short) * new_nsymbol);

	if (addr == NULL) {
	  status = DXP_NOMEM;
	  dxp_log_error("dxp_restore_config", "Out-of-memory allocating array!", status);
	  return status;
	}

	xerxes_md_fgets(line, 132, fp);
  
	token = strtok(line, localDelim);  
	token = strtok(NULL, localDelim);  
	nsymbols = (unsigned short) strtoul(token, NULL, 10);  

	/* Now for the parameter array */
	if (params != NULL) {
	  xerxes_md_free(params);
	}

	params = (unsigned short *) xerxes_md_alloc(sizeof(unsigned short) * nsymbols);

	if (params == NULL) {
	  status = DXP_NOMEM;
	  sprintf(info_string,
			  "Unable to allocate memory for the parameter array for detector channel %d",
			  detChan);
	  dxp_log_error("dxp_restore_config", info_string, status);
	  return status;
	}

	memset(params, 0, sizeof(params));
	nparam = 0;

	while(xerxes_md_fgets(line, 132, fp) != NULL) {  
	  token = strtok(line, localDelim);  
	  token = strtok(NULL, localDelim);
  
	  while(token != NULL) {
		if(isalnum(token[0]) != 0){  
		  status = chosen->btype->funcs->dxp_loc(token, chosen->dsp[modChan], &addr[nparam]);
		  
		  if (status != DXP_SUCCESS) {
			xerxes_md_free(addr);
			xerxes_md_free(params);
			sprintf(info_string, "Unrecognized parameter: %s, file = %s",
					PRINT_NON_NULL(token), chosen->dsp[modChan]->filename);  
			dxp_log_error("dxp_restore_config", info_string, status);  
			return status;  
		  }

		  nparam++;
		}

		token=strtok(NULL,localDelim);  
	  }

	  if(nparam == nsymbols) {
		break;
	  }  
	}  

	/*
	 * Read configuration file:  params  
	 * Load the parameter values
	 */
	nparam = 0;
	while(xerxes_md_fgets(line, 132, fp) != NULL) {  
	  token = strtok(line, localDelim);
	  token = strtok(NULL, localDelim);
  
	  while(token != NULL) {
		if(isxdigit(token[0]) != 0) {
		  /* Store the new value and increment number of entries. */
		  sscanf(token, "%hX", &par);
		  params[nparam] = par;
		  nparam++;
		}
		token = strtok(NULL, localDelim);  
	  }  
	  
	  if(nparam == nsymbols) {
		break;
	  }  
	}  

	/* Now write the parameters back to the board, this function checks for r/w, ro status */
	status = dxp_download_one_params(&detChan, &nsymbols, addr, params);
	
	if (status != DXP_SUCCESS) {
	  xerxes_md_free(addr);
	  xerxes_md_free(params);

	  sprintf(info_string, "Error downloading symbols to detChan %d", detChan);
	  dxp_log_error("dxp_restore_config", info_string, status);
	  return status;
	}
	
	xerxes_md_free(addr);
	addr = NULL;
	xerxes_md_free(params);
	params = NULL;

	/* Read back the current status to correct for any read-only overwrites */
	status = dxp_upload_dspparams(&detChan);

	if (status != DXP_SUCCESS) {
	  sprintf(info_string, "Error loading DSP parameters from detChan %d", detChan);
	  dxp_log_error("dxp_restore_config", info_string, status);
	  return status;
	}

	/* Old way of downloading the info to the board:
	 *
	 * if((status=dxp_download_one_params(&detChan, &nsymbols, addr, params))!=DXP_SUCCESS) break;
	 * if((status=dxp_upload_dspparams(&detChan))!=DXP_SUCCESS) break;
	 */

	/* REMOVED a bunch of "free" code here and forced it up near
	 * the call to dxp_upload_dspparams().
	 */
  }  

  info->status |= 0x1;

  return status;  
}  

/******************************************************************************
 *
 * Open a new file: return *lun that points to an entry in the fout[] array
 *
 ******************************************************************************/
int XERXES_API dxp_open_file(int* lun, char* name, int* mode)
	 /* int *lun;						Output: logical unit number to refer to file */
	 /* char *name;						Input: file name of file to open             */
	 /* int *mode;						Input: 0: open new file for output           */
	 /*         1: open new file or append to existing*/
	 /*               file for output                 */
	 /*         2: open existing file for input       */
{
  int status,i;
  char info_string[INFO_LEN];
  char *type=NULL;
  static int first=1;
  /*
   *    initialize file pointers on first call
   */
  if(first==1){
	first=0;
	dxpopen=0;
	for(i=0;i<MAXFILE;i++) filpnt[i]=NULL;
  }
  /*
   *     make sure file mode is defined...
   */
  if((*mode<0)||(*mode>2)){
	sprintf(info_string,"mode = %d unrecognized; %s not opened",*mode,PRINT_NON_NULL(name));
	status=DXP_BAD_PARAM;
	dxp_log_error("dxp_open_file",info_string,status);
	return status;
  }
  if (*mode==0) 
	type = "w";
  else if(*mode==1)
	type = "a";
  else
	type = "r";
  /*
   *      make sure not too many files are open...
   */
  dxpopen++;
  if(dxpopen==MAXFILE){
	dxpopen--;
	sprintf(info_string,"too many files open, limit = %d",MAXFILE);
	status = DXP_OPEN_FILE;
	dxp_log_error("dxp_open_file",info_string,status);
	return status;
  }
  /*
   *       find an unused pointer
   */
  for(i=0;i<MAXFILE;i++) if(filpnt[i]==NULL) break;
  *lun=i;
  if((filpnt[*lun]=xia_file_open(name,type))==NULL){
	sprintf(info_string,"Error opening %s ",PRINT_NON_NULL(name));
	status=DXP_OPEN_FILE;
	dxp_log_error("dxp_open_file",info_string,status);
	return status;
  }

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Closes the file pointed to by *lun.  If *lun=-1, then close all files.
 *
 ******************************************************************************/
int XERXES_API dxp_close_file(int* lun)
	 /* int *lun;					*/
{

  int i,status;
  char info_string[INFO_LEN];

  if(((*lun)<-1)||((*lun)>=MAXFILE)){
	status = DXP_BAD_PARAM;
	dxp_log_error("dxp_close_file","Illegal logical unit number",status);
	return status;
  }
  if(*lun==-1){
	for(i=0;i<MAXFILE;i++){
	  if(filpnt[i]!=NULL){
		if(xia_file_close(filpnt[i])==EOF){
		  sprintf(info_string,"Error closing lun = %d",i);
		  status=DXP_CLOSE_FILE;
		  dxp_log_error("dxp_close_file",info_string,status);
		}
	  }
	}
	dxpopen=0;
	return DXP_SUCCESS;
  }
  if(filpnt[*lun]==NULL){
	sprintf(info_string,"lun %d was not opened -- no action taken",*lun);
	dxp_log_info("dxp_close_file",info_string);
  } else {
	if(xia_file_close(filpnt[*lun])==EOF){
	  sprintf(info_string,"Error closing lun = %d",*lun);
	  status=DXP_CLOSE_FILE;
	  dxp_log_error("dxp_close_file",info_string,status);
	  return status;
	}
	filpnt[*lun]=NULL;
	dxpopen--;
  }

  return DXP_SUCCESS;
}

/********------******------*****------******-------******-------******------*****
 * Routines to calculate values associated with modules.  Also routines to
 * perform limited IO to the modules.
 *
 ********------******------*****------******-------******-------******------*****/

/******************************************************************************
 *
 * Download nparam values to one dectector channel's DSP memory space 
 * specified by addr[]
 *
 ******************************************************************************/
int XERXES_API dxp_download_one_params(int* detChan, int* nparam, unsigned short addr[], 
									   unsigned short value[])
	 /* int *detChan;						Input: detector channel number	  */
	 /* int *nparam;							Input: number of params to dnload  */
	 /* unsigned short addr[];				Input: address of each param       */
	 /* unsigned short value[];				Input: parameter values            */
{

  int ioChan, modChan, status;
  char info_string[INFO_LEN];
  unsigned short i;
  /* Pointer to the chosen Board */
  Board *chosen=NULL;

  /* Find the detector number in the system */

  if ((status=dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) {
	sprintf(info_string, "Unknown detector element: %d", *detChan);
	dxp_log_error("dxp_download_one_params", info_string, status);
	return status;
  }
  ioChan = chosen->ioChan;

  /* Loop over the parameters requested, writing each value[] to addr[], iff not read-only. */
        
  for(i=0;i<*nparam;i++){
	/* Check the access type and only allow writing parameters that are NOT READ-ONLY
	 */
	if (chosen->dsp[modChan]->params->parameters[addr[i]].access != 0) {
	  if((status=chosen->btype->funcs->dxp_write_dsp_param_addr(&ioChan,&modChan,
																&(chosen->dsp[modChan]->params->parameters[addr[i]].address),
																&value[i]))!=DXP_SUCCESS){
		sprintf(info_string,"error writing parameter %d to detector %d",i,*detChan);
		dxp_log_error("dxp_download_one_params",info_string,status);
		return status;
	  }
	}
  }

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 *  Download nparam values to DSP memory space specified by addr[]
 *
 ******************************************************************************/
int XERXES_API dxp_download_params(int* nparam, unsigned short addr[],
								   unsigned short value[])
	 /* int *nparam;							Input: number of params to dnload  */
	 /* unsigned short addr[];				Input: address of each param       */
	 /* unsigned short value[];				Input: parameter values            */
{

  int status;
  char info_string[INFO_LEN];
  int j;
  unsigned short i;
  /* Pointer to the current Board */
  Board *current=system_head;

  /* Loop over all modules in the system */    
	
  while (current!=NULL) {

	/* Loop over the channels of the module */
	for (j=0;j<(int) current->nchan;j++) {

	  /* Loop over the parameters requested, writing each value[] to addr[]. */
        
	  for(i=0;i<*nparam;i++){
		/* Check the access type and only allow writing parameters that are NOT READ-ONLY
		 */
		if (current->dsp[j]->params->parameters[addr[i]].access != 0) {
		  if((status=current->btype->funcs->dxp_write_dsp_param_addr(&(current->ioChan), &j,
																	 &(current->dsp[j]->params->parameters[addr[i]].address),
																	 &value[i]))!=DXP_SUCCESS){
			sprintf(info_string,"error writing parameter %d to module %d channel %d",
					i,current->mod,j);
			dxp_log_error("dxp_download_params",info_string,status);
			return status;
		  }
		}
	  }
	}
	current = current->next;
  }

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Upload nparam values from DSP memory space of a single detector channel
 * starting at a specified address.
 *
 ******************************************************************************/
int XERXES_API dxp_upload_channel(int* detChan, unsigned short* nparam, 
								  unsigned short* addr, unsigned long value[])
	 /* int *detChan;							Input: detector channel to readout	*/
	 /* unsigned short *nparam;					Input: number of params to dnload	*/
	 /* unsigned short *addr;					Input: address of 1st param			*/
	 /* unsigned long value[];					Output: parameter values			*/
{

  int status;
  char info_string[INFO_LEN];
  unsigned short i;
  char name[20];

  int ioChan, modChan;
  /* Pointer to the chosen Channel */
  Board *chosen=NULL;

  double dtemp;

  /* Get the IO channel number and module channel number that matches the detChan */

  status = dxp_det_to_elec(detChan, &chosen, &modChan);
  if (status != DXP_SUCCESS) 
	{
	  sprintf(info_string, "Unknown detector element: %d", *detChan);
	  dxp_log_error("dxp_upload_channel", info_string, status);
	  return status;
	}
  ioChan = chosen->ioChan;

  for (i = *addr; i < (*addr + *nparam); i++) 
	{
	  status = chosen->btype->funcs->dxp_symbolname(&i, chosen->dsp[modChan], name);
	  if (status != DXP_SUCCESS) 
		{
		  sprintf(info_string,"Error retrieving symbol name for address %d", i);
		  dxp_log_error("dxp_upload_channel",info_string,status);
		  return status;
		}
	  
	  status = chosen->btype->funcs->dxp_read_dspsymbol(&ioChan, &modChan, name, chosen, &dtemp);
	  if (status != DXP_SUCCESS)
		{
		  sprintf(info_string,"Error reading parameters from det chan %d",*detChan);
		  dxp_log_error("dxp_upload_channel",info_string,status);
		  return status;
		}
	  value[i] = (unsigned long) dtemp;
	}

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Loop over all DXP channels and compare downloaded DECIMATION value with
 * *decimation argument.  If all the downloaded values are the same as 
 * the *decimation argument, *different is set to 0; otherwise it is set to 1
 *
 ******************************************************************************/
int XERXES_API dxp_check_decimation(unsigned short* decimation, unsigned short* different)
	 /* unsigned short *decimation;		Input: decimation value           */
	 /* unsigned short *different;		Output: 0 if all are same, else 1 */
{

  char info_string[INFO_LEN];
  unsigned int chan;
  int status, ichan;
  double value;
  /* Pointer to the current Board */
  Board *current=system_head;

  /* Loop over all the modules, checking that the decimation is correct */
    
  *different = 1;
  while (current!=NULL) {
	for(chan=0;chan<current->nchan;chan++){
	  if(((current->used)&(1<<chan))==0) continue;

	  ichan = (int) chan;
	  if((status=current->btype->funcs->dxp_read_dspsymbol(&(current->ioChan),&ichan,
														   "DECIMATION", current, &value))!=DXP_SUCCESS){
		sprintf(info_string,
				"Error reading decimation for module %d channel %d",
				current->mod,chan);
		dxp_log_error("dxp_check_decimation",info_string,status);
		return status;
	  }
	  if (((unsigned short) value) != *decimation) return status;
	}
	current = current->next;
  }

  /* No differences found, set to 0 and return. */

  *different = 0;
  return DXP_SUCCESS;
}

/******************************************************************************
 *
 *  Determine the run statistics.  Livetime, ICR, OCR.
 *
 ******************************************************************************/
int XERXES_API dxp_get_statistics(int* detChan, double* livetime, double* icr, 
								  double* ocr, unsigned long* nevents)
	 /* int *detChan;					Input: Detector Channel number			*/
	 /* double *livetime;				Output: livetime for last run (sec)		*/
	 /* double *icr;						Output: input count rate (Kcounts/sec)	*/
	 /* double *ocr;						Output: output count rate (Kcounts/sec)	*/
	 /* unsigned long *nevents;			Output: number of events in last run	*/
{
  int status;
  char info_string[INFO_LEN];

  unsigned long evts, under, over, fast, base;

  int ioChan, modChan;
  /* Pointer to Chosen Board */
  Board *chosen=NULL;

  /* Get the IO channel number and module channel number that matches the detChan */

  if ((status=dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) {
	sprintf(info_string, "Unknown detector element: %d", *detChan);
	dxp_log_error("dxp_get_statistics", info_string, status);
	return status;
  }
  ioChan = chosen->ioChan;

  /* Allow driver to do neede ops to prepare channel for readout */

  if((status=chosen->btype->funcs->dxp_prep_for_readout(&(chosen->ioChan), &modChan))!=DXP_SUCCESS){
	dxp_log_error("dxp_get_statistics","Error Preparing board for readout",status);
	return status;
  }

  /* Read out the parameter memory of the specified detector */

  if ((status=chosen->btype->funcs->dxp_read_dspparams(&ioChan, &modChan, 
													   chosen, chosen->params[modChan]))!=DXP_SUCCESS) {
	sprintf(info_string, "Unable to read parameters from detector element: %d", 
			*detChan);
	dxp_log_error("dxp_get_statistics", info_string, status);
	return status;
  }

  /* Get the statistics data from the parameter array */

  if((status=chosen->btype->funcs->dxp_get_runstats(&ioChan, &modChan, chosen,
													&evts, &under, &over, &fast, 
													&base, livetime, icr, ocr))!=DXP_SUCCESS){
	dxp_log_error("dxp_get_statistics","Error parsing statistics",status);
	return status;
  }

  /* Note: this can overflow! These values need to be checked a little
   * more carefully.
   */
  *nevents = evts + under + over;

  /* Allow driver to return board to previous state */

  if((status=chosen->btype->funcs->dxp_done_with_readout(&(chosen->ioChan), 
														 &modChan, chosen))!=DXP_SUCCESS){
	dxp_log_error("dxp_get_statistics","Error restarting board for data taking",status);
	return status;
  }

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Search the spectrum with nbins bins for the maximum entry.  Then search on
 * either side of the maximum till the spectrum entry falls below thresh.percent
 * of the peak value.  Return the lower and upper limits of the "peak".
 *
 ******************************************************************************/
int XERXES_API dxp_findpeak(long spectrum[], int* nbins, float* thresh, 
							int* lower, int* upper)
	 /* long spectrum[];					Input: MCA spectrum                    */
	 /* int *nbins;						Input: number of bins                  */
	 /* float *thresh;					Input: Fraction of peak value          */
	 /* int *lower;						Output: lower bin limit                */
	 /* int *upper;						Output: upper bin limit                */
{

  int i,pkpos= -1;
  long max=0,min;

  /* Search the spectrum for the bin with the most entries. */

  for(i=0;i<(*nbins);i++){
	if(spectrum[i]>max){
	  max=spectrum[i];
	  pkpos = i;
	}
  }
  /*
   * find upper bin limit
   */
  min = (long) ((*thresh)*((float) max));
  i=pkpos;
  while(spectrum[i]>min) i++;
  *upper = i;
  /*
   *  find lower bin limit
   */
  i = pkpos;
  while(spectrum[i]>min) i--;
  *lower = i;

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Perform a quick and dirty gaussian fit to a spectrum using following 
 * linearization method: (from Knoll, "Radiation Detection and Measurement"
 * 2nd Edition, John Wiley & Sons, 1989, p677-8...)
 *
 *   y(x) = y0*exp(-(x-x0)^2/(2*sig^2))
 *
 *   define Q(x) = y(x-1)/y(x+1), then
 *
 *   log(Q(x)) = 2*(x-x0)/sigma^2
 *
 *   Thus, fit log(Q) vs. x to a straight line...
 *
 ******************************************************************************/
int XERXES_API dxp_fitgauss0(long spectrum[], int* lower, int* upper, float* pos,
							 float* fwhm)
	 /* long spectrum[];					Input: spectrum to fit                  */
	 /* int *lower;						Input: starting bin to use in fit       */
	 /* int *upper;						Input: ending bin to use in fit         */
	 /* float *pos;						Output: fit result - peak position      */
	 /* float *fwhm;						Output: fit result - full width half max*/
{

  int s0,status=DXP_SPECTRUM,i;
  char info_string[INFO_LEN];
  double s1,sx,sy,sxx,sxy,det,a,b,fact,x,y,wt;
  long cut1=10;					/* minummm number of events in bin			*/
  int cut2=3;						/* minimum number of bins used in fit		*/ 

  s0=0;
  s1=sx=sy=sxx=sxy=0.;
  for(i= *lower;i<= *upper;i++){
	if((spectrum[i-1]<cut1)||(spectrum[i+1]<cut1))continue;
	s0++;
	x = (float)i + 0.5;
	y = log((spectrum[i-1]+0.0)/spectrum[i+1]);
	wt= 1.0/(1./spectrum[i+1]+1./spectrum[i-1]);
	s1+=wt;
	sx+=wt*x;
	sy+=wt*y;
	sxx+=wt*x*x;
	sxy+=wt*x*y;
  }
  if(s0<cut2){
	sprintf(info_string,"Too few bins: start = %d stop = %d bins = %d",
			*lower,*upper,s0);
	dxp_log_error("dxp_fitgauss0",info_string,status);
	return status;
  }
  det = s1*sxx-sx*sx;
  if(det==0.){
	dxp_log_error("dxp_fitgauss0","Singular determinant",status);
	return status;
  }
  fact = 1./det;
  a = fact*(sy*sxx-sx*sxy);
  b = fact*(s1*sxy-sx*sy);
  if(b==0.){
	dxp_log_error("dxp_fitgauss0","vanishing slope",status);
	return status;
  }
  *fwhm = (float) (2.355*sqrt(2./b));
  *pos = (float) (-a/b);

  return DXP_SUCCESS;
}


/******************************************************************************
 * 
 * Returns the electronic channel numbers (e.g. module number and channel
 * within a module) for a given detector channel.
 *
 ******************************************************************************/
int XERXES_API dxp_det_to_elec(int* detChan, Board** passed, int* dxpChan)
	 /* int *detChan;						Input: detector channel               */
	 /* Board **passed;						Output: Pointer to the correct Board   */
	 /* int *dxpChan;						Output: DXP channel number             */
{

  unsigned int chan;
  int status;
  char info_string[INFO_LEN];

  /* Point the current Board to the first Board in the system */
  Board *current = system_head;

  /* Loop through the Boards to find which module we desire */

  while (current != NULL) {
	for (chan=0;chan<current->nchan;chan++) {
	  if (*detChan==current->detChan[chan]) {
		*dxpChan = chan;
		*passed = current;
		return DXP_SUCCESS;
	  }
	}
	current = current->next;
  }

  *passed = NULL;
  sprintf(info_string,"detector channel %d is unknown",*detChan);
  status=DXP_NODETCHAN;
  dxp_log_error("dxp_det_to_elec",info_string,status);
  return status;
}

/******************************************************************************
 * 
 * Returns the electronic channel numbers (e.g. module number and channel
 * within a module) for a given detector channel.
 *
 ******************************************************************************/
int XERXES_API dxp_elec_to_det(int* ioChan, int* dxpChan, int* detChan)
	 /* int *ioChan;						Input: CAMAC I/O number				*/
	 /* int *dxpChan;					Input: DXP channel number			*/
	 /* int *detChan;					Output: detector channel			*/
{

  int status;
  char info_string[INFO_LEN];
  /* Pointer to current Board */
  Board *current=system_head;

  /* Only support up to 4 channels per module. */
	
  if (((unsigned int) *dxpChan) >= current->nchan) {
	sprintf(info_string,"No support for %d channels per module",*dxpChan);
	status=DXP_NOMODCHAN;
	dxp_log_error("dxp_elec_to_det",info_string,status);
	return status;
  }

  /* Now count forward through the module and channel list till we 
   * find the matching detector number */
    
  while (current!=NULL) {
	if (*ioChan==(current->ioChan)) {
	  *detChan = current->detChan[*dxpChan];
	  return DXP_SUCCESS;
	}
	current = current->next;
  }

  /* Should never exit loop normally => no match */
	
  sprintf(info_string,"IO Channel %d unknown",*ioChan);
  status=DXP_NOIOCHAN;
  dxp_log_error("dxp_elec_to_det",info_string,status);
  return status;
}

/******************************************************************************
 *
 * Routine to tell the MD layer to lock resources associated with detChan
 *
 ******************************************************************************/
int XERXES_API dxp_lock_resource(int *detChan, short *lock)
{
  int status = DXP_SUCCESS;
  char info_string[INFO_LEN];
  int modChan;
  /* Pointer to the Chosen Channel */
  Board *chosen=NULL;

  /* Translate the detector channel number to IO and channel numbers for writing. */

  if ((status = dxp_det_to_elec(detChan, &chosen, &modChan))!=DXP_SUCCESS) {
	sprintf(info_string, "Failed to locate detector channel %d", *detChan);
	dxp_log_error("dxp_lock_resource", info_string, status);
	return status;
  }

  /* Set the state of this resource to be locked iff *lock != 0 */

  chosen->state[3] = (short) ((*lock != 0) ? 1 : 0);
	
  /* Check to see if the lock function is implemented at the MD layer */
  if (chosen->iface->funcs->dxp_md_lock_resource != NULL) {
	if ((status = chosen->iface->funcs->dxp_md_lock_resource(&(chosen->ioChan), 
															 &modChan, lock))!=DXP_SUCCESS) {
	  sprintf(info_string, "Failed to lock resource for detector %d", *detChan);
	  dxp_log_error("dxp_lock_resource", info_string, status);
	  return status;
	}
  } else {
	status = DXP_MDLOCK;
	sprintf(info_string, "No Resource Locking function implemented for detector %d", *detChan);
	dxp_log_error("dxp_loc_resource", info_string, status);
  }

  return status;
}


/**********
 * This routine is used to send setup commands
 * to a module. Not all products support
 * this routine.
 **********/
XERXES_EXPORT int XERXES_API dxp_setup_command(int *detChan, char *name,
											   unsigned int *lenS, byte_t *send,
											   unsigned int *lenR, byte_t *receive,
											   byte_t *ioFlags)
{
  int status;
  int modChan;

  char info_string[INFO_LEN];

  Board *chosen = NULL;


  status = dxp_det_to_elec(detChan, &chosen, &modChan);

  if (status != DXP_SUCCESS) {

	sprintf(info_string, "Failed to locate detector channel %d", *detChan);
	dxp_log_error("dxp_setup_command", info_string, status);
	return status;
  }

  status = chosen->btype->funcs->dxp_setup_cmd(chosen, name, lenS, send, lenR, receive, *ioFlags);

  if (status != DXP_SUCCESS) {

	dxp_log_error("dxp_setup_command", "Setup command failed", status);
	return status;
  }

  return DXP_SUCCESS;
}
  

/** @brief Reads out an arbitrary block of memory
 *
 * Parses a name of the form: '[memory type]:[offset (hex)]:length'
 * where 'memory type' is a product specific item.
 *
 */
XERXES_EXPORT int dxp_read_memory(int *detChan, char *name, unsigned long *data)
{
  int status;
  int modChan;

  unsigned long base;
  unsigned long offset;

  char type[MAX_MEM_TYPE_LEN];
  char info_string[INFO_LEN];

  Board *chosen = NULL;


  ASSERT(detChan != NULL);
  ASSERT(name != NULL);
  ASSERT(data != NULL);

  
  status = dxp_parse_memory_str(name, type, &base, &offset);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error parsing memory string '%s'", name);
	dxp_log_error("dxp_read_memory", info_string, status);
	return status;
  }

  status = dxp_det_to_elec(detChan, &chosen, &modChan);

  if (status != DXP_SUCCESS) {

	sprintf(info_string, "Failed to locate detector channel %d", *detChan);
	dxp_log_error("dxp_read_memory", info_string, status);
	return status;
  }

  status = chosen->btype->funcs->dxp_read_mem(&(chosen->ioChan), &modChan,
											  chosen, type, &base, &offset, data);

  if (status != DXP_SUCCESS) {

	sprintf(info_string, "Error reading memory of type %s from detector channel %d",
			name, *detChan);
	dxp_log_error("dxp_read_memory", info_string, status);
	return status;
  }

  return DXP_SUCCESS;
}


/** @brief Writes to an arbitrary block of memory
 *
 */
XERXES_EXPORT int dxp_write_memory(int *detChan, char *name, unsigned long *data)
{
  int status;
  int modChan;

  unsigned long base;
  unsigned long offset;

  char type[MAX_MEM_TYPE_LEN];
  char info_string[INFO_LEN];

  Board *chosen = NULL;


  ASSERT(detChan != NULL);
  ASSERT(name != NULL);
  ASSERT(data != NULL);

  
  status = dxp_parse_memory_str(name, type, &base, &offset);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Error parsing memory string '%s'", name);
	dxp_log_error("dxp_write_memory", info_string, status);
	return status;
  }

  status = dxp_det_to_elec(detChan, &chosen, &modChan);

  if (status != DXP_SUCCESS) {

	sprintf(info_string, "Failed to locate detector channel %d", *detChan);
	dxp_log_error("dxp_write_memory", info_string, status);
	return status;
  }

  status = chosen->btype->funcs->dxp_write_mem(&(chosen->ioChan), &modChan,
											  chosen, type, &base, &offset, data);

  if (status != DXP_SUCCESS) {

	sprintf(info_string, "Error writing memory of type %s to detector channel %d",
			name, *detChan);
	dxp_log_error("dxp_write_memory", info_string, status);
	return status;
  }

  return DXP_SUCCESS;
}


/**********
 * This routine returns the length of the specified memory type.
 **********/
int dxp_memory_length(int *detChan, char *name, unsigned long *length)
{
  UNUSED(detChan);
  UNUSED(name);
  UNUSED(length);

  return DXP_SUCCESS;
}


/**********
 * This routine allows write access to certain 
 * registers.
 **********/
int dxp_write_register(int *detChan, char *name, unsigned long *data)
{
  int status;
  int modChan;

  char info_string[INFO_LEN];

  Board *chosen = NULL;


  status = dxp_det_to_elec(detChan, &chosen, &modChan);

  if (status != DXP_SUCCESS) {
	
	sprintf(info_string, "Failed to locate detector channel %d", *detChan);
	dxp_log_error("dxp_write_register", info_string, status);
	return status;
  }

  status = chosen->btype->funcs->dxp_write_reg(&(chosen->ioChan), &modChan,
											   name, data);

  if (status != DXP_SUCCESS) {

	sprintf(info_string, "Error writing to register for detector channel %d",
			*detChan);
	dxp_log_error("dxp_write_register", info_string, status);
	return status;
  }

  return DXP_SUCCESS;
}


/**********
 * This routine allows read access to certain 
 * registers. The allowed registers are completely
 * defined by the individual device drivers.
 **********/
int dxp_read_register(int *detChan, char *name, unsigned long *data)
{
  int status;
  int modChan;

  char info_string[INFO_LEN];

  Board *chosen = NULL;


  status = dxp_det_to_elec(detChan, &chosen, &modChan);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Failed to locate detector channel %d", *detChan);
	dxp_log_error("dxp_read_register", info_string, status);
	return status;
  }

  status = chosen->btype->funcs->dxp_read_reg(&(chosen->ioChan), &modChan,
											  name, data);

  if (status != DXP_SUCCESS) {
	sprintf(info_string, 
			"Error reading from register on detector channel %d",
			*detChan);
	dxp_log_error("dxp_read_register", info_string, status);
	return status;
  }

  return DXP_SUCCESS;
}


/**********
 * This routine allows access to the defined
 * command routine for a given product. Not
 * all products have a meaningful implementation
 * of the command routine.
 **********/
XERXES_EXPORT int XERXES_API dxp_cmd(int *detChan, byte_t *cmd, unsigned int *lenS,
									 byte_t *send, unsigned int *lenR, byte_t *receive)
{
  /*    DWORD before, after;
   */

  int status;
  int modChan;
    
  char info_string[INFO_LEN];
  byte_t ioFlags = IO_NORMAL;    
  
  Board *chosen = NULL;
    

  status = dxp_det_to_elec(detChan, &chosen, &modChan);
    
  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Failed to locate detector channel %d", *detChan);
	dxp_log_error("dxp_cmd", info_string, status);
	return status;
  }
    
  /*before = GetTickCount();*/

  status = chosen->btype->funcs->dxp_do_cmd(&(chosen->ioChan), *cmd, *lenS,
											send, *lenR, receive, ioFlags);
    
  /*    after = GetTickCount();*/

    /*
    sprintf(info_string, "dT = %lf", (double)((after - before) / 1000.0));
    dxp_log_debug("dxp_cmd", info_string);
    */

  if (status != DXP_SUCCESS) {
	dxp_log_error("dxp_cmd", "Command error", status);
	return status;
  }
    
  return DXP_SUCCESS;    
}


/** @brief Sets the current I/O priority of the library
 *
 */
XERXES_EXPORT int XERXES_API dxp_set_io_priority(int *priority)
{
  int status;


  if (priority == NULL) {
	dxp_log_error("dxp_set_io_priority", "'priority' may not be NULL", DXP_NULL);
	return DXP_NULL;
  }

  status = xerxes_md_set_priority(priority);

  if (status != DXP_SUCCESS) {
	dxp_log_error("dxp_set_io_priority", "Error setting I/O priority", status);
	return status;
  }

  return DXP_SUCCESS;
}


/**********
 * Calls the appropriate exit handler for the specified
 * board type.
 **********/
XERXES_EXPORT int XERXES_API dxp_exit(int *detChan)
{
  int status;
  int modChan;

  char info_string[INFO_LEN];

  Board *chosen = NULL;



  sprintf(info_string, "Preparing to unhook detChan = %d", *detChan);
  dxp_log_debug("dxp_exit", info_string);
    
  status = dxp_det_to_elec(detChan, &chosen, &modChan);

  /* If the detChan is invalid, this could be symptomatic of a problem
   * in the calling library. Perhaps it's list of detChan got corrupted, etc.
   * The fact that it is trying to call dxp_exit() could mean that it is trying
   * to recover, so we need to be as permissive as possible here. That means
   * we only return a warning if we encounter a bad detChan.
   */
  if (status != DXP_SUCCESS) {
	sprintf(info_string, "Failed to locate detector channel %d", *detChan);
	dxp_log_warning("dxp_exit", info_string);
	return DXP_SUCCESS;
  }

  status = chosen->btype->funcs->dxp_unhook(chosen);

  if (status != DXP_SUCCESS) {
	dxp_log_error("dxp_exit", "Unable to un-hook board communications", status);
	return status;
  }

  return DXP_SUCCESS;
}


/** @brief Takes a memory string and returns the parsed data
 *
 * Expects that memory has been allocated for 'type'. Expects strings
 * of the format '[memory type]:[base (hex)]:[offset]'.
 * @sa dxp_read_memory().
 *
 */
XERXES_STATIC int XERXES_API dxp_parse_memory_str(char *name, char *type, unsigned long *base, unsigned long *offset)
{
  size_t name_len = strlen(name) + 1;

  int n = 0;

  char *mem_delim = ":";
  char *full_name = NULL;
  char *tok       = NULL;

  char info_string[INFO_LEN];


  ASSERT(name != NULL);
  ASSERT(type != NULL);
  ASSERT(base != NULL);
  ASSERT(offset != NULL);


  full_name = (char *)xerxes_md_alloc(name_len);
  
  if (full_name == NULL) {
	sprintf(info_string, "Error allocating %d bytes for 'full_name'", name_len);
	dxp_log_error("dxp_parse_memory_str", info_string, DXP_NOMEM);
	return DXP_NOMEM;
  }

  strncpy(full_name, name, name_len);

  tok = strtok(full_name, mem_delim);
  
  if (tok == NULL) {
	sprintf(info_string, "Memory string '%s' is improperly formatted", name);
	dxp_log_error("dxp_parse_memory_str", info_string, DXP_INVALID_STRING);
	return DXP_INVALID_STRING;
  }

  strncpy(type, tok, strlen(tok) + 1);

  tok = strtok(NULL, mem_delim);

  if (tok == NULL) {
	sprintf(info_string, "Memory string '%s' is improperly formatted", name);
	dxp_log_error("dxp_parse_memory_str", info_string, DXP_INVALID_STRING);
	return DXP_INVALID_STRING;
  }

  n = sscanf(tok, "%lx", base);

  if (n == 0) {
	sprintf(info_string, "Memory base '%s' is improperly formatted", tok);
	dxp_log_error("dxp_parse_memory_str", info_string, DXP_INVALID_STRING);
	return DXP_INVALID_STRING;	
  }

  tok = strtok(NULL, mem_delim);

  if (tok == NULL) {
	sprintf(info_string, "Memory string '%s' is improperly formatted", name);
	dxp_log_error("dxp_parse_memory_str", info_string, DXP_INVALID_STRING);
	return DXP_INVALID_STRING;
  }

  n = sscanf(tok, "%lu", offset);

  if (n == 0) {
	sprintf(info_string, "Memory offset '%s' is improperly formatted", tok);
	dxp_log_error("dxp_parse_memory_str", info_string, DXP_INVALID_STRING);
	return DXP_INVALID_STRING;	
  }  

  xerxes_md_free(full_name);
  full_name = NULL;

  return DXP_SUCCESS;
}


/** @brief Returns various components of Xerxes's version information
 *
 * Returns the release, minor and major version numbers of the Xerxes library.
 * These values would typically be reassembled using a syntax such as
 * 'maj'.'min'.'rel'. If any of the @a rel, @a min or @a maj parameters is set to
 * NULL, only the @a pretty string is returned. The pretty string is a
 * preformatted string perfect for writing to a log file or display. The
 * @a pretty string also contains an extra tag of information indicating special
 * build information such as 'dev' or 'release'. There is currently no way to
 * retrieve the special information separate from the @a pretty string.
 *
 */
XERXES_EXPORT void XERXES_API dxp_get_version_info(int *rel, int *min, int *maj,
												   char *pretty)
{
  if (rel && min && maj) {
	*rel = XERXES_RELEASE_VERSION;
	*min = XERXES_MINOR_VERSION;
	*maj = XERXES_MAJOR_VERSION;
  }

  if (pretty) {
	sprintf(pretty, "v%d.%d.%d (%s)", XERXES_MAJOR_VERSION, XERXES_MINOR_VERSION,
			XERXES_RELEASE_VERSION, VERSION_STRING);
  }
}
