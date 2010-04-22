/**
 * @file xia_usb2.c
 * @brief XIA USB2.0 driver, based on cyusb.sys.
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
 * $Id: xia_usb2.c,v 1.5 2009-07-16 17:00:51 rivers Exp $
 */

#include "windows.h"

#include <stdlib.h>
#include <stdio.h>

#include "setupapi.h"

#include "cyioctl.h"

#include "xia_assert.h"

#include "xia_usb2_api.h"
#include "xia_usb2_errors.h"
#include "xia_usb2_private.h"


/* Prototypes */
static int xia_usb2__send_setup_packet(HANDLE h, unsigned long addr,
                                       unsigned long n_bytes, byte_t rw_flag);
static int xia_usb2__xfer(HANDLE h, byte_t ep, DWORD n_bytes, byte_t *buf);
static int xia_usb2__small_read_xfer(HANDLE h, DWORD n_bytes, byte_t *buf);
static int xia_usb2__set_max_packet_size(HANDLE h);

/* Currently we aren't using these routines for anything, but since they
 * were a pain to write (lots of boring details), I thought we should keep
 * them around in case we need to debug any of these data structures in the
 * future.
 */
static void xia_usb2__dump_config_desc(xia_usb2_configuration_descriptor_t *d);
static void xia_usb2__dump_interf_desc(xia_usb2_interface_descriptor_t *d);
static void xia_usb2__dump_ep_desc(xia_usb2_endpoint_descriptor_t *d);


/* This is the Cypress GUID. We may need to generate our own. */
static GUID CYPRESS_GUID = {0xae18aa60, 0x7f6a, 0x11d4, {0x97, 0xdd, 0x0, 0x1,
                            0x2, 0x29, 0xb9, 0x59}};

/* It is more efficient to transfer a complete buffer of whatever the
 * max packet size, even if the amount of bytes requested is less then that.
 */
static unsigned long XIA_USB2_SMALL_READ_PACKET_SIZE = 0;


/**
 * @brief Opens the device with the specified number (@a dev) and returns
 * a valid @c HANDLE to the device or @c NULL if the device could not
 * be opened.
 */
XIA_EXPORT int XIA_API xia_usb2_open(int dev, HANDLE *h)
{
  HDEVINFO dev_info;

  SP_DEVICE_INTERFACE_DATA intfc_data;

  SP_DEVINFO_DATA dev_info_data;

  PSP_INTERFACE_DEVICE_DETAIL_DATA intfc_detail_data;

  BOOL success;

  DWORD intfc_detail_size;
  DWORD err;

  int status;


  dev_info = SetupDiGetClassDevs(&CYPRESS_GUID, NULL, NULL,
                                 DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);

  if (dev_info == NULL) {
    return XIA_USB2_GET_CLASS_DEVS;
  }

  intfc_data.cbSize = sizeof(intfc_data);

  success = SetupDiEnumDeviceInterfaces(dev_info, 0, &CYPRESS_GUID, dev,
                                       &intfc_data);

  if (!success) {
    SetupDiDestroyDeviceInfoList(dev_info);    
    return XIA_USB2_ENUM_DEV_INTFC;
  }

  /* Call this twice: once to get the size of the returned buffer and
   * once to fill the buffer.
   */
  success = SetupDiGetDeviceInterfaceDetail(dev_info, &intfc_data, NULL, 0,
                                           &intfc_detail_size, NULL);

  /* Per Microsoft's documentation, this function should return an
   * ERROR_INSUFFICIENT_BUFFER value.
   */
  if (success) {
    SetupDiDestroyDeviceInfoList(dev_info);
    printf("Status magically was true!\n");
    return XIA_USB2_DEV_INTFC_DETAIL;
  }

  if (GetLastError() != 0x7A) {
    SetupDiDestroyDeviceInfoList(dev_info);
    printf("Last error wasn't 0x7A!\n");
    return XIA_USB2_DEV_INTFC_DETAIL;
  }

  intfc_detail_data = malloc(intfc_detail_size);

  if (intfc_detail_data == NULL) {
    SetupDiDestroyDeviceInfoList(dev_info);
    return XIA_USB2_NO_MEM;
  }

  intfc_detail_data->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA);
  
  dev_info_data.cbSize = sizeof(dev_info_data);

  success = SetupDiGetDeviceInterfaceDetail(dev_info, &intfc_data,
                                            intfc_detail_data,
                                            intfc_detail_size, NULL,
                                            &dev_info_data);

  if (!success) {
    free(intfc_detail_data);
    SetupDiDestroyDeviceInfoList(dev_info);
    err = GetLastError();
    printf("Last Error = %#lx\n", err);
    return XIA_USB2_DEV_INTFC_DETAIL;
  }

  *h = CreateFile(intfc_detail_data->DevicePath, GENERIC_WRITE | GENERIC_READ,
                  FILE_SHARE_WRITE | FILE_SHARE_READ, NULL,
                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  free(intfc_detail_data);
  SetupDiDestroyDeviceInfoList(dev_info);

  if (*h == INVALID_HANDLE_VALUE) {
      err = GetLastError();
      printf("Last Error = %#lx\n", err);
      return XIA_USB2_INVALID_HANDLE;
  }

  status = xia_usb2__set_max_packet_size(*h);

  if (status != XIA_USB2_SUCCESS) {
      return status;
  }

  return XIA_USB2_SUCCESS;
}


