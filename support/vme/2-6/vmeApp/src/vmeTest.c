/*  vmeTest  (was modtest) */
/*  vme test program          F. Lenkszus */
/* an interactive program that accesses vme modules at the regiser level */

#include <vxWorks.h>
#include <stdioLib.h>
#include <ioLib.h>
#include <vme.h>
#include <iv.h>
#include <sysLib.h>
#include <vxLib.h>
#include <logLib.h>
#include <intLib.h>

#include <epicsExport.h>

#define FEXIT   1

static void  ttylinemode();
static void  ttycharmode();
static void  ttyinflush();
static int   fmenu();

static void  srandom();
static long  random();
static char* initstate();

static int setbase();
static int setla();
static int setoffset();
static int listsettings();
static int fwritereg();
static int freadreg();
static int fwrreg();
static int loopread();
static int loopwrite();
static int looprw();
static int rammenu();
static int bitpatmenu();
static int twopatternwrite();
static int a32menu();
static int exitx();

static int setramaoff();
static int setramdoff();
static int setramsz();
static int setramwsz();
static int setramdadd();
static int setramdwords();
static int fillram();
static int dumpram();
static int listrsettings();
static int wraddtest();
static int raddtest();
static int wrrottest();
static int wrrandtest();
static int rrandtest();
static int randchk();
static int wronestest();

static int setintvector();
static void inthandler();


static int bpramsel();
static int bpbanksel();
static int bpfillram();
static int bpdumpram();
static int listbpsettings();
static int bpwraddtest();
static int bpwrrottest();
static int bpwronestest();
static int bprandchk();

static int seta32base();
static int seta32offset();
static int lista32settings();
static int freada32reg();
static int fwritea32reg();
static int fwra32reg();
static int loopa32read();
static int loopa32write();
static int loopa32rw();
static int a32rammenu();
static int seta32ramaoff();
static int seta32ramsz();
static int seta32ramdwords();
static int seta32fast();
static int filla32ram();
static int dumpa32ram();
static int lista32rsettings();
static int a32wraddtest();
static int a32wrrottest();
static int a32wronestest();
static int a32randchk();
static int a32wrrandtest();
static int a32rrandtest();
static int a32raddtest();
static int seta32ramdadd();
static int seta32intvector();
static void a32inthandler();

static void vmeTest_Register();
epicsExportRegistrar(vmeTest_Register);

static unsigned short   *modbase;   /* module base address */
static unsigned short   vxila;      /* vxi logical address */
static unsigned short   reg;        /* register offset */
static unsigned short   writeval;   /* default write value */
static unsigned short   radd;       /* ram address offset */
static unsigned short   rdreg;      /* ram data reg offset */
static unsigned short   rsize;      /* ram size in words */
static unsigned short   rwsize;     /* ram word size in bytes */
static unsigned short   rdadd;      /* ram dump address */
static unsigned short   rdwords;    /* number of words to dump */
static unsigned short   ivect;      /* interrupt vector register offset */
static unsigned short   iresetreg;  /* offset of register to write to reset interrupt */
static unsigned short   irbits;     /* bits to or into reset interrupt pattern */
static unsigned short   ipatmask;   /* interrupt pattern and mask */
static unsigned short   bpnum;      /* bit pattern generator # */
static unsigned short   bpbank;     /* bit pattern bank */
static unsigned short   seed;       /* random num generator seed */

static unsigned long *a32base;      /* module a32 base address */
static unsigned long *a32ibase;     /* module a32 interrupt base add */
static unsigned long a32reg;        /* a32 register offset */
static unsigned long a32writeval;   /*  default a32 write value */
static unsigned long a32radd;       /* ram address offset */
static unsigned long a32rsize;      /* ram size in words */
static unsigned long a32rdadd;      /* ram dump address */
static unsigned long a32rdwords;    /* number of words to dump */
static unsigned long a32seed;       /* random num generator seed */
static unsigned long a32fast;       /* 1 = fast read/write */
static unsigned long a32ivect;      /* interrupt vector register offset */
static unsigned long a32iresetreg;  /* offset of register to write to reset interrupt */
static unsigned long a32irbits;     /* bits to or into reset interrupt pattern */
static unsigned long a32ipatmask;   /* interrupt pattern and mask */

struct  menu_prompt {    /* defines a prompt for a menu */
   char  id;             /* char to input for this choice */
   int   val;            /* value to return for this prompt */
   int   (*proc)();      /* proc to execute for this prompt */
   int   key;            /* key code for this prompt */
   char  *pmt;           /* prompt text */
};

struct   fmenu {
   int   nprompts;
   char  *title;
   char  *prompt;
   struct   menu_prompt *pmt;
};

static struct menu_prompt menu_prompts[] = {
   { 'b', 0,  setbase,         0, "Set module base address" },
   { 'v', 1,  setla,           0, "Set VXI module Logical Address" },
   { 'o', 2,  setoffset,       0, "Set register offset" },
   { 'l', 3,  listsettings,    0, "List settings" },
   { 'r', 4,  freadreg,        0, "Read Register" },
   { 'w', 5,  fwritereg,       0, "Write Regiser" },
   { 'e', 6,  fwrreg,          0, "Write-read Register" },
   { '1', 7,  loopread,        0, "Loop on read" },
   { '2', 8,  loopwrite,       0, "Loop on write" },
   { '3', 9,  looprw,          0, "Loop on write-read" },
   { 'm', 10, rammenu,         0, "Ram Tests" },
   { '9', 11, bitpatmenu,      0, "BPM Bit pattern tests" },
   { 'i', 12, setintvector,    0, "Set Interrupt Vector" },
   { 't', 13, twopatternwrite, 0, "2 pattern register write" },
   { 'a', 14, a32menu,         0, "A32/D32 tests" },
   { 'x', 3,  exitx,           0, "Exit" }
};

static   struct   fmenu mm = {
   sizeof(menu_prompts)/sizeof (struct menu_prompt),
   "Module Test Program",
   "Enter Choice",
   menu_prompts
};

static struct menu_prompt ram_prompts[] = {
   { 'a', 0, setramaoff, 0, "Set ram address register offset" },
   { 'r', 0, setramdoff, 0, "Set ram data register offset" },
   { 's', 0, setramsz, 0, "Set ram size in words" },
   { 'w', 0, setramwsz, 0, "Set ram word size" },
   { 'b', 0, setramdadd, 0, "Set ram dump address" },
   { 'n', 0, setramdwords, 0, "Set number of words to dump" },
   { 'f', 0, fillram, 0, "Fill ram" },
   { 'd', 0, dumpram, 0, "Dump ram" },
   { 'l', 0, listrsettings, 0, "List settings" },
   { '1', 0, wraddtest, 0, "Write-read ram address test" },
   { '2', 0, wrrottest, 0, "Rotate pattern ram test" },
   { '3', 0, wronestest, 0, "Shifting ones test" },
   { '4', 0, randchk, 0, "Read and check" },
   { '5', 0, wrrandtest, 0, "Random pattern test" },
   { '6', 0, rrandtest, 0, "Read random pattern test" },
   { '7', 0, raddtest, 0, "Read ram address test" },
   { 'x', 0, exitx, 0, "Exit" }
};


static struct fmenu rm = {
   sizeof(ram_prompts)/sizeof (struct menu_prompt),
   "Ram Tests",
   "Enter Choice",
   ram_prompts
};

static struct menu_prompt bp_prompts[] = {
   { 'o', 0, bpramsel, 0, "Select Bit Pattern Generator" },
   { 'b', 0, bpbanksel, 0, "Select Bank" },
   { 'n', 0, setramdwords, 0, "Set number of words to dump" },
   { 'f', 0, bpfillram, 0, "Fill ram" },
   { 'd', 0, bpdumpram, 0, "Dump ram" },
   { 'l', 0, listbpsettings, 0, "List settings" },
   { '1', 0, bpwraddtest, 0, "Write-read ram address test" },
   { '2', 0, bpwrrottest, 0, "Rotate pattern ram test" },
   { '3', 0, bpwronestest, 0, "Shifting ones test" },
   { '4', 0, bprandchk, 0, "Read and check" },
   { 'x', 0, exitx, 0, "Exit" }
};

static struct fmenu bpm = {
   sizeof(bp_prompts)/sizeof (struct menu_prompt),
   "BPM Bit Pattern Tests",
   "Enter Choice",
   bp_prompts
};

static struct menu_prompt a32_prompts[] = {
   { 'b', 0, seta32base, 0, "Set module A32 base address" },
   { 'o', 1, seta32offset, 0, "Set a32 register offset (hex byte address)" },
   { 'l', 2, lista32settings, 0, "List settings" },
   { 'r', 3, freada32reg, 0, "Read Register" },
   { 'w', 4, fwritea32reg, 0, "Write Regiser" },
   { 'e', 5, fwra32reg, 0, "Write-read Register" },
   { '1', 5, loopa32read, 0, "Loop on read" },
   { '2', 6,  loopa32write, 0, "Loop on write" },
   { '3', 7, loopa32rw, 0, "Loop on write-read" },
   { 'm', 8, a32rammenu, 0, "A32 Ram Tests" },
   { 'i', 12, seta32intvector, 0, "Set Interrupt Vector" },
   { 'x', 0, exitx, 0, "Exit" }
};

