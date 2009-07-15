/***********************************************************************
 *
 * mar345: mararrays.h
 *
 * Copyright by:        Dr. Claudio Klein
 *                      X-ray Research GmbH, Hamburg
 *
 * Version:     3.0
 * Date:        31/10/2000
 *
 ***********************************************************************/

#define MAX_SIZE 	3450
typedef union {
	unsigned int	i4_img  [MAX_SIZE*MAX_SIZE];
	unsigned short	i2_img  [MAX_SIZE*MAX_SIZE];
} UIMG;

#ifdef MAR_GLOBAL
UIMG			u;
#else
extern UIMG		u;
#endif

