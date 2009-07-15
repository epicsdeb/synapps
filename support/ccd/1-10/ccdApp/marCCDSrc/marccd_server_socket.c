
/* RCS Id string */
static char rcsid[] = "marccd_server_socket.c,v 1.2 2004/05/09 04:16:00 rivers Exp";

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "dsmar_utils.h"
#include "socket_utils.h"

char *dserver_read();

extern struct dserver_vars dserver_v;
static struct dserver_vars *Status = &dserver_v;

static int init_command_socket(int port);
static int command_socket_input(int request_socket, char *command_buffer, int buffer_size, int *data_socket);
static int dserver_input_from_server (char *command_buffer, int buffer_size);
static int dserver_input_from_client (int listen_socket, char *command_buffer, int buffer_size);

static int verbose = 1;

static char pbuffer[256];

#define DEFAULT_PORT 1000

static int ReplyToCommands;
static int SendStateChange;
static int ReplySocket;

int main(int argc, char **argv) 
{
  char command_buf[256];
  int listen_port;
  int  ListenSocket = 0;
  static int counter;

  printf("%s:\n", *argv);
  argc--;

  if (argc) {
    argc--;
    argv++;
    listen_port = strtoul(*argv, NULL, 0);
    printf("Will listen on port: %d\n", listen_port);
  }
  else {
     listen_port = DEFAULT_PORT;
     printf("No argument specifying port. Will use default port: %d\n", listen_port);
  }

   ReplyToCommands = 1;
   SendStateChange = 0;
   Status->state = 0;
   Status->prev_state = 0;

      printf("ReplyToCommands = %d\n", ReplyToCommands);


  dserver_init_child();


  ListenSocket = init_command_socket(listen_port);

  for (;;) {

      if (Status->state != 3 ) counter = 0;

      /* As agent for the client, check for data from the true server (mar) */
      if (dserver_input_from_server(command_buf, sizeof(command_buf)) != 0) {
	 if (verbose && (Status->state != 3 || counter++ == 0)) {
	    sprintf(pbuffer, " : x=%d y=%d xbin=%d ybin=%d preset=%lf state=%d\n",Status->x,Status->y,Status->xbin,Status->ybin,Status->preset,Status->state);
	    timestamp(pbuffer);
	 }
	 if (SendStateChange) {
	    switch(Status->state) {
	    case 0:
	    case 7:
	       {
	       char reply[256];
	       char *cptr;
	       sprintf(reply, "%d\n", Status->state);
	       write_socket (ReplySocket, reply, strlen(reply)+1);
	       if (verbose) {
	          if ((cptr=strchr(reply, '\n'))) *cptr = '\0';
		  sprintf(pbuffer," : Sent: (%s)\n", reply);
		  timestamp(pbuffer);
	       }
	       SendStateChange = 0;
	       }
	       break;
	    default:
	       break;
	    }
	 }
      }



      if (dserver_input_from_client(ListenSocket, command_buf, sizeof(command_buf)) != 0) {
      }

    /* As server, check for commands from the true client (outside world) */

  }
  return(0);
}

int init_command_socket(int port)
{
   int socket;

   if (verbose) fprintf(stderr, "init_command_socket.\n");

   socket = open_listen_socket(port);

   if (socket > 0) {
      return (socket);
   }
   else {
      return(-1);
   }
   
}


int command_socket_input(int request_socket, char *command_buffer, int buffer_size, int *actual_socket)
{

   int nread;

   if ((nread = listen_socket(command_buffer, buffer_size, request_socket, actual_socket, 1)) < 0) {
	 nread = 0;
   }

   return(nread);
}

int command_socket_output(int socket, char *command_buffer, int buffer_size)
{

   int result;

   if ((result = write_socket(socket, command_buffer, buffer_size)) < 0) {
	 result = 0;
   }

   return(result);
}

static int dserver_input_from_server (char *command_buffer, int buffer_size)
{
  char *res;
  int new_state;
  if ((res = dserver_read()) == 0) 
    return 0;

  sscanf(res, "%[^,\n]", command_buffer);

  if (strcmp(command_buffer, "is_size") == 0) {
    if (sscanf(res, "%*[^,],%d,%d", &Status->x, &Status->y) != 2) 
      perror ("is_size protocol error");
  } else if (strcmp(command_buffer, "is_size_bkg") == 0) {
    if (sscanf(res, "%*[^,],%d,%d", &Status->x_bkg, &Status->y_bkg) != 2) 
      perror ("is_size_bkg protocol error");
  } else if (strcmp(command_buffer, "is_bin") == 0) {
    if (sscanf(res, "%*[^,],%d,%d", &Status->xbin, &Status->ybin) != 2) 
      perror ("is_bin protocol error");
  } else if (strcmp(command_buffer, "is_preset") == 0) {
    if (sscanf(res, "%*[^,],%lf", &Status->preset) != 1) 
      perror ("is_preset protocol error");
  } else if (strcmp(command_buffer, "is_frameshift") == 0) {
    if (sscanf(res, "%*[^,],%d", &Status->frameshift_size) != 1) 
      perror ("is_frameshift protocol error");
  } else if (strcmp(command_buffer, "is_state") == 0) {
    if (sscanf(res, "%*[^,],%d", &new_state) != 1) {
      perror ("is_state protocol error");
    }
    else {
      if (new_state != Status->state) {
	 Status->prev_state = Status->state;
	 Status->state = new_state;
      }
    }
  }
  else {
      fprintf(stderr, "protocol error - unrecognized command: %s", command_buffer);
  }

  return 1;

}