static struct fmenu a32 = {
   sizeof(a32_prompts)/sizeof (struct menu_prompt),
   "A32/D32 Tests",
   "Enter Choice",
   a32_prompts
};

static struct menu_prompt a32ram_prompts[] = {
   { 'a', 0, seta32ramaoff, 0, "Set ram start offset" },
   { 's', 0, seta32ramsz, 0, "Set ram size in words" },
   { 'b', 0, seta32ramdadd, 0, "Set ram dump address" },
   { 'n', 0, seta32ramdwords, 0, "Set number of words to dump" },
   { 't', 0, seta32fast, 0, "Set turbo read/write mode" },
   { 'f', 0, filla32ram, 0, "Fill ram" },
   { 'd', 0, dumpa32ram, 0, "Dump ram" },
   { 'l', 0, lista32rsettings, 0, "List settings" },
   { '1', 0, a32wraddtest, 0, "Write-read ram address test" },
   { '2', 0, a32wrrottest, 0, "Rotate pattern ram test" },
   { '3', 0, a32wronestest, 0, "Shifting ones test" },
   { '4', 0, a32randchk, 0, "Read and check" },
   { '5', 0, a32wrrandtest, 0, "Random pattern test" },
   { '6', 0, a32rrandtest, 0, "Read random pattern test" },
   { '7', 0, a32raddtest, 0, "Read ram address test" },
   { 'x', 0, exitx, 0, "Exit" }
};


static struct fmenu a32rm = {
   sizeof(a32ram_prompts)/sizeof (struct menu_prompt),
   "A32 Ram Tests",
   "Enter Choice",
   a32ram_prompts
};

/* Here simply to expose the module */
static void vmeTest_Register(void)
{
   return;
}

void vmeTest()
{
   int   choice;
   int   selected;

   reg = 0;
   selected = 0;
   while( selected == 0 )
   {
      ttycharmode();
      ttyinflush();
      selected = (*(mm.pmt[choice=fmenu(&mm)].proc))();
   }
   ttylinemode();
}

static int rammenu()
{
   int   choice;
   int   selected;

   if(( rwsize < 1) || ( rwsize > 2 ))
      rwsize = 1;
   selected = 0;
   while (selected == 0) {
      ttycharmode();
      ttyinflush();
      selected = (*(rm.pmt[choice=fmenu(&rm)].proc))();
   }
   return(0);
}

static int a32menu()
{
   int   choice;
   int   selected;

   selected = 0;
   while (selected == 0) {
      ttycharmode();
      ttyinflush();
      selected = (*(a32.pmt[choice=fmenu(&a32)].proc))();
   }
   return(0);
}

static int a32rammenu()
{
   int   choice;
   int   selected;

   selected = 0;
   while (selected == 0) {
      ttycharmode();
      ttyinflush();
      selected = (*(a32rm.pmt[choice=fmenu(&a32rm)].proc))();
   }
   return(0);
}

static int exitx()
{
   return(FEXIT);
}

static void ttyinflush()
{
   int   status;
   int   dummy;

   status = ioctl(0, FIORFLUSH, (int)&dummy);

}

static void ttycharmode()
{
   int   status;
   int   ttymode;
   int   nmode;
   int   dummy;

   ttymode  = ioctl(0, FIOGETOPTIONS, (int)&dummy);
   nmode    = ttymode & ~OPT_LINE;
   status   = ioctl(0, FIOSETOPTIONS, nmode);

}
static void ttylinemode()
{
   int   status;
   int   ttymode;
   int   nmode;
   int   dummy;

   ttymode  = ioctl(0, FIOGETOPTIONS, (int)&dummy);
   nmode    = ttymode | OPT_LINE;
   status   = ioctl(0, FIOSETOPTIONS, nmode);

}

static int  setbase()
{
   unsigned long  addr;

   ttylinemode();
   printf("Enter module base address in hex:  ");
   scanf("%lx", &addr);
   if (sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO, (char *)addr,
       (char **)(&modbase)) == ERROR) {
      printf("setbase: Can't find short address space\n");
   }
   printf("Base address set to %p\n", modbase);
   getchar();
   ttycharmode();
   return(0);
}

static int  setla()
{
   unsigned long  addr;

   ttylinemode();
   printf("Enter VXI module Logical Address in hex:  ");
   scanf("%hx", &vxila);
   if ( vxila > 254) {
      printf("VXI LA must be between 0 and 254\n");
      return(0);
   }
   addr = 0xc000 + (vxila << 6);
   if (sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO, (char *)addr,
       (char **)(&modbase)) == ERROR) {
      printf("setbase: Can't find short address space\n");
   }
   printf("Base address set to %p\n", modbase);
   getchar();
   ttycharmode();
   return(0);
}

static int setoffset()
{
   ttylinemode();
   printf("Enter register offset in hex:  ");
   scanf("%hx", &reg);
   printf("Register offset set to %x\n", reg);
   getchar();
   ttycharmode();
   return(0);
}

static int listsettings()
{
   printf("Base address set to %p\n", modbase);
   printf("Register offset set to %x\n", reg);
   printf("Default write value set to %x\n", writeval);
   printf("Interrupt vector register set to %x\n", ivect);
   printf("Interrupt reset register set to %x\n", iresetreg);
   printf("Interrupt rset bits  set to %x\n", irbits);
   printf("Interrupt reset register read mask set to %x\n", ipatmask);
   return(0);
}

static int fmenu(
struct   fmenu *pmenu)
{
   int i;
   int   choice;

   while (1) {
   printf("%s\n\n", pmenu->title);

   for( i=0; i < pmenu->nprompts; i++) {
      printf("%c -- %s\n", pmenu->pmt[i].id, pmenu->pmt[i].pmt);
   }

   printf("\n%s:  ", pmenu->prompt);
   choice = getchar();
   printf("\n");
   for ( i=0; i < pmenu->nprompts; i++) {
      if ( (char)choice == pmenu->pmt[i].id)
         return(i);
   }
   printf("\n%c is not a valid choice\n", (char)choice);
   }

}

static short  readreg(
unsigned short *p)
{
   unsigned short val;
   if (vxMemProbe((char *)p, READ, 2, (char *)&val) == ERROR) {
      printf("Bus Error on read from %p\n", p);
      return(ERROR);
   }
   return(val);
}

static unsigned long  reada32reg(
unsigned long *p)
{
   unsigned long val;
   if(a32fast)
      return(*p);
   if (vxMemProbe((char *)p, READ, 4, (char *)&val) == ERROR) {
      printf("Bus Error on read from %p\n", p);
      return(ERROR);
   }
   return(val);
}

static unsigned short  *setadd(
unsigned short *base,
unsigned short offset)
{
   return(base + offset/2);
}

static unsigned long  *seta32add(
unsigned long  *base,
unsigned long offset)
{
   return(base + offset/4);
}

static int writereg(
unsigned short *p,
unsigned short val)
{
   if (vxMemProbe((char *)p, WRITE, 2, (char *)&val) == ERROR) {
      printf("Bus Error on write from %p\n", p);
      return(ERROR);
   }
   return(OK);
}

static int writea32reg(
unsigned long *p,
unsigned long val)
{
   if(a32fast)
      *p = val;
   else
      if (vxMemProbe((char *)p, WRITE, 4, (char *)&val) == ERROR) {
         printf("Bus Error on write from %p\n", p);
         return(ERROR);
      }
   return(OK);
}

static int freadreg()
{
   unsigned short val;
   unsigned short *p;

   p = modbase + reg/2;
   val = readreg(p);
   printf("Register %p = %x\n", p, val);
   return(0);
}

static int fwritereg()
{
   unsigned short *p;

   p = modbase + reg/2;
   printf("Enter value to write hex:  ");
   scanf("%hx", &writeval);
   getchar();
   writereg(p, writeval);
   return(0);
}
static int fwrreg()
{
   unsigned short *p;

   p = modbase + reg/2;
   printf("Enter value to write hex:  ");
   scanf("%hx", &writeval);
   getchar();
   if (writereg(p, writeval) == OK)
      freadreg();
   return(0);
}

static int loopread()
{
   int   status;
   int   nttychars;
   unsigned short val;
   unsigned short *p;

   p = modbase + reg/2;
   printf("Looping on read of Register %p \n", p);
   nttychars=0;
   while (1) {
      val = readreg(p);
      status = ioctl (0, FIONREAD, (int)&nttychars);
      if ( nttychars != 0 )
         break;
   }
   getchar();
   return(0);
}

