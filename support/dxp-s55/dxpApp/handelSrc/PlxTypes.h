#ifndef __PLXTYPES_H
#define __PLXTYPES_H

/*******************************************************************************
 * Copyright (c) 2007 PLX Technology, Inc.
 * 
 * PLX Technology Inc. licenses this software under specific terms and
 * conditions.  Use of any of the software or derviatives thereof in any
 * product without a PLX Technology chip is strictly prohibited. 
 * 
 * PLX Technology, Inc. provides this software AS IS, WITHOUT ANY WARRANTY,
 * EXPRESS OR IMPLIED, INCLUDING, WITHOUT LIMITATION, ANY WARRANTY OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  PLX makes no guarantee
 * or representations regarding the use of, or the results of the use of,
 * the software and documentation in terms of correctness, accuracy,
 * reliability, currentness, or otherwise; and you rely on the software,
 * documentation and results solely at your own risk.
 * 
 * IN NO EVENT SHALL PLX BE LIABLE FOR ANY LOSS OF USE, LOSS OF BUSINESS,
 * LOSS OF PROFITS, INDIRECT, INCIDENTAL, SPECIAL OR CONSEQUENTIAL DAMAGES
 * OF ANY KIND.  IN NO EVENT SHALL PLX'S TOTAL LIABILITY EXCEED THE SUM
 * PAID TO PLX FOR THE PRODUCT LICENSED HEREUNDER.
 * 
 ******************************************************************************/

/******************************************************************************
 * 
 * File Name:
 *
 *      PlxTypes.h
 *
 * Description:
 *
 *      This file defines the basic types available to the PCI SDK.
 *
 * Revision:
 *
 *      12-01-07 : PLX SDK v5.20
 *
 ******************************************************************************/


#include "PlxDefCk.h"
#include "PlxStat.h"
#include "PciTypes.h"



#ifdef __cplusplus
extern "C" {
#endif




/******************************************
 *   Definitions for Code Portability
 ******************************************/
// Convert pointer to an integer
#define PLX_PTR_TO_INT( ptr )       ((PLX_UINT_PTR)(ptr))

// Convert integer to a pointer
#define PLX_INT_TO_PTR( int )       ((VOID*)(PLX_UINT_PTR)(int))

// For Big Endian CPUs, data shared with PLX device (SGL descr) needs to be swapped
#if defined(PLX_LITTLE_ENDIAN)
    #define PLX_LE_DATA_32(value)   (value)
#else
    #define PLX_LE_DATA_32(value)   ( ((((value) >>  0) & 0xff) << 24) | \
                                      ((((value) >>  8) & 0xff) << 16) | \
                                      ((((value) >> 16) & 0xff) <<  8) | \
                                      ((((value) >> 24) & 0xff) <<  0) )
#endif



/******************************************
 *      Miscellaneous definitions
 ******************************************/
#if !defined(VOID)
    typedef void              VOID;
#endif

#if (!defined(PLX_MSWINDOWS)) || defined(PLX_VXD_DRIVER)
    #if !defined(BOOLEAN)
        typedef S8            BOOLEAN;
    #endif
#endif

#if !defined(PLX_MSWINDOWS)
    #if !defined(BOOL)
        typedef S8            BOOL;
    #endif
#endif

#if !defined(NULL)
    #define NULL              ((VOID *) 0x0)
#endif

#if !defined(TRUE)
    #define TRUE              1
#endif

#if !defined(FALSE)
    #define FALSE             0
#endif

#if defined(PLX_MSWINDOWS) 
    #define PLX_TIMEOUT_INFINITE    INFINITE
#elif defined(PLX_LINUX) || defined(PLX_LINUX_DRIVER)
    #define PLX_TIMEOUT_INFINITE    MAX_SCHEDULE_TIMEOUT
#endif


// Constants used in performance API calculations
#define MAX_NUM_PORTS               12
#define TLP_HEADER_SIZE             22	//14
#define COMPLETION_HEADER_SIZE      20	//12
#define DLLP_SIZE                   8
#define MAX_PORTS_PER_STATION       4



/******************************************
 *   PLX-specific types & structures
 ******************************************/

// Access Size Type
typedef enum _PLX_API_METHOD
{
    PLX_API_METHOD_DRIVER,
    PLX_API_METHOD_I2C_AARDVARK
} PLX_API_METHOD;


// Access Size Type
typedef enum _PLX_ACCESS_TYPE
{
    BitSize8,
    BitSize16,
    BitSize32,
    BitSize64
} PLX_ACCESS_TYPE;


// Power State Definitions
typedef enum _PLX_POWER_LEVEL
{
    D0Uninitialized,
    D0,
    D1,
    D2,
    D3Hot,
    D3Cold
} PLX_POWER_LEVEL;


