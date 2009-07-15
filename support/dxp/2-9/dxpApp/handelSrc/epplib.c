/******************************************************************************
 *
 *   Copyright 1998-2004, X-ray Instrumentation Associates 
 *             2005, XIA LLC
 *   All rights reserved 
 *
 *   epplib.cpp
 *   Created 15-Apr-1998   E.Oltman
 *
 *   Modified 25-May-1998  E.Oltman: Added write_block
 *   Modified 20-Jul-1998  E.Oltman: Turn into a dll
 *
 *   
 *   C-callable routine for communicating with the DXP-X10P under Windows 95
 *
 *   Developed in Microsoft Visual C++ Version 5.0.  The following functions
 *   must be replaced if you arw working with a different compiler...
 *   Note: This routine must be compiled w/default (or no) optimization in VC++
 *
 *      GetTickCount:  Returns elapsed time in msec
 *      Sleep          Suspends executions for time in msec
 *      _inp           reads a byte from a port
 *      _outp          writes a byte to a port
 *
 *    
 *
 *
 *
 *****************************************************************************/
#define CSR  0x8000

#include <math.h>
#include <stdlib.h>

#ifdef _WIN32
  #pragma warning(disable : 4115)
  #include <windows.h>
  #define DLPORTIO 1

  #ifdef DLPORTIO
    #include "dlportio.h"
  #else
    #include <conio.h>
  #endif /* DLPORTIO */
#endif /* _WIN32 */

#ifdef CYGWIN32 
  #include "windows.h"
  #define DLPORTIO 1

  #ifdef DLPORTIO
    #include "dlportio.h"
  #else
    #include <conio.h>
  #endif /* DLPORTIO */
#endif  /* CYGWIN32 */

#ifdef LINUX
  #include "xia_linux.h"
  #include <sys/io.h>
  #define _inp(PORT) inb(PORT)
  #define _outp(PORT, DATA) outb(DATA, PORT)
  #define SLOW
#endif /* LINUX */

#include "Dlldefs.h"
#include "epplib.h"

#ifdef DLPORTIO
static unsigned long PORT=0;
static unsigned long APORT,SPORT,DPORT;
#else
static unsigned short PORT=0;
static unsigned short APORT,SPORT,DPORT;
#endif /* DLPORTIO */

static int rstat=0;
static int status;

/* Keep track of the last ID set, this is used during init() calls to bypass the setting of Control=4
 * This is a problem since the control=4 call will reset the ID to 0 in our interfaces, then the init()
 * call can not proceed if the correct ID of the box is not 0. */
static int lastID = -1;

#ifdef DLPORTIO
#define _inp DlPortReadPortUchar
#define _outp DlPortWritePortUchar
#endif

/*****************************************************************************
 * 
 * This sets the PORT address only.  Used to allow bypassing of the InitEPP()
 * functionality
 *
 *****************************************************************************/
XIA_EXPORT int XIA_API DxpInitPortAddress(int port) {
  /**************
   *
   *   Initialize the parallel port Address.  This function must be called
   *   before any I/O is attempted.
   *
   *     input: port:  Usually 0x378, sometimes 0x278
   *
   **************/
#ifdef DLPORTIO
  /*unsigned char data;*/
  unsigned long EPORT,CPORT;
#else
  /*int data;*/
  unsigned short EPORT,CPORT;
#endif
  PORT = port;
  EPORT = PORT + 0x402;
  DPORT = PORT + 4;
  APORT = PORT + 3;
  CPORT = PORT + 2;
  SPORT = PORT + 1;

  return 0;
}


