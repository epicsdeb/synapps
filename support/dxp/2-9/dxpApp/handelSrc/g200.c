/*
 * g200.c
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
 *    Created		29-Jan-01 JW Copied to X10P.c from dxp4c2x.c 
 *    Created		12-July-01 JW Copied to g200.c from X10P.c 
 *         
 *
 * Copyright (c) 2002, X-ray Instrumentation Associates
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
#include <md_generic.h>
#include <xerxes_structures.h>
#include <xia_xerxes_structures.h>
#include <g200.h>
#include <xerxes_errors.h>
#include <xia_g200.h>

#include "xia_assert.h"

/* Define the length of the error reporting string info_string */
#define INFO_LEN 400
/* Define the length of the line string used to read in files */
#define LINE_LEN 132

/* Starting memory location and length for DSP parameter memory */
static unsigned short startp=START_PARAMS;
/*static unsigned int lenp=MAXSYM;*/
/* Variable to store the number of DAC counts per unit gain (dB)	*/
static double dacpergaindb;
/* Variable to store the number of DAC counts per unit gain (linear) */
static double dacpergain;
/* 
 * Store pointers to the proper DLL routines to talk to the CAMAC crate 
 */
static DXP_MD_IO g200_md_io;
static DXP_MD_SET_MAXBLK g200_md_set_maxblk;
static DXP_MD_GET_MAXBLK g200_md_get_maxblk;
/* 
 * Define the utility routines used throughout this library
 */
static DXP_MD_LOG g200_md_log;
static DXP_MD_ALLOC g200_md_alloc;
static DXP_MD_FREE g200_md_free;
static DXP_MD_PUTS g200_md_puts;
static DXP_MD_WAIT g200_md_wait;

/******************************************************************************
 *
 * Routine to create pointers to all the internal routines
 * 
 ******************************************************************************/
int dxp_init_dgfg200(Functions* funcs)
/* Functions *funcs;				*/
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
	
	return DXP_SUCCESS;
}
/******************************************************************************
 *
 * Routine to initialize the IO Driver library, get the proper pointers
 * 
 ******************************************************************************/
static int dxp_init_driver(Interface* iface) 
/* Interface *iface;				Input: Pointer to the IO Interface		*/
{

/* Assign all the static vars here to point at the proper library routines */
	g200_md_io         = iface->funcs->dxp_md_io;
	g200_md_set_maxblk = iface->funcs->dxp_md_set_maxblk;
	g200_md_get_maxblk = iface->funcs->dxp_md_get_maxblk;

	return DXP_SUCCESS;
}
/******************************************************************************
 *
 * Routine to initialize the Utility routines, get the proper pointers
 * 
 ******************************************************************************/