/**
 * @brief Closes a device handle (@a h) previously opened via. xia_usb2_open().
 */
XIA_EXPORT int XIA_API xia_usb2_close(HANDLE h)
{
  BOOL status;
  
  DWORD err;


  status = CloseHandle(h);

  if (!status) {
    err = GetLastError();
    printf("Close Error = %lu\n", err);
    return XIA_USB2_CLOSE_HANDLE;
  }

  return XIA_USB2_SUCCESS;
}


/**
 * @brief Read the specified number of bytes from the specified address and
 * into the specified buffer.
 *
 * @a buf is expected to be allocated by the calling routine.
 */
XIA_EXPORT int XIA_API xia_usb2_read(HANDLE h, unsigned long addr,
                                     unsigned long n_bytes, byte_t *buf)
{
  int status;


  ASSERT(XIA_USB2_SMALL_READ_PACKET_SIZE == 512 ||
         XIA_USB2_SMALL_READ_PACKET_SIZE == 64);


  if (h == NULL) {
    return XIA_USB2_NULL_HANDLE;
  }

  if (n_bytes == 0) {
    return XIA_USB2_ZERO_BYTES;
  }

  if (buf == NULL) {
    return XIA_USB2_NULL_BUFFER;
  }
  
  if (n_bytes < XIA_USB2_SMALL_READ_PACKET_SIZE) {
    status = xia_usb2__send_setup_packet(h, addr,
                                         XIA_USB2_SMALL_READ_PACKET_SIZE,
                                         XIA_USB2_SETUP_FLAG_READ);

    if (status != XIA_USB2_SUCCESS) {
      return status;
    }

    status = xia_usb2__small_read_xfer(h, (DWORD)n_bytes, buf);

  } else {
    status = xia_usb2__send_setup_packet(h, addr, n_bytes,
                                         XIA_USB2_SETUP_FLAG_READ);

    if (status != XIA_USB2_SUCCESS) {
      return status;
    }

    status = xia_usb2__xfer(h, XIA_USB2_READ_EP, (DWORD)n_bytes, buf);
  }

  return status;
}


/**
 * @brief Writes the requested buffer to the requested address.
 */
XIA_EXPORT int XIA_API xia_usb2_write(HANDLE h, unsigned long addr,
                                      unsigned long n_bytes, byte_t *buf)
{
  int status;


  if (h == NULL) {
    return XIA_USB2_NULL_HANDLE;
  }

  if (n_bytes == 0) {
    return XIA_USB2_ZERO_BYTES;
  }

  if (buf == NULL) {
    return XIA_USB2_NULL_BUFFER;
  }

  status = xia_usb2__send_setup_packet(h, addr, n_bytes,
                                       XIA_USB2_SETUP_FLAG_WRITE);

  if (status != XIA_USB2_SUCCESS) {
    return status;
  }

  status = xia_usb2__xfer(h, XIA_USB2_WRITE_EP, (DWORD)n_bytes, buf);

  return status;
}


/**
 * @brief Sends an XIA-specific setup packet to the "setup" endpoint. This
 * is the first stage of our two-part process for transferring data to
 * and from the board.
 */
static int xia_usb2__send_setup_packet(HANDLE h, unsigned long addr,
                                       unsigned long n_bytes, byte_t rw_flag)
{
  int status;

  byte_t pkt[XIA_USB2_SETUP_PACKET_SIZE];


  ASSERT(n_bytes != 0);
  ASSERT(rw_flag < 2);

  
  pkt[0] = (byte_t)(addr & 0xFF);
  pkt[1] = (byte_t)((addr >> 8) & 0xFF);
  pkt[2] = (byte_t)(n_bytes & 0xFF);
  pkt[3] = (byte_t)((n_bytes >> 8) & 0xFF);
  pkt[4] = (byte_t)((n_bytes >> 16) & 0xFF);
  pkt[5] = (byte_t)((n_bytes >> 24) & 0xFF);
  pkt[6] = rw_flag;
  pkt[7] = (byte_t)((addr >> 16) & 0xFF);
  pkt[8] = (byte_t)((addr >> 24) & 0xFF);

  status = xia_usb2__xfer(h, XIA_USB2_SETUP_EP, XIA_USB2_SETUP_PACKET_SIZE,
                          pkt);

  return status;
}


