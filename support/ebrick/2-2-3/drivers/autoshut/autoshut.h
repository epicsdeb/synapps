// autoshut.h
// Linux Device Driver for the EPICS brick auto shutdown

#ifndef _AS_H
#define _AS_H


/* ioctl commands...
 *
 * R - read value.
 * W - write value.
*/
#define AS_IOCRBASE       (1)
#define AS_IOCRMAJOR      (2)
#define AS_IOCRMINOR      (3)
#define AS_IOCRNAME       (4)
#define AS_IOCRDEVCNT     (5)
#define AS_IOCRMAXDEVCNT  (6)
#define AS_IOCWAITONINT   (7)

#endif  // _AS_H
