/*
 * dxp4c.c
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
#include <dxp4c.h>
#include <xerxes_errors.h>
#include <xia_dxp4c.h>

#include "xia_assert.h"


/* Define the length of the error reporting string info_string */
#define INFO_LEN 400
/* Define the length of the line string used to read in files */
#define LINE_LEN 132

/* Register for poking into X or Y memory of the DSP */
static int xmem=XMEM, ymem=YMEM;
/* Tells the interface on the DXP whether to increment the address during
 * a block transfer of data. */
static int incradd=INCRADD;
/* Shorthand notation telling routines to act on all channels of the DXP (-1 currently). */
static int allChan=ALLCHAN;
/* Starting memory location and length for DSP parameter memory */
static unsigned short startp=START_PARAMS;
/*static unsigned int lenp=MAXSYM;*/
/* The starting memory location and length of spectrum memory in the DSP.
 * MAXSPEC is number of long words  */
static unsigned short starts=START_SPECTRUM;
static unsigned int lens=MAXSPEC; 
/* The starting memory location and length of baseline memory in the DSP. */
static unsigned short startb=START_BASELINE;
static unsigned int lenb=MAXBASE;
/* The starting memory location and length of event buffer memory in the DSP. */
static unsigned short starte=START_EVENT;
static unsigned int lene=MAXEVENT;
/* 
 * Store pointers to the proper DLL routines to talk to the CAMAC crate 
 */
static DXP_MD_IO dxp4c_md_io;
static DXP_MD_SET_MAXBLK dxp4c_md_set_maxblk;
static DXP_MD_GET_MAXBLK dxp4c_md_get_maxblk;
/* 
 * Define the utility routines used throughout this library
 */
static DXP_MD_LOG dxp4c_md_log;
static DXP_MD_ALLOC dxp4c_md_alloc;
static DXP_MD_FREE dxp4c_md_free;
static DXP_MD_PUTS dxp4c_md_puts;
static DXP_MD_WAIT dxp4c_md_wait;

/******************************************************************************
 *
 * Routine to create pointers to all the internal routines
 * 
 ******************************************************************************/
int dxp_init_dxp4c(Functions* funcs)
/* Functions *funcs;					*/
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
    dxp4c_md_io = iface->funcs->dxp_md_io;
    dxp4c_md_set_maxblk = iface->funcs->dxp_md_set_maxblk;
    dxp4c_md_get_maxblk = iface->funcs->dxp_md_get_maxblk;

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
    dxp4c_md_log   = utils->funcs->dxp_md_log;

#ifdef XIA_SPECIAL_MEM
    dxp4c_md_alloc = utils->funcs->dxp_md_alloc;
    dxp4c_md_free  = utils->funcs->dxp_md_free;
#else
    dxp4c_md_alloc = malloc;
    dxp4c_md_free  = free;
