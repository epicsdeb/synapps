/* Test CAMAC 16 bit block mode operations */
#include <vxWorks.h>
#include <camacLib.h>
#define SLOT 14
#define BRANCH 0
#define CRATE 0
#define SIZE 50 
#define SUBADDRESS 0
#define FUNCTION 0
#define MAX_LOOPS 50

test_camac16()
{
   int ext;
   short buff[SIZE];
   int cb[4] = {SIZE,0,0,0};
   int i;
   
   /* QSTP transfers */
   cdreg(&ext, BRANCH, CRATE, SLOT, SUBADDRESS);
   for (i=0; i<MAX_LOOPS; i++) {
      csubc(FUNCTION, ext, buff, cb);
      if (cb[0] != cb[1]) {
         printf("Error - loop=%d, desired transfer size=%d, actual=%d\n",
            i,cb[0],cb[1]);
      }
   }
}
