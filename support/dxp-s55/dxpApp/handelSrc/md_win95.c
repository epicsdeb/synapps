/*****************************************************************************
 *
 *  md_win95.c
 *
 *  Created 06-Oct-1997 Ed Oltman
 *	Add EPP support	01/29/01 JW
 *
 * Copyright (c) 2002,2003,2004, X-ray Instrumentation Associates
 *               2005,2006 XIA LLC
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
 *    This file contains the interface between the Jorway 73A CAMAC SCSI
 *    interface Dynamic Link Library and the DXP primitive calls.
 *
 */


/* System include files */

#include <windows.h>

/* When building on Cygwin _WIN32 should not be defined, but we
 * need to include windows.h, and that defines it.  Undefine it. */
#ifdef CYGWIN32
  #undef _WIN32
#endif

/* These pragmas are only valid on VC, not gcc */
#ifdef _WIN32
#pragma warning(disable : 4115)
#pragma warning(default : 4115)

/* Disables the warning about "conditional expressions" being "constant".
 * This is caused by our use of the TRUE_ and FALSE_ macros that evaluate
 * to statements like (1 == 1).
 */
#pragma warning(disable : 4127)
#endif

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

/* XIA include files */
#include "xerxes_errors.h"
#include "xerxes_structures.h"
#include "md_win95.h"
#include "md_generic.h"
#include "xia_md.h"
#include "xia_common.h"
#include "xia_assert.h"

#define MODE 4


#define SERIAL_WAIT_TIMEOUT  ((unsigned long)1100)

#ifndef EXCLUDE_CAMAC
/* variables to store the IO channel information */
static char *camacName[MAXMOD];
static unsigned int numDXP    = 0;
#endif

/* total number of modules in the system */
static unsigned int numMod    = 0;

/* error string used as a place holder for calls to dxp_md_error() */
static char error_string[LINE_LEN];

/* are we in debug mode? */
static int print_debug=0;

/* maximum number of words able to transfer in a single call to dxp_md_io() */
static unsigned int maxblk=0;

#ifndef EXCLUDE_EPP
/* EPP definitions */
/* The id variable stores an optional ID number associated with each module 
 * (initially included for handling multiple EPP modules hanging off the same
 * EPP address)
 */
static unsigned int numEPP    = 0;
static int eppID[MAXMOD];
/* variables to store the IO channel information */
static char *eppName[MAXMOD];
/* Port stores the port number for each module, only used for the X10P/G200 */
static unsigned short port;

static unsigned short next_addr = 0;

/* Store the currentID used for Daisy Chain systems */
static int currentID = -1;
#endif /* EXCLUDE_EPP */


#ifndef EXCLUDE_USB
/* USB definitions */
/* The id variable stores an optional ID number associated with each module 
 */
static unsigned int numUSB    = 0;
/* static int usbID[MAXMOD]; */
/* variables to store the IO channel information */
static char *usbName[MAXMOD];
/* variables to store the USB Device Name */
/* static char *usbDevice[MAXMOD]; */

/* static HANDLE usbHandle[MAXMOD]; */

static long usb_addr=0;
#endif /* EXCLUDE_USB IO */


#ifndef EXCLUDE_USB2

#include "xia_usb2.h"
#include "xia_usb2_errors.h"

static unsigned int numUSB2   = 0;
/* A string holding the device number in it. */
static char *usb2Names[MAXMOD];

/* The OS handle used for communication. */
static HANDLE usb2Handles[MAXMOD];

/* The cached target address for the next operation. */
static unsigned long usb2AddrCache[MAXMOD];

#endif /* EXCLUDE_USB2 */


#ifndef EXCLUDE_SERIAL

#include "seriallib.h"

#define HEADER_SIZE 4

static unsigned int numSerial = 0;
/* variables to store the IO channel information */
static char *serialName[MAXMOD];

/* Serial port globals */

static int dxp_md_serial_read_header(unsigned short port, unsigned short *bytes,
									 unsigned short *buf);
static int dxp_md_serial_read_data(unsigned short port, unsigned long baud, 
                    unsigned long size, unsigned short *buf);
static int dxp_md_serial_reset(unsigned short port, unsigned long baud);
#endif /* EXCLUDE_SERIAL */


#ifndef EXCLUDE_PLX

#include "plxlibapi.h"
#include "plxlib_errors.h"

static unsigned int numPLX    = 0;
static HANDLE pxiHandles[MAXMOD];
static char * pxiNames[MAXMOD];

#endif /* EXCLUDE_PLX */


/* Globals that don't depend on the communication protocol. */
static char * TMP_PATH = NULL;
static char * PATH_SEP = "\\";


/******************************************************************************
 *
 * Routine to create pointers to the MD utility routines
 * 
 ******************************************************************************/
XIA_MD_EXPORT int XIA_MD_API dxp_md_init_util(Xia_Util_Functions* funcs, char* type)
/* Xia_Util_Functions *funcs;	*/
/* char *type;					*/
{
    /* Check to see if we are intializing the library with this call in addition
     * to assigning function pointers. 
     */

    if (type != NULL) 
    {
	if (STREQ(type, "INIT_LIBRARY")) { numMod = 0; }
    }

    funcs->dxp_md_error_control  = dxp_md_error_control;
    funcs->dxp_md_alloc          = dxp_md_alloc;
    funcs->dxp_md_free           = dxp_md_free;
    funcs->dxp_md_puts           = dxp_md_puts;
    funcs->dxp_md_wait           = dxp_md_wait;

    funcs->dxp_md_error          = dxp_md_error;
    funcs->dxp_md_warning        = dxp_md_warning;
    funcs->dxp_md_info           = dxp_md_info;
    funcs->dxp_md_debug          = dxp_md_debug;
    funcs->dxp_md_output         = dxp_md_output;
    funcs->dxp_md_suppress_log   = dxp_md_suppress_log;
    funcs->dxp_md_enable_log     = dxp_md_enable_log;
    funcs->dxp_md_set_log_level  = dxp_md_set_log_level;
    funcs->dxp_md_log	           = dxp_md_log;
    funcs->dxp_md_set_priority   = dxp_md_set_priority;
    funcs->dxp_md_fgets          = dxp_md_fgets;
    funcs->dxp_md_tmp_path       = dxp_md_tmp_path;
    funcs->dxp_md_clear_tmp      = dxp_md_clear_tmp;
    funcs->dxp_md_path_separator = dxp_md_path_separator;

    if (out_stream == NULL)
    {
	out_stream = stdout;
    }

	md_md_alloc = dxp_md_alloc;
	md_md_free  = dxp_md_free;

    return DXP_SUCCESS;
}

/******************************************************************************
 *
 * Routine to create pointers to the MD utility routines
 * 
 ******************************************************************************/
