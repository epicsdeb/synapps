/***********************************************************************
 *
 * mar345: pck.c
 *
 * Copyright by:        Dr. Claudio Klein
 *                      X-ray Research GmbH, Hamburg
 *
 * Version:     1.0
 * Date:        16/01/1997
 *
 * 		3/29/00 - mb - removed external netcontrol/status stuff
 * 		12/21/00- mb - added my word size headers for portability
 *
 ***********************************************************************/

#include <stdio.h>
#include <stddef.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include "types.h"
#include "mar3xx_pck.h"

#define PACKIDENTIFIER "\nCCP4 packed image, X: %04d, Y: %04d\n"
#define PACKBUFSIZ BUFSIZ
#define DIFFBUFSIZ 16384
#define max(x, y) (((x) > (y)) ? (x) : (y)) 
#define min(x, y) (((x) < (y)) ? (x) : (y)) 
#define abs(x) (((x) < 0) ? (-(x)) : (x))
const INT32 setbits[33] = {0x00000000, 0x00000001, 0x00000003, 0x00000007,
			  0x0000000F, 0x0000001F, 0x0000003F, 0x0000007F,
			  0x000000FF, 0x000001FF, 0x000003FF, 0x000007FF,
			  0x00000FFF, 0x00001FFF, 0x00003FFF, 0x00007FFF,
			  0x0000FFFF, 0x0001FFFF, 0x0003FFFF, 0x0007FFFF,
			  0x000FFFFF, 0x001FFFFF, 0x003FFFFF, 0x007FFFFF,
			  0x00FFFFFF, 0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF,
			  0x0FFFFFFF, 0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF,
                          0xFFFFFFFF};
#define shift_left(x, n)  (((x) & setbits[32 - (n)]) << (n))
#define shift_right(x, n) (((x) >> (n)) & setbits[32 - (n)])

/***************************************************************************/

/*
 * Function prototypes
 */

static INT32 	*diff_words	(INT16 *, int, int, INT32 *, INT32	);
static int  	pack_chunk	(				);
static void 	unpack_word	(FILE *, int, int, INT16 *	);
static void	pack_longs	(				);
static int	bits		(				);

/***************************************************************************
 * Function: Put_pck
 ***************************************************************************/
int  
put_pck(INT16 *img, int x, int y, int fdesc) 
{ 
int 		chunksiz, packsiz, nbits, next_nbits, tot_nbits;
INT32 		buffer[DIFFBUFSIZ];
INT32 		*diffs = buffer;
INT32 		*end = diffs - 1;
INT32 		done = 0;

	while(done < (x * y)) {
	  
	    end = diff_words(img, x, y, buffer, done);
	    done += (end - buffer) + 1;

	    diffs = buffer;
	    while(diffs <= end) {
	      
	        packsiz = 0;
	        chunksiz = 1;
	        nbits = bits(diffs, 1);
	        while(packsiz == 0) {
		  
		    if(end <= (diffs + chunksiz * 2))
			packsiz = chunksiz;
		    else {
		
			  next_nbits = bits(diffs + chunksiz, chunksiz); 
			  tot_nbits = 2 * max(nbits, next_nbits);

			  if(tot_nbits >= (nbits + next_nbits + 6))
			      packsiz = chunksiz;
			  else {
			      
				nbits = tot_nbits;
				if(chunksiz == 64)
				    packsiz = 128;
				  else
				    chunksiz *= 2;
			  }

		    }
		}

		if ( pack_chunk(diffs, packsiz, nbits / packsiz, fdesc) == 0)
			return( 0 );
		diffs += packsiz;
	     }
	}
	if ( pack_chunk(NULL, 0, 0, fdesc) == 0 );
		return( 1 );

	return( 1 );
}

/***************************************************************************
 * Function: bits
 ***************************************************************************/
static int 
bits(INT32 *chunk, int n)
{ 
  int size, maxsize, i;

  for (i = 1, maxsize = abs(chunk[0]); i < n; ++i)
    maxsize = max(maxsize, abs(chunk[i]));
  if (maxsize == 0)
    size = 0;
  else if (maxsize < 8)
    size = 4 * n;
  else if (maxsize < 16)
    size = 5 * n;
  else if (maxsize < 32)
    size = 6 * n;
  else if (maxsize < 64)
    size = 7 * n;
  else if (maxsize < 128)
    size = 8 * n;
  else if (maxsize < 65536)
    size = 16 * n;
  else
    size = 32 * n;
  return(size);
}

/***************************************************************************
 * Function: pack_chunk
 ***************************************************************************/
static int
pack_chunk(INT32 *lng, int nmbr, int bitsize, int fdesc)
{ 
static INT32 	bitsize_encode[33] = {0, 0, 0, 0, 1, 2, 3, 4, 5, 0, 0,
                                      0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0,
                                      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7};
INT32 		descriptor[2], i, j;
static CHAR8 	*buffer = NULL;
static CHAR8 	*buffree = NULL;
static int 	bitmark;

	if(buffer == NULL) {
	    buffree = buffer = (CHAR8 *) malloc(PACKBUFSIZ);
	    bitmark = 0;
	}

	if(lng != NULL) {
	    for (i = nmbr, j = 0; i > 1; i /= 2, ++j);
	    descriptor[0] = j;
	    descriptor[1] = bitsize_encode[bitsize];
	    if((buffree - buffer) > (PACKBUFSIZ - (130 * 4))) {
		if( write(fdesc, buffer,buffree - buffer) == -1 )
			return( 0 );
		buffer[0] = buffree[0];
		buffree = buffer;
	    }
	    pack_longs(descriptor, 2, &buffree, &bitmark, 3);
	    pack_longs(lng, nmbr, &buffree, &bitmark, bitsize);
	 }
	 else {
	    if( write(fdesc,buffer,(buffree - buffer) + 1) == -1 )
		return( 0 );
	    free((void *) buffer);
	    buffer = NULL;
	 }

	return( 1 );
}

