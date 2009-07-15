// omm.h
// Linux Device Driver for the Generic DIO Scaler/w interrupts

#ifndef _OMM_H
#define _OMM_H


/* ioctl commands...
 *
 * R - read value.
 * W - write value.
*/
#define OMM_IOCRBASE       (1)
#define OMM_IOCRMAJOR      (2)
#define OMM_IOCRMINOR      (3)
#define OMM_IOCRNAME       (4)
#define OMM_IOCRDEVCNT     (5)
#define OMM_IOCRMAXDEVCNT  (6)
#define OMM_IOCWAITONINT   (7)

#endif  // _OMM_H
