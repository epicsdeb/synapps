// Interrupt monitor

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>

#include "omm.h"

main(int argc,char** argv)
{
	int fd,len;
    int data;


    // Open device
  	fd = open(argv[1],O_RDWR);
    if( fd == -1 )
    {
   		printf("open error - %s\n",argv[1]);
    	exit(0);
   	}

    // Wait forever
    while( 0==0 )
    {
        len = ioctl(fd,OMM_IOCWAITONINT,&data);
        if( len == 0 )
        {
            printf("OMM - group signal received from scaler %d\n",data);
        }
    }

    close(fd);
}
