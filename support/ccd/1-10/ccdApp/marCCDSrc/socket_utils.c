/*
 * RCV SOCKET TCP
 */

#define	UNIX	1
#define	OS2	0

#include	<stdio.h>
#include	<errno.h>
#include	<ctype.h>
#include	<signal.h>
#include	<sys/types.h>
#include	<sys/socket.h>
#include	<fcntl.h>
#include	<netinet/in.h>
#include	<netdb.h>
#if UNIX
#include	<time.h>
#include	<sys/time.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<arpa/inet.h>
#include	<string.h>
/* #include	<bstring.h> */
#endif
#if !UNIX
#include	<sock_err.h>
#endif
#if	OS2
#include	<sockdefs.h>
#endif

#include "socket_utils.h"


#if defined(__alpha)
#define ALIGNMENT_REQUIRED
#endif


#if	UNIX

#define	MAX_SOCKETS	32
#define	RM_OFFSET(s)	(s)
#define	ADD_OFFSET(s)	(s)

#define	perror_socket(a)	perror(a)
#define	close_socket(a)		close(a)

#endif

#if OS2

typedef unsigned char uchar_t;

#ifndef SIGINT
#define SIGINT 2
#endif

#endif

#define BUFFSIZE 128


static fd_set          cons;
static int             conn_request_skt;
static char            buf[BUFFSIZE];
static char            rbuf[MAX_SOCKETS][BUFFSIZE];

static int 	       verbose = 0;

extern int      errno;


static int getNextAvailSkt(fd_set *ready);
static void printMask(int flag, char *str, fd_set *m);
static void showAddr(const char *str, struct sockaddr_in *a);

int simxedar(void *buffer,int pattern);
int simsiemens(void *buffer,int pattern);


int open_listen_socket(int port)
{
   int             rc;
   struct sockaddr_in sa_in;


   FD_ZERO(&cons);
   if (verbose) {
      printMask(0, "After FD_ZERO(cons) ", &cons);
   }

   {
      int             protocol;
      /****  Create a socket *****/
      protocol = getprotobyname("tcp")->p_proto;
      conn_request_skt = socket(PF_INET, SOCK_STREAM, protocol);
   }
   if (verbose) {
      timestamp(": Initial socket opened.\n");
   }

   if (conn_request_skt == -1) {
      timestamp(": ");
      printf("socket() failed, errno=%d\n", errno);
      perror("");
      return(-1);
   }


   /****  Bind the socket to an address family, port and address *****/

   sa_in.sin_family = AF_INET;
   sa_in.sin_port = htons(port);
   sa_in.sin_addr.s_addr = INADDR_ANY;

   errno = 0;
   rc = bind(conn_request_skt, (struct sockaddr *) & sa_in, sizeof(sa_in));

   timestamp(": Initial socket bound.\n");

   if (rc == -1) {
      timestamp(": ");
      printf("bind() failed, errno=%d\n", errno);
      perror("");
      close_socket(conn_request_skt);
      return(-1);
   }


   /****  Establish willingness to accept incoming connections, queue limit of 2 *****/
   rc = listen(conn_request_skt, 2);

   if (verbose) {
      timestamp(": Begin listen.\n");
   }

   if (rc == -1) {
      timestamp(": ");
      printf("listen() failed, errno=%d\n", errno);
      perror("");
      close_socket(conn_request_skt);
      return(-1);
   }


   return(conn_request_skt);

}

