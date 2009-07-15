#ifndef __PCITYPES_H
#define __PCITYPES_H

/*******************************************************************************
 * Copyright (c) 2004 PLX Technology, Inc.
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
 *      PciTypes.h
 *
 * Description:
 *
 *      This file defines the basic types available to the PCI code.
 *
 * Revision:
 *
 *      05-01-04 : PCI SDK v4.30
 *
 ******************************************************************************/


#include "PlxError.h"


#if defined(WDM_DRIVER)
    #include <wdm.h>            /* WDM Driver types */
#endif

#if defined(NT_DRIVER)
    #include <ntddk.h>          /* NT Kernel Mode Driver types */
#endif

#if defined(PLX_VXD_DRIVER)
    #include <basedef.h>        /* Win9x/Me VxD Driver types */
#endif

#if defined(_WIN32) && !defined(PLX_DRIVER)
    #include <wtypes.h>         /* Windows application level types */
#endif

#if defined(PLX_LINUX)
    #include <memory.h>         /* To prevent application compile errors in Linux */
#endif

#if defined(PLX_LINUX) || defined(PLX_LINUX_DRIVER)
    #include <linux/types.h>    /* Linux types */
#endif


#ifdef __cplusplus
extern "C" {
#endif



/******************************************
 *   Linux Application Level Definitions
 ******************************************/
#if defined(PLX_LINUX)
    typedef __s8             S8,  *PS8;
    typedef __u8             U8,  *PU8;
    typedef __s16            S16, *PS16;
    typedef __u16            U16, *PU16;
    typedef __s32            S32, *PS32;
    typedef __u32            U32, *PU32;
    typedef __s64            LONGLONG;
    typedef __u64            ULONGLONG;

    typedef void            *HANDLE;
    typedef int              PLX_DRIVER_HANDLE;     /* Linux-specific driver handle */

    #define INVALID_HANDLE_VALUE    (HANDLE)-1
#endif



/******************************************
 *    Linux Kernel Level Definitions
 ******************************************/
#if defined(PLX_LINUX_DRIVER)
    typedef s8               S8,  *PS8;
    typedef u8               U8,  *PU8;
    typedef s16              S16, *PS16;
    typedef u16              U16, *PU16;
    typedef s32              S32, *PS32;
    typedef u32              U32, *PU32;
    typedef s64              LONGLONG;
    typedef u64              ULONGLONG;

    /* For 64-bit physical address */
    typedef union _PHYSICAL_ADDRESS
    {
        struct
        {
        #if defined(PLX_LITTLE_ENDIAN)
            U32  LowPart;
            U32  HighPart;
        #else
            U32  HighPart;
            U32  LowPart;
        #endif
        }u;

        ULONGLONG QuadPart;
    } PHYSICAL_ADDRESS;
#endif



/******************************************
 *      Windows Type Definitions
 ******************************************/
#if defined(_WIN32)
    typedef signed char      S8,  *PS8;
    typedef unsigned char    U8,  *PU8;
    typedef signed short     S16, *PS16;
    typedef unsigned short   U16, *PU16;
    typedef signed long      S32, *PS32;
    typedef unsigned long    U32, *PU32;
    typedef signed _int64    LONGLONG;
    typedef unsigned _int64  ULONGLONG;

    typedef HANDLE           PLX_DRIVER_HANDLE;     /* Windows-specific driver handle */
#endif

/******************************************
 *      Cygwin Type Definitions
 ******************************************/
#if defined(CYGWIN32)
#include <windows.h>  /* This is needed so that HANDLE is defined in Plx code */
#undef _WIN32         /* Need to undefine _WIN32 which windows.h defined or it screws things up */
    typedef signed char      S8,  *PS8;
    typedef unsigned char    U8,  *PU8;
    typedef signed short     S16, *PS16;
    typedef unsigned short   U16, *PU16;
    typedef signed long      S32, *PS32;
    typedef unsigned long    U32, *PU32;
    typedef signed long long    LONGLONG;
    typedef unsigned long long  ULONGLONG;

    typedef int           PLX_DRIVER_HANDLE;     /* Windows-specific driver handle */
#endif



/******************************************
 *    Volatile Basic Type Definitions
 ******************************************/
typedef volatile S8           VS8, *PVS8;
typedef volatile U8           VU8, *PVU8;
typedef volatile S16          VS16, *PVS16;
typedef volatile U16          VU16, *PVU16;
typedef volatile S32          VS32, *PVS32;
typedef volatile U32          VU32, *PVU32;
#if defined(LONGLONG)
    typedef volatile LONGLONG     VLONGLONG;
    typedef volatile ULONGLONG    VULONGLONG;
#endif



/******************************************
 *      Miscellaneous definitions
 ******************************************/
#if !defined(VOID)
    typedef void              VOID, *PVOID;
#endif

typedef U8                    BOOLEAN;

#if !defined(_WIN32) && !defined(CYGWIN32)
    typedef U8                BOOL;
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



/******************************************
 * Definitions for code compatibility
 ******************************************/
typedef U32                 ADDRESS;
typedef S32                 SDATA;
typedef U32                 UDATA;



/******************************************
 * PCI SDK Defined Structures
 ******************************************/

/* Device Location Structure */
typedef struct _DEVICE_LOCATION
{
    U8  BusNumber;
    U8  SlotNumber;
    U16 DeviceId;
    U16 VendorId;
    U8  SerialNumber[12];
} DEVICE_LOCATION;


/* PCI Memory Structure */
typedef struct _PCI_MEMORY
{
    U32 UserAddr;
    U32 PhysicalAddr;
    U32 Size;
} PCI_MEMORY;


/* Used for PCI BAR user-mode virtual address mapping */
typedef struct _PLX_PCI_BAR_SPACE
{
    U32     va;                      /* Virtual address of space */
    U32     size;                    /* Size of mapped region */
    U32     offset;                  /* Actual starting offset from virtual base */
    BOOLEAN bMapAttempted;           /* Flag to specify if mapping was attempted */
} PLX_PCI_BAR_SPACE;


/* PCI Device Key Identifier */
typedef struct _PLX_DEVICE_KEY
{
    U32 IsValidTag;                  /* Magic number to determine validity */
    U8  bus;                         /* Physical device location */
    U8  slot;
    U8  function;
    U16 VendorId;                    /* Device Identifier */
    U16 DeviceId;
    U16 SubVendorId;
    U16 SubDeviceId;
    U8  Revision;
    U8  ApiIndex;                    /* Index used internally by the API */
    U8  DeviceNumber;                /* Number used internally by the device driver */
} PLX_DEVICE_KEY;


#if !defined(PLX_DRIVER) && !defined(PLX_LINUX_DRIVER)
/* PLX Device Object Structure */
typedef struct _PLX_DEVICE_OBJECT
{
    U32                IsValidTag;   /* Magic number to determine validity */
    PLX_DEVICE_KEY     Key;          /* Device location key identifier */
    PLX_DRIVER_HANDLE  hDevice;      /* Handle to driver */
    PLX_PCI_BAR_SPACE  PciBar[6];    /* Used for PCI BAR user-mode virtual address mapping */
} PLX_DEVICE_OBJECT;
#endif


/* PLX Notification Object */
typedef struct _PLX_NOTIFY_OBJECT
{
    U32     IsValidTag;              /* Magic number to determine validity */
 #if defined(_WIN32) || defined(CYGWIN32)
    HANDLE  hEvent;                  /* Handle to notification event */
 #endif
    VOID   *pWaitObject;             /* -- INTERNAL -- Wait object used by the driver */
} PLX_NOTIFY_OBJECT;




#ifdef __cplusplus
}
#endif

#endif