XIA_MD_EXPORT int XIA_MD_API dxp_md_init_io(Xia_Io_Functions* funcs, char* type)
/* Xia_Io_Functions *funcs;		*/
/* char *type;					*/
{
    unsigned int i;
	
    for (i = 0; i < strlen(type); i++) {

	type[i]= (char)tolower(type[i]);
    }


#ifndef EXCLUDE_CAMAC
    if (STREQ(type, "camac")) {

	funcs->dxp_md_io         = dxp_md_io;
	funcs->dxp_md_initialize = dxp_md_initialize;
	funcs->dxp_md_open       = dxp_md_open;
	funcs->dxp_md_close      = dxp_md_close;
    } 
#endif /* EXCLUDE_CAMAC */

#ifndef EXCLUDE_EPP
    if (STREQ(type, "epp")) {

	funcs->dxp_md_io         = dxp_md_epp_io;
	funcs->dxp_md_initialize = dxp_md_epp_initialize;
	funcs->dxp_md_open       = dxp_md_epp_open;
	funcs->dxp_md_close      = dxp_md_epp_close;
    }
#endif /* EXCLUDE_EPP */

#ifndef EXCLUDE_USB
    if (STREQ(type, "usb")) 
	  {
		funcs->dxp_md_io            = dxp_md_usb_io;
		funcs->dxp_md_initialize    = dxp_md_usb_initialize;
		funcs->dxp_md_open          = dxp_md_usb_open;
		funcs->dxp_md_close         = dxp_md_usb_close;
	  }
#endif /* EXCLUDE_USB */

#ifndef EXCLUDE_SERIAL
    if (STREQ(type, "serial")) 
	  {
		funcs->dxp_md_io            = dxp_md_serial_io;
		funcs->dxp_md_initialize    = dxp_md_serial_initialize;
		funcs->dxp_md_open          = dxp_md_serial_open;
		funcs->dxp_md_close         = dxp_md_serial_close;
	  }
#endif /* EXCLUDE_SERIAL */

#ifndef EXCLUDE_PLX
	/* Technically, the communications protocol is 'PXI', though the 
	 * driver is a PLX driver, which is why there are two different names
	 * used here.
	 */
	if (STREQ(type, "pxi")) {
		funcs->dxp_md_io            = dxp_md_plx_io;
		funcs->dxp_md_initialize    = dxp_md_plx_initialize;
		funcs->dxp_md_open          = dxp_md_plx_open;
		funcs->dxp_md_close         = dxp_md_plx_close;	  
	}
#endif /* EXCLUDE_PLX */

#ifndef EXCLUDE_USB2
  if (STREQ(type, "usb2")) {
    funcs->dxp_md_io            = dxp_md_usb2_io;
    funcs->dxp_md_initialize    = dxp_md_usb2_initialize;
    funcs->dxp_md_open          = dxp_md_usb2_open;
    funcs->dxp_md_close         = dxp_md_usb2_close;
  }
#endif /* EXCLUDE_USB2 */

    funcs->dxp_md_get_maxblk = dxp_md_get_maxblk;
    funcs->dxp_md_set_maxblk = dxp_md_set_maxblk;
    funcs->dxp_md_lock_resource = NULL;


    return DXP_SUCCESS;
}

#ifndef EXCLUDE_CAMAC
/*****************************************************************************
 * 
 * Initialize the system.  Alloocate the space for the library arrays, define
 * the pointer to the CAMAC library and the IO routine.
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_initialize(unsigned int* maxMod, char* dllname)
/* unsigned int *maxMod;					Input: maximum number of dxp modules allowed */
/* char *dllname;							Input: name of the DLL						*/
{
    int status = DXP_SUCCESS; 
    int lstatus = -1;
    int len;
	
    short buf[256];

    /* "dllname" argument should be used to keep the compiler happy.
     * We may choose to expand the functionality of the library
     * to use this in the future...
     */
    len = strlen(dllname);


    /* Initialize the CAMAC interface */
		
    /* check if all the memory was allocated */
    if (*maxMod>MAXMOD){
	status = DXP_NOMEM;
	sprintf(error_string,"Calling routine requests %d maximum modules: only %d available.", 
		*maxMod, MAXMOD);
	dxp_md_log_error("dxp_md_initialize", error_string, status);
	return status;
    }

    /* Zero out the number of modules currently in the system */
    numDXP = 0;

    /* Initialize the camac-SCSI interface */
    lstatus = xia_caminit(buf);
    if (lstatus!=0){
	sprintf(error_string,"camxfr error: status = 0x%lX, while trying to initialize DLL",lstatus);
	dxp_md_log_error("dxp_md_initialize",error_string,status);
	status = DXP_MDINITIALIZE;
	return status;
    }
	
    return status;
}
/*****************************************************************************
 * 
 * Routine is passed the user defined configuration string *name.  This string
 * contains all the information needed to point to the proper IO channel by 
 * future calls to dxp_md_io().  In the case of a simple CAMAC crate, the string 
 * should contain a branch number (representing the SCSI bus number in this case),
 * a crate number (single SCSI can control multiple crates) and a station 
 * number (better known as a slot number).
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_open(char* ioname, int* camChan)
/* char *ioname;						Input:  string used to specify this IO 
   channel */
/* int *camChan;						Output: returns a reference number for
   this module */
{
    unsigned int i;
    int status=DXP_SUCCESS;

    /* First loop over the existing names to make sure this module 
     * was not already configured?  Don't want/need to perform
     * this operation 2 times. */
    
    for(i=0;i<numDXP;i++){
	if(STREQ(camacName[i],ioname)) {
	    status=DXP_SUCCESS;
	    *camChan = i;
	    return status;
	}
    }

    /* Got a new one.  Increase the number existing and assign the global 
     * information */

    if (camacName[numDXP]!=NULL) {
	md_md_free(camacName[numDXP]);
    }
    camacName[numDXP] = (char *) md_md_alloc((strlen(ioname)+1)*sizeof(char));
    strcpy(camacName[numDXP],ioname);

    *camChan = numDXP++;
    numMod++;

    return status;
}

/*****************************************************************************
 * 
 * This routine performs the IO call to read or write data.  The pointer to 
 * the desired IO channel is passed as camChan.  The address to write to is
 * specified by function and address.  The data length is specified by 
 * length.  And the data itself is stored in data.
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_io(int* camChan, unsigned int* function, 
				       unsigned long *address, void *data,
				       unsigned int* length)
/* int *camChan;						Input: pointer to IO channel to access		*/
/* unsigned int *function;				Input: function number to access (CAMAC F)	*/
/* unsigned int *address;				Input: address to access	(CAMAC A)		*/
/* unsigned short *data;				I/O:  data read or written					*/
/* unsigned int *length;				Input: how much data to read or write		*/
{

    short camadr[4];
    long lstatus=0;
    int status;
    int branch, crate, station;
    /* mode defines Q-stop, etc... types of transfer */
    short mode=MODE;

    short func = (short) *function;
    unsigned long  nbytes;

    sscanf(camacName[*camChan],"%1d%1d%2d",&branch,&crate,&station);

    /* prelimenary definitions used by the DLL routine */
    nbytes=2*(*length);
    camadr[0] = (short) branch;
    camadr[1] = (short) crate;
    camadr[2] = (short) station;
    camadr[3] = (short) *address;

    /* Now perform the read or write operation using the routine */

    lstatus = xia_camxfr(camadr, func, nbytes, mode, (short *)data);
    if ((lstatus!=0)&&(lstatus!=4)){
	status = DXP_MDIO;
	sprintf(error_string,"camxfr error: status = 0x%lX",lstatus);
	dxp_md_log_error("dxp_md_io",error_string,status);
	return status;
    }

    /* All done, free the library and get out */
    status=DXP_SUCCESS;

    return status;
}


/**********
 * This routine is used to "close" the CAMAC connection.
 * For CAMAC, nothing needs to be done...
 **********/
XIA_MD_STATIC int XIA_MD_API dxp_md_close(int *camChan)
{
    UNUSED(camChan);

    return DXP_SUCCESS;
}

#endif /* EXCLUDE_CAMAC */


