#include <sys/io.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>

main(int argc,char* argv[])
{
    char data[80];
	int i,w,r,addr1,addr2,cnt,err,delay;

    iopl(3);

    if( argc > 1 ) sscanf(argv[1],"%x",&addr1);
    if( argc > 2 ) sscanf(argv[2],"%x",&addr2);
    if( argc > 3 ) sscanf(argv[3],"%d",&cnt);

    for( err=0,i=0; i<cnt; ++i )
    {
        w=(i&1)?0x55:0xAA;
        outb(w,addr1);
        r = inb(addr2);
        if( w==r ) continue; else ++err;
    }

    printf("Write/read V.II error count %d after %d iterations\n",err,i);
}
