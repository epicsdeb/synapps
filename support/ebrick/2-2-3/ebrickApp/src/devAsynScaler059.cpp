/*

                          Argonne National Laboratory
                            APS Operations Division
                     Beamline Controls and Data Acquisition

                   EPICS Brick 059 Scaler Asyn Device Support



 -----------------------------------------------------------------------------
                                COPYRIGHT NOTICE
 -----------------------------------------------------------------------------
   Copyright (c) 2002 The University of Chicago, as Operator of Argonne
      National Laboratory.
   Copyright (c) 2002 The Regents of the University of California, as
      Operator of Los Alamos National Laboratory.
   Synapps Versions 4-5
   and higher are distributed subject to a Software License Agreement found
   in file LICENSE that is included with this distribution.
 -----------------------------------------------------------------------------

 Description
    This module provides device support support for the BC059 scaler record
    device support and works only with V2.0 of the FPGA design. To initialize
    device support, call the method below with the following calling sequence.

        devAsynScaler059Init(port,base,inst,rate,intr)

        Where:
            port - Scaler ASYN port name (i.e. "SCALER1")
            base - Base address of the BC059 module
            inst - Module instance
            rate - Poll frequency (i.e. 5 is 5Hz)
            intr - Enable interrupt processing


 Developer notes:
    1. In method scanScaler(), the line if( pport->psr->cnt ) scalerRead(pport)
       was added to eliminate a 'burp' that ocurred when the Scaler record was
       switching to Oneshot mode.
    2. This version of device support works *only* with V2.0 of the BC-059.
    3. Interrupts are only available when a OMM-MM-XT is employed with the
       OMM Linux device driver.
    4. Serial-Link Protocol formats:

       V1.4
       +-------------------------------------------------------------+
       |44               13|12         11|10          6|5           0|
       +-------------------------------------------------------------+
       | Preset (optional) |   Address   |   Function  |   Counter   |
       +-------------------------------------------------------------+

       V2.0
       +-------------------------------------------------------------+
       |43               12|11          9|8           5|4           0|
       +-------------------------------------------------------------+
       | Preset (optional) | Subfunction |   Function  |   Counter   |
       +-------------------------------------------------------------+

       V2.1 (current implementation)
       +-------------------------------------------------------------+
       |63               32|31    21|20     18|17  14|13    8|7     0|
       +-------------------------------------------------------------+
       | Preset (optional) |  User  | Subfnc |  Fnc  |  Ctr  |  Ver  |
       +-------------------------------------------------------------+


 Source control info:
    Modified by:    dkline
                    2008/02/18 19:01:21
                    1.10

 =============================================================================
 History:
 Author: David M. Kline
 -----------------------------------------------------------------------------
 2007-Oct-15  DMK  Derived from the BC-063 Scaler devices support.
 2007-Oct-16  DMK  Initial version complete.
 2007-Dec-18  DMK  Added support for interrupt processing.
 2008-Feb-18  DMK  Modifed to use serial-protocol V2.1 (see above).
 -----------------------------------------------------------------------------

*/


/* EPICS base version-specific definitions (must be performed first) */
#include <epicsVersion.h>
#define LT_EPICSBASE(v,r,l) (EPICS_VERSION<(v)||(EPICS_VERSION==(v)&&(EPICS_REVISION<(r)||(EPICS_REVISION==(r)&&EPICS_MODIFICATION<(l)))))


/* Evaluate EPICS base */
#if LT_EPICSBASE(3,14,6)
    #error "EPICS base must be 3.14.6 or greater"
#endif


/* System related include files */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/io.h>
#include <sys/ioctl.h>


/* EPICS system related include files */
#include <iocsh.h>
#include <epicsStdio.h>
#include <cantProceed.h>
#include <epicsString.h>
#include <epicsExport.h>
#include <epicsThread.h>

#include <cadef.h>
#include <recGbl.h>
#include <devSup.h>
#include <dbEvent.h>
#include <callback.h>


/* EPICS synApps/Asyn related include files */
#include <devScaler.h>
#include <scalerRecord.h>
#include <asynEpicsUtils.h>