// EEPROM status
typedef enum _PLX_EEPROM_STATUS
{
    PLX_EEPROM_STATUS_NONE         = 0,     // Not present
    PLX_EEPROM_STATUS_VALID        = 1,     // Present with valid data
    PLX_EEPROM_STATUS_INVALID_DATA = 2,     // Present w/invalid data or CRC error
    PLX_EEPROM_STATUS_BLANK        = PLX_EEPROM_STATUS_INVALID_DATA,
    PLX_EEPROM_STATUS_CRC_ERROR    = PLX_EEPROM_STATUS_INVALID_DATA
} PLX_EEPROM_STATUS;


// Port types
typedef enum _PLX_LINK_SPEED
{
    PLX_LINK_SPEED_2_5_GBPS     = 1,
    PLX_LINK_SPEED_5_0_GBPS     = 2
} PLX_LINK_SPEED;


// Port types
typedef enum _PLX_PORT_TYPE
{
    PLX_PORT_UNKNOWN            = 0xFF,
    PLX_PORT_ENDPOINT           = 0,
    PLX_PORT_NON_TRANS          = PLX_PORT_ENDPOINT,  // NT port is an endpoint
    PLX_PORT_LEGACY_ENDPOINT    = 1,
    PLX_PORT_ROOT_PORT          = 4,
    PLX_PORT_UPSTREAM           = 5,
    PLX_PORT_DOWNSTREAM         = 6,
    PLX_PORT_PCIE_TO_PCI_BRIDGE = 7,
    PLX_PORT_PCI_TO_PCIE_BRIDGE = 8,
    PLX_PORT_ROOT_ENDPOINT      = 9,
    PLX_PORT_ROOT_EVENT_COLL    = 10
} PLX_PORT_TYPE;


// Non-transparent Port types (used by PLX service driver)
typedef enum _PLX_NT_PORT_TYPE
{
    PLX_NT_PORT_NONE            = 0,     // Not an NT port
    PLX_NT_PORT_VIRTUAL         = 1,     // NT Virtual-side port
    PLX_NT_PORT_LINK            = 2      // NT Link-side port
} PLX_NT_PORT_TYPE;


// DMA Command Definitions
typedef enum _PLX_DMA_COMMAND
{
    DmaPause,
    DmaResume,
    DmaAbort
} PLX_DMA_COMMAND;


// PCI Memory Structure
typedef struct _PLX_PHYSICAL_MEM
{
    U64 UserAddr;                    // User-mode virtual address
    U64 PhysicalAddr;                // Bus physical address
    U64 CpuPhysical;                 // CPU physical address
    U32 Size;                        // Size of the buffer
} PLX_PHYSICAL_MEM;


// PLX Driver Properties
typedef struct _PLX_DRIVER_PROP
{
    char    DriverName[16];          // Name of driver
    BOOLEAN bIsServiceDriver;        // Is service driver or PnP driver?
    U64     AcpiPcieEcam;            // Base address of PCIe ECAM
    U8      Reserved[40];            // Reserved for future use
} PLX_DRIVER_PROP;


// PCI BAR Properties
typedef struct _PLX_PCI_BAR_PROP
{
    U32      BarValue;               // Actual value in BAR
    U64      Physical;               // BAR Physical Address
    U64      Size;                   // Size of BAR space
    BOOLEAN  bIoSpace;               // Memory or I/O space?
    BOOLEAN  bPrefetchable;          // Is space pre-fetchable?
    BOOLEAN  b64bit;                 // Is PCI BAR 64-bit?
} PLX_PCI_BAR_PROP;


// Used for getting the port properties and status
typedef struct _PLX_PORT_PROP
{
    U8  PortType;                    // Port configuration
    U8  PortNumber;                  // Internal port number
    U8  LinkWidth;                   // Negotiated port link width
    U8  MaxLinkWidth;                // Max link width device is capable of
    U8  LinkSpeed;                   // Negotiated link speed
    U8  MaxLinkSpeed;                // Max link speed device is capable of
    U16 MaxReadReqSize;              // Max read request size allowed
    U16 MaxPayloadSize;              // Max payload size setting
} PLX_PORT_PROP;


// PCI Device Key Identifier
typedef struct _PLX_DEVICE_KEY
{
    U32 IsValidTag;                  // Magic number to determine validity
    U8  bus;                         // Physical device location
    U8  slot;
    U8  function;
    U16 VendorId;                    // Device Identifier
    U16 DeviceId;
    U16 SubVendorId;
    U16 SubDeviceId;
    U8  Revision;
    U8  ApiIndex;                    // Used internally by the API
    U8  DeviceNumber;                // Used internally by device drivers
} PLX_DEVICE_KEY;


