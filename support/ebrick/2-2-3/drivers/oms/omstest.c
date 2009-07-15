// Linux Device Driver Template/oms
// Userspace test program

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>

#include "oms.h"

static int cmds[] = {OMS_IOCRBASE,OMS_IOCRMAJOR,OMS_IOCRMINOR,OMS_IOCRNAME,OMS_IOCRDEVCNT,OMS_IOCRMAXDEVCNT,0};

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
        if( cmds[i] == OMS_IOCRNAME )
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

    printf("Enter Command: ");
    fgets(send,80,stdin);
    send[strlen(send)-1] = '\0';

	wlen = strlen(send);
	len = write(fd,send,wlen);
	if( len == -1 )
    {
		printf("write error - %s\n",device);
		exit(1);
	}
	printf("Command '%s' (%d) written to %s\n",send,wlen,device);

	len = read(fd,recv,sizeof(recv));
	if( len == -1 )
    {
		printf("read error - %s\n",device);
		exit(1);
	}

    printf("returned %d bytes\n",len);
    for(i=0; i<len; ++i)
        if( iscntrl(recv[i]) )
            printf(" 0x%2.2X ",recv[i]); 
        else
            printf("%c",recv[i]); 

    printf("\n");

    close(fd);
}
