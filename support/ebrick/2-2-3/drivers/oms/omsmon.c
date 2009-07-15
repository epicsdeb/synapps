// Interrupt monitor

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>

#include "oms.h"

main(int argc,char** argv)
{
	int fd,len;
    unsigned int data;


    // Open device
  	fd = open(argv[1],O_RDONLY,S_IRUSR);
    if( fd == -1 )
    {
   		printf("open error - %s\n",argv[1]);
    	exit(0);
   	}

    // Wait forever
    while( 0==0 )
    {
        len = ioctl(fd,OMS_IOCWAITONINT,&data);
        if( len == 0 )
        {
            printf("OMS - interrupt received 0x%4.4X\n",data);
        }
    }

    close(fd);
}
