/*----------------------------------------------------------
    CAMACDLL.C 3/22/97 using dynamic linking to wnaspi32
    The function camxfr() is exported to the users application
    It is called with 5 parameters (see declaration below).
    The 1st parameter is an array of 4 shorts that convey
    1. Host Adapter number (normally 0 if there is only one)
    2. CrateID
    3. Station
    4. Subaddress
  ----------------------------------------------------------

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


#pragma warning(disable : 4115)
#include <windows.h>

#include <winbase.h>
#include "winaspi.h"
#include "camacdll.h"
#ifdef FOR_IGORXOP
#include "XOPStandardHeaders.h"
#endif

#pragma warning(disable : 4127)

#define MAX_ADAPTS 20			// max nmbr host adapters expected
#define PROC_DEV_TYPE 3
#define UNKNOWN_DEV_TYPE 0x1F

/* Store function pointers to the ASPI library routines */
typedef DWORD (* MYDLLFUNC)(LPSRB);
typedef DWORD (* MYDLLFUNC1)(void);
MYDLLFUNC fpSendAspi32Cmd=0;
MYDLLFUNC1 fpGetSupInfo=0;

/* Variable to store the library pointer */
static struct HINSTANCE__* hLibrary=NULL;

BYTE Num_Adapt;				//number found
/*
elements of arrays in which will store info about each scsi bus found.
*/
BYTE n_targs[MAX_ADAPTS];
BYTE resid_sup[MAX_ADAPTS];
DWORD max_len[MAX_ADAPTS];
BYTE ha_scsi_id[MAX_ADAPTS];
WORD align_mask[MAX_ADAPTS];
BYTE cam_dev_type[MAX_ADAPTS][16];
BYTE tested [MAX_ADAPTS][16];
/*
2-dim arrays where store info on each target found. Device type[] will be
set to 1 if scsi device type 1F hex (73A). tested[] will be set to 1 after
a test unit ready has been issued. 
*/
SRB_ExecSCSICmd ExecCAMSRB;
SRB_ExecSCSICmd ExecTURSRB;
/* WinASPI structures */
SRB_GDEVBlock GetCamDevSRB;

/* A little boolean help to stop the compiler from warning 
 * about while(1) expressions.
 */
#define TRUE_  (1 == 1)

/*******************************************************************************
 * 
 * Routine to perform initialization of the SCSI layer, this will scan the
 * SCSI bus and return a list of codes for the devices located on the bus.
 *
 *******************************************************************************/