static int dserver_input_from_client (int listen_socket, char *command_buffer, int buffer_size)
{
   int nread, nsent;
   char *tptr, *nptr;
   int data_socket = 0;
   char cmd[256];
   char reply[256];
   char *cptr;

   if ((nread = command_socket_input(listen_socket, command_buffer, buffer_size, &data_socket)) != 0)  {
      /* take command(s) out of buffer */
      nsent = 0;
      while(nsent < nread) {
	 /* Individual commands may be terminated by \0 or by \n or by \r */
	 /* We don't send the terminator */
	 nptr=strchr((command_buffer+nsent),'\0');
	 tptr=strchr((command_buffer+nsent),'\n');
	 if (tptr != NULL && nptr > tptr) {
	    /* Terminate the string at the \n and then send the null terminated string */
	    nptr = tptr;
	 }
	 tptr=strchr((command_buffer+nsent),'\r');
	 if (tptr != NULL && nptr > tptr) {
	    /* Terminate the string at the \r and then send the null terminated string */
	    nptr = tptr;
	 }
	 *nptr = '\0';

	 /* Check and/or send the null terminated string */
	 if (strlen(command_buffer+nsent)) {

            /* Some commands are processed here */
	    sscanf(command_buffer+nsent, "%[^,\n]", cmd);

	    if (!strcmp((command_buffer+nsent), "get_state")) {
	       sprintf(reply, "%d\n", Status->state);
	       write_socket (data_socket, reply, strlen(reply)+1);
	       if (verbose) {
	          if ((cptr=strchr(reply, '\n'))) *cptr = '\0';
		  sprintf(pbuffer," : Sent: (%s)\n", reply);
		  timestamp(pbuffer);
	       }
	    }
	    else if (!strcmp((command_buffer+nsent), "get_size")) {
	       sprintf(reply, "%d,%d\n", Status->x, Status->y);
	       write_socket (data_socket, reply, strlen(reply)+1);
	       if (verbose) {
	          if ((cptr=strchr(reply, '\n'))) *cptr = '\0';
		  sprintf(pbuffer," : Sent: (%s)\n", reply);
		  timestamp(pbuffer);
	       }
	    }
	    else if (!strcmp((command_buffer+nsent), "get_size_bkg")) {
	       sprintf(reply, "%d,%d\n", Status->x_bkg, Status->y_bkg);
	       write_socket (data_socket, reply, strlen(reply)+1);
	       if (verbose) {
	          if ((cptr=strchr(reply, '\n'))) *cptr = '\0';
		  sprintf(pbuffer," : Sent: (%s)\n", reply);
		  timestamp(pbuffer);
	       }
	    }
	    else if (!strcmp((command_buffer+nsent), "get_frameshift")) {
	       sprintf(reply, "%d\n", Status->frameshift_size);
	       write_socket (data_socket, reply, strlen(reply)+1);
	       if (verbose) {
	          if ((cptr=strchr(reply, '\n'))) *cptr = '\0';
		  sprintf(pbuffer," : Sent: (%s)\n", reply);
		  timestamp(pbuffer);
	       }
	    }
	    else if (!strcmp((command_buffer+nsent), "get_bin")) {
	       sprintf(reply, "%d,%d\n", Status->xbin, Status->ybin);
	       write_socket (data_socket, reply, strlen(reply)+1);
	       if (verbose) {
	          if ((cptr=strchr(reply, '\n'))) *cptr = '\0';
		  sprintf(pbuffer," : Sent: (%s)\n", reply);
		  timestamp(pbuffer);
	       }
	    }
	    else {
	       if (verbose) {
		  sprintf(pbuffer, " : Sending command: %s\n", command_buffer+nsent);
		  timestamp(pbuffer);
	       }
	       dserver_send ((command_buffer+nsent),NULL);

	       if (ReplyToCommands) {
		  /* Process special cases of commands that block */
		  printf("Checking commands for ReplyToCommands = %d\n", ReplyToCommands);

		  if (!strncmp((command_buffer+nsent),"readout", sizeof("readout")-1) ||
		      !strncmp((command_buffer+nsent),"correct", sizeof("correct")-1) ||
		      !strncmp((command_buffer+nsent),"writefile", sizeof("writefile")-1)) {
		     printf("Will reply to this command = %d\n", ReplyToCommands);
		     /* Set flag to send state change */
		     SendStateChange = 1;
		     ReplySocket = data_socket;
		  }
	       }
	    }
	 }
	 /* Count characters sent, including the NULL - note that a \n or \r terminated
	    buffer will need to loop one extra time just to count (but not send) the
	    NULL at the end */
	 nsent += strlen(command_buffer+nsent) + 1;
      }
   }
   return(0);
}
