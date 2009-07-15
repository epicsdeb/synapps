/* drvPilatusDetector.h
 *
 * This is a driver for a Pilatus detector.
 *
 * Author: Mark Rivers
 *         University of Chicago
 *
 * Created:  June 10, 2008
 *
 */

#ifndef DRV_PILATUSDETECTOR_H
#define DRV_PILATUSDETECTOR_H

#ifdef __cplusplus
extern "C" {
#endif

int pilatusDetectorConfig(const char *portName, const char *camserverPort,
                          int maxSizeX, int maxSizeY,
                          int maxBuffers, size_t maxMemory);

#ifdef __cplusplus
}
#endif
#endif
