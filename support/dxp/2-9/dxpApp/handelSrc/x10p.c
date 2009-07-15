/*
 * x10p.c
 *
 *   Created        25-Sep-1996  by Ed Oltman, Brad Hubbard
 *   Extensive mods 19-Dec-1996  EO
 *   Modified:      03-Feb-1996  EO:  Added "static" qualifier to variable defs
 *       add getenv() to fopen (unix compatibility); replace
 *       #include <dxp_area/...> with #include <...> (unix compatibility)
 *       -->compiler now requires path for include files
 *       introduced LIVECLOCK_TICK_TIME for LIVETIME calculation
 *       added functions dxp_read_long and dxp_write_long
 *    Modified      05-Feb-1997 EO: introduce FNAME replacement for VAX
 *        compatability (getenv crashes if logical not defined)
 *    Modified      07-Feb-1997 EO: fix bug in dxp_read_long for LITTLE_ENDIAN
 *        machines, cast dxp_swaplong argument as (long *) in dxp_get_runstats
 *    Modified      04-Oct-97: Make function declarations compatable w/traditional
 *        C; Replace LITTLE_ENDIAN parameter with function dxp_little_endian; New
 *        function dxp_little_endian -- dynamic test; fix bug in dxp_download_dspconfig;
 *        Replace MAXBLK parameter with function call to dxp_md_get_maxblk
 *    Created  29-Jan-01 JW Copied to X10P.c from dxp4c2x.c
 *
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


#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>

#include "md_generic.h"
#include "xerxes_structures.h"
#include "xia_xerxes_structures.h"
#include "x10p.h"
#include "xerxes_errors.h"
#include "xia_x10p.h"

#include "xia_assert.h"


/* Define the length of the error reporting string info_string */
#define INFO_LEN 400
/* Define the length of the line string used to read in files */
#define LINE_LEN 132

/* Starting memory location and length for DSP parameter memory */
static unsigned short startp=START_PARAMS;
/* Variable to store the number of DAC counts per unit gain (dB) */
static double dacpergaindb;
/* Variable to store the number of DAC counts per unit gain (linear) */
static double dacpergain;
/*
 * Store pointers to the proper DLL routines to talk to the CAMAC crate
 */
static DXP_MD_IO x10p_md_io;
static DXP_MD_SET_MAXBLK x10p_md_set_maxblk;
static DXP_MD_GET_MAXBLK x10p_md_get_maxblk;
/*
 * Define the utility routines used throughout this library
 */
static DXP_MD_LOG x10p_md_log;
static DXP_MD_ALLOC x10p_md_alloc;
static DXP_MD_FREE x10p_md_free;
static DXP_MD_PUTS x10p_md_puts;
static DXP_MD_WAIT x10p_md_wait;
static DXP_MD_FGETS x10p_md_fgets;


XERXES_STATIC int dxp_read_external_memory(int *ioChan, int *modChan, Board *board,
                                           unsigned long base, unsigned long offset, unsigned long *data);
XERXES_STATIC int dxp_read_spectrum_memory(int *ioChan, int *modChan,
                                           Board *board, unsigned long base,
                                           unsigned long offset,
                                           unsigned long *data);
XERXES_STATIC int dxp_write_external_memory(int *ioChan, int *modChan, Board *board,
                                            unsigned long base, unsigned long offset, unsigned long *data);
XERXES_STATIC int dxp_wait_for_busy(int *ioChan, int *modChan, Board *board, int n_timeout, double busy, float wait);


typedef int (*memory_func_t)(int*, int *, Board *, unsigned long, unsigned long, unsigned long *);

typedef struct Mem_Op
{
  char *name;
  memory_func_t f;

}
Mem_Op_t;


static Mem_Op_t mem_readers[] =
  {
    { "external", dxp_read_external_memory
    },
    { "spectrum", dxp_read_spectrum_memory },

  };

#define NUM_MEM_READERS (sizeof(mem_readers) / sizeof(mem_readers[0]))

static Mem_Op_t mem_writers[] =
  {
    { "external", dxp_write_external_memory
    },

  };

#define NUM_MEM_WRITERS (sizeof(mem_writers) / sizeof(mem_writers[0]))


/******************************************************************************
 *
 * Routine to create pointers to all the internal routines
 *
 ******************************************************************************/
int dxp_init_dxpx10p(Functions* funcs)
/* Functions *funcs;    */
{
  funcs->dxp_init_driver = dxp_init_driver;
  funcs->dxp_init_utils  = dxp_init_utils;
  funcs->dxp_get_dspinfo = dxp_get_dspinfo;
  funcs->dxp_get_fipinfo = dxp_get_fipinfo;
  funcs->dxp_get_defaultsinfo = dxp_get_defaultsinfo;
  funcs->dxp_get_dspconfig = dxp_get_dspconfig;
  funcs->dxp_get_fpgaconfig = dxp_get_fpgaconfig;
  funcs->dxp_get_dspdefaults = dxp_get_dspdefaults;
  funcs->dxp_download_fpgaconfig = dxp_download_fpgaconfig;
  funcs->dxp_download_fpga_done = dxp_download_fpga_done;
  funcs->dxp_download_dspconfig = dxp_download_dspconfig;
  funcs->dxp_download_dsp_done = dxp_download_dsp_done;
  funcs->dxp_calibrate_channel = dxp_calibrate_channel;
  funcs->dxp_calibrate_asc = dxp_calibrate_asc;

  funcs->dxp_look_at_me = dxp_look_at_me;
  funcs->dxp_ignore_me = dxp_ignore_me;
  funcs->dxp_clear_LAM = dxp_clear_LAM;
  funcs->dxp_loc = dxp_loc;
  funcs->dxp_symbolname = dxp_symbolname;

  funcs->dxp_read_spectrum = dxp_read_spectrum;
  funcs->dxp_test_spectrum_memory = dxp_test_spectrum_memory;
  funcs->dxp_get_spectrum_length = dxp_get_spectrum_length;
  funcs->dxp_read_baseline = dxp_read_baseline;
  funcs->dxp_test_baseline_memory = dxp_test_baseline_memory;
  funcs->dxp_get_baseline_length = dxp_get_baseline_length;
  funcs->dxp_test_event_memory = dxp_test_event_memory;
  funcs->dxp_get_event_length = dxp_get_event_length;
  funcs->dxp_write_dspparams = dxp_write_dspparams;
  funcs->dxp_write_dsp_param_addr = dxp_write_dsp_param_addr;
  funcs->dxp_read_dspparams = dxp_read_dspparams;
  funcs->dxp_read_dspsymbol = dxp_read_dspsymbol;
  funcs->dxp_modify_dspsymbol = dxp_modify_dspsymbol;
  funcs->dxp_dspparam_dump = dxp_dspparam_dump;

  funcs->dxp_prep_for_readout = dxp_prep_for_readout;
  funcs->dxp_done_with_readout = dxp_done_with_readout;
  funcs->dxp_begin_run = dxp_begin_run;
  funcs->dxp_end_run = dxp_end_run;
  funcs->dxp_run_active = dxp_run_active;
  funcs->dxp_begin_control_task = dxp_begin_control_task;
  funcs->dxp_end_control_task = dxp_end_control_task;
  funcs->dxp_control_task_params = dxp_control_task_params;
  funcs->dxp_control_task_data = dxp_control_task_data;

  funcs->dxp_decode_error = dxp_decode_error;
  funcs->dxp_clear_error = dxp_clear_error;
  funcs->dxp_get_runstats = dxp_get_runstats;
  funcs->dxp_change_gains = dxp_change_gains;
  funcs->dxp_setup_asc = dxp_setup_asc;

  funcs->dxp_setup_cmd = dxp_setup_cmd;

  funcs->dxp_read_mem = dxp_read_mem;
  funcs->dxp_write_mem = dxp_write_mem;

  funcs->dxp_write_reg = dxp_write_reg;
  funcs->dxp_read_reg  = dxp_read_reg;

  funcs->dxp_do_cmd = dxp_do_cmd;

  funcs->dxp_unhook = dxp_unhook;

  funcs->dxp_get_sca_length = dxp_get_sca_length;
  funcs->dxp_read_sca = dxp_read_sca;

  funcs->dxp_get_symbol_by_index = dxp_get_symbol_by_index;
  funcs->dxp_get_num_params      = dxp_get_num_params;

  return DXP_SUCCESS;
}
/******************************************************************************
 *
 * Routine to initialize the IO Driver library, get the proper pointers
 *
 ******************************************************************************/
static int dxp_init_driver(Interface* iface)
/* Interface *iface;    Input: Pointer to the IO Interface  */
{

  /* Assign all the static vars here to point at the proper library routines */
  x10p_md_io = iface->funcs->dxp_md_io;
  x10p_md_set_maxblk = iface->funcs->dxp_md_set_maxblk;
  x10p_md_get_maxblk = iface->funcs->dxp_md_get_maxblk;

  return DXP_SUCCESS;
}
/******************************************************************************
 *
 * Routine to initialize the Utility routines, get the proper pointers
 *
 ******************************************************************************/
static int dxp_init_utils(Utils* utils)
/* Utils *utils;     Input: Pointer to the utility functions */
{

  /* Assign all the static vars here to point at the proper library routines */
  x10p_md_log = utils->funcs->dxp_md_log;

#ifdef XIA_SPECIAL_MEM
  x10p_md_alloc = utils->funcs->dxp_md_alloc;
  x10p_md_free  = utils->funcs->dxp_md_free;
#else
  x10p_md_alloc = malloc;
  x10p_md_free  = free;
#endif /* XIA_SPECIAL_MEM */

  x10p_md_wait  = utils->funcs->dxp_md_wait;
  x10p_md_puts  = utils->funcs->dxp_md_puts;
  x10p_md_fgets = utils->funcs->dxp_md_fgets;

  return DXP_SUCCESS;
}

/********------******------*****------******-------******-------******------*****
 * Now begins the routines devoted to atomic operations on DXP cards.  These
 * routines involve reading or writing to a location on the DXP card.  Nothing
 * fancy is done in these routines.  Just single or Block transfers to and from
 * the DXP.  These are the proper way to write to the CSR, etc...instead of using
 * the CAMAC F and A values throughout the code.
 *
 ********------******------*****------******-------******-------******------*****/


/******************************************************************************
 * Routine to write data to the TSAR (Transfer Start Address Register)
 *
 * This register tells the DXP where to begin transferring data on the
 * data transfer which shall begin shortly.
 *
 ******************************************************************************/
static int dxp_write_tsar(int* ioChan, unsigned short* addr)
/* int *ioChan;      Input: I/O channel of DXP module      */
/* unsigned short *addr;   Input: address to write into the TSAR */
{
  unsigned int f, len;

  unsigned long a;

  int status;

  f=DXP_TSAR_F_WRITE;
  a=DXP_TSAR_A_WRITE;
  len=0;
  status=x10p_md_io(ioChan,&f,&a,(void *)addr,&len);     /* write TSAR */
  if (status!=DXP_SUCCESS) {
    status = DXP_WRITE_TSAR;
    dxp_log_error("dxp_write_tsar","Error writing TSAR",status);
  }
  return status;
}

/******************************************************************************
 * Routine to write to the CSR (Control/Status Register)
 *
 * This register contains bits which both control the operation of the
 * DXP and report the status of the DXP.
 *
 ******************************************************************************/
static int dxp_write_csr(int* ioChan, unsigned short* data)
/* int *ioChan;      Input: I/O channel of DXP module      */
/* unsigned short *data;   Input: address of data to write to CSR*/
{
  unsigned int f, len;

  unsigned long a;

  int status;
  unsigned short saddr;

  /* write transfer start address register */

  saddr = DXP_CSR_ADDRESS;
  status = dxp_write_tsar(ioChan, &saddr);
  if (status!=DXP_SUCCESS) {
    dxp_log_error("dxp_write_csr","Error writing TSAR",status);
    return status;
  }

  /* Write the CSR data */

  f=DXP_CSR_F_WRITE;
  a=DXP_CSR_A_WRITE;
  len=1;
  status=x10p_md_io(ioChan,&f,&a,(void *)data,&len);     /* write CSR */
  if (status!=DXP_SUCCESS) {
    status = DXP_WRITE_CSR;
    dxp_log_error("dxp_write_csr","Error writing CSR",status);
  }
  return status;
}

/******************************************************************************
 * Routine to read from the CSR (Control/Status Register)
 *
 * This register contains bits which both control the operation of the
 * DXP and report the status of the DXP.
 *
 ******************************************************************************/
static int dxp_read_csr(int* ioChan, unsigned short* data)
/* int *ioChan;      Input: I/O channel of DXP module   */
/* unsigned short *data;   Output: where to put data from CSR */
{
  unsigned int f, len;

  unsigned long a;

  int status;
  unsigned short saddr;

  /* write transfer start address register */

  saddr = DXP_CSR_ADDRESS;
  status = dxp_write_tsar(ioChan, &saddr);
  if (status!=DXP_SUCCESS) {
    dxp_log_error("dxp_read_csr","Error writing TSAR",status);
    return status;
  }

  /* Read the CSR data */

  f=DXP_CSR_F_READ;
  a=DXP_CSR_A_READ;
  len=1;
  status=x10p_md_io(ioChan,&f,&a,(void *)data,&len);     /* write TSAR */
  if (status!=DXP_SUCCESS) {
    status = DXP_READ_CSR;
    dxp_log_error("dxp_read_csr","Error reading CSR",status);
  }
  return status;
}

/******************************************************************************
 * Routine to read data from the DXP
 *
 * This is the generic data transfer routine.  It can transfer data from the
 * DSP for example based on the address previously downloaded to the TSAR
 *
 ******************************************************************************/
static int dxp_read_data(int* ioChan, unsigned short* data, unsigned int len)
/* int *ioChan;       Input: I/O channel of DXP module   */
/* unsigned short *data;    Output: where to put data read     */
/* unsigned int len;     Input: length of the data to read  */
{
  unsigned int f;

  unsigned long a;

  int status;

  f=DXP_DATA_F_READ;
  a=DXP_DATA_A_READ;
  status=x10p_md_io(ioChan,&f,&a,(void *)data,&len);     /* write TSAR */
  if (status!=DXP_SUCCESS) {
    status = DXP_READ_DATA;
    dxp_log_error("dxp_read_data","Error reading data",status);
  }
  return status;
}

/******************************************************************************
 * Routine to write data to the DXP
 *
 * This is the generic data transfer routine.  It can transfer data to the
 * DSP for example based on the address previously downloaded to the TSAR
 *
 ******************************************************************************/
static int dxp_write_data(int* ioChan, unsigned short* data, unsigned int len)
/* int *ioChan;       Input: I/O channel of DXP module   */
/* unsigned short *data;    Input: address of data to write    */
/* unsigned int len;     Input: length of the data to read  */
{
  unsigned int f;

  unsigned long a;

  int status;

  f=DXP_DATA_F_WRITE;
  a=DXP_DATA_A_WRITE;
  status=x10p_md_io(ioChan,&f,&a,(void *)data,&len);     /* write TSAR */
  if (status!=DXP_SUCCESS) {
    status = DXP_WRITE_DATA;
    dxp_log_error("dxp_write_data","Error writing data",status);
  }

  return status;
}

/******************************************************************************
 * Routine to write fippi data
 *
 * This is the routine that transfers the FiPPi program to the DXP.  It
 * assumes that the CSR is already downloaded with what channel requires
 * a FiPPi program.
 *
 ******************************************************************************/
static int dxp_write_fippi(int* ioChan, unsigned short* data, unsigned int len)
/* int *ioChan;       Input: I/O channel of DXP module   */
/* unsigned short *data;    Input: address of data to write    */
/* unsigned int len;     Input: length of the data to read  */
{
  unsigned int f;

  unsigned long a;

  int status;
  unsigned short saddr;

  /* write transfer start address register */

  saddr = DXP_FIPPI_ADDRESS;
  status = dxp_write_tsar(ioChan, &saddr);
  if (status!=DXP_SUCCESS) {
    dxp_log_error("dxp_write_fippi","Error writing TSAR",status);
    return status;
  }

  /* Write the FIPPI data */

  f=DXP_FIPPI_F_WRITE;
  a=DXP_FIPPI_A_WRITE;
  status=x10p_md_io(ioChan,&f,&a,(void *)data,&len);     /* write TSAR */
  if (status!=DXP_SUCCESS) {
    status = DXP_WRITE_FIPPI;
    dxp_log_error("dxp_write_fippi","Error writing to FiPPi reg",status);
  }
  return status;
}

/******************************************************************************
 * Routine to enable the LAMs(Look-At-Me) on the specified DXP
 *
 * Enable the LAM for a single DXP module.
 *
 ******************************************************************************/