/*****************************************************************************/
XIA_EXPORT int XIA_API DxpInitEPP(int port) {
  /**************
   *
   *   Initialize the parallel port.  This function must be called
   *   before any I/O is attempted.
   *
   *     input: port:  Usually 0x378, sometimes 0x278
   *
   **************/
#ifdef DLPORTIO
  unsigned char data;
  unsigned long EPORT,CPORT;
#else
  int data;
  unsigned short EPORT,CPORT;
#endif
  PORT = port;
  EPORT = PORT + 0x402;
  DPORT = PORT + 4;
  APORT = PORT + 3;
  CPORT = PORT + 2;
  SPORT = PORT + 1;

  /*initial return status */
  rstat=0;
  data=_inp(EPORT);
  data = (char)((data&0x1F) + 0x80);
  _outp(EPORT,data);
  /* write 4 to control port */
  /* check the last ID, only execute if the ID is -1 */
  if (lastID == -1) 
	{
	  data=0x0;
	  _outp(CPORT,data);
	  data=0x4;
	  _outp(CPORT,data);
	  data=0x0;
	  _outp(CPORT,data);
	}
  /* write 1 to status port */
  data=1;
  _outp(SPORT,data);
  /* write 0 to status port */
  data=0;
  _outp(SPORT,data);
  /* check status (not time out, nByte=1) */
  status=_inp(SPORT);
  if((status&0x01)==1) rstat+=1;
  if(((status>>5)&0x01)==1) {
	/* interface is off by one byte, attempt to fix */
	_outp(APORT,data);
	status=_inp(SPORT);
	if(((status>>5)&0x01)==1) rstat+=2;
  }
  return rstat;
}


/*****************************************************************************/
XIA_EXPORT int XIA_API set_addr(unsigned short Input_Data) {
  /**************
   *
   *   Write the address for a paralell port transfer (read or write)
   *
   **************/
  /* status returns: 0:good, otherwise bits set to indicate errors
	 bit   0:TO first byte,
	 bit   2:TO second byte,
	 bit   1:nbyte error first byte
	 bit   3:nbyte error second byte */
#ifdef DLPORTIO
  UCHAR data;
#else
  int data;
#endif
  /*initial return status */
  rstat=0;
  /* output byte 0 */
  data = (UCHAR)(Input_Data & 0xff);
  _outp(APORT,data);
  /*check status (not time out, nByte=1) */
  status=_inp(SPORT);
  if((status&0x01)==1) rstat+=1;
  if(((status>>5)&0x01)!=1) rstat+=2;
  /* output byte 1 */
  data = (UCHAR)((Input_Data>>8) & 0xff);
  _outp(APORT,data);
  /* check status (not time out, nbyte=0) */
  status=_inp(SPORT);
  if((status&0x01)==1) rstat+=4;
  if(((status>>5)&0x01)==1) rstat+=8;
  return rstat;
}


/*****************************************************************************/
XIA_EXPORT int XIA_API DxpWriteWord(unsigned short addr,unsigned short data)   {
  /**************
   *
   *   Write a single data word to DATA memory (address>0x4000)
   *
   **************/
  /**************
   *  return code:
   *   0 ok
   *  -1 addr<0x4000
   *  -2 error setting address
   *   1,2,3: error writing first byte
   *   4,8,12: error writing second byte
   *   
   **************/
#ifdef DLPORTIO
  UCHAR cdata;
#else
  int cdata;
#endif
  rstat = 0;
  if(addr<0x4000) {

	return -1;
  }
  if((status=set_addr(addr))!=0){

	return -2;
  }
  cdata = (UCHAR)(data & 0x00FF);
  _outp(DPORT,cdata);
  /*check status (not time out, nByte=1) */
  status=_inp(SPORT);
  if((status&0x01)==1) rstat+=1;
  if(((status>>5)&0x01)!=1) rstat+=2;
  cdata = (UCHAR)((data >> 8) & 0x00FF);
  _outp(DPORT,cdata);
  /*check status (not time out, nByte=1) */
  status=_inp(SPORT);
  if((status&0x01)==1) rstat+=4;
  if(((status>>5)&0x01)==1) rstat+=8;

  return rstat;
}


/*****************************************************************************/
XIA_EXPORT int XIA_API DxpReadWord(unsigned short addr, unsigned short *data)   {
  /****************
   *    return code
   *   0   OK
   *  -1   address<0x4000
   *  -2   error writing address
   ****************/
  unsigned int cdata,tdata;
  if(addr<0x4000) {

	return -1;
  }
  if((status=set_addr(addr))!=0) {

	return -2;
  }
  rstat = 0;
  cdata=_inp(DPORT);
  /*check status (not time out, nByte=1) */
  status=_inp(SPORT);
  if((status&0x01)==1) rstat+=1;
  if(((status>>5)&0x01)!=1) rstat+=2;
  tdata=_inp(DPORT);
  /*check status (not time out, nByte=1) */
  status=_inp(SPORT);
  if((status&0x01)==1) rstat+=4;
  if(((status>>5)&0x01)==1) rstat+=8;
  *data = (unsigned short)(cdata | (tdata << 8));
  return rstat;
}