static int loopwrite()
{
   int   status;
   int   nttychars;
   unsigned short *p;

   p = modbase + reg/2;
   printf("Looping on write of Register %p \n", p);
   nttychars=0;
   while (1) {
      writereg(p, writeval);
      status = ioctl (0, FIONREAD, (int)&nttychars);
      if ( nttychars != 0 )
         break;
   }
   getchar();
   return(0);
}

static  int looprw()
{
   int   status;
   int   nttychars;
   unsigned short val;
   unsigned short *p;

   p = modbase + reg/2;
   printf("Looping on write-read of Register %p \n", p);
   nttychars=0;
   while (1) {
      writereg(p, writeval);
      val = readreg(p);
      status = ioctl (0, FIONREAD, (int)&nttychars);
      if ( nttychars != 0 )
         break;
   }
   getchar();
   return(0);
}

static int setramaoff()
{
   printf("Enter ram address register offset in hex:  ");
   scanf("%hx", &radd);
   printf("Ram address register offset set to %x\n", reg);
   getchar();
   return(0);
}

static int setramdoff()
{
   printf("Enter ram data register offset in hex:  ");
   scanf("%hx", &rdreg);
   printf("Ram data register offset set to %x\n", rdreg);
   getchar();
   return(0);
}

static int setramsz()
{
   printf("Enter ram size in words (dec):  ");
   scanf("%hd", &rsize);
   printf("Ram size set to %d words\n", rsize);
   getchar();
   return(0);
}

static int  setramwsz()
{
   printf("Enter ram word size in bytes  (1|2):  ");
   scanf("%hd", &rwsize);
   if ( rwsize < 1 ) {
      printf("Oops, word size of %d is nonsense\n", rwsize);
      printf("Setting word size to 1 byte\n");
      rwsize = 1;
   } else if ( rwsize > 2) {
      printf("Oops, %d is too big,  setting to 2 bytes\n", rwsize);
      rwsize = 2;
   }
   printf("Ram size set to %d words\n", rwsize);
   getchar();
   return(0);
}

static int setramdadd()
{
   printf("Enter ram dump address in hex:  ");
   scanf("%hx", &rdadd);
   printf("Ram dump address set to %x\n", rdadd);
   getchar();
   return(0);
}

static int setramdwords()
{
   printf("Enter enter number of words to dump in hex:  ");
   scanf("%hx", &rdwords);
   printf("Number of words to dump set to %x\n", rdwords);
   getchar();
   return(0);
}

static int listrsettings()
{
   printf("Base address set to %p\n", modbase);
   printf("Address register offset set to %x\n", radd);
   printf("Data register offset set to %x\n", rdreg);
   printf("Ram size set to %d words\n", rsize);
   printf("Ram word size set to %d bytes\n", rwsize);
   printf("Ram default fill  value set to %x\n", writeval);
   printf("Ram dump address set to %x\n", rdadd);
   printf("Number of words to dump set to %x\n", rdwords);
   return(0);
}

static int dumpram()
{
   int i,j,k;
   unsigned short *pa, *pd;
   unsigned short val;
   int line;
   char  loop;

   loop = 'y';
   line=8;
   pa = setadd( modbase, radd);
   pd = setadd( modbase, rdreg);

   if ( writereg(pa, rdadd) != OK )
      return(0);
   k=0;
   while ( loop  == 'y' && k < (rsize - rdadd) ) {
      i=0;
      while( i < rdwords) {
      printf("\n%x  ", rdadd + k);
      for ( j=0; j < line && i < rdwords; j++, i++, k++) {
         val = readreg(pd);
         if ( rwsize == 1)
            val &= 0xff;
         printf("  %x", val);
      }
      }
      printf("\n\nContinue? (y|n): ");
      loop = (char) getchar();
      printf("\n");
   }
   return(0);
}

static int fillram()
{
   int i;
   unsigned short *pa, *pd;

   pa = setadd( modbase, radd);
   pd = setadd( modbase, rdreg);

   printf("Enter ram fill value in hex:  ");
   scanf("%hx", &writeval);
   getchar();

   if ( writereg(pa, 0) != OK )
      return(0);
   for ( i=0; i < rsize; i++ ) {
      if ( writereg(pd, writeval) != OK )
         break;
   }

   return(0);
}

static int wraddtest()
{
   int   status;
   int   nttychars;
   unsigned short val;
   unsigned short chk;
   int i;
   unsigned short *pa, *pd;
   int   iter;

   printf("Looping on write-read address test\n");
   nttychars=0;
   iter=0;
   pa = setadd( modbase, radd);
   pd = setadd( modbase, rdreg);
   while (1) {

      if ( writereg(pa, 0) != OK )
         return(0);
      for ( i=0; i < rsize; i++ ) {
         if ( writereg(pd, i) != OK )
            break;
      }

      if ( writereg(pa, 0) != OK )
         return(0);

      for ( i=0; i < rsize; i++ ) {
         val = readreg(pd);
         chk = i;
         if ( rwsize == 1) {
            val &= 0xff;
            chk &= 0xff;
         }
         if( val != chk) {
            printf("Error at %x -- wrote %x  read %x\n",
               i, chk, val);
         }

      }
      iter++;
      status = ioctl (0, FIONREAD, (int)&nttychars);
      if ( nttychars != 0 )
         break;
   }
   getchar();
   printf("\n%d iterations completed\n", iter);
   return(0);
}

static int raddtest()
{
   int   status;
   int   nttychars;
   unsigned short val;
   unsigned short chk;
   int i;
   unsigned short *pa, *pd;
   int   iter;

   printf("Looping on read address test\n");
   nttychars=0;
   iter=0;
   pa = setadd( modbase, radd);
   pd = setadd( modbase, rdreg);
   while (1) {


      if ( writereg(pa, 0) != OK )
         return(0);

      for ( i=0; i < rsize; i++ ) {
         val = readreg(pd);
         chk = i;
         if ( rwsize == 1) {
            val &= 0xff;
            chk &= 0xff;
         }
         if( val != chk) {
            printf("Error at %x -- wrote %x  read %x\n",
               i, chk, val);
         }

      }
      iter++;
      status = ioctl (0, FIONREAD, (int)&nttychars);
      if ( nttychars != 0 )
         break;
   }
   getchar();
   printf("\n%d iterations completed\n", iter);
   return(0);
}

static int wrrottest()
{
   int   status;
   int   nttychars;
   unsigned short val,val1;
   unsigned short chk;
   unsigned short rotval;
   int i;
   unsigned short *pa, *pd;
   int   iter;

   printf("Enter value to rotate left in hex:  ");
   scanf("%hx", &rotval);
   getchar();

   printf("Looping on rotate pattern left test\n");
   nttychars=0;
   iter=0;
   pa = setadd( modbase, radd);
   pd = setadd( modbase, rdreg);
   while (1) {

      if ( writereg(pa, 0) != OK )
         return(0);
      val = rotval;
      if ( rwsize == 1)
         val &= 0xff;
      for ( i=0; i < rsize; i++ ) {
         if ( writereg(pd, val) != OK )
            break;
         val1 = val << 1;
         if ( rwsize == 1) {
            if ( val & 0x80 )
               val1 |= 1;
            val1 &= 0xff;
         } else {
            if ( val & 0x8000 )
               val1 |= 1;
         }
         val = val1;
      }

      if ( writereg(pa, 0) != OK )
         return(0);

      chk = rotval;
      if ( rwsize == 1)
         chk &= 0xff;

      for ( i=0; i < rsize; i++ ) {
         val = readreg(pd);
         if ( rwsize == 1) {
            val &= 0xff;
         }
         if( val != chk) {
            printf("Error at %x -- wrote %x  read %x\n",
               i, chk, val);
         }
         val1 = chk << 1;
         if ( rwsize == 1) {
            if ( chk & 0x80 )
               val1 |= 1;
            val1 &= 0xff;
         } else {
            if ( chk & 0x8000 )
               val1 |= 1;
         }
         chk = val1;

      }
      iter++;
      status = ioctl (0, FIONREAD, (int)&nttychars);
      if ( nttychars != 0 )
         break;
   }
   getchar();
   printf("\n%d iterations completed\n", iter);
   return(0);
}

