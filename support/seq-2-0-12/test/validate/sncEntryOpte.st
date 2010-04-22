program sncentryopte
/* snc test program to test entry and exit condition execution, with -e
 * option. Test -e, that entry will be executed on each iteration.
 */

%%#include "epicsThread.h"

float v;

ss ss1
{
	state low
	{
	    option -e;
	    entry 
	    { 
		printf("Pause on each entry to low, including 'iterations'\n");
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
	    entry { printf("\nentered high\n"); }
	    when(v==0.0)
	    {
		printf("changing to low\n");
	    } state low
	    when(delay(3.0))
	    {
		printf("high, delay 3.0 timeout, resetting v\n");
		v = 0;
	    } state high
	}
}

/* Expected output
Pause on each entry to low, including 'iterations'
low, delay timeout, incr v and now reenter low
v = 1
Pause on each entry to low, including 'iterations'
low, delay timeout, incr v and now reenter low
v = 2
Pause on each entry to low, including 'iterations'
low, delay timeout, incr v and now reenter low
v = 3
Pause on each entry to low, including 'iterations'
low, delay timeout, incr v and now reenter low
v = 4
Pause on each entry to low, including 'iterations'
low, delay timeout, incr v and now reenter low
v = 5
Pause on each entry to low, including 'iterations'
low, delay timeout, incr v and now reenter low
v = 6
Pause on each entry to low, including 'iterations'
low, v = 6, now changing to high
low, print this on exit of low

entered high
high, delay 3.0 timeout, resetting v
changing to low
Pause on each entry to low, including 'iterations'
low, delay timeout, incr v and now reenter low
v = 1
Pause on each entry to low, including 'iterations'
low, delay timeout, incr v and now reenter low
v = 2
...*/