static int dxp_init_utils(Utils* utils) 
/* Utils *utils;					Input: Pointer to the utility functions	*/
{

/* Assign all the static vars here to point at the proper library routines */
	g200_md_log   = utils->funcs->dxp_md_log;

#ifdef XIA_SPECIAL_MEM
	g200_md_alloc = utils->funcs->dxp_md_alloc;
	g200_md_free  = utils->funcs->dxp_md_free;
#else
	g200_md_alloc = malloc;
	g200_md_free  = free;
#endif /* XIA_SPECIAL_MEM */

	g200_md_wait  = utils->funcs->dxp_md_wait;
	g200_md_puts  = utils->funcs->dxp_md_puts;

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
/* int *ioChan;						Input: I/O channel of DXP module      */
/* unsigned short *addr;			Input: address to write into the TSAR */
{
	unsigned int f, a, len;
	int status;

	f=G200_TSAR_F_WRITE;
	a=G200_TSAR_A_WRITE;
	len=0;
	status=g200_md_io(ioChan,&f,&a,addr,&len);					/* write TSAR */
	if (status!=DXP_SUCCESS){
		status = DXP_WRITE_TSAR;
		dxp_log_error("dxp_write_tsar","Error writing TSAR",status);
	}
	return status;
}

/******************************************************************************
 * Routine to write to the CSR (Control/Status Register)
 * 
 * This register contains bits which both control the operation of the 
 * DXP and report the status of the G200.
 *
 ******************************************************************************/
static int dxp_write_csr(int* ioChan, unsigned short* data)
/* int *ioChan;						Input: I/O channel of G200 module      */
/* unsigned short *data;			Input: address of data to write to CSR*/
{
	unsigned int f, a, len;
	int status;
	unsigned short saddr;

/* write transfer start address register */
	
	saddr = G200_CSR_ADDRESS;
	status = dxp_write_tsar(ioChan, &saddr);
	if (status!=DXP_SUCCESS){
		dxp_log_error("dxp_write_csr","Error writing TSAR",status);
		return status;
	}

/* Write the CSR data */

	f=G200_CSR_F_WRITE;
	a=G200_CSR_A_WRITE;
	len=1;
	status=g200_md_io(ioChan,&f,&a,data,&len);					/* write CSR */
	if (status!=DXP_SUCCESS){
		status = DXP_WRITE_CSR;
		dxp_log_error("dxp_write_csr","Error writing CSR",status);
	}
	return status;
}

/******************************************************************************
 * Routine to read from the CSR (Control/Status Register)
 * 
 * This register contains bits which both control the operation of the 
 * G200 and report the status of the G200.
 *
 ******************************************************************************/
static int dxp_read_csr(int* ioChan, unsigned short* data)
/* int *ioChan;						Input: I/O channel of G200 module   */
/* unsigned short *data;			Output: where to put data from CSR */
{
	unsigned int f, a, len;
	int status;
	unsigned short saddr;

/* write transfer start address register */
	
	saddr = G200_CSR_ADDRESS;
	status = dxp_write_tsar(ioChan, &saddr);
	if (status!=DXP_SUCCESS){
		dxp_log_error("dxp_read_csr","Error writing TSAR",status);
		return status;
	}

/* Read the CSR data */

	f=G200_CSR_F_READ;
	a=G200_CSR_A_READ;
	len=1;
	status=g200_md_io(ioChan,&f,&a,data,&len);					/* write TSAR */
	if (status!=DXP_SUCCESS){
		status = DXP_READ_CSR;
		dxp_log_error("dxp_read_csr","Error reading CSR",status);
	}
	return status;
}


/******************************************************************************
 * Routine to write to the Word Count Register
 * 
 * This register contains the number of events ready for readout in the 
 * DSP memory
 *
 ******************************************************************************/
static int dxp_read_wcr(int* ioChan, unsigned short* data)
/* int *ioChan;				Input: I/O channel of G200 module   */
/* unsigned short *data;		Output: where to put data from WCR */
{
	unsigned int f, a, len;
	int status;
	unsigned short saddr;

/* write transfer start address register */
	
	saddr = G200_WCR_ADDRESS;
	status = dxp_write_tsar(ioChan, &saddr);		/* write TSAR */
	if (status!=DXP_SUCCESS){
		dxp_log_error("dxp_read_wcr","Error writing TSAR",status);
		return status;
	}

/* Read the WCR data */

	f=G200_WCR_F_READ;
	a=G200_WCR_A_READ;
	len=1;
	status=g200_md_io(ioChan,&f,&a,data,&len);
	if (status!=DXP_SUCCESS){
		status = DXP_READ_WCR;
		dxp_log_error("dxp_read_wcr","Error reading WCR",status);
	}
	return status;
}

/******************************************************************************
 * Routine to read data from the G200
 * 
 * This is the generic data transfer routine.  It can transfer data from the 
 * DSP for example based on the address previously downloaded to the TSAR
 *
 ******************************************************************************/
static int dxp_read_data(int* ioChan, unsigned short* data, unsigned int len)
/* int *ioChan;							Input: I/O channel of G200 module   */
/* unsigned short *data;				Output: where to put data read     */
/* unsigned int len;					Input: length of the data to read  */
{
	unsigned int f, a;
	int status;

	f=G200_DATA_F_READ;
	a=G200_DATA_A_READ;
	status=g200_md_io(ioChan,&f,&a,data,&len);					/* write TSAR */
	if (status!=DXP_SUCCESS){
		status = DXP_READ_DATA;
		dxp_log_error("dxp_read_data","Error reading data",status);
	}
	return status;
}

/******************************************************************************
 * Routine to write data to the G200
 * 
 * This is the generic data transfer routine.  It can transfer data to the 
 * DSP for example based on the address previously downloaded to the TSAR
 *
 ******************************************************************************/
static int dxp_write_data(int* ioChan, unsigned short* data, unsigned int len)
/* int *ioChan;							Input: I/O channel of G200 module   */
/* unsigned short *data;				Input: address of data to write    */
/* unsigned int len;					Input: length of the data to read  */
{
	unsigned int f, a;
	int status;

	f=G200_DATA_F_WRITE;
	a=G200_DATA_A_WRITE;
	status=g200_md_io(ioChan,&f,&a,data,&len);					/* write TSAR */
	if (status!=DXP_SUCCESS){
		status = DXP_WRITE_DATA;
		dxp_log_error("dxp_write_data","Error writing data",status);
	}

	return status;
}

/******************************************************************************
 * Routine to write fippi data
 * 
 * This is the routine that transfers the FiPPi program to the G200.  It 
 * assumes that the CSR is already downloaded with what channel requires
 * a FiPPi program.
 *
 ******************************************************************************/
static int dxp_write_fippi(int* ioChan, unsigned short* data, unsigned int len)
/* int *ioChan;							Input: I/O channel of G200 module   */
/* unsigned short *data;				Input: address of data to write    */
/* unsigned int len;					Input: length of the data to read  */
{
	unsigned int f, a;
	int status;
	unsigned short saddr;

/* write transfer start address register */
	
	saddr = G200_FIPPI_ADDRESS;
	status = dxp_write_tsar(ioChan, &saddr);
	if (status!=DXP_SUCCESS){
		dxp_log_error("dxp_write_fippi","Error writing TSAR",status);
		return status;
	}

/* Write the FIPPI data */

	f=G200_FIPPI_F_WRITE;
	a=G200_FIPPI_A_WRITE;
	status=g200_md_io(ioChan,&f,&a,data,&len);					/* write TSAR */
	if (status!=DXP_SUCCESS){
		status = DXP_WRITE_FIPPI;
		dxp_log_error("dxp_write_fippi","Error writing to FiPPi configuration register",status);
	}
	return status;
}

/******************************************************************************
 * Routine to write MMU data
 * 
 * This is the routine that transfers the MMU program to the G200.  It 
 * assumes that the MMU is already reset.
 *
 ******************************************************************************/
static int dxp_write_mmu(int* ioChan, unsigned short* data, unsigned int len)
/* int *ioChan;							Input: I/O channel of G200 module   */
/* unsigned short *data;				Input: address of data to write    */
/* unsigned int len;						Input: length of the data to read  */
{
	unsigned int f, a;
	int status;
	unsigned short saddr;

/* write transfer start address register */
	
	saddr = G200_MMU_ADDRESS;
	status = dxp_write_tsar(ioChan, &saddr);
	if (status!=DXP_SUCCESS)
	  {
	    dxp_log_error("dxp_write_fippi","Error writing TSAR",status);
	    return status;
	  }

/* Write the FIPPI data */

	f=G200_MMU_F_WRITE;
	a=G200_MMU_A_WRITE;
	status=g200_md_io(ioChan,&f,&a,data,&len);					/* write TSAR */
	if (status!=DXP_SUCCESS)
	  {
	    status = DXP_WRITE_MMU;
	    dxp_log_error("dxp_write_fippi","Error writing to MMU configuration register",status);
	  }
	return status;
}

/******************************************************************************
 * Routine to enable the LAMs(Look-At-Me) on the specified G200
 *
 * Enable the LAM for a single G200 module.
 *
 ******************************************************************************/
static int dxp_look_at_me(int* ioChan, int* modChan)
/* int *ioChan;						Input: I/O channel of G200 module */
/* int *modChan;					Input: G200 channels no (0,1,2,3)      */
{
	int status=DXP_SUCCESS, *itemp;
	unsigned short data;

/* assign the input values to avoid compiler warnings */
	itemp = modChan;

/* modify the LAM enable bit */
	status = dxp_read_csr(ioChan, &data);
	if (status!=DXP_SUCCESS)
	  {
	    dxp_log_error("dxp_ignore_me","Error Reading the CSR",status);
	    return status;
	  }

	data |= MASK_LAM_ENABLE;

	status = dxp_write_csr(ioChan, &data);
	if (status!=DXP_SUCCESS)
	  {
	    dxp_log_error("dxp_ignore_me","Error writing CSR",status);
	    return status;
	  }

	return status;
}

/******************************************************************************
 * Routine to disable the LAMs(Look-At-Me) on the specified G200
 *
 * Disable the LAM for a single G200 module.
 *
 ******************************************************************************/
static int dxp_ignore_me(int* ioChan, int* modChan)
/* int *ioChan;						Input: I/O channel of G200 module */
/* int *modChan;					Input: G200 channels no (0,1,2,3)      */
{
	int status = DXP_SUCCESS, *itemp;
	unsigned short data;

/* assign the input value to avoid compiler warnings */
	itemp = modChan;

/* modify the LAM enable bit */
	status = dxp_read_csr(ioChan, &data);
	if (status!=DXP_SUCCESS)
	  {
	    dxp_log_error("dxp_ignore_me","Error Reading the CSR",status);
	    return status;
	  }

	data &= ~MASK_LAM_ENABLE;

	status = dxp_write_csr(ioChan, &data);
	if (status!=DXP_SUCCESS)
	  {
	    dxp_log_error("dxp_ignore_me","Error writing CSR",status);
	    return status;
	  }

	return status;
}

/******************************************************************************
 * Routine to clear the LAM(Look-At-Me) on the specified G200
 *
 * Clear the LAM for a single G200 module.
 *
 ******************************************************************************/
static int dxp_clear_LAM(int* ioChan, int* modChan)
/* int *ioChan;						Input: I/O channel of G200 module */
/* int *modChan;					Input: G200 channels no (0)       */
{
	int status=DXP_SUCCESS, *itemp;
	unsigned short data;

/* assign the input values to avoid compiler warnings */
	itemp = modChan;

/* Clear the LAM by reading the word count register */
	status = dxp_read_wcr(ioChan, &data);
	if (status!=DXP_SUCCESS){
		dxp_log_error("dxp_clear_LAM","Error reading the Word Count Register",status);
		return status;
	}

	return status;
}

/********------******------*****------******-------******-------******------*****
 * Now begins the code devoted to routines that look like CAMAC transfers to 
 * a preset address to the external world.  They actually write to the TSAR, 
 * then the CSR, then finally write the data to the CAMAC bus.  The user need 
 * never know
 *
 ********------******------*****------******-------******-------******------*****/

/******************************************************************************
 * Routine to read a single word of data from the G200
 * 
 * This routine makes all the neccessary calls to the TSAR and CSR to read
 * data from the appropriate channel and address of the G200.
 *
 ******************************************************************************/
static int dxp_read_word(int* ioChan, int* modChan, unsigned short* addr,
						 unsigned short* readdata)
/* int *ioChan;						Input: I/O channel of G200 module      */
/* int *modChan;					Input: G200 channels no (0,1,2,3)      */
/* unsigned short *addr;			Input: Address within DSP memory      */
/* unsigned short *readdata;		Output: word read from memory         */
{
/*
 *     Read a single word from a DSP address, for a single channel of a
 *                single G200 module.
 */
	int status;
	char info_string[INFO_LEN];
	
	if((*modChan!=0)&&(*modChan!=ALLCHAN)){
		sprintf(info_string,"X10P routine called with channel number %d",*modChan);
		status = DXP_BAD_PARAM;
		dxp_log_error("dxp_read_word",info_string,status);
	}

/* write transfer start address register */
	
	status = dxp_write_tsar(ioChan, addr);
	if (status!=DXP_SUCCESS){
		dxp_log_error("dxp_read_word","Error writing TSAR",status);
		return status;
	}

/* now read the data */

	status = dxp_read_data(ioChan, readdata, 1);
	if(status!=DXP_SUCCESS){
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
 * data to the appropriate channel and address of the G200.
 *
 ******************************************************************************/
static int dxp_write_word(int* ioChan, int* modChan, unsigned short* addr,
						  unsigned short* writedata)
/* int *ioChan;						Input: I/O channel of G200 module      */
/* int *modChan;					Input: G200 channels no (-1,0,1,2,3)   */
/* unsigned short *addr;			Input: Address within X or Y mem.     */
/* unsigned short *writedata;		Input: word to write to memory        */
{
/*
 *     Write a single word to a DSP address, for a single channel or all 
 *            channels of a single G200 module
 */
	int status;
	char info_string[INFO_LEN];

	if((*modChan!=0)&&(*modChan!=ALLCHAN)){
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
	if (status!=DXP_SUCCESS){
		status=DXP_WRITE_WORD;
		dxp_log_error("dxp_write_word","Error writing CAMDATA",status);
		return status;
	}
	return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to read a block of data from the G200
 * 
 * This routine makes all the neccessary calls to the TSAR and CSR to read
 * data from the appropriate channel and address of the DXP.
 *
 ******************************************************************************/
static int dxp_read_block(int* ioChan, int* modChan, unsigned short* addr,
						  unsigned int* length, unsigned short* readdata)
/* int *ioChan;						Input: I/O channel of DXP module       */
/* int *modChan;					Input: DXP channels no (0,1,2,3)       */
/* unsigned short *addr;			Input: start address within X or Y mem.*/
/* unsigned int *length;			Input: # of 16 bit words to transfer   */
/* unsigned short *readdata;		Output: words to read from memory      */
{
/*
 *     Read a block of words from a single DSP address, or consecutive
 *          addresses for a single channel of a single DXP module
 */
	int status;
	char info_string[INFO_LEN];
	unsigned int i,nxfers,xlen;
	unsigned int maxblk;

	if((*modChan!=0)&&(*modChan!=ALLCHAN)){
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
	maxblk=g200_md_get_maxblk();
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
/* int *ioChan;						Input: I/O channel of DXP module     */
/* int *modChan;					Input: DXP channels no (-1,0,1,2,3)  */
/* unsigned short *addr;			Input: start address within X/Y mem  */
/* unsigned int *length;			Input: # of 16 bit words to transfer */
/* unsigned short *writedata;		Input: words to write to memory      */
{
/*
 *    Write a block of words to a single DSP address, or consecutive
 *    addresses for a single channel or all channels of a single DXP module
 */
	int status;
	char info_string[INFO_LEN];
	unsigned int i,nxfers,xlen;
	unsigned int maxblk;

	if((*modChan!=0)&&(*modChan!=ALLCHAN)){
		sprintf(info_string,"X10P routine called with channel number %d",*modChan);
		status = DXP_BAD_PARAM;
		dxp_log_error("dxp_write_block",info_string,status);
	}

/* write transfer start address register */

	status = dxp_write_tsar(ioChan, addr);
	if (status!=DXP_SUCCESS){
		dxp_log_error("dxp_write_block","Error writing TSAR",status);
		return status;
	}

/* Retrieve MAXBLK and check if single transfer is needed */
	maxblk=g200_md_get_maxblk();
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
 * Routine to download all FPGA firmware
 * 
 * This routine downloads the MMU (memory manager unit) and Fippi programs
 * to the G200.
 *
 ******************************************************************************/
static int dxp_download_fpgaconfig(int* ioChan, int* modChan, char *name, Board* board)
/* int *ioChan;			Input: I/O channel of DXP module	*/
/* int *modChan;		Input: DXP channels no (-1,0,1,2,3)	*/
/* char *name;                  Input: type of fpga to download         */
/* Board *board;		Input: Board data			*/
{
/*
 *   Download the appropriate Firmware configuration files to a G200
 */
  int status;
  char info_string[INFO_LEN];
  unsigned int i;
  
  /* convert to lower case */
  for (i=0; i<strlen(name); i++) name[i] = (char) tolower(name[i]);

  sprintf(info_string,"name = %s, modChan = %i", name, *modChan);
  dxp_log_debug("dxp_download_fpgaconfig", info_string);

  /* Do the MMU first if requested */
  if ((STREQ(name, "all")) || (STREQ(name, "mmu"))) 
    {
      status = dxp_download_mmuconfig(ioChan, modChan, board);
      if (status != DXP_SUCCESS) 
	{
	  sprintf(info_string,"Unable to download the MMU to module %i", board->mod);
	  dxp_log_error("dxp_download_fpgaconfig", info_string, status);
	  return status;
	}
    }

  /* Do the FIPPI now */
  if ((STREQ(name, "all")) || (STREQ(name, "fippi"))) 
    {
      status = dxp_download_fippiconfig(ioChan, modChan, board);
      if (status != DXP_SUCCESS) 
	{
	  sprintf(info_string,"Unable to download the FiPPi to module %i", board->mod);
	  dxp_log_error("dxp_download_fpgaconfig", info_string, status);
	  return status;
	}
    }

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * This routine downloads the MMU (memory manager unit) to the G200.
 *
 ******************************************************************************/
static int dxp_download_mmuconfig(int* ioChan, int* modChan, Board* board)
/* int *ioChan;			Input: I/O channel of DXP module	*/
/* int *modChan;		Input: DXP channels no (-1,0,1,2,3)	*/
/* Board *board;		Input: Board data			*/
{
/*
 *   Download the appropriate Firmware configuration files to a G200
 */
  int status;
  char info_string[INFO_LEN];
  unsigned short data;
  unsigned int j,length,xlen,nxfers;
  float wait;
  unsigned int maxblk;
  Fippi_Info *fippi=NULL;
  
  if((*modChan!=0)&&(*modChan!=ALLCHAN)){
    sprintf(info_string,"G200 routine called with channel number %d",*modChan);
    status = DXP_BAD_PARAM;
    dxp_log_error("dxp_download_mmuconfig",info_string,status);
    return status;
  }

/* Download the Memory manager */
  fippi = board->mmu;
/* make sure a valid MMU was found */
  if (fippi==NULL) {
    sprintf(info_string,"There is no valid MMU defined for module %i",board->mod);
    status = DXP_NOFIPPI;
    dxp_log_error("dxp_download_mmuconfig",info_string,status);
    return status;
  }
  
  length = fippi->proglen;
	
/* RMW to CSR to initiate download */
	
  status = dxp_read_csr(ioChan, &data);
  if (status!=DXP_SUCCESS){
    dxp_log_error("dxp_download_mmuconfig","Error reading the CSR",status); 
    return status;
  }

  data |= MASK_MMURESET;
  
  status = dxp_write_csr(ioChan, &data);
  if (status!=DXP_SUCCESS){
    dxp_log_error("dxp_download_mmuconfig","Error writing CSR while reseting MMU",status); 
    return status;
  }
  
/* wait 50ms, for LCA to be ready for next data */

  wait = 0.050f;
  status = g200_md_wait(&wait);

/* Retrieve MAXBLK and check if single transfer is needed */
  maxblk = g200_md_get_maxblk();
  if (maxblk <= 0) maxblk = length;
  
/* prepare for the first pass thru loop */
  nxfers = ((length-1)/maxblk) + 1;
  xlen = ((maxblk>=length) ? length : maxblk);
  j = 0;
  do {

/* now write the data */
        
    status = dxp_write_mmu(ioChan, &(fippi->data[j*maxblk]), xlen);
    if (status!=DXP_SUCCESS){
      status = DXP_WRITE_BLOCK;
      sprintf(info_string,"Error in %dth (last) block transfer of MMU",j);
      dxp_log_error("dxp_download_mmuconfig",info_string,status);
      return status;
    }
/* Next loop */
    j++;
/* On last pass thru loop transfer the remaining bytes */
    if (j==(nxfers-1)) xlen=((length-1)%maxblk) + 1;
  } while (j<nxfers);
  
  return DXP_SUCCESS;
}

/******************************************************************************
 * 
 * This routine downloads Fippi programs to the G200.
 *
 ******************************************************************************/
static int dxp_download_fippiconfig(int* ioChan, int* modChan, Board* board)
/* int *ioChan;			Input: I/O channel of DXP module	*/
/* int *modChan;		Input: DXP channels no (-1,0,1,2,3)	*/
/* Board *board;		Input: Board data			*/
{
/*
 *   Download the appropriate Firmware configuration files to a G200
 */
  int status;
  char info_string[INFO_LEN];
  unsigned short data;
  unsigned int i, j,length,xlen,nxfers;
  float wait;
  unsigned int maxblk;
  Fippi_Info *fippi=NULL;
  
  if((*modChan!=0)&&(*modChan!=ALLCHAN)){
    sprintf(info_string,"G200 routine called with channel number %d",*modChan);
    status = DXP_BAD_PARAM;
    dxp_log_error("dxp_download_fippiconfig",info_string,status);
    return status;
  }

/* If allchan chosen, then select the first valid fippi */
  for (i=0;i<board->nchan;i++) {
    if (((board->used)&(0x1<<i))!=0) {
      fippi = board->fippi[i];
      break;
    }
  }
/* make sure a valid Fippi was found */
  if (fippi==NULL) {
    sprintf(info_string,"There is no valid FiPPi defined for module %i",board->mod);
    status = DXP_NOFIPPI;
    dxp_log_error("dxp_download_fippiconfig",info_string,status);
    return status;
  }
  
  length = fippi->proglen;
	
/* Write to CSR to initiate download */
	
  status = dxp_read_csr(ioChan, &data);
  if (status!=DXP_SUCCESS){
    dxp_log_error("dxp_download_fippiconfig","Error reading the CSR",status); 
    return status;
  }

  data |= MASK_FIPRESET;
  
  status = dxp_write_csr(ioChan, &data);
  if (status!=DXP_SUCCESS){
    dxp_log_error("dxp_download_fippiconfig","Error writing CSR while reseting the FiPPi", status);
    return status;
  }

  dxp_log_info("dxp_download_fippiconfig", "Should have just reset the FIPPI");

/* wait 50ms, for LCA to be ready for next data */

  wait = 0.050f;
  status = g200_md_wait(&wait);
  
/* Retrieve MAXBLK and check if single transfer is needed */
  maxblk=g200_md_get_maxblk();
  if (maxblk <= 0) maxblk = length;
  
/* prepare for the first pass thru loop */
  nxfers = ((length-1)/maxblk) + 1;
  xlen = ((maxblk>=length) ? length : maxblk);
  j = 0;
  do {

/* now write the data */
        
    status = dxp_write_fippi(ioChan, &(fippi->data[j*maxblk]), xlen);
    if (status!=DXP_SUCCESS){
      status = DXP_WRITE_BLOCK;
      sprintf(info_string,"Error in %dth (last) block transfer",j);
      dxp_log_error("dxp_download_fippiconfig",info_string,status);
      return status;
    }
/* Next loop */
    j++;
/* On last pass thru loop transfer the remaining bytes */
    if (j==(nxfers-1)) xlen=((length-1)%maxblk) + 1;
  } while (j<nxfers);
  
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
/* Fippi_Info *fippi;			I/O: structure of Fippi info */
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

	if (fippi->data==NULL){
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
	if((fp = dxp_find_file(fippi->filename,"r"))==NULL){
		status = DXP_OPEN_FILE;
		sprintf(info_string,"%s%s","Unable to open FPGA configuration ",
			fippi->filename);
		dxp_log_error("dxp_get_fpgaconfig",info_string,status);
		return status;
	}
	
/* Stuff the data into the fipconfig array */
		
	lowbyte = 1;
	len = 0;
	while (fgets(line,132,fp)!=NULL){
		if (line[0]=='*') continue;
		nchars = strlen(line)-1;
		while ((nchars>0) && !isxdigit(line[nchars])) {
			nchars--;
		}
		for(j=0;j<nchars;j=j+2) {
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
 * Routine to check that all the FPGAs downloaded successfully to
 * a single module.  This routine will also update the DSP information about
 * the FiPPi after the download is confirmed good.  If the routine returns 
 * DXP_SUCCESS, then the FiPPis are OK and the DSP data is updated
 *
 ******************************************************************************/
static int dxp_download_fpga_done(int *modChan, char *name, Board *board)
/* int *modChan;			Input: Module channel number              */
/* char *name;                          Input: Type of FPGA to check the status of*/
/* board *board;			Input: Board structure for this device 	  */
{

  int status = DXP_SUCCESS;
  unsigned int i;

  int ioChan;
  unsigned short used;

  char info_string[INFO_LEN];

  /* variables associated with a control task */
  short type;
  unsigned int len;
  int idata[2];
  float timeout;
  int mymodChan;

  int idummy;

  unsigned short data;

  /* Satisfy the compiler */
  idummy = *modChan;

  /* Few assignements to make life easier */
  ioChan = board->ioChan;
  used = board->used;

  /* convert the name to lower case */
  for (i=0; i<strlen(name); i++) name[i] = (char) tolower(name[i]);
	
/* Read back the CSR to determine if the download was successfull.  */
  if((status=dxp_read_csr(&ioChan,&data))!=DXP_SUCCESS)
    {
      sprintf(info_string," failed to read CSR for module %d",board->mod);
      dxp_log_error("dxp_download_fippi_done",info_string,status);
      return status;
    }

  /* if not used, then we succeed */
  if(used==0) return DXP_SUCCESS;

  /* Check the CSR value versus the mask for each case */
  if ((STREQ(name, "all")) || (STREQ(name, "mmu")))
    {
      if((data & MASK_MMU_DONE) != 0)
	{
	  sprintf(info_string,
		  "MMU download error (CSR bits) for module %d", board->mod);
	  status=DXP_FPGADOWNLOAD;
	  dxp_log_error("dxp_download_fpga_done",info_string,status);
	}
    }
  if ((STREQ(name, "all")) || (STREQ(name, "fippi")))
    {
      if((data & MASK_FIPPI_DONE) != 0)
	{
	  sprintf(info_string,
		  "FIPPI download error (CSR bits) for module %d", board->mod);
	  status=DXP_FPGADOWNLOAD;
	  dxp_log_error("dxp_download_fpga_done",info_string,status);
	}

      /* Check to see if we have a DSP downloaded yet, iff we do, then force the DSP to 
       * update the FiPPi.  Force the modChan to be 0, G200 are always single channel 
       * devices. 
       */
      mymodChan = 0;
      if (board->chanstate[mymodChan].dspdownloaded == 1) 
	{

	  /* If the request was to download all or fippi, then this is the right time to 
	   * update the fippi register values in the DSP -> force a program_fippi
	   * control task 
	   */
	  type = CT_DGFG200_PROGRAM_FIPPI;
	  len = 2;
	  idata[0] = 1;
	  idata[1] = 1;
	  timeout = 0.01f;
	  /* Be careful not to perform control tasks on ALLCHAN.  For the G200, this is simple since
	   * there is only 1 channel per module.  For other devices, we would need to loop over the
	   * channels to update all DSPs.
	   */
	  if((status=dxp_do_control_task(&ioChan, &mymodChan, &type, board, &len, idata, 
					 &timeout, NULL)) != DXP_SUCCESS)
	    {
	      sprintf(info_string,"Unable to execute the Program FiPPi control task for mod %i", board->mod);
	      dxp_log_error("dxp_download_fippi_done",info_string,status);
	      return status;
	    }
	  /* In addition, perform a set dacs task
	   */
	  type = CT_DGFG200_SETDACS;
	  len = 2;
	  idata[0] = 1;
	  idata[1] = 1;
	  timeout = 0.01f;
	  /* Be careful not to perform control tasks on ALLCHAN.  For the G200, this is simple since
	   * there is only 1 channel per module.  For other devices, we would need to loop over the
	   * channels to update all DSPs.
	   */
	  if((status=dxp_do_control_task(&ioChan, &mymodChan, &type, board, &len, idata, 
					 &timeout, NULL)) != DXP_SUCCESS)
	    {
	      sprintf(info_string,"Unable to execute the SETDACS control task for mod %i", board->mod);
	      dxp_log_error("dxp_download_fippi_done",info_string,status);
	      return status;
	    }
	  /* In addition, perform a set dacs task
	   */
	  type = CT_DGFG200_OPEN_INPUT_RELAY;
	  len = 2;
	  idata[0] = 1;
	  idata[1] = 1;
	  timeout = 0.01f;
	  /* Be careful not to perform control tasks on ALLCHAN.  For the G200, this is simple since
	   * there is only 1 channel per module.  For other devices, we would need to loop over the
	   * channels to update all DSPs.
	   */
	  if((status=dxp_do_control_task(&ioChan, &mymodChan, &type, board, &len, idata, 
					 &timeout, NULL)) != DXP_SUCCESS)
	    {
	      sprintf(info_string,"Unable to execute the SETDACS control task for mod %i", board->mod);
	      dxp_log_error("dxp_download_fippi_done",info_string,status);
	      return status;
	    }
	}
    }

  return status;
}

/******************************************************************************
 * Routine to download the DSP Program
 * 
 * This routine downloads the DSP program to the specified CAMAC slot and 
 * channel on the DXP.  If -1 for the DXP channel is specified then all
 * channels are downloaded.
 *
 ******************************************************************************/
static int dxp_download_dspconfig(int* ioChan, int* modChan, Dsp_Info* dsp)
     /* int *ioChan;					Input: I/O channel of DXP module		*/
     /* int *modChan;				Input: DXP channel no (-1,0,1,2,3)	*/
     /* Dsp_Info *dsp;				Input: DSP structure					*/
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
  float fiftyms = 0.05f;
	

  if((*modChan!=0)&&(*modChan!=ALLCHAN)){
    sprintf(info_string,"G200 called with channel number %d",*modChan);
    status = DXP_BAD_PARAM;
    dxp_log_error("dxp_download_dspconfig",info_string,status);
  }
  
  status = dxp_read_csr(ioChan, &data);
  if (status != DXP_SUCCESS)
    {
      dxp_log_error("dxp_download_dspconfig", "Error reading the CSR", status);
      return status;
    }
  
  /* Write to CSR to initiate download */
  
  data |= MASK_DSPRESET;
  
  status = dxp_write_csr(ioChan, &data);
  if (status!=DXP_SUCCESS){
    dxp_log_error("dxp_download_dspconfig","Error writing CSR",status);
    return status;
  }
  
/* Delay to let the DSP reset */

	g200_md_wait(&fiftyms);

/* 
 * Transfer the DSP program code.  Skip the first word and 
 * return to it at the end.
 */
	length = dsp->proglen - 2;

/* Retrieve MAXBLK and check if single transfer is needed */
	maxblk=g200_md_get_maxblk();
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

/* Delay to let the DSP finish initializations */

	g200_md_wait(&fiftyms);
	g200_md_wait(&fiftyms);

/*
 * All done, clear the LAM on this module 
 */
	if((status=dxp_clear_LAM(ioChan, modChan))!=DXP_SUCCESS){
		dxp_log_error("dxp_download_dspconfig","Unable to clear LAM",status);
	}

	return status;
}
	
/******************************************************************************
 *
 * Routine to check that the DSP is done with its initialization.  
 * If the routine returns DXP_SUCCESS, then the DSP is ready to run.
 *
 ******************************************************************************/
static int dxp_download_dsp_done(int* ioChan, int* modChan, int* mod, 
								 Dsp_Info* dsp, unsigned short* value, 
								 float* timeout)
/* int *ioChan;						Input: I/O channel of the module		*/
/* int *modChan;					Input: Module channel number			*/
/* int *mod;						Input: Module number, for errors		*/
/* Dsp_Info *dsp;					Input: Relavent DSP info				*/
/* unsigned short *value;			Input: Value to match for BUSY		*/
/* float *timeout;					Input: How long to wait, in seconds	*/
{

	int status=DXP_SUCCESS;

	int itemp;
	Dsp_Info *dsptemp;
	unsigned short ustemp;
	float ftemp;

/* Remove those pesky compiler warnings */
	itemp = *ioChan;
	itemp = *modChan;
	itemp = *mod;
	dsptemp = dsp;
	ustemp = *value;
	ftemp = *timeout;

/* Nothing to check for the Gamma ray technology. */

	return status;
}

/******************************************************************************
 * 
 * Routine to retrieve the FIPPI program maximum sizes so that memory
 * can be allocated.
 *
 ******************************************************************************/
static int dxp_get_fipinfo(Fippi_Info* fippi)
/* Fippi_Info *fippi;				I/O: Structure of FIPPI program Info	*/
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
/* Dsp_Defaults *defaults;				I/O: Structure of FIPPI program Info	*/
{

	defaults->params->maxsym	= MAXSYM;
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
/* Dsp_Info *dsp;							I/O: Structure of DSP program Info	*/
{

	dsp->params->maxsym	    = MAXSYM;
	dsp->params->maxsymlen  = MAXSYMBOL_LEN;
	dsp->maxproglen			= MAXDSP_LEN;

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
/* Dsp_Info *dsp;					I/O: Structure of DSP program Info	*/
{
	FILE  *fp, *varfp;
	char info_string[INFO_LEN];
	int   status;
	char  varfile[256];
	
	sprintf(info_string, "Loading DSP program in %s", dsp->filename); 
	dxp_log_info("dxp_get_dspconfig",info_string);

/* Now retrieve the file pointer to the DSP program */

	if((fp = dxp_find_file(dsp->filename,"rb"))==NULL){
		status = DXP_OPEN_FILE;
		sprintf(info_string, "Unable to open %s", dsp->filename); 
		dxp_log_error("dxp_get_dspconfig",info_string,status);
		return status;
	}

/* Copy the parameter file name and change the extension */
	strcpy(varfile, dsp->filename);
	strcpy(varfile+strlen(varfile)-3, "var");

	if((varfp = dxp_find_file(varfile,"r"))==NULL){
		status = DXP_OPEN_FILE;
		sprintf(info_string, "Unable to open %s", varfile); 
		dxp_log_error("dxp_get_dspconfig",info_string,status);
		return status;
	}

/* Fill in some general information about the DSP program		*/
	dsp->params->maxsym		= MAXSYM;
	dsp->params->maxsymlen	= MAXSYMBOL_LEN;
	dsp->maxproglen			= MAXDSP_LEN;

/* Load the symbol table and configuration */

	if ((status = dxp_load_dspfile(varfp, fp, dsp))!=DXP_SUCCESS) {
		status = DXP_DSPLOAD;
		fclose(varfp);
		fclose(fp);
		dxp_log_error("dxp_get_dspconfig","Unable to Load DSP file",status);
		return status;
	}

/* Close the file and get out */

	fclose(varfp);
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
/* Dsp_Defaults *defaults;					I/O: Structure of DSP defaults    */
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

	for(i=0;i<4;i++) strtmp[i] = (char) toupper(defaults->filename[i]);
	if (strncmp(strtmp,"NULL",4)==0) {
		status = DXP_SUCCESS;
		defaults->params->nsymbol=0;
		return status;
	}

	
/* Now retrieve the file pointer to the DSP defaults file */

    if ((fp = dxp_find_file(defaults->filename,"r"))==NULL){
        status = DXP_OPEN_FILE;
		sprintf(info_string,"Unable to open %s",defaults->filename);
        dxp_log_error("dxp_get_dspdefaults",info_string,status);
        return status;
    }

/* Initialize the number of default values */
	nsymbols = 0;

    while(fstatus!=NULL){
        do 
            fstatus = fgets(line,132,fp);
        while ((line[0]=='*') && (fstatus!=NULL));

/* Check if we are downloading more parameters than allowed. */
        
		if (nsymbols==MAXSYM){
			defaults->params->nsymbol = nsymbols;
            status = DXP_ARRAY_TOO_SMALL;
			sprintf(info_string,"Too many parameters in %s",defaults->filename);
	        dxp_log_error("dxp_get_dspdefaults",info_string,status);
            return status;
        }
        if ((fstatus==NULL)) continue;					/* End of file?  or END finishes */
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
static int dxp_load_dspfile(FILE* varfp, FILE* fp, Dsp_Info* dsp)
/* FILE  *varfp;						Input: Pointer to the opened DSP variable file	*/
/* FILE  *fp;							Input: Pointer to the opened DSP file			*/
/* Dsp_Info *dsp;						I/O: Structure of DSP program Info				*/
{
	int status;
   
/* Load the symbol table */
	
	if ((status = dxp_load_dspsymbol_table(varfp, dsp))!=DXP_SUCCESS) {
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
/* FILE *fp;							Input: File pointer from which to read the symbols	*/
/* unsigned short *dspconfig;			Output: Array containing DSP program				*/
/* unsigned int *nwordsdsp;				Output: Size of DSP program							*/
{

	int status;
	
	unsigned long temp;

/* Check if we have some allocated memory space	*/
	if (dsp->data==NULL){
		status = DXP_NOMEM;
		dxp_log_error("dxp_load_dspconfig",
			"Error allocating space for configuration",status);
		return status;
	}
/*
 *  and read the configuration
 */
	dsp->proglen = 0;
	while(fread(&temp, 4, 1, fp)!=0) {
		dsp->data[dsp->proglen++] = (unsigned short) ((temp>>8)&0xFFFF);
		dsp->data[dsp->proglen++] = (unsigned short) (temp&0xFF);
	}

	return DXP_SUCCESS;
}	

/******************************************************************************
 * Routine to read in the DSP symbol name list
 *
 ******************************************************************************/
static int dxp_load_dspsymbol_table(FILE* fp, Dsp_Info* dsp)
/* FILE *fp;						Input: File pointer from which to read the symbols	*/
/* char **pnames;					Output: Array of DSP param names					*/
/* unsigned short *nsymbol;			Output: Number of defined DSP symbols				*/
{
	int status, retval;
	char line[LINE_LEN];
	unsigned short access;
/* Hold the character representing the access type *=R/W -=RO */
/*	char atype[2];*/

/* Do we have allocated space for parameter names? */
	if (dsp->params->parameters==NULL){
		status = DXP_NOMEM;
		dxp_log_error("dxp_load_dspsymbol_table",
			"Memory not allocated for all parameter names",status);
		return status;
	}
/*
 * Parse thru file counting the number of symbols to be read and 
 * read in the symbol names and offsets 
 */
	access = 1;
	dsp->params->nsymbol = 0;
	while(fgets(line,132,fp)!=NULL){
		if (line[0]=='*') continue;
		retval = sscanf(line, "%i %s", &(dsp->params->parameters[dsp->params->nsymbol].address), 
									dsp->params->parameters[dsp->params->nsymbol].pname);
/*		retval = sscanf(line, "%i %s", &(dsp->params->parameters[dsp->params->nsymbol].address), 
									dsp->params->parameters[dsp->params->nsymbol].pname, atype,
									&(dsp->params->parameters[dsp->params->nsymbol].lbound), 
									&(dsp->params->parameters[dsp->params->nsymbol].ubound)); */
/* if we are beyond the 256 word boundry, the parameters change to WO */
		if (dsp->params->parameters[dsp->params->nsymbol].address>255) access = 0;
/* If there were at least 2 parameters read from the line, then there is 
 * at least a parameter name, else skip this line */
		if (retval==2) {
			dsp->params->parameters[dsp->params->nsymbol].access = access;
			dsp->params->parameters[dsp->params->nsymbol].lbound = 0;
			dsp->params->parameters[dsp->params->nsymbol].ubound = 0;
			dsp->params->nsymbol++;
		} else {
			continue;
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
/* unsigned short *lindex;				Input: address of parameter			*/
/* Dsp_Info *dsp;						Input: dsp structure with info		*/
/* char string[];						Output: parameter name				*/
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
/* char name[];					Input: symbol name for accessing YMEM params*/
/* Dsp_Info *dsp;				Input: dsp structure with info				*/
/* unsigned short *address;		Output: address in parameter array			*/
{
/*  
 *       Find pointer into parameter memmory using symbolic name
 */
	int status;
	char info_string[INFO_LEN];
	unsigned short i;

	if((dsp->proglen)<=0){
		status = DXP_DSPLOAD;
		sprintf(info_string, "Must Load DSP code before searching for %s", name);
		dxp_log_error("dxp_loc", info_string, status);
		return status;
	}
	
	*address = USHRT_MAX;
	for(i=0;i<(dsp->params->nsymbol);i++) {
		if (  (strlen(name)==strlen((dsp->params->parameters[i].pname))) &&
			   (strstr(name,(dsp->params->parameters[i].pname))!=NULL)  ) {
			*address = i;
			break;
		}
	}
	
/* Did we find the Symbol in the table? */
	
	status = DXP_SUCCESS;
	if(*address == USHRT_MAX){
		status = DXP_NOSYMBOL;
/*		sprintf(info_string, "Cannot find <%s> in symbol table",name);
		dxp_log_error("dxp_loc",info_string,status);
*/	}

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
/* int *ioChan;						Input: I/O channel of DXP module        */
/* int *modChan;					Input: DXP channels no (0,1,2,3)        */
/* Dsp_Info *dsp;					Input: dsp structure with info		*/
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
	data = (unsigned short *) g200_md_alloc(nsymbol*sizeof(unsigned short));

	if ((status=dxp_read_dspparams(ioChan, modChan, dsp, data))!=DXP_SUCCESS) {
		dxp_log_error("dxp_dspparam_dump","Unable to read the DSP parameters",status);
		g200_md_free(data);
		return status;
	}
	g200_md_puts("\r\nDSP Parameters Memory Dump:\r\n");
	g200_md_puts(" Parameter  Value    Parameter  Value  ");
	g200_md_puts("Parameter  Value    Parameter  Value\r\n");
	g200_md_puts("_________________________________________");
	g200_md_puts("____________________________________\r\n");
	
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
		g200_md_puts(buf);
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
		g200_md_puts(buf);
	}

/* Throw in a final line skip, if there was a stray symbol or 2 on the end of the 
 * table, else it was already added by the g200_md_puts() above */

	if (nleft!=0) g200_md_puts("\r\n");
	
/* Free the allocated memory */
	g200_md_free(data);

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
									Board* board)
/* int *ioChan;					Input: IO channel to test				*/
/* int *modChan;				Input: Channel on the module to test		*/
/* int *pattern;				Input: Pattern to use during testing		*/
/* Board *board;				Input: Relavent Board info					*/
{
	int status, nerrors;
	char info_string[INFO_LEN];
	unsigned int i, j, nloop;
/* Arrays to hold the test pattern and the results */
   long *readbuf;
	int *writebuf;
/* Length of the external memory and of the output buffer */
	unsigned int ext_len, len, total_len, current_len;
/* Control task types for reading and writing external memory */
	short type_write, type_read;
/* Array for the control task */
	int info[20];
	float timeout=1.0;

	return DXP_SUCCESS;
	
	type_write = CT_DGFG200_WRITE_MEMORY_FIRST;
	type_read = CT_DGFG200_READ_MEMORY_FIRST;
	
/* Get the information for the write control task, we will assume that the read and write
 * memory control tasks require the same array sizes. */
	if((status=dxp_control_task_params(ioChan, modChan, &type_write, board, info))!=DXP_SUCCESS){
		dxp_log_error("dxp_test_spectrum_memory",
			"Unable to retrieve control task information",status);
		return status;
	}
	len = (unsigned int) info[0];

/* Allocate the memory for the buffers */
	if (len>0) {
		readbuf = (long *) g200_md_alloc(len*sizeof(long));
/* Write one word first that is the number of loops to perform, so, need +1 more entries */
		writebuf = (int *) g200_md_alloc((len+1)*sizeof(int));
/* Set the number of loops to perform in control tasks to 1 */
		writebuf[0] = 1;
	} else {
        status=DXP_BAD_PARAM;
        sprintf(info_string,
			"Buffer length is %d",len);
        dxp_log_error("dxp_test_spectrum_memory",info_string,status);
        return status;
	}

	switch (*pattern) {
		case 0 : 
			for (j=0;j<len/2;j++) {
				writebuf[2*j+1]   = (int) (j&0x0000FFFF);
				writebuf[2*j+2] = (int) ((j&0xFFFF0000)>>16);
			}
			break;
		case 1 :
			for (j=0;j<len;j++) writebuf[j+1]= 0xFFFF;
			 break;
		case 2 :
			for (j=0;j<len;j++) writebuf[j+1] = (int) (((j&2)==0) ? 0xAAAA : 0x5555);
			break;
		default :
			status = DXP_BAD_PARAM;
			sprintf(info_string,"Pattern %d not implemented",*pattern);
			dxp_log_error("dxp_test_spectrum_memory",info_string,status);
			g200_md_free(readbuf);
			g200_md_free(writebuf);
			return status;
	}

/* Set the External spectrum memory length */
	ext_len = 65536;
	nloop = (unsigned int) ext_len/len;
	total_len = 0;

/* Loop over the number of loops required to test the entire memory */
	for (i=0;i<nloop;i++) {

/* Determine the length of this data xfer: if the total_len tested so far 
 * plus the length of the output buffer is less than the total external length
 * then proceed with the entire output buffer length, else limit the length 
 * to the remaining length to be tested.  */
		current_len = ((total_len+len)<=ext_len) ? len : (ext_len-total_len);
		
/* Perform the control task to write the data to memory */
		if((status=dxp_do_control_task(ioChan, modChan, &type_write, board,
								&current_len, writebuf, &timeout, NULL))!=DXP_SUCCESS) {
			dxp_log_error("dxp_test_spectrum_mem","Unable to write to the external memory",status);
			g200_md_free(readbuf);
			g200_md_free(writebuf);
			return status;
		}

		total_len += len;
		type_write = CT_DGFG200_WRITE_MEMORY_NEXT;
	}

	nloop = (unsigned int) ext_len/len;
	total_len = 0;
	nerrors = 0;
/* Loop over the number of loops required to read the entire memory */
	for (i=0;i<nloop;i++) {

/* Determine the length of this data xfer: if the total_len tested so far 
 * plus the length of the output buffer is less than the total external length
 * then proceed with the entire output buffer length, else limit the length 
 * to the remaining length to be tested.  */
		current_len = ((total_len+len)<=ext_len) ? len : (ext_len-total_len);
		
/* Perform the control task to write the data to memory */
		if((status=dxp_do_control_task(ioChan, modChan, &type_read, board,
								&current_len, writebuf, &timeout, readbuf))!=DXP_SUCCESS) {
			dxp_log_error("dxp_test_spectrum_mem","Unable to write to the external memory",status);
			g200_md_free(readbuf);
			g200_md_free(writebuf);
			return status;
		}
/* Check for errors */
		for(j=0; j<len;j++) {
			if (readbuf[j]!=(long) writebuf[j+1]) {
				nerrors++;
				status = DXP_MEMERROR;
				if(nerrors<10){
					sprintf(info_string, "Error: word %d, wrote %x, read back %lx",
						j,writebuf[j+1], readbuf[j]);
					dxp_log_error("dxp_test_mem",info_string,status);
				}
			}
		}

		total_len += len;
		type_read = CT_DGFG200_READ_MEMORY_NEXT;
	}

	if (nerrors!=0){
		sprintf(info_string, "%d memory compare errors found",nerrors);
		dxp_log_error("dxp_test_mem",info_string,status);
	}

	g200_md_free(readbuf);
	g200_md_free(writebuf);
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
/* int *ioChan;						Input: IO channel to test				*/
/* int *modChan;					Input: Channel on the module to test	*/
/* int *pattern;					Input: Pattern to use during testing	*/
/* Board *board;					Input: Relavent Board info					*/
{
	int status=DXP_SUCCESS;

	int *itemp;
	Board *btemp;

/* Use the input parameters to prevent compiler warnings */
	itemp = ioChan;
	itemp = modChan;
	itemp = pattern;
	btemp = board;

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
/* int *ioChan;						Input: IO channel to test				*/
/* int *modChan;					Input: Channel on the module to test	*/
/* int *pattern;					Input: Pattern to use during testing	*/
/* Board *board;					Input: Relavent Board info					*/
{
	int status;
	char info_string[INFO_LEN];
	unsigned short start, addr;
	unsigned int len;

/* Now read the Event Buffer base address and length and store in
 * static library variables for future use. */

	if((status=dxp_loc("AOUTBUFFER", board->dsp[*modChan], &addr))!=DXP_SUCCESS){
		dxp_log_error("dxp_test_event_memory",
			"Unable to find AOUTBUFFER symbol",status);
	}
	start = (unsigned short) board->params[*modChan][addr];
	if((status=dxp_loc("LOUTBUFFER", board->dsp[*modChan], &addr))!=DXP_SUCCESS){
		dxp_log_error("dxp_test_event_memory",
			"Unable to find LOUTBUFFER symbol",status);
	}
	len = (unsigned int) board->params[*modChan][addr];

	if((status = dxp_test_mem(ioChan, modChan, pattern,
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
/* int *ioChan;						Input: I/O channel of DXP module        */
/* int *modChan;					Input: DXP channels no (0,1,2,3)        */
/* int *pattern;					Input: pattern to write to memmory      */
/* unsigned int *length;			Input: number of 16 bit words to xfer   */
/* unsigned short *addr;			Input: start address for transfer       */
{
	int status, nerrors;
	char info_string[INFO_LEN];
	unsigned int j;
    unsigned short *readbuf,*writebuf;

/* Allocate the memory for the buffers */
	if (*length>0) {
		readbuf = (unsigned short *) g200_md_alloc(*length*sizeof(unsigned short));
		writebuf = (unsigned short *) g200_md_alloc(*length*sizeof(unsigned short));
	} else {
        status=DXP_BAD_PARAM;
        sprintf(info_string,
			"Attempting to test %d elements in DSP memory",*length);
        dxp_log_error("dxp_test_mem",info_string,status);
        return status;
	}

/* First 256 words of Data memory are reserved for DSP parameters. */
	if((*addr>0x4000)&&(*addr<0x4100)){
		status=DXP_BAD_PARAM;
		sprintf(info_string,
		 "Attempting to overwrite Parameter values beginning at address %x",*addr);
		dxp_log_error("dxp_test_mem",info_string,status);
		g200_md_free(readbuf);
		g200_md_free(writebuf);
		return status;
	}
	switch (*pattern) {
		case 0 : 
			for (j=0;j<(*length)/2;j++) {
				writebuf[2*j]   = (unsigned short) (j&&0x00FFFF00);
				writebuf[2*j+1] = (unsigned short) (j&&0x000000FF);
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
			g200_md_free(readbuf);
			g200_md_free(writebuf);
			return status;
	}

/* Write the Pattern to the block of memory */

	if((status=dxp_write_block(ioChan,
							   modChan,
							   addr,
							   length,
							   writebuf))!=DXP_SUCCESS) {
		dxp_log_error("dxp_test_mem"," ",status);
		g200_md_free(readbuf);
		g200_md_free(writebuf);
		return status;
	}

/* Read the memory back and compare */
	
	if((status=dxp_read_block(ioChan,
							  modChan,
							  addr,
							  length,
							  readbuf))!=DXP_SUCCESS) {
		dxp_log_error("dxp_test_mem"," ",status);
		g200_md_free(readbuf);
		g200_md_free(writebuf);
		return status;
	}
	for(j=0,nerrors=0; j<*length;j++) if (readbuf[j]!=writebuf[j]) {
		nerrors++;
		status = DXP_MEMERROR;
		if(nerrors<10){
			sprintf(info_string, "Error: word %d, wrote %x, read back %x",
				j,((unsigned int) writebuf[j]),((unsigned int) readbuf[j]));
			dxp_log_error("dxp_test_mem",info_string,status);
		}
	}
	
	if (nerrors!=0){
		sprintf(info_string, "%d memory compare errors found",nerrors);
		dxp_log_error("dxp_test_mem",info_string,status);
		g200_md_free(readbuf);
		g200_md_free(writebuf);
		return status;
	}

	g200_md_free(readbuf);
	g200_md_free(writebuf);
	return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Set a parameter of the DSP.  Pass the symbol name, value to set and module
 * pointer and channel number.
 *
 ******************************************************************************/
static int dxp_modify_dspsymbol(int* ioChan, int* modChan, char* name, 
				unsigned short* value, Dsp_Info* dsp)
/* int *ioChan;			Input: IO channel to write to			*/
/* int *modChan;		Input: Module channel number to write to	*/
/* char *name;			Input: user passed name of symbol		*/
/* unsigned short *value;	Input: Value to set the symbol to	        */
/* Dsp_Info *dsp;		Input: Relavent DSP info			*/
{

  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];
  unsigned int i;
  unsigned short addr;		/* address of the symbol in DSP memory		*/
  char uname[20]="";		/* Upper case version of the user supplied name */
	
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
/* Check the lower bound */
    if (*value<dsp->params->parameters[addr].lbound) {
      sprintf(info_string, "Value is below the lower acceptable bound %i<%i. Changing to lower bound.", 
	      *value, dsp->params->parameters[addr].lbound);
      status = DXP_DSPPARAMBOUNDS;
      dxp_log_error("dxp_modify_dspsymbol", info_string, status);
/* Set to the lower bound */
      *value = dsp->params->parameters[addr].lbound;
    }
/* Check the upper bound */
    if (*value>dsp->params->parameters[addr].ubound) {
      sprintf(info_string, "Value is above the upper acceptable bound %i<%i. Changing to upper bound.", 
	      *value, dsp->params->parameters[addr].ubound);
      status = DXP_DSPPARAMBOUNDS;
      dxp_log_error("dxp_modify_dspsymbol", info_string, status);
/* Set to the upper bound */
      *value = dsp->params->parameters[addr].ubound;
    }
  }

/* Write the value of the symbol into DSP memory */
	
  if((status=dxp_write_dsp_param_addr(ioChan,modChan,&(dsp->params->parameters[addr].address),
				      value))!=DXP_SUCCESS){
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
/* int *ioChan;					Input: IO channel to write to				*/
/* int *modChan;				Input: Module channel number to write to		*/
/* unsigned int *addr;				Input: address to write in DSP memory		*/
/* unsigned short *value;			Input: Value to set the symbol to			*/
{

	int status=DXP_SUCCESS;
	char info_string[INFO_LEN];

	unsigned short saddr;
	
/* Move the address into Parameter memory.  The passed address is relative to 
 * base memory. */
	
	saddr = (unsigned short) (*addr + startp);

	if((status=dxp_write_word(ioChan,modChan,&saddr,value))!=DXP_SUCCESS){
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
							  Dsp_Info* dsp, double* value)
/* int *ioChan;					Input: IO channel to write to				*/
/* int *modChan;				Input: Module channel number to write to	*/
/* char *name;					Input: user passed name of symbol			*/
/* Dsp_Info *dsp;				Input: Reference to the DSP structure		*/
/* unsigned long *value;		Output: Value to set the symbol to			*/
{

  int status=DXP_SUCCESS;
  char info_string[INFO_LEN];
  unsigned int i;
  unsigned short nword = 1;   /* How many words does this symbol contain?		*/
  unsigned short addr=0;		/* address of the symbol in DSP memory			*/
  unsigned short addr1=0;		/* address of the 2nd word in DSP memory		*/
  unsigned short addr2=0;		/* address of the 3rd word (REALTIME/LIVETIME)	*/
  char uname[30]="", tempchar[30]="";	/* Upper case version of the user supplied name */
  unsigned short stemp;		/* Temp location to store word read from DSP	*/
  double dtemp, dtemp1;     /* double versions for the temporary variable		*/

/* Check that the length of the name is less than maximum allowed length */

  if (strlen(name) > (dsp->params->maxsymlen)) 
	{
	  status = DXP_NOSYMBOL;
	  sprintf(info_string, "Symbol Name must be <%i characters", dsp->params->maxsymlen);
	  dxp_log_error("dxp_read_dspsymbol", info_string, status);
	  return status;
	}

/* Convert the name to upper case for comparison */
  
  for (i = 0; i < strlen(name); i++) uname[i] = (char) toupper(name[i]); 
  
/* Be paranoid and check if the DSP configuration is downloaded.  If not, warn the user */
	
  if ((dsp->proglen) <= 0) 
	{
	  status = DXP_DSPLOAD;
	  sprintf(info_string, "Must Load DSP code before reading %s", name);
	  dxp_log_error("dxp_read_dspsymbol",info_string,status);
	  return status;
	}
    
/* First find the location of the symbol in DSP memory. */

  status = dxp_loc(uname, dsp, &addr);
  if (status != DXP_SUCCESS)
	{
	  /* Failed to find the name directly, add 0 to the name and search again */
	  strcpy(tempchar, uname);
	  sprintf(tempchar, "%sA0",uname);
	  nword = 2;

	  status = dxp_loc(tempchar, dsp, &addr);
	  if (status != DXP_SUCCESS) 
		{
		  /* Failed to find the name with A0 attached, check for symbol without the trailing 0 */
		  strcpy(tempchar, uname);
		  sprintf(tempchar, "%sA",uname);

		  status = dxp_loc(tempchar, dsp, &addr);
		  if (status != DXP_SUCCESS) 
			{
			  /* Failed to find the name with A0 attached, check for symbol without the trailing 0 */
			  sprintf(info_string, "Failed to find symbol %s in DSP memory", name);
			  dxp_log_error("dxp_read_dspsymbol", info_string, status);
			  return status;
			} else {
			  /* Search for the 2nd entry now */
			  sprintf(tempchar, "%sB",uname);

			  status = dxp_loc(tempchar, dsp, &addr1);
			  if (status != DXP_SUCCESS) 
				{
				  /* Failed to find the name with 1 attached, this symbol doesnt exist */
				  sprintf(info_string, "Failed to find symbol %s+1 in DSP memory", name);
				  dxp_log_error("dxp_read_dspsymbol", info_string, status);
				  return status;
				}
			  /* Check for 48 bit numbers */
			  sprintf(tempchar, "%sC",uname);

			  status = dxp_loc(tempchar, dsp, &addr2);
			  if (status == DXP_SUCCESS) 
				{
				  nword = 3;
				}
			}
		} else {
		  /* Search for the 2nd entry now */
		  sprintf(tempchar, "%sB0",uname);

		  status = dxp_loc(tempchar, dsp, &addr1);
		  if (status != DXP_SUCCESS)
			{
			  /* Failed to find the name with 1 attached, this symbol doesnt exist */
			  sprintf(info_string, "Failed to find symbol %s+1 in DSP memory", name);
			  dxp_log_error("dxp_read_dspsymbol", info_string, status);
			  return status;
			}
		  /* Check for 48 bit numbers */
		  sprintf(tempchar, "%sC0",uname);
		  status = dxp_loc(tempchar, dsp, &addr2);
		  if (status == DXP_SUCCESS) 
			{
			  nword = 3;
			}
		}
	}
  
/* Check the access type for this parameter.  Only allow reading from r/w and ro 
 * parameters.
 */
  
  if (dsp->params->parameters[addr].access == 2) 
	{
	  sprintf(info_string, "Parameter %s is Write-Only.  No peeking allowed.", name);
	  status = DXP_DSPACCESS;
	  dxp_log_error("dxp_read_dspsymbol", info_string, status);
	  return status;
	}

/* Read the value of the symbol from DSP memory */

  addr = (unsigned short) (dsp->params->parameters[addr].address + startp);

  status = dxp_read_word(ioChan, modChan, &addr, &stemp);
  if (status != DXP_SUCCESS)
	{
	  sprintf(info_string, "Error writing parameter %s", name);
	  dxp_log_error("dxp_read_dspsymbol",info_string,status);
	  return status;
	}
  dtemp = (double) stemp;
  
/* If there is a second word, read it in */
  if (nword == 1) 
	{
	  /* For single word values, assign it for the user */
	  *value = dtemp;
	} else if (nword == 2) {
	  addr = (unsigned short) (dsp->params->parameters[addr1].address + startp);
	  status = dxp_read_word(ioChan, modChan, &addr, &stemp);
	  if (status != DXP_SUCCESS)
		{
		  sprintf(info_string, "Error writing parameter %s+1", name);
		  dxp_log_error("dxp_read_dspsymbol",info_string,status);
		  return status;
		}
	  dtemp1 = (double) stemp;
	  /* For 2 words, create the proper 32 bit number */
	  *value = dtemp*65536. + dtemp1;
	} else if (nword == 3) {
	  /* Special case of 48 Bit numbers */
	  addr = (unsigned short) (dsp->params->parameters[addr1].address + startp);
	  status = dxp_read_word(ioChan, modChan, &addr, &stemp);
	  if (status != DXP_SUCCESS)
		{
		  sprintf(info_string, "Error writing parameter %s+1", name);
		  dxp_log_error("dxp_read_dspsymbol",info_string,status);
		  return status;
		}
	  dtemp1 = (double) stemp;
	  /* now read 3rd number */
	  addr = (unsigned short) (dsp->params->parameters[addr2].address + startp);
	  status = dxp_read_word(ioChan, modChan, &addr, &stemp);
	  if (status != DXP_SUCCESS)
		{
		  sprintf(info_string, "Error writing parameter %s+2", name);
		  dxp_log_error("dxp_read_dspsymbol",info_string,status);
		  return status;
		}
	  /* For 3 words, create the proper 48 bit number */
	  *value = dtemp*65536.*65536. + dtemp1*65536. + ((double) stemp);
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
static int dxp_read_dspparams(int* ioChan, int* modChan, Dsp_Info* dsp, 
							  unsigned short* params)
/* int *ioChan;						Input: I/O channel of DSP		*/
/* int *modChan;					Input: module channel of DSP		*/
/* Dsp_Info *dsp;					Input: Relavent DSP info			*/
/* unsigned short *params;			Output: array of DSP parameters	*/
{
	int status=DXP_SUCCESS;
	char info_string[INFO_LEN];
	unsigned int i, len;		/* Number of DSP symbols */

	/* The parameter space on the G200 is fixed */
	unsigned short memory[416];

	/* Readout a block of parameters, then pick out our values from the block.  
	 * The memory map of the G200 is not continguous in the DSP, there are arrays, 
	 * which create voids in the offset for parameter names.  So we have to skip 
	 * around in DSP memory when reading these parameters.  However, (for USB) a 
	 * block transfer is MUCH faster than single word transfers, so we must use a 
	 * block rather than many singles (as was done originally). 
	 */
	len = 416;
	if ((status=dxp_read_block(ioChan, modChan, &startp, &len, memory))!=DXP_SUCCESS)
	  {
		sprintf(info_string,"Error reading the block of parameter memory");
		dxp_log_error("dxp_read_dspparams",info_string,status);
		return status;
	  }
	
	
	/* Now pick out the parameters */
	len = dsp->params->nsymbol;
	/* Must read the symbols individually since their location in memory is not 
	 * contiguous 
	 */
	for (i=0;i<len;i++) {
	  /* Check the access type */
	  if (dsp->params->parameters[i].access!=2) {
		params[i] = memory[dsp->params->parameters[i].address];
		/*		addr = (unsigned short) (dsp->params->parameters[i].address);
				if((status=dxp_read_word(ioChan,modChan,&addr,&params[i]))!=DXP_SUCCESS)
				{
				sprintf(info_string,"Error reading parameter number %i",i);
				dxp_log_error("dxp_read_dspparams",info_string,status);
				return status;
		  }
		*/
	  }
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
/* int *ioChan;						Input: I/O channel of DSP		*/
/* int *modChan;					Input: module channel of DSP		*/
/* Dsp_Info *dsp;					Input: Relavent DSP info			*/
/* unsigned short *params;			Input: array of DSP parameters	*/
{

	int status=DXP_SUCCESS;
	char info_string[INFO_LEN];
	unsigned int i, len;		/* Number of DSP symbols */
	
	/* The parameter space on the G200 is fixed */
	unsigned short memory[416];

	/* Readout a block of parameters, then replace our values in the block.  
	 * The memory map of the G200 is not continguous in the DSP, there are arrays, 
	 * which create voids in the offset for parameter names.  So we have to skip 
	 * around in DSP memory when reading these parameters.  However, (for USB) a 
	 * block transfer is MUCH faster than single word transfers, so we must use a 
	 * block rather than many singles (as was done originally). 
	 */
	len = 416;
	if ((status=dxp_read_block(ioChan, modChan, &startp, &len, memory))!=DXP_SUCCESS)
	  {
		sprintf(info_string,"Error reading the block of parameter memory");
		dxp_log_error("dxp_write_dspparams",info_string,status);
		return status;
	  }
	
	
	len = dsp->params->nsymbol;
/* Must write the symbols individually since their location in memory is not 
 * contiguous 
 */
	for (i=0;i<len;i++) {
/* Check the access type */
		if (dsp->params->parameters[i].access!=0) {
		  memory[dsp->params->parameters[i].address] = params[i];
		  /*			addr = (unsigned short) (dsp->params->parameters[i].address + startp);
						if((status=dxp_write_word(ioChan,modChan,&addr,&params[i]))!=DXP_SUCCESS){
						sprintf(info_string,"Error writing parameter number %i",i);
						dxp_log_error("dxp_write_dspparams",info_string,status);
						return status;
			}
		  */
		}
	}

	/* Now write the block back, this makes sure not to modify any other memory
	 * than the parameters.
	 */
	if ((status=dxp_write_block(ioChan, modChan, &startp, &len, memory))!=DXP_SUCCESS)
	  {
		sprintf(info_string,"Error writing the block of parameter memory");
		dxp_log_error("dxp_write_dspparams",info_string,status);
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
static unsigned int dxp_get_spectrum_length(Dsp_Info* dsp, unsigned short* params)
/* Dsp_Info *dsp;                          Input: Relavent DSP info	*/
/* unsigned short *params;	                Input: Array of DSP parameters	*/
{
  int status;
  
  unsigned short addr;
  unsigned int binfact, nspec;
  
/* Get the binning factor for the G200 */
  if((status=dxp_loc("LOG2EBIN0", dsp, &addr))!=DXP_SUCCESS) {
    status = DXP_NOSYMBOL;
    dxp_log_error("dxp_get_spectrum_length","Unable to find LOG2EBIN0 symbol",status);
    return 0;
  }
  /* Check for special case of 0 */
  if (params[addr] == 0) {
    nspec = 65536;
  } else {
    binfact = (int) pow(2.,(double) ((65536-params[addr])&0xFFFF));
    nspec = 65536/binfact;
  }
  
  return nspec;
}

/******************************************************************************
 * Routine to return the length of the baseline memory.
 *
 * For 4C-2X boards, this value is stored in the DSP and dynamic.  
 * For 4C boards, it is fixed.
 * 
 ******************************************************************************/
static unsigned int dxp_get_baseline_length(Dsp_Info* dsp, unsigned short* params)
/* Dsp_Info *dsp;                     Input: Relavent DSP info		*/
/* unsigned short *params;            Input: Array of DSP parameters	*/
{
	unsigned short ustemp;
	Dsp_Info *dsptemp;

	dsptemp = dsp;
	ustemp = *params;

/* There is no baseline memory in the DGF line */

	return 0;
}

/******************************************************************************
 *
 * Routine to return the length of the event buffer.
 *
 ******************************************************************************/
static unsigned int dxp_get_event_length(Dsp_Info* dsp, unsigned short* params)
/* Dsp_Info *dsp;					Input: Relavent DSP info			*/
/* unsigned short *params;			Input: Array of DSP parameters	*/
{
  int status;
  unsigned short addr;
  
  if((status=dxp_loc("LOUTBUFFER", dsp, &addr)) != DXP_SUCCESS)
    {
      dxp_log_error("dxp_get_event_length",
		    "Unable to find LOUTBUFFER symbol",status);
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
/* int *ioChan;				Input: I/O channel of DSP	*/
/* int *modChan;			Input: module channel of DSP	*/
/* Board *board;			Input: Relevent Board info	*/
/* unsigned short *params;		Input: Array of DSP parameters	*/
/* unsigned long *spectrum;		Output: array of spectrum values*/
{

  int status;
  unsigned int i;
  unsigned int slen, ct_len;		/* number of short words in spectrum and buffer	*/
  /* current position of writing data to the spectrum array */
  unsigned int cur_pos=0, copy_end;
  short type;
  unsigned int ilen;
  
  long *ct_data;
  float timeout;
  
  int info[20];
  
  slen = dxp_get_spectrum_length(board->dsp[*modChan], board->params[*modChan]);
/* 
 * To read the spectrum from a G200, one must perform 8 control tasks and 
 * read the data from the ouptut buffer in DSP memory. 
 */
/* Perform the READ_MEMORY_FIRST control task, then the READ_MEMORY_NEXT thereafter */
	type = CT_DGFG200_READ_MEMORY_FIRST;
	do {
/* First retrieve the required length of the control task data */
		if((status=dxp_control_task_params(ioChan,modChan,&type,board,info))!=DXP_SUCCESS){
			dxp_log_error("dxp_read_spectrum","Error retrieving control task information",status);
			return status;
		}
/* Allocate memory for this array of information */
		ct_len = info[0];
		ct_data = (long *) g200_md_alloc(ct_len*sizeof(long));

/* Set the timeout for the control task to 1.0 seconds */
		timeout = 1.0;
/* Pass only one value, the number of loops, to the routine */
		ilen = 2;
		info[0] = 1;
		info[1] = 1;
		if((status=dxp_do_control_task(ioChan,modChan,&type,board,&ilen,info,
										&timeout,ct_data))!=DXP_SUCCESS){
			dxp_log_error("dxp_read_spectrum","Error performing control task",status);
			g200_md_free(ct_data);
			return status;
		}

/* Now pack the data retrieved into the callers output array */
		if ((cur_pos + ((int) (ct_len/2))) < slen) {
/* copy the whole buffer */
			copy_end = cur_pos + ((int) (ct_len/2));
		} else {
/* copy only the portion of the buffer that fills the remaining part of the spectrum */
			copy_end = slen;
		}
		for (i=cur_pos;i<copy_end;i++) {
			spectrum[i] = (unsigned long) ((ct_data[2*(i-cur_pos)+1]&0xFF)<<16);
			spectrum[i] += ((unsigned long) (ct_data[2*(i-cur_pos)]&0xFFFF));
		}

/* Done with the array, free the memory */
		g200_md_free(ct_data);
		cur_pos += ((int) (ct_len/2));
		type = CT_DGFG200_READ_MEMORY_NEXT;
	} while (cur_pos<slen);
	
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
							 unsigned short* baseline)
/* int *ioChan;						Input: I/O channel of DSP					*/
/* int *modChan;					Input: module channel of DSP					*/
/* Board *board;					Input: Relevent Board info		*/
/* unsigned short *params;			Input: Array of DSP parameters				*/
/* unsigned short *baseline;		Output: array of baseline histogram values	*/
{
	int status = DXP_SUCCESS;
	
	int *itemp;
	unsigned short *ustemp;
	Board *btemp;

	itemp = ioChan;
	itemp = modChan;
	btemp = board;
	ustemp = baseline;

/* There is no baseline buffer in the DGF line */

	return status;
}
	
/******************************************************************************
 * Routine to readout the event buffer from a single DSP.
 * 
 * This routine reads the event buffer from the DSP pointed to by ioChan and
 * modChan.  It returns the array to the caller.
 *
 ******************************************************************************/
static int dxp_read_event(int* ioChan, int* modChan, Board* board, unsigned short* event)
/* int *ioChan;				Input: I/O channel of DSP		*/
/* int *modChan;			Input: module channel of DSP		*/
/* Board *board;			Input: Relevent Board info		*/
/* unsigned short *params;		Input: Array of DSP parameters		*/
/* unsigned short *event;		Output: array of history buffervalues	*/
{

  int status;
  unsigned short addr, start;
  unsigned int len;					/* number of short words in spectrum	*/
  
  if((status=dxp_loc("AOUTBUFFER", board->dsp[*modChan], &addr))!=DXP_SUCCESS) 
    {
      status = DXP_NOSYMBOL;
      dxp_log_error("dxp_read_event","Unable to find AOUTBUFFER symbol",status);
      return status;
    }
  start = (unsigned short) (board->params[*modChan][addr]);
  len = dxp_get_event_length(board->dsp[*modChan], board->params[*modChan]);

  /* Read out the event buffer. */
  if((status=dxp_read_block(ioChan,modChan,&start,&len,event)) != DXP_SUCCESS)
    {
      dxp_log_error("dxp_read_event","Error reading out event buffer",status);
      return status;
    }

  return status;
}

/******************************************************************************
 * Routine to readout the event buffer from a single DSP.
 * 
 * This routine reads the event buffer from the DSP pointed to by ioChan and
 * modChan.  It returns the array to the caller.
 *
 ******************************************************************************/
static int dxp_write_event(int* ioChan, int* modChan, Board* board, 
						   unsigned int *len, unsigned short* event)
/* int *ioChan;						Input: I/O channel of DSP					*/
/* int *modChan;					Input: module channel of DSP				*/
/* Board *board;					Input: Relevent Board info					*/
/* unsigned short *params;			Input: Array of DSP parameters				*/
/* unsigned int *len;				Output: length of the array to write		*/
/* unsigned short *event;			Output: array of history buffervalues		*/
{

	int status;
	unsigned short addr, start;

	if((status=dxp_loc("AOUTBUFFER", board->dsp[*modChan], &addr))!=DXP_SUCCESS) {
		status = DXP_NOSYMBOL;
		dxp_log_error("dxp_write_event","Unable to find EVTBSTART symbol",status);
		return status;
	}
	start = (unsigned short) (board->params[*modChan][addr]);

/* Read out the basline histogram. */

	if((status=dxp_write_block(ioChan,modChan,&start,len,event))!=DXP_SUCCESS){
		dxp_log_error("dxp_write_event","Error reading out event buffer",status);
		return status;
	}

	return status;
}

/******************************************************************************
 * Routine to perform a control task.
 *
 * This routine will perform all needed overhead to perform a control
 * task.  It will start a run, wait for run to end, end the run, then read
 * the data pertaining to the special run.  The caller is responsible for
 * allocating memory for the data.
 *
 ******************************************************************************/
static int dxp_do_control_task(int *ioChan, int *modChan, short *type,
			       Board *board, unsigned int *len,
			       int *idata, float *timeout, long *odata)
     /* int *ioChan;			Input: I/O channel of DSP	*/
     /* int *modChan;			Input: module channel of DSP	*/
     /* short *type;			Input: type of control task to perform	*/
     /* Board *board;			Input: Relavent Board info		*/
     /* unsigned int *ilen;		Input: Length of the input array	*/
     /* int *idata;			Input: Array of input data		*/
     /* float *timeout;			Input: Timeout to wait for control task to end	*/
     /* long *odata;			Output: array of control task output data	*/
{
  int status = DXP_SUCCESS;
  char info_string[INFO_LEN];
  int i;
  float wait, poll, curtime;
  unsigned short csr;
  unsigned int true_len;
  
  int info[20];
  
  if (*modChan == ALLCHAN) 
    {
      dxp_log_error("dxp_do_control_task","Currently Control Tasks on ALLCHAN is not supported",status);
      return status;
    }

  sprintf(info_string, "iochan = %i, modChan = %i, type = %i, len = %i, timeout = %f, idata = %i %i", 
	  *ioChan, *modChan, *type, *len, *timeout, idata[0],idata[1]);
  dxp_log_debug("dxp_do_control_task", info_string);

  for (i=0;i<idata[0];i++) {
    if((status=dxp_control_task_params(ioChan,modChan,type,board,info))!=DXP_SUCCESS)
      {
	dxp_log_error("dxp_do_control_task","Error retrieving control task information",status);
	return status;
      }

    poll = (float) (((double) info[2])/1000.);
    /* Because of the way polling is performed, more convenient to calculate wait this way */
    wait = ((float) (((double) info[1])/1000.)) - poll;

  sprintf(info_string, "iochan = %i, modChan = %i, type = %i, len = %i, timeout = %f, idata = %i %i", 
	  *ioChan, *modChan, *type, *len, *timeout, idata[0],idata[1]);
  dxp_log_debug("dxp_do_control_task", info_string);

    /* Start the control task */
    true_len = *len-1;
    if((status=dxp_begin_control_task(ioChan,modChan,type,&true_len,idata+1,board))!=DXP_SUCCESS){
      dxp_log_error("dxp_do_control_task","Error starting control task",status);
			return status;
    }
    
  sprintf(info_string, "iochan = %i, modChan = %i, type = %i, len = %i, timeout = %f, idata = %i %i", 
	  *ioChan, *modChan, *type, *len, *timeout, idata[0],idata[1]);
  dxp_log_debug("dxp_do_control_task", info_string);

    /* Wait for the control task to end by checking the RUN_ACTIVE bit of the CSR */
    g200_md_wait(&wait);
    curtime = wait;
    do {
      g200_md_wait(&poll);
      curtime += poll;
      if (curtime>*timeout) {
	status = DXP_DSPTIMEOUT;
	sprintf(info_string,"Control task took more than %f seconds to finish",*timeout);
	dxp_log_error("dxp_do_control_task",info_string,status);
	return status;
      }
      if((status=dxp_read_csr(ioChan,&csr))!=DXP_SUCCESS){
	dxp_log_error("dxp_do_control_task","Error reading the CSR active bit",status);
	return status;
      }
  sprintf(info_string, "csr = 0x%x", csr);
  dxp_log_debug("dxp_do_control_task", info_string);
    } while ((csr & MASK_RUN_ACTIVE) != 0);
    
  sprintf(info_string, "iochan = %i, modChan = %i, type = %i, len = %i, timeout = %f, idata = %i %i", 
	  *ioChan, *modChan, *type, *len, *timeout, idata[0],idata[1]);
  dxp_log_debug("dxp_do_control_task", info_string);

    /* Stop the control task after the G200 ends control task */
    if((status=dxp_end_control_task(ioChan,modChan,board))!=DXP_SUCCESS)
      {
	dxp_log_error("dxp_do_control_task","Error ending control task",status);
	return status;
      }
    
  sprintf(info_string, "iochan = %i, modChan = %i, type = %i, len = %i, timeout = %f, idata = %i %i", 
	  *ioChan, *modChan, *type, *len, *timeout, idata[0],idata[1]);
  dxp_log_debug("dxp_do_control_task", info_string);

    /* Readout data from the results of the control task */
    if((status=dxp_control_task_data(ioChan,modChan,type,board,odata))!=DXP_SUCCESS)
      {
	dxp_log_error("dxp_do_control_task","Error reading data from control task",status);
	return status;
      }
  }
  
  return status;

}
/******************************************************************************
 * Routine to prepare the G200 for data readout.
 *
 * This routine will ensure that the board is in a state that
 * allows readout via CAMAC bus.
 *
 ******************************************************************************/
int dxp_prep_for_readout(int* ioChan, int *modChan)
/* int *ioChan;						Input: I/O channel of DXP module			*/
/* int *modChan;					Input: Module channel number				*/
{
	int *itemp;

/* Assign input parameters to avoid compiler warnings */
	itemp = ioChan;
	itemp = modChan;

/*
 *   Nothing needs to be done for the G200, the 
 * CAMAC interface does not interfere with normal data taking.
 */

	return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to prepare the G200 for data readout.
 * 
 * This routine will ensure that the board is in a state that 
 * allows readout via CAMAC bus.
 *
 ******************************************************************************/
int dxp_done_with_readout(int* ioChan, int *modChan, Board* board)
/* int *ioChan;						Input: I/O channel of DXP module	*/
/* int *modChan;					Input: Module channel number		*/
/* Board *board;					Input: Relavent Board info					*/
{
	int *itemp;
	Board *btemp;

/* Assign input parameters to avoid compiler warnings */
	itemp = ioChan;
	itemp = modChan;
	btemp = board;

/*
 * Nothing needs to be done for the G200, the CAMAC interface 
 * does not interfere with normal data taking.
 */

	return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to begin a data taking run.
 * 
 * This routine starts a run on the specified EPP channel.  It tells the G200
 * whether to ignore the gate signal and whether to clear the MCA.
 *
 ******************************************************************************/
static int dxp_begin_run(int* ioChan, int* modChan, unsigned short* gate, 
						 unsigned short* resume, Board *board)
/* int *ioChan;						Input: I/O channel of DXP module		*/
/* int *modChan;					Input: Module channel number			*/
/* unsigned short *gate;			Input: ignore (1) or use (0) ext. gate	*/
/* unsigned short *resume;			Input: clear MCA first(0) or update (1)	*/
/* Board *board;					Input: Board information				*/
{
/*
 *   Initiate data taking for all channels of a single DXP module. 
 */
	int status;
	unsigned short data;
	double dtemp;
	char info_string[INFO_LEN];

	/* Set the parameter that controls if the gate input is used to veto
	 * signals, if gate == NULL, then skip setting the CHANCSRA0 */
	if (gate != NULL) 
	  {
		status = dxp_read_dspsymbol(ioChan, modChan, "CHANCSRA0", board->dsp[*modChan], &dtemp);
		if (status != DXP_SUCCESS) 
		  {
			status = DXP_NOSYMBOL;
			dxp_log_error("dxp_begin_run", "Unable to read CHANCSRA0 symbol",status);
			return status;
		  }
		
		data = (unsigned short) dtemp;
		if (*gate==IGNOREGATE) 
		  {
			data &= ~CCSRA_GATE;
		  } else {
			data |= CCSRA_GATE;
		  }

		status = dxp_modify_dspsymbol(ioChan, modChan, "CHANCSRA0", &data, board->dsp[*modChan]);
		if (status != DXP_SUCCESS)
		  {
			status = DXP_NOSYMBOL;
			dxp_log_error("dxp_begin_run", "Unable to write CHANCSRA0 symbol",status);
			return status;
		  }
	  }
	
	/* RMW the CSR to start data run */
	status = dxp_read_csr(ioChan, &data);
	if (status!=DXP_SUCCESS)
	  {
	    dxp_log_error("dxp_begin_run","Error reading the CSR",status);
	    return status;
	  }
	
	data |= MASK_RUNENABLE;
	if (*resume==CLEARMCA) 
	  {
	    data |= MASK_RESETMCA;
	  } else {
	    data &= ~MASK_RESETMCA;
	  }
	
	sprintf(info_string, "Writing 0x%x to the CSR", data);
	dxp_log_debug("dxp_begin_run", info_string);
	
	/* write the CSR back */
	status = dxp_write_csr(ioChan, &data);
	if (status!=DXP_SUCCESS)
	  {
	    dxp_log_error("dxp_begin_run","Error writing CSR",status);
	    return status;
	  }

	return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to end a data taking run.
 * 
 * This routine ends the run on the specified CAMAC channel.
 *
 ******************************************************************************/
static int dxp_end_run(int* ioChan, int* modChan)
/* int *ioChan;						Input: I/O channel of DXP module	*/
/* int *modChan;					Input: Module channel number		*/
{
/*
 *   Terminate a data taking for all channels of a single DXP module. 
 */
  int status;
  unsigned short data;
  
  status = dxp_read_csr(ioChan, &data);                    /* read from CSR */
  if (status!=DXP_SUCCESS) 
    {
      dxp_log_error("dxp_end_run", "Error reading CSR", status);
    }
  
  data &= ~MASK_RUNENABLE;
  
  status = dxp_write_csr(ioChan, &data);                    /* write to CSR */
  if (status!=DXP_SUCCESS) 
    {
      dxp_log_error("dxp_end_run","Error writing CSR",status);
    }
  
  if((status=dxp_clear_LAM(ioChan, modChan))!=DXP_SUCCESS) 
    {
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
/* int *ioChan;						Input: I/O channel of DXP module			*/
/* int *modChan;					Input: module channel number of DXP module	*/
/* int *active;						Output: Does the module think a run is active? */
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
	if ((data & MASK_RUN_ACTIVE)!=0) *active = 1;

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
     /* int *ioChan;		Input: I/O channel of DXP module		*/
     /* int *modChan;		Input: module channel number of DXP module	*/
     /* short *type;		Input: type of control task to perfomr		*/
     /* int *length;		Input: Length of the config info array		*/
     /* int *info;		Input: Configuration info for the task		*/
     /* Board *board;		Input: Board data				*/
{
  int status = DXP_SUCCESS;
  char info_string[INFO_LEN];
  
  unsigned int i;
  
  /* Variables for parameters to be read/written from/to the DSP */
  unsigned short runtask, controltask, xwait, unuseda, blcut, threshold;
  
  unsigned short *ustemp;
  unsigned short zero=0;

  double dtemp;
  
  /* Check that the length of allocated memory is greater than 0 */
  if (*length==0) {
    status = DXP_ALLOCMEM;
    sprintf(info_string,
	    "Must pass an array of at least length 1 containing LOOPCOUNT for module %d chan %d",
	    board->mod,*modChan);
    dxp_log_error("dxp_begin_control_task",info_string,status);
    return status;
  }

  /* Retrieve the current value of RUNTASK and store it in the chanstate array, 
   * we will use the value when the control task is ended */
  if((status = dxp_read_dspsymbol(ioChan,modChan,
								  "RUNTASK",board->dsp[*modChan],&dtemp)) != DXP_SUCCESS)
    {
      sprintf(info_string,
			  "Error reading RUNTASK from module %d chan %d",board->mod,*modChan);
      dxp_log_error("dxp_begin_control_task",info_string,status);
      return status;
    }
  /* Now store the value */
  board->chanstate[*modChan].parameterTemp = (unsigned short) dtemp;
  
  /* Write the RUNTASK parameter */
  runtask = CONTROL_TASK_RUN;
  /* write */
  if((status=dxp_modify_dspsymbol(ioChan,modChan,
								  "RUNTASK",&runtask,board->dsp[*modChan]))!=DXP_SUCCESS){
    sprintf(info_string,
			"Error writing RUNTASK from module %d chan %d",board->mod,*modChan);
    dxp_log_error("dxp_begin_control_task",info_string,status);
    return status;
  }
  
  /* First check if the control task is the ADC readout */
  if (*type==CT_DGFG200_SETDACS) {
    controltask = CONTROLTASK_SETDACS;
  } else if (*type==CT_DGFG200_CLOSE_INPUT_RELAY) {
    controltask = CONTROLTASK_CLOSE_INPUT_RELAY;
  } else if (*type==CT_DGFG200_OPEN_INPUT_RELAY) {
    controltask = CONTROLTASK_OPEN_INPUT_RELAY;
  } else if ((*type==CT_DGFG200_RAMP_OFFSET_DAC) || (*type==CT_DGFG200_ISOLATED_RAMP_OFFSET_DAC)) {
    if (*type == CT_DGFG200_RAMP_OFFSET_DAC) 
      {
		controltask = CONTROLTASK_RAMP_OFFSET_DAC;
      } else {
		controltask = CONTROLTASK_ISOLATED_RAMP_OFFSET_DAC;
      }
	
    
    /* Set a couple parameters important for ramping the offset DAC
     * these parameters are overwritten, higher level routines must
     * restore these values (because of the begin-end sequence needed
     * here */
    /* Max the threshold */
    threshold = TRIGGER_THRESHOLD_MAX;
    if((status=dxp_modify_dspsymbol(ioChan,modChan, "FASTTHRESH0", &threshold,
				    board->dsp[*modChan])) != DXP_SUCCESS)
      {
		sprintf(info_string,
				"Error writing FASTTHRESH0 to module %d chan %d",board->mod,*modChan);
		dxp_log_error("dxp_begin_control_task",info_string,status);
		return status;
      }
	
    /* Disable the Baseline cut routine */
    blcut = 0;
    if((status=dxp_modify_dspsymbol(ioChan,modChan, "BLCUT0", &blcut,
									board->dsp[*modChan])) != DXP_SUCCESS)
      {
		sprintf(info_string,
				"Error writing BLCUT0 to module %d chan %d",board->mod,*modChan);
		dxp_log_error("dxp_begin_control_task",info_string,status);
		return status;
      }

  } else if ((*type==CT_ADC) || (*type==CT_DGFG200_ADC)) {
    controltask = CONTROLTASK_ADC_TRACE;
    /* Make sure at least the user thinks there is allocated memory */
    if (*length>1) {
      /* write XWAIT */
      xwait = (unsigned short) info[1];
      /* do a little extra bounds checking and calculate unusedA */
      unuseda = 65535;
      if (xwait <= 3) 
		{
		  xwait = 3;
		} else if (xwait > 10) { 
		  xwait = (unsigned short) (4 * floor((xwait - 3) / 4) + 3);
		  unuseda = (unsigned short) floor(65536 / ((xwait - 3) / 4));
		}
      /* Let the user know the new value */
      info[1] = (int) xwait;
	  
      /* set the values on the board */
      if((status=dxp_modify_dspsymbol(ioChan,modChan, "XWAIT0",&xwait,
									  board->dsp[*modChan]))!=DXP_SUCCESS){
		sprintf(info_string,
				"Error writing XWAIT0 to module %d chan %d",board->mod,*modChan);
		dxp_log_error("dxp_begin_control_task",info_string,status);
		return status;
      }
      if((status=dxp_modify_dspsymbol(ioChan,modChan, "UNUSEDA0",&unuseda,
									  board->dsp[*modChan]))!=DXP_SUCCESS){
		sprintf(info_string,
				"Error writing UNUSEDA to module %d chan %d",board->mod,*modChan);
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
  } else if (*type==CT_DGFG200_PROGRAM_FIPPI) {
    controltask = CONTROLTASK_PROGRAM_FIPPI;
  } else if (*type==CT_DGFG200_READ_MEMORY_FIRST) {
    controltask = CONTROLTASK_READ_MEMORY_FIRST;
  } else if (*type==CT_DGFG200_READ_MEMORY_NEXT) {
    controltask = CONTROLTASK_READ_MEMORY_NEXT;
  } else if ((*type==CT_DGFG200_WRITE_MEMORY_FIRST)||(*type==CT_DGFG200_WRITE_MEMORY_NEXT)) {
    if (*type==CT_DGFG200_WRITE_MEMORY_FIRST) {
      controltask = CONTROLTASK_WRITE_MEMORY_FIRST;
    } else {
      controltask = CONTROLTASK_WRITE_MEMORY_NEXT;
    }
    /* convert the info array to unsigned short */
    ustemp = (unsigned short *) g200_md_alloc(*length*sizeof(unsigned short));
    for (i=1;i<(unsigned int)(*length+1);i++) ustemp[i] = (unsigned short)info[i];
    /* Write the event buffer */
    if((status=dxp_write_event(ioChan,modChan,board,length,ustemp))!=DXP_SUCCESS){
      sprintf(info_string,
	      "Error writing the event buffer to module %d chan %d",board->mod,*modChan);
      dxp_log_error("dxp_begin_control_task",info_string,status);
      g200_md_free(ustemp);
      return status;
    }
    g200_md_free(ustemp);
  } else if (*type==CT_DGFG200_MEASURE_NOISE) {
    controltask = CONTROLTASK_MEASURE_NOISE;
  } else if (*type==CT_DGFG200_ADC_CALIB_FIRST) {
    controltask = CONTROLTASK_ADC_CALIB_FIRST;
  } else if (*type==CT_DGFG200_ADC_CALIB_NEXT) {
    controltask = CONTROLTASK_ADC_CALIB_NEXT;
  } else {
    status=DXP_NOCONTROLTYPE;
    sprintf(info_string,
	    "Unknown control type %d for this DGFG200 module",*type);
    dxp_log_error("dxp_begin_control_task",info_string,status);
    return status;
  }
  
  /* write CONTROLTASK */
  if((status=dxp_modify_dspsymbol(ioChan,modChan,
				  "CONTROLTASK",&controltask,board->dsp[*modChan]))!=DXP_SUCCESS){
    sprintf(info_string,
	    "Error writing CONTROLTASK to module %d chan %d",board->mod,*modChan);
    dxp_log_error("dxp_begin_control_task",info_string,status);
    return status;
  }

  /* start the control run */
  if((status=dxp_begin_run(ioChan, modChan, NULL, &zero, board))!=DXP_SUCCESS){
    sprintf(info_string,
	    "Error starting control run on module %d chan %d",board->mod,*modChan);
    dxp_log_error("dxp_begin_control_task",info_string,status);
    return status;
  }

  return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Routine to end a control task routine.
 *
 ******************************************************************************/
static int dxp_end_control_task(int* ioChan, int* modChan, Board *board)
/* int *ioChan;						Input: I/O channel of DXP module			*/
/* int *modChan;					Input: module channel number of DXP module	*/
/* Board *board;					Input: Board data							*/
{
  int status = DXP_SUCCESS;
  char info_string[INFO_LEN];
  double temp;
  unsigned short runtask;
  
  if ((status = dxp_end_run(ioChan,modChan)) != DXP_SUCCESS)
    {
      sprintf(info_string,
	      "Error ending control task run for chan %d",*modChan);
      dxp_log_error("dxp_end_control_task",info_string,status);
      return status;
    }

/* Read/Modify/Write the RUNTASK parameter */
/* read */
  status = dxp_read_dspsymbol(ioChan, modChan, "RUNTASK", board->dsp[*modChan], &temp);
  if (status != DXP_SUCCESS)
    {
      sprintf(info_string,
			  "Error reading RUNTASK from module %d chan %d",board->mod,*modChan);
      dxp_log_error("dxp_end_control_task",info_string,status);
      return status;
    }

  runtask = (unsigned short) temp;
  if (runtask == CONTROL_TASK_RUN) 
    {
      /* Set the RUNTASK parameter back to the value it had before the control task was executed */
      runtask = board->chanstate[*modChan].parameterTemp;
      /* write */
      status = dxp_modify_dspsymbol(ioChan, modChan, "RUNTASK", &runtask, board->dsp[*modChan]);
	  if (status != DXP_SUCCESS)
		{
		  sprintf(info_string,
				  "Error writing RUNTASK from module %d chan %d",board->mod,*modChan);
		  dxp_log_error("dxp_end_control_task",info_string,status);
		  return status;
		}
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
/* int *ioChan;			Input: I/O channel of DXP module		*/
/* int *modChan;		Input: module channel number of DXP module	*/
/* short *type;			Input: type of control task to perfomr		*/
/* Board *board;		Input: Board data				*/
/* int info[20];		Output: Configuration info for the task		*/
{
  int status = DXP_SUCCESS;
  char info_string[INFO_LEN];
  
/* Keep the compiler happy. Complains about ioChan not being used. */
  int idummy;
  idummy = *ioChan;

/* Default values */
/* nothing to readout here */
  info[0] = 0;
/* set a minimal wait and delay */
  info[1] = 1;
  info[2] = 1;

  /* Check the control task type */
  if (*type==CT_DGFG200_SETDACS) 
    {
    } else if (*type==CT_DGFG200_CLOSE_INPUT_RELAY) {
    } else if (*type==CT_DGFG200_OPEN_INPUT_RELAY) {
    } else if ((*type==CT_DGFG200_RAMP_OFFSET_DAC) || (*type==CT_DGFG200_ISOLATED_RAMP_OFFSET_DAC)) {
      /* length=event buffer length*/
      /*      info[0] = dxp_get_event_length(board->dsp[*modChan], board->params[*modChan]);*/
      info[0] = 2048;
      /* Recommend waiting 166ms initially */
      info[1] = 166;
      /* Recomment 166ms polling after initial wait */
      info[2] = 166;
    } else if ((*type==CT_ADC) || (*type==CT_DGFG200_ADC)) {
      /* length=spectrum length*/
      info[0] = dxp_get_event_length(board->dsp[*modChan], board->params[*modChan]);
      /* Recommend waiting 4ms initially, 400ns*spectrum length */
      info[1] = 4;
      /* Recomment 1ms polling after initial wait */
      info[2] = 1;
    } else if (*type==CT_DGFG200_PROGRAM_FIPPI) {
      /* No information is returned, set a 1ms wait and poll times */
      info[0] = 0;
      info[1] = 1;
      info[2] = 1;
    } else if (*type==CT_DGFG200_READ_MEMORY_FIRST) {
      /* length=event buffer length */
      info[0] = dxp_get_event_length(board->dsp[*modChan], board->params[*modChan]);
      /* Recommend waiting 4ms initially, 400ns*event buffer length*/
      info[1] = (int) (.0005*info[0]);
      /* Recomment 1ms polling after initial wait */
      info[2] = 1;
    } else if (*type==CT_DGFG200_READ_MEMORY_NEXT) {
      /* length=event buffer length */
      info[0] = dxp_get_event_length(board->dsp[*modChan], board->params[*modChan]);
      /* Recommend waiting 4ms initially, 400ns*event buffer length*/
      info[1] = (int) (.0005*info[0]);
      /* Recomment 1ms polling after initial wait */
      info[2] = 1;
    } else if (*type==CT_DGFG200_WRITE_MEMORY_FIRST) {
      /* length=event buffer length */
      info[0] = dxp_get_event_length(board->dsp[*modChan], board->params[*modChan]);
      /* Recommend waiting 4ms initially, 400ns*event buffer length*/
      info[1] = (int) (.0005*info[0]);
      /* Recomment 1ms polling after initial wait */
      info[2] = 1;
    } else if (*type==CT_DGFG200_WRITE_MEMORY_NEXT) {
      /* length=event buffer length */
      info[0] = dxp_get_event_length(board->dsp[*modChan], board->params[*modChan]);
      /* Recommend waiting 4ms initially, 400ns*event buffer length*/
      info[1] = (int) (.0005*info[0]);
      /* Recomment 1ms polling after initial wait */
      info[2] = 1;
    } else if (*type==CT_DGFG200_MEASURE_NOISE) {
    } else if (*type==CT_DGFG200_ADC_CALIB_FIRST) {
    } else if (*type==CT_DGFG200_ADC_CALIB_NEXT) {
    } else {
      status=DXP_NOCONTROLTYPE;
      sprintf(info_string,
	      "Unknown control type %d for this G200 module",*type);
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
/* int *ioChan;				Input: I/O channel of DXP module		*/
/* int *modChan;			Input: module channel number of DXP module	*/
/* short *type;				Input: type of control task to perfomr		*/
/* Board *board;			Input: Board data				*/
/* void *data;				Output: Data read after the control task	*/
{
  int status = DXP_SUCCESS;
  char info_string[INFO_LEN];
  
  unsigned int i, lene;
  unsigned short *stemp;
  unsigned short *stemp2048;
  unsigned long *ltemp;
  
/* Check the control task type */
  if (*type==CT_DGFG200_SETDACS) 
    {
    } else if (*type==CT_DGFG200_CLOSE_INPUT_RELAY) {
    } else if (*type==CT_DGFG200_OPEN_INPUT_RELAY) {
    } else if ((*type==CT_ADC) || (*type==CT_DGFG200_ADC) ||
	       (*type==CT_DGFG200_READ_MEMORY_FIRST) || (*type==CT_DGFG200_READ_MEMORY_NEXT) ||
	       (*type==CT_DGFG200_RAMP_OFFSET_DAC) || (*type==CT_DGFG200_ISOLATED_RAMP_OFFSET_DAC)) 
      {
	/* allocate memory */
	lene = dxp_get_event_length(board->dsp[*modChan],board->params[*modChan]);
	stemp = (unsigned short *) g200_md_alloc(lene * sizeof(unsigned short));
	if (stemp == NULL) {
	  status = DXP_NOMEM;
	  sprintf(info_string,
		  "Not enough memory to allocate temporary array of length %d", lene);
	  dxp_log_error("dxp_control_task_data", info_string, status);
	  return status;
	}
	
	if((status = dxp_read_event(ioChan, modChan, board, stemp)) != DXP_SUCCESS)
	  {
	    g200_md_free(stemp);
	    sprintf(info_string,
		    "Error ending control task run for chan %d", *modChan);
	    dxp_log_error("dxp_control_task_data", info_string, status);
	    return status;
	  }
      
	/* Only want the first 2048 entries for the RAMP OFFSET DAC task */
	if ((*type != CT_DGFG200_RAMP_OFFSET_DAC) && (*type != CT_DGFG200_ISOLATED_RAMP_OFFSET_DAC))
	  {
	    /* Assign the void *data to a local unsigned long * array for assignment purposes.  Just easier to read than
	     * casting the void * on the left hand side */
	    ltemp = (unsigned long *) data;
	    for (i=0;i<lene;i++) ltemp[i] = (unsigned long) stemp[i];
	  } else {
	    stemp2048 = (unsigned short *) data;
	    for (i=0; i < 2048; i++) stemp2048[i] = stemp[i];
	  }
	/* Free memory */
	g200_md_free(stemp);
	
    } else if (*type==CT_DGFG200_PROGRAM_FIPPI) {
    } else if (*type==CT_DGFG200_MEASURE_NOISE) {
    } else if (*type==CT_DGFG200_WRITE_MEMORY_FIRST) {
    } else if (*type==CT_DGFG200_WRITE_MEMORY_NEXT) {
    } else if (*type==CT_DGFG200_ADC_CALIB_FIRST) {
    } else if (*type==CT_DGFG200_ADC_CALIB_NEXT) {
    } else {
      status=DXP_NOCONTROLTYPE;
      sprintf(info_string,
	      "Unknown control type %d for this DGFG200 module",*type);
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
/* int *ioChan;					Input: I/O channel of DXP module	*/
/* int *modChan;				Input: Module channel number		*/
/* int *calib_task;				Input: int.gain (1) reset (2)		*/
/* Board *board;				Input: Relavent Board info			*/
{
/*
 */
	int status;
	char info_string[INFO_LEN];
	unsigned short resume=CLEARMCA;
	unsigned short controltask= (unsigned short) *calib_task;
	unsigned short runtask=CONTROL_TASK_RUN;

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
	
	
	if((status=dxp_modify_dspsymbol(ioChan, modChan,
									"CONTROLTASK", &controltask, board->dsp[*modChan]))!=DXP_SUCCESS) {    
		dxp_log_error("dxp_begin_calibrate","Error writing CONTROLTASK",status);
		return status;
	}

	if((status=dxp_modify_dspsymbol(ioChan, modChan,
									"RUNTASK",&runtask, board->dsp[*modChan]))!=DXP_SUCCESS) {    
       dxp_log_error("dxp_begin_calibrate","Error writing RUNTASK",status);
       return status;
	}

/* Finally startup the Calibration run. */

	if((status = dxp_begin_run(ioChan, modChan, NULL, &resume, board))!=DXP_SUCCESS){
       dxp_log_error("dxp_begin_calibrate","Unable to start the calibration run",status);
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
/* unsigned short array[];		Input: array from parameter block read	*/
/* Dsp_Info *dsp;				Input: Relavent DSP info					*/
/* unsigned short *runerror;	Output: runerror word					*/
/* unsigned short *errinfo;		Output: errinfo word						*/
{

    unsigned short *ustemp;
	Dsp_Info *dtemp;

/* Use inputs to avoid compiler warnings */
	ustemp = array;
	dtemp = dsp;
    
	*runerror = 0;
    *errinfo = 0;
    return DXP_SUCCESS;

}

/******************************************************************************
 * Routine to clear an error in the DSP
 * 
 * Clears non-fatal DSP error in one or all channels of a single DXP module.
 * If modChan is -1 then all channels are cleared on the DXP.
 *
 ******************************************************************************/
static int dxp_clear_error(int* ioChan, int* modChan, Dsp_Info* dsp)
/* int *ioChan;					Input: I/O channel of DXP module		*/
/* int *modChan;				Input: DXP channels no (-1,0,1,2,3)	*/
/* Dsp_Info *dsp;				Input: Relavent DSP info				*/
{

    int status;
	char info_string[INFO_LEN];
    unsigned short zero;

/* Be paranoid and check if the DSP configuration is downloaded.  If not, do it */
	
	if ((dsp->proglen)<=0) {
		status = DXP_DSPLOAD;
		sprintf(info_string, "Must Load DSP code before clearing errors");
		dxp_log_error("dxp_clear_error", info_string, status);
		return status;
	}

	zero=0;
    status = dxp_modify_dspsymbol(ioChan, modChan, "RUNERROR", 
									&zero, dsp);
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
/* int *calibtest;		Input: Calibration test performed	*/
/* unsigned short *params;	Input: parameters read from the DSP	*/
/* Dsp_Info *dsp;		Input: Relavent DSP info		*/
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
static int dxp_get_runstats(unsigned short array[], Dsp_Info* dsp, 
							unsigned int* evts, unsigned int* under, 
							unsigned int* over, unsigned int* fast, 
							unsigned int* base, double* live,
							double* icr, double* ocr)
/* unsigned short array[];	Input: array from parameter block   */
/* Dsp_Info *dsp;		Input: Relavent DSP info	    */
/* unsigned int *evts;		Output: number of events in spectrum*/
/* unsigned int *under;		Output: number of underflows        */
/* unsigned int *over;		Output: number of overflows         */
/* unsigned int *fast;		Output: number of fast filter events*/
/* unsigned int *base;		Output: number of baseline events   */
/* double *live;		    Output: livetime in seconds         */
/* double *icr;			    Output: Input Count Rate in kHz     */
/* double *ocr;			    Output: Output Count Rate in kHz    */
{

  unsigned short addr[3]={USHRT_MAX, USHRT_MAX, USHRT_MAX};
  int status=DXP_SUCCESS;
  
  /* Temporary values from the DSP code */
  double real = 0.;
  unsigned long nEvents = 0;
  double clock_tick = 0.;
  double liveclock_tick = 0.;
  
  /* Use unsigned long temp variables since we will be bit shifting 
   * by more than an unsigned short or int in the routine */
  unsigned long temp0, temp1, temp2;
  
  /* Now retrieve the location of DSP parameters and fill variables */

  /* Retrieve the clock speed of this board */
  status = dxp_loc("SYSMICROSEC", dsp, &addr[0]);
  /* If we succeed in retrieving the parameter, then use it, else
   * default to 20 MHz 
   */
  if (status == DXP_SUCCESS) 
    {
      liveclock_tick = 16.e-6 / ((double) array[addr[0]]);
      clock_tick = 1.e-6 / ((double) array[addr[0]]);
    } else {
      liveclock_tick = 16.e-6 / 40.;
      clock_tick = 1.e-6 / 40.;
    }
  
  /* Events in the run */
  status = dxp_loc("NUMEVENTSA", dsp, &addr[0]);
  status += dxp_loc("NUMEVENTSB", dsp, &addr[1]);
  temp0 = (unsigned long) array[addr[1]];
  temp1 = (unsigned long) array[addr[0]];
  *evts = (unsigned int) (temp0 + temp1*65536.);

/* Underflows in the run */
  status += dxp_loc("UNDERFLOWA0", dsp, &addr[0]);
  status += dxp_loc("UNDERFLOWB0", dsp, &addr[1]);
  temp0 = (unsigned long) array[addr[1]];
  temp1 = (unsigned long) array[addr[0]];
  *under = (unsigned int) (temp0 + temp1*65536.);

/* Overflows in the run */
  status += dxp_loc("OVERFLOWA0", dsp, &addr[0]);
  status += dxp_loc("OVERFLOWB0", dsp, &addr[1]);
  temp0 = (unsigned long) array[addr[1]];
  temp1 = (unsigned long) array[addr[0]];
  *over = (unsigned int) (temp0 + temp1*65536.);

/* Fast Peaks in the run */
  status += dxp_loc("FASTPEAKSA0", dsp, &addr[0]);
  status += dxp_loc("FASTPEAKSB0", dsp, &addr[1]);
  temp0 = (unsigned long) array[addr[1]];
  temp1 = (unsigned long) array[addr[0]];
  *fast = (unsigned int) (temp0 + temp1*65536.);

  /* Baseline Events in the run */
  *base = 0;

  /* Livetime for the run */
  status += dxp_loc("LIVETIMEA0", dsp, &addr[0]);
  status += dxp_loc("LIVETIMEB0", dsp, &addr[1]);
  status += dxp_loc("LIVETIMEC0", dsp, &addr[2]);
  temp0 = (unsigned long) array[addr[2]];
  temp1 = (unsigned long) array[addr[1]];
  temp2 = (unsigned long) array[addr[0]];
  *live = ((double) (temp0 + temp1*65536. + temp2*65536.*65536.)) * liveclock_tick;
  
  /* Realtime for the run */
  status += dxp_loc("RUNTIMEA", dsp, &addr[0]);
  status += dxp_loc("RUNTIMEB", dsp, &addr[1]);
  status += dxp_loc("RUNTIMEC", dsp, &addr[2]);
  temp0 = (unsigned long) array[addr[2]];
  temp1 = (unsigned long) array[addr[1]];
  temp2 = (unsigned long) array[addr[0]];
  real = ((double) (temp0 + temp1*65536. + temp2*65536.*65536.)) * clock_tick;
  
  /* Calculate the number of events in the run */
  
  nEvents = *evts + *under + *over;
  
  /* Calculate the event rates */
  if(*live > 0.)
	{
	  *icr = 0.001 * (*fast) / (*live);
	} else {
	  *icr = -999.;
	}
  if(real > 0.)
	{
	  *ocr = 0.001 * nEvents / real;
	} else {
	  *ocr = -999.;
	}

  if (status != DXP_SUCCESS)
	{
	  status = DXP_NOSYMBOL;
	  dxp_log_error("dxp_get_runstats",	"Unable to find 1 or more parameters",status);
	  return status;
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
/* float *gainchange;					Input: desired gain change           */
/* unsigned short *old_gaindac;			Input: current gaindac				*/
/* short *delta_gaindac;				Input: required gaindac change       */
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
							float* gainchange, Dsp_Info* dsp)
/* int *ioChan;					Input: IO channel of desired channel			*/
/* int *modChan;				Input: channel on module						*/
/* int *module;					Input: module number: for error reporting	*/
/* float *gainchange;			Input: desired gain change					*/
/* Dsp_Info *dsp;				Input: Relavent DSP info					*/
{
    int status;
	char info_string[INFO_LEN];
    unsigned short gaindac, fast_threshold, slow_threshold;
	double dtemp;
	short delta_gaindac;
	unsigned long lgaindac;		/* Use a long to test bounds of new GAINDAC */

/* Read out the GAINDAC setting from the DSP. */
            
	status = dxp_read_dspsymbol(ioChan, modChan, "GAINDAC0", dsp, &dtemp);
	if (status != DXP_SUCCESS)
	  {
		sprintf(info_string,
				"Error reading GAINDAC0 from mod %d chan %d",*module,*modChan);
		dxp_log_error("dxp_change_gains",info_string,status);
        return status;
	  }
	gaindac = (unsigned short) dtemp;

/* Read out the fast peak FASTTHRESH0 setting from the DSP. */
            
	status = dxp_read_dspsymbol(ioChan, modChan, "FASTTHRESH0", dsp, &dtemp);
	if (status != DXP_SUCCESS)
	  {
        sprintf(info_string,
				"Error reading FASTTHRESH0 from mod %d chan %d",*module,*modChan);
		dxp_log_error("dxp_change_gains",info_string,status);
        return status;
	  }
	fast_threshold = (unsigned short) dtemp;

/* Read out the slow peak FASTADCTHR0 setting from the DSP. */
            
	status=dxp_read_dspsymbol(ioChan, modChan, "FASTADCTHR0", dsp, &dtemp);
	if (status != DXP_SUCCESS)
	  {
        sprintf(info_string,
				"Error reading FASTADCTHR0 from mod %d chan %d",*module,*modChan);
		dxp_log_error("dxp_change_gains",info_string,status);
        return status;
	  }
	slow_threshold = (unsigned short) dtemp;

/* Calculate the delta GAINDAC in bits. */
            
	status = dxp_perform_gaincalc(gainchange, &gaindac, &delta_gaindac);
	if (status != DXP_SUCCESS)
	  {
		sprintf(info_string,
				"DXP module %d Channel %d", *module, *modChan);
		dxp_log_error("dxp_change_gains",info_string,status);
		status=DXP_SUCCESS;
	  }
	
/* Now do some bounds checking.  The variable gain amplifier is only capable of
 * -6 to 30 dB. Do this by subracting instead of adding to avoid bounds of 
 * short integers */

	lgaindac = ((unsigned long) gaindac) + delta_gaindac;
	if ((lgaindac <= ((GAINDAC_MIN + 10.) * dacpergaindb)) ||
		(lgaindac >= ((GAINDAC_MAX + 10.) * dacpergaindb))) 
	  {
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

	if (fast_threshold != 0) 
		fast_threshold = (unsigned short) ((*gainchange) * ((double) fast_threshold) + 0.5);
	if (slow_threshold != 0)
		slow_threshold = (unsigned short) ((*gainchange) * ((double) slow_threshold) + 0.5);

/* Download the GAINDAC, THRESHOLD and SLOWTHRESH back to the DSP. */
			
	status = dxp_modify_dspsymbol(ioChan, modChan, "GAINDAC0", &gaindac, dsp);
	if (status != DXP_SUCCESS)
	  {
		sprintf(info_string,
				"Error writing GAINDAC0 to mod %d chan %d", *module, *modChan);
		dxp_log_error("dxp_change_gains",info_string,status);
        return status;
	  }
	
	status = dxp_modify_dspsymbol(ioChan, modChan, "FASTTHRESH0", &fast_threshold, dsp);
	if (status != DXP_SUCCESS)
	  {
		sprintf(info_string,
				"Error writing FASTTHRESH0 to mod %d chan %d", *module, *modChan);
		dxp_log_error("dxp_change_gains",info_string,status);
        return status;
	  }
	
	status = dxp_modify_dspsymbol(ioChan, modChan, "FASTADCTHR0", &slow_threshold, dsp);
	if (status != DXP_SUCCESS)
	  {
        sprintf(info_string,
				"Error writing FASTADCTHR0 to mod %d chan %d", *module, *modChan);
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
						 float* vmax, float* vstep, Dsp_Info* dsp)
/* int *ioChan;					Input: IO channel number					*/
/* int *modChan;				Input: Module channel number				*/
/* int *module;					Input: Module number: error reporting	*/
/* float *adcRule;				Input: desired ADC rule					*/
/* float *gainmod;				Input: desired finescale gain adjustment */
/* unsigned short *polarity;	Input: polarity of channel				*/
/* float *vmin;					Input: minimum voltage range of channel	*/
/* float *vmax;					Input: maximum voltage range of channel	*/
/* float *vstep;				Input: average step size of single pulse	*/
/* Dsp_Info *dsp;				Input: Relavent DSP info					*/
{
	char info_string[INFO_LEN];
	double gain;
	int status;
	unsigned short gaindac;
	double dtemp;
	unsigned short chancsr;
	
	double g_input = GINPUT;					/* Input attenuator Gain	*/
	double g_input_buff = GINPUT_BUFF;			/* Input buffer Gain		*/
	double g_inverting_amp = GINVERTING_AMP;	/* Inverting Amp Gain		*/
	double g_v_divider = GV_DIVIDER;			/* V Divider after Inv AMP	*/
	double g_gaindac_buff = GGAINDAC_BUFF;		/* GainDAC buffer Gain		*/
	double g_nyquist = GNYQUIST;				/* Nyquist Filter Gain		*/
	double g_adc_buff = GADC_BUFF;				/* ADC buffer Gain			*/
	double g_adc = GADC;						/* ADC Input Gain			*/
	double g_system = 0.;						/* Total gain imposed by ASC
												   not including the GAINDAC*/
	double g_desired = 0.;						/* Desired gain for the 
												   GAINDAC */
	float *ftemp;

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

	if ((g_desired <= GAINDAC_MIN) || (g_desired >= GAINDAC_MAX)) 
	  {
		sprintf(info_string,
				"Required gain of %f (dB) that is out of range for the ASC", g_desired);
		status=DXP_DETECTOR_GAIN;
        dxp_log_error("dxp_setup_asc",info_string,status);
		return status;
	  }

/* Gain is within limits.  Determine the setting for the 16-bit DAC */
	
	gaindac = (short) ((g_desired + 10.0) * dacpergaindb);

/* Write the GAINDAC data back to the DXP module. */
		
	status = dxp_modify_dspsymbol(ioChan, modChan, "GAINDAC0", &gaindac, dsp);
	if (status != DXP_SUCCESS)
	  {
		sprintf(info_string,
				"Error writting GAINDAC0 to module %d channel %d",
				*module,*modChan);
		dxp_log_error("dxp_setup_asc",info_string,status);
		return status;
	  }
/*
 *    Download POLARITY
 */
    status = dxp_read_dspsymbol(ioChan, modChan, "CHANCSRA0", dsp, &dtemp);
	if (status != DXP_SUCCESS)
	  {
		sprintf(info_string,
				"Error writting CHANCSRA0 to module %d channel %d",
				*module,*modChan);
		dxp_log_error("dxp_setup_asc",info_string,status);
		return status;
	  }

	chancsr = (unsigned short) dtemp;
	if (*polarity == 1) 
	  {
		chancsr |= CCSRA_POLARITY;
	  } else {
		chancsr &= ~CCSRA_POLARITY;
	  }
	
	status = dxp_modify_dspsymbol(ioChan, modChan, "CHANCSRA0", &chancsr, dsp);
	if (status != DXP_SUCCESS)
	  {
		sprintf(info_string, "Error writting CHANCSRA0 to module %d channel %d",
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
/* int *mod;					Input: Camac Module number to calibrate			*/
/* int *camChan;				Input: Camac pointer							*/
/* unsigned short *used;		Input: bitmask of channel numbers to calibrate	*/
/* Board *board;				Input: Relavent Board info						*/
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
 *    o save the current value of RUNTASK for each channel
 *    o start run
 *    o wait
 *    o stop run
 *    o readout parameter memory
 *    o check for errors, clear errors if set
 *    o check calibration results..
 *    o restore RUNTASK for each channel
 *
 ******************************************************************************/
static int dxp_calibrate_channel(int* mod, int* camChan, unsigned short* used, 
								 int* calibtask, Board *board)
/* int *mod;						Input: Camac Module number to calibrate			*/
/* int *camChan;					Input: Camac pointer							*/
/* unsigned short *used;			Input: bitmask of channel numbers to calibrate	*/
/* int *calibtask;					Input: which calibration function				*/
/* Board *board;					Input: Relavent Board info						*/
{

    int status=DXP_SUCCESS,status2,chan;
	char info_string[INFO_LEN];
    unsigned short runtask;
    float one_second=1.0;
    unsigned short addr_RUNTASK=USHRT_MAX;
    unsigned short runerror,errinfo;
	
	unsigned short params[MAXSYM];
	double dtemp;

/*
 *  Loop over each channel and save the RUNTASK word.  Then start the
 *  calibration run
 */
    for(chan=0;chan<4;chan++){
		if(((*used)&(1<<chan))==0) continue;

/* Grab the addresses in the DSP for some symbols. */
    
		status = dxp_loc("RUNTASK", board->dsp[chan], &addr_RUNTASK);
		if(status!=DXP_SUCCESS){
			status = DXP_NOSYMBOL;
			dxp_log_error("dxp_calibrate_channel","Unable to locate RUNTASK",status);
			return status;
		}

		status2 = dxp_read_dspsymbol(camChan, &chan, "RUNTASK", board->dsp[chan], &dtemp);
		if (status2 != DXP_SUCCESS)
		  {
			sprintf(info_string,"Error reading RUNTASK from mod %d chan %d",
					*mod,chan);
			dxp_log_error("dxp_calibrate_channel",info_string,status2);
			return status2;
		  }
		runtask = (unsigned short) dtemp;

		status2 = dxp_begin_calibrate(camChan, &chan, calibtask, board);
		if (status2 != DXP_SUCCESS)
		  {
			sprintf(info_string,"Error beginning calibration for mod %d",*mod);
			dxp_log_error("dxp_calibrate_channel",info_string,status2);
			return status2;
		  }
/*
 *   wait a second, then stop the runs
 */
	    g200_md_wait(&one_second);
		if((status2=dxp_end_run(camChan, &chan))!=DXP_SUCCESS){
			dxp_log_error("dxp_calibrate_channel",
					"Unable to end calibration run",status2);
			return status2;
		}
/*
 *  Loop over each of the channels and read out the parameter memory. Check
 *  to see if there were errors.  Finally, restore the original value of 
 *  RUNTASK
 */

/* Read out the parameter memory into the Params array */
            
		if((status2=dxp_read_dspparams(camChan, &chan, board->dsp[chan], params))!=DXP_SUCCESS){
			sprintf(info_string,
				"error reading parameters for mod %d chan %d",*mod,chan);
			dxp_log_error("dxp_calibrate_channel",info_string,status2);
			return status2;
		}

/* Check for errors reported by the DSP. */

		if((status2=dxp_decode_error(params, board->dsp[chan], &runerror, &errinfo))!=DXP_SUCCESS){
			dxp_log_error("dxp_calibrate_channel","Unable to decode errors",status2);
			return status2;
		}
		if(runerror!=0){
			status+=DXP_DSPRUNERROR;
			sprintf(info_string,"DSP error detected for mod %d chan %d",
				*mod,chan);
			dxp_log_error("dxp_calibrate_channel",info_string,status);
			if((status2=dxp_clear_error(camChan, &chan, board->dsp[chan]))!=DXP_SUCCESS){
				sprintf(info_string,
					"Unable to clear error for mod %d chan %d",*mod,chan);
				dxp_log_error("dxp_calibrate_channel",info_string,status2);
				return status2;
			}
		}

/* Call the primitive routine that checks the calibration to ensure
 * that all went well.  The results depend on the calibration performed. */

		if((status += dxp_check_calibration(calibtask, params, board->dsp[chan]))!=DXP_SUCCESS){
			sprintf(info_string,"Calibration Error: mod %d chan %d",
				*mod,chan);
			dxp_log_error("dxp_calibrate_channel",info_string,status);
		}


/* Now write back the value previously stored in RUNTASK */            
			
		if((status2=dxp_modify_dspsymbol(camChan, &chan, "RUNTASK", 
							&runtask, board->dsp[chan]))!=DXP_SUCCESS){
			sprintf(info_string,"Error writing RUNTASK to mod %d chan %d",
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
/* const char *filename;			Input: filename to open			*/
/* const char *mode;				Input: Mode to use when opening	*/
{
	FILE *fp=NULL;
	char *name=NULL, *name2=NULL;
	char *home=NULL;

/* Try to open file directly */
	if((fp=fopen(filename,mode))!=NULL){
		return fp;
	}
/* Try to open the file with the path XIAHOME */
	if ((home=getenv("XIAHOME"))!=NULL) {
		name = (char *) g200_md_alloc(sizeof(char)*
							(strlen(home)+strlen(filename)+2));
		sprintf(name, "%s/%s", home, filename);
		if((fp=fopen(name,mode))!=NULL){
			g200_md_free(name);
			return fp;
		}
		g200_md_free(name);
		name = NULL;
	}
/* Try to open the file with the path DXPHOME */
	if ((home=getenv("DXPHOME"))!=NULL) {
		name = (char *) g200_md_alloc(sizeof(char)*
							(strlen(home)+strlen(filename)+2));
		sprintf(name, "%s/%s", home, filename);
		if((fp=fopen(name,mode))!=NULL){
			g200_md_free(name);
			return fp;
		}
		g200_md_free(name);
		name = NULL;
	}
/* Try to open the file as an environment variable */
	if ((name=getenv(filename))!=NULL) {
		if((fp=fopen(name,mode))!=NULL){
			return fp;
		}
		name = NULL;
	}
/* Try to open the file with the path XIAHOME and pointing 
 * to a file as an environment variable */
	if ((home=getenv("XIAHOME"))!=NULL) {
		if ((name2=getenv(filename))!=NULL) {
		
			name = (char *) g200_md_alloc(sizeof(char)*
								(strlen(home)+strlen(name2)+2));
			sprintf(name, "%s/%s", home, name2);
			if((fp=fopen(name,mode))!=NULL){
				g200_md_free(name);
				return fp;
			}
			g200_md_free(name);
			name = NULL;
		}
	}
/* Try to open the file with the path DXPHOME and pointing 
 * to a file as an environment variable */
	if ((home=getenv("DXPHOME"))!=NULL) {
		if ((name2=getenv(filename))!=NULL) {
		
			name = (char *) g200_md_alloc(sizeof(char)*
								(strlen(home)+strlen(name2)+2));
			sprintf(name, "%s/%s", home, name2);
			if((fp=fopen(name,mode))!=NULL) {
				g200_md_free(name);
				name = NULL;
				return fp;
			}
			g200_md_free(name);
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
 * This routine currently does nothing.
 **********/
XERXES_STATIC int dxp_read_mem(int *ioChan, int *modChan, Board *board,
							   char *name, unsigned long *base, unsigned long *offset,
							   unsigned long *data)
{
  UNUSED(ioChan);
  UNUSED(modChan);
  UNUSED(board);
  UNUSED(name);
  UNUSED(data);
  UNUSED(base);
  UNUSED(offset);

  return DXP_UNIMPLEMENTED;
}


XERXES_STATIC int dxp_write_mem(int *ioChan, int *modChan, Board *board,
								char *name, unsigned long *base, unsigned long *offset,
								unsigned long *data)
{
  UNUSED(ioChan);
  UNUSED(modChan);
  UNUSED(board);
  UNUSED(name);
  UNUSED(data);
  UNUSED(base);
  UNUSED(offset);

  return DXP_UNIMPLEMENTED;
}


/**********
 * This routine does nothing currently.
 **********/
static int dxp_write_reg(int *ioChan, int *modChan, char *name, unsigned short *data)
{
  int status = DXP_SUCCESS;

  unsigned short saddr;
  unsigned long addr;
  
  unsigned long i;

  char info_string[INFO_LEN];

  /* convert to lower case */
  for (i = 0; i < strlen(name); i++) name[i] = (char) tolower(name[i]);
  
  /* Parse the name for known values */
  if (STREQ(name, "csr"))
    {
      status = dxp_write_csr(ioChan, data);
    } else if (STREQ(name, "tsar")) {
      status = dxp_write_tsar(ioChan, data);
    } else {
      /* This is the general register value, parse out the address */
      addr = strtoul(name, NULL, 16);
      if (addr > USHRT_MAX)
	{
	  status = DXP_BAD_ADDRESS;
	} else {
	  saddr = (unsigned short) addr;
	  status = dxp_write_word(ioChan, modChan, &saddr, data);
	}
    }

  if (status != DXP_SUCCESS) 
    {
      sprintf(info_string,"Error writing register: name=%s, ioChan=%i, modChan=%i", name, *ioChan, *modChan);
      dxp_log_error("dxp_write_reg", info_string, status);
      return status;
    }

  return status;
}


/**********
 * This routine currently does nothing.
 **********/
XERXES_STATIC int dxp_read_reg(int *ioChan, int *modChan, char *name, unsigned short *data)
{
  int status = DXP_SUCCESS;

  unsigned short saddr;
  unsigned long addr;
  
  unsigned long i;

  char info_string[INFO_LEN];

  /* convert to lower case */
  for (i = 0; i < strlen(name); i++) name[i] = (char) tolower(name[i]);
  
  /* Parse the name for known values */
  if (STREQ(name, "csr"))
    {
      status = dxp_read_csr(ioChan, data);
    } else if (STREQ(name, "wcr")) {
      status = dxp_read_wcr(ioChan, data);
    } else {
      /* This is the general register value, parse out the address */
      addr = strtoul(name, NULL, 16);
      if (addr > USHRT_MAX)
	{
	  status = DXP_BAD_ADDRESS;
	} else {
	  saddr = (unsigned short) addr;
	  status = dxp_read_word(ioChan, modChan, &saddr, data);
	}
    }

  if (status != DXP_SUCCESS) 
    {
      sprintf(info_string,"Error reading register: name=%s, ioChan=%i, modChan=%i", name, *ioChan, *modChan);
      dxp_log_error("dxp_read_reg", info_string, status);
      return status;
    }

  return status;
}


/**********
 * This routine does nothing currently.
 **********/
XERXES_STATIC int XERXES_API dxp_do_cmd(int *ioChan, byte_t cmd, unsigned int lenS,
					byte_t *send, unsigned int lenR, byte_t *receive,
					byte_t ioFlags)
{
    UNUSED(ioChan);
    UNUSED(cmd);
    UNUSED(lenS);
    UNUSED(send);
    UNUSED(lenR);
    UNUSED(receive);
    UNUSED(ioFlags);
    
    return DXP_SUCCESS;
}


/**********
 * Calls the interface close routine.
 **********/
XERXES_STATIC int XERXES_API dxp_unhook(Board *board)
{
    int status;

    
    status = board->iface->funcs->dxp_md_close(0);

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
  ASSERT(dsp != NULL);
  ASSERT(params != NULL);

  return 0;
}


/** @brief Read out the SCA data buffer from the board
 *
 */
XERXES_STATIC int XERXES_API dxp_read_sca(int *ioChan, int *modChan, Board* board, unsigned long *sca)
{
  ASSERT(ioChan != NULL);
  ASSERT(modChan != NULL);
  ASSERT(board != NULL);
  ASSERT(sca != NULL);


  return DXP_SUCCESS;
}