static int wronestest()
{
   int   status;
   int   nttychars;
   unsigned short val;
   unsigned short chk;
   int i;
   unsigned short *pa, *pd;
   int   iter;
   unsigned short pat;

   printf("Looping on shifting ones test\n");
   nttychars=0;
   iter=0;
   pat = 1;
   pa = setadd( modbase, radd);
   pd = setadd( modbase, rdreg);
   while (1) {

      if ( writereg(pa, 0) != OK )
         return(0);
      val = pat;
      if ( rwsize == 1)
         val &= 0xff;
      for ( i=0; i < rsize; i++ ) {
         if ( writereg(pd, val) != OK )
            break;
         if ( rwsize == 1) {
            if ( val &  0x80 )
               val <<= 1;
            else {
               val <<=1;
               val |= 1;
            }
            val &= 0xff;
         } else {
            if ( val & 0x8000 )
               val <<=1;
            else {
               val <<=1;
               val |= 1;
            }
         }
      }

      if ( writereg(pa, 0) != OK )
         return(0);

      chk = pat;
      if ( rwsize == 1)
         chk &= 0xff;

      for ( i=0; i < rsize; i++ ) {
         val = readreg(pd);
         if ( rwsize == 1) {
            val &= 0xff;
         }
         if( val != chk) {
            printf("Error at %x -- wrote %x  read %x\n",
               i, chk, val);
         }
         if ( rwsize == 1) {
            if ( chk &  0x80 )
               chk <<= 1;
            else {
               chk <<=1;
               chk |= 1;
            }
            chk &= 0xff;
         } else {
            if ( chk & 0x8000 )
               chk <<=1;
            else {
               chk <<=1;
               chk |= 1;
            }
         }

      }
      iter++;
      status = ioctl (0, FIONREAD, (int)&nttychars);
      if ( nttychars != 0 )
         break;
   }
   getchar();
   printf("\n%d iterations completed\n", iter);
   return(0);
}

static int randchk()
{
   int   status;
   int   nttychars;
   unsigned short val;
   unsigned short chk;
   int i;
   unsigned short *pa, *pd;
   int   iter;

   printf("Enter value to compare to in hex:  ");
   scanf("%hx", &chk);
   getchar();

   printf("Looping on read and check test\n");
   nttychars=0;
   iter=0;
   pa = setadd( modbase, radd);
   pd = setadd( modbase, rdreg);
   while (1) {

      if ( writereg(pa, 0) != OK )
         return(0);

      if ( rwsize == 1)
         chk &= 0xff;

      for ( i=0; i < rsize; i++ ) {
         val = readreg(pd);
         if ( rwsize == 1) {
            val &= 0xff;
         }
         if( val != chk) {
            printf("Error at %x -- wrote %x  read %x\n",
               i, chk, val);
         }

      }
      iter++;
      status = ioctl (0, FIONREAD, (int)&nttychars);
      if ( nttychars != 0 )
         break;
   }
   getchar();
   printf("\n%d iterations completed\n", iter);
   return(0);
}

static int junk()
{
   char  buf[128];
   int   status;
   int   nttychars;
   int   ttymode;
   int   nmode;
   int   dummy;

   while (1) {

      nttychars=0;
      ttymode = ioctl(0, FIOGETOPTIONS, (int)&dummy);
      nmode = ttymode & ~OPT_LINE;
      status = ioctl(0, FIOSETOPTIONS, nmode);
      while (nttychars == 0 ) {
         status = ioctl (0, FIONREAD, (int)&nttychars);
      }
      printf("nttychars = %d\n", nttychars);
      read(0, buf, 1);
      printf("input = %c\n", buf[0]);
   }
}

static void inthandler(
int     param)
{
   unsigned short pat;
   unsigned short *p;

        logMsg("tic\n",0,0,0,0,0,0);
   p = modbase + iresetreg/2;
   pat = *p & ipatmask;
   *p = pat | irbits;
}

static void a32inthandler(
int     param)
{
   unsigned long pat;
   unsigned long *p;

        logMsg("tic\n",0,0,0,0,0,0);
   p = a32ibase + a32iresetreg/4;
   pat = *p & a32ipatmask;
   *p = pat | a32irbits;
}

static int setintvector()
{
   int vec;
   unsigned short *p;

   ttylinemode();
   printf("Enter interrupt number in hex:  ");
   scanf("%x", &vec);
   if ( vec < 0 || vec > 255) {
      printf("interrupt number must be between 0 and 255 \n");
      return(-1);
   }
   if ( intConnect( INUM_TO_IVEC(vec), inthandler, 0) != OK ) {
                printf("setintvector: intConnect failed for inum %d\n", vec);
                return(-1);
        }
   printf("Enter interrupt vector register offset in hex:  ");
   scanf("%hx", &ivect);
   p = modbase + ivect/2;
   writereg(p, vec);
   printf("Enter interrupt reset register offset in hex:  ");
   scanf("%hx", &iresetreg);
   printf("Enter interrupt reset bits in hex:  ");
   scanf("%hx", &irbits);
   printf("Enter interrupt reset register and mask in hex:  ");
   scanf("%hx", &ipatmask);
   getchar();
   ttycharmode();
   return(0);
}

static int seta32intvector()
{
   int vec;
   unsigned long *p;
   unsigned long addr;

   ttylinemode();
   printf("Enter interrupt number in hex:  ");
   scanf("%x", &vec);
   if ( vec < 0 || vec > 255) {
      printf("interrupt number must be between 0 and 255 \n");
      return(-1);
   }
   if ( intConnect( INUM_TO_IVEC(vec), a32inthandler, 0) != OK ) {
                printf("setintvector: intConnect failed for inum %d\n", vec);
                return(-1);
        }
   printf("Enter module a32 base address in hex:  ");
   scanf("%lx", &addr);
   if (sysBusToLocalAdrs(VME_AM_EXT_USR_DATA, (char *)addr,
       (char **)(&a32ibase)) == ERROR) {
      printf("setbase: Can't find a32 address space\n");
   }
   printf("Base address set to %p\n", a32ibase);
   printf("Enter interrupt vector register offset in hex:  ");
   scanf("%lx", &a32ivect);
   p = a32ibase + a32ivect/4;
   writea32reg(p, vec);
   printf("Enter interrupt reset register offset in hex:  ");
   scanf("%lx", &a32iresetreg);
   printf("Enter interrupt reset bits in hex:  ");
   scanf("%lx", &a32irbits);
   printf("Enter interrupt reset register and mask in hex:  ");
   scanf("%lx", &a32ipatmask);
   getchar();
   ttycharmode();
   return(0);
}

static int twopatternwrite()
{
   unsigned short pat1;
   unsigned short pat2;
   unsigned short *p;
   int   status;
   int   nttychars;

   ttylinemode();
   printf("Enter pattern 1 in hex:  ");
   scanf("%hx", &pat1);
   printf("Enter pattern 2 in hex:  ");
   scanf("%hx", &pat2);
   getchar();
   ttycharmode();
   p = modbase + reg/2;
   printf("Looping on 2 pattern write of Register %p \n", p);
   nttychars=0;
   while (1) {
      writereg(p, pat1);
      writereg(p, pat2);
      status = ioctl (0, FIONREAD, (int)&nttychars);
      if ( nttychars != 0 )
         break;
   }
   getchar();
   return(0);
}

static int bitpatmenu()
{
   int   choice;
   int   selected;

   rsize = 4096;
   rwsize = 1;
   bpnum = 0;
   bpbank = 0;
   selected = 0;
   while (selected == 0) {
      ttycharmode();
      ttyinflush();
      selected =
      (*(bpm.pmt[choice=fmenu(&bpm)].proc))();
   }
   return(0);
}

static int bpramsel()
{
   printf("Select Ram to test  (0-8):  ");
   scanf("%hd", &bpnum);
   if ( bpnum > 8) {
      printf("Oops, %d is ilegal, 0 thru 8 are legal \n", bpnum);
      bpnum = 0;
   }
   printf("Ram %d selected\n", bpnum);
   getchar();
   return(0);
}

static int bpbanksel()
{
   printf("Select bank to test  (0 or 1):  ");
   scanf("%hd", &bpbank);
   if (  bpbank > 1) {
      printf("Oops, %d is ilegal, only bank 0 and 1 are available\n",
          bpbank);
      bpbank = 0;
   }
   printf("Bank %d selected\n", bpbank);
   getchar();
   return(0);
}

static int listbpsettings()
{
   printf("Base address set to %p\n", modbase);
   printf("Bit pattern number = %x\n", bpnum);
   printf("Bank Select set to %x\n", bpbank);
   printf("Ram size set to %d words\n", rsize);
   printf("Ram word size set to %d bytes\n", rwsize);
   printf("Ram default fill  value set to %x\n", writeval);
   printf("Number of words to dump set to %x\n", rdwords);
   return(0);
}

static int bpinit(num, bank)
unsigned short num;
unsigned short bank;
{
#define  BPSELECT 0x20
#define   BPBANK     0x2a

   unsigned short *p;
   unsigned short v;

   p = modbase + BPBANK/2;
   v = readreg(p);
   if(bank)
      v |= 1<<num;
   else
      v &= ~(1<<num);
   if (writereg(p, v) != OK )
      return(ERROR);
   p = modbase + BPSELECT/2;
   if(writereg(p, num) != OK)
      return(ERROR);
   if(writereg(p, 0x80 | num) != OK)
      return(ERROR);
   return(OK);
}

