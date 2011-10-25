/*

                          Argonne National Laboratory
                            APS Operations Division
                     Beamline Controls and Data Acquisition

            Stanford Research Systems DG645 Digital Delay Generator
                               Asyn Port Driver



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
    This module provides support for a multiple device port driver. To
    initialize the driver, the method drvAsynDG645() is called from the
    startup script with the following calling sequence.

        drvAsynDG645(myport,ioport,ioaddr)

        Where:
            myport - DG645 Asyn interface port driver name (i.e. "DG0" )
            ioport - Communication port driver name (i.e. "S0" )
            ioaddr - Communication port device addr

    The method dbior can be called from the IOC shell to display the current
    status of the driver.


 Developer notes:


 Source control info:
    Modified by:    $Author: dkline $
                    $Date: 2009/03/11 18:43:29 $
                    $Revision: 1.3 $

 =============================================================================
 History:
 Author: David M. Kline
 -----------------------------------------------------------------------------
 2009-Jan-31  DMK  Derived support from drvAsynColby driver.
 2009-Feb-11  DMK  Initial development version V1.0 complete.
 2009-Feb-11  DMK  Development version V1.1 complete.
 2009-Mar-11  DMK  Removed calls to free() during initialization to eliminate
                   a segmentation fault (on soft IOCs) when a DG645 is not
                   connected. This was successful on an EBRICK. 
 2009-Mar-30  DMK  Allow multiple instances of instrument.
 2010-Sep-30  DMK  Solidified multiple instance implementation.
 -----------------------------------------------------------------------------

*/


/* EPICS base version-specific definitions (must be performed first) */
#include <epicsVersion.h>
#define LT_EPICSBASE(v,r,l) (EPICS_VERSION<(v)||(EPICS_VERSION==(v)&&(EPICS_REVISION<(r)||(EPICS_REVISION==(r)&&EPICS_MODIFICATION<(l)))))


/* Evaluate EPICS base */
#if LT_EPICSBASE(3,14,7)
    #error "EPICS base must be 3.14.7 or greater"
#endif

/* System related include files */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* EPICS system related include files */
#include <iocsh.h>
#include <epicsStdio.h>
#include <cantProceed.h>
#include <epicsString.h>
#include <epicsThread.h>


/* EPICS synApps/Asyn related include files */
#include <asynDriver.h>
#include <asynDrvUser.h>
#include <asynInt32.h>
#include <asynFloat64.h>
#include <asynOctet.h>
#include <asynOctetSyncIO.h>
#include <asynUInt32Digital.h>

/* epicsExport.h must come last */
#include <epicsExport.h>


/* Define symbolic constants */
#define TIMEOUT         (3.0)
#define BUFFER_SIZE     (100)
#define INSTANCES       (10)

/* Forward struct declarations */
typedef struct Port Port;
typedef struct ioPvt ioPvt;
typedef struct Status Status;
typedef struct Command Command;
typedef enum {ifaceAsynOctet,ifaceAsynFloat64,ifaceAsynUInt32} ifaceType;

/* Struct declaration (taken from asynOctetSyncIO.c) */
struct ioPvt
{
   asynCommon  *pasynCommon;
   void        *pcommonPvt;
   asynInt32   *pasynInt32;
   void        *int32Pvt;
   asynDrvUser *pasynDrvUser;
   void        *drvUserPvt;
};


/* Declare port driver structure */
struct Port
{
    char* myport;
    char* ioport;
    int   index;
    int   ioaddr;
    int   discos;
    int   writeReads;
    int   writeOnlys;
    int   outputMode[5];
    char  ident[BUFFER_SIZE];
    epicsMutexId syncLock;

    int pvs;
    int refs;
    int init;
    int conns;
    int error;

    /* Asyn info */
    asynUser* pasynUser;
    asynInterface asynOctet;
    asynInterface asynCommon;
    asynInterface asynDrvUser;
    asynInterface asynUInt32;
    asynInterface asynFloat64;
};


/* Declare status message structure */
struct Status
{
    int code;
    char* msg;
};


/* Declare command structure */
struct Command
{
    int refs;
    int ident;
    int channel;
    Port* pport;

    char* readCommand;
    asynStatus (*readFunc)(Command* pcommand,asynUser* pasynUser,char* inpBuf,int inputSize,int* eomReason,ifaceType asynIface);
    int (*readConv)(asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface);

    char* writeCommand;
    asynStatus (*writeFunc)(Command* pcommand,asynUser* pasynUser,void* data,ifaceType asynIface);

    char* desc;
};


/* Public interface forward references */
int drvAsynDG645(const char* myport,const char* ioport,int ioaddr);


/* Forward references for asynCommon methods */
static void report(void* ppvt,FILE* fp,int details);
static asynStatus connect(void* ppvt,asynUser* pasynUser);
static asynStatus disconnect(void* ppvt,asynUser* pasynUser);
static asynCommon common = {report,connect,disconnect};


/* Forward references for asynDrvUser methods */
static asynStatus create(void* ppvt,asynUser* pasynUser,const char* drvInfo, const char** pptypeName,size_t* psize);
static asynStatus destroy(void* ppvt,asynUser* pasynUser);
static asynStatus gettype(void* ppvt,asynUser* pasynUser,const char** pptypeName,size_t* psize);
static asynDrvUser drvuser = {create,gettype,destroy};


/* Forward references for asynFloat64 methods */
static asynStatus readFloat64(void* ppvt,asynUser* pasynUser,epicsFloat64* value);
static asynStatus writeFloat64(void* ppvt,asynUser* pasynUser,epicsFloat64 value);


/* Forward references for asynUInt32Digital methods */
static asynStatus readUInt32(void* ppvt,asynUser* pasynUser,epicsUInt32* value,epicsUInt32 mask);
static asynStatus writeUInt32(void* ppvt,asynUser* pasynUser,epicsUInt32 value,epicsUInt32 mask);


/* Forward references for asynOctet methods */
static asynStatus flushOctet(void* ppvt,asynUser* pasynUser);
static asynStatus writeOctet(void* ppvt,asynUser* pasynUser,const char *data,size_t numchars,size_t* nbytes);
static asynStatus readOctet(void* ppvt,asynUser* pasynUser,char* data,size_t maxchars,size_t *nbytes,int *eom);


/* Forward references for external asynOctet interface */
static asynStatus writeOnly(Port* pport,asynUser* pasynUser,char* outBuf);
static asynStatus writeRead(Port* pport,asynUser* pasynUser,char* outBuf,char* inpBuf,int inputSize,int *eomReason);


/* Forward references for utility methods */
static char* findStatus(int code);
static Command* findCommand(int command);

/* Forward references for command table methods */
static asynStatus readSink(Command* pcommand,asynUser* pasynUser,char* inpBuf,int inputSize,int *eomReason,ifaceType asynIface);
static asynStatus readParam(Command* pcommand,asynUser* pasynUser,char* inpBuf,int inputSize,int *eomReason,ifaceType asynIface);

