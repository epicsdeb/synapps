/*

                          Argonne National Laboratory
                     Beamline Controls and Data Acquisition

            Stanford Research Systems DG645 Digital Delay Generator
                               Asyn Port Driver

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

 =============================================================================
 Authors: David M. Kline (DMK)
          Eric Norum (EN)
          Dohn Arms (DAA)
 -----------------------------------------------------------------------------
 History:
 2009-Jan-31  DMK  Derived support from drvAsynColby driver.
 2009-Feb-11  DMK  Initial development version V1.0 complete.
 2009-Feb-11  DMK  Development version V1.1 complete.
 2009-Mar-11  DMK  Removed calls to free() during initialization to eliminate
                   a segmentation fault (on soft IOCs) when a DG645 is not
                   connected. This was successful on an EBRICK. 
 2009-Mar-30  DMK  Allow multiple instances of instrument.
 2010-Sep-30  DMK  Solidified multiple instance implementation.
 2011-Nov-21  EN   Added support for prescalers.
 2012-Sep-17  DAA  Rewrote it somewhat to fix race condition if multiple
                   devices are running at same time.  Simplified the driver, 
                   and changed from using address as record type to tag.
 2013-Jun-28  DAA  Add description field. Added support for prescale phases,
                   trigger holdoff, advanced triggering switch.
 -----------------------------------------------------------------------------

*/


/* EPICS base version-specific definitions (must be performed first) */
#include <epicsVersion.h>

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
#include <errlog.h>

#include <epicsThread.h>

/* EPICS synApps/Asyn related include files */
#include <asynDriver.h>
#include <asynDrvUser.h>
#include <asynInt32.h>
#include <asynFloat64.h>
#include <asynOctet.h>
#include <asynOctetSyncIO.h>
#include <asynStandardInterfaces.h>

/* epicsExport.h must come last */
#include <epicsExport.h>


#define LT_EPICSBASE(v,r,l) (EPICS_VERSION<(v)||(EPICS_VERSION==(v)&&(EPICS_REVISION<(r)||(EPICS_REVISION==(r)&&EPICS_MODIFICATION<(l)))))
#define LT_ASYN(v,r,l) (ASYN_VERSION<(v)||(ASYN_VERSION==(v)&&(ASYN_REVISION<(r)||(ASYN_REVISION==(r)&&ASYN_MODIFICATION<(l)))))

/* Evaluate EPICS base */
#if LT_EPICSBASE(3,14,7)
    #error "EPICS base must be 3.14.7 or greater"
#endif
/* Evaluate asyn */
#if LT_ASYN(4,10,0)
    #error "asyn must be 4-10 or greater"
#endif



/* Define symbolic constants */
#define TIMEOUT         (3.0)
#define BUFFER_SIZE     (100)

/* Forward struct declarations */
typedef enum {Octet,Float64,Int32} ifaceType;

static char *driver = "drvAsynDG645";      /* String for asynPrint */


/* Declare port driver structure */
struct Port
{
  char* myport;
  char* ioport;
  int   ioaddr;

  int   discos;
  int   writeReads;
  int   writeOnlys;
  char  ident[BUFFER_SIZE];

  int init;
  int error;

  /* Asyn info */
  asynUser* pasynUser;
  asynUser *pasynUserTrace;  /* asynUser for asynTrace on this port */
  asynStandardInterfaces asynStdInterfaces;
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
  char* readCommand;
  asynStatus (*readFunc)(int which,Port *pport,char* inpBuf,int inputSize,int* eomReason,ifaceType asynIface);
  int (*readConv)(int which,Port *pport,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface);

  char* writeCommand;
  asynStatus (*writeFunc)(int which, Port *pport,void* data,ifaceType asynIface);

  char *tag;
  //  char* desc; // not used right now, so they are commented out
};


/* Public interface forward references */
int drvAsynDG645(const char* myport,const char* ioport,int ioaddr);


/* Forward references for asynCommon methods */
static void report(void* ppvt,FILE* fp,int details);
static asynStatus connect(void* ppvt,asynUser* pasynUser);
static asynStatus disconnect(void* ppvt,asynUser* pasynUser);
static asynCommon ifaceCommon = {report,connect,disconnect};


/* Forward references for asynDrvUser methods */
static asynStatus create(void* ppvt,asynUser* pasynUser,const char* drvInfo, const char** pptypeName,size_t* psize);
static asynStatus destroy(void* ppvt,asynUser* pasynUser);
static asynStatus gettype(void* ppvt,asynUser* pasynUser,const char** pptypeName,size_t* psize);
static asynDrvUser ifaceDrvUser = {create,gettype,destroy};

/* Forward references for asynFloat64 methods */
static asynStatus readFloat64(void* ppvt,asynUser* pasynUser,epicsFloat64* value);
static asynStatus writeFloat64(void* ppvt,asynUser* pasynUser,epicsFloat64 value);
static asynFloat64 ifaceFloat64 = {writeFloat64, readFloat64};

/* Forward references for asynInt32 methods */
static asynStatus readInt32(void* ppvt,asynUser* pasynUser,epicsInt32* value);
static asynStatus writeInt32(void* ppvt,asynUser* pasynUser,epicsInt32 value);
static asynInt32 ifaceInt32 =  {writeInt32, readInt32};

/* Forward references for asynOctet methods */
static asynStatus flushOctet(void* ppvt,asynUser* pasynUser);
static asynStatus writeOctet(void* ppvt,asynUser* pasynUser,const char *data,size_t numchars,size_t* nbytes);
static asynStatus readOctet(void* ppvt,asynUser* pasynUser,char* data,size_t maxchars,size_t *nbytes,int *eom);
static asynOctet ifaceOctet = { writeOctet, readOctet, flushOctet};

/* Forward references for external asynOctet interface */
static asynStatus writeOnly(Port* pport,char* outBuf);
static asynStatus writeRead(Port* pport,char* outBuf,char* inpBuf,int inputSize,int *eomReason);