#define BPDATA 0x22

static int bpfillram()
{
   int i;
   unsigned short *pd;

   pd = setadd( modbase, BPDATA);

   printf("Enter ram fill value in hex:  ");
   scanf("%hx", &writeval);
   getchar();

   if ( bpinit(bpnum, bpbank) != OK )
      return(0);
   for ( i=0; i < rsize; i++ ) {
      if ( writereg(pd, writeval) != OK )
         break;
   }

   return(0);
}


static int bpdumpram()
{
   int i,j,k;
   unsigned short *pd;
   unsigned short val;
   int line;
   char  loop;

   loop = 'y';
   line=8;
   pd = setadd( modbase, BPDATA);

   if ( bpinit(bpnum, bpbank) != OK )
      return(0);
   k=0;
   while ( loop  == 'y' && k < (rsize - rdadd) ) {
      i=0;
      while( i < rdwords) {
      printf("\n%x  ", rdadd + k);
      for ( j=0; j < line && i < rdwords; j++, i++, k++) {
         val = readreg(pd);
         if ( rwsize == 1)
            val &= 0xff;
         printf("  %x", val);
      }
      }
      printf("\n\nContinue? (y|n): ");
      loop = (char) getchar();
      printf("\n");
   }
   return(0);
}


static int bpwraddtest()
{
   int   status;
   int   nttychars;
   unsigned short val;
   unsigned short chk;
   int i;
   unsigned short *pd;
   int   iter;

   printf("Looping on write-read address test\n");
   nttychars=0;
   iter=0;
   pd = setadd( modbase, BPDATA);
   while (1) {

      if ( bpinit(bpnum, bpbank) != OK )
         return(0);
      for ( i=0; i < rsize; i++ ) {
         if ( writereg(pd, i) != OK )
            break;
      }

      if ( bpinit(bpnum, bpbank) != OK )
         return(0);

      for ( i=0; i < rsize; i++ ) {
         val = readreg(pd);
         chk = i;
         if ( rwsize == 1) {
            val &= 0xff;
            chk &= 0xff;
         }
         if( val != chk) {
            printf("Error at %x -- wrote %x  read %x\n",
               i, chk, val);
         }

      }
      iter++;
      status = ioctl (0, FIONREAD, (int)&nttychars);
      if ( nttychars != 0 )
         break;
   }
   getchar();
   printf("\n%d iterations completed\n", iter);
   return(0);
}


static int bpwrrottest()
{
   int   status;
   int   nttychars;
   unsigned short val,val1;
   unsigned short chk;
   unsigned short rotval;
   int i;
   unsigned short *pd;
   int   iter;

   printf("Enter value to rotate left in hex:  ");
   scanf("%hx", &rotval);
   getchar();

   printf("Looping on rotate pattern left test\n");
   nttychars=0;
   iter=0;
   pd = setadd( modbase, BPDATA);
   while (1) {

      if ( bpinit(bpnum, bpbank) != OK )
         return(0);
      val = rotval;
      if ( rwsize == 1)
         val &= 0xff;
      for ( i=0; i < rsize; i++ ) {
         if ( writereg(pd, val) != OK )
            break;
         val1 = val << 1;
         if ( rwsize == 1) {
            if ( val & 0x80 )
               val1 |= 1;
            val1 &= 0xff;
         } else {
            if ( val & 0x8000 )
               val1 |= 1;
         }
         val = val1;
      }

      if ( bpinit(bpnum, bpbank) != OK )
         return(0);

      chk = rotval;
      if ( rwsize == 1)
         chk &= 0xff;

      for ( i=0; i < rsize; i++ ) {
         val = readreg(pd);
         if ( rwsize == 1) {
            val &= 0xff;
         }
         if( val != chk) {
            printf("Error at %x -- wrote %x  read %x\n",
               i, chk, val);
         }
         val1 = chk << 1;
         if ( rwsize == 1) {
            if ( chk & 0x80 )
               val1 |= 1;
            val1 &= 0xff;
         } else {
            if ( chk & 0x8000 )
               val1 |= 1;
         }
         chk = val1;

      }
      iter++;
      status = ioctl (0, FIONREAD, (int)&nttychars);
      if ( nttychars != 0 )
         break;
   }
   getchar();
   printf("\n%d iterations completed\n", iter);
   return(0);
}


static int bpwronestest()
{
   int   status;
   int   nttychars;
   unsigned short val;
   unsigned short chk;
   int i;
   unsigned short  *pd;
   int   iter;
   unsigned short pat;

   printf("Looping on shifting ones test\n");
   nttychars=0;
   iter=0;
   pat = 1;
   pd = setadd( modbase, BPDATA);
   while (1) {

      if ( bpinit(bpnum, bpbank) != OK )
         return(0);
      val = pat;
      if ( rwsize == 1)
         val &= 0xff;
      for ( i=0; i < rsize; i++ ) {
         if ( writereg(pd, val) != OK )
            break;
         if ( rwsize == 1) {
            if ( val &  0x80 )
               val <<= 1;
            else {
               val <<=1;
               val |= 1;
            }
            val &= 0xff;
         } else {
            if ( val & 0x8000 )
               val <<=1;
            else {
               val <<=1;
               val |= 1;
            }
         }
      }

      if ( bpinit(bpnum, bpbank) != OK )
         return(0);

      chk = pat;
      if ( rwsize == 1)
         chk &= 0xff;

      for ( i=0; i < rsize; i++ ) {
         val = readreg(pd);
         if ( rwsize == 1) {
            val &= 0xff;
         }
         if( val != chk) {
            printf("Error at %x -- wrote %x  read %x\n",
               i, chk, val);
         }
         if ( rwsize == 1) {
            if ( chk &  0x80 )
               chk <<= 1;
            else {
               chk <<=1;
               chk |= 1;
            }
            chk &= 0xff;
         } else {
            if ( chk & 0x8000 )
               chk <<=1;
            else {
               chk <<=1;
               chk |= 1;
            }
         }

      }
      iter++;
      status = ioctl (0, FIONREAD, (int)&nttychars);
      if ( nttychars != 0 )
         break;
   }
   getchar();
   printf("\n%d iterations completed\n", iter);
   return(0);
}


static int bprandchk()
{
   int   status;
   int   nttychars;
   unsigned short val;
   unsigned short chk;
   int i;
   unsigned short *pd;
   int   iter;

   printf("Enter value to compare to in hex:  ");
   scanf("%hx", &chk);
   getchar();

   printf("Looping on read and check test\n");
   nttychars=0;
   iter=0;
   pd = setadd( modbase, BPDATA);
   while (1) {

      if ( bpinit(bpnum, bpbank) != OK )
         return(0);

      if ( rwsize == 1)
         chk &= 0xff;

      for ( i=0; i < rsize; i++ ) {
         val = readreg(pd);
         if ( rwsize == 1) {
            val &= 0xff;
         }
         if( val != chk) {
            printf("Error at %x -- wrote %x  read %x\n",
               i, chk, val);
         }

      }
      iter++;
      status = ioctl (0, FIONREAD, (int)&nttychars);
      if ( nttychars != 0 )
         break;
   }
   getchar();
   printf("\n%d iterations completed\n", iter);
   return(0);
}



static int wrrandtest()
{
   int   status;
   int   nttychars;
   unsigned short val;
   unsigned short chk;
   int i;
   unsigned short *pa, *pd;
   int   iter;


   printf("Looping on random pattern test\n");
   nttychars=0;
   iter=0;
   pa = setadd( modbase, radd);
   pd = setadd( modbase, rdreg);
   seed = 1;
   while (1) {

      if ( writereg(pa, 0) != OK )
         return(0);
      srandom(seed);

      for ( i=0; i < rsize; i++ ) {
         val = (unsigned short)random();
         if ( rwsize == 1)
            val &= 0xff;
         if ( writereg(pd, val) != OK )
            break;
      }

      if ( writereg(pa, 0) != OK )
         return(0);

      srandom(seed);

      for ( i=0; i < rsize; i++ ) {
         chk = (unsigned short)random();
         if ( rwsize == 1)
            chk &= 0xff;
         val = readreg(pd);
         if ( rwsize == 1) {
            val &= 0xff;
         }
         if( val != chk) {
            printf("Error at %x -- wrote %x  read %x\n",
               i, chk, val);
         }

      }
      iter++;
      seed++;
      status = ioctl (0, FIONREAD, (int)&nttychars);
      if ( nttychars != 0 )
         break;
   }
   seed--;     /* back off seed so read only test will work */
   getchar();
   printf("\n%d iterations completed\n", iter);
   return(0);
}

