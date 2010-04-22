#ifndef __PEX_API_H
#define __PEX_API_H

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
 *     PexApi.h
 *
 * Description:
 *
 *     This file contains all the PLX API function prototypes.
 *
 * Revision:
 *
 *     04-01-07 : PLX SDK v5.00
 *
 ******************************************************************************/


#include "PlxTypes.h"


#ifdef __cplusplus
extern "C" {
#endif


/******************************************
 *             Definitions
 ******************************************/
// DLL support
#ifndef EXPORT
    #if defined(PLX_MSWINDOWS)
        #define EXPORT __declspec(dllexport)
    #else
        #define EXPORT
    #endif
#endif




/******************************************
 *      PLX Device Selection Functions
 *****************************************/
PLX_STATUS EXPORT
PlxPci_DeviceOpen(
    PLX_DEVICE_KEY    *pKey,
    PLX_DEVICE_OBJECT *pDevice
    );

PLX_STATUS EXPORT
PlxPci_DeviceClose(
    PLX_DEVICE_OBJECT *pDevice
    );

PLX_STATUS EXPORT
PlxPci_DeviceFind(
    PLX_DEVICE_KEY *pKey,
    U8              DeviceNumber
    );


/******************************************
 *    Query for Information Functions
 *****************************************/
PLX_STATUS EXPORT
PlxPci_ApiVersion(
    U8 *pVersionMajor,
    U8 *pVersionMinor,
    U8 *pVersionRevision
    );

PLX_STATUS EXPORT
PlxPci_DriverVersion(
    PLX_DEVICE_OBJECT *pDevice,
    U8                *pVersionMajor,
    U8                *pVersionMinor,
    U8                *pVersionRevision
    );

PLX_STATUS EXPORT
PlxPci_DriverProperties(
    PLX_DEVICE_OBJECT *pDevice,
    PLX_DRIVER_PROP   *pDriverProp
    );

PLX_STATUS EXPORT
PlxPci_ChipTypeGet(
    PLX_DEVICE_OBJECT *pDevice,
    U32               *pChipType,
    U8                *pRevision
    );

PLX_STATUS EXPORT
PlxPci_ChipTypeSet(
    PLX_DEVICE_OBJECT *pDevice,
    U32                ChipType,
    U8                 Revision
    );

PLX_STATUS EXPORT
PlxPci_GetPortProperties(
    PLX_DEVICE_OBJECT *pDevice,
    PLX_PORT_PROP     *pPortProp
    );


/******************************************
 *        Device Control Functions
 *****************************************/
PLX_STATUS EXPORT
PlxPci_DeviceReset(
    PLX_DEVICE_OBJECT *pDevice
    );


/******************************************
 *        Register Access Functions
 *****************************************/
U32 EXPORT
PlxPci_PciRegisterRead(
    U8          bus,
    U8          slot,
    U8          function,
    U16         offset,
    PLX_STATUS *pStatus
    );

PLX_STATUS EXPORT
PlxPci_PciRegisterWrite(
    U8  bus,
    U8  slot,
    U8  function,
    U16 offset,
    U32 value
    );

U32 EXPORT
PlxPci_PciRegisterReadFast(
    PLX_DEVICE_OBJECT *pDevice,
    U16                offset,
    PLX_STATUS        *pStatus
    );

PLX_STATUS EXPORT
PlxPci_PciRegisterWriteFast(
    PLX_DEVICE_OBJECT *pDevice,
    U16                offset,
    U32                value
    );

U32 EXPORT
PlxPci_PciRegisterRead_Unsupported(
    U8          bus,
    U8          slot,
    U8          function,
    U16         offset,
    PLX_STATUS *pStatus
    );

PLX_STATUS EXPORT
PlxPci_PciRegisterWrite_Unsupported(
    U8  bus,
    U8  slot,
    U8  function,
    U16 offset,
    U32 value
    );


/******************************************
 * PLX-specific Register Access Functions
 *****************************************/
U32 EXPORT
PlxPci_PlxRegisterRead(
    PLX_DEVICE_OBJECT *pDevice,
    U16                offset,
    PLX_STATUS        *pStatus
    );

PLX_STATUS EXPORT
PlxPci_PlxRegisterWrite(
    PLX_DEVICE_OBJECT *pDevice,
    U16                offset,
    U32                value
    );

U32 EXPORT
PlxPci_PlxMappedRegisterRead(
    PLX_DEVICE_OBJECT *pDevice,
    U32                offset,
    PLX_STATUS        *pStatus
    );

PLX_STATUS EXPORT
PlxPci_PlxMappedRegisterWrite(
    PLX_DEVICE_OBJECT *pDevice,
    U32                offset,
    U32                value
    );


/******************************************
 *         PCI Mapping Functions
 *****************************************/
PLX_STATUS EXPORT
PlxPci_PciBarProperties(
    PLX_DEVICE_OBJECT *pDevice,
    U8                 BarIndex,
    PLX_PCI_BAR_PROP  *pBarProperties
    );

PLX_STATUS EXPORT
PlxPci_PciBarMap(
    PLX_DEVICE_OBJECT  *pDevice,
    U8                  BarIndex,
    VOID              **pVa
    );

PLX_STATUS EXPORT
PlxPci_PciBarUnmap(
    PLX_DEVICE_OBJECT  *pDevice,
    VOID              **pVa
    );


/******************************************
 *   BAR I/O & Memory Access Functions
 *****************************************/
PLX_STATUS EXPORT
PlxPci_IoPortRead(
    PLX_DEVICE_OBJECT *pDevice,
    U64                port,
    VOID              *pBuffer,
    U32                ByteCount,
    PLX_ACCESS_TYPE    AccessType
    );

PLX_STATUS EXPORT
PlxPci_IoPortWrite(
    PLX_DEVICE_OBJECT *pDevice,
    U64                port,
    VOID              *pBuffer,
    U32                ByteCount,
    PLX_ACCESS_TYPE    AccessType
    );

PLX_STATUS EXPORT
PlxPci_PciBarSpaceRead(
    PLX_DEVICE_OBJECT *pDevice,
    U8                 BarIndex,
    U32                offset,
    VOID              *pBuffer,
    U32                ByteCount,
    PLX_ACCESS_TYPE    AccessType,
    BOOLEAN            bOffsetAsLocalAddr
    );

PLX_STATUS EXPORT
PlxPci_PciBarSpaceWrite(
    PLX_DEVICE_OBJECT *pDevice,
    U8                 BarIndex,
    U32                offset,
    VOID              *pBuffer,
    U32                ByteCount,
    PLX_ACCESS_TYPE    AccessType,
    BOOLEAN            bOffsetAsLocalAddr
    );


/******************************************
 *       Physical Memory Functions
 *****************************************/
PLX_STATUS EXPORT
PlxPci_PhysicalMemoryAllocate(
    PLX_DEVICE_OBJECT *pDevice,
    PLX_PHYSICAL_MEM  *pMemoryInfo,
    BOOLEAN            bSmallerOk
    );

PLX_STATUS EXPORT
PlxPci_PhysicalMemoryFree(
    PLX_DEVICE_OBJECT *pDevice,
    PLX_PHYSICAL_MEM  *pMemoryInfo
    );

PLX_STATUS EXPORT
PlxPci_PhysicalMemoryMap(
    PLX_DEVICE_OBJECT *pDevice,
    PLX_PHYSICAL_MEM  *pMemoryInfo
    );

PLX_STATUS EXPORT
PlxPci_PhysicalMemoryUnmap(
    PLX_DEVICE_OBJECT *pDevice,
    PLX_PHYSICAL_MEM  *pMemoryInfo
    );

PLX_STATUS EXPORT
PlxPci_CommonBufferProperties(
    PLX_DEVICE_OBJECT *pDevice,
    PLX_PHYSICAL_MEM  *pMemoryInfo
    );

PLX_STATUS EXPORT
PlxPci_CommonBufferMap(
    PLX_DEVICE_OBJECT  *pDevice,
    VOID              **pVa
    );

PLX_STATUS EXPORT
PlxPci_CommonBufferUnmap(
    PLX_DEVICE_OBJECT  *pDevice,
    VOID              **pVa
    );


/******************************************
 *     Interrupt Support Functions
 *****************************************/
PLX_STATUS EXPORT
PlxPci_InterruptEnable(
    PLX_DEVICE_OBJECT *pDevice,
    PLX_INTERRUPT     *pPlxIntr
    );

PLX_STATUS EXPORT
PlxPci_InterruptDisable(
    PLX_DEVICE_OBJECT *pDevice,
    PLX_INTERRUPT     *pPlxIntr
    );

PLX_STATUS EXPORT
PlxPci_NotificationRegisterFor(
    PLX_DEVICE_OBJECT *pDevice,
    PLX_INTERRUPT     *pPlxIntr,
    PLX_NOTIFY_OBJECT *pEvent
    );

PLX_STATUS EXPORT
PlxPci_NotificationWait(
    PLX_DEVICE_OBJECT *pDevice,
    PLX_NOTIFY_OBJECT *pEvent,
    U64                Timeout_ms
    );

PLX_STATUS EXPORT
PlxPci_NotificationStatus(
    PLX_DEVICE_OBJECT *pDevice,
    PLX_NOTIFY_OBJECT *pEvent,
    PLX_INTERRUPT     *pPlxIntr
    );

PLX_STATUS EXPORT
PlxPci_NotificationCancel(
    PLX_DEVICE_OBJECT *pDevice,
    PLX_NOTIFY_OBJECT *pEvent
    );


/******************************************
 *     Serial EEPROM Access Functions
 *****************************************/
PLX_EEPROM_STATUS EXPORT
PlxPci_EepromPresent(
    PLX_DEVICE_OBJECT *pDevice,
    PLX_STATUS        *pStatus
    );

BOOLEAN EXPORT
PlxPci_EepromProbe(
    PLX_DEVICE_OBJECT *pDevice,
    PLX_STATUS        *pStatus
    );

PLX_STATUS EXPORT
PlxPci_EepromSetAddressWidth(
    PLX_DEVICE_OBJECT *pDevice,
    U8                 width
    );

PLX_STATUS EXPORT
PlxPci_EepromCrcUpdate(
    PLX_DEVICE_OBJECT *pDevice,
    U32               *pCrc,
    BOOLEAN            bUpdateEeprom
    );

PLX_STATUS EXPORT
PlxPci_EepromCrcGet(
    PLX_DEVICE_OBJECT *pDevice,
    U32               *pCrc,
    U8                *pCrcStatus
    );

PLX_STATUS EXPORT
PlxPci_EepromReadByOffset(
    PLX_DEVICE_OBJECT *pDevice,
    U16                offset,
    U32               *pValue
    );

PLX_STATUS EXPORT
PlxPci_EepromWriteByOffset(
    PLX_DEVICE_OBJECT *pDevice,
    U16                offset,
    U32                value
    );

PLX_STATUS EXPORT
PlxPci_EepromReadByOffset_16(
    PLX_DEVICE_OBJECT *pDevice,
    U16                offset,
    U16               *pValue
    );

PLX_STATUS EXPORT
PlxPci_EepromWriteByOffset_16(
    PLX_DEVICE_OBJECT *pDevice,
    U16                offset,
    U16                value
    );


/******************************************
 *          PLX VPD Functions
 *****************************************/
U32 EXPORT 
PlxPci_VpdRead(
    PLX_DEVICE_OBJECT *pDevice,
    U16                offset,
    PLX_STATUS        *pStatus
    );

PLX_STATUS EXPORT
PlxPci_VpdWrite(
    PLX_DEVICE_OBJECT *pDevice,
    U16                offset,
    U32                value
    );


/******************************************
 *            DMA Functions
 *****************************************/
PLX_STATUS EXPORT
PlxPci_DmaControl(
    PLX_DEVICE_OBJECT *pDevice,
    U8                 channel,
    PLX_DMA_COMMAND    command
    );

PLX_STATUS EXPORT
PlxPci_DmaStatus(
    PLX_DEVICE_OBJECT *pDevice,
    U8                 channel
    );

PLX_STATUS EXPORT
PlxPci_DmaChannelOpen(
    PLX_DEVICE_OBJECT *pDevice,
    U8                 channel,
    PLX_DMA_PROP      *pDmaProp
    );

PLX_STATUS EXPORT
PlxPci_DmaTransferBlock(
    PLX_DEVICE_OBJECT *pDevice,
    U8                 channel,
    PLX_DMA_PARAMS    *pDmaParams,
    U64                Timeout_ms
    );

PLX_STATUS EXPORT
PlxPci_DmaTransferUserBuffer(
    PLX_DEVICE_OBJECT *pDevice,
    U8                 channel,
    PLX_DMA_PARAMS    *pDmaParams,
    U64                Timeout_ms
    );

PLX_STATUS EXPORT
PlxPci_DmaChannelClose(
    PLX_DEVICE_OBJECT *pDevice,
    U8                 channel
    );


/******************************************
 *   Performance Monitoring Functions
 *****************************************/
PLX_STATUS EXPORT
PlxPci_PerformanceOpen(
    PLX_DEVICE_OBJECT      *pDevice,
    PLX_PERFORMANCE_OBJECT *pPerformanceObject
    );

PLX_STATUS EXPORT
PlxPci_PerformanceClose(
    PLX_DEVICE_OBJECT      *pDevice,
    PLX_PERFORMANCE_OBJECT *pPerformanceObject
    );

PLX_STATUS EXPORT
PlxPci_PerformanceStartAndGetCounterValues(
    PLX_DEVICE_OBJECT *pDevice,
    U32               *PerformanceCountersForAllPorts,
    U32                NoOfMilliSeconds
    );

PLX_STATUS EXPORT
PlxPci_PerformanceGetStatistics(
    PLX_PERFORMANCE_OBJECT       *pPerformanceObject,
    PLX_PERF_STATISTICS_PER_PORT *pPerformanceStatistics,
    U32                           NoOfMilliSeconds
    );



#ifdef __cplusplus
}
#endif

#endif