/**
 * @brief Wrapper around the low-level transfer to the USB device. Handles
 * the configuration of the SINGLE_TRANSFER structure as required by the
 * Cypress driver.
 *
 * Currently there is no support for a timeout since DeviceIoControl will
 * block on non-overlapped I/O, but it would be easy to add that as an
 * argument to this routine. It would also be easy to use overlapped I/O: 
 * add the appropriate flag to CreateFile() in xia_usb2_open() and pass
 * an OVERLAPPED structure into DeviceIoControl() below.
 */
static int xia_usb2__xfer(HANDLE h, byte_t ep, DWORD n_bytes, byte_t *buf)
{
  SINGLE_TRANSFER st;

  DWORD bytes_ret;
  DWORD err;

  BOOL success;


  ASSERT(n_bytes != 0);
  ASSERT(buf != NULL);


  memset(&st, 0, sizeof(st));
  st.ucEndpointAddress = ep;
  

  success = DeviceIoControl(h, IOCTL_ADAPT_SEND_NON_EP0_DIRECT,
                            &st, sizeof(st), buf, n_bytes, &bytes_ret, NULL);
  
  if (!success) {
    err = GetLastError();
    printf("Xfer Error = %lu\n", err);
    return XIA_USB2_XFER;
  }

  return XIA_USB2_SUCCESS;
}


/**
 * @brief Performs a fast read of a small -- less than the max packet
 * size -- packet.
 *
 * Since the performance of USB2 with small packets is poor, it is faster
 * to read a larger block and extract the small number of bytes we actually
 * want.
 */
static int xia_usb2__small_read_xfer(HANDLE h, DWORD n_bytes, byte_t *buf)
{
  BOOL success;

  byte_t *big_packet = NULL;  

  SINGLE_TRANSFER st;

  DWORD bytes_ret;
  DWORD err;


  ASSERT(XIA_USB2_SMALL_READ_PACKET_SIZE == 512 ||
         XIA_USB2_SMALL_READ_PACKET_SIZE == 64);
  ASSERT(n_bytes < XIA_USB2_SMALL_READ_PACKET_SIZE);
  ASSERT(n_bytes != 0);
  ASSERT(buf != NULL);
  
  big_packet = malloc(XIA_USB2_SMALL_READ_PACKET_SIZE);

  if (!big_packet) {
      return XIA_USB2_NO_MEM;
  }

  memset(&st, 0, sizeof(st));
  st.ucEndpointAddress = XIA_USB2_READ_EP;


  success = DeviceIoControl(h, IOCTL_ADAPT_SEND_NON_EP0_DIRECT,
                            &st, sizeof(st), big_packet,
                            XIA_USB2_SMALL_READ_PACKET_SIZE, &bytes_ret, NULL);

  if (!success) {
      free(big_packet);
      err = GetLastError();
      printf("Xfer Error = %lu\n", err);
      return XIA_USB2_XFER;
  }

  memcpy(buf, big_packet, n_bytes);
  
  free(big_packet);

  return XIA_USB2_SUCCESS;
}


/**
 * XIA USB2 devices transfer data to the host (this code) via EP6. It should
 * be sufficient to read the wMaxPacketSize field from the EP6 descriptor
 * and just use the largest packet size supported by the device which will
 * be either a full-speed or a high-speed device.
 */
