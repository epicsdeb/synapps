// Auto shutdown monitor

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>

#include "autoshut.h"

static char* cmd = "/sbin/poweroff";
static char* args[] = {"-f",NULL};

main(int argc,char** argv)
{
	int fd,len;
    char data[20];


    // Open device
  	fd = open(argv[1],O_RDWR);
    if( fd == -1 )
    {
   		printf("open error - %s\n",argv[1]);
    	exit(0);
   	}

    // Wait forever
    len = ioctl(fd,AS_IOCWAITONINT,data);
    if( len == 0 && strcmp(data,"autoshut-shutdown") == 0)
    {
        execv(cmd,args);
    }

    close(fd);
}