#ifndef EXCLUDE_EPP
/*****************************************************************************
 * 
 * Initialize the system.  Alloocate the space for the library arrays, define
 * the pointer to the CAMAC library and the IO routine.
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_initialize(unsigned int* maxMod, char* dllname)
/* unsigned int *maxMod;					Input: maximum number of dxp modules allowed */
/* char *dllname;							Input: name of the DLL						*/
{
    int status = DXP_SUCCESS;
    int rstat = 0;

    /* EPP initialization */

    /* check if all the memory was allocated */
    if (*maxMod>MAXMOD){
	status = DXP_NOMEM;
	sprintf(error_string,"Calling routine requests %d maximum modules: only %d available.", 
		*maxMod, MAXMOD);
	dxp_md_log_error("dxp_md_epp_initialize",error_string,status);
	return status;
    }

    /* Zero out the number of modules currently in the system */
    numEPP = 0;

    /* Initialize the EPP port */
    rstat = sscanf(dllname,"%hx",&port);
    if (rstat!=1) {
	status = DXP_NOMATCH;
	dxp_md_log_error("dxp_md_epp_initialize",
			 "Unable to read the EPP port address",status);
	return status;
    }
													 
    sprintf(error_string, "EPP Port = %#x", port);
    dxp_md_log_debug("dxp_md_epp_initialize", error_string);

    /* Move the call to InitEPP() to the open() routine, this will allow daisy chain IDs to work. 
     * NOTE: since the port number is stored in a static global, init() better not get called again
     * with a different port, before the open() call!!!!!
     */ 
    /* Call the EPPLIB.DLL routine */	
    /*  rstat = DxpInitEPP((int)port);*/


    /* Check for Success */
    /*  if (rstat==0) {
	status = DXP_SUCCESS;
	} else {
	status = DXP_INITIALIZE;
	sprintf(error_string,
	"Unable to initialize the EPP port: rstat=%d",rstat);
	dxp_md_log_error("dxp_md_epp_initialize", error_string,status);
	return status;
	}*/

	/* Reset the currentID when the EPP interface is initialized */
	currentID = -1;

    return status;
}
/*****************************************************************************
 * 
 * Routine is passed the user defined configuration string *name.  This string
 * contains all the information needed to point to the proper IO channel by 
 * future calls to dxp_md_io().  In the case of a simple CAMAC crate, the string 
 * should contain a branch number (representing the SCSI bus number in this case),
 * a crate number (single SCSI can control multiple crates) and a station 
 * number (better known as a slot number).
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_open(char* ioname, int* camChan)
/* char *ioname;							Input:  string used to specify this IO 
   channel */
/* int *camChan;						Output: returns a reference number for
   this module */
{
    unsigned int i;
    int status=DXP_SUCCESS;
    int rstat = 0;

    sprintf(error_string, "ioname = %s", ioname);
    dxp_md_log_debug("dxp_md_epp_open", error_string);

    /* First loop over the existing names to make sure this module 
     * was not already configured?  Don't want/need to perform
     * this operation 2 times. */
    
	for(i=0;i<numEPP;i++)
	  {
		if(STREQ(eppName[i],ioname)) 
		  {
			status=DXP_SUCCESS;
			*camChan = i;
			return status;
		  }
	  }

    /* Got a new one.  Increase the number existing and assign the global 
     * information */

    if (eppName[numEPP]!=NULL) 
	  {
		md_md_free(eppName[numEPP]);
	  }
    eppName[numEPP] = (char *) md_md_alloc((strlen(ioname)+1)*sizeof(char));
    strcpy(eppName[numEPP],ioname);

    /* See if this is a multi-module EPP chain, if not set its ID to -1 */
    if (ioname[0] == ':') 
	  {
		sscanf(ioname, ":%d", &(eppID[numEPP]));
		
		sprintf(error_string, "ID = %i", eppID[numEPP]);
		dxp_md_log_debug("dxp_md_epp_open", error_string);
		
		/* Initialize the port address first */
		rstat = DxpInitPortAddress((int) port);
		if (rstat != 0) 
		  {
			status = DXP_INITIALIZE;
			sprintf(error_string,
					"Unable to initialize the EPP port address: port=%d", port);
			dxp_md_log_error("dxp_md_epp_open", error_string, status);
			return status;
		  }
		
		/* Call setID now to setup the port for Initialization */
		DxpSetID((unsigned short) eppID[numEPP]); 
		/* No return value
		   if (rstat != 0) 
		   {
		   status = DXP_INITIALIZE;
		   sprintf(error_string,
		   "Unable to set the EPP Port ID: ID=%d", id[numEPP]);
		   dxp_md_log_error("dxp_md_epp_open", error_string, status);
		   return status;
		   }*/
	  } else {
		eppID[numEPP] = -1;
	  }
	
    /* Call the EPPLIB.DLL routine */	
    rstat = DxpInitEPP((int)port);
	
    /* Check for Success */
    if (rstat==0) 
	  {
		status = DXP_SUCCESS;
	  } else {
		status = DXP_INITIALIZE;
		sprintf(error_string,
				"Unable to initialize the EPP port: rstat=%d",rstat);
		dxp_md_log_error("dxp_md_epp_open", error_string,status);
		return status;
	  }
	
    *camChan = numEPP++;
    numMod++;

    return status;
}


/*****************************************************************************
 * 
 * This routine performs the IO call to read or write data.  The pointer to 
 * the desired IO channel is passed as camChan.  The address to write to is
 * specified by function and address.  The data length is specified by 
 * length.  And the data itself is stored in data.
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_io(int* camChan, unsigned int* function, 
					   unsigned long *address, void *data,
					   unsigned int* length)
/* int *camChan;				Input: pointer to IO channel to access	*/
/* unsigned int *function;			Input: XIA EPP function definition	*/
/* unsigned int *address;			Input: XIA EPP address definition	*/
/* unsigned short *data;			I/O:  data read or written		*/
/* unsigned int *length;			Input: how much data to read or write	*/
{
    int rstat = 0; 
    int status;
  
    unsigned int i;

	unsigned short *us_data = (unsigned short *)data;

    unsigned long *temp=NULL;


    if ((currentID != eppID[*camChan]) && (eppID[*camChan] != -1))
	  {
		DxpSetID((unsigned short) eppID[*camChan]);
		
		/* Update the currentID */
		currentID = eppID[*camChan];
		
		sprintf(error_string, "calling SetID = %i, camChan = %i", eppID[*camChan], *camChan);
		dxp_md_log_debug("dxp_md_epp_io", error_string);
	  }
	
    /* Data*/
    if (*address==0) {
	/* Perform short reads and writes if not in program address space */
	if (next_addr>=0x4000) {
	    if (*length>1) {
		if (*function == MD_IO_READ) {
		    rstat = DxpReadBlock(next_addr, us_data, (int) *length);
		} else {
		    rstat = DxpWriteBlock(next_addr, us_data, (int) *length);
		}
	    } else {
		if (*function == MD_IO_READ) {
		    rstat = DxpReadWord(next_addr, us_data);
		} else {
		    rstat = DxpWriteWord(next_addr, us_data[0]);
		}
	    }
	} else {
	    /* Perform long reads and writes if in program address space (24-bit) */
	    /* Allocate memory */
	    temp = (unsigned long *) md_md_alloc(sizeof(unsigned short)*(*length));
	    if (*function == MD_IO_READ) {
		rstat = DxpReadBlocklong(next_addr, temp, (int) *length/2);
		/* reverse the byte order for the EPPLIB library */
		for (i=0;i<*length/2;i++) {
		    us_data[2*i] = (unsigned short) (temp[i]&0xFFFF);
		    us_data[2*i+1] = (unsigned short) ((temp[i]>>16)&0xFFFF);
		}
	    } else {
		/* reverse the byte order for the EPPLIB library */
		for (i=0;i<*length/2;i++) {
		    temp[i] = ((us_data[2*i]<<16) + us_data[2*i+1]);
		}
		rstat = DxpWriteBlocklong(next_addr, temp, (int) *length/2);
	    }
	    /* Free the memory */
	    md_md_free(temp);
	}
	/* Address port*/
    } else if (*address==1) {
	next_addr = (unsigned short) *us_data;
	/* Control port*/
    } else if (*address==2) {
	/*		dest = cport;
	 *length = 1;
	 */
	/* Status port*/
    } else if (*address==3) {
	/*		dest = sport;
	 *length = 1;*/
    } else {
	sprintf(error_string,"Unknown EPP address=%ld",*address);
	status = DXP_MDIO;
	dxp_md_log_error("dxp_md_epp_io",error_string,status);
	return status;
    }

    if (rstat!=0) {
	status = DXP_MDIO;
	sprintf(error_string,"Problem Performing I/O to Function: %d, address: %#lx",*function, *address);
	dxp_md_log_error("dxp_md_epp_io",error_string,status);
	sprintf(error_string,"Trying to write to internal address: %d, length %d",next_addr, *length);
	dxp_md_log_error("dxp_md_epp_io",error_string,status);
	return status;
    }
	
    status=DXP_SUCCESS;

    return status;
}


