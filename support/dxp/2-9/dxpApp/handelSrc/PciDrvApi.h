#ifndef __PCIDRVAPI_H
#define __PCIDRVAPI_H

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
 *     PciDrvApi.h
 *
 * Description:
 *
 *     This file contains all the PLX PCI Driver API function prototypes.
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


/******************************************
 *             Definitions
 ******************************************/
/* DLL support */
#ifndef EXPORT
    #if defined(_WIN32)
        #define EXPORT __declspec(dllexport)
    #else
        #define EXPORT
    #endif
#endif




/******************************************
 *      PLX Device Selection Functions
 *****************************************/
RETURN_CODE EXPORT
PlxPci_DeviceOpen(
    PLX_DEVICE_KEY    *pKey,
    PLX_DEVICE_OBJECT *pDevice
    );

RETURN_CODE EXPORT
PlxPci_DeviceClose(
    PLX_DEVICE_OBJECT *pDevice
    );

RETURN_CODE EXPORT
PlxPci_DeviceFind(
    PLX_DEVICE_KEY *pKey,
    U8              DeviceNumber
    );


/******************************************
 *   Query Version Information Functions
 *****************************************/
RETURN_CODE EXPORT
PlxPci_ApiVersion(
    U8 *pVersionMajor,
    U8 *pVersionMinor,
    U8 *pVersionRevision
    );

RETURN_CODE EXPORT
PlxPci_DriverVersion(
    PLX_DEVICE_OBJECT *pDevice,
    U8                *pVersionMajor,
    U8                *pVersionMinor,
    U8                *pVersionRevision
    );

RETURN_CODE EXPORT
PlxPci_ChipTypeGet(
    PLX_DEVICE_OBJECT *pDevice,
    U32               *pChipType,
    U8                *pRevision
    );

RETURN_CODE EXPORT
PlxPci_ChipTypeSet(
    PLX_DEVICE_OBJECT *pDevice,
    U32                ChipType,
    U8                 Revision
    );


/******************************************
 *        Device Control Functions
 *****************************************/
VOID EXPORT
PlxPci_DeviceReset(
    PLX_DEVICE_OBJECT *pDevice
    );


/******************************************
 *        Register Access Functions
 *****************************************/
U32 EXPORT
PlxPci_PciRegisterRead(
    U8           bus,
    U8           slot,
    U8           function,
    U16          offset,
    RETURN_CODE *pReturnCode
    );

RETURN_CODE EXPORT
PlxPci_PciRegisterWrite(
    U8  bus,
    U8  slot,
    U8  function,
    U16 offset,
    U32 value
    );

U32 EXPORT
PlxPci_PciRegisterRead_Unsupported(
    U8           bus,
    U8           slot,
    U8           function,
    U16          offset,
    RETURN_CODE *pReturnCode
    );

RETURN_CODE EXPORT
PlxPci_PciRegisterWrite_Unsupported(
    U8  bus,
    U8  slot,
    U8  function,
    U16 offset,
    U32 value
    );

U32 EXPORT
PlxPci_PlxRegisterRead(
    PLX_DEVICE_OBJECT *pDevice,
    U16                offset,
    RETURN_CODE       *pReturnCode
    );

RETURN_CODE EXPORT
PlxPci_PlxRegisterWrite(
    PLX_DEVICE_OBJECT *pDevice,
    U16                offset,
    U32                value
    );


/******************************************
 *       Data Access Functions
 *****************************************/
RETURN_CODE EXPORT
PlxPci_IoPortRead(
    PLX_DEVICE_OBJECT *pDevice,
    U16                port,
    VOID              *pBuffer,
    U16                SizeInBytes,
    ACCESS_TYPE        AccessType
    );

RETURN_CODE EXPORT
PlxPci_IoPortWrite(
    PLX_DEVICE_OBJECT *pDevice,
    U16                port,
    VOID              *pBuffer,
    U16                SizeInBytes,
    ACCESS_TYPE        AccessType
    );

RETURN_CODE EXPORT
PlxPci_PciBarSizeGet(
    PLX_DEVICE_OBJECT *pDevice,
    U8                 BarIndex,
    U32               *pBarSize
    );

RETURN_CODE EXPORT
PlxPci_PciBarMap(
    PLX_DEVICE_OBJECT *pDevice,
    U8                 BarIndex,
    VOID              *pVa
    );

RETURN_CODE EXPORT
PlxPci_PciBarUnmap(
    PLX_DEVICE_OBJECT *pDevice,
    VOID              *pVa
    );


/******************************************
 * Interrupt Support Functions
 *****************************************/
RETURN_CODE EXPORT
PlxPci_NotificationRegisterFor(
    PLX_DEVICE_OBJECT *pDevice,
    PLX_INTR          *pPlxIntr,
    PLX_NOTIFY_OBJECT *pEvent
    );

RETURN_CODE EXPORT
PlxPci_NotificationWait(
    PLX_DEVICE_OBJECT *pDevice,
    PLX_NOTIFY_OBJECT *pEvent,
    U32                Timeout_ms
    );

RETURN_CODE EXPORT
PlxPci_NotificationStatus(
    PLX_DEVICE_OBJECT *pDevice,
    PLX_NOTIFY_OBJECT *pEvent,
    PLX_INTR          *pPlxIntr
    );

RETURN_CODE EXPORT
PlxPci_NotificationCancel(
    PLX_DEVICE_OBJECT *pDevice,
    PLX_NOTIFY_OBJECT *pEvent
    );



/******************************************
 *     Serial EEPROM Access Functions
 *****************************************/
BOOLEAN EXPORT 
PlxPci_EepromPresent(
    PLX_DEVICE_OBJECT *pDevice,
    RETURN_CODE       *pReturnCode
    );

RETURN_CODE EXPORT
PlxPci_EepromReadByOffset(
    PLX_DEVICE_OBJECT *pDevice,
    U16                offset,
    U16               *pValue
    );

RETURN_CODE EXPORT
PlxPci_EepromWriteByOffset(
    PLX_DEVICE_OBJECT *pDevice,
    U16                offset,
    U16                value
    );



#ifdef __cplusplus
}
#endif

#endif
