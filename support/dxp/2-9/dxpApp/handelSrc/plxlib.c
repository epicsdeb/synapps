/*
* plxlib.c
*
* Created 04/21/04 -- PJF
*
* The PLX API (and all code from the SDK) is
* Copyright (c) 2003 PLX Technology Inc
*
* Copyright (c) 2004, X-ray Instrumentation Associates
*               2005, XIA LLC
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
*/

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

/* Turns off an annoying warning caused by something in Microsoft's rpcasync.h
* file.
*/
#ifdef _WIN32
#pragma warning( disable : 4115 )
#endif /* _WIN32 */

/* Headers from PLX SDK */
#include "PlxApi.h"

#ifdef _WIN32
#pragma warning( default: 4115 )
#endif /* _WIN32 */

#include "Dlldefs.h"
#include "xia_common.h"
#include "xia_assert.h"

#include "plxlib.h"
#include "plxlib_errors.h"

#define PLXLIB_DEBUG 1

/* In milliseconds. Here are the assumptions for this calculation:
* 1) Maximum memory read could be 1M x 32 bits, which is 4MB.
* 2) A good transfer rate for us is 80 MB/s.
* 3) Let's play worst case and say 50 MB/s.
* 4 MB / 50 MB/s -> 40 ms.
*/
#define MAX_BURST_TIMEOUT 40

static void _plx_log_DEBUG(char *msg, ...);
static void _plx_print_more(int err);

static int _plx_find_handle_index(HANDLE h, unsigned long *idx);
static int _plx_add_slot_to_map(HANDLE h);
static int _plx_remove_slot_from_map(HANDLE h);

static boolean_t IS_DEBUG = FALSE_;
static FILE *LOG_FILE = NULL;

static virtual_map_t V_MAP =
  {
    NULL, NULL, 0
  };

static boolean_t IS_REGISTERED = FALSE_;

/** @brief Close a previously opened PCI slot
*
*/
XIA_EXPORT int XIA_API plx_close_slot(HANDLE h)
{
  RETURN_CODE status;


  status = _plx_remove_slot_from_map(h);

  if (status != PLX_SUCCESS) {
    _plx_log_DEBUG("Error unmapping device (h = %#p)\n", h);
    return status;
  }

  status = PlxPciDeviceClose(h);

  if (status != ApiSuccess) {
    _plx_log_DEBUG("Error closing device (h = %#p)\n", h);
    _plx_print_more(status);
    return PLX_API;
  }

  return PLX_SUCCESS;
}


/** @brief Opens the PCI device located using the specified parameters
*
* As the specification for the DEVICE_LOCATION struct states, @a id, @a bus and
* @a slot may be set to -1 to indicate that the value should not be used in the
* search.
*
*/
XIA_EXPORT int XIA_API plx_open_slot(unsigned short id, byte_t bus, byte_t slot,
                                     HANDLE *h)
{
  RETURN_CODE status;

  DEVICE_LOCATION dev;


  /* Search based on supplied device bus and ID */
  dev.BusNumber       = bus;
  dev.SlotNumber      = slot;
  dev.DeviceId        = id;
  dev.VendorId        = (unsigned short)-1;
  dev.SerialNumber[0] = '\0';

  status = PlxPciDeviceOpen(&dev, h);

  if (status != ApiSuccess) {
    _plx_log_DEBUG("Error opening device (id = %u, bus = %u): status = %d\n",
                   id, bus, status);

    _plx_print_more(status);
    return PLX_API;
  }

  status = _plx_add_slot_to_map(*h);

  if (status != PLX_SUCCESS) {
    _plx_log_DEBUG("Error adding device %u/%u/%u to virtual map\n",
                   dev.BusNumber, dev.SlotNumber, dev.DeviceId);
    return status;
  }


  return PLX_SUCCESS;
}


/** @brief Sets the file that debugging messages will be written to.
*
*/
XIA_EXPORT void XIA_API plx_set_file_DEBUG(char *f)
{
  if (LOG_FILE) {
    fclose(LOG_FILE);
  }

  LOG_FILE = fopen(f, "w");

  if (!LOG_FILE) {
    LOG_FILE = stderr;
    return;
  }
}