static int dxp_look_at_me(int* ioChan, int* modChan)
/* int *ioChan;      Input: I/O channel of DXP module */
/* int *modChan;     Input: DXP channels no (0,1,2,3)      */
{
  int *itemp;

  /* assign the input values to avoid compiler warnings */
  itemp = ioChan;
  itemp = modChan;

  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to enable the LAMs(Look-At-Me) on the specified DXP
 *
 * Disable the LAM for a single DXP module.
 *
 ******************************************************************************/
static int dxp_ignore_me(int* ioChan, int* modChan)
/* int *ioChan;      Input: I/O channel of DXP module */
/* int *modChan;     Input: DXP channels no (0,1,2,3)      */
{
  int *itemp;

  /* assign the input values to avoid compiler warnings */
  itemp = ioChan;
  itemp = modChan;

  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to clear the LAM(Look-At-Me) on the specified DXP
 *
 * Clear the LAM for a single DXP module.
 *
 ******************************************************************************/
static int dxp_clear_LAM(int* ioChan, int* modChan)
/* int *ioChan;      Input: I/O channel of DXP module */
/* int *modChan;     Input: DXP channels no (0,1,2,3)      */
{
  int *itemp;

  /* assign the input values to avoid compiler warnings */
  itemp = ioChan;
  itemp = modChan;

  return DXP_SUCCESS;
}

/********------******------*****------******-------******-------******------*****
 * Now begins the code devoted to routines that look like CAMAC transfers to
 * a preset address to the external world.  They actually write to the TSAR,
 * then the CSR, then finally write the data to the CAMAC bus.  The user need
 * never know
 *
 ********------******------*****------******-------******-------******------*****/

/******************************************************************************
 * Routine to read a single word of data from the DXP
 *
 * This routine makes all the neccessary calls to the TSAR and CSR to read
 * data from the appropriate channel and address of the DXP.
 *
 ******************************************************************************/
static int dxp_read_word(int* ioChan, int* modChan, unsigned short* addr,
                         unsigned short* readdata)
/* int *ioChan;      Input: I/O channel of DXP module      */
/* int *modChan;     Input: DXP channels no (0,1,2,3)      */
/* unsigned short *addr;   Input: Address within DSP memory      */
/* unsigned short *readdata;  Output: word read from memory         */
{
  /*
   *     Read a single word from a DSP address, for a single channel of a
   *                single DXP module.
   */
  int status;
  char info_string[INFO_LEN];

  if ((*modChan!=0)&&(*modChan!=ALLCHAN)) {
    sprintf(info_string,"X10P routine called with channel number %d",*modChan);
    status = DXP_BAD_PARAM;
    dxp_log_error("dxp_read_word",info_string,status);
  }

  /* write transfer start address register */

  status = dxp_write_tsar(ioChan, addr);
  if (status!=DXP_SUCCESS) {
    dxp_log_error("dxp_read_word","Error writing TSAR",status);
    return status;
  }

  /* now read the data */

  status = dxp_read_data(ioChan, readdata, 1);
  if (status!=DXP_SUCCESS) {
    status=DXP_READ_WORD;
    dxp_log_error("dxp_read_word","Error reading CAMDATA",status);
    return status;
  }
  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to write a single word of data to the DXP
 *
 * This routine makes all the neccessary calls to the TSAR and CSR to write
 * data to the appropriate channel and address of the DXP.
 *
 ******************************************************************************/
static int dxp_write_word(int* ioChan, int* modChan, unsigned short* addr,
                          unsigned short* writedata)
/* int *ioChan;      Input: I/O channel of DXP module      */
/* int *modChan;     Input: DXP channels no (-1,0,1,2,3)   */
/* unsigned short *addr;   Input: Address within X or Y mem.     */
/* unsigned short *writedata;  Input: word to write to memory        */
{
  /*
   *     Write a single word to a DSP address, for a single channel or all
   *            channels of a single DXP module
   */
  int status;
  char info_string[INFO_LEN];

  if ((*modChan!=0)&&(*modChan!=ALLCHAN)) {
    sprintf(info_string,"X10P routine called with channel number %d",*modChan);
    status = DXP_BAD_PARAM;
    dxp_log_error("dxp_write_word",info_string,status);
  }

  /* write transfer start address register */

  status = dxp_write_tsar(ioChan, addr);
  if (status!=DXP_SUCCESS) {
    dxp_log_error("dxp_write_word","Error writing TSAR",status);
    return status;
  }

  /* now write the data */

  status = dxp_write_data(ioChan, writedata, 1);
  if (status!=DXP_SUCCESS) {
    status=DXP_WRITE_WORD;
    dxp_log_error("dxp_write_word","Error writing CAMDATA",status);
    return status;
  }
  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to read a block of data from the DXP
 *
 * This routine makes all the neccessary calls to the TSAR and CSR to read
 * data from the appropriate channel and address of the DXP.
 *
 ******************************************************************************/
static int dxp_read_block(int* ioChan, int* modChan, unsigned short* addr,
                          unsigned int* length, unsigned short* readdata)
/* int *ioChan;      Input: I/O channel of DXP module       */
/* int *modChan;     Input: DXP channels no (0,1,2,3)       */
/* unsigned short *addr;   Input: start address within X or Y mem.*/
/* unsigned int *length;   Input: # of 16 bit words to transfer   */
/* unsigned short *readdata;  Output: words to read from memory      */
{
  /*
   *     Read a block of words from a single DSP address, or consecutive
   *          addresses for a single channel of a single DXP module
   */
  int status;
  char info_string[INFO_LEN];
  unsigned int i,nxfers,xlen;
  unsigned int maxblk;


  if ((*modChan!=0)&&(*modChan!=ALLCHAN)) {
    sprintf(info_string,"X10P routine called with channel number %d",*modChan);
    status = DXP_BAD_PARAM;
    dxp_log_error("dxp_read_block",info_string,status);
  }

  /* write transfer start address register */

  status = dxp_write_tsar(ioChan, addr);
  if (status!=DXP_SUCCESS) {
    dxp_log_error("dxp_read_block","Error writing TSAR",status);
    return status;
  }

  /* Retrieve MAXBLK and check if single transfer is needed */
  maxblk=x10p_md_get_maxblk();
  if (maxblk <= 0) maxblk = *length;

  /* prepare for the first pass thru loop */
  nxfers = ((*length-1)/maxblk) + 1;
  xlen = ((maxblk>=*length) ? *length : maxblk);
  i = 0;
  do {

    /* now read the data */

    status = dxp_read_data(ioChan, &readdata[maxblk*i], xlen);
    if (status!=DXP_SUCCESS) {
      status = DXP_READ_BLOCK;
      sprintf(info_string,"Error reading %dth block transer",i);
      dxp_log_error("dxp_read_block",info_string,status);
      return status;
    }
    /* Next loop */
    i++;
    /* On last pass thru loop transfer the remaining bytes */
    if (i==(nxfers-1)) xlen=((*length-1)%maxblk) + 1;
  } while (i<nxfers);

  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to write a single word of data to the DXP
 *
 * This routine makes all the neccessary calls to the TSAR and CSR to write
 * data to the appropriate channel and address of the DXP.
 *
 ******************************************************************************/
static int dxp_write_block(int* ioChan, int* modChan, unsigned short* addr,
                           unsigned int* length, unsigned short* writedata)
/* int *ioChan;      Input: I/O channel of DXP module     */
/* int *modChan;     Input: DXP channels no (-1,0,1,2,3)  */
/* unsigned short *addr;   Input: start address within X/Y mem  */
/* unsigned int *length;   Input: # of 16 bit words to transfer */
/* unsigned short *writedata;  Input: words to write to memory      */
{
  /*
   *    Write a block of words to a single DSP address, or consecutive
   *    addresses for a single channel or all channels of a single DXP module
   */
  int status;
  char info_string[INFO_LEN];
  unsigned int i,nxfers,xlen;
  unsigned int maxblk;

  if ((*modChan!=0)&&(*modChan!=ALLCHAN)) {
    sprintf(info_string,"X10P routine called with channel number %d",*modChan);
    status = DXP_BAD_PARAM;
    dxp_log_error("dxp_write_block",info_string,status);
  }

  /* write transfer start address register */

  status = dxp_write_tsar(ioChan, addr);
  if (status!=DXP_SUCCESS) {
    dxp_log_error("dxp_write_block","Error writing TSAR",status);
    return status;
  }

  /* Retrieve MAXBLK and check if single transfer is needed */
  maxblk=x10p_md_get_maxblk();
  if (maxblk <= 0) maxblk = *length;

  /* prepare for the first pass thru loop */
  nxfers = ((*length-1)/maxblk) + 1;
  xlen = ((maxblk>=*length) ? *length : maxblk);
  i = 0;
  do {

    /* now read the data */

    status = dxp_write_data(ioChan, &writedata[maxblk*i], xlen);
    if (status!=DXP_SUCCESS) {
      status = DXP_WRITE_BLOCK;
      sprintf(info_string,"Error in %dth block transfer",i);
      dxp_log_error("dxp_write_block",info_string,status);
      return status;
    }
    /* Next loop */
    i++;
    /* On last pass thru loop transfer the remaining bytes */
    if (i==(nxfers-1)) xlen=((*length-1)%maxblk) + 1;
  } while (i<nxfers);

  return status;
}

/********------******------*****------******-------******-------******------*****
 * Now begins the section with higher level routines.  Mostly to handle reading
 * in the DSP and FiPPi programs.  And handling starting up runs, ending runs,
 * runing calibration tasks.
 *
 ********------******------*****------******-------******-------******------*****/
/******************************************************************************
 * Routine to download the FiPPi configuration
 *
 * This routine downloads the FiPPi program of specifice decimation(number
 * of clocks to sum data over), CAMAC channel and DXP channel.  If -1 for the
 * DXP channel is specified then all channels are downloaded.
 *
 ******************************************************************************/
static int dxp_download_fpgaconfig(int* ioChan, int* modChan, char *name, Board* board)
/* int *ioChan;   Input: I/O channel of DXP module */
/* int *modChan;   Input: DXP channels no (-1,0,1,2,3) */
/* char *name;                     Input: Type of FPGA to download         */
/* Board *board;   Input: Board data   */
{
  /*
  *   Download the appropriate FiPPi configuration file to a single channel
  *   or all channels of a single DXP module.
  */
  int status;
  int n_attempts;
  int k;

  char info_string[INFO_LEN];

  unsigned short data;

  float xdone_wait      = .001f;

  unsigned int i,j,length,xlen,nxfers;
  float wait;
  unsigned int maxblk;
  Fippi_Info *fippi=NULL;
  int mod;

  /* Variables for the control tasks */
  short task;
  unsigned int ilen=1;
  int taskinfo[1];
  unsigned short value=7;
  float timeout;

  if (!((STREQ(name, "all")) || (STREQ(name, "fippi")))) {
    sprintf(info_string, "The DXPX10P does not have an FPGA called %s for channel number %d", name, *modChan);
    status = DXP_BAD_PARAM;
    dxp_log_error("dxp_download_fpgaconfig",info_string,status);
    return status;
  }

  if ((*modChan!=0)&&(*modChan!=ALLCHAN)) {
    sprintf(info_string,"X10P routine called with channel number %d",*modChan);
    status = DXP_BAD_PARAM;
    dxp_log_error("dxp_download_fpgaconfig",info_string,status);
    return status;
  }

  /* If allchan chosen, then select the first valid fippi */
  for (i=0;i<board->nchan;i++) {
    if (((board->used)&(0x1<<i))!=0) {
      fippi = board->fippi[i];
      break;
    }
  }
  mod = board->mod;
  /* make sure a valid Fippi was found */
  if (fippi==NULL) {
    sprintf(info_string,"There is no valid FiPPi defined for module %i",mod);
    status = DXP_NOFIPPI;
    dxp_log_error("dxp_download_fpgaconfig",info_string,status);
    return status;
  }

  /* If needed, put the DSP to sleep before downloading the FIPPI */
  if (board->chanstate==NULL) {
    sprintf(info_string,"Something wrong in initialization, no channel state information for module %i",mod);
    status = DXP_INITIALIZE;
    dxp_log_error("dxp_download_fpgaconfig",info_string,status);
    return status;
  }
  /* check the DSP download state, if downloaded, then sleep */
  if (board->chanstate[*modChan].dspdownloaded==1) {
    task = CT_DXPX10P_SLEEP_DSP;
    ilen = 1;
    taskinfo[0] = 1;
    if ((status=dxp_begin_control_task(ioChan, modChan, &task,
                                       &ilen, taskinfo, board))!=DXP_SUCCESS) {
      sprintf(info_string,"Error putting the DSP to sleep for module %i",mod);
      status = DXP_DSPSLEEP;
      dxp_log_error("dxp_download_fpgaconfig",info_string,status);
      return status;
    }
    /* Now that a run is started, update the board information as well */
    board->state[4] = task;
    board->state[0] = 1;

    /* Now wait for BUSY=7 to indicate the DSP is asleep */
    value = 7;
    timeout = 2.0;
    if ((status=dxp_download_dsp_done(ioChan, modChan, &mod, board,
                                      &value, &timeout))!=DXP_SUCCESS) {
      sprintf(info_string,"Error waiting for BUSY=7 state for module %i",mod);
      status = DXP_DSPTIMEOUT;
      dxp_log_error("dxp_download_fpgaconfig",info_string,status);
      return status;
    }
  }

  length = fippi->proglen;

  sprintf(info_string, "FiPPI length = %u", length);
  dxp_log_debug("dxp_download_fpgaconfig", info_string);

  status = dxp_read_csr(ioChan, &data);
  if (status!=DXP_SUCCESS) {
    dxp_log_error("dxp_download_fpgaconfig","Error reading the CSR",status);
    return status;
  }

  /* Write to CSR to initiate download */

  data |= MASK_FIPRESET;

  status = dxp_write_csr(ioChan, &data);
  if (status!=DXP_SUCCESS) {
    dxp_log_error("dxp_download_fpgaconfig","Error writing to the CSR",status);
    return status;
  }

  /* wait 50ms, for LCA to be ready for next data */

  wait = 0.050f;
  status = x10p_md_wait(&wait);

  /* single word transfers for first 10 words */
  for (i=0;i<10;i++) {
    status = dxp_write_fippi(ioChan, &(fippi->data[i]), 1);
    if (status!=DXP_SUCCESS) {
      status = DXP_WRITE_WORD;
      sprintf(info_string,"Error in %dth 1-word transfer",i);
      dxp_log_error("dxp_download_fpgaconfig",info_string,status);
      return status;
    }
  }

  /* Retrieve MAXBLK and check if single transfer is needed */
  maxblk=x10p_md_get_maxblk();
  if (maxblk <= 0) maxblk = length;

  sprintf(info_string, "maxblk = %u", maxblk);
  dxp_log_debug("dxp_download_fpgaconfig", info_string);

  /* prepare for the first pass thru loop */
  nxfers = ((length-11)/maxblk) + 1;

  sprintf(info_string, "nxfers = %u", nxfers);
  dxp_log_debug("dxp_download_fpgaconfig", info_string);

  xlen = ((maxblk>=(length-10)) ? (length-10) : maxblk);

  j = 0;
  do {

    sprintf(info_string, "xlen = %u", nxfers);
    dxp_log_debug("dxp_download_fpgaconfig", info_string);


    /* now read the data */

    status = dxp_write_fippi(ioChan, &(fippi->data[j*maxblk+10]), xlen);
    if (status!=DXP_SUCCESS) {
      status = DXP_WRITE_BLOCK;
      sprintf(info_string,"Error in %dth (last) block transfer",j);
      dxp_log_error("dxp_download_fpgaconfig",info_string,status);
      return status;
    }
    /* Next loop */
    j++;
    /* On last pass thru loop transfer the remaining bytes */
    if (j==(nxfers-1)) xlen=((length-11)%maxblk) + 1;
  } while (j<nxfers);

  /* Wait for XDONE to signal that the FiPPI configured. */
  n_attempts = (int)ROUND(XDONE_TIMEOUT / xdone_wait);

  for (k = 0; k < n_attempts; k++) {
    status = dxp_read_csr(ioChan, &data);

    if (status != DXP_SUCCESS) {
      sprintf(info_string, "Error reading CSR while checking XDONE for "
              "ioChan = %d", *ioChan);
      dxp_log_error("dxp_download_fpgaconfig", info_string, status);
      return status;
    }

    sprintf(info_string, "CSR = %#x", data);
    dxp_log_debug("dxp_download_fpgaconfig", info_string);

    if ((data & MASK_FIPERR) == 0) {
      break;
    }

    x10p_md_wait(&xdone_wait);
  }

  if (k == n_attempts) {
    sprintf(info_string, "Timeout waiting (%0.3f seconds) for XDONE to assert "
            "for ioChan = %d", XDONE_TIMEOUT, *ioChan);
    dxp_log_error("dxp_download_fpgaconfig", info_string, DXP_FPGA_TIMEOUT);
    return DXP_FPGA_TIMEOUT;
  }

  /* After FIPPI is downloaded, end the SLEEP mode */
  if (board->chanstate[*modChan].dspdownloaded==1) {
    if ((status=dxp_end_control_task(ioChan, modChan, board))!=DXP_SUCCESS) {
      sprintf(info_string,"Error putting the DSP to sleep for module %i",mod);
      status = DXP_DSPSLEEP;
      dxp_log_error("dxp_download_fpgaconfig",info_string,status);
      return status;
    }
    /* Now that the run is over, update the board information as well */
    board->state[4] = 0;
    board->state[0] = 0;

    /* Now wait for BUSY=0 to indicate the DSP is ready */
    value = 0;
    timeout = 2.0;
    if ((status=dxp_download_dsp_done(ioChan, modChan, &mod, board,
                                      &value, &timeout))!=DXP_SUCCESS) {
      sprintf(info_string,"Error waiting for BUSY=0 state for module %i",mod);
      status = DXP_DSPTIMEOUT;
      dxp_log_error("dxp_download_fpgaconfig",info_string,status);
      return status;
    }
  }

  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to read the FiPPi configuration file into memory
 *
 * This routine reads in the file filename and stores the FiPPi program in
 * the fipconfig global array at location determined by *dec_index.
 *
 ******************************************************************************/
static int dxp_get_fpgaconfig(Fippi_Info* fippi)
/* Fippi_Info *fippi;   I/O: structure of Fippi info */
{
  int status;
  char info_string[INFO_LEN];
  char line[LINE_LEN];
  unsigned int j, nchars, len;
  FILE *fp;

  unsigned short temp=0, lowbyte=1;

  sprintf(info_string,"%s%s%s","Reading FPGA file ",
          fippi->filename,"...");
  dxp_log_info("dxp_get_fpgaconfig",info_string);

  fippi->maxproglen = MAXFIP_LEN;

  if (fippi->data==NULL) {
    status = DXP_NOMEM;
    sprintf(info_string,"%s",
            "Error allocating space for configuration");
    dxp_log_error("dxp_get_fpgaconfig",info_string,status);
    return status;
  }
  /*
   *  Check to see if FiPPI configuration has already been read in: 0 words
   *  means it has not...
   */
  if ((fp = dxp_find_file(fippi->filename,"r"))==NULL) {
    status = DXP_OPEN_FILE;
    sprintf(info_string,"%s%s","Unable to open FPGA configuration ",
            fippi->filename);
    dxp_log_error("dxp_get_fpgaconfig",info_string,status);
    return status;
  }

  /* Stuff the data into the fipconfig array */

  lowbyte = 1;
  len = 0;
  while (x10p_md_fgets(line,132,fp)!=NULL) {
    if (line[0]=='*') continue;
    nchars = strlen(line)-1;
    while ((nchars>0) && !isxdigit(line[nchars])) {
      nchars--;
    }
    for (j=0;j<nchars;j=j+2) {
      sscanf(&line[j],"%2hX",&temp);
      /* Check if the next byte is the low or high byte of the configuration word */
      if (lowbyte==1) {
        len++;
        fippi->data[len-1] = temp;
        lowbyte = 0;
      } else {
        fippi->data[len-1] |= temp<<8;
        lowbyte = 1;
      }
    }
  }
  fippi->proglen = len;
  fclose(fp);
  dxp_log_info("dxp_get_fpgaconfig","...DONE!");

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Routine to check that all the FiPPis downloaded successfully to
 * a single module.  If the routine returns DXP_SUCCESS, then the
 * FiPPis are OK
 *
 ******************************************************************************/
static int dxp_download_fpga_done(int* modChan, char *name, Board *board)
/* int *modChan;   Input: Module channel number              */
/* char *name;                          Input: Type of FPGA to check the status of*/
/* board *board;   Input: Board structure for this device    */
{
  int status;
  char info_string[INFO_LEN];
  unsigned short data;

  unsigned short used;
  int ioChan;

  int idummy;

  /* dummy assignment to satisfy compiler */
  idummy = *modChan;

  /* Few assignements to make life easier */
  ioChan = board->ioChan;
  used = board->used;

  if (!((STREQ(name, "all")) || (STREQ(name, "fippi")))) {
    sprintf(info_string, "The DXPX10P does not have an FPGA called %s for channel number %d", name, board->mod);
    status = DXP_BAD_PARAM;
    dxp_log_error("dxp_download_fpga_done",info_string,status);
    return status;
  }

  /* Read back the CSR to determine if the download was successfull.  */

  if ((status=dxp_read_csr(&ioChan,&data))!=DXP_SUCCESS) {
    sprintf(info_string," failed to read CSR for module %d", board->mod);
    dxp_log_error("dxp_download_fpga_done",info_string,status);
    return status;
  }
  if (used == 0) return DXP_SUCCESS;  /* if not used, then we succeed */
  if ((data&0x0100)!=0) {
    sprintf(info_string,
            "FiPPI download error (CSR bits) for module %d",board->mod);
    status=DXP_FPGADOWNLOAD;
    dxp_log_error("dxp_download_fpga_done",info_string,status);
    return status;
  }

  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to download the DSP Program
 *
 * This routine downloads the DSP program to the specified CAMAC slot and
 * channel on the DXP.  If -1 for the DXP channel is specified then all
 * channels are downloaded.
 *
 ******************************************************************************/
static int dxp_download_dspconfig(int* ioChan, int* modChan, Board *board)
/* int *ioChan;     Input: I/O channel of DXP module  */
/* int *modChan;    Input: DXP channel no (-1,0,1,2,3) */
/* Dsp_Info *dsp;    Input: DSP structure     */
{
  /*
   *   Download the DSP configuration file to a single channel or all channels
   *                          of a single DXP module.
   *
   */

  int status;
  char info_string[INFO_LEN];
  unsigned short data;
  unsigned int length;
  unsigned short start_addr;
  unsigned int j,nxfers,xlen;
  unsigned int maxblk;

  unsigned int i;

  float timeout = 0.0;

  Dsp_Info *dsp = NULL;


  ASSERT(board != NULL);


  if ((*modChan!=0)&&(*modChan!=ALLCHAN)) {
    sprintf(info_string,"X10P called with channel number %d",*modChan);
    status = DXP_BAD_PARAM;
    dxp_log_error("dxp_download_dspconfig",info_string,status);
  }

  /* Since the Saturn hardware is single-channel, we are always referencing
   * module channel 0.
   */
  dsp = board->dsp[0];

  ASSERT(dsp != NULL);

  /* Read-Modify-Write to CSR to initiate download */
  status = dxp_read_csr(ioChan, &data);
  if (status!=DXP_SUCCESS) {
    dxp_log_error("dxp_download_dspconfig","Error reading the CSR",status);
    return status;
  }

  data |= MASK_DSPRESET;

  status = dxp_write_csr(ioChan, &data);
  if (status!=DXP_SUCCESS) {
    dxp_log_error("dxp_download_dspconfig","Error writing to the CSR",status);
    return status;
  }
  /*
   * Transfer the DSP program code.  Skip the first word and
   * return to it at the end.
   */
  length = dsp->proglen - 2;

  /* Retrieve MAXBLK and check if single transfer is needed */
  maxblk=x10p_md_get_maxblk();
  if (maxblk <= 0) maxblk = length;

  /* prepare for the first pass thru loop */
  nxfers = ((length-1)/maxblk) + 1;
  xlen = ((maxblk>=length) ? length : maxblk);
  j = 0;
  do {

    /* now write the data */
    start_addr = (unsigned short) (j*maxblk + 1);
    status = dxp_write_block(ioChan, modChan, &start_addr, &xlen, &(dsp->data[j*maxblk+2]));
    if (status!=DXP_SUCCESS) {
      status = DXP_WRITE_BLOCK;
      sprintf(info_string,"Error in  %dth block transfer",j);
      dxp_log_error("dxp_download_dspconfig",info_string,status);
      return status;
    }
    /* Next loop */
    j++;
    /* On last pass thru loop transfer the remaining bytes */
    if (j==(nxfers-1)) xlen=((length-1)%maxblk) + 1;
  } while (j<nxfers);

  /* Now write the first word of the DSP program to start the program running. */

  xlen = 2;
  start_addr = 0;

  status = dxp_write_block(ioChan, modChan, &start_addr, &xlen, &(dsp->data[0]));

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error writing boot block to ioChan = %d", *ioChan);
    dxp_log_error("dxp_download_dspconfig", info_string, status);
    return status;
  }

  /* Verify that the DSP has booted on all of the channels. */

  timeout = 0.001f;
  x10p_md_wait(&timeout);

  for (i = 0; i < board->nchan; i++) {
    status = dxp_wait_for_busy(ioChan, (int *)&i, board, 50, 0.0, 0.5);

    if (status != DXP_SUCCESS) {
      sprintf(info_string, "Error waiting for DSP to boot ioChan = %d", *ioChan);
      dxp_log_error("dxp_download_dspconfig", info_string, status);
      return status;
    }
  }

  board->chanstate[0].dspdownloaded = 1;

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Routine to check that the DSP is done with its initialization.
 * If the routine returns DXP_SUCCESS, then the DSP is ready to run.
 *
 ******************************************************************************/
static int dxp_download_dsp_done(int* ioChan, int* modChan, int* mod,
                                 Board *board, unsigned short* value,
                                 float* timeout)
/* int *ioChan;      Input: I/O channel of the module  */
/* int *modChan;     Input: Module channel number   */
/* int *mod;      Input: Module number, for errors  */
/* Dsp_Info *dsp;     Input: Relavent DSP info    */
/* unsigned short *value;   Input: Value to match for BUSY  */
/* float *timeout;     Input: How long to wait, in seconds */
{
  int status, i;
  char info_string[INFO_LEN];
  /* The value of the BUSY parameter */
  double data;
  /* Number of times to divide up the timeout period */
  float divisions = 10.;
  /* Time to wait between checking BUSY */
  float wait;

  Dsp_Info *dsp = NULL;


  dsp = board->dsp[*modChan];
  ASSERT(dsp != NULL);

  wait = *timeout/divisions;

  for (i = 0;i < divisions;i++) {
    /* readout the value of the BUSY parameter */
    status = dxp_read_dspsymbol(ioChan, modChan, "BUSY", board, &data);

    sprintf(info_string, "BUSY = %0.1f", data);
    dxp_log_debug("dxp_download_dsp_done", info_string);

    if (status != DXP_SUCCESS) {
      sprintf(info_string,"Error reading BUSY from module %d channel %d", *mod, *modChan);
      dxp_log_error("dxp_download_dsp_done",info_string,status);
      return status;
    }
    /* Check if the BUSY parameter matches the requested value
     * If match then return successful. */
    if (((unsigned short) data)==(*value)) return DXP_SUCCESS;
    /* Wait for another period and try again. */
    x10p_md_wait(&wait);
  }

  /* If here, then timeout period reached.  Report error. */
  status = DXP_DSPTIMEOUT;
  sprintf(info_string,"Timeout waiting for DSP BUSY=%d from module %d channel %d",
          *value, *mod, *modChan);
  dxp_log_error("dxp_download_dsp_done",info_string,status);
  return status;
}

/******************************************************************************
 *
 * Routine to retrieve the FIPPI program maximum sizes so that memory
 * can be allocated.
 *
 ******************************************************************************/
static int dxp_get_fipinfo(Fippi_Info* fippi)
/* Fippi_Info *fippi;    I/O: Structure of FIPPI program Info */
{

  fippi->maxproglen = MAXFIP_LEN;

  return DXP_SUCCESS;

}

/******************************************************************************
 *
 * Routine to retrieve the DSP defaults parameters maximum sizes so that memory
 * can be allocated.
 *
 ******************************************************************************/
static int dxp_get_defaultsinfo(Dsp_Defaults* defaults)
/* Dsp_Defaults *defaults;    I/O: Structure of FIPPI program Info */
{

  defaults->params->maxsym = MAXSYM;
  defaults->params->maxsymlen = MAXSYMBOL_LEN;

  return DXP_SUCCESS;

}

/******************************************************************************
 *
 * Routine to retrieve the DSP program maximum sizes so that memory
 * can be allocated.
 *
 ******************************************************************************/
static int dxp_get_dspinfo(Dsp_Info* dsp)
/* Dsp_Info *dsp;       I/O: Structure of DSP program Info */
{

  dsp->params->maxsym     = MAXSYM;
  dsp->params->maxsymlen  = MAXSYMBOL_LEN;
  dsp->maxproglen = MAXDSP_LEN;

  return DXP_SUCCESS;

}

/******************************************************************************
 * Routine to retrieve the DSP program and symbol table
 *
 * Read the DSP configuration file  -- logical name (or symbolic link)
 * DSP_CONFIG must be defined prior to execution of the program. At present,
 * it is NOT possible to use different configurations for different DXP
 * channels.
 *
 ******************************************************************************/
static int dxp_get_dspconfig(Dsp_Info* dsp)
/* Dsp_Info *dsp;     I/O: Structure of DSP program Info */
{
  char info_string[INFO_LEN];
  FILE  *fp;
  int   status;

  sprintf(info_string, "Loading DSP program in %s", dsp->filename);
  dxp_log_info("dxp_get_dspconfig",info_string);

  /* Now retrieve the file pointer to the DSP program */

  if ((fp = dxp_find_file(dsp->filename,"r"))==NULL) {
    status = DXP_OPEN_FILE;
    sprintf(info_string, "Unable to open %s", dsp->filename);
    dxp_log_error("dxp_get_dspconfig",info_string,status);
    return status;
  }

  /* Fill in some general information about the DSP program  */
  dsp->params->maxsym  = MAXSYM;
  dsp->params->maxsymlen = MAXSYMBOL_LEN;
  dsp->maxproglen   = MAXDSP_LEN;

  /* Load the symbol table and configuration */

  if ((status = dxp_load_dspfile(fp, dsp))!=DXP_SUCCESS) {
    status = DXP_DSPLOAD;
    fclose(fp);
    dxp_log_error("dxp_get_dspconfig","Unable to Load DSP file",status);
    return status;
  }

  /* Close the file and get out */

  fclose(fp);
  dxp_log_info("dxp_get_dspconfig","...DONE!");

  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to retrieve the default DSP parameter values
 *
 * Read the file pointed to by filename and read in the
 * symbol value pairs into the array.  No writing of data
 * here.
 *
 ******************************************************************************/
static int dxp_get_dspdefaults(Dsp_Defaults* defaults)
/* Dsp_Defaults *defaults;     I/O: Structure of DSP defaults    */
{
  char info_string[INFO_LEN];
  char line[LINE_LEN];
  FILE  *fp;
  int   status, i, last;
  char *fstatus=" ";
  unsigned short nsymbols;
  char *token,*delim=" ,:=\r\n\t";
  char strtmp[4];

  sprintf(info_string,"Loading DSP defaults in %s",defaults->filename);
  dxp_log_info("dxp_get_dspdefaults",info_string);

  /* Check the filename: if NULL, then there are no parameters */

  for (i=0;i<4;i++) strtmp[i] = (char) toupper(defaults->filename[i]);
  if (strncmp(strtmp,"NULL",4)==0) {
    status = DXP_SUCCESS;
    defaults->params->nsymbol=0;
    return status;
  }


  /* Now retrieve the file pointer to the DSP defaults file */

  if ((fp = dxp_find_file(defaults->filename,"r"))==NULL) {
    status = DXP_OPEN_FILE;
    sprintf(info_string,"Unable to open %s",defaults->filename);
    dxp_log_error("dxp_get_dspdefaults",info_string,status);
    return status;
  }

  /* Initialize the number of default values */
  nsymbols = 0;

  while (fstatus!=NULL) {
    do
      fstatus = x10p_md_fgets(line,132,fp);
    while ((line[0]=='*') && (fstatus!=NULL));

    /* Check if we are downloading more parameters than allowed. */

    if (nsymbols==MAXSYM) {
      defaults->params->nsymbol = nsymbols;
      status = DXP_ARRAY_TOO_SMALL;
      sprintf(info_string,"Too many parameters in %s",defaults->filename);
      dxp_log_error("dxp_get_dspdefaults",info_string,status);
      return status;
    }
    if ((fstatus==NULL)) continue;     /* End of file?  or END finishes */
    if (strncmp(line,"END",3)==0) break;

    /* Parse the line for the symbol name, value pairs and store the pairs in the defaults
     * structure for later use */

    token=strtok(line,delim);
    /* Got the symbol name, copy the value */
    defaults->params->parameters[nsymbols].pname =
      strcpy(defaults->params->parameters[nsymbols].pname, token);
    /* Now get the value and store it in the data array */
    token = strtok(NULL, delim);
    /* Check if the number is entered as a hex entry? */
    last = strlen(token)-1;
    if ((strncmp(token+last,"h",1)==0)||(strncmp(token+last,"H",1)==0)) {
      token[last] = '\0';
      defaults->data[nsymbols] = (unsigned short) strtoul(token,NULL,16);
    } else {
      defaults->data[nsymbols] = (unsigned short) strtoul(token,NULL,0);
    }

    /* Increment nsymbols and go get the next pair */
    nsymbols++;
  }

  /* assign the number of symbols to the defaults structure */
  defaults->params->nsymbol = nsymbols;

  /* Close the file and get out */

  fclose(fp);
  dxp_log_info("dxp_get_dspdefaults","...DONE!");
  return DXP_SUCCESS;

}

/******************************************************************************
 * Routine to retrieve the DSP program and symbol table
 *
 * Read the DSP configuration file  -- passed filepointer
 *
 ******************************************************************************/
static int dxp_load_dspfile(FILE* fp, Dsp_Info* dsp)
/* FILE  *fp;       Input: Pointer to the opened DSP file*/
/* unsigned short *dspconfig;   Output: Array containing DSP program */
/* unsigned int *nwordsdsp;    Output: Size of DSP program   */
/* char **dspparam;      Output: Array of DSP param names  */
/* unsigned short *nsymbol;    Output: Number of defined DSP symbols*/
{
  int status;

  /* Load the symbol table */

  if ((status = dxp_load_dspsymbol_table(fp, dsp))!=DXP_SUCCESS) {
    status = DXP_DSPLOAD;
    dxp_log_error("dxp_load_dspfile","Unable to read DSP symbol table",status);
    return status;
  }


  /* Load the configuration */

  if ((status = dxp_load_dspconfig(fp, dsp))!=DXP_SUCCESS) {
    status = DXP_DSPLOAD;
    dxp_log_error("dxp_load_dspfile","Unable to read DSP configuration",status);
    return status;
  }

  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to read in the DSP program
 *
 ******************************************************************************/
static int dxp_load_dspconfig(FILE* fp, Dsp_Info* dsp)
/* FILE *fp;       Input: File pointer from which to read the symbols */
/* unsigned short *dspconfig;   Output: Array containing DSP program    */
/* unsigned int *nwordsdsp;    Output: Size of DSP program       */
{

  int i,status;
  char line[LINE_LEN];
  int nchars;

  unsigned int dsp0, dsp1;

  /* Check if we have some allocated memory space */
  if (dsp->data==NULL) {
    status = DXP_NOMEM;
    dxp_log_error("dxp_load_dspconfig",
                  "Error allocating space for configuration",status);
    return status;
  }
  /*
   *  and read the configuration
   */
  dsp->proglen = 0;
  while (x10p_md_fgets(line,132,fp)!=NULL) {
    nchars = strlen(line);
    while ((nchars>0) && !isxdigit(line[nchars])) {
      nchars--;
    }
    for (i=0;i<nchars;i+=6) {
      sscanf(&line[i],"%4X%2X",&dsp0,&dsp1);
      dsp->data[(dsp->proglen)++] = (unsigned short) dsp0;
      dsp->data[(dsp->proglen)++] = (unsigned short) dsp1;
    }
  }

  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to read in the DSP symbol name list
 *
 ******************************************************************************/
static int dxp_load_dspsymbol_table(FILE* fp, Dsp_Info* dsp)
/* FILE *fp;      Input: File pointer from which to read the symbols */
/* char **pnames;     Output: Array of DSP param names     */
/* unsigned short *nsymbol;   Output: Number of defined DSP symbols    */
{
  int status, retval;
  char info_string[INFO_LEN];
  char line[LINE_LEN];
  unsigned short i;
  /* Hold the character representing the access type *=R/W -=RO */
  char atype[2];

  /*
   *  Read comments and number of symbols
   */
  while (x10p_md_fgets(line,132,fp)!=NULL) {
    if (line[0]=='*') continue;
    sscanf(line,"%hu",&(dsp->params->nsymbol));
    break;
  }
  if (dsp->params->nsymbol>0) {
    /*
     *  Allocate space and read symbols
     */
    if (dsp->params->parameters==NULL) {
      status = DXP_NOMEM;
      dxp_log_error("dxp_load_dspsymbol_table",
                    "Memory not allocated for all parameter names",status);
      return status;
    }
    for (i=0;i<(dsp->params->nsymbol);i++) {
      if (dsp->params->parameters[i].pname==NULL) {
        status = DXP_NOMEM;
        dxp_log_error("dxp_load_dspsymbol_table",
                      "Memory not allocated for single parameter name",status);
        return status;
      }
      if (x10p_md_fgets(line, 132, fp)==NULL) {
        status = DXP_BAD_PARAM;
        dxp_log_error("dxp_load_dspsymbol_table",
                      "Error in SYMBOL format of DSP file",status);
        return status;
      }
      retval = sscanf(line, "%s %1s %hu %hu", dsp->params->parameters[i].pname, atype,
                      &(dsp->params->parameters[i].lbound), &(dsp->params->parameters[i].ubound));
      dsp->params->parameters[i].address = i;
      dsp->params->parameters[i].access = 1;
      if (retval>1) {
        if (strcmp(atype,"-")==0) dsp->params->parameters[i].access = 0;
      }
      if (retval==2) {
        dsp->params->parameters[i].lbound = 0;
        dsp->params->parameters[i].ubound = 0;
      }
      if (retval==3) {
        status = DXP_BAD_PARAM;
        sprintf(info_string, "Error in SYMBOL(%s) format of DSP file: 3 parameters found",
                dsp->params->parameters[i].pname);
        dxp_log_error("dxp_load_dspsymbol_table", info_string, status);
        return status;
      }
    }
  }

  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to return the symbol name at location index in the symbol table
 * of the DSP
 *
 ******************************************************************************/
static int dxp_symbolname(unsigned short* lindex, Dsp_Info* dsp, char string[])
/* unsigned short *lindex;    Input: address of parameter   */
/* Dsp_Info *dsp;      Input: dsp structure with info  */
/* char string[];      Output: parameter name    */
{

  int status;

  /* There better be an array call symbolname! */

  if (*lindex>=(dsp->params->nsymbol)) {
    status = DXP_INDEXOOB;
    dxp_log_error("dxp_symbolname","Index greater than the number of symbols",status);
    return status;
  }
  strcpy(string,(dsp->params->parameters[*lindex].pname));

  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to locate a symbol in the DSP symbol table.
 *
 * This routine returns the address of the symbol called name in the DSP
 * symbol table.
 *
 ******************************************************************************/
static int dxp_loc(char name[], Dsp_Info* dsp, unsigned short* address)
/* char name[];     Input: symbol name for accessing YMEM params */
/* Dsp_Info *dsp;    Input: dsp structure with info    */
/* unsigned short *address;  Output: address in YMEM                      */
{
  /*
   *       Find pointer into parameter memmory using symbolic name
   */
  int status;
  char info_string[INFO_LEN];
  unsigned short i;

  if ((dsp->proglen)<=0) {
    status = DXP_DSPLOAD;
    sprintf(info_string, "Must Load DSP code before searching for %s", name);
    dxp_log_error("dxp_loc", info_string, status);
    return status;
  }

  *address = USHRT_MAX;
  for (i=0;i<(dsp->params->nsymbol);i++) {
    if (  (strlen(name)==strlen((dsp->params->parameters[i].pname))) &&
          (strstr(name,(dsp->params->parameters[i].pname))!=NULL)  ) {
      *address = i;
      break;
    }
  }

  /* Did we find the Symbol in the table? */

  status = DXP_SUCCESS;
  if (*address == USHRT_MAX) {
    status = DXP_NOSYMBOL;
    /*  sprintf(info_string, "Cannot find <%s> in symbol table",name);
      dxp_log_error("dxp_loc",info_string,status);
    */
  }

  return status;
}

/******************************************************************************
 * Routine to dump the memory contents of the DSP.
 *
 * This routine prints the memory contents of the DSP in channel modChan
 * of the DXP of ioChan.
 *
 ******************************************************************************/
static int dxp_dspparam_dump(int* ioChan, int* modChan, Dsp_Info* dsp)
/* int *ioChan;      Input: I/O channel of DXP module        */
/* int *modChan;     Input: DXP channels no (0,1,2,3)        */
/* Dsp_Info *dsp;     Input: dsp structure with info  */
{
  /*
   *   Read the parameter memory from a single channel on a signle DXP module
   *              and display it along with their symbolic names
   */
  unsigned short *data;
  int status;
  int ncol, nleft;
  int i;
  char buf[128];
  int col[4]={0,0,0,0};
  unsigned int nsymbol;

  nsymbol = (unsigned int) dsp->params->nsymbol;
  /* Allocate memory to read out parameters */
  data = (unsigned short *) x10p_md_alloc(nsymbol*sizeof(unsigned short));

  if ((status=dxp_read_block(ioChan,
                             modChan,
                             &startp,
                             &nsymbol,
                             data))!=DXP_SUCCESS) {
    dxp_log_error("dxp_mem_dump"," ",status);
    x10p_md_free(data);
    return status;
  }
  x10p_md_puts("\r\nDSP Parameters Memory Dump:\r\n");
  x10p_md_puts(" Parameter  Value    Parameter  Value  ");
  x10p_md_puts("Parameter  Value    Parameter  Value\r\n");
  x10p_md_puts("_________________________________________");
  x10p_md_puts("____________________________________\r\n");

  ncol = (int) (((float) nsymbol) / ((float) 4.));
  nleft = nsymbol%4;
  if (nleft==1) {
    col[0] = 0;
    col[1] = 1;
    col[2] = 1;
    col[3] = 1;
  }
  if (nleft==2) {
    col[0] = 0;
    col[1] = 1;
    col[2] = 2;
    col[3] = 2;
  }
  if (nleft==3) {
    col[0] = 0;
    col[1] = 1;
    col[2] = 2;
    col[3] = 3;
  }
  for (i=0;i<ncol;i++) {
    sprintf(buf,
            "%11s x%4.4x | %11s x%4.4x | %11s x%4.4x | %11s x%4.4x\r\n",
            dsp->params->parameters[i].pname, data[i+col[0]],
            dsp->params->parameters[i+ncol+col[1]].pname, data[i+ncol+col[1]],
            dsp->params->parameters[i+2*ncol+col[2]].pname, data[i+2*ncol+col[2]],
            dsp->params->parameters[i+3*ncol+col[3]].pname, data[i+3*ncol+col[3]]);
    x10p_md_puts(buf);
  }

  /* Add any strays that are left over at the bottom! */

  if (nleft>0) {
    sprintf(buf, "%11s x%4.4x | ",
            dsp->params->parameters[ncol+col[0]].pname, data[ncol+col[0]]);
    for (i=1;i<nleft;i++) {
      sprintf(buf,
              "%s%11s x%4.4x | ", buf,
              dsp->params->parameters[i*ncol+ncol+col[i]].pname, data[i*ncol+ncol+col[i]]);
    }
    sprintf(buf, "%s \r\n", buf);
    x10p_md_puts(buf);
  }

  /* Throw in a final line skip, if there was a stray symbol or 2 on the end of the
   * table, else it was already added by the x10p_md_puts() above */

  if (nleft!=0) x10p_md_puts("\r\n");

  /* Free the allocated memory */
  x10p_md_free(data);

  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to test the DSP spetrum memory
 *
 * Test the DSP memory for a single channel of a single DXP module by writing
 * a string of words to reserved spectrum memory space, reading them back and
 * performing a compare.  The input pattern is defined using
 *
 * see dxp_test_mem for the meaning of *pattern..
 *
 ******************************************************************************/
static int dxp_test_spectrum_memory(int* ioChan, int* modChan, int* pattern,
                                    Board *board)
/* int *ioChan;     Input: IO channel to test    */
/* int *modChan;    Input: Channel on the module to test */
/* int *pattern;    Input: Pattern to use during testing */
/* Board *board;    Input: Relavent Board info    */
{
  int status;
  char info_string[INFO_LEN];
  unsigned short start, addr;
  unsigned int len;

  if ((status=dxp_loc("SPECTSTART", board->dsp[*modChan], &addr))!=DXP_SUCCESS) {
    dxp_log_error("dxp_test_spectrum_memory",
                  "Unable to find SPECTSTART symbol",status);
  }
  start = (unsigned short) (board->params[*modChan][addr] + PROGRAM_BASE);
  if ((status=dxp_loc("SPECTLEN", board->dsp[*modChan], &addr))!=DXP_SUCCESS) {
    dxp_log_error("dxp_test_spectrum_memory",
                  "Unable to find SPECTLEN symbol",status);
  }
  len = (unsigned int) board->params[*modChan][addr];

  if ((status = dxp_test_mem(ioChan, modChan, pattern,
                             &len, &start))!=DXP_SUCCESS) {
    sprintf(info_string,
            "Error testing spectrum memory for IO channel %d, channel %d",*ioChan, *modChan);
    dxp_log_error("dxp_test_spectrum_memory",info_string,status);
    return status;
  }
  return status;
}

/******************************************************************************
 * Routine to test the DSP baseline memory
 *
 * Test the DSP memory for a single channel of a single DXP module by writing
 * a string of words to reserved baseline memory space, reading them back and
 * performing a compare.  The input pattern is defined using
 *
 * see dxp_test_mem for the meaning of *pattern..
 *
 ******************************************************************************/
static int dxp_test_baseline_memory(int* ioChan, int* modChan, int* pattern,
                                    Board *board)
/* int *ioChan;      Input: IO channel to test    */
/* int *modChan;     Input: Channel on the module to test */
/* int *pattern;     Input: Pattern to use during testing */
/* Board *board;     Input: Relavent Board info     */
{
  int status;
  char info_string[INFO_LEN];
  unsigned short start, addr;
  unsigned int len;

  if ((status=dxp_loc("BASESTART", board->dsp[*modChan], &addr))!=DXP_SUCCESS) {
    dxp_log_error("dxp_test_baseline_memory",
                  "Unable to find BASESTART symbol",status);
  }
  start = (unsigned short) (board->params[*modChan][addr] + DATA_BASE);
  if ((status=dxp_loc("BASELEN", board->dsp[*modChan], &addr))!=DXP_SUCCESS) {
    dxp_log_error("dxp_test_baseline_memory",
                  "Unable to find BASELEN symbol",status);
  }
  len = (unsigned int) board->params[*modChan][addr];

  if ((status = dxp_test_mem(ioChan, modChan, pattern,
                             &len, &start))!=DXP_SUCCESS) {
    sprintf(info_string,
            "Error testing baseline memory for IO channel %d, channel %d",*ioChan, *modChan);
    dxp_log_error("dxp_test_baseline_memory",info_string,status);
    return status;
  }

  return status;
}

/******************************************************************************
 * Routine to test the DSP event buffermemory
 *
 * Test the DSP memory for a single channel of a single DXP module by writing
 * a string of words to reserved event buffer memory space, reading them back and
 * performing a compare.  The input pattern is defined using
 *
 * see dxp_test_mem for the meaning of *pattern..
 *
 ******************************************************************************/
static int dxp_test_event_memory(int* ioChan, int* modChan, int* pattern,
                                 Board *board)
/* int *ioChan;      Input: IO channel to test    */
/* int *modChan;     Input: Channel on the module to test */
/* int *pattern;     Input: Pattern to use during testing */
/* Board *board;     Input: Relavent Board info     */
{
  int status;
  char info_string[INFO_LEN];
  unsigned short start, addr;
  unsigned int len;

  /* Now read the Event Buffer base address and length and store in
   * static library variables for future use. */

  if ((status=dxp_loc("EVTBSTART", board->dsp[*modChan], &addr))!=DXP_SUCCESS) {
    dxp_log_error("dxp_test_event_memory",
                  "Unable to find EVTBSTART symbol",status);
  }
  start = (unsigned short) (board->params[*modChan][addr] + DATA_BASE);
  if ((status=dxp_loc("EVTBLEN", board->dsp[*modChan], &addr))!=DXP_SUCCESS) {
    dxp_log_error("dxp_test_event_memory",
                  "Unable to find EVTBLEN symbol",status);
  }
  len = (unsigned int) board->params[*modChan][addr];

  if ((status = dxp_test_mem(ioChan, modChan, pattern,
                             &len, &start))!=DXP_SUCCESS) {
    sprintf(info_string,
            "Error testing baseline memory for IO channel %d, channel %d",*ioChan, *modChan);
    dxp_log_error("dxp_test_event_memory",info_string,status);
    return status;
  }

  return status;
}

/******************************************************************************
 * Routine to test the DSP memory
 *
 * Test the DSP memory for a single channel of a single DXP module by writing
 * a string of words to X or Y memmory, reading them back and performing a
 * compare.  The input string is defined using
 *
 *  *pattern=0 for 0,1,2....
 *  *pattern=1 for 0xFFFF,0xFFFF,0xFFFF,...
 *  *pattern=2 for 0xAAAA,0x5555,0xAAAA,0x5555...
 *
 ******************************************************************************/
static int dxp_test_mem(int* ioChan, int* modChan, int* pattern,
                        unsigned int* length, unsigned short* addr)
/* int *ioChan;      Input: I/O channel of DXP module        */
/* int *modChan;     Input: DXP channels no (0,1,2,3)        */
/* int *pattern;     Input: pattern to write to memmory      */
/* unsigned int *length;   Input: number of 16 bit words to xfer   */
/* unsigned short *addr;   Input: start address for transfer       */
{
  int status, nerrors;
  char info_string[INFO_LEN];
  unsigned int j;
  unsigned short *readbuf,*writebuf;

  /* Allocate the memory for the buffers */
  if (*length>0) {
    readbuf = (unsigned short *) x10p_md_alloc(*length*sizeof(unsigned short));
    writebuf = (unsigned short *) x10p_md_alloc(*length*sizeof(unsigned short));
  } else {
    status=DXP_BAD_PARAM;
    sprintf(info_string,
            "Attempting to test %d elements in DSP memory",*length);
    dxp_log_error("dxp_test_mem",info_string,status);
    return status;
  }

  /* First 256 words of Data memory are reserved for DSP parameters. */
  if ((*addr>0x4000)&&(*addr<0x4100)) {
    status=DXP_BAD_PARAM;
    sprintf(info_string,
            "Attempting to overwrite Parameter values beginning at address %x",*addr);
    dxp_log_error("dxp_test_mem",info_string,status);
    x10p_md_free(readbuf);
    x10p_md_free(writebuf);
    return status;
  }
  switch (*pattern) {
  case 0 :
    for (j=0;j<(*length)/2;j++) {
      writebuf[2*j]   = (unsigned short) (j&0x00FFFF00);
      writebuf[2*j+1] = (unsigned short) (j&0x000000FF);
    }
    break;
  case 1 :
    for (j=0;j<*length;j++) writebuf[j]= 0xFFFF;
    break;
  case 2 :
    for (j=0;j<*length;j++) writebuf[j] = (unsigned short) (((j&2)==0) ? 0xAAAA : 0x5555);
    break;
  default :
    status = DXP_BAD_PARAM;
    sprintf(info_string,"Pattern %d not implemented",*pattern);
    dxp_log_error("dxp_test_mem",info_string,status);
    x10p_md_free(readbuf);
    x10p_md_free(writebuf);
    return status;
  }

  /* Write the Pattern to the block of memory */

  if ((status=dxp_write_block(ioChan,
                              modChan,
                              addr,
                              length,
                              writebuf))!=DXP_SUCCESS) {
    dxp_log_error("dxp_test_mem"," ",status);
    x10p_md_free(readbuf);
    x10p_md_free(writebuf);
    return status;
  }

  /* Read the memory back and compare */

  if ((status=dxp_read_block(ioChan,
                             modChan,
                             addr,
                             length,
                             readbuf))!=DXP_SUCCESS) {
    dxp_log_error("dxp_test_mem"," ",status);
    x10p_md_free(readbuf);
    x10p_md_free(writebuf);
    return status;
  }
  for (j=0,nerrors=0; j<*length;j++) if (readbuf[j]!=writebuf[j]) {
      nerrors++;
      status = DXP_MEMERROR;
      if (nerrors<10) {
        sprintf(info_string, "Error: word %d, wrote %x, read back %x",
                j,((unsigned int) writebuf[j]),((unsigned int) readbuf[j]));
        dxp_log_error("dxp_test_mem",info_string,status);
      }
    }

  if (nerrors!=0) {
    sprintf(info_string, "%d memory compare errors found",nerrors);
    dxp_log_error("dxp_test_mem",info_string,status);
    x10p_md_free(readbuf);
    x10p_md_free(writebuf);
    return status;
  }

  x10p_md_free(readbuf);
  x10p_md_free(writebuf);
  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Set a parameter of the DSP.  Pass the symbol name, value to set and module
 * pointer and channel number.
 *
 ******************************************************************************/
static int dxp_modify_dspsymbol(int* ioChan, int* modChan, char* name,
                                unsigned short* value, Board* board)
/* int *ioChan;     Input: IO channel to write to    */
/* int *modChan;    Input: Module channel number to write to */
/* char *name;     Input: user passed name of symbol   */
/* unsigned short *value;  Input: Value to set the symbol to   */
/* Dsp_Info *dsp;    Input: Relavent DSP info     */
{

  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];
  unsigned int i;
  unsigned short addr;  /* address of the symbol in DSP memory   */
  char uname[20]="";   /* Upper case version of the user supplied name */
  Dsp_Info *dsp = NULL;


  dsp = board->dsp[*modChan];
  ASSERT(dsp != NULL);


  /* Change uname to upper case */

  if (strlen(name)>dsp->params->maxsymlen) {
    status = DXP_NOSYMBOL;
    sprintf(info_string, "Symbol name must be <%d characters", dsp->params->maxsymlen);
    dxp_log_error("dxp_modify_dspsymbol", info_string, status);
    return status;
  }
  for (i = 0; i < strlen(name); i++)
    uname[i] = (char) toupper(name[i]);

  /* Be paranoid and check if the DSP configuration is downloaded.  If not, do it */

  if ((dsp->proglen)<=0) {
    status = DXP_DSPLOAD;
    sprintf(info_string, "Must Load DSP code before modifying %s", name);
    dxp_log_error("dxp_modify_dspsymbol", info_string, status);
    return status;
  }

  /* First find the location of the symbol in DSP memory. */

  if ((status=dxp_loc(uname, dsp, &addr))!=DXP_SUCCESS) {
    sprintf(info_string, "Failed to find symbol %s in DSP memory", uname);
    dxp_log_error("dxp_modify_dspsymbol", info_string, status);
    return status;
  }

  /* Check the access type for this parameter.  Only allow writing to r/w and wo
   * parameters.
   */

  if (dsp->params->parameters[addr].access==0) {
    sprintf(info_string, "Parameter %s is Read-Only.  No writing allowed.", uname);
    status = DXP_DSPACCESS;
    dxp_log_error("dxp_modify_dspsymbol", info_string, status);
    return status;
  }

  /* Check the bounds, set to min or max if out of bounds. */

  /* Check if there are any bounds defined first */
  if ((dsp->params->parameters[addr].lbound!=0)||(dsp->params->parameters[addr].ubound!=0)) {
    if (*value < dsp->params->parameters[addr].lbound) {
      sprintf(info_string,
              "Value is below the lower acceptable bound %u < %u. Changing to lower bound.",
              *value, dsp->params->parameters[addr].lbound);
      status = DXP_DSPPARAMBOUNDS;
      dxp_log_error("dxp_modify_dspsymbol", info_string, status);
      *value = dsp->params->parameters[addr].lbound;
    }

    if (*value>dsp->params->parameters[addr].ubound) {
      sprintf(info_string,
              "Value is above the upper acceptable bound %u < %u. Changing to upper bound.",
              *value, dsp->params->parameters[addr].ubound);
      status = DXP_DSPPARAMBOUNDS;
      dxp_log_error("dxp_modify_dspsymbol", info_string, status);
      /* Set to the upper bound */
      *value = dsp->params->parameters[addr].ubound;
    }
  }

  /* Write the value of the symbol into DSP memory */

  if ((status=dxp_write_dsp_param_addr(ioChan,modChan,&(dsp->params->parameters[addr].address),
                                       value))!=DXP_SUCCESS) {
    sprintf(info_string, "Error writing parameter %s", uname);
    dxp_log_error("dxp_modify_dspsymbol",info_string,status);
    return status;
  }

  return status;
}

/******************************************************************************
 *
 * Write a single parameter to the DSP.  Pass the symbol address, value, module
 * pointer and channel number.
 *
 ******************************************************************************/
static int dxp_write_dsp_param_addr(int* ioChan, int* modChan,
                                    unsigned int* addr, unsigned short* value)
/* int *ioChan;      Input: IO channel to write to    */
/* int *modChan;     Input: Module channel number to write to */
/* unsigned int *addr;    Input: address to write in DSP memory  */
/* unsigned short *value;   Input: Value to set the symbol to   */
{

  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];

  unsigned short saddr;

  /* Move the address into Parameter memory.  The passed address is relative to
   * base memory. */

  saddr = (unsigned short) (*addr + startp);

  if ((status=dxp_write_word(ioChan,modChan,&saddr,value))!=DXP_SUCCESS) {
    sprintf(info_string, "Error writing parameter at %d", *addr);
    dxp_log_error("dxp_write_dsp_param_addr",info_string,status);
    return status;
  }

  return status;
}

/******************************************************************************
 *
 * Read a single parameter of the DSP.  Pass the symbol name, module
 * pointer and channel number.  Returns the value read using the variable value.
 * If the symbol name has the 0/1 dropped from the end, then the 32-bit
 * value is created from the 0/1 contents.  e.g. zigzag0 and zigzag1 exist
 * as a 32 bit number called zigzag.  if this routine is called with just
 * zigzag, then the 32 bit number is returned, else the 16 bit value
 * for zigzag0 or zigzag1 is returned depending on what was passed as *name.
 *
 ******************************************************************************/
static int dxp_read_dspsymbol(int* ioChan, int* modChan, char* name,
                              Board* board, double* value)
/* int *ioChan;     Input: IO channel to write to    */
/* int *modChan;    Input: Module channel number to write to */
/* char *name;     Input: user passed name of symbol   */
/* Dsp_Info *dsp;    Input: Reference to the DSP structure  */
/* double *value;          Output: Value to set the symbol to   */
{

  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];
  unsigned int i;
  unsigned short nword = 1;   /* How many words does this symbol contain?  */
  unsigned short addr=0;  /* address of the symbol in DSP memory   */
  unsigned short addr1=0;  /* address of the 2nd word in DSP memory  */
  unsigned short addr2=0;  /* address of the 3rd word (REALTIME/LIVETIME) */
  char uname[20]="", tempchar[20]=""; /* Upper case version of the user supplied name */
  unsigned short stemp;  /* Temp location to store word read from DSP */
  double dtemp, dtemp1;       /* Long versions for the temporary variable  */
  Dsp_Info *dsp = NULL;


  dsp = board->dsp[*modChan];
  ASSERT(dsp != NULL);

  /* Check that the length of the name is less than maximum allowed length */

  if (strlen(name) > (dsp->params->maxsymlen)) {
    status = DXP_NOSYMBOL;
    sprintf(info_string, "Symbol Name must be <%i characters", dsp->params->maxsymlen);
    dxp_log_error("dxp_read_dspsymbol", info_string, status);
    return status;
  }

  /* Convert the name to upper case for comparison */

  for (i = 0; i < strlen(name); i++) uname[i] = (char) toupper(name[i]);

  /* Be paranoid and check if the DSP configuration is downloaded.  If not, warn the user */

  dsp = board->dsp[*modChan];

  ASSERT(dsp != NULL);

  if ((dsp->proglen)<=0) {
    status = DXP_DSPLOAD;
    sprintf(info_string, "Must Load DSP code before reading %s", name);
    dxp_log_error("dxp_read_dspsymbol",info_string,status);
    return status;
  }

  /* First find the location of the symbol in DSP memory. */

  status = dxp_loc(uname, dsp, &addr);
  if (status != DXP_SUCCESS) {
    /* Failed to find the name directly, add 0 to the name and search again */
    strcpy(tempchar, uname);
    sprintf(tempchar, "%s0",uname);
    nword = 2;
    status = dxp_loc(tempchar, dsp, &addr);
    if (status != DXP_SUCCESS) {
      /* Failed to find the name with 0 attached, this symbol doesnt exist */
      sprintf(info_string, "Failed to find symbol %s in DSP memory", name);
      dxp_log_error("dxp_read_dspsymbol", info_string, status);
      return status;
    }
    /* Search for the 2nd entry now */
    sprintf(tempchar, "%s1",uname);
    status = dxp_loc(tempchar, dsp, &addr1);
    if (status != DXP_SUCCESS) {
      /* Failed to find the name with 1 attached, this symbol doesnt exist */
      sprintf(info_string, "Failed to find symbol %s+1 in DSP memory", name);
      dxp_log_error("dxp_read_dspsymbol", info_string, status);
      return status;
    }
    /* Special case of REALTIME or LIVETIME that are 48 bit numbers */
    if ((strcmp(uname, "LIVETIME") == 0) || (strcmp(uname, "REALTIME") == 0)) {
      nword = 3;
      /* Search for the 3rd entry now */
      sprintf(tempchar, "%s2",uname);
      status = dxp_loc(tempchar, dsp, &addr2);
      if (status != DXP_SUCCESS) {
        /* Failed to find the name with 1 attached, this symbol doesnt exist */
        sprintf(info_string, "Failed to find symbol %s+2 in DSP memory", name);
        dxp_log_error("dxp_read_dspsymbol", info_string, status);
        return status;
      }
    }
  }

  /* Check the access type for this parameter.  Only allow reading from r/w and ro
   * parameters.
   */

  if (dsp->params->parameters[addr].access == 2) {
    sprintf(info_string, "Parameter %s is Write-Only.  No peeking allowed.", name);
    status = DXP_DSPACCESS;
    dxp_log_error("dxp_read_dspsymbol", info_string, status);
    return status;
  }

  /* Read the value of the symbol from DSP memory */

  addr = (unsigned short) (dsp->params->parameters[addr].address + startp);
  status = dxp_read_word(ioChan, modChan, &addr, &stemp);
  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error writing parameter %s", name);
    dxp_log_error("dxp_read_dspsymbol",info_string,status);
    return status;
  }
  dtemp = (double) stemp;

  /* If there is a second word, read it in */
  if (nword>1) {
    addr = (unsigned short) (dsp->params->parameters[addr1].address + startp);
    status = dxp_read_word(ioChan, modChan, &addr, &stemp);
    if (status != DXP_SUCCESS) {
      sprintf(info_string, "Error writing parameter %s+1", name);
      dxp_log_error("dxp_read_dspsymbol", info_string, status);
      return status;
    }
    dtemp1 = (double) stemp;
    /* For 2 words, create the proper 32 bit number */
    *value = dtemp*65536. + dtemp1;
  } else {
    /* For single word values, assign it for the user */
    *value = dtemp;
  }

  /* Special case of 48 Bit numbers */
  if (nword==3) {
    addr = (unsigned short) (dsp->params->parameters[addr2].address + startp);
    status = dxp_read_word(ioChan, modChan, &addr, &stemp);
    if (status != DXP_SUCCESS) {
      sprintf(info_string, "Error writing parameter %s+2", name);
      dxp_log_error("dxp_read_dspsymbol", info_string, status);
      return status;
    }
    /* For 2 words, create the proper 32 bit number */
    *value += ((double) stemp) * 65536. * 65536.;
  }

  return status;

}

/******************************************************************************
 * Routine to readout the parameter memory from a single DSP.
 *
 * This routine reads the parameter list from the DSP pointed to by ioChan and
 * modChan.  It returns the array to the caller.
 *
 ******************************************************************************/
static int dxp_read_dspparams(int* ioChan, int* modChan, Board* b,
                              unsigned short* params)
/* int *ioChan;      Input: I/O channel of DSP  */
/* int *modChan;     Input: module channel of DSP  */
/* Dsp_Info *dsp;     Input: Relavent DSP info   */
/* unsigned short *params;   Output: array of DSP parameters */
{
  int status;
  unsigned int len;  /* Length of the DSP parameter memory */

  Dsp_Info *dsp = b->dsp[*modChan];


  /* Read out the parameters from the DSP memory, stored in Y memory */

  len = dsp->params->nsymbol;
  if ((status=dxp_read_block(ioChan,modChan,&startp,&len,params))!=DXP_SUCCESS) {
    dxp_log_error("dxp_read_dspparams","error reading parameters",status);
    return status;
  }

  return status;
}

/******************************************************************************
 * Routine to write parameter memory to a single DSP.
 *
 * This routine writes the parameter list to the DSP pointed to by ioChan and
 * modChan.
 *
 ******************************************************************************/
static int dxp_write_dspparams(int* ioChan, int* modChan, Dsp_Info* dsp,
                               unsigned short* params)
/* int *ioChan;      Input: I/O channel of DSP  */
/* int *modChan;     Input: module channel of DSP  */
/* Dsp_Info *dsp;     Input: Relavent DSP info   */
/* unsigned short *params;   Input: array of DSP parameters */
{

  int status;
  unsigned int len;  /* Length of the DSP parameter memory */

  /* Read out the parameters from the DSP memory, stored in Y memory */

  len = dsp->params->nsymbol;
  if ((status=dxp_write_block(ioChan,modChan,&startp,&len,params))!=DXP_SUCCESS) {
    dxp_log_error("dxp_write_dspparams","error reading parameters",status);
    return status;
  }

  return status;
}


/******************************************************************************
 * Routine to return the length of the spectrum memory.
 *
 * For 4C-2X boards, this value is stored in the DSP and dynamic.
 * For 4C boards, it is fixed.
 *
 ******************************************************************************/
static int dxp_get_spectrum_length(int *ioChan, int *modChan,
                                   Board *board, unsigned int *len)
{
  int status;

  double MCALIMLO;
  double MCALIMHI;

  char info_string[INFO_LEN];


  status = dxp_read_dspsymbol(ioChan, modChan, "MCALIMLO", board, &MCALIMLO);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error reading lower MCA bin limit for ioChan = %d",
            *ioChan);
    dxp_log_error("dxp_get_spectrum_length", info_string, status);
    return status;
  }

  status = dxp_read_dspsymbol(ioChan, modChan, "MCALIMHI", board, &MCALIMHI);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error reading upper MCA bin limit for ioChan = %d",
            *ioChan);
    dxp_log_error("dxp_get_spectrum_length", info_string, status);
    return status;
  }


  *len = (unsigned int)(MCALIMHI - MCALIMLO);

  return DXP_SUCCESS;
}


/******************************************************************************
 * Routine to return the length of the baseline memory.
 *
 * For 4C-2X boards, this value is stored in the DSP and dynamic.
 * For 4C boards, it is fixed.
 *
 ******************************************************************************/
static int dxp_get_baseline_length(int *modChan, Board *b, unsigned int *len)
{
  int status;

  double BASELEN;

  char info_string[INFO_LEN];


  status = dxp_read_dspsymbol(&(b->ioChan), modChan, "BASELEN", b, &BASELEN);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error getting BASELEN for baseline length "
            "calculation for ioChan = %d", b->ioChan);
    dxp_log_error("dxp_get_baseline_length", info_string, status);
    return status;
  }

  *len = (unsigned int)BASELEN;

  return DXP_SUCCESS;
}


/******************************************************************************
 * Routine to return the length of the event memory.
 *
 * For 4C-2X boards, this value is stored in the DSP and dynamic.
 * For 4C boards, it is fixed.
 *
 ******************************************************************************/
static unsigned int dxp_get_event_length(Dsp_Info* dsp, unsigned short* params)
/* Dsp_Info *dsp;     Input: Relavent DSP info   */
/* unsigned short *params;   Input: Array of DSP parameters */
{
  int status;
  unsigned short addr;

  if ((status=dxp_loc("EVTBLEN", dsp, &addr))!=DXP_SUCCESS) {
    dxp_log_error("dxp_get_event_length",
                  "Unable to find EVTBLEN symbol",status);
    return 0;
  }

  return ((unsigned int) params[addr]);

}

/******************************************************************************
 * Routine to return the length of the history buffer.
 *
 * For 4C-2X boards, this value is stored in the DSP and dynamic.
 * For 4C boards, it is fixed.
 *
 ******************************************************************************/
static unsigned int dxp_get_history_length(Dsp_Info* dsp, unsigned short* params)
/* Dsp_Info *dsp;     Input: Relavent DSP info   */
/* unsigned short *params;   Input: Array of DSP parameters */
{
  int status;
  unsigned short addr;

  if ((status=dxp_loc("HSTLEN", dsp, &addr))!=DXP_SUCCESS) {
    dxp_log_error("dxp_get_history_length",
                  "Unable to find HSTLEN symbol",status);
    return 0;
  }

  return ((unsigned int) params[addr]);

}

/******************************************************************************
 * Routine to readout the spectrum memory from a single DSP.
 *
 * This routine reads the spectrum histogramfrom the DSP pointed to by ioChan and
 * modChan.  It returns the array to the caller.
 *
 ******************************************************************************/
static int dxp_read_spectrum(int* ioChan, int* modChan, Board* board,
                             unsigned long* spectrum)
/* int *ioChan;     Input: I/O channel of DSP  */
/* int *modChan;    Input: module channel of DSP  */
/* Board *board;    Input: Relavent Board info   */
/* unsigned long *spectrum;  Output: array of spectrum values */
{

  int status;
  unsigned int i;
  unsigned short *spec, addr, start;
  unsigned int len;     /* number of short words in spectrum */
  char info_string[INFO_LEN];

  if ((status=dxp_loc("SPECTSTART", board->dsp[*modChan], &addr))!=DXP_SUCCESS) {
    status = DXP_NOSYMBOL;
    dxp_log_error("dxp_read_spectrum",
                  "Unable to find SPECTSTART symbol",status);
    return status;
  }
  start = (unsigned short) (board->params[*modChan][addr] + PROGRAM_BASE);

  status = dxp_get_spectrum_length(ioChan, modChan, board, &len);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error reading the spectrum length for ioChan = %d",
            *ioChan);
    dxp_log_error("dxp_read_spectrum", info_string, status);
    return status;
  }

  len = len * 2;

  /* Allocate memory for the spectrum */
  spec = (unsigned short *) x10p_md_alloc(len*sizeof(unsigned short));

  /* Read the spectrum */

  if ((status=dxp_read_block(ioChan,modChan,&start,&len,spec))!=DXP_SUCCESS) {
    dxp_log_error("dxp_read_spectrum","Error reading out spectrum",status);
    x10p_md_free(spec);
    return status;
  }

  /* Unpack the array of shorts into the long spectra */

  for (i = 0; i < ((unsigned int) len / 2); i++) {
    spectrum[i] = spec[2 * i] | spec[2 * i + 1]<<16;
  }

  /* Free memory used for the spectrum */
  x10p_md_free(spec);

  return status;
}

/******************************************************************************
 * Routine to readout the baseline histogram from a single DSP.
 *
 * This routine reads the baselin histogram from the DSP pointed to by ioChan and
 * modChan.  It returns the array to the caller.
 *
 ******************************************************************************/
static int dxp_read_baseline(int* ioChan, int* modChan, Board* board,
                             unsigned long* baseline)
/* int *ioChan;      Input: I/O channel of DSP     */
/* int *modChan;     Input: module channel of DSP     */
/* Board *board;    Input: Relavent Board info   */
/* unsigned short *baseline;  Output: array of baseline histogram values */
{

  int status;
  unsigned short addr, start;
  unsigned int i;
  unsigned int len;     /* number of short words in spectrum */

  char info_string[INFO_LEN];

  unsigned short *us_baseline = NULL;


  status = dxp_loc("BASESTART", board->dsp[*modChan], &addr);
  if (status != DXP_SUCCESS) {
    status = DXP_NOSYMBOL;
    dxp_log_error("dxp_read_baseline",
                  "Unable to find BASESTART symbol", status);
    return status;
  }
  start = (unsigned short) (board->params[*modChan][addr] + DATA_BASE);

  status = dxp_get_baseline_length(modChan, board, &len);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error getting baseline length for ioChan = %d",
            *ioChan);
    dxp_log_error("dxp_read_baseline", info_string, status);
    return status;
  }

  us_baseline = (unsigned short *)x10p_md_alloc(len * sizeof(unsigned short));

  if (!us_baseline) {
    sprintf(info_string, "Error allocating %d bytes for 'us_baseline'",
            len * sizeof(unsigned short));
    dxp_log_error("dxp_read_baseline", info_string, DXP_ALLOCMEM);
    return DXP_ALLOCMEM;
  }

  /* Read out the basline histogram. */
  status = dxp_read_block(ioChan, modChan, &start, &len, us_baseline);
  if (status != DXP_SUCCESS) {
    dxp_log_error("dxp_read_baseline", "Error reading out baseline", status);
    x10p_md_free(us_baseline);
    return status;
  }

  for (i = 0; i < len; i++) {
    baseline[i] = (unsigned long)us_baseline[i];
  }

  x10p_md_free(us_baseline);
  return status;
}

/******************************************************************************
 * Routine to readout the history buffer from a single DSP.
 *
 * This routine reads the history buffer from the DSP pointed to by ioChan and
 * modChan.  It returns the array to the caller.
 *
 ******************************************************************************/
static int dxp_read_history(int* ioChan, int* modChan, Board* board,
                            unsigned short* history)
/* int *ioChan;      Input: I/O channel of DSP     */
/* int *modChan;     Input: module channel of DSP    */
/* Board *board;    Input: Relavent Board info   */
/* unsigned short *history;   Output: array of history buffervalues  */
{

  int status;

  unsigned short addr, start;

  unsigned int len;


  status = dxp_loc("HSTSTART", board->dsp[*modChan], &addr);
  if (status != DXP_SUCCESS) {
    status = DXP_NOSYMBOL;
    dxp_log_error("dxp_read_history",
                  "Unable to find HSTSTART symbol", status);
    return status;
  }
  start = (unsigned short) (board->params[*modChan][addr] + DATA_BASE);
  len = dxp_get_history_length(board->dsp[*modChan], board->params[*modChan]);

  status = dxp_read_block(ioChan, modChan, &start, &len, history);

  if (status != DXP_SUCCESS) {
    dxp_log_error("dxp_read_history", "Error reading out history buffer", status);
    return status;
  }

  return status;
}

/******************************************************************************
 *
 * Routine to write data to the history buffer for a single DSP.
 *
 ******************************************************************************/
static int dxp_write_history(int* ioChan, int* modChan, Board* board,
                             unsigned int *length,  unsigned short* history)
/* int *ioChan;      Input: I/O channel of DSP     */
/* int *modChan;     Input: module channel of DSP    */
/* Board *board;        Input: Relavent Board info     */
/* unsigned int *length;            Input: Number of words to write       */
/* unsigned short *history;   Input: array of history buffer values */
{

  int status;
  unsigned short addr, start;

  status = dxp_loc("HSTSTART", board->dsp[*modChan], &addr);
  if (status != DXP_SUCCESS) {
    status = DXP_NOSYMBOL;
    dxp_log_error("dxp_write_history", "Unable to find HSTSTART symbol", status);
    return status;
  }
  start = (unsigned short) (board->params[*modChan][addr] + DATA_BASE);

  /* Write to the history buffer */
  status = dxp_write_block(ioChan, modChan, &start, length, history);
  if (status != DXP_SUCCESS) {
    dxp_log_error("dxp_write_history", "Error writing to the history buffer", status);
    return status;
  }

  return status;
}

/******************************************************************************
 * Routine to prepare the DXP4C2X for data readout.
 *
 * This routine will ensure that the board is in a state that
 * allows readout via CAMAC bus.
 *
 ******************************************************************************/
int dxp_prep_for_readout(int* ioChan, int *modChan)
/* int *ioChan;      Input: I/O channel of DXP module   */
/* int *modChan;     Input: Module channel number    */
{
  int *itemp;

  /* Assign input parameters to avoid compiler warnings */
  itemp = ioChan;
  itemp = modChan;

  /*
   *   Nothing needs to be done for the DXP4C2X, the
   * CAMAC interface does not interfere with normal data taking.
   */

  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to prepare the DXP4C2X for data readout.
 *
 * This routine will ensure that the board is in a state that
 * allows readout via CAMAC bus.
 *
 ******************************************************************************/
int dxp_done_with_readout(int* ioChan, int *modChan, Board *board)
/* int *ioChan;      Input: I/O channel of DXP module */
/* int *modChan;     Input: Module channel number  */
/* Board *board;     Input: Relavent Board info     */
{
  int *itemp;
  Board *btemp;

  /* Assign input parameters to avoid compiler warnings */
  itemp = ioChan;
  itemp = modChan;
  btemp = board;

  /*
   * Nothing needs to be done for the DXP4C2X, the CAMAC interface
   * does not interfere with normal data taking.
   */

  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to begin a data taking run.
 *
 * This routine starts a run on the specified CAMAC channel.  It tells the DXP
 * whether to ignore the gate signal and whether to clear the MCA.
 *
 ******************************************************************************/
static int dxp_begin_run(int* ioChan, int* modChan, unsigned short* gate,
                         unsigned short* resume, Board *board, int *id)
/* int *ioChan;      Input: I/O channel of DXP module   */
/* int *modChan;     Input: Module channel number    */
/* unsigned short *gate;   Input: ignore (1) or use (0) ext. gate */
/* unsigned short *resume;   Input: clear MCA first(0) or update (1) */
/* Board *board;     Input: Relavent Board info     */
{
  /*
   *   Initiate data taking for all channels of a single DXP module.
   */
  int status;
  unsigned short data;
  int *itemp;
  Board *btemp;
  char info_string[INFO_LEN];

  UNUSED(id);


  dxp_log_debug("dxp_begin_run", "Entering dxp_begin_run()");

  /* Assign input parameters to avoid compiler warnings */
  itemp = modChan;
  btemp = board;

  /* Read-Modify-Write to CSR to start data run */
  status = dxp_read_csr(ioChan, &data);
  if (status!=DXP_SUCCESS) {
    dxp_log_error("dxp_begin_run","Error reading the CSR",status);
    return status;
  }

  sprintf(info_string, "resume = %d, gate = %d", *resume, *gate);
  dxp_log_debug("dxp_begin_run",info_string);

  data |= MASK_RUNENABLE;
  if (*resume == CLEARMCA) {
    data |= MASK_RESETMCA;
  } else {
    data &= ~MASK_RESETMCA;
  }
  if (*gate == IGNOREGATE) {
    data |= MASK_IGNOREGATE;
  } else {
    data &= ~MASK_IGNOREGATE;
  }

  status = dxp_write_csr(ioChan, &data);                    /* write to CSR */
  if (status!=DXP_SUCCESS) {
    dxp_log_error("dxp_begin_run","Error writing CSR",status);
    return status;
  }

  dxp_log_debug("dxp_begin_run", "Exiting dxp_being_run()");

  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to end a data taking run.
 *
 * This routine ends the run on the specified CAMAC channel.
 *
 ******************************************************************************/
static int dxp_end_run(int* ioChan, int* modChan, Board *board)
/* int *ioChan;      Input: I/O channel of DXP module */
/* int *modChan;     Input: Module channel number  */
{
  /*
   *   Terminate a data taking for all channels of a single DXP module.
   */
  int status;
  unsigned short data;

  UNUSED(board);


  status = dxp_read_csr(ioChan, &data);                    /* read from CSR */
  if (status!=DXP_SUCCESS) {
    dxp_log_error("dxp_end_run","Error reading CSR",status);
  }

  data &= ~MASK_RUNENABLE;

  status = dxp_write_csr(ioChan, &data);                    /* write to CSR */
  if (status!=DXP_SUCCESS) {
    dxp_log_error("dxp_end_run","Error writing CSR",status);
  }

  if ((status=dxp_clear_LAM(ioChan, modChan))!=DXP_SUCCESS) {
    dxp_log_error("dxp_end_run"," ",status);
    return status;
  }

  return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to determine if the module thinks a run is active.
 *
 ******************************************************************************/
static int dxp_run_active(int* ioChan, int* modChan, int* active)
/* int *ioChan;      Input: I/O channel of DXP module   */
/* int *modChan;     Input: module channel number of DXP module */
/* int *active;      Output: Does the module think a run is active? */
{
  int status;
  unsigned short data;
  int *itemp;

  /* Assign input parameters to avoid compiler warnings */
  itemp = modChan;

  status = dxp_read_csr(ioChan, &data);                    /* read the CSR */
  if (status!=DXP_SUCCESS) {
    dxp_log_error("dxp_run_active","Error reading the CSR",status);
  }

  /* Check the run active bit of the CSR */
  if ((data & (0x1<<11))!=0) *active = 1;

  return status;

}

/******************************************************************************
 *
 * Routine to start a control task routine.  Definitions for types are contained
 * in the xerxes_generic.h file.
 *
 ******************************************************************************/
static int dxp_begin_control_task(int* ioChan, int* modChan, short *type,
                                  unsigned int *length, int *info, Board *board)
{
  int status = DXP_SUCCESS;
  char info_string[INFO_LEN];

  /* Variables for parameters to be read/written from/to the DSP */
  unsigned short runtasks, whichtest, tracewait;
  unsigned short EXTPAGE;
  unsigned short EXTADDRESS;
  unsigned short EXTLENGTH;

  unsigned short zero=0;
  unsigned short *ustemp;
  double temp;

  unsigned int i;

  dxp_log_debug("dxp_begin_control_task", "Entering dxp_begin_control_task()");

  /* Check that the length of allocated memory is greater than 0 */
  if (*length==0) {
    status = DXP_ALLOCMEM;
    sprintf(info_string,
            "Must pass an array of at least length 1 containing LOOPCOUNT for module %d chan %d",
            board->mod,*modChan);
    dxp_log_error("dxp_begin_control_task",info_string,status);
    return status;
  }

  /* Read/Modify/Write the RUNTASKS parameter */
  /* read */
  status = dxp_read_dspsymbol(ioChan, modChan,
                              "RUNTASKS", board, &temp);
  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error reading RUNTASKS from module %d chan %d",
            board->mod, *modChan);
    dxp_log_error("dxp_begin_control_task", info_string, status);
    return status;
  }

  runtasks = (unsigned short) temp;
  sprintf(info_string, "runtasks = %x\n", runtasks);
  dxp_log_debug("dxp_begin_control_task", info_string);
  /* modify */
  if (*type != CT_DXPX10P_BASELINE_HIST) {
    runtasks |= CONTROL_TASK;
  }

  sprintf(info_string, "runtasks = %#x\n", runtasks);
  dxp_log_debug("dxp_begin_control_task", info_string);

  if ((*type == CT_ADC) || (*type == CT_DXPX10P_ADC) ||
      (*type == CT_DXPX10P_BASELINE_HIST) ||
      (*type == CT_DXPX10P_READ_MEMORY) || (*type == CT_DXPX10P_WRITE_MEMORY)) {
    /* For baseline history, we just want to change the RUNTASKS variable to stop filling of the history,
     * so we can readout the buffer without it constantly being modified. */
    runtasks |= STOP_BASELINE;
  }

  sprintf(info_string, "runtasks = %#x\n", runtasks);
  dxp_log_debug("dxp_begin_control_task", info_string);

  /* write */
  status = dxp_modify_dspsymbol(ioChan, modChan,
                                "RUNTASKS",&runtasks,board);
  if (status != DXP_SUCCESS) {
    sprintf(info_string,
            "Error writing RUNTASKS from module %d chan %d", board->mod, *modChan);
    dxp_log_error("dxp_begin_control_task", info_string, status);
    return status;
  }

  /* First check if the control task is the ADC readout */
  if (*type==CT_DXPX10P_SET_ASCDAC) {
    whichtest = WHICHTEST_SET_ASCDAC;
  } else if ((*type==CT_ADC) || (*type==CT_DXPX10P_ADC)) {
    whichtest = WHICHTEST_ACQUIRE_ADC;
    /* Make sure at least the user thinks there is allocated memory */
    if (*length > 1) {
      /* write TRACEWAIT */
      tracewait = (unsigned short) info[1];
      status = dxp_modify_dspsymbol(ioChan, modChan,
                                    "TRACEWAIT", &tracewait, board);
      if (status != DXP_SUCCESS) {
        sprintf(info_string,
                "Error writing TRACEWAIT to module %d chan %d",board->mod,*modChan);
        dxp_log_error("dxp_begin_control_task",info_string,status);
        return status;
      }
    } else {
      status = DXP_ALLOCMEM;
      sprintf(info_string,
              "This control task requires at least 2 parameters for mod %d chan %d",board->mod,*modChan);
      dxp_log_error("dxp_begin_control_task",info_string,status);
      return status;
    }
  } else if (*type == CT_DXPX10P_TRKDAC) {
    whichtest = WHICHTEST_TRKDAC;
  } else if (*type == CT_DXPX10P_SLOPE_CALIB) {
    whichtest = WHICHTEST_SLOPE_CALIB;
  } else if (*type == CT_DXPX10P_SLEEP_DSP) {
    whichtest = WHICHTEST_SLEEP_DSP;
  } else if (*type == CT_DXPX10P_PROGRAM_FIPPI) {
    whichtest = WHICHTEST_PROGRAM_FIPPI;
  } else if (*type == CT_DXPX10P_SET_POLARITY) {
    whichtest = WHICHTEST_SET_POLARITY;
  } else if (*type == CT_DXPX10P_CLOSE_INPUT_RELAY) {
    whichtest = WHICHTEST_CLOSE_INPUT_RELAY;
  } else if (*type == CT_DXPX10P_OPEN_INPUT_RELAY) {
    whichtest = WHICHTEST_OPEN_INPUT_RELAY;
  } else if (*type == CT_DXPX10P_RC_BASELINE) {
    whichtest = WHICHTEST_RC_BASELINE;
  } else if (*type == CT_DXPX10P_RC_EVENT) {
    whichtest = WHICHTEST_RC_EVENT;
  } else if (*type == CT_DXPX10P_BASELINE_HIST) {}
  else if (*type == CT_DXPX10P_RESET) {
    whichtest = WHICHTEST_RESET;
  } else if ((*type == CT_DXPX10P_READ_MEMORY) || (*type == CT_DXPX10P_WRITE_MEMORY)) {
    if (*type == CT_DXPX10P_READ_MEMORY) {
      whichtest = WHICHTEST_READ_MEMORY;
    } else {
      whichtest = WHICHTEST_WRITE_MEMORY;
    }
    /* set the Info value */
    if (*length > 3) {
      if (info[1] != -1) {
        /* write EXTPAGE */
        EXTPAGE = (unsigned short) info[1];
        status = dxp_modify_dspsymbol(ioChan, modChan, "EXTPAGE",
                                      &EXTPAGE, board);
        if (status != DXP_SUCCESS) {
          sprintf(info_string,
                  "Error writing EXTPAGE to module %d chan %d", board->mod, *modChan);
          dxp_log_error("dxp_begin_control_task", info_string, status);
          return status;
        }
      }
      if (info[2] != -1) {
        /* write EXTADDRESS */
        EXTADDRESS = (unsigned short) info[2];
        status = dxp_modify_dspsymbol(ioChan, modChan, "EXTADDRESS",
                                      &EXTADDRESS, board);
        if (status != DXP_SUCCESS) {
          sprintf(info_string,
                  "Error writing EXTADDRESS to module %d chan %d", board->mod, *modChan);
          dxp_log_error("dxp_begin_control_task", info_string, status);
          return status;
        }
      }
      if (info[3] != -1) {
        /* write EXTLENGTH */
        EXTLENGTH = (unsigned short) info[3];
        status = dxp_modify_dspsymbol(ioChan, modChan, "EXTLENGTH",
                                      &EXTLENGTH, board);
        if (status != DXP_SUCCESS) {
          sprintf(info_string,
                  "Error writing EXTLENGTH to module %d chan %d", board->mod, *modChan);
          dxp_log_error("dxp_begin_control_task", info_string, status);
          return status;
        }
      }
    } else {
      status = DXP_ALLOCMEM;
      sprintf(info_string,
              "This control task requires at least 4 parameters for mod %d chan %d",
              board->mod, *modChan);
      dxp_log_error("dxp_begin_control_task", info_string, status);
      return status;
    }

    if (*type == CT_DXPX10P_WRITE_MEMORY) {
      ustemp = (unsigned short *)x10p_md_alloc((*length - 4) * sizeof(unsigned short));

      if (ustemp == NULL) {
        sprintf(info_string, "Out-of-memory allocating %d bytes for 'ustemp'",
                (*length - 4) * sizeof(unsigned short));
        dxp_log_error("dxp_begin_control_task", info_string, DXP_NOMEM);
        return DXP_NOMEM;
      }

      for (i = 0; i < (*length - 4); i++) {
        ustemp[i] = (unsigned short) info[i + 4];
      }

      /* Write the output buffer */
      status=dxp_write_history(ioChan, modChan, board, length, ustemp);
      if (status != DXP_SUCCESS) {
        sprintf(info_string,
                "Error writing the history buffer on module %d chan %d",board->mod,*modChan);
        dxp_log_error("dxp_begin_control_task",info_string,status);
        x10p_md_free(ustemp);
        return status;
      }
      x10p_md_free(ustemp);
    }
  } else {
    status=DXP_NOCONTROLTYPE;
    sprintf(info_string,
            "Unknown control type %d for this DXP4C2X module",*type);
    dxp_log_error("dxp_begin_control_task",info_string,status);
    return status;
  }

  /* For baseline history, we just want to change the RUNTASKS variable to stop filling of the history,
   * so we can readout the buffer without it constantly being modified. */

  if (*type != CT_DXPX10P_BASELINE_HIST) {
    /* write WHICHTEST */
    status=dxp_modify_dspsymbol(ioChan, modChan, "WHICHTEST", &whichtest,
                                board);
    if (status != DXP_SUCCESS) {
      sprintf(info_string,
              "Error writing WHICHTEST to module %d chan %d", board->mod, *modChan);
      dxp_log_error("dxp_begin_control_task",info_string,status);
      return status;
    }

    /* start the control run */
    status=dxp_begin_run(ioChan, modChan, &zero, &zero, board, NULL);
    if (status != DXP_SUCCESS) {
      sprintf(info_string,
              "Error starting control run on module %d chan %d", board->mod, *modChan);
      dxp_log_error("dxp_begin_control_task", info_string, status);
      return status;
    }
  }

  return DXP_SUCCESS;

}


/******************************************************************************
 *
 * Routine to end a control task routine.
 *
 ******************************************************************************/
static int dxp_end_control_task(int* ioChan, int* modChan, Board *board)
{
  int status = DXP_SUCCESS;
  char info_string[INFO_LEN];
  double temp;
  unsigned short runtasks;
  unsigned short gate = 0;
  unsigned short resume = 0;

  int mod;
  unsigned short value;
  float timeout;

  mod = board->mod;

  /* Special case for SLEEP mode.  Merely start a run prior to ending the run and the DSP will
   * awaken */
  if (board->state[4] == CT_DXPX10P_SLEEP_DSP) {
    if ((status = dxp_begin_run(ioChan, modChan, &gate,
                                &resume, board, NULL)) != DXP_SUCCESS) {
      sprintf(info_string, "Error starting run to end DSP sleep for module %i", mod);
      status = DXP_DSPSLEEP;
      dxp_log_error("dxp_end_control_task", info_string, status);
      return status;
    }
    /* Now wait for BUSY=0 to indicate the DSP is IDLE */
    value = 0;
    timeout = 2.0;
    if ((status=dxp_download_dsp_done(ioChan, modChan, &mod, board,
                                      &value, &timeout))!=DXP_SUCCESS) {
      sprintf(info_string,"Error waiting for BUSY=0 state for module %i",mod);
      status = DXP_DSPTIMEOUT;
      dxp_log_error("dxp_end_control_task",info_string,status);
      return status;
    }
  }

  /* Do not need to end the run if the control task was baseline history */
  if (board->state[4] != CT_DXPX10P_BASELINE_HIST) {
    status = dxp_end_run(ioChan, modChan, board);
    if (status != DXP_SUCCESS) {
      sprintf(info_string,
              "Error ending control task run for chan %d", *modChan);
      dxp_log_error("dxp_end_control_task", info_string, status);
      return status;
    }
  }

  /* Read/Modify/Write the RUNTASKS parameter */
  /* read */
  status = dxp_read_dspsymbol(ioChan, modChan,
                              "RUNTASKS", board, &temp);
  if (status != DXP_SUCCESS) {
    sprintf(info_string,
            "Error reading RUNTASKS from module %d chan %d", board->mod, *modChan);
    dxp_log_error("dxp_end_control_task", info_string, status);
    return status;
  }
  /* Remove the CONTROL_TASK bit and add the STOP_BASELINE bit */
  /* modify */
  /* Since we never actually start a run for the baseline history, we don't need to
   * flip the CONTROL_TASK bit.  But for all others, we do want to flip that bit */
  runtasks = (unsigned short) temp;
  if (board->state[4] != CT_DXPX10P_BASELINE_HIST) {
    runtasks &= ~CONTROL_TASK;
  }
  /* By default turn off the STOP_BASELINE bit...only matters for BASELINE_HIST and
   * ADC runs */
  runtasks &= ~STOP_BASELINE;

  sprintf(info_string, "runtasks = %#x\n", runtasks);
  dxp_log_debug("dxp_end_control_task", info_string);

  /* write */
  status = dxp_modify_dspsymbol(ioChan, modChan, "RUNTASKS", &runtasks, board);
  if (status != DXP_SUCCESS) {
    sprintf(info_string,
            "Error writing RUNTASKS from module %d chan %d", board->mod, *modChan);
    dxp_log_error("dxp_end_control_task", info_string, status);
    return status;
  }

  return status;
}

/******************************************************************************
 *
 * Routine to get control task parameters.
 *
 ******************************************************************************/
static int dxp_control_task_params(int* ioChan, int* modChan, short *type,
                                   Board *board, int *info)
/* int *ioChan;      Input: I/O channel of DXP module   */
/* int *modChan;     Input: module channel number of DXP module */
/* short *type;      Input: type of control task to perfomr  */
/* Board *board;     Input: Board data       */
/* int info[20];     Output: Configuration info for the task  */
{
  int status = DXP_SUCCESS;
  char info_string[INFO_LEN];
  int *itemp;

  /* Dummy assignment to get rid of warnings */
  itemp = ioChan;

  /* Default values */
  /* nothing to readout here */
  info[0] = 0;
  /* stop when user is ready */
  info[1] = 0;
  /* stop when user is ready */
  info[2] = 0;

  /* Check the control task type */
  if (*type==CT_DXPX10P_SET_ASCDAC) {}
  else if ((*type==CT_ADC) || (*type==CT_DXPX10P_ADC)) {
    /* length=spectrum length*/
    info[0] = dxp_get_history_length(board->dsp[*modChan], board->params[*modChan]);
    /* Recommend waiting 4ms initially, 400ns*spectrum length */
    info[1] = 4;
    /* Recomment 1ms polling after initial wait */
    info[2] = 1;
  } else if (*type==CT_DXPX10P_TRKDAC) {
    /* length=1, the value of the tracking DAC */
    info[0] = 1;
    /* Recommend waiting 10ms initially */
    info[1] = 10;
    /* Recomment 1ms polling after initial wait */
    info[2] = 1;
  } else if (*type==CT_DXPX10P_SLOPE_CALIB) {}
  else if (*type==CT_DXPX10P_SLEEP_DSP) {}
  else if (*type==CT_DXPX10P_PROGRAM_FIPPI) {}
  else if (*type==CT_DXPX10P_SET_POLARITY) {}
  else if (*type==CT_DXPX10P_CLOSE_INPUT_RELAY) {}
  else if (*type==CT_DXPX10P_OPEN_INPUT_RELAY) {}
  else if (*type==CT_DXPX10P_RC_BASELINE) {}
  else if (*type==CT_DXPX10P_RC_EVENT) {}
  else if (*type==CT_DXPX10P_BASELINE_HIST) {
    info[0] = dxp_get_history_length(board->dsp[*modChan], board->params[*modChan]);
    /* Recommend waiting 0ms, baseline history is available immediately */
    info[1] = 1;
    /* Recomment 0ms polling, baseline history is available immediately */
    info[2] = 1;
  } else if (*type==CT_DXPX10P_RESET) {}
  else if ((*type==CT_DXPX10P_READ_MEMORY) || (*type==CT_DXPX10P_WRITE_MEMORY)) {
    /* length=event buffer length */
    info[0] = dxp_get_history_length(board->dsp[*modChan], board->params[*modChan]);
    /* Recommend waiting 500ns*history length*/
    info[1] = (int) (.0005*info[0]);
    /* Recommend 1ms polling after initial wait */
    info[2] = 1;
  } else {
    status=DXP_NOCONTROLTYPE;
    sprintf(info_string,
            "Unknown control type %d for this DXP4C2X module",*type);
    dxp_log_error("dxp_control_task_params",info_string,status);
    return status;
  }

  return status;

}

/******************************************************************************
 *
 * Routine to return control task data.
 *
 ******************************************************************************/
static int dxp_control_task_data(int* ioChan, int* modChan, short *type,
                                 Board *board, void *data)
{
  int status = DXP_SUCCESS;
  char info_string[INFO_LEN];

  unsigned int i, lenh;
  unsigned short *stemp;
  unsigned long *ultemp;
  long *ltemp;
  unsigned long hststart, circular, offset;
  unsigned long lenext;
  double dtemp;


  ASSERT(ioChan  != NULL);
  ASSERT(modChan != NULL);
  ASSERT(type    != NULL);
  ASSERT(board   != NULL);
  ASSERT(data    != NULL);

  if (*type == CT_DXPX10P_SET_ASCDAC) {
    /* Do nothing */

  } else if ((*type == CT_ADC) ||
             (*type == CT_DXPX10P_ADC) ||
             (*type == CT_DXPX10P_BASELINE_HIST) ||
             (*type == CT_DXPX10P_READ_MEMORY) ||
             (*type == CT_DXPX10P_WRITE_MEMORY)) {

    lenh = dxp_get_history_length(board->dsp[*modChan],
                                  board->params[*modChan]);

    stemp = (unsigned short *)x10p_md_alloc(lenh * sizeof(unsigned short));

    if (!stemp) {
      sprintf(info_string, "Not enough memory to allocate temporary array "
              "of length %d", lenh);
      dxp_log_error("dxp_control_task_data", info_string, status);
      return status;
    }

    status = dxp_read_history(ioChan, modChan, board, stemp);

    if (status != DXP_SUCCESS) {
      x10p_md_free(stemp);
      sprintf(info_string, "Error ending control task run for chan %d",
              *modChan);
      dxp_log_error("dxp_control_task_data", info_string, status);
      return status;
    }

    if (*type != CT_DXPX10P_BASELINE_HIST)  {
      /* Assign the void *data to a local unsigned long * array for
       * assignment purposes. Just easier to read than casting the 
       * void * on the left hand side.
       */
      ultemp = (unsigned long *) data;

      /* External memory uses EXTLENGTH, not HSTLEN to determine how
       * much data to copy from the history buffer.
       */
      if (*type == CT_DXPX10P_READ_MEMORY) {

        status = dxp_read_dspsymbol(ioChan, modChan, "EXTLENGTH",
                                    board, &dtemp);

        if (status != DXP_SUCCESS) {
          x10p_md_free(stemp);
          sprintf(info_string, "Error reading EXTLENGTH from module "
                  "%d chan %d", board->mod, *modChan);
          dxp_log_error("dxp_end_control_task", info_string, status);
          return status;
        }

        lenext = (unsigned short) dtemp;

        for (i = 0; i < lenext; i++) {
          ultemp[i] = (unsigned long) stemp[i];
        }

      } else {
        /* Section for the ADC Traces */
        for (i = 0; i < lenh; i++) ultemp[i] = (unsigned long) stemp[i];
      }
    } else {

      dxp_log_debug("dxp_control_task_data",
                    "type == CT_DXPX10P_BASELINE_HIST");

      /* For the baseline history, get the start of the circular buffer and return the array with the
       * start at location 0 instead of the random location within the DSP memory. */
      status = dxp_read_dspsymbol(ioChan, modChan, "CIRCULAR", board,
                                  &dtemp);
      if (status != DXP_SUCCESS) {
        x10p_md_free(stemp);
        sprintf(info_string,
                "Error reading CIRCULAR from module %d chan %d",board->mod,*modChan);
        dxp_log_error("dxp_end_control_task",info_string,status);
        return status;
      }
      circular = (unsigned short) dtemp;

      status = dxp_read_dspsymbol(ioChan, modChan, "HSTSTART", board,
                                  &dtemp);
      if (status != DXP_SUCCESS) {
        x10p_md_free(stemp);
        sprintf(info_string,
                "Error reading HSTSTART from module %d chan %d", board->mod, *modChan);
        dxp_log_error("dxp_end_control_task", info_string, status);
        return status;
      }
      hststart = (unsigned short) dtemp;

      ltemp = (long *) data;
      offset = circular - hststart;
      /* Do a short cast of the unsigned short to force the compiler to do the
       * sign-extension properly.  If you try to cast directly into a long, 
       * the compiler will NOT sign extend the unsigned short array */
      for (i = offset; i < lenh; i++) ltemp[i-offset] = (long) ((short) stemp[i]);
      for (i = 0; i < offset; i++) ltemp[i+(lenh-offset)] = (long) ((short) stemp[i]);
    }

    /* Free memory */
    x10p_md_free(stemp);

  } else if (*type == CT_DXPX10P_TRKDAC) {}
  else if (*type == CT_DXPX10P_SLOPE_CALIB) {}
  else if (*type == CT_DXPX10P_SLEEP_DSP) {}
  else if (*type == CT_DXPX10P_PROGRAM_FIPPI) {}
  else if (*type == CT_DXPX10P_SET_POLARITY) {}
  else if (*type == CT_DXPX10P_CLOSE_INPUT_RELAY) {}
  else if (*type == CT_DXPX10P_OPEN_INPUT_RELAY) {}
  else if (*type == CT_DXPX10P_RC_BASELINE) {}
  else if (*type == CT_DXPX10P_RC_EVENT) {}
  else if (*type == CT_DXPX10P_RESET) {}
  else {
    status=DXP_NOCONTROLTYPE;
    sprintf(info_string,
            "Unknown control type %d for this Saturn module",*type);
    dxp_log_error("dxp_control_task_data",info_string,status);
    return status;
  }

  return status;

}

/******************************************************************************
 * Routine to start a special Calibration run
 *
 * Begin a special data run for all channels of of a single DXP module.
 * Currently supported tasks:
 * SET_ASCDAC
 * ACQUIRE_ADC
 * TRKDAC
 * SLOPE_CALIB
 * SLEEP_DSP
 * PROGRAM_FIPPI
 * SET_POLARITY
 * CLOSE_INPUT_RELAY
 * OPEN_INPUT_RELAY
 * RC_BASELINE
 * RC_EVENT
 *
 *
 ******************************************************************************/
static int dxp_begin_calibrate(int* ioChan, int* modChan, int* calib_task, Board *board)
/* int *ioChan;     Input: I/O channel of DXP module*/
/* int *modChan;    Input: Module channel number */
/* int *calib_task;    Input: int.gain (1) reset (2)   */
/* Board *board;    Input: Relavent Board info  */
{
  /*
   */
  int status;
  char info_string[INFO_LEN];
  unsigned short ignore=IGNOREGATE,resume=CLEARMCA;
  unsigned short whichtest= (unsigned short) *calib_task;
  unsigned short runtasks=CONTROL_TASK;

  /* Be paranoid and check if the DSP configuration is downloaded.  If not, do it */

  if ((board->dsp[*modChan]->proglen) <= 0) {
    status = DXP_DSPLOAD;
    sprintf(info_string, "Must Load DSP code before calibrations");
    dxp_log_error("dxp_begin_calibrate", info_string, status);
    return status;
  }

  /* Determine if we are performing a valid task */

  if (*calib_task==99) {
    status = DXP_BAD_PARAM;
    dxp_log_error("dxp_begin_calibrate",
                  "RESET calibration not necc with this model", status);
    return status;
  } else if ((*calib_task==4)||(*calib_task==5)||
             ((*calib_task>6)&&(*calib_task<11))||(*calib_task>16)) {
    status = DXP_BAD_PARAM;
    sprintf(info_string,"Calibration task = %d is nonexistant",
            *calib_task);
    dxp_log_error("dxp_begin_calibrate", info_string, status);
    return status;
  }


  if ((status=dxp_modify_dspsymbol(ioChan, modChan,
                                   "WHICHTEST", &whichtest, board))!=DXP_SUCCESS) {
    dxp_log_error("dxp_begin_calibrate","Error writing WHICHTEST",status);
    return status;
  }

  if ((status=dxp_modify_dspsymbol(ioChan, modChan,
                                   "RUNTASKS",&runtasks, board))!=DXP_SUCCESS) {
    dxp_log_error("dxp_begin_calibrate","Error writing RUNTASKS",status);
    return status;
  }

  /* Finally startup the Calibration run. */

  if ((status = dxp_begin_run(ioChan, modChan, &ignore, &resume, board, NULL))!=DXP_SUCCESS) {
    dxp_log_error("dxp_begin_calibrate"," ",status);
  }

  return status;
}

/******************************************************************************
 * Routine to decode the error message from the DSP after a run if performed
 *
 * Returns the RUNERROR and ERRINFO words from the DSP parameter block
 *
 ******************************************************************************/
static int dxp_decode_error(unsigned short array[], Dsp_Info* dsp,
                            unsigned short* runerror, unsigned short* errinfo)
/* unsigned short array[];  Input: array from parameter block read */
/* Dsp_Info *dsp;    Input: Relavent DSP info     */
/* unsigned short *runerror; Output: runerror word     */
/* unsigned short *errinfo;  Output: errinfo word      */
{

  int status;
  char info_string[INFO_LEN];
  unsigned short addr_RUNERROR,addr_ERRINFO;

  /* Be paranoid and check if the DSP configuration is downloaded.  If not, do it */

  if ((dsp->proglen)<=0) {
    status = DXP_DSPLOAD;
    sprintf(info_string, "Must Load DSP code before decoding errors");
    dxp_log_error("dxp_decode_error", info_string, status);
    return status;
  }

  /* Now pluck out the location of a couple of symbols */

  status  = dxp_loc("RUNERROR", dsp, &addr_RUNERROR);
  status += dxp_loc("ERRINFO", dsp, &addr_ERRINFO);
  if (status!=DXP_SUCCESS) {
    status=DXP_NOSYMBOL;
    dxp_log_error("dxp_decode_error",
                  "Could not locate either RUNERROR or ERRINFO symbols",status);
    return status;
  }

  *runerror = array[addr_RUNERROR];
  *errinfo = (unsigned short) (*runerror!=0 ? array[addr_ERRINFO] : 0);
  return DXP_SUCCESS;

}

/******************************************************************************
 * Routine to clear an error in the DSP
 *
 * Clears non-fatal DSP error in one or all channels of a single DXP module.
 * If modChan is -1 then all channels are cleared on the DXP.
 *
 ******************************************************************************/
static int dxp_clear_error(int* ioChan, int* modChan, Board* board)
/* int *ioChan;     Input: I/O channel of DXP module  */
/* int *modChan;    Input: DXP channels no (-1,0,1,2,3) */
/* Dsp_Info *dsp;    Input: Relavent DSP info    */
{

  int status;
  char info_string[INFO_LEN];
  unsigned short zero;

  Dsp_Info *dsp = NULL;


  dsp = board->dsp[*modChan];
  ASSERT(dsp != NULL);

  /* Be paranoid and check if the DSP configuration is downloaded.  If not, do it */

  if ((dsp->proglen)<=0) {
    status = DXP_DSPLOAD;
    sprintf(info_string, "Must Load DSP code before clearing errors");
    dxp_log_error("dxp_clear_error", info_string, status);
    return status;
  }

  zero=0;
  status = dxp_modify_dspsymbol(ioChan, modChan, "RUNERROR", &zero, board);
  if (status!=DXP_SUCCESS)
    dxp_log_error("dxp_clear_error","Unable to clear RUNERROR",status);

  return status;

}

/******************************************************************************
 *
 * Routine that checks the status of a completed calibration run.  This
 * typically depends on symbols in the DSP that were changed by the
 * calibration task.
 *
 ******************************************************************************/
static int dxp_check_calibration(int* calibtest, unsigned short* params, Dsp_Info* dsp)
/* int *calibtest;     Input: Calibration test performed */
/* unsigned short *params;   Input: parameters read from the DSP */
/* Dsp_Info *dsp;     Input: Relavent DSP info    */
{

  int status = DXP_SUCCESS;
  int *itemp;
  unsigned short *stemp;
  Dsp_Info *dtemp;

  /* Assign input parameters to avoid compiler warnings */
  itemp = calibtest;
  stemp = params;
  dtemp = dsp;

  /* No checking is currently performed. */

  return status;
}


/******************************************************************************
 * Routine to get run statistics from the DXP.
 *
 * Returns some run statistics from the parameter block array[]
 *
 ******************************************************************************/
static int dxp_get_runstats(int *ioChan, int *modChan, Board *b,
                            unsigned long *evts, unsigned long *under,
                            unsigned long *over, unsigned long *fast,
                            unsigned long *base, double* live,
                            double* icr, double* ocr)
{
  int status;

  double SYSMICROSEC = 0.0;
  double EVTSINRUN   = 0.0;
  double UNDRFLOWS   = 0.0;
  double OVERFLOWS   = 0.0;
  double FASTPEAKS   = 0.0;
  double BASEEVTS    = 0.0;
  double LIVETIME    = 0.0;
  double REALTIME    = 0.0;

  double real          = 0.0;
  double livetime_tick = 0.0;
  double realtime_tick = 0.0;

  unsigned long total_evts = 0;

  char info_string[INFO_LEN];


  /* We need the clock speed of the board in order to determine the correct
   * clock tick for the livetime/realtime counter.
   */
  status = dxp_read_dspsymbol(ioChan, modChan, "SYSMICROSEC", b, &SYSMICROSEC);

  if (status == DXP_SUCCESS) {
    livetime_tick = 16.0 / (SYSMICROSEC * 1.0e6);
  } else {
    /* The default clock speed is 20MHz. */
    livetime_tick = 16.0 / 20.0e6;
  }

  realtime_tick = livetime_tick;


  status = dxp_read_dspsymbol(ioChan, modChan, "EVTSINRUN", b, &EVTSINRUN);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error reading the number of events in the run for "
            "ioChan = %d", *ioChan);
    dxp_log_error("dxp_get_runstats", info_string, status);
    return status;
  }

  *evts = (unsigned long)EVTSINRUN;


  status = dxp_read_dspsymbol(ioChan, modChan, "UNDRFLOWS", b, &UNDRFLOWS);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error reading the number of underflows for ioChan = %d",
            *ioChan);
    dxp_log_error("dxp_get_runstats", info_string, status);
    return status;
  }

  *under = (unsigned long)UNDRFLOWS;


  status = dxp_read_dspsymbol(ioChan, modChan, "OVERFLOWS", b, &OVERFLOWS);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error reading the number of overflows for ioChan = %d",
            *ioChan);
    dxp_log_error("dxp_get_runstats", info_string, status);
    return status;
  }

  *over = (unsigned long)OVERFLOWS;


  status = dxp_read_dspsymbol(ioChan, modChan, "FASTPEAKS", b, &FASTPEAKS);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error reading the number of fastpeaks for ioChan = %d",
            *ioChan);
    dxp_log_error("dxp_get_runstats", info_string, status);
    return status;
  }

  *fast = (unsigned long)FASTPEAKS;


  status = dxp_read_dspsymbol(ioChan, modChan, "BASEEVTS", b, &BASEEVTS);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error reading the number of baseline events for "
            "ioChan = %d", *ioChan);
    dxp_log_error("dxp_get_runstats", info_string, status);
    return status;
  }

  *base = (unsigned long)BASEEVTS;


  status = dxp_read_dspsymbol(ioChan, modChan, "LIVETIME", b, &LIVETIME);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error reading the livetime for ioChan = %d", *ioChan);
    dxp_log_error("dxp_get_runstats", info_string, status);
    return status;
  }

  *live = LIVETIME * livetime_tick;


  status = dxp_read_dspsymbol(ioChan, modChan, "REALTIME", b, &REALTIME);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error reading the realtime for ioChan = %d", *ioChan);
    dxp_log_error("dxp_get_runstats", info_string, status);
    return status;
  }

  real = REALTIME * realtime_tick;


  if (*live > 0.0) {
    *icr = (*fast) / (*live);
  } else {
    *icr = 0.0;
  }

  total_evts = *evts + *under + *over;

  if (real > 0.0) {
    *ocr = total_evts / real;
  } else {
    *ocr = 0.0;
  }

  return DXP_SUCCESS;
}


