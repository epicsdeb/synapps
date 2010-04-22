#include <stdio.h>
#include <sys/io.h>

int main(int argc, char **argv)
{
unsigned char  v;
unsigned short p;

    if ( argc < 2 || 1 != sscanf(argv[1],"%hi",&p) ) {
        fprintf(stderr,"Usage: %s <port>\n",argv[0]);
        return 1;
    }

    if ( iopl(3) ) {
        perror("iopl");
        return 1;
    }

    v = inb(p);

    printf("0x%02x\n",v);
    return 0;
}

