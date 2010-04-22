program sncentryvar
/* snc test program to test entry and exit condition execution, with CA
 * assignment and control of transition. 
 * In this case, CD01:FD:PERIOD has units "seconds", and lopr = 10, hopr = 360.
 */

float v;
assign v to "CD01:FD:PERIOD";
monitor v;

ss ss1
{
	state low
	{
	    entry 
	    { 
		v = 20.0;
                pvPut(v);
		printf("Init v = %g on first entry to low\n",v);
	    }
	    when(v<11.0)
	    {
                /* If CA sets v to < 11.0, this will cause state trasnition */
		printf("low, v = %g, now changing to high\n",v);
	    } state high
	    when(v>80)
	    {   /* cycle v every (80-20)/2 = 40 seconds. */
              v = 20;
              pvPut(v);
	    } state low
	    when(delay(2.0)) 
	    {
		printf("low, delay timeout, incr v and now reenter low\n");
		v = v+1.0;
                pvPut(v);
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
	    when(v>=20.0)
	    {
		printf("high, v = %g changing to low\n", v);
	    } state low
	    when(delay(3.0))
	    {
		v = 20;
                pvPut(v);
		printf("high, delay timeout, resetting v = %g\n",v);
	    } state high
	}
}

/* Expected Output:
Init v = 20 on first entry to low
low, delay timeout, incr v and now reenter low
v = 21
low, delay timeout, incr v and now reenter low
v = 22
low, delay timeout, incr v and now reenter low
v = 23
low, delay timeout, incr v and now reenter low
v = 24
low, delay timeout, incr v and now reenter low
v = 25
low, delay timeout, incr v and now reenter low
v = 26
low, delay timeout, incr v and now reenter low
v = 27
low, delay timeout, incr v and now reenter low
v = 28
low, delay timeout, incr v and now reenter low
v = 29
low, delay timeout, incr v and now reenter low
v = 30
low, delay timeout, incr v and now reenter low
v = 31
low, delay timeout, incr v and now reenter low
v = 32
low, delay timeout, incr v and now reenter low
v = 33
low, delay timeout, incr v and now reenter low
v = 34
low, delay timeout, incr v and now reenter low
v = 35
low, delay timeout, incr v and now reenter low
v = 36
low, delay timeout, incr v and now reenter low
v = 37
low, delay timeout, incr v and now reenter low
v = 38
low, delay timeout, incr v and now reenter low
v = 39
low, delay timeout, incr v and now reenter low
v = 40
low, delay timeout, incr v and now reenter low
v = 41
low, delay timeout, incr v and now reenter low
v = 42
low, delay timeout, incr v and now reenter low
v = 43
low, delay timeout, incr v and now reenter low
v = 44
low, delay timeout, incr v and now reenter low
v = 45
low, delay timeout, incr v and now reenter low
v = 46
low, delay timeout, incr v and now reenter low
v = 47
low, delay timeout, incr v and now reenter low
v = 48
low, delay timeout, incr v and now reenter low
v = 49
low, delay timeout, incr v and now reenter low
v = 50                        ** User set CD01:FD:PERIOD to 10 **
low, v = 10, now changing to high
low, print this on exit of low

Entered high
high, delay timeout, resetting v = 20
high, v = 20 changing to low
Init v = 20 on first entry to low
low, delay timeout, incr v and now reenter low
v = 21
low, delay timeout, incr v and now reenter low
v = 22
low, delay timeout, incr v and now reenter low
v = 23
low, delay timeout, incr v and now reenter low
v = 24
...
*/