/******************************************************************************
 *
 * This routine calculates the new value GAINDAC DAC
 * for a gainchange of (desired gain)/(present gain)
 *
 * assumptions:  The GAINDAC has a linear response in dB
 *
 ******************************************************************************/
static int dxp_perform_gaincalc(float* gainchange, unsigned short* old_gaindac,
                                short* delta_gaindac)
/* float *gainchange;     Input: desired gain change           */
/* unsigned short *old_gaindac;   Input: current gaindac    */
/* short *delta_gaindac;    Input: required gaindac change       */
{

  double gain_db, gain;

  /* Convert the current GAINDAC setting back to dB from bits */

  gain_db = (((double) (*old_gaindac)) / dacpergaindb) - 10.0;

  /* Convert back to pure gain */

  gain = pow(10., gain_db/20.);

  /* Scale by gainchange and convert back to bits */

  gain_db = 20. * log10(gain * (*gainchange));
  *delta_gaindac = (short) ((*old_gaindac) - (unsigned short) ((gain_db + 10.0)*dacpergaindb));

  /* we are done.  leave error checking to the calling routine */

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * This routine changes all DXP channel gains:
 *
 *            initial         final
 *            gain_i -> gain_i*gainchange
 *
 * THRESHOLD is also modified to keep the energy threshold fixed.
 * Note: DACPERADC must be modified -- this can be done via calibration
 *       run or by scaling DACPERADC --> DACPERADC/gainchange.  This routine
 *       does not do either!
 *
 ******************************************************************************/
static int dxp_change_gains(int* ioChan, int* modChan, int* module,
                            float* gainchange, Board* board)
/* int *ioChan;     Input: IO channel of desired channel   */
/* int *modChan;    Input: channel on module      */
/* int *module;     Input: module number: for error reporting */
/* float *gainchange;   Input: desired gain change     */
/* Dsp_Info *dsp;    Input: Relavent DSP info     */
{
  int status;
  char info_string[INFO_LEN];
  unsigned short gaindac, fast_threshold, slow_threshold;
  double dtemp;
  short delta_gaindac;
  unsigned long lgaindac;  /* Use a long to test bounds of new GAINDAC */

  Dsp_Info *dsp = NULL;


  dsp = board->dsp[*modChan];
  ASSERT(dsp != NULL);


  /* Read out the GAINDAC setting from the DSP. */

  status = dxp_read_dspsymbol(ioChan, modChan, "GAINDAC", board, &dtemp);
  if (status != DXP_SUCCESS) {
    sprintf(info_string,
            "Error reading GAINDAC from mod %d chan %d",*module,*modChan);
    dxp_log_error("dxp_change_gains",info_string,status);
    return status;
  }
  gaindac = (unsigned short) dtemp;

  /* Read out the fast peak THRESHOLD setting from the DSP. */

  status = dxp_read_dspsymbol(ioChan, modChan, "THRESHOLD", board, &dtemp);
  if (status != DXP_SUCCESS) {
    sprintf(info_string,
            "Error reading THRESHOLD from mod %d chan %d",*module,*modChan);
    dxp_log_error("dxp_change_gains",info_string,status);
    return status;
  }
  fast_threshold = (unsigned short) dtemp;

  /* Read out the slow peak THRESHOLD setting from the DSP. */

  status = dxp_read_dspsymbol(ioChan, modChan, "SLOWTHRESH", board, &dtemp);
  if (status != DXP_SUCCESS) {
    sprintf(info_string,
            "Error reading SLOWTHRESH from mod %d chan %d",*module,*modChan);
    dxp_log_error("dxp_change_gains",info_string,status);
    return status;
  }
  slow_threshold = (unsigned short) dtemp;

  /* Calculate the delta GAINDAC in bits. */

  status = dxp_perform_gaincalc(gainchange, &gaindac, &delta_gaindac);
  if (status != DXP_SUCCESS) {
    sprintf(info_string,
            "DXP module %d Channel %d", *module, *modChan);
    dxp_log_error("dxp_change_gains",info_string,status);
    status=DXP_SUCCESS;
  }

  /* Now do some bounds checking.  The variable gain amplifier is only capable of
   * -6 to 30 dB. Do this by subracting instead of adding to avoid bounds of 
   * short integers */

  lgaindac = ((unsigned long) gaindac) + delta_gaindac;
  if ((lgaindac<=((GAINDAC_MIN+10.)*dacpergaindb)) ||
      (lgaindac>=((GAINDAC_MAX+10.)*dacpergaindb))) {
    sprintf(info_string,
            "Required GAINDAC setting of %x (bits) that is out of range",
            ((unsigned int) lgaindac));
    status=DXP_DETECTOR_GAIN;
    dxp_log_error("dxp_change_gains",info_string,status);
    return status;
  }

  /* All looks ok, set the new gaindac */

  gaindac = (unsigned short) (gaindac + delta_gaindac);

  /* Change the thresholds as well, to accomadate the new gains.  If the
   * slow or fast thresholds are identically 0, then do not change them. */

  if (fast_threshold!=0)
    fast_threshold = (unsigned short) ((*gainchange)*((double) fast_threshold)+0.5);
  if (slow_threshold!=0)
    slow_threshold = (unsigned short) ((*gainchange)*((double) slow_threshold)+0.5);

  /* Download the GAINDAC, THRESHOLD and SLOWTHRESH back to the DSP. */

  if ((status=dxp_modify_dspsymbol(ioChan, modChan,
                                   "GAINDAC", &gaindac, board))!=DXP_SUCCESS) {
    sprintf(info_string,
            "Error writing GAINDAC to mod %d chan %d", *module, *modChan);
    dxp_log_error("dxp_change_gains",info_string,status);
    return status;
  }
  if ((status=dxp_modify_dspsymbol(ioChan, modChan,
                                   "THRESHOLD", &fast_threshold, board))!=DXP_SUCCESS) {
    sprintf(info_string,
            "Error writing THRESHOLD to mod %d chan %d", *module, *modChan);
    dxp_log_error("dxp_change_gains",info_string,status);
    return status;
  }
  if ((status=dxp_modify_dspsymbol(ioChan, modChan,
                                   "SLOWTHRESH", &slow_threshold, board))!=DXP_SUCCESS) {
    sprintf(info_string,
            "Error writing SLOWTHRESH to mod %d chan %d", *module, *modChan);
    dxp_log_error("dxp_change_gains",info_string,status);
    return status;
  }

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Adjust the DXP channel's gain parameters to achieve the desired ADC rule.
 * The ADC rule is the fraction of the ADC full scale that a single x-ray step
 * of energy *energy contributes.
 *
 * set following, detector specific parameters for all DXP channels:
 *       COARSEGAIN
 *       FINEGAIN
 *       VRYFINGAIN (default:128)
 *       POLARITY
 *       OFFDACVAL
 *
 ******************************************************************************/
static int dxp_setup_asc(int* ioChan, int* modChan, int* module, float* adcRule,
                         float* gainmod, unsigned short* polarity, float* vmin,
                         float* vmax, float* vstep, Board* board)
/* int *ioChan;     Input: IO channel number     */
/* int *modChan;    Input: Module channel number    */
/* int *module;     Input: Module number: error reporting */
/* float *adcRule;    Input: desired ADC rule     */
/* float *gainmod;    Input: desired finescale gain adjustment */
/* unsigned short *polarity; Input: polarity of channel    */
/* float *vmin;     Input: minimum voltage range of channel */
/* float *vmax;     Input: maximum voltage range of channel */
/* float *vstep;    Input: average step size of single pulse */
/* Dsp_Info *dsp;    Input: Relavent DSP info     */
{
  char info_string[INFO_LEN];
  double gain;
  int status;
  unsigned short gaindac;

  double g_input = GINPUT;     /* Input attenuator Gain */
  double g_input_buff = GINPUT_BUFF;   /* Input buffer Gain  */
  double g_inverting_amp = GINVERTING_AMP; /* Inverting Amp Gain  */
  double g_v_divider = GV_DIVIDER;   /* V Divider after Inv AMP */
  double g_gaindac_buff = GGAINDAC_BUFF;  /* GainDAC buffer Gain  */
  double g_nyquist = GNYQUIST;    /* Nyquist Filter Gain  */
  double g_adc_buff = GADC_BUFF;    /* ADC buffer Gain   */
  double g_adc = GADC;      /* ADC Input Gain   */
  double g_system = 0.;      /* Total gain imposed by ASC
                     not including the GAINDAC*/
  double g_desired = 0.;      /* Desired gain for the
                     GAINDAC */
  float *ftemp;

  Dsp_Info *dsp = NULL;


  dsp = board->dsp[*modChan];
  ASSERT(dsp != NULL);

  /* Assign input parameters to avoid compiler warnings */
  ftemp = vmax;
  ftemp = vmin;
  ftemp = gainmod;


  /* First perform some initialization.  This is redundant for now, but eventually
   * these parameters will be determined by calibration runs on the board */

  /* The number of DAC bits per unit gain in dB */
  dacpergaindb = ((1<<GAINDAC_BITS)-1) / 40.;
  /* The number of DAC bits per unit gain (linear) : 40(dB)=20log10(g) => g=100.*/
  dacpergain   = ((1<<GAINDAC_BITS)-1) / 100.;

  /*
   *   Following is required gain to achieve desired eV/ADC.  Note: ADC full
   *      scale = ADC_RANGE mV
   */
  gain = (*adcRule*((float) ADC_RANGE)) / *vstep;
  /*
   * Define the total system gain.  This is the gain without including the
   * GAINDAC user controlled gain.  Any desired gain should be adjusted by
   * this system gain before determining the proper GAINDAC setting.
   */
  g_system = g_input * g_input_buff * g_inverting_amp * g_v_divider *
             g_gaindac_buff * g_nyquist * g_adc_buff * g_adc;

  /* Now the desired GAINDAC gain is just the desired user gain
   * divided by g_system */

  g_desired = gain / g_system;

  /* Now convert this gain to dB since the GAINDAC variable amplifier is linear
   * in dB. */

  g_desired = 20.0 * log10(g_desired);

  /* Now do some bounds checking.  The variable gain amplifier is only capable of
   * -6 to 30 dB. */

  if ((g_desired<=GAINDAC_MIN)||(g_desired>=GAINDAC_MAX)) {
    sprintf(info_string,
            "Required gain of %f (dB) that is out of range for the ASC", g_desired);
    status=DXP_DETECTOR_GAIN;
    dxp_log_error("dxp_setup_asc",info_string,status);
    return status;
  }

  /* Gain is within limits.  Determine the setting for the 16-bit DAC */

  gaindac = (short) ((g_desired + 10.0) * dacpergaindb);

  /* Write the GAINDAC data back to the DXP module. */

  if ((status=dxp_modify_dspsymbol(ioChan, modChan,
                                   "GAINDAC", &gaindac, board))!=DXP_SUCCESS) {
    sprintf(info_string,
            "Error writting GAINDAC to module %d channel %d",
            *module,*modChan);
    dxp_log_error("dxp_setup_asc",info_string,status);
    return status;
  }
  /*
   *    Download POLARITY
   */
  if ((status=dxp_modify_dspsymbol(ioChan, modChan,
                                   "POLARITY", polarity, board))!=DXP_SUCCESS) {
    sprintf(info_string,
            "Error writting POLARITY to module %d channel %d",
            *module,*modChan);
    dxp_log_error("dxp_setup_asc",info_string,status);
    return status;
  }

  return status;
}

/******************************************************************************
 *
 * Perform the neccessary calibration runs to get the ASC ready to take data
 *
 ******************************************************************************/
static int dxp_calibrate_asc(int* mod, int* camChan, unsigned short* used,
                             Board *board)
/* int *mod;     Input: Camac Module number to calibrate   */
/* int *camChan;    Input: Camac pointer       */
/* unsigned short *used;  Input: bitmask of channel numbers to calibrate */
/* Board *board;    Input: Relavent Board info      */
{
  int *itemp;
  unsigned short *stemp;
  Board *btemp;

  /* Assign input parameters to avoid compiler warnings */
  itemp = mod;
  itemp = camChan;
  stemp = used;
  btemp = board;

  /* No Calibration runs are performed for the DXP-4C 2X */

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Preform internal gain calibration or internal TRACKDAC reset point
 * for all DXP channels:
 *    o save the current value of RUNTASKS for each channel
 *    o start run
 *    o wait
 *    o stop run
 *    o readout parameter memory
 *    o check for errors, clear errors if set
 *    o check calibration results..
 *    o restore RUNTASKS for each channel
 *
 ******************************************************************************/
static int dxp_calibrate_channel(int* mod, int* camChan, unsigned short* used,
                                 int* calibtask, Board *board)
/* int *mod;      Input: Camac Module number to calibrate   */
/* int *camChan;     Input: Camac pointer       */
/* unsigned short *used;   Input: bitmask of channel numbers to calibrate */
/* int *calibtask;     Input: which calibration function    */
/* Board *board;     Input: Relavent Board info      */
{
  int status=DXP_SUCCESS,status2,chan;
  char info_string[INFO_LEN];
  unsigned short runtasks;
  float one_second=1.0;
  unsigned short addr_RUNTASKS=USHRT_MAX;
  unsigned short runerror,errinfo;

  unsigned short params[MAXSYM];
  double dtemp;

  /*
   *  Loop over each channel and save the RUNTASKS word.  Then start the
   *  calibration run
   */
  for (chan=0;chan<4;chan++) {
    if (((*used)&(1<<chan))==0) continue;

    /* Grab the addresses in the DSP for some symbols. */

    status = dxp_loc("RUNTASKS", board->dsp[chan], &addr_RUNTASKS);
    if (status!=DXP_SUCCESS) {
      status = DXP_NOSYMBOL;
      dxp_log_error("dxp_calibrate_channel","Unable to locate RUNTASKS",status);
      return status;
    }

    status2 = dxp_read_dspsymbol(camChan, &chan, "RUNTASKS", board, &dtemp);
    if (status2 != DXP_SUCCESS) {
      sprintf(info_string,"Error reading RUNTASKS from mod %d chan %d",
              *mod,chan);
      dxp_log_error("dxp_calibrate_channel",info_string,status2);
      return status2;
    }
    runtasks = (unsigned short) dtemp;

    status2 = dxp_begin_calibrate(camChan, &chan, calibtask, board);
    if (status2 != DXP_SUCCESS) {
      sprintf(info_string,"Error beginning calibration for mod %d", *mod);
      dxp_log_error("dxp_calibrate_channel", info_string, status2);
      return status2;
    }
    /*
     *   wait a second, then stop the runs
     */
    x10p_md_wait(&one_second);
    status2 = dxp_end_run(camChan, &chan, board);
    if (status2 != DXP_SUCCESS) {
      dxp_log_error("dxp_calibrate_channel",
                    "Unable to end calibration run",status2);
      return status2;
    }
    /*
     *  Loop over each of the channels and read out the parameter memory. Check
     *  to see if there were errors.  Finally, restore the original value of
     *  RUNTASKS
     */

    /* Read out the parameter memory into the Params array */

    status2 = dxp_read_dspparams(camChan, &chan, board, params);
    if (status2 != DXP_SUCCESS) {
      sprintf(info_string,
              "error reading parameters for mod %d chan %d",*mod,chan);
      dxp_log_error("dxp_calibrate_channel",info_string,status2);
      return status2;
    }

    /* Check for errors reported by the DSP. */

    status2 = dxp_decode_error(params, board->dsp[chan], &runerror, &errinfo);
    if (status2 != DXP_SUCCESS) {
      dxp_log_error("dxp_calibrate_channel","Unable to decode errors",status2);
      return status2;
    }
    if (runerror != 0) {
      status += DXP_DSPRUNERROR;
      sprintf(info_string,"DSP error detected for mod %d chan %d", *mod, chan);
      dxp_log_error("dxp_calibrate_channel", info_string, status);
      status2 = dxp_clear_error(camChan, &chan, board);
      if (status2 != DXP_SUCCESS) {
        sprintf(info_string,
                "Unable to clear error for mod %d chan %d",*mod,chan);
        dxp_log_error("dxp_calibrate_channel",info_string,status2);
        return status2;
      }
    }

    /* Call the primitive routine that checks the calibration to ensure
     * that all went well.  The results depend on the calibration performed. */

    status += dxp_check_calibration(calibtask, params, board->dsp[chan]);
    if (status != DXP_SUCCESS) {
      sprintf(info_string,"Calibration Error: mod %d chan %d",
              *mod,chan);
      dxp_log_error("dxp_calibrate_channel",info_string,status);
    }


    /* Now write back the value previously stored in RUNTASKS */

    status2 = dxp_modify_dspsymbol(camChan, &chan, "RUNTASKS", &runtasks,
                                   board);
    if (status2 != DXP_SUCCESS) {
      sprintf(info_string,"Error writing RUNTASKS to mod %d chan %d",
              *mod,chan);
      dxp_log_error("dxp_calibrate_channel",info_string,status2);
      return status2;
    }
  }

  return status;
}

/********------******------*****------******-------******-------******------*****
 * Now begins the section with utility routines to handle some of the drudgery
 * associated with different operating systems and such.
 *
 ********------******------*****------******-------******-------******------*****/

/******************************************************************************
 *
 * Routine to open a new file.
 * Try to open the file directly first.
 * Then try to open the file in the directory pointed to
 *     by XIAHOME.
 * Finally try to open the file as an environment variable.
 *
 ******************************************************************************/
static FILE *dxp_find_file(const char* filename, const char* mode)
/* const char *filename;   Input: filename to open   */
/* const char *mode;    Input: Mode to use when opening */
{
  FILE *fp=NULL;
  char *name=NULL, *name2=NULL;
  char *home=NULL;

  /* Try to open file directly */
  if ((fp=fopen(filename,mode))!=NULL) {
    return fp;
  }
  /* Try to open the file with the path XIAHOME */
  if ((home=getenv("XIAHOME"))!=NULL) {
    name = (char *) x10p_md_alloc(sizeof(char)*
                                  (strlen(home)+strlen(filename)+2));
    sprintf(name, "%s/%s", home, filename);
    if ((fp=fopen(name,mode))!=NULL) {
      x10p_md_free(name);
      return fp;
    }
    x10p_md_free(name);
    name = NULL;
  }
  /* Try to open the file with the path DXPHOME */
  if ((home=getenv("DXPHOME"))!=NULL) {
    name = (char *) x10p_md_alloc(sizeof(char)*
                                  (strlen(home)+strlen(filename)+2));
    sprintf(name, "%s/%s", home, filename);
    if ((fp=fopen(name,mode))!=NULL) {
      x10p_md_free(name);
      return fp;
    }
    x10p_md_free(name);
    name = NULL;
  }
  /* Try to open the file as an environment variable */
  if ((name=getenv(filename))!=NULL) {
    if ((fp=fopen(name,mode))!=NULL) {
      return fp;
    }
    name = NULL;
  }
  /* Try to open the file with the path XIAHOME and pointing
   * to a file as an environment variable */
  if ((home=getenv("XIAHOME"))!=NULL) {
    if ((name2=getenv(filename))!=NULL) {

      name = (char *) x10p_md_alloc(sizeof(char)*
                                    (strlen(home)+strlen(name2)+2));
      sprintf(name, "%s/%s", home, name2);
      if ((fp=fopen(name,mode))!=NULL) {
        x10p_md_free(name);
        return fp;
      }
      x10p_md_free(name);
      name = NULL;
    }
  }
  /* Try to open the file with the path DXPHOME and pointing
   * to a file as an environment variable */
  if ((home=getenv("DXPHOME"))!=NULL) {
    if ((name2=getenv(filename))!=NULL) {

      name = (char *) x10p_md_alloc(sizeof(char)*
                                    (strlen(home)+strlen(name2)+2));
      sprintf(name, "%s/%s", home, name2);
      if ((fp=fopen(name,mode))!=NULL) {
        x10p_md_free(name);
        name = NULL;
        return fp;
      }
      x10p_md_free(name);
      name = NULL;
    }
  }

  return NULL;
}


/**********
 * This routine does nothing for this product.
 **********/
static int dxp_setup_cmd(Board *board, char *name, unsigned int *lenS,
                         byte_t *send, unsigned int *lenR, byte_t *receive,
                         byte_t ioFlags)
{
  UNUSED(board);
  UNUSED(name);
  UNUSED(lenS);
  UNUSED(send);
  UNUSED(lenR);
  UNUSED(receive);
  UNUSED(ioFlags);

  return DXP_SUCCESS;
}


/**********
 * Reads the memory of type 'name' starting at 'base' and going
 * until 'base' + 'offset'.
 **********/
XERXES_STATIC int dxp_read_mem(int *ioChan, int *modChan, Board *board,
                               char *name, unsigned long *base, unsigned long *offset,
                               unsigned long *data)
{
  int status;

  size_t i;

  char info_string[INFO_LEN];


  ASSERT(ioChan != NULL);
  ASSERT(modChan != NULL);
  ASSERT(board != NULL);
  ASSERT(name != NULL);
  ASSERT(base != NULL);
  ASSERT(offset != NULL);
  ASSERT(data != NULL);


  for (i = 0; i < NUM_MEM_READERS; i++) {
    if (STREQ(name, mem_readers[i].name)) {
      status = mem_readers[i].f(ioChan, modChan, board, *base, *offset, data);

      if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error reading '%s' memory", name);
        dxp_log_error("dxp_read_mem", info_string, status);
        return status;
      }

      return DXP_SUCCESS;
    }
  }

  sprintf(info_string, "Unknown memory type to read: '%s'", name);
  dxp_log_error("dxp_read_mem", info_string, DXP_UNKNOWN_MEM);
  return DXP_UNKNOWN_MEM;
}


/** @brief Writes memory of the specified type to the specified location
 *
 */
XERXES_STATIC int dxp_write_mem(int *ioChan, int *modChan, Board *board,
                                char *name, unsigned long *base, unsigned long *offset,
                                unsigned long *data)
{
  int status;

  size_t i;

  char info_string[INFO_LEN];


  ASSERT(ioChan != NULL);
  ASSERT(modChan != NULL);
  ASSERT(board != NULL);
  ASSERT(name != NULL);
  ASSERT(base != NULL);
  ASSERT(offset != NULL);
  ASSERT(data != NULL);


  for (i = 0; i < NUM_MEM_WRITERS; i++) {
    if (STREQ(name, mem_writers[i].name)) {
      status = mem_writers[i].f(ioChan, modChan, board, *base, *offset, data);

      if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error writing '%s' memory", name);
        dxp_log_error("dxp_write_mem", info_string, status);
        return status;
      }

      return DXP_SUCCESS;
    }
  }

  sprintf(info_string, "Unknown memory type to write: '%s'", name);
  dxp_log_error("dxp_write_mem", info_string, DXP_UNKNOWN_MEM);
  return DXP_UNKNOWN_MEM;
}


/**********
 * This routine does nothing currently.
 **********/
static int dxp_write_reg(int *ioChan, int *modChan, char *name,
                         unsigned long *data)
{
  UNUSED(ioChan);
  UNUSED(modChan);
  UNUSED(name);
  UNUSED(data);

  return DXP_SUCCESS;
}


/**********
 * This routine currently does nothing.
 **********/
XERXES_STATIC int dxp_read_reg(int *ioChan, int *modChan, char *name,
                               unsigned long *data)
{
  UNUSED(ioChan);
  UNUSED(modChan);
  UNUSED(name);
  UNUSED(data);

  return DXP_SUCCESS;
}


/**********
 * This routine does nothing currently.
 **********/
XERXES_STATIC int XERXES_API dxp_do_cmd(int *ioChan, byte_t cmd, unsigned int lenS,
                                        byte_t *send, unsigned int lenR, byte_t *receive)
{
  UNUSED(ioChan);
  UNUSED(cmd);
  UNUSED(lenS);
  UNUSED(send);
  UNUSED(lenR);
  UNUSED(receive);

  return DXP_SUCCESS;
}


/**********
 * Calls the interface close routine.
 **********/
XERXES_STATIC int XERXES_API dxp_unhook(Board *board)
{
  int status;


  status = board->iface->funcs->dxp_md_close(&(board->ioChan));

  /* Ignore the status due to some issues involving
   * repetitive function calls.
   */

  return DXP_SUCCESS;
}


/** @brief Get the length of the SCA data buffer
 *
 */
XERXES_STATIC unsigned int dxp_get_sca_length(Dsp_Info *dsp, unsigned short *params)
{
  int status;

  unsigned short addr;


  ASSERT(dsp != NULL);
  ASSERT(params != NULL);


  status = dxp_loc("SCADLEN", dsp, &addr);

  if (status != DXP_SUCCESS) {
    dxp_log_error("dxp_get_sca_length", "Unable to find SCADLEN symbol", status);
    return 0;
  }

  /* The user is expecting to create an array of longs with one element per SCA, whereas
   * SCADLEN reports back the total number of words in the buffer,
   * which is twice the number of SCAs on most architectures.
   */
  return ((unsigned int)(params[addr] / 2));
}


/** @brief Read out the SCA data buffer from the board
 *
 */
XERXES_STATIC int XERXES_API dxp_read_sca(int *ioChan, int *modChan, Board* board, unsigned long *sca)
{
  int status;

  unsigned long n_bytes = 0;

  unsigned short idx   = 0;
  unsigned short start = 0;

  unsigned int i;
  unsigned int len = 0;

  char info_string[INFO_LEN];

  unsigned short *sca_short = NULL;


  ASSERT(ioChan != NULL);
  ASSERT(modChan != NULL);
  ASSERT(board != NULL);
  ASSERT(sca != NULL);


  status = dxp_loc("SCADSTART", board->dsp[*modChan], &idx);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Unable to locate SCADSTART for ioChan '%d', modChan '%d'",
            *ioChan, *modChan);
    dxp_log_error("dxp_read_sca", info_string, status);
    return status;
  }

  start = (unsigned short)(board->params[*modChan][idx] + DATA_BASE);

  status = dxp_loc("SCADLEN", board->dsp[*modChan], &idx);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Unable to locate SCADLEN for ioChan '%d', modChan '%d'",
            *ioChan, *modChan);
    dxp_log_error("dxp_read_sca", info_string, status);
    return status;
  }

  len = (unsigned int)board->params[*modChan][idx];

  n_bytes = len * sizeof(unsigned short);
  sca_short = (unsigned short *)x10p_md_alloc(n_bytes);

  if (sca_short == NULL) {
    sprintf(info_string, "Error allocating %lu bytes for 'sca_short'", n_bytes);
    dxp_log_error("dxp_read_sca", info_string, DXP_NOMEM);
    return DXP_NOMEM;
  }

  status = dxp_read_block(ioChan, modChan, &start, &len, sca_short);

  if (status != DXP_SUCCESS) {
    x10p_md_free(sca_short);
    dxp_log_error("dxp_read_sca", "Error reading SCA data buffer", status);
    return status;
  }

  for (i = 0; i < (unsigned int)(len / 2); i++) {
    sca[i] = (unsigned long)(((unsigned long)sca_short[(i * 2) + 1] << 16) | sca_short[i * 2]);
  }

  x10p_md_free(sca_short);

  return DXP_SUCCESS;
}


