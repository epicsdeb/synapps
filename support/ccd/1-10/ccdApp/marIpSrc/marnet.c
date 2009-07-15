/*********************************************************************
 *
 * scan345: marnet.c
 *
 * Copyright by Dr. Claudio Klein, X-Ray Research GmbH.
 *
 * Version:     3.1
 * Date:        14/05/2002
 *
 * History:
 *
 * Date		Version		Description
 * ---------------------------------------------------------------------
 * 15/06/00	2.2		net_data rewritten as in mar345 >= 2.3
 * 14/05/02	3.1		Make compatible for VMS
 *
 **********************************************************************/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

/* For TIMER function */
#include "marcmd.h"

#ifdef VMS
#include <time.h>
#include <types.h>
#include <socket.h>
/* The following standard Unix declarations are missing for some
 * stupid reason on OpenVMS 6.1 and have to be aassigned explicitely
 */
#define	fd_set		int
#define FD_SETSIZE	1024
#define FD_ZERO(p) 	(*(p) = 0)
#define FD_SET(n, p) 	(*(p) |= (1 << (n)))
#define bcopy(b1,b2,len) memmove(b2, b1, (size_t)(len))
#endif

#define			MAX_TRY		1000
#define 		DATA_SIZE       16386
#define 		STAT_SIZE       404
#define 		MESS_SIZE       84
#define			FD_MAX		FD_SETSIZE

#define			SK_COMM		0
#define			SK_STAT		1	
#define			SK_DATA		2
#define			SK_MESS		3	

static char		net_buf		[ DATA_SIZE ];
static char		msg_buf		[ MESS_SIZE ];

char			net_timeout	= 0;
int			mar_socket[4] 	= {-1, -1, -1, -1};
static char		*sk_type[4] 	= { "COMM", "STAT", "DATA", "MESS"};
static char		stop_trying	= 0;
extern char		str		[1024];
extern char		scan_in_progress;
extern int		debug;
extern int		netcontrol;
static struct timeval 	timeout;

/*
 * Local functions
 */
int 			net_open	(int);
int 			net_close	(int);
int			net_data	(void);
int			net_stat	(void);
int			net_comm	(int, char *);
int			net_select	(int);
static void		stop_net	(int);
extern void		mar_kill	();
extern void		mar_abort	();
extern void		swaplong	();
extern void		marFunction	(int, unsigned long);

/******************************************************************
 * Function: net_open
 ******************************************************************/
int 
net_open(int sk)
{
int			i,retry=0;
int 			protocol;
int			new_socket;
struct sockaddr_in 	sin;
struct sockaddr*        sinPtr;
struct hostent 		*hp=NULL;
extern int		mar_port;
extern char		mar_host[32];

    /* Get host address */
    hp = gethostbyname(mar_host);
    if (hp == NULL) {
        return(0);
    }
    if ( strlen( mar_host ) < 1 ) return 0;

    if ( mar_socket[sk]  != -1 ) {
	close( mar_socket[sk] );
	mar_socket[sk] = -1;
    }


    /* Set timeout for socket op's */
    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;

    /*****************************************************
     * Create a socket: try 10 times 
     *****************************************************/
    protocol   = getprotobyname("tcp")->p_proto;

    signal( SIGINT,  stop_net);
    stop_trying = 0;

    while ( retry < MAX_TRY ) {

	    if ( stop_trying ) {
			retry = MAX_TRY;
			break;
	    }

	    new_socket = socket(AF_INET, SOCK_STREAM, protocol);

	    if ( new_socket < 0 ) {
		fprintf( stdout, "scan345: Cannot open socket %s at port %d\n",sk_type[sk],mar_port+sk);
		return(0);
	    }

	   /*****************************************************
	    * Connect to the server.
	    *****************************************************/

	    sin.sin_family 	= AF_INET;
	    sin.sin_port 	= htons(mar_port+sk);
	    bcopy (hp->h_addr, &(sin.sin_addr.s_addr), hp->h_length);
	    sinPtr              = (struct sockaddr *)&sin;

	    if ( connect(new_socket, sinPtr, sizeof(sin)) < 0) {
		fprintf(stdout, "scan345: %d. try to connect to host '%s' ...\n",retry,mar_host);
		if ( retry%5 == 0 ) 
			fprintf(stdout, "        To interrupt, press Ctrl+C\n");
		sleep( 1 );
		close( new_socket );
		retry++;
	    }
	    else 
		break;
    }

    signal( SIGINT,  mar_kill);
    signal( SIGKILL, mar_kill );
    signal( SIGTERM, mar_kill );
    signal( SIGQUIT, mar_abort);

    if ( retry >= MAX_TRY ) {
	fprintf(stdout, "scan345: Giving up ...\n");
	close(new_socket);
	new_socket = -1;
	return 0;
    }

    fprintf(stdout,"scan345: Connected to host '%s' on port %d (%s)\n",mar_host,mar_port+sk,sk_type[sk]);

    /* We don't need the previous socket any more, close it */
    if ( mar_socket[sk] != -1 )
	close( mar_socket[sk] );

    mar_socket[sk] = new_socket;

    /* Set socket NON BLOCKING */
#ifndef VMS
    if ( fcntl( mar_socket[sk], F_SETFL, O_NONBLOCK ) < 0 ) {
	    fprintf(stdout,"scan345: Socket %s cannot be set to NON BLOCKING\n",sk_type[sk]);
    }
#endif

    return 1;
}