/**********
 * "Closes" the EPP connection, which means that it does nothing.
 **********/
XIA_MD_STATIC int XIA_MD_API dxp_md_epp_close(int *camChan)
{
    UNUSED(camChan);
    
    return DXP_SUCCESS;
}

#endif /* EXCLUDE_EPP */

#ifndef EXCLUDE_USB
/*****************************************************************************
 * 
 * Initialize the USB system.  
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_initialize(unsigned int* maxMod, char* dllname)
/* unsigned int *maxMod;					Input: maximum number of dxp modules allowed */
/* char *dllname;							Input: name of the DLL						*/
{
    int status = DXP_SUCCESS;

	UNUSED(dllname);

    /* USB initialization */

    /* check if all the memory was allocated */
    if (*maxMod>MAXMOD)
	  {
		status = DXP_NOMEM;
		sprintf(error_string,"Calling routine requests %d maximum modules: only %d available.", 
				*maxMod, MAXMOD);
		dxp_md_log_error("dxp_md_usb_initialize",error_string,status);
		return status;
	  }

    /* Zero out the number of modules currently in the system */
    numUSB = 0;

    return status;
}
/*****************************************************************************
 * 
 * Routine is passed the user defined configuration string *name.  This string
 * contains all the information needed to point to the proper IO channel by 
 * future calls to dxp_md_io().  In the case of a simple CAMAC crate, the string 
 * should contain a branch number (representing the SCSI bus number in this case),
 * a crate number (single SCSI can control multiple crates) and a station 
 * number (better known as a slot number).
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_open(char* ioname, int* camChan)
	 /* char *ioname;				 Input:  string used to specify this IO channel */
	 /* int *camChan;				 Output: returns a reference number for this module */
{
  int status=DXP_SUCCESS;
  unsigned int i;

  /* Temporary name so that we can get the length */
  char tempName[200];
  
  sprintf(error_string, "ioname = %s", ioname);
  dxp_md_log_debug("dxp_md_usb_open", error_string);
  
  /* First loop over the existing names to make sure this module 
   * was not already configured?  Don't want/need to perform
   * this operation 2 times. */
  
  for(i=0;i<numUSB;i++)
	{
	  if(STREQ(usbName[i],ioname)) 
		{
		  status=DXP_SUCCESS;
		  *camChan = i;
		  return status;
		}
	}
  
  /* Got a new one.  Increase the number existing and assign the global 
   * information */
  sprintf(tempName, "\\\\.\\ezusb-%s", ioname); 
  
  if (usbName[numUSB]!=NULL) 
	{
	  md_md_free(usbName[numUSB]);
	}
  usbName[numUSB] = (char *) md_md_alloc((strlen(tempName)+1)*sizeof(char));
  strcpy(usbName[numUSB],tempName);
  
  /*  dxp_md_log_info("dxp_md_usb_open", "Attempting to open usb handel");
	  if (usb_open(usbName[numUSB], &usbHandle[numUSB]) != 0) 
	  {
	  dxp_md_log_info("dxp_md_usb_open", "Unable to open usb handel");
	  }
	  sprintf(error_string, "device = %s, handle = %i", usbName[numUSB], usbHandle[numUSB]);
	  dxp_md_log_info("dxp_md_usb_open", error_string);
  */
    *camChan = numUSB++;
    numMod++;

    return status;
}


/*****************************************************************************
 * 
 * This routine performs the IO call to read or write data.  The pointer to 
 * the desired IO channel is passed as camChan.  The address to write to is
 * specified by function and address.  The data length is specified by 
 * length.  And the data itself is stored in data.
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_io(int* camChan, unsigned int* function, 
										   unsigned long *address,
										   void *data,
										   unsigned int* length)
	 /* int *camChan;				Input: pointer to IO channel to access	*/
	 /* unsigned int *function;		Input: XIA EPP function definition	    */
	 /* unsigned int *address;		Input: XIA EPP address definition	    */
	 /* unsigned short *data;		I/O:  data read or written		        */
	 /* unsigned int *length;		Input: how much data to read or write	*/
{
  int rstat = 0; 
  int status;
  
  unsigned short *us_data = (unsigned short *)data;

  if (*address == 0) 
	{
	  if (*function == MD_IO_READ) 
		{
		  rstat = usb_read(usb_addr, (long) *length, usbName[*camChan], us_data);
		} else {
		  rstat = usb_write(usb_addr, (long) *length, usbName[*camChan], us_data);
		}
	} else if (*address ==1) {
	  usb_addr = (long) us_data[0];
	}

  if (rstat != 0) 
	{
	  status = DXP_MDIO;
	  sprintf(error_string,"Problem Performing USB I/O to Function: %d, address: %ld",*function, *address);
	  dxp_md_log_error("dxp_md_usb_io",error_string,status);
	  sprintf(error_string,"Trying to write to internal address: %#lx, length %d", usb_addr, *length);
	  dxp_md_log_error("dxp_md_usb_io",error_string,status);
	  return status;
	}
  
  status=DXP_SUCCESS;
  
  return status;
}


/**********
 * "Closes" the USB connection, which means that it does nothing.
 **********/
XIA_MD_STATIC int XIA_MD_API dxp_md_usb_close(int *camChan)
{
    UNUSED(camChan);

    return DXP_SUCCESS;
}

#endif


#ifndef EXCLUDE_SERIAL

/**
 * @brief Initialized any global structures.
 *
 * Only called once per library load.
 */
XIA_MD_STATIC int dxp_md_serial_initialize(unsigned int *maxMod,
                                           char *dllname)
{
  int i;

  UNUSED(maxMod);
  UNUSED(dllname);


  numSerial = 0;

  for (i = 0; i < MAXMOD; i++) {
    serialName[i] = NULL;
  }

  return DXP_SUCCESS;
}


/**
 * @brief Initializes a new COM port instance.
 *
 * This routine is called each time that a COM port is opened. 
 * @a ioname encodes COM port and baud rate as "[port]:[baud_rate]".
 */
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_open(char* ioname, int* camChan)
{
  int status;
  int len_ioname;

  unsigned int i;
  unsigned int port;
  unsigned long baud;

  /* Check for an existing definition. */
  for(i = 0; i < numSerial; i++) {
    if(STREQ(serialName[i], ioname)) {
      *camChan = i;
      return DXP_SUCCESS;
    }
  }
  
  /* Parse in the COM port number and baud rate. */
  sscanf(ioname, "%u:%u", &port, &baud);

  *camChan = numSerial++;

  status = InitSerialPort(port, baud);

  if (status != SERIAL_SUCCESS) {
    sprintf(error_string, "Error opening COM%u, where the driver reports "
            "%d", port, status);
    dxp_md_log_error("dxp_md_serial_open", error_string, DXP_MDOPEN);
    return DXP_MDOPEN;
  }

  ASSERT(serialName[*camChan] == NULL);

  len_ioname = strlen(ioname) + 1;

  serialName[*camChan] = md_md_alloc(len_ioname);

  if (serialName[*camChan] == NULL) {
    sprintf(error_string, "Unable to allocate %u bytes for serialName[%d]",
            len_ioname, *camChan);
    dxp_md_log_error("dxp_md_serial_open", error_string, DXP_MDNOMEM);
    return DXP_MDNOMEM;
  }

  strcpy(serialName[*camChan], ioname);

  numMod++;

  return DXP_SUCCESS;
}