/* Define support constants */
#define ID          (59)
#define RATE        (20)
#define SCALERS     (16)
#define INSTMAX     (4)


/* Define serial link symbolic constants */
#define MAJVER      (2)
#define MINVER      (1)
#define VERSION     ((MAJVER<<4)|MINVER)

#define ENA         (0x4)
#define CLK         (0x2)
#define DAT         (0x1)
#define DIS         (0x0)

#define VER_BIT     (8)
#define CTR_BIT     (6)
#define FNC_BIT     (4)
#define SFN_BIT     (3)
#define USR_BIT     (11)
#define DAT_BIT     (32)
#define DAT_SIZ     (DAT_BIT*2)
#define SLD_SIZ     ((VER_BIT+CTR_BIT+FNC_BIT+SFN_BIT+USR_BIT)*2)

#define DONE        (0x1)
#define ARMED       (0x2)
#define GROUP       (0x4)


/* Define IO port access macros */
#define get059(b,o) inb((b)+(o))
#define put059(b,o,v) outb((v),(b)+(o))


/* Forward references for device support struct */
static long scal059_report(int level);
static long scal059_hdw_init(int after);
static long scal059_rec_done(scalerRecord *psr);
static long scal059_rec_reset(scalerRecord *psr);
static long scal059_rec_arm(scalerRecord *psr,int val);
static long scal059_rec_read(scalerRecord *psr,long* val);
static long scal059_rec_init(scalerRecord* psr,CALLBACK *pcallback);
static long scal059_rec_preset(scalerRecord *psr,int signal,long val);

static SCALERDSET devAsynScaler059 =
{
    8,
    (long int (*)(void*))scal059_report,
    (long int (*)(void*))scal059_hdw_init,
    (long int (*)(void*))scal059_rec_init,
    NULL,
    (long int (*)(void*))scal059_rec_reset,
    (long int (*)(void*))scal059_rec_read,
    (long int (*)(void*))scal059_rec_preset,
    (long int (*)(void*))scal059_rec_arm,
    (long int (*)(void*))scal059_rec_done,
};
epicsExportAddress(dset,devAsynScaler059);


/* Forward struct declarations */
typedef struct Port Port;
typedef struct Scaler Scaler;

/* Declare Scaler structure */
struct Scaler
{
    epicsInt32  value;
    epicsInt32  preval;
    epicsInt32  preset;
};

/* Declare Port structure */
struct Port
{
    char*         name;
    int           fd;
    int           base;
    int           inst;
    int           rate;
    int           intr;
    double        freq;
    unsigned char modid;
    epicsMutexId  syncLock;

    Scaler*       scaler;
    scalerRecord* psr;

    int           poll;
    int           done;
    int           scancnt;
    CALLBACK*     pcallback;
};


/* Public interface forward references */
static int devAsynScaler059Init(const char* port,int base,int inst,int rate,int intr);


/* Private methods forward references */
static void scanScaler(Port* pport);
static void scalerRead(Port* pport);
static void serialLink(int base,int ver,int ctr,int fnc,int sfn,int usr,epicsInt32 preset);

/* Define macros */
#define init059(b)          (put059((b),3,0x92))
#define readDataLB(b)       (get059((b),0))
#define readDataHB(b)       (get059((b),1))
#define scalerNull(b)       {serialLink((b),VERSION,0,0,0,0,0);}
#define scalerArm(b)        {serialLink((b),VERSION,0,1,0,0,0);}
#define scalerDisarm(b)     {serialLink((b),VERSION,0,2,0,0,0);}
#define scalerPreset(b,c,p) {serialLink((b),VERSION,(c),3,0,0,(p));}
#define scalerReset(b)      {serialLink((b),VERSION,0,4,0,0,0);}
#define scalerGroup(b)      {serialLink((b),VERSION,0,5,0,0,0);}
#define scalerUngroup(b)    {serialLink((b),VERSION,0,6,0,0,0);}
#define scalerDataL(b,c)    {serialLink((b),VERSION,(c),7,2,0,0);}
#define scalerDataH(b,c)    {serialLink((b),VERSION,(c),7,4,0,0);}
#define scalerStatus(b)     {serialLink((b),VERSION,0,8,1,0,0);}


