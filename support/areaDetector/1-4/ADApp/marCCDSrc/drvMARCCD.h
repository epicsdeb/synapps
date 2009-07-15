/* drvMARCCD.h
 *
 * This is a driver for a MAR CCD detector.
 *
 * Author: Mark Rivers
 *         University of Chicago
 *
 * Created:  October 26, 2008
 *
 */

#ifndef DRV_MARCCD_H
#define DRV_MARCCD_H

#ifdef __cplusplus
extern "C" {
#endif

int marCCDConfig(const char *portName, const char *marCCDPort,
                 int maxBuffers, size_t maxMemory);

#ifdef __cplusplus
}
#endif
#endif