/**********
 * This routine performs the read/write operations 
 * on the serial port.
 **********/
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_io(int *camChan,
					      unsigned int *function,
					      unsigned long *wait_in_ms,
					      void *data,
					      unsigned int *length)
{
    LARGE_INTEGER freq;
    LARGE_INTEGER before, after;

    int status;

    unsigned int i;

	unsigned short n_bytes = 0;
  unsigned short comPort;
  unsigned long baud;

	unsigned short *us_data = (unsigned short *)data;

    byte_t *buf    = NULL;

	double to = (double)(*wait_in_ms);


    /* Get the proper comPort information
     * Use this when we need to support multiple
     * serial ports.
     */
    sscanf(serialName[*camChan], "%u:%u", &comPort, &baud);

    QueryPerformanceFrequency(&freq);


    if (*function == MD_IO_READ) {

	  QueryPerformanceCounter(&before);

	  status = SetTimeoutInMS(comPort, to);

	  if (status != SERIAL_SUCCESS) {
		sprintf(error_string,
				"Error setting timeout to %.3f milliseconds on COM%u",
				to, comPort);
		dxp_md_log_error("dxp_md_serial_io", error_string, DXP_MDIO);
		return DXP_MDIO;
	  }

	  status = dxp_md_serial_read_header(comPort, &n_bytes, us_data);

	  if (status != DXP_SUCCESS) {
		dxp_md_log_error("dxp_md_serial_io", "Error reading header", status);
		return status;
	  }

	  QueryPerformanceCounter(&after);

	  /* Caclulate the timeout time based on a conservative transfer rate
	   * of 5kb/s with a minimum of 1000 milliseconds.
	   */
	  to = (double)((n_bytes / 5000.0) * 1000.0);

	  if (to < 1000.0) {
		to = 1000.0;
	  }

	  status = SetTimeoutInMS(comPort, to);

	  if (status != SERIAL_SUCCESS) {
		sprintf(error_string, "Error setting timeout to %.3f milliseconds", to);
		dxp_md_log_error("dxp_md_serial_io", error_string, DXP_MDIO);
		return DXP_MDIO;
	  }

	  status = dxp_md_serial_read_data(comPort, baud, n_bytes, us_data + HEADER_SIZE);

	  if (status != DXP_SUCCESS) {
		dxp_md_log_error("dxp_md_serial_io", "Error reading data", status);
		return status;
	  }


    } else if (*function == MD_IO_WRITE) {

	  /* Write to the serial port */

	  status = CheckAndClearTransmitBuffer(comPort);

	  if (status != SERIAL_SUCCESS) {
	    status = DXP_MDIO;
	    dxp_md_log_error("dxp_md_serial_io", "Error clearing transmit buffer", status);
	    return status;
	  }

	  status = CheckAndClearReceiveBuffer(comPort);

	  if (status != SERIAL_SUCCESS) {
	    status = DXP_MDIO;
	    dxp_md_log_error("dxp_md_serial_io", "Error clearing receive buffer", status);
	    return status;
	  }

	  buf = (byte_t *)md_md_alloc(*length * sizeof(byte_t));

	  if (buf == NULL) {

	    status = DXP_NOMEM;
	    sprintf(error_string, "Error allocating %u bytes for buf",
				*length);
	    dxp_md_log_error("dxp_md_serial_io", error_string, status);
	    return status;
	  }

	  for (i = 0; i < *length; i++) {

	    buf[i] = (byte_t)us_data[i];
	  }

	  status = WriteSerialPort(comPort, *length, buf);

	  if (status != SERIAL_SUCCESS) {

	    md_md_free((void *)buf);
	    buf = NULL;

	    status = DXP_MDIO;
	    sprintf(error_string, "Error writing %u bytes to COM%u",
				*length, comPort);
	    dxp_md_log_error("dxp_md_serial_io", error_string, status);
	    return status;
	  }

	  md_md_free((void *)buf);
	  buf = NULL;

    } else if (*function == MD_IO_OPEN) {
	  /* Do nothing */
    } else if (*function == MD_IO_CLOSE) {
	  /* Do nothing */
    } else {

	status = DXP_MDUNKNOWN;
	sprintf(error_string, "Unknown function: %u", *function);
	dxp_md_log_error("dxp_md_serial_io", error_string, status);
	return status;
    }

    return DXP_SUCCESS;
}


/** @brief Closes and re-opens the port
 *
 */
static int dxp_md_serial_reset(unsigned short port, unsigned long baud)
{
  int status;


  status = CloseSerialPort(port);

  if (status != SERIAL_SUCCESS) {
	dxp_md_log_error("dxp_md_serial_reset", "Error closing port", DXP_MDIO);
	return DXP_MDIO;
  }

  status = InitSerialPort(port, baud);

  if (status != SERIAL_SUCCESS) {
	dxp_md_log_error("dxp_md_serial_reset", "Error re-initializing port",
					 DXP_MDIO);
	return DXP_MDIO;
  }

  return DXP_SUCCESS;
}


/** @brief Reads the header of the returned packet
 *
 * Reads the header of the returned packet and puts it into the user
 * specified buffer. Also calculates the number of bytes remaining
 * in the packet including the XOR checksum.
 *
 */
static int dxp_md_serial_read_header(unsigned short port, unsigned short *bytes,
									 unsigned short *buf)
{
  int i;

  byte_t lo;
  byte_t hi;
  byte_t b;

  byte_t header[HEADER_SIZE];

  serial_read_error_t *err = NULL;


  ASSERT(bytes != NULL);
  ASSERT(buf != NULL);


  for (i = 0; i < HEADER_SIZE; i++) {

	err = ReadSerialPort(port, 1, &b);

	if (err->status != SERIAL_SUCCESS) {
	  sprintf(error_string, "Error reading header from COM%u: actual = %d, "
			  "expected = %d, bytes_in_recv_buf = %d, size_recv_buf = %d", port,
			  err->actual, err->expected, err->bytes_in_recv_buf,
			  err->size_recv_buf);
	  dxp_md_log_error("dxp_md_serial_read_header", error_string, DXP_MDIO);
	  return DXP_MDIO;
	}

	header[i] = b;
  }

  /* XXX Is any error checking done elsewhere to verify
   * the initial, non-size bytes in the header? Perhaps in
   * dxp_cmd()/dxp_command()?
   */
  lo = header[2];
  hi = header[3];

  /* Include the XOR checksum in this calculation */
  *bytes = (unsigned short)(((unsigned short)lo | 
							 ((unsigned short)hi << 8)) + 1);

  if (*bytes == 1) {
	dxp_md_log_debug("dxp_md_serial_read_header",
					 "Number of data bytes = 1 in header");
	for (i = 0; i < HEADER_SIZE; i++) {
	  sprintf(error_string, "header[%d] = %#x", i, (unsigned short)header[i]);
	  dxp_md_log_debug("dxp_md_serial_read_header",
					   error_string);
	}
  }

  /* Copy header into data buffer. Can't use memcpy() since we are
   * going from byte_t -> unsigned short.
   */
  for (i = 0; i < HEADER_SIZE; i++) {
	buf[i] = (unsigned short)header[i];
  }

  return DXP_SUCCESS;
}


/** @brief Reads the specified number of bytes from the port and copies them to
 *  the buffer.
 *
 */
static int dxp_md_serial_read_data(unsigned short port, unsigned long baud,
                    unsigned long size, unsigned short *buf)
{
  int status;

  unsigned long i;

  byte_t *b = NULL;

  serial_read_error_t *err = NULL;


  ASSERT(buf != NULL);


  b = (byte_t *)md_md_alloc(size * sizeof(byte_t));

  if (b == NULL) {
	sprintf(error_string, "Error allocating %lu bytes for 'b'", size);
	dxp_md_log_error("dxp_md_serial_read_data", error_string, DXP_NOMEM);
	return DXP_NOMEM;
  }

  err = ReadSerialPort(port, size, b);

  if (err->status != SERIAL_SUCCESS) {
	md_md_free(b);
	sprintf(error_string, "bytes_in_recv_buf = %d, size_recv_buf = %d",
			err->bytes_in_recv_buf, err->size_recv_buf);
	dxp_md_log_debug("dxp_md_serial_read_data", error_string);
	sprintf(error_string, "hardware_overrun = %s, software_overrun = %s",
			err->is_hardware_overrun > 0 ? "true" : "false",
			err->is_software_overrun > 0 ? "true" : "false");
	dxp_md_log_debug("dxp_md_serial_read_data", error_string);
	sprintf(error_string, "Error reading data from COM%u: "
			"expected = %d, actual = %d", port, err->expected, err->actual);
	dxp_md_log_error("dxp_md_serial_read_data", error_string, DXP_MDIO);

	status = dxp_md_serial_reset(port, baud);

	if (status != DXP_SUCCESS) {
	  sprintf(error_string,
			  "Error attempting to reset COM%u in response to a "
			  "communications failure", port);
	  dxp_md_log_error("dxp_md_serial_read_data", error_string, status);
	}

	return DXP_MDIO;
  }

  for (i = 0; i < size; i++) {
	buf[i] = (unsigned short)b[i];
  }

  md_md_free(b);

  return DXP_SUCCESS;
}


