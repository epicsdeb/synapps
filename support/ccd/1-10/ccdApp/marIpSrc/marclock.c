/*********************************************************************
 *
 * scan345: marclock.c 
 *
 * Copyright by Dr. Claudio Klein, X-Ray Research GmbH.
 *
 * Version:     3.1
 * Date:        14/05/2002
 *
 * Version	Date		Mods
 * 3.1		14/05/2002	Make compatible to VMS
 *
 *********************************************************************/
#include	<stdio.h>
#include	<signal.h>
#include	<sys/time.h>
#ifdef VMS
#include	<descrip.h>
#include	<ssdef.h>
#endif

#define	MAXCLQUEUE	100
#define	HEART_GRAIN	(0.1)	/* timer grain */

int	heartbeat_grain_sec 	= 0;
int	heartbeat_grain_usec 	= 100000;
int	heartbeat_fast_sec 	= 0;
int	heartbeat_fast_usec 	= 20000;
double	heartbeat_grain 	= HEART_GRAIN;

struct clq_st {
	int		clq_used;	/* 0 if currently not used, else 1 */
	int		clq_ival;	/* interval timer value */
	struct clq_st	*clq_prev; 	/* pointer to prev quque entry */
	struct clq_st	*clq_next; 	/* pointer to next quque entry */
	void		(*clq_fcn)();	/* fcn to execute on when ival = 0 */
	int		clq_arg;	/* argument to the above function */
};

typedef struct 	clq_st 	clq;

clq		*clqhead;		/* points to first queue entry */
clq		clqueue[MAXCLQUEUE];	/* array containing the clock queue */
int		block_heartbeat;	/* 1 to temporarily block clock heartbeat */
int		clq_is_init = 0;	/* 1 if queues are initialized */

#ifdef VMS
static int	heartbeat_grain_qw[2];
static int	heartbeat_fast_qw[2];
static void 	(*user_fcn)	();
static int	exec_fcn	();
static void 	rtc_setimr	();
static void 	rtc_bintim	(double,int *);
static void 	pad_itoa	(int,int,char *);
#endif

/* 
 * Function prototypes 
 */

void		mar_clock		(void);
void		set_timer_params	(double);
void		start_timer		();
void		enqueue_fcn		();
static void 	clock_heartbeat		(void);
static void 	init_clock		(void);
static void	mar_heartbeat		(int);


/******************************************************************
 * Function: set_timer_params
 ******************************************************************/
void set_timer_params(double val)
{
double	x;
int	i;

	x = val;

#ifdef VMS
	x = val;
	rtc_bintim(x,heartbeat_grain_qw);

	x = val / 5;	/* for the fast timer on heartbeat block */
	rtc_bintim(x,heartbeat_fast_qw);
#else
	i = (int) x;
	heartbeat_grain_sec = i;
	x = x - i;
	if(x < 0) x = 0;
	i = x * 1000000;
	heartbeat_grain_usec = i;

	x = val / 5;	/* for the fast timer on heartbeat block */
	i = (int) x;
	heartbeat_fast_sec = i;
	x = x - i;
	if(x < 0) x = 0;
	i = x * 1000000;
	heartbeat_fast_usec = i;
#endif
	return;
}


/******************************************************************
 * Function: start_timer
 ******************************************************************/
void start_timer(fcn,speed)
void	(*fcn)();
int	speed;
{
#ifdef VMS
	int	*qw;

	user_fcn = fcn;

	if(speed == 0)
		qw = heartbeat_fast_qw;
	 else
		qw = heartbeat_grain_qw;

	if(SS$_NORMAL != sys$setimr(0,qw,exec_fcn,0,0,0))
	  fprintf(stdout,"settimr: did not like call\n");
#else
	struct	itimerval	tv;

	tv.it_interval.tv_sec = 0;
	tv.it_interval.tv_usec = 0;
	if(speed == 0)
	  {
		tv.it_value.tv_sec = heartbeat_fast_sec;
		tv.it_value.tv_usec = heartbeat_fast_usec;
	  }
	 else
	  {
		tv.it_value.tv_sec = heartbeat_grain_sec;
		tv.it_value.tv_usec = heartbeat_grain_usec;
	  }

	signal(SIGALRM,fcn);

	setitimer(ITIMER_REAL,&tv,NULL);
#endif
}

/******************************************************************
 * Function: mar_clock
 ******************************************************************/
void mar_clock()
{
int		i, killtimer = 0;
extern int 	get_status();


	enqueue_fcn(mar_heartbeat,0,1.0);
	enqueue_fcn(get_status,0,1.0);

	init_clock();

	while(killtimer == 0) {
		pause();
	}
}

/******************************************************************
 * Function: enqueue_fcn  
 ******************************************************************/
