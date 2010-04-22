/****************************************************************************************/
/* 20020sys.h																			*/
/*																						*/
/* Include file for Contemporary Controls Null Stack NT driver.							*/
/*																						*/
/* Version: 1.2																			*/
/* Author:  Bennet Levine																*/
/* History: 03/26/99 Version 1.0 release												*/
/*			07/07/99 Version 1.1 Added DIOC_COM20020CANCELTX							*/
/*			07/07/99 Version 1.2 Added DIOC_WAKE_ON_RECON and DIOC_RESET_WAKE_ON_RECON	*/
/****************************************************************************************/

//DEFINES
//timeouts
#define BOOL UCHAR
#define STANDARD_TIMEOUT	0x18
#define QUAD_TIMEOUT 		0x10
#define EIGHT_TIMEOUT	    0x08
#define SIXTEEN_TIMEOUT		0x00

//function returned values
#define TRANSMITTER_OK			    0
#define TRANSMITTER_NOT_AVAILABLE	1
#define ILLEGAL_PACKET_SIZE 		2

//structures
typedef struct tagCOM20020_CONFIG{
unsigned int uiCom20020BaseIOAddress;
UCHAR  byCom20020InterruptLevel;
UCHAR  byCom20020Timeout;
UCHAR  byCom20020NodeID;
BOOL bCom20020_128NAKs;  //128 NAKs (TRUE), 4NAKs(false)
BOOL bCom20020ReceiveAll;
UCHAR byCom20020ClockPrescaler;
BOOL bCom20020SlowArbitration;
BOOL bCom20020ReceiveBroadcasts;
} COM20020_CONFIG;

typedef struct tagCOM20020_TRANSMIT_BUFFER{
ULONG byDestinationNodeID;
ULONG uiNumberOfBytes;
//ULONG byDataBuffer[508];
UCHAR byDataBuffer[508];
} COM20020_TRANSMIT_BUFFER;

typedef struct tagCOM20020_RECEIVE_BUFFER{
ULONG bySourceNodeID;
ULONG byDestinationNodeID;
ULONG uiNumberOfBytes;
//ULONG byDataBuffer[508];
UCHAR byDataBuffer[508];
ULONG byNumberOfFilledBuffers;
} COM20020_RECEIVE_BUFFER;


typedef struct tagCOM20020_STATUS{
ULONG bReceiveActivity;
ULONG bPowerOnReset;
ULONG bRecon;
ULONG bToken;
ULONG byReceivedMessages;
ULONG bTransmissionComplete;
ULONG bTransmissionAcknowledged;
ULONG bExcessiveNAKs;
ULONG dwReserved;
} COM20020_STATUS;

typedef struct tagCOM20020_REGISTER{
BOOL bWrite;
UCHAR byRegister;
UCHAR byValue;
} COM20020_REGISTER;  

//define the IOCTL codes
// Device type
#define CCSI_TYPE 40000

#define DIOC_COM20020INIT			CTL_CODE(CCSI_TYPE,  0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DIOC_COM20020TRANSMIT 		CTL_CODE(CCSI_TYPE,  0x901, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DIOC_COM20020RECEIVE		CTL_CODE(CCSI_TYPE,  0x902, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DIOC_COM20020STATUS			CTL_CODE(CCSI_TYPE,  0x903, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DIOC_COM20020REGISTER		CTL_CODE(CCSI_TYPE,  0x904, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DIOC_COM20020EXIT			CTL_CODE(CCSI_TYPE,  0x905, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DIOC_WAKE_ON_RECEIVE		CTL_CODE(CCSI_TYPE,  0x906, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DIOC_RESET_WAKE_ON_RECEIVE	CTL_CODE(CCSI_TYPE,  0x907, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DIOC_WAKE_ON_TX_COMPLETE	CTL_CODE(CCSI_TYPE,  0x908, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DIOC_RESET_WAKE_ON_TX_COMPLETE	CTL_CODE(CCSI_TYPE,  0x909, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DIOC_COM20020CANCELTX		CTL_CODE(CCSI_TYPE,  0x90A, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DIOC_WAKE_ON_RECON			CTL_CODE(CCSI_TYPE,  0x90B, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DIOC_RESET_WAKE_ON_RECON	CTL_CODE(CCSI_TYPE,  0x90C, METHOD_BUFFERED, FILE_ANY_ACCESS)
