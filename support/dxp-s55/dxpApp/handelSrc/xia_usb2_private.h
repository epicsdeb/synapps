/**
 * @file xia_usb2_private.h
 * @brief Private data for the XIA USB2.0 driver. Nothing in this file should
 * be required for users of the driver.
 */

/*
 * Copyright (c) 2006, XIA LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, 
 * with or without modification, are permitted provided 
 * that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer.
 *   * Redistributions in binary form must reproduce the 
 *     above copyright notice, this list of conditions and the 
 *     following disclaimer in the documentation and/or other 
 *     materials provided with the distribution.
 *   * Neither the name of X-ray Instrumentation Associates 
 *     nor the names of its contributors may be used to endorse 
 *     or promote products derived from this software without 
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF 
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 *
 *
 * $Id: xia_usb2_private.h,v 1.5 2009-07-16 16:59:23 rivers Exp $
 */

#ifndef __XIA_USB2_API_PRIVATE_H__
#define __XIA_USB2_API_PRIVATE_H__

#ifdef _WIN32
#include "windows.h"
#endif

#include "xia_common.h"


#define XIA_USB2_NUM_ENDPOINTS 4

/* Custom messages */
#define WM_CLOSE_DEVICE  (WM_USER + 1)


/* Transfer types */
#define XIA_USB2_READ     0x0


/* Endpoints */
#define XIA_USB2_SETUP_EP   0x1
#define XIA_USB2_READ_EP    0x82
#define XIA_USB2_WRITE_EP   0x6
#define XIA_USB2_CONTROL_EP 0x0


/* XIA USB2 protocol constants */
#define XIA_USB2_SETUP_PACKET_SIZE      9

#define XIA_USB2_SETUP_FLAG_WRITE   0x0
#define XIA_USB2_SETUP_FLAG_READ    0x1


/* Control endpoint requests */
#define XIA_USB2_GET_DESCRIPTOR_REQTYPE        0x80
#define XIA_USB2_GET_DESCRIPTOR_REQ            6
#define XIA_USB2_CONFIGURATION_DESCRIPTOR_TYPE 2


/* Configuration descriptor using the naming convention of the USB 2.0
 * standard.
 */
#pragma pack(1)
typedef struct _configuration_descriptor {
    byte_t         bLength;
    byte_t         bDescriptorType;
    unsigned short wTotalLength;
    byte_t         bNumInterfaces;
    byte_t         bConfigurationValue;
    byte_t         iConfiguration;
    byte_t         bmAttributes;
    byte_t         bMaxPower;
} xia_usb2_configuration_descriptor_t;
#pragma pack()

/* Interface descriptor using the naming convention of the USB 2.0
 * standard.
 */
#pragma pack(1)
typedef struct _interface_descriptor {
    byte_t bLength;
    byte_t bDescriptorType;
    byte_t bInterfaceNumber;
    byte_t bAlternateSetting;
    byte_t bNumEndpoints;
    byte_t bInterfaceClass;
    byte_t bInterfaceSubClass;
    byte_t bInterfaceProtocol;
    byte_t iInterface;
} xia_usb2_interface_descriptor_t;
#pragma pack()

/* Endpoint descriptor using the naming convention of the USB 2.0
 * standard.
 */
#pragma pack(1)
typedef struct _endpoint_descriptor {
    byte_t         bLength;
    byte_t         bDescriptorType;
    byte_t         bEndpointAddress;
    byte_t         bmAttributes;
    unsigned short wMaxPacketSize;
    byte_t         bInterval;
} xia_usb2_endpoint_descriptor_t;
#pragma pack()

#endif /* __XIA_USB2_API_PRIVATE_H__ */
