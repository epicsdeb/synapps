#ifndef __PLX_STATUS_H
#define __PLX_STATUS_H

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
 *      PlxStat.h
 *
 * Description:
 *
 *      This file defines all the status codes for PLX SDK
 *
 * Revision:
 *
 *      12-01-07 : PLX SDK v5.20
 *
 ******************************************************************************/


#ifdef __cplusplus
extern "C" {
#endif




/******************************************
*             Definitions
******************************************/
#define PLX_STATUS_START               0x200   // Starting status code


// API Return Code Values
typedef enum _PLX_STATUS
{
    ApiSuccess = PLX_STATUS_START,
    ApiFailed,
    ApiNullParam,
    ApiUnsupportedFunction,
    ApiNoActiveDriver,
    ApiConfigAccessFailed,
    ApiInvalidDeviceInfo,
    ApiInvalidDriverVersion,
    ApiInvalidOffset,
    ApiInvalidData,
    ApiInvalidSize,
    ApiInvalidAddress,
    ApiInvalidAccessType,
    ApiInvalidIndex,
    ApiInvalidPowerState,
    ApiInvalidIopSpace,
    ApiInvalidHandle,
    ApiInvalidPciSpace,
    ApiInvalidBusIndex,
    ApiInsufficientResources,
    ApiWaitTimeout,
    ApiWaitCanceled,
    ApiDmaChannelUnavailable,
    ApiDmaChannelInvalid,
    ApiDmaDone,
    ApiDmaPaused,
    ApiDmaInProgress,
    ApiDmaCommandInvalid,
    ApiDmaInvalidChannelPriority,
    ApiDmaSglPagesGetError,
    ApiDmaSglPagesLockError,
    ApiMuFifoEmpty,
    ApiMuFifoFull,
    ApiPowerDown,
    ApiHSNotSupported,
    ApiVPDNotSupported,
    ApiLastError               // Do not add API errors below this line
} PLX_STATUS;


// DBG - Added to support legacy code -- will be removed in future release
#if !defined(PLX_DOS)
    typedef PLX_STATUS        RETURN_CODE;
#endif



#ifdef __cplusplus
}
#endif

#endif
