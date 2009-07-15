program sncexitoptx
/* snc test program to test entry and exit condition execution, with -e
 * option. Test -e, that entry will be executed on each iteration.
 */

%%#include "epicsThread.h"

float v;

ss ss1
{
	state low
	{
	    option -x;
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
		printf("Pause on each exit of low, including 'iterations'\n");
		epicsThreadSleep(1.0);
	    }

	}

	state high
	{
	    entry { printf("\nEntered high\n"); }
	    when(v==0.0)
	    {
		printf("changing to low\n");
	    } state low
	    when(delay(3.0))
	    {
		printf("high, delay 3.0 timeout, resetting v\n");
		v = 0;
	    } state high
	    exit 
	    {
		printf("Exit high only on last iteration\n");
	    }
	}
}

/* Expected output
low, delay timeout, incr v and now reenter low
v = 1
Pause on each exit of low, including 'iterations'
low, delay timeout, incr v and now reenter low
v = 2
Pause on each exit of low, including 'iterations'
low, delay timeout, incr v and now reenter low
v = 3
Pause on each exit of low, including 'iterations'
low, delay timeout, incr v and now reenter low
v = 4
Pause on each exit of low, including 'iterations'
low, delay timeout, incr v and now reenter low
v = 5
Pause on each exit of low, including 'iterations'
low, delay timeout, incr v and now reenter low
v = 6
Pause on each exit of low, including 'iterations'
low, v = 6, now changing to high
Pause on each exit of low, including 'iterations'

Entered high
high, delay 3.0 timeout, resetting v
changing to low
Exit high only on last iteration
low, delay timeout, incr v and now reenter low
v = 1
Pause on each exit of low, including 'iterations'
low, delay timeout, incr v and now reenter low
v = 2
Pause on each exit of low, including 'iterations'
low, delay timeout, incr v and now reenter low
v = 3
...
*/