void enqueue_fcn(fcn,arg,dtval)
void	(*fcn)();
int	arg;
double	dtval;
{
int	i,j;
clq	*qp;

	block_heartbeat = 1;
	if(clq_is_init == 0)
	  {
		clqhead = NULL;
		for(i = 0;i < MAXCLQUEUE; i++)
			clqueue[i].clq_used = 0;
		clq_is_init = 1;
	  }
	for(i = 0;i < MAXCLQUEUE;i++)
	  if(clqueue[i].clq_used == 0)
	    {
		clqueue[i].clq_used = 1;
		j = .5 + dtval / heartbeat_grain;
		if(j == 0) j = 1;
		clqueue[i].clq_ival = j;
		clqueue[i].clq_fcn = fcn;
		clqueue[i].clq_arg = arg;
		clqueue[i].clq_next = NULL;
		if(clqhead == NULL)
		  {
		    clqhead = &clqueue[i];
		    clqueue[i].clq_prev = NULL;
		    block_heartbeat = 0;
		    return;
		  }
		for(qp = clqhead; ; qp = qp->clq_next)
		  if(qp->clq_next == NULL)
		    {
			qp->clq_next = &clqueue[i];
			clqueue[i].clq_prev = qp;
			block_heartbeat = 0;
			return;
		    }
	    }
	fprintf(stdout,"enqueue_fcn:  no more clock queue entries avail\n");
	exit(1);
}

/******************************************************************
 * Function: mar_heartbeat
 ******************************************************************/
static void mar_heartbeat(int arg)
{
	/*
	 * Re-enable the timer, and return.
	 */
	enqueue_fcn(mar_heartbeat,0,1.0);
}

/******************************************************************
 * Function: clock_heartbeat
 ******************************************************************/
static void clock_heartbeat(void)
{
clq			*qp;

	if(block_heartbeat == 1)
	  {
	    /*
	     *	If the heartbeat is blocked (e.g., by the enqueue_fcn
	     *	routine), then we just re-enable the timer to be called
	     *	again in 1/5th the normal clock grain.  Since functions
	     *	which block heartbeat must be quick, this fast time
	     *	will cause sucessful execution on the next timer callout.
	     */

	    start_timer(clock_heartbeat,0);

	    return;
	  }

	if(clqhead == NULL)
	  {
	    /*
	     *	There are no functions queued for execution.  Depending
	     *	on how the application which uses these routines is
	     *	written, this condition may indicate an error.  In a
	     *	completely clock callout driver application, this should
	     *	happen ONLY before the first callout is queued, and never
	     *	again.  Since there are other ways to use these routines,
	     *	we don't flag an error here.
	     */

	    start_timer(clock_heartbeat,1);

	    return;
	  }

	/*
	 *	Decrement the clq_ival variables in all active queue
	 *	entries.  Do not decrement any entries whose clq_ival
	 *	has already gone to zero.  These will executed on a
	 *	first come, first served basis.
	 */
	for(qp = clqhead; qp != NULL; qp = qp->clq_next)
	  {
	    if(qp->clq_ival > 0)
		qp->clq_ival--;
	  }
	
	/*
	 *	Queue the next heartbeat now.
	 */
	
	start_timer(clock_heartbeat,1);

	/*
	 *	Find the first entry in the queue whose clq_ival value
	 *	is zero.  Unqueue it and execute it.
	 */

	for(qp = clqhead; qp != NULL; qp = qp->clq_next)
	  if(qp->clq_ival == 0)
	    {
	      if(qp->clq_prev == NULL)
		{
		  clqhead = qp->clq_next;
	          if(qp->clq_next != NULL)
		    (qp->clq_next)->clq_prev = NULL;
		}
	       else
		{
		 (qp->clq_prev)->clq_next = qp->clq_next;
		 if(qp->clq_next != NULL)
			(qp->clq_next)->clq_prev = qp->clq_prev;
		}

	      qp->clq_used = 0;

	      (void) (*qp->clq_fcn)(qp->clq_arg);

	      return;
	    }
	return;
  }

/******************************************************************
 * Function: void init_clock
 ******************************************************************/
static void init_clock(void)
{
int			i;

	if(clq_is_init == 0)
	  {
		clqhead = NULL;
		for(i = 0;i < MAXCLQUEUE; i++)
			clqueue[i].clq_used = 0;
		clq_is_init = 1;
	  }

	set_timer_params(heartbeat_grain);

	start_timer(clock_heartbeat,1);
}

#ifdef VMS

/******************************************************************
 * Function: pad_itoa
 ******************************************************************/
static void pad_itoa(int v,int nd,char *st)
{
    	int	i,j,k;
	char	*cp;

	j = v;  cp = st + nd - 1;
	for(i = 0;i < nd; i++) {
	    k = j - 10 * (j / 10);
	    *cp-- = '0' + k;
	    j = j / 10;
	}
}

/******************************************************************
 * Function: rtc_bintim
 ******************************************************************/
static void rtc_bintim(double t,int *qw)
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

/******************************************************************
 * Function: rtc_settimr
 ******************************************************************/
static void rtc_setimr(qw,fcn)
int	qw[2];
int	(*fcn)();
{
	if(SS$_NORMAL != sys$setimr(0,qw,fcn,0,0,0))
	  fprintf(stdout,"settimr: did not like call\n");
}

/******************************************************************
 * Function: exec_fcn
 ******************************************************************/
int	exec_fcn(int *param)
{
	(void) (*user_fcn)();

	return 0;
}
#endif