/*****************************************************************************/
XIA_EXPORT int XIA_API DxpWriteBlock(unsigned short addr,unsigned short *data,int len)   {
  /**************
   *  return code:
   *   0 ok
   *  -1 addr<0x4000
   *  -2 error setting address
   *   n error writing word n
   *   
   **************/
  rstat = 0;
  if(addr<0x4000) {

	return -1;
  }
  if((status=set_addr(addr))!=0){

	return -2;
  }
#ifdef SLOW
#ifdef DLPORTIO
  UCHAR cdata;
#else
  int cdata;
#endif
  int i;
  for (i=0;i<len;i++){
	cdata=data[i]&0x00FF;
	_outp(DPORT,cdata);
	/*check status (not time out, nByte=1) */
	status=_inp(SPORT);
	if((status&0x01)==1) rstat+=1;
	if(((status>>5)&0x01)!=1) rstat+=2;
	cdata=(data[i]>>8)&0x00FF;
	_outp(DPORT,cdata);
	/*check status (not time out, nByte=1) */
	status=_inp(SPORT);
	if((status&0x01)==1) rstat+=4;
	if(((status>>5)&0x01)==1) rstat+=8;
	if (rstat!=0){

	  return i+1;
	}
  }
#else
  DlPortWritePortBufferUshort(DPORT,data,len);
#endif
  return 0;
}


/*****************************************************************************/
XIA_EXPORT int XIA_API DxpWriteBlocklong(unsigned short addr,unsigned long *data,int len)   {
  /****************
   *    return code
   *   0   OK
   *  -1   address>=0x4000
   *  -2   error writing address
   *   n   error transferring nth longword
   ****************/
  int i;

#ifdef SLOW
#ifdef DLPORTIO
  UCHAR cdata;
#else
  int cdata;
#endif /* DLPORTIO */
#else
  PUSHORT pData = NULL;
#endif /* SLOW */

  if(addr>=0x4000){

	return -1;
  }
  if((status=set_addr(addr))!=0){

	return -2;
  }

#ifdef SLOW
  for(i=0;i<len;i++)   {
	cdata=(data[i]>>16)&0x000000FF;
	_outp(DPORT,cdata);
	/*check status (not time out, nByte=1) */
	status=_inp(SPORT);
	if((status&0x01)==1) rstat+=1;
	if(((status>>5)&0x01)!=1) rstat+=2;

	cdata=(data[i]>>24)&0x000000FF;
	_outp(DPORT,cdata);
	/*check status (not time out, nByte=1) */
	status=_inp(SPORT);
	if((status&0x01)==1) rstat+=4;
	if(((status>>5)&0x01)==1) rstat+=8;

	cdata=(data[i]    )&0x000000FF;
	_outp(DPORT,cdata);
	/*check status (not time out, nByte=1) */
	status=_inp(SPORT);
	if((status&0x01)==1) rstat+=16;
	if(((status>>5)&0x01)!=1) rstat+=32;

	cdata=(data[i]>> 8)&0x000000FF;
	_outp(DPORT,cdata);
	/*check status (not time out, nByte=1) */
	status=_inp(SPORT);
	if((status&0x01)==1) rstat+=64;
	if(((status>>5)&0x01)==1) rstat+=128;
	if (rstat!=0){

	  return i+1;
	}
  }
#else

  pData = (PUSHORT)malloc((2*len) * sizeof(USHORT));

  for (i=0;i<len;i++) {

	pData[2*i]=(USHORT)((data[i]>>16)&0x0000ffff);
	pData[2*i+1]=(USHORT)(data[i]&0x0000ffff);
  }

  DlPortWritePortBufferUshort(DPORT,pData,2*len);
	
  free((void *)pData);
  pData = NULL;

#endif
  return 0;
}


