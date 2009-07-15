// oms.h
// Linux Device Driver for the OMS PC68/78 PC104-based motion controllers

#ifndef _OMS_H
#define _OMS_H


/* ioctl commands...
 *
 * R - read value.
 * W - write value.
*/
#define OMS_IOCRBASE       (1)
#define OMS_IOCRMAJOR      (2)
#define OMS_IOCRMINOR      (3)
#define OMS_IOCRNAME       (4)
#define OMS_IOCRDEVCNT     (5)
#define OMS_IOCRMAXDEVCNT  (6)
#define OMS_IOCWAITONINT   (7)

#endif  // _OMS_H
