#include <sys/io.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>

main(int argc,char* argv[])
{
    char data[80];
	int addr,wdat,rdat;

    iopl(3);


    if( argc > 1 )
        sscanf(argv[1],"%x",&addr);
    else
    {
        printf("Enter addr (hex): ");
        fgets(data,sizeof(data),stdin);
        data[strlen(data)-1] = '\0';
        sscanf(data,"%x",&addr);
    }

    if( argc > 2 )
        sscanf(argv[2],"%x",&wdat);
    else
    {
        printf("Enter write data (hex): ");
        fgets(data,sizeof(data),stdin);
        data[strlen(data)-1] = '\0';
        sscanf(data,"%x",&wdat);
    }

    outb(wdat,addr);
    rdat = inb(addr);
    printf("Write/read data to 0x%X (%d/%d, 0x%X/0x%X)\n",addr,wdat,rdat,wdat,rdat);
}