/** @brief Reads the external memory from the board.
 *
 * This routine is essentially a wrapper around the previously implemented
 * behavior in dxp_begin_control_task(), etc.
 */
XERXES_STATIC int dxp_read_external_memory(int *ioChan, int *modChan, Board *board,
                                           unsigned long base, unsigned long offset, unsigned long *data)
{
  int status;
  int timeout       = 1000;
  int mem_buf_len   = 0;
  int requested_len = (int)offset;
  int current_len   = 0;

  unsigned int info_len = 4;

  short task = CT_DXPX10P_READ_MEMORY;

  unsigned long i;

  float wait = 0.0;
  float poll = 0.0;

  double BUSY = 0.0;

  unsigned long *buf = NULL;

  int info[4];

  char info_string[INFO_LEN];


  ASSERT(ioChan != NULL);
  ASSERT(modChan != NULL);
  ASSERT(board != NULL);
  ASSERT(data != NULL);


  status = dxp_control_task_params(ioChan, modChan, &task, board, info);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error getting info for reading the external memory: ioChan = %d, modChan = %d",
            *ioChan, *modChan);
    dxp_log_error("dxp_read_external_memory", info_string, status);
    return status;
  }

  mem_buf_len = info[0];
  /*  wait = (float)info[1]; */
  wait = .001f;
  poll = (float)info[2];

  for (i = 0; requested_len > 0; requested_len -= mem_buf_len, i++) {

    if (i == 0) {
      info[1] = (int)((base >> 14) & 0xFF);
      info[2] = (int)(base & 0x3FFF);
    } else {
      info[1] = -1;
      info[2] = -1;
    }

    if (requested_len > mem_buf_len) {
      current_len = mem_buf_len;
    } else {
      current_len = requested_len;
    }

    sprintf(info_string, "current_len = %d\n", current_len);
    dxp_log_debug("dxp_read_external_memory", info_string);

    info[3] = current_len;

    status = dxp_begin_control_task(ioChan, modChan, &task, &info_len, info, board);

    if (status != DXP_SUCCESS) {
      sprintf(info_string, "Error starting control task: ioChan = %d, modChan = %d",
              *ioChan, *modChan);
      dxp_log_error("dxp_read_external_memory", info_string, status);
      return status;
    }

    status = dxp_wait_for_busy(ioChan, modChan, board, timeout, BUSY, wait);

    if (status != DXP_SUCCESS) {
      sprintf(info_string, "Error waiting for BUSY: ioChan = %d, modChan %d",
              *ioChan, *modChan);
      dxp_log_error("dxp_read_external_memory", info_string, status);
      return status;
    }

    buf = (unsigned long *)x10p_md_alloc(current_len * sizeof(unsigned long));

    if (buf == NULL) {
      sprintf(info_string, "Out-of-memory allocating %d bytes for 'buf'",
              current_len * sizeof(unsigned long));
      dxp_log_error("dxp_read_external_memory", info_string, DXP_NOMEM);
      return DXP_NOMEM;
    }

    status = dxp_control_task_data(ioChan, modChan, &task, board, (void *)buf);

    if (status != DXP_SUCCESS) {
      x10p_md_free(buf);
      sprintf(info_string, "Error getting external memory data: ioChan = %d, modChan = %d",
              *ioChan, *modChan);
      dxp_log_error("dxp_read_external_memory", info_string, status);
      return status;
    }

    memcpy(data + (i * mem_buf_len), buf, current_len * sizeof(unsigned long));

    x10p_md_free(buf);
    buf = NULL;

    status = dxp_end_control_task(ioChan, modChan, board);

    if (status != DXP_SUCCESS) {
      sprintf(info_string, "Error stopping control task: ioChan = %d, modChan = %d",
              *ioChan, *modChan);
      dxp_log_error("dxp_read_external_memory", info_string, status);
      return status;
    }

  }

  return DXP_SUCCESS;
}


