#ifndef _boardtypesh
#define _boardtypesh
/*

                          Argonne National Laboratory
                            APS Operations Division
                     Beamline Controls and Data Acquisition

                        Supported Board Types & IO Map


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
    This files defines the supported board types andthe IO map.

 Developer notes:

 Source control info:
    Modified by:    dkline
                    2008/02/12 12:19:32
                    1.17

 =============================================================================
 History:
 Author: David M. Kline
 -----------------------------------------------------------------------------
 2006-Mar-09  DMK  Development started.
 2007-Oct-16  DMK  Removed symbol of unsupported hardware.
 2008-Jan-25  DMK  Added Poseidon hardware support.
 -----------------------------------------------------------------------------

*/


/******************************************************************************
 * Define symbolic constants for board types and hardcoded addresses
 ******************************************************************************/

/* Diamond Systems Corporation */
#define DSC_ATHENA      (1)
#define DSC_RMMSIG      (2)
#define DSC_PMM         (3)
#define DSC_OMM         (4)
#define DSC_RMMDIF      (9)
#define DSC_POSEIDON    (10)

/* Sensoray */
#define SEN_SMARTAD     (100)

/* Prodex OMS motor controller */
#define OMS_PC68        (500)
#define OMS_PC78        (501)

/* Inline methods */
inline static void outport(unsigned char d,unsigned short a) {outb(d,a);}
inline static epicsUInt32 inport(unsigned short a) {epicsUInt32 d=inb((a));return(d);}

#endif /* _boardtypesh */
