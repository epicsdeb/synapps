/**
 * arcnetlib.h
 *
 * Header file for the arcnetlib.dll source.
 *
 */

#ifndef __ARCNETLIB_H__
#define __ARCNETLIB_H__



#include "Dlldefs.h"

#ifdef __cplusplus
extern "C" {
#endif
  
  XIA_EXPORT int XIA_API dxpInitializeArcnet(unsigned char nodeID);
  XIA_EXPORT int XIA_API dxpReadArcnet(unsigned char nodeID, unsigned short addr, unsigned short *data, unsigned int len);
  XIA_EXPORT int XIA_API dxpWriteArcnet(unsigned char nodeID, unsigned short addr, unsigned short *data, unsigned int len);
  XIA_EXPORT int XIA_API dxpSetAddressArcnet(unsigned char nodeID, unsigned short addr);

#ifdef __cplusplus
}
#endif


#endif /* __ARCNETLIB_H__ */