static int xia_usb2__set_max_packet_size(HANDLE h)
{
    size_t total_desc_size = 0;

    int total_transfer_size = 0;
    int i;

    SINGLE_TRANSFER *transfer = NULL;

    BOOL success;

    DWORD bytes_ret;
    DWORD err;

    xia_usb2_endpoint_descriptor_t *ep = NULL;


    /* This size is XIA-specific: We care about the first configuration's
     * first interface and the endpoints that we use.
     */
    total_desc_size = sizeof(xia_usb2_configuration_descriptor_t) +
        sizeof(xia_usb2_interface_descriptor_t) +
        XIA_USB2_NUM_ENDPOINTS * sizeof(xia_usb2_endpoint_descriptor_t);

    total_transfer_size = sizeof(SINGLE_TRANSFER) + total_desc_size;

    transfer = (SINGLE_TRANSFER *)malloc(total_transfer_size);
    
    if (transfer == NULL) {
        return XIA_USB2_NO_MEM;
    }

    memset(transfer, 0, total_transfer_size);

    transfer->SetupPacket.bmRequest = XIA_USB2_GET_DESCRIPTOR_REQTYPE;
    transfer->SetupPacket.bRequest  = XIA_USB2_GET_DESCRIPTOR_REQ;
    transfer->SetupPacket.wValue    =
        (unsigned short)(XIA_USB2_CONFIGURATION_DESCRIPTOR_TYPE << 8);
    transfer->SetupPacket.wIndex    = 0;
    transfer->SetupPacket.wLength   = (unsigned short)total_desc_size;
    transfer->SetupPacket.ulTimeOut = 1000;
    transfer->ucEndpointAddress     = XIA_USB2_CONTROL_EP;
    transfer->IsoPacketLength       = 0;
    transfer->BufferOffset          = sizeof(SINGLE_TRANSFER);
    transfer->BufferLength          = total_desc_size;

    success = DeviceIoControl(h, IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER,
                              transfer, total_transfer_size, transfer,
                              total_transfer_size, &bytes_ret, NULL);

    if (!success) {
        free(transfer);
        err = GetLastError();
        printf("Xfer Error = %lu\n", err);
        return XIA_USB2_XFER;
    }
    
    for (i = 0; i < XIA_USB2_NUM_ENDPOINTS; i++) {
        /* Pick the endpoints out from the end of the configuration and
         * interface descriptors.
         */
        ep = (xia_usb2_endpoint_descriptor_t *)
            ((byte_t *)transfer + sizeof(SINGLE_TRANSFER) +
             sizeof(xia_usb2_configuration_descriptor_t) +
             sizeof(xia_usb2_interface_descriptor_t) +
             i * sizeof(xia_usb2_endpoint_descriptor_t));

        if (ep->bEndpointAddress == XIA_USB2_READ_EP) {
            XIA_USB2_SMALL_READ_PACKET_SIZE = ep->wMaxPacketSize;
            free(transfer);
            return XIA_USB2_SUCCESS;
        }
    }

    free(transfer);
    
    /* It is impossible for an XIA device to not have the read EP. */
    ASSERT(FALSE_);
    return XIA_USB2_SUCCESS;
}


/**
 * Debug dump of a configuration descriptor.
 */
static void xia_usb2__dump_config_desc(xia_usb2_configuration_descriptor_t *d)
{
    printf("\nConfiguration Descriptor\n");
    printf("bLength             = %u\n",    d->bLength);
    printf("bDescriptorType     = %#x\n",   d->bDescriptorType);
    printf("wTotalLength        = %u\n",    d->wTotalLength);
    printf("bNumInterfaces      = %u\n",    d->bNumInterfaces);
    printf("bConfigurationValue = %u\n",    d->bConfigurationValue);
    printf("iConfiguration      = %u\n",    d->iConfiguration);
    printf("bmAttributes        = %#x\n",   d->bmAttributes);
    printf("bMaxPower           = %u mA\n", d->bMaxPower * 2);
    printf("\n");
}


/**
 * Debug dump of an interface descriptor.
 */
static void xia_usb2__dump_interf_desc(xia_usb2_interface_descriptor_t *d)
{
    printf("\nInterface Descriptor\n");
    printf("bLength            = %u\n",  d->bLength);
    printf("bDescriptorType    = %#x\n", d->bDescriptorType);
    printf("bInterfaceNumber   = %u\n",  d->bInterfaceNumber);
    printf("bAlternateSetting  = %u\n",  d->bAlternateSetting);
    printf("bNumEndpoints      = %u\n",  d->bNumEndpoints);
    printf("bInterfaceClass    = %#x\n", d->bInterfaceClass);
    printf("bInterfaceSubClass = %#x\n", d->bInterfaceSubClass);
    printf("bInterfaceProtocol = %#x\n", d->bInterfaceProtocol);
    printf("iInterface         = %u\n",  d->iInterface);
    printf("\n");
}


/**
 * Debug dump of an endpoint descriptor.
 */
static void xia_usb2__dump_ep_desc(xia_usb2_endpoint_descriptor_t *d)
{
    printf("\nEndpoint Descriptor\n");
    printf("bLength          = %u\n",  d->bLength);
    printf("bDescriptorType  = %#x\n", d->bDescriptorType);
    printf("bEndpointAddress = %#x\n", d->bEndpointAddress);
    printf("wMaxPacketSize   = %u\n",  d->wMaxPacketSize);
    printf("bInterval        = %u\n",  d->bInterval);
    printf("\n");
}
