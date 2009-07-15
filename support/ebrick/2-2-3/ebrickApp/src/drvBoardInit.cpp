/*

                          Argonne National Laboratory
                            APS Operations Division
                     Beamline Controls and Data Acquisition

                              EPICS Brick Athena



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
    This module provides method(s) to initialize either the Athena or
    Poseidon board.

        drvBoardInit(addr,type)

        Where:
            addr - Board address
            type - Board type (see boardtypes.h)

 Developer notes:
    1. User manual ICMS documents:
       Athena     - APS_1251097
       Poseidon   - APS_1251696

 Source control info:
    Modified by:    dkline
                    2008/02/12 12:19:32
                    1.4

 =============================================================================
 History:
 Author: David M. Kline
 -----------------------------------------------------------------------------
 2006-May-15  DMK  Derived from drvDac source.
 -----------------------------------------------------------------------------

*/


/* EPICS base version-specific definitions (must be performed first) */
#include <epicsVersion.h>
#define LT_EPICSBASE(v,r,l) (EPICS_VERSION<(v)||(EPICS_VERSION==(v)&&(EPICS_REVISION<(r)||(EPICS_REVISION==(r)&&EPICS_MODIFICATION<(l)))))


/* Evaluate EPICS base */
#if LT_EPICSBASE(3,14,6)
    #error "EPICS base must be 3.14.6 or greater"
#endif

/* System related include files */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <unistd.h>


/* EPICS system related include files */
#include <iocsh.h>
#include <epicsStdio.h>
#include <cantProceed.h>
#include <epicsString.h>
#include <epicsExport.h>
#include <asynUInt32Digital.h>


/* Board definition related include files */
#include "boardtypes.h"


/* Public interface forward references */
static int drvBoardInit(int addr,int type);


/****************************************************************************
 * Define public interface methods
 ****************************************************************************/
static int drvBoardInit(int addr,int type)
{

    switch( type )
    {
    case DSC_ATHENA:
        outport(0x7f,addr);
        break;
    case DSC_POSEIDON:
        outport(0x2,addr+7);
        outport(0x38,addr+8);
        break;

    default:
        printf("drvBoardInit unknown board type %d\n",type);
        return( -1 );
    }

    return( 0 );
}


/****************************************************************************
 * Register public methods
 ****************************************************************************/

/* Initialization method definitions */
static const iocshArg arg0 = {"addr",iocshArgInt};
static const iocshArg arg1 = {"type",iocshArgInt};
static const iocshArg* args[]= {&arg0,&arg1};
static const iocshFuncDef drvBoardInitFuncDef = {"drvBoardInit",2,args};
static void drvBoardInitCallFunc(const iocshArgBuf* args)
{
    drvBoardInit(args[0].ival,args[1].ival);
}

/* Registration method */
static void drvBoardRegister(void)
{
    static int firstTime = 1;

    if( firstTime )
    {
        firstTime = 0;
        iocshRegister( &drvBoardInitFuncDef, drvBoardInitCallFunc );
    }
}
epicsExportRegistrar( drvBoardRegister );