static int cvtSink(asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface);
static int cvtCopyText(asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface);
static int cvtErrorText(asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface);
static int cvtErrorCode(asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface);
static int cvtChanRef(asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface);
static int cvtChanDelay(asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface);

static int cvtStrInt(asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface);
static int cvtStrFloat(asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface);

static asynStatus writeSink(Command* pcommand,asynUser* pasynUser,void* data,ifaceType asynIface);
static asynStatus writeIntParam(Command* pcommand,asynUser* pasynUser,void* data,ifaceType asynIface);
static asynStatus writeStrParam(Command* pcommand,asynUser* pasynUser,void* data,ifaceType asynIface);
static asynStatus writeFloatParam(Command* pcommand,asynUser* pasynUser,void* data,ifaceType asynIface);
static asynStatus writeCommandOnly(Command* pcommand,asynUser* pasynUser,void* data,ifaceType asynIface);
static asynStatus writeChannelRef(Command* pcommand,asynUser* pasynUser,void* data,ifaceType asynIface);
static asynStatus writeChannelDelay(Command* pcommand,asynUser* pasynUser,void* data,ifaceType asynIface);


/* Define local variants */
static Port* pports[INSTANCES] = {NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};
static char* delayText[] = {"T0", "T1", "A", "B", "C", "D", "E", "F", "G", "H"};
static Status statusMsg[] =
{
    {  0, "STATUS OK"},
    { 10, "Illegal Value"},
    { 11, "Illegal Mode"},
    { 12, "Illegal Delay"},
    { 13, "Illegal Link"},
    { 14, "Recall Failed"},
    { 15, "Not Allowed"},
    { 16, "Failed Self Test"},
    { 17, "Failed Auto Calibration"},
    { 30, "Lost Data"},
    { 32, "No Listener"},
    { 40, "Failed ROM Check"},
    { 41, "Failed Offset T0 Test"},
    { 42, "Failed Offset AB Test"},
    { 43, "Failed Offset CD Test"},
    { 44, "Failed Offset EF Test"},
    { 45, "Failed Offset GH Test"},
    { 46, "Failed Amplitude T0 Test"},
    { 47, "Failed Amplitude AB Test"},
    { 48, "Failed Amplitude CD Test"},
    { 49, "Failed Amplitude EF Test"},
    { 50, "Failed Amplitude GH Test"},
    { 51, "Failed FPGA Communications Test"},
    { 52, "Failed GPIB Communications Test"},
    { 53, "Failed DDS Communications Test"},
    { 54, "Failed Serial EEPROM Communications Test"},
    { 55, "Failed Temperature Sensor Communications Test"},
    { 56, "Failed PLL Communications Test"},
    { 57, "Failed DAC 0 Communications Test"},
    { 58, "Failed DAC 1 Communications Test"},
    { 59, "Failed DAC 2 Communications Test"},
    { 60, "Failed Sample and Hold Operations Test"},
    { 61, "Failed Vjitter Operations Test"},
    { 62, "Failed Channel T0 Analog Delay Test"},
    { 63, "Failed Channel T1 Analog Delay Test"},
    { 64, "Failed Channel A Analog Delay Test"},
    { 65, "Failed Channel B Analog Delay Test"},
    { 66, "Failed Channel C Analog Delay Test"},
    { 67, "Failed Channel D Analog Delay Test"},
    { 68, "Failed Channel E Analog Delay Test"},
    { 69, "Failed Channel F Analog Delay Test"},
    { 70, "Failed Channel G Analog Delay Test"},
    { 71, "Failed Channel H Analog Delay Test"},
    { 80, "Failed Sample and Hold Calibration"},
    { 81, "Failed T0 Calibration"},
    { 82, "Failed T1 Calibration"},
    { 83, "Failed A Calibration"},
    { 84, "Failed B Calibration"},
    { 85, "Failed C Calibration"},
    { 86, "Failed D Calibration"},
    { 87, "Failed E Calibration"},
    { 88, "Failed F Calibration"},
    { 89, "Failed G Calibration"},
    { 90, "Failed H Calibration"},
    { 91, "Failed Vjitter Calibration"},
    {110, "Illegal Command"},
    {111, "Undefined Comand"},
    {112, "Illegal Query"},
    {113, "Illegal Set"},
    {114, "Null Parameter"},
    {115, "Extra Parameters"},
    {116, "Missing Parameters"},
    {117, "Parameter Overflow"},
    {118, "Invalid Floating Point Number"},
    {120, "Invalid Integer"},
    {121, "Integer Overflow"},
    {122, "Invalid Hexidecimal"},
    {126, "Syntax Error"},
    {170, "Communication Error"},
    {171, "Over run"},
    {254, "Too Many Errors"},
};
static int statusLen=sizeof(statusMsg)/sizeof(Status);