/** @brief Writes to the external memory on the board.
 *
 * Writes to the hardware's external memory. If no external memory is present,
 * or the loaded DSP code doesn't support external memory, an error should
 * be returned by one of the routines called by dxp_write_external_memory().
 *
 * This routine only allows transfers up to the length of the history buffer. Higher
 * level code should wrapper around this routine (@sa dxp_write_mem, @sa dxp_write_memory)
 * to perform transfers greater than the history buffer length.
 */
XERXES_STATIC int dxp_write_external_memory(int *ioChan, int *modChan, Board *board,
                                            unsigned long base, unsigned long offset, unsigned long *data)
{
  int status;
  int timeout       = 1000;
  int mem_buf_len   = 0;
  int requested_len = (int)offset;
  int current_len   = 0;
  int i;
  int j;

  unsigned int info_len;

  short task = CT_DXPX10P_WRITE_MEMORY;

  float wait = 0.0;
  float poll = 0.0;

  double BUSY = 0.0;

  int task_info[4];

  char info_string[INFO_LEN];

  int *info = NULL;


  ASSERT(ioChan != NULL);
  ASSERT(modChan != NULL);
  ASSERT(board != NULL);
  ASSERT(data != NULL);


  status = dxp_control_task_params(ioChan, modChan, &task, board, task_info);

  if (status != DXP_SUCCESS) {
    sprintf(info_string, "Error getting info for writing the external memory: ioChan = %d, modChan = %d",
            *ioChan, *modChan);
    dxp_log_error("dxp_write_external_memory", info_string, status);
    return status;
  }


  mem_buf_len = task_info[0];
  /*  wait = (float)task_info[1]; */
  wait = .001f;
  poll = (float)task_info[2];

  for (i = 0; requested_len > 0; requested_len -= mem_buf_len, i++) {

    if (requested_len > mem_buf_len) {
      current_len = mem_buf_len;
    } else {
      current_len = requested_len;
    }

    info_len = current_len + 4;
    info = (int *)x10p_md_alloc(info_len * sizeof(int));

    if (info == NULL) {
      sprintf(info_string, "Out-of-memory allocating %d bytes for 'info'",
              info_len * sizeof(int));
      dxp_log_error("dxp_write_external_memory", info_string, DXP_NOMEM);
      return DXP_NOMEM;
    }

    if (i == 0) {
      info[1] = (int)((base >> 14) & 0xFF);
      info[2] = (int)(base & 0x3FFF);
    } else {
      info[1] = -1;
      info[2] = -1;
    }

    sprintf(info_string, "current_len = %d\n", current_len);
    dxp_log_debug("dxp_write_external_memory", info_string);

    info[3] = current_len;

    for (j = 0; j < current_len; j++) {
      info[j + 4] = (int)data[(i * mem_buf_len) + j];
    }

    status = dxp_begin_control_task(ioChan, modChan, &task, &info_len, info, board);

    if (status != DXP_SUCCESS) {
      x10p_md_free(info);
      sprintf(info_string, "Error starting control task: ioChan = %d, modChan = %d",
              *ioChan, *modChan);
      dxp_log_error("dxp_write_external_memory", info_string, status);
      return status;
    }

    status = dxp_wait_for_busy(ioChan, modChan, board, timeout, BUSY, wait);

    if (status != DXP_SUCCESS) {
      x10p_md_free(info);
      sprintf(info_string, "Error waiting for BUSY: ioChan = %d, modChan %d",
              *ioChan, *modChan);
      dxp_log_error("dxp_write_external_memory", info_string, status);
      return status;
    }

    status = dxp_end_control_task(ioChan, modChan, board);

    if (status != DXP_SUCCESS) {
      x10p_md_free(info);
      sprintf(info_string, "Error stopping control task: ioChan = %d, modChan = %d",
              *ioChan, *modChan);
      dxp_log_error("dxp_write_external_memory", info_string, status);
      return status;
    }

    x10p_md_free(info);
    info = NULL;
  }

  return DXP_SUCCESS;
}


