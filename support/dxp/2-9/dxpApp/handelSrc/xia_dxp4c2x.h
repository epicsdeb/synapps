/*
 *  xia_dxp4c2x.h
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
 *
 *    Following are prototypes for dxp4c2x.c and xerxes.c routines
 */


#ifndef XIA_DXP4C2X_H
#define XIA_DXP4C2X_H

#include "xerxesdef.h"

/* If this is compiled by a C++ compiler, make it clear that these are C routines */
#ifdef __cplusplus
extern "C" {
#endif

#ifdef _XERXES_PROTO_							/* Begin ANSI C prototypes */
XERXES_EXPORT int XERXES_API dxp_init_dxp4c2x(Functions *funcs);
XERXES_STATIC int XERXES_API dxp_init_driver(Interface *);
XERXES_STATIC int XERXES_API dxp_init_utils(Utils *);
XERXES_STATIC int XERXES_API dxp_write_tsar(int *, unsigned short *);
XERXES_STATIC int XERXES_API dxp_write_csr(int *, unsigned short *);
XERXES_STATIC int XERXES_API dxp_read_csr(int *, unsigned short *);
XERXES_STATIC int XERXES_API dxp_write_data(int *, unsigned short *, unsigned int);
XERXES_STATIC int XERXES_API dxp_read_data(int *, unsigned short *, unsigned int);
XERXES_STATIC int XERXES_API dxp_write_fippi(int *, unsigned short *, unsigned int);
XERXES_STATIC int XERXES_API dxp_read_word(int *,int *,unsigned short *,unsigned short *);
XERXES_STATIC int XERXES_API dxp_write_word(int *,int *,unsigned short *,unsigned short *);
XERXES_STATIC int XERXES_API dxp_read_block(int *,int *,unsigned short *,unsigned int *,
					    unsigned short *);
XERXES_STATIC int XERXES_API dxp_write_block(int *,int *, unsigned short *,unsigned int *,
					     unsigned short *);
XERXES_STATIC int XERXES_API dxp_look_at_me(int *ioChan, int *modChan);
XERXES_STATIC int XERXES_API dxp_ignore_me(int *ioChan, int *modChan);
XERXES_STATIC int XERXES_API dxp_clear_LAM(int *ioChan, int *modChan);
XERXES_STATIC int XERXES_API dxp_prep_for_readout(int *, int *);
XERXES_STATIC int XERXES_API dxp_done_with_readout(int *, int *, Board *board);
XERXES_STATIC int dxp_begin_run(int *ioChan, int *modChan, unsigned short *gate,
								unsigned short *resume, Board *board, int *id);
  XERXES_STATIC int dxp_end_run(int *ioChan, int *modChan, Board *board);
XERXES_STATIC int XERXES_API dxp_run_active(int *, int *, int*);
XERXES_STATIC int XERXES_API dxp_begin_control_task(int* ioChan, int* modChan, short *type, 
						    unsigned int *length, int *info, Board *board);
XERXES_STATIC int XERXES_API dxp_end_control_task(int* ioChan, int* modChan, Board *board);
XERXES_STATIC int XERXES_API dxp_control_task_params(int* ioChan, int* modChan, short *type, 
						     Board *board, int *info);
XERXES_STATIC int XERXES_API dxp_control_task_data(int* ioChan, int* modChan, short *type, 
						   Board *board, void *data);
XERXES_STATIC int XERXES_API dxp_loc(char *, Dsp_Info *, unsigned short *);
XERXES_STATIC int XERXES_API dxp_dspparam_dump(int *,int *, Dsp_Info *);
XERXES_STATIC int XERXES_API dxp_begin_calibrate(int *, int *, int *, Board *);
XERXES_STATIC int XERXES_API dxp_test_mem(int *,int *,int *,unsigned int *,unsigned short *);
XERXES_STATIC int XERXES_API dxp_test_spectrum_memory(int *,int *,int *, Board *);
XERXES_STATIC int XERXES_API dxp_test_baseline_memory(int *,int *,int *, Board *);
XERXES_STATIC int XERXES_API dxp_test_event_memory(int *,int *,int *, Board *);

XERXES_STATIC int XERXES_API dxp_get_dspinfo(Dsp_Info *);
XERXES_STATIC int XERXES_API dxp_get_fipinfo(Fippi_Info *);
XERXES_STATIC int XERXES_API dxp_get_defaultsinfo(Dsp_Defaults *);
XERXES_STATIC int XERXES_API dxp_get_fpgaconfig(Fippi_Info *fippi);
XERXES_STATIC int XERXES_API dxp_download_fpga_done(int *modChan, char *name, Board *board); 
XERXES_STATIC int XERXES_API dxp_download_fpgaconfig(int *ioChan, int *modChan, char *name, 
						     Board *board);

XERXES_STATIC int XERXES_API dxp_download_dspconfig(int *,int *, Board *);
XERXES_STATIC int XERXES_API dxp_download_dsp_done(int *, int *, int*, Board *, 
						   unsigned short *, float *);
XERXES_STATIC int XERXES_API dxp_get_dspconfig(Dsp_Info *);
XERXES_STATIC int XERXES_API dxp_get_dspdefaults(Dsp_Defaults *);
XERXES_STATIC int XERXES_API dxp_load_dspfile(FILE *, Dsp_Info *);
XERXES_STATIC int XERXES_API dxp_load_dspsymbol_table(FILE *, Dsp_Info *);
XERXES_STATIC int XERXES_API dxp_load_dspconfig(FILE *, Dsp_Info *);

XERXES_STATIC int XERXES_API dxp_decode_error(unsigned short [], Dsp_Info *, unsigned short *, 
					      unsigned short *);
XERXES_STATIC int XERXES_API dxp_clear_error(int *, int *, Board *);
XERXES_STATIC int XERXES_API dxp_check_calibration(int *, unsigned short *, Dsp_Info *);
  XERXES_STATIC int dxp_get_runstats(int *ioChan, int *modChan, Board *b,
									 unsigned long *evts, unsigned long *under,
									 unsigned long *over, unsigned long *fast,
									 unsigned long *base, double *live,
									 double *icr, double *ocr);
XERXES_STATIC int XERXES_API dxp_symbolname(unsigned short *, Dsp_Info *, char *);

XERXES_STATIC int XERXES_API dxp_modify_dspsymbol(int *, int *, char *, unsigned short *, Board *);
XERXES_STATIC int XERXES_API dxp_write_dsp_param_addr(int *, int *, unsigned int *, unsigned short *);
XERXES_STATIC int XERXES_API dxp_read_dspsymbol(int *, int *, char *, Board *, double *);
XERXES_STATIC int dxp_read_dspparams(int *ioChan, int *modchan, Board *b,
									 unsigned short *params);
XERXES_STATIC int XERXES_API dxp_write_dspparams(int *, int *, Dsp_Info *, unsigned short *);

  XERXES_STATIC int dxp_get_spectrum_length(int *ioChan, int *modChan,
											Board *board, unsigned int *len);
  XERXES_STATIC unsigned int XERXES_API dxp_get_sca_length(Dsp_Info *, unsigned short *);
XERXES_STATIC int dxp_get_baseline_length(int *modChan, Board *b,
										  unsigned int *len);
XERXES_STATIC unsigned int XERXES_API dxp_get_event_length(Dsp_Info *, unsigned short *);
XERXES_STATIC unsigned int XERXES_API dxp_get_history_length(Dsp_Info *, unsigned short *);
XERXES_STATIC int XERXES_API dxp_read_spectrum(int *, int *, Board *, unsigned long *);
XERXES_STATIC int dxp_read_baseline(int *ioChan, int *modChan, Board *board,
									unsigned long *baseline);
  XERXES_STATIC int XERXES_API dxp_read_sca(int *ioChan, int *modChan, Board *board, unsigned long *sca);
/*XERXES_STATIC int XERXES_API dxp_read_event(int *, int *, Board *, unsigned short *);*/
XERXES_STATIC int XERXES_API dxp_read_history(int *, int *, Board *, unsigned short *);
XERXES_STATIC int XERXES_API dxp_write_history(int *, int *, Board *, unsigned int *length, 
											  unsigned short *buffer);
XERXES_STATIC int XERXES_API dxp_perform_gaincalc(float *,unsigned short *,short *);
XERXES_STATIC int XERXES_API dxp_change_gains(int *, int *, int *, float *,
											  Board *);
XERXES_STATIC int XERXES_API dxp_setup_asc(int *, int *, int *, float *, float *, unsigned short *, 
					   float *, float *, float *, Board *);
XERXES_STATIC int XERXES_API dxp_calibrate_asc(int *, int *, unsigned short *, Board *);
XERXES_STATIC int XERXES_API dxp_calibrate_channel(int *, int *, unsigned short *, int *, Board *);

XERXES_STATIC int XERXES_API dxp_setup_cmd(Board *board, char *name, unsigned int *lenS,
					   byte_t *send, unsigned int *lenR, byte_t *receive,
					   byte_t ioFlags);

XERXES_STATIC int XERXES_API dxp_read_mem(int *ioChan, int *modChan, Board *board,
					     char *name, unsigned long *base, unsigned long *offset, unsigned long *data);
XERXES_STATIC int XERXES_API dxp_write_mem(int *ioChan, int *modChan, Board *board,
					     char *name, unsigned long *base, unsigned long *offset, unsigned long *data);

XERXES_STATIC int dxp_write_reg(int *ioChan, int *modChan, char *name, 
								unsigned long *data);
XERXES_STATIC int dxp_read_reg(int *ioChan, int *modChan, char *name,
							   unsigned long *data);

XERXES_STATIC FILE* XERXES_API dxp_find_file(const char *, const char *);

XERXES_STATIC int XERXES_API dxp_do_cmd(int *ioChan, byte_t cmd, unsigned int lenS,
					byte_t *send, unsigned int lenR, byte_t *receive);

XERXES_STATIC int XERXES_API dxp_unhook(Board *board);

  XERXES_STATIC int dxp_get_symbol_by_index(int modChan, unsigned short index,
											Board *board, char *name);
  XERXES_STATIC int dxp_get_num_params(int modChan, Board *b,
									   unsigned short *n_params);

#else									/* Begin old style C prototypes */
XERXES_EXPORT int XERXES_API dxp_init_dxp4c2x();
XERXES_STATIC int XERXES_API dxp_init_driver();
XERXES_STATIC int XERXES_API dxp_init_utils();
XERXES_STATIC int XERXES_API dxp_write_tsar();
XERXES_STATIC int XERXES_API dxp_write_csr();
XERXES_STATIC int XERXES_API dxp_read_csr();
XERXES_STATIC int XERXES_API dxp_write_data();
XERXES_STATIC int XERXES_API dxp_read_data();
XERXES_STATIC int XERXES_API dxp_write_fippi();
XERXES_STATIC int XERXES_API dxp_read_word();
XERXES_STATIC int XERXES_API dxp_write_word();
XERXES_STATIC int XERXES_API dxp_read_block();
XERXES_STATIC int XERXES_API dxp_write_block();
XERXES_STATIC int XERXES_API dxp_disable_LAM();
XERXES_STATIC int XERXES_API dxp_enable_LAM();
XERXES_STATIC int XERXES_API dxp_clear_LAM();
XERXES_STATIC int XERXES_API dxp_download_fpgaconfig();
XERXES_STATIC int XERXES_API dxp_download_dspconfig();
XERXES_STATIC int XERXES_API dxp_download_dsp_done();
XERXES_STATIC int XERXES_API dxp_get_spectrum_length();
  XERXES_STATIC unsigned int XERXES_API dxp_get_sca_length();
XERXES_STATIC int XERXES_API dxp_get_baseline_length();
XERXES_STATIC int XERXES_API dxp_get_event_length();
XERXES_STATIC int XERXES_API dxp_get_history_length();
XERXES_STATIC int XERXES_API dxp_prep_for_readout();
XERXES_STATIC int XERXES_API dxp_done_with_readout();
XERXES_STATIC int XERXES_API dxp_begin_run();
XERXES_STATIC int XERXES_API dxp_end_run();
XERXES_STATIC int XERXES_API dxp_run_active();
XERXES_STATIC int XERXES_API dxp_begin_control_task();
XERXES_STATIC int XERXES_API dxp_end_control_task();
XERXES_STATIC int XERXES_API dxp_control_task_params();
XERXES_STATIC int XERXES_API dxp_control_task_data();
XERXES_STATIC int XERXES_API dxp_loc();
XERXES_STATIC int XERXES_API dxp_dspparam_dump();
XERXES_STATIC int XERXES_API dxp_begin_calibrate();
XERXES_STATIC int XERXES_API dxp_test_mem();
XERXES_STATIC int XERXES_API dxp_test_spectrum_memory();
XERXES_STATIC int XERXES_API dxp_test_baseline_memory();
XERXES_STATIC int XERXES_API dxp_test_event_memory();

XERXES_STATIC int XERXES_API dxp_get_fpgaconfig();
XERXES_STATIC int XERXES_API dxp_download_fpga_done();

XERXES_STATIC int XERXES_API dxp_get_dspconfig();
XERXES_STATIC int XERXES_API dxp_get_dspdefaults();
XERXES_STATIC int XERXES_API dxp_load_dspfile();
XERXES_STATIC int XERXES_API dxp_load_dspsymbol_table();
XERXES_STATIC int XERXES_API dxp_load_dspconfig();

XERXES_STATIC int XERXES_API dxp_decode_error();
XERXES_STATIC int XERXES_API dxp_clear_error();
XERXES_STATIC int XERXES_API dxp_check_calibration();
XERXES_STATIC int XERXES_API dxp_get_runstats();
XERXES_STATIC int XERXES_API dxp_symbolname();
XERXES_STATIC int XERXES_API dxp_modify_dspsymbol();
XERXES_STATIC int XERXES_API dxp_write_dsp_param_addr();
XERXES_STATIC int XERXES_API dxp_read_dspsymbol();
XERXES_STATIC int XERXES_API dxp_read_dspparams();
XERXES_STATIC int XERXES_API dxp_write_dspparams();
XERXES_STATIC int XERXES_API dxp_read_spectrum();
XERXES_STATIC int XERXES_API dxp_read_baseline();
  XERXES_STATIC int XERXES_API dxp_read_sca();
/*XERXES_STATIC int XERXES_API dxp_read_event();*/
XERXES_STATIC int XERXES_API dxp_read_history();
XERXES_STATIC int XERXES_API dxp_write_history();
XERXES_STATIC int XERXES_API dxp_perform_gaincalc();
XERXES_STATIC int XERXES_API dxp_change_gains();
XERXES_STATIC int XERXES_API dxp_setup_asc();
XERXES_STATIC int XERXES_API dxp_calibrate_asc();
XERXES_STATIC int XERXES_API dxp_calibrate_channel();

XERXES_STATIC int XERXES_API dxp_setup_cmd();

XERXES_STATIC int XERXES_API dxp_read_mem();
  XERXES_STATIC int XERXES_API dxp_write_mem();

XERXES_STATIC int XERXES_API dxp_write_reg();
XERXES_STATIC int XERXES_API dxp_read_reg();

XERXES_STATIC FILE* XERXES_API dxp_find_file();

XERXES_STATIC int XERXES_API dxp_do_cmd();

XERXES_STATIC int XERXES_API dxp_unhook();

#endif                                  /*   end if _XERXES_PROTO_ */

#ifdef __cplusplus
}
#endif

