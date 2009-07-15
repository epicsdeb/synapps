/*
**
**                   Copyright 1996
**                         by
**            The Board of Trustees of the
**          Leland Stanford Junior University.
**               All rights reserved.
**
**
**         Work supported by the U.S. Department
**         of Energy under contract DE-AC03-76SF00515.
**
**                  Disclaimer Notice
**
**   The items furnished herewith were developed
**   under the sponsorship of the U.S. Government.
**   Neither the U.S., nor the U.S. D.O.E., nor the
**   Leland Stanford Junior University, nor their
**   employees, makes any warranty, express or implied,
**   or assumes any liability or responsibility for
**   accuracy, completeness or usefulness of any
**   information, apparatus,product or process disclosed,
**   or represents that its use will not infringe privately
**   owned rights.  Mention of any product, its manufacturer,
**   or suppliers shall not, nor is it intended to, imply
**   approval, disapproval, or fitness for any particular
**   use.  The U.S. and the University at all times retain
**   the right to use and disseminate the furnished items
**   for any purpose whatsoever.
**
**   Notice 91 02 01
*/

/*===========================================================

  Abs:  Functions for system monitoring and control.

  Name: reboot.c
           rebootInit     - general initialization routine
           rebootProc     - IOC Reboot

  Proto: not required and not used.  All functions called
         by the subroutine record get passed one argument:

         psub                       Pointer to the subroutine
          Use:  pointer             record data.  In general,
          Type: struct subRecord *  all functions read the
          Acc:  read/write          input fields (a to l) and
          Mech: reference           write to the val field.

         All functions return a long integer.  0 = OK, -1 = ERROR.
         The subroutine record ignores the status returned by the
         Init routines.  For the calculation routines, the record
         status (STAT) is set to SOFT_ALARM (unless it is already
         set to LINK_ALARM due to severity maximization) and the
         severity (SEVR) is set to psub->brsv (BRSV - set by the
         user in the database (normally set to invalid)).

  Side:  None.

  Auth: 16-Jan-1997, Stephanie Allison
  Rev:  DD-MMM-YYYY, Reviewer's Name (.NE. Author's Name)

-------------------------------------------------------------

  Mod:
        dd-mmm-yyyy, First Lastname (USERNAME):
           Comments

=============================================================*/

/*
 * Include Files
 */
#include   <vxWorks.h>    /* OK                      */
#include   <logLib.h>     /* for logMsg              */
#include   <rebootLib.h>  /* reboot()prototype       */
#include   <taskLib.h>    /* taskDelay()prototype    */
#include   <epicsPrint.h> /* epicsPrintf() prototype */
#include   <subRecord.h>  /* struct subRecord        */

#include "dbDefs.h"
#include "registryFunction.h"
#include "epicsExport.h"

static long rebootInit(struct subRecord *psub);
static long rebootProc(struct subRecord *psub);

static registryFunctionRef rebootSubRef[] = {
   {"rebootInit", (REGISTRYFUNCTION)rebootInit},
   {"rebootProc", (REGISTRYFUNCTION)rebootProc}
};

void registerReboot(void) { registryFunctionRefAdd(rebootSubRef,2); }

epicsExportRegistrar(registerReboot);

/*====================================================

  Abs:  Subroutine record initialization

  Name: rebootInit

  Args: psub                       Subroutine record info
          Use:  pointer
          Type: struct subRecord *
          Acc:  read/write
          Mech: reference

  Rem: General purpose initialization required since all
       subroutine records require a non-NULL init routine
       even if no initialization is required.  Note that most
       subroutines in this file use this routine as an init
       routine.  If init logic is needed for a specific
       subroutine, create a new routine for it - don't modify
       this one.

  Side: None

  Ret: long
            OK - Successful operation (Always)

=======================================================*/
static long rebootInit(struct subRecord *psub)
{
  return(OK);
}




/*====================================================

  Abs:  Subroutine record to reboot IOC

  Name: rebootProc

  Args: psub                       Subroutine record info
          Use:  pointer
          Type: struct subRecord *
          Acc:  read/write
          Mech: reference

  Rem:  This function resets the network
        devices and transfers control to
        boot ROMs.

        If any input A through F is greater
        than zero, the reboot is not allowed;
	these are "inhibits."  Unless input L
	is equal to one, the reboot is not allowed;
	this is an "enable."  The intention is to
	feed a BO record with a one-shot timing of
	a few seconds to it, which has to be set
	within a small window before requesting the
	reboot.
	
        Input G is the bitmask for the reboot
        input argument.  The possible bits are
        defined in sysLib.h.  If input G is
        0 (default), the reboot will be normal
        with countdown.  If the BOOT_CLEAR bit
        is set, the memory will be cleared first.

        A taskDelay is needed before the reboot
        to allow the reboot message to be logged.

  Side: Memory is cleared if BOOT_CLEAR is set.
        A reboot is initiated.
        A message is sent to the error log.

  Ret: long
           OK - Successful operation (Always)

=======================================================*/
static long rebootProc(struct subRecord *psub)
{
  if ((psub->a < 0.5) && (psub->b < 0.5) &&
      (psub->c < 0.5) && (psub->d < 0.5) &&
      (psub->e < 0.5) && (psub->f < 0.5) &&
      (psub->l > 0.5))
  {
     logMsg("IOC reboot started\n",0,0,0,0,0,0);
     taskDelay(120);
     reboot((int)(psub->g + 0.1));
  }
  else
  {
     logMsg("Reboot not allowed at this time from EPICS\n",0,0,0,0,0,0);
  }
  return(OK);
}