/* Forward references for command table methods */
static asynStatus readSink(int which, Port *pport,char* inpBuf,int inputSize,int *eomReason,ifaceType asynIface);
static asynStatus readParam(int which, Port *pport,char* inpBuf,int inputSize,int *eomReason,ifaceType asynIface);

static int cvtSink(int which, Port *pport,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface);
static int cvtCopyText(int which, Port *pport,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface);
static int cvtErrorText(int which, Port *pport,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface);
static int cvtErrorCode(int which, Port *pport,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface);
static int cvtIdent(int which, Port *pport,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface);
static int cvtChanRef(int which, Port *pport,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface);
static int cvtChanDelay(int which, Port *pport,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface);

static int cvtStrInt(int which, Port *pport,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface);
static int cvtStrFloat(int which, Port *pport,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface);

static asynStatus writeSink(int which, Port *pport,void* data,ifaceType asynIface);
static asynStatus writeIntParam(int which, Port *pport,void* data,ifaceType asynIface);
static asynStatus writeStrParam(int which, Port *pport,void* data,ifaceType asynIface);
static asynStatus writeFloatParam(int which, Port *pport,void* data,ifaceType asynIface);
static asynStatus writeCommandOnly(int which, Port *pport,void* data,ifaceType asynIface);
static asynStatus writeChannelRef(int which, Port *pport,void* data,ifaceType asynIface);
static asynStatus writeChannelDelay(int which, Port *pport,void* data,ifaceType asynIface);