/**********
 * Closes the serial port connection so that we don't
 * crash due to interrupt vectors left lying around.
 **********/
XIA_MD_STATIC int XIA_MD_API dxp_md_serial_close(int *camChan)
{
    int status;

    unsigned int comPort;


    ASSERT(camChan != NULL);

    sprintf(error_string, "Called with camChan %d", *camChan);
    dxp_md_log_debug("dxp_md_serial_close", error_string);

    if (serialName[*camChan] != NULL) {
      sscanf(serialName[*camChan], "%u", &comPort);

      sprintf(error_string, "Preparing to close camChan %d, comPort %u",
              *camChan, comPort);
      dxp_md_log_debug("dxp_md_serial_close", error_string);

      status = CloseSerialPort(comPort);

      if (status != 0) {
        return DXP_MDCLOSE;
      }

      md_md_free(serialName[*camChan]);
      serialName[*camChan] = NULL;

      numSerial--;
    }

    return DXP_SUCCESS;
}


#endif /* EXCLUDE_SERIAL */  
 

/*****************************************************************************
 * 
 * Routine to control the error reporting operation.  Currently the only 
 * keyword available is "print_debug".  Then whenever dxp_md_error() is called
 * with an error_code=DXP_DEBUG, the message is printed.  
 * 
 *****************************************************************************/
XIA_MD_STATIC void XIA_MD_API dxp_md_error_control(char* keyword, int* value)
/* char *keyword;						Input: keyword to set for future use by dxp_md_error()	*/
/* int *value;							Input: value to set the keyword to					*/
{

    /* Enable debugging */

    if (strstr(keyword,"print_debug")!=NULL){
	print_debug=(*value);
	return;
    }

    /* Else we have a problem */

    dxp_md_puts("dxp_md_error_control: keyword %s not recognized..\n");
}

/*****************************************************************************
 * 
 * Routine to get the maximum number of words that can be block transfered at 
 * once.  This can change from system to system and from controller to 
 * controller.  A maxblk size of 0 means to write all data at once, regardless 
 * of transfer size.
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_get_maxblk(void)
{

    return maxblk;

}

/*****************************************************************************
 * 
 * Routine to set the maximum number of words that can be block transfered at 
 * once.  This can change from system to system and from controller to 
 * controller.  A maxblk size of 0 means to write all data at once, regardless 
 * of transfer size.
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_set_maxblk(unsigned int* blksiz)
/* unsigned int *blksiz;			*/
{
    int status; 

    if (*blksiz > 0) {
	maxblk = *blksiz;
    } else {
	status = DXP_NEGBLOCKSIZE;
	dxp_md_log_error("dxp_md_set_maxblk","Block size must be positive or zero",status);
	return status;
    }
    return DXP_SUCCESS;

}

/*****************************************************************************
 * 
 * Routine to wait a specified time in seconds.  This allows the user to call
 * routines that are as precise as required for the purpose at hand.
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_wait(float* time)
/* float *time;							Input: Time to wait in seconds	*/
{
    DWORD wait = (DWORD)(1000.0 * (*time));

    Sleep(wait);

    return DXP_SUCCESS;
}

/*****************************************************************************
 * 
 * Routine to allocate memory.  The calling structure is the same as the 
 * ANSI C standard routine malloc().  
 * 
 *****************************************************************************/
XIA_MD_SHARED void* XIA_MD_API dxp_md_alloc(size_t length)
/* size_t length;							Input: length of the memory to allocate
   in units of size_t (defined to be a 
   byte on most systems) */
{

    return malloc(length);

}

/*****************************************************************************
 * 
 * Routine to free memory.  Same calling structure as the ANSI C routine 
 * free().
 * 
 *****************************************************************************/
XIA_MD_SHARED void XIA_MD_API dxp_md_free(void* array)
/* void *array;							Input: pointer to the memory to free */
{

    free(array);

}

/*****************************************************************************
 * 
 * Routine to print a string to the screen or log.  No direct calls to 
 * printf() are performed by XIA libraries.  All output is directed either
 * through the dxp_md_error() or dxp_md_puts() routines.
 * 
 *****************************************************************************/
XIA_MD_STATIC int XIA_MD_API dxp_md_puts(char* s)
/* char *s;								Input: string to print or log	*/
{

    return printf("%s", s);

}


/** @brief Safe version of fgets() that can handle both UNIX and DOS
 * line-endings.
 *
 * If the trailing two characters are '\r' + \n', they are replaced by a
 * single '\n'.
 */
XIA_MD_STATIC char * dxp_md_fgets(char *s, int length, FILE *stream)
{
  char *cstatus = NULL;

  size_t ret_len = 0;


  ASSERT(s != NULL);
  ASSERT(stream != NULL);
  ASSERT(length > 0);


  cstatus = fgets(s, length - 1, stream);

  ret_len = strlen(s);

  if (!cstatus) {
    return NULL;
  }

  if ((ret_len > 1) &&
      (s[ret_len - 2] == '\r') &&
      (s[ret_len - 1] == '\n')) {
    s[ret_len - 2] = '\n';
    s[ret_len - 1] = '\0';
  }

  return s;
}


/** @brief Get a safe temporary directory path.
 *
 * If the temporary path is greater then the length of that passed in
 * string, an error is returned.
 *
 * The search order is defined at
 * http://msdn.microsoft.com/library/default.asp? \ 
 * url=/library/en-us/fileio/fs/gettemppath.asp
 *
 * NOTE: The temporary path is only computed once and the result is
 * cached. If we ever make Handel thread-safe, we won't be able to cache
 * the data as a global anymore.
 *
 */
XIA_MD_STATIC char * dxp_md_tmp_path(void)
{
  DWORD tmp_path_len  = 0;
  DWORD chars_written = 0;

  LPSTR tmp_path;


  if (TMP_PATH) {
    return TMP_PATH;
  }

  /* According to the docs for GetTempPath(), if the actual length is longer
   * then the passed in length, the actual size is returned. We can then
   * use the actual length to allocate the correct amount of memory.
   */
  tmp_path_len = GetTempPathA(0, NULL);

  if (tmp_path_len == 0) {
    dxp_md_log_error("dxp_md_tmp_path", "Windows returned an error trying to "
                     "determine the temporary file path.", DXP_WIN32_API);
    return NULL;
  }

  tmp_path = dxp_md_alloc(tmp_path_len + 1);
  
  if (!tmp_path) {
    sprintf(error_string, "Unable to allocate %ld bytes for 'tmp_path'.",
            tmp_path_len + 1);
    dxp_md_log_error("dxp_md_tmp_path", error_string, DXP_MDNOMEM);
    return NULL;
  }

  chars_written = GetTempPathA(tmp_path_len, tmp_path);

  if (chars_written == 0) {
    dxp_md_log_error("dxp_md_tmp_path", "Windows returned an error trying to "
                     "determine the temporary file path.", DXP_WIN32_API);
    return NULL;
  }
  
  TMP_PATH = (char *)tmp_path;

  return TMP_PATH;
}


/** @brief Clears the temporary path cache.
 *
 * Caution: This will free the cache if it is already allocated. Any pointers
 * that pointed at the old cache will probably segfault if you access them again
 * -- if you're lucky. This routine, mainly, exists for testing dxp_md_tmp_path()
 * and for cleaning up globally allocated resources on exit.
 */