/* Define local variants */
static Port* pports[] = {NULL,NULL,NULL,NULL};


/****************************************************************************
 * Define public interface methods
 ****************************************************************************/
static int devAsynScaler059Init(const char* port,int base,int inst,int rate,int intr)
{
    uint prio;
    Port* pport;
    int len,fd=-1;

    if( inst >= INSTMAX )
    {
        printf("devAsynScaler059Init::init %s: invalid instance count %d\n",port,inst);
        return( -1 );
    }

    if( pports[inst] )
    {
        printf("devAsynScaler059Init::init %s: instance %d already defined\n",port,inst);
        return( -1 );
    }

    if( base )
    {
        init059(base);

        if( intr )
        {
            char device[16];

            rate = RATE;
            sprintf(device,"/dev/omm%1.1d",inst);
            fd = open(device,O_RDWR);
            if( fd < 0 )
            {
                printf("devAsynScaler059Init::init %s: failure to open device \"%s\"\n",port,device);
                return( -1 );
            }
        } else if( rate == 0 )
        {
            printf("devAsynScaler059Init::init %s: invalid rate value of %d\n",port,rate);
            return( -1 );
        }

        scalerReset(base);
    }
    else
    {
        printf("devAsynScaler059Init::init %s: base must be defined\n",port);
        return( -1 );
    }

    len = strlen(port)+1+sizeof(Port)+(sizeof(Scaler)*SCALERS);
    pport = (Port*)callocMustSucceed(len,sizeof(char),"devAsynScaler059Init");

    pport->scaler = (Scaler*)(pport+1);
    pport->name   = (char*)(pport->scaler+SCALERS);

    pport->fd     = fd;
    pport->base   = base;
    pport->inst   = inst;
    pport->rate   = rate;
    pport->intr   = intr;
    pport->modid  = ID;
    strcpy(pport->name,port);
    pport->syncLock = epicsMutexMustCreate();

    pports[inst] = pport;
    epicsThreadLowestPriorityLevelAbove(epicsThreadPriorityHigh,&prio);
    epicsThreadCreate("scanScaler059",prio,epicsThreadGetStackSize(epicsThreadStackSmall),(EPICSTHREADFUNC)scanScaler,pport);

    return( 0 );
}


static void scanScaler(Port* pport)
{
    int cnt;
    unsigned char status;
    double freq = 1.0 / pport->rate;

    // Wait for record to initialize
    while( pport->psr == NULL ) epicsThreadSleep(freq);

    // Process Scaler
    while(0==0)
    {
        if( pport->intr && pport->psr->cnt == 0 ) ioctl(pport->fd,7,&cnt); else epicsThreadSleep(freq);
        if( pport->poll == FALSE ) continue;
        if( pport->psr->cnt ) scalerRead(pport);

        epicsMutexMustLock(pport->syncLock);
        scalerStatus(pport->base);
        status = readDataLB(pport->base);
        epicsMutexUnlock(pport->syncLock);

        if( status & DONE ) ++pport->scancnt; else continue;

        scalerRead(pport);
        pport->done = 1;
        pport->poll = FALSE;
        callbackRequest(pport->pcallback);

//printf("scanScaler() - \"%s\" - done - (%d)\n",pport->name,pport->scancnt);
    }
   
}


/****************************************************************************
 * Define device support private interface
 ****************************************************************************/