/* Define local variants */
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
//---------------------------------------------------------------------------------------------------------------------------------------------
// readCommand readFunc        readConv        writeCommand    writeFunc           Tag             Desc
//---------------------------------------------------------------------------------------------------------------------------------------------

  // Failure-mode -- has to be first
  {"",         readSink,       cvtSink ,       "",             writeSink,          "NONE",            }, // "Failure Record"},

  // Instrument management related commands
  {"*IDN?",    readParam,      cvtIdent,       "",             writeSink,          "IDENT",           }, // "Ident"},
  {"LERR?",    readParam,      cvtErrorText,   "",             writeSink,          "STATUS",          }, // "Status Text"},
  {"",         readSink,       cvtErrorCode,   "",             writeSink,          "STATUS_CODE",     }, // "Status Code"},
  {"",         readSink,       cvtSink,        "*CLS",         writeCommandOnly,   "STATUS_CLEAR",    }, // "Status Clear"},
  {"",         readSink,       cvtSink,        "*RST",         writeCommandOnly,   "RESET",           }, // "Reset Instrument"},
  {"",         readSink,       cvtSink,        "LCAL",         writeCommandOnly,   "LOCAL",           }, // "Goto Local"},
  {"",         readSink,       cvtSink,        "REMT",         writeCommandOnly,   "REMOTE",          }, // "Goto Remote"},
  {"",         readSink,       cvtSink,        "*RCL%d",       writeIntParam,      "RECALL",          }, // "Recall Settings"},
  {"",         readSink,       cvtSink,        "*SAV%d",       writeIntParam,      "SAVE",            }, // "Save Settings"},
  
  // Trigger related commands
  {"TLVL?",    readParam,      cvtStrFloat,    "TLVL%-.2f",    writeFloatParam,    "TRIG_LEVEL",      }, // "Trigger Level"},
  {"TRAT?",    readParam,      cvtStrFloat,    "TRAT%-.6f",    writeFloatParam,    "TRIG_RATE",       }, // "Trigger Rate"},
  {"TSRC?",    readParam,      cvtStrInt,      "TSRC%d",       writeIntParam,      "TRIG_SOURCE",     }, // "Trigger Source"},
  {"INHB?",    readParam,      cvtStrInt,      "INHB%d",       writeIntParam,      "TRIG_INHIBIT",    }, // "Trigger Inhibit"},
  {"",         readSink,       cvtSink,        "*TRG",         writeCommandOnly,   "TRIG_DELAY",      }, // "Trigger Delay"},

  // Advanced trigger related commands
  {"ADVT?",    readParam,      cvtStrInt,      "ADVT%d",       writeIntParam,      "TRIG_ADV_MODE",   }, // "Advanced Trigger Mode"},
  {"HOLD?",    readParam,      cvtStrFloat,    "HOLD%e",       writeFloatParam,    "TRIG_HOLDOFF",    }, // "Trigger Holdoff"},
  {"PRES?0",   readParam,      cvtStrInt,      "PRES 0,%d",    writeIntParam,      "TRIG_PRESCALE",   }, // "Trigger Prescale"},
  {"PRES?1",   readParam,      cvtStrInt,      "PRES 1,%d",    writeIntParam,      "TRIG_AB_PRESCALE",}, // "Channel AB Trigger Prescale"},
  {"PRES?2",   readParam,      cvtStrInt,      "PRES 2,%d",    writeIntParam,      "TRIG_CD_PRESCALE",}, // "Channel CD Trigger Prescale"},
  {"PRES?3",   readParam,      cvtStrInt,      "PRES 3,%d",    writeIntParam,      "TRIG_EF_PRESCALE",}, // "Channel EF Trigger Prescale"},
  {"PRES?4",   readParam,      cvtStrInt,      "PRES 4,%d",    writeIntParam,      "TRIG_GH_PRESCALE",}, // "Channel GH Trigger Prescale"},
  {"PHAS?1",   readParam,      cvtStrInt,      "PHAS 1,%d",    writeIntParam,      "TRIG_AB_PHASE",   }, // "Channel AB Trigger Phase"},
  {"PHAS?2",   readParam,      cvtStrInt,      "PHAS 2,%d",    writeIntParam,      "TRIG_CD_PHASE",   }, // "Channel CD Trigger Phase"},
  {"PHAS?3",   readParam,      cvtStrInt,      "PHAS 3,%d",    writeIntParam,      "TRIG_EF_PHASE",   }, // "Channel EF Trigger Phase"},
  {"PHAS?4",   readParam,      cvtStrInt,      "PHAS 4,%d",    writeIntParam,      "TRIG_GH_PHASE",   }, // "Channel GH Trigger Phase"},

   // Burst mode related commands
  {"BURM?",    readParam,      cvtStrInt,      "BURM%d",       writeIntParam,      "BURST_MODE",      }, // "Burst Mode"},
  {"BURC?",    readParam,      cvtStrInt,      "BURC%d",       writeIntParam,      "BURST_COUNT",     }, // "Burst Count"},
  {"BURT?",    readParam,      cvtStrInt,      "BURT%d",       writeIntParam,      "BURST_T0",        }, // "Burst T0 Config"},
  {"BURD?",    readParam,      cvtStrFloat,    "BURD%e",       writeFloatParam,    "BURST_DELAY",     }, // "Burst Delay"},
  {"BURP?",    readParam,      cvtStrFloat,    "BURP%e",       writeFloatParam,    "BURST_PERIOD",    }, // "Burst Period"},
                                                                
  // Interface configuration related commands
  {"IFCF?0",   readParam,      cvtStrInt,      "IFCF 0,%d",    writeIntParam,      "RS232",           }, // "Serial enable/disable"},
  {"IFCF?1",   readParam,      cvtStrInt,      "IFCF 1,%d",    writeIntParam,      "RS232_BAUD",      }, // "Serial baud rate"},
  {"IFCF?2",   readParam,      cvtStrInt,      "IFCF 2,%d",    writeIntParam,      "GPIB",            }, // "GPIB enable/disable"},
  {"IFCF?3",   readParam,      cvtStrInt,      "IFCF 3,%d",    writeIntParam,      "GPIB_ADDRESS",    }, // "GPIB address"},
  {"IFCF?4",   readParam,      cvtStrInt,      "IFCF 4,%d",    writeIntParam,      "TCPIP",           }, // "LAN TCP/IP enable/disable"},
  {"IFCF?5",   readParam,      cvtStrInt,      "IFCF 5,%d",    writeIntParam,      "DHCP",            }, // "DHCP enable/disable"},
  {"IFCF?6",   readParam,      cvtStrInt,      "IFCF 6,%d",    writeIntParam,      "AUTO_IP",         }, // "Auto-IP enable/disable"},
  {"IFCF?7",   readParam,      cvtStrInt,      "IFCF 7,%d",    writeIntParam,      "STATIC_IP",       }, // "Static-IP enable/disable"},
  {"IFCF?8",   readParam,      cvtStrInt,      "IFCF 8,%d",    writeIntParam,      "BARE_SOCKET",     }, // "Bare socket enable/disable"},
  {"IFCF?9",   readParam,      cvtStrInt,      "IFCF 9,%d",    writeIntParam,      "TELNET",          }, // "Telnet enable/disable"},
  {"IFCF?10",  readParam,      cvtStrInt,      "IFCF 10,%d",   writeIntParam,      "VXI11",           }, // "VXI-11 enable/disable"},
  {"IFCF?11",  readParam,      cvtCopyText,    "IFCF 11,%s",   writeStrParam,      "IP_ADDRESS",      }, // "Static IP address"},
  {"IFCF?12",  readParam,      cvtCopyText,    "IFCF 12,%s",   writeStrParam,      "NET_MASK",        }, // "Network mask"},
  {"IFCF?13",  readParam,      cvtCopyText,    "IFCF 13,%s",   writeStrParam,      "GATEWAY",         }, // "Gateway IP address"},
  {"",         readSink,       cvtSink,        "IFRS 0",       writeCommandOnly,   "RS232_RESET",     }, // "Serial reset"},
  {"",         readSink,       cvtSink,        "IFRS 1",       writeCommandOnly,   "GPIB_RESET",      }, // "GPIB reset"},
  {"",         readSink,       cvtSink,        "IFRS 2",       writeCommandOnly,   "TCPIP_RESET",     }, // "LAN reset"},
  {"EMAC?",    readParam,      cvtCopyText,    "",             writeSink,          "MAC_ADDRESS",     }, // "MAC Address"},

  // Delay channel A related commands
  {"DLAY?2",   readParam,      cvtChanRef,     "DLAY 2,%d,%e", writeChannelRef,    "A_REF",           }, // "Channel A ref"},
  {"DLAY?2",   readParam,      cvtChanDelay,   "DLAY 2,%d,%e", writeChannelDelay,  "A_DELAY",         }, // "Channel A delay"},
  {"",         readSink,       cvtSink,        "SPDL 2,0",     writeCommandOnly,   "A_DELAY_STEP_NEG",}, // "Channel A step delay minus"},
  {"",         readSink,       cvtSink,        "SPDL 2,1",     writeCommandOnly,   "A_DELAY_STEP_POS",}, // "Channel A step delay plus"},
  {"SSDL?2",   readParam,      cvtStrFloat,    "SSDL 2,%e",    writeFloatParam,    "A_DELAY_STEP",    }, // "Channel A step delay size"},

  // Delay channel B related commands
  {"DLAY?3",   readParam,      cvtChanRef,     "DLAY 3,%d,%e", writeChannelRef,    "B_REF",           }, // "Channel B ref"},
  {"DLAY?3",   readParam,      cvtChanDelay,   "DLAY 3,%d,%e", writeChannelDelay,  "B_DELAY",         }, // "Channel B delay"},
  {"",         readSink,       cvtSink,        "SPDL 3,0",     writeCommandOnly,   "B_DELAY_STEP_NEG",}, // "Channel B step delay minus"},
  {"",         readSink,       cvtSink,        "SPDL 3,1",     writeCommandOnly,   "B_DELAY_STEP_POS",}, // "Channel B step delay plus"},
  {"SSDL?3",   readParam,      cvtStrFloat,    "SSDL 3,%e",    writeFloatParam,    "B_DELAY_STEP",    }, // "Channel B step delay size"},
                                                                
  // Delay channel C related commands
  {"DLAY?4",   readParam,      cvtChanRef,     "DLAY 4,%d,%e", writeChannelRef,    "C_REF",           }, // "Channel C ref"},
  {"DLAY?4",   readParam,      cvtChanDelay,   "DLAY 4,%d,%e", writeChannelDelay,  "C_DELAY",         }, // "Channel C delay"},
  {"",         readSink,       cvtSink,        "SPDL 4,0",     writeCommandOnly,   "C_DELAY_STEP_NEG",}, // "Channel C step delay minus"},
  {"",         readSink,       cvtSink,        "SPDL 4,1",     writeCommandOnly,   "C_DELAY_STEP_POS",}, // "Channel C step delay plus"},
  {"SSDL?4",   readParam,      cvtStrFloat,    "SSDL 4,%e",    writeFloatParam,    "C_DELAY_STEP",    }, // "Channel C step delay size"},

  // Delay channel D related commands
  {"DLAY?5",   readParam,      cvtChanRef,     "DLAY 5,%d,%e", writeChannelRef,    "D_REF",           }, // "Channel D ref"},
  {"DLAY?5",   readParam,      cvtChanDelay,   "DLAY 5,%d,%e", writeChannelDelay,  "D_DELAY",         }, // "Channel D delay"},
  {"",         readSink,       cvtSink,        "SPDL 5,0",     writeCommandOnly,   "D_DELAY_STEP_NEG",}, // "Channel D step delay minus"},
  {"",         readSink,       cvtSink,        "SPDL 5,1",     writeCommandOnly,   "D_DELAY_STEP_POS",}, // "Channel D step delay plus"},
  {"SSDL?5",   readParam,      cvtStrFloat,    "SSDL 5,%e",    writeFloatParam,    "D_DELAY_STEP",    }, // "Channel D step delay size"},
                                                                
  // Delay channel E related commands
  {"DLAY?6",   readParam,      cvtChanRef,     "DLAY 6,%d,%e", writeChannelRef,    "E_REF",           }, // "Channel E ref"},
  {"DLAY?6",   readParam,      cvtChanDelay,   "DLAY 6,%d,%e", writeChannelDelay,  "E_DELAY",         }, // "Channel E delay"},
  {"",         readSink,       cvtSink,        "SPDL 6,0",     writeCommandOnly,   "E_DELAY_STEP_NEG",}, // "Channel E step delay minus"},
  {"",         readSink,       cvtSink,        "SPDL 6,1",     writeCommandOnly,   "E_DELAY_STEP_POS",}, // "Channel E step delay plus"},
  {"SSDL?6",   readParam,      cvtStrFloat,    "SSDL 6,%e",    writeFloatParam,    "E_DELAY_STEP",    }, // "Channel E step delay size"},

  // Delay channel F related commands
  {"DLAY?7",   readParam,      cvtChanRef,     "DLAY 7,%d,%e", writeChannelRef,    "F_REF",           }, // "Channel F ref"},
  {"DLAY?7",   readParam,      cvtChanDelay,   "DLAY 7,%d,%e", writeChannelDelay,  "F_DELAY",         }, // "Channel F delay"},
  {"",         readSink,       cvtSink,        "SPDL 7,0",     writeCommandOnly,   "F_DELAY_STEP_NEG",}, // "Channel F step delay minus"},
  {"",         readSink,       cvtSink,        "SPDL 7,1",     writeCommandOnly,   "F_DELAY_STEP_POS",}, // "Channel F step delay plus"},
  {"SSDL?7",   readParam,      cvtStrFloat,    "SSDL 7,%e",    writeFloatParam,    "F_DELAY_STEP",    }, // "Channel F step delay size"},
  
  // Delay channel G related commands
  {"DLAY?8",   readParam,      cvtChanRef,     "DLAY 8,%d,%e", writeChannelRef,    "G_REF",           }, // "Channel G ref"},
  {"DLAY?8",   readParam,      cvtChanDelay,   "DLAY 8,%d,%e", writeChannelDelay,  "G_DELAY",         }, // "Channel G delay"},
  {"",         readSink,       cvtSink,        "SPDL 8,0",     writeCommandOnly,   "G_DELAY_STEP_NEG",}, // "Channel G step delay minus"},
  {"",         readSink,       cvtSink,        "SPDL 8,1",     writeCommandOnly,   "G_DELAY_STEP_POS",}, // "Channel G step delay plus"},
  {"SSDL?8",   readParam,      cvtStrFloat,    "SSDL 8,%e",    writeFloatParam,    "G_DELAY_STEP",    }, // "Channel G step delay size"},

  // Delay channel H related commands
  {"DLAY?9",   readParam,      cvtChanRef,     "DLAY 9,%d,%e", writeChannelRef,    "H_REF",           }, // "Channel H ref"},
  {"DLAY?9",   readParam,      cvtChanDelay,   "DLAY 9,%d,%e", writeChannelDelay,  "H_DELAY",         }, // "Channel H delay"},
  {"",         readSink,       cvtSink,        "SPDL 9,0",     writeCommandOnly,   "H_DELAY_STEP_NEG",}, // "Channel H step delay minus"},
  {"",         readSink,       cvtSink,        "SPDL 9,1",     writeCommandOnly,   "H_DELAY_STEP_POS",}, // "Channel H step delay plus"},
  {"SSDL?9",   readParam,      cvtStrFloat,    "SSDL 9,%e",    writeFloatParam,    "H_DELAY_STEP",    }, // "Channel H step delay size"},
                                                                
  // T0 output commands
  {"LAMP?0",   readParam,      cvtStrFloat,    "LAMP 0,%-.2f", writeFloatParam,    "T0_AMP",            }, // "T0 output amplitude"},
  {"LOFF?0",   readParam,      cvtStrFloat,    "LOFF 0,%-.2f", writeFloatParam,    "T0_OFFSET",         }, // "T0 output offset"},
  {"LPOL?0",   readParam,      cvtStrInt,      "LPOL 0,%d",    writeIntParam,      "T0_POLARITY",       }, // "T0 output polarity"},
  {"",         readSink,       cvtSink,        "SPLA 0,0",     writeCommandOnly,   "T0_AMP_STEP_NEG",   }, // "T0 step amplitude minus"},
  {"",         readSink,       cvtSink,        "SPLA 0,1",     writeCommandOnly,   "T0_AMP_STEP_POS",   }, // "T0 step amplitude plus"},
  {"SSLA?0",   readParam,      cvtStrFloat,    "SSLA 0,%-.2f", writeFloatParam,    "T0_AMP_STEP",       }, // "T0 step amplitude size"},
  {"",         readSink,       cvtSink,        "SPLO 0,0",     writeCommandOnly,   "T0_OFFSET_STEP_NEG",}, // "T0 step offset minus"},
  {"",         readSink,       cvtSink,        "SPLO 0,1",     writeCommandOnly,   "T0_OFFSET_STEP_POS",}, // "T0 step offset plus"},
  {"SSLO?0",   readParam,      cvtStrFloat,    "SSLO 0,%-.2f", writeFloatParam,    "T0_OFSET_STEP",     }, // "T0 step offset size"},
  
  // AB output commands
  {"LAMP?1",   readParam,      cvtStrFloat,    "LAMP 1,%-.2f", writeFloatParam,    "AB_AMP",            }, // "AB output amplitude"},
  {"LOFF?1",   readParam,      cvtStrFloat,    "LOFF 1,%-.2f", writeFloatParam,    "AB_OFFSET",         }, // "AB output offset"},
  {"LPOL?1",   readParam,      cvtStrInt,      "LPOL 1,%d",    writeIntParam,      "AB_POLARITY",       }, // "AB output polarity"},
  {"",         readSink,       cvtSink,        "SPLA 1,0",     writeCommandOnly,   "AB_AMP_STEP_NEG",   }, // "AB step amplitude minus"},
  {"",         readSink,       cvtSink,        "SPLA 1,1",     writeCommandOnly,   "AB_AMP_STEP_POS",   }, // "AB step amplitude plus"},
  {"SSLA?1",   readParam,      cvtStrFloat,    "SSLA 1,%-.2f", writeFloatParam,    "AB_AMP_STEP",       }, // "AB step amplitude size"},
  {"",         readSink,       cvtSink,        "SPLO 1,0",     writeCommandOnly,   "AB_OFFSET_STEP_NEG",}, // "AB step offset minus"},
  {"",         readSink,       cvtSink,        "SPLO 1,1",     writeCommandOnly,   "AB_OFFSET_STEP_POS",}, // "AB step offset plus"},
  {"SSLO?1",   readParam,      cvtStrFloat,    "SSLO 1,%-.2f", writeFloatParam,    "AB_OFFSET_STEP",    }, // "AB step offset size"},

  // CD output commands
  {"LAMP?2",   readParam,      cvtStrFloat,    "LAMP 2,%-.2f", writeFloatParam,    "CD_AMP",            }, // "CD output amplitude"},
  {"LOFF?2",   readParam,      cvtStrFloat,    "LOFF 2,%-.2f", writeFloatParam,    "CD_OFFSET",         }, // "CD output offset"},
  {"LPOL?2",   readParam,      cvtStrInt,      "LPOL 2,%d",    writeIntParam,      "CD_POLARITY",       }, // "CD output polarity"},
  {"",         readSink,       cvtSink,        "SPLA 2,0",     writeCommandOnly,   "CD_AMP_STEP_NEG",   }, // "CD step amplitude minus"},
  {"",         readSink,       cvtSink,        "SPLA 2,1",     writeCommandOnly,   "CD_AMP_STEP_POS",   }, // "CD step amplitude plus"},
  {"SSLA?2",   readParam,      cvtStrFloat,    "SSLA 2,%-.2f", writeFloatParam,    "CD_AMP_STEP",       }, // "CD step amplitude size"},
  {"",         readSink,       cvtSink,        "SPLO 2,0",     writeCommandOnly,   "CD_OFFSET_STEP_NEG",}, // "CD step offset minus"},
  {"",         readSink,       cvtSink,        "SPLO 2,1",     writeCommandOnly,   "CD_OFFSET_STEP_POS",}, // "CD step offset plus"},
  {"SSLO?2",   readParam,      cvtStrFloat,    "SSLO 2,%-.2f", writeFloatParam,    "CD_OFFSET_STEP",    }, // "CD step offset size"},

  // EF output commands
  {"LAMP?3",   readParam,      cvtStrFloat,    "LAMP 3,%-.2f", writeFloatParam,    "EF_AMP",            }, // "EF output amplitude"},
  {"LOFF?3",   readParam,      cvtStrFloat,    "LOFF 3,%-.2f", writeFloatParam,    "EF_OFFSET",         }, // "EF output offset"},
  {"LPOL?3",   readParam,      cvtStrInt,      "LPOL 3,%d",    writeIntParam,      "EF_POLARITY",       }, // "EF output polarity"},
  {"",         readSink,       cvtSink,        "SPLA 3,0",     writeCommandOnly,   "EF_AMP_STEP_NEG",   }, // "EF step amplitude minus"},
  {"",         readSink,       cvtSink,        "SPLA 3,1",     writeCommandOnly,   "EF_AMP_STEP_POS",   }, // "EF step amplitude plus"},
  {"SSLA?3",   readParam,      cvtStrFloat,    "SSLA 3,%-.2f", writeFloatParam,    "EF_AMP_STEP",       }, // "EF step amplitude size"},
  {"",         readSink,       cvtSink,        "SPLO 3,0",     writeCommandOnly,   "EF_OFFSET_STEP_NEG",}, // "EF step offset minus"},
  {"",         readSink,       cvtSink,        "SPLO 3,1",     writeCommandOnly,   "EF_OFFSET_STEP_POS",}, // "EF step offset plus"},
  {"SSLO?3",   readParam,      cvtStrFloat,    "SSLO 3,%-.2f", writeFloatParam,    "EF_OFFSET_STEP",    }, // "EF step offset size"},

  // GH output commands
  {"LAMP?4",   readParam,      cvtStrFloat,    "LAMP 4,%-.2f", writeFloatParam,    "GH_AMP",            }, // "GH output amplitude"},
  {"LOFF?4",   readParam,      cvtStrFloat,    "LOFF 4,%-.2f", writeFloatParam,    "GH_OFFSET",         }, // "GH output offset"},
  {"LPOL?4",   readParam,      cvtStrInt,      "LPOL 4,%d",    writeIntParam,      "GH_POLARITY",       }, // "GH output polarity"},
  {"",         readSink,       cvtSink,        "SPLA 4,0",     writeCommandOnly,   "GH_AMP_STEP_POS",   }, // "GH step amplitude minus"},
  {"",         readSink,       cvtSink,        "SPLA 4,1",     writeCommandOnly,   "GH_AMP_STEP_NEG",   }, // "GH step amplitude plus"},
  {"SSLA?4",   readParam,      cvtStrFloat,    "SSLA 4,%-.2f", writeFloatParam,    "GH_AMP_STEP",       }, // "GH step amplitude size"},
  {"",         readSink,       cvtSink,        "SPLO 4,0",     writeCommandOnly,   "GH_OFFSET_STEP_POS",}, // "GH step offset minus"},
  {"",         readSink,       cvtSink,        "SPLO 4,1",     writeCommandOnly,   "GH_OFFSET_STEP_NEG",}, // "GH step offset plus"},
  {"SSLO?4",   readParam,      cvtStrFloat,    "SSLO 4,%-.2f", writeFloatParam,    "GH_OFFSET_STEP",    }, // "GH step offset size"},
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
  int status = asynSuccess;
  Port* pport;
  int eomReason;
  char inpBuf[BUFFER_SIZE];

  asynStandardInterfaces *pInterfaces;

  pport = (Port*)callocMustSucceed(1,sizeof(Port),"drvAsynDG645");
  pport->myport = epicsStrDup(myport);
  pport->ioport = epicsStrDup(ioport);
  pport->ioaddr = ioaddr;

  status = pasynOctetSyncIO->connect(ioport,ioaddr,&pport->pasynUser,NULL);
  if (status != asynSuccess)
    {
      errlogPrintf("%s::drvAsynDG645 port %s can't connect "
                   "to asynCommon on Octet server %s address %d.\n",
                   driver, myport, ioport, ioaddr);
      return asynError;
    }

  // pasynManager->waitConnect(pport->pasynUser, 1.0);
    
  /* Create asynUser for asynTrace */
  pport->pasynUserTrace = pasynManager->createAsynUser(0, 0);
  pport->pasynUserTrace->userPvt = pport;

  status = pasynManager->registerPort(myport,ASYN_CANBLOCK,1,0,0);
  if( status != asynSuccess) 
    {
      errlogPrintf("%s::drvAsynDG645 port %s can't register port\n",
                   driver, myport);
      return asynError;
    }

  pInterfaces = &pport->asynStdInterfaces;
    
  /* Initialize interface pointers */
  pInterfaces->common.pinterface    = (void *)&ifaceCommon;
  pInterfaces->drvUser.pinterface   = (void *)&ifaceDrvUser;
  pInterfaces->octet.pinterface     = (void *)&ifaceOctet;
  pInterfaces->int32.pinterface     = (void *)&ifaceInt32;
  pInterfaces->float64.pinterface   = (void *)&ifaceFloat64;

  status = pasynStandardInterfacesBase->initialize(myport, pInterfaces,
                                                   pport->pasynUserTrace, 
                                                   pport);
  if (status != asynSuccess) 
    {
      errlogPrintf("%s::drvAsynDG645 port %s"
                   " can't register standard interfaces: %s\n",
                   driver, myport, pport->pasynUserTrace->errorMessage);
      return asynError;
    }

