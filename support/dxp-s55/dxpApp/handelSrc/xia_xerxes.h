/*
 *  xia_xerxes.h
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
 * $Id: xia_xerxes.h,v 1.4 2009-07-06 18:24:32 rivers Exp $
 *
 *    Following are prototypes for dxp driver routines
 */


#ifndef XIA_XERXES_H
#define XIA_XERXES_H

/* Define some generic constants for use by XerXes */
#include "xerxes_generic.h"

/* Include structure typedefs for exporting of global variables */
#include "xia_xerxes_structures.h"
#include "xerxes_structures.h"

#include "xerxesdef.h"

#include "xia_common.h"

/*
 *    CAMAC status Register control codes
 */
#define ALLCHAN              -1

/* If this is compiled by a C++ compiler, make it clear that these are C routines */
#ifdef __cplusplus
extern "C"
{
#endif

#ifdef _XERXES_PROTO_
#include <stdio.h>

  /*
   * following are internal prototypes for xerxes.c routines
   */
  XERXES_EXPORT int XERXES_API dxp_initialize(void);
  XERXES_EXPORT int XERXES_API dxp_init_ds(void);
  XERXES_EXPORT int XERXES_API dxp_enable_log(void);
  XERXES_EXPORT int XERXES_API dxp_suppress_log(void);
  XERXES_EXPORT int XERXES_API dxp_set_log_level(int *level);
  XERXES_EXPORT int XERXES_API dxp_set_log_output(char *filename);
  XERXES_EXPORT int XERXES_API dxp_init_boards_ds(void);
  XERXES_EXPORT int XERXES_API dxp_init_library(void);
  XERXES_EXPORT int XERXES_API dxp_install_utils(const char *utilname);
  XERXES_EXPORT int XERXES_API dxp_read_config(char *cname);
  XERXES_EXPORT int XERXES_API dxp_add_system_item(char *ltoken, char **values);
  XERXES_EXPORT int XERXES_API dxp_add_board_item(char *ltoken, char **values);
  XERXES_EXPORT int XERXES_API dxp_assign_channel(void);
  XERXES_EXPORT int XERXES_API dxp_user_setup(void);
  XERXES_EXPORT int XERXES_API dxp_reset_channel(int *detChan);
  XERXES_EXPORT int XERXES_API dxp_reset_fpgaconfig(int *detChan, char *name);
  XERXES_EXPORT int XERXES_API dxp_del_board(char *type, char *iolib, char *ifacelib,
                                             char *iostring);
  XERXES_EXPORT int XERXES_API dxp_del_btype(char *name);
  XERXES_EXPORT int XERXES_API dxp_del_dsp(char *filename);
  XERXES_EXPORT int XERXES_API dxp_del_fippi(char *filename);
  XERXES_EXPORT int XERXES_API dxp_del_defaults(char *filename);
  XERXES_EXPORT int XERXES_API dxp_add_board(char *type, char *iolib, char *ifacelib,
                                             char *iostring, unsigned short *nchan, int *detChan);
  XERXES_EXPORT int XERXES_API dxp_add_btype(char *name, char *pointer, char *dllname);
  XERXES_EXPORT int XERXES_API dxp_get_board_type(int *detChan, char *name);
  XERXES_EXPORT int XERXES_API dxp_enable_LAM(int *detChan);
  XERXES_EXPORT int XERXES_API dxp_disable_LAM(int *detChan);
  XERXES_EXPORT int XERXES_API dxp_reset_LAM(int *detChan);
  XERXES_EXPORT int XERXES_API dxp_start_run(unsigned short *gate, unsigned short *resume);
  XERXES_EXPORT int XERXES_API dxp_resume_run(void);
  XERXES_EXPORT int XERXES_API dxp_stop_run(void);
  XERXES_EXPORT int XERXES_API dxp_start_one_run(int *detChan, unsigned short *gate,
                                                 unsigned short *resume);
  XERXES_EXPORT int XERXES_API dxp_resume_one_run(int *detChan);
  XERXES_EXPORT int XERXES_API dxp_stop_one_run(int *detChan);
  XERXES_EXPORT int XERXES_API dxp_isrunning(int *detChan, int *active);
  XERXES_EXPORT int XERXES_API dxp_isrunning_any(int *detChan, int *active);
  XERXES_EXPORT int XERXES_API dxp_start_control_task(int *detChan, short *type,
      unsigned int *length, int *info);
  XERXES_EXPORT int XERXES_API dxp_stop_control_task(int *detChan);
  XERXES_EXPORT int XERXES_API dxp_control_task_info(int *detChan, short *type, int *info);
  XERXES_EXPORT int XERXES_API dxp_get_control_task_data(int *detChan, short *type, void *data);
  XERXES_EXPORT int XERXES_API dxp_readout_detector_run(int *detChan,
      unsigned short params[],
      unsigned long baseline[],
      unsigned long spectrum[]);
  XERXES_EXPORT int XERXES_API dxp_readout_sca(int *detChan, unsigned long *sca);
  XERXES_EXPORT int XERXES_API dxp_dspdefaults(int *);
  XERXES_EXPORT int XERXES_API dxp_dspconfig(void);
  XERXES_EXPORT int XERXES_API dxp_fipconfig(void);
  XERXES_EXPORT int XERXES_API dxp_replace_fpgaconfig(int *detChan, char *name, char *filename);
  XERXES_EXPORT int XERXES_API dxp_replace_dspconfig(int *detChan, char *filename);
  XERXES_EXPORT int XERXES_API dxp_put_dspparams(void);
  XERXES_EXPORT int XERXES_API dxp_replace_dspparams(int *);
  XERXES_EXPORT int XERXES_API dxp_dspparams(char *);
  XERXES_EXPORT int XERXES_API dxp_upload_dspparams(int *);
  XERXES_EXPORT int XERXES_API dxp_get_symbol_index(int* detChan, char* name, unsigned short* symindex);
  XERXES_EXPORT int XERXES_API dxp_set_dspsymbol(char *, unsigned short *);
  XERXES_EXPORT int XERXES_API dxp_set_one_dspsymbol(int *,char *, unsigned short *);
  XERXES_EXPORT int XERXES_API dxp_get_one_dspsymbol(int *,char *, unsigned short *);
  XERXES_EXPORT int XERXES_API dxp_get_dspsymbol(int *,char *, double *);
  XERXES_EXPORT int XERXES_API dxp_download_params(int *, unsigned short [], unsigned short []);
  XERXES_EXPORT int XERXES_API dxp_download_one_params(int *, int *,unsigned short [],
      unsigned short []);
  XERXES_EXPORT int XERXES_API dxp_upload_channel(int *, unsigned short *,unsigned short *,
                                                  unsigned long *);
  XERXES_EXPORT int XERXES_API dxp_nspec(int *, unsigned int *);
  XERXES_EXPORT int XERXES_API dxp_nbase(int *, unsigned int *);
  XERXES_EXPORT int XERXES_API dxp_nsca(int *detChan, unsigned short *nSCA);
  XERXES_EXPORT int XERXES_API dxp_nevent(int *, unsigned int *);
  XERXES_EXPORT int XERXES_API dxp_ndxp(int *);
  XERXES_EXPORT int XERXES_API dxp_ndxpchan(int *);
  XERXES_EXPORT int XERXES_API dxp_max_symbols(int *, unsigned short *);
  XERXES_EXPORT int XERXES_API dxp_symbolname_list(int *, char **);
  XERXES_EXPORT int XERXES_API dxp_symbolname_by_index(int *, unsigned short *, char *);
  XERXES_EXPORT int XERXES_API dxp_symbolname_limits(int *, unsigned short *, unsigned short *,
      unsigned short *);
  XERXES_EXPORT int XERXES_API dxp_check_decimation(unsigned short *,unsigned short *);
  XERXES_EXPORT int XERXES_API dxp_calibrate(int *calibtask);
  XERXES_EXPORT int XERXES_API dxp_calibrate_one_channel(int *detChan, int *calibtask);
  XERXES_EXPORT int XERXES_API dxp_findpeak(long [],int *,float *,int *,int *);
  XERXES_EXPORT int XERXES_API dxp_fitgauss0(long [],int *,int *,float *,float *);
  XERXES_EXPORT int XERXES_API dxp_save_config(int *);
  XERXES_EXPORT int XERXES_API dxp_restore_config(int *);
  XERXES_EXPORT int XERXES_API dxp_save_dspparams(int *, int *);
  XERXES_EXPORT int XERXES_API dxp_open_file(int *,char *,int *);
  XERXES_EXPORT int XERXES_API dxp_close_file(int *);
  XERXES_EXPORT int XERXES_API dxp_get_electronics(int *,int [],int []);
  XERXES_EXPORT int XERXES_API dxp_get_detectors(int *,int []);
  XERXES_EXPORT int XERXES_API dxp_modify_gains(float *);
  XERXES_EXPORT int XERXES_API dxp_modify_one_gains(int *, float *);
  XERXES_EXPORT int XERXES_API dxp_initialize_ASC(float *,float *);
  XERXES_EXPORT int XERXES_API dxp_det_to_elec(int *,Board **,int *);
  XERXES_EXPORT int XERXES_API dxp_elec_to_det(int *,int *,int *);
  XERXES_EXPORT int XERXES_API dxp_mem_dump(int *);
  XERXES_EXPORT int XERXES_API dxp_get_statistics(int *, double *, double *, double *, unsigned long *);
  XERXES_EXPORT int XERXES_API dxp_replace_filesymbol(char *, char *);
  XERXES_EXPORT int XERXES_API dxp_read_filesymbol(char *, char *);
  XERXES_EXPORT int XERXES_API dxp_locate_system_files(unsigned int *, char **);
  XERXES_EXPORT int XERXES_API dxp_locate_channel_files(int *, unsigned int *, char **);
  XERXES_EXPORT int XERXES_API dxp_lock_resource(int *detChan, short *lock );

  XERXES_EXPORT int XERXES_API dxp_setup_command(int *detChan, char *name, unsigned int *lenS,
                                                 byte_t *send, unsigned int *lenR,
                                                 byte_t *receive, byte_t *ioFlags);
  XERXES_EXPORT int XERXES_API dxp_read_memory(int *detChan, char *name, unsigned long *data);
  XERXES_EXPORT int XERXES_API dxp_write_memory(int *detChan, char *name, unsigned long*data);
  XERXES_EXPORT int XERXES_API dxp_memory_length(int *detChan, char *name, unsigned long *data);

  XERXES_EXPORT int XERXES_API dxp_write_register(int *detChan, char *name,
                                                  unsigned long *data);
  XERXES_EXPORT int XERXES_API dxp_read_register(int *detChan, char *name,
                                                 unsigned long *data);
  XERXES_EXPORT int XERXES_API dxp_cmd(int *detChan, byte_t *cmd, unsigned int *lenS, byte_t *send,
                                       unsigned int *lenR, byte_t *receive);
  XERXES_EXPORT int XERXES_API dxp_exit(int *detChan);


  XERXES_EXPORT int XERXES_API dxp_set_io_priority(int *priority);

  XERXES_EXPORT void XERXES_API dxp_get_version_info(int *rel, int *min,
      int *maj, char *pretty);

#ifndef EXCLUDE_DXP4C2X
  XERXES_IMPORT int DXP_API dxp_init_dxp4c2x(Functions *funcs);
#endif /* EXCLUDE_DXP4C2X */
#ifndef EXCLUDE_DXPX10P
  XERXES_IMPORT int DXP_API dxp_init_dxpx10p(Functions *funcs);
#endif /* EXCLUDE_DXPX10P */
#ifndef EXCLUDE_UDXPS
  XERXES_IMPORT int DXP_API dxp_init_udxps(Functions *funcs);
#endif /* EXCLUDE_UDXPS */
#ifndef EXCLUDE_UDXP
  XERXES_IMPORT int DXP_API dxp_init_udxp(Functions *funcs);
#endif /* EXCLUDE_UDXP */
#ifndef EXCLUDE_XMAP
  XERXES_IMPORT int DXP_API dxp_init_xmap(Functions *funcs);
#endif /* EXCLUDE_XMAP */
#ifndef EXCLUDE_VEGA
  XERXES_IMPORT int DXP_API dxp_init_vega(Functions *funcs);
#endif /* EXCLUDE_VEGA */
#ifndef EXCLUDE_MERCURY
  XERXES_IMPORT int DXP_API dxp_init_mercury(Functions *funcs);
#endif /* EXCLUDE_MERCURY */

  XERXES_IMPORT int MD_API dxp_md_init_util(Xia_Util_Functions *funcs, char *type);
  XERXES_IMPORT int MD_API dxp_md_init_io(Xia_Io_Functions *funcs, char *type);

  /* Routines contained in xia_common.c.  Routines that are used across libraries but not exported */

  /* Deprecated functions */
  XERXES_EXPORT int XERXES_API dxp_replace_fipconfig(int *detChan, char *filename);
  XERXES_EXPORT int XERXES_API dxp_reset_fipconfig(int *detChan);
#else         /* Begin old style C prototypes */
  /*
  * following are internal prototypes for xerxes.c routines
  */
  XERXES_EXPORT int XERXES_API dxp_initialize();
  XERXES_EXPORT int XERXES_API dxp_init_ds();
  XERXES_EXPORT int XERXES_API dxp_enable_log();
  XERXES_EXPORT int XERXES_API dxp_suppress_log();
  XERXES_EXPORT int XERXES_API dxp_set_log_level();
  XERXES_EXPORT int XERXES_API dxp_set_log_output();
  XERXES_EXPORT int XERXES_API dxp_init_boards_ds();
  XERXES_EXPORT int XERXES_API dxp_init_library();
  XERXES_EXPORT int XERXES_API dxp_install_utils();
  XERXES_EXPORT int XERXES_API dxp_read_config();
  XERXES_EXPORT int XERXES_API dxp_add_system_item();
  XERXES_EXPORT int XERXES_API dxp_add_boards_item();
  XERXES_EXPORT int XERXES_API dxp_init_ds();
  XERXES_EXPORT int XERXES_API dxp_assign_channel();
  XERXES_EXPORT int XERXES_API dxp_user_setup();
  XERXES_EXPORT int XERXES_API dxp_reset_channel();
  XERXES_EXPORT int XERXES_API dxp_reset_fpgaconfig();
  XERXES_EXPORT int XERXES_API dxp_add_board();
  XERXES_EXPORT int XERXES_API dxp_add_btype();
  XERXES_EXPORT int XERXES_API dxp_get_board_type();
  XERXES_EXPORT int XERXES_API dxp_enable_LAM();
  XERXES_EXPORT int XERXES_API dxp_disable_LAM();
  XERXES_EXPORT int XERXES_API dxp_reset_LAM();
  XERXES_EXPORT int XERXES_API dxp_start_run();
  XERXES_EXPORT int XERXES_API dxp_resume_run();
  XERXES_EXPORT int XERXES_API dxp_stop_run();
  XERXES_EXPORT int XERXES_API dxp_start_one_run();
  XERXES_EXPORT int XERXES_API dxp_resume_one_run();
  XERXES_EXPORT int XERXES_API dxp_stop_one_run();
  XERXES_EXPORT int XERXES_API dxp_isrunning();
  XERXES_EXPORT int XERXES_API dxp_isrunning_any();
  XERXES_EXPORT int XERXES_API dxp_start_control_task();
  XERXES_EXPORT int XERXES_API dxp_stop_control_task();
  XERXES_EXPORT int XERXES_API dxp_control_task_info();
  XERXES_EXPORT int XERXES_API dxp_get_control_task_data();
  XERXES_EXPORT int XERXES_API dxp_readout_detector_run();
  XERXES_EXPORT int XERXES_API dxp_readout_sca();
  XERXES_EXPORT int XERXES_API dxp_dspconfig();
  XERXES_EXPORT int XERXES_API dxp_dspdefaults();
  XERXES_EXPORT int XERXES_API dxp_fipconfig();
  XERXES_EXPORT int XERXES_API dxp_replace_fpgaconfig();
  XERXES_EXPORT int XERXES_API dxp_replace_dspconfig();
  XERXES_EXPORT int XERXES_API dxp_put_dspparams();
  XERXES_EXPORT int XERXES_API dxp_replace_dspparams();
  XERXES_EXPORT int XERXES_API dxp_dspparams();
  XERXES_EXPORT int XERXES_API dxp_upload_dspparams();
  XERXES_EXPORT int XERXES_API dxp_get_symbol_index();
  XERXES_EXPORT int XERXES_API dxp_set_dspsymbol();
  XERXES_EXPORT int XERXES_API dxp_set_one_dspsymbol();
  XERXES_EXPORT int XERXES_API dxp_get_one_dspsymbol();
  XERXES_EXPORT int XERXES_API dxp_get_dspsymbol();
  XERXES_EXPORT int XERXES_API dxp_download_params();
  XERXES_EXPORT int XERXES_API dxp_download_one_params();
  XERXES_EXPORT int XERXES_API dxp_upload_channel();
  XERXES_EXPORT int XERXES_API dxp_nspec();
  XERXES_EXPORT int XERXES_API dxp_nbase();
  XERXES_EXPORT int XERXES_API dxp_nsca();
  XERXES_EXPORT int XERXES_API dxp_nevent();
  XERXES_EXPORT int XERXES_API dxp_ndxp();
  XERXES_EXPORT int XERXES_API dxp_ndxpchan();
  XERXES_EXPORT int XERXES_API dxp_max_symbols();
  XERXES_EXPORT int XERXES_API dxp_symbolname_list();
  XERXES_EXPORT int XERXES_API dxp_symbolname_by_index();
  XERXES_EXPORT int XERXES_API dxp_symbolname_limits();
  XERXES_EXPORT int XERXES_API dxp_check_decimation();
  XERXES_EXPORT int XERXES_API dxp_calibrate();
  XERXES_EXPORT int XERXES_API dxp_calibrate_one_channel();
  XERXES_EXPORT int XERXES_API dxp_findpeak();
  XERXES_EXPORT int XERXES_API dxp_fitgauss0();
  XERXES_EXPORT int XERXES_API dxp_save_config();
  XERXES_EXPORT int XERXES_API dxp_restore_config();
  XERXES_EXPORT int XERXES_API dxp_save_dspparams();
  XERXES_EXPORT int XERXES_API dxp_open_file();
  XERXES_EXPORT int XERXES_API dxp_close_file();
  XERXES_EXPORT int XERXES_API dxp_get_electronics();
  XERXES_EXPORT int XERXES_API dxp_get_detectors();
  XERXES_EXPORT int XERXES_API dxp_modify_gains();
  XERXES_EXPORT int XERXES_API dxp_modify_one_gains();
  XERXES_EXPORT int XERXES_API dxp_initialize_ASC();
  XERXES_EXPORT int XERXES_API dxp_det_to_elec();
  XERXES_EXPORT int XERXES_API dxp_elec_to_det();
  XERXES_EXPORT int XERXES_API dxp_mem_dump();
  XERXES_EXPORT int XERXES_API dxp_get_statistics();
  XERXES_EXPORT int XERXES_API dxp_replace_filesymbol();
  XERXES_EXPORT int XERXES_API dxp_read_filesymbol();
  XERXES_EXPORT int XERXES_API dxp_locate_system_files();
  XERXES_EXPORT int XERXES_API dxp_locate_channel_files();
  XERXES_EXPORT int XERXES_API dxp_lock_resource();

  XERXES_EXPORT int XERXES_API dxp_setup_command();
  XERXES_EXPORT int XERXES_API dxp_cmd();

  XERXES_EXPORT int XERXES_API dxp_read_memory();
  XERXES_EXPORT int XERXES_API dxp_write_memory();
  XERXES_EXPORT int XERXES_API dxp_memory_length();

  XERXES_EXPORT int XERXES_API dxp_write_register();
  XERXES_EXPORT int XERXES_API dxp_read_register();

  XERXES_EXPORT int XERXES_API dxp_unhook();

  XERXES_EXPORT int XERXES_API dxp_exit();


  XERXES_EXPORT int XERXES_API dxp_set_io_priority();

  XERXES_EXPORT int XERXES_API dxp_get_version_info();

#ifndef EXCLUDE_DXP4C2X
  XERXES_IMPORT int DXP_API dxp_init_dxp4c2x();
#endif /* EXCLUDE_DXP4C2X */
#ifndef EXCLUDE_DXPX10P
  XERXES_IMPORT int DXP_API dxp_init_dxpx10p();
#endif /* EXCLUDE_DXPX10P */
#ifndef EXCLUDE_UDXPS
  XERXES_IMPORT int DXP_API dxp_init_udxps();
#endif /* EXCLUDE_UDXPS */
#ifndef EXCLUDE_UDXP
  XERXES_IMPORT int DXP_API dxp_init_udxp();
#endif /* EXCLUDE_UDXP */

#ifndef EXCLUDE_XMAP
  XERXES_IMPORT int DXP_API dxp_init_xmap();
#endif /* EXCLUDE_XMAP */

#ifndef EXCLUDE_VEGA
  XERXES_IMPORT int DXP_API dxp_init_vega();
#endif /* EXCLUDE_VEGA */

#ifndef EXCLUDE_MERCURY
  XERXES_IMPORT int DXP_API dxp_init_mercury();
#endif /* EXCLUDE_MERCURY */

  XERXES_IMPORT int MD_API dxp_md_init_util();
  XERXES_IMPORT int MD_API dxp_md_init_io();

  /* Routines contained in xia_common.c.  Routines that are used across libraries but not exported */

  /* Deprecated functions */
  XERXES_EXPORT int XERXES_API dxp_replace_fipconfig();
  XERXES_EXPORT int XERXES_API dxp_reset_fipconfig();
#endif                                  /*   end if _XERXES_PROTO_ */

  /* If this is compiled by a C++ compiler, make it clear that these are C routines */
#ifdef __cplusplus
}
#endif