XIA_MD_STATIC void dxp_md_clear_tmp(void)
{
  if (TMP_PATH) {
    dxp_md_free(TMP_PATH);
  }

  TMP_PATH = NULL;
}


/** @brief Returns the path separator for Win32 platform.
 *
 */
XIA_MD_STATIC char * dxp_md_path_separator(void)
{
  return PATH_SEP;
}


/** @brief Sets the priority of the current process using the Win32 API
 *
 */
XIA_MD_STATIC int XIA_MD_API dxp_md_set_priority(int *priority)
{
  BOOL status;

  DWORD pri;

  HANDLE h;

  char pri_str[5];


  h = GetCurrentProcess();

  switch(*priority) {
  
  default:
	sprintf(error_string, "Invalid priority type: %#x", *priority);
	dxp_md_log_error("dxp_md_set_priority", error_string, DXP_MDINVALIDPRIORITY);
	return DXP_MDINVALIDPRIORITY;
	break;

  case MD_IO_PRI_NORMAL:
	pri = NORMAL_PRIORITY_CLASS;
	strncpy(pri_str, "NORM", 5);
	break;

  case MD_IO_PRI_HIGH:
	pri = REALTIME_PRIORITY_CLASS;
	strncpy(pri_str, "HIGH", 5);
	break;
  }

  status = SetPriorityClass(h, pri);

  if (!status) {
	sprintf(error_string,
			"Error setting priority class (%s) for current process",
			pri_str);
	dxp_md_log_error("dxp_md_set_priority", error_string, DXP_MDPRIORITY);
	return DXP_MDPRIORITY;
  }

  return DXP_SUCCESS;
}


#ifndef EXCLUDE_PLX

/** @brief Initializes any global strucutures.
 *
 * This routine is called once per overall library session and, therefore,
 * its job is to initialize the global structures used to manage the
 * PXI handles.
 *
 */
XIA_MD_STATIC int dxp_md_plx_initialize(unsigned int *maxMod, char *dllname)
{
  UNUSED(maxMod);
  UNUSED(dllname);

  /* All of the other routines check maxMod against MAXMOD, but as near
   * as I can tell it doesn't seem to matter. For now, I will skip this
   * check.
   */
  
  numPLX = 0;

  /*  plx_set_file_DEBUG("plxlib.log"); */

  return DXP_SUCCESS;
}


/** @brief Binds a PXI module to a PLX driver handle.
 *
 * Two unique pieces of information are required to open the PCI device:
 * The slot number and the bus number. These values are typically found
 * in the .INI file that comes with the crate (pxisys.ini). @a ioname encodes
 * these two pieces of information as "[bus]:[slot]".
 */
XIA_MD_STATIC int dxp_md_plx_open(char *ioname, int *camChan)
{
  int i;
  int status;
  int n_scanned  = 0;
  int len_ioname = 0;
  unsigned int temp1, temp2;

  unsigned short id = 0xFFFF;

  byte_t bus;
  byte_t slot;

  
  ASSERT(ioname != NULL);
  ASSERT(camChan != NULL);

  
  /* Search for an existing definition */
  for (i = 0; i < (int)numPLX; i++) {
	if (STREQ(pxiNames[i], ioname)) {
	  *camChan = i;
	  return DXP_SUCCESS;
	}
  }

  /* Parse in and open a new slot */
  n_scanned = sscanf(ioname, "%u:%u", &temp1, &temp2);
  bus = temp1;
  slot = temp2;

  if (n_scanned != 2) {
	sprintf(error_string, "Error parsing ioname = '%s'. The proper format is "
			"'bus:slot'.", ioname);
	dxp_md_log_error("dxp_md_plx_open", error_string, DXP_MDINVALIDNAME);
	return DXP_MDINVALIDNAME;
  }

  *camChan = numPLX++;

  status = plx_open_slot(id, bus, slot, &pxiHandles[*camChan]);

  if (status != PLX_SUCCESS) {
	sprintf(error_string, "Error opening slot '%u' on bus '%u', where the "
			"driver reports a status of %d", slot, bus, status);
	dxp_md_log_error("dxp_md_plx_open", error_string, DXP_MDOPEN);
	return DXP_MDOPEN;
  }
  
  ASSERT(pxiNames[*camChan] == NULL);

  len_ioname = strlen(ioname) + 1;

  pxiNames[*camChan] = (char *)md_md_alloc(len_ioname);

  if (!pxiNames[*camChan]) {
	sprintf(error_string, "Unable to allocate %u bytes for pxiNames[%d]",
			len_ioname, *camChan);
	dxp_md_log_error("dxp_md_plx_open", error_string, DXP_MDNOMEM);
	return DXP_MDNOMEM;
  }

  strncpy(pxiNames[*camChan], ioname, len_ioname);

  numMod++;

  return DXP_SUCCESS;
}


/** @brief Performs the specified I/O operation on the hardware.
 *
 * The PLX driver supports 3 different I/O operations: read/write single words
 * and burst reads. The I/O operation type is controlled via @a function, where
 * 0 corresponds to a single write, 1 corresponds to a single read and 2
 * corresponds to a burst read.
 *
 */
static int dxp_md_plx_io(int *camChan, unsigned int *function,
						 unsigned long *addr, void *data,
						 unsigned int *length)
{
  int status;

  HANDLE h;

  unsigned long *buf = (unsigned long *)data;

  UNUSED(function);
  UNUSED(length);

  
  ASSERT(camChan != NULL);
  ASSERT(data    != NULL);


  h = pxiHandles[*camChan];

  /* How do we check if the handle is valid? These aren't pointers... */

  switch (*function) {
  case 0:
	/* Single word write */
#ifdef XIA_PLX_WRITE_DEBUG
    sprintf(error_string, "XIA_PLX_WRITE_DEBUG: %#x = %#x", *addr, *buf);
    dxp_md_log_debug("dxp_md_plx_io", error_string);
#endif /* XIA_PLX_WRITE_DEBUG */  

    status = plx_write_long(h, *addr, *buf);
	break;

  case 1:
	/* Single word read */
	status = plx_read_long(h, *addr, buf);
	break;

  case 2:
	/* Burst read, normal, 2 dead words */
	status = plx_read_block(h, *addr, *length, 2, buf);
	break;

  default:
	/* This should never occur */
	status = DXP_MDUNKNOWN;
	ASSERT(FALSE_);
  }

  if (status != PLX_SUCCESS) {
	sprintf(error_string, "Unable to perform requested I/O: func = %u, addr = "
			"%#lx, len = %u", *function, *addr, *length);
	dxp_md_log_error("dxp_md_plx_io", error_string, DXP_MDIO);
	return DXP_MDIO;
  }

  return DXP_SUCCESS;
}


/** @brief Closes the port.
 *
 */
static int dxp_md_plx_close(int *camChan)
{
  HANDLE h;

  int status;


  ASSERT(camChan != NULL);


  h = pxiHandles[*camChan];

  if (h) {
	status = plx_close_slot(h);

	if (status != PLX_SUCCESS) {
	  sprintf(error_string, "Error closing PXI slot '%s', handle = %p",
			  pxiNames[*camChan], &h);
	  dxp_md_log_error("dxp_md_plx_close", error_string, DXP_MDCLOSE);
	  return DXP_MDCLOSE;
	}

	ASSERT(pxiNames[*camChan] != NULL);

	md_md_free(pxiNames[*camChan]);
	pxiNames[*camChan] = NULL;

	numPLX--;

	sprintf(error_string, "Closed the PLX HANDLE at camChan = %d", *camChan);
	dxp_md_log_debug("dxp_md_plx_close", error_string);

	/* Is there a way to mark the handle as unused now? */
	pxiHandles[*camChan] = NULL;
  
  }  else {
	sprintf(error_string, "Skipping previously closed camChan = %d", *camChan);
	dxp_md_log_info("dxp_md_plx_close", error_string);
  }

  return DXP_SUCCESS;
}