#ifdef vxWorks
  /* Send a sacrificial clear status to vxworks device (i.e. VME)*/
  /* This fixes a problem with *IDN? call when starting from a cold boot */
  /* with the SBS IP-Octal hardware. */
  if( writeOnly(pport,"") )
    {
      errlogPrintf("%s::drvAsynDG645 port %s failed to write\n",
                   driver, myport);
      return asynError;
    }
#endif

  /* Identification query */
  if( writeRead(pport,"*IDN?",inpBuf,sizeof(inpBuf),&eomReason) )
    {
      errlogPrintf("%s::drvAsynDG645 port %s failed to acquire identification\n",
                   driver, myport);
      return asynError;
    }
  strcpy(pport->ident,inpBuf);

  /* Clear status */
  if( writeOnly(pport,"*CLS") )
    {
      errlogPrintf("%s::drvAsynDG645 port %s failed to clear status\n",
                   driver, myport);
      return asynError;
    }

  /* Complete initialization */
  pport->init=1;
  return asynSuccess;
}


/****************************************************************************
 * Define private convert methods
 ****************************************************************************/
static int cvtSink(int which, Port *pport,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface)
{
  return 0;
}
static int cvtCopyText(int which, Port *pport,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface)
{
  strcpy((char*)outBuf,inpBuf);
  return MIN((int)strlen((char*)outBuf),maxchars);
}
static int cvtErrorText(int which, Port *pport,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface)
{
  int code;
  char *m = NULL;

  code=atoi(inpBuf);
  for(int i=0; i<statusLen; i++) 
    if( statusMsg[i].code == code ) 
      {
        m = statusMsg[i].msg;
        break;
      }

  if( m ) 
    {
      strcpy((char*)outBuf,m);
      pport->error = code;
    } 
  else 
    {
      strcpy((char*)outBuf,"*ERR*");
      pport->error = 0;
    }
  return MIN((int)strlen((char*)outBuf),maxchars);
}
static int cvtErrorCode(int which, Port *pport,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface)
{
  *(epicsInt32*)outBuf = pport->error;
  return 0;
}
static int cvtStrInt(int which, Port *pport,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface)
{
  *(epicsInt32*)outBuf = atoi(inpBuf);
  return 0;
}
static int cvtStrFloat(int which, Port *pport,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface)
{
  *(epicsFloat64*)outBuf = atof(inpBuf);
  return 0;
}
static int cvtIdent(int which, Port *pport,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface)
{
  // this routine just chops the Ident string down to under 40 characters
  char *p;

  p = strchr( inpBuf, ',');
  if( p == NULL) // just in case
    strcpy((char*)outBuf,inpBuf);
  else
    {
      p++;
      sprintf((char *)outBuf, "SRS %s", p);
    }
  return MIN((int)strlen((char*)outBuf),maxchars);
}
static int cvtChanRef(int which, Port *pport,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface)
{
  int chan;
  epicsFloat64 delay;

  sscanf(inpBuf,"%d,%lf",&chan,&delay);
  *(epicsInt32*)outBuf = chan;

  return 0;
}
static int cvtChanDelay(int which, Port *pport,char* inpBuf,int maxchars,void* outBuf,ifaceType asynIface)
{
  char* delayText[] = {"T0", "T1", "A", "B", "C", "D", "E", "F", "G", "H"};

  int chan,siz;
  epicsFloat64 delay;

  sscanf(inpBuf,"%d,%lf",&chan,&delay);
  switch( asynIface )
    {
    case Octet:
      sprintf((char*)outBuf,"%s + %-.12f",delayText[chan],delay);
      siz = MIN((int)strlen((char*)outBuf),maxchars);
      break;
    case Float64:
      *(epicsFloat64*)outBuf = delay;
      siz = sizeof(epicsFloat64);
      break;
    default:
      siz = 0;
    }

  return siz;
}