static int rrandtest()
{
   int   status;
   int   nttychars;
   unsigned short val;
   unsigned short chk;
   int i;
   unsigned short *pa, *pd;
   int   iter;


   printf("Looping on read random pattern test\n");
   nttychars=0;
   iter=0;
   pa = setadd( modbase, radd);
   pd = setadd( modbase, rdreg);
   while (1) {


      if ( writereg(pa, 0) != OK )
         return(0);

      srandom(seed);

      for ( i=0; i < rsize; i++ ) {
         chk = (unsigned short)random();
         if ( rwsize == 1)
            chk &= 0xff;
         val = readreg(pd);
         if ( rwsize == 1) {
            val &= 0xff;
         }
         if( val != chk) {
            printf("Error at %x -- wrote %x  read %x\n",
               i, chk, val);
         }

      }
      iter++;
      status = ioctl (0, FIONREAD, (int)&nttychars);
      if ( nttychars != 0 )
         break;
   }
   getchar();
   printf("\n%d iterations completed\n", iter);
   return(0);
}


/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * This product includes software developed by the University of
 * California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include <stdio.h>
#include <stdlib.h>

/*
 * random.c:
 *
 * An improved random number generation package.  In addition to the standard
 * rand()/srand() like interface, this package also has a special state info
 * interface.  The initstate() routine is called with a seed, an array of
 * bytes, and a count of how many bytes are being passed in; this array is
 * then initialized to contain information for random number generation with
 * that much state information.  Good sizes for the amount of state
 * information are 32, 64, 128, and 256 bytes.  The state can be switched by
 * calling the setstate() routine with the same array as was initiallized
 * with initstate().  By default, the package runs with 128 bytes of state
 * information and generates far better random numbers than a linear
 * congruential generator.  If the amount of state information is less than
 * 32 bytes, a simple linear congruential R.N.G. is used.
 *
 * Internally, the state information is treated as an array of longs; the
 * zeroeth element of the array is the type of R.N.G. being used (small
 * integer); the remainder of the array is the state information for the
 * R.N.G.  Thus, 32 bytes of state information will give 7 longs worth of
 * state information, which will allow a degree seven polynomial.  (Note:
 * the zeroeth word of state information also has some other information
 * stored in it -- see setstate() for details).
 *
 * The random number generation technique is a linear feedback shift register
 * approach, employing trinomials (since there are fewer terms to sum up that
 * way).  In this approach, the least significant bit of all the numbers in
 * the state table will act as a linear feedback shift register, and will
 * have period 2^deg - 1 (where deg is the degree of the polynomial being
 * used, assuming that the polynomial is irreducible and primitive).  The
 * higher order bits will have longer periods, since their values are also
 * influenced by pseudo-random carries out of the lower bits.  The total
 * period of the generator is approximately deg*(2**deg - 1); thus doubling
 * the amount of state information has a vast influence on the period of the
 * generator.  Note: the deg*(2**deg - 1) is an approximation only good for
 * large deg, when the period of the shift register is the dominant factor.
 * With deg equal to seven, the period is actually much longer than the
 * 7*(2**7 - 1) predicted by this formula.
 */

/*
 * For each of the currently supported random number generators, we have a
 * break value on the amount of state information (you need at least this
 * many bytes of state info to support this random number generator), a degree
 * for the polynomial (actually a trinomial) that the R.N.G. is based on, and
 * the separation between the two lower order coefficients of the trinomial.
 */
#define  TYPE_0      0     /* linear congruential */
#define  BREAK_0     8
#define  DEG_0    0
#define  SEP_0    0

#define  TYPE_1      1     /* x**7 + x**3 + 1 */
#define  BREAK_1     32
#define  DEG_1    7
#define  SEP_1    3

#define  TYPE_2      2     /* x**15 + x + 1 */
#define  BREAK_2     64
#define  DEG_2    15
#define  SEP_2    1

#define  TYPE_3      3     /* x**31 + x**3 + 1 */
#define  BREAK_3     128
#define  DEG_3    31
#define  SEP_3    3

#define  TYPE_4      4     /* x**63 + x + 1 */
#define  BREAK_4     256
#define  DEG_4    63
#define  SEP_4    1

/*
 * Array versions of the above information to make code run faster --
 * relies on fact that TYPE_i == i.
 */
#define  MAX_TYPES   5     /* max number of types above */

static int degrees[MAX_TYPES] =  { DEG_0, DEG_1, DEG_2, DEG_3, DEG_4 };
static int seps [MAX_TYPES] = { SEP_0, SEP_1, SEP_2, SEP_3, SEP_4 };

/*
 * Initially, everything is set up as if from:
 *
 * initstate(1, &randtbl, 128);
 *
 * Note that this initialization takes advantage of the fact that srandom()
 * advances the front and rear pointers 10*rand_deg times, and hence the
 * rear pointer which starts at 0 will also end up at zero; thus the zeroeth
 * element of the state information, which contains info about the current
 * position of the rear pointer is just
 *
 * MAX_TYPES * (rptr - state) + TYPE_3 == TYPE_3.
 */

static long randtbl[DEG_3 + 1] = {
   TYPE_3,
   0x9a319039, 0x32d9c024, 0x9b663182, 0x5da1f342, 0xde3b81e0, 0xdf0a6fb5,
   0xf103bc02, 0x48f340fb, 0x7449e56b, 0xbeb1dbb0, 0xab5c5918, 0x946554fd,
   0x8c2e680f, 0xeb3d799f, 0xb11ee0b7, 0x2d436b86, 0xda672e2a, 0x1588ca88,
   0xe369735d, 0x904f35f7, 0xd7158fd6, 0x6fa6f051, 0x616e6b96, 0xac94efdc,
   0x36413f93, 0xc622c298, 0xf5a42ab8, 0x8a88d77b, 0xf5ad9d0e, 0x8999220b,
   0x27fb47b9,
};

/*
 * fptr and rptr are two pointers into the state info, a front and a rear
 * pointer.  These two pointers are always rand_sep places aparts, as they
 * cycle cyclically through the state information.  (Yes, this does mean we
 * could get away with just one pointer, but the code for random() is more
 * efficient this way).  The pointers are left positioned as they would be
 * from the call
 *
 * initstate(1, randtbl, 128);
 *
 * (The position of the rear pointer, rptr, is really 0 (as explained above
 * in the initialization of randtbl) because the state table pointer is set
 * to point to randtbl[1] (as explained below).
 */
static long *fptr = &randtbl[SEP_3 + 1];
static long *rptr = &randtbl[1];

/*
 * The following things are the pointer to the state information table, the
 * type of the current generator, the degree of the current polynomial being
 * used, and the separation between the two pointers.  Note that for efficiency
 * of random(), we remember the first location of the state information, not
 * the zeroeth.  Hence it is valid to access state[-1], which is used to
 * store the type of the R.N.G.  Also, we remember the last location, since
 * this is more efficient than indexing every time to find the address of
 * the last element to see if the front and rear pointers have wrapped.
 */
static long *state = &randtbl[1];
static int rand_type = TYPE_3;
static int rand_deg = DEG_3;
static int rand_sep = SEP_3;
static long *end_ptr = &randtbl[DEG_3 + 1];

/*
 * srandom:
 *
 * Initialize the random number generator based on the given seed.  If the
 * type is the trivial no-state-information type, just remember the seed.
 * Otherwise, initializes state[] based on the given "seed" via a linear
 * congruential generator.  Then, the pointers are set to known locations
 * that are exactly rand_sep places apart.  Lastly, it cycles the state
 * information a given number of times to get rid of any initial dependencies
 * introduced by the L.C.R.N.G.  Note that the initialization of randtbl[]
 * for default usage relies on values produced by this routine.
 */
static
void
srandom(x)
   u_int x;
{
   register int i, j;

   if (rand_type == TYPE_0)
      state[0] = x;
   else {
      j = 1;
      state[0] = x;
      for (i = 1; i < rand_deg; i++)
         state[i] = 1103515245 * state[i - 1] + 12345;
      fptr = &state[rand_sep];
      rptr = &state[0];
      for (i = 0; i < 10 * rand_deg; i++)
         (void)random();
   }
}

/*
 * initstate:
 *
 * Initialize the state information in the given array of n bytes for future
 * random number generation.  Based on the number of bytes we are given, and
 * the break values for the different R.N.G.'s, we choose the best (largest)
 * one we can and set things up for it.  srandom() is then called to
 * initialize the state information.
 *
 * Note that on return from srandom(), we set state[-1] to be the type
 * multiplexed with the current value of the rear pointer; this is so
 * successive calls to initstate() won't lose this information and will be
 * able to restart with setstate().
 *
 * Note: the first thing we do is save the current state, if any, just like
 * setstate() so that it doesn't matter when initstate is called.
 *
 * Returns a pointer to the old state.
 */
