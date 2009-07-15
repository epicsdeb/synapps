/* File: 	  drvPilatusAsyn.h
 * Author:  Mark Rivers
 * Date:    18-May-2007
 *
 * Purpose: 
 * This module provides the driver support for scaler and mca record asyn device support layers
 * for the Pilatus detector.
 *
 */

#ifndef DRVPILATUSASYN_H
#define DRVPILATUSASYN_H

#define PILATUS_ROI_XMIN_STRING       "ROI_XMIN"       /* int32, write */
#define PILATUS_ROI_XMAX_STRING       "ROI_XMAX"       /* int32, write */
#define PILATUS_ROI_YMIN_STRING       "ROI_YMIN"       /* int32, write */
#define PILATUS_ROI_YMAX_STRING       "ROI_YMAX"       /* int32, write */
#define PILATUS_ROI_BGD_WIDTH_STRING  "ROI_BGD_WIDTH"  /* int32, write */

#define MAX_PILATUS_COMMANDS 5

int drvPilatusAsynConfigure(char  *portName,
                            char  *tcpPortName,
                            int   maxROIs,
                            int   maxChans,
                            int   imageXSize,
                            int   imageYSize,
                            char *dbPrefix);
#endif

