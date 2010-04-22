#ifndef mar3xx_pck_h
#define mar3xx_pck_h

#include "types.h"
#ifdef __cplusplus
extern "C" {
#endif

void get_pck (FILE *, INT16 *);
int  put_pck (INT16 *, int, int, int);

#ifdef __cplusplus
}
#endif


#endif /* mar3xx_pck_h */
