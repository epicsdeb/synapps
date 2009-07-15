/***********************************************************************
 * 
 * swap.c
 *
 * Copyright by:	Dr. Claudio Klein
 * 			X-ray Research GmbH, Hamburg
 *
 * Version: 	2.0
 * Date:	08/07/1996
 *
 **********************************************************************/

void swaplong();
void swapshort();

#if ( SGI || sgi || LINUX || linux )
void swapvms();

/******************************************************************
 * Function: swapvms = swaps bytes of floats in image header (SGI only)
 ******************************************************************/
void
swapvms( data, n )
float *data;
int n;
{
	int i;
	float d;

	for (i=0; i<n; i++ ) {
                swab( &data[i], &data[i], 4 );
#if ( SGI || sgi || LINUX || linux )
                data[i]/=4.;
#else
                sprintf( str, "%1.6f\0",data[i] );
                d = atof( str );
                data[i]=d/4.;
#endif
        }
}
#endif

#ifdef SWAP
/******************************************************************
 * Function: swaplong = swaps bytes of 32 bit integers
 ******************************************************************/
void
swaplong(data, nbytes)
register char *data;
int nbytes;
{
        register int i, t1, t2, t3, t4;

        for(i=nbytes/4;i--;) {
                t1 = data[i*4+3];
                t2 = data[i*4+2];
                t3 = data[i*4+1];
                t4 = data[i*4+0];
                data[i*4+0] = t1;
                data[i*4+1] = t2;
                data[i*4+2] = t3;
                data[i*4+3] = t4;
        }
}

/******************************************************************
 * Function: swapshort = swaps the two 8-bit halves of a 16-bit word
 ******************************************************************/
void
swapshort(data, n)
register unsigned short *data;
int n;
{
	register int i;
	register unsigned short t;

	for(i=(n>>1)+1;--i;) {
		/*t = (( (*data)&0xFF) << 8 ) | (( (*data)&0xFF00) >> 8);*/
		t = (((*data) << 8) | ((*data) >> 8));
		*data++ = t;
	}
}
#endif