/** @brief Wait for BUSY to go to a certain value within the number of iterations.
 *
 */
XERXES_STATIC int dxp_wait_for_busy(int *ioChan, int *modChan, Board *board,
                                    int n_timeout, double busy, float wait)
{
  int status;
  int i;

  double BUSY = 0.0;

  char info_string[INFO_LEN];


  ASSERT(ioChan != NULL);
  ASSERT(modChan != NULL);
  ASSERT(board != NULL);


  for (i = 0; i < n_timeout; i++) {
    status = dxp_read_dspsymbol(ioChan, modChan, "BUSY", board, &BUSY);

    if (status != DXP_SUCCESS) {
      sprintf(info_string, "Error reading BUSY: ioChan = %d, modChan = %d",
              *ioChan, *modChan);
      dxp_log_error("dxp_wait_for_busy", info_string, status);
      return status;
    }

    if (BUSY == 0.0) {
      break;
    }

    x10p_md_wait(&wait);
  }

  if (i == n_timeout) {
    sprintf(info_string, "Timeout (%d iterations, %f s per iteration) waiting "
            "for BUSY to go to %u (current = %0.0f)",
            n_timeout, wait, (unsigned short)busy, BUSY);
    dxp_log_error("dxp_wait_for_busy", info_string, DXP_TIMEOUT);
    return DXP_TIMEOUT;
  }

  return DXP_SUCCESS;
}


