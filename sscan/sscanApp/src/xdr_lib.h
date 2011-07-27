
#ifndef __XDR_LIB_H__
#define __XDR_LIB_H__


#include <rpc/rpc.h>

#ifdef vxWorks
struct complex {
  float r;
  float i;
};

bool_t xdr_complex(XDR* xdrs, struct complex *p);
#endif

bool_t xdr_counted_string(XDR* xdrs, char** p);


#endif