/** @brief Writes the specified message to the debug output stream
*
*/
static void _plx_log_DEBUG(char *msg, ...)
{
#ifdef PLXLIB_DEBUG

  va_list args;

  if (!LOG_FILE) {
    LOG_FILE = stderr;
  }


  va_start(args, msg);
  vfprintf(LOG_FILE, msg, args);
  va_end(args);

  fflush(LOG_FILE);

#endif /* PLXLIB_DEBUG */
}


/** @brief Prints out more error information based on the strings in the
* API from PLX Technology.
*
* Add more string to error mappings here as needed
*
*/
static void _plx_print_more(int err)
{
  switch (err) {

  case ApiNullParam:
    _plx_log_DEBUG("One or more parameters is NULL\n");
    break;

  case ApiInvalidDeviceInfo:
    _plx_log_DEBUG("The device information did not match any device in the "
                   "system\n");
    break;

  case ApiNoActiveDriver:
    _plx_log_DEBUG("There is no device driver installed into the system\n");
    break;

  case ApiInvalidDriverVersion:
    _plx_log_DEBUG("Driver version does not match the API library version\n");
    break;

  case ApiDmaSglBuildFailed:
    _plx_log_DEBUG("Insufficient internal driver memory to store the SGL "
                   "descriptors\n");
    break;

  default:
    _plx_log_DEBUG("UNKNOWN ERROR (%d) caught in the 'default' case "
                   "statement\n", err);
    break;

  }
}


/** @brief Adds a slot (specified by h) to the virtual map and sets up the
* BAR.
*
*/
static int _plx_add_slot_to_map(HANDLE h)
{
  RETURN_CODE status;

  unsigned long *new_addr = NULL;

  HANDLE *new_h = NULL;

  PLX_NOTIFY_OBJECT *new_events = NULL;

  PLX_INTR *new_intrs = NULL;

  boolean_t *new_registered = NULL;


  V_MAP.n++;

  if (!V_MAP.addr) {
    ASSERT(V_MAP.n == 1);
    ASSERT(!V_MAP.h);

    V_MAP.addr = (unsigned long *)malloc(sizeof(unsigned long));

    if (!V_MAP.addr) {
      _plx_log_DEBUG("Unable to allocate %d bytes for V_MAP.addr array\n",
                     sizeof(unsigned long));
      return PLX_MEM;
    }

    V_MAP.h = (HANDLE *)malloc(sizeof(HANDLE));

    if (!V_MAP.h) {
      _plx_log_DEBUG("Unable to allocate %d bytes for V_MAP.h array\n",
                     sizeof(HANDLE));
      return PLX_MEM;
    }

    V_MAP.events = (PLX_NOTIFY_OBJECT *)malloc(sizeof(PLX_NOTIFY_OBJECT));

    if (!V_MAP.events) {
      _plx_log_DEBUG("Unable to allocate %d bytes for V_MAP.events array\n",
                     sizeof(PLX_NOTIFY_OBJECT));
      return PLX_MEM;
    }

    V_MAP.intrs = (PLX_INTR *)malloc(sizeof(PLX_INTR));

    if (!V_MAP.intrs) {
      _plx_log_DEBUG("Unable to allocate %d bytes for V_MAP.intrs array\n",
                     sizeof(PLX_INTR));
      return PLX_MEM;
    }

    V_MAP.registered = (boolean_t *)malloc(sizeof(boolean_t));

    if (!V_MAP.registered) {
      _plx_log_DEBUG("Unable to allocate %d bytes for V_MAP.registered array\n",
                     sizeof(boolean_t));
      return PLX_MEM;
    }

  } else {

    /* Need to grow the handle array to accomadate another module. We want to
    * do this as an atomic update. Either both memory allocations succeed or we
    * don't update the virtual map.
    */
    new_addr = (unsigned long *)realloc(V_MAP.addr,
                                        V_MAP.n * sizeof(unsigned long));

    if (!new_addr) {
      _plx_log_DEBUG("Unable to allocate %d bytes for 'new_addr'\n",
                     V_MAP.n * sizeof(unsigned long));
      return PLX_MEM;
    }

    new_h = (HANDLE *)realloc(V_MAP.h, V_MAP.n * sizeof(HANDLE));

    if (!new_h) {
      _plx_log_DEBUG("Unable to allocate %d bytes for 'new_h'\n",
                     V_MAP.n * sizeof(HANDLE));
      return PLX_MEM;
    }

    new_events = (PLX_NOTIFY_OBJECT *)realloc(V_MAP.events, V_MAP.n *
                                              sizeof(PLX_NOTIFY_OBJECT));

    if (!new_events) {
      _plx_log_DEBUG("Unable to allocate %d bytes for 'new_events'\n",
                     V_MAP.n * sizeof(PLX_NOTIFY_OBJECT));
      return PLX_MEM;
    }

    new_intrs = (PLX_INTR *)realloc(V_MAP.intrs, V_MAP.n * sizeof(PLX_INTR));

    if (!new_intrs) {
      _plx_log_DEBUG("Unable to allocate %d bytes for 'new_intrs'\n",
                     V_MAP.n * sizeof(PLX_INTR));
      return PLX_MEM;
    }

    new_registered = (boolean_t *)realloc(V_MAP.registered, V_MAP.n *
                                          sizeof(boolean_t));

    if (!new_registered) {
      _plx_log_DEBUG("Unable to allocate %d bytes for 'new_registered'\n",
                     V_MAP.n * sizeof(boolean_t));
      return PLX_MEM;
    }

    V_MAP.addr       = new_addr;
    V_MAP.h          = new_h;
    V_MAP.events     = new_events;
    V_MAP.intrs      = new_intrs;
    V_MAP.registered = new_registered;

  }

  V_MAP.h[V_MAP.n - 1] = h;

  status = PlxPciBarMap(h, PLX_PCI_SPACE_0, &(V_MAP.addr[V_MAP.n - 1]));

  if (status != ApiSuccess) {
    /* Undo memory allocation, etc. here */
    _plx_log_DEBUG("Error getting BAR for handle %#p\n", h);
    _plx_print_more(status);
    return PLX_API;
  }

  V_MAP.registered[V_MAP.n - 1] = FALSE_;

  return PLX_SUCCESS;
}