static Command commandTable[] = 
{
//-------------------------------------------------------------------------------------------------------------------------------------
//   refs   ident   channel pport   readCommand readFunc        readConv        writeCommand    writeFunc           Desc
//-------------------------------------------------------------------------------------------------------------------------------------

    // Instrument management related commands
    {0,     0,      0,      NULL,   "*IDN?",    readParam,      cvtCopyText,    "",             writeSink,          "Ident"},
    {0,     1,      0,      NULL,   "LERR?",    readParam,      cvtErrorText,   "",             writeSink,          "Status Text"},
    {0,     2,      0,      NULL,   "",         readSink,       cvtErrorCode,   "",             writeSink,          "Status Code"},
    {0,     3,      0,      NULL,   "",         readSink,       cvtSink,        "*CLS",         writeCommandOnly,   "Status Clear"},
    {0,     4,      0,      NULL,   "",         readSink,       cvtSink,        "*RST",         writeCommandOnly,   "Reset Instrument"},
    {0,     5,      0,      NULL,   "",         readSink,       cvtSink,        "LCAL",         writeCommandOnly,   "Goto Local"},
    {0,     6,      0,      NULL,   "",         readSink,       cvtSink,        "REMT",         writeCommandOnly,   "Goto Remote"},

    // Trigger related commands
    {0,    10,      0,      NULL,   "TLVL?",    readParam,      cvtStrFloat,    "TLVL%-.2f",    writeFloatParam,    "Trigger Level"},
    {0,    11,      0,      NULL,   "TRAT?",    readParam,      cvtStrFloat,    "TRAT%-.6f",    writeFloatParam,    "Trigger Rate"},
    {0,    12,      0,      NULL,   "TSRC?",    readParam,      cvtStrInt,      "TSRC%d",       writeIntParam,      "Trigger Source"},
    {0,    13,      0,      NULL,   "INHB?",    readParam,      cvtStrInt,      "INHB%d",       writeIntParam,      "Trigger Inhibit"},
    {0,    14,      0,      NULL,   "",         readSink,       cvtSink,        "*TRG",         writeCommandOnly,   "Trigger Delay"},
                                                                
    // Burst mode related commands
    {0,    20,      0,      NULL,   "BURM?",    readParam,      cvtStrInt,      "BURM%d",       writeIntParam,      "Burst Mode"},
    {0,    21,      0,      NULL,   "BURC?",    readParam,      cvtStrInt,      "BURC%d",       writeIntParam,      "Burst Count"},
    {0,    22,      0,      NULL,   "BURT?",    readParam,      cvtStrInt,      "BURT%d",       writeIntParam,      "Burst T0 Config"},
    {0,    23,      0,      NULL,   "BURD?",    readParam,      cvtStrFloat,    "BURD%e",       writeFloatParam,    "Burst Delay"},
    {0,    24,      0,      NULL,   "BURP?",    readParam,      cvtStrFloat,    "BURP%e",       writeFloatParam,    "Burst Period"},
                                                                
    // Interface configuration related commands
    {0,    30,      0,      NULL,   "IFCF?0",   readParam,      cvtStrInt,      "IFCF 0,%d",    writeIntParam,      "Serial enable/disable"},
    {0,    31,      0,      NULL,   "IFCF?1",   readParam,      cvtStrInt,      "IFCF 1,%d",    writeIntParam,      "Serial baud rate"},
    {0,    32,      0,      NULL,   "IFCF?2",   readParam,      cvtStrInt,      "IFCF 2,%d",    writeIntParam,      "GPIB enable/disable"},
    {0,    33,      0,      NULL,   "IFCF?3",   readParam,      cvtStrInt,      "IFCF 3,%d",    writeIntParam,      "GPIB address"},
    {0,    34,      0,      NULL,   "IFCF?4",   readParam,      cvtStrInt,      "IFCF 4,%d",    writeIntParam,      "LAN TCP/IP enable/disable"},
    {0,    35,      0,      NULL,   "IFCF?5",   readParam,      cvtStrInt,      "IFCF 5,%d",    writeIntParam,      "DHCP enable/disable"},
    {0,    36,      0,      NULL,   "IFCF?6",   readParam,      cvtStrInt,      "IFCF 6,%d",    writeIntParam,      "Auto-IP enable/disable"},
    {0,    37,      0,      NULL,   "IFCF?7",   readParam,      cvtStrInt,      "IFCF 7,%d",    writeIntParam,      "Static-IP enable/disable"},
    {0,    38,      0,      NULL,   "IFCF?8",   readParam,      cvtStrInt,      "IFCF 8,%d",    writeIntParam,      "Bare socket enable/disable"},
    {0,    39,      0,      NULL,   "IFCF?9",   readParam,      cvtStrInt,      "IFCF 9,%d",    writeIntParam,      "Telnet enable/disable"},
    {0,    40,      0,      NULL,   "IFCF?10",  readParam,      cvtStrInt,      "IFCF 10,%d",   writeIntParam,      "VXI-11 enable/disable"},
    {0,    41,      0,      NULL,   "IFCF?11",  readParam,      cvtCopyText,    "IFCF 11,%s",   writeStrParam,      "Static IP address"},
    {0,    42,      0,      NULL,   "IFCF?12",  readParam,      cvtCopyText,    "IFCF 12,%s",   writeStrParam,      "Network mask"},
    {0,    43,      0,      NULL,   "IFCF?13",  readParam,      cvtCopyText,    "IFCF 13,%s",   writeStrParam,      "Gateway IP address"},

    {0,    44,      0,      NULL,   "",         readSink,       cvtSink,        "IFRS 0",       writeCommandOnly,   "Serial reset"},
    {0,    45,      0,      NULL,   "",         readSink,       cvtSink,        "IFRS 1",       writeCommandOnly,   "GPIB reset"},
    {0,    46,      0,      NULL,   "",         readSink,       cvtSink,        "IFRS 2",       writeCommandOnly,   "LAN reset"},

    {0,    47,      0,      NULL,   "EMAC?",    readParam,      cvtCopyText,    "",             writeSink,          "MAC Address"},

    // Delay channel A related commands
    {0,   100,      2,      NULL,   "DLAY?2",   readParam,      cvtChanRef,     "DLAY 2,%d,%e", writeChannelRef,    "Channel A ref"},
    {0,   101,      2,      NULL,   "DLAY?2",   readParam,      cvtChanDelay,   "DLAY 2,%d,%e", writeChannelDelay,  "Channel A delay"},
    {0,   102,      2,      NULL,   "",         readSink,       cvtSink,        "SPDL 2,0",     writeCommandOnly,   "Channel A step delay minus"},
    {0,   103,      2,      NULL,   "",         readSink,       cvtSink,        "SPDL 2,1",     writeCommandOnly,   "Channel A step delay plus"},
    {0,   104,      2,      NULL,   "SSDL?2",   readParam,      cvtStrFloat,    "SSDL 2,%e",    writeFloatParam,    "Channel A step delay size"},
                                                                
    // Delay channel B related commands
    {0,   110,      3,      NULL,   "DLAY?3",   readParam,      cvtChanRef,     "DLAY 3,%d,%e", writeChannelRef,    "Channel B ref"},
    {0,   111,      3,      NULL,   "DLAY?3",   readParam,      cvtChanDelay,   "DLAY 3,%d,%e", writeChannelDelay,  "Channel B delay"},
    {0,   112,      3,      NULL,   "",         readSink,       cvtSink,        "SPDL 3,0",     writeCommandOnly,   "Channel B step delay minus"},
    {0,   113,      3,      NULL,   "",         readSink,       cvtSink,        "SPDL 3,1",     writeCommandOnly,   "Channel B step delay plus"},
    {0,   114,      3,      NULL,   "SSDL?3",   readParam,      cvtStrFloat,    "SSDL 3,%e",    writeFloatParam,    "Channel B step delay size"},
                                                                
    // Delay channel C related commands
    {0,   120,      4,      NULL,   "DLAY?4",   readParam,      cvtChanRef,     "DLAY 4,%d,%e", writeChannelRef,    "Channel C ref"},
    {0,   121,      4,      NULL,   "DLAY?4",   readParam,      cvtChanDelay,   "DLAY 4,%d,%e", writeChannelDelay,  "Channel C delay"},
    {0,   122,      4,      NULL,   "",         readSink,       cvtSink,        "SPDL 4,0",     writeCommandOnly,   "Channel C step delay minus"},
    {0,   123,      4,      NULL,   "",         readSink,       cvtSink,        "SPDL 4,1",     writeCommandOnly,   "Channel C step delay plus"},
    {0,   124,      4,      NULL,   "SSDL?4",   readParam,      cvtStrFloat,    "SSDL 4,%e",    writeFloatParam,    "Channel C step delay size"},
                                                                
    // Delay channel D related commands
    {0,   130,      5,      NULL,   "DLAY?5",   readParam,      cvtChanRef,     "DLAY 5,%d,%e", writeChannelRef,    "Channel D ref"},
    {0,   131,      5,      NULL,   "DLAY?5",   readParam,      cvtChanDelay,   "DLAY 5,%d,%e", writeChannelDelay,  "Channel D delay"},
    {0,   132,      5,      NULL,   "",         readSink,       cvtSink,        "SPDL 5,0",     writeCommandOnly,   "Channel D step delay minus"},
    {0,   133,      5,      NULL,   "",         readSink,       cvtSink,        "SPDL 5,1",     writeCommandOnly,   "Channel D step delay plus"},
    {0,   134,      5,      NULL,   "SSDL?5",   readParam,      cvtStrFloat,    "SSDL 5,%e",    writeFloatParam,    "Channel D step delay size"},
                                                                
    // Delay channel E related commands
    {0,   140,      6,      NULL,   "DLAY?6",   readParam,      cvtChanRef,     "DLAY 6,%d,%e", writeChannelRef,    "Channel E ref"},
    {0,   141,      6,      NULL,   "DLAY?6",   readParam,      cvtChanDelay,   "DLAY 6,%d,%e", writeChannelDelay,  "Channel E delay"},
    {0,   142,      6,      NULL,   "",         readSink,       cvtSink,        "SPDL 6,0",     writeCommandOnly,   "Channel E step delay minus"},
    {0,   143,      6,      NULL,   "",         readSink,       cvtSink,        "SPDL 6,1",     writeCommandOnly,   "Channel E step delay plus"},
    {0,   144,      6,      NULL,   "SSDL?6",   readParam,      cvtStrFloat,    "SSDL 6,%e",    writeFloatParam,    "Channel E step delay size"},
                                                                
    // Delay channel F related commands
    {0,   150,      7,      NULL,   "DLAY?7",   readParam,      cvtChanRef,     "DLAY 7,%d,%e", writeChannelRef,    "Channel F ref"},
    {0,   151,      7,      NULL,   "DLAY?7",   readParam,      cvtChanDelay,   "DLAY 7,%d,%e", writeChannelDelay,  "Channel F delay"},
    {0,   152,      7,      NULL,   "",         readSink,       cvtSink,        "SPDL 7,0",     writeCommandOnly,   "Channel F step delay minus"},
    {0,   153,      7,      NULL,   "",         readSink,       cvtSink,        "SPDL 7,1",     writeCommandOnly,   "Channel F step delay plus"},
    {0,   154,      7,      NULL,   "SSDL?7",   readParam,      cvtStrFloat,    "SSDL 7,%e",    writeFloatParam,    "Channel F step delay size"},
                                                                
    // Delay channel G related commands
    {0,   160,      8,      NULL,   "DLAY?8",   readParam,      cvtChanRef,     "DLAY 8,%d,%e", writeChannelRef,    "Channel G ref"},
    {0,   161,      8,      NULL,   "DLAY?8",   readParam,      cvtChanDelay,   "DLAY 8,%d,%e", writeChannelDelay,  "Channel G delay"},
    {0,   162,      8,      NULL,   "",         readSink,       cvtSink,        "SPDL 8,0",     writeCommandOnly,   "Channel G step delay minus"},
    {0,   163,      8,      NULL,   "",         readSink,       cvtSink,        "SPDL 8,1",     writeCommandOnly,   "Channel G step delay plus"},
    {0,   164,      8,      NULL,   "SSDL?8",   readParam,      cvtStrFloat,    "SSDL 8,%e",    writeFloatParam,    "Channel G step delay size"},
                                                                
    // Delay channel H related commands
    {0,   170,      9,      NULL,   "DLAY?9",   readParam,      cvtChanRef,     "DLAY 9,%d,%e", writeChannelRef,    "Channel H ref"},
    {0,   171,      9,      NULL,   "DLAY?9",   readParam,      cvtChanDelay,   "DLAY 9,%d,%e", writeChannelDelay,  "Channel H delay"},
    {0,   172,      9,      NULL,   "",         readSink,       cvtSink,        "SPDL 9,0",     writeCommandOnly,   "Channel H step delay minus"},
    {0,   173,      9,      NULL,   "",         readSink,       cvtSink,        "SPDL 9,1",     writeCommandOnly,   "Channel H step delay plus"},
    {0,   174,      9,      NULL,   "SSDL?9",   readParam,      cvtStrFloat,    "SSDL 9,%e",    writeFloatParam,    "Channel H step delay size"},
                                                                
    // T0 output commands
    {0,   200,      0,      NULL,   "LAMP?0",   readParam,      cvtStrFloat,    "LAMP 0,%-.2f", writeFloatParam,    "T0 output amplitude"},
    {0,   201,      0,      NULL,   "LOFF?0",   readParam,      cvtStrFloat,    "LOFF 0,%-.2f", writeFloatParam,    "T0 output offset"},
    {0,   202,      0,      NULL,   "LPOL?0",   readParam,      cvtStrInt,      "LPOL 0,%d",    writeIntParam,      "T0 output polarity"},
    {0,   203,      0,      NULL,   "",         readSink,       cvtSink,        "SPLA 0,0",     writeCommandOnly,   "T0 step amplitude minus"},
    {0,   204,      0,      NULL,   "",         readSink,       cvtSink,        "SPLA 0,1",     writeCommandOnly,   "T0 step amplitude plus"},
    {0,   205,      0,      NULL,   "SSLA?0",   readParam,      cvtStrFloat,    "SSLA 0,%-.2f", writeFloatParam,    "T0 step amplitude size"},
    {0,   206,      0,      NULL,   "",         readSink,       cvtSink,        "SPLO 0,0",     writeCommandOnly,   "T0 step offset minus"},
    {0,   207,      0,      NULL,   "",         readSink,       cvtSink,        "SPLO 0,1",     writeCommandOnly,   "T0 step offset plus"},
    {0,   208,      0,      NULL,   "SSLO?0",   readParam,      cvtStrFloat,    "SSLO 0,%-.2f", writeFloatParam,    "T0 step offset size"},

    // AB output commands
    {0,   210,      1,      NULL,   "LAMP?1",   readParam,      cvtStrFloat,    "LAMP 1,%-.2f", writeFloatParam,    "AB output amplitude"},
    {0,   211,      1,      NULL,   "LOFF?1",   readParam,      cvtStrFloat,    "LOFF 1,%-.2f", writeFloatParam,    "AB output offset"},
    {0,   212,      1,      NULL,   "LPOL?1",   readParam,      cvtStrInt,      "LPOL 1,%d",    writeIntParam,      "AB output polarity"},
    {0,   213,      1,      NULL,   "",         readSink,       cvtSink,        "SPLA 1,0",     writeCommandOnly,   "AB step amplitude minus"},
    {0,   214,      1,      NULL,   "",         readSink,       cvtSink,        "SPLA 1,1",     writeCommandOnly,   "AB step amplitude plus"},
    {0,   215,      1,      NULL,   "SSLA?1",   readParam,      cvtStrFloat,    "SSLA 1,%-.2f", writeFloatParam,    "AB step amplitude size"},
    {0,   216,      1,      NULL,   "",         readSink,       cvtSink,        "SPLO 1,0",     writeCommandOnly,   "AB step offset minus"},
    {0,   217,      1,      NULL,   "",         readSink,       cvtSink,        "SPLO 1,1",     writeCommandOnly,   "AB step offset plus"},
    {0,   218,      1,      NULL,   "SSLO?1",   readParam,      cvtStrFloat,    "SSLO 1,%-.2f", writeFloatParam,    "AB step offset size"},

    // CD output commands
    {0,   220,      2,      NULL,   "LAMP?2",   readParam,      cvtStrFloat,    "LAMP 2,%-.2f", writeFloatParam,    "CD output amplitude"},
    {0,   221,      2,      NULL,   "LOFF?2",   readParam,      cvtStrFloat,    "LOFF 2,%-.2f", writeFloatParam,    "CD output offset"},
    {0,   222,      2,      NULL,   "LPOL?2",   readParam,      cvtStrInt,      "LPOL 2,%d",    writeIntParam,      "CD output polarity"},
    {0,   223,      2,      NULL,   "",         readSink,       cvtSink,        "SPLA 2,0",     writeCommandOnly,   "CD step amplitude minus"},
    {0,   224,      2,      NULL,   "",         readSink,       cvtSink,        "SPLA 2,1",     writeCommandOnly,   "CD step amplitude plus"},
    {0,   225,      2,      NULL,   "SSLA?2",   readParam,      cvtStrFloat,    "SSLA 2,%-.2f", writeFloatParam,    "CD step amplitude size"},
    {0,   226,      2,      NULL,   "",         readSink,       cvtSink,        "SPLO 2,0",     writeCommandOnly,   "CD step offset minus"},
    {0,   227,      2,      NULL,   "",         readSink,       cvtSink,        "SPLO 2,1",     writeCommandOnly,   "CD step offset plus"},
    {0,   228,      2,      NULL,   "SSLO?2",   readParam,      cvtStrFloat,    "SSLO 2,%-.2f", writeFloatParam,    "CD step offset size"},

    // EF output commands
    {0,   230,      3,      NULL,   "LAMP?3",   readParam,      cvtStrFloat,    "LAMP 3,%-.2f", writeFloatParam,    "EF output amplitude"},
    {0,   231,      3,      NULL,   "LOFF?3",   readParam,      cvtStrFloat,    "LOFF 3,%-.2f", writeFloatParam,    "EF output offset"},
    {0,   232,      3,      NULL,   "LPOL?3",   readParam,      cvtStrInt,      "LPOL 3,%d",    writeIntParam,      "EF output polarity"},
    {0,   233,      3,      NULL,   "",         readSink,       cvtSink,        "SPLA 3,0",     writeCommandOnly,   "EF step amplitude minus"},
    {0,   234,      3,      NULL,   "",         readSink,       cvtSink,        "SPLA 3,1",     writeCommandOnly,   "EF step amplitude plus"},
    {0,   235,      3,      NULL,   "SSLA?3",   readParam,      cvtStrFloat,    "SSLA 3,%-.2f", writeFloatParam,    "EF step amplitude size"},
    {0,   236,      3,      NULL,   "",         readSink,       cvtSink,        "SPLO 3,0",     writeCommandOnly,   "EF step offset minus"},
    {0,   237,      3,      NULL,   "",         readSink,       cvtSink,        "SPLO 3,1",     writeCommandOnly,   "EF step offset plus"},
    {0,   238,      3,      NULL,   "SSLO?3",   readParam,      cvtStrFloat,    "SSLO 3,%-.2f", writeFloatParam,    "EF step offset size"},

    // GH output commands
    {0,   240,      4,      NULL,   "LAMP?4",   readParam,      cvtStrFloat,    "LAMP 4,%-.2f", writeFloatParam,    "GH output amplitude"},
    {0,   241,      4,      NULL,   "LOFF?4",   readParam,      cvtStrFloat,    "LOFF 4,%-.2f", writeFloatParam,    "GH output offset"},
    {0,   242,      4,      NULL,   "LPOL?4",   readParam,      cvtStrInt,      "LPOL 4,%d",    writeIntParam,      "GH output polarity"},
    {0,   243,      4,      NULL,   "",         readSink,       cvtSink,        "SPLA 4,0",     writeCommandOnly,   "GH step amplitude minus"},
    {0,   244,      4,      NULL,   "",         readSink,       cvtSink,        "SPLA 4,1",     writeCommandOnly,   "GH step amplitude plus"},
    {0,   245,      4,      NULL,   "SSLA?4",   readParam,      cvtStrFloat,    "SSLA 4,%-.2f", writeFloatParam,    "GH step amplitude size"},
    {0,   246,      4,      NULL,   "",         readSink,       cvtSink,        "SPLO 4,0",     writeCommandOnly,   "GH step offset minus"},
    {0,   247,      4,      NULL,   "",         readSink,       cvtSink,        "SPLO 4,1",     writeCommandOnly,   "GH step offset plus"},
    {0,   248,      4,      NULL,   "SSLO?4",   readParam,      cvtStrFloat,    "SSLO 4,%-.2f", writeFloatParam,    "GH step offset size"},

};
static int commandLen=sizeof(commandTable)/sizeof(Command);


