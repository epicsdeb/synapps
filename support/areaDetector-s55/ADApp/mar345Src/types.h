#ifndef types_h
#define types_h

#if defined(__alpha) 
#define ADDR32	unsigned int
#define CHAR8	char
#define INT8	signed char
#define UINT8	unsigned char
#define INT16	signed short
#define UINT16	unsigned short
#define INT32	signed int
#define UINT32	unsigned int
#define INT64	signed long
#define UINT64	unsigned long
#define FLOAT32	float
#define FLOAT64	double
/*
#elif defined(__hpux)
#define ADDR32	unsigned long
#define CHAR8	char
#define INT8	char
#define UINT8	unsigned char
#define INT16	short
#define UINT16	unsigned short
#define INT32	long
#define UINT32	unsigned long
#define FLOAT32	float
#define FLOAT64	double
*/
#elif defined(__sgi)
#if defined(_MIPS_SIM_ABI64)
#define ADDR32	unsigned int
#define CHAR8	char
#define INT8	signed char
#define UINT8	unsigned char
#define INT16	signed short
#define UINT16	unsigned short
#define INT32	signed int
#define UINT32	unsigned int
#define INT64	long long
#define UINT64	unsigned long long
#define FLOAT32	float
#define FLOAT64	double
#else
#define ADDR32	unsigned long
#define CHAR8	char
#define INT8	signed char
#define UINT8	unsigned char
#define INT16	signed short
#define UINT16	unsigned short
#define INT32	signed long
#define UINT32	unsigned long
#define INT64	long long
#define UINT64	unsigned long long
#define FLOAT32	float
#define FLOAT64	double
#endif
#else
#define ADDR32	unsigned long
#define CHAR8	char
#define INT8	signed char
#define UINT8	unsigned char
#define INT16	signed short
#define UINT16	unsigned short
#define INT32	signed long
#define INT64	long long
#define UINT64	unsigned long long
#define UINT32	unsigned long
#define FLOAT32	float
#define FLOAT64	double
#endif


#endif /* types_h */
