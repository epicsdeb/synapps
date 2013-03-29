/*
FILENAME..	devOmsCom.h

USAGE... 	This file contains OMS device information that is
		common to all OMS device support modules.

Version:        $Revision: 10834 $
Modified By:    $Author: sluiter $
Last Modified:  $Date: 2010-04-29 12:04:39 -0500 (Thu, 29 Apr 2010) $
HeadURL:        $URL: https://subversion.xor.aps.anl.gov/synApps/motor/tags/R6-7-1/motorApp/OmsSrc/devOmsCom.h $
*/

/*
 *      Original Author: Ron Sluiter
 *      Date: 12/22/98
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contract
 *      W-31-109-ENG-38 at Argonne National Laboratory.
 *
 *      Beamline Controls & Data Acquisition Group
 *      Experimental Facilities Division
 *      Advanced Photon Source
 *      Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 */

#ifndef	INCdevOmsComh
#define	INCdevOmsComh 1

#include "motordevCom.h"

extern RTN_STATUS oms_build_trans(motor_cmnd, double *, struct motorRecord *);

#endif	/* INCdevOmsComh */