static
char *
initstate(seed, arg_state, n)
   u_int seed;       /* seed for R.N.G. */
   char *arg_state;     /* pointer to state array */
   int n;            /* # bytes of state info */
{
   register char *ostate = (char *)(&state[-1]);

   if (rand_type == TYPE_0)
      state[-1] = rand_type;
   else
      state[-1] = MAX_TYPES * (rptr - state) + rand_type;
   if (n < BREAK_0) {
      (void)fprintf(stderr,
          "random: not enough state (%d bytes); ignored.\n", n);
      return(0);
   }
   if (n < BREAK_1) {
      rand_type = TYPE_0;
      rand_deg = DEG_0;
      rand_sep = SEP_0;
   } else if (n < BREAK_2) {
      rand_type = TYPE_1;
      rand_deg = DEG_1;
      rand_sep = SEP_1;
   } else if (n < BREAK_3) {
      rand_type = TYPE_2;
      rand_deg = DEG_2;
      rand_sep = SEP_2;
   } else if (n < BREAK_4) {
      rand_type = TYPE_3;
      rand_deg = DEG_3;
      rand_sep = SEP_3;
   } else {
      rand_type = TYPE_4;
      rand_deg = DEG_4;
      rand_sep = SEP_4;
   }
   state = &(((long *)arg_state)[1]);  /* first location */
   end_ptr = &state[rand_deg];   /* must set end_ptr before srandom */
   srandom(seed);
   if (rand_type == TYPE_0)
      state[-1] = rand_type;
   else
      state[-1] = MAX_TYPES*(rptr - state) + rand_type;
   return(ostate);
}

/*
 * setstate:
 *
 * Restore the state from the given state array.
 *
 * Note: it is important that we also remember the locations of the pointers
 * in the current state information, and restore the locations of the pointers
 * from the old state information.  This is done by multiplexing the pointer
 * location into the zeroeth word of the state information.
 *
 * Note that due to the order in which things are done, it is OK to call
 * setstate() with the same state as the current state.
 *
 * Returns a pointer to the old state information.
 */
static
char *
setstate(arg_state)
   char *arg_state;
{
   register long *new_state = (long *)arg_state;
   register int type = new_state[0] % MAX_TYPES;
   register int rear = new_state[0] / MAX_TYPES;
   char *ostate = (char *)(&state[-1]);

   if (rand_type == TYPE_0)
      state[-1] = rand_type;
   else
      state[-1] = MAX_TYPES * (rptr - state) + rand_type;
   switch(type) {
   case TYPE_0:
   case TYPE_1:
   case TYPE_2:
   case TYPE_3:
   case TYPE_4:
      rand_type = type;
      rand_deg = degrees[type];
      rand_sep = seps[type];
      break;
   default:
      (void)fprintf(stderr,
          "random: state info corrupted; not changed.\n");
   }
   state = &new_state[1];
   if (rand_type != TYPE_0) {
      rptr = &state[rear];
      fptr = &state[(rear + rand_sep) % rand_deg];
   }
   end_ptr = &state[rand_deg];      /* set end_ptr too */
   return(ostate);
}

/*
 * random:
 *
 * If we are using the trivial TYPE_0 R.N.G., just do the old linear
 * congruential bit.  Otherwise, we do our fancy trinomial stuff, which is
 * the same in all the other cases due to all the global variables that have
 * been set up.  The basic operation is to add the number at the rear pointer
 * into the one at the front pointer.  Then both pointers are advanced to
 * the next location cyclically in the table.  The value returned is the sum
 * generated, reduced to 31 bits by throwing away the "least random" low bit.
 *
 * Note: the code takes advantage of the fact that both the front and
 * rear pointers can't wrap on the same call by not testing the rear
 * pointer if the front one has wrapped.
 *
 * Returns a 31-bit random number.
 */
static
long
random()
{
   long i;

   if (rand_type == TYPE_0)
      i = state[0] = (state[0] * 1103515245 + 12345) & 0x7fffffff;
   else {
      *fptr += *rptr;
      i = (*fptr >> 1) & 0x7fffffff;   /* chucking least random bit */
      if (++fptr >= end_ptr) {
         fptr = state;
         ++rptr;
      } else if (++rptr >= end_ptr)
         rptr = state;
   }
   return(i);
}

static int seta32base()
{
   unsigned long  addr;

   ttylinemode();
   printf("Enter module a32 base address in hex:  ");
   scanf("%lx", &addr);
   if (sysBusToLocalAdrs(VME_AM_EXT_USR_DATA, (char *)addr,
       (char **)(&a32base)) == ERROR) {
      printf("setbase: Can't find a32 address space\n");
   }
   printf("Base address set to %p\n", a32base);
   getchar();
   ttycharmode();
   return(0);
}

static int seta32offset()
{
   ttylinemode();
   printf("Enter a32 register offset in hex:  ");
   scanf("%lx", &a32reg);
   printf("Register offset set to %x\n", (unsigned int)a32reg);
   getchar();
   ttycharmode();
   return(0);
}

static int lista32settings()
{
   printf("Base address set to %p\n", (unsigned int*)a32base);
   printf("Register offset set to %x\n", (unsigned int)a32reg);
   printf("Default write value set to %x\n", (unsigned int)a32writeval);
   return(0);
}

static int freada32reg()
{
   unsigned long val;
   unsigned long *p;

   p = a32base + a32reg/4;
   val = reada32reg(p);
   printf("Register %p = 0x%x\n", p, (unsigned int)val);
   return(0);
}

static int fwritea32reg()
{
   unsigned long *p;

   p = a32base + a32reg/4;
   printf("Enter value to write hex:  ");
   scanf("%lx", &a32writeval);
   getchar();
   writea32reg(p, a32writeval);
   return(0);
}
static int fwra32reg()
{
   unsigned long *p;

   p = a32base + a32reg/4;
   printf("Enter value to write hex:  ");
   scanf("%lx", &a32writeval);
   getchar();
   if (writea32reg(p, a32writeval) == OK)
      freada32reg();
   return(0);
}

static int loopa32read()
{
   int   status;
   int   nttychars;
   unsigned long val;
   unsigned long *p;

   p = a32base + a32reg/4;
   printf("Looping on read of Register %p \n", p);
   nttychars=0;
   while (1) {
      val = reada32reg(p);
      status = ioctl (0, FIONREAD, (int)&nttychars);
      if ( nttychars != 0 )
         break;
   }
   getchar();
   return(0);
}

static int loopa32write()
{
   int   status;
   int   nttychars;
   unsigned long *p;

   p = a32base + a32reg/4;
   printf("Looping on write of Register %p \n", p);
   nttychars=0;
   while (1) {
      writea32reg(p, a32writeval);
      status = ioctl (0, FIONREAD, (int)&nttychars);
      if ( nttychars != 0 )
         break;
   }
   getchar();
   return(0);
}

static int loopa32rw()
{
   int   status;
   int   nttychars;
   unsigned long val;
   unsigned long *p;

   p = a32base + a32reg/4;
   printf("Looping on write-read of Register %p \n", p);
   nttychars=0;
   while (1) {
      writea32reg(p, a32writeval);
      val = reada32reg(p);
      status = ioctl (0, FIONREAD, (int)&nttychars);
      if ( nttychars != 0 )
         break;
   }
   getchar();
   return(0);
}

static int seta32ramaoff()
{
   printf("Enter offset of start of ram in hex (# of bytes above base:  ");
   scanf("%lx", &a32radd);
   printf("Offset of start of ram set to %x\n", (unsigned int)a32radd);
   getchar();
   return(0);
}

static int seta32ramsz()
{
   printf("Enter ram size in words (dec):  ");
   scanf("%ld", &a32rsize);
   printf("Ram size set to %d words\n", (unsigned int)a32rsize);
   getchar();
   return(0);
}

static int seta32ramdadd()
{
   printf("Enter ram dump address in hex:  ");
   scanf("%lx", &a32rdadd);
   printf("Ram dump address set to %x\n", (unsigned int)a32rdadd);
   getchar();
   return(0);
}
static int seta32ramdwords()
{
   printf("Enter enter number of words to dump in hex:  ");
   scanf("%lx", &a32rdwords);
   printf("Number of words to dump set to %x\n", (unsigned int)a32rdwords);
   getchar();
   return(0);
}

static int seta32fast()
{
   printf("Enter 1 to enable turbo read/write mode, 0 to disable:  ");
   scanf("%lx", &a32fast);
   if (a32fast)
      printf("Turbo mode enabled\n");
   else
      printf("Turbo mode disabled\n");
   getchar();
   return(0);
}

static int lista32rsettings()
{
   printf("A32 Base address set to %p\n", (unsigned int*)a32base);
   printf("Offset of start of ram set to %x\n", (unsigned int)a32radd);
   printf("Ram size set to %d words\n", (unsigned int)a32rsize);
   printf("Ram default fill  value set to %x\n", (unsigned int)a32writeval);
   printf("Ram dump address set to %x\n", (unsigned int)a32rdadd);
   printf("Number of words to dump set to %x\n", (unsigned int)a32rdwords);
   if (a32fast)
      printf("Turbo mode enabled\n");
   else
      printf("Turbo mode disabled\n");
   return(0);
}