/******************************************************************
 * Function: net_close
 ******************************************************************/
int 
net_close(int sk)
{
	if ( mar_socket[sk] < 0 || netcontrol < 1 ) return 0;
	close		( mar_socket[sk] );
	mar_socket[sk] = -1;
	return 1;
}

/******************************************************************
 * Function: net_data: read blocks on data socket
 ******************************************************************/
int  net_data()
{
static int	p_pixels= 9999;
static int	size	= DATA_SIZE;
static int	tot_pix	= 0;
static int	nbeg	= 0;
int		nend	= 0;
int		nread	= 0;
int		ncop 	= 0;
int		ioff	= 0;
int		j 	= 0;
int		i,n;
char		*buf_ptr;
char            rbuf[ DATA_SIZE ];
fd_set		r;
static unsigned short	block_no;

extern int	stat_blocks_scanned;
extern int	stat_blocks_sent;
extern int	ict1,ict2;
extern int	bytes2xfer,maximum_bytes;
extern void	swapshort();
extern void	Transform( int, int, unsigned short *);

	ict1++;

	if ( mar_socket[SK_DATA] < 0 || netcontrol < 1 ) return 0;
	FD_ZERO( &r );
	FD_SET ( mar_socket[SK_DATA], &r );
	i = select(FD_MAX,&r,(fd_set *)0,(fd_set *)0,&timeout);
	if ( i < 1 ) return (-2);

	/* Read from socket */
	nread = read(mar_socket[SK_DATA], rbuf, size); 

	if ( nread<1 ) return -1;

	bytes2xfer 	+= nread;

        /* Increase pointer for end of this chunk of data */
        nend  = nbeg+nread;

        /* Make sure that we dont exceed 16386 kB */
        if ( nend > DATA_SIZE) {
                ioff = nend-DATA_SIZE;
                ncop = nread - ioff;
                nend = DATA_SIZE;
        }
        else {
                ioff = 0;
                ncop = nread;
        }

        /* Copy latest data into net_buf (16kB block) */
        memcpy( net_buf + nbeg, rbuf, ncop );

#ifdef DEBUG
if (debug & 0x200 )
printf("scan345: net_data %5d  block=%4d\n",nread,block_no);
#endif

        /* Increase pointer for begin of next chunk of data */
        nbeg += ncop;

        /* Block not yet finished: read more data */
        if ( nend < 16386 ) {
                block_no        = stat_blocks_sent;
                return nread;
        }

	/* Block of data finished: transform it ... */
#if ( __linux__ || __osf__ || VMS )
	swapshort( (unsigned short *)net_buf, nend);
#endif

	ict2++;

        /* Start of data block: get block no from first 2 bytes */
        memcpy( &block_no, net_buf, sizeof(short) );

        stat_blocks_sent        = block_no;
        buf_ptr                 = net_buf+2;
        n                       = (DATA_SIZE-2)/2;
        tot_pix                 += n;

	/* Transform ... */
	if ( scan_in_progress )
		Transform( n, block_no, (unsigned short *)buf_ptr);

	nbeg = 0;

        /* This time, there are still some data in rbuf that we can't
         * loose. Copy into start of net_buf
         */
        if ( ioff )  {
                memcpy( net_buf, rbuf+ncop, ioff);
                nbeg = ioff;
                ioff = 0;
        }

	return nread;
}

/******************************************************************
 * Function: net_comm: send command to scanner
 ******************************************************************/
