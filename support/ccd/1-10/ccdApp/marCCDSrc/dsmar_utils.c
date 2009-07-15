#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdarg.h>

#include <sys/time.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include "dsmar_utils.h"

#define DSERVER_BUFSIZE 1024
#define DSMAR_IDLE 0
#define DSMAR_AQUIRE 1
#define DSMAR_READOUT 2
#define DSMAR_CORRECT 3
#define DSMAR_WRITING 4

static int dserver_fdin, dserver_fdout;
static int dserver_pid;
struct dserver_vars dserver_v;

void dserver_init_child() 
{
  dserver_fdout = atoi(getenv("IN_FD"));     /* We read from here */
  dserver_fdin = atoi(getenv("OUT_FD"));  /* and write to there */
  fprintf (stderr, "%d %d\n",dserver_fdout,dserver_fdin); 

  return;
}
  
int dserver_init_com(path)
     char *path;
{
  int p_write[2], p_read[2]; /* I write to p_write and read from p_read */
  char infd[16], outfd[16];

  if (pipe(p_read) == -1 || pipe(p_write) == -1) {
    perror(" Can not create pipes\n");
    return(-1);
  }

  dserver_pid = fork();
  
  if (dserver_pid < 0) {	/* fork error */
    perror("Can't fork\n");
    return(-1);
  }
  
  if (dserver_pid == 0) {	/* we are in the child */

    close(p_write[1]); 
    close(p_read[0]); 

    sprintf(outfd,"OUT_FD=%d",p_read[1]); /* the other one writes here */
    sprintf(infd,"IN_FD=%d",p_write[0]); /* the other one gets input here */

    putenv(infd);
    putenv(outfd);
    
    printf("exec(%s)\n",path);
    execlp(path, path, "test", NULL);
    
    perror("can't exec dserver");
    exit(-1);
  }	/*** end of Fork *****/
  
  /******* close off unused ends of pipes ****/
  
  close (p_read[1]);
  close (p_write[0]);
  
  dserver_fdout  = p_read[0];    /* We read from here */
  dserver_fdin   = p_write[1];   /* and write to there */

  return(0);
}

void dserver_close() {
  close (dserver_fdin);
  close (dserver_fdout);
  if (dserver_pid) { /* In the parent */
    kill (dserver_pid, 2);
    wait (NULL);
  }
}
  
int dserver_send(char *cmd, ...) 
{
  va_list   args;
  char *arg;
  static char buf[DSERVER_BUFSIZE]; 
  int len;

  va_start(args, cmd);
  arg = va_arg(args,char*);
  

  if (cmd) 
    strcpy (buf,cmd);
  while (arg) {
    strcat (buf, ",");
    strcat (buf, arg);
    arg = va_arg(args,char*);
  }
  va_end(args);
  strcat (buf, "\n");
  len = strlen (buf);
  if (write (dserver_fdin, buf, len) != len) {
    perror ("Write failed\n");
    return -1;
  }
  return 0;
}
    
int dserver_check() 
{
  struct itimerval value;
  int timeout;
  int fd_width;
  fd_set in_fdset;

#ifdef HPUX
  fd_width = sysconf(_SC_OPEN_MAX);
#else
  fd_width = getdtablesize();
#endif

  /* Check our input streams */
  FD_ZERO(&in_fdset);
  FD_SET(dserver_fdout,&in_fdset);
  
  value.it_value.tv_usec = 1000; /* 1 ms timeout should be OK */ 
  value.it_value.tv_sec = 0;

#ifdef HPUX
  timeout = select(fd_width,(int*) (&in_fdset),NULL,NULL,&value.it_value);
#else
  timeout = select(fd_width,(&in_fdset),NULL,NULL,&value.it_value);
#endif
  
  /* See if there is input from the dserver */
  if (!FD_ISSET(dserver_fdout,&in_fdset)) 
    return 0;

  return 1;
}