/* Define macros */
#define ASYN_SUCCESS(s) (asynSuccess==(s))
#define ASYN_ERROR(s)   (!ASYN_SUCCESS(s))
#define MIN(a,b)        ((a)>(b)?(b):(a))


/****************************************************************************
 * Define public interface methods
 ****************************************************************************/
int drvAsynDG645(const char* myport,const char* ioport,int ioaddr)
{
    Port* pport;
    int len,i,j,eomReason,index;
    char inpBuf[BUFFER_SIZE];
    asynUser* pasynUser;
    asynOctet* pasynOctet;
    asynFloat64* pasynFloat64;
    asynUInt32Digital* pasynUInt32;

    for(index=0;index<INSTANCES;++index) if( pports[index]==NULL ) break;
    if( index==INSTANCES )
    {
        printf("drvAsynDG645:init %s: all interfaces already established\n",myport);
        return( -1 );
    }

    if( pasynOctetSyncIO->connect(ioport,ioaddr,&pasynUser,NULL) )
    {
        printf("drvAsynDG645:init %s: cannot connect to asyn port %s\n",myport,ioport);
        return( -1 );
    }

    i = strlen(myport)+1;
    j = strlen(ioport)+1;

    len = i+j+sizeof(Port)+sizeof(asynFloat64)+sizeof(asynUInt32Digital)+sizeof(asynOctet);
    pport = (Port*)callocMustSucceed(len,sizeof(char),"drvAsynDG645");

    pasynUInt32   = (asynUInt32Digital*)(pport + 1);
    pasynFloat64  = (asynFloat64*)(pasynUInt32 + 1);
    pasynOctet    = (asynOctet*)(pasynFloat64 + 1);
    pport->myport = (char*)(pasynOctet + 1);
    pport->ioport = (char*)(pport->myport + i);

    pport->index     = index;
    pport->ioaddr    = ioaddr;
    pport->pasynUser = pasynUser;
    pport->syncLock  = epicsMutexMustCreate();
    strcpy(pport->myport,myport);
    strcpy(pport->ioport,ioport);

    if( pasynManager->registerPort(myport,ASYN_MULTIDEVICE|ASYN_CANBLOCK,1,0,0) )
    {
        printf("drvAsynDG645::init %s: failure to register port\n",myport);

        return( -1 );
    }

    pport->asynCommon.interfaceType = asynCommonType;
    pport->asynCommon.pinterface = &common;
    pport->asynCommon.drvPvt = pport;

    if( pasynManager->registerInterface(myport,&pport->asynCommon) )
    {
        printf("drvAsynDG645::init %s: failure to register asynCommon\n",myport);

        return( -1 );
    }

    pport->asynDrvUser.interfaceType = asynDrvUserType;
    pport->asynDrvUser.pinterface = &drvuser;
    pport->asynDrvUser.drvPvt = pport;

    if( pasynManager->registerInterface(myport,&pport->asynDrvUser) )
    {
        printf("drvAsynDG645::init %s: failure to register asynDrvUser\n",myport);

        return( -1 );
    }

    pasynFloat64->read = readFloat64;
    pasynFloat64->write = writeFloat64;
    pport->asynFloat64.interfaceType = asynFloat64Type;
    pport->asynFloat64.pinterface = pasynFloat64;
    pport->asynFloat64.drvPvt = pport;

    if( pasynFloat64Base->initialize(myport,&pport->asynFloat64) )
    {
        printf("drvAsynDG645::init %s: failure to initialize asynFloat64Base\n",myport);

        return( -1 );
    }

    pasynUInt32->read = readUInt32;
    pasynUInt32->write = writeUInt32;
    pport->asynUInt32.interfaceType = asynUInt32DigitalType;
    pport->asynUInt32.pinterface = pasynUInt32;
    pport->asynUInt32.drvPvt = pport;

    if( pasynUInt32DigitalBase->initialize(myport,&pport->asynUInt32) )
    {
        printf("drvAsynDG645::init %s: failure to initialize asynUInt32DigitalBase\n",myport);

        return( -1 );
    }

    pasynOctet->flush = flushOctet;
    pasynOctet->read  = readOctet;
    pasynOctet->write = writeOctet;
    pport->asynOctet.drvPvt = pport;
    pport->asynOctet.pinterface = pasynOctet;
    pport->asynOctet.interfaceType = asynOctetType;

    if( pasynOctetBase->initialize(myport,&pport->asynOctet,0,0,0) )
    {
        printf("drvAsynDG645::init %s: failure to initialize asynOctetBase\n",myport);

        return( -1 );
    }

    /* Identification query */
    if( writeRead(pport,pasynUser,"*IDN?",inpBuf,sizeof(inpBuf),&eomReason) )
    {
        printf("drvAsynDG645::init %s: failure to acquire identification\n",myport);

        return( -1 );
    }
    else
        strcpy(pport->ident,inpBuf);

    /* Clear status */
    if( writeOnly(pport,pasynUser,"*CLS") )
    {
        printf("drvAsynDG645::init %s: failure to clear status\n",myport);

        return( -1 );
    }
    else
        strcpy(pport->ident,inpBuf);

    /* Complete initialization */
    pport->init=1;
    pports[index] = pport;
    return( 0 );
}


