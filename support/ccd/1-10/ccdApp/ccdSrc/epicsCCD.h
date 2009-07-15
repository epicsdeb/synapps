/* This file defines constants and PVs for the SNL programs.  
 * It is put in a common file, because each SNL program (marCCD, marImagePlate, 
 * brukerCCD, roperCCD) will use the same definitions for constants and PVs.
 *
 * Mark Rivers
 * October 20, 2003
 */

/* The following must agree with the states in the ConnectState PV */
#define STATE_DISCONNECTED 0
#define STATE_CONNECTED 1

/* The following must agree with the states in the ShutterControl PV */
#define SHUTTER_NONE   0
#define SHUTTER_CAMERA 1
#define SHUTTER_EPICS  2

/* The following must agree with the states in the FrameType PV */
#define FRAME_NORMAL                 0
#define FRAME_DOUBLE_CORRELATION     1
#define FRAME_BACKGROUND             2
#define FRAME_FLATFIELD              3

/* The following must agree with the states in the CCDState PV */
#define DETECTOR_IDLE        0
#define DETECTOR_ACQUIRE     1
#define DETECTOR_READOUT     2
#define DETECTOR_CORRECT     3
#define DETECTOR_WRITING     4
#define DETECTOR_ABORTING    5
#define DETECTOR_UNAVAILABLE 6
#define DETECTOR_ERROR       7
#define DETECTOR_BUSY        8

/* PVs */
string CCDManufacturer;   assign  CCDManufacturer  to "{P}{C}CCDManufacturer";
string CCDModel;          assign  CCDModel         to "{P}{C}CCDModel";
int    BitDepth;          assign  BitDepth         to "{P}{C}BitDepth";
double MeasuredTemp;      assign  MeasuredTemp     to "{P}{C}MeasuredTemp";
double SetTemp;           assign  SetTemp          to "{P}{C}SetTemp";           monitor SetTemp;
int    ADC;               assign  ADC              to "{P}{C}ADC";
int    BinX;              assign  BinX             to "{P}{C}BinX";              monitor BinX;
int    BinY;              assign  BinY             to "{P}{C}BinY";              monitor BinY;
int    ActualBinX;        assign  ActualBinX       to "{P}{C}ActualBinX";
int    ActualBinY;        assign  ActualBinY       to "{P}{C}ActualBinY";
int    ROITop;            assign  ROITop           to "{P}{C}ROITop";            monitor ROITop;
int    ROIBottom;         assign  ROIBottom        to "{P}{C}ROIBottom";         monitor ROIBottom;
int    ROILeft;           assign  ROILeft          to "{P}{C}ROILeft";           monitor ROILeft;
int    ROIRight;          assign  ROIRight         to "{P}{C}ROIRight";          monitor ROIRight;
int    ActualROITop;      assign  ActualROITop     to "{P}{C}ActualROITop";
int    ActualROIBottom;   assign  ActualROIBottom  to "{P}{C}ActualROIBottom";
int    ActualROILeft;     assign  ActualROILeft    to "{P}{C}ActualROILeft";
int    ActualROIRight;    assign  ActualROIRight   to "{P}{C}ActualROIRight";
int    ComputeROICts;     assign  ComputeROICts    to "{P}{C}ComputeROICts";     monitor ComputeROICts;
int    NumFrames;         assign  NumFrames        to "{P}{C}NumFrames";         monitor NumFrames;
int    ActualNumFrames;   assign  ActualNumFrames  to "{P}{C}ActualNumFrames";
double ROITotal;          assign  ROITotal         to "{P}{C}ROITotal";
double ROINet;            assign  ROINet           to "{P}{C}ROINet";
double Hours;             assign  Hours            to "{P}{C}Hours";             monitor Hours;
double Minutes;           assign  Minutes          to "{P}{C}Minutes";           monitor Minutes;
double Seconds;           assign  Seconds          to "{P}{C}Seconds";           monitor Seconds;
double ActualSeconds;     assign  ActualSeconds    to "{P}{C}ActualSeconds";
double Milliseconds;      assign  Milliseconds     to "{P}{C}Milliseconds";      monitor Milliseconds;
double TimeRemaining;     assign  TimeRemaining    to "{P}{C}TimeRemaining";     monitor TimeRemaining;
int    SeqNumber;         assign  SeqNumber        to "{P}{C}SeqNumber";         monitor SeqNumber;
int    NumExposures;      assign  NumExposures     to "{P}{C}NumExposures";      monitor NumExposures;
int    AcquireCLBK;       assign  AcquireCLBK      to "{P}{C}AcquireCLBK";       monitor AcquireCLBK;
int    AcquirePOLL;       assign  AcquirePOLL      to "{P}{C}AcquirePOLL";       monitor AcquirePOLL;
int    Abort;             assign  Abort            to "{P}{C}Abort";             monitor Abort;
int    Initialize;        assign  Initialize       to "{P}{C}Initialize";        monitor Initialize;
string FilePath;          assign  FilePath         to "{P}{C}FilePath";          monitor FilePath;
string FileTemplate;      assign  FileTemplate     to "{P}{C}FileTemplate";      monitor FileTemplate;
string FilenameFormat;    assign  FilenameFormat   to "{P}{C}FilenameFormat";    monitor FilenameFormat;
string FullFilename;      assign  FullFilename     to "{P}{C}FullFilename";      monitor FullFilename;
int    SaveFile;          assign  SaveFile         to "{P}{C}SaveFile";          monitor SaveFile;
int    Compression;       assign  Compression      to "{P}{C}Compression";       monitor Compression;
string HDFTemplate;       assign  HDFTemplate      to "{P}{C}HDFTemplate";       monitor HDFTemplate;
string ServerName;        assign  ServerName       to "{P}{C}ServerName";        monitor ServerName;
int    ServerPort;        assign  ServerPort       to "{P}{C}ServerPort";        monitor ServerPort;
int    ConnectState;      assign  ConnectState     to "{P}{C}ConnectState";
int    PollDetState;      assign  PollDetState     to "{P}{C}PollDetState";      monitor PollDetState;
int    SNLWatchdog;       assign  SNLWatchdog      to "{P}{C}SNLWatchdog";       monitor SNLWatchdog;
int    DetectorState;     assign  DetectorState    to "{P}{C}DetectorState";
string Shutter;           assign  Shutter          to "";
int    OpenShutter;       assign  OpenShutter      to "{P}{C}OpenShutter";       monitor OpenShutter;
string OpenShutterStr;    assign  OpenShutterStr   to "{P}{C}OpenShutterStr";    monitor OpenShutterStr;
double OpenShutterDly;    assign  OpenShutterDly   to "{P}{C}OpenShutterDly";    monitor OpenShutterDly;
double CloseShutterDly;   assign  CloseShutterDly  to "{P}{C}CloseShutterDly";   monitor CloseShutterDly;
int    CloseShutter;      assign  CloseShutter     to "{P}{C}CloseShutter";      monitor CloseShutter;
string CloseShutterStr;   assign  CloseShutterStr  to "{P}{C}CloseShutterStr";   monitor CloseShutterStr;
string ShutterStatLink;   assign  ShutterStatLink  to "{P}{C}ShutterStatus.INP"; monitor ShutterStatLink;
int    ShutterStatProc;   assign  ShutterStatProc  to "{P}{C}ShutterStatus.PROC";
int    ShutterMode;       assign  ShutterMode      to "{P}{C}ShutterMode";       monitor ShutterMode;
int    FrameType;         assign  FrameType        to "{P}{C}FrameType";         monitor FrameType;
int    CorrectBackground; assign CorrectBackground to "{P}{C}CorrectBackground"; monitor CorrectBackground;
int    CorrectFlatfield;  assign  CorrectFlatfield to "{P}{C}CorrectFlatfield";  monitor CorrectFlatfield;
int    CorrectSpatial;    assign  CorrectSpatial   to "{P}{C}CorrectSpatial";    monitor CorrectSpatial;
int    AutoSave;          assign  AutoSave         to "{P}{C}AutoSave";          monitor AutoSave;
string DetInStr;          assign  DetInStr         to "{P}{C}DetInStr";
string DetOutStr;         assign  DetOutStr        to "{P}{C}DetOutStr";
int    DebugFlag;         assign  DebugFlag        to "{P}{C}DebugFlag";         monitor DebugFlag;
string Comment1;          assign  Comment1         to "{P}{C}Comment1";          monitor Comment1;
string Comment1Desc;      assign  Comment1Desc     to "{P}{C}Comment1.DESC";
string Comment2;          assign  Comment2         to "{P}{C}Comment2";          monitor Comment2;
string Comment2Desc;      assign  Comment2Desc     to "{P}{C}Comment2.DESC";
string Comment3;          assign  Comment3         to "{P}{C}Comment3";          monitor Comment3;
string Comment3Desc;      assign  Comment3Desc     to "{P}{C}Comment3.DESC";
string Comment4;          assign  Comment4         to "{P}{C}Comment4";          monitor Comment4;
string Comment4Desc;      assign  Comment4Desc     to "{P}{C}Comment4.DESC";
string Comment5;          assign  Comment5         to "{P}{C}Comment5";          monitor Comment5;
string Comment5Desc;      assign  Comment5Desc     to "{P}{C}Comment5.DESC";
string AsynPort;          assign  AsynPort         to "{P}{C}AsynIO.PORT";