/****************************************************************************
 * Define private read parameter methods
 ****************************************************************************/
static asynStatus readSink(int which, Port *pport,char* inpBuf,int inputSize,int *eomReason,ifaceType asynIface)
{
  return asynSuccess;
}
static asynStatus readParam(int which, Port *pport,char* inpBuf,int inputSize,int* eomReason,ifaceType asynIface)
{
  return writeRead(pport,commandTable[which].readCommand,inpBuf,inputSize,eomReason);
}


/****************************************************************************
 * Define private write parameter methods
 ****************************************************************************/
static asynStatus writeSink(int which, Port *pport, void* data,ifaceType asynIface)
{
  return asynSuccess;
}
static asynStatus writeIntParam(int which, Port *pport,void* data,ifaceType asynIface)
{
  char outBuf[BUFFER_SIZE];

  sprintf(outBuf,commandTable[which].writeCommand,*(epicsInt32*)data);
  return writeOnly(pport,outBuf);
}
static asynStatus writeStrParam(int which, Port *pport,void* data,ifaceType asynIface)
{
  char outBuf[BUFFER_SIZE];

  sprintf(outBuf,commandTable[which].writeCommand,(char*)data);
  return writeOnly(pport,outBuf);
}
static asynStatus writeFloatParam(int which, Port *pport,void* data,ifaceType asynIface)
{
  char outBuf[BUFFER_SIZE];

  sprintf(outBuf,commandTable[which].writeCommand,*(epicsFloat64*)data);
  return writeOnly(pport,outBuf);
}
static asynStatus writeCommandOnly(int which, Port *pport,void* data,ifaceType asynIface)
{
  return writeOnly(pport,commandTable[which].writeCommand);
}
static asynStatus writeChannelRef(int which, Port *pport,void* data,ifaceType asynIface)
{
  asynStatus status;
  int chan,eomReason;
  epicsFloat64 delay;
  char datBuf[BUFFER_SIZE];

  status = writeRead(pport,commandTable[which].readCommand,datBuf,sizeof(datBuf),&eomReason);
  if( ASYN_ERROR(status) ) 
    return status;

  sscanf(datBuf,"%d,%lf",&chan,&delay);
  sprintf((char*)datBuf,commandTable[which].writeCommand,*(int*)data,delay);

  return writeOnly(pport,datBuf);
}
static asynStatus writeChannelDelay(int which, Port *pport,void* data,ifaceType asynIface)
{
  asynStatus status;
  int chan,eomReason;
  epicsFloat64 delay;
  char datBuf[BUFFER_SIZE];

  status = writeRead(pport,commandTable[which].readCommand,datBuf,sizeof(datBuf),&eomReason);
  if( ASYN_ERROR(status) ) 
    return status;

  sscanf(datBuf,"%d,%lf",&chan,&delay);
  sprintf((char*)datBuf,commandTable[which].writeCommand,chan,*(epicsFloat64*)data);

  return writeOnly(pport,datBuf);
}