/***************************************************************************
 * Function: diff_words
 ***************************************************************************/
INT32 
*diff_words(INT16 *word, int x, int y, INT32 *diffs, INT32 done)
{ 
INT32 i = 0;
INT32 tot = x * y;

	if(done == 0)
	  { 
	    *diffs = word[0];
	    ++diffs;
	    ++done;
	    ++i;
	  }
	while((done <= x) && (i < DIFFBUFSIZ))
	  {
	    *diffs = word[done] - word[done - 1];
	    ++diffs;
	    ++done;
	    ++i;
	  }
	while ((done < tot) && (i < DIFFBUFSIZ))
	  {
	    *diffs = word[done] - (word[done - 1] + word[done - x + 1] +
                     word[done - x] + word[done - x - 1] + 2) / 4;
	    ++diffs;
	    ++done;
	    ++i;
	  }
	return(--diffs);
}

/***************************************************************************
 * Function: pack_longs
 ***************************************************************************/
static void
pack_longs(INT32 *lng, int n, CHAR8 **target, int *bit, int size)
  { 
	INT32 mask, window;
	int valids, i, temp;
	int temp_bit = *bit;
	CHAR8 *temp_target = *target;

	if (size > 0)
	  {
	    mask = setbits[size];
	    for(i = 0; i < n; ++i)
	      {
		window = lng[i] & mask;
		valids = size;
		if(temp_bit == 0)
			*temp_target = (CHAR8) window;
		  else
		    {
		      temp = shift_left(window, temp_bit);
        	      *temp_target |= temp;
		    }
		 window = shift_right(window, 8 - temp_bit);
		valids = valids - (8 - temp_bit);
		if(valids < 0)
		    temp_bit += size;
		  else
		    {
		      while (valids > 0)
			{ 
			  *++temp_target = (CHAR8) window;
          		  window = shift_right(window, 8);
          		  valids -= 8;
			}
        	      temp_bit = 8 + valids;
		    }
      		if(valids == 0)
      		  { 
		    temp_bit = 0;
        	    ++temp_target;
		  }
	      }
  	    *target = temp_target;
  	    *bit = (*bit + (size * n)) % 8;
	  }
}

/***************************************************************************
 * Function: get_pck
 ***************************************************************************/
void 
get_pck(FILE *fp, INT16 *img)
{ 
int x = 0, y = 0, i = 0, c = 0;
char header[BUFSIZ];

	if ( fp == NULL ) return;
	rewind ( fp );
	header[0] = '\n';
    	header[1] = 0;

    	while ((c != EOF) && ((x == 0) || (y == 0))) {
    		c = i = x = y = 0;

      		while ((++i < BUFSIZ) && (c != EOF) && (c != '\n') && (x==0) && (y==0))
        		if ((header[i] = c = getc(fp)) == '\n')
          			sscanf(header, PACKIDENTIFIER, &x, &y);
	}

    	unpack_word(fp, x, y, img);
}

/***************************************************************************
 * Function: unpack_word
 ***************************************************************************/
static void 
unpack_word(FILE *packfile, int x, int y, INT16 *img)
{
int 		valids = 0, spillbits = 0, usedbits, total = x * y;
INT32 		window = 0, spill = 0, pixel = 0, nextint, bitnum, pixnum;
static int 	bitdecode[8] = {0, 4, 5, 6, 7, 8, 16, 32};

    while (pixel < total) {
      	if (valids < 6) {
          if (spillbits > 0) {
      		window |= shift_left(spill, valids);
        	valids += spillbits;
        	spillbits = 0;
	}
        else {
      		spill = (INT32) getc(packfile);
        	spillbits = 8;
	}
    }
    else {
    	pixnum = 1 << (window & setbits[3]);
      	window = shift_right(window, 3);
      	bitnum = bitdecode[window & setbits[3]];
      	window = shift_right(window, 3);
      	valids -= 6;
      	while ((pixnum > 0) && (pixel < total)) {
      		if (valids < bitnum) {
        		if (spillbits > 0) {
          			window |= shift_left(spill, valids);
            			if ((32 - valids) > spillbits) {
            				valids += spillbits;
              				spillbits = 0;
				}
            			else {
            				usedbits = 32 - valids;
              				spill = shift_right(spill, usedbits);
              				spillbits -= usedbits;
              				valids = 32;
				}
			}
          		else {
          			spill = (INT32) getc(packfile);
            			spillbits = 8;
			}
		}
        	else {
			--pixnum;
          		if (bitnum == 0) 
				nextint = 0;
          		else {
          			nextint = window & setbits[bitnum];
            			valids -= bitnum;
            			window = shift_right(window, bitnum);
            			if ((nextint & (1 << (bitnum - 1))) != 0)
              				nextint |= ~setbits[bitnum];}
          			if (pixel > x) {
         				img[pixel] = (INT16) (nextint +
                                      		(img[pixel-1] + img[pixel-x+1] +
                                       		img[pixel-x] + img[pixel-x-1] + 2) / 4);
            				++pixel;
				}
          			else if (pixel != 0) {
          				img[pixel] = (INT16) (img[pixel - 1] + nextint);
            				++pixel;
				}
          			else
            				img[pixel++] = (INT16) nextint;
			}
		}
	}
   }
}
