/* cygwin include for htonl, etc. */
/* #include <asm/byteorder.h> */

#include <string.h>

#include "writeXDR.h"

#define UNKNOWN_E 0
#define LITTLE_E 1
#define BIG_E 2

int endianUs = UNKNOWN_E;

union {
	int i;
	char a[4];
} endianTest;

int write_XDR_Init() {
	endianTest.i = 1;
	if (endianTest.a[0] == 1)
		endianUs = LITTLE_E;
	else
		endianUs = BIG_E;
	/* printf("endianTest: We're %s endian\n", endianUs==LITTLE_E ? "little" : "big");*/
	return(endianUs);
}

int writeXDR_char(FILE *fd, char *cp) {
	int i = *cp;

	if (!writeXDR_int(fd, &i)) {
		return (0);
	}
	return (1);
}

int writeXDR_short(FILE *fd, short *sp) {
	epicsInt32 l = *sp;
	return (writeXDR_epicsInt32(fd, &l));
}

int writeXDR_int(FILE *fd, int *ip) {
	epicsInt32 l = *ip;
	return (writeXDR_epicsInt32(fd, &l));
}

int writeXDR_long(FILE *fd, long *longp) {
	epicsInt32 l32 = (epicsInt32)*longp;
	return (writeXDR_epicsInt32(fd, &l32));
}

int writeXDR_epicsInt32(FILE *fd, epicsInt32 *lp) {
	union {
		epicsUInt32 l;
		unsigned char c[4];
	} u;

	if (endianUs == LITTLE_E) {
		u.l = (epicsUInt32) *lp;
		u.l = ((u.c[0]<<8 | u.c[1])<<8 | u.c[2])<<8 | u.c[3];
		lp = (epicsInt32 *)&u.l;
	}
	if (fwrite((char *)lp, sizeof(epicsInt32), 1, fd) != 1)
		return (0);
	return (1);
}

int writeXDR_float(FILE *fd, float *fp) {
	return (writeXDR_epicsInt32(fd, (epicsInt32 *)fp));
}


int writeXDR_double(FILE *fd, double *dp) {
	epicsInt32 *lp;

	lp = (epicsInt32 *)dp;
	if (endianUs == LITTLE_E)
		/* #if defined(__CYGWIN32__) || defined(__MINGW32__) */
		return (writeXDR_epicsInt32(fd, lp+1) && writeXDR_epicsInt32(fd, lp));
	else
		return (writeXDR_epicsInt32(fd, lp) && writeXDR_epicsInt32(fd, lp+1));

}


int writeXDR_counted_string(FILE *fd, char **p) {
  int length;
  length = strlen(*p);

  if (!writeXDR_int(fd, &length)) return(0);
  return(length ? writeXDR_string(fd, p, length) : 1);
}


int writeXDR_string(FILE *fd, char **cpp, int maxsize) {
	char *sp = *cpp;  /* sp is the actual string pointer */
	int size;

	size = strlen(sp);
	if (size > maxsize) size = maxsize;
	if (!writeXDR_int(fd, &size)) {
		return(0);
	}

	return (writeXDR_opaque(fd, sp, size));
}


static char zero[4] = { 0, 0, 0, 0 };

int writeXDR_opaque(FILE *fd, char *cp, int cnt) {
	int nPad;

	if (cnt == 0)
		return (1);
	if (cnt < 0)
		return(0);

	if (!writeXDR_bytes(fd, cp, cnt)) {
		return (0);
	}

	nPad = cnt % 4;
	if (nPad == 0) return (1);
	return (writeXDR_bytes(fd, zero, 4 - nPad));
}

int writeXDR_bytes(FILE *fd, char *addr, size_t len) {

	if ((len != 0) && (fwrite(addr, len, 1, fd) != 1))
		return (0);
	return (1);
}

int writeXDR_vector(FILE *fd, char *basep, int nelem, int elemsize, xdrproc_t xdr_elem) {
	int i;
	char *elptr;

	elptr = basep;
	for (i = 0; i < nelem; i++) {
		if (! (*xdr_elem)(fd, elptr)) {
			return(0);
		}
		elptr += elemsize;
	}
	return(1);	
}

long writeXDR_getpos(FILE *fd) {

	return (ftell(fd));
}

int writeXDR_setpos(FILE *fd, long pos) { 

	return ((fseek(fd, pos, 0) < 0) ? 0 : 1);
}
