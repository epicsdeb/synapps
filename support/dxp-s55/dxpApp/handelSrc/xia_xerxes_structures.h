/*
 *  xia_xerxes_structures.h
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
 * $Id: xia_xerxes_structures.h,v 1.4 2009-07-06 18:24:32 rivers Exp $
 *
 */


#ifndef XIA_XERXES_STRUCTURES_H
#define XIA_XERXES_STRUCTURES_H

#include "xerxesdef.h"
#include "xia_common.h"

/* 
 * Begin XerXes structures 
 */

struct Parameter {
  /* parameter name */
  char *pname;
  /* Address offset of the parameters, typically from Data memory start */
  unsigned int address;
  /* parameter access mode 0=RO, 1=RW, 2=WO */
  unsigned short access;
  /* parameter lower bound value */
  unsigned short lbound;
  /* parameter upper bound value */
  unsigned short ubound;
};
typedef struct Parameter Parameter;


/*
 * Structure containing the DSP parameter names
 */
struct Dsp_Params {
  /* Array of nsymbol parameters */
  struct Parameter *parameters;
  /* number of parameters */
  unsigned short nsymbol;
  /* Need the maximum number of symbols for allocating memory */
  unsigned short maxsym;
  /* Need the maximum symbol length for allocating memory */
  unsigned short maxsymlen;

  /* These members were added to improve support for hardware
   * that doesn't have a 1-to-1 mapping between DSP chips and
   * channels. This means that a single DSP parameter array
   * also has to contain all of the parameters for every channel in
   * the system.
   *
   * The parameters contained above in 'parameters' are assumed to be the global
   * DSP parameters. Similarly, 'nsymbol' refers to the number of global DSP
   * parameters.
   */
  struct Parameter *per_chan_parameters;

  unsigned short n_per_chan_symbols;

  unsigned long *chan_offsets;
};
typedef struct Dsp_Params Dsp_Params;


/*
 * Linked list containing the default parameter information
 * obtained from DSP parameter configuration files
 */
struct Dsp_Defaults {
  /* Filename identifying this entry */
  char *filename;
  /* Structure containing the parameter information, only really used to store
   * the names of the parameters that change from their defaults. */
  struct Dsp_Params *params;
  /* parameter values */
  unsigned short *data;
  /* Pointer to the next entry */
  struct Dsp_Defaults *next;
};
typedef struct Dsp_Defaults Dsp_Defaults;
/*
 *	Linked list to contain all DSP configurations
 */
struct Dsp_Info {
  char *filename;
  unsigned short *data;
  /* Need the number of words in the dsp program (length in size unsigned short) */
  unsigned long proglen;
  /* Need the maximum program length for allocating memory */
  unsigned int maxproglen;
  /* Structure containing parameter information */
  struct Dsp_Params *params;
  struct Dsp_Info *next;
};
typedef struct Dsp_Info Dsp_Info;
/*
 *	Linked list to contain all FiPPi configurations
 */ 
struct Fippi_Info {
  char *filename;
  unsigned short *data;
  /* Need the program length for allocating memory */
  unsigned int proglen;
  /* Need the maximum program length for general information */
  unsigned int maxproglen;
  struct Fippi_Info *next;
};
typedef struct Fippi_Info Fippi_Info;


/* System FiPPI configuration, which is closer to a DSP configuration then
 * an FPGA but still lives in an FPGA chip.
 */
struct System_FiPPI_Info {
  char *filename;

  unsigned long max_data_len;

  unsigned long data_len;
  unsigned short *data;

  Dsp_Params *params;

  struct System_FiPPI_Info *next;
};
typedef struct System_FiPPI_Info System_FiPPI_Info;

/*
 *	Linked list to contain the Preamp Limits
 */ 