/* Logging macro wrappers */
#define dxp_log_error(x, y, z)  dxp4c2x_md_log(MD_ERROR, (x), (y), (z), __FILE__, __LINE__)
#define dxp_log_warning(x, y)	dxp4c2x_md_log(MD_WARNING, (x), (y), 0, __FILE__, __LINE__)
#define dxp_log_info(x, y)	dxp4c2x_md_log(MD_INFO, (x), (y), 0, __FILE__, __LINE__)
#define dxp_log_debug(x, y)	dxp4c2x_md_log(MD_DEBUG, (x), (y), 0, __FILE__, __LINE__)

/** Constants **/
static unsigned short DSP_DATA_MEM_OFFSET = 0x4000;

/* CODEVAR */
#define CV_MULTSCA_SCAN_EXT_COM  10
#define CV_MULTSCA_SCAN_EXT_STD  11
#define CV_MULTMCA_SCAN_EXT_COM  17
#define CV_MULTMCA_SCAN_EXT_STD  18
#define CV_LIST_MODE_EXT         25
#define CV_EXT_MEM_TEST          34

/* BUSY values that can be passed into dxp_wait_for_busy() or used elsewhere */
#define BUSY_READ_EXT_MEM        99

#endif						/* Endif for XIA_DXP4C2X_H */