/****************************************************************************
 * Define private interface asynCommon methods
 ****************************************************************************/
static void report(void* ppvt,FILE* fp,int details)
{
  Port* pport = (Port*)ppvt;

  fprintf(fp,"    %s\n",pport->ident);
  fprintf(fp,"    discos %d writeReads %d writeOnlys %d\n",
          pport->discos,pport->writeReads,pport->writeOnlys);
  fprintf(fp,"    support %s initialized\n",(pport->init)?"IS":"IS NOT");
  fprintf(fp,"    myport \"%s\" ioport \"%s\"\n",pport->myport,pport->ioport);
}

static asynStatus connect(void* ppvt,asynUser* pasynUser)
{
  int addr;

  if( pasynManager->getAddr(pasynUser,&addr)) 
    return asynError;

  pasynManager->exceptionConnect(pasynUser);
  return asynSuccess;
}
static asynStatus disconnect(void* ppvt,asynUser* pasynUser)
{
  int addr;

  if( pasynManager->getAddr(pasynUser,&addr)) 
    return asynError;

  pasynManager->exceptionDisconnect(pasynUser);
  return asynSuccess;
}


/****************************************************************************
 * Define private interface asynDrvUser methods
 ****************************************************************************/
static asynStatus create(void* ppvt,asynUser* pasynUser,const char* drvInfo, const char** pptypeName,size_t* psize)
{
  Port* pport = (Port*)ppvt;
  int i;
  
  for(i = 0; i < commandLen; i++) 
    if( !epicsStrCaseCmp( drvInfo, commandTable[i].tag) ) 
      {
        pasynUser->reason = i;
        break;
      }
  if( i == commandLen ) 
    {
      errlogPrintf("%s::create port %s failed to find tag %s\n",
                   driver, pport->myport, drvInfo);
      pasynUser->reason = 0;
      return asynError;
    }

  return asynSuccess;
}

