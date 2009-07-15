#include <sys/io.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>

main(int argc,char* argv[])
{
    char data[80];
	int s=0,i,addr,rdat,cnt=1;

    iopl(3);

    if( argc > 1)
        sscanf(argv[1],"%x",&addr);
    else
    {
        printf("Enter addr (hex): ");
        fgets(data,sizeof(data),stdin);
        data[strlen(data)-1] = '\0';
        sscanf(data,"%x",&addr);
    }

    if( argc > 2)
    {
        s=1;
        sscanf(argv[2],"%d",&cnt);
    }

    for(i=0; i<cnt; ++i)
    {
        sleep(s);
        rdat = inb(addr);
        printf("%3.3d Read data (%3d / 0x%2.2X) from 0x%X\n",i,rdat,rdat,addr);
    }
}