/** @brief Remove the slot (specified by h) from the system
*
* Includes unmapping the BAR
*/
static int _plx_remove_slot_from_map(HANDLE h)
{
  RETURN_CODE status;

  unsigned long i;
  unsigned long j;
  unsigned long idx;

  unsigned long *new_addr = NULL;

  HANDLE *new_h = NULL;

  PLX_NOTIFY_OBJECT *new_events = NULL;

  PLX_INTR *new_intrs = NULL;

  boolean_t *new_registered = NULL;

  status = _plx_find_handle_index(h, &idx);

  if (status != PLX_SUCCESS) {
    _plx_log_DEBUG("Error removing slot with HANDLE %#p\n", h);
    return status;
  }

  ASSERT(V_MAP.h[idx] == h);

  status = PlxPciBarUnmap(h, &(V_MAP.addr[idx]));

  if (status != ApiSuccess) {
    _plx_log_DEBUG("Error unmapping HANDLE %#p\n", h);
    _plx_print_more(status);
    return PLX_API;
  }

  /* Shrink the virtual map */
  new_addr = (unsigned long *)malloc((V_MAP.n - 1) * sizeof(unsigned long));

  if (!new_addr) {
    _plx_log_DEBUG("Unable to allocate %d bytes for 'new_addr'\n",
                   (V_MAP.n - 1) * sizeof(unsigned long));
    return PLX_MEM;
  }

  new_h = (HANDLE *)malloc((V_MAP.n - 1) * sizeof(HANDLE));

  if (!new_h) {
    _plx_log_DEBUG("Unable to allocate %d bytes for 'new_h'\n",
                   (V_MAP.n - 1) * sizeof(HANDLE));
    return PLX_MEM;
  }

  new_events = (PLX_NOTIFY_OBJECT *)malloc((V_MAP.n - 1) *
                                           sizeof(PLX_NOTIFY_OBJECT));

  if (!new_events) {
    _plx_log_DEBUG("Unable to allocate %d bytes for 'new_events'\n",
                   (V_MAP.n - 1) * sizeof(PLX_NOTIFY_OBJECT));
    return PLX_MEM;
  }

  new_intrs = (PLX_INTR *)malloc((V_MAP.n - 1) * sizeof(PLX_INTR));

  if (!new_intrs) {
    _plx_log_DEBUG("Unable to allocated %d bytes for 'new_intrs'\n",
                   (V_MAP.n - 1) * sizeof(PLX_INTR));
    return PLX_MEM;
  }

  new_registered = (boolean_t *)malloc((V_MAP.n - 1) * sizeof(boolean_t));

  if (!new_registered) {
    _plx_log_DEBUG("Unable to allocated %d bytes for 'new_registered'\n",
                   (V_MAP.n - 1) * sizeof(boolean_t));
    return PLX_MEM;
  }
  /* Transfer the old addr/h contents over, skipping the unmapped index */
  for (i = 0, j = 0; i < V_MAP.n; i++) {
    if (i != idx) {
      new_addr[j]   = V_MAP.addr[i];
      new_h[j]      = V_MAP.h[i];
      new_events[j] = V_MAP.events[i];
      new_intrs[j]  = V_MAP.intrs[i];
      new_registered[j]  = V_MAP.registered[i];
      j++;
    }
  }

  free(V_MAP.addr);
  free(V_MAP.h);
  free(V_MAP.events);
  free(V_MAP.intrs);
  free(V_MAP.registered);

  V_MAP.n--;

  if (V_MAP.n == 0) {
    /* Last slot has been removed. Need to free all the memory */
    free(new_addr);
    free(new_h);
    free(new_events);
    free(new_intrs);
    free(new_registered);
    
    V_MAP.addr = NULL;
    V_MAP.h = NULL;
    V_MAP.events = NULL;
    V_MAP.intrs = NULL;
    V_MAP.registered = NULL;    
  } else {
    V_MAP.addr   = new_addr;
    V_MAP.h      = new_h;
    V_MAP.events = new_events;
    V_MAP.intrs  = new_intrs;
    V_MAP.registered  = new_registered;
  }

  return PLX_SUCCESS;
}


