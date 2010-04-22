/*
 *  md_win95.h
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
 * $Id: md_win95.h,v 1.6 2009-07-16 17:52:51 rivers Exp $
 *
 */


#ifndef DXP_MD_H
#define DXP_MD_H


#include "xia_mddef.h"
#include "xia_common.h"


/* Remove this when the uDXP is ready to go live */
/*#define EXCLUDE_UDXP    0xDEAD*/



/* Some constants */
#define MAXMOD 100

#define MD_IO_READ  0
#define MD_IO_WRITE 1
#define MD_IO_OPEN  2
#define MD_IO_CLOSE 3


/* If this is compiled by a C++ compiler, make it clear that these are C routines */
#ifdef __cplusplus
extern "C" {
#endif

#ifdef _DXP_PROTO_						/* ANSI C prototypes */

  /* XXX Need to clean up the naming scheme here. These
   * are actually Camac I/O commands.
   */
#ifndef EXCLUDE_CAMAC
XIA_MD_STATIC int XIA_MD_API dxp_md_initialize(unsigned int *, char *);
XIA_MD_STATIC int XIA_MD_API dxp_md_open(char *, int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_open_bcn(int *, int *, int *, int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_io(int *camChan, unsigned int *function,
									   unsigned long *addr, void *data,
									   unsigned int *length);
XIA_MD_STATIC int XIA_MD_API dxp_md_close(int *camChan);
#endif /* EXCLUDE_CAMAC */

#ifndef EXCLUDE_EPP
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_initialize(unsigned int *, char *);
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_open(char *, int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_io(int *camChan, unsigned int *function,
										   unsigned long *addr, void *data,
										   unsigned int *length);
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_close(int *camChan);
#endif /* EXCLUDE_EPP */

#ifndef EXCLUDE_USB
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_initialize(unsigned int *maxMod, char *dllName);
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_open(char *ioname, int *camChan);
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_io(int *camChan, unsigned int *function, 
										   unsigned long *address, void *data,
										   unsigned int *length);
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_close(int *camChan);
#endif /* EXCLUDE_USB */


#ifndef EXCLUDE_USB2
XIA_MD_STATIC int  dxp_md_usb2_initialize(unsigned int *maxMod,
                                         char *dllName);
XIA_MD_STATIC int  dxp_md_usb2_open(char *ioname, int *camChan);
XIA_MD_STATIC int  dxp_md_usb2_io(int *camChan, unsigned int *function, 
										   unsigned long *address, void *data,
										   unsigned int *length);
XIA_MD_STATIC int  dxp_md_usb2_close(int *camChan);
#endif /* EXCLUDE_USB2 */


#ifndef EXCLUDE_SERIAL
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_initialize(unsigned int *maxMod, char *dllName);
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_open(char *ioname, int *camChan);
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_io(int *camChan,
											  unsigned int *function, 
											  unsigned long *address, void *data,
											  unsigned int *length);
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_close(int *camChan);
#endif /* EXCLUDE_SERIAL */

#ifndef EXCLUDE_PLX
  XIA_MD_STATIC int dxp_md_plx_initialize(unsigned int *maxMod, char *dllName);
  XIA_MD_STATIC int dxp_md_plx_open(char *ioname, int *camChan);
  XIA_MD_STATIC int dxp_md_plx_io(int *camChan, unsigned int *function,
								  unsigned long *address, void *data,
								  unsigned int *length);
  XIA_MD_STATIC int dxp_md_plx_close(int *camChan);

#endif /* EXCLUDE_PLX */

/* XIA_MD_STATIC int XIA_MD_API dxp_md_lock_resource(int *ioChan, int *modChan, short *lock); */
XIA_MD_STATIC int XIA_MD_API dxp_md_wait(float *);
XIA_MD_STATIC void XIA_MD_API dxp_md_error_control(char *,int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_get_maxblk(void);
XIA_MD_STATIC int XIA_MD_API dxp_md_set_maxblk(unsigned int *);
XIA_MD_STATIC int XIA_MD_API dxp_md_puts(char *);
  XIA_MD_STATIC int XIA_MD_API dxp_md_set_priority(int *priority);
  XIA_MD_STATIC char * dxp_md_fgets(char *s, int length, FILE *stream);
  XIA_MD_STATIC char * dxp_md_tmp_path(void);
  XIA_MD_STATIC void dxp_md_clear_tmp(void);
  XIA_MD_STATIC char * dxp_md_path_separator(void);


#ifndef EXCLUDE_CAMAC
XIA_MD_IMPORT int XIA_MD_API xia_camxfr(short *camadr, short func, long nbytes, short mode, short *buf);
XIA_MD_IMPORT int XIA_MD_API xia_caminit(short *buf);
#endif /* EXCLUDE_CAMAC */

#ifndef EXLCUDE_EPP
XIA_MD_IMPORT int XIA_MD_API DxpInitPortAddress(int );
XIA_MD_IMPORT int XIA_MD_API DxpInitEPP(int );
XIA_MD_IMPORT int XIA_MD_API DxpWriteWord(unsigned short,unsigned short);
XIA_MD_IMPORT int XIA_MD_API DxpWriteBlock(unsigned short,unsigned short *,int);
XIA_MD_IMPORT int XIA_MD_API DxpWriteBlocklong(unsigned short,unsigned long *, int);
XIA_MD_IMPORT int XIA_MD_API DxpReadWord(unsigned short,unsigned short *);
XIA_MD_IMPORT int XIA_MD_API DxpReadBlock(unsigned short, unsigned short *,int);
XIA_MD_IMPORT int XIA_MD_API DxpReadBlockd(unsigned short, double *,int);
XIA_MD_IMPORT int XIA_MD_API DxpReadBlocklong(unsigned short,unsigned long *,int);
XIA_MD_IMPORT int XIA_MD_API DxpReadBlocklongd(unsigned short, double *,int);
XIA_MD_IMPORT void XIA_MD_API DxpSetID(unsigned short id);
XIA_MD_IMPORT int XIA_MD_API DxpWritePort(unsigned short port, unsigned short data);
XIA_MD_IMPORT int XIA_MD_API DxpReadPort(unsigned short port, unsigned short *data);
#endif /* EXCLUDE_EPP */

#ifndef EXCLUDE_USB
XIA_MD_IMPORT int XIA_MD_API usb_open(char *device, HANDLE *hDevice);
XIA_MD_IMPORT int XIA_MD_API usb_close(HANDLE hDevice);
XIA_MD_IMPORT int XIA_MD_API usb_read(long address, long nWords, char *device, unsigned short *data);
XIA_MD_IMPORT int XIA_MD_API usb_write(long address, long nWords, char *device, unsigned short *data);
  
#endif /* EXCLUDE_USB */


#ifndef EXCLUDE_SERIAL

#include "seriallib.h"

XIA_MD_IMPORT int XIA_MD_API InitSerialPort(unsigned short port, unsigned long baud);
XIA_MD_IMPORT int XIA_MD_API CloseSerialPort(unsigned short port);
XIA_MD_IMPORT serial_read_error_t * XIA_MD_API ReadSerialPort(
											   unsigned short port,
											   unsigned long size,
											   unsigned char *data);
XIA_MD_IMPORT int XIA_MD_API WriteSerialPort(unsigned short port, unsigned long size, unsigned char *data);
XIA_MD_IMPORT int XIA_MD_API CheckAndClearTransmitBuffer(unsigned short port);
XIA_MD_IMPORT int XIA_MD_API CheckAndClearReceiveBuffer(unsigned short port);
XIA_MD_IMPORT int XIA_MD_API GetErrors(unsigned short port, unsigned short *errs);
XIA_MD_IMPORT int XIA_MD_API NumBytesAtSerialPort(unsigned short port, unsigned long *numBytes);
  XIA_MD_IMPORT int XIA_MD_API SetTimeoutInMS(unsigned short port, double ms);
#endif /* EXCLUDE_UDXP */


#else									/* old style prototypes */

#ifndef EXCLUDE_CAMAC
XIA_MD_STATIC int XIA_MD_API dxp_md_initialize();
XIA_MD_STATIC int XIA_MD_API dxp_md_open();
XIA_MD_STATIC int XIA_MD_API dxp_md_open_bcn();
XIA_MD_STATIC int XIA_MD_API dxp_md_io();
XIA_MD_STATIC int XIA_MD_API dxp_md_close();
#endif /* EXCLUDE_CAMAC */

#ifndef EXCLUDE_EPP
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_initialize();
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_open();
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_io();
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_close();
#endif /* EXCLUDE_EPP */

#ifndef EXCLUDE_USB
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_initialize();
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_open();
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_io();
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_close();
#endif /* EXCLUDE_USB */

#ifndef EXCLUDE_SERIAL
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_initialize();
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_open();
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_io();
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_close();
#endif /* EXCLUDE_SERIAL */

#ifndef EXCLUDE_PLX
  XIA_MD_STATIC int dxp_md_plx_initialize();
  XIA_MD_STATIC int dxp_md_plx_open();
  XIA_MD_STATIC int dxp_md_plx_io();
  XIA_MD_STATIC int dxp_md_plx_close();
#endif /* EXCLUDE_PLX */

#ifndef EXCLUDE_USB2
  XIA_MD_STATIC int dxp_md_usb2_initialize();
  XIA_MD_STATIC int dxp_md_usb2_open();
  XIA_MD_STATIC int dxp_md_usb2_io();
  XIA_MD_STATIC int dxp_md_usb2_close();
#endif /* EXCLUDE_USB2 */


XIA_MD_STATIC int XIA_MD_API dxp_md_lock_resource();
XIA_MD_STATIC int XIA_MD_API dxp_md_wait();
XIA_MD_STATIC void XIA_MD_API dxp_md_error_control();
XIA_MD_STATIC int XIA_MD_API dxp_md_get_maxblk();
XIA_MD_STATIC int XIA_MD_API dxp_md_set_maxblk();
XIA_MD_STATIC void XIA_MD_API *dxp_md_alloc();
XIA_MD_STATIC void XIA_MD_API dxp_md_free();
XIA_MD_STATIC int XIA_MD_API dxp_md_puts();
  XIA_MD_STATIC int XIA_MD_API dxp_md_set_priority();

#ifndef EXCLUDE_CAMAC
XIA_MD_IMPORT int XIA_MD_API xia_camxfr();
XIA_MD_IMPORT int XIA_MD_API xia_caminit();
#endif /* EXCLUDE_CAMAC */

#ifndef EXCLUDE_EPP
XIA_MD_IMPORT int XIA_MD_API DxpInitPortAddress();
XIA_MD_IMPORT int XIA_MD_API DxpInitEPP();
XIA_MD_IMPORT int XIA_MD_API DxpWriteWord();
XIA_MD_IMPORT int XIA_MD_API DxpWriteBlock();
XIA_MD_IMPORT int XIA_MD_API DxpWriteBlocklong();
XIA_MD_IMPORT int XIA_MD_API DxpReadWord();
XIA_MD_IMPORT int XIA_MD_API DxpReadBlock();
XIA_MD_IMPORT int XIA_MD_API DxpReadBlockd();
XIA_MD_IMPORT int XIA_MD_API DxpReadBlocklong();
XIA_MD_IMPORT int XIA_MD_API DxpReadBlocklongd();
XIA_MD_IMPORT void XIA_MD_API DxpSetID();
XIA_MD_IMPORT int XIA_MD_API DxpWritePort();
XIA_MD_IMPORT int XIA_MD_API DxpReadPort();
#endif /* EXCLUDE_EPP */

#ifndef EXCLUDE_USB
XIA_MD_IMPORT int XIA_MD_API usb_open();
XIA_MD_IMPORT int XIA_MD_API usb_close();
XIA_MD_IMPORT int XIA_MD_API usb_read();
XIA_MD_IMPORT int XIA_MD_API usb_write();
#endif /* EXCLUDE_USB */

#ifndef EXCLUDE_SERIAL
XIA_MD_IMPORT int XIA_MD_API InitSerialPort();
XIA_MD_IMPORT int XIA_MD_API CloseSerialPort();
XIA_MD_IMPORT int XIA_MD_API NumBytesAtSerialPort();
XIA_MD_IMPORT int XIA_MD_API ReadSerialPort();
XIA_MD_IMPORT int XIA_MD_API WriteSerialPort();
XIA_MD_IMPORT int XIA_MD_API GetErrors();
#endif /* EXCLUDE_SERIAL */

#endif
#ifdef __cplusplus
}
#endif


#endif					/* End if for DXP_MD_H */