static asynStatus gettype(void* ppvt,asynUser* pasynUser,const char** pptypeName,size_t* psize)
{
  if( pptypeName ) 
    *pptypeName = NULL;
  if( psize ) 
    *psize = 0;

  return asynSuccess;
}
static asynStatus destroy(void* ppvt,asynUser* pasynUser)
{
  return asynSuccess;
}


/****************************************************************************
 * Define private interface asynFloat64 methods
 ****************************************************************************/
static asynStatus writeFloat64(void* ppvt,asynUser* pasynUser,epicsFloat64 value)
{
  asynStatus status;
  Port* pport=(Port*)ppvt;
  int which = pasynUser->reason;

  if( pport->init==0 ) 
    return asynError;

  status = commandTable[which].writeFunc(which, pport, &value, Float64);

  return status;
}
static asynStatus readFloat64(void* ppvt,asynUser* pasynUser,epicsFloat64* value)
{
  int eom;
  asynStatus status;
  char inpBuf[BUFFER_SIZE];
  Port* pport=(Port*)ppvt;
  int which = pasynUser->reason;

  if( pport->init==0 ) 
    return asynError;

  status = commandTable[which].readFunc(which, pport, inpBuf,sizeof(inpBuf), &eom,Float64);

  if( ASYN_ERROR(status) )
    return status;
  commandTable[which].readConv(which, pport, inpBuf,sizeof(epicsFloat64),value,Float64);
  return status;
}