int listen_socket(char *buffer, int bufsize, int conn_request_skt, int *data_socket, int poll)
{
   int             rc;
   int             addrlen;
   struct sockaddr_in c_addr;
   fd_set          ready;
   int nread;
   struct timeval  timeout;
   struct timeval  *timeoutp;


   if (poll) {
      timeout.tv_sec = 0;
      timeout.tv_usec = 0;
      timeoutp = &timeout;
   }
   else {
      timeoutp = NULL;
   }


   FD_SET(conn_request_skt, &cons);

   /*****  looking for connection requests *****/

      /* store set of all connections */
      memcpy((char *) &ready, (char *) &cons, sizeof(fd_set));

      if (verbose & !poll) {
	 printMask(1, "Before select ", &ready);
	 fflush(stdout);
      }

      /***** Find number of "ready" file descriptors *******/
      /***** Only interested in read fds *******/
      rc = select(MAX_SOCKETS, &ready, (fd_set *) 0, (fd_set *) 0, timeoutp);

      switch(rc) {

      case -1:
	 printf("select() failed, errno=%d\n", errno);
	 perror("");
	 return(-1);
	 break;

      case 0:
	 return(0);
         break;

      default:

         nread = 0;

         if (verbose) {
	    timestamp(": ");
	    printf("nready=%d\n", rc);
	    printMask(2, "After select ", &ready);
	 }

         if (rc < 0) {
	    /* This should never happen */
	    break;
	 }


	 /* Check the connection request socket, for new connection requests */
	 if (FD_ISSET(conn_request_skt, &ready)) {
	    int             ns;
	    /****  Accept a new incoming connection *****/
	    addrlen = sizeof(struct sockaddr_in);
	    ns = accept(conn_request_skt, (struct sockaddr *) & c_addr, &addrlen);

	    if (verbose) {
	       showAddr("After accept", &c_addr);
	    }

	    /* fcntl (ns, F_SETFL, FNONBLK); */
	    FD_SET(ns, &cons);

	    if (verbose) {
	       timestamp(": ");
	       printf("%d; <new-connection>\n", ns);
	    }
	 }
	 /* Process any pending input data  on open connections */
	 else {
	    int             skt;
	    int             n;

	    if ((skt = getNextAvailSkt(&ready)) != -1) {

	       if (skt == conn_request_skt) {
		  printf("NextAvailSocket == acceptFd\n");
	       }
	       else {
		  switch(n = recv(skt, rbuf[RM_OFFSET(skt)], BUFFSIZE-1, 0)) {
		  default:

		     rbuf[skt][n] = '\0';

		     if (verbose) {
			printf("%02d[%s]: %d bytes\n", skt, rbuf[skt], n);
		     }

		     /* Copy data from any socket to input buffer */
		     strncpy(buffer, rbuf[RM_OFFSET(skt)], bufsize);

		     nread = n < bufsize ? n : bufsize;
		     if (data_socket) *data_socket = skt;

		     break;
		  case 0:
		     timestamp(": ");
		     printf("rcv returned 0 on  socket %d \n", skt);
		  case -1:
		     FD_CLR(skt, &cons);
		     close_socket(skt);
		     timestamp(": ");
		     printf("Closed socket %d (on -1 recv) \n", skt);
		     break;
		  }
	       }
	    }
	    else {
		     timestamp(": No socket from getNextAvailSkt");
	    }
	 }
	 return(nread);
         break;
      }
      return(0);
}

int open_socket (char *hostname, int port)
{
   short port_h = (short) port;
   int   sock;
   short port_n;
   struct in_addr  addr_h, addr_n;
   int   socket_type;
   int   protocol_family;
   int   protocol;
   int   rc;
   struct hostent *he;
   struct sockaddr_in sa_in;


   if (verbose) {
      fprintf(stderr, "open_socket: hostname = %s, port = %d", hostname, port);
   }


/*******************************************************************************

        Get the address of the desired remote host

*******************************************************************************/

   if ((he = gethostbyname(hostname)) != 0) {
      /* copy address structure  and keep a copy in host and in network byte order */
      memcpy((char *) &addr_n, he->h_addr, he->h_length);
      addr_h.s_addr = ntohl(addr_n.s_addr);
      port_n = htons(port_h);
   }
   else {
      if (verbose) {
         herror("open_socket: gethostbyname");
         fprintf(stderr, "open_socket: gethostbyname failed for %s", hostname);
      }
      return (-1);
   }


/*******************************************************************************

        Initialize the socket data structures
        (Internet address and port of remote partner)

*******************************************************************************/

   socket_type = SOCK_STREAM;
   protocol_family = PF_INET;
   protocol = getprotobyname("tcp")->p_proto;

   sa_in.sin_family = AF_INET;
   sa_in.sin_port = port_n;
   sa_in.sin_addr = addr_n;


/*******************************************************************************

        Socket call

*******************************************************************************/


   if ((sock = socket (protocol_family, socket_type, protocol)) == -1) {
      if (verbose) {
         perror("open_socket: socket");
         fprintf(stderr, "open_socket: Could not open socket\n");
      }
      return (-1);
   }
   if (verbose) {
      fprintf(stderr, "open_socket: socket fd = %d\n", sock);
   }

/*******************************************************************************

        Connection request

*******************************************************************************/

   if (verbose) {
      showAddr("open_socket: target address = ", &sa_in);
      fprintf(stderr, "open_socket: posting connection request, port = %d\n",
            sa_in.sin_port);
   }

   if ((rc = connect(sock, (struct sockaddr *) & sa_in, sizeof(sa_in))) == -1) {
      if (verbose) {
         perror("open_socket: connect");
         fprintf(stderr, "open_socket: Could not connect\n");
         fprintf(stderr, "connect(%d) failed\n",sock);
      }
      close_socket(sock);
      return (-1);
   }

   if (verbose) {
      fprintf(stderr, "After connect rc=%d\n", rc);
      fprintf(stderr, "open_socket: connected to %s\n", hostname);
   }


/*******************************************************************************

        return the socket descriptor, to be used as a file descriptor

*******************************************************************************/
   FD_SET(sock, &cons);

   return sock;
}                               /* end of open_socket */


