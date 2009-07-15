/* drvAdsc.h
 *
 * This is a driver for ADSC detectors (Q4, Q4r, Q210, Q210r, Q270, Q315,
 * Q315r).
 *
 * Author: J. Lewis Muir (based on Prosilica driver by Mark Rivers)
 *         University of Chicago
 *
 * Created: April 11, 2008
 *
 */

#ifndef DRV_ADSC_H
#define DRV_ADSC_H

#ifdef __cplusplus
extern "C" {
#endif

int adscConfig(const char *portName, const char *modelName);

#ifdef __cplusplus
}
#endif
#endif
