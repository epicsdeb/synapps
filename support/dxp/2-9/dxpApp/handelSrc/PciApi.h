#ifndef __PCIAPI_H
#define __PCIAPI_H

/*******************************************************************************
 * Copyright (c) 2005 PLX Technology, Inc.
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
 *     PciApi.h
 *
 * Description:
 *
 *     This file contains all the PCI API function prototypes.
 *
 * Revision:
 *
 *     06-01-05 : PCI SDK v4.40
 *
 ******************************************************************************/


#include "PlxTypes.h"


#ifdef __cplusplus
extern "C" {
#endif


/* DLL support */
#ifndef EXPORT
    #if defined(_WIN32)
        #define EXPORT __declspec(dllexport)
    #else
        #define EXPORT
    #endif
#endif



/******************************************
 * Miscellaneous Functions
 *****************************************/
RETURN_CODE EXPORT
PlxSdkVersion(
    U8 *VersionMajor,
    U8 *VersionMinor,
    U8 *VersionRevision
    );

RETURN_CODE EXPORT
PlxDriverVersion(
    HANDLE  hDevice,
    U8     *VersionMajor,
    U8     *VersionMinor,
    U8     *VersionRevision
    );

RETURN_CODE EXPORT
PlxChipTypeGet(
    HANDLE  hDevice,
    U32    *pChipType,
    U8     *pRevision
    );

VOID EXPORT
PlxPciBoardReset(
    HANDLE hDevice
    );

RETURN_CODE EXPORT
PlxPciCommonBufferGet(
    HANDLE      hDevice,
    PCI_MEMORY *pMemoryInfo
    );

RETURN_CODE EXPORT
PlxPciCommonBufferProperties(
    HANDLE      hDevice,
    PCI_MEMORY *pMemoryInfo
    );

RETURN_CODE EXPORT
PlxPciCommonBufferMap(
    HANDLE  hDevice,
    VOID   *pVa
    );

RETURN_CODE EXPORT
PlxPciCommonBufferUnmap(
    HANDLE  hDevice,
    VOID   *pVa
    );

RETURN_CODE EXPORT
PlxPciPhysicalMemoryAllocate(
    HANDLE      hDevice,
    PCI_MEMORY *pMemoryInfo,
    BOOLEAN     bSmallerOk
    );

RETURN_CODE EXPORT
PlxPciPhysicalMemoryFree(
    HANDLE      hDevice,
    PCI_MEMORY *pMemoryInfo
    );

RETURN_CODE EXPORT
PlxPciPhysicalMemoryMap(
    HANDLE      hDevice,
    PCI_MEMORY *pMemoryInfo
    );

RETURN_CODE EXPORT
PlxPciPhysicalMemoryUnmap(
    HANDLE      hDevice,
    PCI_MEMORY *pMemoryInfo
    );


/******************************************
 * PLX Device Management Functions
 *****************************************/
RETURN_CODE EXPORT
PlxPciDeviceOpen(
    DEVICE_LOCATION *pDevice,
    HANDLE          *pHandle
    );

RETURN_CODE EXPORT
PlxPciDeviceClose(
    HANDLE hDevice
    );

RETURN_CODE EXPORT
PlxPciDeviceFind(
    DEVICE_LOCATION *pDevice,
    U32             *pRequestLimit
    );

RETURN_CODE EXPORT
PlxPciBarGet(
    HANDLE   hDevice,
    U8       BarIndex,
    U32     *pPciBar,
    BOOLEAN *pFlag_IsIoSpace
    );

RETURN_CODE EXPORT
PlxPciBarRangeGet(
    HANDLE  hDevice,
    U8      BarIndex,
    U32    *pData
    );

RETURN_CODE EXPORT
PlxPciBarMap(
    HANDLE  hDevice,
    U8      BarIndex,
    VOID   *pVa
    );

RETURN_CODE EXPORT
PlxPciBarUnmap(
    HANDLE  hDevice,
    VOID   *pVa
    );


/******************************************
 * Register Access Functions
 *****************************************/
U32 EXPORT
PlxPciConfigRegisterRead(
    U8           bus,
    U8           slot,
    U16          offset,
    RETURN_CODE *pReturnCode
    );

RETURN_CODE EXPORT
PlxPciConfigRegisterWrite(
    U8   bus,
    U8   slot,
    U16  offset,
    U32 *pValue
    );

U32 EXPORT
PlxPciRegisterRead_Unsupported(
    U8           bus,
    U8           slot,
    U16          offset,
    RETURN_CODE *pReturnCode
    );

RETURN_CODE EXPORT
PlxPciRegisterWrite_Unsupported(
    U8  bus,
    U8  slot,
    U16 offset,
    U32 value
    );

U32 EXPORT
PlxRegisterRead(
    HANDLE       hDevice,
    U16          offset,
    RETURN_CODE *pReturnCode
    );

RETURN_CODE EXPORT
PlxRegisterWrite(
    HANDLE hDevice,
    U16    offset,
    U32    value
    );

U32 EXPORT
PlxRegisterMailboxRead(
    HANDLE       hDevice,
    MAILBOX_ID   MailboxId,
    RETURN_CODE *pReturnCode
    );

RETURN_CODE EXPORT
PlxRegisterMailboxWrite(
    HANDLE     hDevice,
    MAILBOX_ID MailboxId,
    U32        value
    );

U32 EXPORT 
PlxRegisterDoorbellRead(
    HANDLE       hDevice,
    RETURN_CODE *pReturnCode
    );

RETURN_CODE EXPORT
PlxRegisterDoorbellSet(
    HANDLE hDevice,
    U32    value
    );


/******************************************
 * Interrupt Support Functions
 *****************************************/
RETURN_CODE EXPORT
PlxNotificationRegisterFor(
    HANDLE             hDevice,
    PLX_INTR          *pPlxIntr,
    PLX_NOTIFY_OBJECT *pEvent
    );

RETURN_CODE EXPORT
PlxNotificationWait(
    HANDLE             hDevice,
    PLX_NOTIFY_OBJECT *pEvent,
    U32                Timeout_ms
    );

RETURN_CODE EXPORT
PlxNotificationStatus(
    HANDLE             hDevice,
    PLX_NOTIFY_OBJECT *pEvent,
    PLX_INTR          *pPlxIntr
    );

RETURN_CODE EXPORT
PlxNotificationCancel(
    HANDLE             hDevice,
    PLX_NOTIFY_OBJECT *pEvent
    );

RETURN_CODE EXPORT
PlxIntrEnable(
    HANDLE    hDevice,
    PLX_INTR *pPlxIntr
    );

RETURN_CODE EXPORT
PlxIntrDisable(
    HANDLE    hDevice,
    PLX_INTR *pPlxIntr
    );

RETURN_CODE EXPORT
PlxIntrStatusGet(
    HANDLE    hDevice,
    PLX_INTR *pPlxIntr
    );


/******************************************
 * Bus Memory and I/O Functions
 *****************************************/
RETURN_CODE EXPORT
PlxBusIopRead(
    HANDLE       hDevice,
    IOP_SPACE    IopSpace,
    U32          address,
    BOOLEAN      bRemap,
    VOID        *pBuffer,
    U32          ByteCount,
    ACCESS_TYPE  AccessType
    );

RETURN_CODE EXPORT
PlxBusIopWrite(
    HANDLE       hDevice,
    IOP_SPACE    IopSpace,
    U32          address,
    BOOLEAN      bRemap,
    VOID        *pBuffer,
    U32          ByteCount,
    ACCESS_TYPE  AccessType
    );

RETURN_CODE EXPORT 
PlxIoPortRead(
    HANDLE       hDevice,
    U32          IoPort,
    ACCESS_TYPE  AccessType,
    VOID        *pValue
    );

RETURN_CODE EXPORT
PlxIoPortWrite(
    HANDLE       hDevice,
    U32          IoPort,
    ACCESS_TYPE  AccessType,
    VOID        *pValue
    );


/******************************************
 * Serial EEPROM Access Functions
 *****************************************/
BOOLEAN EXPORT 
PlxSerialEepromPresent(
    HANDLE       hDevice,
    RETURN_CODE *pReturnCode
    );

RETURN_CODE EXPORT
PlxSerialEepromRead(
    HANDLE       hDevice,
    EEPROM_TYPE  EepromType,
    U32         *buffer,
    U32          size
    );

RETURN_CODE EXPORT
PlxSerialEepromReadByOffset(
    HANDLE  hDevice,
    U16     offset,
    U32    *pValue
    );

RETURN_CODE EXPORT
PlxSerialEepromWrite(
    HANDLE       hDevice,
    EEPROM_TYPE  EepromType,
    U32         *buffer,
    U32          size
    );

RETURN_CODE EXPORT
PlxSerialEepromWriteByOffset(
    HANDLE hDevice,
    U16    offset,
    U32    value
    );


/******************************************
 * DMA Functions
 *****************************************/
RETURN_CODE EXPORT
PlxDmaControl(
    HANDLE      hDevice,
    DMA_CHANNEL channel, 
    DMA_COMMAND command
    );

RETURN_CODE EXPORT
PlxDmaStatus(
    HANDLE      hDevice,
    DMA_CHANNEL channel
    );


/******************************************
 * Block DMA Functions
 *****************************************/
RETURN_CODE EXPORT
PlxDmaBlockChannelOpen(
    HANDLE            hDevice,
    DMA_CHANNEL       channel, 
    DMA_CHANNEL_DESC *pDesc
    );

RETURN_CODE EXPORT
PlxDmaBlockTransfer(
    HANDLE                hDevice,
    DMA_CHANNEL           channel,
    DMA_TRANSFER_ELEMENT *dmaData,
    BOOLEAN               returnImmediate
    );

RETURN_CODE EXPORT 
PlxDmaBlockChannelClose(
    HANDLE      hDevice,
    DMA_CHANNEL channel
    );


/******************************************
 * SGL DMA Functions
 *****************************************/
RETURN_CODE EXPORT
PlxDmaSglChannelOpen(
    HANDLE            hDevice,
    DMA_CHANNEL       channel, 
    DMA_CHANNEL_DESC *pDesc
    );

RETURN_CODE EXPORT
PlxDmaSglTransfer(
    HANDLE                hDevice,
    DMA_CHANNEL           channel,
    DMA_TRANSFER_ELEMENT *dmaData,
    BOOLEAN               returnImmediate
    );

RETURN_CODE EXPORT 
PlxDmaSglChannelClose(
    HANDLE      hDevice,
    DMA_CHANNEL channel
    );


/******************************************
 * Messaging Unit Functions
 *****************************************/
RETURN_CODE EXPORT
PlxMuInboundPortRead(
    HANDLE  hDevice,
    U32    *pFrame
    );

RETURN_CODE EXPORT
PlxMuInboundPortWrite(
    HANDLE  hDevice,
    U32    *pFrame
    );

RETURN_CODE EXPORT
PlxMuOutboundPortRead(
    HANDLE  hDevice,
    U32    *pFrame
    );

RETURN_CODE EXPORT
PlxMuOutboundPortWrite(
    HANDLE  hDevice,
    U32    *pFrame
    );


/******************************************
 * Power Management Functions
 *****************************************/
PLX_POWER_LEVEL EXPORT
PlxPowerLevelGet(
    HANDLE       hDevice,
    RETURN_CODE *pReturnCode
    );

RETURN_CODE EXPORT
PlxPowerLevelSet(
    HANDLE          hDevice,
    PLX_POWER_LEVEL plxPowerLevel
    );

U8 EXPORT 
PlxPmNcpRead(
    HANDLE       hDevice,
    RETURN_CODE *pReturnCode
    );


/******************************************
 * Hot Swap Functions
 *****************************************/
U8 EXPORT
PlxHotSwapNcpRead(
    HANDLE       hDevice,
    RETURN_CODE *pReturnCode
    );

U8 EXPORT 
PlxHotSwapStatus(
    HANDLE       hDevice,
    RETURN_CODE *pReturnCode
    );


/******************************************
 * VPD Functions
 *****************************************/
U8 EXPORT
PlxVpdNcpRead(
    HANDLE       hDevice,
    RETURN_CODE *pReturnCode
    );

U32 EXPORT 
PlxVpdRead(
    HANDLE       hDevice,
    U16          offset,
    RETURN_CODE *pReturnCode
    );

RETURN_CODE EXPORT
PlxVpdWrite(
    HANDLE hDevice,
    U16    offset,
    U32    value
    );




#ifdef __cplusplus
}
#endif

#endif
