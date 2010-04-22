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

/* The above include file causes _WIN32 to be defined on Cygwin, undefine it. */
#ifdef CYGWIN32
  #undef _WIN32
#endif

#ifdef _WIN32
#pragma warning( default: 4115 )
#endif /* _WIN32 */

#include "Dlldefs.h"
#include "xia_common.h"
#include "xia_assert.h"

#define PLXLIB_DEBUG 1

#include "plxlib.h"
#include "plxlib_errors.h"

/* In milliseconds. Here are the assumptions for this calculation:
* 1) Maximum memory read could be 1M x 32 bits, which is 4MB.
* 2) A good transfer rate for us is 80 MB/s.
* 3) Let's play worst case and say 50 MB/s.
* 4 MB / 50 MB/s -> 40 ms.
*/
#define MAX_BURST_TIMEOUT 40

static void _plx_log_DEBUG(char *msg, ...);
static void _plx_print_more(PLX_STATUS err);

static int _plx_find_handle_index(HANDLE h, unsigned long *idx);
static int _plx_add_slot_to_map(PLX_DEVICE_OBJECT *device);
static int _plx_remove_slot_from_map(unsigned long idx);

/* static boolean_t IS_DEBUG = FALSE_; */
static FILE *LOG_FILE = NULL;

static virtual_map_t V_MAP =
  {
    NULL, NULL, 0
  };

/* static boolean_t IS_REGISTERED = FALSE_; */

/** @PLX error string from error code translation
*
*/
API_ERRORS ApiErrors[] =
{
    { ApiSuccess,                   "ApiSuccess"                   },
    { ApiFailed,                    "ApiFailed"                    },
    { ApiNullParam,                 "ApiNullParam"                 },
    { ApiUnsupportedFunction,       "ApiUnsupportedFunction"       },
    { ApiNoActiveDriver,            "ApiNoActiveDriver"            },
    { ApiConfigAccessFailed,        "ApiConfigAccessFailed"        },
    { ApiInvalidDeviceInfo,         "ApiInvalidDeviceInfo"         },
    { ApiInvalidDriverVersion,      "ApiInvalidDriverVersion"      },
    { ApiInvalidOffset,             "ApiInvalidOffset"             },
    { ApiInvalidData,               "ApiInvalidData"               },
    { ApiInvalidSize,               "ApiInvalidSize"               },
    { ApiInvalidAddress,            "ApiInvalidAddress"            },
    { ApiInvalidAccessType,         "ApiInvalidAccessType"         },
    { ApiInvalidIndex,              "ApiInvalidIndex"              },
    { ApiInvalidPowerState,         "ApiInvalidPowerState"         },
    { ApiInvalidIopSpace,           "ApiInvalidIopSpace"           },
    { ApiInvalidHandle,             "ApiInvalidHandle"             },
    { ApiInvalidPciSpace,           "ApiInvalidPciSpace"           },
    { ApiInvalidBusIndex,           "ApiInvalidBusIndex"           },
    { ApiInsufficientResources,     "ApiInsufficientResources"     },
    { ApiWaitTimeout,               "ApiWaitTimeout"               },
    { ApiWaitCanceled,              "ApiWaitCanceled"              },
    { ApiDmaChannelUnavailable,     "ApiDmaChannelUnavailable"     },
    { ApiDmaChannelInvalid,         "ApiDmaChannelInvalid"         },
    { ApiDmaDone,                   "ApiDmaDone"                   },
    { ApiDmaPaused,                 "ApiDmaPaused"                 },
    { ApiDmaInProgress,             "ApiDmaInProgress"             },
    { ApiDmaCommandInvalid,         "ApiDmaCommandInvalid"         },
    { ApiDmaInvalidChannelPriority, "ApiDmaInvalidChannelPriority" },
    { ApiDmaSglPagesGetError,       "ApiDmaSglPagesGetError"       },
    { ApiDmaSglPagesLockError,      "ApiDmaSglPagesLockError"      },
    { ApiMuFifoEmpty,               "ApiMuFifoEmpty"               },
    { ApiMuFifoFull,                "ApiMuFifoFull"                },
    { ApiPowerDown,                 "ApiPowerDown"                 },
    { ApiHSNotSupported,            "ApiHSNotSupported"            },
    { ApiVPDNotSupported,           "ApiVPDNotSupported"           },
    { ApiLastError,                 "Unknown"                      }
};


