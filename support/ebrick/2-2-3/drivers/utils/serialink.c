// serialink.c
// 
// This application implements the serial-link protocol between the EBRICK and
// the BC-059 Generic Digital IO board.
// 
// Author: David M. Kline, ANL, APS, AES, BCDA.
//
// 2008.02.15  DMK  Initial FPGA serial-link protocol version 2.0.
// 
// 
//  Serial-Link Protocol V1.4 format:
//  +-------------------------------------------------------------+
//  |44               13|12         11|10          6|5           0|
//  +-------------------------------------------------------------+
//  | Preset (optional) |   Address   |   Function  |   Counter   |
//  +-------------------------------------------------------------+
//
//  Serial-Link Protocol V2.0 format:
//  +-------------------------------------------------------------+
//  |43               12|11          9|8           5|4           0|
//  +-------------------------------------------------------------+
//  | Preset (optional) | Subfunction |   Function  |   Counter   |
//  +-------------------------------------------------------------+
// 
// 

#include <sys/io.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>

#define ENA        (0x4)
#define CLK        (0x2)
#define DAT        (0x1)
#define DIS        (0x0)

#define CTR_BIT    (5)
#define FNC_BIT    (4)
#define SFN_BIT    (3)
#define SLD_SIZ    ((FNC_BIT+CTR_BIT+SFN_BIT)*2)
#define DAT_BIT    (32)
#define DAT_SIZ    (DAT_BIT*2)

main(int argc,char* argv[])
{
    int i,addr,ctr,fnc,sfn,pre,sld,dat;
    unsigned int msk,mask = 0xFF;

    iopl(3);

    sscanf(argv[1],"%x",&addr);
    sscanf(argv[2],"%d",&ctr);
    sscanf(argv[3],"%d",&fnc);
    sscanf(argv[4],"%d",&sfn);
    if(argc > 5) sscanf(argv[5],"%d",&pre); else pre=0;

    outb(DIS,addr);
    outb(ENA,addr);

    // Send preset
    if( pre )
    {
        msk = 1 << ((DAT_SIZ/2)-1);
        sld = pre;

        for(i=0; i<DAT_SIZ; ++i)
        {
            dat = ENA;
            if((i&1)==0)
            {
                dat |= (CLK | ((msk & sld) ? 1 : 0) );
                sld <<= 1;
            }
            dat &= mask;
            outb(dat,addr);
        }
    }

    // Send command
    msk = 1 << ((SLD_SIZ/2)-1);
    fnc = (fnc << CTR_BIT);
    sld = (sfn << (CTR_BIT+FNC_BIT)) | (fnc) | (ctr);

    for(i=0; i<SLD_SIZ; ++i)
    {
        dat = ENA;
        if((i&1)==0)
        {
            dat |= (CLK | ((msk & sld) ? 1 : 0) );
            sld <<= 1;
        }
        dat &= mask;
        outb(dat,addr);
    }
    outb(DIS,addr);
}