int 
net_comm(int mode, char *buf)
{
int	i,j,nwrite,fail=0;
int	sel;
fd_set	r;

	if ( mar_socket[SK_COMM] < 0 || netcontrol < 1 ) return -1;

	/* Check that we can write to socket ... */
	FD_ZERO( &r );
	FD_SET ( mar_socket[SK_COMM], &r );

	if ( mode == 0 ) 	/* Something to read on socket ? */	
		sel = select(FD_MAX,&r,(fd_set *)0,(fd_set *)0,&timeout);
	else 			/* Something to write on socket ? */	
		sel = select(FD_MAX,(fd_set *)0,&r,(fd_set *)0,&timeout);

	if ( sel < 0 ) return -1;

	/* There are data to read on socket: USER PARAMETERS */
	if ( mode == 0 ) {
		return 64;
		nwrite = 0;
		while ( ( i=read(mar_socket[SK_COMM], buf, 64)) > 0 ) {
			nwrite += i;
		}
		return nwrite;
	}

	net_timeout = 0;
	i = nwrite  = 0;
	j = 60;

#if ( __linux__ || __osf__ || VMS )
	swaplong( buf+4, sizeof(char)*28 );
#endif

	while ( nwrite<j ) {
	        i=write(mar_socket[SK_COMM], buf+nwrite, j);
		if  ( i < 1 ) {
			fail++;
			if ( fail > 100 ) {
				net_timeout++;
				return -1;
			}
			continue;
		}
		nwrite+=i;
		j     -=i;
	}

	return nwrite;
}

/******************************************************************
 * Function: net_select
 ******************************************************************/
int 
net_select(int sk)
{
int	sel;
fd_set	r;

	if ( mar_socket[sk] < 0 || netcontrol < 1 ) return -1;

	FD_ZERO( &r );
	FD_SET ( mar_socket[sk], &r );

	sel = select(FD_MAX,(fd_set *)0,&r,(fd_set *)0,&timeout);

	if ( sel < 0 )
	    return -1;

	return sel;
}

/******************************************************************
 * Function: stop_net
 ******************************************************************/
static void stop_net( int signo )
{
	printf( "scan345: signal SIGINT caught\n");
	stop_trying = 1;
}

/******************************************************************
 * Function: net_status: read status block
 ******************************************************************/
int 
net_stat()
{
register unsigned int	j;
static char	s1buf[STAT_SIZE];
extern int 	process_status();
int		nread,i,size,sel;
time_t		t1, t2;
fd_set		r;

    	if ( mar_socket[SK_STAT] < 0 || netcontrol < 1 || net_timeout ) return 0;

        /* Check that we can read from socket ... */

	FD_ZERO( &r );
	FD_SET ( mar_socket[SK_STAT], &r );
	sel = select(FD_MAX,&r,(fd_set *)0,(fd_set *)0,&timeout);

	if ( sel < 1 ) return sel;

	size 	= STAT_SIZE;
    	nread 	= 0;
	j	= 0;
	t1	= time(NULL);

	while ( nread < STAT_SIZE ) {
		j++;
		i     = read(mar_socket[SK_STAT], s1buf+nread, size); 
		if ( j > 10000000 ) {
			t2    = time(NULL);
			if ( difftime( t2, t1 ) > 5.0 ) break;
			continue;
		}
		if ( i < 0 ) continue;
		nread += i;
		size  -= i;
	}

#ifdef DEBUG
	if (debug & 0x800 )
	printf("scan345: net_stat %d\n",i);
#endif

#if ( __linux__ || __osf__ || VMS)
	swaplong( s1buf, (int)STAT_SIZE);
#endif

	if ( nread >= STAT_SIZE )
		i = process_status( s1buf+4 );
	else {
		fprintf( stdout, "scan345: Only %d bytes in status block !!!\n",nread);
	}

	return nread;
}

/******************************************************************
 * Function: net_msg: read message block
 ******************************************************************/
int 
net_msg()
{
extern void	print_msg();
int		sel;
fd_set		r;

    	if ( mar_socket[SK_MESS] < 0 || netcontrol < 1 || net_timeout ) return 0;

	/* We can't accumulate messages in the port, so always try to read
	 * as much as possible 
	 */

	while ( 1 ) {
		sel = read(mar_socket[SK_MESS], msg_buf, MESS_SIZE); 

		if ( sel < 1 ) break;

		print_msg( msg_buf );
	}

	return sel;
}
