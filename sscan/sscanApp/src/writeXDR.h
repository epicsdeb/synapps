#include <sys/types.h>
#include <stdio.h>
#include <epicsTypes.h>

typedef int (*xdrproc_t)();

extern int write_XDR_Init();
extern int writeXDR_char(FILE *fd, char *cp);
extern int writeXDR_short(FILE *fd, short *sp);
extern int writeXDR_int(FILE *fd, int *ip);
extern int writeXDR_long(FILE *fd, long *lp);
extern int writeXDR_epicsInt32(FILE *fd, epicsInt32 *lp);
extern int writeXDR_float(FILE *fd, float *fp);
extern int writeXDR_double(FILE *fd, double *dp);
extern int writeXDR_counted_string(FILE *fd, char **p);
extern int writeXDR_string(FILE *fd, char **cpp, int maxsize);
extern int writeXDR_opaque(FILE *fd, char *cp, int cnt);
extern int writeXDR_bytes(FILE *fd, void *addr, size_t len);
extern int writeXDR_vector(FILE *fd, char *basep, int nelem, int elemsize, xdrproc_t xdr_elem);
extern long writeXDR_getpos(FILE *fd);
extern int writeXDR_setpos(FILE *fd, long pos); 