int write_socket(int socket, char *buffer, int bufsize)
{
   int             nsent;

   if (verbose) {
      timestamp(": ");
      printf("sending %d bytes on socket %d\n", bufsize, socket);
   }

   switch((nsent = send(socket, buffer, bufsize, MSG_DONTWAIT))) {
   case -1:
      switch(errno) {
      case EAGAIN:
      default:
	 printf("send() failed, errno=%d\n", errno);
	 perror("");
	 break;
      }
      break;
   default:
      break;
   }

   return (nsent);
}

void timestamp(const char *string)
{
   time_t tloc;
   struct timeval tod;

   time(&tloc);
   gettimeofday(&tod, 0);
   tloc = tod.tv_sec;
#if defined(_SVID_SOURCE)|| defined(_BSD_SOURCE) || defined(_POSIX_SOURCE) 
   strftime(buf,32,"%d-%b-%Y %X", localtime(&tloc));
#elif defined(_SGI_SOURCE)
   ascftime(buf,"%d-%b-%Y %X",localtime(&tloc));
#else
   strftime(buf,32,"%x %X", localtime(&tloc));
#endif
   printf("%s.%3.3ld%s", buf,tod.tv_usec/1000,string);
   fflush(stdout);

   return;
}


static void showAddr(const char *str, struct sockaddr_in *a)
{
   printf("%s", str);
#if !UNIX
   printf("%03d.", (uchar_t) a->sin_addr.S_un.S_un_b.s_b1);
   printf("%03d.", (uchar_t) a->sin_addr.S_un.S_un_b.s_b2);
   printf("%03d.", (uchar_t) a->sin_addr.S_un.S_un_b.s_b3);
   printf("%03d!", (uchar_t) a->sin_addr.S_un.S_un_b.s_b4);
#else
   printf("%s!", inet_ntoa(a->sin_addr));
#endif
   printf("%u \n", (unsigned short) a->sin_port);

   return;
}

static void printMask(flag, str, m)
   int             flag;
   char           *str;
   fd_set         *m;
{
   int             i;
   if (flag < 1)
      return;
   printf("%d; ", flag);
   printf("%-17s : mask[", str);
   for (i = 0; i < MAX_SOCKETS; i++) {
      if (i % 8 == 0)
	 putchar(' ');
      if (FD_ISSET(i, m))
	 putchar('1');
      else
	 putchar('0');
   }
   printf(" ]  ");
}
/*
 * cycles through the list of sockets, returning next available on each successive call
 */
static int getNextAvailSkt(ready)
   fd_set         *ready;
{
   static int      rs = 0;
   int             i;
   rs %= MAX_SOCKETS;
   for (i = rs; i < MAX_SOCKETS; i++)
      if (FD_ISSET(i, ready)) {
	 rs = i + 1;
	 return (i);
      }
   for (i = 0; i < rs; i++)
      if (FD_ISSET(i, ready)) {
	 rs = i + 1;
	 return (i);
      }
   return (-1);
}