#endif /* EXCLUDE_PLX */


#ifndef EXCLUDE_USB2

/**
 * @brief Perform any one-time initialization tasks for the USB2 driver.
 */
XIA_MD_STATIC int  dxp_md_usb2_initialize(unsigned int *maxMod,
                                         char *dllname)
{
  UNUSED(maxMod);
  UNUSED(dllname);


  numUSB2 = 0;

  return DXP_SUCCESS;
}


/**
 * @brief Open the requested USB2 device.
 */
XIA_MD_STATIC int  dxp_md_usb2_open(char *ioname, int *camChan)
{
  int i;
  int dev;
  int status;
  int len;


  ASSERT(ioname != NULL);
  ASSERT(camChan != NULL);


  for (i = 0; i < (int)numUSB2; i++) {
    if (STREQ(usb2Names[i], ioname)) {
      *camChan = i;
      return DXP_SUCCESS;
    }
  }

  sscanf(ioname, "%d", &dev);

  *camChan = numUSB2++;

  status = xia_usb2_open(dev, &usb2Handles[*camChan]);

  if (status != XIA_USB2_SUCCESS) {
    sprintf(error_string, "Error opening USB device '%d', where the driver "
            "reports a status of %d", dev, status);
    dxp_md_log_error("dxp_md_usb2_open", error_string, DXP_MDOPEN);
    return DXP_MDOPEN;
  }
  
  ASSERT(usb2Names[*camChan] == NULL);

  len = strlen(ioname) + 1;

  usb2Names[*camChan] = md_md_alloc(len);

  if (usb2Names[*camChan] == NULL) {
    sprintf(error_string, "Unable to allocate %d bytes for usb2Names[%d]",
            len, *camChan);
    dxp_md_log_error("dxp_md_usb2_open", error_string, DXP_MDNOMEM);
    return DXP_MDNOMEM;
  }

  strcpy(usb2Names[*camChan], ioname);

  usb2AddrCache[*camChan] = MD_INVALID_ADDR;

  numMod++;

  return DXP_SUCCESS;
}


/**
 * @brief Wraps both read/write USB2 operations.
 *
 * The I/O type (read/write) is specified in @a function. The allowed types
 * are the same as the USB driver so that we can use USB2 and USB interchangably
 * as communication protocols.
 *
 * 
 */
XIA_MD_STATIC int  dxp_md_usb2_io(int *camChan, unsigned int *function,
                                 unsigned long *addr, void *data,
                                 unsigned int *len)
{
  int status;

  unsigned int i;

  unsigned long cache_addr;

  unsigned short *buf = NULL;
  
  byte_t *byte_buf = NULL;

  unsigned long n_bytes = 0;


  ASSERT(addr != NULL);
  ASSERT(function != NULL);
  ASSERT(data != NULL);
  ASSERT(camChan != NULL);
  ASSERT(len != NULL);


  buf = (unsigned short *)data;

  n_bytes = (unsigned long)(*len) * 2;

  /* Unlike some of our other communication types, we require that the
   * target address be set as a separate operation. This value will be saved
   * until the real I/O is requested. In a perfect world, we wouldn't require
   * this type of operation anymore, but we have to maintain backwards
   * compatibility for products that want to use USB2 but originally used
   * other protocols that did require the address be set separately.
   */
  switch (*addr) {

  case 0:
    if (usb2AddrCache[*camChan] == MD_INVALID_ADDR) {
      sprintf(error_string, "No target address set for camChan %d", *camChan);
      dxp_md_log_error("dxp_md_usb2_io", error_string, DXP_MD_TARGET_ADDR);
      return DXP_MD_TARGET_ADDR;
    }

    cache_addr = (unsigned long)usb2AddrCache[*camChan];

    switch (*function) {
    case MD_IO_READ:
      /* The data comes from the calling routine as an unsigned short, so
       * we need to convert it to a byte array for the USB2 driver.
       */
      byte_buf = md_md_alloc(n_bytes);

      if (byte_buf == NULL) {
        sprintf(error_string, "Error allocating %ld bytes for 'byte_buf' for "
                "camChan %d", n_bytes, *camChan);
        dxp_md_log_error("dxp_md_usb2_io", error_string, DXP_MDNOMEM);
        return DXP_MDNOMEM;
      }

      status = xia_usb2_read(usb2Handles[*camChan], cache_addr, n_bytes,
                             byte_buf);

      if (status != XIA_USB2_SUCCESS) {
        md_md_free(byte_buf);
        sprintf(error_string, "Error reading %lu bytes from %#lx for "
                "camChan %d", n_bytes, cache_addr, *camChan);
        dxp_md_log_error("dxp_md_usb2_io", error_string, DXP_MDIO);
        return DXP_MDIO;
      }

      for (i = 0; i < *len; i++) {
        buf[i] = (unsigned short)(byte_buf[i * 2] |
                                  (byte_buf[(i * 2) + 1] << 8));
      }

      md_md_free(byte_buf);

      break;
      
    case MD_IO_WRITE:
      /* The data comes from the calling routine as an unsigned short, so
       * we need to convert it to a byte array for the USB2 driver.
       */
      byte_buf = md_md_alloc(n_bytes);

      if (byte_buf == NULL) {
        sprintf(error_string, "Error allocating %lu bytes for 'byte_buf' for "
                "camChan %d", n_bytes, *camChan);
        dxp_md_log_error("dxp_md_usb2_io", error_string, DXP_MDNOMEM);
        return DXP_MDNOMEM;
      }

      for (i = 0; i < *len; i++) {
        byte_buf[i * 2]       = (byte_t)(buf[i] & 0xFF);
        byte_buf[(i * 2) + 1] = (byte_t)((buf[i] >> 8) & 0xFF);
      }

      status = xia_usb2_write(usb2Handles[*camChan], cache_addr, n_bytes,
                              byte_buf);

      md_md_free(byte_buf);

      if (status != XIA_USB2_SUCCESS) {
        sprintf(error_string, "Error writing %lu bytes to %#lx for "
                "camChan %d", n_bytes, cache_addr, *camChan);
        dxp_md_log_error("dxp_md_usb2_io", error_string, DXP_MDIO);
        return DXP_MDIO;
      }

      break;

    default:
      ASSERT(FALSE_);
      break;
    }

    break;

  case 1:
    /* Even though we aren't entirely thread-safe yet, it doesn't hurt to
     * store the address as a function of camChan, instead of as a global
     * variable like dxp_md_usb_io().
     */
    usb2AddrCache[*camChan] = *((unsigned long *)data);
    break;

  default:
    ASSERT(FALSE_);
    break;
  }

  return DXP_SUCCESS;
}


/**
 * @brief Closes a device previously opened with dxp_md_usb2_open().
 */
XIA_MD_STATIC int  dxp_md_usb2_close(int *camChan)
{
  int status;

  HANDLE h;


  ASSERT(camChan != NULL);


  h = usb2Handles[*camChan];

  if (h == NULL) {
    sprintf(error_string, "Skipping previously closed camChan = %d", *camChan);
    dxp_md_log_info("dxp_md_usb2_close", error_string);
    return DXP_SUCCESS;
  }

  status = xia_usb2_close(h);

  if (status != XIA_USB2_SUCCESS) {
    sprintf(error_string, "Error closing camChan (%d) with HANDLE = %p",
            *camChan, h);
    dxp_md_log_error("dxp_md_usb2_close", error_string, DXP_MDCLOSE);
    return DXP_MDCLOSE;
  }

  usb2Handles[*camChan] = NULL;

  ASSERT(usb2Names[*camChan] != NULL);

  md_md_free(usb2Names[*camChan]);
  usb2Names[*camChan] = NULL;

  numUSB2--;

  return DXP_SUCCESS;
}


#endif /* EXCLUDE_USB2 */

#ifdef _WIN32
#pragma warning( default : 4127 )
#endif