struct Preamp_Info {
  /* Typical X-Ray Energy (KeV) */
  float energy;
  /* Fraction of total ADC range to use for a single X-Ray step */
  float adcrule;
  /* Factor to use to modify the gain for this channel */
  float gainmod;
  /* Minimum Voltage level for the Preamp Output (V) */
  float vmin;
  /* Maximum Voltage level for the Preamp Output (V) */
  float vmax;
  /* Polarity of the X-Ray step: 0=negative, 1=positive */
  unsigned short polarity;
  /* Typical step size for a single incident X-Ray (mV) */
  float vstep;
};
typedef struct Preamp_Info Preamp_Info;
/*
 *	Structure containing information about utility routines
 */
struct Utils {
  char *dllname;
  struct Xia_Util_Functions *funcs;
};
typedef struct Utils Utils;
/*
 *	Linked list containing pointers to libraries to interface with boards
 */
struct Interface {
  char *dllname;
  char *ioname;
  struct Xia_Io_Functions *funcs;
  struct Interface *next;
};
typedef struct Interface Interface;
/*
 *	Structure containing information about the state of each channel
 */
struct Chan_State {
  /* keep track of the DSP download state(2=needs update(DSP changed, but not redownloaded), 1=downloaded, 0=not) */
  short dspdownloaded;
  /* Temporary storage for a DSP Parameter (Added to allow the G200 to store general purpose for driver layer usage. */ 
  short parameterTemp;
};
typedef struct Chan_State Chan_State;
/*
 *	Define a system linked list for all boards in use
 */
struct Board {
  /* CAMAC IO channel*/
  int ioChan;
  /* Bit packed integer of which channels are used */
  unsigned short used;
  /* Detector channel ID numbers (defined in DXP_MODULE) */
  int *detChan;
  /* Module ID number (start counting at 0) */
  int mod;
  /* Total number of channels on this board */
  unsigned int nchan;
  /* String passed to the interface library, used to identify the board
   * or if need to restart the interface, for a typical CAMAC system contains
   * the SCSI bus number, crate number and slot number.  */
  char *iostring;
  /* Array to hold misc information about the board state 
   * state[0] = run status (1=run started, 0=no run started)
   * state[1] = gate used (1=ignore gate, 0=use gate)
   * state[2] = lock value (1=locked, 0=free)
   * state[3] = HanDeL update status (1=needs update, 0=up to date)
   * state[4] = Type of control task running (values defined in xerxes_generic.h)
   */
  short state[5];
  /* State structure for each channel of the board
   * chanstate[chan] is used like state[], but contains information about each channel*/
  struct Chan_State *chanstate;
  /* Pointer to the IO interface library structure */
  struct Interface *iface;
  /* Parameter Memory for these channels */
  unsigned short **params;
  /* Pointer to the DSP Program file for each channel */
  struct Dsp_Info **dsp;
  /* Pointer to the DSP Parameter Defaults for each channel */
  struct Dsp_Defaults **defaults;
  /* Pointer to the FiPPi Program file for each channel */
  struct Fippi_Info **fippi;
  /* Pointer to the System FiPPI. */
  struct System_FiPPI_Info *system_fippi;
  /* Pointer to the System FPGA for the module (optional) */
  struct Fippi_Info *system_fpga;
  /* Pointer to the FiPPI A program file (optional) */
  struct Fippi_Info *fippi_a;
  /* Pointer to the Single DSP code for modules with only a single DSP for all
   * of the channels.
   */
  struct Dsp_Info *system_dsp;
  /* Pointer to the MMU Program file for each channel */
  struct Fippi_Info *mmu;
  /* Pointer to the User Defined FiPPi Program file for each channel */
  struct Fippi_Info **user_fippi;
  /* Information about the Preamp associated with each channel */
  struct Preamp_Info *preamp;
  /* Pointer to the structure of function calls */
  struct Board_Info *btype;
  /* Pointer to the array of structures of Firmware Deifinitions */
  /*struct FirmwareSet **firmware;*/
  /* Pointer to next board in the linked list */
  struct Board *next;
};
typedef struct Board Board;
/*
 *  Structure that points at the functions within a board type DLL
 */