/** @brief Find the index of the specified HANDLE in the virtual map
*
*/
static int _plx_find_handle_index(HANDLE h, unsigned long *idx)
{
  unsigned long i;


  ASSERT(h);
  ASSERT(idx);


  for (i = 0; i < V_MAP.n; i++) {
    if (V_MAP.h[i] == h) {
      *idx = i;
      return PLX_SUCCESS;
    }
  }

  _plx_log_DEBUG("Unable to locate HANDLE %#p in the virtual map\n", h);

  return PLX_UNKNOWN_HANDLE;
}


/** @brief Read a long from the specified address
*
*/
XIA_EXPORT int XIA_API plx_read_long(HANDLE h, unsigned long addr,
                                     unsigned long *data)
{
  RETURN_CODE status;

  unsigned long idx;


  ASSERT(h);
  ASSERT(data);


  status = _plx_find_handle_index(h, &idx);

  if (status != PLX_SUCCESS) {
    _plx_log_DEBUG("Unable to find HANDLE %#p\n", h);
    return status;
  }

  *data = ACCESS_VADDR(idx, addr);

#ifdef PLX_DEBUG_IO_TRACE
  _plx_log_DEBUG("[plx_read_long] addr = %#x, data = %#x\n", addr, data);
#endif /* PLX_DEBUG_IO_TRACE */

  /* XXX What to do here? */

  return PLX_SUCCESS;
}


/** @brief Write a long to the specified address
*
*/
XIA_EXPORT int XIA_API plx_write_long(HANDLE h, unsigned long addr,
                                      unsigned long data)
{
  RETURN_CODE status;

  unsigned long idx;


  ASSERT(h);


  status = _plx_find_handle_index(h, &idx);

  if (status != PLX_SUCCESS) {
    _plx_log_DEBUG("Unable to find HANDLE %#p\n", h);
    return status;
  }

#ifdef PLX_DEBUG_IO_TRACE
  _plx_log_DEBUG("[plx_write_long] addr = %#x, data = %#x\n", addr, data);
#endif /* PLX_DEBUG_IO_TRACE */

  ASSERT(V_MAP.addr[idx] != 0);

  ACCESS_VADDR(idx, addr) = data;

  return PLX_SUCCESS;
}