__declspec(dllexport) long xia_caminit(short* buf)
{
	DWORD SupportInfo,ReturnVal;
    SRB_HAInquiry ThisSRB;
	int status, numHA;
	unsigned int i, j, index;

	if(hLibrary == NULL){
		hLibrary = LoadLibrary("wnaspi32.dll");
		if(hLibrary == NULL) return 7;//-1;
		fpGetSupInfo = (MYDLLFUNC1) GetProcAddress(hLibrary,"GetASPI32SupportInfo");
		fpSendAspi32Cmd = (MYDLLFUNC) GetProcAddress(hLibrary,"SendASPI32Command");
	}

	if(fpGetSupInfo == NULL)	return 8;	//-1;
	if(fpSendAspi32Cmd == NULL) return 9;	//-1;

/* get the number of host adapters */
	SupportInfo = (*fpGetSupInfo)();
	status=(SupportInfo & 0xff00)>>8;
	numHA=SupportInfo & 0xff;
	if(status != SS_COMP)	return 10;//FALSE;					 
 
/* for each, save info and scan the bus */
	index=1;
	ThisSRB.SRB_Cmd = SC_HA_INQUIRY;
	ThisSRB.SRB_Flags = 0;
	ThisSRB.SRB_Hdr_Rsvd = 0;
	for (i=0; i< (unsigned int) min(MAX_ADAPTS,numHA); i++) { // loop over host adapters
		ThisSRB.SRB_HaId = (unsigned char)i;
		(*fpSendAspi32Cmd) ( (LPSRB) &ThisSRB );
		if (ThisSRB.SRB_Status != SS_COMP){	// if HA did not respond
			for (j=0; j<16; j++) cam_dev_type[i][j]=0;
			continue;
		}
             
		align_mask[i] = (WORD)((ThisSRB.HA_Unique[1]<< 8) | ThisSRB.HA_Unique[0]);
		resid_sup[i] = (unsigned char)(ThisSRB.HA_Unique[2] & 2);
		n_targs[i] = 8;
		if (ThisSRB.HA_Unique[3] != 0) n_targs[i] = ThisSRB.HA_Unique[3];
		n_targs[i] = (unsigned char)min(n_targs[i],16);		// send warning if n_targs > 16 
             
		max_len[i] = (DWORD)((ThisSRB.HA_Unique[7] <<8) | ThisSRB.HA_Unique[6]);  
		max_len[i] = (max_len[i] <<8) | (DWORD)ThisSRB.HA_Unique[5];
		max_len[i] = (max_len[i] <<8) | (DWORD)ThisSRB.HA_Unique[4];
		ha_scsi_id[i] = ThisSRB.HA_SCSI_ID;
//for each target in this bus, get type
		for (j=0; j<n_targs[i]; j++) {
			tested[i][j]=0;
			cam_dev_type[i][j]=0;
			if ( j == ha_scsi_id[i] ) cam_dev_type[i][j] = 0;  //except for host ID
			else {
				GetCamDevSRB.SRB_Cmd = SC_GET_DEV_TYPE;
				GetCamDevSRB.SRB_HaId = (unsigned char)i;
				GetCamDevSRB.SRB_Flags = 0;
				GetCamDevSRB.SRB_Hdr_Rsvd = 0L;
				GetCamDevSRB.SRB_Target = (unsigned char)j;
				GetCamDevSRB.SRB_Lun = 0;
				GetCamDevSRB.SRB_Rsvd1 = 0;
				ReturnVal =(*fpSendAspi32Cmd) ( (LPSRB) &GetCamDevSRB );
				if( (ReturnVal == SS_COMP) && ((GetCamDevSRB.SRB_DeviceType & 0x1f)== PROC_DEV_TYPE) )
				{	
					cam_dev_type[i][j] = 1;          //it is 73A
					buf[index++] = (unsigned short)i;   buf[index++] = (unsigned short)j;
				}
				
			/*if ( ReturnVal == SS_NO_DEVICE) cam_dev_type[i][j] = 0;
				else {
				if (ReturnVal == SS_COMP){
				if ((GetCamDevSRB.SRB_DeviceType & 0x1f)!= PROC_DEV_TYPE)
				cam_dev_type[i][j] = 0;
				else  cam_dev_type[i][j] = 1;          //it is 73A
				} 
					else return 0;
			    }*/
			}
		}
	}		// ends loop over host adapters
	buf[0] = (unsigned short)((index - 1) / 2);
	return(0);

}

/*******************************************************************************
 *
 * Routine to perform camac transfers via the WINASPI layer to the SCSI 
 * controller 
 *
 *******************************************************************************/