typedef int (*DXP_INIT_DRIVER)(Interface *);
typedef int (*DXP_INIT_UTILS)(Utils *);
typedef int (*DXP_GET_DSPCONFIG)(Dsp_Info *);
typedef int (*DXP_GET_DSPINFO)(Dsp_Info *);
typedef int (*DXP_GET_FIPINFO)(void *);
typedef int (*DXP_GET_DEFAULTSINFO)(Dsp_Defaults *);
typedef int (*DXP_GET_FPGACONFIG)(void *);
typedef int (*DXP_DOWNLOAD_FPGACONFIG)(int *ioChan, int *modChan, char *name, Board *board);
typedef int (*DXP_DOWNLOAD_FPGA_DONE)(int *modChan, char *name, Board *board);
typedef int (*DXP_GET_DSPDEFAULTS)(Dsp_Defaults *);
typedef int (*DXP_DOWNLOAD_DSPCONFIG)(int *ioChan, int *modChan, Board *board);
typedef int (*DXP_DOWNLOAD_DSP_DONE)(int *ioChan, int *modChan, int *mod,
									 Board *board, unsigned short *value,
									 float *timeout);
typedef int (*DXP_CALIBRATE_CHANNEL)(int *, int *, unsigned short *, int *, Board *board);
typedef int (*DXP_CALIBRATE_ASC)(int *, int *, unsigned short *, Board *board);
typedef int (*DXP_LOOK_AT_ME)(int *, int *);
typedef int (*DXP_IGNORE_ME)(int *, int *);
typedef int (*DXP_CLEAR_LAM)(int *, int *);
typedef int (*DXP_LOC)(char *, Dsp_Info *, unsigned short *);
typedef int (*DXP_SYMBOLNAME)(unsigned short *, Dsp_Info *, char *);
typedef int (*DXP_READ_SPECTRUM)(int *, int *, Board *, unsigned long *);
typedef int (*DXP_TEST_SPECTRUM_MEMORY)(int *, int *, int *, Board *);
typedef int (*DXP_GET_SPECTRUM_LENGTH)(int *ioChan, int *modChan, Board *board,
									   unsigned int *len);
typedef int (*DXP_READ_SCA)(int *, int *, Board *, unsigned long *);
typedef unsigned int (*DXP_GET_SCA_LENGTH)(Dsp_Info *, unsigned short *);
typedef int (*DXP_READ_BASELINE)(int *ioChan, int *modChan, Board *board,
								 unsigned long *baseline);
typedef int (*DXP_TEST_BASELINE_MEMORY)(int *, int *, int *, Board *);
typedef int (*DXP_GET_BASELINE_LENGTH)(int *modChan, Board *b,
									   unsigned int *len);
typedef int (*DXP_TEST_EVENT_MEMORY)(int *, int *, int *, Board *);
typedef unsigned int (*DXP_GET_EVENT_LENGTH)(Dsp_Info *, unsigned short *);
typedef int (*DXP_WRITE_DSPPARAMS)(int *, int *, Dsp_Info *, unsigned short *);
typedef int (*DXP_WRITE_DSP_PARAM_ADDR)(int *, int *, unsigned int *, unsigned short *);
typedef int (*DXP_READ_DSPPARAMS)(int *ioChan, int *modChan, Board *b,
								  unsigned short *params);
typedef int (*DXP_READ_DSPSYMBOL)(int *, int *, char *, Board *, double *);
typedef int (*DXP_MODIFY_DSPSYMBOL)(int *, int *, char *, unsigned short *,
									Board *);
typedef int (*DXP_DSPPARAM_DUMP)(int *, int *, Dsp_Info *);
typedef int (*DXP_BEGIN_RUN)(int *ioChan, int *modChan, unsigned short *gate, 
							unsigned short *resume, Board *board, int *id);
