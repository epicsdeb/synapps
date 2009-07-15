/******************************************************************************
 *
 *   Copyright 2003 X-ray Instrumentation Associates 
 *   All rights reserved 
 *
 *   arcnetlib.c
 *   Created 25-Apr-2003   J.Wahl
 *
 *   C-callable routine for communicating with the ARCNET protocol under Windows 9x
 *
 *****************************************************************************/
#include <math.h>
#include <stdlib.h>

#pragma warning(disable : 4115)
#include <windows.h>

#pragma warning(disable : 4201)
#include <winioctl.h>

#include "20020sys.h"

#include "Dlldefs.h"
#include "arcnetlib.h"

COM20020_TRANSMIT_BUFFER ctb;

HANDLE hDevice;
HANDLE RxEvent, TxEvent;

XIA_STATIC int sendPackage();
XIA_STATIC int receivePackage(unsigned short *data);

/*****************************************************************************
 * 
 *   Initialize the Arcnet device port Address.  This function must be called
 *   before any I/O is attempted.
 *
 *   Input: nodeID to initialize
 *
 *****************************************************************************/
XIA_EXPORT int XIA_API dxpInitializeArcnet(unsigned char nodeID) {
  int rstat = 0;

  BYTE byReturn;
  DWORD ReceivedByteCount;
  
  COM20020_CONFIG cfg;
  
  cfg.byCom20020Timeout = STANDARD_TIMEOUT;
  cfg.byCom20020NodeID = nodeID;
  cfg.bCom20020_128NAKs = TRUE;
  cfg.bCom20020ReceiveAll = FALSE;
  cfg.byCom20020ClockPrescaler = 0;
  cfg.bCom20020SlowArbitration = FALSE;
  cfg.bCom20020ReceiveBroadcasts = TRUE;
  
  hDevice = CreateFile("\\\\.\\CCSI20020Dev1",GENERIC_READ | GENERIC_WRITE, \
					   FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
  
  if(hDevice == INVALID_HANDLE_VALUE)
	{
	  return 1;
	}
  
  rstat = DeviceIoControl(hDevice, (unsigned long) DIOC_COM20020INIT, &cfg, sizeof(COM20020_CONFIG), \
						  &byReturn, 1, &ReceivedByteCount, NULL);
  if (rstat != 0)
	{
	  return 2;
	}
  
  RxEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  rstat = DeviceIoControl(hDevice, (unsigned long) DIOC_WAKE_ON_RECEIVE, &RxEvent, sizeof(PHANDLE), \
						  NULL, 0, &ReceivedByteCount, NULL);
  
  if (rstat != 0)
	{
	  return 3;
	}
  
  TxEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  rstat = DeviceIoControl(hDevice, (unsigned long) DIOC_WAKE_ON_TX_COMPLETE, &TxEvent, sizeof(PHANDLE), \
						  NULL, 0, &ReceivedByteCount, NULL);
  
  if (rstat != 0)
	{
	  return 4;
	}

  CloseHandle(hDevice);
  
  return 0;
}


/*****************************************************************************
 *
 * Set the address for the next transfer
 *
 *****************************************************************************/
XIA_EXPORT int XIA_API dxpSetAddressArcnet(unsigned char nodeID, unsigned short addr) 
{
  int rstat = 0;
  int loc = 248;

  /* send address package */
  ctb.byDestinationNodeID = nodeID;
  /* number of ARCNET data bytes (fixed) */
  ctb.uiNumberOfBytes = loc + 4;
  /* number of data bytes for G200 */
  ctb.byDataBuffer[loc] = 0;							
  /* address low byte */
  ctb.byDataBuffer[loc+1] = (unsigned char)(addr & 0x00FF);		
  /* address high byte */
  ctb.byDataBuffer[loc+2] = (unsigned char)((addr & 0xFF00) >> 8);	
  /* command byte: bit 2 = 0 (address), bit 1 = 0 (16bit word), bit 0 = 0 (write) */
  ctb.byDataBuffer[loc+3] = 0;		

  rstat = sendPackage();
  if (rstat != 0) return 1;

  /* Wait for 150ms */
  Sleep(150);

  return rstat;
}


/* write procedure ****************************
*
*	Igor sends data, target address and number of bytes to write
*	
*	xop sends address only in first "address" package
*	xop sends data in subsequent "data" packages; no need to specify address
*	because of the auto-increment in the DSP
*
*********************************************** */

XIA_EXPORT int XIA_API dxpWriteArcnet(unsigned char nodeID, unsigned short addr, unsigned short *data, unsigned int len)
{
  int i;
  int x;
  int bytecount = 0;
  int cmd = 0;
  int rstat = 0;
  int loc;
  
  /* location of first G200 I/O control word in data buffer */
  loc = 248;	
  
  if(len > 65536)
	{
	  return 1;
	}
  bytecount = len * 2;
  
  /* set the address */
  rstat = dxpSetAddressArcnet(nodeID, addr);
  if (rstat != 0) 
	{
	  return 2;
	}

  /* send data packages */
  /* number of ARCNET data bytes (fixed) */
  ctb.byDestinationNodeID = nodeID;
  /* address low byte first */
  ctb.byDataBuffer[249] = (unsigned char) (addr & 0x00FF);	
  /* address high byte last */
  ctb.byDataBuffer[250] = (unsigned char) ((addr & 0xFF00) >> 8);

  cmd = 4;
  if (addr < 0x4000) cmd |= 0x0002;	
  /* command byte: bit 2 = 1 (data), bit 1 = depends on address (16bit word), bit 0 = 0 (write) */
  ctb.byDataBuffer[loc+3] = (unsigned char) cmd;	
  
  x=0;
  while(bytecount > 0)
	{
	  /* Still more than 248 bytes left */
	  if ((bytecount) > 248)
		{
		  for(i=0; i<248; i=i+2,x++)
			{
			  /* low byte */
			  ctb.byDataBuffer[i] = (unsigned char) (data[x] & 0x00FF);
			  /* high byte */
			  ctb.byDataBuffer[i+1] = (unsigned char) (data[x] >> 8);
			}
		  /* number of bytes to send to G200 */
		  ctb.byDataBuffer[loc] = 248;					
		  /* number of ARCNET data bytes (data bytes plus 4 control bytes) */
		  ctb.uiNumberOfBytes = 248 + 4;					

		  bytecount -= 248;

		  rstat = sendPackage();
		  if(rstat != 0) return 3;

		  /* Wait for 200ms */
		  Sleep(200);
		} else {
		  for(i=0; i<bytecount; i=i+2,x++)
			{
			  /* low byte */
			  ctb.byDataBuffer[i] = (unsigned char) (data[x] & 0x00FF);
			  /* high byte */
			  ctb.byDataBuffer[i+1] = (unsigned char) (data[x] >> 8);
			}
		  
		  loc=bytecount;
		  /* number of bytes to send to G200 */
		  ctb.byDataBuffer[loc] = (unsigned char) bytecount;
		  /* address low byte */
		  ctb.byDataBuffer[loc+1] = (unsigned char)(addr & 0x00FF);
		  /* address high byte */
		  ctb.byDataBuffer[loc+2] = (unsigned char)((addr & 0xFF00) >> 8);
		  /* command byte: bit 2 = 1 (data), bit 1 = 0 (16bit word), bit 0 = 0 (write) */
		  ctb.byDataBuffer[loc+3] = (unsigned char) cmd;
		  
		  /* number of ARCNET data bytes (data bytes plus 4 control bytes) */
		  ctb.uiNumberOfBytes = bytecount + 4;

		  bytecount =0;

		  rstat = sendPackage();
		  if (rstat != 0) return 3;

		  Sleep(200);
		}
	}

	return 0;
}

/************************************************************************
 *
 *  This routine sends "read request" package to CU, specifying address
 *	to read from and number of bytes. 
 *   (read request package contains only the 4 control bytes)
 *	Then it waits for the return package with the data. 
 *  If number of bytes requested <= 249 can be one packet, else have to
 *	request and wait for multiple packets. No need to specify address
 *	in subsequent requests when setting the autoincrement bit. 
 *
 ************************************************************************/  
XIA_EXPORT int XIA_API dxpReadArcnet(unsigned char nodeID, unsigned short addr, unsigned short *data, unsigned int len)
{
	int bytecount = 0;
	int rstat;
	int loc;
	
	loc=0;
	if(len > 65536)
	  {
		return 1;
	  }
	bytecount = len * 2;

	/* set up data buffer (common elements */
	ctb.byDestinationNodeID = nodeID;
	/* number of ARCNET data bytes (fixed) */
	ctb.uiNumberOfBytes = loc+4;							
	/* address low byte */
	ctb.byDataBuffer[loc+1] = (unsigned char) (addr & 0x00FF);		
	/* address high byte */
	ctb.byDataBuffer[loc+2] = (unsigned char) ((addr & 0xFF00) >> 8);	
	/* command byte: bit 2 = 1 (auto incr.), bit 1 = 0 (16bit word), bit 0 = 1 (read) */
	ctb.byDataBuffer[loc+3] = 5;		
	
	while(bytecount > 0)
	  {
		if (bytecount > 248)
		  {				
			/* number of data bytes requested from G200 */
			ctb.byDataBuffer[loc] = 248;	
			/* send read request */
			rstat = sendPackage();			
			if (rstat != 0) return 2;				
			
			/* wait to get data */
			receivePackage(data);
			bytecount -= 248;
		  }	else {
			/* number of data bytes requested from G200 */
			ctb.byDataBuffer[loc] = (unsigned char) bytecount;
			/* send read request */
			rstat = sendPackage();			
			if (rstat != 0) return 2;
			
			/* wait to get data */
			receivePackage(data);
			bytecount = 0;
		}
	}

	return 0;
}

/************************************************************************
 *
 *  This routine sends the Arcnet packet
 *
 ************************************************************************/  
XIA_STATIC int sendPackage(void)
{
  BYTE byReturn;
  
  BOOL bInProgress;
  
  int rstat = 0;

  COM20020_STATUS cs;
  
  DWORD receivedByteCount;
  DWORD timeout;

  cs.bTransmissionAcknowledged = FALSE;
  while(!cs.bTransmissionAcknowledged)
	{
	  while (rstat == 0)
		{
		  rstat = DeviceIoControl(hDevice, (unsigned long) DIOC_COM20020TRANSMIT, (LPVOID) &ctb,
									sizeof(COM20020_TRANSMIT_BUFFER), &byReturn, 1, &receivedByteCount, NULL);
		}
	  
	  bInProgress = TRUE;
	  timeout = WaitForSingleObject(TxEvent, 10000);
	  if(timeout == WAIT_TIMEOUT)
		{
		  return 1;
		}
	  ResetEvent(TxEvent);
	  
	  while(bInProgress)
		{
		  rstat = DeviceIoControl(hDevice, (unsigned long) DIOC_COM20020STATUS, NULL, 0, &cs,
								  sizeof(COM20020_STATUS), &receivedByteCount, NULL);
		  if(cs.bTransmissionComplete && cs.bTransmissionAcknowledged)
			{
			  bInProgress = FALSE;
			  return 2;
			  }
		  if(cs.bTransmissionComplete && !cs.bTransmissionAcknowledged)
			{
			  bInProgress = FALSE;
			  return 3;
			}
		  if(cs.bExcessiveNAKs)
			{
			  bInProgress = FALSE;
			  return 4;
			}
		}
	}
  return 0;
}

/************************************************************************
 *
 *  This routine receives the Arcnet packet
 *
 ************************************************************************/  
XIA_STATIC int receivePackage(unsigned short *data)
{
  COM20020_RECEIVE_BUFFER crb;

  DWORD receivedByteCount;
  
  int i;
  int x;
  
  WaitForSingleObject(RxEvent, 10000);

  DeviceIoControl(hDevice, (unsigned long) DIOC_COM20020RECEIVE, NULL, 0, &crb,
				  sizeof(COM20020_RECEIVE_BUFFER), &receivedByteCount, NULL);
  if(crb.byNumberOfFilledBuffers == 0)
	{
	  ResetEvent(RxEvent);
	  return 1;
	}

  x=0;
  while(crb.byNumberOfFilledBuffers){
	for(i=0; i < abs(crb.uiNumberOfBytes); i += 2, x++)
	  {
		data[x] = crb.byDataBuffer[i + 1];
		data[x] = (unsigned short) (data[x] << 8);
		data[x] = (unsigned short) (data[x] | crb.byDataBuffer[i]);
	  }
	
	DeviceIoControl(hDevice, (unsigned long) DIOC_COM20020RECEIVE, NULL, 0, &crb,
					sizeof(COM20020_RECEIVE_BUFFER), &receivedByteCount, NULL);	
  }
  
  ResetEvent(RxEvent);

  return 0;
}
