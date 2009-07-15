// Linux Device Driver Template/oms
// Userspace test program

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>

#include "autoshut.h"

static int cmds[] = {AS_IOCRBASE,AS_IOCRMAJOR,AS_IOCRMINOR,AS_IOCRNAME,AS_IOCRDEVCNT,AS_IOCRMAXDEVCNT,0};

main() {
	int i,fd,len,wlen,sts,data;
    char send[80],recv[80],device[80];

    printf("Enter device file: ");
    fgets(device,sizeof(device),stdin);
    device[strlen(device)-1] = '\0';

  	fd = open(device,O_RDWR);
    if( fd == -1)
    {
   		printf("open error - %s\n",device);
    	exit(0);
   	}

    for(i=0;cmds[i];++i)
    {
        printf("Command %d returned ",cmds[i]);
        if( cmds[i] == AS_IOCRNAME )
        {
            sts = ioctl(fd,cmds[i],recv);
            if( sts ) {printf("Command %d failed\n",cmds[i]);continue;}
            printf("\"%s\" \n",recv);
        }
        else
        {
            sts = ioctl(fd,cmds[i],&data);
            if( sts ) {printf("Command %d failed\n",cmds[i]);continue;}
            printf("%d 0x%2.2X\n",data,data);
        }
    }

    close(fd);
}