typedef int (*DXP_END_RUN)(int *ioChan, int *modChan, Board *board);
typedef int (*DXP_RUN_ACTIVE)(int *, int *, int *);
typedef int (*DXP_BEGIN_CONTROL_TASK)(int *ioChan, int *modChan, short *type, unsigned int *length,
									  int *info, Board *board);
typedef int (*DXP_END_CONTROL_TASK)(int *ioChan, int *modChan, Board *board);
typedef int (*DXP_CONTROL_TASK_PARAMS)(int *ioChan, int *modChan, short *type, Board *board, int *info);
typedef int (*DXP_CONTROL_TASK_DATA)(int *ioChan, int *modChan, short *type, Board *board, void *data);
typedef int (*DXP_GET_RUNSTATS)(int *ioChan, int *modChan, Board *b,
								unsigned long *evts, unsigned long *under, 
								unsigned long *over, unsigned long *fast,
								unsigned long *basee, double *live, double *icr,
								double *ocr);
typedef int (*DXP_DECODE_ERROR)(int *ioChan, int *modChan, Dsp_Info *, unsigned short *, unsigned short *);
typedef int (*DXP_CLEAR_ERROR)(int *, int *, Board *);
typedef int (*DXP_CHANGE_GAINS)(int *, int *, int *, float *, Board *);
typedef int (*DXP_SETUP_ASC)(int *, int *, int *, float *, float *, unsigned short *, 
								float *, float *, float *, Board *);
typedef int (*DXP_PREP_FOR_READOUT)(int *, int *);
typedef int (*DXP_DONE_WITH_READOUT)(int *, int *, Board *);
typedef int (*DXP_SETUP_CMD)(Board *, char *, unsigned int *, byte_t *, unsigned int *, byte_t *, byte_t);

typedef int (*DXP_READ_MEM)(int *, int *, Board *, char *, unsigned long *, unsigned long *, unsigned long *);

typedef int (*DXP_WRITE_REG)(int *ioChan, int *modChan, char *name,
							 unsigned long *data);
typedef int (*DXP_READ_REG)(int *ioChan, int *modChan, char *name,
							unsigned long *data);

typedef int (*DXP_DO_CMD)(int *, byte_t, unsigned int, byte_t *, unsigned int, byte_t *);

typedef int (*DXP_UNHOOK)(Board *board);

typedef int (*DXP_WRITE_MEM)(int *, int *, Board *, char *, unsigned long *, unsigned long *, unsigned long *);

typedef int (*DXP_GET_SYMBOL_BY_INDEX)(int modChan, unsigned short index,
									   Board *board, char *name);
typedef int (*DXP_GET_NUM_PARAMS)(int modChan, Board *board,
								  unsigned short *n_params);

struct Functions {
  DXP_INIT_DRIVER dxp_init_driver;
  DXP_INIT_UTILS dxp_init_utils;
  DXP_GET_DSPINFO dxp_get_dspinfo;
  DXP_GET_FIPINFO dxp_get_fipinfo;
  DXP_GET_DEFAULTSINFO dxp_get_defaultsinfo;
  DXP_GET_DSPCONFIG dxp_get_dspconfig;
  DXP_GET_FPGACONFIG dxp_get_fpgaconfig;
  DXP_GET_DSPDEFAULTS dxp_get_dspdefaults;
  DXP_DOWNLOAD_FPGACONFIG dxp_download_fpgaconfig;
  DXP_DOWNLOAD_FPGA_DONE dxp_download_fpga_done;
  DXP_DOWNLOAD_DSPCONFIG dxp_download_dspconfig;
  DXP_DOWNLOAD_DSP_DONE dxp_download_dsp_done;
  DXP_CALIBRATE_CHANNEL dxp_calibrate_channel;
  DXP_CALIBRATE_ASC dxp_calibrate_asc;
  