static void scalerRead(Port* pport)
{
    unsigned char b0,b1,b2,b3;

    epicsMutexMustLock(pport->syncLock);
    for( int i=0; i<SCALERS; ++i )
    {
        scalerDataL(pport->base,(i+1))
        b0=readDataLB(pport->base);
        b1=readDataHB(pport->base);

        scalerDataH(pport->base,(i+1))
        b2=readDataLB(pport->base);
        b3=readDataHB(pport->base);

        pport->scaler[i].preval = pport->scaler[i].value;
        pport->scaler[i].value  = (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
    }
    epicsMutexUnlock(pport->syncLock);
}


static void serialLink(int base,int ver,int ctr,int fnc,int sfn,int usr,epicsInt32 preset)
{
    int i,dat;
    unsigned int sld,msk,mask = 0xFF;

    put059(base,2,DIS);
    put059(base,2,ENA);

    // Send preset
    if( preset )
    {
        msk = 1 << ((DAT_SIZ/2)-1);
        sld = preset;

        for(i=0; i<DAT_SIZ; ++i)
        {
            dat = ENA;
            if((i&1)==0)
            {
                dat |= (CLK | ((msk & sld) ? 1 : 0) );
                sld <<= 1;
            }
            dat &= mask;
            put059(base,2,dat);
        }
    }

    // Send command
    msk = 1 << ((SLD_SIZ/2)-1);
    ctr = (ctr << (VER_BIT));
    fnc = (fnc << (VER_BIT+CTR_BIT));
    sfn = (sfn << (VER_BIT+CTR_BIT+FNC_BIT));
    usr = (usr << (VER_BIT+CTR_BIT+FNC_BIT+SFN_BIT));
    sld = usr | sfn | fnc | ctr | ver;

    for(i=0; i<SLD_SIZ; ++i)
    {
        dat = ENA;
        if((i&1)==0)
        {
            dat |= (CLK | ((msk & sld) ? 1 : 0) );
            sld <<= 1;
        }
        dat &= mask;
        put059(base,2,dat);
    }
    put059(base,2,DIS);
}


static long scal059_report(int level)
{
    int i,j;
    unsigned char status;

//printf("scal059_report(%d)\n",level);

    for( i=0; pports[i]; ++i )
    {
        epicsMutexMustLock(pports[i]->syncLock);
        scalerStatus(pports[i]->base);
        status=readDataLB(pports[i]->base);
        epicsMutexUnlock(pports[i]->syncLock);

        printf("%s-%d-%3.3d %dch scaler,%.3fMHz,poll %d/sec,interrupt %s\n",pports[i]->name,pports[i]->inst,pports[i]->modid,SCALERS,(pports[i]->freq/1000000.0),pports[i]->rate,(pports[i]->intr)?"enabled":"disabled");
        printf("    Status flags %s,%s,%s\n",(pports[i]->poll)?"polling":"!polling",(pports[i]->done)?"done":"!done",(pports[i]->psr->cnt)?"cnt":"!cnt");
        printf("    FPGA version %1d.%1d, status %s,%s,%s (0x%0X)\n",MAJVER,MINVER,(status&DONE)?"done":"!done",(status&ARMED)?"armed":"!armed",(status&GROUP)?"grp":"!grp",status);
        printf("    Scan count %d\n",pports[i]->scancnt);
        for( j=0; j<SCALERS; ++j ) printf("    Scaler %2.2d preset %d, value %d\n",(j+1),pports[i]->scaler[j].preset,pports[i]->scaler[j].value);
    }

    return( 0 );
}


static long scal059_hdw_init(int after)
{
    int i;

//printf("scal059_hdw_init(%d)\n",after);

    for( i=0; pports[i]; ++i )
    {
        pports[i]->done = 0;
        pports[i]->poll = FALSE;

        epicsMutexMustLock(pports[i]->syncLock);
        scalerReset(pports[i]->base);
        epicsMutexUnlock(pports[i]->syncLock);
    }

    return( 0 );
}


static long scal059_rec_init(scalerRecord* psr,CALLBACK *pcallback)
{
    int addr;
    Port* pport;
    asynStatus sts;
    asynUser* pasynUser;
    char *port,*userParam;

//printf("scal059_rec_init(psr)\n");

    switch( psr->out.type )
    {
    case INST_IO:
        pasynUser = pasynManager->createAsynUser(0,0);
        sts = pasynEpicsUtils->parseLink(pasynUser,&psr->out,&port,&addr,&userParam);
        pport = pports[addr];
        break;
    default:
        recGblRecordError(S_dev_badBus,(void*)psr,"devAsynScaler: scal059_rec_init - bad bus type");
        return(S_dev_badBus);
    }

    psr->nch = SCALERS;
    psr->dpvt = pport;

    pport->pcallback = pcallback;
    pport->freq = psr->freq;
    pport->psr = psr;

    return( 0 );
}


static long scal059_rec_reset(scalerRecord* psr)
{
    Port* pport = (Port*)psr->dpvt;

//printf("scal059_rec_reset(%s)\n",pport->name);

    pport->done = 0;
    pport->poll = FALSE;
    for( int i=0; i<SCALERS; ++i ) pport->scaler[i].preset=0;

    epicsMutexMustLock(pport->syncLock);
    scalerReset(pport->base);
    epicsMutexUnlock(pport->syncLock);

    return( 0 );
}


static long scal059_rec_read(scalerRecord* psr,long* val)
{
    int i;
    Port* pport = (Port*)psr->dpvt;

//printf("scal059_rec_read(%s)\n",pport->name);

    for( i=0; i<SCALERS; ++i ) val[i] = (long)pport->scaler[i].preset + (long)pport->scaler[i].value;
    return( 0 );
}


static long scal059_rec_preset(scalerRecord* psr,int signal,long val)
{
    epicsInt32 preset;
    Port* pport = (Port*)psr->dpvt;

//printf("scal059_rec_preset(%s,%d,%d)\n",pport->name,signal,(int)val);

    preset = (epicsInt32)val * -1;
    epicsMutexMustLock(pport->syncLock);
    scalerPreset(pport->base,(signal+1),preset);
    epicsMutexUnlock(pport->syncLock);
    pport->scaler[signal].preset = (epicsInt32)val;

    return( 0 );
}


static long scal059_rec_arm(scalerRecord* psr,int val)
{
    Port* pport = (Port*)psr->dpvt;

//printf("scal059_rec_arm(%s,%d)\n",pport->name,val);

    epicsMutexMustLock(pport->syncLock);
    if( val )
    {
        scalerGroup(pport->base);
        scalerArm(pport->base);

        pport->done = 0;
        pport->poll = TRUE;
    }
    else
    {
        pport->poll = FALSE;
        pport->done = 1;

        scalerDisarm(pport->base);
        scalerUngroup(pport->base);
    }
    epicsMutexUnlock(pport->syncLock);

    return( 0 );
}


static long scal059_rec_done(scalerRecord* psr)
{
    Port* pport = (Port*)psr->dpvt;

//printf("scal059_rec_done(%s) - done %d - cnt %d - cont %d\n",pport->name,pport->done,pport->psr->cnt,pport->psr->cont);

    if( pport->done )
    {
        pport->done = 0;
        pport->poll = FALSE;
        scalerDisarm(pport->base);

        return( 1 );
    }

    return( 0 );
}


/****************************************************************************
 * Register public methods
 ****************************************************************************/
/* Initialization method definitions */
static const iocshArg devAsynScaler059InitArg0 = {"port",iocshArgString};
static const iocshArg devAsynScaler059InitArg1 = {"base",iocshArgInt};
static const iocshArg devAsynScaler059InitArg2 = {"inst",iocshArgInt};
static const iocshArg devAsynScaler059InitArg3 = {"rate",iocshArgInt};
static const iocshArg devAsynScaler059InitArg4 = {"intr",iocshArgInt};
static const iocshArg* devAsynScaler059InitArgs[]= {&devAsynScaler059InitArg0,&devAsynScaler059InitArg1,&devAsynScaler059InitArg2,&devAsynScaler059InitArg3,&devAsynScaler059InitArg4};
static const iocshFuncDef devAsynScaler059InitFuncDef = {"devAsynScaler059Init",5,devAsynScaler059InitArgs};
static void devAsynScaler059InitCallFunc(const iocshArgBuf* args)
{
    devAsynScaler059Init(args[0].sval,args[1].ival,args[2].ival,args[3].ival,args[4].ival);
}

/* Registration method */
static void devAsynScaler059Register(void)
{
    static int firstTime = 1;

    if( firstTime )
    {
        firstTime = 0;
        iocshRegister(&devAsynScaler059InitFuncDef,devAsynScaler059InitCallFunc);
    }
}
epicsExportRegistrar( devAsynScaler059Register );