/****************************************************************************
 * Define private interface asynInt32 methods
 ****************************************************************************/
static asynStatus writeInt32(void* ppvt,asynUser* pasynUser,epicsInt32 value)
{
  asynStatus status;
  Port* pport=(Port*)ppvt;
  int which = pasynUser->reason;

  if( pport->init==0 ) 
    return asynError;

  status = commandTable[which].writeFunc( which,pport,&value, Int32);

  return status;
}
static asynStatus readInt32(void* ppvt,asynUser* pasynUser,epicsInt32* value)
{
  int eom;
  asynStatus status;
  char inpBuf[BUFFER_SIZE];
  Port* pport=(Port*)ppvt;
  int which = pasynUser->reason;

  if( pport->init==0 ) 
    return asynError;

  status = commandTable[which].readFunc(which, pport, inpBuf,sizeof(inpBuf), &eom,Int32);

  if( ASYN_ERROR(status) ) 
    return status;
  commandTable[which].readConv(which,pport, inpBuf,sizeof(epicsInt32),value, Int32);

  return status;
}


/****************************************************************************
 * Define private interface asynOctet methods
 ****************************************************************************/
static asynStatus flushOctet(void *ppvt,asynUser* pasynUser)
{
  return asynSuccess;
}
static asynStatus writeOctet(void *ppvt,asynUser *pasynUser,const char *data,size_t numchars,size_t *nbytes)
{
  asynStatus status;
  Port* pport=(Port*)ppvt;
  int which = pasynUser->reason;

  if( pport->init==0 ) 
    return asynError;

  status = commandTable[which].writeFunc(which,pport, (char*)data,Octet);

  if( ASYN_ERROR(status) ) 
    *nbytes=0; 
  else 
    *nbytes=strlen(data);

  return status;
}
static asynStatus readOctet(void* ppvt,asynUser* pasynUser,char* data,size_t maxchars,size_t* nbytes,int* eom)
{
  asynStatus status;
  char inpBuf[BUFFER_SIZE];
  Port* pport=(Port*)ppvt;
  int which = pasynUser->reason;

  if( pport->init==0 ) 
    return asynError;

  status = commandTable[which].readFunc(which,pport,inpBuf,sizeof(inpBuf), eom,Octet);

  if( ASYN_ERROR(status) ) 
    *nbytes=0;
  else
    *nbytes=commandTable[which].readConv(which,pport, inpBuf,maxchars,data, Octet);

  return status;
}


/****************************************************************************
 * Define private DG645 external interface asynOctet methods
 ****************************************************************************/
static asynStatus writeOnly(Port* pport,char* outBuf)
{
  asynStatus status;
  size_t nActual,nRequested=strlen(outBuf);

  status = pasynOctetSyncIO->write(pport->pasynUser,outBuf,nRequested,TIMEOUT,&nActual);
  if( nActual!=nRequested ) 
    status = asynError;
  if( status!=asynSuccess )
    {
      ++pport->discos;
      asynPrint(pport->pasynUserTrace,ASYN_TRACE_ERROR,"%s writeOnly: error %d wrote \"%s\"\n",pport->myport,status,outBuf);
    }
  
  if( status==asynSuccess ) 
    ++pport->writeOnlys;
  asynPrint(pport->pasynUserTrace,ASYN_TRACEIO_FILTER,"%s writeOnly: wrote \"%s\"\n",pport->myport,outBuf);

  return status;
}
static asynStatus writeRead(Port* pport,char* outBuf,char* inpBuf,int inputSize,int* eomReason)
{
  asynStatus status;
  size_t nWrite,nRead,nWriteRequested=strlen(outBuf);

  status = pasynOctetSyncIO->writeRead(pport->pasynUser,outBuf,nWriteRequested,inpBuf,inputSize,TIMEOUT,&nWrite,&nRead,eomReason);
  if( nWrite!=nWriteRequested ) 
    status = asynError;

  if( status!=asynSuccess )
    {
      ++pport->discos;
      asynPrint(pport->pasynUserTrace,ASYN_TRACE_ERROR,"%s writeRead: error %d wrote \"%s\"\n",pport->myport,status,outBuf);
    }
  else
    {
      inpBuf[nRead]='\0';
      ++pport->writeReads;
    }

  asynPrint(pport->pasynUserTrace,ASYN_TRACEIO_FILTER,"%s writeRead: wrote \"%s\" read \"%s\"\n",pport->myport,outBuf,inpBuf);

  return status;
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
extern "C" 
{
  epicsExportRegistrar( drvAsynDG645Register );
}