__declspec(dllexport) long xia_camxfr(short* camadr, short cam_func, long len,
								  short mode, short* buf)
{
    short retval;
    unsigned int i,t;
	
/* Execute a regular CAMAC command */
	
	if(fpSendAspi32Cmd == 0) return 9;		//-1
	
	if (cam_dev_type[camadr[HA_NMBR]][camadr[CRATE]] == 0 )
		return 11;//0x8080;                                      // not a 73A
	if (tested[camadr[HA_NMBR]][camadr[CRATE]] == 0) {
		ExecTURSRB.SRB_Cmd = SC_EXEC_SCSI_CMD;
		ExecTURSRB.SRB_HaId = (unsigned char)camadr[HA_NMBR];
		ExecTURSRB.SRB_Flags = 0;
		ExecTURSRB.SRB_Hdr_Rsvd = 0;
		ExecTURSRB.SRB_Target = (unsigned char)camadr[CRATE];
		ExecTURSRB.SRB_Lun = 0;
		ExecTURSRB.SRB_Rsvd1 = 0; 
		ExecTURSRB.SRB_BufLen = 0L;
		ExecTURSRB.SRB_BufPointer = (BYTE *) NULL;
		ExecTURSRB.SRB_SenseLen = SENSE_LEN;
		ExecTURSRB.SRB_CDBLen = 6;
		ExecTURSRB.SRB_PostProc = 0;
		ExecTURSRB.SRB_Rsvd2 = NULL;
		for (i=0; i<16; i++) ExecTURSRB.SRB_Rsvd3[i] = 0;
		
		ExecTURSRB.CDBByte[0] = 0;
		ExecTURSRB.CDBByte[1] = 0;
		ExecTURSRB.CDBByte[2] = 0;
		ExecTURSRB.CDBByte[3] = 0;
		ExecTURSRB.CDBByte[4] = 0;
		ExecTURSRB.CDBByte[5] = 0;
		(*fpSendAspi32Cmd) ((LPSRB) &ExecTURSRB );
		
		/******************* New **********************************/
		t = 0;
		do
		{
			if(ExecTURSRB.SRB_Status != 0)
				break;
			t++;
			if(t == 100)
				return 12;//-1;
			Sleep(2);
		}while(TRUE_);
		/**********************************************************/
		
		tested[camadr[HA_NMBR]][camadr[CRATE]] = 1;
	}
	if (!(cam_func & 8)) {
		ExecCAMSRB.SRB_Cmd = SC_EXEC_SCSI_CMD;
		ExecCAMSRB.SRB_HaId = (unsigned char)camadr[HA_NMBR];
		ExecCAMSRB.SRB_Flags = (unsigned char)((cam_func & 16) ? SRB_DIR_OUT : SRB_DIR_IN);
		ExecCAMSRB.SRB_Hdr_Rsvd = 0;
		ExecCAMSRB.SRB_Target = (BYTE)camadr[CRATE];
		ExecCAMSRB.SRB_Lun = 0;
		ExecCAMSRB.SRB_Rsvd1 = 0; 
		ExecCAMSRB.SRB_BufLen = len;
		ExecCAMSRB.SRB_BufPointer = (BYTE *) buf;
		ExecCAMSRB.SRB_SenseLen = SENSE_LEN;
		ExecCAMSRB.SRB_CDBLen = 10;
		ExecCAMSRB.SRB_PostProc = 0;
		ExecCAMSRB.SRB_Rsvd2 = NULL;
		for (i=0; i<16; i++) ExecCAMSRB.SRB_Rsvd3[i] = 0;
		
		ExecCAMSRB.CDBByte[0] = 0x21;
		ExecCAMSRB.CDBByte[1] = 0;
		ExecCAMSRB.CDBByte[2] = (BYTE)cam_func;
		ExecCAMSRB.CDBByte[3] = (unsigned char)((mode << 5) | (camadr[STA] & 0x1F));
		ExecCAMSRB.CDBByte[4] = (BYTE)camadr[SUBADR];
		ExecCAMSRB.CDBByte[5] = 0;
		ExecCAMSRB.CDBByte[8] = (BYTE)len;
		len >>= 8;
		ExecCAMSRB.CDBByte[7] = (BYTE)len;
		len >>= 8;
		ExecCAMSRB.CDBByte[6] = (BYTE)len;
		ExecCAMSRB.CDBByte[9] = 0;
		(*fpSendAspi32Cmd) ((LPSRB) &ExecCAMSRB );

		/******************* New **********************************/
		t = 0;
		do
		{
			if((retval = ExecCAMSRB.SRB_Status) != 0)
				break;
			t++;
			if(t == 100)
				return 12;
			Sleep(2);
		}while(TRUE_);
		
		if(retval != SS_COMP)
			return 13;
		return 0;
		/**********************************************************/
	}
	else {                              
		ExecCAMSRB.SRB_Cmd = SC_EXEC_SCSI_CMD;
		ExecCAMSRB.SRB_HaId = (unsigned char)camadr[HA_NMBR];
		ExecCAMSRB.SRB_Flags =0;
		ExecCAMSRB.SRB_Hdr_Rsvd = 0;
		ExecCAMSRB.SRB_Target = (BYTE)camadr[CRATE];
		ExecCAMSRB.SRB_Lun = 0;
		ExecCAMSRB.SRB_Rsvd1 = 0; 
		ExecCAMSRB.SRB_BufLen = 0;
		ExecCAMSRB.SRB_BufPointer = 0;
		ExecCAMSRB.SRB_SenseLen = SENSE_LEN;
		ExecCAMSRB.SRB_CDBLen = 6; 
		ExecCAMSRB.SRB_PostProc = 0;
		ExecCAMSRB.SRB_Rsvd2 = NULL;
		for (i=0; i<16; i++) ExecCAMSRB.SRB_Rsvd3[i] = 0;
		
		ExecCAMSRB.CDBByte[0] = 1;
		ExecCAMSRB.CDBByte[1] = (BYTE)cam_func;
		ExecCAMSRB.CDBByte[2] = (BYTE)camadr[STA];
		ExecCAMSRB.CDBByte[3] = (BYTE)camadr[SUBADR];
		ExecCAMSRB.CDBByte[4] = 0;
		ExecCAMSRB.CDBByte[5] = 0;
		(*fpSendAspi32Cmd) ( (LPSRB) &ExecCAMSRB );

		/******************* New **********************************/
		t = 0;
		do
		{
			if((retval = ExecCAMSRB.SRB_Status) != 0)
				break;
			t++;
			if(t == 100)
				return 12;//-1;
			Sleep(2);
		}while(TRUE_);
		
		if(retval != SS_COMP)
			return 13;
		return 0;
		/**********************************************************/
	}
}

