#ifndef __PLX_H
#define __PLX_H

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
 *      Plx.h
 *
 * Description:
 *
 *      This file contains definitions that are common to all PCI SDK code.
 *
 * Revision:
 *
 *      06-01-05 : PCI SDK v4.40
 *
 ******************************************************************************/


#ifdef __cplusplus
extern "C" {
#endif




/**********************************************
*               Definitions
**********************************************/
/* SDK Version information */
#define PLX_SDK_VERSION_MAJOR            4
#define PLX_SDK_VERSION_MINOR            4
#define PLX_SDK_VERSION_REVISION         0

#define MAX_PCI_BUS                      32             /* Max PCI Buses */
#define MAX_PCI_DEV                      32             /* Max PCI Slots */
#define MAX_PCI_FUNC                     8              /* Max PCI Functions */
#define PCI_NUM_BARS_TYPE_00             6              /* Total PCI BARs for Type 0 Header */
#define PCI_NUM_BARS_TYPE_01             2              /* Total PCI BARs for Type 1 Header */

#define PLX_VENDOR_ID                    0x10B5         /* PLX Vendor ID */
#define PLX_VENDOR_ID_PTOP               0x3388         /* PLX Vendor ID in PCI-to-PCI Bridges (PCI 6000 series) */

/* Device object validity codes */
#define PLX_TAG_VALID                    0x5F504C58     /* "_PLX" in Hex */
#define PLX_TAG_INVALID                  0x564F4944     /* "VOID" in Hex */
#define ObjectValidate(pObj)             ((pObj)->IsValidTag = PLX_TAG_VALID)
#define ObjectInvalidate(pObj)           ((pObj)->IsValidTag = PLX_TAG_INVALID)
#define IsObjectValid(pObj)              ((pObj)->IsValidTag == PLX_TAG_VALID)

/* PLX chip default Device IDs */
#define PLX_9050_DEVICE_ID               0x9050
#define PLX_9030_DEVICE_ID               0x9030
#define PLX_9060_DEVICE_ID               0x9060
#define PLX_9080_DEVICE_ID               0x9080
#define PLX_9054_DEVICE_ID               0x9054
#define PLX_9056_DEVICE_ID               0x9056
#define PLX_9656_DEVICE_ID               0x9656

/* Device IDs of PLX reference boards */
#define PLX_9080RDK_960_DEVICE_ID        0x0960
#define PLX_9080RDK_401B_DEVICE_ID       0x0401
#define PLX_9080RDK_860_DEVICE_ID        0x0860
#define PLX_9054RDK_860_DEVICE_ID        0x1860
#define PLX_9054RDK_LITE_DEVICE_ID       0x5406
#define PLX_CPCI9054RDK_860_DEVICE_ID    0xC860
#define PLX_9056RDK_LITE_DEVICE_ID       0x5601
#define PLX_9056RDK_860_DEVICE_ID        0x56c2
#define PLX_9656RDK_LITE_DEVICE_ID       0x9601
#define PLX_9656RDK_860_DEVICE_ID        0x96c2
#define PLX_9030RDK_LITE_DEVICE_ID       0x3001
#define PLX_CPCI9030RDK_LITE_DEVICE_ID   0x30c1
#define PLX_9050RDK_LITE_DEVICE_ID       0x9050
#define PLX_9052RDK_LITE_DEVICE_ID       0x5201

/* Message Unit FIFO sizes */
#define FIFO_SIZE_512B                   0x00200
#define FIFO_SIZE_2K                     0x00800
#define FIFO_SIZE_8K                     0x02000
#define FIFO_SIZE_16K                    0x04000
#define FIFO_SIZE_32K                    0x08000
#define FIFO_SIZE_64K                    0x10000
#define FIFO_SIZE_128K                   0x20000
#define FIFO_SIZE_256K                   0x40000

/* Used for locating PCI devices */
#define PCI_FIELD_IGNORE                 (-1)

/* Hot swap status definitions */
#define HS_LED_ON                        0x08
#define HS_BOARD_REMOVED                 0x40
#define HS_BOARD_INSERTED                0x80

/* New Capabilities flags */
#define CAPABILITY_POWER_MANAGEMENT      (1 << 0)
#define CAPABILITY_HOT_SWAP              (1 << 1)
#define CAPABILITY_VPD                   (1 << 2)

/* Power Management States */
#define PM_D0_STATE                      0
#define PM_D1_STATE                      1
#define PM_D2_STATE                      2
#define PM_D3HOT_STATE                   3

/* Used for VPD accesses */
#if defined(IOP_CODE)
    #define VPD_COMMAND_MAX_RETRIES      10        /* Max number VPD command re-issues */
    #define VPD_STATUS_MAX_POLL          10000     /* Max number of times to read VPD status */
    #define VPD_STATUS_POLL_DELAY        500       /* Delay between polling VPD status */
#else
    #define VPD_COMMAND_MAX_RETRIES      5         /* Max number VPD command re-issues */
    #define VPD_STATUS_MAX_POLL          10        /* Max number of times to read VPD status */
    #define VPD_STATUS_POLL_DELAY        5         /* Delay between polling VPD status (Milliseconds) */
#endif

/* Doorbell value to initiate a local CPU reset */
#define PLX_RESET_EMBED_INT              ((unsigned long)1 << 31)

/* Define a large value for a signal to the driver */
#define FIND_AMOUNT_MATCHED              80001


/* Constants for Big-Endian 32-bit bit positions */
#define BE_U32_BIT0                      0x80000000
#define BE_U32_BIT1                      0x40000000
#define BE_U32_BIT2                      0x20000000
#define BE_U32_BIT3                      0x10000000
#define BE_U32_BIT4                      0x08000000
#define BE_U32_BIT5                      0x04000000
#define BE_U32_BIT6                      0x02000000
#define BE_U32_BIT7                      0x01000000
#define BE_U32_BIT8                      0x00800000
#define BE_U32_BIT9                      0x00400000
#define BE_U32_BIT10                     0x00200000
#define BE_U32_BIT11                     0x00100000
#define BE_U32_BIT12                     0x00080000
#define BE_U32_BIT13                     0x00040000
#define BE_U32_BIT14                     0x00020000
#define BE_U32_BIT15                     0x00010000
#define BE_U32_BIT16                     0x00008000
#define BE_U32_BIT17                     0x00004000
#define BE_U32_BIT18                     0x00002000
#define BE_U32_BIT19                     0x00001000
#define BE_U32_BIT20                     0x00000800
#define BE_U32_BIT21                     0x00000400
#define BE_U32_BIT22                     0x00000200
#define BE_U32_BIT23                     0x00000100
#define BE_U32_BIT24                     0x00000080
#define BE_U32_BIT25                     0x00000040
#define BE_U32_BIT26                     0x00000020
#define BE_U32_BIT27                     0x00000010
#define BE_U32_BIT28                     0x00000008
#define BE_U32_BIT29                     0x00000004
#define BE_U32_BIT30                     0x00000002
#define BE_U32_BIT31                     0x00000001


/* Constants for Big-Endian 16-bit bit positions */
#define BE_U16_BIT0                      0x8000
#define BE_U16_BIT1                      0x4000
#define BE_U16_BIT2                      0x2000
#define BE_U16_BIT3                      0x1000
#define BE_U16_BIT4                      0x0800
#define BE_U16_BIT5                      0x0400
#define BE_U16_BIT6                      0x0200
#define BE_U16_BIT7                      0x0100
#define BE_U16_BIT8                      0x0080
#define BE_U16_BIT9                      0x0040
#define BE_U16_BIT10                     0x0020
#define BE_U16_BIT11                     0x0010
#define BE_U16_BIT12                     0x0008
#define BE_U16_BIT13                     0x0004
#define BE_U16_BIT14                     0x0002
#define BE_U16_BIT15                     0x0001


/* Constants for Big-Endian 8-bit bit positions */
#define BE_U8_BIT0                       0x80
#define BE_U8_BIT1                       0x40
#define BE_U8_BIT2                       0x20
#define BE_U8_BIT3                       0x10
#define BE_U8_BIT4                       0x08
#define BE_U8_BIT5                       0x04
#define BE_U8_BIT6                       0x02
#define BE_U8_BIT7                       0x01




#ifdef __cplusplus
}
#endif

#endif