#endif /* XIA_SPECIAL_MEM */

    dxp4c_md_free  = utils->funcs->dxp_md_free;
    dxp4c_md_wait  = utils->funcs->dxp_md_wait;
    dxp4c_md_puts  = utils->funcs->dxp_md_puts;

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
/* unsigned short *addr;				Input: address to write into the TSAR */
{

    unsigned int f, a, len;
    int status;

    f=DXP_TSAR_F_WRITE;
    a=DXP_TSAR_A_WRITE;
    len=1;
    status=dxp4c_md_io(ioChan,&f,&a,addr,&len);					/* write TSAR */
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
 * DXP and report the status of the DXP.
 *
 ******************************************************************************/
static int dxp_write_csr(int* ioChan, unsigned short* data)
/* int *ioChan;						Input: I/O channel of DXP module      */
/* unsigned short *data;			Input: address of data to write to CSR*/
{
    unsigned int f, a, len;
    int status;

    f=DXP_CSR_F_WRITE;
    a=DXP_CSR_A_WRITE;
    len=1;
    status=dxp4c_md_io(ioChan,&f,&a,data,&len);					/* write CSR */
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
 * DXP and report the status of the DXP.
 *
 ******************************************************************************/
static int dxp_read_csr(int* ioChan, unsigned short* data)
/* int *ioChan;						Input: I/O channel of DXP module   */
/* unsigned short *data;			Output: where to put data from CSR */
{

    unsigned int f, a, len;
    int status;

    f=DXP_CSR_F_READ;
    a=DXP_CSR_A_READ;
    len=1;
    status=dxp4c_md_io(ioChan,&f,&a,data,&len);					/* write TSAR */
    if (status!=DXP_SUCCESS){
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
/* int *ioChan;							Input: I/O channel of DXP module   */
/* unsigned short *data;				Output: where to put data read     */
/* unsigned int len;					Input: length of the data to read  */
{

    unsigned int f, a;
    int status;

    f=DXP_DATA_F_READ;
    a=DXP_DATA_A_READ;
    status=dxp4c_md_io(ioChan,&f,&a,data,&len);					/* write TSAR */
    if (status!=DXP_SUCCESS){
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
/* int *ioChan;							Input: I/O channel of DXP module   */
/* unsigned short *data;				Input: address of data to write    */
/* unsigned int len;					Input: length of the data to read  */
{

    unsigned int f, a;
    int status;

    f=DXP_DATA_F_WRITE;
    a=DXP_DATA_A_WRITE;
    status=dxp4c_md_io(ioChan,&f,&a,data,&len);					/* write TSAR */
    if (status!=DXP_SUCCESS){
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
/* int *ioChan;							Input: I/O channel of DXP module   */
/* unsigned short *data;				Input: address of data to write    */
/* unsigned int len;					Input: length of the data to read  */
{

    unsigned int f, a;
    int status;

    f=DXP_FIPPI_F_WRITE;
    a=DXP_FIPPI_A_WRITE;
    status=dxp4c_md_io(ioChan,&f,&a,data,&len);					/* write TSAR */
    if (status!=DXP_SUCCESS){
	status = DXP_WRITE_FIPPI;
	dxp_log_error("dxp_write_fippi","Error writing to FiPPi reg",status);
    }
    return status;
}

/******************************************************************************
 * Routine to write DSP program
 * 
 * This is the routine that transfers the DSP program to the DXP.  It 
 * assumes that the CSR is already downloaded with what channel requires
 * a FiPPi program.
 *
 ******************************************************************************/
static int dxp_write_dsp(int* ioChan, unsigned short* data, unsigned int len)
/* int *ioChan;							Input: I/O channel of DXP module   */
/* unsigned short *data;				Input: address of data to write    */
/* unsigned int len;					Input: length of the data to read  */
{

    unsigned int f, a;
    int status;

    f=DXP_DSP_F_WRITE;
    a=DXP_DSP_A_WRITE;
    status=dxp4c_md_io(ioChan,&f,&a,data,&len);					/* write TSAR */
    if (status!=DXP_SUCCESS){
	status = DXP_WRITE_DSP;
	dxp_log_error("dxp_write_dsp","Error writing to DSP reg",status);
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
/* int *ioChan;						Input: I/O channel of DXP module */
/* int *modChan;					Input: DXP channels no (0,1,2,3)      */
{
    int status;
    unsigned int f,a,len;
    unsigned short dummy;
    int *itemp;

    /* Assign the unused inputs to stop compiler warnings */
    itemp = modChan;
    
    f=DXP_ENABLE_LAM_F;
    a=DXP_ENABLE_LAM_A;
    len=1;
    status=dxp4c_md_io(ioChan,&f,&a,&dummy,&len);                    /* enable LAM */
    if (status!=DXP_SUCCESS){
	status=DXP_ENABLE_LAM;
	dxp_log_error("dxp_enable_LAM","Error enabling LAM",status);
    }
    return status;
}

/******************************************************************************
 * Routine to enable the LAMs(Look-At-Me) on the specified DXP
 *
 * Disable the LAM for a single DXP module.
 *
 ******************************************************************************/
static int dxp_ignore_me(int* ioChan, int* modChan)
/* int *ioChan;						Input: I/O channel of DXP module */
/* int *modChan;					Input: DXP channels no (0,1,2,3)      */
{
    int status;
    unsigned int f,a,len;
    unsigned short dummy;
    int *itemp;

    /* Assign the unused inputs to stop compiler warnings */
    itemp = modChan;
    
    f=DXP_DISABLE_LAM_F;
    a=DXP_DISABLE_LAM_A;
    len=1;
    status=dxp4c_md_io(ioChan,&f,&a,&dummy,&len);                    /* disable LAM */
    if (status!=DXP_SUCCESS){
	status=DXP_DISABLE_LAM;
	dxp_log_error("dxp_disable_LAM","Error disabling LAM",status);
    }
    return status;
}

/******************************************************************************
 * Routine to clear the LAM(Look-At-Me) on the specified DXP
 *
 * Clear the LAM for a single DXP module.
 *
 ******************************************************************************/
static int dxp_clear_LAM(int* ioChan, int* modChan)
/* int *ioChan;						Input: I/O channel of DXP module */
/* int *modChan;					Input: DXP channels no (0,1,2,3)      */
{
    /*
     *     Clear the LAM for a single DXP module
     */
    int status;
    unsigned int f,a,len;
    unsigned short dummy;
    int *itemp;

    /* Assign the unused inputs to stop compiler warnings */
    itemp = modChan;

    f=DXP_CLEAR_LAM_F;
    a=DXP_CLEAR_LAM_A;
    len=1;
    status=dxp4c_md_io(ioChan,&f,&a,&dummy,&len);                      /* clear LAM */
    if (status!=DXP_SUCCESS){
	status=DXP_CLR_LAM;
	dxp_log_error("dxp_clear_LAM","Error clearing LAM",status);
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
 * Routine to read a single word of data from the DXP
 * 
 * This routine makes all the neccessary calls to the TSAR and CSR to read
 * data from the appropriate channel and address of the DXP.
 *
 ******************************************************************************/
static int dxp_read_word(int* ioChan, int* modChan, int* xy, 
			 unsigned short* addr, unsigned short* readdata)
/* int *ioChan;						Input: I/O channel of DXP module      */
/* int *modChan;					Input: DXP channels no (0,1,2,3)      */
/* int *xy;							Input: X (0) or Y (1) memmory         */
/* unsigned short *addr;			Input: Address within X or Y mem.     */
/* unsigned short *readdata;		Output: word read from memory         */
{
    /*
     *     Read a single word from a DSP address, for a single channel of a
     *                single DXP module.
     */
    int status;
    char info_string[INFO_LEN];
    unsigned short data, saddr= *addr;

    if((*modChan<0)||(*modChan>3)){
	sprintf(info_string,"called with DXP channel number %d",*modChan);
	status = DXP_BAD_PARAM;
	dxp_log_error("dxp_read_word",info_string,status);
    }

    /* write transfer start address register */

    status = dxp_write_tsar(ioChan, &saddr);
    if (status!=DXP_SUCCESS){
	dxp_log_error("dxp_read_word","Error writing TSAR",status);
	return status;
    }

    data  = MASK_CAMXFER;
    data |= (*modChan<<6);
    if (*xy==YMEM) data |= MASK_YMEM;

    status = dxp_write_csr(ioChan, &data);
    if (status!=DXP_SUCCESS){
	dxp_log_error("dxp_read_word","Error writing to CSR",status);
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
 * data to the appropriate channel and address of the DXP.
 *
 ******************************************************************************/
static int dxp_write_word(int* ioChan, int* modChan, int* xy,
			  unsigned short* addr, unsigned short* writedata)
/* int *ioChan;						Input: I/O channel of DXP module      */
/* int *modChan;					Input: DXP channels no (-1,0,1,2,3)   */
/* int *xy;							Input: X (0) or Y (1) memmory         */
/* unsigned short *addr;			Input: Address within X or Y mem.     */
/* unsigned short *writedata;		Input: word to write to memory        */
{
    /*
     *     Write a single word to a DSP address, for a single channel or all 
     *            channels of a single DXP module
     */
    int status;
    char info_string[INFO_LEN];
    unsigned short data, saddr= *addr;

    if((*modChan<-1)||(*modChan>3)){
	sprintf(info_string,"called with DXP channel number %d",*modChan);
	status = DXP_BAD_PARAM;
	dxp_log_error("dxp_write_word",info_string,status);
    }
    
    /* write transfer start address register */

    status = dxp_write_tsar(ioChan, &saddr);
    if (status!=DXP_SUCCESS) {
	dxp_log_error("dxp_write_word","Error writing TSAR",status);
	return status;
    }

    data = (MASK_CAMXFER | MASK_WRITE);
    if (*modChan == ALLCHAN) 
    {
	data |= MASK_ALLCHAN;
    } else {
	data |= (*modChan<<6);
    }
    if (*xy == YMEM) data |= MASK_YMEM;

    status = dxp_write_csr(ioChan, &data);
    if (status!=DXP_SUCCESS) {
	dxp_log_error("dxp_write_word","Error writing CSR",status);
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
 * Routine to read a block of data from the DXP
 * 
 * This routine makes all the neccessary calls to the TSAR and CSR to read
 * data from the appropriate channel and address of the DXP.
 *
 ******************************************************************************/
static int dxp_read_block(int* ioChan, int* modChan, int* xy, int* constaddr,
			  unsigned short* addr, unsigned int* length, 
			  unsigned short* readdata)
/* int *ioChan;				Input: I/O channel of DXP module	*/
/* int *modChan;				Input: DXP channels no (0,1,2,3)	*/
/* int *xy;				Input: X (0) or Y (1)memmory		*/
/* int *constaddr;				Input: const add(1) incr. addr.(0)	*/
/* unsigned short *addr;			Input: start address within X or Y mem.	*/
/* unsigned int *length;			Input: # of 16 bit words to transfer	*/
/* unsigned short *readdata;		Output: words to read from memory	*/
{
    /*
     *     Read a block of words from a single DSP address, or consecutive
     *          addresses for a single channel of a single DXP module
     */
    int status;
    char info_string[INFO_LEN];
    unsigned short data, saddr= *addr;
    unsigned int i,nxfers,xlen;
    unsigned int maxblk;

    if((*modChan<0)||(*modChan>3)){
	sprintf(info_string,"called with DXP channel number %d",*modChan);
	status = DXP_BAD_PARAM;
	dxp_log_error("dxp_read_block",info_string,status);
    }

    /* write transfer start address register */

    status = dxp_write_tsar(ioChan, &saddr);
    if (status!=DXP_SUCCESS) {
	dxp_log_error("dxp_read_block","Error writing TSAR",status);
	return status;
    }


    data  = MASK_CAMXFER;
    data |= (*modChan<<6);
    if (*xy == YMEM) data |= MASK_YMEM;
    if (*constaddr == CONSTADD) data |= MASK_CONSTADD;
    
    status = dxp_write_csr(ioChan, &data);
    if (status!=DXP_SUCCESS) {
	dxp_log_error("dxp_read_block","Error writing CSR",status);
	return status;
    }

    /* Retrieve MAXBLK and check if single transfer is needed */
    maxblk=dxp4c_md_get_maxblk();
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
	if (i==(nxfers-1)) xlen=((*length)%maxblk) + 1;
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
static int dxp_write_block(int* ioChan, int* modChan, int* xy, int* constaddr,
			   unsigned short* addr, unsigned int* length,
			   unsigned short* writedata)
/* int *ioChan;						Input: I/O channel of DXP module     */
/* int *modChan;					Input: DXP channels no (-1,0,1,2,3)  */
/* int *xy;							Input: X (0) or Y (1)memmory         */
/* int *constaddr;					Input: const add(1) incr. addr.(0)   */
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
    unsigned short data, saddr= *addr;
    unsigned int i,nxfers,xlen;
    unsigned int maxblk;

    if((*modChan<-1)||(*modChan>3)){
	sprintf(info_string,"called with DXP channel number %d",*modChan);
	status = DXP_BAD_PARAM;
	dxp_log_error("dxp_write_block",info_string,status);
    }
    
    /* write transfer start address register */

    status = dxp_write_tsar(ioChan, &saddr);
    if (status!=DXP_SUCCESS){
	dxp_log_error("dxp_write_block","Error writing TSAR",status);
	return status;
    }

    data = (MASK_CAMXFER | MASK_WRITE);
    if (*modChan ==ALLCHAN) 
    {
	data |= MASK_ALLCHAN;
    } else {
	data |= (*modChan<<6);
    }
    if (*xy == YMEM) data |= MASK_YMEM;
    if (*constaddr == CONSTADD) data |= MASK_CONSTADD;
    
    status = dxp_write_csr(ioChan, &data);
    if (status!=DXP_SUCCESS){
	dxp_log_error("dxp_write_block","Error writing CSR",status);
	return status;
    }

    /* Retrieve MAXBLK and check if single transfer is needed */
    maxblk=dxp4c_md_get_maxblk();
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
	if (i==(nxfers-1)) xlen=((*length)%maxblk) + 1;
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
 * DXP channel is specified then all channels are downloaded.  The program 
 * is read from the file filename. 
 *
 ******************************************************************************/
static int dxp_download_fpgaconfig(int* ioChan, int* modChan, char *name, Board* board)
/* int *ioChan;			Input: I/O channel of DXP module	*/
/* int *modChan;			Input: DXP channels no (-1,0,1,2,3)	*/
/* char *name;                     Input: Type of FPGA to download         */
/* Board *board;			Input: Board data			*/
{
    /*
     *   Download the appropriate FiPPi configuration file to a single channel
     *   or all channels of a single DXP module.
     */
    int status;
    char info_string[INFO_LEN];
    unsigned short data;
    unsigned int i,j,length,xlen,nxfers;
    float wait;
    unsigned int maxblk;
    Fippi_Info *fippi=NULL;

    if (!((STREQ(name, "all")) || (STREQ(name, "fippi")))) 
    {
	sprintf(info_string, "The DXP4C does not have an FPGA called %s for channel number %d", name, *modChan);
	status = DXP_BAD_PARAM;
	dxp_log_error("dxp_download_fpgaconfig",info_string,status);
	return status;
    }

    if((*modChan<-1)||(*modChan>3)){
	sprintf(info_string,"called with DXP channel number %d",*modChan);
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
    /* make sure a valid Fippi was found */
    if (fippi==NULL) {
	sprintf(info_string,"There is no valid FiPPi defined for module %i",board->mod);
	status = DXP_NOFIPPI;
	dxp_log_error("dxp_download_fpgaconfig",info_string,status);
	return status;
    }

    length = fippi->proglen;
	
    /* Write to CSR to initiate download */

    /* turn on the fippi reset bit and the write bit */
    data = (MASK_FIPRESET | MASK_WRITE);

    if (*modChan == ALLCHAN) 
    {
	data |= MASK_ALLCHAN;
    } else {
	data |= (*modChan<<6);
    }

    /* Write the CSR back to the board */
    status = dxp_write_csr(ioChan, &data);
    if (status!=DXP_SUCCESS){
	dxp_log_error("dxp_download_fpgaconfig","Error writing CSR",status); 
	return status;
    }

    /* wait 50ms, for LCA to be ready for next data */

    wait = 0.050f;
    status = dxp4c_md_wait(&wait);

    /* single word transfers for first 10 words */
    for (i=0;i<10;i++){
	status = dxp_write_fippi(ioChan, &(fippi->data[i]), 1);
	if (status!=DXP_SUCCESS){
	    status = DXP_WRITE_WORD;
	    sprintf(info_string,"Error in %dth 1-word transfer",i);
	    dxp_log_error("dxp_download_fpgaconfig",info_string,status);
	    return status;
	}
    }

    /* Retrieve MAXBLK and check if single transfer is needed */
    maxblk=dxp4c_md_get_maxblk();
    if (maxblk <= 0) maxblk = length;

    /* prepare for the first pass thru loop */
    nxfers = ((length-11)/maxblk) + 1;
    xlen = ((maxblk>=(length-10)) ? (length-10) : maxblk);
    j = 0;
    do {

	/* now read the data */
        
	status = dxp_write_fippi(ioChan, &(fippi->data[j*maxblk+10]), xlen);
	if (status!=DXP_SUCCESS){
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
  
    return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to read the FPGA configuration file into memory
 *
 * This routine reads in the file filename and stores the FiPPi program
 *
 ******************************************************************************/
static int dxp_get_fpgaconfig(Fippi_Info* fippi)
/* Fippi_Info *fippi;			I/O: structure of Fippi info	*/
{

    int status, j;
    char info_string[INFO_LEN];
    char line[LINE_LEN];
    int nchars;
    unsigned int len;
    FILE *fp;

    sprintf(info_string,"%s%s%s","Reading FPGA file ",
	    fippi->filename,"...");
    dxp_log_info("dxp_get_fpgaconfig",info_string);
    fippi->maxproglen = MAXFIP_LEN;
    if (fippi->data==NULL){
	status = DXP_NOMEM;
	sprintf(info_string,"Error space not allocated for data in file %s",
		fippi->filename);
	dxp_log_error("dxp_get_fpgaconfig",info_string,status);
	return status;
    }
    if((fp = dxp_find_file(fippi->filename,"r"))==NULL){
	status = DXP_OPEN_FILE;
	sprintf(info_string,"Unable to open FPGA file %s",
		fippi->filename);
	dxp_log_error("dxp_get_fpgaconfig",info_string,status);
	return status;
    }

    /* Stuff the data into the fippi array */

    len = 0;
    while (fgets(line,132,fp)!=NULL){
	if (line[0]=='*') continue;
	nchars = strlen(line);
	while ((nchars>0) && !isxdigit(line[nchars])) {
	    nchars--;
	}
	for(j=0;j<nchars;j=j+2) {
	    sscanf(&line[j],"%2hX",&(fippi->data[len]));
	    len++;
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
/* int *modChan;			Input: Module channel number              */
/* char *name;                          Input: Type of FPGA to check the status of*/
/* board *board;			Input: Board structure for this device 	  */
{
    int status, chan;
    char info_string[INFO_LEN];
    unsigned short data;

    int ioChan;
    unsigned short used;

    int idummy;

    /* Assignment to satisfy the compiler */
    idummy = *modChan;

    /* Few assignements to make life easier */
    ioChan = board->ioChan;
    if (*modChan == allChan) 
    {
	used = board->used;
    } else {
	used = (unsigned short) (board->used & (1 << *modChan));
    }

    if (!((STREQ(name, "all")) || (STREQ(name, "fippi")))) 
    {
	sprintf(info_string, "The DXP4C2X does not have an FPGA called %s for channel number %d", name, board->mod);
	status = DXP_BAD_PARAM;
	dxp_log_error("dxp_download_fpga_done",info_string,status);
	return status;
    }

    /* Read back the CSR to determine if the download was successfull.  */
    if((status=dxp_read_csr(&ioChan,&data))!=DXP_SUCCESS){
	sprintf(info_string,"Failed to read CSR for module %d", board->mod);
	dxp_log_error("dxp_download_fpga_done",info_string,status);
	return status;
    }
  
  
    for(chan=0;chan<4;chan++){
	if((used&(1<<chan))==0) continue;		/* if not used, then we succeed */
	if((data&(0x0100<<chan))!=0){
	    sprintf(info_string,
		    "FiPPI download error (CSR bits) for module %d chan %d",
		    board->mod,chan);
	    status=DXP_FPGADOWNLOAD;
	    dxp_log_error("dxp_download_fpga_done",info_string,status);
	    return status;
	}
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
static int dxp_download_dspconfig(int* ioChan, int* modChan, Dsp_Info* dsp)
/* int *ioChan;			Input: I/O channel of DXP modul		*/
/* int *modChan;			Input: DXP channel no (-1,0,1,2,3)	*/
/* Dsp_Info *dsp;			Input: DSP structure				*/
{
    /*
     *   Download the DSP configuration file to a single channel or all channels 
     *                          of a single DXP module.
     *
     */

    int status;
    char info_string[INFO_LEN];
    unsigned short data;
    unsigned short length;
    unsigned int j,nxfers,xlen;
    float wait;
    unsigned int maxblk;

    dxp_log_debug("dxp_download_dspconfig", "Preparing to download DSP config");
  
    if((*modChan<-1)||(*modChan>3))
    {
	sprintf(info_string,"called with DXP channel number %d",*modChan);
	status = DXP_BAD_PARAM;
	dxp_log_error("dxp_download_dspconfig",info_string,status);
    }
   
    /* Write to CSR to initiate download */
  
    data = (MASK_DSPRESET | MASK_WRITE);
 
    if (*modChan==ALLCHAN) data |= MASK_ALLCHAN;

    else data |= (*modChan<<6);
  
    status = dxp_write_csr(ioChan, &data);
    if (status!=DXP_SUCCESS)
    {
	dxp_log_error("dxp_download_dspconfig","Error writing to the CSR",status);
	return status;
    }

    /* write "instruction wait cycle count" */
    data = 0;
    status = dxp_write_dsp(ioChan, &data, 1);
    if (status!=DXP_SUCCESS) 
    {
	status = DXP_WRITE_WORD;
	dxp_log_error("dxp_download_dspconfig","Error writing instruction wait cycle",
		      status);
	return status;
    }
  
    /* Set the length to words */
    length = (unsigned short)(dsp->proglen / 2);

    /* write the size of the boot code */
    status = dxp_write_dsp(ioChan, &length, 1);
    if (status!=DXP_SUCCESS) {
    
	status = DXP_WRITE_WORD;
	dxp_log_error("dxp_download_dspconfig","Error writing boot code size",status); 
	return status;
    }
  
    /* Reset the length to bytes */    
    length = (unsigned short)(length * 2);

    /* write the host/DSP handshake mode */
    status = dxp_write_dsp(ioChan, &data, 1);
    if (status!=DXP_SUCCESS){
    
	status = DXP_WRITE_WORD;
	dxp_log_error("dxp_download_dspconfig","Error writing host/DSP handshake mode",
		      status);
	return status;
    }

    /* Retrieve MAXBLK and check if single transfer is needed */
    maxblk=dxp4c_md_get_maxblk();
    if (maxblk <= 0) maxblk = length;

    /* prepare for the first pass thru loop */
    nxfers = ((length-1)/maxblk) + 1;
    xlen = ((maxblk>=length) ? length : maxblk);
    j = 0;
    do {

	/* now write the data */
	wait = 0.1f;
	status=dxp4c_md_wait(&wait); 
	status = dxp_write_dsp(ioChan, &(dsp->data[maxblk*j]), xlen); 
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

    if((status=dxp_clear_LAM(ioChan, &allChan))!=DXP_SUCCESS){
	dxp_log_error("dxp_download_dspconfig","Error clearing LAM",status);
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
/* int *ioChan;					Input: I/O channel of the module		*/
/* int *modChan;				Input: Module channel number			*/
/* int *mod;					Input: Module number, for errors		*/
/* Dsp_Info *dsp;				Input: Relavent DSP info				*/
/* unsigned short *value;		Input: Value to match for BUSY			*/
/* float *timeout;				Input: How long to wait, in seconds		*/
{
    int *temp;
    Dsp_Info *dtemp;
    unsigned short *stemp;
    float *ftemp;

    /* Assign all the input variables to stop the compiler from generating warnings */
    temp = ioChan;
    temp = modChan;
    temp = mod;
    dtemp = dsp;
    stemp = value;
    ftemp = timeout;

    /* Functionality not implemented for the DXP-4C */
    return DXP_SUCCESS;

}

/******************************************************************************
 * 
 * Routine to retrieve the FIPPI program maximum sizes so that memory
 * can be allocated.
 *
 ******************************************************************************/
static int dxp_get_fipinfo(Fippi_Info *fippi)
/* Fippi_Info *fippi;			I/O: Structure of FIPPI program Info	*/
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
/* Dsp_Defaults *defaults;		I/O: Structure of FIPPI program Info	*/
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
/* Dsp_Info *dsp;				I/O: Structure of DSP program Info	*/
{

    dsp->params->maxsym	    = MAXSYM;
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
/* Dsp_Info *dsp;				I/O: Structure of DSP program Info	*/
{
    FILE  *fp;
    int   status;
    char info_string[INFO_LEN];
    
    sprintf(info_string,"Loading DSP program in %s",dsp->filename);
    dxp_log_info("dxp_get_dspconfig",info_string);

    /* Now retrieve the file pointer to the DSP program via DSP_CONFIG */

    if((fp = dxp_find_file(dsp->filename,"r"))==NULL){
	status = DXP_OPEN_FILE;
	sprintf(info_string,"Unable to open %s",dsp->filename);
	dxp_log_error("dxp_get_dspconfig",info_string,status);
	return status;
    }

    /* Fill in some general information about the DSP program */
    dsp->params->maxsym	    = MAXSYM;
    dsp->params->maxsymlen  = MAXSYMBOL_LEN;
    dsp->maxproglen = MAXDSP_LEN;
	
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
static int dxp_get_dspdefaults(Dsp_Defaults *defaults)
/* Dsp_Defaults *defaults;		I/O: Structure of DSP defaults    */
{
    FILE  *fp;
    int   status,i, last;
    char *fstatus=" ";
    unsigned short nsymbols;
    char *token,*delim=" ,:=\r\n\t";
    char strtmp[4];
    char info_string[INFO_LEN];
    char line[LINE_LEN];

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

    if((fp = dxp_find_file(defaults->filename,"r"))==NULL){
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
	if (fstatus==NULL) continue;					/* End of file?  or END finishes */
	if (strncmp(line,"END",3)==0) break;

	/* Parse the line for the symbol name, value pairs and store the pairs in the defaults
	 * structure for later use */

	token=strtok(line,delim);
	/* Got the symbol name, copy the value */
	defaults->params->parameters[nsymbols].pname=
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
/* FILE  *fp;							Input: FILE pointer to opened DSP	*/
/* unsigned short *dspconfig;			Output: Array containing DSP program	*/
/* unsigned int *nwordsdsp;				Output: Size of DSP program			*/
/* char **dspparam;						Output: Array of DSP param names		*/
/* unsigned short *nsymbol;				Output: Number of defined DSP symbols*/
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
/* FILE *fp;						Input: File pointer from which to read the symbols	*/
/* unsigned short *dspconfig;		Output: Array containing DSP program					*/
/* unsigned int *nwordsdsp;			Output: Number of words in the DSP program			*/
{

    int i,status, reclen, junk, rectyp;
    char c1;
    unsigned short data, data1;
    char line[LINE_LEN];

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
    while(fgets(line,100,fp)!=NULL) {
	char *p = line;
	if (sscanf(p,"%c%2x%4x%2x",&c1,&reclen,&junk,&rectyp)==4){
	    p += 9;
	    for(i=0;i<reclen/2;i++, p+=4){
		sscanf(p,"%2hx%2hx",&data,&data1);
		if(rectyp==0) {
		    dsp->data[dsp->proglen] = (unsigned short) ((data1<<8) | data); 
		    (dsp->proglen)++;
		}
	    } 
	}
    }

    return DXP_SUCCESS;
}	
/******************************************************************************
 * Routine to read in the DSP symbol name list
 *
 ******************************************************************************/
static int dxp_load_dspsymbol_table(FILE* fp, Dsp_Info* dsp)
/* FILE *fp;						Input: File pointer of DSP program	*/
/* char **pnames;					Output: array of parameter names	*/
/* unsigned short *nsymbol;			Output: number of symbols			*/
{

    int status, retval;
    unsigned short i;
    /* Hold the character representing the access type *=R/W -=RO */
    char atype[2];
    char line[LINE_LEN];
    char info_string[INFO_LEN];

    /*
     *  Read comments and number of symbols
     */
    while(fgets(line,132,fp)!=NULL){
	if (line[0]=='*') continue;
	sscanf(line,"%hd",&(dsp->params->nsymbol));
	break;
    }
    if (dsp->params->nsymbol>0){
	/*
	 *  Allocate space and read symbols
	 */
	if ((dsp->params->parameters)==NULL) {
	    status = DXP_NOMEM;
	    dxp_log_error("dxp_load_dspsymbol_table",
			  "Error allocating space for symbol table (0)",status);
	    return status;
	}
	for(i=0;i<(dsp->params->nsymbol);i++) {
	    if (dsp->params->parameters[i].pname==NULL) {
		status = DXP_NOMEM;
		dxp_log_error("dxp_load_dspsymbol_table",
			      "Error allocating space for symbol table (1)",status);
		return status;
	    }
	    if (fgets(line, 132, fp)==NULL) {
		status = DXP_BAD_PARAM;
		dxp_log_error("dxp_load_dspsymbol_table",
			      "Error in SYMBOL format of DSP file",status);
		return status;
	    }
	    retval = sscanf(line, "%s %1s %hd %hd", dsp->params->parameters[i].pname, atype,
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
/* unsigned short *lindex;			Input: address of parameter	*/
/* Dsp_Info *dsp;					Input: dsp struct with info	*/
/* char string[];					Output: parameter name		*/
{

    int status;

    /* There better be an array called symbolname! */

    if (*lindex >= (dsp->params->nsymbol)) {
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
/* char name[];					Input: symbol name for accessing YMEM params */
/* Dsp_Info *dsp;					Input: dsp struct with info					*/
/* unsigned short *address;		Output: address in YMEM                      */
{
    /*
     *       Find pointer into parameter memmory using symbolic name
     *
     *  NOTE: THIS IS NOT FORTRAN CALLABLE:  FORTRAN USERS MUST CALL
     *         DXP_LOC_FORTRAN(symbol)
     */
    int status;
    unsigned short i;
    char info_string[INFO_LEN];

    if((dsp->proglen)<=0){
	status = DXP_DSPLOAD;
	sprintf(info_string, "Must Load DSP code before searching for %s", name);
	dxp_log_error("dxp_loc",info_string,status);
	return status;
    }

    *address= USHRT_MAX;
    for(i=0;i<(dsp->params->nsymbol);i++) {
	if (  (strlen(name)==strlen((dsp->params->parameters[i].pname))) &&
	      (strstr(name,(dsp->params->parameters[i].pname))!=NULL)  ) {
	    *address = i;
	    break;
	}
    }

    /* Did we find the Symbol in the table? */

    status = DXP_SUCCESS;
    if(*address==USHRT_MAX){
	status = DXP_NOSYMBOL;
	/*        sprintf(info_string, "Cannot find <%s> in symbol table",name);
		  dxp_log_error("dxp_loc",info_string,status);
	*/    }
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
/* int *ioChan;					Input: I/O channel of DXP module	*/
/* int *modChan;				Input: DXP channels no (0,1,2,3)	*/
/* Dsp_Info *dsp;				Input: dsp struct with info			*/
{
    /*
     *   Read the parameter memory from a single channel on a signle DXP module
     *              and display it along with their symbolic names
     */
    unsigned short *data;
    int status;
    int ncol, nleft;
    int i,xy,inc;
    unsigned short zero;
    unsigned int nsymbol;
    char buf[128];
    int col[4]={0,0,0,0};

    /* Allocate memory to read out parameters */
    data = (unsigned short *) dxp4c_md_alloc((dsp->params->nsymbol)*sizeof(unsigned short));

    xy=YMEM,inc=INCRADD,zero=0;
    nsymbol= (unsigned int) dsp->params->nsymbol;
    if ((status=dxp_read_block(ioChan,
			       modChan,
			       &xy,
			       &inc,
			       &zero,
			       &nsymbol,
			       data))!=DXP_SUCCESS) {
	dxp_log_error("dxp_mem_dump"," ",status);
	dxp4c_md_free(data);
	return status;
    }
    dxp4c_md_puts("\r\nDSP Parameters Memory Dump:\r\n");
    dxp4c_md_puts(" Parameter  Value    Parameter  Value  ");
    dxp4c_md_puts("Parameter  Value    Parameter  Value\r\n");
    dxp4c_md_puts("_________________________________________");
    dxp4c_md_puts("____________________________________\r\n");
	
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
	dxp4c_md_puts(buf);
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
	dxp4c_md_puts(buf);
    }

    /* Throw in a final line skip, if there was a stray symbol or 2 on the end of the 
     * table, else it was already added by the dxp4c_md_puts() above */

    if (nleft!=0) dxp4c_md_puts("\r\n");

    /* Free the allocated memory */
    dxp4c_md_free(data);

    return DXP_SUCCESS;

}


/******************************************************************************
 * Routine to return the length of the spectrum memory.
 *
 * For 4C-2X boards, this value is stored in the DSP and dynamic.  
 * For 4C boards, it is fixed.
 * 
 ******************************************************************************/
static unsigned int dxp_get_spectrum_length(Dsp_Info* dsp, unsigned short* params)
/* Dsp_Info *dsp;					Input: Relavent DSP info		*/
/* unsigned short *params;			Input: Array of DSP parameters	*/
{
    Dsp_Info *dtemp;
    unsigned short *stemp;

    /* Assign the input variables to stop compiler warnings */
    dtemp = dsp;
    stemp = params;

    return lens;

}

/******************************************************************************
 * Routine to return the length of the baseline memory.
 *
 * For 4C-2X boards, this value is stored in the DSP and dynamic.  
 * For 4C boards, it is fixed.
 * 
 ******************************************************************************/
static unsigned int dxp_get_baseline_length(Dsp_Info* dsp, unsigned short* params)
/* Dsp_Info *dsp;					Input: Relavent DSP info		*/
/* unsigned short *params;			Input: Array of DSP parameters	*/
{
    Dsp_Info *dtemp;
    unsigned short *stemp;

    /* Assign the input variables to stop compiler warnings */
    dtemp = dsp;
    stemp = params;

    return lenb;

}

/******************************************************************************
 * Routine to return the length of the event memory.
 *
 * For 4C-2X boards, this value is stored in the DSP and dynamic.  
 * For 4C boards, it is fixed.
 * 
 ******************************************************************************/
static unsigned int dxp_get_event_length(Dsp_Info* dsp, unsigned short* params)
/* Dsp_Info *dsp;					Input: Relavent DSP info		*/
/* unsigned short *params;			Input: Array of DSP parameters	*/
{
    Dsp_Info *dtemp;
    unsigned short *stemp;

    /* Assign the input variables to stop compiler warnings */
    dtemp = dsp;
    stemp = params;

    return lene;

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
/* int *modChan;				Input: Channel on the module to test	*/
/* int *pattern;				Input: Pattern to use during testing	*/
/* Board *board;				Input: Relavent Board info				*/
{

    int status;
    unsigned int len = 2*lens;	/* Test double the memory space.  MAXSPEC is defined
				   for 32-bit words, not 16-bit as this test performs */
    Board *btemp = NULL;
    char info_string[INFO_LEN];

    /* Assign the input variables to stop compiler warnings */
    btemp = board;

    dxp_log_debug("dxp_test_spectrum_memory", "Preparing to test memory");


    if((status = dxp_test_mem(ioChan, modChan, pattern, &xmem, 
			      &len, &starts))!=DXP_SUCCESS) {
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
				    Board* board)
/* int *ioChan;					Input: IO channel to test				*/
/* int *modChan;				Input: Channel on the module to test	*/
/* int *pattern;				Input: Pattern to use during testing	*/
/* Board *board;				Input: Relavent Board info				*/
{

    int status;
    Board *btemp;
    char info_string[INFO_LEN];

    /* Assign the input variables to stop compiler warnings */
    btemp = board;

    if((status = dxp_test_mem(ioChan, modChan, pattern, &ymem,
			      &lenb, &startb))!=DXP_SUCCESS) {
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
				 Board* board)
/* int *ioChan;					Input: IO channel to test				*/
/* int *modChan;				Input: Channel on the module to test	*/
/* int *pattern;				Input: Pattern to use during testing	*/
/* Board *board;				Input: Relavent Board info				*/
{

    int status;
    Board *btemp;
    char info_string[INFO_LEN];

    /* Assign the input variables to stop compiler warnings */
    btemp = board;


    if((status = dxp_test_mem(ioChan, modChan, pattern, &ymem,
			      &lene, &starte))!=DXP_SUCCESS) {
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
static int dxp_test_mem(int* ioChan, int* modChan, int* pattern, int* xy,
			unsigned int* length, unsigned short* addr)
/* int *ioChan;						Input: I/O channel of DXP module        */
/* int *modChan;					Input: DXP channels no (0,1,2,3)        */
/* int *pattern;					Input: pattern to write to memmory      */
/* int *xy;							Input: X (0) or Y (1) memmory           */
/* unsigned int *length;			Input: number of 16 bit words to xfer   */
/* unsigned short *addr;			Input: start address for transfer       */
{

    int status,nerrors,incr;
    unsigned short j;
    unsigned short *readbuf,*writebuf;
    char info_string[INFO_LEN];

    dxp_log_debug("dxp_test_mem", "Preparing to test memory");

    /* Allocate the memory for the buffers */
    if (*length>0) {
	readbuf = (unsigned short *) dxp4c_md_alloc(*length*sizeof(unsigned short));
	writebuf = (unsigned short *) dxp4c_md_alloc(*length*sizeof(unsigned short));

	sprintf(info_string, "readBuf = %p", readbuf);
	dxp_log_debug("dxp_test_mem", info_string);
	sprintf(info_string, "writeBuf = %p", writebuf);
	dxp_log_debug("dxp_test_mem", info_string);

    } else {
	status=DXP_BAD_PARAM;
	sprintf(info_string,
		"Attempting to test %d elements in DSP memory",*length);
	dxp_log_error("dxp_test_mem",info_string,status);
	return status;
    }

    /* Make sure we are not writing into parameter memory */

    if((*xy==YMEM)&&(*addr<MAXSYM)){
	status=DXP_BAD_PARAM;
	sprintf(info_string,
		"Attempting to write YMEM values begining at address %d",*addr);
	dxp_log_error("dxp_test_mem",info_string,status);
	dxp4c_md_free(readbuf);
	dxp4c_md_free(writebuf);
	return status;
    }
    incr=INCRADD;
    switch (*pattern) {
      case 0 : 
	for (j=0;j<*length;j++) writebuf[j] = j;
	break;
      case 1 :
	for (j=0;j<*length;j++) writebuf[j] = 0xFFFF;
	break;
      case 2 :
	for (j=0;j<*length;j++) 
	    writebuf[j] = (unsigned short) (((j&2)==0) ? 0xAAAA : 0x5555);
	break;
      default :
	status = DXP_BAD_PARAM;
	sprintf(info_string,"Option %d not implemented",*pattern);
	dxp_log_error("dxp_test_mem",info_string,status);
	dxp4c_md_free(readbuf);
	dxp4c_md_free(writebuf);
	return status;
    }
 
    /* Write the Pattern to the block of memory */

    if((status=dxp_write_block(ioChan,
			       modChan,
			       xy,
			       &incr,
			       addr,
			       length,
			       writebuf))!=DXP_SUCCESS) {
	dxp_log_error("dxp_test_mem"," ",status);
	dxp4c_md_free(readbuf);
	dxp4c_md_free(writebuf);
	return status;
    }
 
    dxp_log_debug("dxp_test_mem", "Finished writing block");

    /* Read the memory back and compare */

    if((status=dxp_read_block(ioChan,
			      modChan,
			      xy,
			      &incr,
			      addr,
			      length,
			      readbuf))!=DXP_SUCCESS) {
	dxp_log_error("dxp_test_mem"," ",status);
	dxp4c_md_free(readbuf);
	dxp4c_md_free(writebuf);
	return status;
    }

    dxp_log_debug("dxp_test_mem", "Finished reading block");

    for(j=0,nerrors=0; j<*length;j++) if (readbuf[j]!=writebuf[j]) {
	nerrors++;
	status = DXP_MEMERROR;
	if(nerrors<10){
	    sprintf(info_string, "Error: word %d, wrote %x, read back %x",
		    j,writebuf[j],readbuf[j]);
	    dxp_log_error("dxp_test_mem",info_string,status);
	}
    }

    if (nerrors!=0){
	sprintf(info_string, "%d memory compare errors found",nerrors);
	dxp_log_error("dxp_test_mem",info_string,status);
	dxp4c_md_free(readbuf);
	dxp4c_md_free(writebuf);
	return status;
    }

    dxp_log_debug("dxp_test_mem", "Preparing to free readbuf and writebuf");

    dxp4c_md_free(readbuf);
    dxp4c_md_free(writebuf);
   
    dxp_log_debug("dxp_test_mem", "Finished testing memory");

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
/* int *ioChan;					Input: IO channel to write to			*/
/* int *modChan;				Input: Module channel number to write to	*/
/* char *name;					Input: user passed name of symbol		*/
/* unsigned short *value;		Input: Value to set the symbol to		*/
/* Dsp_Info *dsp;				Input: Reference to the DSP structure	*/
{
    int status=DXP_SUCCESS;
    unsigned int i;
    unsigned short addr;		/* address of the symbol in DSP memory			*/
    char uname[20]="";			/* Upper case version of the user supplied name */
    char info_string[INFO_LEN];

    /* Change uname to upper case */

    if (strlen(name) > (dsp->params->maxsymlen)) {
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
	dxp_log_error("dxp_modify_dspsymbol",info_string,status);
	return status;
    }
    
    /* First find the location of the symbol in DSP memory. */

    if ((status  = dxp_loc(uname, dsp, &addr))!=DXP_SUCCESS) {
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
/* int *modChan;				Input: Module channel number to write to	*/
/* unsigned int *addr;			Input: address to write in DSP memory		*/
/* unsigned short *value;		Input: Value to set the symbol to			*/
{

    int status=DXP_SUCCESS;

    unsigned short saddr;

    char info_string[INFO_LEN];

    saddr = (unsigned short) (*addr + startp);

    if((status=dxp_write_word(ioChan,modChan,&ymem,&saddr,value))!=DXP_SUCCESS){
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
/* double *value;		        Output: Value to set the symbol to			*/
{

    int status=DXP_SUCCESS;
    unsigned int i;
    unsigned short nword = 1;   /* How many words does this symbol contain?		*/
    unsigned short addr=0;		/* address of the symbol in DSP memory			*/
    unsigned short addr1=0;		/* address of the 2nd word in DSP memory		*/
    char uname[20]="", tempchar[20]="";	/* Upper case version of the user supplied name */
    unsigned short stemp;		/* Temp location to store word read from DSP	*/
    double dtemp, dtemp1;       /* Long versions for the temporary variable		*/
    char info_string[INFO_LEN];

    /* Change uname to upper case */

    if (strlen(name)>(dsp->params->maxsymlen)) 
	  {
		status = DXP_NOSYMBOL;
		sprintf(info_string, "Symbol Name must be <%i characters", dsp->params->maxsymlen);
		dxp_log_error("dxp_read_dspsymbol", info_string, status);
		return status;
	  }

    /* Convert the name to upper case for comparison */

    for (i = 0; i < strlen(name); i++) 
	uname[i] = (char) toupper(name[i]); 

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
		sprintf(tempchar, "%s0",uname);
		nword = 2;
		status = dxp_loc(tempchar, dsp, &addr);
		if (status != DXP_SUCCESS)
		  {
			/* Failed to find the name with 0 attached, this symbol doesnt exist */
			sprintf(info_string, "Failed to find symbol %s in DSP memory", name);
			dxp_log_error("dxp_read_dspsymbol", info_string, status);
			return status;
		  }
		/* Search for the 2nd entry now */
		sprintf(tempchar, "%s1",uname);
		status = dxp_loc(tempchar, dsp, &addr1);
		if (status != DXP_SUCCESS)
		  {
			/* Failed to find the name with 1 attached, this symbol doesnt exist */
			sprintf(info_string, "Failed to find symbol %s+1 in DSP memory", name);
			dxp_log_error("dxp_read_dspsymbol", info_string, status);
			return status;
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
    status = dxp_read_word(ioChan, modChan, &ymem, &addr, &stemp);
	if (status != DXP_SUCCESS)
	  {
		sprintf(info_string, "Error reading parameter %s", name);
		dxp_log_error("dxp_read_dspsymbol",info_string,status);
		return status;
	  }
    dtemp = (double) stemp;

    /* If there is a second word, read it in */
    if (nword==2) 
	  {
		addr = (unsigned short) (dsp->params->parameters[addr1].address + startp);
		status = dxp_read_word(ioChan, modChan, &ymem, &addr, &stemp);
		if (status != DXP_SUCCESS)
		  {
			sprintf(info_string, "Error reading parameter %s+1", name);
			dxp_log_error("dxp_read_dspsymbol", info_string, status);
			return status;
		  }
		dtemp1 = (double) stemp;
		/* For 2 words, create the proper 32 bit number */
		*value = dtemp * 65536. + dtemp1;
	  } else {
		/* For single word values, assign it for the user */
		*value = dtemp;
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
static int dxp_read_dspparams(int* ioChan, int* modChan, 
			      Dsp_Info* dsp, unsigned short* params)
/* int *ioChan;						Input: I/O channel of DSP				*/
/* int *modChan;					Input: module channel of DSP			*/
/* Dsp_Info *dsp;					Input: Reference to the DSP structure	*/
/* unsigned short *params;			Output: array of DSP parameters			*/
{

    int status;
    unsigned int len;				/* Length of the DSP parameter memory */

    /* Read out the parameters from the DSP memory, stored in Y memory */

    dxp_log_debug("dxp_read_dspparams", "Preparing to read block");

    len = dsp->params->nsymbol;

    if((status=dxp_read_block(ioChan,modChan,&ymem,&incradd,
			      &startp,&len,params))!=DXP_SUCCESS){
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
static int dxp_write_dspparams(int* ioChan, int* modChan, 
			       Dsp_Info* dsp, unsigned short* params)
/* int *ioChan;						Input: I/O channel of DSP				*/
/* int *modChan;					Input: module channel of DSP			*/
/* Dsp_Info *dsp;					Input: Reference to the DSP structure	*/
/* unsigned short *params;			Input: array of DSP parameters			*/
{

    int status;
    unsigned int len;					/* Length of the DSP parameter memory */

    /* Read out the parameters from the DSP memory, stored in Y memory */

    len = dsp->params->nsymbol;

    if((status=dxp_write_block(ioChan,modChan,&ymem,&incradd,
			       &startp,&len,params))!=DXP_SUCCESS){
	dxp_log_error("dxp_write_dspparams","error reading parameters",status);
	return status;
    }

    return status;

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
/* int *ioChan;					Input: I/O channel of DSP			*/
/* int *modChan;				Input: module channel of DSP		*/
/* Board *board;				Input: Relavent Board				*/
/* unsigned long *spectrum;		Output: array of spectrum values	*/
{

    int status;
    unsigned int i;
    unsigned short *sspec;
    unsigned int lenss=2*lens;		/* number of short words in spectrum	*/
    Board *btemp;

    /* Assign the input variables to stop compiler warnings */
    btemp = board;

    /* Allocate memory for the spectrum */
    sspec = (unsigned short *) dxp4c_md_alloc(lenss*sizeof(unsigned short));

    /* Read the spectrum */

    if((status=dxp_read_block(ioChan,modChan,&xmem,&incradd,&starts,
			      &lenss,sspec))!=DXP_SUCCESS){
	dxp_log_error("dxp_read_spectrum","Error reading out spectrum",status);
	return status;
    }

    /* Unpack the array of shorts into the long spectra */

    for (i=0;i<lens;i++) {
	spectrum[i] = ((sspec[2*i]&0xFFFF)<<16) + (sspec[2*i+1]&0xFFFF);
    }

    /* If little endian, then swap all the bytes of the spectrum memory */
	
    /*    if (dxp_little_endian()) dxp_swaplong(&lens,spectrum);
     */	
    /* Free memory used for the spectrum */
    dxp4c_md_free(sspec);

    return status;

}

/******************************************************************************
 * Routine to readout the baseline histogram from a single DSP.
 * 
 * This routine reads the baselin histogram from the DSP pointed to by ioChan and
 * modChan.  It returns the array to the caller.
 *
 ******************************************************************************/
static int dxp_read_baseline(int* ioChan, int* modChan, Board *board, 
			     unsigned short* baseline)
/* int *ioChan;					Input: I/O channel of DSP					*/
/* int *modChan;				Input: module channel of DSP				*/
/* Board *board;				Input: Relavent Board info					*/
/* unsigned short *baseline;	Output: array of baseline histogram values	*/
{

    int status;
    Board *btemp;

    /* Assign the input variables to stop compiler warnings */
    btemp = board;

    /* Read out the basline histogram. */

    if((status=dxp_read_block(ioChan,modChan,&ymem,&incradd,&startb,
			      &lenb,baseline))!=DXP_SUCCESS){
	dxp_log_error("dxp_read_baseline","Error reading out baseline",status);
	return status;
    }

    return status;

}

/******************************************************************************
 * Routine to readout the Event data memory from a single DSP.
 * 
 * This routine reads the event memory from the DSP pointed to by ioChan and
 * modChan.  It returns the array to the caller.
 *
 ******************************************************************************/
static int dxp_read_event(int* ioChan, int* modChan, Board* board, 
			  unsigned short* event)
/* int *ioChan;					Input: I/O channel of DSP		*/
/* int *modChan;				Input: module channel of DSP	*/
/* Board *board;				Input: Relavent Board info		*/
/* unsigned short *event;		Output: array of event memory	*/
{

    int status;
    Board *btemp;

    /* Assign the input variables to stop compiler warnings */
    btemp = board;

    /* Read out the basline histogram. */

    if((status=dxp_read_block(ioChan,modChan,&ymem,&incradd,&starte,
			      &lene,event))!=DXP_SUCCESS){
	dxp_log_error("dxp_read_event","Error reading out event memory",status);
	return status;
    }

    return status;

}

/******************************************************************************
 * Routine to prepare the DXP4C for data readout.
 * 
 * This routine will ensure that the board is in a state that 
 * allows readout via CAMAC bus.
 *
 ******************************************************************************/
int dxp_prep_for_readout(int* ioChan, int *modChan)
/* int *ioChan;						Input: I/O channel of DXP module			*/
/* int *modChan;					Input: Module channel number				*/
{
    /*
     * Check if run is active.  If so, stop the run.
     */
    int status;
    unsigned short data;
    float wait;

    status = dxp_read_csr(ioChan, &data);	/* Read the state of the CSR */
    if (status!=DXP_SUCCESS){
	dxp_log_error("dxp_prep_for_readout",
		      "Error preparing module for readout",status);
	return status;
    }

    /* Is there an active run? */
    if (data & MASK_RUNENABLE) {
	status = dxp_end_run(ioChan, modChan);	/* Stop the current run. */
	if (status!=DXP_SUCCESS){
	    dxp_log_error("dxp_prep_for_readout",
			  "Error ending run",status);
	    return status;
	}
	/* Wait for 10ms after stopping run for events to process */
	wait = (float) 0.01;
	status = dxp4c_md_wait(&wait);
	if (status!=DXP_SUCCESS){
	    dxp_log_error("dxp_prep_for_readout",
			  "Error waiting for events to clear",status);
	    return status;
	}
    }
	
    return DXP_SUCCESS;
}

/******************************************************************************
 * Routine to prepare the DXP4C for data readout.
 * 
 * This routine will ensure that the board is in a state that 
 * allows readout via CAMAC bus.
 *
 ******************************************************************************/
int dxp_done_with_readout(int* ioChan, int *modChan, Board* board)
/* int *ioChan;					Input: I/O channel of DXP module	*/
/* int *modChan;				Input: Module channel number		*/
/* Board *board;				Input: Relavent Board info			*/
{
    /*
     * If there was a run active then restart the run with the same 
     * settings, do not clear the MCA memory in this case.
     */
    int status;
    unsigned short resume, gate;
	
    if (board->state[0]==1) {
	resume = 1;
	gate = (unsigned short) board->state[1];
	/* restart the run. */
	status = dxp_begin_run(ioChan, modChan, &gate, &resume, board);	
	if (status!=DXP_SUCCESS){
	    dxp_log_error("dxp_done_with_readout",
			  "Error restarting the run",status);
	    return status;
	}
    }

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
			 unsigned short* resume, Board* board)
/* int *ioChan;						Input: I/O channel of DXP module        */
/* int *modChan;					Input: module channel number			*/
/* unsigned short *gate;			Input: ignore (1) or use (0) ext. gate  */
/* unsigned short *resume;			Input: clear MCA first(0) or update (1) */
/* Board *board;					Input: Relavent Board info				*/
{
    /*
     *   Initiate data taking for all channels of a single DXP module. 
     */
    int status;
    unsigned short data;
    int *itemp;
    Board *btemp;

    /* Assign unused inputs to avoid compiler warnings */
    itemp = modChan;
    btemp = board;

    data = MASK_RUNENABLE;
    /*	data |= (unsigned short) (*modChan==ALLCHAN ? MASK_ALLCHAN : *modChan<<6);*/
    data |= (unsigned short) MASK_ALLCHAN;
    if (*resume == CLEARMCA) data |= MASK_RESETMCA;
    if (*gate == IGNOREGATE) data |= MASK_IGNOREGATE;

    status = dxp_write_csr(ioChan, &data);                    /* write to CSR */
    if (status!=DXP_SUCCESS){
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
/* int *ioChan;						Input: I/O channel of DXP module			*/
/* int *modChan;					Input: module channel number of DXP module	*/
{
    /*
     *   Terminate a data taking for all channels of a single DXP module. 
     */
    int status;
    unsigned short data;
    int *itemp;

    /* Assign unused inputs to avoid compiler warnings */
    itemp = modChan;

    data = MASK_ALLCHAN;

    status = dxp_write_csr(ioChan, &data);                    /* write to CSR */
    if (status!=DXP_SUCCESS) 
    {
	dxp_log_error("dxp_end_run","Error writing CSR",status);
    }

    if((status=dxp_clear_LAM(ioChan, &allChan))!=DXP_SUCCESS) {
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
    int *itemp;

    /* Assign the input variables to stop compiler warnings */
    itemp = ioChan;
    itemp = modChan;

    /* The DXP4C has no mechanism for determining run active status */
    *active = 0;

    return DXP_SUCCESS;    

}

/******************************************************************************
 *
 * Routine to start a control task routine.  Definitions for types are contained
 * in the xerxes_generic.h file.
 * 
 ******************************************************************************/
static int dxp_begin_control_task(int* ioChan, int* modChan, short *type, 
				  unsigned int *length, int *info, Board *board)
/* int *ioChan;						Input: I/O channel of DXP module			*/
/* int *modChan;					Input: module channel number of DXP module	*/
/* short *type;						Input: type of control task to perfomr		*/
/* int *length;						Input: Length of the config info array		*/
/* int *info;						Input: Configuration info for the task		*/
/* Board *board;					Input: Board data							*/
{
    int status = DXP_SUCCESS;

    /* Variables for parameters to be read/written from/to the DSP */
    unsigned short runtasks, whichtest, loopcount;

    unsigned short zero=0;
    double temp;

    int special_run;

    char info_string[INFO_LEN];

    /* Define if this requires a special run: setting RUNTASKS, starting run, etc...*/
    special_run = (*type==CT_DXP4C_RESETASC) || (*type==CT_DXP4C_TRKDAC) ||
	(*type==CT_DXP4C_DACSET) || (*type==CT_DXP4C_ADCTEST) || (*type==CT_DXP4C_ASCMON) ||
	(*type==CT_DXP4C_RESET) || (*type==CT_ADC) || (*type==CT_DXP4C_ADC) ||
	(*type==CT_DXP4C_FIPTRACE);

    if (special_run) {
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
	if((status=dxp_read_dspsymbol(ioChan,modChan,
				      "RUNTASKS",board->dsp[*modChan],&temp))!=DXP_SUCCESS){
	    sprintf(info_string,
		    "Error reading RUNTASKS from module %d chan %d",board->mod,*modChan);
	    dxp_log_error("dxp_begin_control_task",info_string,status);
	    return status;
	}
	/* modify */
	runtasks = (unsigned short) temp;
	runtasks |= CONTROL_TASK;
	/* write */
	if((status=dxp_modify_dspsymbol(ioChan,modChan,
					"RUNTASKS",&runtasks,board->dsp[*modChan]))!=DXP_SUCCESS){
	    sprintf(info_string,
		    "Error writing RUNTASKS from module %d chan %d",board->mod,*modChan);
	    dxp_log_error("dxp_begin_control_task",info_string,status);
	    return status;
	}

	
	/* First check if the control task is the ADC readout */
	if (*type==CT_DXP4C_RESETASC) {
	    whichtest = WHICHTEST_RESETASC;
	} else if (*type==CT_DXP4C_TRKDAC) {
	    whichtest = WHICHTEST_TRKDAC;
	} else if (*type==CT_DXP4C_DACSET) {
	    whichtest = WHICHTEST_DACSET;
	} else if (*type==CT_DXP4C_ADCTEST) {
	    whichtest = WHICHTEST_ADCTEST;
	} else if (*type==CT_DXP4C_ASCMON) {
	    whichtest = WHICHTEST_ASCMON;
	} else if (*type==CT_DXP4C_RESET) {
	    whichtest = WHICHTEST_RESET;
	} else if ((*type==CT_ADC) || (*type==CT_DXP4C_ADC)) {
	    whichtest = WHICHTEST_ADC_TRACE;
	} else if (*type==CT_DXP4C_FIPTRACE) {
	    whichtest = WHICHTEST_FIP_TRACE;
	} else {
	    status=DXP_NOCONTROLTYPE;
	    sprintf(info_string,
		    "Unknown control type %d for this DXP4C module",*type);
	    dxp_log_error("dxp_begin_control_task",info_string,status);
	    return status;
	}

	/* write WHICHTEST */
	if((status=dxp_modify_dspsymbol(ioChan,modChan,
					"WHICHTEST",&whichtest,board->dsp[*modChan]))!=DXP_SUCCESS){
	    sprintf(info_string,
		    "Error writing WHICHTEST to module %d chan %d",board->mod,*modChan);
	    dxp_log_error("dxp_begin_control_task",info_string,status);
	    return status;
	}
	/* write the LOOPCOUNT parameter */
	if ((info[0]<65536)&&(info[0]>=0)) {
	    loopcount = (unsigned short) info[0];
	} else {
	    loopcount = 65535;
	}
	if((status=dxp_modify_dspsymbol(ioChan,modChan,
					"LOOPCOUNT",&loopcount,board->dsp[*modChan]))!=DXP_SUCCESS){
	    sprintf(info_string,
		    "Error writing LOOPCOUNT to module %d chan %d",board->mod,*modChan);
	    dxp_log_error("dxp_begin_control_task",info_string,status);
	    return status;
	}

	/* start the control run */
	if((status=dxp_begin_run(ioChan,modChan,&zero,&zero,board))!=DXP_SUCCESS){
	    sprintf(info_string,
		    "Error starting control run on module %d chan %d",board->mod,*modChan);
	    dxp_log_error("dxp_begin_control_task",info_string,status);
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
/* int *ioChan;						Input: I/O channel of DXP module			*/
/* int *modChan;					Input: module channel number of DXP module	*/
/* Board *board;					Input: Board data							*/
{
    int status = DXP_SUCCESS;
    double temp;
    unsigned short runtasks;
    char info_string[INFO_LEN];

    if((status=dxp_end_run(ioChan,modChan))!=DXP_SUCCESS){
	sprintf(info_string,
		"Error ending control task run for chan %d",*modChan);
	dxp_log_error("dxp_end_control_task",info_string,status);
	return status;
    }

    /* Read/Modify/Write the RUNTASKS parameter */
    /* read */
    if((status=dxp_read_dspsymbol(ioChan,modChan,
				  "RUNTASKS",board->dsp[*modChan],&temp))!=DXP_SUCCESS){
	sprintf(info_string,
		"Error reading RUNTASKS from module %d chan %d",board->mod,*modChan);
	dxp_log_error("dxp_end_control_task",info_string,status);
	return status;
    }

	runtasks = (unsigned short) temp;
    if ((runtasks & CONTROL_TASK) != 0) 
	  {
		/* modify */
		runtasks &= ~CONTROL_TASK;
		/* write */
		status = dxp_modify_dspsymbol(ioChan,modChan, "RUNTASKS", &runtasks, board->dsp[*modChan]);
		if (status != DXP_SUCCESS)
		  {
			sprintf(info_string,
					"Error writing RUNTASKS from module %d chan %d",board->mod,*modChan);
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
/* int *ioChan;						Input: I/O channel of DXP module			*/
/* int *modChan;					Input: module channel number of DXP module	*/
/* short *type;						Input: type of control task to perfomr		*/
/* Board *board;					Input: Board data							*/
/* int info[20];					Output: Configuration info for the task		*/
{
    int status = DXP_SUCCESS;
    double decimation;
    char info_string[INFO_LEN];

    /* Default values */
    /* nothing to readout here */
    info[0] = 0;
    /* stop when user is ready */
    info[1] = 0;
    /* stop when user is ready */
    info[2] = 0;

    /* Check the control task type */
    if (*type==CT_DXP4C_RESETASC) {
    } else if (*type==CT_DXP4C_TRKDAC) {
	/* length=1, the value of the tracking DAC */
	info[0] = 1;
	/* Recommend waiting 10ms initially */
	info[1] = 10;
	/* Recomment 1ms polling after initial wait */
	info[2] = 1;
    } else if (*type==CT_DXP4C_DACSET) {
    } else if (*type==CT_DXP4C_ADCTEST) {
    } else if (*type==CT_DXP4C_ASCMON) {
    } else if (*type==CT_DXP4C_RESET) {
	/* length=1, the reset value of the calibration */
	info[0] = 1;
	/* Recommend waiting 10ms initially */
	info[1] = 10;
	/* Recomment 1ms polling after initial wait */
	info[2] = 1;
    } else if ((*type==CT_ADC) || (*type==CT_DXP4C_ADC)) {
	/* length=spectrum length*/
	info[0] = lens;
	/* Recommend waiting 4ms initially, 400ns*spectrum length */
	info[1] = 4;
	/* Recomment 1ms polling after initial wait */
	info[2] = 1;
    } else if (*type==CT_DXP4C_FIPTRACE) {
	/* need to read the decimation to determine the waiting time */
	if((status=dxp_read_dspsymbol(ioChan,modChan,
				      "DECIMATION",board->dsp[*modChan],&decimation))!=DXP_SUCCESS){
	    sprintf(info_string,
		    "Error reading DECIMATION from module %d chan %d",board->mod,*modChan);
	    dxp_log_error("dxp_control_task_params",info_string,status);
	    return status;
	}
	/* length=event data length*/
	info[0] = lene;
	/* Recommend waiting 400ns*event data length*2^decimation */
	info[1] = 1 * ((int) pow(2, decimation));
	/* Recomment 1ms polling after initial wait */
	info[2] = 5;
    } else {
	  status=DXP_NOCONTROLTYPE;
	  sprintf(info_string,
			  "Unknown control type %d for this DXP4C module",*type);
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
/* int *ioChan;						Input: I/O channel of DXP module			*/
/* int *modChan;					Input: module channel number of DXP module	*/
/* short *type;						Input: type of control task to perfomr		*/
/* Board *board;					Input: Board data							*/
/* void *data;						Output: Data read after the control task	*/
{
    int status = DXP_SUCCESS;

    unsigned int i;
    unsigned short *stemp;
    unsigned long *ltemp;
    char info_string[INFO_LEN];

    /* Check the control task type */
    if (*type==CT_DXP4C_RESETASC) {
    } else if (*type==CT_DXP4C_TRKDAC) {
    } else if (*type==CT_DXP4C_DACSET) {
    } else if (*type==CT_DXP4C_ADCTEST) {
    } else if (*type==CT_DXP4C_ASCMON) {
    } else if (*type==CT_DXP4C_RESET) {
    } else if ((*type==CT_ADC) || (*type==CT_DXP4C_ADC)) {
	/* allocate memory */
	stemp = (unsigned short *) dxp4c_md_alloc(lene*sizeof(unsigned short));
	if (stemp==NULL) {
	    status = DXP_NOMEM;
	    sprintf(info_string,
		    "Not enough memory to allocate temporary array of length %d",lene);
	    dxp_log_error("dxp_control_task_data",info_string,status);
	    return status;
	}

	if((status=dxp_read_event(ioChan, modChan, board, stemp))!=DXP_SUCCESS){
	    dxp4c_md_free(stemp);
	    sprintf(info_string,
		    "Error ending control task run for chan %d",*modChan);
	    dxp_log_error("dxp_control_task_data",info_string,status);
	    return status;
	}

	/* Assign the void *data to a local unsigned long * array for assignment purposes.  Just easier to read than
	 * casting the void * on the left hand side */
	ltemp = (unsigned long *) data;
	for (i=0;i<lene;i++) ltemp[i] = (unsigned long) stemp[i];

	/* Free memory */
	dxp4c_md_free(stemp);

    } else if (*type==CT_DXP4C_FIPTRACE) {
    } else {
	status=DXP_NOCONTROLTYPE;
	sprintf(info_string,
		"Unknown control type %d for this DXP4C module",*type);
	dxp_log_error("dxp_control_task_data",info_string,status);
	return status;
    }
	
    return status;    

}
/******************************************************************************
 * Routine to start a special Calibration run
 * 
 * Begin a special data run on specified channel of of a single DXP module.  
 * At present, there are only two: Measure the internal gain 
 * (*calib_task=CALIB_TRKDAC) or determine the TRACKDAC value for resets
 * (*calib_task=CALIB_RESET)
 *
 ******************************************************************************/
static int dxp_begin_calibrate(int* ioChan, int* modChan, int* calib_task, 
			       Board *board)
/* int *ioChan;					Input: I/O channel of DXP module	*/
/* int *modChan;				Input: Module channel of DXP module	*/
/* int *calib_task;				Input: int.gain (1) reset (2)		*/
/* Board *board;				Input: Relavent Board info			*/
{
    /*
     */
    int status;
    unsigned short ignore=IGNOREGATE,resume=CLEARMCA;
    unsigned short whichtest=2;
    unsigned short loopcount=20;
    unsigned short runtasks=256;
    char info_string[INFO_LEN];

    /* Be paranoid and check if the DSP configuration is downloaded.  If not, do it */
	
    if ((board->dsp[*modChan]->proglen)<=0) {
	status = DXP_DSPLOAD;
	sprintf(info_string, "Must Load DSP code before calibrations");
	dxp_log_error("dxp_begin_calibrate",info_string,status);
	return status;
    }

    /* Determine the type of test to perform */

    switch (*calib_task) {
      case CALIB_TRKDAC :
	whichtest = 2;
	break;
      case CALIB_RESET :
	whichtest = 7;
	break;
      default :
	sprintf(info_string,"Calibration task = %d is nonexistant",
		*calib_task);
	status = DXP_BAD_PARAM;
	dxp_log_error("dxp_begin_calibrate",info_string,status);
	return status;
    } 
    if((status=dxp_modify_dspsymbol(ioChan,modChan,
				    "WHICHTEST",&whichtest,board->dsp[*modChan]))!=DXP_SUCCESS) {    
	dxp_log_error("dxp_begin_calibrate","Error writing WHICHTEST",status);
	return status;
    }
    if((status=dxp_modify_dspsymbol(ioChan,modChan,
				    "LOOPCOUNT",&loopcount,board->dsp[*modChan]))!=DXP_SUCCESS) {    
	dxp_log_error("dxp_begin_calibrate","Error writing LOOPCOUNT",status);
	return status;
    }
    if((status=dxp_modify_dspsymbol(ioChan,modChan,
				    "RUNTASKS",&runtasks,board->dsp[*modChan]))!=DXP_SUCCESS) {    
	dxp_log_error("dxp_begin_calibrate","Error writing RUNTASKS",status);
	return status;
    }

    /* Finally startup the Calibration run. */

    if((status = dxp_begin_run(ioChan, modChan, &ignore, &resume, board))!=DXP_SUCCESS){
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
/* unsigned short array[];			Input: array from parameter block read */
/* Dsp_Info *dsp;					Input: Reference to the DSP structure	*/
/* unsigned short *runerror;		Output: runerror word                  */
/* unsigned short *errinfo;			Output: errinfo word                   */
{

    int status;
    char info_string[INFO_LEN];
    unsigned short addr_RUNERROR,addr_ERRINFO;

    /* Be paranoid and check if the DSP configuration is downloaded.  If not, do it */
	
    if ((dsp->proglen)<=0) {
	status = DXP_DSPLOAD;
	sprintf(info_string, "Must Load DSP code before decoding errors");
	dxp_log_error("dxp_decode_error",info_string,status);
	return status;
    }

    /* Now pluck out the location of a couple of symbols */

    status  = dxp_loc("RUNERROR",dsp,&addr_RUNERROR);
    status += dxp_loc("ERRINFO",dsp,&addr_ERRINFO);
    if(status!=DXP_SUCCESS){
	status=DXP_NOSYMBOL;
	dxp_log_error("dxp_decode_error"," ",status);
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
static int dxp_clear_error(int* ioChan, int* modChan, Dsp_Info* dsp)
/* int *ioChan;						Input: I/O channel of DXP module			*/
/* int *modChan;					Input: DXP channels no (-1,0,1,2,3)		*/
/* Dsp_Info *dsp;					Input: Reference to the DSP structure	*/
{

    int status;
    char info_string[INFO_LEN];
    unsigned short zero;

    /* Be paranoid and check if the DSP configuration is downloaded.  If not, do it */
	
    if ((dsp->proglen)<=0) {
	status = DXP_DSPLOAD;
	sprintf(info_string, "Must Load DSP code before clearing errors");
	dxp_log_error("dxp_clear_error",info_string,status);
	return status;
    }

    zero=0;
    status = dxp_modify_dspsymbol(ioChan,modChan,"RUNERROR",&zero,dsp);
    if (status!=DXP_SUCCESS) dxp_log_error("dxp_clear_error"," ",status);

    return status;

}

			
/******************************************************************************
 * 
 * Routine that checks the status of a completed calibration run.  This 
 * typically depends on symbols in the DSP that were changed by the 
 * calibration task.
 *
 ******************************************************************************/
static int dxp_check_calibration(int* calibtask, unsigned short* params, 
				 Dsp_Info* dsp)
/* int *calibtask;						Input: Calibration test performed	*/
/* unsigned short *params;				Input: parameters read from the DSP	*/
/* Dsp_Info *dsp;						Input: Reference to the DSP structure	*/
{

    int status = DXP_SUCCESS;
    char info_string[INFO_LEN];
    unsigned short addr_TRACKRST=USHRT_MAX, addr_DACPERADC=USHRT_MAX;

    /* Grab the addresses of some DSP parameters */

    status  = dxp_loc("TRACKRST",dsp,&addr_TRACKRST);
    status += dxp_loc("DACPERADC",dsp,&addr_DACPERADC);
    if(status!=DXP_SUCCESS){
	status = DXP_NOSYMBOL;
	dxp_log_error("dxp_check_calibration"," ",status);
	return status;
    }

    /* If the task was TRACKDAC calibration, make sure the gain wasn't determined to be 0! */
            
    if(*calibtask==CALIB_TRKDAC){
	if(params[addr_DACPERADC]==0){
	    status = DXP_INTERNAL_GAIN;
	    sprintf(info_string,"Internal gain error: wrong polarity?");
	    dxp_log_error("dxp_check_calibration",info_string,status);
	}


	/* If the task was RESET, make sure the reset value for the TRACKDAC is not set too
	 * close to the range of possible values */

    } else if (*calibtask==CALIB_RESET){
	if((params[addr_TRACKRST]<2)||(params[addr_TRACKRST]>253)){
	    sprintf(info_string,
		    "RESET calib/WARNING: TRACKRST = %d",params[addr_TRACKRST]);
	    status = DXP_RESET_WARNING;
	    dxp_log_error("dxp_check_calibration",info_string,status);
	}
    }
	
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
/* unsigned short array[];				Input: array from parameter block	 */
/* Dsp_Info *dsp;						Input: Reference to the DSP structure*/
/* unsigned int *evts;					Output: number of events in spectrum */
/* unsigned int *under;					Output: number of underflows		 */
/* unsigned int *over;					Output: number of overflows			 */
/* unsigned int *fast;					Output: number of fast filter events */
/* unsigned int *base;					Output: number of baseline events	 */
/* double *live;						Output: livetime in seconds			 */
/* double *icr;					        Output: Input Count Rate in kHz      */
/* double *ocr;					        Output: Output Count Rate in kHz     */
{

    unsigned short addr_EVTSINRUN=USHRT_MAX,addr_LIVETIME=USHRT_MAX;
    unsigned int one=1, live_int, *ptr;
    int status;
    char info_string[INFO_LEN];

	/* Temporary values from the DSP code */
	unsigned long nEvents = 0;

    /* Be paranoid and check if the DSP configuration is downloaded.  If not, do it */
    if ((dsp->proglen)<=0) 
	  {
		status = DXP_DSPLOAD;
		sprintf(info_string, "Must Load DSP code before decoding runstats");
		dxp_log_error("dxp_get_runstats",info_string,status);
		return status;
	  }

    status = dxp_loc("EVTSINRUN0",dsp,&addr_EVTSINRUN);
    status += dxp_loc("LIVETIME0",dsp,&addr_LIVETIME);
    if(status!=DXP_SUCCESS)
	  {
		status = DXP_NOSYMBOL;
		dxp_log_error("dxp_get_runstats"," ",status);
		return status;
	  }

    ptr = (unsigned int *)&array[addr_EVTSINRUN];
    *evts  = *ptr;
    *under = *(ptr+1);
    *over  = *(ptr+2);
    *fast  = *(ptr+3);
    *base  = *(ptr+4);
    ptr = (unsigned int *)&array[addr_LIVETIME];
    live_int  = *ptr;
    if (dxp_little_endian())
	  {
		dxp_swaplong(&one, (unsigned long *)evts);
		dxp_swaplong(&one, (unsigned long *)under);
		dxp_swaplong(&one, (unsigned long *)over);
		dxp_swaplong(&one, (unsigned long *)fast);
		dxp_swaplong(&one, (unsigned long *)base);
		dxp_swaplong(&one, (unsigned long *)&live_int);
	  }
    *live = live_int * LIVECLOCK_TICK_TIME;
	
    /* Calculate the number of events in the run */
    nEvents = *evts + *under + *over;

    /* Calculate the event rates */
    if(*live > 0.)
	  {
		*icr = 0.001 * (*fast) / (*live);
		*ocr = 0.001 * (nEvents) / (*live);
	  } else {
		*icr = -999.;
		*ocr = -999.;
	  }

    return DXP_SUCCESS;

}

/******************************************************************************
 * 
 * This routine calculates the new values of the coarse and fine gain DACs 
 * after a change in the input gain of gainchange = (desired gain)/(present gain)
 *
 * assumptions:  The fine gain DAC has a logarithmic response that spans 12 dB
 *               Coarse gain settings are as follows:
 *
 *                 CG(3)/CG(2) = 3.3
 *                 CG(2)/CG(1) = 3.0
 *                 CG(1)/CG(0) = 3.3
 *
 * Since there is significant overlap between coarse gain settings, avoid the
 * extreme limits of the fine gain...
 *
 ******************************************************************************/
static int dxp_perform_gaincalc(float* gainchange, unsigned short* coarsegain,
				unsigned short* finegain, short* deltacoarse,
				short* deltafine)
/* float *gainchange;						Input: desired gain change           */
/* unsigned short *coarsegain;				Input: current finegain              */
/* unsigned short *finegain;				Input: current coarse gain           */
/* short *deltacoarse;						Output: required coarsegain change   */
/* short *deltafine;						Output: required finegain change     */
{

    float gc;
    float factor=(float) 426.6667;		/* 20*(256 DAC steps)/(12 dB)  1 dB=20log(G) */
    unsigned int fg,cg,df;
    int   status;
    
    gc = *gainchange;
    cg = *coarsegain;
    df = (int) (factor*log10(gc));
    fg = *finegain + df;
    /*
     *    Adjust the coarse gain to keep the final finegain in the interval
     *       [9<finegain<246]
     */
    status = DXP_SUCCESS;
    while ((fg<10)||(fg>245)){
	if (fg>245){                          /* need to INCREASE coarse gain */

	    /* Gain is out of range high */

	    if (cg==3){
		if (fg>255){
		    status = DXP_DETECTOR_GAIN;
		    dxp_log_error("dxp_gaincalc","required coarsegain>3",status);
		    fg=255;
		} 
		break;
	    } else {
		if (cg==1) gc = gc/((float) 3.0);
		else gc = gc/((float) 3.3);
		cg++;
		df = (int) (factor*log10(gc));
		fg = *finegain + df;
	    }
	} else if (fg<10){                   /* need to DECREASE coarse gain  */

	    /* Gain is out of range low */
			
	    if (cg==0){
		if (fg<0){
		    status = DXP_DETECTOR_GAIN;
		    dxp_log_error("dxp_gaincalc","required coarsegain<0",status);
		    fg=0;
		}
		break;
	    } else {
		if (cg==2) gc = gc*((float) 3.0);
		else gc = gc*((float) 3.3);
		cg--;
		df = (int) (factor*log10(gc));
		fg = *finegain + df;
	    }
	}
    }
    *deltafine = (short) (fg - (*finegain));
    *deltacoarse = (short) (cg - (*coarsegain));

    return status;
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
/* int *ioChan;				Input: IO channel of desired channel			*/
/* int *modChan;			Input: channel on module						*/
/* int *module;				Input: module number: for error reporting	*/
/* float *gainchange;		Input: desired gain change					*/
/* Dsp_Info *dsp;			Input: Reference to the DSP structure	*/
{
    int status;
    char info_string[INFO_LEN];
    unsigned short coarsegain,finegain,threshold;
    double temp;
    short deltacoarse,deltafine;

    /* Read out the COARSEGAIN setting from the DSP. */

    status = dxp_read_dspsymbol(ioChan,modChan,	"COARSEGAIN", dsp, &temp);
	if (status != DXP_SUCCESS)
	  {
		sprintf(info_string,
				"Error reading COARSEGAIN from mod %d chan %d",*module,*modChan);
		dxp_log_error("dxp_change_gain",info_string,status);
		return status;
	  }
    coarsegain = (unsigned short) temp;

    /* Read out the FINEGAIN setting from the DSP. */

    status = dxp_read_dspsymbol(ioChan,modChan, "FINEGAIN", dsp, &temp);
	if (status != DXP_SUCCESS)
	  {
		sprintf(info_string,
				"Error reading FINEGAIN to mod %d chan %d",*module,*modChan);
		dxp_log_error("dxp_change_gain",info_string,status);
		return status;
	  }
    finegain = (unsigned short) temp;

    /* Read out the THRESHOLD setting from the DSP. */

    status = dxp_read_dspsymbol(ioChan, modChan, "THRESHOLD", dsp, &temp);
	if (status != DXP_SUCCESS)
	  {
		sprintf(info_string,
				"Error reading THRESHOLD to mod %d chan %d",*module,*modChan);
		dxp_log_error("dxp_change_gain",info_string,status);
		return status;
	  }
    threshold = (unsigned short) temp;

    /* Calculate the new COARSEGAIN and FINEGAIN. */

    status = dxp_perform_gaincalc(gainchange, &coarsegain, &finegain, &deltacoarse, &deltafine);
	if (status != DXP_SUCCESS)
	  {
		sprintf(info_string,
				"DXP module %d Channel %d", *module, *modChan);
		dxp_log_error("dxp_change_gain",info_string,status);
		status=DXP_SUCCESS;
	  }
    finegain = (unsigned short) (finegain + deltafine);
    coarsegain = (unsigned short) (coarsegain + deltacoarse);
	
    /* Change the threshold as well, to accomadate the new gains */
			
    threshold = (unsigned short) ((*gainchange)*((float) threshold)+0.5);

    /* Download the COARSEGAIN, FINEGAIN and THRESHOLD back to the DSP. */
			
    if((status=dxp_modify_dspsymbol(ioChan,modChan,
									"COARSEGAIN",&coarsegain,dsp))!=DXP_SUCCESS){
	  sprintf(info_string,
			  "Error writing COARSEGAIN to mod %d chan %d", *module, *modChan);
	  dxp_log_error("dxp_change_gain",info_string,status);
	  return status;
    }
    if((status=dxp_modify_dspsymbol(ioChan,modChan,
									"FINEGAIN",&finegain,dsp))!=DXP_SUCCESS){
	  sprintf(info_string,
			  "Error writing FINEGAIN to mod %d chan %d", *module, *modChan);
	  dxp_log_error("dxp_change_gain",info_string,status);
	  return status;
    }
    if((status=dxp_modify_dspsymbol(ioChan,modChan,
									"THRESHOLD",&threshold,dsp))!=DXP_SUCCESS){
	  sprintf(info_string,
		"Error writing THRESHOLD to mod %d chan %d", *module, *modChan);
	  dxp_log_error("dxp_change_gain",info_string,status);
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
/* Dsp_Info *dsp;				Input: Reference to the DSP structure	*/
{
    char info_string[INFO_LEN];
    float gain;
    int status;
    unsigned short coarsegain, finegain, vryfingain;
    unsigned short offdacval;
    float *ftemp;

    /* Assign the input variables to stop compiler warnings */
    ftemp = gainmod;

    /*
     *   Following is required gain to achieve desired eV/ADC.  Note: ADC full
     *      scale = ADC_RANGE mV
     */
    gain = (*adcRule*((float) ADC_RANGE)) / *vstep;
    /*
     *  set fine gain and coarse gain DAC values
     *
     */
    if(gain>GAIN3_MAX){
	coarsegain = 3;
	finegain = 255;
	sprintf(info_string,"required coarsegain>3 for module %d, channel %d",
		*module, *modChan);
	status=DXP_DETECTOR_GAIN;
	dxp_log_error("dxp_setup_asc",info_string,status);
    } else if (gain>GAIN3_MIN){
	coarsegain=3;
	finegain= (unsigned short) (((short) (FINEGAIN_FACTOR*log10(gain/GAIN3_FAC))) + 1);
    } else if (gain>GAIN2_MIN){
	coarsegain=2;
	finegain= (unsigned short) (((short) (FINEGAIN_FACTOR*log10(gain/GAIN2_FAC))) + 1);
    } else if (gain>GAIN1_MIN){
	coarsegain=1;
	finegain= (unsigned short) (((short) (FINEGAIN_FACTOR*log10(gain/GAIN1_FAC))) + 1);
    } else if (gain>GAIN0_MIN){
	coarsegain=0;
	finegain= (unsigned short) (((short) (FINEGAIN_FACTOR*log10(gain/GAIN0_MIN)))+ 1);
    } else {
	coarsegain=0;
	finegain=0;
	sprintf(info_string,"required coarsegain<0 for module %d, channel %d",
		*module, *modChan);
	status=DXP_DETECTOR_GAIN;
	dxp_log_error("dxp_setup_asc",info_string,status);
    }
    if (finegain>255) finegain=255;

    /* Write the COARSEGAIN and FINEGAIN data back to the DXP module.  Also 
     * set the VRYFINGAIN value to 128 and write to the module. */
		
    if((status=dxp_modify_dspsymbol(ioChan,modChan,
				    "COARSEGAIN",&coarsegain,dsp))!=DXP_SUCCESS){
	sprintf(info_string,
		"Error writting COARSEGAIN to module %d channel %d",
		*module,*modChan);
	dxp_log_error("dxp_setup_asc",info_string,status);
	return status;
    }
    if((status=dxp_modify_dspsymbol(ioChan,modChan,
				    "FINEGAIN",&finegain,dsp))!=DXP_SUCCESS){
	sprintf(info_string,
		"Error writting FINEGAIN to module %d channel %d",
		*module,*modChan);
	dxp_log_error("dxp_setup_asc",info_string,status);
	return status;
    }
    vryfingain=128;
    if((status=dxp_modify_dspsymbol(ioChan,modChan,
				    "VRYFINGAIN",&vryfingain,dsp))!=DXP_SUCCESS){
	sprintf(info_string,
		"Error writting VRYFINGAIN to module %d channel %d",
		*module,*modChan);
	dxp_log_error("dxp_setup_asc",info_string,status);
	return status;
    }
    /*
     *    Download POLARITY
     */
    if((status=dxp_modify_dspsymbol(ioChan,modChan,
				    "POLARITY",polarity,dsp))!=DXP_SUCCESS){
	sprintf(info_string,
		"Error writting POLARITY to module %d channel %d",
		*module,*modChan);
	dxp_log_error("dxp_setup_asc",info_string,status);
	return status;
    }
    /*
     *    Download OFFDACVAL
     */
    offdacval = (unsigned short) (127 - ((unsigned short) (OFFDAC_FACTOR*0.5*(*vmax+*vmin))));
    if((status=dxp_modify_dspsymbol(ioChan,modChan,
				    "OFFDACVAL",&offdacval,dsp))!=DXP_SUCCESS){
	sprintf(info_string,
		"Error writting OFFDACVAL to module %d channel %d",
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
static int dxp_calibrate_asc(int* mod, int* ioChan, unsigned short* used, 
			     Board *board) 
/* int *mod;						Input: Camac Module number to calibrate			*/
/* int *ioChan;						Input: Camac pointer							*/
/* unsigned short *used;			Input: bitmask of channel numbers to calibrate	*/
/* Board *board;					Input: Relavent Board info						*/
{
    int status;
    int trkdac = CALIB_TRKDAC, reset = CALIB_RESET;

    /*
     *  Perform TRACKDAC gain calibration to see if hardware polarity switch in in
     *  correct position
     */
    dxp_log_info("dxp_user_setup","Beginning internal gain calibration");
    if((status=dxp_calibrate_channel(mod, ioChan, used, &trkdac, board))!=DXP_SUCCESS){
	dxp_log_error("dxp_calibrate_asc","TRKDAC Calibration Error",status);
	return status;
    }
    /*
     *  Finally, perform RESET calibration....
     */
    dxp_log_info("dxp_user_setup","beginning measurement of reset range");
    if((status=dxp_calibrate_channel(mod, ioChan, used, &reset, board))!=DXP_SUCCESS){
	if(status!=DXP_RESET_WARNING){
	    dxp_log_error("dxp_calibrate_asc","RESET Calibration Error",status);
	    return status;
	}
    }

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
static int dxp_calibrate_channel(int* mod, int* ioChan, unsigned short* used, 
				 int* calibtask, Board *board)
/* int *mod;						Input: Camac Module number to calibrate			*/
/* int *ioChan;						Input: Camac pointer							*/
/* unsigned short *used;			Input: bitmask of channel numbers to calibrate	*/
/* int *calibtask;					Input: which calibration function				*/
/* Board *board;					Input: Relavent Board info						*/
{

    int status,status2,chan;
    char info_string[INFO_LEN];
    unsigned short runtasks;
    double temp;
    float one_second=1.0;
    unsigned short addr_RUNTASKS=USHRT_MAX;
    unsigned short runerror,errinfo;
	
    unsigned short params[MAXSYM];

    /*
     *  Loop over each channel and save the RUNTASKS word.  Then start the
     *  calibration run
     */
    status = DXP_SUCCESS;
    for(chan=0;chan<4;chan++){
	if(((*used)&(1<<chan))==0) continue;
	/* Grab the addresses in the DSP for some symbols. */
	status = dxp_loc("RUNTASKS",board->dsp[chan],&addr_RUNTASKS);
	if(status!=DXP_SUCCESS){
	    status = DXP_NOSYMBOL;
	    dxp_log_error("dxp_calibrate_channel"," ",status);
	    return status;
	}

	/* Save the RUNTASKS word */
	if((status2=dxp_read_dspsymbol(ioChan,&chan,"RUNTASKS",board->dsp[chan],&temp))!=DXP_SUCCESS){
	    sprintf(info_string,"Error reading RUNTASKS from mod %d chan %d",
		    *mod,chan);
	    dxp_log_error("dxp_calibrate_channel",info_string,status2);
	    return status2;
	}
	runtasks = (unsigned short) temp;
	/* Perform the calibration on this channel */
	if((status2=dxp_begin_calibrate(ioChan,&chan,calibtask,board))!=DXP_SUCCESS){
	    sprintf(info_string,"Error beginning calibration for mod %d",*mod);
	    dxp_log_error("dxp_calibrate_channel",info_string,status2);
	    return status2;
	}
	/*
	 *   wait a second, then stop the run
	 */
	dxp4c_md_wait(&one_second);
	if((status2=dxp_end_run(ioChan,&chan))!=DXP_SUCCESS){
	    dxp_log_error("dxp_calibrate_channel"," ",status2);
	    return status2;
	}
	/*
	 *  Loop over each of the channels and read out the parameter memory. Check
	 *  to see if there were errors.  Finally, restore the original value of 
	 *  RUNTASKS
	 */

	/* Read out the parameter memory into the Params array */
            
	if((status2=dxp_read_dspparams(ioChan,&chan,board->dsp[chan],params))!=DXP_SUCCESS){
	    sprintf(info_string,
		    "error reading parameters for mod %d chan %d",*mod,chan);
	    dxp_log_error("dxp_calibrate_channel",info_string,status2);
	    return status2;
	}

	/* Check for errors reported by the DSP. */

	if((status2=dxp_decode_error(params,board->dsp[chan],&runerror,&errinfo))!=DXP_SUCCESS){
	    dxp_log_error("dxp_calibrate_channel"," ",status2);
	    return status2;
	}
	if(runerror!=0){
	    status+=DXP_DSPRUNERROR;
	    sprintf(info_string,"DSP error detected for mod %d chan %d",
		    *mod,chan);
	    dxp_log_error("dxp_calibrate_channel",info_string,status);
	    if((status2=dxp_clear_error(ioChan,&chan,board->dsp[chan]))!=DXP_SUCCESS){
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


	/* Now write back the value previously stored in RUNTASKS */            
			
	if((status2=dxp_modify_dspsymbol(ioChan,&chan,
					 "RUNTASKS",&runtasks,board->dsp[chan]))!=DXP_SUCCESS){
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
	name = (char *) dxp4c_md_alloc(sizeof(char)*
				       (strlen(home)+strlen(filename)+2));
	sprintf(name, "%s/%s", home, filename);
	if((fp=fopen(name,mode))!=NULL){
	    dxp4c_md_free(name);
	    return fp;
	}
	dxp4c_md_free(name);
	name = NULL;
    }
    /* Try to open the file with the path DXPHOME */
    if ((home=getenv("DXPHOME"))!=NULL) {
	name = (char *) dxp4c_md_alloc(sizeof(char)*
				       (strlen(home)+strlen(filename)+2));
	sprintf(name, "%s/%s", home, filename);
	if((fp=fopen(name,mode))!=NULL){
	    dxp4c_md_free(name);
	    return fp;
	}
	dxp4c_md_free(name);
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
		
	    name = (char *) dxp4c_md_alloc(sizeof(char)*
					   (strlen(home)+strlen(name2)+2));
	    sprintf(name, "%s/%s", home, name2);
	    if((fp=fopen(name,mode))!=NULL){
		dxp4c_md_free(name);
		return fp;
	    }
	    dxp4c_md_free(name);
	    name = NULL;
	}
    }
    /* Try to open the file with the path DXPHOME and pointing 
     * to a file as an environment variable */
    if ((home=getenv("DXPHOME"))!=NULL) {
	if ((name2=getenv(filename))!=NULL) {
		
	    name = (char *) dxp4c_md_alloc(sizeof(char)*
					   (strlen(home)+strlen(name2)+2));
	    sprintf(name, "%s/%s", home, name2);
	    if((fp=fopen(name,mode))!=NULL) {
		dxp4c_md_free(name);
		return fp;
	    }
	    dxp4c_md_free(name);
	    name = NULL;
	}
    }

    return NULL;
}

/******************************************************************************
 * Routine to swap the the words of an array of (long)s
 * 
 ******************************************************************************/
VOID dxp_swaplong(unsigned int* len, unsigned long* array)
/* unsigned int *len;			Input: Number of elements in array to swap     */
/* unsigned long *array;		I/O: Input/Output of array                   */
{

    unsigned int i;

    for (i=0; i<*len; i++) array[i]=
			       ((array[i]<<16)&0xFFFF0000) | ((array[i]>>16)&0x0000FFFF);
}

/******************************************************************************
 * Routine to determine if the system is little or big endian....
 *
 ******************************************************************************/
static int dxp_little_endian(VOID)
{

    static int endian=-1;
    union{ long Long; char Char[sizeof(long)]; }u;

    if(endian == -1){
	u.Long=1;
	endian = u.Char[0]==1 ? 1 : 0;
    }

    return (endian);

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
 * This routine does nothing currently.
 **********/
static int dxp_read_mem(int *ioChan, int *modChan, Board *board,
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
 * This routine currently does nothing.
 **********/
static int dxp_write_reg(int *ioChan, int *modChan, char *name, unsigned short *data)
{
    UNUSED(ioChan);
    UNUSED(modChan);
    UNUSED(name);
    UNUSED(data);

    return DXP_UNIMPLEMENTED;
}


/**********
 * This routine currently does nothing.
 **********/
XERXES_STATIC int dxp_read_reg(int *ioChan, int *modChan, char *name, unsigned short *data)
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