/** @brief 'Burst' read a block of data
*
*/
XIA_EXPORT int XIA_API plx_read_block(HANDLE h, unsigned long addr,
                                      unsigned long len, unsigned long n_dead,
                                      unsigned long *data)
{
  unsigned long idx;

  unsigned long *local = NULL;

  RETURN_CODE status;
  RETURN_CODE ignored_status;

  DMA_CHANNEL_DESC dma_chan_desc;

  DMA_TRANSFER_ELEMENT dma_data;


  ASSERT(len > 0);
  ASSERT(data != NULL);


  memset(&dma_chan_desc, 0, sizeof(DMA_CHANNEL_DESC));

  dma_chan_desc.EnableReadyInput    = 1;
  dma_chan_desc.EnableBTERMInput    = 1;
  dma_chan_desc.EnableIopBurst      = 1;
  dma_chan_desc.DmaStopTransferMode = AssertBLAST;
  dma_chan_desc.IopBusWidth         = 3;
  dma_chan_desc.DmaChannelPriority  = Channel0Highest;

  status = PlxDmaSglChannelOpen(h, PrimaryPciChannel0, &dma_chan_desc);

  if (status != ApiSuccess) {
    _plx_log_DEBUG("Error opening PCI channel 0 for 'burst' read: HANDLE %#p\n",
                   h);
    _plx_print_more(status);
    return PLX_API;
  }

  status = _plx_find_handle_index(h, &idx);

  if (status != PLX_SUCCESS) {
    ignored_status = PlxDmaSglChannelClose(h, PrimaryPciChannel0);
    _plx_log_DEBUG("Error finding handle (h = %#p) index\n", h);
    _plx_print_more(status);
    return PLX_API;
  }

  /* If the handle is not registered as a notifier, then we need to do it.
  * this only needs to be done once per handle.
  */
  if (!V_MAP.registered[idx]) {
    memset(&(V_MAP.intrs[idx]), 0, sizeof(PLX_INTR));
    V_MAP.intrs[idx].PciDmaChannel0 = 1;

    status = PlxNotificationRegisterFor(h, &(V_MAP.intrs[idx]),
                                        &(V_MAP.events[idx]));

    if (status != ApiSuccess) {
      ignored_status = PlxDmaSglChannelClose(h, PrimaryPciChannel0);
      _plx_log_DEBUG("Error registering for notification of PCI DMA channel 0: "
                     "HANDLE %#p\n", h);
      _plx_print_more(status);
      return PLX_API;
    }

    V_MAP.registered[idx] = TRUE_;
  }

  *((unsigned long *)(V_MAP.addr[idx] + (0x50))) = addr;

  memset(&dma_data, 0, sizeof(DMA_TRANSFER_ELEMENT));

  /* We include the dead words in the transfer */
  local = (unsigned long *)malloc((len + n_dead) * sizeof(unsigned long));

  if (!local) {
    _plx_log_DEBUG("Error allocating %d bytes for 'local'.\n",
                   (len + 2) * sizeof(unsigned long));
    return PLX_MEM;
  }

  dma_data.u.UserVa          = (U32)local;
  dma_data.LocalAddr         = EXTERNAL_MEMORY_LOCAL_ADDR;
  dma_data.TransferCount     = (len + n_dead) * 4;
  dma_data.LocalToPciDma     = 1;
  dma_data.TerminalCountIntr = 0;

  status = PlxDmaSglTransfer(h, PrimaryPciChannel0, &dma_data, TRUE);

  if (status != ApiSuccess) {
    free(local);
    ignored_status = PlxDmaSglChannelClose(h, PrimaryPciChannel0);
    _plx_log_DEBUG("Error during 'burst' read: HANDLE %#p\n", h);
    _plx_print_more(status);
    return PLX_API;
  }

  /* ASSERT((V_MAP.events[idx]).IsValidTag == PLX_TAG_VALID); */
  status = PlxNotificationWait(h, &(V_MAP.events[idx]), 10000);

  if (status != ApiSuccess) {
    free(local);    
    ignored_status = PlxDmaSglChannelClose(h, PrimaryPciChannel0);
    _plx_log_DEBUG("Error waiting for 'burst' read to complete: HANDLE %#p\n", h);
    _plx_print_more(status);
    return PLX_API;
  }

  memcpy(data, local + n_dead, len * sizeof(unsigned long));
  free(local);

  status = PlxDmaSglChannelClose(h, PrimaryPciChannel0);

  if (status != ApiSuccess) {
    _plx_log_DEBUG("Error closing PCI channel 0: HANDLE %#p\n", h);
    _plx_print_more(status);
    return PLX_API;
  }

  return PLX_SUCCESS;
}