/*****************************************************************************/
XIA_EXPORT int XIA_API DxpReadBlock(unsigned short addr,unsigned short *data,int len)   {
  /****************
   *    return code
   *   0   OK
   *  -1   address<0x4000
   *  -2   error writing address
   *   n   error transferring nth longword
   ****************/
  if(addr<0x4000) {

	return -1;
  }
  if((status=set_addr(addr))!=0){

	return -2;
  }
#ifdef SLOW
  int i;
  unsigned int cdata,tdata;
  for(i=0;i<len;i++)   {
	cdata=_inp(DPORT);
	/*check status (not time out, nByte=1) */
	status=_inp(SPORT);
	if((status&0x01)==1) rstat+=1;
	if(((status>>5)&0x01)!=1) rstat+=2;
	tdata=_inp(DPORT);
	/*check status (not time out, nByte=1) */
	status=_inp(SPORT);
	if((status&0x01)==1) rstat+=4;
	if(((status>>5)&0x01)==1) rstat+=8;
	data[i]=cdata|(tdata<<8);
	if(rstat!=0){

	  return i+1;
	}
  }
#else
  DlPortReadPortBufferUshort(DPORT,data,len);
#endif
  return 0;
}


/*****************************************************************************/
XIA_EXPORT int XIA_API DxpReadBlocklong(unsigned short addr,unsigned long *data,int len){
  /****************
   *    return code
   *   0   OK
   *  -1   address>=0x4000
   *  -2   error writing address
   *   n   error transferring nth word
   ****************/
  if(addr>=0x4000){

	return -1;
  }
  if((status=set_addr(addr))!=0){

	return -2;
  }
#ifdef SLOW
  int i;
  unsigned char junk;
  unsigned long cdata0,cdata1,cdata2;
  for(i=0;i<len;i++){
	cdata0 = (unsigned char)_inp(DPORT);
	/*check status (not time out, nByte=1) */
	status=_inp(SPORT);
	if((status&0x01)==1) rstat+=1;
	if(((status>>5)&0x01)!=1) rstat+=2;
	cdata1 = (unsigned char)_inp(DPORT);
	/*check status (not time out, nByte=1) */
	status=_inp(SPORT);
	if((status&0x01)==1) rstat+=4;
	if(((status>>5)&0x01)==1) rstat+=8;
	cdata2 = (unsigned char)_inp(DPORT);
	/*check status (not time out, nByte=1) */
	status=_inp(SPORT);
	if((status&0x01)==1) rstat+=16;
	if(((status>>5)&0x01)!=1) rstat+=32;
	junk   = _inp(DPORT);
	/*check status (not time out, nByte=1) */
	status=_inp(SPORT);
	if((status&0x01)==1) rstat+=64;
	if(((status>>5)&0x01)==1) rstat+=128;
	data[i] =   (0x000000FF &  cdata0    )
	  |   (0x0000FF00 & (cdata1<< 8)) 
	  |   (0x00FF0000 & (cdata2<<16));
	if (rstat!=0){

	  return i+1;
	}
  }
#else
  DlPortReadPortBufferUlong(DPORT,data,len);
#endif
  return 0;
}


/*****************************************************************************/
XIA_EXPORT int XIA_API DxpReadBlockd(unsigned short addr,double *data,int len)   {
  /****************
   *    return code
   *   0   OK
   *  -1   address<0x4000
   *  -2   error writing address
   *   n   error transferring nth word
   ****************/
#ifdef SLOW
  unsigned int cdata,tdata;
#else
  PUSHORT pData = NULL;
#endif

  int i;
  if(addr<0x4000) return -1;
  if(set_addr(addr)!=0) return -2;

#ifdef SLOW
  for(i=0;i<len;i++)   {
	cdata=_inp(DPORT);
	/*check status (not time out, nByte=1) */
	status=_inp(SPORT);
	if((status&0x01)==1) rstat+=1;
	if(((status>>5)&0x01)!=1) rstat+=2;
	tdata=_inp(DPORT);
	/*check status (not time out, nByte=1) */
	status=_inp(SPORT);
	if((status&0x01)==1) rstat+=4;
	if(((status>>5)&0x01)==1) rstat+=8;
	data[i]=(double)(cdata|(tdata<<8));
	if(rstat!=0) return i+1;
  }
#else
  pData = (PUSHORT)malloc(len * sizeof(USHORT));

  DlPortReadPortBufferUshort(DPORT,pData,len);

  for (i=0;i<len;i++) data[i]=(double)pData[i];
	
  free((void *)pData);
#endif
  return 0;
}


