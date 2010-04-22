program sncdelay
/* snc test program to demonstrate taskDelay and state transition without
   using external events.
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
	    when(v>5.0)
	    {
		printf("low, v = %g, now changing to high\n",v);
		epicsThreadSleep(2.0);
	    } state high
	    when( v<5.0 )
	    {
		printf("low, v = %g, now transition to low again\n",v);
		epicsThreadSleep(0.1);
		v = 5.1;
	    } state low
	}
	state high
	{
	    when(v<=5.0)
	    {
		printf("high, v = %g, now changing to low\n",v);
	    } state low
	    when(v>5.0)
	    {
		epicsThreadSleep(1.0);
		printf("high, v = %g, now changing to low\n",v);
		v = 4.9;
	    } state low
	}
}