/** @brief Retrieves the symbol name located at the specified index in the
 * DSP parameter memory.
 *
 */
XERXES_STATIC int dxp_get_symbol_by_index(int modChan, unsigned short index,
                                          Board *b, char *name)
{
  Dsp_Info *dsp = NULL;


  ASSERT(b != NULL);
  ASSERT(name != NULL);


  dsp = b->dsp[modChan];
  ASSERT(dsp != NULL);
  ASSERT(index < dsp->params->nsymbol);


  strncpy(name, dsp->params->parameters[index].pname, MAXSYMBOL_LEN);

  return DXP_SUCCESS;
}


/** @brief Gets the total number of DSP parameters for the specified
 * module channel.
 *
 */
XERXES_STATIC int dxp_get_num_params(int modChan, Board *b,
                                     unsigned short *n_params)
{
  Dsp_Info *dsp = NULL;


  ASSERT(b != NULL);
  ASSERT(n_params != NULL);


  dsp = b->dsp[modChan];
  ASSERT(dsp != NULL);

  *n_params = dsp->params->nsymbol;

  return DXP_SUCCESS;
}


/**
 * @brief Read the spectrum directly, bypassing the checks in
 * dxp_read_spectrum().
 */
XERXES_STATIC int dxp_read_spectrum_memory(int *ioChan, int *modChan,
                                           Board *board, unsigned long base,
                                           unsigned long offset,
                                           unsigned long *data)
{
  int status;

  unsigned long i;

  unsigned int n_mca_words = (unsigned int)(offset * 2);

  char info_string[INFO_LEN];

  unsigned short *mca = NULL;

  unsigned short addr = (unsigned short)base;

  UNUSED(board);


  mca = x10p_md_alloc(n_mca_words * sizeof(unsigned short));

  if (mca == NULL) {
    sprintf(info_string, "Unable to allocated %d bytes for 'mca'.",
            n_mca_words * sizeof(unsigned short));
    dxp_log_error("dxp_read_spectrum_memory", info_string, DXP_NOMEM);
    return DXP_NOMEM;
  }

  status = dxp_read_block(ioChan, modChan, &addr, &n_mca_words, mca);

  if (status != DXP_SUCCESS) {
    x10p_md_free(mca);
    sprintf(info_string, "Error reading spectrum memory block: %#lx - %#lx for "
            "ioChan = %d", base, base + offset, *ioChan);
    dxp_log_error("dxp_read_spectrum_memory", info_string, status);
    return status;
  }

  for (i = 0; i < offset; i++) {
    data[i] = (unsigned long)((mca[(2 * i) + 1] << 16) | mca[2 * i]);
  }

  x10p_md_free(mca);

  return DXP_SUCCESS;
}