evflag Hours_mon;            sync Hours               Hours_mon;
evflag Minutes_mon;          sync Minutes             Minutes_mon;
evflag Seconds_mon;          sync Seconds             Seconds_mon;
evflag Milliseconds_mon;     sync Milliseconds        Milliseconds_mon;
evflag BinX_mon;             sync BinX                BinX_mon;
evflag BinY_mon;             sync BinY                BinY_mon;
evflag SetTemp_mon;          sync SetTemp             SetTemp_mon;
evflag AcquireCLBK_mon;      sync AcquireCLBK         AcquireCLBK_mon;
evflag AcquirePOLL_mon;      sync AcquirePOLL         AcquirePOLL_mon;
evflag Abort_mon;            sync Abort               Abort_mon;
evflag Initialize_mon;       sync Initialize          Initialize_mon;
evflag ServerName_mon;       sync ServerName          ServerName_mon;
evflag ServerPort_mon;       sync ServerPort          ServerPort_mon;
evflag SaveFile_mon;         sync SaveFile            SaveFile_mon;
evflag ShutterStatLink_mon;  sync ShutterStatLink     ShutterStatLink_mon;
evflag OpenShutter_mon;      sync OpenShutter         OpenShutter_mon;
evflag CloseShutter_mon;     sync CloseShutter        CloseShutter_mon;
evflag PollDetState_mon;     sync PollDetState        PollDetState_mon;
evflag SNLWatchdog_mon;      sync SNLWatchdog         SNLWatchdog_mon;
evflag FilePath_mon;         sync FilePath            FilePath_mon;
evflag ROITop_mon;           sync ROITop              ROITop_mon;
evflag ROIBottom_mon;        sync ROIBottom           ROIBottom_mon;
evflag ROILeft_mon;          sync ROILeft             ROILeft_mon;
evflag ROIRight_mon;         sync ROIRight            ROIRight_mon;
evflag NumFrames_mon;        sync NumFrames           NumFrames_mon;
