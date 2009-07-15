/*************
 *  usblib.c  
 *
 *  JEW: Shamelessly adapted from work by Don Wharton
 * 
 * Copyright (c) 2002,2003,2004, X-ray Instrumentation Associates
 *               2005, XIA LLC
 * All rights reserved.
 **************/

/* These #pragmas remove warnings generated
 * by bugs in the Microsoft headers.
 */

#ifdef WIN32
  #pragma warning( disable : 4115 )
  #pragma warning( disable : 4201 )
  #include <conio.h>
#endif
#include <windows.h>

#include <winioctl.h>

#include <stdio.h>
#include <stdlib.h>

#include "Dlldefs.h"
#include "usblib.h"

static unsigned char inBuffer[262144];
static unsigned char outBuffer[262144];

XIA_EXPORT int XIA_API usb_open(char *device, HANDLE *hDevice)
{
  /* Get handle to USB device */
  *hDevice = CreateFile(device,
					   GENERIC_WRITE,
					   FILE_SHARE_WRITE,
					   NULL,
					   OPEN_EXISTING,
					   0,
					   NULL);
  
  if(hDevice == INVALID_HANDLE_VALUE)
	{
	  return 1;
	}

  return 0;
}

XIA_EXPORT int XIA_API usb_close(HANDLE hDevice)
{
  /* Close the Handle */
  CloseHandle(hDevice);  

  return 0;
}

XIA_EXPORT int XIA_API usb_read(long address, long nWords, char *device, unsigned short *buffer)
{	
  /*
  * Declare variables
  */

  unsigned char* pData = (unsigned char*)buffer;
  long byte_count; 
  UCHAR ctrlBuffer[CTRL_SIZE];
  UCHAR lo_address, hi_address, lo_count, hi_count;
  int i = 0;
  unsigned long nBytes = 0;
  BOOL bResult = FALSE;
  HANDLE hDevice = NULL;
  long inPacketSize, outPacketSize;
  BULK_TRANSFER_CONTROL bulkControl;

  int ret;

  byte_count = (nWords * 2);
  
  hi_address = (unsigned char)(address >> 8);
  lo_address = (unsigned char)(address & 0x00ff);
  hi_count = (unsigned char)(byte_count >> 8);
  lo_count = (unsigned char)(byte_count & 0x00ff);
  
  ctrlBuffer[0] = lo_address;
  ctrlBuffer[1] = hi_address;
  ctrlBuffer[2] = lo_count;
  ctrlBuffer[3] = hi_count;
  ctrlBuffer[4] = (unsigned char)0x01;
  
  /******************************************************/
	
  /* Get handle to USB device */
  ret = usb_open(device, &hDevice);

  if (ret != 0) 
	{
	  return 1;
	}
  /******************************************************/
  
  /* Write Address and Byte Count */	
  bulkControl.pipeNum = OUT1;
  outPacketSize = CTRL_SIZE;
  
  bResult = DeviceIoControl(hDevice,
							IOCTL_EZUSB_BULK_WRITE,
							&bulkControl,
							sizeof(BULK_TRANSFER_CONTROL),
							&ctrlBuffer[0],
							outPacketSize,
							&nBytes,
							NULL);
  
  if(bResult != TRUE)
	{
	  usb_close(hDevice);
	  return 14;
	}
  
  /******************************************************/
		
  /* Read Data */
  bulkControl.pipeNum = IN2;
  inPacketSize = byte_count;
  
  bResult = DeviceIoControl(hDevice,
							IOCTL_EZUSB_BULK_READ,
							&bulkControl,
							sizeof(BULK_TRANSFER_CONTROL),
							&inBuffer[0],
							inPacketSize,
							&nBytes,
							NULL);
  
  if(bResult != TRUE)
	{
	  usb_close(hDevice);
	  return 2;
	}
  
  for(i=0;i<byte_count;i++)
	{
	  *pData++ = inBuffer[i];
	}
  
  /*****************************************************/

  /* Close Handle to USB device */
  usb_close(hDevice);
  
  return 0;
}

XIA_EXPORT int XIA_API usb_write(long address, long nWords, char *device, unsigned short *buffer)
{
  /*****************************************************/
  /* Declare variables */
  
  unsigned char* pData = (unsigned char*)buffer;
  long byte_count; 
  UCHAR ctrlBuffer[CTRL_SIZE];
  UCHAR hi_address, lo_address, hi_count, lo_count;
  int i = 0;
  unsigned long nBytes = 0;
  BOOL bResult = FALSE;
  HANDLE hDevice = NULL;
  long outPacketSize;
  BULK_TRANSFER_CONTROL bulkControl;

  int ret;

  byte_count = (nWords * 2);
  
  hi_address = (unsigned char)(address >> 8);
  lo_address = (unsigned char)(address & 0x00ff);
  hi_count = (unsigned char)(byte_count >> 8);
  lo_count = (unsigned char)(byte_count & 0x00ff);
  
  ctrlBuffer[0] = lo_address;
  ctrlBuffer[1] = hi_address;
  ctrlBuffer[2] = lo_count;
  ctrlBuffer[3] = hi_count;
  ctrlBuffer[4] = (unsigned char)0x00;
  
  /******************************************************/

  /* Get handle to USB device */
  ret = usb_open(device, &hDevice);
  
  if (ret != 0)
	{
	  return 1;
	}
  /******************************************************/

  /* Write Address and Byte Count	*/
  bulkControl.pipeNum = OUT1;
  outPacketSize = CTRL_SIZE;
  
  bResult = DeviceIoControl(hDevice,
							IOCTL_EZUSB_BULK_WRITE,
							&bulkControl,
							sizeof(BULK_TRANSFER_CONTROL),
							&ctrlBuffer[0],
							outPacketSize,
							&nBytes,
							NULL);
  
  if(bResult != TRUE)
	{
	  usb_close(hDevice);
	  return 14;
	}
  /******************************************************/
	
  /* Write Data */
  for(i=0;i<byte_count;i++)
	{
	  outBuffer[i] = *pData++;
	  Sleep(0);
	}
  
  bulkControl.pipeNum = OUT2;
  outPacketSize = byte_count;
  
  bResult = DeviceIoControl(hDevice,
							IOCTL_EZUSB_BULK_WRITE,
							&bulkControl,
							sizeof(BULK_TRANSFER_CONTROL),
							&outBuffer[0],
							outPacketSize,
							&nBytes,
							NULL);
  
  if(bResult != TRUE)
	{
	  usb_close(hDevice);
	  return 15;
	}
  
  /******************************************************/
  
  /* Close Handle to USB device */
  usb_close(hDevice);
  
  return 0;
}

/*int usbscan()
{
  int x;
  HANDLE hDevice = NULL;
  
  char* pUSBAddress[] = {"\\\\.\\ezusb-0",
						 "\\\\.\\ezusb-1",
						 "\\\\.\\ezusb-2",
						 "\\\\.\\ezusb-3"};
  
  for(x=1;x<5;x++)
	{
	  hDevice = CreateFile(*(pUSBAddress+(x-1)),
						   GENERIC_WRITE,
						   FILE_SHARE_WRITE,
						   NULL,
						   OPEN_EXISTING,
						   0,
						   NULL);
	  
	  if(hDevice == INVALID_HANDLE_VALUE)
		{
		  modules[x] = 0;
		} else {
		  modules[x] = 1;
		  CloseHandle(hDevice);
		}
	}
  
  return 0;
}
*/