/*****************************************************************************/
XIA_EXPORT int XIA_API DxpReadBlocklongd(unsigned short addr,double *data,int len){
  /****************
   *    return code
   *   0   OK
   *  -1   address>=0x4000
   *  -2   error writing address
   *   n   error transferring nth word
   ****************/
  int i;

#ifdef SLOW
  unsigned long cdata0,cdata1,cdata2;
  unsigned char junk;
#else
  PULONG pData = NULL; 
#endif

  if(addr>=0x4000) return -1;
  if(set_addr(addr)!=0) return -2;
#ifdef SLOW
  for(i=0;i<len;i++){
	cdata0 = (unsigned char)_inp(DPORT);
	/*check status (not time out, nByte=1) */
	status=_inp(SPORT);
	if((status&0x01)==1) rstat+=1;
	if(((status>>5)&0x01)!=1) rstat+=2;
	cdata1 = (unsigned char)_inp(DPORT);
	/*check status (not time out, nByte=1) */
	status=_inp(SPORT);
	if((status&0x01)==1) rstat+=4;
	if(((status>>5)&0x01)==1) rstat+=8;
	cdata2 = (unsigned char)_inp(DPORT);
	/*check status (not time out, nByte=1) */
	status=_inp(SPORT);
	if((status&0x01)==1) rstat+=16;
	if(((status>>5)&0x01)!=1) rstat+=32;
	junk   = _inp(DPORT);
	/*check status (not time out, nByte=1) */
	status=_inp(SPORT);
	if((status&0x01)==1) rstat+=64;
	if(((status>>5)&0x01)==1) rstat+=128;
	data[i] = (double)(
					   (0x000000FF &  cdata0     )
					   |   (0x0000FF00 & (cdata1<< 8)) 
					   |   (0x00FF0000 & (cdata2<<16)));
	if(rstat!=0) return i+1;
  }
#else

  pData = (PULONG)malloc(len * sizeof(ULONG));

  DlPortReadPortBufferUlong(DPORT,pData,len);

  for (i=0;i<len;i++) data[i]=(double)pData[i];

  free((void *)pData);

#endif
  return 0;
}


XIA_EXPORT void XIA_API DxpSetID(unsigned short id) 
{
  /*  int i;*/
  /*  double garbage;*/

  /* time in msec */
  /*  DWORD delay = 1;*/

#ifdef DLPORTIO
  unsigned char data;
  unsigned long CPORT;
#else
  int data;
  unsigned short CPORT;
#endif
	
  CPORT = PORT + 2;
	
  /* 1) Write ID to SPP Data */
  _outp(PORT, (unsigned char)(id & 0xFF));

  /*  for (i = 0; i < 500000; i++) {*/
	/* Delaying tactic */
  /*	garbage = log(100) * pow(2,2); 
  }
*/
  /* Pause for delay time to let the signal settle */
  /*  Sleep(delay); */

  /* 2) Toggle C2 twice */
  data = _inp(CPORT);
  data ^= 0x04;
  _outp(CPORT, data);
  data ^= 0x04;
  _outp(CPORT, data);

  /* 3) Write 0x00 to SPP Data */
  _outp(PORT, 0x00);

  /* Set the static variable so that control port toggling is skipped in Initialization */
  lastID = (int)id;
}


XIA_EXPORT int XIA_API DxpWritePort(unsigned short port, unsigned short data) 
{

#ifdef DLPORTIO
  unsigned char DATA = (unsigned char)data;
#else
  unsigned short DATA = data;
#endif

  _outp(port, DATA);
  return 0;
}


XIA_EXPORT int XIA_API DxpReadPort(unsigned short port, unsigned short *data) {
	
  (*data) = _inp(port);
  return 0;
}
