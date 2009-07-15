/* drvRoper.h
 *
 * This is a driver for a Roper Scientific (Princeton Instruments and Photometrics) area detectors.
 *
 * Author: Mark Rivers
 *         University of Chicago
 *
 * Created:  November 26, 2008
 *
 */

#ifndef DRV_ROPER_H
#define DRV_ROPER_H

#ifdef __cplusplus
extern "C" {
#endif

int roperConfig(const char *portName,
                int maxBuffers, size_t maxMemory);

#ifdef __cplusplus
}
#endif
#endif