/****************************************************************************
 * Define private utility methods
 ****************************************************************************/
static char* findStatus(int code)
{
    for(int i=0; i<statusLen; ++i) if( statusMsg[i].code==code ) return( statusMsg[i].msg );
    return( NULL );
}
static Command* findCommand(int ident)
{
    for(int i=0; i<commandLen; ++i) if( commandTable[i].ident==ident ) return( &commandTable[i] );
    return( NULL );
}


/****************************************************************************
 * Define private convert methods
 ****************************************************************************/
static int cvtSink(asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface)
{
    return( 0 );
}
static int cvtCopyText(asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface)
{
    strcpy((char*)outBuf,inpBuf);
    return( MIN((int)strlen((char*)outBuf),maxchars) );
}
static int cvtErrorText(asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface)
{
    int v=atoi(inpBuf);
    char* m=findStatus(v);
    Command* pcommand=(Command*)pasynUser->drvUser;

    if( m ) {strcpy((char*)outBuf,m);pcommand->pport->error=v;} else {strcpy((char*)outBuf,"*ERR*");pcommand->pport->error=0;}
    return( MIN((int)strlen((char*)outBuf),maxchars) );
}
static int cvtErrorCode(asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface)
{
    Command* pcommand=(Command*)pasynUser->drvUser;

    *(epicsUInt32*)outBuf = pcommand->pport->error;
    return( 0 );
}
static int cvtStrInt(asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface)
{
    *(epicsUInt32*)outBuf = atoi(inpBuf);
    return( 0 );
}
static int cvtStrFloat(asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface)
{
    *(epicsFloat64*)outBuf = atof(inpBuf);
    return( 0 );
}
static int cvtChanRef(asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface)
{
    int chan;
    epicsFloat64 delay;

    sscanf(inpBuf,"%d,%lf",&chan,&delay);
    *(epicsUInt32*)outBuf = chan;

    return( 0 );
}
static int cvtChanDelay(asynUser* pasynUser,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface)
{
    int chan,siz;
    epicsFloat64 delay;

    sscanf(inpBuf,"%d,%lf",&chan,&delay);
    switch( asynIface )
    {
    case ifaceAsynOctet:
        sprintf((char*)outBuf,"%s + %-.12f",delayText[chan],delay);
        siz = MIN((int)strlen((char*)outBuf),maxchars);
        break;
    case ifaceAsynFloat64:
        *(epicsFloat64*)outBuf = delay;
        siz = sizeof(epicsFloat64);
        break;
    default:
        siz = 0;
    }

    return( siz );
}