// PLX Device Object Structure
typedef struct _PLX_DEVICE_OBJECT
{
    U32                IsValidTag;   // Magic number to determine validity
    PLX_API_METHOD     ApiMethod;    // Method API uses to access device
    PLX_DEVICE_KEY     Key;          // Device location key identifier
    PLX_DRIVER_HANDLE  hDevice;      // Handle to driver
    PLX_PCI_BAR_PROP   PciBar[6];    // PCI BAR properties
    U64                PciBarVa[6];  // For PCI BAR user-mode BAR mappings
    U8                 BarMapRef[6]; // BAR map count used by API
    U32                PlxChipType;  // PLX chip type
    U8                 PlxRevision;  // PLX chip revision
    PLX_PHYSICAL_MEM   CommonBuffer; // Used to store common buffer information
    U32                Reserved[8];  // Reserved for future use
} PLX_DEVICE_OBJECT;


// PLX Notification Object
typedef struct _PLX_NOTIFY_OBJECT
{
    U32 IsValidTag;                  // Magic number to determine validity
    U64 pWaitObject;                 // -- INTERNAL -- Wait object used by the driver
    U64 hEvent;                      // User event handle (HANDLE can be 32 or 64 bit)
} PLX_NOTIFY_OBJECT;


// PLX Interrupt Structure 
typedef struct _PLX_INTERRUPT
{
    U32 Doorbell;
    U8  PciMain               :1;
    U8  PciAbort              :1;
    U8  LocalToPci_1          :1;
    U8  LocalToPci_2          :1;
    U8  DmaChannel_0          :1;
    U8  DmaChannel_1          :1;
    U8  MuInboundPost         :1;
    U8  MuOutboundPost        :1;
    U8  MuOutboundOverflow    :1;
    U8  TargetRetryAbort      :1;
    U8  Message_0             :1;
    U8  Message_1             :1;
    U8  Message_2             :1;
    U8  Message_3             :1;
    U8  SwInterrupt           :1;
    U8  ResetDeassert         :1;
    U8  PmeDeassert           :1;
    U8  GPIO_4_5              :1;
    U8  GPIO_14_15            :1;
    U8  HotPlugAttention      :1;
    U8  HotPlugPowerFault     :1;
    U8  HotPlugMrlSensor      :1;
    U8  HotPlugChangeDetect   :1;
    U8  HotPlugCmdCompleted   :1;
    U32 Reserved              :32;      // Reserved space for future
} PLX_INTERRUPT;


// DMA Channel Properties Structure
typedef struct _PLX_DMA_PROP
{
    U8 ReadyInput           :1;
    U8 Burst                :1;
    U8 BurstInfinite        :1;
    U8 WriteInvalidMode     :1;
    U8 LocalAddrConst       :1;
    U8 DualAddressCycles    :1;
    U8 ValidMode            :1;
    U8 TransferCountClear   :1;
    U8 ValidStopControl     :1;
    U8 DemandMode           :1;
    U8 EOTPin               :1;
    U8 EOTEndLink           :1;
    U8 StopTransferMode     :1;
    U8 LocalBusWidth        :2;
    U8 WaitStates           :4;
} PLX_DMA_PROP;


// DMA Transfer Parameters
typedef struct _PLX_DMA_PARAMS
{
    union
    {
        U64 UserVa;                // User space virtual address
        U32 PciAddrLow;            // Lower 32-bits of PCI address
    } u;
    U32     PciAddrHigh;           // Upper 32-bits of PCI address
    U32     LocalAddr;             // Local bus address
    U32     ByteCount;             // Number of bytes to transfer
    U32     TerminalCountIntr :1;
    U32     LocalToPciDma     :1;
} PLX_DMA_PARAMS;


// Performance object
typedef struct _PLX_PERFORMANCE_OBJECT
{
    U32 IsValidTag;   // Magic number to determine validity

    //Port properties
    U8  PortNumber;
    U8  LinkWidth;
    U8  Generation;

    // TIC
    U32 TicIngressTlpPostedHeader;
    U32 TicIngressTlpPostedDW;
    U32 TicIngressTlpNonpostedDW;
    U32 TicIngressTlpCompletionHeader;
    U32 TicIngressTlpCompletionDW;

    // TEC 
    U32 TecEgressTlpPostedHeader;
    U32 TecEgressTlpPostedDW;
    U32 TecEgressTlpNonpostedDW;
    U32 TecEgressTlpCompletionHeader;
    U32 TecEgressTlpCompletionDW;

    // DLLP
    U32 DllpIngress;
    U32 DllpEgress;

    // PHY
    U32 PhyIngress;
    U32 PhyEgress;
} PLX_PERFORMANCE_OBJECT;


// Performance statistics
typedef struct _PLX_PERF_STATISTICS_PER_PORT
{
    // Port Input Statistics
    double InputLinkUtilization;
    double InputPayLoadSize;
    double InputPayloadByteRate;
    double InputTotalBytes;
    double InputByteRate;

    // Port Output Statistics
    double OutputLinkUtilization;
    double OutputPayLoadSize;
    double OutputPayloadByteRate;
    double OutputTotalBytes;
    double OutputByteRate;
} PLX_PERF_STATISTICS_PER_PORT;



#ifdef __cplusplus
}
#endif

#endif
