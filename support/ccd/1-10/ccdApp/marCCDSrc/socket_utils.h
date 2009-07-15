#ifndef socket_utils_h
#define socket_utils_h

/*------------------------------------------------------------------*/
/* function declarations (and prototypes) */



int open_listen_socket(int port);

int open_socket (char *hostname, int port);

int listen_socket(char *buffer, int bufsize, int conn_request_skt, int *data_socket, int poll);

int write_socket(int socket, char *buffer, int size);

void timestamp(const char *string);


#endif /* socket_utils_h */
