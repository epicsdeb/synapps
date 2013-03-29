/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
#include <stdio.h>

typedef struct {
    char chars[40];
} string;

typedef char plain_string[40];

int a[2] = {1,2};

int (*pa)[2] = &a;

unsigned b[2] = {0x01234567,0x01234567};

int main()
{
    int i;
    string x = {"bla"};
    plain_string y = "bla";
    char *z = "blub";

    int s = 10;
    int e = 5;
    char (r[s])[e];

    printf("%s\n",x.chars);
    x = (string){"12345678901234567890123456789012345678901234567890"};
    printf("%s\n",x.chars);

#if 0
    x = (string){z};
    printf("%s\n",x.chars);
#endif

    printf("%s\n",y);
    y = (string)"12345678901234567890123456789012345678901234567890";
    printf("%s\n",y);

    pa = (int (*)[2])&a;

    printf("%p,%p,%p,%d,%d\n",pa,&a[0],&a[1],(*pa)[0],(*pa)[1]);
    printf("b=0x");
    for(i=0;i<2;i++)
        printf("%.8x",b[i]);
    printf("\n");
    return 0;
}