/*
 * Define the utility routines used throughout this library
 */

/* Added new logging routines 8/22/01 -- PJF */

DXP_MD_ERROR_CONTROL xerxes_md_error_control;
DXP_MD_ERROR xerxes_md_error;
DXP_MD_WARNING xerxes_md_warning;
DXP_MD_INFO xerxes_md_info;
DXP_MD_DEBUG xerxes_md_debug;
DXP_MD_OUTPUT xerxes_md_output;
DXP_MD_SUPPRESS_LOG xerxes_md_suppress_log;
DXP_MD_ENABLE_LOG xerxes_md_enable_log;
DXP_MD_SET_LOG_LEVEL xerxes_md_set_log_level;
DXP_MD_LOG xerxes_md_log;
DXP_MD_ALLOC xerxes_md_alloc;
DXP_MD_FREE xerxes_md_free;
DXP_MD_PUTS xerxes_md_puts;
DXP_MD_WAIT xerxes_md_wait;
DXP_MD_SET_PRIORITY xerxes_md_set_priority;
DXP_MD_FGETS xerxes_md_fgets;
DXP_MD_TMP_PATH xerxes_md_tmp_path;
DXP_MD_CLEAR_TMP xerxes_md_clear_tmp;
DXP_MD_PATH_SEP  xerxes_md_path_separator;

XERXES_EXPORT Utils *utils;


/* Logging macro wrappers */
#define dxp_log_error(x, y, z) xerxes_md_log(MD_ERROR, (x), (y), (z), __FILE__, __LINE__)
#define dxp_log_warning(x, y)  xerxes_md_log(MD_WARNING, (x), (y), 0, __FILE__, __LINE__)
#define dxp_log_info(x, y)   xerxes_md_log(MD_INFO, (x), (y), 0, __FILE__, __LINE__)
#define dxp_log_debug(x, y)  xerxes_md_log(MD_DEBUG, (x), (y), 0, __FILE__, __LINE__)


/* Memory allocation macro wrappers */
#ifdef USE_XIA_MEM_MANAGER
#include "xia_mem.h"
#define xerxes_md_alloc(n)  xia_mem_malloc((n), __FILE__, __LINE__)
#define xerxes_md_free(ptr) xia_mem_free(ptr)
#endif /* USE_XIA_MEM_MANAGER */

/* Constants */
#define MAX_MEM_TYPE_LEN  80

/* This macro helps reduce the line length for function calls to the DD layer. */
#define DD_FUNC(x)   (x)->btype->funcs


#endif      /* Endif for XIA_XERXES_H */
