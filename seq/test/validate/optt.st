/*************************************************************************\
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
program opttTest
/* snc test program to test state option -t, not to reset timers.
 */

%%#include "epicsThread.h"

float v;
/* assign v to "{user}:xxxExample";
monitor v;
*/
ss ss1
{
	state low
	{
  	    option -t; /* Don't reset timers on re-entry to state */
	    entry 
	    { 
		printf("Print something on entry to low\n");
		printf("Also pause on entry to low\n");
		epicsThreadSleep(1.0);
	    }
	    when(v>5.0)
	    {
		printf("low, v = %g, now changing to high\n",v);
	    } state high
	    when(delay(2.0)) 
	    {
		printf("low, delay timeout, incr v and now reenter low\n");
		v = v+1.0;
		printf("v = %g\n",v); 
	    } state low
	    exit 
	    { 
		printf("low, print this on exit of low\n");
	    }

	}

	state high
	{
	    entry { printf("\nEntered high\n"); }
	    when(v==0)
	    {
		printf("changing to low\n");
	    } state low
	    when(delay(3.0))
	    {
		printf("high, delay 3.0 timeout, decr v and re-enter high\n");
		v = v-1.0;
		printf("v = %g\n",v); 
	    } state high
	}
}
