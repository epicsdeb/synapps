program sncopttvar

option +r;

/* snc test program to test state option -t, not to reset timers on entry
 * from self, where delay is a variable. Test is that;
 *   i. the variable is re-initialized on the iteration even 
 *      when -t is in effect.
 *   ii.The delay is timed from the states first entry when -t is in effect,
 *      and on each entry when +t is in effect (the default). 
 *   iii. Re-entrant code, variable d is assigned properly in entry{}.
 */

float d; /* Delay variable for managing low state*/
float v; /* Iteration Counter for managing high state*/

ss ss1
{
	state low
	{
  	    option -t; /* -t Don't reset timers on re-entry to state */
	    entry 
	    { 
		d = 4.0;
		printf("Entered low, initialize delay to %g sec.\n",d);
	    }
	    when(d>10.0)
	    {
		printf("low, delay = %g, now changing to high\n",d);
	    } state high
	    when(delay(d)) 
	    {
		printf("low, delay timeout, reenter low\n");
		d += 2.0;
		printf("low, waiting %g secs to iterate\n",d);
	    } state low
	    exit 
	    { 
		printf("low, print this on exit of low\n");
	    }

	}

	state high
	{
	    entry { 
                 printf("\nEntered high\n");
                 v = 3.0;
            }
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

/* Expected Output:
Entered low, initialize delay to 4 sec.
low, delay timeout, reenter low
low, waiting 6 secs to iterate
low, delay timeout, reenter low
low, waiting 8 secs to iterate
low, delay timeout, reenter low
low, waiting 10 secs to iterate
low, delay timeout, reenter low
low, waiting 12 secs to iterate
low, delay = 12, now changing to high
low, print this on exit of low

Entered high
high, delay 3.0 timeout, decr v and re-enter high
v = 2
high, delay 3.0 timeout, decr v and re-enter high
v = 1
high, delay 3.0 timeout, decr v and re-enter high
v = 0
changing to low
Entered low, initialize delay to 4 sec.
...
*/
