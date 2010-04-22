/*
 *  xerxes.h
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
 * $Id: xerxes.h,v 1.4 2009-07-06 18:24:31 rivers Exp $
 *
 *    Following are prototypes for dxp driver routines
 */


#ifndef XERXES_H
#define XERXES_H

/* Define some generic constants for use by XerXes */
#include "xerxes_generic.h"
#include "xia_xerxes_structures.h"
#include "xerxes_structures.h"
#include "xerxesdef.h"

/* If this is compiled by a C++ compiler, make it clear that these are C routines */
#ifdef __cplusplus
extern "C" {
#endif

#ifdef _XERXES_PROTO_
/*
 * Need the initialization routines from the XIA_MD_* library
 */
XERXES_IMPORT int XERXES_API dxp_md_init_util(Xia_Util_Functions *funcs, char *type);
XERXES_IMPORT int XERXES_API dxp_md_init_io(Xia_Io_Functions *funcs, char *type);
/*
 * following are prototypes for xerxes.c routines
 */
XERXES_IMPORT int XERXES_API dxp_initialize(void);
XERXES_IMPORT int XERXES_API dxp_init_ds(void);
XERXES_IMPORT int XERXES_API dxp_set_log_output(char *filename);
XERXES_IMPORT int XERXES_API dxp_init_boards_ds(void);
XERXES_IMPORT int XERXES_API dxp_init_library(void);
XERXES_IMPORT int XERXES_API dxp_install_utils(const char *utilname);
XERXES_IMPORT int XERXES_API dxp_read_config(char *cname);
XERXES_IMPORT int XERXES_API dxp_add_system_item(char *ltoken, char **values);
XERXES_IMPORT int XERXES_API dxp_add_board_item(char *ltoken, char **values);
XERXES_IMPORT int XERXES_API dxp_assign_channel(void);
XERXES_IMPORT int XERXES_API dxp_user_setup(void);
XERXES_IMPORT int XERXES_API dxp_reset_channel(int *detChan);
XERXES_IMPORT int XERXES_API dxp_reset_fpgaconfig(int *detChan, char *name);
XERXES_IMPORT int XERXES_API dxp_del_board(char *type, char *iolib, char *ifacelib, 
					   char *iostring);
XERXES_IMPORT int XERXES_API dxp_del_btype(char *name);
XERXES_IMPORT int XERXES_API dxp_del_dsp(char *filename);
XERXES_IMPORT int XERXES_API dxp_del_fippi(char *filename);
XERXES_IMPORT int XERXES_API dxp_del_defaults(char *filename);
XERXES_IMPORT int XERXES_API dxp_add_board(char *type, char *iolib, char *ifacelib,
					   char *iostring, unsigned short *nchan, int *detChan);
XERXES_IMPORT int XERXES_API dxp_add_btype(char *name, char *pointer, char *dllname);
XERXES_IMPORT int XERXES_API dxp_get_board_type(int *detChan, char *name);
XERXES_IMPORT int XERXES_API dxp_enable_LAM(int *detChan);
XERXES_IMPORT int XERXES_API dxp_disable_LAM(int *detChan);
XERXES_IMPORT int XERXES_API dxp_reset_LAM(int *detChan);
XERXES_IMPORT int XERXES_API dxp_start_run(unsigned short *gate, unsigned short *resume);
XERXES_IMPORT int XERXES_API dxp_resume_run(void);
XERXES_IMPORT int XERXES_API dxp_stop_run(void);
XERXES_IMPORT int XERXES_API dxp_start_one_run(int *detChan, unsigned short *gate, 
					       unsigned short *resume);
XERXES_IMPORT int XERXES_API dxp_resume_one_run(int *detChan);
XERXES_IMPORT int XERXES_API dxp_stop_one_run(int *detChan);
XERXES_IMPORT int XERXES_API dxp_isrunning(int *detChan, int *active);
XERXES_IMPORT int XERXES_API dxp_isrunning_any(int *detChan, int *active);
XERXES_IMPORT int XERXES_API dxp_start_control_task(int *detChan, short *type, 
						    unsigned int *length, int *info);
XERXES_IMPORT int XERXES_API dxp_stop_control_task(int *detChan);
XERXES_IMPORT int XERXES_API dxp_control_task_info(int *detChan, short *type, int *info);
XERXES_IMPORT int XERXES_API dxp_get_control_task_data(int *detChan, short *type, void *data);
XERXES_IMPORT int XERXES_API dxp_readout_detector_run(int *detChan,
													  unsigned short params[],
													  unsigned long baseline[],
													  unsigned long spectrum[]);
  XERXES_IMPORT int XERXES_API dxp_readout_sca(int *detChan, unsigned long *sca);
XERXES_IMPORT int XERXES_API dxp_dspdefaults(int *);
XERXES_IMPORT int XERXES_API dxp_dspconfig(void);
XERXES_IMPORT int XERXES_API dxp_fipconfig(void);
XERXES_IMPORT int XERXES_API dxp_replace_fpgaconfig(int *detChan, char *name, char *filename);
XERXES_IMPORT int XERXES_API dxp_replace_dspconfig(int *, char *);
XERXES_IMPORT int XERXES_API dxp_put_dspparams(void);
XERXES_IMPORT int XERXES_API dxp_replace_dspparams(int *);
XERXES_IMPORT int XERXES_API dxp_dspparams(char *);
XERXES_IMPORT int XERXES_API dxp_upload_dspparams(int *);
XERXES_IMPORT int XERXES_API dxp_get_symbol_index(int* detChan, char* name, unsigned short* symindex);
XERXES_IMPORT int XERXES_API dxp_set_dspsymbol(char *, unsigned short *);
XERXES_IMPORT int XERXES_API dxp_set_one_dspsymbol(int *,char *, unsigned short *);
XERXES_IMPORT int XERXES_API dxp_get_one_dspsymbol(int *,char *, unsigned short *);
XERXES_IMPORT int XERXES_API dxp_get_dspsymbol(int *,char *, double *);
XERXES_IMPORT int XERXES_API dxp_download_params(int *, unsigned short [], unsigned short []);
XERXES_IMPORT int XERXES_API dxp_download_one_params(int *, int *,unsigned short [], 
						     unsigned short []);
XERXES_IMPORT int XERXES_API dxp_upload_channel(int *, unsigned short *,unsigned short *, 
						unsigned long *);
XERXES_IMPORT int XERXES_API dxp_nspec(int *, unsigned int *);
XERXES_IMPORT int XERXES_API dxp_nbase(int *, unsigned int *);
  XERXES_IMPORT int XERXES_API dxp_nsca(int *detChan, unsigned short *scaLen);
XERXES_IMPORT int XERXES_API dxp_nevent(int *, unsigned int *);
XERXES_IMPORT int XERXES_API dxp_ndxp(int *);
XERXES_IMPORT int XERXES_API dxp_ndxpchan(int *);
XERXES_IMPORT int XERXES_API dxp_max_symbols(int *, unsigned short *);
XERXES_IMPORT int XERXES_API dxp_symbolname_list(int *, char **);
XERXES_IMPORT int XERXES_API dxp_symbolname_by_index(int *, unsigned short *, char *);
XERXES_IMPORT int XERXES_API dxp_symbolname_limits(int *, unsigned short *, unsigned short *, 
						   unsigned short *);
XERXES_IMPORT int XERXES_API dxp_check_decimation(unsigned short *,unsigned short *);
XERXES_IMPORT int XERXES_API dxp_calibrate(int *calibtask);
XERXES_IMPORT int XERXES_API dxp_calibrate_one_channel(int *detChan, int *calibtask);
XERXES_IMPORT int XERXES_API dxp_findpeak(long [],int *,float *,int *,int *);
XERXES_IMPORT int XERXES_API dxp_fitgauss0(long [],int *,int *,float *,float *);
XERXES_IMPORT int XERXES_API dxp_save_config(int *);
XERXES_IMPORT int XERXES_API dxp_restore_config(int *);
XERXES_IMPORT int XERXES_API dxp_save_dspparams(int *, int *);
XERXES_IMPORT int XERXES_API dxp_open_file(int *,char *,int *);
XERXES_IMPORT int XERXES_API dxp_close_file(int *);
XERXES_IMPORT int XERXES_API dxp_get_electronics(int *,int [],int []);
XERXES_IMPORT int XERXES_API dxp_get_detectors(int *,int []);
XERXES_IMPORT int XERXES_API dxp_modify_gains(float *);
XERXES_IMPORT int XERXES_API dxp_modify_one_gains(int *, float *);
XERXES_IMPORT int XERXES_API dxp_initialize_ASC(float *,float *);
XERXES_IMPORT int XERXES_API dxp_mem_dump(int *);
XERXES_IMPORT int XERXES_API dxp_get_statistics(int *, double *, double *, double *, unsigned long *);
XERXES_IMPORT int XERXES_API dxp_replace_filesymbol(char *, char *);
XERXES_IMPORT int XERXES_API dxp_read_filesymbol(char *, char *);
XERXES_IMPORT int XERXES_API dxp_locate_system_files(unsigned int *, char **);
XERXES_IMPORT int XERXES_API dxp_locate_channel_files(int *, unsigned int *, char **);
XERXES_IMPORT int XERXES_API dxp_lock_resource(int *detChan, short *lock);
XERXES_IMPORT int XERXES_API dxp_enable_log(void);
XERXES_IMPORT int XERXES_API dxp_suppress_log(void);
XERXES_IMPORT int XERXES_API dxp_set_log_level(int *level);
XERXES_IMPORT int XERXES_API dxp_set_log_output(char *filename);

XERXES_IMPORT int XERXES_API dxp_setup_command(int *detChan, char *name, unsigned int *lenS,
					       byte_t *send, unsigned int *lenR,
					       byte_t *receive, byte_t *ioFlags);

XERXES_IMPORT int XERXES_API dxp_read_memory(int *detChan, char *name, unsigned long *data);
XERXES_IMPORT int XERXES_API dxp_write_memory(int *detChan, char *name, unsigned long *data);
XERXES_IMPORT int XERXES_API dxp_memory_length(int *detChan, char *name, unsigned long *length);

XERXES_IMPORT int XERXES_API dxp_write_register(int *detChan, char *name,
												unsigned long *data);
XERXES_IMPORT int XERXES_API dxp_read_register(int *detChan, char *name,
											   unsigned long *data);

XERXES_IMPORT int XERXES_API dxp_cmd(int *detChan, byte_t *cmd, unsigned int *lenS, byte_t *send,
				     unsigned int *lenR, byte_t *receive);

XERXES_IMPORT int XERXES_API dxp_exit(int *detChan);


XERXES_IMPORT int XERXES_API dxp_set_io_priority(int *priority);

XERXES_IMPORT void XERXES_API dxp_get_version_info(int *rel, int *min,
													 int *maj, char *pretty);

/* Deprecated functions */
XERXES_IMPORT int XERXES_API dxp_replace_fipconfig(int *detChan, char *filename);
XERXES_IMPORT int XERXES_API dxp_reset_fipconfig(int *detChan);
#else									/* Begin old style C prototypes */
/*
 * Need the initialization routines from the XIA_MD_* library
 */
XERXES_IMPORT int XERXES_API dxp_md_init_util();
XERXES_IMPORT int XERXES_API dxp_md_init_io();
/*
 * following are prototypes for xerxes.c routines
 */
XERXES_IMPORT int XERXES_API dxp_initialize();
XERXES_IMPORT int XERXES_API dxp_init_ds();
XERXES_IMPORT int XERXES_API dxp_set_log_output();
XERXES_IMPORT int XERXES_API dxp_init_boards_ds();
XERXES_IMPORT int XERXES_API dxp_init_library();
XERXES_IMPORT int XERXES_API dxp_install_utils();
XERXES_IMPORT int XERXES_API dxp_read_config();
XERXES_IMPORT int XERXES_API dxp_add_system_item();
XERXES_IMPORT int XERXES_API dxp_add_board_item();
XERXES_IMPORT int XERXES_API dxp_assign_channel();
XERXES_IMPORT int XERXES_API dxp_user_setup();
XERXES_IMPORT int XERXES_API dxp_reset_channel();
XERXES_IMPORT int XERXES_API dxp_reset_fpgaconfig();
XERXES_IMPORT int XERXES_API dxp_add_board();
XERXES_IMPORT int XERXES_API dxp_add_btype();
XERXES_IMPORT int XERXES_API dxp_get_board_type();
XERXES_IMPORT int XERXES_API dxp_enable_LAM();
XERXES_IMPORT int XERXES_API dxp_disable_LAM();
XERXES_IMPORT int XERXES_API dxp_reset_LAM();
XERXES_IMPORT int XERXES_API dxp_start_run();
XERXES_IMPORT int XERXES_API dxp_resume_run();
XERXES_IMPORT int XERXES_API dxp_stop_run();
XERXES_IMPORT int XERXES_API dxp_start_one_run();
XERXES_IMPORT int XERXES_API dxp_resume_one_run();
XERXES_IMPORT int XERXES_API dxp_stop_one_run();
XERXES_IMPORT int XERXES_API dxp_isrunning();
XERXES_IMPORT int XERXES_API dxp_isrunning_any();
XERXES_IMPORT int XERXES_API dxp_start_control_task();
XERXES_IMPORT int XERXES_API dxp_stop_control_task();
XERXES_IMPORT int XERXES_API dxp_control_task_info();
XERXES_IMPORT int XERXES_API dxp_get_control_task_data();
XERXES_IMPORT int XERXES_API dxp_readout_detector_run();
XERXES_IMPORT int XERXES_API dxp_readout_sca();
XERXES_IMPORT int XERXES_API dxp_dspconfig();
XERXES_IMPORT int XERXES_API dxp_dspdefaults();
XERXES_IMPORT int XERXES_API dxp_fipconfig();
XERXES_IMPORT int XERXES_API dxp_replace_fpgaconfig();
XERXES_IMPORT int XERXES_API dxp_replace_dspconfig();
XERXES_IMPORT int XERXES_API dxp_put_dspparams();
XERXES_IMPORT int XERXES_API dxp_replace_dspparams();
XERXES_IMPORT int XERXES_API dxp_dspparams();
XERXES_IMPORT int XERXES_API dxp_upload_dspparams();
XERXES_IMPORT int XERXES_API dxp_get_symbol_index();
XERXES_IMPORT int XERXES_API dxp_set_dspsymbol();
XERXES_IMPORT int XERXES_API dxp_set_one_dspsymbol();
XERXES_IMPORT int XERXES_API dxp_get_one_dspsymbol();
XERXES_IMPORT int XERXES_API dxp_get_dspsymbol();
XERXES_IMPORT int XERXES_API dxp_download_params();
XERXES_IMPORT int XERXES_API dxp_download_one_params();
XERXES_IMPORT int XERXES_API dxp_nspec();
XERXES_IMPORT int XERXES_API dxp_nbase();
XERXES_IMPORT int XERXES_API dxp_nsca();
XERXES_IMPORT int XERXES_API dxp_nevent();
XERXES_IMPORT int XERXES_API dxp_ndxp();
XERXES_IMPORT int XERXES_API dxp_ndxpchan();
XERXES_IMPORT int XERXES_API dxp_max_symbols();
XERXES_IMPORT int XERXES_API dxp_symbolname_list();
XERXES_IMPORT int XERXES_API dxp_symbolname_by_index();
XERXES_IMPORT int XERXES_API dxp_symbolname_limits();
XERXES_IMPORT int XERXES_API dxp_check_decimation();
XERXES_IMPORT int XERXES_API dxp_calibrate();
XERXES_IMPORT int XERXES_API dxp_calibrate_one_channel();
XERXES_IMPORT int XERXES_API dxp_findpeak();
XERXES_IMPORT int XERXES_API dxp_fitgauss0();
XERXES_IMPORT int XERXES_API dxp_save_config();
XERXES_IMPORT int XERXES_API dxp_restore_config();
XERXES_IMPORT int XERXES_API dxp_save_dspparams();
XERXES_IMPORT int XERXES_API dxp_open_file();
XERXES_IMPORT int XERXES_API dxp_close_file();
XERXES_IMPORT int XERXES_API dxp_get_electronics();
XERXES_IMPORT int XERXES_API dxp_get_detectors();
XERXES_IMPORT int XERXES_API dxp_modify_gains();
XERXES_IMPORT int XERXES_API dxp_modify_one_gains();
XERXES_IMPORT int XERXES_API dxp_initialize_ASC();
XERXES_IMPORT int XERXES_API dxp_mem_dump();
XERXES_IMPORT int XERXES_API dxp_get_statistics();
XERXES_IMPORT int XERXES_API dxp_replace_filesymbol();
XERXES_IMPORT int XERXES_API dxp_read_filesymbol();
XERXES_IMPORT int XERXES_API dxp_locate_system_files();
XERXES_IMPORT int XERXES_API dxp_locate_channel_files();
XERXES_IMPORT int XERXES_API dxp_lock_resource();
XERXES_IMPORT int XERXES_API dxp_enable_log();
XERXES_IMPORT int XERXES_API dxp_suppress_log();
XERXES_IMPORT int XERXES_API dxp_set_log_level();
XERXES_IMPORT int XERXES_API dxp_set_log_output();

XERXES_IMPORT int XERXES_API dxp_setup_command(); 

XERXES_IMPORT int XERXES_API dxp_read_memory();
XERXES_IMPORT int XERXES_API dxp_write_memory();
XERXES_IMPORT int XERXES_API dxp_memory_length();

XERXES_IMPORT int XERXES_API dxp_write_register();
XERXES_IMPORT int XERXES_API dxp_read_register();

XERXES_IMPORT int XERXES_API dxp_cmd();

XERXES_IMPORT int XERXES_API dxp_exit();


XERXES_IMPORT void XERXES_API dxp_get_version_info();

/* Deprecated functions */
XERXES_IMPORT int XERXES_API dxp_replace_fipconfig();
XERXES_IMPORT int XERXES_API dxp_reset_fipconfig();
#endif                                  /*   end if _XERXES_PROTO_ */

/* If this is compiled by a C++ compiler, make it clear that these are C routines */
#ifdef __cplusplus
}
#endif

XERXES_IMPORT Utils *utils;


#endif						/* Endif for XERXES_H */
