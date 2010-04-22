/* drvPVCam.h
 *
 * This is a driver for a PVCam (PI/Acton) detector.
 *
 * Author: Brian Tieman
 *
 * Created:  06/14/2009
 *
 */

#ifndef DRV_PVCAM_H
#define DRV_PVCAM_H

#ifdef __cplusplus
extern "C" {
#endif

int pvCamConfig(const char *portName, int maxSizeX, int maxSizeY, int dataType,
                      int maxBuffers, size_t maxMemory, int priority, int stackSize);

#ifdef __cplusplus
}
#endif
#endif