/****************************************************************************
 * Define private read parameter methods
 ****************************************************************************/
static asynStatus readSink(Command* pcommand,asynUser* pasynUser,char* inpBuf,int inputSize,int *eomReason,ifaceType asynIface)
{
    return( asynSuccess );
}
static asynStatus readParam(Command* pcommand,asynUser* pasynUser,char* inpBuf,int inputSize,int* eomReason,ifaceType asynIface)
{
    return( writeRead(pcommand->pport,pasynUser,pcommand->readCommand,inpBuf,inputSize,eomReason) );
}


/****************************************************************************
 * Define private write parameter methods
 ****************************************************************************/
static asynStatus writeSink(Command* pcommand,asynUser* pasynUser,void* data,ifaceType asynIface)
{
    return( asynSuccess );
}
static asynStatus writeIntParam(Command* pcommand,asynUser* pasynUser,void* data,ifaceType asynIface)
{
    char outBuf[BUFFER_SIZE];

    sprintf(outBuf,pcommand->writeCommand,*(epicsUInt32*)data);
    return( writeOnly(pcommand->pport,pasynUser,outBuf) );
}
static asynStatus writeStrParam(Command* pcommand,asynUser* pasynUser,void* data,ifaceType asynIface)
{
    char outBuf[BUFFER_SIZE];

    sprintf(outBuf,pcommand->writeCommand,(char*)data);
    return( writeOnly(pcommand->pport,pasynUser,outBuf) );
}
static asynStatus writeFloatParam(Command* pcommand,asynUser* pasynUser,void* data,ifaceType asynIface)
{
    char outBuf[BUFFER_SIZE];

    sprintf(outBuf,pcommand->writeCommand,*(epicsFloat64*)data);
    return( writeOnly(pcommand->pport,pasynUser,outBuf) );
}
static asynStatus writeCommandOnly(Command* pcommand,asynUser* pasynUser,void* data,ifaceType asynIface)
{
    return( writeOnly(pcommand->pport,pasynUser,pcommand->writeCommand) );
}
static asynStatus writeChannelRef(Command* pcommand,asynUser* pasynUser,void* data,ifaceType asynIface)
{
    asynStatus status;
    int chan,eomReason;
    epicsFloat64 delay;
    char datBuf[BUFFER_SIZE];

    status = writeRead(pcommand->pport,pasynUser,pcommand->readCommand,datBuf,sizeof(datBuf),&eomReason);
    if( ASYN_ERROR(status) ) return( status );

    sscanf(datBuf,"%d,%lf",&chan,&delay);
    sprintf((char*)datBuf,pcommand->writeCommand,*(int*)data,delay);

    return( writeOnly(pcommand->pport,pasynUser,datBuf) );
}
static asynStatus writeChannelDelay(Command* pcommand,asynUser* pasynUser,void* data,ifaceType asynIface)
{
    asynStatus status;
    int chan,eomReason;
    epicsFloat64 delay;
    char datBuf[BUFFER_SIZE];

    status = writeRead(pcommand->pport,pasynUser,pcommand->readCommand,datBuf,sizeof(datBuf),&eomReason);
    if( ASYN_ERROR(status) ) return( status );

    sscanf(datBuf,"%d,%lf",&chan,&delay);
    sprintf((char*)datBuf,pcommand->writeCommand,chan,*(epicsFloat64*)data);

    return( writeOnly(pcommand->pport,pasynUser,datBuf) );
}