  DXP_LOOK_AT_ME dxp_look_at_me;
  DXP_IGNORE_ME dxp_ignore_me;
  DXP_CLEAR_LAM dxp_clear_LAM;
  DXP_LOC dxp_loc;
  DXP_SYMBOLNAME dxp_symbolname;
  
  DXP_READ_SPECTRUM dxp_read_spectrum;
  DXP_TEST_SPECTRUM_MEMORY dxp_test_spectrum_memory;
  DXP_GET_SPECTRUM_LENGTH dxp_get_spectrum_length;
  DXP_READ_BASELINE dxp_read_baseline;
  DXP_TEST_BASELINE_MEMORY dxp_test_baseline_memory;
  DXP_GET_BASELINE_LENGTH dxp_get_baseline_length;
  DXP_TEST_EVENT_MEMORY dxp_test_event_memory;
  DXP_GET_EVENT_LENGTH dxp_get_event_length;
  DXP_WRITE_DSPPARAMS dxp_write_dspparams;
  DXP_WRITE_DSP_PARAM_ADDR dxp_write_dsp_param_addr;
  DXP_READ_DSPPARAMS dxp_read_dspparams;
  DXP_READ_DSPSYMBOL dxp_read_dspsymbol;
  DXP_MODIFY_DSPSYMBOL dxp_modify_dspsymbol;
  DXP_DSPPARAM_DUMP dxp_dspparam_dump;
  
  DXP_BEGIN_RUN dxp_begin_run;
  DXP_END_RUN dxp_end_run;
  DXP_RUN_ACTIVE dxp_run_active;
  DXP_BEGIN_CONTROL_TASK dxp_begin_control_task;
  DXP_END_CONTROL_TASK dxp_end_control_task;
  DXP_CONTROL_TASK_PARAMS dxp_control_task_params;
  DXP_CONTROL_TASK_DATA dxp_control_task_data;
  DXP_DECODE_ERROR dxp_decode_error;
  DXP_CLEAR_ERROR dxp_clear_error;
  DXP_GET_RUNSTATS dxp_get_runstats;
  DXP_CHANGE_GAINS dxp_change_gains;
  DXP_SETUP_ASC dxp_setup_asc;

	DXP_PREP_FOR_READOUT dxp_prep_for_readout;
	DXP_DONE_WITH_READOUT dxp_done_with_readout;

  DXP_SETUP_CMD dxp_setup_cmd;

  DXP_READ_MEM dxp_read_mem;
  
  DXP_WRITE_REG dxp_write_reg;

    DXP_DO_CMD dxp_do_cmd;

    DXP_READ_REG dxp_read_reg;
    
    DXP_UNHOOK dxp_unhook;

  DXP_GET_SCA_LENGTH dxp_get_sca_length;
  DXP_READ_SCA dxp_read_sca;
  DXP_WRITE_MEM dxp_write_mem;

  DXP_GET_SYMBOL_BY_INDEX dxp_get_symbol_by_index;
  DXP_GET_NUM_PARAMS dxp_get_num_params;
};
typedef struct Functions Functions;

/*
 *	Linked list to contain information about different board types
 */
struct Board_Info {
  char *name;
  char *pointer;
  int type;
  struct Functions *funcs;
  struct Board_Info *next;
};
typedef struct Board_Info Board_Info;
/* 
 * Define a struct to contain global information 
 */
struct System_Info {
  /* Pointer to the first defined board type in the system */
  struct Board_Info *btypes;
  /* The file that contains the PREAMP configuration */
  char *preamp;
  /* The file that contains the module configurations */
  char *modules;
  /* Pointer to the utils structure */
  Utils *utils;
  /* Tell if the system was started from configuration or restore_config()
   * Hold the status bits of the system
   *
   * 0 = startup from default configuration
   * 1 = startup from saved configuration
   */
  short status;
};
typedef struct System_Info System_Info;

#endif						/* Endif for XIA_XERXES_STRUCTURES_H */