char *dserver_read() 
{
  static char bufin[DSERVER_BUFSIZE]; 
  static char bufout[DSERVER_BUFSIZE]; 
  static char * bp, *bufend = bufin;
  static int buffer_message = 0;
  int len, something_left;

  if (dserver_check() == 0) {
    if (!buffer_message)
      return NULL;
  } else {
    len = read (dserver_fdout, bufend, DSERVER_BUFSIZE);
    if (len == -1) {
      perror ("Read failed\n");
      return 0;
    }
    buffer_message = 1;
    bufend += len;
  }

  for (bp = bufin; bp < bufend ; bp++) {
    if (*bp == '\n') {
      *bp = '\0';
      strcpy (bufout, bufin);
      something_left = bufend - bp - 1;
      if (something_left)
	memmove (bufin, bp + 1, something_left);
      bufend = bufin + something_left;
      return bufout;
    }
  }
  buffer_message = 0;
  return NULL;
}

/* Called from mar to set size in dserver */
void dserver_is_size (x,y) 
{
  char xstr[256], ystr[256];
  sprintf(xstr,"%d",x);
  sprintf(ystr,"%d",y);
  dserver_send ("is_size",xstr,ystr,NULL);
}

/* Called from dserver to ask mar to send size info */
void dserver_get_size () 
{
  dserver_send ("get_size",NULL);
}

/* Called from dserver to ask mar to set the size */
void dserver_set_size(x,y)
{
  char xstr[256], ystr[256];
  sprintf(xstr,"%d",x);
  sprintf(ystr,"%d",y);
  dserver_send ("set_size",xstr,ystr,NULL);
}  

/* Called from mar to set binning in dserver */
void dserver_is_bin (x,y) 
{
  char xstr[256], ystr[256];
  sprintf(xstr,"%d",x);
  sprintf(ystr,"%d",y);
  dserver_send ("is_bin",xstr,ystr,NULL);
}

/* Called from dserver to ask mar to send binning info */
void dserver_get_bin () 
{
  dserver_send ("get_bin",NULL);
}

/* Called from dserver to ask mar to set the binning */
void dserver_set_bin(x,y)
{
  char xstr[256], ystr[256];
  sprintf(xstr,"%d",x);
  sprintf(ystr,"%d",y);
  dserver_send ("set_bin",xstr,ystr,NULL);
}  

/* Called from mar to set preset in dserver */
void dserver_is_preset (time)
     double time;
{
  char tstr[256];
  sprintf(tstr,"%10f",time);
  dserver_send ("is_preset",tstr,NULL);
}

/* Called from dserver to ask mar to send binning info */
void dserver_get_preset () 
{
  dserver_send ("get_preset",NULL);
}

/* Called from dserver to ask mar to set the preset */
/* If preset is 0 then expose until readout */ 
void dserver_set_preset(time)
     double time;
{
  char tstr[256];
  sprintf(tstr,"%10f",time);
  dserver_send ("set_preset",tstr,NULL);
}  

/* Called from mar to set preset in dserver */
void dserver_is_state (state)
     int state;
{
  char sstr[256];
  sprintf(sstr,"%d",state);
  dserver_send ("is_state",sstr,NULL);
}

/* Called from dserver to ask mar to send the state */
void dserver_get_state () 
{
  dserver_send ("get_state",NULL);
}

void dserver_start_acq ()
{
  dserver_send ("start",NULL);
}

void dserver_abort ()
{
  dserver_send ("abort",NULL);
}

void dserver_readout (background_flag)
     int background_flag;
{
  char sstr[256];
  sprintf(sstr,"%d",background_flag);
  dserver_send ("readout",sstr, NULL);
}

void dserver_writefile (filename, correct_flag)
     char *filename;
     char *correct_flag;
{
  dserver_send ("writefile",filename,correct_flag, NULL);
}

