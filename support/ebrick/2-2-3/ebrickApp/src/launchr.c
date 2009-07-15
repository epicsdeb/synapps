/*

                          Argonne National Laboratory
                            APS Operations Division
                     Beamline Controls and Data Acquisition

                    Support for Diamond Systems Incorporated
                      Athena Application Lanch Mechanism



 -----------------------------------------------------------------------------
                                COPYRIGHT NOTICE
 -----------------------------------------------------------------------------
   Copyright (c) 2002 The University of Chicago, as Operator of Argonne
      National Laboratory.
   Copyright (c) 2002 The Regents of the University of California, as
      Operator of Los Alamos National Laboratory.
   Synapps Versions 4-5
   and higher are distributed subject to a Software License Agreement found
   in file LICENSE that is included with this distribution.
 -----------------------------------------------------------------------------

 Description
    This module provides the mechanism necessary to launch the EPICS
    application. It opens all IO ports before launching the specified
    application and leaves the application as root. This was derived
    from launch.c when libnet/libpcap is being used. A sysadmin MUST
    be involved to allow this application to run.

 Developer notes:
    1. Launchr must be made setuid-root using the following commands.
       In order to do so, one must be logged in as root:

       chown root:root launchr
       chmod 4511 launchr
    2. Returns the following values:
       - If successful, it should never return.
       - -1, invalid arguments.
       - -2, failure to open ports.
       - -3, failure to launch application


 Source control info:
    Modified by:    dkline
                    2008/04/05 12:51:03
                    1.2

 =============================================================================
 History:
 Author: David M. Kline
 -----------------------------------------------------------------------------
 2005-Nov-16  DMK  Development started (taken from the Prometheus development).
 2006-May-12  DMK  Open up all 512 ports from 0x200..0x3FF.
 2006-Aug-18  DMK  Employ iopl() instead of ioperm().
 2007-Jan-21  DMK  Taken from launch.c.
 -----------------------------------------------------------------------------

*/


/* System related include files */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/io.h>


/* Define primary entry point */
int main(int argc,char** argv)
{
    /* Validate application usage */
    if(argc < 2)
    {
        printf("Usage: %s executable [args ...]\n",argv[0]);
        return( -1 );
    }

    /* Open all ports */
    if(iopl(3))
    {
        printf("launchr: Can't open IO ports: \"%s\"\n",strerror(errno));
        return( -2 );
    }

    /* Launch the application (leave as root) */
    argv++;
    execv(argv[0],argv);
    printf("launchr: Can't execute %s: %s\n",argv[0],strerror(errno));
    return( -3 );
}
