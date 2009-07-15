/* This programs tests an apparent bug in the 2917/3922 hardware when doing a
   QSTP transfer from an empty slot.
*/
#include <vxWorks.h>
#include <camacLib.h>
#define FULL_SLOT 4  /* This slot has a module in it */
#define EMPTY_SLOT 1   /* This slot is empty */
#define BRANCH 0
#define CRATE 0
#define SIZE 1024
#define MAX_LOOPS 10

test_qstp()
{
   int ext;
   int buff[SIZE];
   int cb[4] = {SIZE,0,0,0};
   int i;
   
   
   /* QRPT transfers to a full slot */
   cdreg(&ext, BRANCH, CRATE, FULL_SLOT, 0);
   printf("QRPT to full slot\n");
   for (i=0; i<MAX_LOOPS; i++) cfubr(0, ext, buff, cb);

   /* QSTP transfers to a full slot */
   cdreg(&ext, BRANCH, CRATE, FULL_SLOT, 0);
   printf("QSTP to full slot\n");
   for (i=0; i<MAX_LOOPS; i++) cfubc(0, ext, buff, cb);

   /* QRPT transfers to an empty slot */
   cdreg(&ext, BRANCH, CRATE, EMPTY_SLOT, 0);
   printf("QRPT to empty slot\n");
   for (i=0; i<MAX_LOOPS; i++) cfubr(0, ext, buff, cb);

   /* QSTP transfers to an empty slot */
   cdreg(&ext, BRANCH, CRATE, EMPTY_SLOT, 0);
   printf("QSTP to empty slot\n");
   for (i=0; i<MAX_LOOPS; i++) cfubc(0, ext, buff, cb);

}