int dserver_header (no, header_strings)
     int no;
     char *header_strings[];
{
  int i, len=0;
  static char str[DSERVER_BUFSIZE - 64]; 

  for (i = 0; i < no; i++) 
    len += 1 + strlen(header_strings[i]);
  
  if (len > DSERVER_BUFSIZE - 64) 
    return 1;

  str[0] = '\0';

  for (i = 0; i < no; i++) { 
    strcat(str, header_strings[i]);
    if (i != no - 1 )
      strcat(str,",");
  }

  dserver_send ("header",str, NULL);
  return(0);
}

void dserver_correct ()
{
  dserver_send ("correct",NULL);
}

int dserver_check_in () 
{ 
  char cmd[256], filename[1024], *res;
  int xsize, ysize, xbin, ybin, correctflag;
  double preset;
  if ((res = dserver_read()) == 0) 
    return 0;
  sscanf(res, "%[^,\n]", cmd);
  if (strcmp(cmd, "get_size") == 0) {
    /* mar sends dserver the actual detector size */
    /* dserver_is_size (xsize,ysize); */
  } else if (strcmp(cmd, "set_size") == 0) {
    if (sscanf(res, "%*[^,],%d,%d", &xsize, &ysize) != 2) 
      perror ("set_size protocol error");
    /* mar has to program a new size and sends the new size if OK*/
    dserver_is_size (xsize,ysize);
  } else if (strcmp(cmd, "get_bin") == 0) {
    /* mar sends dserver the actual binning */
    /* dserver_is_bin (xbin,ybin); */
   } else if (strcmp(cmd, "set_bin") == 0) {
    if (sscanf(res, "%*[^,],%d,%d", &xbin, &ybin) != 2) 
      perror ("set_bin protocol error");
    /* mar has to program a new binning and sends the new bin if OK*/
    dserver_is_bin (xbin,ybin); 
   } else if (strcmp(cmd, "get_preset") == 0) {
    /* mar has to send the preset value to the dserver*/
    /* dserver_is_preset (time); */
   } else if (strcmp(cmd, "set_preset") == 0) {
    if (sscanf(res, "%*[^,],%lf", &preset) != 2) 
      perror ("set_preset protocol error");
    /* mar has to program a preset send the new preset back*/
    dserver_is_preset (preset);
   } else if (strcmp(cmd, "start") == 0) {
    /* mar has to start image taking */
     dserver_is_state (DSMAR_AQUIRE);
   } else if (strcmp(cmd, "readout") == 0) {
    /* mar has to start readout of the ccd and put it to either background 
     or forground image */
     dserver_is_state (DSMAR_READOUT);
   } else if (strcmp(cmd, "correct") == 0) {
    /* mar has to start correct image on line */
     dserver_is_state (DSMAR_CORRECT);
   } else if (strcmp(cmd, "writefile") == 0) {
    /* mar has to write image to disk */
    if (sscanf(res, "%*[^,],%[^,],%d", filename, &correctflag) != 2)
     perror ("writefile protocol error");
     dserver_is_state (DSMAR_WRITING);
   } else if (strcmp(cmd, "abort") == 0) {
    /* mar has to abort the image taking */
     dserver_is_state (DSMAR_IDLE);
   }
   return(0);
}

int dserver_check_in_client ()
{
  char cmd[256], *res;
  if ((res = dserver_read()) == 0) 
    return 0;
  sscanf(res, "%[^,\n]", cmd);
  if (strcmp(cmd, "is_size") == 0) {
    if (sscanf(res, "%*[^,],%d,%d", &dserver_v.x, &dserver_v.y) != 2) 
      perror ("is_size protocol error");
  } else if (strcmp(cmd, "is_bin") == 0) {
    if (sscanf(res, "%*[^,],%d,%d", &dserver_v.xbin, &dserver_v.ybin) != 2) 
      perror ("is_bin protocol error");
  } else if (strcmp(cmd, "is_preset") == 0) {
    if (sscanf(res, "%*[^,],%lf", &dserver_v.preset) != 1) 
      perror ("is_preset protocol error");
  } else if (strcmp(cmd, "is_state") == 0) {
    if (sscanf(res, "%*[^,],%d", &dserver_v.state) != 1) 
      perror ("is_state protocol error");
  }
  return 1;
}
