/* This programs tests an apparent bug in the 2917/3922 hardware when doing a
   QSTP transfer from an empty slot.
*/
#include <vxWorks.h>
#include <camacLib.h>
#define SLOT 13 
#define BRANCH 0
#define CRATE 0
#define SIZE 1
#define MAX_LOOPS 100

test_camac24()
{
   int ext;
   int buff[SIZE];
   int cb[4] = {SIZE,0,0,0};
   int i;
   
   /* QSTP transfers */
   cdreg(&ext, BRANCH, CRATE, SLOT, 0);
   for (i=0; i<MAX_LOOPS; i++) cfubc(0, ext, buff, cb);
}