/****************************************************************************
 * Define private interface asynCommon methods
 ****************************************************************************/
static void report(void* ppvt,FILE* fp,int details)
{
    int i;
    Port* pport = (Port*)ppvt;

    fprintf(fp,"    %s index %d\n",pport->ident,pport->index);
    fprintf(fp,"    conns %d refs %d pvs %d discos %d writeReads %d writeOnlys %d\n",pport->conns,pport->refs,pport->pvs,pport->discos,pport->writeReads,pport->writeOnlys);
    fprintf(fp,"    support %s initialized\n",(pport->init)?"IS":"IS NOT");
    fprintf(fp,"    myport \"%s\" ioport \"%s\"\n",pport->myport,pport->ioport);
    fprintf(fp,"    total # of commands %d, # of status codes %d\n",commandLen,statusLen);
    for( i=0; i<commandLen; ++i ) if( commandTable[i].refs ) fprintf(fp,"    %d refs for \"%s\" command\n",commandTable[i].refs,commandTable[i].desc);

}
static asynStatus connect(void* ppvt,asynUser* pasynUser)
{
    int addr;
    Port* pport = (Port*)ppvt;

    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );
    ++pport->conns;

    pasynManager->exceptionConnect(pasynUser);
    return( asynSuccess );
}
static asynStatus disconnect(void* ppvt,asynUser* pasynUser)
{
    int addr;
    Port* pport = (Port*)ppvt;

    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );
    --pport->conns;

    pasynManager->exceptionDisconnect(pasynUser);
    return( asynSuccess );
}


/****************************************************************************
 * Define private interface asynDrvUser methods
 ****************************************************************************/
static asynStatus create(void* ppvt,asynUser* pasynUser,const char* drvInfo, const char** pptypeName,size_t* psize)
{
    int addr;
    Command* pcommand;
    Port* pport=(Port*)ppvt;

    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );

    pcommand = findCommand(addr);
    if( pcommand==NULL ) return( asynError );

    ++pport->refs;
    if( addr>=0 ) ++pport->pvs;

    ++pcommand->refs;
    pasynUser->drvUser = pcommand;

    return( asynSuccess );
}
static asynStatus gettype(void* ppvt,asynUser* pasynUser,const char** pptypeName,size_t* psize)
{
    if( pptypeName ) *pptypeName = NULL;
    if( psize ) *psize = 0;

    return( asynSuccess );
}
static asynStatus destroy(void* ppvt,asynUser* pasynUser)
{
    Port* pport=(Port*)ppvt;
    Command* pcommand=(Command*)pasynUser->drvUser;

    if( pport ) --pport->pvs;
    if( pcommand ) --pcommand->refs;

    return( asynSuccess );
}


/****************************************************************************
 * Define private interface asynFloat64 methods
 ****************************************************************************/
static asynStatus writeFloat64(void* ppvt,asynUser* pasynUser,epicsFloat64 value)
{
    int addr;
    asynStatus status;
    Port* pport=(Port*)ppvt;
    Command* pcommand=(Command*)pasynUser->drvUser;

    if( pport->init==0 ) return( asynError );
    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );

    epicsMutexMustLock(pport->syncLock);
    pcommand->pport = pports[pport->index];
    status = pcommand->writeFunc(pcommand,pport->pasynUser,&value,ifaceAsynFloat64);
    epicsMutexUnlock(pport->syncLock);

    return( status );
}
static asynStatus readFloat64(void* ppvt,asynUser* pasynUser,epicsFloat64* value)
{
    int addr,eom;
    asynStatus status;
    char inpBuf[BUFFER_SIZE];
    Port* pport=(Port*)ppvt;
    Command* pcommand=(Command*)pasynUser->drvUser;

    if( pport->init==0 ) return( asynError );
    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );

    epicsMutexMustLock(pport->syncLock);
    pcommand->pport = pports[pport->index];
    status = pcommand->readFunc(pcommand,pport->pasynUser,inpBuf,sizeof(inpBuf),&eom,ifaceAsynFloat64);
    epicsMutexUnlock(pport->syncLock);

    if( ASYN_ERROR(status) ) return( status );
    pcommand->readConv(pasynUser,inpBuf,sizeof(epicsFloat64),value,ifaceAsynFloat64);

    return( status );
}


/****************************************************************************
 * Define private interface asynUInt32Digital methods
 ****************************************************************************/