/** @brief Close a previously opened PCI slot
*
*/
XIA_EXPORT int XIA_API plx_close_slot(HANDLE h)
{
  PLX_STATUS status;
  PLX_DEVICE_OBJECT device_object;

  unsigned long idx;


  ASSERT(h);

  status = _plx_find_handle_index(h, &idx);

  if (status != PLX_SUCCESS) {
    _plx_log_DEBUG("Unable to find HANDLE %#p\n", h);
    return status;
  }
  
  device_object = V_MAP.device[idx]; 
  
  status = _plx_remove_slot_from_map(idx);

  if (status != PLX_SUCCESS) {
    _plx_log_DEBUG("Error unmapping device (h = %#p)\n", h);
    return status;
  }

  status = PlxPci_DeviceClose(&device_object);

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
  PLX_STATUS status;

  PLX_DEVICE_KEY dev;
  PLX_DEVICE_OBJECT device_object;

  memset(&dev, PCI_FIELD_IGNORE, sizeof(PLX_DEVICE_KEY));

  dev.bus       = bus;
  dev.slot      = slot;
  dev.DeviceId  = 0x9054;
  dev.VendorId  = PLX_VENDOR_ID;

  status = PlxPci_DeviceOpen(&dev, &device_object);

  if (status != ApiSuccess) {
    _plx_log_DEBUG("Error opening device (id = %u, bus = %u): status = %d\n",
                   id, bus, status);

    _plx_print_more(status);
    return PLX_API;
  }

  status = _plx_add_slot_to_map(&device_object);
  
  if (status != PLX_SUCCESS) {
    _plx_log_DEBUG("Error adding device %u/%u/%u to virtual map\n",
                   dev.bus, dev.slot, dev.DeviceId);
    return status;
  }

  *h = (HANDLE) device_object.hDevice;

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
*
*/
static void _plx_print_more(PLX_STATUS err)
{
    int i = 0;

    while (ApiErrors[i].code != ApiLastError)
    {
        if (ApiErrors[i].code == err)
        {
           _plx_log_DEBUG("Error caught in plxlib, %s\n", ApiErrors[i].text);
           return;
        }
        i++;
    }

    _plx_log_DEBUG("UNKNOWN ERROR (%d) caught in plxlib\n", err);  
}


/** @brief Adds a slot (specified by h) to the virtual map and sets up the
* BAR.
*
*/
static int _plx_add_slot_to_map(PLX_DEVICE_OBJECT *device)
{
  PLX_STATUS status;

  unsigned long *new_addr = NULL;

  PLX_NOTIFY_OBJECT *new_events = NULL;

  PLX_INTERRUPT *new_intrs = NULL;

  PLX_DEVICE_OBJECT *new_device = NULL;

  boolean_t *new_registered = NULL;


  V_MAP.n++;

  if (!V_MAP.addr) {

  ASSERT(V_MAP.n == 1);
    ASSERT(!V_MAP.device);

    V_MAP.addr = (unsigned long *)malloc(sizeof(unsigned long));

    if (!V_MAP.addr) {
      _plx_log_DEBUG("Unable to allocate %d bytes for V_MAP.addr array\n",
                     sizeof(unsigned long));
      return PLX_MEM;
    }

    V_MAP.device = (PLX_DEVICE_OBJECT *)malloc(sizeof(PLX_DEVICE_OBJECT));

    if (!V_MAP.device) {
      _plx_log_DEBUG("Unable to allocate %d bytes for V_MAP.device array\n",
                     sizeof(PLX_DEVICE_OBJECT));
      return PLX_MEM;
    }
    
    V_MAP.events = (PLX_NOTIFY_OBJECT *)malloc(sizeof(PLX_NOTIFY_OBJECT));

    if (!V_MAP.events) {
      _plx_log_DEBUG("Unable to allocate %d bytes for V_MAP.events array\n",
                     sizeof(PLX_NOTIFY_OBJECT));
      return PLX_MEM;
    }

    V_MAP.intrs = (PLX_INTERRUPT *)malloc(sizeof(PLX_INTERRUPT));

    if (!V_MAP.intrs) {
      _plx_log_DEBUG("Unable to allocate %d bytes for V_MAP.intrs array\n",
                     sizeof(PLX_INTERRUPT));
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

    new_device = (PLX_DEVICE_OBJECT *)realloc(V_MAP.device, V_MAP.n *
                                              sizeof(PLX_DEVICE_OBJECT));

    if (!new_device) {
      _plx_log_DEBUG("Unable to allocate %d bytes for 'new_device'\n",
                     V_MAP.n * sizeof(PLX_DEVICE_OBJECT));
      return PLX_MEM;
    }

    new_events = (PLX_NOTIFY_OBJECT *)realloc(V_MAP.events, V_MAP.n *
                                              sizeof(PLX_NOTIFY_OBJECT));

    if (!new_events) {
      _plx_log_DEBUG("Unable to allocate %d bytes for 'new_events'\n",
                     V_MAP.n * sizeof(PLX_NOTIFY_OBJECT));
      return PLX_MEM;
    }

    new_intrs = (PLX_INTERRUPT *)realloc(V_MAP.intrs, V_MAP.n * sizeof(PLX_INTERRUPT));

    if (!new_intrs) {
      _plx_log_DEBUG("Unable to allocate %d bytes for 'new_intrs'\n",
                     V_MAP.n * sizeof(PLX_INTERRUPT));
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
    V_MAP.device     = new_device;
    V_MAP.events     = new_events;
    V_MAP.intrs      = new_intrs;
    V_MAP.registered = new_registered;

  }

  V_MAP.device[V_MAP.n - 1] = *device;
  device = &(V_MAP.device[V_MAP.n - 1]);

  status = PlxPci_PciBarMap(&(V_MAP.device[V_MAP.n - 1]), PLX_PCI_SPACE_0, 
                            (VOID **)&(V_MAP.addr[V_MAP.n - 1]));

  if (status != ApiSuccess) {
    /* Undo memory allocation, etc. here */
    _plx_log_DEBUG("Error getting BAR for handle %#p\n", &device);
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
static int _plx_remove_slot_from_map(unsigned long idx)
{
  PLX_STATUS status;

  unsigned long i;
  unsigned long j;

  unsigned long *new_addr = NULL;

  PLX_DEVICE_OBJECT *new_device = NULL;
  PLX_NOTIFY_OBJECT *new_events = NULL;
  PLX_INTERRUPT *new_intrs = NULL;

  boolean_t *new_registered = NULL;

  status = PlxPci_PciBarUnmap(&(V_MAP.device[idx]), (VOID**)&(V_MAP.addr[idx]));

  if (status != ApiSuccess) {
    _plx_log_DEBUG("Error unmapping HANDLE %#p\n", &(V_MAP.device[idx]));
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

  new_device = (PLX_DEVICE_OBJECT *)malloc((V_MAP.n - 1) *
                                           sizeof(PLX_DEVICE_OBJECT));

  if (!new_device) {
    _plx_log_DEBUG("Unable to allocate %d bytes for 'new_device'\n",
                   (V_MAP.n - 1) * sizeof(PLX_DEVICE_OBJECT));
    return PLX_MEM;
  }

  new_events = (PLX_NOTIFY_OBJECT *)malloc((V_MAP.n - 1) *
                                           sizeof(PLX_NOTIFY_OBJECT));

  if (!new_events) {
    _plx_log_DEBUG("Unable to allocate %d bytes for 'new_events'\n",
                   (V_MAP.n - 1) * sizeof(PLX_NOTIFY_OBJECT));
    return PLX_MEM;
  }

  new_intrs = (PLX_INTERRUPT *)malloc((V_MAP.n - 1) * sizeof(PLX_INTERRUPT));

  if (!new_intrs) {
    _plx_log_DEBUG("Unable to allocated %d bytes for 'new_intrs'\n",
                   (V_MAP.n - 1) * sizeof(PLX_INTERRUPT));
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
      new_device[j] = V_MAP.device[i];
      new_events[j] = V_MAP.events[i];
      new_intrs[j]  = V_MAP.intrs[i];
      new_registered[j]  = V_MAP.registered[i];
      j++;
    }
  }

  free(V_MAP.addr);
  free(V_MAP.device);
  free(V_MAP.events);
  free(V_MAP.intrs);
  free(V_MAP.registered);

  V_MAP.n--;

  if (V_MAP.n == 0) {
    /* Last slot has been removed. Need to free all the memory */
    free(new_addr);
    free(new_device);
    free(new_events);
    free(new_intrs);
    free(new_registered);
    
    V_MAP.addr = NULL;
    V_MAP.device = NULL;
    V_MAP.events = NULL;
    V_MAP.intrs = NULL;
    V_MAP.registered = NULL;    
  } else {
    V_MAP.addr   = new_addr;
    V_MAP.device = new_device;
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
    if (V_MAP.device[i].hDevice == (PLX_DRIVER_HANDLE) h) {
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
  PLX_STATUS status;

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
  PLX_STATUS status;

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

  PLX_STATUS status;
  PLX_STATUS ignored_status;

  PLX_DMA_PROP dma_prop;

  PLX_DMA_PARAMS dma_params;


  ASSERT(len > 0);
  ASSERT(data != NULL);

  status = _plx_find_handle_index(h, &idx);

  if (status != PLX_SUCCESS) {
    _plx_log_DEBUG("Unable to find HANDLE %#p\n", h);
    return status;
  }
  
  memset(&dma_prop, 0, sizeof(PLX_DMA_PROP));

  dma_prop.ReadyInput       = 1;
  dma_prop.Burst            = 1;
  dma_prop.BurstInfinite    = 1;
  dma_prop.LocalAddrConst   = 1;
  dma_prop.LocalBusWidth    = 2;  // 32-bit bus 

  
  status = PlxPci_DmaChannelOpen(&(V_MAP.device[idx]), 0, &dma_prop);

  if (status != ApiSuccess) {
    _plx_log_DEBUG("Error opening PCI channel 0 for 'burst' read: HANDLE %#p\n",
                   h);
    _plx_print_more(status);
    return PLX_API;
  }

  /* If the handle is not registered as a notifier, then we need to do it.
  * this only needs to be done once per handle.
  */
  if (!V_MAP.registered[idx]) {
    memset(&(V_MAP.intrs[idx]), 0, sizeof(PLX_INTERRUPT));

    // Setup to wait for DMA channel 0
    V_MAP.intrs[idx].DmaChannel_0 = 1;

    status = PlxPci_NotificationRegisterFor(&(V_MAP.device[idx]), 
                                        &(V_MAP.intrs[idx]), &(V_MAP.events[idx]));

    if (status != ApiSuccess) {
      ignored_status = PlxPci_DmaChannelClose(&(V_MAP.device[idx]), 0);
      _plx_log_DEBUG("Error registering for notification of PCI DMA channel 0: "
                     "HANDLE %#p\n", h);
      _plx_print_more(status);
      return PLX_API;
    } else {
      /* Print some message here to make sure that the event is registerd */
      _plx_log_DEBUG("Registered for notification of PCI DMA channel 0: "
                     "HANDLE %#p\n", h);    
    }

    V_MAP.registered[idx] = TRUE_;
  }

  /* Write transfer address to XMAP_REG_TAR */
  status = plx_write_long(h, 0x50, addr);  

  if (status != PLX_SUCCESS) {
    ignored_status = PlxPci_DmaChannelClose(&(V_MAP.device[idx]), 0);
    _plx_log_DEBUG("Error setting block address %#x: HANDLE %#p\n", addr, h);
    _plx_print_more(status);
    return PLX_API;
  }
  
  memset(&dma_params, 0, sizeof(PLX_DMA_PARAMS));

  /* We include the dead words in the transfer */
  local = (unsigned long *)malloc((len + n_dead) * sizeof(unsigned long));

  if (!local) {
    _plx_log_DEBUG("Error allocating %d bytes for 'local'.\n",
                   (len + 2) * sizeof(unsigned long));
    return PLX_MEM;
  }

  dma_params.u.UserVa          = (U32)local;
  dma_params.LocalAddr         = EXTERNAL_MEMORY_LOCAL_ADDR;
  dma_params.ByteCount         = (len + n_dead) * 4;
  dma_params.LocalToPciDma     = 1;
 
  status = PlxPci_DmaTransferUserBuffer(&(V_MAP.device[idx]), 0, &dma_params, 0);

  if (status != ApiSuccess) {
    free(local);
    ignored_status = PlxPci_DmaChannelClose(&(V_MAP.device[idx]), 0);
    _plx_log_DEBUG("Error during 'burst' read: HANDLE %#p\n", h);
    _plx_print_more(status);
    return PLX_API;
  }

  /* ASSERT((V_MAP.events[idx]).IsValidTag == PLX_TAG_VALID); */
  status = PlxPci_NotificationWait(&(V_MAP.device[idx]), &(V_MAP.events[idx]), 10000);

  if (status != ApiSuccess) {
    plx_dump_vmap_DEBUG();
    free(local);    
    ignored_status = PlxPci_DmaChannelClose(&(V_MAP.device[idx]), 0);
    _plx_log_DEBUG("Error waiting for 'burst' read to complete: HANDLE %#p\n", h);
    _plx_print_more(status);
    return PLX_API;
  }

  memcpy(data, local + n_dead, len * sizeof(unsigned long));
  free(local);

  status = PlxPci_DmaChannelClose(&(V_MAP.device[idx]), 0);

  if (status != ApiSuccess) {
    _plx_log_DEBUG("Error closing PCI channel 0: HANDLE %#p\n", h);
    _plx_print_more(status);
    return PLX_API;
  }

  return PLX_SUCCESS;
}


/** @brief Dump out the virtual map.
*
*/
XIA_EXPORT void XIA_API plx_dump_vmap_DEBUG(void)
{
  unsigned long i = 0;


  _plx_log_DEBUG("Starting virtual map dump.\n");

  for (i = 0; i < V_MAP.n; i++) {
    _plx_log_DEBUG("\t%u: addr = %#x, HANDLE = %p, REGISTERED = %u\n", 
					i, V_MAP.addr[i], V_MAP.device[i].hDevice, V_MAP.registered[i]);
					
	if (V_MAP.registered[i]) {	
		_plx_log_DEBUG("\t   hEvent = %p, IsValidTag = %p\n", 
			(U64) (V_MAP.events[i]).hEvent,
			(U32) (V_MAP.events[i]).IsValidTag 
		);
    }

    if (V_MAP.addr[i]) {
      _plx_log_DEBUG(
			"PCI BAR\n"
    		"0: 0x%08x\n"
    		"1: 0x%08x\n"
    		"2: 0x%08x\n"
    		"3: 0x%08x\n"
    		"5: 0x%08x\n",
    		"4: 0x%08x\n",
    		(unsigned long *)(V_MAP.addr[i]), 
    		(unsigned long *)(V_MAP.addr[i] + (0x10)),
    		(unsigned long *)(V_MAP.addr[i] + (0x20)),
    		(unsigned long *)(V_MAP.addr[i] + (0x30)),
    		(unsigned long *)(V_MAP.addr[i] + (0x50)),
    		(unsigned long *)(V_MAP.addr[i] + (0x40))
  		);
	}
	
                   
  }

  _plx_log_DEBUG("Virtual map dump complete.\n");
}
