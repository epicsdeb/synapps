#include	<stdio.h>
#include	<descrip.h>
#include	<ssdef.h>

/*
 *	Module to handle the simple alarm timer and signal processing
 *	for the mardc data collection module.  Included in this module
 *	is the generic set_timer_params to set up system specific clock
 *	parameters for the given heartbeat grain, and a generic start_timer
 *	routine which takes the set up parameter and initiates the clock.
 *
 *	This version uses standard VMS SETIMER system calls.
 */

int	heartbeat_grain_qw[2];
int	heartbeat_fast_qw[2];

pad_itoa(v,nd,st)
int	v,nd;
char	*st;
  {
    	int	i,j,k;
	char	*cp;

	j = v;  cp = st + nd - 1;
	for(i = 0;i < nd; i++)
	  {
	    k = j - 10 * (j / 10);
	    *cp-- = '0' + k;
	    j = j / 10;
	  }
  }

rtc_bintim(t,qw)
double	t;
int	qw[2];
  {
	int	t1;
	int	cc,ss,mm,hh,dddd;
	int	p1;
	char	d[20];
	struct dsc$descriptor	ts;

	t1 = (t + .001) * 100;
	cc = t1 - 100 * (t1 / 100);
	t1 = t1 / 100;
	ss = t1 - 60 * (t1 / 60);
	t1 = t1 / 60;
	mm = t1 - 60 * (t1 / 60);
	t1 = t1 / 60;
	hh = t1 - 24 * (t1 / 24);
	dddd = t1 / 24;


	if(dddd == 0)
	  {  d[0] = '0';  p1 = 1;}
	 else
	  {  pad_itoa(dddd,3,d); p1 = 3;}
	d[p1++] = ' ';

	if(hh > 0)
	  {  pad_itoa(hh,2,&d[p1]);  p1 += 2;}
	d[p1++] = ':';

	if(mm > 0)
	  {  pad_itoa(mm,2,&d[p1]);  p1 += 2;}
	d[p1++] = ':';

	if(ss > 0)
	  {  pad_itoa(ss,2,&d[p1]);  p1 += 2;}
	if(cc > 0)
	  {  d[p1++] = '.';  pad_itoa(cc,2,&d[p1]);  p1 += 2;}
	d[p1++] = '\0';

	ts.dsc$w_length = strlen(d);
	ts.dsc$b_dtype = DSC$K_DTYPE_T;
	ts.dsc$b_class = DSC$K_CLASS_S;
	ts.dsc$a_pointer = d;

	if(SS$_NORMAL != sys$bintim(&ts,qw))
	    fprintf(stderr,"bintim: did not like:%s as ascii string\n",d);
  }

rtc_setimr(qw,fcn)
int	qw[2];
int	(*fcn)();
  {
	if(SS$_NORMAL != sys$setimr(0,qw,fcn,0,0,0))
	  fprintf(stdout,"settimr: did not like call\n");
  }

/*
 *	set_timer_params  -  set the clock timer parameters for the
 *			     value given in val.  Also set the fast
 *			     timer values to 1/5 the input value.
 */

set_timer_params(val)
double	val;
  {
	double	x;
	int	i;

	x = val;
	rtc_bintim(x,heartbeat_grain_qw);

	x = val / 5;	/* for the fast timer on heartbeat block */
	rtc_bintim(x,heartbeat_fast_qw);

	return;
  }

/*
 *	This function gets called when the timer expires.  Since the
 *	form of the function which gets called is system specific,
 *	we execute exec_fcn, which then executes the "generic" function
 *	which the caller to start_timer (below) actually requested.
 */

static void (*user_fcn)();

int	exec_fcn(param)
int	*param;
  {
	(void) (*user_fcn)();

	return 0;
  }

/*
 *	Cause the system call to interupt the program according
 *	to the parameters previously set up.  If speed is 0, then
 *	use the fast timer value, otherwise the normal one.
 */

start_timer(fcn,speed)
void	(*fcn)();
  {
	int	*qw;

	user_fcn = fcn;

	if(speed == 0)
		qw = heartbeat_fast_qw;
	 else
		qw = heartbeat_grain_qw;

	if(SS$_NORMAL != sys$setimr(0,qw,exec_fcn,0,0,0))
	  fprintf(stdout,"settimr: did not like call\n");

  }
