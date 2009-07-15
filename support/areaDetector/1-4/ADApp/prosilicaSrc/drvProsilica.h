/* drvProsilica.h
 *
 * This is a driver for Prosilica cameras (GigE and CameraLink).
 *
 * Author: Mark Rivers
 *         University of Chicago
 *
 * Created:  March 20, 2008
 *
 */
#ifndef DRV_PROSILICA_H
#define DRV_PROSILICA_H

#ifdef __cplusplus
extern "C" {
#endif

int prosilicaConfig(char *portName,       /* Camera number */
                    int uniqueId,        /* Unique ID number of the camera to use */
                    int maxBuffers,
                    size_t maxMemory);  

#ifdef __cplusplus
}
#endif
#endif