static asynStatus writeUInt32(void* ppvt,asynUser* pasynUser,epicsUInt32 value,epicsUInt32 mask)
{
    int addr;
    asynStatus status;
    Port* pport=(Port*)ppvt;
    Command* pcommand=(Command*)pasynUser->drvUser;

    if( pport->init==0 ) return( asynError );
    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );

    epicsMutexMustLock(pport->syncLock);
    pcommand->pport = pports[pport->index];
    status = pcommand->writeFunc(pcommand,pport->pasynUser,&value,ifaceAsynUInt32);
    epicsMutexUnlock(pport->syncLock);

    return( status );
}
static asynStatus readUInt32(void* ppvt,asynUser* pasynUser,epicsUInt32* value,epicsUInt32 mask)
{
    int addr,eom;
    asynStatus status;
    char inpBuf[BUFFER_SIZE];
    Port* pport=(Port*)ppvt;
    Command* pcommand=(Command*)pasynUser->drvUser;

    if( pport->init==0 ) return( asynError );
    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );

    epicsMutexMustLock(pport->syncLock);
    pcommand->pport = pports[pport->index];
    status = pcommand->readFunc(pcommand,pport->pasynUser,inpBuf,sizeof(inpBuf),&eom,ifaceAsynUInt32);
    epicsMutexUnlock(pport->syncLock);

    if( ASYN_ERROR(status) ) return( status );
    pcommand->readConv(pasynUser,inpBuf,sizeof(epicsUInt32),value,ifaceAsynUInt32);

    return( status );
}


/****************************************************************************
 * Define private interface asynOctet methods
 ****************************************************************************/
static asynStatus flushOctet(void *ppvt,asynUser* pasynUser)
{
    return( asynSuccess );
}
static asynStatus writeOctet(void *ppvt,asynUser *pasynUser,const char *data,size_t numchars,size_t *nbytes)
{
    int addr;
    asynStatus status;
    Port* pport=(Port*)ppvt;
    Command* pcommand=(Command*)pasynUser->drvUser;

    if( pport->init==0 ) return( asynError );
    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );

    epicsMutexMustLock(pport->syncLock);
    pcommand->pport = pports[pport->index];
    status = pcommand->writeFunc(pcommand,pport->pasynUser,(char*)data,ifaceAsynOctet);
    epicsMutexUnlock(pport->syncLock);

    if( ASYN_ERROR(status) ) *nbytes=0; else *nbytes=strlen(data);

    return( status );
}
static asynStatus readOctet(void* ppvt,asynUser* pasynUser,char* data,size_t maxchars,size_t* nbytes,int* eom)
{
    int addr;
    asynStatus status;
    char inpBuf[BUFFER_SIZE];
    Port* pport=(Port*)ppvt;
    Command* pcommand=(Command*)pasynUser->drvUser;

    if( pport->init==0 ) return( asynError );
    if( pasynManager->getAddr(pasynUser,&addr)) return( asynError );

    epicsMutexMustLock(pport->syncLock);
    pcommand->pport = pports[pport->index];
    status = pcommand->readFunc(pcommand,pport->pasynUser,inpBuf,sizeof(inpBuf),eom,ifaceAsynOctet);
    epicsMutexUnlock(pport->syncLock);

    if( ASYN_ERROR(status) ) {*nbytes=0;return( status );}
    *nbytes=pcommand->readConv(pasynUser,inpBuf,maxchars,data,ifaceAsynOctet);

    return( status );
}


/****************************************************************************
 * Define private DG645 external interface asynOctet methods
 ****************************************************************************/
static asynStatus writeOnly(Port* pport,asynUser* pasynUser,char* outBuf)
{
    asynStatus status;
    size_t nActual,nRequested=strlen(outBuf);
    ioPvt* pioPvt = (ioPvt*)pasynUser->userPvt;

    status = pasynOctetSyncIO->write(pasynUser,outBuf,nRequested,TIMEOUT,&nActual);
    if( nActual!=nRequested ) status = asynError;
    if( status!=asynSuccess )
    {
        ++pport->discos;

        pasynManager->lockPort(pasynUser);
        pioPvt->pasynCommon->disconnect(pioPvt->pcommonPvt,pasynUser);
        pioPvt->pasynCommon->connect(pioPvt->pcommonPvt,pasynUser);
        pasynManager->unlockPort(pasynUser);

        asynPrint(pasynUser,ASYN_TRACE_ERROR,"%s writeOnly: error %d wrote \"%s\"\n",pport->myport,status,outBuf);
    }

    if( status==asynSuccess ) ++pport->writeOnlys;
    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"%s writeOnly: wrote \"%s\"\n",pport->myport,outBuf);

    return( status );
}
static asynStatus writeRead(Port* pport,asynUser* pasynUser,char* outBuf,char* inpBuf,int inputSize,int* eomReason)
{
    asynStatus status;
    size_t nWrite,nRead,nWriteRequested=strlen(outBuf);
    ioPvt* pioPvt = (ioPvt*)pasynUser->userPvt;

    status = pasynOctetSyncIO->writeRead(pasynUser,outBuf,nWriteRequested,inpBuf,inputSize,TIMEOUT,&nWrite,&nRead,eomReason);
    if( nWrite!=nWriteRequested ) status = asynError;
    if( status!=asynSuccess )
    {
        ++pport->discos;

        pasynManager->lockPort(pasynUser);
        pioPvt->pasynCommon->disconnect(pioPvt->pcommonPvt,pasynUser);
        pioPvt->pasynCommon->connect(pioPvt->pcommonPvt,pasynUser);
        pasynManager->unlockPort(pasynUser);

        asynPrint(pasynUser,ASYN_TRACE_ERROR,"%s writeRead: error %d wrote \"%s\"\n",pport->myport,status,outBuf);
    }

    if( status==asynSuccess ) {inpBuf[nRead]='\0';++pport->writeReads;}
    asynPrint(pasynUser,ASYN_TRACEIO_FILTER,"%s writeRead: wrote \"%s\" read \"%s\"\n",pport->myport,outBuf,inpBuf);

    return( status );
}


/****************************************************************************
 * Register public methods
 ****************************************************************************/

/* Initialization method definitions */
static const iocshArg arg0 = {"myport",iocshArgString};
static const iocshArg arg1 = {"ioport",iocshArgString};
static const iocshArg arg2 = {"ioaddr",iocshArgInt};
static const iocshArg* args[]= {&arg0,&arg1,&arg2};
static const iocshFuncDef drvAsynDG645FuncDef = {"drvAsynDG645",3,args};
static void drvAsynDG645CallFunc(const iocshArgBuf* args)
{
    drvAsynDG645(args[0].sval,args[1].sval,args[2].ival);
}

/* Registration method */
static void drvAsynDG645Register(void)
{
    static int firstTime = 1;

    if( firstTime )
    {
        firstTime = 0;
        iocshRegister( &drvAsynDG645FuncDef,drvAsynDG645CallFunc );
    }
}
extern "C" {
epicsExportRegistrar( drvAsynDG645Register );
}