static int dumpa32ram()
{
   int i,j,k;
   unsigned long *pa;
   unsigned long val;
   int line;
   char  loop;

   loop = 'y';
   line=8;
   pa = seta32add( a32base, a32radd);
   pa += a32rdadd;
   printf("starting at address %p\n\n", pa);
   k=0;
   while ( loop  == 'y' && k < (a32rsize) ) {
      i=0;
      while( i < a32rdwords) {
      printf("\n%x  ", (unsigned int)(a32rdadd + k) );
      for ( j=0; j < line && i < a32rdwords; j++, i++, k++, pa++) {
         val = reada32reg(pa);
         printf("  %x", (unsigned int)val);
      }
      }
      printf("\n\nContinue? (y|n): ");
      loop = (char) getchar();
      printf("\n");
   }
   return(0);
}

static int filla32ram()
{
   int i;
   unsigned long *pa;

   pa = seta32add( a32base, a32radd);

   printf("Enter ram fill value in hex:  ");
   scanf("%lx", &a32writeval);
   getchar();

   for ( i=0; i < a32rsize; i++, pa++ ) {
      if ( writea32reg(pa, a32writeval) != OK )
         break;
   }

   return(0);
}

static int a32wraddtest()
{
   int   status;
   int   nttychars;
   unsigned long val;
   unsigned long chk;
   int i;
   unsigned long *pa;
   int   iter;

   printf("Looping on a32 write-read address test\n");
   nttychars=0;
   iter=0;
   while (1) {
      pa = seta32add( a32base, a32radd);
      for ( i=0; i < a32rsize; i++, pa++ ) {
         if ( writea32reg(pa, i) != OK )
            break;
      }

      pa = seta32add( a32base, a32radd);
      for ( i=0; i < a32rsize; i++, pa++ ) {
         val = reada32reg(pa);
         chk = i;
         if( val != chk) {
            printf("Error at %x -- wrote %x  read %x\n",
               (unsigned int)i, (unsigned int)chk, (unsigned int)val);
         }

      }
      iter++;
      status = ioctl (0, FIONREAD, (int)&nttychars);
      if ( nttychars != 0 )
         break;
   }
   getchar();
   printf("\n%d iterations completed\n", iter);
   return(0);
}

static int a32raddtest()
{
   int   status;
   int   nttychars;
   unsigned long val;
   unsigned long chk;
   int i;
   unsigned long *pa;
   int   iter;

   printf("Looping on a32 read address test\n");
   nttychars=0;
   iter=0;
   while (1) {
      pa = seta32add( a32base, a32radd);

      for ( i=0; i < a32rsize; i++, pa++ ) {
         val = reada32reg(pa);
         chk = i;
         if( val != chk) {
            printf("Error at %x -- wrote %x  read %x\n",
               (unsigned int)i, (unsigned int)chk, (unsigned int)val);
         }

      }
      iter++;
      status = ioctl (0, FIONREAD, (int)&nttychars);
      if ( nttychars != 0 )
         break;
   }
   getchar();
   printf("\n%d iterations completed\n", iter);
   return(0);
}

static int a32wrrottest()
{
   int   status;
   int   nttychars;
   unsigned long val,val1;
   unsigned long chk;
   unsigned long rotval;
   int i;
   unsigned long *pa;
   int   iter;

   printf("Enter value to rotate left in hex:  ");
   scanf("%lx", &rotval);
   getchar();

   printf("Looping on a32 rotate pattern left test\n");
   nttychars=0;
   iter=0;
   while (1) {

      pa = seta32add( a32base, a32radd);
      val = rotval;
      for ( i=0; i < a32rsize; i++, pa++ ) {
         if ( writea32reg(pa, val) != OK )
            break;
         val1 = val << 1;
         if ( val & 0x80000000 )
            val1 |= 1;
         val = val1;
      }

      pa = seta32add( a32base, a32radd);
      chk = rotval;
      for ( i=0; i < a32rsize; i++, pa++ ) {
         val = reada32reg(pa);
         if( val != chk) {
            printf("Error at %x -- wrote %x  read %x\n",
               (unsigned int)i, (unsigned int)chk, (unsigned int)val);
         }
         val1 = chk << 1;
         if ( chk & 0x80000000 )
            val1 |= 1;
         chk = val1;

      }
      iter++;
      status = ioctl (0, FIONREAD, (int)&nttychars);
      if ( nttychars != 0 )
         break;
   }
   getchar();
   printf("\n%d iterations completed\n", iter);
   return(0);
}

static int a32wronestest()
{
   int   status;
   int   nttychars;
   unsigned long val;
   unsigned long chk;
   int i;
   unsigned long *pa;
   int   iter;
   unsigned long pat;

   printf("Looping on shifting ones test\n");
   nttychars=0;
   iter=0;
   pat = 1;
   while (1) {
      pa = seta32add( a32base, a32radd);
      val = pat;
      for ( i=0; i < a32rsize; i++, pa++ ) {
         if ( writea32reg(pa, val) != OK )
            break;
         if ( val & 0x80000000 )
            val <<=1;
         else {
            val <<=1;
            val |= 1;
         }
      }
      pa = seta32add( a32base, a32radd);

      chk = pat;
      for ( i=0; i < a32rsize; i++, pa++ ) {
         val = reada32reg(pa);
         if( val != chk) {
            printf("Error at %x -- wrote %x  read %x\n",
               (unsigned int)i, (unsigned int)chk, (unsigned int)val);
         }
         if ( chk & 0x80000000 )
            chk <<=1;
         else {
            chk <<=1;
            chk |= 1;
         }
      }
      iter++;
      status = ioctl (0, FIONREAD, (int)&nttychars);
      if ( nttychars != 0 )
         break;
   }
   getchar();
   printf("\n%d iterations completed\n", iter);
   return(0);
}

static int a32randchk()
{
   int   status;
   int   nttychars;
   unsigned long val;
   unsigned long chk;
   int i;
   unsigned long *pa;
   int   iter;

   printf("Enter value to compare to in hex:  ");
   scanf("%lx", &chk);
   getchar();

   printf("Looping on read and check test\n");
   nttychars=0;
   iter=0;
   while (1) {
      pa = seta32add( a32base, a32radd);

      for ( i=0; i < a32rsize; i++, pa++ ) {
         val = reada32reg(pa);
         if( val != chk) {
            printf("Error at %x -- wrote %x  read %x\n",
               (unsigned int)i, (unsigned int)chk, (unsigned int)val);
         }

      }
      iter++;
      status = ioctl (0, FIONREAD, (int)&nttychars);
      if ( nttychars != 0 )
         break;
   }
   getchar();
   printf("\n%d iterations completed\n", iter);
   return(0);
}

static int a32wrrandtest()
{
   int   status;
   int   nttychars;
   unsigned long val;
   unsigned long chk;
   int i;
   unsigned long *pa;
   int   iter;


   printf("Looping on random pattern test\n");
   nttychars=0;
   iter=0;
   a32seed = 1;
   while (1) {
      pa = seta32add( a32base, a32radd);
      srandom(a32seed);

      for ( i=0; i < a32rsize; i++, pa++ ) {
         val = (unsigned long)random();
         if ( writea32reg(pa, val) != OK )
            break;
      }

      pa = seta32add( a32base, a32radd);
      srandom(a32seed);

      for ( i=0; i < a32rsize; i++, pa++ ) {
         chk = (unsigned long)random();
         val = reada32reg(pa);
         if( val != chk) {
            printf("Error at %x -- wrote %x  read %x\n",
               (unsigned int)i, (unsigned int)chk, (unsigned int)val);
         }

      }
      iter++;
      a32seed++;
      status = ioctl (0, FIONREAD, (int)&nttychars);
      if ( nttychars != 0 )
         break;
   }
   a32seed--;     /* back off seed so read only test will work */
   getchar();
   printf("\n%d iterations completed\n", iter);
   return(0);
}

static int a32rrandtest()
{
   int   status;
   int   nttychars;
   unsigned long val;
   unsigned long chk;
   int i;
   unsigned long *pa;
   int   iter;


   printf("Looping on read random pattern test\n");
   nttychars=0;
   iter=0;
   while (1) {
      pa = seta32add( a32base, a32radd);
      srandom(a32seed);

      for ( i=0; i < a32rsize; i++, pa++ ) {
         chk = (unsigned long)random();
         val = reada32reg(pa);
         if( val != chk) {
            printf("Error at %x -- wrote %x  read %x\n",
               (unsigned int)i, (unsigned int)chk, (unsigned int)val);
         }

      }
      iter++;
      status = ioctl (0, FIONREAD, (int)&nttychars);
      if ( nttychars != 0 )
         break;
   }
   getchar();
   printf("\n%d iterations completed\n", iter);
   return(0);
}
